
/* **************************************************************************																					
 *                                OpenPicus                 www.openpicus.com
 *                                                            italian concept
 * 
 *            openSource wireless Platform for sensors and Internet of Things	
 * **************************************************************************
 *  FileName:        HWlib.c
 *  Dependencies:    Microchip configs files
 *  Module:          FlyPort WI-FI
 *  Compiler:        Microchip C30 v3.12 or higher
 *
 *  Author               Rev.    Date              Comment
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  Gabriele Allegria    1.0     1/20/2011		   First release  (core team)
 *  Stefano Cappello 							   Added I2C + support in PWM
 *  Andrea Seraghiti     2.2     11/21/2011        Added ethernet support
 *  Simone Marra         2.3     10/22/2012        Added GPRS/3G support
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  Software License Agreement
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  This is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License (version 2) as published by 
 *  the Free Software Foundation AND MODIFIED BY OpenPicus team.
 *  
 *  ***NOTE*** The exception to the GPL is included to allow you to distribute
 *  a combined work that includes OpenPicus code without being obliged to 
 *  provide the source code for proprietary components outside of the OpenPicus
 *  code. 
 *  OpenPicus software is distributed in the hope that it will be useful, but 
 *  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details. 
 * 
 * 
 * Warranty
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * THE SOFTWARE AND DOCUMENTATION ARE PROVIDED "AS IS" WITHOUT
 * WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTY OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * WE ARE LIABLE FOR ANY INCIDENTAL, SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF
 * PROCUREMENT OF SUBSTITUTE GOODS, TECHNOLOGY OR SERVICES, ANY CLAIMS
 * BY THIRD PARTIES (INCLUDING BUT NOT LIMITED TO ANY DEFENSE
 * THEREOF), ANY CLAIMS FOR INDEMNITY OR CONTRIBUTION, OR OTHER
 * SIMILAR COSTS, WHETHER ASSERTED ON THE BASIS OF CONTRACT, TORT
 * (INCLUDING NEGLIGENCE), BREACH OF WARRANTY, OR OTHERWISE.
 *
 **************************************************************************/
 
 

#include "HWlib.h"
#include "p24FJ256GA106.h"

extern BOOL TimerOn[5];
/****************************************************************************
  Function 		static void HWInit(int conf)

  Description 	This routine initializes the hardware.

  Precondition	None

  Parameters    None - None

  Returns	    None

  Remarks 		None
 ****************************************************************************/
void HWInit(int conf)
{	
	TimerOn[0] = TRUE;
	TimerOn[4] = TRUE;
	if (conf == HWDEFAULT)
	{
// IOs configuration
		IOInit(i1,in);
		IOInit(i2,in);
		IOInit(i3,in);
		IOInit(i4,in);
		IOInit(i5,in);
		
		IOInit(o1,out);
		IOInit(o2,out);
		IOInit(o3,out);
		IOInit(o4,out);
		IOInit(o5,out);

//	ADC initialization	
		ADCInit();
		
        // TRIS setup
        HILO_TXD_TRIS       = 0;
        HILO_RTS_TRIS       = 0;
        HILO_RXD_TRIS       = 1;
        HILO_CTS_TRIS       = 1;
        HILO_DSR_TRIS       = 1;
        HILO_DCD_TRIS       = 1;
        HILO_RI_TRIS        = 1;
        HILO_DTR_TRIS       = 0;

        // SET DTR:
        HILO_DTR_IO         = TURN_HILO_DTR_ON;

        // Before to set POK as output, enable open drain feature:
        HILO_POK_ODC        = 1;
        HILO_POK_IO         = 1;
        HILO_POK_TRIS       = 0;
        // Set open drain for RESET pin
        HILO_RESET_ODC      = 1;
        HILO_RESET_TRIS     = 0;
        HILO_RESET_IO       = 1;
/*
        HILO_GPIO1_TRIS     = 1;
        HILO_GPIO2_TRIS     = 1;
        HILO_GPIO3_TRIS     = 1;
        HILO_VGPIO_TRIS     = 1;
*/

	// Unlock registers to PPS configuration
	__builtin_write_OSCCONL(OSCCON & 0xBF);
        // PPS configuration
        // HiLo UART (UART4)
        RPOR9bits.RP19R = 30;                   // Assign RP19 to U4TX (output)
        RPOR13bits.RP27R = 31;                  // Assign RP27 to U4RTS (output)

        RPINR27bits.U4RXR = 21;                 // Assign RP21 to U4RX (input)
        RPINR27bits.U4CTSR = 26;                // Assign RP26 to U4CTS (input)
        

		// Configure UART 1 pins
		RPINR18bits.U1RXR = 24;					// Assign RP24 to U1RX (input)
		RPOR11bits.RP22R = 3;					// Assign RP22 to U1TX (output)
			
		// Lock Registers
		__builtin_write_OSCCONL(OSCCON | 0x40);
        
	}

}

