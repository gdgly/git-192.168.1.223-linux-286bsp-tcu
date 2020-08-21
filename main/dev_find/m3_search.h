//***************************************************************************** 
//
// M3�豸����̫��discovery������Э��Ӧ��
//
//*****************************************************************************

#ifndef __ETH_DEVICE_CONFIG_H__
#define __ETH_DEVICE_CONFIG_H__

/////////////////////////////////////////////////////////////////////////////////


#define M3_DEVICE_DISCOVERY   0x0809  //�Զ���M3�����豸�ķ���������ʹ�õ�ARPЭ���е�Э�������ֶ�����������(��IP���ݱ�0x0800)����ͻ----


//M3����֡������
#define M3_DEVICE_DISCOVERY_REQ       0x0001
#define M3_DEVICE_DISCOVERY_ACK       0x0002
#define M3_DEVICE_MAC_IP_SET_REQ      0x0003
#define M3_DEVICE_MAC_IP_SET_ACK      0x0004
#define M3_DEVICE_PARA_SET_REQ        0x0005
#define M3_DEVICE_PARA_SET_ACK        0x0006
#define M3_DEVICE_RM_CTL_REQ          0x0007
#define M3_DEVICE_RM_CTL_ACK          0x0008
//��̫��Զ�̿���������
#define M3_RESTART   0xF0
#define M3_SHUTDOWN  0xFF
/////////////////////////////////////////////////////////////////////////////////

//���������ٶȳ���
#define  M3_ETH_SPEED_AUTO    	 0
#define  M3_ETH_SPEED_10M_FULL   1
#define  M3_ETH_SPEED_10M_HALF   2
#define  M3_ETH_SPEED_100M_FULL  3
#define  M3_ETH_SPEED_100M_HALF  4

typedef enum
{
	MCU_UART_BAUD1200 = 0x00,//������1200
	MCU_UART_BAUD2400 = 0x01,//������2400
	MCU_UART_BAUD4800 = 0x02,//������4800
	MCU_UART_BAUD9600 = 0x03,//������9600
	MCU_UART_BAUD14400 = 0x04,//������14400
	MCU_UART_BAUD19200 = 0x05,//������19200
	MCU_UART_BAUD38400 = 0x06,//������38400
	MCU_UART_BAUD57600 = 0x07,//������57600
	MCU_UART_BAUD115200 = 0x08,//������115200
	
}eMCU_BAUD;

enum
{
	M3_NONE_PARITY = 0x01,//��У��
	M3_EVEN_PARITY = 0x02,//żУ��
	M3_ODD_PARITY = 0x03,//��У��	
};

//����UARTͨ�Ų����ṹ
struct M3_UART_DCB{
	unsigned char baud;//0x00��1200, 0x01��2400,0x02��4800, 0x03��9600,0x04��14400, 0x05��19200,0x06-38400,0x07-57600,0x08-115200
	unsigned char bytesize;// 0x08---8λ, 0x09---9λ
	unsigned char stopbits;//0x01��1λ, 0x02��2λ
	unsigned char parity;//0x01��ʾ����λ��У�飬0x02��ʾУ��λΪżУ�飬0x03��ʾУ��λΪ��У��
};

