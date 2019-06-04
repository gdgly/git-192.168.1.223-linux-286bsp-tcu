/****************************************Copyright (c)****************************************************
**                               DongGuan EAST Co.,LTD.
**                                     
**                                 http://www.EASTUPS.com
**
**--------------File Info---------------------------------------------------------------------------------
** File Name:               type_def.h
** Last modified Date:      2012.12.7
** Last Version:            1.0
** Description:             ���幤�����õ��ĸ�����������
** 
**--------------------------------------------------------------------------------------------------------
** Created By:              
** Created date:            2012.12.7
** Version:                 1.0
** Descriptions:            The original version ��ʼ�汾
**
**--------------------------------------------------------------------------------------------------------
*********************************************************************************************************/

#ifndef  __TYPE_DEF_H__
#define  __TYPE_DEF_H__


//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C" {
#endif

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#define  SOFT_VER   0x0101 //V1.01,BCD��ʽ,��0x0311��ʾV3.11
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



#pragma pack (1) /*ָ����1�ֽڶ���*/
//����UARTͨ�Ų����ṹ
struct M3_UART_DCB{
	unsigned char baud;//0x00��1200, 0x01��2400,0x02��4800, 0x03��9600,0x04��14400, 0x05��19200,0x06-38400,0x07-57600,0x08-115200
	unsigned char bytesize;// 0x08---8λ, 0x09---9λ
	unsigned char stopbits;//0x01��1λ, 0x02��2λ
	unsigned char parity;//0x01��ʾ����λ��У�飬0x02��ʾУ��λΪżУ�飬0x03��ʾУ��λΪ��У��
};

typedef struct //��̫����������
{
	unsigned char  mac_addr[8];        //  ����MAC��ַ,��2�ֽڣ���FLASH����ռ��8���ֽ�
	unsigned char  ip_addr[4];         //  ����IPv4��ַ
	unsigned char  net_mask[4];        //  ����IP��ַ����
	unsigned char  gate_way[4];        //  ����IPv4��ַ
	//����������÷�ʽ(IP��ַ��ȡ��ʽDHCP��̬)
	//Bit0��IP��ȡ��ʽ,0-DHCP,1-��̬	
	unsigned char  m3_eth_config; 
	unsigned char  m3_eth_mode;//���ڹ���ģʽ����,0---�Զ�Э��(Ĭ��),1-10M Full, 2-10M Half,3-100M Full,4-100M Half
	unsigned char  reserved[10];//�ṹ��size 4�ֽڶ���
}ETH_PARA,*PETH_PARA;

enum SYS_EVENTS
{	
	eEMERGE_PRESSED_EVENT=0,//��ͣ��ť����
	eAC_VOLT_HIGH_FAULT,//������ѹ
	eAC_VOLT_LOW_FAULT ,//����Ƿѹ
	eAC_CURR_HIGH_FAULT ,//��������	
	eAC_PE_FAULT ,//�ӵع���
	eMETER_COMM_FAULT ,//���ͨ�Ź���
	eCARD_COMM_FAULT ,//ˢ����ͨ�Ź���
	eLCD_COMM_FAULT ,//LCDͨ�Ź���
	eGPS_COMM_FAULT ,//GPSͨ�Ź���
	eCAN_COMM_FAULT,//CANͨ�Ź���
		
};

enum COIN_TYPE_LIST
{	
	eRMB=0,//�����
	eDollar,//��Ԫ
	eEuro,//ŷԪ
	ePound,//Ӣ��	
	eTHB,//̩��
	eRS,//¬��
	eCOIN_TYPE_END = eRS,
};

typedef struct
{
	unsigned short year;//��	
	unsigned char month;//�� 1---12
	unsigned char day_of_month;//ĳ���е�ĳ�գ���3��2��, 1---31	
	
	unsigned char week_day;//���ڼ� 1---7  7-sunday,1-monday....6-saturday
	unsigned char hour;//Сʱ0---23
	unsigned char minute;//���� 0---59
	unsigned char second;//�� 0---59
	
}CHG_BIN_TIME;	//��ֵΪ�����Ƹ�ʽ

