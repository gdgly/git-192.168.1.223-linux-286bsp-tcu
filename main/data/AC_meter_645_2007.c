/*******************************************************************************
  Copyright(C)    2014-2015,      EAST Co.Ltd.
  File name :     AC_meter_645_2007.c
  Author :        dengjh
  Version:        1.0
  Date:           2016.8.30
  Description:    
  Other:          
  Function List:  
  History:        
          <author>   <time>   <version>   <desc>
           dengjh    2016.8.30   1.0      Create
		   交流电能表 DL/T645-2007 通信协议
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
#include <sys/prctl.h>

#include "globalvar.h"

#define METER_645_DEBUG  0        //ISO8583_DEBUG 调试使能

//extern UINT32 Meter_Read_Write_Watchdog;


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


// 645-2007                                            0    1    2    3    4    5    6   7    8    9    10   11   12   13    14    15
unsigned char ACMt_645_2007_All_Electricity_cmd[16]={0x68,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0x68,0x11,0x04,0x00,0x00,0x01,0x00,0x00,0x16};//当前总电量  0.01KWH 4字节BCD



/*********************************************************
函数原形：void AddSub0x33(char * P, char len, char Cost)
功    能：+33h或者-33h //cost=1=+33,其它=-33
Cost=1=+33,其它=-33
**********************************************************/
static void AddSub0x33(unsigned char * P, unsigned char len, unsigned char Cost)
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


//读总电量
static int All_kwh_read(UINT32 *meter_now_All_KWH, int fd, unsigned char *addr)
{
	int i=0,j=0,start_flag=0;
    int Count,status;
	int rd_data_len;//电表响应报文中的长度域的值
	int expected_data_bytes = 4;//期望读取的实际数据长度(如3相电压为6,总正向有功电量为4，3相电流为9)
    unsigned char SentBuf[255];
    unsigned char recvBuf[255];
	unsigned char READ_DI3 = 0x00;
	unsigned char READ_DI2 = 0x01;
	unsigned char READ_DI1 = 0x00;
	unsigned char READ_DI0 = 0x00;
    SentBuf[0] = 0xFE;
    SentBuf[1] = 0xFE;
    SentBuf[2] = 0xFE;
    SentBuf[3] = 0xFE;
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 0] = str2_to_BCD(&addr[10]); //A0
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 1] = str2_to_BCD(&addr[8]); //A1
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 2] = str2_to_BCD(&addr[6]); //A2
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 3] = str2_to_BCD(&addr[4]); //A3
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 4] = str2_to_BCD(&addr[2]); //A4
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 5] = str2_to_BCD(&addr[0]); //A5
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_ID0] = READ_DI0;
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_ID1] = READ_DI1;
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_ID2] = READ_DI2;
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_ID3] = READ_DI3;
    AddSub0x33(&ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_ID0], ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_L], 1);//+33处理  Cost=1=+33,其它=-33
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_CS] = Meter_645_cs((unsigned char *)&ACMt_645_2007_All_Electricity_cmd,(sizeof(ACMt_645_2007_All_Electricity_cmd)-2));
    memcpy(&SentBuf[4], ACMt_645_2007_All_Electricity_cmd, sizeof(ACMt_645_2007_All_Electricity_cmd));//复制表地址
	
	read_datas_tty(fd, recvBuf, 255, 1000000, 300000);//读空!
	Led_Relay_Control(8, 0);
	write(fd, SentBuf, sizeof(ACMt_645_2007_All_Electricity_cmd) + 4);
	do{
		ioctl(fd, TIOCSERGETLSR, &status);
	} while (status!=TIOCSER_TEMT);
	usleep(4000);
	Led_Relay_Control(8, 1);
	//Globa->ac_meter_tx_cnt++;
	Count = read_datas_tty(fd, recvBuf, 255, 1000000, 300000);//增加字节间超时，避免读取数据不全以及后续数据错位!
	
	#if METER_645_DEBUG
	printf("AC_meter_645_2007 All_kwh_read return %d bytes:\n",Count);
	for(i=0;i<Count;i++)
		printf("%02x ",recvBuf[i]);
	printf("\n\n");
	#endif
	
   if(Count >= (sizeof(ACMt_645_2007_All_Electricity_cmd)+expected_data_bytes))
	{//响应可能没有0xfe引导码  +4个字节数据

        for(i=0;i<5;i++){
            if(recvBuf[i] == 0x68){
                start_flag = i;
                break;
            }
        }

        if(i > 4){
            return (-1);
        }

        if((Count-start_flag) != (sizeof(ACMt_645_2007_All_Electricity_cmd)+expected_data_bytes)){
            return (-1);
        }

        if((Meter_645_cs(&recvBuf[start_flag], Count-start_flag-2)) == recvBuf[Count-2]){
            if((recvBuf[start_flag] == 0x68)&&(recvBuf[Count-1] == 0x16)){//起始符，结束符正确
				rd_data_len = recvBuf[start_flag + Dlt645_Read_L];
				if(rd_data_len ==  (4+expected_data_bytes) ){//长度正确				
					AddSub0x33(&recvBuf[start_flag + Dlt645_Read_ID0], rd_data_len, 2);//减33h       Cost=1=+33,其它=-33
					if((recvBuf[start_flag+Dlt645_Read_ID0] == READ_DI0)&&(recvBuf[start_flag+Dlt645_Read_ID1] == READ_DI1)&&(recvBuf[start_flag+Dlt645_Read_ID2] == READ_DI2)&&(recvBuf[start_flag+Dlt645_Read_ID3] == READ_DI3))//标识符正确
					{
						*meter_now_All_KWH = (bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 4])*500000 + \
								bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 3])*10000 + \
								bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 2])*100 + \
								bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 1]));
						//Globa->ac_meter_rx_ok_cnt++;
						return (0);//抄收数据正确
					}
				}
            }
        }
    }

	return (-1);
}


