
#include <sys/prctl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <net/route.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <ctype.h>
#include <net/if.h>       /* ifreq struct */
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>

#include <pthread.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <signal.h>
#include <netinet/ip.h>

#include <linux/netlink.h> 
#include <linux/rtnetlink.h>    //for rtnetlink 

#include "../data/globalvar.h"
#include "macsearch_public.h"

const char* const DEV_SW_VER      = SOFTWARE_VER;//"EVAC_NUC972_V1.0";//软件版本16bytes max
const char* const DEV_HW_VER      = "V001";//硬件版本16bytes max

NET_SETTING     NET_setting;
CUR_NET_SETTING cur_net_setting;


void Uppercase(char* in,char*out)
{
	char din,dout;
	din = *in;
	if((din >= 'a') && (din <= 'f'))//MAC 字母必须在A--F
	{
		dout = din - 32;
		*out = dout;
	}
	else
	{	
		if( ((din >= 'A') && (din <= 'F')) ||
			((din >= '0') && (din <= '9')) ||
			(din == ':'))//no change
			*out = din;	
		else//异常值
			*out = '3';//异常全部给3
	}
}

#ifndef BUFSIZE
#define BUFSIZE 8192
#endif

// struct route_info
// {
    // u_int dstAddr;
    // u_int srcAddr;
    // u_int gateWay;
    // char ifName[IF_NAMESIZE];
// };

/*
*
*/
int read_nlSock(int sockFd, char *bufPtr, int seqNum, int pId)
{
    struct nlmsghdr *nlHdr;
    int readLen = 0, msgLen = 0;
    
    do{
        //receive kernel response
        if((readLen = recv(sockFd, bufPtr, BUFSIZE - msgLen, 0)) < 0)
        {
              //perror("SOCK READ: ");
              return -1;
        }
 
        nlHdr = (struct nlmsghdr *)bufPtr;
        
        //check header valid
        if((NLMSG_OK(nlHdr, readLen) == 0)
            || (nlHdr->nlmsg_type == NLMSG_ERROR))
        {
              //perror("Error in received packet");
              return -1;
        }
 
        // Check if the its the last message
        if(nlHdr->nlmsg_type == NLMSG_DONE)
        {
             break;
        }
        else
        {
              //Else move the pointer to buffer appropriately
              bufPtr += readLen;
              msgLen += readLen;
        }
 
        //Check if its a multi part message
        if((nlHdr->nlmsg_flags & NLM_F_MULTI) == 0)
        {
              //return if its not
             break;
        }
    } while((nlHdr->nlmsg_seq != seqNum) || (nlHdr->nlmsg_pid != pId));

    return msgLen;
}

/*
*
*/
int parse_routes(struct nlmsghdr *nlHdr, struct route_info *rtInfo, const char *eth_name, char *gateway)
{
    int ret = -1;
    
    struct rtmsg *rtMsg;
    struct rtattr *rtAttr;
    int rtLen;
    
    struct in_addr dst;
    struct in_addr gate;

    rtMsg = (struct rtmsg *)NLMSG_DATA(nlHdr);
    
    // If the route is not for AF_INET or does not belong to main routing
    // table, then return.
    if ((rtMsg->rtm_family != AF_INET)
        || (rtMsg->rtm_table != RT_TABLE_MAIN))
    {
        return ret;
    }

    //get the rtattr field
    rtAttr = (struct rtattr *)RTM_RTA(rtMsg);
    rtLen = RTM_PAYLOAD(nlHdr);    
    
    for (; RTA_OK(rtAttr,rtLen); rtAttr = RTA_NEXT(rtAttr,rtLen))
    {
           switch(rtAttr->rta_type)
        {
           case RTA_OIF:
            if_indextoname(*(int *)RTA_DATA(rtAttr), rtInfo->ifName);
            break;
            
           case RTA_GATEWAY:
            rtInfo->gateWay = *(u_int *)RTA_DATA(rtAttr);
            break;
           
           case RTA_PREFSRC:
            rtInfo->srcAddr = *(u_int *)RTA_DATA(rtAttr);
            break;
           
           case RTA_DST:
            rtInfo->dstAddr = *(u_int *)RTA_DATA(rtAttr);
            break;
           }
    }

    dst.s_addr = rtInfo->dstAddr;
    if (strstr((char *)inet_ntoa(dst), "0.0.0.0")
        && strcmp(eth_name, rtInfo->ifName) == 0)
    {
        gate.s_addr = rtInfo->gateWay;
        strcpy((char *)gateway, (char *)inet_ntoa(gate));
        ret = 0;
    }
    
    return ret;
}