typedef struct   //�豸������������
{	
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
	
	unsigned short can_bitrate;
	// unsigned short volt_33;//���ÿ��Ƶ�Ԫ��3.3V��ʵ��ֵ�Ա�׼ȷ����PT1000���¶�
	// unsigned short current_low_shutdown;//��������������ڸ�ֵ��ض��������λmA,Ĭ��200mA
	unsigned char can_reserved[14];	
	
	unsigned char http_password[8];
	//�������׮��������
	unsigned char pwm_percent;//����PWM�ٷֱȣ���ֵ20��ʾ20%����Ӧ����16A
	unsigned char pe_det_en;//�ӵع��ϼ��ʹ�ܣ�ֵ0��ʹ�ܣ�����ֵʹ��	
	unsigned char LangSelect;//LCD����ѡ��,0---���ļ��壬1---Ӣ��
	unsigned char constrat;//�ԱȶȰٷֱ�
	
	unsigned short ac_volt_high_alarm;//��ѹ�澯ֵ����λVϵ��1
	unsigned short ac_volt_low_alarm;//Ƿѹ�澯ֵ����λVϵ��1
	
	unsigned short ac_current_high_shutdown;//��������ֵ�������İٷ�������120��ʾ�������120%ʱ�ضϳ���·
	unsigned short meter_type;//ֵ1��ʾ������ֵ3��ʾ3����
	unsigned long  password;//ϵͳ���õ�6λ������ֵ��Ĭ��123456
	
	unsigned long  UserPWD;//��ͨ�û����룬Ĭ��123456
	
	unsigned char light_on_hour;//���俪��Сʱλ
	unsigned char light_on_min;//���俪������λ
	unsigned char light_off_hour;//����ر�Сʱλ
	unsigned char light_off_min;//����رշ���λ
	
	unsigned short price_per_kwh[4];//4�����ʵ���Ԫ��С����2λ����ֵ88��ʾ0.88Ԫÿ�ȵ磬����Ϊ�⡢�塢ƽ����4����ۣ�ԭ���ϼ۸�����½�
	unsigned short service_price;//����ѣ�����Ԫ��С����2λ����ֵ10��ʾÿ�ȵ���ȡ0.10Ԫ�ķ����
	
	unsigned short time_zone[12];//48��ʱ��εķ���ѡ��time_zone[0]��ֵ��Ϊ0x1234���ʾ0:00-0:30Ϊ����1��0:30-1:00Ϊ����2��1:00-1:30Ϊ����3��1:30-2:00Ϊ����4
	unsigned short card_consume_en;//��Ƭ�۷�ʹ�ܣ�ֵ0��ʹ�ܣ�ֵ1ʹ�ܣ�Ĭ��ʹ��
	
	unsigned char  charger_ID[8];//���׮��ţ�16������λ��BCD�洢--��׮Э������Ϊ���׮��ַʹ��
	
	unsigned short service_price2;//������ȡ�ķ���ѣ�����Ԫ��С����2λ
	unsigned short service_price3;//�����ʱ��(ռ�ó��׮ʱ��)��ȡ�ķ���ѣ�����Ԫ��С����2λ��Ԫ/10����
	
	unsigned char  service_sel;//ѡ����ȡ����ѵķ�ʽ��ֵ3��ʾ��������ֵ1��ʾ������ȡ��ֵ2��ʾ��ʱ����ȡ(Ԫ/10����)
	unsigned char  card_dev_type;//ˢ�������ͣ�0-����ʽˢ���� 1-�ǲ���ʽˢ����	
	unsigned char  meter_feilv_type;//�Ƿ��Ƿ��ʱ� 0--���� 1--��
	//unsigned char  pad2;
	unsigned char delta_volt;//����Ƿѹ�澯�ز��λV��ϵ��1-------------
	
	unsigned char server1_ip[4];//BIN�룬������IP��ַ---1�ű���IP	
	unsigned short server1_port;//BIN�룬�ȴ����ֽ�---1�ű���IP�Ķ˿�
	unsigned char server2_ip[4];//BIN�룬������IP��ַ---2�ű���IP	
	unsigned short server2_port;//BIN�룬�ȴ����ֽ�---2�ű���IP�Ķ˿�
	unsigned short change_ip_flag;//�Ƿ�ָ��ʱ���л�������IP��ַ,ֵ1�ǣ�ֵ0��
	unsigned char m1_key_valid;//�Ƿ���ˢ����ע����Կ  pad2[1];
	unsigned char plug_lock_ctl;//�Ƿ�����ǹ������,ֵ0�����ã�ֵ1�����Ҽ����״̬��ֵ2���õ������״̬
	
	CHG_BIN_TIME  server_ip_change_time;//8�ֽ�,����������·��ķ�����IP����ָ��Ҫ����ָ��ʱ�����ķ�����IP���򽫸�ʱ�䱣���ڴ�
	
	unsigned char  time_zone_num;//ÿ���ʱ������1��12
	//unsigned char  pad3[1];//4�ֽڲ���
	unsigned char cp_fault_det_en;//ֵ0��ʹ��CP�ӵع��ϼ�⣬����ʹ��
	//unsigned char  emergy_stop_button;//�Ƿ��м�ͣ��ť��ֵ0��ʾ�ޣ�ֵ1��ʾ��
	unsigned char  leak_current_alarm;//©������ֵ����λmA��ϵ��1--------------
	unsigned char  conn_server_type;//���ӷ������ķ�ʽ��0-������ ,1-RJ45��̫��,2-DTU(��ӳ��ͨ)����,3-WIFI,4-USB 3G
	
	//unsigned short pad4[1];//����4�ֽڶ���
	unsigned short card_order_price;//ˢ��ԤԼ׮��ʱ����ڼ�ķ��õ��ۣ�Ԫ/���ӣ�ϵ��0.01,ֵ100��ʾ1.00Ԫÿ����
	unsigned short time_zone_feilv[3];//����12��ʱ��εķ��ʺ�ѡ��time_zone_feilv[0]��ֵ��Ϊ0x1234���ʾ��1��ʱ��ʹ�÷���1����2��ʱ��ʹ�÷���2����3��ʱ��ʹ�÷���3����4��ʱ��ʹ�÷���4
	
	unsigned short time_zone_tabel[12];//���12��ʱ���---���ֽڱ�ʾʱ��εĿ�ʼСʱ�������ֽڱ�ʾ��ʼ��������BIN��
	
	unsigned char m1_card_key[8];//��6�ֽ�Ϊ��ȡM1��Ƭ����Կ
	
	unsigned char soft_ver[8];//5���ֽڵ�ASCII�汾�ţ������ж��Ƿ���Ҫ�����������¹̼�,��ʼȫ'0'
	
	unsigned char send_soft_update_confirm;//�Ƿ����������ȷ��֡��������
	unsigned char emergy_button_en;//��ͣ��ťʹ�ܿ���
	unsigned char temp_sensor_en;//�¶ȴ�����ʹ�ܿ���
	unsigned char fanglei_en;//�Ƿ����÷��������
	
	unsigned short w_UgainA;//���Ƶ�ԪA���ѹ����
	unsigned short w_UgainB;//���Ƶ�ԪB���ѹ����
	unsigned short w_UgainC;//���Ƶ�ԪC���ѹ����
	unsigned short temp_High_shutdown_value;//20170228���ӹ���ֹͣ��磬��λ�棬ϵ��1 pad4;//
	
	unsigned char output_type;//�����ʽ 0-��ǹ��1-˫ǹ��2-����ǹ
	unsigned char setup_type;//��װ��ʽ 0-�ڹ�ʽ��1-���ʽ��2-��Яʽ��3-��ʽ
	unsigned short oem_code;//��˾���� 0-�����׵�(EV) 1-����(NW) 2-��ׯ(DZ),3-�Ϻ�ֿ��(ZD),4-��ʢ����(YS),5-����(ZE)...
	
	enum COIN_TYPE_LIST e_coin_type;//��������---0-RMB(��),1-��Ԫ($)��2-ŷԪ(�)��3-Ӣ��(��),4-̩��(THB)
	unsigned char car_lock_addr;
	unsigned char pad5[2];
	
	unsigned char phone_num[16];//11λ�Ĺ�����ϵ�ֻ��Ż�̶��绰����076988888888
	//unsigned char pad4[100];
	unsigned char black_list_ver[8];
	
	unsigned short car_lock_num;//��λ������0---2
	unsigned short car_lock_time;//��⵽�޳���ʱ�򣬳�λ������ʱ��,��λ���ӣ�Ĭ��3���ӣ��ڽ��³�����ʼ��ʱ����ʱ����Ȼ�޳������ϳ�λ
	unsigned long car_park_price;//ÿ10���ӵ�ͣ���ѣ���λԪ��ϵ��0.01
	
	unsigned short free_minutes_after_charge;//�����ɺ󻹿����ͣ����ʱ��,��λ����
	unsigned char  free_when_charge;//����ڼ��Ƿ���ͣ���ѣ�ֵ1��ѣ�ֵ0�����
	unsigned char emergy_button_type;//ֵ0��ʾ��������(�պϱ�ʾ��ͣ����),ֵ1��ʾ��������(�Ͽ���ʾ��ͣ����)
	
	
	unsigned short volt_33;//���ÿ��Ƶ�Ԫ��3.3V��ʵ��ֵ�Ա�׼ȷ����PT1000���¶�
	unsigned short current_low_shutdown;//��������������ڸ�ֵ��ض��������λmA,Ĭ��200mA
	
	
	unsigned short kwh_service_price[4];//�⡢�塢ƽ���ȷ�ʱ����ѣ�����Ԫ��С����2λ����ֵ10��ʾÿ�ȵ���ȡ0.10Ԫ�ķ����
	unsigned short price_show_type;//�Ƿ񽫷���Ѻ͵��ͳһ��ʾ����Ļ�ϣ�1��ʾ�ǣ�0��ʾ��ʾ����(��������۷ֿ���ʾ��
	unsigned short pad6;
	//��2�׵�۲���
	unsigned short new_price_valid_year;
	unsigned short new_price_valid_month;
	unsigned short new_price_valid_day;
	unsigned short new_price_valid_hour;
	unsigned short new_price_valid_min;
	unsigned short new_price_valid_sec;
	unsigned short new_price_valid_flag;//�µ����Ч��־ 1-��Ч��������Ч
	unsigned short new_price_time_zone_num;//ÿ���ʱ������1��12
	
	unsigned short new_price_time_zone_feilv[4];//����12��ʱ��εķ��ʺ�ѡ��time_zone_feilv[0]��ֵ��Ϊ0x1234���ʾ��1��ʱ��ʹ�÷���1����2��ʱ��ʹ�÷���2����3��ʱ��ʹ�÷���3����4��ʱ��ʹ�÷���4
	unsigned short new_price_time_zone_tabel[12];//���12��ʱ���---���ֽڱ�ʾʱ��εĿ�ʼСʱ�������ֽڱ�ʾ��ʼ��������BIN��

	unsigned short new_price_per_kwh[4];//4�����ʵ���Ԫ��С����2λ����ֵ88��ʾ0.88Ԫÿ�ȵ磬����Ϊ�⡢�塢ƽ����4����ۣ�ԭ���ϼ۸�����½�
	
	unsigned short new_price_kwh_service_price[4];//�⡢�塢ƽ���ȷ�ʱ����ѣ�����Ԫ��С����2λ����ֵ10��ʾÿ�ȵ���ȡ0.10Ԫ�ķ����
	
	unsigned short new_price_show_type;//�Ƿ񽫷���Ѻ͵��ͳһ��ʾ����Ļ�ϣ�1��ʾ�ǣ�0��ʾ��ʾ����(��������۷ֿ���ʾ��
	unsigned short new_price_service_price;//����ѣ�����Ԫ��С����2λ����ֵ10��ʾÿ�ȵ���ȡ0.10Ԫ�ķ����
	
	unsigned short new_price_service_price2;//������ȡ�ķ���ѣ�����Ԫ��С����2λ
	unsigned short new_price_service_price3;//�����ʱ��(ռ�ó��׮ʱ��)��ȡ�ķ���ѣ�����Ԫ��С����2λ��Ԫ/10����
	
	unsigned short new_price_service_sel;//ѡ����ȡ����ѵķ�ʽ��ֵ3��ʾ��������ֵ1��ʾ������ȡ��ֵ2��ʾ��ʱ����ȡ(Ԫ/10����)
	unsigned short pad7;
	
	unsigned long  new_price_car_park_price;//ÿ10���ӵ�ͣ���ѣ���λԪ��ϵ��0.01
	
	unsigned short new_price_free_minutes_after_charge;//�����ɺ󻹿����ͣ����ʱ��,��λ����
	unsigned short new_price_free_when_charge;//����ڼ��Ƿ���ͣ���ѣ�ֵ1��ѣ�ֵ0�����
	
	unsigned char volt_abnormal_stop_en;//�Ƿ��/Ƿѹֹͣ���
	unsigned char cp_filter_cnt;//CP����˲�����
	unsigned char ctl1_need_upgrade;//�Ƿ���Ҫ�������Ƶ�Ԫ1
	unsigned char pad[1];
	
	unsigned char pad8[20];
}DEV_PARA,*PDEV_PARA;//����Ϊ4��������,MAX 736�ֽ�

