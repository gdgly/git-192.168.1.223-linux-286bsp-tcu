//*****************************************************************************
//ʵ�ֶ�PC��iSearch�����������������Ӧ��
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
	u16_t reserved1;//Ӳ����ַ���Ⱥ�Э���ַ����(��2�ֽڣ�ȫ0)
	u16_t ack_packet;//2�ֽڣ��̶�Ϊ0x0002,ץ�����ֱ���豸��ӦΪȫ0Ҳ����
	u8_t  reserved2[14];//14�ֽڱ���ȫ0
	u8_t  dev_password[16];//16�ֽ��豸����,ASCII��
	u8_t  dev_hw_ver[16];//16�ֽ��豸Ӳ���汾,ASCII��
	u8_t  dev_sw_ver[16];//16�ֽ��豸����汾,ASCII��
	u8_t  dev_serial_num[20];//20�ֽ��豸���к�,ASCII��
	u8_t  dev_http_port[2];//2�ֽ��豸WEB�˿ںţ�ʮ�����ƣ��͵�ַ��Ÿ��ֽ�
	u8_t  dev_telnet_port[2];//2�ֽ��豸telnet�˿ںţ�ʮ�����ƣ��͵�ַ��Ÿ��ֽ�
	u8_t  dev_protocol_flag;//ֵ0xFF��ʾ��iSearch����Э��
	u8_t  dev_ip_method;//IP��ַ��ȡ��ʽ�������̬��ȡ�����0x55,�����̬���������0xFF
	u8_t  ip_addr2[4];//IP��ַ2��������Ĭ��ȫ0
	u8_t  ip_addr3[4];//IP��ַ3���豸��IP��ַ
	u8_t  net_mask[4];//��������
	u8_t  gate_way[4];//���ص�ַ	
}ISEARCH_DISCOVERY_RESP,*PISEARCH_DISCOVERY_RESP;//iSearch����֡����Ӧ֡�ṹ

typedef struct 
{	
	u8_t  dev_password[16];//16�ֽ��豸���룬����iSearch���͹���Ϊȫ0
	u8_t  set_dev_password[16];//����16�ֽ��豸����
	u8_t  reserved1[14];//14�ֽڱ���ȫ0
	u8_t  gate_way[4];//�����豸�����ص�ַ
	u8_t  ip_addr2[4];//IP��ַ��ȡ��ʽ��4�ֽ�ȫΪxFF��ʾʹ�þ�̬IP������ʹ��DHCP��ʽ
	u8_t  ip_addr3[4];//�����豸��IP��ַ
	u8_t  net_mask[4];//�����豸����������	
	u8_t  dev_http_port[2];//2�ֽ��豸WEB�˿ںţ�ʮ�����ƣ��͵�ַ��Ÿ��ֽ�
	u8_t  dev_telnet_port[2];//2�ֽ��豸telnet�˿ںţ�ʮ�����ƣ��͵�ַ��Ÿ��ֽ�
	u8_t  reserved2[39];
}ISEARCH_SETIP_CMD,*PISEARCH_SETIP_CMD;//iSearch����IP��ַ��Ϣ��֡�ṹ

typedef struct 
{	
	u8_t  pad[4];//4�ֽڱ���
	u8_t  set_dev_mac_addr[18];//�����豸MAC��ַ,ASCII�룬����iSearch���͹���Ϊȫ0������û�нӿ�����MAC---30:89:99:12:34:56
	u8_t  set_dev_serial_num[20];//20�ֽڵ��豸���к�	
	u8_t  reserved[10];
}ISEARCH_SETMAC_CMD,*PISEARCH_SETMAC_CMD;//iSearch����MAC��ַ��֡�ṹ

typedef struct 
{	
	u8_t  pad[4];//4�ֽڱ���
	u8_t  set_dev_mac_addr[18];//�����豸MAC��ַ,ASCII�룬����iSearch���͹���Ϊȫ0������û�нӿ�����MAC---30:89:99:12:34:56
	u8_t  set_dev_serial_num[20];//20�ֽڵ��豸���к�	
	u8_t  reserved[10];
}ISEARCH_SETSN_CMD,*PISEARCH_SETSN_CMD;//iSearch�������к�֡�ṹ



