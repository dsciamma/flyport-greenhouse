/** \file HTTPlib.c
 *  \brief library to manage HTTP GET or POST Requests using data connection
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
 *  FileName:        HTTPlib.c
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
#include "HTTPlib.h"

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

static int  xHTTPCode = 0;
static char* httpReqUrl;
static BYTE httpReqType = 0;
static char* httpParams;
static char* httpWriteBuffer;
static char* httpReadBuffer;
static int httpReadBufferCount;
/// @endcond

/**
\defgroup HTTP
@{
Provides functions to send HTTP Get and POST request, poll socket status and read response data
*/

/**
 * Opens a connection to HTTP host
 * \param TCP_SOCKET to use. <I>Please, remember to use the "&" operator</I>
 * \param name of HTTP host, or its IP address
 * \param name of HTTP port
 * \return None
 */
void HTTPOpen(TCP_SOCKET* sock, char* httphost, char* httpport)
{
	return TCPClientOpen(sock, httphost, httpport);
}

/**
 * Closes connection to HTTP host
 * \param TCP_SOCKET to use
 * \return None
 */
void HTTPClose(TCP_SOCKET* sock)
{
	return TCPClientClose(sock);
}

/**
 * Fills TCP_SOCKET struct with updated values.
 * \param TCP_SOCKET to update. <I>Please, remember to use the "&" operator</I>
 * \return None
 */
void HTTPStatus(TCP_SOCKET* sock)
{
	TCPStatus(sock);
}

/**
 * Sends a HTTP Request to host
 * \param TCP_SOCKET to use. <I>Please, remember to use the "&" operator</I>
 * \param method to use
 <UL>
	<LI>HTTP_GET</LI>
	<LI>HTTP_POST</LI>
 </UL>
 * \param URL of the request
 * \param data to send with request
     * \param addictional parameters to add to request. It can be used HTTP_NO_PARAM to ignore this feature
 * \return None
 */
