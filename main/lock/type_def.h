/****************************************Copyright (c)****************************************************
**                               DongGuan EAST Co.,LTD.
**                                     
**                                 http://www.EASTUPS.com
**
**--------------File Info---------------------------------------------------------------------------------
** File Name:               type_def.h
** Last modified Date:      2012.12.7
** Last Version:            1.0
** Description:             定义工程中用到的各种数据类型
** 
**--------------------------------------------------------------------------------------------------------
** Created By:              
** Created date:            2012.12.7
** Version:                 1.0
** Descriptions:            The original version 初始版本
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

#define  SOFT_VER   0x0101 //V1.01,BCD格式,如0x0311表示V3.11
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



#pragma pack (1) /*指定按1字节对齐*/
//定义UART通信参数结构
struct M3_UART_DCB{
	unsigned char baud;//0x00—1200, 0x01—2400,0x02—4800, 0x03—9600,0x04—14400, 0x05—19200,0x06-38400,0x07-57600,0x08-115200
	unsigned char bytesize;// 0x08---8位, 0x09---9位
	unsigned char stopbits;//0x01—1位, 0x02—2位
	unsigned char parity;//0x01表示检验位无校验，0x02表示校验位为偶校验，0x03表示校验位为奇校验
};

typedef struct //以太网参数定义
{
	unsigned char  mac_addr[8];        //  本机MAC地址,多2字节，在FLASH中亦占用8个字节
	unsigned char  ip_addr[4];         //  本机IPv4地址
	unsigned char  net_mask[4];        //  本机IP地址掩码
	unsigned char  gate_way[4];        //  网关IPv4地址
	//网络参数配置方式(IP地址获取方式DHCP或静态)
	//Bit0—IP获取方式,0-DHCP,1-静态	
	unsigned char  m3_eth_config; 
	unsigned char  m3_eth_mode;//网口工作模式配置,0---自动协商(默认),1-10M Full, 2-10M Half,3-100M Full,4-100M Half
	unsigned char  reserved[10];//结构体size 4字节对齐
}ETH_PARA,*PETH_PARA;

enum SYS_EVENTS
{	
	eEMERGE_PRESSED_EVENT=0,//急停按钮按下
	eAC_VOLT_HIGH_FAULT,//交流过压
	eAC_VOLT_LOW_FAULT ,//交流欠压
	eAC_CURR_HIGH_FAULT ,//交流过流	
	eAC_PE_FAULT ,//接地故障
	eMETER_COMM_FAULT ,//电表通信故障
	eCARD_COMM_FAULT ,//刷卡机通信故障
	eLCD_COMM_FAULT ,//LCD通信故障
	eGPS_COMM_FAULT ,//GPS通信故障
	eCAN_COMM_FAULT,//CAN通信故障
		
};

enum COIN_TYPE_LIST
{	
	eRMB=0,//人民币
	eDollar,//美元
	eEuro,//欧元
	ePound,//英镑	
	eTHB,//泰铢
	eRS,//卢比
	eCOIN_TYPE_END = eRS,
};

typedef struct
{
	unsigned short year;//年	
	unsigned char month;//月 1---12
	unsigned char day_of_month;//某月中的某日，如3月2日, 1---31	
	
	unsigned char week_day;//星期几 1---7  7-sunday,1-monday....6-saturday
	unsigned char hour;//小时0---23
	unsigned char minute;//分钟 0---59
	unsigned char second;//秒 0---59
	
}CHG_BIN_TIME;	//数值为二进制格式