/*
* get_ip_mask: get the ip and mask, specify eth name
*
* eth_name:
* ip:
* mask:
*
* return: 0 succ
*          -1 fail
*/
int get_ip_mask(const char *eth_name, char *ip, char *mask)
{
    if (eth_name == NULL || ip == NULL || mask == NULL)
    {
        return -1;
    }

    int sock;
    struct sockaddr_in sin;
    struct ifreq ifr;

    if ((sock =socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        return -1;
    }

    strncpy(ifr.ifr_name, eth_name, IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ -1] = 0;

    if (ioctl(sock, SIOCGIFADDR, &ifr) < 0)
    {
	close(sock);
        return -1;
    }

    memcpy(&sin, &ifr.ifr_addr, sizeof(sin));
    char *ip_get = inet_ntoa(sin.sin_addr);
    strncpy(ip, ip_get, strlen(ip_get));

    if (ioctl(sock, SIOCGIFNETMASK, &ifr) < 0)
    {
	close(sock);
        return -1;    
    }

    memcpy(&sin, &ifr.ifr_addr, sizeof(sin));
    char *mask_get = inet_ntoa(sin.sin_addr);
    strncpy(mask, mask_get, strlen(mask_get));
    close(sock);
    return 0;
}

/********************************************************************
* get_gateway: get the gateway, specify eth name
*
* eth_name： eth name, not null
* gateway: gateway, not null
*
* return： 0 succ
*         -1 fail
********************************************************************/
int get_gateway(const char *eth_name, char *gateway)
{
    int ret = -1;
    if (eth_name == NULL || gateway == NULL)
    {
        return ret;
    }
    strncpy((char*)gateway,"0.0.0.0",8);
    struct nlmsghdr *nlMsg;
    struct route_info *rtInfo;
    char msgBuf[BUFSIZE];

    int sock, len, msgSeq = 0;
    
    if((sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE)) < 0)
    {
           //perror("Socket Creation: ");
           return -1;
    }

    //Initialize the buffer
    memset(msgBuf, 0, BUFSIZE);

    //point the header and the msg structure pointers into the buffer
    nlMsg = (struct nlmsghdr *)msgBuf;

    nlMsg->nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
    nlMsg->nlmsg_type = RTM_GETROUTE;
    nlMsg->nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST;
    nlMsg->nlmsg_seq = msgSeq++;
    nlMsg->nlmsg_pid = getpid();

    //Send the request
    if (send(sock, nlMsg, nlMsg->nlmsg_len, 0) < 0){
        close(sock);
	return ret;
    }

    //Read the response
    if((len = read_nlSock(sock, msgBuf, msgSeq, getpid())) < 0) {
        //printf("Read From Socket Failed.../n");
        close(sock);
	return ret;
    }

    //Parse and print the response
    rtInfo = (struct route_info *)malloc(sizeof(struct route_info));
    for(; NLMSG_OK(nlMsg, len);    nlMsg = NLMSG_NEXT(nlMsg, len))
    {
        memset(rtInfo, 0, sizeof(struct route_info));
        if (parse_routes(nlMsg, rtInfo, eth_name, gateway) == 0)
        {
            ret = 0;
            break;
        }
    }
    
    free(rtInfo);
    close(sock);
    return 0;
}



/*获取IP地址获取状态，0为静态，1为动态*/
int getDhcpState(char *ethName)
{
    char buf[500]={0};
    int state=0;
    char fileName[100]={0};
    //snprintf(fileName,sizeof(fileName),"/etc/network/interfaces");
	snprintf(fileName,sizeof(fileName),"/etc/rc.d/rs.conf");
	
    FILE *f=fopen(fileName,"r");
    if(f)
    {
        while(fgets(buf,500,f))
        {
            if(strstr(buf,ethName)!=NULL && strstr(buf,"dhcp"))
            {
                state=1;
                break;
            }
        }
        fclose(f);
    }
    return state;

}
#ifdef __linux__
int sGet_MAC(char *ethNo,unsigned char *macaddr)
{
    int sock;
    struct ifreq ifr;
    strncpy((char*)macaddr,"xx:xx:xx:xx:xx:xx",19);
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
    {
        perror("socket");
        return -1;
    }
    strncpy(ifr.ifr_name, ethNo, IFNAMSIZ);/*这个是网卡的标识符*/
    ifr.ifr_name[IFNAMSIZ - 1] = 0;
    if (ioctl(sock,SIOCGIFHWADDR, &ifr)< 0)
    {
        perror("get mac");
        close(sock);
        return -1;
    }

    snprintf((char *)macaddr,19,"%02X:%02X:%02X:%02X:%02X:%02X",(unsigned char)ifr.ifr_addr.sa_data[0],(unsigned char)ifr.ifr_dstaddr.sa_data[1],
            (unsigned char)ifr.ifr_addr.sa_data[2],(unsigned char)ifr.ifr_dstaddr.sa_data[3],(unsigned char)ifr.ifr_dstaddr.sa_data[4],(unsigned char)ifr.ifr_dstaddr.sa_data[5]);

    close(sock);
    return 0;
}

