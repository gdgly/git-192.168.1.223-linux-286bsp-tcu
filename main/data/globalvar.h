/*******************************************************************************
  Copyright(C)    2014-2015,      EAST Co.Ltd.
  File name :     globalvar.h
  Author :        dengjh
  Version:        1.0
  Date:           2014.4.11
  Description:    
  Other:          
  Function List:  
  History:        
          <author>   <time>   <version>   <desc>
            dengjh    2014.4.11   1.0       Create


** modified log:VS.2.10 基本版本	
                      			
** modified log:VS.2.11 180912
                      A.解决充电计费信息中，32位变量溢出问题，与QT通信的电量和金额由2字节增加到4字节
				VS.2.12  180917
					  A.VIN鉴权处理变更，直接启动充电，鉴权失败后再停止，之前是启动2次，第1次不是真正启动，只获取VIN，到
					  第2次启动的时候，有的车就不认了。
				VS.2.13 180927 
					  A. VIN启动增加停止按钮处理	  
				VS.2.14 180929
					继电器跳问题解决 双枪同时充，电流均分，100A的枪按最大输出电流计算需求模块
					将模块需求数量计算移植到MC.c中。
					BMS电池最低、最高温度上传了10倍
					SOC变化时，上传后台134报文
					134报文中的告警位发送后清0避免长时间发送告警有效的状态
*******************************************************************************/
#ifndef  _GLOBALVAR_H_
#define  _GLOBALVAR_H_

#include <sys/types.h>

