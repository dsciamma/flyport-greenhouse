/* **************************************************************************																					
 *                                OpenPicus                 www.openpicus.com
 *                                                            italian concept
 * 
 *            openSource wireless Platform for sensors and Internet of Things	
 * **************************************************************************
 *  FileName:        GSMData.h
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

#ifndef __GSMDATA_H
#define __GSMDATA_H

// ----- GSM DEFINES AND VARIABLES -----

// GSM Module struct
typedef struct 
{
	char	IMEI[16];
	BYTE	ConnStatus;
	BOOL 	PinUnlocked;
	BOOL	HWReady;
}GSMModule;

// ----- SMS DEFINES AND VARIABLES -----



// SMS params struct
typedef struct
{
	BYTE	MemType;
	int		Index;
	char	Text[161];
	char	Sender[30];
	char	Destination[30];
	char	DateTime[22];	
	struct tm	time;
	int		ReportValue;  // Value of status report <st>
	int		MessageReference; // Value of message reference <mr>
}SMSData;

// ----- CALL DEFINES AND VARIABLES -----

// CALL params struct
typedef struct
{
	char	CallerID[30];
	BYTE	Status;
}CALLData;

// Operation Status struct
typedef struct
{
	int		Function; // FP_CMD number... (or Cmd)
	int		ExecStat; // 0 -> Success, -1 In execution, >0 Error
	int		ErrorCode; // CMS or CME or GPRS error number
}OpStat;

#endif
