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

#include "globalvar.h"
#include "MC_can.h"

static UINT32 count_250ms=0,count_350ms=0,count_500ms=0,count_650ms=0,count_1s=0,count_3s_or5s=0,count_5s=0,count_5s1=0,count_5s2=0,count_1min=0;
static UINT32 count_250ms_flag=0,count_350ms_flag=0,count_500ms_flag=0,count_650ms_flag=0,count_1s_flag=0,count_3s_or5s_flag=0,count_5s_flag=0,count_5s1_flag=0,count_5s2_flag=0,count_1min_flag=0;
static UINT32 count_250ms_enable=0,count_350ms_enable=0,count_500ms_enable=0,count_650ms_enable=0,count_1s_enable=0,count_3s_or5s_enable=0,count_5s_enable=0,count_5s1_enable=0,count_5s2_enable=0,count_1min_enable=0;

extern MC_ALL_FRAME MC_All_Frame_2;
extern UINT32 MC_Manage_Watchdog_2;
extern UINT32 Dc_shunt_Set_CL_flag_2;

extern void timer_start_2(int time)
{
	switch(time){
		case 250:{
			count_250ms = 0;
			count_250ms_flag = 0;
  		count_250ms_enable = 1;

			break;
		}	 
		case 350:{
			count_350ms = 0;
			count_350ms_flag = 0;
  		count_350ms_enable = 1;
			break;
		}
		case 500:{
			count_500ms = 0;
			count_500ms_flag = 0;
  		count_500ms_enable = 1;

			break;
		}
		case 650:{
			count_650ms = 0;
			count_650ms_flag = 0;
  		count_650ms_enable = 1;
			break;
		}
		case 1000:{
			count_1s = 0;
			count_1s_flag = 0;
  		count_1s_enable = 1;

			break;
		}
		case 3000:{
			count_3s_or5s = 0;
			count_3s_or5s_flag = 0;
  		count_3s_or5s_enable = 1;

			break;
		}
		case 5000:{
			count_5s = 0;
			count_5s_flag = 0;
  		count_5s_enable = 1;

			break;
		}
		case 5001:{
			count_5s1 = 0;
			count_5s1_flag = 0;
  		count_5s1_enable = 1;

			break;
		}
		case 5002:{
			count_5s2 = 0;
			count_5s2_flag = 0;
  		count_5s2_enable = 1;

			break;
		}
		case 60000:{
			count_1min = 0;
			count_1min_flag = 0;
  		count_1min_enable = 1;

			break;
		}
		default:{
			break;
		}
	}
}
extern void timer_stop_2(int time)
{
	switch(time){
		case 250:{
			count_250ms = 0;
			count_250ms_flag = 0;
  		count_250ms_enable = 0;

			break;
		}
		case 350:{
			count_350ms = 0;
			count_350ms_flag = 0;
  		count_350ms_enable = 0;
			break;
		}
		case 500:{
			count_500ms = 0;
			count_500ms_flag = 0;
  		count_500ms_enable = 0;

			break;
		}
		case 650:{
			count_650ms = 0;
			count_650ms_flag = 0;
  		count_650ms_enable = 0;
			break;
		}
		case 1000:{
			count_1s = 0;
			count_1s_flag = 0;
  		count_1s_enable = 0;

			break;
		}
		case 3000:{
			count_3s_or5s = 0;
			count_3s_or5s_flag = 0;
  		count_3s_or5s_enable = 0;

			break;
		}
		case 5000:{
			count_5s = 0;
			count_5s_flag = 0;
  		count_5s_enable = 0;

			break;
		}
		case 5001:{
			count_5s1 = 0;
			count_5s1_flag = 0;
  		count_5s1_enable = 0;

			break;
		}
		case 5002:{
			count_5s2 = 0;
			count_5s2_flag = 0;
  		count_5s2_enable = 0;

			break;
		}
		case 60000:{
			count_1min = 0;
			count_1min_flag = 0;
  		count_1min_enable = 0;

			break;
		}

		default:{
			break;
		}
	}
}
extern UINT32 timer_tick_2(int time)
{
	UINT32 flag=0;

	switch(time){
		case 250:{
			flag = count_250ms_flag;

			break;
		}	
		case 350:{
			flag = count_350ms_flag;
			break;
		}
		case 500:{
			flag = count_500ms_flag;

			break;
		}
		case 650:{
			flag = count_650ms_flag;
			break;
		}
		case 1000:{
			flag = count_1s_flag;

			break;
		}
		case 3000:{
			flag = count_3s_or5s_flag;

			break;
		}
		case 5000:{
			flag = count_5s_flag;

			break;
		}
		case 5001:{
			flag = count_5s1_flag;

			break;
		}
		case 5002:{
			flag = count_5s2_flag;

			break;
		}
		case 60000:{
			flag = count_1min_flag;

			break;
		}

		default:{
			break;
		}
	}

	return (flag);
}

