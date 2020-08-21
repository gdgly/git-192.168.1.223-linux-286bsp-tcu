/****************************************Copyright (c)**************************
**                               DongGuang EAST Co.,LTD.
**
**                                 http://www.EASTUPS.com
**
**--------------File Info-------------------------------------------------------
** File Name:               tcp_modbus.c
** Last modified Date:      2012-02-10 9:26:00
** Last Version:            1.0
** Description:             TCP网络MODBUS数据的解析，解析，封包程序     
** 
**------------------------------------------------------------------------------
**TCP_MODBUS_Parsing()
**网络MODBUS数据接收及发送程序
**------------------------------------------------------------------------------
**TCP_DATA_Parsing()
**网络解析及封包程序
*******************************************************************************/

//#define DEBUG_MODBUS_TCP 	// uncomment to see the data sent and received

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <termio.h>
#include <sys/time.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <pthread.h>
#include <sys/shm.h> 
#include "globalvar.h"
#include "tcp_modbus.h"
#include "iso8583.h"

#define MODBUS_TCP_PORT 502      // the port users will be connecting to
#define BACKLOG 20               // how many pending connections queue will hold
#define MAXCLIENT 32              // allow connections number
#define MAX_IDLECONNCTIME 5      // how many idle time connections will be closed,minite

#define BUF_SIZE 255


#define COMMS_FAILURE          0
#define ILLEGAL_FUNCTION       1
#define ILLEGAL_DATA_ADDRESS   2
#define ILLEGAL_DATA_VALUE     3
#define SLAVE_DEVICE_FAILURE   4


