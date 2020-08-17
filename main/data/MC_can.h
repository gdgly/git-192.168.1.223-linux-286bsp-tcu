/*******************************************************************************
  Copyright(C)    2014-2015,      EAST Co.Ltd.
  File name :     MC_can.h
  Author :        dengjh
  Version:        1.0
  Date:           2016.01.11
  Description:    按照与控制器通信协议开发
  Other:          
  Function List:  
  History:        
          <author>   <time>   <version>   <desc>
            dengjh   2016.01.11   1.0      Create
*******************************************************************************/
#ifndef MC_CAN_H                                                                 
#define MC_CAN_H                                                                 

#include "globalvar.h"


#define CONTR_CAN_ADDRESS 0xF6
#define METER_CAN_ADDRESS 0x8A

/** PDU 协议数据单元格式 *******************************************************
    -------------------------------------------------------------------
    | P |EDP| DP |     PF    |    PS(DA)   |    SA   |     DATA       |
    -------------------------------------------------------------------
    | 3 | 1 | 1  |     8     |     8       |     8   |     0~64       | 位
    -------------------------------------------------------------------
    |0/1| 0 | 0  |    0/1    |    0xF4     |   0x56  |      0/1       | 本协议实际取值(发送)

   PGN  格式定义
   -----------------------------------------
   | R |EDP| DP |     PF    |    PS        |
   -----------------------------------------
   | 6 | 1 | 1  |     8     |     8        |
   -----------------------------------------
   | 0 | 0 | 0  |    0/1    |     0        |  本协议实际参数组描述取值
   -----------------------------------------
*******************************************************************************/                                                     
//------------------------------------报文描述	PGN	 HEX 优先权 长度 数据类型	报文周期ms	源地址-目的地址
//--------------------------- 1 命令帧  ----------------------------------------   
#define  PGN256_ID_MC  0x1001F68A   //充电启动帧	256	  000100H	4	8	BIN	250	计费单元--控制器
#define  PGN512_ID_CM  0x10028AF6   //启动应答帧	512	  000200H	4	8	BIN	250	控制器--计费单元
#define  PGN768_ID_MC  0x1003F68A   //充电停止帧	768	  000300H	4	8	BIN	250	计费单元--控制器
#define  PGN1024_ID_CM 0x10048AF6   //停止应答帧	1024	000400H	4	8	BIN	250	控制器--计费单元
#define  PGN1280_ID_MC 0x1805F68A   //下发对时帧	1280	000500H	6	8	BIN	500	计费单元--控制器
#define  PGN1536_ID_CM 0x18068AF6   //对时应答帧	1536	000600H	6	8	BIN	500	控制器--计费单元
#define  PGN1792_ID_MC 0x1807F68A   //校验版本	  1792	000700H	6	8	BIN	500	计费单元--控制器
#define  PGN2048_ID_CM 0x18088AF6   //版本确认	  2048	000800H	6	8	BIN	500	控制器--计费单元
#define  PGN2304_ID_MC 0x1809F68A   //下发充电参数信息	2304	000900H	6	8	BIN	250	计费单元--控制器
#define  PGN2560_ID_CM 0x180A8AF6   //充电参数信息确认	2560	000A00H	6	8	BIN	250	控制器--计费单元
#define  PGN2816_ID_CM 0x180B8AF6   //请求充电参数信息	2816	000B00H	6	8	BIN	250	控制器--计费单元
#define  PGN3072_ID_MC 0x180CF68A   //下发直分流器量程  3172	000C00H	6	8	BIN	250	计费单元--控制器
#define  PGN3328_ID_CM 0x180D8AF6   //设置分流器确认    3328	000D00H	6	8	BIN	250	控制器--计费单元

#define  PGN3584_ID_MC 0x180EF68A   //电子锁控制        3584	000E00H	6	8	BIN	250	计费单元--控制器
#define  PGN3840_ID_CM 0x180F8AF6   //电子锁控制反馈    3840	000F00H	6	8	BIN	250	控制器--计费单元

//---------------------------------功率控制------------------------------------------------------
#define  PGN3840_ID_MC 0x100FF68A    //功率调节帧	      3840	001100H	4	8	BIN	250	计费单元--控制器
#define  PGN4096_ID_CM 0x10108AF6   //功率调节帧应答帧	4096	001200H	4	8	BIN	250	控制器--计费单元


