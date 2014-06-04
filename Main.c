/** @mainpage OpenPicus
*
*  
<B>OpenPicus</B> is open source Hardware and Software wireless project to enable Smart Sensors and Internet of Things.
<UL>
	<LI><B>Hardware platform:</B> it's modular, the Modules are PICUSes while the Carrier Boards are their NESTs</LI>
	<LI><B>Wireless:</B> Wi-Fi, Ethernet or GPRS/3G. You have full control of the Stack and power down modes.</LI>
	<LI><B>Software Framework:</B> your Apps can control the functions of the Protocol Stack, but you don't need to be an expert of it.</LI>
	<LI><B>Development tool:</B> free IDE is ready to let you start development immediately.</LI>
	<LI><B>Serial Bootloader:</B> Brutus loaded on modules, you don't need a programmer</LI>
</UL><BR><BR>
<B>Flyport</B> is the first OpenPicus device, embedding the TCP/IP and WiFi stack for wireless communication with 802.11 devices. 
The Flyport also includes a real time operating system (FreeRTOS - www.freertos.org) to manage easily the TCP/IP stack and the user application in two different tasks.
\image html images/flyport_top.jpg

\image latex images/flyport_top.jpg
* @authors Gabriele Allegria
* @authors Claudio Carnevali<BR>
*/


/****************************************************************************
  SECTION 	Includes
****************************************************************************/

#include "Hilo.h"
#include "taskFlyport.h"

/*****************************************************************************
 *								--- CONFIGURATION BITS ---					 *
 ****************************************************************************/
_CONFIG2(FNOSC_PRI & POSCMOD_HS & IOL1WAY_OFF )		// Primary HS OSC 
_CONFIG1(JTAGEN_OFF & FWDTEN_OFF & ICS_PGx2 )		// JTAG off, watchdog timer off
//_CONFIG3(WPDIS_WPDIS)

int Cmd = 0;
int mainGSMStateMachine = SM_GSM_IDLE;
extern OpStat	mainOpStatus;
extern GSMModule mainGSM;

extern int HiloStdModeOn(long int baud);
#ifdef GSM_TASK_STARTUP_DISABLED
extern int cGSMHibernate_GSM_DISABLED();
#endif

// FrontEnd variables
BYTE xIPAddress[100];
WORD xTCPPort = 22;
TCP_SOCKET* xSocket;
FTP_SOCKET* xFTPSocket;

//	RTOS variables
xTaskHandle hGSMTask;
xTaskHandle hFlyTask;
xQueueHandle xQueue;
xSemaphoreHandle xSemFrontEnd = NULL;
xSemaphoreHandle xSemHW = NULL;
portBASE_TYPE xStatus;

const long int baudComp[8] = {	1200,	2400,	4800,	9600,
								19200,	38400,	57600,	115200		
							 };	// Warning those values are the baud config compatible with both
								// HiloV2 and Hilo3G models... 

static int (*FP_GSM[35])();

void CmdCheck(int mainStat)
{
	if (mainOpStatus.Function != 0)
	{
		int fresult = 0;
		while (xSemaphoreTake(xSemFrontEnd,0) != pdTRUE);
		if (mainOpStatus.ExecStat == mainStat)
		{
			fresult = FP_GSM[mainOpStatus.Function]();
			xSemaphoreGive(xSemFrontEnd);
			taskYIELD();
		}
		else 
		{
			xSemaphoreGive(xSemFrontEnd);
			taskYIELD();		
		}
	}
}

/****************************************************************************
  MAIN APPLICATION ENTRY POINT
****************************************************************************/
int main(void)
{
    // Initialize application specific hardware
	HWInit(HWDEFAULT);
	UARTInit(1, UART_DBG_DEF_BAUD);
	UARTOn(1);

#if defined GSM_DEBUG_AT_UART_2
    #define GSM_DBG_pRX     p15// #warning, please define a pin for uart RX
    #define GSM_DBG_pTX     p17// #warning, please define a pin for uart TX
    IOInit(GSM_DBG_pRX, in);
    IOInit(GSM_DBG_pRX, UART2RX);
    IOInit(GSM_DBG_pTX, UART2TX);
    UARTInit(2, 230400);
    UARTOn(2);
#elif defined GSM_DEBUG_AT_UART_3
    #define GSM_DBG_pRX     #warning, please define a pin for uart rx
    #define GSM_DBG_pTX     #warning, please define a pin for uart rx
    IOInit(GSM_DBG_pRX, in);
    IOInit(GSM_DBG_pRX, UART3RX);
    IOInit(GSM_DBG_pTX, UART3TX);
    UARTInit(3, 230400);
    UARTOn(3);
#endif
    
	//	Queue creation - will be used for communication between the stack and other tasks
	xQueue = xQueueCreate(3, sizeof (int));

	xSemFrontEnd = xSemaphoreCreateMutex();
	
	// RTOS starting
	if (xSemFrontEnd != NULL) 
	{
		// Creates the task to handle all HiLo functions
		xTaskCreate(GSMTask, (signed char*) "GSM", STACK_SIZE_GSM,
		NULL, tskIDLE_PRIORITY + 1, &hGSMTask);
	
		// Start of the RTOS scheduler, this function should never return
		vTaskStartScheduler();
	}
	
	_dbgwrite("Unexpected end of program...\r\n");
	while(1);
	return -1;
}

