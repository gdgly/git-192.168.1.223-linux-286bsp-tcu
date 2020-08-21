/*******************************************************************************
  Copyright(C)    2014-2015,      EAST Co.Ltd.
  File name :     
  Author :        dengjh
  Version:        1.0
  Date:           2014.4.11
  Description:    
  Other:          
  Function List:  
  History:        
          <author>   <time>   <version>   <desc>
            dengjh    2014.4.11   1.0      Create
*******************************************************************************/
#ifndef   _QT_H
#define   _QT_H

#include "globalvar.h"


extern UINT32 Dc_shunt_Set_meter_flag[CONTROL_DC_NUMBER];
extern UINT32 Dc_shunt_Set_CL_flag[CONTROL_DC_NUMBER];

#define BUFFLEN 300
typedef struct _MSGSTR{
  long mtype; //大于0
  unsigned char mtext[BUFFLEN];
}CHARGER_MSG;


typedef struct _PAGE_90_FRAME
{                                        
	UINT8   reserve1;        //保留
	UINT8   flag;            //二维码显示与�?0：不显示  1显示
	UINT8   sn[16];          //桩序列号
	UINT8   http[60];        //APP下载网址 最�?0个字�?	UINT8  gun_config; //充电枪数 1---N
}PAGE_90_FRAME;


