/** \file Hilo.c
 *  \brief Hardware library to manage the analog and digital IOs and UART
 */

/**
\addtogroup Hardware
@{
*/

/* **************************************************************************																					
 *                                OpenPicus                 www.openpicus.com
 *                                                            italian concept
 * 
 *            openSource wireless Platform for sensors and Internet of Things	
 * **************************************************************************
 *  FileName:        Hilo.c
 *  Dependencies:    Microchip configs files
 *  Module:          FlyPort WI-FI
 *  Compiler:        Microchip C30 v3.12 or higher
 *
 *  Author               Rev.    Date              Comment
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  Gabriele Allegria    1.0     1/20/2011		   First release  (core team)
 *  Stefano Cappello 							   Added I2C + support in PWM
 *  Simone Marra         2.3    08/02/2013              Added GPRS/3G support
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

#include "HWmap.h"

#include "Hilo.h"
#include "p24FJ256GA106.h"
#if defined (FLYPORTGPRS)

extern int *UMODEs[];
extern int *USTAs[];
extern int *UBRGs[];
extern int *UIFSs[];
extern int *UIECs[];
extern int *UTXREGs[];
extern int *URXREGs[];
extern int UTXIPos[];
extern int URXIPos[];

static int bufind_w;
static int bufind_r;
static int last_op;
static char GSMBuffer[GSM_BUFFER_SIZE];
static char GSMbuffover;
static char UnsolBuffer[150];

// FrontEnd variables
/*
extern int xFrontEndStat;
extern int xFrontEndStatRet;
extern int xErr;
*/

extern const long int baudComp[];

//	RTOS variables
extern xTaskHandle hGSMTask;
extern xTaskHandle hFlyTask;
extern xQueueHandle xQueue;
extern xSemaphoreHandle xSemFrontEnd;
extern xSemaphoreHandle xSemHW;
extern portBASE_TYPE xStatus;

// GSM variables
GSMModule 		mainGSM;
CALLData		mainCall;
static int		errorCode;
int             signal_dBm_Gsm = 0;

SMSData			mainSMS;
static BYTE		IncomingSMS_MemType;
static int		IncomingSMS_Index;
OpStat			mainOpStatus;
extern int 	mainGSMStateMachine;
extern FTP_SOCKET* xFTPSocket;

int EventType=0;
int HiloComTest();
int CheckEcho(int countData, const DWORD tick, char* cmdReply, const char* msg2send, const BYTE maxtimeout);

/**
 * Returns last GSM network connection status
 * \return - Connection status
 * <UL>
 *	<LI><B> NO_REG </B>(0) - not registered</LI>
 *	<LI><B> REG_SUCCESS </B>(1) - registered correctly on network</LI>		
 *	<LI><B> SEARCHING </B>(2) - searching for operator</LI>
 *	<LI><B> REG_DENIED </B>(3) - registration on network denied</LI>
 *	<LI><B> UNKOWN </B>(4) - unknown status</LI>
 *	<LI><B> ROAMING </B>(5) - registered in roaming mode</LI>
 * </UL>
 */
BYTE LastConnStatus()
{
	return mainGSM.ConnStatus;
}

/**
 * Returns last GSM Task operation status
 * \return - Operation status.
 * <UL>
 *	<LI><B> OP_SUCCESS </B>(0) - Last function executed correctly</LI>
 *	<LI><B> OP_EXECUTION </B>(-1) - Function still executing</LI>		
 *	<LI><B> OP_LL </B>(-2) - Low Level mode is activate</LI>
 *	<LI><B> OP_TIMEOUR </B>(1) - Timeout error: GPRS module has not answered within the required timeout for the operation </LI>	
 *	<LI><B> OP_SYNTAX_ERR </B>(2) - GPRS module reported a "syntax error" message</LI>	
 *	<LI><B> OP_CMS_ERR </B>(3) - GPRS module reported a CMS error</LI>
 *	<LI><B> OP_CME_ERR </B>(4) - GPRS module reported a CME error</LI>
 *	<LI><B> OP_NO_CARR_ERR </B>(5) - GPRS module reported a NO CARRIER error</LI>
 *	<LI><B> OP_SMTP_ERR </B>(6) - Error in sending the email</LI>
 *	<LI><B> OP_FTP_ERR </B>(7) - Error message received in FTP operation</LI>
 *	<LI><B> OP_HIB_ERR </B>(8) - GPRS module is turned off and cannot reply to commands</LI>
 * </UL>
 */
int LastExecStat()
{
	return mainOpStatus.ExecStat;
}	

/**
 * Returns last GSM Task error code
 * \return - error code value.
 */
int LastErrorCode()
{
	return mainOpStatus.ErrorCode;
}

int LastSignalRssi()
{
    return signal_dBm_Gsm;
}

void callbackDbg(BYTE smInt)
{
	#if defined (STACK_USE_UART)
	char dbgcall[100];
	sprintf(dbgcall, "CALLBACK DEBUG:\r\n\tFunction: %d\r\n\tsmInternal: %d\r\n",
					mainOpStatus.Function, smInt);
	_dbgwrite(dbgcall);
	#endif
}

void gsmDebugPrint(char* dbgmessage)
{
	_dbgwrite(dbgmessage);
}

// Executes hardware reset to modem, with time delay
void HiloReset()
{
	// Reset the module:
    HILO_RESET_IO = 0;
    DelayMs(20);
    HILO_RESET_IO = 1;
    DelayMs(50);
}

// Applies low level pulse on POK to startup the module, and waits for CTS signal
BOOL HiloPok(int timeout)
{
	HILO_CTS_TRIS = 1;
	HILO_POK_IO = 1;
    DelayMs(2000);
    while((HILO_CTS_IO == 1)&&(timeout > 0))
    {
        HILO_POK_IO = 0;
        DelayMs(1);
        timeout--;
    }
    HILO_POK_IO = 0;
    if(timeout > 0)
        return TRUE;
    else
        return FALSE;
}

// Initializes Flyport UART4 to be used with Hilo Modem with sperified "long int baud" baudrate. It enables also HW flow signals CTS/RTS
void HiloUARTInit(long int baud)
{
    // Initialize HILO UART...
    int port = HILO_UART-1;
	long int brg , baudcalc , clk , err;
	clk = GetInstructionClock();
	brg = (clk/(baud*16ul))-1;
	baudcalc = (clk/16ul/(brg+1));
	err = (abs(baudcalc-baud)*100ul)/baud;

	int UMODEval = 0;
	UMODEval = (*UMODEs[port] & 0x3CFF);
	
	if (err<2)
	{
		*UMODEs[port] = (0xFFF7 & UMODEval);
		*UBRGs[port] = brg;
	}
	else
	{
		brg = (clk/(baud*4ul))-1;
		*UMODEs[port] = (0x8 | UMODEval);
		*UBRGs[port] = brg;
	}
	
	// UART ON:
	*UMODEs[port] = *UMODEs[port] | 0x8000;
	*USTAs[port] = *USTAs[port] | 0x400;

	*UIFSs[port] = *UIFSs[port] & (~URXIPos[port]);
	*UIFSs[port] = *UIFSs[port] & (~UTXIPos[port]);
	*UIECs[port] = *UIECs[port] | URXIPos[port];
}

// Initializes Hilo using Reset, UART setup, Pok and waits until the procedure is finished
void HiloInit(long int baud)
{
	int a = 1;
	
	while(a != 0)
	{
		if(HiloPok(2500))
        {
            vTaskDelay(50);
            HiloReset();
            vTaskDelay(50);
		
            a = HiloComTest();
        }
        else
        {
            _dbgwrite("\r\nHilo not reply to POK signal!\r\n");
            HiloReset();
            vTaskDelay(300);
            HiloReset();
            _dbgwrite("Testing POK again...\r\n");
        }
	}
}