//--------------------------- 2 状态帧  ----------------------------------------   
#define  PGN4352_ID_CM 0x10118AF6   //启动完成帧	    4352	001100H	4	不定	BIN	250	控制器--计费单元
#define  PGN4608_ID_MC 0x1012F68A   //启动完成应答帧	4608	001200H	4	8	BIN	250	计费单元--控制器
#define  PGN4864_ID_CM 0x10138AF6   //停止完成帧	    4864	001300H	4	不定	BIN	250	控制器--计费单元
#define  PGN5120_ID_MC 0x1014F68A   //停止完成应答帧	5120	001400H	4	8	BIN	250	计费单元--控制器
//--------------------------- 3 数据帧  ----------------------------------------   
#define  PGN8448_ID_CM 0x18218AF6   //遥信帧	     8448	002100H	6	 8	  BIN	1000 控制器--计费单元
#define  PGN8704_ID_CM 0x18228AF6   //遥测帧	     8704	002200H	6	不定	BIN	1000 控制器--计费单元
#define  PGN8960_ID_MC 0x1823F68A   //遥测响应帧	 8960	002300H	6	 8	  BIN	1000 计费单元--控制器
#define  PGN9216_ID_CM 0x18248AF6   //遥测AC帧   9216	002400H	6	不定	BIN	3000 控制器--计费单元
#define  PGN9472_ID_CM 0x18258AF6   //遥信状态帧	 9472	002500H	6	 8	  BIN	1000 控制器--计费单元
//--------------------------- 4 心跳帧  ----------------------------------------   
#define  PGN12544_ID_MC 0x1831F68A  //心跳帧	    12544	003100H	6	8	BIN	1000	计费单元--控制器
#define  PGN12800_ID_CM 0x18328AF6  //心跳响应帧	12800	003200H	6	8	BIN	1000	控制器--计费单元

//---------------------------  错误报文-----------------------------------------
#define  PGN7680_ID_GB 0x081E56F4   //P = 2
#define  PGN7936_ID_GB 0x081FF456   //P = 2

//-------------------------------------  CAN升级程序----------------------------------------------
#define  PGN20992_ID_MC 0x1852F68A  //升级请求报文	        20480	005200H	6	8	BIN	10	计费单元--控制器
#define  PGN21248_ID_CM 0x18538AF6  //升级请求报文响应报文	21248	005300H	6	8	BIN	10	控制器--计费单元
#define  PGN21504_ID_CM 0x18548AF6  //请求升级数据报文帧	  21504	005400H	6	8	BIN	10	控制器--计费单元
#define  PGN21760_ID_MC 0x1855F68A  //升级数据包报文	      21760	005500H	6	8	BIN	10	计费单元--控制器
#define  PGN22016_ID_CM 0x18568AF6  //升级数据包应答报文	  22016	005600H	6	8	BIN	10	控制器--计费单元

//-------------------------------------  告警调试参数----------------------------------------------
#define  PGN22272_ID_MC 0x1857F68A  //调试参数下发	        20480	005700H	6	8	BIN	10	计费单元--控制器
#define  PGN22528_ID_CM 0x18588AF6  //调试参数反馈	        21248	005800H	6	8	BIN	10	控制器--计费单元

#define  PGN24576_ID_MC 0x1860F68A  //下发交流采集板接触器控制	20480	006000H	6	8	BIN	10	计费单元--控制器
#define  PGN24832_ID_CM 0x18618AF6  //下发交流采集板反馈	      24832	006100H	6	8	BIN	10	控制器--计费单元

#define  PGN25088_ID_CM 0x18628AF6  //BMS信息主动上传记录	      25088	006200H	6	8	BIN	10	控制器--计费单元
#define  PGN25344_ID_MC 0x1863F68A  //BMS信息主动上传反馈记录	  25088	006300H	6	8	BIN	10	计费单元--控制器

//--------------------------- 1 命令帧 -----------------------------------------
//充电启动命令帧
typedef struct _MC_PGN256_FRAME
{                                       
	UINT8 gun;     //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
	UINT8 couple;  //负荷控制开关	BIN	1Byte	根据用户类型提供不同功率输出。1启用，2关闭，其他无效。 
	UINT8 ch_type; //充电启动方式	BIN	1Byte	1自动，2手动，其他无效。
	UINT8 set_voltage[2]; //手动设定充电电压	BIN	2Byte	精确到小数点后1位0V - 950V
	UINT8 set_current[2]; //手动设定充电电流	BIN	2Byte	精确到小数点后1位0A - 400A
	UINT8 Gun_Elec_Way;   //保留字节，补足8字节  ---电子锁解锁方式
}MC_PGN256_FRAME;

//充电启动响应帧
typedef struct _MC_PGN512_FRAME
{                                       
   UINT8   gun;     //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
   UINT8   couple;  //负荷控制开关	BIN	1Byte	根据用户类型提供不同功率输出。1启用，2关闭，其他无效。
   UINT8   ok;      //成功标识	BIN	1Byte	0成功；1失败。
	 UINT8   FF[5];   //保留字节，补足8字节
}MC_PGN512_FRAME;

//停止充电命令帧
typedef struct _MC_PGN768_FRAME
{                                       
   UINT8   gun;     //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
   UINT8   cause;   //停止充电原因	BIN	1Byte	0x01：计费控制单元正常停止 0x02: 计费控制单元故障终止
	 UINT8   FF[6];   //保留字节，补足8字节
}MC_PGN768_FRAME;

