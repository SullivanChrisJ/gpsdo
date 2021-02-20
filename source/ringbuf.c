/*
 * ringbuf.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ringbuf.h"
#include "serial.h"



ring * ring_create(uint8_t length)
{
	ring * b = malloc(sizeof(ring) + (int)length);
	b->space = length;
	b->length = length;
	b->in = 0;
	b->out = 0;
	return b;
}

void ring_destroy(ring * b)
{
	if (b) free(b);
};

/*
 * ring_putb - insert a byte into the ring buffer
 * Returns 1 if no room
 */

int8_t ring_putb(ring * b, uint8_t byte)
{
	if (b->space)
	{
	    b->buffer[b->in++] = byte;
	    b->in %= b->length;
	    b->space -=1;
	    return 0;
	}
	return 1;
}

/*
 * ring_getb - retrieve a byte from the ring buffer. If nothing there, then return "-1"
 */

int8_t ring_getb(ring * b)
{
	if (b->space < b->length)
	{
	    uint8_t byte = b->buffer[b->out++];
	    b->out %= b->length;
	    b->space++;
	    return byte;
	} else return 0xFF;
}

/*
 * ring_putbs(ring *b, uint8_t count, uint8_t bytes[])
 * Adds the bytes to the ring buffer b. Returns 1 if not enough room.
 */
int8_t ring_putbs(ring *b, uint8_t count, uint8_t bytes[])
{
	if (b->space >= count)
	{
	    if (b->length - b->in >= count)
	    {
		memcpy(&b->buffer[b->in], bytes, count);				//
		b->in += count;
	    } else {
		memcpy(&b->buffer[b->in], bytes, b->length - b->in);	// Write what we can
		memcpy(&b->buffer[0], &bytes[b->length - b->in], count - (b->length - b->in));
		b->in = count - (b->length - b->in);
	    }
	    b->space -= count;
	    return 0;
	} else return 1;
}
