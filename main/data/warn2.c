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
extern void MC_state_deal_2(UINT32 gun, UINT32 state)
{
	sent_warning_message(0x94, state, gun, 0);
}

/*********************************************************
**description：处理充电控制器传送过来的告警
***parameter  :none
***return		  :none
**********************************************************/
extern void MC_alarm_deal_2(UINT32 gun, UINT32 state, UINT32 alarm_sn, UINT32 fault, UINT32 num)
{   
	switch(alarm_sn){
		case 0x01:{//急停按下故障
			//信号接在一号控制板，一号板处理
			break;
		}
		case 0x02:{//系统接地故障
			if(Globa_2->Error.PE_Fault != fault){
				Globa_2->Error.PE_Fault = fault;
    		if(Globa_2->Error.PE_Fault == 1){
    			sent_warning_message(0x99, 2, gun, 0);
    			Globa_2->Error_system++;
    			Globa_2->Error_ctl++;
    		}else{
    			sent_warning_message(0x98, 2, gun, 0);
    			Globa_2->Error_system--;
    			Globa_2->Error_ctl--;
    			
    		}
    	}
			break;
		}
		case 0x03:{//测量芯片通信故障
			if(Globa_2->Error.Meter_Fault != fault){
				Globa_2->Error.Meter_Fault = fault;
    		if(Globa_2->Error.Meter_Fault == 1){
    			sent_warning_message(0x99, 3, gun, 0);
    			Globa_2->Error_system++;
    			Globa_2->Error_ctl++;
    		}else{
    			sent_warning_message(0x98, 3, gun, 0);
    			Globa_2->Error_system--;
    			Globa_2->Error_ctl--;
    		}
    	}
			break;
		}
		case 0x04:{//交流采集模块通信中断
			//信号接在一号控制板，一号板处理
			break;
		}
		case 0x05:{//充电模块通信中断
		  if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
				if(Globa_2->Error.charg_connect_lost[num] != fault){
					Globa_2->Error.charg_connect_lost[num] = fault;
					if(Globa_2->Error.charg_connect_lost[num] == 1){
						sent_warning_message(0x99, 5, gun, num);
						Globa_2->Warn_system++;
					}else{
						sent_warning_message(0x98, 5, gun, num);
						Globa_2->Warn_system--;
					}
				}
			}
			break;
		}
		case 0x06:{//充电模块故障
			if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
				if(Globa_2->Error.charg_fault[num] != fault){
					Globa_2->Error.charg_fault[num] = fault;
					if(Globa_2->Error.charg_fault[num] == 1){
						sent_warning_message(0x99, 6, gun, num);
						Globa_2->Warn_system++;
					}else{
						sent_warning_message(0x98, 6, gun, num);
						Globa_2->Warn_system--;
					}
				}
		  }
			break;
		}
		case 0x07:{//充电模块风扇故障
			if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
				if(Globa_2->Error.charg_fun_fault[num] != fault){
					Globa_2->Error.charg_fun_fault[num] = fault;
					if(Globa_2->Error.charg_fun_fault[num] == 1){
						sent_warning_message(0x99, 7, gun, num);
						Globa_2->Warn_system++;
					}else{
						sent_warning_message(0x98, 7, gun, num);
						Globa_2->Warn_system--;
					}
				}
			}
			break;
		}
		case 0x08:{//充电模块交流输入告警
			if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
				if(Globa_2->Error.charg_AC_alarm[num] != fault){
					Globa_2->Error.charg_AC_alarm[num] = fault;
					if(Globa_2->Error.charg_AC_alarm[num] == 1){
						sent_warning_message(0x99, 8, gun, num);
						Globa_2->Warn_system++;
					}else{
						sent_warning_message(0x98, 8, gun, num);
						Globa_2->Warn_system--;
					}
				}
			}
			break;
		}
		case 0x09:{//充电模块输出短路故障
		  if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
				if(Globa_2->Error.charg_out_shout[num] != fault){
					Globa_2->Error.charg_out_shout[num] = fault;
					if(Globa_2->Error.charg_out_shout[num] == 1){
						sent_warning_message(0x99, 9, gun, num);
						Globa_2->Warn_system++;
					}else{
						sent_warning_message(0x98, 9, gun, num);
						Globa_2->Warn_system--;
					}
				}
		  }
			break;
		}
		case 0x0A:{//充电模块输出过流告警
		  if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
				if(Globa_2->Error.charg_out_OC[num] != fault){
					Globa_2->Error.charg_out_OC[num] = fault;
					if(Globa_2->Error.charg_out_OC[num] == 1){
						sent_warning_message(0x99, 10, gun, num);
						Globa_2->Warn_system++;
					}else{
						sent_warning_message(0x98, 10, gun, num);
						Globa_2->Warn_system--;
					}
				}
			}
			break;
		}
		case 0x0B:{//充电模块输出过压告警
			if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
				if(Globa_2->Error.charg_out_OV[num] != fault){
					Globa_2->Error.charg_out_OV[num] = fault;
					if(Globa_2->Error.charg_out_OV[num] == 1){
						sent_warning_message(0x99, 11, gun, num);
						Globa_2->Warn_system++;
					}else{
						sent_warning_message(0x98, 11, gun, num);
						Globa_2->Warn_system--;
					}
				}
			}
			break;
		}
		case 0x0C:{//充电模块输出欠压告警
		  if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
				if(Globa_2->Error.charg_out_LV[num] != fault){
					Globa_2->Error.charg_out_LV[num] = fault;
					if(Globa_2->Error.charg_out_LV[num] == 1){
						sent_warning_message(0x99, 12, gun, num);
						Globa_2->Warn_system++;
					}else{
						sent_warning_message(0x98, 12, gun, num);
						Globa_2->Warn_system--;
					}
				}
			}
			break;
		}
		case 0x0D:{//充电模块输入过压告警
		  if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
				if(Globa_2->Error.charg_in_OV[num] != fault){
					Globa_2->Error.charg_in_OV[num] = fault;
					if(Globa_2->Error.charg_in_OV[num] == 1){
						sent_warning_message(0x99, 13, gun, num);
						Globa_2->Warn_system++;
					}else{
						sent_warning_message(0x98, 13, gun, num);
						Globa_2->Warn_system--;
					}
				}
			}
			break;
		}
		case 0x0E:{//充电模块输入欠压告警
			if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
				if(Globa_2->Error.charg_in_LV[num] != fault){
					Globa_2->Error.charg_in_LV[num] = fault;
					if(Globa_2->Error.charg_in_LV[num] == 1){
						sent_warning_message(0x99, 14, gun, num);
						Globa_2->Warn_system++;
					}else{
						sent_warning_message(0x98, 14, gun, num);
						Globa_2->Warn_system--;
					}
				}
			}
			break;
		}
		case 0x0F:{//充电模块输入缺相告警
		  if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
				if(Globa_2->Error.charg_in_lost[num] != fault){
					Globa_2->Error.charg_in_lost[num] = fault;
					if(Globa_2->Error.charg_in_lost[num] == 1){
						sent_warning_message(0x99, 15, gun, num);
						Globa_2->Warn_system++;
					}else{
						sent_warning_message(0x98, 15, gun, num);
						Globa_2->Warn_system--;
					}
				}
			}
			break;
		}
		case 0x10:{//充电模块过温告警
		if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
				if(Globa_2->Error.charg_over_tmp[num] != fault){
					Globa_2->Error.charg_over_tmp[num] = fault;
					if(Globa_2->Error.charg_over_tmp[num] == 1){
						sent_warning_message(0x99, 16, gun, num);
						Globa_2->Warn_system++;
					}else{
						sent_warning_message(0x98, 16, gun, num);
						Globa_2->Warn_system--;
					}
				}
			}
			break;
		}
		case 0x11:{//充电模块过压关机告警
		if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
				if(Globa_2->Error.charg_OV_down[num] != fault){
					Globa_2->Error.charg_OV_down[num] = fault;
					if(Globa_2->Error.charg_OV_down[num] == 1){
						sent_warning_message(0x99, 17, gun, num);
						Globa_2->Warn_system++;
					}else{
						sent_warning_message(0x98, 17, gun, num);
						Globa_2->Warn_system--;
					}
				}
			}
			break;
		}
		case 0x12:{//充电模块其他告警
			if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
				if(Globa_2->Error.charg_other_alarm[num] != fault){
					Globa_2->Error.charg_other_alarm[num] = fault;
					if(Globa_2->Error.charg_other_alarm[num] == 1){
						sent_warning_message(0x99, 18, gun, num);
						Globa_2->Warn_system++;
					}else{
						sent_warning_message(0x98, 18, gun, num);
						Globa_2->Warn_system--;
					}
				}
			}
			break;
		}
		case 0x13:{//系统输出过压告警
			if(Globa_2->Error.output_over_limit != fault){
				Globa_2->Error.output_over_limit = fault;
    		if(Globa_2->Error.output_over_limit == 1){
    			sent_warning_message(0x99, 19, gun, 0);
    			Globa_2->Warn_system++;
    		}else{
    			sent_warning_message(0x98, 19, gun, 0);
    			Globa_2->Warn_system--;
    		}
    	}
			break;
		}
		case 0x14:{//系统输出欠压告警
			if(Globa_2->Error.output_low_limit != fault){
				Globa_2->Error.output_low_limit = fault;
    		if(Globa_2->Error.output_low_limit == 1){
    			sent_warning_message(0x99, 20, gun, 0);
    			Globa_2->Warn_system++;
    		}else{
    			sent_warning_message(0x98, 20, gun, 0);
    			Globa_2->Warn_system--;
    		}
    	}
			break;
		}
		case 0x15:{//系统绝缘告警
			if(Globa_2->Error.sistent_alarm != fault){
				Globa_2->Error.sistent_alarm = fault;
    		if(Globa_2->Error.sistent_alarm == 1){
    			sent_warning_message(0x99, 21, gun, 0);
    			Globa_2->Warn_system++;
    		}else{
    			sent_warning_message(0x98, 21, gun, 0);
    			Globa_2->Warn_system--;
    		}
    	}
			break;
		}
		case 0x16:{//交流输入过压告警
			//信号接在一号控制板，一号板处理
			break;
		}
		case 0x17:{//交流输入过压保护
			//信号接在一号控制板，一号板处理
			break;
		}
		case 0x18:{//交流输入欠压告警，更改为保护
			//信号接在一号控制板，一号板处理
			break;
		}
		case 0x19:{//交流输入开关跳闸
			//信号接在一号控制板，一号板处理
			break;
		}
		case 0x1A:{//交流防雷器跳闸
			//信号接在一号控制板，一号板处理
			break;
		}
		
		case 0x1B:{//直流输出开关跳闸
			if(Globa_1->Error.output_switch_off[num]  != fault){
				Globa_1->Error.output_switch_off[num]  = fault;
    		if(Globa_1->Error.output_switch_off[num]  == 1){
    			sent_warning_message(0x99, 27, 0, num);
    			Globa_2->Error_system++;
    			Globa_2->Error_ctl++;
    		}else{
    			sent_warning_message(0x98, 27, 0, num);
    			Globa_2->Error_system--;
    			Globa_2->Error_ctl--;
    		}
    	}
			break;
		}
		case 0x1C:{//烟雾告警
			/*if(Globa_2->Error.fire_alarm != fault){
				Globa_2->Error.fire_alarm = fault;
    		if(Globa_2->Error.fire_alarm == 1){
    			sent_warning_message(0x99, 28, gun, 0);
    			Globa_2->Warn_system++;
    		}else{
    			sent_warning_message(0x98, 28, gun, 0);
    			Globa_2->Warn_system--;
    		}
    	}*/
			//信号接在一号控制板，一号板处理
			break;
		}
		case 0x1D:{//充电桩过温故障
			if(Globa_2->Error.sys_over_tmp != fault){
				Globa_2->Error.sys_over_tmp = fault;
    		if(Globa_2->Error.sys_over_tmp == 1){
    			sent_warning_message(0x99, 29, gun, 0);
    			Globa_2->Warn_system++;
    		}else{
    			sent_warning_message(0x98, 29, gun, 0);
    			Globa_2->Warn_system--;
    		}
    	}
			break;
		}
		//------------------------ 51-----------------------------------------------
		case 0x1E	:{//充电模块输出过压告警
			if(Globa_2->Error.charg_Output_overvoltage_lockout[num] != fault){
				Globa_2->Error.charg_Output_overvoltage_lockout[num] = fault;
    		if(Globa_2->Error.charg_Output_overvoltage_lockout[num] == 1){
    			sent_warning_message(0x99, 0x1E, gun, num);
    			Globa_2->Warn_system++;
    		}else{
    			sent_warning_message(0x98, 0x1E, gun, num);
    			Globa_2->Warn_system--;
    		}
    	}
			break;
		}
		case 0x1F:{//充电模块保护
			if((Globa_2->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
				if(Globa_2->Error.charg_protection[num] != fault){
					Globa_2->Error.charg_protection[num] = fault;
					if(Globa_2->Error.charg_protection[num] == 1){
						sent_warning_message(0x99, 0x1F, gun, num);
						Globa_2->Warn_system++;
					}else{
						sent_warning_message(0x98, 0x1F, gun, num);
						Globa_2->Warn_system--;
					}
				}
			}
			break;
		}
		
		case 0x21:{//交流断路器跳闸
			break;
		}
		case 0x22:{//交流断路器断开
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
	  case 0x42:{//门禁故障---双枪的时候接在该处
			if(Globa_2->Error.Door_No_Close_Fault != fault){
				Globa_2->Error.Door_No_Close_Fault = fault;
    		if(Globa_2->Error.Door_No_Close_Fault == 1){
    			sent_warning_message(0x99, 66, 0, 0);
    			Globa_1->Error_system++;
    			Globa_2->Error_system++;
    			Globa_2->Error_ctl++;
    		}else{
    			sent_warning_message(0x98, 66, 0, 0);
    			Globa_1->Error_system--;
    			Globa_2->Error_system--;
    			Globa_2->Error_ctl--;
    		}
    	}
			break;
		}
		case 0x5A:{//充电控制板与功率分配柜通讯异常
			if(Globa_2->Error.Power_Allocation_ComFault != fault){
				Globa_2->Error.Power_Allocation_ComFault = fault;
    		if(Globa_2->Error.Power_Allocation_ComFault == 1){
    			sent_warning_message(0x99, 90, gun, 0);
    			Globa_2->Error_system++;
    			Globa_2->Error_ctl++;
    		}else{
    			sent_warning_message(0x98, 90, gun, 0);
    			Globa_2->Error_system--;
    			Globa_2->Error_ctl--;
    		}
    	}
			break;
		}
		case 0x5B:{//功率分配柜系统异常（总）
			if(Globa_2->Error.Power_Allocation_SysFault != fault){
				Globa_2->Error.Power_Allocation_SysFault = fault;
    		if(Globa_2->Error.Power_Allocation_SysFault == 1){
    			sent_warning_message(0x99, 91, gun, 0);
					Globa_2->Error_system++;
    			Globa_2->Error_ctl++;
    		}else{
    			sent_warning_message(0x98, 91, gun, 0);
					Globa_2->Error_system--;
    			Globa_2->Error_ctl--;
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
extern void MC_alarm_Clearn_gun2()
{
  UINT32 i = 0;

	if (Globa_2->Error.PE_Fault == 1)
	{		
      Globa_2->Error.PE_Fault = 0;
			sent_warning_message(0x98, 2, 2, 0);
		  Globa_2->Error_system--;
			Globa_2->Error_ctl--;
	}
	if (Globa_2->Error.Meter_Fault == 1)
	{
			Globa_2->Error.Meter_Fault = 0;
			sent_warning_message(0x98, 3, 2, 0);
			Globa_2->Error_system--;
			Globa_2->Error_ctl--;
	}

  for (i = 0;i<Globa_2->Charger_param.Charge_rate_number2;i++)
	{	
    if(Globa_2->Error.charg_connect_lost[i+1] == 1)
		{
				Globa_2->Error.charg_connect_lost[i+1] = 0;
			  sent_warning_message(0x98, 5, 2, i+1);	
				
		}
		if(Globa_2->Error.charg_fault[i+1] == 1)
		{
				Globa_2->Error.charg_fault[i+1] = 0;
				sent_warning_message(0x98, 6, 2, i+1);	
		}
		if(Globa_2->Error.charg_fun_fault[i+1] == 1)
		{		
	      Globa_2->Error.charg_fun_fault[i+1] = 0;
				sent_warning_message(0x98, 7, 2, i+1);	
				
		}
		if(Globa_2->Error.charg_AC_alarm[i+1] == 1)
		{
			  Globa_2->Error.charg_AC_alarm[i+1] = 0;
				sent_warning_message(0x98, 8, 2, i+1);	
		}
		if(Globa_2->Error.charg_out_shout[i+1] == 1)
		{
				Globa_2->Error.charg_out_shout[i+1] = 0;
				sent_warning_message(0x98, 9, 2, i+1);	
		}
		if(Globa_2->Error.charg_out_OC[i+1] == 1)
		{
				Globa_2->Error.charg_out_OC[i+1] = 0;
				sent_warning_message(0x98, 10, 2, i+1);	
		}
		if(Globa_2->Error.charg_out_OV[i+1] == 1)
		{		
	      Globa_2->Error.charg_out_OV[i+1] = 0;
		    sent_warning_message(0x98, 11, 2, i+1);	
		}
		if(Globa_2->Error.charg_out_LV[i+1] == 1)
		{
				Globa_2->Error.charg_out_LV[i+1] = 0;
			  sent_warning_message(0x98, 12, 2, i+1);	

		}
		if(Globa_2->Error.charg_in_OV[i+1] == 1)
		{			  
	      Globa_2->Error.charg_in_OV[i+1] = 0;

			  sent_warning_message(0x98, 13, 2, i+1);	
		}
		if(Globa_2->Error.charg_in_LV[i+1] == 1)
		{
			  Globa_2->Error.charg_in_LV[i+1] = 0;
				sent_warning_message(0x98, 14, 2, i+1);	
		}
		if(Globa_2->Error.charg_in_lost[i+1] == 1)
		{   
	      Globa_2->Error.charg_in_lost[i+1] = 0;
				sent_warning_message(0x98, 15, 2, i+1);	
		}
		if(Globa_2->Error.charg_over_tmp[i+1] == 1)
		{
			  Globa_2->Error.charg_over_tmp[i+1] = 0;
        sent_warning_message(0x98, 16, 2, i+1);	
		}
		if(Globa_2->Error.charg_OV_down[i+1] == 1)
		{   
	      Globa_2->Error.charg_OV_down[i+1] = 0;
				sent_warning_message(0x98, 17, 2, i+1);	
		}
		if(Globa_2->Error.charg_other_alarm[i+1] == 1)
		{  
	      Globa_2->Error.charg_other_alarm[i+1] = 0;

			  sent_warning_message(0x98, 18, 2, i+1);	
		}
		if(Globa_2->Error.charg_Output_overvoltage_lockout[i+1] == 1)
		{   
	      Globa_2->Error.charg_Output_overvoltage_lockout[i+1] = 0;

				sent_warning_message(0x98, 0x1E, 2, i+1);	
		}
		if(Globa_2->Error.charg_protection[i+1] == 1)
		{   
	      Globa_2->Error.charg_protection[i+1] = 0;
				sent_warning_message(0x98, 0x1F, 2, i+1);	
				
		}
	}
  
	if (Globa_2->Error.output_over_limit == 1)
	{   
      Globa_2->Error.output_over_limit = 0;
		  sent_warning_message(0x98, 19, 2, 0);
	}
	if (Globa_2->Error.output_switch_off[0] == 1)
	{  
      Globa_2->Error.output_switch_off[0] = 0;
		  sent_warning_message(0x98, 27, 2, 0);
			Globa_2->Error_system--;
			Globa_2->Error_ctl--;
	}
	if (Globa_2->Error.sys_over_tmp == 1)
	{   
      Globa_2->Error.sys_over_tmp = 0;
		  sent_warning_message(0x98, 29, 2, 0);
	}
	if (Globa_2->Error.Door_No_Close_Fault == 1)
	{   Globa_2->Error.Door_No_Close_Fault = 0;
		  sent_warning_message(0x98, 66, 0, 0);
			Globa_2->Error_system--;
			Globa_2->Error_ctl--;
			Globa_1->Error_system--;

	}
		if(Globa_2->Error.Power_Allocation_ComFault == 1){
      sent_warning_message(0x98, 90, 2, 0);
      Globa_2->Error_system--;
      Globa_2->Error_ctl--;
			Globa_2->Error.Power_Allocation_ComFault = 0;
    }
	 if(Globa_2->Error.Power_Allocation_SysFault == 1){
    sent_warning_message(0x98, 91, 2, 0);
	  Globa_2->Error_system--;
    Globa_2->Error_ctl--;
	  Globa_2->Error.Power_Allocation_SysFault = 0;
  }
}