typedef struct   //���豸FLASH�ж�Ӧ�ı��������������һ��
{
	ETH_PARA       eth_dcb;
	DEV_PARA       dev_para;
}DEV_CFG,*PDEV_CFG;//����Ϊ4��������

//�����豸���������Ϣ
typedef struct
{	unsigned char main_class;
	unsigned char sub_class;
	unsigned short info_len;//�豸��Ϣ���ֽڳ���---���1024�ֽڣ�Ϊ�����dev_name��ʼ��dev_time�ĳ����Լ�DEV_PARA�ĳ���!	
	char dev_name[32];//�豸����32�ֽ�
	char dev_sw_ver[32];//�豸�̼��汾32�ֽ�	
	char dev_manufacturer[64];//�豸������	
	char dev_location[64];//�豸ʹ�õص�
	char dev_user_info[64];//�û��Զ�����Ϣ
	char dev_time[32];//�豸��ǰʱ����Ϣ
	char dev_serial_num[20];//�豸���кţ����20�ֽ�
}DEV_INFO,*PDEV_INFO;

typedef unsigned char tBoolean;
typedef unsigned char BOOLEAN;

typedef unsigned long long Time_Stamp;//ʱ�������Ϊ64bit����

//  ����MODBUS������Ӧ����ṹ
typedef struct
{    
	unsigned char slave_addr;			//  ��ǰ��Ӧ�����е�MODBUS�ӻ���ַ
	unsigned char func_code;			//  ��Ӧ�Ĺ�������0x03,0x04...	
	unsigned char res_len;              //  res_buf����Ч�����ֽڳ���
	unsigned char res_buf[256];         //  ���ݻ�����	
	unsigned char crc_low;				//  ���յ���CRC���ֽ�
	unsigned char crc_high;				//  ���յ���CRC���ֽ�
	
	unsigned short com_err;             //  ָʾ���յ�ͨ�Ź����еĸ����쳣,������ɺ�˴�Ϊ0��ʾ��������	
} Modbus_resp;


