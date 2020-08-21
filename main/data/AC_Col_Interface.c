/*******************************************************************************
  Copyright(C)    2014-2015,      EAST Co.Ltd.
  File name :     interface_meter.c
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
#include <asm/ioctls.h>
#include <sys/prctl.h>
#include "globalvar.h"
unsigned char ac_out = 0;
time_t ac_out_sec = 0;
unsigned char Jkac_con = 0x50;
typedef enum _AC_MOD_FUN04_ENUM{
    FUN04_AC_MOD_1_AB_V,             //= 0 第一路输入AB线电压
    FUN04_AC_MOD_1_BC_V,             //    第一路输入BC线电压
    FUN04_AC_MOD_1_CA_V,             //    第一路输入CA线电压
    FUN04_AC_MOD_2_AB_V,             //    第二路输入AB线电压 
    FUN04_AC_MOD_2_BC_V,             //    第二路输入BC线电压
    FUN04_AC_MOD_2_CA_V,             //=5  第二路输入CA线电压
    FUN04_AC_MOD_A_A,                //    A相电流
    FUN04_AC_MOD_B_A,                //    B相电流
    FUN04_AC_MOD_C_A,                //    C相电流
    FUN04_AC_MOD_A_ACT_P,            //    A相有功功率
    FUN04_AC_MOD_B_ACT_P,            //=10 B相有功功率
    FUN04_AC_MOD_C_ACT_P,            //    C相有功功率
    FUN04_AC_MOD_T_ACT_P,            //    总有功功率
    FUN04_AC_MOD_A_REACT_P,          //    A相无功功率
    FUN04_AC_MOD_B_REACT_P,          //    B相无功功率
    FUN04_AC_MOD_C_REACT_P,          //=15 C相无功功率
    FUN04_AC_MOD_T_REACT_P,          //    总无功功率
    FUN04_AC_MOD_A_APP_P,            //    A相视在功率
    FUN04_AC_MOD_B_APP_P,
    FUN04_AC_MOD_C_APP_P,
    FUN04_AC_MOD_T_APP_P,            //=20 总视在功率
    FUN04_AC_MOD_P_FACTOR,           //    功率因数
    FUN04_AC_MOD_1_F,                //    第一路频率
    FUN04_AC_MOD_2_F,                //    第二路频率

    FUN04_AC_MOD_TELECOMMAND_1,     //=24交流遥信量（0——15）
    FUN04_AC_MOD_TELECOMMAND_2,    //=25交流遥信量（16——31）
    FUN04_AC_MOD_TELECOMMAND_3,    //=26交流遥信量（32）

    FUN04_AC_MOD_RESERVER = 31
}AC_MOD_FUN04_ENUM;

//修改交流采集模块波特率为2400
int ChangeBAUDTO2400(int fd,char* sendbuf,int DIR_Ctl_io)
{
	UINT16 wCRCCheckValue= 0;
	UINT32 Count = 0;
	UINT8 Buf[255];
	int status= 0;
	sendbuf[0] = 62;   //addr  这个地址要加偏移量
	sendbuf[1] = 0x06;  //功能码
	sendbuf[2] = 0x00;  //起始地址高字节
	sendbuf[3] = 0x00;  //起始地址低字节，起始地址0
	sendbuf[4] = 0x08;  //
	sendbuf[5] = 0x01;  //

	wCRCCheckValue = ModbusCRC(&sendbuf[0], 6);  //Modbus 协议crc校验
	sendbuf[6] = wCRCCheckValue>>8;
	sendbuf[7] = wCRCCheckValue&0x00ff;	

	Led_Relay_Control(DIR_Ctl_io, COM_RS485_TX_LEVEL);
	write(fd, sendbuf, 8);
	do{
		ioctl(fd, TIOCSERGETLSR, &status);
	} while (status!=TIOCSER_TEMT);
	usleep(5000);
	Led_Relay_Control(DIR_Ctl_io, COM_RS485_RX_LEVEL);

	Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE-1, 2000000,300000);
	if(Count > 3)
	{
		if( (Count == 8) || ( (Buf[Count-1]==sendbuf[7])  &&  (Buf[Count-2]==sendbuf[6]) ))//长度匹配或末尾2字符匹配
		{
			//交流采集板已切换至2400波特率
			return 1;
		}
	}
	return 0;
	
}

/*******************************************************************************
**description：j交流采集盒数据电表读线程
***parameter  :none
***return		  :none
*******************************************************************************/
static void AC_Col_Interface_Task(void* para)
{
	int fd = -1, status= 0;
	int i = 0,AC_Relay_control=0;
	UINT32  gun_config;
	UINT32 Count= 0,cnn=0;
	UINT16 wCRCCheckValue= 0;
	UINT8 sendbuf[25], Buf[255];
	UINT32 measure1 = 0,AC_kg_time = 0;
	UINT16 AC_fault_Number = 0,input_alarm_over = 0,input_alarm_under = 0;
	UINT16 tpval_low=0,tpval_high=0,input_alarm=0,vflag=0,voffset=0,wflag=0,xflag=0;

	int DIR_Ctl_io = COM3_DIR_IO;//IO换向
	
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
	prctl(PR_SET_NAME,(unsigned long)"AC_Col_Interface");//设置线程名字
	
	do{
		Count++;
		usleep(500000);//300ms
		cnn = ChangeBAUDTO2400(fd,sendbuf,DIR_Ctl_io);//以9600波特率与采集板通信，设置其为2400波特率
		if(cnn)	
			break;
	}while(Count < 10);
	//if(cnn)//不成功可能从机已经是2400
	{
		close(fd);
		fd = OpenDev(com_thread_para.com_dev_name);
		
		if(fd == -1) 
		{
			while(1);
		}else
		{
			set_speed(fd, 2400);
			set_Parity(fd, 8, 1, 0);
			close(fd);
		}

		fd = OpenDev(com_thread_para.com_dev_name);
		if(fd == -1) {
			while(1);
		}
	}
	
	while(1)
	{
		gun_config = Globa->Charger_param.gun_config;
		if(gun_config > CONTROL_DC_NUMBER)
			gun_config = CONTROL_DC_NUMBER;
		usleep(300000);//300ms
		sendbuf[0] = 62;   //addr  这个地址要加偏移量
		sendbuf[1] = 0x04;  //功能码
		sendbuf[2] = 0x00;  //起始地址高字节
		sendbuf[3] = 0x00;  //起始地址低字节，起始地址0
		sendbuf[4] = 0x00;  //总共读取的寄存器个数高字节
		sendbuf[5] = 0x1F;  //总共读取的寄存器个数低字节 31个

		wCRCCheckValue = ModbusCRC(&sendbuf[0], 6);  //Modbus 协议crc校验
		sendbuf[6] = wCRCCheckValue>>8;
		sendbuf[7] = wCRCCheckValue&0x00ff;	

		Led_Relay_Control(DIR_Ctl_io, COM_RS485_TX_LEVEL);
		write(fd, sendbuf, 8);
		do{
			ioctl(fd, TIOCSERGETLSR, &status);
		} while (status!=TIOCSER_TEMT);
		usleep(5000);
		Led_Relay_Control(DIR_Ctl_io, COM_RS485_RX_LEVEL);
		Globa->ac_col_tx_cnt++;
		Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE-1, 2000000,300000);
		if((Count == (2*sendbuf[5]+5))&&(sendbuf[0] == Buf[0]))
		{//响应数据长度
			wCRCCheckValue = ModbusCRC(&Buf[0], Count-2);//校验CRC
			if(((wCRCCheckValue>>8) == Buf[Count -2])&&((wCRCCheckValue&0x00ff) == Buf[Count -1]))
			{				
				AC_measure_data.V1_AB[0] = Buf[3+2*FUN04_AC_MOD_1_AB_V+1];
				AC_measure_data.V1_AB[1] = Buf[3+2*FUN04_AC_MOD_1_AB_V];
				AC_measure_data.V1_BC[0] = Buf[3+2*FUN04_AC_MOD_1_BC_V+1];
				AC_measure_data.V1_BC[1] = Buf[3+2*FUN04_AC_MOD_1_BC_V];
				AC_measure_data.V1_CA[0] = Buf[3+2*FUN04_AC_MOD_1_CA_V+1];
				AC_measure_data.V1_CA[1] = Buf[3+2*FUN04_AC_MOD_1_CA_V];

				AC_measure_data.C1_A[0] = Buf[3+2*FUN04_AC_MOD_A_A+1];
				AC_measure_data.C1_A[1] = Buf[3+2*FUN04_AC_MOD_A_A];
				AC_measure_data.C1_B[0] = Buf[3+2*FUN04_AC_MOD_B_A+1];
				AC_measure_data.C1_B[1] = Buf[3+2*FUN04_AC_MOD_B_A];
				AC_measure_data.C1_C[0] = Buf[3+2*FUN04_AC_MOD_C_A+1];
				AC_measure_data.C1_C[1] = Buf[3+2*FUN04_AC_MOD_C_A];

				AC_measure_data.PW_ACT[0] = Buf[3+2*FUN04_AC_MOD_T_ACT_P+1];
				AC_measure_data.PW_ACT[1] = Buf[3+2*FUN04_AC_MOD_T_ACT_P];
				AC_measure_data.PW_INA[0] = Buf[3+2*FUN04_AC_MOD_T_REACT_P+1];
				AC_measure_data.PW_INA[1] = Buf[3+2*FUN04_AC_MOD_T_REACT_P];
				AC_measure_data.F1[0] = Buf[3+2*FUN04_AC_MOD_1_F+1];
				AC_measure_data.F1[1] = Buf[3+2*FUN04_AC_MOD_1_F];

				AC_measure_data.output_switch_on1 = (Buf[3+2*FUN04_AC_MOD_TELECOMMAND_1+1]&0x08)?1:0;
				AC_measure_data.output_switch_on2 = (Buf[3+2*FUN04_AC_MOD_TELECOMMAND_1+1]&0x20)?1:0;
				AC_measure_data.input_switch_on   = (Buf[3+2*FUN04_AC_MOD_TELECOMMAND_1+1]&0x01)?1:0;
				AC_measure_data.fanglei_on        = (Buf[3+2*FUN04_AC_MOD_TELECOMMAND_1+1]&0x04)?1:0;
				
				int measure1 = (AC_measure_data.V1_AB[0] + (AC_measure_data.V1_AB[1]<<8) + \
						AC_measure_data.V1_BC[0] + (AC_measure_data.V1_BC[1]<<8) + \
						AC_measure_data.V1_CA[0] + (AC_measure_data.V1_CA[1]<<8))*100/3;

				
				//--------------------------  判断输入过、欠压            -----------/
				if(measure1 > (Globa->Charger_param.input_over_limit))
				{//输入过压保护判断
					if(Globa->Control_DC_Infor_Data[0].Error.input_over_limit == 0)//之前没过压，现在过压了
					{//交流过压
						Globa->Control_DC_Infor_Data[0].Error.input_over_limit = 1;
						sent_warning_message(0x99, 22, 0,0);
						for(i=0;i<gun_config;i++)
							Globa->Control_DC_Infor_Data[i].Error_system++;	
						input_alarm_over = 1;
					}
				}
				else 
				{
					if(measure1 < Globa->Charger_param.input_over_limit-10000)
					{//保护消失 带回差
						if(Globa->Control_DC_Infor_Data[0].Error.input_over_limit == 1)
						{//交流过压
							Globa->Control_DC_Infor_Data[0].Error.input_over_limit = 0;
							sent_warning_message(0x98, 22, 0,0);
							for(i=0;i<gun_config;i++)
								Globa->Control_DC_Infor_Data[i].Error_system--;	
						}
						input_alarm_over = 0;
					}
				}
				
				if(measure1 < Globa->Charger_param.input_lown_limit)
				{//输入欠压告警判断
					if(Globa->Control_DC_Infor_Data[0].Error.input_lown_limit == 0)
					{ //交流欠压
						Globa->Control_DC_Infor_Data[0].Error.input_lown_limit = 1;
						sent_warning_message(0x99, 24, 0,0);
						for(i=0;i<gun_config;i++)
							Globa->Control_DC_Infor_Data[i].Error_system++;	
					}
					input_alarm_under = 1;
				}else
				{					
					if(measure1 > (Globa->Charger_param.input_lown_limit+10000))
					{//告警消失 带回差10v
						if(Globa->Control_DC_Infor_Data[0].Error.input_lown_limit == 1)
						{ //交流欠压
							Globa->Control_DC_Infor_Data[0].Error.input_lown_limit = 0;
							sent_warning_message(0x98, 24, 0,0);
							for(i=0;i<gun_config;i++)
								Globa->Control_DC_Infor_Data[i].Error_system--;							
						}
						input_alarm_under = 0;
					}
				}
				
				if(AC_measure_data.fanglei_on == 0)
				{//
					if(Globa->Control_DC_Infor_Data[0].Error.fanglei_off == 0)
					{//防雷状态
						Globa->Control_DC_Infor_Data[0].Error.fanglei_off = 1;
						sent_warning_message(0x99, 26, 0,0);
						for(i=0;i<gun_config;i++)
							Globa->Control_DC_Infor_Data[i].Error_system++;						
					}
				}else
				{
					if(Globa->Control_DC_Infor_Data[0].Error.fanglei_off == 1)
					{////防雷状态
						Globa->Control_DC_Infor_Data[0].Error.fanglei_off = 0;
						sent_warning_message(0x98, 26, 0,0);
						for(i=0;i<gun_config;i++)
							Globa->Control_DC_Infor_Data[i].Error_system--;						
					}
				}
				
				AC_Relay_control = 0;
				for(i=0;i<gun_config;i++)
					AC_Relay_control |= Globa->Control_DC_Infor_Data[i].AC_Relay_control;
				
				if( AC_Relay_control)//需要闭合交流输入接触器			
				{//交流接触器闭合和风扇闭合
					if(AC_measure_data.input_switch_on == 0)
					{
						AC_kg_time ++;
						if(AC_kg_time >= 100)
						{
							if(Globa->Control_DC_Infor_Data[0].Error.input_switch_off == 0)
							{
								Globa->Control_DC_Infor_Data[0].Error.input_switch_off = 1;
								/* sent_warning_message(0x99, 12, 0);
								Globa->Control_DC_Infor_Data[0].Error_system++;
								Globa->Control_DC_Infor_Data[1].Error_system++;
								Globa->Control_DC_Infor_Data[2].Error_system++;
								Globa->Control_DC_Infor_Data[3].Error_system++; */
							}
						}
					}else
					{
						if(Globa->Control_DC_Infor_Data[0].Error.input_switch_off == 1)
						{
							Globa->Control_DC_Infor_Data[0].Error.input_switch_off = 0;
							/* sent_warning_message(0x98, 12, 0);
							Globa->Control_DC_Infor_Data[0].Error_system--;
							Globa->Control_DC_Infor_Data[1].Error_system--;
							Globa->Control_DC_Infor_Data[2].Error_system--;
							Globa->Control_DC_Infor_Data[3].Error_system--; */
						}
						AC_kg_time = 0;
					}
				}else
				{
					if(Globa->Control_DC_Infor_Data[0].Error.input_switch_off == 1)
					{
						Globa->Control_DC_Infor_Data[0].Error.input_switch_off = 0;
						/* sent_warning_message(0x98, 12, 0);
						Globa->Control_DC_Infor_Data[0].Error_system--;
						Globa->Control_DC_Infor_Data[1].Error_system--;
						Globa->Control_DC_Infor_Data[2].Error_system--;
						Globa->Control_DC_Infor_Data[3].Error_system--; */
					}
					AC_kg_time = 0;
				}
				if(AC_measure_data.output_switch_on1 == 0)
				{//断路器跳状态
					if(Globa->Control_DC_Infor_Data[0].Error.Ac_circuit_breaker_trip == 0)
					{////
						Globa->Control_DC_Infor_Data[0].Error.Ac_circuit_breaker_trip = 1;
						/* sent_warning_message(0x99, 13, 0);
						Globa->Control_DC_Infor_Data[0].Error_system++;
						Globa->Control_DC_Infor_Data[1].Error_system++;
						Globa->Control_DC_Infor_Data[2].Error_system++;
						Globa->Control_DC_Infor_Data[3].Error_system++;  */
					}
				}else
				{
					if(Globa->Control_DC_Infor_Data[0].Error.Ac_circuit_breaker_trip == 1)
					{////
						Globa->Control_DC_Infor_Data[0].Error.Ac_circuit_breaker_trip = 0;
						/* sent_warning_message(0x98, 13, 0);
						Globa->Control_DC_Infor_Data[0].Error_system--;
						Globa->Control_DC_Infor_Data[1].Error_system--;
						Globa->Control_DC_Infor_Data[2].Error_system--;
						Globa->Control_DC_Infor_Data[3].Error_system--;  */
					}
				}

				if(AC_measure_data.output_switch_on2 == 0)
				{//断路器开关状态
					if(Globa->Control_DC_Infor_Data[0].Error.AC_circuit_breaker_off  == 0)
					{////
						Globa->Control_DC_Infor_Data[0].Error.AC_circuit_breaker_off = 1;
						/* sent_warning_message(0x99, 14, 0);
						Globa->Control_DC_Infor_Data[0].Error_system++;
						Globa->Control_DC_Infor_Data[1].Error_system++;
						Globa->Control_DC_Infor_Data[2].Error_system++;
						Globa->Control_DC_Infor_Data[3].Error_system++; */
					}
				}else
				{
					if(Globa->Control_DC_Infor_Data[0].Error.AC_circuit_breaker_off == 1)
					{////
						Globa->Control_DC_Infor_Data[0].Error.AC_circuit_breaker_off = 0;
						/* sent_warning_message(0x98, 14, 0);
						Globa->Control_DC_Infor_Data[0].Error_system--;
						Globa->Control_DC_Infor_Data[1].Error_system--;
						Globa->Control_DC_Infor_Data[2].Error_system--;
						Globa->Control_DC_Infor_Data[3].Error_system--; */
					}
				}			
				AC_fault_Number = 0; 
				Globa->ac_col_rx_ok_cnt++;
			}else
			{
				AC_fault_Number++;
			}
		}else
		{
			AC_fault_Number++;
			printf("04 recv Count = %d,need %d\n",Count,(2*sendbuf[5]+5));
		}
					
		usleep(1000000);//sleep 1秒
		
		sendbuf[0] = 62;  //addr = 62
		sendbuf[1] = 0x05;  //功能码
		sendbuf[2] = 0x00;  //起始地址高字节
		sendbuf[3] = 0x00;  //起始地址低字节，起始地址0
		if(0xA0 == Jkac_con)
			sendbuf[4] = 0xFF;//闭合继电器
		else//断开
			sendbuf[4] = 0x00;
			
		sendbuf[5] = 0x00;  
		wCRCCheckValue = ModbusCRC(&sendbuf[0], 6);  //Modbus CRC
		sendbuf[6] = wCRCCheckValue>>8;
		sendbuf[7] = wCRCCheckValue&0x00ff;	
		
		Led_Relay_Control(DIR_Ctl_io, COM_RS485_TX_LEVEL);
		write(fd, sendbuf, 8);
		do{
			ioctl(fd, TIOCSERGETLSR, &status);
		} while (status!=TIOCSER_TEMT);
		usleep(5000);
		Led_Relay_Control(DIR_Ctl_io, COM_RS485_RX_LEVEL);
		Globa->ac_col_tx_cnt++;
		Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE-1, 2000000, 300000);
		if((Count == 8)&&(sendbuf[0] == Buf[0]))
		{//响应数据长度
			wCRCCheckValue = ModbusCRC(&Buf[0], Count-2);//校验CRC
			AC_fault_Number= 0; 
			Globa->ac_col_rx_ok_cnt++;
		}else
		{
			printf("05 recv Count = %d,need 8\n",Count);
		}
		
		if(AC_fault_Number >  500)
		{
			AC_fault_Number = 0;
			
			if(Globa->Control_DC_Infor_Data[0].Error.AC_connect_lost == 0)
			{
				Globa->Control_DC_Infor_Data[0].Error.AC_connect_lost = 1;
				sent_warning_message(0x99, 4, 0,0);
				for(i=0;i<gun_config;i++)
					Globa->Control_DC_Infor_Data[i].Error_system++;		
			}			
		}else if(AC_fault_Number == 0)
		{
			if(Globa->Control_DC_Infor_Data[0].Error.AC_connect_lost == 1)
			{
				Globa->Control_DC_Infor_Data[0].Error.AC_connect_lost = 0;
				sent_warning_message(0x98, 4, 0,0);
				for(i=0;i<gun_config;i++)
					Globa->Control_DC_Infor_Data[i].Error_system--;		
			}
		}
	}
	close(fd);
}

/*******************************************************************************
**description：电压电流表读线程初始化及建立
***parameter  :none
***return		  :none
*******************************************************************************/
extern void AC_Col_Interface_Thread(void* para)
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
	if(0 != pthread_create(&td1, &attr, (void *)AC_Col_Interface_Task, para)){
		perror("####pthread_create AC_Col_Interface_Task ERROR ####\n\n");
	}

	ret = pthread_attr_destroy(&attr);
	if(ret != 0)
		return;
}
