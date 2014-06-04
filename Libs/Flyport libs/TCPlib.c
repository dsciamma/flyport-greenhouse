/** \file TCPlib.c
 *  \brief library to handle TCP Client sockets functionalities
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
 *  FileName:        TCPlib.c
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
#include "TCPlib.h"

extern void callbackDbg(BYTE smInt);

extern int Cmd;
extern int mainGSMStateMachine;
extern OpStat	mainOpStatus;
extern GSMModule mainGSM;

extern int CheckCmd(int countData, int chars2read, const DWORD tick, char* cmdReply, const char* msg2send, const BYTE maxtimeout);
extern int CheckEcho(int countData, const DWORD tick, char* cmdReply, const char* msg2send, const BYTE maxtimeout);
extern void CheckErr(int result, BYTE* smInt, DWORD* tickUpdate);
extern void gsmDebugPrint(char*);

static BYTE smInternal = 0; // State machine for internal use of callback functions
static BYTE maxtimeout = 2;
static int counterLen = 0;

static char* tcpWriteBuffer;
static int tcpWriteBufferCount;
static char* tcpReadBuffer;
static int tcpReadBufferCount;
/// @endcond

/**
\defgroup TCP
@{
Provides TCP Client functions
*/


/**
 * Creates a TCP client on specified IP address and port
 * \param tcpaddr - IP address of the remote server. Example: "192.168.1.100" (the char array must be NULL terminated).
 * \param tcpport - Port of the remote server to connect. Example: "1234" (the char array must be NULL terminated).
 * \param tcpsock - TCP_SOCKET of reference.
 * \return - INVALID_SOCKET: the operation was failed. Maybe there are not available sockets.
 * \return - A TCP_SOCKET handle to the created socket. It must be used to access the socket in the program (read/write operations and close socket).
 */
void TCPClientOpen(TCP_SOCKET* sock, char* tcpaddr, char* tcpport)
{
	BOOL opok = FALSE;
	
	if(mainGSM.HWReady != TRUE)
		return;
	
	// Check if sock->number is INVALID SOCKET
	if(sock->number != INVALID_SOCKET)
		return;
			
	//	Function cycles until it is not executed
	while (!opok)
	{
		while (xSemaphoreTake(xSemFrontEnd,0) != pdTRUE);		//	xSemFrontEnd TAKE

		// Check mainOpStatus.ExecStat
		if (mainOpStatus.ExecStat != OP_EXECUTION)
		{		
			mainOpStatus.ExecStat = OP_EXECUTION;
			mainOpStatus.Function = 20;
			mainOpStatus.ErrorCode = 0;
			
			// Set Params	
			xTCPPort = atoi(tcpport);
			int termChar = strlen(tcpaddr);
			if(termChar > 100) // xIPAddress array size is 100
				termChar = 100;
			strncpy((char*)xIPAddress, tcpaddr, termChar);
			xIPAddress[termChar] = '\0';
			xSocket = sock;
			//xSocket->number = INVALID_SOCKET;
			
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
//	cTCPClientOpen callback function
//****************************************************************************
int cTCPClientOpen()
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
			// ----------	TCP Connection Configuration	----------
			sprintf(msg2send, "AT+KTCPCFG=0,0,\"%s\",%d\r",xIPAddress, xTCPPort);
			
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
			// Get reply "+KTCPCFG: <socket>"
			vTaskDelay(1);
			sprintf(msg2send, "+KTCPCFG");
			chars2read = 2;
			countData = 2; // GSM buffer should be: <CR><LF>+KTCPCFG: <socket><CR><LF>
			
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
					gsmDebugPrint( "Error in getfield\r\n");
					break;
				}
				else
				{
					xSocket->number = atoi(temp);
				}	
			}	
			
		case 4:
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

        case 5:
			// AT+KCNXTIMER
			sprintf(msg2send, "AT+KCNXTIMER=0,60,2,70\r");

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
			sprintf(msg2send, "OK");
			chars2read = 2;
			countData = 2; // GSM buffer should be: <CR><LF>OK<CR><LF>
			resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);

			CheckErr(resCheck, &smInternal, &tick);

			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			
        case 8://5:
			// ----------	Initiate TCP Connection	----------
			sprintf(msg2send, "AT+KTCPCNX=%d\r",xSocket->number);
			
			GSMWrite(msg2send);
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			maxtimeout = 70;
			smInternal++;
			
        case 9://6:
			vTaskDelay(1);
			// Check ECHO 
			countData = 0;
			
			resCheck = CheckEcho(countData, tick, cmdReply, msg2send, maxtimeout);
						
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				xSocket->number = INVALID_SOCKET;
				return mainOpStatus.ErrorCode;
			}
			
        case 10://7:
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
				xSocket->number = INVALID_SOCKET;
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
 * Closes the client socket specified by the handle.
 * \param Sockclose - The handle of the socket to close (the handle returned by the command TCPClientOpen).
 * \return None.
 */
