/****************************************Copyright (c)****************************************************
**                               DongGuan EAST Co.,LTD.
**                                     
**                                 http://www.EASTUPS.com
**
**--------------File Info---------------------------------------------------------------------------------
** File Name:               carlock_task.h
** Last modified Date:      2016.9.29
** Last Version:            1.0
** Description:             与地锁模块通信
** 
**--------------------------------------------------------------------------------------------------------
** Created By:              
** Created date:            2016.9.29
** Version:                 1.0
** Descriptions:            The original version 初始版本
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
typedef unsigned long long Time_Stamp;//时间戳定义为64bit变量
#define	ISO8583_BUSY_SN		20	//业务流水号长度
typedef struct
{
	unsigned short year;//年	
	unsigned char month;//月 1---12
	unsigned char day_of_month;//某月中的某日，如3月2日, 1---31	
	
	unsigned char week_day;//星期几 1---7  7-sunday,1-monday....6-saturday
	unsigned char hour;//小时0---23
	unsigned char minute;//分钟 0---59
	unsigned char second;//秒 0---59
	
}CHG_BIN_TIME;	//数值为二进制格式

typedef struct
{
	unsigned short year;//年	
	unsigned char month;//月 1---12
	unsigned char day_of_month;//某月中的某日，如3月2日, 1---31	
	
	unsigned char week_day;//星期几 1---7  7-sunday,1-monday....6-saturday
	unsigned char hour;//小时0---23
	unsigned char minute;//分钟 0---59
	unsigned char second;//秒 0---59
	
}BIN_TIME;	//数值为二进制格式

typedef struct{
	unsigned char park_busy;//值0表示车位空闲可用
	unsigned char park_port;//停车位，0---第1个车位锁所在的车位,1---第2个车位锁所在的车位
	unsigned char park_start_valid;//开始时间写入有效标志，值1有效
	unsigned char park_end_valid;//停止时间写入有效标志，值1有效
	unsigned char user_id[16];//用户卡号,BCD
	unsigned long park_rmb;//停车费用（以分为单位）,BIN
	time_t  park_start_time;//停车开始时间(按检测到车进入停车位开始算),BIN
	time_t  park_end_time;//停车结束时间(车离开车位的时间),BIN

	unsigned char park_recrod_flag;//是否已写入部分停车交易记录，值0未写，值1开启充电交易记录已写入了停车开始时间
	unsigned char chg_once_done_flag;//是否已结算完成了1次充电
	unsigned char park_card_type;//值PARK_FEE_BY_APP表示APP控制停车，值PARK_FEE_BY_CARD表示刷卡控制停车
	unsigned char pad;
	
	unsigned short park_price;//停车阶段的收费标准 单位元/10分钟，系数0.01
}USER_PARK_INFO;
/*
typedef struct
{
	/////////////////////////V2.1.8协议增加停车费用//////////////////////////////
  unsigned char ter_sn[12];
	unsigned char card_sn[16];
	unsigned char falg[2]; // 3 02 交易结果(00：扣费成功；01：灰锁，02：补充交易)
	unsigned char charge_type[2]; // 充电方式（01：刷卡，02：手机）
	unsigned char charge_BSN[20]; // 11业务流水号;
	unsigned char stop_car_allrmb; // 停车费（以分为单位）;
	unsigned char start_time[14]; // 停车费（以分为单位）;
	unsigned char end_time[14]; // 停车费（以分为单位）;
	unsigned char stop_car_rmb_price; // 停车费（元/10分钟);
	unsigned char charge_last_rmb; // 停车费（元/10分钟);
	unsigned char ConnectorId;
}CHG_DEAL_REC;//充电结算数据记录----152字节----20170928//128字节,必须为4的整数倍---128字节---20161111
*/
typedef struct _CHG_DEAL_REC_Struct
{
	unsigned char User_Card[16];		//用户ID	 2 16N 
	unsigned char falg[2]; // 3 02 交易结果(00：扣费成功；01：灰锁，02：补充交易)
	unsigned char charge_type[2]; // 充电方式（01：刷卡，02：手机）
	unsigned char Busy_SN[20]; // 11业务流水号;
	unsigned char stop_car_allrmb[8]; // 停车费（以分为单位）;
	unsigned char stop_car_start_time[14]; // 停车费（以分为单位）;
	unsigned char stop_car_end_time[14]; // 停车费（以分为单位）;
	unsigned char stop_car_rmb_price[8]; // 停车费（元/10分钟);
	unsigned char charge_last_rmb[8]; // 卡前余额 （元/10分钟) ---暂时无用;
	unsigned char ter_sn[12];     //充电桩序列号 41	N	n..12	
  unsigned char ConnectorId[2];     //充电枪号	54	N	n2
}CHG_DEAL_REC;

typedef struct   //设备其他参数定义
{	
	unsigned char  plug_lock_ctl;//是否启用枪锁控制,值0不启用，值1启用且检测锁状态，值2启用但不检测状态	
	unsigned char  car_lock_addr[2];
	unsigned short car_lock_num;//车位锁数量0---2
	unsigned short car_lock_time;//检测到无车的时候，车位上锁的时间,单位分钟，默认3分钟，在降下车锁后开始计时，超时后仍然无车则锁上车位
	unsigned long  car_park_price;//每10分钟的停车费，单位元，系数0.01
	unsigned short free_minutes_after_charge;//充电完成后还可免费停车的时长,单位分钟
	unsigned char  free_when_charge;//充电期间是否免停车费，值1免费，值0不免费
}DEV_PARA,*PDEV_PARA;//必须为4的整数倍,MAX 736字节

typedef struct   //与设备FLASH中对应的保存参数的区域定义一致
{
	DEV_PARA       dev_para;
}DEV_CFG,*PDEV_CFG;//必须为4的整数倍
extern DEV_CFG dev_cfg;


extern USER_PARK_INFO g_user_park_info[MAX_CARLOCK_NUM];
extern unsigned short g_car_idle_min_distance[MAX_CARLOCK_NUM];
extern unsigned char g_card_car_lock_flag;//刷卡控制地锁控制值，值APP_CAR_LOCK_UP和APP_CAR_LOCK_DOWN表升起和放下地锁
extern unsigned char g_card_car_lock_port;//刷卡控制地锁号，值0表示控制地锁1，值1表示控制地锁2	
/*********************************************************************************************************
   函数声明
*********************************************************************************************************/	
//返回值1表示车位上有车，0表示无车
extern unsigned char IsCarPresent(unsigned char car_lock_index);

//返回值1表示车位锁已经降下，0表示车位锁未降下
extern unsigned char IsCarLockDown(unsigned char car_lock_index);

//返回值1表示车位锁通信故障，0表示正常
extern unsigned char IsLockCommFailed(unsigned char car_lock_index);

//返回值1表示当前卡号正在泊车中，值0没有
extern unsigned char IsCardParked(unsigned char card_id[]);

//检查是否有有效的泊车，如在设备启动时，地锁是放下的，则会将值0作为泊车用户!此为无效用户
extern unsigned char IsParkBusy(unsigned char car_lock_index);

extern CHG_BIN_TIME   RtcTime2ParkTime(BIN_TIME  cur_rtc_time);
//中断处理
extern void carlock_UART_IRQ(void);
extern void carlockTimeOutChk(void);
//uCOSII task,串口的数据收发处理
extern void carlock_Task();

#ifdef __cplusplus
    }
#endif

#endif  //  __carlock_TASK_H__