//停止充电响应帧
typedef struct _MC_PGN1024_FRAME
{                                       
   UINT8   gun;     //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
   UINT8   ok;      //成功标识	BIN	1Byte	0成功；1失败。
	 UINT8   FF[6];   //保留字节，补足8字节
}MC_PGN1024_FRAME;

//对时命令帧
typedef struct _MC_PGN1280_FRAME
{                                       
	UINT8   start;     //是否立即执行	BIN	1Byte	0-立即执行，1-控制器自行选择时间执行
	UINT8   date[7];   //时间	CP56time2a	7Byte BCD码 秒(1) 分 时 日 月 年(6 7) 可选项 
}MC_PGN1280_FRAME;

//对时命令响应帧
typedef struct _MC_PGN1536_FRAME
{                                       
	UINT8   start;   //是否立即执行	BIN	1Byte	0-立即执行，1-控制器自行选择时间执行
	UINT8   ok;      //确认标识	BIN	1Byte	0-对时确认 1-对时拒绝
	UINT8   FF[6];   //保留字节，补足8字节
}MC_PGN1536_FRAME;

//版本校验命令帧
typedef struct _MC_PGN1792_FRAME
{                                       
	UINT8   m_ver[2];   //计费控制单元当前通信版本号	BCD	2Byte	版本号组成分为：主板本号、次版本号。版本号发送的是通讯协议的版本号。示例：主板本号：12 次版本号：10 版本号为：12.10
	UINT8   FF[6];   //保留字节，补足8字节
}MC_PGN1792_FRAME;

//版本校验响应帧
typedef struct _MC_PGN2048_FRAME
{                                       
	UINT8   c_ver[2];   //控制器当前通信版本号	BCD	2Byte	版本号组成分为：主板本号、次版本号。版本号发送的是通讯协议的版本号。示例：主板本号：12 次版本号：10 版本号为：12.10
	UINT8   FF[6];   //保留字节，补足8字节
}MC_PGN2048_FRAME;

typedef struct _CHARGER_SHILELD_ALARM_MC{
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
  unsigned char Shileld_door_falg:1;        //
	unsigned char Shileld_keep_data:1;
}CHARGER_SHILELD_ALARM_MC;
//充电参数发送
typedef struct _MC_PGN2304_FRAME
{
	UINT8 Charge_rate_voltage[2];	//充电机额定电压  V
	UINT8 Charge_rate_number;		  //模块数量 1-32
	UINT8 model_rate_current;		  //单模块额定电流  A
	UINT8 gun_config;				      //充电枪数量 1--单枪-2-双枪轮流充，3-双枪同时充
	UINT8 couple_flag;		        //充电机单、并机标示 1：单机 2：并机
	UINT8 BMS_work;		            //系统工作模式  1：正常，2：测试

	UINT8 input_over_limit[2];		  //输入过压点  V
	UINT8 input_lown_limit[2];		  //输入欠压点  V
	UINT8 output_over_limit[2];		  //输出过压点  V
	UINT8 output_lown_limit[2];		  //输出欠压点  V

	UINT8 BMS_CAN_ver;             //BMS的协议版本 1：国家标准 2：普天标准-深圳版本
	UINT8 current_limit;           //最大输出电流
  UINT8 model_Type;              //模块类型
	UINT8 Charging_gun_type;       //充电枪类型
  UINT8 gun_allowed_temp;        //充电枪最高允许温度
	UINT8 Input_Relay_Ctl_Type; 
	UINT8 LED_Type_Keep;         //灯板控制高4位为灯板控制方式0-有监控屏1-控制板接入，各接入一个2-双枪控制灯板由1号机器进行控制 3-无灯板控制由原来的圆灯控制
	UINT8 Charge_min_voltage[2]; //充电模块最低电压
	CHARGER_SHILELD_ALARM_MC charger_shileld_alarm_value;
}MC_PGN2304_FRAME;

//充电参数发送响应帧
typedef struct _MC_PGN2560_FRAME
{                                       
	UINT8   gun;     //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
	UINT8   ok;      //成功标识	BIN	1Byte	0成功；1失败。
	UINT8   FF[6];   //保留字节，补足8字节
}MC_PGN2560_FRAME;

//请求充电参数信息
typedef struct _MC_PGN2816_FRAME
{                                       
	UINT8   gun;     //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
	UINT8   Cvel[7];   //107控制版本号
}MC_PGN2816_FRAME;

//直流分流器参数下发
typedef struct _MC_PGN3072_FRAME
{                                       
	UINT8   gun;                //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
	UINT8   DC_Shunt_Range[2];  //分流器量程     
	UINT8   FF[5];   //保留字节，补足8字节
}MC_PGN3072_FRAME;

