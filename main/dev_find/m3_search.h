//***************************************************************************** 
//
// M3设备的以太网discovery和配置协议应用
//
//*****************************************************************************

#ifndef __ETH_DEVICE_CONFIG_H__
#define __ETH_DEVICE_CONFIG_H__

/////////////////////////////////////////////////////////////////////////////////


#define M3_DEVICE_DISCOVERY   0x0809  //自定义M3网络设备的发现与配置使用的ARP协议中的协议类型字段与其他类型(如IP数据报0x0800)不冲突----


//M3搜索帧操作码
#define M3_DEVICE_DISCOVERY_REQ       0x0001
#define M3_DEVICE_DISCOVERY_ACK       0x0002
#define M3_DEVICE_MAC_IP_SET_REQ      0x0003
#define M3_DEVICE_MAC_IP_SET_ACK      0x0004
#define M3_DEVICE_PARA_SET_REQ        0x0005
#define M3_DEVICE_PARA_SET_ACK        0x0006
#define M3_DEVICE_RM_CTL_REQ          0x0007
#define M3_DEVICE_RM_CTL_ACK          0x0008
//以太网远程控制命令码
#define M3_RESTART   0xF0
#define M3_SHUTDOWN  0xFF
/////////////////////////////////////////////////////////////////////////////////

//网卡工作速度常数
#define  M3_ETH_SPEED_AUTO    	 0
#define  M3_ETH_SPEED_10M_FULL   1
#define  M3_ETH_SPEED_10M_HALF   2
#define  M3_ETH_SPEED_100M_FULL  3
#define  M3_ETH_SPEED_100M_HALF  4

typedef enum
{
	MCU_UART_BAUD1200 = 0x00,//波特率1200
	MCU_UART_BAUD2400 = 0x01,//波特率2400
	MCU_UART_BAUD4800 = 0x02,//波特率4800
	MCU_UART_BAUD9600 = 0x03,//波特率9600
	MCU_UART_BAUD14400 = 0x04,//波特率14400
	MCU_UART_BAUD19200 = 0x05,//波特率19200
	MCU_UART_BAUD38400 = 0x06,//波特率38400
	MCU_UART_BAUD57600 = 0x07,//波特率57600
	MCU_UART_BAUD115200 = 0x08,//波特率115200
	
}eMCU_BAUD;

enum
{
	M3_NONE_PARITY = 0x01,//无校验
	M3_EVEN_PARITY = 0x02,//偶校验
	M3_ODD_PARITY = 0x03,//奇校验	
};

//定义UART通信参数结构
struct M3_UART_DCB{
	unsigned char baud;//0x00—1200, 0x01—2400,0x02—4800, 0x03—9600,0x04—14400, 0x05—19200,0x06-38400,0x07-57600,0x08-115200
	unsigned char bytesize;// 0x08---8位, 0x09---9位
	unsigned char stopbits;//0x01—1位, 0x02—2位
	unsigned char parity;//0x01表示检验位无校验，0x02表示校验位为偶校验，0x03表示校验位为奇校验
};

