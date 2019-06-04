/****************************************Copyright (c)****************************************************
**                               DongGuan EAST Co.,LTD.
**                                     
**                                 http://www.EASTUPS.com
**
**--------------File Info---------------------------------------------------------------------------------
** File Name:               carlock_task.h
** Last modified Date:      2016.9.29
** Last Version:            1.0
** Description:             �����ģ��ͨ��
** 
**--------------------------------------------------------------------------------------------------------
** Created By:              
** Created date:            2016.9.29
** Version:                 1.0
** Descriptions:            The original version ��ʼ�汾
**
**--------------------------------------------------------------------------------------------------------
*********************************************************************************************************/

#ifndef  __Carlock_TASK_H__
#define  __Carlock_TASK_H__



//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C" {
#endif
#include "Car_lock.h"
//#include "user_storage.h"
//#include "user_uart_driver.h"
//#include "Time_Utity.h"
typedef unsigned long long Time_Stamp;//ʱ�������Ϊ64bit����
#define	ISO8583_BUSY_SN		20	//ҵ����ˮ�ų���
typedef struct
{
	unsigned short year;//��	
	unsigned char month;//�� 1---12
	unsigned char day_of_month;//ĳ���е�ĳ�գ���3��2��, 1---31	
	
	unsigned char week_day;//���ڼ� 1---7  7-sunday,1-monday....6-saturday
	unsigned char hour;//Сʱ0---23
	unsigned char minute;//���� 0---59
	unsigned char second;//�� 0---59
	
}CHG_BIN_TIME;	//��ֵΪ�����Ƹ�ʽ

typedef struct
{
	unsigned short year;//��	
	unsigned char month;//�� 1---12
	unsigned char day_of_month;//ĳ���е�ĳ�գ���3��2��, 1---31	
	
	unsigned char week_day;//���ڼ� 1---7  7-sunday,1-monday....6-saturday
	unsigned char hour;//Сʱ0---23
	unsigned char minute;//���� 0---59
	unsigned char second;//�� 0---59
	
}BIN_TIME;	//��ֵΪ�����Ƹ�ʽ

typedef struct{
	unsigned char park_busy;//ֵ0��ʾ��λ���п���
	unsigned char park_port;//ͣ��λ��0---��1����λ�����ڵĳ�λ,1---��2����λ�����ڵĳ�λ
	unsigned char park_start_valid;//��ʼʱ��д����Ч��־��ֵ1��Ч
	unsigned char park_end_valid;//ֹͣʱ��д����Ч��־��ֵ1��Ч
	unsigned char user_id[16];//�û�����,BCD
	unsigned long park_rmb;//ͣ�����ã��Է�Ϊ��λ��,BIN
	time_t  park_start_time;//ͣ����ʼʱ��(����⵽������ͣ��λ��ʼ��),BIN
	time_t  park_end_time;//ͣ������ʱ��(���뿪��λ��ʱ��),BIN

	unsigned char park_recrod_flag;//�Ƿ���д�벿��ͣ�����׼�¼��ֵ0δд��ֵ1������罻�׼�¼��д����ͣ����ʼʱ��
	unsigned char chg_once_done_flag;//�Ƿ��ѽ��������1�γ��
	unsigned char park_card_type;//ֵPARK_FEE_BY_APP��ʾAPP����ͣ����ֵPARK_FEE_BY_CARD��ʾˢ������ͣ��
	unsigned char pad;
	
	unsigned short park_price;//ͣ���׶ε��շѱ�׼ ��λԪ/10���ӣ�ϵ��0.01
}USER_PARK_INFO;
/*
typedef struct
{
	/////////////////////////V2.1.8Э������ͣ������//////////////////////////////
  unsigned char ter_sn[12];
	unsigned char card_sn[16];
	unsigned char falg[2]; // 3 02 ���׽��(00���۷ѳɹ���01��������02�����佻��)
	unsigned char charge_type[2]; // ��緽ʽ��01��ˢ����02���ֻ���
	unsigned char charge_BSN[20]; // 11ҵ����ˮ��;
	unsigned char stop_car_allrmb; // ͣ���ѣ��Է�Ϊ��λ��;
	unsigned char start_time[14]; // ͣ���ѣ��Է�Ϊ��λ��;
	unsigned char end_time[14]; // ͣ���ѣ��Է�Ϊ��λ��;
	unsigned char stop_car_rmb_price; // ͣ���ѣ�Ԫ/10����);
	unsigned char charge_last_rmb; // ͣ���ѣ�Ԫ/10����);
	unsigned char ConnectorId;
}CHG_DEAL_REC;//���������ݼ�¼----152�ֽ�----20170928//128�ֽ�,����Ϊ4��������---128�ֽ�---20161111
*/
typedef struct _CHG_DEAL_REC_Struct
{
	unsigned char User_Card[16];		//�û�ID	 2 16N 
	unsigned char falg[2]; // 3 02 ���׽��(00���۷ѳɹ���01��������02�����佻��)
	unsigned char charge_type[2]; // ��緽ʽ��01��ˢ����02���ֻ���
	unsigned char Busy_SN[20]; // 11ҵ����ˮ��;
	unsigned char stop_car_allrmb[8]; // ͣ���ѣ��Է�Ϊ��λ��;
	unsigned char stop_car_start_time[14]; // ͣ���ѣ��Է�Ϊ��λ��;
	unsigned char stop_car_end_time[14]; // ͣ���ѣ��Է�Ϊ��λ��;
	unsigned char stop_car_rmb_price[8]; // ͣ���ѣ�Ԫ/10����);
	unsigned char charge_last_rmb[8]; // ��ǰ��� ��Ԫ/10����) ---��ʱ����;
	unsigned char ter_sn[12];     //���׮���к� 41	N	n..12	
  unsigned char ConnectorId[2];     //���ǹ��	54	N	n2
}CHG_DEAL_REC;

