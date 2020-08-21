/*******************************************************************************
  Copyright(C)    2014-2015,      EAST Co.Ltd.
  File name :     core.c
  Author :        dengjh
  Version:        1.0
  Date:           2014.4.11
  Description:    
  Other:          
  Function List:  
  History:        
          <author>   <time>   <version>   <desc>
            dengjh    2014.4.11   1.0      Create
*******************************************************************************/
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/prctl.h>
#include "globalvar.h"
#include "iso8583.h"
#include "../../inc/sqlite/sqlite3.h"

#include "qt.h"
extern UINT32 Main_1_Watchdog;
UINT32 Dc_shunt_Set_meter_flag[CONTROL_DC_NUMBER] = {0};
UINT32 Dc_shunt_Set_CL_flag[CONTROL_DC_NUMBER] = {0};


CHARGER_MSG msg;	

static PAGE_90_FRAME   		Page_90;
static PAGE_110_FRAME  		Page_110_Info;
static PAGE_1000_FRAME 		Page_1000;//自动充电-》充电枪连接确认
static PAGE_200_FRAME  		Page_200;
static PAGE_400_FRAME       Page_400;//VIN鉴权等待

static PAGE_BMS_FRAME  		Page_BMS_data;
static PAGE_1111_FRAME 		Page_1111;
static PAGE_1E1_FRAME     Page_1E1;//双枪对一辆车界面选择
static PAGE_DAOJISHI_FRAME  Page_daojishi;
static PAGE_1300_FRAME 		Page_1300;
static PAGE_3600_FRAME 		Page_3600;
static PAGE_3503_FRAME 		Page_3503;//系统管理 系统设置 3503 
static PAGE_3203_FRAME 		Page_3203;//系统管理 网络设置 3203
static PAGE_2100_FRAME		Page_2100;//关于2100
static PAGE_5200_FRAME		Page_5200;
static PAGE_6100_FRAME		Page_6100;

static MISC_METER_DATA		misc_measure_data;//交流采集和电表数据

static UINT32 QT_count_500ms=0;
static UINT32 QT_count_500ms_flag=0;
static UINT32 QT_count_500ms_enable=0;
static UINT32 QT_count_5000ms=0;
static UINT32 QT_count_5000ms_flag=0;
static UINT32 QT_count_5000ms_enable=0;

static void QT_timer_start(int time)
{
	switch(time)
	{
		case 500:
		{
			QT_count_500ms = 0;
			QT_count_500ms_flag = 0;
			QT_count_500ms_enable = 1;
			break;
		}
		case 5000:
		{
			QT_count_5000ms = 0;
			QT_count_5000ms_flag = 0;
			QT_count_5000ms_enable = 1;
			break;
		}
		default:
		{
			break;
		}
	}
}
static void QT_timer_stop(int time)
{
	switch(time)
	{
		case 500:
		{
			QT_count_500ms = 0;
			QT_count_500ms_flag = 0;
			QT_count_500ms_enable = 0;
			break;
		}
		case 5000:
		{
			QT_count_5000ms = 0;
			QT_count_5000ms_flag = 0;
			QT_count_5000ms_enable = 0;
			break;
		}

		default:
		{
			break;
		}
	}
}
static UINT32 QT_timer_tick(int time)
{
	UINT32 flag=0;

	switch(time)
	{
		case 500:
		{
			flag = QT_count_500ms_flag;

			break;
		}
		case 5000:
		{
			flag = QT_count_5000ms_flag;
			break;
		}
		default:
		{
			break;
		}
	}

	return (flag);
}

// Voicechip_operation_function(0xE7); --调节声音的
/*************************语音芯片操作*****************************************/
void Voicechip_operation_function(UINT8 addr)
{
	UINT32 i;

	VO_CHIP_SELECT_L;
	usleep(5000);//5MS

	for(i =0;i<8;i++){
		VO_CLOCK_SIGNAL_L;
		if(((addr>>i)&0x01) == 1){
			VO_DATA_SIGNAL_H ;
		}else{
			VO_DATA_SIGNAL_L;
		}

		usleep(150);//150US
		VO_CLOCK_SIGNAL_H;
		usleep(150);//150US
	}
	
	usleep(5000);//5MS

	VO_DATA_SIGNAL_H;//data cs clk要保持高电平
	VO_CHIP_SELECT_H;
}

unsigned char order_chg_flag;//有序充电用户卡标记，域45
unsigned char order_card_end_time[2];       //当前有序充电卡允许充电的终止时刻 HH:MM	

/*旧
00H	"欢迎光临.wav 300s 请刷卡1.wav"			
01H	请插入充电枪并确认连接状态.wav			
02H	系统忙暂停使用.wav		//意义不大	
03H	系统故障暂停使用.wav			
04H	刷卡成功 新
05H	10s			
06H	10s			
07H	10s			
08H	10s			
09H	10s			
0AH	10s			
0BH	10s			
0CH	10s			
0DH	10s			
0EH	10s			
0FH	10s			
10H	请刷卡结算.wav			
11H	谢谢使用再见.wav			
*/

/*新
正常充电提示音:
00H     “欢迎光临”OK
01H     “请选择充电枪 ”OK
02H"    “请选择充电方式"  +
03H     “请插入充电枪，并确认连接状态 ” 连接状态发音急促
04H     “请刷卡”OK
05H     “系统自检中，请稍候”+
06H     “等待车辆连接中“OK
07H     “充电已完成，请刷卡结算”+
08H     “结算成功，请取下充电枪并挂回原处“+
09H     “谢谢惠顾，再见”+
刷卡处理提示音:
刷卡异常处理提示音：
0aH     “余额不足，请充值”OK
0bH     “卡片已灰锁，请解锁”+
0cH     “卡片已挂失，请联系发卡单位处理”+
0dH     “卡片未注册，请联系发卡单位处理”+
卡片处理提示音：
0eH     “请刷卡”OK
0fH     “操作中，请勿移开卡片”+
10H     “后台处理中，请取走卡片”+
11H     “卡片解锁成功”+
12H     “卡片解锁失败，请联系发卡单位处理“+
系统升级提升音:
13H    “系统维护中，暂停使用 ”+
异常处理提示应
14H    “系统故障，暂停使用 ”系发音过长
15H    “交流电压输入异常，请联系设备管理员检查线路”+
16H    “系统接地异常，请联系设备管理员检查线路”+
*/

void Send_main_info_data()
{
	int i = 0;
	unsigned long ul_temp,current;
	Page_110_Info.reserve1 = Globa->Charger_param.gun_config;//发送当前配置的枪数
	Page_110_Info.support_double_gun_to_one_car = Globa->Charger_param.support_double_gun_to_one_car;//是否启用双枪对一车
	for(i=0;i<CONTROL_DC_NUMBER;i++)
	{
		if(Globa->Control_DC_Infor_Data[i].charger_state == 0x00)
			Page_110_Info.Main_110_Data[i].charge_state = 0;//不存在==test
		else
		{
			if(Globa->Control_DC_Infor_Data[i].charger_state == 0x03)
			{
				if(Globa->Control_DC_Infor_Data[i].Error_system != 0 )
				{
					Page_110_Info.Main_110_Data[i].charge_state = 0x01; //故障
				}else
				{
					if(Globa->Control_DC_Infor_Data[i].gun_link == 0)
					{
					 Page_110_Info.Main_110_Data[i].charge_state = 0x03; //空闲
					}else if(Globa->Control_DC_Infor_Data[i].gun_link == 1)
					{
					 Page_110_Info.Main_110_Data[i].charge_state = 0x02;  //连接状态
					}
				}
			}else if(Globa->Control_DC_Infor_Data[i].charger_state == 0x04)
			{
				Page_110_Info.Main_110_Data[i].charge_state = 0x04; //充电中
			}if(Globa->Control_DC_Infor_Data[i].charger_state == 0x05)
			{
				Page_110_Info.Main_110_Data[i].charge_state  = 0x05; //完成
			}if(Globa->Control_DC_Infor_Data[i].charger_state == 0x06)
			{
				Page_110_Info.Main_110_Data[i].charge_state = 0x06; //预约
			}
			else if(Globa->Control_DC_Infor_Data[i].charger_state == 0x01){
				Page_110_Info.Main_110_Data[i].charge_state = 0x01; //故障
			}
		}
		Page_110_Info.Main_110_Data[i].charge_V[0] =  Globa->Control_DC_Infor_Data[i].Output_voltage/100;
		Page_110_Info.Main_110_Data[i].charge_V[1] = (Globa->Control_DC_Infor_Data[i].Output_voltage/100)>>8;
		current = Globa->Control_DC_Infor_Data[i].Output_current/100;
		Page_110_Info.Main_110_Data[i].charge_C[0] = current;
		Page_110_Info.Main_110_Data[i].charge_C[1] = current>>8;		
		//ul_temp = Globa->Control_DC_Infor_Data[i].kwh;
     	ul_temp = Globa->Control_DC_Infor_Data[i].soft_KWH;
		memcpy(Page_110_Info.Main_110_Data[i].KWH, &ul_temp, 4);
		//Page_110_Info.Main_110_Data[i].KWH[0] = Globa->Control_DC_Infor_Data[i].kwh;
		//Page_110_Info.Main_110_Data[i].KWH[1] = Globa->Control_DC_Infor_Data[i].kwh>>8;
	}
	if( (Globa->Control_DC_Infor_Data[0].Metering_Step >= 0x02)||
		(Globa->Control_DC_Infor_Data[1].Metering_Step >= 0x02)||
		(Globa->Control_DC_Infor_Data[2].Metering_Step >= 0x02)||
		(Globa->Control_DC_Infor_Data[3].Metering_Step >= 0x02)||
		(Globa->Control_DC_Infor_Data[4].Metering_Step >= 0x02)
		)
	{
	  Page_110_Info.Button_Show_flag = 1;
	}else
	{
	  Page_110_Info.Button_Show_flag = 0;//显示返回按钮
	}
	//test
	//for(i=0;( (i<Globa->Charger_param.gun_config) && (i<CONTROL_DC_NUMBER) );i++)
		//printf("Page_110_Info.Main_110_Data[%d].charge_state=%d\n",i,Page_110_Info.Main_110_Data[i].charge_state);
	
	memcpy(&msg.mtext[0], &Page_110_Info.reserve1, sizeof(Page_110_Info));
	msg.mtype = 0x120;
	msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(Page_110_Info), IPC_NOWAIT);
	
}

void Send_charge_info_data(int Gun_no)
{
	unsigned long ul_temp,current;
	Page_1111.Gun_no = Gun_no + 1;

	Page_1111.BATT_type = Globa->Control_DC_Infor_Data[Gun_no].BATT_type;		
	Page_1111.BATT_V[0] = Globa->Control_DC_Infor_Data[Gun_no].Output_voltage/100;
	Page_1111.BATT_V[1] = (Globa->Control_DC_Infor_Data[Gun_no].Output_voltage/100)>>8;
	current = Globa->Control_DC_Infor_Data[Gun_no].Output_current/100;//0.1A
	Page_1111.BATT_C[0] = current;
	Page_1111.BATT_C[1] = current>>8;
	//ul_temp = Globa->Control_DC_Infor_Data[Gun_no].kwh;	
	ul_temp = Globa->Control_DC_Infor_Data[Gun_no].soft_KWH;
	memcpy(Page_1111.KWH, &ul_temp,4);
	//Page_1111.KWH[0] =  Globa->Control_DC_Infor_Data[Gun_no].kwh;
	//Page_1111.KWH[1] =  Globa->Control_DC_Infor_Data[Gun_no].kwh>>8;

	Page_1111.Capact =  Globa->Control_DC_Infor_Data[Gun_no].BMS_batt_SOC;
	Page_1111.run    = (Globa->Control_DC_Infor_Data[Gun_no].Output_current > 1000)?1:0;//有充电电流 》1A

	Page_1111.elapsed_time[0] = Globa->Control_DC_Infor_Data[Gun_no].Time;
	Page_1111.elapsed_time[1] = Globa->Control_DC_Infor_Data[Gun_no].Time>>8;
	Page_1111.need_time[0] = Globa->Control_DC_Infor_Data[Gun_no].BMS_need_time;
	Page_1111.need_time[1] = Globa->Control_DC_Infor_Data[Gun_no].BMS_need_time>>8;
	Page_1111.cell_HV[0] =  Globa->Control_DC_Infor_Data[Gun_no].BMS_cell_HV;
	Page_1111.cell_HV[1] =  Globa->Control_DC_Infor_Data[Gun_no].BMS_cell_HV>>8;
	
	ul_temp = Globa->Control_DC_Infor_Data[Gun_no].total_rmb;
	memcpy(Page_1111.money, &ul_temp, 4);
	
	//Page_1111.charging = Page_1111.run;
	
	Page_1111.aux_gun = 0;
	if((1 == Globa->Control_DC_Infor_Data[Gun_no].Double_Gun_To_Car_falg)&&(0 == Globa->Control_DC_Infor_Data[Gun_no].Double_Gun_To_Car_Hostgun))//副枪
		Page_1111.aux_gun = 1;
	Page_1111.link =  (Globa->Control_DC_Infor_Data[Gun_no].gun_link == 1)?1:0;
	memcpy(&Page_1111.SN[0], &Globa->Control_DC_Infor_Data[Gun_no].card_sn[0], sizeof(Page_1111.SN));
	Page_1111.charger_start_way  = Globa->Control_DC_Infor_Data[Gun_no].charger_start_way + 1;    //充电启动方式 1:刷卡  2：手机APP 101:VIN
	if((Page_1111.charger_start_way == 1)||(Page_1111.charger_start_way == 101))//刷卡/VIN启动需显示停止按钮===180927
	{
		Page_1111.Button_Show_flag = 0;//显示按钮是否出现 0--出现 1--消失
		if(Page_1111.aux_gun)//双枪对一辆车充电，辅枪不显示停止按钮
			Page_1111.Button_Show_flag = 1;
	}else
	{
		Page_1111.Button_Show_flag = 1;
	}
	
	Page_1111.BMS_12V_24V = Globa->Control_DC_Infor_Data[Gun_no].BMS_Power_V;
	memcpy(&msg.mtext[0], &Page_1111.reserve1, sizeof(PAGE_1111_FRAME));
	//msg.mtype = 0x1111;//充电界面

	ul_temp = Globa->Control_DC_Infor_Data[Gun_no].output_kw;
	memcpy(Page_1111.power_kw, &ul_temp,4);
	
	memset(Page_1111.start_time,0,sizeof(Page_1111.start_time));
	memcpy(Page_1111.start_time,&Globa->Control_DC_Infor_Data[Gun_no].charge_start_time[11],8);	
	msg.mtype = 0x1122;//充电界面--单枪 
	msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_1111_FRAME), IPC_NOWAIT);
	QT_timer_start(500);
}

void Send_BMS_info_data(int Gun_no)
{
	short BMS_cell_T;//精确到1℃
	Page_BMS_data.BMS_Info_falg = Globa->Control_DC_Infor_Data[Gun_no].BMS_Info_falg;
	Page_BMS_data.BMS_H_vol[0]=  Globa->Control_DC_Infor_Data[Gun_no].BMS_H_voltage/100;
	Page_BMS_data.BMS_H_vol[1]=  (Globa->Control_DC_Infor_Data[Gun_no].BMS_H_voltage/100)>>8;
	Page_BMS_data.BMS_H_cur[0]=  Globa->Control_DC_Infor_Data[Gun_no].BMS_H_current/100;
	Page_BMS_data.BMS_H_cur[1]=  (Globa->Control_DC_Infor_Data[Gun_no].BMS_H_current/100)>>8;
	BMS_cell_T = Globa->Control_DC_Infor_Data[Gun_no].BMS_cell_LT;//UINT8--->short
	BMS_cell_T -= 50;//上传时有50度偏移---为了保持界面不修改，在这里先减50度偏移
	//Page_BMS_data.BMS_cell_LT[0]=  BMS_cell_T;//低字节
	//Page_BMS_data.BMS_cell_LT[1]=  BMS_cell_T>>8; 
	memcpy(Page_BMS_data.BMS_cell_LT,&BMS_cell_T,2);
	BMS_cell_T = Globa->Control_DC_Infor_Data[Gun_no].BMS_cell_HT;//UINT8--->short
	BMS_cell_T -= 50;//上传时有50度偏移
	//Page_BMS_data.BMS_cell_HT[0]=  BMS_cell_T;
	//Page_BMS_data.BMS_cell_HT[1]=  BMS_cell_T>>8;
	memcpy(Page_BMS_data.BMS_cell_HT,&BMS_cell_T,2);

	Page_BMS_data.need_voltage[0]=  Globa->Control_DC_Infor_Data[Gun_no].need_voltage/100;
	Page_BMS_data.need_voltage[1]=  (Globa->Control_DC_Infor_Data[Gun_no].need_voltage/100)>>8;
	Page_BMS_data.need_current[0]=  (Globa->Control_DC_Infor_Data[Gun_no].need_current/100);
	Page_BMS_data.need_current[1]=  (Globa->Control_DC_Infor_Data[Gun_no].need_current/100)>>8;
	
	memcpy(&Page_BMS_data.BMS_Vel[0],&Globa->Control_DC_Infor_Data[Gun_no].BMS_Vel[0],sizeof(Page_BMS_data.BMS_Vel));
	
	Page_BMS_data.Battery_Rate_Vol[0]=  Globa->Control_DC_Infor_Data[Gun_no].BMS_rate_voltage;
	Page_BMS_data.Battery_Rate_Vol[1]=  (Globa->Control_DC_Infor_Data[Gun_no].BMS_rate_voltage)>>8;
	Page_BMS_data.Battery_Rate_AH[0]=  Globa->Control_DC_Infor_Data[Gun_no].BMS_rate_AH;
	Page_BMS_data.Battery_Rate_AH[1]=  (Globa->Control_DC_Infor_Data[Gun_no].BMS_rate_AH)>>8;
	Page_BMS_data.Battery_Rate_KWh[0]=  (Globa->Control_DC_Infor_Data[Gun_no].Battery_Rate_KWh);
	Page_BMS_data.Battery_Rate_KWh[1]=  (Globa->Control_DC_Infor_Data[Gun_no].Battery_Rate_KWh)>>8;

	Page_BMS_data.Cell_H_Cha_Vol[0]=  (Globa->Control_DC_Infor_Data[Gun_no].Cell_H_Cha_Vol);
	Page_BMS_data.Cell_H_Cha_Vol[1]=  (Globa->Control_DC_Infor_Data[Gun_no].Cell_H_Cha_Vol)>>8;
	Page_BMS_data.Cell_H_Cha_Temp =  Globa->Control_DC_Infor_Data[Gun_no].Cell_H_Cha_Temp;//精确到1℃ -50℃ ~ +200℃ 值0表示-50℃
	Page_BMS_data.Charge_Mode =  Globa->Control_DC_Infor_Data[Gun_no].Charge_Mode;
	Page_BMS_data.Charge_Gun_Temp =  Globa->Control_DC_Infor_Data[Gun_no].Charge_Gun_Temp;//精确到1℃ -50℃ ~ +200℃ 值0表示-50℃
	Page_BMS_data.BMS_cell_HV_NO =  Globa->Control_DC_Infor_Data[Gun_no].BMS_cell_HV_NO;
	Page_BMS_data.BMS_cell_LT_NO =  Globa->Control_DC_Infor_Data[Gun_no].BMS_cell_LT_NO;
	Page_BMS_data.BMS_cell_HT_NO =  Globa->Control_DC_Infor_Data[Gun_no].BMS_cell_HT_NO;
	memcpy(&Page_BMS_data.VIN[0],&Globa->Control_DC_Infor_Data[Gun_no].VIN[0],sizeof(Page_BMS_data.VIN));
	memcpy(&msg.mtext[0], &Page_BMS_data.reserve1, sizeof(PAGE_BMS_FRAME));
	msg.mtype = 0x1588;//BMS info 0x1588
	msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_BMS_FRAME), IPC_NOWAIT);
}  

//计算文件的CRC32值，结果写入crc32_value
//返回值1表示计算OK，其他值异常
//file_size为待计算的文件长度
static int File_CRC32_Calc(FILE* fp,unsigned long file_size,unsigned long* crc32_value)
{
	unsigned char read_buf[1025] = {0};
	int i,ret = 0;
	char pad_bytes=0;//1---3  不能整除时补0xFF
	
	if(NULL == fp)
		return -1;
	
	UINT32 CRC_Value = 0,Bytes_read =0;
			
	crc32_calc((const unsigned char*)read_buf, ret,1);//CRC初始化
	fseek(fp,0,SEEK_SET);
	do	
	{
		ret = fread(&read_buf, sizeof(UINT8), 1024, fp);
		if(ret > 0 )
		{
			if(ret == 1024)
			{
				CRC_Value = crc32_calc((const unsigned char*)read_buf,1024,0);
				Bytes_read  = Bytes_read + 1024;
			}else//last read
			{
				if(ret%4 == 0)
				{
					CRC_Value = crc32_calc((const unsigned char*)read_buf, ret,0);
					Bytes_read = Bytes_read + ret;
				}else
				{
					pad_bytes = 4- (ret%4);	
					for(i=0;i<pad_bytes;i++)
					{
						read_buf[ret + i]  = 0xFF;  	
					}
					CRC_Value = crc32_calc((const unsigned char*)read_buf, (ret + pad_bytes),0);
					Bytes_read = Bytes_read + ret + pad_bytes;
				}
			}
		}else//error
			break;
	}while(Bytes_read < file_size);
	
	//fclose(fp);
	if(Bytes_read >= file_size)
	{
		*crc32_value = CRC_Value;
		return 1;
	}else
		return 0;
}

unsigned char CheckUpgradeFile(void)
{
	FILE*  upgrade_fp = NULL;
	
	unsigned long firmware_size,crc32_value;
	
	upgrade_fp = fopen(F_UPDATE_EAST_DC_CTL  ,"rb");
	if(upgrade_fp != NULL)
	{
		fseek(upgrade_fp,0, SEEK_END);
		firmware_size = ftell(upgrade_fp);
		if((firmware_size > 30*1024)&&(firmware_size < 240*1024))//至少30K以上---认为有升级文件需升级
		{
			if(1 == File_CRC32_Calc(upgrade_fp,  firmware_size, &crc32_value ))
			{
				fclose(upgrade_fp);
				Globa->Control_Upgrade_Firmware_Info[0].Firmware_ALL_Bytes = firmware_size;				
				Globa->Control_Upgrade_Firmware_Info[0].Firware_CRC_Value = crc32_value;
				return 1;				
			}				
		}
	}
	return 0;//191017
}

//Group_Gun_No--主枪号
//Type 0--刷卡，1--VIN
void  Auxiliary_Gun_Data_Assignment(int Group_Gun_No,int Type)//辅助枪数据赋值		
{
	UINT8 Other_Group_Gun_No = Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun;
	if(Globa->Control_DC_Infor_Data[Other_Group_Gun_No].Double_Gun_To_Car_falg == 1)
	{
		Globa->Control_DC_Infor_Data[Other_Group_Gun_No].charge_mode = Globa->Control_DC_Infor_Data[Group_Gun_No].charge_mode;
		Globa->Control_DC_Infor_Data[Other_Group_Gun_No].qiyehao_flag = Globa->Control_DC_Infor_Data[Group_Gun_No].qiyehao_flag;
		Globa->Control_DC_Infor_Data[Other_Group_Gun_No].card_type = Globa->Control_DC_Infor_Data[Group_Gun_No].card_type;
		memcpy(&Globa->Control_DC_Infor_Data[Other_Group_Gun_No].card_sn[0], &Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn[0], sizeof(Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn));
		Globa->Control_DC_Infor_Data[Other_Group_Gun_No].last_rmb = Globa->Control_DC_Infor_Data[Group_Gun_No].last_rmb;
		Globa->Control_DC_Infor_Data[Other_Group_Gun_No].private_price_acquireOK = Globa->Control_DC_Infor_Data[Group_Gun_No].private_price_acquireOK;
		Globa->Control_DC_Infor_Data[Other_Group_Gun_No].charger_start_way = Globa->Control_DC_Infor_Data[Group_Gun_No].charger_start_way;
		if(Type == 0)
		{
			Globa->Control_DC_Infor_Data[Other_Group_Gun_No].MT625_Card_Start_ok_flag  = 1;
		}
		else
		{
			memcpy(Globa->Control_DC_Infor_Data[Other_Group_Gun_No].VIN_User_Id,Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_User_Id, 16);
			Globa->Control_DC_Infor_Data[Other_Group_Gun_No].VIN_auth_money = Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_money; 
			Globa->Control_DC_Infor_Data[Other_Group_Gun_No].VIN_start  = 1;
		}
	}
}

