/*******************************************************************************
  Copyright(C)    2014-2015,      EAST Co.Ltd.
  File name :     Deal_Handler.h
  Author :        
  Version:        1.0
  Date:           
  Description:    交易记录数据库处理
  Other:          
  Function List:  
  History:        
          <author>   <time>   <version>   <desc>
           
*******************************************************************************/
#ifndef	DEAL_HANDLER_H
#define	DEAL_HANDLER_H

#define  TOTAL_DEAL_DB_SEL   0 //选择交易记录总表

#define TRANSACTION_FLAG    0       //开启任务事件--针对数据库而言

enum CHARGE_END_REASON
{
	//1：刷卡，2：远程停止，3：充满自动停止
	//4：余额不足，5：充电桩故障，6-BMS故障，7-按急停终止
	//8：充电时间到停止，9：充电金额到停止，10-充电电量到停止，255-其他
	
	BY_SWIPE_CARD = 1,//插卡结束充电
	BY_APP_END  = 2,//APP结束充电
	BY_SOC_FULL = 3,//充满自动停止
	BY_LACK_OF_MONEY = 4,//金额不足	
	BY_CHARGER_FAULT = 5,//充电桩故障
	BY_BMS_FAULT     = 6,//BMS故障
	BY_EMERGY_STOP   = 7,//按下紧急停机按钮
	BY_TIME_OVER     = 8,//充电时间达到设定值
	BY_MONEY_REACH   = 9,//充电金额达到设定值	
	BY_KWH_REACH     = 10,//充电电量达到设定值
	
	BY_ORDER_CARD_TIME_OVER = 11,//有序充电卡时间到
	BY_DATABASE_FAULT = 12,//数据库插入异常终止
	BY_PRESS_STOP_BUTTON = 13,//VIN充电按停止按钮停止
	BY_REMOTE_STOP = 14,//远程禁用
	BY_DOOR_OPEN = 15,//门禁打开
	BY_DC_CURRENT_LOW1 = 16,//电流持续低于限值 如充电3分钟后持续1分钟低于5A
	
	BY_DC_CURRENT_LOW2 = 17,//达到SOC限定值后判断电流持续低于设定限值 如达到SOC设定值95%以后，电流低于设定值1分钟
	
	BY_METER_CURRENT_ABNORMAL = 18,//电表电流采样与控制板电流采样相差太大停机，防止电表异常无电量
	BY_METER_VOLT_ABNORMAL = 19,//电表电压采样与控制板电压采样相差太大停机，防止电表异常无电量
	
	
	#define  DC_CTRL_STOP_REASON_OFFSET  100 //直流控制板停止充电原因偏移量
	BY_DC_Over_KWH = DC_CTRL_STOP_REASON_OFFSET+7,// 达到它可以达到的最大充电电量
	BY_DC_READ_KWH_ERR = DC_CTRL_STOP_REASON_OFFSET+8,//电表读取异常，持续读取的电量比上次少

	BY_DC_LimitSOC_Value = DC_CTRL_STOP_REASON_OFFSET+9,//达到SOC限定值后判断电流持续低于设定限值 如达到SOC设定值95%以后，电流低于设定值1分钟

	BY_DC_CURRENT_LOW = DC_CTRL_STOP_REASON_OFFSET+10,//由于充电过程中电流为0导致的结束原因
	//============================================11
	BY_DC_CTRL_NORMAL_STOP = DC_CTRL_STOP_REASON_OFFSET+12,//控制单元正常终止
	
	BY_DC_METER_FAIL = DC_CTRL_STOP_REASON_OFFSET+13,//直流电表故障
	BY_DC_FUSE_FAULT = DC_CTRL_STOP_REASON_OFFSET+14,//直流输出熔断器故障
	BY_DC_MODULE_COMM_FAULT = DC_CTRL_STOP_REASON_OFFSET+15,//充电模块通信中断
	BY_DC_CTRL_TIMEOUT_FAULT = DC_CTRL_STOP_REASON_OFFSET+16,//控制板响应超时======
	
