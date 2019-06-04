#include "Car_lock.h"
#include "sw_fifo.h"
//#include "type_def.h"
ecarlock_park_st g_car_st[MAX_CARLOCK_NUM];

unsigned char g_carlock_addr[MAX_CARLOCK_NUM]={0,1};
unsigned char CRC8_TAB[256]=
{
	0x00,0x5e,0xbc,0xe2,0x61,0x3f,0xdd,0x83,
	0xc2,0x9c,0x7e,0x20,0xa3,0xfd,0x1f,0x41,
	0x9d,0xc3,0x21,0x7f,0xfc,0xa2,0x40,0x1e,
	0x5f,0x01,0xe3,0xbd,0x3e,0x60,0x82,0xdc,
	0x23,0x7d,0x9f,0xc1,0x42,0x1c,0xfe,0xa0,
	0xe1,0xbf,0x5d,0x03,0x80,0xde,0x3c,0x62,
	0xbe,0xe0,0x02,0x5c,0xdf,0x81,0x63,0x3d,
	0x7c,0x22,0xc0,0x9e,0x1d,0x43,0xa1,0xff,
	0x46,0x18,0xfa,0xa4,0x27,0x79,0x9b,0xc5,
	0x84,0xda,0x38,0x66,0xe5,0xbb,0x59,0x07,
	0xdb,0x85,0x67,0x39,0xba,0xe4,0x06,0x58,
	0x19,0x47,0xa5,0xfb,0x78,0x26,0xc4,0x9a,
	0x65,0x3b,0xd9,0x87,0x04,0x5a,0xb8,0xe6,
	0xa7,0xf9,0x1b,0x45,0xc6,0x98,0x7a,0x24,
	0xf8,0xa6,0x44,0x1a,0x99,0xc7,0x25,0x7b,
	0x3a,0x64,0x86,0xd8,0x5b,0x05,0xe7,0xb9,
	0x8c,0xd2,0x30,0x6e,0xed,0xb3,0x51,0x0f,
	0x4e,0x10,0xf2,0xac,0x2f,0x71,0x93,0xcd,
	0x11,0x4f,0xad,0xf3,0x70,0x2e,0xcc,0x92,
	0xd3,0x8d,0x6f,0x31,0xb2,0xec,0x0e,0x50,
	0xaf,0xf1,0x13,0x4d,0xce,0x90,0x72,0x2c,
	0x6d,0x33,0xd1,0x8f,0x0c,0x52,0xb0,0xee,
	0x32,0x6c,0x8e,0xd0,0x53,0x0d,0xef,0xb1,
	0xf0,0xae,0x4c,0x12,0x91,0xcf,0x2d,0x73,
	0xca,0x94,0x76,0x28,0xab,0xf5,0x17,0x49,
	0x08,0x56,0xb4,0xea,0x69,0x37,0xd5,0x8b,
	0x57,0x09,0xeb,0xb5,0x36,0x68,0x8a,0xd4,
	0x95,0xcb,0x29,0x77,0xf4,0xaa,0x48,0x16,
	0xe9,0xb7,0x55,0x0b,0x88,0xd6,0x34,0x6a,
	0x2b,0x75,0x97,0xc9,0x4a,0x14,0xf6,0xa8,
	0x74,0x2a,0xc8,0x96,0x15,0x4b,0xa9,0xf7,
	0xb6,0xe8,0x0a,0x54,0xd7,0x89,0x6b,0x35
};
//precrc为CRC-8初始因子，如无特殊规定，则默认为0x00
unsigned char CRC8_Tab(unsigned char* ptr,unsigned char len,unsigned char precrc)
{
	unsigned char index;
	unsigned char crc8 = precrc;
	while(len--)
	{
		index=crc8^(*(ptr++));
		//__NOP();
		//__NOP();
		crc8=CRC8_TAB[index];
		//__NOP();
		//__NOP();
	}
	//__NOP();
	//__NOP();
	return(crc8);
}
//查询设备ID CMD 0x1D 其地址位为0xff；
//开锁： CMD 0X01 
//闭锁： CMD 0x02
//读取锁状态： CMD 0X06
//读取超声波检测周期 CMD  0X08
//读取超声波滤波时间 CMD  0X0A
//超声波运行参数查询 CMD  0X14
//软件版本号查询：   CMD  0X1A
unsigned char  Fill_AP01_Frame(unsigned char* SendBuf,unsigned char address,unsigned char CMD)
{
	unsigned char tx_len = sizeof(CAR_AP01_Frame);
	CAR_AP01_Frame*  Head_Frame ;
	Head_Frame = (CAR_AP01_Frame*)(SendBuf);
	Head_Frame->Start_Sign= 0x55;  
	Head_Frame->Address = address; //车位锁设备的地址
	Head_Frame->Length = 1; 
	Head_Frame->CMD = CMD;//操作码，
	Head_Frame->CHK = CRC8_Tab(&Head_Frame->Address,(tx_len-3),0X00);
	Head_Frame->END = 0XAA;//操作码	

	return tx_len;
}
//设置超声波检测周期： CMD 0X07
//设置超声波滤波时间： CMD 0X09
//蜂鸣器设置或查询：   CMD 0X15
//设置超声波探测开关或查询： CMD 0X1B
//ADDR设置：           CMD 0X1C
//波特率设置：         CMD 0X1E
unsigned char Fill_AP02_Frame(unsigned char* SendBuf,unsigned char address,unsigned char CMD,unsigned char data)
{
	unsigned char tx_len = sizeof(CAR_AP02_Frame),i=0;
	CAR_AP02_Frame*  Head_Frame ;
	Head_Frame = (CAR_AP02_Frame*)(SendBuf);
	Head_Frame->Start_Sign= 0x55;  
	Head_Frame->Address = address;//address; //车位锁设备的地址
	Head_Frame->Length = 2; 
	Head_Frame->CMD = CMD;//操作码，
	Head_Frame->DATA = data;//数据，
	Head_Frame->CHK = CRC8_Tab(&Head_Frame->Address,(tx_len-3),0X00);
	Head_Frame->END = 0XAA;//操作码，	
	return tx_len;
}