int Main_Task()
{
	int Ret=0;

	UINT32 count_wait=0,count_wait1 = 0,count_wait2 = 0,gun_config = 0,QT_gun_select = 0,count_wait_ID = 0; //从刷卡的时候，进入充电界面需要等待1s之后才判断是否退出;
	UINT32 i =0,j=0;

	UINT32 firmware_info_checkfile = 0; //检测升级文件是否存在
	UINT32 Upgrade_Firmware_time = 0;
	UINT32 ul_temp;
	UINT32 old_Updat_data_value = 0,oldSignals_value = 255;
	UINT32 total_rmb,total_kwh;//双枪充1个车时，2个枪加总的数据
	int old_have_hart = 255;
	int unlock_local = 0;//值1使用本地记录解锁，解锁后需清除锁卡标记
	UINT32 QT_Step = 255;
	UINT8 temp_char[256]; 
	UINT32  Aux_Gun_No = 0;//双枪同充副枪号0--n
	UINT32 Group_Gun_No = 0;//充电枪序号
	SEND_CARD_INFO send_card_info_data;//[4];
	time_t systime=0,log_time =0;
	struct tm tm_t,log_tm_t;
	time_t return_home_time=0;
	Globa->QT_Step = 0x01;//主界面

	msg.mtype = 0x100;//主界面;
	msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);

	VO_CLOCK_SIGNAL_H;//data cs clk要保持高电平
	VO_DATA_SIGNAL_H;
	VO_CHIP_SELECT_H;
	VO_RESET_L; //拉低复位信号
	sleep(1);
	VO_RESET_H;
	sleep(1);
	prctl(PR_SET_NAME,(unsigned long)"MainQT_Task");//设置线程名字
	Voicechip_operation_function(0xE6); //--调节声音的
	//发送二维码地址内容
	Page_90.flag = 1;//Globa->Charger_param.APP_enable;
	Page_90.gun_config =  Globa->Charger_param.gun_config;//1---N
	memcpy(&Page_90.sn[0], &Globa->Charger_param.SN[0], sizeof(Page_90.sn));
	snprintf(&Page_90.http[0], sizeof(Page_90.http), "%s", &Globa->Charger_param2.http[0]);
	msg.mtype = 0x90;
	memcpy(&msg.mtext[0], &Page_90.reserve1, sizeof(PAGE_90_FRAME));
	msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_90_FRAME), IPC_NOWAIT);
	sleep(6);//等待电表通信正常
	Globa->sys_restart_flag = 0;
    if((Globa->Charger_param.gun_config == 2)&&(Globa->Charger_param.support_double_gun_power_allocation ==1))
	{
		gun_config = 4;
	}else{
		gun_config = Globa->Charger_param.gun_config;//4;
	}
	
	for(i=0;i<Globa->Charger_param.gun_config;i++)//重启后下发一次
	{	
	  if(Globa->Charger_param.meter_config_flag == DCM3366Q)
		{
	   	 Dc_shunt_Set_meter_flag[i] = 1;	
		}		
		Dc_shunt_Set_CL_flag[i] = 1;							
	}
	
	while(1)
	{
		usleep(10000);//10ms
		Main_1_Watchdog = 1;

		if(QT_count_500ms_enable == 1)
		{//使能500MS 计时
			if(QT_count_500ms_flag != 1){//计时时间未到
				QT_count_500ms++;
				if(QT_count_500ms >= 50){
					QT_count_500ms_flag = 1;
				}
			}
		}

		//发送二维码地址内容
		if(Globa->have_APP_change1 == 1)
		{
			Globa->have_APP_change1 = 0;

			Page_90.flag = 1;//Globa->Charger_param.APP_enable;
			Page_90.gun_config =  Globa->Charger_param.gun_config;//1---N
			memcpy(&Page_90.sn[0], &Globa->Charger_param.SN[0], sizeof(Page_90.sn));
			snprintf(&Page_90.http[0], sizeof(Page_90.http), "%s", &Globa->Charger_param2.http[0]);
			msg.mtype = 0x90;
			memcpy(&msg.mtext[0], &Page_90.reserve1, sizeof(PAGE_90_FRAME));
			msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_90_FRAME), IPC_NOWAIT);
		}
		
		if((Globa->have_hart != old_have_hart)||
		(Globa->Updat_data_value != old_Updat_data_value)||
		(( oldSignals_value != Globa->Signals_value_No)&&(Globa->Charger_param.Serial_Network == 1)))
		{ //桩运行状态
			msg.mtype = 0x91;
			if(Globa->Software_Version_change == 1)
			{
				msg.mtext[0] = 2;
			}else
			{
			  msg.mtext[0] = Globa->have_hart;
			}
			msg.mtext[1] = Globa->Updat_data_value;
			msg.mtext[2] = Globa->Charger_param.Serial_Network;
			if(Globa->Charger_param.Serial_Network == 0)
			{
				msg.mtext[3] = 0;
			}else
			{
			  msg.mtext[3] = Globa->Signals_value_No;
			}
			msgsnd(Globa->arm_to_qt_msg_id, &msg, 4, IPC_NOWAIT);
			old_have_hart = Globa->have_hart ;
			oldSignals_value = Globa->Signals_value_No;
			old_Updat_data_value = Globa->Updat_data_value;
	    }
		
		msg.mtype = 0x00;//每次读之前初始化
		Ret = msgrcv(Globa->qt_to_arm_msg_id, &msg, sizeof(CHARGER_MSG)-sizeof(long), 0, IPC_NOWAIT);//接收QT发送过来的消息队列
		if(Ret >= 0)
		{
			printf("xxxxxx ret=%d,%0x,%0x\n", Ret, msg.mtype, msg.mtext[0]);
		}

    if( (Globa->Control_DC_Infor_Data[0].Error_system == 0)&&
		(Globa->Control_DC_Infor_Data[1].Error_system == 0)&&
		(Globa->Control_DC_Infor_Data[2].Error_system == 0)&&
		(Globa->Control_DC_Infor_Data[3].Error_system == 0)&&
		(Globa->Control_DC_Infor_Data[4].Error_system == 0)
		)
	{//没有系统故障
	  	GROUP1_FAULT_LED_OFF;
	}else
	{
		GROUP1_FAULT_LED_ON;
	}

		if(Globa->QT_Step != QT_Step)
		{
			QT_Step = Globa->QT_Step;
			printf("xxxxxx Globa->QT_Step %0x\n",Globa->QT_Step);
		}

		
		switch(Globa->QT_Step)
		{//判断 UI 所在界面
			case 0x01:
			{//---------------0 主界面 ( 初始 )状态  0.1  ------------------
				//该线程所用变量准备初始化
				QT_timer_stop(500);
				msg.mtype = 0x100;////add 181228---test
				msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);//add 181228---test
				Globa->QT_Step = 0x02; // 0 主界面 就绪( 可操作 )状态  0.2
				Group_Gun_No  = 0; 
				memset(&Globa->MUT100_Dz_Card_Info.IC_type, 0, sizeof(MUT100_DZ_CARD_INFO_DATA));

				if(firmware_info_checkfile == 0)
				{//上电第一次进行检测文件
					if(CheckUpgradeFile())
					{
						msg.mtype = 0x6000; //提示是否升级界面
						memset(&Page_6100.reserve1, 0, sizeof(PAGE_6100_FRAME));
						memcpy(&msg.mtext[0], &Page_6100.reserve1, sizeof(PAGE_6100_FRAME));
						msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_6100_FRAME), IPC_NOWAIT);
						Globa->QT_Step = 0x60;//0 主界面 ( 初始 )状态  0.1  	
						firmware_info_checkfile = 1;
					}
				}
				
				break;
			}
			case 0x02:{//---------------0 主界面 就绪( 可操作 )状态  0.2  ------------
				if(Globa->sys_restart_flag)
				{
					sleep(5);//wait等待响应报文发送
					system("sync");
					system("reboot");
				}
				//if(Globa->Electric_pile_workstart_Enable == 0)
                if (Globa->Electric_pile_workstart_Enable[0] == 0)//桩使能  // todo: 由邓工改一下枪禁用QT这边的处理
				{ //1//禁止本桩工作 
					printf("服务器远程禁用本桩!\n");
					msg.mtype = 0x5000; //系统忙界面
					msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
					Globa->QT_Step = 0x43; //主界面
					if(Globa->Charger_param.NEW_VOICE_Flag == 1){
					  Voicechip_operation_function(0x13);  //13H    “系统维护中，暂停使用 ”
					}
					break;
				}
				if( (Globa->Control_DC_Infor_Data[0].Metering_Step >= 0x02)||
					(Globa->Control_DC_Infor_Data[1].Metering_Step >= 0x02)||
					(Globa->Control_DC_Infor_Data[2].Metering_Step >= 0x02)||
					(Globa->Control_DC_Infor_Data[3].Metering_Step >= 0x02)||
					(Globa->Control_DC_Infor_Data[4].Metering_Step >= 0x02)
					)
				{
					Send_main_info_data(); //发送主要信息框架
					Globa->QT_Step = 0x03;
					count_wait = 0;	
					return_home_time = time(NULL);
					break;
				}
				
				if(msg.mtype == 0x100)
				{//判断是否为   主界面
					//printf("Globa->system_upgrade_busy=%d\n",Globa->system_upgrade_busy);
					if(0==Globa->system_upgrade_busy)//不在升级中才跳转
					{
						switch(msg.mtext[0])
						{//判断 在当前界面下 用户操作的按钮
							case 0x01:
							{//自动充电按钮---进入充电界面选择
								Send_main_info_data(); //发送主要信息框架
								Globa->QT_Step = 0x03;
								count_wait = 0;	
								count_wait2 = 0;
								return_home_time = time(NULL);
								break;
							}

							case 0x02:
							{//关于 按钮
								Page_2100.reserve1 = 1;
								memset(&Page_2100.reserve1, 0x00, sizeof(Page_2100));
								sprintf(&Page_2100.measure_ver[0], "%s", SOFTWARE_VER);
								
								Page_2100.gun_config = gun_config;//1---N
								sprintf(&Page_2100.contro_ver[0][0], "%s",  &Globa->contro_ver[0][0]);
								sprintf(&Page_2100.contro_ver[1][0], "%s",  &Globa->contro_ver[1][0]);
								sprintf(&Page_2100.contro_ver[2][0], "%s",  &Globa->contro_ver[2][0]);
								sprintf(&Page_2100.contro_ver[3][0], "%s",  &Globa->contro_ver[3][0]);
								sprintf(&Page_2100.contro_ver[4][0], "%s",  &Globa->contro_ver[4][0]);
								sprintf(&Page_2100.protocol_ver[0], "%s", PROTOCOL_VER);
								sprintf(&Page_2100.kernel_ver[0], "%s", &Globa->kernel_ver[0]);

								memcpy(Page_2100.sharp_rmb_kwh, &Globa->Charger_param2.share_time_kwh_price[0], 4);
								memcpy(Page_2100.peak_rmb_kwh,  &Globa->Charger_param2.share_time_kwh_price[1], 4);
								memcpy(Page_2100.flat_rmb_kwh,  &Globa->Charger_param2.share_time_kwh_price[2], 4);
								memcpy(Page_2100.valley_rmb_kwh,&Globa->Charger_param2.share_time_kwh_price[3], 4);
								
								Page_2100.Serv_Type = 3;//Globa->Charger_param.Serv_Type;
								Page_2100.run_state = Globa->have_hart;
								Page_2100.dispaly_Type = 1;//Globa->Charger_param2.show_price_type;  //金额显示类型， 0--详细显示金额+分时费    1--直接显示金额
								
								memcpy(Page_2100.service_price,&Globa->Charger_param2.share_time_kwh_price_serve[0], 4*4);
								memcpy(Page_2100.book_price,&Globa->Charger_param2.book_price, 4);
								memcpy(Page_2100.park_price,&Globa->Charger_param2.charge_stop_price.stop_price, 4);
								Page_2100.time_zone_num = Globa->Charger_param2.time_zone_num;  //时段数
								for(i=0;i<12;i++)
								{
									Page_2100.time_zone_tabel[i][0] = Globa->Charger_param2.time_zone_tabel[i]&0XFF;//时段表
									Page_2100.time_zone_tabel[i][1] = (Globa->Charger_param2.time_zone_tabel[i]>>8)&0XFF;//时段表
									Page_2100.time_zone_feilv[i] = Globa->Charger_param2.time_zone_feilv[i];//时段表位于的价格---表示对应的为尖峰平谷的值
								}
								memcpy(&msg.mtext[0], &Page_2100.reserve1, sizeof(Page_2100));
								msg.mtype = 0x2000;
								msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(Page_2100), IPC_NOWAIT);							
								Globa->QT_Step = 0x42;
								break;
							}
							case 0x03:
							{//系统管理按钮
								count_wait = 0;
								msg.mtype = 0x200;//等待刷卡界面
								memset(&Page_200.reserve1, '0', sizeof(PAGE_200_FRAME));
								Page_200.state = 10;//请刷管理员卡
								memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
								msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);

								Globa->QT_Step = 0x30; // 系统管理界面
								if(Globa->Charger_param.NEW_VOICE_Flag == 1){
								  Voicechip_operation_function(0x04);//04H     “请刷卡”	
								}
								else{
									Voicechip_operation_function(0x00);//00H	"欢迎光临.wav 300s 请刷卡1.wav"	
								}
								count_wait = 0;
								Init_Find_St(); //查询卡片
								break;
							}   
							case 0x04:
							{//帮助按钮
								msg.mtype = 0x4000;//帮助界面
								msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);

								Globa->QT_Step = 0x41; // 帮助界面

								break;
							}
							case 0x05:
							{//支付卡查询余额/解锁
								msg.mtype = 0x5300;//帮助界面
								msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
								Globa->QT_Step = 0x50; // 支付卡查询余额/解锁
								break;
							}
							
							default:
							{
								break;
							}
						}
					}
				}

				break;
			}
			case 0x03:
			{//--------------- 充电主界面（选择枪号） ------------------------
				if(msg.mtype == 0x120)
				{//收到改界面选择
					switch(msg.mtext[0])
					{//判断 在当前界面下 用户操作的按钮
						case 0x01:
						case 0x02:
						case 0x03:
						case 0x04:
						case 0x05:
						{//X#号枪
							Group_Gun_No = msg.mtext[0] - 1; //枪号 since 0
							count_wait = 0;
							if(Globa->Control_DC_Infor_Data[Group_Gun_No].charger_state == 0x03)
							{
								Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg = 0;
								Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Hostgun = 0;
								Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun = 0;
			
								if(Globa->Control_DC_Infor_Data[Group_Gun_No].Error_system == 0)
								{//没有系统故障
									Page_1000.link = (Globa->Control_DC_Infor_Data[Group_Gun_No].gun_link == 1)?1:0;  //1:连接  0：断开
									Page_1000.flag = 0;//发0，不显示VIN鉴权 //Globa->have_hart;
									Page_1000.gun_no = Group_Gun_No + 1;
									msg.mtype = 0x1000;
									memcpy(&msg.mtext[0], &Page_1000.reserve1, sizeof(PAGE_1000_FRAME));
									msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_1000_FRAME), IPC_NOWAIT);
									QT_timer_start(500);//需要刷新充电枪的状态
									Globa->QT_Step = 0x11; // 1 自动充电-》充电枪连接确认 状态  1.1
									if(Globa->Charger_param.NEW_VOICE_Flag == 1){
										Voicechip_operation_function(0x03);  //03H     “请插入充电枪，并确认连接状态 ”
									}else{
										Voicechip_operation_function(0x01);
									}
								}else if(Globa->Control_DC_Infor_Data[Group_Gun_No].Error.PE_Fault==1)
								{
									if(Globa->Charger_param.NEW_VOICE_Flag == 1){
										Voicechip_operation_function(0x16);//  16H    “系统接地异常，请联系设备管理员检查线路”	
									}else{
										Voicechip_operation_function(0x03);
									}
								}else if((Globa->Control_DC_Infor_Data[Group_Gun_No].Error.input_over_protect==1)||(Globa->Control_DC_Infor_Data[Group_Gun_No].Error.input_lown_limit==1))
								{
									 if(Globa->Charger_param.NEW_VOICE_Flag == 1){
										Voicechip_operation_function(0x15); //15H    “交流电压输入异常，请联系设备管理员检查线路”
									}else{
										Voicechip_operation_function(0x03);
									}	
								}else
								{
									if(Globa->Charger_param.NEW_VOICE_Flag == 1){
										Voicechip_operation_function(0x14);//  14H    “系统故障，暂停使用 ”
									}else{
										Voicechip_operation_function(0x03);
									}	
								}
								break;
							}else  if(Globa->Control_DC_Infor_Data[Group_Gun_No].charger_state == 0x04)
							{ //充电中
								count_wait = 0;
								count_wait1 = 0;
								count_wait2 = 0;
								Send_charge_info_data(Group_Gun_No);//充电界面
								Globa->QT_Step = 0x21;  
								break;
							}else if(Globa->Control_DC_Infor_Data[Group_Gun_No].charger_state == 0x05 )
							{ //充电完成
								Page_1300.cause = Globa->Control_DC_Infor_Data[Group_Gun_No].charge_end_reason;//
								Page_1300.Capact = Globa->Control_DC_Infor_Data[Group_Gun_No].BMS_batt_SOC;
								Page_1300.elapsed_time[0] =  Globa->Control_DC_Infor_Data[Group_Gun_No].Time;
								Page_1300.elapsed_time[1] =  Globa->Control_DC_Infor_Data[Group_Gun_No].Time>>8;
								
								//memcpy(Page_1300.KWH,&Globa->Control_DC_Infor_Data[Group_Gun_No].kwh, 4);	
								memcpy(Page_1300.KWH,&Globa->Control_DC_Infor_Data[Group_Gun_No].soft_KWH, 4);//0.001度	
								memcpy(Page_1300.RMB,&Globa->Control_DC_Infor_Data[Group_Gun_No].total_rmb, 4);
								
								memcpy(&Page_1300.SN[0], &Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn[0], sizeof(Page_1300.SN));
								//Page_1300.charger_start_way = 1;   //充电启动方式 1:刷卡  2：手机APP 101:VIN
								Page_1300.charger_start_way = Globa->Control_DC_Infor_Data[Group_Gun_No].charger_start_way + 1;   //充电启动方式 1:刷卡  2：手机APP 101:VIN
								Page_1300.Gun_no = Group_Gun_No;
								
								////////////////////////////////////////////////////////////////////////////////
								memset(Page_1300.start_time,0,sizeof(Page_1300.start_time));
								memcpy(Page_1300.start_time,&Globa->Control_DC_Infor_Data[Group_Gun_No].charge_start_time[11],8);
								memset(Page_1300.end_time,0,sizeof(Page_1300.end_time));
								memcpy(Page_1300.end_time,&Globa->Control_DC_Infor_Data[Group_Gun_No].charge_end_time[11],8);								
								Page_1300.Double_Gun_To_Car_falg = Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg;
								if(1 == Page_1300.Double_Gun_To_Car_falg)
								{
									total_rmb = Globa->Control_DC_Infor_Data[Group_Gun_No].total_rmb + Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].total_rmb;
									//total_kwh = Globa->Control_DC_Infor_Data[Group_Gun_No].kwh + Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].kwh;
									total_kwh = Globa->Control_DC_Infor_Data[Group_Gun_No].soft_KWH + Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].soft_KWH;//0.001kwh
									memcpy(Page_1300.KWH,&total_kwh, 4);								
									memcpy(Page_1300.RMB,&total_rmb, 4);								
								}
								/////////////////////////////////////////////////////////////////////////////////
								
								memcpy(&msg.mtext[0], &Page_1300.reserve1, sizeof(PAGE_1300_FRAME));

								msg.mtype = 0x1310;//单枪结束界面
								msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_1300_FRAME), IPC_NOWAIT);
								Globa->QT_Step = 0x21;
								count_wait1 = 201;//触发立即发结束信息
								break;
							}else if(Globa->Control_DC_Infor_Data[Group_Gun_No].charger_state == 0x06 )
							{ //预约
								Page_daojishi.hour = Globa->Control_DC_Infor_Data[Group_Gun_No].remain_hour;		
								Page_daojishi.min  = Globa->Control_DC_Infor_Data[Group_Gun_No].remain_min;
								Page_daojishi.second = Globa->Control_DC_Infor_Data[Group_Gun_No].remain_second;
								Page_daojishi.Button_Show_flag = 0;
								Page_daojishi.Gun_no = Group_Gun_No;
								memcpy(&Page_daojishi.card_sn[0], &Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn[0], sizeof(Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn));
								memcpy(&msg.mtext[0], &Page_daojishi.reserve1, sizeof(PAGE_DAOJISHI_FRAME));
								msg.mtype = 0x1151;//倒计时界面
								msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_DAOJISHI_FRAME), IPC_NOWAIT);
								Globa->QT_Step = 0x202; //预约界面
								break;
							}
						  break;
					  }
						case 0xE1:{//双枪对一辆车充电选择界面
								if(2 == Globa->Charger_param.gun_config)
								{
									if(	(1 == Globa->Control_DC_Infor_Data[0].gun_link)&&
										(1 == Globa->Control_DC_Infor_Data[1].gun_link)&&
										(3 == Globa->Control_DC_Infor_Data[0].charger_state)&&
										(3 == Globa->Control_DC_Infor_Data[1].charger_state)&&
										(0 == Globa->Control_DC_Infor_Data[0].Error_system)&&
										(0 == Globa->Control_DC_Infor_Data[1].Error_system))
										{
										}
										else//不能启动双枪同充									
											break;								
								}
								memset(&Page_1E1.reserve1,0,sizeof(Page_1E1));
								Page_1E1.Gun_Number = Globa->Charger_param.gun_config;
								Page_1E1.On_link = Globa->have_hart;
								for(i =0;i<Globa->Charger_param.gun_config;i++)
								{
									Page_1E1.Gun_link[i] = Globa->Control_DC_Infor_Data[i].gun_link;
									Page_1E1.Gun_State[i] = (Globa->Control_DC_Infor_Data[i].charger_state == 3)? 1:0; //只有空闲的时候才可以用
								  Page_1E1.Error_system[i] = (Globa->Control_DC_Infor_Data[i].Error_system == 0)? 0:1;
								}
								msg.mtype = 0x1E1;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选择界面，才有此按钮功能）
								memcpy(&msg.mtext[0], &Page_1E1.reserve1, sizeof(PAGE_1E1_FRAME));
								msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_1E1_FRAME), IPC_NOWAIT);
								Globa->QT_Step = 0x04; //双枪界面选择
								count_wait = 0;
							
							break;
						}
  				  case 0xEE:{//退出按钮
					  	msg.mtype = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选择界面，才有此按钮功能）
					  	msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa->QT_Step = 0x02; //主界面
						count_wait = 0;
  					  break;
					}
						
						default:
						{
								break;
						}
				  }
				}
				count_wait++;
				if(count_wait > 200)
				{//等待时间超时2S
					count_wait = 0;
					Send_main_info_data();
					memcpy(&msg.mtext[0], &Page_110_Info.reserve1, sizeof(Page_110_Info));
					msg.mtype = 0x120;
					msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(Page_110_Info), IPC_NOWAIT);
				}
				if( (Globa->Control_DC_Infor_Data[0].Metering_Step >= 0x02)||
					(Globa->Control_DC_Infor_Data[1].Metering_Step >= 0x02)||
					(Globa->Control_DC_Infor_Data[2].Metering_Step >= 0x02)||
					(Globa->Control_DC_Infor_Data[3].Metering_Step >= 0x02)||
					(Globa->Control_DC_Infor_Data[4].Metering_Step >= 0x02)
					)
				{
					return_home_time = time(NULL);//reset timer
				}
				else//60秒后自动返回首页
				{
					if(0x03 == Globa->QT_Step)
					{
						if((time(NULL)-return_home_time)>60)//返回首页
						{
							msg.mtype = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选择界面，才有此按钮功能）
							msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa->QT_Step = 0x02; //主界面
							count_wait = 0;
						}
					}					
				}
			  break;
			}
			
			case 0x04://双枪界面选择
			{
			  if(msg.mtype == 0x1E1)
				{//收到改界面选择
					switch(msg.mtext[0])
					{
						case 0x01://确定按钮--刷卡启动
						{//msg.mtext[1]--主枪 msg.mtext[2]--辅枪 msg.mtext[3]--选择辅助电源
							Group_Gun_No = msg.mtext[1] - 1; //主枪号 since 0
							Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg = 1;
							Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Hostgun = 1;
							Aux_Gun_No = msg.mtext[2] - 1 ;//副枪号
							Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun = Aux_Gun_No;//副枪号
							Globa->Control_DC_Infor_Data[Aux_Gun_No].Double_Gun_To_Car_falg = 1;
							Globa->Control_DC_Infor_Data[Aux_Gun_No].Double_Gun_To_Car_Hostgun = 0;
							Globa->Control_DC_Infor_Data[Aux_Gun_No].Double_Gun_To_Car_Othergun = Group_Gun_No;
							printf("-----Group_Gun_No = %d msg.mtext[1] =%d\n",Group_Gun_No,msg.mtext[1]);
							printf("-----Double_Gun_To_Car_Hostgun = %d  Double_Gun_To_Car_Othergun  = %d msg.mtext[1] =%d \n",Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Hostgun,Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun,msg.mtext[2]);
							printf("-----Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun  = %d   =%d\n",Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun,Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Othergun);
													
							printf("----2- = %d [1] =%d\n",Globa->Control_DC_Infor_Data[0].Double_Gun_To_Car_falg,Globa->Control_DC_Infor_Data[1].Double_Gun_To_Car_falg);
							printf("---23-- = %d [1] =%d\n",Globa->Control_DC_Infor_Data[0].Double_Gun_To_Car_Hostgun,Globa->Control_DC_Infor_Data[1].Double_Gun_To_Car_Hostgun);
							printf("---234-- = %d [1] =%d\n",Globa->Control_DC_Infor_Data[0].Double_Gun_To_Car_Othergun,Globa->Control_DC_Infor_Data[1].Double_Gun_To_Car_Othergun);

							count_wait = 0;
							Globa->Control_DC_Infor_Data[Group_Gun_No].BMS_Power_V = msg.mtext[3];
							if(Globa->Control_DC_Infor_Data[Group_Gun_No].BMS_Power_V == 1)
							{
								if(Group_Gun_No == 0){
									GROUP1_BMS_Power24V_ON;
								}else if(Group_Gun_No == 1){
									GROUP2_BMS_Power24V_ON;
								}else if(Group_Gun_No == 2){
									GROUP3_BMS_Power24V_ON;
								}else if(Group_Gun_No == 3){
									GROUP4_BMS_Power24V_ON;
								}
							}else
							{
								if(Group_Gun_No == 0){
									GROUP1_BMS_Power24V_OFF;
								}else if(Group_Gun_No == 1){
									GROUP2_BMS_Power24V_OFF;
								}else if(Group_Gun_No == 2){
									GROUP3_BMS_Power24V_OFF;
								}else if(Group_Gun_No == 3){
									GROUP4_BMS_Power24V_OFF;
								}
							}
							
							//直接自动启动
							Globa->Control_DC_Infor_Data[Group_Gun_No].charge_mode = AUTO_MODE;
							msg.mtype = 0x200;//等待刷卡界面
							memset(&Page_200.reserve1, '0', sizeof(PAGE_200_FRAME));
							Page_200.state = 1;//1 没有检测到用户卡
							memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
								
							memset(&Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn[0], 0, sizeof(Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn));

							if(Globa->Charger_param.NEW_VOICE_Flag == 1)
							{
								Voicechip_operation_function(0x04);//04H     “请刷卡”
							}else
							{
								Voicechip_operation_function(0x00);//00H	"欢迎光临.wav 300s 请刷卡1.wav"	
							}
							Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_check_flag = 0;
							Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_check_result = 0;
							Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_Present = 0;
							Globa->QT_Step = 0x20; // 2 自动充电-等待刷卡界面  2.
							count_wait = 0;
							Init_Find_St(); //查询卡片							
							break;
						}
						case 0x02://VIN START
						{			
							Group_Gun_No = msg.mtext[1] - 1; //枪号 since 0
						//	Group_Other_Gun_No = msg.mtext[2] - 1 ;
							Aux_Gun_No = msg.mtext[2] - 1 ;//副枪号
							Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg = 1;
							Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Hostgun = 1;							
							Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun = Aux_Gun_No;
							Globa->Control_DC_Infor_Data[Aux_Gun_No].Double_Gun_To_Car_falg = 1;
							Globa->Control_DC_Infor_Data[Aux_Gun_No].Double_Gun_To_Car_Hostgun = 0;
							Globa->Control_DC_Infor_Data[Aux_Gun_No].Double_Gun_To_Car_Othergun = Group_Gun_No;
							count_wait = 0;
							Globa->Control_DC_Infor_Data[Group_Gun_No].BMS_Power_V = msg.mtext[3];
							if(Globa->Control_DC_Infor_Data[Group_Gun_No].BMS_Power_V == 1)
							{
								if(Group_Gun_No == 0){
									GROUP1_BMS_Power24V_ON;
								}else if(Group_Gun_No == 1){
									GROUP2_BMS_Power24V_ON;
								}else if(Group_Gun_No == 2){
									GROUP3_BMS_Power24V_ON;
								}else if(Group_Gun_No == 3){
									GROUP4_BMS_Power24V_ON;
								}
							}
							else
							{
								if(Group_Gun_No == 0){
									GROUP1_BMS_Power24V_OFF;
								}else if(Group_Gun_No == 1){
									GROUP2_BMS_Power24V_OFF;
								}else if(Group_Gun_No == 2){
									GROUP3_BMS_Power24V_OFF;
								}else if(Group_Gun_No == 3){
									GROUP4_BMS_Power24V_OFF;
								}
							}						
							//先启动控制板按VIN鉴权方式
							Globa->Control_DC_Infor_Data[Group_Gun_No].charge_mode = AUTO_MODE;
							msg.mtype = 0x400;//VIN鉴权等待界面
							memset(&Page_400.reserve1, 0, sizeof(PAGE_400_FRAME));
							
							Page_400.VIN_auth_step = 0;//等待获取车辆VIN
							
							memcpy(&msg.mtext[0], &Page_400.reserve1, sizeof(PAGE_400_FRAME));
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_400_FRAME), IPC_NOWAIT);
							
							Globa->Control_DC_Infor_Data[Group_Gun_No].BMS_Info_falg = 0;
							
							memset(Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_User_Id,'0',16);//先设置为全'0'//
							Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_start = 1;//start
							count_wait = 0;
							//等待控制板的VIN标记返回，超时时间1分钟
							do{
								sleep(1);//阻塞等待===是否可以
								count_wait++;
								if(count_wait > 59)
									break;
								if(Globa->Control_DC_Infor_Data[Group_Gun_No].charger_over_flag > 0)//控制板已结束
									break;
								if(Globa->Control_DC_Infor_Data[Group_Gun_No].Error.emergency_switch )
									break;								
							}while(0==Globa->Control_DC_Infor_Data[Group_Gun_No].BMS_Info_falg );
							count_wait = 0;
							//test===
							// memcpy(Globa->Control_DC_Infor_Data[Group_Gun_No].VIN,"12345678912345678",17);//test							
							// Globa->Control_DC_Infor_Data[Group_Gun_No].BMS_Info_falg = 1;
							if(Globa->Control_DC_Infor_Data[Group_Gun_No].Error.emergency_switch )
							{
								Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_start = 7;
								sleep(1);//show auth result
								//返回首页
								Send_main_info_data(); //发送主要信息框架
								Globa->QT_Step = 0x03;
								count_wait = 0;
						
							  Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_falg = 0;
							  Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Hostgun = 0;
							  Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Othergun = 0;
							  Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg = 0;
							  Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Hostgun = 0;
							  Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun = 0;
							  break;
							}
							//1分钟内收到VIN后启动鉴权，等待超时1分钟
							if(Globa->Control_DC_Infor_Data[Group_Gun_No].BMS_Info_falg)//get VIN
							{
								Page_400.VIN_auth_step = 1;//发送VIN鉴权请求等待响应
								for(i=0;i<17;i++)
									Page_400.VIN[i] = Globa->Control_DC_Infor_Data[Group_Gun_No].VIN[i];
								Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_ack = 0;
								Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_failed_reason=0;
								Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_money = 0;
								Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_success=0;
								Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_req = 1;//请求鉴权
								
								memcpy(&msg.mtext[0], &Page_400.reserve1, sizeof(PAGE_400_FRAME));
								msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_400_FRAME), IPC_NOWAIT);
								
								count_wait = 0;
								//等待服务器返回VIN鉴权，超时时间1分钟
								do{
									sleep(1);//阻塞等待===是否可以
									count_wait++;
									if(count_wait > 10)//59)
										break;
									if(Globa->Control_DC_Infor_Data[Group_Gun_No].Error.emergency_switch )
										break;
								}while(0==Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_ack );
								count_wait = 0;
								Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_req = 0;//clear
								if(Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_ack)
								{
									Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_ack = 0;
									Page_400.VIN_auth_step = 2;//获取到VIN鉴权响应信息
									Page_400.VIN_auth_success = Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_success;
									Page_400.VIN_auth_failed_reason[0] = Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_failed_reason;
									Page_400.VIN_auth_failed_reason[1] = Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_failed_reason>>8;
									
									memcpy(Page_400.VIN_auth_money,&Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_money,4);
									
									memcpy(&msg.mtext[0], &Page_400.reserve1, sizeof(PAGE_400_FRAME));
									msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_400_FRAME), IPC_NOWAIT);
									
									if(Page_400.VIN_auth_success)
									{
										//if(Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_money >= MIN_USER_MONEY_NEED)//目前后台没有VIN鉴权余额，不判断
										{
											//进入自动充电
											sleep(2);//show auth result
											count_wait2 = 0;//sleep(30);//test delay
											Send_charge_info_data(Group_Gun_No);//充电界面
											Globa->QT_Step = 0x21; 
											Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_start = 2;//go
											memcpy(Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn,Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_User_Id, 16);
											if(Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg == 1)
											{
												Auxiliary_Gun_Data_Assignment(Group_Gun_No,1);//辅助枪数据赋值--VIN
											}
											count_wait_ID = 0; //从刷卡的时候，进入充电界面需要等待1s之后才判断是否退出
										}
									}
									else//鉴权失败
									{
										Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_start = 4;
										sleep(5);//show auth result
										//返回首页
										Send_main_info_data(); //发送主要信息框架
										Globa->QT_Step = 0x03;
										count_wait = 0;	
										Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_falg = 0;
										Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Hostgun = 0;
										Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Othergun = 0;
										Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg = 0;
										Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Hostgun = 0;
										Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun = 0;									
									}
									
								}else//VIN鉴权响应超时
								{
									Page_400.VIN_auth_step = 3;//VIN鉴权响应超时
									Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_start = 5;
									memcpy(&msg.mtext[0], &Page_400.reserve1, sizeof(PAGE_400_FRAME));
									msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_400_FRAME), IPC_NOWAIT);
									Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_falg = 0;
									Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Hostgun = 0;
									Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Othergun = 0;
									Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg = 0;
									Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Hostgun = 0;
									Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun = 0;
									sleep(5);//show auth result
									//返回首页
									Send_main_info_data(); //发送主要信息框架
									Globa->QT_Step = 0x03;
									count_wait = 0;								
								}
							}else//控制板返回VIN超时
							{
								Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_falg = 0;
								Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Hostgun = 0;
								Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Othergun = 0;
								Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg = 0;
								Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Hostgun = 0;
								Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun = 0;
								Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_start = 6;
								sleep(5);//show auth result
								//返回首页
								Send_main_info_data(); //发送主要信息框架
								Globa->QT_Step = 0x03;
								count_wait = 0;
							}
						 
						  break;
						}
						case 0xEE://退出按钮
						{
							Send_main_info_data(); //发送主要信息框架
							Globa->QT_Step = 0x03;
							count_wait = 0;	
							return_home_time = time(NULL);
							break;
						} 
					}
			  }else{
					count_wait ++ ;
					if( count_wait >= 100 )
					{
						count_wait = 0;
						memset(&Page_1E1.reserve1,0,sizeof(Page_1E1));
						Page_1E1.Gun_Number = Globa->Charger_param.gun_config;
						Page_1E1.On_link = Globa->have_hart;
						for(i =0;i<Globa->Charger_param.gun_config;i++)
						{
							Page_1E1.Gun_link[i] = Globa->Control_DC_Infor_Data[i].gun_link;
							Page_1E1.Gun_State[i] = (Globa->Control_DC_Infor_Data[i].charger_state == 3)? 1:0; //只有空闲的时候才可以用
							Page_1E1.Error_system[i] = (Globa->Control_DC_Infor_Data[i].Error_system == 0)? 0:1;
						}
						msg.mtype = 0x1E1;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选择界面，才有此按钮功能）
						memcpy(&msg.mtext[0], &Page_1E1.reserve1, sizeof(PAGE_1E1_FRAME));
						msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_1E1_FRAME), IPC_NOWAIT);
					 }
				}
				break;
			}
			case 0x11:
			{//---------------1 自动充电-》充电枪连接确认 状态  1.1---------
				if(msg.mtype == 0x1000)
				{//收到该界面消息
					switch(msg.mtext[0])
					{//判断 在当前界面下 用户操作的按钮
						case 0x01:
						{//确认按钮
							//根据充电枪的连接状态在进行判断处理
							Globa->Control_DC_Infor_Data[Group_Gun_No].BMS_Power_V = msg.mtext[1];
							if(Globa->Control_DC_Infor_Data[Group_Gun_No].BMS_Power_V == 1)
							{
								if(Group_Gun_No == 0){
									GROUP1_BMS_Power24V_ON;
								}else if(Group_Gun_No == 1){
									GROUP2_BMS_Power24V_ON;
								}else if(Group_Gun_No == 2){
									GROUP3_BMS_Power24V_ON;
								}else if(Group_Gun_No == 3){
									GROUP4_BMS_Power24V_ON;
								}
							}else
							{
								if(Group_Gun_No == 0){
									GROUP1_BMS_Power24V_OFF;
								}else if(Group_Gun_No == 1){
									GROUP2_BMS_Power24V_OFF;
								}else if(Group_Gun_No == 2){
									GROUP3_BMS_Power24V_OFF;
								}else if(Group_Gun_No == 3){
									GROUP4_BMS_Power24V_OFF;
								}
							}
								
							msg.mtype = 0x1100;// 充电方式选择界面
							msgsnd(Globa->arm_to_qt_msg_id, &msg, 2, IPC_NOWAIT);//发送两个字节
							Globa->QT_Step = 0x121; // 1 自动充电-》充电方式选择 状态  1.2.1 	
							if(Globa->Charger_param.NEW_VOICE_Flag == 1)
							{
							  Voicechip_operation_function(0x02);  //02H"    “请选择充电方式"
							}
							QT_timer_stop(500);
							break;
						}
						case 0x02:
						{//退出按钮
							Send_main_info_data(); //发送主要信息框架
							Globa->QT_Step = 0x03;
							count_wait = 0;
							break;
						}
						case 0x03://VIN START
						{							
							Globa->Control_DC_Infor_Data[Group_Gun_No].BMS_Power_V = msg.mtext[1];
							if(Globa->Control_DC_Infor_Data[Group_Gun_No].BMS_Power_V == 1)
							{
								if(Group_Gun_No == 0){
									GROUP1_BMS_Power24V_ON;
								}else if(Group_Gun_No == 1){
									GROUP2_BMS_Power24V_ON;
								}else if(Group_Gun_No == 2){
									GROUP3_BMS_Power24V_ON;
								}else if(Group_Gun_No == 3){
									GROUP4_BMS_Power24V_ON;
								}
							}else
							{
								if(Group_Gun_No == 0){
									GROUP1_BMS_Power24V_OFF;
								}else if(Group_Gun_No == 1){
									GROUP2_BMS_Power24V_OFF;
								}else if(Group_Gun_No == 2){
									GROUP3_BMS_Power24V_OFF;
								}else if(Group_Gun_No == 3){
									GROUP4_BMS_Power24V_OFF;
								}
							}
							
							//先启动控制板按VIN鉴权方式
							Globa->Control_DC_Infor_Data[Group_Gun_No].charge_mode = AUTO_MODE;
							msg.mtype = 0x400;//VIN鉴权等待界面
							memset(&Page_400.reserve1, 0, sizeof(PAGE_400_FRAME));
							
							Page_400.VIN_auth_step = 0;//等待获取车辆VIN
							
							memcpy(&msg.mtext[0], &Page_400.reserve1, sizeof(PAGE_400_FRAME));
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_400_FRAME), IPC_NOWAIT);
							
							Globa->Control_DC_Infor_Data[Group_Gun_No].BMS_Info_falg = 0;
							
							memset(Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_User_Id,'0',16);//先设置为全'0'//
							Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_Judgment_Result = 0;//鉴权中
							Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_start = 1;//start

							count_wait = 0;
							//等待控制板的VIN标记返回，超时时间1分钟
							do{
								sleep(1);//阻塞等待===是否可以
								count_wait++;
								if(count_wait > 59)
									break;
								if(Globa->Control_DC_Infor_Data[Group_Gun_No].charger_over_flag > 0)//控制板已结束
									break;
								if(Globa->Control_DC_Infor_Data[Group_Gun_No].Error.emergency_switch )
									break;
							}while(0==Globa->Control_DC_Infor_Data[Group_Gun_No].BMS_Info_falg );
							count_wait = 0;
							//test===
							// memcpy(Globa->Control_DC_Infor_Data[Group_Gun_No].VIN,"12345678912345678",17);//test							
							// Globa->Control_DC_Infor_Data[Group_Gun_No].BMS_Info_falg = 1;
							if(Globa->Control_DC_Infor_Data[Group_Gun_No].Error.emergency_switch )
							{
								Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_start = 7;
								sleep(1);//show auth result
								//返回首页
								Send_main_info_data(); //发送主要信息框架
								Globa->QT_Step = 0x03;
								count_wait = 0;
								break;
							}
							//1分钟内收到VIN后启动鉴权，等待超时1分钟
							if(Globa->Control_DC_Infor_Data[Group_Gun_No].BMS_Info_falg)//get VIN
							{
								Page_400.VIN_auth_step = 1;//发送VIN鉴权请求等待响应
								for(i=0;i<17;i++)
									Page_400.VIN[i] = Globa->Control_DC_Infor_Data[Group_Gun_No].VIN[i];
								Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_ack = 0;
								Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_failed_reason=0;
								Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_money = 0;
								Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_success=0;
								Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_req = 1;//请求鉴权
								
								Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_Judgment_Result = 0;//鉴权中
								
								memcpy(&msg.mtext[0], &Page_400.reserve1, sizeof(PAGE_400_FRAME));
								msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_400_FRAME), IPC_NOWAIT);
								
								count_wait = 0;
								//等待服务器返回VIN鉴权，超时时间1分钟
								do{
									sleep(1);//阻塞等待===是否可以
									count_wait++;
									if(count_wait > 10)//59)
										break;
									if(Globa->Control_DC_Infor_Data[Group_Gun_No].Error.emergency_switch )
										break;
								}while(0==Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_ack );
								count_wait = 0;
								Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_req = 0;//clear
								if(Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_ack)
								{
									Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_ack = 0;
									Page_400.VIN_auth_step = 2;//获取到VIN鉴权响应信息
									Page_400.VIN_auth_success = Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_success;
									Page_400.VIN_auth_failed_reason[0] = Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_failed_reason;
									Page_400.VIN_auth_failed_reason[1] = Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_failed_reason>>8;
									
									memcpy(Page_400.VIN_auth_money,&Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_money,4);
									
									memcpy(&msg.mtext[0], &Page_400.reserve1, sizeof(PAGE_400_FRAME));
									msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_400_FRAME), IPC_NOWAIT);
									
									if(Page_400.VIN_auth_success)
									{
										//if(Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_auth_money >= MIN_USER_MONEY_NEED)//目前后台没有VIN鉴权余额，不判断
										{
											//进入自动充电
											Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_Judgment_Result = 1;//鉴权成功										
											sleep(2);//show auth result
											count_wait2 = 0;//sleep(30);//test delay
											Send_charge_info_data(Group_Gun_No);//充电界面
											Globa->QT_Step = 0x21; 
											memcpy(Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn,Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_User_Id, 16);
									    Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_start = 2;//go
											count_wait_ID = 0; //从刷卡的时候，进入充电界面需要等待1s之后才判断是否退出
										}
										// else//余额不足，请充值”OK
										// {
											// if(Globa->Charger_param.NEW_VOICE_Flag == 1)
											// {
											  // Voicechip_operation_function(0x0a);  //0aH 余额不足，请充值”OK
											// }
											// Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_start = 3;
											// //返回首页
											// Send_main_info_data(); //发送主要信息框架
											// Globa->QT_Step = 0x03;
											// count_wait = 0;
										// }
									}
									else//鉴权失败
									{
										Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_start = 4;
										Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_Judgment_Result = 2;//鉴权失败									
										sleep(5);//show auth result
										//返回首页
										Send_main_info_data(); //发送主要信息框架
										Globa->QT_Step = 0x03;
										count_wait = 0;										
									}
								}else//VIN鉴权响应超时
								{
									Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_Judgment_Result = 2;//鉴权失败									
									Page_400.VIN_auth_step = 3;//VIN鉴权响应超时
									Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_start = 5;
									memcpy(&msg.mtext[0], &Page_400.reserve1, sizeof(PAGE_400_FRAME));
									msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_400_FRAME), IPC_NOWAIT);
									
									sleep(5);//show auth result
									//返回首页
									Send_main_info_data(); //发送主要信息框架
									Globa->QT_Step = 0x03;
									count_wait = 0;								
								}
								
							}else//控制板返回VIN超时
							{
								Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_start = 6;
								sleep(5);//show auth result
								//返回首页
								Send_main_info_data(); //发送主要信息框架
								Globa->QT_Step = 0x03;
								count_wait = 0;
								
							}
							
						}
						break;
						
						default:
						{
							break;
						}
					}
				}else if(QT_timer_tick(500) == 1)
				{  //定时500MS
					Page_1000.link = (Globa->Control_DC_Infor_Data[Group_Gun_No].gun_link == 1)?1:0;  //1:连接  0：断开
					Page_1000.flag = Globa->have_hart;
					Page_1000.gun_no = Group_Gun_No + 1;
					msg.mtype = 0x1000;
					memcpy(&msg.mtext[0], &Page_1000.reserve1, sizeof(PAGE_1000_FRAME));
					msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_1000_FRAME), IPC_NOWAIT);
					QT_timer_start(500);
				}			
				//等待点击VIN或刷卡或返回按钮期间如果扫码启动了，需跳转界面
				if(	Globa->Control_DC_Infor_Data[Group_Gun_No].App.APP_start)//本枪扫码启动了
				{
					Send_main_info_data(); //发送主要信息框架
					Globa->QT_Step = 0x03;
					count_wait = 0;
				}
				
				break;
			}
			
			case 0x121:
			{//--------------1 自动充电-》 充电方式选择 状态  1.2.1--------
				if(msg.mtype == 0x1100)
				{//收到该界面消息
					switch(msg.mtext[0])
					{//判断 在当前界面下 用户操作的按钮
						case 0x01:
						{//自动充电 按钮
							Globa->Control_DC_Infor_Data[Group_Gun_No].charge_mode = AUTO_MODE;
							msg.mtype = 0x200;//等待刷卡界面
							memset(&Page_200.reserve1, '0', sizeof(PAGE_200_FRAME));
							Page_200.state = 1;//1 没有检测到用户卡
							memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
								
							memset(&Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn[0], 0, sizeof(Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn));

							if(Globa->Charger_param.NEW_VOICE_Flag == 1)
							{
								Voicechip_operation_function(0x04);//04H     “请刷卡”
							}else
							{
								Voicechip_operation_function(0x00);//00H	"欢迎光临.wav 300s 请刷卡1.wav"	
							}
							Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_check_flag = 0;
							Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_check_result = 0;
							Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_Present = 0;
							Globa->QT_Step = 0x20; // 2 自动充电-等待刷卡界面  2.
							count_wait = 0;
							Init_Find_St(); //查询卡片
							break;
						}
						case 0x02:
						{//按金额充电 按钮
							msg.mtype = 0x1120; // 按金额充电界面
							msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa->QT_Step = 0x131; // 1 自动充电-》 输入 状态  1.3.1---

							break;
						}
						case 0x03:
						{//按电量充电 按钮
							msg.mtype = 0x1130; // 按电量充电界面
							msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa->QT_Step = 0x131; // 1 自动充电-》 输入 状态  1.3.1---

							break;
						}
						case 0x04:
						{//按时间充电 按钮
							msg.mtype = 0x1140; // 按时间充电界面
							msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa->QT_Step = 0x131; // 1 自动充电-》 输入 状态  1.3.1---

							break;
						}
						case 0x05:
						{//预约充电 按钮
							msg.mtype = 0x1150; // 预约充电界面
							msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa->QT_Step = 0x131;//1 自动充电-》 输入 状态  1.3.1---
							break;
						}
						case 0x07:
						{//退出按钮
							Send_main_info_data(); //发送主要信息框架
							Globa->QT_Step = 0x03;
							count_wait = 0;
							break;
						}
						default:
						{
							break;
						}
					}
				}
				break;
			}
			case 0x131:
			{//--------------1 自动充电-》 输入 状态  1.3.1----------------
				if( (msg.mtype == 0x1120)||
					(msg.mtype == 0x1130)||
					(msg.mtype == 0x1140)||
					(msg.mtype == 0x1150))
				{//收到该界面消息
					switch(msg.mtext[0])
					{//判断 在当前界面下 用户操作的按钮
						case 0x01:
						{//确认按钮
							//根据不同的充电方式 输入界面 分别处理输入的数据
							if(msg.mtype == 0x1120)
							{//按金额充电
								Globa->Control_DC_Infor_Data[Group_Gun_No].charge_money_limit = ((msg.mtext[2]<<8) + msg.mtext[1])*100;//0.01元
								Globa->Control_DC_Infor_Data[Group_Gun_No].charge_mode = MONEY_MODE;
							}else if(msg.mtype == 0x1130)
							{//按电量充电
								Globa->Control_DC_Infor_Data[Group_Gun_No].charge_kwh_limit =   ((msg.mtext[2]<<8) + msg.mtext[1])*100;//0.01度
								Globa->Control_DC_Infor_Data[Group_Gun_No].charge_mode = KWH_MODE;
							}else if(msg.mtype == 0x1140)
							{//按时间充电
								Globa->Control_DC_Infor_Data[Group_Gun_No].charge_sec_limit =  ((msg.mtext[2]<<8) + msg.mtext[1])*60;//充电时长秒数限制
								Globa->Control_DC_Infor_Data[Group_Gun_No].charge_mode = TIME_MODE;
							}else if(msg.mtype == 0x1150)
							{//预约时间
								Globa->Control_DC_Infor_Data[Group_Gun_No].pre_time = msg.mtext[2] + msg.mtext[1]*60;//此处保存的是需要开始充电的时间 ===几点几分
								Globa->Control_DC_Infor_Data[Group_Gun_No].charge_mode = DELAY_CHG_MODE;
							}
		   
							msg.mtype = 0x200;//等待刷卡界面
							memset(&Page_200.reserve1, '0', sizeof(PAGE_200_FRAME));
							Page_200.state = 1;//1 没有检测到用户卡
							memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);

							Globa->QT_Step = 0x20; // 2 自动充电-等待刷卡界面  2.0
							Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_Present = 0;
					
							count_wait = 0;
							memset(&Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn[0], 0, sizeof(Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn));
						

							if(Globa->Charger_param.NEW_VOICE_Flag == 1)
							{
								 Voicechip_operation_function(0x04);//04H     “请刷卡”
							}else
							{
								Voicechip_operation_function(0x00);//00H	"欢迎光临.wav 300s 请刷卡1.wav"	
							}
							Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_check_flag = 0;
							Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_check_result = 0;
							Init_Find_St();
								
							break;
						}
						case 0x02:
						{//返回按钮
							msg.mtype = 0x1100;
							msgsnd(Globa->arm_to_qt_msg_id, &msg, 2, IPC_NOWAIT);					
							if(Globa->Charger_param.NEW_VOICE_Flag == 1)
							{
								Voicechip_operation_function(0x02);  //02H"    “请选择充电方式"
							}
							Globa->QT_Step = 0x121; //1 自动充电-》充电方式选择 状态  1.2.1

							break;
						}
						default:
						{
							break;
						}
					}
				}
				break;
			}

			case 0x20:
			{//---------------2 自动充电-》 等待刷卡界面   2.0--------------
				count_wait++;
				if(count_wait > 3000)
				{//等待时间超时30S
					count_wait = 0;
					Init_Find_St_End();
					Send_main_info_data(); //发送主要信息框架
					Globa->QT_Step = 0x03; 
					Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_check_flag = 0;//停止鉴权
					if(Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg == 1)
					{
						Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_falg = 0;
						Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Hostgun = 0;
						Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Othergun = 0;
						Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg = 0;
						Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Hostgun = 0;
						Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun = 0;
					}//恢复190929
					break;//超时必须退出
				}
				if(msg.mtype == 0x200)
				{
					if(msg.mtext[0] == 0x02)//返回按钮
					{	
						Init_Find_St_End();
						Send_main_info_data(); //发送主要信息框架
						Globa->QT_Step = 0x03;
						count_wait = 0;
						Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_check_flag = 0;//停止鉴权
						if(Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg == 1)
						{
							Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_falg = 0;
							Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Hostgun = 0;
							Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Othergun = 0;
							Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg = 0;
							Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Hostgun = 0;
							Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun = 0;
						}//恢复190929
						break;
					}
				}
				if(Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_Present ==  0)
				{
					if(IsFind_Idle_OK() == 1)
					{//卡片查询完毕
						if(Globa->MUT100_Dz_Card_Info.IC_Read_OK == 1)
						{//读出卡片信息

							if(valid_card_check(&Globa->MUT100_Dz_Card_Info.IC_ID[0]) > 0)
							{//黑名单
								printf("卡号已列入本地黑名单中!\n");
								msg.mtype = 0x200;//等待刷卡界面
								Page_200.state = 0x46;//7;//卡片已注销====黑名单
								memcpy(&Page_200.sn[0], &Globa->MUT100_Dz_Card_Info.IC_ID[0], sizeof(Page_200.sn));
									
								memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
								msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
								if(Globa->Charger_param.NEW_VOICE_Flag == 1)
								{
								  Voicechip_operation_function(0x0c);//0cH     “卡片已挂失，请联系发卡单位处理”
								}
								sleep(3);
								Init_Find_St_End();							
								Send_main_info_data(); //发送主要信息框架
								Globa->QT_Step = 0x03;
								count_wait = 0;
								// if(Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg == 1)
								// {
									// Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_falg = 0;
									// Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Hostgun = 0;
									// Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Othergun = 0;
									// Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg = 0;
									// Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Hostgun = 0;
									// Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun = 0;
								// }
								break;
							}
							Globa->Control_DC_Infor_Data[Group_Gun_No].card_type = Globa->MUT100_Dz_Card_Info.IC_type;	
							Globa->Control_DC_Infor_Data[Group_Gun_No].IC_state = Globa->MUT100_Dz_Card_Info.IC_state;	
							printf("---卡片查询完毕,卡片状态=0x%x---------\n",Globa->Control_DC_Infor_Data[Group_Gun_No].IC_state);

							
							if((Globa->Control_DC_Infor_Data[Group_Gun_No].card_type == 1)||
							   (Globa->Control_DC_Infor_Data[Group_Gun_No].card_type == 2)
							)
							{//卡片状态 1--用户卡 2-员工卡 3-管理员卡4-非充电卡 
								memcpy(&Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn[0], &Globa->MUT100_Dz_Card_Info.IC_ID[0], sizeof(Globa->MUT100_Dz_Card_Info.IC_ID));
								
								Globa->Control_DC_Infor_Data[Group_Gun_No].last_rmb = Globa->MUT100_Dz_Card_Info.IC_money;
								//load 卡特殊标记
								Globa->Control_DC_Infor_Data[Group_Gun_No].qiyehao_flag = Globa->MUT100_Dz_Card_Info.qiyehao_flag;
								Globa->Control_DC_Infor_Data[Group_Gun_No].private_price_flag = Globa->MUT100_Dz_Card_Info.private_price_flag ;
								Globa->Control_DC_Infor_Data[Group_Gun_No].order_chg_flag = Globa->MUT100_Dz_Card_Info.order_chg_flag ;
								Globa->Control_DC_Infor_Data[Group_Gun_No].offline_chg_flag = Globa->MUT100_Dz_Card_Info.offline_chg_flag ;
								
								if((Globa->Control_DC_Infor_Data[Group_Gun_No].card_type == 1)&&(Globa->Control_DC_Infor_Data[Group_Gun_No].IC_state == 0x31))//用户卡灰锁
								{
									Page_200.state = 4;	//用户卡状态异常					
									msg.mtype = 0x200;//等待刷卡界面
									memcpy(&Page_200.sn[0], &Globa->MUT100_Dz_Card_Info.IC_ID[0], sizeof(Page_200.sn));
									sprintf(&temp_char[0], "%08d",Globa->Control_DC_Infor_Data[Group_Gun_No].last_rmb);				
									memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
									
									memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
									msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
									
									sleep(3);
									Init_Find_St_End();
									Send_main_info_data(); //发送主要信息框架
									Globa->QT_Step = 0x03;
									count_wait = 0;		
									
									// if(Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg == 1)
									// {
										// Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_falg = 0;
										// Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Hostgun = 0;
										// Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Othergun = 0;
										// Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg = 0;
										// Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Hostgun = 0;
										// Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun = 0;
									// }
									break;
								}else//用户卡状态正常或员工卡
								{
									Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_Present = 1;//卡片可能可以充电
									if(Globa->have_hart == 1)
									{//在线运行----卡片进行校验
										if( (Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_check_result == 0)&&
											(Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_check_flag == 0)
											)
										{
											msg.mtype = 0x200;//等待刷卡界面
											Page_200.state = 12;//卡片验证中...请保持刷卡状态！
											memcpy(&Page_200.sn[0], &Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn[0], sizeof(Page_200.sn));
											sprintf(&temp_char[0], "%08d",Globa->Control_DC_Infor_Data[Group_Gun_No].last_rmb);				
											memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
											
											memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
											msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
											memcpy(&Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_check_card_ID[0], &Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn[0], sizeof(Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_check_card_ID));
											Globa->Control_DC_Infor_Data[Group_Gun_No].private_price_acquireOK = 0;//清除私有电价标记
											Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_check_flag = 1;
											break;
										}
									}else
									{//离线运行需判断余额
										Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_check_flag = 0;
										if((Globa->Control_DC_Infor_Data[Group_Gun_No].card_type == 1)&&(0xAA!=Globa->Control_DC_Infor_Data[Group_Gun_No].qiyehao_flag))//用户卡且不是企业号需要卡上扣费，需判断余额
										{
											if(Globa->Control_DC_Infor_Data[Group_Gun_No].last_rmb >= MIN_USER_MONEY_NEED)											
												Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_check_result = 2;//
											else
												Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_check_result = 1;//接下来判断余额
										
										}else//员工卡或企业号
											Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_check_result = 2;//等进一步判断===
										
									}
								}
							}
							else//无效的卡片
							{
								Init_Find_St_End();							
								Send_main_info_data(); //发送主要信息框架
								Globa->QT_Step = 0x03;
								count_wait = 0;
								// if(Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg == 1)
								// {
									// Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_falg = 0;
									// Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Hostgun = 0;
									// Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Othergun = 0;
									// Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg = 0;
									// Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Hostgun = 0;
									// Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun = 0;
								// }
								break;							
							}						
						}
						else
						{//没有读出卡片，重新进行读取
							Init_Find_St_End();
							Init_Find_St();
						}
					}					
				}
				
				if(Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_Present ==  1)//已检测到卡片
				{
					if((Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_check_result > 3))//		
					{//解读服务器验证结果
						Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_check_flag = 0;
						msg.mtype = 0x200;//等待刷卡界面
						//User_Card_check_result值如下	
						//4--该卡未进行发卡 5--无效充电桩序列号 6--黑名单 7--卡的企业号标记不同步 8--卡的私有电价标记不同步
						//9--卡的有序充电标记不同步  10--非规定时间充电卡 11--企业号余额不足 12--专用时间其他卡不可充
						//13--企业号启动达到限定桩数
						Page_200.state = (Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_check_result )&0xff;//只取低字节
						if(Page_200.state < 0x99)//
							Page_200.state |= 0x40;//增加偏移
						memcpy(&Page_200.sn[0], &Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn[0], sizeof(Page_200.sn));
						sprintf(&temp_char[0], "%08d", Globa->Control_DC_Infor_Data[Group_Gun_No].last_rmb);	
						memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
						memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
						msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
						
						sleep(3);
						Init_Find_St_End();
						Send_main_info_data(); //发送主要信息框架
						Globa->QT_Step = 0x03;
						count_wait = 0;
						// if(Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg == 1)
						// {
							// Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_falg = 0;
							// Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Hostgun = 0;
							// Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Othergun = 0;
							// Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg = 0;
							// Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Hostgun = 0;
							// Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun = 0;
						// }
						break;//退出，不能充电
					}else if(Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_check_result == 1)
					{//验证成功
						Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_check_flag = 0;
						msg.mtype = 0x200;//等待刷卡界面
						Page_200.state = 15;//验证成功请保持刷卡状态
						memcpy(&Page_200.sn[0], &Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn[0], sizeof(Page_200.sn));
						sprintf(&temp_char[0], "%08d", Globa->Control_DC_Infor_Data[Group_Gun_No].last_rmb);	
						memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
						memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
						msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
						
						//printf("-------卡片在线验证成功----\n");
						int check_money_enough = 0;
						if((Globa->Control_DC_Infor_Data[Group_Gun_No].card_type == 1)&&(0xAA!=Globa->Control_DC_Infor_Data[Group_Gun_No].qiyehao_flag))//用户卡且不是企业号需要加锁
						{
							if(Globa->Control_DC_Infor_Data[Group_Gun_No].last_rmb >= MIN_USER_MONEY_NEED)
								check_money_enough = 1;
						}
						else//员工卡或企业卡属性不判断余额
							check_money_enough = 1;
						
						if(check_money_enough)
						{ //直接启动					 	
							msg.mtype = 0x200;//等待刷卡界面
							if(0xAA==Globa->Control_DC_Infor_Data[Group_Gun_No].qiyehao_flag)//企业号
								Page_200.state = 17;
							else
							{
								if(Globa->Control_DC_Infor_Data[Group_Gun_No].card_type == 1)//用户卡
									Page_200.state = 5;//显示用户卡和卡片余额
								else
									Page_200.state = 6;//员工卡
							}
							memcpy(&Page_200.sn[0], &Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn[0], sizeof(Page_200.sn));
							sprintf(&temp_char[0], "%08d", Globa->Control_DC_Infor_Data[Group_Gun_No].last_rmb);	
							memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
							memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
							sleep(3);//显示余额
						
							Init_Find_St_End();
							
							if((Globa->Control_DC_Infor_Data[Group_Gun_No].card_type == 1)&&(0xAA!=Globa->Control_DC_Infor_Data[Group_Gun_No].qiyehao_flag))//用户卡且不是企业号需要加锁
							{						
								msg.mtype = 0x200;//等待刷卡界面				
								Page_200.state =  0x88;//卡片加锁中，请勿移动卡片!
								memcpy(&Page_200.sn[0], &Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn[0], sizeof(Page_200.sn));
								sprintf(&temp_char[0], "%08d", Globa->Control_DC_Infor_Data[Group_Gun_No].last_rmb);						
								memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
								memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
								msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
								
								send_card_info_data.Card_Command = 1;//加锁
								send_card_info_data.Command_Result = 0;
								send_card_info_data.Command_IC_money = Globa->Control_DC_Infor_Data[Group_Gun_No].total_rmb;
								memcpy(&send_card_info_data.IC_ID[0], &Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn, sizeof(send_card_info_data.IC_ID));
								send_card_info_data.Last_IC_money = Globa->Control_DC_Infor_Data[Group_Gun_No].last_rmb;
								Init_Power_On_End();
								Init_Card_PowerOn_St(&send_card_info_data);//卡片加锁处理
								count_wait = 0;
								Globa->QT_Step = 0x201;//加锁等待	
							}
							else
							{
								if(Globa->Control_DC_Infor_Data[Group_Gun_No].charge_mode == DELAY_CHG_MODE){
									Globa->QT_Step = 0x202;//预约界面
								}
								else
								{
									Globa->QT_Step = 0x21;//立即充电界面
									count_wait_ID = 0; //从刷卡的时候，进入充电界面需要等待1s之后才判断是否退出
									if(Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg == 1)
									{
										Auxiliary_Gun_Data_Assignment(Group_Gun_No,0);//辅助枪数据赋值		
									}									
								}
								Globa->Control_DC_Infor_Data[Group_Gun_No].MT625_Card_Start_ok_flag = 1;
							}
							break;
						}else//余额不足
						{
							Globa->Control_DC_Infor_Data[Group_Gun_No].MT625_Card_Start_ok_flag = 0;
							msg.mtype = 0x200;//等待刷卡界面
							Page_200.state = 11;//卡里余额过低
							memcpy(&Page_200.sn[0], &Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn[0], sizeof(Page_200.sn));
							sprintf(&temp_char[0], "%08d", Globa->Control_DC_Infor_Data[Group_Gun_No].last_rmb);	
							memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
							memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
							
							if(Globa->Charger_param.NEW_VOICE_Flag == 1)
							{
							  Voicechip_operation_function(0x0a);  //0aH 余额不足，请充值”OK
							}
							sleep(3);
							Init_Find_St_End();
							Send_main_info_data(); //发送主要信息框架
							Globa->QT_Step = 0x03;
							count_wait = 0;						
							// if(Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg == 1)
							// {
								// Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_falg = 0;
								// Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Hostgun = 0;
								// Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Othergun = 0;
								// Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg = 0;
								// Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Hostgun = 0;
								// Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun = 0;
							// }
							break;//退出，不能充电
						}
						
					}else if(Globa->Control_DC_Infor_Data[Group_Gun_No].User_Card_check_result == 2)//离线员工卡或企业卡充电
					{
						
						msg.mtype = 0x200;//等待刷卡界面					
						if(0xAA==Globa->Control_DC_Infor_Data[Group_Gun_No].qiyehao_flag)//企业号
						{							
							if(0xAA==Globa->Control_DC_Infor_Data[Group_Gun_No].offline_chg_flag)//允许离线充
								Page_200.state = 17;
							else//离线不允许充
								Page_200.state = 29;
						}
						else
						{
							if(Globa->Control_DC_Infor_Data[Group_Gun_No].card_type == 1)//用户卡
								Page_200.state = 5;//显示用户卡和卡片余额
							else
								Page_200.state = 6;//员工卡
						}
						memcpy(&Page_200.sn[0], &Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn[0], sizeof(Page_200.sn));
						sprintf(&temp_char[0], "%08d", Globa->Control_DC_Infor_Data[Group_Gun_No].last_rmb);						
						memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
						memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
						msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
						//sleep(3);//显示余额
						
						Init_Find_St_End();
						
						if((Globa->Control_DC_Infor_Data[Group_Gun_No].card_type == 1)&&(0xAA!=Globa->Control_DC_Infor_Data[Group_Gun_No].qiyehao_flag))//用户卡且不是企业号需要加锁
						{						
							msg.mtype = 0x200;//等待刷卡界面				
							Page_200.state =  0x88;//卡片加锁中，请勿移动卡片!
							memcpy(&Page_200.sn[0], &Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn[0], sizeof(Page_200.sn));
							sprintf(&temp_char[0], "%08d", Globa->Control_DC_Infor_Data[Group_Gun_No].last_rmb);						
							memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
							memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
							
							send_card_info_data.Card_Command = 1;//加锁
							send_card_info_data.Command_Result = 0;
							send_card_info_data.Command_IC_money = Globa->Control_DC_Infor_Data[Group_Gun_No].total_rmb;
							memcpy(&send_card_info_data.IC_ID[0], &Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn, sizeof(send_card_info_data.IC_ID));
							send_card_info_data.Last_IC_money = Globa->Control_DC_Infor_Data[Group_Gun_No].last_rmb;
							Init_Power_On_End();
							Init_Card_PowerOn_St(&send_card_info_data);//卡片加锁处理
							count_wait = 0;
							Globa->QT_Step = 0x201;//加锁等待	
						}
						else//无需加锁
						{
							sleep(2);
							if(Page_200.state == 29)//不允许充电
							{
								Send_main_info_data(); //发送主要信息框架
								Globa->QT_Step = 0x03;
								count_wait = 0;	
								Globa->Control_DC_Infor_Data[Group_Gun_No].MT625_Card_Start_ok_flag = 0;
								// if(Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg == 1)
								// {
									// Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_falg = 0;
									// Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Hostgun = 0;
									// Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Othergun = 0;
									// Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg = 0;
									// Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Hostgun = 0;
									// Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun = 0;
								// }
							}else
							{
								
								if(Globa->Control_DC_Infor_Data[Group_Gun_No].charge_mode == DELAY_CHG_MODE)
								{
									Globa->QT_Step = 0x202;//预约界面
								}else{
									Globa->QT_Step = 0x21;//立即充电界面	
									count_wait_ID = 0; //从刷卡的时候，进入充电界面需要等待1s之后才判断是否退出
									if(Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg == 1)
									{
										Auxiliary_Gun_Data_Assignment(Group_Gun_No,0);//辅助枪数据赋值		
									}	
								}
								Globa->Control_DC_Infor_Data[Group_Gun_No].MT625_Card_Start_ok_flag = 1;
							}
						}
						break;					
					}
					
				}
				break;
			}
			case 0x201://用户卡加锁
			{
				count_wait++;
				if(count_wait > 1000)
				{//等待时间超时10S
					count_wait = 0;
					Init_Find_St_End();
					Init_Power_On_End();
					Send_main_info_data(); //发送主要信息框架
					Globa->QT_Step = 0x03; //
					if(Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg == 1)
					{
						Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_falg = 0;
						Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Hostgun = 0;
						Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Othergun = 0;
						Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg = 0;
						Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Hostgun =0;
						Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun = 0;
					}
					break;//超时必须退出
				}
				if(IsCard_PwrON_OK())
				{
					if(IsCard_PwrON_Result() == 1)
					{//加锁成功
						sleep(2);//show
						Globa->Control_DC_Infor_Data[Group_Gun_No].MT625_Card_Start_ok_flag = 1;//卡片加锁成功
						if(Globa->Control_DC_Infor_Data[Group_Gun_No].charge_mode == DELAY_CHG_MODE){
							Globa->QT_Step = 0x202;//预约界面
					  }else{
							Globa->QT_Step = 0x21;//立即充电界面
							count_wait_ID = 0; //从刷卡的时候，进入充电界面需要等待1s之后才判断是否退出
							if(Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg == 1)
							{
								 Auxiliary_Gun_Data_Assignment(Group_Gun_No,0);//辅助枪数据赋值		
							}	
						}							
						break;
					}else
					{
						Init_Power_On_End();
						Init_Card_PowerOn_St(&send_card_info_data);//卡片加锁处理
					}
				}
				
			}
			break;
			
			case 0x202:
			{ //预约界面
				count_wait++;
				if(count_wait > 3000)//预约界面超时30秒后返回首页-----
				{//等待时间超时30S后返回上一界面，使得可以远程APP启动充电
					count_wait = 0;
					Send_main_info_data(); //发送主要信息框架
					Globa->QT_Step = 0x03; 
				
					break;//超时必须退出
				}
				
				if(Globa->Control_DC_Infor_Data[Group_Gun_No].charger_state == 0x06)
				{ 
					if((Globa->Control_DC_Infor_Data[Group_Gun_No].remain_hour != Page_daojishi.hour)||
						(Globa->Control_DC_Infor_Data[Group_Gun_No].remain_min != Page_daojishi.min)||
						(Globa->Control_DC_Infor_Data[Group_Gun_No].remain_second != Page_daojishi.second)
						)
						{						
							Page_daojishi.hour = Globa->Control_DC_Infor_Data[Group_Gun_No].remain_hour;		
							Page_daojishi.min  = Globa->Control_DC_Infor_Data[Group_Gun_No].remain_min;
							Page_daojishi.second = Globa->Control_DC_Infor_Data[Group_Gun_No].remain_second;
							Page_daojishi.Gun_no = Group_Gun_No;
							Page_daojishi.Button_Show_flag = 0;//显示按钮是否出现 0--出现 1--消失
							memcpy(&Page_daojishi.card_sn[0], &Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn[0], sizeof(Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn));
							memcpy(&msg.mtext[0], &Page_daojishi.reserve1, sizeof(PAGE_DAOJISHI_FRAME));
							msg.mtype = 0x1151;//倒计时界面
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_DAOJISHI_FRAME), IPC_NOWAIT);
						}
				}else if(Globa->Control_DC_Infor_Data[Group_Gun_No].charger_state == 0x04)
				{ //充电中
					count_wait = 0;
					Send_charge_info_data(Group_Gun_No);//充电界面
					count_wait1 = 0;
					count_wait2 = 0;
					Globa->QT_Step = 0x21;  
					break;
				}else if(Globa->Control_DC_Infor_Data[Group_Gun_No].charger_state == 0x05 )
				{ //充电完成
					Page_1300.cause = Globa->Control_DC_Infor_Data[Group_Gun_No].charge_end_reason;//charger_over_flag;//直接手动结束的 ;
					Page_1300.Capact = Globa->Control_DC_Infor_Data[Group_Gun_No].BMS_batt_SOC;
					Page_1300.elapsed_time[0] =  Globa->Control_DC_Infor_Data[Group_Gun_No].Time;
					Page_1300.elapsed_time[1] =  Globa->Control_DC_Infor_Data[Group_Gun_No].Time>>8;
					
					//memcpy(Page_1300.KWH,&Globa->Control_DC_Infor_Data[Group_Gun_No].kwh, 4);
					memcpy(Page_1300.KWH,&Globa->Control_DC_Infor_Data[Group_Gun_No].soft_KWH, 4);
					memcpy(Page_1300.RMB,&Globa->Control_DC_Infor_Data[Group_Gun_No].total_rmb, 4);
					Page_1300.charger_start_way = Globa->Control_DC_Infor_Data[Group_Gun_No].charger_start_way + 1;   //充电启动方式 1:刷卡  2：手机APP 101:VIN
					memcpy(&Page_1300.SN[0], &Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn[0], sizeof(Page_1300.SN));
					Page_1300.Gun_no = Group_Gun_No;
					////////////////////////////////////////////////////////////////////////////////
					memset(Page_1300.start_time,0,sizeof(Page_1300.start_time));
					memcpy(Page_1300.start_time,&Globa->Control_DC_Infor_Data[Group_Gun_No].charge_start_time[11],8);
					memset(Page_1300.end_time,0,sizeof(Page_1300.end_time));
					memcpy(Page_1300.end_time,&Globa->Control_DC_Infor_Data[Group_Gun_No].charge_end_time[11],8);								
					Page_1300.Double_Gun_To_Car_falg = Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg;
					if(1 == Page_1300.Double_Gun_To_Car_falg)
					{
						total_rmb = Globa->Control_DC_Infor_Data[Group_Gun_No].total_rmb + Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].total_rmb;
						//total_kwh = Globa->Control_DC_Infor_Data[Group_Gun_No].kwh + Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].kwh;
						total_kwh = Globa->Control_DC_Infor_Data[Group_Gun_No].soft_KWH + Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].soft_KWH;
						memcpy(Page_1300.KWH,&total_kwh, 4);								
						memcpy(Page_1300.RMB,&total_rmb, 4);								
					}
					/////////////////////////////////////////////////////////////////////////////////
					
					memcpy(&msg.mtext[0], &Page_1300.reserve1, sizeof(PAGE_1300_FRAME));

					msg.mtype = 0x1310;//单枪结束界面
					msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_1300_FRAME), IPC_NOWAIT);
					Globa->QT_Step = 0x21;
					break;
				}
				
				if((msg.mtype == 0x1151))
				{//收到该界面消息
					switch(msg.mtext[0])
					{//取消
						case 0x01:
						{//取消
							msg.mtype = 0x200;//等待刷卡界面
							memset(&Page_200.reserve1, '0', sizeof(PAGE_200_FRAME));
							Page_200.state = 1;//1 没有检测到用户卡
							memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
							Globa->QT_Step = 0x203; // 2查询卡片
							count_wait = 0; //取消预约
							Init_Find_St();
							break;
						}
						case 0x02:
						{//返回主页
							Send_main_info_data(); //发送主要信息框架
							Globa->QT_Step = 0x03;
							count_wait = 0;
							count_wait1 = 0;
						
							break;
						}
					}
				}
				break ;
			}
			case 0x203:
			{//取消预约
				count_wait++;
				if(count_wait > 1000)
				{//等待时间超时10S
					count_wait = 0;
					Init_Find_St_End();
					Globa->QT_Step = 0x202; //返回预约界面
					break;//超时必须退出
				}
				if(msg.mtype == 0x200)
				{
					if(msg.mtext[0] == 0x02)//返回按钮
					{	
						Init_Find_St_End();
						Globa->QT_Step = 0x202; //返回预约界面
						break;
					}
				}
				if(IsFind_Idle_OK() == 1)
				{//卡片查询完毕
					if(Globa->MUT100_Dz_Card_Info.IC_Read_OK == 1)
					{//读出卡片信息
						if(0 == memcmp(&Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn[0], &Globa->MUT100_Dz_Card_Info.IC_ID[0], sizeof(Globa->Control_DC_Infor_Data[i].card_sn)))
						{//同一张卡片
							if((Globa->Control_DC_Infor_Data[Group_Gun_No].card_type == 0x02)||(Globa->Control_DC_Infor_Data[Group_Gun_No].qiyehao_flag==0xAA ))//员工卡或企业号倒计时充电无需解锁
							{//员工卡
								Globa->Control_DC_Infor_Data[Group_Gun_No].Credit_card_success_flag = 1;
								//
								msg.mtype = 0x200;//等待刷卡界面
								memset(&Page_200.reserve1, '0', sizeof(PAGE_200_FRAME));
								Page_200.state = 32;//32 卡号匹配，取现倒计时充电
								memcpy(&Page_200.sn[0], &Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn[0], sizeof(Page_200.sn));
					
								memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
								msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);							
								if(Globa->Control_DC_Infor_Data[Group_Gun_No].charger_state == 0x05)//下面已结束
								{
									Globa->Control_DC_Infor_Data[Group_Gun_No].MT625_Card_End_ok_flag = 1;
									sleep(3);
									count_wait = 0;
									Init_Find_St_End();
									Send_main_info_data(); //发送主要信息框架
									Globa->QT_Step = 0x03; 
									break;
								}
								
							}else//用户卡需解锁
							{	
								Globa->Control_DC_Infor_Data[Group_Gun_No].Credit_card_success_flag = 1;//先通知计费预约终止
								msg.mtype = 0x200;//
								memset(&Page_200.reserve1, '0', sizeof(PAGE_200_FRAME));
								memcpy(&Page_200.sn[0], &Globa->MUT100_Dz_Card_Info.IC_ID[0], sizeof(Page_200.sn));
								Page_200.state = 0x89;   //卡片解锁中，请勿移动卡片
								sprintf(&temp_char[0], "%08d",Globa->Control_DC_Infor_Data[Group_Gun_No].last_rmb);				
								memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
											
								memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
								msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
								count_wait = 0;
								send_card_info_data.Card_Command = 2;//解锁
								send_card_info_data.Command_Result = 0;
								send_card_info_data.Command_IC_money = 0;//Globa->Control_DC_Infor_Data[Group_Gun_No].total_rmb;
								memcpy(&send_card_info_data.IC_ID[0], &Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn, sizeof(send_card_info_data.IC_ID));
								send_card_info_data.Last_IC_money = Globa->Control_DC_Infor_Data[Group_Gun_No].last_rmb;
								Init_Power_Off_End();
								Init_Card_PowerOff_St(&send_card_info_data);//卡片解锁处理
								Globa->QT_Step = 0x204; 
								break;
							}
							
						}else//卡号不匹配
						{
							msg.mtype = 0x200;//等待刷卡界面
							memset(&Page_200.reserve1, '0', sizeof(PAGE_200_FRAME));
							Page_200.state = 33;//33 卡号不匹配
							memcpy(&Page_200.sn[0], &Globa->MUT100_Dz_Card_Info.IC_ID[0], sizeof(Page_200.sn));
					
							memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));							
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
							
							Init_Find_St_End();
							sleep(2);
							Globa->QT_Step = 0x202; //返回预约界面
							break;
						}
					}else
					{//没有读出卡片，重新进行读取
						Init_Find_St_End();
						Init_Find_St();
					}
				}
				break;
			}
			case 0x204:
			{//卡片解锁
				count_wait++;
				if(count_wait > 3000)
				{//等待时间超时30S
					count_wait = 0;
					Init_Find_St_End();
					Globa->QT_Step = 0x202; //预约
					break;//超时必须退出
				}
				
				if(IsCard_PwrOff_OK())
				{
					if(IsCard_PwrOff_Result() == 1)
					{//解锁成功
						//显示余额
						msg.mtype = 0x200;//
						memset(&Page_200.reserve1, '0', sizeof(PAGE_200_FRAME));
						memcpy(&Page_200.sn[0], &Globa->MUT100_Dz_Card_Info.IC_ID[0], sizeof(Page_200.sn));
						Page_200.state = 0x8A;   //卡片解锁成功
						sprintf(&temp_char[0], "%08d",send_card_info_data.Last_IC_money - send_card_info_data.Command_IC_money );				
						memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
									
						memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
						msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
						
						if(Globa->Charger_param.NEW_VOICE_Flag == 1){
								Voicechip_operation_function(0x11);//11H     “卡片解锁成功”
							}
						sleep(2);
						Globa->QT_Step = 0x205; //预约结束
						break;
					}else
					{
						Init_Power_Off_End();
						Init_Card_PowerOff_St(&send_card_info_data);//卡片解锁处理
					}
				}
				break;
			}
			case 0x205:
			{//---------------2 预约结束 结算界面显示部分      2.3-------------
  					
					Page_1300.cause = Globa->Control_DC_Infor_Data[Group_Gun_No].charge_end_reason;//					
					Page_1300.Capact = Globa->Control_DC_Infor_Data[Group_Gun_No].BMS_batt_SOC;
					Page_1300.elapsed_time[0] =  Globa->Control_DC_Infor_Data[Group_Gun_No].Time;
					Page_1300.elapsed_time[1] =  Globa->Control_DC_Infor_Data[Group_Gun_No].Time>>8;				
					memcpy(Page_1300.KWH,&Globa->Control_DC_Infor_Data[Group_Gun_No].kwh, 4);
					memcpy(Page_1300.RMB,&Globa->Control_DC_Infor_Data[Group_Gun_No].total_rmb, 4);
					Page_1300.charger_start_way = Globa->Control_DC_Infor_Data[Group_Gun_No].charger_start_way + 1;   //充电启动方式 1:刷卡  2：手机APP 101:VIN
					Page_1300.Button_Show_flag = 1;//不显示结算按钮
					Page_1300.Gun_no = Group_Gun_No;
					memcpy(&Page_1300.SN[0], &Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn[0], sizeof(Page_1300.SN));
					
					////////////////////////////////////////////////////////////////////////////////
					memset(Page_1300.start_time,0,sizeof(Page_1300.start_time));
					memcpy(Page_1300.start_time,&Globa->Control_DC_Infor_Data[Group_Gun_No].charge_start_time[11],8);
					memset(Page_1300.end_time,0,sizeof(Page_1300.end_time));
					memcpy(Page_1300.end_time,&Globa->Control_DC_Infor_Data[Group_Gun_No].charge_end_time[11],8);								
					Page_1300.Double_Gun_To_Car_falg = 0;					
					/////////////////////////////////////////////////////////////////////////////////
					memcpy(&msg.mtext[0], &Page_1300.reserve1, sizeof(PAGE_1300_FRAME));

					msg.mtype = 0x1310;//单枪结束界面
					msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_1300_FRAME), IPC_NOWAIT);
					if(Globa->Control_DC_Infor_Data[Group_Gun_No].charger_state == 0x05 )//等控制单元停止完成后再置标志
					{
						Globa->Control_DC_Infor_Data[Group_Gun_No].MT625_Card_End_ok_flag = 1;//延迟置，避免计费任务清空了当前的计量信息!
						sleep(2);
						count_wait = 0;
						count_wait1 = 0;
						Init_Find_St_End();
						Init_Power_On_End();
						Init_Power_Off_End();
						Send_main_info_data(); //发送主要信息框架
						Globa->QT_Step = 0x03; 
					}
					sleep(1);
				break;
			}
			
			
			case 0x21://根据charger_state状态跳转不同界面
			{//---------------2 自动充电-》 自动充电界面 状态  2.1----------
				count_wait++;
				count_wait1++;
				count_wait2++;
				if((msg.mtype == 0x1122)||(msg.mtype == 0x1310))
				{//收到该界面消息
					switch(msg.mtext[0])
					{//判断 在当前界面下 用户操作的按钮
						case 0x01:
						{//停止充电按钮
							count_wait = 0;
							count_wait1 = 0;							
							
							if(Globa->Control_DC_Infor_Data[Group_Gun_No].charger_start_way == CHG_BY_CARD)
							{
								if(0 == Globa->Control_DC_Infor_Data[Group_Gun_No].MT625_Card_End_ok_flag)
								{
									Globa->QT_Step = 0x22; //等待刷卡
									Globa->Control_DC_Infor_Data[Group_Gun_No].Credit_card_success_flag = 0;//
									
									Init_Find_St_End();
									Init_Find_St();
									msg.mtype = 0x200;//等待刷卡界面
									memset(&Page_200.reserve1, '0', sizeof(PAGE_200_FRAME));
									Page_200.state = 1;//1 没有检测到用户卡
									memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
									msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
								}else
								{
									count_wait = 0;
									count_wait1 = 0;
									Init_Find_St_End();
									Init_Power_On_End();
									Init_Power_Off_End();
									Send_main_info_data(); //发送主要信息框架
									Globa->QT_Step = 0x03; 
									if(1 == Globa->Control_DC_Infor_Data[Group_Gun_No].MT625_Card_End_ok_flag)
									{
										Globa->Control_DC_Infor_Data[Group_Gun_No].MT625_Card_End_ok_flag = 2;//test
										if(Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg == 1)
											Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].MT625_Card_End_ok_flag = 2; 
									}
									break;
								}
							}
						
							
							if(Globa->Control_DC_Infor_Data[Group_Gun_No].charger_start_way == CHG_BY_VIN)
							{
								if(msg.mtype == 0x1310)//完成界面点 确定
								{
									if( 0 == Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg)//单把枪直接走
										Globa->Control_DC_Infor_Data[Group_Gun_No].MT625_Card_End_ok_flag = 2;//延迟置，避免计费任务清空了当前的计量信息!
									else//双枪对一辆车充电
									{
										if(	1 == Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Hostgun)//主枪点了确定才走
										{
											Globa->Control_DC_Infor_Data[Group_Gun_No].MT625_Card_End_ok_flag = 2;//延迟置，避免计费任务清空了当前的计量信息!
											if(Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg == 1)
											{
												Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].MT625_Card_End_ok_flag = 2; 
											}
										}else//辅枪
										{
											count_wait = 0;
											count_wait1 = 0;
											Init_Find_St_End();
											Init_Power_On_End();
											Init_Power_Off_End();
											Send_main_info_data(); //发送主要信息框架
											Globa->QT_Step = 0x03; 
											break;										
										}
									}
									
								}else//充电中点 停止
									Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_Stop++;
							}
							
							
							break;
						}
						case 0x02:
						{//返回主页
							count_wait = 0;
							count_wait1 = 0;
							Init_Find_St_End();
							Init_Power_On_End();
							Init_Power_Off_End();
							Send_main_info_data(); //发送主要信息框架
							Globa->QT_Step = 0x03; 
							break;
						}
					}
				}
				
				if(Globa->Control_DC_Infor_Data[Group_Gun_No].charger_state == 0x04)
				{ //充电中
					if(count_wait > 50){//500mS发1次充电数据
						count_wait = 0;
						Send_charge_info_data(Group_Gun_No);
					}else if(count_wait1 > 200){//平均2S发一次bms数据
						count_wait1 = 0;
						Send_BMS_info_data(Group_Gun_No);
						Globa->Control_DC_Infor_Data[Group_Gun_No].VIN_Stop = 0;//clear 2秒内没按按钮时
					}
					if(count_wait2 > 3000)//单枪充电信息显示界面超时30秒后返回首页-----
					{//等待时间超时30S后返回上一界面，使得可以远程APP启动充电
						count_wait = 0;	
						count_wait1 = 0;
						count_wait2 = 0;
						Send_main_info_data(); //发送主要信息框架
						Globa->QT_Step = 0x03; 
						break;//超时必须退出
					}	
				}else if(Globa->Control_DC_Infor_Data[Group_Gun_No].charger_state == 0x05 )//计费已判断到充电停止
				{ //充电完成
					if(count_wait1 > 200)
					{//平均2S发一次数据
						count_wait1 = 0;						
						Page_1300.cause = Globa->Control_DC_Infor_Data[Group_Gun_No].charge_end_reason;// ;
						Page_1300.Capact = Globa->Control_DC_Infor_Data[Group_Gun_No].BMS_batt_SOC;
						Page_1300.elapsed_time[0] =  Globa->Control_DC_Infor_Data[Group_Gun_No].Time;
						Page_1300.elapsed_time[1] =  Globa->Control_DC_Infor_Data[Group_Gun_No].Time>>8;					
						//memcpy(Page_1300.KWH,&Globa->Control_DC_Infor_Data[Group_Gun_No].kwh, 4);
						memcpy(Page_1300.KWH,&Globa->Control_DC_Infor_Data[Group_Gun_No].soft_KWH, 4);
						memcpy(Page_1300.RMB,&Globa->Control_DC_Infor_Data[Group_Gun_No].total_rmb, 4);
						Page_1300.charger_start_way = Globa->Control_DC_Infor_Data[Group_Gun_No].charger_start_way + 1;   //充电启动方式 1:刷卡  2：手机APP 101:VIN
						if(Page_1300.charger_start_way == 2){//APP
							Page_1300.Button_Show_flag = 1;//显示结算按钮是否出现 0--出现 1--消失
						}else{//刷卡或VIN显示按钮
							Page_1300.Button_Show_flag = 0;//显示按钮是否出现 0--出现 1--消失
						}
						Page_1300.Gun_no = Group_Gun_No;
						////////////////////////////////////////////////////////////////////////////////
						memset(Page_1300.start_time,0,sizeof(Page_1300.start_time));
						memcpy(Page_1300.start_time,&Globa->Control_DC_Infor_Data[Group_Gun_No].charge_start_time[11],8);
						memset(Page_1300.end_time,0,sizeof(Page_1300.end_time));
						memcpy(Page_1300.end_time,&Globa->Control_DC_Infor_Data[Group_Gun_No].charge_end_time[11],8);								
						Page_1300.Double_Gun_To_Car_falg = Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg;
						if(1 == Page_1300.Double_Gun_To_Car_falg)
						{
							total_rmb = Globa->Control_DC_Infor_Data[Group_Gun_No].total_rmb + Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].total_rmb;
							//total_kwh = Globa->Control_DC_Infor_Data[Group_Gun_No].kwh + Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].kwh;
							total_kwh = Globa->Control_DC_Infor_Data[Group_Gun_No].soft_KWH + Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].soft_KWH;
							memcpy(Page_1300.KWH,&total_kwh, 4);								
							memcpy(Page_1300.RMB,&total_rmb, 4);								
						}
						/////////////////////////////////////////////////////////////////////////////////
						
						memcpy(&Page_1300.SN[0], &Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn[0], sizeof(Page_1300.SN));

						memcpy(&msg.mtext[0], &Page_1300.reserve1, sizeof(PAGE_1300_FRAME));

						msg.mtype = 0x1310;//单枪结束界面
						msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_1300_FRAME), IPC_NOWAIT);
						
					}
					if(count_wait > 3000)
					{//等待时间超时30S
						count_wait = 0;	
						count_wait1 = 0;
						Send_main_info_data(); //发送主要信息框架
						Globa->QT_Step = 0x03; 
						break;//超时必须退出
					}	
				}else if(Globa->Control_DC_Infor_Data[Group_Gun_No].charger_state == 0x03)
				{//空闲则退出
			    count_wait_ID++;
					if(count_wait_ID >= 100){//1s之后退出
						count_wait = 0;
						count_wait1 = 0;
						count_wait_ID = 0;
						Init_Find_St_End();
						Init_Power_On_End();
						Init_Power_Off_End();
						Send_main_info_data(); //发送主要信息框架
						Globa->QT_Step = 0x03; 
						printf("--------空闲则退出---------\n");
						// if(Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg == 1)
						// {
							// Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_falg = 0;
							// Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Hostgun = 0;
							// Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Othergun = 0;
							// Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg = 0;
							// Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Hostgun = 0;
							// Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun = 0;
						// }
					}
				}
				break;
			}
			case 0x22:
			{//查询卡片
				count_wait++;
				if(count_wait > 1000)
				{//等待时间超时10S
					count_wait = 0;
					Init_Find_St_End();
					Globa->QT_Step = 0x21; 
					break;//超时必须退出
				}
				if(msg.mtype == 0x200)
				{
					if(msg.mtext[0] == 0x02)//返回按钮
					{	
						Init_Find_St_End();	
						count_wait1 = 200;//跳转到0x21后立即发界面数据，以便切换界面，避免感觉卡!
						Globa->QT_Step = 0x21; 
						break;//
					}
				}
				if(IsFind_Idle_OK() == 1)
				{//卡片查询完毕
				  if(Globa->MUT100_Dz_Card_Info.IC_Read_OK == 1)
				  {//读出卡片信息
						if(0 == memcmp(&Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn[0], &Globa->MUT100_Dz_Card_Info.IC_ID[0], sizeof(Globa->Control_DC_Infor_Data[i].card_sn)))
						{//
							msg.mtype = 0x200;//等待刷卡界面
							memset(&Page_200.reserve1, '0', sizeof(PAGE_200_FRAME));
							memcpy(&Page_200.sn[0], &Globa->MUT100_Dz_Card_Info.IC_ID[0], sizeof(Page_200.sn));							
							Page_200.state = 32;//停机中
							memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
							
							Globa->Control_DC_Infor_Data[Group_Gun_No].Credit_card_success_flag = 1; 
							if(Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg == 1)
							{
								Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Credit_card_success_flag = 1; 
							}							
							count_wait = 0;
							Init_Find_St_End();
							sleep(2);
							if((Globa->Control_DC_Infor_Data[Group_Gun_No].card_type == 1)&&(0xAA!=Globa->Control_DC_Infor_Data[Group_Gun_No].qiyehao_flag))//用户卡且不是企业号需要加锁
								Globa->QT_Step = 0x23;//准备解锁
							else//员工卡
							{
								Globa->QT_Step = 0x25; //卡号匹配充电结束								
							}
							break;
						}
						else
						{
							msg.mtype = 0x200;//卡片不不一样请刷另一张卡片
							memset(&Page_200.reserve1, '0', sizeof(PAGE_200_FRAME));
							
							Page_200.state = 33;//33 卡号不匹配
							memcpy(&Page_200.sn[0], &Globa->MUT100_Dz_Card_Info.IC_ID[0], sizeof(Page_200.sn));
					
							memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
							count_wait = 51;//状态切换后触发界面数据刷新
							count_wait2 = 0;
							Init_Find_St_End();
							//Init_Find_St();
							sleep(2);//show info		
							count_wait1 = 200;//状态切换后触发界面数据刷新						
							Globa->QT_Step = 0x21; //返回此状态后根据计费的状态来更新显示==
						}
					}
				}
				break;
			}
			case 0x23:
			{//卡片准备解锁状态
				count_wait++;
				if(count_wait > 3000)
				{//等待时间超时30S
					count_wait = 0;
					Init_Find_St_End();
					Globa->QT_Step = 0x21;
					break;//超时必须退出
				}
				
				if(Globa->Control_DC_Infor_Data[Group_Gun_No].charger_state == 0x05)
				{ //充电完成计费信息不再刷新后再扣卡片费用，避免扣费与记录不同
					msg.mtype = 0x200;//
					memset(&Page_200.reserve1, '0', sizeof(PAGE_200_FRAME));
					memcpy(&Page_200.sn[0], &Globa->MUT100_Dz_Card_Info.IC_ID[0], sizeof(Page_200.sn));
					Page_200.state = 0x89;   //卡片解锁中，请勿移动卡片
					sprintf(&temp_char[0], "%08d",Globa->Control_DC_Infor_Data[Group_Gun_No].last_rmb);				
					memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
								
					memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
					msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
					count_wait = 0;
					send_card_info_data.Card_Command = 2;//解锁
					send_card_info_data.Command_Result = 0;
					if(Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg == 1)
					{
						send_card_info_data.Command_IC_money = Globa->Control_DC_Infor_Data[Group_Gun_No].total_rmb + Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].total_rmb;
					}
					else
					{
					   send_card_info_data.Command_IC_money = Globa->Control_DC_Infor_Data[Group_Gun_No].total_rmb;
					}
					memcpy(&send_card_info_data.IC_ID[0], &Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn, sizeof(send_card_info_data.IC_ID));
					send_card_info_data.Last_IC_money = Globa->Control_DC_Infor_Data[Group_Gun_No].last_rmb;
					Init_Power_Off_End();
					Init_Card_PowerOff_St(&send_card_info_data);//卡片解锁处理
					Globa->QT_Step = 0x24; 
					break;
				}
				break;
			}
			case 0x24:
			{//卡片解锁
				count_wait++;
				if(count_wait > 3000)
				{//等待时间超时30S
					count_wait = 0;
					Init_Find_St_End();
					Globa->QT_Step = 0x21;
					break;//超时必须退出
				}
				
				if(IsCard_PwrOff_OK())
				{
					if(IsCard_PwrOff_Result() == 1)
					{//解锁成功
						//显示余额
						msg.mtype = 0x200;//
						memset(&Page_200.reserve1, '0', sizeof(PAGE_200_FRAME));
						memcpy(&Page_200.sn[0], &Globa->MUT100_Dz_Card_Info.IC_ID[0], sizeof(Page_200.sn));
						Page_200.state = 0x8A;   //卡片解锁成功
						sprintf(&temp_char[0], "%08d",send_card_info_data.Last_IC_money - send_card_info_data.Command_IC_money );				
						memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
									
						memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
						msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
						
						if(Globa->Charger_param.NEW_VOICE_Flag == 1){
								Voicechip_operation_function(0x11);//11H     “卡片解锁成功”
							}
						sleep(2);
						Globa->QT_Step = 0x25; //充电结束
						break;
					}else
					{
						Init_Power_Off_End();
						Init_Card_PowerOff_St(&send_card_info_data);//卡片解锁处理
					}
				}
				break;
			}
			case 0x25:
			{//---------------2 充电结束 结算界面显示部分      2.3-------------
				//if(Globa->Control_DC_Infor_Data[Group_Gun_No].charger_state == 0x03 )
				{ //充电完成					
					Page_1300.cause = Globa->Control_DC_Infor_Data[Group_Gun_No].charge_end_reason;
					Page_1300.Capact = Globa->Control_DC_Infor_Data[Group_Gun_No].BMS_batt_SOC;
					Page_1300.elapsed_time[0] =  Globa->Control_DC_Infor_Data[Group_Gun_No].Time;
					Page_1300.elapsed_time[1] =  Globa->Control_DC_Infor_Data[Group_Gun_No].Time>>8;				
					//memcpy(Page_1300.KWH,&Globa->Control_DC_Infor_Data[Group_Gun_No].kwh, 4);
					memcpy(Page_1300.KWH,&Globa->Control_DC_Infor_Data[Group_Gun_No].soft_KWH, 4);
					memcpy(Page_1300.RMB,&Globa->Control_DC_Infor_Data[Group_Gun_No].total_rmb, 4);
					Page_1300.charger_start_way = Globa->Control_DC_Infor_Data[Group_Gun_No].charger_start_way + 1;   //充电启动方式 1:刷卡  2：手机APP 101:VIN
					Page_1300.Button_Show_flag = 1;//不显示结算按钮
					Page_1300.Gun_no = Group_Gun_No;
					memcpy(&Page_1300.SN[0], &Globa->Control_DC_Infor_Data[Group_Gun_No].card_sn[0], sizeof(Page_1300.SN));
					////////////////////////////////////////////////////////////////////////////////
					memset(Page_1300.start_time,0,sizeof(Page_1300.start_time));
					memcpy(Page_1300.start_time,&Globa->Control_DC_Infor_Data[Group_Gun_No].charge_start_time[11],8);
					memset(Page_1300.end_time,0,sizeof(Page_1300.end_time));
					memcpy(Page_1300.end_time,&Globa->Control_DC_Infor_Data[Group_Gun_No].charge_end_time[11],8);								
					Page_1300.Double_Gun_To_Car_falg = Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg;
					if(1 == Page_1300.Double_Gun_To_Car_falg)
					{
						total_rmb = Globa->Control_DC_Infor_Data[Group_Gun_No].total_rmb + Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].total_rmb;
						//total_kwh = Globa->Control_DC_Infor_Data[Group_Gun_No].kwh + Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].kwh;
						total_kwh = Globa->Control_DC_Infor_Data[Group_Gun_No].soft_KWH + Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].soft_KWH;
						memcpy(Page_1300.KWH,&total_kwh, 4);								
						memcpy(Page_1300.RMB,&total_rmb, 4);								
					}
					/////////////////////////////////////////////////////////////////////////////////
					memcpy(&msg.mtext[0], &Page_1300.reserve1, sizeof(PAGE_1300_FRAME));

					msg.mtype = 0x1310;//单枪结束界面
					msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_1300_FRAME), IPC_NOWAIT);
					if(Globa->Control_DC_Infor_Data[Group_Gun_No].charger_state == 0x05 )//等控制单元停止完成后再置标志
					{	
						Globa->Control_DC_Infor_Data[Group_Gun_No].MT625_Card_End_ok_flag = 1;
						Globa->Control_DC_Infor_Data[Group_Gun_No].Operation_command = 2;//进行解锁---避免计费任务在等第2次按确定导致枪没解锁
						if(Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg == 1)
						{
							Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].MT625_Card_End_ok_flag = 1; 
							Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Operation_command = 2;
						}	
						sleep(2);
						count_wait = 0;
						count_wait1 = 0;
						Init_Find_St_End();
						Init_Power_On_End();
						Init_Power_Off_End();
						Send_main_info_data(); //发送主要信息框架
						Globa->QT_Step = 0x03; 
						// if(Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg == 1)
						// {
							// Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_falg = 0;
							// Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Hostgun = 0;
							// Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun].Double_Gun_To_Car_Othergun = 0;
							// Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_falg = 0;
							// Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Hostgun = 0;
							// Globa->Control_DC_Infor_Data[Group_Gun_No].Double_Gun_To_Car_Othergun = 0;
						// }
					}
					sleep(1);
					break;
				}
			}
			case 0x30:
			{//等待刷卡--等待刷维护卡才能进入系统管理状态
				count_wait++;
				if(count_wait > 1000)
				{//等待时间超时20S
					count_wait = 0;
					msg.mtype = 0x100;
					msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
					Globa->QT_Step = 0x02;
					Init_Find_St_End();
					break;//超时必须退出
				}
				if(msg.mtype == 0x200)
				{
					if(msg.mtext[0] == 0x02)//返回按钮
					{
						count_wait = 0;
						msg.mtype = 0x100;
						msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa->QT_Step = 0x02;
						Init_Find_St_End();
						break;//超时必须退出
					}
				}
				if(IsFind_Idle_OK() == 1)
				{//卡片查询完毕
				  if(Globa->MUT100_Dz_Card_Info.IC_Read_OK == 1)
				  {//读出卡片信息
						if(Globa->MUT100_Dz_Card_Info.IC_type == 3)
						{
							msg.mtype = 0x3000;//系统管理界面
							msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa->QT_Step = 0x31; // 系统管理界面
							count_wait = 0;
							break;
						}else
						{
							if(Globa->MUT100_Dz_Card_Info.IC_type == 33)//管理卡密钥不匹配
							{
								msg.mtype = 0x200;//等待刷卡界面
								memset(&Page_200.reserve1, '0', sizeof(PAGE_200_FRAME));
								Page_200.state = 34;//提示管理卡密钥不匹配
								memcpy(&Page_200.sn[0], &Globa->MUT100_Dz_Card_Info.IC_ID[0], sizeof(Page_200.sn));
								
								memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
								msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
							}
						  Init_Find_St_End();
						  Init_Find_St();
						}
					}
				}
				break;
			}
			case 0x31:{//---------------3     系统管理 选择  状态  3.1----------------
				if(msg.mtype == 0x3000)
				{//收到该界面消息
					switch(msg.mtext[0])
					{//判断 在当前界面下 用户操作的按钮
						case 0x01:
						{//实时数据 按钮
							memcpy(&misc_measure_data,&AC_measure_data.reserve1, sizeof(AC_measure_data));
							
							misc_measure_data.AC_VA[0] = Globa->ac_volt[0];//0.1V
							misc_measure_data.AC_VA[1] = Globa->ac_volt[0]>>8;
							misc_measure_data.AC_VB[0] = Globa->ac_volt[1];//0.1V
							misc_measure_data.AC_VB[1] = Globa->ac_volt[1]>>8;
							misc_measure_data.AC_VC[0] = Globa->ac_volt[2];//0.1V
							misc_measure_data.AC_VC[1] = Globa->ac_volt[2]>>8;
							misc_measure_data.AC_CA[0] = Globa->ac_current[0];//0.01A
							misc_measure_data.AC_CA[1] = Globa->ac_current[0]>>8;
							misc_measure_data.AC_CB[0] = Globa->ac_current[1];//0.01A
							misc_measure_data.AC_CB[1] = Globa->ac_current[1]>>8;
							misc_measure_data.AC_CC[0] = Globa->ac_current[2];//0.01A
							misc_measure_data.AC_CC[1] = Globa->ac_current[2]>>8;
							
							memcpy(misc_measure_data.AC_KWH, &Globa->ac_meter_kwh , 4);
							
							memcpy(misc_measure_data.DC1_KWH, &Globa->Control_DC_Infor_Data[0].meter_KWH , 4);
							memcpy(misc_measure_data.DC2_KWH, &Globa->Control_DC_Infor_Data[1].meter_KWH , 4);
							memcpy(misc_measure_data.DC3_KWH, &Globa->Control_DC_Infor_Data[2].meter_KWH , 4);
							memcpy(misc_measure_data.DC4_KWH, &Globa->Control_DC_Infor_Data[3].meter_KWH , 4);
							memcpy(misc_measure_data.DC5_KWH, &Globa->Control_DC_Infor_Data[4].meter_KWH , 4);
							
							
							memcpy(&msg.mtext[0], &misc_measure_data.reserve1, sizeof(misc_measure_data));
							msg.mtype = 0x3100;//实时数据
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(misc_measure_data), IPC_NOWAIT);

							Globa->QT_Step = 0x312;//3    系统管理 实时数据  状态  3.1.2

							QT_timer_start(500);

							break;
						}
						case 0x02:
						{//网络设置 按钮
							Page_3203.reserve1 = 1;
			
							memcpy(&Page_3203.Server_IP[0], &Globa->ISO8583_NET_setting.ISO8583_Server_IP[0], sizeof(Globa->ISO8583_NET_setting.ISO8583_Server_IP));
							memcpy(&Page_3203.port[0], &Globa->ISO8583_NET_setting.ISO8583_Server_port[0], sizeof(Globa->ISO8583_NET_setting.ISO8583_Server_port));
							memcpy(&Page_3203.addr[0], &Globa->ISO8583_NET_setting.addr[0], sizeof(Globa->ISO8583_NET_setting.addr));  					

							memcpy(&msg.mtext[0], &Page_3203.reserve1, sizeof(Page_3203));
							msg.mtype = 0x3200;
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(Page_3203), IPC_NOWAIT);
								
							Globa->QT_Step = 0x311;//3    系统管理 具体设置  状态  3.1.1
							break;
						}
						case 0x03:
						{//报警查询 按钮
							msg.mtype = 0x3300;
							msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							
							Globa->QT_Step = 0x311;//3    系统管理 具体设置  状态  3.1.1
							break;
						}
						case 0x04:
						{//USB模块 按钮
							msg.mtype = 0x3400;
							msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
								
							Globa->QT_Step = 0x311;//3    系统管理 具体设置  状态  3.1.1
							break;
						}
						case 0x05:
						{//参数设置 按钮
							Page_3503.reserve1 = 1;
							for(i=0;i<CONTROL_DC_NUMBER;i++)
								Page_3503.module_num[i] = Globa->Charger_param.Charge_rate_number[i];							

							Page_3503.rate_v[0] = (Globa->Charger_param.Charge_rate_voltage/1000)&0xff;
							Page_3503.rate_v[1] = ((Globa->Charger_param.Charge_rate_voltage/1000)>>8)&0xff; 
							Page_3503.single_rate_c = Globa->Charger_param.model_rate_current/1000;	  //单模块额定电流 1A

							memcpy(&Page_3503.SN[0], &Globa->Charger_param.SN[0], sizeof(Globa->Charger_param.SN));
							Page_3503.meter_config = Globa->Charger_param.meter_config_flag;

							Page_3503.output_over_limit[0] = (Globa->Charger_param.output_over_limit/100)&0xff;
							Page_3503.output_over_limit[1] = ((Globa->Charger_param.output_over_limit/100)>>8)&0xff;
							Page_3503.output_lown_limit[0] = (Globa->Charger_param.output_lown_limit/100)&0xff;
							Page_3503.output_lown_limit[1] = ((Globa->Charger_param.output_lown_limit/100)>>8)&0xff;

							Page_3503.input_over_limit[0] = (Globa->Charger_param.input_over_limit/100)&0xff;
							Page_3503.input_over_limit[1] = ((Globa->Charger_param.input_over_limit/100)>>8)&0xff;
							Page_3503.input_lown_limit[0] = (Globa->Charger_param.input_lown_limit/100)&0xff;
							Page_3503.input_lown_limit[1] = ((Globa->Charger_param.input_lown_limit/100)>>8)&0xff;
							for(i=0;i<CONTROL_DC_NUMBER;i++)
								Page_3503.current_limit[i] = Globa->Charger_param.current_limit[i];

							Page_3503.BMS_work = Globa->Charger_param.BMS_work;
							Page_3503.BMS_CAN_ver = Globa->Charger_param.BMS_CAN_ver;

							Page_3503.NEW_VOICE_Flag = Globa->Charger_param.NEW_VOICE_Flag;
							Page_3503.AC_Meter_CT = Globa->Charger_param.AC_Meter_CT;
							for(i=0;i<CONTROL_DC_NUMBER;i++)
							{
								Page_3503.DC_Shunt_Range[i][0] = (Globa->Charger_param.DC_Shunt_Range[i])&0xff;
								Page_3503.DC_Shunt_Range[i][1] = ((Globa->Charger_param.DC_Shunt_Range[i])>>8)&0xff;
							}
							Page_3503.charge_modlue_factory = Globa->Charger_param.charge_modlue_factory;//厂家索引
							Page_3503.charge_modlue_index = Globa->Charger_param.charge_modlue_index;
							Page_3503.AC_DAQ_POS = Globa->Charger_param.AC_DAQ_POS;
							for(i=0;i<CONTROL_DC_NUMBER;i++)
								memcpy(Page_3503.Power_meter_addr[i], Globa->Charger_param.Power_meter_addr[i], 12);
							
							Page_3503.gun_allowed_temp = (Globa->Charger_param.gun_allowed_temp)&0xff;
							Page_3503.Charging_gun_type = Globa->Charger_param.Charging_gun_type;
							Page_3503.Input_Relay_Ctl_Type = Globa->Charger_param.Input_Relay_Ctl_Type;
							Page_3503.System_Type = Globa->Charger_param.gun_config - 1;//值0索引1把枪，值1在界面显示2把枪 // Globa->Charger_param.System_Type;
							Page_3503.Serial_Network = Globa->Charger_param.Serial_Network;
							Page_3503.DTU_Baudrate = Globa->Charger_param.DTU_Baudrate;
							
							Page_3503.SOC_limit   = Globa->Charger_param.SOC_limit;
							Page_3503.min_current = Globa->Charger_param.min_current;
							Page_3503.heartbeat_idle_period   = Globa->Charger_param.heartbeat_idle_period;
							Page_3503.heartbeat_busy_period = Globa->Charger_param.heartbeat_busy_period;
							for(i=0;i<MAX_COMS;i++)
								Page_3503.COM_Func_sel[i] = Globa->Charger_param.COM_Func_sel[i];
							Page_3503.support_double_gun_to_one_car = Globa->Charger_param.support_double_gun_to_one_car;
							Page_3503.support_double_gun_power_allocation = Globa->Charger_param.support_double_gun_power_allocation ;
		          
							Page_3503.Power_Contor_By_Mode =  Globa->Charger_param.Power_Contor_By_Mode;//功率控制方式
	            Page_3503.Contactor_Line_Num = Globa->Charger_param.Contactor_Line_Num;  //系统并机数量
  						memcpy(&Page_3503.Contactor_Line_Position[0], &Globa->Charger_param.Contactor_Line_Position[0], sizeof(Globa->Charger_param.Contactor_Line_Position));// //系统并机路线节点：高4位为主控点（即控制并机的ccu）地4位为连接点

              Page_3503.CurrentVoltageJudgmentFalg =  Globa->Charger_param.CurrentVoltageJudgmentFalg; 

							memcpy(&msg.mtext[0], &Page_3503.reserve1, sizeof(Page_3503));
							msg.mtype = 0x3500;
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(Page_3503), IPC_NOWAIT);
							
							Globa->QT_Step = 0x311;//3    系统管理 具体设置  状态  3.1.1
							break;
						}
					
						case 0x06:
						{//手动充电按钮
							Page_3600.reserve1 = 1;//设置按钮值，刷新全部数据
							Page_3600.NEED_V[0] =  Globa->Charger_param.set_voltage/100;
							Page_3600.NEED_V[1] = (Globa->Charger_param.set_voltage/100)>>8;
							Page_3600.NEED_C[0] =  Globa->Charger_param.set_current/100;
							Page_3600.NEED_C[1] = (Globa->Charger_param.set_current/100)>>8;
							Page_3600.time[0]  = 0x0f;  //时间 分
							Page_3600.time[1]  = 0x27;  //默认 9999
							Page_3600.select = Globa->Charger_param.gun_config; //总共枪数，每次进入都显示第一把为先        

							Page_3600.BATT_V[0] = 0;
							Page_3600.BATT_V[1] = 0;
							Page_3600.BATT_C[0] = 0;
							Page_3600.BATT_C[1] = 0;
					  
							Page_3600.Time[0] = 0;
							Page_3600.Time[1] = 0;
							Page_3600.KWH[0]  = 0;
							Page_3600.KWH[1]  = 0;

							msg.mtype = 0x3600;//手动充电 界面

							memcpy(&msg.mtext[0], &Page_3600.reserve1, sizeof(PAGE_3600_FRAME));
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_3600_FRAME), IPC_NOWAIT);
					  
							QT_timer_start(500);
							Globa->QT_Step = 0x313; // 2 手动充电-手动充电界面 状态  2.3
							QT_gun_select = 0; 
              for(i=0;i<Globa->Charger_param.gun_config;i++)
							{
								Globa->Control_DC_Infor_Data[i].kwh = 0;
								Globa->Control_DC_Infor_Data[i].soft_KWH =0;
								Globa->Control_DC_Infor_Data[i].Time = 0;
								Globa->Control_DC_Infor_Data[i].meter_start_KWH = Globa->Control_DC_Infor_Data[0].meter_KWH;//取初始电表示数
							}
							break;
						}
						
						case 0x07:
						{//退出 按钮
							msg.mtype = 0x100;
							msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							
							Globa->QT_Step = 0x02; // 0 主界面 空闲( 可操作 )状态  0.2

							break;
						}
						case 0x08:{//费率计算
						  PAGE_3703_FRAME Page_3703;
							memset(&Page_3703,0,sizeof(PAGE_3703_FRAME));
							msg.mtype = 0x3700;
	            int i =0;
	            for(i = 0;i<4;i++){
								memcpy(&Page_3703.share_time_kwh_price[i][0],&Globa->Charger_param2.share_time_kwh_price[i],4*sizeof(char));
								memcpy(&Page_3703.share_time_kwh_price_serve[i][0],&Globa->Charger_param2.share_time_kwh_price_serve[i],4*sizeof(char));
								Page_3703.Card_reservation_price[i] = 0;
							}
							Page_3703.Stop_Car_price[0] = 0;
							Page_3703.Stop_Car_price[1] = 0;
							Page_3703.Stop_Car_price[2] = 0;
							Page_3703.Stop_Car_price[3] = 0;
							Page_3703.time_zone_num = Globa->Charger_param2.time_zone_num;
							for(i=0;i<12;i++){
							  memcpy(&Page_3703.time_zone_tabel[i][0],&Globa->Charger_param2.time_zone_tabel[i],2*sizeof(char));
						  	memcpy(&Page_3703.time_zone_feilv[i],&Globa->Charger_param2.time_zone_feilv[i],sizeof(char));
							}
          		memcpy(&msg.mtext[0], &Page_3703.reserve1, sizeof(Page_3703));
              msgsnd(Globa->arm_to_qt_msg_id, &msg,sizeof(Page_3703), IPC_NOWAIT);
						
							Globa->QT_Step = 0x311; // 0 主界面 空闲( 可操作 )状态  0.2
							break;
						}
						case 0x09:{//告警屏蔽功能
							msg.mtype = 0x7000;
							msg.mtext[0] = 0;
          		memcpy(&msg.mtext[1], &Globa->Charger_Shileld_Alarm, sizeof(Globa->Charger_Shileld_Alarm));
							msgsnd(Globa->arm_to_qt_msg_id, &msg, (sizeof(Globa->Charger_Shileld_Alarm) + 1), IPC_NOWAIT);
							Globa->QT_Step = 0x70; // 0 主界面 空闲( 可操作 )状态  0.2
    					break;
    				}
						default:
						{
							break;
						}
					}
				}

				break;
			}
			case 0x311:
			{	//--------------3    系统管理 具体设置  状态  3.1.1-----------
				switch(msg.mtype)
				{//判断当前所在界面
					case 0x3300: //系统管理 ->报警查询
					case 0x3400:
					{ //系统管理 ->USB模块
						switch(msg.mtext[0])
						{
							case 0x01:
							{  //返回上一层按钮
								msg.mtype = 0x3000;
								msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);  				

								Globa->QT_Step = 0x31; // 3     系统管理 选择  状态  3.1
		
								break;
							}
							case 0x02:
							{ //返回主页  
								msg.mtype = 0x100;
								msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);				

								Globa->QT_Step = 0x02; // 0 主界面 空闲( 可操作 )状态  0.1
			
								break;
							}
							default:
							{
								break;
							}
						}
						
						break;
					}
					case 0x3200:
					{ //系统管理 ->网络设置
						switch(msg.mtext[0])
						{
							case 0x01:
							{  //返回上一层按钮
								msg.mtype = 0x3000;
								msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);				

								Globa->QT_Step = 0x31; // 3     系统管理 选择  状态  3.1
			
								break;
							}
							case 0x02:
							{ //返回主页  
								msg.mtype = 0x100;
								msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);

								Globa->QT_Step = 0x02; // 0 主界面 空闲( 可操作 )状态  0.1
			
								break;
							}
							case 0x03:
							{ //网络设置 保存按钮
								PAGE_3203_FRAME *Page_3203_temp = (PAGE_3203_FRAME *)&msg.mtext[0];
					
								memcpy(&Globa->ISO8583_NET_setting.ISO8583_Server_IP[0], &Page_3203_temp->Server_IP[0], sizeof(Globa->ISO8583_NET_setting.ISO8583_Server_IP));
								memcpy(&Globa->ISO8583_NET_setting.ISO8583_Server_port[0], &Page_3203_temp->port[0], sizeof(Globa->ISO8583_NET_setting.ISO8583_Server_port));
								memcpy(&Globa->ISO8583_NET_setting.addr[0], &Page_3203_temp->addr[0], sizeof(Globa->ISO8583_NET_setting.addr));
	 
								printf("服务器端地址: %s\n", Globa->ISO8583_NET_setting.ISO8583_Server_IP);
								printf("服务器端口号: %d\n", Globa->ISO8583_NET_setting.ISO8583_Server_port[0]+(Globa->ISO8583_NET_setting.ISO8583_Server_port[1]<<8)); 
								printf("本机公共地址: %d\n", Globa->ISO8583_NET_setting.addr[0]+(Globa->ISO8583_NET_setting.addr[1]<<8)); 

								ISO8583_NET_setting_save();//保存网络参数
								system("reboot");

								break;
							}
							default:
							{
								break;
							}
						}

						break;
					}
					case 0x3500:
					{//系统管理 ->参数设置
						switch(msg.mtext[0])
						{
							case 0x01:
							{  //返回上一层按钮
								msg.mtype = 0x3000;
								msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);				

								Globa->QT_Step = 0x31; // 3     系统管理 选择  状态  3.1
			
								break;
							}
							case 0x02:
							{ //返回主页  
								msg.mtype = 0x100;
								msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);

								Globa->QT_Step = 0x02; // 0 主界面 空闲( 可操作 )状态  0.1
		
								break;
							}
							case 0x03:
							{ //模块设置 保存按钮
								PAGE_3503_FRAME *Page_3503_temp = (PAGE_3503_FRAME *)&msg.mtext[0];
								for(i=0;i<CONTROL_DC_NUMBER;i++)
									Globa->Charger_param.Charge_rate_number[i] = Page_3503_temp->module_num[i];
								

								Globa->Charger_param.Charge_rate_voltage = ((Page_3503_temp->rate_v[1]<<8)+Page_3503_temp->rate_v[0])*1000;	//充电机额定电压  0.001V
								Globa->Charger_param.model_rate_current = Page_3503_temp->single_rate_c*1000;
								
								for(i=0;i<CONTROL_DC_NUMBER;i++)	
									Globa->Charger_param.Charge_rate_current[i] = Globa->Charger_param.Charge_rate_number[i]*Globa->Charger_param.model_rate_current;		  //充电机1额定电流  0.001A
								
								
								Globa->Charger_param.BMS_CAN_ver = Page_3503_temp->BMS_CAN_ver;
								Globa->Charger_param.meter_config_flag = Page_3503_temp->meter_config;
								Globa->Charger_param.BMS_work = Page_3503_temp->BMS_work;

								
								Globa->Charger_param.charge_modlue_factory = Page_3503_temp->charge_modlue_factory ;//厂商编号
								Globa->Charger_param.charge_modlue_index = Page_3503_temp->charge_modlue_index;
								Globa->Charger_param.AC_DAQ_POS = Page_3503_temp->AC_DAQ_POS;

								Globa->Charger_param.output_over_limit = ((Page_3503_temp->output_over_limit[1]<<8)+Page_3503_temp->output_over_limit[0])*100;
								Globa->Charger_param.output_lown_limit = ((Page_3503_temp->output_lown_limit[1]<<8)+Page_3503_temp->output_lown_limit[0])*100;

								Globa->Charger_param.input_over_limit = ((Page_3503_temp->input_over_limit[1]<<8)+Page_3503_temp->input_over_limit[0])*100;
								Globa->Charger_param.input_lown_limit = ((Page_3503_temp->input_lown_limit[1]<<8)+Page_3503_temp->input_lown_limit[0])*100;			
								for(i=0;i<CONTROL_DC_NUMBER;i++)
									Globa->Charger_param.current_limit[i] = Page_3503_temp->current_limit[i];

								memcpy(&Globa->Charger_param.SN[0], &Page_3503_temp->SN[0], sizeof(Globa->Charger_param.SN));
						
								for(i=0;i<CONTROL_DC_NUMBER;i++)
									memcpy(Globa->Charger_param.Power_meter_addr[i],Page_3503_temp->Power_meter_addr[i], 12);
								
									
								Globa->Charger_param.Charging_gun_type = Page_3503_temp->Charging_gun_type;
								//Globa->Charger_param.Charging_gun_type = 0;//test
								Globa->Charger_param.gun_allowed_temp = Page_3503_temp->gun_allowed_temp;
								Globa->Charger_param.Input_Relay_Ctl_Type = Page_3503_temp->Input_Relay_Ctl_Type;
								//Globa->Charger_param.Input_Relay_Ctl_Type = ;//
								
								//Globa->Charger_param.System_Type = Page_3503_temp->System_Type;
								Globa->Charger_param.gun_config = Page_3503_temp->System_Type+1;//
								/*if(Globa->Charger_param.charge_modlue_index <= 1 ){
										Globa->Charger_param.charge_modlue_factory = 0;
									}else if(Globa->Charger_param.charge_modlue_index >= 6 ){
										Globa->Charger_param.charge_modlue_factory  = 1;
									}else{
										Globa->Charger_param.charge_modlue_factory = 2;
									}*/
									// if(Globa->Charger_param.charge_modlue_index <= 4 ){//重新添加R95021G1 --20KW 21A R50040G1 20kW 
										// Globa->Charger_param.charge_modlue_factory = HUAWEI_MODULE;//0; //华为模块
									// }else if((Globa->Charger_param.charge_modlue_index >= 5)&&(Globa->Charger_param.charge_modlue_index <= 8)){
										// Globa->Charger_param.charge_modlue_factory = 3; //EAST 模块
									// }else if((Globa->Charger_param.charge_modlue_index >= 9)&&(Globa->Charger_param.charge_modlue_index <= 20)){//英可瑞
										// Globa->Charger_param.charge_modlue_factory  = INCREASE_MODULE;//1;
									// }else{//英飞源(与永联模块协议一致)
										// Globa->Charger_param.charge_modlue_factory = INFY_MODULE;//2; 
									// }
									Globa->Charger_param.Serial_Network = Page_3503_temp->Serial_Network; 
									Globa->Charger_param.DTU_Baudrate = Page_3503_temp->DTU_Baudrate;
									Globa->Charger_param.NEW_VOICE_Flag = Page_3503_temp->NEW_VOICE_Flag;
									Globa->Charger_param.AC_Meter_CT = Page_3503_temp->AC_Meter_CT;
									
									Globa->Charger_param.SOC_limit = Page_3503_temp->SOC_limit;
									Globa->Charger_param.min_current = Page_3503_temp->min_current;
									Globa->Charger_param.heartbeat_idle_period = Page_3503_temp->heartbeat_idle_period;
									Globa->Charger_param.heartbeat_busy_period = Page_3503_temp->heartbeat_busy_period;
							
									for(i=0;i<MAX_COMS;i++)
										Globa->Charger_param.COM_Func_sel[i] = Page_3503_temp->COM_Func_sel[i] ;
									Globa->Charger_param.support_double_gun_to_one_car = Page_3503_temp->support_double_gun_to_one_car;
									Globa->Charger_param.support_double_gun_power_allocation = Page_3503_temp->support_double_gun_power_allocation;
									
									Globa->Charger_param.Power_Contor_By_Mode = Page_3503_temp->Power_Contor_By_Mode;//功率控制方式
	                Globa->Charger_param.Contactor_Line_Num = Page_3503_temp->Contactor_Line_Num;  //系统并机数量
  								memcpy(&Globa->Charger_param.Contactor_Line_Position[0], &Page_3503_temp->Contactor_Line_Position[0], sizeof(Globa->Charger_param.Contactor_Line_Position));// //系统并机路线节点：高4位为主控点（即控制并机的ccu）地4位为连接点
                  Globa->Charger_param.CurrentVoltageJudgmentFalg = Page_3503_temp->CurrentVoltageJudgmentFalg;
									
									
									printf("Globa->Charger_param.charge_modlue_factory %d\n",Globa->Charger_param.charge_modlue_factory); 
									printf("Globa->Charger_param.charge_modlue_index %d\n", Globa->Charger_param.charge_modlue_index); 
									//printf("Globa->Charger_param.System_Type: %d\n", Globa->Charger_param.System_Type); 
									for(i=0;i<CONTROL_DC_NUMBER;i++)
										printf("设置枪%d模块数量: %d\n", i+1,Globa->Charger_param.Charge_rate_number[i]); 
			
									printf("设置系统额定电压: %d\n", Globa->Charger_param.Charge_rate_voltage);
									printf("设置单模块额定电流: %d\n", Globa->Charger_param.model_rate_current); 
									printf("BMS VER: %d\n", Globa->Charger_param.BMS_CAN_ver); 
									
									printf("SN:");
									for(i=0;i<16;i++){
										printf("%c", Page_3503_temp->SN[i]); 
									}
									printf("\r\n");
								 
									printf("gun_config = %d\n", Globa->Charger_param.gun_config); 
									printf("meter_config_flag = %d\n", Globa->Charger_param.meter_config_flag); 
									
									for(i = 0;i<CONTROL_DC_NUMBER;i++)
									{
										printf("Meter %d:",i+1);
										for(j=0;j<12;j++)									
											printf("%c", Globa->Charger_param.Power_meter_addr[i][j]); 									
										printf("\r\n");
									}								
									
									printf("output_over_limit = %d\n", Globa->Charger_param.output_over_limit); 
									printf("output_lown_limit = %d\n", Globa->Charger_param.output_lown_limit); 
									printf("input_over_limit = %d\n", Globa->Charger_param.input_over_limit); 
									printf("input_lown_limit = %d\n", Globa->Charger_param.input_lown_limit); 					
									printf("Serial_Network = %d\n", Globa->Charger_param.Serial_Network); 		
									printf("gun_allowed_temp = %d\n", Globa->Charger_param.gun_allowed_temp); 		

									printf("DTU_Baudrate = %d\n", Globa->Charger_param.DTU_Baudrate);											
									printf("NEW_VOICE_Flag = %d\n", Globa->Charger_param.NEW_VOICE_Flag);						
								
								System_param_save();//保存系统参数
								usleep(100000);
								system("sync");
								usleep(100000);
								system("reboot");
								break;
							}
							case 0x04:
							{ //设置参数直流分流器量程
								Globa->Charger_param.DC_Shunt_Range[0] = msg.mtext[2]<<8|msg.mtext[1];
								Globa->Charger_param.DC_Shunt_Range[1] = msg.mtext[4]<<8|msg.mtext[3];
								Globa->Charger_param.DC_Shunt_Range[2] = msg.mtext[6]<<8|msg.mtext[5];
								Globa->Charger_param.DC_Shunt_Range[3] = msg.mtext[8]<<8|msg.mtext[7];
								Globa->Charger_param.DC_Shunt_Range[4] = msg.mtext[10]<<8|msg.mtext[9];
								if((Globa->Charger_param.gun_config == 2)&&(Globa->Charger_param.support_double_gun_power_allocation ==1))
								{
									gun_config = 4;
									for(i=0;i<gun_config;i++)
									{	
										Dc_shunt_Set_CL_flag[i] = 1;							
									}
									
									if(Globa->Charger_param.meter_config_flag == DCM3366Q)
									{
										Dc_shunt_Set_meter_flag[0] = 1;		
										Dc_shunt_Set_meter_flag[1] = 1;							
										Dc_shunt_Set_meter_flag[2] = 0;		
										Dc_shunt_Set_meter_flag[3] = 0;//只有两个电表
									}
									
								}else{
									for(i=0;i<gun_config;i++)
									{	
										if(Globa->Charger_param.meter_config_flag == DCM3366Q)
										{
											Dc_shunt_Set_meter_flag[i] = 1;		
										}										
										Dc_shunt_Set_CL_flag[i] = 1;							
									}
								}
								sleep(5);//
								
								msg.mtype = 0x93;
								msg.mtext[0] = 1;
								int res=0;
								for(i=0;i<gun_config;i++)
								{
									res |= Dc_shunt_Set_meter_flag[i];//值0表示各电表均已下发分流器量程
									res |= Dc_shunt_Set_CL_flag[i];
									printf("Dc_shunt_Set_meter_flag[%d]=%d,Dc_shunt_Set_CL_flag[%d]=%d\n",
											i,Dc_shunt_Set_meter_flag[i],
											i,Dc_shunt_Set_CL_flag[i]);
								}
								printf("res = %d\n",res);
								//if res==1,one is not done
								msg.mtext[1] = 1 - res;
								//if(msg.mtext[1] == 1)	//控制单元和电表均已使用新的分流器量程					
								System_param_save();//保存系统参数						
								msgsnd(Globa->arm_to_qt_msg_id, &msg, 2, IPC_NOWAIT);
						
								for(i=0;i<gun_config;i++)//超时后取消设置，避免电表那里没收到正常响应时，未重启前一直设置
								{	
									Dc_shunt_Set_meter_flag[i] = 0;							
									Dc_shunt_Set_CL_flag[i] = 0;							
								}								
							  break;
							}
					  
							case 0x05:
							{ //修改M1密钥
								Globa->Charger_param.Key[0] = 0xFF;
								Globa->Charger_param.Key[1] = 0xFF;
								Globa->Charger_param.Key[2] = 0xFF;
								Globa->Charger_param.Key[3] = 0xFF;
								Globa->Charger_param.Key[4] = 0xFF;
								Globa->Charger_param.Key[5] = 0xFF;
								System_param_save();//保存系统参数
								usleep(100000);
								system("sync");
								break;
							}
							default:
							{
								break;
							}
						}
						break;
					}
  				
					case 0x3700:{//系统参数--费率设置
						switch(msg.mtext[0]){
      				case 0x01:{  //返回上一层按钮
   							msg.mtype = 0x3000;
      					msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);				
      					Globa->QT_Step = 0x31; // 3     系统管理 选着  状态  3.1
      					break;
      				}
      				case 0x02:{ //返回主页  
      					msg.mtype = 0x100;
      					msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
      					Globa->QT_Step = 0x02; // 0 主界面 空闲( 可操作 )状态  0.1
      					break;
      				}
      				case 0x03:{ //保存按钮
  							PAGE_3703_FRAME *Page_3703_temp = (PAGE_3703_FRAME *)&msg.mtext[0];
      					if(Globa->have_hart != 1){ //与后台无通讯
								 for(i = 0;i<4;i++){
										memcpy(&Globa->Charger_param2.share_time_kwh_price[i],&Page_3703_temp->share_time_kwh_price[i][0],4*sizeof(char));
										memcpy(&Globa->Charger_param2.share_time_kwh_price_serve[i],&Page_3703_temp->share_time_kwh_price_serve[i][0],4*sizeof(char));
										//Page_3703.Card_reservation_price[i] = 0;
										//Page_3703.Stop_Car_price[i] = 0;
									
									}
									Globa->Charger_param2.time_zone_num = Page_3703_temp->time_zone_num;

									for(i=0;i<12;i++){
										memcpy(&Globa->Charger_param2.time_zone_tabel[i],&Page_3703_temp->time_zone_tabel[i][0],2*sizeof(char));
										memcpy(&Globa->Charger_param2.time_zone_feilv[i],&Page_3703_temp->time_zone_feilv[i],sizeof(char));
									}
								  memset(&Globa->Charger_param2.price_type_ver[0], '9', sizeof(Globa->Charger_param2.price_type_ver));

									System_param2_save();
								  usleep(200000);
								  system("sync");
									msg.mtype = 0x3000;
									msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);  				
									Globa->QT_Step = 0x31; // 3     系统管理 选着  状态  3.1
								}
      					break;
      				}
						}
					}
					default:
					{
						break;
					}
				}
				break;
			}
			case 0x312:{//--------------3    系统管理 实时数据  状态  3.1.2-----------
				if(msg.mtype == 0x3100)
				{//判断当前所在界面
					switch(msg.mtext[0])
					{
						case 0x01:
						{  //返回上一层按钮
							QT_timer_stop(500);
							msg.mtype = 0x3000;
							msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);  				

							Globa->QT_Step = 0x31; // 3     系统管理 选择  状态  3.1
	  
							break;
						}
						case 0x02:
						{ //返回主页 
							QT_timer_stop(500); 

							msg.mtype = 0x100;
							msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);				

							Globa->QT_Step = 0x02; // 0 主界面 空闲( 可操作 )状态  0.1
  
							break;
						}
						default:
						{
							break;
						}
					}
				}else if(QT_timer_tick(500) == 1)
				{
					memcpy(&msg.mtext[0], &AC_measure_data.reserve1, sizeof(AC_measure_data));
					msg.mtype = 0x3100;//实时数据
					msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(AC_measure_data), IPC_NOWAIT);
					
					QT_timer_start(500);
				}

				break;
			}
		 	case 0x313:
			{//---------------2 手动充电-》 手动充电界面       2.3----------
				if(Globa->Control_DC_Infor_Data[QT_gun_select].Manu_start != 0)
				Globa->Control_DC_Infor_Data[QT_gun_select].kwh = (Globa->Control_DC_Infor_Data[QT_gun_select].meter_KWH - Globa->Control_DC_Infor_Data[QT_gun_select].meter_start_KWH);
				
				if(msg.mtype == 0x3600)
				{//收到该界面消息
					switch(msg.mtext[0])
					{//判断 在当前界面下 用户操作的按钮
						case 0x01:
						{//设置按钮
							Globa->Charger_param.set_voltage = ((msg.mtext[2]<<8)+msg.mtext[1])*100;
							Globa->Charger_param.set_current = ((msg.mtext[4]<<8)+msg.mtext[3])*100;

							Globa->QT_charge_time = (msg.mtext[6]<<8)+msg.mtext[5];  //时间    分
							System_param_save();//保存系统参数
							QT_gun_select = msg.mtext[7];    //充电枪选择 1：充电枪1 2: 充电枪2

							printf("手动设置电压: %d\n", Globa->Charger_param.set_voltage);
							printf("手动设置电流: %d\n", Globa->Charger_param.set_current); 
							printf("预设充电时间: %d\n", Globa->QT_charge_time);
							printf("启动抢号: %d\n", QT_gun_select);

							//界面保持不变，不回应数据给界面 
		
							break;
						}   
						case 0x02:
						{//开始充电按钮
							Globa->Control_DC_Infor_Data[QT_gun_select].Manu_start = 2;
							Globa->Control_DC_Infor_Data[QT_gun_select].charger_start = 2;
							
							Globa->Control_DC_Infor_Data[QT_gun_select].charger_state = 0x04;
							break;
						}
						case 0x03:{//停止充电按钮
								Globa->Control_DC_Infor_Data[QT_gun_select].Manu_start = 0;

							break;
						}
						case 0x04:{//退出 按钮
								Globa->Control_DC_Infor_Data[QT_gun_select].Manu_start = 0;
								msg.mtype = 0x3000;
								msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);					
								Globa->QT_Step = 0x31;
								Globa->Control_DC_Infor_Data[QT_gun_select].charger_state = 0x03;

								break;
						}
						default:{
							break;
						}
					}

					break;//必须退出，不然下面同时要数新数据的话，会导致界面无法返回
				}

				if(QT_timer_tick(500) == 1)
				{
					//定时刷新数据
					Page_3600.reserve1 = 2;//只刷新实时数据
					Page_3600.NEED_V[0] =  Globa->Charger_param.set_voltage/100;
					Page_3600.NEED_V[1] = (Globa->Charger_param.set_voltage/100)>>8;
					Page_3600.NEED_C[0] =  Globa->Charger_param.set_current/100;
					Page_3600.NEED_C[1] = (Globa->Charger_param.set_current/100)>>8;
					Page_3600.time[0] = Globa->QT_charge_time;
					Page_3600.time[1] = Globa->QT_charge_time>>8;
					Page_3600.select = QT_gun_select; //Globa->QT_gun_select;    //充电枪选择 1：充电枪1 2: 充电枪2

					Page_3600.BATT_V[0] = Globa->Control_DC_Infor_Data[0].Output_voltage/100;
					Page_3600.BATT_V[1] = (Globa->Control_DC_Infor_Data[0].Output_voltage/100)>>8;
					Page_3600.BATT_C[0] = Globa->Control_DC_Infor_Data[0].Output_current/100;
					Page_3600.BATT_C[1] = (Globa->Control_DC_Infor_Data[0].Output_current/100)>>8;
					//ul_temp = Globa->Control_DC_Infor_Data[0].kwh;
					ul_temp = Globa->Control_DC_Infor_Data[0].soft_KWH;
					Page_3600.Time[0] = Globa->Control_DC_Infor_Data[0].Time;
					Page_3600.Time[1] = Globa->Control_DC_Infor_Data[0].Time>>8;
					Page_3600.KWH[0] =  ul_temp;
					Page_3600.KWH[1] =  ul_temp>>8;

					msg.mtype = 0x3600;
					memcpy(&msg.mtext[0], &Page_3600.reserve1, sizeof(PAGE_3600_FRAME));
					msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_3600_FRAME), IPC_NOWAIT);
					
					QT_timer_start(500);
					
					break;
				}

				break;
			}
			case 0x41://---------------4   查看帮助 状态  4.1 -----------------------
			case 0x42:
			{//->关于	
				if((msg.mtype == 0x4000)||(msg.mtype == 0x2000))
				{//收到该界面消息
				  if(msg.mtype == 0x2000)
				  {
						if(msg.mtext[0] == 3)
						{
							msg.mtype = 0x3300;
							msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa->QT_Step = 0x311;//3    系统管理 具体设置  状态  3.1.1
							break;
						}else if(msg.mtext[0] == 4)
						{//进入参数设置界面
							msg.mtype = 0x3000;//系统管理界面
							msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa->QT_Step = 0x31; // 系统管理界面
							count_wait = 0;
							break;
						}
					}
					msg.mtype = 0x100;
					msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
					Globa->QT_Step = 0x02; // 0 主界面 空闲( 可操作 )状态  0.1
				}
				break;
			}
			case 0x43:
			{//系统忙暂停使用
				if(Globa->sys_restart_flag)
				{
					system("sync");
					system("reboot");
				}
				if(msg.mtype == 0x5000)
				{
					if(msg.mtext[0] == 1)
					{
						msg.mtype = 0x3000;//系统管理界面
						msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa->QT_Step = 0x31; // 系统管理界面
						count_wait = 0;				
						break;
					}
				}

				//if(Globa->Electric_pile_workstart_Enable == 1)//桩使能
                if (Globa->Electric_pile_workstart_Enable[0] == 1)//桩使能  // todo: 由邓工改一下枪禁用QT这边的处理
				{ 
					msg.mtype = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选择界面，才有此按钮功能）
					msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
					Globa->QT_Step = 0x02; //主界面
					//防止按键处理不同步，此处同步一下
					msg.mtype = 0x150;//
					msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
					Globa->QT_Step = 0x03; 
					break;
				}
				break;
			}
			case 0x50:
			{//查询余额还是卡解锁
				count_wait++;
				if(count_wait > 9000)//
				{//等待时间超时90S
					count_wait = 0;
					msg.mtype = 0x100;
					msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
					Globa->QT_Step = 0x02; // 0 主界面 空闲( 可操作 )状态  0.1
					memset(&Globa->Control_DC_Infor_Data[0].card_sn[0], '0', sizeof(Globa->Control_DC_Infor_Data[0].card_sn));
					break;//超时必须退出
				}
				if(msg.mtype == 0x5300)
				{//判断是否为   主界面
					switch(msg.mtext[0])
					{//判断 在当前界面下 用户操作的按钮
						case 0x01:
						{//卡余额查询
							msg.mtype = 0x200;//等待刷卡界面
							memset(&Page_200.reserve1, '0', sizeof(PAGE_200_FRAME));
							Page_200.state = 1;//1 没有检测到用户卡
							memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
							count_wait = 0;
							Globa->QT_Step = 0x51; // 
							
							Globa->Control_DC_Infor_Data[0].MT625_Card_Start_ok_flag = 0;//set to 0避免启动了充电
							Globa->Control_DC_Infor_Data[0].User_Card_check_flag = 0;//test
							Globa->Control_DC_Infor_Data[0].User_Card_check_result = 0;//
							Globa->Control_DC_Infor_Data[0].User_Card_Present = 0;
							Init_Find_St();		
							if(Globa->Charger_param.NEW_VOICE_Flag == 1)
							{
								Voicechip_operation_function(0x04);//04H     “请刷卡”
							}else
							{
								Voicechip_operation_function(0x00);//00H	"欢迎光临.wav 300s 请刷卡1.wav"
							}
							break;
						}
						case 0x02:
						{ //支付卡解锁功能
							
								msg.mtype = 0x5400;//解锁界面
								memset(&Page_5200.reserve1, '0', sizeof(PAGE_5200_FRAME));
								Page_5200.Result_Type = 0;    	
								memset(&Page_5200.card_lock_time, '0', sizeof(Page_5200.card_lock_time));
								memset(&Page_5200.Deduct_money, '0', sizeof(Page_5200.Deduct_money));
								memset(&Page_5200.busy_SN, '0', sizeof(Page_5200.busy_SN));
								memcpy(&msg.mtext[0], &Page_5200.reserve1, sizeof(PAGE_5200_FRAME));
								msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_5200_FRAME), IPC_NOWAIT);
								
								count_wait = 0;
								memset(&Globa->User_Card_Lock_Info, 0, sizeof(Globa->User_Card_Lock_Info));
								Globa->User_Card_Lock_Info.Unlock_card_setp = 0;
								Globa->User_Card_Lock_Info.Card_unlock_Query_Results = 0;
								Globa->User_Card_Lock_Info.User_card_unlock_Requestfalg = 0;
								Globa->User_Card_Lock_Info.User_card_unlock_Data_Requestfalg = 0;	
								Globa->QT_Step = 0x52; // 2 支付卡解锁功能
			
								if(Globa->Charger_param.NEW_VOICE_Flag == 1)
								{
									Voicechip_operation_function(0x04);//04H     “请刷卡”
								}else
								{
									Voicechip_operation_function(0x00);//00H	"欢迎光临.wav 300s 请刷卡1.wav"
								}
								Init_Find_St();//开始搜索卡
														
							break;
						}
						case 0x00:
						{//返回主界面
							msg.mtype = 0x100;
							msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa->QT_Step = 0x02; // 0 主界面 空闲( 可操作 )状态  0.1
							memset(&Globa->Control_DC_Infor_Data[0].card_sn[0], '0', sizeof(Globa->Control_DC_Infor_Data[0].card_sn));
							break;
						}
					}
				}
				break;	
			}
			case 0x51:
			{//->查看卡余额	
				count_wait++;
				if(count_wait > 1000)
				{//等待时间超时10S
					count_wait = 0;
					msg.mtype = 0x5300;//帮助界面
					msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
					Globa->QT_Step = 0x50; // 支付卡查询余额/解锁
					Globa->Control_DC_Infor_Data[0].User_Card_Present =  0;
					Init_Find_St_End();
					Globa->Control_DC_Infor_Data[0].User_Card_check_flag = 0;//停止鉴权
					break;//超时必须退出
				}
				if(msg.mtype == 0x200)
				{
					if(msg.mtext[0] == 0x02)//返回按钮
					{	
						count_wait = 0;
						msg.mtype = 0x5300;//帮助界面
						msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa->QT_Step = 0x50; // 支付卡查询余额/解锁
						Globa->Control_DC_Infor_Data[0].User_Card_Present =  0;
						Init_Find_St_End();
						Globa->Control_DC_Infor_Data[0].User_Card_check_flag = 0;//停止鉴权
						break;//
					}
				}
				if(Globa->Control_DC_Infor_Data[0].User_Card_Present ==  0)
				{
					if(IsFind_Idle_OK() == 1)
					{//读出卡片信息
						if(valid_card_check(&Globa->MUT100_Dz_Card_Info.IC_ID[0]) > 0)
						{//黑名单
							printf("卡号已列入本地黑名单中!\n");
							msg.mtype = 0x200;//等待刷卡界面
							Page_200.state = 0x46;//7;//卡片已注销====黑名单
							memcpy(&Page_200.sn[0], &Globa->MUT100_Dz_Card_Info.IC_ID[0], sizeof(Page_200.sn));
							memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
							if(Globa->Charger_param.NEW_VOICE_Flag == 1)
							{
							  Voicechip_operation_function(0x0c);//0cH     “卡片已挂失，请联系发卡单位处理”
							}
							sleep(3);
							msg.mtype = 0x5300;//帮助界面
							msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Init_Find_St_End();	
							Globa->QT_Step = 0x50; // 支付卡查询余额/解锁
							count_wait = 0;
							break;
						}
						
						memcpy(Globa->Control_DC_Infor_Data[0].card_sn,Globa->MUT100_Dz_Card_Info.IC_ID,16);
						
						if((Globa->MUT100_Dz_Card_Info.IC_type == 1)||
						   (Globa->MUT100_Dz_Card_Info.IC_type == 2)
						)
						{//卡片状态 1--用户卡 2-员工卡 3-管理员卡4-非充电卡 
							Globa->Control_DC_Infor_Data[0].last_rmb = Globa->MUT100_Dz_Card_Info.IC_money;//获取卡片内当前余额
							//load 卡特殊标记
							Globa->Control_DC_Infor_Data[0].qiyehao_flag = Globa->MUT100_Dz_Card_Info.qiyehao_flag;
							Globa->Control_DC_Infor_Data[0].private_price_flag = Globa->MUT100_Dz_Card_Info.private_price_flag ;
							Globa->Control_DC_Infor_Data[0].order_chg_flag = Globa->MUT100_Dz_Card_Info.order_chg_flag ;
							Globa->Control_DC_Infor_Data[0].offline_chg_flag = Globa->MUT100_Dz_Card_Info.offline_chg_flag ;
								
							if((Globa->MUT100_Dz_Card_Info.IC_type == 1)&&(Globa->MUT100_Dz_Card_Info.IC_state == 0x31))//用户卡灰锁
							{
								Page_200.state = 4;	//用户卡状态异常					
								msg.mtype = 0x200;//等待刷卡界面
								memcpy(&Page_200.sn[0], &Globa->MUT100_Dz_Card_Info.IC_ID[0], sizeof(Page_200.sn));
								sprintf(&temp_char[0], "%08d",Globa->Control_DC_Infor_Data[0].last_rmb);				
								memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
								
								memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
								msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
								
								sleep(3);//时间可调整
								count_wait = 0;
								msg.mtype = 0x5300;//帮助界面
								msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
								Globa->QT_Step = 0x50; // 支付卡查询余额/解锁
								Init_Find_St_End();
								break;
								
							}else//用户卡状态正常或员工卡
							{	
								Globa->Control_DC_Infor_Data[0].User_Card_Present =  1;
								if(Globa->have_hart == 1)
								{//在线运行----卡片进行校验
									if( (Globa->Control_DC_Infor_Data[0].User_Card_check_result == 0)&&
										(Globa->Control_DC_Infor_Data[0].User_Card_check_flag == 0)
										)
									{
										msg.mtype = 0x200;//等待刷卡界面
										Page_200.state = 12;//卡片验证中...请保持刷卡状态！
										memcpy(&Page_200.sn[0], &Globa->MUT100_Dz_Card_Info.IC_ID[0], sizeof(Page_200.sn));
										sprintf(&temp_char[0], "%08d",Globa->Control_DC_Infor_Data[0].last_rmb);				
										memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
										
										memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
										msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
										memcpy(&Globa->Control_DC_Infor_Data[0].User_Card_check_card_ID[0], &Globa->MUT100_Dz_Card_Info.IC_ID[0], 16);
										Globa->Control_DC_Infor_Data[0].User_Card_check_flag = 1;
										break;
									}
								}
								else
								{//离线查询
									Globa->Control_DC_Infor_Data[0].User_Card_check_flag = 0;								
									Globa->Control_DC_Infor_Data[0].User_Card_check_result = 2;							
								}
							}
						}
						else//不可充电的卡
						{
							Page_200.state = 8;	//不可充电的卡					
							msg.mtype = 0x200;//等待刷卡界面
							memcpy(&Page_200.sn[0], &Globa->MUT100_Dz_Card_Info.IC_ID[0], sizeof(Page_200.sn));
							sprintf(&temp_char[0], "%08d",0);//Globa->MUT100_Dz_Card_Info.IC_money);				
							memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
							
							memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
							
							sleep(3);//时间可调整
							count_wait = 0;
							msg.mtype = 0x5300;//帮助界面
							msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa->QT_Step = 0x50; // 支付卡查询余额/解锁
							Init_Find_St_End();
							break;
						}						
					}
				}
				if(Globa->Control_DC_Infor_Data[0].User_Card_Present ==  1)//已检测到卡片
				{
					
					if((Globa->Control_DC_Infor_Data[0].User_Card_check_result > 3))		
					{//解读服务器验证结果
						Globa->Control_DC_Infor_Data[0].User_Card_check_flag = 0;
						msg.mtype = 0x200;//等待刷卡界面
						//User_Card_check_result值如下	
						//4--该卡未进行发卡 5--无效充电桩序列号 6--黑名单 7--卡的企业号标记不同步 8--卡的私有电价标记不同步
						//9--卡的有序充电标记不同步  10--非规定时间充电卡 11--企业号余额不足 12--专用时间其他卡不可充
						//13--企业号启动达到限定桩数				
						Page_200.state = (Globa->Control_DC_Infor_Data[0].User_Card_check_result )&0xff;//只取低字节
						if(Page_200.state < 0x99)//User_Card_check_result 0x5099是系统错误
							Page_200.state |= 0x40;//增加偏移
						memcpy(&Page_200.sn[0], &Globa->MUT100_Dz_Card_Info.IC_ID[0], sizeof(Page_200.sn));
						sprintf(&temp_char[0], "%08d", Globa->Control_DC_Infor_Data[0].last_rmb);	
						memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
						memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
						msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
						
						sleep(3);//时间可调整
						count_wait = 0;
						msg.mtype = 0x5300;//帮助界面
						msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa->QT_Step = 0x50; // 支付卡查询余额/解锁
						Init_Find_St_End();
						break;//
					}else if((Globa->Control_DC_Infor_Data[0].User_Card_check_result == 2)||(Globa->Control_DC_Infor_Data[0].User_Card_check_result == 1))//离线刷卡或鉴权OK
					{
						msg.mtype = 0x200;//等待刷卡界面
						if(Globa->Control_DC_Infor_Data[0].qiyehao_flag == 0xAA)
							Page_200.state = 17;
						else
							if(Globa->MUT100_Dz_Card_Info.IC_type == 1)//用户卡
								Page_200.state = 5;//显示用户卡和卡片余额
							else//员工卡
								Page_200.state = 6;
						memcpy(&Page_200.sn[0], &Globa->MUT100_Dz_Card_Info.IC_ID[0], sizeof(Page_200.sn));
						sprintf(&temp_char[0], "%08d", Globa->Control_DC_Infor_Data[0].last_rmb);						
						memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
						memcpy(&msg.mtext[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
						msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
						
						sleep(3);//时间可调整
						count_wait = 0;
						msg.mtype = 0x5300;//帮助界面
						msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa->QT_Step = 0x50; // 支付卡查询余额/解锁
						Init_Find_St_End();
							
						break;					
					}
					
				}
				break;
			}
		
			case 0x52:
			{//支付卡解锁功能
				if(msg.mtype == 0x5400)
				{//判断是否为   主界面
					switch(msg.mtext[0])
					{//判断 在当前界面下 用户操作的按钮
						case 0x00:
						{//退出界面
							count_wait = 0;
							msg.mtype = 0x5300;//帮助界面
							msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa->QT_Step = 0x50; // 支付卡查询余额/解锁
						  break;
						}
						case 0x01:
						if(Globa->User_Card_Lock_Info.Card_unlock_Query_Results == 1)//
						{//得到数据之后进行解锁
							msg.mtype = 0x5400;//解锁界面
							memset(&Page_5200.reserve1, '0', sizeof(PAGE_5200_FRAME));
							Page_5200.Result_Type = 8;     //结果类型 0--请刷卡,1--卡片正常,2--数据核对查询当中,3--查询成功显示从后台或本地获取的信息（显示相应的数据，其他的都隐藏），4--查询成功未找到充电记录 5-查询失败--卡号无效 ，6--查询失败--无效充电桩序列号 7，MAC 出差或终端为未对时	8,支付卡解锁中9,解锁完成
							memset(&Page_5200.card_lock_time, '0', sizeof(Page_5200.card_lock_time));
							memset(&Page_5200.Deduct_money, '0', sizeof(Page_5200.Deduct_money));
							memset(&Page_5200.busy_SN, '0', sizeof(Page_5200.busy_SN));
							memcpy(&msg.mtext[0], &Page_5200.reserve1, sizeof(PAGE_5200_FRAME));
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_5200_FRAME), IPC_NOWAIT);
							Globa->QT_Step = 0x53; // 2 支付卡解锁功能
							
							send_card_info_data.Card_Command = 2;
							send_card_info_data.Command_Result = 0;
							send_card_info_data.Command_IC_money = Globa->User_Card_Lock_Info.Deduct_money;//待扣金额
						  //send_card_info_data.Last_IC_money = 0;
							Globa->User_Card_Lock_Info.User_card_unlock_Data_Requestfalg = 0;
							Init_Power_Off_End();
							Init_Card_PowerOff_St(&send_card_info_data);//卡片解锁处理

							Globa->User_Card_Lock_Info.Unlock_card_setp = 0;  //处于解锁步骤
							if(Globa->Charger_param.NEW_VOICE_Flag == 1)
							{
								Voicechip_operation_function(0x04);//04H     “请刷卡”
							}else
							{
								Voicechip_operation_function(0x00);//00H	"欢迎光临.wav 300s 请刷卡1.wav"
							}
							break;
						}
					}
				  break;
				}
				if(0 == Globa->User_Card_Lock_Info.Unlock_card_setp)
				{
					if(IsFind_Idle_OK() == 1)
					{//卡片查询完毕
						if(Globa->MUT100_Dz_Card_Info.IC_Read_OK == 1)
						{//读出卡片信息
							if(Globa->MUT100_Dz_Card_Info.IC_state == 0x31)//锁卡
							{
								printf("-------卡片查询完毕,卡片灰锁----\n");
								send_card_info_data.Last_IC_money = Globa->MUT100_Dz_Card_Info.IC_money;
								memcpy(&send_card_info_data.IC_ID[0], &Globa->MUT100_Dz_Card_Info.IC_ID[0], sizeof(Globa->MUT100_Dz_Card_Info.IC_ID));
								memcpy(&Globa->User_Card_Lock_Info.card_sn[0], &Globa->MUT100_Dz_Card_Info.IC_ID[0], sizeof(Globa->MUT100_Dz_Card_Info.IC_ID));//解锁卡号
								Globa->User_Card_Lock_Info.Unlock_card_setp = 1;//等待解锁
								Globa->User_Card_Lock_Info.Card_unlock_Query_Results = 0;
								
								int result;
								{
									pthread_mutex_lock(&busy_db_pmutex);
									result = Select_Unlock_Info(); 
									pthread_mutex_unlock(&busy_db_pmutex);
								}
								unlock_local = 0;
								if(result == 0)//本地有锁卡记录
								{
									Globa->User_Card_Lock_Info.Card_unlock_Query_Results = 1; //查询成功有记录
									unlock_local = 1;//使用本地记录解锁
								}
								else
								{
									if(Globa->have_hart == 1)//在线
										Globa->User_Card_Lock_Info.User_card_unlock_Requestfalg = 1;//进行在线解锁申请
									else
										Globa->User_Card_Lock_Info.Card_unlock_Query_Results = 4;//无记录
								}
								Init_Find_St_End();
								
							}else
							{
								printf("-------卡片查询完毕卡片正常----\n");
								msg.mtype = 0x5400;//解锁界面
								memset(&Page_5200.reserve1, '0', sizeof(PAGE_5200_FRAME));
								memset(&Page_5200.card_lock_time, '0', sizeof(Page_5200.card_lock_time));
								memset(&Page_5200.Deduct_money, '0', sizeof(Page_5200.Deduct_money));
								memset(&Page_5200.busy_SN, '0', sizeof(Page_5200.busy_SN));
								Page_5200.Result_Type = 1;     //结果类型 0--请刷卡,1--卡片正常,2--数据核对查询当中,3--查询成功显示从后台或本地获取的信息（显示相应的数据，其他的都隐藏），4--查询成功未找到充电记录 5-查询失败--卡号无效 ，6--查询失败--无效充电桩序列号 7，MAC 出差或终端为未对时	
								memcpy(&msg.mtext[0], &Page_5200.reserve1, sizeof(PAGE_5200_FRAME));
								msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_5200_FRAME), IPC_NOWAIT);
								sleep(2);
								count_wait = 0;
								msg.mtype = 0x5300;//帮助界面
								msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
								Globa->QT_Step = 0x50; // 支付卡查询余额/解锁
								Init_Find_St_End();
								break;
							}
						}
					}
				}
				if(Globa->User_Card_Lock_Info.Unlock_card_setp == 1)
				{
					if(Globa->User_Card_Lock_Info.Card_unlock_Query_Results == 1)
					{//查询成功有记录--显示数据出来
						msg.mtype = 0x5400;//解锁界面
						memset(&Page_5200.reserve1, '0', sizeof(PAGE_5200_FRAME));
						Page_5200.Result_Type = 3;     
						memcpy(&Page_5200.card_lock_time, &Globa->User_Card_Lock_Info.card_lock_time, sizeof(Page_5200.card_lock_time));
						Page_5200.Deduct_money[0] = Globa->User_Card_Lock_Info.Deduct_money&0xFF; 
						Page_5200.Deduct_money[1] = Globa->User_Card_Lock_Info.Deduct_money>>8; 
					//	memcpy(&Page_5200.Deduct_money, &Globa->User_Card_Lock_Info.Deduct_money, sizeof(Page_5200.Deduct_money));
						memcpy(&Page_5200.busy_SN, &Globa->User_Card_Lock_Info.busy_SN, sizeof(Page_5200.busy_SN));
						memcpy(&msg.mtext[0], &Page_5200.reserve1, sizeof(PAGE_5200_FRAME));
						msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_5200_FRAME), IPC_NOWAIT);
						break;
					}else if(Globa->User_Card_Lock_Info.Card_unlock_Query_Results == 4)
					{//未找到记录
						msg.mtype = 0x5400;//解锁界面
						memset(&Page_5200.reserve1, '0', sizeof(PAGE_5200_FRAME));
						Page_5200.Result_Type = 4;    
						memset(&Page_5200.card_lock_time, '0', sizeof(Page_5200.card_lock_time));
						memset(&Page_5200.Deduct_money, '0', sizeof(Page_5200.Deduct_money));
						memset(&Page_5200.busy_SN, '0', sizeof(Page_5200.busy_SN));
						memcpy(&msg.mtext[0], &Page_5200.reserve1, sizeof(PAGE_5200_FRAME));
						msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_5200_FRAME), IPC_NOWAIT);
						if(Globa->Charger_param.NEW_VOICE_Flag == 1){
							Voicechip_operation_function(0x12);//12H     “卡片解锁失败，请联系发卡单位处理“
						}
						sleep(3);				
						msg.mtype = 0x5300;//帮助界面
						msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa->QT_Step = 0x50; // 支付卡查询余额/解锁
						break;
					}else if(Globa->User_Card_Lock_Info.Card_unlock_Query_Results == 2)
					{//失败，卡号无效
						msg.mtype = 0x5400;//解锁界面
						memset(&Page_5200.reserve1, '0', sizeof(PAGE_5200_FRAME));
						Page_5200.Result_Type = 5;     
						memset(&Page_5200.card_lock_time, '0', sizeof(Page_5200.card_lock_time));
						memset(&Page_5200.Deduct_money, '0', sizeof(Page_5200.Deduct_money));
						memset(&Page_5200.busy_SN, '0', sizeof(Page_5200.busy_SN));
						memcpy(&msg.mtext[0], &Page_5200.reserve1, sizeof(PAGE_5200_FRAME));
						msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_5200_FRAME), IPC_NOWAIT);
						if(Globa->Charger_param.NEW_VOICE_Flag == 1){
							Voicechip_operation_function(0x12);//12H     “卡片解锁失败，请联系发卡单位处理“
						}
						sleep(3);				
						msg.mtype = 0x5300;//帮助界面
						msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa->QT_Step = 0x50; // 支付卡查询余额/解锁
						break;
					}else if(Globa->User_Card_Lock_Info.Card_unlock_Query_Results == 3)
					{//无效充电桩序列号
						msg.mtype = 0x5400;//解锁界面
						memset(&Page_5200.reserve1, '0', sizeof(PAGE_5200_FRAME));
						Page_5200.Result_Type = 6;     
						memset(&Page_5200.card_lock_time, '0', sizeof(Page_5200.card_lock_time));
						memset(&Page_5200.Deduct_money, '0', sizeof(Page_5200.Deduct_money));
						memset(&Page_5200.busy_SN, '0', sizeof(Page_5200.busy_SN));
						memcpy(&msg.mtext[0], &Page_5200.reserve1, sizeof(PAGE_5200_FRAME));
						msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_5200_FRAME), IPC_NOWAIT);
						if(Globa->Charger_param.NEW_VOICE_Flag == 1){
							Voicechip_operation_function(0x12);//12H     “卡片解锁失败，请联系发卡单位处理“
						}
						sleep(3);				
						msg.mtype = 0x5300;//帮助界面
						msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa->QT_Step = 0x50; // 支付卡查询余额/解锁
						break;
					}else if(Globa->User_Card_Lock_Info.Card_unlock_Query_Results == 5)
					{//MAC 出差或终端为未对时
						msg.mtype = 0x5400;//解锁界面
						memset(&Page_5200.reserve1, '0', sizeof(PAGE_5200_FRAME));
						Page_5200.Result_Type = 7;     
						memset(&Page_5200.Deduct_money, '0', sizeof(Page_5200.Deduct_money));
						memset(&Page_5200.busy_SN, '0', sizeof(Page_5200.busy_SN));
						memcpy(&msg.mtext[0], &Page_5200.reserve1, sizeof(PAGE_5200_FRAME));
						msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_5200_FRAME), IPC_NOWAIT);
						if(Globa->Charger_param.NEW_VOICE_Flag == 1){
							Voicechip_operation_function(0x12);//12H     “卡片解锁失败，请联系发卡单位处理“
						}
						sleep(3);				
						msg.mtype = 0x5400;//帮助界面
						msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa->QT_Step = 0x50; // 支付卡查询余额/解锁
						break;
					}else
					{
						msg.mtype = 0x5400;//解锁界面
						memset(&Page_5200.reserve1, '0', sizeof(PAGE_5200_FRAME));
						Page_5200.Result_Type = 2;     
						memset(&Page_5200.Deduct_money, '0', sizeof(Page_5200.Deduct_money));
						memset(&Page_5200.busy_SN, '0', sizeof(Page_5200.busy_SN));
						memcpy(&msg.mtext[0], &Page_5200.reserve1, sizeof(PAGE_5200_FRAME));
						msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_5200_FRAME), IPC_NOWAIT);
					}
				}
				
				count_wait++;
				if(count_wait > 1000)//10秒超时返回
				{//返回主界面
					count_wait = 0;
					msg.mtype = 0x100;
					msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
					Globa->QT_Step = 0x02; // 0 主界面 空闲( 可操作 )状态  0.1
					memset(&Globa->Control_DC_Infor_Data[0].card_sn[0], '0', sizeof(Globa->Control_DC_Infor_Data[0].card_sn));
					break;
				}				
			}
			break;
			case 0x53://执行解锁
			{
					if(msg.mtype == 0x5400)
					{//判断是否为   主界面
						switch(msg.mtext[0])
						{//判断 在当前界面下 用户操作的按钮
							case 0x00:{//退出界面
							count_wait = 0;
							msg.mtype = 0x5300;//帮助界面
							msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa->QT_Step = 0x50; // 支付卡查询余额/解锁
							break;
							}
						}
					}
				
					if(IsCard_PwrOff_OK())
					{
						if(IsCard_PwrOff_Result() == 1)
						{
							msg.mtype = 0x5400;//解锁界面
							memset(&Page_5200.reserve1, 0, sizeof(PAGE_5200_FRAME));
							Page_5200.Result_Type = 10;    	
							memset(&Page_5200.card_lock_time, 0, sizeof(Page_5200.card_lock_time));
							memset(&Page_5200.Deduct_money, 0, sizeof(Page_5200.Deduct_money));
							memset(&Page_5200.busy_SN, '0', sizeof(Page_5200.busy_SN));
							memcpy(&msg.mtext[0], &Page_5200.reserve1, sizeof(PAGE_5200_FRAME));
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_5200_FRAME), IPC_NOWAIT);
							if(Globa->Charger_param.NEW_VOICE_Flag == 1){
								Voicechip_operation_function(0x11);//11H     “卡片解锁成功”
							}
							if(unlock_local)
								Set_Unlock_flag(Globa->User_Card_Lock_Info.Local_Data_Baseid_ID, 3);//值3锁卡记录上送前本地解锁
							else
								//如果是在线解锁需上报
								Globa->User_Card_Lock_Info.User_card_unlock_Data_Requestfalg = 1;//解锁完成，上报解锁完成报文
							
							sleep(3);				
							msg.mtype = 0x5300;//帮助界面
							msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa->QT_Step = 0x50; // 支付卡查询余额/解锁
							Init_Power_Off_End();
							break;
						}else
						{
							msg.mtype = 0x5400;//解锁界面
							memset(&Page_5200.reserve1, 0, sizeof(PAGE_5200_FRAME));
							Page_5200.Result_Type = 9;    	
							memset(&Page_5200.card_lock_time, 0, sizeof(Page_5200.card_lock_time));
							memset(&Page_5200.Deduct_money, 0, sizeof(Page_5200.Deduct_money));
							memset(&Page_5200.busy_SN, '0', sizeof(Page_5200.busy_SN));
							memcpy(&msg.mtext[0], &Page_5200.reserve1, sizeof(PAGE_5200_FRAME));
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_5200_FRAME), IPC_NOWAIT);
							if(Globa->Charger_param.NEW_VOICE_Flag == 1){
								Voicechip_operation_function(0x12);//12H     “卡片解锁失败，请联系发卡单位处理“
							}
							sleep(3);				
							msg.mtype = 0x5300;//帮助界面
							msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa->QT_Step = 0x50; // 支付卡查询余额/解锁
							Init_Power_Off_End();
							break;
						}
					}
				break;
			}
		
			case 0x60:{//检测到升级包提示用户是否进行升级		
				if(msg.mtype == 0x6000)
				{
					//判断是否为   主界面
					switch(msg.mtext[0])
					{//判断 在当前界面下 用户操作的按钮
						case 0x01:
						{//进行升级处理
							msg.mtype = 0x6000;					
							Page_6100.reserve1 = 0;        //保留
							Page_6100.Display_prompt = 1;        //显示进度条等数据

							Page_6100.file_number = gun_config;//4;          //下发文件数量
							Page_6100.file_no = 1;          //当前下发文件
							Page_6100.file_Bytes[0]= 0;    //总字节数
							Page_6100.file_Bytes[1]= 0;    //总字节数
							Page_6100.file_Bytes[2]= 0;    //总字节数
							Page_6100.sendfile_Bytes[0]= 0;          //成功下发的字节数/
							Page_6100.sendfile_Bytes[1]= 0;          //成功下发的字节数/
							Page_6100.sendfile_Bytes[2]= 0;           //成功下发的字节数/
							Page_6100.over_flag[0] = 0;      //控制板1,2,升级步骤标志  0--等待升级 1--下发升级中 2--下发完成（进行结果判定）
							Page_6100.over_flag[1] = 0;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
							Page_6100.over_flag[2] = 0;
							Page_6100.over_flag[3] = 0;
							Page_6100.over_flag[4] = 0;
							Page_6100.info_success[0] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
							Page_6100.info_success[1] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
							Page_6100.info_success[2] = 0;
							Page_6100.info_success[3] = 0;
							Page_6100.info_success[4] = 0;
							
							memcpy(&msg.mtext[0], &Page_6100.reserve1, sizeof(PAGE_6100_FRAME));
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_6100_FRAME), IPC_NOWAIT);
							if(Globa->Charger_param.NEW_VOICE_Flag == 1){
							  Voicechip_operation_function(0x13);//13H    “系统维护中，暂停使用 ”
							}
							Globa->QT_Step = 0x61; //退出到主界面
							MC_timer_start(0,5000);
							MC_timer_start(0,250);
							count_wait = 0;
							Upgrade_Firmware_time = 0;
							for(i=0;i<Page_6100.file_number;i++)
								Globa->Control_Upgrade_Firmware_Info[i].firmware_info_setStep = 0x00;
							
							
							Globa->Control_Upgrade_Firmware_Info[0].firmware_info_setflg = 1;
							if(Page_6100.file_number >= 2)
								Globa->Control_Upgrade_Firmware_Info[1].firmware_info_setflg = 1;
							if(Page_6100.file_number >= 3)
								Globa->Control_Upgrade_Firmware_Info[2].firmware_info_setflg = 1;
							if(Page_6100.file_number >= 4)
								Globa->Control_Upgrade_Firmware_Info[3].firmware_info_setflg = 1;
							if(Page_6100.file_number >= 5)
								Globa->Control_Upgrade_Firmware_Info[4].firmware_info_setflg = 1;
							break;
						}
						case 0x00:
					case 0x02:
					  {
						  //取消升级处理,不删除升级文件
							for(i=0;i<gun_config;i++)
							{
								Globa->Control_Upgrade_Firmware_Info[i].firmware_info_setStep = 0x00;
								Globa->Control_Upgrade_Firmware_Info[i].firmware_info_setflg = 0;
							}
							
							msg.mtype = 0x100;//退出结束界面
							msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);				
							Globa->QT_Step = 0x02; //退出到主界面
							count_wait = 0;
					   	break;
						}
					default:
						break;
					}
				}
				
				count_wait++;
				if(count_wait > 2000)
				{//等待时间超时20S ---按默认升级处理
					count_wait = 0;
					/* msg.mtype = 0x100;//退出结束界面
					msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);				
					Globa->QT_Step = 0x02; //退出到主界面
					*/
					msg.mtype = 0x6000;					
					Page_6100.reserve1 = 0;        //保留
					Page_6100.Display_prompt = 1;        //显示进度条等数据
		
					Page_6100.file_number = gun_config;//4;
					Page_6100.file_no = 1;          //当前下发文件
					Page_6100.file_Bytes[0]= 0;    //总字节数
					Page_6100.file_Bytes[1]= 0;    //总字节数
					Page_6100.file_Bytes[2]= 0;    //总字节数
					Page_6100.sendfile_Bytes[0]= 0;          //成功下发的字节数/
					Page_6100.sendfile_Bytes[1]= 0;          //成功下发的字节数/
					Page_6100.sendfile_Bytes[2]= 0;          //成功下发的字节数/
					Page_6100.over_flag[0] = 0;      //控制板1,2,升级步骤标志  0--等待升级 1--下发升级中 2--下发完成（进行结果判定）
					Page_6100.over_flag[1] = 0;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
					Page_6100.over_flag[2] = 0;
					Page_6100.over_flag[3] = 0;
					Page_6100.over_flag[4] = 0;
					Page_6100.info_success[0] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
					Page_6100.info_success[1] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
					Page_6100.info_success[2] = 0;
					Page_6100.info_success[3] = 0;
					Page_6100.info_success[4] = 0;

					memcpy(&msg.mtext[0], &Page_6100.reserve1, sizeof(PAGE_6100_FRAME));
					msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_6100_FRAME), IPC_NOWAIT);
					if(Globa->Charger_param.NEW_VOICE_Flag == 1){
					  Voicechip_operation_function(0x13);//13H    “系统维护中，暂停使用 ”
					}
					Globa->QT_Step = 0x61; 
					MC_timer_start(0,5000);
					MC_timer_start(0,250);
					count_wait = 0;
					Upgrade_Firmware_time = 0;
					for(i=0;i<Page_6100.file_number;i++)
						Globa->Control_Upgrade_Firmware_Info[i].firmware_info_setStep = 0x00;
					
					Globa->Control_Upgrade_Firmware_Info[0].firmware_info_setflg = 1;
					if(Page_6100.file_number >= 2)
						Globa->Control_Upgrade_Firmware_Info[1].firmware_info_setflg = 1;
					if(Page_6100.file_number >= 3)
						Globa->Control_Upgrade_Firmware_Info[2].firmware_info_setflg = 1;
					if(Page_6100.file_number >= 4)
						Globa->Control_Upgrade_Firmware_Info[3].firmware_info_setflg = 1;
					if(Page_6100.file_number >= 5)
						Globa->Control_Upgrade_Firmware_Info[4].firmware_info_setflg = 1;
				 
					break;//
				}
				break;
			}
			case 0x61:{//程序下发升级包过程
				count_wait++;
				int upgrade_busy = 0;
				int cur_upgrade_plug = 1;//当前升级文件下发的控制板
				int upgrade_failed = 0;
	
				for(i=0;i<gun_config;i++)
				{	
					if(Globa->Control_Upgrade_Firmware_Info[i].firmware_info_setStep != 0x10)
					{
						upgrade_busy = 1;//升级中
						cur_upgrade_plug = i+1;
						break;
					}
				}
				// if((Globa->Control_Upgrade_Firmware_Info[0].firmware_info_setStep != 0x10)||
					// (Globa->Control_Upgrade_Firmware_Info[1].firmware_info_setStep != 0x10)||
					// (Globa->Control_Upgrade_Firmware_Info[2].firmware_info_setStep != 0x10)||
					// (Globa->Control_Upgrade_Firmware_Info[3].firmware_info_setStep != 0x10)
					// )
				if(upgrade_busy)
				{
					if(count_wait > 100)
					{//等待时间超时1S  ----每1S发送一次
						count_wait = 0;
						msg.mtype = 0x6000;					
						Page_6100.reserve1 = 0;        //保留
						Page_6100.Display_prompt = 1;        //显示进度条等数据
			
						Page_6100.file_number  = gun_config;//4;
						
					//	Page_6100.file_number = Globa->Charger_param.gun_config;//4;
						// if(Globa->Control_Upgrade_Firmware_Info[0].firmware_info_setStep != 0x10){
							// Page_6100.file_no = 1;          //当前下发文件
						// }else if(Globa->Control_Upgrade_Firmware_Info[1].firmware_info_setStep != 0x10){
							// Page_6100.file_no = 2;          //当前下发文件
						// }else if(Globa->Control_Upgrade_Firmware_Info[2].firmware_info_setStep != 0x10){
							// Page_6100.file_no = 3;          //当前下发文件
						// }else if(Globa->Control_Upgrade_Firmware_Info[3].firmware_info_setStep != 0x10){
							// Page_6100.file_no = 4;          //当前下发文件
						// }
						Page_6100.file_no = cur_upgrade_plug;
						if(Page_6100.file_no <= CONTROL_DC_NUMBER)
						{
							Page_6100.file_Bytes[0]= Globa->Control_Upgrade_Firmware_Info[Page_6100.file_no - 1].Firmware_ALL_Bytes;    //总字节数
							Page_6100.file_Bytes[1]= Globa->Control_Upgrade_Firmware_Info[Page_6100.file_no - 1].Firmware_ALL_Bytes>>8;    //总字节数
							Page_6100.file_Bytes[2]= Globa->Control_Upgrade_Firmware_Info[Page_6100.file_no - 1].Firmware_ALL_Bytes>>16;    //总字节数
							Page_6100.sendfile_Bytes[0]= Globa->Control_Upgrade_Firmware_Info[Page_6100.file_no - 1].Send_Tran_All_Bytes_0K;          //成功下发的字节数/
							Page_6100.sendfile_Bytes[1]= Globa->Control_Upgrade_Firmware_Info[Page_6100.file_no - 1].Send_Tran_All_Bytes_0K>>8;          //成功下发的字节数/
							Page_6100.sendfile_Bytes[2]= Globa->Control_Upgrade_Firmware_Info[Page_6100.file_no - 1].Send_Tran_All_Bytes_0K>>16; //成功下发的字节数/
							
							if(Page_6100.file_no == 1)
							{
								Page_6100.over_flag[0] = 1;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[1] = 0;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[2] = 0;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[3] = 0;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[4] = 0;
								Page_6100.info_success[0] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[1] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[2] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[3] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[4] = 0;
							
							}else if(Page_6100.file_no == 2)
							{		
								Page_6100.over_flag[0] = 2;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[1] = 1;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[2] = 0;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[3] = 0;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[4] = 0; 
								Page_6100.info_success[0] = Globa->Control_Upgrade_Firmware_Info[0].firmware_info_success;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[1] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[2] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[3] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[4] = 0;
							}else if(Page_6100.file_no == 3){
								
								Page_6100.over_flag[0] = 2;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[1] = 2;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[2] = 1;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[3] = 0;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[4] = 0;	
								Page_6100.info_success[0] = Globa->Control_Upgrade_Firmware_Info[0].firmware_info_success;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[1] = Globa->Control_Upgrade_Firmware_Info[1].firmware_info_success;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[2] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[3] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[4] = 0;
							}else if(Page_6100.file_no == 4){
								
								Page_6100.over_flag[0] = 2;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[1] = 2;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[2] = 2;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[3] = 1;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[4] = 0;
								Page_6100.info_success[0] = Globa->Control_Upgrade_Firmware_Info[0].firmware_info_success;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[1] = Globa->Control_Upgrade_Firmware_Info[1].firmware_info_success;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[2] = Globa->Control_Upgrade_Firmware_Info[2].firmware_info_success;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[3] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[4] = 0;
							}else if(Page_6100.file_no == 5){//第5个控制板
								
								Page_6100.over_flag[0] = 2;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[1] = 2;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[2] = 2;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[3] = 2;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[4] = 1;
								Page_6100.info_success[0] = Globa->Control_Upgrade_Firmware_Info[0].firmware_info_success;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[1] = Globa->Control_Upgrade_Firmware_Info[1].firmware_info_success;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[2] = Globa->Control_Upgrade_Firmware_Info[2].firmware_info_success;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[3] = Globa->Control_Upgrade_Firmware_Info[3].firmware_info_success;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[4] = 0;
								
							}
							memcpy(&msg.mtext[0], &Page_6100.reserve1, sizeof(PAGE_6100_FRAME));
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_6100_FRAME), IPC_NOWAIT);
						}
					}
				}else//all done
				{
					msg.mtype = 0x6000;					
					Page_6100.reserve1 = 0;        //保留
					Page_6100.Display_prompt = 2;        //显示进度条等数据并且通知结果
			
					Page_6100.file_number = gun_config;
					
					Page_6100.file_Bytes[0]= Globa->Control_Upgrade_Firmware_Info[0].Firmware_ALL_Bytes;    //总字节数
					Page_6100.file_Bytes[1]= Globa->Control_Upgrade_Firmware_Info[0].Firmware_ALL_Bytes>>8;    //总字节数
					Page_6100.file_Bytes[2]= Globa->Control_Upgrade_Firmware_Info[0].Firmware_ALL_Bytes>>16;    //总字节数
					Page_6100.sendfile_Bytes[0]= Globa->Control_Upgrade_Firmware_Info[Page_6100.file_number-1].Send_Tran_All_Bytes_0K;          //成功下发的字节数/
					Page_6100.sendfile_Bytes[1]= Globa->Control_Upgrade_Firmware_Info[Page_6100.file_number-1].Send_Tran_All_Bytes_0K>>8;          //成功下发的字节数/
					Page_6100.sendfile_Bytes[2]= Globa->Control_Upgrade_Firmware_Info[Page_6100.file_number-1].Send_Tran_All_Bytes_0K>>16;           //成功下发的字节数/
					
					for(i=0;i<Page_6100.file_number;i++)
						Page_6100.over_flag[i] = 2; //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
					// Page_6100.over_flag[0] = 2;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
					// Page_6100.over_flag[1] = 2;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
					// Page_6100.over_flag[2] = 2;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
					// Page_6100.over_flag[3] = 2;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
					for(i=0;i<Page_6100.file_number;i++)
						Page_6100.info_success[i] = Globa->Control_Upgrade_Firmware_Info[i].firmware_info_success;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
					
					// Page_6100.info_success[0] = Globa->Control_Upgrade_Firmware_Info[0].firmware_info_success;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
					// Page_6100.info_success[1] = Globa->Control_Upgrade_Firmware_Info[1].firmware_info_success;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
					// Page_6100.info_success[2] = Globa->Control_Upgrade_Firmware_Info[2].firmware_info_success;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
					// Page_6100.info_success[3] = Globa->Control_Upgrade_Firmware_Info[3].firmware_info_success;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
				
					memcpy(&msg.mtext[0], &Page_6100.reserve1, sizeof(PAGE_6100_FRAME));
					msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_6100_FRAME), IPC_NOWAIT);
					sleep(4);
					
					if(Upgrade_Firmware_time == 0)
					{//着手升级第二次，
						// if((Page_6100.info_success[0] == 1)||
							// (Page_6100.info_success[1] == 1)||
							// (Page_6100.info_success[2] == 1)||
							// (Page_6100.info_success[3] == 1))
						upgrade_failed = 0;
						for(i=0;i<Page_6100.file_number;i++)
						{
							if(1==Page_6100.info_success[i])//failed
							{
								upgrade_failed = 1;
								Page_6100.file_no = i+1;//从第1个失败的开始升----
								break;
							}
						}
							
						if(upgrade_failed)
						{
							Upgrade_Firmware_time = 1;
							Page_6100.Display_prompt = 3;        //提示是否重新升级
							memcpy(&msg.mtext[0], &Page_6100.reserve1, sizeof(PAGE_6100_FRAME));
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_6100_FRAME), IPC_NOWAIT);
							Globa->QT_Step = 0x62; 
							count_wait = 0;
						}else
						{
							system("rm -rf /mnt/Nand1/update/EASE_DC_CTL.bin");
							system("echo Upgrade ok >> /mnt/systemlog&& date >> /mnt/systemlog");
							system("reboot");
						}
					}else//第2次升级了
					{
						// if((Page_6100.info_success[0] == 1)||
							// (Page_6100.info_success[1] == 1)||
							// (Page_6100.info_success[2] == 1)||
							// (Page_6100.info_success[3] == 1))
						upgrade_failed = 0;
						for(i=0;i<Page_6100.file_number;i++)
							if(1==Page_6100.info_success[i])//failed
								upgrade_failed = 1;
						if(upgrade_failed)
						{
							system("rm -rf /mnt/Nand1/update/EASE_DC_CTL.bin");
							system("echo Upgrade failed >> /mnt/systemlog&& date >> /mnt/systemlog");
							system("reboot");
						}else
						{
							system("rm -rf /mnt/Nand1/update/EASE_DC_CTL.bin");
							system("echo Upgrade ok >> /mnt/systemlog&& date >> /mnt/systemlog");
							system("reboot");
						}
					}
			  }
			  break;	
			}
			case 0x62:{//结果判定，是否进行第二次升级
				count_wait++;
				if(count_wait > 2000)//超时取消升级
				{
					system("rm -rf /mnt/Nand1/update/EASE_DC_CTL.bin");
					system("echo Upgrade failed >> /mnt/systemlog&& date >> /mnt/systemlog");
					system("reboot");
					break;
				}
				if(msg.mtype == 0x6000)
				{//判断是否为   主界面
					switch(msg.mtext[0])
					{//判断 在当前界面下 用户操作的按钮
    				case 0x03:{//进行再次升级
							msg.mtype = 0x6000;					
							Page_6100.reserve1 = 0;        //保留
							Page_6100.Display_prompt = 1;        //显示进度条等数据
         
							Page_6100.file_number = gun_config;//4;
							Page_6100.file_Bytes[0]= 0;    //总字节数
							Page_6100.file_Bytes[1]= 0;    //总字节数
							Page_6100.file_Bytes[2]= 0;    //总字节数
							Page_6100.sendfile_Bytes[0]= 0;          //成功下发的字节数/
							Page_6100.sendfile_Bytes[1]= 0;          //成功下发的字节数/
							Page_6100.sendfile_Bytes[2]= 0;           //成功下发的字节数/
							
							if(Page_6100.file_no == 1){
								Page_6100.over_flag[0] = 0;      //控制板1,2,升级步骤标志  0--等待升级 1--下发升级中 2--下发完成（进行结果判定）
								Page_6100.over_flag[1] = 0;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[2] = 0;      //控制板1,2,升级步骤标志  0--等待升级 1--下发升级中 2--下发完成（进行结果判定）
								Page_6100.over_flag[3] = 0;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[4] = 0;
								Page_6100.info_success[0] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[1] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[2] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[3] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[4] = 0;
						  }else if(Page_6100.file_no == 2){
								Page_6100.over_flag[0] = 2;      //控制板1,2,升级步骤标志  0--等待升级 1--下发升级中 2--下发完成（进行结果判定）
								Page_6100.over_flag[1] = 0;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[2] = 0;      //控制板1,2,升级步骤标志  0--等待升级 1--下发升级中 2--下发完成（进行结果判定）
								Page_6100.over_flag[3] = 0;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[4] = 0;
								Page_6100.info_success[0] = 2;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[1] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[2] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[3] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[4] = 0;
						  }else if(Page_6100.file_no == 3){
								Page_6100.over_flag[0] = 2;      //控制板1,2,升级步骤标志  0--等待升级 1--下发升级中 2--下发完成（进行结果判定）
								Page_6100.over_flag[1] = 2;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[2] = 0;      //控制板1,2,升级步骤标志  0--等待升级 1--下发升级中 2--下发完成（进行结果判定）
								Page_6100.over_flag[3] = 0;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[4] = 0;
								Page_6100.info_success[0] = 2;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[1] = 2;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[2] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[3] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[4] = 0;
						  }else if(Page_6100.file_no == 4){
								Page_6100.over_flag[0] = 2;      //控制板1,2,升级步骤标志  0--等待升级 1--下发升级中 2--下发完成（进行结果判定）
								Page_6100.over_flag[1] = 2;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[2] = 2;      //控制板1,2,升级步骤标志  0--等待升级 1--下发升级中 2--下发完成（进行结果判定）
								Page_6100.over_flag[3] = 0;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[4] = 0;
								Page_6100.info_success[0] = 2;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[1] = 2;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[2] = 2;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[3] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[4] = 0;
						  }else if(Page_6100.file_no == 5){
								Page_6100.over_flag[0] = 2;      //控制板1,2,升级步骤标志  0--等待升级 1--下发升级中 2--下发完成（进行结果判定）
								Page_6100.over_flag[1] = 2;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[2] = 2;      //控制板1,2,升级步骤标志  0--等待升级 1--下发升级中 2--下发完成（进行结果判定）
								Page_6100.over_flag[3] = 2;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.over_flag[4] = 0;
								Page_6100.info_success[0] = 2;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[1] = 2;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[2] = 2;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[3] = 2;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[4] = 0;
						  }
							
							memcpy(&msg.mtext[0], &Page_6100.reserve1, sizeof(PAGE_6100_FRAME));
							msgsnd(Globa->arm_to_qt_msg_id, &msg, sizeof(PAGE_6100_FRAME), IPC_NOWAIT);
							
						if(Globa->Charger_param.NEW_VOICE_Flag == 1){
							  Voicechip_operation_function(0x13);//13H    “系统维护中，暂停使用 ”
							}
							Globa->QT_Step = 0x61; //退出到主界面
							MC_timer_start(0,5000);
							MC_timer_start(0,250);
							count_wait = 0;
					
							for(i=Page_6100.file_no;i<=Page_6100.file_number;i++)
								Globa->Control_Upgrade_Firmware_Info[i-1].firmware_info_setStep = 0x00;
							
							
							Globa->Control_Upgrade_Firmware_Info[0].firmware_info_setflg = 1;
							if(Page_6100.file_number >= 2)
								Globa->Control_Upgrade_Firmware_Info[1].firmware_info_setflg = 1;
							if(Page_6100.file_number >= 3)
								Globa->Control_Upgrade_Firmware_Info[2].firmware_info_setflg = 1;
							if(Page_6100.file_number >= 4)
								Globa->Control_Upgrade_Firmware_Info[3].firmware_info_setflg = 1;
							if(Page_6100.file_number >= 5)
								Globa->Control_Upgrade_Firmware_Info[4].firmware_info_setflg = 1;
							for(i=0;i<(Page_6100.file_no-1);i++)//已升级过的不再升级
								Globa->Control_Upgrade_Firmware_Info[i].firmware_info_setflg = 0;
							
					   	break;
						}
						case 0x04:{
						  system("rm -rf /mnt/Nand1/update/EASE_DC_CTL.bin");
						  system("echo Upgrade failed >> /mnt/systemlog&& date >> /mnt/systemlog");
						  system("reboot");
							break;
						}
				  }
				}
				break;
			}
		  case 0x70:{//调试参数界面
				if(msg.mtype == 0x7000){//判断是否为   主界面
    			switch(msg.mtext[0]){//判断 在当前界面下 用户操作的按钮
    				case 0x02:{
							PAGE_7000_FRAME *Page_7000_temp = (PAGE_7000_FRAME *)&msg.mtext[0];
						  if(0 != memcmp(&Globa->Charger_Shileld_Alarm, &Page_7000_temp->Charger_Shileld_Alarm, sizeof(Globa->Charger_Shileld_Alarm))){
								memcpy(&Globa->Charger_Shileld_Alarm,&Page_7000_temp->Charger_Shileld_Alarm, (sizeof(Globa->Charger_Shileld_Alarm)));
							  System_Charger_Shileld_Alarm_save();
								usleep(200000);
								system("sync");
							  system("reboot");
							}
							break;
					  }
						case 0x00:{
						  msg.mtype = 0x100;//退出结束界面
							msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);				
							Globa->QT_Step = 0x02; //退出到主界面
							count_wait = 0;
					   	break;
						}
						case 0x01:{//返回上一层
							msg.mtype = 0x3000;
      				msgsnd(Globa->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);  				
      				Globa->QT_Step = 0x31; // 3     系统管理 选着  状态  3.1
					   	break;
						}
					}
				}
				break;
			}
			default:{
				printf("操作界面进入未知界面状态!!!!!!!!!!!!!!\n");
				break;
			}
		}
	}
}
