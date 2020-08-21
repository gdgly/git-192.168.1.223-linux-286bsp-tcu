
#include <sys/prctl.h>
#include <stdlib.h>

#include <net/route.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <net/if.h>       /* ifreq struct */
#include <netdb.h>
#include <linux/netlink.h> 
#include <linux/rtnetlink.h>    //for rtnetlink 


#ifndef __MACSEARCH_PUBLIC_H__
#define __MACSEARCH_PUBLIC_H__


#define  FREESCALE
//#define  NUC972

// #define  __linux__
#define ETH_MIN_LEN   60  //配置协议规定最小以太网帧的大小为60字节(不含CRC)

typedef unsigned char u8_t;
typedef unsigned short u16_t;

#define HTONS(n) (u16_t)((((u16_t) (n)) << 8) | (((u16_t) (n)) >> 8))

struct uip_eth_addr {
  u8_t addr[6];
};


/**
 * The Ethernet header.
 */
struct uip_eth_hdr {
  struct uip_eth_addr dest;
  struct uip_eth_addr src;
  u16_t type;
};//14 bytes

#define UIP_ETHTYPE_ARP 0x0806
#define UIP_ETHTYPE_IP  0x0800
#define UIP_ETHTYPE_IP6 0x86dd

#define ARP_REQUEST 1
#define ARP_REPLY   2
#define ARP_HWTYPE_ETH 1

struct arp_hdr {
  struct uip_eth_hdr ethhdr;
  u16_t hwtype;
  u16_t protocol;
  u8_t hwlen;
  u8_t protolen;
  u16_t opcode;
  struct uip_eth_addr shwaddr;
  u16_t sipaddr[2];
  struct uip_eth_addr dhwaddr;
  u16_t dipaddr[2];
};

struct iSearch_config_hdr {
  struct uip_eth_hdr ethhdr;//dest mac and src mac and eth type 
  u16_t hwtype;
  u16_t protocol;
  u8_t pad[1036];
};


typedef struct{
	
	unsigned char ethName[20];//"eth0"...
	unsigned char assign_method;//0---静态设置IP,1---动态设置IP
	unsigned char IP_address[20];//192.168.0.123
	unsigned char msk_address[20];//255.255.255.0
	unsigned char gate_address[20];//192.168.0.1
	unsigned char main_DNS_address[20];//8.8.8.8
	unsigned char minor_DNS_address[20];//1.1.1.1
	unsigned char mac_add[8];
}NET_SETTING;


typedef struct _NET_SETTING{
    char IP_address[16];
    char msk_address[16];
    char gate_address[16];
    char mac_address[18];
    unsigned char isdhcp;
}NET_SETTING_286;//IMX286

typedef struct {
//当前的运行IP参数
	char  dhcp;//1---dhcp  0--static
	char  ip_addr[4];
	char  gate_way[4];
	char  net_mask[4];
}CUR_NET_SETTING;

#ifndef BUFSIZE
#define BUFSIZE 8192
#endif

struct route_info
{
    u_int dstAddr;
    u_int srcAddr;
    u_int gateWay;
    char ifName[IF_NAMESIZE];
};

/*
*
*/
int read_nlSock(int sockFd, char *bufPtr, int seqNum, int pId);


/*
*
*/
int parse_routes(struct nlmsghdr *nlHdr, struct route_info *rtInfo, const char *eth_name, char *gateway);


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
int get_ip_mask(const char *eth_name, char *ip, char *mask);


/********************************************************************
* get_gateway: get the gateway, specify eth name
*
* eth_name： eth name, not null
* gateway: gateway, not null
*
* return： 0 succ
*         -1 fail
********************************************************************/
int get_gateway(const char *eth_name, char *gateway);


/*获取IP地址获取状态，0为静态，1为动态*/
int getDhcpState(char *ethName);

int sGet_MAC(char *ethNo,unsigned char *macaddr);

int get_mac_add(char *ethNo,unsigned char *mac_add );

int get_ip(char *ethNo,unsigned char *ip);

int get_netmask(char *ethNo,unsigned char *netMask);


//验证ip合法性
int sValid_Address(unsigned char *str);

int sValid_SubnetMask(unsigned char* subnet,unsigned char* pStr);

int sGet_GateWay(char *ethNo,unsigned char *gateway);

int sGet_IP_Address(char *ethName,unsigned char *IPAddress);

int sGet_Net_Mask(char *ethNo,unsigned char *netMask);

int ascii_to_number(unsigned char *msk_hex,unsigned char *msk_ascii);

int if_a_string_is_a_valid_ipv4_address(const char *str,char ip_check) ;

int IsSubnetMask(char* subnet) ;

void UpdateEthPara(void);
void Uppercase(char* in,char*out);
int resetNetwork(NET_SETTING *net_set);
int savenetinfo();//保存网络参数

extern NET_SETTING NET_setting;
extern NET_SETTING_286 cur_286_netinfo,*netinfo;
extern CUR_NET_SETTING cur_net_setting;

extern const char* const DEV_SW_VER;
extern const char* const DEV_HW_VER;

#endif