#define MB_CMD_PARA_LEN    64 //�ӹ�������1���ֽڵ�CRC֮ǰ��������󳤶�
//  ����MODBUS����ͻ���ṹ
typedef struct
{
	unsigned char slave_addr;			//  ��ǰҪ���ʵ�MODBUS�ӻ���ַ
	unsigned char func_code;			//  ��������0x01,0x02,0x03,0x04,0x05,0x06,0x0F,0x10...	
	unsigned char para_buf[MB_CMD_PARA_LEN];         //  ����������
	unsigned char crc_low;				//  �ȷ���CRC���ֽ�
	unsigned char crc_high;				//  �ٷ���CRC���ֽ�	
	
	unsigned char para_len;             //  para_buf����Ч�����ֽڳ���	
} Modbus_cmd;

#define MODBUS_CMDS_FIFO_CNT     32  //
typedef struct{
	unsigned char rd_index;//д����,��ָ��
	unsigned char wr_index;//������,дָ��
	unsigned char full;//fifo����־,1��Ч
	unsigned char empty;//fifo�ձ�־,1��Ч
	Modbus_cmd    modbus_host2dev_cmds[MODBUS_CMDS_FIFO_CNT];//��̫�����յ���λ�������͸�CC2530�豸�Ŀ��ƺ���������֡
}MODBUS_CMD_FIFO;



