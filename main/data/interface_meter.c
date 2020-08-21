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

#include "globalvar.h"

#define  DEBUG_RS232_PC   0

extern UINT32 PC_Watchdog;

typedef enum _MODBUS_FRAME_ENUM{
  fun03_addr,
  fun03_fun,
  fun03_regh,
  fun03_regl,
  fun03_lenh,
  fun03_lenl,
  fun03_crch,
  fun03_crcl
}MODBUS_FRAME_ENUM;


void UART_PC_write_data(int fd, UINT8 *send_buf, UINT32 len)
{
	int status;

//	Led_Relay_Control(6, 0);

	write(fd, send_buf, len);
	
//	write(fd, send_buf, len+1);

//	do{
//		ioctl(fd, TIOCSERGETLSR, &status);
//	} while (status!=TIOCSER_TEMT);

//	Led_Relay_Control(6, 1);
}

/******************************************************************************/
/*             收到上位机发送的modbus协议命令 之后反馈出去的数据              */
/*             响应 04 功能码数据                                             */
/******************************************************************************/
void modbus_func04_ack(INT32 fd, UINT16 start_reg_addr, UINT16 reg_num)
{
  UINT32 i,j;
  UINT16 cur_reg_addr;
  UINT16 wCRCCheckValue;
  UINT8  send_buf[MAX_MODBUS_FRAMESIZE];   //响应上位机的MODBUS数据发送缓冲区
  UINT32 index = 0;

  send_buf[index++] = 0x01;//本机MODBUS地址
  send_buf[index++] = 0x03;//功能码
  send_buf[index++] = reg_num*2 ;  //1个地址有2个字节数据,必须<=254,也即最大1次访问127个寄存器地址
 
  //填充实际数据
  for(j=0; j < reg_num; j++){
    cur_reg_addr = start_reg_addr + j;

    send_buf[index++] = Globa->modbus_485_PC04[cur_reg_addr] >> 8;    //先传高字节,注意数组不要越界
    send_buf[index++] = Globa->modbus_485_PC04[cur_reg_addr];         //再传低字节
  }

  //计算CRC
   wCRCCheckValue = ModbusCRC(send_buf, reg_num*2+3);  //Modbus 协议crc校验
   send_buf[index++] = wCRCCheckValue >> 8;  //crc low byte
   send_buf[index++] = wCRCCheckValue & 0x00ff;//crc high byte

   UART_PC_write_data(fd, send_buf, index);
}

/*******************************************************************/
//            收到上位机发送的modbus协议命令 之后反馈出去的数据
//            响应2功能码数据
/********************************************************************/
void modbus_func02_ack(INT32 fd, UINT16 start_reg_addr, UINT16 reg_num)
{
  UINT32   bytes_cnt;
  UINT32   j,i;
  UINT32   index = 0;
  UINT32   registers_length;
  UINT16 wCRCCheckValue;
  UINT8  send_buf[MAX_MODBUS_FRAMESIZE];   //响应上位机的MODBUS数据发送缓冲区
	UINT32 *pdata = &Globa->Control_DC_Infor_Data[0].Error.voltage_connect_lost;

  send_buf[index++] = 0X01;//本机MODBUS地址
  send_buf[index++] = 0x02;//func code

  if(reg_num <= 8){//离散量数不超过1个字节的8个bit,1个字节即可表示
    bytes_cnt = 1;//1个字节
  }else{
    bytes_cnt = (reg_num%8 == 0) ? (reg_num >> 3): ((reg_num >> 3) + 1);
  }

  send_buf[index++] = bytes_cnt;//返回离散量状态所需的字节数,未使用的高bit为0

	registers_length = 0;
  for(j=0; j < bytes_cnt; j++){
    send_buf[index] = 0 ;
    for(i=0; i < 8; i++){
      send_buf[index] |= ((pdata[start_reg_addr + registers_length] & 0x01)<< i);
      registers_length++;
      if(registers_length == reg_num){
        break;
      }
    }

    index++;
  }

  //计算CRC
  wCRCCheckValue = ModbusCRC(send_buf, 3+bytes_cnt);
  send_buf[index++] = wCRCCheckValue >> 8; //crc low byte
  send_buf[index++] = wCRCCheckValue & 0x00ff;//crc high byte
  UART_PC_write_data(fd, send_buf, index);
}