int findStr(char* str, int vTaskTimeout)
{
	char buff;
	int cnt=0;
	int cnt2=0;
	int len = strlen(str);
	// Waiting for the first char...
	while (GSMBufferSize() == 0)
	{
		cnt++;
		vTaskDelay(1);
		if (cnt == vTaskTimeout)
			return -1;
	}
	
	//	Searching for OK answer
	while (GSMBufferSize() > 0)
	{
		GSMRead(&buff, 1);
			
		if (buff == str[cnt2])
			cnt2++;
		else
			cnt2 = 0;
		if (cnt2 == len)
		{
			return 0;
		}	
		while (GSMBufferSize() == 0)
		{
			vTaskDelay(1);
			cnt++;
			if (cnt == vTaskTimeout)
				break;
		}				
	}		
	return -1;
}

int HiloComTest()
{
	int cnt, i;
	char tofind[]="OK\r", buff;
	#if defined(STACK_USE_UART)
	char gab[55];
	#endif
	for (i=7; i>=0; i--)
	{
		cnt = 0;
		HILO_RTS_IO = 0;
		
		//	Waiting for CTS signal enabled, timeout 10 secs.
		while (HILO_CTS_IO == 1)
		{
			cnt++;
			vTaskDelay(50);
			if (cnt == 20)
			{
				return -1;
			}
		}
		
		//	Hilo UART initialization, without hardware flow control
		HiloUARTInit(baudComp[i]);
		#if defined(STACK_USE_UART)
		sprintf(gab, "Testing %ld baud...\n", baudComp[i]);
		_dbgwrite(gab);
		#endif
		while (GSMBufferSize() > 0)
		{
			GSMFlush();
			vTaskDelay(50);
		}
			
		//	Setting hardware flow control on Hilo module
		GSMWrite("AT&K3\r");
		
		//	Waiting for Hilo answer, timeout 5 secs.
		cnt = 0;
		while (GSMBufferSize() == 0)
		{
			cnt++;
			vTaskDelay(10);
			if (cnt == 50)
				break;		
		}
		
		//	If Hilo timed out, trying next baudrate
		if (cnt == 50)
		{
			continue;
		}
		cnt = 0;
		//	Searching for OK answer
		while (GSMBufferSize() > 0)
		{
			GSMRead(&buff, 1);
				
			if (buff == tofind[cnt])
				cnt++;
			else
				cnt = 0;
			if (cnt == 3)
			{
				if(i != 7)
				{
					GSMWrite("AT+IPR=115200\r");
					vTaskDelay(50);
					HiloUARTInit(115200);
					vTaskDelay(20);
					GSMFlush();
				}
				return 0;
			}	
			vTaskDelay(5);
			if (GSMBufferSize() == 0)
				vTaskDelay(100);				
		}
	}
	return -1;
}

// Executes tests to check if GSM modem is set at desired baudrate
int HiloTestBaud(long int baud)
{
	GSMFlush();
	GSMWrite("ATE0\r");
	DelayMs(20);
	GSMWrite("ATE0\r");
	DelayMs(200);
	GSMFlush();
	char baudRate[20];
	sprintf(baudRate, "AT+IPR=%ld\r", baud);
	GSMWrite(baudRate);
	DelayMs(400);
	int length = GSMBufferSize();
	if(length > 0)
	{
		DelayMs(20);
		if(length > 20)
			length = 20;
		GSMRead(baudRate, length);
		
		if(strstr(baudRate, "OK")!=NULL)
		{
			return 1;
		}
		else
		{
			if (strstr(baudRate, ">")!=NULL)
			{
				char quitSequence[10];
				char q = 0x1B;
				sprintf(quitSequence, "quit!%c", q);
				GSMWrite(quitSequence);
				GSMWrite(quitSequence);
				GSMWrite(quitSequence);
				DelayMs(20);
				GSMFlush();
				return 2;
			}
			return 0;
		}
	}
	else
	{
		return 0;
	}
}

// #define _HILO_NO_SIM_CONNECTED_

// #define _HILO_PRINT_FW_VER_

