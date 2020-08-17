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

** modified log:VS.2.01 基本版本
** modified log:VS.2.02 修改交流输入欠压告警为保护，修改控制板结束充电标志的未定义值的处理
** modified log:VS.2.03 修改QT界面中多余的参数设置，如AC配置，绝缘监测使能，增加系统测试控制参数
** modified log:VS.2.04 修改两个控制板共用的告警信号如急停之类
** modified log:VS.2.05 修改两个控制板有时会通信中断问题（多帧数据传输用了局部变量引起）
** modified log:VS.2.06 注释掉IC.C中的打印信息，修改cfminit.c中信号捕捉的处理
** modified log:VS.2.07 去除了关于界面中第二块控制版本的版本号显示
** modified log:VS.2.08 
											2016.	05.27 添加分时计费功能，从服务费中获取。	
											a.修改业务数据上传包，添加分时时段数据。
											b.修改数据库，添加数据库中各个分时段数据
											c.添加服务费具体分类。
											d.修改操作数据库函数中的缓存区大小由原来的512更改为800，或者会引起错误。
											e.修改网络PE信号
											f.添加APP充电启动及关闭功能
											g.添加模块类型选择，即直接选择模块类型不需要再填写模块的额定电压电流， 2016-07-11
											h.添加二维码是否显示功能，防止非标的时候扫出来的还是我们的主题内容。 2016-07-11
											I.修改如果与后台无通讯时修改电价则可以使其分时电价都和这个电价一样。 2016-07-12
                      J.添加桩运行状态                       2016-07-16
											k.添加后台数据告警信息                      2016-07-16
										  q.添加下发电流限制功能（之前没有添加其作用）2016-07-29
											
** modified log:VS.2.10 	
                      A.修补充电枪温度过高显示			2016-08-30					
											B.添加充电中无电流显示时，报充电电流为0，结束充电 2016-08-30	
                      C.添加针对CAN出差不能恢复的情况的更改方法  2016-08-30	
                      D.充电过程中心跳帧由3秒更改为5秒超时  2016-08-30		 	
                      E.添加直流电表2007协议及添加交流三项电表2007协议			2016-08-30					
											E.添加余额查询，			2016-08-30					
** modified log:VS.2.11
                      A.添加二号控制板中关于输出熔丝段故障	
** modified log:VS.2.12
                      A.添加M1卡的支持，修改上传充电业务数据反馈值。
                      B.支持24V与12V之间的切换功能。
											C.添加故障分为A枪输出熔丝段B枪输出熔丝段，交流断路器断开 交流断路器跳闸
											D.支持上传告警信息
											E.支持本地解锁及后台联机解锁功能
* modified log:VS.2.13							
										 B.添加支付卡本地解锁和远程解锁功能。
										 C.添加控制板CAN程序升级。
										 D。添加可以通过输入密码进入参数设置界面										
                     E.添加远程下载升级功能程序功能。
   									 F.增加充电枪类型及充电枪最高允许温度（需要下发到下面去）
										 G.增加BMS信息数据上传功能
										 H.增加一些告警信息
										 i.升级后台协议，修改心跳包数据，增加一些数据量心跳数据帧:增加充电方式，充电卡号或账号，控制类型，
										   开启限制数据域，预约开始时间，预约结束时间，可空，在不充电时不传这几个域.
										 j.添加刷卡余额小于0.5元时提示用户充值，充电过程中余额快小于0.2元时结束充电并提示相应的原因。
										 k.修改数据库中增加结束原因代码及启动的充电枪号
										 O.添加告警信息上传后台协议
										 添加24/12 通过软件界面进行选择。
										 1.添加后台最新协议： 无添加地锁（4.2.11） 无：充电卡确认请求接口（后期加上）
										 1.心跳返回值增加桩禁用功能。 工作密钥请求接口--更改添加了地锁个数，及实时数据更改添加地锁状态
* modified log:VS.2.14
                     1.添加新语音		
* modified log:VS.2.15
                    1.修改到最新后台协议
* modified log:VS.2.16
                    1.添加单枪双枪轮流程序整合在一起。20171113			
* modified log:VS.2.17 
                    修改数据库访问以及后台数据访问添加
										
* modified log:VS.2.18 	
               1.各个枪的数据对应一个数据库函数
               2.修改二维码显示数据长度	【60】--》【80】		
											
* modified log:VS.2.27
               1.添加VIN
               2.离线保持远程禁用该功能		
* modified log:VS.2.28
               1.纠正双枪对一辆车时，手动结束时账单问题
* modified log:VS.2.29
               1.纠正双枪对一辆车是，2号账单问题
* modified log:VS.2.30
               1.添加VIN结束按钮，及VIN语音提示错误问题	
* modified log:VS.2.31
               1.清除2号电量问题		
* modified log:VS.2.33			
               添加BCL数据异常结束原因	
* modified log:VS.2.34			
               添加中兴模块,支持双枪单独手动启动	
* modified log:VS.2.35 -200326
               1.添加SOC限制，达到SOC时候允许再次充电1min 外加电流限制
               2.	添加国网模块类型 
               3.添加电流电压不一样配置键。	
               4.添加根据初始SOC判断最大还可以充的电量数5%的额定电量			