//  ����ͨ�ý��ջ���ṹ
#define MAX_RX_BUF_SIZE   1024 //300
typedef struct
{	
	unsigned char data[MAX_RX_BUF_SIZE];         //  �������ݻ�����	
	unsigned short len;             //  data����Ч�����ֽڳ���	
}RX_BUF;

//  ����ͨ�ý��ջ���ṹ
#define MAX_TX_BUF_SIZE   512//300
typedef struct
{	
	unsigned char data[MAX_TX_BUF_SIZE];         //  �������ݻ�����	
	unsigned short len;             //  data����Ч�����ֽڳ���	
}TX_BUF;

#define RX_BUF_CNT  4//1//2// 5
typedef struct{
	unsigned char rd_index;//д����,��ָ��
	unsigned char wr_index;//������,дָ��
	unsigned char full;//fifo����־,1��Ч
	unsigned char empty;//fifo�ձ�־,1��Ч
	RX_BUF    rx_bufs[RX_BUF_CNT];//
}SW_RX_FIFO;

#define TX_BUF_CNT   3//4//1//2//5
typedef struct{
	unsigned char rd_index;//д����,��ָ��
	unsigned char wr_index;//������,дָ��
	unsigned char full;//fifo����־,1��Ч
	unsigned char empty;//fifo�ձ�־,1��Ч
	TX_BUF    tx_bufs[TX_BUF_CNT];//
}SW_TX_FIFO;