//������յ���iSearch�������̫��discovery������֡,���ر��Ĵ����tx_buf
//��������ֵ����0��ʾ���������ݳ���
int iSearch_Packetin(char* recv_buf,int rx_len,char* tx_buf)
{ 	
	int i;
	PISEARCH_DISCOVERY_RESP  ptr1;
	PISEARCH_SETIP_CMD       ptr2;	
	PISEARCH_SETMAC_CMD      ptr3;
	PISEARCH_SETSN_CMD       ptr4;
	
	char macCfgPath[50]={0};
	
	snprintf(macCfgPath,sizeof(macCfgPath),"%s","/etc/rc.d/rc.hw");//286 mac����·��
	
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
	  case HTONS(ISEARCH_DEVICE_DISCOVERY_REQ)://������DISCOVERY����
	  {
			ptr1 = (PISEARCH_DISCOVERY_RESP) (send_buf->pad);
			memset(ptr1,0,sizeof(ISEARCH_DISCOVERY_RESP));
			memcpy(send_buf->ethhdr.dest.addr, send_buf->ethhdr.src.addr, 6);//Ŀ��MACΪ���յ���֡�е�ԴMAC
			memcpy(send_buf->ethhdr.src.addr, NET_setting.mac_add, 6);//��Ӧ֡��ԴMACΪ����MAC
			
		    send_buf->protocol = HTONS(ISEARCH_DEVICE_DISCOVERY_ACK);//��ӦiSearch������Э��ֵ
						
			ptr1->reserved1 = 0;
			ptr1->ack_packet = 0;
			
			memset(ptr1->reserved2,0,14);
			memcpy(ptr1->dev_password,"12345678",8);
			strncpy(ptr1->dev_hw_ver,DEV_HW_VER,15);
			strncpy(ptr1->dev_sw_ver,DEV_SW_VER,15);
			
			
			memcpy(ptr1->dev_serial_num,Globa->Charger_param.SN,12);//���кţ�20�ֽ�
		  
			ptr1->dev_http_port[0] = 0;
			ptr1->dev_http_port[1] = 80;
		  
			ptr1->dev_telnet_port[0] = 0;
			ptr1->dev_telnet_port[1] = 23;
		  
			ptr1->dev_protocol_flag = 0xFF;//iSearch new protocol
			if(0 == NET_setting.assign_method)//IP��̬����
				ptr1->dev_ip_method = 0xFF;//IP��ʽ
			else
				ptr1->dev_ip_method = 0x55;
			
			ptr1->ip_addr2[0] = 0;
			ptr1->ip_addr2[1] = 0;
			ptr1->ip_addr2[2] = 0;
			ptr1->ip_addr2[3] = 0;
			
			get_ip(NET_setting.ethName,ptr1->ip_addr3);//������ǰIP��ַ		  
			get_netmask(NET_setting.ethName,ptr1->net_mask);//������ǰ��������
			get_gateway(NET_setting.ethName,gateway_addr);//������ǰ���ص�ַ	
			ascii_to_number(ptr1->gate_way,gateway_addr);//"192.168.0,1"==>  192  168  0  1 
			
			//////////////////////////////////////////////////////////////////////////////
			
			send_len = sizeof(struct uip_eth_hdr) + 4 + sizeof(ISEARCH_DISCOVERY_RESP);//tell uip how much data to send
		}

		break;
	  
	case HTONS(ISEARCH_DEVICE_ALIVE_CHK_REQ)://������alive check����
	  {	
			memcpy(send_buf->ethhdr.dest.addr, send_buf->ethhdr.src.addr, 6);//Ŀ��MACΪ���յ���֡�е�ԴMAC
			memcpy(send_buf->ethhdr.src.addr, NET_setting.mac_add, 6);//��Ӧ֡��ԴMACΪ����MAC
			
		    send_buf->protocol = HTONS(ISEARCH_DEVICE_ALIVE_CHK_ACK);//��ӦiSearch��alive check			
			
			send_len = 42;//tell uip how much data to send
		}

		break;	
		
	case HTONS(ISEARCH_DEVICE_SET_INFO_REQ)://����������IP��ַ�����룬�˿ڵ���Ϣ����
	  {
			ptr2 = (PISEARCH_SETIP_CMD) (send_buf->pad);
			
			memcpy(send_buf->ethhdr.dest.addr, send_buf->ethhdr.src.addr, 6);//Ŀ��MACΪ���յ���֡�е�ԴMAC
			memcpy(send_buf->ethhdr.src.addr, NET_setting.mac_add, 6);//��Ӧ֡��ԴMACΪ����MAC
			
		    send_buf->protocol = HTONS(ISEARCH_DEVICE_SET_INFO_ACK);//��ӦiSearch
			
			if( (ptr2->ip_addr2[0]  & ptr2->ip_addr2[1] & ptr2->ip_addr2[2] & ptr2->ip_addr2[3]) == 0xFF)//ʹ�þ�̬IP���÷�ʽ
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
						cur_net_setting.ip_addr[0] = ptr2->ip_addr3[0];//IP��ַ
						cur_net_setting.ip_addr[1] = ptr2->ip_addr3[1];//IP��ַ
						cur_net_setting.ip_addr[2] = ptr2->ip_addr3[2];//IP��ַ
						cur_net_setting.ip_addr[3] = ptr2->ip_addr3[3];//IP��ַ
						
					  
						cur_net_setting.net_mask[0] = ptr2->net_mask[0];//��������
						cur_net_setting.net_mask[1] = ptr2->net_mask[1];//��������
						cur_net_setting.net_mask[2] = ptr2->net_mask[2];//��������
						cur_net_setting.net_mask[3] = ptr2->net_mask[3];//��������
						
						cur_net_setting.gate_way[0] = ptr2->gate_way[0] ;//���ص�ַ
						cur_net_setting.gate_way[1] = ptr2->gate_way[1] ;//���ص�ַ
						cur_net_setting.gate_way[2] = ptr2->gate_way[2] ;//���ص�ַ
						cur_net_setting.gate_way[3] = ptr2->gate_way[3] ;//���ص�ַ
							
						strncpy(netinfo->IP_address,ip_addr,15);
						strncpy(netinfo->msk_address,netmask_addr,15);
						strncpy(netinfo->gate_address,gateway_addr,15);
									
						//set ip addr and netmask
						UpdateEthPara();//���µ�ǰϵͳʹ�õ�IP��ַ��Ϣ
					}
			}
			else
			{
				NET_setting.assign_method = 1;	//��̬
				cur_net_setting.dhcp = 1;
				netinfo->isdhcp = 1;
				//System_param_save();			
				//set ip addr and netmask
				UpdateEthPara();//���µ�ǰϵͳʹ�õ�IP��ַ��Ϣ
			}
			system("sync");
			system("reboot");
			
			send_len = 42;//tell uip how much data to send
		}

		break;	
		
	case HTONS(ISEARCH_DEVICE_SET_MAC_REQ)://����������MAC,���кŵ���Ϣ����
		{
			ptr3 = (PISEARCH_SETMAC_CMD) (send_buf->pad);
			
			 //�޸�mac
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
					if( 0 == (temp & 0x01))//����MAC��ַ
					{
						if((ptr3->set_dev_mac_addr[2]==0x2d)||(ptr3->set_dev_mac_addr[2]==0x3a))//-����:
							snprintf((char *)mac,sizeof(mac), "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c",ptr3->set_dev_mac_addr[0],ptr3->set_dev_mac_addr[1],ptr3->set_dev_mac_addr[3],ptr3->set_dev_mac_addr[4],ptr3->set_dev_mac_addr[6],ptr3->set_dev_mac_addr[7],ptr3->set_dev_mac_addr[9],ptr3->set_dev_mac_addr[10],ptr3->set_dev_mac_addr[12],ptr3->set_dev_mac_addr[13],ptr3->set_dev_mac_addr[15],ptr3->set_dev_mac_addr[16]);
						else
							snprintf((char *)mac,sizeof(mac), "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c",ptr3->set_dev_mac_addr[0],ptr3->set_dev_mac_addr[1],ptr3->set_dev_mac_addr[2],ptr3->set_dev_mac_addr[3],ptr3->set_dev_mac_addr[4],ptr3->set_dev_mac_addr[5],ptr3->set_dev_mac_addr[6],ptr3->set_dev_mac_addr[7],ptr3->set_dev_mac_addr[8],ptr3->set_dev_mac_addr[9],ptr3->set_dev_mac_addr[10],ptr3->set_dev_mac_addr[11]);
						for(i=0;i<17;i++)
							Uppercase(&mac[i],&mac[i]);//ȷ��MAC��ַ��д��ֹQT�����жϵ�ǰMAC���ļ������MAC�ַ����Ƚϲ�һ�¶���������!!						
						printf("\nnew mac:%s\n",mac);
						strcpy(netinfo->mac_address,mac);
						savenetinfo();						
						snprintf((char *)tmp,sizeof(tmp), "echo export HW_FACER=%s>%s&&reboot",mac,macCfgPath);
						

						system((char *)tmp);//�޸�MAC��ֱ������
					}	
				}				
				send_len = 0;//no response			
		}

		break;
	case HTONS(ISEARCH_DEVICE_SET_SN_REQ)://�������������кŵ���Ϣ����
		{
			ptr4 = (PISEARCH_SETSN_CMD) (send_buf->pad);
			
			// memcpy(send_buf->ethhdr.dest.addr, send_buf->ethhdr.src.addr, 6);//Ŀ��MACΪ���յ���֡�е�ԴMAC
			// memcpy(send_buf->ethhdr.src.addr, NET_setting.mac_add, 6);//��Ӧ֡��ԴMACΪ����MAC
			
			// send_buf->protocol = HTONS(ISEARCH_DEVICE_SET_MAC_ACK);//��ӦiSearch
			
			memcpy(Globa->Charger_param.SN, ptr4->set_dev_serial_num, 12);//copy new serial num
			System_param_save();
			system("sync");
			system("reboot");
		}

		break;	
	case HTONS(ISEARCH_DEVICE_RESTORE_REQ)://�����Ļָ�����ֵ����
		{
			memcpy(send_buf->ethhdr.dest.addr, send_buf->ethhdr.src.addr, 6);//Ŀ��MACΪ���յ���֡�е�ԴMAC
			memcpy(send_buf->ethhdr.src.addr, NET_setting.mac_add, 6);//��Ӧ֡��ԴMACΪ����MAC
			
			send_buf->protocol = HTONS(ISEARCH_DEVICE_RESTORE_ACK);//��ӦiSearch
						
			send_len = 42;//tell uip how much data to send
			printf("recv restore default cmd\n");
		}

		break;	
		
		default:	//other cmd,not support,no response		
			
		
			break;
	} 
	
	return send_len;	
}


