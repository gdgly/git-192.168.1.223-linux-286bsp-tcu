/*******************************************************************************
  Copyright(C)    2014-2015,      EAST Co.Ltd.
  File name :     iso8583.h
  Author :        dengjh
  Version:        1.0
  Date:           2015.5.19
  Description:    
  Other:          
  Function List:  
  History:        
          <author>   <time>   <version>   <desc>
            dengjh    2015.5.19   1.0      Create
*******************************************************************************/
#ifndef	ISO8583_H
#define	ISO8583_H

/*===========================================================================*/
/* Data Size Definition                                                      */
/*---------------------------------------------------------------------------*/
#define	ISO8583_BUSY_SN		20	//业务流水号长度

typedef struct _ISO8583_Head_Struct
{
	//会晤报文头 Ans8
	unsigned char SessionLen[2];   //报文长度
	unsigned char SessionRule;     //应用协议规则 本标准直接填 ‘0’
	unsigned char SessionResult;   //会晤结果  本标准直接填 ‘0’
	unsigned char SessionReserv[4];//内部私有域 本标准直接填 ‘0’

	//报文类型 An4
	unsigned char MessgTypeIdent[4]; //消息标识符

	//(主位图) B64
	unsigned char PrimaryBitmap[8];  //主位元表
}ISO8583_Head_Frame;


//签到请求 数据帧
typedef	struct _ISO8583_AP_Get_Random_Struct
{ 	
  unsigned char charge_pipletype[2];     //充电桩类型（01：单枪直流充电桩，02单相交流充电桩，03三相交流充电桩，04：双枪轮流充直流充电桩，05：双枪同时充直流充电桩）33	N	   N2
	unsigned char sec[2];     //加密方式 （01）	38	N	N2
	unsigned char protocol[4];//协议版本号	39	N	N4
	unsigned char type[2];     //终端类型（01：充电桩，02：网点）	40	N		N2
	unsigned char ter_sn[12];  //充电终端序列号 41	N 	n12
}ISO8583_AP_Get_Random_Frame;

//签到请求 响应数据帧
typedef	struct _ISO8583_AP_Get_Random_Resp_Struct
{ 
	unsigned char random[32];  //随机字符串	4	N	N32
	unsigned char ip[80];      //IP列表	5	N	N80
	unsigned char data_len[2]; //后面数据长度域 Ans..50 所以长度域 2字节 0x35 0x30
	unsigned char http[50];    //APP下载地址	6	Y	Ans..50
	unsigned char NO_car_uptime_lock[8];//7	Y地锁检测无车提示上升的时间（单位：分钟）
	unsigned char flag[2];    //信息处理结果标识（00：成功；01：无效充电桩序列号：02：无效终端类型）	39	N	An2
}ISO8583_AP_Get_Random_Resp_Frame;