	BY_BMSNEED_TO_HIGH = DC_CTRL_STOP_REASON_OFFSET+17,//超出了系统输出电压能力
	BY_BAT_VOLT_FAIL = DC_CTRL_STOP_REASON_OFFSET+18,// 检测电池电压误差比较大====
	//============================================19
	BY_PRE_CHG_VOLT_FAIL = DC_CTRL_STOP_REASON_OFFSET+20,//预充阶段继电器两端电压相差大于10V
		
	BY_BMS_TIMEOUT1 = DC_CTRL_STOP_REASON_OFFSET+21,//车载BMS未响应
	BY_BMS_TIMEOUT2 = DC_CTRL_STOP_REASON_OFFSET+22,//车载BMS响应超时
	BY_BAT_REVERSE = DC_CTRL_STOP_REASON_OFFSET+23,//电池反接
	BY_NOT_DET_BAT = DC_CTRL_STOP_REASON_OFFSET+24,//未检测到电池电压
	BY_BMS_NOT_READY = DC_CTRL_STOP_REASON_OFFSET+25,//BMS未准备就绪
	BY_DC_PLUG_OUT = DC_CTRL_STOP_REASON_OFFSET+26,//充电枪连接断开==============
	BY_DC_PLUGLOCK_FAIL = DC_CTRL_STOP_REASON_OFFSET+27,//枪锁异常
	BY_SELFCHECK_VOUT_FAIL = DC_CTRL_STOP_REASON_OFFSET+28,//自检输出电压异常
	BY_SELFCHECK_XIEFANG_FAIL = DC_CTRL_STOP_REASON_OFFSET+29,//自检泄放电压异常----自检前检测到外侧电压异常
	BY_AUX_PWR_FAIL = DC_CTRL_STOP_REASON_OFFSET+30,//辅助电源异常
	BY_CHG_VOLT_HIGH = DC_CTRL_STOP_REASON_OFFSET+31,//充电电压过压告警===
	BY_CHG_CUR_HIGH = DC_CTRL_STOP_REASON_OFFSET+32,//充电电流过流告警
	BY_DC_OUTPUT_SHORT = DC_CTRL_STOP_REASON_OFFSET+33,//输出短路故障
	BY_OUTPUT_VOLT_HIGH = DC_CTRL_STOP_REASON_OFFSET+34,//系统输出过压保护===
	BY_DC_ISO_FAIL = DC_CTRL_STOP_REASON_OFFSET+35,//系统绝缘故障
	BY_DC_CONTACT_FAULT = DC_CTRL_STOP_REASON_OFFSET+36,//输出接触器异常
	BY_AUX_PWR_RELAY_FAIL = DC_CTRL_STOP_REASON_OFFSET+37,//辅助电源接触器异常
	BY_BINGJI_RELAY_FAIL = DC_CTRL_STOP_REASON_OFFSET+38,//系统并机接触器异常	
	BY_DC_PLUG_TEMP_HIGH = DC_CTRL_STOP_REASON_OFFSET+39,//充电枪温度过高
	BY_DC_FAULT_STOP = DC_CTRL_STOP_REASON_OFFSET+40,//控制单元故障终止 
	 
	BY_BMS_SOC_REACH = DC_CTRL_STOP_REASON_OFFSET+41,//BMS正常终止充电,达到所需的SOC
	BY_BMS_BAT_VOLT_REACH = DC_CTRL_STOP_REASON_OFFSET+42, //BMS正常终止充电,达到总电压的设定值
	BY_BMS_CELL_VOLT_REACH = DC_CTRL_STOP_REASON_OFFSET+43, //达到单体电压的设定值
	
