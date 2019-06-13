/*
 * SIM800C.c
 *
 *  Created on: 2018��7��25��
 *      Author: YuanWP
 */
#include "SIM800C.h"
#include "main.h"
#include "cmsis_os.h"
#include "uart.h"
#include "stm32f1xx_hal.h"
#include "GPRS.h"
#include "Mqtt.h"
#include "board_info.h"
#include "SIM800C_Scheduler_data.h"
#include "LCD12864.h"
//#include "stm32f1xx_hal_dma.h"

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern uint8_t a2RxBuffer;			//�����жϻ���
extern uint8_t Uart2_RxBuff[];
extern DMA_HandleTypeDef hdma_usart2_rx;
extern uint8_t *G_p_index_start,*G_p_index_end;

SemaphoreHandle_t Sim800c_Semaphore;//����һ�������ź���

osThreadId Start_Send_State_data_TaskHandle = NULL;	//�������ݵ���λ�����
osThreadId Start_Recv_Onenet_data_TaskHandle;	//�������ݵ���λ�����
osThreadId Start_Scheduler_data_Task_TaskHandle;
osThreadId Start_Reset_Sim800c_Task_TaskHandle;

osThreadId Sim800c_Rcv_TaskHandle;
extern uint8_t Uart2_Rx_Cnt;
extern osThreadId Start_SIM800C_TaskHandle;	//800c���
uint8_t G_Sim800C_Signal;


void reset_Sim800C(void)
{
	//vTaskSuspend( Start_Scheduler_data_Task_TaskHandle );		//����lcd12864��ʾ���˵��Ľ���
	SIM800C_PWKEY_CTL_L;
	osDelay(1000);
	SIM800C_PWKEY_CTL_H;
	osDelay(1500);
	SIM800C_PWKEY_CTL_L;
	osDelay(4000);	
	printf("restart 800c\r\n");
	Reset_Uart_DMA();
}

void init_Sim800C(void)
{
	uint8_t start_state = 0;
	reset_Sim800C();
	Reset_Uart_DMA();
	while(GPRS_AT(AT,"OK\r") == ERROR)
	{
		osDelay(300);
		start_state++;
		if(start_state == 3)
		{
			start_state = 0;
			reset_Sim800C();
			Reset_Uart_DMA();
		}
	}
	Reset_Uart_DMA();
	osDelay(300);	
	GPRS_ATE0(ATE0);
	osDelay(5000);
	Reset_Uart_DMA();
	vTaskResume( Start_Scheduler_data_Task_TaskHandle );//�ָ�����
	do{
		Reset_Uart_DMA();
		Get_Sim800C_Signal();
		printf("Get_Sim800C_Signal\r\n");
		osDelay(1000);
	}while(G_Sim800C_Signal<10);
	vTaskSuspend( Start_Scheduler_data_Task_TaskHandle );//����
	Reset_Uart_DMA();
	GPRS_AT_CGDCONT(AT_CGDCONT,"OK\r");
	printf("GPRS_AT_CGDCONT = ok\r\n");
	Reset_Uart_DMA();
	osDelay(300);
	GPRS_AT_CGATT(AT_CGATT,"OK\r");
	Reset_Uart_DMA();
	osDelay(300);
	GPRS_AT_CIPCSGP(AT_CIPCSGP,"OK\r");
	Reset_Uart_DMA();
	osDelay(300);
	GPRS_AT_CIPSTATUS(AT_CIPSTATUS,"OK\r");//��ѯ״̬INITIAL
	osDelay(300);
	Reset_Uart_DMA();
}



/*
��λSim800C
*/
void Start_Reset_Sim800c_Task(void const * argument)
{
	osDelay(100);
	vTaskSuspend( Start_Scheduler_data_Task_TaskHandle );		//����lcd12864��ʾ���˵��Ľ���
	vTaskSuspend( Start_SIM800C_TaskHandle );		//����lcd12864��ʾ���˵��Ľ���
	
	if(Start_Send_State_data_TaskHandle != NULL){
		osThreadTerminate(Start_Send_State_data_TaskHandle);//ɾ����������
		Start_Send_State_data_TaskHandle = NULL;
	}
	printf("init_sim800C\r\n");
	init_Sim800C();
	while(GPRS_AT_CIPSTART(AT_CIPSTART,"CONNECT OK") == ERROR)
	{
		reset_Sim800C();
		init_Sim800C();
	}
	Reset_Uart_DMA();
	
	vTaskResume( Start_Scheduler_data_Task_TaskHandle );	//�ָ�����
	vTaskResume( Start_SIM800C_TaskHandle );							//�ָ�����
	
	taskENTER_CRITICAL();				//�����ٽ���
	
	osThreadDef(Send_State_data_Task, Start_Send_State_data_Task, osPriorityNormal, 0, 128);	//����������ʪ��+����ת״̬��
	Start_Send_State_data_TaskHandle = osThreadCreate(osThread(Send_State_data_Task), NULL);	//

	taskEXIT_CRITICAL();				//�˳��ٽ���
	G_Delete_Task_struct.D_Task = Start_Reset_Sim800c_Task_TaskHandle;
	Start_Reset_Sim800c_Task_TaskHandle = NULL;
	G_Delete_Task_struct.sign = ENABLE;
	while(1)
	{
		osDelay(1000);
	}
}

void Start_Sim800c_Task(void const * argument)
{
	Sim800c_Semaphore = xSemaphoreCreateMutex();
	if( Sim800c_Semaphore == NULL )
	{
		printf("Sim800c_Semaphore = NULL\r\n");
	}
	else
	{
	//	printf("xSemaphore = success\r\n");
	}
	
	taskENTER_CRITICAL();				//�����ٽ���
	
	osThreadDef(Scheduler_data_Task, Start_Scheduler_data_Task, osPriorityNormal, 0, 128);	//�������ݷ���������Ⱥ���
	Start_Scheduler_data_Task_TaskHandle = osThreadCreate(osThread(Scheduler_data_Task), NULL);
	
	osThreadDef(Reset_Sim800c_Task, Start_Reset_Sim800c_Task, osPriorityNormal, 0, 128);	//��������Sim800C��ʼ�����Ⱥ���
	Start_Reset_Sim800c_Task_TaskHandle = osThreadCreate(osThread(Reset_Sim800c_Task), NULL);

	osThreadDef(Recv_Onenet_data_Task, Start_Recv_Onenet_data_Task, osPriorityNormal, 0, 128);	//������������
	Start_Recv_Onenet_data_TaskHandle = osThreadCreate(osThread(Recv_Onenet_data_Task), NULL);	//

	taskEXIT_CRITICAL();				//�˳��ٽ���

	while(1)
	{
//		xSemaphoreTake(Sim800c_Semaphore,portMAX_DELAY);//��ȡ�����ź���
//		Get_Sim800C_Signal();
//		xSemaphoreGive(Sim800c_Semaphore);//�ͷ��ź���
		//printf("G_Sim800C_Signal = %d\r\n",G_Sim800C_Signal);
		//12864��ʾ���ź�
		osDelay(15000);
	}
}