int get_mac_add(char *ethNo,unsigned char *mac_add )
{
    int sockfd;
    struct ifreq struReq;

    sockfd = socket(PF_INET,SOCK_STREAM,0);
    if(sockfd < 0){
        perror("socket create error");
        return(-1);
    }

    memset(&struReq,0,sizeof(struReq));
    strncpy(struReq.ifr_name, ethNo, sizeof(struReq.ifr_name));
    if(ioctl(sockfd,SIOCGIFHWADDR,&struReq) == -1){
        perror("get SIOCGIFHWADDR fail!\n");
        close(sockfd);
        return(-1);
    }

    *mac_add++ = (unsigned char)struReq.ifr_hwaddr.sa_data[0];
    *mac_add++ = (unsigned char)struReq.ifr_hwaddr.sa_data[1];
    *mac_add++ = (unsigned char)struReq.ifr_hwaddr.sa_data[2];
    *mac_add++ = (unsigned char)struReq.ifr_hwaddr.sa_data[3];
    *mac_add++ = (unsigned char)struReq.ifr_hwaddr.sa_data[4];
    *mac_add++ = (unsigned char)struReq.ifr_hwaddr.sa_data[5];

    close(sockfd);
    return(0);
}
#endif
int get_ip(char *ethNo,unsigned char *ip)
{
#ifdef __linux__
    int sock;
    struct sockaddr_in sin;
    struct ifreq ifr;
	ip[0] = 0;
	ip[1] = 0;
	ip[2] = 0;
	ip[3] = 0;
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
    {
        perror("socket");
        return -1;
    }

    strncpy(ifr.ifr_name, ethNo, IFNAMSIZ);//这个是网卡的标识符

    ifr.ifr_name[IFNAMSIZ - 1] = 0;

    if (ioctl(sock, SIOCGIFADDR, &ifr) < 0)
    {
        perror("get ip");
        close(sock); //cwenfu 20120410
        return -1;
    }
    memcpy(&sin, &ifr.ifr_addr, sizeof(sin));

    *ip = (unsigned char)(sin.sin_addr.s_addr >> 0  );
    *(ip+1) = (unsigned char)(sin.sin_addr.s_addr >> 8  );
    *(ip+2) = (unsigned char)(sin.sin_addr.s_addr >> 16  );
    *(ip+3) = (unsigned char)(sin.sin_addr.s_addr >> 24  );
    close(sock); //cwenfu 20120410
#endif
    return 0;
}
int get_netmask(char *ethNo,unsigned char *netMask) //
{
    int sock;
#ifdef __linux__
    struct sockaddr_in sin;
    struct ifreq ifr;
    //strncpy((char*)netMask,"0.0.0.0",8);
	netMask[0] = 0;
	netMask[1] = 0;
	netMask[2] = 0;
	netMask[3] = 0;
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
    {
        perror("socket");
        return -1;
    }
    strncpy(ifr.ifr_name, ethNo, IFNAMSIZ);/*这个是网卡的标识符*/
    ifr.ifr_name[IFNAMSIZ - 1] = 0;
    if (ioctl(sock, SIOCGIFNETMASK, &ifr)< 0)
    {
        perror("sGet_Net_Mask");
        close(sock);
        return -1;
    }
    memcpy(&sin, &ifr.ifr_addr, sizeof(sin));
    //snprintf((char *)netMask,16, "%.15s",inet_ntoa(sin.sin_addr));
	netMask[0] = (unsigned char)(sin.sin_addr.s_addr >> 0  );
    netMask[1] = (unsigned char)(sin.sin_addr.s_addr >> 8  );
    netMask[2] = (unsigned char)(sin.sin_addr.s_addr >> 16  );
    netMask[3] = (unsigned char)(sin.sin_addr.s_addr >> 24  );
    //printf("ZWYM = %s\n",netMask);
    close(sock);
#endif
    return 0;
}

