#include<sys/types.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <iconv.h>
#include <sys/time.h>
#include <time.h>

#include "globalvar.h"
#include "qt.h"
#include "Time_Utity.h"
/*********设定全局变量使用的互斥锁*************************/

pthread_mutex_t *param_pmutex;

pthread_mutex_t busy_db_pmutex;
pthread_mutex_t mutex_log;
pthread_mutex_t modbus_pc_mutex;

UINT32 * process_dog_flag = NULL;
  
GLOBAVAL *Globa;

const char* g_sys_COM_name[]=
{
	"/dev/ttySP4",//COM1
	"/dev/ttyAM0",//COM2
	"/dev/ttySP2",//COM3
	"/dev/ttySP3",//COM4
	"/dev/ttySP1",//COM5
	"/dev/ttySP0",//COM6
};

/************library function API********************************/
const UINT8 auchCRCHi[] = {
0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 
0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 
0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 
0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 
0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 
0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 
0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 
0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 
0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 
0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 
0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 
0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 
0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 
0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 
0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 
0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 
0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 
0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 
0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 
0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 
0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 
0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 
0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 
0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 
0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 
0x80, 0x41, 0x00, 0xC1, 0x81, 0x40
}; 

const UINT8 auchCRCLo[] = {
0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 
0x07, 0xC7, 0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD, 
0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09, 
0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A, 
0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC, 0x14, 0xD4, 
0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3, 
0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 
0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4, 
0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A, 
0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29, 
0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED, 
0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26, 
0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 
0x61, 0xA1, 0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67, 
0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F, 
0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68, 
0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA, 0xBE, 0x7E, 
0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5, 
0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 
0x70, 0xB0, 0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92, 
0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C, 
0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B, 
0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B, 
0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C, 
0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 
0x43, 0x83, 0x41, 0x81, 0x80, 0x40
} ;


UINT16 ModbusCRC(UINT8 *puchMsg, UINT8 usDataLen)
{
	UINT16 uchCRCHi = 0xff;
	UINT16 uchCRCLo = 0xff;
	UINT16 uIndex ; // CRC循环中的索引

	while(usDataLen--){
		uIndex = uchCRCHi ^ *puchMsg++; // 计算CRC
		uchCRCHi = uchCRCLo ^ auchCRCHi[uIndex] ;
		uchCRCLo = auchCRCLo[uIndex] ;
	}

	return ((uchCRCHi << 8 )|uchCRCLo);
}

int ttyled_fd;

/*******************干接点动作*****************************/
int LedRelayControl_init()
{

     ttyled_fd=open("/dev/dm850_gpios",O_RDWR|O_SYNC);

     if(ttyled_fd==-1){
       printf("can't Open Led_Relay device\n");
       return -1;
     }else{
       printf("Open Led_Relay device  ok!\n");
       return(0);
     }
}
/*
下表为GPIO对应表，要控制哪个PIN仅需要调用函数ioctl(ttyled_fd,value,pin);  其中PIN 为下面对应的0---21, value为0，1。
"GPIO2_2",//run_led  0
"GPIO3_27",//fault_led  1
"GPIO3_21",//LED1_CTL  2
"GPIO3_20",//LED_E1_CTL  3
"GPIO2_21",//LED2_CTL   4
"GPIO2_20",//LED_E2_CTL  5
"GPIO3_2", //R/D_CTL1  6
"GPIO3_3", //R/D_CTL2  7
"GPIO0_21", //R/D_CTL3  8
 
"GPIO0_17", //R/D_CTL4   9
"GPIO2_0",//CS  10
"GPIO2_10", //clk  11
"GPIO2_8", //data  12
"GPIO3_28",//RLY1_CTL  13
"GPIO2_9",//RLY2_CTL 14
"GPIO2_7",//RLY3_CTL 15
"GPIO2_6",//RLY4_CTL 16
 
"GPIO2_5",//RLY5_CTL 17
"GPIO2_4",//RLY6_CTL 18
"GPIO2_3",//RLY7_CTL 19
"GPIO3_29",//RLY8_CTL 20
"GPIO2_1",//RESET  21
*/

int Led_Relay_Control(int pin,int value)
{
    int ret;
    if(((pin >= 0) && (pin < 22))&&((value==0)||(value==1))){
      ret = ioctl(ttyled_fd,value,pin);
      if(ret){
//        printf("ioctl DRIVER_GPIO_IOCTL\n");
        return(-1);
      }
    }else{
      printf("error LED gpio %d value %d\n",pin,value);
      return -1;
    }
    return(0);
}


