/** \file HILOlib.c
 *  \brief library to manage GSM Connection and IMEI
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
 *  FileName:        HILOlib.c
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
#include "HILOlib.h"
#include "GSMData.h"
#include "DATAlib.h"

extern int Cmd;
extern int mainGSMStateMachine;
extern OpStat	mainOpStatus;
extern GSMModule mainGSM;
extern int signal_dBm_Gsm;

extern int CheckCmd(int countData, int chars2read, const DWORD tick, char* cmdReply, const char* msg2send, const BYTE maxtimeout);
extern int CheckEcho(int countData, const DWORD tick, char* cmdReply, const char* msg2send, const BYTE maxtimeout);
extern void CheckErr(int result, BYTE* smInt, DWORD* tickUpdate);
extern void gsmDebugPrint(char*);

static BYTE smInternal = 0; // State machine for internal use of callback functions
static BYTE maxtimeout = 2;

//static BOOL connVal = FALSE;
/// @endcond

/**
\defgroup HILO
@{
Provides IMEI and Network Connection functions for GSM
*/

/**
 * Provides modem IMEI
 * \param None
 * \return char* with Hilo IMEI
 */
char* GSMGetIMEI()
{
	return (char*)mainGSM.IMEI;
}

/**
 * Turns GSM module OFF
 * \param None
 * \return None
 */
