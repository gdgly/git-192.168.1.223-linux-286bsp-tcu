/*******************************************************************************
  Copyright(C)    2018-2015,      EAST Co.Ltd.
  File name :     Led_Control.c
  Author :        dengjh
  Version:        1.0
  Date:           2018.9.11
  Description:    
  Other:          
  Function List:  
  History:        
          <author>   <time>   <version>   <desc>
            dengjh    2018.9.11   1.0      Create
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
#include <asm/ioctls.h>
#include "globalvar.h"
/*******************************************************************************
**description：电表读线程
***parameter  :none
***return		  :none
*******************************************************************************/
static void LED_C_Interface_Task(void)
{
	int fd, status,i=0;
  UINT32 Count;
  UINT16 wCRCCheckValue,AC_connect,AC_connect_count;
	UINT8 sendbuf[50], Buf[255];
	UINT8 LED_C_STEP = 0,LED_C_STEP_RUN = 0,old_LED_C_STEP= 255,old_LED_C_STEP_RUN = 255;
	fd = OpenDev(LED_CONTROL_COM_ID);
	if(fd == -1) {
		while(1);
	}else{
		set_speed(fd, 9600);
	//	set_speed(fd, 2400);
		set_Parity(fd, 8, 1, 0);
		close(fd);
  }
	fd = OpenDev(LED_CONTROL_COM_ID);
	if(fd == -1) {
		while(1);
	}
	prctl(PR_SET_NAME,(unsigned long)"led_Control_Task");//设置线程名字 

  while(1){
  	usleep(600000);//10ms
		AC_connect = 0;	
		if((LED_C_STEP != old_LED_C_STEP)||(old_LED_C_STEP_RUN!= LED_C_STEP_RUN)){
			old_LED_C_STEP_RUN = LED_C_STEP_RUN;
			old_LED_C_STEP = LED_C_STEP;
  	   printf("灯的控制方式 ret=%d,%0x,%0x\n", LED_C_STEP, LED_C_STEP_RUN);
		}
		switch(LED_C_STEP)
		{
			case 0:
			{
				switch(LED_C_STEP_RUN)
				{
					case 0://设置灯控制模式
					{
						sendbuf[0] = 18;  //addr = 62
						sendbuf[1] = 0x06;  //功能码
						sendbuf[2] = 0x00;  //起始地址高字节
						sendbuf[3] = 13;  //起始地址低字节，起始地址0

						sendbuf[4] = 0;  //寄存器值
						sendbuf[5] = 2;  //寄存器值 值2表示上位机下发系统状态，灯板自动控制灯的显示状态(默认此模式
					
  					wCRCCheckValue = ModbusCRC(&sendbuf[0], 6);  //Modbus 协议crc校验
						sendbuf[6] = wCRCCheckValue >> 8;
						sendbuf[7] = wCRCCheckValue & 0x00ff;
						
						Led_Relay_Control(7, 0);
						usleep(5000); //50ms
						write(fd, sendbuf, 8);
						do{
							ioctl(fd, TIOCSERGETLSR, &status);
						} while (status!=TIOCSER_TEMT);
						usleep(50000); //50ms
						Led_Relay_Control(7, 1);
						Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE-1, 300000, 20000);
            if((Count == 8)&&(Buf[1] == sendbuf[1])){//表示设置成功
							LED_C_STEP_RUN = 1;
							LED_C_STEP = 0;
						}
						break;
					}
					case 1://进行灯控制
					{
						//------------------------------------------------------------------
						sendbuf[0] = 18;  //addr = 62
						sendbuf[1] = 0x10;  //功能码
						sendbuf[2] = 0x00;  //起始地址高字节
						sendbuf[3] = 17;  //起始地址低字节，起始地址0
						
						sendbuf[4] = 0x00;  //寄存器个数
					 // sendbuf[5] = 3;  //31
						if ((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 ))
						{//表示两把枪
							sendbuf[5] = 3;  //31
							sendbuf[6] = 6;  //31
							
							sendbuf[7] = 0x00;  //寄存器个数
							sendbuf[8] = 2;  //31
						}
						else
						{
							sendbuf[5] = 2;  //31
							sendbuf[6] = 4;  //31
							sendbuf[7] = 0x00;  //
							sendbuf[8] = 1;  //1把枪  
						}
						
						if (Globa_1->Error_system != 0)
						{//有故障
							sendbuf[9] = 0x00;  //
							sendbuf[10] = 4;  //1把枪 值 4 表示故障(红灯闪烁)
						}
						else
						{
							 if (Globa_1->gun_state== 0x07)
							 {//插枪---
								 sendbuf[9] = 0x00;  //
								 sendbuf[10] = 1;  //1把枪值 值 1 表示已插枪(绿灯常亮)
							 }
							 else if (Globa_1->gun_state == 0x03)
							 {//空闲--白色灯
								 sendbuf[9] = 0x00;  //
								 sendbuf[10] = 0;  //1把枪值 值 0 表示空闲未插枪
							 }
							 else if (Globa_1->gun_state == 0x04) 
							 {//充电中--绿灯跑马灯
								 sendbuf[9] = 0x00;  
								 sendbuf[10] = 2; 
							 }
							 else if (Globa_1->gun_state == 0x05)
							 {//充电完成--绿灯闪烁
								 sendbuf[9] = 0x00;  //
								 sendbuf[10] = 3;  //1把枪值 值 0 表示空闲未插枪
							 }
							 else if (Globa_1->gun_state == 0x06)
							 {//预约--
								 sendbuf[9] = 0x00;  //
								 sendbuf[10] = 6;  //1把枪值 值 0 表示空闲未插枪
							 }
						}
						
						if ((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 ))
						{//表示两把枪
							if (Globa_2->Error_system != 0)
							{//有故障
								sendbuf[11] = 0x00;  //
								sendbuf[12] = 4;  //1把枪 值 4 表示故障(红灯闪烁)
							}
							else
						  {
							   if (Globa_2->gun_state== 0x07)
								 {//插枪---
									 if((Globa_1->Charger_param.System_Type == 4)&&(Globa_1->QT_Step  == 0x24)){
										 //充电中--绿灯跑马灯
										 sendbuf[11] = 0x00;  
										 sendbuf[12] = 2; 
									 }else{
										 sendbuf[11] = 0x00;  //
									   sendbuf[12] = 1;  //1把枪值 值 1 表示已插枪(绿灯常亮)
									 }
								 }
								 else if (Globa_2->gun_state == 0x03)
								 {//空闲--白色灯
									 sendbuf[11] = 0x00;  //
									 sendbuf[12] = 0;  //1把枪值 值 0 表示空闲未插枪
								 }
								 else if (Globa_2->gun_state == 0x04) 
								 {//充电中--绿灯跑马灯
									 sendbuf[11] = 0x00;  
									 sendbuf[12] = 2; 
								 }
								 else if (Globa_2->gun_state == 0x05)
								 {//充电完成--绿灯闪烁
									 sendbuf[11] = 0x00;  //
									 sendbuf[12] = 3;  //1把枪值 值 0 表示空闲未插枪
								 }
								 else if (Globa_2->gun_state == 0x06)
								 {//预约--
									 sendbuf[11] = 0x00;  //
									 sendbuf[12] = 6;  //1把枪值 值 0 表示空闲未插枪
								 }
						  }
						}
						
						if(Globa_1->Charger_param.System_Type <= 1 ){//枪1的状态
							wCRCCheckValue = ModbusCRC(&sendbuf[0], 13);  //Modbus 协议crc校验
							sendbuf[13] = wCRCCheckValue >> 8;
							sendbuf[14] = wCRCCheckValue & 0x00ff;
							
							Led_Relay_Control(7, 0);
							usleep(5000); //50ms
							write(fd, sendbuf, 15);
							do{
								ioctl(fd, TIOCSERGETLSR, &status);
							} while (status!=TIOCSER_TEMT);
							usleep(50000); //50ms
							Led_Relay_Control(7, 1);
						}else{
							wCRCCheckValue = ModbusCRC(&sendbuf[0], 11);  //Modbus 协议crc校验
							sendbuf[11] = wCRCCheckValue >> 8;
							sendbuf[12] = wCRCCheckValue & 0x00ff;
							Led_Relay_Control(7, 0);
							usleep(5000); //50ms
							write(fd, sendbuf, 13);
							do{
								ioctl(fd, TIOCSERGETLSR, &status);
							} while (status!=TIOCSER_TEMT);
							usleep(50000); //50ms
							Led_Relay_Control(7, 1);
						}
						Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE-1, 300000, 20000);
						usleep(100000); 
						
					/*	sendbuf[0] = 18;  //addr = 62
						sendbuf[1] = 0x10;  //功能码
						sendbuf[2] = 0x00;  //起始地址高字节
						sendbuf[3] = 21;  //起始地址低字节，起始地址0
						sendbuf[4] = 0x00;  //寄存器个数
						sendbuf[5] = 0x07;  //寄存器个数
						sendbuf[6] = 14;  //字节数
						sendbuf[7] = 0x00;  //
						sendbuf[8] = 3;  //空闲状态指示灯颜色
						sendbuf[9] = 0x00;  //
						sendbuf[10] = 2;   //已插枪指示灯颜色
						sendbuf[11] = 0x00;  //
						sendbuf[12] = 1;    //充电中跑马灯颜色
						sendbuf[13] = 0x00;  //
						sendbuf[14] = 2;  //充电完成等待结算灯
						sendbuf[15] = 0x00;  //
						sendbuf[16] = 4;  //故障灯颜色
						sendbuf[17] = 0x00;  //
						sendbuf[18] = 4;  //充电枪禁用灯颜色		
						sendbuf[19] = 0x00;  //
						sendbuf[20] = 7;  //充电枪预约灯颜色
						wCRCCheckValue = ModbusCRC(&sendbuf[0], 21);  //Modbus 协议crc校验
						sendbuf[21] = wCRCCheckValue >> 8;
						sendbuf[22] = wCRCCheckValue & 0x00ff;
						Led_Relay_Control(7, 0);
						usleep(5000); //50ms
						write(fd, sendbuf, 23);
						do{
							ioctl(fd, TIOCSERGETLSR, &status);
						} while (status!=TIOCSER_TEMT);
						usleep(50000); //50ms
						Led_Relay_Control(7, 1);
					  Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE-1, 300000, 20000);
*/
						/*if(Globa_1->Error.System_power_fault == 1)//表示停电----
						{
							LED_C_STEP_RUN = 0;
							LED_C_STEP = 1;
						}*/
						break;
					}
				}
			
			  break;
			}
			case 1:{
				switch(LED_C_STEP_RUN)
				{
					case 0://设置灯控制模式
					{
						sendbuf[0] = 18;  //addr = 18
						sendbuf[1] = 0x06;  //功能码
						sendbuf[2] = 0x00;  //起始地址高字节
						sendbuf[3] = 13;  //起始地址低字节，起始地址0

						sendbuf[4] = 0;  //寄存器值
						sendbuf[5] = 1;  //寄存器值 值2表示上位机下发系统状态，灯板自动控制灯的显示状态(默认此模式
					
  					wCRCCheckValue = ModbusCRC(&sendbuf[0], 6);  //Modbus 协议crc校验
						sendbuf[6] = wCRCCheckValue >> 8;
						sendbuf[7] = wCRCCheckValue & 0x00ff;
						
						Led_Relay_Control(7, 0);
						usleep(5000); //50ms
						write(fd, sendbuf, 8);
						do{
							ioctl(fd, TIOCSERGETLSR, &status);
						} while (status!=TIOCSER_TEMT);
						usleep(50000); //50ms
						Led_Relay_Control(7, 1);
						Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE-1, 300000, 20000);
            if((Count == 8)&&(Buf[1] == sendbuf[1])){//表示设置成功
							LED_C_STEP_RUN = 1;
							LED_C_STEP = 1;
						}
						break;
					}
					case 1://进行灯控制
					{
						sendbuf[0] = 18;  //addr = 18
						sendbuf[1] = 0x10;  //功能码
						sendbuf[2] = 0x00;  //起始地址高字节
						sendbuf[3] = 0;  //起始地址低字节，起始地址0

						sendbuf[4] = 0;  //寄存器值
						sendbuf[5] = 13; //寄存器值 
					  
						sendbuf[6] = 26; //字节数4
						
						sendbuf[7] = 0;  //寄存器值
						sendbuf[8] = 4; //寄存器值 
						sendbuf[9] = 0;  //寄存器值
					  sendbuf[10] = 4; //寄存器值 
						sendbuf[11] = 0;  //寄存器值
						sendbuf[12] = 4; //寄存器值 
	          sendbuf[13] = 0;  //寄存器值
						sendbuf[14] = 4; //寄存器值 	
						sendbuf[15] = 0;  //寄存器值
						sendbuf[16] = 4; //寄存器值 	
						
						sendbuf[17] = 0;  //寄存器值
						sendbuf[18] = 0; //寄存器值 
						sendbuf[19] = 0;  //寄存器值
					  sendbuf[20] = 0; //寄存器值 
						sendbuf[21] = 0;  //寄存器值
						sendbuf[22] = 0; //寄存器值 
					
						sendbuf[23] = 0;  //寄存器值
						sendbuf[24] = 4; //寄存器值 
						sendbuf[25] = 0;  //寄存器值
					  sendbuf[26] = 4; //寄存器值 
						sendbuf[27] = 0;  //寄存器值
						sendbuf[28] = 4; //寄存器值 
	          sendbuf[29] = 0;  //寄存器值
						sendbuf[30] = 4; //寄存器值 	
						sendbuf[31] = 0;  //寄存器值
						sendbuf[32] = 4; //寄存器值 	
						
  					wCRCCheckValue = ModbusCRC(&sendbuf[0], 33);  //Modbus 协议crc校验
						sendbuf[33] = wCRCCheckValue >> 8;
						sendbuf[34] = wCRCCheckValue & 0x00ff;
						
						Led_Relay_Control(7, 0);
						usleep(5000); //50ms
						write(fd, sendbuf, 35);
						do{
							ioctl(fd, TIOCSERGETLSR, &status);
						} while (status!=TIOCSER_TEMT);
						usleep(50000); //50ms
						Led_Relay_Control(7, 1);
						Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE-1, 300000, 20000);
         	  /*if(Globa_1->Error.System_power_fault == 0)//表示停电
						{
							LED_C_STEP_RUN = 0;
							LED_C_STEP = 0;
						}*/
						break;
					}
			    break;
			  }
		  	break;
		  }
		}
		AC_connect = 1;	
	/*	if(AC_connect == 0){
			AC_connect_count++;
			if(AC_connect_count >= 100){
				AC_connect_count = 0;
				if(Globa_1->Error.AC_connect_lost == 0){
					Globa_1->Error.AC_connect_lost = 1;
					sent_warning_message(0x99, 4, 0, 0);
						Globa_1->Error_system++;
						Globa_2->Error_system++;
						ChargePoint_Status_data[0].ErrorCode = OtherError;
					}
			}
		}else{
			AC_connect_count = 0;
			if(Globa_1->Error.AC_connect_lost == 1){
				Globa_1->Error.AC_connect_lost = 0;
				sent_warning_message(0x98, 4, 0, 0);
						Globa_1->Error_system--;
						Globa_2->Error_system--;
						//Globa_1->Error_ctl1--;
			}
		}*/
		//------------------------------end-----------------------------------------
	}
	close(fd);
}

/*******************************************************************************
**description：电压电流表读线程初始化及建立
***parameter  :none
***return		  :none
*******************************************************************************/
extern void LED_C_Interface_Thread(void)
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
	if(0 != pthread_create(&td1, &attr, (void *)LED_C_Interface_Task, NULL)){
		perror("####pthread_create IC_Read_Write_Task ERROR ####\n\n");
	}

	ret = pthread_attr_destroy(&attr);
	if(ret != 0)
		return;
}