#define _HILO_INIT_TIMEOUT_ 430
extern const long int baudComp[];
// Configures GSM modem to enter in "StandardMode", setting up desired parameters
int HiloStdModeOn(long int baud)
{
	// Hw init:
	HiloInit(baud);
	vTaskDelay(20);
	GSMFlush();

	// Set parameters
	_dbgwrite("setup parameters...\r\n");
	
	// ATE1
	sprintf(UnsolBuffer, "ATE1\r");
	GSMWrite(UnsolBuffer);
	if(!findStr("OK\r", _HILO_INIT_TIMEOUT_))
	{
		_dbgwrite("ATE1 OK\r\n");
	}
	else
	{
		_dbgwrite("ATE1 ERROR\r\n");
		return OP_SYNTAX_ERR;
	}

	// AT+CREG=0
	sprintf(UnsolBuffer,"AT+CREG=0\r");
	GSMWrite(UnsolBuffer);
	if(!findStr("OK\r", _HILO_INIT_TIMEOUT_))
	{
		_dbgwrite("AT+CREG=0 OK\r\n");
	}
	else
	{
		_dbgwrite("AT+CREG=0 ERROR\r\n");
		return OP_SYNTAX_ERR;
	}
	
	// AT+CRC=0
	sprintf(UnsolBuffer,"AT+CRC=0\r");
	GSMWrite(UnsolBuffer);
	if(!findStr("OK\r", _HILO_INIT_TIMEOUT_))
	{
		_dbgwrite("AT+CRC=0 OK\r\n");
	}
	else
	{
		_dbgwrite("AT+CRC=0 ERROR\r\n");
		return OP_SYNTAX_ERR;
	}

#ifndef _HILO_NO_SIM_CONNECTED_
	// AT+CAOC=0
	sprintf(UnsolBuffer,"AT+CAOC=0\r");
	GSMWrite(UnsolBuffer);
	if(!findStr("OK\r", _HILO_INIT_TIMEOUT_))
	{
		_dbgwrite("AT+CAOC=0 OK\r\n");
	}
	else
	{
		_dbgwrite("AT+CAOC=0 ERROR\r\n");
		return OP_SYNTAX_ERR;
	}
	 		
	// AT+CCWA=0
	sprintf(UnsolBuffer,"AT+CCWA=0\r");
	GSMWrite(UnsolBuffer);
	if(!findStr("OK\r", _HILO_INIT_TIMEOUT_))
	{
		_dbgwrite("AT+CCWA=0 OK\r\n");
	}
	else
	{
		_dbgwrite("AT+CCWA=0 ERROR\r\n");
		return OP_SYNTAX_ERR;
	}
		
	// AT+CNMI=2,1,0,1
	sprintf(UnsolBuffer,"AT+CNMI=2,1,0,1\r");
	GSMWrite(UnsolBuffer);
	if(!findStr("OK\r", _HILO_INIT_TIMEOUT_))
	{
		_dbgwrite("AT+CNMI=2,1,0,1 OK\r\n");
	}
	else
	{
		_dbgwrite("AT+CNMI=2,1,0,1 ERROR\r\n");
		return OP_SYNTAX_ERR;
	}
	 		
	// AT+CSMP=17,167,0,0
	sprintf(UnsolBuffer,"AT+CSMP=17,167,0,0\r");
	GSMWrite(UnsolBuffer);
	if(!findStr("OK\r", _HILO_INIT_TIMEOUT_))
	{
		_dbgwrite("AT+CSMP=17,167,0,0 OK\r\n");
	}
	else
	{
		_dbgwrite("AT+CSMP=17,167,0,0 ERROR\r\n");
		return OP_SYNTAX_ERR;
	}
	 		
	// AT+CUSD=0		
	sprintf(UnsolBuffer,"AT+CUSD=0\r");
	GSMWrite(UnsolBuffer);
	if(!findStr("OK\r", _HILO_INIT_TIMEOUT_))
	{
		_dbgwrite("AT+CUSD=0 OK\r\n");
	}
	else
	{
		_dbgwrite("AT+CUSD=0 ERROR\r\n");
		return OP_SYNTAX_ERR;
	}
	
	// AT+CLIP=1 
	sprintf(UnsolBuffer,"AT+CLIP=1\r");
	GSMWrite(UnsolBuffer);
	if(!findStr("OK\r", _HILO_INIT_TIMEOUT_))
	{
		_dbgwrite("AT+CLIP=1 OK\r\n");
	}
	else
	{
		_dbgwrite("AT+CLIP=1 ERROR\r\n");
		return OP_SYNTAX_ERR;
	}
	
	// AT+KPATTERN=\"--EOF--Pattern--\"
	sprintf(UnsolBuffer,"AT+KPATTERN=\"--EOF--Pattern--\"\r");
	GSMWrite(UnsolBuffer);
	if(!findStr("OK\r", _HILO_INIT_TIMEOUT_))
	{
		_dbgwrite("AT+KPATTERN=\"--EOF--Pattern--\" OK\r\n");
	}
	else
	{
		_dbgwrite("AT+KPATTERN=\"--EOF--Pattern--\" ERROR\r\n");
		return OP_SYNTAX_ERR;
	}
	
	// AT+CSDH=1 
	sprintf(UnsolBuffer,"AT+CSDH=1\r");
	GSMWrite(UnsolBuffer);
	if(!findStr("OK\r", _HILO_INIT_TIMEOUT_))
	{
		_dbgwrite("AT+CSDH=1 OK\r\n");
	}
	else
	{
		_dbgwrite("AT+CSDH=1 ERROR\r\n");
		return OP_SYNTAX_ERR;
	}
	
	// AT+CMGF=1
	sprintf(UnsolBuffer,"AT+CMGF=1\r");
	GSMWrite(UnsolBuffer);
	if(!findStr("OK\r\n", _HILO_INIT_TIMEOUT_))
	{
		_dbgwrite("AT+CMGF=1 OK\r\n");
	}
	else
	{
		_dbgwrite("AT+CMGF=1 ERROR\r\n");
		return OP_SYNTAX_ERR;
	}
	
	// AT+CREG=1
	sprintf(UnsolBuffer,"AT+CREG=1\r");
	GSMWrite(UnsolBuffer);
	if(!findStr("OK\r", _HILO_INIT_TIMEOUT_))
	{
		_dbgwrite("AT+CREG=1 OK\r\n");
	}
	else
	{
		_dbgwrite("AT+CREG=1 ERROR\r\n");
		return OP_SYNTAX_ERR;
	}
	
	// AT+KSLEEP=0
	sprintf(UnsolBuffer,"AT+KSLEEP=0\r");
	GSMWrite(UnsolBuffer);
	if(!findStr("OK\r", _HILO_INIT_TIMEOUT_))
	{
		_dbgwrite("AT+KSLEEP=0 OK\r\n");
	}
	else
	{
		_dbgwrite("AT+KSLEEP=0 ERROR\r\n");
		return OP_SYNTAX_ERR;
	}

#endif

#ifdef _HILO_PRINT_FW_VER_
    // AT+CGMR (to get Revision identification)
	sprintf(UnsolBuffer, "AT+CGMR\r");
	GSMFlush();
	GSMWrite(UnsolBuffer);
	vTaskDelay(25);
	char revision[250];
    if(!findStr("SAGEMCOM ", _HILO_INIT_TIMEOUT_))
	{
		int lenRevStr = GSMRead((char*)revision, 240);

		if(lenRevStr > 0)
		{
			revision[lenRevStr] = '\0';
			_dbgwrite(revision);
		}
		else
		{
			_dbgwrite("ERROR getting revision!");
		}
    }
#endif

	// AT+KGSN=0 (to get IMEI)
	sprintf(UnsolBuffer, "AT+KGSN=1\r");
	GSMWrite(UnsolBuffer);
	if(!findStr("+KGSN: ", _HILO_INIT_TIMEOUT_))
	{
		GSMRead((char*)mainGSM.IMEI, 15);
		mainGSM.IMEI[16] = '\0';
	}
	if(findStr("OK\r\n", _HILO_INIT_TIMEOUT_))
	{
		sprintf((char*)mainGSM.IMEI, "ERROR!");
	}
	
	mainGSM.HWReady = TRUE;
	return 0;

}

// UART4 Rx Interrupt to store received chars from modem
void GSMRxInt()
{
	int port = HILO_UART - 1;
	
	while ((*USTAs[port] & 1)!=0)
	{
		if (bufind_w == bufind_r)
		{
			if (last_op == 1)
			{
				GSMbuffover = 1;
				bufind_w = 0;
				bufind_r = 0;
				last_op = 0;
			}
		}

		GSMBuffer[bufind_w] = *URXREGs[port];
        #if defined GSM_DEBUG_AT_UART_1
        UARTWriteCh(1, GSMBuffer[bufind_w]);
        #elif defined GSM_DEBUG_AT_UART_2
        UARTWriteCh(2, GSMBuffer[bufind_w]);
        #elif defined GSM_DEBUG_AT_UART_3
        UARTWriteCh(3, GSMBuffer[bufind_w]);
        #endif
		if (bufind_w == GSM_BUFFER_SIZE - 1)
		{
			bufind_w = 0;
		}
		else
			bufind_w++;
	}
	last_op = 1;
	*UIFSs[port] = *UIFSs[port] & (~URXIPos[port]);
}

// Writes to GSM Modem the chars contained on data2wr until a '\0' is reached
void GSMWrite(char* data2wr)
{
	int port = HILO_UART-1;
	int pdsel;
	int cnt = 0;	
	while(HILO_CTS_IO == 1)
	{
		vTaskDelay(10);
		cnt++;
		if(cnt == 50)
			return;
	}
	HILO_RTS_IO = 1;
	// transmits till NUL character is encountered 
	pdsel = (*UMODEs[port] & 6) >>1;
    if (pdsel == 3)                             // checks if TX is 8bits or 9bits
    {
        while(*data2wr != '\0') 
        {
            #if defined GSM_DEBUG_AT_UART_1
            UARTWriteCh(1, *data2wr);
            #elif defined GSM_DEBUG_AT_UART_2
            UARTWriteCh(2, *data2wr);
            #elif defined GSM_DEBUG_AT_UART_3
            UARTWriteCh(3, *data2wr);
            #endif

            while((*USTAs[port] & 512)>0);	// waits if the buffer is full 
            *UTXREGs[port] = *data2wr++;         // sends char to TX reg
        }
    }
    else
    {
        while(*data2wr != '\0')
        {
            #if defined GSM_DEBUG_AT_UART_1
            UARTWriteCh(1, *data2wr);
            #elif defined GSM_DEBUG_AT_UART_2
            UARTWriteCh(2, *data2wr);
            #elif defined GSM_DEBUG_AT_UART_3
            UARTWriteCh(3, *data2wr);
            #endif

            while((*USTAs[port] & 512)>0);      // sends char to TX reg
            *UTXREGs[port] = *data2wr++ & 0xFF;  // sends char to TX reg
        }
    }
    HILO_RTS_IO = 0;
}