* modified log:VS.2.36 -200512 增加交易记录上传队列（会循环提前数据库中没有上传的记录-队列中都发送出去了才会去提前），防止一条记录被卡住之前的就上传不了。
                               增加远程启动订单判断，防止由于网络问题导致企业号启动多个流水号不一样的情况	
                        200514 去除第三方启动限制，离线支持继续充电		
* modified log:VS.2.37 -20200720	：修改第二把枪由于上次金额不足引起下个用户桩主动结束的时候，结束原因异常。	
* modified log:VS.2.37 -20200728	：1.修改APP启动命令反馈给服务器的用户ID要和下发的一样。2添加心跳中的电量，时间数据给CCU以及结束原因。	
										
*******************************************************************************/
#ifndef  _GLOBALVAR_H_
#define  _GLOBALVAR_H_

#include <sys/types.h>
#include <sys/prctl.h>

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

#define SOFTWARE_VER "V2.37"  //不能超过20个字符
#define PROTOCOL_VER "02.2.7" //不能超过20个字符，格式不能更改  

#define MAC_INTERFACE0  "eth0"
#define MAC_INTERFACE1  "eth1"

#define METER_COM_ID      "/dev/ttySP3"
#define LED_CONTROL_COM_ID "/dev/ttySP2"
#define CARLOCK_COM_ID     "/dev/ttySP0"
#define AC_METER_COM_ID    "/dev/ttySP1"

#define DTU_COM_ID         "/dev/ttyAM0"
#define IC_COMID          "/dev/ttySP4"

//#define IC_COMID         "/dev/ttyAM0"
//#define DTU_COM_ID          "/dev/ttySP4"

#define GUN_CAN_COM_ID1   "can1"
#define GUN_CAN_COM_ID2   "can0"
#define DEBUGLOG     1  //把日志打印到文件中

#define ISO8583_DEBUG 0       //ISO8583_DEBUG 调试使能
#define TRANSACTION_FLAG 0       //开启任务事件--针对数据库而言
#define CHECK_BUTTON       //点击结束按钮

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
x
PCB标号                      232_1      232_2       485_2  485_3      CAN1  CAN2     485_1   485_4
驱动标号                    ttySP4     ttyAM0      ttySP2  ttySP3     can1  can0     ttySP1  ttySP0
驱动IO号                                            3_3(7) 3_2(6)                   0_21(8) 0_17(9)
                             读卡器     pc后台     接口板  电能表                   充电模块 并机接口
                                                  电压/电流
                                                   灯板控制
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
#define GROUP1_RUN_LED_ON           Led_Relay_Control(2, 0)
#define GROUP1_RUN_LED_OFF          Led_Relay_Control(2, 1)
#define GROUP2_RUN_LED_ON           Led_Relay_Control(4, 0)
#define GROUP2_RUN_LED_OFF          Led_Relay_Control(4, 1)
#define GROUP1_FAULT_LED_ON         Led_Relay_Control(3, 0)
#define GROUP1_FAULT_LED_OFF        Led_Relay_Control(3, 1)


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

#define LIGHT_CONTROL_Power_ON       Led_Relay_Control(17, 0)
#define LIGHT_CONTROL_Power_OFF      Led_Relay_Control(17, 1)  //灯箱控制开关---断开则是闭合继电器，因为灯箱接入为常开触点，


#define MODBUS_PC04_MAX_NUM         32
#define MODBUS_PC02_MAX_NUM         255

#define  CHARGEMODNUMBER  32              //充电模块最大数量
#define  LOW_VOLTAGE_LIMINT           199*1000
#define  SHOT_VOLTAGE_LIMINT          20*1000    //短路电压上限阀值 0.001V 低于该电压认为短路

#define MAX_NUM_BUSY_UP_TIME  10  //充电交易上传最大次数
#define MAX_MODEL_CURRENT_LIMINT  85  //模块节能模式时电流控制点默认85%

#define MAX_CHARGER_BUSY_NUM_LIMINT  3000  //充电交易数据最大纪录数目
#define COMPENSATION_VALUE 50  //计费时4舍五入
#define METER_CURR_THRESHOLD_VALUE  5 //电表采集的值与控制板采集上来的值误差在5%范围内
#define METER_CURR_COUNT   30   //30S-电表计量数据异常，累计判断时间韦30s
#define SOCLIMIT_CURR_COUNT  30   //达到SOC时候允许再次充电时间（并且电流还需要满足限制）
#define METER_ELECTRICAL_ERRO_COUNT 60  //读取的电量与上次对比，永远小于上次则异常
#define LIMIT_CURRENT_VALUE 20000  //20A
#define MIN_LIMIT_CURRENT_VALUE  500 //5A的时候才去判断电流问题，
#define MAX_LIMIT_OVER_RATEKWH  10     //最大允许多冲电度数10%*额定的---车企的SOC误差为5%
#define OVER_RATEKWH_COUNT 60  //最大允许多冲电度数10%*额定的,连续判断时间

/*Section: Macro Definitions----------------------------------------------------*/

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
#define  CARD_AUTH_TYPE  1 //刷卡鉴权报文
#define  VIN_AUTH_TYPE   2 //VIN鉴权报文

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
#define F_BAK_SETNET "/mnt/Nand1/DisParam/setnet.bak"                   /* 本机网络设置 */

/* 数据文件 */
#define F_DATAPATH          "/mnt/Nand1/ea_app/data"
#define F_DATA_LOGPATH      "/home/root/log"

