/*******************************************************************************
  Copyright(C)    2014-2015,      EAST Co.Ltd.
  File name :     bms.c
  Author :        dengjh
  Version:        1.0
  Date:           2014.5.11
  Description:    
  Other:          
  Function List:  
  History:        
          <author>   <time>   <version>   <desc>
            dengjh    2014.4.11   1.0      Create
*******************************************************************************/
#include <getopt.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include<time.h>
#include <sys/prctl.h>
#include "globalvar.h"
#include "MC_can.h"

#define PF_CAN   29

extern UINT32 Dc_shunt_Set_CL_flag[CONTROL_DC_NUMBER];

MC_ALL_FRAME    MC_All_Frame[CONTROL_DC_NUMBER];
MC_CONTROL_VAR  mc_control_var[CONTROL_DC_NUMBER];

PC_PGN16512_FRAME host_sent[CONTROL_DC_NUMBER];
PC_PGN16256_FRAME host_recv[CONTROL_DC_NUMBER];

#define  PGN16256_ID_CM_1 0x18108AF6  //	控制器-功率分配

unsigned char host_recv_flag[CONTROL_DC_NUMBER] = {0};
unsigned char host_congji[CONTROL_DC_NUMBER][CONTROL_DC_NUMBER];
unsigned int model_temp_v[CONTROL_DC_NUMBER];
unsigned int model_temp_a[CONTROL_DC_NUMBER];
unsigned char host_model_num[CONTROL_DC_NUMBER];
unsigned char debug = 0;

//五枪变量定义
#define max_gun_num CONTROL_DC_NUMBER //宏定义系统的枪数量便于数组的定义
unsigned char GUN_MAX  = max_gun_num;   //该变量根据系统类型进行赋初值 
unsigned char gun_5_temp = 0;//用来循环处理该枪的并机信息
PC_PGN16512_FRAME host_sent_temp[max_gun_num];//中间数组，用于实际发送并机数据与映射发送并机数据处理
PC_PGN16256_FRAME host_recv_temp[max_gun_num];//中间数组，用于实际接收并机数据与映射并机数据处理

//检查该枪是可以并机；
//如果该枪没有插枪，如果该枪已开启输出，判断是否是当前枪的从机，若不是，不能并机
unsigned char host_panduan(unsigned char gun_zhu,unsigned char gun_cong)
{
	unsigned char falg = 0,i,host_f;
	{
		for(i=0;i<Globa->Charger_param.gun_config;i++)
		{
			if(i!=gun_zhu)
			{
				if(host_congji[i][gun_cong] != 2)
					host_f = 1;
				else
				{
					host_f = 3;
					break;
				}
			}		
		}
		if((host_recv[gun_cong].chgstart == 0) && (host_f != 3))
		{
			falg = 1;
		}
		else if(host_congji[gun_zhu][gun_cong] == 2)
		{
			falg = 1;
		}
		return falg;
	}	
}
//五枪专用判断
//检查该枪是可以并机；
//如果该枪没有插枪，如果该枪已开启输出，判断是否是当前枪的从机，若不是，不能并机
unsigned char host_panduan_temp(unsigned char gun_zhu,unsigned char gun_cong)
{
	unsigned char falg = 0,i,host_f;
	{
		for(i=0;i<Globa->Charger_param.gun_config;i++)
		{
			if(i!=gun_zhu)
			{
				if(host_congji[i][gun_cong] != 2)
					host_f = 1;
				else
				{
					host_f = 3;
					break;
				}
			}		
		}
		if((host_recv_temp[gun_cong].chgstart == 0) && (host_f != 3))
		{
			falg = 1;
		}
		else if(host_congji[gun_zhu][gun_cong] == 2)
		{
			falg = 1;
		}
		return falg;
	}	
}

#define USER_TICK_PERIOD    10 //计数器递增1代表的时长，单位ms
//Gun_No 0---3 启动一个定时器定时time指定时长，单位ms
//返回值1表示有空闲定时器，定时器初始化成功
UINT32 MC_timer_start(int Gun_No, int time)
{
	if(Gun_No >= Globa->Charger_param.gun_config)
		return 0;
		int i;
	for(i=0;i<USER_TIMER_NO;i++)
	{
		//if(0==mc_control_var[Gun_No].mc_timers[i].timer_enable)
		{
			if( (mc_control_var[Gun_No].mc_timers[i].timer_set_in_ms == 0)||//0表示该计数器未初始化可用=========
				(mc_control_var[Gun_No].mc_timers[i].timer_set_in_ms == time)//之前已初始化过，重新开始计数
			)
			{
				mc_control_var[Gun_No].mc_timers[i].timer_end_flag = 0;
				mc_control_var[Gun_No].mc_timers[i].timer_tick_cnt = 0;
				mc_control_var[Gun_No].mc_timers[i].timer_stop_tick = time/USER_TICK_PERIOD;//设置定时器终点计数
				mc_control_var[Gun_No].mc_timers[i].timer_set_in_ms = time;
				mc_control_var[Gun_No].mc_timers[i].timer_enable = 1;//start
				return 1;
				//break;//break at first match
			}
		}
	}
	return 0;//no idle timer
}

//停止指定时长的定时器
//返回值1表示停止成功
UINT32 MC_timer_stop(int Gun_No, int time)
{
	if(Gun_No >= Globa->Charger_param.gun_config)
		return 0;
	int i;
	for(i=0;i<USER_TIMER_NO;i++)
	{
		if(mc_control_var[Gun_No].mc_timers[i].timer_enable)
		{
			if(mc_control_var[Gun_No].mc_timers[i].timer_set_in_ms == time)//match
			{
				mc_control_var[Gun_No].mc_timers[i].timer_enable = 0;//stop
				mc_control_var[Gun_No].mc_timers[i].timer_end_flag = 0;
				mc_control_var[Gun_No].mc_timers[i].timer_tick_cnt = 0;
				mc_control_var[Gun_No].mc_timers[i].timer_stop_tick = 0;//reset
				mc_control_var[Gun_No].mc_timers[i].timer_set_in_ms = 0;//reset
				return 1;//break at first match
				//break;
			}
		}
	}
	return 0;
}

//初始化指定充电枪的所有定时器
void MC_Init_timers(int Gun_No)
{
	if(Gun_No >= Globa->Charger_param.gun_config)
		return ;
	int i;
	for(i=0;i<USER_TIMER_NO;i++)
		memset(&mc_control_var[Gun_No].mc_timers[i], 0, sizeof(MC_TIMER));
	
}
//运行指定充电枪的定时器
void MC_timer_run(int Gun_No)
{
	if(Gun_No >= Globa->Charger_param.gun_config)
		return ;
	int i;
	for(i=0;i<USER_TIMER_NO;i++)
	{
		if(mc_control_var[Gun_No].mc_timers[i].timer_enable)
		{
			if(0 == mc_control_var[Gun_No].mc_timers[i].timer_end_flag)
			{
				mc_control_var[Gun_No].mc_timers[i].timer_tick_cnt++;				
				if(mc_control_var[Gun_No].mc_timers[i].timer_tick_cnt >= mc_control_var[Gun_No].mc_timers[i].timer_stop_tick)
					mc_control_var[Gun_No].mc_timers[i].timer_end_flag = 1;
			}
		}
	}	
}

//返回值1表示对应的定时器定时时间到
UINT32 MC_timer_flag(int Gun_No, int time)
{
	UINT32 flag=0;
	if(Gun_No >= Globa->Charger_param.gun_config)
		return 0;
	int i;
	for(i=0;i<USER_TIMER_NO;i++)
	{
		if(mc_control_var[Gun_No].mc_timers[i].timer_enable)
		{
			if(mc_control_var[Gun_No].mc_timers[i].timer_set_in_ms == time)//match
			{
				flag = mc_control_var[Gun_No].mc_timers[i].timer_end_flag;
				break;//break at first match
			}
		}
	}
	return (flag);
}

UINT32 MC_timer_cur_tick(int Gun_No ,int time)
{
	if(Gun_No >= Globa->Charger_param.gun_config)
		return 0;
	int i;
	for(i=0;i<USER_TIMER_NO;i++)
	{
		if(mc_control_var[Gun_No].mc_timers[i].timer_enable)
		{
			if(mc_control_var[Gun_No].mc_timers[i].timer_set_in_ms == time)//match
			{
				return mc_control_var[Gun_No].mc_timers[i].timer_tick_cnt;
			}
		}
	}
	return 0;		
}

//初始化CAN通信的文件描述符
int BMS_can_fd_init(unsigned char *can_id)
{
	struct ifreq ifr;
	struct sockaddr_can addr;
	int s;

	s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	if(s < 0) {
		perror("can socket");
		return (-1);
	}

	strcpy(ifr.ifr_name, can_id);
	if(ioctl(s, SIOCGIFINDEX, &ifr)){
		perror("can ioctl");
		return (-1);
	}

	addr.can_family = PF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
	if(bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("can bind");
		return (-1);
	}

	//原始套接字选项 CAN_RAW_LOOPBACK 为了满足众多应用程序的需要，本地回环功能默认是开启的, 但是在一些嵌入式应用场景中（比如只有一个用户在使用CAN总线），回环功能可以被关闭（各个套接字之间是独立的）
	int loopback = 0; // 0 = disabled, 1 = enabled (default)
	setsockopt(s, SOL_CAN_RAW, CAN_RAW_LOOPBACK, &loopback, sizeof(loopback));

	return (s);
}

void host_handle(struct can_frame frame)
{
	unsigned char gun = 0;
	unsigned char gun_temp = 0,i;
	unsigned char sum = 0,ml_data[100];
	switch(frame.data[0]){//判断接收帧ID
		case 1:
		{   //
			gun = 0;
			host_recv_flag[gun] = 1;
			host_recv[gun].gunstate = frame.data[1] & 0x0f;//host_recv_temp.gunstate;// = Globa.gun_link;	//枪的连接状态	BIN 1Byte	0---未连接 1—连接
			Globa->Control_DC_Infor_Data[gun].gun_link = host_recv[gun].gunstate;
			//host_recv[gun].chgen = host_recv_temp.chgen;// = 0;	   	//允许充电状态	BIN 1Byte	0-允许 1-允许，目前不知道怎么用，先留着
			host_recv[gun].chgstart	= (frame.data[1] >> 4) & 0x0f;//host_recv_temp.chgstart;// = Globa.Charger_on;	//充电开启状态	BIN 1Byte	0-停止 1-开始 ，将开机状态给过去，
			host_recv[gun].bmsmv[0] = frame.data[2];//host_recv_temp.mv[0];// = (Globa.Output_voltage/100);	  	//实际测量电压	BIN 2Byte	 0.1V
			host_recv[gun].bmsmv[1] = frame.data[3];//host_recv_temp.mv[1];// = (Globa.Output_voltage/100) >> 8;	  	//实际测量电压	BIN 2Byte	0.1V 
			host_recv[gun].bmsma[0] = frame.data[4];// = (Globa.Output_current);		//实际测量电流	BIN 2Byte	0.01A
			host_recv[gun].bmsma[1] = frame.data[5];// = (Globa.Output_current) >> 8;		//实际测量电流	BIN 2Byte	 0.01A
			//host_recv[gun].bmsmv[0] = host_recv_temp.bmsmv[0];// = (Globa.Output_voltage);	//BMS需求电压	BIN 2Byte	0.1V
			//host_recv[gun].bmsmv[1] = host_recv_temp.bmsmv[1];// = (Globa.Output_voltage) >> 8;	//BMS需求电压	BIN 2Byte	0.1V
			//host_recv[gun].bmsma[0] = host_recv_temp.bmsma[0];// = (Globa.need_current);	//BMS需求电流	BIN 2Byte 0.01A
			//host_recv[gun].bmsma[1] = host_recv_temp.bmsma[1];// = (Globa.need_current) >> 8;	//BMS需求电流	BIN 2Byte	0.01A
			host_recv[gun].host_flag = frame.data[6];//host_recv_temp.host_flag;
			host_recv[gun].Charger_good_num = frame.data[7] & 0x0f;
			host_recv[gun].need_model_num = (frame.data[7] >> 4) & 0x0f;
			if(debug == 1)printf("host_recv[0].model_output_vol[0] = %x\n",host_recv[gun].bmsmv[0]);
			if(debug == 1)printf("host_recv[0].model_output_vol[1] = %x\n",host_recv[gun].bmsmv[1]);
			if(debug == 1)printf("host_recv[gun_temp].need_model_num = %d\n",host_recv[gun].need_model_num);			
			break;
		}
		case 2:
		{   //
			gun = 1;
			host_recv_flag[gun] = 1;
			host_recv[gun].gunstate = frame.data[1] & 0x0f;//host_recv_temp.gunstate;// = Globa.gun_link;	//枪的连接状态	BIN 1Byte	0---未连接 1—连接
			Globa->Control_DC_Infor_Data[gun].gun_link = host_recv[gun].gunstate;
			//host_recv[gun].chgen = host_recv_temp.chgen;// = 0;	   	//允许充电状态	BIN 1Byte	0-允许 1-允许，目前不知道怎么用，先留着
			host_recv[gun].chgstart	= (frame.data[1] >> 4) & 0x0f;//host_recv_temp.chgstart;// = Globa.Charger_on;	//充电开启状态	BIN 1Byte	0-停止 1-开始 ，将开机状态给过去，
			host_recv[gun].bmsmv[0] = frame.data[2];//host_recv_temp.mv[0];// = (Globa.Output_voltage/100);	  	//实际测量电压	BIN 2Byte	 0.1V
			host_recv[gun].bmsmv[1] = frame.data[3];//host_recv_temp.mv[1];// = (Globa.Output_voltage/100) >> 8;	  	//实际测量电压	BIN 2Byte	0.1V 
			host_recv[gun].bmsma[0] = frame.data[4];// = (Globa.Output_current);		//实际测量电流	BIN 2Byte	0.01A
			host_recv[gun].bmsma[1] = frame.data[5];// = (Globa.Output_current) >> 8;		//实际测量电流	BIN 2Byte	 0.01A
			//host_recv[gun].bmsmv[0] = host_recv_temp.bmsmv[0];// = (Globa.Output_voltage);	//BMS需求电压	BIN 2Byte	0.1V
			//host_recv[gun].bmsmv[1] = host_recv_temp.bmsmv[1];// = (Globa.Output_voltage) >> 8;	//BMS需求电压	BIN 2Byte	0.1V
			//host_recv[gun].bmsma[0] = host_recv_temp.bmsma[0];// = (Globa.need_current);	//BMS需求电流	BIN 2Byte 0.01A
			//host_recv[gun].bmsma[1] = host_recv_temp.bmsma[1];// = (Globa.need_current) >> 8;	//BMS需求电流	BIN 2Byte	0.01A
			host_recv[gun].host_flag = frame.data[6];//host_recv_temp.host_flag;
			host_recv[gun].Charger_good_num = frame.data[7] & 0x0f;
			host_recv[gun].need_model_num = (frame.data[7] >> 4) & 0x0f;	
			if(debug == 1)printf("host_recv[1].model_output_vol[0] = %x\n",host_recv[gun].bmsmv[0]);
			if(debug == 1)printf("host_recv[1].model_output_vol[1] = %x\n",host_recv[gun].bmsmv[1]);			
			break;
		}
		case 3:
		{   //
			gun = 2;
			host_recv_flag[gun] = 1;
			host_recv[gun].gunstate = frame.data[1] & 0x0f;//host_recv_temp.gunstate;// = Globa.gun_link;	//枪的连接状态	BIN 1Byte	0---未连接 1—连接
			Globa->Control_DC_Infor_Data[gun].gun_link = host_recv[gun].gunstate;
			//host_recv[gun].chgen = host_recv_temp.chgen;// = 0;	   	//允许充电状态	BIN 1Byte	0-允许 1-允许，目前不知道怎么用，先留着
			host_recv[gun].chgstart	= (frame.data[1] >> 4) & 0x0f;//host_recv_temp.chgstart;// = Globa.Charger_on;	//充电开启状态	BIN 1Byte	0-停止 1-开始 ，将开机状态给过去，
			host_recv[gun].bmsmv[0] = frame.data[2];//host_recv_temp.mv[0];// = (Globa.Output_voltage/100);	  	//实际测量电压	BIN 2Byte	 0.1V
			host_recv[gun].bmsmv[1] = frame.data[3];//host_recv_temp.mv[1];// = (Globa.Output_voltage/100) >> 8;	  	//实际测量电压	BIN 2Byte	0.1V 
			host_recv[gun].bmsma[0] = frame.data[4];// = (Globa.Output_current);		//实际测量电流	BIN 2Byte	0.01A
			host_recv[gun].bmsma[1] = frame.data[5];// = (Globa.Output_current) >> 8;		//实际测量电流	BIN 2Byte	 0.01A
			//host_recv[gun].bmsmv[0] = host_recv_temp.bmsmv[0];// = (Globa.Output_voltage);	//BMS需求电压	BIN 2Byte	0.1V
			//host_recv[gun].bmsmv[1] = host_recv_temp.bmsmv[1];// = (Globa.Output_voltage) >> 8;	//BMS需求电压	BIN 2Byte	0.1V
			//host_recv[gun].bmsma[0] = host_recv_temp.bmsma[0];// = (Globa.need_current);	//BMS需求电流	BIN 2Byte 0.01A
			//host_recv[gun].bmsma[1] = host_recv_temp.bmsma[1];// = (Globa.need_current) >> 8;	//BMS需求电流	BIN 2Byte	0.01A
			host_recv[gun].host_flag = frame.data[6];//host_recv_temp.host_flag;
			host_recv[gun].Charger_good_num = frame.data[7] & 0x0f;
			host_recv[gun].need_model_num = (frame.data[7] >> 4) & 0x0f;
			if(debug == 1)printf("host_recv[2].model_output_vol[0] = %x\n",host_recv[gun].bmsmv[0]);
			if(debug == 1)printf("host_recv[2].model_output_vol[1] = %x\n",host_recv[gun].bmsmv[1]);			
			break;
		}
		case 4:
		{   //
			gun = 3;
			host_recv_flag[gun] = 1;
			host_recv[gun].gunstate = frame.data[1] & 0x0f;//host_recv_temp.gunstate;// = Globa.gun_link;	//枪的连接状态	BIN 1Byte	0---未连接 1—连接
			Globa->Control_DC_Infor_Data[gun].gun_link = host_recv[gun].gunstate;
			//host_recv[gun].chgen = host_recv_temp.chgen;// = 0;	   	//允许充电状态	BIN 1Byte	0-允许 1-允许，目前不知道怎么用，先留着
			host_recv[gun].chgstart	= (frame.data[1] >> 4) & 0x0f;//host_recv_temp.chgstart;// = Globa.Charger_on;	//充电开启状态	BIN 1Byte	0-停止 1-开始 ，将开机状态给过去，
			host_recv[gun].bmsmv[0] = frame.data[2];//host_recv_temp.mv[0];// = (Globa.Output_voltage/100);	  	//实际测量电压	BIN 2Byte	 0.1V
			host_recv[gun].bmsmv[1] = frame.data[3];//host_recv_temp.mv[1];// = (Globa.Output_voltage/100) >> 8;	  	//实际测量电压	BIN 2Byte	0.1V 
			host_recv[gun].bmsma[0] = frame.data[4];// = (Globa.Output_current);		//实际测量电流	BIN 2Byte	0.01A
			host_recv[gun].bmsma[1] = frame.data[5];// = (Globa.Output_current) >> 8;		//实际测量电流	BIN 2Byte	 0.01A
			//host_recv[gun].bmsmv[0] = host_recv_temp.bmsmv[0];// = (Globa.Output_voltage);	//BMS需求电压	BIN 2Byte	0.1V
			//host_recv[gun].bmsmv[1] = host_recv_temp.bmsmv[1];// = (Globa.Output_voltage) >> 8;	//BMS需求电压	BIN 2Byte	0.1V
			//host_recv[gun].bmsma[0] = host_recv_temp.bmsma[0];// = (Globa.need_current);	//BMS需求电流	BIN 2Byte 0.01A
			//host_recv[gun].bmsma[1] = host_recv_temp.bmsma[1];// = (Globa.need_current) >> 8;	//BMS需求电流	BIN 2Byte	0.01A
			host_recv[gun].host_flag = frame.data[6];//host_recv_temp.host_flag;
			host_recv[gun].Charger_good_num = frame.data[7] & 0x0f;
			host_recv[gun].need_model_num = (frame.data[7] >> 4) & 0x0f;	
			if(debug == 1)printf("host_recv[3].model_output_vol[0] = %x\n",host_recv[gun].bmsmv[0]);
			if(debug == 1)printf("host_recv[3].model_output_vol[1] = %x\n",host_recv[gun].bmsmv[1]);			
			break;
		}
		case 5:
		{   //
			gun = 4;
			host_recv_flag[gun] = 1;
			host_recv[gun].gunstate = frame.data[1] & 0x0f;//host_recv_temp.gunstate;// = Globa.gun_link;	//枪的连接状态	BIN 1Byte	0---未连接 1—连接
			Globa->Control_DC_Infor_Data[gun].gun_link = host_recv[gun].gunstate;
			//host_recv[gun].chgen = host_recv_temp.chgen;// = 0;	   	//允许充电状态	BIN 1Byte	0-允许 1-允许，目前不知道怎么用，先留着
			host_recv[gun].chgstart	= (frame.data[1] >> 4) & 0x0f;//host_recv_temp.chgstart;// = Globa.Charger_on;	//充电开启状态	BIN 1Byte	0-停止 1-开始 ，将开机状态给过去，
			host_recv[gun].bmsmv[0] = frame.data[2];//host_recv_temp.mv[0];// = (Globa.Output_voltage/100);	  	//实际测量电压	BIN 2Byte	 0.1V
			host_recv[gun].bmsmv[1] = frame.data[3];//host_recv_temp.mv[1];// = (Globa.Output_voltage/100) >> 8;	  	//实际测量电压	BIN 2Byte	0.1V 
			host_recv[gun].bmsma[0] = frame.data[4];// = (Globa.Output_current);		//实际测量电流	BIN 2Byte	0.01A
			host_recv[gun].bmsma[1] = frame.data[5];// = (Globa.Output_current) >> 8;		//实际测量电流	BIN 2Byte	 0.01A
			//host_recv[gun].bmsmv[0] = host_recv_temp.bmsmv[0];// = (Globa.Output_voltage);	//BMS需求电压	BIN 2Byte	0.1V
			//host_recv[gun].bmsmv[1] = host_recv_temp.bmsmv[1];// = (Globa.Output_voltage) >> 8;	//BMS需求电压	BIN 2Byte	0.1V
			//host_recv[gun].bmsma[0] = host_recv_temp.bmsma[0];// = (Globa.need_current);	//BMS需求电流	BIN 2Byte 0.01A
			//host_recv[gun].bmsma[1] = host_recv_temp.bmsma[1];// = (Globa.need_current) >> 8;	//BMS需求电流	BIN 2Byte	0.01A
			host_recv[gun].host_flag = frame.data[6];//host_recv_temp.host_flag;
			host_recv[gun].Charger_good_num = frame.data[7] & 0x0f;
			host_recv[gun].need_model_num = (frame.data[7] >> 4) & 0x0f;	
			if(debug == 1)printf("host_recv[3].model_output_vol[0] = %x\n",host_recv[gun].bmsmv[0]);
			if(debug == 1)printf("host_recv[3].model_output_vol[1] = %x\n",host_recv[gun].bmsmv[1]);			
			break;
		}
	}
		
}
//上面是用来接收控制板发过来的信息及状态