//验证ip合法性
int sValid_Address(unsigned char *str)
{
    struct in_addr addr;
    int ret;
    ret = inet_pton(AF_INET, (const char*)str, &addr);
    return ret;
}

int sValid_SubnetMask(unsigned char* subnet,unsigned char* pStr)
{
    if(sValid_Address(subnet)>0)
    {
        unsigned int b = 0;
        //将子网掩码存入32位无符号整型
        b = * (pStr+3);
        b += *(pStr+2) <<8;
        b += *(pStr+1) <<16;
        b += *pStr <<24;
        b = ~b + 1;
        if((b & (b - 1)) == 0)   //判断是否为2^n
            return 1;
    }

    return 0;
}



int sGet_GateWay(char *ethNo,unsigned char *gateway)
{
    FILE *fp;
    char buf[512];
    char iface[16];
    unsigned long dest_addr, gate_addr;

    strncpy((char*)gateway,"0.0.0.0",8);
    fp = fopen("/proc/net/route", "r");
    if (fp == NULL)
        return -1;
    /* Skip title line */
    fgets(buf, sizeof(buf), fp);
    while(fgets(buf, sizeof(buf), fp)) {
        if(sscanf(buf, "%s\t%lX\t%lX", iface, &dest_addr, &gate_addr) != 3 || dest_addr != 0){
            continue;
        }
        if(strncmp(iface,ethNo,strlen(iface))!=0)
        {
            continue;
        }
        snprintf((char*)gateway,16, "%d.%d.%d.%d",(unsigned char)((gate_addr >> 0)&0x000000ff), (unsigned char)((gate_addr >> 8)&0x000000ff), (unsigned char)((gate_addr >> 16)&0x000000ff), (unsigned char)((gate_addr >> 24 )&0x000000ff));
        //printf("sxxxxxxxxxxxxxxxxxxxxxxxxxxGet_GateWay = %s\n",gateway);
        break;
    }
    fclose(fp);
    return 0;
}

int sGet_IP_Address(char *ethName,unsigned char *IPAddress)
{
    int sock;
    struct sockaddr_in sin;
#ifdef __linux__
    struct ifreq ifr;
    strncpy((char*)IPAddress,"0.0.0.0",8);
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
    {
        perror("socket");
        return -1;
    }
    strncpy(ifr.ifr_name, ethName, IFNAMSIZ);/*这个是网卡的标识符*/
    ifr.ifr_name[IFNAMSIZ - 1] = 0;
    if (ioctl(sock, SIOCGIFADDR, &ifr) < 0)
    {
        perror("sGet_Ip_Address");
        close(sock);
        return -1;
    }
    memcpy(&sin, &ifr.ifr_addr, sizeof(sin));
    snprintf((char *)IPAddress,16, "%.15s",inet_ntoa(sin.sin_addr));
    //printf("IP Address = %s\n",IPAddress);
    close(sock);
#endif
    return 0;
}

int sGet_Net_Mask(char *ethNo,unsigned char *netMask) //add by cwenfu 20120509
{
    int sock;
#ifdef __linux__
    struct sockaddr_in sin;
    struct ifreq ifr;
    strncpy((char*)netMask,"0.0.0.0",8);
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
    {
        perror("socket");
        return -1;
    }
    strncpy(ifr.ifr_name, ethNo, IFNAMSIZ);/*这个是网卡的标识符*/
    ifr.ifr_name[IFNAMSIZ - 1] = 0;
    if (ioctl(sock, SIOCGIFNETMASK, &ifr)< 0)
    {
        perror("sGet_Net_Mask");
        close(sock);
        return -1;
    }
    memcpy(&sin, &ifr.ifr_addr, sizeof(sin));
    snprintf((char *)netMask,16, "%.15s",inet_ntoa(sin.sin_addr));
    //printf("ZWYM = %s\n",netMask);
    close(sock);
#endif
    return 0;
}