/******************************************************************************/
/*             收到上位机发送的modbus协议命令  06设置功能码                   */
/*             响应 06 功能码数据                                             */
/******************************************************************************/
void modbus_func06_ack(INT32 fd, UINT16 start_reg_addr, UINT16 pata)
{
	UINT16  wCRCCheckValue;
	UINT8  send_buf[MAX_MODBUS_FRAMESIZE];   //响应上位机的MODBUS数据发送缓冲区

	send_buf[0] = 0x01; //本机MODBUS地址
	send_buf[1] = 0x06;
	send_buf[2] = start_reg_addr/256;
	send_buf[3] = start_reg_addr%256;
	send_buf[4] = pata/256;
	send_buf[5] = pata%256;

	wCRCCheckValue = ModbusCRC(send_buf, 6);  //Modbus 协议crc校验
	send_buf[6] = wCRCCheckValue >> 8;//crc low byte
	send_buf[7] = wCRCCheckValue & 0x00ff ;//crc high byte

	UART_PC_write_data(fd, send_buf, 8) ; //
}
/********************** 接收到异常的MODBUS指令时向上位机报告异常 **************/
void modbus_fault_ack(INT32 fd, UINT8 err_function_code ,UINT8 err_Correct_code)
{
  UINT16  wCRCCheckValue;
  UINT8  send_buf[MAX_MODBUS_FRAMESIZE];   //响应上位机的MODBUS数据发送缓冲区

  send_buf[0] = 0x01; //本机MODBUS地址
  send_buf[1] = err_function_code;
  send_buf[2] = err_Correct_code;  //异常代码 02 非法数据值

  wCRCCheckValue = ModbusCRC(send_buf, 3);  //Modbus 协议crc校验
  send_buf[3] = wCRCCheckValue >> 8;//crc low byte
  send_buf[4] = wCRCCheckValue & 0x00ff ;//crc high byte

  UART_PC_write_data(fd, send_buf, 5) ; //
}