void host_hand_1()
{
	unsigned char gun = 0;
	unsigned char gun_temp = 0,i;
	if(Globa->Charger_param.gun_config == 3)//3把枪
		host_recv[3].host_flag = 1;	
	if(Globa->Charger_param.gun_config == 2)//2把枪
	{
		host_recv[3].host_flag = 1;	
		host_recv[2].host_flag = 1;	
	}
	if(Globa->Charger_param.gun_config == 1)//1把枪
	{
		host_recv[3].host_flag = 1;	
		host_recv[2].host_flag = 1;	
		host_recv[1].host_flag = 1;	
	}
	for(i=0;i<CONTROL_DC_NUMBER;i++)
		if(Globa->Control_DC_Infor_Data[i].Error.ctrl_connect_lost)
			host_recv[i].host_flag = 1;	
		
	//并机逻辑，枪A的处理	
	gun_temp = 0;
	if(host_recv_flag[gun_temp] == 1)
	{
		host_recv_flag[gun_temp] = 0;
/* 		printf("host_recv[gun_temp].gunstate = %d\n",host_recv[gun_temp].gunstate);
		printf("host_recv[gun_temp].chgstart = %d\n",host_recv[gun_temp].chgstart);
		printf("host_recv[gun_temp].need_model_num = %d\n",host_recv[gun_temp].need_model_num);
		printf("host_recv[gun_temp].Charger_good_num = %d\n",host_recv[gun_temp].Charger_good_num);
		 */
		if(host_recv[gun_temp].gunstate == 1 && host_recv[gun_temp].chgstart == 1 && host_recv[gun_temp].host_flag == 1)
		{//枪插着，并且开机了，说明该枪需要进行模块输出处理
			if(host_recv[gun_temp].need_model_num > host_recv[gun_temp].Charger_good_num)
			{//此时需要并机,需要进行下一步判断
				//if(host_recv[1].host_flag != 0)//判断B枪是否可以并机
				if(!((host_recv[1].host_flag == 0) && (1 == host_panduan(0,1))))
				{//不能并机
					host_sent[1].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
						//if((host_recv[2].host_flag != 0) || !((host_recv[2].host_flag == 0) && (1 == host_panduan(0,2))))//判断C枪是否可以并机
						if(!((host_recv[2].host_flag == 0) && (1 == host_panduan(0,2))))
						{//不能并机
							//当B、C枪都不能并机时，枪A不能并机。
							host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
						    host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
						    host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
						    host_sent[gun_temp].model_output_cur[0] = host_recv[gun_temp].bmsma[0];
						    host_sent[gun_temp].model_output_cur[1] = host_recv[gun_temp].bmsma[1]; 
						   //断开并机接触器
						    host_sent[gun_temp].host_Relay_control = 0;
							host_sent[1].host_Relay_control = 0;
						}
						else if((host_recv[2].host_flag == 0) && (1 == host_panduan(0,2)))
						{//可以并机
							if(host_recv[gun_temp].need_model_num > 
								(host_recv[gun_temp].Charger_good_num + 
									host_recv[2].Charger_good_num))
							{//在C枪并机之后还需要并机
								//if(host_recv[3].host_flag != 0)//判断D枪是否可以并机
								if(!((host_recv[3].host_flag == 0) && (1 == host_panduan(0,3))))
								{//枪D不可以并机，此时只有C枪进行输出
									host_sent[1].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_sent[2].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
								
									host_congji[gun_temp][2] = 2;//标记枪C已经是枪的从机了。
									host_sent[0].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪A的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[2].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪C的模块输出赋值
									host_sent[2].chgenable = 1;
									host_sent[2].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[2].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num +  host_recv[2].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[2].Charger_good_num)/host_model_num[gun_temp];
									host_sent[2].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[2].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								}
								else if((host_recv[3].host_flag == 0) && (1 == host_panduan(0,3)))
								{//可以并机
									host_sent[1].host_Relay_control = 0;//下发并机接触器断开指令
									host_sent[3].host_Relay_control = 0;//下发并机接触器断开指令
									
									host_congji[gun_temp][2] = 2;//标记枪C已经是枪的从机了。
									host_congji[gun_temp][3] = 2;//标记枪D已经是枪的从机了。
									host_sent[0].host_Relay_control = 1;//可以并机，闭合并机接触器
									host_sent[2].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪A的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num 
																+ host_recv[2].Charger_good_num + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
									//为枪C的模块输出赋值
									host_sent[2].chgenable = 1;
									host_sent[2].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[2].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num 
																+ host_recv[2].Charger_good_num + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[2].Charger_good_num)/host_model_num[gun_temp];
									host_sent[2].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[2].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
									//为枪D的模块输出赋值
									host_sent[3].chgenable = 1;
									host_sent[3].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[3].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num 
																+ host_recv[2].Charger_good_num + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[3].Charger_good_num)/host_model_num[gun_temp];
									host_sent[3].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[3].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								}
							}
							else 
							{//不再需要了，此时将A、C的输出信息下发
									//此时B\D都不要并机了，判断有没有之前已经是从机的，有则断开
									host_sent[2].host_Relay_control = 0;//可以并机，闭合并机接触器
									host_sent[1].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
									if(host_congji[gun_temp][1] == 2)
										{
											i = 1;
											host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
											//将开机标志清零；
											host_sent[i].chgenable = 0;
											//将模块输出清零；
											 host_sent[i].model_output_vol[0] = 0;
											 host_sent[i].model_output_vol[1] = 0;
											 host_sent[i].model_output_cur[0] = 0;
											 host_sent[i].model_output_cur[1] = 0; 
											 host_congji[gun_temp][i] = 0;
										}
									if(host_congji[gun_temp][3] == 2)
										{
											i = 3;
											host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
											//将开机标志清零；
											host_sent[i].chgenable = 0;
											//将模块输出清零；
											 host_sent[i].model_output_vol[0] = 0;
											 host_sent[i].model_output_vol[1] = 0;
											 host_sent[i].model_output_cur[0] = 0;
											 host_sent[i].model_output_cur[1] = 0; 
											 host_congji[gun_temp][i] = 0;
										}
									host_congji[gun_temp][2] = 2;//标记枪C已经是枪的从机了。
									host_sent[0].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪A的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[2].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪C的模块输出赋值
									host_sent[2].chgenable = 1;
									host_sent[2].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[2].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num +  host_recv[2].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[2].Charger_good_num)/host_model_num[gun_temp];
									host_sent[2].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[2].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
							}
							
						}
										
				}
				else if((host_recv[1].host_flag == 0) && (1 == host_panduan(0,1)))
				{//可以并机
					if(host_recv[gun_temp].need_model_num > 
						(host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num))
					{//将B枪并进去后，还需要进行并机
						//if(host_recv[2].host_flag != 0)//判断C枪是否可以并机
						if(!((host_recv[2].host_flag == 0) && (1 == host_panduan(0,2))))
						{//不能并机
							//host_sent[2].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
							//if(host_recv[3].host_flag != 0)//判断D枪是否可以并机
							if(!((host_recv[3].host_flag == 0) && (1 == host_panduan(0,3))))
								{//不能并机 此时C、D枪都不能并机，只有B枪可以并机。
									host_sent[3].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_sent[0].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									
									host_congji[gun_temp][1] = 2;//标记枪B已经是枪的从机了。
									host_sent[1].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪A的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪B的模块输出赋值
									host_sent[1].chgenable = 1;
									host_sent[1].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[1].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[1].Charger_good_num)/host_model_num[gun_temp];
									host_sent[1].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[1].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								}
							else if((host_recv[3].host_flag == 0) && (1 == host_panduan(0,3)))
							{//可以并机
								//此时A、B、D可以并机
									host_sent[2].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_sent[0].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
								
									host_congji[gun_temp][1] = 2;//标记枪B已经是枪的从机了。
									host_congji[gun_temp][3] = 2;//标记枪D已经是枪的从机了。
									host_sent[1].host_Relay_control = 1;//可以并机，闭合并机接触器
									host_sent[3].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪A的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num 
															   + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪B的模块输出赋值
									host_sent[1].chgenable = 1;
									host_sent[1].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[1].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num 
																+ host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[1].Charger_good_num)/host_model_num[gun_temp];
									host_sent[1].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[1].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
									//为枪D的模块输出赋值
									host_sent[3].chgenable = 1;
									host_sent[3].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[3].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num 
																  + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[3].Charger_good_num)/host_model_num[gun_temp];
									host_sent[3].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[3].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
							}
						}
						else if((host_recv[2].host_flag == 0) && (1 == host_panduan(0,2)))
						{//可以并机
							if(host_recv[gun_temp].need_model_num > 
								(host_recv[gun_temp].Charger_good_num + 
									host_recv[1].Charger_good_num + 
									host_recv[2].Charger_good_num))
							{//在B、C枪并机之后还需要并机
								//if(host_recv[3].host_flag != 0)//判断D枪是否可以并机
								if(!((host_recv[3].host_flag == 0) && (1 == host_panduan(0,3))))
								{//枪D不可以并机，此时只有A、B、C三枪进行输出
									host_sent[3].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令	
									host_sent[2].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_congji[gun_temp][1] = 2;//标记枪B已经是枪的从机了。
									host_congji[gun_temp][2] = 2;//标记枪C已经是枪的从机了。
									host_sent[1].host_Relay_control = 1;//可以并机，闭合并机接触器
									host_sent[0].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪A的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num + host_recv[2].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪B的模块输出赋值
									host_sent[1].chgenable = 1;
									host_sent[1].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[1].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num + host_recv[2].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[1].Charger_good_num)/host_model_num[gun_temp];
									host_sent[1].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[1].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
									//为枪C的模块输出赋值
									host_sent[2].chgenable = 1;
									host_sent[2].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[2].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num + host_recv[2].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[2].Charger_good_num)/host_model_num[gun_temp];
									host_sent[2].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[2].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								}
								else if((host_recv[3].host_flag == 0) && (1 == host_panduan(0,3)))
								{//可以并机
									host_sent[3].host_Relay_control = 0;//将并机接触器断开；
									host_congji[gun_temp][1] = 2;//标记枪B已经是枪的从机了。
									host_congji[gun_temp][2] = 2;//标记枪C已经是枪的从机了。
									host_congji[gun_temp][3] = 2;//标记枪D已经是枪的从机了。
									host_sent[1].host_Relay_control = 1;//可以并机，闭合并机接触器
									host_sent[0].host_Relay_control = 1;//可以并机，闭合并机接触器
									host_sent[2].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪A的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num 
																+ host_recv[2].Charger_good_num + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪B的模块输出赋值
									host_sent[1].chgenable = 1;
									host_sent[1].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[1].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num 
																+ host_recv[2].Charger_good_num + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[1].Charger_good_num)/host_model_num[gun_temp];
									host_sent[1].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[1].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
									//为枪C的模块输出赋值
									host_sent[2].chgenable = 1;
									host_sent[2].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[2].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num 
																+ host_recv[2].Charger_good_num + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[2].Charger_good_num)/host_model_num[gun_temp];
									host_sent[2].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[2].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
									//为枪D的模块输出赋值
									host_sent[3].chgenable = 1;
									host_sent[3].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[3].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num 
																+ host_recv[2].Charger_good_num + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[3].Charger_good_num)/host_model_num[gun_temp];
									host_sent[3].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[3].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								}
							}
							else 
							{//不再需要了，此时将A、B、C的输出信息下发
								host_sent[2].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
								host_sent[3].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
								if(host_congji[gun_temp][3] == 2)
								{
									i = 3;
									host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
									//将开机标志清零；
									host_sent[i].chgenable = 0;
									//将模块输出清零；
									 host_sent[i].model_output_vol[0] = 0;
									 host_sent[i].model_output_vol[1] = 0;
									 host_sent[i].model_output_cur[0] = 0;
									 host_sent[i].model_output_cur[1] = 0; 
									 host_congji[gun_temp][i] = 0;
								}
								
								host_congji[gun_temp][1] = 2;//标记枪B已经是枪的从机了。
								host_congji[gun_temp][2] = 2;//标记枪C已经是枪的从机了。
								host_sent[1].host_Relay_control = 1;//可以并机，闭合并机接触器
								host_sent[0].host_Relay_control = 1;//可以并机，闭合并机接触器
								//为枪A的模块输出赋值
								host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
								host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
								host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
								model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
								host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num + host_recv[2].Charger_good_num;
								model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
								host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
								host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
								//为枪B的模块输出赋值
								host_sent[1].chgenable = 1;
								host_sent[1].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
								host_sent[1].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
								model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
								host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num + host_recv[2].Charger_good_num;
								model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[1].Charger_good_num)/host_model_num[gun_temp];
								host_sent[1].model_output_cur[0] = model_temp_a[gun_temp];
								host_sent[1].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								//为枪C的模块输出赋值
								host_sent[2].chgenable = 1;
								host_sent[2].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
								host_sent[2].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
								model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
								host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num + host_recv[2].Charger_good_num;
								model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[2].Charger_good_num)/host_model_num[gun_temp];
								host_sent[2].model_output_cur[0] = model_temp_a[gun_temp];
								host_sent[2].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
							}
							
						}
					}
					else
					{//将B枪并进去之后，不需要在并机了。
						host_sent[0].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
						host_sent[3].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
						
						host_congji[gun_temp][1] = 2;//标记枪B已经是枪的从机了。
						host_sent[1].host_Relay_control = 1;//可以并机，闭合并机接触器
						
						if(host_congji[gun_temp][3] == 2)
						{
							i = 3;
							host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
							//将开机标志清零；
							host_sent[i].chgenable = 0;
							//将模块输出清零；
							 host_sent[i].model_output_vol[0] = 0;
							 host_sent[i].model_output_vol[1] = 0;
							 host_sent[i].model_output_cur[0] = 0;
							 host_sent[i].model_output_cur[1] = 0; 
							 host_congji[gun_temp][i] = 0;
						}
						if(host_congji[gun_temp][2] == 2)
						{
							i = 2;
							host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
							//将开机标志清零；
							host_sent[i].chgenable = 0;
							//将模块输出清零；
							 host_sent[i].model_output_vol[0] = 0;
							 host_sent[i].model_output_vol[1] = 0;
							 host_sent[i].model_output_cur[0] = 0;
							 host_sent[i].model_output_cur[1] = 0; 
							 host_congji[gun_temp][i] = 0;
						}
						
						//为枪A的模块输出赋值
						host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
						host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
						host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
						model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
						host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num;
						model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
						host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
						host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
						//为枪B的模块输出赋值
						host_sent[1].chgenable = 1;
						host_sent[1].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
						host_sent[1].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
						model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
						host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num;
						model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[1].Charger_good_num)/host_model_num[gun_temp];
						host_sent[1].model_output_cur[0] = model_temp_a[gun_temp];
						host_sent[1].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
					}
						
				}
			}
			else
			{//该枪模块可以满足需求，不需要并机
			  //将需求电压、电流原值下发。
			  
			  if(host_congji[gun_temp][1] == 2)
				{
					i = 1;
					host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
					//将开机标志清零；
					host_sent[i].chgenable = 0;
					//将模块输出清零；
					 host_sent[i].model_output_vol[0] = 0;
					 host_sent[i].model_output_vol[1] = 0;
					 host_sent[i].model_output_cur[0] = 0;
					 host_sent[i].model_output_cur[1] = 0; 
					 host_congji[gun_temp][i] = 0;
				}
				if(host_congji[gun_temp][2] == 2)
				{
					i = 2;
					host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
					//将开机标志清零；
					host_sent[i].chgenable = 0;
					//将模块输出清零；
					 host_sent[i].model_output_vol[0] = 0;
					 host_sent[i].model_output_vol[1] = 0;
					 host_sent[i].model_output_cur[0] = 0;
					 host_sent[i].model_output_cur[1] = 0; 
					 host_congji[gun_temp][i] = 0;
				}
				if(host_congji[gun_temp][3] == 2)
				{
					i = 3;
					host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
					//将开机标志清零；
					host_sent[i].chgenable = 0;
					//将模块输出清零；
					 host_sent[i].model_output_vol[0] = 0;
					 host_sent[i].model_output_vol[1] = 0;
					 host_sent[i].model_output_cur[0] = 0;
					 host_sent[i].model_output_cur[1] = 0; 
					 host_congji[gun_temp][i] = 0;
				}
			  
			  host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
			  host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
			  host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
			  host_sent[gun_temp].model_output_cur[0] = host_recv[gun_temp].bmsma[0];
			  host_sent[gun_temp].model_output_cur[1] = host_recv[gun_temp].bmsma[1]; 
			  if(debug == 1)printf("host_sent[gun_temp].model_output_vol[0] = %x\n",host_sent[gun_temp].model_output_vol[0]);
			  if(debug == 1)printf("host_sent[gun_temp].model_output_vol[1] = %x\n",host_sent[gun_temp].model_output_vol[1]);
			  //断开并机接触器
			  host_sent[gun_temp].host_Relay_control = 0;
			  host_sent[1].host_Relay_control = 0;
			}
		}
		else//当枪A不需要并机了，将它的从机全部断开
		{
			if(host_congji[gun_temp][1] == 2)
			{
				i = 1;
				host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
				//将开机标志清零；
				host_sent[i].chgenable = 0;
				//将模块输出清零；
				 host_sent[i].model_output_vol[0] = 0;
				 host_sent[i].model_output_vol[1] = 0;
				 host_sent[i].model_output_cur[0] = 0;
				 host_sent[i].model_output_cur[1] = 0; 
				 host_congji[gun_temp][i] = 0;
			}
			if(host_congji[gun_temp][2] == 2)
			{
				i = 2;
				host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
				//将开机标志清零；
				host_sent[i].chgenable = 0;
				//将模块输出清零；
				 host_sent[i].model_output_vol[0] = 0;
				 host_sent[i].model_output_vol[1] = 0;
				 host_sent[i].model_output_cur[0] = 0;
				 host_sent[i].model_output_cur[1] = 0; 
				 host_congji[gun_temp][i] = 0;
			}
			if(host_congji[gun_temp][3] == 2)
			{
				i = 3;
				host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
				//将开机标志清零；
				host_sent[i].chgenable = 0;
				//将模块输出清零；
				 host_sent[i].model_output_vol[0] = 0;
				 host_sent[i].model_output_vol[1] = 0;
				 host_sent[i].model_output_cur[0] = 0;
				 host_sent[i].model_output_cur[1] = 0; 
				 host_congji[gun_temp][i] = 0;
			}
		
			if(host_recv[gun_temp].chgstart != 1)
			{
				i = gun_temp;
				host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
				//将开机标志清零；
				host_sent[i].chgenable = 0;
				//将模块输出清零；
				 host_sent[i].model_output_vol[0] = 0;
				 host_sent[i].model_output_vol[1] = 0;
				 host_sent[i].model_output_cur[0] = 0;
				 host_sent[i].model_output_cur[1] = 0; 
				 host_congji[gun_temp][i] = 0;
			}
		}
		
	}
	
	//当该枪中的某把枪不能并机了，将它的从机标志清掉
	for(i=0;i<Globa->Charger_param.gun_config;i++)
	{
		if(i != gun_temp)
			if(host_recv[i].host_flag != 0)
				if(host_congji[gun_temp][i] == 2)
					host_congji[gun_temp][i] = 0;
	}
	if(debug == 1)
		for(i=0;i<Globa->Charger_param.gun_config;i++)
		{
			printf("host_sent[%d].model_output_vol[0] = %d\n",i,host_sent[i].model_output_vol[0]);
			printf("host_sent[%d].model_output_vol[1] = %d\n",i,host_sent[i].model_output_vol[1]);
			printf("host_sent[%d].model_output_cur[0] = %d\n",i,host_sent[i].model_output_vol[0]);
			printf("host_sent[%d].model_output_cur[1] = %d\n",i,host_sent[i].model_output_vol[1]);
			printf("host_sent[%d].model_output_vol[0] = %d\n",i,host_sent[i].model_output_vol[0]);
			printf("host_sent[%d].chgenable = %d\n",i,host_sent[i].chgenable);
			printf("host_sent[%d].host_Relay_control = %d\n",i,host_sent[i].host_Relay_control);
			printf("\n\n");
		}
	
	//并机逻辑，枪B的处理	
	//B-A/D-C
	gun_temp = 1;
	if(host_recv_flag[gun_temp] == 1)//判断是否接收到了需求信息
	{
		host_recv_flag[gun_temp] = 0;
		if(host_recv[gun_temp].gunstate == 1 && host_recv[gun_temp].chgstart == 1 && host_recv[gun_temp].host_flag == 1)
		{//枪插着，并且开机了，说明该枪需要进行模块输出处理
			if(host_recv[gun_temp].need_model_num > host_recv[gun_temp].Charger_good_num)
			{//此时需要并机,需要进行下一步判断
				//if(host_recv[0].host_flag != 0)//判断A枪是否可以并机
				if(!((host_recv[0].host_flag == 0) && (1 == host_panduan(1,0))))
				{//不能并机
					host_sent[1].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
						//if(host_recv[3].host_flag != 0)//判断D枪是否可以并机
						if((!((host_recv[3].host_flag == 0) && (1 == host_panduan(1,3)))) && Globa->Charger_param.gun_config == 4)
						{//不能并机
							//当A、D枪都不能并机时，枪B不能并机。
							host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
						    host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
						    host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
						    host_sent[gun_temp].model_output_cur[0] = host_recv[gun_temp].bmsma[0];
						    host_sent[gun_temp].model_output_cur[1] = host_recv[gun_temp].bmsma[1]; 
						   //断开并机接触器
						    host_sent[gun_temp].host_Relay_control = 0;
							host_sent[3].host_Relay_control = 0;
						}	
						else if((!((host_recv[2].host_flag == 0) && (1 == host_panduan(1,2)))) && Globa->Charger_param.gun_config == 3)
						{//不能并机
							//当A、D枪都不能并机时，枪B不能并机。
							host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
						    host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
						    host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
						    host_sent[gun_temp].model_output_cur[0] = host_recv[gun_temp].bmsma[0];
						    host_sent[gun_temp].model_output_cur[1] = host_recv[gun_temp].bmsma[1]; 
						   //断开并机接触器
						    host_sent[gun_temp].host_Relay_control = 0;
							host_sent[3].host_Relay_control = 0;
						}
						/* #ifdef THREE_GUN
						else if((host_recv[3].host_flag == 0) && (1 == host_panduan(1,3)))
						#endif
						else if((host_recv[2].host_flag == 0) && (1 == host_panduan(1,2))) */
						else	
						{//可以并机
							if(host_recv[gun_temp].need_model_num > 
								(host_recv[gun_temp].Charger_good_num + 
									host_recv[3].Charger_good_num))
							{//在D枪并机之后还需要并机
								//if(host_recv[2].host_flag != 0)//判断C枪是否可以并机
								if(!((host_recv[2].host_flag == 0) && (1 == host_panduan(1,2))))
								{//枪C不可以并机，此时只有B、D枪进行输出
									host_sent[2].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_sent[1].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
								
									host_congji[gun_temp][3] = 2;//标记枪D已经是枪的从机了。
									host_sent[3].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪B的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪D的模块输出赋值
									host_sent[3].chgenable = 1;
									host_sent[3].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[3].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num +  host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[3].Charger_good_num)/host_model_num[gun_temp];
									host_sent[3].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[3].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								}
								else if((host_recv[2].host_flag == 0) && (1 == host_panduan(1,2)))
								{//可以并机
									host_sent[1].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_sent[0].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									
									host_congji[gun_temp][2] = 2;//标记枪C已经是枪的从机了。
									host_congji[gun_temp][3] = 2;//标记枪D已经是枪的从机了。
									host_sent[3].host_Relay_control = 1;//可以并机，闭合并机接触器
									host_sent[2].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪B的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num 
																+ host_recv[2].Charger_good_num + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
									//为枪C的模块输出赋值
									host_sent[2].chgenable = 1;
									host_sent[2].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[2].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num 
																+ host_recv[2].Charger_good_num + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[2].Charger_good_num)/host_model_num[gun_temp];
									host_sent[2].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[2].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
									//为枪D的模块输出赋值
									host_sent[3].chgenable = 1;
									host_sent[3].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[3].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num 
																+ host_recv[2].Charger_good_num + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[3].Charger_good_num)/host_model_num[gun_temp];
									host_sent[3].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[3].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								}
							}
							else 
							{//不再需要了，此时将B、D的输出信息下发
									host_sent[2].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
									host_sent[1].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
									if(host_congji[gun_temp][0] == 2)
									{
										i = 0;
										host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
										//将开机标志清零；
										host_sent[i].chgenable = 0;
										//将模块输出清零；
										 host_sent[i].model_output_vol[0] = 0;
										 host_sent[i].model_output_vol[1] = 0;
										 host_sent[i].model_output_cur[0] = 0;
										 host_sent[i].model_output_cur[1] = 0; 
										 host_congji[gun_temp][i] = 0;
									}
									if(host_congji[gun_temp][2] == 2)
									{
										i = 2;
										host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
										//将开机标志清零；
										host_sent[i].chgenable = 0;
										//将模块输出清零；
										 host_sent[i].model_output_vol[0] = 0;
										 host_sent[i].model_output_vol[1] = 0;
										 host_sent[i].model_output_cur[0] = 0;
										 host_sent[i].model_output_cur[1] = 0; 
										 host_congji[gun_temp][i] = 0;
									}
									
									host_congji[gun_temp][3] = 2;//标记枪D已经是枪的从机了。
									host_sent[3].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪B的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪D的模块输出赋值
									host_sent[3].chgenable = 1;
									host_sent[3].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[3].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num +  host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[3].Charger_good_num)/host_model_num[gun_temp];
									host_sent[3].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[3].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
							}
							
						}
										
				}
				else if((host_recv[0].host_flag == 0) && (1 == host_panduan(1,0)))
				{//可以并机
					if(host_recv[gun_temp].need_model_num > 
						(host_recv[gun_temp].Charger_good_num + host_recv[0].Charger_good_num))
					{//将A枪并进去后，还需要进行并机
						//if(host_recv[2].host_flag != 0)//判断C枪是否可以并机
						if(!((host_recv[2].host_flag == 0) && (1 == host_panduan(1,2))))
						{//不能并机
							//host_sent[2].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
							//if(host_recv[3].host_flag != 0)//判断D枪是否可以并机
							if((!((host_recv[3].host_flag == 0) && (1 == host_panduan(1,3)))) && Globa->Charger_param.gun_config == 4) 
							{//不能并机 此时C、D枪都不能并机，只有B枪可以并机。
									host_sent[3].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_sent[0].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_congji[gun_temp][0] = 2;//标记枪B已经是枪的从机了。
									host_sent[1].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪B的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[0].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪A的模块输出赋值
									host_sent[0].chgenable = 1;
									host_sent[0].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[0].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[0].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[0].Charger_good_num)/host_model_num[gun_temp];
									host_sent[0].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[0].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								}	
							else if((!((host_recv[2].host_flag == 0) && (1 == host_panduan(1,2)))) && Globa->Charger_param.gun_config == 3) 
								{//不能并机 此时C、D枪都不能并机，只有B枪可以并机。
									host_sent[3].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_sent[0].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_congji[gun_temp][0] = 2;//标记枪B已经是枪的从机了。
									host_sent[1].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪B的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[0].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪A的模块输出赋值
									host_sent[0].chgenable = 1;
									host_sent[0].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[0].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[0].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[0].Charger_good_num)/host_model_num[gun_temp];
									host_sent[0].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[0].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								}
							/* #ifdef THREE_GUN
							else if((host_recv[3].host_flag == 0) && (1 == host_panduan(1,3)))
							#endif
							else if((host_recv[2].host_flag == 0) && (1 == host_panduan(1,2))) */
							else
							{//可以并机
								//此时A、B、D可以并机
									host_sent[0].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_sent[2].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_congji[gun_temp][0] = 2;//标记枪A已经是枪的从机了。
									host_congji[gun_temp][3] = 2;//标记枪D已经是枪的从机了。
									host_sent[1].host_Relay_control = 1;//可以并机，闭合并机接触器
									host_sent[3].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪B的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[0].Charger_good_num 
															   + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪A的模块输出赋值
									host_sent[0].chgenable = 1;
									host_sent[0].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[0].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[0].Charger_good_num 
																+ host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[0].Charger_good_num)/host_model_num[gun_temp];
									host_sent[0].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[0].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
									//为枪D的模块输出赋值
									host_sent[3].chgenable = 1;
									host_sent[3].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[3].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[0].Charger_good_num 
																  + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[3].Charger_good_num)/host_model_num[gun_temp];
									host_sent[3].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[3].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
							}
						}
						else if((host_recv[2].host_flag == 0) && (1 == host_panduan(1,2)))
						{//可以并机
							if(host_recv[gun_temp].need_model_num > 
								(host_recv[gun_temp].Charger_good_num + 
									host_recv[0].Charger_good_num + 
									host_recv[2].Charger_good_num))
							{//在A、C枪并机之后还需要并机
								//if(host_recv[3].host_flag != 0)//判断D枪是否可以并机
								if((!((host_recv[3].host_flag == 0) && (1 == host_panduan(1,3)))) && Globa->Charger_param.gun_config == 4)
								{//枪D不可以并机，此时只有A、B、C三枪进行输出
									host_sent[3].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_sent[2].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
								
									host_congji[gun_temp][0] = 2;//标记枪A已经是枪的从机了。
									host_congji[gun_temp][2] = 2;//标记枪C已经是枪的从机了。
									host_sent[1].host_Relay_control = 1;//可以并机，闭合并机接触器
									host_sent[0].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪B的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[0].Charger_good_num + host_recv[2].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪A的模块输出赋值
									host_sent[0].chgenable = 1;
									host_sent[0].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[0].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num + host_recv[2].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[0].Charger_good_num)/host_model_num[gun_temp];
									host_sent[0].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[0].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
									//为枪C的模块输出赋值
									host_sent[2].chgenable = 1;
									host_sent[2].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[2].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num + host_recv[2].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[2].Charger_good_num)/host_model_num[gun_temp];
									host_sent[2].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[2].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								}	
								else if((!((host_recv[2].host_flag == 0) && (1 == host_panduan(1,2)))) && Globa->Charger_param.gun_config == 3) 
								{//枪D不可以并机，此时只有A、B、C三枪进行输出
									host_sent[3].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_sent[2].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
								
									host_congji[gun_temp][0] = 2;//标记枪A已经是枪的从机了。
									host_congji[gun_temp][2] = 2;//标记枪C已经是枪的从机了。
									host_sent[1].host_Relay_control = 1;//可以并机，闭合并机接触器
									host_sent[0].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪B的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[0].Charger_good_num + host_recv[2].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪A的模块输出赋值
									host_sent[0].chgenable = 1;
									host_sent[0].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[0].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num + host_recv[2].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[0].Charger_good_num)/host_model_num[gun_temp];
									host_sent[0].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[0].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
									//为枪C的模块输出赋值
									host_sent[2].chgenable = 1;
									host_sent[2].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[2].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num + host_recv[2].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[2].Charger_good_num)/host_model_num[gun_temp];
									host_sent[2].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[2].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								}
								/* #ifdef THREE_GUN
								else if((host_recv[3].host_flag == 0) && (1 == host_panduan(1,3)))
								#endif
								else if((host_recv[2].host_flag == 0) && (1 == host_panduan(1,2))) */
								else
								{//可以并机
									host_sent[3].host_Relay_control = 0;//下发并机接触器断开指令
									host_congji[gun_temp][0] = 2;//标记枪B已经是枪的从机了。
									host_congji[gun_temp][2] = 2;//标记枪C已经是枪的从机了。
									host_congji[gun_temp][3] = 2;//标记枪D已经是枪的从机了。
									host_sent[1].host_Relay_control = 1;//可以并机，闭合并机接触器
									host_sent[0].host_Relay_control = 1;//可以并机，闭合并机接触器
									host_sent[2].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪B的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[0].Charger_good_num 
																+ host_recv[2].Charger_good_num + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪A的模块输出赋值
									host_sent[0].chgenable = 1;
									host_sent[0].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[0].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num 
																+ host_recv[2].Charger_good_num + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[0].Charger_good_num)/host_model_num[gun_temp];
									host_sent[0].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[0].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
									//为枪C的模块输出赋值
									host_sent[2].chgenable = 1;
									host_sent[2].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[2].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num 
																+ host_recv[2].Charger_good_num + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[2].Charger_good_num)/host_model_num[gun_temp];
									host_sent[2].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[2].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
									//为枪D的模块输出赋值
									host_sent[3].chgenable = 1;
									host_sent[3].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[3].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num 
																+ host_recv[2].Charger_good_num + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[3].Charger_good_num)/host_model_num[gun_temp];
									host_sent[3].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[3].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								}
							}
							else 
							{//不再需要了，此时将A、B、C的输出信息下发
								host_sent[3].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
								host_sent[2].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
								if(host_congji[gun_temp][3] == 2)
								{
									i = 3;
									host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
									//将开机标志清零；
									host_sent[i].chgenable = 0;
									//将模块输出清零；
									 host_sent[i].model_output_vol[0] = 0;
									 host_sent[i].model_output_vol[1] = 0;
									 host_sent[i].model_output_cur[0] = 0;
									 host_sent[i].model_output_cur[1] = 0; 
									 host_congji[gun_temp][i] = 0;
								}
						
								host_congji[gun_temp][0] = 2;//标记枪A已经是枪的从机了。
								host_congji[gun_temp][2] = 2;//标记枪C已经是枪的从机了。
								host_sent[1].host_Relay_control = 1;//可以并机，闭合并机接触器
								host_sent[0].host_Relay_control = 1;//可以并机，闭合并机接触器
								//为枪B的模块输出赋值
								host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
								host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
								host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
								model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
								host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[0].Charger_good_num + host_recv[2].Charger_good_num;
								model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
								host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
								host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
								//为枪A的模块输出赋值
								host_sent[0].chgenable = 1;
								host_sent[0].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
								host_sent[0].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
								model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
								host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[0].Charger_good_num + host_recv[2].Charger_good_num;
								model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[0].Charger_good_num)/host_model_num[gun_temp];
								host_sent[0].model_output_cur[0] = model_temp_a[gun_temp];
								host_sent[0].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								//为枪C的模块输出赋值
								host_sent[2].chgenable = 1;
								host_sent[2].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
								host_sent[2].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
								model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
								host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[0].Charger_good_num + host_recv[2].Charger_good_num;
								model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[2].Charger_good_num)/host_model_num[gun_temp];
								host_sent[2].model_output_cur[0] = model_temp_a[gun_temp];
								host_sent[2].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
							}
							
						}
					}
					else
					{//将A枪并进去之后，不需要在并机了。
						host_sent[0].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
						host_sent[3].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
						if(host_congji[gun_temp][2] == 2)
						{
							i = 2;
							host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
							//将开机标志清零；
							host_sent[i].chgenable = 0;
							//将模块输出清零；
							 host_sent[i].model_output_vol[0] = 0;
							 host_sent[i].model_output_vol[1] = 0;
							 host_sent[i].model_output_cur[0] = 0;
							 host_sent[i].model_output_cur[1] = 0; 
							 host_congji[gun_temp][i] = 0;
						}
						if(host_congji[gun_temp][3] == 2)
						{
							i = 3;
							host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
							//将开机标志清零；
							host_sent[i].chgenable = 0;
							//将模块输出清零；
							 host_sent[i].model_output_vol[0] = 0;
							 host_sent[i].model_output_vol[1] = 0;
							 host_sent[i].model_output_cur[0] = 0;
							 host_sent[i].model_output_cur[1] = 0; 
							 host_congji[gun_temp][i] = 0;
						}
						
						host_congji[gun_temp][0] = 2;//标记枪A已经是枪的从机了。
						host_sent[1].host_Relay_control = 1;//可以并机，闭合并机接触器
						//为枪B的模块输出赋值
						host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
						host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
						host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
						model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
						host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[0].Charger_good_num;
						model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
						host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
						host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
						//为枪A的模块输出赋值
						host_sent[0].chgenable = 1;
						host_sent[0].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
						host_sent[0].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
						model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
						host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num;
						model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[0].Charger_good_num)/host_model_num[gun_temp];
						host_sent[0].model_output_cur[0] = model_temp_a[gun_temp];
						host_sent[0].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
					}
						
				}
			}
			else
			{//该枪模块可以满足需求，不需要并机
			  //将需求电压、电流原值下发。
			  
			  if(host_congji[gun_temp][0] == 2)
				{
					i = 0;
					host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
					//将开机标志清零；
					host_sent[i].chgenable = 0;
					//将模块输出清零；
					 host_sent[i].model_output_vol[0] = 0;
					 host_sent[i].model_output_vol[1] = 0;
					 host_sent[i].model_output_cur[0] = 0;
					 host_sent[i].model_output_cur[1] = 0; 
					 host_congji[gun_temp][i] = 0;
				}
				if(host_congji[gun_temp][2] == 2)
				{
					i = 2;
					host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
					//将开机标志清零；
					host_sent[i].chgenable = 0;
					//将模块输出清零；
					 host_sent[i].model_output_vol[0] = 0;
					 host_sent[i].model_output_vol[1] = 0;
					 host_sent[i].model_output_cur[0] = 0;
					 host_sent[i].model_output_cur[1] = 0; 
					 host_congji[gun_temp][i] = 0;
				}
				if(host_congji[gun_temp][3] == 2)
				{
					i = 3;
					host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
					//将开机标志清零；
					host_sent[i].chgenable = 0;
					//将模块输出清零；
					 host_sent[i].model_output_vol[0] = 0;
					 host_sent[i].model_output_vol[1] = 0;
					 host_sent[i].model_output_cur[0] = 0;
					 host_sent[i].model_output_cur[1] = 0; 
					 host_congji[gun_temp][i] = 0;
				}
			  
			  host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
			  host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
			  host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
			  host_sent[gun_temp].model_output_cur[0] = host_recv[gun_temp].bmsma[0];
			  host_sent[gun_temp].model_output_cur[1] = host_recv[gun_temp].bmsma[1]; 
			  if(debug == 1)printf("host_sent[1].model_output_vol[0] = %x\n",host_sent[gun_temp].model_output_vol[0]);
			  if(debug == 1)printf("host_sent[1].model_output_vol[1] = %x\n",host_sent[gun_temp].model_output_vol[1]);
			  //断开并机接触器
			  host_sent[gun_temp].host_Relay_control = 0;
			  host_sent[3].host_Relay_control = 0;
			}
		}
		else//当枪B不需要并机了，将它的从机全部断开
		{
			if(host_congji[gun_temp][0] == 2)
			{
				i = 0;
				host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
				//将开机标志清零；
				host_sent[i].chgenable = 0;
				//将模块输出清零；
				 host_sent[i].model_output_vol[0] = 0;
				 host_sent[i].model_output_vol[1] = 0;
				 host_sent[i].model_output_cur[0] = 0;
				 host_sent[i].model_output_cur[1] = 0; 
				 host_congji[gun_temp][i] = 0;
			}
			if(host_congji[gun_temp][2] == 2)
			{
				i = 2;
				host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
				//将开机标志清零；
				host_sent[i].chgenable = 0;
				//将模块输出清零；
				 host_sent[i].model_output_vol[0] = 0;
				 host_sent[i].model_output_vol[1] = 0;
				 host_sent[i].model_output_cur[0] = 0;
				 host_sent[i].model_output_cur[1] = 0; 
				 host_congji[gun_temp][i] = 0;
			}
			if(host_congji[gun_temp][3] == 2)
			{
				i = 3;
				host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
				//将开机标志清零；
				host_sent[i].chgenable = 0;
				//将模块输出清零；
				 host_sent[i].model_output_vol[0] = 0;
				 host_sent[i].model_output_vol[1] = 0;
				 host_sent[i].model_output_cur[0] = 0;
				 host_sent[i].model_output_cur[1] = 0; 
				 host_congji[gun_temp][i] = 0;
			}
			
			if(host_recv[gun_temp].chgstart != 1)
			{
				i = gun_temp;
				host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
				//将开机标志清零；
				host_sent[i].chgenable = 0;
				//将模块输出清零；
				 host_sent[i].model_output_vol[0] = 0;
				 host_sent[i].model_output_vol[1] = 0;
				 host_sent[i].model_output_cur[0] = 0;
				 host_sent[i].model_output_cur[1] = 0; 
				 host_congji[gun_temp][i] = 0;
			}
		}
		
	}
	
	//当该枪中的某把枪不能并机了，将它的从机标志清掉
	for(i=0;i<Globa->Charger_param.gun_config;i++)
	{
		if(i != gun_temp)
			if(host_recv[i].host_flag != 0)
				if(host_congji[gun_temp][i] == 2)
					host_congji[gun_temp][i] = 0;
	}
	if(debug == 1)
		for(i=0;i<Globa->Charger_param.gun_config;i++)
		{
			printf("host_sent[%d].model_output_vol[0] = %d\n",i,host_sent[i].model_output_vol[0]);
			printf("host_sent[%d].model_output_vol[1] = %d\n",i,host_sent[i].model_output_vol[1]);
			printf("host_sent[%d].model_output_cur[0] = %d\n",i,host_sent[i].model_output_vol[0]);
			printf("host_sent[%d].model_output_cur[1] = %d\n",i,host_sent[i].model_output_vol[1]);
			printf("host_sent[%d].model_output_vol[0] = %d\n",i,host_sent[i].model_output_vol[0]);
			printf("host_sent[%d].chgenable = %d\n",i,host_sent[i].chgenable);
			printf("host_sent[%d].host_Relay_control = %d\n",i,host_sent[i].host_Relay_control);
			printf("\n\n");
		}
	
	//并机逻辑，枪C的处理
