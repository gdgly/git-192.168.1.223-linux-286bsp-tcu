/*******************************************************************************
  Copyright(C)    2014-2015,      EAST Co.Ltd.
  File name :     meter_645_2007.c
  Author :        dengjh
  Version:        1.0
  Date:           2016.8.30
  Description:    
  Other:          
  Function List:  
  History:        
          <author>   <time>   <version>   <desc>
           dengjh    2016.8.30   1.0      Create
			   	DCM3366Q 型直流电能表 DL/T645-2007 通信协议
*******************************************************************************/
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <asm/ioctls.h>


#include "globalvar.h"
#define METER_645_DEBUG 0        //ISO8583_DEBUG 调试使能

extern UINT32 Meter_Read_Write_Watchdog;
extern UINT32 Dc_shunt_Set_meter_flag_1;
extern UINT32 Dc_shunt_Set_meter_flag_2;

//----645-1997读数据格式------------
typedef enum Dlt645Read_2007{
    Dlt645_Read_0x681,                    //0
    Dlt645_Read_SrcAddr,             //地址 发送低到高 A0 ... A5   BCD   
    Dlt645_Read_0x682=7,
    Dlt645_Read_c,                   //控制码 8
    Dlt645_Read_L,                   //数据长度 9 
    Dlt645_Read_ID0,                 //标识0    10
    Dlt645_Read_ID1,                 //标识1     11
		Dlt645_Read_ID2,                 //标识2     12
    Dlt645_Read_ID3,                 //标识3     13
    Dlt645_Read_CS,                  //校验      14
    Dlt645_Read_16                   //结束,位置=13，从0起   15
}DLT645READ_2007;

//----645-2007写数据格式------------
typedef enum Dlt645Write_2007{
    Dlt645_Write_0x681,
    Dlt645_Write_SrcAddr,             //地址 发送低到高 A0 ... A5   BCD
    Dlt645_Write_0x682=7,
    Dlt645_Write_c,                   //控制码
    Dlt645_Write_L,                   //数据长度
    Dlt645_Write_ID0,                 //标识0
    Dlt645_Write_ID1,                 //标识1
    Dlt645_Write_ID2,                 //标识2
    Dlt645_Write_ID3,                 //标识3
    Dlt645_Write_pa,					        //权限
    Dlt645_Write_p0,					        //密码
    Dlt645_Write_c0,		              //操作者标识
    Dlt645_Write_data		              //待写入的数据
}DLT645WRITE_2007;


// 645-2007                                              0    1    2    3    4    5    6   7    8    9    10   11   12   13    14    15
unsigned char Mt_645_2007_All_Electricity_cmd[16]={0x68,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0x68,0x11,0x04,0x00,0x00,0x01,0x00,0x00,0x16};//当前总电量  0.01KWH 4字节BCD
unsigned char Mt_645_2007_Tip_Electricity_cmd[16]={0x68,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0x68,0x11,0x04,0x00,0x01,0x01,0x00,0x00,0x16};//当前尖电量  0.01KWH 4字节BCD
unsigned char Mt_645_2007_Peak_Electricity_cmd[16]={0x68,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0x68,0x11,0x04,0x00,0x02,0x01,0x00,0x00,0x16};//当前峰电量  0.01KWH 4字节BCD
unsigned char Mt_645_2007_Flat_Electricity_cmd[16]={0x68,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0x68,0x11,0x04,0x00,0x03,0x01,0x00,0x00,0x16};//当前平电量  0.01KWH 4字节BCD
unsigned char Mt_645_2007_Valley_Electricity_cmd[16]={0x68,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0x68,0x11,0x04,0x00,0x04,0x01,0x00,0x00,0x16};//当前谷电量  0.01KWH 4字节BCD

unsigned char Mt_645_2007_C_cmd[16]={0x68,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0x68,0x11,0x04,0x00,0x01,0x00,0x02,0x00,0x16};//1路电流 0.0001A  4字节BCD
unsigned char Mt_645_2007_V_cmd[16]={0x68,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0x68,0x11,0x04,0x00,0x02,0x00,0x02,0x00,0x16};//1路电压 0.001V 4字节BCD

//const unsigned char Mt_645_2007_C_RATIO_SET_cmd[16]={0x68,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0x68,0x14,0x04,0x01,0x01,0x00,0x04,0x00,0x16};//最大电流比值 XXXX.XX 3字节BCD---电流变比？


/*字符串大小比较，比较最大个数为 int 型
//从后向前比较
//输入：两个字符串地址*s1 ,*s2；
//返回：== 0 相等；== -1 s1 < s2；== 1 s1 > s2：
--------------------------------------*/
static signed char	StringCompareConverse(unsigned char *su1, unsigned char *su2, unsigned int n) 
{
	su1 += (n-1);
	su2 += (n-1);
	for (; 0 < n; --su1, --su2, --n){
		if (*su1 != *su2){
			return (*su1 < *su2 ? -1 : +1);
		}
	}

	return (0);
}


/*********************************************************
函数原形：void AddSun0x33(char * P, char len, char Cost)
功    能：+33h或者-33h //cost=1=+33,其它=-33
Cost=1=+33,其它=-33
**********************************************************/
static void AddSun0x33(unsigned char * P, unsigned char len, unsigned char Cost)
{
	unsigned char i;

	for(i=0;i<len;i++){
		if(1 == Cost){
			P[i] += 0x33;
		}else{
			P[i] -= 0x33;
		}    
	}
}

