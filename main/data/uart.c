/*******************************************************************************
  Copyright(C)    2014-2015,      EAST Co.Ltd.
  File name :     uart.c
  Author :        dengjh
  Version:        1.0
  Date:           2014.4.11
  Description:    串口底层发送数据及接收数据模块
  Other:          
  Function List:  
  History:        
          <author>   <time>   <version>   <desc>
            dengjh    2014.4.11   1.0      Create
*******************************************************************************/  					
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>      /*标准输入输出定义*/
#include <stdlib.h>     /*标准函数库定义*/
#include <unistd.h>     /*Unix 标准函数定义*/
#include <sys/types.h>  /*数据类型，比如一些XXX_t的那种*/
#include <sys/stat.h>   /*定义了一些返回值的结构，没看明白*/
#include <fcntl.h>      /*文件控制定义*/
#include <termios.h>    /*PPSIX 终端控制定义*/
#include <errno.h>      /*错误号定义*/
#include <pthread.h>
#include <sys/time.h>
#include <math.h>
#include <sys/sysinfo.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <signal.h>

#include "globalvar.h"


#define  CMSPAR 010000000000
#define  DEBUG_RS232 0


/* 速度 */
static const UINT32 speed_arr[] = {B1200, B2400, B4800, B9600, B19200, B38400, B57600, B115200, B230400, B460800, B921600};
/* 串口速度 */
static const UINT32 name_arr[] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};


///////////////////////////////////////////////////////
//程序代码、函数功能说明、参数说明、及返回值
///////////////////////////////////////////////////////


/***************set series API***************************
***description：设置串口通信速率
***parameter   :fd       打开串口的文件句柄
***            :speed    串口速度
***return		   :none
**********************************************************/
void set_speed(INT32 fd, UINT32 speed)
{
  UINT32   i;
  INT32   status;
  struct termios   Opt;
	
  tcgetattr(fd, &Opt);
  for(i= 0; i < sizeof(speed_arr)/sizeof(UINT32); i++){
    if(speed == name_arr[i]){
    	    printf("name_arr %d\n", name_arr[i]);
      tcflush(fd, TCIOFLUSH);
      cfsetispeed(&Opt, speed_arr[i]);
      cfsetospeed(&Opt, speed_arr[i]);
      status = tcsetattr(fd, TCSANOW, &Opt);
      if(status != 0){
        perror("tcsetattr fd");
        return;
      }
      tcflush(fd,TCIOFLUSH);
    }
  }
}

/*************************************************************
**description：设置串口数据位，停止位和效验位
***parameter :fd     类型  int  打开的串口文件句柄
databits 类型  int 数据位   取值 为 7 或者8
stopbits 类型  int 停止位   取值为 1 或者2
parity  类型  int  效验类型 取值为N,E,O,S,M
**return		：none
**************************************************************/