//C-A/D-B	
	gun_temp = 2;
	if(host_recv_flag[gun_temp] == 1)
	{
		host_recv_flag[gun_temp] = 0;
		if(host_recv[gun_temp].gunstate == 1 && host_recv[gun_temp].chgstart == 1 && host_recv[gun_temp].host_flag == 1)
		{//枪插着，并且开机了，说明该枪需要进行模块输出处理
			if(host_recv[gun_temp].need_model_num > host_recv[gun_temp].Charger_good_num)
			{//此时需要并机,需要进行下一步判断
				//if(host_recv[0].host_flag != 0)//判断A枪是否可以并机
				if(!((host_recv[0].host_flag == 0) && (1 == host_panduan(2,0))))
				{//不能并机
					host_sent[0].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
						//if(host_recv[3].host_flag != 0)//判断D枪是否可以并机
					if((!((host_recv[3].host_flag == 0) && (1 == host_panduan(2,3)))) && Globa->Charger_param.gun_config == 4)
					{//不能并机
							//当D、A枪都不能并机时，枪C不能并机。
							host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
						    host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
						    host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
						    host_sent[gun_temp].model_output_cur[0] = host_recv[gun_temp].bmsma[0];
						    host_sent[gun_temp].model_output_cur[1] = host_recv[gun_temp].bmsma[1]; 
						   //断开并机接触器
						    host_sent[gun_temp].host_Relay_control = 0;
							host_sent[0].host_Relay_control = 0;
					}
					else if((!((host_recv[1].host_flag == 0) && (1 == host_panduan(2,1)))) && Globa->Charger_param.gun_config == 3)	
						{//不能并机
							//当D、A枪都不能并机时，枪C不能并机。
							host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
						    host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
						    host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
						    host_sent[gun_temp].model_output_cur[0] = host_recv[gun_temp].bmsma[0];
						    host_sent[gun_temp].model_output_cur[1] = host_recv[gun_temp].bmsma[1]; 
						   //断开并机接触器
						    host_sent[gun_temp].host_Relay_control = 0;
							host_sent[0].host_Relay_control = 0;
						}
						/* #ifdef THREE_GUN
							else if((host_recv[3].host_flag == 0) && (1 == host_panduan(2,3)))
						#endif
							else if((host_recv[1].host_flag == 0) && (1 == host_panduan(2,1))) */
					else
						{//可以并机
							if(host_recv[gun_temp].need_model_num > 
								(host_recv[gun_temp].Charger_good_num + 
									host_recv[3].Charger_good_num))
							{//在D枪并机之后还需要并机
								//if(host_recv[1].host_flag != 0)//判断B枪是否可以并机
								if(!((host_recv[1].host_flag == 0) && (1 == host_panduan(2,1))))
								{//枪B不可以并机，此时只有C、D枪进行输出
									host_sent[0].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_sent[3].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_congji[gun_temp][3] = 2;//标记枪D已经是枪的从机了。
									host_sent[2].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪C的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪D的模块输出赋值
									host_sent[3].chgenable = 1;
									host_sent[3].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[3].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num +  host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[3].Charger_good_num)/host_model_num[gun_temp];
									host_sent[3].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[3].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								}
								else if((host_recv[1].host_flag == 0) && (1 == host_panduan(2,1)))
								{//可以并机
									host_sent[1].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_sent[0].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_congji[gun_temp][1] = 2;//标记枪B已经是枪的从机了。
									host_congji[gun_temp][3] = 2;//标记枪D已经是枪的从机了。
									host_sent[3].host_Relay_control = 1;//可以并机，闭合并机接触器
									host_sent[2].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪C的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num 
																+ host_recv[1].Charger_good_num + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
									//为枪B的模块输出赋值
									host_sent[1].chgenable = 1;
									host_sent[1].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[1].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num 
																+ host_recv[1].Charger_good_num + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[1].Charger_good_num)/host_model_num[gun_temp];
									host_sent[1].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[1].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
									//为枪D的模块输出赋值
									host_sent[3].chgenable = 1;
									host_sent[3].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[3].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num 
																+ host_recv[1].Charger_good_num + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[3].Charger_good_num)/host_model_num[gun_temp];
									host_sent[3].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[3].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								}
							}
							else 
							{//不再需要了，此时将D、C的输出信息下发
									host_sent[0].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
									host_sent[3].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
									if(host_congji[gun_temp][0] == 2)
									{
										i = 0;
										host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
										//将开机标志清零；
										host_sent[i].chgenable = 0;
										//将模块输出清零；
										 host_sent[i].model_output_vol[0] = 0;
										 host_sent[i].model_output_vol[1] = 0;
										 host_sent[i].model_output_cur[0] = 0;
										 host_sent[i].model_output_cur[1] = 0; 
										 host_congji[gun_temp][i] = 0;
									}
									if(host_congji[gun_temp][1] == 2)
									{
										i = 1;
										host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
										//将开机标志清零；
										host_sent[i].chgenable = 0;
										//将模块输出清零；
										 host_sent[i].model_output_vol[0] = 0;
										 host_sent[i].model_output_vol[1] = 0;
										 host_sent[i].model_output_cur[0] = 0;
										 host_sent[i].model_output_cur[1] = 0; 
										 host_congji[gun_temp][i] = 0;
									}
									host_congji[gun_temp][3] = 2;//标记枪D已经是枪的从机了。
									host_sent[2].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为C的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪D的模块输出赋值
									host_sent[3].chgenable = 1;
									host_sent[3].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[3].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num +  host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[3].Charger_good_num)/host_model_num[gun_temp];
									host_sent[3].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[3].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
							}
							
						}
										
				}
				else if((host_recv[0].host_flag == 0) && (1 == host_panduan(2,0)))
				{//可以并机
					if(host_recv[gun_temp].need_model_num > 
						(host_recv[gun_temp].Charger_good_num + host_recv[0].Charger_good_num))
					{//将A枪并进去后，还需要进行并机
						//if(host_recv[1].host_flag != 0)//判断B枪是否可以并机
						if(!((host_recv[1].host_flag == 0) && (1 == host_panduan(2,1))))
						{//不能并机
							host_sent[1].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
							//if(host_recv[3].host_flag != 0)//判断D枪是否可以并机
							if((!((host_recv[3].host_flag == 0) && (1 == host_panduan(2,3)))) && Globa->Charger_param.gun_config == 4)
							{//不能并机 此时B、D枪都不能并机，只有A枪可以并机。
									host_sent[2].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_sent[1].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									
									host_congji[gun_temp][0] = 2;//标记枪A已经是枪的从机了。
									host_sent[0].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪C的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[0].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪A的模块输出赋值
									host_sent[0].chgenable = 1;
									host_sent[0].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[0].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[0].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[0].Charger_good_num)/host_model_num[gun_temp];
									host_sent[0].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[0].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								}
							else if((!((host_recv[1].host_flag == 0) && (1 == host_panduan(2,1)))) && Globa->Charger_param.gun_config == 3)
								{//不能并机 此时B、D枪都不能并机，只有A枪可以并机。
									host_sent[2].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_sent[1].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									
									host_congji[gun_temp][0] = 2;//标记枪A已经是枪的从机了。
									host_sent[0].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪C的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[0].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪A的模块输出赋值
									host_sent[0].chgenable = 1;
									host_sent[0].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[0].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[0].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[0].Charger_good_num)/host_model_num[gun_temp];
									host_sent[0].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[0].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								}
							/* #if THREE_GUN
							else if((host_recv[3].host_flag == 0) && (1 == host_panduan(2,3)))
							#endif
							else if((host_recv[1].host_flag == 0) && (1 == host_panduan(2,1))) */
							else
							{//可以并机
								//此时A、C、D可以并机
									host_sent[3].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_sent[1].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_congji[gun_temp][0] = 2;//标记枪A已经是枪的从机了。
									host_congji[gun_temp][3] = 2;//标记枪D已经是枪的从机了。
									host_sent[0].host_Relay_control = 1;//可以并机，闭合并机接触器
									host_sent[2].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪C的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[0].Charger_good_num 
															   + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪A的模块输出赋值
									host_sent[0].chgenable = 1;
									host_sent[0].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[0].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[0].Charger_good_num 
																+ host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[0].Charger_good_num)/host_model_num[gun_temp];
									host_sent[0].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[0].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
									//为枪D的模块输出赋值
									host_sent[3].chgenable = 1;
									host_sent[3].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[3].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[0].Charger_good_num 
																  + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[3].Charger_good_num)/host_model_num[gun_temp];
									host_sent[3].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[3].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
							}
						}
						else if((host_recv[1].host_flag == 0) && (1 == host_panduan(2,1)))
						{//可以并机
							if(host_recv[gun_temp].need_model_num > 
								(host_recv[gun_temp].Charger_good_num + 
									host_recv[1].Charger_good_num + 
									host_recv[0].Charger_good_num))
							{//在A、B枪并机之后还需要并机
								//if(host_recv[3].host_flag != 0)//判断D枪是否可以并机
							if((!((host_recv[3].host_flag == 0) && (1 == host_panduan(2,3)))) && Globa->Charger_param.gun_config == 4)
							{//枪D不可以并机，此时只有A、B、C三枪进行输出
									host_sent[3].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_sent[2].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_congji[gun_temp][1] = 2;//标记枪B已经是枪的从机了。
									host_congji[gun_temp][0] = 2;//标记枪A已经是枪的从机了。
									host_sent[1].host_Relay_control = 1;//可以并机，闭合并机接触器
									host_sent[0].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪C的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num + host_recv[0].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪B的模块输出赋值
									host_sent[1].chgenable = 1;
									host_sent[1].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[1].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num + host_recv[0].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[1].Charger_good_num)/host_model_num[gun_temp];
									host_sent[1].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[1].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
									//为枪A的模块输出赋值
									host_sent[0].chgenable = 1;
									host_sent[0].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[0].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num + host_recv[0].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[0].Charger_good_num)/host_model_num[gun_temp];
									host_sent[0].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[0].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								}
							else if((!((host_recv[1].host_flag == 0) && (1 == host_panduan(2,1)))) && Globa->Charger_param.gun_config == 3)
								{//枪D不可以并机，此时只有A、B、C三枪进行输出
									host_sent[3].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_sent[2].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_congji[gun_temp][1] = 2;//标记枪B已经是枪的从机了。
									host_congji[gun_temp][0] = 2;//标记枪A已经是枪的从机了。
									host_sent[1].host_Relay_control = 1;//可以并机，闭合并机接触器
									host_sent[0].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪C的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num + host_recv[0].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪B的模块输出赋值
									host_sent[1].chgenable = 1;
									host_sent[1].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[1].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num + host_recv[0].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[1].Charger_good_num)/host_model_num[gun_temp];
									host_sent[1].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[1].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
									//为枪A的模块输出赋值
									host_sent[0].chgenable = 1;
									host_sent[0].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[0].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num + host_recv[0].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[0].Charger_good_num)/host_model_num[gun_temp];
									host_sent[0].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[0].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								}
								/* #ifdef THREE_GUN
								else if((host_recv[3].host_flag == 0) && (1 == host_panduan(2,3)))
								#endif
								else if((host_recv[1].host_flag == 0) && (1 == host_panduan(2,1))) */
								else
								{//可以并机
									host_sent[3].host_Relay_control = 0;//下发并机接触器断开指令
									host_congji[gun_temp][1] = 2;//标记枪B已经是枪的从机了。
									host_congji[gun_temp][0] = 2;//标记枪A已经是枪的从机了。
									host_congji[gun_temp][3] = 2;//标记枪D已经是枪的从机了。
									host_sent[1].host_Relay_control = 1;//可以并机，闭合并机接触器
									host_sent[0].host_Relay_control = 1;//可以并机，闭合并机接触器
									host_sent[2].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪C的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num 
																+ host_recv[0].Charger_good_num + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪B的模块输出赋值
									host_sent[1].chgenable = 1;
									host_sent[1].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[1].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num 
																+ host_recv[0].Charger_good_num + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[1].Charger_good_num)/host_model_num[gun_temp];
									host_sent[1].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[1].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
									//为枪A的模块输出赋值
									host_sent[0].chgenable = 1;
									host_sent[0].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[0].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num 
																+ host_recv[0].Charger_good_num + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[0].Charger_good_num)/host_model_num[gun_temp];
									host_sent[0].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[0].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
									//为枪D的模块输出赋值
									host_sent[3].chgenable = 1;
									host_sent[3].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[3].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num 
																+ host_recv[0].Charger_good_num + host_recv[3].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[3].Charger_good_num)/host_model_num[gun_temp];
									host_sent[3].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[3].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								}
							}
							else 
							{//不再需要了，此时将A、B、C的输出信息下发
								host_sent[2].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
								host_sent[3].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
								if(host_congji[gun_temp][3] == 2)
								{
									i = 3;
									host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
									//将开机标志清零；
									host_sent[i].chgenable = 0;
									//将模块输出清零；
									 host_sent[i].model_output_vol[0] = 0;
									 host_sent[i].model_output_vol[1] = 0;
									 host_sent[i].model_output_cur[0] = 0;
									 host_sent[i].model_output_cur[1] = 0; 
									 host_congji[gun_temp][i] = 0;
								}
								host_congji[gun_temp][1] = 2;//标记枪B已经是枪的从机了。
								host_congji[gun_temp][0] = 2;//标记枪A已经是枪的从机了。
								host_sent[1].host_Relay_control = 1;//可以并机，闭合并机接触器
								host_sent[0].host_Relay_control = 1;//可以并机，闭合并机接触器
								//为枪C的模块输出赋值
								host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
								host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
								host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
								model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
								host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num + host_recv[0].Charger_good_num;
								model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
								host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
								host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
								//为枪B的模块输出赋值
								host_sent[1].chgenable = 1;
								host_sent[1].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
								host_sent[1].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
								model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
								host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num + host_recv[0].Charger_good_num;
								model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[1].Charger_good_num)/host_model_num[gun_temp];
								host_sent[1].model_output_cur[0] = model_temp_a[gun_temp];
								host_sent[1].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								//为枪A的模块输出赋值
								host_sent[0].chgenable = 1;
								host_sent[0].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
								host_sent[0].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
								model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
								host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num + host_recv[0].Charger_good_num;
								model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[0].Charger_good_num)/host_model_num[gun_temp];
								host_sent[0].model_output_cur[0] = model_temp_a[gun_temp];
								host_sent[0].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
							}
							
						}
					}
					else
					{//将A枪并进去之后，不需要在并机了。
						host_sent[1].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
						host_sent[2].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
						if(host_congji[gun_temp][1] == 2)
						{
							i = 1;
							host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
							//将开机标志清零；
							host_sent[i].chgenable = 0;
							//将模块输出清零；
							 host_sent[i].model_output_vol[0] = 0;
							 host_sent[i].model_output_vol[1] = 0;
							 host_sent[i].model_output_cur[0] = 0;
							 host_sent[i].model_output_cur[1] = 0; 
							 host_congji[gun_temp][i] = 0;
						}
						if(host_congji[gun_temp][3] == 2)
						{
							i = 3;
							host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
							//将开机标志清零；
							host_sent[i].chgenable = 0;
							//将模块输出清零；
							 host_sent[i].model_output_vol[0] = 0;
							 host_sent[i].model_output_vol[1] = 0;
							 host_sent[i].model_output_cur[0] = 0;
							 host_sent[i].model_output_cur[1] = 0; 
							 host_congji[gun_temp][i] = 0;
						}
						host_congji[gun_temp][0] = 2;//标记枪A已经是枪的从机了。
						host_sent[0].host_Relay_control = 1;//可以并机，闭合并机接触器
						//为枪C的模块输出赋值
						host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
						host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
						host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
						model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
						host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[0].Charger_good_num;
						model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
						host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
						host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
						//为枪A的模块输出赋值
						host_sent[0].chgenable = 1;
						host_sent[0].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
						host_sent[0].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
						model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
						host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[0].Charger_good_num;
						model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[0].Charger_good_num)/host_model_num[gun_temp];
						host_sent[0].model_output_cur[0] = model_temp_a[gun_temp];
						host_sent[0].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
					}
						
				}
			}
			else
			{//该枪模块可以满足需求，不需要并机
			  //将需求电压、电流原值下发。
			  
			  if(host_congji[gun_temp][0] == 2)
				{
					i = 0;
					host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
					//将开机标志清零；
					host_sent[i].chgenable = 0;
					//将模块输出清零；
					 host_sent[i].model_output_vol[0] = 0;
					 host_sent[i].model_output_vol[1] = 0;
					 host_sent[i].model_output_cur[0] = 0;
					 host_sent[i].model_output_cur[1] = 0; 
					 host_congji[gun_temp][i] = 0;
				}
				if(host_congji[gun_temp][1] == 2)
				{
					i = 1;
					host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
					//将开机标志清零；
					host_sent[i].chgenable = 0;
					//将模块输出清零；
					 host_sent[i].model_output_vol[0] = 0;
					 host_sent[i].model_output_vol[1] = 0;
					 host_sent[i].model_output_cur[0] = 0;
					 host_sent[i].model_output_cur[1] = 0; 
					 host_congji[gun_temp][i] = 0;
				}
				if(host_congji[gun_temp][3] == 2)
				{
					i = 3;
					host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
					//将开机标志清零；
					host_sent[i].chgenable = 0;
					//将模块输出清零；
					 host_sent[i].model_output_vol[0] = 0;
					 host_sent[i].model_output_vol[1] = 0;
					 host_sent[i].model_output_cur[0] = 0;
					 host_sent[i].model_output_cur[1] = 0; 
					 host_congji[gun_temp][i] = 0;
				}
			  
			  host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
			  host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
			  host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
			  host_sent[gun_temp].model_output_cur[0] = host_recv[gun_temp].bmsma[0];
			  host_sent[gun_temp].model_output_cur[1] = host_recv[gun_temp].bmsma[1]; 
			  if(debug == 1)printf("host_sent[gun_temp].model_output_vol[0] = %x\n",host_sent[gun_temp].model_output_vol[0]);
			  if(debug == 1)printf("host_sent[gun_temp].model_output_vol[1] = %x\n",host_sent[gun_temp].model_output_vol[1]);
			  //断开并机接触器
			  host_sent[gun_temp].host_Relay_control = 0;
			  host_sent[0].host_Relay_control = 0;
			}
		}
		else//当枪C不需要并机了，将它的从机全部断开
		{
			if(host_congji[gun_temp][0] == 2)
			{
				i = 0;
				host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
				//将开机标志清零；
				host_sent[i].chgenable = 0;
				//将模块输出清零；
				 host_sent[i].model_output_vol[0] = 0;
				 host_sent[i].model_output_vol[1] = 0;
				 host_sent[i].model_output_cur[0] = 0;
				 host_sent[i].model_output_cur[1] = 0; 
				 host_congji[gun_temp][i] = 0;
			}
			if(host_congji[gun_temp][1] == 2)
			{
				i = 1;
				host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
				//将开机标志清零；
				host_sent[i].chgenable = 0;
				//将模块输出清零；
				 host_sent[i].model_output_vol[0] = 0;
				 host_sent[i].model_output_vol[1] = 0;
				 host_sent[i].model_output_cur[0] = 0;
				 host_sent[i].model_output_cur[1] = 0; 
				 host_congji[gun_temp][i] = 0;
			}
			if(host_congji[gun_temp][3] == 2)
			{
				i = 3;
				host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
				//将开机标志清零；
				host_sent[i].chgenable = 0;
				//将模块输出清零；
				 host_sent[i].model_output_vol[0] = 0;
				 host_sent[i].model_output_vol[1] = 0;
				 host_sent[i].model_output_cur[0] = 0;
				 host_sent[i].model_output_cur[1] = 0; 
				 host_congji[gun_temp][i] = 0;
			}
			
			if(host_recv[gun_temp].chgstart != 1)
			{
				i = gun_temp;
				host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
				//将开机标志清零；
				host_sent[i].chgenable = 0;
				//将模块输出清零；
				 host_sent[i].model_output_vol[0] = 0;
				 host_sent[i].model_output_vol[1] = 0;
				 host_sent[i].model_output_cur[0] = 0;
				 host_sent[i].model_output_cur[1] = 0; 
				 host_congji[gun_temp][i] = 0;
			}
		}
		
	}
			
	//当该枪中的某把枪不能并机了，将它的从机标志清掉
	for(i=0;i<Globa->Charger_param.gun_config;i++)
	{
		if(i != gun_temp)
			if(host_recv[i].host_flag != 0)
				if(host_congji[gun_temp][i] == 2)
					host_congji[gun_temp][i] = 0;
	}
	if(debug == 1)
		for(i=0;i<Globa->Charger_param.gun_config;i++)
		{
			printf("host_sent[%d].model_output_vol[0] = %d\n",i,host_sent[i].model_output_vol[0]);
			printf("host_sent[%d].model_output_vol[1] = %d\n",i,host_sent[i].model_output_vol[1]);
			printf("host_sent[%d].model_output_cur[0] = %d\n",i,host_sent[i].model_output_vol[0]);
			printf("host_sent[%d].model_output_cur[1] = %d\n",i,host_sent[i].model_output_vol[1]);
			printf("host_sent[%d].model_output_vol[0] = %d\n",i,host_sent[i].model_output_vol[0]);
			printf("host_sent[%d].chgenable = %d\n",i,host_sent[i].chgenable);
			printf("host_sent[%d].host_Relay_control = %d\n",i,host_sent[i].host_Relay_control);
			printf("\n\n");
		}
	
	//并机逻辑，枪D的处理
	//D-B/C-A	
	gun_temp = 3;
	if(host_recv_flag[gun_temp] == 1)
	{
		host_recv_flag[gun_temp] = 0;
		if(host_recv[gun_temp].gunstate == 1 && host_recv[gun_temp].chgstart == 1 && host_recv[gun_temp].host_flag == 1)
		{//枪插着，并且开机了，说明该枪需要进行模块输出处理
			if(host_recv[gun_temp].need_model_num > host_recv[gun_temp].Charger_good_num)
			{//此时需要并机,需要进行下一步判断
				//if(host_recv[1].host_flag != 0)//判断B枪是否可以并机
				if(!((host_recv[1].host_flag == 0) && (1 == host_panduan(3,1))))
				{//不能并机
					//host_sent[1].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
						//if(host_recv[2].host_flag != 0)//判断C枪是否可以并机
						if(!((host_recv[2].host_flag == 0) && (1 == host_panduan(3,2))))
						{//不能并机
							//当B、C枪都不能并机时，枪D不能并机。
							host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
						    host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
						    host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
						    host_sent[gun_temp].model_output_cur[0] = host_recv[gun_temp].bmsma[0];
						    host_sent[gun_temp].model_output_cur[1] = host_recv[gun_temp].bmsma[1]; 
						   //断开并机接触器
						    host_sent[gun_temp].host_Relay_control = 0;
							host_sent[2].host_Relay_control = 0;
						}
						else if((host_recv[2].host_flag == 0) && (1 == host_panduan(3,2)))
						{//可以并机
							if(host_recv[gun_temp].need_model_num > 
								(host_recv[gun_temp].Charger_good_num + 
									host_recv[2].Charger_good_num))
							{//在C枪并机之后还需要并机
								//if(host_recv[0].host_flag != 0)//判断A枪是否可以并机
								if(!((host_recv[0].host_flag == 0) && (1 == host_panduan(3,0))))
								{//枪A\B不可以并机，此时只有C枪进行输出
									host_sent[0].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_sent[3].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
									
									host_congji[gun_temp][2] = 2;//标记枪C已经是枪的从机了。
									host_sent[2].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪D的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[2].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪C的模块输出赋值
									host_sent[2].chgenable = 1;
									host_sent[2].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[2].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num +  host_recv[2].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[2].Charger_good_num)/host_model_num[gun_temp];
									host_sent[2].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[2].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								}
								else if((host_recv[0].host_flag == 0) && (1 == host_panduan(3,0)))
								{//可以并机
									host_sent[3].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
									host_sent[1].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
									
									host_congji[gun_temp][2] = 2;//标记枪C已经是枪的从机了。
									host_congji[gun_temp][0] = 2;//标记枪A已经是枪的从机了。
									host_sent[0].host_Relay_control = 1;//可以并机，闭合并机接触器
									host_sent[2].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪D的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num 
																+ host_recv[2].Charger_good_num + host_recv[0].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
									//为枪C的模块输出赋值
									host_sent[2].chgenable = 1;
									host_sent[2].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[2].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num 
																+ host_recv[2].Charger_good_num + host_recv[0].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[2].Charger_good_num)/host_model_num[gun_temp];
									host_sent[2].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[2].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
									//为枪A的模块输出赋值
									host_sent[0].chgenable = 1;
									host_sent[0].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[0].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num 
																+ host_recv[2].Charger_good_num + host_recv[0].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[0].Charger_good_num)/host_model_num[gun_temp];
									host_sent[0].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[0].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								}
							}
							else 
							{//不再需要了，此时将D、C的输出信息下发
									host_sent[0].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
									host_sent[3].host_Relay_control = 0;//不需要并机了，将并机接触器断开；	
									if(host_congji[gun_temp][0] == 2)
									{
										i = 0;
										host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
										//将开机标志清零；
										host_sent[i].chgenable = 0;
										//将模块输出清零；
										 host_sent[i].model_output_vol[0] = 0;
										 host_sent[i].model_output_vol[1] = 0;
										 host_sent[i].model_output_cur[0] = 0;
										 host_sent[i].model_output_cur[1] = 0; 
										 host_congji[gun_temp][i] = 0;
									}
									if(host_congji[gun_temp][1] == 2)
									{
										i = 1;
										host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
										//将开机标志清零；
										host_sent[i].chgenable = 0;
										//将模块输出清零；
										 host_sent[i].model_output_vol[0] = 0;
										 host_sent[i].model_output_vol[1] = 0;
										 host_sent[i].model_output_cur[0] = 0;
										 host_sent[i].model_output_cur[1] = 0; 
										 host_congji[gun_temp][i] = 0;
									}						
									host_congji[gun_temp][2] = 2;//标记枪C已经是枪的从机了。
									host_sent[2].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪D的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[2].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪C的模块输出赋值
									host_sent[2].chgenable = 1;
									host_sent[2].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[2].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num +  host_recv[2].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[2].Charger_good_num)/host_model_num[gun_temp];
									host_sent[2].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[2].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
							}
							
						}
										
				}
				else if((host_recv[1].host_flag == 0) && (1 == host_panduan(3,1)))
				{//可以并机
					if(host_recv[gun_temp].need_model_num > 
						(host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num))
					{//将B枪并进去后，还需要进行并机
						//if(host_recv[2].host_flag != 0)//判断C枪是否可以并机
						if(!((host_recv[2].host_flag == 0) && (1 == host_panduan(3,2))))
						{//不能并机
							host_sent[2].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
							//if(host_recv[0].host_flag != 0)//判断A枪是否可以并机
							if(!((host_recv[0].host_flag == 0) && (1 == host_panduan(3,0))))
								{//不能并机 此时C、D枪都不能并机，只有B枪可以并机。
									host_sent[2].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_sent[1].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
									
									host_congji[gun_temp][1] = 2;//标记枪B已经是枪的从机了。
									host_sent[3].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪D的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪B的模块输出赋值
									host_sent[1].chgenable = 1;
									host_sent[1].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[1].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[1].Charger_good_num)/host_model_num[gun_temp];
									host_sent[1].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[1].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								}
							else if((host_recv[0].host_flag == 0) && (1 == host_panduan(3,0)))
							{//可以并机
								//此时A、B、D可以并机
									host_sent[0].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
									host_sent[2].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
									
									host_congji[gun_temp][1] = 2;//标记枪B已经是枪的从机了。
									host_congji[gun_temp][0] = 2;//标记枪A已经是枪的从机了。
									host_sent[1].host_Relay_control = 1;//可以并机，闭合并机接触器
									host_sent[3].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪D的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num 
															   + host_recv[0].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪B的模块输出赋值
									host_sent[1].chgenable = 1;
									host_sent[1].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[1].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num 
																+ host_recv[0].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[1].Charger_good_num)/host_model_num[gun_temp];
									host_sent[1].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[1].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
									//为枪A的模块输出赋值
									host_sent[0].chgenable = 1;
									host_sent[0].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[0].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num 
																  + host_recv[0].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[0].Charger_good_num)/host_model_num[gun_temp];
									host_sent[0].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[0].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
							}
						}
						else if((host_recv[2].host_flag == 0) && (1 == host_panduan(3,2)))
						{//可以并机
							if(host_recv[gun_temp].need_model_num > 
								(host_recv[gun_temp].Charger_good_num + 
									host_recv[1].Charger_good_num + 
									host_recv[2].Charger_good_num))
							{//在B、C枪并机之后还需要并机
								//if(host_recv[0].host_flag != 0)//判断A枪是否可以并机
								if(!((host_recv[0].host_flag == 0) && (1 == host_panduan(3,0))))
								{//枪A不可以并机，此时只有D、B、C三枪进行输出
									host_sent[0].host_Relay_control = 0;//不能并机的话，下发并机接触器断开指令
									host_sent[1].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
									host_congji[gun_temp][1] = 2;//标记枪B已经是枪的从机了。
									host_congji[gun_temp][2] = 2;//标记枪C已经是枪的从机了。
									host_sent[2].host_Relay_control = 1;//可以并机，闭合并机接触器
									host_sent[3].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪D的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num + host_recv[2].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪B的模块输出赋值
									host_sent[1].chgenable = 1;
									host_sent[1].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[1].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num + host_recv[2].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[1].Charger_good_num)/host_model_num[gun_temp];
									host_sent[1].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[1].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
									//为枪C的模块输出赋值
									host_sent[2].chgenable = 1;
									host_sent[2].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[2].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num + host_recv[2].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[2].Charger_good_num)/host_model_num[gun_temp];
									host_sent[2].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[2].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								}
								else if((host_recv[0].host_flag == 0) && (1 == host_panduan(3,0)))
								{//可以并机
									host_sent[2].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
											
									host_congji[gun_temp][1] = 2;//标记枪B已经是枪的从机了。
									host_congji[gun_temp][2] = 2;//标记枪C已经是枪的从机了。
									host_congji[gun_temp][0] = 2;//标记枪A已经是枪的从机了。
									host_sent[1].host_Relay_control = 1;//可以并机，闭合并机接触器
									host_sent[0].host_Relay_control = 1;//可以并机，闭合并机接触器
									host_sent[3].host_Relay_control = 1;//可以并机，闭合并机接触器
									//为枪D的模块输出赋值
									host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
									host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num 
																+ host_recv[2].Charger_good_num + host_recv[0].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
									host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
									//为枪B的模块输出赋值
									host_sent[1].chgenable = 1;
									host_sent[1].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[1].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num 
																+ host_recv[2].Charger_good_num + host_recv[0].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[1].Charger_good_num)/host_model_num[gun_temp];
									host_sent[1].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[1].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
									//为枪C的模块输出赋值
									host_sent[2].chgenable = 1;
									host_sent[2].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[2].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num 
																+ host_recv[2].Charger_good_num + host_recv[0].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[2].Charger_good_num)/host_model_num[gun_temp];
									host_sent[2].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[2].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
									//为枪A的模块输出赋值
									host_sent[0].chgenable = 1;
									host_sent[0].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
									host_sent[0].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
									model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
									host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num 
																+ host_recv[2].Charger_good_num + host_recv[0].Charger_good_num;
									model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[0].Charger_good_num)/host_model_num[gun_temp];
									host_sent[0].model_output_cur[0] = model_temp_a[gun_temp];
									host_sent[0].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								}
							}
							else 
							{//不再需要了，此时将D、B、C的输出信息下发
								host_sent[0].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
								host_sent[1].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
								if(host_congji[gun_temp][0] == 2)
								{
									i = 0;
									host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
									//将开机标志清零；
									host_sent[i].chgenable = 0;
									//将模块输出清零；
									 host_sent[i].model_output_vol[0] = 0;
									 host_sent[i].model_output_vol[1] = 0;
									 host_sent[i].model_output_cur[0] = 0;
									 host_sent[i].model_output_cur[1] = 0; 
									 host_congji[gun_temp][i] = 0;
								}
									
								host_congji[gun_temp][1] = 2;//标记枪B已经是枪的从机了。
								host_congji[gun_temp][2] = 2;//标记枪C已经是枪的从机了。
								host_sent[3].host_Relay_control = 1;//可以并机，闭合并机接触器
								host_sent[2].host_Relay_control = 1;//可以并机，闭合并机接触器
								//为枪D的模块输出赋值
								host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
								host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
								host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
								model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
								host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num + host_recv[2].Charger_good_num;
								model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
								host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
								host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
								//为枪B的模块输出赋值
								host_sent[1].chgenable = 1;
								host_sent[1].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
								host_sent[1].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
								model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
								host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num + host_recv[2].Charger_good_num;
								model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[1].Charger_good_num)/host_model_num[gun_temp];
								host_sent[1].model_output_cur[0] = model_temp_a[gun_temp];
								host_sent[1].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
								//为枪C的模块输出赋值
								host_sent[2].chgenable = 1;
								host_sent[2].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
								host_sent[2].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
								model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
								host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num + host_recv[2].Charger_good_num;
								model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[2].Charger_good_num)/host_model_num[gun_temp];
								host_sent[2].model_output_cur[0] = model_temp_a[gun_temp];
								host_sent[2].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
							}
							
						}
					}
					else
					{//将B枪并进去之后，不需要在并机了。
						host_sent[2].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
						host_sent[1].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
						
						if(host_congji[gun_temp][0] == 2)
						{
							i = 0;
							host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
							//将开机标志清零；
							host_sent[i].chgenable = 0;
							//将模块输出清零；
							 host_sent[i].model_output_vol[0] = 0;
							 host_sent[i].model_output_vol[1] = 0;
							 host_sent[i].model_output_cur[0] = 0;
							 host_sent[i].model_output_cur[1] = 0; 
							 host_congji[gun_temp][i] = 0;
						}
						if(host_congji[gun_temp][2] == 2)
						{
							i = 2;
							host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
							//将开机标志清零；
							host_sent[i].chgenable = 0;
							//将模块输出清零；
							 host_sent[i].model_output_vol[0] = 0;
							 host_sent[i].model_output_vol[1] = 0;
							 host_sent[i].model_output_cur[0] = 0;
							 host_sent[i].model_output_cur[1] = 0; 
							 host_congji[gun_temp][i] = 0;
						}
						
						host_congji[gun_temp][1] = 2;//标记枪B已经是枪的从机了。
						host_sent[3].host_Relay_control = 1;//可以并机，闭合并机接触器
						//为枪D的模块输出赋值
						host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
						host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
						host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
						model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
						host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num;
						model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[gun_temp].Charger_good_num)/host_model_num[gun_temp];
						host_sent[gun_temp].model_output_cur[0] = model_temp_a[gun_temp];
						host_sent[gun_temp].model_output_cur[1] = model_temp_a[gun_temp] >> 8; 
						//为枪B的模块输出赋值
						host_sent[1].chgenable = 1;
						host_sent[1].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
						host_sent[1].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
						model_temp_a[gun_temp] = ((host_recv[gun_temp].bmsma[0]) + (host_recv[gun_temp].bmsma[1] << 8));
						host_model_num[gun_temp] = host_recv[gun_temp].Charger_good_num + host_recv[1].Charger_good_num;
						model_temp_a[gun_temp] = (model_temp_a[gun_temp]*host_recv[1].Charger_good_num)/host_model_num[gun_temp];
						host_sent[1].model_output_cur[0] = model_temp_a[gun_temp];
						host_sent[1].model_output_cur[1] = model_temp_a[gun_temp] >> 8;
					}
						
				}
			}
			else
			{//该枪模块可以满足需求，不需要并机
			  //将需求电压、电流原值下发。
			  
			  if(host_congji[gun_temp][0] == 2)
				{
					i = 0;
					host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
					//将开机标志清零；
					host_sent[i].chgenable = 0;
					//将模块输出清零；
					 host_sent[i].model_output_vol[0] = 0;
					 host_sent[i].model_output_vol[1] = 0;
					 host_sent[i].model_output_cur[0] = 0;
					 host_sent[i].model_output_cur[1] = 0; 
					 host_congji[gun_temp][i] = 0;
				}
				if(host_congji[gun_temp][1] == 2)
				{
					i = 1;
					host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
					//将开机标志清零；
					host_sent[i].chgenable = 0;
					//将模块输出清零；
					 host_sent[i].model_output_vol[0] = 0;
					 host_sent[i].model_output_vol[1] = 0;
					 host_sent[i].model_output_cur[0] = 0;
					 host_sent[i].model_output_cur[1] = 0; 
					 host_congji[gun_temp][i] = 0;
				}
				if(host_congji[gun_temp][2] == 2)
				{
					i = 2;
					host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
					//将开机标志清零；
					host_sent[i].chgenable = 0;
					//将模块输出清零；
					 host_sent[i].model_output_vol[0] = 0;
					 host_sent[i].model_output_vol[1] = 0;
					 host_sent[i].model_output_cur[0] = 0;
					 host_sent[i].model_output_cur[1] = 0; 
					 host_congji[gun_temp][i] = 0;
				}
			  
			  host_sent[gun_temp].chgenable = host_recv[gun_temp].chgstart;
			  host_sent[gun_temp].model_output_vol[0] = host_recv[gun_temp].bmsmv[0];
			  host_sent[gun_temp].model_output_vol[1] = host_recv[gun_temp].bmsmv[1];
			  host_sent[gun_temp].model_output_cur[0] = host_recv[gun_temp].bmsma[0];
			  host_sent[gun_temp].model_output_cur[1] = host_recv[gun_temp].bmsma[1]; 
			  if(debug == 1)printf("host_sent[gun_temp].model_output_vol[0] = %x\n",host_sent[gun_temp].model_output_vol[0]);
			  if(debug == 1)printf("host_sent[gun_temp].model_output_vol[1] = %x\n",host_sent[gun_temp].model_output_vol[1]);
			  //断开并机接触器
			  host_sent[gun_temp].host_Relay_control = 0;
			  host_sent[2].host_Relay_control = 0;
			}
		}
		else//当枪D不需要并机了，将它的从机全部断开
		{
			if(host_congji[gun_temp][0] == 2)
			{
				i = 0;
				host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
				//将开机标志清零；
				host_sent[i].chgenable = 0;
				//将模块输出清零；
				 host_sent[i].model_output_vol[0] = 0;
				 host_sent[i].model_output_vol[1] = 0;
				 host_sent[i].model_output_cur[0] = 0;
				 host_sent[i].model_output_cur[1] = 0; 
				 host_congji[gun_temp][i] = 0;
			}
			if(host_congji[gun_temp][1] == 2)
			{
				i = 1;
				host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
				//将开机标志清零；
				host_sent[i].chgenable = 0;
				//将模块输出清零；
				 host_sent[i].model_output_vol[0] = 0;
				 host_sent[i].model_output_vol[1] = 0;
				 host_sent[i].model_output_cur[0] = 0;
				 host_sent[i].model_output_cur[1] = 0; 
				 host_congji[gun_temp][i] = 0;
			}
			if(host_congji[gun_temp][2] == 2)
			{
				i = 2;
				host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
				//将开机标志清零；
				host_sent[i].chgenable = 0;
				//将模块输出清零；
				 host_sent[i].model_output_vol[0] = 0;
				 host_sent[i].model_output_vol[1] = 0;
				 host_sent[i].model_output_cur[0] = 0;
				 host_sent[i].model_output_cur[1] = 0; 
				 host_congji[gun_temp][i] = 0;
			}
			if(host_recv[gun_temp].chgstart != 1)
			{
				i = gun_temp;
				host_sent[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
				//将开机标志清零；
				host_sent[i].chgenable = 0;
				//将模块输出清零；
				 host_sent[i].model_output_vol[0] = 0;
				 host_sent[i].model_output_vol[1] = 0;
				 host_sent[i].model_output_cur[0] = 0;
				 host_sent[i].model_output_cur[1] = 0; 
				 host_congji[gun_temp][i] = 0;
			}
		}
		
	}
	
	//当该枪中的某把枪不能并机了，将它的从机标志清掉
	for(i=0;i<Globa->Charger_param.gun_config;i++)
	{
		if(i != gun_temp)
			if(host_recv[i].host_flag != 0)
				if(host_congji[gun_temp][i] == 2)
					host_congji[gun_temp][i] = 0;
	}
	if(debug == 1)
		for(i=0;i<Globa->Charger_param.gun_config;i++)
		{
			printf("host_sent[%d].model_output_vol[0] = %d\n",i,host_sent[i].model_output_vol[0]);
			printf("host_sent[%d].model_output_vol[1] = %d\n",i,host_sent[i].model_output_vol[1]);
			printf("host_sent[%d].model_output_cur[0] = %d\n",i,host_sent[i].model_output_vol[0]);
			printf("host_sent[%d].model_output_cur[1] = %d\n",i,host_sent[i].model_output_vol[1]);
			printf("host_sent[%d].model_output_vol[0] = %d\n",i,host_sent[i].model_output_vol[0]);
			printf("host_sent[%d].chgenable = %d\n",i,host_sent[i].chgenable);
			printf("host_sent[%d].host_Relay_control = %d\n",i,host_sent[i].host_Relay_control);
			printf("\n\n");
		}
	
}