void GSMHibernate()
{
	BOOL opok = FALSE;
	
	if(mainGSM.HWReady != TRUE)
		return;
	if(mainGSMStateMachine == SM_GSM_HIBERNATE)
		return;
	
	//	Function cycles until it is not executed
	while (!opok)
	{
		while (xSemaphoreTake(xSemFrontEnd,0) != pdTRUE);		//	xSemFrontEnd TAKE

		// Check mainOpStatus.ExecStat
		if (mainOpStatus.ExecStat != OP_EXECUTION)
		{		
			mainOpStatus.ExecStat = OP_EXECUTION;
			mainOpStatus.Function = 28;
			mainOpStatus.ErrorCode = 0;
			
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
int cGSMHibernate()
{
	// AT*PSCPOF -> response: OK
	char cmdReply[200];
	char msg2send[200];
	int resCheck = 0;
	DWORD tick;
	int countData = 0;
	int chars2read = 1;
	
	switch (smInternal)
	{
		case 0:
            // Pause to reduce the bugs on this command
            vTaskDelay(350);
			if(GSMBufferSize() > 0)
			{
				// Parse Unsol Message
				mainGSMStateMachine = SM_GSM_CMD_PENDING;
                vTaskDelay(250); // wait a while to complete incoming message...
				return -1;
			}
			else
				smInternal++;
		
		case 1:
			sprintf(msg2send, "AT*PSCPOF\r");
			GSMWrite(msg2send);
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			// Real max timeout for this command: 5 seconds
			maxtimeout = 15;
			smInternal++;
			
		case 2:
			vTaskDelay(2);
			// Check ECHO 
			countData = 0;
			resCheck = CheckEcho(countData, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}


            maxtimeout = 15;
            smInternal++;
			
		case 3:
			// Get reply (OK, BUSY, ERROR, etc...)
			vTaskDelay(20);

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
				
				// Patch for registration:
				mainGSM.ConnStatus = NO_REG;
				
				HILO_POK_IO = 0;
			   	vTaskDelay(250);
			    HILO_POK_IO = 1;
			    
				int ctsTris = 0;
				if(HILO_CTS_TRIS != 0)
					ctsTris = 0;
				
				HILO_CTS_TRIS = 1;
				int cnt = 30;
				
				while ((cnt > 0)&&(HILO_CTS_IO != 0))
				{
					cnt--;
					vTaskDelay(20);
				}
				HILO_CTS_TRIS = ctsTris;
			}
				
		default:
			break;
	}
	
	smInternal = 0;
	// Cmd = 0 only if the last command successfully executed
	mainOpStatus.ExecStat = OP_SUCCESS;
	mainOpStatus.Function = 0;
	mainOpStatus.ErrorCode = 0;
	mainGSMStateMachine = SM_GSM_HIBERNATE;
	return -1;
	
}
/// @endcond

int cGSMHibernate_GSM_DISABLED()
{
	// AT*PSCPOF -> response: OK
	char cmdReply[200];
	char msg2send[200];
	int resCheck = 0;
	DWORD tick;
	int countData = 0;
	int chars2read = 1;

	switch (smInternal)
	{
		case 0:
			if(GSMBufferSize() > 0)
			{
				// Parse Unsol Message
				mainGSMStateMachine = SM_GSM_CMD_PENDING;
				return -1;
			}
			else
				smInternal++;

		case 1:
			sprintf(msg2send, "AT*PSCPOF\r");
			GSMWrite(msg2send);
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			// Real max timeout for this command: 5 seconds
			maxtimeout = 10;
			smInternal++;

		case 2:
			/*
            vTaskDelay(2);
            
			// Check ECHO
			countData = 0;
			resCheck = CheckEcho(countData, tick, cmdReply, msg2send, maxtimeout);

			CheckErr(resCheck, &smInternal, &tick);

			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
            */
		case 3:
			// Get reply (OK, BUSY, ERROR, etc...)
			vTaskDelay(20);

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

				// Patch for registration:
				mainGSM.ConnStatus = NO_REG;

				HILO_POK_IO = 0;
			   	vTaskDelay(250);
			    HILO_POK_IO = 1;

				int ctsTris = 0;
				if(HILO_CTS_TRIS != 0)
					ctsTris = 0;

				HILO_CTS_TRIS = 1;
				int cnt = 30;

				while ((cnt > 0)&&(HILO_CTS_IO != 0))
				{
					cnt--;
					vTaskDelay(20);
				}
				HILO_CTS_TRIS = ctsTris;
			}

		default:
			break;
	}

	smInternal = 0;
	// Cmd = 0 only if the last command successfully executed
	mainOpStatus.ExecStat = OP_SUCCESS;
	mainOpStatus.Function = 0;
	mainOpStatus.ErrorCode = 0;
	mainGSMStateMachine = SM_GSM_HIBERNATE;
	return -1;

}

void GSMLowPowerEnable()
{
    BOOL opok = FALSE;

	if(mainGSM.HWReady != TRUE)
		return;
	if(mainGSMStateMachine == SM_GSM_LOW_POWER)
		return;

	//	Function cycles until it is not executed
	while (!opok)
	{
		while (xSemaphoreTake(xSemFrontEnd,0) != pdTRUE);		//	xSemFrontEnd TAKE

		// Check mainOpStatus.ExecStat
		if (mainOpStatus.ExecStat != OP_EXECUTION)
		{
			mainOpStatus.ExecStat = OP_EXECUTION;
			mainOpStatus.Function = 9;
			mainOpStatus.ErrorCode = 0;

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

int cGSMLowPowerEnable()
{
    // >> AT+KSLEEP=0\r
    // ECHO
    // +KSLEEP=0
    // OK

    // DTR = "high level" => Module goes low power mode!
	char cmdReply[200];
	char msg2send[200];
	int resCheck = 0;
	DWORD tick;
	int countData = 0;
	int chars2read = 1;

	switch (smInternal)
	{
		case 0:
            // Pause to reduce the bugs on this command
            //vTaskDelay(350);
			if(GSMBufferSize() > 0)
			{
				// Parse Unsol Message
				mainGSMStateMachine = SM_GSM_CMD_PENDING;
                //vTaskDelay(250); // wait a while to complete incoming message...
				return -1;
			}
			else
				smInternal++;

		case 1:
			sprintf(msg2send, "AT+KSLEEP=0\r");
			GSMWrite(msg2send);
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			// Real max timeout for this command: 5 seconds
			maxtimeout = 15;
			smInternal++;

		case 2:
			vTaskDelay(2);
			// Check ECHO
			countData = 0;
			resCheck = CheckEcho(countData, tick, cmdReply, msg2send, maxtimeout);

			CheckErr(resCheck, &smInternal, &tick);

			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}


            maxtimeout = 15;
            smInternal++;

		case 3:
			// Get reply (OK, BUSY, ERROR, etc...)
			vTaskDelay(20);

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
                // Turn OFF DTR
                HILO_DTR_IO = TURN_HILO_DTR_OFF;
			}

		default:
			break;
	}

	smInternal = 0;
	// Cmd = 0 only if the last command successfully executed
	mainOpStatus.ExecStat = OP_SUCCESS;
	mainOpStatus.Function = 0;
	mainOpStatus.ErrorCode = 0;
	mainGSMStateMachine = SM_GSM_LOW_POWER;
	return -1;
}

/// @endcond

/**
 * Turns GSM module OFF and PIC microcontroller in Sleep mode 
 * \param None
 * \return None
 */
void GSMSleep()
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
			mainOpStatus.Function = 28; //29;
			mainOpStatus.ErrorCode = 0;
			
			xQueueSendToBack(xQueue,&mainOpStatus.Function,0);	//	Send COMMAND request to the stack
			
			xSemaphoreGive(xSemFrontEnd);						//	xSemFrontEnd GIVE, the stack can answer to the command
			opok = TRUE;
			taskYIELD();
		}
		else
		{
			xSemaphoreGive(xSemFrontEnd);
			taskYIELD();
		}
	}
	while (mainOpStatus.ExecStat == OP_EXECUTION);		// wait for callback Execution...	

	if(mainGSMStateMachine == SM_GSM_HIBERNATE)
	{
		// Set Sleep for PIC 
		// PIC sleep mode
		RCONbits.VREGS = 0;
		vTaskDelay(50);
		asm("PWRSAV #0");
	}
}

