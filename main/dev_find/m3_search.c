//*****************************************************************************
//M3设备的以太网discovery和配置协议应用
//*****************************************************************************
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
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

#include "../data/globalvar.h"
#include "../data/Time_Utity.h"
#include "macsearch_public.h"
#include "m3_search.h"
 

struct eth_device_config_hdr {
  struct uip_eth_hdr ethhdr;//dest mac and src mac and eth type 
  u16_t hwtype;
  u16_t protocol;
  u16_t opcode;  
  u8_t sipaddr[4];//本帧发送端的IPv4地址
  u8_t pad[1036];
};

struct eth_device_config_ack_frame {
  struct uip_eth_hdr ethhdr;//dest mac and src mac and eth type 
  u16_t hwtype;
  u16_t protocol;
  u16_t opcode;    
  u8_t pad[1036];
};


//处理接收到的M3设备的以太网discovery和配置帧,返回报文打包到tx_buf
//函数返回值大于0表示待发送数据长度
int eth_device_packetin(char* recv_buf,int rx_len,char* tx_buf)
{ 
    unsigned short i=0;
	int send_len = 0;
	PM3_DISCOVERY_RESP  ptr1;
	PM3_MAC_IP_SET_REQ  ptr2;
	PM3_MAC_IP_SET_RESP ptr3;
	
	PM3_SET_PARA_REQ    para_ptr;

	
	char ip_addr[20];
	char netmask_addr[20];
	char gateway_addr[20];
	char tmp[256];
	char macCfgPath[50]={0};
	

	snprintf(macCfgPath,sizeof(macCfgPath),"%s","/etc/rc.d/rc.hw");//286 mac设置路径
	
	 
	struct eth_device_config_ack_frame* send_buf = (struct eth_device_config_ack_frame *)tx_buf;
	struct eth_device_config_hdr*   rx_frame = (struct eth_device_config_hdr*)recv_buf;
	
    if(rx_len < ETH_MIN_LEN) 
	{
		rx_len = 0;
		return 0;
	}
	memcpy(send_buf,recv_buf,rx_len);
  
	switch(send_buf->opcode) 
	{
	  case HTONS(M3_DEVICE_DISCOVERY_REQ)://主机的DISCOVERY命令
	  {
			ptr1 = (PM3_DISCOVERY_RESP) (send_buf->pad);
			memset(ptr1,0,sizeof(M3_DISCOVERY_RESP));
			memcpy(send_buf->ethhdr.dest.addr, send_buf->ethhdr.src.addr, 6);//目的MAC为接收到的帧中的源MAC
			memcpy(send_buf->ethhdr.src.addr, NET_setting.mac_add, 6);//响应帧的源MAC为本机MAC
			
			send_buf->opcode = HTONS(M3_DEVICE_DISCOVERY_ACK);			
			
			get_ip(NET_setting.ethName,ptr1->ip_addr);//本机当前IP地址		  
			get_netmask(NET_setting.ethName,ptr1->net_mask);//本机当前子网掩码
			get_gateway(NET_setting.ethName,gateway_addr);//本机当前网关地址	
			ascii_to_number(ptr1->gate_way,gateway_addr);//"192.168.0,1"==>  192  168  0  1 
			
			if(0 == NET_setting.assign_method)//IP静态配置
				ptr1->dev_ip_method = 1;//静态配置
			else
				ptr1->dev_ip_method = 0;//dhcp
			ptr1->m3_eth_mode = M3_ETH_SPEED_AUTO;
			
			ptr1->dev_type[0] = 0x0A;
			ptr1->dev_type[1] = 0x01;
			int info_len = sizeof(M3_DISCOVERY_RESP) - 18;
			ptr1->dev_info_len[0] = info_len >> 8;//MSB first
			ptr1->dev_info_len[1] = info_len;
			
			strncpy(ptr1->dev_name,"常规8583协议多枪直流桩",32);
			snprintf(ptr1->dev_sw_ver,31,"%s %s %s",DEV_SW_VER,__DATE__,__TIME__);
			
			strncpy(ptr1->dev_manufacturer,"易事特集团中能易电新能源技术有限公司" ,64);
			strncpy(ptr1->dev_location,Globa->Charger_param.dev_location  ,64);			
			strncpy(ptr1->dev_user_info,Globa->Charger_param.dev_user_info  ,64);	
			
			time_t systime;
			struct tm tm_t;
			systime = time(NULL);         //获得系统的当前时间 
			localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
			snprintf( ptr1->dev_time, 31, "%04d-%02d-%02d %02d:%02d:%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
														   tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
			//填充设备序列号数据
			memcpy(ptr1->dev_serial_num, Globa->Charger_param.SN, 12);			
			
			
			//填充其他参数信息			
			ptr1->uart1_dcb.baud     = MCU_UART_BAUD2400;
			ptr1->uart1_dcb.bytesize = 8;
			ptr1->uart1_dcb.stopbits = 1;
			ptr1->uart1_dcb.parity   = M3_EVEN_PARITY;
			ptr1->uart1_addr = 1;
			
			ptr1->uart2_dcb.baud     = MCU_UART_BAUD2400;
			ptr1->uart2_dcb.bytesize = 8;
			ptr1->uart2_dcb.stopbits = 1;
			ptr1->uart2_dcb.parity   = M3_EVEN_PARITY;
			ptr1->uart2_addr = 1;
			
			ptr1->uart3_dcb.baud     = MCU_UART_BAUD115200;//读卡器
			ptr1->uart3_dcb.bytesize = 8;
			ptr1->uart3_dcb.stopbits = 1;
			ptr1->uart3_dcb.parity   = M3_NONE_PARITY;
			ptr1->uart3_addr = 1;
			
			ptr1->uart4_dcb.baud     = MCU_UART_BAUD9600;//DGUS
			ptr1->uart4_dcb.bytesize = 8;
			ptr1->uart4_dcb.stopbits = 1;
			ptr1->uart4_dcb.parity   = M3_NONE_PARITY;
			ptr1->uart4_addr = 1;
			
			ptr1->uart5_dcb.baud     = MCU_UART_BAUD9600;//地锁
			ptr1->uart5_dcb.bytesize = 8;
			ptr1->uart5_dcb.stopbits = 1;
			ptr1->uart5_dcb.parity   = M3_NONE_PARITY;
			ptr1->uart5_addr = 1;
			
			ptr1->can_bitrate[0] = 250;
			ptr1->can_bitrate[1] = 0;
			//////////////////////////////////////////////////////////////////////////////
			
			send_len = sizeof(struct uip_eth_hdr) + 6 + sizeof(M3_DISCOVERY_RESP);//tell uip how much data to send
		}

		break;
	  
	  case HTONS(M3_DEVICE_MAC_IP_SET_REQ)://主机的MAC和IP等设置命令
		{
			ptr3 = (PM3_MAC_IP_SET_RESP) (send_buf->pad);
			ptr2 = (PM3_MAC_IP_SET_REQ)(rx_frame->pad);
			
			memset(ptr3,0,sizeof(M3_MAC_IP_SET_RESP));
			memcpy(send_buf->ethhdr.dest.addr, send_buf->ethhdr.src.addr, 6);//目的MAC为接收到的帧中的源MAC
			memcpy(send_buf->ethhdr.src.addr, NET_setting.mac_add, 6);//响应帧的源MAC为本机MAC
			
			send_buf->opcode = HTONS(M3_DEVICE_MAC_IP_SET_ACK);	
			memcpy(ptr3->new_ip_addr,ptr2->new_ip_addr,4);
			memcpy(ptr3->new_net_mask,ptr2->new_net_mask,4);
			memcpy(ptr3->new_gate_way,ptr2->new_gate_way,4);
			ptr3->new_dev_ip_method = ptr2->new_dev_ip_method ;
			send_len = sizeof(struct uip_eth_hdr) + 6 + sizeof(M3_MAC_IP_SET_RESP);
						
			if(ptr2->new_dev_ip_method & 0x01)//设置静态IP时才更新FLASH中相应参数
			{	
				NET_setting.assign_method = 0;//静态设置IP
				snprintf(ip_addr,19,"%d.%d.%d.%d",ptr2->new_ip_addr[0],ptr2->new_ip_addr[1],ptr2->new_ip_addr[2],ptr2->new_ip_addr[3]);
				snprintf(netmask_addr,19,"%d.%d.%d.%d",ptr2->new_net_mask[0],ptr2->new_net_mask[1],ptr2->new_net_mask[2],ptr2->new_net_mask[3]);
				snprintf(gateway_addr,19,"%d.%d.%d.%d",ptr2->new_gate_way[0],ptr2->new_gate_way[1],ptr2->new_gate_way[2],ptr2->new_gate_way[3]);
				if( (if_a_string_is_a_valid_ipv4_address(  ip_addr ,1))&&
					(if_a_string_is_a_valid_ipv4_address(  gateway_addr ,1))&&
					(IsSubnetMask(  netmask_addr))
					)
					{
						cur_net_setting.dhcp = 0;
						netinfo->isdhcp = 0;
						cur_net_setting.ip_addr[0] = ptr2->new_ip_addr[0];//本机当前IP地址
						cur_net_setting.ip_addr[1] = ptr2->new_ip_addr[1];//本机当前IP地址
						cur_net_setting.ip_addr[2] = ptr2->new_ip_addr[2];//本机当前IP地址
						cur_net_setting.ip_addr[3] = ptr2->new_ip_addr[3];//本机当前IP地址						
					  
						cur_net_setting.net_mask[0] = ptr2->new_net_mask[0];//本机当前子网掩码
						cur_net_setting.net_mask[1] = ptr2->new_net_mask[1];//本机当前子网掩码
						cur_net_setting.net_mask[2] = ptr2->new_net_mask[2];//本机当前子网掩码
						cur_net_setting.net_mask[3] = ptr2->new_net_mask[3];//本机当前子网掩码
						
						cur_net_setting.gate_way[0] = ptr2->new_gate_way[0] ;//本机当前网关地址
						cur_net_setting.gate_way[1] = ptr2->new_gate_way[1] ;//本机当前网关地址
						cur_net_setting.gate_way[2] = ptr2->new_gate_way[2] ;//本机当前网关地址
						cur_net_setting.gate_way[3] = ptr2->new_gate_way[3] ;//本机当前网关地址
						
						strncpy(netinfo->IP_address,ip_addr,15);
						strncpy(netinfo->msk_address,netmask_addr,15);
						strncpy(netinfo->gate_address,gateway_addr,15);
						
						//System_param_save();
		
						//set ip addr and netmask
						UpdateEthPara();//更新当前系统使用的IP地址信息
					}			
			}
			else//上位机要求M3设备使用DHCP方式，则忽略接收到的静态IP信息!只更新MAC地址和网络配置方式
			{
				NET_setting.assign_method = 1;
				cur_net_setting.dhcp = 1;
				netinfo->isdhcp = 1;
				//System_param_save();			
				//set ip addr and netmask
				UpdateEthPara();//更新当前系统使用的IP地址信息
			}
			if( 0 == (ptr2->new_MAC[0] & 0x01 ))//MAC地址为单播地址
			{
				char mac[50]={0};
				
				snprintf((char *)mac,sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",ptr2->new_MAC[0],ptr2->new_MAC[1],ptr2->new_MAC[2],ptr2->new_MAC[3],ptr2->new_MAC[4],ptr2->new_MAC[5]);
				strcpy(netinfo->mac_address,mac);
				savenetinfo();
				snprintf((char *)tmp,sizeof(tmp), "echo export HW_FACER=%s>%s&&reboot",mac,macCfgPath);
				system((char *)tmp);//修改MAC后直接重启
				
			}
			
		}
		break;
		
	  case HTONS(M3_DEVICE_PARA_SET_REQ)://主机设置M3设备其他参数的命令
		{
			para_ptr = (PM3_SET_PARA_REQ)(rx_frame->pad);			
			memcpy(send_buf->ethhdr.dest.addr, send_buf->ethhdr.src.addr, 6);//目的MAC为接收到的帧中的源MAC
			memcpy(send_buf->ethhdr.src.addr, NET_setting.mac_add, 6);//响应帧的源MAC为本机MAC
			
			send_buf->opcode = HTONS(M3_DEVICE_PARA_SET_ACK);
			
			get_ip(NET_setting.ethName,send_buf->pad);//本机当前IP地址			
			
			{
				//memcpy(dev_info.dev_name,(const void*)para_ptr->dev_name,32);//从接收帧中提取设备名称
				//memcpy(dev_info.dev_sw_ver,(const void*)para_ptr->dev_sw_ver,32);//从接收帧中提取软件版本
				//memcpy(dev_info.dev_manufacturer,(const void*)para_ptr->dev_manufacturer,64);//从接收帧中提取制造商信息
				memcpy(Globa->Charger_param.dev_location,(const void*)para_ptr->dev_location,64);
				memcpy(Globa->Charger_param.dev_user_info,(const void*)para_ptr->dev_user_info,64);
				//memcpy(dev_info.dev_time,(const void*)para_ptr->dev_time,32);
				
				memcpy(Globa->Charger_param.SN,(const void*)para_ptr->dev_serial_num,12);
				
				//将所有参数写入FLASH
				System_param_save();
				
				BIN_TIME new_time;
				//将dev_info.dev_time的字符串时间转换为PCF8563的时间设置到芯片PCF8563中
				if(Str2Time(para_ptr->dev_time ,&new_time))//上位机设置的时间有效
					Set_RTC_Time(&new_time);
			}			
			send_len = sizeof(struct uip_eth_hdr) + 10 + sizeof(M3_SET_PARA_REQ);
			
		}
		break;
	  
	  
	  case HTONS(M3_DEVICE_RM_CTL_REQ)://主机远程控制命令
	  {	
			memcpy(send_buf->ethhdr.dest.addr, send_buf->ethhdr.src.addr, 6);//目的MAC为接收到的帧中的源MAC
			memcpy(send_buf->ethhdr.src.addr, NET_setting.mac_add, 6);//响应帧的源MAC为本机MAC			
		
			send_buf->opcode = HTONS(M3_DEVICE_RM_CTL_ACK);
			get_ip(NET_setting.ethName,send_buf->pad);//本机当前IP地址	
			
			switch(rx_frame->pad[0])//远程命令码
			{
				case M3_RESTART:
					Globa->sys_restart_flag = 0xBB;//界面空闲后重启		
					break;
					
				case M3_SHUTDOWN: 
					Globa->sys_restart_flag = 0xBB;//界面空闲后重启	
					break;
					
				default: //not support
					break;
			
			}
		
			send_len = 60;//notify uip to send data
	  }
		break;
		
	  default:
		break;
	}
	
	return send_len;
}


