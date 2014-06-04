#include "GSM_Events.h"
	
/****************************************************************************
  FUNCTIONS TO HANDLE THE GSM EVENTS
****************************************************************************/

// INCOMING CALL
void OnRing(char* phoneNumber)
{
	#if defined(STACK_USE_UART)
	char buf[20];
	_dbgwrite("Event: On Ring\r\nPhone number: ");
	sprintf(buf, "%s", phoneNumber);
	_dbgwrite(buf);
	_dbgwrite("\r\n");
	#endif
}

// No Carrier
void OnNoCarrier(char* phoneNumber)
{
	#if defined(STACK_USE_UART)
	char buf[20];
	_dbgwrite("Event: On No Carrier\r\nPhone number: ");
	sprintf(buf, "%s", phoneNumber);
	_dbgwrite(buf);
	_dbgwrite("\r\n");
	#endif
}

// Busy
void OnBusy(char* phoneNumber)
{
	#if defined(STACK_USE_UART)
	char buf[20];
	_dbgwrite("Event: On Busy\r\nPhone number: ");
	sprintf(buf, "%s", phoneNumber);
	_dbgwrite(buf);
	_dbgwrite("\r\n");
	#endif
}

void OnSMSReceived(BYTE memtype, int index)
{
	#if defined(STACK_USE_UART)
	_dbgwrite("Event: On SMS Received\r\nmemory type: ");
	if(memtype == SM_MEM)
		_dbgwrite("SIM card");
	else
		_dbgwrite("Module Memory");
	_dbgwrite("\r\nmemory index: ");
	char smsRec[7];
	sprintf(smsRec, "%d\r\n", index);
	_dbgwrite(smsRec);
	#endif
}

void OnSMSSentReport(int msgreference, int msgreport)
{
	#if defined(STACK_USE_UART)
	char buf[30];
	_dbgwrite("Event: On SMS Sent Report\r\n");
	sprintf(buf, "message reference:%d\r\n", msgreference);
	_dbgwrite(buf);
	sprintf(buf, "report value:%d\r\n", msgreport);
	_dbgwrite(buf);
	#endif
}

void OnError(int error, int errorNumber)
{
	#if defined(STACK_USE_UART)
	char numErr[10];
	_dbgwrite("Event: On Error\r\nerror: ");
	sprintf(numErr, "%d\r\n", error);
	_dbgwrite(numErr);
	_dbgwrite("error number: ");
	sprintf(numErr, "%d\r\n", errorNumber);
	_dbgwrite(numErr);
	#endif
}

void OnRegistration(BYTE Status)
{
	#if defined(STACK_USE_UART)
	_dbgwrite("Event: On Registration\r\n");
	switch(Status)
	{
		case 0:
			_dbgwrite( "Not registered\r\n");
			break;
		case 1:
			_dbgwrite( "Registered on Network\r\n");
			break;
		case 2:
			_dbgwrite("Searching for new operator\r\n");
			break;
		case 3:
			_dbgwrite("Registration denied\r\n");
			break;
		case 4:
			_dbgwrite("Unkown registration status\r\n");
			break;
		case 5:
			_dbgwrite("Roaming\r\n");
			break;
	}		
	#endif
}

void OnLowPowerDisabled()
{
    #if defined(STACK_USE_UART)
    _dbgwrite("Event: GSM Modem Low Power Disabled!\r\n");
    #endif
}

