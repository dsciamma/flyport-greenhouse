/** \file LowLevelLib.c
 *  \brief library to manage Standard/LowLevel mode switch, and user AT commands
 */

/**
\addtogroup GSM
@{
*/
/* **************************************************************************																					
 *                                OpenPicus                 www.openpicus.com
 *                                                            italian concept
 * 
 *            openSource wireless Platform for sensors and Internet of Things	
 * **************************************************************************
 *  FileName:        LowLevelLib.c
 *  Dependencies:    Microchip configs files
 *  Module:          FlyPort GPRS/3G
 *  Compiler:        Microchip C30 v3.12 or higher
 *
 *  Author               Rev.    Date              Comment
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  Gabriele Allegria    1.0     02/08/2013		   First release  (core team)
 *  Simone Marra
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
/// @cond debug
#include "HWlib.h"
#include "Hilo.h"
#include "GSMData.h"
#include "LowLevelLib.h"

extern int Cmd;
extern int mainGSMStateMachine;
extern OpStat	mainOpStatus;
extern GSMModule mainGSM;

extern int HiloStdModeOn(long int baud);
extern const long int baudComp[8];

static char* writeBuffer;
static int writeBufferCount;
/// @endcond

/**
\defgroup LowLevel Mode
@{
Provides functions to switch between Standard and LowLevel mode. 
Offers also Hilo Hardware Communication functions if LowLevel mode enabled.
*/

/// @cond debug
int cLLModeEnable()
{
	if(mainGSM.HWReady == TRUE)
		mainGSM.HWReady = FALSE;
	
	if(mainGSMStateMachine != SM_GSM_LL_MODE)
		mainGSMStateMachine = SM_GSM_LL_MODE;
		
	mainOpStatus.ExecStat = OP_LL;
	mainOpStatus.Function = 0;
	mainOpStatus.ErrorCode = 0;
		
	return 0;
}
/// @endcond


/**
 * Switches the framework to LL mode. 
 <B>Warning!</B> In this mode, unsolicited messages and events are not handled by framework 
 * \param None
 * \return None
 */
void LLModeEnable()
{
	BOOL opok = FALSE;
	
	//	Function cycles until it is not executed
	while (!opok)
	{
		while (xSemaphoreTake(xSemFrontEnd,0) != pdTRUE);		//	xSemFrontEnd TAKE
		
		// Check mainOpStatus.ExecStat
		if (mainOpStatus.ExecStat != OP_LL)
		{		
			// Set mainOpStatus parameters
			mainOpStatus.ExecStat = OP_LL;
			if(mainGSMStateMachine != SM_GSM_LL_MODE)
				mainGSMStateMachine = SM_GSM_LL_MODE;
			mainOpStatus.Function = 18;
			mainOpStatus.ErrorCode = 0;
			
			// Set Params	
						
			xQueueSendToBack(xQueue,&mainOpStatus.Function,0);	//	Send COMMAND request to the stack
			
			xSemaphoreGive(xSemFrontEnd);						//	xSemFrontEnd GIVE, the stack can answer to the command
			opok = TRUE;
		}
		else
		{
			xSemaphoreGive(xSemFrontEnd);
			taskYIELD();
		}
	}
}

/// @cond debug
int cSTDModeEnable()
{	
	// Enter STD Mode:
	int res = HiloStdModeOn(baudComp[7]); // 115200 baud...
    if(res == 0)
        mainOpStatus.ExecStat = OP_SUCCESS;
    else
        mainOpStatus.ExecStat = res;
	mainOpStatus.Function = 0;
	mainOpStatus.ErrorCode = res;
	
	return res;
}
/// @endcond


/**
 * Switches the framework to Standard mode. 
 <B>Warning!</B> When this functions is called, a Hilo reset is performed, so any data / voice connection is lost  
 * \param None
 * \return None
 */
void STDModeEnable()
{
	BOOL opok = FALSE;
	
	//	Function cycles until it is not executed
	while (!opok)
	{
		while (xSemaphoreTake(xSemFrontEnd,0) != pdTRUE);		//	xSemFrontEnd TAKE
		
		// Check mainOpStatus.ExecStat
		if (mainOpStatus.ExecStat != OP_EXECUTION)
		{		
			if(mainGSMStateMachine == SM_GSM_LL_MODE)
				mainGSMStateMachine = SM_GSM_IDLE;
			
			mainOpStatus.ExecStat = OP_EXECUTION;

			// Set mainOpStatus parameters
			// mainOpStatus.ExecStat = OP_EXECUTION;
			mainOpStatus.Function = 19;
			mainOpStatus.ErrorCode = 0;
			
			// Set Params	
						
			xQueueSendToBack(xQueue,&mainOpStatus.Function,0);	//	Send COMMAND request to the stack
			
			xSemaphoreGive(xSemFrontEnd);						//	xSemFrontEnd GIVE, the stack can answer to the command
			opok = TRUE;
		}
		else
		{
			xSemaphoreGive(xSemFrontEnd);
			taskYIELD();
		}
	}
}

