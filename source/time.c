/*
 * time.c
 *
 *  Created on: Dec 25, 2014
 *  Major updates: July 12, 2020
 *      Author: Chris Sullivan
 *  Provides timer queuing services at 10ms resolution using an 8 bit timer.
 *  Due to use of prescaler, there is up to 256 clock cycles jitter +/- the
 *  desired interval. Also, as the timers are built around 10ms intervals, the
 *  timer will expire up to 10ms later than then requested interval. Jitter is
 *  corrected over long intervals (up to 1024) so that there is no clock drift
 *  othen than the accuracy of the processor clock itself.  
 */

/*
    GPSDO - Discipline an adjustable oscillator (typically OCXO) with GPS timing signals
    Copyright (C) 2021  Chris Sullivan

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

    You may contact the author via his Github page: SullivanChrisJ
*/

#include "config.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdint.h>
#include "gpsdo.h"
#include "time.h"
#include "serial.h"
#include "gpsdo.h"
#include "led.h"

// Internal function prototypes

unsigned char ltime_xeq(struct tlist *);


//Fork counts (Union of forkq array and individual labels). Counts should not exceed 1.

volatile uint8_t std_timer;			// 10ms timer completion count

// END EXTERNAL


// Move these defines to .h file
#if F_CPU > 255 * 100 * 1024		// >= 26.112 MHz
  #pragma GCC error "F_CPU exceeds maximum permissable value"
#elif F_CPU > 255 * 100 * 256		// >= 6.528 MHz
  #define prescale 1024
  #define clock_select (1<<CS02 | 1<<CS00)
#elif F_CPU > 255 * 100 * 64		// >= 1.632 MHz
  #define prescale 256
  #define clock_select 1<<CS02
#elif F_CPU > 255 * 100 * 8		// >= 204 kHz
  #define prescale 64
  #define clock_select (1<<CS01 | 1<<CS00)
#elif F_CPU > 255 * 100			// >=25.5 kHz
  #define prescale 8
  #define clock_select 1<<CS01
#else
  #pragma GCC error "F_CPU value is too low"
#endif

// Values to track drift. 
#define lead_interval (F_CPU / 100 / prescale) // Standard counter interval
#define lag_interval (lead_interval + 1)	// Drift correction counter

#define lead ((F_CPU / 100) % prescale)	// Drift due to std_interval
#define lag (prescale - lead)		// Drift due to alt_interval

// Calculate initial value of drift counter to remove bias offset
/*
#if lead % 512 == 0
  #define drift0 256
#elif lead % 256 == 0
  #define drift0 128
#elif lead % 128 == 0
  #define drift0 64
#elif lead % 64 == 0
  #define drift0 32
#elif lead % 32 == 0
  #define drift0 16
#elif lead % 16 == 0
  #define drift0 8
#elif lead % 8 == 0
  #define drift0 4
#elif lead % 4 == 0
  #define drift0 2
#elif lead % 2 == 0
  #define drift0 1
#else
  #define drift0 0
#endif
#if lead > lag
  #define drift0 -drift0
#endif
*/

#define TIMEBUF_NUM 5


volatile struct tlist *time_free;	// Free timer buffers
volatile struct tlist *time_fork;	// Interface between ISR & bacground
	 struct tlist *time_active;	// Main timer
	 struct tlist *time_done;	// Expired timer list

	 struct tlist time_bufs[TIMEBUF_NUM];

#if prescale > 127
volatile int16_t drift;				// # of ticks ahead (behind) actual time
#else
volatile int8_t drift;
#endif