typedef struct   //设备其他参数定义
{	
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
	
	unsigned short can_bitrate;
	// unsigned short volt_33;//设置控制单元的3.3V的实际值以便准确计算PT1000的温度
	// unsigned short current_low_shutdown;//输出电流持续低于该值后关断输出，单位mA,默认200mA
	unsigned char can_reserved[14];	
	
	unsigned char http_password[8];
	//交流充电桩其他参数
	unsigned char pwm_percent;//交流PWM百分比，如值20表示20%，对应电流16A
	unsigned char pe_det_en;//接地故障检测使能，值0不使能，其他值使能	
	unsigned char LangSelect;//LCD语言选择,0---中文简体，1---英文
	unsigned char constrat;//对比度百分比
	
	unsigned short ac_volt_high_alarm;//过压告警值，单位V系数1
	unsigned short ac_volt_low_alarm;//欠压告警值，单位V系数1
	
	unsigned short ac_current_high_shutdown;//交流过流值与额定电流的百分数，如120表示额定电流的120%时关断充电电路
	unsigned short meter_type;//值1表示单相电表，值3表示3相电表
	unsigned long  password;//系统设置的6位数密码值，默认123456
	
	unsigned long  UserPWD;//普通用户密码，默认123456
	
	unsigned char light_on_hour;//灯箱开启小时位
	unsigned char light_on_min;//灯箱开启分钟位
	unsigned char light_off_hour;//灯箱关闭小时位
	unsigned char light_off_min;//灯箱关闭分钟位
	
	unsigned short price_per_kwh[4];//4个费率单价元，小数点2位，如值88表示0.88元每度电，依次为尖、峰、平、谷4个电价，原则上价格逐个下降
	unsigned short service_price;//服务费，单价元，小数点2位，如值10表示每度电收取0.10元的服务费
	
	unsigned short time_zone[12];//48个时间段的费率选择，time_zone[0]的值如为0x1234则表示0:00-0:30为费率1，0:30-1:00为费率2，1:00-1:30为费率3，1:30-2:00为费率4
	unsigned short card_consume_en;//卡片扣费使能，值0不使能，值1使能，默认使能
	
	unsigned char  charger_ID[8];//充电桩编号，16个数字位，BCD存储--电桩协议中作为充电桩地址使用
	
	unsigned short service_price2;//按次收取的服务费，单价元，小数点2位
	unsigned short service_price3;//按充电时间(占用充电桩时间)收取的服务费，单价元，小数点2位，元/10分钟
	
	unsigned char  service_sel;//选择收取服务费的方式，值3表示按电量，值1表示按次收取，值2表示按时间收取(元/10分钟)
	unsigned char  card_dev_type;//刷卡机类型，0-插入式刷卡机 1-非插入式刷卡机	
	unsigned char  meter_feilv_type;//是否是费率表 0--不是 1--是
	//unsigned char  pad2;
	unsigned char delta_volt;//过，欠压告警回差，单位V，系数1-------------
	
	unsigned char server1_ip[4];//BIN码，服务器IP地址---1号备用IP	
	unsigned short server1_port;//BIN码，先传低字节---1号备用IP的端口
	unsigned char server2_ip[4];//BIN码，服务器IP地址---2号备用IP	
	unsigned short server2_port;//BIN码，先传低字节---2号备用IP的端口
	unsigned short change_ip_flag;//是否到指定时间切换服务器IP地址,值1是，值0否
	unsigned char m1_key_valid;//是否已刷管理卡注入密钥  pad2[1];
	unsigned char plug_lock_ctl;//是否启用枪锁控制,值0不启用，值1启用且检测锁状态，值2启用但不检测状态
	
	CHG_BIN_TIME  server_ip_change_time;//8字节,如果服务器下发的服务器IP设置指令要求在指定时间后更改服务器IP，则将该时间保存在此
	
	unsigned char  time_zone_num;//每天的时段数，1到12
	//unsigned char  pad3[1];//4字节补齐
	unsigned char cp_fault_det_en;//值0不使能CP接地故障检测，否则使能
	//unsigned char  emergy_stop_button;//是否有急停按钮，值0表示无，值1表示有
	unsigned char  leak_current_alarm;//漏电流阈值，单位mA，系数1--------------
	unsigned char  conn_server_type;//连接服务器的方式，0-不联网 ,1-RJ45以太网,2-DTU(如映翰通)联网,3-WIFI,4-USB 3G
	
	//unsigned short pad4[1];//补齐4字节对齐
	unsigned short card_order_price;//刷卡预约桩定时充电期间的费用单价，元/分钟，系数0.01,值100表示1.00元每分钟
	unsigned short time_zone_feilv[3];//至多12个时间段的费率号选择，time_zone_feilv[0]的值如为0x1234则表示第1个时段使用费率1，第2个时段使用费率2，第3个时段使用费率3，第4个时段使用费率4
	
	unsigned short time_zone_tabel[12];//最多12个时间段---高字节表示时间段的开始小时数，低字节表示开始分钟数，BIN码
	
	unsigned char m1_card_key[8];//低6字节为读取M1卡片的密钥
	
	unsigned char soft_ver[8];//5个字节的ASCII版本号，用于判断是否需要服务器下载新固件,初始全'0'
	
	unsigned char send_soft_update_confirm;//是否发送软件升级确认帧到服务器
	unsigned char emergy_button_en;//急停按钮使能控制
	unsigned char temp_sensor_en;//温度传感器使能控制
	unsigned char fanglei_en;//是否启用防雷器检测
	
	unsigned short w_UgainA;//控制单元A相电压增益
	unsigned short w_UgainB;//控制单元B相电压增益
	unsigned short w_UgainC;//控制单元C相电压增益
	unsigned short temp_High_shutdown_value;//20170228增加过温停止充电，单位℃，系数1 pad4;//
	
	unsigned char output_type;//输出方式 0-单枪，1-双枪，2-不带枪
	unsigned char setup_type;//安装方式 0-壁挂式，1-落地式，2-便携式，3-立式
	unsigned short oem_code;//公司代码 0-中能易电(EV) 1-南网(NW) 2-电庄(DZ),3-上海挚达(ZD),4-粤盛电气(YS),5-中兴(ZE)...
	
	enum COIN_TYPE_LIST e_coin_type;//货币类型---0-RMB(￥),1-美元($)，2-欧元(�)，3-英镑(￡),4-泰铢(THB)
	unsigned char car_lock_addr;
	unsigned char pad5[2];
	
	unsigned char phone_num[16];//11位的故障联系手机号或固定电话号码076988888888
	//unsigned char pad4[100];
	unsigned char black_list_ver[8];
	
	unsigned short car_lock_num;//车位锁数量0---2
	unsigned short car_lock_time;//检测到无车的时候，车位上锁的时间,单位分钟，默认3分钟，在降下车锁后开始计时，超时后仍然无车则锁上车位
	unsigned long car_park_price;//每10分钟的停车费，单位元，系数0.01
	
	unsigned short free_minutes_after_charge;//充电完成后还可免费停车的时长,单位分钟
	unsigned char  free_when_charge;//充电期间是否免停车费，值1免费，值0不免费
	unsigned char emergy_button_type;//值0表示常开输入(闭合表示急停按下),值1表示常闭输入(断开表示急停按下)
	
	
	unsigned short volt_33;//设置控制单元的3.3V的实际值以便准确计算PT1000的温度
	unsigned short current_low_shutdown;//输出电流持续低于该值后关断输出，单位mA,默认200mA
	
	
	unsigned short kwh_service_price[4];//尖、峰、平、谷分时服务费，单价元，小数点2位，如值10表示每度电收取0.10元的服务费
	unsigned short price_show_type;//是否将服务费和电价统一显示在屏幕上，1表示是，0表示显示详情(服务费与电价分开显示）
	unsigned short pad6;
	//第2套电价参数
	unsigned short new_price_valid_year;
	unsigned short new_price_valid_month;
	unsigned short new_price_valid_day;
	unsigned short new_price_valid_hour;
	unsigned short new_price_valid_min;
	unsigned short new_price_valid_sec;
	unsigned short new_price_valid_flag;//新电价有效标志 1-有效，其余无效
	unsigned short new_price_time_zone_num;//每天的时段数，1到12
	
	unsigned short new_price_time_zone_feilv[4];//至多12个时间段的费率号选择，time_zone_feilv[0]的值如为0x1234则表示第1个时段使用费率1，第2个时段使用费率2，第3个时段使用费率3，第4个时段使用费率4
	unsigned short new_price_time_zone_tabel[12];//最多12个时间段---高字节表示时间段的开始小时数，低字节表示开始分钟数，BIN码

	unsigned short new_price_per_kwh[4];//4个费率单价元，小数点2位，如值88表示0.88元每度电，依次为尖、峰、平、谷4个电价，原则上价格逐个下降
	
	unsigned short new_price_kwh_service_price[4];//尖、峰、平、谷分时服务费，单价元，小数点2位，如值10表示每度电收取0.10元的服务费
	
	unsigned short new_price_show_type;//是否将服务费和电价统一显示在屏幕上，1表示是，0表示显示详情(服务费与电价分开显示）
	unsigned short new_price_service_price;//服务费，单价元，小数点2位，如值10表示每度电收取0.10元的服务费
	
	unsigned short new_price_service_price2;//按次收取的服务费，单价元，小数点2位
	unsigned short new_price_service_price3;//按充电时间(占用充电桩时间)收取的服务费，单价元，小数点2位，元/10分钟
	
	unsigned short new_price_service_sel;//选择收取服务费的方式，值3表示按电量，值1表示按次收取，值2表示按时间收取(元/10分钟)
	unsigned short pad7;
	
	unsigned long  new_price_car_park_price;//每10分钟的停车费，单位元，系数0.01
	
	unsigned short new_price_free_minutes_after_charge;//充电完成后还可免费停车的时长,单位分钟
	unsigned short new_price_free_when_charge;//充电期间是否免停车费，值1免费，值0不免费
	
	unsigned char volt_abnormal_stop_en;//是否过/欠压停止充电
	unsigned char cp_filter_cnt;//CP检测滤波次数
	unsigned char ctl1_need_upgrade;//是否需要升级控制单元1
	unsigned char pad[1];
	
	unsigned char pad8[20];
}DEV_PARA,*PDEV_PARA;//必须为4的整数倍,MAX 736字节

