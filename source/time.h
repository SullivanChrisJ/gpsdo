/*
 * time.h
 *
 *  Created on: Dec 25, 2014
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

#ifndef TIME_H_
#define TIME_H_

#include <stdlib.h>
#include <stdint.h>

// Delay timer for short delays

struct fnstruct {
	uint8_t (*ufn)(struct fnstruct *);
};

// Long timer add-on to short timer - these cannot be auto-requeued

struct tdata {
	uint16_t secs;								// Seconds to go
	void (*ufn)(uint8_t);							// Extended callback pointer
};

// A list of tlist entries, not a queue as it will not be ordered by expiry time.
struct tlist {
	struct tlist * tl_next;			// Forward pointer, null if last entry
    	unsigned char tl_ticks;			// Downward counter
	unsigned char tl_interval;		// Non-zero # of counts if auto-requeue
	unsigned char (*tl_ufn)(struct tlist *);// user callback function
	unsigned char tl_ucontext;		// Timer context (so callback can handle >1 task)
	union {
	    unsigned char bytes[4];		// 4 bytes if the user wants it
	    uint16_t words[2];			// Or two 16 bit integers
	    uint32_t longs;			// 
	    struct tdata ltimer;		// For long timers
	} tl_udata;
};

void time_init(void);
int8_t time_delay(uint8_t, uint8_t (*)(void));
int8_t time_set(unsigned char (*)(struct tlist *), uint8_t, uint8_t, uint8_t[], int8_t);
int8_t ltime_set(void (*)(uint8_t), uint16_t, uint8_t);
int8_t isr_fork(unsigned char (*)(struct tlist *), uint8_t, uint8_t[]);

uint8_t proc_timer(void);
void time_xeq(void);
void time_dump(void);

#endif /* TIME_H_ */
