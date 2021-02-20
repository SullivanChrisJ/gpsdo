/*
 * LED.h
 *
 *  Created on: Dec 21, 2014
 *      Author: CSullivan
 *  Initial version will allow (1) the initialization of the LED data structures (2) turning the LED
 *  on (3) turning the LED off.
 *  Blinking the LED periodically will require a timer subsystem
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

#ifndef LED_H_
#define LED_H_

void led_init(void);
void led_toggle(int8_t);
void led_state(int8_t, int8_t);

#endif /* LED_H_ */