int ascii_to_number(unsigned char *msk_hex,unsigned char *msk_ascii)
{
     unsigned char *p = NULL;
    unsigned char *dest = NULL;
    int number=0;
    //unsigned char sum=0;
    int sum=0;

    if(msk_ascii == NULL)
        return -1;

    dest = msk_hex;
    for (p = msk_ascii; *p != '\0'; p++)
    {
        if (*p >= '0' && *p  <= '9')
        {

            if (number == 0)
            {
                number = 1;
                sum = (*p-48);
            }
            else
            {
                sum = (sum * 10 + (*p-48));
            }
        }
        else
        {
            number=0;
            if(-1 != sum)
            {
                *dest = (unsigned char)sum;
                dest++ ;
            }
            sum=-1;
        }
    }
    *dest = sum;

    return 0;
}


int if_a_string_is_a_valid_ipv4_address(const char *str,char ip_check)  
{  
    struct in_addr addr;  
    int ret;  
    volatile int local_errno;  
	//unsigned long ip;
    errno = 0;  
    ret = inet_pton(AF_INET, str, &addr);  
    local_errno = errno;  
    if (ret > 0)
	{
		if(ip_check)//判断IP地址不是子网掩码
		{
			if( ((addr.s_addr&0xff) == 0)|| ((addr.s_addr&0xff) == 255)||
				( ((addr.s_addr>>24)&0xff) == 0)||( ((addr.s_addr>>24)&0xff) == 255)
			)
			return 0;//首字节或末字节为0或255无效
		}
	} 
    else if (ret < 0)  
        printf("EAFNOSUPPORT: %s\n", strerror(local_errno));  
    else  
        printf("\"%s\" is not a valid IPv4 address\n", str);  	
    return ret;
}

int IsSubnetMask(char* subnet)  
{  
	if(if_a_string_is_a_valid_ipv4_address (subnet,0)>0)  
	{  
		unsigned int b = 0, i, n[4];  
		sscanf(subnet, "%u.%u.%u.%u", &n[3], &n[2], &n[1], &n[0]);  
		for(i = 0; i < 4; ++i) //将子网掩码存入32位无符号整型  
			b += n[i] << (i * 8);   
		b = ~b + 1;  
		if((b & (b - 1)) == 0)   //判断是否为2^n  
			return 1;  
	}  
	return 0;  
}

NET_SETTING_286* netinfo;
NET_SETTING_286  cur_286_netinfo;
#ifdef FREESCALE
int System_net_setting_init(void)
{
    UINT16 ret, ret1;
    UINT32 fileLen;


    ret = readWholeFile((UINT8 *)F_SETNET, (UINT8 *)netinfo, &fileLen);
    printf("netinfo->isdhcp=%d\n",netinfo->isdhcp);
	cur_net_setting.dhcp = netinfo->isdhcp;//1---dhcp 0--static
	printf("netinfo->mac_address=%s\n",netinfo->mac_address);	
	if(0 == netinfo->isdhcp)//静态IP
    {
		printf("netinfo->IP_address=%s\n",netinfo->IP_address);
		printf("netinfo->msk_address=%s\n",netinfo->msk_address);
		printf("netinfo->gate_address=%s\n",netinfo->gate_address);
		sscanf(netinfo->IP_address, "%u.%u.%u.%u", &cur_net_setting.ip_addr[0], &cur_net_setting.ip_addr[1], &cur_net_setting.ip_addr[2], &cur_net_setting.ip_addr[3]);  
		sscanf(netinfo->msk_address, "%u.%u.%u.%u", &cur_net_setting.net_mask[0], &cur_net_setting.net_mask[1], &cur_net_setting.net_mask[2], &cur_net_setting.net_mask[3]);  
		sscanf(netinfo->gate_address, "%u.%u.%u.%u", &cur_net_setting.gate_way[0], &cur_net_setting.gate_way[1], &cur_net_setting.gate_way[2], &cur_net_setting.gate_way[3]);  
		
	}
    if(OK == ret){
        printf("read net setting File sucess\n");        
        return OK;
    }

    printf("read net setting back File \n");
    ret = readWholeFile((UINT8 *)F_BAK_SETNET, (UINT8 *)netinfo, &fileLen);   /* 若读取错误，读取备份文件 */
    if(OK == ret)
	{
		cur_net_setting.dhcp = netinfo->isdhcp;//1---dhcp 0--static
		sscanf(netinfo->IP_address, "%u.%u.%u.%u", &cur_net_setting.ip_addr[0], &cur_net_setting.ip_addr[1], &cur_net_setting.ip_addr[2], &cur_net_setting.ip_addr[3]);  
		sscanf(netinfo->msk_address, "%u.%u.%u.%u", &cur_net_setting.net_mask[0], &cur_net_setting.net_mask[1], &cur_net_setting.net_mask[2], &cur_net_setting.net_mask[3]);  
		sscanf(netinfo->gate_address, "%u.%u.%u.%u", &cur_net_setting.gate_way[0], &cur_net_setting.gate_way[1], &cur_net_setting.gate_way[2], &cur_net_setting.gate_way[3]);  
				
		UpdateEthPara();
       
        ret1 = copyFile((UINT8 *)F_BAK_SETNET, (UINT8 *)F_SETNET);   /* 拷贝副本到正本 */
        if(ERROR16 == ret1){
            printf("copy net setting back File error\n");
        }else{
            printf("copy net setting back File ok\n");
        }
        return OK;
    }else
	{
        netinfo->isdhcp=1;//动态
		cur_net_setting.dhcp = 1;
        /*strcpy(netinfo->IP_address,"192.168.162.123");
        strcpy(netinfo->msk_address,"255.255.255.0");
        strcpy(netinfo->gate_address,"192.168.162.1");*/
       // strcpy(netinfo->mac_address,"30:89:99:00:00:00");
        //00:3C:00:00:00:00 00:E0:5C:00:00:00
        //sConfigure_Net();
        printf("read net setting back File error, auto reset\n");
		UpdateEthPara();
        return OK;
    }
}

