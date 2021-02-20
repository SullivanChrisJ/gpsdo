/*
 * gpsdo.h
 *
 *  Created on: Dec 20, 2014
 *      Author: CSullivan
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

#ifndef GPSDO_H_
#define GPSDO_H_

#include <stdint.h>

// Useful stuff

#define  sbi(port, bit) (port) |= (1 << (bit))
#define  cbi(port, bit) (port) &= ~(1 << (bit))

#endif /* GPSDO_H_ */
