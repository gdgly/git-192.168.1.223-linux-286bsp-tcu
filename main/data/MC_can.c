/*******************************************************************************
  Copyright(C)    2014-2015,      EAST Co.Ltd.
  File name :     MC_can.c
  Author :        dengjh
  Version:        1.0
  Date:           2014.5.11
  DescriGBion:    国标协议开发  GB/T 27930-2011 
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
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>

#include "MC_can.h"
#include "globalvar.h"
#define  MC_DEBUG  0
extern MC_ALL_FRAME  MC_All_Frame[CONTROL_DC_NUMBER];
extern MC_CONTROL_VAR  mc_control_var[CONTROL_DC_NUMBER];
extern unsigned char ls_need_model_num_temp[CONTROL_DC_NUMBER];
extern unsigned char host_congji[CONTROL_DC_NUMBER][CONTROL_DC_NUMBER];

extern unsigned char ac_out;
extern time_t ac_out_sec;
extern unsigned char Jkac_con;
UINT32 model_output_vol[CONTROL_DC_NUMBER] = {0};
UINT32 model_output_cur[CONTROL_DC_NUMBER] = {0};
UINT32 host_Relay_control[CONTROL_DC_NUMBER] = {0};
UINT32 host_Relay_control_Output[CONTROL_DC_NUMBER] = {0};
UINT32 chgenable[CONTROL_DC_NUMBER] = {0};



UINT32 Output_voltage[CONTROL_DC_NUMBER] = {0};
UINT32 Output_current[CONTROL_DC_NUMBER] = {0};
UINT32 need_voltage[CONTROL_DC_NUMBER] ={0};
UINT32 need_current[CONTROL_DC_NUMBER] = {0};
UINT32 Output_kw[CONTROL_DC_NUMBER] = {0};
UINT32 kw_percent[CONTROL_DC_NUMBER] = {0};
UINT32 chgstart[CONTROL_DC_NUMBER] = {0};
UINT32 Module_Readyok[CONTROL_DC_NUMBER] = {0};

UINT32 pre_outvolt[CONTROL_DC_NUMBER] = {0},pre_out_current[CONTROL_DC_NUMBER] = {0},pre_need_volt[CONTROL_DC_NUMBER] = {0},pre_need_current[CONTROL_DC_NUMBER] = {0};
UINT32 pre_Gunlink[CONTROL_DC_NUMBER] = {0},pre_Modbule_On[CONTROL_DC_NUMBER] = {0},pre_Charged_State[CONTROL_DC_NUMBER] = {0},pre_Relay_State[CONTROL_DC_NUMBER] = {0};
UINT16 gAdhesion_fault_Cont[CONTROL_DC_NUMBER] = {0},gMisoperation_fault_Cont[CONTROL_DC_NUMBER] = {0},gMiss_fault_Cont[CONTROL_DC_NUMBER] = {0};

UINT16 grContactor_Adhesion_fault[CONTROL_DC_NUMBER] = {0},grContactor_Misoperation_fault[CONTROL_DC_NUMBER] = {0},grContactor_Miss_fault[CONTROL_DC_NUMBER] = {0};


/*******************************************************************************
**description：MC读线程
***parameter  :int fd
***return		  :none
*******************************************************************************/
void MC_read_deal(struct can_frame frame,int Gun_No)
{
	int i;
	UINT32 sum = 0,Other_Gun_No = 0;
	
//	printf("----------frame.can_id&CAN_EFF_MASK[%d] = %x\n",Gun_No,frame.can_id&CAN_EFF_MASK);

	switch(frame.can_id&CAN_EFF_MASK)
	{//判断接收帧ID
	
		case PGN16256_ID_MC:{//控制信息请求帧，控制模块的输出电压电流由计费单元进行分配支出
			if((frame.data[1] >= 1)&&(frame.data[1] <= ((sizeof(PC_PGN16256_FRAME)+5) + 5)/6))
			{
				if(frame.data[1] == 1)
				{//第一个数据包
					mc_control_var[Gun_No].MC_var.sequence = frame.data[1];
					mc_control_var[Gun_No].MC_var.Total_Num_Packet = frame.data[2];
					mc_control_var[Gun_No].MC_var.Total_Mess_Size = frame.data[3] + (frame.data[4]<<8);

					if((mc_control_var[Gun_No].MC_var.Total_Num_Packet == ((sizeof(PC_PGN16256_FRAME)+5) + 5)/6)&&(mc_control_var[Gun_No].MC_var.Total_Mess_Size == sizeof(PC_PGN16256_FRAME))){
						memcpy(&mc_control_var[Gun_No].ml_data[0], &frame.data[2], 6);
						mc_control_var[Gun_No].MC_var.sequence++;
					}
				}else if(frame.data[1] == ((sizeof(PC_PGN16256_FRAME)+5) + 5)/6)
				{//最后一个数据包
					if(frame.data[1] == mc_control_var[Gun_No].MC_var.sequence)
					{
						memcpy(&mc_control_var[Gun_No].ml_data[(mc_control_var[Gun_No].MC_var.sequence-1)*6], &frame.data[2], 6);

						sum = 0;
						for(i=0; i<(3+sizeof(PC_PGN16256_FRAME)); i++)
						{
							sum +=  mc_control_var[Gun_No].ml_data[i];
						}

						if(((sum&0xff) ==  mc_control_var[Gun_No].ml_data[i])&&((sum&0xff00) == (mc_control_var[Gun_No].ml_data[i+1]<<8)))
						{
							MC_All_Frame[Gun_No].MC_Recved_PGN16256_Flag = 1;
							mc_control_var[Gun_No].MC_var.sequence = 0;
							memcpy(&MC_All_Frame[Gun_No].Pc_Pgn16256_data, &mc_control_var[Gun_No].ml_data[3], sizeof(PC_PGN16256_FRAME));
							
							Globa->Sys_RevCcu_Data[Gun_No].Gunlink = MC_All_Frame[Gun_No].Pc_Pgn16256_data.gun_link;
							Globa->Control_DC_Infor_Data[Gun_No].gun_link = MC_All_Frame[Gun_No].Pc_Pgn16256_data.gun_link;
							Globa->Sys_RevCcu_Data[Gun_No].Charge_State = MC_All_Frame[Gun_No].Pc_Pgn16256_data.Charge_state;
							Globa->Sys_RevCcu_Data[Gun_No].Modbule_On = MC_All_Frame[Gun_No].Pc_Pgn16256_data.Charge_modbule_on;
							Globa->Sys_RevCcu_Data[Gun_No].AC_Relay_needcontrol = MC_All_Frame[Gun_No].Pc_Pgn16256_data.AC_Relay_needcontrol;
		          Globa->Sys_RevCcu_Data[Gun_No].Need_Voltage = (MC_All_Frame[Gun_No].Pc_Pgn16256_data.need_voltage[1]<<8)|(MC_All_Frame[Gun_No].Pc_Pgn16256_data.need_voltage[0]);//0.1
		          Globa->Sys_RevCcu_Data[Gun_No].Need_Current = (MC_All_Frame[Gun_No].Pc_Pgn16256_data.need_current[1]<<8)|(MC_All_Frame[Gun_No].Pc_Pgn16256_data.need_current[0]); //0.1
		          Globa->Sys_RevCcu_Data[Gun_No].Output_Voltage = (MC_All_Frame[Gun_No].Pc_Pgn16256_data.Output_voltage[1]<<8)|(MC_All_Frame[Gun_No].Pc_Pgn16256_data.Output_voltage[0]);//0.1
		          Globa->Sys_RevCcu_Data[Gun_No].Output_Current = (MC_All_Frame[Gun_No].Pc_Pgn16256_data.Output_current[1]<<8)|(MC_All_Frame[Gun_No].Pc_Pgn16256_data.Output_current[0]);//0.1
							Globa->Sys_RevCcu_Data[Gun_No].Relay_State = MC_All_Frame[Gun_No].Pc_Pgn16256_data.Relay_State;  //继电器状态(高位为输出继电器，低位为并机接触器)
						  Globa->Sys_RevCcu_Data[Gun_No].Errorsystem = MC_All_Frame[Gun_No].Pc_Pgn16256_data.Errorsystem;
							Globa->Sys_RevCcu_Data[Gun_No].need_model_num = MC_All_Frame[Gun_No].Pc_Pgn16256_data.need_model_num;
							Globa->Sys_RevCcu_Data[Gun_No].Charger_good_num = MC_All_Frame[Gun_No].Pc_Pgn16256_data.Charger_good_num;
							Globa->Sys_MODGroup_Control_Data[Gun_No].Sys_Grp_OKNumber = MC_All_Frame[Gun_No].Pc_Pgn16256_data.Charger_good_num;
              Globa->Sys_RevCcu_Data[Gun_No].model_Output_voltage =  (MC_All_Frame[Gun_No].Pc_Pgn16256_data.model_Output_voltage[1]<<8)|(MC_All_Frame[Gun_No].Pc_Pgn16256_data.model_Output_voltage[0]);
						  Globa->Sys_RevCcu_Data[Gun_No].model_onoff =  MC_All_Frame[Gun_No].Pc_Pgn16256_data.model_onoff;
						  Globa->Sys_MODGroup_Control_Data[Gun_No].SYS_MobuleGrp_OutVolatge =  Globa->Sys_RevCcu_Data[Gun_No].model_Output_voltage;

						 //  printf("Gun_No =%d --start\n",Gun_No);
						  if(MC_DEBUG) printf("MC-----Gun_No =%d Globa->Sys_RevCcu_Data[%d].Gunlink = %d \n",Gun_No,Gun_No,Globa->Sys_RevCcu_Data[Gun_No].Gunlink);
						  if(MC_DEBUG) printf("MC-----Gun_No =%d time =%ld ,Globa->Sys_RevCcu_Data[Gun_No].Charge_State =%d,Globa->Sys_RevCcu_Data[Gun_No].Modbule_On = %d \n",Gun_No,time(NULL),Globa->Sys_RevCcu_Data[Gun_No].Charge_State,Globa->Sys_RevCcu_Data[Gun_No].Modbule_On);
						  if(MC_DEBUG) printf("MC-----Gun_No =%d Globa->Sys_RevCcu_Data[%d].AC_Relay_control = %d \n",Gun_No,Gun_No,Globa->Sys_RevCcu_Data[Gun_No].AC_Relay_needcontrol);
						  if(MC_DEBUG) printf(" \n MC-----Gun_No =%d Globa->Sys_RevCcu_Data[%d].Relay_State = %d Globa->Sys_Tcu2CcuControl_Data[Gun_No].WeaverRelay_control =%d time=%d \n",Gun_No,Gun_No,Globa->Sys_RevCcu_Data[Gun_No].Relay_State,Globa->Sys_Tcu2CcuControl_Data[Gun_No].WeaverRelay_control,time(NULL));
						  
							if(MC_DEBUG) printf(" \n MC-----Gun_No =%d Globa->Sys_RevCcu_Data[Gun_No].Output_Voltage= %d Globa->Sys_RevCcu_Data[Gun_No].Output_Current =%d \n",Gun_No,Globa->Sys_RevCcu_Data[Gun_No].Output_Voltage,Globa->Sys_RevCcu_Data[Gun_No].Output_Current);
							if(MC_DEBUG) printf("\n MC-----Gun_No =%d Globa->Sys_RevCcu_Data[Gun_No].Need_Voltage= %d Globa->Sys_RevCcu_Data[Gun_No].Need_Current= %d\n",Gun_No,Globa->Sys_RevCcu_Data[Gun_No].Need_Voltage,Globa->Sys_RevCcu_Data[Gun_No].Need_Current);
							if(MC_DEBUG) printf(" \n MC-----Gun_No =%d Globa->Sys_RevCcu_Data[Gun_No].model_Output_voltage= %d  Globa->Sys_MODGroup_Control_Data[Gun_No].SYS_MobuleGrp_OutVolatge = %d  Globa->Sys_RevCcu_Data[Gun_No].model_onoff =%d \n",Gun_No,Globa->Sys_RevCcu_Data[Gun_No].model_Output_voltage,Globa->Sys_MODGroup_Control_Data[Gun_No].SYS_MobuleGrp_OutVolatge,Globa->Sys_RevCcu_Data[Gun_No].model_onoff);
						 	if(MC_DEBUG) printf(" \n MC-----Gun_No =%d Globa->Sys_RevCcu_Data[Gun_No].need_model_num= %d Globa->Sys_RevCcu_Data[Gun_No].Charger_good_num =%d \n",Gun_No, Globa->Sys_RevCcu_Data[Gun_No].need_model_num,Globa->Sys_RevCcu_Data[Gun_No].Charger_good_num);

					//	 printf("Gun_No =%d --end\n",Gun_No);
					
					    for(i = 0;i<Globa->Charger_param.Contactor_Line_Num;i++)
							{
								if((Gun_No + 1) == Globa->Charger_Contactor_param_data[i].Correspond_control_unit_address)
								{
							if((Globa->Sys_RevCcu_Data[Gun_No].Relay_State&0x0f) == 1)//实时状态
							{
								SETBITSTO1(Globa->rContactor_Actual_State[Gun_No],0);
								SETBITSTO1(Globa->rContactor_Actual_State[Gun_No],1);
							}else{
								SETBITSTO0(Globa->rContactor_Actual_State[Gun_No],0);
								SETBITSTO0(Globa->rContactor_Actual_State[Gun_No],1);
							}
						 	if(MC_DEBUG) printf(" \n MC-----Gun_No =%d Globa->rContactor_Actual_State[Gun_No]= %d \n",Gun_No,Globa->rContactor_Actual_State[Gun_No]);

							//因为目前状态反馈是接在一起的
							if(((Globa->Sys_RevCcu_Data[Gun_No].Relay_State&0x0f) == 1)&&(Globa->Sys_Tcu2CcuControl_Data[Gun_No].WeaverRelay_control == 0))//黏连
							{
								if(MC_DEBUG) printf("\n-----读取的状态和控制状态不一样:Globa->Switch_State =%d Globa->rContactor_Control_State=%d\n");
								gAdhesion_fault_Cont[Gun_No]++;
								if(gAdhesion_fault_Cont[Gun_No] >= 10){
									SETBITSTO1(Globa->rContactor_Adhesion_fault[Gun_No],0);
									SETBITSTO1(Globa->rContactor_Adhesion_fault[Gun_No],1);
									gAdhesion_fault_Cont[Gun_No] = 0; 
									if(grContactor_Adhesion_fault[Gun_No] != 1){//黏连
										grContactor_Adhesion_fault[Gun_No] = 1;
										sent_warning_message(0x99, 40, Gun_No+1);
									}
								}
							}else{
								SETBITSTO0(Globa->rContactor_Adhesion_fault[Gun_No],0);
								SETBITSTO0(Globa->rContactor_Adhesion_fault[Gun_No],1);
								gAdhesion_fault_Cont[Gun_No] = 0; 
								if(grContactor_Adhesion_fault[Gun_No] != 0){//黏连
									grContactor_Adhesion_fault[Gun_No] = 0;
									sent_warning_message(0x98, 40, Gun_No+1);
								}
							}
						//	printf("\n---Gun_No = %d gMisoperation_fault_Cont[Gun_No] =%d Relay_State =%d  WeaverRelay_control =%d \n",Gun_No,gMisoperation_fault_Cont[Gun_No],(Globa->Sys_RevCcu_Data[Gun_No].Relay_State&0x0f),Globa->Sys_Tcu2CcuControl_Data[Gun_No].WeaverRelay_control);
							if((((Globa->Sys_RevCcu_Data[Gun_No].Relay_State&0x0f) == 0)&&(Globa->Sys_Tcu2CcuControl_Data[Gun_No].WeaverRelay_control == 1))
								||(((Globa->Sys_RevCcu_Data[Gun_No].Relay_State&0x0f) == 1)&&(Globa->Sys_Tcu2CcuControl_Data[Gun_No].WeaverRelay_control == 0)))
							{//勿动
								gMisoperation_fault_Cont[Gun_No]++;
								if(gMisoperation_fault_Cont[Gun_No] >= 10){
									SETBITSTO1(Globa->rContactor_Misoperation_fault[Gun_No],0);
									SETBITSTO1(Globa->rContactor_Misoperation_fault[Gun_No],1);
									gMisoperation_fault_Cont[Gun_No] = 0; 
									if(grContactor_Misoperation_fault[Gun_No] != 1){//拒动
										grContactor_Misoperation_fault[Gun_No] = 1;
										sent_warning_message(0x99, 41, Gun_No+1);
									}
								}
							}else{
								SETBITSTO0(Globa->rContactor_Misoperation_fault[Gun_No],0);
								SETBITSTO0(Globa->rContactor_Misoperation_fault[Gun_No],1);
								gMisoperation_fault_Cont[Gun_No] = 0; 
								if(grContactor_Misoperation_fault[Gun_No] != 0){//拒动
									grContactor_Misoperation_fault[Gun_No] = 0;
									sent_warning_message(0x98, 41, Gun_No+1);
								}
							}
							
							if(((Globa->Sys_RevCcu_Data[Gun_No].Relay_State&0x0f) == 0)&&(Globa->Sys_Tcu2CcuControl_Data[Gun_No].WeaverRelay_control == 1))//拒动
							{	
								gMiss_fault_Cont[Gun_No]++;
								if(gMiss_fault_Cont[Gun_No] >= 10){
									SETBITSTO1(Globa->rContactor_Miss_fault[Gun_No],0);
									SETBITSTO1(Globa->rContactor_Miss_fault[Gun_No],1);
									gMiss_fault_Cont[Gun_No] = 0; 
									if(grContactor_Miss_fault[Gun_No] != 1){//拒动
										grContactor_Miss_fault[Gun_No] = 1;
										sent_warning_message(0x99, 42, Gun_No+1);
									}
								}
							}else{
								SETBITSTO0(Globa->rContactor_Miss_fault[Gun_No],0);
								SETBITSTO0(Globa->rContactor_Miss_fault[Gun_No],1);
								gMiss_fault_Cont[Gun_No] = 0; 
								if(grContactor_Miss_fault[Gun_No] != 0){//拒动
									grContactor_Miss_fault[Gun_No] = 0;
									sent_warning_message(0x98, 42, Gun_No+1);
								}
							}
							    break;
								}
							}
							
							if((abs(pre_outvolt[Gun_No] - Globa->Sys_RevCcu_Data[Gun_No].Output_Voltage) >= 100)||(abs(pre_out_current[Gun_No] - Globa->Sys_RevCcu_Data[Gun_No].Output_Current)>= 10)\
								||( abs(pre_need_volt[Gun_No] - Globa->Sys_RevCcu_Data[Gun_No].Need_Voltage) >= 100) ||(abs(pre_need_current[Gun_No] - Globa->Sys_RevCcu_Data[Gun_No].Need_Current)>= 10)\
								||(pre_Gunlink[Gun_No] !=  Globa->Sys_RevCcu_Data[Gun_No].Gunlink)||( pre_Charged_State[Gun_No] !=  Globa->Sys_RevCcu_Data[Gun_No].Charge_State)
								||( pre_Modbule_On[Gun_No] !=  Globa->Sys_RevCcu_Data[Gun_No].Modbule_On)
								||(Globa->Sys_RevCcu_Data[Gun_No].Relay_State != pre_Relay_State[Gun_No] ))
							{
								pre_outvolt[Gun_No] = Globa->Sys_RevCcu_Data[Gun_No].Output_Voltage;
								pre_need_volt[Gun_No] = Globa->Sys_RevCcu_Data[Gun_No].Need_Voltage;
								pre_need_current[Gun_No] =   Globa->Sys_RevCcu_Data[Gun_No].Need_Current;
								pre_out_current[Gun_No] =  Globa->Sys_RevCcu_Data[Gun_No].Output_Current;
								pre_Gunlink[Gun_No] = Globa->Sys_RevCcu_Data[Gun_No].Gunlink;
								pre_Modbule_On[Gun_No] =  Globa->Sys_RevCcu_Data[Gun_No].Modbule_On;
								pre_Charged_State[Gun_No] =  Globa->Sys_RevCcu_Data[Gun_No].Charge_State;
								pre_Relay_State[Gun_No] =  Globa->Sys_RevCcu_Data[Gun_No].Relay_State;
								RunEventLogOut(Gun_No,"控制信息请求帧:%d枪：枪连接 =%d 充电标志=%d 模块开启=%d 需求电压(0.1)=%dV,需求电流(0.1)=%dA,输出电压(0.1)=%dV,输出电流(0.1)=%dA 接触器状态=%d",Gun_No+1,pre_Gunlink[Gun_No],pre_Charged_State[Gun_No],pre_Modbule_On[Gun_No],pre_need_volt[Gun_No],pre_need_current[Gun_No],pre_outvolt[Gun_No],pre_out_current[Gun_No],(pre_Relay_State[Gun_No]&0x0f));
							}
						}
					}
				}else
				{//中间帧
					if(frame.data[1] == mc_control_var[Gun_No].MC_var.sequence)
					{
						memcpy(&mc_control_var[Gun_No].ml_data[(mc_control_var[Gun_No].MC_var.sequence-1)*6], &frame.data[2], 6);
						mc_control_var[Gun_No].MC_var.sequence++;
					}
				}
			}			
			break;
		}
		case PGN512_ID_CM:{   //启动充电响应帧
			MC_PGN512_FRAME * pdata = (MC_PGN512_FRAME *)frame.data;
		//	printf("------启动充电响应帧------Gun_No = %d\n",Gun_No);

			MC_All_Frame[Gun_No].MC_Recved_PGN512_Flag = 1;

			break;
		}
		case PGN1024_ID_CM:{  //停止充电响应帧
			MC_PGN1024_FRAME * pdata = (MC_PGN1024_FRAME *)frame.data;

			MC_All_Frame[Gun_No].MC_Recved_PGN1024_Flag = 1;

			break;
		}
		case PGN2560_ID_CM:{  //设置充电参数信息响应帧
			MC_PGN2560_FRAME * pdata = (MC_PGN2560_FRAME *)frame.data;

			MC_All_Frame[Gun_No].MC_Recved_PGN2560_Flag = 1;

			break;
		}
		case PGN2816_ID_CM:{  //请求充电参数信息
			MC_PGN2816_FRAME * pdata = (MC_PGN2816_FRAME *)frame.data;

			memcpy(Globa->contro_ver[Gun_No], &pdata->Cvel[0], sizeof(pdata->Cvel));
			char*contro_ver = strstr(Globa->contro_ver[Gun_No],".");
			contro_ver++;
			float contro_ver_value = atof (contro_ver);
			SysControlLogOut("%d#枪请求充电参数信息：控制版本:%0.2f ",Gun_No+1,contro_ver_value);
	
			//memcpy(&Globa->contro_ver[[0], &pdata->Cvel[0], sizeof(pdata->Cvel));
			MC_All_Frame[Gun_No].MC_Recved_PGN2816_Flag = 1;

			break;
		}
		case PGN3328_ID_CM:{ //直流分流器量程确认帧
			MC_PGN3328_FRAME * pdata = (MC_PGN3328_FRAME *)frame.data;
			MC_All_Frame[Gun_No].MC_Recved_PGN3328_Flag = 1;
			break;
		}
		
	  case PGN4096_ID_CM:{ //功率调节反馈，默认为成功，
			Power_PGN4096_FRAME * pdata = (Power_PGN4096_FRAME *)frame.data;
			MC_All_Frame[Gun_No].MC_Recved_Power_PGN4096_Flag = 1;
			break;
		}
		
		case PGN4352_ID_CM:{  //启动完成帧
			MC_PGN4352_FRAME * pdata = (MC_PGN4352_FRAME *)frame.data;

			Globa->Control_DC_Infor_Data[Gun_No].BATT_type = pdata->BATT_type;
			Globa->Control_DC_Infor_Data[Gun_No].BMS_H_voltage = ((pdata->BMS_H_vol[1]<<8) + pdata->BMS_H_vol[0])*100;
			Globa->Control_DC_Infor_Data[Gun_No].BMS_H_current = ((pdata->BMS_H_cur[1]<<8) + pdata->BMS_H_cur[0])*10;
      Globa->Control_DC_Infor_Data[Gun_No].BMS_batt_start_SOC = 0;//Globa->Control_DC_Infor_Data[Gun_No].BMS_batt_SOC;

			if((Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_falg == 1)&&(Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Hostgun == 1))//主枪
			{
				Other_Gun_No = Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Othergun;

				Globa->Control_DC_Infor_Data[Other_Gun_No].BATT_type = Globa->Control_DC_Infor_Data[Gun_No].BATT_type;
				Globa->Control_DC_Infor_Data[Other_Gun_No].BMS_H_voltage = Globa->Control_DC_Infor_Data[Gun_No].BMS_H_voltage;
				Globa->Control_DC_Infor_Data[Other_Gun_No].BMS_H_current = Globa->Control_DC_Infor_Data[Gun_No].BMS_H_current;
				Globa->Control_DC_Infor_Data[Other_Gun_No].BMS_batt_start_SOC = 0;//Globa->Control_DC_Infor_Data[Gun_No].BMS_batt_SOC;
			}
			MC_All_Frame[Gun_No].MC_Recved_PGN4352_Flag = 1;

			break;
		}
		case PGN4864_ID_CM:{  //停止完成帧
			MC_PGN4864_FRAME * pdata = (MC_PGN4864_FRAME *)frame.data;

			Globa->Control_DC_Infor_Data[Gun_No].ctl_over_flag = pdata->cause[0] + (pdata->cause[1]<<8);
			if((Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_falg == 1)&&(Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Hostgun == 1))//主枪收到停止完成帧
			{
				Other_Gun_No = Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Othergun;
				Globa->Control_DC_Infor_Data[Other_Gun_No].ctl_over_flag = Globa->Control_DC_Infor_Data[Gun_No].ctl_over_flag;
			}
			MC_All_Frame[Gun_No].MC_Recved_PGN4864_Flag = 1;

			break;
		}
		case PGN12800_ID_CM:{ //心跳响应帧
			MC_All_Frame[Gun_No].MC_Recved_PGN12800_Flag = 1;

			break;
		}
		case PGN8448_ID_CM:{   //遥信数据帧
			MC_PGN8448_FRAME * pdata = (MC_PGN8448_FRAME *)frame.data;
			
			MC_alarm_deal(pdata->gun, pdata->state, pdata->alarm_sn, pdata->fault, pdata->num[0]+(pdata->num[1]<<8));

			MC_All_Frame[Gun_No].MC_Recved_PGN8448_Flag = 1;

			break;
		}
		case PGN8704_ID_CM:
		{   //遥测数据帧
			if((frame.data[1] >= 1)&&(frame.data[1] <= ((sizeof(MC_PGN8704_FRAME)+5) + 5)/6))
			{
				if(frame.data[1] == 1)
				{//第一个数据包
					mc_control_var[Gun_No].MC_var.sequence = frame.data[1];
					mc_control_var[Gun_No].MC_var.Total_Num_Packet = frame.data[2];
					mc_control_var[Gun_No].MC_var.Total_Mess_Size = frame.data[3] + (frame.data[4]<<8);

					if((mc_control_var[Gun_No].MC_var.Total_Num_Packet == ((sizeof(MC_PGN8704_FRAME)+5) + 5)/6)&&(mc_control_var[Gun_No].MC_var.Total_Mess_Size == sizeof(MC_PGN8704_FRAME))){
						memcpy(&mc_control_var[Gun_No].ml_data[0], &frame.data[2], 6);
						mc_control_var[Gun_No].MC_var.sequence++;
					}
				}else if(frame.data[1] == ((sizeof(MC_PGN8704_FRAME)+5) + 5)/6)
				{//最后一个数据包
					if(frame.data[1] == mc_control_var[Gun_No].MC_var.sequence)
					{
						memcpy(&mc_control_var[Gun_No].ml_data[(mc_control_var[Gun_No].MC_var.sequence-1)*6], &frame.data[2], 6);

						sum = 0;
						for(i=0; i<(3+sizeof(MC_PGN8704_FRAME)); i++)
						{
							sum +=  mc_control_var[Gun_No].ml_data[i];
						}

						if(((sum&0xff) ==  mc_control_var[Gun_No].ml_data[i])&&((sum&0xff00) == (mc_control_var[Gun_No].ml_data[i+1]<<8)))
						{
							MC_All_Frame[Gun_No].MC_Recved_PGN8704_Flag = 1;
							mc_control_var[Gun_No].MC_var.sequence = 0;
							memcpy(&MC_All_Frame[Gun_No].MC_PGN8704_frame, &mc_control_var[Gun_No].ml_data[3], sizeof(MC_PGN8704_FRAME));
		  				
							Globa->Control_DC_Infor_Data[Gun_No].ctl_state =   MC_All_Frame[Gun_No].MC_PGN8704_frame.state;

							Globa->Control_DC_Infor_Data[Gun_No].Output_voltage = ((MC_All_Frame[Gun_No].MC_PGN8704_frame.Output_voltage[1]<<8) + MC_All_Frame[Gun_No].MC_PGN8704_frame.Output_voltage[0])*100;
							Globa->Control_DC_Infor_Data[Gun_No].Output_current = ((MC_All_Frame[Gun_No].MC_PGN8704_frame.Output_current[1]<<8) + MC_All_Frame[Gun_No].MC_PGN8704_frame.Output_current[0])*10;
							Globa->Control_DC_Infor_Data[Gun_No].line_v = MC_All_Frame[Gun_No].MC_PGN8704_frame.line_v;
							Globa->Control_DC_Infor_Data[Gun_No].Charge_Gun_Temp = MC_All_Frame[Gun_No].MC_PGN8704_frame.TEMP; //充电枪温度过高

							if((Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_falg == 1)&&(Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Hostgun == 0)&&(Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Othergun].charger_start != 0))
							{
								Other_Gun_No = Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Othergun;//主枪号
								//主枪启动后把主枪的BMS信息复制给辅枪
								Globa->Control_DC_Infor_Data[Gun_No].BMS_batt_SOC =  Globa->Control_DC_Infor_Data[Other_Gun_No].BMS_batt_SOC;
								if(0 == Globa->Control_DC_Infor_Data[Gun_No].BMS_batt_start_SOC){
									Globa->Control_DC_Infor_Data[Gun_No].BMS_batt_start_SOC = Globa->Control_DC_Infor_Data[Gun_No].BMS_batt_SOC;
								//	Globa->Control_DC_Infor_Data[Gun_No].BMS_current_voltage = 	Globa->Control_DC_Infor_Data[Gun_No].Output_voltage；
								}
								Globa->Control_DC_Infor_Data[Gun_No].BMS_cell_HV = Globa->Control_DC_Infor_Data[Other_Gun_No].BMS_cell_HV;
								Globa->Control_DC_Infor_Data[Gun_No].Charge_Mode = Globa->Control_DC_Infor_Data[Other_Gun_No].Charge_Mode;        //8充电模式
								Globa->Control_DC_Infor_Data[Gun_No].BMS_cell_HV_NO = Globa->Control_DC_Infor_Data[Other_Gun_No].BMS_cell_HV_NO;  //8最高单体电压序号
								Globa->Control_DC_Infor_Data[Gun_No].BMS_cell_HT = Globa->Control_DC_Infor_Data[Other_Gun_No].BMS_cell_HT;
								Globa->Control_DC_Infor_Data[Gun_No].BMS_cell_HT_NO = Globa->Control_DC_Infor_Data[Other_Gun_No].BMS_cell_HT_NO;
								Globa->Control_DC_Infor_Data[Gun_No].BMS_cell_LT = Globa->Control_DC_Infor_Data[Other_Gun_No].BMS_cell_LT;
								Globa->Control_DC_Infor_Data[Gun_No].BMS_cell_LT_NO = Globa->Control_DC_Infor_Data[Other_Gun_No].BMS_cell_LT_NO;
								Globa->Control_DC_Infor_Data[Gun_No].BMS_need_time= Globa->Control_DC_Infor_Data[Other_Gun_No].BMS_need_time;
								Globa->Control_DC_Infor_Data[Gun_No].need_voltage = Globa->Control_DC_Infor_Data[Other_Gun_No].need_voltage;
								Globa->Control_DC_Infor_Data[Gun_No].need_current = Globa->Control_DC_Infor_Data[Other_Gun_No].need_current;
							}
							else//主枪 
							{ 	
								if(MC_All_Frame[Gun_No].MC_PGN8704_frame.SOC > 0)//有获取到SOC
									Globa->Control_DC_Infor_Data[Gun_No].BMS_batt_SOC =   MC_All_Frame[Gun_No].MC_PGN8704_frame.SOC;
						   	if(0 == Globa->Control_DC_Infor_Data[Gun_No].BMS_batt_start_SOC){
									Globa->Control_DC_Infor_Data[Gun_No].BMS_batt_start_SOC = Globa->Control_DC_Infor_Data[Gun_No].BMS_batt_SOC;
									//Globa->Control_DC_Infor_Data[Gun_No].BMS_current_voltage = 	Globa->Control_DC_Infor_Data[Gun_No].Output_voltage；
								}
								Globa->Control_DC_Infor_Data[Gun_No].BMS_cell_HV = (MC_All_Frame[Gun_No].MC_PGN8704_frame.BMS_cell_HV[1]<<8) + MC_All_Frame[Gun_No].MC_PGN8704_frame.BMS_cell_HV[0];
								Globa->Control_DC_Infor_Data[Gun_No].Charge_Mode = MC_All_Frame[Gun_No].MC_PGN8704_frame.Charge_Mode;        //8充电模式
								Globa->Control_DC_Infor_Data[Gun_No].BMS_cell_HV_NO = MC_All_Frame[Gun_No].MC_PGN8704_frame.BMS_cell_HV_NO;  //8最高单体电压序号
								Globa->Control_DC_Infor_Data[Gun_No].BMS_cell_HT = MC_All_Frame[Gun_No].MC_PGN8704_frame.BMS_cell_HT;
								Globa->Control_DC_Infor_Data[Gun_No].BMS_cell_HT_NO = MC_All_Frame[Gun_No].MC_PGN8704_frame.BMS_cell_HT_NO;
								Globa->Control_DC_Infor_Data[Gun_No].BMS_cell_LT = MC_All_Frame[Gun_No].MC_PGN8704_frame.BMS_cell_LT;
								Globa->Control_DC_Infor_Data[Gun_No].BMS_cell_LT_NO = MC_All_Frame[Gun_No].MC_PGN8704_frame.BMS_cell_LT_NO;
								Globa->Control_DC_Infor_Data[Gun_No].BMS_need_time  = (MC_All_Frame[Gun_No].MC_PGN8704_frame.BMS_need_time[1]<<8) + MC_All_Frame[Gun_No].MC_PGN8704_frame.BMS_need_time[0];
								Globa->Control_DC_Infor_Data[Gun_No].need_voltage = ((MC_All_Frame[Gun_No].MC_PGN8704_frame.need_voltage[1]<<8) + MC_All_Frame[Gun_No].MC_PGN8704_frame.need_voltage[0])*100;
								Globa->Control_DC_Infor_Data[Gun_No].need_current = ((MC_All_Frame[Gun_No].MC_PGN8704_frame.need_current[1]<<8) + MC_All_Frame[Gun_No].MC_PGN8704_frame.need_current[0])*10;
							}
								//test
							//printf("recv plug %d PGN8704 data,linv=%d\n",Gun_No,Globa->Control_DC_Infor_Data[Gun_No].line_v);
						}
					}
				}else
				{//中间帧
					if(frame.data[1] == mc_control_var[Gun_No].MC_var.sequence)
					{
						memcpy(&mc_control_var[Gun_No].ml_data[(mc_control_var[Gun_No].MC_var.sequence-1)*6], &frame.data[2], 6);
						mc_control_var[Gun_No].MC_var.sequence++;
					}
				}
			}
			break;
		}
		case PGN9216_ID_CM:
		{	//双枪同时充的机器只配置一个交流采集盒 接在1号控制板上
			if((frame.data[1] >= 1)&&(frame.data[1] <= ((sizeof(AC_MEASURE_DATA)+5) + 5)/6))
			{
				if(frame.data[1] == 1)
				{//第一个数据包
					mc_control_var[Gun_No].MC_var.sequence = frame.data[1];
					mc_control_var[Gun_No].MC_var.Total_Num_Packet = frame.data[2];
					mc_control_var[Gun_No].MC_var.Total_Mess_Size = frame.data[3] + (frame.data[4]<<8);

					if((mc_control_var[Gun_No].MC_var.Total_Num_Packet == ((sizeof(AC_MEASURE_DATA)+5) + 5)/6)&&(mc_control_var[Gun_No].MC_var.Total_Mess_Size == sizeof(AC_MEASURE_DATA)))
					{
						memcpy(&mc_control_var[Gun_No].ml_data[0], &frame.data[2], 6);
						mc_control_var[Gun_No].MC_var.sequence++;
					}
				}else if(frame.data[1] == ((sizeof(AC_MEASURE_DATA)+5) + 5)/6)
				{//最后一个数据包
					if(frame.data[1] == mc_control_var[Gun_No].MC_var.sequence)
					{
						memcpy(&mc_control_var[Gun_No].ml_data[(mc_control_var[Gun_No].MC_var.sequence-1)*6], &frame.data[2], 6);

						sum = 0;
						for(i=0; i<(3+sizeof(AC_MEASURE_DATA)); i++)
						{
							sum +=  mc_control_var[Gun_No].ml_data[i];
						}

						if(((sum&0xff) ==  mc_control_var[Gun_No].ml_data[i])&&((sum&0xff00) == (mc_control_var[Gun_No].ml_data[i+1]<<8)))
						{
							MC_All_Frame[Gun_No].MC_Recved_PGN9216_Flag = 1;
							mc_control_var[Gun_No].MC_var.sequence = 0;

							memcpy(&AC_measure_data.reserve1, &mc_control_var[Gun_No].ml_data[3], sizeof(AC_MEASURE_DATA));
						}
					}
				}else
				{//中间帧
					if(frame.data[1] == mc_control_var[Gun_No].MC_var.sequence)
					{
						memcpy(&mc_control_var[Gun_No].ml_data[(mc_control_var[Gun_No].MC_var.sequence-1)*6], &frame.data[2], 6);
						mc_control_var[Gun_No].MC_var.sequence++;
					}
				}
			}
			break;
		}
		case PGN9472_ID_CM:{   //遥信充电机状态帧
			MC_PGN9472_FRAME * pdata = (MC_PGN9472_FRAME *)frame.data;
			
			MC_state_deal(pdata->gun, pdata->state);

			MC_All_Frame[Gun_No].MC_Recved_PGN8448_Flag = 1;

			break;
		}
  	case PGN21248_ID_CM:{////升级请求报文响应报文(PF:0x53) 控制器--计费单元
			MC_PGN21248_FRAME * pdata = (MC_PGN21248_FRAME *)frame.data;
			if(pdata->Success_flag == 0)
			{
				MC_All_Frame[Gun_No].MC_Recved_PGN21248_Flag = 1;
			}else
			{
				MC_All_Frame[Gun_No].MC_Recved_PGN21248_Flag = 0;
			}
			break;
		}
		case PGN21504_ID_CM:{//请求升级数据报文帧(PF:0x54)  控制器--计费单元
			MC_All_Frame[Gun_No].MC_Recved_PGN21504_Flag = 1;
			break;
		}
		case PGN22016_ID_CM:
		{
			MC_PGN22016_FRAME * pdata = (MC_PGN22016_FRAME *)frame.data;
			if(pdata->Success_flag == 0)
			{//成功发送下一帧报文
				mc_control_var[Gun_No].send_Tran_Packet_Number = ((pdata->Tran_Packet_Number[1]<<8)|pdata->Tran_Packet_Number[0]) + 1;
				if((Globa->Control_Upgrade_Firmware_Info[Gun_No].Send_Tran_All_Bytes_0K >= Globa->Control_Upgrade_Firmware_Info[Gun_No].Firmware_ALL_Bytes)&&(Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_setStep == 0x02))
				{ //说明发送完毕并且也接收到最后一帧数据返回
					if(mc_control_var[Gun_No].open_fp != NULL)
					{	
						fclose(mc_control_var[Gun_No].open_fp);
						mc_control_var[Gun_No].open_fp = NULL;
					}
					Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_success = 2;
					Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_setStep = 0x10;
					MC_All_Frame[Gun_No].MC_Recved_PGN22016_Flag = 0;
					break;	
				}
				MC_All_Frame[Gun_No].MC_Recved_PGN22016_Flag = 1;
			}else
			{ //失败重新发送
				mc_control_var[Gun_No].send_Tran_Packet_Number = ((pdata->Tran_Packet_Number[1]<<8)|pdata->Tran_Packet_Number[0]);
			}
			break;
		}
		
		case PGN3840_ID_CM:{//控制板反馈电子锁解锁动作
		  MC_All_Frame[Gun_No].MC_Recved_PGN3840_Flag = 1;
			break;	
    }
		case PGN25088_ID_CM:{ //接收BMS上传过来的剩余变量数据。(比较固定的充电中不会更改的)
			if((frame.data[1] >= 1)&&(frame.data[1] <= ((sizeof(MC_PGN25088_FRAME)+5) + 5)/6)){
				if(frame.data[1] == 1){//第一个数据包
					mc_control_var[Gun_No].MC_var.sequence = frame.data[1];
					mc_control_var[Gun_No].MC_var.Total_Num_Packet = frame.data[2];
					mc_control_var[Gun_No].MC_var.Total_Mess_Size = frame.data[3] + (frame.data[4]<<8);

					if((mc_control_var[Gun_No].MC_var.Total_Num_Packet == ((sizeof(MC_PGN25088_FRAME)+5) + 5)/6)&&(mc_control_var[Gun_No].MC_var.Total_Mess_Size == sizeof(MC_PGN25088_FRAME))){
						memcpy(&mc_control_var[Gun_No].ml_data[0], &frame.data[2], 6);
						mc_control_var[Gun_No].MC_var.sequence++;
					}
				}else if(frame.data[1] == ((sizeof(MC_PGN25088_FRAME)+5) + 5)/6){//最后一个数据包
					if(frame.data[1] == mc_control_var[Gun_No].MC_var.sequence){
						memcpy(&mc_control_var[Gun_No].ml_data[(mc_control_var[Gun_No].MC_var.sequence-1)*6], &frame.data[2], 6);

						sum = 0;
						for(i=0; i<(3+sizeof(MC_PGN25088_FRAME)); i++){
							sum +=  mc_control_var[Gun_No].ml_data[i];
						}

						if(((sum&0xff) ==  mc_control_var[Gun_No].ml_data[i])&&((sum&0xff00) == (mc_control_var[Gun_No].ml_data[i+1]<<8))){
							MC_All_Frame[Gun_No].MC_Recved_PGN25088_Flag = 1;
							mc_control_var[Gun_No].MC_var.sequence = 0;
							memcpy(&MC_All_Frame[Gun_No].MC_PGN25088_frame, &mc_control_var[Gun_No].ml_data[3], sizeof(MC_PGN25088_FRAME));
							memcpy(&Globa->Control_DC_Infor_Data[Gun_No].BMS_Vel[0], &MC_All_Frame[Gun_No].MC_PGN25088_frame.BMS_Vel, sizeof(MC_All_Frame[Gun_No].MC_PGN25088_frame.BMS_Vel));
							Globa->Control_DC_Infor_Data[Gun_No].BMS_rate_AH = ((MC_All_Frame[Gun_No].MC_PGN25088_frame.Battery_Rate_AH[1]<<8) + MC_All_Frame[Gun_No].MC_PGN25088_frame.Battery_Rate_AH[0]);
							Globa->Control_DC_Infor_Data[Gun_No].BMS_rate_voltage = ((MC_All_Frame[Gun_No].MC_PGN25088_frame.Battery_Rate_Vol[1]<<8) + MC_All_Frame[Gun_No].MC_PGN25088_frame.Battery_Rate_Vol[0]);
							Globa->Control_DC_Infor_Data[Gun_No].Battery_Rate_KWh = ((MC_All_Frame[Gun_No].MC_PGN25088_frame.Battery_Rate_KWh[1]<<8) + MC_All_Frame[Gun_No].MC_PGN25088_frame.Battery_Rate_KWh[0]);
							Globa->Control_DC_Infor_Data[Gun_No].Cell_H_Cha_Temp =   MC_All_Frame[Gun_No].MC_PGN25088_frame.Cell_H_Cha_Temp;
							Globa->Control_DC_Infor_Data[Gun_No].Cell_H_Cha_Vol = (MC_All_Frame[Gun_No].MC_PGN25088_frame.Cell_H_Cha_Vol[1]<<8) + MC_All_Frame[Gun_No].MC_PGN25088_frame.Cell_H_Cha_Vol[0];
							memcpy(&Globa->Control_DC_Infor_Data[Gun_No].VIN[0], &MC_All_Frame[Gun_No].MC_PGN25088_frame.VIN, sizeof(MC_All_Frame[Gun_No].MC_PGN25088_frame.VIN));
							//memcpy(&Globa->Control_DC_Infor_Data[Gun_No].VIN[0],"12345678901234567",17);//test
							Globa->Control_DC_Infor_Data[Gun_No].BMS_Info_falg = 1;
							
							if((Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_falg == 1)&&(Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Hostgun == 1))//主枪
							{
								Other_Gun_No = Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Othergun;
								memcpy(&Globa->Control_DC_Infor_Data[Other_Gun_No].BMS_Vel[0], &Globa->Control_DC_Infor_Data[Gun_No].BMS_Vel[0], sizeof(Globa->Control_DC_Infor_Data[Gun_No].BMS_Vel));
								Globa->Control_DC_Infor_Data[Other_Gun_No].BMS_rate_AH = Globa->Control_DC_Infor_Data[Gun_No].BMS_rate_AH;
								Globa->Control_DC_Infor_Data[Other_Gun_No].BMS_rate_voltage = Globa->Control_DC_Infor_Data[Gun_No].BMS_rate_voltage;
								Globa->Control_DC_Infor_Data[Other_Gun_No].Battery_Rate_KWh = Globa->Control_DC_Infor_Data[Gun_No].Battery_Rate_KWh;
								Globa->Control_DC_Infor_Data[Other_Gun_No].Cell_H_Cha_Temp =  Globa->Control_DC_Infor_Data[Gun_No].Cell_H_Cha_Temp;
								Globa->Control_DC_Infor_Data[Other_Gun_No].Cell_H_Cha_Vol =  Globa->Control_DC_Infor_Data[Gun_No].Cell_H_Cha_Vol;
								memcpy(&Globa->Control_DC_Infor_Data[Other_Gun_No].VIN[0], &Globa->Control_DC_Infor_Data[Gun_No].VIN[0], sizeof(Globa->Control_DC_Infor_Data[Gun_No].VIN));
								Globa->Control_DC_Infor_Data[Other_Gun_No].BMS_Info_falg = Globa->Control_DC_Infor_Data[Gun_No].BMS_Info_falg;
							}
						}
					}
				}else{//中间帧
					if(frame.data[1] == mc_control_var[Gun_No].MC_var.sequence){
						memcpy(&mc_control_var[Gun_No].ml_data[(mc_control_var[Gun_No].MC_var.sequence-1)*6], &frame.data[2], 6);
						mc_control_var[Gun_No].MC_var.sequence++;
					}
				}
			}
			break;
		}
		default:{
			if(0){
				printf("接收到未知 ID %08x\n", frame.can_id&CAN_EFF_MASK);
			}
			break;
		}
	}
}