//函数功能：计算CS, 返回参数：所计算的累加和；
static unsigned char Meter_645_cs(unsigned char *p, int L)
{
	int i;
	unsigned char c = 0x00;

	for(i=0; i<L; i++){
		c = (*(p+i)) + c;
	}

	return c;
}
//读总电量--根据雅达电表自定义的3位小数，
static int All_kwh_read_New(unsigned int *meter_now_All_KWH, int fd, unsigned char *addr)
{
    int i=0,j=0,start_flag=0;
    int Count,status;

    unsigned char SentBuf[255];
    unsigned char recvBuf[255];
 
		SentBuf[0] = 0xFE;
		SentBuf[1] = 0xFE;
		SentBuf[2] = 0xFE;
		SentBuf[3] = 0xFE;
		Mt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 0] = str2_to_BCD(&addr[10]); //A0
		Mt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 1] = str2_to_BCD(&addr[8]); //A1
		Mt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 2] = str2_to_BCD(&addr[6]); //A2
		Mt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 3] = str2_to_BCD(&addr[4]); //A3
		Mt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 4] = str2_to_BCD(&addr[2]); //A4
		Mt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 5] = str2_to_BCD(&addr[0]); //A5
		Mt_645_2007_All_Electricity_cmd[Dlt645_Read_ID0] = 0x00;
		Mt_645_2007_All_Electricity_cmd[Dlt645_Read_ID1] = 0x00;
		Mt_645_2007_All_Electricity_cmd[Dlt645_Read_ID2] = 0xD1; //当前正向有功总电能
		Mt_645_2007_All_Electricity_cmd[Dlt645_Read_ID3] = 0x00;
		AddSun0x33(&Mt_645_2007_All_Electricity_cmd[Dlt645_Read_ID0], Mt_645_2007_All_Electricity_cmd[Dlt645_Read_L], 1);//+33处理  Cost=1=+33,其它=-33
		Mt_645_2007_All_Electricity_cmd[Dlt645_Read_CS] = Meter_645_cs(Mt_645_2007_All_Electricity_cmd,(sizeof(Mt_645_2007_All_Electricity_cmd)-2));	
		memcpy(&SentBuf[4], Mt_645_2007_All_Electricity_cmd, sizeof(Mt_645_2007_All_Electricity_cmd));//复制表地址								
		
		Led_Relay_Control(6, 0);
		write(fd, SentBuf, sizeof(Mt_645_2007_All_Electricity_cmd) + 4);
		do{
			ioctl(fd, TIOCSERGETLSR, &status);
		} while (status!=TIOCSER_TEMT);
		usleep(5000);
		Led_Relay_Control(6, 1);
		Count = read_datas_tty(fd, recvBuf, 255, 1000000, 300000);
		if(Count >= (sizeof(Mt_645_2007_All_Electricity_cmd)+4))
		{
			for(i=0;i<5;i++)
			{
				if(recvBuf[i] == 0x68){
					start_flag = i;
					break;
				}
			}

			if(i > 4){
				return (-1);
			}

			if((Count-start_flag) != (sizeof(Mt_645_2007_All_Electricity_cmd)+4))
			{
				return (-1);
			}

			if((Meter_645_cs(&recvBuf[start_flag], Count-start_flag-2)) == recvBuf[Count-2])
			{
				if((recvBuf[start_flag] == 0x68)&&(recvBuf[Count-1] == 0x16))
				{//起始符，结束符正确				
					AddSun0x33(&recvBuf[start_flag + Dlt645_Read_ID0], 8, 2);//减33h       Cost=1=+33,其它=-33	
					for(i=0;i<8;i++)
					{
						printf("---recvBuf[start_flag + Dlt645_Read_ID0 + i] =%0x \n",recvBuf[start_flag + Dlt645_Read_ID0 + i]);
					}
					
					if((recvBuf[start_flag+Dlt645_Read_ID0] == 0x00)&&(recvBuf[start_flag+Dlt645_Read_ID1] == 0x00)&&(recvBuf[start_flag+Dlt645_Read_ID2] == 0xD1)&&(recvBuf[start_flag+Dlt645_Read_ID3] == 0x00))
					{//标识符正确

						*meter_now_All_KWH = (((recvBuf[start_flag + Dlt645_Read_ID3 + 4])<<24)|((recvBuf[start_flag + Dlt645_Read_ID3 + 3])<<16)|((recvBuf[start_flag + Dlt645_Read_ID3 + 2])<<8)|(recvBuf[start_flag + Dlt645_Read_ID3 + 1]))/10;
														 
						return (0);//抄收数据正确
					}
				}
			}
		}
		//printf("All_kwh_read Count=%02d\n",Count);
		return (-1);
}
//读总电量
static int All_kwh_read(UINT32 *meter_now_All_KWH, int fd, unsigned char *addr)
{
	int i=0,j=0,start_flag=0;
	int Count,status;

	unsigned char SentBuf[MAX_MODBUS_FRAMESIZE];
	unsigned char recvBuf[MAX_MODBUS_FRAMESIZE];
 
	SentBuf[0] = 0xFE;
	SentBuf[1] = 0xFE;
	SentBuf[2] = 0xFE;
	SentBuf[3] = 0xFE;
  Mt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 0] = str2_to_BCD(&addr[10]); //A0
  Mt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 1] = str2_to_BCD(&addr[8]); //A1
  Mt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 2] = str2_to_BCD(&addr[6]); //A2
  Mt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 3] = str2_to_BCD(&addr[4]); //A3
  Mt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 4] = str2_to_BCD(&addr[2]); //A4
  Mt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 5] = str2_to_BCD(&addr[0]); //A5
  Mt_645_2007_All_Electricity_cmd[Dlt645_Read_ID0] = 0x00;
  Mt_645_2007_All_Electricity_cmd[Dlt645_Read_ID1] = 0x00;
  Mt_645_2007_All_Electricity_cmd[Dlt645_Read_ID2] = 0x01;
  Mt_645_2007_All_Electricity_cmd[Dlt645_Read_ID3] = 0x00;
	AddSun0x33(&Mt_645_2007_All_Electricity_cmd[Dlt645_Read_ID0], Mt_645_2007_All_Electricity_cmd[Dlt645_Read_L], 1);//+33处理  Cost=1=+33,其它=-33
  Mt_645_2007_All_Electricity_cmd[Dlt645_Read_CS] = Meter_645_cs(&Mt_645_2007_All_Electricity_cmd,(sizeof(Mt_645_2007_All_Electricity_cmd)-2));	
	memcpy(&SentBuf[4], Mt_645_2007_All_Electricity_cmd, sizeof(Mt_645_2007_All_Electricity_cmd));//复制表地址								
	
	Led_Relay_Control(6, 0);
	write(fd, SentBuf, sizeof(Mt_645_2007_All_Electricity_cmd) + 4);
	do{
		ioctl(fd, TIOCSERGETLSR, &status);
	} while (status!=TIOCSER_TEMT);
	usleep(5000);
	Led_Relay_Control(6, 1);

	Count = read_datas_tty(fd, recvBuf, MAX_MODBUS_FRAMESIZE, 500000, 30000);
  if(Count >= (sizeof(Mt_645_2007_All_Electricity_cmd)+4)){//响应可能没有0xfe引导码  +4个字节数据
#if METER_645_DEBUG == 1
			printf("kwh_read recv server length = %d,data= ", Count);
			for(j=0; j<Count; j++)printf("%02x ",recvBuf[j]);
			printf("\n");
#endif
  	for(i=0;i<5;i++){
  		if(recvBuf[i] == 0x68){
  			start_flag = i;
  			break;
  		}
  	}

  	if(i > 4){
  		return (-1);
  	}

  	if((Count-start_flag) != (sizeof(Mt_645_2007_All_Electricity_cmd)+4)){
  		return (-1);
  	}

		if((Meter_645_cs(&recvBuf[start_flag], Count-start_flag-2)) == recvBuf[Count-2]){
			if((recvBuf[start_flag] == 0x68)&&(recvBuf[Count-1] == 0x16)){//起始符，结束符正确
				AddSun0x33(&recvBuf[start_flag + Dlt645_Read_ID0], 8, 2);//减33h       Cost=1=+33,其它=-33	
				if((recvBuf[start_flag+Dlt645_Read_ID0] == 0x00)&&(recvBuf[start_flag+Dlt645_Read_ID1] == 0x00)&&(recvBuf[start_flag+Dlt645_Read_ID2] == 0x01)&&(recvBuf[start_flag+Dlt645_Read_ID3] == 0x00)){//标识符正确

	       	*meter_now_All_KWH = (bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 4])*1000000 + \
					                 bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 3])*10000 + \
					                 bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 2])*100 + \
					                 bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 1]));
				// printf("MEter meter_now_All_KWH xxxxxxxxxxxxxxxxxxxxxx= %d KWH\n", *meter_now_All_KWH);
								 
					return (0);//抄收数据正确
				}
			}
		}
	}

	return (-1);
}

