/** \file FTPlib.c
 *  \brief library to manage FTP file exchange. It uses Hilo internal flash memory as file storage for Flyport module
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
 *  FileName:        FTPlib.c
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
#include "FTPlib.h"

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

static WORD xFTPPort;
extern FTP_SOCKET* xFTPSocket;
static char* xFTPServName;
static char* xFTPLogin;
static char* xFTPPassw;
static char* xFTPServFilename;
static char* xFTPServPath;
static char* xFTPFlashFilename;
static BOOL xFTPAppeMode = FALSE;
/// @endcond

/**
\defgroup FTP
@{
Provides FTP Client functions
*/

/**
 * Configures FTP Server parameters
 * \param FTP_SOCKET to be used for connection
 * \param char* to serverName string
 * \param char* to username login string
 * \param char* to password string
 * \param WORD with port number
 * \return None
 */
void FTPConfig(FTP_SOCKET* ftpSocket, char* serverName, char* login, char* password, WORD portNumber)
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
			mainOpStatus.Function = 12;
			mainOpStatus.ErrorCode = 0;
			
			// Set Params	
			xFTPSocket = ftpSocket;
			xFTPSocket->ftpError = 0;
			xFTPServName = serverName;
			xFTPLogin = login;
			xFTPPassw = password;
			xFTPPort = portNumber;
			
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
int cFTPConfig()
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
			// ----------	FTP Connection Configuration	----------
			//sprintf(msg2send, "AT+KFTPCFG=0,\"%s\",\"%s\",\"%s\",%d,1\r", xFTPServName, xFTPLogin, xFTPPassw, xFTPPort);
			sprintf(msg2send, "AT+KFTPCFG=0,\"");
			char portnum[6];
			sprintf(portnum, "%d\r", xFTPPort);
			GSMWrite(msg2send);
			GSMWrite(xFTPServName);
			GSMWrite("\",\"");
			GSMWrite(xFTPLogin);
			GSMWrite("\",\"");
			GSMWrite(xFTPPassw);
			GSMWrite("\",");
			GSMWrite(portnum);
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
				int dummyLen = strlen(xFTPServName) + strlen(xFTPLogin) + strlen(xFTPPassw) + strlen(portnum) + 10;
				while(dummyLen > 0)
				{
					GSMRead(cmdReply, 1);
					dummyLen--;
				}
				cmdReply[0] = '\0';
			}
			
		case 3:
			// Get reply "+KFTPCFG: <socket>"
			vTaskDelay(2);
			sprintf(msg2send, "+KFTPCFG");
			chars2read = 2;
			countData = 0; // GSM buffer should be: <CR><LF>+KFTPCFG: <socket><CR><LF>
			
			resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			else
			{
				// Get socket number
				char temp[25];
				int res = getfield(':', '\r', 5, 1, cmdReply, temp, 500);
				if(res != 1)
				{
					// Execute Error Handler
					gsmDebugPrint("Error in getfield\r\n");
					break;
				}
				else
				{
					xFTPSocket->number = atoi(temp);
				}	
			}	
			
		case 4:
			// Get reply (\r\nOK\r\n)
			vTaskDelay(1);
			// Get OK
			sprintf(msg2send, "OK\r\n");
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

// In FTP Receive path, we will use always the "ftp" directory
/**
 * Download a file from FTP Server
 * \param FTP_SOCKET to be used for connection
 * \param char* to flash file name string to create/overwrite
 * \param char* to server path string
 * \param char* to server file name string
 * \return None
 */
