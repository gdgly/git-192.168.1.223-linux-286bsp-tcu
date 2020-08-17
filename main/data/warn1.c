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


/*********************************************************
**description：处理充电控制器传送过来的充电机充电状态
***parameter  :none
***return		  :none
**********************************************************/
extern void MC_state_deal_1(UINT32 gun, UINT32 state)
{
	sent_warning_message(0x94, state, gun, 0);
}

/*********************************************************
**description：处理充电控制器传送过来的告警
***parameter  :none
***return		  :none
**********************************************************/
extern void MC_alarm_deal_1(UINT32 gun, UINT32 state, UINT32 alarm_sn, UINT32 fault, UINT32 num)
{
	switch(alarm_sn){
		case 0x01:{//急停按下故障
			if(Globa_1->Error.emergency_switch != fault){
				Globa_1->Error.emergency_switch = fault;
    		if(Globa_1->Error.emergency_switch == 1){
    			sent_warning_message(0x99, 1, 0, 0);
    			Globa_1->Error_system++;
    			Globa_2->Error_system++;
    			Globa_1->Error_ctl++;
					Globa_2->Operation_command = 2;//进行解锁
					Globa_1->Operation_command = 2;//进行解锁
    		}else{
    			sent_warning_message(0x98, 1, 0, 0);
    			Globa_1->Error_system--;
    			Globa_2->Error_system--;
    			Globa_1->Error_ctl--;
    		}
    	}
			break;
		}
		case 0x02:{//系统接地故障
			if(Globa_1->Error.PE_Fault != fault){
				Globa_1->Error.PE_Fault = fault;
    		if(Globa_1->Error.PE_Fault == 1){
				  if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候{
    			  sent_warning_message(0x99, 2, gun, 0);
					}else{
					  sent_warning_message(0x99, 2, 0, 0);	
					}
    			Globa_1->Error_system++;
    			Globa_1->Error_ctl++;
    		}else{
					if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x98, 2, gun, 0);
					}else{
					  sent_warning_message(0x98, 2, 0, 0);
					}
    			Globa_1->Error_system--;
    			Globa_1->Error_ctl--;
    			
    		}
    	}
			break;
		}
		case 0x03:{//测量芯片通信故障
			if(Globa_1->Error.Meter_Fault != fault){
				Globa_1->Error.Meter_Fault = fault;
    		if(Globa_1->Error.Meter_Fault == 1){
				  if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x99, 3, gun, 0);
					}else{
					  sent_warning_message(0x99, 3, 0, 0);	
					}
    			Globa_1->Error_system++;
    			Globa_1->Error_ctl++;
    		}else{
					if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x98, 3, gun, 0);
					}else{
					  sent_warning_message(0x98, 3, 0, 0);	
					}
    			Globa_1->Error_system--;
    			Globa_1->Error_ctl--;
    		}
    	}
			break;
		}
		case 0x04:{//交流采集模块通信中断
			if(Globa_1->Error.AC_connect_lost != fault){
				Globa_1->Error.AC_connect_lost = fault;
    		if(Globa_1->Error.AC_connect_lost == 1){
    			sent_warning_message(0x99, 4, 0, 0);
    			Globa_1->Error_system++;
    			Globa_2->Error_system++;
    			Globa_1->Error_ctl++;
    		}else{
    			sent_warning_message(0x98, 4, 0, 0);
    			Globa_1->Error_system--;
    			Globa_2->Error_system--;
    			Globa_1->Error_ctl--;
    		}
    	}
			break;
		}
		case 0x05:{//充电模块通信中断
			if(Globa_1->Error.charg_connect_lost[num] != fault){
				Globa_1->Error.charg_connect_lost[num] = fault;
    		if(Globa_1->Error.charg_connect_lost[num] == 1){
					if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x99, 5, gun, num);
					}else{
					  sent_warning_message(0x99, 5, 0, num);	
					}
    			Globa_1->Warn_system++;
    		}else{
					if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x98, 5, gun, num);
					}else{
					  sent_warning_message(0x98, 5, 0, num);	
					}
    			Globa_1->Warn_system--;
    		}
    	}
			break;
		}
		case 0x06:{//充电模块故障
			if(Globa_1->Error.charg_fault[num] != fault){
				Globa_1->Error.charg_fault[num] = fault;
    		if(Globa_1->Error.charg_fault[num] == 1){
				  if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x99, 6, gun, num);
					}else{
					  sent_warning_message(0x99, 6, 0, num);	
					}
    			Globa_1->Warn_system++;
    		}else{
    			if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x98, 6, gun, num);
					}else{
					  sent_warning_message(0x98, 6, 0, num);	
					}
    			Globa_1->Warn_system--;
    		}
    	}
			break;
		}
		case 0x07:{//充电模块风扇故障
			if(Globa_1->Error.charg_fun_fault[num] != fault){
				Globa_1->Error.charg_fun_fault[num] = fault;
    		if(Globa_1->Error.charg_fun_fault[num] == 1){
				  if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x99, 7, gun, num);
					}else{
					  sent_warning_message(0x99, 7, 0, num);	
					}
    			Globa_1->Warn_system++;
    		}else{
				  if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x98, 7, gun, num);
					}else{
					  sent_warning_message(0x98, 7, 0, num);	
					}
    			Globa_1->Warn_system--;
    		}
    	}
			break;
		}
		case 0x08:{//充电模块交流输入告警
			if(Globa_1->Error.charg_AC_alarm[num] != fault){
				Globa_1->Error.charg_AC_alarm[num] = fault;
    		if(Globa_1->Error.charg_AC_alarm[num] == 1){
					if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x99, 8, gun, num);
					}else{
					  sent_warning_message(0x99, 8, 0, num);	
					}
    			Globa_1->Warn_system++;
    		}else{
				  if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x98, 8, gun, num);
					}else{
					  sent_warning_message(0x98, 8, 0, num);	
					}
    			Globa_1->Warn_system--;
    		}
    	}
			break;
		}
		case 0x09:{//充电模块输出短路故障
			if(Globa_1->Error.charg_out_shout[num] != fault){
				Globa_1->Error.charg_out_shout[num] = fault;
    		if(Globa_1->Error.charg_out_shout[num] == 1){
    			if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x99, 9, gun, num);
					}else{
					  sent_warning_message(0x99, 9, 0, num);	
					}
    			Globa_1->Warn_system++;
    		}else{
					if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x98, 9, gun, num);
					}else{
					  sent_warning_message(0x98, 9, 0, num);	
					}
    			Globa_1->Warn_system--;
    		}
    	}
			break;
		}
		case 0x0A:{//充电模块输出过流告警
			if(Globa_1->Error.charg_out_OC[num] != fault){
				Globa_1->Error.charg_out_OC[num] = fault;
    		if(Globa_1->Error.charg_out_OC[num] == 1){
					if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x99, 10, gun, num);
					}else{
					  sent_warning_message(0x99, 10, 0, num);	
					}
    			Globa_1->Warn_system++;
    		}else{
    			if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x98, 10, gun, num);
					}else{
					  sent_warning_message(0x98, 10, 0, num);	
					}
    			Globa_1->Warn_system--;
    		}
    	}
			break;
		}
		case 0x0B:{//充电模块输出过压告警
			if(Globa_1->Error.charg_out_OV[num] != fault){
				Globa_1->Error.charg_out_OV[num] = fault;
    		if(Globa_1->Error.charg_out_OV[num] == 1){
					if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x99, 11, gun, num);
					}else{
					  sent_warning_message(0x99, 11, 0, num);	
					}
    			Globa_1->Warn_system++;
    		}else{
					if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x98, 11, gun, num);
					}else{
					  sent_warning_message(0x98, 11, 0, num);	
					}
    			Globa_1->Warn_system--;
    		}
    	}
			break;
		}
		case 0x0C:{//充电模块输出欠压告警
			if(Globa_1->Error.charg_out_LV[num] != fault){
				Globa_1->Error.charg_out_LV[num] = fault;
    		if(Globa_1->Error.charg_out_LV[num] == 1){
					if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x99, 12, gun, num);
					}else{
					  sent_warning_message(0x99, 12, 0, num);	
					}
    			Globa_1->Warn_system++;
    		}else{
					if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x98, 12, gun, num);
					}else{
					  sent_warning_message(0x98, 12, 0, num);	
					}
    			Globa_1->Warn_system--;
    		}
    	}
			break;
		}
		case 0x0D:{//充电模块输入过压告警
			if(Globa_1->Error.charg_in_OV[num] != fault){
				Globa_1->Error.charg_in_OV[num] = fault;
    		if(Globa_1->Error.charg_in_OV[num] == 1){
					if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x99, 13, gun, num);
					}else{
					  sent_warning_message(0x99, 13, 0, num);	
					}
    			Globa_1->Warn_system++;
    		}else{
    			if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x98, 13, gun, num);
					}else{
					  sent_warning_message(0x98, 13, 0, num);	
					}
    			Globa_1->Warn_system--;
    		}
    	}
			break;
		}
		case 0x0E:{//充电模块输入欠压告警
			if(Globa_1->Error.charg_in_LV[num] != fault){
				Globa_1->Error.charg_in_LV[num] = fault;
    		if(Globa_1->Error.charg_in_LV[num] == 1){
    			if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x99, 14, gun, num);
					}else{
					  sent_warning_message(0x99, 14, 0, num);	
					}
    			Globa_1->Warn_system++;
    		}else{
    			if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x98, 14, gun, num);
					}else{
					  sent_warning_message(0x98, 14, 0, num);	
					}
    			Globa_1->Warn_system--;
    		}
    	}
			break;
		}
		case 0x0F:{//充电模块输入缺相告警
			if(Globa_1->Error.charg_in_lost[num] != fault){
				Globa_1->Error.charg_in_lost[num] = fault;
    		if(Globa_1->Error.charg_in_lost[num] == 1){
					if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x99, 15, gun, num);
					}else{
					  sent_warning_message(0x99, 15, 0, num);	
					}
    			Globa_1->Warn_system++;
    		}else{
    			if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x98, 15, gun, num);
					}else{
					  sent_warning_message(0x98, 15, 0, num);	
					}
    			Globa_1->Warn_system--;
    		}
    	}
			break;
		}
		case 0x10:{//充电模块过温告警
			if(Globa_1->Error.charg_over_tmp[num] != fault){
				Globa_1->Error.charg_over_tmp[num] = fault;
    		if(Globa_1->Error.charg_over_tmp[num] == 1){
    			if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x99, 16, gun, num);
					}else{
					  sent_warning_message(0x99, 16, 0, num);	
					}
    			Globa_1->Warn_system++;
    		}else{
    			if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x98, 16, gun, num);
					}else{
					  sent_warning_message(0x98, 16, 0, num);	
					}
    			Globa_1->Warn_system--;
    		}
    	}
			break;
		}
		case 0x11:{//充电模块过压关机告警
			if(Globa_1->Error.charg_OV_down[num] != fault){
				Globa_1->Error.charg_OV_down[num] = fault;
    		if(Globa_1->Error.charg_OV_down[num] == 1){
					if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x99, 17, gun, num);
					}else{
					  sent_warning_message(0x99, 17, 0, num);	
					}
    			Globa_1->Warn_system++;
    		}else{
    			if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x98, 17, gun, num);
					}else{
					  sent_warning_message(0x98, 17, 0, num);	
					}
    			Globa_1->Warn_system--;
    		}
    	}
			break;
		}
		case 0x12:{//充电模块其他告警
			if(Globa_1->Error.charg_other_alarm[num] != fault){
				Globa_1->Error.charg_other_alarm[num] = fault;
    		if(Globa_1->Error.charg_other_alarm[num] == 1){
    			if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x99, 18, gun, num);
					}else{
					  sent_warning_message(0x99, 18, 0, num);	
					}
    			Globa_1->Warn_system++;
    		}else{
	        if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x98, 18, gun, num);
					}else{
					  sent_warning_message(0x98, 18, 0, num);	
					}    		
					Globa_1->Warn_system--;
    		}
    	}
			break;
		}
		case 0x13:{//系统输出过压告警
			if(Globa_1->Error.output_over_limit != fault){
				Globa_1->Error.output_over_limit = fault;
    		if(Globa_1->Error.output_over_limit == 1){
					if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x99, 19, gun, 0);
					}else{
					  sent_warning_message(0x99, 19, 0, 0);	
					}
    			Globa_1->Warn_system++;
    		}else{
			    if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x98, 19, gun, 0);
					}else{
					  sent_warning_message(0x98, 19, 0, 0);	
					}    
    			Globa_1->Warn_system--;
    		}
    	}
			break;
		}
		case 0x14:{//系统输出欠压告警
			if(Globa_1->Error.output_low_limit != fault){
				Globa_1->Error.output_low_limit = fault;
    		if(Globa_1->Error.output_low_limit == 1){
				  if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x99, 20, gun, 0);
					}else{
					  sent_warning_message(0x99, 20, 0, 0);	
					}
    			Globa_1->Warn_system++;
    		}else{
					if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x98, 20, gun, 0);
					}else{
					  sent_warning_message(0x98, 20, 0, 0);	
					}  
    			Globa_1->Warn_system--;
    		}
    	}
			break;
		}
		case 0x15:{//系统绝缘告警
			if(Globa_1->Error.sistent_alarm != fault){
				Globa_1->Error.sistent_alarm = fault;
    		if(Globa_1->Error.sistent_alarm == 1){
			    if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4)){
    				sent_warning_message(0x99, 21, gun, 0);
					}else{
					  sent_warning_message(0x99, 21, 0, 0);
					}
    			Globa_1->Warn_system++;
    		}else{
					if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x98, 21, gun, 0);
					}else{
					  sent_warning_message(0x98, 21, 0, 0);	
					} 
    			Globa_1->Warn_system--;
    		}
    	}
			break;
		}
		case 0x16:{//交流输入过压告警
			if(Globa_1->Error.input_over_limit != fault){
				Globa_1->Error.input_over_limit = fault;
    		if(Globa_1->Error.input_over_limit == 1){
    			sent_warning_message(0x99, 22, 0, 0);
    			Globa_1->Warn_system++;
    		}else{
    			sent_warning_message(0x98, 22, 0, 0);
    			Globa_1->Warn_system--;
    		}
    	}
			break;
		}
		case 0x17:{//交流输入过压保护
			if(Globa_1->Error.input_over_protect != fault){
				Globa_1->Error.input_over_protect = fault;
    		if(Globa_1->Error.input_over_protect == 1){
    			sent_warning_message(0x99, 23, 0, 0);
    			Globa_1->Error_system++;
    			Globa_2->Error_system++;
    			Globa_1->Error_ctl++;
    		}else{
    			sent_warning_message(0x98, 23, 0, 0);
    			Globa_1->Error_system--;
    			Globa_2->Error_system--;
    			Globa_1->Error_ctl--;
    		}
    	}
			break;
		}
		case 0x18:{//交流输入欠压告警，更改为保护
			if(Globa_1->Error.input_lown_limit != fault){
				Globa_1->Error.input_lown_limit = fault;
    		if(Globa_1->Error.input_lown_limit == 1){
    			sent_warning_message(0x99, 24, 0, 0);
    			Globa_1->Error_system++;
    			Globa_2->Error_system++;
    			Globa_1->Error_ctl++;
    		}else{
    			sent_warning_message(0x98, 24, 0, 0);
    			Globa_1->Error_system--;
    			Globa_2->Error_system--;    			
    			Globa_1->Error_ctl--;
    		}
    	}
			break;
		}
		case 0x19:{//交流输入开关跳闸
			if(Globa_1->Error.input_switch_off != fault){
				Globa_1->Error.input_switch_off = fault;
    		if(Globa_1->Error.input_switch_off == 1){
    			sent_warning_message(0x99, 25, 0, 0);
    			Globa_1->Error_system++;
    			Globa_2->Error_system++;
    			Globa_1->Error_ctl++;
    		}else{
    			sent_warning_message(0x98, 25, 0, 0);
    			Globa_1->Error_system--;
    			Globa_2->Error_system--;
    			Globa_1->Error_ctl--;
    		}
    	}
			break;
		}
		case 0x1A:{//交流防雷器跳闸
			if(Globa_1->Error.fanglei_off != fault){
				Globa_1->Error.fanglei_off = fault;
    		if(Globa_1->Error.fanglei_off == 1){
    			sent_warning_message(0x99, 26, 0, 0);
    			Globa_1->Error_system++;
    			Globa_2->Error_system++;
    			Globa_1->Error_ctl++;
    		}else{
    			sent_warning_message(0x98, 26, 0, 0);
    			Globa_1->Error_system--;
    			Globa_2->Error_system--;
    			Globa_1->Error_ctl--;
    		}
    	}
			break;
		}
		
		case 0x1B:{
			if(Globa_1->Error.output_switch_off[num]  != fault){
				Globa_1->Error.output_switch_off[num]  = fault;
    		if(Globa_1->Error.output_switch_off[num]  == 1){
    			sent_warning_message(0x99, 27, 0, num);
    			Globa_1->Error_system++;
    			//Globa_2->Error_system++;
    			Globa_1->Error_ctl++;
    		}else{
    			sent_warning_message(0x98, 27, 0, num);
    			Globa_1->Error_system--;
    			//Globa_2->Error_system--;
    			Globa_1->Error_ctl--;
    		}
    	}
			break;
		}
		case 0x1C:{//烟雾告警
			if(Globa_1->Error.fire_alarm != fault){
				Globa_1->Error.fire_alarm = fault;
    		if(Globa_1->Error.fire_alarm == 1){
    			sent_warning_message(0x99, 28, 0, 0);
    			Globa_1->Error_system++;
    			Globa_2->Error_system++;
    			Globa_1->Error_ctl++;
    		}else{
    			sent_warning_message(0x98, 28, 0, 0);
    			Globa_1->Error_system--;
    			Globa_2->Error_system--;
    			Globa_1->Error_ctl--;
    		}
    	}
			break;
		}
		case 0x1D:{//充电桩过温故障
			if(Globa_1->Error.sys_over_tmp != fault){
				Globa_1->Error.sys_over_tmp = fault;
    		if(Globa_1->Error.sys_over_tmp == 1){
    			sent_warning_message(0x99, 29, 0, 0);
    			Globa_1->Warn_system++;
    		}else{
    			sent_warning_message(0x98, 29, 0, 0);
    			Globa_1->Warn_system--;
    		}
    	}
			break;
		}
    case 0x1E	:{//充电模块输出过压告警
			if(Globa_1->Error.charg_Output_overvoltage_lockout[num] != fault){
				Globa_1->Error.charg_Output_overvoltage_lockout[num] = fault;
    		if(Globa_1->Error.charg_Output_overvoltage_lockout[num] == 1){
    			//sent_warning_message(0x99, 0x1E, gun, num);
				  if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x99, 0x1E, gun, num);
					}else{
					  sent_warning_message(0x99, 0x1E, 0, num);
					}
    			Globa_1->Warn_system++;
    		}else{
    			//sent_warning_message(0x98, 0x1E, gun, num);
					if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x98, 0x1E, gun, num);
					}else{
					  sent_warning_message(0x98, 0x1E, 0, num);
					}
    			Globa_1->Warn_system--;
    		}
    	}
			break;
		}
		case 0x1F:{//充电模块保护
			if(Globa_1->Error.charg_protection[num] != fault){
				Globa_1->Error.charg_protection[num] = fault;
    		if(Globa_1->Error.charg_protection[num] == 1){
					if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x99, 0x1F, gun, num);
					}else{
					  sent_warning_message(0x99, 0x1F, 0, num);
					}
    			Globa_1->Warn_system++;
    		}else{
			  	if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
    			  sent_warning_message(0x98, 0x1F, gun, num);
					}else{
					  sent_warning_message(0x98, 0x1F, 0, num);
					}
    			Globa_1->Warn_system--;
    		}
    	}
			break;
		}

	//------------------------ 51-----------------------------------------------
		case 0x21:{//交流断路器跳闸
			if(Globa_1->Error.breaker_trip != fault){
				Globa_1->Error.breaker_trip = fault;
    		if(Globa_1->Error.breaker_trip == 1){
    			sent_warning_message(0x99, 33, 0, 0);
    			Globa_1->Error_system++;
    			Globa_2->Error_system++;
    			Globa_1->Error_ctl++;
    		}else{
    			sent_warning_message(0x98, 33, 0, 0);
    			Globa_1->Error_system--;
    			Globa_2->Error_system--;
    			Globa_1->Error_ctl--;
    		}
    	}
			break;
		}
		case 0x22:{//交流断路器断开
			if(Globa_1->Error.killer_switch != fault){
				Globa_1->Error.killer_switch = fault;
    		if(Globa_1->Error.killer_switch == 1){
    			sent_warning_message(0x99, 34, 0, 0);
    			Globa_1->Error_system++;
    			Globa_2->Error_system++;
    			Globa_1->Error_ctl++;
    		}else{
    			sent_warning_message(0x98, 34, 0, 0);
    			Globa_1->Error_system--;
    			Globa_2->Error_system--;
    			Globa_1->Error_ctl--;
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
			break;
		}
		case 0x40:{//辅助电源接触器异常
			break;
		}
		case 0x41:{//系统并机接触器异常
			break;
		}
		case 0x42:{//门禁故障
			if(Globa_1->Error.Door_No_Close_Fault != fault){
				Globa_1->Error.Door_No_Close_Fault = fault;
    		if(Globa_1->Error.Door_No_Close_Fault == 1){
    			sent_warning_message(0x99, 66, 0, 0);
    			Globa_1->Error_system++;
    			Globa_2->Error_system++;
    			Globa_1->Error_ctl++;
    		}else{
    			sent_warning_message(0x98, 66, 0, 0);
    			Globa_1->Error_system--;
    			Globa_2->Error_system--;
    			Globa_1->Error_ctl--;
    		}
    	}
			break;
		}
	  case 0x5A:{//充电控制板与功率分配柜通讯异常
			if(Globa_1->Error.Power_Allocation_ComFault != fault){
				Globa_1->Error.Power_Allocation_ComFault = fault;
    		if(Globa_1->Error.Power_Allocation_ComFault == 1){
    			sent_warning_message(0x99, 90, gun, 0);
    			Globa_1->Error_system++;
    			Globa_1->Error_ctl++;
    		}else{
    			sent_warning_message(0x98, 90, gun, 0);
    			Globa_1->Error_system--;
    			Globa_1->Error_ctl--;
    		}
    	}
			break;
		}
		case 0x5B:{//功率分配柜系统异常（总）
			if(Globa_1->Error.Power_Allocation_SysFault != fault){
				Globa_1->Error.Power_Allocation_SysFault = fault;
    		if(Globa_1->Error.Power_Allocation_SysFault == 1){
    			sent_warning_message(0x99, 91, gun, 0);
    			Globa_1->Error_system++;
					Globa_2->Error_system++;
    			Globa_1->Error_ctl++;
    		}else{
    			sent_warning_message(0x98, 91, gun, 0);
    			Globa_1->Error_system--;
					Globa_2->Error_system--;
    			Globa_1->Error_ctl--;
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

/*********************************************************
**description：清除已经发送的故障(指控制板中的)
***parameter  :none
***return		  :none
**********************************************************/
extern void MC_alarm_Clearn_gun1()
{
  UINT32 i = 0;
	if (Globa_1->Error.emergency_switch == 1)
	{   Globa_1->Error.emergency_switch = 0;
	    sent_warning_message(0x98, 1, 0, 0);
			Globa_1->Error_system--;
			Globa_2->Error_system--;
			Globa_1->Error_ctl--;
	}
	
	if (Globa_1->Error.PE_Fault == 1)
	{		
      Globa_1->Error.PE_Fault = 0;
		  Globa_1->Error_system--;
			Globa_1->Error_ctl--;
			if ((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4))
			{
					sent_warning_message(0x98, 2, 1, 0);
			}
			else
			{
					sent_warning_message(0x98, 2, 0, 0);
			}
	}
	
	if (Globa_1->Error.Meter_Fault == 1)
	{
			Globa_1->Error.Meter_Fault = 0;
		  Globa_1->Error_system--;
			Globa_1->Error_ctl--;
			if ((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4))
			{
					sent_warning_message(0x98, 3, 1, 0);
			}
			else
			{
					sent_warning_message(0x98, 3, 0, 0);
			}
	}
	
	if (Globa_1->Error.AC_connect_lost == 1)
	{
		  Globa_1->Error.AC_connect_lost = 0;
		  sent_warning_message(0x98, 4, 0, 0);
			Globa_1->Error_system--;
			Globa_2->Error_system--;
			Globa_1->Error_ctl--;
	}
	
	if (Globa_1->Error.input_over_limit == 1)
	{
		  Globa_1->Error.input_over_limit = 0;
		  sent_warning_message(0x98, 22, 0, 0);
	}
	
  if (Globa_1->Error.input_over_protect == 1)
	{
		  Globa_1->Error.input_over_protect = 0;
			sent_warning_message(0x98, 23, 0, 0);
			Globa_1->Error_system--;
			Globa_2->Error_system--;
			Globa_1->Error_ctl--;
	}
	if (Globa_1->Error.input_lown_limit == 1)
	{
		  Globa_1->Error.input_lown_limit = 0;
			sent_warning_message(0x98, 24, 0, 0);
			Globa_1->Error_system--;
			Globa_2->Error_system--;
			Globa_1->Error_ctl--;
	}
	if (Globa_1->Error.input_switch_off == 1)
	{
		  Globa_1->Error.input_switch_off = 0;
			sent_warning_message(0x98, 25, 0, 0);
			Globa_1->Error_system--;
			Globa_2->Error_system--;
			Globa_1->Error_ctl--;
	}
	if (Globa_1->Error.fanglei_off == 1)
	{
		  Globa_1->Error.fanglei_off = 0;
			sent_warning_message(0x98, 26, 0, 0);
			Globa_1->Error_system--;
			Globa_2->Error_system--;
			Globa_1->Error_ctl--;
	}
	if (Globa_1->Error.breaker_trip == 1)
	{
		  Globa_1->Error.breaker_trip = 0;
			sent_warning_message(0x98, 33, 0, 0);
			Globa_1->Error_system--;
			Globa_2->Error_system--;
			Globa_1->Error_ctl--;
	}
	if (Globa_1->Error.killer_switch == 1)
	{
		  Globa_1->Error.killer_switch = 0;
			sent_warning_message(0x98, 34, 0, 0);
			Globa_1->Error_system--;
			Globa_2->Error_system--;
			Globa_1->Error_ctl--;
	}
	
  for (i = 0;i<Globa_1->Charger_param.Charge_rate_number1;i++)
	{	
    if(Globa_1->Error.charg_connect_lost[i+1] == 1)
		{
				Globa_1->Error.charg_connect_lost[i+1] = 0;

				if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4))
				{
					  sent_warning_message(0x98, 5, 1, i+1);
				}
				else
				{
					  sent_warning_message(0x98, 5, 0, i+1);	
				}
		}
		if(Globa_1->Error.charg_fault[i+1] == 1)
		{
				Globa_1->Error.charg_fault[i+1] = 0;

				if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4))
				{
					  sent_warning_message(0x98, 6, 1, i+1);
				}
				else
				{
					  sent_warning_message(0x98, 6, 0, i+1);	
				}
		}
		if(Globa_1->Error.charg_fun_fault[i+1] == 1)
		{		
	      Globa_1->Error.charg_fun_fault[i+1] = 0;

				if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4))
				{
					  sent_warning_message(0x98, 7, 1, i+1);
				}
				else
				{
					  sent_warning_message(0x98, 7, 0, i+1);	
				}
		}
		if(Globa_1->Error.charg_AC_alarm[i+1] == 1)
		{
			  Globa_1->Error.charg_AC_alarm[i+1] = 0;
				if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4))
				{
					  sent_warning_message(0x98, 8, 1, i+1);
				}
				else
				{
					  sent_warning_message(0x98, 8, 0, i+1);	
				}
		}
		if(Globa_1->Error.charg_out_shout[i+1] == 1)
		{
				Globa_1->Error.charg_out_shout[i+1] = 0;

				if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4))
				{
					  sent_warning_message(0x98, 9, 1, i+1);
				}
				else
				{
					  sent_warning_message(0x98, 9, 0, i+1);	
				}
		}
		if(Globa_1->Error.charg_out_OC[i+1] == 1)
		{
				Globa_1->Error.charg_out_OC[i+1] = 0;
				if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4))
				{
					  sent_warning_message(0x98, 10, 1, i+1);
				}
				else
				{
					  sent_warning_message(0x98, 10, 0, i+1);	
				}
		}
		if(Globa_1->Error.charg_out_OV[i+1] == 1)
		{		
	      Globa_1->Error.charg_out_OV[i+1] = 0;
				if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4))
				{
					  sent_warning_message(0x98, 11, 1, i+1);
				}
				else
				{
					  sent_warning_message(0x98, 11, 0, i+1);	
				}
		}
		if(Globa_1->Error.charg_out_LV[i+1] == 1)
		{
				Globa_1->Error.charg_out_LV[i+1] = 0;

				if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4))
				{
					  sent_warning_message(0x98, 12, 1, i+1);
				}
				else
				{
					  sent_warning_message(0x98, 12, 0, i+1);	
				}
		}
		if(Globa_1->Error.charg_in_OV[i+1] == 1)
		{			  
	      Globa_1->Error.charg_in_OV[i+1] = 0;

				if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4))
				{
					  sent_warning_message(0x98, 13, 1, i+1);
				}
				else
				{
					  sent_warning_message(0x98, 13, 0, i+1);	
				}
		}
		if(Globa_1->Error.charg_in_LV[i+1] == 1)
		{
			  Globa_1->Error.charg_in_LV[i+1] = 0;
				if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4))
				{
					  sent_warning_message(0x98, 14, 1, i+1);
				}
				else
				{
					  sent_warning_message(0x98, 14, 0, i+1);	
				}
		}
		if(Globa_1->Error.charg_in_lost[i+1] == 1)
		{   
	      Globa_1->Error.charg_in_lost[i+1] = 0;
				if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4))
				{
					  sent_warning_message(0x98, 15, 1, i+1);
				}
				else
				{
					  sent_warning_message(0x98, 15, 0, i+1);	
				}
		}
		if(Globa_1->Error.charg_over_tmp[i+1] == 1)
		{
			  Globa_1->Error.charg_over_tmp[i+1] = 0;
				if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4))
				{
					  sent_warning_message(0x98, 16, 1, i+1);
				}
				else
				{
					  sent_warning_message(0x98, 16, 0, i+1);	
				}
		}
		if(Globa_1->Error.charg_OV_down[i+1] == 1)
		{   
	      Globa_1->Error.charg_OV_down[i+1] = 0;
				if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4))
				{
					  sent_warning_message(0x98, 17, 1, i+1);
				}
				else
				{
					  sent_warning_message(0x98, 17, 0, i+1);	
				}
		}
		if(Globa_1->Error.charg_other_alarm[i+1] == 1)
		{  
	      Globa_1->Error.charg_other_alarm[i+1] = 0;
				if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4))
				{
					  sent_warning_message(0x98, 18, 1, i+1);
				}
				else
				{
					  sent_warning_message(0x98, 18, 0, i+1);	
				}
		}
		if(Globa_1->Error.charg_Output_overvoltage_lockout[i+1] == 1)
		{   
	      Globa_1->Error.charg_Output_overvoltage_lockout[i+1] = 0;
				if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4))
				{
					  sent_warning_message(0x98, 0x1E, 1, i+1);
				}
				else
				{
					  sent_warning_message(0x98, 0x1E, 0, i+1);	
				}
		}
		if(Globa_1->Error.charg_protection[i+1] == 1)
		{   
	      Globa_1->Error.charg_protection[i+1] = 0;
				if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4))
				{
					  sent_warning_message(0x98, 0x1F, 1, i+1);
				}
				else
				{
					  sent_warning_message(0x98, 0x1F, 0, i+1);	
				}
		}
	}
  
	if (Globa_1->Error.output_over_limit == 1)
	{   
      Globa_1->Error.output_over_limit = 0;
		  sent_warning_message(0x98, 19, 0, 0);
	}
	if (Globa_1->Error.output_switch_off[0] == 1)
	{  
		  Globa_1->Error_system--;
			Globa_1->Error_ctl--;
      Globa_1->Error.output_switch_off[0] = 0;
		  sent_warning_message(0x98, 27, 0, 0);
	}
	if (Globa_1->Error.sys_over_tmp == 1)
	{   
      Globa_1->Error.sys_over_tmp = 0;
		  sent_warning_message(0x98, 29, 0, 0);
	}
	if (Globa_1->Error.Door_No_Close_Fault == 1)
	{   Globa_1->Error.Door_No_Close_Fault = 0;
		  sent_warning_message(0x98, 66, 0, 0);
		  Globa_1->Error_system--;
			Globa_1->Error_ctl--;
	}
	
	if(Globa_2->Error.Power_Allocation_ComFault == 1){
    sent_warning_message(0x98, 90, 1, 0);
    Globa_1->Error_system--;
    Globa_1->Error_ctl--;
	  Globa_1->Error.Power_Allocation_ComFault = 0;
  }
  if(Globa_1->Error.Power_Allocation_SysFault == 1){
    sent_warning_message(0x98, 91, 1, 0);
    Globa_1->Error_system--;
    Globa_1->Error_ctl--;
	  Globa_1->Error.Power_Allocation_SysFault = 0;
  }
}