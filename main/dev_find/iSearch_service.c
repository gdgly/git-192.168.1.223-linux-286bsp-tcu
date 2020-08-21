//*****************************************************************************
//实现对PC机iSearch软件的搜索和设置响应。
//*****************************************************************************
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

#include "../data/globalvar.h"
#include "macsearch_public.h"
#include "iSearch_service.h"
 

typedef struct 
{
	u16_t reserved1;//硬件地址长度和协议地址长度(共2字节，全0)
	u16_t ack_packet;//2字节，固定为0x0002,抓包发现别的设备响应为全0也可以
	u8_t  reserved2[14];//14字节保留全0
	u8_t  dev_password[16];//16字节设备密码,ASCII码
	u8_t  dev_hw_ver[16];//16字节设备硬件版本,ASCII码
	u8_t  dev_sw_ver[16];//16字节设备软件版本,ASCII码
	u8_t  dev_serial_num[20];//20字节设备序列号,ASCII码
	u8_t  dev_http_port[2];//2字节设备WEB端口号，十六进制，低地址存放高字节
	u8_t  dev_telnet_port[2];//2字节设备telnet端口号，十六进制，低地址存放高字节
	u8_t  dev_protocol_flag;//值0xFF表示是iSearch的新协议
	u8_t  dev_ip_method;//IP地址获取方式，如果动态获取则填充0x55,如果静态配置则填充0xFF
	u8_t  ip_addr2[4];//IP地址2，保留，默认全0
	u8_t  ip_addr3[4];//IP地址3，设备主IP地址
	u8_t  net_mask[4];//子网掩码
	u8_t  gate_way[4];//网关地址	
}ISEARCH_DISCOVERY_RESP,*PISEARCH_DISCOVERY_RESP;//iSearch搜索帧的响应帧结构

typedef struct 
{	
	u8_t  dev_password[16];//16字节设备密码，发现iSearch发送过来为全0
	u8_t  set_dev_password[16];//设置16字节设备密码
	u8_t  reserved1[14];//14字节保留全0
	u8_t  gate_way[4];//设置设备的网关地址
	u8_t  ip_addr2[4];//IP地址获取方式，4字节全为xFF表示使用静态IP，否则使用DHCP方式
	u8_t  ip_addr3[4];//设置设备的IP地址
	u8_t  net_mask[4];//设置设备的子网掩码	
	u8_t  dev_http_port[2];//2字节设备WEB端口号，十六进制，低地址存放高字节
	u8_t  dev_telnet_port[2];//2字节设备telnet端口号，十六进制，低地址存放高字节
	u8_t  reserved2[39];
}ISEARCH_SETIP_CMD,*PISEARCH_SETIP_CMD;//iSearch设置IP地址信息的帧结构

typedef struct 
{	
	u8_t  pad[4];//4字节保留
	u8_t  set_dev_mac_addr[18];//设置设备MAC地址,ASCII码，发现iSearch发送过来为全0，界面没有接口设置MAC---30:89:99:12:34:56
	u8_t  set_dev_serial_num[20];//20字节的设备序列号	
	u8_t  reserved[10];
}ISEARCH_SETMAC_CMD,*PISEARCH_SETMAC_CMD;//iSearch设置MAC地址的帧结构

typedef struct 
{	
	u8_t  pad[4];//4字节保留
	u8_t  set_dev_mac_addr[18];//设置设备MAC地址,ASCII码，发现iSearch发送过来为全0，界面没有接口设置MAC---30:89:99:12:34:56
	u8_t  set_dev_serial_num[20];//20字节的设备序列号	
	u8_t  reserved[10];
}ISEARCH_SETSN_CMD,*PISEARCH_SETSN_CMD;//iSearch设置序列号帧结构