/**********read IP broadcoast netmask gateway API*********************/
// int sGet_Net_Mask(unsigned char *netMask)
// {
	// int sock;
  // struct sockaddr_in sin,*ip;
  // struct ifreq ifr;
  
	// sock = socket(AF_INET, SOCK_DGRAM, 0);
	// if (sock == -1)
	// {
		// perror("socket");
		// return -1;      
	// }
	// strncpy(ifr.ifr_name, MAC_INTERFACE0, IFNAMSIZ);/*这个是网卡的标识符*/
	// ifr.ifr_name[IFNAMSIZ - 1] = 0;
	// if (ioctl(sock, SIOCGIFNETMASK, &ifr)< 0)
	// {
		// perror("ioctl");
		// close(sock); 
		// return -1;
	// }
	// memcpy(&sin, &ifr.ifr_addr, sizeof(sin));  
   
// /* mask[0] = (char)(sin.sin_addr.s_addr >> 0  );
   // mask[1] = (char)(sin.sin_addr.s_addr >> 8  );
   // mask[2] = (char)(sin.sin_addr.s_addr >> 16 );
   // mask[3] = (char)(sin.sin_addr.s_addr >> 24 );*/
	// sprintf(netMask, "%.15s",inet_ntoa(sin.sin_addr));
	// //printf("ZWYM = %s\n",netMask);
	// close(sock); 
  // return 0;
// }
// int sGet_IP_Address(unsigned char *IPAddress)
// {
  // int sock;
  // struct sockaddr_in sin,*ip;
  // struct ifreq ifr;
   
	// sock = socket(AF_INET, SOCK_DGRAM, 0);
	// if (sock == -1)
	// {
		// perror("socket");
		// return -1;      
	// }
   
	// strncpy(ifr.ifr_name, MAC_INTERFACE0, IFNAMSIZ);/*这个是网卡的标识符*/

	// ifr.ifr_name[IFNAMSIZ - 1] = 0;
   
	// if (ioctl(sock, SIOCGIFADDR, &ifr) < 0)
	// {
		// perror("ioctl");
		// close(sock); //cwenfu 20120410
		// return -1;
	// }
	// memcpy(&sin, &ifr.ifr_addr, sizeof(sin)); 

	// sprintf(IPAddress, "%.15s",inet_ntoa(sin.sin_addr));
	// //printf("IP Address = %s\n",IPAddress);    
	// close(sock);
  // return 0;
// }
// int sGet_GateWay(unsigned char *gateway)
// {
	// FILE *fp;
	// char buf[512];
	// char iface[16];
	// unsigned long dest_addr, gate_addr;
   
	// fp = fopen("/proc/net/route", "r");
	// if (fp == NULL)
		// return -1;
		// /* Skip title line */
		// fgets(buf, sizeof(buf), fp);
		// while(fgets(buf, sizeof(buf), fp)) {
			// if(sscanf(buf, "%s\t%lX\t%lX", iface, &dest_addr, &gate_addr) != 3 || dest_addr != 0){    
				// continue;
			// }

			// sprintf(gateway, "%d.%d.%d.%d",(gate_addr >> 0)&0x000000ff, (gate_addr >> 8)&0x000000ff, (gate_addr >> 16)&0x000000ff, (gate_addr >> 24 )&0x000000ff); 
			// //printf("sxxxxxxxxxxxxxxxxxxxxxxxxxxGet_GateWay = %s\n",gateway);  
			// break;
		// }    

	// fclose(fp);    
	// return 0;
// }

int sGet_DNS(unsigned char *dns1, unsigned char *dns2)
{
	FILE *fp;
	char buf[100];
	char iface[20];
	unsigned long dest_addr, gate_addr;
   
	fp = fopen("/etc/resolv.conf", "r");
	if (fp == NULL)
		return -1;

	if(NULL != fgets(buf,sizeof(buf),fp)){
		if(sscanf(buf, "%s\t%s", iface, dns1) != 2){    
			*dns1 = 0;
		}
	}

	if(NULL != fgets(buf,sizeof(buf),fp)){
		if(sscanf(buf, "%s\t%s", iface, dns2) != 2){    
			*dns2 = 0;
		}
	}

	fclose(fp);    
	return 0;
}

//发送告警消息到QT界面程序
void sent_warning_message(UINT32 type, UINT32 warn_sn, UINT32 row, UINT32 sn)
{
	CHARGER_MSG msg;

	msg.mtype = type;
	msg.mtext[0] = warn_sn;
	msg.mtext[1] = row;
	msg.mtext[2] = sn;
	msgsnd(Globa->arm_to_qt_msg_id, &msg, 3, IPC_NOWAIT);
}


