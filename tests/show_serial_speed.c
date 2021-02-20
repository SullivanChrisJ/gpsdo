/*
	This program is for Gnu LINUX, not AVR.
	Build program with: gcc -o show_serial_speed show_serial_speed.c

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

#define F_CPU 10000000			// Standard OCXO frequency
#define __AVR_ATmega32A__ 1

#include <stdio.h>
#include <stdint.h>
#include "../source/serial.h"		// Long calculations of CPU speed table.

static uint16_t bps_div[BPS_LEN] =
{
        UBRR_1200,
        UBRR_2400,
        UBRR_4800,
        UBRR_9600,
        UBRR_19200,
        UBRR_38400,
        UBRR_57600,
        UBRR_115200
};

int main()
{
	uint32_t speed = 1200;

	printf("Register settings for CPU Speed %i\n", F_CPU);
	for (int i=0; i<BPS_LEN; i++)
	{
	    printf("Speed: %6i, UBBRH: %5i, UBBRLL: %5i\n", speed, (bps_div[i] >> 8) & 0xFF, bps_div[i] & 0xFF);
	    speed *= 2;
	};
};