int savenetinfo()
{
    UINT16 ret;
    UINT32 fileLen;

    fileLen = sizeof(*netinfo);

    ret = writeFile((UINT8 *)F_SETNET, (UINT8 *)netinfo, fileLen, 0, 0);
    if (ERROR16 == ret){
        perror("Save net setting: write File error\n");
        return ERROR16;
    }
    else
        printf("Save net setting:write File success\n");

    ret = copyFile((UINT8 *)F_SETNET, (UINT8 *)F_BAK_SETNET);
    if (ERROR16 == ret){
        printf("Save net setting:copy File error\n");
    }
    else
        printf("Save net setting:copy file success\n");

    return OK;
}

void UpdateEthPara(void)
{ 
  
	char tmp[100]="";
    int dhcpSet= cur_net_setting.dhcp;
    char ip[64];	
    char netmask[64];	
    char gateway[64];
	
    if(dhcpSet==0 )//静态IP
    {
        sprintf(ip,"%d.%d.%d.%d",cur_net_setting.ip_addr[0],cur_net_setting.ip_addr[1],cur_net_setting.ip_addr[2],cur_net_setting.ip_addr[3]);
		sprintf(netmask,"%d.%d.%d.%d",cur_net_setting.net_mask[0],cur_net_setting.net_mask[1],cur_net_setting.net_mask[2],cur_net_setting.net_mask[3]);
		sprintf(gateway,"%d.%d.%d.%d",cur_net_setting.gate_way[0],cur_net_setting.gate_way[1],cur_net_setting.gate_way[2],cur_net_setting.gate_way[3]);
	
		if((if_a_string_is_a_valid_ipv4_address(ip,1)>0)&&(IsSubnetMask(netmask)>0)&&(if_a_string_is_a_valid_ipv4_address(gateway,1)>0))
		{
			 sprintf(tmp, "echo export SYSCFG_IFACE0=y>/etc/rc.d/rs.conf");
			 system(tmp);
			 sprintf(tmp, "echo export INTERFACE0=\"%s\">>/etc/rc.d/rs.conf",MAC_INTERFACE0);//m_netname);
			 system(tmp);

			 sprintf(tmp, "echo export IPADDR0=\"%s\">>/etc/rc.d/rs.conf", ip);
			 system(tmp);
			 sprintf(tmp, "echo export NETMASK0=\"%s\">>/etc/rc.d/rs.conf", netmask);
			 system(tmp);
			 sprintf(tmp, "echo export GATEWAY0=\"%s\">>/etc/rc.d/rs.conf", gateway);
			 system(tmp);
			 fflush(NULL);
		}
    }
    else//DHCP
    {    
		 sprintf(tmp, "echo export SYSCFG_IFACE0=y>/etc/rc.d/rs.conf");
		 system(tmp);
		 sprintf(tmp, "echo export INTERFACE0=\"%s\">>/etc/rc.d/rs.conf",MAC_INTERFACE0);//m_netname);
		 system(tmp);
		 sprintf(tmp, "echo export IPADDR0=\"%s\">>/etc/rc.d/rs.conf", "dhcp");
		 system(tmp);
    }
	savenetinfo();
}
#endif

