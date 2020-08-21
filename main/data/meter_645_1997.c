/*******************************************************************************
  Copyright(C)    2014-2015,      EAST Co.Ltd.
  File name :     meter_645_1997.c
  Author :        dengjh
  Version:        1.0
  Date:           2015.4.11
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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <asm/ioctls.h>
#include <sys/prctl.h>

#include "globalvar.h"

#define METER_645_DEBUG 0        //ISO8583_DEBUG 调试使能

extern UINT32 Meter_Read_Write_Watchdog;
extern UINT32 Dc_shunt_Set_meter_flag[CONTROL_DC_NUMBER];

static UINT32 Dc_shunt_Set_meter_done_flag[CONTROL_DC_NUMBER];

//----645-1997读数据格式------------
typedef enum Dlt645Read_1997{
    Dlt645_Read_0x681,
    Dlt645_Read_SrcAddr,             //地址 发送低到高 A0 ... A5   BCD
    Dlt645_Read_0x682=7,
    Dlt645_Read_c,                   //控制码
    Dlt645_Read_L,                   //数据长度
    Dlt645_Read_ID0,                 //标识0
    Dlt645_Read_ID1,                 //标识1
    Dlt645_Read_CS,                  //校验
    Dlt645_Read_16                   //结束,位置=13，从0起
}DLT645READ_1997;


// 645-1997                                  0    1    2    3    4    5    6   7    8    9    10   11   12   13  
const unsigned char Mt_645_97_9010_cmd[14]={0x68,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0x68,0x01,0x02,0x10,0x90,0x00,0x16};//当前正向有功总电能  0.01KWH 4字节BCD
const unsigned char Mt_645_97_B691_cmd[14]={0x68,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0x68,0x01,0x02,0x91,0xB6,0x00,0x16};//1路电压 0.001V  3字节BCD
const unsigned char Mt_645_97_B6A1_cmd[14]={0x68,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0x68,0x01,0x02,0xA1,0xB6,0x00,0x16};//1路电流 0.0001A 4字节BCD


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

//读电量
static int kwh_read(UINT32 *meter_now_KWH, int fd, unsigned char *addr,int gun_id,int DIR_Ctl_io)
{
	int i=0,j=0,start_flag=0;
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
	SentBuf[12] = 0x01; //读数据
	SentBuf[13] = 0x02; //数据域长度
	SentBuf[14] = 0x43; //10+0x33 9010是当前正向有功总电量数据的地址---先传低字节
	SentBuf[15] = 0xC3; //90+0x33
	SentBuf[16] = Meter_645_cs(&SentBuf[4],12);	
	SentBuf[17] = 0x16;	

	Led_Relay_Control(DIR_Ctl_io, COM_RS485_TX_LEVEL);
	write(fd, SentBuf, 18+1);
	do{
		ioctl(fd, TIOCSERGETLSR, &status);
	} while (status!=TIOCSER_TEMT);
	Led_Relay_Control(DIR_Ctl_io, COM_RS485_RX_LEVEL);
	Globa->dc_meter_tx_cnt[gun_id]++;
	Count = read_datas_tty(fd, recvBuf, MAX_MODBUS_FRAMESIZE, 1000000, 300000);
  if(Count >= 18){//响应可能没有0xfe引导码

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

  	if((Count-start_flag) != 18){
  		return (-1);
  	}

		if((Meter_645_cs(&recvBuf[start_flag], Count-start_flag-2)) == recvBuf[Count-2]){
			if((recvBuf[start_flag] == 0x68)&&(recvBuf[Count-1] == 0x16)){//起始符，结束符正确
				AddSun0x33(&recvBuf[start_flag + Dlt645_Read_ID0], 6, 2);//减33h       Cost=1=+33,其它=-33														
				if((recvBuf[start_flag+Dlt645_Read_ID0] == 0x10)&&(recvBuf[start_flag+Dlt645_Read_ID1] == 0x90)){//标识符正确
					*meter_now_KWH = (bcd_to_int_1byte(recvBuf[start_flag+15])*1000000 + \
					                 bcd_to_int_1byte(recvBuf[start_flag+14])*10000 + \
					                 bcd_to_int_1byte(recvBuf[start_flag+13])*100 + \
					                 bcd_to_int_1byte(recvBuf[start_flag+12]));

//			printf("meter_now_KWH xxxxxxxxxxxxxxxxxxxxxx= %d \n", *meter_now_KWH);
					Globa->dc_meter_rx_ok_cnt[gun_id]++;
					return (0);//抄收数据正确
				}
			}
		}
	}

	return (-1);
}

//读电压
static int volt_read(UINT32 *Output_voltage, int fd, unsigned char *addr,int DIR_Ctl_io)
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
	SentBuf[12] = 0x01; //读数据
	SentBuf[13] = 0x02; //数据域长度
	SentBuf[14] = 0xC4; //91+0x33 B691是当前正向有功总电量数据的地址---先传低字节
	SentBuf[15] = 0xE9; //B6+0x33
	SentBuf[16] = Meter_645_cs(&SentBuf[4],12);	
	SentBuf[17] = 0x16;	

	Led_Relay_Control(DIR_Ctl_io, COM_RS485_TX_LEVEL);
	write(fd, SentBuf, 18+1);
	do{
		ioctl(fd, TIOCSERGETLSR, &status);
	} while (status!=TIOCSER_TEMT);
	Led_Relay_Control(DIR_Ctl_io, COM_RS485_RX_LEVEL);

	Count = read_datas_tty(fd, recvBuf, MAX_MODBUS_FRAMESIZE, 1000000, 300000);
  if(Count >= 17){//响应可能没有0xfe引导码
  	for(i=0;i<5;i++){
  		if(recvBuf[i] == 0x68){
  			start_flag = i;
  			break;
  		}
  	}

  	if(i > 4){
  		return (-1);
  	}

  	if((Count-start_flag) != 17){
  		return (-1);
  	}

		if((Meter_645_cs(&recvBuf[start_flag], Count-start_flag-2)) == recvBuf[Count-2]){
			if((recvBuf[start_flag] == 0x68)&&(recvBuf[Count-1] == 0x16)){//起始符，结束符正确
				AddSun0x33(&recvBuf[start_flag + Dlt645_Read_ID0], 6, 2);//减33h       Cost=1=+33,其它=-33														
				if((recvBuf[start_flag+Dlt645_Read_ID0] == 0x91)&&(recvBuf[start_flag+Dlt645_Read_ID1] == 0xB6)){//标识符正确
					*Output_voltage = (bcd_to_int_1byte(recvBuf[start_flag+14]&0x7F)*10000 + \
					                  bcd_to_int_1byte(recvBuf[start_flag+13])*100 + \
					                  bcd_to_int_1byte(recvBuf[start_flag+12]));
					                  
//			printf("Output_voltage xxxxxxxxxxxxxxxxxxxxxx= %d \n", *Output_voltage);
					return (0);//抄收数据正确
				}
			}
		}
	}

	return (-1);
}

//读电流
static int curt_read(UINT32 *Output_current, int fd, unsigned char *addr,int DIR_Ctl_io)
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
	SentBuf[12] = 0x01; //读数据
	SentBuf[13] = 0x02; //数据域长度
	SentBuf[14] = 0xD4; //A1+0x33 B6A1是当前正向有功总电量数据的地址---先传低字节
	SentBuf[15] = 0xE9; //B6+0x33
	SentBuf[16] = Meter_645_cs(&SentBuf[4],12);	
	SentBuf[17] = 0x16;	


	Led_Relay_Control(DIR_Ctl_io, COM_RS485_TX_LEVEL);
	write(fd, SentBuf, 18+1);
	do{
		ioctl(fd, TIOCSERGETLSR, &status);
	} while (status!=TIOCSER_TEMT);
	Led_Relay_Control(DIR_Ctl_io, COM_RS485_RX_LEVEL);

	Count = read_datas_tty(fd, recvBuf, MAX_MODBUS_FRAMESIZE, 1000000, 300000);
  if(Count >= 18){//响应可能没有0xfe引导码
  	for(i=0;i<5;i++){
  		if(recvBuf[i] == 0x68){
  			start_flag = i;
  			break;
  		}
  	}

  	if(i > 4){
  		return (-1);
  	}

  	if((Count-start_flag) != 18){
  		return (-1);
  	}

		if((Meter_645_cs(&recvBuf[start_flag], Count-start_flag-2)) == recvBuf[Count-2]){
			if((recvBuf[start_flag] == 0x68)&&(recvBuf[Count-1] == 0x16)){//起始符，结束符正确
				AddSun0x33(&recvBuf[start_flag + Dlt645_Read_ID0], 6, 2);//减33h       Cost=1=+33,其它=-33
				if((recvBuf[start_flag+Dlt645_Read_ID0] == 0xA1)&&(recvBuf[start_flag+Dlt645_Read_ID1] == 0xB6)){//标识符正确
					*Output_current = (bcd_to_int_1byte(recvBuf[start_flag+15]&0x7F)*1000000 + \
					                  bcd_to_int_1byte(recvBuf[start_flag+14])*10000 + \
					                  bcd_to_int_1byte(recvBuf[start_flag+13])*100 + \
					                  bcd_to_int_1byte(recvBuf[start_flag+12]))/10;

//			printf("Output_current xxxxxxxxxxxxxxxxxxxxxx= %d \n", *Output_current);
					return (0);//抄收数据正确
				}
			}
		}
	}

	return (-1);
}


//下发充直流分流器参数Dc_shunt_Set_meter_flag
static int Dc_shunt_Set_meter(UINT32 DC_Shunt_Range, int fd, unsigned char *addr, int gun,int DIR_Ctl_io)
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
	SentBuf[14] = 0x44; //A1+0x33 B6A1是当前正向有功总电量数据的地址---先传低字节
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
	Led_Relay_Control(DIR_Ctl_io, COM_RS485_TX_LEVEL);
	write(fd, SentBuf, 26+1);
	do{
		ioctl(fd, TIOCSERGETLSR, &status);
	} while (status!=TIOCSER_TEMT);
	Led_Relay_Control(DIR_Ctl_io, COM_RS485_RX_LEVEL);

	Count = read_datas_tty(fd, recvBuf, MAX_MODBUS_FRAMESIZE, 1000000, 300000);
	//if(Count >= 16)
	{//响应可能没有0xfe引导码
		for(i=0;i<5;i++){
			if(recvBuf[i] == 0x68){
				start_flag = i;
				break;
			}
		}

		if(i > 4){
			return (-1);
		}

		// if((Count-start_flag) != 16){
			// return (-1);
		// }

		if((Meter_645_cs(&recvBuf[start_flag], Count-start_flag-2)) == recvBuf[Count-2]){
			if((recvBuf[start_flag] == 0x68)&&(recvBuf[Count-1] == 0x16))
			{//起始符，结束符正确				
				Dc_shunt_Set_meter_done_flag[gun-1] = 1;
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
static void Meter_Read_Write_Task(void* para)
{
	int fd, Ret = 0;
	int DIR_Ctl_io = COM3_DIR_IO;//IO换向
	int i=0,j;
	UINT32 meter_connect[CONTROL_DC_NUMBER];
	UINT32 meter_connect_count[CONTROL_DC_NUMBER];
	UINT32 s_count[CONTROL_DC_NUMBER];//test
	UINT32 Output_voltage = 0;				//充电机输出电压 0.001V
	UINT32 Output_current = 0;				//充电机输出电流 0.001A
	UINT32 meter_now_KWH = 0;				//电表电能 0.01kwh
	
	UINT32 gun_config = CONTROL_DC_NUMBER;
	unsigned char Power_meter_addr[CONTROL_DC_NUMBER][12];
	
	Com_run_para  com_thread_para = *((Com_run_para*)para);
	if( SYS_COM3 == com_thread_para.com_id)
		DIR_Ctl_io = COM3_DIR_IO;	
	if( SYS_COM4 == com_thread_para.com_id)
		DIR_Ctl_io = COM4_DIR_IO;	
	if( SYS_COM5 == com_thread_para.com_id)
		DIR_Ctl_io = COM5_DIR_IO;	
	if( SYS_COM6 == com_thread_para.com_id)
		DIR_Ctl_io = COM6_DIR_IO;	
	
	fd = OpenDev(com_thread_para.com_dev_name);
	if(fd == -1) 
	{
		while(1);
	}else
	{
		set_speed(fd, com_thread_para.com_baud);
		//set_Parity(fd, 8, 1, 2);//偶校验
		set_Parity(fd, com_thread_para.com_data_bits, com_thread_para.com_stop_bits, com_thread_para.even_odd);//偶校验
		close(fd);
	}
	fd = OpenDev(com_thread_para.com_dev_name);	
	if(fd == -1) {
		while(1);
	}
	prctl(PR_SET_NAME,(unsigned long)"DCM_97_Task");//设置线程名字
	for(i=0;i<CONTROL_DC_NUMBER;i++)
	{
		meter_connect[i] = 0;
		meter_connect_count[i] = 0;
		s_count[i] = 0;
	}
	
	while(1)
	{
		usleep(100000);//100ms    500K 7-8秒一度电
		Meter_Read_Write_Watchdog = 1;

		gun_config = Globa->Charger_param.gun_config;
		if(gun_config > CONTROL_DC_NUMBER)
			gun_config = CONTROL_DC_NUMBER;
		
		for(i=0;i<gun_config;i++)
			memcpy(Power_meter_addr[i],Globa->Charger_param.Power_meter_addr[i],12);

		for(i=0;i<gun_config;i++)
		{
			Ret = kwh_read(&meter_now_KWH, fd, Power_meter_addr[i],i,DIR_Ctl_io);//读电量
			if(Ret == -1)
			{
			}else if(Ret == 0)
			{
				meter_connect[i] = 1;
				Globa->Control_DC_Infor_Data[i].meter_KWH = meter_now_KWH*METER_READ_MULTIPLE;// + s_count[i]*500;//test每次加1度
				Globa->Control_DC_Infor_Data[i].meter_KWH_update_flag = 1;
				s_count[i]++;//test
				//printf("Globa[%d]->meter_KWH = %d.%d kwh\n",i,meter_now_KWH/100,meter_now_KWH%100);
			}
			usleep(200000);//200ms
		}
		
		for(i=0;i<gun_config;i++)
		{
			if(Dc_shunt_Set_meter_flag[i] == 1)//下发电表分流器量程
			{
				Dc_shunt_Set_meter_done_flag[i] = 0;//clear
				Dc_shunt_Set_meter(Globa->Charger_param.DC_Shunt_Range[i], fd, Power_meter_addr[i],i+1,DIR_Ctl_io);
				usleep(200000);//200ms
				if(Dc_shunt_Set_meter_done_flag[i])//下发成功
					Dc_shunt_Set_meter_flag[i] = 0;//clear
			}
		}
		
		
		for(i=0;i<gun_config;i++)
		{
			if(meter_connect[i] == 0)
			{
				meter_connect_count[i]++;
				if(meter_connect_count[i] >= 10)
				{
					meter_connect_count[i] = 0;
					if(Globa->Control_DC_Infor_Data[i].Error.meter_connect_lost == 0)
					{
						Globa->Control_DC_Infor_Data[i].Error.meter_connect_lost = 1;							
						sent_warning_message(0x99, 54, i+1, 0);//i+1为枪号						
						Globa->Control_DC_Infor_Data[i].Error_system++;
					}
				}
			}
			else//ok
			{
				meter_connect_count[i] = 0;
				if(Globa->Control_DC_Infor_Data[i].Error.meter_connect_lost == 1)
				{
					Globa->Control_DC_Infor_Data[i].Error.meter_connect_lost = 0;
					sent_warning_message(0x98, 54, i+1, 0);	//i+1为枪号					
					Globa->Control_DC_Infor_Data[i].Error_system--;						
				}
			}
		}
	  
			
	}
}

/*********************************************************
**description：电表读写线程初始化及建立
***parameter  :none
***return		  :none
**********************************************************/
extern void Meter_Read_Write_Thread(void* para)
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
	if(0 != pthread_create(&td1, &attr, (void *)Meter_Read_Write_Task, para)){
		perror("####pthread_create IC_Read_Write_Task ERROR ####\n\n");
	}

	ret = pthread_attr_destroy(&attr);
	if(ret != 0)
		return;
}