void TCPClientClose (TCP_SOCKET* sock)
{
	BOOL opok = FALSE;
	
	if(mainGSM.HWReady != TRUE)
		return;
	
	if(sock->number == INVALID_SOCKET)
		return;
	
	//	Function cycles until it is not executed
	while (!opok)
	{
		while (xSemaphoreTake(xSemFrontEnd,0) != pdTRUE);		//	xSemFrontEnd TAKE

		// Check mainOpStatus.ExecStat
		if (mainOpStatus.ExecStat != OP_EXECUTION)
		{		
			mainOpStatus.ExecStat = OP_EXECUTION;
			mainOpStatus.Function = 21;
			mainOpStatus.ErrorCode = 0;
			
			// Set Params	
			xSocket = sock;
			
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
//	cTCPClientClose callback function
//****************************************************************************
int cTCPClientClose()
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
			// ----------	TCP Connection Close	----------
			sprintf(msg2send, "AT+KTCPCLOSE=%d,1\r",xSocket->number);
			
			GSMWrite(msg2send);
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			maxtimeout = 60 + 2;
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
			// Send AT command
			// ----------	Delete TCP Socket ----------
			sprintf(msg2send, "AT+KTCPDEL=%d\r",xSocket->number);
			
			GSMWrite(msg2send);
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			maxtimeout = 2+2;
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
				xSocket->number = INVALID_SOCKET;
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
 * Reads the specified number of characters from a TCP socket and puts them into the specified char array. <B>NOTE:</B> This function flushes the buffer after reading!
 * \param socktoread - The handle of the socket to read (the handle returned by the command TCPClientOpen or TCPServerOpen).
 * \param readch - The char array to fill with the read characters.
 * \param rlen - The number of characters to read. 
 * \warning The length of the array must be AT LEAST = rlen+1, because at the end of the operation the array it's automatically NULL terminated (is added the '\0' character).
 * \return None.
 */
void TCPRead(TCP_SOCKET* sock , char* readch , int rlen)
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
			mainOpStatus.Function = 24;
			mainOpStatus.ErrorCode = 0;
			
			// Set Params	
			xSocket = sock;
			tcpReadBuffer = readch;
			
			// Set max of socket buffer size
			if(rlen > 1460)
				rlen = 1460;
			tcpReadBufferCount = rlen;
			
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
//	cTCPRead callback function
//****************************************************************************
int cTCPRead()
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
			// ----------	TCP Status Update ----------
			sprintf(msg2send, "AT+KTCPSTAT=%d\r",xSocket->number);
			
			GSMWrite(msg2send);
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			maxtimeout = 60;
			smInternal++;
			
		case 2:
			vTaskDelay(20);
			// Check ECHO 
			countData = 0;
			
			resCheck = CheckEcho(countData, tick, cmdReply, msg2send, maxtimeout);
						
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				#if defined (STACK_USE_UART)
				char debugStr[100];
				sprintf(debugStr, "error on function %d, smInternal %d\r\n", mainOpStatus.Function, smInternal);
				gsmDebugPrint( debugStr);
				vTaskDelay(20);
				#endif
				return mainOpStatus.ErrorCode;
			}
			
		case 3:
			// Get reply "\r\n+KTCPSTAT: <status>,<tcp_notif>,<rem_data>,<rcv_data>\r\n"
			vTaskDelay(20);
			sprintf(msg2send, "+KTCPSTAT");
			chars2read = 2;
			countData = 2; // GSM buffer should be: <CR><LF>+KTCPSTAT: <status>,<tcp_notif>,<rem_data>,<rcv_data><CR><LF>
			
			resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			else
			{
				// Get status
				char temp[25];
				int res = getfield(':', ',', 5, 1, cmdReply, temp, 500);
				if(res != 1)
				{
					// Execute Error Handler
					gsmDebugPrint( "Error in getfield for +KTCPSTAT socket\r\n");
					break;
				}
				else
				{
					xSocket->status = atoi(temp);
				}
				
				// Get tcp_notif
				res = getfield(',', ',', 5, 1, cmdReply, temp, 500);
				if(res != 1)
				{
					// Execute Error Handler
					gsmDebugPrint( "Error in getfield for +KTCPSTAT socket\r\n");
					break;
				}
				else
				{
					xSocket->notif = atoi(temp);
				}
				
				// Get rcv_data
				res = getfield(',', '\r', 6, 3, cmdReply, temp, 500);
				if(res != 1)
				{
					// Execute Error Handler
					gsmDebugPrint( "Error in getfield for +KTCPSTAT socket\r\n");
					break;
				}
				else
				{
					xSocket->rxLen = atoi(temp);
				}
			}	
			
		case 4:
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
			
		case 5:
			if(xSocket->rxLen == 0)
			{
				// Break operation
				break;
			}
			// Send first AT command
			// ----------	TCP Read Command ----------
			if(tcpReadBufferCount > xSocket->rxLen)
				tcpReadBufferCount = xSocket->rxLen;				
			
			if(tcpReadBufferCount > GSM_BUFFER_SIZE)
					tcpReadBufferCount = GSM_BUFFER_SIZE;
			
			sprintf(msg2send, "AT+KTCPRCV=%d,%d\r",xSocket->number, tcpReadBufferCount);
			
			GSMWrite(msg2send);
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			maxtimeout = 60;
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
			// Get reply "\r\nCONNECT\r\n"
			vTaskDelay(1);
			sprintf(msg2send, "CONNECT");
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
				// Read data from TCP Socket
				int rxCount = 0;
				int retCount = 0;			
				while(rxCount < tcpReadBufferCount)
				{
					char tmp[1];
					retCount = GSMRead(tmp, 1);
					*(tcpReadBuffer+rxCount) = tmp[0];
					rxCount += retCount;
					if(retCount == 0)
						vTaskDelay(1);
				}
				
				// Set tcpReadBufferCount as the effective number of BYTEs read
				tcpReadBufferCount = rxCount;
			}
		
		case 8:
			// Get reply (--EOF--Pattern--\r\nOK\r\n)
			vTaskDelay(1);
			// Get OK
			sprintf(msg2send, "--EOF--Pattern--\r\nOK\r\n");
			chars2read = 2;
			countData = 0; // GSM buffer should be: --EOF--Pattern--<CR><LF>OK<CR><LF>
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
 * Writes an array of characters on the specified socket.
 * \param socktowrite - The socket to which data is to be written (it's the handle returned by the command TCPClientOpen or TCPServerOpen).
 * \param writech - Pointer to the array of characters to be written.
 * \param wlen - The number of characters to write.
 * \return The number of bytes written to the socket. If less than len, the buffer became full or the socket is not conected.
 */
void TCPWrite(TCP_SOCKET* sock , char* writech , int wlen)
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
			mainOpStatus.Function = 23;
			mainOpStatus.ErrorCode = 0;
			
			// Set Params	
			xSocket = sock;
			tcpWriteBuffer = writech;
			tcpWriteBufferCount = wlen;
			
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
//	cTCPWrite callback function
//****************************************************************************
int cTCPWrite()
{
	char cmdReply[200];
	char msg2send[200];
	int resCheck = 0;
	DWORD tick;
	int countData;
	int chars2read;
    counterLen = 0;
	
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
			// ----------	TCP Write Command ----------
			sprintf(msg2send, "AT+KTCPSND=%d,%d\r",xSocket->number, tcpWriteBufferCount);
			
			GSMWrite(msg2send);
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			maxtimeout = 60;
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
			// Get reply "\r\nCONNECT\r\n"
			vTaskDelay(20);
			sprintf(msg2send, "\r\nCONNECT");
			chars2read = 2;
			countData = 4; // GSM buffer should be: <CR><LF>CONNECT<CR><LF>
			
			resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			else
			{
				while((counterLen < tcpWriteBufferCount) && (GSMBufferSize() == 0))
				{
					GSMWriteCh(tcpWriteBuffer[counterLen]);
					counterLen++;
				}		
                if(GSMBufferSize() > 0)
                {
                    // Check if NO CARRIER message arrived
                    smInternal++;
                }
				// and write --EOF--Pattern-- (without \r)
				GSMWrite("--EOF--Pattern--");
			}	
	
		case 4:
			// Get reply (\r\nOK\r\n)
			vTaskDelay(20);
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
 * Updates the status of a TCP socket.
 * \param sockconn - The handle of the socket to control.
 * \return none
 */
void TCPStatus(TCP_SOCKET* sock)
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
			mainOpStatus.Function = 22;
			mainOpStatus.ErrorCode = 0;
			
			// Set Params	
			xSocket = sock;
			
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
//	cTCPStatus callback function
//****************************************************************************
int cTCPStatus()
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
			// ----------	TCP Status Update ----------
			sprintf(msg2send, "AT+KTCPSTAT=%d\r",xSocket->number);
			
			GSMWrite(msg2send);
			// Start timeout count
			tick = TickGetDiv64K(); // 1 tick every seconds
			maxtimeout = 70;
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
			// Get reply "\r\n+KTCPSTAT: <status>,<tcp_notif>,<rem_data>,<rcv_data>\r\n"
			vTaskDelay(20);
			sprintf(msg2send, "+KTCPSTAT");
			chars2read = 2;
			countData = 2; // GSM buffer should be: <CR><LF>+KTCPSTAT: <status>,<tcp_notif>,<rem_data>,<rcv_data><CR><LF>
			
			resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);
			
			CheckErr(resCheck, &smInternal, &tick);
			
			if(resCheck)
			{
				return mainOpStatus.ErrorCode;
			}
			else
			{
				// Get status
				char temp[25];
				int res = getfield(':', ',', 5, 1, cmdReply, temp, 500);
				if(res != 1)
				{
					// Execute Error Handler
					gsmDebugPrint( "Error in getfield for +KTCP_DATA socket\r\n");
					break;
				}
				else
				{
					xSocket->status = atoi(temp);
				}
				
				// Get tcp_notif
				res = getfield(',', ',', 5, 1, cmdReply, temp, 500);
				if(res != 1)
				{
					// Execute Error Handler
					gsmDebugPrint( "Error in getfield for +KTCP_DATA socket\r\n");
					break;
				}
				else
				{
					xSocket->notif = atoi(temp);
				}
				
				// Get rcv_data
				res = getfield(',', '\r', 6, 3, cmdReply, temp, 500);
				if(res != 1)
				{
					// Execute Error Handler
					gsmDebugPrint( "Error in getfield for +KTCP_DATA socket\r\n");
					break;
				}
				else
				{
					xSocket->rxLen = atoi(temp);
				}
			}	
			
		case 4:
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
 * Empty specified TCP socket RX Buffer.
 * \param sockflush - The handle of the socket to empty (the handle returned by the command TCPClientOpen or TCPServerOpen).
 * \return none.
 */
void TCPRxFlush(TCP_SOCKET* sock)
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
			mainOpStatus.Function = 25;
			mainOpStatus.ErrorCode = 0;
			
			// Set Params	
			xSocket = sock;
			
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
//	cTCPisConn callback function
//****************************************************************************
int cTCPRxFlush()
{
	char cmdReply[200];
	char msg2send[200];
	int resCheck = 0;
	DWORD tick;
	int countData;
	int chars2read;
	BOOL repeatLoop = TRUE;
	
	do
	{
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
				// ----------	TCP Status Update ----------
				sprintf(msg2send, "AT+KTCPSTAT=%d\r",xSocket->number);
				
				GSMWrite(msg2send);
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
				
			case 3:
				// Get reply "\r\n+KTCPSTAT: <status>,<tcp_notif>,<rem_data>,<rcv_data>\r\n"
				vTaskDelay(20);
				sprintf(msg2send, "+KTCPSTAT");
				chars2read = 2;
				countData = 2; // GSM buffer should be: <CR><LF>+KTCPSTAT: <status>,<tcp_notif>,<rem_data>,<rcv_data><CR><LF>
				
				resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);
				
				CheckErr(resCheck, &smInternal, &tick);
				
				if(resCheck)
				{
					return mainOpStatus.ErrorCode;
				}
				else
				{
					// Get status
					char temp[25];
					int res = getfield(':', ',', 5, 1, cmdReply, temp, 500);
					if(res != 1)
					{
						// Execute Error Handler
						gsmDebugPrint( "Error in getfield for +KTCPSTAT socket\r\n");
						break;
					}
					else
					{
						xSocket->status = atoi(temp);
					}
					
					// Get tcp_notif
					res = getfield(',', ',', 5, 1, cmdReply, temp, 500);
					if(res != 1)
					{
						// Execute Error Handler
						gsmDebugPrint( "Error in getfield for +KTCPSTAT socket\r\n");
						break;
					}
					else
					{
						xSocket->notif = atoi(temp);
					}
					
					// Get rcv_data
					res = getfield(',', '\r', 6, 3, cmdReply, temp, 500);
					if(res != 1)
					{
						// Execute Error Handler
						gsmDebugPrint( "Error in getfield for +KTCPSTAT socket\r\n");
						break;
					}
					else
					{
						xSocket->rxLen = atoi(temp);
					}	
				}	
				
			case 4:
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
				
			case 5:
				if(xSocket->rxLen == 0)
				{
					// Break operation
					repeatLoop = FALSE;
					break;
				}
				// Send first AT command
				// ----------	TCP Read Command ----------
				tcpReadBufferCount = xSocket->rxLen;				
				
				if(tcpReadBufferCount > GSM_BUFFER_SIZE)
						tcpReadBufferCount = GSM_BUFFER_SIZE;
				
				sprintf(msg2send, "AT+KTCPRCV=%d,%d\r",xSocket->number, tcpReadBufferCount);
				
				GSMWrite(msg2send);
				// Start timeout count
				tick = TickGetDiv64K(); // 1 tick every seconds
				maxtimeout = 60;
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
				// Get reply "\r\nCONNECT\r\n"
				vTaskDelay(2);
				sprintf(msg2send, "CONNECT");
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
					// Read data from TCP Socket
					int rxCount = 0;
					
					while(rxCount < tcpReadBufferCount)
					{
						char tmp[1];
						GSMRead(tmp, 1);
						//*(tcpReadBuffer+rxCount) = tmp[0];
						rxCount++;
					}
					
					// Set tcpReadBufferCount as the effective number of BYTEs read
					tcpReadBufferCount = rxCount;
				}
			
			case 8:
				// Get reply (\r\n--EOF--Pattern--\r\nOK\r\n)
				vTaskDelay(1);
				// Get OK
				sprintf(msg2send, "--EOF--Pattern--\r\nOK");
				chars2read = 2;
				countData = 2; // GSM buffer should be: <CR><LF>--EOF--Pattern--<CR><LF>OK<CR><LF>
				resCheck = CheckCmd(countData, chars2read, tick, cmdReply, msg2send, maxtimeout);
				
				CheckErr(resCheck, &smInternal, &tick);
				
				if(resCheck)
				{
					return mainOpStatus.ErrorCode;
				}
				else
				{
					// In this case, we should execute again the commands
					// starting from "AT+KTCPSTAT", just to be sure that 
					// TCP RX Buffer is blank
					vTaskDelay(20);
					repeatLoop = TRUE;
					smInternal = 1;
					break;
				}
				
			default:
				break;
		
		}
	} while(repeatLoop == TRUE);
		
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