typedef struct   //与设备FLASH中对应的保存参数的区域定义一致
{
	ETH_PARA       eth_dcb;
	DEV_PARA       dev_para;
}DEV_CFG,*PDEV_CFG;//必须为4的整数倍

//定义设备类型相关信息
typedef struct
{	unsigned char main_class;
	unsigned char sub_class;
	unsigned short info_len;//设备信息块字节长度---最大1024字节，为下面从dev_name开始到dev_time的长度以及DEV_PARA的长度!	
	char dev_name[32];//设备名称32字节
	char dev_sw_ver[32];//设备固件版本32字节	
	char dev_manufacturer[64];//设备制造商	
	char dev_location[64];//设备使用地点
	char dev_user_info[64];//用户自定义信息
	char dev_time[32];//设备当前时间信息
	char dev_serial_num[20];//设备序列号，最大20字节
}DEV_INFO,*PDEV_INFO;

typedef unsigned char tBoolean;
typedef unsigned char BOOLEAN;

typedef unsigned long long Time_Stamp;//时间戳定义为64bit变量

//  定义MODBUS命令响应缓冲结构
typedef struct
{    
	unsigned char slave_addr;			//  当前响应数据中的MODBUS从机地址
	unsigned char func_code;			//  响应的功能码如0x03,0x04...	
	unsigned char res_len;              //  res_buf的有效数据字节长度
	unsigned char res_buf[256];         //  数据缓冲区	
	unsigned char crc_low;				//  接收到的CRC低字节
	unsigned char crc_high;				//  接收到的CRC高字节
	
	unsigned short com_err;             //  指示接收的通信过程中的各种异常,接收完成后此处为0表示接收正常	
} Modbus_resp;