/* Standard Timer

	The standard timer is designed to make use of an 8 bit timer 
	with 1024 prescaler to produce 10ms intervals. So setting
	a timer of n ticks results in the callback function being
	called after 10 * n <= t < 10 * (n + 1) ms, with an uncertainty
	of +/- 512 clock cycles (.064 ms @ 8 MHz).

	The timer can be one-shot or periodic. To maintain the accuracy
	of the periodic timer over a long interval, the lack of
	precision resulting from using the prescaler must be corrected.
	For example, at 8MHz 10ms is 80K clock cycles. If the count
	register is set to 78, an interrupt will be triggered 160
	clock cycles (.02ms) too early, which is 1/8th of the prescale
	number.  So the clock will be fast by 7.2 seconds/hour. By
	increasing the value of the count register by 1 every 8th
	cycle, the error can be maintained at < .08ms.

	The 10ms timer runs continuously once time_init is called. When
	a timer is set to a positive integer, it creates an entry on the
	time_active queue. The entry is moved to the time_done queue
	when the timer expires. Setting a timer of zero ticks places the
	entry on the time_done queue which is a sneaky way to invoke a
	background callback function from an ISR.
*/

void time_init(void)
// Using timer zero, initialize the timekeeping structure
// Timer granularity is 40ms (25/second). As we use prescalar value
// 1024, for most clock speeds we will need to adjust the count
// to eliminate cumulative error.
{
	// Initialize the main timer list & timer expired tlists
	time_active = 0;
	time_fork = 0;
	time_done = 0;

	drift = -lead;

        // Create the free list of events.
	time_free = time_bufs;
        {
            struct tlist * next = 0;
            for (int8_t i = TIMEBUF_NUM; i-- > 0;)
            {
                time_bufs[i].tl_next = next;
                next = &time_bufs[i];
            };
        };

	// Counter for timer interrupts
	std_timer = 0;

	// Initialize 8 bit timer/counter 0.
	// Count up to <interval> and generate interrupt 
#if defined (__AVR_ATmega32A__)
	OCR0 = lead_interval;			// Use shorter interval
	TCCR0 = (1<<WGM01 | clock_select);	// CTC mode, Set prescaler
	//TIMSK = 1<<OCIE0;			// Interrupt on expiry
	sbi(TIMSK, OCIE0);			// Interrupt on expiry
#elif defined (__AVR_ATmega1284P__)
	OCR0A = lead_interval;			// Interval <= 10ms
	OCR0B = lag_interval;			// Interval > 10ms
	//TIMSKA = 1<<OCIE0A;			// lead <= lag, use lead
	sbi(TIMSK, OCIE0);			// Interrupt on expiry
	TCCR0A = 1<<WGM01;			// CTC mode
	TCCR0B = (1<<FOC0A | clock_select);	// Set prescaler & lead intval
#endif
};

/* time_set(ufn, ticks, context, data[], periodic)
 * 
 * Set a timer that executes function ufn after ticks * 10ms and execute
 * indefinitely until ufn returns non-zero.
 * Function ufn is passed structure tlist which has a single byte value
 * context and a 4 byte value data. 
 */

int8_t time_set(unsigned char (*ufn)(struct tlist *), uint8_t ticks, uint8_t context, uint8_t data[], int8_t periodic)
{
	static struct tlist *ptr;

	cli();
	if (ptr = (struct tlist *)time_free)
	{
	    time_free = ptr->tl_next;
	    sei();

	    ptr->tl_ticks = ticks;				// Set timer interval (decremented by interrupt)
	    ptr->tl_interval = periodic ? ticks : 0; 		// Set recurrence if desired
	    ptr->tl_ufn = ufn;					// Set callback function
	    ptr->tl_ucontext = context;				// User context

	    if (data)
	    {
		(ptr->tl_udata).bytes[0] = data[0];
		(ptr->tl_udata).bytes[1] = data[1];
		(ptr->tl_udata).bytes[2] = data[2];
		(ptr->tl_udata).bytes[3] = data[3];
	    }

	    // If an interval is specified, set it up, otherwise expire the new timer immediately
	    if (ticks)
	    {
		ptr->tl_next = time_active;
		time_active = ptr;
	    } else {
		ptr->tl_next = time_done;
		time_done = ptr;
	    }
	    return 0;
	} else {
	    sei();
	    return 1;
	};
};

//	The following ISRs handle timer completion for the 10ms timer. 