typedef struct 
{
	u8_t  ip_addr[4];//�豸��IP��ַ
	u8_t  net_mask[4];//��������
	u8_t  gate_way[4];//���ص�ַ
	u8_t  dev_ip_method;//IP��ַ��ȡ��ʽ��,0---DHCP, 1---��̬
	u8_t  m3_eth_mode;//���ڹ���ģʽ����,0---�Զ�Э��(Ĭ��),1-10M Full, 2-10M Half,3-100M Full,4-100M Half
	u8_t  dev_type[2];//2�ֽ��豸����
	u8_t  dev_info_len[2];//2�ֽ��豸��Ϣ����	

	char dev_name[32];//�豸����32�ֽ�
	char dev_sw_ver[32];//�豸�̼��汾32�ֽ�	
	char dev_manufacturer[64];//�豸������	
	char dev_location[64];//�豸ʹ�õص�
	char dev_user_info[64];//�û��Զ�����Ϣ
	char dev_time[32];//�豸��ǰʱ����Ϣ
	char dev_serial_num[20];//�豸���кţ����20�ֽ�
	
	
	struct M3_UART_DCB   uart1_dcb;	
	unsigned char uart1_addr;//uart1��ΪMODBUS�ӻ�ʱ�ĵ�ַ
	unsigned char uart1_reserved[11];
	
	struct M3_UART_DCB   uart2_dcb;
	unsigned char uart2_addr;//uart2��ΪMODBUS�ӻ�ʱ�ĵ�ַ
	unsigned char uart2_reserved[11];
	
	
	
	struct M3_UART_DCB   uart3_dcb;
	unsigned char uart3_addr;//uart3��ΪMODBUS�ӻ�ʱ�ĵ�ַ	
	unsigned char uart3_reserved[11];
	
	
	struct M3_UART_DCB   uart4_dcb;
	unsigned char uart4_addr;//uart4��ΪMODBUS�ӻ�ʱ�ĵ�ַ
	unsigned char uart4_reserved[11];
	
	struct M3_UART_DCB   uart5_dcb;
	unsigned char uart5_addr;//uart5��ΪMODBUS�ӻ�ʱ�ĵ�ַ
	unsigned char uart5_reserved[11];
	
	unsigned char can_bitrate[2];
	unsigned char can_reserved[14];	
	
}M3_DISCOVERY_RESP,*PM3_DISCOVERY_RESP;//M3����֡����Ӧ֡�ṹ

typedef struct 
{
	u8_t  new_MAC[6];//��MAC��ַ
	u8_t  new_ip_addr[4];//
	u8_t  new_net_mask[4];//��������
	u8_t  new_gate_way[4];//���ص�ַ
	u8_t  new_dev_ip_method;//IP��ַ��ȡ��ʽ��,0---DHCP, 1---��̬
	
}M3_MAC_IP_SET_REQ,*PM3_MAC_IP_SET_REQ;//

typedef struct 
{
	u8_t  new_ip_addr[4];//
	u8_t  new_net_mask[4];//��������
	u8_t  new_gate_way[4];//���ص�ַ
	u8_t  new_dev_ip_method;//IP��ַ��ȡ��ʽ��,0---DHCP, 1---��̬
	
}M3_MAC_IP_SET_RESP,*PM3_MAC_IP_SET_RESP;//

typedef struct 
{
	u8_t  dev_type[2];//2�ֽ��豸����
	u8_t  dev_info_len[2];//2�ֽ��豸��Ϣ����	

	char dev_name[32];//�豸����32�ֽ�
	char dev_sw_ver[32];//�豸�̼��汾32�ֽ�	
	char dev_manufacturer[64];//�豸������	
	char dev_location[64];//�豸ʹ�õص�
	char dev_user_info[64];//�û��Զ�����Ϣ
	char dev_time[32];//�豸��ǰʱ����Ϣ
	char dev_serial_num[20];//�豸���кţ����20�ֽ�
	
	
	struct M3_UART_DCB   uart1_dcb;	
	unsigned char uart1_addr;//uart1��ΪMODBUS�ӻ�ʱ�ĵ�ַ
	unsigned char uart1_reserved[11];
	
	struct M3_UART_DCB   uart2_dcb;
	unsigned char uart2_addr;//uart2��ΪMODBUS�ӻ�ʱ�ĵ�ַ
	unsigned char uart2_reserved[11];
	
	
	
	struct M3_UART_DCB   uart3_dcb;
	unsigned char uart3_addr;//uart3��ΪMODBUS�ӻ�ʱ�ĵ�ַ	
	unsigned char uart3_reserved[11];
	
	
	struct M3_UART_DCB   uart4_dcb;
	unsigned char uart4_addr;//uart4��ΪMODBUS�ӻ�ʱ�ĵ�ַ
	unsigned char uart4_reserved[11];
	
	struct M3_UART_DCB   uart5_dcb;
	unsigned char uart5_addr;//uart5��ΪMODBUS�ӻ�ʱ�ĵ�ַ
	unsigned char uart5_reserved[11];
	
	unsigned char can_bitrate[2];
	unsigned char can_reserved[14];	
	
}M3_SET_PARA_REQ,*PM3_SET_PARA_REQ;//


extern int eth_device_packetin(char* recv_buf,int rx_len,char* tx_buf);//������յ���M3�豸����̫��discovery������֡



#endif // __ETH_DEVICE_CONFIG_H__