//读电表的尖电量
static int Tip_kwh_read(UINT32 *meter_now_Tip_KWH, int fd, unsigned char *addr)
{
	int i=0,j=0,start_flag=0;
	int Count,status;

	unsigned char SentBuf[MAX_MODBUS_FRAMESIZE];
	unsigned char recvBuf[MAX_MODBUS_FRAMESIZE];
 
	SentBuf[0] = 0xFE;
	SentBuf[1] = 0xFE;
	SentBuf[2] = 0xFE;
	SentBuf[3] = 0xFE;
  Mt_645_2007_Tip_Electricity_cmd[Dlt645_Read_SrcAddr + 0] = str2_to_BCD(&addr[10]); //A0
  Mt_645_2007_Tip_Electricity_cmd[Dlt645_Read_SrcAddr + 1] = str2_to_BCD(&addr[8]); //A1
  Mt_645_2007_Tip_Electricity_cmd[Dlt645_Read_SrcAddr + 2] = str2_to_BCD(&addr[6]); //A2
  Mt_645_2007_Tip_Electricity_cmd[Dlt645_Read_SrcAddr + 3] = str2_to_BCD(&addr[4]); //A3
  Mt_645_2007_Tip_Electricity_cmd[Dlt645_Read_SrcAddr + 4] = str2_to_BCD(&addr[2]); //A4
  Mt_645_2007_Tip_Electricity_cmd[Dlt645_Read_SrcAddr + 5] = str2_to_BCD(&addr[0]); //A5
  
	Mt_645_2007_Tip_Electricity_cmd[Dlt645_Read_ID0] = 0x00;
  Mt_645_2007_Tip_Electricity_cmd[Dlt645_Read_ID1] = 0x01;
  Mt_645_2007_Tip_Electricity_cmd[Dlt645_Read_ID2] = 0x01;
  Mt_645_2007_Tip_Electricity_cmd[Dlt645_Read_ID3] = 0x00;
	
	AddSun0x33(&Mt_645_2007_Tip_Electricity_cmd[Dlt645_Read_ID0], Mt_645_2007_Tip_Electricity_cmd[Dlt645_Read_L], 1);//+33处理  Cost=1=+33,其它=-33
  Mt_645_2007_Tip_Electricity_cmd[Dlt645_Read_CS] = Meter_645_cs(&Mt_645_2007_Tip_Electricity_cmd,(sizeof(Mt_645_2007_Tip_Electricity_cmd)-2));	
	memcpy(&SentBuf[4], Mt_645_2007_Tip_Electricity_cmd, sizeof(Mt_645_2007_Tip_Electricity_cmd));//复制表地址								
	
	Led_Relay_Control(6, 0);
	write(fd, SentBuf, sizeof(Mt_645_2007_Tip_Electricity_cmd) + 4);
	do{
		ioctl(fd, TIOCSERGETLSR, &status);
	} while (status!=TIOCSER_TEMT);
	usleep(5000);
	Led_Relay_Control(6, 1);

	Count = read_datas_tty(fd, recvBuf, MAX_MODBUS_FRAMESIZE, 500000, 30000);
  if(Count >= (sizeof(Mt_645_2007_Tip_Electricity_cmd)+4)){//响应可能没有0xfe引导码  +4个字节数据
#if METER_645_DEBUG == 1
			printf("kwh_read recv server length = %d,data= ", Count);
			for(j=0; j<Count; j++)printf("%02x ",recvBuf[j]);
			printf("\n");
#endif
  	for(i=0;i<5;i++){
  		if(recvBuf[i] == 0x68){
  			start_flag = i;
  			break;
  		}
  	}

  	if(i > 4){
  		return (-1);
  	}

  	if((Count-start_flag) != (sizeof(Mt_645_2007_Tip_Electricity_cmd)+4)){
  		return (-1);
  	}
		
		if((Meter_645_cs(&recvBuf[start_flag], Count-start_flag-2)) == recvBuf[Count-2]){
			if((recvBuf[start_flag] == 0x68)&&(recvBuf[Count-1] == 0x16)){//起始符，结束符正确
				AddSun0x33(&recvBuf[start_flag + Dlt645_Read_ID0], 8, 2);//减33h       Cost=1=+33,其它=-33														
				if((recvBuf[start_flag+Dlt645_Read_ID0] == 0x00)&&(recvBuf[start_flag+Dlt645_Read_ID1] == 0x01)&&(recvBuf[start_flag+Dlt645_Read_ID2] == 0x01)&&(recvBuf[start_flag+Dlt645_Read_ID3] == 0x00)){//标识符正确

	       	*meter_now_Tip_KWH = (bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 4])*1000000 + \
					                 bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 3])*10000 + \
					                 bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 2])*100 + \
					                 bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 1]));
													 
			//printf("meter_now_KWH xxxxxxxxxxDCxxxxxxxxxxxx= %d \n", *meter_now_KWH);

					return (0);//抄收数据正确
				}
			}
		}
	}

	return (-1);
}