void FTPReceive(FTP_SOCKET* ftpSocket, char* flashFilename, char* serverPath, char* serverFilename)
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
			mainOpStatus.Function = 13;
			mainOpStatus.ErrorCode = 0;
			
			// Set Params	
			xFTPSocket = ftpSocket;
			xFTPFlashFilename = flashFilename;
			xFTPServPath = serverPath;
			xFTPServFilename = serverFilename;

			
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
int cFTPReceive()
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
			
			// ----------	FTP Receive Command ----------
			// AT+KFTPRCV = <xFTPSocket->number>,
			//				"/<xFTPFlashFilename>",
			//				"/<xFTPServPath>",
			//				"<xFTPFilename>,
			//				0\r
			
			sprintf(msg2send, "AT+KFTPRCV=%d,\"/", xFTPSocket->number);
			GSMWrite(msg2send);
			GSMWrite(xFTPFlashFilename);
			GSMWrite("\",\"");
			GSMWrite(xFTPServPath);
			GSMWrite("\",\"");
			GSMWrite(xFTPServFilename);
			GSMWrite("\",0\r");
			
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			maxtimeout = 60;
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
				int dummyLen = strlen(xFTPFlashFilename) + strlen(xFTPServPath) + strlen(xFTPServFilename) + 10;
				while(dummyLen > 0)
				{
					GSMRead(cmdReply, 1);
					dummyLen--;
				}
				cmdReply[0] = '\0';
			}
			
		case 3:
			// Get reply (\r\nOK\r\n)
			vTaskDelay(1);
			// Get OK
			sprintf(msg2send, "OK\r\n");
			chars2read = 2;
			countData = 2; // GSM buffer should be: <CR><LF>OK<CR><LF>
			resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			
			
		case 4:
			// Get reply "+KFTP_RCV_DONE: <session_id>"
			vTaskDelay(1);
			sprintf(msg2send, "+KFTP_RCV_DONE");
			chars2read = 2;
			countData = 2; // GSM buffer should be: <CR><LF>+KFTP_RCV_DONE: <session_id><CR><LF>
			
			resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			else
			{
				// Get FTP Socket last session_id:
				char temp[25];
				int res = getfield(':', '\r', 5, 1, cmdReply, temp, 500);
				if(res != 1)
				{
					// Execute Error Handler
					gsmDebugPrint("Error in getfield for +KFTP_RCV_DONE socket\r\n");
					break;
				}
				else
				{
					xFTPSocket->number = atoi(temp);
				}
			}
			
		case 5:
			// ----------	FTP Close Socket Command ----------
			sprintf(msg2send, "AT+KFTPCLOSE=%d\r", xFTPSocket->number);
			GSMWrite(msg2send);
			
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			maxtimeout = 2;
			smInternal++;	
				
		case 6:
			vTaskDelay(1);
			// Check ECHO 
			countData = 0;
			
			resCheck = CheckEcho(countData, tick, cmdReply, msg2send, maxtimeout);
						
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			
		case 7:
			// Get reply (\r\nOK\r\n)
			vTaskDelay(1);
			// Get OK
			sprintf(msg2send, "OK\r\n");
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
 * Uploads a file to FTP Server
 * \param FTP_SOCKET to be used for connection
 * \param char* to flash file name string to upload
 * \param char* to server path string
 * \param char* to server file name string
 * \param BOOL appendMode: TRUE enables append, FALSE disables append
 * \return None
 */