//bcd多字节加法
unsigned char bcd_addM(unsigned char *psoure,unsigned char *pdest,unsigned char ucnByte)
{
	   unsigned char ucTmpCn=0;
	   unsigned char ucTmpOne,ucTmpTwo,ucTmpSum,ucTmpData=0;
	   
	    while(ucnByte--)
	    {
	    	//ucTmpCn=ucnByte;
	    	ucTmpOne=*(psoure+ucTmpCn);
	    	ucTmpTwo=*(pdest+ucTmpCn);
	    	ucTmpSum=(ucTmpOne&0x0f)+(ucTmpTwo&0x0f)+ucTmpData;
	    	ucTmpData=((ucTmpOne&0xf0)>>4)+((ucTmpTwo&0xf0)>>4);
	    	if(ucTmpSum>9)
	    		{
	    			ucTmpSum+=6;
	    			ucTmpData++;
	    		}
	    	if(ucTmpData>9)
	    		{
	    			ucTmpData+=6;
	    		}
	    	*(psoure+ucTmpCn)=(ucTmpData<<4)+(ucTmpSum&0x0f);
	    	ucTmpData>>=4;
	    	ucTmpCn++;
	    }
	return ucTmpData;
}
//bcd多字节减法
unsigned char bcd_subM(unsigned char *pucSubV,unsigned char *pucSubedV,unsigned char ucTmpLen)
{ 
	unsigned char ucX,ucY,ucV,ucC=0,i=0;
	if(ucTmpLen>0){
	  ucC=0;
	  do{
	    ucTmpLen--;
	    ucX=(pucSubV[i]&0x0f);
	    ucY=(pucSubedV[i]&0x0f)+ucC;
	    if(ucX<ucY){
	    	ucC=1;
	    	ucV=ucX+10-ucY;
	    }
	    else{
	    	ucC=0;
	    	ucV=ucX-ucY;
	    }
	    ucX=(pucSubV[i]&0x0f0);
	    ucY=(pucSubedV[i]&0x0f0)+(ucC<<4);
	    if(ucX<ucY){
	    	ucC=1;
	    	ucV+=ucX+(0xa0-ucY);
	    }
	    else{
	    	ucC=0;
	    	ucV+=ucX-ucY;
	    }
	    pucSubV[i]=ucV;
	    i++;
	  }while(ucTmpLen);
	}
	return(ucC);
}
 
//单字节的BCD转十进制
unsigned char bcd_to_int_1byte(unsigned char pucTmpV)
{
	unsigned char ucxx,ucyy,rucV;
	
	if((pucTmpV >0x99)||((pucTmpV&0x0f) >0x09)){
	  return(0);
	}
  
  ucxx = pucTmpV/16;
	ucyy = pucTmpV%16;	
	rucV = ucxx*10 + ucyy ;
	return(rucV);  
}

//单字节的十进制转BCD
unsigned char int_to_bcd_1byte(unsigned char pucTmpV)
{
	unsigned char ucxx,ucyy,rucV;
	
	if(pucTmpV > 99){
	  return(0);
	}
	ucxx = pucTmpV/10;
	ucyy = pucTmpV%10;	
	rucV = ucxx*16 + ucyy ;
	return(rucV);  
}

/* 多字节BCD码转字符串********************************************************** 
输入:
unsigned char *str: 转换后的存储位置
unsigned char *bcd: 被转换的BCD码位置
unsigned int len:   被转换的BCD码字节长度

说明：str指向的空间需是 len的两倍，因一个BCD码转换后是两个字节
*******************************************************************************/
unsigned char bcd_to_str(unsigned char *str, unsigned char *bcd, unsigned int len)
{
	unsigned int i;

	for(i = 0; i < len; i++){
		if((bcd[i] >0x99)||((bcd[i]&0x0f) >0x09)){
	  	return(0);
		}
		
		str[2*i]   = 0x30 + (bcd[i]/16);
		str[2*i+1] = 0x30 + (bcd[i]%16);
	}

	return(1);  
}

/* 多字节十六进制数转字符串******************************************************* 
输入:
unsigned char *str: 转换后的存储位置
unsigned char *hex: 被转换的十六进制数位置
unsigned int len:   被转换的十六进制数字节长度

说明：str指向的空间需是 len的两倍，因一字节十六进制数转换后是两个字节
*******************************************************************************/
unsigned char hex_to_str(unsigned char *str, unsigned char *hex, unsigned int len)
{
	unsigned int i;
	unsigned char high,low;

	for(i = 0; i < len; i++){
		high = hex[i] >> 4;
		low  = hex[i] & 0x0F;

		str[2*i]   = (high <= 9)? (high + 0x30):(high + 0x37);
		str[2*i+1] = (low <= 9)? (low + 0x30):(low + 0x37);
	}

	return(1);  
}