//读电表峰电量
static int Peak_kwh_read(UINT32 *meter_now_Peak_KWH, int fd, unsigned char *addr)
{
	int i=0,j=0,start_flag=0;
	int Count,status;

	unsigned char SentBuf[MAX_MODBUS_FRAMESIZE];
	unsigned char recvBuf[MAX_MODBUS_FRAMESIZE];
 
	SentBuf[0] = 0xFE;
	SentBuf[1] = 0xFE;
	SentBuf[2] = 0xFE;
	SentBuf[3] = 0xFE;
  Mt_645_2007_Peak_Electricity_cmd[Dlt645_Read_SrcAddr + 0] = str2_to_BCD(&addr[10]); //A0
  Mt_645_2007_Peak_Electricity_cmd[Dlt645_Read_SrcAddr + 1] = str2_to_BCD(&addr[8]); //A1
  Mt_645_2007_Peak_Electricity_cmd[Dlt645_Read_SrcAddr + 2] = str2_to_BCD(&addr[6]); //A2
  Mt_645_2007_Peak_Electricity_cmd[Dlt645_Read_SrcAddr + 3] = str2_to_BCD(&addr[4]); //A3
  Mt_645_2007_Peak_Electricity_cmd[Dlt645_Read_SrcAddr + 4] = str2_to_BCD(&addr[2]); //A4
  Mt_645_2007_Peak_Electricity_cmd[Dlt645_Read_SrcAddr + 5] = str2_to_BCD(&addr[0]); //A5
	
	Mt_645_2007_Peak_Electricity_cmd[Dlt645_Read_ID0] = 0x00;
  Mt_645_2007_Peak_Electricity_cmd[Dlt645_Read_ID1] = 0x02;
  Mt_645_2007_Peak_Electricity_cmd[Dlt645_Read_ID2] = 0x01;
  Mt_645_2007_Peak_Electricity_cmd[Dlt645_Read_ID3] = 0x00;
	
	AddSun0x33(&Mt_645_2007_Peak_Electricity_cmd[Dlt645_Read_ID0], Mt_645_2007_Peak_Electricity_cmd[Dlt645_Read_L], 1);//+33处理  Cost=1=+33,其它=-33
  Mt_645_2007_Peak_Electricity_cmd[Dlt645_Read_CS] = Meter_645_cs(&Mt_645_2007_Peak_Electricity_cmd,(sizeof(Mt_645_2007_Peak_Electricity_cmd)-2));	
	memcpy(&SentBuf[4], Mt_645_2007_Peak_Electricity_cmd, sizeof(Mt_645_2007_Peak_Electricity_cmd));//复制表地址								
	
	Led_Relay_Control(6, 0);
	write(fd, SentBuf, sizeof(Mt_645_2007_Peak_Electricity_cmd) + 4);
	do{
		ioctl(fd, TIOCSERGETLSR, &status);
	} while (status!=TIOCSER_TEMT);
	usleep(5000);
	Led_Relay_Control(6, 1);

	Count = read_datas_tty(fd, recvBuf, MAX_MODBUS_FRAMESIZE, 500000, 30000);
  if(Count >= (sizeof(Mt_645_2007_Peak_Electricity_cmd)+4)){//响应可能没有0xfe引导码  +4个字节数据
#if METER_645_DEBUG == 1
			printf("kwh_read recv server length = %d,data= ", Count);
			for(j=0; j<Count; j++)printf("%02x ",recvBuf[j]);
			printf("\n");
#endif
  	for(i=0;i<5;i++){
  		if(recvBuf[i] == 0x68){
  			start_flag = i;
  			break;
  		}
  	}

  	if(i > 4){
  		return (-1);
  	}

  	if((Count-start_flag) != (sizeof(Mt_645_2007_Peak_Electricity_cmd)+4)){
  		return (-1);
  	}
		
		if((Meter_645_cs(&recvBuf[start_flag], Count-start_flag-2)) == recvBuf[Count-2]){
			if((recvBuf[start_flag] == 0x68)&&(recvBuf[Count-1] == 0x16)){//起始符，结束符正确
				AddSun0x33(&recvBuf[start_flag + Dlt645_Read_ID0], 8, 2);//减33h       Cost=1=+33,其它=-33														
				if((recvBuf[start_flag+Dlt645_Read_ID0] == 0x00)&&(recvBuf[start_flag+Dlt645_Read_ID1] == 0x02)&&(recvBuf[start_flag+Dlt645_Read_ID2] == 0x01)&&(recvBuf[start_flag+Dlt645_Read_ID3] == 0x00)){//标识符正确

	       	*meter_now_Peak_KWH = (bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 4])*1000000 + \
					                 bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 3])*10000 + \
					                 bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 2])*100 + \
					                 bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 1]));
													 
//			printf("meter_now_Peak_KWH xxxxxxxxxxxxxxxxxxxxxx= %d \n", *meter_now_Peak_KWH);

					return (0);//抄收数据正确
				}
			}
		}
	}

	return (-1);
}

//读电表平电量
static int Flat_kwh_read(UINT32 *meter_now_Flat_KWH, int fd, unsigned char *addr)
{
	int i=0,j=0,start_flag=0;
	int Count,status;

	unsigned char SentBuf[MAX_MODBUS_FRAMESIZE];
	unsigned char recvBuf[MAX_MODBUS_FRAMESIZE];
 
	SentBuf[0] = 0xFE;
	SentBuf[1] = 0xFE;
	SentBuf[2] = 0xFE;
	SentBuf[3] = 0xFE;
  Mt_645_2007_Flat_Electricity_cmd[Dlt645_Read_SrcAddr + 0] = str2_to_BCD(&addr[10]); //A0
  Mt_645_2007_Flat_Electricity_cmd[Dlt645_Read_SrcAddr + 1] = str2_to_BCD(&addr[8]); //A1
  Mt_645_2007_Flat_Electricity_cmd[Dlt645_Read_SrcAddr + 2] = str2_to_BCD(&addr[6]); //A2
  Mt_645_2007_Flat_Electricity_cmd[Dlt645_Read_SrcAddr + 3] = str2_to_BCD(&addr[4]); //A3
  Mt_645_2007_Flat_Electricity_cmd[Dlt645_Read_SrcAddr + 4] = str2_to_BCD(&addr[2]); //A4
  Mt_645_2007_Flat_Electricity_cmd[Dlt645_Read_SrcAddr + 5] = str2_to_BCD(&addr[0]); //A5
	
	Mt_645_2007_Flat_Electricity_cmd[Dlt645_Read_ID0] = 0x00;
  Mt_645_2007_Flat_Electricity_cmd[Dlt645_Read_ID1] = 0x03;
  Mt_645_2007_Flat_Electricity_cmd[Dlt645_Read_ID2] = 0x01;
  Mt_645_2007_Flat_Electricity_cmd[Dlt645_Read_ID3] = 0x00;
	
	AddSun0x33(&Mt_645_2007_Flat_Electricity_cmd[Dlt645_Read_ID0], Mt_645_2007_Flat_Electricity_cmd[Dlt645_Read_L], 1);//+33处理  Cost=1=+33,其它=-33
  Mt_645_2007_Flat_Electricity_cmd[Dlt645_Read_CS] = Meter_645_cs(&Mt_645_2007_Flat_Electricity_cmd,(sizeof(Mt_645_2007_Flat_Electricity_cmd)-2));	
	memcpy(&SentBuf[4], Mt_645_2007_Flat_Electricity_cmd, sizeof(Mt_645_2007_Flat_Electricity_cmd));//复制表地址								
	
	Led_Relay_Control(6, 0);
	write(fd, SentBuf, sizeof(Mt_645_2007_Flat_Electricity_cmd) + 4);
	do{
		ioctl(fd, TIOCSERGETLSR, &status);
	} while (status!=TIOCSER_TEMT);
	usleep(5000);
	Led_Relay_Control(6, 1);

	Count = read_datas_tty(fd, recvBuf, MAX_MODBUS_FRAMESIZE, 500000, 30000);
  if(Count >= (sizeof(Mt_645_2007_Flat_Electricity_cmd)+4)){//响应可能没有0xfe引导码  +4个字节数据
#if METER_645_DEBUG == 1
			printf("kwh_read recv server length = %d,data= ", Count);
			for(j=0; j<Count; j++)printf("%02x ",recvBuf[j]);
			printf("\n");
#endif
  	for(i=0;i<5;i++){
  		if(recvBuf[i] == 0x68){
  			start_flag = i;
  			break;
  		}
  	}

  	if(i > 4){
  		return (-1);
  	}

  	if((Count-start_flag) != (sizeof(Mt_645_2007_Flat_Electricity_cmd)+4)){
  		return (-1);
  	}
		
		if((Meter_645_cs(&recvBuf[start_flag], Count-start_flag-2)) == recvBuf[Count-2]){
			if((recvBuf[start_flag] == 0x68)&&(recvBuf[Count-1] == 0x16)){//起始符，结束符正确
				AddSun0x33(&recvBuf[start_flag + Dlt645_Read_ID0], 8, 2);//减33h       Cost=1=+33,其它=-33														
				if((recvBuf[start_flag+Dlt645_Read_ID0] == 0x00)&&(recvBuf[start_flag+Dlt645_Read_ID1] == 0x03)&&(recvBuf[start_flag+Dlt645_Read_ID2] == 0x01)&&(recvBuf[start_flag+Dlt645_Read_ID3] == 0x00)){//标识符正确

	       	*meter_now_Flat_KWH = (bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 4])*1000000 + \
					                 bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 3])*10000 + \
					                 bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 2])*100 + \
					                 bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 1]));
													 