static void PC_Task()
{
	int fd;
  int	i,ret;
  UINT8	buffer[MAX_MODBUS_FRAMESIZE];
  UINT16   start_reg,reg_num;
	UINT16 wCRCCheckValue = 0;
  UART_M4_MSG msg;
  UINT32 loop = 0;//循环计数值， 作为数据抄收和处理的时间基准
  UINT16 start_addr=0,start_pada=0;

	fd = OpenDev(PC_COM_ID);
	if(fd == -1) {
		while(1);
	}else{
		set_speed(fd, 9600);
		set_Parity(fd, 8, 1, 0);
		close(fd);//此处必须先关闭，后面在打开，不然设置无效
  }
	fd = OpenDev(PC_COM_ID);//需重新打开
	if(fd == -1) {
		while(1);
	}

//	Led_Relay_Control(6, 1);

  while(1){
    PC_Watchdog = 1;//喂软件狗

    ret=read_datas_tty(fd, buffer, MAX_MODBUS_FRAMESIZE, 20000, 20000);//

#if DEBUG_RS232_PC
    if(ret>0){
      printf("____ UART1 receive from pc =%d\n ", ret);
      for(i=0; i<ret; i++){
        printf("%02x ", buffer[i]);
      }
       printf("\n");
    }
#endif

    if(ret > 5){
      if(buffer[0] == 1){
        UINT16 wCRCCheckValue = 0;
        wCRCCheckValue = ModbusCRC(&buffer[0], ret-2);//校验CRC
        if(((wCRCCheckValue >> 8) == buffer[ret-2])&&((wCRCCheckValue & 0x00ff) == buffer[ret-1])){
          if(buffer[1] == 0x03){
          	//更新数据
						Globa->modbus_485_PC04[0] = Globa->Control_DC_Infor_Data[0].Output_voltage/100;
						Globa->modbus_485_PC04[1] = Globa->Control_DC_Infor_Data[0].Output_current/100;
						Globa->modbus_485_PC04[2] = Globa->Control_DC_Infor_Data[0].Time;
						Globa->modbus_485_PC04[3] = Globa->Control_DC_Infor_Data[0].kwh/METER_READ_MULTIPLE;
						Globa->modbus_485_PC04[4] = Globa->Control_DC_Infor_Data[0].need_voltage/100;
						Globa->modbus_485_PC04[5] = Globa->Control_DC_Infor_Data[0].need_current/100;
						Globa->modbus_485_PC04[6] = 0;
						Globa->modbus_485_PC04[7] = 0;
						Globa->modbus_485_PC04[8] = 0;
						Globa->modbus_485_PC04[9] = Globa->Control_DC_Infor_Data[0].BMS_batt_SOC;
						Globa->modbus_485_PC04[10] = Globa->Control_DC_Infor_Data[0].BMS_need_time;
						Globa->modbus_485_PC04[11] = 99;
						Globa->modbus_485_PC04[12] = 99;
						Globa->modbus_485_PC04[14] = 1;
						Globa->modbus_485_PC04[15] = (Globa->Control_DC_Infor_Data[0].charger_start != 0)?1:0;

            start_reg = (buffer[2] << 8) + buffer[3];
            reg_num = (buffer[4]<<8) + buffer[5];//
            if((start_reg > (MODBUS_PC04_MAX_NUM-1))||((start_reg + reg_num) > MODBUS_PC04_MAX_NUM)){  //寄存器访问越界
              if(start_reg > (MODBUS_PC04_MAX_NUM-1)){
                modbus_fault_ack(fd, 0x84, 0x02);    //报告异常代码_无效地址
              }else{
                modbus_fault_ack(fd, 0x84,0x03);//报告异常代码无效数据
              }
            }else{//正常响应
              modbus_func04_ack(fd, start_reg, reg_num);
            }
          }else if(buffer[1] == 0x02){
          	//更新数据
            start_reg = (buffer[2] << 8) + buffer[3];
            reg_num = (buffer[4]<<8) + buffer[5];//
            if((start_reg > (MODBUS_PC02_MAX_NUM-1))||((start_reg + reg_num) > MODBUS_PC02_MAX_NUM)){  //寄存器访问越界
              if(start_reg > (MODBUS_PC02_MAX_NUM-1)){
                modbus_fault_ack(fd, 0x82, 0x02);    //报告异常代码_无效地址
              }else{
                modbus_fault_ack(fd, 0x82, 0x03);//报告异常代码无效数据
              }
            }else{//正常响应
              modbus_func02_ack(fd, start_reg, reg_num);
            }
          }else if(buffer[1] == 0x06){
          	//更新数据
            start_reg = (buffer[2] << 8) + buffer[3];
            reg_num = (buffer[4]<<8) + buffer[5];//
            if(start_reg == 0x0f){  //控制开关机
              if(reg_num  == 1){
								Globa->Control_DC_Infor_Data[0].Manu_start = 2;
								Globa->Control_DC_Infor_Data[0].charger_start = 2;								
								printf("远程控制手动开机 start 2\n");
              }else{
								Globa->Control_DC_Infor_Data[0].Manu_start = 0;
              }

  						modbus_func06_ack(fd, start_reg, reg_num);
            }else if(start_reg == 0x04){//设置电压
              Globa->Charger_param.set_voltage = reg_num*100;
  						System_param_save();//保存系统参数

  						modbus_func06_ack(fd, start_reg, reg_num);
            }else if(start_reg == 0x05){//设置电流
							Globa->Charger_param.set_current = reg_num*100;
  						System_param_save();//保存系统参数

  						modbus_func06_ack(fd, start_reg, reg_num);
            }else if(start_reg == 0x0e){//设置电压电流调节使能
  						modbus_func06_ack(fd, start_reg, reg_num);
            }else{
            	modbus_fault_ack(fd, 0x84, 0x02);    //报告异常代码_无效地址
            }
          }else{
            modbus_fault_ack(fd, (0x80 + buffer[1]), 0x01);//报告异常代码无效数据
          }
        }
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
extern void PC_Thread(void)
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
	if(0 != pthread_create(&td1, &attr, (void *)PC_Task, NULL)){
		perror("####pthread_create IC_Read_Write_Task ERROR ####\n\n");
	}

	ret = pthread_attr_destroy(&attr);
	if(ret != 0)
		return;
}