//工作密钥请求 请求帧
typedef	struct _ISO8583_AP_Get_Key_Struct
{
	unsigned char random[32];  //随机字符串	4	N	N32
	unsigned char fenshi[2];   //电费类型（01：通用，02：分时）	6	N	N2
	unsigned char ground_lock_num[2];//7地锁个数 00--没有 01--1个02-两个
	unsigned char type[2];     //终端类型（01：充电桩，02：网点）	40	N		N2
	unsigned char ter_sn[12];  //充电终端序列号    41	N	n12
	unsigned char ESAM[12];    //ESAM卡号          42	n	n12
	unsigned char MD5[16];     //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_AP_Get_Key_Frame;

//工作密钥请求 响应数据帧
typedef	struct _ISO8583_AP_Get_Key_Resp_Struct
{ 
	unsigned char flag[2];    //信息处理结果标识（00：成功；01：MAC校验失败，02：终端未对时,03:无效的充电桩序列号，04：无效终端类型05：ISAM卡无效 ，06：ESAM卡不对应） 39	N	An2
	unsigned char KEY[16];    //工作密钥密文 40	N	N16
	unsigned char time[14];    //服务器时间（格式yyyyMMddHHmmss）或随机码（网点）	61	N	n14
}ISO8583_AP_Get_Key_Resp_Frame;

//心跳包中的实时数据
typedef	struct _ISO8583_AP_Heart_Real_Data_01_Struct
{
	unsigned char state;             //工作状态	BIN码	1Byte	0x01-故障  0x02-告警  03-空闲 0x04-充电中 0x05-完成 0x06-预约
	unsigned char gun1;              //充电枪1状态	BIN码	1Byte	0x01-故障  0x02-告警  03-空闲 0x04-充电中 0x05-完成
	unsigned char reserve1;

	unsigned char Output_voltage[2];    //充电机输出电压	BIN码	2Byte	精确到小数点后一位
	unsigned char Output_current[2];    //充电机输出电流	BIN码	2Byte	精确到小数点后二位
	unsigned char batt_SOC;             //SOC	BIN码	1Byte	整型
	unsigned char batt_LT[2];           //电池组最低温度	BIN码	2Byte	精确到小数点后一位
	unsigned char batt_HT[2];           //电池组最高温度	BIN码	2Byte	精确到小数点后一位

	unsigned char cell_HV[2];           //单体电池最高电压	BIN码	2Byte	精确到小数点后三位
	unsigned char cell_LV[2];           //单体电池最低电压	BIN码	2Byte	精确到小数点后三位
	unsigned char KWH[4];               //有功总电度	BIN码	4Byte	精确到小数点后二位
	unsigned char Time[2];              //累计充电时间	BIN码	2Byte	单位：min
	unsigned char need_time[2];         //剩余时间   单位 分钟
	//unsigned char reserve2[20];         //保留 20字节---告警
	
	unsigned char Voltmeter_comm_fault_1;  //  1#电压表通信中断
  unsigned char Currtmeter_comm_fault_1;  //1#电流表通信中断
  unsigned char Charge_control_comm_fault_1;  //1#接口板通信中断
	unsigned char Meter_comm_fault_1;  //1#电能表通信中断	
  unsigned char Card_Reader_comm_fault_1;//1#读卡器通信中断
  unsigned char AC_comm_fault_1;//1#交流采集模块通信中断
  unsigned char Rectifier_comm_fault_1;//1#组充电模块通信中断
  unsigned char Rectifier_fault_1;//1#组充电模块故障
 
  unsigned char Emergency_stop_fault;//急停故障
  unsigned char Rectifier_output_short_fault_1;//1#组充电机输出短路故障
  unsigned char System_Alarm_Insulation_fault_1; //1#组充电机绝缘故障
  unsigned char Rectifier_output_over_vol_fault_1;//1#组充电机输出过压保护
  unsigned char Rectifier_input_over_vol_fault_1;//1#1组充电机输入过压保护
  unsigned char system_output_overvoltage_fault_1;//1#组充电电压过压告警
  unsigned char system_output_over_current_fault_1;//1#1组充电电流过流告警
  unsigned char Disconnect_gun_1;//1#充电枪连接断开
  unsigned char BMS_comm_fault_1;//1#1组BMS通信异常
	unsigned char Reverse_Battery_1;//1#1组电池反接
  unsigned char battery_voltage_fault_1;//1#电池电压检测
	unsigned char BMS_fault_1;//1#组BMS故障终止充电
	unsigned char ground_lock;          //地锁1状态
}ISO8583_AP_Heart_Real_Data_Frame_01;


//心跳 请求帧
typedef	struct _ISO8583_AP_Heart_01_Struct
{
	unsigned char charge_way[2];  //充电方式（01：刷卡，02：手机）	3	Y	N2
	unsigned char card_sn[16];//充电卡号或账号	4	Y	N16
	unsigned char control_type[3];     //控制类型	5	N	N3	控制类型见下表对应数据--对应的是自动还是金额电量时间等。
	unsigned char charge_limt_data[8];     //开启限制数据	6	N	N8	自动充满停机方式下无意义。电量控制,单位为“度”，精确到0.01,传输数据放大100倍；时间控制，单位为“秒“，精确到1,；金额控制，单位为“元”，精确到0.01，传输数据放大100倍 结合控制内型。
	unsigned char appointment_start_time[14];//预约开始时间（格式yyyyMMddHHmmss）	7	Y	N14
  unsigned char appointment_end_time[14];//预约结束时间（格式yyyyMMddHHmmss）	8	Y	N14
	unsigned char stop_car_time[14];//停车费开始时间（格式yyyyMMddHHmmss）9 Y	N14
  unsigned char Busy_SN[ISO8583_BUSY_SN];//业务流水号	11	 N	N20
	unsigned char type[2];     //充电桩类型（01：单枪直流充电桩，02单相交流充电桩，03三相交流充电桩，04：双枪轮流充直流充电桩，05：双枪同时充直流充电桩）33	N	   N2
	unsigned char data_len[3]; //后面数据长度域 Ans…999 所以长度域 3字节 0x31 0x37 0x32
	unsigned char data[2*sizeof(ISO8583_AP_Heart_Real_Data_Frame_01)];    //充电桩数据	35	N	   Ans…999  本机为 (24+20)*2个字节
	unsigned char rmb[8];       //充电总金额（以分为单位）	36	N	n8
	unsigned char serv_rmb[8];  //服务费金额	37	N	N8
	unsigned char gun[2];       //充电抢号	38	N	N2
	unsigned char ter_sn[12];  //充电终端序列号    41	N	n12
	unsigned char time[14];    //终端时间（格式yyyyMMddHHmmss）	62	N	N14
	unsigned char MD5[16];     //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_AP_Heart_Frame_01;

typedef	struct _ISO8583_AP_Heart_01_NOchargeStruct
{
	unsigned char type[2];     //充电桩类型（01：单枪直流充电桩，02单相交流充电桩，03三相交流充电桩，04：双枪轮流充直流充电桩，05：双枪同时充直流充电桩）33	N	   N2
	unsigned char data_len[3]; //后面数据长度域 Ans…999 所以长度域 3字节 0x31 0x37 0x32
	unsigned char data[2*sizeof(ISO8583_AP_Heart_Real_Data_Frame_01)];    //充电桩数据	35	N	   Ans…999  本机为 (24+20)*2个字节
	unsigned char rmb[8];       //充电总金额（以分为单位）	36	N	n8
	unsigned char serv_rmb[8];  //服务费金额	37	N	N8
	unsigned char gun[2];       //充电抢号	38	N	N2
	unsigned char ter_sn[12];  //充电终端序列号    41	N	n12
	unsigned char time[14];    //终端时间（格式yyyyMMddHHmmss）	62	N	N14
	unsigned char MD5[16];     //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_AP_Heart_NOchargeFrame_01;

//轮充具体数据
typedef	struct _ISO8583_AP_Heart_Real_Data_04_Struct
{
	unsigned char state;             //工作状态	BIN码	1Byte	0x01-故障  0x02-告警  03-空闲 0x04-充电中 0x05-完成 0x06-预约
	unsigned char gun1;              //充电枪1状态	BIN码	1Byte	0x01-故障  0x02-告警  03-空闲 0x04-充电中 0x05-完成
	unsigned char gun2;              //充电枪2状态	BIN码	1Byte	0x01-故障  0x02-告警  03-空闲 0x04-充电中 0x05-完成

	unsigned char Output_voltage[2];    //充电机输出电压	BIN码	2Byte	精确到小数点后一位
	unsigned char Output_current[2];    //充电机输出电流	BIN码	2Byte	精确到小数点后二位
	unsigned char batt_SOC;             //SOC	BIN码	1Byte	整型
	unsigned char batt_LT[2];           //电池组最低温度	BIN码	2Byte	精确到小数点后一位
	unsigned char batt_HT[2];           //电池组最高温度	BIN码	2Byte	精确到小数点后一位

	unsigned char cell_HV[2];           //单体电池最高电压	BIN码	2Byte	精确到小数点后三位
	unsigned char cell_LV[2];           //单体电池最低电压	BIN码	2Byte	精确到小数点后三位
	unsigned char KWH[4];               //有功总电度	BIN码	4Byte	精确到小数点后二位
	unsigned char Time[2];              //累计充电时间	BIN码	2Byte	单位：min
	unsigned char need_time[2];         //剩余时间   单位 分钟
	//unsigned char reserve2[22];         //保留 20字节--告警由下面替代
  unsigned char Voltmeter_comm_fault_1;  //  1#电压表通信中断
  unsigned char Currtmeter_comm_fault_1;  //1#电流表通信中断
  unsigned char Charge_control_comm_fault_1;  //1#接口板通信中断
	unsigned char Meter_comm_fault_1;  //1#电能表通信中断
  unsigned char Card_Reader_comm_fault_1;//1#读卡器通信中断
  unsigned char AC_comm_fault_1;//1#交流采集模块通信中断
  unsigned char Rectifier_comm_fault_1;//1#组充电模块通信中断
  unsigned char Rectifier_fault_1;//1#组充电模块故障
  unsigned char Emergency_stop_fault;//急停故障
  unsigned char Rectifier_output_short_fault_1;//1#组充电机输出短路故障
  unsigned char System_Alarm_Insulation_fault_1; //1#组充电机绝缘故障
  unsigned char System_Alarm_Insulation_fault_2;  //2#组充电机绝缘故障
  unsigned char Rectifier_output_over_vol_fault_1;//1#组充电机输出过压保护
  unsigned char Rectifier_input_over_vol_fault_1;//1#1组充电机输入过压保护
	
  unsigned char system_output_overvoltage_fault_1;//1#组充电电压过压告警
  unsigned char system_output_over_current_fault_1;//1#1组充电电流过流告警
  unsigned char Disconnect_gun_1;//1#充电枪连接断开
  unsigned char Disconnect_gun_2;//2#充电枪连接断开
  unsigned char BMS_comm_fault_1;//1#1组BMS通信异常
	unsigned char Reverse_Battery_1;//1#1组电池反接
  unsigned char battery_voltage_fault_1;//1#电池电压检测
	unsigned char BMS_fault_1;//1#组BMS故障终止充电
	unsigned char ground_lock_1;  //地锁1状态
	unsigned char ground_lock_2;  //地锁2状态
}ISO8583_AP_Heart_Real_Data_Frame_04;

//心跳 请求帧---充电的时候
typedef	struct _ISO8583_AP_Heart_04_Struct
{
	unsigned char charge_way[2];  //充电方式（01：刷卡，02：手机）	3	Y	N2
	unsigned char card_sn[16];//充电卡号或账号	4	Y	N16
	unsigned char control_type[3];     //控制类型	5	N	N3	控制类型见下表对应数据--对应的是自动还是金额电量时间等。
	unsigned char charge_limt_data[8];     //开启限制数据	6	N	N8	自动充满停机方式下无意义。电量控制,单位为“度”，精确到0.01,传输数据放大100倍；时间控制，单位为“秒“，精确到1,；金额控制，单位为“元”，精确到0.01，传输数据放大100倍 结合控制内型。
	unsigned char appointment_start_time[14];//预约开始时间（格式yyyyMMddHHmmss）	7	Y	N14
  unsigned char appointment_end_time[14];//预约结束时间（格式yyyyMMddHHmmss）	8	Y	N14
	unsigned char stop_car_time[14];//停车费开始时间（格式yyyyMMddHHmmss）9 Y	N14

	unsigned char Busy_SN[ISO8583_BUSY_SN];//业务流水号	11	 N	N20
	unsigned char type[2];     //充电桩类型（01：单枪直流充电桩，02单相交流充电桩，03三相交流充电桩，04：双枪轮流充直流充电桩，05：双枪同时充直流充电桩）33	N	   N2
	unsigned char data_len[3]; //后面数据长度域 Ans…999 所以长度域 3字节 0x31 0x37 0x32
	unsigned char data[2*sizeof(ISO8583_AP_Heart_Real_Data_Frame_04)];    //充电桩数据	35	N	   Ans…999  本机为 (24+22)*2个字节
	unsigned char rmb[8];       //充电总金额（以分为单位）	36	N	n8
	unsigned char serv_rmb[8];  //服务费金额	37	N	N8
	unsigned char gun[2];       //充电抢号	38	N	N2
	unsigned char ter_sn[12];  //充电终端序列号    41	N	n12
	unsigned char time[14];    //终端时间（格式yyyyMMddHHmmss）	62	N	N14
	unsigned char MD5[16];     //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_AP_Heart_Frame_04;
//非充电的时候
typedef	struct _ISO8583_AP_Heart_04_NOchargeStruct
{
	unsigned char type[2];     //充电桩类型（01：单枪直流充电桩，02单相交流充电桩，03三相交流充电桩，04：双枪轮流充直流充电桩，05：双枪同时充直流充电桩）33	N	   N2
	unsigned char data_len[3]; //后面数据长度域 Ans…999 所以长度域 3字节 0x31 0x37 0x32
	unsigned char data[2*sizeof(ISO8583_AP_Heart_Real_Data_Frame_04)];    //充电桩数据	35	N	   Ans…999  本机为 (24+22)*2个字节
	unsigned char rmb[8];       //充电总金额（以分为单位）	36	N	n8
	unsigned char serv_rmb[8];  //服务费金额	37	N	N8
	unsigned char gun[2];       //充电抢号	38	N	N2
	unsigned char ter_sn[12];  //充电终端序列号    41	N	n12
	unsigned char time[14];    //终端时间（格式yyyyMMddHHmmss）	62	N	N14
	unsigned char MD5[16];     //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_AP_Heart_NOchargeFrame_04;


//心跳包中的实时数据 --双枪
typedef	struct _ISO8583_AP_Heart_Real_Data_Struct
{
	unsigned char state;             //工作状态	BIN码	1Byte	0x01-故障  0x02-告警  03-空闲 0x04-充电中 0x05-完成 0x06-预约
	unsigned char gun1;              //充电枪1状态	BIN码	1Byte	0x01-故障  0x02-告警  03-空闲 0x04-充电中 0x05-完成
	unsigned char gun2;              //充电枪2状态	BIN码	1Byte	0x01-故障  0x02-告警  03-空闲 0x04-充电中 0x05-完成

	unsigned char Output_voltage1[2];    //充电机输出电压	BIN码	2Byte	精确到小数点后一位
	unsigned char Output_voltage2[2];    //充电机输出电压	BIN码	2Byte	精确到小数点后一位
	unsigned char Output_current1[2];    //充电机输出电流	BIN码	2Byte	精确到小数点后二位
	unsigned char Output_current2[2];    //充电机输出电流	BIN码	2Byte	精确到小数点后二位
	unsigned char batt_SOC1;             //SOC	BIN码	1Byte	整型
	unsigned char batt_SOC2;             //SOC	BIN码	1Byte	整型
	unsigned char batt_LT1[2];           //电池组最低温度	BIN码	2Byte	精确到小数点后一位
	unsigned char batt_LT2[2];           //电池组最低温度	BIN码	2Byte	精确到小数点后一位
	unsigned char batt_HT1[2];           //电池组最高温度	BIN码	2Byte	精确到小数点后一位
	unsigned char batt_HT2[2];           //电池组最高温度	BIN码	2Byte	精确到小数点后一位

	unsigned char cell_HV1[2];           //单体电池最高电压	BIN码	2Byte	精确到小数点后三位
	unsigned char cell_HV2[2];           //单体电池最高电压	BIN码	2Byte	精确到小数点后三位
	unsigned char cell_LV1[2];           //单体电池最低电压	BIN码	2Byte	精确到小数点后三位
	unsigned char cell_LV2[2];           //单体电池最低电压	BIN码	2Byte	精确到小数点后三位
	unsigned char KWH1[4];               //有功总电度	BIN码	4Byte	精确到小数点后二位
	unsigned char KWH2[4];               //有功总电度	BIN码	4Byte	精确到小数点后二位
	unsigned char Time1[2];              //累计充电时间	BIN码	2Byte	单位：min
	unsigned char Time2[2];              //累计充电时间	BIN码	2Byte	单位：min

	unsigned char need_time1[2];         //剩余时间   单位 分钟
	unsigned char need_time2[2];         //剩余时间   单位 分钟
 
  unsigned char Voltmeter_comm_fault_1;  //  1#电压表通信中断
  unsigned char Voltmeter_comm_fault_2;  //2#电压表通信中断
  unsigned char Currtmeter_comm_fault_1;  //1#电流表通信中断
  unsigned char Currtmeter_comm_fault_2;  //2#电流表通信中断
  unsigned char Charge_control_comm_fault_1;  //1#接口板通信中断
  unsigned char Charge_control_comm_fault_2;  //2#接口板通信中断
	unsigned char Meter_comm_fault_1;  //1#电能表通信中断
  unsigned char Meter_comm_fault_2;  //2#电能表通信中断
	
  unsigned char Card_Reader_comm_fault_1;//1#读卡器通信中断
  unsigned char Card_Reader_comm_fault_2;//2#读卡器通信中断
  unsigned char AC_comm_fault_1;//1#交流采集模块通信中断
  unsigned char AC_comm_fault_2;//2#交流采集模块通信中断
  unsigned char Rectifier_comm_fault_1;//1#组充电模块通信中断
  unsigned char Rectifier_comm_fault_2;//2#组充电模块通信中断
  unsigned char Rectifier_fault_1;//1#组充电模块故障
  unsigned char Rectifier_fault_2;//2#组充电模块故障
 
  unsigned char Emergency_stop_fault;//急停故障
  unsigned char Rectifier_output_short_fault_1;//1#组充电机输出短路故障
  unsigned char Rectifier_output_short_fault_2;//2#组充电机输出短路故障
  unsigned char System_Alarm_Insulation_fault_1; //1#组充电机绝缘故障
  unsigned char System_Alarm_Insulation_fault_2;  //2#组充电机绝缘故障
  unsigned char Rectifier_output_over_vol_fault_1;//1#组充电机输出过压保护
  unsigned char Rectifier_output_over_vol_fault_2;//2#组充电机输出过压保护
  unsigned char Rectifier_input_over_vol_fault_1;//1#1组充电机输入过压保护
	
  unsigned char Rectifier_input_over_vol_fault_2;//2#1组充电机输入过压保护
  unsigned char system_output_overvoltage_fault_1;//1#组充电电压过压告警
  unsigned char system_output_overvoltage_fault_2;//2#组充电电压过压告警
  unsigned char system_output_over_current_fault_1;//1#1组充电电流过流告警
  unsigned char system_output_over_current_fault_2;//2#1组充电电流过流告警
  unsigned char Disconnect_gun_1;//1#充电枪连接断开
  unsigned char Disconnect_gun_2;//2#充电枪连接断开
  unsigned char BMS_comm_fault_1;//1#1组BMS通信异常
	
  unsigned char BMS_comm_fault_2;//2#1组BMS通信异常
	unsigned char Reverse_Battery_1;//1#1组电池反接
  unsigned char Reverse_Battery_2;//2#1组电池反接
  unsigned char battery_voltage_fault_1;//1#电池电压检测
  unsigned char battery_voltage_fault_2;//2#电池电压检测
	unsigned char BMS_fault_1;//1#组BMS故障终止充电
  unsigned char BMS_fault_2;//2#组BMS故障终止充电
  unsigned char ground_lock_1;  //地锁1状态
	unsigned char ground_lock_2;  //地锁2状态
}ISO8583_AP_Heart_Real_Data_Frame;

//心跳 请求帧--充电状态下终端向服务器发送心跳报文，发起方：充电终端---以使服务器确定终端处理联机状态；终端在响应报文中下发当前可用的最大输出功率
typedef	struct _ISO8583_AP_Heart_Charging_Struct
{
	unsigned char charge_way[2];  //充电方式（01：刷卡，02：手机）	3	Y	N2
	unsigned char card_sn[16];//充电卡号或账号	4	Y	N16
	unsigned char control_type[3];     //控制类型	5	N	N3	控制类型见下表对应数据--对应的是自动还是金额电量时间等。
	unsigned char charge_limt_data[8];     //开启限制数据	6	N	N8	自动充满停机方式下无意义。电量控制,单位为“度”，精确到0.01,传输数据放大100倍；时间控制，单位为“秒“，精确到1,；金额控制，单位为“元”，精确到0.01，传输数据放大100倍 结合控制内型。
	unsigned char appointment_start_time[14];//预约开始时间（格式yyyyMMddHHmmss）	7	Y	N14
  unsigned char appointment_end_time[14];//预约结束时间（格式yyyyMMddHHmmss）	8	Y	N14
  unsigned char stop_car_time[14];//停车费开始时间（格式yyyyMMddHHmmss）9 Y	N14

	unsigned char Busy_SN[ISO8583_BUSY_SN];//业务流水号	11	 N	N20
	unsigned char type[2];     //充电桩类型（01：单枪直流充电桩，02单相交流充电桩，03三相交流充电桩，04：双枪轮流充直流充电桩，05：双枪同时充直流充电桩）33	N	   N2
	unsigned char data_len[3]; //后面数据长度域 Ans…999 所以长度域 3字节 0x30 0x39 0x30
	unsigned char data[172];    //充电桩数据	35	N	   Ans…999   --双枪同时充--90+78个字节。
	unsigned char rmb[8];       //电量金额（以分为单位）	36	N	n8
	unsigned char serv_rmb[8];  //服务费金额	37	N	N8
  unsigned char gun_NO[2];  //服务费金额	38	N	N8

	unsigned char ter_sn[12];  //充电终端序列号    41	N	n12
	unsigned char time[14];    //终端时间（格式yyyyMMddHHmmss）	62	N	N14
	unsigned char MD5[16];     //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_AP_Heart_Charging_Frame;

//心跳 请求帧--非充电状态下终端向服务器发送心跳报文，发起方：充电终端---以使服务器确定终端处理联机状态；终端在响应报文中下发当前可用的最大输出功率
typedef	struct _ISO8583_AP_Heart_NoCharging_Struct
{
	unsigned char type[2];     //充电桩类型（01：单枪直流充电桩，02单相交流充电桩，03三相交流充电桩，04：双枪轮流充直流充电桩，05：双枪同时充直流充电桩）33	N	   N2
	unsigned char data_len[3]; //后面数据长度域 Ans…999 所以长度域 3字节 0x30 0x39 0x30
	unsigned char data[172];    //充电桩数据	35	N	   Ans…999   --双枪同时充--90+78个字节。
	unsigned char rmb[8];       //电量金额（以分为单位）	36	N	n8
	unsigned char serv_rmb[8];  //服务费金额	37	N	N8
  unsigned char gun_NO[2];  //服务费金额	38	N	N8

	unsigned char ter_sn[12];  //充电终端序列号    41	N	n12
	unsigned char time[14];    //终端时间（格式yyyyMMddHHmmss）	62	N	N14
	unsigned char MD5[16];     //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_AP_Heart_NoCharging_Frame;

//心跳 响应数据帧
typedef	struct _ISO8583_AP_Heart_Resp_Struct
{
	unsigned char flag[2];       //信息处理结果标识（00：成功；01：失败）39	N	An2

	unsigned char power_a[4];    //高4位-平均功率 电桩离线时使用	44	N	n8 输出功率（kW）
	unsigned char power_h[4];    //低4位-最大功率 电桩在线时使用	44	N	n8 输出功率（kW）
	unsigned char Software_Version[5];  //保留	  52	N	N20 15-20
	unsigned char APP_ver[5];    //APP 版本号     52	N	N20 11-15
	unsigned char price_ver[5];  //价格表 版本号  52	N	N20 6-10
	unsigned char inval_ver[5];  //黑名单 版本号	52	N	N20 1-5
	unsigned char work_Enable[2];//充电桩工作使能
	unsigned char time[14];      //服务器时间（格式yyyyMMddHHmmss）	61	N	n14
	unsigned char MD5[16];       //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_AP_Heart_Resp_Frame;

//获取流水号 命令数据帧 ---充电桩上传充电业务数据或营业网点上传充值业务数据时向服务器发起请求
typedef	struct _ISO8583_AP_Get_SN_Struct
{ 
	unsigned char flag[2];    //序列号类型（01：充值02：充电消费）  40 n2 
	unsigned char ter_sn[12];   //充电终端序列号    41	N	n12
	unsigned char time[14];     //终端时间（格式yyyyMMddHHmmss）	62	N	N14
	unsigned char MD5[16];      //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_AP_Get_SN_Frame;
//获取流水号 响应数据帧
typedef	struct _ISO8583_AP_Get_SN_Resp_Struct
{ 
	unsigned char Busy_SN[ISO8583_BUSY_SN]; //业务流水号 11	N	n20
	unsigned char flag[2];                  //信息处理结果标识（00：成功；01：失败）39	N	An2
	unsigned char MD5[16];      //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_AP_Get_SN_Resp_Frame;


// 响应数据帧 结果为正常或异常
typedef	struct _ISO8583_AP_Resp_Struct
{ 
	unsigned char flag[2];  //信息处理结果标识（00：成功；01：失败）39	N	An2
	unsigned char MD5[16];  //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_AP_Resp_Frame;

//充电数据业务数据上报数据帧--发起方：充电终端 充电桩在完成一笔充电业务后，主动将充电业务数据上报给中心服务器
typedef	struct _ISO8583_AP_charg_busy_Struct
{
	unsigned char card_sn[16];     //用户卡号	2	N	n16
	unsigned char flag[2];         //交易结果(00：扣费成功；01：灰锁，02：补充交易) 3	N	N2
	unsigned char rmb[8];          //充电总金额（以分为单位如果为内部车辆，则为0）	4	N	n8
	unsigned char kwh[8];          //充电总电量（度*100，如10度电用1000表示）	5	N	n8

	unsigned char server_rmb[8];   //服务费（以分为单位）	6	N	N8
	unsigned char total_rmb[8];    //总费用（充电总金额 +服务费）	7	N	N8
	unsigned char type[2];         //充电方式（01：刷卡，02：手机）	8	N	N2
	unsigned char Serv_Type[2];    //服务费收费类型 01：按次收费；02：按时间收费(单位：分/10分钟；03：按电量	9	N	N2

	unsigned char Busy_SN[ISO8583_BUSY_SN]; //业务流水号 11	N	n20
	unsigned char last_rmb[8];     //充电前卡余额（以分为单位）	30	N	n8

	unsigned char ter_sn[12];      //充电终端序列号    41	N	n12
		
	unsigned char sharp_rmb_kwh_Serv[8];		//尖电价单位分 42 服务费
	unsigned char sharp_allrmb_Serv[8];		//对应金额
	unsigned char peak_rmb_kwh_Serv[8];		//尖电价单位分43
	unsigned char peak_allrmb_Serv[8];		//峰电价单位分+ 对应金额
	unsigned char flat_rmb_kwh_Serv[8];		//尖电价单位分44
	unsigned char flat_allrmb_Serv[8];		//平电价单位分+ 对应金额 服务费
	unsigned char valley_rmb_kwh_Serv[8];		//尖电价单位分45
	unsigned char valley_allrmb_Serv[8];		//谷电价单位分+ 对应金额 _Serv
	
	unsigned char start_kwh[8];    //开始电表度数	46	N	N8
	unsigned char end_kwh[8];      //结束电表度数	47	N	N8
	unsigned char rmb_kwh[8];      //平均电价（单位：分/度）	48	N	n8
	
	unsigned char sharp_rmb_kwh[8];		//尖电价单位分 49
	unsigned char sharp_allrmb[8];		//对应金额
	unsigned char peak_rmb_kwh[8];		//尖电价单位分50
	unsigned char peak_allrmb[8];		//峰电价单位分+ 对应金额
	unsigned char flat_rmb_kwh[8];		//尖电价单位分51
	unsigned char flat_allrmb[8];		//平电价单位分+ 对应金额
	unsigned char valley_rmb_kwh[8];		//尖电价单位分52
	unsigned char valley_allrmb[8];		//谷电价单位分+ 对应金额
	
	unsigned char ConnectorId[2];     //充电枪号	54	N	n2
	unsigned char End_code[2];       //充电结束原因代码	55	N	n2

	unsigned char end_time[14];     //充电结束时间（格式yyyyMMddHHmmss）	59	N	n14

	unsigned char car_sn[17];       //充电车辆标识 60	Y	Ans17
	unsigned char start_time[14];   //充电开始时间（格式yyyyMMddHHmmss）	61	N	n14

	unsigned char time[14];         //终端时间（格式yyyyMMddHHmmss）	62	N	N14
	unsigned char MD5[16];          //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_AP_charg_busy_Frame;


//请求黑名单 命令数据帧 --- 发起方：充电终端
typedef	struct _ISO8583_AP_Get_INVAL_CARD_Struct
{ 
	unsigned char sn[3];        //请求数据包序列号	11	N	n3
	unsigned char ter_sn[12];   //充电终端序列号    41	N	n12
	unsigned char ver[5];       //黑名单数据版本号	52	N	N5
	unsigned char time[14];     //终端时间（格式yyyyMMddHHmmss）	62	N	N14
	unsigned char MD5[16];      //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_AP_Get_INVAL_CARD_Frame;

//请求黑名单 响应数据帧
typedef	struct _ISO8583_AP_Get_INVAL_CARD_Resp_Struct
{ 
	unsigned char now_sn[3];    //当前数据包序号	23	Y	n3
	unsigned char flag[2];      //信息处理结果标识（00：成功；01：版本号已修改） 39	N	An2
	unsigned char tot_sn[3];    //数据包总数量	40	Y	n3
	unsigned char data_len[3];  //后面数据长度域 Ans…480 所以长度域 3字节 0x31 0x37 0x32
	unsigned char data[480];    //黑名单卡号列表	61	Y	Ans..480
	unsigned char MD5[16];       //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_AP_Get_INVAL_CARD_Resp_Frame;

//电价数据请求接口 命令数据帧
typedef	struct _ISO8583_AP_Get_Price_Struct
{ 
	unsigned char ter_sn[12];   //充电终端序列号 41    41	N	n12
	unsigned char price_type[2];//获取定时电价00:表示获取当前电价，01：表示获取定时电价 42	N	N2
	unsigned char time[14];     //终端时间（格式yyyyMMddHHmmss）	62	N	N14
	unsigned char MD5[16];      //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_AP_Get_Price_Frame;

//电价数据请求接口 响应数据帧
typedef	struct _ISO8583_AP_Get_Price_Resp_Struct
{ 
	unsigned char serv_type[2];    //服务费收费类型1：按次收费；2：按时间收费(单位：分/10分钟；3：按电量	4	N	N2
	unsigned char serv_rmb[8];     //服务费(单位：分)	5	N	N8
	unsigned char aver_rmb[4];     //非分时电价（单位：分）	6	N	N4
	unsigned char step_rmb[16];    //分时电价（4个电价，4字符电价，单位：分）	7	N	N16
	unsigned char step[2];         //时段数	8	N	N2
	unsigned char timezone[12][5];   //时段+对应电价（4字符时段+1字符电价）*n个时段	9	N	N60
	unsigned char  stop_rmb[8];//停车费单价（元/10分钟）	11 N8
  unsigned char  stop_rmb_start[2];//充电期间是否免停车费(01：表示免费，02：不免费)	12 N2
  unsigned char  stop_time_free[8];// 充完电后免停车费的时长(单位：分钟)	13             N8
	
	unsigned char step_server_price[16];    //分时服务电价（4个电价，4字符电价，单位：分）	14	N	N16
	unsigned char show_price_type[2];    //分00：表示显示详细单价，01：表示只显示一个总单价	15	N	N2
	unsigned char IS_fixed_price[2];    //是否有定时电价00：表示没有，01：表示有。如果有定时电价，需要再次请求电价信息	16	N	N2
	//unsigned char fixed_price_effective_time[14];    //定时电价生效时间（格式yyyyMMddHHmmss）17	Y	N14

	unsigned char flag[2];      //信息处理结果标识（00：成功；01：版本号已修改） 39	N	An2
	unsigned char MD5[16];       //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_AP_Get_Price_Resp_Frame;

//电价数据请求接口 响应数据帧--定时电价
typedef	struct _ISO8583_AP_Get_fixedPrice_Resp_Struct
{ 
	unsigned char serv_type[2];    //服务费收费类型1：按次收费；2：按时间收费(单位：分/10分钟；3：按电量	4	N	N2
	unsigned char serv_rmb[8];     //服务费(单位：分)	5	N	N8
	unsigned char aver_rmb[4];     //非分时电价（单位：分）	6	N	N4
	unsigned char step_rmb[16];    //分时电价（4个电价，4字符电价，单位：分）	7	N	N16
	unsigned char step[2];         //时段数	8	N	N2
	unsigned char timezone[12][5];   //时段+对应电价（4字符时段+1字符电价）*n个时段	9	N	N60
	unsigned char  stop_rmb[8];//停车费单价（元/10分钟）	11 N8
  unsigned char  stop_rmb_start[2];//充电期间是否免停车费(01：表示免费，02：不免费)	12 N2
  unsigned char  stop_time_free[8];// 充完电后免停车费的时长(单位：分钟)	13             N8
	
	unsigned char step_server_price[16];    //分时服务电价（4个电价，4字符电价，单位：分）	14	N	N16
	unsigned char show_price_type[2];    //分00：表示显示详细单价，01：表示只显示一个总单价	15	N	N2
	unsigned char IS_fixed_price[2];    //是否有定时电价00：表示没有，01：表示有。如果有定时电价，需要再次请求电价信息	16	N	N2
	unsigned char fixed_price_effective_time[14];    //定时电价生效时间（格式yyyyMMddHHmmss）17	Y	N14

	unsigned char flag[2];      //信息处理结果标识（00：成功；01：版本号已修改） 39	N	An2
	unsigned char MD5[16];       //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_AP_Get_fixedPrice_Resp_Frame;

//用户卡解锁请求接口 命令数据帧
typedef	struct _ISO8583_User_Card_Unlock_Request_Struct
{ 
  unsigned char card_sn[16];        //用户卡号	2	N	n16
	unsigned char lockstart_time[14];   //锁卡时终端时间    12	N	n14
	unsigned char data_len[2];  //后面数据长度域 Ans…99 所以长度域 2字节 
	unsigned char ter_sn[12];    //充电桩序列号	42	N	N12
	unsigned char time[14];     //终端时间（格式yyyyMMddHHmmss）	62	N	N14
	unsigned char MD5[16];      //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_User_Card_Unlock_Request_Frame;

//用户卡解锁请求接口 响应数据帧
typedef	struct _ISO8583_User_Card_Unlock_Request_Resp_Struct
{ 
  unsigned char total_rmb[8];    //总费用（充电总金额 +服务费）	4	N	N8
	unsigned char Busy_SN[ISO8583_BUSY_SN]; //业务流水号 11	N	n20	
	unsigned char flag[2];      //信息处理结果标识（00：成功；01：版本号已修改） 39	N	An2
	unsigned char MD5[16];       //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_User_Card_Unlock_Request_Resp_Frame;

//用户卡解锁请求接口 响应数据帧
typedef	struct _ISO8583_User_Card_Unlock_Request_Resp111_Struct
{ 
	unsigned char flag[2];      //信息处理结果标识（00：成功；01：版本号已修改） 39	N	An2
	unsigned char MD5[16];       //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_User_Card_Unlock_Request_Resp111_Frame;


//用户卡解锁数据上报接口 命令数据帧
typedef	struct _ISO8583_User_Card_Unlock_Data_Request_Struct
{ 
  unsigned char card_sn[16];        //用户卡号	2	N	n16
	unsigned char Busy_SN[ISO8583_BUSY_SN]; //业务流水号 11	N	n20	
	unsigned char unlock_time[14];   //锁卡时终端时间    12	N	n14
	unsigned char data_len[2];  //后面数据长度域 Ans…99 所以长度域 2字节 
	unsigned char ter_sn[12];      //充电桩序列号	42	N	N12
	unsigned char time[14];     //终端时间（格式yyyyMMddHHmmss）	62	N	N14
	unsigned char MD5[16];      //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_User_Card_Unlock_Data_Request_Frame;

//用户卡解锁数据上报接口 响应数据帧
typedef	struct _ISO8583_User_Card_Unlock_Data_Request_Resp_Struct
{ 
	unsigned char flag[2];      //信息处理结果标识（00：成功；01：版本号已修改） 39	N	An2
}ISO8583_User_Card_Unlock_Data_Request_Resp_Frame;
//APP开启、停止充电接口 请求数据帧 --- 正常启动
typedef	struct _ISO8583_AP_APP_Contol_Struct
{ 
	unsigned char start[2];   //操作类型	4	N	N2	01：启动，02：停止
	unsigned char num[2];     //充电口号	5	N	N2	01表示A口，02表示B口
	unsigned char type[3];     //控制类型	6	N	N3	控制类型见下表对应数据。
	unsigned char data[8];     //开启限制数据	7	N	N8	自动充满停机方式下无意义。电量控制,单位为“度”，精确到0.01,传输数据放大100倍；时间控制，单位为“秒“，精确到1,；金额控制，单位为“元”，精确到0.01，传输数据放大100倍。
	unsigned char time[8];     //定时启动	8	N	N8	单位为“秒“

	unsigned char ID[16];      //用户标识	9	N	N16	
	unsigned char last_rmb[8]; //账号余额	10	N	N8	单位为“分“
	unsigned char Busy_SN[ISO8583_BUSY_SN]; //交易流水号	11	N	N20
	unsigned char BMS_Power_V[2]; // 12//12/24V  2.20才有的
  unsigned char APP_Start_Account_type[2];//23 app启动账户类型用户类型：00：表示本系统用户，01：表示第三方系统用户
	unsigned char ter_sn[12];   //充电桩序列号	41	N	N12

	unsigned char MD5[16];       //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_AP_APP_Contol_Frame;

//APP开启、停止充电接口 请求数据帧 --- 正常启动--带私有电价的
typedef	struct _ISO8583_AP_APP_Contol_Private_Struct
{ 
	unsigned char start[2];   //操作类型	4	N	N2	01：启动，02：停止
	unsigned char num[2];     //充电口号	5	N	N2	01表示A口，02表示B口
	unsigned char type[3];     //控制类型	6	N	N3	控制类型见下表对应数据。
	unsigned char data[8];     //开启限制数据	7	N	N8	自动充满停机方式下无意义。电量控制,单位为“度”，精确到0.01,传输数据放大100倍；时间控制，单位为“秒“，精确到1,；金额控制，单位为“元”，精确到0.01，传输数据放大100倍。
	unsigned char time[8];     //定时启动	8	N	N8	单位为“秒“

	unsigned char ID[16];      //用户标识	9	N	N16	
	unsigned char last_rmb[8]; //账号余额	10	N	N8	单位为“分“
	unsigned char Busy_SN[ISO8583_BUSY_SN]; //交易流水号	11	N	N20
	unsigned char BMS_Power_V[2]; // 12//12/24V  2.20才有的
	
  unsigned char serv_type[2];    //13服务费收费类型1：按次收费；2：按时间收费(单位：分/10分钟；3：按电量	4	N	N2
	unsigned char serv_rmb[8];     //14服务费(单位：分)	5	N	N8
	unsigned char aver_rmb[4];     //15非分时电价（单位：分）	6	N	N4
	unsigned char step_rmb[16];    //16分时电价（4个电价，4字符电价，单位：分）	7	N	N16
	unsigned char step[2];         //17时段数	8	N	N2
	unsigned char timezone[12][5];   //18时段+对应电价（4字符时段+1字符电价）*n个时段	9	N	N60
	unsigned char  stop_rmb[8];//19停车费单价（元/10分钟）	11 N8
  unsigned char  stop_rmb_start[2];//20充电期间是否免停车费(01：表示免费，02：不免费)	12 N2
  unsigned char  stop_time_free[8];// 21充完电后免停车费的时长(单位：分钟)	13             N8
	unsigned char step_server_price[16];  //22分时服务电价（4个电价，4字符电价，单位：分）	14	N	N16
	unsigned char   APP_Start_Account_type[2];//23 app启动账户类型用户类型：00：表示本系统用户，01：表示第三方系统用户
	unsigned char ter_sn[12];   //充电桩序列号	41	N	N12
	unsigned char MD5[16];       //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_AP_APP_Contol_Private_Frame;

//APP开启、停止充电接口 响应数据帧
typedef	struct _ISO8583_AP_APP_Contol_Resp_Struct
{ 
	unsigned char ID[16];      //用户标识	9	N	N16	
	unsigned char flag[2];      //信息处理结果标识 39	N	An2 （00：成功；01：MAC校验失败，02：充电桩序列号不对应，03：未插枪，04：充电桩已被使用，无法充电，05：非当前用户，无法取消充电, 06：充电桩故障无法充电,07:充电桩忙, 08:余额不足
	unsigned char ter_sn[12];   //充电桩序列号	41	N	N12

	unsigned char MD5[16];       //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_AP_APP_Contol_Resp_Frame;


//在线重启
typedef	struct _ISO8583_3040_Struct
{ 
	unsigned char reboot_flag[2];        //请求数据包序列号	11	N	n3
	unsigned char ter_sn[12];   //充电终端序列号    41	N	n12
	unsigned char MD5[16];      //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_3040_Frame;

//桩回应在线重启机制
typedef	struct _ISO8583_3041_Resp_Struct
{ 
	unsigned char flag[2];      //信息处理结果标识（00：成功；01：版本号已修改） 39	N	An2
	unsigned char ter_sn[12];   //充电终端序列号    41	N	n12
	unsigned char MD5[16];       //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_3041_Resp_Frame;


#define MAX_ALARM_NO	36
typedef	struct _ISO8583_alarm_Frame
{	
	unsigned char charger_type[2];       //  充电桩类型（01：单枪直流充电桩，02单相交流充电桩，03三相交流充电桩，04：双枪轮流充直流充电桩，05：双枪同时充直流充电桩，06：单相交流双枪充电桩，07：三相交流双枪充电桩）
	unsigned char charger_alarm_num[3];             //告警数目---'0' '0' '5'  34
  unsigned char charger_data[MAX_ALARM_NO*12+12+40];//charger_data[MAX_ALARM_NO*12+12];
}ISO8583_alarm_Frame;//告警帧中的单相数据信息体


typedef	struct _SINGLE_Alarm_Info
{
	unsigned char charger_alarm_num;//告警数目不超过MAX_ALARM_NO

	unsigned char alarm_id_h[MAX_ALARM_NO][5];//告警标识（信号ID高5位）*75	35	N	   Ans…375
	unsigned char alarm_id_l[MAX_ALARM_NO][3];//同类信号的顺序号（信号ID低3位）*75	36	N	Ans…225
	
	unsigned char alarm_type[MAX_ALARM_NO][2];//信号类型（01：告警，02：事件日志）*75 37	N	  Ans…150
	
	unsigned char alarm_sts[MAX_ALARM_NO][2];//告警动作（01：发生，02：消失）*75 38	N	  Ans…150

}Alarm_Info_Get;//告警上传帧实时信息

typedef	struct _ISO8583_Rcev_Alarm_Frame
{	
	unsigned char result[2];      //心跳响应结果 00：成功，01：MAC错误 02：终端未对时  03: 无效的充电桩序列号，04：充电桩类型错误，05：充电桩数据长度非偶数
	unsigned char mac[16];//前面各个域的MAC，取MD5结果的单字节作DES加密  63 N 16	
}ISO8583_Rcev_Alarm_Frame;//告警帧的响应帧信息体
typedef	struct _ISO8583_APUPDATE
{	
	char data_block_num[8];     //请求获取的数据包号	从1开始	
	unsigned char charger_sn[12];  //充电桩序列号
	unsigned char chg_soft_ver_get[5];//请求的版本号
	char time[14];//终端时间（格式yyyyMMddHHmmss）	62	N	n14
	unsigned char MD5[16];    //前面多个成员经MD5计算后取单字节再做DES后的结果	63	N	n16
}ISO8583_APUPDATE_Frame;//桩发送获取升级固件数据包信息体

typedef	struct _ISO8583_APUPDATEResp_Frame
{	
	unsigned char  curent_data_block_num[8];     //当前数据包序号 23 N8
	unsigned char  flag[2];     //信息处理结果标识（00：成功；01：MAC校验失败；02：终端未对时，03：充电桩序列号无效，04：版本号未找到，05：包序号未找到）  39 N2
	unsigned char  all_data_num[8];     //数据包总数量  40 N8
	unsigned char data_len[3];  //后面数据长度域 Ans…999 所以长度域 3字节 
	unsigned char data[640];  //数据量 41 N Ans…640
	unsigned char MD5[16];    //前面多个成员经MD5计算后取单字节再做DES后的结果	63	N	n16
}ISO8583_APUPDATE_Resp_Frame;//桩发送获取升级固件数据包信息体

//桩升级确认
typedef	struct
{	
	unsigned char charger_sn[12];  //充电桩序列号
	unsigned char chg_soft_ver[5];//当前的软件版本号
	char time[14];//终端时间（格式yyyyMMddHHmmss）	62	N	n14	
	unsigned char MD5[16];    //前面多个成员经MD5计算后取单字节再做DES后的结果	63	N	n16
}ISO8583_AP1070_Frame;//桩发送软件更新确认帧信息体

//卡片校验上传
typedef	struct _ISO8583_User_Card_Struct
{ 
	unsigned char User_Card[16];      //用户卡号	2	N	N16	
	unsigned char ter_sn[12];   //充电桩序列号	42	N	N12
	
	unsigned char qiyehao_flag[2];//企业号标记，域43
	unsigned char private_price_flag[2];//私有电价标记，域44
	unsigned char order_chg_flag[2];//有序充电用户卡标记，域45
	
	unsigned char time[14];     //定时启动	62	N	N14	单位为“秒“
	unsigned char MD5[16];       //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_User_Card_Frame;

typedef	struct
{ 
	unsigned char User_Card[16];      //用户卡号全0	2	N	N16	
	unsigned char ter_sn[12];   //充电桩序列号	42	N	N12
	unsigned char VIN[17];       //VIN码
	unsigned char time[14];     
	unsigned char MD5[16];       //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_VIN_Auth_Frame;

//卡片校验回应 1
typedef	struct _ISO8583_User_Card_Resp_Struct
{  
  unsigned char bind_bmsvin[17];     //15车辆识别码
	unsigned char flag[2];    //信息处理结果标识 39	N	An2 （00：成功；01：MAC校验出错；02：终端未对时，03: 该卡未进行发卡;04:无效的充电桩序列号;05:黑名单
	unsigned char MD5[16];       //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_User_Card_Resp_Frame;
//卡片校验回应 2

typedef	struct _ISO8583_AP1120_Resp_full_Frame
{
	unsigned char serv_type[2];       //服务费收取方式 4	N	n2
	unsigned char serv_rmb[8];     //服务费 5	N	n8
	unsigned char aver_rmb[4];        //非分时电价 单位分
	unsigned char kwh_price1[4];		//尖电价单位分
	unsigned char kwh_price2[4];		//峰电价单位分
	unsigned char kwh_price3[4];		//平电价单位分
	unsigned char kwh_price4[4];		//谷电价单位分
	unsigned char timezone_num[2];      //时段数
	unsigned char timezone[12][5];  //时段表	9	N	n60
	
	unsigned char park_price[8];     //停车费 11	N	n8
	
	unsigned char park_free_when_chg[2];     //充电期间是否免停车费 12	N	n2
	unsigned char park_free_time_after_chg[8]; //充电完成后多长时间免停车费 13	N	n8
	
	unsigned char kwh_service_price1[4];		//尖时段服务费单位分 
	unsigned char kwh_service_price2[4];		//峰时段服务费单位分
	unsigned char kwh_service_price3[4];		//平时段服务费单位分
	unsigned char kwh_service_price4[4];		//谷时段服务费单位分
	
	unsigned char bind_bmsvin[17];     //车辆识别码
	unsigned char order_card_end_time[4];       //20当前有序充电卡允许充电的终止时刻 HH:MM
	unsigned char resp_status[2];//2字节的响应状态码
	unsigned char MD5[16];    //前面多个成员经MD5计算后取单字节再做DES后的结果	63	N	n16
}ISO8583_AP1120_Resp_full_Frame;//电价数据响应帧信息体

//卡片校验回应 3
typedef	struct
{	
  unsigned char bind_bmsvin[17];     //车辆识别码
	unsigned char order_card_end_time[4];       //当前有序充电卡允许充电的终止时刻 HH:MM
	unsigned char flag[2];//2字节的响应状态码
	unsigned char MD5[16];    //前面多个成员经MD5计算后取单字节再做DES后的结果	63	N	n16
}ISO8583_AP1120_Resp2_Frame;

typedef	struct _ISO8583_AP1130_Frame
{	
	unsigned char User_Card[16];		//用户ID	 2 N 
	unsigned char plug_id[2];       //枪号 01枪1  02枪2    38 N2
	unsigned char ter_sn[12];     //充电桩序列号 42	N	n..12	
	unsigned char time[14];//终端时间（格式yyyyMMddHHmmss）	62	N	n14
	unsigned char MD5[16];//前面各个域的MAC，取MD5结果的单字节作DES加密  63 N 16
}ISO8583_AP1130_Frame;//企业卡刷卡充电成功后上报


//4.6.5 蓄电池参数上送接口  命令数据帧上发
typedef struct _ISO8583_Battery_Parameter_Request_Struct
{
	unsigned char charge_id[21];  //充电标识号(序列号+yymmdd(年月日)+三位自增数字)	31	N	N21
	unsigned char ter_sn[12];	  //充电桩序列号	32	N	N12
	unsigned char type[2];		  //充电桩类型（01：单枪直流充电桩，02单相交流充电桩，03三相交流充电桩，04：双枪轮流充直流充电桩，05：双枪同时充直流充电桩,06：单相交流双枪充电桩，07：三相交流双枪充电桩）	33	N	   N2
	unsigned char battery_type[2];//蓄电池类型			 34	N	    N2
	unsigned char rated_AH[8];	  //蓄电池组额定容量(Ah) 35	N		N8
	unsigned char battery_group_total_vol[8];//蓄电池组额定总电压	36	N	  N8  0.001
	unsigned char max_voltage[8];			 //最高允许充电总电压	37	N	  N8        0.001
	unsigned char max_current[8];			 //最高允许充电电流		38	N	  N8        0.001
	unsigned char battery_cell_max_vol[8];   //单体蓄电池允许最高充电电压	39	N	N8 0.01
	unsigned char battery_max_T[8];//蓄电池最高允许温度	40	N	N8
	unsigned char battery_kwh[8];  //电池额定电量（Kwh）41	N	N8
	unsigned char muzzle_num[2];   //枪口号	42	N	N2
	unsigned char MD5[16];		   //Mac	63	N	N16	 前面多个成员经MD5计算后取单字节再做DES后的结果
}ISO8583_Battery_Parameter_Request_Frame;

//4.6.5 蓄电池参数上送接口  响应数据帧处理
typedef struct _ISO8583_Battery_Parameter_Respone_Struct
{
	unsigned char flag[2]; //信息处理结果标识（00：成功；01：MAC校验失败; 02：无效的充电桩序列号） 39	N	An2
	unsigned char MD5[16]; //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_Battery_Parameter_Respone_Frame;

//4.6.6 BMS实时数据上送接口 命令数据帧上发
typedef struct _ISO8583_BMS_Real_Data_Request_Struct
{
	unsigned char charge_id[21];//充电标识号(序列号+yymmdd(年月日)+三位自增数字)	31	N	N21
	unsigned char ter_sn[12];   //充电桩序列号	32	N	N12
	unsigned char type[2];	  	//充电桩类型（01：单枪直流充电桩，02单相交流充电桩，03三相交流充电桩，04：双枪轮流充直流充电桩，05：双枪同时充直流充电桩,06：单相交流双枪充电桩，07：三相交流双枪充电桩）	33	N	   N2
	unsigned char charge_vol_require[8];//充电电压需求值	34	N	N8  0.001
	unsigned char charge_cur_require[8];//充电电流需求值	35	N	N8  0.001
	unsigned char charge_vol_measure[8];//充电电压测量值	36	N	N8  0.001
	unsigned char charge_cur_measure[8];//充电电流测量值	37	N	N8  0.001
	unsigned char battery_cell_H_Vol[8];//单体电池最高电压38	N	N8  0.01
	unsigned char soc[2];			//当前荷电状态(电池SOC)	39	N	N2 
	unsigned char battery_H_T[8];	//最高蓄电池温度	40	N	N8
	unsigned char battery_L_T[8];	//最低蓄电池温度	41	N	N8
	unsigned char car_sn[17];		//车辆识别码 	42	N	N17
	unsigned char battery_cell_H_VOL_NO[3];//BMS单体电池最高电压序号	43	N	N3
	unsigned char battery_cell_H_T_NO[3];  //BMS单体电池最高温度序号	44	N	N3
	unsigned char battery_cell_L_T_NO[3];  //BMS单体电池最低温度序号	45	N	N3
	unsigned char gun_T[8];		//充电枪温度 46	N	N8
	unsigned char muzzle_NO[2];//枪口号 47	N	N2
	unsigned char MD5[16];		//Mac	 63	N	N16
}ISO8583_BMS_Real_Data_Request_Frame;
//4.6.6 BMS实时数据上送接口 响应数据帧处理
typedef struct _ISO8583_BMS_Real_Data_Respone_Struct
{
	unsigned char flag[2]; //信息处理结果标识（00：成功；01：MAC校验失败; 02：无效的充电桩序列号） 39	N	An2
	unsigned char MD5[16]; //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_BMS_Real_Data_Respone_Frame;

typedef struct _ISO8583_3050_Read_Battery_Frame
{
	unsigned char ter_sn[12];   //充电桩序列号	41	N	N12
	unsigned char gunno[2]; //枪号unsigned char 
	unsigned char MD5[16]; //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_3050_Read_Battery_Frame;

typedef struct _ISO8583_3051_Send_Battery_Frame
{
	unsigned char flag[2]; //信息处理结果标识（00：成功；01：MAC校验失败; 02：无效的充电桩序列号） 39	N	An2
	unsigned char ter_sn[12];   //充电桩序列号	41	N	N12
	unsigned char Now_Khw[8];//电表读数   	46	N	N8
	unsigned char MD5[16]; //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_3051_Send_Battery_Frame;

//4.5.3地锁开关接口 命令数据帧下发
typedef struct _ISO8583_Car_lock_Data_Request_Struct
{
	unsigned char Cmd_Type[2];//操作类型	4	N	N2	01：关，02：开 
	unsigned char Car_lock_NO[2];   //地锁号	5	N	N2	01表示地锁1，02表示地锁2
	unsigned char User_Card[16];	  	//用户标识	9	N	N16		N	   N2
	unsigned char ter_sn[12];//充电桩序列号	41	N	N12
	unsigned char MD5[16];		//Mac	 63	N	N16
}ISO8583_Car_lock_Data_Request_Frame;

//4.5.3地锁开关接口 响应数据帧处理
typedef struct _ISO8583_Car_lock_Data_Respone_Struct
{
	unsigned char User_Card[16];		//用户ID	 9 16N 
	unsigned char falg[2]; //（00：成功；01：MAC校验失败，02：充电桩序列号不对应，03：充电桩已被使用，无法控制地锁，04：地锁通信故障，）05：操作的地锁不存在，06：有车不能升起地锁，07：非自己使用的车位锁，08：只能下放一个地锁
	unsigned char ter_sn[12];     //充电桩序列号 41	N	n..12	
	unsigned char MD5[16]; //（00：成功；01：MAC校验失败，02：充电桩序列号不对应，03：充电桩已被使用，无法控制地锁，04：地锁通信故障，）05：操作的地锁不存在，06：有车不能升起地锁，07：非自己使用的车位锁，08：只能下放一个地锁
}ISO8583_Car_lock_Data_Respone_Frame;

//4.2.11停车费业务数据上报接口  1110
typedef struct _ISO8583_Car_Stop_busy_Struct
{
	unsigned char User_Card[16];		//用户ID	 2 16N 
	unsigned char falg[2]; // 3 02 交易结果(00：扣费成功；01：灰锁，02：补充交易)
	unsigned char charge_type[2]; // 充电方式（01：刷卡，02：手机）
	unsigned char charge_BSN[20]; // 11业务流水号;
	unsigned char stop_car_allrmb[8]; // 停车费（以分为单位）;
	unsigned char stop_car_start_time[14]; // 停车费（以分为单位）;
	unsigned char stop_car_end_time[14]; // 停车费（以分为单位）;
	unsigned char stop_car_rmb_price[8]; // 停车费（元/10分钟);
	unsigned char charge_last_rmb[8]; // 停车费（元/10分钟);
	unsigned char ter_sn[12];     //充电桩序列号 41	N	n..12	
	unsigned char time[14];     //62定时启动	62	N	N14	单位为“秒“
	unsigned char MD5[16]; //63（00：成功；01：MAC校验失败，02：充电桩序列号不对应，03：充电桩已被使用，无法控制地锁，04：地锁通信故障，）05：操作的地锁不存在，06：有车不能升起地锁，07：非自己使用的车位锁，08：只能下放一个地锁
}ISO8583_Car_Stop_busy_Struct;
// 响应数据帧 结果为正常或异常
typedef	struct _ISO8583_Car_Stop_Resp_Struct
{ 
	unsigned char flag[2];  //信息处理结果标识（00：成功；01：失败）39	N	An2
	unsigned char MD5[16];  //MD5摘要（取其中的单字节加密） 63	N	N16
}ISO8583_Car_Stop_Resp_Frame;

//灯箱控制
typedef struct _ISO8583_Light3060_Data_Struct
{
	 unsigned char  Light_Control_time[8]; //启动开始时间 时/分 结束时间时/分
	 unsigned char  ter_sn[12];     //充电桩序列号 41	N	n..12	
	 unsigned char  MD5[16]; //（00：成功；01：MAC校验失败，02：充电桩序列号不对应，03：充电桩已被使用，无法控制地锁，04：地锁通信故障，）05：操作的地锁不存在，06：有车不能升起地锁，07：非自己使用的车位锁，08：只能下放一个地锁
}ISO8583_Light3060_Data_Frame;

typedef struct _ISO8583_Light3061_Data_Respone_Struct
{
	unsigned char falg[2]; //（00：成功；01：MAC校验失败，02：充电桩序列号不对应，03：充电桩已被使用，无法控制地锁，04：地锁通信故障，）05：操作的地锁不存在，06：有车不能升起地锁，07：非自己使用的车位锁，08：只能下放一个地锁
	unsigned char ter_sn[12];     //充电桩序列号 41	N	n..12	
	unsigned char MD5[16]; //（00：成功；01：MAC校验失败，02：充电桩序列号不对应，03：充电桩已被使用，无法控制地锁，04：地锁通信故障，）05：操作的地锁不存在，06：有车不能升起地锁，07：非自己使用的车位锁，08：只能下放一个地锁
}ISO8583_Light3061_Data_Respone_Frame;



typedef	struct _ISO8583_APP_Struct
{
	int             Heart_TCP_state;
	int             Heart_Socket_fd;
	unsigned char   random[32];      //随机字符串
	unsigned char   work_key[8];     //工作密钥
	int            ISO8583_Random_Rcev_falg; //签到返回信息
	int            ISO8583_Key_Rcev_falg;    //获取工作密钥返回信息
	
	//int            AP_APP_Sent_1_falg[2];            //1号.枪APP数据反馈及反馈结果
	//int            AP_APP_Sent_2_falg[2];            //2号.枪APP数据反馈及反馈结果
  int            ISO8583_Alarm_Rcev_falg;         //获取服务器下发的告警处理成功事件
	int            ISO8583_Get_SN_falg;             // //获取流水号数据反馈及反馈结果
  char           ISO8583_busy_SN_Vlaue[20];       //流水号值
	int            ISO8583_Busy_Need_Up_falg;   
	int            ISO8583_Car_Stop_Need_Up_falg;   //停车业务数据上传
	
	int            invalid_card_bag_now;   //发送当前需要获取的页数。--针对黑名单
	int            invalid_card_Allbag_now;   //发送当前需要获取的页数。--针对黑名单
  int            invalid_card_Rcev_falg; 
	int            Card_unlock_RequestRcev_falg; 
	int            Software_upate_flag; 
	int            Software_Request_flag; 
	int            old_gun_state[2]; //两把枪的充电状态	
  int            old_Car_Lock_state[2]; //地锁状态	
	char           App_ID_1[16];
	char           App_ID_2[16];
	
	
  ISO8583_AP_charg_busy_Frame busy_data;  //临时存储的交易记录数据
	ISO8583_Car_Stop_busy_Struct Car_Stop_busy_data;  //临时存储的交易记录数据
}ISO8583_APP_Struct;

#endif