#define MB_CMD_PARA_LEN    64 //从功能码下1个字节到CRC之前的数据最大长度
//  定义MODBUS命令发送缓冲结构
typedef struct
{
	unsigned char slave_addr;			//  当前要访问的MODBUS从机地址
	unsigned char func_code;			//  功能码如0x01,0x02,0x03,0x04,0x05,0x06,0x0F,0x10...	
	unsigned char para_buf[MB_CMD_PARA_LEN];         //  参数缓冲区
	unsigned char crc_low;				//  先发送CRC低字节
	unsigned char crc_high;				//  再发送CRC高字节	
	
	unsigned char para_len;             //  para_buf的有效数据字节长度	
} Modbus_cmd;

#define MODBUS_CMDS_FIFO_CNT     32  //
typedef struct{
	unsigned char rd_index;//写索引,读指针
	unsigned char wr_index;//读索引,写指针
	unsigned char full;//fifo满标志,1有效
	unsigned char empty;//fifo空标志,1有效
	Modbus_cmd    modbus_host2dev_cmds[MODBUS_CMDS_FIFO_CNT];//以太网口收到上位机将发送给CC2530设备的控制和设置命令帧
}MODBUS_CMD_FIFO;



//  定义通用接收缓冲结构
#define MAX_RX_BUF_SIZE   1024 //300
typedef struct
{	
	unsigned char data[MAX_RX_BUF_SIZE];         //  接收数据缓冲区	
	unsigned short len;             //  data的有效数据字节长度	
}RX_BUF;

