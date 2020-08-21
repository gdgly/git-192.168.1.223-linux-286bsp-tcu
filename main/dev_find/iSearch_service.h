//***************************************************************************** 
//
// 实现对PC机iSearch软件的搜索和设置响应。
//
//*****************************************************************************

#ifndef __ISEARCH_SERVICE_H__
#define __ISEARCH_SERVICE_H__

/////////////////////////////////////////////////////////////////////////////////


#define ISEARCH_DEVICE_DISCOVERY   0xEA00  //自定义iSearch网络设备的发现与配置使用的ARP协议中的协议类型字段与其他类型(如IP数据报0x0800)不冲突----注意高字节固定为0xEA，低字节是变化的!!


//定义iSearch搜索帧的protocol字段值
#define ISEARCH_DEVICE_DISCOVERY_REQ       0xEA01//设备搜索，与0xEA05基本一样
#define ISEARCH_DEVICE_DISCOVERY_ACK       0xEA02

#define ISEARCH_DEVICE_ALIVE_CHK_REQ       0xEA03//检查设备是否依然在线
#define ISEARCH_DEVICE_ALIVE_CHK_ACK       0xEA04

#define ISEARCH_DEVICE_GET_INFO_REQ        0xEA05//获取IP地址，密码，端口等信息
#define ISEARCH_DEVICE_GET_INFO_ACK        0xEA06

#define ISEARCH_DEVICE_SET_INFO_REQ        0xEA07//设置IP地址，密码，端口等信息
#define ISEARCH_DEVICE_SET_INFO_ACK        0xEA08

// #define ISEARCH_DEVICE_SET_MAC_REQ         0xEA09 //手动设置MAC和序列号
// #define ISEARCH_DEVICE_SET_MAC_ACK         0xEA0A

#define ISEARCH_DEVICE_SET_MAC_REQ         0xEA1A //手动设置MAC
//#define ISEARCH_DEVICE_SET_MAC_ACK         0xEA1A

#define ISEARCH_DEVICE_SET_SN_REQ         0xEA19 //设置序列号
//#define ISEARCH_DEVICE_SET_SN_ACK         0xEA0A

#define ISEARCH_DEVICE_RESTORE_REQ         0xEA0B  //恢复出厂值
#define ISEARCH_DEVICE_RESTORE_ACK         0xEA0C  




/////////////////////////////////////////////////////////////////////////////////

extern int iSearch_Packetin(char* recv_buf,int rx_len,char* tx_buf);//处理接收到的iSearch软件的以太网discovery和配置帧



#endif // __ISEARCH_SERVICE_H__