/*********************************************************
**description：MC管理线程
***parameter  :none
***return		  :none
**********************************************************/
static void MC_Manage_Task_2(void)
{
	INT32 fd;
	UINT32 temp1=0xff,temp2=0xff;
	UINT32 Error_equal = 0,ctr2_connect_count = 0;
	UINT32 Operation_command_cont = 0;//为了兼容以前的，最多连续发送10次
	time_t systime=0,last_systime=0;

	timer_stop_2(250);
	timer_stop_2(500);//控制灯闪烁
	timer_stop_2(1000);//启动电量累计功能
	timer_stop_2(3000);
	timer_stop_2(5000);
	timer_stop_2(5001);//充电控制器故障统计计时
	timer_start_2(5002);//遥测帧计时
	timer_stop_2(60000);
	timer_stop_2(650);//控制灯闪烁

	Globa_2->charger_start = 0;//充电停止

	GROUP1_RUN_LED_OFF;
	GROUP2_RUN_LED_OFF;

	Globa_2->Error_charger = 0;//充电错误尝试次数
	Globa_2->Time = 0;//充电时间

	Globa_2->SYS_Step = 0x00;

	if((fd = BMS_can_fd_init(GUN_CAN_COM_ID1)) < 0){
		while(1);
	}
	//在指定的CAN_RAW套接字上禁用接收过滤规则,则该线程只发送，不会接收数据，省内存和CPU时间
	setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FILTER, NULL, 0);
	prctl(PR_SET_NAME,(unsigned long)"MC2_Manage_Task");//设置线程名字 

	while(1){
		usleep(50000);//做基准时钟调度10ms
		MC_Manage_Watchdog_2 = 1;

    if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候{//同时充电和轮流充电才运行
			if(count_250ms_enable == 1){//使能250MS 计时
				if(count_250ms_flag != 1){//计时时间未到
					count_250ms++;
					if(count_250ms >= 25){
						count_250ms_flag = 1;
					}
				}
			}
			if(count_350ms_enable == 1){//使能250MS 计时
				if(count_350ms_flag != 1){//计时时间未到
					count_350ms++;
					if(count_350ms >= 7){
						count_350ms_flag = 1;
					}
				}
			}
			if(count_500ms_enable == 1){//使能500MS 计时
				if(count_500ms_flag != 1){//计时时间未到
					count_500ms++;
					if(count_500ms >= 10){
						Globa_2->led_run_flag = !Globa_2->led_run_flag;
						count_500ms_flag = 1;
					}
				}
			}
			if(count_650ms_enable == 1){//使能650MS 计时
				if(count_650ms_flag != 1){//计时时间未到
					count_650ms++;
					if(count_650ms >= 13){
						count_650ms_flag = 1;
					}
				}
			}
			if(count_1s_enable == 1){//使能 1秒 计时
				if(count_1s_flag != 1){//计时时间未到
					systime = time(NULL);        //获得系统的当前时间,从1970.1.1 00:00:00 到现在的秒数
					if(systime != last_systime){
						last_systime = systime;
						count_1s_flag = 1;
					}
				}
			}
			if(count_3s_or5s_enable == 1){//使能 3秒 计时 --
				if(count_3s_or5s_flag != 1){//计时时间未到
					count_3s_or5s++;
					if(count_3s_or5s >= 200){ //600
						count_3s_or5s_flag = 1;
					}
				}
			}
			if(count_5s_enable == 1){//使能 5秒 计时
				if(count_5s_flag != 1){//计时时间未到
					count_5s++;
					if(count_5s >= 100){
						count_5s_flag = 1;
					}
				}
			}
			if(count_5s1_enable == 1){//使能 5秒 计时
				if(count_5s1_flag != 1){//计时时间未到
					count_5s1++;
					if(count_5s1 >= 100){
						count_5s1_flag = 1;
					}
				}
			}
			if(count_5s2_enable == 1){//使能 5秒 计时
				if(count_5s2_flag != 1){//计时时间未到
					count_5s2++;
					if(count_5s2 >= 100){
						count_5s2_flag = 1;
					}
				}
			}
			if(count_1min_enable == 1){//使能 60秒 计时
				if(count_1min_flag != 1){//计时时间未到
					count_1min++;
					if(count_1min >= 1200){
						count_1min_flag = 1;
					}
				}
			}

			if(Globa_2->Control_Upgrade_Firmware_Info.firmware_info_setflg == 1){ //进行升级
					sleep(2);
			}else{
				if(((Globa_2->Error_ctl == 0x00)&&(Globa_2->ctl_state == 0x00))||((Globa_2->Error_ctl != 0x00)&&(Globa_2->ctl_state == 0x01))){//判断计量单元统计的控制器的故障数量是否与控制器的状态一致
					Error_equal = 0;
					timer_stop_2(5001);
				}else{
					if(Error_equal == 0){
						Error_equal = 1;
						timer_start_2(5001);
					}
				}

				/*if(timer_tick_2(5001) == 1){//超时5秒
					memset(&Globa_2->Error.emergency_switch, 0x00, (&Globa_2->Error.reserve30 - &Globa_2->Error.emergency_switch));//为了保持与计量单元的故障记录计数同步，把标志全部清零
					//Globa_2->Error_ctl = 0;

					system("echo Globa_2->Error_ctl != 0x00)&&(Globa_2->ctl_state != 0x11)) >> /mnt/systemlog");
					sleep(3);
					system("reboot");
				}*/

				if((MC_All_Frame_2.MC_Recved_PGN8704_Flag == 0x01)||(MC_All_Frame_2.MC_Recved_PGN8448_Flag == 1)){//遥测帧
					if(MC_All_Frame_2.MC_Recved_PGN8704_Flag == 1){
						MC_All_Frame_2.MC_Recved_PGN8704_Flag = 0;
						MC_sent_data_2(fd, PGN8960_ID_MC, 1, 2);  //遥测响应帧
						if((Globa_2->line_v > 32)&&(Globa_2->line_v < 48)){
							Globa_2->gun_link = 1;
						}else{
							Globa_2->gun_link = 0;    		
						}
					}
					MC_All_Frame_2.MC_Recved_PGN8448_Flag = 0;
					timer_start_2(5002);
					ctr2_connect_count = 0;
					if(Globa_2->Error.ctrl_connect_lost != 0){
						Globa_2->Error.ctrl_connect_lost = 0;
						sent_warning_message(0x98, 53, 2, 0);
						Globa_2->Error_system--;
					}
				}else if(timer_tick_2(5002) == 1){//超时5秒
					ctr2_connect_count++;
					timer_start_2(5002);
					if(ctr2_connect_count >= 2){
						if(Globa_2->Error.ctrl_connect_lost != 1){
							Globa_2->Error.ctrl_connect_lost = 1;
							sent_warning_message(0x99, 53, 2, 0);
							Globa_2->Error_system++;
						}
						ctr2_connect_count = 0;
					}
				}
				if(MC_All_Frame_2.MC_Recved_PGN2816_Flag == 0x01){//请求充电参数信息
					MC_All_Frame_2.MC_Recved_PGN2816_Flag = 0;

					MC_sent_data_2(fd, PGN2304_ID_MC, 1, 2);  //下发充电参数信息
				}
				
			  if(Globa_2->Operation_command == 2){//进行解锁动作
					if(MC_All_Frame_2.MC_Recved_PGN3840_Flag == 1){ //收到下发电子锁控制反馈
						timer_stop_2(650);
						MC_All_Frame_2.MC_Recved_PGN3840_Flag = 0;
						Globa_2->Operation_command = 0;
						Operation_command_cont = 0;
					}else{
						if(timer_tick_2(650) == 1){  //定时350MS
							MC_sent_data_2(fd, PGN3584_ID_MC, 2, 2);  //下发充电参数信息
							timer_start_2(650);
							Operation_command_cont++;
							if(Operation_command_cont >= 10){
								Operation_command_cont = 0;
								Globa_2->Operation_command = 0;
								timer_stop_2(650);
								MC_All_Frame_2.MC_Recved_PGN3840_Flag = 0;
							}
						}
					}
				}else{
					timer_start_2(650);
				}
		
				switch(Globa_2->SYS_Step){
					case 0x00:{//---------------------------系统空闲状态00 -------------------
						GROUP2_RUN_LED_OFF;

						timer_start_2(250);
						timer_stop_2(500);
						timer_stop_2(1000);
						timer_stop_2(3000);
						timer_stop_2(5000);
						timer_stop_2(60000);

						Globa_2->Error_charger = 0;//充电错误尝试次数

						Globa_2->SYS_Step = 0x01;

						break;
					}
					case 0x01:{//--------------------系统就绪状态01---------------------------
						if((Globa_2->charger_start == 1)||(Globa_2->charger_start == 2)){//启动自动充电或手动充电
							timer_stop_2(250);
							timer_start_2(500);//控制灯闪烁
							timer_start_2(1000);//启动电量累计功能
							Globa_2->SYS_Step = 0x11;
							Globa_2->MC_Step  = 0x01;
						}
					
						if(MC_All_Frame_2.MC_Recved_PGN3328_Flag == 1){ //收到下发直流分流器量程变比的反馈信息
							Dc_shunt_Set_CL_flag_2 = 0;
							MC_All_Frame_2.MC_Recved_PGN3328_Flag = 0;
						}else{
							if(Dc_shunt_Set_CL_flag_2 == 1){//下发CT变比
								if(timer_tick_2(250) == 1){  //定时250MS
									MC_sent_data_2(fd, PGN3072_ID_MC, 1, 2);  //下发充电参数信息
									timer_start_2(250);
								}
							}
						}
				
						break;
					}
					case 0x11:{//--------------------系统充电状态 ----------------------------
						if(timer_tick_2(1000) == 1){//定时1秒
							/*if(Globa_2->Charger_param.meter_config_flag == 0){
								Globa_2->soft_KWH  += (Globa_2->Output_voltage/100)*(Globa_2->Output_current/100)/36000; // 1s  累计一次 电量度  4位小数
							}*/

							Globa_2->Time += 1;
							timer_start_2(1000);
						}
						
            if(Globa_1->Charger_param.LED_Type_Config == 3)
						{
							if(timer_tick_2(500) == 1){  //定时500MS
								timer_start_2(500);
								
								if(Globa_2->led_run_flag != 0){//点亮（灯闪烁）
									GROUP2_RUN_LED_ON;
								}else{
									GROUP2_RUN_LED_OFF;
								}
							}
					  }

						MC_control_2(fd);//充电应用逻辑控制（包括与控制器的通信、刷卡计费的处理、上层系统故障的处理）

						break;
					}
					case 0x21:{//--------------------充电结束状态处理 ------------------------
						Globa_2->charger_start = 0;//充电停止
						Globa_2->SYS_Step = 0x00;

						break;
					}
					default:{
						Globa_2->charger_start = 0;//充电停止
						GROUP2_RUN_LED_OFF;

						while(1);
						break;
						}
					}
				}
		}else{
			sleep(5);
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
extern void 	MC_Manage_Thread_2(void)
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
	if(0 != pthread_create(&td1, &attr, (void *)MC_Manage_Task_2, NULL)){
		perror("####pthread_create IC_Read_Write_Task ERROR ####\n\n");
	}

	ret = pthread_attr_destroy(&attr);
	if(ret != 0){
		return;
	}
}