//			printf("meter_now_Flat_KWH xxxxxxxxxxxxxxxxxxxxxx= %d \n", *meter_now_Flat_KWH);

					return (0);//抄收数据正确
				}
			}
		}
	}

	return (-1);
}

//读电表谷电量
static int Valley_kwh_read(UINT32 *meter_now_Valley_KWH, int fd, unsigned char *addr)
{
	int i=0,j=0,start_flag=0;
	int Count,status;

	unsigned char SentBuf[MAX_MODBUS_FRAMESIZE];
	unsigned char recvBuf[MAX_MODBUS_FRAMESIZE];
 
	SentBuf[0] = 0xFE;
	SentBuf[1] = 0xFE;
	SentBuf[2] = 0xFE;
	SentBuf[3] = 0xFE;
  Mt_645_2007_Valley_Electricity_cmd[Dlt645_Read_SrcAddr + 0] = str2_to_BCD(&addr[10]); //A0
  Mt_645_2007_Valley_Electricity_cmd[Dlt645_Read_SrcAddr + 1] = str2_to_BCD(&addr[8]); //A1
  Mt_645_2007_Valley_Electricity_cmd[Dlt645_Read_SrcAddr + 2] = str2_to_BCD(&addr[6]); //A2
  Mt_645_2007_Valley_Electricity_cmd[Dlt645_Read_SrcAddr + 3] = str2_to_BCD(&addr[4]); //A3
  Mt_645_2007_Valley_Electricity_cmd[Dlt645_Read_SrcAddr + 4] = str2_to_BCD(&addr[2]); //A4
  Mt_645_2007_Valley_Electricity_cmd[Dlt645_Read_SrcAddr + 5] = str2_to_BCD(&addr[0]); //A5
	
	Mt_645_2007_Valley_Electricity_cmd[Dlt645_Read_ID0] = 0x00;
  Mt_645_2007_Valley_Electricity_cmd[Dlt645_Read_ID1] = 0x04;
  Mt_645_2007_Valley_Electricity_cmd[Dlt645_Read_ID2] = 0x01;
  Mt_645_2007_Valley_Electricity_cmd[Dlt645_Read_ID3] = 0x00;
	
	AddSun0x33(&Mt_645_2007_Valley_Electricity_cmd[Dlt645_Read_ID0], Mt_645_2007_Valley_Electricity_cmd[Dlt645_Read_L], 1);//+33处理  Cost=1=+33,其它=-33
  Mt_645_2007_Valley_Electricity_cmd[Dlt645_Read_CS] = Meter_645_cs(&Mt_645_2007_Valley_Electricity_cmd,(sizeof(Mt_645_2007_Valley_Electricity_cmd)-2));	
	memcpy(&SentBuf[4], Mt_645_2007_Valley_Electricity_cmd, sizeof(Mt_645_2007_Valley_Electricity_cmd));//复制表地址								
	
	Led_Relay_Control(6, 0);
	write(fd, SentBuf, sizeof(Mt_645_2007_Valley_Electricity_cmd) + 4);
	do{
		ioctl(fd, TIOCSERGETLSR, &status);
	} while (status!=TIOCSER_TEMT);
	usleep(5000);
	Led_Relay_Control(6, 1);

	Count = read_datas_tty(fd, recvBuf, MAX_MODBUS_FRAMESIZE, 500000, 30000);
  if(Count >= (sizeof(Mt_645_2007_Valley_Electricity_cmd)+4)){//响应可能没有0xfe引导码  +4个字节数据
#if METER_645_DEBUG == 1
			printf("kwh_read recv server length = %d,data= ", Count);
			for(j=0; j<Count; j++)printf("%02x ",recvBuf[j]);
			printf("\n");
#endif
  	for(i=0;i<5;i++){
  		if(recvBuf[i] == 0x68){
  			start_flag = i;
  			break;
  		}
  	}

  	if(i > 4){
  		return (-1);
  	}

  	if((Count-start_flag) != (sizeof(Mt_645_2007_Valley_Electricity_cmd)+4)){
  		return (-1);
  	}
		
		if((Meter_645_cs(&recvBuf[start_flag], Count-start_flag-2)) == recvBuf[Count-2]){
			if((recvBuf[start_flag] == 0x68)&&(recvBuf[Count-1] == 0x16)){//起始符，结束符正确
				AddSun0x33(&recvBuf[start_flag + Dlt645_Read_ID0], 8, 2);//减33h       Cost=1=+33,其它=-33														
				if((recvBuf[start_flag+Dlt645_Read_ID0] == 0x00)&&(recvBuf[start_flag+Dlt645_Read_ID1] == 0x04)&&(recvBuf[start_flag+Dlt645_Read_ID2] == 0x01)&&(recvBuf[start_flag+Dlt645_Read_ID3] == 0x00)){//标识符正确

	       	*meter_now_Valley_KWH = (bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 4])*1000000 + \
					                 bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 3])*10000 + \
					                 bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 2])*100 + \
					                 bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 1]));
													 
//			printf("meter_now_Valley_KWH xxxxxxxxxxxxxxxxxxxxxx= %d \n", *meter_now_Valley_KWH);

					return (0);//抄收数据正确
				}
			}
		}
	}

	return (-1);
}


