

#include <stdlib.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <linux/if_ether.h>

#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h> /* the L2 protocols */

#include "../data/globalvar.h"	   
#include "macsearch_public.h"

#include "iSearch_service.h"
#include "m3_search.h"


int eth_packet_in(char* recv_buf,int rx_len,char* tx_buf)
{
	u16_t protocol;
	int tx_len = 0;
	struct arp_hdr *BUF = ((struct arp_hdr *)&recv_buf[0]);

	if(rx_len < sizeof(struct arp_hdr)) 
	{
		return 0;
	}
  
	if(BUF->hwtype == HTONS(ARP_HWTYPE_ETH))    
	{ 
		protocol = BUF->protocol;
		
		if( (protocol&0x00FF) == HTONS(ISEARCH_DEVICE_DISCOVERY) )//iSearch搜索设置帧,只判断0xEA
			tx_len = iSearch_Packetin(recv_buf,rx_len,tx_buf);
		else
		{
			switch(protocol)//增加一些判断以便与我们自定义的M3的搜索功能区分开来
			{
				case HTONS(UIP_ETHTYPE_IP)://认为一定是标准的ARP协议
				{	
					switch(BUF->opcode)//判断ARP协议中的操作码 
					{
					  case HTONS(ARP_REQUEST):
						/* ARP request. If it asked for our address, we send out a
						   reply. */
					
						
						break;
					  case HTONS(ARP_REPLY):
						
						break;
					}
				}
					break;
			
				case HTONS(M3_DEVICE_DISCOVERY)://自定义的M3设备搜索协议
					tx_len = eth_device_packetin(recv_buf,rx_len,tx_buf);
				break;
			
				default: //not support protocol
				
					break;
			}
		}
		
	}  
  
	return tx_len;
}

void macsearch(void*arg)
{
#ifdef __linux__
    int len;
    unsigned char *p;
    unsigned char buf[1514];
    unsigned char tmp[100];
    unsigned char mac_add[8];
	char tx_buf[1514];
	int tx_len ;
	char gate_way[20];
    struct ifreq ifstruct;
    struct sockaddr_ll sll;
    prctl(PR_SET_NAME,"macsearch");
    printf("macsearch start!\n");
    memset(&NET_setting,0,sizeof(NET_SETTING));

    char ethName[15]={0};
    FILE*fp=fopen("config/ethName","r");
    if(fp)
    {
        fgets(ethName,15,fp);
        int i=0;
        char *p=strchr(ethName,'\n');
        if(p)
            *p=0;
        memcpy(NET_setting.ethName,ethName,15);
        fclose(fp);
    }
    else
    {
        snprintf(NET_setting.ethName,sizeof(NET_setting.ethName),"eth0");
        fp=fopen("config/ethName","w");
        if(fp)
        {
            fwrite(NET_setting.ethName,15,strlen(NET_setting.ethName),fp);
            fflush(fp);
            fclose(fp);
        }
    }

	netinfo = &cur_286_netinfo;
 
	System_net_setting_init();
	NET_setting.assign_method=cur_net_setting.dhcp;
	

			
    struct timeval tv;
    fd_set readfds;

    int sock = socket(PF_PACKET, SOCK_RAW, HTONS(ETH_P_ALL));
    if (sock < 0){
        perror("socket create error");
        return NULL;
    }

    memset(&sll, 0, sizeof(sll)); // Maybe no need

    while(1)
    {
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 10000;//10ms
        select(0, NULL, NULL, NULL, &tv);
        memset(&mac_add[0], 0, sizeof(mac_add)); // Maybe no need
        if(get_mac_add(NET_setting.ethName,&mac_add[0]) == -1){
            continue;
        }
		memcpy(NET_setting.mac_add,mac_add,6);
		
        memset(&buf, 0, sizeof(buf)); // Maybe no need

        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        if(select(sock+1,&readfds,NULL, NULL, &tv) > 0)
		{
            if(FD_ISSET(sock, &readfds)){
                socklen_t addr_len=sizeof(sll);
                len = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&sll, &addr_len);
                if(len < 0){
                    continue;
                }
            }else{
                continue;
            }
        }else
		{
            continue;
        }
		struct uip_eth_hdr* eth_hdr = (struct uip_eth_hdr*)buf;       
        if( ((eth_hdr->dest.addr[0] == 0xFF)&&(eth_hdr->dest.addr[1] == 0xFF)&&(eth_hdr->dest.addr[2] == 0xFF)&&
			 (eth_hdr->dest.addr[3] == 0xFF)&&(eth_hdr->dest.addr[4] == 0xFF)&&(eth_hdr->dest.addr[5] == 0xFF))//广播地址
            ||
			((eth_hdr->dest.addr[0] == mac_add[0])&&(eth_hdr->dest.addr[1] == mac_add[1])&&(eth_hdr->dest.addr[2] == mac_add[2])&&
			 (eth_hdr->dest.addr[3] == mac_add[3])&&(eth_hdr->dest.addr[4] == mac_add[4])&&(eth_hdr->dest.addr[5] == mac_add[5])))//本机地址
        {
           tx_len =  eth_packet_in(buf,len,tx_buf);
		   if(tx_len > 0)
			   sendto(sock, tx_buf, tx_len, 0, (struct sockaddr *)&sll, sizeof(sll));
        }
    }

    close(sock); //cwenfu 20120410
#endif
    return ;
}

extern void macsearch_Thread(void* para)
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
	
	/* 创建线程 */
	if(0 != pthread_create(&td1, &attr, (void *)macsearch, para)){
		perror("####pthread_create macsearch ERROR ####\n\n");
	}

	ret = pthread_attr_destroy(&attr);
	if(ret != 0)
		return;
}