/*********************************************************
**descriGBion：充电器发送数据函数
***parameter  :none
***return		  :none
**********************************************************/
int MC_sent_data(INT32 fd, UINT32 PGN, UINT32 flag, UINT32 gun)
{
	int ret;
	unsigned char ac_con = 0,gun_link=0,gun_config = 2;
	UINT32 data;
	UINT32 i, sum;
	struct can_frame frame;
	struct tm tm_t;
	time_t systime=0;	
	UINT8 Gun_No=0,gun_state = 0;
	UINT8 mc_data[50];  //多帧数据发送的暂存缓存区

	if(gun > 0 && gun <= CONTROL_DC_NUMBER)
		Gun_No = gun - 1;//since 0
	else//never in
		return -1;
		
	memset(&frame.data[0], 0xff, sizeof(frame.data)); //不用数据项填1
	if((Globa->Charger_param.gun_config == 2)&&(Globa->Charger_param.support_double_gun_power_allocation ==1))
	{
		gun_config = 4;
	}else{
		gun_config = Globa->Charger_param.gun_config;//4;
	}
	switch(PGN){
		case PGN16512_ID_PC:{//模快输出继电器控制
			frame.can_id = PGN16512_ID_PC;
			frame.can_dlc = 8;

			PC_PGN16512_FRAME * pdata = (PC_PGN16512_FRAME *)frame.data;
			gun_link=0;	
			//如果有任意一把枪的状态是4（充电）或者7（等待充电），则闭合接触器，否则断开接触器

			if(gun_config > CONTROL_DC_NUMBER)
				gun_config = CONTROL_DC_NUMBER;
			for(i=0;i<gun_config;i++)
			{
				if(Globa->Control_DC_Infor_Data[i].AC_Relay_control)//有1把枪要求合输入接触器或风扇
				{
					gun_link = 1;
					break;
				}
			}								
			if(1 == gun_link)//至少有1个枪需求闭合输入接触器和风扇====
			{//交流接触器闭合和风扇闭合					
				if(ac_out == 0)
					ac_out_sec = time(NULL);
				ac_out = 1;	
				ac_con = 0xA0;  //高字节  -闭合继电器。	
								
			}
			else//断掉交流采集板控制的接触器		
			{
				ac_out = 0;//标记已关闭
				ac_con = 0x50; //没闭合过，断开。	

			}
	
			if(Globa->Control_DC_Infor_Data[0].Error.emergency_switch == 1)//急停了
			{
				ac_con = 0x50;//断开
				ac_out = 0;//标记已关闭

			}
			Jkac_con  = ac_con;
			//下面收到高字节为A关闭模块的交流接触器，收到高字节为5时，打开模块的交流接触器
			pdata->gun = ac_con | gun; //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
			pdata->charge_on = Globa->Sys_Tcu2CcuControl_Data[gun-1].model_charge_on;    //允许充电状态	BIN	1Byte	0-不允许 1-允许
			pdata->model_output_vol[0] = Globa->Sys_Tcu2CcuControl_Data[gun-1].model_output_vol;	//模块输出电压	BIN 2Byte	Mv
			pdata->model_output_vol[1] = Globa->Sys_Tcu2CcuControl_Data[gun-1].model_output_vol>>8;	//模块输出电压	BIN 2Byte	Mv  0.1V
			pdata->model_output_cur[0] = Globa->Sys_Tcu2CcuControl_Data[gun-1].model_output_cur;	//模块输出电流	BIN 2Byte	Ma    
			pdata->model_output_cur[1] = Globa->Sys_Tcu2CcuControl_Data[gun-1].model_output_cur>>8;	//模块输出电流	BIN 2Byte	Ma    0.1A
			pdata->Module_Readyok = Globa->Sys_Tcu2CcuControl_Data[gun-1].Module_Readyok; //模块准备ok
			pdata->WeaverRelay_control = Globa->Sys_Tcu2CcuControl_Data[gun-1].WeaverRelay_control;//并机继电器控制
			if(ac_con == 0xA0){
			  pdata->ACRelay_control  = 1;//AC继电器控制
			}else{
			  pdata->ACRelay_control  = 0;//AC继电器控制
			}
			
			pdata->keep_data = 0;


			if(Globa->Control_DC_Infor_Data[gun-1].gun_state == 0x03)
					gun_state = 0;//空闲未插枪      
			else if(Globa->Control_DC_Infor_Data[gun-1].gun_state == 0x07)
					gun_state = 1;//空闲已插枪
			else if(Globa->Control_DC_Infor_Data[gun-1].gun_state == 0x04)
					gun_state = 2;//充电中
			else if(Globa->Control_DC_Infor_Data[gun-1].gun_state == 0x05)
					gun_state = 3;//充电完成
			if(Globa->Control_DC_Infor_Data[gun-1].Error_system > 0)
					gun_state = 4;//枪故障
			pdata->gun_state = gun_state;
		
  		pdata->host_Relay_control_Output = Globa->Sys_Tcu2CcuControl_Data[gun-1].host_Relay_control_Output;
		
		 if(MC_DEBUG) printf("下发：-gun= %d -pdata->charge_on =%d -Module_Readyok=%0d pdata->WeaverRelay_control =%0d  model_output_vol =%d  model_output_cur =%d pdata->host_Relay_control_Output =%d \n",gun-1,pdata->charge_on,pdata->Module_Readyok,pdata->WeaverRelay_control,((pdata->model_output_vol[1]<<8)|pdata->model_output_vol[0]),((pdata->model_output_cur[1]<<8)|pdata->model_output_cur[0]),pdata->host_Relay_control_Output);

			if((model_output_vol[gun-1] != Globa->Sys_Tcu2CcuControl_Data[gun-1].model_output_vol)
				||(model_output_cur[gun-1] != Globa->Sys_Tcu2CcuControl_Data[gun-1].model_output_cur)
			  ||(host_Relay_control[gun-1] != Globa->Sys_Tcu2CcuControl_Data[gun-1].WeaverRelay_control)
				||(host_Relay_control_Output[gun-1] != Globa->Sys_Tcu2CcuControl_Data[gun-1].host_Relay_control_Output)
				||(chgenable[gun-1] != Globa->Sys_Tcu2CcuControl_Data[gun-1].model_charge_on)
				||((Output_voltage[gun-1]/1000) != (Globa->Control_DC_Infor_Data[gun-1].Output_voltage/1000))
				||((Output_current[gun-1]/1000) != (Globa->Control_DC_Infor_Data[gun-1].Output_current/1000))
				||(need_voltage[gun-1] != Globa->Sys_RevCcu_Data[gun-1].Need_Voltage)
				||(need_current[gun-1] != Globa->Sys_RevCcu_Data[gun-1].Need_Current)
			//	||((Output_kw[gun-1]/10) != (Globa->Control_DC_Infor_Data[gun-1].output_kw/10))
				||(chgstart[gun-1] != Globa->Sys_RevCcu_Data[gun-1].Modbule_On)
		    ||(Module_Readyok[gun-1] != Globa->Sys_Tcu2CcuControl_Data[gun-1].Module_Readyok)
				)
  		{
				model_output_vol[gun-1] = Globa->Sys_Tcu2CcuControl_Data[gun-1].model_output_vol; //0.1V
				model_output_cur[gun-1] = Globa->Sys_Tcu2CcuControl_Data[gun-1].model_output_cur; //0.1A
				host_Relay_control[gun-1] = Globa->Sys_Tcu2CcuControl_Data[gun-1].WeaverRelay_control;
	      host_Relay_control_Output[gun-1] = Globa->Sys_Tcu2CcuControl_Data[gun-1].host_Relay_control_Output;
				chgenable[gun-1] = Globa->Sys_Tcu2CcuControl_Data[gun-1].model_charge_on;
				
				Output_voltage[gun-1] = Globa->Control_DC_Infor_Data[gun-1].Output_voltage; //mV
				Output_current[gun-1] = Globa->Control_DC_Infor_Data[gun-1].Output_current; //mA
				need_voltage[gun-1] = Globa->Sys_RevCcu_Data[gun-1].Need_Voltage;    //0.1V
				need_current[gun-1] =  Globa->Sys_RevCcu_Data[gun-1].Need_Current;   //0.1A
				Output_kw[gun-1] = 	Globa->Control_DC_Infor_Data[gun-1].output_kw;
				chgstart[gun-1] = Globa->Sys_RevCcu_Data[gun-1].Modbule_On;
				Module_Readyok[gun-1] = Globa->Sys_Tcu2CcuControl_Data[gun-1].Module_Readyok;

				RUN_GUN_ControlLogOut(gun-1,"%d# 输出电压（0.1V）=%d V 输出电流（0.1A）=%d A 需求出电压（0.1V）=%d V 原始需求电流（0.1A）=%d  A（限功率后）需求电流（0.1A）=%d A 输出功率（0.1kw）=%d 功率限制（0.1%）=%d 模块组开关机(0-关机1-开机)= %d ",gun,Output_voltage[gun-1]/100,Output_current[gun-1]/100,need_voltage[gun-1],Globa->Control_DC_Infor_Data[gun-1].need_current/100,need_current[gun-1],Output_kw[gun-1],Globa->Control_DC_Infor_Data[gun-1].kw_percent,chgstart[gun-1]);
			  RUN_GUN_ControlLogOut(gun-1,"SET %d# 设置模块输出电压（0.1V)=%d V 设置模块输出电流（0.1A)=%d A  该枪控股的并机接触器（0-断开1-闭合）=%d  控制改组输出接触器（0/2-断开1-闭合）=%d 该模块组准备(0-未准备 1-准备oK)= %d 该模块组开关机(0-关机1-开机)= %d ",gun,model_output_vol[gun-1], model_output_cur[gun-1],host_Relay_control[gun-1],host_Relay_control_Output[gun-1],Module_Readyok[gun-1],chgenable[gun-1]);
			  RUN_GUN_ControlLogOut(gun-1,"-------------");
				if(Globa->Sys_RevCcu_Data[gun-1].Gunlink == 1 && Globa->Sys_RevCcu_Data[gun-1].Charge_State  == 1)
				{
					if((Output_current[gun-1] > 1.3*(Globa->Control_DC_Infor_Data[gun-1].need_current))&&(Globa->Control_DC_Infor_Data[gun-1].need_current > 10000))
					{
						RUN_GUN_ControlErrLogOut(gun-1,"%d# 输出电压（0.1V）=%d V 输出电流（0.1A）=%d A 需求出电压（0.1V）=%d V 原始需求电流-遥测（0.1A）=%d  A（另外上送的）需求电流（0.1A）=%d A 输出功率（0.1kw）=%d  模块组开关机(0-关机1-开机)= %d  模块通信正常数=%d 需求模块总数=%d ",gun,Output_voltage[gun-1]/100,Output_current[gun-1]/100,need_voltage[gun-1],Globa->Control_DC_Infor_Data[gun-1].need_current/100,need_current[gun-1],Output_kw[gun-1],chgstart[gun-1],Globa->Sys_RevCcu_Data[gun-1].Charger_good_num,ls_need_model_num_temp[gun-1]);
						RUN_GUN_ControlErrLogOut(gun-1,"SET %d# 设置模块输出电压（0.1V)=%d V 设置模块输出电流（0.1A)=%d A  该枪控股的并机接触器（0-断开1-闭合）=%d  控制改组输出接触器（0/2-断开1-闭合）=%d 该模块组开关机(0-关机1-开机)= %d ",gun,model_output_vol[gun-1], model_output_cur[gun-1],host_Relay_control[gun-1],host_Relay_control_Output[gun-1],chgenable[gun-1]);
						RUN_GUN_ControlErrLogOut(gun-1,"-------------end");
					}
				}
			}
			break;
		}
		
		case PGN256_ID_MC:{//充电启动帧
			frame.can_id = PGN256_ID_MC;
			frame.can_dlc = 8;

			MC_PGN256_FRAME * pdata = (MC_PGN256_FRAME *)frame.data;

			pdata->gun = gun;        //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
			//pdata->couple = 2;     //负荷控制开关	BIN	1Byte	根据用户类型提供不同功率输出。1启用，2关闭，其他无效。
			pdata->Gun_Elec_Way = 0X0F;

			if(Globa->Charger_param.Power_Contor_By_Mode == 1)
			{			
				pdata->couple = Globa->Control_DC_Infor_Data[gun-1].Double_Gun_To_Car_falg;//190731
			}
      else
		  { 	

				 if(Globa->Control_DC_Infor_Data[gun-1].Double_Gun_To_Car_falg == 1)
				 {
					 pdata->couple = 0xBB;  
					 pdata->Gun_Elec_Way = ((Globa->Control_DC_Infor_Data[gun-1].Double_Gun_To_Car_Othergun +1) << 4)|0X0F;
				 }else {
					 pdata->couple = 0;
				 }
      }
			
			if(CHG_BY_VIN == Globa->Control_DC_Infor_Data[gun-1].charger_start_way)
			{
				if(Globa->Charger_param.Power_Contor_By_Mode == 1)
				{
			     pdata->ch_type = flag; //充电启动方式	BIN	1Byte	1自动，2手动，其他无效。
				}else{		
				   pdata->ch_type = 3;
				}
			}else {
			  pdata->ch_type = flag; //充电启动方式	BIN	1Byte	1自动，2手动，其他无效。
			}
			
			data = Globa->Charger_param.set_voltage/100;
			pdata->set_voltage[0] = data; 
			pdata->set_voltage[1] = data>>8;
			data = Globa->Charger_param.set_current/100;
			pdata->set_current[0] = data;
			pdata->set_current[1] = data>>8;
			break;
		}
	
		case PGN768_ID_MC:{//充电停止帧
			frame.can_id = PGN768_ID_MC;
			frame.can_dlc = 8;

			MC_PGN768_FRAME * pdata = (MC_PGN768_FRAME *)frame.data;

			pdata->gun = gun; //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
			pdata->cause = 1; //停止充电原因	BIN	1Byte	0x01：计费控制单元正常停止 0x02: 计费控制单元故障终止

			break;
		}
		case PGN2304_ID_MC:{//下发充电参数信息
			frame.can_id = PGN2304_ID_MC;
			frame.can_dlc = 8;

			memset(&mc_data[0], 0x00, sizeof(mc_data)); //不用数据项填0,这个特殊，用于参数扩展使用，其他没有特殊说明镇1

			data = Globa->Charger_param.Charge_rate_voltage/1000;
			MC_All_Frame[Gun_No].MC_PGN2304_frame.Charge_rate_voltage[0] = data;
			MC_All_Frame[Gun_No].MC_PGN2304_frame.Charge_rate_voltage[1] = data>>8;

			MC_All_Frame[Gun_No].MC_PGN2304_frame.Charge_rate_number = Globa->Charger_param.Charge_rate_number[Gun_No];
			MC_All_Frame[Gun_No].MC_PGN2304_frame.model_rate_current = Globa->Charger_param.model_rate_current/1000;
			
			if(gun_config > 2)//3把枪以上
				MC_All_Frame[Gun_No].MC_PGN2304_frame.gun_config = 5;
			else if(gun_config == 2)
				MC_All_Frame[Gun_No].MC_PGN2304_frame.gun_config = 3;//		//0、1.单枪 2，轮流充电双枪，3-同时充电双枪 4--壁挂和便携式充电机 5---4枪或5枪机器
			else//==1
				MC_All_Frame[Gun_No].MC_PGN2304_frame.gun_config = 1;
				
			MC_All_Frame[Gun_No].MC_PGN2304_frame.AC_DAQ_POS = Globa->Charger_param.AC_DAQ_POS;
			MC_All_Frame[Gun_No].MC_PGN2304_frame.BMS_work = Globa->Charger_param.BMS_work;

			data = Globa->Charger_param.input_over_limit/1000;
			MC_All_Frame[Gun_No].MC_PGN2304_frame.input_over_limit[0] = data;
			MC_All_Frame[Gun_No].MC_PGN2304_frame.input_over_limit[1] = data>>8;

			data = Globa->Charger_param.input_lown_limit/1000;
			MC_All_Frame[Gun_No].MC_PGN2304_frame.input_lown_limit[0] = data;
			MC_All_Frame[Gun_No].MC_PGN2304_frame.input_lown_limit[1] = data>>8;

			data = Globa->Charger_param.output_over_limit/1000;
			MC_All_Frame[Gun_No].MC_PGN2304_frame.output_over_limit[0] = data;
			MC_All_Frame[Gun_No].MC_PGN2304_frame.output_over_limit[1] = data>>8;

			data = Globa->Charger_param.output_lown_limit/1000;
			MC_All_Frame[Gun_No].MC_PGN2304_frame.output_lown_limit[0] = data;
			MC_All_Frame[Gun_No].MC_PGN2304_frame.output_lown_limit[1] = data>>8;
			MC_All_Frame[Gun_No].MC_PGN2304_frame.BMS_CAN_ver = Globa->Charger_param.BMS_CAN_ver;
			MC_All_Frame[Gun_No].MC_PGN2304_frame.current_limit  = Globa->Charger_param.current_limit[Gun_No];
			MC_All_Frame[Gun_No].MC_PGN2304_frame.gun_allowed_temp = Globa->Charger_param.gun_allowed_temp;
			MC_All_Frame[Gun_No].MC_PGN2304_frame.Charging_gun_type = Globa->Charger_param.Charging_gun_type;
			MC_All_Frame[Gun_No].MC_PGN2304_frame.charge_modlue_factory  = Globa->Charger_param.charge_modlue_factory;
			if(WINLINE_MODULE == Globa->Charger_param.charge_modlue_factory )
				MC_All_Frame[Gun_No].MC_PGN2304_frame.charge_modlue_factory = INFY_MODULE;//永联模块按英飞源模块协议===test
			
			if(OTHER_MODULE == Globa->Charger_param.charge_modlue_factory )
				MC_All_Frame[Gun_No].MC_PGN2304_frame.charge_modlue_factory = 3;//EAST===test
			
			
			
			MC_All_Frame[Gun_No].MC_PGN2304_frame.Input_Relay_Ctl_Type  = Globa->Charger_param.Input_Relay_Ctl_Type;

			mc_data[0] = ((sizeof(MC_PGN2304_FRAME)+5) + 5)/6;
			mc_data[1] = sizeof(MC_PGN2304_FRAME);
			mc_data[2] = 0;
			memcpy(&mc_data[3], &MC_All_Frame[Gun_No].MC_PGN2304_frame, sizeof(MC_PGN2304_FRAME));

			sum = 0;
			for(i=0; i<(3+sizeof(MC_PGN2304_FRAME)); i++){
				sum +=  mc_data[i];
      }
			mc_data[i] = sum;
			i++;
			mc_data[i] = sum>>8;


			//发送多帧数据
			for(i=0; i<mc_data[0]; i++){
				frame.data[0] = gun;
				frame.data[1] = i+1;
				memcpy(&frame.data[2], &mc_data[i*6], 6);

				frame.can_id |= CAN_EFF_FLAG;//扩展帧的标识
				ret = write(fd, &frame, sizeof(frame));
				if(ret == -1){
					perror("can write error");
				}
				
				usleep(5000);//帧之间延时5ms
      }

			return(ret);//此处多帧连续发送，发完直接退出函数

			break;
		}
		case PGN3072_ID_MC:{ //下发直流分流器量程
			frame.can_id = PGN3072_ID_MC;
			frame.can_dlc = 8;
			MC_PGN3072_FRAME * pdata = (MC_PGN3072_FRAME *)frame.data;
			pdata->gun = gun;			
			pdata->DC_Shunt_Range[0] = Globa->Charger_param.DC_Shunt_Range[Gun_No]; 
			pdata->DC_Shunt_Range[1] = Globa->Charger_param.DC_Shunt_Range[Gun_No]>>8;
			break;
		}
		
		case PGN3840_ID_MC:{ //下发功率限制
			frame.can_id = PGN3840_ID_MC;
			frame.can_dlc = 8;
			Power_PGN3840_FRAME * pdata = (Power_PGN3840_FRAME *)frame.data;
			pdata->gun = gun;			
			
			pdata->Power_Adjust_Command = MC_All_Frame[Gun_No].Power_Adjust_Command;  //（目前就直接限定功率）功率调节指令类型:绝对值和百分比两种 01H：功率绝对值，输出值=功率调节参数值 02 百分比 输出值= 最大输出功率*百分比     
		
			pdata->Power_Adjust_data[0] = flag;
			pdata->Power_Adjust_data[1] = flag>>8;
			
			pdata->FF[0];   //保留字节，补足8字节
	    pdata->FF[1];   //保留字节，补足8字节
	    pdata->FF[2];   //保留字节，补足8字节
	    pdata->FF[3];   //保留字节，补足8字节
			printf("发送ID %0x  	pdata->gun =%d pdata->Power_Adjust_Command=%d \n", PGN,	pdata->gun,pdata->Power_Adjust_Command);
			break;
		}
		
		case PGN4608_ID_MC:{//充电启动完成应答报文
			frame.can_id = PGN4608_ID_MC;
			frame.can_dlc = 8;

			MC_PGN4608_FRAME * pdata = (MC_PGN4608_FRAME *)frame.data;

			pdata->gun = gun;    //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
			pdata->couple = 2;   //负荷控制开关	BIN	1Byte	根据用户类型提供不同功率输出。1启用，2关闭，其他无效。
			pdata->ok = 0;       //确认标识	BIN	1Byte	0成功；1失败

			break;
		}
		case PGN5120_ID_MC:{//停止充电完成应答报文
			frame.can_id = PGN5120_ID_MC;
			frame.can_dlc = 8;

			MC_PGN5120_FRAME * pdata = (MC_PGN5120_FRAME *)frame.data;

			pdata->gun = gun;    //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
			pdata->ok = 0;       //确认标识	BIN	1Byte	0成功；1失败

			break;
		}
		case PGN8960_ID_MC:{//遥测响应帧
			frame.can_id = PGN8960_ID_MC;
			frame.can_dlc = 8;

			MC_PGN8960_FRAME * pdata = (MC_PGN8960_FRAME *)frame.data;

			pdata->gun = gun;    //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
			pdata->ok = 0;       //确认标识	BIN	1Byte	0成功；1失败
			//修改为充电桩的状态
			if(Globa->Control_DC_Infor_Data[gun-1].gun_link == 0 && Globa->Control_DC_Infor_Data[gun-1].gun_state == 0x03)
					pdata->ok = 0;//空闲未插枪      
			else if(Globa->Control_DC_Infor_Data[gun-1].gun_state == 0x07)
					pdata->ok = 1;//空闲已插枪
			else if(Globa->Control_DC_Infor_Data[gun-1].gun_state == 0x04)
					pdata->ok = 2;//充电中
			else if(Globa->Control_DC_Infor_Data[gun-1].gun_state == 0x05)
					pdata->ok = 3;//充电完成
			else 
					pdata->ok = 0;//空闲未插枪
			if(Globa->Control_DC_Infor_Data[gun-1].Error_system > 0)
				pdata->ok = 4;//枪故障
			pdata->crc[0] = 0;   //
			pdata->crc[1] = 0;   //
      pdata->gun_state = pdata->ok;
	    pdata->JKError_system = (Globa->Control_DC_Infor_Data[gun-1].Error_system != 0)? 1:0; 

			break;
		}

		case PGN12544_ID_MC:{//心跳帧
			frame.can_id = PGN12544_ID_MC;
			frame.can_dlc = 8;

			MC_PGN12544_FRAME * pdata = (MC_PGN12544_FRAME *)frame.data;

			pdata->gun = gun;      //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
			pdata->mc_state = 0;   //计费控制单元状态信息	BIN	1Byte	0-正常 1-故障

			data = Globa->Control_DC_Infor_Data[Gun_No].kwh/100; //0.1kw
			pdata->kwh[0] = data; 
			pdata->kwh[1] = data>>8;
			data = Globa->Control_DC_Infor_Data[Gun_No].Time/60;
			pdata->time[0] = data;
			pdata->time[1] = data>>8;
			pdata->VIN_start = Globa->Control_DC_Infor_Data[Gun_No].VIN_start;//add lhb 180903
			break;
		}
/*
		case 1792:{//充电机发送时间同步信息报文
			tmp_can_frame.can_id = PGN1792_ID_GB;
			tmp_can_frame.can_dlc = sizeof(GB_BMS_PGN1792_FRAME);

			GB_BMS_PGN1792_FRAME * pgn_frame = (GB_BMS_PGN1792_FRAME *)tmp_can_frame.data;

  		systime = time(NULL);        //获得系统的当前时间 
  		localtime_r(&systime,&tm_t);  //结构指针指向当前时间	

			//BCD码 秒(1) 分 时 日 月 年(6 7)
			pgn_frame->date[0] = int_to_bcd_1byte(tm_t.tm_sec);
			pgn_frame->date[1] = int_to_bcd_1byte(tm_t.tm_min);
			pgn_frame->date[2] = int_to_bcd_1byte(tm_t.tm_hour);
			pgn_frame->date[3] = int_to_bcd_1byte(tm_t.tm_mday);
			pgn_frame->date[4] = int_to_bcd_1byte(tm_t.tm_mon+1);
			pgn_frame->date[5] = int_to_bcd_1byte((tm_t.tm_year+1900)%100);
			pgn_frame->date[6] = int_to_bcd_1byte((tm_t.tm_year+1900)/100);

			break;
		}
*/
		case PGN20992_ID_MC:{//升级请求报文
			frame.can_id = PGN20992_ID_MC;
			frame.can_dlc = 8;
			MC_PGN20992_FRAME * pdata = (MC_PGN20992_FRAME *)frame.data;
			pdata->gun = gun;    //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
			Globa->Control_Upgrade_Firmware_Info[Gun_No].Firmware_ALL_Bytes = Globa->Control_Upgrade_Firmware_Info[0].Firmware_ALL_Bytes;
			Globa->Control_Upgrade_Firmware_Info[Gun_No].Firware_CRC_Value = Globa->Control_Upgrade_Firmware_Info[0].Firware_CRC_Value;
			pdata->Firmware_Bytes[0] = Globa->Control_Upgrade_Firmware_Info[Gun_No].Firmware_ALL_Bytes;
			pdata->Firmware_Bytes[1] = Globa->Control_Upgrade_Firmware_Info[Gun_No].Firmware_ALL_Bytes>>8;
			pdata->Firmware_Bytes[2] = Globa->Control_Upgrade_Firmware_Info[Gun_No].Firmware_ALL_Bytes>>16;
			pdata->Firmware_CRC[0] = Globa->Control_Upgrade_Firmware_Info[Gun_No].Firware_CRC_Value;
			pdata->Firmware_CRC[1] = Globa->Control_Upgrade_Firmware_Info[Gun_No].Firware_CRC_Value>>8;
			pdata->Firmware_CRC[2] = Globa->Control_Upgrade_Firmware_Info[Gun_No].Firware_CRC_Value>>16;
			pdata->Firmware_CRC[3] = Globa->Control_Upgrade_Firmware_Info[Gun_No].Firware_CRC_Value>>24;
			
			break;
		}
		case PGN21760_ID_MC:{
			if(flag == 0){//表示第一次发送数据并打开文件
				if(mc_control_var[Gun_No].open_fp == NULL){
					mc_control_var[Gun_No].open_fp = fopen(F_UPDATE_EAST_DC_CTL, "rb");
					if(NULL == mc_control_var[Gun_No].open_fp){
						perror("open update bin file error \n");
						mc_control_var[Gun_No].open_fp = NULL;
						break;
					}
					if(fread(&mc_control_var[Gun_No].read_send_buf, sizeof(UINT8), 5, mc_control_var[Gun_No].open_fp) == 5)
					{
						mc_control_var[Gun_No].send_Tran_Packet_Number = 1;
						mc_control_var[Gun_No].SendTran_Packet = 1; //第一帧
						Globa->Control_Upgrade_Firmware_Info[Gun_No].Send_Tran_All_Bytes_0K = 5;
					}
				}
			}
		  if(Globa->Control_Upgrade_Firmware_Info[Gun_No].Send_Tran_All_Bytes_0K <= Globa->Control_Upgrade_Firmware_Info[Gun_No].Firmware_ALL_Bytes){
				frame.can_id = PGN21760_ID_MC;
				frame.can_dlc = 8;
				MC_PGN21760_FRAME * pdata = (MC_PGN21760_FRAME *)frame.data;
				pdata->gun = gun;
				pdata->Tran_Packet_Number[0] = mc_control_var[Gun_No].send_Tran_Packet_Number;
				pdata->Tran_Packet_Number[1] = mc_control_var[Gun_No].send_Tran_Packet_Number>>8;
				if((mc_control_var[Gun_No].SendTran_Packet + 1) == mc_control_var[Gun_No].send_Tran_Packet_Number)
				{
					mc_control_var[Gun_No].SendTran_Packet++;
					ret = fread(&mc_control_var[Gun_No].read_send_buf, sizeof(UINT8), 5, mc_control_var[Gun_No].open_fp);
					if(ret > 0){
						Globa->Control_Upgrade_Firmware_Info[Gun_No].Send_Tran_All_Bytes_0K = Globa->Control_Upgrade_Firmware_Info[Gun_No].Send_Tran_All_Bytes_0K + ret;
						if(ret != 5){
							for(i =ret;i<5;i++){ 
								mc_control_var[Gun_No].read_send_buf[i] = 0xFF;	
							}
						}
					}else{
						if(mc_control_var[Gun_No].open_fp != NULL){	
							fclose(mc_control_var[Gun_No].open_fp);
							mc_control_var[Gun_No].open_fp = NULL;
						}
						Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_success = 2;
				    Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_setStep = 0x10;
						return(-1);
					}
				}
				pdata->Tran_data[0] = mc_control_var[Gun_No].read_send_buf[0];
				pdata->Tran_data[1] = mc_control_var[Gun_No].read_send_buf[1];
				pdata->Tran_data[2] = mc_control_var[Gun_No].read_send_buf[2];
				pdata->Tran_data[3] = mc_control_var[Gun_No].read_send_buf[3];
				pdata->Tran_data[4] = mc_control_var[Gun_No].read_send_buf[4];
			}else
			{
				if(mc_control_var[Gun_No].open_fp != NULL)
				{	
					fclose(mc_control_var[Gun_No].open_fp);
					mc_control_var[Gun_No].open_fp = NULL;
				}
				Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_success = 2;
				Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_setStep = 0x10;
				return (-1);
			}
			break;  	
		}
		/*case PGN22272_ID_MC:{//
			frame.can_id = PGN22272_ID_MC;
			frame.can_dlc = 8;

			MC_PGN22272_FRAME * PGN22272_frame = (MC_PGN22272_FRAME *)frame.data;
			PGN22272_frame->gun = gun;        //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
			PGN22272_frame->Shileld_emergency_switch = (Globa->Charge_Shileld_Alarmflag.Shileld_emergency_switch == 1)? 1:0;//屏蔽急停按下故障
			PGN22272_frame->Shileld_PE_Fault = (Globa->Charge_Shileld_Alarmflag.Shileld_PE_Fault == 1)? 1:0;	//屏蔽接地故障告警	    
			PGN22272_frame->Shileld_AC_connect_lost = (Globa->Charge_Shileld_Alarmflag.Shileld_AC_connect_lost == 1)? 1:0;	//屏蔽交流输通讯告警	    
			PGN22272_frame->Shileld_AC_input_switch_off = (Globa->Charge_Shileld_Alarmflag.Shileld_AC_input_switch_off == 1)? 1:0;//屏蔽交流输入开关跳闸
			
			PGN22272_frame->Shileld_AC_fanglei_off = (Globa->Charge_Shileld_Alarmflag.Shileld_AC_fanglei_off == 1)? 1:0;    //屏蔽交流防雷器跳闸
			PGN22272_frame->Shileld_AC_output_switch_off_1 = 	(Globa->Charge_Shileld_Alarmflag.Shileld_AC_output_switch_off[0] == 1)? 1:0;//屏蔽直流输出开关跳闸1.2
			PGN22272_frame->Shileld_AC_output_switch_off_4 = (Globa->Charge_Shileld_Alarmflag.Shileld_AC_output_switch_off[1] == 1)? 1:0;//屏蔽直流输出开关跳闸1.2
			PGN22272_frame->Shileld_AC_input_over_limit = (Globa->Charge_Shileld_Alarmflag.Shileld_AC_input_over_limit == 1)? 1:0;//屏蔽交流输入过压保护 ----  data1
			
			PGN22272_frame->Shileld_AC_input_lown_limit = (Globa->Charge_Shileld_Alarmflag.Shileld_AC_input_lown_limit == 1)? 1:0; //屏蔽交流输入欠压
			PGN22272_frame->Shileld_GUN_LOCK_FAILURE = (Globa->Charge_Shileld_Alarmflag.Shileld_GUN_LOCK_FAILURE == 1)? 1:0;  //屏蔽枪锁检测未通过
			PGN22272_frame->Shileld_AUXILIARY_POWER = (Globa->Charge_Shileld_Alarmflag.Shileld_AUXILIARY_POWER == 1)? 1:0;   //屏蔽辅助电源检测未通过
			PGN22272_frame->Shileld_OUTSIDE_VOLTAGE = (Globa->Charge_Shileld_Alarmflag.Shileld_OUTSIDE_VOLTAGE == 1)? 1:0;    //屏蔽输出接触器外侧电压检测（标准10V，放开100V）
			
			PGN22272_frame->Shileld_SELF_CHECK_V = (Globa->Charge_Shileld_Alarmflag.Shileld_SELF_CHECK_V == 1)? 1:0;      //屏蔽自检输出电压异常
			PGN22272_frame->Shileld_INSULATION_FAULT = (Globa->Charge_Shileld_Alarmflag.Shileld_INSULATION_FAULT == 1)? 1:0;   //屏蔽绝缘故障
			PGN22272_frame->Shileld_BMS_WARN = (Globa->Charge_Shileld_Alarmflag.Shileld_BMS_WARN == 1)? 1:0;          //屏蔽 BMS告警终止充电
			PGN22272_frame->Shileld_CHARG_OUT_OVER_V = (Globa->Charge_Shileld_Alarmflag.Shileld_CHARG_OUT_OVER_V == 1)? 1:0;  //屏蔽输出过压停止充电   ----  data2
			
			PGN22272_frame->Shileld_OUT_OVER_BMSH_V = (Globa->Charge_Shileld_Alarmflag.Shileld_OUT_OVER_BMSH_V == 1)? 1:0;   //屏蔽输出电压超过BMS最高电压
			PGN22272_frame->Shileld_CHARG_OUT_OVER_C = (Globa->Charge_Shileld_Alarmflag.Shileld_CHARG_OUT_OVER_C == 1)? 1:0;  //屏蔽输出过流
			PGN22272_frame->Shileld_OUT_SHORT_CIRUIT = (Globa->Charge_Shileld_Alarmflag.Shileld_OUT_SHORT_CIRUIT == 1)? 1:0;  //屏蔽输出短路
			
			PGN22272_frame->Shileld_GUN_OVER_TEMP = (Globa->Charge_Shileld_Alarmflag.Shileld_GUN_OVER_TEMP == 1)? 1:0;     //屏蔽充电枪温度过高
			PGN22272_frame->Shileld_OUT_SWITCH = 	(Globa->Charge_Shileld_Alarmflag.Shileld_OUT_SWITCH  == 1)? 1:0;       //屏蔽输出开关
			PGN22272_frame->Shileld_OUT_NO_CUR = (Globa->Charge_Shileld_Alarmflag.Shileld_OUT_NO_CUR == 1)? 1:0;       //屏蔽输出无电流显示
			PGN22272_frame->Shileld_Link_Switch = (Globa->Charge_Shileld_Alarmflag.Shileld_Link_Switch == 1)? 1:0 ;      //屏蔽并机接触器     ----  data3

			break;
		}*/
		case PGN3584_ID_MC:{//枪电子锁解锁控制
			MC_PGN3584_FRAME * pdata = (MC_PGN3584_FRAME *)frame.data;
			frame.can_id = PGN3584_ID_MC;
			frame.can_dlc = 8;
			pdata->gun = gun;    //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
			pdata->Operation_command = flag;   //负荷控制开关	BIN	1Byte	根据用户类型提供不同功率输出。1启用，2关闭，其他无效。
			break;
		}
		case PGN25344_ID_MC:{//VIN反馈
			MC_PGN25344_FRAME * pdata = (MC_PGN25344_FRAME *)frame.data;
			frame.can_id = PGN25344_ID_MC;
			frame.can_dlc = 8;
			pdata->gun = gun;  
			pdata->VIN_Results = flag; //成功
			break;
		}
		default:{
			printf("发送ID不合法 %d\n", PGN);
			return(-1);

			break;
		}
	}
	//printf("发送ID合法 %d\n", PGN);

	frame.can_id |= CAN_EFF_FLAG;//扩展帧的标识

	ret = write(fd, &frame, sizeof(frame));
	if (ret == -1) {
		perror("can write error");
	}

	return(ret);
}