//直流分流器参数发送响应帧
typedef struct _MC_PGN3328_FRAME
{                                       
	UINT8   gun;     //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
	UINT8   ok;      //成功标识	BIN	1Byte	0成功；1失败。
	UINT8   FF[6];   //保留字节，补足8字节
}MC_PGN3328_FRAME;

//---------------------------------功率控制------------------------------------------------------
typedef struct _Power_PGN3840_FRAME
{                                       
	UINT8   gun;                //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
	UINT8   Power_Adjust_Command;  //功率调节指令类型:绝对值和百分比两种 01H：功率绝对值，输出值=功率调节参数值 02 百分比 输出值= 最大输出功率*百分比     
	UINT8   Power_Adjust_data[2];// 01H：绝对值，数据分辨类：0.1KW/位 偏移量：-1000.kw：数据范围 -1000.0kw ~ 1000.0kw（正表示充电，负是放电）02：百分比%1为。0-100%
	UINT8   FF[4];   //保留字节，补足8字节
}Power_PGN3840_FRAME;

//直流分流器参数发送响应帧
typedef struct _Power_PGN4096_FRAME
{                                       
	UINT8   gun;                //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
	UINT8   Power_Adjust_Command;  //功率调节指令类型:绝对值和百分比两种 01H：功率绝对值，输出值=功率调节参数值 02 百分比 输出值= 最大输出功率*百分比     
	UINT8   Power_Adjust_data[2];// 01H：绝对值，数据分辨类：0.1KW/位 偏移量：-1000.kw：数据范围 -1000.0kw ~ 1000.0kw（正表示充电，负是放电）02：百分比%1为。0-100%
	UINT8   Ok;
  UINT8   failure_cause;//停止充电原因	BIN	2Byte	低字节：停止的具体原因，见附录D1；高字节：停止的系统原因，如下：0xE1：计费控制单元控制停止充电。0xE2：充电控制器判定充电机故障，充电控制器自行主动终止充电。0xE3：充电控制器判定车载BMS异常，充电控制器自行主动终止充电。0xE4：车载BMS正常终止充电。0xE5：车载BMS故障终止充电。
	UINT8   FF[2];   //保留字节，补足8字节
}Power_PGN4096_FRAME; 

//--------------------------- 2 状态帧 -----------------------------------------
//充电启动完成状态帧
typedef struct _MC_PGN4352_FRAME
{
	UINT8 gun;           //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
	UINT8 BMS_11_15;     //BMS协议版本 0： 国标2015 1：国标2011 2：普天标准
	UINT8 BATT_type;     //BMS电池类型
	UINT8 BMS_H_vol[2];  //BMS最高允许充电电压  0.1V
	UINT8 BMS_H_cur[2];  //BMS最高允许充电电流  0.01A
	UINT8 FF;            //保留字节，补足8字节
}MC_PGN4352_FRAME;

//充电启动完成应答报文
typedef struct _MC_PGN4608_FRAME
{                                       
   UINT8   gun;     //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
   UINT8   couple;  //负荷控制开关	BIN	1Byte	根据用户类型提供不同功率输出。1启用，2关闭，其他无效。
   UINT8   ok;      //成功标识	BIN	1Byte	0成功；1失败。
	 UINT8   FF[5];   //保留字节，补足8字节
}MC_PGN4608_FRAME;

//停止充电完成状态帧
typedef struct _MC_PGN4864_FRAME
{
   UINT8   gun;     //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
   UINT8   cause[2];//停止充电原因	BIN	2Byte	低字节：停止的具体原因，见附录D1；高字节：停止的系统原因，如下：0xE1：计费控制单元控制停止充电。0xE2：充电控制器判定充电机故障，充电控制器自行主动终止充电。0xE3：充电控制器判定车载BMS异常，充电控制器自行主动终止充电。0xE4：车载BMS正常终止充电。0xE5：车载BMS故障终止充电。
	 UINT8   FF[5];   //保留字节，补足8字节
}MC_PGN4864_FRAME;

//停止充电完成应答报文
typedef struct _MC_PGN5120_FRAME
{                                       
   UINT8   gun;     //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
   UINT8   ok;      //成功标识	BIN	1Byte	0成功；1失败。
	 UINT8   FF[6];   //保留字节，补足8字节
}MC_PGN5120_FRAME;

//--------------------------- 3 数据帧 -----------------------------------------
//遥信数据帧
typedef struct _MC_PGN8448_FRAME
{
	UINT8 gun;          //1 充电接口标识	Data0	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
	UINT8 state;        //2 工作状态		0x00正常 0x01- 故障

	UINT8 alarm_sn;     //告警编号
	UINT8 fault;        //告警标识（布尔型, 变化上传；0正常，1异常）

	UINT8 num[2];       //告警值字节，（可以为模拟量值，如“过压”，为“电压值”，模块通信中断为模块编号）

	UINT8 FF[2];        //保留字节，补足8字节
}MC_PGN8448_FRAME;