typedef struct 
{
	u8_t  ip_addr[4];//设备主IP地址
	u8_t  net_mask[4];//子网掩码
	u8_t  gate_way[4];//网关地址
	u8_t  dev_ip_method;//IP地址获取方式，,0---DHCP, 1---静态
	u8_t  m3_eth_mode;//网口工作模式配置,0---自动协商(默认),1-10M Full, 2-10M Half,3-100M Full,4-100M Half
	u8_t  dev_type[2];//2字节设备类型
	u8_t  dev_info_len[2];//2字节设备信息长度	

	char dev_name[32];//设备名称32字节
	char dev_sw_ver[32];//设备固件版本32字节	
	char dev_manufacturer[64];//设备制造商	
	char dev_location[64];//设备使用地点
	char dev_user_info[64];//用户自定义信息
	char dev_time[32];//设备当前时间信息
	char dev_serial_num[20];//设备序列号，最大20字节
	
	
	struct M3_UART_DCB   uart1_dcb;	
	unsigned char uart1_addr;//uart1作为MODBUS从机时的地址
	unsigned char uart1_reserved[11];
	
	struct M3_UART_DCB   uart2_dcb;
	unsigned char uart2_addr;//uart2作为MODBUS从机时的地址
	unsigned char uart2_reserved[11];
	
	
	
	struct M3_UART_DCB   uart3_dcb;
	unsigned char uart3_addr;//uart3作为MODBUS从机时的地址	
	unsigned char uart3_reserved[11];
	
	
	struct M3_UART_DCB   uart4_dcb;
	unsigned char uart4_addr;//uart4作为MODBUS从机时的地址
	unsigned char uart4_reserved[11];
	
	struct M3_UART_DCB   uart5_dcb;
	unsigned char uart5_addr;//uart5作为MODBUS从机时的地址
	unsigned char uart5_reserved[11];
	
	unsigned char can_bitrate[2];
	unsigned char can_reserved[14];	
	
}M3_DISCOVERY_RESP,*PM3_DISCOVERY_RESP;//M3搜索帧的响应帧结构

typedef struct 
{
	u8_t  new_MAC[6];//新MAC地址
	u8_t  new_ip_addr[4];//
	u8_t  new_net_mask[4];//子网掩码
	u8_t  new_gate_way[4];//网关地址
	u8_t  new_dev_ip_method;//IP地址获取方式，,0---DHCP, 1---静态
	
}M3_MAC_IP_SET_REQ,*PM3_MAC_IP_SET_REQ;//

typedef struct 
{
	u8_t  new_ip_addr[4];//
	u8_t  new_net_mask[4];//子网掩码
	u8_t  new_gate_way[4];//网关地址
	u8_t  new_dev_ip_method;//IP地址获取方式，,0---DHCP, 1---静态
	
}M3_MAC_IP_SET_RESP,*PM3_MAC_IP_SET_RESP;//

typedef struct 
{
	u8_t  dev_type[2];//2字节设备类型
	u8_t  dev_info_len[2];//2字节设备信息长度	

	char dev_name[32];//设备名称32字节
	char dev_sw_ver[32];//设备固件版本32字节	
	char dev_manufacturer[64];//设备制造商	
	char dev_location[64];//设备使用地点
	char dev_user_info[64];//用户自定义信息
	char dev_time[32];//设备当前时间信息
	char dev_serial_num[20];//设备序列号，最大20字节
	
	
	struct M3_UART_DCB   uart1_dcb;	
	unsigned char uart1_addr;//uart1作为MODBUS从机时的地址
	unsigned char uart1_reserved[11];
	
	struct M3_UART_DCB   uart2_dcb;
	unsigned char uart2_addr;//uart2作为MODBUS从机时的地址
	unsigned char uart2_reserved[11];
	
	
	
	struct M3_UART_DCB   uart3_dcb;
	unsigned char uart3_addr;//uart3作为MODBUS从机时的地址	
	unsigned char uart3_reserved[11];
	
	
	struct M3_UART_DCB   uart4_dcb;
	unsigned char uart4_addr;//uart4作为MODBUS从机时的地址
	unsigned char uart4_reserved[11];
	
	struct M3_UART_DCB   uart5_dcb;
	unsigned char uart5_addr;//uart5作为MODBUS从机时的地址
	unsigned char uart5_reserved[11];
	
	unsigned char can_bitrate[2];
	unsigned char can_reserved[14];	
	
}M3_SET_PARA_REQ,*PM3_SET_PARA_REQ;//


extern int eth_device_packetin(char* recv_buf,int rx_len,char* tx_buf);//处理接收到的M3设备的以太网discovery和配置帧



#endif // __ETH_DEVICE_CONFIG_H__
