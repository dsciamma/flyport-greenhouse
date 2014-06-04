/* **************************************************************************																					
 *                                OpenPicus                 www.openpicus.com
 *                                                            italian concept
 * 
 *            openSource wireless Platform for sensors and Internet of Things	
 * **************************************************************************
 *  FileName:        Hilo.h
 *  Dependencies:    Microchip configs files
 *  Module:          FlyPort GPRS
 *  Compiler:        Microchip C30 v3.12 or higher
 *
  *  Author               Rev.    Date              Comment
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  Gabriele Allegria    1.0     11/26/2012		   First release  (core team)
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

#include "GenericTypeDefs.h"
#include <string.h>
#include <stdlib.h>
#include "HWlib.h"
#include "Tick.h"
#include "RTCClib.h"
#include "GSMData.h"
#include "HILOlib.h"
#include "GSM_Events.h"
#include "SMSlib.h"
#include "CALLlib.h"
#include "LowLevelLib.h"
#include "DATAlib.h"
#include "TCPlib.h"
#include "HTTPlib.h"
#include "SMTPlib.h"
#include "FTPlib.h"
#include "FSlib.h"

#ifndef __HILO_H
#define __HILO_H

#if defined (FLYPORTGPRS)
#define GSM_BUFFER_SIZE   1512
#endif

// Size of the stack for GSM
#define STACK_SIZE_GSM	(configMINIMAL_STACK_SIZE * 5)	

// State Machine Defines
#define		SM_GSM_IDLE				0
#define		SM_GSM_CMD_PENDING		1
#define 	SM_GSM_HW_FAULT			2
#define		SM_GSM_LL_MODE			3
#define		SM_GSM_HIBERNATE		4
#define     SM_GSM_LOW_POWER        5

// GSM task function prototypes
void GSMTask();
void GSMUnsol(int errorType);
void EventHandler();
void ErrorHandler(int errorType);

// GSM UART/modem related functions
void GSMRxInt();
void HiloReset();
BOOL HiloPok(int timeout);
void HiloUARTInit(long int baud);
void HiloInit(long int baud);
int  HiloTestBaud(long int baud);
void GSMFlush();
int	 GSMBufferSize();
int  GSMRead(char*, int);
int  GSMpRead(char*, int);
int  GSMReadN(int maxlen, char term, int occurr, char* destbuff);
BOOL GSMpSeek(int start, int num, char* destBuff);
void GSMWrite(char* data2wr);
void GSMWriteCh(char chr);

// Parsing helper functions
int echoFind(const char* echoStr);
int getAnswer(const char* answer2src, int lineNumber, char* replyBuffer);
int getfield(char start, char stop, int maxlen, int parnum, char* srcbuff, char* destbuff, int timeout);

#endif