void HTTPRequest(TCP_SOCKET* sock, BYTE type, char* reqUrl, char* data2snd, char* param)
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
			mainOpStatus.Function = 27;
			mainOpStatus.ErrorCode = 0;
			
			// Set Params	
			xHTTPCode = 0;
			xSocket = sock;
			
			httpReqUrl = reqUrl;
			httpReqType = type;
			httpWriteBuffer = data2snd;
			
			if(param != HTTP_NO_PARAM)
			{
				httpParams = param;
			}
			else
			{
				httpParams[0] = '\0';
			}
			
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
int cHTTPRequest()
{
	char cmdReply[200];
	char msg2send[200];
	int resCheck = 0;
	DWORD tick;
	int countData;
	int chars2read;
	int msgLength = 0;
        
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

            // Calculate tcp write length...
            // Header length:
            if(httpReqType == HTTP_GET)
            {
                msgLength = 131;
            }
            else if(httpReqType == HTTP_POST)
            {
                msgLength = 132;
            }

            msgLength += strlen(httpReqUrl);

            msgLength += strlen(httpWriteBuffer);

            sprintf(msg2send, "%d", strlen(httpWriteBuffer));

            msgLength += strlen(msg2send);

            if(httpParams != HTTP_NO_PARAM)
            {
                msgLength += strlen(httpParams);
            }

            // ----------	TCP Write Command ----------
            sprintf(msg2send, "AT+KTCPSND=%d,%d\r",xSocket->number, msgLength);

            GSMWrite(msg2send);
            // Start timeout count
            tick = TickGetDiv64K(); // 1 tick every seconds
            maxtimeout = 75;// 60;
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
                if(httpReqType == HTTP_GET)
                {
                    GSMWrite("GET ");
                }
                else
                {
                    GSMWrite("POST ");
                }

                char* req = strstr(httpReqUrl, "/");
                GSMWrite(req);
                GSMWrite(" HTTP/1.1\r\nHost: ");
                req = strtok(httpReqUrl, "/");

                GSMWrite(req);
                GSMWrite("\r\nUser-Agent: Flyport-GPRS\r\n");
                if(httpParams != HTTP_NO_PARAM)
                {
                    GSMWrite(httpParams);
                }
                else
                {
                    GSMWrite("Content-Length: ");
                    char contentlength[5];
                    sprintf(contentlength, "%d", strlen(httpWriteBuffer));
                    GSMWrite(contentlength);
                    GSMWrite("\r\nContent-Type: application/x-www-form-urlencoded\r\nAccept: */*");
                }
                GSMWrite("\r\n\r\n");
                GSMWrite(httpWriteBuffer);


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

        case 5:
            if(GSMBufferSize() > 0)
            {
                // Parse Unsol Message
                mainGSMStateMachine = SM_GSM_CMD_PENDING;
                return -1;
            }
            // Read 1 char at time on Hilo TCP Buffer to get
            // HTTP Header reply, using GSMpSeek/GSMpRead, etc

            // ----------	TCP Status Update ----------
            sprintf(msg2send, "AT+KTCPSTAT=%d\r",xSocket->number);

            GSMWrite(msg2send);
            // Start timeout count
            tick = TickGetDiv64K(); // 1 tick every seconds
            maxtimeout = 75;// 60;
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

        case 8:
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
            smInternal++; // exit from the switch!
            break;
        /*
        case 9:
            // check if there are enough BYTEs on TCP Buffer for HTTP Response parsing
            if(xSocket->rxLen < 12)
            {
                // Break operation
                smInternal = 5;
                maxHTTPattempt--;
                break;
            }
            maxHTTPattempt = 10;
            // Send first AT command
            // ----------	TCP Read Command ----------
            httpReadBufferCount = 12;

            sprintf(msg2send, "AT+KTCPRCV=%d,%d\r",xSocket->number, httpReadBufferCount);

            GSMWrite(msg2send);
            // Start timeout count
            tick = TickGetDiv64K(); // 1 tick every seconds
            maxtimeout = 60;
            smInternal++;

        case 10:
            vTaskDelay(20);
            // Check ECHO
            countData = 0;

            resCheck = CheckEcho(countData, tick, cmdReply, msg2send, maxtimeout);

            CheckErr(resCheck, &smInternal, &tick);

            if(resCheck)
            {
                return mainOpStatus.ErrorCode;
            }

        case 11:
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

        case 12:
            // Get "HTTP/1.1 "reply:
            // Read data from TCP Socket
            httpReadBufferCount = GSMRead(tmp, 12);
            tmp[12] = '\0';
            // Decrease xSocket->rxLen:
            xSocket->rxLen -= httpReadBufferCount;

            char* httpReplyVal = strstr(tmp, "HTTP/1.1 ");
            if(httpReplyVal != NULL)
            {
                httpCount = atoi(&httpReplyVal[9]);
            }
            xHTTPCode = httpCount;
            httpCount = 0;
            smInternal++;

        case 13:
            // Now flush all the TCP Data until the first \r\n\r\n

            // ----------	TCP Status Update ----------
            sprintf(msg2send, "AT+KTCPSTAT=%d\r",xSocket->number);

            GSMWrite(msg2send);
            // Start timeout count
            tick = TickGetDiv64K(); // 1 tick every seconds
            maxtimeout = 60;
            smInternal++;

        case 14:
            vTaskDelay(2);
            // Check ECHO
            countData = 0;

            resCheck = CheckEcho(countData, tick, cmdReply, msg2send, maxtimeout);

            CheckErr(resCheck, &smInternal, &tick);

            if(resCheck)
            {
                return mainOpStatus.ErrorCode;
            }

        case 15:
            // Get reply "\r\n+KTCPSTAT: <status>,<tcp_notif>,<rem_data>,<rcv_data>\r\n"
            vTaskDelay(2);
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
                    gsmDebugPrint("Error in getfield for +KTCPSTAT socket\r\n");
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
                    gsmDebugPrint("Error in getfield for +KTCPSTAT socket\r\n");
                    break;
                }
                else
                {
                    xSocket->rxLen = atoi(temp);
                }
            }

        case 16:
            // Get reply (\r\nOK\r\n)
            //vTaskDelay(1);
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

        case 17:
            if(xSocket->rxLen == 0)
            {
                smInternal = 13;
                maxHTTPattempt--;
                if(maxHTTPattempt > 0)
                    vTaskDelay(20);
                else
                    // Break operation
                    break;
            }
            else
            {
                // ----------	TCP Read Command ----------
                sprintf(msg2send, "AT+KTCPRCV=%d,1\r",xSocket->number);

                GSMWrite(msg2send);
                // Start timeout count
                tick = TickGetDiv64K(); // 1 tick every seconds
                maxtimeout = 60;
                smInternal++;
            }

        case 18:
            //vTaskDelay(2);
            // Check ECHO
            countData = 0;

            resCheck = CheckEcho(countData, tick, cmdReply, msg2send, maxtimeout);

            CheckErr(resCheck, &smInternal, &tick);

            if(resCheck)
            {
                return mainOpStatus.ErrorCode;
            }

        case 19:
            // Get reply "\r\nCONNECT\r\n"
            //vTaskDelay(2);
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
                GSMRead(tmp, 1+22); //our char + "--EOF--Pattern--\r\nOK"
                //UARTWriteCh(1, tmp[0]);
                xSocket->rxLen--;
                if(tmp[0] == termString[httpCount])
                {
                    httpCount++;
                }
                if(httpCount > 4)
                {
                    maxHTTPattempt = -2; // we found the termString as desired
                    break;
                }
                else
                {
                    smInternal = 13;
                }
            }
         * */
            /*
        case 20:
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
            */

        default:
            // Check if Buffer is free
            if(GSMBufferSize() > 0)
            {
                // Parse Unsol Message
                mainGSMStateMachine = SM_GSM_CMD_PENDING;
                return -1;
            }
            else
                smInternal++;
            break;

    }// switch(smInternal)
    
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
 * Reads data form http socket
 * \param TCP_SOCKET to use. <I>Please, remember to use the "&" operator</I>
 * \param data buffer to fill
 * \param amount of BYTEs to read
 * \return None
 */
void HTTPReadData(TCP_SOCKET* sock, char* dataBuffer, int datalen)
{
	httpReadBuffer = dataBuffer;
	httpReadBufferCount = datalen;
	TCPRead(sock, httpReadBuffer, httpReadBufferCount);
}	

/*! @} */

/*! @} */