/*****************************************************************************
 FUNCTION 	GSMTask
			Main function to handle the HiLo stack
 
 RETURNS  	None
 
 PARAMS		None
*****************************************************************************/
void GSMTask()
{
	//	Function pointers for the callback function of the HiLo stack 
	FP_GSM[1] = cSMSSend;
	FP_GSM[2] = cSMSRead;
	FP_GSM[3] = cSMSDelete;
    FP_GSM[4] = cUpdateConnStatus;
	FP_GSM[5] = cSMTPParamsClear;
	FP_GSM[6] = cSMTPParamsSet;
	FP_GSM[7] = cSMTPEmailTo;
	FP_GSM[8] = cSMTPEmailSend;
    FP_GSM[9] = cGSMLowPowerEnable;
	FP_GSM[10] = cCALLHangUp;
	FP_GSM[11] = cCALLVoiceStart;
	FP_GSM[12] = cFTPConfig;
	FP_GSM[13] = cFTPReceive;
	FP_GSM[14] = cFTPSend;
	FP_GSM[15] = cFTPDelete;
	FP_GSM[16] = cGSMSignalQualityUpdate;
	FP_GSM[17] = cLLWrite;
	FP_GSM[18] = cLLModeEnable;		// it will be executed only if Standard mode is enabled
	FP_GSM[19] = cSTDModeEnable;	// it will be executed only if LowLevel mode is enabled
	FP_GSM[20] = cTCPClientOpen;
	FP_GSM[21] = cTCPClientClose;
	FP_GSM[22] = cTCPStatus;
	FP_GSM[23] = cTCPWrite;
	FP_GSM[24] = cTCPRead;
	FP_GSM[25] = cTCPRxFlush;
	FP_GSM[26] = cAPNConfig;
	FP_GSM[27] = cHTTPRequest;
	FP_GSM[28] = cGSMHibernate;
	FP_GSM[29] = cGSMOn;
	FP_GSM[30] = cFSWrite;
	FP_GSM[31] = cFSRead;
	FP_GSM[32] = cFSDelete;
	FP_GSM[33] = cFSSize;
	FP_GSM[34] = cFSAppend;
	
	
	// Initialization of tick only at the startup of the device
	if (hFlyTask == NULL)
	{
	    TickInit();
	}

#ifdef GSM_TASK_STARTUP_DISABLED
    if (hFlyTask == NULL)
    {
        _dbgwrite("Flyport GPRS starting...\r\n");

        // Init Hilo UART:
        HiloInit(baudComp[7]);
        vTaskDelay(20);
        GSMFlush();

        // Stop GSM task for this time only:
        mainGSM.HWReady = TRUE;
        GSMFlush();
        cGSMHibernate_GSM_DISABLED();

        
        //	Creates the task dedicated to user code
		xTaskCreate(FlyportTask,(signed char*) "FLY" , (configMINIMAL_STACK_SIZE *4 ),
		NULL, tskIDLE_PRIORITY + 1, &hFlyTask);

    }
    	
#else // GSM_TASK_STARTUP_DISABLED is not defined, we are in standard startup configuartion

	if (hFlyTask == NULL)
	{
		_dbgwrite("Flyport GPRS starting...\r\n");
		_dbgwrite("setting up HiLo module...\r\n");
				
		// Enter Standard Mode:
		while(HiloStdModeOn(baudComp[7])); // 115200 baud...
	}
	
	if (hFlyTask == NULL)
	{
		//	Creates the task dedicated to user code
		xTaskCreate(FlyportTask,(signed char*) "FLY" , (configMINIMAL_STACK_SIZE *4 ), 
		NULL, tskIDLE_PRIORITY + 1, &hFlyTask);	
	}
#endif
    
//-------------------------------------------------------------------------------------------
//|							--- COOPERATIVE MULTITASKING LOOP ---							|
//-------------------------------------------------------------------------------------------
    while(1)
    {
        switch(mainGSMStateMachine)
	    {
	    	case SM_GSM_IDLE:
	    		GSMUnsol(NO_ERR);
	    		// Check on the queue to verify if other task have requested some stack function
				xStatus = xQueueReceive(xQueue,&Cmd,0);
				CmdCheck(OP_EXECUTION);
				break;
			
			case SM_GSM_CMD_PENDING:
				GSMUnsol(CMD_UNEXPECTED);
	    		// Check on the queue to verify if other task have requested some stack function
				CmdCheck(OP_EXECUTION);
				break;
			
			case SM_GSM_LL_MODE:
				CmdCheck(OP_LL);
				break;
				
			case SM_GSM_HW_FAULT:
				mainGSM.HWReady = FALSE;
                vTaskDelay(20);
				HiloReset();
				vTaskDelay(20);
				// Enter Standard Mode:
				while(HiloStdModeOn(baudComp[7])); // 115200 baud...
				
				mainGSMStateMachine = SM_GSM_IDLE;
				mainOpStatus.Function = 0;
				mainOpStatus.ExecStat = OP_SUCCESS;
				mainOpStatus.ErrorCode = 0;
				mainGSM.HWReady = TRUE;
				break;
				
			case SM_GSM_HIBERNATE:
				// GSMUnsol(NO_ERR);
				// Accept only function 29 (cGSMOn)
				if(mainOpStatus.Function == 29)
					CmdCheck(OP_EXECUTION);
				else if(mainOpStatus.Function != 0)
				{
					mainOpStatus.ExecStat = OP_HIB_ERR;
					mainOpStatus.Function = 0;
					mainOpStatus.ErrorCode = -1;
				}
				break;

            case SM_GSM_LOW_POWER:
                GSMUnsol(NO_ERR);
                // Accept only function 29 (cGSMOn)
				if(mainOpStatus.Function == 29)
					CmdCheck(OP_EXECUTION);
				else if(mainOpStatus.Function != 0)
				{
					mainOpStatus.ExecStat = OP_LOW_POW_ERR;
					mainOpStatus.Function = 0;
					mainOpStatus.ErrorCode = -1;
				}
				break;
	    }
	}
}

