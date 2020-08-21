/*******************************************************************************
  Copyright(C)    2014-2016,      EAST Co.Ltd.
  File name :     warn.c
  Author :        dengjh
  Version:        1.0
  Date:           2016.02.26
  Description:    
  Other:          
  Function List:  
  History:        
          <author>   <time>   <version>   <desc>
            dengjh    2016.02.26   1.0      Create
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



extern unsigned char ac_out;
extern time_t ac_out_sec;
time_t model_sec;

/*********************************************************
**description：处理充电控制器传送过来的充电机充电状态
***parameter  :none
***return		  :none
**********************************************************/
extern void MC_state_deal(UINT32 gun, UINT32 state)
{
	sent_warning_message(0x94, state, gun, 0);
}

/*********************************************************
**description：处理充电控制器传送过来的告警
***parameter  :gun---从1开始
***return		  :none
**********************************************************/
extern void MC_alarm_deal(UINT32 gun, UINT32 state, UINT32 alarm_sn, UINT32 fault, UINT32 num)
{
	int i=0,lost_cnt;
  printf("description：处理充电控制器传送过来的告警 gun =%d state=%d alarm_sn=%0x num =%d \n",gun,state, alarm_sn,num);
	if((gun < 1)||(gun > CONTROL_DC_NUMBER))//error
		return;
	switch(alarm_sn)
	{
		case 0x01:{//急停按下故障==关系到所有枪
			if(gun == 1)
			{
				if(Globa->Control_DC_Infor_Data[0].Error.emergency_switch != fault)
				{
					for(i=0;i<CONTROL_DC_NUMBER;i++)
						Globa->Control_DC_Infor_Data[i].Error.emergency_switch = fault;
					
					if(Globa->Control_DC_Infor_Data[0].Error.emergency_switch == 1)
					{
						sent_warning_message(0x99, 1, 0, 0);
						for(i=0;i<CONTROL_DC_NUMBER;i++)
						{
							Globa->Control_DC_Infor_Data[i].Error_system++;
							Globa->Control_DC_Infor_Data[i].Error_ctl++;
							Globa->Control_DC_Infor_Data[i].Operation_command = 2;//进行解锁
						}						
					}else
					{
						sent_warning_message(0x98, 1, 0, 0);
						
						for(i=0;i<CONTROL_DC_NUMBER;i++)
						{
							if(Globa->Control_DC_Infor_Data[i].Error_ctl > 0)
								Globa->Control_DC_Infor_Data[i].Error_ctl--;
							if(Globa->Control_DC_Infor_Data[i].Error_system > 0)
								Globa->Control_DC_Infor_Data[i].Error_system--;
						}
					}
				}
			}
			break;
		}
		case 0x02:{//系统接地故障
			if(Globa->Control_DC_Infor_Data[gun - 1].Error.PE_Fault != fault)
			{
				Globa->Control_DC_Infor_Data[gun - 1].Error.PE_Fault = fault;
				if(Globa->Control_DC_Infor_Data[gun - 1].Error.PE_Fault == 1)
				{
					sent_warning_message(0x99, 2, gun, 0);
					Globa->Control_DC_Infor_Data[gun - 1].Error_system++;
					Globa->Control_DC_Infor_Data[gun - 1].Error_ctl++;
				}else
				{
					sent_warning_message(0x98, 2, gun, 0);					
					Globa->Control_DC_Infor_Data[gun - 1].Error_system--;
					Globa->Control_DC_Infor_Data[gun - 1].Error_ctl--;
				}
			}
			break;
		}
		case 0x03:{//测量芯片通信故障
			if(Globa->Control_DC_Infor_Data[gun - 1].Error.Meter_Fault != fault)
			{
				Globa->Control_DC_Infor_Data[gun - 1].Error.Meter_Fault = fault;
				if(Globa->Control_DC_Infor_Data[gun - 1].Error.Meter_Fault == 1)
				{
					sent_warning_message(0x99, 3, gun, 0);
					Globa->Control_DC_Infor_Data[gun - 1].Error_system++;
					Globa->Control_DC_Infor_Data[gun - 1].Error_ctl++;
				}else
				{
					sent_warning_message(0x98, 3, gun, 0);
					Globa->Control_DC_Infor_Data[gun - 1].Error_system--;
					Globa->Control_DC_Infor_Data[gun - 1].Error_ctl--;
				}
			}
			break;
		}
		case 0x04:{//交流采集模块通信中断
			if(gun == 1)//交流采集模块只在控制板1
			{
				if(Globa->Control_DC_Infor_Data[0].Error.AC_connect_lost != fault)
				{
					for(i=0;i<CONTROL_DC_NUMBER;i++)
						Globa->Control_DC_Infor_Data[i].Error.AC_connect_lost = fault;
					
					if(Globa->Control_DC_Infor_Data[0].Error.AC_connect_lost == 1)
					{
						sent_warning_message(0x99, 4, 0, 0);
						for(i=0;i<CONTROL_DC_NUMBER;i++)
						{	
							Globa->Control_DC_Infor_Data[i].Error_system++;						
							Globa->Control_DC_Infor_Data[i].Error_ctl++;
						}
					}else
					{
						sent_warning_message(0x98, 4, 0, 0);					
						for(i=0;i<CONTROL_DC_NUMBER;i++)
						{
							if(Globa->Control_DC_Infor_Data[i].Error_ctl > 0)
								Globa->Control_DC_Infor_Data[i].Error_ctl--;
							if(Globa->Control_DC_Infor_Data[i].Error_system > 0)
								Globa->Control_DC_Infor_Data[i].Error_system--;
						}
					}
				}
			}
			break;
		}
		case 0x05:{//充电模块通信中断
		model_sec = time(NULL);
		if(ac_out == 1 && (model_sec - ac_out_sec > 60))//交流接触器闭合和风扇闭合
			{
				if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_connect_lost[num] != fault)
				{
					Globa->Control_DC_Infor_Data[gun - 1].Error.charg_connect_lost[num] = fault;
					if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_connect_lost[num] == 1)
					{
						sent_warning_message(0x99, 5, gun, num);
						Globa->Control_DC_Infor_Data[gun - 1].Warn_system++;
						
						lost_cnt = 0;
						for(i=0;i< Globa->Charger_param.Charge_rate_number[gun - 1];i++)
						{
							if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_connect_lost[i])
								lost_cnt++;
						}
						
						if(lost_cnt == Globa->Charger_param.Charge_rate_number[gun - 1])//all lost
						{
							Globa->Control_DC_Infor_Data[gun - 1].Error_system++;
							Globa->Control_DC_Infor_Data[gun - 1].Error_ctl++;
						}
					}else
					{
						sent_warning_message(0x98, 5, gun, num);
						Globa->Control_DC_Infor_Data[gun - 1].Warn_system--;
						if(Globa->Control_DC_Infor_Data[gun - 1].Error_system >0)
							Globa->Control_DC_Infor_Data[gun - 1].Error_system--;
						if(Globa->Control_DC_Infor_Data[gun - 1].Error_ctl > 0)
							Globa->Control_DC_Infor_Data[gun - 1].Error_ctl--;
						
					}
				}
			}
			break;
		}
		case 0x06:{//充电模块故障
		model_sec = time(NULL);
		if(ac_out == 1 && (model_sec - ac_out_sec > 60))//交流接触器闭合和风扇闭合
		{
			if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_fault[num] != fault){
				Globa->Control_DC_Infor_Data[gun - 1].Error.charg_fault[num] = fault;
    		if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_fault[num] == 1){
    			sent_warning_message(0x99, 6, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system++;
    		}else{
    			sent_warning_message(0x98, 6, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system--;
    		}
    	}
		}
			break;
		}
		case 0x07:{//充电模块风扇故障
		model_sec = time(NULL);
		if(ac_out == 1 && (model_sec - ac_out_sec > 60))//交流接触器闭合和风扇闭合
		{
			if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_fun_fault[num] != fault){
				Globa->Control_DC_Infor_Data[gun - 1].Error.charg_fun_fault[num] = fault;
    		if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_fun_fault[num] == 1){
    			sent_warning_message(0x99, 7, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system++;
    		}else{
    		  sent_warning_message(0x98, 7, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system--;
    		}
    	}
		}
			break;
		}
		case 0x08:{//充电模块交流输入告警
			model_sec = time(NULL);
		if(ac_out == 1 && (model_sec - ac_out_sec > 60))//交流接触器闭合和风扇闭合
		{
			if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_AC_alarm[num] != fault){
				Globa->Control_DC_Infor_Data[gun - 1].Error.charg_AC_alarm[num] = fault;
    		if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_AC_alarm[num] == 1){
    		  sent_warning_message(0x99, 8, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system++;
    		}else{
    			sent_warning_message(0x98, 8, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system--;
    		}
    	}
		}
			break;
		}
		case 0x09:{//充电模块输出短路故障
			model_sec = time(NULL);
		if(ac_out == 1 && (model_sec - ac_out_sec > 10))//交流接触器闭合和风扇闭合
		{
			if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_out_shout[num] != fault){
				Globa->Control_DC_Infor_Data[gun - 1].Error.charg_out_shout[num] = fault;
    		if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_out_shout[num] == 1){
    			sent_warning_message(0x99, 9, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system++;
    		}else{
    			sent_warning_message(0x98, 9, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system--;
    		}
    	}
		}
			break;
		}
		case 0x0A:{//充电模块输出过流告警
			if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_out_OC[num] != fault){
				Globa->Control_DC_Infor_Data[gun - 1].Error.charg_out_OC[num] = fault;
    		if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_out_OC[num] == 1){
    		  sent_warning_message(0x99, 10, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system++;
    		}else{
    			sent_warning_message(0x98, 10, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system--;
    		}
    	}
			break;
		}
		case 0x0B:{//充电模块输出过压告警
			if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_out_OV[num] != fault){
				Globa->Control_DC_Infor_Data[gun - 1].Error.charg_out_OV[num] = fault;
    		if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_out_OV[num] == 1){
    		  sent_warning_message(0x99, 11, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system++;
    		}else{
    			sent_warning_message(0x98, 11, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system--;
    		}
    	}
			break;
		}
		case 0x0C:{//充电模块输出欠压告警
			if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_out_LV[num] != fault){
				Globa->Control_DC_Infor_Data[gun - 1].Error.charg_out_LV[num] = fault;
    		if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_out_LV[num] == 1){
    			sent_warning_message(0x99, 12, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system++;
    		}else{
    		  sent_warning_message(0x98, 12, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system--;
    		}
    	}
			break;
		}
		case 0x0D:{//充电模块输入过压告警
		model_sec = time(NULL);
		if(ac_out == 1 && (model_sec - ac_out_sec > 60))//交流接触器闭合和风扇闭合
		{
			if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_in_OV[num] != fault){
				Globa->Control_DC_Infor_Data[gun - 1].Error.charg_in_OV[num] = fault;
    		if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_in_OV[num] == 1){
    		  sent_warning_message(0x99, 13, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system++;
    		}else{
    			sent_warning_message(0x98, 13, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system--;
    		}
    	}
		}
			break;
		}
		case 0x0E:{//充电模块输入欠压告警
			model_sec = time(NULL);
		if(ac_out == 1 && (model_sec - ac_out_sec > 60))//交流接触器闭合和风扇闭合
		{
		if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_in_LV[num] != fault){
				Globa->Control_DC_Infor_Data[gun - 1].Error.charg_in_LV[num] = fault;
    		if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_in_LV[num] == 1){
    		  sent_warning_message(0x99, 14, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system++;
    		}else{
    			sent_warning_message(0x98, 14, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system--;
    		}
    	}
		}
			break;
		}
		case 0x0F:{//充电模块输入缺相告警
		model_sec = time(NULL);
		if(ac_out == 1 && (model_sec - ac_out_sec > 30))//交流接触器闭合和风扇闭合
		{
			if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_in_lost[num] != fault){
				Globa->Control_DC_Infor_Data[gun - 1].Error.charg_in_lost[num] = fault;
    		if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_in_lost[num] == 1){
    			sent_warning_message(0x99, 15, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system++;
    		}else{
    			sent_warning_message(0x98, 15, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system--;
    		}
    	}
		}
			break;
		}
		case 0x10:{//充电模块过温告警
			if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_over_tmp[num] != fault){
				Globa->Control_DC_Infor_Data[gun - 1].Error.charg_over_tmp[num] = fault;
    		if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_over_tmp[num] == 1){
    			sent_warning_message(0x99, 16, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system++;
    		}else{
    			sent_warning_message(0x98, 16, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system--;
    		}
    	}
			break;
		}
		case 0x11:{//充电模块过压关机告警
		model_sec = time(NULL);
		if(ac_out == 1 && (model_sec - ac_out_sec > 30))//交流接触器闭合和风扇闭合
		{
			if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_OV_down[num] != fault){
				Globa->Control_DC_Infor_Data[gun - 1].Error.charg_OV_down[num] = fault;
    		if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_OV_down[num] == 1){
    			sent_warning_message(0x99, 17, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system++;
    		}else{
    			sent_warning_message(0x98, 17, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system--;
    		}
    	}
		}
			break;
		}
		case 0x12:{//充电模块其他告警
			model_sec = time(NULL);
		if(ac_out == 1 && (model_sec - ac_out_sec > 60))//交流接触器闭合和风扇闭合
		{
			if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_other_alarm[num] != fault){
				Globa->Control_DC_Infor_Data[gun - 1].Error.charg_other_alarm[num] = fault;
    		if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_other_alarm[num] == 1){
    			sent_warning_message(0x99, 18, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system++;
    		}else{
    			sent_warning_message(0x98, 18, gun, num);
					Globa->Control_DC_Infor_Data[gun - 1].Warn_system--;
    		}
    	}
		}
			break;
		}
		case 0x13:{//系统输出过压告警
			if(Globa->Control_DC_Infor_Data[gun - 1].Error.output_over_limit != fault){
				Globa->Control_DC_Infor_Data[gun - 1].Error.output_over_limit = fault;
    		if(Globa->Control_DC_Infor_Data[gun - 1].Error.output_over_limit == 1){
    			sent_warning_message(0x99, 19, gun, 0);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system++;
    		}else{
    		  sent_warning_message(0x98, 19, gun, 0);
    		 	Globa->Control_DC_Infor_Data[gun - 1].Warn_system--;
    		}
    	}
			break;
		}
		case 0x14:{//系统输出欠压告警
			if(Globa->Control_DC_Infor_Data[gun - 1].Error.output_low_limit != fault){
				Globa->Control_DC_Infor_Data[gun - 1].Error.output_low_limit = fault;
    		if(Globa->Control_DC_Infor_Data[gun - 1].Error.output_low_limit == 1){
    		  sent_warning_message(0x99, 20, gun, 0);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system++;
    		}else{
    		  sent_warning_message(0x98, 20, gun, 0);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system--;
    		}
    	}
			break;
		}
		case 0x15:{//系统绝缘告警
			if(Globa->Control_DC_Infor_Data[gun - 1].Error.sistent_alarm != fault){
				Globa->Control_DC_Infor_Data[gun - 1].Error.sistent_alarm = fault;
    		if(Globa->Control_DC_Infor_Data[gun - 1].Error.sistent_alarm == 1){
    		  sent_warning_message(0x99, 21, gun, 0);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system++;
    		}else{
    		 sent_warning_message(0x98, 21, gun, 0);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system--;
    		}
    	}
			break;
		}
		case 0x16:{//交流输入过压告警
			if(gun == 1)
			{
				if(Globa->Control_DC_Infor_Data[0].Error.input_over_limit != fault)
				{
					for(i=0;i<CONTROL_DC_NUMBER;i++)
						Globa->Control_DC_Infor_Data[i].Error.input_over_limit = fault;
					
					if(Globa->Control_DC_Infor_Data[0].Error.input_over_limit == 1)
					{
						sent_warning_message(0x99, 22, 0, 0);
						for(i=0;i<CONTROL_DC_NUMBER;i++)
							Globa->Control_DC_Infor_Data[i].Warn_system++;
					}else
					{
						sent_warning_message(0x98, 22, 0, 0);
						for(i=0;i<CONTROL_DC_NUMBER;i++)
							Globa->Control_DC_Infor_Data[i].Warn_system--;
					}
				}
			}
			break;
		}
		case 0x17:{//交流输入过压保护
			if(gun == 1)
			{
				if(Globa->Control_DC_Infor_Data[0].Error.input_over_protect != fault)
				{
					for(i=0;i<CONTROL_DC_NUMBER;i++)
						Globa->Control_DC_Infor_Data[i].Error.input_over_protect = fault;
					
					if(Globa->Control_DC_Infor_Data[0].Error.input_over_protect == 1)
					{
						sent_warning_message(0x99, 23, 0, 0);
						for(i=0;i<CONTROL_DC_NUMBER;i++)
						{
							Globa->Control_DC_Infor_Data[i].Error_system++;						
							Globa->Control_DC_Infor_Data[i].Error_ctl++;
						}
					}else
					{
						sent_warning_message(0x98, 23, 0, 0);
						for(i=0;i<CONTROL_DC_NUMBER;i++)
						{
							Globa->Control_DC_Infor_Data[i].Error_system--;						
							Globa->Control_DC_Infor_Data[i].Error_ctl--;
						}
					}
				}
			}
			break;
		}
		case 0x18:{//交流输入欠压告警，更改为保护
			if(gun == 1)
			{
				if(Globa->Control_DC_Infor_Data[0].Error.input_lown_limit != fault)
				{
					for(i=0;i<CONTROL_DC_NUMBER;i++)
						Globa->Control_DC_Infor_Data[i].Error.input_lown_limit = fault;
					
					if(Globa->Control_DC_Infor_Data[0].Error.input_lown_limit == 1)
					{
						sent_warning_message(0x99, 24, 0, 0);
						for(i=0;i<CONTROL_DC_NUMBER;i++)
						{
							Globa->Control_DC_Infor_Data[i].Error_system++;						
							Globa->Control_DC_Infor_Data[i].Error_ctl++;
						}
					}else
					{
						sent_warning_message(0x98, 24, 0, 0);
						for(i=0;i<CONTROL_DC_NUMBER;i++)
						{
							Globa->Control_DC_Infor_Data[i].Error_system--;						
							Globa->Control_DC_Infor_Data[i].Error_ctl--;
						}
					}
				}
			}
			break;
		}
		case 0x19:{//交流输入开关跳闸
			if(gun == 1)
			{
				if(Globa->Control_DC_Infor_Data[0].Error.input_switch_off != fault)
				{
					for(i=0;i<CONTROL_DC_NUMBER;i++)
						Globa->Control_DC_Infor_Data[i].Error.input_switch_off = fault;
					
					if(Globa->Control_DC_Infor_Data[0].Error.input_switch_off == 1)
					{
						sent_warning_message(0x99, 25, 0, 0);
						for(i=0;i<CONTROL_DC_NUMBER;i++)
						{
							Globa->Control_DC_Infor_Data[i].Error_system++;						
							Globa->Control_DC_Infor_Data[i].Error_ctl++;
						}
					}else
					{
						sent_warning_message(0x98, 25, 0, 0);
						for(i=0;i<CONTROL_DC_NUMBER;i++)
						{
							Globa->Control_DC_Infor_Data[i].Error_system--;						
							Globa->Control_DC_Infor_Data[i].Error_ctl--;
						}
					}
				}
			}
			break;
		}
		case 0x1A:{//交流防雷器跳闸
			if(gun == 1)
			{
				if(Globa->Control_DC_Infor_Data[0].Error.fanglei_off != fault)
				{
					for(i=0;i<CONTROL_DC_NUMBER;i++)
						Globa->Control_DC_Infor_Data[i].Error.fanglei_off = fault;
					
					if(Globa->Control_DC_Infor_Data[0].Error.fanglei_off == 1)
					{
						sent_warning_message(0x99, 26, 0, 0);
						for(i=0;i<CONTROL_DC_NUMBER;i++)
						{
							Globa->Control_DC_Infor_Data[i].Error_system++;						
							Globa->Control_DC_Infor_Data[i].Error_ctl++;
						}
					}else
					{
						sent_warning_message(0x98, 26, 0, 0);
						for(i=0;i<CONTROL_DC_NUMBER;i++)
						{
							Globa->Control_DC_Infor_Data[i].Error_system--;						
							Globa->Control_DC_Infor_Data[i].Error_ctl--;
						}
					}
				}
			}
			break;
		}
		
		case 0x1B:{//输出熔丝异常
			if(Globa->Control_DC_Infor_Data[gun - 1].Error.output_switch_off[0]  != fault)
			{
				Globa->Control_DC_Infor_Data[gun - 1].Error.output_switch_off[0]  = fault;
				if(Globa->Control_DC_Infor_Data[gun - 1].Error.output_switch_off[0]  == 1)
				{
					sent_warning_message(0x99, 27, gun, 0);
					Globa->Control_DC_Infor_Data[gun - 1].Error_system++;
					//Globa->Error_system++;
					Globa->Control_DC_Infor_Data[gun - 1].Error_ctl++;
				}else
				{
					sent_warning_message(0x98, 27, gun, 0);
					Globa->Control_DC_Infor_Data[gun - 1].Error_system--;
					//Globa->Error_system--;
					Globa->Control_DC_Infor_Data[gun - 1].Error_ctl--;
				}
			}
			break;
		}
		case 0x1C:{//烟雾告警
			if(Globa->Control_DC_Infor_Data[0].Error.fire_alarm != fault)
			{
				for(i=0;i<CONTROL_DC_NUMBER;i++)
					Globa->Control_DC_Infor_Data[i].Error.fire_alarm = fault;
				if(Globa->Control_DC_Infor_Data[0].Error.fire_alarm == 1)
				{
					sent_warning_message(0x99, 28, 0, 0);
					for(i=0;i<CONTROL_DC_NUMBER;i++)
					{
						Globa->Control_DC_Infor_Data[i].Error_system++;						
						Globa->Control_DC_Infor_Data[i].Error_ctl++;
					}
				}else
				{
					sent_warning_message(0x98, 28, 0, 0);
					for(i=0;i<CONTROL_DC_NUMBER;i++)
					{
						Globa->Control_DC_Infor_Data[i].Error_system--;						
						Globa->Control_DC_Infor_Data[i].Error_ctl--;
					}
				}
    	}
			break;
		}
		case 0x1D:{//充电桩过温故障
			/*if(Globa->Error.sys_over_tmp != fault){
				Globa->Error.sys_over_tmp = fault;
    		if(Globa->Error.sys_over_tmp == 1){
    			sent_warning_message(0x99, 29, 0, 0);
    			Globa->Warn_system++;
    		}else{
    			sent_warning_message(0x98, 29, 0, 0);
    			Globa->Warn_system--;
    		}
    	}*/
			break;
		}
		case 0x1E:{//充电模块输出过压告警
		model_sec = time(NULL);
		if(ac_out == 1 && (model_sec - ac_out_sec > 30))//交流接触器闭合和风扇闭合
		{
			if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_Output_overvoltage_lockout[num] != fault){
				Globa->Control_DC_Infor_Data[gun - 1].Error.charg_Output_overvoltage_lockout[num] = fault;
    		if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_Output_overvoltage_lockout[num] == 1){
    			//sent_warning_message(0x99, 0x1E, gun, num);
    			sent_warning_message(0x99, 0x1E, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system++;
    		}else{
    			//sent_warning_message(0x98, 0x1E, gun, num);
    			sent_warning_message(0x98, 0x1E, gun, num);
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system--;
    		}
    	}
		}
			break;
		}
		case 0x1F:{//充电模块保护
		model_sec = time(NULL);
		if(ac_out == 1 && (model_sec - ac_out_sec > 30))//交流接触器闭合和风扇闭合
		{
			if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_protection[num] != fault){
				Globa->Control_DC_Infor_Data[gun - 1].Error.charg_protection[num] = fault;
    		if(Globa->Control_DC_Infor_Data[gun - 1].Error.charg_protection[num] == 1){
    			sent_warning_message(0x99, 0x1F, gun, num);
		
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system++;
    		}else{
    			sent_warning_message(0x98, 0x1F, gun, num);
				
    			Globa->Control_DC_Infor_Data[gun - 1].Warn_system--;
    		}
    	}
		}
			break;
		}

	//------------------------ 51-----------------------------------------------
		case 0x21:{//交流断路器跳闸
			if(gun == 1)
			{
				if(Globa->Control_DC_Infor_Data[0].Error.breaker_trip != fault)
				{
					for(i=0;i<CONTROL_DC_NUMBER;i++)
						Globa->Control_DC_Infor_Data[i].Error.breaker_trip = fault;
					if(Globa->Control_DC_Infor_Data[0].Error.breaker_trip == 1)
					{
						sent_warning_message(0x99, 33, 0, 0);
						for(i=0;i<CONTROL_DC_NUMBER;i++)
						{
							Globa->Control_DC_Infor_Data[i].Error_system++;						
							Globa->Control_DC_Infor_Data[i].Error_ctl++;
						}
					}else
					{
						sent_warning_message(0x98, 33, 0, 0);
						for(i=0;i<CONTROL_DC_NUMBER;i++)
						{
							Globa->Control_DC_Infor_Data[i].Error_system--;						
							Globa->Control_DC_Infor_Data[i].Error_ctl--;
						}
					}
				}
			}
			break;
		}
		case 0x22:{//交流断路器断开
			if(gun == 1)
			{
				if(Globa->Control_DC_Infor_Data[0].Error.killer_switch != fault)
				{
					for(i=0;i<CONTROL_DC_NUMBER;i++)
						Globa->Control_DC_Infor_Data[0].Error.killer_switch = fault;
					
					if(Globa->Control_DC_Infor_Data[0].Error.killer_switch == 1)
					{
						sent_warning_message(0x99, 34, 0, 0);
						for(i=0;i<CONTROL_DC_NUMBER;i++)
						{
							Globa->Control_DC_Infor_Data[i].Error_system++;						
							Globa->Control_DC_Infor_Data[i].Error_ctl++;
						}
					}else
					{
						sent_warning_message(0x98, 34, 0, 0);
						for(i=0;i<CONTROL_DC_NUMBER;i++)
						{
							Globa->Control_DC_Infor_Data[i].Error_system--;						
							Globa->Control_DC_Infor_Data[i].Error_ctl--;
						}
					}
				}
			}
			break;
		}
		case 0x33:{//电压表通信中断
			break;
		}
		case 0x34:{//电流表通信中断
			break;
		}
		case 0x35:{//充电控制板通信中断
			break;
		}
		case 0x36:{//电表通信中断
			break;
		}
		case 0x37:{//读卡器通信中断
			break;
		}
		case 0x38:{//并机从机通信中断
			break;
		}
		//------------------------ 61 - 65------------------------------------------
		case 0x3D:{//系统绝缘故障
			break;
		}
		case 0x3E:{//输出短路故障
			break;
		}
		case 0x3F:{//输出接触器异常
				if(Globa->Control_DC_Infor_Data[gun - 1].Error.output_switch[0] != fault)
					{
						Globa->Control_DC_Infor_Data[gun - 1].Error.output_switch[0] = fault;
						if(Globa->Control_DC_Infor_Data[gun - 1].Error.output_switch[0] == 1)
						{
							sent_warning_message(0x99, 0x3f, gun, 0);
							Globa->Control_DC_Infor_Data[gun - 1].Error_system++;
							Globa->Control_DC_Infor_Data[gun - 1].Error_ctl++;
						}else
						{
							sent_warning_message(0x98, 0x3f, gun, 0);
							Globa->Control_DC_Infor_Data[gun - 1].Error_system--;
							Globa->Control_DC_Infor_Data[gun - 1].Error_ctl--;
						}
					}
			break;
		}
		case 0x40:{//辅助电源接触器异常
			break;
		}
		case 0x41:{//系统并机接触器异常
					if(Globa->Control_DC_Infor_Data[gun - 1].Error.COUPLE_switch != fault)
					{
						Globa->Control_DC_Infor_Data[gun - 1].Error.COUPLE_switch = fault;
						if(Globa->Control_DC_Infor_Data[gun - 1].Error.COUPLE_switch == 1)
						{
							sent_warning_message(0x99, 0x41, gun, 0);
							Globa->Control_DC_Infor_Data[gun - 1].Error_system++;
							Globa->Control_DC_Infor_Data[gun - 1].Error_ctl++;
						}else
						{
							sent_warning_message(0x98, 0x41, gun, 0);
							Globa->Control_DC_Infor_Data[gun - 1].Error_system--;
							Globa->Control_DC_Infor_Data[gun - 1].Error_ctl--;
						}
					}
			break;
		}
		case 0x42:{//门禁故障
			if(Globa->Control_DC_Infor_Data[0].Error.Door_No_Close_Fault != fault)
			{
				for(i=0;i<CONTROL_DC_NUMBER;i++)
					Globa->Control_DC_Infor_Data[i].Error.Door_No_Close_Fault = fault;
				
				if(Globa->Control_DC_Infor_Data[0].Error.Door_No_Close_Fault == 1)
				{
					sent_warning_message(0x99, 66, 0, 0);
					for(i=0;i<CONTROL_DC_NUMBER;i++)
					{
						Globa->Control_DC_Infor_Data[i].Error_system++;						
						Globa->Control_DC_Infor_Data[i].Error_ctl++;
					}
				}else
				{
					sent_warning_message(0x98, 66, 0, 0);
					for(i=0;i<CONTROL_DC_NUMBER;i++)
					{
						Globa->Control_DC_Infor_Data[i].Error_system--;						
						Globa->Control_DC_Infor_Data[i].Error_ctl--;
					}
				}
			}
			break;
		}
		case 0x43:{//枪锁故障
			if(Globa->Control_DC_Infor_Data[gun - 1].Error.gun_lock_Fault != fault)
			{
				Globa->Control_DC_Infor_Data[gun - 1].Error.gun_lock_Fault = fault;
				if(Globa->Control_DC_Infor_Data[gun - 1].Error.gun_lock_Fault == 1)
				{
					sent_warning_message(0x99, 0x43, gun, 0);
					Globa->Control_DC_Infor_Data[gun - 1].Error_system++;
					Globa->Control_DC_Infor_Data[gun - 1].Error_ctl++;
				}else
				{
					sent_warning_message(0x98, 0x43, gun, 0);
					Globa->Control_DC_Infor_Data[gun - 1].Error_system--;
					Globa->Control_DC_Infor_Data[gun - 1].Error_ctl--;
				}
			}	
	  	break;			
		}				
		case 0x5A:{//充电控制板与功率分配柜通讯异常
			if(Globa->Control_DC_Infor_Data[gun - 1].Error.Power_Allocation_ComFault != fault)
			{
					for(i=0;i<CONTROL_DC_NUMBER;i++)
					Globa->Control_DC_Infor_Data[i].Error.Power_Allocation_ComFault = fault;
				
				if(Globa->Control_DC_Infor_Data[0].Error.Power_Allocation_ComFault == 1)
				{
					sent_warning_message(0x99, 90, 0, 0);
					for(i=0;i<CONTROL_DC_NUMBER;i++)
					{
						Globa->Control_DC_Infor_Data[i].Error_system++;						
						Globa->Control_DC_Infor_Data[i].Error_ctl++;
					}
				}else
				{
					sent_warning_message(0x98, 90, 0, 0);
					for(i=0;i<CONTROL_DC_NUMBER;i++)
					{
						Globa->Control_DC_Infor_Data[i].Error_system--;						
						Globa->Control_DC_Infor_Data[i].Error_ctl--;
					}
				}
			}
			
			break;
		}
		case 0x5B:{//功率分配柜系统异常（总）
			if(Globa->Control_DC_Infor_Data[gun - 1].Error.Power_Allocation_SysFault != fault)
			{
					for(i=0;i<CONTROL_DC_NUMBER;i++)
					Globa->Control_DC_Infor_Data[i].Error.Power_Allocation_SysFault = fault;
				
				if(Globa->Control_DC_Infor_Data[0].Error.Power_Allocation_SysFault == 1)
				{
					sent_warning_message(0x99, 91, 0, 0);
					for(i=0;i<CONTROL_DC_NUMBER;i++)
					{
						Globa->Control_DC_Infor_Data[i].Error_system++;						
						Globa->Control_DC_Infor_Data[i].Error_ctl++;
					}
				}else
				{
					sent_warning_message(0x98, 91, 0, 0);
					for(i=0;i<CONTROL_DC_NUMBER;i++)
					{
						Globa->Control_DC_Infor_Data[i].Error_system--;						
						Globa->Control_DC_Infor_Data[i].Error_ctl--;
					}
				}
			}
			break;
		}
		default:{
			printf("未定义的告警编号 %d\n", alarm_sn);
			return ;

			break;
		}
	}
}