//遥测数据帧
typedef struct _MC_PGN8704_FRAME
{                                       
	UINT8 state;             //2 工作状态		0x00正常 0x01- 故障
	UINT8 Output_voltage[2]; //2	充电输出电压	2	BIN	2Byte	精确到小数点后1位0V - 950V
	UINT8 Output_current[2]; //3	充电输出电流	4	BIN	2Byte	精确到小数点后2位0A - 400A
	UINT8 SOC;               //4	SOC	6	BIN	2Byte	整型 范围0 - 100
	UINT8 BMS_cell_LT;       //5	电池组最低温度	8	BIN	2Byte	精确到小数点后一位 -50℃ ~ +200℃
	UINT8 BMS_cell_LT_NO;    //5	电池组最低温度编号	8	BIN	2Byte	精确到小数点后一位 -50℃ ~ +200℃
	UINT8 BMS_cell_HT;       //6	电池组最高温度	10	BIN	2Byte	精确到小数点后一位 -50℃ ~ +200℃
	UINT8 BMS_cell_HT_NO;    //6	电池组最高温度编号	10	BIN	2Byte	精确到小数点后一位 -50℃ ~ +200℃

	UINT8 BMS_cell_HV[2];    //7	单体电池最高电压 及其组号	12	BIN	2Byte	精确到小数点后二位 范围0-24V
	
	UINT8 Charge_Mode;        //8充电模式
	UINT8 BMS_cell_HV_NO;     //8最高单体电压

	UINT8 TEMP;              //9	充电机环境温度（枪的温度）	16	BIN	2Byte	精确到个位 范围-50℃ ~ +200℃
	UINT8 line_v;            //10	充电导引电压	18	BIN	2Byte	精确到小数点后1位0A - 12V
	UINT8 BMS_need_time[2];	 //11 充满剩余时间 min
	UINT8 need_voltage[2];   //12	充电输出电压	2	BIN	2Byte	精确到小数点后1位0V - 950V
	UINT8 need_current[2];   //13	充电输出电流	4	BIN	2Byte	精确到小数点后2位0A - 400A
}MC_PGN8704_FRAME;

//遥测响应帧
typedef struct _MC_PGN8960_FRAME
{                                       
   UINT8   gun;    //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
   UINT8   ok;     //确认标识	BIN	1Byte	0成功；1失败。
	 UINT8   crc[2]; //下发参数校验 2字节
	 UINT8   gun_state; //枪的状态
	 UINT8   JKError_system;
	 UINT8   FF[2];    //保留字节，补足8字节
}MC_PGN8960_FRAME;

//遥信充电机状态帧
typedef struct _MC_PGN9472_FRAME
{
	UINT8 gun;          //1 充电接口标识	Data0	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
	UINT8 state;        //2 工作状态	

	UINT8 FF[6];        //保留字节，补足8字节
}MC_PGN9472_FRAME;

//--------------------------- 4 心跳帧 -----------------------------------------
//心跳报文
typedef struct _MC_PGN12544_FRAME
{                                       
	UINT8   gun;        //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
	UINT8   mc_state;   //计费控制单元状态信息	BIN	1Byte	0-正常 1-故障
	UINT8   kwh[2];     //当前充电电量	BIN	2Byte	数据分辨率：0.1 kWh/位，0 kWh偏移量；数据范围：0~1000 kWh;（待机过程中此数据项为0）
	UINT8   time[2];    //累计充电时长	BIN	2Byte	数据分辨率：1 min/位，0 min偏移量；数据范围：0~6000 min；（待机过程中此数据项为0）
	UINT8   FF[2];   //保留字节，补足8字节
}MC_PGN12544_FRAME;
//心跳响应报文
typedef struct _MC_PGN12800_FRAME
{                                       
	UINT8   gun;        //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
	UINT8   mc_state;   //计费控制单元状态信息	BIN	1Byte	0-正常 1-故障
	UINT8   kwh[2];     //当前充电电量	BIN	2Byte	数据分辨率：0.1 kWh/位，0 kWh偏移量；数据范围：0~1000 kWh;（待机过程中此数据项为0）
	UINT8   time[2];    //累计充电时长	BIN	2Byte	数据分辨率：1 min/位，0 min偏移量；数据范围：0~6000 min；（待机过程中此数据项为0）
	UINT8   FF[2];   //保留字节，补足8字节
}MC_PGN12800_FRAME;

//---------------------------  错误报文-----------------------------------------

//-------------------------------------  CAN升级程序----------------------------------------------
//升级请求报文(PF:0x52)  计费单元--控制器
typedef struct _MC_PGN20992_FRAME
{
	UINT8 gun;                //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
	UINT8 Firmware_Bytes[3]; //固件字节数 先传低字节后传高字节（程序超过了63KB）
  UINT8 Firmware_CRC[4];  //先传低字节后传高字节
}MC_PGN20992_FRAME;

