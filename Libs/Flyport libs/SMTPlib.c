/** \file SMTPlib.c
 *  \brief library to send emails
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
 *  FileName:        SMTPlib.c
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
#include "SMTPlib.h"

extern void callbackDbg(BYTE smInt);
extern void gsmDebugPrint(char*);

extern int Cmd;
extern int mainGSMStateMachine;
extern OpStat	mainOpStatus;
extern GSMModule mainGSM;

extern int CheckCmd(int countData, int chars2read, const DWORD tick, char* cmdReply, const char* msg2send, const BYTE maxtimeout);
extern int CheckEcho(int countData, const DWORD tick, char* cmdReply, const char* msg2send, const BYTE maxtimeout);
extern void CheckErr(int result, BYTE* smInt, DWORD* tickUpdate);

static BYTE smInternal = 0; // State machine for internal use of callback functions
static BYTE maxtimeout = 2;

static char* smtpdomain;
static int   smtpport;
static char* smtpsender;
static char* smtplogin;
static char* smtppassw;
static char* smtpTo1;
static char* smtpTo2;
static char* smtpCc1;
static char* smtpCc2;
static char* smtpsubject;
static char* smtpmsg;
static int   smtpLastSessionId = 0;
/// @endcond

/**
\defgroup SMTP
@{
Provides email send functions
*/

/**
 * Provides last known SMTP session ids
 * \param None
 * \return int of the sessions id value
 */
int LastSMTPSessionId()
{
	return smtpLastSessionId;
}

/**
 * Sets SMTP parameter to use
 * \param char[] SMTP Domain name
 * \param int of smtp port used
 * \param char[] sender email
 * \param char[] smtp service login
 * \param char[] smtp service password
 * \return None
 */
void SMTPParamsSet(char* smtpDomain, int smtpPort, char* senderEmail, char* smtpLogin, char* smtpPassw)
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
			mainOpStatus.Function = 6;
			mainOpStatus.ErrorCode = 0;
			
			// Set Params	
			smtpdomain = smtpDomain;
			smtpport = smtpPort;
			smtpsender = senderEmail;
			smtplogin = smtpLogin;
			smtppassw = smtpPassw;
			
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
int  cSMTPParamsSet()
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
			// AT&K3
			sprintf(msg2send, "AT&K3\r");
			
			GSMWrite(msg2send);
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			maxtimeout = 2;
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
			// AT+KCNXTIMER
			sprintf(msg2send, "AT+KCNXTIMER=0,60,2,70\r");
			
			GSMWrite(msg2send);
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			maxtimeout = 2;
			smInternal++;
			
		case 5:
			vTaskDelay(1);
			// Check ECHO 
			countData = 0;
			
			resCheck = CheckEcho(countData, tick, cmdReply, msg2send, maxtimeout);
						
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			
		case 6:
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
			
		case 7:
			// AT+CGATT=1
			sprintf(msg2send, "AT+CGATT=1\r");
			
			GSMWrite(msg2send);
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			maxtimeout = 60;
			smInternal++;
			
		case 8:
			vTaskDelay(1);
			// Check ECHO 
			countData = 0;
			
			resCheck = CheckEcho(countData, tick, cmdReply, msg2send, maxtimeout);
						
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			
		case 9:
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
			
		case 10:
			// ----------	SMTP Parameter Configuration	----------
			sprintf(msg2send, "AT+KSMTPPARAM=\"%s\",%d,\"%s\"\r",smtpdomain, smtpport, smtpsender);
			
			GSMWrite(msg2send);
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			maxtimeout = 2;
			smInternal++;
			
		case 11:
			vTaskDelay(20);
			// Check ECHO 
			countData = 0;
			
			resCheck = CheckEcho(countData, tick, cmdReply, msg2send, maxtimeout);
						
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			
		case 12:
			/*
			// Get reply "+KSMTPPARAM: <socket>"
			vTaskDelay(2);
			sprintf(msg2send, "+KSMTPPARAM");
			chars2read = 2;
			countData = 2; // GSM buffer should be: <CR><LF>+KSMTPPARAM: <socket><CR><LF>
			
			resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			*/
			smInternal++;	
			
		case 13:
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
		
		case 14:
			// ----------	SMTP Login Configuration	----------
			sprintf(msg2send, "AT+KSMTPPWD=\"%s\",\"%s\"\r",smtplogin, smtppassw);
			
			GSMWrite(msg2send);
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			maxtimeout = 2;
			smInternal++;
			
		case 15:
			vTaskDelay(1);
			// Check ECHO 
			countData = 0;
			
			resCheck = CheckEcho(countData, tick, cmdReply, msg2send, maxtimeout);
						
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			
		case 16:
			/*
			// Get reply "+KSMTPPWD: <socket>"
			vTaskDelay(2);
			sprintf(msg2send, "+KSMTPPWD");
			chars2read = 2;
			countData = 2; // GSM buffer should be: <CR><LF>+KSMTPPARAM: <socket><CR><LF>
			
			resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			else
			{
				
			}	
			*/
			smInternal++;
			
		case 17:
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
 * Sets email destination
 * \param char[] email of first destination
 * \param char[] email of second destination
 * \param char[] email of first copy destination
 * \param char[] email of second copy destination
