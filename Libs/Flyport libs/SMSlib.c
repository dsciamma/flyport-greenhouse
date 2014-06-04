/** \file SMSlib.c
 *  \brief library to manage SMS functionalities
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
 *  FileName:        SMSlib.c
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
#include "SMSlib.h"

extern void callbackDbg(BYTE smInt);

extern SMSData mainSMS;
extern int Cmd;
extern int mainGSMStateMachine;
extern OpStat	mainOpStatus;
extern GSMModule mainGSM;

static BOOL ackSMSrequested;

extern int CheckCmd(int countData, int chars2read, const DWORD tick, char* cmdReply, const char* msg2send, const BYTE maxtimeout);
extern int CheckEcho(int countData, const DWORD tick, char* cmdReply, const char* msg2send, const BYTE maxtimeout);
extern void CheckErr(int result, BYTE* smInt, DWORD* tickUpdate);
extern void gsmDebugPrint(char*);

static BYTE smInternal = 0; // State machine for internal use of callback functions
static BYTE maxtimeout = 2;
/// @endcond

/**
\defgroup SMS
@{
Provides SMS functions for GSM
*/

/**
 * Provides last known SMS sender
 * \param None
 * \return char* with sender
 */
char*	LastSmsSender()
{
	return (char*)mainSMS.Sender;
}

/**
 * Provides last known SMS text content
 * \param None
 * \return char* with text
 */
char*	LastSmsText()
{
	return (char*)mainSMS.Text;
}

/**
 * Provides last known SMS memory type
 * \param None
 * \return memory type
 <UL>
	<LI>SM_MEM</LI>
	<LI>ME_MEM</LI>
 </UL>
 */
BYTE	LastSmsMemType()
{
	return mainSMS.MemType;
}

/**
 * Provides last known SMS index
 * \param None
 * \return SMS index
 */
int		LastSmsIndex()
{
	return mainSMS.Index;
}

/**
 * Provides last known SMS time
 * \param None
 * \return struct tm of last time
 */
struct tm LastSmsTime()
{
	return mainSMS.time;
}


/**
 * Returns message reference of last sms sent
 * \param None
 * \return int value of last sent sms message reference
 */
int LastSmsMsgRef()
{
	return mainSMS.MessageReference;
}

/**
 * Returns report value of last sms
 * \param None
 * \return int value of last report
 */
int LastSmsRepValue()
{
	return mainSMS.ReportValue;
}

