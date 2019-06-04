/****************************************Copyright (c)****************************************************
**                               DongGuan EAST Co.,LTD.
**                                     
**                                 http://www.EASTUPS.com
**
**--------------File Info---------------------------------------------------------------------------------
** File Name:               InDTU332W_Driver.h
** Last modified Date:      2015.11.13
** Last Version:            1.0
** Description:             GPRSģ��InDTU332Wͨ�Žӿ�
** 
**--------------------------------------------------------------------------------------------------------
** Created By:              
** Created date:            2015.11.13
** Version:                 1.0
** Descriptions:            The original version ��ʼ�汾
**
**--------------------------------------------------------------------------------------------------------
*********************************************************************************************************/

#ifndef  __InDTU332W_DRIVER_H__
#define  __InDTU332W_DRIVER_H__


//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C" {
#endif

#define InDTU332W_RX_FR_MIN_LEN 8// 9//�޸�Ϊ9���ֽ�20151215//10
#define DTU_UART_BAUD   dev_cfg.dev_para.uart2_dcb.baud

typedef enum
{
	InDTU332W_UART_BAUD9600 = 0x00,//������9600
	InDTU332W_UART_BAUD300 = 0x01,//������300
	InDTU332W_UART_BAUD600 = 0x02,//������600
	InDTU332W_UART_BAUD1200 = 0x03,//������1200
	InDTU332W_UART_BAUD2400 = 0x04,//������2400
	InDTU332W_UART_BAUD4800 = 0x05,//������4800
	//InDTU332W_UART_BAUD9600 = 0x06,//������9600
	InDTU332W_UART_BAUD14400 = 0x07,//������14400
	InDTU332W_UART_BAUD19200 = 0x08,//������19200
	
	InDTU332W_UART_BAUD38400 = 0x09,//������38400
	InDTU332W_UART_BAUD56000 = 0x0A,//������56000
	InDTU332W_UART_BAUD57600 = 0x0B,//������57600
	InDTU332W_UART_BAUD115200 = 0x0C,//������115200
	
	
}eInDTU332W_BAUD;

typedef enum
{
	InDTU332W_SCAN = 0x03,//̽��GPRSģ��
	InDTU332W_LOGIN=0x05, //��¼
	InDTU332W_RESET=0x12, //����
	InDTU332W_ACTIVE=0x14, //����GPRSģ������GPRS
	InDTU332W_RD_STATUS = 0x15,//��ȡ״̬
	InDTU332W_RD_SINGLE_CFG=0x16, //��ȡ��������
	InDTU332W_WR_SINGLE_CFG=0x18, //д�뵥������	
	
}eInDTU332W_CMD;

typedef enum
{
	TAG_GPRS_APN = 0x0002,//APN��ASCIIֵ,��cmnet
	
	TAG_UART1_BAUD=0x0011, //����1������
	TAG_UART1_DATABITS=0x0012, //����1����λ
	TAG_UART1_STOPBITS=0x0013, //����1ֹͣλ
	TAG_UART1_PARITY=0x0014, //����1У��λ
	
	TAG_CONNECT_METHOD = 0x001E,//���ӷ�ʽ,0�����ӣ�1������
	TAG_DATA_ACTIVE = 0x8007,//���ݼ���
	
	TAG_GATEWAY_ADDR = 0x0019,//��ҵ���ص�ַ��IP�Ӷ˿�
	TAG_GATEWAY_NAME = 0x8123,//��ҵ��������
	TAG_GATEWAY_CONN_METHOD = 0x001A,//��ҵ�������ӷ�ʽ��0x00��TCP����
	TAG_GATEWAY_CONN_ST = 0x8183,// ���ص�ǰ����״̬��ֵ0x04��ʾ������վ�ɹ�
	TAG_DNS_ADDR = 0x8019,//DNS��������ַ
	TAG_CSQ_ST = 0x8184,//�ź�ǿ��
	TAG_REGISTRATION_ST = 0x81A1,//ע��״̬
	TAG_ICCID_VALUE = 0x81D0,//sim��Ψһ��Ӧ��ID����20λ
}eInDTU332W_CFG_TAG;//�������TAG



typedef struct
{
	unsigned char header[4];//֡ͷ���͵�GPRSģ��0x55,0xAA,0x55,0xAA,GPRSģ�鷵��0xAA,0x55,0xAA,0x55
	unsigned char CMD;//������
	unsigned char data_len[2];//������ֽڳ���
	//unsigned char tlvs[n];//TLVS����	
	
}STR_GPRS_FRAME;//GPRSģ���շ�֡ͷ



//��ģ�麯������
extern unsigned char InDTU332W_Scan_Frame(unsigned char* tx_buf);//���GPRSģ��̽��֡,����֡�ĳ���
extern unsigned char InDTU332W_Active_Frame(unsigned char* tx_buf);//���GPRSģ�鼤������֡,����֡�ĳ���
extern unsigned char InDTU332W_Login_Frame(unsigned char* tx_buf,char* login_name,char* login_password);//���GPRSģ���¼֡,����֡�ĳ���

extern unsigned char InDTU332W_Reset_Frame(unsigned char* tx_buf);//���GPRSģ������֡,����֡�ĳ���
extern unsigned char InDTU332W_Single_RD_Frame(unsigned char* tx_buf, eInDTU332W_CFG_TAG  tag_read);//���GPRSģ���ȡ��������֡,����֡�ĳ���
extern unsigned char InDTU332W_Single_WR_Frame(unsigned char* tx_buf, eInDTU332W_CFG_TAG  tag_write);//���GPRSģ�����õ�������֡,����֡�ĳ���

extern unsigned char InDTU332W_Status_RD_Frame(unsigned char* tx_buf, eInDTU332W_CFG_TAG  tag_read);//���GPRSģ���ȡ����״̬TAG֡,����֡�ĳ���
//����ֵ0��ʾ��Ӧ֡OK,����ֵ1��ʾ��Ӧ֡�쳣
extern unsigned char InDTU332W_Response_Handler(unsigned char* rx_buf,unsigned short rx_len);//InDTU332W����֡����


extern eInDTU332W_CMD     	g_eInDTU332W_cmd;//���1�η��͵�֡����
extern eInDTU332W_CFG_TAG 	g_eInDTU332W_cfg_tag;

extern unsigned char    g_InDTU332W_exist_flag;//ֵ0��ʾδ��⵽GPRSģ��,ֵ1��ʾ��⵽GPRSģ��
extern unsigned char	g_InDTU332W_login_flag;//ֵ0��ʾ��¼��OK��ֵ1 OK
extern unsigned char	g_InDTU332W_conn_flag;//ֵ0��ʾ���ӷ�������OK��ֵ1 ��ʾ���ӷ�����OK

extern unsigned char  g_InDTU332W_gateway_ip[4];//������IP
extern unsigned short g_InDTU332W_gateway_port;//�������˿�

#ifdef __cplusplus
    }
#endif

#endif //__InDTU332W_DRIVER_H__