#define F_CHARGER_BUSY_COMPLETE_DB     "/mnt/Nand1/ea_app/data/busy_coplete.db"  /* 正式充电完成数据库*/
#define F_CHARGER_1_BUSY_COMPLETE_DB   "/mnt/Nand1/ea_app/data/busy_coplete_1.db"  /* 1号枪充电完成数据库*/
#define F_CHARGER_2_BUSY_COMPLETE_DB   "/mnt/Nand1/ea_app/data/busy_coplete_2.db"  /* 2号枪充电完成数据库*/
#define F_CHARGER_CARD_DB              "/mnt/Nand1/ea_app/data/card.db"  /* 黑名单数据库*/

#define F_CarLock_Busy_DB              "/mnt/Nand1/ea_app/data/CarLock_Busy.db"  /* 地锁停车费数据库*/

#define F_UPDATE                        "/mnt/Nand1/update/"
#define F_UPDATE_EAST_DC_CTL            "/mnt/Nand1/update/EASE_DC_CTL.bin"
#define F_UPDATE_Electric_Charge_Pile   "/mnt/Nand1/update/Electric_Charge_Pile"
#define F_UPDATE_Charger                "/mnt/Nand1/update/charger"
#define F_UPDATE_Charger_Package        "/mnt/Nand1/update/DC_Charger.tar.gz"  //下载升级包 直流充电桩升级包
#define F_UPDATE_Charger_Conf           "/mnt/Nand1/update/charger.conf"       //参数配置文件，里面注释需要删除的配置文件及其他
#define F_UPDATE_CHECK_INFO             "/mnt/Nand1/update/update.info"       //升级文件校验
/* 参数备份 */
#define F_BACKUPPATH                    "/mnt/Nand1/ea_app/backup/"
#define F_BAK_UPSPARAM                  "/mnt/Nand1/ea_app/backup/param/ups.bak"      /* 充电机参数配置备份文件 */
#define F_BAK_UPSPARAM2                 "/mnt/Nand1/ea_app/backup/param/ups2.bak"     /* 充电机参数2配置备份文件 */

#define F_BAK_SETNET_104                "/mnt/Nand1/ea_app/backup/param/netset_104.bak"   /* 网络设置 */

#define F_PARAMFixedpirce               "/mnt/Nand1/ea_app/param/Fixedpirce"    /* 定时电价 */
#define F_BAK_PARAMFixedpirce           "/mnt/Nand1/ea_app/backup/param/Fixedpirce.bak"    /* 定时电价 */

#define F_Charger_Shileld_Alarm         "/mnt/Nand1/ea_app/param/Charger_Shileld_Alarm" //参数配置文件，里面注释需要删除的配置文件及其他
#define F_BAK_Charger_Shileld_Alarm     "/mnt/Nand1/ea_app/backup/param/Charger_Shileld_Alarm.bak" //参数配置文件，里面注释需要删除的配置文件及其他

#define F_CarLOCK_Param                 "/mnt/Nand1/ea_app/param/CarLOCK_Param" //参数配置文件，里面注释需要删除的配置文件及其他
#define F_BAK_CarLOCK_Param             "/mnt/Nand1/ea_app/backup/param/CarLOCK_Param.bak" //参数配置文件，里面注释需要删除的配置文件及其他

#define F_BackgroupIssued_Param                 "/mnt/Nand1/ea_app/param/BackgroupIssued_Param" //参数配置文件，里面注释需要删除的配置文件及其他
#define F_BAK_BackgroupIssued_Param             "/mnt/Nand1/ea_app/backup/param/backgroupIssued_Param.bak" //参数配置文件，里面注释需要删除的配置文件及其他


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
/*4.系统参数和配置参数
掉电保存配置参数，采取文件方式保存，在每次上电时候读取一次进行初始化，将文件值赋值到相应的结构体中
*/
//桩通过的方式启动
typedef enum  _startmode
{
   CARD_START_TYPE = 1,//CAR
	 APP_START_TYPE = 2,//app 
   VIN_START_TYPE  = 3 //VIN
}startmode;

