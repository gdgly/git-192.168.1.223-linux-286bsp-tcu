/****************************************Copyright (c)****************************************************
**                               DongGuan EAST Co.,LTD.
**                                     
**                                 http://www.EASTUPS.com
**
**--------------File Info---------------------------------------------------------------------------------
** File Name:               InDTU332W_Driver.h
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

#ifndef  __InDTU332W_DRIVER_H__
#define  __InDTU332W_DRIVER_H__


//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C" {
#endif

#define InDTU332W_RX_FR_MIN_LEN 8// 9//修改为9个字节20151215//10
#define DTU_UART_BAUD   dev_cfg.dev_para.uart2_dcb.baud

typedef enum
{
	InDTU332W_UART_BAUD9600 = 0x00,//波特率9600
	InDTU332W_UART_BAUD300 = 0x01,//波特率300
	InDTU332W_UART_BAUD600 = 0x02,//波特率600
	InDTU332W_UART_BAUD1200 = 0x03,//波特率1200
	InDTU332W_UART_BAUD2400 = 0x04,//波特率2400
	InDTU332W_UART_BAUD4800 = 0x05,//波特率4800
	//InDTU332W_UART_BAUD9600 = 0x06,//波特率9600
	InDTU332W_UART_BAUD14400 = 0x07,//波特率14400
	InDTU332W_UART_BAUD19200 = 0x08,//波特率19200
	
	InDTU332W_UART_BAUD38400 = 0x09,//波特率38400
	InDTU332W_UART_BAUD56000 = 0x0A,//波特率56000
	InDTU332W_UART_BAUD57600 = 0x0B,//波特率57600
	InDTU332W_UART_BAUD115200 = 0x0C,//波特率115200
	
	
}eInDTU332W_BAUD;

typedef enum
{
	InDTU332W_SCAN = 0x03,//探测GPRS模块
	InDTU332W_LOGIN=0x05, //登录
	InDTU332W_RESET=0x12, //重启
	InDTU332W_ACTIVE=0x14, //激活GPRS模块连接GPRS
	InDTU332W_RD_STATUS = 0x15,//读取状态
	InDTU332W_RD_SINGLE_CFG=0x16, //读取单个配置
	InDTU332W_WR_SINGLE_CFG=0x18, //写入单个配置	
	
}eInDTU332W_CMD;

typedef enum
{
	TAG_GPRS_APN = 0x0002,//APN的ASCII值,如cmnet
	
	TAG_UART1_BAUD=0x0011, //串口1波特率
	TAG_UART1_DATABITS=0x0012, //串口1数据位
	TAG_UART1_STOPBITS=0x0013, //串口1停止位
	TAG_UART1_PARITY=0x0014, //串口1校验位
	
	TAG_CONNECT_METHOD = 0x001E,//连接方式,0长连接，1短连接
	TAG_DATA_ACTIVE = 0x8007,//数据激活
	
	TAG_GATEWAY_ADDR = 0x0019,//企业网关地址，IP加端口
	TAG_GATEWAY_NAME = 0x8123,//企业网关域名
	TAG_GATEWAY_CONN_METHOD = 0x001A,//企业网关连接方式，0x00纯TCP连接
	TAG_GATEWAY_CONN_ST = 0x8183,// 网关当前连接状态，值0x04表示连接主站成功
	TAG_DNS_ADDR = 0x8019,//DNS服务器地址
	TAG_CSQ_ST = 0x8184,//信号强度
	TAG_REGISTRATION_ST = 0x81A1,//注册状态
	TAG_ICCID_VALUE = 0x81D0,//sim卡唯一对应的ID卡号20位
}eInDTU332W_CFG_TAG;//设置项的TAG



typedef struct
{
	unsigned char header[4];//帧头发送到GPRS模块0x55,0xAA,0x55,0xAA,GPRS模块返回0xAA,0x55,0xAA,0x55
	unsigned char CMD;//命令字
	unsigned char data_len[2];//包体的字节长度
	//unsigned char tlvs[n];//TLVS数据	
	
}STR_GPRS_FRAME;//GPRS模块收发帧头



//本模块函数声明
extern unsigned char InDTU332W_Scan_Frame(unsigned char* tx_buf);//填充GPRS模块探测帧,返回帧的长度
extern unsigned char InDTU332W_Active_Frame(unsigned char* tx_buf);//填充GPRS模块激活命令帧,返回帧的长度
extern unsigned char InDTU332W_Login_Frame(unsigned char* tx_buf,char* login_name,char* login_password);//填充GPRS模块登录帧,返回帧的长度

extern unsigned char InDTU332W_Reset_Frame(unsigned char* tx_buf);//填充GPRS模块重启帧,返回帧的长度
extern unsigned char InDTU332W_Single_RD_Frame(unsigned char* tx_buf, eInDTU332W_CFG_TAG  tag_read);//填充GPRS模块读取单个配置帧,返回帧的长度
extern unsigned char InDTU332W_Single_WR_Frame(unsigned char* tx_buf, eInDTU332W_CFG_TAG  tag_write);//填充GPRS模块设置单个配置帧,返回帧的长度

extern unsigned char InDTU332W_Status_RD_Frame(unsigned char* tx_buf, eInDTU332W_CFG_TAG  tag_read);//填充GPRS模块读取单个状态TAG帧,返回帧的长度
//返回值0表示响应帧OK,返回值1表示响应帧异常
extern unsigned char InDTU332W_Response_Handler(unsigned char* rx_buf,unsigned short rx_len);//InDTU332W接收帧处理


extern eInDTU332W_CMD     	g_eInDTU332W_cmd;//最后1次发送的帧命令
extern eInDTU332W_CFG_TAG 	g_eInDTU332W_cfg_tag;

extern unsigned char    g_InDTU332W_exist_flag;//值0表示未检测到GPRS模块,值1表示检测到GPRS模块
extern unsigned char	g_InDTU332W_login_flag;//值0表示登录不OK，值1 OK
extern unsigned char	g_InDTU332W_conn_flag;//值0表示连接服务器不OK，值1 表示连接服务器OK

extern unsigned char  g_InDTU332W_gateway_ip[4];//服务器IP
extern unsigned short g_InDTU332W_gateway_port;//服务器端口

#ifdef __cplusplus
    }
#endif

#endif //__InDTU332W_DRIVER_H__