#define	ISO8583_BUSY_SN		20	//ҵ����ˮ�ų���
//���׼�¼�еı�־�ֶ��еĸ�bit����
#define  CARD_PWR_ON_OK_BIT     0  //����0ֵΪ0��ʾ��Ƭ��ʼ�ӵ�ɹ�
//#define  CARD_PWR_OFF_OK_BIT    1  //����1ֵΪ0��ʾ��Ƭ�����ӵ�ɹ�

#define  DEAL_SENT_OK_BIT       31 //����31ֵΪ0��ʾ�ý��׼�¼�ѳɹ��ϴ���������
typedef struct{
	
	unsigned long  deal_local_seq;//����������ˮ��1��ʼ�������ϵ�ʱ����������ˮ��
	
	unsigned char card_sn[8];     //�û�����	8�ֽ� BCD�洢��16λ����
	
	unsigned char deal_result;  //���׽��  0x00���۷ѳɹ���0x01��������0x02�����佻��,BIN
	unsigned char chg_end_reason;//���׽���ԭ��
	//unsigned char pad1[1];
	unsigned short card_pwr_off_flag;//�û��������ӵ��־��ֵ0x55AA��ʾ�ѽ����ӵ�---20160718
	
	unsigned char chg_type;//��緽ʽ 1-ˢ����磬2-APP���,3-ͣ����(���μ�¼����ͣ������)
	unsigned char chg_service_sel;//�������ȡ��ʽ��01�������շѣ�02����ʱ���շ�(��λ����/10���ӣ�03�������� ��/��
	unsigned char charge_mode;//���ģʽ 1-�Զ����� 2-����� 3-������ 4-��ʱ��
	unsigned char plug_sel;
	
	unsigned long chg_service_money;//����ѣ��Է�Ϊ��λ
	
	unsigned long rmb;          //����ܽ��Է�Ϊ��λ���Ϊ�ڲ���������Ϊ0),BIN
	unsigned long kwh;          //����ܵ�������*100����10�ȵ���1000��ʾ��,BIN
	
	unsigned char Busy_SN[ISO8583_BUSY_SN]; //ҵ����ˮ�� 20�������ַ�,BIN,��ʼ����bitΪȫ1
	
	unsigned long last_rmb;     //���ǰ�����Է�Ϊ��λ��,BIN
		
	unsigned long pre_kwh;      //��翪ʼ����������*100����10�ȵ���1000��ʾ��,BIN
	unsigned long cur_kwh;      //����������������*100����10�ȵ���1000��ʾ��,BIN
	
	unsigned long rmb_kwh;      //��ۣ���λ����/�ȣ�,BIN---�����ν��׽��/���׵����ϴ�
	
		
	CHG_BIN_TIME  start_time;  //��翪ʼʱ��,BIN
	CHG_BIN_TIME  end_time;    //������ʱ��,BIN
	
	unsigned short price_per_kwh[4];//4�����ʵ���Ԫ��С����2λ����ֵ88��ʾ0.88Ԫÿ�ȵ磬����Ϊ�⡢�塢ƽ����4����ۣ�ԭ���ϼ۸�����½�
	unsigned short rmb_per_price[4];//4����۵ĳ���С����2λ����ֵ88��ʾ0.88Ԫ������Ϊ�⡢�塢ƽ����4�����
	
	////////////////////////////////////////////////////////////////////////
	////V2.2.3��̨Э����������
	unsigned short service_price_per_kwh[4];//4������ѷ��ʵ���Ԫ��С����2λ����ֵ88��ʾ0.88Ԫÿ�ȵ磬����Ϊ�⡢�塢ƽ����4����ۣ�ԭ���ϼ۸�����½�
	unsigned short kwh_charged[4];//4������Ѷ�Ӧ�ĳ�������С����2λ����ֵ88��ʾ0.88�ȣ�����Ϊ�⡢�塢ƽ����4������
	unsigned short service_rmb[4];//4������ѷ��ʵĽ���λԪ��С����2λ����ֵ88��ʾ0.88Ԫÿ�ȵ磬����Ϊ�⡢�塢ƽ����4����ۣ�ԭ���ϼ۸�����½�
	///////////////////////////////////////////////////////////////////////
	//��¼��ر�־��ʼ����bitȫ1
	//����31��ֵΪ0��ʾ�ü�¼�ѳɹ����͵�Զ�˷�����������0��ֵ1��ʾ���Ͳ��ɹ���û�п�ʼ����
	//����0��ֵΪ0��ʾ�ÿ�����ɿ�ʼ�ӵ�---���ν����ڱ��ؿ�ʼOK	
	unsigned long deal_record_flag; 
	
	/////////////////////////V2.1.8Э������ͣ������//////////////////////////////
	unsigned long park_rmb;     //ͣ�����ã��Է�Ϊ��λ��,BIN
	CHG_BIN_TIME  park_start_time;  //ͣ����ʼʱ��(����⵽������ͣ��λ��ʼ��),BIN
	CHG_BIN_TIME  park_end_time;    //ͣ������ʱ��(���뿪��λ��ʱ��),BIN
	//////////////////////////////////////////////////////////////////////////////
	
	
	unsigned long deal_data_valid_flag;//��¼��Ч��־0x55AA55AA��ʾ�ü�¼��Ч

}CHG_DEAL_REC;//���������ݼ�¼----152�ֽ�----20170928//128�ֽ�,����Ϊ4��������---128�ֽ�---20161111

typedef enum
{
	DEAL_CMD_FAILED = 0,//����ִ��ʧ��
	DEAL_CMD_OK = 1,//����ִ�����
	ADD_NEW_REC = 2, //�����¼�¼
	MODIFY_REC = 3,//�޸Ľ��׼�¼�еĳ������
	
	
}eDealRec_CMD;

typedef struct
{
	unsigned short BCR;
	unsigned short BSR;
	unsigned short ID1;
	unsigned short ID2;
	unsigned short REG4;
	unsigned short REG5;
	unsigned short REG6;
	unsigned short SR;//�Ĵ���31 Special Register
}PHY_REGS;

#pragma pack () /*ȡ��ָ�����룬�ָ�ȱʡ����*/

#ifdef __cplusplus
    }
#endif

#endif //__TYPE_DEF_H__

