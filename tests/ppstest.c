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

/*
	In pps.c there is a calculation of the frequency error tolerance in 
	Hertz based on the number of parts per million of F_CPU. This
	seemingly simple calculation is made difficult by the large
	numbers. I have chosen a simple way to do this, although everything
	is rounded to the nearest 100 ppm. 

	There's probably a better way to do this but this rounding
	doesn't have any material impact on the operation of the device.

	This code was used to test the calculation before putting it into
	the MCU firmware where it is harder to test.
*/


#define F_CPU 4000000			// Standard OCXO frequency
#define TOLERANCE 15000			// 15000 ppm tolerance
#include <stdio.h>
#include <stdint.h>

int main()
{
	int32_t ppserr;
	int32_t ppserr_max;
	int8_t  ppsint;
	uint32_t tolerance;

	tolerance = TOLERANCE;

	ppserr_max = (tolerance + 99) / 100 * (F_CPU / 100) / 100;
	printf("F_CPU = %li\n", F_CPU);
	printf("Tolerance = %i\n", TOLERANCE);
	printf("ppserr_max = %li\n", ppserr_max);
};
