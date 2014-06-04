/** \file FSlib.c
 *  \brief library to manage Hilo module internal flash files
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
 *  FileName:        FSlib.c
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
#include "FSlib.h"

extern int Cmd;
extern int mainGSMStateMachine;
extern OpStat	mainOpStatus;
extern GSMModule mainGSM;

extern int CheckCmd(int countData, int chars2read, const DWORD tick, char* cmdReply, const char* msg2send, const BYTE maxtimeout);
extern int CheckEcho(int countData, const DWORD tick, char* cmdReply, const char* msg2send, const BYTE maxtimeout);
extern void CheckErr(int result, BYTE* smInt, DWORD* tickUpdate);
extern void callbackDbg(BYTE smInt);
extern void gsmDebugPrint(char*);

static BYTE smInternal = 0; // State machine for internal use of callback functions
static BYTE maxtimeout = 2;

static char* xFSFilename;
static BYTE* xFSBuffer;
static unsigned int xFSDatasize;
static int* xFSSize;
/// @endcond

/**
\defgroup Hilo Flash Files
@{
FSlib contains the functions to manage Hilo internal flash files

*/

/**
 * Writes data on a file. If files exists, this function overrides all data.
 * \param filename - char[] with name of file to write
 * \param dataBuffer - BYTE[] with data to write
 * \param dataSize - amount of data to write
 * \return None
 */
void FSWrite(char* filename, BYTE* dataBuffer, unsigned int dataSize)
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
			mainOpStatus.Function = 30;
			mainOpStatus.ErrorCode = 0;
			
			// Set Params	
			xFSFilename = filename;
			xFSBuffer = dataBuffer;
			xFSDatasize = dataSize;
			
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
int cFSWrite()
{
	char cmdReply[200];
	char msg2send[200];
	int resCheck = 0;
	DWORD tick;
	int countData;
	int chars2read;
	char sizeStr[10];
	
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
			// ----------	KFSFILE File Write Command ----------
			// AT+KFSFILE = 0, <uri>,<NbData>\r
			sprintf(sizeStr, "%d\r", xFSDatasize);
			sprintf(msg2send, "AT+KFSFILE=0,\"/ftp/");
			GSMWrite(msg2send);
			GSMWrite(xFSFilename);
			GSMWrite("\",");
			GSMWrite(sizeStr);
			
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
				int dummyLen = strlen(xFSFilename) + strlen(sizeStr) + 4;
				while(dummyLen > 0)
				{
					GSMRead(cmdReply, 1);
					dummyLen--;
				}
				cmdReply[0] = '\0';
			}
			
		case 3:
			// Get reply (\r\nCONNECT\r\n)
			sprintf(msg2send, "CONNECT\r\n");
			chars2read = 2;
			countData = 0; // GSM buffer should be: <CR><LF>CONNECT<CR><LF>
			resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			else
			{
				int cc;
				for(cc = 0; cc < xFSDatasize; cc++)
				{
					GSMWriteCh(xFSBuffer[cc]);
				}
				//GSMWrite("--EOF--Pattern--");
			}
			
			
		case 4:
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
 * Reads data from a file. 
 * \param filename - char[] with name of file to read
 * \param dataBuffer - BYTE[] to fill with data. <I>Please, remember to use the "&" operator</I>
 * \param dataSize - amount of data to read
 * \return None
 */
void FSRead(char* filename, BYTE* dataBuffer, unsigned int dataSize)
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
			mainOpStatus.Function = 31;
			mainOpStatus.ErrorCode = 0;
			
			// Set Params	
			xFSFilename = filename;
			xFSBuffer = dataBuffer;
			xFSDatasize = dataSize;
			
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
int cFSRead()
{
	char cmdReply[200];
	char msg2send[200];
	int resCheck = 0;
	DWORD tick;
	int countData;
	int chars2read;
	char sizeStr[10];
	
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
			// ----------	KFSFILE File Read Command ----------
			// AT+KFSFILE=1,<uri>,<NbData>\r
			sprintf(sizeStr, "%d\r", xFSDatasize);
			sprintf(msg2send, "AT+KFSFILE=1,\"/ftp/");
			GSMWrite(msg2send);
			GSMWrite(xFSFilename);
			GSMWrite("\",");
			GSMWrite(sizeStr);
			
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
				int dummyLen = strlen(xFSFilename) + strlen(sizeStr) + 2;
				while(dummyLen > 0)
				{
					GSMRead(cmdReply, 1);
					dummyLen--;
				}
				cmdReply[0] = '\0';
			}
			
		case 3:
			// Get reply (\r\nCONNECT\r\n)
			sprintf(msg2send, "CONNECT\r\n");
			chars2read = 2;
			countData = 0; // GSM buffer should be: <CR><LF>CONNECT<CR><LF>
			resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			else
			{
				// Read data from file
				int rxCount = 0;
				
				while(rxCount < xFSDatasize)
				{
					char tmp[2];
					GSMRead(tmp, 1);
					*(xFSBuffer+rxCount) = tmp[0];
					rxCount++;
				}
				*(xFSBuffer+xFSDatasize) = '\0';
				// Set tcpReadBufferCount as the effective number of BYTEs read
				xFSDatasize = rxCount;
			}
			
			
		case 4:
			// Get reply (\r\nOK\r\n)
			vTaskDelay(1);
			// Get OK
			sprintf(msg2send, "\r\nOK\r\n");
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
 * Deletes file from flash memory. 
 * \param filename - char[] with name of file to delete
 * \return None
 */