void GSMWriteCh(char chr)
{
	int port = HILO_UART-1;
	int pdsel;
	pdsel = (*UMODEs[port] & 6) >>1;
	int cnt = 0;
	while(HILO_CTS_IO == 1)
	{
		vTaskDelay(10);
		cnt++;
		if(cnt == 50)
			return;
	}
	HILO_RTS_IO = 1;
    if(pdsel == 3)        /* checks if TX is 8bits or 9bits */
    {
        #if defined GSM_DEBUG_AT_UART_1
        UARTWriteCh(1, chr);
        #elif defined GSM_DEBUG_AT_UART_2
        UARTWriteCh(2, chr);
        #elif defined GSM_DEBUG_AT_UART_3
        UARTWriteCh(3, chr);
        #endif
        while((*USTAs[port] & 512)>0);	/* waits if the buffer is full */
        *UTXREGs[port] = chr;    		/* transfer data to TX reg */
    }
    else
    {
        #if defined GSM_DEBUG_AT_UART_1
        UARTWriteCh(1, chr);
        #elif defined GSM_DEBUG_AT_UART_2
        UARTWriteCh(2, chr);
        #elif defined GSM_DEBUG_AT_UART_3
        UARTWriteCh(3, chr);
        #endif
        while((*USTAs[port] & 512)>0); /* waits if the buffer is full */
        *UTXREGs[port] = chr & 0xFF;   /* transfer data to TX reg */
    }
    HILO_RTS_IO = 0;
}


void GSMFlush()
{
	bufind_w = 0;
	bufind_r = 0;
	last_op = 0;
	GSMBuffer[0] = '\0';
}


int GSMBufferSize()
{
	BYTE loc_last_op = last_op;
	int conf_buff;
	int bsize=0;

	conf_buff = bufind_r - bufind_w;
	if (conf_buff > 0)
		bsize = GSM_BUFFER_SIZE - bufind_r + bufind_w;
	else if (conf_buff < 0)
		bsize = bufind_w - bufind_r;
	else if (conf_buff == 0)
		if (loc_last_op == 1)
			bsize = GSM_BUFFER_SIZE;

	return bsize;
}

// Reads a max of count chars and puts them inside towrite char array, from GSMBuffer. This functions clears GSM buffer array
int GSMRead(char *towrite , int count)
{
	int rd,limit;
	limit = GSMBufferSize();
	if (count > limit)
		count=limit;
	int irx = 0;
	rd = 0;
	while (irx < count)
	{
        *(towrite+irx) = GSMBuffer[bufind_r];

		if (bufind_r == (GSM_BUFFER_SIZE-1))
			bufind_r = 0;
		else
			bufind_r++;		

		irx++;
	}
	
	if ( GSMbuffover != 0 )
	{
		rd = -count;
		GSMbuffover = 0;
	}
	else
		rd = count;

	last_op = 2;
	return rd;
}

// Reads a max of count chars inside towrite char array, from GSMBuffer. This functions DOES NOT clear GSM buffer array
int GSMpRead(char *towrite, int count)
{
	int rd,limit;
	int bufind_r_temp = bufind_r;
	
	limit = GSMBufferSize();
	if (count > limit)
		count=limit;
	int irx = 0;
	rd = 0;
	while (irx < count)
	{
        *(towrite+irx) = GSMBuffer[bufind_r_temp];

		if (bufind_r_temp == (GSM_BUFFER_SIZE-1))
			bufind_r_temp = 0;
		else
			bufind_r_temp++;		

		irx++;
	}
	
	if ( GSMbuffover != 0 )
	{
		rd = -count;
		GSMbuffover = 0;
	}
	else
		rd = count;
	return rd;
}

// Reads a max of num chars inside towrite char array, from GSMBuffer, starting from start relative position. 
// This functions DOES NOT clear GSM buffer array 
BOOL GSMpSeek(int start, int num, char* towrite)
{
	int limit = GSMBufferSize();
	// Check if we have a sufficient number of BYTEs on GSMBuffer
	if((start + num) > limit)
		return FALSE;
	else
	{
		limit = num;
		int bufind_r_temp = bufind_r + start;
		
		if(bufind_r_temp > GSM_BUFFER_SIZE)
			bufind_r_temp = bufind_r_temp - GSM_BUFFER_SIZE;
			
		int irx = 0;
		
		while(irx < limit)
		{
			*(towrite+irx) = GSMBuffer[bufind_r_temp];

			if (bufind_r_temp == (GSM_BUFFER_SIZE-1))
				bufind_r_temp = 0;
			else
				bufind_r_temp++;		
	
			irx++;
		}
		return TRUE;	
	}
}


int GSMReadN(int maxlen, char term, int occurr, char* destbuff)
{
	int count = 0;
	int i, occ_count = 0;
	for(i = 0; i < maxlen; i++)
	{
		GSMRead(destbuff+count, 1);
		if( *(destbuff+count) == term)
			occ_count++;
		count++;
		if(occ_count == occurr)
		{
			return count;
		}
	}
	// Error (less occ_count than expected)
	return -1;
}