//读电表电压 
static int Volt_read(UINT32 *Output_voltage, int fd, unsigned char *addr)
{
	int i=0,j=0,start_flag=0;
	int Count,status;

	unsigned char SentBuf[MAX_MODBUS_FRAMESIZE];
	unsigned char recvBuf[MAX_MODBUS_FRAMESIZE];
 
	SentBuf[0] = 0xFE;
	SentBuf[1] = 0xFE;
	SentBuf[2] = 0xFE;
	SentBuf[3] = 0xFE;
  Mt_645_2007_V_cmd[Dlt645_Read_SrcAddr + 0] = str2_to_BCD(&addr[10]); //A0
  Mt_645_2007_V_cmd[Dlt645_Read_SrcAddr + 1] = str2_to_BCD(&addr[8]); //A1
  Mt_645_2007_V_cmd[Dlt645_Read_SrcAddr + 2] = str2_to_BCD(&addr[6]); //A2
  Mt_645_2007_V_cmd[Dlt645_Read_SrcAddr + 3] = str2_to_BCD(&addr[4]); //A3
  Mt_645_2007_V_cmd[Dlt645_Read_SrcAddr + 4] = str2_to_BCD(&addr[2]); //A4
  Mt_645_2007_V_cmd[Dlt645_Read_SrcAddr + 5] = str2_to_BCD(&addr[0]); //A5
	  
	Mt_645_2007_V_cmd[Dlt645_Read_ID0] = 0x00;
  Mt_645_2007_V_cmd[Dlt645_Read_ID1] = 0x02;
  Mt_645_2007_V_cmd[Dlt645_Read_ID2] = 0x00;
  Mt_645_2007_V_cmd[Dlt645_Read_ID3] = 0x02;
	
	AddSun0x33(&Mt_645_2007_V_cmd[Dlt645_Read_ID0], Mt_645_2007_V_cmd[Dlt645_Read_L], 1);//+33处理  Cost=1=+33,其它=-33
  Mt_645_2007_V_cmd[Dlt645_Read_CS] = Meter_645_cs(&Mt_645_2007_V_cmd,(sizeof(Mt_645_2007_V_cmd)-2));	
	memcpy(&SentBuf[4], Mt_645_2007_V_cmd, sizeof(Mt_645_2007_V_cmd));//复制表地址								
	
	Led_Relay_Control(6, 0);
	write(fd, SentBuf, sizeof(Mt_645_2007_V_cmd) + 4);
	do{
		ioctl(fd, TIOCSERGETLSR, &status);
	} while (status!=TIOCSER_TEMT);
	usleep(5000);
	Led_Relay_Control(6, 1);

	Count = read_datas_tty(fd, recvBuf, MAX_MODBUS_FRAMESIZE, 500000, 30000);
  if(Count >= (sizeof(Mt_645_2007_V_cmd)+4)){//响应可能没有0xfe引导码  +4个字节数据
#if METER_645_DEBUG == 1
			printf("kwh_read recv server length = %d,data= ", Count);
			for(j=0; j<Count; j++)printf("%02x ",recvBuf[j]);
			printf("\n");
#endif
  	for(i=0;i<5;i++){
  		if(recvBuf[i] == 0x68){
  			start_flag = i;
  			break;
  		}
  	}

  	if(i > 4){
  		return (-1);
  	}

  	if((Count-start_flag) != (sizeof(Mt_645_2007_V_cmd)+4)){
  		return (-1);
  	}
		
		if((Meter_645_cs(&recvBuf[start_flag], Count-start_flag-2)) == recvBuf[Count-2]){
			if((recvBuf[start_flag] == 0x68)&&(recvBuf[Count-1] == 0x16)){//起始符，结束符正确
				AddSun0x33(&recvBuf[start_flag + Dlt645_Read_ID0], 8, 2);//减33h       Cost=1=+33,其它=-33														
				if((recvBuf[start_flag+Dlt645_Read_ID0] == 0x00)&&(recvBuf[start_flag+Dlt645_Read_ID1] == 0x02)&&(recvBuf[start_flag+Dlt645_Read_ID2] == 0x00)&&(recvBuf[start_flag+Dlt645_Read_ID3] == 0x02)){//标识符正确

	       	*Output_voltage = (bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 4]&0x7F)*1000000 + \
					                 bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 3])*10000 + \
					                 bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 2])*100 + \
					                 bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 1]));
													 
//			printf("meter_now_Valley_KWH xxxxxxxxxxxxxxxxxxxxxx= %d \n", *meter_now_Valley_KWH);

					return (0);//抄收数据正确
				}
			}
		}
	}

	return (-1);
}

//读电表电流 
static int curt_read(UINT32 *Output_current, int fd, unsigned char *addr)
{
	int i=0,j=0,start_flag=0;
	int Count,status;

	unsigned char SentBuf[MAX_MODBUS_FRAMESIZE];
	unsigned char recvBuf[MAX_MODBUS_FRAMESIZE];
 
	SentBuf[0] = 0xFE;
	SentBuf[1] = 0xFE;
	SentBuf[2] = 0xFE;
	SentBuf[3] = 0xFE;
  Mt_645_2007_C_cmd[Dlt645_Read_SrcAddr + 0] = str2_to_BCD(&addr[10]); //A0
  Mt_645_2007_C_cmd[Dlt645_Read_SrcAddr + 1] = str2_to_BCD(&addr[8]); //A1
  Mt_645_2007_C_cmd[Dlt645_Read_SrcAddr + 2] = str2_to_BCD(&addr[6]); //A2
  Mt_645_2007_C_cmd[Dlt645_Read_SrcAddr + 3] = str2_to_BCD(&addr[4]); //A3
  Mt_645_2007_C_cmd[Dlt645_Read_SrcAddr + 4] = str2_to_BCD(&addr[2]); //A4
  Mt_645_2007_C_cmd[Dlt645_Read_SrcAddr + 5] = str2_to_BCD(&addr[0]); //A5
	
	Mt_645_2007_C_cmd[Dlt645_Read_ID0] = 0x00;
  Mt_645_2007_C_cmd[Dlt645_Read_ID1] = 0x01;
  Mt_645_2007_C_cmd[Dlt645_Read_ID2] = 0x00;
  Mt_645_2007_C_cmd[Dlt645_Read_ID3] = 0x02;
	
	AddSun0x33(&Mt_645_2007_C_cmd[Dlt645_Read_ID0], Mt_645_2007_C_cmd[Dlt645_Read_L], 1);//+33处理  Cost=1=+33,其它=-33
  Mt_645_2007_C_cmd[Dlt645_Read_CS] = Meter_645_cs(&Mt_645_2007_C_cmd,(sizeof(Mt_645_2007_C_cmd)-2));	
	memcpy(&SentBuf[4], Mt_645_2007_C_cmd, sizeof(Mt_645_2007_C_cmd));//复制表地址								
	
	Led_Relay_Control(6, 0);
	write(fd, SentBuf, sizeof(Mt_645_2007_C_cmd) + 4);
	do{
		ioctl(fd, TIOCSERGETLSR, &status);
	} while (status!=TIOCSER_TEMT);
	usleep(5000);
	Led_Relay_Control(6, 1);

	Count = read_datas_tty(fd, recvBuf, MAX_MODBUS_FRAMESIZE, 500000, 30000);
  if(Count >= (sizeof(Mt_645_2007_C_cmd)+4)){//响应可能没有0xfe引导码  +4个字节数据
#if METER_645_DEBUG == 1
			printf("kwh_read recv server length = %d,data= ", Count);
			for(j=0; j<Count; j++)printf("%02x ",recvBuf[j]);
			printf("\n");
#endif
  	for(i=0;i<5;i++){
  		if(recvBuf[i] == 0x68){
  			start_flag = i;
  			break;
  		}
  	}

  	if(i > 4){
  		return (-1);
  	}

  	if((Count-start_flag) != (sizeof(Mt_645_2007_C_cmd)+4)){
  		return (-1);
  	}
		
		if((Meter_645_cs(&recvBuf[start_flag], Count-start_flag-2)) == recvBuf[Count-2]){
			if((recvBuf[start_flag] == 0x68)&&(recvBuf[Count-1] == 0x16)){//起始符，结束符正确
				AddSun0x33(&recvBuf[start_flag + Dlt645_Read_ID0], 8, 2);//减33h       Cost=1=+33,其它=-33														
				if((recvBuf[start_flag+Dlt645_Read_ID0] == 0x00)&&(recvBuf[start_flag+Dlt645_Read_ID1] == 0x01)&&(recvBuf[start_flag+Dlt645_Read_ID2] == 0x00)&&(recvBuf[start_flag+Dlt645_Read_ID3] == 0x02)){//标识符正确
	       	*Output_current = (bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 4]&0x7F)*1000000 + \
					                 bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 3])*10000 + \
					                 bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 2])*100 + \
					                 bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 1]))/10;
													 
			  //printf("MEter Output_current xxxxxxxxxxxxxxxxxxxxxx= %d \n", *Output_current);

					return (0);//抄收数据正确
				}
			}
		}
	}
	return (-1);
}