//一机五枪专用处理函数
//五把枪并机逻辑
/*
	1、构建一个循环，把一个枪号输入进去；
	2、输出各个继电器的状态，以及对应模块输出的电压、电流；
	3、A\B\C\D\E五把枪对应数字0、1、2、3、4
	4、0、1、2、3、4分别对应五把枪上的并机接触器，0控制0和1之间的，1控制1和2之间的，...4控制4和0之间的；
	5、0、1、2、3、4五把枪构成一个环，当一个枪的相邻两把枪不能并机时，该枪不能并机。
	6、判断并机的顺序是，先判断系统中有哪些枪可以并机，然后在判断在可以并机的枪中，有没有并机路线不通的；
*/

//判断除该枪之外的枪哪些可以给该枪并机做从机
void host_gun_judge(unsigned char ben_gun)
{
	unsigned char i=0,m=0,cnn=0;
	unsigned char ben_gun_temp=0;//在循环判断阶段将ben_gun的枪编号提高一个圈，便于判断
	unsigned char host_temp[max_gun_num];
	unsigned char host_temp_num = 0;
	unsigned char host_gun_1[max_gun_num];
	unsigned int model_temp_a[max_gun_num];
	unsigned char left_cnn = 0,right_cnn=0;
	unsigned char gun_host_temp = 0;
	unsigned char host_model_num_ok = 0;
	unsigned char host_model_num =host_recv_temp[ben_gun].Charger_good_num;
	memset(host_temp,0xff,max_gun_num);
	memset(host_gun_1,0xff,max_gun_num);
	//下面这个循环是为了取出可以作为并机的枪号用 host_temp[m]表示，m的值表示有几个可以并机枪个数
	for(i=0;i<GUN_MAX;i++)
	{
		//首先判断系统中那些枪可以用来做并机
		if(i != ben_gun)//不是该枪的时候在进行判断赋值
		{
			if(((host_recv_temp[i].host_flag == 0) && (1 == host_panduan_temp(ben_gun,i))))
			{
				host_temp_num++;
			}
			else
			{
				printf("初次 枪 %d 不能并机\n",i);
			}
		}	
	}
	printf("ben_gun = %d\n",ben_gun);
	printf("host_temp_num_chuci = %d\n",host_temp_num);
	//在下面这个刷选里边必须将对应的接触器控制也实现了
	if(host_temp_num > 0)//说明有枪有可能可以给ben_gun枪并机
	{
		host_temp_num = 0;//将该枪的可并机枪数量清零，下面重新判断
		ben_gun_temp = ben_gun + GUN_MAX;
		//从右边开始遍历，遍历长度为 (GUN_MAX - 1)
		//遇到不能并机的退出循环
		//当并机的模块数量满足需求了，退出循环
		for(right_cnn=1;right_cnn < (GUN_MAX);right_cnn++)
		{
			if(((host_recv_temp[((ben_gun_temp+right_cnn)%GUN_MAX)].host_flag == 0) && (1 == host_panduan_temp(ben_gun,((ben_gun_temp+right_cnn)%GUN_MAX)))))
			{//将ben_gun加上往右边判断的次数，作为将要判断的枪，该枪可以充电
				//可以充电时，将该枪记录一下，将从机标志位置位为2，将本枪已并机数量加1
				//将已经并机的模块数量累积
				//将需要并机的接触器闭合
				host_temp[host_temp_num] = ((ben_gun_temp+right_cnn)%GUN_MAX);
				host_congji[ben_gun][((ben_gun_temp+right_cnn)%GUN_MAX)] = 2;
				host_sent_temp[((ben_gun_temp+right_cnn - 1)%GUN_MAX)].host_Relay_control = 1;
				host_model_num += host_recv_temp[((ben_gun_temp+right_cnn)%GUN_MAX)].Charger_good_num;
				host_temp_num++;
				gun_host_temp = 1;
			}
			else
			{
				host_sent_temp[((ben_gun_temp+right_cnn - 1)%GUN_MAX)].host_Relay_control = 0;
				host_congji[ben_gun][((ben_gun_temp+right_cnn)%GUN_MAX)] = 0;				
				printf("右边枪 %d 不能并机\n",((ben_gun_temp+right_cnn)%GUN_MAX));
				break;
			}
			if(!(host_model_num < host_recv_temp[ben_gun].need_model_num))
			{
				host_model_num_ok = 1;
				printf("右边 manzubingjile\n");
				break;
			}
				
		}
		//判断是否已经满足要求或者判断了一圈了
		if(!((right_cnn == GUN_MAX) || host_model_num_ok == 1))
		{//没有判断一圈，并且不满足模块需求
			//接下来从左边遍历，
			for(left_cnn=1;left_cnn < (GUN_MAX);left_cnn++)
			{
				if(((host_recv_temp[((ben_gun_temp-left_cnn)%GUN_MAX)].host_flag == 0) && (1 == host_panduan_temp(ben_gun,((ben_gun_temp-left_cnn)%GUN_MAX)))))
				{//将ben_gun减去往左边判断的次数，作为将要判断的枪，该枪可以充电
					//可以充电时，将该枪记录一下，将从机标志位置位为2，将本枪已并机数量加1
					//将已经并机的模块数量累积
					host_temp[host_temp_num] = ((ben_gun_temp-left_cnn)%GUN_MAX);
					host_congji[ben_gun][((ben_gun_temp-left_cnn)%GUN_MAX)] = 2;
					host_sent_temp[((ben_gun_temp-left_cnn)%GUN_MAX)].host_Relay_control = 1;
					host_model_num += host_recv_temp[((ben_gun_temp-left_cnn)%GUN_MAX)].Charger_good_num;
					host_temp_num++;
					gun_host_temp = 1;
				}
				else
				{
					host_sent_temp[((ben_gun_temp-left_cnn)%GUN_MAX)].host_Relay_control = 0;
					host_congji[ben_gun][((ben_gun_temp-left_cnn)%GUN_MAX)] = 0;
					printf("左边枪 %d 不能并机\n",((ben_gun_temp-left_cnn)%GUN_MAX));
					break;
				}
				if(!(host_model_num < host_recv_temp[ben_gun].need_model_num))
				{
					host_model_num_ok = 1;
					printf("左边 zuo manzubingjile\n");
					break;
				}
			}
		}
		
	}
	else
	{
		gun_host_temp = 0;
		//将该枪两边的接触器断开
		host_sent_temp[ben_gun].host_Relay_control = 0;
		if(ben_gun == 0)
			host_sent_temp[GUN_MAX-1].host_Relay_control = 0;
		else
			host_sent_temp[ben_gun - 1].host_Relay_control = 0;
	}
	if(host_temp_num == 0)
		gun_host_temp = 0;
	printf("ben_gun = %d\n",ben_gun);
	printf("gun_host_temp = %d\n",gun_host_temp);
	printf("host_temp_num = %d\n",host_temp_num);
	printf("host_congji[bengun][0] = %d\n",host_congji[ben_gun][0]);
	printf("host_congji[bengun][1] = %d\n",host_congji[ben_gun][1]);
	printf("host_congji[bengun][2] = %d\n",host_congji[ben_gun][2]);
	printf("host_congji[bengun][3] = %d\n",host_congji[ben_gun][3]);
	printf("host_congji[bengun][4] = %d\n",host_congji[ben_gun][4]);
	
	
	
	
	if(gun_host_temp == 1)//有枪作为从机，下发对应电压电流。
	{
		//把本机的模块输出先处理了
		host_sent_temp[ben_gun].chgenable = host_recv_temp[ben_gun].chgstart;
		host_sent_temp[ben_gun].model_output_vol[0] = host_recv_temp[ben_gun].bmsmv[0];
		host_sent_temp[ben_gun].model_output_vol[1] = host_recv_temp[ben_gun].bmsmv[1];
		model_temp_a[ben_gun] = ((host_recv_temp[ben_gun].bmsma[0]) + (host_recv_temp[ben_gun].bmsma[1] << 8));//计算出需求电流
		//将需求电流按每个枪的数量进行分配
		model_temp_a[ben_gun] = (model_temp_a[ben_gun]*host_recv_temp[ben_gun].Charger_good_num)/host_model_num;
		host_sent_temp[ben_gun].model_output_cur[0] = model_temp_a[ben_gun];
		host_sent_temp[ben_gun].model_output_cur[1] = model_temp_a[ben_gun] >> 8; 
		
		//在处理并机模块的输出
		for(cnn=0;cnn<host_temp_num;cnn++)
		{
			host_sent_temp[host_temp[cnn]].chgenable = 1;
			host_sent_temp[host_temp[cnn]].model_output_vol[0] = host_recv_temp[ben_gun].bmsmv[0];
			host_sent_temp[host_temp[cnn]].model_output_vol[1] = host_recv_temp[ben_gun].bmsmv[1];
			model_temp_a[ben_gun] = ((host_recv_temp[ben_gun].bmsma[0]) + (host_recv_temp[ben_gun].bmsma[1] << 8));//计算出需求电流
			//将需求电流按每个枪的数量进行分配
			model_temp_a[ben_gun] = (model_temp_a[ben_gun]*host_recv_temp[host_temp[cnn]].Charger_good_num)/host_model_num;
			host_sent_temp[host_temp[cnn]].model_output_cur[0] = model_temp_a[ben_gun];
			host_sent_temp[host_temp[cnn]].model_output_cur[1] = model_temp_a[ben_gun] >> 8; 
			
		}
	}
	else//没有枪或者合适的枪作为从机，原值下发电压电流
	{
		host_sent_temp[ben_gun].host_Relay_control = 0;
		if(ben_gun == 0)
			host_sent_temp[GUN_MAX-1].host_Relay_control = 0;
		else
			host_sent_temp[ben_gun - 1].host_Relay_control = 0;
		//现将之前并机从机的枪退出去
		for(i=0;i<GUN_MAX;i++)
		{
			if(i != ben_gun)
			{
				if(host_congji[ben_gun][i] == 2)
				{
					host_sent_temp[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
					//将开机标志清零；
					host_sent_temp[i].chgenable = 0;
					//将模块输出清零；
					 host_sent_temp[i].model_output_vol[0] = 0;
					 host_sent_temp[i].model_output_vol[1] = 0;
					 host_sent_temp[i].model_output_cur[0] = 0;
					 host_sent_temp[i].model_output_cur[1] = 0; 
					 host_congji[ben_gun][i] = 0;
				}
			}						
		}
		//在将该枪的需求值进行下发
	   host_sent_temp[ben_gun].chgenable = host_recv_temp[ben_gun].chgstart;
	   host_sent_temp[ben_gun].model_output_vol[0] = host_recv_temp[ben_gun].bmsmv[0];
	   host_sent_temp[ben_gun].model_output_vol[1] = host_recv_temp[ben_gun].bmsmv[1];
	   host_sent_temp[ben_gun].model_output_cur[0] = host_recv_temp[ben_gun].bmsma[0];
	   host_sent_temp[ben_gun].model_output_cur[1] = host_recv_temp[ben_gun].bmsma[1]; 
	}
	//在刷选出来的可以做该枪并机的枪中在进行刷选
	//刷选条件：1、不管是从左到右，还是从右到左，至少要有一条路是通的；
	//          2、优先选择临近的枪作为并机从机；
	//          3、当并机从机数量已经满足了该枪的需求时，不再继续进行并机
	//将其他的不满足的或者多余的赋值为0，
	//先从左到右判断,判断的数量是上一个循环记录的可以并机的枪个数
}

void host_hand_temp()
{
	unsigned char i=0;
	GUN_MAX = max_gun_num;
	for(gun_5_temp=0;gun_5_temp<GUN_MAX;gun_5_temp++)	
	{
		//先判断有没有收到并机需求帧
		if(host_recv_flag[gun_5_temp] == 1)
		{
			host_recv_flag[gun_5_temp] = 0;
			//先判断该枪是不是需要进行模快输出处理
			//该枪的状态必须是：枪插着，充电启动标志为1，并机状态=不能并机（增加该项是保证在其他枪插枪空闲时也可以被用来做从机）
			if(host_recv_temp[gun_5_temp].gunstate == 1 && host_recv_temp[gun_5_temp].chgstart == 1 && host_recv_temp[gun_5_temp].host_flag == 1)
			{
				//先判断该枪是否需要并机
				if(host_recv_temp[gun_5_temp].need_model_num > host_recv_temp[gun_5_temp].Charger_good_num)
				{//当该枪需要并机时;
					host_gun_judge(gun_5_temp);//首先先判断一下系统中可以作为该枪的从机的枪有哪些。
					//判断一下需要闭合和断开那些并机接触器
					//根据可以并机的枪信息，计算没把枪需要输出的电压电流	
				}
				else//当该枪不需要并机时，原值下发
				{
					//现将之前并机从机的枪退出去
					for(i=0;i<GUN_MAX;i++)
					{
						if(i != gun_5_temp)
						{
							if(host_congji[gun_5_temp][i] == 2)
							{
								host_sent_temp[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
								//将开机标志清零；
								host_sent_temp[i].chgenable = 0;
								//将模块输出清零；
								 host_sent_temp[i].model_output_vol[0] = 0;
								 host_sent_temp[i].model_output_vol[1] = 0;
								 host_sent_temp[i].model_output_cur[0] = 0;
								 host_sent_temp[i].model_output_cur[1] = 0; 
								 host_congji[gun_5_temp][i] = 0;
							}
						}						
					}
					//在将该枪的需求值进行下发
				   host_sent_temp[gun_5_temp].chgenable = host_recv_temp[gun_5_temp].chgstart;
				   host_sent_temp[gun_5_temp].model_output_vol[0] = host_recv_temp[gun_5_temp].bmsmv[0];
				   host_sent_temp[gun_5_temp].model_output_vol[1] = host_recv_temp[gun_5_temp].bmsmv[1];
				   host_sent_temp[gun_5_temp].model_output_cur[0] = host_recv_temp[gun_5_temp].bmsma[0];
				   host_sent_temp[gun_5_temp].model_output_cur[1] = host_recv_temp[gun_5_temp].bmsma[1]; 
				   //将改枪左右两把枪的并机接触器断开
					//该枪左边的并机接触器由枪ID小一个数字的枪控制
					if(gun_5_temp == 0)
						host_sent_temp[GUN_MAX-1].host_Relay_control = 0;
					else
						host_sent_temp[gun_5_temp-1].host_Relay_control = 0;
					//该枪右边的并机接触器
				    host_sent_temp[gun_5_temp].host_Relay_control = 0;
				}
				
				
			}
			else//当该枪不需要并机了，将它的从机全部断开
			{
				if(host_recv_temp[gun_5_temp].chgstart != 1)//当该枪的充电启动标志位清零了，即不在充电中，也不是并机从机，将其并
				{
					i = gun_5_temp;
					host_sent_temp[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
					//将开机标志清零；
					host_sent_temp[i].chgenable = 0;
					//将模块输出清零；
					 host_sent_temp[i].model_output_vol[0] = 0;
					 host_sent_temp[i].model_output_vol[1] = 0;
					 host_sent_temp[i].model_output_cur[0] = 0;
					 host_sent_temp[i].model_output_cur[1] = 0; 
					 host_congji[gun_5_temp][i] = 0;
				}
				for(i=0;i<GUN_MAX;i++)
				{
					if(i != gun_5_temp)
					{
						if(host_congji[gun_5_temp][i] == 2)
						{
							host_sent_temp[i].host_Relay_control = 0;//不需要并机了，将并机接触器断开；
							//将开机标志清零；
							host_sent_temp[i].chgenable = 0;
							//将模块输出清零；
							 host_sent_temp[i].model_output_vol[0] = 0;
							 host_sent_temp[i].model_output_vol[1] = 0;
							 host_sent_temp[i].model_output_cur[0] = 0;
							 host_sent_temp[i].model_output_cur[1] = 0; 
							 host_congji[gun_5_temp][i] = 0;
						}
					}						
				}
			}
			
		}
			
	}
}

//原始变量与处理中间变量进行映射
void host_hand_5_gun()
{
	//现将接收到的信息进行映射处理
	memcpy(&host_recv_temp[0].gunstate,&host_recv[0].gunstate,sizeof(PC_PGN16256_FRAME));
	memcpy(&host_recv_temp[1].gunstate,&host_recv[2].gunstate,sizeof(PC_PGN16256_FRAME));
	memcpy(&host_recv_temp[2].gunstate,&host_recv[4].gunstate,sizeof(PC_PGN16256_FRAME));
	memcpy(&host_recv_temp[3].gunstate,&host_recv[3].gunstate,sizeof(PC_PGN16256_FRAME));
	memcpy(&host_recv_temp[4].gunstate,&host_recv[1].gunstate,sizeof(PC_PGN16256_FRAME));
	host_hand_temp();
	memcpy(&host_sent[0].gun,&host_sent_temp[0].gun,sizeof(PC_PGN16512_FRAME));
	memcpy(&host_sent[2].gun,&host_sent_temp[1].gun,sizeof(PC_PGN16512_FRAME));
	memcpy(&host_sent[4].gun,&host_sent_temp[2].gun,sizeof(PC_PGN16512_FRAME));
	memcpy(&host_sent[3].gun,&host_sent_temp[3].gun,sizeof(PC_PGN16512_FRAME));
	memcpy(&host_sent[1].gun,&host_sent_temp[4].gun,sizeof(PC_PGN16512_FRAME));
}

static void MC_Read_Task(void)
{
	int fd;
	struct can_filter rfilter[2];

	int i,Ret;
	fd_set rfds;
	struct timeval tv;
	struct can_frame frame;
	if((fd = BMS_can_fd_init(GUN_CAN_COM_ID1)) < 0){
		perror("BMS_Read_Task BMS_can_fd_init GUN_CAN_COM_ID1");
		while(1);
	}
		
	//<received_can_id> & mask == can_id
	rfilter[0].can_id   = 0x00008AF6;
	rfilter[0].can_mask = 0x0000FFFF;
	rfilter[1].can_id   = 0x00008AF7;
	rfilter[1].can_mask = 0x0000FFFF;

	if(setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter)) != 0){
		perror("setsockopt filter");
		while(1);
	}
	make_CRC32Table();//生成CRC校验码
	prctl(PR_SET_NAME,(unsigned long)"MC_Read_Task");//设置线程名字
	while(1)
	{
		usleep(1000);//1ms

		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		tv.tv_sec = 0;
		tv.tv_usec = 10000;//10ms
		//printf("MC_Read_Task\n");
		Ret = select(fd + 1, &rfds, NULL, NULL, &tv);
		if(Ret < 0)
		{
			perror("can raw socket read ");
			usleep(100000);//100ms
		}else if(Ret > 0)
		{
			if(FD_ISSET(fd, &rfds))
			{
			  Ret = read(fd, &frame, sizeof(frame));
			  if(Ret == sizeof(frame))
			  {
				 if((frame.can_id&0x1fffffff) == PGN16256_ID_CM_1)
				{
					//host_handle(frame);//当是并机报文时处理一下
					// if(frame.data[0] == 1)
						// printf("host_handle %x\n",frame.data[1]);
				}
				else
					if((frame.data[0] > 0) && (frame.data[0] <= Globa->Charger_param.gun_config))					
					{
						//printf("rx plug%d can_frame\n",frame.data[0]);//test
						MC_read_deal(frame,frame.data[0]-1);//x号控制板发过来的数据帧处理
					}					
			  }
			}
		}
	}
	close(fd);
	exit(0);
}

/*********************************************************
**description：BMS读线程初始化及建立
***parameter  :none
***return		  :none
**********************************************************/
extern void MC_Read_Thread(void)
{
	pthread_t td1;
	int ret ,stacksize = 1024*1024;
	pthread_attr_t attr;

	ret = pthread_attr_init(&attr);
	if (ret != 0)
		return;

	ret = pthread_attr_setstacksize(&attr, stacksize);
	if(ret != 0)
		return;
	
	/* 创建自动串口抄收线程 */
	if(0 != pthread_create(&td1, &attr, (void *)MC_Read_Task, NULL)){
		perror("####pthread_create IC_Read_Write_Task ERROR ####\n\n");
	}

	ret = pthread_attr_destroy(&attr);
	if(ret != 0)
		return;
}

/*********************************************************
**description：MC管理线程
***parameter  :none
***return		  :none
**********************************************************/
static void MC_Manage_Task(void* gun_no)
{	
	INT32 fd;
	UINT32 temp1=0xff,temp2=0xff;
	UINT32 Error_equal = 0,ctr_connect_count = 0;
	UINT32  AC_Relay_Ctl_Count = 0;	
	UINT8 temp_char[32]; 
	UINT32 pre_SYS_Step,pre_MC_Step;
	UINT32 Gun_No = *((UINT32*)gun_no);//值0---4
	if(Gun_No+1 > CONTROL_DC_NUMBER)
		return ;
	MC_Init_timers(Gun_No);
	MC_timer_start(Gun_No,5002);//遥测帧定时
	MC_timer_start(Gun_No,400);
	
	Globa->Control_DC_Infor_Data[Gun_No].charger_start = 0;//充电停止
	Globa->Control_DC_Infor_Data[Gun_No].Time = 0;//已充电时长
	Globa->Control_DC_Infor_Data[Gun_No].SYS_Step = 0x00;

	if((fd = BMS_can_fd_init(GUN_CAN_COM_ID1)) < 0)
	{
		while(1);
	}
	sprintf(temp_char,"MC%d_Mange_Task",Gun_No+1);
	prctl(PR_SET_NAME,(unsigned long)temp_char);//设置线程名字

	//在指定的CAN_RAW套接字上禁用接收过滤规则,则该线程只发送，不会接收数据，省内存和CPU时间
	setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FILTER, NULL, 0);

	while(1)
	{
		usleep(10000);//做基准时钟调度10ms
		mc_control_var[Gun_No].MC_Manage_Watchdog = 1;
		MC_timer_run(Gun_No);

		if(pre_SYS_Step != Globa->Control_DC_Infor_Data[Gun_No].SYS_Step)
		{
			printf("44444xxxxxx Plug%d MC_SYS_Step=0x%x\n",Gun_No,Globa->Control_DC_Infor_Data[Gun_No].SYS_Step);
			pre_SYS_Step = Globa->Control_DC_Infor_Data[Gun_No].SYS_Step;
		}
		if(pre_MC_Step != Globa->Control_DC_Infor_Data[Gun_No].MC_Step)
		{
			printf("55555xxxxxx Plug%d MC_Step=0x%x\n",Gun_No,Globa->Control_DC_Infor_Data[Gun_No].MC_Step);
			pre_MC_Step = Globa->Control_DC_Infor_Data[Gun_No].MC_Step;
		}
		
		
		if(Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_setflg == 1)
		{ //进行升级
		//	printf("44444xxxxxx Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_setStep = %0x\n",Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_setStep);

			switch(Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_setStep)
			{
				  case 0x00:
				  {//第x块板子进行升级
						if(Gun_No > 0)
						{
							if(Globa->Control_Upgrade_Firmware_Info[Gun_No-1].firmware_info_setStep == 0x10)//前1个板子已升级
							{//第x块板更新完毕
								if(MC_All_Frame[Gun_No].MC_Recved_PGN21248_Flag == 1)
								{//升级请求报文响应报文
									MC_All_Frame[Gun_No].MC_Recved_PGN21248_Flag = 0;
									MC_timer_start(Gun_No,5000);
									Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_setStep = 0x01;
									Globa->Control_Upgrade_Firmware_Info[Gun_No].Send_Tran_All_Bytes_0K = 0; //重新计数
									break;
								}
								if(MC_timer_flag(Gun_No,5000) == 1)
								{//遥测响应帧 超过5秒
									MC_timer_stop(Gun_No,5000);
									Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_setStep = 0x10;
									Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_success = 1;
									break;
								}
								if(MC_timer_flag(Gun_No,250) == 1)
								{//下发升级请求报文
									MC_timer_start(Gun_No,250);
									MC_sent_data(fd, PGN20992_ID_MC, 1, Gun_No+1);  //下发升级请求报文
									Globa->Control_Upgrade_Firmware_Info[Gun_No].Send_Tran_All_Bytes_0K = 0; //重新计数
									Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_success = 0;
								} 
							}else
							{
								MC_timer_start(Gun_No,250);
								MC_timer_start(Gun_No,5000);
							}
						}else//第1号板子
						{
							if(MC_All_Frame[0].MC_Recved_PGN21248_Flag == 1)
							{//升级请求报文响应报文
								MC_All_Frame[0].MC_Recved_PGN21248_Flag = 0;
								MC_timer_start(0,5000);
								Globa->Control_Upgrade_Firmware_Info[0].firmware_info_setStep = 0x01;
								Globa->Control_Upgrade_Firmware_Info[0].Send_Tran_All_Bytes_0K = 0; //重新计数
								break;
							}
							if(MC_timer_flag(0,5000) == 1)
							{//遥测响应帧 超过5秒
								MC_timer_stop(0,5000);
								Globa->Control_Upgrade_Firmware_Info[0].firmware_info_setStep = 0x10;
								Globa->Control_Upgrade_Firmware_Info[0].firmware_info_success = 1;
								break;
							}
							if(MC_timer_flag(0,250) == 1)
							{//下发升级请求报文
								MC_timer_start(0,250);
								MC_sent_data(fd, PGN20992_ID_MC, 1, 1);  //下发升级请求报文
								Globa->Control_Upgrade_Firmware_Info[0].Send_Tran_All_Bytes_0K = 0; //重新计数
								Globa->Control_Upgrade_Firmware_Info[0].firmware_info_success = 0;
							}
						}
						break;
					}
					case 0x01:
					{
						if(MC_All_Frame[Gun_No].MC_Recved_PGN21504_Flag == 1){ //请求升级数据报文帧
							MC_All_Frame[Gun_No].MC_Recved_PGN21504_Flag = 0;
							MC_timer_start(Gun_No,5000);
							MC_timer_start(Gun_No,250);
							MC_sent_data(fd, PGN21760_ID_MC, 0, Gun_No+1);
						}
						if(MC_All_Frame[Gun_No].MC_Recved_PGN22016_Flag == 1){ //请求升级数据报文帧
							MC_All_Frame[Gun_No].MC_Recved_PGN22016_Flag = 0;
							MC_timer_start(Gun_No,5000);
							MC_sent_data(fd, PGN21760_ID_MC, 1, Gun_No+1);
							Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_setStep = 0x02;
							MC_timer_start(Gun_No,250);
						}
						if(MC_timer_flag(Gun_No,5000) == 1){//遥测响应帧 超过5秒
							MC_timer_start(Gun_No,5000);
							Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_setStep = 0x10;
							Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_success = 1;
							break;
						}
						break;
					}
					case 0x02:{//升级包文件下发
						if(MC_All_Frame[Gun_No].MC_Recved_PGN22016_Flag == 1){ //请求升级数据报文帧
							MC_All_Frame[Gun_No].MC_Recved_PGN22016_Flag = 0;
							MC_timer_start(Gun_No,5000);
							MC_timer_start(Gun_No,250);
							MC_sent_data(fd, PGN21760_ID_MC, 1, Gun_No+1);
						}
						if(MC_timer_flag(Gun_No,5000) == 1){//遥测响应帧 超过5秒
							MC_timer_stop(Gun_No,5000);
							Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_setStep = 0x10;
							Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_success = 1;
							break;
						}else if(MC_timer_flag(Gun_No,250) == 1){//如果下面控制板还没有
							MC_timer_start(Gun_No,250);
							MC_sent_data(fd, PGN21760_ID_MC, 1, Gun_No+1);
							break;
						}
						break;
					}
					case 0x10:{//升级完毕，不管是失败还是成功

						break;
					}
				}
		}else//非升级中
		{
			if(((Globa->Control_DC_Infor_Data[Gun_No].Error_ctl == 0x00)&&(Globa->Control_DC_Infor_Data[Gun_No].ctl_state == 0x00))	||
				((Globa->Control_DC_Infor_Data[Gun_No].Error_ctl != 0x00)&&(Globa->Control_DC_Infor_Data[Gun_No].ctl_state == 0x01))
				)
				{//判断计量单元统计的控制器的故障数量是否与控制器的状态一致
					Error_equal = 0;
					MC_timer_stop(Gun_No,5001);
				}else
				{
					if(Error_equal == 0)
					{
						Error_equal = 1;
						MC_timer_start(Gun_No,5001);
					}
				}
				
			if((MC_timer_flag(Gun_No,400) == 1)&&(0==Gun_No))//定时400ms发送一轮模块输出信息帧
			{
				//MC_sent_data(fd, PGN16512_ID_PC, 1, Gun_No+1);
				MC_timer_start(Gun_No,400);
				if(Globa->Charger_param.gun_config > 1)
				{
					int i=0;
					for(i=0;i<Globa->Charger_param.gun_config;i++)
						MC_sent_data(fd, PGN16512_ID_PC, 1, i+1);
				}
			}
			if((Globa->Charger_param.gun_config < CONTROL_DC_NUMBER)&&(0==Gun_No))//只枪A线程跑===
				host_hand_1();
			else if((Globa->Charger_param.gun_config == CONTROL_DC_NUMBER)&&(0==Gun_No))//只枪A线程跑===
			{
				host_hand_5_gun();//五枪多枪专用函数
			}
				
				if(MC_timer_flag(Gun_No,5001) == 1)
				{//超时5秒
				//	memset(&Globa->Control_DC_Infor_Data[Gun_No].Error.emergency_switch, 0x00, (&Globa->Control_DC_Infor_Data[Gun_No].Error.reserve30 - &Globa->Control_DC_Infor_Data[Gun_No].Error.emergency_switch));//为了保持与计量单元的故障记录计数同步，把标志全部清零
					//Globa->Error_ctl = 0;

				//	system("echo Globa->Control_DC_Infor_Data[Gun_No].Error_ctl != 0x00)&&(Globa->Control_DC_Infor_Data[Gun_No].ctl_state != 0x11)) >> /mnt/systemlog");
					//sleep(3);
					//system("reboot");
				}

				if((MC_All_Frame[Gun_No].MC_Recved_PGN8704_Flag == 0x01)||
				   (MC_All_Frame[Gun_No].MC_Recved_PGN8448_Flag == 1)
				  )
				{//遥测帧
					if(MC_All_Frame[Gun_No].MC_Recved_PGN8704_Flag == 1)
					{
						MC_All_Frame[Gun_No].MC_Recved_PGN8704_Flag = 0;
						MC_sent_data(fd, PGN8960_ID_MC, 1, Gun_No+1);  //遥测响应帧

						/* if((Globa->Control_DC_Infor_Data[Gun_No].line_v > 32)&&(Globa->Control_DC_Infor_Data[Gun_No].line_v < 48)){
							Globa->Control_DC_Infor_Data[Gun_No].gun_link = 1;
						}else
						{
							Globa->Control_DC_Infor_Data[Gun_No].gun_link = 0;    		
						} */
					}
					MC_All_Frame[Gun_No].MC_Recved_PGN8448_Flag = 0;
					MC_timer_start(Gun_No,5002);
					ctr_connect_count = 0;
					if(Globa->Control_DC_Infor_Data[Gun_No].Error.ctrl_connect_lost != 0)
					{
						Globa->Control_DC_Infor_Data[Gun_No].Error.ctrl_connect_lost = 0;
						sent_warning_message(0x98, 53, Gun_No+1, 0);
						Globa->Control_DC_Infor_Data[Gun_No].Error_system--;
					}
				}else if(MC_timer_flag(Gun_No,5002) == 1)
				{//超时5秒
					ctr_connect_count++;
					MC_timer_start(Gun_No,5002);
					if(ctr_connect_count >= 10)//TEST2)
					{
						if(Globa->Control_DC_Infor_Data[Gun_No].Error.ctrl_connect_lost != 1)
						{
							Globa->Control_DC_Infor_Data[Gun_No].Error.ctrl_connect_lost = 1;
							sent_warning_message(0x99, 53, Gun_No+1, 0);
							Globa->Control_DC_Infor_Data[Gun_No].Error_system++;
						}
						ctr_connect_count = 0;
					}
				}
				//test
				//printf("MC_timer_cur_tick(%d,5002)=%d\n",Gun_No,MC_timer_cur_tick(Gun_No,5002));
				
				if(MC_All_Frame[Gun_No].MC_Recved_PGN2816_Flag == 0x01)
				{//请求充电参数信息
					MC_All_Frame[Gun_No].MC_Recved_PGN2816_Flag = 0;

					MC_sent_data(fd, PGN2304_ID_MC, 1, Gun_No+1);  //下发充电参数信息
				}
				//if(Globa->Charger_param.Input_Relay_Ctl_Type != 0)
				{
					if(Globa->Control_DC_Infor_Data[Gun_No].Over_gun_link == 0)
					{
						if(Globa->Control_DC_Infor_Data[Gun_No].gun_link == 1)
						{
							AC_Relay_Ctl_Count = 0;
							Globa->Control_DC_Infor_Data[Gun_No].AC_Relay_control = 1;
						}
					}else
					{
						if(Globa->Control_DC_Infor_Data[Gun_No].gun_link == 0)
						{
							Globa->Control_DC_Infor_Data[Gun_No].Over_gun_link = 0;
						}
					}
					
					if(Globa->Control_DC_Infor_Data[Gun_No].charger_start == 1)
					{  //启动之后才开风扇
						Globa->Control_DC_Infor_Data[Gun_No].AC_Relay_control = 1;
						AC_Relay_Ctl_Count = 0;
					}else
					{
						AC_Relay_Ctl_Count++;
						if(AC_Relay_Ctl_Count>= 3000)
						{
							AC_Relay_Ctl_Count = 3000;
							Globa->Control_DC_Infor_Data[Gun_No].AC_Relay_control = 0;
						}
					}
				}
				
				if(Globa->Control_DC_Infor_Data[Gun_No].Operation_command == 2)
				{//进行解锁动作
					if(MC_All_Frame[Gun_No].MC_Recved_PGN3840_Flag == 1)
					{ //收到下发电子锁控制反馈
						MC_timer_stop(Gun_No,650);
						MC_All_Frame[Gun_No].MC_Recved_PGN3840_Flag = 0;
						Globa->Control_DC_Infor_Data[Gun_No].Operation_command = 0;
					}else
					{
						if(MC_timer_flag(Gun_No,650) == 1)
						{  //定时650MS
							MC_sent_data(fd, PGN3584_ID_MC, 2, Gun_No+1);  //下发充电参数信息
							MC_timer_start(Gun_No,650);
						}
					}
				}else
				{
					MC_timer_start(Gun_No,650);
				}
				//test
				// printf("Control_DC_Infor_Data[%d].SYS_Step=%d,cp_volt=%d,gun_link=%d\n",Gun_No,
							// Globa->Control_DC_Infor_Data[Gun_No].SYS_Step,
							// Globa->Control_DC_Infor_Data[Gun_No].line_v,
							// Globa->Control_DC_Infor_Data[Gun_No].gun_link
							// );
				
				switch(Globa->Control_DC_Infor_Data[Gun_No].SYS_Step)
				{
					case 0x00:{//---------------------------系统空闲状态00 -------------------
						
						MC_timer_start(Gun_No,250);
						MC_timer_stop(Gun_No,500);
						MC_timer_stop(Gun_No,1000);
						MC_timer_stop(Gun_No,3000);
						MC_timer_stop(Gun_No,5000);
						MC_timer_stop(Gun_No,60000);

						Globa->Control_DC_Infor_Data[Gun_No].SYS_Step = 0x01;

						break;
					}
					case 0x01:{//--------------------系统就绪状态01---------------------------
						if((Globa->Control_DC_Infor_Data[Gun_No].charger_start == 1)||
							(Globa->Control_DC_Infor_Data[Gun_No].charger_start == 2)||
							(Globa->Control_DC_Infor_Data[Gun_No].charger_start == 3)||//VIN start step1
							(Globa->Control_DC_Infor_Data[Gun_No].charger_start == 4)//VIN start step2
							)
						{//启动自动充电或手动充电
							MC_timer_stop(Gun_No,250);
							MC_timer_start(Gun_No,500);//控制灯闪烁
							MC_timer_start(Gun_No,1000);//启动电量累计功能

							Globa->Control_DC_Infor_Data[Gun_No].SYS_Step = 0x11;
							Globa->Control_DC_Infor_Data[Gun_No].MC_Step  = 0x01;
						}

						if(MC_All_Frame[Gun_No].MC_Recved_PGN3328_Flag == 1)
						{ //收到下发直流分流器量程变比的反馈信息
							Dc_shunt_Set_CL_flag[Gun_No] = 0;
							MC_All_Frame[Gun_No].MC_Recved_PGN3328_Flag = 0;
						}else
						{
							if(Dc_shunt_Set_CL_flag[Gun_No] == 1)
							{//下发分流器
								if(MC_timer_flag(Gun_No,250) == 1)
								{  //定时250MS
									MC_sent_data(fd, PGN3072_ID_MC, 1, Gun_No+1);  //下发充电参数信息
									MC_timer_start(Gun_No,250);
								}
							}
						}
				
						break;
					}
					case 0x11:{//--------------------系统充电状态 ----------------------------
						if(MC_timer_flag(Gun_No,1000) == 1)
						{	//定时1秒							
							Globa->Control_DC_Infor_Data[Gun_No].Time += 1;
							MC_timer_start(Gun_No,1000);
						}
						if(Globa->Control_DC_Infor_Data[Gun_No].charger_start == 4)
							Globa->Control_DC_Infor_Data[Gun_No].charger_start = 1;//
						// if(MC_timer_flag(Gun_No,500) == 1)
						// {  //定时500MS
							// MC_timer_start(Gun_No,500);													
						// }

						MC_control(fd, Gun_No);//充电应用逻辑控制（包括与控制器的通信、刷卡计费的处理、上层系统故障的处理）

						break;
					}
					case 0x21:{//--------------------充电结束状态处理 ------------------------
						Globa->Control_DC_Infor_Data[Gun_No].charger_start = 0;//充电停止
						Globa->Control_DC_Infor_Data[Gun_No].SYS_Step = 0x00;
						break;
					}
					default:
					{
						Globa->Control_DC_Infor_Data[Gun_No].charger_start = 0;//充电停止
						Globa->Control_DC_Infor_Data[Gun_No].SYS_Step = 0x00;
						//while(1);
						break;
					}
				}
			}
	
	}
	close(fd);
	exit(0);
}

/*********************************************************
**description：MC管理线程初始化及建立
***parameter  :none
***return		  :none
**********************************************************/
extern void 	MC_Manage_Thread(void* gun_no)
{
	pthread_t td1;
	int ret ,stacksize = 1024*1024;
	pthread_attr_t attr;

	ret = pthread_attr_init(&attr);
	if(ret != 0){
		return;
	}

	ret = pthread_attr_setstacksize(&attr, stacksize);
	if(ret != 0){
		return;
	}
	
	/* 创建自动串口抄收线程 */
	if(0 != pthread_create(&td1, &attr, (void *)MC_Manage_Task, gun_no)){
		perror("####pthread_create IC_Read_Write_Task ERROR ####\n\n");
	}

	ret = pthread_attr_destroy(&attr);
	if(ret != 0){
		return;
	}
}
