/*******************************************************************************
  Copyright(C)    2014-2015,      EAST Co.Ltd.
  File name :     Led_Control.c
  Author :        
  Version:        1.0
  Date:           2019.2.28
  Description:    
  Other:          
  Function List:  
  History:        
          <author>   <time>   <version>   <desc>
                   2019.2.28   1.0      Create
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
#include <sys/prctl.h>
#include "globalvar.h"



enum led_Plug_Status
{
	Plug_Idle = 0,
	Plug_In,
	Plug_Charging,
	Plug_Charge_end,
	Plug_Fault,
	Plug_Unaviable,
	Plug_Ordering,	
};
#define LED_AUTO_RUNMODE    0 //LED灯根据状态值自动控�?
#define LED_HOST_CTL_MODE   1 //LED灯由上位机单独控制每个LED
/*******************************************************************************
**description：电表读线程
***parameter  :none
***return		  :none
*******************************************************************************/
static void LED_C_Interface_Task(void* para)
{
	int fd, status,i=0;
	int DIR_Ctl_io = COM3_DIR_IO;//IO换向
	UINT32 Count;
	UINT16 wCRCCheckValue,Led_connect,Led_connect_lost_count;
	UINT8 send_buf[50], recv_buf[255];
	UINT8 LED_CTL_MODE = 0,LED_CTL_STEP = 0,old_LED_CTL_MODE= 255,old_LED_CTL_STEP = 255;
	UINT32 led_board_no;//�?---4
	Com_run_para  com_thread_para = *((Com_run_para*)para);
	led_board_no = com_thread_para.reserved1;//
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
		set_Parity(fd, com_thread_para.com_data_bits, com_thread_para.com_stop_bits, com_thread_para.even_odd);//偶校�?
		close(fd);
	}
	fd = OpenDev(com_thread_para.com_dev_name);
	if(fd == -1) 
	{
		while(1);
	}
	if(0 == led_board_no)
		prctl(PR_SET_NAME,(unsigned long)"LED12_Control_Task");//设置线程名字 
	else
		prctl(PR_SET_NAME,(unsigned long)"LED34_Control_Task");//设置线程名字 
	while(1)
	{
		usleep(600000);//600ms
	
		Led_connect = 0;	
		
		if((LED_CTL_MODE != old_LED_CTL_MODE)||(old_LED_CTL_STEP!= LED_CTL_STEP))
		{
			old_LED_CTL_STEP = LED_CTL_STEP;
			old_LED_CTL_MODE = LED_CTL_MODE;
			printf("灯的控制方式 LED_CTL_MODE=%d,LED_CTL_STEP=%d\n", LED_CTL_MODE, LED_CTL_STEP);
		}
		switch(LED_CTL_MODE)
		{
			case LED_AUTO_RUNMODE://自控模式
			{
				switch(LED_CTL_STEP)
				{
					case 0://设置灯控制模�?
					{
						send_buf[0] = 18;  //addr = 18
						send_buf[1] = 0x06;  //功能�?
						send_buf[2] = 0x00;  //起始地址高字�?
						send_buf[3] = 13;  //起始地址低字节，起始地址0

						send_buf[4] = 0;  //寄存器�?
						send_buf[5] = 2;  //寄存器�?�?表示上位机下发系统状态，灯板自动控制灯的显示状�?默认此模�?
					
						wCRCCheckValue = ModbusCRC(&send_buf[0], 6);  //Modbus 协议crc校验
						send_buf[6] = wCRCCheckValue >> 8;
						send_buf[7] = wCRCCheckValue & 0x00ff;
						
						Led_Relay_Control(DIR_Ctl_io, COM_RS485_TX_LEVEL);
						usleep(5000); //50ms
						write(fd, send_buf, 8);
						do{
							ioctl(fd, TIOCSERGETLSR, &status);
						} while (status!=TIOCSER_TEMT);
						usleep(50000); //50ms
						Led_Relay_Control(DIR_Ctl_io, COM_RS485_RX_LEVEL);
						Count = read_datas_tty(fd, recv_buf, MAX_MODBUS_FRAMESIZE-1, 1000000, 300000);
						if((Count == 8)&& (0==memcmp(recv_buf,send_buf,8)))//all match
						{//表示设置成功
							LED_CTL_STEP = 1;
							Led_connect = 1;
						}
						break;
					}
					case 1://设置枪数�?
					{
						send_buf[0] = 18;  //addr = 18
						send_buf[1] = 0x06;  //功能�?
						send_buf[2] = 0x00;  //起始地址高字�?
						send_buf[3] = 17;  //起始地址低字节，起始地址0

						send_buf[4] = 0;  //寄存器�?
						send_buf[5] = 2;  //寄存器�?�?表示2把枪模式
					
						wCRCCheckValue = ModbusCRC(&send_buf[0], 6);  //Modbus 协议crc校验
						send_buf[6] = wCRCCheckValue >> 8;
						send_buf[7] = wCRCCheckValue & 0x00ff;
						
						Led_Relay_Control(DIR_Ctl_io, COM_RS485_TX_LEVEL);
						usleep(5000); //50ms
						write(fd, send_buf, 8);
						do{
							ioctl(fd, TIOCSERGETLSR, &status);
						} while (status!=TIOCSER_TEMT);
						usleep(50000); //50ms
						Led_Relay_Control(DIR_Ctl_io, COM_RS485_RX_LEVEL);
						Count = read_datas_tty(fd, recv_buf, MAX_MODBUS_FRAMESIZE-1, 1000000, 300000);
						if((Count == 8)&& (0==memcmp(recv_buf,send_buf,8)))//all match
						{//表示设置成功
							LED_CTL_STEP = 2;
							Led_connect = 1;
						}						
					}
					break;
					
					case 2://进行灯控�?
					{
						//------------------------------------------------------------------
						send_buf[0] = 18;  //addr = 18
						send_buf[1] = 0x10;  //功能�?
						send_buf[2] = 0x00;  //起始地址高字�?
						send_buf[3] = 18;  //起始地址低字节，起始地址18
						
						send_buf[4] = 0x00;  //寄存器个�?
						send_buf[5] = 2;  //控制2把枪
						
						send_buf[6] = 4;//字节数，2个寄存器�?字节
						
						send_buf[7] = 0x00;  //
						if(Globa->Control_DC_Infor_Data[led_board_no*2].Error_system != 0)
						{//有故�?						
							send_buf[8] = Plug_Fault;  //枪故�?红灯闪烁)
						}else
						{
							 if(Globa->Electric_pile_workstart_Enable[led_board_no * 2] == 0){
								send_buf[8] = Plug_Unaviable;  //表示充电枪不可使�?红色常亮)
							 }else if(Globa->Control_DC_Infor_Data[led_board_no*2].gun_state == 0x05){								 
								send_buf[8] = Plug_Charge_end;  //表示充电完成等待结算(绿灯同时闪烁)
							 }else if(Globa->Control_DC_Infor_Data[led_board_no*2].gun_state == 0x04){								 
								send_buf[8] = Plug_Charging;  //表示充电�?绿灯跑马�?							 
							 }else if(Globa->Control_DC_Infor_Data[led_board_no*2].gun_state == 0x03){//表示空闲未插�?
								send_buf[8] = Plug_Idle; 							 
							 }else if(Globa->Control_DC_Infor_Data[led_board_no*2].gun_state == 0x07){
								send_buf[8] = Plug_In;  //表示已插�?绿灯常亮)
							 }
							 if(Globa->Control_DC_Infor_Data[led_board_no*2].charger_state == 0x06)//倒计时充�?
								send_buf[8] = Plug_Ordering;  //表示预约
						}
						send_buf[9] = 0x00;
						if(Globa->Control_DC_Infor_Data[led_board_no*2+1].Error_system != 0)
						{//有故�?						
							send_buf[10] = Plug_Fault;  //枪故�?红灯闪烁)
						}else
						{
							if(Globa->Electric_pile_workstart_Enable[led_board_no * 2 + 1] == 0){
								send_buf[10] = Plug_Unaviable;  //表示充电枪不可使�?红色常亮)
							 }else if(Globa->Control_DC_Infor_Data[led_board_no*2+1].gun_state == 0x05){								 
								send_buf[10] = Plug_Charge_end;  //表示充电完成等待结算(绿灯同时闪烁)
							 }else if(Globa->Control_DC_Infor_Data[led_board_no*2+1].gun_state == 0x04){								 
								send_buf[10] = Plug_Charging;  //表示充电�?绿灯跑马�?							 
							 }else if(Globa->Control_DC_Infor_Data[led_board_no*2+1].gun_state == 0x03){//表示空闲未插�?
								send_buf[10] = Plug_Idle; 							 
							 }else if(Globa->Control_DC_Infor_Data[led_board_no*2+1].gun_state == 0x07){
								send_buf[10] = Plug_In;  //表示已插�?绿灯常亮)
							 }
							 if(Globa->Control_DC_Infor_Data[led_board_no*2+1].charger_state == 0x06)//倒计时充�?
								send_buf[10] = Plug_Ordering;  //表示预约
						}
																		
						wCRCCheckValue = ModbusCRC(&send_buf[0], 11);  //Modbus 协议crc校验
						send_buf[11] = wCRCCheckValue >> 8;
						send_buf[12] = wCRCCheckValue & 0x00ff;
						Led_Relay_Control(DIR_Ctl_io, COM_RS485_TX_LEVEL);
						usleep(5000); //50ms
						write(fd, send_buf, 13 );
						do{
							ioctl(fd, TIOCSERGETLSR, &status);
						} while (status!=TIOCSER_TEMT);
						usleep(50000); //50ms
						Led_Relay_Control(DIR_Ctl_io, COM_RS485_RX_LEVEL);
						
						Count = read_datas_tty(fd, recv_buf, MAX_MODBUS_FRAMESIZE-1, 1000000, 300000);
						if((Count == 8)&&  (recv_buf[1] == 0x10) )
						{//表示设置成功
							Led_connect = 1;
						}
						// if(Globa->Control_DC_Infor_Data[0].Error.System_power_fault == 1)//表示停电�?
						// {
							// LED_CTL_STEP = 0;
							// LED_CTL_MODE = LED_HOST_CTL_MODE;
						// }
						break;
					}
				}
			
			  break;
			}
			/* case LED_HOST_CTL_MODE://上位机控制模�?
			{
				switch(LED_CTL_STEP)
				{
					case 0://设置灯控制模�?
					{
						send_buf[0] = 18;  //addr = 18
						send_buf[1] = 0x06;  //功能�?
						send_buf[2] = 0x00;  //起始地址高字�?
						send_buf[3] = 13;  //起始地址低字节，起始地址0

						send_buf[4] = 0;  //寄存器�?
						send_buf[5] = 1;  //寄存器�?�?表示上位机下发系统状态，灯板自动控制灯的显示状�?默认此模�?
					
						wCRCCheckValue = ModbusCRC(&send_buf[0], 6);  //Modbus 协议crc校验
						send_buf[6] = wCRCCheckValue >> 8;
						send_buf[7] = wCRCCheckValue & 0x00ff;
						
						Led_Relay_Control(DIR_Ctl_io, COM_RS485_TX_LEVEL);
						usleep(5000); //50ms
						write(fd, send_buf, 8);
						do{
							ioctl(fd, TIOCSERGETLSR, &status);
						} while (status!=TIOCSER_TEMT);
						usleep(50000); //50ms
						Led_Relay_Control(DIR_Ctl_io, COM_RS485_RX_LEVEL);
						Count = read_datas_tty(fd, recv_buf, MAX_MODBUS_FRAMESIZE-1, 1000000, 300000);
						if((Count == 8)&& (0==memcmp(recv_buf,send_buf,8)))//all match
						{//表示设置成功
							LED_CTL_STEP = 1;	
							Led_connect = 1;
						}
						break;
					}
					case 1://进行灯控�?
					{
						send_buf[0] = 18;  //addr = 18
						send_buf[1] = 0x10;  //功能�?
						send_buf[2] = 0x00;  //起始地址高字�?
						send_buf[3] = 0;  //起始地址低字节，起始地址0

						send_buf[4] = 0;  //寄存器个数高8�?
						send_buf[5] = 13; //寄存器个数低8�?
					  
						send_buf[6] = 26; //字节�?6
						
						send_buf[7] = 0;  //LED1寄存器值高8�?
						send_buf[8] = 4;  //LED1寄存器值低8�?
						send_buf[9] =  0; //LED2寄存器值高8�?
						send_buf[10] = 4; //LED2寄存器值低8�?
						send_buf[11] = 0; //LED3寄存器值高8�?
						send_buf[12] = 4; //LED3寄存器值低8�?
						send_buf[13] = 0; //LED4寄存器值高8�?
						send_buf[14] = 4; //LED4寄存器值低8�?	
						send_buf[15] = 0; //LED5寄存器值高8�?
						send_buf[16] = 4; //LED5寄存器值低8�?
						
						send_buf[17] = 0; //LED6寄存器值高8�?
						send_buf[18] = 0; //LED6寄存器值低8�?
						send_buf[19] = 0; //LED7寄存器值高8�?
						send_buf[20] = 0; //LED7寄存器值低8�?
						send_buf[21] = 0; //LED8寄存器值高8�?
						send_buf[22] = 0; //LED8寄存器值低8�?
					
						send_buf[23] = 0; //LED9寄存器值高8�?
						send_buf[24] = 4; //LED9寄存器值低8�?
						send_buf[25] = 0; //LED10寄存器值高8�?
						send_buf[26] = 4; //LED10寄存器值低8�?
						send_buf[27] = 0; //LED11寄存器值高8�?
						send_buf[28] = 4; //LED11寄存器值低8�?
						send_buf[29] = 0; //LED12寄存器值高8�?
						send_buf[30] = 4; //LED12寄存器值低8�?	
						send_buf[31] = 0; //LED13寄存器值高8�?
						send_buf[32] = 4; //LED13寄存器值低8�?	
						
						wCRCCheckValue = ModbusCRC(&send_buf[0], 33);  //Modbus 协议crc校验
						send_buf[33] = wCRCCheckValue >> 8;
						send_buf[34] = wCRCCheckValue & 0x00ff;
						
						Led_Relay_Control(DIR_Ctl_io, COM_RS485_TX_LEVEL);
						usleep(5000); //50ms
						write(fd, send_buf, 35);
						do{
							ioctl(fd, TIOCSERGETLSR, &status);
						} while (status!=TIOCSER_TEMT);
						usleep(50000); //50ms
						Led_Relay_Control(DIR_Ctl_io, COM_RS485_RX_LEVEL);
						Count = read_datas_tty(fd, recv_buf, MAX_MODBUS_FRAMESIZE-1, 1000000, 300000);
						if((Count == 8)&& (recv_buf[1] == 0x10))//
						{//表示设置成功
							//LED_CTL_STEP = 1;	
							Led_connect = 1;
						}
						if(Globa->Control_DC_Infor_Data[0].Error.System_power_fault == 0)//来电�?
						{
							LED_CTL_STEP = 0;
							LED_CTL_MODE = LED_AUTO_RUNMODE;
						}
						break;
					}
			    break;
			  }
				break;
			} */
			default:
				LED_CTL_STEP = 0;
				LED_CTL_MODE = LED_AUTO_RUNMODE;
			break;
		}
			
		/* if(Led_connect == 0)
		{
			Led_connect_lost_count++;
			if(Led_connect_lost_count >= 20)
			{
				Led_connect_lost_count = 0;
				if(Globa->led_connect_lost == 0)
				{
					Globa->led_connect_lost = 1;
					sent_warning_message(0x99, ALARM_LED_BOARD_COMM_FAILED, 0, 0);
					Globa->Control_DC_Infor_Data[0].Warn_system++;
					Globa->Control_DC_Infor_Data[1].Warn_system++;
					Globa->Control_DC_Infor_Data[2].Warn_system++;					
				}
			}
		}else
		{
			Led_connect_lost_count = 0;
			if(Globa->led_connect_lost == 1)
			{
				Globa->led_connect_lost = 0;
				sent_warning_message(0x98, ALARM_LED_BOARD_COMM_FAILED, 0, 0);
				if(Globa->Control_DC_Infor_Data[0].Warn_system > 0)
					Globa->Control_DC_Infor_Data[0].Warn_system--;
				if(Globa->Control_DC_Infor_Data[1].Warn_system > 0)
					Globa->Control_DC_Infor_Data[1].Warn_system--;
				if(Globa->Control_DC_Infor_Data[2].Warn_system > 0)
					Globa->Control_DC_Infor_Data[2].Warn_system--;
				
			}
		} */
		//------------------------------end-----------------------------------------
	}
	close(fd);
}

/*******************************************************************************
**description：线程初始化及建�?
***parameter  :none
***return		  :none
*******************************************************************************/
extern void LED_C_Interface_Thread(void* para)
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
	if(0 != pthread_create(&td1, &attr, (void *)LED_C_Interface_Task, para)){
		perror("####pthread_create IC_Read_Write_Task ERROR ####\n\n");
	}

	ret = pthread_attr_destroy(&attr);
	if(ret != 0)
		return;
}