void FSDelete(char* filename)
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
			mainOpStatus.Function = 32;
			mainOpStatus.ErrorCode = 0;
			
			// Set Params	
			xFSFilename = filename;
			
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
int cFSDelete()
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
			// ----------	KFSFILE File Delete Command ----------
			// AT+KFSFILE=2,<uri>\r
			sprintf(msg2send, "AT+KFSFILE=2,\"/ftp/");
			GSMWrite(msg2send);
			GSMWrite(xFSFilename);
			GSMWrite("\"\r");
			
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
				int dummyLen = strlen(xFSFilename) + 4;
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
 * Provide file size (in bytes). 
 * \param filename - char[] with name of file
 * \param dataSize - int number of data size of file.<I>Please, remember to use the "&" operator</I>
 * \return None
 */
void FSSize(char* filename, int* dataSize)
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
			mainOpStatus.Function = 33;
			mainOpStatus.ErrorCode = 0;
			
			// Set Params	
			xFSFilename = filename;
			xFSSize = dataSize;
			
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
int cFSSize()
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
			// ----------	KFSFILE File Size Command ----------
			// AT+KFSFILE=3,<uri>\r
			sprintf(msg2send, "AT+KFSFILE=3,\"/ftp/");
			GSMWrite(msg2send);
			GSMWrite(xFSFilename);
			GSMWrite("\"\r");
			
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
				int dummyLen = strlen(xFSFilename) + 4;
				while(dummyLen > 0)
				{
					GSMRead(cmdReply, 1);
					dummyLen--;
				}
				cmdReply[0] = '\0';
			}
			
		case 3:
			// Get reply (\r\n+KFSFILE: <size>\r\n)
			sprintf(msg2send, "+KFSFILE: ");
			chars2read = 2;
			countData = 0; // GSM buffer should be: <CR><LF>CONNECT<CR><LF>
			resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			else
			{
				// Get size
				char temp[25];
				int res = getfield(':', '\r', 5, 1, cmdReply, temp, 500);
				if(res != 1)
				{
					// Execute Error Handler
					gsmDebugPrint("Error in getfield for +KFSFILE socket\r\n");
					*xFSSize = 0;
					break;
				}
				else
				{
					*xFSSize = atoi(temp);
				}
			}
			
			
		case 4:
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
 * Appends data on a file.
 * \param filename - char[] with name of file to write
 * \param dataBuffer - BYTE[] with data to write
 * \param dataSize - amount of data to write
 * \return None
 */
void FSAppend(char* filename, BYTE* dataBuffer, unsigned int dataSize)
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
			mainOpStatus.Function = 34;
			mainOpStatus.ErrorCode = 0;
			
			// Set Params	
			xFSFilename = filename;
			xFSBuffer = dataBuffer;
			xFSDatasize = dataSize;
			
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
int cFSAppend()
{
	char cmdReply[200];
	char msg2send[200];
	int resCheck = 0;
	DWORD tick;
	int countData;
	int chars2read;
	char sizeStr[10];
	
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
			// ----------	KFSFILE File Write Command ----------
			// AT+KFSFILE = 0, <uri>,<NbData>\r
			sprintf(sizeStr, "%d\r", xFSDatasize);
			sprintf(msg2send, "AT+KFSFILE=5,\"/ftp/");
			GSMWrite(msg2send);
			GSMWrite(xFSFilename);
			GSMWrite("\",");
			GSMWrite(sizeStr);
			
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
				int dummyLen = strlen(xFSFilename) + strlen(sizeStr) + 4;
				while(dummyLen > 0)
				{
					GSMRead(cmdReply, 1);
					dummyLen--;
				}
				cmdReply[0] = '\0';
			}
			
		case 3:
			// Get reply (\r\nCONNECT\r\n)
			sprintf(msg2send, "CONNECT\r\n");
			chars2read = 2;
			countData = 0; // GSM buffer should be: <CR><LF>CONNECT<CR><LF>
			resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			else
			{
				int cc;
				for(cc = 0; cc < xFSDatasize; cc++)
				{
					GSMWriteCh(xFSBuffer[cc]);
				}
				//GSMWrite("--EOF--Pattern--");
			}
			
			
		case 4:
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
// @endcond

/*! @} */

/*! @} */