typedef struct _MAIN_110_FRAME
{                                        
	UINT8  charge_state;      //0---空闲  1--连接�?2-充电�?3--充电结束
	UINT8  charge_V[2];       //充电电压      0.1V
	UINT8  charge_C[2];       //充电电流      0.1A
	UINT8  KWH[4];            //充电电量      0.001 �?}MAIN_110_FRAME;

typedef struct _PAGE_110_FRAME
{                                        
	UINT8   reserve1;        //保留
	UINT8   Button_Show_flag;//显示按钮是否出现  0--出现 1--消失
	UINT8   support_double_gun_to_one_car;//是否启用双枪对一�?	MAIN_110_FRAME Main_110_Data[CONTROL_DC_NUMBER];
}PAGE_110_FRAME;


//自动充电-》充电枪连接确认
typedef struct _PAGE_1000_FRAME
{                                        
	UINT8   reserve1;        //保留
	UINT8   flag;        //保留
	UINT8   link;            //1:连接  0：断开
	UINT8   gun_no;//reserve3;        //保留
}PAGE_1000_FRAME;


typedef struct _PAGE_200_FRAME
{                                        
	UINT8   reserve1;        //保留
	UINT8   state;           //状�?
	//state = 5�? 时显�?卡号和余�?，其他数字时显示为空
	//1:没有检测到充电�?	//2:无法识别充电�?	//3:卡片处理失败，请重新刷卡
	//4:用户卡状态异�?	//5:检测到用户�?  出现卡号和金�?
	//6:检测到员工�?  出现卡号和金�?	//7:卡片已注销
	//8:非充电卡   主要指维修卡类不能使�?	//9:已被本机使用
	//10:请刷管理员卡
	//11:充电余额过低，请充值Charge card balance is too low, please recharge
	//;13 06: 卡的企业号标记不同步;07: 卡的私有电价标记不同�?08: 卡的有序充电标记不同�?09:非规定时间充电卡;10:企业号余额不�?11:专用时间其他卡不可充
	UINT8   sn[16];          //卡号 数字字符
	UINT8   rmb[8];          //余额 数字字符 精确到分

}PAGE_200_FRAME;

/*
typedef struct _setmod{
    unsigned char nc;
    unsigned char mun1;
    unsigned char mun2;
}setmod;

充电方式 ：按金额，电量，时间
mun1低字�?mun2 高字�?单位依次�?�?�?分钟
如：mun1�?x44; mun2�?x1; 表示300
预约充电
mun1 为小�?mun2 为分�?如：mun1�?x14; mun2�?x1; 表示 20�?1
*/

//自动充电界面 一把枪还是两把枪共用一个界�?typedef struct _PAGE_BMS_FRAME
{  
	UINT8 reserve1;                               
	UINT8 BMS_Info_falg; //BMS更新标志                              
	UINT8 BMS_H_vol[2];      //BMS最高允许充电电�? 0.1V
	UINT8 BMS_H_cur[2];      //BMS最高允许充电电�? 0.01A
	UINT8 BMS_cell_LT[2];       //5	电池组最低温�?8	BIN	2Byte	精确到小数点�?�?-50�?~ +200�?偏移50�?	UINT8 BMS_cell_HT[2];      //6	电池组最高温�?10BIN	2Byte	精确到小数点�?�?-50�?~ +200�?偏移50�?	UINT8 need_voltage[2];   //12	充电输出电压	2	BIN	2Byte	精确到小数点�?�?V - 950V
	UINT8 need_current[2];   //13	充电输出电流	4	BIN	2Byte	精确到小数点�?�?A - 400A
	UINT8 BMS_Vel[3];         //BMS协议版本                                    
	UINT8 Battery_Rate_AH[2]; //2	整车动力蓄电池额定容�?.1Ah
	UINT8 Battery_Rate_Vol[2]; //整车动力蓄电池额定总电�?	UINT8 Battery_Rate_KWh[2]; //动力蓄电池标称总能量KWH
	UINT8 Cell_H_Cha_Vol[2];     //单体动力蓄电池最高允许充电电�?0.01 0-24v
	UINT8 Cell_H_Cha_Temp;     //单体充电最高保护温�?-50+200 偏移50�?	UINT8 Charge_Mode;        //8充电模式
	UINT8 Charge_Gun_Temp;   //充电枪温�?-50+200 偏移50�?	UINT8 BMS_cell_HV_NO; 
	UINT8 BMS_cell_LT_NO;            //5电池组最低温度编�?
	UINT8 BMS_cell_HT_NO;            //6电池组最高温度编�?
	UINT8 VIN[17];              //车辆识别�?	UINT8 keep[5];
}PAGE_BMS_FRAME;


typedef struct _PAGE_1111_FRAME
{                                        
	UINT8   reserve1;        //保留
	UINT8   Gun_no;
	UINT8   Button_Show_flag;//显示按钮是否出现  0--出现 1--消失

	UINT8   BATT_type;       //电池类型
	UINT8   BATT_V[2];       //充电电压      0.1V
	UINT8   BATT_C[2];       //充电电流      0.1A
	UINT8   KWH[4];          //充电电量      0.001 �?
	UINT8   Capact;          //BMS容量   0-100%
	UINT8   run;             //电池充电动态显示容�?0:静�?1：动�?
	UINT8   elapsed_time[2];  //已充时间      �?
	UINT8   need_time[2];     //剩余时间      �?

	UINT8   cell_HV[2];      //单体最高电�? 0.01V
	UINT8   money[4];        //充电金额  精确到分 低在�?高在�?
	//UINT8   charging;        //0：都没有在充�?1:充电�?	UINT8   aux_gun;         //是否是双枪同充一车的副枪�?是，0�?	UINT8   link;            //1:连接  0：断开

	UINT8   SN[16];          //卡号 16�?	UINT8   charger_start_way;             //充电启动方式 1:刷卡  2：手机APP 101:VIN
	UINT8   BMS_12V_24V;     //BMS电源选择
	
	UINT8   power_kw[4];//0.1kw功率
	UINT8   start_time[9];//18:18:20
	 //PAGE_BMS_FRAME Page_BMS_Info;
}PAGE_1111_FRAME;

//双枪对一辆车充电

typedef struct _PAGE_1E1_FRAME
{
	 UINT8   reserve1;        //保留
	 UINT8   On_link;
	 UINT8   Gun_Number;
	 UINT8   Gun_link[10];  //各个枪的连接状�?	 UINT8   Gun_State[10];  //各个枪的状�?	 UINT8   Error_system[10];//枪异常状�?}PAGE_1E1_FRAME;

//倒计时界�?typedef struct _PAGE_DAOJISHI_FRAME
{                                        
	UINT8   reserve1;        //保留
	UINT8   Gun_no;
	UINT8   Button_Show_flag;//显示按钮是否出现 0--出现 1--消失

	UINT8   hour;            //剩余小时
	UINT8   min;             //剩余分钟
	UINT8   second;          //剩余�?	UINT8   type;            //类型 0---显示倒计时间�?--不显示到计时
	UINT8   card_sn[16];          //卡号 16�?}PAGE_DAOJISHI_FRAME;

//充电结束界面
typedef struct _PAGE_1300_FRAME
{                                        
	UINT8   reserve1;        //保留
	UINT8   Gun_no;
	UINT8   Button_Show_flag;//显示按钮是否出现 0--出现 1--消失

	UINT8   cause;            //充电结束原因

	UINT8   Capact;           //结束时BMS容量   0-100%
	UINT8   start_time[9];//18:18:20
	UINT8   end_time[9];//18:18:20
	UINT8   elapsed_time[2];  //本次充电时间      �?
	UINT8   KWH[4];           //本次充电电量      0.001 KWH
	UINT8   RMB[4];           //本次消费金额      0.01 �?	UINT8   SN[16];           //卡号 16�?	UINT8   charger_start_way;             //充电启动方式 1:刷卡  2：手机APP 101:VIN
	UINT8   Double_Gun_To_Car_falg;//双枪对一辆车
}PAGE_1300_FRAME;


//手动充电界面 一把枪还是两把枪共用一个界�?typedef struct _PAGE_3600_FRAME
{
   UINT8   reserve1;        //保留

   UINT8   NEED_V[2];       //设置电压         0.1V/�?   UINT8   NEED_C[2];       //设置电流         0.1A/�?   UINT8   time[2];         //时间             1-9999 min 
   UINT8   select;          //选择的充电枪     1：充电枪1 2: 充电�?

   UINT8   BATT_V[2];       //输出电压         0.1V/�?   UINT8   BATT_C[2];       //输出电流         0.1A/�?   UINT8   Time[2];         //充电时间         �?   UINT8   KWH[2];          //电量             0.01 �?}PAGE_3600_FRAME;

//系统管理 系统设置 3503 
typedef struct _PAGE_3503_FRAME
{
	UINT8   reserve1;        //保留

	UINT8   module_num[CONTROL_DC_NUMBER];
	UINT8   rate_v[2];       //模块额定电压    1V/�?最大�?750
	UINT8   single_rate_c;   //模块额定电流    1A/�?
	UINT8   BMS_CAN_ver;     //BMS协议版本  2：普天标�?1：国家标�?	UINT8   SN[16];          //系统编号        16位数字字符串

	UINT8  meter_config;     //电能表配置标�? 1：有�?：无
	UINT8  Power_meter_addr[CONTROL_DC_NUMBER][12];		//电能表地址 12字节

	UINT8  output_over_limit[2];      //输出过压�?  0.1V/�? 低在前高在后
	UINT8  output_lown_limit[2];      //输出欠压�?  0.1V/�? 低在前高在后

	UINT8  input_over_limit[2];      //输入过压�?  0.1V/�? 低在前高在后
	UINT8  input_lown_limit[2];      //输入欠压�?  0.1V/�? 低在前高在后
	UINT8  current_limit[CONTROL_DC_NUMBER];            //最大输出电�?  每枪单独设置


	UINT8  BMS_work;             //系统工作模式 1：正常；2：测�?	UINT8  DC_Shunt_Range[CONTROL_DC_NUMBER][2];    //直流分流器量�?

	UINT8  charge_modlue_factory;//模块厂家索引 0-华为模块 1-英可瑞模�?2-英飞源模�?3-永联模块�?-其他类型模块
	UINT8  charge_modlue_index;//模块型号索引 0---N
	
	UINT8  AC_DAQ_POS;//交流采集板安装位�?	UINT8  gun_allowed_temp;
	UINT8  Charging_gun_type;
	UINT8  Input_Relay_Ctl_Type;
	UINT8  System_Type;
	UINT8  Serial_Network;
	UINT8  DTU_Baudrate;
	UINT8  NEW_VOICE_Flag;
	UINT8  AC_Meter_CT;//增加交流变比设置
	UINT8  SOC_limit;	//SOC达到多少时，判断电流低于min_current后停止充�?�?5表示95%
	UINT8  min_current;//单位A  �?0表示低于30A后持�?分钟停止充电
	UINT8  heartbeat_idle_period;//空闲心跳周期 �?	UINT8  heartbeat_busy_period;//充电心跳周期 �?	UINT8  COM_Func_sel[10];//COM口功能定�?	
	UINT8 support_double_gun_to_one_car;
	UINT8 support_double_gun_power_allocation;  //双枪支持功率分配，即双枪外加2个控制板
	UINT8 Power_Contor_By_Mode;//功率控制方式
	UINT8 Contactor_Line_Num;  //系统并机数量
	UINT8 Contactor_Line_Position[5];  //系统并机路线节点：高4位为主控点（即控制并机的ccu）地4位为连接�?	UINT8 CurrentVoltageJudgmentFalg;//电流电压校准判断
  UINT8 keep_data;
}PAGE_3503_FRAME;


//系统管理 网络设置 3203 
typedef struct _PAGE_3203_FRAME
{
	UINT8  reserve1;        //保留

	UINT8  Server_IP[50];
	UINT8  port[2];
	UINT8  addr[2];
}PAGE_3203_FRAME;


//关于 3600
typedef struct _PAGE_2100_FRAME
{
	UINT8   reserve1;        //保留

	UINT8  measure_ver[20];
	UINT8  contro_ver[CONTROL_DC_NUMBER][20];//控制版本
	UINT8  protocol_ver[20];//协议版本
	UINT8  kernel_ver[20];//内核版本
	UINT8  gun_config; //充电枪数 1---N
	UINT8  Serv_Type;      //服务费收费方�?1：按次收费；2：按时间收费(�?10分钟�?：按电量
	UINT8  run_state;      //运行状�?   
	UINT8  dispaly_Type;  //金额显示类型�?0--详细显示金额+分时�?   1--直接显示金额

	UINT8  sharp_rmb_kwh[4];
	UINT8  peak_rmb_kwh[4];
	UINT8  flat_rmb_kwh[4];
	UINT8  valley_rmb_kwh[4];
	UINT8  service_price[4][4];//尖峰平谷4种服务费===181128
	UINT8  book_price[4];
	UINT8  park_price[4];
	// UINT8  sharp_rmb_kwh_serve[2];//尖峰分时服务电价  
	// UINT8  peak_rmb_kwh_serve[2];//高峰分时服务电价  
	// UINT8  flat_rmb_kwh_serve[2];//平峰分时服务电价  
	// UINT8  valley_rmb_kwh_serve[2];//谷分分时服务电价 
	UINT8 time_zone_num;  //时段�?	UINT8 time_zone_feilv[12];//时段表位于的价格---表示对应的为尖峰平谷的�?
	UINT8 time_zone_tabel[12][2];//时段�?}PAGE_2100_FRAME;


typedef struct _PAGE_5200_FRAME
{                                        
	UINT8   reserve1;       //保留
	UINT8   Result_Type;    //结果类型 0--请刷�?1--卡片正常,2--数据核对查询当中请稍�?3--查询成功显示从后台或本地获取的信息（显示相应的数据，其他的都隐藏），4--查询成功未找到锁卡记录，请稍候查�?5-查询失败--卡号无效 �?							//6--查询失败--无效充电桩序列号 7，MAC 出差或终端为未对�?8,支付卡解锁中�?，两次刷卡卡号不一样，10解锁成功
	card_lock_bcdtime card_lock_time; //锁卡时间
	UINT8 Deduct_money[2];     //需扣款金额
	UINT8  busy_SN[20];        //交易流水�?11	N	N20
}PAGE_5200_FRAME;


typedef struct _PAGE_6100_FRAME
{                                        
	UINT8   reserve1;        //保留
	UINT8   Display_prompt;  //显示提示�?	UINT8   file_number;      //下发文件数量
	UINT8   file_no;          //当前下发文件
	UINT8   file_Bytes[3];    //总字节数
	UINT8   sendfile_Bytes[3];            //成功下发的字节数/
	UINT8   over_flag[5];      //控制�?,2,升级步骤标志  0--等待下发 1--下发�?2--下发完成（进行结果判定）
	UINT8   info_success[5];   //控制�?,2,下发情况,结果 0--未进行判断（还在下发�? 1---升级失败 2---升级成功
}PAGE_6100_FRAME;

typedef struct {
	UINT8 reserve1;     //保留
	UINT8 V1_AB[2];		  //AB线电�? 0.1V
	UINT8 V1_BC[2];		  //BC线电�? 0.1V
	UINT8 V1_CA[2];		  //CA线电�? 0.1V
	UINT8 C1_A[2];		  //A线电�?  0.01A
	UINT8 C1_B[2];		  //B线电�?  0.01A
	UINT8 C1_C[2];		  //C线电�?  0.01A

	UINT8 PW_ACT[2];		  //总有�?10 W
	UINT8 PW_INA[2];		  //总无�?10 Var
	UINT8 F1[2];		      //频率    0.01Hz

	UINT8 input_switch_on;   //输入开关状态信号，1 闭合�? 断开
	UINT8 output_switch_on1; //输出开关状态信�?�? 闭合�? 断开
	UINT8 fanglei_on;        //交流避雷器报警信号，1 闭合�? 断开
	UINT8 output_switch_on2; //输出开关状态信�?�? 闭合�? 断开
	
	UINT8 AC_VA[2];		  //A相电�? 0.1V
	UINT8 AC_VB[2];		  //B相电�? 0.1V
	UINT8 AC_VC[2];		  //C相电�? 0.1V
	UINT8 AC_CA[2];		  //A相电�?  0.01A
	UINT8 AC_CB[2];		  //B相电�?  0.01A
	UINT8 AC_CC[2];		  //C相电�?  0.01A
	
	UINT8 AC_KWH[4];   //AC有功电能 0.01�?	UINT8 DC1_KWH[4];  //DC1有功电能 0.01�?	UINT8 DC2_KWH[4];  //DC2有功电能 0.01�?	UINT8 DC3_KWH[4];  //DC3有功电能 0.01�?	UINT8 DC4_KWH[4];  //DC4有功电能 0.01�?	UINT8 DC5_KWH[4];  //DC5有功电能 0.01�?	
}MISC_METER_DATA;//交流采集和电表数�?



typedef struct _PAGE_400_FRAME
{
	UINT8  reserve1;        //保留
	UINT8  VIN[17];
	UINT8  VIN_auth_step; //0---等待车辆返回VIN ,1---发送VIN鉴权请求等待响应,2---获取到VIN鉴权响应信息,3---等待VIN鉴权超时
	UINT8  VIN_auth_success;//1--成功,0-失败
	UINT8  VIN_auth_failed_reason[2];//
	UINT8  VIN_auth_money[4];//鉴权账户余额�?.01�?}PAGE_400_FRAME;

//计费模式
typedef struct _PAGE_3703_FRAME
{
	UINT8  reserve1;        //保留
  UINT8 share_time_kwh_price[4][4];		     //分时电价  //尖电价单位分 //峰电价单位分 //平电价单位分 //谷电价单位分
	UINT8 share_time_kwh_price_serve[4][4]; //分时服务电价
  UINT8 Card_reservation_price[4];//刷卡预约�?	UINT8 Stop_Car_price[4];//停车�?	
  UINT8 time_zone_num;  //时段�?	UINT8 time_zone_tabel[12][2];//时段�?  UINT8 time_zone_feilv[12];//时段表位于的价格
}PAGE_3703_FRAME;
typedef struct _PAGE_7000_FRAME
{                                        
	UINT8   reserve1;        
  CHARGER_SHILELD_ALARMFALG Charger_Shileld_Alarm; 
}PAGE_7000_FRAME;
enum {
	
	QT_Step_VIN_CHECK = 0x400,//VIN鉴权等待
	
};



#endif