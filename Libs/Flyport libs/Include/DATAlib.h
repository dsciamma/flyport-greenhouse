/* **************************************************************************																					
 *                                OpenPicus                 www.openpicus.com
 *                                                            italian concept
 * 
 *            openSource wireless Platform for sensors and Internet of Things	
 * **************************************************************************
 *  FileName:        DATAlib.h
 *  Dependencies:    Microchip configs files
 *  Module:          FlyPort GPRS/3G
 *  Compiler:        Microchip C30 v3.12 or higher
 *
 *  Author               Rev.    Date              Comment
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  Gabriele Allegria    1.0     02/08/2012		   First release  (core team)
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
 
 #include "HWlib.h"
 #include "Helpers.h"

#ifndef __DATALIB_H
#define __DATALIB_H

#define DYNAMIC_IP	"0.0.0.0"

// TCP_NOTIF CODES:
#define TCP_NOTIF_NET_ERR			0
#define TCP_NOTIF_NO_SOCK_AVAILABLE	1
#define TCP_NOTIF_MEMORY_PROBLEMS	2
#define TCP_NOTIF_DNS_ERROR			3
#define TCP_NOTIF_TCP_DISCONNECTION	4
#define TCP_NOTIF_TCP_CONN_ERROR	5
#define TCP_NOTIF_GENERIC_ERROR		6
#define TCP_NOTIF_FAIL_REQUEST		7
#define TCP_NOTIF_WAITING_CHARS		8
#define TCP_NOTIF_BAD_SESSION_ID	9
#define TCP_NOTIF_SESSION_RUNNING	10
#define TCP_NOTIF_ALL_SESSION_USED	11

// TCP_STAT STATUS CODES:
#define TCP_STAT_INVALID_SOCK		0
#define TCP_STAT_FREE_SOCK			1
#define TCP_STAT_OPENING_SOCK		2
#define TCP_STAT_CONNECTED_SOCK		3
#define TCP_STAT_CLOSING_SOCK		4
#define TCP_STAT_CLOSED_SOCK		5


// FrontEnd variables
extern BYTE xIPAddress[];
extern WORD xTCPPort;
//extern TCP_SOCKET* xSocket;
//extern int xFrontEndStat;
//extern int xFrontEndStatRet;
//extern int xErr;

//	RTOS variables
extern xTaskHandle hGSMTask;
extern xTaskHandle hFlyTask;
extern xQueueHandle xQueue;
extern xSemaphoreHandle xSemFrontEnd;
extern xSemaphoreHandle xSemHW;
extern portBASE_TYPE xStatus;

/*****************************************************************************
	DATA function declarations	
*****************************************************************************/

void APNConfig(char* apn, char* login, char* passw, char* ip, char* dns1, char* dns2);
int  cAPNConfig();

#endif
