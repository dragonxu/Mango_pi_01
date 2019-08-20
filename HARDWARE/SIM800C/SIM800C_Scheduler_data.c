/*
 * SIM800C_Scheduler_data.c
 *
 *  Created on: 2019��5��5��
 *      Author: Administrator
 */

#include "SIM800C_Scheduler_data.h"
#include <string.h>
#include "SIM800C.h"

#define CMP_AT 							(strstr(_GPRS->P_CMD, "AT\r\n"))
#define CMP_ATE0						(strstr(_GPRS->P_CMD, "ATE0\r\n"))
#define CMP_AT_CIPSRIP			(strstr(_GPRS->P_CMD, "AT+CIPSRIP=1\r\n"))
#define CMP_AT_CSQ					(strstr(_GPRS->P_CMD, "AT+CSQ\r\n"))
#define CMP_AT_CIPCLOSE			(strstr(_GPRS->P_CMD, "AT+CIPCLOSE\r\n"))	
#define CMP_AT_CIPSTART			(strstr(_GPRS->P_CMD, "AT+CIPSTART"))	
#define CMP_AT_CIPSTATUS		(strstr(_GPRS->P_CMD, "AT+CIPSTATUS\r\n"))
#define CMP_AT_CIPSHUT			(strstr(_GPRS->P_CMD, "AT+CIPSHUT\r\n"))
#define CMP_AT_CGDCONT			(strstr(_GPRS->P_CMD, "AT+CGDCONT="))

#define CMP_AT_CIPSEND			(strstr(_GPRS->P_CMD, "AT+CIPSEND="))	
#define CMP_AT_CSMINS				(strstr(_GPRS->P_CMD, "AT+CSMINS?"))

#define DAT_MQTT_DAT				(strstr(_GPRS->P_CMD, "MQTT_DAT"))
#define RECV_MQTT_DAT				(strstr(_GPRS->P_CMD, "RECV"))



extern char * GPRS_Rx_Dat;
//extern uint8_t G_Sim800C_Signal;
extern uint8_t Recv_cmd_data[];
extern SemaphoreHandle_t uart2_Idle_xSemaphore;
extern osThreadId Start_Scheduler_data_Task_TaskHandle;
extern osThreadId Start_Send_State_data_TaskHandle;	//�������ݵ���λ�����
extern osThreadId Start_Recv_Onenet_data_TaskHandle;	//�������ݵ���λ�����
extern GPRS_TypeDef *p_GPRS,GPRS;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern SemaphoreHandle_t recv_gprs_cmd_xSemaphore;//��ֵ�ź���
extern SemaphoreHandle_t send_gprs_dat_xSemaphore;//��ֵ�ź���

/*
 * ����sim800C�������ݵ�task
 */
void Start_Scheduler_data_Task(void const * argument)
{
	
	while(1)
	{
		if( xSemaphoreTake( uart2_Idle_xSemaphore, portMAX_DELAY ) == pdTRUE )
		{
			Process_USART2_Dat(p_GPRS);//ȷ��һ֡���ݽ��������Ҫ�ж�������ʲô		
		}
	}
}

uint8_t Auto_Return_Data(GPRS_TypeDef	* _GPRS)
{
	uint8_t clear = 0;
	if(strstr(GPRS_Rx_Dat,"+CFUN: 1"))
	{
		clear = 1;
		_GPRS->CFUN = 1;
	}
	if(strstr(GPRS_Rx_Dat,"+CPIN: READY"))
	{
		clear = 2;
		_GPRS->CPIN = 1;
	}
	if(strstr(GPRS_Rx_Dat,"Call Ready"))
	{
		clear = 3;
		_GPRS->CALL = 1;
	}
	if(strstr(GPRS_Rx_Dat,"SMS Ready"))
	{
		clear = 4;
		_GPRS->SMS = 1;
	}
	if(_GPRS->P_CMD == NULL)//��û�з�������
	{
		if(strstr(GPRS_Rx_Dat,"CLOSED"))//�Զ������˸�CLOSED
		{
			printf("close\r\n");
			clear = 5;
			_GPRS->CIPSTATUS = 8;	//���õ�ǰ״̬ΪCLOSED
		}
	}
	if(clear != 0)
	{
		Clear_Recv_Data();
	}
	return clear;
}