	BY_BMS_ISO_FAIL = DC_CTRL_STOP_REASON_OFFSET+44,//BMS报绝缘故障
	BY_BMS_CONNECTOR_TEMP_HIGH_FAIL = DC_CTRL_STOP_REASON_OFFSET+45,//BMS报输出连接器过温故障
	BY_BMS_COMPONET_TEMP_HIGH_FAIL = DC_CTRL_STOP_REASON_OFFSET+46,//BMS元件过温故障
	BY_BMS_CONNECTOR_FAIL = DC_CTRL_STOP_REASON_OFFSET+47,//BMS充电连接器故障
	BY_BMS_BAT_TEMP_HIGH = DC_CTRL_STOP_REASON_OFFSET+48,//BMS电池组温度过高故障	
	BY_BMS_HIGH_VOLT_RELAY_FAIL = DC_CTRL_STOP_REASON_OFFSET+49,//BMS高压继电器故障
	BY_BMS_CC2_VOLT_FAIL = DC_CTRL_STOP_REASON_OFFSET+50,//BMS检测点2电压检测故障
	BY_BMS_OTHER_FAIL = DC_CTRL_STOP_REASON_OFFSET+51,//BMS其他故障
	BY_BMS_CHG_CURR_HIGH = DC_CTRL_STOP_REASON_OFFSET+52,//BMS充电电流过大
	
	BY_BMS_CHG_VOLT_FAIL = DC_CTRL_STOP_REASON_OFFSET+53,//BMS充电电压异常
	BY_BMS_UNKNOWN_FAIL = DC_CTRL_STOP_REASON_OFFSET+54,//BMS未知故障
	//============================================55
	BY_BMS_CELL_VOLT_FAIL = DC_CTRL_STOP_REASON_OFFSET+56,//BMS单体电压异常
	BY_BMS_SOC_FAIL = DC_CTRL_STOP_REASON_OFFSET+57,//BMS SOC异常	
	BY_BMS_TEMP_HIGH = DC_CTRL_STOP_REASON_OFFSET+58,//BMS 过温告警
	BY_BMS_CURRENT_HIGH = DC_CTRL_STOP_REASON_OFFSET+59,//BMS过流告警
	BY_BMS_ISO_ABNORMAL = DC_CTRL_STOP_REASON_OFFSET+60,//BMS绝缘异常	
	BY_BMS_CONNECTOR_ABNORMAL = DC_CTRL_STOP_REASON_OFFSET+61,//BMS输出连接器异常
	
	BY_SYSTEM_CELL_OVER_VOLT = DC_CTRL_STOP_REASON_OFFSET+64,//系统判定BMS单体持续过压
	BY_BMS_VIN_ABNORMAL = DC_CTRL_STOP_REASON_OFFSET+75,//检测到VIN不匹配
	BY_CHARGEMODULE_NOREADY = DC_CTRL_STOP_REASON_OFFSET+76,//充电模块未准备
	BY_BMS_BCLDATA_ABNORMAL = DC_CTRL_STOP_REASON_OFFSET+77,////检测到BCL数据异常（需求电压，电流 会大于最高电压，电流）
	
	BY_VIN_MISMATCH = 252,//卡片绑定的VIN码与车的VIN码不匹配
	BY_VIN_AUTH_FAILED = 253,//VIN鉴权失败===
	
	BY_POWER_DOWN    = 254,//系统掉电	
		