typedef struct _card_lock_bcdtime
{
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
	UINT32 Start_Unlock_flag;   //开始解卡标志
	UINT32 Deduct_money;       //需扣款金额
	UINT32 Local_Data_Baseid_ID; //本地数据库上的ID
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
//服务器下发参数设置
typedef struct _BackgroupIssued
{                                        
	UINT8   Electric_pile_workstart_Enable; //启动开始时
	UINT8   keepdata[3]; //启动开始分
	UINT32  keepdata1[3];
}BackgroupIssued;
extern BackgroupIssued gBackgroupIssued;

typedef struct _Light_CONTROL
{                                        
	UINT8   Start_time_on_hour; //启动开始时
	UINT8   Start_time_on_min; //启动开始分
	UINT8   End_time_off_hour; //结束时
	UINT8   End_time_off_min; //结束分
}Light_CONTROL;

typedef struct _CHARGER_PARAM{
	UINT32 Charge_rate_voltage;		  //充电机额定电压  0.001V
	UINT32 Charge_rate_current1;		//充电机1额定电流  0.001A
	UINT32 Charge_rate_current2;		//充电机2额定电流  0.001A
	UINT32 Charge_rate_number1;		  //模块1组数量
	UINT32 Charge_rate_number2;		  //模块2组数量 
	UINT32 model_rate_current;		  //单模块额定电流  0.001A
	UINT32 set_voltage;				      //设置电压  0.001V   主要是为了保存手动设置电压电流
	UINT32 set_current;				      //设置电流  0.001A
	UINT32 gun_config;				      //充电枪数量 1--单枪-2-双枪轮流充，3-双枪同时充
	UINT32 Input_Relay_Ctl_Type;		//交流采集盒继电器控制
	UINT32 Charging_gun_type;       //充电枪类型
	UINT32 gun_allowed_temp;        //充电枪温度最大允许充电温度
	
	UINT32 couple_flag;		          //充电机单、并机标示 1：单机 2：并机
	UINT32 charge_addr;		          //充电机地址偏移，组要是在并机模式时用，1、N
	UINT32 limit_power;		          //输出功率限制点(离线时用的默认功率限制值)  1kw
	UINT32 current_limit;           //最大输出电流
	UINT32 meter_config_flag;		    //电能表配置标志  1：有，0：无
	UINT32 Price;                   //单价,精确到分
	UINT32 Model_Type;               //模块类型：0-华为模块 1-英可瑞模块 2-英飞源模块 3-其他类型模块
  UINT32 System_Type;		           //系统类型---0--双枪同时充 ---1双枪轮流充电 2--单枪立式 3-壁挂式
	UINT32 Serv_Type;               //服务费收费类型 1：按次收费；2：按时间收费(单位：分/10分钟；3：按电量
	UINT32 Serv_Price;              //服务费,精确到分 默认安电量收
	UINT8  NEW_VOICE_Flag;          //新语音
  UINT8  Control_board_Baud_rate;  //控制板波特率
  UINT8  AC_Meter_config;         //交流电表配置
  UINT8  AC_Meter_CT;             //交流电表配置CT变比
	Light_CONTROL  Light_Control_Data;
	UINT32 input_over_limit;		    //输入过压点  0.001V
	UINT32 input_lown_limit;		    //输入欠压点  0.001V
	UINT32 output_over_limit;		    //输出过压点  0.001V
	UINT32 output_lown_limit;		    //输出欠压点  0.001V
	
	UINT8 BMS_work;		            //系统工作模式  1：正常，2：测试
	UINT8 keep_reserve[3];		     //保留3个8位 --从APP里面拆弄下来的

	UINT8 BMS_CAN_ver;             //BMS的协议版本 1：国家标准 2：普天标准-深圳版本
	UINT8 keep_reserve1[3];		     //保留3个8位 --从APP里面拆弄下来的

	UINT32 DC_Shunt_Range;          //直流分流器量程设置;	
	
	UINT8  charge_modluetype;          // 由32 位的拆成4个8位的
	UINT8  charge_modlue_index;         //模块具体型号索引--QT界面设置
	UINT8  heartbeat_idle_period;     //空闲心跳周期 秒
	UINT8  heartbeat_busy_period;     //充电心跳周期 秒
	
	UINT8  APP_enable;		          //APP二维码显示 0：否，1：显示  --先定义位低字节
	UINT8  keep_reserve3[3];		     //保留3个8位 --从APP里面拆弄下来的
	
	UINT8  Serial_Network;		      //与后台通讯方式 1---串口模式 0--网络模式由之前的
	UINT8  SOC_limit;	//SOC达到多少时，判断电流低于min_current后停止充电,值95表示95%
	UINT8  min_current;//单位A  值30表示低于30A后持续1分钟停止充电
	UINT8  CurrentVoltageJudgmentFalg;//0-不判断 1-进行判断。收到的电压电流和CCU发送过来的进行判断

	UINT8  SN[12];		              //充电桩编号 12 位数 数字字符串表示
	UINT8  Power_meter_addr1[12];		//电能表地址 12字节
	UINT8  Power_meter_addr2[12];		//电能表地址 12字节
	UINT8  DTU_Baudrate;		        //DTU 波特率选择
	UINT8  LED_Type_Config;		      //灯板控制方式0--监控屏 1--由下面控制 2-无灯板控制
	UINT16 reserve4;		            //保留2个32位
	UINT8 Key[6];
}CHARGER_PARAM;

typedef struct _CHARGER_PARAM2{
	UINT8 invalid_card_ver[5];		  //黑名单版本号
	UINT8 price_type_ver[5];		    //资费策略版本号
	UINT8 APP_ver[5];		            //APP下载地址版本号
	UINT8 Software_Version[5];     //
	UINT8 http[55];		             //APP下载网址
  UINT8 reserve1[25];		        //保留30字节 //100
	
  UINT32 share_time_kwh_price[4];		//分时电价  //尖电价单位分 //峰电价单位分 //平电价单位分 //谷电价单位分
  UINT32 time_zone_num;  //时段数
	UINT32 time_zone_tabel[12];//时段表
  UINT32 time_zone_feilv[12];//时段表位于的价格
	
	UINT32 share_time_kwh_price_serve[4]; //分时服务电价
	UINT32 show_price_type;//电价显示方式 0--详细显示 1--总电价显示
	UINT32 IS_fixed_price;
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


typedef struct _FIXED_PRICE{
	UINT32 falg;            //存在定时电价，需要更新
	UINT32 Serv_Type;               //服务费收费类型 1：按次收费；2：按时间收费(单位：分/10分钟；3：按电量
	UINT32 Serv_Price;              //服务费,精确到分 默认安电量收
  UINT32 Price;                   //单价,精确到分
	UINT32 share_time_kwh_price[4];		//分时电价  //尖电价单位分 //峰电价单位分 //平电价单位分 //谷电价单位分
  UINT32 time_zone_num;  //时段数
	UINT32 time_zone_tabel[12];//时段表
  UINT32 time_zone_feilv[12];//时段表位于的价格
	UINT32 share_time_kwh_price_serve[4]; //分时服务电价
	UINT32 show_price_type;//电价显示方式 0--详细显示 1--总电价显示
	UINT32 IS_fixed_price;
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
}Special_price_param;  //充电私有电价

typedef struct _APP_CHARGER_PARAM{
	UINT32 APP_start;         //0, 结束充电 1, 启动充电

	UINT32 APP_BMS_type;      //0，BMS控制充电 1，盲充

	UINT32 APP_charger_type;   //1，电量控制充电 2，时间控制充电 3，金额控制充电 4，充满为止
	UINT32 APP_charge_money;   //充电金额   单位为“元”，精确到0.01
	UINT32 APP_charge_kwh;     //充电电量   单位为“度”，精确到0.01
	UINT32 APP_charge_time;    //充电时间   单位为“秒“
	UINT32 APP_pre_time;       //预约时间   单位为”分”

	UINT8  ID[16];		         //用户标识 12 位数 字符串表示
	UINT32 last_rmb;           //账号余额	10	N	N8	单位为“分“
	UINT8  busy_SN[20];        //交易流水号	11	N	N20

}APP_CHARGER_PARAM;

typedef struct _ISO8583_SERVER_NET_SETTING{
	UINT8 ISO8583_Server_IP[16];  //字符串形式保存  例如 192.168.162.133  为15个字符
	UINT8 ISO8583_Server_port[2]; //低在前高在后 端口80： 0x50 0x00
	UINT8 addr[2];
	
	UINT8 Server_IP[16];  //字符串形式保存  例如 192.168.162.133  为15个字符
	UINT8 port[2]; //低在前高在后 端口80： 0x50 0x00
}ISO8583_SERVER_NET_SETTING;


typedef struct _CHARGER_MSG{
    long  type;
    UINT8 argv[MAX_MODBUS_FRAMESIZE];
}CHARGER_MSG;

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

typedef struct _CHARGER_ERROR{
	UINT8 emergency_switch;        //1  急停按下故障 (控制板开关输入 2路) 
	UINT8 PE_Fault;                //2  系统接地故障
	UINT8 Meter_Fault;             //3  测量芯片通信故障
	UINT8 AC_connect_lost;         //4  交流模块通信中断
	UINT8 charg_connect_lost[33];  //5  充电模块通信中断 1# ~32#  数据填充是从[1]~[32]，故定义33个数组

	UINT8 charg_fault[33];         //6  充电模块故障     1# ~32#  数据填充是从[1]~[32]，故定义33个数组
	UINT8 charg_fun_fault[33];     //7  充电模块风扇故障 1# ~32#  数据填充是从[1]~[32]，故定义33个数组
	UINT8 charg_AC_alarm[33];      //8  充电模块交流输入告警 1# ~32#  数据填充是从[1]~[32]，故定义33个数组
	UINT8 charg_out_shout[33];     //9  充电模块输出短路故障 1# ~32#  数据填充是从[1]~[32]，故定义33个数组
	UINT8 charg_out_OC[33];        //10 充电模块输出过流告警 1# ~32#  数据填充是从[1]~[32]，故定义33个数组

	UINT8 charg_out_OV[33];        //11 充电模块输出过压告警 1# ~32#  数据填充是从[1]~[32]，故定义33个数组
	UINT8 charg_out_LV[33];        //12 充电模块输出欠压告警 1# ~32#  数据填充是从[1]~[32]，故定义33个数组
	UINT8 charg_in_OV[33];         //13 充电模块输入过压告警 1# ~32#  数据填充是从[1]~[32]，故定义33个数组
	UINT8 charg_in_LV[33];         //14 充电模块输入欠压告警 1# ~32#  数据填充是从[1]~[32]，故定义33个数组
	UINT8 charg_in_lost[33];       //15 充电模块输入缺相告警 1# ~32#  数据填充是从[1]~[32]，故定义33个数组

	UINT8 charg_over_tmp[33];      //16 充电模块过温告警 1# ~32#     数据填充是从[1]~[32]，故定义33个数组
	UINT8 charg_OV_down[33];       //17 充电模块过压关机告警 1# ~32# 数据填充是从[1]~[32]，故定义33个数组
	UINT8 charg_other_alarm[33];   //18 充电模块其他告警 1# ~32#     数据填充是从[1]~[32]，故定义33个数组
	UINT8 output_over_limit;		    //19 系统输出过压告警
	UINT8 output_low_limit;        //20 系统输出欠压告警

	UINT8 sistent_alarm;		        //21 系统绝缘告警
	UINT8 input_over_limit;		    //22 交流输入过压告警
	UINT8 input_over_protect;		  //23 交流输入过压保护
	UINT8 input_lown_limit;		    //24 交流输入欠压告警
	UINT8 input_switch_off;        //25 交流输入开关跳闸

	UINT8 fanglei_off;             //26 交流防雷器跳闸
	UINT8 output_switch_off[2];    //27 直流输出开关跳闸
	UINT8 fire_alarm;              //28 烟雾告警
	UINT8 sys_over_tmp;            //29 充电桩过温故障
	UINT8 gun_over_temp_alarm[2];   //30 充电枪头过温
  UINT8 charg_Output_overvoltage_lockout[33];   //30 充电模块其他告警 1# ~32#     数据填充是从[1]~[32]，故定义33个数组
  UINT8 charg_protection[33];   //31 充电模块保护
	UINT8 Ac_circuit_breaker_trip; //33交流断路器跳闸
  UINT8 AC_circuit_breaker_off;  //34交流断路器断开

	UINT8 voltage_connect_lost;     //51 电压表通信中断
	UINT8 current_connect_lost;     //52 电流表通信中断
	UINT8 ctrl_connect_lost;     //53 充电控制板通信中断
	UINT8 meter_connect_lost;       //54 电表表通信中断
	UINT8 IC_connect_lost;          //55 读卡器通信中断
	UINT8 COUPLE_connect_lost;      //56 并机从机通信中断

  UINT8 Door_No_Close_Fault;      //门禁故障
  UINT8 CarLock_connect_lost;     //地锁通讯异常
	UINT32 system_sisitent;          //61 系统绝缘故障
	UINT32 output_shout;	           //62 输出短路故障
	UINT8 output_switch[2];         //63 输出接触器异常
	UINT8 assist_power_switch[2];   //64 辅助电源接触器异常
	UINT8 COUPLE_switch;            //65 系统并机接触器异常
  UINT8 breaker_trip;
  UINT8 killer_switch;
	UINT8 ac_meter_connect_lost; //交流电表异常
	UINT8 charger_connect_lost;  // 充电模块通信中断 
	UINT8 charger_fun_fault;     // 充电模块风扇故障 
	UINT8 charger_over_tmp;      // 充电模块过温告警 
	UINT8 charger_OV_down;       // 充电模块过压关机告警  
	UINT8 charger_other_alarm;   // 充电模块其他告警 
	UINT8 charger_fault;          //// 充电模块故障 
	UINT8 Power_Allocation_ComFault;
	UINT8 Power_Allocation_SysFault;

	UINT8 pre_emergency_switch;        //先前状态 急停按下故障 (控制板开关输入 2路)
	UINT8 pre_AC_connect_lost;         //先前状态 交流模块通信中断
	UINT8 pre_charger_connect_lost;  //先前状态 充电模块通信中断 
	UINT8 pre_charger_fun_fault;     //先前状态 充电模块风扇故障 
	UINT8 pre_charger_over_tmp;      //先前状态 充电模块过温告警 
	UINT8 pre_charger_OV_down;       //先前状态 充电模块过压关机告警  
	UINT8 pre_charger_other_alarm;   //先前状态 充电模块其他告警  
	UINT8 pre_output_over_limit;		    //先前状态 系统输出过压告警
	UINT8 pre_output_low_limit;        //先前状态 系统输出欠压告警
	UINT8 pre_sistent_alarm;		        //先前状态 系统绝缘告警
	UINT8 pre_input_over_limit;		    //先前状态 交流输入过压告警
	UINT8 pre_input_over_protect;		  //先前状态 交流输入过压保护
	UINT8 pre_input_lown_limit;		    //先前状态 交流输入欠压告警
	UINT8 pre_input_switch_off;        //先前状态 交流输入开关跳闸

	UINT8 pre_fanglei_off;             //先前状态 交流防雷器跳闸
	UINT8 pre_output_switch_off[2];    //先前状态 直流输出开关跳闸
	UINT8 pre_sys_over_tmp;            //先前状态 充电桩过温故障
	UINT8 pre_ctrl_connect_lost;     //先前状态 充电控制板通信中断
	UINT8 pre_meter_connect_lost;       //先前状态 电表表通信中断
	UINT8 pre_system_sisitent;          //先前状态 系统绝缘故障
	UINT8 pre_output_shout;	           //先前状态 输出短路故障
	UINT8 pre_output_switch[2];         //先前状态 输出接触器异常
	UINT8 pre_assist_power_switch[2];   //先前状态 辅助电源接触器异常
	UINT8 pre_COUPLE_switch;            //先前状态 系统并机接触器异常
	UINT8 pre_charger_fault;          //// 先前状态  充电模块故障
	UINT8 reserve30;  
}CHARGER_ERROR;

//系统全局交互使用的唯一结构体
typedef struct _GLOBAVAL{
	UINT32 reserve1[20];
	
	UINT32 user_sys_time; //系统本次上电运行累计时间 单位 秒

	UINT32 time_update_flag; //系统时间需要跟新标志 1：是 0：否
	UINT32 year;
	UINT32 month;
	UINT32 date;
	UINT32 hour;
	UINT32 minute;
	UINT32 second;

	ISO8583_SERVER_NET_SETTING     ISO8583_NET_setting; 

	UINT32 reserve2[20];
	CHARGER_PARAM		Charger_param;
	UINT32 reserve3[20];
	CHARGER_PARAM2 Charger_param2;
	
	UINT32 reserve31[20];
  FIXED_PRICE   charger_fixed_price; //充电定时电价 --需要存储
  Special_price_param	Special_price_data; //私人专用电价--卡里需要存储这个标志
	Special_price_param	Special_price_APP_data; //私人专用电价--卡里需要存储这个标志
  UINT32 Special_price_APP_data_falg;
	UINT8 tmp_card_ver[5];		  //黑名单版本号
	UINT8 tmp_price_ver[5];		  //价格版本号
	UINT8 tmp_APP_ver[5];		    //APP下载地址版本号
	UINT32 reserve32[20];

	UINT8 kernel_ver[10];		  //内核版本号
	UINT8 contro_ver[20];		  //控制版本--107
	UINT8 protocol_ver[15];       //后台协议版本

	UINT32 reserve4[20];

	key_t           arm_to_qt_msg_id;
	key_t           qt_to_arm_msg_id;

	CHARGER_ERROR   Error;

	APP_CHARGER_PARAM App;

	UINT32 reserve5[20];
	UINT32 APP_Card_charge_time_limit;//按时间充电时间
	UINT32 Card_charger_type; //1，电量控制充电 2，时间控制充电 3，金额控制充电 4，充满为止

	INT32          Error_charger;  //充电故障计数 不等于0 代表有故障
	INT32          Error_system;   //系统故障计数
	INT32          pre_Error_system;   //先前状态 系统故障计数
	INT32          Error_ctl;      //充电控制单元故障计数

	INT32          Warn_system;    //系统告警计数

	UINT32         ctl_state;      //充电控制单元状态 0x00-待机 0x01-自检  0x02-充电  0x03-充满 0x04-告警 0x11 故障。

	UINT32         charger_over_flag; //系统充电结束原因
	UINT32         ctl_over_flag;     //控制单元结束充电原因

	UINT32 gun_link;					      //充电枪连接         1：连接 0：断开
	UINT32 pre_gun_link;					  //先前状态 充电枪连接         1：连接 0：断开

	UINT8  ESAM_SN[12];		          //ESAM卡号 12 位数 字符串表示
	UINT32 charger_state;           //终端状态 取值只有 0x01-故障  0x02-告警  03-空闲 0x04-充电中 0x05-完成 0x06-预约
	UINT32 gun_state;              //枪状态 取值只有  0x03-空闲 0x04-充电中 0x05-完成 0x07-等待

	UINT32 led_run_flag;            //充电指示灯闪烁标示
	UINT32 Manu_start;					    //人工启动充电方式    1：自动 2：手动
	UINT32 SYS_Step;
	UINT32 MC_Step;
	UINT32 QT_Step;                 //QT界面状态
	UINT32 Batt_Step;

	UINT32 checkout;
	UINT8  tmp_card_sn[16];
	UINT8  card_sn[16];             //用户卡号
	UINT32 last_rmb;                //充电前卡余额（以分为单位）
	UINT32 pay_step;

	UINT32 IC_check_ok;             //刷卡标志    1：有刷卡 0：无
	UINT32 QT_charge_money;         //充电金额   1-9999 元 
	UINT32 QT_charge_kwh;           //充电电量   1-9999 kwh 
	UINT32 QT_charge_time;          //第一把枪充电时间   1-9999*60 sec
	UINT32 QT_pre_time;             //预约时间   0-23*60+59 min 
	UINT32 QT_gun_select;           //充电枪选择 1：充电枪1 2: 充电枪2

	UINT32 charger_start;					  //系统开始充电 1：开始 0：停止
	UINT32 card_type;					      //卡类型 1：用户卡 2：员工卡



	//UINT32 meter_voltage;					  //计量输出电压 0.001V
	//UINT32 meter_current;					  //计量输出电流 0.001A

	UINT32 limit_kw;					      //充电机输出限制功率  kw
	UINT32 have_hart;					      //与后台心跳标志存在 1:正常 0：否
	UINT32 have_card_change;				//黑名单版本变化 1:变化 0：否
	UINT32 have_price_change;				//电价版本变化 1:变化 0：否
	UINT32 have_APP_change;				  //APP地址变化 1:变化 0：否
	UINT32 have_APP_change1;				//给qt用，以便告知  1:变化 0：否

	UINT32 meter_start_KWH;         //电表起始示数 0.01 度 用于电表计量
	UINT32 meter_stop_KWH;          //电表结束示数 0.01 度 用于电表计量
	UINT32 meter_KWH;               //电表当前示数 0.01 度

	UINT32 kwh;                     //已充电电量 0.01 度
	UINT32 Time;				            //已充电时间   1s
	UINT32 soft_KWH;                //软件累计充电电量 0.0001 度 用于软件计算
	
	UINT32 total_rmb;               //总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
	UINT32 rmb;                     //电量消费金额 0.01yuan

	UINT32 ISO_8583_rmb_Serv;               //总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
	UINT32 ISO_8583_rmb;                     //电量消费金额 0.01yuan
	
	UINT32 batt_sistent_p1;          //电池组1正对地绝缘 0.1ko
	UINT32 batt_sistent_n1;          //电池组1负对地绝缘
	
	UINT32 need_voltage;				    //电压需求  0.001V
	UINT32 need_current;				    //电流需求  0.001A
	UINT32 Output_voltage;					//充电机输出电压 0.001V
	UINT32 Output_current;					//充电机输出电流 0.001A
	UINT32 line_v;                  //充电导引电压 精确到小数点后1位0V - 12V
	
	UINT8  VIN[17];                 //车辆识别码VIN
	UINT32 BMS_rate_voltage;				//BMS额定电压  0.001V
	UINT32 BMS_rate_AH;				      //BMS额定容量  AH
	UINT32 Battery_Rate_KWh;         //动力蓄电池标称总能量KWH
	UINT32 BATT_type;               //BMS电池类型
	UINT32 BMS_H_voltage;				    //BMS最高允许充电电压  0.001V
	UINT32 BMS_H_current;				    //BMS最高允许充电电流  0.001A
	UINT32 BMS_batt_SOC;				    //BMS电池容量
	UINT32 BMS_batt_Start_SOC;     //充电开始后第1次读取到的SOC

	UINT32 BMS_cell_HV;				      //单体电池最高电压   0.01V
	UINT32 BMS_cell_LV;				      //单体电池最低电压   0.01V
	UINT32 Charge_Mode;        //8充电模式
	
	UINT32 BMS_cell_HV_NO;     //8最高单体电压
	UINT32 Cell_H_Cha_Vol;     //单体动力蓄电池最高允许充电电压 0.01 0-24v
	UINT8  Cell_H_Cha_Temp;       //单体充电最高保护温度 -50+200

  UINT8 BMS_Vel[3];         //BMS协议版本                                    

	INT16 BMS_cell_HT;				      //单体电池最高温度   1  //
	INT16 BMS_cell_LT;				      //单体电池最低温度   1
	UINT8 BMS_cell_LT_NO;            //5电池组最低温度编号	
	UINT8 BMS_cell_HT_NO;            //6电池组最高温度编号	
	UINT32 BMS_need_time;				     //剩余时间 min
	
	UINT32 BMS_Power_V;
	UINT32 Charge_Gun_Temp;         //充电枪温度
	UINT32 Charger_Over_Rmb_flag;
	Control_Upgrade_Firmware Control_Upgrade_Firmware_Info;
	Card_Lock_Info   User_Card_Lock_Info;     //锁卡信息
	UINT32 Software_Version_change;
	UINT8  tmp_Software_Version[5];//升级包软件版本
	UINT32 Electric_pile_workstart_Enable;     // 电桩禁用标记(00:表示正常，01表示禁用)
	UINT32 User_Card_check_flag;     //卡片校验标志
	UINT32 User_Card_check_result; //卡片校验结果
	UINT32 User_Card_check_card_ID[16];//验证卡片信息
	UINT32 Operation_command;//电子锁 解锁 1--加锁，2-- 解锁，只负责解锁
	UINT32 page_flag;
	UINT32 Apply_Price_type;//申请的电价类型，0--当前电价 1--定时电价
	unsigned char fixed_price_effective_time[17];    //定时电价生效时间（格式yyyyMMddHHmmss）17	Y	N17
  UINT32 show_price_type;//电价显示方式//分00：表示显示详细单价，01：表示只显示一个总单价	15	N	N2
	UINT32 Enterprise_Card_flag;//企业卡号成功充电上传标志
	UINT32 Enterprise_Card_check_result;//企业卡号成功
	unsigned char qiyehao_flag;//企业号标记，域43
	unsigned char private_price_flag;//私有电价标记，域44
	unsigned char private_price_acquireOK;//获取私有电价标记成功，域44

	unsigned char APP_Start_Account_type;//app启动账户类型用户类型：00：表示本系统用户，01：表示第三方系统用户
	unsigned char order_chg_flag;//有序充电用户卡标记，域45
	unsigned char order_card_end_time[2];       //当前有序充电卡允许充电的终止时刻 HH:MM	
  unsigned char  Updat_data_value;
	unsigned char  Signals_value_No;  //信号值
	unsigned char  Registration_value; //无线模块注册情况
	unsigned char  ICCID_value[20];
	UINT32 BMS_Info_falg;		//接收到BMS信息数据反馈标志，没有接收到则上送给监控全为0
	UINT32 gun_link_over;					      //充电枪连接         1：连接 0：断开
	UINT32 Special_card_offline_flag;//允许特殊卡进行离线处理标记
	UINT32 Double_Gun_To_Car_falg;
  unsigned char  Bind_BmsVin[17];//绑定车辆识别码
	CHARGER_SHILELD_ALARMFALG Charger_Shileld_Alarm;
	
	unsigned char Car_lock_Card[16];
	unsigned char Car_lockCmd_Type;
	unsigned char Car_lock_comfault;
	unsigned char Car_lock_have_car;
	unsigned char park_start_time[14];//停车开始时间
	unsigned int ac_volt[3];
	unsigned int ac_current[3];
	unsigned int ac_meter_kwh; //交流电表
  unsigned int meter_Current_A; //从电表中读取的值
	unsigned int meter_Current_V; //从电表中读取的值
	unsigned int Start_Mode; //启动方式 01 刷卡 02 app 03vin 
	unsigned int VIN_Support_falg;//需要查看控制板的版本来确定是否支持
	unsigned int VIN_start;
	unsigned int VIN_Judgment_Result;
	
	//UINT16          modbus_485_PC04[MODBUS_PC04_MAX_NUM];
	//UINT32          modbus_485_PC02[MODBUS_PC02_MAX_NUM];
}GLOBAVAL;

extern GLOBAVAL *Globa_1, *Globa_2;

extern pthread_mutex_t busy_db_pmutex;
extern pthread_mutex_t mutex_log;
extern pthread_mutex_t sw_fifo;

extern UINT32 * process_dog_flag;


#ifdef __cplusplus
}
#endif

#endif  //_GLOBALVAR.H_