//下发充直流分流器参数Dc_shunt_Set_meter_flag ---待定
static int Dc_shunt_Set_meter(UINT32 DC_Shunt_Range, int fd, unsigned char *addr, int gun)
{
	int i=0,start_flag=0;
	int Count,status;

	unsigned char SentBuf[MAX_MODBUS_FRAMESIZE];
	unsigned char recvBuf[MAX_MODBUS_FRAMESIZE];

	SentBuf[0] = 0xFE;
	SentBuf[1] = 0xFE;
	SentBuf[2] = 0xFE;
	SentBuf[3] = 0xFE;

	SentBuf[4] = 0x68;
	SentBuf[5] = str2_to_BCD(&addr[10]); //A0
	SentBuf[6] = str2_to_BCD(&addr[8]); //A1
	SentBuf[7] = str2_to_BCD(&addr[6]); //A2
	SentBuf[8] = str2_to_BCD(&addr[4]); //A3
	SentBuf[9] = str2_to_BCD(&addr[2]); //A4
	SentBuf[10] = str2_to_BCD(&addr[0]);//A5
	SentBuf[11] = 0x68;
	SentBuf[12] = 0x04; //读数据
	SentBuf[13] = 0x0A; //数据域长度
	SentBuf[14] = 0x44; //A1+0x33 B6A1
	SentBuf[15] = 0x44; //B6+0x33
	SentBuf[16] = 0x33; 
	SentBuf[17] = 0x33; 
	SentBuf[18] = 0x33;
	SentBuf[19] = 0x33; 
 	SentBuf[20] = 0x71; 
 	
	SentBuf[21] = int_to_bcd_1byte((DC_Shunt_Range*150)%100)+0x33; 
	SentBuf[22] = int_to_bcd_1byte((DC_Shunt_Range*150)%10000/100)+0x33; 
	SentBuf[23] = int_to_bcd_1byte((DC_Shunt_Range*150)/10000)+0x33; 


	SentBuf[24] = Meter_645_cs(&SentBuf[4],20);	
	SentBuf[25] = 0x16;	
	Led_Relay_Control(6, 0);
	write(fd, SentBuf, 26);
	do{
		ioctl(fd, TIOCSERGETLSR, &status);
	} while (status!=TIOCSER_TEMT);
	usleep(5000);
	Led_Relay_Control(6, 1);

	Count = read_datas_tty(fd, recvBuf, MAX_MODBUS_FRAMESIZE, 500000, 30000);
  if(Count >= 16){//响应可能没有0xfe引导码
  	for(i=0;i<5;i++){
  		if(recvBuf[i] == 0x68){
  			start_flag = i;
  			break;
  		}
  	}

  	if(i > 4){
  		return (-1);
  	}

  	if((Count-start_flag) != 16){
  		return (-1);
  	}

		if((Meter_645_cs(&recvBuf[start_flag], Count-start_flag-2)) == recvBuf[Count-2]){
			if((recvBuf[start_flag] == 0x68)&&(recvBuf[Count-1] == 0x16)){//起始符，结束符正确
	      if(gun == 1){
					Dc_shunt_Set_meter_flag_1 = 0;
				}else{
					Dc_shunt_Set_meter_flag_2 = 0;
				}
			}
		}
	}

	return (-1);
}