unsigned char CAR_LOCK_Rx_Handler(unsigned char* rx_buf,unsigned char rx_len )
{
	//unsigned char rx_buf[30];

	unsigned char rx_car_addr = 0;
	unsigned char rx_data_len = rx_buf[2];//should equal to rx_len
	unsigned char result = 0;//0表示数据接收处理OK
	unsigned char rx_cmd_resp = 0;
	unsigned char rx_dev_index;
	unsigned char i,k;
	rx_car_addr = rx_buf[1];
	rx_cmd_resp = rx_buf[3];
	if(rx_len < 6)
		return 1;
	for(i=0;i<2;i++)//目前按最大2个检测模块配置
		if(g_carlock_addr[i] == rx_car_addr)
			break;
	if(i==2)//地址不匹配
		return 3;
	
	rx_dev_index = i;				
	if((rx_buf[0] == 0x5A || rx_buf[0] == 0x5B) && (rx_buf[rx_len-1] == 0xAA))
	{
		if(rx_buf[rx_len-2] == CRC8_Tab(&(rx_buf[1]),(rx_len-3),0x00))
		{
			if( GET_LOCK_ST == rx_cmd_resp)//读取锁状态
			{
				if(rx_buf[0] == 0x5B)//设备执行命令失败
					result = 6;	
				else
				{	
				  if((rx_buf[5] == 0x00)&&(rx_buf[4] == 0x01))
					{
					  g_car_st[rx_dev_index].lock_st = 0x10;
					}
					else{
					  g_car_st[rx_dev_index].lock_st = rx_buf[4];
					}
					g_car_st[rx_dev_index].CSB = rx_buf[5]; //CSB—车辆传感器状态
					g_car_st[rx_dev_index].BAT_SOC = rx_buf[6]; //BAT—电池电量，03满格，02中间，01低电 00没电
				//	printf("\n-----rx_dev_index =%d- g_car_st[rx_dev_index].lock_st =%0x \n",rx_dev_index,g_car_st[rx_dev_index].lock_st);
				//	printf("\n-----rx_buf[4] =%0x-rx_buf[5] =%0x rx_buf[6] =%0x\n",rx_buf[4],rx_buf[5],rx_buf[6]);
				}
				
			}
			if( GET_ULTRASONIC_CYC == rx_cmd_resp)//读取超声波检测周期
			{
				if(rx_buf[0] == 0x5B)//设备执行命令失败
					result = 6;	
				else
					g_car_st[rx_dev_index].ULTRASONIC_CYC = rx_buf[4];
			}
			if( GET_ULTRASONIC_TIME == rx_cmd_resp)//读取超声波滤波时间
			{
				if(rx_buf[0] == 0x5B)//设备执行命令失败
					result = 6;	
				else
					g_car_st[rx_dev_index].ULTRASONIC_TIME = rx_buf[4];
			}
			if( GET_ULTRASONIC_PARA == rx_cmd_resp)//读取超声运行参数
			{
				if(rx_buf[0] == 0x5B)//设备执行命令失败
					result = 6;	
				else
				{
					g_car_st[rx_dev_index].ULTRASONIC_PARA[0] = rx_buf[4];
					g_car_st[rx_dev_index].ULTRASONIC_PARA[1] = rx_buf[5];
				}
			}
			if( GET_FIRMWARE_VER == rx_cmd_resp)//获取软硬件版本号
			{
				if(rx_buf[0] == 0x5B)//设备执行命令失败
					result = 6;	
				else
				{
					g_car_st[rx_dev_index].FIRMWARE_VER[0] = rx_buf[4];
					g_car_st[rx_dev_index].FIRMWARE_VER[1] = rx_buf[5];
				}
			}
			if( SET_ULTRASONIC == rx_cmd_resp)//设置超声波探测开关或查询：
			{
				if(rx_buf[0] == 0x5B)//设备执行命令失败)
					result = 6;
				else
					g_car_st[rx_dev_index].ULTRASONIC = rx_buf[4];
			}
			if( GET_Equipment_ID == rx_cmd_resp)//获取设备ID：
			{
				if(rx_buf[0] == 0x5B)//设备执行命令失败)
					result = 6;
				else
					g_car_st[rx_dev_index].module_ID = rx_buf[4];
			}
			if( SET_BEEP == rx_cmd_resp)//查询或设置蜂鸣器
			{
				if(rx_buf[0] == 0x5B)//设备执行命令失败)
					result = 6;
				else
					g_car_st[rx_dev_index].BEEP = rx_buf[4];
			}
			if( OPEN_LOCK == rx_cmd_resp)//开锁
			{
				if(rx_buf[0] == 0x5B)//设备执行命令失败
					result = 6;				
			}
			if( SHUT_LOCK == rx_cmd_resp)//闭锁
			{
				if(rx_buf[0] == 0x5B)//设备执行命令失败
					result = 6;
			}
			if( SET_ULTRASONIC_CYC == rx_cmd_resp)//设置超声波检测周期：
			{
				if(rx_buf[0] == 0x5B)//设备执行命令失败
					result = 6;	
			}
			if( SET_ULTRASONIC_TIME == rx_cmd_resp)//设置超声波滤波时间：
			{
				if(rx_buf[0] == 0x5B)//设备执行命令失败
					result = 6;	
			}
			if( SET_ADDR == rx_cmd_resp)//设置ADDR：
			{
				if(rx_buf[0] == 0x5B)//设备执行命令失败
					result = 6;	
			}
			if( SET_BAUD == rx_cmd_resp)//设置波特率：
			{
				if(rx_buf[0] == 0x5B)//设备执行命令失败
					result = 6;	
			}
			return result;
		}
		else//
			return 5;
	}
	else
		return 4;

}