/**
 * Flushes Hilo dedicated UART buffer data. 
 <B>Note:</B> this function works only when LL mode is enabled
 * \param None
 * \return None
 */
void LLFlush()
{
	if(mainOpStatus.ExecStat == OP_LL)
	{
		GSMFlush();
		return;
	}
	else
		return;

}

/**
 * Gets RX buffer size of Hilo dedicated UART. 
 <B>Note:</B> this function works only when LL mode is enabled
 * \param None
 * \return size of Hilo buffer. Returns -1 if LL mode is not enabled
 */
int  LLBufferSize()
{
	if(mainOpStatus.ExecStat == OP_LL)
		return GSMBufferSize();
	else
		return -1;
}

/**
 * Reads data from Hilo dedicated UART Buffer. 
 <B>Note:</B> this function works only when LL mode is enabled
 * \param char[] buffer to fill with data
 * \param amount of data to read
 * \return number of BYTEs actually read from buffer. Returns -1 if LL mode is not enabled
 */
int  LLRead(char *buffer , int count)
{
	if(mainOpStatus.ExecStat == OP_LL)
		return GSMRead(buffer, count);
	else
		return -1;
}

/// @cond debug
int cLLWrite()
{
	int c;
	for (c = 0; c < writeBufferCount; c++)
		GSMWriteCh(writeBuffer[c]);
	
	mainOpStatus.Function = 0;
	mainOpStatus.ErrorCode = 0;
		
	return c;
}
/// @endcond

/**
 * Writes data to Hilo dedicated UART. 
 <B>Note:</B> this function works only when LL mode is enabled. 
 This function is similar to standard mode functions, and user firmware will be not locked waiting for end of operation.
 * \param char[] of data to write
 * \param amount of chars to write
 * \return None
 */
void LLWrite(char *buffer, int count)
{
	BOOL opok = FALSE;
	
	if(mainOpStatus.ExecStat != OP_LL)
		return;
	
	//	Function cycles until it is not executed
	while (!opok)
	{
		while (xSemaphoreTake(xSemFrontEnd,0) != pdTRUE);		//	xSemFrontEnd TAKE
		
		// Check mainOpStatus.ExecStat
		if (mainOpStatus.ExecStat == OP_LL)
		{		
			// Set mainOpStatus parameters
			mainOpStatus.Function = 17;
			mainOpStatus.ErrorCode = 0;
			
			// Set Params	
			writeBuffer = buffer;
			writeBufferCount = count;
						
			xQueueSendToBack(xQueue,&mainOpStatus.Function,0);	//	Send COMMAND request to the stack
			
			xSemaphoreGive(xSemFrontEnd);						//	xSemFrontEnd GIVE, the stack can answer to the command
			opok = TRUE;
		}
		else
		{
			xSemaphoreGive(xSemFrontEnd);
			taskYIELD();
		}
	}
}

/**
* Writes a single char on Hilo GSM UART. 
 <B>Note:</B> this function works only when LL mode is enabled. 
 * \param char to be written
 * \return None
 */
void LLWriteCh(char chr)
{
	char manCh[2] = { '\0', '\0' };
	manCh[0] = chr;
	LLWrite(manCh, 1);
}

/**
 * Performs a reset of Hilo GSM module. 
 <B>Note:</B> this function works only when LL mode is enabled. 
 * \param None
 * \return None
 */
void LLReset()
{
	if(mainOpStatus.ExecStat == OP_LL)
	{
		HiloReset();
		return;
	}
	else
		return;
}

/**
 * Changes DTR signal of Hilo UART dedicated. 
 <B>Note:</B> this function works only when LL mode is enabled. 
 * \param state of DTR (TRUE or FALSE)
 * \return None
 */
void LLDtr(BOOL onoff)
{
	if(mainOpStatus.ExecStat == OP_LL)
	{
		HILO_DTR_IO = onoff;
		return;
	}
	else
		return;
}

/**
 * Reads DCD signal of Hilo UART dedicated. 
 <B>Note:</B> this function works only when LL mode is enabled, else return FALSE
 * \param None
 * \return value of DCD signal
 */
BOOL LLDcd()
{
	if(mainOpStatus.ExecStat == OP_LL)
		return HILO_DCD_IO;
	else
		return FALSE;
}

/**
 * Reads DSR signal of Hilo UART dedicated. 
 <B>Note:</B> this function works only when LL mode is enabled, else return FALSE
 * \param None
 * \return value of DSR signal
 */
BOOL LLDsr()
{
	if(mainOpStatus.ExecStat == OP_LL)
		return HILO_DSR_IO;
	else
		return FALSE;
}

/**
 * Performs a POK of Hilo GSM module. 
 <B>Note:</B> this function works only when LL mode is enabled. 
 * \param None
 * \return TRUE if execution success, FALSE if Hilo do not reply at POK signal
 */
BOOL LLPok()
{
	if(mainOpStatus.ExecStat == OP_LL)
	{
		return HiloPok(2500);
	}
	else
		return FALSE;
}

/*! @} */

/*! @} */
