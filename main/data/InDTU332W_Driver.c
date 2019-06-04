/****************************************Copyright (c)****************************************************
**                               DongGuan EAST Co.,LTD.
**                                     
**                                 http://www.EASTUPS.com
**
**--------------File Info---------------------------------------------------------------------------------
** File Name:               InDTU332W_Driver.c
** Last modified Date:      2015.11.13
** Last Version:            1.0
** Description:             GPRS模块InDTU332W通信接口
** 
**--------------------------------------------------------------------------------------------------------
** Created By:              
** Created date:            2015.11.13
** Version:                 1.0
** Descriptions:            The original version 初始版本
**
**--------------------------------------------------------------------------------------------------------
*********************************************************************************************************/

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
#include <pthread.h>
#include <sys/shm.h> 

#include "../data/globalvar.h"

#include "../../inc/sqlite/sqlite3.h"
#include "../data/md5.h"  
#include "InDTU332W_Driver.h"

eInDTU332W_CMD     	g_eInDTU332W_cmd;//最后1次发送的帧命令
eInDTU332W_CFG_TAG 	g_eInDTU332W_cfg_tag;

unsigned char   g_InDTU332W_exist_flag;//值0表示未检测到GPRS模块,值1表示检测到GPRS模块
unsigned char	g_InDTU332W_login_flag;//值0表示登录不OK，值1 OK
unsigned char	g_InDTU332W_conn_flag;//值0表示连接服务器不OK，值1 表示连接服务器OK
unsigned char hexip[4];
unsigned char hexport[2];
unsigned char  g_InDTU332W_gateway_ip[4];//服务器IP
unsigned short g_InDTU332W_gateway_port;//服务器端口

unsigned char* server_name;// = "cbrokertest.jiasmei.com";

const static unsigned short fcsTab[256] = 
{ 
0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf, 
0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7, 
0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e, 
0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876, 
0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd, 
0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5, 
0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c, 
0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974, 
0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb, 
0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3, 
0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a, 
0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72, 
0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9, 
0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738, 
0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70, 
0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7, 
0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff, 
0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036, 
0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e, 
0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5, 
0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd, 
0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134, 
0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c, 
0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3, 
0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb, 
0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232, 
0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a, 
0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1, 
0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9, 
0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330, 
0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78 };

/* * Calculate a new fcs given the current fcs and the new data. */ 
extern unsigned short pppfcs16(unsigned short fcs, unsigned char *cp, unsigned short len) 
{ 
	while (len--)
	{ 
		fcs = (fcs >> 8) ^ fcsTab[(fcs ^ *cp++) & 0xff]; 
	}
	//return (fcs^0xFFFF); 
	return fcs;
}


unsigned char InDTU332W_Active_Frame(unsigned char* tx_buf)//填充GPRS模块激活命令帧,返回帧的长度
{
	
	unsigned short tx_len = 0;
	unsigned short fcs;
	tx_buf[tx_len++] = 0x55;
	tx_buf[tx_len++] = 0xAA;
	tx_buf[tx_len++] = 0x55;
	tx_buf[tx_len++] = 0xAA;
	
	tx_buf[tx_len++] = InDTU332W_ACTIVE;
	tx_buf[tx_len++] = 0;
	tx_buf[tx_len++] = 5;
	tx_buf[tx_len++] = 0x81;
	tx_buf[tx_len++] = 0x85;
	tx_buf[tx_len++] = 0x00;
	tx_buf[tx_len++] = 0x00;
	tx_buf[tx_len++] = 0x01;
	
		
	fcs = pppfcs16(0xFFFF,tx_buf,tx_len);
	fcs ^= 0xFFFF;
	tx_buf[tx_len++] = fcs;//low byte 
	tx_buf[tx_len++] = fcs>>8;
	
	return tx_len;
	
}

//填充GPRS模块探测帧,返回帧的长度
unsigned char InDTU332W_Scan_Frame(unsigned char* tx_buf)
{
	unsigned short tx_len = 0;
	unsigned short fcs;
	tx_buf[tx_len++] = 0x55;
	tx_buf[tx_len++] = 0xAA;
	tx_buf[tx_len++] = 0x55;
	tx_buf[tx_len++] = 0xAA;
	
	tx_buf[tx_len++] = InDTU332W_SCAN;
	tx_buf[tx_len++] = 0;
	tx_buf[tx_len++] = 0;
		
	fcs = pppfcs16(0xFFFF,tx_buf,tx_len);
	fcs ^= 0xFFFF;
	tx_buf[tx_len++] = fcs;//low byte 
	tx_buf[tx_len++] = fcs>>8;
	
	return tx_len;
}