//*****************************************************************************
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//*****************************************************************************
#ifdef __cplusplus
extern "C"
{
#endif
/*------------------------------------------------------------------------------
Section: Includes
------------------------------------------------------------------------------*/


//#define SOFTWARE_VER "V2.12"  //不能超过20个字符
//#define SOFTWARE_VER "V2.13"  //不能超过20个字符 180927 VIN启动增加停止按钮处理
//#define SOFTWARE_VER "V2.14"  //不能超过20个字符 继电器跳问题解决 双枪同时充，电流均分，100A的枪按最大输出电流计算需求模块
							//将模块需求数量计算移植到MC.c中。BMS电池最低、最高温度上传了10倍

//#define SOFTWARE_VER "V2.15" //将二位码是否显示的参数修改为交流采集板的接线位置	
							//将与控制板的参数配置中的并机标志修改为交流采集板的接线位置	
//#define SOFTWARE_VER "V2.16" //将并机模式修改，在是否可以并机的地方做处理	
							//							
//#define SOFTWARE_VER "V2.17" //将新旧语音在屏幕上可设置	
//#define SOFTWARE_VER "V2.18" //不是恒功率模块电压不是 950.000 V的模块使用额定电流计算模块个数
// #define SOFTWARE_VER "V2.19" //修改B枪并机时，电流下发时模块数量数组的下角标错误导致过流。
// #define SOFTWARE_VER "V2.20" //增加交流变比设置
								
// #define SOFTWARE_VER "V2.21" //修改QT的控制板升级代码。 181023

//#define SOFTWARE_VER "V2.22" //增加分时服务费，最多尖、峰、平、谷4种服务费,重启后如果系统时间少于2018年，则设置一个默认时间,181128

//#define SOFTWARE_VER "V2.23" //增加VIN在线鉴权，黑名单更新时，如果服务器下发的总数为0就删除黑名单数据库

//#define SOFTWARE_VER "V2.24"//优化TCP重连，发现底层send函数发送不出去数据2次后就关闭TCP连接，然后5秒后重连，避免TCP重传需要10分钟

//#define SOFTWARE_VER "V2.25" //修改8583VIN鉴权报文,服务器增加该VIN关联的卡号字段,VIN交易记录增加保存关联账号，上传交易记录需要改账号

//#define SOFTWARE_VER "V2.26"

//#define SOFTWARE_VER "EAM_V2.40"
//支持双枪对1个车，增加屏幕常亮设置QT界面，恢复枪锁类型，BMS电压选择，交流采集板继电器功能设置(似乎没什么用),增加COM口功能设置 190627
//8583升级包的解包格式按升级程序单独计算MD5方式后再压缩的方式来解析 190628
//#define PROTOCOL_VER "02.2.6"//"02.1.9"   //不能超过20个字符，格式不能更改 

//当交流采集板继电器配置为节能接触器控制时
//增加插枪不充电10分钟后关闭模块电源功能190712
//关闭后，任意一把枪重新插即可再次开启模块或者操作界面准备充电就开启，检测到APP开启充电也触发开启模块电源
//充电结束拔枪或不拔枪，等待30秒后关闭模块，等待期间重新插枪则不再关模块，重新等待充电计时10分钟
//10分钟内没充电，直接关闭模块，不再等30秒延时
//充电停止后没拔枪继续充电时，不能关闭模块
//心跳报文上传状态时如果在充电就一定上传充电中状态，绝缘告警时不传桩状态故障
//#define SOFTWARE_VER "EAM_V2.41"

//#define SOFTWARE_VER "EAM_V2.42"//优化VIN鉴权避免控制板已经停止了，界面还在等待，增加充电功率显示，开始时间显示等,增加是否启用双枪对一车充电
//#define SOFTWARE_VER "EAM_V2.43"//优化双枪对一辆车充电，辅枪电流问题
//#define SOFTWARE_VER "EAM_V2.44"//添加m3_search/iSearch_service 查询IP和MAC设置等操作后期再进行升级功能。

//#define SOFTWARE_VER "EAM_V2.45"//添加电表电量异常判断190925，修改双枪同充标志没有及时清0问题，修改用户卡充电时界面显示的充电开始时间错误问题。增加了一个桩号文本文件，便于配合导记录脚本使用，直接导出到序列号所在的文件夹下。
//#define SOFTWARE_VER "EAM_V2.46"//支持最新常规控制板双枪对一辆车充电模式
//#define SOFTWARE_VER "EAM_V2.47"//修改卡片，支持其他卡行

//#define SOFTWARE_VER "EAM_V2.49"//添加支持双枪，单枪模式
//#define SOFTWARE_VER "EAM_V2.50"//添加支持新的控制方式，添加对2.2.8协议支持,添加一些保护功能，添加电量显示为3位小数位功能，添加电量为3为小数的电表-2020-0415
//#define SOFTWARE_VER "EAM_V2.51"//解决关于掉电恢复之后金额异常的情况
#define SOFTWARE_VER "EAM_V2.52"//添加中兴，国网模块类型

#define PROTOCOL_VER "02.2.7"     

#define MAC_INTERFACE0  "eth0"
#define MAC_INTERFACE1  "eth1"
//#define  MODULE_CONTROL_BY_JK 
#define COMPENSATION_VALUE  50000  //计费时4舍五入

#define METER_CURR_THRESHOLD_VALUE  5 //电表采集的值与控制板采集上来的值误差在5%范围内
#define METER_CURR_COUNT   30   //30S-电表计量数据异常，累计判断时间韦30s
#define SOCLIMIT_CURR_COUNT  60   //达到SOC时候允许再次充电时间（并且电流还需要满足限制）
#define METER_ELECTRICAL_ERRO_COUNT 60  //读取的电量与上次对比，永远小于上次则异常
#define LIMIT_CURRENT_VALUE 20000  //20A
#define MIN_LIMIT_CURRENT_VALUE  500 //5A的时候才去判断电流问题，
#define MAX_LIMIT_OVER_RATEKWH  10     //最大允许多冲电度数10%*额定的---车企的SOC误差为5%
#define OVER_RATEKWH_COUNT 60  //最大允许多冲电度数10%*额定的,连续判断时间
#define METER_READ_MULTIPLE 10 //电表换算到3位小数进行计算，以前的电表只有两位小数，所以需要兼容

extern const char* g_sys_COM_name[];
#define MAX_COMS   6 
enum SYS_COM_ID//485换向IO
{
	SYS_COM1 = 1,
	SYS_COM2 = 2,
	SYS_COM3 = 3,
	SYS_COM4 = 4,
	SYS_COM5 = 5,
	SYS_COM6 = 6,
};

enum COM_EVEN_ODD
{
	NO_PARITY = 0,
	ODD_PARITY = 1,
	EVEN_PARITY = 2,
};

enum COM_RS485_DIR_CTL_IO//485换向IO
{
	COM3_DIR_IO = 7,
	COM4_DIR_IO = 6,
	COM5_DIR_IO = 8,
	COM6_DIR_IO = 9,
};

enum COM_RS485_TX_RX_LEVEL
{
	COM_RS485_RX_LEVEL = 1,//接收
	COM_RS485_TX_LEVEL = 0,//发送	
};

typedef struct 
{
	char *com_dev_name;
	enum SYS_COM_ID com_id;//1--COM1 ,2---COM2,...
	unsigned int com_baud;//波特率 2400 9600 ...
	unsigned int com_data_bits;//数据位 8
	unsigned int com_stop_bits;//停止位 1
	enum COM_EVEN_ODD even_odd;//校验位 0-无校验 1-奇校验 2-偶校验
	int reserved1;
	int reserved2;
}Com_run_para;

enum COM_FUNC_DEF{
	COM_NOT_USED = 0,//未使用
	CARD_READER_FUNC = 1,//读卡器
	MODBUS_SLAVE_FUNC = 2,//MODBUS 从机
	AC_INPUT_METER_FUNC = 3,//交流输入总3相电表
	DC_MEASURE_METER_FUNC = 4,//充电电量按直流计量
	AC_MEASURE_METER_FUNC = 5,//充电电量按交流计量
	AC_CAIJI_FUNC = 6,//交流采集板
	LED_BOARD1_FUNC = 7,//灯板1---
	LED_BOARD2_FUNC = 8,//灯板2
	CAR_LOCKER_FUNC = 9,//车位锁
	
	COM_END_FUNC = CAR_LOCKER_FUNC,
	
};

enum Input_Relay_Ctl_Func{
	//交流采集盒继电器控制 0--无作用，1-控制给模块供电的交流接触器 2-控制风机 3-模块和风机
	INPUT_RELAY_NOT_USED = 0,
	INPUT_RELAY_CTL_MODULE = 1,//控制模块输入交流电源
	INPUT_RELAY_CTL_FAN    = 2,//控制系统散热风机
	INPUT_RELAY_CTL_MODULE_AND_FAN = 3,//同时控制模块和风机
};

#define AC_METER_COM_ID   "/dev/ttySP2"//交流电能表  RS485 COM3 2400 e 8 1
#define DC_METER_COM_ID   "/dev/ttySP3"//直流电能表  RS485 COM4 9600 e 8 1
#define COUPLE_COM_ID     "/dev/ttySP0"//            RS485 COM6
#define AC_COM_ID         "/dev/ttySP1" //交流采集板 RS485 COM5 9600/2400 N 8 1
#define PC_COM_ID         "/dev/ttyAM0" //后台modbus RS232 COM2
#define IC_COMID          "/dev/ttySP4"//刷卡机通信  RS232 COM1 115200 N 8 1


#define GUN_CAN_COM_ID1   "can1"
#define GUN_CAN_COM_ID2   "can0"

/*消息队列 QUEUE 的设备定义*/
#define QUEUE_DEV_TCU		"TCU"		
#define QUEUE_DEV_REMOTE    "REMOTE"


#define DEBUGLOG     1  //把日志打印到文件中
#define NEW_VOICE    1//0  
#define CONTROL_DC_NUMBER    5//4 //4把枪

#define NO_DC_METER   0 //无直流电能表
#define DCM3366Q      1 //直流电能表DCM3366Q  DLT645-07协议
#define DCM3366       2 //直流电能表DCM3366  DLT645-97协议
#define DCM_DJS3366   3 //直流电能表 DCM3366 DLT645-2007协议，电量带三位小数
#define MIN_USER_MONEY_NEED    200 //1.00元以上才能启动充电
#define MIN_CURRENT_NEED    2000 //2.000A判断电流持续低的临界电流

#define CONNECT_USE_ETH   0 //使用网口联网
#define CONNECT_USE_DTU   1 //使用串口DTU联网

#define CHG_BY_CARD  0
#define CHG_BY_APP   1
#define CHG_BY_VIN   100

#define TIME_SHARE_SECTION_NUMBER 48 //电价分时段数

#define MODULE_INPUT_GROUP 5      //模块输入组数，先默认为6组-系统默认最大的组数为6组--CCU 个数
#define SYS_CONTACTOR_NUMBER 15 //最大线路条数全矩阵也才16条线路

enum DC_ModuleType
{
	HUAWEI_MODULE = 0,//华为模块
	INCREASE_MODULE = 1,//英可瑞模块
	INFY_MODULE = 2,//英飞源模块
	WINLINE_MODULE = 3,//永联模块----与英飞源协议相同
	ZTE_MODULE = 4,//中兴
	GW_MODULE = 5,//国网
	OTHER_MODULE = 6,//其他模块

};

enum HUAWEI_MODULE_LIST
{
	GR50030 = 0,
	GR50040,
	GR75020,
	GR75027,
	GR95021,
};//与QT界面的QComboBox控件中list的顺序必须一致

enum INCREASE_MODULE_LIST
{
	EVR10035 = 0,
	EVR15050,
	EVR20018,
	EVR30025,
	EVR35020,
	EVR50007,
	EVR50015,
	EVR70005,
	EVR75013,
	EVR100015,
	EVR500_10000,
	EVR700_10000,
};//与QT界面的QComboBox控件中list的顺序必须一致

enum INFY_MODULE_LIST
{
	REG50040 = 0,
	REG50050,
	REG75030,
	REG75035,
	REG75040,
	REG75050,//5
	REG1K025,//	6
};//与QT界面的QComboBox控件中list的顺序必须一致

enum WINLINE_MODULE_LIST
{
	NXR50020 = 0,
	NXR50040,
	NXR75020,
	NXR75030,
};


typedef enum _ALARM_SN
{
	ALARM_EMERGENCY_PRESSED = 0x01,//急停按下故障
	ALARM_PE_FAULT = 0x02,//接地故障
	ALARM_AT7022E_FAULT = 0x03,//107电能芯片通信故障
	ALARM_AC_DAQ_COMM_FAULT = 0x04,//交流采集模块通信中断
	ALARM_DC_MODULE_COMM_FAULT = 0x05,//充电模块通信中断
	ALARM_DC_MODULE_FAULT = 0x06,//充电模块故障
	ALARM_DC_MODULE_FAN_FAULT = 0x07,//充电模块风扇故障
	ALARM_DC_MODULE_AC_INPUT_WARNING = 0x08,//充电模块交流输入告警
	ALARM_DC_MODULE_OUTPUT_SHORT = 0x09,//充电模块输出短路故障
	ALARM_DC_MODULE_OUTPUT_CURRENT_HIGH = 0x0A,//充电模块输出过流告警
	ALARM_DC_MODULE_OUTPUT_VOLT_HIGH = 0x0B,//充电模块输出过压告警
	ALARM_DC_MODULE_OUTPUT_VOLT_LOW = 0x0C,//充电模块输出欠压告警
	ALARM_DC_MODULE_INPUT_VOLT_HIGH = 0x0D,//充电模块输入过压告警
	ALARM_DC_MODULE_INPUT_VOLT_LOW = 0x0E,//充电模块输入欠压告警
	ALARM_DC_MODULE_INPUT_VOLT_LOSS_PHASE = 0x0F,//充电模块输入缺相告警
	ALARM_DC_MODULE_TEMP_HIGH = 0x10,//充电模块过温告警
	ALARM_DC_MODULE_OVER_VOLT_SHUTDOWN = 0x11,//充电模块过压关机告警
	ALARM_DC_MODULE_OTHER_WARNING = 0x12,//充电模块其他告警
	ALARM_SYSTEM_OUTPUT_VOLT_HIGH = 0x13,//系统输出过压告警
	ALARM_SYSTEM_OUTPUT_VOLT_LOW = 0x14,//系统输出欠压告警
	ALARM_SYSTEM_ISO_LOW = 0x15,//系统绝缘告警
	ALARM_AC_INPUT_VOLT_HIGH_WARNING = 0x16,//交流输入过压告警
	ALARM_AC_INPUT_VOLT_HIGH_PROTECTED = 0x17,//交流输入过压保护
	ALARM_AC_INPUT_VOLT_LOW_PROTECTED = 0x18,//交流输入欠压告警，更改为保护
	ALARM_AC_INPUT_SWITCH_BREAK_OFF = 0x19,//交流输入开关跳闸
	ALARM_AC_INPUT_FANGLEI_OFF = 0x1A,//交流防雷器跳闸
	ALARM_OUTPUT_SWITCH_BREAK_OFF = 0x1B,//输出开关跳闸
	ALARM_FIRE_WARNING = 0x1C,//烟雾告警
	ALARM_TEMP_HIGH_WARNING = 0x1D,//充电桩过温故障
	ALARM_DC_MODULE_OUTPUT_VOLT_HIGH_LOCKOUT = 0x1E,//充电模块输出过压lockout告警
	ALARM_DC_MODULE_PROCTED = 0x1F,//充电模块保护
	ALARM_PLUG_TEMP_HIGH = 0x20,//充电枪过温故障
	ALARM_AC_BREAKER_TRIP = 0x21,//交流断路器跳闸
	ALARM_AC_KILLER_SWITCH = 0x22,//交流断路器断开
	ALARM_SYSTEM_POWER_FAULT = 0x23,//系统掉电=======
	ALARM_PLC_COMM_FAILED = 0x24,//与欧标或日标PLC通信盒通信中断---36========
	ALARM_LED_BOARD_COMM_FAILED = 37,//-----37灯板通信异常
	
	ALARM_VOLTMETER_COMM_FAILED = 51,//电压表通信中断
	ALARM_CURRMETER_COMM_FAILED = 52,//电流表通信中断
	ALARM_CTRL_BOARD_COMM_FAILED = 53,//充电控制板通信中断
	ALARM_METER_COMM_FAILED = 54,//电能表通信中断
	ALARM_CARD_READER_COMM_FAILED = 55,//刷卡机通信中断
	ALARM_SLAVE_COMM_FAILED = 56,//并机从机通信中断
	ALARM_INSULATION_FAULT = 61,//系统绝缘故障    System insulation fault
	ALARM_OUTPUT_SHORT_CIRCUIT_FAULT = 62,//Output short-circuit fault
	ALARM_OUTPUT_CONTACTOR_ABNORMAL = 63,//输出接触器状态异常 Output contactor abnormal
	ALARM_AUX_POWER_CONTACTOR_ABNORMAL = 64,//辅助电源接触器异常  Auxiliary power contactor abnormal
	ALARM_BINGJI_CONTACTOR_ABNORMAL = 65,//系统并机接触器异常  System and machine contactor abnormal
	//=====================================================
	ALARM_DOOR_NO_CLOSE_FAULT = 66,//门禁故障66
	
	
}e_ALARM_ID;

/*
下表为GPIO对应表，要控制哪个PIN仅需要调用函数ioctl(ttyled_fd,value,pin);  其中PIN 为下面对应的0---21, value为0，1。
"GPIO2_2",//run_led  0
"GPIO3_27",//fault_led  1
"GPIO3_21",//LED1_CTL  2
"GPIO3_20",//LED_E1_CTL  3
"GPIO2_21",//LED2_CTL   4
"GPIO2_20",//LED_E2_CTL  5
"GPIO3_2", //R/D_CTL1  6
"GPIO3_3", //R/D_CTL2  7
"GPIO0_21", //R/D_CTL3  8
 
"GPIO0_17", //R/D_CTL4   9
"GPIO2_0",//CS  10
"GPIO2_10", //clk  11
"GPIO2_8", //data  12
"GPIO3_28",//RLY1_CTL  13
"GPIO2_9",//RLY2_CTL 14
"GPIO2_7",//RLY3_CTL 15
"GPIO2_6",//RLY4_CTL 16
 
"GPIO2_5",//RLY5_CTL 17
"GPIO2_4",//RLY6_CTL 18
"GPIO2_3",//RLY7_CTL 19
"GPIO3_29",//RLY8_CTL 20
"GPIO2_1",//RESET  21
*/
/******************************************************************************************************************************
           j29                                  j31                       j30                                     j21
      | K1   K2   K3  K4  KC1 |       | K5   K6   K7  K8  KC2 |     |  K9     KC3   K10    K11    KC4   K12  |  |  PE  0V  5V |
      | K3   K2   K1  K8 COM1 |       | K7   K6   K5  K4 COM2 |     | LEDE1  COM1   LED1   LEDE2  COM2  LED2 |  | PGND GND +5V|
        |    |    |   |                 |    |    |   |                |             |      |            |
        |    |    |   |                 |    |    |   |                |             |      |            |
Pin:   19   18   17   16                15   14   13  20               3             2      5            4
 
value:  1   1    1    1                 1    1    1    1               0             0      0            0
 
端子标号 |*******J13****| |*********J18*********| |*****J14******| |*******J16****| |*******J15****|
信号标示 | PWM+ PWM- 空 | | TX RX GND TX RX GND | | B  A   B  A  | | H  L   H  L  | | B  A   B  A  |
丝印标号     VOICE            COM1       COM2       COM3   COM4      CAN1   CAN2      COM5   COM6

PCB标号                      232_1      232_2       485_2  485_3      CAN1  CAN2     485_1   485_4
驱动标号                    ttySP4     ttyAM0      ttySP2  ttySP3     can1  can0     ttySP1  ttySP0
驱动IO号                                            3_3(7) 3_2(6)                   0_21(8) 0_17(9)
                             读卡器     pc后台     接口板  电能表                   充电模块 并机接口
                                                  电压/电流
                                                   AC模块
监控板：
KY1： 输出接触器2控制信号
KY2： 辅助电源2开关输入
KY3： 输出接触器1控制信号
KY4： 辅助电源1控制信号

KY5： 门电子锁
KY6： 充电枪1 电子锁
KY7： 充电枪2 电子锁
KY8： 控制并机开关（控制直流输出和通信线并机）

KY9：  故障指示灯
KY10： 充电指示灯1
KY11： 保留
KY12： 充电指示灯2

接口板：
KY1： 紧急停止开关输入
KY2： 辅助电源1开关输入
KY3： 辅助电源2开关输入
KY4： 输出接触器1开关输入
KY5： 输出接触器2开关输入
KY6： 门位置输入
KY7： 并机开关输入
************************************************************************************************************/
//光继电器常断，给低电平闭合
#define GUN2_GROUP1_RUN_LED_ON           Led_Relay_Control(2, 0)//K10
#define GUN2_GROUP1_RUN_LED_OFF          Led_Relay_Control(2, 1)
#define GUN2_GROUP2_RUN_LED_ON           Led_Relay_Control(4, 0)//K12
#define GUN2_GROUP2_RUN_LED_OFF          Led_Relay_Control(4, 1)
#define GROUP1_FAULT_LED_ON         Led_Relay_Control(3, 0)//K9
#define GROUP1_FAULT_LED_OFF        Led_Relay_Control(3, 1)

#define GROUP1_RUN_LED_ON           Led_Relay_Control(15, 1)//K5
#define GROUP1_RUN_LED_OFF          Led_Relay_Control(15, 0)
#define GROUP2_RUN_LED_ON           Led_Relay_Control(14, 1)//K6
#define GROUP2_RUN_LED_OFF          Led_Relay_Control(14, 0)
#define GROUP3_RUN_LED_ON           Led_Relay_Control(13, 1)//K7
#define GROUP3_RUN_LED_OFF          Led_Relay_Control(13, 0)
#define GROUP4_RUN_LED_ON           Led_Relay_Control(20, 1)//K8
#define GROUP4_RUN_LED_OFF          Led_Relay_Control(20, 0)


#define  VO_CHIP_SELECT_H      Led_Relay_Control(10, 1)   //拉高片选信号
#define  VO_CHIP_SELECT_L      Led_Relay_Control(10, 0)   //拉低片选信号
#define  VO_CLOCK_SIGNAL_H     Led_Relay_Control(11, 1)   //拉高时钟信号
#define  VO_CLOCK_SIGNAL_L     Led_Relay_Control(11, 0)   //拉低时钟信号
#define  VO_DATA_SIGNAL_H      Led_Relay_Control(12, 1)   //拉高数据信号
#define  VO_DATA_SIGNAL_L      Led_Relay_Control(12, 0)   //拉低数据信号
#define  VO_RESET_H            Led_Relay_Control(21, 1)   //拉高复位信号
#define  VO_RESET_L            Led_Relay_Control(21, 0)   //拉低复位信号

#define GROUP1_BMS_Power24V_ON       Led_Relay_Control(19, 1)
#define GROUP1_BMS_Power24V_OFF      Led_Relay_Control(19, 0)  //BMS电源24V开关控制 ，默认为12V的
#define GROUP2_BMS_Power24V_ON       Led_Relay_Control(18, 1)
#define GROUP2_BMS_Power24V_OFF      Led_Relay_Control(18, 0)  //BMS电源24V开关控制 ，默认为12V的

#define GROUP3_BMS_Power24V_ON       Led_Relay_Control(17, 1)
#define GROUP3_BMS_Power24V_OFF      Led_Relay_Control(17, 0)  //BMS电源24V开关控制 ，默认为12V的
#define GROUP4_BMS_Power24V_ON       Led_Relay_Control(16, 1)
#define GROUP4_BMS_Power24V_OFF      Led_Relay_Control(16, 0)  //BMS电源24V开关控制 ，默认为12V的


#define MODBUS_PC04_MAX_NUM         1024//32
#define MODBUS_PC02_MAX_NUM         255
#define MODBUS_PC06_MAX_NUM         128

#define  CHARGEMODNUMBER  32              //充电模块最大数量
#define  LOW_VOLTAGE_LIMINT           199*1000
#define  SHOT_VOLTAGE_LIMINT          20*1000    //短路电压上限阀值 0.001V 低于该电压认为短路

#define MAX_NUM_BUSY_UP_TIME  10  //充电交易上传最大次数
//#define MAX_MODEL_CURRENT_LIMINT  85  //模块节能模式时电流控制点默认85%

#define MAX_CHARGER_BUSY_NUM_LIMINT  5000  //充电交易数据最大纪录数目

/*Section: Macro Definitions----------------------------------------------------*/

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

#ifndef MAX
#define MAX(x, y)   (((x) < (y)) ? (y) : (x))
#endif 

#ifndef MIN
#define MIN(x, y)   (((x) < (y)) ? (x) : (y))
#endif 

#define BCD2HEX(x) (((x) >> 4) * 10 + ((x) & 0x0F))         /*20H -> 20*/
#define HEX2BCD(x) (((x) % 10) + ((((x) / 10) % 10) << 4))  /*20 -> 20H*/

#define BYTESWAP(x) ((MSB(x) | (LSB(x) << 8)))
#define BITS(x,y) (((x)>>(y))&0x00000001)   /* 判断某位是否为1 */
#define SETBITS(x,y,n) (x) = (n) ? ((x)|(1 << (y))) : ((x) &(~(1 << (y))));
#define SETBITSTO0(x,y) (x) = ((x) &(~(1 << (y))));
#define SETBITSTO1(x,y) (x) = ((x)|(1 << (y)));
#define INVERSE(x,y)    (x) = ((x)^(1<<y));  /* 给某位置反 */

//Big-Endian
#define BigtoLittle16(A) ((((UINT16)(A) & 0xff00) >> 8)|(((UINT16)(A) & 0x00ff) << 8))
#define BigtoLittle32(A) ((((UINT32)(A) & 0xff000000) >> 24)|(((UINT32)(A) & 0x00ff0000) >> 8)|(((UINT32)(A) & 0x0000ff00) << 8)|(((UINT32)(A) & 0x000000ff) << 24))

//Little-Endian to Big-Endian
#define LittletoBigEndian16(A) ((((UINT16)(A) & 0xff00) >> 8)|(((UINT16)(A) & 0x00ff) << 8))
#define LittletoBigEndian32(A) ((((UINT32)(A) & 0xff000000) >> 24)|(((UINT32)(A) & 0x00ff0000) >> 8)|(((UINT32)(A) & 0x0000ff00) << 8)|(((UINT32)(A) & 0x000000ff) << 24))
//本系统ARM9260使用小端模式


#define OK 0
#define ERROR8 0xFF
#define ERROR16 0xFFFF
#define ERROR32 0xFFFFFFFF


/*------------------------------------------------------------------------------
Section: Type Definitions
------------------------------------------------------------------------------*/
typedef char                INT8;
typedef short int           INT16;
typedef int                 INT32;
typedef long long           INT64;

typedef unsigned char       UINT8;
typedef unsigned short int  UINT16;
typedef unsigned int        UINT32;
typedef unsigned long long  UINT64;

typedef unsigned char       byte;
typedef unsigned char       bits;

typedef unsigned char       uchar;

typedef unsigned short      STATUS;

typedef union {
    unsigned long longValue;
    unsigned char array[4];
    struct{unsigned short high,low;} shortValue;
    struct{unsigned char highest,higher,middle,low;}charValue;
}U_UINT32;


#ifndef FALSE
typedef enum  Boolean
{
   FALSE = 0,
   TRUE  = 1
}boolean;
#endif 

/*文件相关目录定义*/
#define FILENUM  4               /* 文件数量 */
#define NAMENUM 100              /* 文件名大小 */

#define MAXFRAMESIZE   255       /* 最大报文长度 */
#define MAX_MODBUS_FRAMESIZE 255 /* 最大报文长度 */

/* 参数，电量的存储目录宏定义*********************************************
*1.告警日志(EventLog)
*掉电需要保存，最大保存条数1万条，数据以.csv文件格式保存，最新发生的事件序号最高，
*排列按照序号从高到低排列
*文件初始设定路径：/mnt/Nand1/ea_app//data/eventlog
*保存格式：事件发生时间，事件ID号
*ID号对应数据用结构体对应：
*2.日常日志
*掉电需要保存，最大保存条数1万条，数据以.csv文件格式保存，单相的单独建立一个文件，
*初始路径：/mnt/Nand1/ea_app/data1phase，三相建立一个文件，路径：/mnt/Nand1/ea_app/data3phase
*保存格式：保存序号，保存时间，（三相）输入电压，（三相）输出电压，输入频率，旁路频率，
*输出频率，负载，温度*
**************************************************************************/
#define F_PATH_Nand1        "/mnt/Nand1/" 
#define F_INIT              "/mnt/Nand1/ea_init/"		//出厂mac、序列号等不可删除文件夹

#define F_PATH              "/mnt/Nand1/ea_app/"   
#define F_PARAMPATH         "/mnt/Nand1/ea_app/param"                  /* 参数文件 */
#define F_UPSPARAM          "/mnt/Nand1/ea_app/param/ups"     /* 充电机参数配置 */
#define F_UPSPARAM2         "/mnt/Nand1/ea_app/param/ups2"    /* 充电机参数配置2 */

#define F_SETNET_104        "/mnt/Nand1/ea_app/param/netset_104"         /* 网络设置 */

//
#define F_SETNET "/mnt/Nand1/DisParam/setnet"
#define F_BAK_SETNET "/mnt/Nand1/DisParam/setnet.bak"   /* 本机网络设置 */

/* 数据文件 */
#define F_DATAPATH          "/mnt/Nand1/ea_app/data"
#define F_DATA_LOGPATH      "/mnt/Nand1/ea_app/data/log"

//#define F_CHARGER_BUSY_COMPLETE_DB   "/mnt/Nand1/ea_app/data/busy_coplete.db"  /* 充电完成数据库*/
#define F_CHARGER_CARD_DB            "/mnt/Nand1/ea_app/data/card.db"  /* 黑名单数据库*/

#define F_CarLock_Busy_DB              "/mnt/Nand1/ea_app/data/CarLock_Busy.db"  /* 地锁停车费数据库*/

#define F_UPDATE                      "/mnt/Nand1/update/"
#define F_UPDATE_EAST_DC_CTL          "/mnt/Nand1/update/EASE_DC_CTL.bin"
#define F_UPDATE_Electric_Charge_Pile "/mnt/Nand1/update/Electric_Charge_Pile"
#define F_UPDATE_Charger              "/mnt/Nand1/update/charger"
#define F_UPDATE_Charger_Package      "/mnt/Nand1/update/DC_Charger.tar.gz"  //下载升级包 直流充电桩升级包
#define F_UPDATE_Charger_Conf         "/mnt/Nand1/update/charger.conf"       //参数配置文件，里面注释需要删除的配置文件及其他
#define F_UPDATE_CHECK_INFO           "/mnt/Nand1/update/update.info"

#define F_RESOLV_CONF       "/etc/resolv.conf"  //域名解析配置文件

/* 参数备份 */
#define F_BACKUPPATH             "/mnt/Nand1/ea_app/backup/"
#define F_BAK_UPSPARAM           "/mnt/Nand1/ea_app/backup/param/ups.bak"      /* 充电机参数配置备份文件 */
#define F_BAK_UPSPARAM2          "/mnt/Nand1/ea_app/backup/param/ups2.bak"     /* 充电机参数2配置备份文件 */

#define F_BAK_SETNET_104         "/mnt/Nand1/ea_app/backup/param/netset_104.bak"   /* 网络设置 */

#define F_PARAMFixedpirce         "/mnt/Nand1/ea_app/param/Fixedpirce"    /* 定时电价 */
#define F_BAK_PARAMFixedpirce         "/mnt/Nand1/ea_app/backup/param/Fixedpirce.bak"    /* 定时电价 */

#define F_Charger_Shileld_Alarm         "/mnt/Nand1/ea_app/param/Charger_Shileld_Alarm" //参数配置文件，里面注释需要删除的配置文件及其他
#define F_BAK_Charger_Shileld_Alarm     "/mnt/Nand1/ea_app/backup/param/Charger_Shileld_Alarm.bak" //参数配置文件，里面注释需要删除的配置文件及其他


#define PRICE_DIVID_FACTOR    100000   //单价5位小数点，单位元  100//2位小数
#define ISO8583_DEBUG         1  //
/*4.系统参数和配置参数
掉电保存配置参数，采取文件方式保存，在每次上电时候读取一次进行初始化，将文件值赋值到相应的结构体中
*/
typedef struct _card_lock_bcdtime{
	UINT8 year_H;
	UINT8 year_L;
	UINT8 month;
	UINT8 day_of_month;
	UINT8 hour;
	UINT8 minute;
	UINT8 second;
}card_lock_bcdtime;

typedef struct _Card_Lock_Info{
	card_lock_bcdtime card_lock_time;  //锁卡时间
	UINT32 Unlock_card_setp;           //查询锁卡步骤
	UINT32 User_card_unlock_Requestfalg;   //向后台访问数据查询
	UINT32 Card_unlock_Query_Results;   //查询结果后台/本地，0--查询中 1--记录显示中 2--失败，卡号无效 3--无效充电桩序列号 4--未找到充电记录 5--MAC 出差或终端为未对时	
	UINT32 User_card_unlock_Data_Requestfalg; //向后台发送用户卡解锁数据上报接口
	UINT32 Deduct_money;       //需扣款金额
	UINT32 Local_Data_Baseid_ID; //本地数据库上的ID
	UINT8  card_sn[16];//需要解锁的卡号
	UINT8  busy_SN[20];        //交易流水号	11	N	N20
}Card_Lock_Info;

typedef struct _Control_Upgrade_Firmware{
	UINT32 Firmware_ALL_Bytes;
	UINT32 Firware_CRC_Value;
	UINT32 Send_Tran_All_Bytes_0K; //下发成功字节
	UINT32 firmware_info_success;    //下发升级成功标志
	UINT32 firmware_info_setStep; 
	UINT32 firmware_info_setflg; 
}Control_Upgrade_Firmware;

typedef struct
{
    UINT32 stop_price_ver;			// 停车资费版本号
    UINT32 stop_price;     			// 停车费 0.00001元/bit
    UINT8  is_free_when_charge;		// 充电中是否免费 01-免费，02-不免费
    UINT16 free_time;			    // 充电前后免费时长 分钟/bit
}STOP_PRICE;

typedef struct _CHARGER_PARAM{
	UINT32 Charge_rate_voltage;		  //充电机额定电压  0.001V
	UINT32 Charge_rate_current[CONTROL_DC_NUMBER];		//充电机x额定电流  0.001A
	UINT32 Charge_rate_number[CONTROL_DC_NUMBER];		  //模块x组数量
	UINT32 model_rate_current;		  //单模块额定电流  0.001A
	UINT32 set_voltage;				      //设置电压  0.001V   主要是为了保存手动设置电压电流
	UINT32 set_current;				      //设置电流  0.001A
	UINT32 gun_config;				      //充电枪数量 1--单枪-2-双枪，3-三枪...
	UINT32 Input_Relay_Ctl_Type;		//交流采集盒继电器控制 0--无作用，1-控制给模块供电的交流接触器 2-控制风机 3-模块和风机
	UINT32 AC_Meter_CT;       //充电枪类型 //修改为交流变比
	UINT32 gun_allowed_temp;        //充电枪温度最大允许充电温度
	
	//UINT32 couple_flag;		          //充电机单、并机标示 1：单机 2：并机
	//UINT32 charge_addr;		          //充电机地址偏移，组要是在并机模式时用，1、N
	UINT32 ftp_upgrade_notify; //升级完成后上报升级结果，值0x55AA请求有效
	UINT32 ftp_upgrade_result; //升级结果，值0升级失败，值1升级成功
	
	UINT32 limit_power;		          //输出功率限制点(离线时用的默认功率限制值)  1kw

    UINT8  limit_type[CONTROL_DC_NUMBER];  // 功率调节指令 01H：功率绝对值 02H: 功率百分比
    UINT32 limit_VAL[CONTROL_DC_NUMBER];   // 功率调节参数


	UINT32 current_limit[CONTROL_DC_NUMBER];           //最大输出电流,每个枪单独设置
	UINT32 meter_config_flag;		    //电能表配置标志 0:无电能表  1：DCM3366Q  2:DCM3366
	
	UINT32 Model_Type;               //模块类型：0-华为模块 1-英可瑞模块 2-英飞源模块 3-其他类型模块
	UINT32 System_Type;		           //系统类型---0--双枪同时充 ---1双枪轮流充电 2--单枪立式 3-壁挂式

	UINT32 Price;                   //单价,精确到分
	UINT32 Serv_Type;               //服务费收费类型 1：按次收费；2：按时间收费(单位：分/10分钟；3：按电量
	UINT32 Serv_Price;              //服务费,精确到分 默认安电量收
	

	UINT32 input_over_limit;		    //输入过压点  0.001V
	UINT32 input_lown_limit;		    //输入欠压点  0.001V
	UINT32 output_over_limit;		    //输出过压点  0.001V
	UINT32 output_lown_limit;		    //输出欠压点  0.001V
	UINT32 BMS_work;		            //系统工作模式  1：正常，2：测试
	UINT32 BMS_CAN_ver;             //BMS的协议版本 1：国家标准 2：普天标准-深圳版本
	UINT32 DC_Shunt_Range[CONTROL_DC_NUMBER];          //直流分流器量程设置;	
	UINT32 charge_modluetype;
	UINT32 AC_DAQ_POS;		          //交流采集板的接线位置 0--屏幕 1-控制板
	UINT32 Serial_Network;		      //与后台通讯方式 1---串口模式 0--网络模式

	UINT8  SN[16];		              //充电桩编号 16 位数 数字字符串表示
	UINT8  Power_meter_addr[CONTROL_DC_NUMBER][12];		//电能表地址 12字节
	UINT32 DTU_Baudrate;		        //DTU波特率
	UINT8  Key[6];
	UINT8  NEW_VOICE_Flag; //1-新语音
	
	UINT8  AC_meter_addr[12];		    //AC电能表地址 12字节
	
	UINT8  SOC_limit;	//SOC达到多少时，判断电流低于min_current后停止充电,值95表示95%
	UINT8  min_current;//单位A  值30表示低于30A后持续1分钟停止充电
	
	UINT32 charge_modlue_index;         //模块具体型号索引--QT界面设置
	UINT32 charge_modlue_factory;               //模块类型：0-华为模块 1-英可瑞模块 2-英飞源模块 3-永联模块，4-其他类型模块
	
	UINT8  heartbeat_idle_period;//空闲心跳周期 秒
	UINT8  heartbeat_busy_period;//充电心跳周期 秒
	// 0-连续供电常开
	// 1-连续供电常闭
	// 2-脉冲供电常开
	// 3-脉冲供电常闭
	UINT8	Charging_gun_type;//
	
	UINT8  COM_Func_sel[10]; //COM口功能定义数组0--COM1 1---COM2...
	UINT8  support_double_gun_to_one_car;//是否启用双枪对一车充电
	UINT8  support_double_gun_power_allocation;//是否启用双枪功率分配
	UINT8  Power_Contor_By_Mode;     //功率控制方式1-通过监控屏控制，0-通过CCU/TCU控制
	UINT8  Contactor_Line_Num;  //系统并机数量
	UINT8  Contactor_Line_Position[5];  //系统并机路线节点：高4位为主控点（即控制并机的ccu）地4位为连接点
	UINT8  CurrentVoltageJudgmentFalg;//0-不判断 1-进行判断。收到的电压电流和CCU发送过来的进行判断
	UINT8  pad[90];//预留空间
	/////////////////////////////////////////////////////////////
	char dev_location[64];//设备使用地点
	char dev_user_info[64];//用户自定义信息

        /********以下为兼容二代协议的数据********/
    char   gunQrCode[CONTROL_DC_NUMBER][256];   // 枪二维码前缀（当需要拼接时为前缀）
    UINT8  IsWhloeFlag[CONTROL_DC_NUMBER];      // 是否需要拼接 0 需要拼接 01不需要拼接
    char   devSn[14];		                    // 设备序列号
    char   tcuNo[2];                            // TCU编号

	int sgp_standby_msg_period;					// 待机的时机间隔
	int sgp_chgbusy_msg_period；				// 充电时候的消息间隔
}CHARGER_PARAM;



typedef struct _CHARGER_PARAM2{
	UINT8 invalid_card_ver[5];		  //黑名单版本号
	UINT8 APP_ver[5];		            //APP下载地址版本号
    UINT8 Software_Version[5];     // 软件版本号
	UINT8 http[55];		             //APP下载网址
	UINT8 reserve1[25];		        //保留30字节 //100
	
	//当前使用的分时电价信息和时段信息
    UINT8  price_type_ver[5];	   // 资费策略版本号
    UINT32 show_price_type;        // 电价显示方式 0--详细显示 1--总电价显示
	UINT32 time_zone_num;  //时段数
    UINT32 share_time_kwh_price[TIME_SHARE_SECTION_NUMBER];       // 分时电价0.00001元/度
    UINT32 time_zone_tabel[TIME_SHARE_SECTION_NUMBER];            // 时段表
    UINT32 time_zone_feilv[TIME_SHARE_SECTION_NUMBER];            // 时段表位于的价格
    UINT32 share_time_kwh_price_serve[TIME_SHARE_SECTION_NUMBER]; // 服务费0.00001元/度
	
	UINT32 book_price;//0.00001元/次预约费

    STOP_PRICE charge_stop_price;   // 停车费

	UINT8 reserve2[84];		        //保留84字节 共300字节
}CHARGER_PARAM2;

typedef struct _CHARGER_SHILELD_ALARMFALG{
	unsigned char Shileld_PE_Fault:1;	       //屏蔽接地故障告警 ----非停机故障，指做显示	    
	unsigned char Shileld_AC_connect_lost:1;	//屏蔽交流输通讯告警	---非停机故障，只提示告警信息    
	unsigned char Shileld_AC_input_switch_off:1;//屏蔽交流输入开关跳闸-----非停机故障交流接触器
	unsigned char Shileld_AC_fanglei_off:1;    //屏蔽交流防雷器跳闸---非停机故障，只提示告警信息 
	unsigned char Shileld_circuit_breaker_trip:1;//交流断路器跳闸.2---非停机故障，只提示告警信息 
	unsigned char Shileld_circuit_breaker_off:1;// 交流断路器断开.2---非停机故障，只提示告警信息 
	unsigned char Shileld_AC_input_over_limit:1;//屏蔽交流输入过压保护 ----  ---非停机故障，只提示告警信息 
	unsigned char Shileld_AC_input_lown_limit:1; //屏蔽交流输入欠压 ---非停机故障，只提示告警信息 
	
	unsigned char Shileld_GUN_LOCK_FAILURE:1;  //屏蔽枪锁检测未通过
	unsigned char Shileld_AUXILIARY_POWER:1;   //屏蔽辅助电源检测未通过
	unsigned char Shileld_OUTSIDE_VOLTAGE:1;    //屏蔽输出接触器外侧电压检测（标准10V，放开100V）
	unsigned char Shileld_SELF_CHECK_V:1;      //屏蔽自检输出电压异常
	unsigned char Shileld_CHARG_OUT_OVER_V:1;  //屏蔽输出过压停止充电   ----  data2
  unsigned char Shileld_CHARG_OUT_Under_V:1;  //屏蔽输出欠压停止充电   ----  data2
	unsigned char Shileld_CHARG_OUT_OVER_C:1;  //屏蔽输出过流
	unsigned char Shileld_OUT_SHORT_CIRUIT:1;  //屏蔽输出短路
	
	unsigned char Shileld_GUN_OVER_TEMP:1;     //屏蔽充电枪温度过高
	unsigned char Shileld_INSULATION_CHECK:1;   //屏蔽绝缘检测
	unsigned char Shileld_OUT_SWITCH:1;       //屏蔽输出开关检测
	unsigned char Shileld_OUT_NO_CUR:1;       //屏蔽输出无电流显示检测	
	unsigned char Shileld_BMS_WARN_Monomer_vol_anomaly:1;          //屏蔽BMS状态:单体电压异常
	unsigned char Shileld_BMS_WARN_Soc_anomaly:1;          //屏蔽BMS状态:SOC异常
	unsigned char Shileld_BMS_WARN_over_temp_anomaly:1;          //屏蔽BMS状态:过温告警
	unsigned char Shileld_BMS_WARN_over_Cur_anomaly:1;          //屏蔽BMS状态:BMS状态:过流告警
	
	unsigned char Shileld_BMS_WARN_Insulation_anomaly:1;          //BMS状态:绝缘异常
	unsigned char Shileld_BMS_WARN_Outgoing_conn_anomaly:1;          //BMS状态:输出连接器异常
	unsigned char Shileld_CELL_V_falg:1;       //是否上送单体电压标志
	unsigned char Shileld_CELL_T_falg:1;      //是否上送单体温度标志
	unsigned char Shileld_output_fuse_switch:1;//输出熔丝段
	unsigned char Shileld_BMS_OUT_OVER_V:1;  //屏蔽输出过压停止充电 与BMS电压对比 包括单体
  unsigned char Shileld_door_falg:1;
  unsigned char Shileld_keep_data:1;
	
	unsigned char Shileld_Control_board_comm:1;// 控制板通信异常
	unsigned char Shileld_meter_comm:1;       //      电表通信异常
	unsigned char Shileld_keep_comm:6;       //  监控屏上的其他异常

  unsigned char Shileld_keep[4];
}CHARGER_SHILELD_ALARMFALG; 


typedef struct _APP_CHARGER_PARAM{
	UINT32 APP_start;         //0, 结束充电 1, 启动充电

	UINT32 APP_BMS_type;      //0，BMS控制充电 1，盲充

	UINT32 APP_charge_mode;   //1，电量控制充电 2，时间控制充电 3，金额控制充电 4，充满为止
	UINT32 APP_charge_money_limit;   //充电金额   单位为“元”，精确到0.01
	UINT32 APP_charge_kwh_limit;     //充电电量   单位为“度”，精确到0.01
	UINT32 APP_charge_sec_limit;    //充电时间   单位为“秒“
	UINT32 APP_pre_time;       //预约时间   单位为”分”

	UINT8  ID[16];		         //用户标识 12 位数 字符串表示
	UINT32 last_rmb;           //账号余额	10	N	N8	单位为“分“
	UINT8  busy_SN[20];        //交易流水号	11	N	N20
    
    UINT8  userType;           // 用户类型


	UINT8 APP_Start_Account_type;//app启动账户类型用户类型：00：表示本系统用户，01：表示第三方系统用户
        
	UINT32 APP_book_start;  //0, 取消预约充电 1, 预约充电	
    UINT32 APP_book_time;   // 预约时长，单位：秒
    UINT64 APP_book_Start;  //开始时间，单位：秒
}APP_CHARGER_PARAM;


typedef struct _CHARGER_ERROR{
	UINT32 emergency_switch;        //1  急停按下故障 (控制板开关输入 2路) 
	UINT32 PE_Fault;                //2  系统接地故障
	UINT32 Meter_Fault;             //3  测量芯片通信故障
	UINT32 AC_connect_lost;         //4  交流模块通信中断
	UINT32 charg_connect_lost[33];  //5  充电模块通信中断 1# ~32#  数据填充是从[1]~[32]，故定义33个数组

	UINT32 charg_fault[33];         //6  充电模块故障     1# ~32#  数据填充是从[1]~[32]，故定义33个数组
	UINT32 charg_fun_fault[33];     //7  充电模块风扇故障 1# ~32#  数据填充是从[1]~[32]，故定义33个数组
	UINT32 charg_AC_alarm[33];      //8  充电模块交流输入告警 1# ~32#  数据填充是从[1]~[32]，故定义33个数组
	UINT32 charg_out_shout[33];     //9  充电模块输出短路故障 1# ~32#  数据填充是从[1]~[32]，故定义33个数组
	UINT32 charg_out_OC[33];        //10 充电模块输出过流告警 1# ~32#  数据填充是从[1]~[32]，故定义33个数组

	UINT32 charg_out_OV[33];        //11 充电模块输出过压告警 1# ~32#  数据填充是从[1]~[32]，故定义33个数组
	UINT32 charg_out_LV[33];        //12 充电模块输出欠压告警 1# ~32#  数据填充是从[1]~[32]，故定义33个数组
	UINT32 charg_in_OV[33];         //13 充电模块输入过压告警 1# ~32#  数据填充是从[1]~[32]，故定义33个数组
	UINT32 charg_in_LV[33];         //14 充电模块输入欠压告警 1# ~32#  数据填充是从[1]~[32]，故定义33个数组
	UINT32 charg_in_lost[33];       //15 充电模块输入缺相告警 1# ~32#  数据填充是从[1]~[32]，故定义33个数组

	UINT32 charg_over_tmp[33];      //16 充电模块过温告警 1# ~32#     数据填充是从[1]~[32]，故定义33个数组
	UINT32 charg_OV_down[33];       //17 充电模块过压关机告警 1# ~32# 数据填充是从[1]~[32]，故定义33个数组
	UINT32 charg_other_alarm[33];   //18 充电模块其他告警 1# ~32#     数据填充是从[1]~[32]，故定义33个数组
	UINT32 output_over_limit;		    //19 系统输出过压告警
	UINT32 output_low_limit;        //20 系统输出欠压告警

	UINT32 sistent_alarm;		        //21 系统绝缘告警
	UINT32 input_over_limit;		    //22 交流输入过压告警
	UINT32 input_over_protect;		  //23 交流输入过压保护
	UINT32 input_lown_limit;		    //24 交流输入欠压告警
	UINT32 input_switch_off;        //25 交流输入开关跳闸

	UINT32 fanglei_off;             //26 交流防雷器跳闸
	UINT32 output_switch_off[2];    //27 直流输出开关跳闸
	UINT32 fire_alarm;              //28 烟雾告警
	UINT32 sys_over_tmp;            //29 充电桩过温故障
	UINT32 gun_over_temp_alarm[2];   //30 充电枪头过温
	UINT32 charg_Output_overvoltage_lockout[33];   //30 充电模块其他告警 1# ~32#     数据填充是从[1]~[32]，故定义33个数组
	UINT32 charg_protection[33];   //31 充电模块保护
	UINT32 Ac_circuit_breaker_trip; //33交流断路器跳闸
	UINT32 AC_circuit_breaker_off;  //34交流断路器断开

	UINT32 voltage_connect_lost;     //51 电压表通信中断
	UINT32 current_connect_lost;     //52 电流表通信中断
	UINT32 ctrl_connect_lost;        //53 充电控制板通信中断
	UINT32 meter_connect_lost;       //54 电表表通信中断
	UINT32 IC_connect_lost;          //55 读卡器通信中断
	UINT32 COUPLE_connect_lost;      //56 并机从机通信中断

	UINT32 Door_No_Close_Fault;      //门禁故障
	UINT32  gun_lock_Fault; //枪锁故障
	
	UINT32 system_sisitent;          //61 系统绝缘故障
	UINT32 output_shout;	           //62 输出短路故障
	UINT32 output_switch[2];         //63 输出接触器异常
	UINT32 assist_power_switch[2];   //64 辅助电源接触器异常
	UINT32 COUPLE_switch;            //65 系统并机接触器异常
	UINT32 breaker_trip;
	UINT32 killer_switch;

	UINT32 charger_connect_lost;  // 充电模块通信中断 
	UINT32 charger_fun_fault;     // 充电模块风扇故障 
	UINT32 charger_over_tmp;      // 充电模块过温告警 
	UINT32 charger_OV_down;       // 充电模块过压关机告警  
	UINT32 charger_other_alarm;   // 充电模块其他告警 
	UINT32 charger_fault;          //// 充电模块故障 
	UINT32 Power_Allocation_ComFault;//与功率分配柜通信异常
  UINT32 Power_Allocation_SysFault; //功率分配柜系统异常

	UINT32 pre_emergency_switch;        //先前状态 急停按下故障 (控制板开关输入 2路)
	UINT32 pre_AC_connect_lost;         //先前状态 交流模块通信中断
	UINT32 pre_charger_connect_lost;  //先前状态 充电模块通信中断 
	UINT32 pre_charger_fun_fault;     //先前状态 充电模块风扇故障 
	UINT32 pre_charger_over_tmp;      //先前状态 充电模块过温告警 
	UINT32 pre_charger_OV_down;       //先前状态 充电模块过压关机告警  
	UINT32 pre_charger_other_alarm;   //先前状态 充电模块其他告警  
	UINT32 pre_output_over_limit;		    //先前状态 系统输出过压告警
	UINT32 pre_output_low_limit;        //先前状态 系统输出欠压告警
	UINT32 pre_sistent_alarm;		        //先前状态 系统绝缘告警
	UINT32 pre_input_over_limit;		    //先前状态 交流输入过压告警
	UINT32 pre_input_over_protect;		  //先前状态 交流输入过压保护
	UINT32 pre_input_lown_limit;		    //先前状态 交流输入欠压告警
	UINT32 pre_input_switch_off;        //先前状态 交流输入开关跳闸

	UINT32 pre_fanglei_off;             //先前状态 交流防雷器跳闸
	UINT32 pre_output_switch_off[2];    //先前状态 直流输出开关跳闸
	UINT32 pre_sys_over_tmp;            //先前状态 充电桩过温故障
	UINT32 pre_ctrl_connect_lost;     //先前状态 充电控制板通信中断
	UINT32 pre_meter_connect_lost;       //先前状态 电表表通信中断
	UINT32 pre_system_sisitent;          //先前状态 系统绝缘故障
	UINT32 pre_output_shout;	           //先前状态 输出短路故障
	UINT32 pre_output_switch[2];         //先前状态 输出接触器异常
	UINT32 pre_assist_power_switch[2];   //先前状态 辅助电源接触器异常
	UINT32 pre_COUPLE_switch;            //先前状态 系统并机接触器异常
	UINT32 pre_charger_fault;          //// 先前状态  充电模块故障
	UINT32 reserve30;  
}CHARGER_ERROR;

typedef struct _ISO8583_SERVER_NET_SETTING{
	UINT8 ISO8583_Server_IP[50];  //字符串形式保存  例如 192.168.162.133  为15个字符
	UINT8 ISO8583_Server_port[2]; //低在前高在后 端口80： 0x50 0x00
	UINT8 addr[2];	
}ISO8583_SERVER_NET_SETTING;


// typedef struct _CHARGER_MSG{
    // long  type;
    // UINT8 argv[MAX_MODBUS_FRAMESIZE];
// }CHARGER_MSG;

typedef struct _UART_M4_MSG{
    long  type;
    UINT8 argv[MAX_MODBUS_FRAMESIZE];
}UART_M4_MSG;

typedef struct _AC_MEASURE_DATA{
	UINT8 reserve1;     //保留
	UINT8 V1_AB[2];		  //AB线电压  0.1V
	UINT8 V1_BC[2];		  //BC线电压  0.1V
	UINT8 V1_CA[2];		  //CA线电压  0.1V
	UINT8 C1_A[2];		  //A线电流   0.01A
	UINT8 C1_B[2];		  //B线电流   0.01A
	UINT8 C1_C[2];		  //C线电流   0.01A

	UINT8 PW_ACT[2];		  //总有功 10 W
	UINT8 PW_INA[2];		  //总无功 10 W
	UINT8 F1[2];		      //频率    0.01Hz

	UINT8 input_switch_on;   //输入开关状态信号，1 闭合，0 断开
	UINT8 output_switch_on1; //输出开关状态信号1，1 闭合，0 断开
	UINT8 fanglei_on;        //交流避雷器报警信号，1 闭合，0 断开
	UINT8 output_switch_on2; //输出开关状态信号1，1 闭合，0 断开
}AC_MEASURE_DATA;

AC_MEASURE_DATA AC_measure_data;


typedef struct
{
	unsigned char IC_type;                //卡片类型---用户卡或员工卡或管理卡
	unsigned char IC_state;              //卡片状态----灰锁或正常
	unsigned char IC_Read_OK;             //读到卡片 
	unsigned char IC_ID[16];             //卡号---8字节BCD码，16位数字ASCII码
	unsigned int  IC_money;         //剩余金额---用户卡中有效,bin码，精确到分,最低字节存放4字节金额的最高字节
	
	unsigned char qiyehao_flag;//0xAA企业号
	unsigned char private_price_flag;//0xAA私有电价
	unsigned char order_chg_flag;//0xAA需在指定时间段才可充电
	unsigned char offline_chg_flag;//0xAA离线时企业号，私有电价，有序充电卡都可以充电
	
}MUT100_DZ_CARD_INFO_DATA;//EAST M1充电卡信息

typedef struct _SEND_CARD_INFO{
	UINT32 Card_Command;			 //刷卡命令帧	--2--解锁 1--加锁 0--等待命令
	UINT32 Command_Result;		//命令结果反馈 0--等待 1--成功 2---失败 -- 卡号不一样  
	UINT32 Command_IC_money;    //消费金额 
	UINT32 Last_IC_money;    //消费金额 
	UINT8 IC_ID[16];	        //发送的卡号
}SEND_CARD_INFO;

typedef struct _FIXED_PRICE{
	UINT32 falg;            //1存在定时电价，需要更新
	UINT32 Serv_Type;               //服务费收费类型 1：按次收费；2：按时间收费(单位：分/10分钟；3：按电量
	UINT32 Serv_Price;              //服务费,精确到分 默认安电量收
	UINT32 Price;                   //单价,精确到分
	UINT32 share_time_kwh_price[4];		//分时电价  //尖电价单位分 //峰电价单位分 //平电价单位分 //谷电价单位分
	UINT32 time_zone_num;  //时段数
	UINT32 time_zone_tabel[12];//时段表
	UINT32 time_zone_feilv[12];//时段表位于的价格
	UINT32 share_time_kwh_price_serve[4]; //分时服务电价
	UINT32 show_price_type;//电价显示方式 0--详细显示 1--总电价显示
	//UINT32 IS_fixed_price;
	UINT8 fixed_price_effective_time[7];//电价更新时间
}FIXED_PRICE; //充电定时电价

typedef struct _Special_price_param{
	UINT32 Serv_Type;               //服务费收费类型 1：按次收费；2：按时间收费(单位：分/10分钟；3：按电量
	UINT32 Serv_Price;              //服务费,精确到分 默认安电量收
	UINT32 Price;                   //单价,精确到分
	UINT32 share_time_kwh_price[4];		//分时电价  //尖电价单位分 //峰电价单位分 //平电价单位分 //谷电价单位分
	UINT32 time_zone_num;  //时段数
	UINT32 time_zone_tabel[12];//时段表
	UINT32 time_zone_feilv[12];//时段表位于的价格
	UINT32 share_time_kwh_price_serve[4]; //分时服务电价
	UINT32 stop_rmb_start;//充电期间是否免停车费(01：表示免费，02：不免费)	12 N2
	UINT32 stop_time_free;// 充完电后免停车费的时长(单位：分钟)	13             N8
}SPECIAL_PRICE_PARAM;  //充电私有电价

typedef struct _Control_DC_INFOR{
	INT32  Error_system;   //系统故障计数
	INT32  Error_ctl;      //1# 充电控制单元故障计数
	INT32  Warn_system;    //1# 系统告警计数
	UINT32 ctl_state;      //1# 充电控制单元状态 0x00正常 0x01 故障。
	CHARGER_ERROR   Error;
	
	UINT32 charge_mode; //1，电量控制充电 2，时间控制充电 3，金额控制充电 4，充满为止
	UINT32 charge_money_limit;         //充电金额设定   1-9999 元 
	UINT32 charge_kwh_limit;           //充电电量设定   1-9999 kwh 
	UINT32 charge_sec_limit;       // 充电秒数设定 60---
	UINT32 pre_time;             //倒计时充电时长   0-23*60+59 min 
	UINT32 remain_hour; 
	UINT32 remain_min; 
	UINT32 remain_second; 

	UINT32 Credit_card_success_flag; //表示刷卡成功标志 1-成功刷卡 0--失败无刷卡
	UINT32 MT625_Card_Start_ok_flag; //刷卡启动充电，卡片处理成功(用户卡已锁卡)
	UINT32 MT625_Card_End_ok_flag;   //结束时刷卡成功处理了(用户卡已解锁)

	UINT32 charger_start_way;			//系统启动方式，0--刷卡 1---手机APP
	UINT32 charger_start;				 //系统开始充电 1：开始 0：停止 2 手动充电 3-VIN鉴权
	UINT32 charger_state;           //终端状态 取值只有 0x01-故障  0x02-告警  03-空闲 0x04-充电中 0x05-完成 0x06-预约
	UINT32 gun_state;              //枪状态 取值只有  0x03-空闲 0x04-充电中 0x05-完成 0x07-等待
	UINT32 Manu_start;			  //人工启动充电方式    1：自动 2：手动
	UINT32 Metering_Step;         //计量步骤
	UINT32 SYS_Step;
	UINT32 MC_Step;


	APP_CHARGER_PARAM    App;   //APP启动控制参数
	UINT8 AP_APP_Sent_flag[2]; //APP启动后的结果反馈给后台
	
	SPECIAL_PRICE_PARAM  Special_price_data; //当前充电电价--刷卡或APP
	
	UINT32 charger_over_flag;       //系统充电结束原因
	UINT32 ctl_over_flag;            //控制单元结束充电原因
	UINT32 gun_link;				//充电枪连接         1：连接 0：断开
	UINT32 gun_over_link;          //本充电枪已经充过电，是否结束过连接 1：否 0：结束过连接

	UINT32 charge_end_reason;
	
	UINT8  card_sn[16];             //用户卡号/或者是app用户名
	UINT32 last_rmb;                //充电前卡余额（以分为单位）
	UINT32 card_type;				//卡类型 1：用户卡 2：员工卡
	UINT32 IC_state;                //卡片状态 0x30正常，0x31锁卡

	UINT32 Enterprise_Card_flag;//企业卡号成功充电上传标志 1-请求上报1130报文
	UINT32 Enterprise_Card_check_result;//企业号充电卡启动成功上报响应1131的结果====暂未判断结果 1-成功 2-MAC校验错误 3-终端未对时 4-该卡未进行发卡 5-无效充电桩序列号 6-黑名单 7-卡的企业号标记不同步 8-该启动记录已上报 9-其他
	
	
	unsigned char private_price_acquireOK;//刷卡鉴权或APP启动已获取私有电价标记
	unsigned char order_card_end_time[2]; //当前有序充电卡允许充电的终止时刻 HH:MM	(二进制，order_card_end_time[0]为小时，order_card_end_time[1]为分钟)
	
	unsigned char qiyehao_flag;//0xAA企业号
	unsigned char private_price_flag;//0xAA私有电价
	unsigned char order_chg_flag;//0xAA需在指定时间段才可充电
	unsigned char offline_chg_flag;//0xAA离线时企业号，私有电价，有序充电卡都可以充电
	
	unsigned char  Bind_BmsVin[17];//后台卡片绑定的车辆识别码
	unsigned char  Bind_BmsVin_flag;//卡片是否有绑定VIN标记
	unsigned char  Updat_data_value;//升级进度
	
	UINT8  User_Card_Present;//
	UINT32 User_Card_check_flag;     //卡片校验标志
	UINT32 User_Card_check_result; //卡片校验结果
	UINT8 User_Card_check_card_ID[16];//验证卡片ID

	UINT32 meter_start_KWH;         //电表起始示数 0.01 度 用于电表计量
	UINT32 meter_stop_KWH;          //电表结束示数 0.01 度 用于电表计量
	UINT32 meter_KWH;               //电表当前示数 0.01 度
	UINT32 meter_KWH_update_flag;   //已获取电表电量标志
	UINT32 meter_voltage;//0.001V
	UINT32 meter_current;//0.001A
	UINT32 meter_voltage_update_flag;
	UINT32 meter_current_update_flag;
	
	UINT32 kwh;                     //已充电电量 0.01 度
	UINT32 Time;				    //已充电时间   1s
	UINT32 soft_KWH;                //软件累计充电电量 0.0001 度 用于软件计算
	UINT32 pre_kwh;
	unsigned long  total_rmb;     //总费用, 包括服务费（按照电量收）和电量消费金额以及预约费和停车费 精确到分
	unsigned long  rmb;          //电量消费金额 0.01元
	unsigned long  service_rmb;//总服务费 0.01元
	unsigned long  park_rmb; //停车费 0.01元
	UINT32 book_rmb; //预约费 0.01元
	UINT32 ulcharged_kwh[4];//分时充电量 0.01kwh
	unsigned long ulcharged_rmb[4];//分时充电金额 0.01元 
	unsigned long ulcharged_service_rmb[4];//分时充电服务费 0.01元
	UINT32 BMS_Power_V;  //值0-12V,值1-24V
	UINT32 led_run_flag;            //充电指示灯闪烁标示

	UINT32 deal_restore_end_flag; //记录恢复完成标志，在恢复完成前不要连服务器，避免电价被更新===停电前的记录应使用停电前的电价
	
	UINT32 need_voltage;				    //电压需求  0.001V
	UINT32 need_current;				    //电流需求  0.001A
	UINT32 Output_voltage;					//充电机输出电压 0.001V
	UINT32 Output_current;					//充电机输出电流 0.001A
	UINT32 line_v;                  //充电导引电压 精确到小数点后1位0V - 12V

	UINT32 BMS_rate_voltage;				//BMS额定电压  0.001V
	UINT32 BMS_rate_AH;				      //BMS额定容量  AH
	UINT32 Battery_Rate_KWh;         //动力蓄电池标称总能量KWH
	UINT32 BATT_type;               //BMS电池类型
	UINT32 BMS_H_voltage;				    //BMS最高允许充电电压  0.001V
	UINT32 BMS_H_current;				    //BMS最高允许充电电流  0.001A
	UINT32 BMS_batt_start_SOC; //充电开始后第1次读取到的SOC
	UINT32 BMS_batt_SOC;				    //BMS电池容量
	UINT32 BMS_cell_HV;				      //单体电池最高电压   0.01V
	UINT32 BMS_cell_LV;				      //单体电池最低电压   0.01V
	UINT32 Charge_Mode;        //8充电模式
	UINT32 BMS_cell_HV_NO;     //8最高单体电压
	UINT32 Cell_H_Cha_Vol;     //单体动力蓄电池最高允许充电电压 0.01 0-24v
	UINT8  Cell_H_Cha_Temp;       //单体充电最高保护温度 -50+200
	UINT8 BMS_Vel[3];         //BMS协议版本                                    
	UINT8 BMS_cell_HT;				      //单体电池最高温度  精确到1℃ -50℃ ~ +200℃ 值0表示-50℃
	UINT8 BMS_cell_LT;				      //单体电池最低温度  精确到1℃ -50℃ ~ +200℃ 值0表示-50℃
	UINT8 BMS_cell_LT_NO;            //5电池组最低温度编号	
	UINT8 BMS_cell_HT_NO;            //6电池组最高温度编号	
	UINT32 BMS_need_time;				     //剩余时间 秒
	UINT8  Charge_Gun_Temp;         //充电枪温度 精确到1℃ -50℃ ~ +200℃ 值0表示-50℃
	UINT32 BMS_Info_falg;		//接收到BMS信息数据反馈标志，没有接收到则上送给监控全为0
	UINT8  VIN[17];
	UINT32 Operation_command;//电子锁 解锁 1--加锁，2-- 解锁，只负责解锁
	UINT32 AC_Relay_control;//是否合节能接触器
	UINT32 Over_gun_link;	//之前的插枪状态 0-没插枪，1-插枪
	
	UINT32 BMS_pre_batt_SOC;	//BMS先前的电池容量
	
	UINT8   bms_com_failed;//BMS通信异常 0-正常 1-异常
	UINT8   bat_temp_high_alarm;//蓄电池过温 0-正常 1-过温
	UINT8   output_overload_alarm;//充电过流 0-正常 1-过流
	UINT8   plug_temp_high_fault;//充电枪过温保护 0-正常 1-过温
	UINT16 kw_percent;//枪的功率限制百分比 系数0.1 NNN.N	
	UINT8  kw_limit_end_time[7];//枪功率限制截止时间 CP56Time2a格式

	UINT8	Battery_Parameter_Sent_Flag;//是否已发送4050报文，每次充电只发送1次
	
	//UINT16 kw_percent;//枪的功率限制百分比 系数0.1 NNN.N	
	//UINT8  kw_limit_end_time[7];//枪功率限制截止时间 CP56Time2a格式
	
	UINT8	chg_start_result;//app启动充电的结果 1-成功，2-失败
	UINT8	report_chg_started;//请求上报app启动充电结果
	
	UINT8   VIN_auth_req;//在已经读取到BMS信息后请求VIN鉴权置1====
	UINT8   VIN_auth_ack;
	UINT8   VIN_auth_success;//1--成功,0-失败

	UINT16  VIN_auth_failed_reason;
	
	UINT32  VIN_auth_money;//鉴权账户余额，0.01元
	UINT8   VIN_start;//VIN鉴权成功后启动充电
	UINT8	VIN_chg_start_result;//启动充电的结果 1-成功，2-失败
	UINT8	report_VIN_chg_started;//请求上报VIN启动充电结果
	UINT8   VIN_Stop;//VIN点停止按钮
	
	UINT8   VIN_User_Id[16];//VIN关联的用户ID====add 20190218
	unsigned char Car_lock_Card[16];
	unsigned char Car_lockCmd_Type;
	unsigned char Car_lock_comfault;
	unsigned char Car_lock_have_car;
	unsigned char park_start_time[14];//停车开始时间
	unsigned char keepdata[5];//保留
	unsigned int Double_Gun_To_Car_falg;//双枪对一辆车充电标志
	unsigned int Double_Gun_To_Car_Hostgun;//1---做主枪0-做辅枪
	unsigned int Double_Gun_To_Car_Othergun;//另一把枪的枪号 0开始
	unsigned int Host_Recv_Need_A;//枪的需求电流
	unsigned int Host_Recv_BMSChargeStep_Falg;//BMS充电阶段
	unsigned int Car_AllMileage;                  //车行驶总里程
	UINT8  remote_stop;//停止充电
	UINT8  VIN_Judgment_Result;//vin 鉴权结果
	UINT32  output_kw;//0.1kw
	UINT8  charge_start_time[24];//20xx-xx-xx 17:20:35
	UINT8  charge_end_time[24];//20xx-xx-xx 17:20:35


    /*本地订单号-单枪订单号-适用于单枪或并机单枪，在启动成功后，产生并存储
     设备地址（16位）+枪号（2位）+并机充电标记（2位：00单枪常规充电 01并机汇总  
     02并机单枪)+ yyMMddHHmmss(年月日时分秒))*/
    UINT8  local_SigGunBusySn[34];   
    /*本地订单号-汇总订单号-适用于并机充电的汇总订单，在并机和从机充电都启动时产生并存储
    设备地址（16位）+枪号（2位）+并机充电标记（2位：00单枪常规充电 01并机汇总
    02并机单枪)+ yyMMddHHmmss(年月日时分秒))*/
    UINT8  local_TotalBusySn[34];

    /* 目前程序按照没有地锁处理 */
    UINT8  landLockOnResult;        // 地锁放下结果
    UINT8  landLockUserID[16];      // 用户标识
    UINT8  landLockBusySN[20];      // 地锁控制云平台订单号
	
}Control_DC_INFOR;


enum SYS_MOBULEGRP_STATE 
{
  MOD_GROUP_STATE_INACTIVE = 0, //0-未激活
  MOD_GROUP_STATE_ACTIVE = 1, //1-激活
  MOD_GROUP_STATE_ALLOCATED_CAST_GOING = 2, //2-已分配投切中
  MOD_GROUP_STATE_ALLOCATED_OK = 3, //2-已分配完成
  MOD_GROUP_STATE_USEING = 4, //2-使用中
	MOD_GROUP_STATE_ALLOCATED_EXITING = 5, //5-已分配退出中 
	MOD_GROUP_STATE_EXITOK = 6, //6-退出完成 
  MOD_GROUP_STATE_FAULT = 7,    //处理失败
	MOD_GROUP_STATE_ORDER = 8   //未分配被预定，待分配
};
//开关动作定义
enum EM_RECTIFIER_MOD_PLUGIN_OnOffAction
{
     EM_RECTIFIER_ONOFF_OFF = 0,
     EM_RECTIFIER_ONOFF_ON=1
};

//线路关系表，因为该套程序是，模块和CCU是绑定在一起的，所有不需要模块组配置
//[母线配置/枪号配置]
typedef struct _CHARGER_GUN_BUS_PARAM
{
	int reserver; //保留
	int Data_valid; //数据是否有效 1--有效
	int Bus_no;//母线号
  int Bus_to_mod_Gno;//母线固定模块组号
	int Bus_to_ccu_no;//母线对应的枪号 ---- (-1则无CCU)，不连接枪号
  int Bus_max_current;//母线最高充电电流
}CHARGER_GUN_BUS_PARAM;

//[[接触器]配置]
typedef struct _CHARGER_CONTACTOR_PARAM
{
	int reserver; //保留
	int Data_valid; //数据是否有效 1--有效
  int contactor_no;//接触器编号 ---就是两个母线之间的连接线路
  int contactor_bus_P;//正向关联母线号(目前母线号和组号是一一对应的)
	int contactor_bus_N;//反向关联母线号 （代表那两个CCU可以并机在一起）
  int contactor_maxcurrent;//接触器额定电流
  int allow_reverse;//允许反向标记(反向并机)
	int Correspond_control_unit_address;//对应控制单元(IO
  int Correspond_control_unit_no;//对应控制单元逻辑位号地址(IO
}CHARGER_CONTACTOR_PARAM;

typedef struct _SYS_GROUP_CONTROL_DATA{
  UINT8   reserve;//保留
	UINT8   Sys_Grp_OKNumber;//该组模块组通讯正常数量
  UINT8   Sys_Grp_OnOff;  //该组对应的开关状态 0-开机 1-关机
	UINT8   Sys_MobuleGrp_State;//模块组对应的状态。0-未激活 1-激活 2-已分配投切中  已分配完成 =3 使用中 =4  已分配退出中 = 5 退出完成 =6
  UINT8   Sys_MobuleGrp_Gun;  //每组模块组对应的CCU号（即组号）对应的每组身上
  UINT8   Sys_Grp_Fault_Cont;//每组本次充电累计故障次数
	UINT8   Sys_Grp_Fault;    //该组模块组是否异常被禁用
	UINT32  SYS_MobuleGrp_OutVolatge;//该模块组对应的输出电压
	UINT32  SYS_MobuleGrp_OutCurrent;//该模块组对应的输出电流
	UINT8   Control_Timeout_Count;
	UINT8   Control_Timeout_Falg;//模块组控制超时
}SYS_GROUP_CONTROL_DATA;

//每个CCU请求数据
typedef struct _SYS_CCU_DATA{
  UINT8   reserve;//保留
	UINT8   Errorsystem;//CCu故障
	UINT8   Gunoverlink;//枪连接状态
  UINT8   Gunlink;//枪连接状态
	UINT8   Charge_State;//充电状态1-开始充电 0-结束充电
	UINT8   Modbule_On;//模块开启状态
	UINT8   AC_Relay_needcontrol;//接触器控制
	UINT8   Relay_State; //接触器状态
	UINT8   need_model_num;  //充电需求模块数量
  UINT8   Charger_good_num; //该组通信正常模块数量
  UINT32  Need_Voltage;//需求电压
	UINT32  Need_Current;//需求电流
  UINT32  Output_Voltage;//实际输出电压电压
	UINT32  Output_Current;//需求电流
	UINT32  model_Output_voltage;//模块输出电压
  UINT32  model_onoff;//该组模块状态
}SYS_CCU_DATA;

typedef struct _SYS_TCUCONTROLCCUSET_DATA{
	UINT32 model_charge_on;    //该组对应的模块开启功能
	UINT32 model_output_vol;	//改组模块要输出的电压	BIN 2Byte	Mv
	UINT32 model_output_cur;	//该组模块要输出的电流	BIN 2Byte	Ma
	UINT32 Module_Readyok; //模块准备ok
  UINT32 WeaverRelay_control;//并机继电器控制
	UINT32 host_Relay_control_Output;//输出接触器控制----0--都断开 1-闭合
  UINT32 ACRelay_control;//AC继电器控制
	UINT32 keep_data;
}SYS_TCUCONTROLCCUSET_DATA;


//系统全局交互使用的唯一结构体
typedef struct _GLOBAVAL{
	UINT32 user_sys_time; //系统本次上电运行累计时间 单位 秒
	UINT32 time_update_flag; //系统时间需要跟新标志 1：是 0：否
	UINT32 year;
	UINT32 month;
	UINT32 date;
	UINT32 hour;
	UINT32 minute;
	UINT32 second;
	ISO8583_SERVER_NET_SETTING     ISO8583_NET_setting; 
	CHARGER_PARAM		Charger_param;
	CHARGER_PARAM2      Charger_param2;

	UINT32 reserve31[20];
	UINT8 tmp_card_ver[5];		  //黑名单版本号
	UINT8 tmp_price_ver[5];		  //价格版本号
	UINT8 tmp_APP_ver[5];		    //APP下载地址版本号
	UINT8 kernel_ver[20];		  //内核版本号
	UINT8 contro_ver[CONTROL_DC_NUMBER][20];		  //控制版本--107
	UINT8 protocol_ver[20];       //后台协议版本
	key_t           arm_to_qt_msg_id;
	key_t           qt_to_arm_msg_id;

	UINT32 limit_kw;					      //充电机输出限制功率  kw
	UINT32 have_hart;					      //与后台心跳标志存在 1:正常 0：否
	UINT32 have_card_change;				//黑名单版本变化 1:变化 0：否
	UINT32 have_price_change;				//电价版本变化 1:变化 0：否
	UINT32 have_APP_change;				  //APP地址变化 1:变化 0：否
	UINT32 have_APP_change1;				//给qt用，以便告知  1:变化 0：否

	UINT32 QT_Step;
	UINT32 QT_charge_time; //手动充电设置的充电时长
	Control_DC_INFOR  Control_DC_Infor_Data[CONTROL_DC_NUMBER];//最大支持2把枪直流 ，，，另一把枪没有

	Control_Upgrade_Firmware Control_Upgrade_Firmware_Info[CONTROL_DC_NUMBER];
	Card_Lock_Info   User_Card_Lock_Info;     //锁卡信息
	UINT32 Software_Version_change;
	UINT8  tmp_Software_Version[5];//升级包软件版本
	UINT32 Electric_pile_workstart_Enable[CONTROL_DC_NUMBER];     // 电桩禁用标记(1:表示正常，0表示禁用)


	UINT16   modbus_485_PC04[MODBUS_PC04_MAX_NUM];
	UINT8   modbus_485_PC02[MODBUS_PC02_MAX_NUM];//每个变量保存1位遥信量==
	UINT16   modbus_485_PC06[MODBUS_PC06_MAX_NUM];
	
	
	UINT32 Apply_Price_type;//申请的电价类型，0--当前电价 1--定时电价
	UINT32 show_price_type;//电价显示方式//分00：表示显示详细单价，01：表示只显示一个总单价	15	N	N2
	FIXED_PRICE   charger_fixed_price; //充电定时电价 --需要存储
	unsigned char Updat_data_value;//升级文件下载进度
	unsigned char  Signals_value_No;  //信号值
	unsigned char  Registration_value; //无线模块注册情况
	unsigned char  ICCID_value[20];
	MUT100_DZ_CARD_INFO_DATA MUT100_Dz_Card_Info;
	
	UINT32  ac_meter_kwh;//交流电表度数 0.01kwh
	UINT32  ac_volt[3];//3相输入电压0.1V
	UINT32  ac_current[3];//3相输入电流0.01A
	
	UINT32 ac_col_tx_cnt;//交流采集模块帧发送计数
	UINT32 ac_col_rx_ok_cnt;
	
	UINT32 ac_meter_tx_cnt;
	UINT32 ac_meter_rx_ok_cnt;
	
	UINT32 dc_meter_tx_cnt[CONTROL_DC_NUMBER];
	UINT32 dc_meter_rx_ok_cnt[CONTROL_DC_NUMBER];
	
	UINT32 server_tx_cnt;
	UINT32 server_rx_cnt;
	UINT32 server_rx_ok_cnt;
	UINT32 iso8583_login_cnt;
	

	UINT32 system_upgrade_busy;//系统升级中
	UINT32 sys_restart_flag;//系统重启标记
  	UINT32 Group_Modules_Num; //模块组个数--这里因为CCU和模块是绑定在一起的也是CCU个数
	UINT32 Contactor_Line_Num; //接触器对数及并机线路 
	UINT16 Error_system[MODULE_INPUT_GROUP];
	UINT16 Operated_Relay_NO_falg[SYS_CONTACTOR_NUMBER];
  	UINT16 Operated_Relay_NO_Fault_Count[SYS_CONTACTOR_NUMBER];//继电器故障次数，累计了5次就不加这条记录。

	UINT32 Module_ReadyOk[MODULE_INPUT_GROUP];
	UINT32 Ac_Input_Contactor[MODULE_INPUT_GROUP];
	UINT32 rContactor_Actual_State[MODULE_INPUT_GROUP];//接触器1~32 实际状态 bit0:接触器1    0：断开，1：闭合bit1:接触器2以此类推  //--接触器实际状态
	UINT32 rContactor_Control_State[MODULE_INPUT_GROUP];//接触器1~32 控制状态 bit0:接触器1&2   0：断开，1：闭合 bit1:接触器3&4  //--接触器控制状态
	UINT32 rContactor_Miss_fault[MODULE_INPUT_GROUP];//接触器1~32 拒动故障 bit0:接触器1    0：消失，1：发生bit1:接触器2以此类推 //--接触器拒动故障
	UINT32 rContactor_Adhesion_fault[MODULE_INPUT_GROUP];//bit0:接触器1    0：消失，1：发生bit1:接触器2以此类推 //--接触器粘连故障
	UINT32 rContactor_Misoperation_fault[MODULE_INPUT_GROUP];//bit0:接触器1    0：消失，1：发生bit1:接触器2以此类推 //--接触器粘连故障   
  UINT32 Operated_Relay_NO;
	SYS_TCUCONTROLCCUSET_DATA Sys_Tcu2CcuControl_Data[MODULE_INPUT_GROUP];
	SYS_GROUP_CONTROL_DATA   Sys_MODGroup_Control_Data[MODULE_INPUT_GROUP];
	SYS_CCU_DATA             Sys_RevCcu_Data[MODULE_INPUT_GROUP];
	CHARGER_CONTACTOR_PARAM  Charger_Contactor_param_data[SYS_CONTACTOR_NUMBER];//6组最大接入量为15条线，
  CHARGER_GUN_BUS_PARAM    Charger_gunbus_param_data[MODULE_INPUT_GROUP];
	
	CHARGER_SHILELD_ALARMFALG Charger_Shileld_Alarm;

	// 为兼容二代协议增加的数据
	int sgp_billSendingFlag;					// 充电账单发送中标志
	ISO8583_charge_db_data  sgp_bill_data_db;   // 读取出来的数据库中的交易记录
	int billSendingCount;						// 充电账单发送次数
	int sgp_g_Busy_Need_Up_id;					// 最后1次上报的记录的id

	int sgp_FaultSendingFlag;					// 故障信息发送标志
	int sgp_IsLogin;							// TCU登陆状态

	int sgp_card_check_send_flag;				// 刷卡鉴权发送标志
	int sgp_card_check_send_num;				// 刷卡鉴权的枪号 从1开始编码

	int sgp_card_ok_wait_charge;				// 刷卡鉴权成功等待启动充电状态
	int sgp_User_Card_Charge_flag;				// 刷卡启动标志

	int sgp_vin_charge_send_flag;				// Vin码启动发送标志
	int sgp_vin_charge_send_num;				// Vin码启动的枪号 从1开始编码
}GLOBAVAL;


extern GLOBAVAL *Globa;

extern pthread_mutex_t busy_db_pmutex;
extern pthread_mutex_t mutex_log;
extern pthread_mutex_t modbus_pc_mutex;
extern UINT32 * process_dog_flag;

#ifdef __cplusplus
}
#endif

#endif  //_GLOBALVAR.H_