int set_Parity(int fd,int databits,int stopbits,int parity)
{ 
	struct termios options; 
	if  ( tcgetattr( fd,&options)  !=  0) 
	{ 
    perror("SetupSerial 1");   
    return(FALSE);  
	}
  options.c_cflag &= ~CSIZE; 
  options.c_lflag  &= ~(ICANON | ECHO | ECHOE | ISIG);  /*Input*/
  options.c_oflag  &= ~OPOST;   /*Output*/
  options.c_cflag |= CLOCAL | CREAD;
	options.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  switch (databits) /*设置数据位数*/
  { 
  	case 5:                 
    options.c_cflag |= CS5; 
    break;
  	  
  	case 6:                 
    options.c_cflag |= CS6; 
    break;
  	
		case 7:                 
    options.c_cflag |= CS7; 
    break;

    case 8:     
    options.c_cflag |= CS8;
    break;   
         
    default:    
    printf("####Unsupported data size error####\n\n");
    return (FALSE);  

	}

	switch (parity) 
	{   
		//case 'n':
		//case 'N': 
		case 0:    
		options.c_cflag &= ~PARENB;   /* Clear parity enable */
		options.c_iflag &= ~INPCK;    /* Disnable parity checking */
    break;  
    
   // case 'o':   
   // case 'O': 
   	case 1:     
    options.c_cflag |= (PARODD | PARENB); /* 设置为奇效验*/  
		options.c_iflag |= INPCK;       /* Enable parity checking */
    break;  
    
   // case 'e':  
   // case 'E': 
   	case 2:   
    options.c_cflag |= PARENB;     /* Enable parity */    
    options.c_cflag &= ~PARODD;    /* 转换为偶效验*/     
		options.c_iflag |= INPCK;      /* Enable parity checking */
    break;
  
   // case 'S': 
   // case 's':  /*空格校验*/ 
   	case 3:
   	options.c_cflag |= PARENB | CS8 | CMSPAR;  
    break;  
		
		case 4://MARK校验
   	options.c_cflag |= PARENB|CS8|CMSPAR|PARODD; 
    break;
		
		default:       
		printf("####Unsupported parity error####\n\n");
    return (FALSE);  
	} 
/* 设置停止位*/  
	switch (stopbits)
	{   
		case 1:    
    options.c_cflag &= ~CSTOPB;  
    break;  
    
		case 2:    
    options.c_cflag |= CSTOPB;  
    break;

    default:    
    printf("####Unsupported stop bits error####\n\n"); 
    return (FALSE); 
	} 


	tcflush(fd,TCIFLUSH);
	options.c_cc[VTIME] = 0; /* 设置超时15 seconds*/   
	options.c_cc[VMIN] = 0; /* define the minimum bytes data to be readed*/

	if (tcsetattr(fd,TCSANOW,&options) != 0){ 
		perror("SetupSerial 3 error");  
    return (FALSE);  
	}

return (TRUE);
}

/*打开串口设备*/
int OpenDev(char *Dev) 
{  
  int fd = open(Dev, O_RDWR | O_NOCTTY|O_NDELAY );
  if(-1 == fd){
    perror("Can't Open Serial Port");
    return -1;
  }else{
    return fd;
  }
}

/*
**description：*select读主要实现的功能是，在一定时间内不停地
*看串口有没有数据，有数据则进行读，当时间过
*去后还没有数据，则返回超时错误，Linux下直接
*用read读串口可能会造成堵塞，或数据读出错误。
*然而用select先查询com口，再用read去读就可以避免，
*并且当com口延时时，程序可以退出，这样就不至
*于由于com口堵塞，程序就死了
***parameter :fd     类型  int  打开的串口文件句柄
							rcv_buf : 指向接收数据的指针地址
							Len:要返回的数据长度
							usec1  :设置无响应数据时的超时时间 微秒为单位
							usec2  :设置数据帧字符间的超时时间 微秒为单位
**return		：正常返回实际接收的数据长度
*/							
int read_datas_tty(int fd, unsigned char *rcv_buf, int Len, int usec1, int usec2)
{
	int retval;
	fd_set rfds;
	struct timeval tv;
	int ret,i;

	int count = Len;//rcv_buf 指向的剩余空间字节个数
	unsigned char *pbuf = rcv_buf;


	tv.tv_sec = 0;
	tv.tv_usec = usec1;//没有响应的超时时间

	while(1){
		FD_ZERO(&rfds);
		FD_SET(fd,&rfds);

		retval = select(fd+1, &rfds, NULL, NULL, &tv);
		if(retval == -1){
			perror("select()");
			break;
		}else if(retval){
			if(FD_ISSET(fd, &rfds)){//先判断一下这外被监视的句柄是否真的变成可读的了			
        ret= read(fd, pbuf, count);
        if(ret > 0){
        	count = count - ret;
        	pbuf = pbuf + ret;
          if(count == 0){
            break;
          }

					tv.tv_sec = 0;
					tv.tv_usec = usec2;//当收到数据后重新复位计时字符间超时时间
        }else{
          break;
        }
			}else{
				break;
			}
		}else{
			break;
		}
	}

#if DEBUG_RS232
   printf("Count=%d,Buf= ",Len - count);
   for(i=0; i<Len - count; i++ )printf(" %02x",rcv_buf[i]);
   printf("\n");
#endif

	return (Len - count);  
}