/*
 * This subroutine processes the callbacks for all expired timers. It should be called whenever
 * the processor wakes up after the timer overflow interrupt. It is used to wait for the OCXO
 * to warm up, but could also be applied to issuing periodic status messages.
 *
 * 

 */

void time_xeq(void)
{
	static struct tlist *ptr;
	while (1)
	{

	    // Is there work to do on the fork queue?
	    cli();
	    if (ptr = (struct tlist *)time_fork)
	    { 
		time_fork = ptr->tl_next;
	    };
	    sei();

	    // How about the timer done queue?
 	    if (!ptr)
	    {
		if (ptr = time_done) 
	    	{
		    time_done = ptr->tl_next;
	    	} else {
		    break;
		};
	    };

	    // Invoke call-back function and reschedule on normal return if required
	    if (!ptr->tl_ufn(ptr) && ptr->tl_interval)
	    {
		// Reschedule by putting it back on the active list
		ptr->tl_ticks = ptr->tl_interval;
		ptr->tl_next = time_active;
	    	time_active = ptr;
	    } else {
		// Normal return or not periodic, free the entry
		cli();
		ptr->tl_next = (struct tlist *)time_free;
		time_free = ptr;
		sei();
	    };
	};
};

// Background fork for processing each timer tick

uint8_t proc_timer()
// Called upon wake-up due to interrupt. If a clock interrupt has
// been counted (in std_timer), the time in each entry on the active
// timer queue is decremented, and if it has expired, the entry is
// moved to the done queue for processing.
{
	struct tlist *ptr;
	struct tlist **pptr;	// Pointer to where we found the pointer

	while (std_timer)
	{
	    std_timer--;
	    // Find each item on the timer queue
	    pptr = &time_active;
	    ptr = time_active;

	    // Count down each timer by 1 tick
	    while (ptr)
	    {
		// Move entry to front of done queue at 0 ticks
		if ((ptr->tl_ticks)--)
		{
		    pptr = &(ptr->tl_next);				// Not expired yet, move on to next entry
		    ptr = ptr->tl_next;
		} else {
		    *pptr = ptr->tl_next;				// Unlink expired timer
		    ptr->tl_next = time_done;				// Link to first item on done queue
		    time_done = ptr;					// And put it on the front of the queue
		    ptr = *pptr;					// Read to process next item in active list
		};
	    };
	};
	return 0;
};

int8_t isr_fork(unsigned char (*ufn)(struct tlist *), uint8_t context, uint8_t data[])
// Create an entry in the 'done' queue for immediate processing
// This is must be called from an ISR (ie with interrupts disabled) to schedule something into the background
{
	struct tlist *ptr;

	// Get a free timer queue entry
        if (ptr = (struct tlist *)time_free)
        {
            time_free = ptr->tl_next;
	    ptr->tl_ufn = ufn;					// Set callback function
	    ptr->tl_ucontext = context;				// User context
	    if (data)
	    {
		(ptr->tl_udata).bytes[0] = data[0];
		(ptr->tl_udata).bytes[1] = data[1];
		(ptr->tl_udata).bytes[2] = data[2];
		(ptr->tl_udata).bytes[3] = data[3];
	    };
	    ptr->tl_interval = 0;				// Don't let it be rescheduled!

	    ptr->tl_next = (struct tlist *)time_fork;
	    time_fork = ptr;

	};
};

#if defined (__AVR_ATmega32A__)
ISR(TIMER0_COMP_vect)
// Timer has reached threshold of approximately 10 ms. The task scheduler
// running in the background is signalled with a completion count, which
// allows stacking of completions in case of undue processing delays. The
// count register is reset with the appropriate value.
{
	std_timer++;				// Tell background to process
	if (drift >= 0) {			// Keep drift within limits
	    drift -= lead;
	    OCR0 = lead_interval;
	} else {
	    drift += lag;
	    OCR0 = lag_interval;
	}
}
#elif defined (__AVR_ATmega1284P__)
ISR(TIMER0_COMPA_vect)
// Count timer expiries. Calculate & correct clock drift by
// switching between timer compare registers as required. 
{
	std_timer++;				// Tell background to process
  #if lead != 0
	// Choose whether adding or subtracting to keep drift within limts
        if (drift >= 0) {
	    drift -= lead;			// Select shorter interval
        } else {
	    drift += lag;
	    TIMSK0 = 1<<OCIE0B;			// Select longer interval
        }
  #endif
}