/*********************************************************
**descriGBion：MC_control
***parameter  :INT32 fd
***return		  :none
**********************************************************/
extern void MC_control(INT32 fd,int Gun_No)
{
	UINT32 sistent;
	UINT32 voltage;
	
	switch(Globa->Control_DC_Infor_Data[Gun_No].MC_Step)
	{
		case 0x01:
		{//----------------------0 就绪状态  0.1--------------------------

			memset(&MC_All_Frame[Gun_No], 0x00, sizeof(MC_ALL_FRAME));//把MC相关的数据和标志全部清零

			Globa->Control_DC_Infor_Data[Gun_No].MC_Step = 0x11;        //1 等待启动充电应答  1.1

			MC_timer_start(Gun_No,250);
			MC_timer_start(Gun_No,5000);

			MC_sent_data(fd, PGN256_ID_MC, Globa->Control_DC_Infor_Data[Gun_No].charger_start, Gun_No+1);  //充电启动帧
			sent_warning_message(0x94, 1, Gun_No+1, 0);
			mc_control_var[Gun_No].ctrl_cmd_lost_count = 0;
			SysControlLogOut("%d#枪计费下发启动充电",Gun_No+1);

			break;
		}
		case 0x11:
		{//----------------------1 等待启动充电应答  1.1------------------
			if(MC_All_Frame[Gun_No].MC_Recved_PGN512_Flag == 0x01){//启动应答帧
				MC_All_Frame[Gun_No].MC_Recved_PGN512_Flag = 0x00;
				Globa->Control_DC_Infor_Data[Gun_No].MC_Step = 0x21;   //充电中  2.1
				MC_timer_start(Gun_No,250);
				MC_timer_start(Gun_No,3000);
				MC_timer_stop(Gun_No,5000);
				mc_control_var[Gun_No].ctrl_cmd_lost_count = 0;
  			break;
			}

			if(MC_timer_flag(Gun_No,250) == 1)
			{  //定时250MS
				MC_sent_data(fd, PGN256_ID_MC, Globa->Control_DC_Infor_Data[Gun_No].charger_start, Gun_No+1);  //充电启动帧
				MC_timer_start(Gun_No,250);
			}

			if(MC_timer_flag(Gun_No,5000) == 1)
			{//超时5秒
				mc_control_var[Gun_No].ctrl_cmd_lost_count++;
				MC_timer_start(Gun_No,5000);
				if(mc_control_var[Gun_No].ctrl_cmd_lost_count >= 2)
				{
					Globa->Control_DC_Infor_Data[Gun_No].MC_Step = 0xE1;//控制器响应超时
					mc_control_var[Gun_No].ctrl_cmd_lost_count = 0;
				}
  			break;
			}

			break;
		}
		case 0x21:
		{//----------------------2  充电中  2.1---------------------------
			if(MC_All_Frame[Gun_No].MC_Recved_PGN12800_Flag == 0x01)
			{//心跳响应帧
				MC_All_Frame[Gun_No].MC_Recved_PGN12800_Flag = 0x00;
				MC_timer_start(Gun_No,3000);
			}

			if(MC_timer_flag(Gun_No,250) == 1){  //定时250MS
				MC_sent_data(fd, PGN12544_ID_MC, 1, Gun_No+1);  //心跳帧
				MC_timer_start(Gun_No,250);
			}
	    
			if(MC_All_Frame[Gun_No].MC_Recved_PGN25088_Flag == 0x01){//BMS信息反馈
				MC_All_Frame[Gun_No].MC_Recved_PGN25088_Flag = 0x00;
				MC_sent_data(fd, PGN25344_ID_MC,Globa->Control_DC_Infor_Data[Gun_No].VIN_Judgment_Result, Gun_No+1);  //心跳帧
			}
			
			if(MC_All_Frame[Gun_No].MC_Recved_PGN4352_Flag == 0x01){//启动完成帧
				MC_All_Frame[Gun_No].MC_Recved_PGN4352_Flag = 0x00;
				MC_sent_data(fd, PGN4608_ID_MC, 1, Gun_No+1);  //启动完成帧应答
			}
			
			if((Globa->Control_DC_Infor_Data[Gun_No].Manu_start == 0)||(Globa->Control_DC_Infor_Data[Gun_No].Error_system != 0))
			{//手动中止充电或系统故障中止充电
				MC_sent_data(fd, PGN768_ID_MC, 1, Gun_No+1);  //充电停止帧				
				if(Globa->Control_DC_Infor_Data[Gun_No].Error.ctrl_connect_lost == 1)
				{
					Globa->Control_DC_Infor_Data[Gun_No].MC_Step = 0xE1;//控制器响应超时
					MC_timer_stop(Gun_No,5000);
					mc_control_var[Gun_No].ctrl_cmd_lost_count = 0;
					break;
				}
				Globa->Control_DC_Infor_Data[Gun_No].MC_Step = 0x31;
				MC_timer_start(Gun_No,250);
				MC_timer_start(Gun_No,5000);
				MC_timer_stop(Gun_No,3000);
				mc_control_var[Gun_No].ctrl_cmd_lost_count = 0;
			  SysControlLogOut("%d#枪计费主动停止充电：Manu_start =%d Error_system=%d",Gun_No+1,Globa->Control_DC_Infor_Data[Gun_No].Manu_start,Globa->Control_DC_Infor_Data[Gun_No].Error_system);

				break;
			}

			if(MC_All_Frame[Gun_No].MC_Recved_PGN4864_Flag == 0x01)
			{//停止完成帧
				
				MC_All_Frame[Gun_No].MC_Recved_PGN4864_Flag = 0x00;
				Globa->Control_DC_Infor_Data[Gun_No].MC_Step = 0xE0;   //充电流程结束
				mc_control_var[Gun_No].ctrl_cmd_lost_count = 0;
				MC_sent_data(fd, PGN5120_ID_MC, 1, Gun_No+1);  //停止充电完成应答帧
				MC_sent_data(fd, PGN5120_ID_MC, 1, Gun_No+1);  //停止充电完成应答帧			
			  SysControlLogOut("%d#枪收到控制板主动停止充电：Manu_start =%d Error_system=%d",Gun_No+1,Globa->Control_DC_Infor_Data[Gun_No].Manu_start,Globa->Control_DC_Infor_Data[Gun_No].Error_system);
				
				break;
			}

			/*if(MC_timer_flag(Gun_No,3000) == 1){//超时3秒
				Globa->Control_DC_Infor_Data[Gun_No].MC_Step = 0xE1;//控制器响应超时
  			break;
			}*/

			break;
		}
		case 0x31:{//----------------------3  等待停止充电应答  3.1-----------------
			if(MC_All_Frame[Gun_No].MC_Recved_PGN1024_Flag == 0x01){//停止充电应答帧
				MC_All_Frame[Gun_No].MC_Recved_PGN1024_Flag = 0x00;	
				Globa->Control_DC_Infor_Data[Gun_No].MC_Step = 0x32;   //等待停止完成帧  3.2
				MC_timer_stop(Gun_No,250);
				MC_timer_stop(Gun_No,3000);
				MC_timer_start(Gun_No,5000);
				mc_control_var[Gun_No].ctrl_cmd_lost_count = 0;
  			break;
			}
						
			
			if(MC_timer_flag(Gun_No,250) == 1){  //定时250MS
				MC_sent_data(fd, PGN768_ID_MC, 1,Gun_No+1);  //充电停止帧
				MC_timer_start(Gun_No,250);
			}

			if(MC_timer_flag(Gun_No,5000) == 1){//超时5秒
			 	mc_control_var[Gun_No].ctrl_cmd_lost_count++;
				MC_timer_start(Gun_No,5000);
				if(	mc_control_var[Gun_No].ctrl_cmd_lost_count>= 2)
				{
					Globa->Control_DC_Infor_Data[Gun_No].MC_Step = 0xE1;//控制器响应超时
					mc_control_var[Gun_No].ctrl_cmd_lost_count = 0;
				}
  			break;
			}

			break;
		}
		case 0x32:{//----------------------3  等待停止完成帧  3.2-------------------
			if(MC_All_Frame[Gun_No].MC_Recved_PGN4864_Flag == 0x01){//停止完成帧
				MC_All_Frame[Gun_No].MC_Recved_PGN4864_Flag = 0x00;
				Globa->Control_DC_Infor_Data[Gun_No].MC_Step = 0xE0;   //充电流程结束

				MC_sent_data(fd, PGN5120_ID_MC, 1, Gun_No+1);  //停止充电完成应答帧
				MC_sent_data(fd, PGN5120_ID_MC, 1, Gun_No+1);  //停止充电完成应答帧

  			break;
			}

			if(MC_timer_flag(Gun_No,5000) == 1)
			{//超时5秒
			 	mc_control_var[Gun_No].ctrl_cmd_lost_count++;
				MC_timer_start(Gun_No,5000);
				if(mc_control_var[Gun_No].ctrl_cmd_lost_count >= 2)
				{
					Globa->Control_DC_Infor_Data[Gun_No].MC_Step = 0xE1;//控制器响应超时
					mc_control_var[Gun_No].ctrl_cmd_lost_count = 0;
				}
				break;
			}

			break;
		}
		
		case 0xE0:{//---------------------------E0 充电流程正常结束-----------------
			SysControlLogOut("%d#枪收到控制板发送的结束原因：0%x",Gun_No+1,Globa->Control_DC_Infor_Data[Gun_No].ctl_over_flag);
			switch(Globa->Control_DC_Infor_Data[Gun_No].ctl_over_flag & 0xFF00){//判断充电结束原因
				case 0xE100:{//计费控制单元控制停止
					if(Globa->Control_DC_Infor_Data[Gun_No].Manu_start == 0){//计费控制单元正常停止(手动终止或达到设定条件如时间、电量、金额、余额不足)
						if(Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason == 2){//远程APP/服务器下发停止
							sent_warning_message(0x94, 9, Gun_No+1, 0);
						}else{
						  sent_warning_message(0x94, 14, Gun_No+1, 0);
						}
						Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 12;
					}else
					{//计费控制单元故障终止
						sent_warning_message(0x94, 40, Gun_No+1, 0);
						Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 40;
					}

					break;
				}
				case 0xE200:{//控制器判定充电机故障，终止充电
					switch(Globa->Control_DC_Infor_Data[Gun_No].ctl_over_flag){
    				case 0xE201:{//车载BMS未响应
    					sent_warning_message(0x94, 21, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 21;
						Globa->Control_DC_Infor_Data[Gun_No].bms_com_failed = 1;
    					break;
    				}
    				case 0xE202:{//车载BMS响应超时
    					sent_warning_message(0x94, 22, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 22;
						Globa->Control_DC_Infor_Data[Gun_No].bms_com_failed = 1;
    					break;
    				}
    				case 0xE203:{//电池反接
    					sent_warning_message(0x94, 23, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 23;
    					break;
    				}
    				case 0xE204:{//未检测到电池电压
    					sent_warning_message(0x94, 24, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 24;
    					break;
    				}
    				case 0xE205:{//BMS未准备就绪
    					sent_warning_message(0x94, 25, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 25;
						Globa->Control_DC_Infor_Data[Gun_No].bms_com_failed = 1;
    					break;
    				}
    				case 0xE206:{//充电枪连接断开
    					sent_warning_message(0x94, 26, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 26;
    					break;
    				}
    				case 0xE207:{//枪锁异常
    					sent_warning_message(0x94, 27, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 27;
    					break;
    				}
    				case 0xE208:{//自检输出电压异常
    					sent_warning_message(0x94, 28, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 28;
    					break;
    				}
    				case 0xE209:{//自检泄放电压异常----自检前检测到外侧电压异常
    					sent_warning_message(0x94, 29, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 29;
    					break;
    				}
    				case 0xE20A:{//辅助电源异常
    					sent_warning_message(0x94, 30, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 30;
    					break;
    				}
    				case 0xE20B:{//充电电压过压告警
    					sent_warning_message(0x94, 31, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 31;
    					break;
    				}
    				case 0xE20C:{//充电电流过流告警
    					sent_warning_message(0x94, 32, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 32;
						Globa->Control_DC_Infor_Data[Gun_No].output_overload_alarm = 1;
    					break;
    				}
    				case 0xE20D:{//输出短路故障
    					sent_warning_message(0x94, 33, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 33;
						Globa->Control_DC_Infor_Data[Gun_No].Error.output_shout = 1;
    					break;
    				}
    				case 0xE20E:{//系统输出过压保护
    					sent_warning_message(0x94, 34, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 34;
    					break;
    				}
    				case 0xE20F:{//系统绝缘故障
    					sent_warning_message(0x94, 35, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 35;
    					break;
    				}
    				case 0xE210:{//输出接触器异常
    					sent_warning_message(0x94, 36, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 36;
    					break;
    				}
    				case 0xE211:{//辅助电源接触器异常
    					sent_warning_message(0x94, 37, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 37;
    					break;
    				}
    				case 0xE212:{//系统并机接触器异常
    					sent_warning_message(0x94, 38, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 38;
    					break;
    				}
						case 0xE213:{//充电枪温度过高
    					sent_warning_message(0x94, 39, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 39;
						Globa->Control_DC_Infor_Data[Gun_No].plug_temp_high_fault = 1;
    					break;
    				}
						case 0xE214:{//由于充电过程中电流为0导致的结束原因，更改为正常结束
    					sent_warning_message(0x94, 10, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 10;
    					break;
    				}
						case 0xE215:{//超出了系统输出电压能力
    					sent_warning_message(0x94, 17, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 17;
    					break;
    				}
						
						case 0xE216:{//检测电池电压误差比较大
    					sent_warning_message(0x94, 18, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 18;
    					break;
    				}
						case 0xE217:{//预充阶段继电器两端电压相差大于10V
    					sent_warning_message(0x94, 20,Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 20;
    					break;
    				}
						case 0xE218:{//系统判定BMS单体持续过压
    					sent_warning_message(0x94, 64,Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 64;
    					break;
    				}
						case 0xE219:{//检测到VIN不匹配
    					sent_warning_message(0x94, 75, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 75;
    					break;
    				}
						case 0xE21A:{//充电模块未准备
    					sent_warning_message(0x94, 76,Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 76;
    					break;
    				}
						case 0xE21B:{//检测到BCL数据异常（需求电压，电流 会大于最高电压，电流）
    					sent_warning_message(0x94, 77, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 77;
    					break;
    				}
    				case 0xE2FF:{//系统其它故障，请查询告警记录
    					sent_warning_message(0x94, 40, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 40;
    					break;
    				}
    				default:{//未定义原因
    					sent_warning_message(0x94, Globa->Control_DC_Infor_Data[Gun_No].ctl_over_flag+20, Gun_No+1, 0);//加20因为报告给QT显示的时候从20开始偏移
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = Globa->Control_DC_Infor_Data[Gun_No].ctl_over_flag+20;
    					break;
    				}
    			}
    			
    			break;
    		}
				case 0xE300:{//充电机判定BMS异常，终止充电
					switch(Globa->Control_DC_Infor_Data[Gun_No].ctl_over_flag){
    				case 0xE301:{//单体电压异常
    					sent_warning_message(0x94, 56, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 56;
    					break;
    				}
    				case 0xE302:{//SOC异常
    					sent_warning_message(0x94, 57, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 57;
    					break;
    				}
    				case 0xE304:{//过温告警
    					sent_warning_message(0x94, 58, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 58;
    					break;
    				}
    				case 0xE303:{//过流告警
    					sent_warning_message(0x94, 59, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 59;
    					break;
    				}
    				case 0xE305:{//绝缘异常
    					sent_warning_message(0x94, 60, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 60;
    					break;
    				}
    				case 0xE306:{//输出连接器异常
    					sent_warning_message(0x94, 61, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 61;
    					break;
    				}
    				default:{//未定义原因
    					sent_warning_message(0x94, Globa->Control_DC_Infor_Data[Gun_No].ctl_over_flag+55, Gun_No+1, 0);//加55因为报告给QT显示的时候从55开始偏移
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = Globa->Control_DC_Infor_Data[Gun_No].ctl_over_flag+55;
    					break;
    				}
    			}
    			
    			break;
    		}
				case 0xE400:{//BMS正常终止充电
					switch(Globa->Control_DC_Infor_Data[Gun_No].ctl_over_flag){
    				case 0xE401:{//达到所需的SOC
    					sent_warning_message(0x94, 41, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 41;
    					break;
    				}
    				case 0xE402:{//达到总电压的设定值
    					sent_warning_message(0x94, 42, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 42;
    					break;
    				}
    				case 0xE403:{//达到单体电压的设定值
    					sent_warning_message(0x94, 43, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 43;
    					break;
    				}

    				default:{//未定义原因
    					sent_warning_message(0x94, Globa->Control_DC_Infor_Data[Gun_No].ctl_over_flag+40, 2, 0);//加40因为报告给QT显示的时候从40开始偏移
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = Globa->Control_DC_Infor_Data[Gun_No].ctl_over_flag+40;
    					break;
    				}
    			}
    			
    			break;
    		}
				case 0xE500:{//控制器判定充电机故障，终止充电
					switch(Globa->Control_DC_Infor_Data[Gun_No].ctl_over_flag){
    				case 0xE501:{//绝缘故障
    					sent_warning_message(0x94, 44, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 44;
    					break;
    				}
    				case 0xE502:{//输出连接器过温故障
    					sent_warning_message(0x94, 45, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 45;
    					break;
    				}
    				case 0xE503:{//BMS原件过温故障
    					sent_warning_message(0x94, 46,Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 46;
    					break;
    				}
    				case 0xE504:{//充电连接器故障
    					sent_warning_message(0x94, 47, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 47;
    					break;
    				}
    				case 0xE505:{//电池组温度过高故障
    					sent_warning_message(0x94, 48, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 48;
						Globa->Control_DC_Infor_Data[Gun_No].bat_temp_high_alarm = 1;
    					break;
    				}
    				case 0xE506:{//高压继电器故障
    					sent_warning_message(0x94, 49, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 49;
    					break;
    				}
    				case 0xE507:{//检测点2电压检测故障
    					sent_warning_message(0x94, 50, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 50;
    					break;
    				}
    				case 0xE508:{//其他故障
    					sent_warning_message(0x94, 51, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 51;
    					break;
    				}
    				case 0xE509:{//充电电流过大
    					sent_warning_message(0x94, 52, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 52;
    					break;
    				}
    				case 0xE50A:{//充电电压异常
    					sent_warning_message(0x94, 53, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 53;
    					break;
    				}
    				case 0xE50B:{//BMS 未告知
    					sent_warning_message(0x94, 54, Gun_No+1, 0);
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 54;
    					break;
    				}
    				default:{//未定义原因
    					sent_warning_message(0x94, Globa->Control_DC_Infor_Data[Gun_No].ctl_over_flag+43, 2, 0);//加43因为报告给QT显示的时候从43开始偏移
    					Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = Globa->Control_DC_Infor_Data[Gun_No].ctl_over_flag+43;
    					break;
    				}
    			}
    			
    			break;
    		}
    		default:{//未定义原因
    			sent_warning_message(0x94, Globa->Control_DC_Infor_Data[Gun_No].ctl_over_flag>>8, Gun_No+1, 0);//右移8位 让QT显示出非E1之E5的结束原因
    			Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = Globa->Control_DC_Infor_Data[Gun_No].ctl_over_flag>>8;
    			break;
    		}
			}
			Globa->Control_DC_Infor_Data[Gun_No].SYS_Step = 0x21;    //系统充电结束状态处理
			break;
		}
		case 0xE1:{//---------------------------E1 控制器响应超时-------------------
    	/*if(Globa->Error.ctrl_connect_lost != 1){
    		Globa->Error.ctrl_connect_lost = 1;
    		sent_warning_message(0x99, 53, Gun_No+1, 0);
    		Globa->Error_system++;
    	}*///此处没有成对出现告警消失，其消失放在遥测数据的地方判断
			
			sent_warning_message(0x94, 16, Gun_No+1, 0);//充电控制板响应超时
			Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 16;
			Globa->Control_DC_Infor_Data[Gun_No].SYS_Step = 0x21;    //系统充电结束状态处理
			mc_control_var[Gun_No].ctrl_cmd_lost_count = 0;
			SysControlLogOut("%d#枪控制板响应超时",Gun_No+1);
			break;
		}
		default:{
			printf("系统进入未知状态 Globa->Control_DC_Infor_Data[%d].MC_Step =%d\n", Gun_No+1,Globa->Control_DC_Infor_Data[Gun_No].MC_Step);
			while(1);//等待系统复位未知

			break;
		}
	}
}