<B>Note:</B>parameter SMTP_NO_DEST can be used to ignore a parameter.
 * \return None
 */
void SMTPEmailTo(char* toDest1, char* toDest2, char* ccDest1, char* ccDest2)
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
			mainOpStatus.Function = 7;
			mainOpStatus.ErrorCode = 0;
			
			// Set Params	
			smtpTo1 = toDest1;
			smtpTo2 = toDest2;
			smtpCc1 = ccDest1;
			smtpCc2 = ccDest2;
			
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
int  cSMTPEmailTo()
{
	char cmdReply[350];
	char msg2send[350];
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
			// AT+KSMTPTO	
			sprintf(msg2send, "AT+KSMTPTO=\"%s\",",smtpTo1);
			GSMWrite(msg2send);
			GSMWrite("\"");
			GSMWrite(smtpTo2);
			GSMWrite("\",\"");
			GSMWrite(smtpCc1);
			GSMWrite("\",\"");
			GSMWrite(smtpCc2);
			GSMWrite("\"");
			GSMWrite("\r");
			
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			maxtimeout = 2;
			smInternal++;
			
		case 2:
			vTaskDelay(1);
			// Check ECHO 
			countData = 0;
			
			resCheck = CheckEcho(countData, tick, cmdReply, msg2send, maxtimeout);
						
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			else
			{
				int flushNum = 	strlen(smtpTo2) + 
								strlen(smtpCc1) + 
								strlen(smtpCc2) + 10;
				while(flushNum > 0)
				{
					GSMRead(cmdReply, 1);
					flushNum--;	
				}	
				cmdReply[0] = '\0';
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
 * Sends email to destinations
 * \param char[] subject of email 
 * \param char[] text of email
 * \return None
 */
void SMTPEmailSend(char* subject, char* text)
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
			mainOpStatus.Function = 8;
			mainOpStatus.ErrorCode = 0;
			
			// Set Params	
			smtpsubject = subject;
			smtpmsg = text;
			smtpLastSessionId = 0;
			
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
int  cSMTPEmailSend()
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
			// AT+KSMTPSUBJECT
			sprintf(msg2send, "AT+KSMTPSUBJECT=\"");
			
			GSMWrite(msg2send);
			GSMWrite(smtpsubject);
			GSMWrite("\"\r");
			
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			maxtimeout = 2;
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
			else
			{
				int flushNum = strlen(smtpsubject) + 2;
				while(flushNum > 0)
				{
					GSMRead(cmdReply, 1);
					flushNum--;	
				}	
				cmdReply[0] = '\0';
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
			maxtimeout = 60; //just to prevent hardware lock... it should be less since depends on data size
			// AT+KSMTPUL=1,<size> -> with "1,<size>" it is specified to use normal mode 
			int smtpmsglen = strlen(smtpmsg);
			sprintf(msg2send, "AT+KSMTPUL=1,%d\r",smtpmsglen);
			
			GSMWrite(msg2send);
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			
			smInternal++;
			
		case 5:
			vTaskDelay(20);
			// Check ECHO 
			countData = 0;
			
			resCheck = CheckEcho(countData, tick, cmdReply, msg2send, maxtimeout);
						
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			
		case 6:
			// Get reply "+KSMTPUL: <session_id>"
			vTaskDelay(20);
			sprintf(msg2send, "+KSMTPUL");
			chars2read = 2;
			countData = 2; // GSM buffer should be: <CR><LF>+KSMTPUL: <session_id><CR><LF>
			
			resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			else
			{
				// Get SMPT last session_id:
				char temp[25];
				int res = getfield(':', '\r', 5, 1, cmdReply, temp, 500);
				if(res != 1)
				{
					// Execute Error Handler
					gsmDebugPrint("Error in getfield for +KSMTPUL socket\r\n");
					break;
				}
				else
				{
					smtpLastSessionId = atoi(temp);
				}
			}
			
		case 7:
			// Get reply "\r\nCONNECT\r\n"
			vTaskDelay(2);
			sprintf(msg2send, "CONNECT\r\n");
			chars2read = 2;
			countData = 2; // GSM buffer should be: <CR><LF>CONNECT<CR><LF>
			
			resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			else
			{
				GSMWrite(smtpmsg);			
			}
		
		case 8:
			// Get reply (\r\nOK\r\n)
			vTaskDelay(20);
			// Get OK
			sprintf(msg2send, "\r\nOK");
			chars2read = 2;
			countData = 0; // GSM buffer should be: <CR><LF>OK<CR><LF>
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
 * Clears SMTP parameter
 * \param None
 * \return None
 */
void SMTPParamsClear()
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
			mainOpStatus.Function = 5;
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
int  cSMTPParamsClear()
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
			// AT+KSMTPSUBJECT
			sprintf(msg2send, "AT+KSMTPCLEAR\r");
			
			GSMWrite(msg2send);
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			maxtimeout = 2;
			smInternal++;
			
		case 2:
			vTaskDelay(1);
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