/*********************************************************
**description：电表读写线程
***parameter  :none
***return		  :none
**********************************************************/
static void Meter_Read_Write_Task(void)
{
	int fd, Ret = 0;

  UINT32 meter_connect1 = 0;
  UINT32 meter_connect_count1 = 0;

  UINT32 meter_connect2 = 0;
  UINT32 meter_connect_count2 = 0;

	UINT32 Output_voltage = 0;				//充电机输出电压 0.001V
	UINT32 Output_current = 0;				//充电机输出电流 0.001A
	UINT32 meter_now_KWH = 0;					//充电机输出电流 0.01kwh
	UINT32 meter_Current_A = 0,meter_Current_V = 0;       //电表读取电流
	fd = OpenDev(METER_COM_ID);
	if(fd == -1) {
		while(1);
	}else{
		set_speed(fd, 9600);
		set_Parity(fd, 8, 1, 2);//偶校验
		close(fd);
  }
	fd = OpenDev(METER_COM_ID);
	if(fd == -1) {
		while(1);
	}
	prctl(PR_SET_NAME,(unsigned long)"DC_meter2007_Task");//设置线程名字 

	while(1){
		usleep(100000);//100ms    500K 7-8秒一度电
		Meter_Read_Write_Watchdog = 1;

		meter_connect1 = 0;
		meter_connect2 = 0;
    if(Globa_1->Charger_param.System_Type != 3){//不是壁挂的
		//--------------------------- 1号电表 ------------------------------------
			if(Globa_1->Charger_param.meter_config_flag == 2)	
			{
  		  Ret = All_kwh_read_New(&meter_now_KWH, fd, &Globa_1->Charger_param.Power_meter_addr1[0]);//读电量，3位小数
			}else{
			  Ret = All_kwh_read(&meter_now_KWH, fd, &Globa_1->Charger_param.Power_meter_addr1[0]);//读电量	
			}
  		if(Ret == -1){
  		}else if(Ret == 0){
      	meter_connect1 = 1;
				Globa_1->meter_KWH = meter_now_KWH;
	    	if(Globa_1->Charger_param.System_Type == 1){//单枪或者轮流充电
				  Globa_2->meter_KWH = meter_now_KWH;
				}
  		}
			usleep(100000);//100ms
			Ret = curt_read(&meter_Current_A, fd, &Globa_1->Charger_param.Power_meter_addr1[0]);//读电流
			if(Ret == -1){
  		}else if(Ret == 0){
      	meter_connect1 = 1;
				Globa_1->meter_Current_A = meter_Current_A;
	    	if(Globa_1->Charger_param.System_Type == 1){//单枪或者轮流充电
				  Globa_2->meter_Current_A = meter_Current_A;
				}
  		}
		/*	usleep(100000);//100ms
			Ret = volt_read(&meter_Current_V, fd, &Globa_1->Charger_param.Power_meter_addr1[0]);//读电流
			if(Ret == -1){
			}else if(Ret == 0){
				meter_connect1 = 1;
				Globa_1->meter_Current_V = meter_Current_V;
				if(Globa_1->Charger_param.System_Type == 1){//双枪同时充电的时候才需要
			    Globa_2->meter_Current_V = meter_Current_V;
				}
			}*/
			if(Globa_1->Charger_param.meter_config_flag == 1)	
			{
				if(Dc_shunt_Set_meter_flag_1 == 1){ //下发电表分流器量程
					Dc_shunt_Set_meter(Globa_1->Charger_param.DC_Shunt_Range, fd, &Globa_1->Charger_param.Power_meter_addr1[0], 1);
					usleep(200000);//200ms
					if((Globa_1->Charger_param.System_Type == 0)||(Globa_1->Charger_param.System_Type == 4)){//同时充电
						Dc_shunt_Set_meter(Globa_2->Charger_param.DC_Shunt_Range, fd, &Globa_2->Charger_param.Power_meter_addr2[0], 2);
						usleep(200000);//200ms
					}
				}
			}else{
				Dc_shunt_Set_meter_flag_1 = 0;
			}

  		if(meter_connect1 == 0){
  			meter_connect_count1++;
  			if(meter_connect_count1 >= 10){
  				meter_connect_count1 = 0;
  				if(Globa_1->Error.meter_connect_lost == 0){
  					Globa_1->Error.meter_connect_lost = 1;
						
						if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4)){
							sent_warning_message(0x99, 54, 1, 0);
						}else{
							sent_warning_message(0x99, 54, 0, 0);
						}
  					Globa_1->Error_system++;
						if(Globa_1->Charger_param.System_Type == 1){//单枪或者轮流充电
							Globa_2->Error_system++;
						}
  				}
  			}
  		}else{
  			meter_connect_count1 = 0;
  			if(Globa_1->Error.meter_connect_lost == 1){
  				Globa_1->Error.meter_connect_lost = 0;
					if((Globa_1->Charger_param.System_Type == 0)||(Globa_1->Charger_param.System_Type == 4)){
						sent_warning_message(0x98, 54, 1, 0);
					}else{
						sent_warning_message(0x98, 54, 0, 0);
					}
  				Globa_1->Error_system--;
					if(Globa_1->Charger_param.System_Type == 1){//单枪或者轮流充电
						Globa_2->Error_system--;
					}
  			}
  		}
		  
			if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候{//双枪同时充电的时候才需要
				//--------------------------- 2号电表 ------------------------------------
				if(Globa_1->Charger_param.meter_config_flag == 2)	
				{
					Ret = All_kwh_read_New(&meter_now_KWH, fd, &Globa_2->Charger_param.Power_meter_addr2[0]);//读电量，3位小数
				}else{
					Ret = All_kwh_read(&meter_now_KWH, fd, &Globa_2->Charger_param.Power_meter_addr2[0]);//读电量	
				}
				
				//Ret = All_kwh_read(&meter_now_KWH, fd, &Globa_2->Charger_param.Power_meter_addr2[0]);//读电量
				if(Ret == -1){
				}else if(Ret == 0){
					meter_connect2 = 1;
					Globa_2->meter_KWH = meter_now_KWH;
				}

				usleep(100000);//100ms
			
  			Ret = curt_read(&meter_Current_A, fd, &Globa_2->Charger_param.Power_meter_addr2[0]);//读电流
				if(Ret == -1){
				}else if(Ret == 0){
					meter_connect2 = 1;
					Globa_2->meter_Current_A = meter_Current_A;
				}
			/*	usleep(100000);//100ms
				Ret = volt_read(&meter_Current_V, fd, &Globa_2->Charger_param.Power_meter_addr2[0]);//读电流
				if(Ret == -1){
				}else if(Ret == 0){
					meter_connect2 = 1;
					Globa_2->meter_Current_V = meter_Current_V;
				}*/
		    if(Globa_1->Charger_param.meter_config_flag == 1)	
				{
					
					if(Dc_shunt_Set_meter_flag_2 == 1){ //下发电表分流器量程
						Dc_shunt_Set_meter(Globa_1->Charger_param.DC_Shunt_Range, fd, &Globa_1->Charger_param.Power_meter_addr1[0], 1);
						usleep(200000);//200ms
						Dc_shunt_Set_meter(Globa_2->Charger_param.DC_Shunt_Range, fd, &Globa_2->Charger_param.Power_meter_addr2[0], 2);
						usleep(200000);//200ms
					}
			  }else{
					Dc_shunt_Set_meter_flag_2 = 0;
				}

				if(meter_connect2 == 0){
					meter_connect_count2++;
					if(meter_connect_count2 >= 10){
						meter_connect_count2 = 0;
						if(Globa_2->Error.meter_connect_lost == 0){
							Globa_2->Error.meter_connect_lost = 1;
							sent_warning_message(0x99, 54, 2, 0);
							Globa_2->Error_system++;
						}
					} 
				}else{
					meter_connect_count2 = 0;
					if(Globa_2->Error.meter_connect_lost == 1){
						Globa_2->Error.meter_connect_lost = 0;
						sent_warning_message(0x98, 54, 2, 0);
						Globa_2->Error_system--;
					}
				}
	    }
		}else{
			sleep(5);
		}
  }
}

/*********************************************************
**description：电表读写线程初始化及建立
***parameter  :none
***return		  :none
**********************************************************/
extern void DC_2007Meter_Read_Write_Thread(void)
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
	
	/* 创建自动串口抄收线程 */
	if(0 != pthread_create(&td1, &attr, (void *)Meter_Read_Write_Task, NULL)){
		perror("####pthread_create IC_Read_Write_Task ERROR ####\n\n");
	}

	ret = pthread_attr_destroy(&attr);
	if(ret != 0)
		return;
}