//填充GPRS模块登录帧,返回帧的长度
unsigned char InDTU332W_Login_Frame(unsigned char* tx_buf,char* login_name,char* login_password)
{
	unsigned short tx_len = 0;
	unsigned short name_len=strlen(login_name);
	unsigned short password_len = strlen(login_password);
	unsigned short data_len = 8 + name_len + password_len;//2个TAG+2个TAG对应的长度域即8个字节
	unsigned short fcs;
	tx_buf[tx_len++] = 0x55;
	tx_buf[tx_len++] = 0xAA;
	tx_buf[tx_len++] = 0x55;
	tx_buf[tx_len++] = 0xAA;
	
	tx_buf[tx_len++] = InDTU332W_LOGIN;
	tx_buf[tx_len++] = data_len>>8;//大端顺序
	tx_buf[tx_len++] = data_len;
	tx_buf[tx_len++] = 0x81;
	tx_buf[tx_len++] = 0x10;//login name tag 0x8110
	tx_buf[tx_len++] = name_len>>8;//大端顺序
	tx_buf[tx_len++] = name_len;
	memcpy(&tx_buf[tx_len],login_name,name_len);
	tx_len += name_len;
	tx_buf[tx_len++] = 0x81;
	tx_buf[tx_len++] = 0x11;//login password tag 0x8111
	tx_buf[tx_len++] = password_len>>8;//大端顺序
	tx_buf[tx_len++] = password_len;
	memcpy(&tx_buf[tx_len],login_password,password_len);
	tx_len += password_len;
	
	fcs = pppfcs16(0xFFFF,tx_buf,tx_len);//去掉帧头4字节
	fcs ^= 0xFFFF;
	tx_buf[tx_len++] = fcs;//low byte 
	tx_buf[tx_len++] = fcs>>8;
	
	return tx_len;
}

//填充GPRS模块重启帧,返回帧的长度
unsigned char InDTU332W_Reset_Frame(unsigned char* tx_buf)
{
	unsigned short tx_len = 0;	
	unsigned short fcs;
	
	tx_buf[tx_len++] = 0x55;
	tx_buf[tx_len++] = 0xAA;
	tx_buf[tx_len++] = 0x55;
	tx_buf[tx_len++] = 0xAA;
	
	tx_buf[tx_len++] = InDTU332W_RESET;
	tx_buf[tx_len++] = 0;
	tx_buf[tx_len++] = 0;
	
	fcs = pppfcs16(0xFFFF,tx_buf,tx_len);
	fcs ^= 0xFFFF;
	tx_buf[tx_len++] = fcs;//low byte 
	tx_buf[tx_len++] = fcs>>8;
	return tx_len;
}

//填充GPRS模块读取单个状态TAG帧,返回帧的长度
unsigned char InDTU332W_Status_RD_Frame(unsigned char* tx_buf, eInDTU332W_CFG_TAG  tag_read)
{
	unsigned short tx_len = 0;	
	unsigned short fcs;
	
	tx_buf[tx_len++] = 0x55;
	tx_buf[tx_len++] = 0xAA;
	tx_buf[tx_len++] = 0x55;
	tx_buf[tx_len++] = 0xAA;
	
	tx_buf[tx_len++] = InDTU332W_RD_STATUS;
	tx_buf[tx_len++] = 0;
	tx_buf[tx_len++] = 4;
	
	tx_buf[tx_len++] = tag_read>>8;
	tx_buf[tx_len++] = tag_read;
	
	tx_buf[tx_len++] = 0;
	tx_buf[tx_len++] = 0;
	
	
	fcs = pppfcs16(0xFFFF,tx_buf,tx_len);
	fcs ^= 0xFFFF;
	tx_buf[tx_len++] = fcs;//low byte 
	tx_buf[tx_len++] = fcs>>8;
	if(TAG_GATEWAY_CONN_ST == tag_read)
		g_InDTU332W_conn_flag = 0;//reset
	return tx_len;
}


