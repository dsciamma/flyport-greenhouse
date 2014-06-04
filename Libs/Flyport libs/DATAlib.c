/** \file DATAlib.c
 *  \brief library to manage APN parameters for data connection
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
 *  FileName:        DATAlib.c
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
#include "DATAlib.h"

extern int Cmd;
extern int mainGSMStateMachine;
extern OpStat	mainOpStatus;
extern GSMModule mainGSM;

extern int CheckCmd(int countData, int chars2read, const DWORD tick, char* cmdReply, const char* msg2send, const BYTE maxtimeout);
extern int CheckEcho(int countData, const DWORD tick, char* cmdReply, const char* msg2send, const BYTE maxtimeout);
extern void CheckErr(int result, BYTE* smInt, DWORD* tickUpdate);

static BYTE smInternal = 0; // State machine for internal use of callback functions
static BYTE maxtimeout = 2;

static char* xApn;
static char* xLogin;
static char* xPassw;
static char* xIp;
static char* xDns1;
static char* xDns2;
/// @endcond

/**
\defgroup Data Connection
@{
DATAlib contains the functions to setup APN parameters

*/

/**
 * Configures APN params for data connections. Please, contact your SIM mobile operator to retrive more info.
 * \param apn - APN host name
 * \param login - login name
 * \param passw - password name
 * \param ip - IP address of modem. Example: "DYNAMIC" or "192.168.1.100" (the char array must be NULL terminated).
 * \param dns1 - IP address of primary dns. Example: "DYNAMIC" or "192.168.1.100" (the char array must be NULL terminated). 
 * \param dns2 - IP address of secondary dns. Example: "DYNAMIC" or "192.168.1.100" (the char array must be NULL terminated). 
 * \return - none.
 */
void APNConfig(char* apn, char* login, char* passw, char* ip, char* dns1, char* dns2)
{
	BOOL opok = FALSE;
	
	if(mainGSM.HWReady != TRUE)
		return;
	
	//	Function cycles until it is not executed
	while (!opok)
	{
		while (xSemaphoreTake(xSemFrontEnd,0) != pdTRUE);		//	xSemFrontEnd TAKE

		// Check mainOpStatus.ExecStat
		if (mainOpStatus.ExecStat != OP_EXECUTION)
		{		
			mainOpStatus.ExecStat = OP_EXECUTION;
			mainOpStatus.Function = 26;
			mainOpStatus.ErrorCode = 0;
			
			// Set Params	
			xApn = apn;
			xLogin = login;
			xPassw = passw;
			xIp = ip;
			xDns1 = dns1;
			xDns2 = dns2;
			
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
//****************************************************************************
//	Only internal use:
//	cAPNConfig callback function
//****************************************************************************
int cAPNConfig()
{
	char cmdReply[200];
	char msg2send[200];
	int resCheck = 0;
	DWORD tick;
	int countData;
	int chars2read;
	
	switch(smInternal)
	{
		case 0:
			// Check if Buffer is free
			if(GSMBufferSize() > 0)
			{
				// Parse Unsol Message
				mainGSMStateMachine = SM_GSM_CMD_PENDING;
				return -1;
			}
			else
				smInternal++;
				
		case 1:	
			// Send first AT command
			// ----------	APN Configuration	----------
			sprintf(msg2send, 	"AT+KCNXCFG=0,\"GPRS\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"\r",
								xApn, xLogin, xPassw, xIp, xDns1, xDns2);
			
			GSMWrite(msg2send);
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			maxtimeout = 2 + 3;
			smInternal++;
			
		case 2:
			vTaskDelay(20);
			// Check ECHO 
			countData = 0;
			
			resCheck = CheckEcho(countData, tick, cmdReply, msg2send, maxtimeout);
						
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			
		case 3:
			// Get reply (\r\nOK\r\n)
			vTaskDelay(1);
			// Get OK
			sprintf(msg2send, "OK");
			chars2read = 2;
			countData = 2; // GSM buffer should be: <CR><LF>OK<CR><LF>
			resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			else
			{
				vTaskDelay(200); // To prevent too fast TCP connection attempt
			}
			
		default:
			break;
	
	}
	
	smInternal = 0;
	// Cmd = 0 only if the last command successfully executed
	mainOpStatus.ExecStat = OP_SUCCESS;
	mainOpStatus.Function = 0;
	mainOpStatus.ErrorCode = 0;
	mainGSMStateMachine = SM_GSM_IDLE;
	return -1;
}
/// @endcond

/*! @} */

/*! @} */