static void Modbus_Fault_response(unsigned int fd, unsigned char *msg, unsigned char err_num)
{
	msg[EXMB_fun] = msg[EXMB_fun] + 0x80;
	msg[EXMB_state] =  err_num;
	msg[MBAP_length+1] =  0x03;
	
	send(fd, msg, msg[MBAP_length+1] + MBAP_unit, 0);//发送数据
}
//更新遥测寄存器变量
void mb_func04hook(void)
{
	unsigned long ul_temp,i,j,reg_offset;
	
	Globa->modbus_485_PC04[0] = Globa->ac_volt[0];//0.1v
	Globa->modbus_485_PC04[1] = Globa->ac_volt[1];//0.1v
	Globa->modbus_485_PC04[2] = Globa->ac_volt[2];//0.1v
	Globa->modbus_485_PC04[3] = Globa->ac_current[0];//0.01A
	Globa->modbus_485_PC04[4] = Globa->ac_current[1];//0.01A
	Globa->modbus_485_PC04[5] = Globa->ac_current[2];//0.01A
	Globa->modbus_485_PC04[6] = Globa->ac_meter_kwh>>16;//0.01KWH
	Globa->modbus_485_PC04[7] = Globa->ac_meter_kwh;
	Globa->modbus_485_PC04[8] = (Globa->Control_DC_Infor_Data[0].meter_KWH/METER_READ_MULTIPLE)>>16;//0.01KWH
	Globa->modbus_485_PC04[9] = (Globa->Control_DC_Infor_Data[0].meter_KWH/METER_READ_MULTIPLE);
	Globa->modbus_485_PC04[10] = (Globa->Control_DC_Infor_Data[1].meter_KWH/METER_READ_MULTIPLE)>>16;
	Globa->modbus_485_PC04[11] = (Globa->Control_DC_Infor_Data[1].meter_KWH/METER_READ_MULTIPLE);
	Globa->modbus_485_PC04[12] = (Globa->Control_DC_Infor_Data[2].meter_KWH/METER_READ_MULTIPLE)>>16;
	Globa->modbus_485_PC04[13] = (Globa->Control_DC_Infor_Data[2].meter_KWH/METER_READ_MULTIPLE);
	Globa->modbus_485_PC04[14] = (Globa->Control_DC_Infor_Data[3].meter_KWH/METER_READ_MULTIPLE)>>16;
	Globa->modbus_485_PC04[15] = (Globa->Control_DC_Infor_Data[3].meter_KWH/METER_READ_MULTIPLE);
	Globa->modbus_485_PC04[16] = (Globa->Control_DC_Infor_Data[4].meter_KWH/METER_READ_MULTIPLE)>>16;
	Globa->modbus_485_PC04[17] = (Globa->Control_DC_Infor_Data[4].meter_KWH/METER_READ_MULTIPLE);
		
	Globa->modbus_485_PC04[18] = Globa->Control_DC_Infor_Data[0].Output_voltage>>16;
	Globa->modbus_485_PC04[19] = Globa->Control_DC_Infor_Data[0].Output_voltage;//充电机输出电压 0.001V
	Globa->modbus_485_PC04[20] = Globa->Control_DC_Infor_Data[0].Output_current>>16;
	Globa->modbus_485_PC04[21] = Globa->Control_DC_Infor_Data[0].Output_current;//充电机输出电流 0.001A
	
	Globa->modbus_485_PC04[22] = Globa->Control_DC_Infor_Data[0].need_voltage>>16;
	Globa->modbus_485_PC04[23] = Globa->Control_DC_Infor_Data[0].need_voltage;//电压需求  0.001V
	Globa->modbus_485_PC04[24] = Globa->Control_DC_Infor_Data[0].need_current>>16;
	Globa->modbus_485_PC04[25] = Globa->Control_DC_Infor_Data[0].need_current;//电流需求  0.001A
	
	Globa->modbus_485_PC04[26] = Globa->Control_DC_Infor_Data[0].gun_link;		
	Globa->modbus_485_PC04[27] = Globa->Control_DC_Infor_Data[1].gun_link;	
	Globa->modbus_485_PC04[28] = Globa->Control_DC_Infor_Data[2].gun_link;	
	Globa->modbus_485_PC04[29] = Globa->Control_DC_Infor_Data[3].gun_link;	
	Globa->modbus_485_PC04[30] = Globa->Control_DC_Infor_Data[4].gun_link;	
	
	
	Globa->modbus_485_PC04[31] = Globa->Control_DC_Infor_Data[1].Output_voltage>>16;
	Globa->modbus_485_PC04[32] = Globa->Control_DC_Infor_Data[1].Output_voltage;//充电机输出电压 0.001V
	Globa->modbus_485_PC04[33] = Globa->Control_DC_Infor_Data[1].Output_current>>16;
	Globa->modbus_485_PC04[34] = Globa->Control_DC_Infor_Data[1].Output_current;//充电机输出电流 0.001A
	
	Globa->modbus_485_PC04[35] = Globa->Control_DC_Infor_Data[1].need_voltage>>16;
	Globa->modbus_485_PC04[36] = Globa->Control_DC_Infor_Data[1].need_voltage;//电压需求  0.001V
	Globa->modbus_485_PC04[37] = Globa->Control_DC_Infor_Data[1].need_current>>16;
	Globa->modbus_485_PC04[38] = Globa->Control_DC_Infor_Data[1].need_current;//电流需求  0.001A
	
	Globa->modbus_485_PC04[39] = Globa->Control_DC_Infor_Data[2].Output_voltage>>16;
	Globa->modbus_485_PC04[40] = Globa->Control_DC_Infor_Data[2].Output_voltage;//充电机输出电压 0.001V
	Globa->modbus_485_PC04[41] = Globa->Control_DC_Infor_Data[2].Output_current>>16;
	Globa->modbus_485_PC04[42] = Globa->Control_DC_Infor_Data[2].Output_current;//充电机输出电流 0.001A
	
	Globa->modbus_485_PC04[43] = Globa->Control_DC_Infor_Data[2].need_voltage>>16;
	Globa->modbus_485_PC04[44] = Globa->Control_DC_Infor_Data[2].need_voltage;//电压需求  0.001V
	Globa->modbus_485_PC04[45] = Globa->Control_DC_Infor_Data[2].need_current>>16;
	Globa->modbus_485_PC04[46] = Globa->Control_DC_Infor_Data[2].need_current;//电流需求  0.001A
	
	Globa->modbus_485_PC04[47] = Globa->Control_DC_Infor_Data[3].Output_voltage>>16;
	Globa->modbus_485_PC04[48] = Globa->Control_DC_Infor_Data[3].Output_voltage;//充电机输出电压 0.001V
	Globa->modbus_485_PC04[49] = Globa->Control_DC_Infor_Data[3].Output_current>>16;
	Globa->modbus_485_PC04[50] = Globa->Control_DC_Infor_Data[3].Output_current;//充电机输出电流 0.001A
	
	Globa->modbus_485_PC04[51] = Globa->Control_DC_Infor_Data[3].need_voltage>>16;
	Globa->modbus_485_PC04[52] = Globa->Control_DC_Infor_Data[3].need_voltage;//电压需求  0.001V
	Globa->modbus_485_PC04[53] = Globa->Control_DC_Infor_Data[3].need_current>>16;
	Globa->modbus_485_PC04[54] = Globa->Control_DC_Infor_Data[3].need_current;//电流需求  0.001A
	
	Globa->modbus_485_PC04[55] = Globa->Control_DC_Infor_Data[4].Output_voltage>>16;
	Globa->modbus_485_PC04[56] = Globa->Control_DC_Infor_Data[4].Output_voltage;//充电机输出电压 0.001V
	Globa->modbus_485_PC04[57] = Globa->Control_DC_Infor_Data[4].Output_current>>16;
	Globa->modbus_485_PC04[58] = Globa->Control_DC_Infor_Data[4].Output_current;//充电机输出电流 0.001A
	
	Globa->modbus_485_PC04[59] = Globa->Control_DC_Infor_Data[4].need_voltage>>16;
	Globa->modbus_485_PC04[60] = Globa->Control_DC_Infor_Data[4].need_voltage;//电压需求  0.001V
	Globa->modbus_485_PC04[61] = Globa->Control_DC_Infor_Data[4].need_current>>16;
	Globa->modbus_485_PC04[62] = Globa->Control_DC_Infor_Data[4].need_current;//电流需求  0.001A
	
	reg_offset = 63;
	for(i=0;i<CONTROL_DC_NUMBER;i++)//充电参数设定
	{
		Globa->modbus_485_PC04[reg_offset++] = Globa->Control_DC_Infor_Data[i].charge_mode;
		Globa->modbus_485_PC04[reg_offset++] = Globa->Control_DC_Infor_Data[i].charge_kwh_limit>>16;//0.01度
		Globa->modbus_485_PC04[reg_offset++] = Globa->Control_DC_Infor_Data[i].charge_kwh_limit;
		Globa->modbus_485_PC04[reg_offset++] = Globa->Control_DC_Infor_Data[i].charge_money_limit>>16; //0.01元
		Globa->modbus_485_PC04[reg_offset++] = Globa->Control_DC_Infor_Data[i].charge_money_limit;		
		Globa->modbus_485_PC04[reg_offset++] = Globa->Control_DC_Infor_Data[i].charge_sec_limit>>16;	
		Globa->modbus_485_PC04[reg_offset++] = Globa->Control_DC_Infor_Data[i].charge_sec_limit;
	}
	
	Globa->modbus_485_PC04[100] = Globa->QT_Step;
	Globa->modbus_485_PC04[101] = Globa->Control_DC_Infor_Data[0].SYS_Step;
	Globa->modbus_485_PC04[102] = Globa->Control_DC_Infor_Data[0].MC_Step;
	Globa->modbus_485_PC04[103] = Globa->Control_DC_Infor_Data[0].Metering_Step;
	Globa->modbus_485_PC04[104] = Globa->Control_DC_Infor_Data[1].SYS_Step;
	Globa->modbus_485_PC04[105] = Globa->Control_DC_Infor_Data[1].MC_Step;
	Globa->modbus_485_PC04[106] = Globa->Control_DC_Infor_Data[1].Metering_Step;
	Globa->modbus_485_PC04[107] = Globa->Control_DC_Infor_Data[2].SYS_Step;
	Globa->modbus_485_PC04[108] = Globa->Control_DC_Infor_Data[2].MC_Step;
	Globa->modbus_485_PC04[109] = Globa->Control_DC_Infor_Data[2].Metering_Step;
	Globa->modbus_485_PC04[110] = Globa->Control_DC_Infor_Data[3].SYS_Step;
	Globa->modbus_485_PC04[111] = Globa->Control_DC_Infor_Data[3].MC_Step;
	Globa->modbus_485_PC04[112] = Globa->Control_DC_Infor_Data[3].Metering_Step;
	Globa->modbus_485_PC04[113] = Globa->Control_DC_Infor_Data[4].SYS_Step;
	Globa->modbus_485_PC04[114] = Globa->Control_DC_Infor_Data[4].MC_Step;
	Globa->modbus_485_PC04[115] = Globa->Control_DC_Infor_Data[4].Metering_Step;
	Globa->modbus_485_PC04[116] = g_iso8583_st;
	Globa->modbus_485_PC04[117] = Globa->have_hart;
	Globa->modbus_485_PC04[118] = Globa->iso8583_login_cnt>>16;
	Globa->modbus_485_PC04[119] = Globa->iso8583_login_cnt;
	
	Globa->modbus_485_PC04[120] = Globa->ac_col_tx_cnt>>16;
	Globa->modbus_485_PC04[121] = Globa->ac_col_tx_cnt;
	
	Globa->modbus_485_PC04[122] = Globa->ac_col_rx_ok_cnt>>16;
	Globa->modbus_485_PC04[123] = Globa->ac_col_rx_ok_cnt;
	
	Globa->modbus_485_PC04[124] = Globa->ac_meter_tx_cnt>>16;
	Globa->modbus_485_PC04[125] = Globa->ac_meter_tx_cnt;
	
	Globa->modbus_485_PC04[126] = Globa->ac_meter_rx_ok_cnt>>16;
	Globa->modbus_485_PC04[127] = Globa->ac_meter_rx_ok_cnt;
	
	Globa->modbus_485_PC04[128] = Globa->dc_meter_tx_cnt[0]>>16;
	Globa->modbus_485_PC04[129] = Globa->dc_meter_tx_cnt[0];
	
	Globa->modbus_485_PC04[130] = Globa->dc_meter_rx_ok_cnt[0]>>16;
	Globa->modbus_485_PC04[131] = Globa->dc_meter_rx_ok_cnt[0];
	
	Globa->modbus_485_PC04[132] = Globa->dc_meter_tx_cnt[1]>>16;
	Globa->modbus_485_PC04[133] = Globa->dc_meter_tx_cnt[1];
	
	Globa->modbus_485_PC04[134] = Globa->dc_meter_rx_ok_cnt[1]>>16;
	Globa->modbus_485_PC04[135] = Globa->dc_meter_rx_ok_cnt[1];
	
	Globa->modbus_485_PC04[136] = Globa->dc_meter_tx_cnt[2]>>16;
	Globa->modbus_485_PC04[137] = Globa->dc_meter_tx_cnt[2];
	
	Globa->modbus_485_PC04[138] = Globa->dc_meter_rx_ok_cnt[2]>>16;
	Globa->modbus_485_PC04[139] = Globa->dc_meter_rx_ok_cnt[2];
	
	Globa->modbus_485_PC04[140] = Globa->dc_meter_tx_cnt[3]>>16;
	Globa->modbus_485_PC04[141] = Globa->dc_meter_tx_cnt[3];
	
	Globa->modbus_485_PC04[142] = Globa->dc_meter_rx_ok_cnt[3]>>16;
	Globa->modbus_485_PC04[143] = Globa->dc_meter_rx_ok_cnt[3];
	
	Globa->modbus_485_PC04[144] = Globa->dc_meter_tx_cnt[4]>>16;
	Globa->modbus_485_PC04[145] = Globa->dc_meter_tx_cnt[4];
	
	Globa->modbus_485_PC04[146] = Globa->dc_meter_rx_ok_cnt[4]>>16;
	Globa->modbus_485_PC04[147] = Globa->dc_meter_rx_ok_cnt[4];
	
	
	Globa->modbus_485_PC04[148] = Globa->server_tx_cnt>>16;
	Globa->modbus_485_PC04[149] = Globa->server_tx_cnt;
	
	Globa->modbus_485_PC04[150] = Globa->server_rx_cnt>>16;
	Globa->modbus_485_PC04[151] = Globa->server_rx_cnt;
	
	Globa->modbus_485_PC04[152] = Globa->server_rx_ok_cnt>>16;
	Globa->modbus_485_PC04[153] = Globa->server_rx_ok_cnt;
	
	ul_temp = GetTickCount();//test
	Globa->modbus_485_PC04[154] = ul_temp>>16;
	Globa->modbus_485_PC04[155] = ul_temp;
	
	ul_temp = Globa->Control_DC_Infor_Data[0].meter_voltage;
	Globa->modbus_485_PC04[156] = ul_temp>>16;
	Globa->modbus_485_PC04[157] = ul_temp;
	ul_temp = Globa->Control_DC_Infor_Data[0].meter_current;
	Globa->modbus_485_PC04[158] = ul_temp>>16;
	Globa->modbus_485_PC04[159] = ul_temp;
	
	ul_temp = Globa->Control_DC_Infor_Data[1].meter_voltage;
	Globa->modbus_485_PC04[160] = ul_temp>>16;
	Globa->modbus_485_PC04[161] = ul_temp;
	ul_temp = Globa->Control_DC_Infor_Data[1].meter_current;
	Globa->modbus_485_PC04[162] = ul_temp>>16;
	Globa->modbus_485_PC04[163] = ul_temp;
	
	ul_temp = Globa->Control_DC_Infor_Data[2].meter_voltage;
	Globa->modbus_485_PC04[164] = ul_temp>>16;
	Globa->modbus_485_PC04[165] = ul_temp;
	ul_temp = Globa->Control_DC_Infor_Data[2].meter_current;
	Globa->modbus_485_PC04[166] = ul_temp>>16;
	Globa->modbus_485_PC04[167] = ul_temp;
	
	ul_temp = Globa->Control_DC_Infor_Data[3].meter_voltage;
	Globa->modbus_485_PC04[168] = ul_temp>>16;
	Globa->modbus_485_PC04[169] = ul_temp;
	ul_temp = Globa->Control_DC_Infor_Data[3].meter_current;
	Globa->modbus_485_PC04[170] = ul_temp>>16;
	Globa->modbus_485_PC04[171] = ul_temp;
	
	ul_temp = Globa->Control_DC_Infor_Data[4].meter_voltage;
	Globa->modbus_485_PC04[172] = ul_temp>>16;
	Globa->modbus_485_PC04[173] = ul_temp;
	ul_temp = Globa->Control_DC_Infor_Data[4].meter_current;
	Globa->modbus_485_PC04[174] = ul_temp>>16;
	Globa->modbus_485_PC04[175] = ul_temp;
	
	//分时电费和服务费
	reg_offset = 200;
	for(i=0;i<CONTROL_DC_NUMBER;i++)
	{
		ul_temp = Globa->Control_DC_Infor_Data[i].ulcharged_kwh[0]/METER_READ_MULTIPLE;
		Globa->modbus_485_PC04[reg_offset++] = ul_temp>>16;
		Globa->modbus_485_PC04[reg_offset++] = ul_temp;
		
		ul_temp = Globa->Control_DC_Infor_Data[i].ulcharged_kwh[1]/METER_READ_MULTIPLE;
		Globa->modbus_485_PC04[reg_offset++] = ul_temp>>16;
		Globa->modbus_485_PC04[reg_offset++] = ul_temp;
		
		ul_temp = Globa->Control_DC_Infor_Data[i].ulcharged_kwh[2]/METER_READ_MULTIPLE;
		Globa->modbus_485_PC04[reg_offset++] = ul_temp>>16;
		Globa->modbus_485_PC04[reg_offset++] = ul_temp;
		
		ul_temp = Globa->Control_DC_Infor_Data[i].ulcharged_kwh[3]/METER_READ_MULTIPLE;
		Globa->modbus_485_PC04[reg_offset++] = ul_temp>>16;
		Globa->modbus_485_PC04[reg_offset++] = ul_temp;
		
		ul_temp = Globa->Control_DC_Infor_Data[i].kwh;
		Globa->modbus_485_PC04[reg_offset++] = ul_temp>>16;
		Globa->modbus_485_PC04[reg_offset++] = ul_temp;
		
		//210	
		Globa->modbus_485_PC04[reg_offset++] = Globa->Control_DC_Infor_Data[i].ulcharged_rmb[0]>>16;
		Globa->modbus_485_PC04[reg_offset++] = Globa->Control_DC_Infor_Data[i].ulcharged_rmb[0];
		
		Globa->modbus_485_PC04[reg_offset++] = Globa->Control_DC_Infor_Data[i].ulcharged_rmb[1]>>16;
		Globa->modbus_485_PC04[reg_offset++] = Globa->Control_DC_Infor_Data[i].ulcharged_rmb[1];
		
		Globa->modbus_485_PC04[reg_offset++] = Globa->Control_DC_Infor_Data[i].ulcharged_rmb[2]>>16;
		Globa->modbus_485_PC04[reg_offset++] = Globa->Control_DC_Infor_Data[i].ulcharged_rmb[2];
		
		Globa->modbus_485_PC04[reg_offset++] = Globa->Control_DC_Infor_Data[i].ulcharged_rmb[3]>>16;
		Globa->modbus_485_PC04[reg_offset++] = Globa->Control_DC_Infor_Data[i].ulcharged_rmb[3];
		
		Globa->modbus_485_PC04[reg_offset++] = Globa->Control_DC_Infor_Data[i].total_rmb>>16;
		Globa->modbus_485_PC04[reg_offset++] = Globa->Control_DC_Infor_Data[i].total_rmb;
		
		Globa->modbus_485_PC04[reg_offset++] = Globa->Control_DC_Infor_Data[i].rmb>>16;
		Globa->modbus_485_PC04[reg_offset++] = Globa->Control_DC_Infor_Data[i].rmb;
		
		ul_temp = Globa->Control_DC_Infor_Data[i].total_rmb - Globa->Control_DC_Infor_Data[i].rmb;//服务费
		Globa->modbus_485_PC04[reg_offset++] = ul_temp>>16;
		Globa->modbus_485_PC04[reg_offset++] = ul_temp;
		
		ul_temp = Globa->Control_DC_Infor_Data[i].park_rmb;//停车费
		Globa->modbus_485_PC04[reg_offset++] = ul_temp>>16;
		Globa->modbus_485_PC04[reg_offset++] = ul_temp;
	}
	

	reg_offset = 300;
	for(i=0;i<CONTROL_DC_NUMBER;i++)
	{
		Globa->modbus_485_PC04[reg_offset++] = Globa->Control_DC_Infor_Data[i].Double_Gun_To_Car_falg;
		Globa->modbus_485_PC04[reg_offset++] = Globa->Control_DC_Infor_Data[i].Double_Gun_To_Car_Hostgun;
		Globa->modbus_485_PC04[reg_offset++] = Globa->Control_DC_Infor_Data[i].Double_Gun_To_Car_Othergun;
		ul_temp = Globa->Control_DC_Infor_Data[i].Host_Recv_Need_A;
		Globa->modbus_485_PC04[reg_offset++] = ul_temp>>16;
		Globa->modbus_485_PC04[reg_offset++] = ul_temp;
		Globa->modbus_485_PC04[reg_offset++] = Globa->Control_DC_Infor_Data[i].Host_Recv_BMSChargeStep_Falg;
	}
}