//  定义通用接收缓冲结构
#define MAX_TX_BUF_SIZE   512//300
typedef struct
{	
	unsigned char data[MAX_TX_BUF_SIZE];         //  接收数据缓冲区	
	unsigned short len;             //  data的有效数据字节长度	
}TX_BUF;

#define RX_BUF_CNT  4//1//2// 5
typedef struct{
	unsigned char rd_index;//写索引,读指针
	unsigned char wr_index;//读索引,写指针
	unsigned char full;//fifo满标志,1有效
	unsigned char empty;//fifo空标志,1有效
	RX_BUF    rx_bufs[RX_BUF_CNT];//
}SW_RX_FIFO;

#define TX_BUF_CNT   3//4//1//2//5
typedef struct{
	unsigned char rd_index;//写索引,读指针
	unsigned char wr_index;//读索引,写指针
	unsigned char full;//fifo满标志,1有效
	unsigned char empty;//fifo空标志,1有效
	TX_BUF    tx_bufs[TX_BUF_CNT];//
}SW_TX_FIFO;




#define	ISO8583_BUSY_SN		20	//业务流水号长度
//交易记录中的标志字段中的各bit描述
#define  CARD_PWR_ON_OK_BIT     0  //比特0值为0表示卡片开始加电成功
//#define  CARD_PWR_OFF_OK_BIT    1  //比特1值为0表示卡片结束加电成功

