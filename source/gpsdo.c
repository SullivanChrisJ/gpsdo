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
#include <avr/interrupt.h>                                                              // Target processor interrupts
#include <avr/sleep.h>
#include <avr/power.h>
#include <util/delay.h>
#include <stdint.h>

#include "time.h"
#include "led.h"
#include "serial.h"
#include "pps.h"
#include "spi.h"
#include "gpsdo.h"

unsigned char flasher(struct tlist *);
unsigned char clock(struct tlist *);

/* Interrupt Vector reference

$000	RESET		N/A
$002	INT0		N/A
$004	INT1		N/A
$006	INT2		External switch on pin 3
$008	TIMER2 COMP	N/A
$00A	TIMER2 OVF	N/A
$00C	TIMER1 CAPT	1PPS input from GPS on pin 19
$00E	TIMER1 COMPA	N/A
$010	TIMER1 COMPB	N/A
$012	TIMER1 OVF	Carries into 4 byte # of cycles value in 1PPS
$014	TIMER0 COMP	In time.c to decrement timer tick count
$016	TIMER0 OVF	N/A
$018	SPI, STC	In spi.c to indicate ready to accept new byte
$01A	USART, RXC	N/A (will be used for input at some stage)
$01C	USART, URDE	Data register empty - used in serial.c output
$01E	USART, TCX	N/A
$020	ADC		N/A
$022	EE_RDY		N/A
$024	ANA_COMP	N/A
$026	TWI		N/A
$028	SPM_RDY		N/A

*/


union {
	uint8_t bytes[4];
	uint32_t seconds;
} clock_data;

union {
	uint8_t bytes[4];
	uint32_t counter;
} spi_data;

int main(void)
{
	sei();

	// Initialize LEDs for output, initially off
	led_init();

	// Initialize serial output, clear screen, print banner
	serial_init(2);
	serial_printf("%c[2JGPSDO V0\r\n\n", 27);

	// Initialize timer
	time_init();

	// Start counting CPU cycles between PPS pulses
	pps_init();

	// Initialize serial peripheral interface to communicate to Pi
	spi_init();
	
	// Start uptime counter (to be moved to LCD when ready)
	// serial_printf("Setting 2 second clock output\r\n");
	clock_data.seconds = 0;
	time_set(clock, 200, 2, clock_data.bytes, 1);

	// Flash LED once/sec during development
	// serial_printf("Setting LED 1 flash/sec\r\n");
	time_set(flasher, 50, 0, 0, 1);

	serial_printf("Entering main loop\r\n");


	// We sleep as much as possible, waiting for interrup
        set_sleep_mode(SLEEP_MODE_IDLE);

        while (1)
        {
            sleep_mode();               // Go to sleep until interrupted
            // ocxo_gps_sync();         // First up, check for GPS pulse & process
	    // switch_xeq();		// Respond to a switch press	
	    proc_timer();		// Background process for timer interrupts
            time_xeq();                 // Dispatch any timers which have expired
            spi_cmd();			// Execute any commands from SPI
        };
};

unsigned char flasher(struct tlist * tl)
{
	// Turn LED off if on, on if off, and let timer continue
        led_toggle(LEDG_unit);
        return 0;
};

unsigned char clock(struct tlist * tl)
// Log the time. Call to time set includes the # of seconds to increment on each call
// (context) and the current timer state (udata). In its present implementation the
// clock will run about 1% slow, or more if the CPU is overloaded.
{
	uint32_t s;
	uint16_t dd; 
	uint8_t  hh;
	uint8_t  mm;
	uint8_t  ss;

	s = (tl->tl_udata.longs += tl->tl_ucontext);
	dd = s  / 86400;
	s -= dd * 86400;
	hh = s  /  3600;
	s -= hh * (uint32_t) 3600;
	mm = s  /    60;
	s -= mm * (uint16_t) 60;
	ss = s;

	serial_printf("%3id %02i:%02i:%02i\r\n", dd, hh, mm, ss);
	return 0;
}


// Very simple acknowledgement of receiving a message over the spi
void msg1(struct spi_buf * buf)
{
        serial_printf("Received message 1\r\n");
}