/**
 * Turns GSM module ON after hibernation or sleep 
 * \param None
 * \return None
 */
void GSMOn()
{
	BOOL opok = FALSE;
	
	if(mainGSM.HWReady != TRUE)
		return;
    if(( mainGSMStateMachine != SM_GSM_HIBERNATE)&&(mainGSMStateMachine != SM_GSM_LOW_POWER))
		return;
	
	//	Function cycles until it is not executed
	while (!opok)
	{
		while (xSemaphoreTake(xSemFrontEnd,0) != pdTRUE);		//	xSemFrontEnd TAKE

		// Check mainOpStatus.ExecStat
		if (mainOpStatus.ExecStat != OP_EXECUTION)
		{		
			mainOpStatus.ExecStat = OP_EXECUTION;
			mainOpStatus.Function = 29;
			mainOpStatus.ErrorCode = 0;
			
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
int cGSMOn()
{
    if(mainGSMStateMachine == SM_GSM_LOW_POWER)
    {
        // Turn on module by setting DTR:
        HILO_DTR_IO = TURN_HILO_DTR_ON;

        DWORD tick = TickGetDiv64K();
        while((HILO_CTS_IO == 1)&&((TickGetDiv64K() - tick) < 15))
        {
            vTaskDelay(20);
        }
        if(HILO_CTS_IO == 0)
        {
            mainGSMStateMachine = SM_GSM_IDLE;
            mainOpStatus.ExecStat = OP_SUCCESS;
            mainOpStatus.Function = 0;
            mainOpStatus.ErrorCode = 0;
        }
        else
        {
            mainGSMStateMachine = SM_GSM_IDLE;
            mainOpStatus.ExecStat = OP_LOW_POW_ERR;
            mainOpStatus.Function = 0;
            mainOpStatus.ErrorCode = 0;
        }
    }
    else
    {
        while(cSTDModeEnable());
    }
	
	mainGSMStateMachine = SM_GSM_IDLE;
	return -1;
}
/// @endcond


/**
 * Forces the update of GSM network connection status.
 * To read the connection status value, please use LastConnStatus() function
 * \param None
 * \return None
 */
void UpdateConnStatus()
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
			mainOpStatus.Function = 4;
			mainOpStatus.ErrorCode = 0;

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

/// cond debug
int cUpdateConnStatus()
{
    // AT+CGREG? --> response: ECHO\r\n+CGREG: <n>,<stat>[,<lac>,<ci>]\r\nOK\r\n
    int resCheck = 0;
	char cmdReply[200];
	char msg2send[200];
	DWORD tick;
	int countData = 0;
	int chars2read = 1;

	switch (smInternal)
	{
		case 0:
			if(GSMBufferSize() > 0)
			{
				// Parse Unsol Message
				mainGSMStateMachine = SM_GSM_CMD_PENDING;
				return -1;
			}
			else
				smInternal++;

		case 1:
            // Send AT command
			sprintf(msg2send, "AT+CGREG?\r");
			GSMWrite(msg2send);
            // Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			// Update max timeout for this command: 2 seconds (3 seconds to decrease error frequency...)
			maxtimeout = 3 + 2;
			smInternal++;

        case 2:
			vTaskDelay(2);
			// Check ECHO
			countData = 0;
			resCheck = CheckEcho(countData, tick, cmdReply, msg2send, maxtimeout);

			CheckErr(resCheck, &smInternal, &tick);

			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
            maxtimeout = 3 + 2;
            smInternal++;
            
        case 3:
			vTaskDelay(2);
            maxtimeout = 3 + 2;
			// Get +CREG
			sprintf(msg2send, "+CGREG");
			chars2read = 2;
			countData = 2; // GSM buffer should be: <CR><LF>+CGREG:...<CR><LF>
			resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);

			CheckErr(resCheck, &smInternal, &tick);

			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
            else
            {
                // Response
                // +CGREG: <n>,<stat>[,<lac>,<ci>]\r\n
                // OK\r\n
                // Get first number <n>
				char temp[25];
				int res = getfield(':', ',', 5, 1, cmdReply, temp, 500);
				if(res != 1)
				{
					// Execute Error Handler
					gsmDebugPrint( "Error in getfield for AT+CGREG command\r\n");
					break;
				}
                // Get connection status <stat>
                res = getfield(',', '\n', 30,1, cmdReply, temp, 500);
				if(res != 1)
				{
					// Execute Error Handler
					gsmDebugPrint( "Error in getfield for AT+CGREG command\r\n");
					break;
				}
				else
                {
                    int EventType=0;
                    int regStatus = atoi(temp);
                    switch(regStatus)
                    {
                        case 0:
                            // Set GSM Event:
                            EventType = ON_REG;
                            // Set mainGSM Connection Status
                            mainGSM.ConnStatus = NO_REG;
                            break;
                        case 1:
                            // Set GSM Event:
                            EventType = ON_REG;
                            // Set mainGSM Connection Status
                            mainGSM.ConnStatus = REG_SUCCESS;
                            break;
                        case 2:
                            // Set GSM Event:
                            EventType = ON_REG;
                            // Set mainGSM Connection Status
                            mainGSM.ConnStatus = SEARCHING;
                            break;
                        case 3:
                            // Set GSM Event:
                            EventType = ON_REG;
                            // Set mainGSM Connection Status
                            mainGSM.ConnStatus = REG_DENIED;
                            break;
                        case 4:
                            // Set GSM Event:
                            EventType = ON_REG;
                            // Set mainGSM Connection Status
                            mainGSM.ConnStatus = UNKOWN;
                            break;
                        case 5:
                            // Set GSM Event:
                            EventType = ON_REG;
                            // Set mainGSM Connection Status
                            mainGSM.ConnStatus = ROAMING;
                            break;
                        default:
                            // Set GSM Event:
                            EventType = ON_REG;
                            // Set mainGSM Connection Status
                            mainGSM.ConnStatus = UNKOWN;
                            break;
                    }
                    // In this case the event Handler will not print any message
                    // since the state machine is not IDLE, but CMD_PEN
                    EventHandler();
                }
            }

		case 4:
			// Get reply (OK, BUSY, ERROR, etc...)
			vTaskDelay(2);
            maxtimeout = 3 + 2;
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

/**
 * Forces the update of GSM network signal power value.
 * To read the signal power value, please use LastSignalRssi() function
 * \param None
 * \return None
 */
void GSMSignalQualityUpdate()
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
			mainOpStatus.Function = 16;
			mainOpStatus.ErrorCode = 0;

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

/// cond debug
int cGSMSignalQualityUpdate(void)
{
    // AT+CSQ --> response: ECHO\r\n+CSQ:x,y\r\nOK\r\n
    int resCheck = 0;
	char cmdReply[200];
	char msg2send[200];
	DWORD tick;
	int countData = 0;
	int chars2read = 1;

	switch (smInternal)
	{
		case 0:
			if(GSMBufferSize() > 0)
			{
				// Parse Unsol Message
				mainGSMStateMachine = SM_GSM_CMD_PENDING;
				return -1;
			}
			else
				smInternal++;

		case 1:
            // Send AT command
			sprintf(msg2send, "AT+CSQ\r");
			GSMWrite(msg2send);
            // Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			// Update max timeout for this command: 2 seconds (3 seconds to decrease error frequency...)
			maxtimeout = 3;
			smInternal++;

        case 2:
			vTaskDelay(2);
			// Check ECHO
			countData = 0;
			resCheck = CheckEcho(countData, tick, cmdReply, msg2send, maxtimeout);

			CheckErr(resCheck, &smInternal, &tick);

			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}

        case 3:
			vTaskDelay(2);

			// Get +CSQ
			sprintf(msg2send, "+CSQ");
			chars2read = 2;
			countData = 2; // GSM buffer should be: <CR><LF>+CSQ:...<CR><LF>
			resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);

			CheckErr(resCheck, &smInternal, &tick);

			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
            else
            {
                // Response
                // +CSQ: <rssi>,<ber>\r\n
                // OK\r\n
                // Get first number <rssi>
				char temp[25];
				int res = getfield(':', ',', 5, 1, cmdReply, temp, 500);
				if(res != 1)
				{
					// Execute Error Handler
					gsmDebugPrint( "Error in getfield for AT+CSQ command\r\n");
					break;
				}
                else
                {
                    int readVal = atoi(temp);
                    
                    if(readVal > 31)
                        signal_dBm_Gsm = 0;
                    else
                        signal_dBm_Gsm = (-113)+(2*readVal);
                }
                // Get <ber> (not used yet in FlyportGPRS framework...
                res = getfield(',', '\n', 30,1, cmdReply, temp, 500);
				if(res != 1)
				{
					// Execute Error Handler
					gsmDebugPrint( "Error in getfield for AT+CGREG command\r\n");
					break;
				}
				else
                {
                    // Warning!! Not Used Yet!!
                    int ber = atoi(temp);
                    if(ber > 7)
                        ber = -1; // not valid
                }
            }

		case 4:
			// Get reply (OK, BUSY, ERROR, etc...)
			vTaskDelay(2);
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