void Process_USART2_Dat(GPRS_TypeDef	* _GPRS)
{
	BaseType_t xHigherPriorityTaskWoken;


	HAL_UART_DMAStop(&huart2);		//�ر��ƶ���DMAͨ��x,�������
	GPRS_Rx_Dat = &_GPRS->GPRS_BUF->Buf[_GPRS->GPRS_BUF->Bufuse][0];
	_GPRS->GPRS_BUF->Len[_GPRS->GPRS_BUF->Bufuse] = PDATA_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart2_rx);//��ȡ���յ������ݵĳ���
	_GPRS->GPRS_BUF->Bufuse = !_GPRS->GPRS_BUF->Bufuse;					//�л�������BUFF
	HAL_UART_Receive_DMA(&huart2, _GPRS->GPRS_BUF->Buf[_GPRS->GPRS_BUF->Bufuse], PDATA_SIZE);//DMA���տ���

	if(Auto_Return_Data(_GPRS) != 0)	//�ж��Զ����ص����ݲ�����0 ��ʾΪ�Զ��������ݲ�������code����
	{
		return;
	}

	if(CMP_AT_CIPSEND)		//�ȴ�������������
	{
		//printf("[%s]",GPRS_Rx_Dat);
		if(strstr(GPRS_Rx_Dat,_GPRS->P_CMD_CHECK))	//>\r\n
		{
			_GPRS->ATCIPSEND = 1;
		//	xSemaphoreGiveFromISR(recv_gprs_cmd_xSemaphore,&xHigherPriorityTaskWoken);
		//	xSemaphoreGiveFromISR(send_gprs_dat_xSemaphore,&xHigherPriorityTaskWoken);
			return;
		}
	}
	if(CMP_AT_CSMINS)		//����
	{
		printf("[%s]",GPRS_Rx_Dat);
		if(strstr(GPRS_Rx_Dat,_GPRS->P_CMD_CHECK))	//+CSMINS:
		{
			_GPRS->ATCSMINS = 1;
		//	xSemaphoreGiveFromISR(recv_gprs_cmd_xSemaphore,&xHigherPriorityTaskWoken);
		//	xSemaphoreGiveFromISR(send_gprs_dat_xSemaphore,&xHigherPriorityTaskWoken);
			return;
		}
	}
	if(DAT_MQTT_DAT)
	{
		if(strstr(GPRS_Rx_Dat,_GPRS->P_CMD_CHECK))	//SEND OK\r\n
		{
			_GPRS->SEND_OK = 1;
			//xSemaphoreGiveFromISR(send_gprs_dat_xSemaphore,&xHigherPriorityTaskWoken);
			return;
		}
	}
	if (CMP_AT_CIPSTATUS)//��ѯTCP״̬
	{
		if(CHECK_NUM_0)		//��ʾ������������
			_GPRS->CIPSTATUS = 0;
		if(CHECK_NUM_1)		//��ʾ������������
			_GPRS->CIPSTATUS = 1;
		if(CHECK_NUM_2)		//��ʾ������������
			_GPRS->CIPSTATUS = 2;
		if(CHECK_NUM_3)		//��ʾ������������
			_GPRS->CIPSTATUS = 3;
		if(CHECK_NUM_4)		//��ʾ������������
			_GPRS->CIPSTATUS = 4;
		if(CHECK_NUM_5)		//��ʾ������������
			_GPRS->CIPSTATUS = 5;
		if(CHECK_NUM_6)		//��ʾ������������
			_GPRS->CIPSTATUS = 6;
		if(CHECK_NUM_7)		//��ʾ������������
			_GPRS->CIPSTATUS = 7;
		if(CHECK_NUM_8)		//��ʾ������������
			_GPRS->CIPSTATUS = 8;
		if(CHECK_NUM_9)		//��ʾ������������
			_GPRS->CIPSTATUS = 9;
		_GPRS->ATCIPSTATUS = 1;
		//xSemaphoreGiveFromISR(recv_gprs_cmd_xSemaphore,&xHigherPriorityTaskWoken);
		return;
	}
	if (CMP_AT_CIPSHUT||CMP_AT_CGDCONT)
	{
		if(strstr(GPRS_Rx_Dat,"OK\r\n"))
		{
			xSemaphoreGiveFromISR(recv_gprs_cmd_xSemaphore,&xHigherPriorityTaskWoken);
//			taskEXIT_CRITICAL();			//�˳��ٽ���
			return;
		}
	}
	

	if(CMP_AT_CIPSTART)	//�ȴ�CONNECT OK
	{
		if(strstr(GPRS_Rx_Dat,"CONNECT OK\r\n"))
		{
			_GPRS->CIPSTATUS = 8;
			//xSemaphoreGiveFromISR(recv_gprs_cmd_xSemaphore,&xHigherPriorityTaskWoken);
			return;
		}
	}
	
	
	if(RECV_MQTT_DAT)	//RECV
	{
		if(strstr(GPRS_Rx_Dat,_GPRS->P_CMD_CHECK))	//RECV FROM
		{
			xSemaphoreGiveFromISR(send_gprs_dat_xSemaphore,&xHigherPriorityTaskWoken);
			return;
		}
	}	
	if(CMP_AT)
	{
		if(strstr(GPRS_Rx_Dat,"OK\r\n"))
		{
			_GPRS->AT = 1;
			return;
		}
	}
	if(CMP_ATE0)
	{
		if(strstr(GPRS_Rx_Dat,"OK\r\n"))
		{
			_GPRS->ATE = 1;
			return;
		}
	}
	if(CMP_AT_CIPSRIP)
	{
		if(strstr(GPRS_Rx_Dat,"OK\r\n"))
		{
			_GPRS->CIPSRIP = 1;
			return;
		}
	}
	if (CMP_AT_CSQ)
	{
		if(strstr(GPRS_Rx_Dat,"OK\r\n"))
		{
			_GPRS->ATCSQ = 1;
			return;
		}
	}
	if(CMP_AT_CIPCLOSE)//CIPCLOSE
	{
		if(strstr(GPRS_Rx_Dat,"ERROR\r\n"))
		{
			_GPRS->CIPCLOSE = 1;
			return;
		}
	}
	
	//printf("GPRS_Rx_Dat = [%s]",GPRS_Rx_Dat);//��ӡ��û����������������
	//	if (strstr(GPRS_Rx_Dat,"SMS Ready\r\n"))
//		_GPRS->READY++;
	Clear_Recv_Data();
}

void Clear_Recv_Data(void)
{
	memcpy(p_GPRS->DOWN_BUF,p_GPRS->GPRS_BUF->Buf[p_GPRS->GPRS_BUF->RX_Dispose],p_GPRS->GPRS_BUF->Len[p_GPRS->GPRS_BUF->RX_Dispose]);
	(p_GPRS->DOWN_BUF[p_GPRS->GPRS_BUF->Len[p_GPRS->GPRS_BUF->RX_Dispose] + 1])  = '\0';//�����һ�����ݸ�ֵΪ0
	memset(p_GPRS->GPRS_BUF->Buf[p_GPRS->GPRS_BUF->RX_Dispose],'\0',strlen(p_GPRS->GPRS_BUF->Buf[p_GPRS->GPRS_BUF->RX_Dispose]));//�������
	//p_GPRS->GPRS_BUF->Buf[_GPRS->GPRS_BUF->RX_Dispose]����������ݴ������
	p_GPRS->GPRS_BUF->RX_Dispose = !p_GPRS->GPRS_BUF->RX_Dispose;//�л�������Ļ�����ָ��
}