//填充GPRS模块读取单个配置帧,返回帧的长度
unsigned char InDTU332W_Single_RD_Frame(unsigned char* tx_buf, eInDTU332W_CFG_TAG  tag_read)
{
	unsigned short tx_len = 0;	
	unsigned short fcs;
	
	tx_buf[tx_len++] = 0x55;
	tx_buf[tx_len++] = 0xAA;
	tx_buf[tx_len++] = 0x55;
	tx_buf[tx_len++] = 0xAA;
	
	tx_buf[tx_len++] = InDTU332W_RD_SINGLE_CFG;
	tx_buf[tx_len++] = 0;
	tx_buf[tx_len++] = 4;
	
	tx_buf[tx_len++] = tag_read>>8;
	tx_buf[tx_len++] = tag_read;
	
	tx_buf[tx_len++] = 0;
	tx_buf[tx_len++] = 0;
	
	
	fcs = pppfcs16(0xFFFF,tx_buf,tx_len);
	fcs ^= 0xFFFF;
	tx_buf[tx_len++] = fcs;//low byte 
	tx_buf[tx_len++] = fcs>>8;
	
	return tx_len;
}

//填充GPRS模块设置单个配置帧,返回帧的长度
unsigned char InDTU332W_Single_WR_Frame(unsigned char* tx_buf, eInDTU332W_CFG_TAG  tag_write)
{
	unsigned short tx_len = 0;	
	unsigned short fcs;
	unsigned char domain_len=0;
	tx_buf[tx_len++] = 0x55;
	tx_buf[tx_len++] = 0xAA;
	tx_buf[tx_len++] = 0x55;
	tx_buf[tx_len++] = 0xAA;
	
	tx_buf[tx_len++] = InDTU332W_WR_SINGLE_CFG;
	switch(tag_write)
	{
		case TAG_GATEWAY_ADDR:
			tx_buf[tx_len++] = 0;
			tx_buf[tx_len++] = 10;//设置网关的包体长度10字节，tag(2字节)+长度(2字节)+6字节数据
			tx_buf[tx_len++] = tag_write>>8;
			tx_buf[tx_len++] = tag_write;
			tx_buf[tx_len++] = 0;
			tx_buf[tx_len++] = 6;//后续有6字节参数
			tx_buf[tx_len++] = hexip[0];
			tx_buf[tx_len++] = hexip[1];
			tx_buf[tx_len++] = hexip[2];
			tx_buf[tx_len++] = hexip[3];
			tx_buf[tx_len++] = hexport[1];//Globa_1->ISO8583_NET_setting.ISO8583_Server_port[1];
			tx_buf[tx_len++] = hexport[0];//Globa_1->ISO8583_NET_setting.ISO8583_Server_port[0];			
			break;
	
			case TAG_UART1_BAUD:
			tx_buf[tx_len++] = 0;
			tx_buf[tx_len++] = 6;//设置网关的包体长度10字节，tag(2字节)+长度(2字节)+2字节数据
			tx_buf[tx_len++] = tag_write>>8;
			tx_buf[tx_len++] = tag_write;
			tx_buf[tx_len++] = 0;
			tx_buf[tx_len++] = 2;//后续有6字节参数
			
			tx_buf[tx_len++] = 0;//波特率索引高8位为0
			//tx_buf[tx_len++] = InDTU332W_UART_BAUD9600;//波特率固定按9600
			/* 
			if( MCU_UART_BAUD1200 == DTU_UART_BAUD)
				tx_buf[tx_len++] = InDTU332W_UART_BAUD1200;
			
			if( MCU_UART_BAUD2400 == DTU_UART_BAUD)
				tx_buf[tx_len++] = InDTU332W_UART_BAUD2400;
			
			if( MCU_UART_BAUD4800 == DTU_UART_BAUD)
				tx_buf[tx_len++] = InDTU332W_UART_BAUD4800;
			
			if( MCU_UART_BAUD9600 == DTU_UART_BAUD) */
				tx_buf[tx_len++] = InDTU332W_UART_BAUD9600;
			
			/* if( MCU_UART_BAUD14400 == DTU_UART_BAUD)
				tx_buf[tx_len++] = InDTU332W_UART_BAUD14400;
			
			if( MCU_UART_BAUD19200 == DTU_UART_BAUD)
				tx_buf[tx_len++] = InDTU332W_UART_BAUD19200;
			
			if( MCU_UART_BAUD38400 == DTU_UART_BAUD)
				tx_buf[tx_len++] = InDTU332W_UART_BAUD38400;
			
			if( MCU_UART_BAUD57600 == DTU_UART_BAUD)
				tx_buf[tx_len++] = InDTU332W_UART_BAUD57600;
			
			if( MCU_UART_BAUD115200 == DTU_UART_BAUD)
				tx_buf[tx_len++] = InDTU332W_UART_BAUD115200; */
			
			break;
      case TAG_DNS_ADDR:
				tx_buf[tx_len++] = 0;
				tx_buf[tx_len++] = 8;//设置网关的包体长度8字节，tag(2字节)+长度(2字节)+4字节数据
				tx_buf[tx_len++] = tag_write>>8;
				tx_buf[tx_len++] = tag_write;
				tx_buf[tx_len++] = 0;
				tx_buf[tx_len++] = 4;//后续有4字节参数
				tx_buf[tx_len++] = 8;
				tx_buf[tx_len++] = 8;
				tx_buf[tx_len++] = 8;
				tx_buf[tx_len++] = 8;//DNS 8.8.8.8
			break;
		case TAG_GATEWAY_NAME://域名地址  server_name
			domain_len = strlen(server_name);
			tx_buf[tx_len++] = 0;
			tx_buf[tx_len++] = 4+domain_len;//设置网关的包体长度8字节，tag(2字节)+长度(2字节)+4字节数据
			tx_buf[tx_len++] = tag_write>>8;
			tx_buf[tx_len++] = tag_write;
			tx_buf[tx_len++] = 0;
			tx_buf[tx_len++] = domain_len;//不超过1个字节长度
			memcpy(&tx_buf[tx_len],server_name,domain_len);
			tx_len += domain_len;				
			break;
		default:
			break;
	}	
	
	
	fcs = pppfcs16(0xFFFF,tx_buf,tx_len);
	fcs ^= 0xFFFF;
	tx_buf[tx_len++] = fcs;//low byte 
	tx_buf[tx_len++] = fcs>>8;
	return tx_len;
}


