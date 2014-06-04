#include "taskFlyport.h"
#include "grovelib.h"
#include "analog_temp.h"
#include "MQTT.h"

TCP_SOCKET sockTcp;
char inBuff[1500];
int lenTemp = 0;

int connect(char * server, char * username, char * password)
{
	char buffer[250];
	int len=0;
	memset(buffer, 0, 250);
	
	UARTWrite(1, "Connecting to TCP Server...\r\n");
	TCPClientOpen(&sockTcp, server, "1883");
	while(LastExecStat() == OP_EXECUTION)
		vTaskDelay(1);
	vTaskDelay(20);
	if(LastExecStat() != OP_SUCCESS)
	{
		UARTWrite(1, "Errors on TCPClientOpen function!\r\n");	
		return 0;
	}	
	else
	{
		UARTWrite(1, "TCPClientOpen OK \r\n");
		UARTWrite(1, "Updating TCP_SOCKET Status...\r\n");
		TCPStatus(&sockTcp);
		while(LastExecStat() == OP_EXECUTION)
			vTaskDelay(1);
		vTaskDelay(20);
		if(LastExecStat() != OP_SUCCESS)
		{
			UARTWrite(1, "Errors on updating TCPStatus!\r\n");	
			return 0;
		}
		else
		{
			sprintf(inBuff, "TCP Socket Status: %d\r\n", sockTcp.status);
			UARTWrite(1, inBuff);
			
			// Send data
			memset(buffer, 0, 250);
			len = MQTT_ConnectLOGIN(buffer, "flyportmqtt", 120, 0, username, password);
			UARTWrite(1, inBuff);
			if(len>0)
			{
				TCPWrite(&sockTcp,buffer,len);
			
				while(LastExecStat() == OP_EXECUTION)
					vTaskDelay(1);
				vTaskDelay(20);
				if(LastExecStat() != OP_SUCCESS)
				{
					UARTWrite(1, "Errors connecting to AirVantage!\r\n");
					return 0;
				}	
				else
				{
					return 1;
				}	
			}
		}
	}
	return 0;
}

int publish(char * topic, char * message)
{
	char buffer[250];
	int len=0;
	memset(buffer, 0, 250);
	len = MQTT_Publish(buffer, message, topic, 50, MQTT_QOS_0);	
	/*UARTWrite(1, "Topic: ");
	UARTWrite(1, topic);
	UARTWrite(1, "\r\n");	
	UARTWrite(1, "Message: ");
	UARTWrite(1, message);
	UARTWrite(1, "\r\n");
	sprintf(inBuff, "Buffer length: %d\r\n", len);
	UARTWrite(1, inBuff);
	UARTWrite(1, "Buffer: ");
	UARTWrite(1, buffer);		
	UARTWrite(1, "\r\n");*/		
	if(len>0)
	{
		TCPWrite(&sockTcp, buffer, len);			
		
		while(LastExecStat() == OP_EXECUTION)
			vTaskDelay(1);
		vTaskDelay(20);
		if(LastExecStat() != OP_SUCCESS)
		{
			return 0;
		}	
		else
		{
			return 1;
		}	
	}
	return 0;
}

void FlyportTask()
{
	
	vTaskDelay(20);
    _dbgwrite("Flyport Task Started...\r\n");
	
	// GROVE board
	void *board = new(GroveNest);
	
	// GROVE devices	
	// Digital Input
	void *button = new(Dig_io, IN);
	attachToBoard(board, button, DIG1);	
	
	// Analog Input 1 - Temperature Sensor
	void *tempAn = new(An_Temp);
	attachToBoard(board, tempAn, AN1);
	
	// Analog Input 2 - Moisture sensor
	void *moisture_sensor = new (An_i);
	attachToBoard(board, moisture_sensor, AN2);
	
	// Analog Input 3 - Light sensor
	void *light_sensor = new (An_i);
	attachToBoard(board, light_sensor, AN3);
	
	sockTcp.number = INVALID_SOCKET;
			
	char* myIMEI;	
			
	// Set APN
	UARTWrite(1, "Setup APN params\r\n");
	APNConfig("orange.m2m.spec","","", DYNAMIC_IP, DYNAMIC_IP, DYNAMIC_IP);
	vTaskDelay(20);
	while(LastExecStat() == OP_EXECUTION)
		vTaskDelay(20);
		
	myIMEI = strtok(GSMGetIMEI(), " ");
	UARTWrite(1, "MY IMEI: ");
	UARTWrite(1, myIMEI);
	UARTWrite(1, "\r\n");
	int lenIMEI=strlen(myIMEI);
	char topic[lenIMEI+14];
	sprintf(topic, "%s/messages/json", myIMEI);

		
	while(1)
	{		
		if ((LastConnStatus() != REG_SUCCESS) && (LastConnStatus() != ROAMING))
		{
			IOInit(p21, out);
			
			// Wait for GSM Connection successfull
			_dbgwrite("Waiting network connection...");
			while((LastConnStatus() != REG_SUCCESS) && (LastConnStatus() != ROAMING))
			{
				vTaskDelay(20);
				IOPut(p21, toggle);
				do
				{
					UpdateConnStatus();
					while(LastExecStat() == OP_EXECUTION)
						vTaskDelay(1);
				}while(LastExecStat() != OP_SUCCESS);
			}
			vTaskDelay(20);
			IOPut(p21, on);
			_dbgwrite("Flyport registered on network!\r\n");
				
			if(LastExecStat() == OP_SUCCESS)
			{
				if(connect("edge.airvantage.net", myIMEI, "mypass"))
				{
					UARTWrite(1, "Connected to AirVantage\r\n");
				}	
				else
				{
					UARTWrite(1, "Unable to connect to AirVantage\r\n");
				}
			}
		}
		
		if ((LastConnStatus() == REG_SUCCESS) || (LastConnStatus() == ROAMING))
		{					
			char msg[300];
			double temp = (double)(get(tempAn));
			int moisture = get(moisture_sensor);
			float luminosity = get(light_sensor);
			sprintf(msg, "{\"greenhouse.temperature\":%3.2f,\"greenhouse.humidity\":%d,\"greenhouse.luminosity\":%3.2f}", temp, moisture, (double) luminosity);
			if(publish(topic, msg))
			{
				UARTWrite(1, "Data sent:");
				UARTWrite(1, msg);
				UARTWrite(1, "\r\n");
			}	
			else
			{
				UARTWrite(1, "Errors sending data!\r\n");	
			}	
			vTaskDelay(30000);
		}
	}
}