#define  DEAL_SENT_OK_BIT       31 //比特31值为0表示该交易记录已成功上传到服务器
typedef struct{
	
	unsigned long  deal_local_seq;//本机交易流水号1开始递增，上电时加载最后的流水号
	
	unsigned char card_sn[8];     //用户卡号	8字节 BCD存储，16位数字
	
	unsigned char deal_result;  //交易结果  0x00：扣费成功；0x01：灰锁，0x02：补充交易,BIN
	unsigned char chg_end_reason;//交易结束原因
	//unsigned char pad1[1];
	unsigned short card_pwr_off_flag;//用户卡结束加电标志，值0x55AA表示已结束加电---20160718
	
	unsigned char chg_type;//充电方式 1-刷卡充电，2-APP充电,3-停车费(本次记录的是停车费用)
	unsigned char chg_service_sel;//服务费收取方式，01：按次收费；02：按时间收费(单位：分/10分钟；03：按电量 分/度
	unsigned char charge_mode;//充电模式 1-自动充满 2-按金额 3-按电量 4-按时间
	unsigned char plug_sel;
	
	unsigned long chg_service_money;//服务费，以分为单位
	
	unsigned long rmb;          //充电总金额（以分为单位如果为内部车辆，则为0),BIN
	unsigned long kwh;          //充电总电量（度*100，如10度电用1000表示）,BIN
	
	unsigned char Busy_SN[ISO8583_BUSY_SN]; //业务流水号 20个数字字符,BIN,初始化各bit为全1
	
	unsigned long last_rmb;     //充电前卡余额（以分为单位）,BIN
		
	unsigned long pre_kwh;      //充电开始电表读数（度*100，如10度电用1000表示）,BIN
	unsigned long cur_kwh;      //充电结束电表读数（度*100，如10度电用1000表示）,BIN
	
	unsigned long rmb_kwh;      //电价（单位：分/度）,BIN---按本次交易金额/交易电量上传
	
		
	CHG_BIN_TIME  start_time;  //充电开始时间,BIN
	CHG_BIN_TIME  end_time;    //充电结束时间,BIN
	
	unsigned short price_per_kwh[4];//4个费率单价元，小数点2位，如值88表示0.88元每度电，依次为尖、峰、平、谷4个电价，原则上价格逐个下降
	unsigned short rmb_per_price[4];//4个电价的充电金额，小数点2位，如值88表示0.88元，依次为尖、峰、平、谷4个电价
	
	////////////////////////////////////////////////////////////////////////
	////V2.2.3后台协议新增内容
	unsigned short service_price_per_kwh[4];//4个服务费费率单价元，小数点2位，如值88表示0.88元每度电，依次为尖、峰、平、谷4个电价，原则上价格逐个下降
	unsigned short kwh_charged[4];//4个服务费对应的充电电量，小数点2位，如值88表示0.88度，依次为尖、峰、平、谷4个电量
	unsigned short service_rmb[4];//4个服务费费率的金额，单位元，小数点2位，如值88表示0.88元每度电，依次为尖、峰、平、谷4个电价，原则上价格逐个下降
	///////////////////////////////////////////////////////////////////////
	//记录相关标志初始化各bit全1
	//比特31的值为0表示该记录已成功发送到远端服务器，比特0的值1表示发送不成功或没有开始发送
	//比特0的值为0表示该卡已完成开始加电---本次交易在本地开始OK	
	unsigned long deal_record_flag; 
	
	/////////////////////////V2.1.8协议增加停车费用//////////////////////////////
	unsigned long park_rmb;     //停车费用（以分为单位）,BIN
	CHG_BIN_TIME  park_start_time;  //停车开始时间(按检测到车进入停车位开始算),BIN
	CHG_BIN_TIME  park_end_time;    //停车结束时间(车离开车位的时间),BIN
	//////////////////////////////////////////////////////////////////////////////
	
	
	unsigned long deal_data_valid_flag;//记录有效标志0x55AA55AA表示该记录有效

}CHG_DEAL_REC;//充电结算数据记录----152字节----20170928//128字节,必须为4的整数倍---128字节---20161111

typedef enum
{
	DEAL_CMD_FAILED = 0,//命令执行失败
	DEAL_CMD_OK = 1,//命令执行完成
	ADD_NEW_REC = 2, //增加新记录
	MODIFY_REC = 3,//修改交易记录中的充电数据
	
	
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
	unsigned short SR;//寄存器31 Special Register
}PHY_REGS;

#pragma pack () /*取消指定对齐，恢复缺省对齐*/

#ifdef __cplusplus
    }
#endif

#endif //__TYPE_DEF_H__

