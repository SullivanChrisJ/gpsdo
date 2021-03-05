/*
 * config.h
 *  This controls the processor resources that the program uses, to provide flexibility in
 *  hardware configuration.
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

#ifndef CONFIG_H_
#define CONFIG_H_

#include <avr/io.h>

// CPU Speed
//#define F_CPU  8000000				// Internal RC Clock 8MHz
//#define F_CPU 10000000			// Eventual frequency for OCXO
//#define F_CPU 12000000			// Leftover crystal from VE3HII project
//#define F_CPU   14247000			// Latest random crystal
#define F_CPU 4000000				// Old crystal - 4.000445 MHz
//#define F_CPU 7023000				// Crystal from Pixie Transceiver

/*
 * Oven controlled oscillator parameters. Warmup time is from system boot and no adjustments will be made until that time has passed unless
 * the software has been signalled somehow (e.g. by a switch attached to a processor pin) that the oscillator is already warm. OCXO_MINDELTA
 * species the (log 2) minimum measurement time. OCXO_JITTER / 2**OCXO_MINDELTA should be less than or equal to OCXO_RANGE/1000 otherwise the
 * GPS jitter could result in exceeding the adjustment range of the OCXO
 */


// All of this will need to be changed when PID control is implemented
#define OCXO_WARMUP 60*15			// 15 minute oscillator warm-up time
#define OCXO_MINDELTA 3				// Log 2 of the minimum useful GPS interval (3 = 8 seconds)
#define OCXO_MAXDELTA 15			// Log 2 of the maximum GPS interval (15 = 9.1 hours)
#define OCXO_FREQ F_CPU				// Will be the same as the clock
#define OCXO_STEP (OCXO_FREQ % 65536)		// Expected timer step value (modulo 2^16)
#define OCXO_RANGE 2.5				// Adjustable +/- value in HZ
#define OCXO_CTRL_MIN 0				// Lowest control voltage
#define OCXO_CTRL_MAX 5				// Highest control voltage
#define OCXO_JITTER	16		 	// Tolerance in OCXO_FREQ including GPS jitter in cycles

#define OCXO_TIMER 1				// Use timer 1 for OCXO control

// Define some LEDs to play with
#define LED_port PORTA				// This for testing
#define LED_ddr  DDRA				// This direction
#define LEDG_pin PORTA0				// White LED is on this pin (used for timer running indicator)
#define LEDR_pin PORTA1 			// Red LED - On, fork buffer alloc failed, Off - succeeded
#define LEDB_pin PORTA2				// Green LED - inverts with each PPS pulse
#define LEDG_unit 0				// Give these unit numbers to reference when call routines
#define LEDR_unit 1
#define LEDB_unit 2

#define LED_pinr PINA				// Pin input (on 1284P, inverts data bit)

// Serial port requirements
#define NUM_USARTS 1				// Number of serial ports used (varies by processor)
#define USART1_BPS 4800
#define USART2_BPS 4800


#endif /* CONFIG_H_ */
