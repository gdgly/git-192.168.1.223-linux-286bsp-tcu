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

#include "globalvar.h"
#include "MC_can.h"


static MC_VAR MC_var;
static UINT8 ml_data[255];   //多帧数据的暂存缓存区
MC_ALL_FRAME MC_All_Frame_2;
static FILE *open_fp2 = NULL;
static char read_send_buf[5] = {0};
extern UINT32 send_Tran_Packet_Number ,SendTran_Packet;
int ctr2_cmd_lost_count = 0;
static UINT32 pre_outvolt,pre_out_current,pre_need_volt,pre_need_current;
UINT32 TEMP_OFSET_2 = 0;

/*******************************************************************************
**description：MC读线程
***parameter  :int fd
***return		  :none
*******************************************************************************/
extern void MC_read_deal_2(struct can_frame frame)
{
	int i;
	UINT32 sum;

	switch(frame.can_id&CAN_EFF_MASK){//判断接收帧ID
		case PGN512_ID_CM:{   //启动充电响应帧
			MC_PGN512_FRAME * pdata = (MC_PGN512_FRAME *)frame.data;

			MC_All_Frame_2.MC_Recved_PGN512_Flag = 1;

			break;
		}
		case PGN1024_ID_CM:{  //停止充电响应帧
			MC_PGN1024_FRAME * pdata = (MC_PGN1024_FRAME *)frame.data;

			MC_All_Frame_2.MC_Recved_PGN1024_Flag = 1;

			break;
		}
		case PGN2560_ID_CM:{  //设置充电参数信息响应帧
			MC_PGN2560_FRAME * pdata = (MC_PGN2560_FRAME *)frame.data;

			MC_All_Frame_2.MC_Recved_PGN2560_Flag = 1;

			break;
		}
		case PGN2816_ID_CM:{  //请求充电参数信息
			MC_PGN2816_FRAME * pdata = (MC_PGN2816_FRAME *)frame.data;

			memcpy(&Globa_2->contro_ver[0], &pdata->Cvel[0], sizeof(pdata->Cvel));
			MC_All_Frame_2.MC_Recved_PGN2816_Flag = 1;
      char*contro_ver = strstr(Globa_2->contro_ver,".");
			contro_ver++;
      float contro_ver_value = atof (contro_ver);
      int contro_ver_value1= contro_ver_value*100;
		  if(contro_ver_value1 >= 220){
				TEMP_OFSET_2 = 0;
			  if(contro_ver_value1 >= 224){
					Globa_2->VIN_Support_falg = 1;
				}else{
					Globa_2->VIN_Support_falg = 0;
				}
			}else{
				TEMP_OFSET_2 = 50;
				Globa_2->VIN_Support_falg = 0;
			}
			break;
		}
		case PGN3328_ID_CM:{ //直流分流器量程确认帧
			MC_PGN3328_FRAME * pdata = (MC_PGN3328_FRAME *)frame.data;
			MC_All_Frame_2.MC_Recved_PGN3328_Flag = 1;
			break;
		}
		case PGN4352_ID_CM:{  //启动完成帧
			MC_PGN4352_FRAME * pdata = (MC_PGN4352_FRAME *)frame.data;

		  Globa_2->BATT_type = pdata->BATT_type;
		  Globa_2->BMS_H_voltage = ((pdata->BMS_H_vol[1]<<8) + pdata->BMS_H_vol[0])*100;
		  Globa_2->BMS_H_current = ((pdata->BMS_H_cur[1]<<8) + pdata->BMS_H_cur[0])*10;

			MC_All_Frame_2.MC_Recved_PGN4352_Flag = 1;

			break;
		}
		case PGN4864_ID_CM:{  //停止完成帧
			MC_PGN4864_FRAME * pdata = (MC_PGN4864_FRAME *)frame.data;

			Globa_2->ctl_over_flag = pdata->cause[0] + (pdata->cause[1]<<8);

			MC_All_Frame_2.MC_Recved_PGN4864_Flag = 1;

			break;
		}
		case PGN12800_ID_CM:{ //心跳响应帧
			MC_All_Frame_2.MC_Recved_PGN12800_Flag = 1;

			break;
		}
		case PGN8448_ID_CM:{   //遥信数据帧
			MC_PGN8448_FRAME * pdata = (MC_PGN8448_FRAME *)frame.data;
			
			MC_alarm_deal_2(pdata->gun, pdata->state, pdata->alarm_sn, pdata->fault, pdata->num[0]+(pdata->num[1]<<8));

			MC_All_Frame_2.MC_Recved_PGN8448_Flag = 1;

			break;
		}
		case PGN8704_ID_CM:{   //遥测数据帧
			if((frame.data[1] >= 1)&&(frame.data[1] <= ((sizeof(MC_PGN8704_FRAME)+5) + 5)/6)){
				if(frame.data[1] == 1){//第一个数据包
					MC_var.sequence = frame.data[1];
					MC_var.Total_Num_Packet = frame.data[2];
					MC_var.Total_Mess_Size = frame.data[3] + (frame.data[4]<<8);

					if((MC_var.Total_Num_Packet == ((sizeof(MC_PGN8704_FRAME)+5) + 5)/6)&&(MC_var.Total_Mess_Size == sizeof(MC_PGN8704_FRAME))){
						memcpy(&ml_data[0], &frame.data[2], 6);
						MC_var.sequence++;
					}
				}else if(frame.data[1] == ((sizeof(MC_PGN8704_FRAME)+5) + 5)/6){//最后一个数据包
					if(frame.data[1] == MC_var.sequence){
						memcpy(&ml_data[(MC_var.sequence-1)*6], &frame.data[2], 6);

						sum = 0;
						for(i=0; i<(3+sizeof(MC_PGN8704_FRAME)); i++){
							sum +=  ml_data[i];
						}

						if(((sum&0xff) ==  ml_data[i])&&((sum&0xff00) == (ml_data[i+1]<<8))){
							MC_All_Frame_2.MC_Recved_PGN8704_Flag = 1;
							MC_var.sequence = 0;
		  				memcpy(&MC_All_Frame_2.MC_PGN8704_frame, &ml_data[3], sizeof(MC_PGN8704_FRAME));
		  				
							Globa_2->ctl_state =   MC_All_Frame_2.MC_PGN8704_frame.state;

		  				Globa_2->Output_voltage = ((MC_All_Frame_2.MC_PGN8704_frame.Output_voltage[1]<<8) + MC_All_Frame_2.MC_PGN8704_frame.Output_voltage[0])*100;
		  				Globa_2->Output_current = ((MC_All_Frame_2.MC_PGN8704_frame.Output_current[1]<<8) + MC_All_Frame_2.MC_PGN8704_frame.Output_current[0])*10;
		  				if((Globa_1->Charger_param.System_Type == 4)&&(Globa_1->Double_Gun_To_Car_falg == 1)){
								Globa_2->BMS_batt_SOC = Globa_1->BMS_batt_SOC;//MC_All_Frame_2.MC_PGN8704_frame.SOC;
								Globa_2->BMS_cell_HV = Globa_1->BMS_cell_HV;//(MC_All_Frame_2.MC_PGN8704_frame.BMS_cell_HV[1]<<8) + MC_All_Frame_2.MC_PGN8704_frame.BMS_cell_HV[0];
								Globa_2->BMS_cell_HV_NO = Globa_1->BMS_cell_HV_NO;//MC_All_Frame_2.MC_PGN8704_frame.BMS_cell_HV_NO;  //8最高单体电压序号
								Globa_2->Charge_Mode = Globa_1->Charge_Mode;//MC_All_Frame_2.MC_PGN8704_frame.Charge_Mode;        //8充电模式
								Globa_2->BMS_cell_HT = Globa_1->BMS_cell_HT + TEMP_OFSET_2 ;//MC_All_Frame_2.MC_PGN8704_frame.BMS_cell_HT;
								Globa_2->BMS_cell_HT_NO = Globa_1->BMS_cell_HT_NO;//MC_All_Frame_2.MC_PGN8704_frame.BMS_cell_HT_NO;
								Globa_2->BMS_cell_LT = Globa_1->BMS_cell_LT + TEMP_OFSET_2;//MC_All_Frame_2.MC_PGN8704_frame.BMS_cell_LT;
								Globa_2->BMS_cell_LT_NO = Globa_1->BMS_cell_LT_NO;//MC_All_Frame_2.MC_PGN8704_frame.BMS_cell_LT_NO;
								Globa_2->Charge_Gun_Temp = MC_All_Frame_2.MC_PGN8704_frame.TEMP; //充电枪温度过高

								Globa_2->line_v = MC_All_Frame_2.MC_PGN8704_frame.line_v;
								Globa_2->BMS_need_time  = Globa_1->BMS_need_time;//(MC_All_Frame_2.MC_PGN8704_frame.BMS_need_time[1]<<8) + MC_All_Frame_2.MC_PGN8704_frame.BMS_need_time[0];
								Globa_2->need_voltage = Globa_1->need_voltage;//((MC_All_Frame_2.MC_PGN8704_frame.need_voltage[1]<<8) + MC_All_Frame_2.MC_PGN8704_frame.need_voltage[0])*100;
								Globa_2->need_current = Globa_1->need_current;//((MC_All_Frame_2.MC_PGN8704_frame.need_current[1]<<8) + MC_All_Frame_2.MC_PGN8704_frame.need_current[0])*10;
								if( (pre_outvolt != Globa_2->Output_voltage/1000)||(pre_out_current != Globa_2->Output_current/1000)||
							    ( pre_need_volt !=  Globa_2->need_voltage/1000) || ( pre_need_current !=   Globa_2->need_current/1000))
								{
									pre_outvolt = Globa_2->Output_voltage/1000;
									pre_need_volt =  Globa_2->need_voltage/1000;
									pre_need_current =  Globa_2->need_current/1000;
									pre_out_current = Globa_2->Output_current/1000;
									RunEventLogOut_2("双枪对一辆车模式:需求电压=%dV,需求电流=%dA,输出电压=%dV,输出电流=%dA,SOC=%d,枪温%d℃,单体最高电压(0.01)%dV Charge_Mode = %d",				
													pre_need_volt,pre_need_current,pre_outvolt,pre_out_current,
													Globa_2->BMS_batt_SOC,Globa_2->Charge_Gun_Temp-50,Globa_2->BMS_cell_HV,Globa_2->Charge_Mode);
								}
							}else {
								Globa_2->BMS_batt_SOC =   MC_All_Frame_2.MC_PGN8704_frame.SOC;
								Globa_2->BMS_cell_HV = (MC_All_Frame_2.MC_PGN8704_frame.BMS_cell_HV[1]<<8) + MC_All_Frame_2.MC_PGN8704_frame.BMS_cell_HV[0];
								Globa_2->BMS_cell_HV_NO = MC_All_Frame_2.MC_PGN8704_frame.BMS_cell_HV_NO;  //8最高单体电压序号
								Globa_2->Charge_Mode = MC_All_Frame_2.MC_PGN8704_frame.Charge_Mode;        //8充电模式
								Globa_2->BMS_cell_HT = MC_All_Frame_2.MC_PGN8704_frame.BMS_cell_HT + TEMP_OFSET_2;
								Globa_2->BMS_cell_HT_NO = MC_All_Frame_2.MC_PGN8704_frame.BMS_cell_HT_NO;
								Globa_2->BMS_cell_LT = MC_All_Frame_2.MC_PGN8704_frame.BMS_cell_LT + TEMP_OFSET_2;
								Globa_2->BMS_cell_LT_NO = MC_All_Frame_2.MC_PGN8704_frame.BMS_cell_LT_NO;
								Globa_2->Charge_Gun_Temp = MC_All_Frame_2.MC_PGN8704_frame.TEMP; //充电枪温度过高

								Globa_2->line_v = MC_All_Frame_2.MC_PGN8704_frame.line_v;
								Globa_2->BMS_need_time  = (MC_All_Frame_2.MC_PGN8704_frame.BMS_need_time[1]<<8) + MC_All_Frame_2.MC_PGN8704_frame.BMS_need_time[0];
								Globa_2->need_voltage = ((MC_All_Frame_2.MC_PGN8704_frame.need_voltage[1]<<8) + MC_All_Frame_2.MC_PGN8704_frame.need_voltage[0])*100;
								Globa_2->need_current = ((MC_All_Frame_2.MC_PGN8704_frame.need_current[1]<<8) + MC_All_Frame_2.MC_PGN8704_frame.need_current[0])*10;
						  	if( (pre_outvolt != Globa_2->Output_voltage/1000)||(pre_out_current != Globa_2->Output_current/1000)||
							    ( pre_need_volt !=  Globa_2->need_voltage/1000) || ( pre_need_current !=   Globa_2->need_current/1000))
								{
									pre_outvolt = Globa_2->Output_voltage/1000;
									pre_need_volt =  Globa_2->need_voltage/1000;
									pre_need_current =  Globa_2->need_current/1000;
									pre_out_current = Globa_2->Output_current/1000;
									RunEventLogOut_2("需求电压=%dV,需求电流=%dA,输出电压=%dV,输出电流=%dA,SOC=%d,枪温%d℃,单体最高电压(0.01)%dV Charge_Mode = %d",				
													pre_need_volt,pre_need_current,pre_outvolt,pre_out_current,
													Globa_2->BMS_batt_SOC,Globa_2->Charge_Gun_Temp-50,Globa_2->BMS_cell_HV,Globa_2->Charge_Mode);
								}
							}
						}
					}
				}else{//中间帧
					if(frame.data[1] == MC_var.sequence){
						memcpy(&ml_data[(MC_var.sequence-1)*6], &frame.data[2], 6);
						MC_var.sequence++;
					}
				}
			}
			break;
		}
		case PGN9216_ID_CM:{   //双枪同时充的机器只配置一个交流采集盒 接在1号控制板上
			if((frame.data[1] >= 1)&&(frame.data[1] <= ((sizeof(AC_MEASURE_DATA)+5) + 5)/6)){
				if(frame.data[1] == 1){//第一个数据包
					MC_var.sequence = frame.data[1];
					MC_var.Total_Num_Packet = frame.data[2];
					MC_var.Total_Mess_Size = frame.data[3] + (frame.data[4]<<8);

					if((MC_var.Total_Num_Packet == ((sizeof(AC_MEASURE_DATA)+5) + 5)/6)&&(MC_var.Total_Mess_Size == sizeof(AC_MEASURE_DATA))){
						memcpy(&ml_data[0], &frame.data[2], 6);
						MC_var.sequence++;
					}
				}else if(frame.data[1] == ((sizeof(AC_MEASURE_DATA)+5) + 5)/6){//最后一个数据包
					if(frame.data[1] == MC_var.sequence){
						memcpy(&ml_data[(MC_var.sequence-1)*6], &frame.data[2], 6);

						sum = 0;
						for(i=0; i<(3+sizeof(AC_MEASURE_DATA)); i++){
							sum +=  ml_data[i];
						}

						if(((sum&0xff) ==  ml_data[i])&&((sum&0xff00) == (ml_data[i+1]<<8))){
							MC_All_Frame_2.MC_Recved_PGN9216_Flag = 1;
							MC_var.sequence = 0;

		  				memcpy(&AC_measure_data.reserve1, &ml_data[3], sizeof(AC_MEASURE_DATA));
						}
					}
				}else{//中间帧
					if(frame.data[1] == MC_var.sequence){
						memcpy(&ml_data[(MC_var.sequence-1)*6], &frame.data[2], 6);
						MC_var.sequence++;
					}
				}
			}
			break;
		}
		case PGN9472_ID_CM:{   //遥信充电机状态帧
			MC_PGN9472_FRAME * pdata = (MC_PGN9472_FRAME *)frame.data;
			
			MC_state_deal_2(pdata->gun, pdata->state);

			MC_All_Frame_2.MC_Recved_PGN8448_Flag = 1;

			break;
		}
  	case PGN21248_ID_CM:{////升级请求报文响应报文(PF:0x53) 控制器--计费单元
			MC_PGN21248_FRAME * pdata = (MC_PGN21248_FRAME *)frame.data;
      if(pdata->Success_flag == 0){
				MC_All_Frame_2.MC_Recved_PGN21248_Flag = 1;
			}else{
				MC_All_Frame_2.MC_Recved_PGN21248_Flag = 0;
			}
			break;
		}
		case PGN21504_ID_CM:{//请求升级数据报文帧(PF:0x54)  控制器--计费单元
			MC_All_Frame_2.MC_Recved_PGN21504_Flag = 1;

			break;
		}
		case PGN22016_ID_CM:{
		  MC_PGN22016_FRAME * pdata = (MC_PGN22016_FRAME *)frame.data;
			if(pdata->Success_flag == 0){//成功发送下一帧报文
        send_Tran_Packet_Number = ((pdata->Tran_Packet_Number[1]<<8)|pdata->Tran_Packet_Number[0]) + 1;
			  if((Globa_2->Control_Upgrade_Firmware_Info.Send_Tran_All_Bytes_0K >= Globa_2->Control_Upgrade_Firmware_Info.Firmware_ALL_Bytes)&&(Globa_2->Control_Upgrade_Firmware_Info.firmware_info_setStep == 0x05)){ //说明发送完毕并且也接收到最后一帧数据返回
					if(open_fp2 != NULL){	
						fclose(open_fp2);
						open_fp2 = NULL;
						Globa_2->Control_Upgrade_Firmware_Info.firmware_info_success = 2;
						MC_All_Frame_2.MC_Recved_PGN22016_Flag = 0;
					  Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x10;
				    Globa_2->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x10;
					}
					break;	
				}
			  MC_All_Frame_2.MC_Recved_PGN22016_Flag = 1;
			}else{ //失败重新发送
        send_Tran_Packet_Number = ((pdata->Tran_Packet_Number[1]<<8)|pdata->Tran_Packet_Number[0]);
			}
			break;
		}
	 
 	  case PGN24832_ID_CM:{
		  MC_All_Frame_2.MC_Recved_PGN24832_Flag = 1;
			break;	
    }
		case PGN3840_ID_CM:{//控制板反馈电子锁解锁动作
		  MC_All_Frame_2.MC_Recved_PGN3840_Flag = 1;
			break;	
    }
		case PGN25088_ID_CM:{ //接收BMS上传过来的剩余变量数据。(比较固定的充电中不会更改的)
			if((frame.data[1] >= 1)&&(frame.data[1] <= ((sizeof(MC_PGN25088_FRAME)+5) + 5)/6)){
				if(frame.data[1] == 1){//第一个数据包
					MC_var.sequence = frame.data[1];
					MC_var.Total_Num_Packet = frame.data[2];
					MC_var.Total_Mess_Size = frame.data[3] + (frame.data[4]<<8);

					if((MC_var.Total_Num_Packet == ((sizeof(MC_PGN25088_FRAME)+5) + 5)/6)&&(MC_var.Total_Mess_Size == sizeof(MC_PGN25088_FRAME))){
						memcpy(&ml_data[0], &frame.data[2], 6);
						MC_var.sequence++;
					}
				}else if(frame.data[1] == ((sizeof(MC_PGN25088_FRAME)+5) + 5)/6){//最后一个数据包
					if(frame.data[1] == MC_var.sequence){
						memcpy(&ml_data[(MC_var.sequence-1)*6], &frame.data[2], 6);

						sum = 0;
						for(i=0; i<(3+sizeof(MC_PGN25088_FRAME)); i++){
							sum +=  ml_data[i];
						}

						if(((sum&0xff) ==  ml_data[i])&&((sum&0xff00) == (ml_data[i+1]<<8))){
							MC_All_Frame_2.MC_Recved_PGN25088_Flag = 1;
							MC_var.sequence = 0;
		  				memcpy(&MC_All_Frame_2.MC_PGN25088_frame, &ml_data[3], sizeof(MC_PGN25088_FRAME));
							memcpy(&Globa_2->BMS_Vel[0], &MC_All_Frame_2.MC_PGN25088_frame.BMS_Vel, sizeof(MC_All_Frame_2.MC_PGN25088_frame.BMS_Vel));
		  				Globa_2->BMS_rate_AH = ((MC_All_Frame_2.MC_PGN25088_frame.Battery_Rate_AH[1]<<8) + MC_All_Frame_2.MC_PGN25088_frame.Battery_Rate_AH[0]);
		  				Globa_2->BMS_rate_voltage = ((MC_All_Frame_2.MC_PGN25088_frame.Battery_Rate_Vol[1]<<8) + MC_All_Frame_2.MC_PGN25088_frame.Battery_Rate_Vol[0]);
		  				Globa_2->Battery_Rate_KWh = ((MC_All_Frame_2.MC_PGN25088_frame.Battery_Rate_KWh[1]<<8) + MC_All_Frame_2.MC_PGN25088_frame.Battery_Rate_KWh[0]);
							Globa_2->Cell_H_Cha_Temp =   MC_All_Frame_2.MC_PGN25088_frame.Cell_H_Cha_Temp;
		  				Globa_2->Cell_H_Cha_Vol = (MC_All_Frame_2.MC_PGN25088_frame.Cell_H_Cha_Vol[1]<<8) + MC_All_Frame_2.MC_PGN25088_frame.Cell_H_Cha_Vol[0];
							memcpy(&Globa_2->VIN[0], &MC_All_Frame_2.MC_PGN25088_frame.VIN, sizeof(MC_All_Frame_2.MC_PGN25088_frame.VIN));
						  Globa_2->BMS_Info_falg = 1;
						}
					}
				}else{//中间帧
					if(frame.data[1] == MC_var.sequence){
						memcpy(&ml_data[(MC_var.sequence-1)*6], &frame.data[2], 6);
						MC_var.sequence++;
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
int MC_sent_data_2(INT32 fd, UINT32 PGN, UINT32 flag, UINT32 gun)
{
	int ret;
	UINT32 data;
	UINT32 i, sum;
	struct can_frame frame;
	struct tm tm_t;
	time_t systime=0;

	UINT8 mc_data[50];  //多帧数据发送的暂存缓存区

	memset(&frame.data[0], 0xff, sizeof(frame.data)); //不用数据项填1

	switch(PGN){
		case PGN256_ID_MC:{//充电启动帧
			frame.can_id = PGN256_ID_MC;
			frame.can_dlc = 8;

			MC_PGN256_FRAME * pdata = (MC_PGN256_FRAME *)frame.data;

			pdata->gun = gun;        //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
			pdata->couple = 2;     //负荷控制开关	BIN	1Byte	根据用户类型提供不同功率输出。1启用，2关闭，其他无效。
			if(Globa_2->Start_Mode == VIN_START_TYPE){
				if(Globa_2->VIN_Support_falg == 1){
				   pdata->ch_type = 3;
				}else {
					pdata->ch_type = 1;//直接启动
				}
			}else {
			  pdata->ch_type = flag; //充电启动方式	BIN	1Byte	1自动，2手动，其他无效。
		  }
			data = Globa_2->Charger_param.set_voltage/100;
			pdata->set_voltage[0] = data; 
			pdata->set_voltage[1] = data>>8;
			data = Globa_2->Charger_param.set_current/100;
			pdata->set_current[0] = data;
			pdata->set_current[1] = data>>8;
			pdata->Gun_Elec_Way = 0X0F;
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

			data = Globa_2->Charger_param.Charge_rate_voltage/1000;
			MC_All_Frame_2.MC_PGN2304_frame.Charge_rate_voltage[0] = data;
			MC_All_Frame_2.MC_PGN2304_frame.Charge_rate_voltage[1] = data>>8;

			MC_All_Frame_2.MC_PGN2304_frame.Charge_rate_number = Globa_2->Charger_param.Charge_rate_number2;
			MC_All_Frame_2.MC_PGN2304_frame.model_rate_current = Globa_2->Charger_param.model_rate_current/1000;
		//	MC_All_Frame_2.MC_PGN2304_frame.gun_config = Globa_2->Charger_param.gun_config;
			if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候{
			  MC_All_Frame_2.MC_PGN2304_frame.gun_config = 3;//				控制版中的是4-壁挂 1-单枪2-轮充 3--同时充电
			}else if(Globa_1->Charger_param.System_Type == 1 ){//2-轮充
				MC_All_Frame_2.MC_PGN2304_frame.gun_config = 2;//	
			}else if(Globa_1->Charger_param.System_Type == 2 ){//单枪
			  MC_All_Frame_2.MC_PGN2304_frame.gun_config = 1;//	
			}else if(Globa_1->Charger_param.System_Type == 3){//壁挂式
			  MC_All_Frame_2.MC_PGN2304_frame.gun_config = 4;//	
			}
			MC_All_Frame_2.MC_PGN2304_frame.couple_flag = Globa_2->Charger_param.couple_flag;
			MC_All_Frame_2.MC_PGN2304_frame.BMS_work = Globa_2->Charger_param.BMS_work;

			data = Globa_2->Charger_param.input_over_limit/1000;
			MC_All_Frame_2.MC_PGN2304_frame.input_over_limit[0] = data;
			MC_All_Frame_2.MC_PGN2304_frame.input_over_limit[1] = data>>8;

			data = Globa_2->Charger_param.input_lown_limit/1000;
			MC_All_Frame_2.MC_PGN2304_frame.input_lown_limit[0] = data;
			MC_All_Frame_2.MC_PGN2304_frame.input_lown_limit[1] = data>>8;

			data = Globa_2->Charger_param.output_over_limit/1000;
			MC_All_Frame_2.MC_PGN2304_frame.output_over_limit[0] = data;
			MC_All_Frame_2.MC_PGN2304_frame.output_over_limit[1] = data>>8;

			data = Globa_2->Charger_param.output_lown_limit/1000;
			MC_All_Frame_2.MC_PGN2304_frame.output_lown_limit[0] = data;
			MC_All_Frame_2.MC_PGN2304_frame.output_lown_limit[1] = data>>8;
			MC_All_Frame_2.MC_PGN2304_frame.BMS_CAN_ver = Globa_2->Charger_param.BMS_CAN_ver;
			MC_All_Frame_2.MC_PGN2304_frame.current_limit  = Globa_2->Charger_param.current_limit;
			MC_All_Frame_2.MC_PGN2304_frame.gun_allowed_temp = Globa_2->Charger_param.gun_allowed_temp;
			MC_All_Frame_2.MC_PGN2304_frame.Charging_gun_type = Globa_2->Charger_param.Charging_gun_type;
			MC_All_Frame_2.MC_PGN2304_frame.model_Type  = Globa_2->Charger_param.Model_Type;
			MC_All_Frame_2.MC_PGN2304_frame.Input_Relay_Ctl_Type  = Globa_1->Charger_param.Input_Relay_Ctl_Type;
			MC_All_Frame_2.MC_PGN2304_frame.LED_Type_Keep  = (Globa_1->Charger_param.LED_Type_Config << 4)&0xF0;
	    MC_All_Frame_2.MC_PGN2304_frame.Charge_min_voltage[0] = 0;
			MC_All_Frame_2.MC_PGN2304_frame.Charge_min_voltage[1] = 0;
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_PE_Fault = Globa_1->Charger_Shileld_Alarm.Shileld_PE_Fault;	       //屏蔽接地故障告警 ----非停机故障，指做显示	    
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_AC_connect_lost = Globa_1->Charger_Shileld_Alarm.Shileld_AC_connect_lost;	//屏蔽交流输通讯告警	---非停机故障，只提示告警信息    
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_AC_input_switch_off = Globa_1->Charger_Shileld_Alarm.Shileld_AC_input_switch_off;//屏蔽交流输入开关跳闸-----非停机故障交流接触器
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_AC_fanglei_off = Globa_1->Charger_Shileld_Alarm.Shileld_AC_fanglei_off;    //屏蔽交流防雷器跳闸---非停机故障，只提示告警信息 
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_circuit_breaker_trip = Globa_1->Charger_Shileld_Alarm.Shileld_circuit_breaker_trip;//交流断路器跳闸.2---非停机故障，只提示告警信息 
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_circuit_breaker_off = Globa_1->Charger_Shileld_Alarm.Shileld_circuit_breaker_off;// 交流断路器断开.2---非停机故障，只提示告警信息 
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_AC_input_over_limit = Globa_1->Charger_Shileld_Alarm.Shileld_AC_input_over_limit;//屏蔽交流输入过压保护 ----  ---非停机故障，只提示告警信息 
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_AC_input_lown_limit = Globa_1->Charger_Shileld_Alarm.Shileld_AC_input_lown_limit; //屏蔽交流输入欠压 ---非停机故障，只提示告警信息 
			
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_GUN_LOCK_FAILURE = Globa_1->Charger_Shileld_Alarm.Shileld_GUN_LOCK_FAILURE;  //屏蔽枪锁检测未通过
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_AUXILIARY_POWER = Globa_1->Charger_Shileld_Alarm.Shileld_AUXILIARY_POWER;   //屏蔽辅助电源检测未通过
	  	MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_OUTSIDE_VOLTAGE = Globa_1->Charger_Shileld_Alarm.Shileld_OUTSIDE_VOLTAGE;    //屏蔽输出接触器外侧电压检测（标准10V，放开100V）
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_SELF_CHECK_V = Globa_1->Charger_Shileld_Alarm.Shileld_SELF_CHECK_V;      //屏蔽自检输出电压异常
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_CHARG_OUT_OVER_V = Globa_1->Charger_Shileld_Alarm.Shileld_CHARG_OUT_OVER_V;  //屏蔽输出过压停止充电   ----  data2
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_CHARG_OUT_Under_V = Globa_1->Charger_Shileld_Alarm.Shileld_CHARG_OUT_Under_V;  //屏蔽输出欠压停止充电   ----  data2
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_CHARG_OUT_OVER_C = Globa_1->Charger_Shileld_Alarm.Shileld_CHARG_OUT_OVER_C;  //屏蔽输出过流
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_OUT_SHORT_CIRUIT = Globa_1->Charger_Shileld_Alarm.Shileld_OUT_SHORT_CIRUIT;  //屏蔽输出短路
			
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_GUN_OVER_TEMP = Globa_1->Charger_Shileld_Alarm.Shileld_GUN_OVER_TEMP;     //屏蔽充电枪温度过高
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_INSULATION_CHECK = Globa_1->Charger_Shileld_Alarm.Shileld_INSULATION_CHECK;   //屏蔽绝缘检测
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_OUT_SWITCH = Globa_1->Charger_Shileld_Alarm.Shileld_OUT_SWITCH;       //屏蔽输出开关检测
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_OUT_NO_CUR = Globa_1->Charger_Shileld_Alarm.Shileld_OUT_NO_CUR;       //屏蔽输出无电流显示检测	
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_BMS_WARN_Monomer_vol_anomaly = Globa_1->Charger_Shileld_Alarm.Shileld_BMS_WARN_Monomer_vol_anomaly;          //屏蔽BMS状态:单体电压异常
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_BMS_WARN_Soc_anomaly = Globa_1->Charger_Shileld_Alarm.Shileld_BMS_WARN_Soc_anomaly;          //屏蔽BMS状态:SOC异常
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_BMS_WARN_over_temp_anomaly = Globa_1->Charger_Shileld_Alarm.Shileld_BMS_WARN_over_temp_anomaly;          //屏蔽BMS状态:过温告警
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_BMS_WARN_over_Cur_anomaly = Globa_1->Charger_Shileld_Alarm.Shileld_BMS_WARN_over_Cur_anomaly;          //屏蔽BMS状态:BMS状态:过流告警
			
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_BMS_WARN_Insulation_anomaly = Globa_1->Charger_Shileld_Alarm.Shileld_BMS_WARN_Insulation_anomaly;          //BMS状态:绝缘异常
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_BMS_WARN_Outgoing_conn_anomaly = Globa_1->Charger_Shileld_Alarm.Shileld_BMS_WARN_Outgoing_conn_anomaly;          //BMS状态:输出连接器异常
			
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_BMS_OUT_OVER_V = Globa_1->Charger_Shileld_Alarm.Shileld_BMS_OUT_OVER_V;          //BMS状态:输出连接器异常
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_output_fuse_switch = Globa_1->Charger_Shileld_Alarm.Shileld_output_fuse_switch;          ////输出熔丝段
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_door_falg = Globa_1->Charger_Shileld_Alarm.Shileld_door_falg; //门禁         //保留

			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_keep_data = 0;          //保留
			
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_CELL_V_falg = Globa_1->Charger_Shileld_Alarm.Shileld_CELL_V_falg;          //是否上送单体电压标志
			MC_All_Frame_2.MC_PGN2304_frame.charger_shileld_alarm_value.Shileld_CELL_T_falg = Globa_1->Charger_Shileld_Alarm.Shileld_CELL_T_falg;          //是否上送单体温度标志
			
		  char*contro_ver = strstr(Globa_2->contro_ver,".");
			contro_ver++;
      float contro_ver_value = atof(contro_ver);
			//printf("xxxxxxxxxxxxxxxxxxxxxx2222222 =%f  Globa_2->contro_ver=%s  contro_ver =%s\n",contro_ver_value,Globa_2->contro_ver,contro_ver);
      int contro_ver_value1= contro_ver_value*100;
	
		  if(contro_ver_value1 < 218){
				mc_data[0] = ((sizeof(MC_PGN2304_FRAME) - 2 - sizeof(CHARGER_SHILELD_ALARM_MC) + 5) + 5)/6;
				mc_data[1] = sizeof(MC_PGN2304_FRAME)- 2 - sizeof(CHARGER_SHILELD_ALARM_MC);
				mc_data[2] = 0;
				memcpy(&mc_data[3], &MC_All_Frame_2.MC_PGN2304_frame, sizeof(MC_PGN2304_FRAME)- 2 - sizeof(CHARGER_SHILELD_ALARM_MC));
				sum = 0;
				for(i=0; i<(3+ sizeof(MC_PGN2304_FRAME)- 2 - sizeof(CHARGER_SHILELD_ALARM_MC)); i++){
					sum +=  mc_data[i];
				}
			}else{
				mc_data[0] = ((sizeof(MC_PGN2304_FRAME)+5) + 5)/6;
				mc_data[1] = sizeof(MC_PGN2304_FRAME);
				mc_data[2] = 0;
				memcpy(&mc_data[3], &MC_All_Frame_2.MC_PGN2304_frame, sizeof(MC_PGN2304_FRAME));

				sum = 0;
				for(i=0; i<(3+sizeof(MC_PGN2304_FRAME)); i++){
					sum +=  mc_data[i];
				}
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
      MC_alarm_Clearn_gun2();
			return(ret);//此处多帧连续发送，发完直接退出函数

			break;
		}
		case PGN3072_ID_MC:{ //下发直流分流器量程
			frame.can_id = PGN3072_ID_MC;
			frame.can_dlc = 8;
			MC_PGN3072_FRAME * pdata = (MC_PGN3072_FRAME *)frame.data;
			pdata->gun = gun;
			pdata->DC_Shunt_Range[0] = Globa_2->Charger_param.DC_Shunt_Range; 
			pdata->DC_Shunt_Range[1] = Globa_2->Charger_param.DC_Shunt_Range>>8;
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
			pdata->crc[0] = 0;   //
			pdata->crc[1] = 0;   //
      pdata->gun_state = Globa_2->gun_state;
			pdata->JKError_system = Globa_2->Error_system;
      pdata->FF[0] = 0xFF;   //
			pdata->FF[1] = 0xFF;   //
			break;
		}

		case PGN12544_ID_MC:{//心跳帧
			frame.can_id = PGN12544_ID_MC;
			frame.can_dlc = 8;

			MC_PGN12544_FRAME * pdata = (MC_PGN12544_FRAME *)frame.data;

			pdata->gun = gun;      //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
			pdata->mc_state = 0;   //计费控制单元状态信息	BIN	1Byte	0-正常 1-故障

			data = Globa_2->kwh/10;
			pdata->kwh[0] = data; 
			pdata->kwh[1] = data>>8;
			data = Globa_2->Time/60;
			pdata->time[0] = data;
			pdata->time[1] = data>>8;

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
			pdata->Firmware_Bytes[0] = Globa_2->Control_Upgrade_Firmware_Info.Firmware_ALL_Bytes;
			pdata->Firmware_Bytes[1] = Globa_2->Control_Upgrade_Firmware_Info.Firmware_ALL_Bytes>>8;
			pdata->Firmware_Bytes[2] = Globa_2->Control_Upgrade_Firmware_Info.Firmware_ALL_Bytes>>16;
			pdata->Firmware_CRC[0] = Globa_2->Control_Upgrade_Firmware_Info.Firware_CRC_Value;
			pdata->Firmware_CRC[1] = Globa_2->Control_Upgrade_Firmware_Info.Firware_CRC_Value>>8;
			pdata->Firmware_CRC[2] = Globa_2->Control_Upgrade_Firmware_Info.Firware_CRC_Value>>16;
			pdata->Firmware_CRC[3] = Globa_2->Control_Upgrade_Firmware_Info.Firware_CRC_Value>>24;
			break;
		}
		case PGN21760_ID_MC:{
			if(flag == 0){//表示第一次发送数据并打开文件
				if(open_fp2 == NULL){
					open_fp2 = fopen(F_UPDATE_EAST_DC_CTL, "rb");
					if(NULL == open_fp2){
						perror("open mstConfig error \n");
						open_fp2 = NULL;
						break;
					}
				  if(fread(&read_send_buf, sizeof(UINT8), 5, open_fp2) == 5){
						send_Tran_Packet_Number = 1;
						SendTran_Packet = 1; //第一帧
						Globa_2->Control_Upgrade_Firmware_Info.Send_Tran_All_Bytes_0K = 5;
					}
				}
			}
		  if(Globa_2->Control_Upgrade_Firmware_Info.Send_Tran_All_Bytes_0K <= Globa_2->Control_Upgrade_Firmware_Info.Firmware_ALL_Bytes){
				frame.can_id = PGN21760_ID_MC;
				frame.can_dlc = 8;
				MC_PGN21760_FRAME * pdata = (MC_PGN21760_FRAME *)frame.data;
				pdata->gun = gun;
				pdata->Tran_Packet_Number[0] = send_Tran_Packet_Number;
				pdata->Tran_Packet_Number[1] = send_Tran_Packet_Number>>8;
				if((SendTran_Packet + 1) == send_Tran_Packet_Number){
					SendTran_Packet = SendTran_Packet +1;
					ret = fread(&read_send_buf, sizeof(UINT8), 5, open_fp2);
					if(ret > 0){
						Globa_2->Control_Upgrade_Firmware_Info.Send_Tran_All_Bytes_0K = Globa_2->Control_Upgrade_Firmware_Info.Send_Tran_All_Bytes_0K + ret;
						if(ret != 5){
							for(i =ret;i<5;i++){ 
								read_send_buf[i] = 0xFF;	
							}
						}
					}else{
						if(open_fp2 != NULL){	
							fclose(open_fp2);
							open_fp2 = NULL;
						}
						Globa_2->Control_Upgrade_Firmware_Info.firmware_info_success = 2;
						Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x10;
				    Globa_2->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x10;
						return(-1);
					}
				}
				pdata->Tran_data[0] = read_send_buf[0];
				pdata->Tran_data[1] = read_send_buf[1];
				pdata->Tran_data[2] = read_send_buf[2];
				pdata->Tran_data[3] = read_send_buf[3];
				pdata->Tran_data[4] = read_send_buf[4];
			}else{
				if(open_fp2 != NULL){	
					fclose(open_fp2);
					open_fp2 = NULL;
				}
				Globa_2->Control_Upgrade_Firmware_Info.firmware_info_success = 2;
				Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x10;
				Globa_2->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x10;
				return (-1);
			}
			break;  	
		}
		/*case PGN22272_ID_MC:{//
			frame.can_id = PGN22272_ID_MC;
			frame.can_dlc = 8;

			MC_PGN22272_FRAME * PGN22272_frame = (MC_PGN22272_FRAME *)frame.data;
			PGN22272_frame->gun = gun;        //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
			PGN22272_frame->Shileld_emergency_switch = (Globa_1->Charge_Shileld_Alarmflag.Shileld_emergency_switch == 1)? 1:0;//屏蔽急停按下故障
			PGN22272_frame->Shileld_PE_Fault = (Globa_1->Charge_Shileld_Alarmflag.Shileld_PE_Fault == 1)? 1:0;	//屏蔽接地故障告警	    
			PGN22272_frame->Shileld_AC_connect_lost = (Globa_1->Charge_Shileld_Alarmflag.Shileld_AC_connect_lost == 1)? 1:0;	//屏蔽交流输通讯告警	    
			PGN22272_frame->Shileld_AC_input_switch_off = (Globa_1->Charge_Shileld_Alarmflag.Shileld_AC_input_switch_off == 1)? 1:0;//屏蔽交流输入开关跳闸
			
			PGN22272_frame->Shileld_AC_fanglei_off = (Globa_1->Charge_Shileld_Alarmflag.Shileld_AC_fanglei_off == 1)? 1:0;    //屏蔽交流防雷器跳闸
			PGN22272_frame->Shileld_AC_output_switch_off_1 = 	(Globa_1->Charge_Shileld_Alarmflag.Shileld_AC_output_switch_off[0] == 1)? 1:0;//屏蔽直流输出开关跳闸1.2
			PGN22272_frame->Shileld_AC_output_switch_off_2 = (Globa_1->Charge_Shileld_Alarmflag.Shileld_AC_output_switch_off[1] == 1)? 1:0;//屏蔽直流输出开关跳闸1.2
			PGN22272_frame->Shileld_AC_input_over_limit = (Globa_1->Charge_Shileld_Alarmflag.Shileld_AC_input_over_limit == 1)? 1:0;//屏蔽交流输入过压保护 ----  data1
			
			PGN22272_frame->Shileld_AC_input_lown_limit = (Globa_1->Charge_Shileld_Alarmflag.Shileld_AC_input_lown_limit == 1)? 1:0; //屏蔽交流输入欠压
			PGN22272_frame->Shileld_GUN_LOCK_FAILURE = (Globa_1->Charge_Shileld_Alarmflag.Shileld_GUN_LOCK_FAILURE == 1)? 1:0;  //屏蔽枪锁检测未通过
			PGN22272_frame->Shileld_AUXILIARY_POWER = (Globa_1->Charge_Shileld_Alarmflag.Shileld_AUXILIARY_POWER == 1)? 1:0;   //屏蔽辅助电源检测未通过
			PGN22272_frame->Shileld_OUTSIDE_VOLTAGE = (Globa_1->Charge_Shileld_Alarmflag.Shileld_OUTSIDE_VOLTAGE == 1)? 1:0;    //屏蔽输出接触器外侧电压检测（标准10V，放开100V）
			
			PGN22272_frame->Shileld_SELF_CHECK_V = (Globa_1->Charge_Shileld_Alarmflag.Shileld_SELF_CHECK_V == 1)? 1:0;      //屏蔽自检输出电压异常
			PGN22272_frame->Shileld_INSULATION_FAULT = (Globa_1->Charge_Shileld_Alarmflag.Shileld_INSULATION_FAULT == 1)? 1:0;   //屏蔽绝缘故障
			PGN22272_frame->Shileld_BMS_WARN = (Globa_1->Charge_Shileld_Alarmflag.Shileld_BMS_WARN == 1)? 1:0;          //屏蔽 BMS告警终止充电
			PGN22272_frame->Shileld_CHARG_OUT_OVER_V = (Globa_1->Charge_Shileld_Alarmflag.Shileld_CHARG_OUT_OVER_V == 1)? 1:0;  //屏蔽输出过压停止充电   ----  data2
			
			PGN22272_frame->Shileld_OUT_OVER_BMSH_V = (Globa_1->Charge_Shileld_Alarmflag.Shileld_OUT_OVER_BMSH_V == 1)? 1:0;   //屏蔽输出电压超过BMS最高电压
			PGN22272_frame->Shileld_CHARG_OUT_OVER_C = (Globa_1->Charge_Shileld_Alarmflag.Shileld_CHARG_OUT_OVER_C == 1)? 1:0;  //屏蔽输出过流
			PGN22272_frame->Shileld_OUT_SHORT_CIRUIT = (Globa_1->Charge_Shileld_Alarmflag.Shileld_OUT_SHORT_CIRUIT == 1)? 1:0;  //屏蔽输出短路
			
			PGN22272_frame->Shileld_GUN_OVER_TEMP = (Globa_1->Charge_Shileld_Alarmflag.Shileld_GUN_OVER_TEMP == 1)? 1:0;     //屏蔽充电枪温度过高
			PGN22272_frame->Shileld_OUT_SWITCH = 	(Globa_1->Charge_Shileld_Alarmflag.Shileld_OUT_SWITCH  == 1)? 1:0;       //屏蔽输出开关
			PGN22272_frame->Shileld_OUT_NO_CUR = (Globa_1->Charge_Shileld_Alarmflag.Shileld_OUT_NO_CUR == 1)? 1:0;       //屏蔽输出无电流显示
			PGN22272_frame->Shileld_Link_Switch = (Globa_1->Charge_Shileld_Alarmflag.Shileld_Link_Switch == 1)? 1:0 ;      //屏蔽并机接触器     ----  data3

			break;
		}*/
		case PGN24576_ID_MC:{
			frame.can_id = PGN24576_ID_MC;
			frame.can_dlc = 8;
			MC_PGN24576_FRAME * pdata = (MC_PGN24576_FRAME *)frame.data;
			pdata->gun = gun;    //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
			pdata->Relay_control = flag;   //负荷控制开关	BIN	1Byte	根据用户类型提供不同功率输出。1启用，2关闭，其他无效。
			break;
		}
		case PGN3584_ID_MC:{//枪电子锁解锁控制
			frame.can_id = PGN3584_ID_MC;
			frame.can_dlc = 8;
			MC_PGN3584_FRAME * pdata = (MC_PGN3584_FRAME *)frame.data;
			pdata->gun = gun;    //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
			pdata->Operation_command = flag;   //负荷控制开关	BIN	1Byte	根据用户类型提供不同功率输出。1启用，2关闭，其他无效。
			break;
		}
		case PGN25344_ID_MC:{//VIN反馈
			frame.can_id = PGN25344_ID_MC;
			frame.can_dlc = 8;
			MC_PGN25344_FRAME * pdata = (MC_PGN25344_FRAME *)frame.data;
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
extern void MC_control_2(INT32 fd)
{
	UINT32 sistent;
	UINT32 voltage;

	switch(Globa_2->MC_Step){
		case 0x01:{//----------------------0 就绪状态  0.1--------------------------

			memset(&MC_All_Frame_2, 0x00, sizeof(MC_All_Frame_2));//把MC相关的数据和标志全部清零

			Globa_2->MC_Step = 0x11;        //1 等待启动充电应答  1.1

    	timer_start_2(250);
    	timer_start_2(5000);

  		MC_sent_data_2(fd, PGN256_ID_MC, Globa_2->charger_start, 2);  //充电启动帧
  		sent_warning_message(0x94, 1, 2, 0);
      ctr2_cmd_lost_count = 0;
			break;
		}
		case 0x11:{//----------------------1 等待启动充电应答  1.1------------------
			if(MC_All_Frame_2.MC_Recved_PGN512_Flag == 0x01){//启动应答帧
				MC_All_Frame_2.MC_Recved_PGN512_Flag = 0x00;
				Globa_2->MC_Step = 0x21;   //充电中  2.1
  			timer_start_2(250);
				timer_start_2(3000);
  			timer_stop_2(5000);
				ctr2_cmd_lost_count = 0;
  			break;
			}

			if(timer_tick_2(250) == 1){  //定时250MS
				MC_sent_data_2(fd, PGN256_ID_MC, Globa_2->charger_start, 2);  //充电启动帧
				timer_start_2(250);
			}

			if(timer_tick_2(5000) == 1){//超时5秒
		  	ctr2_cmd_lost_count++;
				timer_start_2(5000);
				if(ctr2_cmd_lost_count >= 2){
			  	Globa_2->MC_Step = 0xE1;//控制器响应超时
					ctr2_cmd_lost_count = 0;
				}
  			break;
			}

			break;
		}
		case 0x21:{//----------------------2  充电中  2.1---------------------------
			if(MC_All_Frame_2.MC_Recved_PGN12800_Flag == 0x01){//心跳响应帧
				MC_All_Frame_2.MC_Recved_PGN12800_Flag = 0x00;
  			timer_start_2(3000);
			}

			if(timer_tick_2(250) == 1){  //定时250MS
				MC_sent_data_2(fd, PGN12544_ID_MC, 1, 2);  //心跳帧
				timer_start_2(250);
			}
      
			if(MC_All_Frame_2.MC_Recved_PGN25088_Flag == 0x01){//启动完成帧
				MC_All_Frame_2.MC_Recved_PGN25088_Flag = 0x00;
				MC_sent_data_2(fd, PGN25344_ID_MC, Globa_2->VIN_Judgment_Result, 2);  //心跳帧
			}
	
			if(MC_All_Frame_2.MC_Recved_PGN4352_Flag == 0x01){//启动完成帧
				MC_All_Frame_2.MC_Recved_PGN4352_Flag = 0x00;
				MC_sent_data_2(fd, PGN4608_ID_MC, 1, 2);  //启动完成帧应答
			}
			
  		if((Globa_2->Manu_start == 0)||(Globa_2->Error_system != 0)){//手动中止充电或系统故障中止充电
  			MC_sent_data_2(fd, PGN768_ID_MC, 1, 2);  //充电停止帧
			  if(Globa_2->Error.ctrl_connect_lost == 1){
					Globa_2->MC_Step = 0xE1;//控制器响应超时
					timer_stop_2(5000);
					ctr2_cmd_lost_count = 0;
  		   	break;
				}
				Globa_2->MC_Step = 0x31;
				timer_start_2(250);
				timer_start_2(5000);
				timer_stop_2(3000);
				ctr2_cmd_lost_count = 0;
  			break;
			}

			if(MC_All_Frame_2.MC_Recved_PGN4864_Flag == 0x01){//停止完成帧
				MC_All_Frame_2.MC_Recved_PGN4864_Flag = 0x00;
				Globa_2->MC_Step = 0xE0;   //充电流程结束
        ctr2_cmd_lost_count = 0;
				MC_sent_data_2(fd, PGN5120_ID_MC, 1, 2);  //停止充电完成应答帧
				MC_sent_data_2(fd, PGN5120_ID_MC, 1, 2);  //停止充电完成应答帧

  			break;
			}

			/*if(timer_tick_2(3000) == 1){//超时3秒
				Globa_2->MC_Step = 0xE1;//控制器响应超时
  			break;
			}*/

			break;
		}
		case 0x31:{//----------------------3  等待停止充电应答  3.1-----------------
			if(MC_All_Frame_2.MC_Recved_PGN1024_Flag == 0x01){//停止充电应答帧
				MC_All_Frame_2.MC_Recved_PGN1024_Flag = 0x00;
				Globa_2->MC_Step = 0x32;   //等待停止完成帧  3.2
				timer_stop_2(250);
				timer_stop_2(3000);
				timer_start_2(5000);
				ctr2_cmd_lost_count = 0;
  			break;
			}

			if(timer_tick_2(250) == 1){  //定时250MS
				MC_sent_data_2(fd, PGN768_ID_MC, 1, 2);  //充电停止帧
				timer_start_2(250);
			}

			if(timer_tick_2(5000) == 1){//超时5秒
			 	ctr2_cmd_lost_count++;
				timer_start_2(5000);
				if(	ctr2_cmd_lost_count>= 2){
			  	Globa_2->MC_Step = 0xE1;//控制器响应超时
					ctr2_cmd_lost_count = 0;
				}
  			break;
			}

			break;
		}
		case 0x32:{//----------------------3  等待停止完成帧  3.2-------------------
			if(MC_All_Frame_2.MC_Recved_PGN4864_Flag == 0x01){//停止完成帧
				MC_All_Frame_2.MC_Recved_PGN4864_Flag = 0x00;
				Globa_2->MC_Step = 0xE0;   //充电流程结束

				MC_sent_data_2(fd, PGN5120_ID_MC, 1, 2);  //停止充电完成应答帧
				MC_sent_data_2(fd, PGN5120_ID_MC, 1, 2);  //停止充电完成应答帧

  			break;
			}

			if(timer_tick_2(5000) == 1){//超时5秒
			 	ctr2_cmd_lost_count++;
				timer_start_2(5000);
				if(ctr2_cmd_lost_count >= 2){
			  	Globa_2->MC_Step = 0xE1;//控制器响应超时
					ctr2_cmd_lost_count = 0;
				}
  			break;
			}

			break;
		}
		case 0xE0:{//---------------------------E0 充电流程正常结束-----------------
			switch(Globa_2->ctl_over_flag & 0xFF00){//判断充电结束原因
				case 0xE100:{//计费控制单元控制停止
					if(Globa_2->Manu_start == 0){//计费控制单元正常停止(手动终止或达到设定条件如时间、电量、金额、余额不足)
						sent_warning_message(0x94, 14, 2, 0);
						Globa_2->charger_over_flag = 12;
					}else{//计费控制单元故障终止
						sent_warning_message(0x94, 40, 2, 0);
						Globa_2->charger_over_flag = 40;
					}

					break;
				}
				case 0xE200:{//控制器判定充电机故障，终止充电
					switch(Globa_2->ctl_over_flag){
    				case 0xE201:{//车载BMS未响应
    					sent_warning_message(0x94, 21, 2, 0);
    					Globa_2->charger_over_flag = 21;
    					break;
    				}
    				case 0xE202:{//车载BMS响应超时
    					sent_warning_message(0x94, 22, 2, 0);
    					Globa_2->charger_over_flag = 22;
    					break;
    				}
    				case 0xE203:{//电池反接
    					sent_warning_message(0x94, 23, 2, 0);
    					Globa_2->charger_over_flag = 23;
    					break;
    				}
    				case 0xE204:{//未检测到电池电压
    					sent_warning_message(0x94, 24, 2, 0);
    					Globa_2->charger_over_flag = 24;
    					break;
    				}
    				case 0xE205:{//BMS未准备就绪
    					sent_warning_message(0x94, 25, 2, 0);
    					Globa_2->charger_over_flag = 25;
    					break;
    				}
    				case 0xE206:{//充电枪连接断开
    					sent_warning_message(0x94, 26, 2, 0);
    					Globa_2->charger_over_flag = 26;
    					break;
    				}
    				case 0xE207:{//枪锁异常
    					sent_warning_message(0x94, 27, 2, 0);
    					Globa_2->charger_over_flag = 27;
    					break;
    				}
    				case 0xE208:{//自检输出电压异常
    					sent_warning_message(0x94, 28, 2, 0);
    					Globa_2->charger_over_flag = 28;
    					break;
    				}
    				case 0xE209:{//自检泄放电压异常----自检前检测到外侧电压异常
    					sent_warning_message(0x94, 29, 2, 0);
    					Globa_2->charger_over_flag = 29;
    					break;
    				}
    				case 0xE20A:{//辅助电源异常
    					sent_warning_message(0x94, 30, 2, 0);
    					Globa_2->charger_over_flag = 30;
    					break;
    				}
    				case 0xE20B:{//充电电压过压告警
    					sent_warning_message(0x94, 31, 2, 0);
    					Globa_2->charger_over_flag = 31;
    					break;
    				}
    				case 0xE20C:{//充电电流过流告警
    					sent_warning_message(0x94, 32, 2, 0);
    					Globa_2->charger_over_flag = 32;
    					break;
    				}
    				case 0xE20D:{//输出短路故障
    					sent_warning_message(0x94, 33, 2, 0);
    					Globa_2->charger_over_flag = 33;
    					break;
    				}
    				case 0xE20E:{//系统输出过压保护
    					sent_warning_message(0x94, 34, 2, 0);
    					Globa_2->charger_over_flag = 34;
    					break;
    				}
    				case 0xE20F:{//系统绝缘故障
    					sent_warning_message(0x94, 35, 2, 0);
    					Globa_2->charger_over_flag = 35;
    					break;
    				}
    				case 0xE210:{//输出接触器异常
    					sent_warning_message(0x94, 36, 2, 0);
    					Globa_2->charger_over_flag = 36;
    					break;
    				}
    				case 0xE211:{//辅助电源接触器异常
    					sent_warning_message(0x94, 37, 2, 0);
    					Globa_2->charger_over_flag = 37;
    					break;
    				}
    				case 0xE212:{//系统并机接触器异常
    					sent_warning_message(0x94, 38, 2, 0);
    					Globa_2->charger_over_flag = 38;
    					break;
    				}
						case 0xE213:{//充电枪温度过高
    					sent_warning_message(0x94, 39, 2, 0);
    					Globa_2->charger_over_flag = 39;
    					break;
    				}
						case 0xE214:{//由于充电过程中电流为0导致的结束原因，更改为正常结束
    					sent_warning_message(0x94, 10, 2, 0);
    					Globa_2->charger_over_flag = 10;
    					break;
    				}
						case 0xE215:{//超出了系统输出电压能力
    					sent_warning_message(0x94, 17, 2, 0);
    					Globa_2->charger_over_flag = 17;
    					break;
    				}
						case 0xE216:{//检测电池电压误差比较大
    					sent_warning_message(0x94, 18, 2, 0);
    					Globa_2->charger_over_flag = 18;
    					break;
    				}
						case 0xE217:{//预充阶段继电器两端电压相差大于10V
    					sent_warning_message(0x94, 20, 2, 0);
    					Globa_2->charger_over_flag = 20;
    					break;
    				}
						case 0xE218:{//系统判定BMS单体持续过压
    					sent_warning_message(0x94, 64,2, 0);
    					Globa_2->charger_over_flag = 64;
    					break;
    				}
						case 0xE219:{//系统判定BMS单体持续过压
    					sent_warning_message(0x94, 75, 2, 0);
    					Globa_2->charger_over_flag = 75;
    					break;
    				}
						case 0xE21A:{//充电模块未准备
    					sent_warning_message(0x94, 76, 1, 0);
    					Globa_2->charger_over_flag = 76;
    					break;
    				}
    				case 0xE2FF:{//系统其它故障，请查询告警记录
    					sent_warning_message(0x94, 40, 2, 0);
    					Globa_2->charger_over_flag = 40;
    					break;
    				}
    				default:{//未定义原因
    					sent_warning_message(0x94, Globa_2->ctl_over_flag+20, 2, 0);//加20因为报告给QT显示的时候从20开始偏移
    					Globa_2->charger_over_flag = Globa_2->ctl_over_flag+20;
    					break;
    				}
    			}
    			
    			break;
    		}
				case 0xE300:{//充电机判定BMS异常，终止充电
					switch(Globa_2->ctl_over_flag){
    				case 0xE301:{//单体电压异常
    					sent_warning_message(0x94, 56, 2, 0);
    					Globa_2->charger_over_flag = 56;
    					break;
    				}
    				case 0xE302:{//SOC异常
    					sent_warning_message(0x94, 57, 2, 0);
    					Globa_2->charger_over_flag = 57;
    					break;
    				}
    				case 0xE303:{//过流告警
    					sent_warning_message(0x94, 59, 2, 0);
    					Globa_2->charger_over_flag = 59;
    					break;
    				}
    				case 0xE304:{//过温告警 
    					sent_warning_message(0x94, 58, 2, 0);
    					Globa_2->charger_over_flag = 58;
    					break;
    				}
    				case 0xE305:{//绝缘异常
    					sent_warning_message(0x94, 60, 2, 0);
    					Globa_2->charger_over_flag = 60;
    					break;
    				}
    				case 0xE306:{//输出连接器异常
    					sent_warning_message(0x94, 61, 2, 0);
    					Globa_2->charger_over_flag = 61;
    					break;
    				}
    				default:{//未定义原因
    					sent_warning_message(0x94, Globa_2->ctl_over_flag+55, 2, 0);//加55因为报告给QT显示的时候从55开始偏移
    					Globa_2->charger_over_flag = Globa_2->ctl_over_flag+55;
    					break;
    				}
    			}
    			
    			break;
    		}
				case 0xE400:{//BMS正常终止充电
					switch(Globa_2->ctl_over_flag){
    				case 0xE401:{//达到所需的SOC
    					sent_warning_message(0x94, 41, 2, 0);
    					Globa_2->charger_over_flag = 41;
    					break;
    				}
    				case 0xE402:{//达到总电压的设定值
    					sent_warning_message(0x94, 42, 2, 0);
    					Globa_2->charger_over_flag = 42;
    					break;
    				}
    				case 0xE403:{//达到单体电压的设定值
    					sent_warning_message(0x94, 43, 2, 0);
    					Globa_2->charger_over_flag = 43;
    					break;
    				}

    				default:{//未定义原因
    					sent_warning_message(0x94, Globa_2->ctl_over_flag+40, 2, 0);//加40因为报告给QT显示的时候从40开始偏移
    					Globa_2->charger_over_flag = Globa_2->ctl_over_flag+40;
    					break;
    				}
    			}
    			
    			break;
    		}
				case 0xE500:{//控制器判定充电机故障，终止充电
					switch(Globa_2->ctl_over_flag){
    				case 0xE501:{//绝缘故障
    					sent_warning_message(0x94, 44, 2, 0);
    					Globa_2->charger_over_flag = 44;
    					break;
    				}
    				case 0xE502:{//输出连接器过温故障
    					sent_warning_message(0x94, 45, 2, 0);
    					Globa_2->charger_over_flag = 45;
    					break;
    				}
    				case 0xE503:{//BMS原件过温故障
    					sent_warning_message(0x94, 46, 2, 0);
    					Globa_2->charger_over_flag = 46;
    					break;
    				}
    				case 0xE504:{//充电连接器故障
    					sent_warning_message(0x94, 47, 2, 0);
    					Globa_2->charger_over_flag = 47;
    					break;
    				}
    				case 0xE505:{//电池组温度过高故障
    					sent_warning_message(0x94, 48, 2, 0);
    					Globa_2->charger_over_flag = 48;
    					break;
    				}
    				case 0xE506:{//高压继电器故障
    					sent_warning_message(0x94, 49, 2, 0);
    					Globa_2->charger_over_flag = 49;
    					break;
    				}
    				case 0xE507:{//检测点2电压检测故障
    					sent_warning_message(0x94, 50, 2, 0);
    					Globa_2->charger_over_flag = 50;
    					break;
    				}
    				case 0xE508:{//其他故障
    					sent_warning_message(0x94, 51, 2, 0);
    					Globa_2->charger_over_flag = 51;
    					break;
    				}
    				case 0xE509:{//充电电流过大
    					sent_warning_message(0x94, 52, 2, 0);
    					Globa_2->charger_over_flag = 52;
    					break;
    				}
    				case 0xE50A:{//充电电压异常
    					sent_warning_message(0x94, 53, 2, 0);
    					Globa_2->charger_over_flag = 53;
    					break;
    				}
    				case 0xE50B:{//BMS 未告知
    					sent_warning_message(0x94, 54, 2, 0);
    					Globa_2->charger_over_flag = 54;
    					break;
    				}
    				default:{//未定义原因
    					sent_warning_message(0x94, Globa_2->ctl_over_flag+43, 2, 0);//加43因为报告给QT显示的时候从43开始偏移
    					Globa_2->charger_over_flag = Globa_2->ctl_over_flag+43;
    					break;
    				}
    			}
    			
    			break;
    		}
    		default:{//未定义原因
    			sent_warning_message(0x94, Globa_2->ctl_over_flag>>8, 2, 0);//右移8位 让QT显示出非E1之E5的结束原因
    			Globa_2->charger_over_flag = Globa_2->ctl_over_flag>>8;
    			break;
    		}
			}

			Globa_2->SYS_Step = 0x21;    //系统充电结束状态处理

			break;
		}
		case 0xE1:{//---------------------------E1 控制器响应超时-------------------
    	/*if(Globa_2->Error.ctrl_connect_lost != 1){
    		Globa_2->Error.ctrl_connect_lost = 1;
    		sent_warning_message(0x99, 53, 2, 0);
    		Globa_2->Error_system++;
    	}*///此处没有成对出现告警消失，其消失放在遥测数据的地方判断

			sent_warning_message(0x94, 16, 2, 0);//充电控制板响应超时
			Globa_2->charger_over_flag = 16;
			Globa_2->SYS_Step = 0x21;    //系统充电结束状态处理
      ctr2_cmd_lost_count = 0;
			break;
		}
		default:{
			printf("系统进入未知状态 Globa_2->MC_Step =%d\n", Globa_2->MC_Step);
			while(1);//等待系统复位未知

			break;
		}
	}
}