	BY_UNKNOWN_REASON = 255,
	
	
};
//交易记录存储结构
typedef	struct
{
	unsigned char charger_SN[16];        //充电桩编号  字符串 目前12位
	unsigned char chg_port;           //充电枪口号 从1开始
	unsigned char deal_SN[32];            //交易流水号 字符串 目前仅用20字节
	unsigned char card_sn[16];           //充电卡号或用户账号 16位 字符串
	unsigned long chg_start_by;       //充电启动方式，值1-APP，值0-刷卡,100--VIN鉴权
	unsigned long card_deal_result;         //0卡交易成功，1卡交易失败
	
	unsigned char   start_time[14];    //充电开始时间  字符串
	unsigned char  	end_time[14];	  //充电结束时间  字符串

	
	unsigned long  start_kwh;           //总起示值 0.01kwh
	unsigned long  end_kwh;             //总止示值 0.01kwh
	
	unsigned long  kwh1_price;             //尖单价，单位元，系数0.00001
	unsigned long  kwh1_charged;           //尖时段用电量，4字节BIN码，单位千瓦时,系数0.01
	unsigned long  kwh1_money;           //尖金额，4字节BIN码，单位元,系数0.01
	
	
	unsigned long  kwh2_price;             //峰单价，单位元，系数0.00001
	unsigned long  kwh2_charged;           //峰时段用电量，4字节BIN码，单位千瓦时,系数0.01
	unsigned long  kwh2_money;             //峰金额，4字节BIN码，单位元,系数0.01
	
	unsigned long  kwh3_price;             //平单价，单位元，系数0.00001
	unsigned long  kwh3_charged;           //平时段用电量，4字节BIN码，单位千瓦时,系数0.01
	unsigned long  kwh3_money;             //平金额，4字节BIN码，单位元,系数0.01
	
	unsigned long  kwh4_price;             //谷单价，单位元，系数0.00001
	unsigned long  kwh4_charged;           //谷时段用电量，4字节BIN码，单位千瓦时,系数0.01
	unsigned long  kwh4_money;             //谷金额，4字节BIN码，单位元,系数0.01
	
	unsigned long  kwh_total_charged;      //总用电量，4字节BIN码，单位千瓦时,系数0.01
	
	long  money_left;      //扣款后账户余额，4字节BIN,单位元，小数点2位
	unsigned long  last_rmb;        //充电前账户余额，4字节BIN,单位元，小数点2位
	
	unsigned long rmb_kwh;        //消费单价,0.01
	unsigned long total_kwh_money;           //充电消费总金额，单位元，系数0.01	
	
	unsigned char  car_VIN[17];              //车辆编号 字符串
	
	/////////////////////////////////////////////////////////////////////////////
	unsigned long  kwh1_service_price;//尖服务费单价,0.00001元
	unsigned long  kwh1_service_money;//尖服务费金额,0.01元	
	unsigned long  kwh2_service_price;//峰服务费单价,0.00001元
	unsigned long  kwh2_service_money;//峰服务费金额,0.01元	
	unsigned long  kwh3_service_price;//平服务费单价,0.00001元
	unsigned long  kwh3_service_money;//平服务费金额,0.01元	
	unsigned long  kwh4_service_price;//谷服务费单价,0.00001元
	unsigned long  kwh4_service_money;//谷服务费金额,0.01元	
	/////////////////////////////////////////////////////////////////////////////
	
	unsigned long  book_price;//预约费单价,0.00001元
	unsigned long  book_money;//预约费金额,0.01元	
	
	unsigned long  park_price;//停车费单价,0.00001元
	unsigned long  park_money;//停车费金额,0.01元
	
	unsigned long  all_chg_money;      //总消费金额，4字节BIN码，单位元,系数0.01		
	
	unsigned long  end_reason;            //
	unsigned long  Car_AllMileage;
	unsigned long  Start_Soc;//开始SOC
	unsigned long  End_Soc;//结束soc
	unsigned long  up_num;//上传标记

	// 为兼容二代协议  fortest
	unsigned char sgp_busy_sn[32];           	// 云平台订单号
	unsigned char sgp_chargePriceCount; 		// 费用阶段数
    T_SChargeConsume sgp_chargeCosume[48];		// 区间电费信息
    unsigned char sgp_isParallels;				// 是否并机
    unsigned char sgp_isParallelsOrder;			// 是否并机汇总账单
    unsigned char sgp_isHost;					// 是否是主枪
    float sgp_slaveCharge; 						// 从机电量
    float sgp_slaveChrConsume;					// 从机充电消费
    float sgp_slaveSrvConsume;					// 从机服务费
    unsigned int priceVersion;					// 计费版本
    unsigned char isOutline;					// 是否离线
}ISO8583_charge_db_data;