//升级请求报文响应报文(PF:0x53) 控制器--计费单元
typedef struct _MC_PGN21248_FRAME
{
	UINT8 gun;              //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
  UINT8 Success_flag;     //0--成功；1-失败
	UINT8 FF[6];
}MC_PGN21248_FRAME;

//请求升级数据报文帧(PF:0x54)  控制器--计费单元
typedef struct _MC_PGN21504_FRAME
{
  UINT8 gun;              //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
	UINT8 FF[7];
}MC_PGN21504_FRAME;

//升级数据包报文(PF:0x55)  计费单元--控制器
typedef struct _MC_PGN21760_FRAME
{
	UINT8 gun;                  //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
	UINT8 Tran_Packet_Number[2];  //传输数据包编号
	UINT8 Tran_data[5];       //程序二进制数据先传低字节后传高字节，最后1包不足5字节时以0xFF补齐
}MC_PGN21760_FRAME;

//升级数据包应答报文(PF:0x56)  控制器--计费单元
typedef struct _MC_PGN22016_FRAME
{
	UINT8 gun;                  //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
  UINT8 Success_flag;     //0--成功；1-失败
	UINT8 Tran_Packet_Number[2];  //从2开始,先传低字节,再高字节,如果此值小于计费单元发送过来的值， 表明计费单元需要重发该包编号的数据
	UINT8 FF[4];
}MC_PGN22016_FRAME;


//调试参数下发(PF:0x57)  计费单元--控制器
typedef struct _MC_PGN22272_FRAME
{
	UINT8 gun;                  //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
  UINT8 Shileld_emergency_switch:1;//屏蔽急停按下故障
	UINT8 Shileld_PE_Fault:1;	//屏蔽接地故障告警	    
	UINT8 Shileld_AC_connect_lost:1;	//屏蔽交流输通讯告警	    
	UINT8 Shileld_AC_input_switch_off:1;//屏蔽交流输入开关跳闸
	
	UINT8 Shileld_AC_fanglei_off:1;    //屏蔽交流防雷器跳闸
	UINT8 Shileld_AC_output_switch_off_1:1;//屏蔽直流输出开关跳闸1.2
	UINT8 Shileld_AC_output_switch_off_2:1;//屏蔽直流输出开关跳闸1.2
	UINT8 Shileld_AC_input_over_limit:1;//屏蔽交流输入过压保护 ----  data1
	
	UINT8 Shileld_AC_input_lown_limit:1; //屏蔽交流输入欠压
	UINT8 Shileld_GUN_LOCK_FAILURE:1;  //屏蔽枪锁检测未通过
	UINT8 Shileld_AUXILIARY_POWER:1;   //屏蔽辅助电源检测未通过
	UINT8 Shileld_OUTSIDE_VOLTAGE:1;    //屏蔽输出接触器外侧电压检测（标准10V，放开100V）
	
	UINT8 Shileld_SELF_CHECK_V:1;      //屏蔽自检输出电压异常
	UINT8 Shileld_INSULATION_FAULT:1;   //屏蔽绝缘故障
	UINT8 Shileld_BMS_WARN:1;          //屏蔽 BMS告警终止充电
	UINT8 Shileld_CHARG_OUT_OVER_V:1;  //屏蔽输出过压停止充电   ----  data2
	
	UINT8 Shileld_OUT_OVER_BMSH_V:1;   //屏蔽输出电压超过BMS最高电压
	UINT8 Shileld_CHARG_OUT_OVER_C:1;  //屏蔽输出过流
	UINT8 Shileld_OUT_SHORT_CIRUIT:1;  //屏蔽输出短路
	UINT8 Shileld_GUN_OVER_TEMP:1;     //屏蔽充电枪温度过高
	
	UINT8 Shileld_OUT_SWITCH:1;       //屏蔽输出开关
	UINT8 Shileld_OUT_NO_CUR:1;       //屏蔽输出无电流显示
  UINT8 Shileld_Link_Switch:1;      //屏蔽并机接触器     ----  data3
	UINT8 keep:1;                     //保留
	
  UINT8  keep_data[4];             //保留
}MC_PGN22272_FRAME;