//处理接收到的iSearch软件的以太网discovery和配置帧,返回报文打包到tx_buf
//函数返回值大于0表示待发送数据长度
int iSearch_Packetin(char* recv_buf,int rx_len,char* tx_buf)
{ 	
	int i;
	PISEARCH_DISCOVERY_RESP  ptr1;
	PISEARCH_SETIP_CMD       ptr2;	
	PISEARCH_SETMAC_CMD      ptr3;
	PISEARCH_SETSN_CMD       ptr4;
	
	char macCfgPath[50]={0};
	
	snprintf(macCfgPath,sizeof(macCfgPath),"%s","/etc/rc.d/rc.hw");//286 mac设置路径
	
	struct iSearch_config_hdr *send_buf = (struct iSearch_config_hdr *)tx_buf;
	int send_len = 0;
	
	char ip_addr[20];
	char netmask_addr[20];
	char gateway_addr[20];//"192.168.0.1"
	char tmp[256];
	
    if(rx_len < ETH_MIN_LEN) 
	{
		return 0;
	}
	memcpy(send_buf,recv_buf,rx_len);
  
	switch(send_buf->protocol) 
	{
	  case HTONS(ISEARCH_DEVICE_DISCOVERY_REQ)://主机的DISCOVERY命令
	  {
			ptr1 = (PISEARCH_DISCOVERY_RESP) (send_buf->pad);
			memset(ptr1,0,sizeof(ISEARCH_DISCOVERY_RESP));
			memcpy(send_buf->ethhdr.dest.addr, send_buf->ethhdr.src.addr, 6);//目的MAC为接收到的帧中的源MAC
			memcpy(send_buf->ethhdr.src.addr, NET_setting.mac_add, 6);//响应帧的源MAC为本机MAC
			
		    send_buf->protocol = HTONS(ISEARCH_DEVICE_DISCOVERY_ACK);//响应iSearch的搜索协议值
						
			ptr1->reserved1 = 0;
			ptr1->ack_packet = 0;
			
			memset(ptr1->reserved2,0,14);
			memcpy(ptr1->dev_password,"12345678",8);
			strncpy(ptr1->dev_hw_ver,DEV_HW_VER,15);
			strncpy(ptr1->dev_sw_ver,DEV_SW_VER,15);
			
			
			memcpy(ptr1->dev_serial_num,Globa->Charger_param.SN,12);//序列号，20字节
		  
			ptr1->dev_http_port[0] = 0;
			ptr1->dev_http_port[1] = 80;
		  
			ptr1->dev_telnet_port[0] = 0;
			ptr1->dev_telnet_port[1] = 23;
		  
			ptr1->dev_protocol_flag = 0xFF;//iSearch new protocol
			if(0 == NET_setting.assign_method)//IP静态配置
				ptr1->dev_ip_method = 0xFF;//IP方式
			else
				ptr1->dev_ip_method = 0x55;
			
			ptr1->ip_addr2[0] = 0;
			ptr1->ip_addr2[1] = 0;
			ptr1->ip_addr2[2] = 0;
			ptr1->ip_addr2[3] = 0;
			
			get_ip(NET_setting.ethName,ptr1->ip_addr3);//本机当前IP地址		  
			get_netmask(NET_setting.ethName,ptr1->net_mask);//本机当前子网掩码
			get_gateway(NET_setting.ethName,gateway_addr);//本机当前网关地址	
			ascii_to_number(ptr1->gate_way,gateway_addr);//"192.168.0,1"==>  192  168  0  1 
			
			//////////////////////////////////////////////////////////////////////////////
			
			send_len = sizeof(struct uip_eth_hdr) + 4 + sizeof(ISEARCH_DISCOVERY_RESP);//tell uip how much data to send
		}

		break;
	  
	case HTONS(ISEARCH_DEVICE_ALIVE_CHK_REQ)://主机的alive check命令
	  {	
			memcpy(send_buf->ethhdr.dest.addr, send_buf->ethhdr.src.addr, 6);//目的MAC为接收到的帧中的源MAC
			memcpy(send_buf->ethhdr.src.addr, NET_setting.mac_add, 6);//响应帧的源MAC为本机MAC
			
		    send_buf->protocol = HTONS(ISEARCH_DEVICE_ALIVE_CHK_ACK);//响应iSearch的alive check			
			
			send_len = 42;//tell uip how much data to send
		}

		break;	
		
	case HTONS(ISEARCH_DEVICE_SET_INFO_REQ)://主机的设置IP地址，密码，端口等信息命令
	  {
			ptr2 = (PISEARCH_SETIP_CMD) (send_buf->pad);
			
			memcpy(send_buf->ethhdr.dest.addr, send_buf->ethhdr.src.addr, 6);//目的MAC为接收到的帧中的源MAC
			memcpy(send_buf->ethhdr.src.addr, NET_setting.mac_add, 6);//响应帧的源MAC为本机MAC
			
		    send_buf->protocol = HTONS(ISEARCH_DEVICE_SET_INFO_ACK);//响应iSearch
			
			if( (ptr2->ip_addr2[0]  & ptr2->ip_addr2[1] & ptr2->ip_addr2[2] & ptr2->ip_addr2[3]) == 0xFF)//使用静态IP设置方式
			{
				NET_setting.assign_method = 0;
				snprintf(ip_addr,19,"%d.%d.%d.%d",ptr2->ip_addr3[0],ptr2->ip_addr3[1],ptr2->ip_addr3[2],ptr2->ip_addr3[3]);
				snprintf(netmask_addr,19,"%d.%d.%d.%d",ptr2->net_mask[0],ptr2->net_mask[1],ptr2->net_mask[2],ptr2->net_mask[3]);
				snprintf(gateway_addr,19,"%d.%d.%d.%d",ptr2->gate_way[0],ptr2->gate_way[1],ptr2->gate_way[2],ptr2->gate_way[3]);
				if( (if_a_string_is_a_valid_ipv4_address(  ip_addr ,1))&&
					(if_a_string_is_a_valid_ipv4_address(  gateway_addr ,1))&&
					(IsSubnetMask(  netmask_addr))
					)
					{
						cur_net_setting.dhcp = 0;				
						netinfo->isdhcp = 0;
						cur_net_setting.ip_addr[0] = ptr2->ip_addr3[0];//IP地址
						cur_net_setting.ip_addr[1] = ptr2->ip_addr3[1];//IP地址
						cur_net_setting.ip_addr[2] = ptr2->ip_addr3[2];//IP地址
						cur_net_setting.ip_addr[3] = ptr2->ip_addr3[3];//IP地址
						
					  
						cur_net_setting.net_mask[0] = ptr2->net_mask[0];//子网掩码
						cur_net_setting.net_mask[1] = ptr2->net_mask[1];//子网掩码
						cur_net_setting.net_mask[2] = ptr2->net_mask[2];//子网掩码
						cur_net_setting.net_mask[3] = ptr2->net_mask[3];//子网掩码
						
						cur_net_setting.gate_way[0] = ptr2->gate_way[0] ;//网关地址
						cur_net_setting.gate_way[1] = ptr2->gate_way[1] ;//网关地址
						cur_net_setting.gate_way[2] = ptr2->gate_way[2] ;//网关地址
						cur_net_setting.gate_way[3] = ptr2->gate_way[3] ;//网关地址
							
						strncpy(netinfo->IP_address,ip_addr,15);
						strncpy(netinfo->msk_address,netmask_addr,15);
						strncpy(netinfo->gate_address,gateway_addr,15);
									
						//set ip addr and netmask
						UpdateEthPara();//更新当前系统使用的IP地址信息
					}
			}
			else
			{
				NET_setting.assign_method = 1;	//动态
				cur_net_setting.dhcp = 1;
				netinfo->isdhcp = 1;
				//System_param_save();			
				//set ip addr and netmask
				UpdateEthPara();//更新当前系统使用的IP地址信息
			}
			system("sync");
			system("reboot");
			
			send_len = 42;//tell uip how much data to send
		}

		break;	
		
	case HTONS(ISEARCH_DEVICE_SET_MAC_REQ)://主机的设置MAC,序列号等信息命令
		{
			ptr3 = (PISEARCH_SETMAC_CMD) (send_buf->pad);
			
			 //修改mac
				char mac[50]={0};
				char temp = 255;
				if( (ptr3->set_dev_mac_addr[1] >= '0')&&(ptr3->set_dev_mac_addr[1] <= '9'))
					temp = ptr3->set_dev_mac_addr[1] - '0';
				if( (ptr3->set_dev_mac_addr[1] >= 'a')&&(ptr3->set_dev_mac_addr[1] <= 'f'))
					temp = ptr3->set_dev_mac_addr[1] - 87;
				if( (ptr3->set_dev_mac_addr[1] >= 'A')&&(ptr3->set_dev_mac_addr[1] <= 'F'))
					temp = ptr3->set_dev_mac_addr[1] - 55;
				if( (temp >= 0) && ( temp <= 0x0F) )//may valid
				{
					if( 0 == (temp & 0x01))//单播MAC地址
					{
						if((ptr3->set_dev_mac_addr[2]==0x2d)||(ptr3->set_dev_mac_addr[2]==0x3a))//-或者:
							snprintf((char *)mac,sizeof(mac), "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c",ptr3->set_dev_mac_addr[0],ptr3->set_dev_mac_addr[1],ptr3->set_dev_mac_addr[3],ptr3->set_dev_mac_addr[4],ptr3->set_dev_mac_addr[6],ptr3->set_dev_mac_addr[7],ptr3->set_dev_mac_addr[9],ptr3->set_dev_mac_addr[10],ptr3->set_dev_mac_addr[12],ptr3->set_dev_mac_addr[13],ptr3->set_dev_mac_addr[15],ptr3->set_dev_mac_addr[16]);
						else
							snprintf((char *)mac,sizeof(mac), "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c",ptr3->set_dev_mac_addr[0],ptr3->set_dev_mac_addr[1],ptr3->set_dev_mac_addr[2],ptr3->set_dev_mac_addr[3],ptr3->set_dev_mac_addr[4],ptr3->set_dev_mac_addr[5],ptr3->set_dev_mac_addr[6],ptr3->set_dev_mac_addr[7],ptr3->set_dev_mac_addr[8],ptr3->set_dev_mac_addr[9],ptr3->set_dev_mac_addr[10],ptr3->set_dev_mac_addr[11]);
						for(i=0;i<17;i++)
							Uppercase(&mac[i],&mac[i]);//确保MAC地址大写防止QT界面判断当前MAC与文件保存的MAC字符串比较不一致而反复重启!!						
						printf("\nnew mac:%s\n",mac);
						strcpy(netinfo->mac_address,mac);
						savenetinfo();						
						snprintf((char *)tmp,sizeof(tmp), "echo export HW_FACER=%s>%s&&reboot",mac,macCfgPath);
						

						system((char *)tmp);//修改MAC后直接重启
					}	
				}				
				send_len = 0;//no response			
		}

		break;
	case HTONS(ISEARCH_DEVICE_SET_SN_REQ)://主机的设置序列号等信息命令
		{
			ptr4 = (PISEARCH_SETSN_CMD) (send_buf->pad);
			
			// memcpy(send_buf->ethhdr.dest.addr, send_buf->ethhdr.src.addr, 6);//目的MAC为接收到的帧中的源MAC
			// memcpy(send_buf->ethhdr.src.addr, NET_setting.mac_add, 6);//响应帧的源MAC为本机MAC
			
			// send_buf->protocol = HTONS(ISEARCH_DEVICE_SET_MAC_ACK);//响应iSearch
			
			memcpy(Globa->Charger_param.SN, ptr4->set_dev_serial_num, 12);//copy new serial num
			System_param_save();
			system("sync");
			system("reboot");
		}

		break;	
	case HTONS(ISEARCH_DEVICE_RESTORE_REQ)://主机的恢复出厂值命令
		{
			memcpy(send_buf->ethhdr.dest.addr, send_buf->ethhdr.src.addr, 6);//目的MAC为接收到的帧中的源MAC
			memcpy(send_buf->ethhdr.src.addr, NET_setting.mac_add, 6);//响应帧的源MAC为本机MAC
			
			send_buf->protocol = HTONS(ISEARCH_DEVICE_RESTORE_ACK);//响应iSearch
						
			send_len = 42;//tell uip how much data to send
			printf("recv restore default cmd\n");
		}

		break;	
		
		default:	//other cmd,not support,no response		
			
		
			break;
	} 
	
	return send_len;	
}