ISR(TIMER0_COMPB_vect)
// See above. This interrupt handles the lag_interval.
{
	std_timer++;				// Signal background
        if (drift >= 0) {
	    drift -= lead;
	    TIMSK0 = 1<<OCIE0A;
        } else {
	    drift += lag;
        }
};
#endif

#if 1 == 0

/*
 * ltime_set allows for timers of more than 2.55 seconds to be set. It simply sets the 10ms
 * timer for a 1 second interval and counts them down. The 4 byte user data field is used
 * to carry the 16 bit number of seconds (allowing timers of just over 18 hours) and the
 * callback function (as the tlist callback element is used forltime_xeq).
 */

int8_t ltime_set(void (* ufn)(uint8_t), uint16_t seconds, uint8_t context)
{
	union {
		uint8_t bytes[4];	// 4 bytes if the user wants it
		struct tdata ltimer;	// For long timers
	} udata;
	udata.ltimer.ufn = ufn;		// Short timer payload keeps track of long timer
	udata.ltimer.secs = seconds;
	return time_set(ltime_xeq, 100, context, udata.bytes, 1); // Set the recurring timer for 1 second  (100 x 10ms)
};

/*
 * ltime_xeq is only used as a callback routine. It decrements one second each time
 * it is called back by the 10ms timer. By using the 10ms timer it does not need to
 * maintain its own queue or run with interrupts disabled.
 */

unsigned char ltime_xeq(struct tlist *tl)
{
	if (--(tl->tl_udata.ltimer.secs)) // If time is left
	{
	    return 0;			// Keep the timer running
	} else {
	    tl->tl_udata.ltimer.ufn(tl->tl_ucontext); // Execute the callback function
	    return 1;			// And cancel the timer
	}
}

#endif


// Dumps the timer queues for debugging.

/*
void time_dump()
{
        static struct tlist ** q_list[4] = {(struct tlist **)&time_free, &time_active, &time_done, (struct tlist **)&time_fork};
        static char * q_lbl[4] = {"Free", "Active", "Done", "Fork"};
        static struct tlist * ptr;
        static struct tlist * time_addr[TIMEBUF_NUM];
        static int8_t time_q[TIMEBUF_NUM];

        static char format[80];
        static char * fmt_ptr;


	for (int8_t i=0; i < TIMEBUF_NUM; i++) time_addr[i] = 0;

        int8_t j = 0;

	// Gather the timer pointer information without interruption
        sei();
	for (int8_t i=0; i < 4; i++)
        {
            for(ptr=*(q_list[i]); ptr; ptr=ptr->tl_next)
            {
                time_addr[j] = ptr;
                time_q[j++] = i;
            };
        };

	cli();

	// Output it in concise formate (one line to not overrun serial output queue
        j = 0;
        fmt_ptr = format;
        for (int8_t i=0; i < 4; i++)
        {
          if (time_q[j]==i)
          {
            for (int8_t k=0; q_lbl[i][k]; k++) *fmt_ptr++ = q_lbl[i][k];
            *fmt_ptr++ = ':';

            for (int8_t k=0; j<TIMEBUF_NUM && time_q[j]==i; j++, k++)
            {
                if (k) *fmt_ptr++ = ',';
                *fmt_ptr++ = ' ';
                *fmt_ptr++ = '%';
                *fmt_ptr++ = '0';
                *fmt_ptr++ = '5';
                *fmt_ptr++ = 'x';
            };
            *fmt_ptr++ = ' ';
          };
        };
        *fmt_ptr++ = '\r';
        *fmt_ptr++ = '\n';
        *fmt_ptr++ = 0;

        serial_printf(format, time_addr[0], time_addr[1], time_addr[2], time_addr[3], time_addr[4]);
};
*/
