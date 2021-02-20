/*
 * ringbuf.h
 *
 *  Created on: Jan 30, 2015
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

#ifndef RINGBUF_H_
#define RINGBUF_H_

typedef struct{
	unsigned char space;
	unsigned char length;
	unsigned char in;
	unsigned char out;
	unsigned char buffer[];
}  ring ;

// Function prototypes

ring * ring_create(uint8_t);
void ring_destroy(ring *);
int8_t ring_putb(ring *, uint8_t);
int8_t ring_getb(ring *);
int8_t ring_putbs(ring *, uint8_t, uint8_t []);

#endif /* RINGBUF_H_ */
