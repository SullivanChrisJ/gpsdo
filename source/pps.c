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
	int32_t pps_long;
} pps_count;

// Report variance to F_CPU every <interval> seconds
#define INTERVAL 16

int32_t ppserr;
int32_t ppserr_max;
int8_t  ppsint;

// Time can't be reset while running, so we
// need to know where we are starting from
volatile uint16_t pps_start;

// Print pps count function
static unsigned char pps_report(struct tlist *);

uint8_t pps_init(uint32_t tolerance)
/*
 Count processor cycles between PPS interrupts. An exact count is needed so
 no prescaler is used. After 2^16 cycles, an interrupt will occur which is
 counted in the high order word of the 32-bit count value.
 Uses 16 bit Timer 1.
	At 1MHz,  count will be 0x 00:0f:42:40
 	At 10MHz, count will be 0x 00:98:96:80
 Tolerance is the maximum error (in ppm) allowed for a valid count
 PPS signal should connect to pin ICP1.
*/
{
	// Set up reporting
	ppsint = 0;
	ppserr = 0;
	// Translate tolerance into cycles, being careful about integer overflow
        ppserr_max = (tolerance + 99) / 100 * (F_CPU / 100) / 100;

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

/*

This is sample code to read the fuse bits. "Sensible" accuracy is much lower
with the internal RC oscillator. The 4MHz value is almost 1% low. As RC
oscillators are adjustable, the GPS could discipline it quite easily.

First, though, we must make the tolerance wider when using the internal
RC oscillator, perhaps 1.5%, vs. .1% for the crystal and more for an
external oscillator. Not sure about external RC circuit but probably
wide as well with available component tolerances.

#include <avr/boot.h>

void print_val(char *msg, uint8_t val)
{
    Serial.print(msg);
    Serial.println(val, HEX);
}

void setup(void)
{

    Serial.begin(9600);
    while (!Serial) ;
    print_val("lockb = 0x", boot_lock_fuse_bits_get(1));
    print_val("ext fuse = 0x", boot_lock_fuse_bits_get(2));
    print_val("high fuse = 0x", boot_lock_fuse_bits_get(3));
    print_val("low fuse = 0x", boot_lock_fuse_bits_get(0));
#define SIGRD 5
#if defined(SIGRD) || defined(RSIG)
    Serial.print("Signature : ");
    for (uint8_t i = 0; i < 5; i += 2) {
        Serial.print(" 0x");
        Serial.print(boot_signature_byte_get(i), HEX);
    }
    Serial.println();
#endif
    Serial.print("Serial Number : ");
    for (uint8_t i = 14; i < 24; i += 1) {
        Serial.print(" 0x");
        Serial.print(boot_signature_byte_get(i), HEX);
    }
    Serial.println();

}
*/
void loop(void)
{
}


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

	// DEBUG - turn LED off and on with PPS
	led_toggle(LEDB_unit);
}

static unsigned char pps_report(struct tlist * tl)
// Report number of processor cycles in a 1 second interval
{
	struct spi_buf * buf;
	int32_t fcpu_err;

	// # of cycles +/- nominal CPU frequency
	fcpu_err = tl->tl_udata.longs - F_CPU;

	// First print to console, remove when spi comms debugged
	serial_printf("%8li cycles\r\n", fcpu_err);

	// Accumlated error over INTERVAL seconds, then send value to SPI master
	if (abs(fcpu_err) <= ppserr_max)
	{
	    // Add the error to total
	    ppserr += fcpu_err;

	    // After INTERVAL seconds, send to master and reset
	    if (++ppsint >= INTERVAL)
	    {
		// DEBUG - print equivalent message on serial console
		serial_printf("F_CPU: %8lu, Interval: %u, Error: %8li\r\n", F_CPU, ppsint, ppserr);

		// If no buffers available, just keep counting.
		if (buf = spi_getbuf())
		{
		    // DEBUG - turn on red LED, SPI ISR turns it off.
		    led_state(1, LEDR_unit);

		    *(buf->ptr++) = SPICMD_PPS;
		    *(buf->ptr++) = F_CPU & 0xFF;
		    *(buf->ptr++) = (F_CPU >>  8) & 0xFF;
		    *(buf->ptr++) = (F_CPU >> 16) & 0xFF;
		    *(buf->ptr++) = (F_CPU >> 24) & 0xFF;
		    *(buf->ptr++) = ppsint;
		    *(buf->ptr++) = ppserr & 0xFF;
		    *(buf->ptr++) = (ppserr >>  8) & 0xFF;
		    *(buf->ptr++) = (ppserr >> 16) & 0xFF;
		    *(buf->ptr++) = (ppserr >> 24) & 0xFF;
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
