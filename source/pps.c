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
#include <stdint.h>

#include "serial.h"
#include "time.h"
#include "pps.h"
#include "led.h"
#include "spi.h"


// pps_count:
// 	pps_bytes: to pass data to the fork function
//	pps_words: to count overflow & pin interrupt separately
//	    Note: Little endian, so [0] is low order [1] is high order
//	pps_long:  for overall count of processor cycles between pin interruprs

union {
	uint8_t  pps_bytes[4];
	uint16_t pps_words[2];
	uint32_t pps_long;
} pps_count;

// Report variance to F_CPU every <interval> seconds
#define INTERVAL 16
#define PPSERR (F_CPU / 1000)

int16_t ppserr;
int8_t  ppsint;

// Time can't be reset while running, so we
// need to know where we are starting from
volatile uint16_t pps_start;

// Print pps count function
static unsigned char pps_report(struct tlist *);

uint8_t pps_init(void)
// Count processor cycles between PPS interrupts. An exact count is needed so
// no prescaler is used. After 2^16 cycles, an interrupt will occur which is
// counted in the high order word of the 32-bit count value.
// Uses 16 bit Timer 1.
//	At 1MHz,  count will be 0x 00:0f:42:40
// 	At 10MHz, count will be 0x 00:98:96:80
// At 20Mhz, the high order byte would be used under normal operation.
// PPS signal should connect to pin ICP1
{
	// Set up reporting
	ppsint = 0;
	ppserr = 0;

	// Normal port operation
	TCCR1A = 0;
	// Noise cancellation, PPS rising edge, system clock w/out prescaling
	TCCR1B = 1<<ICNC1 | 1<<ICES1 | 1<<CS10;
	// Initialize cycle counters
	pps_start = 0;
	pps_count.pps_long = 0;
	// Enable input capture & overflow interrupts
	TIMSK |= 1<<TICIE1 | 1<<TOIE1;
};

ISR(TIMER1_OVF_vect)
{
	pps_count.pps_words[1] += 1;
};

ISR(TIMER1_CAPT_vect)
{
	uint16_t icr;
	// Save the current interval
	icr = ICR1;
	pps_count.pps_words[0] = icr;
	pps_count.pps_long -= pps_start;
	pps_start = icr;
	
	// Now place an entry on timer completion queue to fork 
	// to a background process that reports the number of cycles.
	isr_fork(pps_report, 0, pps_count.pps_bytes);
	pps_count.pps_words[1] = 0;

	// DEBUG - toggle the blue LED
	led_toggle(LEDB_unit);
}

static unsigned char pps_report(struct tlist * tl)
// Report number of processor cycles in a 1 second interval
{
	struct spi_buf * buf;

	// First print to console, remove when spi comms debugged
	serial_printf("%8lu cycles\r\n", tl->tl_udata.longs - F_CPU);

	// Accumlated error over INTERVAL seconds, then send value to SPI master
	if (abs(tl->tl_udata.longs - F_CPU) <= PPSERR)
	{
	    // Add the error to total
	    ppserr += tl->tl_udata.longs - F_CPU;

	    // After INTERVAL seconds, send to master and reset
	    if (++ppsint >= INTERVAL)
	    {
		// DEBUG - print equivalent message on serial console
		serial_printf("F_CPU: %8lu, Interval: %u, Error: %i\r\n", F_CPU, ppsint, ppserr);

		// If no buffers available, just keep counting.
		if (buf = spi_getbuf())
		{
		    // DEBUG
		    led_state(1, LEDR_unit);

		    *(buf->ptr++) = SPICMD_PPS;
		    *(buf->ptr++) = F_CPU & 0xFF;
		    *(buf->ptr++) = (F_CPU >>  8) & 0xFF;
		    *(buf->ptr++) = (F_CPU >> 16) & 0xFF;
		    *(buf->ptr++) = (F_CPU >> 24) & 0xFF;
		    *(buf->ptr++) = ppsint;
		    *(buf->ptr++) = ppserr & 0xFF;
		    *(buf->ptr++) = (ppserr >> 8) & 0xFF;
		    spi_tx_queue(buf);
		};
		ppserr = 0;
		ppsint = 0;
	    }; 
	} else {
	    // If error exceeds PPSERR, we are in an unlocked state
	    ppsint = 0;
	    ppserr = 0;
	};
	return 1;
}