void FTPSend(FTP_SOCKET* ftpSocket, char* flashFilename, char* serverPath, char* serverFilename, BOOL appendMode)
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
			mainOpStatus.Function = 14;
			mainOpStatus.ErrorCode = 0;
			
			// Set Params	
			xFTPSocket = ftpSocket;
			xFTPFlashFilename = flashFilename;
			xFTPServPath = serverPath;
			xFTPServFilename = serverFilename;
			xFTPAppeMode = appendMode;
			
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
int cFTPSend()
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
			
			// ----------	FTP Send Command ----------
			// AT+KFTPSND = <xFTPSocket->number>,
			//				"/<xFTPFlashFilename>",
			//				"/<xFTPServPath>",
			//				"<xFTPFilename>,
			//				0,
			//				<append 0/1>\r
			
			sprintf(msg2send, "AT+KFTPSND=%d,\"/", xFTPSocket->number);
			GSMWrite(msg2send);
			GSMWrite(xFTPFlashFilename);
			GSMWrite("\",\"");
			GSMWrite(xFTPServPath);
			GSMWrite("\",\"");
			GSMWrite(xFTPServFilename);
			GSMWrite("\",0,");
			if(xFTPAppeMode == TRUE)
				GSMWrite("1\r");
			else
				GSMWrite("0\r");
			
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			maxtimeout = 60;
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
				int dummyLen = strlen(xFTPFlashFilename) + strlen(xFTPServPath) + strlen(xFTPServFilename) + 14;
				while(dummyLen > 0)
				{
					GSMRead(cmdReply, 1);
					dummyLen--;
				}
				cmdReply[0] = '\0';
			}
			
		case 3:
			// Get reply (\r\nOK\r\n)
			vTaskDelay(1);
			// Get OK
			sprintf(msg2send, "OK\r\n");
			chars2read = 2;
			countData = 0; // GSM buffer should be: <CR><LF>OK<CR><LF>
			resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			
			
		case 4:
			// Get reply "+KFTP_SND_DONE: <session_id>"
			vTaskDelay(1);
			sprintf(msg2send, "+KFTP_SND_DONE");
			chars2read = 2;
			countData = 2; // GSM buffer should be: <CR><LF>+KFTP_SND_DONE: <session_id><CR><LF>
			
			resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			else
			{
				// Get FTP Socket last session_id:
				char temp[25];
				int res = getfield(':', '\r', 5, 1, cmdReply, temp, 500);
				if(res != 1)
				{
					// Execute Error Handler
					gsmDebugPrint("Error in getfield for +KFTP_SND_DONE socket\r\n");
					break;
				}
				else
				{
					xFTPSocket->number = atoi(temp);
				}
			}
			
		case 5:
			// ----------	FTP Close Socket Command ----------
			sprintf(msg2send, "AT+KFTPCLOSE=%d\r", xFTPSocket->number);
			GSMWrite(msg2send);
			
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			maxtimeout = 2;
			smInternal++;	
				
		case 6:
			vTaskDelay(1);
			// Check ECHO 
			countData = 0;
			
			resCheck = CheckEcho(countData, tick, cmdReply, msg2send, maxtimeout);
						
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			
		case 7:
			// Get reply (\r\nOK\r\n)
			vTaskDelay(1);
			// Get OK
			sprintf(msg2send, "OK\r\n");
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
 * Deletes a file from FTP Server
 * \param FTP_SOCKET to be used for connection
 * \param char* to server path string
 * \param char* to server file name string
 * \return None
 */
void FTPDelete(FTP_SOCKET* ftpSocket, char* serverPath, char* serverFilename)
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
			mainOpStatus.Function = 15;
			mainOpStatus.ErrorCode = 0;
			
			// Set Params	
			xFTPSocket = ftpSocket;
			xFTPServPath = serverPath;
			xFTPServFilename = serverFilename;
			
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
int cFTPDelete()
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
			
			// ----------	FTP Delete File Command ----------
			// AT+KFTPDEL = <xFTPSocket->number>,
			//				"/<xFTPServPath>",
			//				"<xFTPFilename>,
			//				0\r,
			sprintf(msg2send, "AT+KFTPDEL=%d,\"", xFTPSocket->number);
			GSMWrite(msg2send);
			GSMWrite(xFTPServPath);
			GSMWrite("\",\"");
			GSMWrite(xFTPServFilename);
			GSMWrite("\",0\r");
			
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			maxtimeout = 60;
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
				int dummyLen = strlen(xFTPServPath) + strlen(xFTPServFilename) + 9;
				while(dummyLen > 0)
				{
					GSMRead(cmdReply, 1);
					dummyLen--;
				}
				cmdReply[0] = '\0';
			}
			
		case 3:
			// Get reply (\r\nOK\r\n)
			vTaskDelay(1);
			// Get OK
			sprintf(msg2send, "OK\r\n");
			chars2read = 2;
			countData = 0; // GSM buffer should be: <CR><LF>OK<CR><LF>
			resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
				
		case 4:
			// ----------	FTP Close Socket Command ----------
			sprintf(msg2send, "AT+KFTPCLOSE=%d\r", xFTPSocket->number);
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
			sprintf(msg2send, "OK\r\n");
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