/// @cond debug
int cSMSSend()
{
	char cmdReply[200];
	char msg2send[200];
	int resCheck = 0;
	DWORD tick;
	int countData;
	int chars2read;
	char ctrlz[2];
	ctrlz[0] = 0x1A;
	ctrlz[1] = '\0';//'\r';
			
	
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
			// ----------	SMS report ACKNOWLEDGE	----------
			// please, see http://www.smssolutions.net/tutorials/gsm/sendsmsat/
			if(ackSMSrequested == FALSE)
			{
				// unset +CSMP param (AT+CSMP=0,0,0,0)
				sprintf(msg2send, "AT+CSMP=17,167,0,0\r");
			}
			else
			{
				// set +CSMP param (AT+CSMP=49,167,0,0)
				sprintf(msg2send, "AT+CSMP=49,167,0,0\r");
			}
			
			GSMWrite(msg2send);
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			maxtimeout = 5;
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
			
		case 4:
			// Send "AT+CMGS"
			sprintf(msg2send, "AT+CMGS=\"%s\"\r", mainSMS.Destination);
			
			GSMWrite(msg2send);
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			// Update max timeout for this command: 30 seconds
			maxtimeout = 30;
			smInternal++;
			
		case 5:
			// Check ECHO
			countData = 0;
			resCheck = CheckEcho(countData, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			
		case 6:
			// Check Reply ">"
			vTaskDelay(2);
			sprintf(msg2send, "> ");
			chars2read = 2;
			countData = 2; // GSM buffer should be: <CR><LF>> 
			
			// here it is not possible to use the "getAnswer" (so neither CheckCmd) function 
			// since there is no '\n' chars...
			// for this case only we have to use a custom loop:
			int lenAnsw = strlen(msg2send);

			GSMpRead(cmdReply, lenAnsw+2);
			cmdReply[lenAnsw+2] = '\0';
			
			if(strstr(cmdReply, msg2send) != NULL)
			{
				// Reply present! Read until the lineNumber occurrence of '\n' char are found...
				int cc = GSMReadN(200, '\n', chars2read, cmdReply);
				cmdReply[cc] = '\0';
		
				resCheck = 0;
			}
			else
				resCheck = -1;
			
			//#warning removed CheckErr since if we received echo, we must receive the "> " cursor...
			// CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			
		case 7:
			// Send SMS TEXT + CtrlZ
			sprintf(msg2send, "%s%s", mainSMS.Text, ctrlz);
			GSMWrite(msg2send);
			
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			// Update max timeout for this command: 30 seconds
			maxtimeout = 30;
			smInternal++;
			
		case 8:
			vTaskDelay(50);
			
			// Check ECHO
			countData = 0;
			resCheck = CheckEcho(countData, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			
		case 9:
			// Get reply "+CMGS: <mr>"
			vTaskDelay(50);
			sprintf(msg2send, "+CMG");
			chars2read = 2;
			countData = 2; // GSM buffer should be: <CR><LF>+CMGS: <mr><CR><LF>

			resCheck = CheckCmd(countData, chars2read , tick, cmdReply, msg2send, maxtimeout);	
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			
			// Get parameters:
			if(resCheck == 0)
			{
				// Get <mr> parameter
				char temp[10];
				int res = getfield(' ', '\r', 10, 1, cmdReply, temp, 500);
				if(res != 1)
				{
					// Execute Error Handler
					ErrorHandler(TIMEOUT_ERR);
					break;
				}
				else
				{
					mainSMS.MessageReference = atoi(temp);
				}		
			}
				
		case 10:
			// Get reply (\r\nOK\r\n)
			vTaskDelay(1);
			// Get OK
			sprintf(msg2send, "\r\nOK");
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
 * Sends a SMS to provided phone number
 * \param char[] with phone number
 * \param char[] with SMS text content
 * \param send notification SMS.
<UL>
	<LI>FALSE - no send notification SMS is requested</LI>
	<LI>TRUE - send notification SMS is requested. <B>NOTE:<B> operator could add additional costs for this feature</LI>
</UL>
 * \return None
 */
void SMSSend(char* phoneNumber, char* text, BOOL ack)
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
			mainOpStatus.Function = 1;
			mainOpStatus.ErrorCode = 0;
			
			// Set Params	
			strcpy(mainSMS.Text, text);
			strcpy(mainSMS.Destination, phoneNumber);
			
			// Needed to set +CSMP parameters
			ackSMSrequested = ack;
			
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
int cSMSRead()
{
	int resCheck = 0;
	char cmdReply[200];
	char msg2send[200];
	DWORD tick;
	int countData = 0;
	int chars2read = 1;
	// Prepare memory string parameter
	char memoryString [3];
	int smsLength = 0;
	
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
			if(mainSMS.MemType == 0)
			{
				sprintf(memoryString, "SM");
			}
			else
			{
				sprintf(memoryString, "ME");
			}
			
			// Change memory type
			sprintf(msg2send, "AT+CPMS=\"%s\",\"%s\",\"%s\"\r", memoryString, memoryString, memoryString);
			GSMWrite(msg2send);
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			// Update max timeout for this command: 30 seconds
			maxtimeout = 2;
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
			
			// Get +CPMS
			sprintf(msg2send, "+CPMS");
			chars2read = 2;
			countData = 2; // GSM buffer should be: <CR><LF>+CPMS...<CR><LF>
			resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
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
			
		case 5:	
			// Force "GSM" character set 
			sprintf(msg2send, "AT+CSCS=\"GSM\"\r");
			GSMWrite(msg2send);
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			// Update max timeout for this command: 30 seconds
			maxtimeout = 2;
			smInternal++;
			
		case 6:
			vTaskDelay(2);
			// Check ECHO 
			countData = 0;
			resCheck = CheckEcho(countData, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
		
		case 7:
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
						
		case 8: 
			vTaskDelay(2);
			// Write "AT+CMGR=X\r"
			// Change memory type
			sprintf(msg2send, "AT+CMGR=%d\r", mainSMS.Index);
			GSMWrite(msg2send);
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			// Update max timeout for this command: 30 seconds
			maxtimeout = 30;
			smInternal++;
			
		case 9:
			vTaskDelay(2);
			// Check ECHO 
			countData = 0;
			resCheck = CheckEcho(countData, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
		
		case 10:
			// Get reply (+CMGR:...)
			vTaskDelay(20);
			sprintf(msg2send, "+CMGR:");
			chars2read = 2;
			countData = 2; // GSM buffer should be: <CR><LF>+CMGR:...params...<CR><LF>
			resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			else
			{
				// Get sender phone number
				char temp[25];
				int res = getfield('\"', '\"', 30, 3, cmdReply, temp, 500);
				if(res != 1)
				{
					// Execute Error Handler
					gsmDebugPrint( "Error in getfield for phone number\r\n");
					break;
				}
				else
				{
					sprintf(mainSMS.Sender, "%s", temp);
					
				}	
				// Get DateTime string
				res = getfield('\"', '\"', 21, 7, cmdReply, temp, 500);
				if(res != 1)
				{
					// Execute Error Handler
					gsmDebugPrint( "Error in getfield for DateTime\r\n");
					break;
				}
				else
				{
					sprintf(mainSMS.DateTime, "%s", temp);
					
					// Convert DateTime string to struct time:
					int countFirstSlash = strlen(mainSMS.DateTime);
					int dmp;
					char tmpval[25];
					// Get index of first slash:
					for(dmp = 0; dmp < countFirstSlash; dmp++)
					{
						if(mainSMS.DateTime[dmp] == '/')
							countFirstSlash = dmp;
					}
					// copy first fieldof DateTime inside tmpval char[]
					strncpy(tmpval, mainSMS.DateTime, countFirstSlash);
					
					// Convert YEAR:
					mainSMS.time.tm_year = atoi(tmpval)+100;
					
					// Get second field (month)
					getfield('/', '/', 5, 1, mainSMS.DateTime, tmpval, 500);
					// Convert MONTH:
					mainSMS.time.tm_mon = atoi(tmpval) - 1;
					
					// Get third field (day)
					getfield('/', ',', 5, 2, mainSMS.DateTime, tmpval, 500);
					// Convert DAY
					mainSMS.time.tm_mday = atoi(tmpval);
					
					// Get hour
					getfield(',', ':', 5, 1, mainSMS.DateTime, tmpval, 500);
					// convert HOUR
					mainSMS.time.tm_hour = atoi(tmpval);
					
					// Get minutes
					getfield(':', ':', 5, 1, mainSMS.DateTime, tmpval, 500);
					// convert MINUTES
					mainSMS.time.tm_min = atoi(tmpval);
					
					// Get seconds & GMT
					countFirstSlash = strlen(mainSMS.DateTime);
					int quithere = 0;
					// Get index of last ':' char to retrieve seconds:
					for(dmp = 0; dmp < countFirstSlash; dmp++)
					{
						// stop at second ':'
						if((mainSMS.DateTime[dmp] == ':')&&(quithere != 0))
						{
							// store index of second ':'
							countFirstSlash = dmp;
						}
						// get first ':'
						if(mainSMS.DateTime[dmp] == ':')
						{	
							quithere++;
						}	
						
					}
					// copy 5 chars (2 chars for seconds, up to 5 chars for GMT)
					strncpy(tmpval, &mainSMS.DateTime[countFirstSlash+1], 5);
					
					// copy seconds
					char secs[3];
					secs[0] = tmpval[0];
					secs[1] = tmpval[1];
					secs[2] = '\0';
					
					// copy gmt
					char gmt[4];
					gmt[0] = tmpval[2];
					gmt[1] = tmpval[3];
					gmt[2] = tmpval[4];
					gmt[3] = '\0';
					
					// Convert SECONDS
					mainSMS.time.tm_sec = atoi(secs);
					/*
					// Add GMT to hours
					int gmtVal = atoi(gmt);
					gmtVal = gmtVal + mainSMS.time.tm_hour;
					if(gmtVal < 0)
					{
						mainSMS.time.tm_hour = gmtVal + 24;
						mainSMS.time.tm_mday--; 
					}
					else if(gmtVal > 24)
					{
						mainSMS.time.tm_hour = gmtVal - 24;
						mainSMS.time.tm_mday++;
					}
					*/
				}	
				// Get length
				res = getfield(',', '\r', 4, 11, cmdReply, temp, 500);
				if(res != 1)
				{
					// Execute Error Handler
					gsmDebugPrint( "Error in getfield for length\r\n");
					break;
				}
				else
				{
					smsLength = atoi(temp);
				}
			
				if(res != 0)
				{
					
					// Read smsLength bytes of data:
					res = GSMRead(msg2send, smsLength);
					msg2send[smsLength] = '\0';
				
					// Received text is plain text... no need conversion
					sprintf(mainSMS.Text, msg2send);
					mainSMS.Text[smsLength] = '\0';
				}
			}
			
		case 11:
			// Get reply (OK, BUSY, ERROR, etc...)
			vTaskDelay(20);
			// Get \r\n\r\nOK
			sprintf(msg2send, "\r\n\r\nOK");
			chars2read = 8;
			countData = 0; // GSM buffer should be: <CR><LF><CR><LF>OK<CR><LF>
			resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			
		case 12:	
			// Force "IRA" (default) character set 
			sprintf(msg2send, "AT+CSCS=\"IRA\"\r");
			GSMWrite(msg2send);
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			// Update max timeout for this command: 30 seconds
			maxtimeout = 2;
			smInternal++;
			
		case 13:
			vTaskDelay(2);
			// Check ECHO 
			countData = 0;
			resCheck = CheckEcho(countData, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
		
		case 14:
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

/**
 * Fills SMS struct with SMS content
 * \param index of SMS to read
 * \param memory type to use
<UL>
	<LI>SM_MEM</LI>
	<LI>ME_MEM</LI>
</UL>
 * \return None
 */
void SMSRead(int indexMsg, BYTE memType)
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
			// Set mainOpStatus parameters
			mainOpStatus.ExecStat = OP_EXECUTION;
			mainOpStatus.Function = 2;
			mainOpStatus.ErrorCode = 0;	
			smInternal = 0;
			
			// Set Params	
			mainSMS.MemType = memType;
			mainSMS.Index = indexMsg;
			
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
int cSMSDelete()
{
	int resCheck = 0;
	char cmdReply[200];
	char msg2send[200];
	DWORD tick;
	int countData = 0;
	int chars2read = 1;
	
	// Prepare memory string parameter
	char memoryString [3];
	
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
			if(mainSMS.MemType == 0)
			{
				sprintf(memoryString, "SM");
			}
			else
			{
				sprintf(memoryString, "ME");
			}
			
			// Change memory type
			sprintf(msg2send, "AT+CPMS=\"%s\",\"%s\",\"%s\"\r", memoryString, memoryString, memoryString);
			GSMWrite(msg2send);
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			// Set timeout max
			maxtimeout = 60;
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
			// Get reply (+CPMS: ...)
			sprintf(msg2send, "+CPMS");
			chars2read = 2;
			countData = 2;
			
			resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			
		case 4:
			vTaskDelay(2);
			// Get OK
			sprintf(msg2send, "OK");
			chars2read = 2;
			countData = 2;
			
			resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			
		case 5:
			vTaskDelay(2);
			/*
			// Check sms index...
			if(mainSMS.Index <= 0)
			{
				// If mainSMS.Index is not valid we simulate a "ERROR" reply...
				resCheck = 1;
				tick = TickGetDiv64K();
				CheckErr(resCheck, &smInternal, &tick);
			}
			*/
			// Write AT+CMGD=index,0\r
			sprintf(msg2send, "AT+CMGD=%d,0\r", mainSMS.Index);
			GSMWrite(msg2send);
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			// Set timeout max
			maxtimeout = 30;
			smInternal++;
			
		case 6:
			vTaskDelay(20);
			// Check ECHO 
			countData = 0;
			resCheck = CheckEcho(countData, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			
		case 7:
			// Get reply (OK, BUSY, ERROR, etc...)
			vTaskDelay(20);
			// Get OK
			sprintf(msg2send, "\r\nOK");
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
 * Deletes SMS of provided memory and index
 * \param index of SMS to read
 * \param memory type to use
<UL>
	<LI>SM_MEM</LI>
	<LI>ME_MEM</LI>
</UL>
 * \return None
 */
void SMSDelete(int indexMsg, BYTE memType)
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
			// Set mainOpStatus parameters
			mainOpStatus.ExecStat = OP_EXECUTION;
			mainOpStatus.Function = 3;
			mainOpStatus.ErrorCode = 0;
			smInternal = 0;
			
			// Set Params	
			mainSMS.MemType = memType;
			mainSMS.Index = indexMsg;
						
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

/*! @} */

/*! @} */