/* 多字节字符串转十六进制数******************************************************* 
输入:
unsigned char *hex: 转换后的存储位置
unsigned char *str: 被转换的字符串
unsigned int len:   被转换的字符串字节长度

说明：hex指向的空间需是 len的一半，因转换后是两个字节合并为一字节
*******************************************************************************/
unsigned char str_to_hex(unsigned char *hex, unsigned char *str, unsigned int len)
{
	unsigned int i;
	unsigned char high,low;

	for(i = 0; i < len/2; i++){
		high = (str[2*i] > 0x39)? (str[2*i] - 0x37):(str[2*i] - 0x30);
		low  = (str[2*i+1] > 0x39)? (str[2*i+1] - 0x37):(str[2*i+1] - 0x30);

		hex[i] = (high<<4) + (low&0x0F);
	}

	return(1);  
}

/* 两字节字符串转十六进制数******************************************************* 
输入:
unsigned char *hex: 转换后的存储位置
unsigned char *str: 被转换的字符串
unsigned int len:   被转换的字符串字节长度

说明：hex指向的空间需是 len的一半，因转换后是两个字节合并为一字节
*******************************************************************************/
unsigned char str2_to_BCD(unsigned char *str)
{
	unsigned int i;
	unsigned char high,low;
	unsigned char hex;
	high = (str[0] > 0x39)? (str[0] - 0x37):(str[0] - 0x30);
	low  = (str[1] > 0x39)? (str[1] - 0x37):(str[1] - 0x30);
	hex = (high<<4) + (low&0x0F);

	return(hex);  
}
/**********read nrtmask number*********************/

void my_strncpy(char* dst,const char* src,unsigned int dst_len)
{
	strncpy(dst,src,dst_len);//会强制写dst_len个字符到dst,如果src长度小于dst_len，则在dst后续字节处填0，如果长度相等则没有结束符了，必须强制不上
	dst[dst_len-1] = 0;//确保字符串有结束符	
}

// unsigned long get_ticks_elapsed(time_t pre_time)
// {
	// time_t cur_time;
	// time(&cur_time);
	
	// return abs(cur_time - pre_time);	
// }

unsigned long GetTickCount(void)//返回系统开机以来的秒数----
{
	struct timespec   ts;
	
	clock_gettime(CLOCK_MONOTONIC, &ts);
	
	return( ts.tv_sec );
	
}


void GetCurTime(BIN_TIME* cur_time)
{
	time_t systime=0;
    struct tm tm_t;
	systime = time(NULL);         //获得系统的当前时间
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间
	
	cur_time->year = tm_t.tm_year+1900-YEAR_OFFSET;
	cur_time->month = tm_t.tm_mon+1;
	cur_time->day_of_month = tm_t.tm_mday;
	cur_time->hour = tm_t.tm_hour;
	cur_time->minute = tm_t.tm_min;
	cur_time->second = tm_t.tm_sec;	
}

void Set_RTC_Time(BIN_TIME* new_time)
{
	char timestr[32];//2018-04-12 18:18:18
	char shell_cmd[256];
	unsigned short year;
	year = new_time->year+YEAR_OFFSET;
	
	sprintf(timestr,"%04d-%02d-%02d %02d:%02d:%02d",year,new_time->month,new_time->day_of_month,new_time->hour,new_time->minute,new_time->second);
	sprintf(shell_cmd, "date -s \"%s\"", timestr);
	system(shell_cmd);                
	system("hwclock --systohc --utc");
}
/**
******************************************************************************
* @brief       生成本地订单号
* @param[in]    char *DevSn  			设备地址
* @param[in]    unsigned char gunID		枪号
* @param[in]    unsigned char parallelFlag	并机标记（0-单枪，1-并机汇总，2-并机单枪）
* @param[in]    unsigned short maxLen	本地订单号接收缓存区最大长度（防止溢出）
* @param[out]   char *localSn			本地订单号
* @retval       -1 ：参数不合法    0-成功
* @details     1. 根据设备名查找消息队列头
* @note        NONE
******************************************************************************
*/
int Make_Local_SN(char *DevSn, unsigned char gunID, unsigned char parallelFlag, char *localSn, unsigned short maxLen)
{
    time_t systime;
    struct tm tm_t;

    if (NULL == DevSn || NULL == localSn || 0 == maxLen)
    {
        return -1;
    } 

    systime = time(NULL);         //获得系统的当前时间 
    localtime_r(&systime, &tm_t);  //结构指针指向当前时间

    snprintf(localSn, maxLen, "%.16s%02d%02d%02d%02d%02d%02d%02d%02d", DevSn, gunID,
        parallelFlag, tm_t.tm_year + 1900 - 2000, tm_t.tm_mon + 1, tm_t.tm_mday,
        tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);

    return 0;
}