//读3相电压
static int All_volt_read(unsigned int meter_volt[], int fd, unsigned char *addr)
{
    int i=0,j=0,start_flag=0;
    int Count,status;
	int rd_data_len;//电表响应报文中的长度域的值
	int expected_data_bytes = 6;//期望读取的实际数据长度(如3相电压为6,总正向有功电量为4，3相电流为9)
    unsigned char SentBuf[255];
    unsigned char recvBuf[255];
	unsigned char READ_DI3 = 0x02;
	unsigned char READ_DI2 = 0x01;
	unsigned char READ_DI1 = 0xFF;
	unsigned char READ_DI0 = 0x00;
    SentBuf[0] = 0xFE;
    SentBuf[1] = 0xFE;
    SentBuf[2] = 0xFE;
    SentBuf[3] = 0xFE;
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 0] = str2_to_BCD(&addr[10]); //A0
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 1] = str2_to_BCD(&addr[8]); //A1
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 2] = str2_to_BCD(&addr[6]); //A2
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 3] = str2_to_BCD(&addr[4]); //A3
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 4] = str2_to_BCD(&addr[2]); //A4
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 5] = str2_to_BCD(&addr[0]); //A5
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_ID0] = READ_DI0;
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_ID1] = READ_DI1;
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_ID2] = READ_DI2;
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_ID3] = READ_DI3;
    AddSub0x33(&ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_ID0], ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_L], 1);//+33处理  Cost=1=+33,其它=-33
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_CS] = Meter_645_cs((unsigned char *)&ACMt_645_2007_All_Electricity_cmd,(sizeof(ACMt_645_2007_All_Electricity_cmd)-2));
    memcpy(&SentBuf[4], ACMt_645_2007_All_Electricity_cmd, sizeof(ACMt_645_2007_All_Electricity_cmd));//复制表地址

	read_datas_tty(fd, recvBuf, 255, 1000000, 300000);//读空!
    Led_Relay_Control(8, 0);
	write(fd, SentBuf, sizeof(ACMt_645_2007_All_Electricity_cmd) + 4);
	do{
		ioctl(fd, TIOCSERGETLSR, &status);
	} while (status!=TIOCSER_TEMT);
	usleep(10000);
	Led_Relay_Control(8, 1);
	//Globa->ac_meter_tx_cnt++;
	Count = read_datas_tty(fd, recvBuf, 255, 1000000, 300000);	
	
	#if METER_645_DEBUG
	printf("AC_meter_645_2007 All_volt_read return %d bytes:\n",Count);
	for(i=0;i<Count;i++)
		printf("%02x ",recvBuf[i]);
	printf("\n\n");
	#endif
	
    if(Count >= (sizeof(ACMt_645_2007_All_Electricity_cmd)+expected_data_bytes))
	{//响应可能没有0xfe引导码  + 6个字节3相电压数据！！

        for(i=0;i<5;i++){
            if(recvBuf[i] == 0x68){
                start_flag = i;
                break;
            }
        }

        if(i > 4){
            return (-1);
        }

        if((Count-start_flag) != (sizeof(ACMt_645_2007_All_Electricity_cmd)+expected_data_bytes)){
            return (-1);
        }

        if((Meter_645_cs(&recvBuf[start_flag], Count-start_flag-2)) == recvBuf[Count-2]){
            if((recvBuf[start_flag] == 0x68)&&(recvBuf[Count-1] == 0x16)){//起始符，结束符正确
				rd_data_len = recvBuf[start_flag + Dlt645_Read_L];
				if(rd_data_len ==  (4+expected_data_bytes) )//长度正确,读3相电压返回的数据长度是4字节读取ID+6字节电压数据
				{
					AddSub0x33(&recvBuf[start_flag + Dlt645_Read_ID0], rd_data_len, 2);//减33h       Cost=1=+33,其它=-33
					if((recvBuf[start_flag+Dlt645_Read_ID0] == READ_DI0)&&(recvBuf[start_flag+Dlt645_Read_ID1] == READ_DI1)&&(recvBuf[start_flag+Dlt645_Read_ID2] == READ_DI2)&&(recvBuf[start_flag+Dlt645_Read_ID3] == READ_DI3)){//标识符正确

						meter_volt[0] = (bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 2])*100 + bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 1]));

						meter_volt[1] = (bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 4])*100 + bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 3]));	

						meter_volt[2] = (bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 6])*100 + bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 5]));				
						//Globa->ac_meter_rx_ok_cnt++;
						return (0);//抄收数据正确
					}
				}
            }
        }
    }

    return (-1);
}
//读3相电流
static int All_current_read(unsigned int meter_current[], int fd, unsigned char *addr)
{
    int i=0,j=0,start_flag=0;
    int Count,status;
	int rd_data_len;//电表响应报文中的长度域的值
	int expected_data_bytes = 9;//期望读取的实际数据长度(如3相电压为6,总正向有功电量为4，3相电流为9)
    unsigned char SentBuf[255];
    unsigned char recvBuf[255];
	unsigned char READ_DI3 = 0x02;
	unsigned char READ_DI2 = 0x02;
	unsigned char READ_DI1 = 0xFF;
	unsigned char READ_DI0 = 0x00;
    SentBuf[0] = 0xFE;
    SentBuf[1] = 0xFE;
    SentBuf[2] = 0xFE;
    SentBuf[3] = 0xFE;
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 0] = str2_to_BCD(&addr[10]); //A0
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 1] = str2_to_BCD(&addr[8]); //A1
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 2] = str2_to_BCD(&addr[6]); //A2
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 3] = str2_to_BCD(&addr[4]); //A3
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 4] = str2_to_BCD(&addr[2]); //A4
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 5] = str2_to_BCD(&addr[0]); //A5
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_ID0] = READ_DI0;
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_ID1] = READ_DI1;
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_ID2] = READ_DI2;
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_ID3] = READ_DI3;
    AddSub0x33(&ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_ID0], ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_L], 1);//+33处理  Cost=1=+33,其它=-33
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_CS] = Meter_645_cs((unsigned char *)&ACMt_645_2007_All_Electricity_cmd,(sizeof(ACMt_645_2007_All_Electricity_cmd)-2));
    memcpy(&SentBuf[4], ACMt_645_2007_All_Electricity_cmd, sizeof(ACMt_645_2007_All_Electricity_cmd));//复制表地址

	read_datas_tty(fd, recvBuf, 255, 1000000, 300000);//读空!
	Led_Relay_Control(8, 0);
	write(fd, SentBuf, sizeof(ACMt_645_2007_All_Electricity_cmd) + 4);
	do{
		ioctl(fd, TIOCSERGETLSR, &status);
	} while (status!=TIOCSER_TEMT);
	usleep(10000);
	Led_Relay_Control(8, 1);
	//Globa->ac_meter_tx_cnt++;
	Count = read_datas_tty(fd, recvBuf, 255, 1000000, 300000);
	#if METER_645_DEBUG
	printf("AC_meter_645_2007 All_current_read return %d bytes:\n",Count);
	for(i=0;i<Count;i++)
		printf("%02x ",recvBuf[i]);
	printf("\n\n");
	#endif
	
    if(Count >= (sizeof(ACMt_645_2007_All_Electricity_cmd)+expected_data_bytes))
	{//响应可能没有0xfe引导码  + 9个字节3相电流数据！！

        for(i=0;i<5;i++){
            if(recvBuf[i] == 0x68){
                start_flag = i;
                break;
            }
        }

        if(i > 4){
            return (-1);
        }

        if((Count-start_flag) != (sizeof(ACMt_645_2007_All_Electricity_cmd)+expected_data_bytes)){
            return (-1);
        }

        if((Meter_645_cs(&recvBuf[start_flag], Count-start_flag-2)) == recvBuf[Count-2]){
            if((recvBuf[start_flag] == 0x68)&&(recvBuf[Count-1] == 0x16)){//起始符，结束符正确
				rd_data_len = recvBuf[start_flag + Dlt645_Read_L];
				if(rd_data_len ==  (4+expected_data_bytes) )//长度正确,读3相电流返回的数据长度是4字节读取ID+9字节电流数据
				{
					AddSub0x33(&recvBuf[start_flag + Dlt645_Read_ID0], rd_data_len, 2);//减33h       Cost=1=+33,其它=-33
					if((recvBuf[start_flag+Dlt645_Read_ID0] == READ_DI0)&&(recvBuf[start_flag+Dlt645_Read_ID1] == READ_DI1)&&(recvBuf[start_flag+Dlt645_Read_ID2] == READ_DI2)&&(recvBuf[start_flag+Dlt645_Read_ID3] == READ_DI3)){//标识符正确

						meter_current[0] = (bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 3])*10000 + bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 2])*100 + bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 1]));

						meter_current[1] = (bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 6])*10000 + bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 5])*100 + bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 4]));	

						meter_current[2] = (bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 9])*10000 + bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 8])*100 + bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 7]));				
						//Globa->ac_meter_rx_ok_cnt++;
						return (0);//抄收数据正确
					}
				}
            }
        }
    }

    return (-1);
}