/******************************************************************************
** Function name:       InDTU332W_Response_Handler
** Descriptions:	    InDTU332W接收帧处理
** Input parameters:	rx_buf---数据接收缓冲区指针
                       	rx_len---接收字节长度				
** Output parameters:	
** Returned value:		返回值0表示响应帧OK,返回值1表示响应帧异常   
** Created by:		
** Created Date:		
**------------------------------------------------------------------------------
** Modified by:            
** Modified date:          
**------------------------------------------------------------------------------
*******************************************************************************/
unsigned char InDTU332W_Response_Handler(unsigned char* rx_buf,unsigned short rx_len)
{
	unsigned short fcs_check,fcs_rxd;
	unsigned char result=0;
	
	if((rx_len >= InDTU332W_RX_FR_MIN_LEN)  && (rx_len <= 256))
	{	
		if((rx_buf[0] == 0xAA) && (rx_buf[1] == 0x55) && (rx_buf[2] == 0xAA) && (rx_buf[3] == 0x55))//帧头匹配
		{
			fcs_rxd = (rx_buf[rx_len-1]<<8) | rx_buf[rx_len-2];
						
			fcs_check = pppfcs16(0xFFFF,rx_buf,4);//头的校验
			fcs_check = pppfcs16(fcs_check,&rx_buf[5],rx_len-7);//剩余字节的校验,跳过命令字和CRC
			fcs_check ^= 0xFFFF;
			if( fcs_check == fcs_rxd)//fcs ok			
			{
				printf("g_eInDTU332W_cmd=%d\n", g_eInDTU332W_cmd);
				switch(g_eInDTU332W_cmd)//先前发送的命令帧
				{
					case InDTU332W_SCAN://扫描帧的响应
						if(rx_len == 18)//响应帧长度OK
						{
							if(	rx_buf[4] == InDTU332W_SCAN )							
								g_InDTU332W_exist_flag = 1;
							else
								g_InDTU332W_exist_flag = 0;
													
						}
						else//响应帧长度异常
						{
							result=5;
							g_InDTU332W_exist_flag = 0;
						}
						break;
					case InDTU332W_LOGIN://登录的响应
						if(rx_len == 10)//响应帧长度OK
						{
							if(	(rx_buf[4] == InDTU332W_LOGIN ) && (rx_buf[7] == 0x01 ))//管理员登录成功								
								g_InDTU332W_login_flag = 1;
							else
								g_InDTU332W_login_flag = 0;
													
						}
						else//响应帧长度异常
						{
							result=5;
							g_InDTU332W_login_flag = 0;
						}
						break;
				
					case InDTU332W_RESET://
						if(rx_len == 10)//响应帧长度OK
						{
							
						}
						else//响应帧长度异常
						{
							result=5;
						
						}
						break;
					case InDTU332W_RD_STATUS://读取状态
							switch(g_eInDTU332W_cfg_tag)
							{
								case TAG_GATEWAY_CONN_ST://读取服务器连接状态
									if(rx_len == 14)//响应帧长度OK
									{
										if(4 == rx_buf[11])
											g_InDTU332W_conn_flag = 1;
										else
											g_InDTU332W_conn_flag = 0;																				
									}
									else
										result = 5;
									break;
								case TAG_CSQ_ST://读取信号强度
									if(rx_len == 14){//响应帧长度OK
									 // Globa_1->Signals_value_No = rx_buf[11];//信号强度	
									 if(rx_buf[11] >= 99){
										 rx_buf[11] = 0;
									 }
									 
									/* if(Globa_1->Registration_value == 0){
										  Globa_1->Signals_value_No = 0;
									 }else {
                     Globa_1->Signals_value_No = (((rx_buf[11]*100)/31)+19)/20;
									 }*/
									  Globa_1->Signals_value_No = (((rx_buf[11]*100)/31)+19)/20;

           					printf("读取信号强度-----No %d  rx_buf[11] =%d \n",Globa_1->Signals_value_No,rx_buf[11]);
									}else{
										result = 5;
									}
								 break;
								case TAG_REGISTRATION_ST:// = 0x81A1,//查询注册状态
									if(rx_len == 15){//响应帧长度OK
										Globa_1->Registration_value = rx_buf[12];//查询注册状态
										if(!((Globa_1->Registration_value == 0x01)||(Globa_1->Registration_value == 0x05))){
											Globa_1->Signals_value_No = 0;
										}
										printf("查询注册状态----= %d  rx_buf[12] =%d  rx_buf[12] =%d \n",Globa_1->Registration_value,rx_buf[11],rx_buf[12]);
									}else{
										result = 5;
									}
									break;
	              case TAG_ICCID_VALUE: //0x81D0,//sim卡唯一对应的ID卡号20位
									if(rx_len >= 33){//响应帧长度OK
										//memcpy(&Globa_1->ICCID_value[0], &rx_buf[11], sizeof(Globa_1->ICCID_value));
										hex_to_str(&Globa_1->ICCID_value[0], &rx_buf[11], 20);
										printf("sim卡唯一对应的ID卡号20位---%s-= \n",Globa_1->ICCID_value);

                    int  i=0;
										for(i =0;i<20;i++){
										printf("sim卡唯一对应的ID卡号20位---%d-=   rx_buf[i+11] =%0x \n",i,rx_buf[11+i]);
									  }
									}else{
										result = 5;
									}
									break;	
								default:
										break;
							}
						break;
					case InDTU332W_RD_SINGLE_CFG://读取当前配置
						{
							switch(g_eInDTU332W_cfg_tag)
							{								
								case TAG_GATEWAY_ADDR: 
									if(rx_len == 19)//响应帧长度OK
									{
										g_InDTU332W_gateway_ip[0] = rx_buf[11];
										g_InDTU332W_gateway_ip[1] = rx_buf[12];
										g_InDTU332W_gateway_ip[2] = rx_buf[13];
										g_InDTU332W_gateway_ip[3] = rx_buf[14];
										
										g_InDTU332W_gateway_port = (rx_buf[15]<<8) | rx_buf[16];
										
									}
									else
										result = 5;
									break;
								case TAG_GATEWAY_NAME:
									break;
								default:
									break;
							}
						
						}
						break;
					
						case InDTU332W_WR_SINGLE_CFG://设置当前配置
						{
							switch(g_eInDTU332W_cfg_tag)
							{
								case TAG_GATEWAY_ADDR:
									if(rx_len == 19)//响应帧长度OK
									{
										
										
									}
									else
										result = 5;
									break;
								case TAG_UART1_BAUD:
									if(rx_len == 15)//响应帧长度OK
									{
										
										
									}
									else
										result = 5;
									break;
								case TAG_GATEWAY_NAME:
									
									break;
								default:
								
									break;
								
							}
						
						}
						break;
					case InDTU332W_ACTIVE:
						if(rx_len == 9)//响应帧长度OK---AA 55 AA 55 14 00 00 B8 C9
						{
							if(	(0x14 == rx_buf[4]) && (0x00 == rx_buf[5]) && (0x00 == rx_buf[6]))
								result = 0;
							else
								result = 6;							
						}
						else
							result = 5;	
						break;
						
					default:
					
						break;
			
				}
			}
			else
				result=3;
		}
		else//响应帧异常
		{
			result=2;
		
		}
	}
	else
		result = 1;
	return result;
}