//db_sel值0表示交易记录总表
//db_sel值1表示枪1的交易记录
//db_sel值2表示枪2的交易记录

//插入一条新记录到指定数据库,通过id返回该记录在数据库中的序号
int Insert_Charger_Busy_DB(unsigned char db_sel, ISO8583_charge_db_data *frame, unsigned int *id, unsigned int up_num);

//更新指定id的记录中的部分字段
int Update_Charger_Busy_DB(unsigned char db_sel,unsigned int id, ISO8583_charge_db_data *frame, unsigned int up_num);

//删除数据库中指定id的记录
int Delete_Record_busy_DB(unsigned char db_sel,unsigned int id);

//查询ID值最大的记录
int Select_MAX_ID_Busy(unsigned char db_sel,ISO8583_charge_db_data *frame);

//查询还没有上传到总表或总表中未上传到服务器的记录返回 ID最大值
int Select_Busy_Need_Up(unsigned char db_sel,ISO8583_charge_db_data *frame, unsigned int *up_num);

//修改数据库指定id的记录的上传标志up_num值
int Set_Busy_UP_num(unsigned char db_sel, int id, unsigned int up_num);

//VIN鉴权启动后写入对应的充电内部账号号
int Update_VIN_CardSN(unsigned char db_sel,int id,ISO8583_charge_db_data *frame);

//查询指定id的记录的
//up_num等于0x55或0x66并且记录中的up_num不等于参数up_num的记录
//返回值0表示有匹配的记录
int Select_NO_Over_Record(unsigned char db_sel, int id,int up_num);
//将up_num等于180的记录的up_num值修为0
int Set_0XB4_Busy_ID(unsigned char db_sel);

//删除总表中超过2000条的最早记录（一次只删除一条记录）
int delete_over_2000_busy(void);

//查看是否有与frame相匹配的记录在数据库中
//返回值0表示有匹配的记录
int Select_Busy_BCompare_ID(unsigned char db_sel,ISO8583_charge_db_data *frame);

//查询意外掉电的不完整数据记录
int Select_0X55_0X66_Busy_ID(unsigned char db_sel,ISO8583_charge_db_data *frame, unsigned int *up_num);

//更新意外掉电的不完整数据记录
int Restore_0X55_Busy_ID(unsigned char db_sel,ISO8583_charge_db_data *frame, int id);
//将id指定的记录的up_num值更新为0===========
int Set_0X66_Busy_ID(unsigned char db_sel,int id);//

//将记录中up_num为85(0x55)或102(0x66)或180(0xB4)的值设置为0=======
int Set_0X550X66_Busy_ID(unsigned char db_sel);

//查询没有流水号的最大记录 ID 值 type值0表示查询充电业务数据库，其他值查询停车业务数据库
int Select_NO_Busy_SN(int type);

//添加充电记录的流水号 Busy_SN_Type值0表示查询充电业务数据库，其他值查询停车业务数据库
int Set_Busy_SN(int id, unsigned char *Busy_SN,int Busy_SN_Type);


//查询有没有该卡号锁卡记录最大记录 ID 值，有则读出相应的数据
int Select_Unlock_Info(void);

//清除锁卡记录，设置卡片补扣或锁卡记录已上送标记
//card_deal_result--值0解锁成功先于记录上送
//值1锁卡
//值2锁卡记录已上送
//值3锁卡记录上送前本地解锁
int Set_Unlock_flag(int id,int card_deal_result);

//黑名单数据更新
int Insert_card_DB(unsigned char *sn, unsigned int count);

//查询卡片是否为黑名单中的卡号
//返回值不是-1表示卡片黑名单
int valid_card_check(unsigned char *sn);

//删除黑名单列表中指定sn的项
int delete_card_blist(unsigned char *sn);

// 更新临时表的交易流水号
int Set_Temp_Busy_SN(unsigned char db_sel, unsigned char *Busy_SN);
#endif