//读取总有功功率
static int Active_Power_read(unsigned int meter_active_power[], int fd, unsigned char *addr)
{
    int i=0,j=0,start_flag=0;
    int Count,status;
	int rd_data_len;//电表响应报文中的长度域的值
	int expected_data_bytes = 3;//期望读取的实际数据长度(如3相电压为6,总正向有功电量为4，3相电流为9)
    unsigned char SentBuf[255];
    unsigned char recvBuf[255];
	unsigned char READ_DI3 = 0x02;
	unsigned char READ_DI2 = 0x03;
	unsigned char READ_DI1 = 0x00;
	unsigned char READ_DI0 = 0x00;
    SentBuf[0] = 0xFE;
    SentBuf[1] = 0xFE;
    SentBuf[2] = 0xFE;
    SentBuf[3] = 0xFE;
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 0] = str2_to_BCD(&addr[10]); //A0
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 1] = str2_to_BCD(&addr[8]); //A1
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 2] = str2_to_BCD(&addr[6]); //A2
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 3] = str2_to_BCD(&addr[4]); //A3
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 4] = str2_to_BCD(&addr[2]); //A4
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_SrcAddr + 5] = str2_to_BCD(&addr[0]); //A5
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_ID0] = READ_DI0;
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_ID1] = READ_DI1;
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_ID2] = READ_DI2;
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_ID3] = READ_DI3;
    AddSub0x33(&ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_ID0], ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_L], 1);//+33处理  Cost=1=+33,其它=-33
    ACMt_645_2007_All_Electricity_cmd[Dlt645_Read_CS] = Meter_645_cs((unsigned char *)&ACMt_645_2007_All_Electricity_cmd,(sizeof(ACMt_645_2007_All_Electricity_cmd)-2));
    memcpy(&SentBuf[4], ACMt_645_2007_All_Electricity_cmd, sizeof(ACMt_645_2007_All_Electricity_cmd));//复制表地址

	read_datas_tty(fd, recvBuf, 255, 1000000, 300000);//读空!
    Led_Relay_Control(8, 0);
	write(fd, SentBuf, sizeof(ACMt_645_2007_All_Electricity_cmd) + 4);
	do{
		ioctl(fd, TIOCSERGETLSR, &status);
	} while (status!=TIOCSER_TEMT);
	usleep(10000);
	Led_Relay_Control(8, 1);
	//Globa->ac_meter_tx_cnt++;
	Count = read_datas_tty(fd, recvBuf, 255, 1000000, 300000);
	
	#if METER_645_DEBUG
	printf("AC_meter_645_2007 Active_Power_read return %d bytes:\n",Count);
	for(i=0;i<Count;i++)
		printf("%02x ",recvBuf[i]);
	printf("\n\n");
	#endif
	
    if(Count >= (sizeof(ACMt_645_2007_All_Electricity_cmd)+expected_data_bytes))
	{//响应可能没有0xfe引导码  + 9个字节3相电流数据！！

        for(i=0;i<5;i++){
            if(recvBuf[i] == 0x68){
                start_flag = i;
                break;
            }
        }

        if(i > 4){
            return (-1);
        }

        if((Count-start_flag) != (sizeof(ACMt_645_2007_All_Electricity_cmd)+expected_data_bytes)){
            return (-1);
        }

        if((Meter_645_cs(&recvBuf[start_flag], Count-start_flag-2)) == recvBuf[Count-2]){
            if((recvBuf[start_flag] == 0x68)&&(recvBuf[Count-1] == 0x16)){//起始符，结束符正确
				rd_data_len = recvBuf[start_flag + Dlt645_Read_L];
				if(rd_data_len ==  (4+expected_data_bytes) )//长度正确,读3相电流返回的数据长度是4字节读取ID+9字节电流数据
				{
					AddSub0x33(&recvBuf[start_flag + Dlt645_Read_ID0], rd_data_len, 2);//减33h       Cost=1=+33,其它=-33
					if((recvBuf[start_flag+Dlt645_Read_ID0] == READ_DI0)&&(recvBuf[start_flag+Dlt645_Read_ID1] == READ_DI1)&&(recvBuf[start_flag+Dlt645_Read_ID2] == READ_DI2)&&(recvBuf[start_flag+Dlt645_Read_ID3] == READ_DI3)){//标识符正确

						meter_active_power[0] = (bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 3])*10000 + bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 2])*100 + bcd_to_int_1byte(recvBuf[start_flag + Dlt645_Read_ID3 + 1]));
					//	Globa->ac_meter_rx_ok_cnt++;
						return (0);//抄收数据正确
					}
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
	unsigned char AC_meter_addr[12];
	unsigned int meter_volt[3]={0,0,0};//3相电压 0.1V	
	unsigned int meter_current[3]={0,0,0};//3相电流 0.001A
	unsigned int meter_active_power[3]={0,0,0};//总有功功率//0.0001 kw	
	unsigned int meter_now_KWH = 0;		//交流有功电能 0.01kwh
	memcpy(AC_meter_addr, "AAAAAAAAAAAA", 12);
	
	fd = OpenDev(AC_METER_COM_ID);
	if(fd == -1) 
	{
		while(1);
	}else
	{
		//set_speed(fd, 9600);//test
		set_speed(fd, 2400);
		set_Parity(fd, 8, 1, 2);//偶校验
		close(fd);
	}
	fd = OpenDev(AC_METER_COM_ID);
	if(fd == -1) 
	{
		while(1);
	}
	
	prctl(PR_SET_NAME,(unsigned long)"AC_METER_Task");//设置线程名字 
	meter_connect1 = 0;
	while(1)
	{
		usleep(200000);//100ms    500K 7-8秒一度电
	//	Meter_Read_Write_Watchdog = 1;
		meter_connect1 = 0;
		//读正向总有功电量
		Ret = All_kwh_read(&meter_now_KWH, fd, AC_meter_addr);
		if(0 == Ret)
		{
			meter_connect1 = 1;
			Globa_1->ac_meter_kwh = meter_now_KWH*Globa_1->Charger_param.AC_Meter_CT;//交流电表度数 0.01kwh	
			printf("Globa_1->ac_meter_kwh = %d.%d kwh\n",Globa_1->ac_meter_kwh/100,Globa_1->ac_meter_kwh%100);			
  		}	
		
		usleep(100000);
		//读电压
		Ret = All_volt_read(meter_volt, fd, AC_meter_addr);
		if(0 == Ret)
		{
			meter_connect1 = 1;
			Globa_1->ac_volt[0] = meter_volt[0];//3相输入电压0.1V
			Globa_1->ac_volt[1] = meter_volt[1];//3相输入电压0.1V		
			Globa_1->ac_volt[2] = meter_volt[2];//3相输入电压0.1V					
    }
		
		usleep(100000);
		//读电流
		Ret = All_current_read(meter_current, fd, AC_meter_addr);
		if(0 == Ret)
		{
			meter_connect1 = 1;
			Globa_1->ac_current[0] = meter_current[0]*Globa_1->Charger_param.AC_Meter_CT/10;//3相输入电流0.01AV
			Globa_1->ac_current[1] = meter_current[1]*Globa_1->Charger_param.AC_Meter_CT/10;//3相输入电流0.01A	
			Globa_1->ac_current[2] = meter_current[2]*Globa_1->Charger_param.AC_Meter_CT/10;//3相输入电流0.01A				
    }
		
		if(meter_connect1 == 0){
  		meter_connect_count1++;
  	  if(meter_connect_count1 >= 10){
  			meter_connect_count1 = 0;
  			if(Globa_1->Error.ac_meter_connect_lost == 0){
  				Globa_1->Error.ac_meter_connect_lost = 1;
					sent_warning_message(0x99, 57, 0, 0);
  			}
			}
  	}else{
  		meter_connect_count1 = 0;
  		if(Globa_1->Error.ac_meter_connect_lost == 1){
  			Globa_1->Error.ac_meter_connect_lost = 0;
				sent_warning_message(0x98, 57, 0, 0);
  		}
  	}
	}
}

/*********************************************************
**description：电表读写线程初始化及建立
***parameter  :none
***return		  :none
**********************************************************/
extern void AC_2007Meter_Read_Write_Thread(void)
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
