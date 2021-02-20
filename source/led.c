/*
 * LED.c
 *
 *  Created on: Dec 21, 2014
 *      Author: CSullivan
 *
 *  Manage turning LEDs on and off. Blinking LEDs might be needed in future.
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

#include <avr/io.h>
#include "gpsdo.h"
#include "led.h"
#include "config.h"

// Initialize all configured LEDS (White, red and green)
void led_init(void)
{
	LED_ddr  |=  (1<<LEDG_pin | 1<<LEDR_pin | 1<<LEDB_pin);
	LED_port &= ~(1<<LEDG_pin | 1<<LEDR_pin | 1<<LEDB_pin);
}

// Unit is 0, 1 and 2 (should assert equivalence to the pin assignments)
void led_toggle(int8_t unit)
{
#if defined (__AVR_ATmega32A__)
	LED_port ^= (1 << unit);
	// LED_port ^= (1<<LED_pin);
#elif defined (__AVR_ATmega1284P__)
//	sbi(LED_pinr, LED_pin);				// If LED was off, turn it on & vice versa.
	LED_pinr |= (1<<LED_pin);
#endif
};

// Turn LED off or on. state=0 means off, all else means on)
void led_state(int8_t state, int8_t unit)
{
	if (state)
	{
	    sbi(LED_port, unit);
	} else {
	    cbi(LED_port, unit);
	};
};