void mb_func03hook(void)
{
	
	
}


void mb_func06hook(unsigned short reg_addr,unsigned short reg_value)
{
	Globa->modbus_485_PC06[reg_addr] = reg_value;
	
}

/*函数：Tcp_modbus_Deal *********************************************************
//输入：无；
//返回：无:
////暂时不考虑粘包问题，若粘包，则前面一帧有效，后面的帧丢掉不响应
*******************************************************************************/

INT32 Tcp_modbus_Deal(unsigned int fd, unsigned char *msg, unsigned char length)
{
    UINT32 i,j;
	UINT8  MB_func_code;
    UINT16   start_reg,reg_num,reg_value;
    UINT16   cur_reg_addr;
    UINT32 index = 0,bytes_cnt=0;
    UINT32   registers_length=0;

    if((length < MBAP_fun)||(length >= BUF_SIZE )){ //length <7 or >BUF_SIZE
		Modbus_Fault_response(fd, msg, COMMS_FAILURE);
		return (-1);
	}else if(!((msg[MBAP_proto] == 0)&&(msg[MBAP_proto+1] == 0))){//协议标识符不正确
		Modbus_Fault_response(fd, msg, COMMS_FAILURE);
		return (-1);
	}else if(!((msg[MBAP_unit] >= 1)&&(msg[MBAP_unit] <255))){//地址无效
		Modbus_Fault_response(fd, msg, COMMS_FAILURE);
		return (-1);
	}else if(!((msg[MBAP_length] == 0x00)&&(msg[MBAP_length+1] >= 1)&& \
	(msg[MBAP_length+1] <=BUF_SIZE-MBAP_unit)&&(msg[MBAP_length+1] + MBAP_unit == length))){//长度不正确
		Modbus_Fault_response(fd, msg, COMMS_FAILURE);
		return (-1);
	}
	
	MB_func_code = msg[MBAP_unit+1];
	
	switch(MB_func_code)
	{
		case 0x02://读取遥信量
		{      //功能码 02 读离散量输入msg [7]
		  if(length == CMB02_regnumL + 1){ //帧长度正确
			start_reg = (msg[CMB02_addrH]<<8) + msg[CMB02_addrL];
			reg_num = (msg[CMB02_regnumH]<<8) + msg[CMB02_regnumL];
			if(start_reg <= (REG_02INPUT_NREGS - 1)){//地址正确
			  if(reg_num <= (REG_02INPUT_NREGS - start_reg)){//读取的数据范围正确
				msg[RMB02_regnum] =  (msg[CMB02_regnumL] + 7)/8;   //寄存器个数
				msg[MBAP_length+1] =  RMB02_regdata + msg[RMB02_regnum] - MBAP_unit;  //长度

				if(reg_num <= 8){//离散量数不超过1个字节的8个bit,1个字节即可表示
				  bytes_cnt = 1;//1个字节
				}else{
				  bytes_cnt = (reg_num%8 == 0) ? (reg_num >> 3): ((reg_num >> 3) + 1);
				}

				pthread_mutex_lock(&modbus_pc_mutex);
				index = RMB02_regdata ;
				for(j=0; j < bytes_cnt; j++)
				{
				  msg[index] = 0 ;
				  for(i=0; i < 8; i++)
				  {
					registers_length ++;
					msg[index] |= ((Globa->modbus_485_PC02[start_reg + 8*j+i] & 0x01)<< i);
					if(registers_length == reg_num){
					  break;
					}
				  }
				  index++;
				}
			   pthread_mutex_unlock(&modbus_pc_mutex);

	#ifdef DEBUG_MODBUS_TCP
				printf("sent ength = %d data =", msg[MBAP_length+1] + MBAP_unit);
				for(j=0; j<msg[MBAP_length+1] + MBAP_unit; j++)printf("%02x ",msg[j]);
				printf("\n");
	#endif
				send(fd, msg, msg[MBAP_length+1] + MBAP_unit, 0);//发送数据
				return (0);
			  }else{//数据异常响应
				Modbus_Fault_response(fd, msg, ILLEGAL_DATA_VALUE);
			  }
			}
		  }else{//长度异常响应
			Modbus_Fault_response(fd, msg, COMMS_FAILURE);
		  }
		}
		break;
		case 0x03://读保持寄存器
		{//功能码 03 读保持寄存器
			  if(length == CMB03_regnumL + 1){ //帧长度正确
				  start_reg = (msg[CMB03_addrH]<<8) + msg[CMB03_addrL];
				 reg_num = (msg[CMB03_regnumH]<<8) + msg[CMB03_regnumL];

				 if(start_reg <= (REG_06INPUT_NREGS - 1))
				 {//地址正确
				   if(reg_num <= (REG_06INPUT_NREGS - start_reg))
				   {//读取的数据范围正确

					msg[RMB03_regnum] =  msg[CMB03_regnumL]*2;   //寄存器个数
					msg[MBAP_length+1] =  RMB03_regdata + msg[RMB03_regnum] - MBAP_unit;  //长度
					index = RMB03_regdata;
					pthread_mutex_lock(&modbus_pc_mutex);
					mb_func03hook();//更新变量
					for(j=0; j < reg_num; j++){
					  cur_reg_addr = start_reg + j;

					  msg[index++] = Globa->modbus_485_PC06[cur_reg_addr] >> 8;    //先传高字节,注意数组不要越界
					  msg[index++] = Globa->modbus_485_PC06[cur_reg_addr];         //再传低字节
					}
					pthread_mutex_unlock(&modbus_pc_mutex);
		#ifdef DEBUG_MODBUS_TCP
					printf("sent ength = %d data =", msg[MBAP_length+1] + MBAP_unit);
					for(j=0; j<msg[MBAP_length+1] + MBAP_unit; j++)printf("%02x ",msg[j]);
					  printf("\n");
		#endif
					send(fd, msg, msg[MBAP_length+1] + MBAP_unit, 0);//发送数据
					return (0);
				  }else{//数据异常响应
					Modbus_Fault_response(fd, msg, ILLEGAL_DATA_VALUE);
				  }
				}
			  }else{//长度异常响应
				Modbus_Fault_response(fd, msg, COMMS_FAILURE);
			  }
		}
		break;	
		case 0x04://读遥测寄存器
		{//功能码 04 读输入寄存器
			  if(length == CMB04_regnumL + 1){ //帧长度正确
				  start_reg = (msg[CMB04_addrH]<<8) + msg[CMB04_addrL];
				 reg_num = (msg[CMB04_regnumH]<<8) + msg[CMB04_regnumL];

				 if(start_reg <= (REG_04INPUT_NREGS - 1))
				 {//地址正确
				   if(reg_num <= (REG_04INPUT_NREGS - start_reg))
				   {//读取的数据范围正确

					msg[RMB04_regnum] =  msg[CMB04_regnumL]*2;   //寄存器个数
					msg[MBAP_length+1] =  RMB04_regdata + msg[RMB04_regnum] - MBAP_unit;  //长度
					index = RMB04_regdata;
					pthread_mutex_lock(&modbus_pc_mutex);
					mb_func04hook();//更新变量
					for(j=0; j < reg_num; j++){
					  cur_reg_addr = start_reg + j;

					  msg[index++] = Globa->modbus_485_PC04[cur_reg_addr] >> 8;    //先传高字节,注意数组不要越界
					  msg[index++] = Globa->modbus_485_PC04[cur_reg_addr];         //再传低字节
					}
					pthread_mutex_unlock(&modbus_pc_mutex);
		#ifdef DEBUG_MODBUS_TCP
					printf("sent ength = %d data =", msg[MBAP_length+1] + MBAP_unit);
					for(j=0; j<msg[MBAP_length+1] + MBAP_unit; j++)printf("%02x ",msg[j]);
					  printf("\n");
		#endif
					send(fd, msg, msg[MBAP_length+1] + MBAP_unit, 0);//发送数据
					return (0);
				  }else{//数据异常响应
					Modbus_Fault_response(fd, msg, ILLEGAL_DATA_VALUE);
				  }
				}
			  }else{//长度异常响应
				Modbus_Fault_response(fd, msg, COMMS_FAILURE);
			  }
		}
		break;
		case 0x06://写单个保持寄存器
		{//功能码 06 
			if(length == RMB06_regdataL + 1)
			{ //帧长度正确
				  start_reg = (msg[CMB06_addrH]<<8) + msg[CMB06_addrL];
				  reg_value = (msg[RMB06_regdataH]<<8) + msg[RMB06_regdataL];

				 if(start_reg <= (REG_06INPUT_NREGS - 1))
				 {//地址未越界           
					pthread_mutex_lock(&modbus_pc_mutex);
						mb_func06hook(start_reg,reg_value);//更新变量
					pthread_mutex_unlock(&modbus_pc_mutex);

					
					send(fd, msg, length, 0);//06功能码原样返回发送数据
					return (0);
				 }
			}
		}
		break;
		default:	 //功能码 非法功能码
				Modbus_Fault_response(fd, msg, ILLEGAL_FUNCTION);			
			break;
	}

 return (-1);
}