// Unsolicited Messages Parsing
void GSMUnsol(int errorType)
{
	// Parse unsollecited message on GSMBuffer
	if (GSMBufferSize() > 3)
	{
		vTaskDelay(1);
		char unsolStr[5];
		GSMRead(unsolStr, 4);
		unsolStr[4] = '\0';
		
		while(GSMBufferSize() > 0)
		{
			// Scan for "RING" message
			if(strstr(unsolStr, "RING")!=NULL)
			{
				// Wait for message complete...
				int counterEx = 120;
				while((counterEx>0)&&(GSMBufferSize() < 10))
				{
					counterEx--;
					vTaskDelay(2);
				}	
				//RING
				int res = GSMReadN(150, '\n', 3, UnsolBuffer);
				
				if(res == -1)
				{
					//ERROR:
					_dbgwrite("ERROR GSMReadN RING:\r\n");
					
					// Execute Error Handler
					ErrorHandler(errorType);
				}
				else
				{
					UnsolBuffer[res] = '\0';
					
					// get phone number
					res = getfield('"', '"', 20, 1, UnsolBuffer, mainCall.CallerID, 500);
					
					if(res != 1)
					{
						_dbgwrite("ERROR getfield RING\r\n");
						
						// Execute Error Handler
						ErrorHandler(errorType);						
						break;
					}
					else
					{				
						// Set GSM Event:
						EventType = ON_RING;
						
						// Set mainCall status
						mainCall.Status = CALL_IN_PROG;
					}
				}
				
				break;
			}
			// Scan for "NO CARRIER" message
			else if(strstr(unsolStr, "NO C")!=NULL)
			{
				vTaskDelay(2);
				// NO CARRIER
				int res = GSMReadN(15, '\n', 1, UnsolBuffer);
				if(res == -1)
				{
					//ERROR:
					_dbgwrite("ERROR GSMReadN NO CARRIER\r\n");
										
					// Execute Error Handler
					ErrorHandler(errorType);
				}
				else
				{
					UnsolBuffer[res] = '\0';
					EventType = ON_NO_CARRIER;
					mainCall.Status = CALL_READY;
				}	
				break;
			}
			// Scan for "BUSY" message
			else if(strstr(unsolStr, "BUSY")!=NULL)
			{
				vTaskDelay(2);
				// BUSY
				int res = GSMReadN(15, '\n', 1, UnsolBuffer);
				if(res == -1)
				{
					// ERROR:
					_dbgwrite("ERROR GSMReadN BUSY\r\n");
										
					// Execute Error Handler
					ErrorHandler(errorType);
				}
				else
				{
					UnsolBuffer[res] = '\0';
					EventType = ON_BUSY;
					mainCall.Status = CALL_BUSY;
				}
				break;
			}
			// Scan for "+CMS ERROR" message
			else if(strstr(unsolStr, "+CMS")!=NULL)
			{
				vTaskDelay(2);
				//+CMS ERROR:<n>
				// Search the first LineFeed and put the string inside UnsolBuffer
				int res = GSMReadN(20, '\n', 1, UnsolBuffer);
				
				// Check result
				if(res == -1)
				{
					_dbgwrite("ERROR GSMReadN +CMS\r\n");
										
					// Execute Error Handler
					ErrorHandler(errorType);
				}
				else
				{
					UnsolBuffer[res] = '\0';
					char temp[10];
					// get error number
					res = getfield(':', '\r', 8, 1, UnsolBuffer, temp, 500);
					
					if(res != 1)
					{
						// Execute Error Handler
						ErrorHandler(errorType);
						
						break;
					}
					else
					{				
						// Set error code
						errorCode = atoi(temp);
						mainOpStatus.ErrorCode = errorCode;
						// Set GSM Event:
						EventType = ON_ERROR;	
					}
				}	
				break;
			}
			// Scan for "+CME ERROR" message
			else if(strstr(unsolStr, "+CME")!=NULL)
			{
				vTaskDelay(2);
				//+CME ERROR:<n>
				// Search the first LineFeed and put the string inside UnsolBuffer
				int res = GSMReadN(20, '\n', 1, UnsolBuffer);
				UnsolBuffer[res] = '\0';
				
				// Check result
				if(res == -1)
				{
					//ERROR:
					_dbgwrite("ERROR GSMReadN +CME\r\n");
					
					// Execute Error Handler
					ErrorHandler(errorType);
				}
				else
				{
					UnsolBuffer[res] = '\0';
					char temp[10];
					// get error number
					res = getfield(':', '\r', 8, 1, UnsolBuffer, temp, 500);
					
					if(res != 1)
					{
						// Execute Error Handler
						ErrorHandler(errorType);
						
						break;
					}
					else
					{				
						// Set error code
						errorCode = atoi(temp);
						mainOpStatus.ErrorCode = errorCode;
						// Set GSM Event:
						EventType = ON_ERROR;	
					}
				}	
				break;
			}
			// Scan for "+CMTI:..." message
			else if(strstr(unsolStr, "+CMT")!=NULL)
			{
				vTaskDelay(2);
				//+CMTI:<memory>,<index>
				// <memory> can be "SM" or "ME"
				// <index> is the int number of new SMS location in memory 
				
				// Search the first LineFeed and put the string inside UnsolBuffer
				int res = GSMReadN(20, '\n', 1, UnsolBuffer);
				
				// Check result
				if(res == -1)
				{
					//ERROR:
					_dbgwrite("ERROR GSMReadN +CMTI\r\n");
										
					// Execute Error Handler
					ErrorHandler(errorType);
				}
				else
				{
					UnsolBuffer[res] = '\0';
					char temp[10];
					// get Memory Type
					res = getfield('"', '"', 4, 1, UnsolBuffer, temp, 500);
					
					if(res != 1)
					{
						// Execute Error Handler
						ErrorHandler(errorType);
					
						break;
					}
					else
					{				
						// Set mainSMS.MemType
						if(strstr(temp, "SM")!=NULL)
							IncomingSMS_MemType = SM_MEM;
						else if(strstr(temp, "ME")!=NULL)
							IncomingSMS_MemType = ME_MEM;
							
						// Get also index if Memory type is correct (SM or ME)
						res = getfield(',','\r', 5, 1, UnsolBuffer, temp, 500);
						int indexSMS = atoi(temp);
						
						if(indexSMS > 0)
						{
							IncomingSMS_Index = indexSMS;
						}
						// Set GSM Event:
						EventType = ON_SMS_REC;	
					}
				}
				break;
			} 
			// Scan for ":..." message
			else if(strstr(unsolStr, "+CDS")!=NULL)
			{
				vTaskDelay(50);
				char temp[25];
				//+CDS: <fo>,<mr>,[<ra>],[<tora>],<scts>,<dt>, <st>(text mode enabled)
				
				// Search the first LineFeed and put the string inside UnsolBuffer
				int res = GSMReadN(145, '\n', 1, UnsolBuffer);
				
				// Check result
				if(res == -1)
				{
					//ERROR:
					_dbgwrite("ERROR GSMReadN +CDS\r\n");
					
					// Execute Error Handler
					ErrorHandler(errorType);
				}
				else
				{
					UnsolBuffer[res] = '\0';
					// get <mr>
					res = getfield(',', ',', 4, 1, UnsolBuffer, temp, 500);
					if(res != 1)
					{
						// Execute Error Handler
						ErrorHandler(errorType);
						break;
					}
					else
					{
						mainSMS.MessageReference = atoi(temp);
					}
					
					// get <ra> (destination)
					res = getfield('\"', '\"', 20, 1, UnsolBuffer, temp, 500);
					
					if(res != 1)
					{
						// Execute Error Handler
						ErrorHandler(errorType);
						break;
					}
					else
					{
						sprintf(mainSMS.Destination, temp);
					}
					
					// get <dt>
					res = getfield('\"', '\"', 22, 5, UnsolBuffer, temp, 500);
					if(res != 1)
					{
						// Execute Error Handler
						ErrorHandler(errorType);
						break;
					}
					else
					{
						sprintf(mainSMS.DateTime, temp);
						
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
						mainSMS.time.tm_mon = atoi(tmpval);
						
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
					
					// get <st>
					res = getfield(',', '\r', 4, 8, UnsolBuffer, temp, 500);
					if(res != 1)
					{
						// Execute Error Handler
						ErrorHandler(errorType);
						break;
					}
					else
					{
						mainSMS.ReportValue = atoi(temp);
					}
				}			
				
				// Set GSM Event:
				EventType = ON_SMS_SENT;
				
				break;
			}
			// Scan for "+CREG:..." message
			else if(strstr(unsolStr, "+CRE")!=NULL)
			{
				vTaskDelay(20);
				//+CREG
				// replies: +CREG:<stat>
				// <stat>
				// - 0: deregistration
				// - 1: registration
				// - 2: searching for new operator
				// - 3: registration failed
				// - 4: unknown
				// - 5: registered, roaming
				
				// Search the first LineFeed and put the string inside UnsolBuffer
				int res = GSMReadN(15, '\n', 1, UnsolBuffer);
				
				// Set GSM.ConnStatus = +CREG value
				if(res == -1)
				{
					//ERROR:
					_dbgwrite("ERROR GSMReadN +CREG\r\n");
					
					// Execute Error Handler
					ErrorHandler(errorType);
				}
				else
				{
					UnsolBuffer[res] = '\0';
					char temp[5];
					// get registration status number
					res = getfield(':', '\r', 4, 1, UnsolBuffer, temp, 500);
					
					if(res != 1)
					{
						// Execute Error Handler
						ErrorHandler(errorType);
					
						break;
					}
					else
					{				
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
					}
				}
				
				break;
			}
			// Scan for "+KTCP_NOTIF:..." message or +KTCP_DATA
			else if(strstr(unsolStr, "+KTC")!=NULL)
			{
				vTaskDelay(20);
				
				// Search the first LineFeed and put the string inside UnsolBuffer
				int res = GSMReadN(20, '\n', 1, UnsolBuffer);
				
				if(strstr(UnsolBuffer, "P_NOTIF:") != NULL)
				{			
					//+KTCP_NOTIF
					// response: +KTCP_NOTIF:<session_id>,<tcp_notif>
					// <tcp_notif>
					// 0- Network error 
					// 1- No more sockets available; max. number already reached 
					// 2- Memory problem  
					// 3- DNS error 
					// 4-TCP disconnection by the server or remote client
					// 5-TCP connection error 
					// 6- Generic error 
					// 7- Fail to accept client request’s 
					// 8- Data sending is OK but KTCPSND was waiting more or less characters 
					// 9- Bad session ID 
					// 10- Session is already running 
					// 11- All sessions are used 

					
					// Set xSocket.notif = tcp_notif
					if(res == -1)
					{
						//ERROR:
						_dbgwrite("ERROR GSMReadN +KTCP_NOTIF\r\n");
						
						// Execute Error Handler
						ErrorHandler(errorType);
					}
					else
					{
						UnsolBuffer[res] = '\0';
						char temp[5];
						// get phone number
						res = getfield(',', '\r', 4, 1, UnsolBuffer, temp, 500);
						
						if(res != 1)
						{
							// Execute Error Handler
							ErrorHandler(errorType);
						
							break;
						}
						else
						{				
							xSocket->notif = atoi(temp);
						}
					}
				}
				else if(strstr(UnsolBuffer, "P_DATA:") != NULL)// Ignore +KTCP_DATA
				{
					//+KTCP_DATA
					// response: +KTCP_DATA:<session_id>,<rxLen>
					// <rxLen>
					
					// Set xSocket.notif = tcp_notif
					if(res == -1)
					{
						//ERROR:
						_dbgwrite("ERROR GSMReadN +KTCP_DATA\r\n");

						// Execute Error Handler
						ErrorHandler(errorType);
					}
					else
					{
						UnsolBuffer[res] = '\0';
						char temp[7];
						// get phone number
						res = getfield(',', '\r', 6, 1, UnsolBuffer, temp, 500);
						
						if(res != 1)
						{
							// Execute Error Handler
							ErrorHandler(errorType);
						
							break;
						}
						else
						{				
							xSocket->rxLen = atoi(temp);
						}
					}
				}
				break;
			}			
			// Scan for "+KFTP_ERROR:<session_id>,<ftpError>..." message or 
			// +KFTP_SND_DONE:<session_id> or +KFTP_SND_ERROR:<session_id>,<ftp_error> or
			// +KFTP_RCV_DONE:<session_id> or +KFTP_RCV_ERROR:<session_id>,<ftp_error> 
			else if(strstr(unsolStr, "+KFT")!=NULL)
			{
				vTaskDelay(20);

				// Search the first LineFeed and put the string inside UnsolBuffer
				int res = GSMReadN(20, '\n', 1, UnsolBuffer);

				if(strstr(UnsolBuffer, "P_ERROR :") != NULL)
				{			
					// response: +KFTP_ERROR:<session_id>,<ftp_error>
					// <ftp_error>
					
					// xFTPSocket
					// Set xFTPSocket.ftpError = ftp_error
					if(res == -1)
					{
						//ERROR:
						_dbgwrite("ERROR GSMReadN +KFTP_ERROR\r\n");
				
						// Execute Error Handler
						ErrorHandler(errorType);
					}
					else
					{
						UnsolBuffer[res] = '\0';
						char temp[5];
						// get phone number
						res = getfield(',', '\r', 4, 1, UnsolBuffer, temp, 500);
						
						if(res != 1)
						{
							// Execute Error Handler
							ErrorHandler(errorType);
						
							break;
						}
						else
						{				
							xFTPSocket->ftpError = atoi(temp);
						}
					}
				}
			}	
			// Message not understood, shift one char over and tries again
			else
			{	
				if(errorType == NO_ERR)
				{
					int i;
					for(i = 1; i < 4; i++)
						unsolStr[i-1] = unsolStr[i];
					GSMRead(&unsolStr[3], 1);
				}
				else if(errorType == CMD_UNEXPECTED)
				{
					int i;
					for(i = 1; i < 4; i++)
						unsolStr[i-1] = unsolStr[i];
					GSMRead(&unsolStr[3], 1);
				}
			}
		}	
	}	
	// If the UnsolParsing generated a GSM Event, launch the event handler
	if (EventType != NO_EVENT)
	{
        if(mainGSMStateMachine == SM_GSM_LOW_POWER)
        {
            HILO_DTR_IO = TURN_HILO_DTR_ON;
            EventHandler();
            mainGSMStateMachine = SM_GSM_IDLE;
        }
        else
        {
            EventHandler();
        }
	}
}

void EventHandler()
{
    if((mainGSMStateMachine == SM_GSM_LOW_POWER) && (EventType != NO_EVENT))
    {
        OnLowPowerDisabled();
    }
    
	switch(EventType)
	{
		case ON_RING:
			OnRing(mainCall.CallerID);
			EventType = NO_EVENT;
			break;
			
		case ON_NO_CARRIER:
			OnNoCarrier(mainCall.CallerID);
			EventType = NO_EVENT;
			break;
			
		case ON_ERROR:
			OnError(mainOpStatus.ExecStat, mainOpStatus.ErrorCode);
			EventType = NO_EVENT;
			break;
				
		case ON_SMS_SENT:
			OnSMSSentReport(mainSMS.MessageReference, mainSMS.ReportValue);
			EventType = NO_EVENT;
			break;
		
		case ON_SMS_REC:
			OnSMSReceived(IncomingSMS_MemType, IncomingSMS_Index);
			EventType = NO_EVENT;
			break;
			
		case ON_REG:
			OnRegistration(mainGSM.ConnStatus);
			EventType = NO_EVENT;
			break;
			
		case ON_BUSY:
			OnBusy(mainCall.CallerID);
			EventType = NO_EVENT;
			break;
	}
}

// Executes actions related to the errorType received, and state machine status
void ErrorHandler(int errorType)
{
	// Check if state machine is IDLE
	if(mainGSMStateMachine == SM_GSM_IDLE)
	{
		#if defined(STACK_USE_UART)
		char temp[50];
		sprintf(temp, "State machine: IDLE, Error type: %d\r\n", errorType);
		_dbgwrite(temp);
		#endif
		
		// SET State Machine as FAULT						
		// Simone 
		mainOpStatus.ExecStat = OP_SYNTAX_ERR;
		mainGSMStateMachine = SM_GSM_HW_FAULT;
		_dbgwrite("generic error on message received!\r\n");
	}
	
	// Check if state machine is CMD_PENDING
	else if(mainGSMStateMachine == SM_GSM_CMD_PENDING)
	{
		#if defined(STACK_USE_UART)
		char temp[50];
		sprintf(temp, "State machine: CMD_PENDING, Error type: %d\r\n", errorType);
		_dbgwrite(temp);
		#endif
		
		// SET State Machine as FAULT						
		// Simone 
		mainOpStatus.ExecStat = OP_SYNTAX_ERR;
		mainGSMStateMachine = SM_GSM_HW_FAULT;
		_dbgwrite("generic error on message received!\r\n");
	}
	
	// Check if state machine is FAULT
	else if (mainGSMStateMachine == SM_GSM_HW_FAULT)
	{
		#if defined(STACK_USE_UART)
		char temp[50];
		sprintf(temp, "State machine: HW_FAULT, Error type: %d\r\n", errorType);
		_dbgwrite(temp);
		#endif
		// Execute HWFault_Test() until unlocked...
		HiloInit(115200);
		int a = 1;
		while(a != 0)
		{	
			/*HiloPok();
			vTaskDelay(50);
			HiloReset();
			vTaskDelay(50);
			
			a = HiloComTest();*/
            if(HiloPok(2500))
            {
                vTaskDelay(50);
                HiloReset();
                vTaskDelay(50);

                a = HiloComTest();
            }
            else
            {
                _dbgwrite("\r\nHilo not reply to POK signal!\r\n");
                HiloReset();
                vTaskDelay(300);
                HiloReset();
                _dbgwrite("Testing POK again...\r\n");
            }
		}
	}
}

/****** Parsing Helper Functions ******/

/**
* getfield - Reads the "parnum" field of "srcbuff" buffer, and puts it on "destbuff".
* \param start - char symbol to use as start parsing marker
* \param stop - char symbol to use as stop parsing marker
* \param maxlen - max length of field (usefull to prevent array overflows)
* \param parnum - number of the occurrence of same field to be accepted
* \param srcbuff - char[] source buffer of the string to parse
* \param destbuff - char[] destinationbuffer of the string to be filled with field content
* \param timeout - max time before to exit with error result (ms, max 1000 ms)
* \return the result of operation.
*/
int getfield(char start, char stop, int maxlen, int parnum, char* srcbuff, char* destbuff, int timeout)
{
	// TODO: handle Tick overflow
	DWORD startTick = TickGetDiv256();
	DWORD thisTick = TickGetDiv256();
    int len = 0;
    int fieldCount = 0;
    int startInd = 0;
    int stopInd = 0;
    int startFound = 0;
    int length = strlen(srcbuff);
    // Transform timeout in ms...
    timeout = timeout*5/20;

    while(((thisTick - startTick) <= (timeout))&&(len < length))
    {
        len++;
        thisTick = TickGetDiv256();
        // Search start char
        if(startFound == 0)
        {
            if(srcbuff[startInd] == start)
            {
                fieldCount++;
                if(fieldCount == parnum)
                    startFound++;
            }
            startInd++;
        }
        // Search Stop char
        else
        {
            
            if(stopInd < maxlen)
            {            
	            if(srcbuff[stopInd + startInd] == stop)
	            {
	                strncpy(destbuff, &srcbuff[startInd], stopInd);
	                destbuff[stopInd] = '\0';
	                return 1;
	            }
	            stopInd++;
	        }
	        else
	        	return 2;
        }
    }
    if((thisTick - startTick) > timeout)
    {
		return 3;
    }
    return 0;
}

/**
 * echoFind - searches the echo reply from GSMBuffer. It clears echo from GSMBuffer only if string is found.
 			- (200-4) chars max echo can be found
 * \param echoStr - const char* that contains echo string to search
 * \return result of operation.
 */
int echoFind(const char* echoStr)
{
	// 200-4 max echo to find... to be defined!
	char echoBuff[200];
	int lenStr = strlen(echoStr);
	
	GSMpRead(echoBuff, lenStr);
	echoBuff[lenStr] = '\0';
		
	if(strstr(echoBuff, echoStr)!=NULL)
	{
		// Flush GSMBuffer of the same size of string read...
		GSMRead(echoBuff, lenStr);
		echoBuff[lenStr] = '\0';
		return 0;
	}
	else if(strstr(echoBuff, "ERROR") != NULL)
	{
		// Reply present! Read until the lineNumber occurrence of '\n' char are found...
		int cc = GSMReadN(200, '\n', 2, echoBuff);
		echoBuff[cc] = '\0';
		
		// SET State Machine as IDLE						
		mainOpStatus.ExecStat = OP_SYNTAX_ERR;
		mainGSMStateMachine = SM_GSM_IDLE;
		return 1;
	}
	else if(strstr(echoBuff, "+CMS ") != NULL)
	{
		// Reply present! Read until the lineNumber occurrence of '\n' char are found...
		int cc = GSMReadN(200, '\n', 2, echoBuff);
		echoBuff[cc] = '\0';
		
		// SET State Machine as IDLE						
		mainOpStatus.ExecStat = OP_CMS_ERR;
		mainGSMStateMachine = SM_GSM_IDLE;
		
		//+CMS ERROR:<n>
		char temp[10];
		// get error number
		int res = getfield(':', '\r', 8, 1, echoBuff, temp, 500);
		
		if(res == 1)
		{
			// Set error code
			errorCode = atoi(temp);
			mainOpStatus.ErrorCode = errorCode;
			// Set GSM Event:
			EventType = ON_ERROR;	
		}	
		return 2;
	}
	else if(strstr(echoBuff, "+CME ") != NULL)
	{
		// Reply present! Read until the lineNumber occurrence of '\n' char are found...
		int cc = GSMReadN(200, '\n', 2, echoBuff);
		echoBuff[cc] = '\0';
		
		// SET State Machine as IDLE
		mainOpStatus.ExecStat = OP_CME_ERR;
		mainGSMStateMachine = SM_GSM_IDLE;
		
		//+CME ERROR:<n>
		char temp[10];
		// get error number
		int res = getfield(':', '\r', 8, 1, echoBuff, temp, 500);
		
		if(res == 1)
		{
			// Set error code
			errorCode = atoi(temp);
			mainOpStatus.ErrorCode = errorCode;
			// Set GSM Event:
			EventType = ON_ERROR;	
		}	
		return 3;	
	}
	else
	{
		return -1;
	}	
}

/**
 * getAnswer - searches the answer reply from GSMBuffer. It clears answer + lineNumber lines from GSMBuffer only if string is found.
 			 - The answer should be always in <CR><LF><response><CR><LF> format!
 * \param answer2src - const char* that contains answer string to search
 * \param lineNumber - how many lines to read if string is found
 * \param replyBuffer - char* buffer for destination
 * \return result of operation.
 */
int getAnswer(const char* answer2src, int lineNumber, char* replyBuffer)
{
	// Answer is in format:
	// <CR><LF><response><CR><LF>
	int lenStr = strlen(answer2src);
	GSMpRead(replyBuffer, lenStr+2);
	replyBuffer[lenStr+2] = '\0';

	if(strstr(replyBuffer, answer2src) != NULL)
	{
		// Reply present! Read until the lineNumber occurrence of '\n' char are found...
		int cc = GSMReadN(200, '\n', lineNumber, replyBuffer);
		replyBuffer[cc] = '\0';

		return 0;
	}
	else if(strstr(replyBuffer, "+CMS") != NULL)
	{
		// Reply present! Read until the lineNumber occurrence of '\n' char are found...
		int cc = GSMReadN(200, '\n', 2, replyBuffer);
		replyBuffer[cc] = '\0';
		
		// SET State Machine as IDLE						
		mainOpStatus.ExecStat = OP_CMS_ERR;
		mainGSMStateMachine = SM_GSM_IDLE;
		
		//+CMS ERROR:<n>
		char temp[10];
		// get error number
		int res = getfield(':', '\r', 8, 1, replyBuffer, temp, 500);
		
		if(res == 1)
		{
			// Set error code
			errorCode = atoi(temp);
			mainOpStatus.ErrorCode = errorCode;
			// Set GSM Event:
			EventType = ON_ERROR;	
		}	
		return 2;
	}
	else if(strstr(replyBuffer, "+CME") != NULL)
	{
		// Reply present! Read until the lineNumber occurrence of '\n' char are found...
		int cc = GSMReadN(200, '\n', 2, replyBuffer);
		replyBuffer[cc] = '\0';
		
		// SET State Machine as IDLE
		mainOpStatus.ExecStat = OP_CME_ERR;
		mainGSMStateMachine = SM_GSM_IDLE;
		
		//+CME ERROR:<n>
		char temp[10];
		// get error number
		int res = getfield(':', '\r', 8, 1, replyBuffer, temp, 500);
		
		if(res == 1)
		{
			// Set error code
			errorCode = atoi(temp);
			mainOpStatus.ErrorCode = errorCode;
			// Set GSM Event:
			EventType = ON_ERROR;	
		}	
		return 3;	
	}
	else if(strstr(replyBuffer, "NO C") != NULL)
	{
		// Reply present! Read until the lineNumber occurrence of '\n' char are found...
		int cc = GSMReadN(200, '\n', 2, replyBuffer);
		replyBuffer[cc] = '\0';
		
		// SET State Machine as IDLE						
		mainOpStatus.ExecStat = OP_NO_CARR_ERR;
		mainGSMStateMachine = SM_GSM_IDLE;
		return 4;
	}
	
	else if(strstr(replyBuffer, "+KSM")!=NULL)
	{
		// Reply present! Read until the lineNumber occurrence of '\n' char are found...
		int cc = GSMReadN(200, '\n', 2, replyBuffer);
		replyBuffer[cc] = '\0';
		
		// SET State Machine as IDLE
		mainOpStatus.ExecStat = OP_SMTP_ERR;
		mainGSMStateMachine = SM_GSM_IDLE;
		
		//+KSMTP ERROR:<n>
		char temp[10];
		// get error number
		int res = getfield(':', '\r', 8, 1, replyBuffer, temp, 500);
		
		if(res == 1)
		{
			// Set error code
			errorCode = atoi(temp);
			mainOpStatus.ErrorCode = errorCode;
			// Set GSM Event:
			EventType = ON_ERROR;	
		}	
		return 3;	
	}
	
	else if(strstr(replyBuffer, "+KFT")!=NULL)
	{
		// Reply present! Read until the lineNumber occurrence of '\n' char are found...
		int cc = GSMReadN(200, '\n', 2, replyBuffer);
		replyBuffer[cc] = '\0';
		
		// SET State Machine as IDLE
		mainOpStatus.ExecStat = OP_FTP_ERR;
		mainGSMStateMachine = SM_GSM_IDLE;
		
		//+KFTP ERROR:<session_id>,<n>
		char temp[10];
		// get error number
		int res = getfield(',', '\r', 8, 1, replyBuffer, temp, 500);
		
		if(res == 1)
		{
			// Set error code
			errorCode = atoi(temp);
			xFTPSocket->ftpError = errorCode;
			mainOpStatus.ErrorCode = errorCode;
			// Set GSM Event:
			EventType = ON_ERROR;	
		}	
		return 3;
	}
	
	else if(strstr(replyBuffer, "ERRO") != NULL)
	{
		// Reply present! Read until the lineNumber occurrence of '\n' char are found...
		int cc = GSMReadN(200, '\n', 2, replyBuffer);
		replyBuffer[cc] = '\0';
		
		// SET State Machine as IDLE						
		mainOpStatus.ExecStat = OP_SYNTAX_ERR;
		mainGSMStateMachine = SM_GSM_IDLE;
		return 1;
	}
	
	else
	{
		// NOT FOUND
		return -1;
	}
}


/**
 * CheckCmd - 	Check if GSM received echo message (written inside msg2send). 
 				This functions searches the '\r' char inside GSMBuffer using GSMpSeek, and after uses getAnswer
 			 -  If yes set TRUE dataFound, if timeout occurs set TRUE timeout
 * \param countData - the starting position related to circular GSMBuffer (it uses GSMpSeek)
 * \param chars2read - the amount of lines to be read using getAnswer
 * \param tick - the starting tick value when the command was written to UART
 * \param cmdReply - char* to be used as buffer
 * \param msg2send - char* with echo to search
 * \param maxtimeout - max timeout seconds to track
 * \return result of operation
 */
int CheckCmd(int countData, int chars2read, const DWORD tick, char* cmdReply, const char* msg2send, const BYTE maxtimeout)
{
	// Check timeout	
	while((TickGetDiv64K() - tick) < (maxtimeout + 2))
	{
		// Seek GSMBuffer to search '\r' character...
		if(GSMpSeek(countData, 1, cmdReply) == TRUE)
		{			
			if(cmdReply[0] == '\r')
			{
				return getAnswer(msg2send, chars2read, cmdReply);
			}	
			else
				countData++;
		}
		else
		{
			// do not increase buffer, but wait a while to increase buffer
			vTaskDelay(10);
		}
	}
	
	// Timeout
	return -2;
}

/**
 * CheckEcho - 	Check if GSM received echo message (written inside msg2send). 
 				This functions searches the '\r' char inside buffer using GSMpSeek, and after uses echoFind
 			 -  If yes set TRUE dataFound, if timeout occurs set TRUE timeout
 * \param countData - the starting position related to circular GSMBuffer (it uses GSMpSeek)
 * \param tick - the starting tick value when the command was written to UART
 * \param cmdReply - char* to be used as buffer
 * \param msg2send - char* with echo to search
 * \param maxtimeout - max timeout seconds to track
 * \return result of operation
 */
int CheckEcho(int countData, const DWORD tick, char* cmdReply, const char* msg2send, const BYTE maxtimeout)
{
	// Check timeout	
	while((TickGetDiv64K() - tick) < (maxtimeout + 2))
	{
		// Seek GSMBuffer to search '\r' character...
		if(GSMpSeek(countData, 1, cmdReply) == TRUE)
		{
			if(cmdReply[0] == '\r')
			{
				return echoFind(msg2send);
			}	
			else
				countData++;
		}
		else
		{
			// do not increase buffer, but wait a while to increase buffer
			vTaskDelay(10);
		}	
	}
	
	// Timeout
	return -2;
}

/**
 * CheckErr  - 	Check if GSM received error message and updates mainGSMStateMachine, mainOpStatus and smInternal
 * \param result - result of CheckEcho/CheckCmd
 * \param smInt - reference of smInternal
 * \return none
 */
void CheckErr(int result, BYTE* smInt, DWORD* tickUpdate)
{
	switch(result)
	{
		case 0:
			// Operation Success
			mainGSMStateMachine = SM_GSM_IDLE;
			(*smInt)++;
			mainOpStatus.ExecStat = OP_EXECUTION;
			mainOpStatus.ErrorCode = 0;
			break;
		
		case 1:
			// ERROR
			mainGSMStateMachine = SM_GSM_IDLE;
			*smInt = 0;
			mainOpStatus.ExecStat = OP_SYNTAX_ERR;
			mainOpStatus.ErrorCode = 0;
			break;
			
		case 2:
			// +CMS
			mainGSMStateMachine = SM_GSM_IDLE;
			*smInt = 0;
			mainOpStatus.ExecStat = OP_CMS_ERR;
			break;
			
		case 3:
			// +CME
			mainGSMStateMachine = SM_GSM_IDLE;
			*smInt = 0;
			mainOpStatus.ExecStat = OP_CME_ERR;
			break;
			
		case 4:
			// NO CARRIER
			mainGSMStateMachine = SM_GSM_IDLE;
			*smInt = 0;
			mainOpStatus.ExecStat = OP_NO_CARR_ERR;
			break;
					
		case -1:
			// Need GSMUnsol test...
			mainGSMStateMachine = SM_GSM_CMD_PENDING;
			mainOpStatus.ExecStat = OP_EXECUTION;
			mainOpStatus.ErrorCode = 0;
			*tickUpdate = TickGetDiv64K();
			break;
		
		case -2:
			// Timeout
			mainGSMStateMachine = SM_GSM_HW_FAULT;
			*smInt = 0;
			mainOpStatus.ExecStat = OP_TIMEOUT;
			mainOpStatus.ErrorCode = 0;
			break;
	}
}

#endif