typedef struct   //�豸������������
{	
	unsigned char  plug_lock_ctl;//�Ƿ�����ǹ������,ֵ0�����ã�ֵ1�����Ҽ����״̬��ֵ2���õ������״̬	
	unsigned char  car_lock_addr[2];
	unsigned short car_lock_num;//��λ������0---2
	unsigned short car_lock_time;//��⵽�޳���ʱ�򣬳�λ������ʱ��,��λ���ӣ�Ĭ��3���ӣ��ڽ��³�����ʼ��ʱ����ʱ����Ȼ�޳������ϳ�λ
	unsigned long  car_park_price;//ÿ10���ӵ�ͣ���ѣ���λԪ��ϵ��0.01
	unsigned short free_minutes_after_charge;//�����ɺ󻹿����ͣ����ʱ��,��λ����
	unsigned char  free_when_charge;//����ڼ��Ƿ���ͣ���ѣ�ֵ1��ѣ�ֵ0�����
}DEV_PARA,*PDEV_PARA;//����Ϊ4��������,MAX 736�ֽ�

typedef struct   //���豸FLASH�ж�Ӧ�ı��������������һ��
{
	DEV_PARA       dev_para;
}DEV_CFG,*PDEV_CFG;//����Ϊ4��������
extern DEV_CFG dev_cfg;


extern USER_PARK_INFO g_user_park_info[MAX_CARLOCK_NUM];
extern unsigned short g_car_idle_min_distance[MAX_CARLOCK_NUM];
extern unsigned char g_card_car_lock_flag;//ˢ�����Ƶ�������ֵ��ֵAPP_CAR_LOCK_UP��APP_CAR_LOCK_DOWN������ͷ��µ���
extern unsigned char g_card_car_lock_port;//ˢ�����Ƶ����ţ�ֵ0��ʾ���Ƶ���1��ֵ1��ʾ���Ƶ���2	
/*********************************************************************************************************
   ��������
*********************************************************************************************************/	
//����ֵ1��ʾ��λ���г���0��ʾ�޳�
extern unsigned char IsCarPresent(unsigned char car_lock_index);

//����ֵ1��ʾ��λ���Ѿ����£�0��ʾ��λ��δ����
extern unsigned char IsCarLockDown(unsigned char car_lock_index);

//����ֵ1��ʾ��λ��ͨ�Ź��ϣ�0��ʾ����
extern unsigned char IsLockCommFailed(unsigned char car_lock_index);

//����ֵ1��ʾ��ǰ�������ڲ����У�ֵ0û��
extern unsigned char IsCardParked(unsigned char card_id[]);

//����Ƿ�����Ч�Ĳ����������豸����ʱ�������Ƿ��µģ���Ὣֵ0��Ϊ�����û�!��Ϊ��Ч�û�
extern unsigned char IsParkBusy(unsigned char car_lock_index);

extern CHG_BIN_TIME   RtcTime2ParkTime(BIN_TIME  cur_rtc_time);
//�жϴ���
extern void carlock_UART_IRQ(void);
extern void carlockTimeOutChk(void);
//uCOSII task,���ڵ������շ�����
extern void carlock_Task();

#ifdef __cplusplus
    }
#endif

#endif  //  __carlock_TASK_H__