/*函数：TCP MODBUS接收及发送线程 ******************************************
//输入：无；
//返回：无：
*******************************************************************************/
INT32  TCP_MODBUS_Server()
{
    unsigned int i=0;
	unsigned char msg[BUF_SIZE];

    int Listen_socket = 0, ret, bind_on;
	struct sockaddr_in local_addr;
	struct sockaddr_in client_addr;

	fd_set fdsr; //文件描述符集的定义
	socklen_t addr_size;
	addr_size = sizeof(struct sockaddr_in);
	
	unsigned int fd[MAXCLIENT] = {0};    // accepted connection fd
	unsigned int con_time[MAXCLIENT] = {0};
	int new_fd;
	struct timeval tv;
	time_t systime;	
	struct tm *tm_t;
    int lastminute = 0;

	int flag_minutechange = 0;
	int maxsock = Listen_socket;

	//建立socket套接字
	if((Listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1){
      perror("TCP_MODBUS_THREAD failed create socket");
      return(-1);
	}
	maxsock = Listen_socket;

	//bind API 函数将允许地址的立即重用
	bind_on = 1;
	ret = setsockopt( Listen_socket, SOL_SOCKET, SO_REUSEADDR, &bind_on, sizeof(bind_on));

	struct timeval tv1;//kaka 2013-7-30 8:09:15
	tv1.tv_sec = 5;
	tv1.tv_usec = 0;
	//设置发送时限
	setsockopt(Listen_socket, SOL_SOCKET, SO_SNDTIMEO, &tv1, sizeof(tv1));
	//设置接收时限
	setsockopt(Listen_socket, SOL_SOCKET, SO_RCVTIMEO, &tv1, sizeof(tv1));

	//设置本机服务类型
	local_addr.sin_family = AF_INET;
	local_addr.sin_port = htons(MODBUS_TCP_PORT);
	local_addr.sin_addr.s_addr = INADDR_ANY;

	//绑定本机IP和端口号
	if(bind(Listen_socket, (struct sockaddr*)&local_addr, sizeof(struct sockaddr)) == -1){
      perror("TCP_MODBUS_THREAD failed bind");
      return(-1);
	}

	//监听客户端连接
	if(listen(Listen_socket, BACKLOG) == -1){
      perror("TCP_MODBUS_THREAD failed listen");
      return(-1);
	}
	prctl(PR_SET_NAME,(unsigned long)"tcp_mb_server");//设置线程名字
	/****************************************/
	while(1)
	{
      //TCP_Server_Watchdog = 1;

      flag_minutechange = 0;
      systime = time(NULL);        //获得系统的当前时间
      tm_t = localtime(&systime);		//结构指针指向当前时间
      if(tm_t->tm_min != lastminute){//每次循环开始都读取系统时间，与上次分钟数比较，为以下超时判断提供依据
        lastminute = tm_t->tm_min;
        flag_minutechange = 1;
      }

      FD_ZERO(&fdsr); //每次进入循环都重建描述符集
      FD_SET(Listen_socket, &fdsr);
      for(i = 0; i < MAXCLIENT; i++){//将存在的套接字加入描述符集
        if(fd[i] != 0){
          FD_SET(fd[i], &fdsr);

          if(flag_minutechange == 1){
            con_time[i]--;
            if(con_time[i] <= 0){
              close(fd[i]);
              FD_CLR(fd[i], &fdsr);
              fd[i] = 0;
            }
          }
        }
      }

      tv.tv_sec = 10;
      tv.tv_usec = 0;
      ret = select(maxsock + 1, &fdsr, NULL, NULL, &tv);
      if(ret < 0){//<0表示探测失败
        perror("failed select\n");
        break;//退出，看门狗起作用，等待重启
      }else if(ret == 0){//=0表示超时，下一轮循环
        continue;
      }

     //如果select发现有异常，循环判断各活跃连接是否有数据到来
      for(i = 0; i < MAXCLIENT; i++){
        if(FD_ISSET(fd[i], &fdsr)){
          ret = recv(fd[i], msg, BUF_SIZE, 0);
          if(ret <= 0){// recv<=0,表明客户端关闭连接，服务器也关闭相应连接，并把连接套接子从文件描述符集中清除

#ifdef DEBUG_MODBUS_TCP
            printf("client[%d] close\n", i);
#endif

            close(fd[i]);
            FD_CLR(fd[i], &fdsr);
            fd[i] = 0;
          }else{//否则表明客户端有数据发送过来，作相应接受处理
            con_time[i] = MAX_IDLECONNCTIME; //重新写入fd[i]的超时数，再此之后如果MAX_IDLECONNCTIME分钟内此连接无反应，服务器会关闭该连接

#ifdef DEBUG_MODBUS_TCP
            printf("recv %d length = %d msg =",++k, ret);
            for(j=0; j<ret; j++)printf("%02x ",msg[j]);
              printf("\n");
#endif
              Tcp_modbus_Deal(fd[i], msg, ret);
          }
        }
      }

		// 以下说明异常有来自客户端的连接请求
      if(FD_ISSET(Listen_socket, &fdsr)){
        new_fd = accept(Listen_socket, (struct sockaddr *)&client_addr, &addr_size);
        if(new_fd <= 0){
          perror("failed accept");
          continue;
        }

        //判断活跃连接数时候是否小于最大连接数，如果是，添加新连接到文件描述符集中
        for(i = 0; i < MAXCLIENT; i++){
          if(fd[i] == 0){
            fd[i] = new_fd;
            con_time[i] = MAX_IDLECONNCTIME; //每次新建立连接，设置该连接的超时数，如果此连接此后MAX_IDLECONNCTIME分钟内无反应，关闭该连接

#ifdef DEBUG_MODBUS_TCP
            printf("new connection client[%d] %s:%d\n", i, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
#endif

            if(new_fd > maxsock){
              maxsock = new_fd;
            }

            break;
          }
        }

        if(i >= MAXCLIENT){

#ifdef DEBUG_MODBUS_TCP
          printf("MAXCLIENT arrive, exit\n");
          send(new_fd, "over MAXCLIENT\n", 25, 0);
#endif

          close(new_fd);
        }
      }
      
	}

	return(-1);
}


/*函数：TCP-TP MODBUS接收及发送线程创建 ****************************************
//输入：无；
//返回：无：
********************************************************************************/
void TCP_MODBUS_thread(void)
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

	//sleep(3);
   
	if(0 != pthread_create(&td1, &attr, (void *)TCP_MODBUS_Server, NULL)){
		printf("####pthread_create TCP_MODBUS_Server ERROR ####\n\n");
	}

	ret = pthread_attr_destroy(&attr);
	if(ret != 0)
		return;
}