//调试参数反馈(PF:0x56)  控制器--计费单元
typedef struct _MC_PGN22528_FRAME
{
	UINT8 gun;               //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
  UINT8 Success_flag;     //0--成功；1-失败
  UINT8 Shileld_emergency_switch:1;//屏蔽急停按下故障
	UINT8 Shileld_PE_Fault:1;	//屏蔽接地故障告警	    
	UINT8 Shileld_AC_connect_lost:1;	//屏蔽交流输通讯告警	    
	UINT8 Shileld_AC_input_switch_off:1;//屏蔽交流输入接触器
	
	UINT8 Shileld_AC_fanglei_off:1;    //屏蔽交流防雷器跳闸
	UINT8 Shileld_AC_output_switch_off_1:1;//屏蔽直流输出开关跳闸1.2
	UINT8 Shileld_AC_output_switch_off_2:1;//屏蔽直流输出开关跳闸1.2
	UINT8 Shileld_AC_input_over_limit:1;//屏蔽交流输入过压保护 ----  data1
	
	UINT8 Shileld_AC_input_lown_limit:1; //屏蔽交流输入欠压
	UINT8 Shileld_GUN_LOCK_FAILURE:1;  //屏蔽枪锁检测未通过
	UINT8 Shileld_AUXILIARY_POWER:1;   //屏蔽辅助电源检测未通过
	UINT8 Shileld_OUTSIDE_VOLTAGE:1;    //屏蔽输出接触器外侧电压检测（标准10V，放开100V）
	
	UINT8 Shileld_SELF_CHECK_V:1;      //屏蔽自检输出电压异常
	UINT8 Shileld_INSULATION_FAULT:1;   //屏蔽绝缘故障
	UINT8 Shileld_BMS_WARN:1;          //屏蔽 BMS告警终止充电
	UINT8 Shileld_CHARG_OUT_OVER_V:1;  //屏蔽输出过压停止充电   ----  data2
	
	UINT8 Shileld_OUT_OVER_BMSH_V:1;   //屏蔽输出电压超过BMS最高电压
	UINT8 Shileld_CHARG_OUT_OVER_C:1;  //屏蔽输出过流
	UINT8 Shileld_OUT_SHORT_CIRUIT:1;  //屏蔽输出短路
	UINT8 Shileld_GUN_OVER_TEMP:1;     //屏蔽充电枪温度过高
	
	UINT8 Shileld_OUT_SWITCH:1;       //屏蔽输出开关
	UINT8 Shileld_OUT_NO_CUR:1;       //屏蔽输出无电流显示
  UINT8 Shileld_Link_Switch:1;      //屏蔽并机接触器     ----  data3
  UINT8 keep:1;                     //保留
	
  UINT8 keep_data[3];             //保留
}MC_PGN22528_FRAME;

typedef struct _MC_PGN24576_FRAME
{
	UINT8 gun;             //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
  UINT8 Relay_control;  //继电器控制
	UINT8 FF[6];       //程序二进制数据先传低字节后传高字节，最后1包不足5字节时以0xFF补齐
}MC_PGN24576_FRAME;
typedef struct _MC_PGN24832_FRAME
{
  UINT8 gun;             //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
  UINT8 Relay_control;  //继电器控制
	UINT8 FF[6];       //程序二进制数据先传低字节后传高字节，最后1包不足5字节时以0xFF补齐
}MC_PGN24832_FRAME;

typedef struct _MC_PGN3584_FRAME
{
   UINT8   gun;     //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
   UINT8   Operation_command;   // 1上锁 2,解锁
	 UINT8   FF[6];   //保留字节，补足8字节
}MC_PGN3584_FRAME;
typedef struct _MC_PGN3840_FRAME
{                                       
   UINT8   gun;     //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
   UINT8   Operation_command;   // 1上锁 2,解锁
	 UINT8   FF[6];   //保留字节，补足8字节
}MC_PGN3840_FRAME;

//接收BMS上传过来的剩余变量数据。(比较固定的充电中不会更改的)
typedef struct _MC_PGN25088_FRAME
{  
  UINT8 BMS_Vel[3];         //BMS协议版本                                    
	UINT8 Battery_Rate_AH[2]; //2	整车动力蓄电池额定容量0.1Ah
	UINT8 Battery_Rate_Vol[2]; //整车动力蓄电池额定总电压
	UINT8 Battery_Rate_KWh[2]; //动力蓄电池标称总能量KWH
	UINT8 Cell_H_Cha_Vol[2];     //单体动力蓄电池最高允许充电电压 0.01 0-24v
	UINT8 Cell_H_Cha_Temp;     //单体充电最高保护温度 -50+200
	UINT8 VIN[17];              //车辆识别码
	UINT8 FF[10];
}MC_PGN25088_FRAME;
//BMS信息反馈
typedef struct _MC_PGN25344_FRAME
{
  UINT8 gun;             //充电接口标识	BIN	1Byte	一桩（机）多充时用来标记接口号。一桩（机）一充时此项为0，多个接口时顺序对每个接口进行编号，范围1-255。
  UINT8 VIN_Results;  //VIN反馈结果，1--允许继续充电，其他结束充电
	UINT8 FF[6];       //程序二进制数据先传低字节后传高字节，最后1包不足5字节时以0xFF补齐
}MC_PGN25344_FRAME;

//---------------------------  全局计费单元与控制器的变量-----------------------
typedef struct _MC_ALL_FRAME
{
	MC_PGN256_FRAME MC_PGN256_frame;     //充电启动帧
	UINT32 MC_Recved_PGN256_Flag;

	MC_PGN512_FRAME  MC_PGN512_frame;     //启动应答帧
	UINT32 MC_Recved_PGN512_Flag;

	MC_PGN768_FRAME  MC_PGN768_frame;
	UINT32 MC_Recved_PGN768_Flag;           //充电停止帧

	MC_PGN1024_FRAME  MC_PGN1024_frame;
	UINT32 MC_Recved_PGN1024_Flag;           //停止应答帧

	MC_PGN1280_FRAME  MC_PGN1280_frame;
	UINT32 MC_Recved_PGN1280_Flag;           //下发对时帧

	MC_PGN1536_FRAME  MC_PGN1536_frame;
	UINT32 MC_Recved_PGN1536_Flag;           //对时应答帧

	MC_PGN1792_FRAME  MC_PGN1792_frame;
	UINT32 MC_Recved_PGN1792_Flag;           //校验版本	

	MC_PGN2048_FRAME  MC_PGN2048_frame;
	UINT32 MC_Recved_PGN2048_Flag;           //版本确认

	MC_PGN2304_FRAME  MC_PGN2304_frame;
	UINT32 MC_Recved_PGN2304_Flag;           //下发充电参数信息	

	MC_PGN2560_FRAME  MC_PGN2560_frame;
	UINT32 MC_Recved_PGN2560_Flag;           //充电参数信息确认

	MC_PGN2816_FRAME  MC_PGN2816_frame;
	UINT32 MC_Recved_PGN2816_Flag;           //请求充电参数信息

	MC_PGN3072_FRAME  MC_PGN3072_frame;
	UINT32 MC_Recved_PGN3072_Flag;           //下发直流分流器

	MC_PGN3328_FRAME  MC_PGN3328_frame;
	UINT32 MC_Recved_PGN3328_Flag;           //直流分流器信息确认

	MC_PGN4352_FRAME  MC_PGN4352_frame;
	UINT32 MC_Recved_PGN4352_Flag;           //启动完成帧

	MC_PGN4608_FRAME  MC_PGN4608_frame;
	UINT32 MC_Recved_PGN4608_Flag;           //启动完成应答帧

	MC_PGN4864_FRAME  MC_PGN4864_frame;
	UINT32 MC_Recved_PGN4864_Flag;           //停止完成帧

	MC_PGN5120_FRAME  MC_PGN5120_frame;
	UINT32 MC_Recved_PGN5120_Flag;           //停止完成应答帧

	MC_PGN8448_FRAME  MC_PGN8448_frame;
	UINT32 MC_Recved_PGN8448_Flag;           //遥信帧

	MC_PGN8704_FRAME  MC_PGN8704_frame;
	UINT32 MC_Recved_PGN8704_Flag;           //遥测帧	

	MC_PGN8960_FRAME  MC_PGN8960_frame;
	UINT32 MC_Recved_PGN8960_Flag;           //遥测响应帧

	AC_MEASURE_DATA  MC_PGN9216_frame;
	UINT32 MC_Recved_PGN9216_Flag;           //遥测帧	AC采集数据

	MC_PGN9472_FRAME  MC_PGN9472_frame;
	UINT32 MC_Recved_PGN9472_Flag;           //遥信充电机状态帧

	MC_PGN12544_FRAME  MC_PGN12544_frame;
	UINT32 MC_Recved_PGN12544_Flag;           //心跳帧

	MC_PGN12800_FRAME  MC_PGN12800_frame;
	UINT32 MC_Recved_PGN12800_Flag;           //心跳响应帧
	
	
	MC_PGN21248_FRAME  MC_PGN21248_frame;
	UINT32 MC_Recved_PGN21248_Flag;           //升级请求报文响应报文
	
	MC_PGN21504_FRAME  MC_PGN21504_frame;
	UINT32 MC_Recved_PGN21504_Flag;           //请求升级数据报文帧
	
	MC_PGN22016_FRAME  MC_PGN22016_frame;
	UINT32 MC_Recved_PGN22016_Flag;           //升级数据包应答报文
	UINT32 MC_Recved_PGN24832_Flag; 
	UINT32 MC_Recved_PGN3840_Flag; 
	
	UINT32 Power_kw_percent;
	UINT32 Power_Adjust_Command;
	UINT32 MC_Recved_Power_PGN4096_Flag; 
	
	MC_PGN25088_FRAME MC_PGN25088_frame;
	UINT32 MC_Recved_PGN25088_Flag; 
}MC_ALL_FRAME;

typedef struct _MC_VAR
{
	UINT32 Total_Mess_Size;
	UINT32 Total_Num_Packet;
	UINT32 sequence;
}MC_VAR;

#endif
