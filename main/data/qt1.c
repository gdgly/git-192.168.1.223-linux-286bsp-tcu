/*******************************************************************************
  Copyright(C)    2014-2015,      EAST Co.Ltd.
  File name :     core.c
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
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "globalvar.h"
#include "iso8583.h"
#include "../../inc/sqlite/sqlite3.h"
#include "../lock/Carlock_task.h"

#define MT318_620_DEBUG 0        //ISO8583_DEBUG 调试使能
 
#include "common.h"

extern UINT32 Main_1_Watchdog;
UINT32 Dc_shunt_Set_meter_flag_1 = 0;
UINT32 Dc_shunt_Set_meter_flag_2 = 0;
UINT32 Dc_shunt_Set_CL_flag_1 = 0;
UINT32 Dc_shunt_Set_CL_flag_2 = 0;

extern int  AP_APP_Sent_1_falg[2];            //1号.枪APP数据反馈及反馈结果

typedef struct _PAGE_90_FRAME
{                                        
	UINT8   reserve1;        //保留

	UINT8   flag;            //二维码显示与否 0：不显示  1显示

	UINT8   sn[12];          //桩序列号

	UINT8   http[60];        //APP下载网址 最大60个字符
}PAGE_90_FRAME;
static PAGE_90_FRAME Page_90;
//自动充电-》充电枪连接确认
typedef struct _PAGE_1000_FRAME
{                                        
	UINT8   reserve1;        //保留

	UINT8   other_link;        //高位为有效符，低位为第二把枪的连接状态
	UINT8   link;            //1:连接  0：断开
	UINT8   VIN_Support;    //--是否支持VIN
}PAGE_1000_FRAME;
static PAGE_1000_FRAME Page_1000;

typedef struct _PAGE_200_FRAME
{                                        
	UINT8   reserve1;        //保留

	UINT8   state;           //状态

						//state = 5，6 时显示 卡号和余额 ，其他数字时显示为空
						//1:没有检测到充电卡
						//2:无法识别充电卡
						//3:卡片处理失败，请重新刷卡
						//4:用户卡状态异常
						//5:检测到用户卡   出现卡号和金额 
						//6:检测到员工卡   出现卡号和金额
						//7:卡片已注销
						//8:非充电卡   主要指维修卡类不能使用
						//9:已被本机使用
					//10:请刷管理员卡
				 //11:充电余额过低，请充值Charge card balance is too low, please recharge
	       //;13 06: 卡的企业号标记不同步;07: 卡的私有电价标记不同步;08: 卡的有序充电标记不同步;09:非规定时间充电卡;10:企业号余额不足;11:专用时间其他卡不可充
	UINT8   sn[16];          //卡号 数字字符
	UINT8   rmb[8];          //余额 数字字符 精确到分

}PAGE_200_FRAME;
static PAGE_200_FRAME Page_200;

//自动充电界面 一把枪还是两把枪共用一个界面
typedef struct _PAGE_BMS_FRAME
{  
   UINT8 reserve1;                               
   UINT8 BMS_Info_falg; //BMS更新标志                              
	 UINT8 BMS_H_vol[2];      //BMS最高允许充电电压  0.1V
	 UINT8 BMS_H_cur[2];      //BMS最高允许充电电流  0.01A
	 UINT8 BMS_cell_LT[2];       //5	电池组最低温度	8	BIN	2Byte	精确到小数点后0位 -50℃ ~ +200℃
	 UINT8 BMS_cell_HT[2];      //6	电池组最高温度	10BIN	2Byte	精确到小数点后0位 -50℃ ~ +200℃
	 UINT8 need_voltage[2];   //12	充电输出电压	2	BIN	2Byte	精确到小数点后1位0V - 950V
   UINT8 need_current[2];   //13	充电输出电流	4	BIN	2Byte	精确到小数点后2位0A - 400A
	 UINT8 BMS_Vel[3];           //BMS协议版本                                    
	 UINT8 Battery_Rate_AH[2];   //2	整车动力蓄电池额定容量0.1Ah
	 UINT8 Battery_Rate_Vol[2]; //整车动力蓄电池额定总电压
	 UINT8 Battery_Rate_KWh[2]; //动力蓄电池标称总能量KWH
	 UINT8 Cell_H_Cha_Vol[2];   //单体动力蓄电池最高允许充电电压 0.01 0-24v
	 UINT8 Cell_H_Cha_Temp;     //单体充电最高保护温度 -50+200
	 UINT8 Charge_Mode;        //8充电模式
	 UINT8 Charge_Gun_Temp; 
	 UINT8 BMS_cell_HV_NO; 
	 UINT8 BMS_cell_LT_NO;            //5电池组最低温度编号	
	 UINT8 BMS_cell_HT_NO;            //6电池组最高温度编号	
	 UINT8 VIN[17];              //车辆识别码
	 UINT8 keep[5];
}PAGE_BMS_FRAME;
static PAGE_BMS_FRAME Page_BMS_data;

typedef struct _PAGE_1111_FRAME
{                                        
   UINT8   reserve1;        //保留

   UINT8   BATT_type;       //电池类型
   UINT8   BATT_V[2];       //充电电压      0.1V
   UINT8   BATT_C[2];       //充电电流      0.1A
   UINT8   KWH[2];          //充电电量      0.01 度

   UINT8   Capact;          //BMS容量   0-100%
   UINT8   run;             //电池充电动态显示容量 0:静态 1：动态

	 UINT8   elapsed_time[2];  //已充时间      秒 
	 UINT8   need_time[2];     //剩余时间      秒 

   UINT8   cell_HV[2];      //单体最高电压  0.01V
   UINT8   money[4];        //充电金额  精确到分 低在前 高在后

   UINT8   charging;        //0：都没有在充电 1:充电中
   UINT8   link;            //1:连接  0：断开

   UINT8   SN[16];          //卡号 16位
	 UINT8   APP;             //充电启动方式 1:刷卡  2：手机APP
	 UINT8   BMS_12V_24V;     //BMS电源选择
	 //PAGE_BMS_FRAME Page_BMS_Info;
}PAGE_1111_FRAME;
static PAGE_1111_FRAME Page_1111;

//充电界面数据---双枪对一辆车
typedef struct _PAGE_2020_FRAME
{
	 UINT8   reserve1;        //保留

   UINT8   BATT_type;       //电池类型
   UINT8   BATT_V[2];       //充电电压      0.1V
   UINT8   BATT_C[2];       //充电电流      0.1A
   UINT8   KWH[2];          //充电电量      0.01 度
   UINT8   money[4];        //充电金额  精确到分 低在前 高在后
	 
   UINT8   BATT_V_2[2];       //充电电压      0.1V
   UINT8   BATT_C_2[2];       //充电电流      0.1A
   UINT8   KWH_2[2];          //充电电量      0.01 度
   UINT8   money_2[4];        //充电金额  精确到分 低在前 高在后

   UINT8   Capact;          //BMS容量   0-100%
   UINT8   run;             //电池充电动态显示容量 0:静态 1：动态
	 UINT8   elapsed_time[2];  //已充时间      秒 
	 UINT8   need_time[2];     //剩余时间      秒 
   UINT8   cell_HV[2];      //单体最高电压  0.01V
   UINT8   charging;        //0：都没有在充电 1:充电中
   UINT8   link;            //1:连接  0：断开

   UINT8   SN[16];          //卡号 16位
	 UINT8   APP;             //充电启动方式 1:刷卡  2：手机APP
	 UINT8   BMS_12V_24V;     //BMS电源选择
}PAGE_2020_FRAME;
static PAGE_2020_FRAME Page_2020;

//倒计时界面
typedef struct _PAGE_DAOJISHI_FRAME
{                                        
   UINT8   reserve1;        //保留

   INT8   hour;            //剩余小时
   INT8   min;             //剩余分钟
   INT8   second;          //剩余秒
	 UINT8   type;            //类型 0---显示倒计时间，1--不显示到计时
	 UINT8   card_sn[16];          //卡号 16位
}PAGE_DAOJISHI_FRAME;
static PAGE_DAOJISHI_FRAME Page_daojishi;

//充电结束界面
typedef struct _PAGE_1300_FRAME
{                                        
   UINT8   reserve1;        //保留

   UINT8   cause;            //充电结束原因

   UINT8   Capact;           //结束时BMS容量   0-100%

	 UINT8   elapsed_time[2];  //本次充电时间      秒 
   UINT8   KWH[2];           //本次充电电量      0.01 KWH
   UINT8   RMB[4];           //本次消费金额      0.01 元
	 UINT8   SN[16];           //卡号 16位
	 UINT8   APP;             //充电启动方式 1:刷卡  2：手机APP
}PAGE_1300_FRAME;
static PAGE_1300_FRAME Page_1300;

//手动充电界面 一把枪还是两把枪共用一个界面
typedef struct _PAGE_3600_FRAME
{
   UINT8   reserve1;        //保留

   UINT8   NEED_V[2];       //设置电压         0.1V/位
   UINT8   NEED_C[2];       //设置电流         0.1A/位
   UINT8   time[2];         //时间             1-9999 min 
   UINT8   select;          //选择的充电枪     1：充电枪1 2: 充电枪2

   UINT8   BATT_V[2];       //输出电压         0.1V/位
   UINT8   BATT_C[2];       //输出电流         0.1A/位
   UINT8   Time[2];         //充电时间         秒
   UINT8   KWH[2];          //电量             0.01 度
}PAGE_3600_FRAME;

static PAGE_3600_FRAME Page_3600;

//系统管理 系统设置 3503 
typedef struct _PAGE_3503_FRAME
{
   UINT8   reserve1;        //保留

   UINT8   num1;              //模块个数       1~N   最大值 32
   UINT8   num2;              //模块个数       1~N   最大值 32

   UINT8   rate_v[2];       //模块额定电压    1V/位 最大值 750
   UINT8   single_rate_c;   //模块额定电流    1A/位

   UINT8   BMS_CAN_ver;     //BMS协议版本  2：普天标准 1：国家标准
   UINT8   SN[12];          //系统编号        12位数字字符串

   UINT8  meter_config;     //电能表配置标志  1：有，0：无
	 UINT8  Power_meter_addr1[12];		//电能表地址 12字节
	 UINT8  Power_meter_addr2[12];	//电能表地址 12字节

   UINT8  output_over_limit[2];      //输出过压点   0.1V/位  低在前高在后
   UINT8  output_lown_limit[2];      //输出欠压点   0.1V/位  低在前高在后

   UINT8  input_over_limit[2];      //输入过压点   0.1V/位  低在前高在后
   UINT8  input_lown_limit[2];      //输入欠压点   0.1V/位  低在前高在后
   UINT8  current_limit;            //最大输出电流   1

   UINT8  Price[2];         //单价,精确到分 低在前高在后
   UINT8  Serv_Type;             //服务费收费方式 1：按次收费；2：按时间收费(元/10分钟；3：按电量
   UINT8  Serv_Price[2];         //服务费价格 ,精确到分 默认安电量收

   UINT8  BMS_work;             //系统工作模式 1：正常；2：测试
   UINT8  DC_Shunt_Range[2];    //直流分流器量程
	 
   UINT8  charge_modluetype;
   UINT8  APP_enable;//APP二维码显示 0：否，1：显示
	 UINT8  gun_allowed_temp;
   UINT8  Charging_gun_type;
	 UINT8  Input_Relay_Ctl_Type;
	 UINT8  System_Type;
	 UINT8  Serial_Network;
	 UINT8  DTU_Baudrate;
	 UINT8  NEW_VOICE_Flag;
   UINT8  couple_flag;
	 UINT8  Control_board_Baud_rate;
	 UINT8  Start_time_on_hour;
	 UINT8  Start_time_on_min;
	 UINT8  End_time_off_hour;
	 UINT8  End_time_off_min;
	 UINT8  Car_Lock_number;
   UINT8  Car_Lock_addr1;
   UINT8  Car_Lock_addr2;
	 UINT8  LED_Type_Config;
	 UINT8  AC_Meter_config;
	 UINT8  AC_Meter_CT;
	 
	 UINT8  Model_Type;//模块类型
	 UINT8  heartbeat_idle_period;     //空闲心跳周期 秒
	 UINT8  heartbeat_busy_period;     //充电心跳周期 秒
	 UINT8  charge_modlue_index; //具体模块索引
	 UINT8  SOC_limit;	//SOC达到多少时，判断电流低于min_current后停止充电,值95表示95%
	 UINT8  min_current ;//单位A  值30表示低于30A后持续1分钟停止充电
	 UINT8  CurrentVoltageJudgmentFalg;//0-不判断 1-进行判断。收到的电压电流和CCU发送过来的进行判断
	 UINT8  keep_data[2];//保留2个字节设置项
}PAGE_3503_FRAME;
static PAGE_3503_FRAME Page_3503;

//系统管理 网络设置 3203 
typedef struct _PAGE_3203_FRAME
{
	UINT8   reserve1;        //保留

	UINT8  Server_IP[16];
	UINT8  port[2];
	UINT8  addr[2];
}PAGE_3203_FRAME;

static PAGE_3203_FRAME Page_3203;

//计费模式
typedef struct _PAGE_3703_FRAME
{
	UINT8  reserve1;        //保留
  UINT8 share_time_kwh_price[4][4];		     //分时电价  //尖电价单位分 //峰电价单位分 //平电价单位分 //谷电价单位分
	UINT8 share_time_kwh_price_serve[4][4]; //分时服务电价
  UINT8 Card_reservation_price[4];//刷卡预约费
	UINT8 Stop_Car_price[4];//停车费
	
  UINT8 time_zone_num;  //时段数
	UINT8 time_zone_tabel[12][2];//时段表
  UINT8 time_zone_feilv[12];//时段表位于的价格
}PAGE_3703_FRAME;
static PAGE_3703_FRAME Page_3703;


//关于 3600
typedef struct _PAGE_2100_FRAME
{
	UINT8   reserve1;        //保留

	UINT8  measure_ver[15];
	UINT8  contro_ver[40];//控制版本
	UINT8  protocol_ver[15];//协议版本
	UINT8  kernel_ver[10];//内核版本

	UINT8  Serv_Type;      //服务费收费方式 1：按次收费；2：按时间收费(元/10分钟；3：按电量
  UINT8  run_state;      //运行状态    
  UINT8  dispaly_Type;  //金额显示类型， 0--详细显示金额+分时费    1--直接显示金额

  UINT8  sharp_rmb_kwh[2];
  UINT8  peak_rmb_kwh[2];
  UINT8  flat_rmb_kwh[2];
  UINT8  valley_rmb_kwh[2];
  UINT8  sharp_rmb_kwh_serve[2];//尖峰分时服务电价  
  UINT8  peak_rmb_kwh_serve[2];//高峰分时服务电价  
  UINT8  flat_rmb_kwh_serve[2];//平峰分时服务电价  
  UINT8  valley_rmb_kwh_serve[2];//谷分分时服务电价 
	UINT8 time_zone_num;  //时段数
	UINT8 time_zone_feilv[12];//时段表位于的价格---表示对应的为尖峰平谷的值 
	UINT8 time_zone_tabel[12][2];//时段表
}PAGE_2100_FRAME;

static PAGE_2100_FRAME Page_2100;

typedef struct _PAGE_5200_FRAME
{                                        
	UINT8   reserve1;        //保留
	UINT8   Result_Type;     //结果类型 0--请刷卡,1--卡片正常,2--数据核对查询当中请稍等,3--查询成功显示从后台或本地获取的信息（显示相应的数据，其他的都隐藏），4--查询成功未找到锁卡记录，请稍候查询 5-查询失败--卡号无效 ，
	                         //6--查询失败--无效充电桩序列号 7，MAC 出差或终端为未对时	8,支付卡解锁中，9，两次刷卡卡号不一样，10解锁成功
  card_lock_bcdtime card_lock_time; //锁卡时间
	UINT8 Deduct_money[4];     //需扣款金额
	UINT8  busy_SN[20];        //交易流水号	11	N	N20
}PAGE_5200_FRAME;
PAGE_5200_FRAME Page_5200;

typedef struct _PAGE_6100_FRAME
{                                        
	UINT8   reserve1;        //保留
	UINT8   Display_prompt;  //显示提示句
	UINT8   file_number;          //下发文件数量
  UINT8   file_no;          //当前下发文件
	UINT8   file_Bytes[3];    //总字节数
	UINT8   sendfile_Bytes[3];            //成功下发的字节数/
  UINT8   over_flag[2];      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
	UINT8   info_success[2];   //控制板1,2,下发情况,结果 0--未进行判断（还在下发）  1---升级失败 2---升级成功
}PAGE_6100_FRAME;
PAGE_6100_FRAME Page_6100;

typedef struct _PAGE_7000_FRAME
{                                        
	UINT8   reserve1;        
  CHARGER_SHILELD_ALARMFALG Charger_Shileld_Alarm; 
}PAGE_7000_FRAME;

static UINT32 QT_count_500ms=0;
static UINT32 QT_count_500ms_flag=0;
static UINT32 QT_count_500ms_enable=0;
static UINT32 QT_count_5000ms=0;
static UINT32 QT_count_5000ms_flag=0;
static UINT32 QT_count_5000ms_enable=0;
//获取当前时段的费率号1---4	
static unsigned char GetCurPriceIndex(void)
{
	unsigned char cur_price_index = 0,i,cur_time_zone_num = 0,time_zone_index = 0;
	unsigned short cur_time;//高字节为当前小时数，低字节为当前分钟数，BIN码
//	Get_RTC_Time(&g_rtc_time);
		time_t systime;
		struct tm tm_t;
		systime = time(NULL);        //获得系统的当前时间 
		localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
	
	cur_time = (tm_t.tm_hour<<8 ) | tm_t.tm_min;
  if(Globa_1->Special_price_APP_data_falg == 1){
		cur_time_zone_num = Globa_1->Special_price_APP_data.time_zone_num;
		for(i=0;i<cur_time_zone_num;i++){
			if((cur_time >= Globa_1->Special_price_APP_data.time_zone_tabel[i]) && (cur_time < Globa_1->Special_price_APP_data.time_zone_tabel[(i+1)%cur_time_zone_num]))//开始点为闭区间,可以取等[1段开始,1段结束)，[2段开始,2段结束)..."1段结束"值=="2段开始"值
				break;
		}
		if(i<cur_time_zone_num){
			time_zone_index = i + 1;//since 1 
		}else{//最后1个时段
			time_zone_index = cur_time_zone_num;
		}
		cur_price_index = Globa_1->Special_price_APP_data.time_zone_feilv[time_zone_index -1];//取得第1个时间段的费率号
  }else	if((Globa_1->private_price_flag == 1)&&(Globa_1->private_price_acquireOK == 1)){//私有时区//获取私有电价
		cur_time_zone_num = Globa_1->Special_price_data.time_zone_num;
		for(i=0;i<cur_time_zone_num;i++){
			if((cur_time >= Globa_1->Special_price_data.time_zone_tabel[i]) && (cur_time < Globa_1->Special_price_data.time_zone_tabel[(i+1)%cur_time_zone_num]))//开始点为闭区间,可以取等[1段开始,1段结束)，[2段开始,2段结束)..."1段结束"值=="2段开始"值
				break;
		}
		if(i<cur_time_zone_num){
			time_zone_index = i + 1;//since 1 
		}else{//最后1个时段
			time_zone_index = cur_time_zone_num;
		}
		cur_price_index = Globa_1->Special_price_data.time_zone_feilv[time_zone_index -1];//取得第1个时间段的费率号
	}else{//通用时区
		cur_time_zone_num = Globa_1->Charger_param2.time_zone_num;
		for(i=0;i<cur_time_zone_num;i++){
			if((cur_time >= Globa_1->Charger_param2.time_zone_tabel[i]) && (cur_time < Globa_1->Charger_param2.time_zone_tabel[(i+1)%cur_time_zone_num]))//开始点为闭区间,可以取等[1段开始,1段结束)，[2段开始,2段结束)..."1段结束"值=="2段开始"值
				break;
		}
		if(i<cur_time_zone_num){
			time_zone_index = i + 1;//since 1 
		}else{//最后1个时段
			time_zone_index = cur_time_zone_num;
		}
		cur_price_index = Globa_1->Charger_param2.time_zone_feilv[time_zone_index -1];//取得第1个时间段的费率号
	}	
	return cur_price_index;
}
static void QT_timer_start(int time)
{
	switch(time){
		case 500:{
			QT_count_500ms = 0;
			QT_count_500ms_flag = 0;
  		QT_count_500ms_enable = 1;

			break;
		}
		case 5000:{
			QT_count_5000ms = 0;
			QT_count_5000ms_flag = 0;
  		QT_count_5000ms_enable = 1;

			break;
		}
		default:{
			break;
		}
	}
}
static void QT_timer_stop(int time)
{
	switch(time){
		case 500:{
			QT_count_500ms = 0;
			QT_count_500ms_flag = 0;
  		QT_count_500ms_enable = 0;

			break;
		}
		case 5000:{
			QT_count_5000ms = 0;
			QT_count_5000ms_flag = 0;
  		QT_count_5000ms_enable = 0;

			break;
		}

		default:{
			break;
		}
	}
}
static UINT32 QT_timer_tick(int time)
{
	UINT32 flag=0;

	switch(time){
		case 500:{
			flag = QT_count_500ms_flag;

			break;
		}
		case 5000:{
			flag = QT_count_5000ms_flag;

			break;
		}
		default:{
			break;
		}
	}

	return (flag);
}

// Voicechip_operation_function(0xE7); --调节声音的
/*************************语音芯片操作*****************************************/
void Voicechip_operation_function(UINT8 addr)
{
	UINT32 i;

	VO_CHIP_SELECT_L;
	usleep(5000);//5MS

	for(i =0;i<8;i++){
		VO_CLOCK_SIGNAL_L;
		if(((addr>>i)&0x01) == 1){
			VO_DATA_SIGNAL_H ;
		}else{
			VO_DATA_SIGNAL_L;
		}

		usleep(150);//150US
		VO_CLOCK_SIGNAL_H;
		usleep(150);//150US
	}
	
	usleep(5000);//5MS

	VO_DATA_SIGNAL_H;//data cs clk要保持高电平
	VO_CHIP_SELECT_H;
}

//查询卡片是否为黑名单中的卡号
extern int valid_card_check(unsigned char *sn)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;
	int id;

	char sql[512];

	rc = sqlite3_open_v2(F_CHARGER_CARD_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		sqlite3_close(db);
		return -1;
	}
	sprintf(sql, "SELECT MAX(ID) FROM Card_DB WHERE card_sn = '%.16s' ;", sn);
	if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL)){
		if(azResult[ncolumn*1+0] != NULL){//2行1列
			id = atoi(azResult[ncolumn*1+0]);
			sqlite3_free_table(azResult);
			sqlite3_close(db);
			return (id );
		}else{
			//释放掉 azResult 的内存空间 //不论数据库查询是否成功，都释放 char** 查询结果，使用 sqlite 提供的功能来释放
			sqlite3_free_table(azResult );
		}
	}else{
		//执行失败
		//释放掉 azResult 的内存空间 //不论数据库查询是否成功，都释放 char** 查询结果，使用 sqlite 提供的功能来释放
		sqlite3_free_table(azResult );
	}
	sqlite3_close(db);
	return -1;
}

static int Insert_Charger_Busy_DB_1(ISO8583_AP_charg_busy_Frame *frame, unsigned int *id, unsigned int up_num)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;

//	char sql[512];
  char sql[800];
	rc = sqlite3_open_v2(F_CHARGER_1_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE |
									     SQLITE_OPEN_CREATE,
									     NULL);
	if(rc){
		//printf("Can't create F_CHARGER_1_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		if(DEBUGLOG) RUN_GUN1_LogOut("%s","打开数据库失败:");  
		return -1;
	}

	sprintf(sql,"CREATE TABLE if not exists Charger_Busy_Complete_Data(ID INTEGER PRIMARY KEY autoincrement, card_sn TEXT, flag TEXT, rmb TEXT, kwh TEXT, server_rmb TEXT,total_rmb TEXT, type TEXT, Serv_Type TEXT, Busy_SN TEXT, last_rmb TEXT, ter_sn TEXT, start_kwh TEXT, end_kwh TEXT, rmb_kwh TEXT, sharp_rmb_kwh TEXT, sharp_allrmb TEXT, peak_rmb_kwh TEXT, peak_allrmb TEXT, flat_rmb_kwh TEXT, flat_allrmb TEXT, valley_rmb_kwh TEXT, valley_allrmb TEXT,sharp_rmb_kwh_Serv TEXT, sharp_allrmb_Serv TEXT, peak_rmb_kwh_Serv TEXT, peak_allrmb_Serv TEXT, flat_rmb_kwh_Serv TEXT, flat_allrmb_Serv TEXT, valley_rmb_kwh_Serv TEXT, valley_allrmb_Serv TEXT,end_time TEXT, car_sn TEXT, start_time TEXT, ConnectorId TEXT, End_code TEXT, up_num INTEGER);");
			
	if(sqlite3_exec(db, sql, 0, 0, &zErrMsg) != SQLITE_OK){
		//printf("CREATE TABLE Charger_Busy_Complete_Data error: %s\n", sqlite3_errmsg(db));
		sqlite3_free(zErrMsg);
	}

	if(TRANSACTION_FLAG) sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);
	
	sprintf(sql, "INSERT INTO Charger_Busy_Complete_Data VALUES (NULL,'%.16s','%.2s','%.8s','%.8s','%.8s','%.8s','%.2s','%.2s','%.20s','%.8s','%.12s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.14s', '%.17s', '%.14s', '%.2s', '%.2s', '%d');",frame->card_sn, frame->flag, frame->rmb, frame->kwh, frame->server_rmb, frame->total_rmb, frame->type, frame->Serv_Type, frame->Busy_SN, frame->last_rmb, frame->ter_sn, frame->start_kwh,frame->end_kwh, frame->rmb_kwh, frame->sharp_rmb_kwh, frame->sharp_allrmb, frame->peak_rmb_kwh, frame->peak_allrmb, frame->flat_rmb_kwh, frame->flat_allrmb,frame->valley_rmb_kwh, frame->valley_allrmb,frame->sharp_rmb_kwh_Serv, frame->sharp_allrmb_Serv,frame->peak_rmb_kwh_Serv,frame->peak_allrmb_Serv,frame->flat_rmb_kwh_Serv,frame->flat_allrmb_Serv,frame->valley_rmb_kwh_Serv, frame->valley_allrmb_Serv,frame->end_time,frame->car_sn,frame->start_time,frame->ConnectorId,frame->End_code,up_num);
  

 	if(DEBUGLOG) RUN_GUN1_LogOut("1号枪新建记录：%s",sql);  
	if(sqlite3_exec(db , sql , 0 , 0 , &zErrMsg) != SQLITE_OK){
		printf("Insert DI error: %s\n", sqlite3_errmsg(db));
		sqlite3_free(zErrMsg);
		if(TRANSACTION_FLAG) sqlite3_exec(db, "rollback transaction", NULL, NULL, NULL);
	  //if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
		if(DEBUGLOG) RUN_GUN1_LogOut("%s %s","插入命令执行失败枪号:",frame->ConnectorId);  
		sqlite3_close(db);
		return -1;
	}

	sprintf(sql, "SELECT MAX(ID) FROM Charger_Busy_Complete_Data WHERE card_sn = '%.16s';", frame->card_sn);
	if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL)){
		if(azResult[ncolumn*1+0] != NULL){//2行1列
			*id = atoi(azResult[ncolumn*1+0]);
			sqlite3_free_table(azResult);
			if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
			sqlite3_close(db);
			return (*id);
		}else{
			//释放掉 azResult 的内存空间 //不论数据库查询是否成功，都释放 char** 查询结果，使用 sqlite 提供的功能来释放
			sqlite3_free_table(azResult );
		}
	}else{
		//执行失败
		//释放掉 azResult 的内存空间 //不论数据库查询是否成功，都释放 char** 查询结果，使用 sqlite 提供的功能来释放
		sqlite3_free_table(azResult );
	}
	if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
	sqlite3_close(db);
	return -1;
}

static int Update_Charger_Busy_DB_1(unsigned int id, ISO8583_AP_charg_busy_Frame *frame, unsigned int flag, unsigned int up_num)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;
  char car_sn[18]={'\0'};

	char sql[800];

	rc = sqlite3_open_v2(F_CHARGER_1_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE |
									     SQLITE_OPEN_CREATE,
									     NULL);
	if(rc){
		printf("Can't create F_CHARGER_1_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}
	
	sprintf(sql,"CREATE TABLE if not exists Charger_Busy_Complete_Data(ID INTEGER PRIMARY KEY autoincrement, card_sn TEXT, flag TEXT, rmb TEXT, kwh TEXT, server_rmb TEXT,total_rmb TEXT, type TEXT, Serv_Type TEXT, Busy_SN TEXT, last_rmb TEXT, ter_sn TEXT, start_kwh TEXT, end_kwh TEXT, rmb_kwh TEXT, sharp_rmb_kwh TEXT, sharp_allrmb TEXT, peak_rmb_kwh TEXT, peak_allrmb TEXT, flat_rmb_kwh TEXT, flat_allrmb TEXT, valley_rmb_kwh TEXT, valley_allrmb TEXT,sharp_rmb_kwh_Serv TEXT, sharp_allrmb_Serv TEXT, peak_rmb_kwh_Serv TEXT, peak_allrmb_Serv TEXT, flat_rmb_kwh_Serv TEXT, flat_allrmb_Serv TEXT, valley_rmb_kwh_Serv TEXT, valley_allrmb_Serv TEXT,end_time TEXT, car_sn TEXT, start_time TEXT, ConnectorId TEXT, End_code TEXT, up_num INTEGER);");
			 
	if(sqlite3_exec(db, sql, 0, 0, &zErrMsg) != SQLITE_OK){
		sqlite3_free(zErrMsg);
	}
	if(TRANSACTION_FLAG) sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);
  
	if(id > 0){
		if((frame->car_sn[0] == 0xFF)&&(frame->car_sn[1] == 0xFF)&&(frame->car_sn[2] == 0xFF)){
		  sprintf(&car_sn,"%s","00000000000000000");
		}else{
      memcpy(&car_sn, &frame->car_sn, sizeof(car_sn)-1);
		}
		sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET rmb = '%.8s', kwh = '%.8s', rmb_kwh = '%.8s', server_rmb = '%.8s', total_rmb = '%.8s', sharp_allrmb= '%.8s', peak_allrmb= '%.8s', flat_allrmb= '%.8s', valley_allrmb= '%.8s', sharp_allrmb_Serv= '%.8s', peak_allrmb_Serv= '%.8s', flat_allrmb_Serv= '%.8s', valley_allrmb_Serv= '%.8s',start_kwh = '%.8s',end_kwh = '%.8s', end_time = '%.14s',flag = '%.2s',End_code = '%.2s',car_sn = '%.17s',up_num = '%d' WHERE ID = '%d' ;",frame->rmb, frame->kwh, frame->rmb_kwh, frame->server_rmb, frame->total_rmb, frame->sharp_allrmb, frame->peak_allrmb, frame->flat_allrmb,frame->valley_allrmb, frame->sharp_allrmb_Serv, frame->peak_allrmb_Serv, frame->flat_allrmb_Serv,frame->valley_allrmb_Serv, frame->start_kwh,frame->end_kwh, frame->end_time,frame->flag,frame->End_code,car_sn, up_num, id);
		
		if(up_num == 0){//最后一次更新数据价格
			if(DEBUGLOG) RUN_GUN1_LogOut("1号枪更新记录：%s",sql);  
		}
		
		if(sqlite3_exec(db, sql, 0, 0, &zErrMsg) != SQLITE_OK){
  		printf("UPDATE Charger_Busy_Complete_Data error: %s\n", sqlite3_errmsg(db));
  		sqlite3_free(zErrMsg);
		if(TRANSACTION_FLAG)	sqlite3_exec(db, "rollback transaction", NULL, NULL, NULL);
	   //if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
			sqlite3_close(db);
			if(DEBUGLOG) RUN_GUN1_LogOut("%s %s","充电中更新数据失败:",sql);  
			return -1;
  	}
	}
	if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
	sqlite3_close(db);
	return 0;
}


//查询有没有正常结束的
static int Select_NO_Over_Record_1(int id,int up_num)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;


	char sql[512];

	rc = sqlite3_open_v2(F_CHARGER_1_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		//printf("Can't create F_CHARGER_1_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	sprintf(sql, "SELECT MAX(ID) FROM Charger_Busy_Complete_Data WHERE id = '%d' AND (up_num = 85 OR up_num = 102) AND up_num != '%d';",id,up_num);

	if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL)){
		if(azResult[ncolumn*1+0] != NULL){//2行1列
			sqlite3_free_table(azResult);
			sqlite3_close(db);
			return 0;
		}else{
			//释放掉 azResult 的内存空间 //不论数据库查询是否成功，都释放 char** 查询结果，使用 sqlite 提供的功能来释放
			sqlite3_free_table(azResult );
		}
	}else{
		//执行失败
		//释放掉 azResult 的内存空间 //不论数据库查询是否成功，都释放 char** 查询结果，使用 sqlite 提供的功能来释放
		sqlite3_free_table(azResult );
	}

	sqlite3_close(db);
	return -1;
}

//删除该记录中已经存入到总表的数据
static int Delete_Record_busy_DB(unsigned int id)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;

	char sql[512];


	rc = sqlite3_open_v2(F_CHARGER_1_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		sqlite3_close(db);
		return -1;
	}

	sprintf(sql, "DELETE FROM Charger_Busy_Complete_Data WHERE ID = '%d';", id);
  if(sqlite3_exec(db , sql , 0, 0, &zErrMsg) != SQLITE_OK){
		sqlite3_free(zErrMsg);
		return 0;
	}else{
		sqlite3_close(db);
	}
	sqlite3_close(db);
	return -1;
}

//查询还没有上传成功的记录 ID最大值
static int Select_Busy_NO_insert_1(ISO8583_AP_charg_busy_Frame *frame)//查询需要上传的业务记录的最大 ID 值
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;

	int id = 0;

	char sql[512];


	rc = sqlite3_open_v2(F_CHARGER_1_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		//printf("Can't create F_CHARGER_1_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	/*sprintf(sql, "SELECT count (*)  FROM Charger_Busy_Complete_Data;");
	if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL)){
		if(ncolumn > 0){
		}else{
			sqlite3_free_table(azResult);
		}
	}else{
		sqlite3_free_table(azResult);
	}*/
	sprintf(sql, "SELECT MIN(ID) FROM Charger_Busy_Complete_Data WHERE  up_num < '%d' ;", MAX_NUM_BUSY_UP_TIME);
	if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL)){
		if(azResult[ncolumn*1+0] != NULL){//2行1列
			id = atoi(azResult[ncolumn*1+0]);
			sqlite3_free_table(azResult);
			sprintf(sql, "SELECT * FROM Charger_Busy_Complete_Data WHERE ID = '%d';", id);
			if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL)){
				if(azResult[ncolumn*1+0] != NULL){//2行1列
					id = atoi(azResult[ncolumn*1+0]);
					memcpy(&frame->card_sn[0], azResult[ncolumn*1+1], sizeof(frame->card_sn));
					memcpy(&frame->flag[0], azResult[ncolumn*1+2], sizeof(frame->flag));
					memcpy(&frame->rmb[0], azResult[ncolumn*1+3], sizeof(frame->rmb));
					memcpy(&frame->kwh[0], azResult[ncolumn*1+4], sizeof(frame->kwh));
					memcpy(&frame->server_rmb[0], azResult[ncolumn*1+5], sizeof(frame->server_rmb));
					memcpy(&frame->total_rmb[0], azResult[ncolumn*1+6], sizeof(frame->total_rmb));
					memcpy(&frame->type[0], azResult[ncolumn*1+7], sizeof(frame->type));
					memcpy(&frame->Serv_Type[0], azResult[ncolumn*1+8], sizeof(frame->Serv_Type));
					memcpy(&frame->Busy_SN[0], azResult[ncolumn*1+9], sizeof(frame->Busy_SN));
					memcpy(&frame->last_rmb[0], azResult[ncolumn*1+10], sizeof(frame->last_rmb));
					memcpy(&frame->ter_sn[0], azResult[ncolumn*1+11], sizeof(frame->ter_sn));
					memcpy(&frame->start_kwh[0], azResult[ncolumn*1+12], sizeof(frame->start_kwh));
					memcpy(&frame->end_kwh[0], azResult[ncolumn*1+13], sizeof(frame->end_kwh));
					memcpy(&frame->rmb_kwh[0], azResult[ncolumn*1+14], sizeof(frame->rmb_kwh));

				
					memcpy(&frame->sharp_rmb_kwh[0], azResult[ncolumn*1+15], sizeof(frame->sharp_rmb_kwh));
					memcpy(&frame->sharp_allrmb[0], azResult[ncolumn*1+16], sizeof(frame->sharp_allrmb));
					memcpy(&frame->peak_rmb_kwh[0], azResult[ncolumn*1+17], sizeof(frame->peak_rmb_kwh));
					memcpy(&frame->peak_allrmb[0], azResult[ncolumn*1+18], sizeof(frame->peak_allrmb));
					memcpy(&frame->flat_rmb_kwh[0], azResult[ncolumn*1+19], sizeof(frame->flat_rmb_kwh));
					memcpy(&frame->flat_allrmb[0], azResult[ncolumn*1+20], sizeof(frame->flat_allrmb));
					memcpy(&frame->valley_rmb_kwh[0], azResult[ncolumn*1+21], sizeof(frame->valley_rmb_kwh));
					memcpy(&frame->valley_allrmb[0], azResult[ncolumn*1+22], sizeof(frame->valley_allrmb));
					
					memcpy(&frame->sharp_rmb_kwh_Serv[0], azResult[ncolumn*1+23], sizeof(frame->sharp_rmb_kwh_Serv));
					memcpy(&frame->sharp_allrmb_Serv[0], azResult[ncolumn*1+24], sizeof(frame->sharp_allrmb_Serv));
					memcpy(&frame->peak_rmb_kwh_Serv[0], azResult[ncolumn*1+25], sizeof(frame->peak_rmb_kwh_Serv));
					memcpy(&frame->peak_allrmb_Serv[0], azResult[ncolumn*1+26], sizeof(frame->peak_allrmb_Serv));
					memcpy(&frame->flat_rmb_kwh_Serv[0], azResult[ncolumn*1+27], sizeof(frame->flat_rmb_kwh_Serv));
					memcpy(&frame->flat_allrmb_Serv[0], azResult[ncolumn*1+28], sizeof(frame->flat_allrmb_Serv));
					memcpy(&frame->valley_rmb_kwh_Serv[0], azResult[ncolumn*1+29], sizeof(frame->valley_rmb_kwh_Serv));
					memcpy(&frame->valley_allrmb_Serv[0], azResult[ncolumn*1+30], sizeof(frame->valley_allrmb_Serv));
					
					memcpy(&frame->end_time[0], azResult[ncolumn*1+31], sizeof(frame->end_time));
					memcpy(&frame->car_sn[0], azResult[ncolumn*1+32], sizeof(frame->car_sn));
					memcpy(&frame->start_time[0], azResult[ncolumn*1+33], sizeof(frame->start_time));
					memcpy(&frame->ConnectorId[0], azResult[ncolumn*1+34], sizeof(frame->ConnectorId));
					memcpy(&frame->End_code[0], azResult[ncolumn*1+35], sizeof(frame->End_code));
					
					sqlite3_free_table(azResult);
					sqlite3_close(db);
					return (id);
				}else{
					sqlite3_free_table(azResult);
				}
			}else{
				sqlite3_free_table(azResult);
			}
		}else{
			sqlite3_free_table(azResult);
		}
	}else{
		sqlite3_free_table(azResult);
	}

	sqlite3_close(db);
	return -1;
}

static int Set_0X550X66_TO0X00_Busy_ID_Gun_1(int gun)
{ 
  sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;
  char ls_gun[3]={0};
	char sql[512];
	rc = sqlite3_open_v2(F_CHARGER_1_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		printf("Can't create F_CHARGER_1_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}
	
	if(TRANSACTION_FLAG) sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);
	
	sprintf(ls_gun,"%.2d",gun);
	sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET up_num = 0 WHERE  ConnectorId = '%.2s' AND up_num >= 85 AND up_num <= 102;",ls_gun);
	
	if(sqlite3_exec(db , sql , 0, 0, &zErrMsg) != SQLITE_OK){
		sqlite3_free(zErrMsg);
	 if(TRANSACTION_FLAG) sqlite3_exec(db, "rollback transaction", NULL, NULL, NULL);
		sqlite3_close(db);
		//printf("UPDATE Set_0X550X66_TO0X00_Busy_ID_Gun err  : %s\n",sql);
		return -1;
	}

 if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
	sqlite3_close(db);
	return 0;
}


//查询以外掉电的不完整数据记录
static int Select_0X55_0X66_Busy_ID(ISO8583_AP_charg_busy_Frame *frame, unsigned int *up_num)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;
	int id;

	char sql[512];

	rc = sqlite3_open_v2(F_CHARGER_1_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		printf("Can't create F_CHARGER_1_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}
	
	printf("xxxxxxxxxxxxxxxxxxx Select_0X55_0X66_Busy_ID failed\n");

	sprintf(sql, "SELECT MAX(ID) FROM Charger_Busy_Complete_Data WHERE (up_num = 85 OR up_num = 102);");
	if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL)){
		if(azResult[ncolumn*1+0] != NULL){//2行1列
			id = atoi(azResult[ncolumn*1+0]);
			sqlite3_free_table(azResult);
				printf("xxxxxxxxxxxxxxxxxxx Select_0X55_0X66_Busy_ID -----------\n");

			sprintf(sql, "SELECT * FROM Charger_Busy_Complete_Data WHERE ID = '%d';", id);
			if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL)){
				if(azResult[ncolumn*1+0] != NULL){//2行1列
					id = atoi(azResult[ncolumn*1+0]);
					memcpy(&frame->card_sn[0], azResult[ncolumn*1+1], sizeof(frame->card_sn));
					memcpy(&frame->flag[0], azResult[ncolumn*1+2], sizeof(frame->flag));
					memcpy(&frame->rmb[0], azResult[ncolumn*1+3], sizeof(frame->rmb));
					memcpy(&frame->kwh[0], azResult[ncolumn*1+4], sizeof(frame->kwh));
					memcpy(&frame->server_rmb[0], azResult[ncolumn*1+5], sizeof(frame->server_rmb));
					memcpy(&frame->total_rmb[0], azResult[ncolumn*1+6], sizeof(frame->total_rmb));
					memcpy(&frame->type[0], azResult[ncolumn*1+7], sizeof(frame->type));
					memcpy(&frame->Serv_Type[0], azResult[ncolumn*1+8], sizeof(frame->Serv_Type));
					memcpy(&frame->Busy_SN[0], azResult[ncolumn*1+9], sizeof(frame->Busy_SN));
					memcpy(&frame->last_rmb[0], azResult[ncolumn*1+10], sizeof(frame->last_rmb));
					memcpy(&frame->ter_sn[0], azResult[ncolumn*1+11], sizeof(frame->ter_sn));
					memcpy(&frame->start_kwh[0], azResult[ncolumn*1+12], sizeof(frame->start_kwh));
					memcpy(&frame->end_kwh[0], azResult[ncolumn*1+13], sizeof(frame->end_kwh));
					memcpy(&frame->rmb_kwh[0], azResult[ncolumn*1+14], sizeof(frame->rmb_kwh));

				
					memcpy(&frame->sharp_rmb_kwh[0], azResult[ncolumn*1+15], sizeof(frame->sharp_rmb_kwh));
					memcpy(&frame->sharp_allrmb[0], azResult[ncolumn*1+16], sizeof(frame->sharp_allrmb));
					memcpy(&frame->peak_rmb_kwh[0], azResult[ncolumn*1+17], sizeof(frame->peak_rmb_kwh));
					memcpy(&frame->peak_allrmb[0], azResult[ncolumn*1+18], sizeof(frame->peak_allrmb));
					memcpy(&frame->flat_rmb_kwh[0], azResult[ncolumn*1+19], sizeof(frame->flat_rmb_kwh));
					memcpy(&frame->flat_allrmb[0], azResult[ncolumn*1+20], sizeof(frame->flat_allrmb));
					memcpy(&frame->valley_rmb_kwh[0], azResult[ncolumn*1+21], sizeof(frame->valley_rmb_kwh));
					memcpy(&frame->valley_allrmb[0], azResult[ncolumn*1+22], sizeof(frame->valley_allrmb));
					
					memcpy(&frame->sharp_rmb_kwh_Serv[0], azResult[ncolumn*1+23], sizeof(frame->sharp_rmb_kwh_Serv));
					memcpy(&frame->sharp_allrmb_Serv[0], azResult[ncolumn*1+24], sizeof(frame->sharp_allrmb_Serv));
					memcpy(&frame->peak_rmb_kwh_Serv[0], azResult[ncolumn*1+25], sizeof(frame->peak_rmb_kwh_Serv));
					memcpy(&frame->peak_allrmb_Serv[0], azResult[ncolumn*1+26], sizeof(frame->peak_allrmb_Serv));
					memcpy(&frame->flat_rmb_kwh_Serv[0], azResult[ncolumn*1+27], sizeof(frame->flat_rmb_kwh_Serv));
					memcpy(&frame->flat_allrmb_Serv[0], azResult[ncolumn*1+28], sizeof(frame->flat_allrmb_Serv));
					memcpy(&frame->valley_rmb_kwh_Serv[0], azResult[ncolumn*1+29], sizeof(frame->valley_rmb_kwh_Serv));
					memcpy(&frame->valley_allrmb_Serv[0], azResult[ncolumn*1+30], sizeof(frame->valley_allrmb_Serv));
					
					memcpy(&frame->end_time[0], azResult[ncolumn*1+31], sizeof(frame->end_time));
					memcpy(&frame->car_sn[0], azResult[ncolumn*1+32], sizeof(frame->car_sn));
					memcpy(&frame->start_time[0], azResult[ncolumn*1+33], sizeof(frame->start_time));
					memcpy(&frame->ConnectorId[0], azResult[ncolumn*1+34], sizeof(frame->ConnectorId));
					memcpy(&frame->End_code[0], azResult[ncolumn*1+35], sizeof(frame->End_code));

					*up_num = atoi(azResult[ncolumn*1+36]);
				  printf("xxxxxxxxxxxxxxxxxxx Select_0X55_0X66_ ID =%d -----------\n",id);

					sqlite3_free_table(azResult);
					sqlite3_close(db);
					return (id);
				}else{
					sqlite3_free_table(azResult);
				}
			}else{
				sqlite3_free_table(azResult);
			}
		}else{
			//释放掉 azResult 的内存空间 //不论数据库查询是否成功，都释放 char** 查询结果，使用 sqlite 提供的功能来释放
			sqlite3_free_table(azResult );
		}
	}else{
		//释放掉 azResult 的内存空间 //不论数据库查询是否成功，都释放 char** 查询结果，使用 sqlite 提供的功能来释放
		sqlite3_free_table(azResult );
		printf("xxxxxxxxxxxxxxxxxxx Select_0X55_0X66_Busy_ID failed\n");
	}


	sqlite3_close(db);
	return -1;
}

//更新以外掉电的不完整数据记录
static int Set_0X55_Busy_ID(ISO8583_AP_charg_busy_Frame *frame,int up_num, int id)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;

	char sql[512];

	rc = sqlite3_open_v2(F_CHARGER_1_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		printf("Can't create F_CHARGER_1_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);

	//sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET rmb = '%.8s', kwh = '%.8s', total_rmb = '%.8s', end_kwh = '%.8s',up_num = '%d' WHERE ID = '%d';",frame->rmb, frame->kwh, frame->total_rmb, frame->end_kwh, 0x00, id);
  sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET rmb = '%.8s', kwh = '%.8s', rmb_kwh = '%.8s', server_rmb = '%.8s', total_rmb = '%.8s', sharp_allrmb= '%.8s', peak_allrmb= '%.8s', flat_allrmb= '%.8s', valley_allrmb= '%.8s', sharp_allrmb_Serv= '%.8s', peak_allrmb_Serv= '%.8s', flat_allrmb_Serv= '%.8s', valley_allrmb_Serv= '%.8s',start_kwh = '%.8s',end_kwh = '%.8s', end_time = '%.14s',End_code = '%.2s',up_num = '%d' WHERE ID = '%d' ;",frame->rmb, frame->kwh, frame->rmb_kwh, frame->server_rmb, frame->total_rmb, frame->sharp_allrmb, frame->peak_allrmb, frame->flat_allrmb,frame->valley_allrmb, frame->sharp_allrmb_Serv, frame->peak_allrmb_Serv, frame->flat_allrmb_Serv,frame->valley_allrmb_Serv, frame->start_kwh,frame->end_kwh, frame->end_time,frame->End_code, up_num, id);

//	sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET up_num = '%d' WHERE ID = '%d';", 0x00, id);																											
	if(sqlite3_exec(db, sql, 0, 0, &zErrMsg) != SQLITE_OK){
		printf("UPDATE Charger_Busy_Complete_Data error: %s\n", sqlite3_errmsg(db));
		sqlite3_free(zErrMsg);
		
		sqlite3_exec(db, "rollback transaction", NULL, NULL, NULL);

		sqlite3_close(db);
		return -1;
	}

	sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);

	sqlite3_close(db);
	return 0;
}

static int Set_0X66_Busy_ID(int id)//
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;

	char sql[512];

	rc = sqlite3_open_v2(F_CHARGER_1_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		printf("Can't create F_CHARGER_1_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);

	sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET up_num = '%d' WHERE ID = '%d';", 0x00, id);
	if(sqlite3_exec(db , sql , 0, 0, &zErrMsg) != SQLITE_OK){
		sqlite3_free(zErrMsg);
		
		sqlite3_exec(db, "rollback transaction", NULL, NULL, NULL);

		sqlite3_close(db);
		return -1;
	}
	
	sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);

	sqlite3_close(db);
	return 0;
}

static int Set_0X550X66_Busy_ID()
{ 
  sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;

	char sql[512];
	rc = sqlite3_open_v2(F_CHARGER_1_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		printf("Can't create F_CHARGER_1_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}
	sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);
	sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET up_num = '%d' WHERE up_num = 85 OR up_num = 102 OR up_num = 180;", 0x00);
	if(sqlite3_exec(db , sql , 0, 0, &zErrMsg) != SQLITE_OK){
		sqlite3_free(zErrMsg);
		
		sqlite3_exec(db, "rollback transaction", NULL, NULL, NULL);

		sqlite3_close(db);
		return -1;
	}
	sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
	sqlite3_close(db);
	return 0;
}


int ISorder_chg_control()
{
	time_t systime=0;
	struct tm tm_t;
	systime = time(NULL);        //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
	unsigned int cur_time,End_time_on = 0;//当前时间=分来计数的
	cur_time = tm_t.tm_hour*60 +	tm_t.tm_min	;//运行的当前时间	
	End_time_on = Globa_1->order_card_end_time[0]*60 + Globa_1->order_card_end_time[1];//	

	if(cur_time >= End_time_on){//先部用考虑跨天的情况
		return 1;
	}else{
		return 0;
	}
}


//灯箱控制00：00---00:00  ---表示全天都是关闭状态。
void Light_control()
{
	time_t systime=0;
	struct tm tm_t;
	systime = time(NULL);        //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
	unsigned int cur_time,Start_time_on = 0,End_time_on = 0;//当前时间=分来计数的
	cur_time = tm_t.tm_hour*60 +	tm_t.tm_min	;//运行的当前时间	
	Start_time_on = Globa_1->Charger_param.Light_Control_Data.Start_time_on_hour*60 + Globa_1->Charger_param.Light_Control_Data.Start_time_on_min;	
	End_time_on = Globa_1->Charger_param.Light_Control_Data.End_time_off_hour*60 + Globa_1->Charger_param.Light_Control_Data.End_time_off_min;//	

	if((Start_time_on == 0)&&(End_time_on == 0)){
		 LIGHT_CONTROL_Power_OFF; 
	}else{
		if(Start_time_on < End_time_on){//当天
		  if((cur_time >= Start_time_on)&&(cur_time <= End_time_on)){
				//printf("Start_time_on < End_time_on (cur_time >= Start_time_on)&&(cur_time <= End_time_on)\n ");
				LIGHT_CONTROL_Power_ON; 
			}else{//全天开启
			//	printf("Start_time_on < End_time_on 11111111111111111\n ");
				LIGHT_CONTROL_Power_OFF; 
			}
		}else if(Start_time_on == End_time_on){//全天开启
			LIGHT_CONTROL_Power_OFF;
		}else if(Start_time_on > End_time_on){//跨天的时候
      if((cur_time <= End_time_on)||(cur_time >= Start_time_on)){
				//LIGHT_CONTROL_Power_OFF;
			//	printf("Start_time_on < End_time_on2222222222222222222\n ");
				LIGHT_CONTROL_Power_ON; 
			}else{
				//LIGHT_CONTROL_Power_ON;
				//printf("Start_time_on < End_time_on 33333333333333333333\n ");
				LIGHT_CONTROL_Power_OFF;
			}
		}
	}
}

/*旧
00H	"欢迎光临.wav 300s 请刷卡1.wav"			
01H	请插入充电枪并确认连接状态.wav			
02H	系统忙暂停使用.wav		//意义不大	
03H	系统故障暂停使用.wav			
04H	刷卡成功 新
05H	10s			
06H	10s			
07H	10s			
08H	10s			
09H	10s			
0AH	10s			
0BH	10s			
0CH	10s			
0DH	10s			
0EH	10s			
0FH	10s			
10H	请刷卡结算.wav			
11H	谢谢使用再见.wav			
*/

/*新
正常充电提示音:
00H     “欢迎光临”OK
01H     “请选择充电枪 ”OK
02H"    “请选择充电方式"  +
03H     “请插入充电枪，并确认连接状态 ” 连接状态发音急促
04H     “请刷卡”OK
05H     “系统自检中，请稍候”+
06H     “等待车辆连接中“OK
07H     “充电已完成，请刷卡结算”+
08H     “结算成功，请取下充电枪并挂回原处“+
09H     “谢谢惠顾，再见”+
刷卡处理提示音:
刷卡异常处理提示音：
0aH     “余额不足，请充值”OK
0bH     “卡片已灰锁，请解锁”+
0cH     “卡片已挂失，请联系发卡单位处理”+
0dH     “卡片未注册，请联系发卡单位处理”+
卡片处理提示音：
0eH     “请刷卡”OK
0fH     “操作中，请勿移开卡片”+
10H     “后台处理中，请取走卡片”+
11H     “卡片解锁成功”+
12H     “卡片解锁失败，请联系发卡单位处理“+
系统升级提升音:
13H    “系统维护中，暂停使用 ”+
异常处理提示应
14H    “系统故障，暂停使用 ”系发音过长
15H    “交流电压输入异常，请联系设备管理员检查线路”+
16H    “系统接地异常，请联系设备管理员检查线路”+
*/
static unsigned char GetTimePriceIndex(unsigned short cur_time)
{
	unsigned char cur_price_index = 0,i,cur_time_zone_num = 0,time_zone_index = 0;
	//unsigned short cur_time;//高字节为当前小时数，低字节为当前分钟数，BIN码
    cur_time_zone_num = Globa_1->Charger_param2.time_zone_num;
		for(i=0;i<cur_time_zone_num;i++){
			if((cur_time >= Globa_1->Charger_param2.time_zone_tabel[i]) && (cur_time < Globa_1->Charger_param2.time_zone_tabel[(i+1)%cur_time_zone_num]))//开始点为闭区间,可以取等[1段开始,1段结束)，[2段开始,2段结束)..."1段结束"值=="2段开始"值
				break;
		}
		if(i<cur_time_zone_num){
			time_zone_index = i + 1;//since 1 
		}else{//最后1个时段
			time_zone_index = cur_time_zone_num;
		}
		cur_price_index = Globa_1->Charger_param2.time_zone_feilv[time_zone_index -1];//取得第1个时间段的费率号
	  return cur_price_index;
}
int Main_Task_1()
{
	int fd, Ret=0, Ret1=0,Busy_NO_insert_id = -1;
	int all_id = 0;
	int Insertflag = 0;
	
	UINT32	IC_check_count1 = 0,card_falg_count = 0;
	UINT32 count_wait=0,count_wait1 = 0;
	UINT32 charger_type = 0;
	UINT32 temp1 = 0,i =0;
	UINT32 consume_flag = 0;
  UINT32 firmware_info_checkfile = 0; //检测升级文件是否存在
	UINT32 Upgrade_Firmware_time = 0;
	UINT32 cur_price_index = 0;  //当前处于收费电价阶段
  UINT32 pre_price_index = 0;  //之前处于的那个时刻
  
	UINT32 ul_start_kwh =0; //每个峰的起始时刻
  UINT32 g_ulcharged_kwh[4] = {0};
	UINT32 g_ulcharged_rmb[4] = {0}; //添加各个时区的电价总额。----By dengjh
	UINT32 g_ulcharged_rmb_Serv[4] = {0}; //添加各个时区的电价总额。----By dengjh
  
	UINT32 ul_start_kwh_2 =0; //每个峰的起始时刻---第二个枪的
  UINT32 g_ulcharged_kwh_2[4] = {0};
	UINT32 g_ulcharged_rmb_2[4] = {0}; //添加各个时区的电价总额。----By dengjh
	UINT32 g_ulcharged_rmb_Serv_2[4] = {0}; //添加各个时区的电价总额。----By dengjh
  
	UINT32 tmp_value = 0;

	UINT32 share_time_kwh_price[4] = {0};
	UINT32 share_time_kwh_price_serve[4] = {0}; //服务
  UINT32 Serv_Type = 0; //服务类型 
	UINT32 old_Updat_data_value = 0,oldSignals_value = 255;

	CHARGER_MSG msg;

	ISO8583_AP_charg_busy_Frame busy_frame,busy_frame_2;
	ISO8583_AP_charg_busy_Frame pre_old_busy_frame;//上次充电完成数据
	UINT8 card_sn[16];     //用户卡号
	UINT32 last_rmb = 0;   //充电前卡余额（以分为单位）
	UINT32 temp_rmb = 0,Select_NO_Over_count = 0;   //
	UINT32 temp_kwh = 0,temp_kwh_2 = 0;
	int id = -1,pre_old_id = 0,old_have_hart = 255,id_NO_2 = -1;
	UINT32 up_num = 0,log_old_tm_mday= 0,Bind_BmsVin_falg = 0;
  UINT32 QT_Step = 255,Select_Busy_count = 0;
	UINT8 temp_char[256];  //
  UINT8 Sing_pushbtton_count1 = 0;
	UINT8 Sing_pushbtton_count2 = 0;
	UINT32 kwh_charged_rmbkwh = 0,charged_rmb = 0, kwh_charged_rmb = 0, kwh_charged_rmbkwh_serv = 0,  charged_rmb_serv = 0,kwh_charged_rmb_serv = 0,delta_kwh = 0;
	time_t systime=0,log_time =0;
	UINT8 check_current_low_st = 0;//判断SOC达到设定限值后，电流低于设定值后停机
	time_t Current_Systime=0,current_low_timestamp = 0,meter_Curr_Threshold_count= 0,Electrical_abnormality  = 0,current_overkwh_timestamp = 0;
	struct tm tm_t,log_tm_t;
	
	prctl(PR_SET_NAME,(unsigned long)"QT1_Task");//设置线程名字 

	Globa_1->Manu_start = 0;
	Globa_1->QT_Step = 0x01;//主界面
  Globa_1->Card_charger_type = 0;

	GROUP1_FAULT_LED_OFF;

	Globa_1->charger_state = 0x03;  //终端状态 0x01-故障  0x02-告警  03-空闲 0x04-充电中 0x05-完成 0x06-预约 
	Globa_1->gun_state = 0x03;     //枪1状态 取值只有  03-空闲 0x04-充电中 0x05-完成 0x07-等待

  msg.type = 0x100;//主界面;
	msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);

	VO_CLOCK_SIGNAL_H;//data cs clk要保持高电平
	VO_DATA_SIGNAL_H;
	VO_CHIP_SELECT_H;
	VO_RESET_L; //拉低复位信号
	sleep(1);
	VO_RESET_H;
	sleep(1);

	Voicechip_operation_function(0xE6); //--调节声音的
	//发送二维码地址内容
	Page_90.flag = Globa_1->Charger_param.APP_enable;
	memcpy(&Page_90.sn[0], &Globa_1->Charger_param.SN[0], sizeof(Page_90.sn));
	snprintf(&Page_90.http[0], sizeof(Page_90.http), "%s", &Globa_1->Charger_param2.http[0]);
	msg.type = 0x90;
	memcpy(&msg.argv[0], &Page_90.reserve1, sizeof(PAGE_90_FRAME));
	msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_90_FRAME), IPC_NOWAIT);
	sleep(6);//等待电表通信正常

	//1: 查询以外掉电的不完整数据记录，然后根据条件更新数据记录
	pthread_mutex_lock(&busy_db_pmutex);
	id = Select_0X55_0X66_Busy_ID(&busy_frame, &up_num);//查询没有流水号的最大记录 ID 值
	if(id != -1){
		printf("xxxxxxxxxxxxxxxxxxxxxx   Select_0X55_0X66_Busy_ID ok id=%d up_num =%d Globa_1->Error.meter_connect_lost =%d \n", id,up_num,Globa_1->Error.meter_connect_lost);
		if((up_num == 0x55)||(up_num == 0x66)){
			if(Globa_1->Error.meter_connect_lost == 0){//表计通信正常
				sprintf(&temp_char[0], "%.8s", &busy_frame.end_kwh[0]);	
				temp_kwh = atoi(&temp_char[0]);
		    printf("xxxxxxxxxxxxxxxxxxxxxx   temp_kwh =%d \n", temp_kwh);

				if((Globa_1->meter_KWH >= temp_kwh)&&(Globa_1->meter_KWH < (temp_kwh+100))){//当前电量大于等于记录结束电量，且不大于1度电
					unsigned short last_end_hhmm = (busy_frame.end_time[8]-'0')*10+(busy_frame.end_time[9]-'0');//hour
			    last_end_hhmm <<= 8;
			    last_end_hhmm |= ((busy_frame.end_time[10]-'0')*10+(busy_frame.end_time[11]-'0'));//mm
			    cur_price_index = GetTimePriceIndex(last_end_hhmm);
					delta_kwh = Globa_1->meter_KWH - temp_kwh;		
					sprintf(&temp_char[0], "%.8s", &busy_frame.start_kwh[0]);	
				   printf("xxxxxxxxxxxxxxxxxxxxxx   cur_price_index =%d \n", cur_price_index);

					if(0 == cur_price_index)
					{
						sprintf(&temp_char[0], "%.8s", &busy_frame.sharp_allrmb[0]);	
					  kwh_charged_rmb =  atoi(&temp_char[0]);	
						sprintf(&temp_char[0], "%.8s", &busy_frame.sharp_rmb_kwh[0]);	
					  kwh_charged_rmbkwh =  atoi(&temp_char[0]);	
						charged_rmb = (delta_kwh*kwh_charged_rmbkwh)/100;
						kwh_charged_rmb += charged_rmb;
						
						sprintf(&temp_char[0], "%08d", kwh_charged_rmb);				
					  memcpy(&busy_frame.sharp_allrmb[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb)); //	更新分时价格，分时服务器价格	 	
						
						sprintf(&temp_char[0], "%.8s", &busy_frame.sharp_allrmb_Serv[0]);	
					  kwh_charged_rmb_serv =  atoi(&temp_char[0]);	
						sprintf(&temp_char[0], "%.8s", &busy_frame.sharp_rmb_kwh_Serv[0]);	
					  kwh_charged_rmbkwh_serv =  atoi(&temp_char[0]);	
						charged_rmb_serv = (delta_kwh*kwh_charged_rmbkwh_serv)/100;	
						kwh_charged_rmb_serv += charged_rmb_serv;	
						sprintf(&temp_char[0], "%08d", kwh_charged_rmb_serv);				
					  memcpy(&busy_frame.sharp_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb_Serv)); //	更新分时价格，分时服务器价格	 							
					
				    sprintf(&temp_char[0], "%.8s", &busy_frame.server_rmb[0]);	
					  temp_rmb = atoi(&temp_char[0]) + charged_rmb_serv;
					  sprintf(&temp_char[0], "%08d", temp_rmb);				
					  memcpy(&busy_frame.server_rmb[0], &temp_char[0], sizeof(busy_frame.server_rmb));
					 
					  sprintf(&temp_char[0], "%.8s", &busy_frame.rmb[0]);	
					  temp_rmb = atoi(&temp_char[0])  + charged_rmb;
					  sprintf(&temp_char[0], "%08d", temp_rmb);				
					  memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));
					 
					  sprintf(&temp_char[0], "%.8s", &busy_frame.total_rmb[0]);	
					  temp_rmb = atoi(&temp_char[0]) + charged_rmb_serv + charged_rmb;
					  sprintf(&temp_char[0], "%08d", temp_rmb);				
					  memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));
						
					}
					else if(1 == cur_price_index)
					{
							sprintf(&temp_char[0], "%.8s", &busy_frame.peak_allrmb[0]);	
					  kwh_charged_rmb =  atoi(&temp_char[0]);	
						sprintf(&temp_char[0], "%.8s", &busy_frame.peak_rmb_kwh[0]);	
					  kwh_charged_rmbkwh =  atoi(&temp_char[0]);	
						charged_rmb = (delta_kwh*kwh_charged_rmbkwh)/100;
						kwh_charged_rmb += charged_rmb;
						
						sprintf(&temp_char[0], "%08d", kwh_charged_rmb);				
					  memcpy(&busy_frame.peak_allrmb[0], &temp_char[0], sizeof(busy_frame.peak_allrmb)); //	更新分时价格，分时服务器价格	 	
						
						sprintf(&temp_char[0], "%.8s", &busy_frame.peak_allrmb_Serv[0]);	
					  kwh_charged_rmb_serv =  atoi(&temp_char[0]);	
						sprintf(&temp_char[0], "%.8s", &busy_frame.peak_rmb_kwh_Serv[0]);	
					  kwh_charged_rmbkwh_serv =  atoi(&temp_char[0]);	
						charged_rmb_serv = (delta_kwh*kwh_charged_rmbkwh_serv)/100;	
						kwh_charged_rmb_serv += charged_rmb_serv;	
						sprintf(&temp_char[0], "%08d", kwh_charged_rmb_serv);				
					  memcpy(&busy_frame.peak_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.peak_allrmb_Serv)); //	更新分时价格，分时服务器价格	 							
					
				    sprintf(&temp_char[0], "%.8s", &busy_frame.server_rmb[0]);	
					  temp_rmb = atoi(&temp_char[0]) + charged_rmb_serv;
					  sprintf(&temp_char[0], "%08d", temp_rmb);				
					  memcpy(&busy_frame.server_rmb[0], &temp_char[0], sizeof(busy_frame.server_rmb));
					 
					  sprintf(&temp_char[0], "%.8s", &busy_frame.rmb[0]);	
					  temp_rmb = atoi(&temp_char[0])  + charged_rmb;
					  sprintf(&temp_char[0], "%08d", temp_rmb);				
					  memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));
					 
					  sprintf(&temp_char[0], "%.8s", &busy_frame.total_rmb[0]);	
					  temp_rmb = atoi(&temp_char[0]) + charged_rmb_serv + charged_rmb;
					  sprintf(&temp_char[0], "%08d", temp_rmb);				
					  memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));						
					}
					else if(2 == cur_price_index)
					{
						sprintf(&temp_char[0], "%.8s", &busy_frame.flat_allrmb[0]);	
					  kwh_charged_rmb =  atoi(&temp_char[0]);	
						sprintf(&temp_char[0], "%.8s", &busy_frame.flat_rmb_kwh[0]);	
					  kwh_charged_rmbkwh =  atoi(&temp_char[0]);	
						charged_rmb = (delta_kwh*kwh_charged_rmbkwh)/100;
						kwh_charged_rmb += charged_rmb;
						
						sprintf(&temp_char[0], "%08d", kwh_charged_rmb);				
					  memcpy(&busy_frame.flat_allrmb[0], &temp_char[0], sizeof(busy_frame.flat_allrmb)); //	更新分时价格，分时服务器价格	 	
						
						sprintf(&temp_char[0], "%.8s", &busy_frame.flat_allrmb_Serv[0]);	
					  kwh_charged_rmb_serv =  atoi(&temp_char[0]);	
						sprintf(&temp_char[0], "%.8s", &busy_frame.flat_rmb_kwh_Serv[0]);	
					  kwh_charged_rmbkwh_serv =  atoi(&temp_char[0]);	
						charged_rmb_serv = (delta_kwh*kwh_charged_rmbkwh_serv)/100;	
						kwh_charged_rmb_serv += charged_rmb_serv;	
						sprintf(&temp_char[0], "%08d", kwh_charged_rmb_serv);				
					  memcpy(&busy_frame.flat_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.flat_allrmb_Serv)); //	更新分时价格，分时服务器价格	 							
					
				    sprintf(&temp_char[0], "%.8s", &busy_frame.server_rmb[0]);	
					  temp_rmb = atoi(&temp_char[0]) + charged_rmb_serv;
					  sprintf(&temp_char[0], "%08d", temp_rmb);				
					  memcpy(&busy_frame.server_rmb[0], &temp_char[0], sizeof(busy_frame.server_rmb));
					 
					  sprintf(&temp_char[0], "%.8s", &busy_frame.rmb[0]);	
					  temp_rmb = atoi(&temp_char[0])  + charged_rmb;
					  sprintf(&temp_char[0], "%08d", temp_rmb);				
					  memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));
					 
					  sprintf(&temp_char[0], "%.8s", &busy_frame.total_rmb[0]);	
					  temp_rmb = atoi(&temp_char[0]) + charged_rmb_serv + charged_rmb;
					  sprintf(&temp_char[0], "%08d", temp_rmb);				
					  memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));					
					}
					else if(3 == cur_price_index)
					{
						sprintf(&temp_char[0], "%.8s", &busy_frame.valley_allrmb[0]);	
					  kwh_charged_rmb =  atoi(&temp_char[0]);	
						sprintf(&temp_char[0], "%.8s", &busy_frame.valley_rmb_kwh[0]);	
					  kwh_charged_rmbkwh =  atoi(&temp_char[0]);	
						charged_rmb = (delta_kwh*kwh_charged_rmbkwh)/100;
						kwh_charged_rmb += charged_rmb;
						
						sprintf(&temp_char[0], "%08d", kwh_charged_rmb);				
					  memcpy(&busy_frame.valley_allrmb[0], &temp_char[0], sizeof(busy_frame.valley_allrmb)); //	更新分时价格，分时服务器价格	 	
						
						sprintf(&temp_char[0], "%.8s", &busy_frame.valley_allrmb_Serv[0]);	
					  kwh_charged_rmb_serv =  atoi(&temp_char[0]);	
						sprintf(&temp_char[0], "%.8s", &busy_frame.valley_rmb_kwh_Serv[0]);	
					  kwh_charged_rmbkwh_serv =  atoi(&temp_char[0]);	
						charged_rmb_serv = (delta_kwh*kwh_charged_rmbkwh_serv)/100;	
						kwh_charged_rmb_serv += charged_rmb_serv;	
						sprintf(&temp_char[0], "%08d", kwh_charged_rmb_serv);				
					  memcpy(&busy_frame.valley_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.valley_allrmb_Serv)); //	更新分时价格，分时服务器价格	 							
					
				    sprintf(&temp_char[0], "%.8s", &busy_frame.server_rmb[0]);	
					  temp_rmb = atoi(&temp_char[0]) + charged_rmb_serv;
					  sprintf(&temp_char[0], "%08d", temp_rmb);				
					  memcpy(&busy_frame.server_rmb[0], &temp_char[0], sizeof(busy_frame.server_rmb));
					 
					  sprintf(&temp_char[0], "%.8s", &busy_frame.rmb[0]);	
					  temp_rmb = atoi(&temp_char[0])  + charged_rmb;
					  sprintf(&temp_char[0], "%08d", temp_rmb);				
					  memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));
					 
					  sprintf(&temp_char[0], "%.8s", &busy_frame.total_rmb[0]);	
					  temp_rmb = atoi(&temp_char[0]) + charged_rmb_serv + charged_rmb;
					  sprintf(&temp_char[0], "%08d", temp_rmb);				
					  memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));
					}
					sprintf(&temp_char[0], "%.8s", &busy_frame.start_kwh[0]);	
					temp_kwh = Globa_1->meter_KWH - atoi(&temp_char[0]);
					if(temp_kwh != 0){
					  sprintf(&temp_char[0], "%.8s", &busy_frame.rmb[0]);	
					  sprintf(&temp_char[0], "%08d", (atoi(&temp_char[0]))/temp_kwh);	
						memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));
					}
					sprintf(&temp_char[0], "%08d", temp_kwh);				
					memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
					sprintf(&temp_char[0], "%08d", Globa_1->meter_KWH);				
					memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));
				}
			}

			Ret = Set_0X55_Busy_ID(&busy_frame,0, id);
			if(Ret == -1){//失败
				printf("xxxxxxxxxxxxxxxxxxx Set_0X55_Busy_ID failed\n");
			}else{//成功
				printf("yyyyyyyyyyyyyyyyyyy Set_0X55_Busy_ID sucess\n");
			}
		}/*else if(up_num == 0x66){//
			Ret = Set_0X66_Busy_ID(id);
			if(Ret == -1){//失败
				printf("xxxxxxxxxxxxxxxxxxx Set_0X66_Busy_ID failed\n");
			}else{//成功
				printf("yyyyyyyyyyyyyyyyyyy Set_0X66_Busy_ID sucess\n");
			}
		}*/
	}
	pthread_mutex_unlock(&busy_db_pmutex);
	sleep(2);
	pthread_mutex_lock(&busy_db_pmutex);
	Set_0X550X66_Busy_ID();
	pthread_mutex_unlock(&busy_db_pmutex);
	id = 0;
	while(1){
		usleep(200000);//10ms
		Main_1_Watchdog = 1;
		Current_Systime = time(NULL);
	  if(Globa_1->BMS_Power_V == 1){
			GROUP1_BMS_Power24V_ON;
		}else{
			GROUP1_BMS_Power24V_OFF;
		}

		if(QT_count_500ms_enable == 1){//使能500MS 计时
			if(QT_count_500ms_flag != 1){//计时时间未到
				QT_count_500ms++;
				if(QT_count_500ms >= 2){
					QT_count_500ms_flag = 1;
				}
			}
		}

		//发送二维码地址内容
		if(Globa_1->have_APP_change1 == 1){
			Globa_1->have_APP_change1 = 0;

			Page_90.flag = Globa_1->Charger_param.APP_enable;
			memcpy(&Page_90.sn[0], &Globa_1->Charger_param.SN[0], sizeof(Page_90.sn));
			snprintf(&Page_90.http[0], sizeof(Page_90.http), "%s", &Globa_1->Charger_param2.http[0]);

			msg.type = 0x90;
			memcpy(&msg.argv[0], &Page_90.reserve1, sizeof(PAGE_90_FRAME));
			msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_90_FRAME), IPC_NOWAIT);
		}

	  if((Globa_1->have_hart != old_have_hart)||(Globa_1->Updat_data_value != old_Updat_data_value)||(( oldSignals_value != Globa_1->Signals_value_No)&&(Globa_1->Charger_param.Serial_Network == 1))){ //桩运行状态
	    msg.type = 0x91;
			if(Globa_1->Software_Version_change == 1){
				msg.argv[0] = 2;
			}else{
			  msg.argv[0] = Globa_1->have_hart;
			}
			msg.argv[1] = Globa_1->Updat_data_value;
			msg.argv[2] = Globa_1->Charger_param.Serial_Network;
	    if(Globa_1->Charger_param.Serial_Network == 0){
				msg.argv[3] = 0;
			}else{
			  msg.argv[3] = Globa_1->Signals_value_No;
			}
			msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 4, IPC_NOWAIT);
			old_have_hart = Globa_1->have_hart ;
			oldSignals_value = Globa_1->Signals_value_No;
			old_Updat_data_value = Globa_1->Updat_data_value;
	  }
		
		msg.type = 0x00;//每次读之前初始化
		Ret = msgrcv(Globa_1->qt_to_arm_msg_id, &msg, sizeof(CHARGER_MSG)-sizeof(long), 0, IPC_NOWAIT);//接收QT发送过来的消息队列
		if(Ret >= 0){
			printf("xxxxxx ret=%d,%0x,%0x\n", Ret, msg.type, msg.argv[0]);
		}

    if((Globa_1->Error_system == 0)&&(Globa_2->Error_system == 0)){//没有系统故障
	  	GROUP1_FAULT_LED_OFF;
		}else{
			GROUP1_FAULT_LED_ON;
		}

		//判断枪状态 取值只有  03-空闲 0x04-充电中 0x05-完成 0x07-等待
		if(Globa_1->gun_link != 1){  //枪连接断开
			Globa_1->gun_state = 0x03; //充电枪连接只要断开就为 03-空闲
		}else{
			if(Globa_1->gun_state == 0x03){ //枪状态 原来为 03-空闲 那现在改变为  0x07-等待
				Globa_1->gun_state = 0x07;
			}else if((Globa_1->gun_state == 0x07)&&(Globa_1->charger_state == 0x04)){ //枪状态 原来为 0x07-等待 那现在改变为  0x04-充电中
				Globa_1->gun_state = 0x04;
			}else if((Globa_1->gun_state == 0x04)&&(Globa_1->charger_state != 0x04)){ //枪状态 原来为 0x04-充电中 那现在改变为  0x05-完成
				Globa_1->gun_state = 0x05;
			}else if((Globa_1->gun_state == 0x05)&&(Globa_1->charger_state == 0x04)){ //枪状态 原来为 0x05-完成 那现在改变为  0x04-充电中 可能不拔枪又开始充电
				Globa_1->gun_state = 0x04;
			}
		}

		if(Globa_1->QT_Step != QT_Step){
			QT_Step = Globa_1->QT_Step;
			printf("xxxxxx Globa_1->QT_Step %0x\n",Globa_1->QT_Step);
		}
	
  	Light_control();

		switch(Globa_1->QT_Step){//判断 UI 所在界面
			case 0x01:{//---------------0 主界面 ( 初始 )状态  0.1  ------------------
    		//该线程所用变量准备初始化
    		Globa_1->Manu_start = 0;
    		Globa_1->charger_start = 0;
    		Globa_1->App.APP_start = 0;
			  Globa_1->BMS_Info_falg = 0;
        Globa_1->Special_price_APP_data_falg = 0; 
	      Globa_1->APP_Start_Account_type = 0;
    		QT_timer_stop(500);
	      Globa_1->BMS_Info_falg = 0;
    		charger_type = 0;
				Globa_1->card_type = 0;//卡类型 0:未知 1：用户卡 2：员工卡 3：维护卡
				Globa_1->checkout = 0;
				Globa_1->pay_step = 0;
	      Globa_1->User_Card_check_flag = 0;
			  Globa_1->User_Card_check_result = 0;
				
        Globa_1->QT_charge_money = 0;
        Globa_1->QT_charge_kwh = 0;
        Globa_1->QT_charge_time = 0;  
        Globa_1->QT_pre_time = 0;
        
    		Globa_1->BATT_type = 0;
    		Globa_1->BMS_need_time = 0;
				Globa_1->BMS_cell_HV = 0;
				Globa_1->BMS_cell_LV = 0;
				Globa_1->BMS_cell_HT = 0;
				Globa_1->BMS_cell_LT = 0;
	      Globa_1->meter_KWH = 0;
				Globa_1->soft_KWH = 0;
        Globa_1->QT_Step = 0x02; // 0 主界面 就绪( 可操作 )状态  0.2
				Globa_1->BMS_Power_V = 0;
			
  			Globa_1->ISO_8583_rmb_Serv = 0;               //总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
	      Globa_1->ISO_8583_rmb= 0;                     //电量消费金额 0.01yuan
	
				if(firmware_info_checkfile == 0){//上电第一次进行检测文件
				  FILE * fp = NULL;
			    fp = fopen(F_UPDATE_EAST_DC_CTL, "r");
				  if(NULL == fp){
				  	perror("open mstConfig error \n");
	          firmware_info_checkfile = 1;
					  //break;
				  }else{
						fclose(fp);
						msg.type = 0x6000; //提示是否升级界面
						memset(&Page_6100.reserve1, 0, sizeof(PAGE_6100_FRAME));
						memcpy(&msg.argv[0], &Page_6100.reserve1, sizeof(PAGE_6100_FRAME));
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_6100_FRAME), IPC_NOWAIT);
						Globa_1->QT_Step = 0x60;//0 主界面 ( 初始 )状态  0.1  	
						firmware_info_checkfile = 1;
					}
				}

				pthread_mutex_lock(&busy_db_pmutex);
				Set_0X550X66_Busy_ID();
	      pthread_mutex_unlock(&busy_db_pmutex);
				if(DEBUGLOG) RUN_GUN1_LogOut("系统重新上电");  

				break;
			}
			case 0x02:{//---------------0 主界面 就绪( 可操作 )状态  0.2  ------------
				if(Globa_1->charger_state == 0x05){//工作状态	BIN码	1Byte	0x01-故障  0x02-告警  03-空闲 0x04-充电中 0x05-完成 0x06-预约
					if(Globa_1->gun_link != 1){
						Globa_1->charger_state = 0x03;
						Globa_1->Time = 0;
						Globa_1->kwh = 0;
						Globa_1->BMS_batt_SOC = 0;
					}
				}else{
					Globa_1->charger_state = 0x03;
					Globa_1->Time = 0;
        	Globa_1->kwh = 0;
        	Globa_1->BMS_batt_SOC = 0;
				}

			  if(pre_old_id != 0){
					Select_NO_Over_count ++;
					if(Select_NO_Over_count >= 10){
						if(Select_NO_Over_Record_1(pre_old_id,0)!= -1){//表示有没有上传的记录
							pthread_mutex_lock(&busy_db_pmutex);
							Update_Charger_Busy_DB_1(pre_old_id, &pre_old_busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
							pthread_mutex_unlock(&busy_db_pmutex);
							Select_NO_Over_count = 0;
						}else{
							if(DEBUGLOG) RUN_GUN1_LogOut("%s %d","一号枪交易数据,进行再次更新成功 ID=",pre_old_id);  
						}
					
  					pthread_mutex_lock(&busy_db_pmutex);
						if(Select_Busy_BCompare_ID(&pre_old_busy_frame) == -1 ){//表示有记录没有插入到新的数据库中
							all_id = 0;
							Insertflag = Insert_Charger_Busy_DB(&pre_old_busy_frame, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
							if((Insertflag != -1)&&(all_id != 0)){//表示插入数据成功，其他的则需要重新插入
								if(Delete_Record_busy_DB(pre_old_id) != -1){
									if(DEBUGLOG) RUN_GUN1_LogOut("一号枪交易数据,已经再次插入到正式数据库中并删除临时数据库记录 ID = %d",pre_old_id);  
									id = 0;
									pre_old_id = 0;
									Select_NO_Over_count = 0;
									memset(&pre_old_busy_frame, '0', sizeof(pre_old_busy_frame));
								}
							}else{
							  if(DEBUGLOG) RUN_GUN1_LogOut("一号枪存在交易数据,再次插入到正式数据库中失败 数据类型 pre_old_id = %d",pre_old_id);  
							}
						}else{//表示已经存在了，则直接删除该文件
						  Delete_Record_busy_DB(pre_old_id);
							Busy_NO_insert_id = 0;
							pre_old_id = 0;
							Select_NO_Over_count = 0;
							memset(&pre_old_busy_frame, '0', sizeof(pre_old_busy_frame));
						}
						pthread_mutex_unlock(&busy_db_pmutex);
					}
				}else{
					Select_NO_Over_count = 0;
					Select_Busy_count++;
					if(Select_Busy_count >= 50){//10s
						Select_Busy_count = 0;
						 Busy_NO_insert_id = Select_Busy_NO_insert_1(&pre_old_busy_frame);
						if(Busy_NO_insert_id != -1 ){//防止中途掉电的时候出现异常的现象,表示有记录没有插入到新的数据库中
						//进行数据库对比，确定是否已经插入到了正式的数据跨中。
							pthread_mutex_lock(&busy_db_pmutex);
						  if(Select_Busy_BCompare_ID(&pre_old_busy_frame) == -1){
								all_id = 0;
								Insertflag = Insert_Charger_Busy_DB(&pre_old_busy_frame, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
								if((Insertflag != -1)&&(all_id != 0)){//表示插入数据成功，其他的则需要重新插入
									if(Delete_Record_busy_DB(Busy_NO_insert_id) != -1){
										Busy_NO_insert_id = 0;
										pre_old_id = 0;
										Select_NO_Over_count = 0;
										memset(&pre_old_busy_frame, '0', sizeof(pre_old_busy_frame));
									}
								}else{
								  if(DEBUGLOG) RUN_GUN1_LogOut("一号枪存在交易数据,再次插入到正式数据库中失败 数据类型 pre_old_id = %d",pre_old_id);  
							  }
							}else{//表示已经存在了，则直接删除该文件
								Delete_Record_busy_DB(Busy_NO_insert_id);
								Busy_NO_insert_id = 0;
								pre_old_id = 0;
								Select_NO_Over_count = 0;
								memset(&pre_old_busy_frame, '0', sizeof(pre_old_busy_frame));
							}
							pthread_mutex_unlock(&busy_db_pmutex);
						}
					}
				}
			  Sing_pushbtton_count2 = 0;
				Sing_pushbtton_count1 = 0;
				Globa_1->BMS_Info_falg = 0;
				Globa_1->Double_Gun_To_Car_falg = 0;
				Globa_1->User_Card_check_flag = 0;
			  Globa_1->User_Card_check_result = 0;
			  Globa_1->private_price_flag  = 0;
				Globa_1->Enterprise_Card_flag = 0;//企业卡号成功充电上传标志
	      Globa_1->qiyehao_flag = 0;//企业号标记，域43
        Globa_1->order_chg_flag = 0;
				Globa_1->private_price_acquireOK = 0;
				Globa_1->order_card_end_time[0] = 99;
			  Globa_1->order_card_end_time[1] = 99;

				Globa_1->ISO_8583_rmb_Serv= 0;               //总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
	      Globa_1->ISO_8583_rmb= 0;                     //电量消费金额 0.01yuan
	      Globa_1->checkout = 0;
				Globa_1->pay_step = 0;
			  if(Globa_1->Charger_param.System_Type == 3){//壁挂是没有电表的
          Globa_1->meter_KWH = 0;
				}
				Globa_1->soft_KWH = 0;
				
				if(Globa_1->Electric_pile_workstart_Enable == 1){ //禁止本桩工作
					msg.type = 0x5000; //系统忙界面
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
					Globa_1->QT_Step = 0x43; //主界面
					break;
				}
				
				if(Globa_1->App.APP_start == 1){//手机启动充电 ---有可能会获取到电价则用它的对电价
					count_wait = 0;
					memset(&busy_frame, '0', sizeof(busy_frame));
		      Select_NO_Over_count = 0;
					
					if(pre_old_id != 0){
						pthread_mutex_lock(&busy_db_pmutex);
						Set_0X550X66_TO0X00_Busy_ID_Gun_1(1); //强制性更新上传标志
						pthread_mutex_unlock(&busy_db_pmutex);
						pre_old_id = 0;
						Select_NO_Over_count = 0;
						memset(&pre_old_busy_frame, '0', sizeof(pre_old_busy_frame));
					}
					//-------------------     新业务进度数据------------------------------
					temp_rmb = 0;
					Globa_1->kwh = 0;
					Globa_1->rmb = 0;
					Globa_1->ISO_8583_rmb_Serv = 0;               //总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
	        Globa_1->ISO_8583_rmb= 0;                     //电量消费金额 0.01yuan
	
					Globa_1->total_rmb = 0;
					ul_start_kwh = 0;
		      ul_start_kwh_2 = 0;
				 	for(i=0;i<4;i++){
						g_ulcharged_rmb[i] = 0;
						g_ulcharged_kwh[i] = 0;
						g_ulcharged_rmb_Serv[i] = 0;
					
   					g_ulcharged_rmb_2[i] = 0;
						g_ulcharged_kwh_2[i] = 0;
						g_ulcharged_rmb_Serv_2[i] = 0;
					}
					
					if(Globa_1->Special_price_APP_data_falg == 1){
						for(i=0;i<4;i++){
							share_time_kwh_price[i] = Globa_1->Special_price_APP_data.share_time_kwh_price[i];
							share_time_kwh_price_serve[i] = Globa_1->Special_price_APP_data.share_time_kwh_price_serve[i]; //服务
						}
						Serv_Type = Globa_1->Special_price_APP_data.Serv_Type;
					}else{
						for(i=0;i<4;i++){
							share_time_kwh_price[i] = Globa_1->Charger_param2.share_time_kwh_price[i];
							share_time_kwh_price_serve[i] = Globa_1->Charger_param2.share_time_kwh_price_serve[i]; //服务
						}
						Serv_Type = Globa_1->Charger_param.Serv_Type;
					}
					
				  cur_price_index = GetCurPriceIndex(); //获取当前时间
					pre_price_index = cur_price_index;
					
				  if(Globa_1->BMS_Power_V == 1){
						GROUP1_BMS_Power24V_ON;
					}else{
						GROUP1_BMS_Power24V_OFF;
					}
					
					temp_kwh = 0;
					Globa_1->soft_KWH = 0;
					Globa_1->Time = 0;
					Globa_1->BMS_need_time = 0;
					Globa_1->BMS_batt_SOC = 0;
					Globa_1->BMS_cell_HV = 0;
   
					
					memset(Globa_1->VIN, '0', sizeof(Globa_1->VIN));

					memcpy(&busy_frame.card_sn[0], &Globa_1->App.ID[0], sizeof(busy_frame.card_sn));
					memcpy(&busy_frame.Busy_SN[0], &Globa_1->App.busy_SN[0], sizeof(busy_frame.Busy_SN));

					systime = time(NULL);         //获得系统的当前时间 
					localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
					sprintf(&temp_char[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
						                             tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
						
					memcpy(&busy_frame.start_time[0], &temp_char[0], sizeof(busy_frame.start_time));         
  				memcpy(&busy_frame.end_time[0], &temp_char[0], sizeof(busy_frame.end_time));
  				
					memcpy(&busy_frame.flag[0], "00", sizeof(busy_frame.flag));//员工卡，交易标志直接标记完成

					sprintf(&temp_char[0], "%08d", Globa_1->App.last_rmb);				
					memcpy(&busy_frame.last_rmb[0], &temp_char[0], sizeof(busy_frame.last_rmb));

					memcpy(&busy_frame.ter_sn[0],  &Globa_1->Charger_param.SN[0], sizeof(busy_frame.ter_sn));

			    Globa_1->meter_start_KWH = Globa_1->meter_KWH;//取初始电表示数
					Globa_1->meter_stop_KWH = Globa_1->meter_start_KWH;
					
					sprintf(&temp_char[0], "%08d", Globa_1->meter_start_KWH);    
					memcpy(&busy_frame.start_kwh[0], &temp_char[0], sizeof(busy_frame.start_kwh));
					
					sprintf(&temp_char[0], "%08d", Globa_1->meter_stop_KWH);				
					memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

					sprintf(&temp_char[0], "%08d", Globa_1->Charger_param.Price);    
					memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));

					sprintf(&temp_char[0], "%08d", Globa_1->kwh);				
					memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
					
					sprintf(&temp_char[0], "%08d", Globa_1->rmb);				
					memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));
	  			//起始总费用等于电量费用，暂时只做安电量收服务费
	  			sprintf(&temp_char[0], "%08d", Globa_1->total_rmb);
	  			memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));
	  			sprintf(&temp_char[0], "%08d", Globa_1->total_rmb - Globa_1->rmb);
	  			memcpy(&busy_frame.server_rmb[0], &temp_char[0], sizeof(busy_frame.server_rmb));
	  			busy_frame.type[0] = '0';//充电方式（01：刷卡，02：手机）
  				busy_frame.type[1] = '2';
  				sprintf(&temp_char[0], "%02d", Serv_Type);//服务费收费类型 01：按次收费；02：按时间收费(单位：分/10分钟；03：按电量	9	N	N2
  				memcpy(&busy_frame.Serv_Type[0], &temp_char[0], sizeof(busy_frame.Serv_Type));
					tmp_value = share_time_kwh_price[0];
					sprintf(&temp_char[0], "%08d", tmp_value);				
					memcpy(&busy_frame.sharp_rmb_kwh[0], &temp_char[0], sizeof(busy_frame.sharp_rmb_kwh));

					tmp_value = g_ulcharged_rmb[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.sharp_allrmb[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb));
				
					tmp_value = share_time_kwh_price[1];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.peak_rmb_kwh[0], &temp_char[0], sizeof(busy_frame.peak_rmb_kwh));
					tmp_value = g_ulcharged_rmb[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame.peak_allrmb[0], &temp_char[0], sizeof(busy_frame.peak_allrmb));
					
					tmp_value = share_time_kwh_price[2];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.flat_rmb_kwh[0], &temp_char[0], sizeof(busy_frame.flat_rmb_kwh));
					tmp_value = g_ulcharged_rmb[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame.flat_allrmb[0], &temp_char[0], sizeof(busy_frame.flat_allrmb));
				
					tmp_value = share_time_kwh_price[3];
					sprintf(&temp_char[0], "%08d", tmp_value);				
					memcpy(&busy_frame.valley_rmb_kwh[0], &temp_char[0], sizeof(busy_frame.valley_rmb_kwh));
					tmp_value = g_ulcharged_rmb[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.valley_allrmb[0], &temp_char[0], sizeof(busy_frame.valley_allrmb));
	
	
	        tmp_value = share_time_kwh_price_serve[0];
					sprintf(&temp_char[0], "%08d", tmp_value);				
					memcpy(&busy_frame.sharp_rmb_kwh_Serv[0], &temp_char[0], sizeof(busy_frame.sharp_rmb_kwh_Serv));

					tmp_value = g_ulcharged_rmb_Serv[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.sharp_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb_Serv));
				
					tmp_value = share_time_kwh_price_serve[1];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.peak_rmb_kwh_Serv[0], &temp_char[0], sizeof(busy_frame.peak_rmb_kwh_Serv));
					tmp_value = g_ulcharged_rmb_Serv[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame.peak_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.peak_allrmb_Serv));
					
					tmp_value = share_time_kwh_price_serve[2];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.flat_rmb_kwh_Serv[0], &temp_char[0], sizeof(busy_frame.flat_rmb_kwh_Serv));
					tmp_value = g_ulcharged_rmb_Serv[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame.flat_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.flat_allrmb_Serv));
				
					tmp_value = share_time_kwh_price_serve[3];
					sprintf(&temp_char[0], "%08d", tmp_value);				
					memcpy(&busy_frame.valley_rmb_kwh_Serv[0], &temp_char[0], sizeof(busy_frame.valley_rmb_kwh_Serv));
					tmp_value = g_ulcharged_rmb_Serv[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.valley_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.valley_allrmb_Serv));
	
				  busy_frame.ConnectorId[0] = '0';//充电枪号
					busy_frame.ConnectorId[1] = '1';
					
          busy_frame.End_code[0] = '1';//结束代码
				  busy_frame.End_code[1] = '1';
					
  				tmp_value =0;
					
					pthread_mutex_lock(&busy_db_pmutex);
					id = 0;
					Insertflag = Insert_Charger_Busy_DB_1(&busy_frame, &id, 0x55);//写新数据库记录 有流水号 传输标志0x55不能上传
					pthread_mutex_unlock(&busy_db_pmutex);
					
          if(Insertflag == -1){
						AP_APP_Sent_1_falg[1] = 6;
						AP_APP_Sent_1_falg[0] = 1;
						Page_1300.cause = 63;//数据库异常;
						Page_1300.Capact = Globa_1->BMS_batt_SOC;
						Page_1300.elapsed_time[0] =  Globa_1->Time;
						Page_1300.elapsed_time[1] =  Globa_1->Time>>8;
						Page_1300.KWH[0] =  Globa_1->kwh;
						Page_1300.KWH[1] =  Globa_1->kwh>>8;
						Page_1300.RMB[0] =  Globa_1->total_rmb;
						Page_1300.RMB[1] =  Globa_1->total_rmb>>8;
						Page_1300.RMB[2] =  Globa_1->total_rmb>>16;
					  Page_1300.RMB[3] =  Globa_1->total_rmb>>24;
					
						memcpy(&Page_1300.SN[0], &busy_frame.card_sn[0], sizeof(Page_1300.SN));
						Page_1300.APP = 2;   //充电启动方式 1:刷卡  2：手机APP
						memcpy(&msg.argv[0], &Page_1300.reserve1, sizeof(PAGE_1300_FRAME));
						if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
							msg.type = 0x1300;//充电结束界面
						}else{
							msg.type = 0x1310;//单枪结束界面
						}
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_1300_FRAME), IPC_NOWAIT);
						sleep(1);//不用延时太久，APP控制，需要立即上传业务数据
						Globa_1->BMS_Power_V = 0;
						Globa_1->Operation_command = 2;//进行解锁
						memset(&Globa_1->App.ID, '0', sizeof(Globa_1->App.ID));
						if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
							msg.type = 0x150;
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);						
							QT_timer_stop(500);
							Globa_1->QT_Step = 0x03; //自动充电分开的AB通道
						}else{//单枪
							//防止按键处理不同步，此处同步一下
							msg.type = 0x150;//
							msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa_2->QT_Step = 0x03; //主界面
							
							msg.type = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选着界面，才有此按钮功能）
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa_1->QT_Step = 0x02; //主界面
						}
					  break;
					}		
					AP_APP_Sent_1_falg[0] = 1;
					usleep(10000);
    			Globa_1->Manu_start = 1;
      		Globa_1->charger_start = 1;
    
     			if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
						msg.type = 0x1111;//充电界面---双枪的时候
				  }else{
						msg.type = 0x1122;//充电界面--单枪
					}
     			memset(&msg.argv[0], 0x00, sizeof(PAGE_1111_FRAME));
					memset(&Page_1111.reserve1, 0x00, sizeof(PAGE_1111_FRAME));
					 Page_1111.APP  = 2;   //充电启动方式 1:刷卡  2：手机APP
					memcpy(&Page_1111.SN[0], &busy_frame.card_sn[0], sizeof(Page_1111.SN));
					
					memcpy(&Page_1111.SN[0], &busy_frame.card_sn[0], sizeof(Page_1111.SN));
    			msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_1111_FRAME), IPC_NOWAIT);
         
    			QT_timer_start(500);
    			Globa_1->QT_Step = 0x22;   // 2 自动充电-》 APP 充电界面 状态 2.2-
          if(Globa_1->Charger_param.NEW_VOICE_Flag == 1){
    			  Voicechip_operation_function(0x05);  //05H     “系统自检中，请稍候”
					}
					Globa_1->charger_state = 0x04;
    			count_wait = 0;
			    count_wait1 = 0;
					meter_Curr_Threshold_count = Current_Systime;
					current_low_timestamp = Current_Systime;
					check_current_low_st = 0;
					Electrical_abnormality = Current_Systime;
					current_overkwh_timestamp = Current_Systime;
					break;
    		}
    		
				if(msg.type == 0x100){//判断是否为   主界面
    			switch(msg.argv[0]){//判断 在当前界面下 用户操作的按钮
    				case 0x01:{//自动充电按钮
						  if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
								msg.type = 0x150;//充电枪启动选着界面
								msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
								Globa_1->QT_Step = 0x03; // 系统自动充电分开的AB通道。

								//防止按键处理不同步，此处同步一下
								msg.type = 0x150;//充电枪启动选着界面
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
								Globa_2->QT_Step = 0x03; // 系统自动充电分开的AB通道。
							}else{//单枪直接跳到充电选择界面
								if(Globa_1->Error_system == 0){//没有系统故障
									Page_1000.link = (Globa_1->gun_link == 1)?1:0;  //1:连接  0：断开
									Page_1000.VIN_Support = (((Globa_1->have_hart == 1)&&(Globa_1->VIN_Support_falg == 1)) == 1)? 1:0;
									Page_1000.other_link = 0;
									msg.type = 0x1000;
									memcpy(&msg.argv[0], &Page_1000.reserve1, sizeof(PAGE_1000_FRAME));
									msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_1000_FRAME), IPC_NOWAIT);
		
									QT_timer_start(500);//需要刷新充电枪的状态
									Globa_1->QT_Step = 0x11; // 1 自动充电-》充电枪连接确认 状态  1.1
									if(Globa_1->Charger_param.NEW_VOICE_Flag == 1){
										Voicechip_operation_function(0x03);  //03H     “请插入充电枪，并确认连接状态 ”
									}else{
										Voicechip_operation_function(0x01);
									}
								}else if(Globa_1->Error.PE_Fault==1){
									if(Globa_1->Charger_param.NEW_VOICE_Flag == 1){
										Voicechip_operation_function(0x16);//  16H    “系统接地异常，请联系设备管理员检查线路”	
									}else{
										Voicechip_operation_function(0x03);
									}
								}else if((Globa_1->Error.input_over_protect==1)||(Globa_1->Error.input_lown_limit==1)){
									 if(Globa_1->Charger_param.NEW_VOICE_Flag  == 1){
										Voicechip_operation_function(0x15); //15H    “交流电压输入异常，请联系设备管理员检查线路”
									}else{
										Voicechip_operation_function(0x03);
									}	
								}else{
									if( Globa_1->Charger_param.NEW_VOICE_Flag == 1){
										Voicechip_operation_function(0x14);//  14H    “系统故障，暂停使用 ”
									}else{
										Voicechip_operation_function(0x03);
									}	
								}
							}
							
							if( Globa_1->Charger_param.NEW_VOICE_Flag == 1){
							  Voicechip_operation_function(0x00);  //00H     “欢迎光临”
							}
							Globa_1->page_flag = 0;
							count_wait1 = 0;
							Globa_1->Double_Gun_To_Car_falg = 0;
    					break;
    				}

    				case 0x02:{//关于 按钮
    					Page_2100.reserve1 = 1;
    					memset(&Page_2100.reserve1, 0x00, sizeof(Page_2100));
    					sprintf(&Page_2100.measure_ver[0], "%s", SOFTWARE_VER);
							if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
    					  sprintf(&Page_2100.contro_ver[0], "A:%s|B:%s",&Globa_1->contro_ver[0],&Globa_2->contro_ver[0]);
							}else{
							  sprintf(&Page_2100.contro_ver[0], "%s",&Globa_1->contro_ver[0]);	
							}								
    					sprintf(&Page_2100.protocol_ver[0], "%s", PROTOCOL_VER);
    					sprintf(&Page_2100.kernel_ver[0], "%s", &Globa_1->kernel_ver[0]);

							Page_2100.sharp_rmb_kwh[0] = Globa_1->Charger_param2.share_time_kwh_price[0]&0XFF;
							Page_2100.sharp_rmb_kwh[1] = (Globa_1->Charger_param2.share_time_kwh_price[0]>>8)&0XFF;		

							Page_2100.peak_rmb_kwh[0] = Globa_1->Charger_param2.share_time_kwh_price[1]&0XFF;
							Page_2100.peak_rmb_kwh[1] =(Globa_1->Charger_param2.share_time_kwh_price[1]>>8)&0XFF;	

							Page_2100.flat_rmb_kwh[0] = Globa_1->Charger_param2.share_time_kwh_price[2]&0XFF;
							Page_2100.flat_rmb_kwh[1] =(Globa_1->Charger_param2.share_time_kwh_price[2]>>8)&0XFF;				

							Page_2100.valley_rmb_kwh[0] = Globa_1->Charger_param2.share_time_kwh_price[3]&0XFF;
							Page_2100.valley_rmb_kwh[1] =(Globa_1->Charger_param2.share_time_kwh_price[3]>>8)&0XFF;				
              
							Page_2100.Serv_Type = Globa_1->Charger_param.Serv_Type;
							Page_2100.run_state = Globa_1->have_hart;
							Page_2100.dispaly_Type = Globa_1->Charger_param2.show_price_type;  //金额显示类型， 0--详细显示金额+分时费    1--直接显示金额
							Page_2100.sharp_rmb_kwh_serve[0] = Globa_1->Charger_param2.share_time_kwh_price_serve[0]&0XFF;
							Page_2100.sharp_rmb_kwh_serve[1] = (Globa_1->Charger_param2.share_time_kwh_price_serve[0]>>8)&0XFF;		

							Page_2100.peak_rmb_kwh_serve[0] = Globa_1->Charger_param2.share_time_kwh_price_serve[1]&0XFF;
							Page_2100.peak_rmb_kwh_serve[1] =(Globa_1->Charger_param2.share_time_kwh_price_serve[1]>>8)&0XFF;	

							Page_2100.flat_rmb_kwh_serve[0] = Globa_1->Charger_param2.share_time_kwh_price_serve[2]&0XFF;
							Page_2100.flat_rmb_kwh_serve[1] =(Globa_1->Charger_param2.share_time_kwh_price_serve[2]>>8)&0XFF;				

							Page_2100.valley_rmb_kwh_serve[0] = Globa_1->Charger_param2.share_time_kwh_price_serve[3]&0XFF;
							Page_2100.valley_rmb_kwh_serve[1] =(Globa_1->Charger_param2.share_time_kwh_price_serve[3]>>8)&0XFF;				
              
							Page_2100.time_zone_num = Globa_1->Charger_param2.time_zone_num;  //时段数
							for(i=0;i<12;i++){
								Page_2100.time_zone_tabel[i][0] = Globa_1->Charger_param2.time_zone_tabel[i]&0XFF;//时段表
							 	Page_2100.time_zone_tabel[i][1] = (Globa_1->Charger_param2.time_zone_tabel[i]>>8)&0XFF;//时段表
							  Page_2100.time_zone_feilv[i] = Globa_1->Charger_param2.time_zone_feilv[i];//时段表位于的价格---表示对应的为尖峰平谷的值
							}
							memcpy(&msg.argv[0], &Page_2100.reserve1, sizeof(Page_2100));
							msg.type = 0x2000;
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(Page_2100), IPC_NOWAIT);
							Globa_1->QT_Step = 0x42;
    					break;
    				}
						
    				case 0x03:{//系统管理按钮
							count_wait = 0;
							msg.type = 0x200;//等待刷卡界面
							memset(&Page_200.reserve1, '0', sizeof(PAGE_200_FRAME));
	           	Page_200.state = 10;//请刷管理员卡
							memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
    					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);

    					Globa_1->checkout = 0;
							Globa_1->pay_step = 5;
    					Globa_1->QT_Step = 0x30; // 系统管理界面
							if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
							  Voicechip_operation_function(0x04);//04H     “请刷卡”	
							}
							break;
    				}   
    				case 0x04:{//帮助按钮
     					msg.type = 0x4000;//帮助界面
    					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);

    					Globa_1->QT_Step = 0x41; // 帮助界面
    					break;
    				}
						case 0x05:{//支付卡查询余额/解锁
						  msg.type = 0x5300;//帮助界面
    					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
    					Globa_1->QT_Step = 0x50; // 支付卡查询余额/解锁
							break;
    				}
						
    				default:{
    					break;
    				}
    			}
    		}

				break;
			}
			case 0x03:{//--------------- 自动充电分开的AB通道 ------------------------
				if(Globa_1->page_flag == 0){
					count_wait1++;
					if(count_wait1 >= 30){//等待时间5S
						if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
							Voicechip_operation_function(0x01);  //01H     “请选择充电枪 ”
						}
					}
				}
				
				if(Globa_1->charger_state == 0x05){//工作状态	BIN码	1Byte	0x01-故障  0x02-告警  03-空闲 0x04-充电中 0x05-完成 0x06-预约
					if(Globa_1->gun_link != 1){
						Globa_1->charger_state = 0x03;
						
						Globa_1->Time = 0;
						Globa_1->kwh = 0;
					}
				}else{
					Globa_1->charger_state = 0x03;
					Globa_1->Time = 0;
        	Globa_1->kwh = 0;
				}
				card_falg_count = 0;
        Bind_BmsVin_falg = 0;
	   		Globa_1->Manu_start = 0;
    		Globa_1->charger_start = 0;
        Globa_1->BMS_Info_falg = 0;
    		QT_timer_stop(500);
        Globa_1->order_card_end_time[0] = 99;
			  Globa_1->order_card_end_time[1] = 99;
    		charger_type = 0;
				Globa_1->Card_charger_type = 0;
				Globa_1->card_type = 0;//卡类型 0:未知 1：用户卡 2：员工卡 3：维护卡
				Globa_1->checkout = 0;
				Globa_1->pay_step = 0;
        Globa_1->Double_Gun_To_Car_falg = 0;
        Globa_1->QT_charge_money = 0;
        Globa_1->QT_charge_kwh = 0;
        Globa_1->QT_charge_time = 0;  
        Globa_1->QT_pre_time = 0;
        Globa_1->Start_Mode = 0;   
    		Globa_1->BATT_type = 0;
    		Globa_1->BMS_batt_SOC = 0;
    		Globa_1->BMS_need_time = 0;
				Globa_1->BMS_batt_SOC = 0;
				Globa_1->BMS_cell_HV = 0;
				Globa_1->User_Card_check_flag = 0;
			  Globa_1->User_Card_check_result = 0;
			
  			Globa_1->BMS_Info_falg = 0;
				Globa_1->User_Card_check_flag = 0;
			  Globa_1->User_Card_check_result = 0;
			  Globa_1->private_price_flag  = 0;
				Globa_1->Enterprise_Card_flag = 0;//企业卡号成功充电上传标志
	      Globa_1->qiyehao_flag = 0;//企业号标记，域43
        Globa_1->order_chg_flag = 0;
				Globa_1->private_price_acquireOK = 0;
				Globa_1->Special_card_offline_flag = 0;
        Globa_1->Charger_Over_Rmb_flag = 0;

			  if(Globa_1->Charger_param.System_Type == 3){//壁挂是没有电表的
          Globa_1->meter_KWH = 0;
				}
				Globa_1->soft_KWH = 0;
				
				if(pre_old_id != 0){
					Select_NO_Over_count ++;
					if(Select_NO_Over_count >= 10){
						if(Select_NO_Over_Record_1(pre_old_id,0) != -1){//表示有没有上传的记录
							pthread_mutex_lock(&busy_db_pmutex);
							Update_Charger_Busy_DB_1(pre_old_id, &pre_old_busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
							pthread_mutex_unlock(&busy_db_pmutex);
							Select_NO_Over_count = 0;
							if(DEBUGLOG) RUN_GUN1_LogOut("%s %d","一号枪交易数据中更新失败,进行再次更新ID=",pre_old_id);  
						}else{
							if(DEBUGLOG) RUN_GUN1_LogOut("%s %d","一号枪交易数据,进行再次更新成功 ID=",pre_old_id);  
						}
						
						pthread_mutex_lock(&busy_db_pmutex);
						if(Select_Busy_BCompare_ID(&pre_old_busy_frame) == -1 ){//表示有记录没有插入到新的数据库中
							all_id = 0;
							Insertflag = Insert_Charger_Busy_DB(&pre_old_busy_frame, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
							if((Insertflag != -1)&&(all_id != 0)){//表示插入数据成功，其他的则需要重新插入
								if(Delete_Record_busy_DB(pre_old_id) != -1){
									if(DEBUGLOG) RUN_GUN1_LogOut("一号枪交易数据,已经再次插入到正式数据库中并删除临时数据库记录 pre_old_id = %d",pre_old_id);  
									id = 0;
									pre_old_id = 0;
									Select_NO_Over_count = 0;
									memset(&pre_old_busy_frame, '0', sizeof(pre_old_busy_frame));
								}
							}else{
								if(DEBUGLOG) RUN_GUN1_LogOut("一号枪存在交易数据,再次插入到正式数据库中失败 数据类型 pre_old_id = %d",pre_old_id);  
							}
						}else{//表示已经存在了，则直接删除该文件
							Delete_Record_busy_DB(pre_old_id);
							Busy_NO_insert_id = 0;
							pre_old_id = 0;
							Select_NO_Over_count = 0;
							memset(&pre_old_busy_frame, '0', sizeof(pre_old_busy_frame));
						}
						pthread_mutex_unlock(&busy_db_pmutex);
					}
				}else{
					Select_NO_Over_count = 0;
					Select_Busy_count++;
					if(Select_Busy_count >= 50){
						Select_Busy_count = 0;
						 Busy_NO_insert_id = Select_Busy_NO_insert_1(&pre_old_busy_frame);
						if(Busy_NO_insert_id != -1 ){//防止中途掉电的时候出现异常的现象,表示有记录没有插入到新的数据库中
						//进行数据库对比，确定是否已经插入到了正式的数据跨中。
							pthread_mutex_lock(&busy_db_pmutex);
							if(Select_Busy_BCompare_ID(&pre_old_busy_frame) == -1){
							  all_id = 0;
							  Insertflag = Insert_Charger_Busy_DB(&pre_old_busy_frame, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
								if((Insertflag != -1)&&(all_id != 0)){//表示插入数据成功，其他的则需要重新插入
									if(Delete_Record_busy_DB(Busy_NO_insert_id) != -1){
										Busy_NO_insert_id = 0;
										pre_old_id = 0;
										Select_NO_Over_count = 0;
										memset(&pre_old_busy_frame, '0', sizeof(pre_old_busy_frame));
									}
								}else{
									if(DEBUGLOG) RUN_GUN1_LogOut("查询到一号枪存在交易数据,再次插入到正式数据库中失败 数据类型 pre_old_id = %d",pre_old_id);  
								}
							}else{//表示已经存在了，则直接删除该文件
								Delete_Record_busy_DB(Busy_NO_insert_id);
								Busy_NO_insert_id = 0;
								pre_old_id = 0;
								Select_NO_Over_count = 0;
								memset(&pre_old_busy_frame, '0', sizeof(pre_old_busy_frame));
							}
							pthread_mutex_unlock(&busy_db_pmutex);
						}
					}
				}
  			
				if(Globa_1->Electric_pile_workstart_Enable == 1){ //1//禁止本桩工作 
					msg.type = 0x5000; //系统忙界面
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
					Globa_1->QT_Step = 0x43; //主界面
					if(Globa_1->Charger_param.NEW_VOICE_Flag == 1){
					  Voicechip_operation_function(0x13);  //13H    “系统维护中，暂停使用 ”
					}
					break;
				}
				
				if((Globa_2->QT_Step == 0x03)||(Globa_2->QT_Step == 0x201)||(Globa_2->QT_Step == 0x21)||(Globa_2->QT_Step == 0x23)||(Globa_2->QT_Step == 0x22)){//枪2界面在半屏状态
					if(Globa_1->App.APP_start == 1){//手机启动充电
						Select_NO_Over_count = 0;
						
						if(pre_old_id != 0){
							pthread_mutex_lock(&busy_db_pmutex);
							Set_0X550X66_TO0X00_Busy_ID_Gun_1(1); //强制性更新上传标志
							pthread_mutex_unlock(&busy_db_pmutex);
							pre_old_id = 0;
							Select_NO_Over_count = 0;
							memset(&pre_old_busy_frame, '0', sizeof(pre_old_busy_frame));
						}
						
						count_wait = 0;
						memset(&busy_frame, '0', sizeof(busy_frame));
            memset(&Page_BMS_data,0,sizeof(PAGE_BMS_FRAME));
						memset(Globa_1->VIN, '0', sizeof(Globa_1->VIN));

						//-------------------     新业务进度数据------------------------------
						temp_rmb = 0;
						Globa_1->kwh = 0;
						Globa_1->rmb = 0;
						Globa_1->total_rmb = 0;
						ul_start_kwh = 0;
						ul_start_kwh_2 = 0;
						Globa_1->Start_Mode = APP_START_TYPE;//app

						for(i=0;i<4;i++){
							g_ulcharged_rmb[i] = 0;
							g_ulcharged_kwh[i] = 0;
							g_ulcharged_rmb_Serv[i] = 0;
						  g_ulcharged_rmb_2[i] = 0;
						  g_ulcharged_kwh_2[i] = 0;
						  g_ulcharged_rmb_Serv_2[i] = 0;
						}
						if(Globa_1->Special_price_APP_data_falg == 1){
							for(i=0;i<4;i++){
								share_time_kwh_price[i] = Globa_1->Special_price_APP_data.share_time_kwh_price[i];
						  	share_time_kwh_price_serve[i] = Globa_1->Special_price_APP_data.share_time_kwh_price_serve[i]; //服务
							}
							Serv_Type = Globa_1->Special_price_APP_data.Serv_Type;
						}else{
							for(i=0;i<4;i++){
								share_time_kwh_price[i] = Globa_1->Charger_param2.share_time_kwh_price[i];
								share_time_kwh_price_serve[i] = Globa_1->Charger_param2.share_time_kwh_price_serve[i]; //服务
							}
							Serv_Type = Globa_1->Charger_param.Serv_Type;
						}
						
						cur_price_index = GetCurPriceIndex(); //获取当前时间
						pre_price_index = cur_price_index;

						
						if(Globa_1->BMS_Power_V == 1){
							GROUP1_BMS_Power24V_ON;
						}else{
							GROUP1_BMS_Power24V_OFF;
						}
						temp_kwh = 0;
						Globa_1->soft_KWH = 0;
						Globa_1->Time = 0;
						Globa_1->BMS_need_time = 0;
						Globa_1->BMS_batt_SOC = 0;
						Globa_1->BMS_cell_HV = 0;

						memcpy(&busy_frame.card_sn[0], &Globa_1->App.ID[0], sizeof(busy_frame.card_sn));
						memcpy(&busy_frame.Busy_SN[0], &Globa_1->App.busy_SN[0], sizeof(busy_frame.Busy_SN));

						systime = time(NULL);         //获得系统的当前时间 
						localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
						sprintf(&temp_char[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
																					 tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
							
						memcpy(&busy_frame.start_time[0], &temp_char[0], sizeof(busy_frame.start_time));         
						memcpy(&busy_frame.end_time[0], &temp_char[0], sizeof(busy_frame.end_time));
						
						memcpy(&busy_frame.flag[0], "00", sizeof(busy_frame.flag));//员工卡，交易标志直接标记完成

						sprintf(&temp_char[0], "%08d", Globa_1->App.last_rmb);				
						memcpy(&busy_frame.last_rmb[0], &temp_char[0], sizeof(busy_frame.last_rmb));

						memcpy(&busy_frame.ter_sn[0],  &Globa_1->Charger_param.SN[0], sizeof(busy_frame.ter_sn));

						Globa_1->meter_start_KWH = Globa_1->meter_KWH;//取初始电表示数
						Globa_1->meter_stop_KWH = Globa_1->meter_start_KWH;

						sprintf(&temp_char[0], "%08d", Globa_1->meter_start_KWH);    
						memcpy(&busy_frame.start_kwh[0], &temp_char[0], sizeof(busy_frame.start_kwh));
						
						sprintf(&temp_char[0], "%08d", Globa_1->meter_stop_KWH);				
						memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

						sprintf(&temp_char[0], "%08d", Globa_1->Charger_param.Price);    
						memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));

						sprintf(&temp_char[0], "%08d", Globa_1->kwh);				
						memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
						
						sprintf(&temp_char[0], "%08d", Globa_1->rmb);				
						memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));
						//起始总费用等于电量费用，暂时只做安电量收服务费
						sprintf(&temp_char[0], "%08d", Globa_1->total_rmb);
						memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));
						sprintf(&temp_char[0], "%08d", Globa_1->total_rmb - Globa_1->rmb);
						memcpy(&busy_frame.server_rmb[0], &temp_char[0], sizeof(busy_frame.server_rmb));
						busy_frame.type[0] = '0';//充电方式（01：刷卡，02：手机）
						busy_frame.type[1] = '2';
						sprintf(&temp_char[0], "%02d", Serv_Type);//服务费收费类型 01：按次收费；02：按时间收费(单位：分/10分钟；03：按电量	9	N	N2
						memcpy(&busy_frame.Serv_Type[0], &temp_char[0], sizeof(busy_frame.Serv_Type));
						
						tmp_value = share_time_kwh_price[0];
						sprintf(&temp_char[0], "%08d", tmp_value);				
						memcpy(&busy_frame.sharp_rmb_kwh[0], &temp_char[0], sizeof(busy_frame.sharp_rmb_kwh));

						tmp_value = g_ulcharged_rmb[0];
						sprintf(&temp_char[0], "%08d", tmp_value);	
						memcpy(&busy_frame.sharp_allrmb[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb));
					
						tmp_value = share_time_kwh_price[1];
						sprintf(&temp_char[0], "%08d", tmp_value);	
						memcpy(&busy_frame.peak_rmb_kwh[0], &temp_char[0], sizeof(busy_frame.peak_rmb_kwh));
						tmp_value = g_ulcharged_rmb[1];
						sprintf(&temp_char[0], "%08d", tmp_value);
						memcpy(&busy_frame.peak_allrmb[0], &temp_char[0], sizeof(busy_frame.peak_allrmb));
						
						tmp_value = share_time_kwh_price[2];
						sprintf(&temp_char[0], "%08d", tmp_value);	
						memcpy(&busy_frame.flat_rmb_kwh[0], &temp_char[0], sizeof(busy_frame.flat_rmb_kwh));
						tmp_value = g_ulcharged_rmb[2];
						sprintf(&temp_char[0], "%08d", tmp_value);		
						memcpy(&busy_frame.flat_allrmb[0], &temp_char[0], sizeof(busy_frame.flat_allrmb));
					
						tmp_value = share_time_kwh_price[3];
						sprintf(&temp_char[0], "%08d", tmp_value);				
						memcpy(&busy_frame.valley_rmb_kwh[0], &temp_char[0], sizeof(busy_frame.valley_rmb_kwh));
						tmp_value = g_ulcharged_rmb[3];
						sprintf(&temp_char[0], "%08d", tmp_value);	
						memcpy(&busy_frame.valley_allrmb[0], &temp_char[0], sizeof(busy_frame.valley_allrmb));
						
						tmp_value = share_time_kwh_price_serve[0];
						sprintf(&temp_char[0], "%08d", tmp_value);				
						memcpy(&busy_frame.sharp_rmb_kwh_Serv[0], &temp_char[0], sizeof(busy_frame.sharp_rmb_kwh_Serv));

						tmp_value = g_ulcharged_rmb_Serv[0];
						sprintf(&temp_char[0], "%08d", tmp_value);	
						memcpy(&busy_frame.sharp_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb_Serv));
					
						tmp_value = share_time_kwh_price_serve[1];
						sprintf(&temp_char[0], "%08d", tmp_value);	
						memcpy(&busy_frame.peak_rmb_kwh_Serv[0], &temp_char[0], sizeof(busy_frame.peak_rmb_kwh_Serv));
						tmp_value = g_ulcharged_rmb_Serv[1];
						sprintf(&temp_char[0], "%08d", tmp_value);
						memcpy(&busy_frame.peak_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.peak_allrmb_Serv));
						
						tmp_value = share_time_kwh_price_serve[2];
						sprintf(&temp_char[0], "%08d", tmp_value);	
						memcpy(&busy_frame.flat_rmb_kwh_Serv[0], &temp_char[0], sizeof(busy_frame.flat_rmb_kwh_Serv));
						tmp_value = g_ulcharged_rmb_Serv[2];
						sprintf(&temp_char[0], "%08d", tmp_value);		
						memcpy(&busy_frame.flat_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.flat_allrmb_Serv));
					
						tmp_value = share_time_kwh_price_serve[3];
						sprintf(&temp_char[0], "%08d", tmp_value);				
						memcpy(&busy_frame.valley_rmb_kwh_Serv[0], &temp_char[0], sizeof(busy_frame.valley_rmb_kwh_Serv));
						tmp_value = g_ulcharged_rmb_Serv[3];
						sprintf(&temp_char[0], "%08d", tmp_value);	
						memcpy(&busy_frame.valley_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.valley_allrmb_Serv));
		
						tmp_value =0;
						busy_frame.ConnectorId[0] = '0';//充电枪号
						busy_frame.ConnectorId[1] = '1';
						busy_frame.End_code[0] = '1';//结束代码
						busy_frame.End_code[1] = '1';
						
						pthread_mutex_lock(&busy_db_pmutex);
						id = 0;
						Insertflag = Insert_Charger_Busy_DB_1(&busy_frame, &id, 0x55);//写新数据库记录 有流水号 传输标志0x55不能上传
						pthread_mutex_unlock(&busy_db_pmutex);
            usleep(10000);
				    if(DEBUGLOG) RUN_GUN1_LogOut("%s %d Insertflag = %d","一号枪:APP启动充电并插入交易记录中的ID=",id,Insertflag);  
						if(Insertflag == -1){
							AP_APP_Sent_1_falg[1] = 6;
							AP_APP_Sent_1_falg[0] = 1;
							Globa_1->Manu_start = 0;
							Globa_1->charger_start = 0;
							Globa_1->App.APP_start = 0;
							Page_1300.cause = 63;//数据库异常;
							Page_1300.Capact = Globa_1->BMS_batt_SOC;
							Page_1300.elapsed_time[0] =  Globa_1->Time;
							Page_1300.elapsed_time[1] =  Globa_1->Time>>8;
							Page_1300.KWH[0] =  Globa_1->kwh;
							Page_1300.KWH[1] =  Globa_1->kwh>>8;
							Page_1300.RMB[0] =  Globa_1->total_rmb;
							Page_1300.RMB[1] =  Globa_1->total_rmb>>8;
							Page_1300.RMB[2] =  Globa_1->total_rmb>>16;
					    Page_1300.RMB[3] =  Globa_1->total_rmb>>24;
					    memcpy(&Page_1300.SN[0], &busy_frame.card_sn[0], sizeof(Page_1300.SN));
							Page_1300.APP = 2;   //充电启动方式 1:刷卡  2：手机APP
							memcpy(&msg.argv[0], &Page_1300.reserve1, sizeof(PAGE_1300_FRAME));
							if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
								msg.type = 0x1300;//充电结束界面
							}else{
								msg.type = 0x1310;//单枪结束界面
							}
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_1300_FRAME), IPC_NOWAIT);
							sleep(1);//不用延时太久，APP控制，需要立即上传业务数据
							Globa_1->BMS_Power_V = 0;
							Globa_1->Operation_command = 2;//进行解锁
							memset(&Globa_1->App.ID, '0', sizeof(Globa_1->App.ID));
							if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
								msg.type = 0x150;
								msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);						
								QT_timer_stop(500);
								Globa_1->QT_Step = 0x03; //自动充电分开的AB通道
							}else{//单枪
								//防止按键处理不同步，此处同步一下
								msg.type = 0x150;//
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
								Globa_2->QT_Step = 0x03; //主界面
								
								msg.type = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选着界面，才有此按钮功能）
								msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
								Globa_1->QT_Step = 0x02; //主界面
							}
							break;
						}
						
						AP_APP_Sent_1_falg[0] = 1;
						Globa_1->Manu_start = 1;
						Globa_1->charger_start = 1;
						if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
							msg.type = 0x1111;//充电界面---双枪的时候
						}else{
							msg.type = 0x1122;//充电界面--单枪
						}
						memset(&Page_1111.reserve1, 0x00, sizeof(PAGE_1111_FRAME));
					  Page_1111.APP  = 2;   //充电启动方式 1:刷卡  2：手机APP
						memcpy(&Page_1111.SN[0], &busy_frame.card_sn[0], sizeof(Page_1111.SN));
						memcpy(&msg.argv[0], &Page_1111.reserve1, sizeof(PAGE_1111_FRAME));
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_1111_FRAME), IPC_NOWAIT);

			   
						QT_timer_start(500);
						Globa_1->QT_Step = 0x22;   // 2 自动充电-》 APP 充电界面 状态 2.2-

						Globa_1->charger_state = 0x04;
						count_wait = 0;
						if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
						  Voicechip_operation_function(0x05);  //05H     “系统自检中，请稍候”
						}
						meter_Curr_Threshold_count = Current_Systime;
						current_low_timestamp = Current_Systime;
						check_current_low_st = 0;
						Electrical_abnormality = Current_Systime;
						current_overkwh_timestamp = Current_Systime;
						break;
					}
				}

			  if(Globa_1->Charger_param.System_Type == 1 ){//双枪轮流充电才需要这个
					if((Globa_2->QT_Step == 0x21)||(Globa_2->QT_Step == 0x22)){//表示1号枪在充电
					  Sing_pushbtton_count2++;
	          if(Sing_pushbtton_count2 <= 3){
  					  msg.type = 0x88;
						  msg.argv[0] = 1; //可以预约
			        msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						}else{
							Sing_pushbtton_count2 = 4;
							Sing_pushbtton_count1 = 0;
						}
					}else{
	          Sing_pushbtton_count1++;
	          if(Sing_pushbtton_count1 <= 3){
							msg.type = 0x88;
							msg.argv[0] = 0; //直接显示充电
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						}else{
							Sing_pushbtton_count1 = 4;
							Sing_pushbtton_count2 = 0;
						}
					}
				}else if(Globa_1->Charger_param.System_Type == 4 ){//双枪电才需要这个
					//if((Globa_1->gun_link == 1)&&(Globa_2->gun_link == 1)&&(Globa_2->QT_Step == 0x03)){
					if(Globa_2->QT_Step == 0x03){
						Sing_pushbtton_count2++;
						if(Sing_pushbtton_count2 <= 3){
							msg.type = 0x87;
							msg.argv[0] = 1; //可以预约
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						}else{
							Sing_pushbtton_count2 = 4;
							Sing_pushbtton_count1 = 0;
						}
					}else{
						Sing_pushbtton_count1++;
						if(Sing_pushbtton_count1 <= 3){
							msg.type = 0x87;
							msg.argv[0] = 0; //可以预约
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						}else{
							Sing_pushbtton_count2 = 0;
							Sing_pushbtton_count1 = 4;
						}
					}
				}
				
			  if(msg.type == 0x150){//收到该界面消息
				  Sing_pushbtton_count2 = 0;
				  Sing_pushbtton_count1 = 0;
    		  switch(msg.argv[0]){//判断 在当前界面下 用户操作的按钮
					  case 0x01:{//1#通道选着按钮
    					if(Globa_1->Error_system == 0){//没有系统故障
    						if((Globa_2->QT_Step == 0x03)||(Globa_2->QT_Step == 0x201)||(Globa_2->QT_Step == 0x21)||(Globa_2->QT_Step == 0x23)||(Globa_2->QT_Step == 0x22)){//枪2界面在半屏状态
    							Page_1000.link = (Globa_1->gun_link == 1)?1:0;  //1:连接  0：断开
    							Page_1000.VIN_Support = (((Globa_1->have_hart == 1)&&(Globa_1->VIN_Support_falg == 1)) == 1)? 1:0;
									Page_1000.other_link = 0;
									
									msg.type = 0x1000;
         					memcpy(&msg.argv[0], &Page_1000.reserve1, sizeof(PAGE_1000_FRAME));
        					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_1000_FRAME), IPC_NOWAIT);
    
        					QT_timer_start(500);//需要刷新充电枪的状态
        					Globa_1->QT_Step = 0x11; // 1 自动充电-》充电枪连接确认 状态  1.1
        				  if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
        					  Voicechip_operation_function(0x03);  //03H     “请插入充电枪，并确认连接状态 ”
									}else{
										Voicechip_operation_function(0x01);
									}
								}
      				}else if(Globa_1->Error.PE_Fault==1){
							  if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
							  	Voicechip_operation_function(0x16);//  16H    “系统接地异常，请联系设备管理员检查线路”	
								}else{
									Voicechip_operation_function(0x03);
								}
      				}else if((Globa_1->Error.input_over_protect==1)||(Globa_1->Error.input_lown_limit==1)){
								 if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
							    Voicechip_operation_function(0x15); //15H    “交流电压输入异常，请联系设备管理员检查线路”
								}else{
									Voicechip_operation_function(0x03);
								}	
							}else{
								if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
      				  	Voicechip_operation_function(0x14);//  14H    “系统故障，暂停使用 ”
								}else{
									Voicechip_operation_function(0x03);
								}	
      				}
							Globa_1->Double_Gun_To_Car_falg = 0;//单枪独立
				      memset(&Globa_1->Bind_BmsVin[0], '0',sizeof(Globa_1->Bind_BmsVin));
    					break;
    			  }
  				  case 0x02:{//退出按钮
					  	msg.type = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选着界面，才有此按钮功能）
					  	msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						  Globa_1->QT_Step = 0x02; //主界面

					  	//防止按键处理不同步，此处同步一下
					  	msg.type = 0x150;//
					  	msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						  Globa_2->QT_Step = 0x03; //主界面
	            Globa_1->Double_Gun_To_Car_falg = 0;//单枪独立
  					  break;
  				  }
  			    case 0x03:{//双枪对一辆车充电模式
							if(Globa_1->Error_system == 0){//没有系统故障
								Page_1000.link = (Globa_1->gun_link == 1)?1:0;  //1:连接  0：断开
								Page_1000.VIN_Support = (((Globa_1->have_hart == 1)&&(Globa_1->VIN_Support_falg == 1)) == 1)? 1:0;
								Page_1000.other_link = (1<<4)|(Globa_2->gun_link);
								msg.type = 0x1000;
								memcpy(&msg.argv[0], &Page_1000.reserve1, sizeof(PAGE_1000_FRAME));
								msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_1000_FRAME), IPC_NOWAIT);
		            Globa_1->Double_Gun_To_Car_falg = 1;
								QT_timer_start(500);//需要刷新充电枪的状态
								Globa_1->QT_Step = 0x11; // 1 自动充电-》充电枪连接确认 状态  1.1
								if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
									Voicechip_operation_function(0x03);  //03H     “请插入充电枪，并确认连接状态 ”
								}else{
									Voicechip_operation_function(0x01);
								}
							}
    					break;
						}
						default:{
  					  break;
  				  }
  			  }
			  }
			  break;
			}
			case 0x11:{//---------------1 自动充电-》充电枪连接确认 状态  1.1---------
    		if(msg.type == 0x1000){//收到该界面消息
    			switch(msg.argv[0]){//判断 在当前界面下 用户操作的按钮
    				case 0x01:{//确认按钮
    					//根据充电枪的连接状态在进行判断处理
      				Globa_1->QT_gun_select = 1;				
	            Globa_1->BMS_Power_V = msg.argv[1];
			        if(Globa_1->BMS_Power_V == 1){
								GROUP1_BMS_Power24V_ON;
							}else{
								GROUP1_BMS_Power24V_OFF;
							}
							
							if((Globa_1->Double_Gun_To_Car_falg == 1)&&(Globa_1->Charger_param.System_Type == 4))
							{
								charger_type = 1;
								Globa_1->Card_charger_type = 4;
								msg.type = 0x200;//等待刷卡界面
								memset(&Page_200.reserve1, '0', sizeof(PAGE_200_FRAME));
								Page_200.state = 1;//1 没有检测到用户卡
								memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
								msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
								Globa_1->Special_card_offline_flag = 0;

								Globa_1->QT_Step = 0x20; // 2 自动充电-等待刷卡界面  2.0

								count_wait = 0;
								memset(&Globa_1->card_sn[0], '0', sizeof(Globa_1->card_sn));
								memset(&Globa_1->tmp_card_sn[0], '0', sizeof(Globa_1->tmp_card_sn));
								memset(&busy_frame, '0', sizeof(busy_frame));
								memset(&busy_frame_2, '0', sizeof(busy_frame_2));

								memset(Globa_1->VIN, '0', sizeof(Globa_1->VIN));

								if(Globa_1->Charger_param.NEW_VOICE_Flag ==1){
								 Voicechip_operation_function(0x04);//04H     “请刷卡”
								}else{
									Voicechip_operation_function(0x00);//00H	"欢迎光临.wav 300s 请刷卡1.wav"	
								}
								Globa_1->User_Card_check_flag = 0;
								Globa_1->User_Card_check_result = 0;
								Select_NO_Over_count = 0;
								Globa_1->checkout = 0;
								Globa_1->pay_step = 1;  
						    memset(&Globa_1->Bind_BmsVin[0], '0',sizeof(Globa_1->Bind_BmsVin));
							}else{
								msg.type = 0x1100;// 充电方式选择界面
								msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 2, IPC_NOWAIT);//发送两个字节
								Globa_1->QT_Step = 0x121; // 1 自动充电-》充电方式选着 状态  1.2.1 	
								if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
									Voicechip_operation_function(0x02);  //02H"    “请选择充电方式"
								}
								QT_timer_stop(500);
							}
              if(pre_old_id != 0){
								for(i=0;i<5;i++){
									usleep(200000);
									if(Select_NO_Over_Record_1(pre_old_id,0)!= -1){//表示有没有上传的记录
										pthread_mutex_lock(&busy_db_pmutex);
										Update_Charger_Busy_DB_1(pre_old_id, &pre_old_busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
										pthread_mutex_unlock(&busy_db_pmutex);
										Select_NO_Over_count = 0;
									}else{
										pre_old_id = 0;
										Select_NO_Over_count = 0;
										memset(&pre_old_busy_frame, '0', sizeof(pre_old_busy_frame));
										break;
									}
								}
							}else{
								Select_NO_Over_count = 0;
							}
    					break;
    				}
    				case 0x02:{//退出按钮
						  if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
								msg.type = 0x150;
								msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);						
								QT_timer_stop(500);
								Globa_1->QT_Step = 0x03; //自动充电分开的AB通道
						  }else{//单枪
						
								//防止按键处理不同步，此处同步一下
								msg.type = 0x150;//
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
								Globa_2->QT_Step = 0x03; //主界面
								
								msg.type = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选着界面，才有此按钮功能）
					  	  msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						    Globa_1->QT_Step = 0x02; //主界面
							}
    					break;
    				}
    				case 0x03:{//VIN启动
						    Globa_1->QT_gun_select = 1;				
								Globa_1->BMS_Power_V = msg.argv[1];
								if(Globa_1->BMS_Power_V == 1){
									GROUP1_BMS_Power24V_ON;
								}else{
									GROUP1_BMS_Power24V_OFF;
								}
						    Globa_1->kwh = 0;
								ul_start_kwh = 0;
								ul_start_kwh_2 = 0;
								cur_price_index = GetCurPriceIndex(); //获取当前时间
								pre_price_index = cur_price_index;
								for(i=0;i<4;i++){
									g_ulcharged_rmb[i] = 0;
									g_ulcharged_kwh[i] = 0;
									g_ulcharged_rmb_Serv[i] = 0;
									share_time_kwh_price[i] = 0;
									share_time_kwh_price_serve[i] = 0; //服务
									g_ulcharged_rmb_2[i] = 0;
									g_ulcharged_kwh_2[i] = 0;
									g_ulcharged_rmb_Serv_2[i] = 0;
								}
								Globa_1->rmb = 0;
								Globa_1->total_rmb = 0;
								Globa_1->ISO_8583_rmb_Serv = 0;               //总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
								Globa_1->ISO_8583_rmb= 0;                     //电量消费金额 0.01yuan
					
								temp_kwh = 0;
								Globa_1->soft_KWH = 0;
								Globa_1->Time = 0;
								Globa_1->card_type = 0;//卡类型 0:未知 1：用户卡 2：员工卡 3：维护卡

						  charger_type = 1;
    					Globa_1->Card_charger_type = 4;
     					msg.type = 0x200;//等待刷卡界面
     					memset(&Page_200.reserve1, '0', sizeof(PAGE_200_FRAME));
     					Page_200.state = 40;//1 系统启动中,获取VIN吗
							memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
    					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
              Globa_1->Special_card_offline_flag = 0;
    					Globa_1->QT_Step = 0x19; // 2 自动充电-等待刷卡界面  2.0
							Globa_1->VIN_Judgment_Result = 0;
							count_wait = 0;
							memset(&Globa_1->card_sn[0], '0', sizeof(Globa_1->card_sn));
							memset(&Globa_1->tmp_card_sn[0], '0', sizeof(Globa_1->tmp_card_sn));
							memset(&busy_frame, '0', sizeof(busy_frame));
							memset(&busy_frame_2, '0', sizeof(busy_frame_2));
							memset(Globa_1->VIN, '0', sizeof(Globa_1->VIN));
							Globa_1->User_Card_check_flag = 0;
							Globa_1->User_Card_check_result = 0;
			        Select_NO_Over_count = 0;
							Globa_1->Start_Mode = VIN_START_TYPE;//VIN启动
							Globa_1->Manu_start = 0;
		
              if(pre_old_id != 0){
								pthread_mutex_lock(&busy_db_pmutex);
								Set_0X550X66_TO0X00_Busy_ID_Gun_1(1); //强制性更新上传标志
								pthread_mutex_unlock(&busy_db_pmutex);
								pre_old_id = 0;
								Select_NO_Over_count = 0;
								memset(&pre_old_busy_frame, '0', sizeof(pre_old_busy_frame));
							}
							Globa_1->checkout = 0;
							Globa_1->pay_step = 0;

							//插入启动充电数据：
					  if((Globa_1->private_price_acquireOK == 1)){//专有电价
							for(i=0;i<4;i++){
								share_time_kwh_price[i] = Globa_1->Special_price_data.share_time_kwh_price[i];
								share_time_kwh_price_serve[i] = Globa_1->Special_price_data.share_time_kwh_price_serve[i]; //服务
							}
							Serv_Type = Globa_1->Special_price_data.Serv_Type;
						}else{//通用电价
							for(i=0;i<4;i++){
								share_time_kwh_price[i] = Globa_1->Charger_param2.share_time_kwh_price[i];
								share_time_kwh_price_serve[i] = Globa_1->Charger_param2.share_time_kwh_price_serve[i]; //服务
							}
							Serv_Type = Globa_1->Charger_param.Serv_Type;
						}
						memcpy(&busy_frame.card_sn[0], &Globa_1->card_sn[0], sizeof(busy_frame.card_sn));

						systime = time(NULL);         //获得系统的当前时间 
						localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
						sprintf(&temp_char[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
																				 tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
					
						memcpy(&busy_frame.start_time[0], &temp_char[0], sizeof(busy_frame.start_time));   
						memcpy(&busy_frame.end_time[0], &temp_char[0], sizeof(busy_frame.end_time));
					  memcpy(&busy_frame.flag[0], "00", sizeof(busy_frame.flag));//员工卡直接标志位完成，不灰锁
					
						sprintf(&temp_char[0], "%08d", Globa_1->last_rmb);		
						memcpy(&busy_frame.last_rmb[0], &temp_char[0], sizeof(busy_frame.last_rmb));

						memcpy(&busy_frame.ter_sn[0],  &Globa_1->Charger_param.SN[0], sizeof(busy_frame.ter_sn));
						Globa_1->meter_start_KWH = Globa_1->meter_KWH;//取初始电表示数
						Globa_1->meter_stop_KWH = Globa_1->meter_start_KWH;
						sprintf(&temp_char[0], "%08d", Globa_1->meter_start_KWH);    
						memcpy(&busy_frame.start_kwh[0], &temp_char[0], sizeof(busy_frame.start_kwh));
						
						sprintf(&temp_char[0], "%08d", Globa_1->meter_stop_KWH);				
						memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

						sprintf(&temp_char[0], "%08d", Globa_1->Charger_param.Price);    
						memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));

						sprintf(&temp_char[0], "%08d", Globa_1->kwh);				
						memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
						
						sprintf(&temp_char[0], "%08d", Globa_1->rmb);				
						memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));

						//起始总费用等于电量费用，暂时只做安电量收服务费
						sprintf(&temp_char[0], "%08d", Globa_1->total_rmb);
						memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));
						sprintf(&temp_char[0], "%08d", Globa_1->total_rmb-Globa_1->rmb);
						memcpy(&busy_frame.server_rmb[0], &temp_char[0], sizeof(busy_frame.server_rmb));
						busy_frame.type[0] = '0';//充电方式（01：刷卡，02：手机）
						busy_frame.type[1] = '3';
						sprintf(&temp_char[0], "%02d", Serv_Type);//服务费收费类型 01：按次收费；02：按时间收费(单位：分/10分钟；03：按电量	9	N	N2
						memcpy(&busy_frame.Serv_Type[0], &temp_char[0], sizeof(busy_frame.Serv_Type));
						memset(&busy_frame.car_sn[0], '0', sizeof(busy_frame.car_sn));//
						tmp_value = share_time_kwh_price[0];
						sprintf(&temp_char[0], "%08d", tmp_value);				
						memcpy(&busy_frame.sharp_rmb_kwh[0], &temp_char[0], sizeof(busy_frame.sharp_rmb_kwh));

						tmp_value = g_ulcharged_rmb[0];
						sprintf(&temp_char[0], "%08d", tmp_value);	
						memcpy(&busy_frame.sharp_allrmb[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb));
					
						tmp_value = share_time_kwh_price[1];
						sprintf(&temp_char[0], "%08d", tmp_value);	
						memcpy(&busy_frame.peak_rmb_kwh[0], &temp_char[0], sizeof(busy_frame.peak_rmb_kwh));
						tmp_value = g_ulcharged_rmb[1];
						sprintf(&temp_char[0], "%08d", tmp_value);
						memcpy(&busy_frame.peak_allrmb[0], &temp_char[0], sizeof(busy_frame.peak_allrmb));
						
						tmp_value = share_time_kwh_price[2];
						sprintf(&temp_char[0], "%08d", tmp_value);	
						memcpy(&busy_frame.flat_rmb_kwh[0], &temp_char[0], sizeof(busy_frame.flat_rmb_kwh));
						tmp_value = g_ulcharged_rmb[2];
						sprintf(&temp_char[0], "%08d", tmp_value);		
						memcpy(&busy_frame.flat_allrmb[0], &temp_char[0], sizeof(busy_frame.flat_allrmb));
					
						tmp_value = share_time_kwh_price[3];
						sprintf(&temp_char[0], "%08d", tmp_value);				
						memcpy(&busy_frame.valley_rmb_kwh[0], &temp_char[0], sizeof(busy_frame.valley_rmb_kwh));
						tmp_value = g_ulcharged_rmb[3];
						sprintf(&temp_char[0], "%08d", tmp_value);	
						memcpy(&busy_frame.valley_allrmb[0], &temp_char[0], sizeof(busy_frame.valley_allrmb));
					
  					tmp_value = share_time_kwh_price_serve[0];
						sprintf(&temp_char[0], "%08d", tmp_value);				
						memcpy(&busy_frame.sharp_rmb_kwh_Serv[0], &temp_char[0], sizeof(busy_frame.sharp_rmb_kwh_Serv));

						tmp_value = g_ulcharged_rmb_Serv[0];
						sprintf(&temp_char[0], "%08d", tmp_value);	
						memcpy(&busy_frame.sharp_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb_Serv));
					
						tmp_value = share_time_kwh_price_serve[1];
						sprintf(&temp_char[0], "%08d", tmp_value);	
						memcpy(&busy_frame.peak_rmb_kwh_Serv[0], &temp_char[0], sizeof(busy_frame.peak_rmb_kwh_Serv));
						tmp_value = g_ulcharged_rmb_Serv[1];
						sprintf(&temp_char[0], "%08d", tmp_value);
						memcpy(&busy_frame.peak_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.peak_allrmb_Serv));
						
						tmp_value = share_time_kwh_price_serve[2];
						sprintf(&temp_char[0], "%08d", tmp_value);	
						memcpy(&busy_frame.flat_rmb_kwh_Serv[0], &temp_char[0], sizeof(busy_frame.flat_rmb_kwh_Serv));
						tmp_value = g_ulcharged_rmb_Serv[2];
						sprintf(&temp_char[0], "%08d", tmp_value);		
						memcpy(&busy_frame.flat_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.flat_allrmb_Serv));
					
						tmp_value = share_time_kwh_price_serve[3];
						sprintf(&temp_char[0], "%08d", tmp_value);				
						memcpy(&busy_frame.valley_rmb_kwh_Serv[0], &temp_char[0], sizeof(busy_frame.valley_rmb_kwh_Serv));
						tmp_value = g_ulcharged_rmb_Serv[3];
						sprintf(&temp_char[0], "%08d", tmp_value);	
						memcpy(&busy_frame.valley_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.valley_allrmb_Serv));
						busy_frame.ConnectorId[0] = '0';//充电枪号
						busy_frame.ConnectorId[1] = '1';
						busy_frame.End_code[0] = '1';//结束代码
						busy_frame.End_code[1] = '1';
						
						tmp_value =0;
						id = 0;
						pthread_mutex_lock(&busy_db_pmutex);
						Insertflag = Insert_Charger_Busy_DB_1(&busy_frame, &id, 0x55);//写新数据库记录 无流水号 传输标志0x55不能上传
						pthread_mutex_unlock(&busy_db_pmutex);
				    if(DEBUGLOG) RUN_GUN1_LogOut("%s %d","一号枪:刷卡启动充电并插入交易记录中的ID=",id);  
            if((Insertflag  == -1)&&(id == 0)){//数据有异常
							 Page_1300.cause = 63;
						   Page_1300.Capact = Globa_1->BMS_batt_SOC;
							 Page_1300.elapsed_time[0] =  Globa_1->Time;
							 Page_1300.elapsed_time[1] =  Globa_1->Time>>8;
							 Page_1300.KWH[0] =  Globa_1->kwh;
							 Page_1300.KWH[1] =  Globa_1->kwh>>8;
							 Page_1300.RMB[0] =  Globa_1->total_rmb;
							 Page_1300.RMB[1] =  Globa_1->total_rmb>>8;
							 Page_1300.RMB[2] =  Globa_1->total_rmb>>16;
					     Page_1300.RMB[3] =  Globa_1->total_rmb>>24;
							 memcpy(&Page_1300.SN[0], &Globa_1->card_sn[0], sizeof(Page_1300.SN));
							// Page_1300.APP = 3;   //充电启动方式 1:刷卡  2：手机APP 3 VIN
							#ifdef CHECK_BUTTON								
								Page_1300.APP  = 3;   //充电启动方式 1:刷卡  2：手机APP
							#else 
								Page_1300.APP  = 1;   //充电启动方式 1:刷卡  2：手机APP
							#endif	
							 memcpy(&msg.argv[0], &Page_1300.reserve1, sizeof(PAGE_1300_FRAME));
							 if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
							  	msg.type = 0x1300;//充电结束界面
							 }else{
							  	msg.type = 0x1310;//单枪结束界面
							 }
							
							 msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_1300_FRAME), IPC_NOWAIT);
							 Globa_1->QT_Step = 0x23; // 2      充电结束 结算界面 2.3----------------
		           Globa_1->Manu_start = 0;
						   Globa_1->charger_start =0;
							 Globa_1->charger_state = 0x05;
							 Globa_1->checkout = 0;
							 Globa_1->pay_step = 0;//1通道在等待刷卡扣费
							 card_falg_count = 0;
							 Globa_1->Charger_Over_Rmb_flag = 0;
							 if(DEBUGLOG) RUN_GUN1_LogOut("%s %d","一号枪:数据库异常请处理=",id);  
							 break;
					   }
						//------------------------------------------------------------------
					   Globa_1->VIN_Judgment_Result = 0;
						 Globa_1->Manu_start = 1;
						 Globa_1->charger_start = 1;
						 Globa_1->charger_state = 0x04; //启动充电
    					break;
    				}
						default:{
    					break;
    				}
    			}
    		}else if(QT_timer_tick(500) == 1){  //定时500MS
					Page_1000.link = (Globa_1->gun_link == 1)?1:0;  //1:连接  0：断开
					Page_1000.VIN_Support = (((Globa_1->have_hart == 1)&&(Globa_1->VIN_Support_falg == 1)) == 1)? 1:0;
					if((Globa_1->Double_Gun_To_Car_falg == 1)&&(Globa_1->Charger_param.System_Type == 4))
					{
					  Page_1000.other_link = (1<<4)|(Globa_2->gun_link);
					}else{
						Page_1000.other_link = 0;
					}

					msg.type = 0x1000;
					memcpy(&msg.argv[0], &Page_1000.reserve1, sizeof(PAGE_1000_FRAME));
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_1000_FRAME), IPC_NOWAIT);
					QT_timer_start(500);
				}

				break;
			}
			
			case 0x121:{//--------------1 自动充电-》 充电方式选着 状态  1.2.1--------
    		if(msg.type == 0x1100){//收到该界面消息
    			switch(msg.argv[0]){//判断 在当前界面下 用户操作的按钮
    				case 0x01:{//自动充电 按钮
          		charger_type = 1;
    					Globa_1->Card_charger_type = 4;
     					msg.type = 0x200;//等待刷卡界面
     					memset(&Page_200.reserve1, '0', sizeof(PAGE_200_FRAME));
     					Page_200.state = 1;//1 没有检测到用户卡
							memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
    					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
              Globa_1->Special_card_offline_flag = 0;

    					Globa_1->QT_Step = 0x20; // 2 自动充电-等待刷卡界面  2.0

							count_wait = 0;
							memset(&Globa_1->card_sn[0], '0', sizeof(Globa_1->card_sn));
							memset(&Globa_1->tmp_card_sn[0], '0', sizeof(Globa_1->tmp_card_sn));
							memset(&busy_frame, '0', sizeof(busy_frame));
							memset(Globa_1->VIN, '0', sizeof(Globa_1->VIN));

              if(Globa_1->Charger_param.NEW_VOICE_Flag ==1){
							 Voicechip_operation_function(0x04);//04H     “请刷卡”
						  }else{
								Voicechip_operation_function(0x00);//00H	"欢迎光临.wav 300s 请刷卡1.wav"	
							}
							Globa_1->User_Card_check_flag = 0;
							Globa_1->User_Card_check_result = 0;
			        Select_NO_Over_count = 0;
							Globa_1->Start_Mode = CARD_START_TYPE;//刷卡启动
              if(pre_old_id != 0){
								pthread_mutex_lock(&busy_db_pmutex);
								Set_0X550X66_TO0X00_Busy_ID_Gun_1(1); //强制性更新上传标志
								pthread_mutex_unlock(&busy_db_pmutex);
								pre_old_id = 0;
								Select_NO_Over_count = 0;
								memset(&pre_old_busy_frame, '0', sizeof(pre_old_busy_frame));
							}
							
							Globa_1->checkout = 0;
							Globa_1->pay_step = 1;
    					break;
    				}
    				case 0x02:{//按金额充电 按钮
     					msg.type = 0x1120; // 按金额充电界面
    					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);

    					Globa_1->QT_Step = 0x131; // 1 自动充电-》 输入 状态  1.3.1---

    					break;
    				}
    				case 0x03:{//按电量充电 按钮
     					msg.type = 0x1130; // 按电量充电界面
    					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);

    					Globa_1->QT_Step = 0x131; // 1 自动充电-》 输入 状态  1.3.1---

    					break;
    				}
    				case 0x04:{//按时间充电 按钮
     					msg.type = 0x1140; // 按时间充电界面
    					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);

    					Globa_1->QT_Step = 0x131; // 1 自动充电-》 输入 状态  1.3.1---

    					break;
    				}
    				case 0x05:{//预约充电 按钮
     					msg.type = 0x1150; // 预约充电界面
    					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);

    					Globa_1->QT_Step = 0x131;//1 自动充电-》 输入 状态  1.3.1---

    					break;
    				}
    				case 0x07:{//退出按钮
						  if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
								msg.type = 0x150;
								msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);						
								QT_timer_stop(500);
								Globa_1->QT_Step = 0x03; //自动充电分开的AB通道
						  }else{//单枪
						
								//防止按键处理不同步，此处同步一下
								msg.type = 0x150;//
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
								Globa_2->QT_Step = 0x03; //主界面
								
								msg.type = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选着界面，才有此按钮功能）
					  	  msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						    Globa_1->QT_Step = 0x02; //主界面
							}

    					break;
    				}
    				default:{
    					break;
    				}
    			}
    		}

				break;
			}
			
			case 0x131:{//--------------1 自动充电-》 输入 状态  1.3.1----------------
    		if((msg.type == 0x1120)||(msg.type == 0x1130)||(msg.type == 0x1140)||(msg.type == 0x1150)){//收到该界面消息
    			switch(msg.argv[0]){//判断 在当前界面下 用户操作的按钮
    				case 0x01:{//确认按钮
    					//根据不同的充电方式 输入界面 分别处理输入的数据
      				if(msg.type == 0x1120){//按金额充电
          			Globa_1->QT_charge_money = (msg.argv[2]<<8) + msg.argv[1];
          			charger_type = 2;
								Globa_1->Card_charger_type = 3;
          		}else if(msg.type == 0x1130){//按电量充电
          			Globa_1->QT_charge_kwh =   (msg.argv[2]<<8) + msg.argv[1];
          			charger_type = 3;
								Globa_1->Card_charger_type = 1;
          		}else if(msg.type == 0x1140){//按时间充电
          			Globa_1->QT_charge_time =  ((msg.argv[2]<<8) + msg.argv[1])*60 + Globa_1->user_sys_time;//此处保存的是需要结束充电的时间
   			        Globa_1->APP_Card_charge_time_limit =  ((msg.argv[2]<<8) + msg.argv[1])*60;
          			charger_type = 4;
	              Globa_1->Card_charger_type = 2;
          		}else if(msg.type == 0x1150){//预约时间
								time_t systime1;
								struct tm tm_t1;
								
								systime1 = time(NULL);        //获得系统的当前时间 
								localtime_r(&systime1, &tm_t1);  //结构指针指向当前时间	
            		if((tm_t1.tm_min + tm_t1.tm_hour*60)> (msg.argv[2] + msg.argv[1]*60)){//需要隔天 ---倒计时总时间
									Globa_1->QT_pre_time = systime1 + (24*3600 - (tm_t1.tm_hour*3600 + tm_t1.tm_min*60 + tm_t1.tm_sec)) + (msg.argv[2] + msg.argv[1]*60)*60;//最终目标值
								}else{
									Globa_1->QT_pre_time = systime1 + (msg.argv[2] + msg.argv[1]*60)*60 - (tm_t1.tm_sec + tm_t1.tm_min*60 + tm_t1.tm_hour*3600);//此处保存的是需要开始充电的时间  
								}
								
								charger_type = 5;
	              Globa_1->Card_charger_type = 5;
      				}
							
							Select_NO_Over_count = 0;
							
							
              if(pre_old_id != 0){
								pthread_mutex_lock(&busy_db_pmutex);
								Set_0X550X66_TO0X00_Busy_ID_Gun_1(1); //强制性更新上传标志
								pthread_mutex_unlock(&busy_db_pmutex);
								pre_old_id = 0;
								Select_NO_Over_count = 0;
								memset(&pre_old_busy_frame, '0', sizeof(pre_old_busy_frame));
							}
				
							
     					msg.type = 0x200;//等待刷卡界面
     					memset(&Page_200.reserve1, '0', sizeof(PAGE_200_FRAME));
     					Page_200.state = 1;//1 没有检测到用户卡
							memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
    					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
              Globa_1->Special_card_offline_flag = 0;

    					Globa_1->QT_Step = 0x20; // 2 自动充电-等待刷卡界面  2.0
							count_wait = 0;
							memset(&Globa_1->card_sn[0], '0', sizeof(Globa_1->card_sn));
							memset(&Globa_1->tmp_card_sn[0], '0', sizeof(Globa_1->tmp_card_sn));
							memset(&busy_frame, '0', sizeof(busy_frame));
							memset(Globa_1->VIN, '0', sizeof(Globa_1->VIN));

             if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
							 Voicechip_operation_function(0x04);//04H     “请刷卡”
						  }else{
								Voicechip_operation_function(0x00);//00H	"欢迎光临.wav 300s 请刷卡1.wav"	
							}
              Globa_1->User_Card_check_flag = 0;
							Globa_1->User_Card_check_result = 0;
							Globa_1->Start_Mode = CARD_START_TYPE;//刷卡启动
   						Globa_1->checkout = 0;
							Globa_1->pay_step = 1;
    					break;
    				}
    				case 0x02:{//返回按钮
							msg.type = 0x1100;
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 2, IPC_NOWAIT);					
              if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
						  	Voicechip_operation_function(0x02);  //02H"    “请选择充电方式"
							}
							Globa_1->QT_Step = 0x121; //1 自动充电-》充电方式选着 状态  1.2.1

    					break;
    				}
    				default:{
    					break;
    				}
    			}
    		}

				break;
			}
		  case 0x19:{//---------------2 自动充电-》 VIN   2.0--------------
				if(Globa_1->have_hart == 1){
					if((Globa_1->User_Card_check_result == 0)&&(Globa_1->User_Card_check_flag == 0)){
					  if(Globa_1->BMS_Info_falg  == 1) //正确获取到了VIN码
						{
						  if((0 != memcmp(&Globa_1->VIN[0], "00000000000000000", sizeof(Globa_1->VIN)))&&(!((Globa_1->VIN[0] == 0xFF)&&(Globa_1->VIN[1] == 0xFF)&&(Globa_1->VIN[2] == 0xFF)))){
								msg.type = 0x200;//等待刷卡界面
								Page_200.state = 41;//卡片验证中...请保持刷卡状态！
								memcpy(&Page_200.sn[0], &Globa_1->VIN[0], sizeof(Page_200.sn));
								memcpy(&Page_200.rmb[0], &busy_frame.last_rmb[0], sizeof(Page_200.rmb));
								memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
								msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
								memcpy(&Globa_1->User_Card_check_card_ID[0], &Globa_1->card_sn[0], sizeof(Globa_1->User_Card_check_card_ID));
								Globa_1->User_Card_check_flag = 1;
								break;
							}else{
								Globa_1->User_Card_check_flag = 0;
								Globa_1->VIN_Judgment_Result = 2;
								Globa_1->Manu_start = 0;
								msg.type = 0x200;//等待刷卡界面
								Page_200.state = 19;//14;//MAC校验错误
								memcpy(&Page_200.sn[0], &Globa_1->card_sn[0], sizeof(Page_200.sn));
								sprintf(&temp_char[0], "%08d", Globa_1->last_rmb);	
								memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
								memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
								msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
								Globa_1->Operation_command = 2; //解锁
								memcpy(&busy_frame.car_sn[0],Globa_1->VIN,sizeof(Globa_1->VIN));//
								pthread_mutex_lock(&busy_db_pmutex);
								Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00);
								all_id = 0;
								Insertflag = Insert_Charger_Busy_DB(&busy_frame, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
								if((Insertflag != -1)&&(all_id != 0)){//表示插入数据成功，其他的则需要重新插入
									Delete_Record_busy_DB(id);
									if(DEBUGLOG) RUN_GUN1_LogOut("一号枪交易数据,插入到正式数据库中并删除临时数据库记录 ID = %d",id);  
									id = 0;
								}else{
									if(DEBUGLOG) RUN_GUN1_LogOut("一号枪交易数据,插入到正式数据库中失败 ID = %d",id);  
								}
								pthread_mutex_unlock(&busy_db_pmutex);

								if(id != 0){
									pthread_mutex_lock(&busy_db_pmutex);
									for(i=0;i<5;i++){
										if(Select_NO_Over_Record_1(id,0)!= -1){//表示有没有上传的记录
											Select_NO_Over_count = 0;
											memcpy(&pre_old_busy_frame, &busy_frame, sizeof(pre_old_busy_frame));
											pre_old_id = id;
											Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
										}else{
											if(DEBUGLOG) RUN_GUN1_LogOut("%s %d","一号枪交易数据,更新upm=0x00成功 ID=",id);  
											Select_NO_Over_count = 0;
											pre_old_id = 0;	
											id = 0;
											break;
										}
										sleep(1);
									}
									pthread_mutex_unlock(&busy_db_pmutex);
								}else{
									pre_old_id = 0;	
									id = 0;
								}

								sleep(4);
								if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
									msg.type = 0x150;
									msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);						
									QT_timer_stop(500);
									Globa_1->QT_Step = 0x03; //自动充电分开的AB通道
								}else{//单枪
									//防止按键处理不同步，此处同步一下
									msg.type = 0x150;//
									msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
									Globa_2->QT_Step = 0x03; //主界面
									
									msg.type = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选着界面，才有此按钮功能）
									msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
									Globa_1->QT_Step = 0x02; //主界面
								}
								break;
							}
					  }
					}
				}
				/*else{//离线桩
					Globa_1->User_Card_check_flag = 0;
					Globa_1->User_Card_check_result = 0;
					Globa_1->Manu_start = 0;
					msg.type = 0x200;//等待刷卡界面
					Page_200.state = 42 ;//离线不能获取结果
					memcpy(&Page_200.sn[0], &busy_frame.card_sn[0], sizeof(Page_200.sn));
					memcpy(&Page_200.rmb[0], &busy_frame.last_rmb[0], sizeof(Page_200.rmb));
					memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
					memcpy(&Globa_1->User_Card_check_card_ID[0], &Globa_1->card_sn[0], sizeof(Globa_1->User_Card_check_card_ID));
					Globa_1->VIN_Judgment_Result = 2;
				
   				pthread_mutex_lock(&busy_db_pmutex);
					Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00);
					all_id = 0;
					Insertflag = Insert_Charger_Busy_DB(&busy_frame, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
					if((Insertflag != -1)&&(all_id != 0)){//表示插入数据成功，其他的则需要重新插入
						Delete_Record_busy_DB(id);
						if(DEBUGLOG) RUN_GUN1_LogOut("一号枪交易数据,插入到正式数据库中并删除临时数据库记录 ID = %d",id);  
						id = 0;
					}else{
						if(DEBUGLOG) RUN_GUN1_LogOut("一号枪交易数据,插入到正式数据库中失败 ID = %d",id);  
					}
					pthread_mutex_unlock(&busy_db_pmutex);

					if(id != 0){
						pthread_mutex_lock(&busy_db_pmutex);
						for(i=0;i<5;i++){
							if(Select_NO_Over_Record_1(id,0)!= -1){//表示有没有上传的记录
								Select_NO_Over_count = 0;
								memcpy(&pre_old_busy_frame, &busy_frame, sizeof(pre_old_busy_frame));
								pre_old_id = id;
								Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
							}else{
								if(DEBUGLOG) RUN_GUN1_LogOut("%s %d","一号枪交易数据,更新upm=0x00成功 ID=",id);  
								Select_NO_Over_count = 0;
								pre_old_id = 0;	
								id = 0;
								break;
							}
							sleep(1);
						}
						pthread_mutex_unlock(&busy_db_pmutex);
					}else{
						pre_old_id = 0;	
						id = 0;
					}
					Globa_1->Operation_command = 2; //解锁
					sleep(3);
					if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
						msg.type = 0x150;
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);						
						QT_timer_stop(500);
						Globa_1->QT_Step = 0x03; //自动充电分开的AB通道
					}else{//单枪
						//防止按键处理不同步，此处同步一下
						msg.type = 0x150;//
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_2->QT_Step = 0x03; //主界面
						msg.type = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选着界面，才有此按钮功能）
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_1->QT_Step = 0x02; //主界面
					}
				}*/
				
			  if((Globa_1->User_Card_check_result >= 2)&&(Globa_1->User_Card_check_result <= 14)){
					Globa_1->User_Card_check_flag = 0;
					Globa_1->VIN_Judgment_Result = 2;
	        Globa_1->Manu_start = 0;
					msg.type = 0x200;//等待刷卡界面
					Page_200.state = 17 + (Globa_1->User_Card_check_result - 2 );//14;//MAC校验错误
					memcpy(&Page_200.sn[0], &Globa_1->card_sn[0], sizeof(Page_200.sn));
					sprintf(&temp_char[0], "%08d", Globa_1->last_rmb);	
					memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
					memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
				 	Globa_1->Operation_command = 2; //解锁
					memcpy(&busy_frame.car_sn[0],Globa_1->VIN,sizeof(Globa_1->VIN));//
					pthread_mutex_lock(&busy_db_pmutex);
					Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00);
					all_id = 0;
					Insertflag = Insert_Charger_Busy_DB(&busy_frame, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
					if((Insertflag != -1)&&(all_id != 0)){//表示插入数据成功，其他的则需要重新插入
						Delete_Record_busy_DB(id);
						if(DEBUGLOG) RUN_GUN1_LogOut("一号枪交易数据,插入到正式数据库中并删除临时数据库记录 ID = %d",id);  
						id = 0;
					}else{
						if(DEBUGLOG) RUN_GUN1_LogOut("一号枪交易数据,插入到正式数据库中失败 ID = %d",id);  
					}
					pthread_mutex_unlock(&busy_db_pmutex);

					if(id != 0){
						pthread_mutex_lock(&busy_db_pmutex);
						for(i=0;i<5;i++){
							if(Select_NO_Over_Record_1(id,0)!= -1){//表示有没有上传的记录
								Select_NO_Over_count = 0;
								memcpy(&pre_old_busy_frame, &busy_frame, sizeof(pre_old_busy_frame));
								pre_old_id = id;
								Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
							}else{
								if(DEBUGLOG) RUN_GUN1_LogOut("%s %d","一号枪交易数据,更新upm=0x00成功 ID=",id);  
								Select_NO_Over_count = 0;
								pre_old_id = 0;	
								id = 0;
								break;
							}
							sleep(1);
						}
						pthread_mutex_unlock(&busy_db_pmutex);
					}else{
						pre_old_id = 0;	
						id = 0;
					}

					sleep(4);
					if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
						msg.type = 0x150;
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);						
						QT_timer_stop(500);
						Globa_1->QT_Step = 0x03; //自动充电分开的AB通道
					}else{//单枪
						//防止按键处理不同步，此处同步一下
						msg.type = 0x150;//
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_2->QT_Step = 0x03; //主界面
						
						msg.type = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选着界面，才有此按钮功能）
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_1->QT_Step = 0x02; //主界面
					}
					break;//退出，不能充电
				}
				else if(Globa_1->User_Card_check_result == 1){//验证成功请在次刷卡
					Globa_1->User_Card_check_flag = 0;
					Globa_1->User_Card_check_result = 0;
					if((0 == memcmp(&Globa_1->card_sn[0], "0000000000000000", sizeof(Globa_1->card_sn))))
					{//获取vin 对应的卡号
		        Globa_1->User_Card_check_flag = 0;
						Globa_1->VIN_Judgment_Result = 2;
						Globa_1->Manu_start = 0;
						msg.type = 0x200;//等待刷卡界面
						Page_200.state = 42;//14;//MAC校验错误
						memcpy(&Page_200.sn[0], &Globa_1->card_sn[0], sizeof(Page_200.sn));
						sprintf(&temp_char[0], "%08d", Globa_1->last_rmb);	
						memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
						memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
						Globa_1->Operation_command = 2; //解锁
						memcpy(&busy_frame.car_sn[0],Globa_1->VIN,sizeof(Globa_1->VIN));//
						pthread_mutex_lock(&busy_db_pmutex);
						Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00);
						all_id = 0;
						Insertflag = Insert_Charger_Busy_DB(&busy_frame, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
						if((Insertflag != -1)&&(all_id != 0)){//表示插入数据成功，其他的则需要重新插入
							Delete_Record_busy_DB(id);
							if(DEBUGLOG) RUN_GUN1_LogOut("一号枪交易数据,插入到正式数据库中并删除临时数据库记录 ID = %d",id);  
							id = 0;
						}else{
							if(DEBUGLOG) RUN_GUN1_LogOut("一号枪交易数据,插入到正式数据库中失败 ID = %d",id);  
						}
						pthread_mutex_unlock(&busy_db_pmutex);

						if(id != 0){
							pthread_mutex_lock(&busy_db_pmutex);
							for(i=0;i<5;i++){
								if(Select_NO_Over_Record_1(id,0)!= -1){//表示有没有上传的记录
									Select_NO_Over_count = 0;
									memcpy(&pre_old_busy_frame, &busy_frame, sizeof(pre_old_busy_frame));
									pre_old_id = id;
									Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
								}else{
									if(DEBUGLOG) RUN_GUN1_LogOut("%s %d","一号枪交易数据,更新upm=0x00成功 ID=",id);  
									Select_NO_Over_count = 0;
									pre_old_id = 0;	
									id = 0;
									break;
								}
								sleep(1);
							}
							pthread_mutex_unlock(&busy_db_pmutex);
						}else{
							pre_old_id = 0;	
							id = 0;
						}

						sleep(4);
						if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
							msg.type = 0x150;
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);						
							QT_timer_stop(500);
							Globa_1->QT_Step = 0x03; //自动充电分开的AB通道
						}else{//单枪
							//防止按键处理不同步，此处同步一下
							msg.type = 0x150;//
							msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa_2->QT_Step = 0x03; //主界面
							
							msg.type = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选着界面，才有此按钮功能）
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa_1->QT_Step = 0x02; //主界面
						}
						break;
					}
				
   				Globa_1->pay_step = 0;//进入空闲状态
					Globa_1->card_type = 2; //卡类型 0:未知 1：用户卡 2：员工卡 3：维护卡卡类型 1：用户卡 2：员工卡
					if((Globa_1->private_price_acquireOK == 1)){//专有电价
						for(i=0;i<4;i++){
							share_time_kwh_price[i] = Globa_1->Special_price_data.share_time_kwh_price[i];
							share_time_kwh_price_serve[i] = Globa_1->Special_price_data.share_time_kwh_price_serve[i]; //服务
						}
						Serv_Type = Globa_1->Special_price_data.Serv_Type;
					}else{//通用电价
						for(i=0;i<4;i++){
							share_time_kwh_price[i] = Globa_1->Charger_param2.share_time_kwh_price[i];
							share_time_kwh_price_serve[i] = Globa_1->Charger_param2.share_time_kwh_price_serve[i]; //服务
						}
						Serv_Type = Globa_1->Charger_param.Serv_Type;
					}
					memcpy(&busy_frame.card_sn[0], &Globa_1->card_sn[0], sizeof(busy_frame.card_sn));

					memcpy(&busy_frame.end_time[0], &temp_char[0], sizeof(busy_frame.end_time));
					memcpy(&busy_frame.flag[0], "00", sizeof(busy_frame.flag));//员工卡直接标志位完成，不灰锁
				
					sprintf(&temp_char[0], "%08d", Globa_1->last_rmb);		
					memcpy(&busy_frame.last_rmb[0], &temp_char[0], sizeof(busy_frame.last_rmb));

					memcpy(&busy_frame.ter_sn[0],  &Globa_1->Charger_param.SN[0], sizeof(busy_frame.ter_sn));
				 //Globa_1->meter_start_KWH = Globa_1->meter_KWH;//取初始电表示数
					Globa_1->meter_stop_KWH = Globa_1->meter_start_KWH;
					sprintf(&temp_char[0], "%08d", Globa_1->meter_start_KWH);    
					memcpy(&busy_frame.start_kwh[0], &temp_char[0], sizeof(busy_frame.start_kwh));
					
					sprintf(&temp_char[0], "%08d", Globa_1->meter_stop_KWH);				
					memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

					sprintf(&temp_char[0], "%08d", Globa_1->Charger_param.Price);    
					memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));

					sprintf(&temp_char[0], "%08d", Globa_1->kwh);				
					memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
					
					sprintf(&temp_char[0], "%08d", Globa_1->rmb);				
					memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));
					
					memcpy(&busy_frame.car_sn[0],Globa_1->VIN,sizeof(Globa_1->VIN));//

					//起始总费用等于电量费用，暂时只做安电量收服务费
					sprintf(&temp_char[0], "%08d", Globa_1->total_rmb);
					memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));
					sprintf(&temp_char[0], "%08d", Globa_1->total_rmb-Globa_1->rmb);
					memcpy(&busy_frame.server_rmb[0], &temp_char[0], sizeof(busy_frame.server_rmb));
					busy_frame.type[0] = '0';//充电方式（01：刷卡，02：手机）
					busy_frame.type[1] = '3';
					sprintf(&temp_char[0], "%02d", Serv_Type);//服务费收费类型 01：按次收费；02：按时间收费(单位：分/10分钟；03：按电量	9	N	N2
					memcpy(&busy_frame.Serv_Type[0], &temp_char[0], sizeof(busy_frame.Serv_Type));

				//	memset(&busy_frame.car_sn[0], '0', sizeof(busy_frame.car_sn));//
					tmp_value = share_time_kwh_price[0];
					sprintf(&temp_char[0], "%08d", tmp_value);				
					memcpy(&busy_frame.sharp_rmb_kwh[0], &temp_char[0], sizeof(busy_frame.sharp_rmb_kwh));

					tmp_value = g_ulcharged_rmb[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.sharp_allrmb[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb));
				
					tmp_value = share_time_kwh_price[1];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.peak_rmb_kwh[0], &temp_char[0], sizeof(busy_frame.peak_rmb_kwh));
					tmp_value = g_ulcharged_rmb[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame.peak_allrmb[0], &temp_char[0], sizeof(busy_frame.peak_allrmb));
					
					tmp_value = share_time_kwh_price[2];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.flat_rmb_kwh[0], &temp_char[0], sizeof(busy_frame.flat_rmb_kwh));
					tmp_value = g_ulcharged_rmb[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame.flat_allrmb[0], &temp_char[0], sizeof(busy_frame.flat_allrmb));
				
					tmp_value = share_time_kwh_price[3];
					sprintf(&temp_char[0], "%08d", tmp_value);				
					memcpy(&busy_frame.valley_rmb_kwh[0], &temp_char[0], sizeof(busy_frame.valley_rmb_kwh));
					tmp_value = g_ulcharged_rmb[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.valley_allrmb[0], &temp_char[0], sizeof(busy_frame.valley_allrmb));
				
					tmp_value = share_time_kwh_price_serve[0];
					sprintf(&temp_char[0], "%08d", tmp_value);				
					memcpy(&busy_frame.sharp_rmb_kwh_Serv[0], &temp_char[0], sizeof(busy_frame.sharp_rmb_kwh_Serv));

					tmp_value = g_ulcharged_rmb_Serv[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.sharp_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb_Serv));
				
					tmp_value = share_time_kwh_price_serve[1];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.peak_rmb_kwh_Serv[0], &temp_char[0], sizeof(busy_frame.peak_rmb_kwh_Serv));
					tmp_value = g_ulcharged_rmb_Serv[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame.peak_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.peak_allrmb_Serv));
					
					tmp_value = share_time_kwh_price_serve[2];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.flat_rmb_kwh_Serv[0], &temp_char[0], sizeof(busy_frame.flat_rmb_kwh_Serv));
					tmp_value = g_ulcharged_rmb_Serv[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame.flat_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.flat_allrmb_Serv));
				
					tmp_value = share_time_kwh_price_serve[3];
					sprintf(&temp_char[0], "%08d", tmp_value);				
					memcpy(&busy_frame.valley_rmb_kwh_Serv[0], &temp_char[0], sizeof(busy_frame.valley_rmb_kwh_Serv));
					tmp_value = g_ulcharged_rmb_Serv[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.valley_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.valley_allrmb_Serv));
					busy_frame.ConnectorId[0] = '0';//充电枪号
					busy_frame.ConnectorId[1] = '1';
					busy_frame.End_code[0] = '1';//结束代码
					busy_frame.End_code[1] = '1';
					
					memcpy(&busy_frame.car_sn[0],Globa_1->VIN,sizeof(Globa_1->VIN));//

					tmp_value =0;
					if(id != 0 ){
						pthread_mutex_lock(&busy_db_pmutex);
						Delete_Record_busy_DB(id);
						pthread_mutex_unlock(&busy_db_pmutex);
						sleep(1);
					}
			    	id = 0;
						pthread_mutex_lock(&busy_db_pmutex);
						Insertflag = Insert_Charger_Busy_DB_1(&busy_frame, &id, 0x55);//写新数据库记录 无流水号 传输标志0x55不能上传
						pthread_mutex_unlock(&busy_db_pmutex);
						if(Insertflag == -1){
						AP_APP_Sent_1_falg[1] = 6;
						AP_APP_Sent_1_falg[0] = 1;
						Page_1300.cause = 63;//数据库异常;
						Page_1300.Capact = Globa_1->BMS_batt_SOC;
						Page_1300.elapsed_time[0] =  Globa_1->Time;
						Page_1300.elapsed_time[1] =  Globa_1->Time>>8;
						Page_1300.KWH[0] =  Globa_1->kwh;
						Page_1300.KWH[1] =  Globa_1->kwh>>8;
						Page_1300.RMB[0] =  Globa_1->total_rmb;
						Page_1300.RMB[1] =  Globa_1->total_rmb>>8;
						Page_1300.RMB[2] =  Globa_1->total_rmb>>16;
					  Page_1300.RMB[3] =  Globa_1->total_rmb>>24;
					
						memcpy(&Page_1300.SN[0], &busy_frame.card_sn[0], sizeof(Page_1300.SN));
						Page_1300.APP = 2;   //充电启动方式 1:刷卡  2：手机APP
						memcpy(&msg.argv[0], &Page_1300.reserve1, sizeof(PAGE_1300_FRAME));
						if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
							msg.type = 0x1300;//充电结束界面
						}else{
							msg.type = 0x1310;//单枪结束界面
						}
						Globa_1->Manu_start = 0;
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_1300_FRAME), IPC_NOWAIT);
						sleep(3);
						Globa_1->BMS_Power_V = 0;
						Globa_1->Operation_command = 2;//进行解锁
						memset(&Globa_1->App.ID, '0', sizeof(Globa_1->App.ID));
						if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
							msg.type = 0x150;
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);						
							QT_timer_stop(500);
							Globa_1->QT_Step = 0x03; //自动充电分开的AB通道
						}else{//单枪
							//防止按键处理不同步，此处同步一下
							msg.type = 0x150;//
							msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa_2->QT_Step = 0x03; //主界面
							
							msg.type = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选着界面，才有此按钮功能）
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa_1->QT_Step = 0x02; //主界面
						}
					  break;
					}		
					if((Globa_1->Double_Gun_To_Car_falg == 1)&&(Globa_1->Charger_param.System_Type == 4)){//表示启动双枪模式：
						memcpy(&busy_frame_2, &busy_frame, sizeof(busy_frame));
						busy_frame_2.ConnectorId[0] = '0';//充电枪号
						busy_frame_2.ConnectorId[1] = '2';
						
						Globa_2->meter_start_KWH = Globa_2->meter_KWH;//取初始电表示数
						Globa_2->meter_stop_KWH = Globa_2->meter_start_KWH;
					

						sprintf(&temp_char[0], "%08d", Globa_2->meter_start_KWH);    
						memcpy(&busy_frame_2.start_kwh[0], &temp_char[0], sizeof(busy_frame.start_kwh));
						
						sprintf(&temp_char[0], "%08d", Globa_2->meter_stop_KWH);				
						memcpy(&busy_frame_2.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

						id_NO_2 = 0;
						pthread_mutex_lock(&busy_db_pmutex);
						Insertflag = Insert_Charger_Busy_DB_2(&busy_frame_2, &id_NO_2, 0x55);//写新数据库记录 无流水号 传输标志0x55不能上传
						pthread_mutex_unlock(&busy_db_pmutex);
						if(DEBUGLOG) RUN_GUN1_LogOut("%s %d","双枪启动模式-2号枪:插入交易记录中的=",id);  
					}
				
				

					if((Globa_1->Charger_param.System_Type == 1)&&(Globa_2->charger_start == 1)){//轮流充电的时候出现一把枪正在充电则另一把需要等待
						memset(&Page_daojishi.reserve1, 0x00, sizeof(PAGE_DAOJISHI_FRAME));
						memcpy(&Page_daojishi.card_sn[0], &Globa_1->card_sn[0],sizeof(Globa_1->card_sn));
						Page_daojishi.type = 1;
						memcpy(&msg.argv[0], &Page_daojishi.reserve1, sizeof(PAGE_DAOJISHI_FRAME));
						
						if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
							msg.type = 0x1151;//倒计时界面//充电界面---双枪的时候
						}else{
							Page_daojishi.type = 0;
							msg.type = 0x1161;//倒计时界面//充电界面--单枪
						}
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_DAOJISHI_FRAME), IPC_NOWAIT); 
						QT_timer_start(500);
						Globa_1->QT_Step = 0x201; // 1 自动充电-》倒计时界面  2.0.1
						Globa_1->charger_state = 0x06;
						Globa_1->checkout = 0;
						Globa_1->pay_step = 0;//0：空闲 1：刷卡启动 2：刷卡停止预约 3：刷卡停止充电 4：刷卡结算
						count_wait = 30;
						card_falg_count = 0;
						break;
					}else{
						 Globa_1->VIN_Judgment_Result = 1;
						if((Globa_1->Double_Gun_To_Car_falg == 1)&&(Globa_1->Charger_param.System_Type == 4)){
							msg.type = 0x1133;//充电界面--双枪对一辆车
							Globa_1->QT_Step = 0x24;//双枪对一辆车
							Globa_2->charger_state = 0x04;
						  memset(&Page_2020.reserve1, 0x00, sizeof(PAGE_2020_FRAME));
						 #ifdef CHECK_BUTTON			
							Page_2020.APP = 3;   //充电启动方式 1:刷卡  2：手机APP	
						#else 
							Page_2020.APP = 1;
						#endif	
	            Page_2020.BMS_12V_24V = Globa_1->BMS_Power_V;
							memcpy(&Page_2020.SN[0], &Globa_1->card_sn[0], sizeof(Page_2020.SN));
							memcpy(&msg.argv[0], &Page_2020.reserve1, sizeof(PAGE_2020_FRAME));
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_2020_FRAME), IPC_NOWAIT);
						}else{
							if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4)){//双枪的时候
								msg.type = 0x1111;//充电界面---双枪的时候
							}else{
								msg.type = 0x1122;//充电界面--单枪
							}
							Globa_1->QT_Step = 0x21;   // 2 自动充电-》 充电界面 状态 2.1-
					    memset(&Page_1111.reserve1, 0x00, sizeof(PAGE_1111_FRAME));
							Page_1111.APP  = 3;   //充电启动方式 1:刷卡  2：手机APP
							memcpy(&Page_1111.SN[0], &Globa_1->card_sn[0], sizeof(Page_1111.SN));
							memcpy(&msg.argv[0], &Page_1111.reserve1, sizeof(PAGE_1111_FRAME));
					  	msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_1111_FRAME), IPC_NOWAIT);
					  }
						QT_timer_start(500);
					
						if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
							Voicechip_operation_function(0x05);  //05H     “系统自检中，请稍候”
						}
						//Globa_1->charger_state = 0x04;
						Globa_1->checkout = 0;
						Globa_1->pay_step = 0;//0：空闲 1：刷卡启动 2：刷卡停止预约 3：刷卡停止充电 4：刷卡结算
						count_wait = 0;
						count_wait1 = 0;
						card_falg_count = 0;
						meter_Curr_Threshold_count = Current_Systime;
						current_low_timestamp = Current_Systime;
						check_current_low_st = 0;
						Electrical_abnormality = Current_Systime;
						current_overkwh_timestamp = Current_Systime;
						break;
					}
					break;
				}
				
        
				if(Globa_1->charger_start == 0){//充电彻底结束处理
				  printf("-TTTTST------------\n");
				  sleep(2);
					Globa_1->meter_stop_KWH = (Globa_1->meter_KWH >= Globa_1->meter_stop_KWH)? Globa_1->meter_KWH:Globa_1->meter_stop_KWH;//取初始电表示数
					Globa_1->kwh = (Globa_1->meter_stop_KWH - Globa_1->meter_start_KWH);
					
					g_ulcharged_kwh[pre_price_index] = Globa_1->kwh - ul_start_kwh;//累加当前时段的耗电量	
					cur_price_index = GetCurPriceIndex(); //获取当前时间
					if(cur_price_index != pre_price_index){
						ul_start_kwh = Globa_1->kwh - g_ulcharged_kwh[cur_price_index];//load new start kwh,需要减去时段之前累加的电量，否则该时段之前累加的电量丢失----20160312
						pre_price_index = cur_price_index;							
					}	
					for(i = 0;i<4;i++){
						g_ulcharged_rmb[i] = g_ulcharged_kwh[i]*share_time_kwh_price[i];
					}
					
					for(i = 0;i<4;i++){
						g_ulcharged_rmb[i] = (g_ulcharged_rmb[i] + COMPENSATION_VALUE)/100;//4
					}
					Globa_1->rmb = g_ulcharged_rmb[0] + g_ulcharged_rmb[1] + g_ulcharged_rmb[2] + g_ulcharged_rmb[3]; 

					if(Serv_Type == 1){ //按次数收
						Globa_1->total_rmb = (Globa_1->rmb + Globa_1->Charger_param.Serv_Price);//已充电电量对应的金额
					}else if(Serv_Type == 2){//按时间收
						Globa_1->total_rmb = (Globa_1->rmb + Globa_1->Time*Globa_1->Charger_param.Serv_Price/600);//已充电电量对应的金额
					}else{
						//Globa_1->total_rmb = (Globa_1->rmb + Globa_1->kwh*Globa_1->Charger_param.Serv_Price/100);//已充电电量对应的金额
						for(i = 0;i<4;i++){
							g_ulcharged_rmb_Serv[i] = g_ulcharged_kwh[i]*share_time_kwh_price_serve[i];
						}
						for(i = 0;i<4;i++)
						{
							g_ulcharged_rmb_Serv[i] = (g_ulcharged_rmb_Serv[i]+ COMPENSATION_VALUE)/100;
						}
						Globa_1->total_rmb = Globa_1->rmb + g_ulcharged_rmb_Serv[0] + g_ulcharged_rmb_Serv[1] + g_ulcharged_rmb_Serv[2] + g_ulcharged_rmb_Serv[3]; 
					}
				  Globa_1->ISO_8583_rmb_Serv = Globa_1->total_rmb - Globa_1->rmb ;               //总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
	        Globa_1->ISO_8583_rmb = Globa_1->rmb;                     //电量消费金额 0.01yuan
	
					//-------------------扣费 更新业务进度数据----------------------------
					sprintf(&temp_char[0], "%08d", Globa_1->meter_stop_KWH);				
					memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

					sprintf(&temp_char[0], "%08d", Globa_1->kwh);				
					memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
					
					sprintf(&temp_char[0], "%08d", Globa_1->rmb);				
					memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));
				  if(Globa_1->kwh != 0){
	          sprintf(&temp_char[0], "%08d", (Globa_1->rmb*100/Globa_1->kwh));  
	        }else{
	          sprintf(&temp_char[0], "%08d", 0);  
	        }
					memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));
					sprintf(&temp_char[0], "%08d", Globa_1->total_rmb);				
					memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));

					sprintf(&temp_char[0], "%08d", Globa_1->total_rmb-Globa_1->rmb);				
					memcpy(&busy_frame.server_rmb[0], &temp_char[0], sizeof(busy_frame.server_rmb));

					systime = time(NULL);         //获得系统的当前时间 
					localtime_r(&systime, &tm_t);  //结构指针指向当前时间	       				
					sprintf(&temp_char[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
			                             tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
					memcpy(&busy_frame.end_time[0], &temp_char[0], sizeof(busy_frame.end_time));
					tmp_value = g_ulcharged_rmb[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.sharp_allrmb[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb));
				
					tmp_value = g_ulcharged_rmb[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame.peak_allrmb[0], &temp_char[0], sizeof(busy_frame.peak_allrmb));
					
					tmp_value = g_ulcharged_rmb[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame.flat_allrmb[0], &temp_char[0], sizeof(busy_frame.flat_allrmb));

					tmp_value = g_ulcharged_rmb[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.valley_allrmb[0], &temp_char[0], sizeof(busy_frame.valley_allrmb));
					
					tmp_value = g_ulcharged_rmb_Serv[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.sharp_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb_Serv));

					tmp_value = g_ulcharged_rmb_Serv[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame.peak_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.peak_allrmb_Serv));
	
					tmp_value = g_ulcharged_rmb_Serv[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame.flat_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.flat_allrmb_Serv));
				
					tmp_value = g_ulcharged_rmb_Serv[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.valley_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.valley_allrmb_Serv));

					if(Globa_1->Charger_Over_Rmb_flag == 1){
						Page_1300.cause = 13; 
						sent_warning_message(0x94, Page_1300.cause, 1, 0);
					}else if(Globa_1->Charger_Over_Rmb_flag == 3){
						Page_1300.cause = 19; 
						sent_warning_message(0x94, Page_1300.cause, 1, 0);
					}else if(Globa_1->Charger_Over_Rmb_flag == 4){
						Page_1300.cause = 62; 
						sent_warning_message(0x94, Page_1300.cause, 1, 0);
					}else if(Globa_1->Charger_Over_Rmb_flag == 5){
						Page_1300.cause = 65; 
						sent_warning_message(0x94, Page_1300.cause, 1, 0);
					}else if(Globa_1->Charger_Over_Rmb_flag == 6){
						Page_1300.cause = 9; 
						sent_warning_message(0x94, Page_1300.cause, 1, 0);
					}else if(Globa_1->Charger_Over_Rmb_flag == 7){//读取到的电表电量异常
						Page_1300.cause = 8;
						sent_warning_message(0x94, Page_1300.cause, 1, 0);							
					}else if(Globa_1->Charger_Over_Rmb_flag == 8){//充电电流超过SOC能允许充电的值
						Page_1300.cause = 7;
						sent_warning_message(0x94, Page_1300.cause, 1, 0);							
					}else{
					  Page_1300.cause = Globa_1->charger_over_flag; 
					}
					sprintf(&temp_char[0], "%02d", Page_1300.cause);		
					memcpy(&busy_frame.End_code[0], &temp_char[0], sizeof(busy_frame.End_code));//结束原因代码
					
					memcpy(&busy_frame.car_sn[0],Globa_1->VIN,sizeof(Globa_1->VIN));//

					tmp_value =0;
					pthread_mutex_lock(&busy_db_pmutex);
					Update_Charger_Busy_DB_1(id, &busy_frame, 1, 0x66);
					pthread_mutex_unlock(&busy_db_pmutex);

          Globa_1->Charger_Over_Rmb_flag = 0;
					Page_1300.Capact = Globa_1->BMS_batt_SOC;
					Page_1300.elapsed_time[0] =  Globa_1->Time;
					Page_1300.elapsed_time[1] =  Globa_1->Time>>8;
					Page_1300.KWH[0] =  Globa_1->kwh;
					Page_1300.KWH[1] =  Globa_1->kwh>>8;
					Page_1300.RMB[0] =  Globa_1->total_rmb;
					Page_1300.RMB[1] =  Globa_1->total_rmb>>8;
					Page_1300.RMB[2] =  Globa_1->total_rmb>>16;
					Page_1300.RMB[3] =  Globa_1->total_rmb>>24;
					
					memcpy(&Page_1300.SN[0], &Globa_1->card_sn[0], sizeof(Page_1300.SN));
				//	Page_1300.APP = 3;   //充电启动方式 1:刷卡  2：手机APP 3.VIN
          #ifdef CHECK_BUTTON			
					  Page_1300.APP = 3;   //充电启动方式 1:刷卡  2：手机APP	
					#else 
						Page_1300.APP = 1;
					#endif	
					memcpy(&msg.argv[0], &Page_1300.reserve1, sizeof(PAGE_1300_FRAME));
				
  				if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
					  msg.type = 0x1300;//充电结束界面
				  }else{
						msg.type = 0x1310;//单枪结束界面
					}
					
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_1300_FRAME), IPC_NOWAIT);

					Globa_1->QT_Step = 0x23; // 2      充电结束 结算界面 2.3----------------
				  printf("-TTTTST----3--------\n");

					Globa_1->charger_state = 0x05;
					Globa_1->checkout = 0;
					if(Globa_1->Start_Mode == VIN_START_TYPE){
						Globa_1->pay_step = 0;
					}
					else
					{
					  Globa_1->pay_step = 4;//1通道在等待刷卡扣费
						if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
							Voicechip_operation_function(0x07);//07H     “充电已完成，请刷卡结算”
						}else{
							Voicechip_operation_function(0x10);//10H	请刷卡结算.wav
						}
					}
          Globa_1->Charger_Over_Rmb_flag = 0;
					count_wait = 0;
					break;
				}
				break;
			}
			case 0x20:{//---------------2 自动充电-》 等待刷卡界面   2.0--------------
		    Globa_1->kwh = 0;
				ul_start_kwh = 0;
				ul_start_kwh_2 = 0;
			  cur_price_index = GetCurPriceIndex(); //获取当前时间
				pre_price_index = cur_price_index;
      	for(i=0;i<4;i++){
					g_ulcharged_rmb[i] = 0;
					g_ulcharged_kwh[i] = 0;
					g_ulcharged_rmb_Serv[i] = 0;
					share_time_kwh_price[i] = 0;
	        share_time_kwh_price_serve[i] = 0; //服务
					g_ulcharged_rmb_2[i] = 0;
					g_ulcharged_kwh_2[i] = 0;
					g_ulcharged_rmb_Serv_2[i] = 0;
				}
				Globa_1->rmb = 0;
				Globa_1->total_rmb = 0;
				Globa_1->ISO_8583_rmb_Serv = 0;               //总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
	      Globa_1->ISO_8583_rmb= 0;                     //电量消费金额 0.01yuan
	
				temp_kwh = 0;
				Globa_1->soft_KWH = 0;
				Globa_1->Time = 0;
				Globa_1->card_type = 0;//卡类型 0:未知 1：用户卡 2：员工卡 3：维护卡

				count_wait++;
				if(count_wait > 150){//等待时间超时30S
    			count_wait = 0;
					if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
						msg.type = 0x150;
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);						
						QT_timer_stop(500);
						Globa_1->QT_Step = 0x03; //自动充电分开的AB通道
					}else{//单枪
						//防止按键处理不同步，此处同步一下
						msg.type = 0x150;//
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_2->QT_Step = 0x03; //主界面
						
						msg.type = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选着界面，才有此按钮功能）
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_1->QT_Step = 0x02; //主界面
					}
  				break;//超时必须退出
  			}

				/*if(Globa_1->checkout == 0){//卡片处理失败，请重新操作
     			//继续寻卡
    			break;
  			}else*/
				if(Globa_1->checkout == 4){//无法识别充电卡
					Globa_1->checkout = 0;//清除刷卡结果标志
					count_wait = 0;
          //Voicechip_operation_function(0x0d);//0dH     “卡片未注册，请联系发卡单位处理”
     			//等待30S超时
     			msg.type = 0x200;//等待刷卡界面
     			Page_200.state = 2;
					memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
    			msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);

    			break;
  			}else if((Globa_1->checkout == 5)||(Globa_1->checkout == 6)||(Globa_1->checkout == 7)){//卡片状态异常
     			msg.type = 0x200;//等待刷卡界面
     			if(Globa_1->checkout == 5){//卡片状态异常
     				Page_200.state = 4;
						if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
						  Voicechip_operation_function(0x0b);//0bH     “卡片已灰锁，请解锁”
						}
     			}else if(Globa_1->checkout == 6){//卡片已使用
     				Page_200.state = 9;
     			}else if(Globa_1->checkout == 7){//非充电卡
     				Page_200.state = 8;
     			}

					memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
    			msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
    		  sleep(3);
					if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
						msg.type = 0x150;
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);						
						QT_timer_stop(500);
						Globa_1->QT_Step = 0x03; //自动充电分开的AB通道
					}else{//单枪
						//防止按键处理不同步，此处同步一下
						msg.type = 0x150;//
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_2->QT_Step = 0x03; //主界面
						
						msg.type = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选着界面，才有此按钮功能）
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_1->QT_Step = 0x02; //主界面
					}
					break;
      	}else if((Globa_1->checkout == 1)||(Globa_1->checkout == 3)||(Globa_1->checkout == 12)){//卡片预处理成功 员工卡
					if(Globa_1->have_hart == 1){
						if((Globa_1->User_Card_check_result == 0)&&(Globa_1->User_Card_check_flag == 0)){
							msg.type = 0x200;//等待刷卡界面
							Page_200.state = 12;//卡片验证中...请保持刷卡状态！
							memcpy(&Page_200.sn[0], &busy_frame.card_sn[0], sizeof(Page_200.sn));
							memcpy(&Page_200.rmb[0], &busy_frame.last_rmb[0], sizeof(Page_200.rmb));
							memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
							memcpy(&Globa_1->User_Card_check_card_ID[0], &Globa_1->card_sn[0], sizeof(Globa_1->User_Card_check_card_ID));
							Globa_1->User_Card_check_flag = 1;
						  break;
						}
					}else{//离线桩
					  if((Globa_1->qiyehao_flag == 1)&&(Globa_1->Special_card_offline_flag == 0)){//企业标号---离线无法启动
							msg.type = 0x200;//等待刷卡界面
							Page_200.state = 35;//企业标号---离线无法启动
							Page_200.reserve1 = 0;				
							memcpy(&Page_200.sn[0], &Globa_1->card_sn[0], sizeof(Page_200.sn));
							sprintf(&temp_char[0], "%08d", Globa_1->last_rmb);	
							memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
							memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
			
							sleep(2);
							if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
								msg.type = 0x150;
								msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);						
								QT_timer_stop(500);
								Globa_1->QT_Step = 0x03; //自动充电分开的AB通道
							}else{//单枪
								//防止按键处理不同步，此处同步一下
								msg.type = 0x150;//
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
								Globa_2->QT_Step = 0x03; //主界面
								msg.type = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选着界面，才有此按钮功能）
								msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
								Globa_1->QT_Step = 0x02; //主界面
							}
							break;//退出，不能充电
						}else if((Globa_1->order_chg_flag == 1)&&(Globa_1->Special_card_offline_flag == 0)){////有序充电用户卡标记---离线无法启动
							msg.type = 0x200;//等待刷卡界面
							Page_200.state = 36;//企业标号---有序充电用户卡标记---离线无法启动
							Page_200.reserve1 = 0;				
							memcpy(&Page_200.sn[0], &Globa_1->card_sn[0], sizeof(Page_200.sn));
							sprintf(&temp_char[0], "%08d", Globa_1->last_rmb);	
							memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
							memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
				
							sleep(2);
						  if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
								msg.type = 0x150;
								msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);						
								QT_timer_stop(500);
								Globa_1->QT_Step = 0x03; //自动充电分开的AB通道
							}else{//单枪
								//防止按键处理不同步，此处同步一下
								msg.type = 0x150;//
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
								Globa_2->QT_Step = 0x03; //主界面
								
								msg.type = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选着界面，才有此按钮功能）
								msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
								Globa_1->QT_Step = 0x02; //主界面
							}
							break;//退出，不能充电
						}
						Globa_1->User_Card_check_flag = 0;
						Globa_1->User_Card_check_result = 1;
					}
				}else if(Globa_1->checkout == 8){//余额不足
					msg.type = 0x200;//等待刷卡界面
					Page_200.state = 11;//卡里余额过低，停止充电
					Page_200.reserve1 = 0;				
					memcpy(&Page_200.sn[0], &Globa_1->card_sn[0], sizeof(Page_200.sn));
					sprintf(&temp_char[0], "%08d", Globa_1->last_rmb);	
					memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
					memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
					if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
					  Voicechip_operation_function(0x0a);//0aH     “余额不足，请充值”
					}
				  sleep(2);
					if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
						msg.type = 0x150;
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);						
						QT_timer_stop(500);
						Globa_1->QT_Step = 0x03; //自动充电分开的AB通道
					}else{//单枪
				
						//防止按键处理不同步，此处同步一下
						msg.type = 0x150;//
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_2->QT_Step = 0x03; //主界面
						
						msg.type = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选着界面，才有此按钮功能）
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_1->QT_Step = 0x02; //主界面
					}

					break;//退出，不能充电
				}else if(Globa_1->checkout == 9){
					msg.type = 0x200;//等待刷卡界面
					Page_200.state = 7;
					memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
				  if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
					  Voicechip_operation_function(0x0c);//0cH     “卡片已挂失，请联系发卡单位处理”
					}
					sleep(3);
					if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
						msg.type = 0x150;
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);						
						QT_timer_stop(500);
						Globa_1->QT_Step = 0x03; //自动充电分开的AB通道
					}else{//单枪
						//防止按键处理不同步，此处同步一下
						msg.type = 0x150;//
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_2->QT_Step = 0x03; //主界面
						
						msg.type = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选着界面，才有此按钮功能）
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_1->QT_Step = 0x02; //主界面
					}

					break;//退出，不能充电
			  }else{
  				Globa_1->checkout = 0;//清除刷卡结果标志
  			}
				
			  if((Globa_1->User_Card_check_result >= 2)&&(Globa_1->User_Card_check_result <= 14)){
					Globa_1->User_Card_check_flag = 0;
					msg.type = 0x200;//等待刷卡界面
					Page_200.state = 17 + (Globa_1->User_Card_check_result - 2 );//14;//MAC校验错误
					memcpy(&Page_200.sn[0], &Globa_1->card_sn[0], sizeof(Page_200.sn));
					sprintf(&temp_char[0], "%08d", Globa_1->last_rmb);	
					memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
					memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
					if(Globa_1->User_Card_check_result == 4){
						if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
						  Voicechip_operation_function(0x0d);//0dH     “卡片未注册，请联系发卡单位处理”
					  }
					}
					sleep(2);
					if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
						msg.type = 0x150;
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);						
						QT_timer_stop(500);
						Globa_1->QT_Step = 0x03; //自动充电分开的AB通道
					}else{//单枪
						//防止按键处理不同步，此处同步一下
						msg.type = 0x150;//
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_2->QT_Step = 0x03; //主界面
						
						msg.type = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选着界面，才有此按钮功能）
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_1->QT_Step = 0x02; //主界面
					}
					break;//退出，不能充电
				}
				else if(Globa_1->User_Card_check_result == 1){//验证成功请在次刷卡
					Globa_1->User_Card_check_flag = 0;
					msg.type = 0x200;//等待刷卡界面
					Page_200.state = 15;//验证成功请保持刷卡状态
	        memcpy(&Page_200.sn[0], &Globa_1->card_sn[0], sizeof(Page_200.sn));
					sprintf(&temp_char[0], "%08d", Globa_1->last_rmb);	
					memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
					memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
					
					if((Globa_1->checkout == 3)||(Globa_1->checkout == 1)){
						Globa_1->User_Card_check_flag = 0;
						Globa_1->User_Card_check_result = 0;
						Globa_1->pay_step = 0;//进入空闲状态
						if(Globa_1->checkout == 1){//卡片状态异常 灰锁
							Globa_1->card_type = 1; //卡类型 0:未知 1：用户卡 2：员工卡 3：维护卡卡类型 1：用户卡 2：员工卡
						}else{
							Globa_1->card_type = 2; //卡类型 0:未知 1：用户卡 2：员工卡 3：维护卡卡类型 1：用户卡 2：员工卡
						}
						
					  msg.type = 0x200;//等待刷卡界面
						if(Globa_1->card_type == 1){
							Page_200.state = 5;
						}else{
							Page_200.state = 6;
						}
						
					  if(Globa_1->qiyehao_flag){//为企业卡号
							Page_200.state = 17;
							Globa_1->card_type = 2;
						}
						
				    memcpy(&Page_200.sn[0], &Globa_1->card_sn[0], sizeof(Page_200.sn));
					  sprintf(&temp_char[0], "%08d", Globa_1->last_rmb);	
					  memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
						memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
						sleep(1);
						
            if((Globa_1->private_price_acquireOK == 1)&&(Globa_1->private_price_flag == 1)){//专有电价
							for(i=0;i<4;i++){
								share_time_kwh_price[i] = Globa_1->Special_price_data.share_time_kwh_price[i];
								share_time_kwh_price_serve[i] = Globa_1->Special_price_data.share_time_kwh_price_serve[i]; //服务
							}
							Serv_Type = Globa_1->Special_price_data.Serv_Type;
						}else{//通用电价
							for(i=0;i<4;i++){
								share_time_kwh_price[i] = Globa_1->Charger_param2.share_time_kwh_price[i];
								share_time_kwh_price_serve[i] = Globa_1->Charger_param2.share_time_kwh_price_serve[i]; //服务
							}
							Serv_Type = Globa_1->Charger_param.Serv_Type;
						}
						memcpy(&busy_frame.card_sn[0], &Globa_1->card_sn[0], sizeof(busy_frame.card_sn));

						if(Globa_1->card_type == 1){
							sprintf(&temp_char[0], "%02d%02d%02d%02d%02d%02d%02d", Globa_1->User_Card_Lock_Info.card_lock_time.year_H, Globa_1->User_Card_Lock_Info.card_lock_time.year_L, Globa_1->User_Card_Lock_Info.card_lock_time.month, \
																					 Globa_1->User_Card_Lock_Info.card_lock_time.day_of_month , Globa_1->User_Card_Lock_Info.card_lock_time.hour, Globa_1->User_Card_Lock_Info.card_lock_time.minute,Globa_1->User_Card_Lock_Info.card_lock_time.second);
						}else{
							systime = time(NULL);         //获得系统的当前时间 
							localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
							sprintf(&temp_char[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
																					 tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
						}
						memcpy(&busy_frame.start_time[0], &temp_char[0], sizeof(busy_frame.start_time));   
						
						memcpy(&busy_frame.end_time[0], &temp_char[0], sizeof(busy_frame.end_time));

						if(Globa_1->card_type == 1){
							memcpy(&busy_frame.flag[0], "01", sizeof(busy_frame.flag));//用户卡标志灰锁
						}else{
							memcpy(&busy_frame.flag[0], "00", sizeof(busy_frame.flag));//员工卡直接标志位完成，不灰锁
						}

						sprintf(&temp_char[0], "%08d", Globa_1->last_rmb);		
						memcpy(&busy_frame.last_rmb[0], &temp_char[0], sizeof(busy_frame.last_rmb));

						memcpy(&busy_frame.ter_sn[0],  &Globa_1->Charger_param.SN[0], sizeof(busy_frame.ter_sn));

				
						Globa_1->meter_start_KWH = Globa_1->meter_KWH;//取初始电表示数
						Globa_1->meter_stop_KWH = Globa_1->meter_start_KWH;
					

						sprintf(&temp_char[0], "%08d", Globa_1->meter_start_KWH);    
						memcpy(&busy_frame.start_kwh[0], &temp_char[0], sizeof(busy_frame.start_kwh));
						
						sprintf(&temp_char[0], "%08d", Globa_1->meter_stop_KWH);				
						memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

						sprintf(&temp_char[0], "%08d", Globa_1->Charger_param.Price);    
						memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));

						sprintf(&temp_char[0], "%08d", Globa_1->kwh);				
						memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
						
						sprintf(&temp_char[0], "%08d", Globa_1->rmb);				
						memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));

						//起始总费用等于电量费用，暂时只做安电量收服务费
						sprintf(&temp_char[0], "%08d", Globa_1->total_rmb);
						memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));
						sprintf(&temp_char[0], "%08d", Globa_1->total_rmb-Globa_1->rmb);
						memcpy(&busy_frame.server_rmb[0], &temp_char[0], sizeof(busy_frame.server_rmb));
						busy_frame.type[0] = '0';//充电方式（01：刷卡，02：手机）
						busy_frame.type[1] = '1';
						sprintf(&temp_char[0], "%02d", Serv_Type);//服务费收费类型 01：按次收费；02：按时间收费(单位：分/10分钟；03：按电量	9	N	N2
						memcpy(&busy_frame.Serv_Type[0], &temp_char[0], sizeof(busy_frame.Serv_Type));

						memset(&busy_frame.car_sn[0], '0', sizeof(busy_frame.car_sn));//
						tmp_value = share_time_kwh_price[0];
						sprintf(&temp_char[0], "%08d", tmp_value);				
						memcpy(&busy_frame.sharp_rmb_kwh[0], &temp_char[0], sizeof(busy_frame.sharp_rmb_kwh));

						tmp_value = g_ulcharged_rmb[0];
						sprintf(&temp_char[0], "%08d", tmp_value);	
						memcpy(&busy_frame.sharp_allrmb[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb));
					
						tmp_value = share_time_kwh_price[1];
						sprintf(&temp_char[0], "%08d", tmp_value);	
						memcpy(&busy_frame.peak_rmb_kwh[0], &temp_char[0], sizeof(busy_frame.peak_rmb_kwh));
						tmp_value = g_ulcharged_rmb[1];
						sprintf(&temp_char[0], "%08d", tmp_value);
						memcpy(&busy_frame.peak_allrmb[0], &temp_char[0], sizeof(busy_frame.peak_allrmb));
						
						tmp_value = share_time_kwh_price[2];
						sprintf(&temp_char[0], "%08d", tmp_value);	
						memcpy(&busy_frame.flat_rmb_kwh[0], &temp_char[0], sizeof(busy_frame.flat_rmb_kwh));
						tmp_value = g_ulcharged_rmb[2];
						sprintf(&temp_char[0], "%08d", tmp_value);		
						memcpy(&busy_frame.flat_allrmb[0], &temp_char[0], sizeof(busy_frame.flat_allrmb));
					
						tmp_value = share_time_kwh_price[3];
						sprintf(&temp_char[0], "%08d", tmp_value);				
						memcpy(&busy_frame.valley_rmb_kwh[0], &temp_char[0], sizeof(busy_frame.valley_rmb_kwh));
						tmp_value = g_ulcharged_rmb[3];
						sprintf(&temp_char[0], "%08d", tmp_value);	
						memcpy(&busy_frame.valley_allrmb[0], &temp_char[0], sizeof(busy_frame.valley_allrmb));
					
  					tmp_value = share_time_kwh_price_serve[0];
						sprintf(&temp_char[0], "%08d", tmp_value);				
						memcpy(&busy_frame.sharp_rmb_kwh_Serv[0], &temp_char[0], sizeof(busy_frame.sharp_rmb_kwh_Serv));

						tmp_value = g_ulcharged_rmb_Serv[0];
						sprintf(&temp_char[0], "%08d", tmp_value);	
						memcpy(&busy_frame.sharp_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb_Serv));
					
						tmp_value = share_time_kwh_price_serve[1];
						sprintf(&temp_char[0], "%08d", tmp_value);	
						memcpy(&busy_frame.peak_rmb_kwh_Serv[0], &temp_char[0], sizeof(busy_frame.peak_rmb_kwh_Serv));
						tmp_value = g_ulcharged_rmb_Serv[1];
						sprintf(&temp_char[0], "%08d", tmp_value);
						memcpy(&busy_frame.peak_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.peak_allrmb_Serv));
						
						tmp_value = share_time_kwh_price_serve[2];
						sprintf(&temp_char[0], "%08d", tmp_value);	
						memcpy(&busy_frame.flat_rmb_kwh_Serv[0], &temp_char[0], sizeof(busy_frame.flat_rmb_kwh_Serv));
						tmp_value = g_ulcharged_rmb_Serv[2];
						sprintf(&temp_char[0], "%08d", tmp_value);		
						memcpy(&busy_frame.flat_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.flat_allrmb_Serv));
					
						tmp_value = share_time_kwh_price_serve[3];
						sprintf(&temp_char[0], "%08d", tmp_value);				
						memcpy(&busy_frame.valley_rmb_kwh_Serv[0], &temp_char[0], sizeof(busy_frame.valley_rmb_kwh_Serv));
						tmp_value = g_ulcharged_rmb_Serv[3];
						sprintf(&temp_char[0], "%08d", tmp_value);	
						memcpy(&busy_frame.valley_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.valley_allrmb_Serv));
		
						busy_frame.ConnectorId[0] = '0';//充电枪号
						busy_frame.ConnectorId[1] = '1';
						
						busy_frame.End_code[0] = '1';//结束代码
						busy_frame.End_code[1] = '1';
						
						tmp_value =0;
						id = 0;
						pthread_mutex_lock(&busy_db_pmutex);
						Insertflag = Insert_Charger_Busy_DB_1(&busy_frame, &id, 0x55);//写新数据库记录 无流水号 传输标志0x55不能上传
						pthread_mutex_unlock(&busy_db_pmutex);
				    if(DEBUGLOG) RUN_GUN1_LogOut("%s %d","一号枪:刷卡启动充电并插入交易记录中的ID=",id);  
            if((Insertflag  == -1)&&(id == 0)){//数据有异常
							 Page_1300.cause = 63;
						   Page_1300.Capact = Globa_1->BMS_batt_SOC;
							 Page_1300.elapsed_time[0] =  Globa_1->Time;
							 Page_1300.elapsed_time[1] =  Globa_1->Time>>8;
							 Page_1300.KWH[0] =  Globa_1->kwh;
							 Page_1300.KWH[1] =  Globa_1->kwh>>8;
							 Page_1300.RMB[0] =  Globa_1->total_rmb;
							 Page_1300.RMB[1] =  Globa_1->total_rmb>>8;
							 Page_1300.RMB[2] =  Globa_1->total_rmb>>16;
					     Page_1300.RMB[3] =  Globa_1->total_rmb>>24;
					
							 memcpy(&Page_1300.SN[0], &Globa_1->card_sn[0], sizeof(Page_1300.SN));
							 Page_1300.APP = 1;   //充电启动方式 1:刷卡  2：手机APP

							 memcpy(&msg.argv[0], &Page_1300.reserve1, sizeof(PAGE_1300_FRAME));
						
							 if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
							  	msg.type = 0x1300;//充电结束界面
							 }else{
							  	msg.type = 0x1310;//单枪结束界面
							 }
							
							 msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_1300_FRAME), IPC_NOWAIT);

							 Globa_1->QT_Step = 0x23; // 2      充电结束 结算界面 2.3----------------
		           Globa_1->Manu_start = 0;
						   Globa_1->charger_start =0;
							 Globa_1->charger_state = 0x05;
							 Globa_1->checkout = 0;
							 Globa_1->pay_step = 4;//1通道在等待刷卡扣费
							 card_falg_count = 0;
							 Globa_1->Charger_Over_Rmb_flag = 0;
							 if(DEBUGLOG) RUN_GUN1_LogOut("%s %d","一号枪:数据库异常请处理=",id);  
							 break;
					   }
						//------------------------------------------------------------------
						if((Globa_1->private_price_acquireOK == 0)&&(Globa_1->private_price_flag == 1)){//提示用户无法享受优惠电价，
							msg.type = 0x200;//等待刷卡界面
							Page_200.state = 37;
							memcpy(&Page_200.sn[0], &Globa_1->card_sn[0], sizeof(Page_200.sn));
							sprintf(&temp_char[0], "%08d", Globa_1->last_rmb);	
							memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
							memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
							sleep(2);//时间可调整
					  }
						
					  if((Globa_1->Double_Gun_To_Car_falg == 1)&&(Globa_1->Charger_param.System_Type == 4)){//表示启动双枪模式：
							memcpy(&busy_frame_2, &busy_frame, sizeof(busy_frame));
	            busy_frame_2.ConnectorId[0] = '0';//充电枪号
						  busy_frame_2.ConnectorId[1] = '2';
							
							Globa_2->meter_start_KWH = Globa_2->meter_KWH;//取初始电表示数
							Globa_2->meter_stop_KWH = Globa_2->meter_start_KWH;
						

							sprintf(&temp_char[0], "%08d", Globa_2->meter_start_KWH);    
							memcpy(&busy_frame_2.start_kwh[0], &temp_char[0], sizeof(busy_frame.start_kwh));
							
							sprintf(&temp_char[0], "%08d", Globa_2->meter_stop_KWH);				
							memcpy(&busy_frame_2.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

							id_NO_2 = 0;
							pthread_mutex_lock(&busy_db_pmutex);
							Insertflag = Insert_Charger_Busy_DB_2(&busy_frame_2, &id_NO_2, 0x55);//写新数据库记录 无流水号 传输标志0x55不能上传
							pthread_mutex_unlock(&busy_db_pmutex);
							if(DEBUGLOG) RUN_GUN1_LogOut("%s %d","双枪启动模式-2号枪:插入交易记录中的=",id);  
						}
					
						if(Globa_1->Card_charger_type == 5){//倒计时界面
							memset(&Page_daojishi.reserve1, 0, sizeof(PAGE_DAOJISHI_FRAME));
							memcpy(&Page_daojishi.card_sn[0], &Globa_1->card_sn[0],sizeof(Globa_1->card_sn));
							Page_daojishi.type = 0;
							time_t systime1;
							systime1 = time(NULL);        //获得系统的当前时间 
							int daojishi_time = Globa_1->QT_pre_time - systime1;
							daojishi_time  = (daojishi_time > 0)? daojishi_time:0;
							Page_daojishi.hour = daojishi_time/3600;		
							Page_daojishi.min  = (daojishi_time%3600)/60;
							Page_daojishi.second =(daojishi_time%3600)%60;
							if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
								msg.type = 0x1151;//倒计时界面//充电界面---双枪的时候
							}else{
							  msg.type = 0x1161;//倒计时界面//充电界面--单枪
							}
							memcpy(&msg.argv[0], &Page_daojishi.reserve1, sizeof(PAGE_DAOJISHI_FRAME));
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_DAOJISHI_FRAME), IPC_NOWAIT); 

							QT_timer_start(500);
							Globa_1->QT_Step = 0x201; // 1 自动充电-》倒计时界面  2.0.1
							Globa_1->charger_state = 0x06;
							Globa_1->checkout = 0;
							//Globa_1->pay_step = 2;//0：空闲 1：刷卡启动 2：刷卡停止预约 3：刷卡停止充电 4：刷卡结算
							card_falg_count = 0;
							count_wait = 0;
						}else{
							if((Globa_1->Charger_param.System_Type == 1)&&(Globa_2->charger_start == 1)){//轮流充电的时候出现一把枪正在充电则另一把需要等待
								memset(&Page_daojishi.reserve1, 0x00, sizeof(PAGE_DAOJISHI_FRAME));
								memcpy(&Page_daojishi.card_sn[0], &Globa_1->card_sn[0],sizeof(Globa_1->card_sn));
								Page_daojishi.type = 1;
								memcpy(&msg.argv[0], &Page_daojishi.reserve1, sizeof(PAGE_DAOJISHI_FRAME));
								
								if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
									msg.type = 0x1151;//倒计时界面//充电界面---双枪的时候
								}else{
								  Page_daojishi.type = 0;
									msg.type = 0x1161;//倒计时界面//充电界面--单枪
								}
								msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_DAOJISHI_FRAME), IPC_NOWAIT); 
								QT_timer_start(500);
								Globa_1->QT_Step = 0x201; // 1 自动充电-》倒计时界面  2.0.1
								Globa_1->charger_state = 0x06;
								Globa_1->checkout = 0;
								//Globa_1->pay_step = 2;//0：空闲 1：刷卡启动 2：刷卡停止预约 3：刷卡停止充电 4：刷卡结算
								count_wait = 30;
								card_falg_count = 0;
							  break;
							}else{
								Globa_1->Manu_start = 1;
								Globa_1->charger_start = 1;
								if(Globa_1->qiyehao_flag){//为企业卡号
									Globa_1->Enterprise_Card_flag = 1;
								}else{
									Globa_1->Enterprise_Card_flag = 0;
								}
								Globa_1->charger_state = 0x04;
							
								if((Globa_1->Double_Gun_To_Car_falg == 1)&&(Globa_1->Charger_param.System_Type == 4)){
									msg.type = 0x1133;//充电界面--双枪对一辆车
									Globa_1->QT_Step = 0x24;//双枪对一辆车
									Globa_2->charger_state = 0x04;
								  memset(&Page_2020.reserve1, 0x00, sizeof(PAGE_2020_FRAME));
								  Page_2020.APP  = 1;   //充电启动方式 1:刷卡  2：手机APP ,3VIN
									Page_2020.BMS_12V_24V = Globa_1->BMS_Power_V;
									memcpy(&Page_2020.SN[0], &Globa_1->card_sn[0], sizeof(Page_2020.SN));
									memcpy(&msg.argv[0], &Page_2020.reserve1, sizeof(PAGE_2020_FRAME));
									msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_2020_FRAME), IPC_NOWAIT);
								}else{
									if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4)){//双枪的时候
										msg.type = 0x1111;//充电界面---双枪的时候
									}else{
										msg.type = 0x1122;//充电界面--单枪
									}
									Globa_1->QT_Step = 0x21;   // 2 自动充电-》 充电界面 状态 2.1-
								}
								memset(&msg.argv[0], 0x00, sizeof(PAGE_1111_FRAME));
								Page_1111.APP  = 1;   //充电启动方式 1:刷卡  2：手机APP
								Page_1111.BMS_12V_24V = Globa_1->BMS_Power_V;
								memcpy(&Page_1111.SN[0], &Globa_1->card_sn[0], sizeof(Page_2020.SN));
							  memcpy(&msg.argv[0], &Page_1111.reserve1, sizeof(PAGE_1111_FRAME));
								msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_1111_FRAME), IPC_NOWAIT);
								QT_timer_start(500);
								if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
									Voicechip_operation_function(0x05);  //05H     “系统自检中，请稍候”
								}
								//Globa_1->charger_state = 0x04;
								Globa_1->checkout = 0;
								//Globa_1->pay_step = 3;//0：空闲 1：刷卡启动 2：刷卡停止预约 3：刷卡停止充电 4：刷卡结算
								count_wait = 0;
								count_wait1 = 0;
								card_falg_count = 0;
								memset(&Page_BMS_data,0,sizeof(PAGE_BMS_FRAME));
							  meter_Curr_Threshold_count = Current_Systime;
								current_low_timestamp = Current_Systime;
								check_current_low_st = 0;
								Electrical_abnormality = Current_Systime;
								current_overkwh_timestamp = Current_Systime;
                break;
						  }
						  break;
					  }
					}
				}
				break;
			}
			
			case 0x201:{//--------------2 自动充电-》 倒计时界面  2.0.1---------------
				count_wait++;
				card_falg_count ++;
				if(card_falg_count > 20){
					Globa_1->pay_step = 2; //允许刷卡 --刷卡停止预约
				}
				if(Globa_1->checkout != 0){//1通道检测到充电卡 0：未知 1：灰锁 2：补充交易 3：卡片正常
      		msg.type = 0x95;//解除屏保
  				msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);

    			if(Globa_1->checkout == 1){//交易处理 灰锁
    				//不动作，继续等待刷卡
    			}else if(Globa_1->checkout == 2){//交易处理 补充交易
						memcpy(&busy_frame.flag[0], "02", sizeof(busy_frame.flag));//"00" 补充交易
            memcpy(&busy_frame.End_code[0], "11", sizeof(busy_frame.End_code));//结束原因代码

						pthread_mutex_lock(&busy_db_pmutex);
						if(Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00) == -1){
							usleep(10000);
							Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
						}
						pthread_mutex_unlock(&busy_db_pmutex);
    			}else if(Globa_1->checkout == 3){//交易处理 正常
						memcpy(&busy_frame.flag[0], "00", sizeof(busy_frame.flag));//"00" 成功
            memcpy(&busy_frame.End_code[0], "11", sizeof(busy_frame.End_code));//结束原因代码

						pthread_mutex_lock(&busy_db_pmutex);			
						if(Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00) == -1){
							usleep(10000);
							Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
						}
						pthread_mutex_unlock(&busy_db_pmutex);

						//------------结算终于结束，退出结算界面--------------------------
            if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
						  Voicechip_operation_function(0x09);//09H     “谢谢惠顾，再见”			
					  }
  					
						if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
							msg.type = 0x150;
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);						
							QT_timer_stop(500);
							Globa_1->QT_Step = 0x03; //自动充电分开的AB通道
						}else{//单枪
					
							//防止按键处理不同步，此处同步一下
							msg.type = 0x150;//
							msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa_2->QT_Step = 0x03; //主界面
							
							msg.type = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选着界面，才有此按钮功能）
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa_1->QT_Step = 0x02; //主界面
						}
    			}
    			
    			Globa_1->checkout = 0;
					break;
    		}

    		if(count_wait > 2){//等待时间超时500mS
					count_wait = 0;

    			//判断预约时间是否到
					time_t systime;
					struct tm tm_t;
					
					systime = time(NULL);        //获得系统的当前时间 
					localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
        
					if(Page_daojishi.type == 1){
						if((Globa_1->Charger_param.System_Type == 1)&&(Globa_2->charger_start ==0)&&(!((Globa_2->QT_Step == 0x21)||(Globa_2->QT_Step == 0x22)))){//轮流充电的时候出现一把枪正在充电则另一把需要等待
						  sleep(3);
							Globa_1->meter_start_KWH = Globa_1->meter_KWH;//取初始电表示数
							Globa_1->meter_stop_KWH = Globa_1->meter_start_KWH;
							sprintf(&temp_char[0], "%08d", Globa_1->meter_start_KWH);    
							memcpy(&busy_frame.start_kwh[0], &temp_char[0], sizeof(busy_frame.start_kwh));
							sprintf(&temp_char[0], "%08d", Globa_1->meter_stop_KWH);				
							memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));
							
  						charger_type = 1;
							Globa_1->Card_charger_type = 4;
							Globa_1->Manu_start = 1;
							Globa_1->charger_start = 1;
		          if(Globa_1->qiyehao_flag){//为企业卡号
						  	Globa_1->Enterprise_Card_flag = 1;
						  }else{
						  	Globa_1->Enterprise_Card_flag = 0;
					  	}
							msg.type = 0x1111;
							
							memset(&msg.argv[0], 0x00, sizeof(PAGE_1111_FRAME));
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_1111_FRAME), IPC_NOWAIT);
            
							QT_timer_start(500);
							Globa_1->QT_Step = 0x21;   // 2 自动充电-》 充电界面 状态 2.1-
              if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
  					    Voicechip_operation_function(0x05);  //05H     “系统自检中，请稍候”
					    }
							Globa_1->charger_state = 0x04;
							Globa_1->checkout = 0;
							Globa_1->pay_step = 3;
							count_wait = 0;
						  meter_Curr_Threshold_count = Current_Systime;
							current_low_timestamp = Current_Systime;
							check_current_low_st = 0;
							Electrical_abnormality = Current_Systime;
							current_overkwh_timestamp = Current_Systime;
							break;
						}
					}else{
					  //if((Page_daojishi.hour <= 0)&&(Page_daojishi.min <= 0)&&(Page_daojishi.second <= 0)){//(tm_t.tm_min + tm_t.tm_hour*60) >= Globa_1->QT_pre_time){
						if(Globa_1->QT_pre_time  <= systime){
							if((Globa_1->Charger_param.System_Type == 1)&&((Globa_2->charger_start ==1)||(Globa_2->App.APP_start ==1))){//轮流充电的时候出现一把枪正在充电则另一把需要等待
								Page_daojishi.type = 1;
								memcpy(&Page_daojishi.card_sn[0], &Globa_1->card_sn[0],sizeof(Globa_1->card_sn));
								memcpy(&msg.argv[0], &Page_daojishi.reserve1, sizeof(PAGE_DAOJISHI_FRAME));
								msg.type = 0x1151;//倒计时界面
								msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_DAOJISHI_FRAME), IPC_NOWAIT);
						  }else{
								Globa_1->meter_start_KWH = Globa_1->meter_KWH;//取初始电表示数
								Globa_1->meter_stop_KWH = Globa_1->meter_start_KWH;
								sprintf(&temp_char[0], "%08d", Globa_1->meter_start_KWH);    
								memcpy(&busy_frame.start_kwh[0], &temp_char[0], sizeof(busy_frame.start_kwh));
								sprintf(&temp_char[0], "%08d", Globa_1->meter_stop_KWH);				
								memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

								charger_type = 1;
								Globa_1->Card_charger_type = 4;
								Globa_1->Manu_start = 1;
								Globa_1->charger_start = 1;
			
								if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
									msg.type = 0x1111;//充电界面---双枪的时候
								}else{
									msg.type = 0x1122;//充电界面--单枪
								}
								memset(&msg.argv[0], 0x00, sizeof(PAGE_1111_FRAME));
								msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_1111_FRAME), IPC_NOWAIT);
								QT_timer_start(500);
								Globa_1->QT_Step = 0x21;   // 2 自动充电-》 充电界面 状态 2.1-

								Globa_1->charger_state = 0x04;
								Globa_1->checkout = 0;
								Globa_1->pay_step = 3;	
								count_wait = 0;
								meter_Curr_Threshold_count = Current_Systime;
								current_low_timestamp = Current_Systime;
								check_current_low_st = 0;
								Electrical_abnormality = Current_Systime;
								current_overkwh_timestamp = Current_Systime;
						  }
  					  break;
    			  }else{
							//定时刷新数据
							int daojishi_time = Globa_1->QT_pre_time - systime;
							daojishi_time  = (daojishi_time > 0)? daojishi_time:0;
							Page_daojishi.hour = daojishi_time/3600;		
							Page_daojishi.min  = (daojishi_time%3600)/60;
							Page_daojishi.second =(daojishi_time%3600)%60;
							Page_daojishi.type = 0;
							if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
								msg.type = 0x1151;//倒计时界面//充电界面---双枪的时候
							}else{
							  msg.type = 0x1161;//倒计时界面//充电界面--单枪
							}
							memcpy(&msg.argv[0], &Page_daojishi.reserve1, sizeof(PAGE_DAOJISHI_FRAME));
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_DAOJISHI_FRAME), IPC_NOWAIT);
							break;
						}
    	  	}
				}

		   	break;
			}
			
			case 0x21:{//---------------2 自动充电-》 自动充电界面 状态  2.1----------
	      card_falg_count ++;
				if((card_falg_count > 20)&&(Globa_1->Start_Mode != VIN_START_TYPE)){
					Globa_1->pay_step = 3; //允许刷卡 --刷卡停止充电
				}
				//Globa_1->meter_stop_KWH = Globa_1->meter_KWH;//取初始电表示数
				if(Globa_1->meter_KWH >= Globa_1->meter_stop_KWH)
				{
					Globa_1->meter_stop_KWH = Globa_1->meter_KWH;//取初始电表示数
					Electrical_abnormality = Current_Systime;
				}else{//连续3s获取到的数据都比较起始数据小，则直接停止充电
					if(abs(Current_Systime - Electrical_abnormality) > METER_ELECTRICAL_ERRO_COUNT)//超过1分钟了，需停止充电
					{
						Electrical_abnormality = Current_Systime;
						Globa_1->Manu_start = 0;
						Globa_1->Charger_Over_Rmb_flag = 7;//读取电量有异常
						RunEventLogOut_1("计费QT判断读取电量有异常：Globa_1->meter_KWH=%d Globa_1->meter_stop_KWH=%d",Globa_1->meter_KWH,Globa_1->meter_stop_KWH);
					}
				}
				Globa_1->kwh = Globa_1->meter_stop_KWH - Globa_1->meter_start_KWH;
				
				g_ulcharged_kwh[pre_price_index] = Globa_1->kwh - ul_start_kwh;//累加当前时段的耗电量	
				cur_price_index = GetCurPriceIndex(); //获取当前时间
				if(cur_price_index != pre_price_index){
					ul_start_kwh = Globa_1->kwh - g_ulcharged_kwh[cur_price_index];//load new start kwh,需要减去时段之前累加的电量，否则该时段之前累加的电量丢失----20160312
					pre_price_index = cur_price_index;							
				}	
				for(i = 0;i<4;i++){
					g_ulcharged_rmb[i] = g_ulcharged_kwh[i]*share_time_kwh_price[i];
				}
				
		   	for(i = 0;i<4;i++){
					g_ulcharged_rmb[i] = (g_ulcharged_rmb[i] + COMPENSATION_VALUE)/100;//4
				}
				Globa_1->rmb = g_ulcharged_rmb[0] + g_ulcharged_rmb[1] + g_ulcharged_rmb[2] + g_ulcharged_rmb[3]; 

				if(Serv_Type == 1){ //按次数收
					Globa_1->total_rmb = (Globa_1->rmb + Globa_1->Charger_param.Serv_Price);//已充电电量对应的金额
				}else if(Serv_Type == 2){//按时间收
					Globa_1->total_rmb = (Globa_1->rmb + Globa_1->Time*Globa_1->Charger_param.Serv_Price/600);//已充电电量对应的金额
				}else{
				 // Globa_1->total_rmb = (Globa_1->rmb + Globa_1->kwh*Globa_1->Charger_param.Serv_Price/100);//已充电电量对应的金额
					for(i = 0;i<4;i++){
						g_ulcharged_rmb_Serv[i] = g_ulcharged_kwh[i]*share_time_kwh_price_serve[i];
					}
					for(i = 0;i<4;i++)
					{
						g_ulcharged_rmb_Serv[i] = (g_ulcharged_rmb_Serv[i]+ COMPENSATION_VALUE)/100;
					}
					Globa_1->total_rmb = Globa_1->rmb + g_ulcharged_rmb_Serv[0] + g_ulcharged_rmb_Serv[1] + g_ulcharged_rmb_Serv[2] + g_ulcharged_rmb_Serv[3]; 
				}
				Globa_1->ISO_8583_rmb_Serv = Globa_1->total_rmb - Globa_1->rmb ;               //总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
	      Globa_1->ISO_8583_rmb = Globa_1->rmb;                     //电量消费金额 0.01yuan
	
				
				//充电过程中实时更新业务数据记录;
				//若意外掉电，则上电时判断最后一条数据记录的“上传标示”是否为“0x55”;
				//若是，以此时电表示数作为上次充电结束时的示数;
				if((Globa_1->kwh - temp_kwh) > 50){//每隔0.5度电左右保存一次数据;
					temp_kwh = Globa_1->kwh;

					sprintf(&temp_char[0], "%08d", Globa_1->meter_stop_KWH);				
					memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

					sprintf(&temp_char[0], "%08d", Globa_1->kwh);				
					memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
					
					sprintf(&temp_char[0], "%08d", Globa_1->rmb);				
					memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));
	        if(Globa_1->kwh != 0){
	          sprintf(&temp_char[0], "%08d", (Globa_1->rmb*100/Globa_1->kwh));  
	        }else{
	          sprintf(&temp_char[0], "%08d", 0);  
	        }
					memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));
					
					sprintf(&temp_char[0], "%08d", Globa_1->total_rmb);				
					memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));

					sprintf(&temp_char[0], "%08d", Globa_1->total_rmb-Globa_1->rmb);				
					memcpy(&busy_frame.server_rmb[0], &temp_char[0], sizeof(busy_frame.server_rmb));

					systime = time(NULL);         //获得系统的当前时间 
					localtime_r(&systime, &tm_t);  //结构指针指向当前时间	       				
					sprintf(&temp_char[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
			                             tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
					memcpy(&busy_frame.end_time[0], &temp_char[0], sizeof(busy_frame.end_time));
					tmp_value = g_ulcharged_rmb[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.sharp_allrmb[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb));

					tmp_value = g_ulcharged_rmb[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame.peak_allrmb[0], &temp_char[0], sizeof(busy_frame.peak_allrmb));
	
					tmp_value = g_ulcharged_rmb[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame.flat_allrmb[0], &temp_char[0], sizeof(busy_frame.flat_allrmb));
				
					tmp_value = g_ulcharged_rmb[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.valley_allrmb[0], &temp_char[0], sizeof(busy_frame.valley_allrmb));
					
					tmp_value = g_ulcharged_rmb_Serv[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.sharp_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb_Serv));

					tmp_value = g_ulcharged_rmb_Serv[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame.peak_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.peak_allrmb_Serv));
	
					tmp_value = g_ulcharged_rmb_Serv[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame.flat_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.flat_allrmb_Serv));
				
					tmp_value = g_ulcharged_rmb_Serv[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.valley_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.valley_allrmb_Serv));
					
					tmp_value =0;
					pthread_mutex_lock(&busy_db_pmutex);
					Update_Charger_Busy_DB_1(id, &busy_frame, 1, 0x55);
					pthread_mutex_unlock(&busy_db_pmutex);
				}


				//------------------ 刷卡停止充电 --------------------------------------
				if(((Globa_1->checkout == 3)&&(Globa_1->card_type == 2))|| \
					 ((Globa_1->checkout == 1)&&(Globa_1->card_type == 1))||(Globa_1->Electric_pile_workstart_Enable == 1)||((Globa_1->Start_Mode == VIN_START_TYPE)&&(msg.argv[0] == 1)&&((msg.type == 0x1111)||(msg.type == 0x1122)))){//用户卡灰锁 或正常员工卡
					RunEventLogOut_1("计费QT判断用户主动停止充电");

     			Globa_1->Manu_start = 0;
          sleep(3);
					Globa_1->meter_stop_KWH = (Globa_1->meter_KWH >= Globa_1->meter_stop_KWH)? Globa_1->meter_KWH:Globa_1->meter_stop_KWH;//取初始电表示数
					Globa_1->kwh = (Globa_1->meter_stop_KWH - Globa_1->meter_start_KWH);
					
					g_ulcharged_kwh[pre_price_index] = Globa_1->kwh - ul_start_kwh;//累加当前时段的耗电量	
					cur_price_index = GetCurPriceIndex(); //获取当前时间
					if(cur_price_index != pre_price_index){
						ul_start_kwh = Globa_1->kwh - g_ulcharged_kwh[cur_price_index];//load new start kwh,需要减去时段之前累加的电量，否则该时段之前累加的电量丢失----20160312
						pre_price_index = cur_price_index;							
					}	
					for(i = 0;i<4;i++){
						g_ulcharged_rmb[i] = g_ulcharged_kwh[i]*share_time_kwh_price[i];
					}

					for(i = 0;i<4;i++){
						g_ulcharged_rmb[i] = (g_ulcharged_rmb[i] + COMPENSATION_VALUE)/100;//4
					}
					Globa_1->rmb = g_ulcharged_rmb[0] + g_ulcharged_rmb[1] + g_ulcharged_rmb[2] + g_ulcharged_rmb[3]; 

					if(Serv_Type == 1){ //按次数收
						Globa_1->total_rmb = (Globa_1->rmb + Globa_1->Charger_param.Serv_Price);//已充电电量对应的金额
					}else if(Serv_Type == 2){//按时间收
						Globa_1->total_rmb = (Globa_1->rmb + Globa_1->Time*Globa_1->Charger_param.Serv_Price/600);//已充电电量对应的金额
					}else{
						for(i = 0;i<4;i++){
							g_ulcharged_rmb_Serv[i] = g_ulcharged_kwh[i]*share_time_kwh_price_serve[i];
						}
						for(i = 0;i<4;i++)
						{
							g_ulcharged_rmb_Serv[i] = (g_ulcharged_rmb_Serv[i]+ COMPENSATION_VALUE)/100;
						}
						Globa_1->total_rmb = Globa_1->rmb + g_ulcharged_rmb_Serv[0] + g_ulcharged_rmb_Serv[1] + g_ulcharged_rmb_Serv[2] + g_ulcharged_rmb_Serv[3]; 
					}
				
				  Globa_1->ISO_8583_rmb_Serv = Globa_1->total_rmb - Globa_1->rmb ;               //总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
	        Globa_1->ISO_8583_rmb = Globa_1->rmb;                     //电量消费金额 0.01yuan
	
					//-------------------扣费 更新业务进度数据----------------------------
					sprintf(&temp_char[0], "%08d", Globa_1->meter_stop_KWH);				
					memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

					sprintf(&temp_char[0], "%08d", Globa_1->kwh);				
					memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
					
					sprintf(&temp_char[0], "%08d", Globa_1->rmb);				
					memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));
	        if(Globa_1->kwh != 0){
	          sprintf(&temp_char[0], "%08d", (Globa_1->rmb*100/Globa_1->kwh));  
	        }else{
	          sprintf(&temp_char[0], "%08d", 0);  
	        }
					memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));
					
					sprintf(&temp_char[0], "%08d", Globa_1->total_rmb);				
					memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));

					sprintf(&temp_char[0], "%08d", Globa_1->total_rmb-Globa_1->rmb);				
					memcpy(&busy_frame.server_rmb[0], &temp_char[0], sizeof(busy_frame.server_rmb));

					systime = time(NULL);         //获得系统的当前时间 
					localtime_r(&systime, &tm_t);  //结构指针指向当前时间	       				
					sprintf(&temp_char[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
			                             tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
					memcpy(&busy_frame.end_time[0], &temp_char[0], sizeof(busy_frame.end_time));

					tmp_value = g_ulcharged_rmb[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.sharp_allrmb[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb));

					tmp_value = g_ulcharged_rmb[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame.peak_allrmb[0], &temp_char[0], sizeof(busy_frame.peak_allrmb));
					
					tmp_value = g_ulcharged_rmb[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame.flat_allrmb[0], &temp_char[0], sizeof(busy_frame.flat_allrmb));
				
					tmp_value = g_ulcharged_rmb[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.valley_allrmb[0], &temp_char[0], sizeof(busy_frame.valley_allrmb));
					
					tmp_value = g_ulcharged_rmb_Serv[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.sharp_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb_Serv));

					tmp_value = g_ulcharged_rmb_Serv[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame.peak_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.peak_allrmb_Serv));
	
					tmp_value = g_ulcharged_rmb_Serv[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame.flat_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.flat_allrmb_Serv));
				
					tmp_value = g_ulcharged_rmb_Serv[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.valley_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.valley_allrmb_Serv));
					
					tmp_value =0;
					Page_1300.cause = 11;//直接手动结束的 Globa_1->charger_over_flag;
					sprintf(&temp_char[0], "%02d", Page_1300.cause);		
				  memcpy(&busy_frame.End_code[0], &temp_char[0], sizeof(busy_frame.End_code));//结束原因代码
					
					pthread_mutex_lock(&busy_db_pmutex);
					Update_Charger_Busy_DB_1(id, &busy_frame, 1, 0x66);
					pthread_mutex_unlock(&busy_db_pmutex);
					
					Page_1300.Capact = Globa_1->BMS_batt_SOC;
					Page_1300.elapsed_time[0] =  Globa_1->Time;
					Page_1300.elapsed_time[1] =  Globa_1->Time>>8;
					Page_1300.KWH[0] =  Globa_1->kwh;
					Page_1300.KWH[1] =  Globa_1->kwh>>8;
					Page_1300.RMB[0] =  Globa_1->total_rmb;
					Page_1300.RMB[1] =  Globa_1->total_rmb>>8;
					Page_1300.RMB[2] =  Globa_1->total_rmb>>16;
					Page_1300.RMB[3] =  Globa_1->total_rmb>>24;
					
					memcpy(&Page_1300.SN[0], &Globa_1->card_sn[0], sizeof(Page_1300.SN));
					if(Globa_1->Start_Mode == VIN_START_TYPE){
						//Page_1300.APP = 3;   //充电启动方式 1:刷卡  2：手机APP 3.VIN
						 #ifdef CHECK_BUTTON								
              Page_1300.APP  = 3;   //充电启动方式 1:刷卡  2：手机APP
            #else 
	            Page_1300.APP  = 1;   //充电启动方式 1:刷卡  2：手机APP
            #endif	
					}else{
						Page_1300.APP = 1;   //充电启动方式 1:刷卡  2：手机APP
					}
		     
					memcpy(&msg.argv[0], &Page_1300.reserve1, sizeof(PAGE_1300_FRAME));
					
					if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
					  msg.type = 0x1300;//充电结束界面
				  }else{
						msg.type = 0x1310;//单枪结束界面
					}
					
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_1300_FRAME), IPC_NOWAIT);

					Globa_1->QT_Step = 0x23; // 2      充电结束 扣费界面 2.3----------------
					
					Globa_1->charger_state = 0x05;
					Globa_1->checkout = 0;

					if(Globa_1->Start_Mode == VIN_START_TYPE){
						Globa_1->pay_step = 0;
					}
					else
					{
					  Globa_1->pay_step = 4;//1通道在等待刷卡扣费
					}
          Globa_1->Charger_Over_Rmb_flag = 0;
					count_wait = 0;
					break;
				}else if(Globa_1->checkout != 0){//无用的检测结果清零
					Globa_1->checkout = 0;
				}

    		if(Globa_1->charger_start == 0){//充电彻底结束处理
				  sleep(2);
					Globa_1->meter_stop_KWH = (Globa_1->meter_KWH >= Globa_1->meter_stop_KWH)? Globa_1->meter_KWH:Globa_1->meter_stop_KWH;//取初始电表示数
					Globa_1->kwh = (Globa_1->meter_stop_KWH - Globa_1->meter_start_KWH);
					
					g_ulcharged_kwh[pre_price_index] = Globa_1->kwh - ul_start_kwh;//累加当前时段的耗电量	
					cur_price_index = GetCurPriceIndex(); //获取当前时间
					if(cur_price_index != pre_price_index){
						ul_start_kwh = Globa_1->kwh - g_ulcharged_kwh[cur_price_index];//load new start kwh,需要减去时段之前累加的电量，否则该时段之前累加的电量丢失----20160312
						pre_price_index = cur_price_index;							
					}	
					for(i = 0;i<4;i++){
						g_ulcharged_rmb[i] = g_ulcharged_kwh[i]*share_time_kwh_price[i];
					}
					
					for(i = 0;i<4;i++){
						g_ulcharged_rmb[i] = (g_ulcharged_rmb[i] + COMPENSATION_VALUE)/100;//4
					}
					Globa_1->rmb = g_ulcharged_rmb[0] + g_ulcharged_rmb[1] + g_ulcharged_rmb[2] + g_ulcharged_rmb[3]; 

					if(Serv_Type == 1){ //按次数收
						Globa_1->total_rmb = (Globa_1->rmb + Globa_1->Charger_param.Serv_Price);//已充电电量对应的金额
					}else if(Serv_Type == 2){//按时间收
						Globa_1->total_rmb = (Globa_1->rmb + Globa_1->Time*Globa_1->Charger_param.Serv_Price/600);//已充电电量对应的金额
					}else{
						//Globa_1->total_rmb = (Globa_1->rmb + Globa_1->kwh*Globa_1->Charger_param.Serv_Price/100);//已充电电量对应的金额
						for(i = 0;i<4;i++){
							g_ulcharged_rmb_Serv[i] = g_ulcharged_kwh[i]*share_time_kwh_price_serve[i];
						}
						for(i = 0;i<4;i++)
						{
							g_ulcharged_rmb_Serv[i] = (g_ulcharged_rmb_Serv[i]+ COMPENSATION_VALUE)/100;
						}
						Globa_1->total_rmb = Globa_1->rmb + g_ulcharged_rmb_Serv[0] + g_ulcharged_rmb_Serv[1] + g_ulcharged_rmb_Serv[2] + g_ulcharged_rmb_Serv[3]; 
					}
				  Globa_1->ISO_8583_rmb_Serv = Globa_1->total_rmb - Globa_1->rmb ;               //总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
	        Globa_1->ISO_8583_rmb = Globa_1->rmb;                     //电量消费金额 0.01yuan
	
					//-------------------扣费 更新业务进度数据----------------------------
					sprintf(&temp_char[0], "%08d", Globa_1->meter_stop_KWH);				
					memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

					sprintf(&temp_char[0], "%08d", Globa_1->kwh);				
					memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
					
					sprintf(&temp_char[0], "%08d", Globa_1->rmb);				
					memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));
				  if(Globa_1->kwh != 0){
	          sprintf(&temp_char[0], "%08d", (Globa_1->rmb*100/Globa_1->kwh));  
	        }else{
	          sprintf(&temp_char[0], "%08d", 0);  
	        }
					memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));
					sprintf(&temp_char[0], "%08d", Globa_1->total_rmb);				
					memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));

					sprintf(&temp_char[0], "%08d", Globa_1->total_rmb-Globa_1->rmb);				
					memcpy(&busy_frame.server_rmb[0], &temp_char[0], sizeof(busy_frame.server_rmb));

					systime = time(NULL);         //获得系统的当前时间 
					localtime_r(&systime, &tm_t);  //结构指针指向当前时间	       				
					sprintf(&temp_char[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
			                             tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
					memcpy(&busy_frame.end_time[0], &temp_char[0], sizeof(busy_frame.end_time));
					tmp_value = g_ulcharged_rmb[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.sharp_allrmb[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb));
				
					tmp_value = g_ulcharged_rmb[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame.peak_allrmb[0], &temp_char[0], sizeof(busy_frame.peak_allrmb));
					
					tmp_value = g_ulcharged_rmb[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame.flat_allrmb[0], &temp_char[0], sizeof(busy_frame.flat_allrmb));

					tmp_value = g_ulcharged_rmb[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.valley_allrmb[0], &temp_char[0], sizeof(busy_frame.valley_allrmb));
					
					tmp_value = g_ulcharged_rmb_Serv[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.sharp_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb_Serv));

					tmp_value = g_ulcharged_rmb_Serv[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame.peak_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.peak_allrmb_Serv));
	
					tmp_value = g_ulcharged_rmb_Serv[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame.flat_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.flat_allrmb_Serv));
				
					tmp_value = g_ulcharged_rmb_Serv[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.valley_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.valley_allrmb_Serv));

					if(Globa_1->Charger_Over_Rmb_flag == 1){//用户卡余额不足
						Page_1300.cause = 13; 
						sent_warning_message(0x94, Page_1300.cause, 1, 0);
					}else if(Globa_1->Charger_Over_Rmb_flag == 3){////有序充电时间到
						Page_1300.cause = 19; 
						sent_warning_message(0x94, Page_1300.cause, 1, 0);
					}else if(Globa_1->Charger_Over_Rmb_flag == 4){//与绑定的VIN不一样
						Page_1300.cause = 62;
						sent_warning_message(0x94, Page_1300.cause, 1, 0);						
					}else if(Globa_1->Charger_Over_Rmb_flag == 5){//电流计量有误差--电表和控制板的电流相差比较大
						Page_1300.cause = 65; 
						sent_warning_message(0x94, Page_1300.cause, 1, 0);
					}else if(Globa_1->Charger_Over_Rmb_flag == 6){//达到SOC限制条件
						Page_1300.cause = 9; 
						sent_warning_message(0x94, Page_1300.cause, 1, 0);
					}else if(Globa_1->Charger_Over_Rmb_flag == 7){//读取到的电表电量异常
						Page_1300.cause = 8;
						sent_warning_message(0x94, Page_1300.cause, 1, 0);							
					}else if(Globa_1->Charger_Over_Rmb_flag == 8){//充电电流超过SOC能允许充电的值
						Page_1300.cause = 7;
						sent_warning_message(0x94, Page_1300.cause, 1, 0);							
					}else{
					  Page_1300.cause = Globa_1->charger_over_flag; 
					}
					RunEventLogOut_1("计费QT判断充电结束:Page_1300.cause =%d,Globa_1->charger_over_flag =%d ,Charger_Over_Rmb_flag =%d ",Page_1300.cause,Globa_1->charger_over_flag,Globa_1->Charger_Over_Rmb_flag);

					sprintf(&temp_char[0], "%02d", Page_1300.cause);		
					memcpy(&busy_frame.End_code[0], &temp_char[0], sizeof(busy_frame.End_code));//结束原因代码
					
					tmp_value =0;
					pthread_mutex_lock(&busy_db_pmutex);
					Update_Charger_Busy_DB_1(id, &busy_frame, 1, 0x66);
					pthread_mutex_unlock(&busy_db_pmutex);
					
          Globa_1->Charger_Over_Rmb_flag = 0;
					Page_1300.Capact = Globa_1->BMS_batt_SOC;
					Page_1300.elapsed_time[0] =  Globa_1->Time;
					Page_1300.elapsed_time[1] =  Globa_1->Time>>8;
					Page_1300.KWH[0] =  Globa_1->kwh;
					Page_1300.KWH[1] =  Globa_1->kwh>>8;
					Page_1300.RMB[0] =  Globa_1->total_rmb;
					Page_1300.RMB[1] =  Globa_1->total_rmb>>8;
					Page_1300.RMB[2] =  Globa_1->total_rmb>>16;
					Page_1300.RMB[3] =  Globa_1->total_rmb>>24;
					
					memcpy(&Page_1300.SN[0], &Globa_1->card_sn[0], sizeof(Page_1300.SN));
		      if(Globa_1->Start_Mode == VIN_START_TYPE){
						//Page_1300.APP = 3;   //充电启动方式 1:刷卡  2：手机APP 3.VIN
					  #ifdef CHECK_BUTTON								
              Page_1300.APP  = 3;   //充电启动方式 1:刷卡  2：手机APP
            #else 
	            Page_1300.APP  = 1;   //充电启动方式 1:刷卡  2：手机APP
            #endif	
					}else{
						Page_1300.APP = 1;   //充电启动方式 1:刷卡  2：手机APP
					}
					memcpy(&msg.argv[0], &Page_1300.reserve1, sizeof(PAGE_1300_FRAME));
				
  				if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
					  msg.type = 0x1300;//充电结束界面
				  }else{
						msg.type = 0x1310;//单枪结束界面
					}
					
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_1300_FRAME), IPC_NOWAIT);

					Globa_1->QT_Step = 0x23; // 2      充电结束 结算界面 2.3----------------

					Globa_1->charger_state = 0x05;
					Globa_1->checkout = 0;
					if(Globa_1->Start_Mode == VIN_START_TYPE){
						Globa_1->pay_step = 0;
					}
					else
					{
					  Globa_1->pay_step = 4;//1通道在等待刷卡扣费
					  if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
							Voicechip_operation_function(0x07);//07H     “充电已完成，请刷卡结算”
						}else{
							Voicechip_operation_function(0x10);//10H	请刷卡结算.wav
						}
					}
          Globa_1->Charger_Over_Rmb_flag = 0;
					count_wait = 0;
					break;
				}
				
	      if(Globa_1->Start_Mode != VIN_START_TYPE){
					if((Globa_1->qiyehao_flag == 0)&&(Globa_1->card_type == 1)&&((Globa_1->total_rmb + 50) >= Globa_1->last_rmb)){//用户卡余额不足,企业号不进行余额判断
						Globa_1->Manu_start = 0;
						Globa_1->Charger_Over_Rmb_flag = 1;
					}
					
					if((Globa_1->BMS_Info_falg == 1)&&(Bind_BmsVin_falg == 0)&&(Globa_1->have_hart == 1)){
						Bind_BmsVin_falg = 1; 
						if((0 != memcmp(&Globa_1->Bind_BmsVin[0], "00000000000000000", sizeof(Globa_1->Bind_BmsVin)))&&(0 != memcmp(&Globa_1->VIN[0], "00000000000000000", sizeof(Globa_1->VIN)))&&(!((Globa_1->VIN[0] == 0xFF)&&(Globa_1->VIN[1] == 0xFF)&&(Globa_1->VIN[2] == 0xFF)))){
							if(0 != memcmp(&Globa_1->Bind_BmsVin[0],&Globa_1->VIN[0], sizeof(Globa_1->Bind_BmsVin))){
								Globa_1->Manu_start = 0;
								Globa_1->Charger_Over_Rmb_flag = 4;//与绑定的VIN不一样
							}
						}
					}else if(Globa_1->BMS_Info_falg == 0){
						Bind_BmsVin_falg = 0;
					}
				}
				if(Globa_1->order_chg_flag == 1){//有序充电时，进行试驾判断，当时间大于
				  if(ISorder_chg_control()){
						Globa_1->Manu_start = 0;
						Globa_1->Charger_Over_Rmb_flag = 3;//有序充电时间到
					}
				}
				
			

				if(Globa_1->Time >  120)//120秒，充电启动2分种之后对电流判断
				{
					if(Globa_1->Charger_param.CurrentVoltageJudgmentFalg == 1)
					{ 						
						if(((abs(Globa_1->Output_current - Globa_1->meter_Current_A)*100 > METER_CURR_THRESHOLD_VALUE*Globa_1->Output_current)&&(Globa_1->Output_current >= LIMIT_CURRENT_VALUE))||
							((abs(Globa_1->Output_current - Globa_1->meter_Current_A) > 1000)&&(Globa_1->Output_current < LIMIT_CURRENT_VALUE)&&(Globa_1->Output_current >= MIN_LIMIT_CURRENT_VALUE)))
						{
							if(abs(Current_Systime - meter_Curr_Threshold_count) > METER_CURR_COUNT)//超过1分钟了，需停止充电
							{
								Globa_1->Manu_start = 0;
								Globa_1->Charger_Over_Rmb_flag = 5;//电流计量有误差
								meter_Curr_Threshold_count = Current_Systime;
								RunEventLogOut_1("计费QT判断计量有误差 Globa_1->meter_Current_A =%d Globa_1->Output_current=%d",Globa_1->meter_Current_A,Globa_1->Output_current);
							}
						}
						else
						{
							meter_Curr_Threshold_count = Current_Systime;
						}
					}
				
					if(0 == check_current_low_st)
					{
						if(Globa_1->BMS_batt_SOC >= Globa_1->Charger_param.SOC_limit   )//
						{						
							if(Globa_1->Output_current < (Globa_1->Charger_param.min_current*1000))//电流低于限值
							{
								current_low_timestamp = Current_Systime;
								check_current_low_st = 1;
							}
						}	
					}else//电流低持续是否达到1分钟
					{
						if(Globa_1->Output_current > (Globa_1->Charger_param.min_current*1000 + 1000))//1A回差
						{
							check_current_low_st = 0;//clear						
						}
						
						if(Globa_1->BMS_batt_SOC < Globa_1->Charger_param.SOC_limit   )
							check_current_low_st = 0;//clear
						
						if(Globa_1->Output_current < (Globa_1->Charger_param.min_current*1000))
						{
							if(abs(Current_Systime - current_low_timestamp) > SOCLIMIT_CURR_COUNT)//超过1分钟了，需停止充电
							{
								Globa_1->Manu_start = 0;	
								Globa_1->Charger_Over_Rmb_flag = 6;//达到SOC限制条件
								RunEventLogOut_1("计费QT判断达到soc限制条件");
                current_low_timestamp = Current_Systime;
							}
						}				
					}
					
					if((Globa_1->BMS_Info_falg == 1)&&(Globa_1->Battery_Rate_KWh > 200))//收到BMs额定信息,额定电度需要大于20kwh才进行判断
					{
						if(Globa_1->kwh >= ((100 - Globa_1->BMS_batt_Start_SOC + MAX_LIMIT_OVER_RATEKWH)*Globa_1->Battery_Rate_KWh/10))
						{
							if(abs(Current_Systime - current_overkwh_timestamp) > OVER_RATEKWH_COUNT)//超过1分钟了，需停止充电
							{
								Globa_1->Manu_start = 0;	
								Globa_1->Charger_Over_Rmb_flag = 8;//达到允许可充电电量
								RunEventLogOut_1("计费QT判断达到根据总电量和起始SOC算出需要的总电量：Battery_Rate_KWh =%d.%d  充电电量 Globa_1->kwh = %d.%d ：startsoc=%d endsoc=%d",Globa_1->Battery_Rate_KWh/10,Globa_1->Battery_Rate_KWh%10, Globa_1->kwh/100,Globa_1->kwh%100,Globa_1->BMS_batt_Start_SOC,Globa_1->BMS_batt_SOC);
                current_overkwh_timestamp = Current_Systime;
							}
						}else{
							current_overkwh_timestamp = Current_Systime;
						}
					}else{
						current_overkwh_timestamp = Current_Systime;
					}
						
				}
					
    		if(Globa_1->Card_charger_type == 3){//按金额充电
    			if((Globa_1->total_rmb + 50) >= Globa_1->QT_charge_money*100){
    				Globa_1->Manu_start = 0;
    			}
    		}else if(Globa_1->Card_charger_type == 1){//按电量充电
    			if((Globa_1->kwh/100) >= Globa_1->QT_charge_kwh){
    				Globa_1->Manu_start = 0;
    			}
    		}else if(Globa_1->Card_charger_type == 2){//按时间充电
    			if(Globa_1->user_sys_time >= Globa_1->QT_charge_time){
    				Globa_1->Manu_start = 0;
    			}
    		}

				count_wait++;
			//	count_wait1++;
				if((count_wait %5  == 0)&&(count_wait1 == 0)){//等待时间超时500mS
          count_wait1 = 1;
    			Page_1111.BATT_type = Globa_1->BATT_type;		
					Page_1111.BATT_V[0] = Globa_1->Output_voltage/100;
					Page_1111.BATT_V[1] = (Globa_1->Output_voltage/100)>>8;
					Page_1111.BATT_C[0] = Globa_1->Output_current/100;
					Page_1111.BATT_C[1] = (Globa_1->Output_current/100)>>8;
					Page_1111.KWH[0] =  Globa_1->kwh;
					Page_1111.KWH[1] =  Globa_1->kwh>>8;

					Page_1111.Capact =  Globa_1->BMS_batt_SOC;
					Page_1111.run    = (Globa_1->Output_current > 1000)?1:0;//有充电电流 》1A

					Page_1111.elapsed_time[0] = Globa_1->Time;
					Page_1111.elapsed_time[1] = Globa_1->Time>>8;
					Page_1111.need_time[0] = Globa_1->BMS_need_time;
					Page_1111.need_time[1] = Globa_1->BMS_need_time>>8;
					Page_1111.cell_HV[0] =  Globa_1->BMS_cell_HV;
					Page_1111.cell_HV[1] =  Globa_1->BMS_cell_HV>>8;
          Page_1111.money[0] =  Globa_1->total_rmb;
					Page_1111.money[1] =  Globa_1->total_rmb>>8;
          Page_1111.money[2] =  Globa_1->total_rmb>>16;
				  Page_1111.money[3] =  Globa_1->total_rmb>>24;

					Page_1111.charging = Page_1111.run;
					Page_1111.link =  (Globa_1->gun_link == 1)?1:0;
					memcpy(&Page_1111.SN[0], &Globa_1->card_sn[0], sizeof(Page_1111.SN));
					if(Globa_1->Start_Mode == VIN_START_TYPE){
            Page_1111.APP  = 3;   //充电启动方式 1:刷卡  2：手机APP
					}else{
						Page_1111.APP  = 1; 
					}
	        Page_1111.BMS_12V_24V = Globa_1->BMS_Power_V;
         	memcpy(&msg.argv[0], &Page_1111.reserve1, sizeof(PAGE_1111_FRAME));
					if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
						msg.type = 0x1111;//充电界面---双枪的时候
				  }else{
						msg.type = 0x1122;//充电界面--单枪 
					}
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_1111_FRAME), IPC_NOWAIT);
					QT_timer_start(500);
    		}else if((count_wait %5 == 0)&&(count_wait1 != 0)){//平均2S发一次数据
			  	count_wait1 = 0;
					Page_BMS_data.BMS_Info_falg = Globa_1->BMS_Info_falg;
				  Page_BMS_data.BMS_H_vol[0]=  Globa_1->BMS_H_voltage/100;
				  Page_BMS_data.BMS_H_vol[1]=  (Globa_1->BMS_H_voltage/100)>>8;
          Page_BMS_data.BMS_H_cur[0]=  Globa_1->BMS_H_current/100;
				  Page_BMS_data.BMS_H_cur[1]=  (Globa_1->BMS_H_current/100)>>8;
					Page_BMS_data.BMS_cell_LT[0]=  Globa_1->BMS_cell_LT;
				  Page_BMS_data.BMS_cell_LT[1]=  Globa_1->BMS_cell_LT>>8; 
					Page_BMS_data.BMS_cell_HT[0]=  Globa_1->BMS_cell_HT;
				  Page_BMS_data.BMS_cell_HT[1]=  Globa_1->BMS_cell_HT>>8;
	
					Page_BMS_data.need_voltage[0]=  Globa_1->need_voltage/100;
				  Page_BMS_data.need_voltage[1]=  (Globa_1->need_voltage/100)>>8;
					Page_BMS_data.need_current[0]=  (Globa_1->need_current/100);
				  Page_BMS_data.need_current[1]=  (Globa_1->need_current/100)>>8;
					
					memcpy(&Page_BMS_data.BMS_Vel[0],&Globa_1->BMS_Vel[0],sizeof(Page_BMS_data.BMS_Vel));
					
					Page_BMS_data.Battery_Rate_Vol[0]=  Globa_1->BMS_rate_voltage;
				  Page_BMS_data.Battery_Rate_Vol[1]=  (Globa_1->BMS_rate_voltage)>>8;
					Page_BMS_data.Battery_Rate_AH[0]=  Globa_1->BMS_rate_AH;
				  Page_BMS_data.Battery_Rate_AH[1]=  (Globa_1->BMS_rate_AH)>>8;
					Page_BMS_data.Battery_Rate_KWh[0]=  (Globa_1->Battery_Rate_KWh);
				  Page_BMS_data.Battery_Rate_KWh[1]=  (Globa_1->Battery_Rate_KWh)>>8;
				
 				  Page_BMS_data.Cell_H_Cha_Vol[0]=  (Globa_1->Cell_H_Cha_Vol);
				  Page_BMS_data.Cell_H_Cha_Vol[1]=  (Globa_1->Cell_H_Cha_Vol)>>8;
				  Page_BMS_data.Cell_H_Cha_Temp =  Globa_1->Cell_H_Cha_Temp;
				  Page_BMS_data.Charge_Mode =  Globa_1->Charge_Mode;
				  Page_BMS_data.Charge_Gun_Temp =  Globa_1->Charge_Gun_Temp;
				  Page_BMS_data.BMS_cell_HV_NO =  Globa_1->BMS_cell_HV_NO;
				  Page_BMS_data.BMS_cell_LT_NO =  Globa_1->BMS_cell_LT_NO;
				  Page_BMS_data.BMS_cell_HT_NO =  Globa_1->BMS_cell_HT_NO;
					memcpy(&Page_BMS_data.VIN[0],&Globa_1->VIN[0],sizeof(Page_BMS_data.VIN));
  				memcpy(&msg.argv[0], &Page_BMS_data.reserve1, sizeof(PAGE_BMS_FRAME));
					if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
						msg.type = 0x1188;//充电界面---双枪的时候
				  }else{
						msg.type = 0x1588;//充电界面--单枪 0x1588
					}
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_BMS_FRAME), IPC_NOWAIT);
				}  

				break;
			}
			case 0x22:{//---------------2 自动充电-》 APP 充电界面 状态  2.2----------
			
			//	Globa_1->meter_stop_KWH = Globa_1->meter_KWH;//取初始电表示数
				
				if(Globa_1->meter_KWH >= Globa_1->meter_stop_KWH)
				{
					Globa_1->meter_stop_KWH = Globa_1->meter_KWH;//取初始电表示数
					Electrical_abnormality = Current_Systime;
				}else{//连续3s获取到的数据都比较起始数据小，则直接停止充电
					if(abs(Current_Systime - Electrical_abnormality) > METER_ELECTRICAL_ERRO_COUNT)//超过1分钟了，需停止充电
					{
						Electrical_abnormality = Current_Systime;
						Globa_1->Manu_start = 0;
						Globa_1->Charger_Over_Rmb_flag = 7;//读取电量有异常
						RunEventLogOut_1("计费QT判断读取电量有异常：Globa_1->meter_KWH=%d Globa_1->meter_stop_KWH=%d",Globa_1->meter_KWH,Globa_1->meter_stop_KWH);
					}
				}
				
				Globa_1->kwh = (Globa_1->meter_stop_KWH - Globa_1->meter_start_KWH);
				g_ulcharged_kwh[pre_price_index] = Globa_1->kwh - ul_start_kwh;//累加当前时段的耗电量	
				
				cur_price_index = GetCurPriceIndex(); //获取当前时间
				if(cur_price_index != pre_price_index){
					ul_start_kwh = Globa_1->kwh - g_ulcharged_kwh[cur_price_index];//load new start kwh,需要减去时段之前累加的电量，否则该时段之前累加的电量丢失----20160312
					pre_price_index = cur_price_index;							
				}	
				for(i = 0;i<4;i++){
					g_ulcharged_rmb[i] = g_ulcharged_kwh[i]*share_time_kwh_price[i];
				}
	     
				for(i = 0;i<4;i++){
					g_ulcharged_rmb[i] = (g_ulcharged_rmb[i] + COMPENSATION_VALUE)/100;//4
				}
				Globa_1->rmb = g_ulcharged_rmb[0] + g_ulcharged_rmb[1] + g_ulcharged_rmb[2] + g_ulcharged_rmb[3]; 

				if(Serv_Type == 1){ //按次数收
					Globa_1->total_rmb = (Globa_1->rmb + Globa_1->Charger_param.Serv_Price);//已充电电量对应的金额
				}else if(Serv_Type == 2){//按时间收
					Globa_1->total_rmb = (Globa_1->rmb + Globa_1->Time*Globa_1->Charger_param.Serv_Price/600);//已充电电量对应的金额
				}else{
					//Globa_1->total_rmb = (Globa_1->rmb + Globa_1->kwh*Globa_1->Charger_param.Serv_Price/100);//已充电电量对应的金额
					for(i = 0;i<4;i++){
						g_ulcharged_rmb_Serv[i] = g_ulcharged_kwh[i]*share_time_kwh_price_serve[i];
					}
					for(i = 0;i<4;i++)
					{
						g_ulcharged_rmb_Serv[i] = (g_ulcharged_rmb_Serv[i]+ COMPENSATION_VALUE)/100;
					}
					Globa_1->total_rmb = Globa_1->rmb + g_ulcharged_rmb_Serv[0] + g_ulcharged_rmb_Serv[1] + g_ulcharged_rmb_Serv[2] + g_ulcharged_rmb_Serv[3]; 
				}
				Globa_1->ISO_8583_rmb_Serv = Globa_1->total_rmb - Globa_1->rmb ;               //总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
	      Globa_1->ISO_8583_rmb = Globa_1->rmb;                     //电量消费金额 0.01yuan
	
				
				//充电过程中实时更新业务数据记录;
				//若意外掉电，则上电时判断最后一条数据记录的“上传标示”是否为“0x55”;
				//若是，以此时电表示数作为上次充电结束时的示数;
				if((Globa_1->kwh - temp_kwh) > 50){//每隔0.5度电左右保存一次数据;
					temp_kwh = Globa_1->kwh;

					sprintf(&temp_char[0], "%08d", Globa_1->meter_stop_KWH);				
					memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

					sprintf(&temp_char[0], "%08d", Globa_1->kwh);				
					memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
					
					sprintf(&temp_char[0], "%08d", Globa_1->rmb);				
					memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));
          if(Globa_1->kwh != 0){
	          sprintf(&temp_char[0], "%08d", (Globa_1->rmb*100/Globa_1->kwh));  
	        }else{
	          sprintf(&temp_char[0], "%08d", 0);  
	        }
					memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));
					sprintf(&temp_char[0], "%08d", Globa_1->total_rmb);				
					memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));

					sprintf(&temp_char[0], "%08d", Globa_1->total_rmb - Globa_1->rmb);				
					memcpy(&busy_frame.server_rmb[0], &temp_char[0], sizeof(busy_frame.server_rmb));

					systime = time(NULL);         //获得系统的当前时间 
					localtime_r(&systime, &tm_t);  //结构指针指向当前时间	       				
					sprintf(&temp_char[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
			                             tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
					memcpy(&busy_frame.end_time[0], &temp_char[0], sizeof(busy_frame.end_time));

					tmp_value = g_ulcharged_rmb[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.sharp_allrmb[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb));
				
					tmp_value = g_ulcharged_rmb[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame.peak_allrmb[0], &temp_char[0], sizeof(busy_frame.peak_allrmb));
					
					tmp_value = g_ulcharged_rmb[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame.flat_allrmb[0], &temp_char[0], sizeof(busy_frame.flat_allrmb));
				
					tmp_value = g_ulcharged_rmb[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.valley_allrmb[0], &temp_char[0], sizeof(busy_frame.valley_allrmb));
					
					tmp_value = g_ulcharged_rmb_Serv[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.sharp_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb_Serv));

					tmp_value = g_ulcharged_rmb_Serv[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame.peak_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.peak_allrmb_Serv));
	
					tmp_value = g_ulcharged_rmb_Serv[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame.flat_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.flat_allrmb_Serv));
				
					tmp_value = g_ulcharged_rmb_Serv[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.valley_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.valley_allrmb_Serv));
					
					tmp_value =0;
					pthread_mutex_lock(&busy_db_pmutex);
					Update_Charger_Busy_DB_1(id, &busy_frame, 1, 0x55);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
					pthread_mutex_unlock(&busy_db_pmutex);
				}

				//手机终止充电
				if((Globa_1->App.APP_start == 0)||(Globa_1->Electric_pile_workstart_Enable == 1)||((Globa_1->APP_Start_Account_type == 1)&&(Globa_1->have_hart == 0))){
     			RunEventLogOut_1("计费QT判断用户主远程动停止充电");
					Globa_1->Manu_start = 0;
					
          Globa_1->Special_price_APP_data_falg = 0; 
					Globa_1->App.APP_start = 0;
					Globa_1->APP_Start_Account_type = 0;
          sleep(3);
					Globa_1->meter_stop_KWH = (Globa_1->meter_KWH >= Globa_1->meter_stop_KWH)? Globa_1->meter_KWH:Globa_1->meter_stop_KWH;//取初始电表示数
				  Globa_1->kwh = (Globa_1->meter_stop_KWH - Globa_1->meter_start_KWH);
					g_ulcharged_kwh[pre_price_index] = Globa_1->kwh - ul_start_kwh;//累加当前时段的耗电量	
					
					cur_price_index = GetCurPriceIndex(); //获取当前时间
					if(cur_price_index != pre_price_index){
						ul_start_kwh = Globa_1->kwh - g_ulcharged_kwh[cur_price_index];//load new start kwh,需要减去时段之前累加的电量，否则该时段之前累加的电量丢失----20160312
						pre_price_index = cur_price_index;							
					}	
					for(i = 0;i<4;i++){
						g_ulcharged_rmb[i] = g_ulcharged_kwh[i]*share_time_kwh_price[i];
					}
				 
					for(i = 0;i<4;i++){
						g_ulcharged_rmb[i] = (g_ulcharged_rmb[i] + COMPENSATION_VALUE)/100;//4
					}
					Globa_1->rmb = g_ulcharged_rmb[0] + g_ulcharged_rmb[1] + g_ulcharged_rmb[2] + g_ulcharged_rmb[3]; 

					if(Serv_Type == 1){ //按次数收
						Globa_1->total_rmb = (Globa_1->rmb + Globa_1->Charger_param.Serv_Price);//已充电电量对应的金额
					}else if(Serv_Type == 2){//按时间收
						Globa_1->total_rmb = (Globa_1->rmb + Globa_1->Time*Globa_1->Charger_param.Serv_Price/600);//已充电电量对应的金额
					}else{
						//Globa_1->total_rmb = (Globa_1->rmb + Globa_1->kwh*Globa_1->Charger_param.Serv_Price/100);//已充电电量对应的金额
						for(i = 0;i<4;i++){
							g_ulcharged_rmb_Serv[i] = g_ulcharged_kwh[i]*share_time_kwh_price_serve[i];
						}
						for(i = 0;i<4;i++)
						{
							g_ulcharged_rmb_Serv[i] = (g_ulcharged_rmb_Serv[i]+ COMPENSATION_VALUE)/100;
						}
						Globa_1->total_rmb = Globa_1->rmb + g_ulcharged_rmb_Serv[0] + g_ulcharged_rmb_Serv[1] + g_ulcharged_rmb_Serv[2] + g_ulcharged_rmb_Serv[3]; 
					}

				  Globa_1->ISO_8583_rmb_Serv = Globa_1->total_rmb - Globa_1->rmb ;               //总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
	        Globa_1->ISO_8583_rmb = Globa_1->rmb;                     //电量消费金额 0.01yuan
	
					//-------------------扣费 更新业务进度数据----------------------------
					sprintf(&temp_char[0], "%08d", Globa_1->meter_stop_KWH);				
					memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

					sprintf(&temp_char[0], "%08d", Globa_1->kwh);				
					memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
					
					sprintf(&temp_char[0], "%08d", Globa_1->rmb);				
					memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));
          if(Globa_1->kwh != 0){
	          sprintf(&temp_char[0], "%08d", (Globa_1->rmb*100/Globa_1->kwh));  
	        }else{
	          sprintf(&temp_char[0], "%08d", 0);  
	        }
					memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));
  
					sprintf(&temp_char[0], "%08d", Globa_1->total_rmb);				
					memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));

					sprintf(&temp_char[0], "%08d", Globa_1->total_rmb - Globa_1->rmb);				
					memcpy(&busy_frame.server_rmb[0], &temp_char[0], sizeof(busy_frame.server_rmb));

					systime = time(NULL);         //获得系统的当前时间 
					localtime_r(&systime, &tm_t);  //结构指针指向当前时间	       				
					sprintf(&temp_char[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
			                             tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
					memcpy(&busy_frame.end_time[0], &temp_char[0], sizeof(busy_frame.end_time));
					tmp_value = g_ulcharged_rmb[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.sharp_allrmb[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb));
				
					tmp_value = g_ulcharged_rmb[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame.peak_allrmb[0], &temp_char[0], sizeof(busy_frame.peak_allrmb));
					
					tmp_value = g_ulcharged_rmb[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame.flat_allrmb[0], &temp_char[0], sizeof(busy_frame.flat_allrmb));
				
					tmp_value = g_ulcharged_rmb[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.valley_allrmb[0], &temp_char[0], sizeof(busy_frame.valley_allrmb));
					
					tmp_value = g_ulcharged_rmb_Serv[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.sharp_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb_Serv));
					
					tmp_value = g_ulcharged_rmb_Serv[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame.peak_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.peak_allrmb_Serv));

					tmp_value = g_ulcharged_rmb_Serv[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame.flat_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.flat_allrmb_Serv));
				
					tmp_value = g_ulcharged_rmb_Serv[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.valley_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.valley_allrmb_Serv));
					
					tmp_value =0;
					Page_1300.cause = 11; 
					sprintf(&temp_char[0], "%02d", Page_1300.cause);		
				  memcpy(&busy_frame.End_code[0], &temp_char[0], sizeof(busy_frame.End_code));//结束原因代码
				  Globa_1->Charger_Over_Rmb_flag = 0;
					memcpy(&busy_frame.car_sn[0], &Globa_1->VIN, sizeof(busy_frame.car_sn));
					
					pthread_mutex_lock(&busy_db_pmutex);
					Update_Charger_Busy_DB_1(id, &busy_frame, 1, 0x66);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
	        pthread_mutex_unlock(&busy_db_pmutex);
        
			    sleep(1);
				  pthread_mutex_lock(&busy_db_pmutex);		
				  Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
				  pthread_mutex_unlock(&busy_db_pmutex);

					//-------------------界面显示数据内容---------------------------------
					if( Globa_1->Charger_param.NEW_VOICE_Flag == 1){
				  	Voicechip_operation_function(0x09);//09H     “谢谢惠顾，再见”
					}else{
						Voicechip_operation_function(0x11);//11H	谢谢使用再见.wav
					}
					Globa_1->charger_state = 0x03;//为了使手机控制停止后可以再启动，此处充电机置为空闲状态
					Globa_1->gun_state = 0x03;
				 
					Page_1300.cause = 11;//直接手动结束的 ;
					Page_1300.Capact = Globa_1->BMS_batt_SOC;
					Page_1300.elapsed_time[0] =  Globa_1->Time;
					Page_1300.elapsed_time[1] =  Globa_1->Time>>8;
					Page_1300.KWH[0] =  Globa_1->kwh;
					Page_1300.KWH[1] =  Globa_1->kwh>>8;
					Page_1300.RMB[0] =  Globa_1->total_rmb;
					Page_1300.RMB[1] =  Globa_1->total_rmb>>8;
					Page_1300.RMB[2] =  Globa_1->total_rmb>>16;
					Page_1300.RMB[3] =  Globa_1->total_rmb>>24;
					memcpy(&Page_1300.SN[0], &busy_frame.card_sn[0], sizeof(Page_1300.SN));
					Page_1300.APP = 2;   //充电启动方式 1:刷卡  2：手机APP

					memcpy(&msg.argv[0], &Page_1300.reserve1, sizeof(PAGE_1300_FRAME));
					if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
					  msg.type = 0x1300;//充电结束界面
				  }else{
						msg.type = 0x1310;//单枪结束界面
					}
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_1300_FRAME), IPC_NOWAIT);

			  	sleep(1);//不用延时太久，APP控制，需要立即上传业务数据//插入到需要上传的交易记录数据库中
					pthread_mutex_lock(&busy_db_pmutex);
				  all_id = 0;
					Insertflag = Insert_Charger_Busy_DB(&busy_frame, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
					if((Insertflag != -1)&&(all_id != 0)){//表示插入数据成功，其他的则需要重新插入
						Delete_Record_busy_DB(id);
						if(DEBUGLOG) RUN_GUN1_LogOut("一号枪交易数据,插入到正式数据库中并删除临时数据库记录 ID = %d",id);  
						id = 0;
					}else{
						if(DEBUGLOG) RUN_GUN1_LogOut("一号枪交易数据,插入到正式数据库中失败 ID = %d",id);  
					}
					pthread_mutex_unlock(&busy_db_pmutex);
					
					if(id != 0 ){
						pthread_mutex_lock(&busy_db_pmutex);
						for(i=0;i<5;i++){
							if(Select_NO_Over_Record_1(id,0)!= -1){//表示有没有上传的记录
								Select_NO_Over_count = 0;
								memcpy(&pre_old_busy_frame, &busy_frame, sizeof(pre_old_busy_frame));
								pre_old_id = id;
								Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
							}else{
								Select_NO_Over_count = 0;
								pre_old_id = 0;	
								id = 0;
								break;
							}
							sleep(1);
						}
						pthread_mutex_unlock(&busy_db_pmutex);
					}
					
					Globa_1->BMS_Power_V = 0;
	        Globa_1->Operation_command = 2;//进行解锁
					memset(&Globa_1->App.ID, '0', sizeof(Globa_1->App.ID));
				  if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
						msg.type = 0x150;
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);						
						QT_timer_stop(500);
						Globa_1->QT_Step = 0x03; //自动充电分开的AB通道
					}else{//单枪
						//防止按键处理不同步，此处同步一下
						msg.type = 0x150;//
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_2->QT_Step = 0x03; //主界面
						
						msg.type = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选着界面，才有此按钮功能）
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_1->QT_Step = 0x02; //主界面
					}
					
					break;
				}

    		if(Globa_1->charger_start == 0){//充电彻底结束处理
    			Globa_1->App.APP_start = 0;
          Globa_1->Special_price_APP_data_falg = 0; 
          sleep(2);
					Globa_1->meter_stop_KWH = (Globa_1->meter_KWH >= Globa_1->meter_stop_KWH)? Globa_1->meter_KWH:Globa_1->meter_stop_KWH;//取初始电表示数
					 Globa_1->kwh = (Globa_1->meter_stop_KWH - Globa_1->meter_start_KWH);
					 g_ulcharged_kwh[pre_price_index] = Globa_1->kwh - ul_start_kwh;//累加当前时段的耗电量	
					
					cur_price_index = GetCurPriceIndex(); //获取当前时间
					if(cur_price_index != pre_price_index){
						ul_start_kwh = Globa_1->kwh - g_ulcharged_kwh[cur_price_index];//load new start kwh,需要减去时段之前累加的电量，否则该时段之前累加的电量丢失----20160312
						pre_price_index = cur_price_index;							
					}	
					for(i = 0;i<4;i++){
						g_ulcharged_rmb[i] = g_ulcharged_kwh[i]*share_time_kwh_price[i];
					}
				 	for(i = 0;i<4;i++){
						g_ulcharged_rmb[i] = (g_ulcharged_rmb[i] + COMPENSATION_VALUE)/100;//4
					}
					Globa_1->rmb = g_ulcharged_rmb[0] + g_ulcharged_rmb[1] + g_ulcharged_rmb[2] + g_ulcharged_rmb[3]; 

					if(Serv_Type == 1){ //按次数收
						Globa_1->total_rmb = (Globa_1->rmb + Globa_1->Charger_param.Serv_Price);//已充电电量对应的金额
					}else if(Serv_Type == 2){//按时间收
						Globa_1->total_rmb = (Globa_1->rmb + Globa_1->Time*Globa_1->Charger_param.Serv_Price/600);//已充电电量对应的金额
					}else{
						//Globa_1->total_rmb = (Globa_1->rmb + Globa_1->kwh*Globa_1->Charger_param.Serv_Price/100);//已充电电量对应的金额
						for(i = 0;i<4;i++){
							g_ulcharged_rmb_Serv[i] = g_ulcharged_kwh[i]*share_time_kwh_price_serve[i];
						}
						for(i = 0;i<4;i++)
						{
							g_ulcharged_rmb_Serv[i] = (g_ulcharged_rmb_Serv[i]+ COMPENSATION_VALUE)/100;
						}
						Globa_1->total_rmb = Globa_1->rmb + g_ulcharged_rmb_Serv[0] + g_ulcharged_rmb_Serv[1] + g_ulcharged_rmb_Serv[2] + g_ulcharged_rmb_Serv[3]; 
					}

				  
					Globa_1->ISO_8583_rmb_Serv = Globa_1->total_rmb - Globa_1->rmb ;               //总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
	        Globa_1->ISO_8583_rmb = Globa_1->rmb;                     //电量消费金额 0.01yuan
	
					//-------------------扣费 更新业务进度数据----------------------------
					sprintf(&temp_char[0], "%08d", Globa_1->meter_stop_KWH);				
					memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

					sprintf(&temp_char[0], "%08d", Globa_1->kwh);				
					memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
					
					sprintf(&temp_char[0], "%08d", Globa_1->rmb);				
					memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));
          if(Globa_1->kwh != 0){
	          sprintf(&temp_char[0], "%08d", (Globa_1->rmb*100/Globa_1->kwh));  
	        }else{
	          sprintf(&temp_char[0], "%08d", 0);  
	        }
					memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));
   
					sprintf(&temp_char[0], "%08d", Globa_1->total_rmb);				
					memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));

					sprintf(&temp_char[0], "%08d", Globa_1->total_rmb - Globa_1->rmb);				
					memcpy(&busy_frame.server_rmb[0], &temp_char[0], sizeof(busy_frame.server_rmb));

					systime = time(NULL);         //获得系统的当前时间 
					localtime_r(&systime, &tm_t);  //结构指针指向当前时间	       				
					sprintf(&temp_char[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
			                             tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
					memcpy(&busy_frame.end_time[0], &temp_char[0], sizeof(busy_frame.end_time));
		      tmp_value = g_ulcharged_rmb[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.sharp_allrmb[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb));
				
					tmp_value = g_ulcharged_rmb[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame.peak_allrmb[0], &temp_char[0], sizeof(busy_frame.peak_allrmb));
					
					tmp_value = g_ulcharged_rmb[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame.flat_allrmb[0], &temp_char[0], sizeof(busy_frame.flat_allrmb));
				
					tmp_value = g_ulcharged_rmb[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.valley_allrmb[0], &temp_char[0], sizeof(busy_frame.valley_allrmb));
					tmp_value = g_ulcharged_rmb_Serv[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.sharp_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb_Serv));

					tmp_value = g_ulcharged_rmb_Serv[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame.peak_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.peak_allrmb_Serv));
	
					tmp_value = g_ulcharged_rmb_Serv[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame.flat_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.flat_allrmb_Serv));
				
					tmp_value = g_ulcharged_rmb_Serv[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.valley_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.valley_allrmb_Serv));
					
					tmp_value =0;
					sprintf(&temp_char[0], "%02d", Globa_1->charger_over_flag);	
          memcpy(&busy_frame.End_code[0],&temp_char[0], sizeof(busy_frame.End_code));//结束原因代码
      		
					pthread_mutex_lock(&busy_db_pmutex);
					Update_Charger_Busy_DB_1(id, &busy_frame, 1, 0x66);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
					pthread_mutex_unlock(&busy_db_pmutex);
					usleep(200000);		
					//-------------------界面显示数据内容---------------------------------
					if(Globa_1->Charger_Over_Rmb_flag == 1){
						Page_1300.cause = 13; 
						sent_warning_message(0x94, Page_1300.cause, 1, 0);
					}else if (Globa_1->Charger_Over_Rmb_flag == 5){
						Page_1300.cause = 65;
						sent_warning_message(0x94, Page_1300.cause, 1, 0);
					}else if (Globa_1->Charger_Over_Rmb_flag == 6){//达到SOC限制条件
						Page_1300.cause = 9;
						sent_warning_message(0x94, Page_1300.cause, 1, 0);
					}else if(Globa_1->Charger_Over_Rmb_flag == 7){//读取到的电表电量异常
						Page_1300.cause = 8;
						sent_warning_message(0x94, Page_1300.cause, 1, 0);							
					}else if(Globa_1->Charger_Over_Rmb_flag == 8){//充电电流超过SOC能允许充电的值
						Page_1300.cause = 7;
						sent_warning_message(0x94, Page_1300.cause, 1, 0);							
					}else{
					  Page_1300.cause = Globa_1->charger_over_flag; 
					}
					RunEventLogOut_1("计费QT判断充电结束:Page_1300.cause =%d,Globa_1->charger_over_flag =%d ,Charger_Over_Rmb_flag =%d ",Page_1300.cause,Globa_1->charger_over_flag,Globa_1->Charger_Over_Rmb_flag);

					sprintf(&temp_char[0], "%02d", Page_1300.cause);		
				  memcpy(&busy_frame.End_code[0], &temp_char[0], sizeof(busy_frame.End_code));//结束原因代码
          Globa_1->Charger_Over_Rmb_flag = 0;
					
	  			memcpy(&busy_frame.car_sn[0], &Globa_1->VIN, sizeof(busy_frame.car_sn));

          pthread_mutex_lock(&busy_db_pmutex);	
          Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00);					
					pthread_mutex_unlock(&busy_db_pmutex);

					//-------------------界面显示数据内容---------------------------------
	        if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
				  	Voicechip_operation_function(0x09);//09H     “谢谢惠顾，再见”
					}else{
						Voicechip_operation_function(0x11);//11H	谢谢使用再见.wav
					}					
					Globa_1->charger_state = 0x03;//为了使手机控制停止后可以再启动，此处充电机置为空闲状态
					Globa_1->gun_state = 0x03;
					
				//	Page_1300.cause = Globa_1->charger_over_flag;
					Page_1300.Capact = Globa_1->BMS_batt_SOC;
					Page_1300.elapsed_time[0] =  Globa_1->Time;
					Page_1300.elapsed_time[1] =  Globa_1->Time>>8;
					Page_1300.KWH[0] =  Globa_1->kwh;
					Page_1300.KWH[1] =  Globa_1->kwh>>8;
					Page_1300.RMB[0] =  Globa_1->total_rmb;
					Page_1300.RMB[1] =  Globa_1->total_rmb>>8;
					Page_1300.RMB[2] =  Globa_1->total_rmb>>16;
					Page_1300.RMB[3] =  Globa_1->total_rmb>>24;
					
					memcpy(&Page_1300.SN[0], &busy_frame.card_sn[0], sizeof(Page_1300.SN));
					Page_1300.APP = 2;   //充电启动方式 1:刷卡  2：手机APp
					memcpy(&msg.argv[0], &Page_1300.reserve1, sizeof(PAGE_1300_FRAME));
  				if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
					  msg.type = 0x1300;//充电结束界面
				  }else{
						msg.type = 0x1310;//单枪结束界面
					}
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_1300_FRAME), IPC_NOWAIT);

			    sleep(1);//不用延时太久，APP控制，需要立即上传业务数据
					//插入到需要上传的交易记录数据库中
					pthread_mutex_lock(&busy_db_pmutex);
					all_id = 0;
					Insertflag = Insert_Charger_Busy_DB(&busy_frame, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
					if((Insertflag != -1)&&(all_id != 0)){//表示插入数据成功，其他的则需要重新插入
						Delete_Record_busy_DB(id);
						if(DEBUGLOG) RUN_GUN1_LogOut("一号枪交易数据,插入到正式数据库中并删除临时数据库记录 ID = %d",id);  
						id = 0;
					}else{
						if(DEBUGLOG) RUN_GUN1_LogOut("一号枪交易数据,插入到正式数据库中失败 ID = %d",id);  
					}
					pthread_mutex_unlock(&busy_db_pmutex);
					
					if(id != 0){
						pthread_mutex_lock(&busy_db_pmutex);
						for(i=0;i<5;i++){
							usleep(200000);
							if(Select_NO_Over_Record_1(id,0)!= -1){//表示有没有上传的记录
								Select_NO_Over_count = 0;
								memcpy(&pre_old_busy_frame, &busy_frame, sizeof(pre_old_busy_frame));
								pre_old_id = id;
								Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上
							}else{
								if(DEBUGLOG) RUN_GUN1_LogOut("%s %d","1号枪交易数据,更新upm=0x00成功 ID=",id);  
								Select_NO_Over_count = 0;
								pre_old_id = 0;	
								id = 0;
								break;
							}
						}
						pthread_mutex_unlock(&busy_db_pmutex);
					}
					
					Globa_1->BMS_Power_V = 0;
					Globa_1->Operation_command = 2;//进行解锁
					memset(&Globa_1->App.ID, '0', sizeof(Globa_1->App.ID));
					
				  if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
						msg.type = 0x150;
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);						
						QT_timer_stop(500);
						Globa_1->QT_Step = 0x03; //自动充电分开的AB通道
					}else{//单枪
						//防止按键处理不同步，此处同步一下
						msg.type = 0x150;//
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_2->QT_Step = 0x03; //主界面
						
						msg.type = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选着界面，才有此按钮功能）
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_1->QT_Step = 0x02; //主界面
					}
					break;
				}

    		if((Globa_1->total_rmb + 50) >= Globa_1->App.last_rmb){//用户卡余额不足
    			Globa_1->Manu_start = 0;
					Globa_1->Charger_Over_Rmb_flag = 1;
    		}
				
		 
				if(Globa_1->Time >  120)//120秒，充电启动2分种之后对电流判断
				{
					if(Globa_1->Charger_param.CurrentVoltageJudgmentFalg == 1)
					{ 						
						if(((abs(Globa_1->Output_current - Globa_1->meter_Current_A)*100 > METER_CURR_THRESHOLD_VALUE*Globa_1->Output_current)&&(Globa_1->Output_current >= LIMIT_CURRENT_VALUE))||
							((abs(Globa_1->Output_current - Globa_1->meter_Current_A) > 1000)&&(Globa_1->Output_current < LIMIT_CURRENT_VALUE)&&(Globa_1->Output_current >= MIN_LIMIT_CURRENT_VALUE)))
						{
							if(abs(Current_Systime - meter_Curr_Threshold_count) > METER_CURR_COUNT)//超过1分钟了，需停止充电
							{
								Globa_1->Manu_start = 0;
								Globa_1->Charger_Over_Rmb_flag = 5;//电流计量有误差
								meter_Curr_Threshold_count = Current_Systime;
								RunEventLogOut_1("计费QT判断计量有误差 Globa_1->meter_Current_A =%d Globa_1->Output_current=%d",Globa_1->meter_Current_A,Globa_1->Output_current);

							}
						}
						else
						{
							meter_Curr_Threshold_count = Current_Systime;
						}
					}			

					if(0 == check_current_low_st)
					{
						if(Globa_1->BMS_batt_SOC >= Globa_1->Charger_param.SOC_limit   )//
						{						
							if(Globa_1->Output_current < (Globa_1->Charger_param.min_current*1000))//电流低于限值
							{
								current_low_timestamp = Current_Systime;
								check_current_low_st = 1;
							}
						}	
					}else//电流低持续是否达到1分钟
					{
						if(Globa_1->Output_current > (Globa_1->Charger_param.min_current*1000 + 1000))//1A回差
						{
							check_current_low_st = 0;//clear						
						}
						
						if(Globa_1->BMS_batt_SOC < Globa_1->Charger_param.SOC_limit   )
							check_current_low_st = 0;//clear
						
						if(Globa_1->Output_current < (Globa_1->Charger_param.min_current*1000))
						{
							if(abs(Current_Systime - current_low_timestamp) > SOCLIMIT_CURR_COUNT)//超过1分钟了，需停止充电
							{
								Globa_1->Manu_start = 0;	
								Globa_1->Charger_Over_Rmb_flag = 6;//达到SOC限制条件
								current_low_timestamp = Current_Systime;
								RunEventLogOut_1("计费QT判断达到soc限制条件");

							}
						}				
					}
					
					if((Globa_1->BMS_Info_falg == 1)&&(Globa_1->Battery_Rate_KWh > 200))//收到BMs额定信息,额定电度需要大于20kwh才进行判断
					{
						if(Globa_1->kwh >= ((100 - Globa_1->BMS_batt_Start_SOC + MAX_LIMIT_OVER_RATEKWH)*Globa_1->Battery_Rate_KWh/10))
						{
							if(abs(Current_Systime - current_overkwh_timestamp) > OVER_RATEKWH_COUNT)//超过1分钟了，需停止充电
							{
								Globa_1->Manu_start = 0;	
								Globa_1->Charger_Over_Rmb_flag = 8;//达到SOC限制条件
								RunEventLogOut_1("计费QT判断达到根据总电量和起始SOC算出需要的总电量：Battery_Rate_KWh =%d.%d  充电电量 Globa_1->kwh = %d.%d ：startsoc=%d endsoc=%d",Globa_1->Battery_Rate_KWh/10,Globa_1->Battery_Rate_KWh%10, Globa_1->kwh/100,Globa_1->kwh%100,Globa_1->BMS_batt_Start_SOC,Globa_1->BMS_batt_SOC);
                current_overkwh_timestamp = Current_Systime;
							}
						}else{
							current_overkwh_timestamp = Current_Systime;
						}
					}else{
						current_overkwh_timestamp = Current_Systime;
					}
				}
				
    		if(Globa_1->App.APP_charger_type == 1){//按电量充电
    			if(Globa_1->kwh >= Globa_1->App.APP_charge_kwh){
    				Globa_1->Manu_start = 0;
    			}
    		}else if(Globa_1->App.APP_charger_type == 2){//按时间充电
    			if(Globa_1->user_sys_time >= Globa_1->App.APP_charge_time){
    				Globa_1->Manu_start = 0;
    			}
    		}else if(Globa_1->App.APP_charger_type == 3){//按金额充电
    			if((Globa_1->total_rmb + 50) >= Globa_1->App.APP_charge_money){
    				Globa_1->Manu_start = 0;
    			}
    		}

				count_wait++;
			//	count_wait1++;
		  	if((count_wait %5  == 0)&&(count_wait1 == 0)){//等待时间超时500mS
          count_wait1 = 1;
//    		}else if(QT_timer_tick(500) == 1){
    			//定时刷新数据
    	    Page_1111.BATT_type = Globa_1->BATT_type;		
					Page_1111.BATT_V[0] = Globa_1->Output_voltage/100;
					Page_1111.BATT_V[1] = (Globa_1->Output_voltage/100)>>8;
					Page_1111.BATT_C[0] = Globa_1->Output_current/100;
					Page_1111.BATT_C[1] = (Globa_1->Output_current/100)>>8;
					Page_1111.KWH[0] =  Globa_1->kwh;
					Page_1111.KWH[1] =  Globa_1->kwh>>8;

					Page_1111.Capact =  Globa_1->BMS_batt_SOC;
					Page_1111.run    = (Globa_1->Output_current > 1000)?1:0;//有充电电流 》1A

					Page_1111.elapsed_time[0] = Globa_1->Time;
					Page_1111.elapsed_time[1] = Globa_1->Time>>8;
					Page_1111.need_time[0] = Globa_1->BMS_need_time;
					Page_1111.need_time[1] = Globa_1->BMS_need_time>>8;
					Page_1111.cell_HV[0] =  Globa_1->BMS_cell_HV;
					Page_1111.cell_HV[1] =  Globa_1->BMS_cell_HV>>8;
		
          Page_1111.money[0] =  Globa_1->total_rmb;
					Page_1111.money[1] =  Globa_1->total_rmb>>8;
          Page_1111.money[2] =  Globa_1->total_rmb>>16;
				  Page_1111.money[3] =  Globa_1->total_rmb>>24;
					Page_1111.charging = Page_1111.run;
					Page_1111.link =  (Globa_1->gun_link == 1)?1:0;

					memcpy(&Page_1111.SN[0], &busy_frame.card_sn[0], sizeof(Page_1111.SN));
					Page_1111.APP = 2;   //充电启动方式 1:刷卡  2：手机APP
          Page_1111.BMS_12V_24V = Globa_1->BMS_Power_V;
					memcpy(&msg.argv[0], &Page_1111.reserve1, sizeof(PAGE_1111_FRAME));
					if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
						msg.type = 0x1111;//充电界面---双枪的时候
				  }else{
						msg.type = 0x1122;//充电界面--单枪
					}
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_1111_FRAME), IPC_NOWAIT);
					//QT_timer_start(500);
    		}else  if((count_wait %5  == 0)&&(count_wait1 != 0)){//等待时间超时500mS
          count_wait1 = 0;
					Page_BMS_data.BMS_Info_falg = Globa_1->BMS_Info_falg;
				  Page_BMS_data.BMS_H_vol[0]=  Globa_1->BMS_H_voltage/100;
				  Page_BMS_data.BMS_H_vol[1]=  (Globa_1->BMS_H_voltage/100)>>8;
          Page_BMS_data.BMS_H_cur[0]=  Globa_1->BMS_H_current/100;
				  Page_BMS_data.BMS_H_cur[1]=  (Globa_1->BMS_H_current/100)>>8;
					Page_BMS_data.BMS_cell_LT[0]=  Globa_1->BMS_cell_LT;
				  Page_BMS_data.BMS_cell_LT[1]=  Globa_1->BMS_cell_LT>>8; 
					Page_BMS_data.BMS_cell_HT[0]=  Globa_1->BMS_cell_HT;
				  Page_BMS_data.BMS_cell_HT[1]=  Globa_1->BMS_cell_HT>>8;
	
					Page_BMS_data.need_voltage[0]=  Globa_1->need_voltage/100;
				  Page_BMS_data.need_voltage[1]=  (Globa_1->need_voltage/100)>>8;
					Page_BMS_data.need_current[0]=  (Globa_1->need_current/100);
				  Page_BMS_data.need_current[1]=  (Globa_1->need_current/100)>>8;
					
					memcpy(&Page_BMS_data.BMS_Vel[0],&Globa_1->BMS_Vel[0],sizeof(Page_BMS_data.BMS_Vel));
					
					Page_BMS_data.Battery_Rate_Vol[0]=  Globa_1->BMS_rate_voltage;
				  Page_BMS_data.Battery_Rate_Vol[1]=  (Globa_1->BMS_rate_voltage)>>8;
					Page_BMS_data.Battery_Rate_AH[0]=  Globa_1->BMS_rate_AH;
				  Page_BMS_data.Battery_Rate_AH[1]=  (Globa_1->BMS_rate_AH)>>8;
					Page_BMS_data.Battery_Rate_KWh[0]=  (Globa_1->Battery_Rate_KWh);
				  Page_BMS_data.Battery_Rate_KWh[1]=  (Globa_1->Battery_Rate_KWh)>>8;
				
 				  Page_BMS_data.Cell_H_Cha_Vol[0]=  (Globa_1->Cell_H_Cha_Vol);
				  Page_BMS_data.Cell_H_Cha_Vol[1]=  (Globa_1->Cell_H_Cha_Vol)>>8;
				  Page_BMS_data.Cell_H_Cha_Temp =  Globa_1->Cell_H_Cha_Temp;
				  Page_BMS_data.Charge_Mode =  Globa_1->Charge_Mode;
				  Page_BMS_data.Charge_Gun_Temp =  Globa_1->Charge_Gun_Temp;
				  Page_BMS_data.BMS_cell_HV_NO =  Globa_1->BMS_cell_HV_NO;
				  Page_BMS_data.BMS_cell_LT_NO =  Globa_1->BMS_cell_LT_NO;
				  Page_BMS_data.BMS_cell_HT_NO =  Globa_1->BMS_cell_HT_NO;
					memcpy(&Page_BMS_data.VIN[0],&Globa_1->VIN[0],sizeof(Page_BMS_data.VIN));
  				memcpy(&msg.argv[0], &Page_BMS_data.reserve1, sizeof(PAGE_BMS_FRAME));
				  if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
						msg.type = 0x1188;//充电界面---双枪的时候
				  }else{
						msg.type = 0x1588;//充电界面--单枪 0x1588
					}
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_BMS_FRAME), IPC_NOWAIT);
				}

				break;
			}
			case 0x23:{//---------------2      充电结束 结算界面      2.3-------------
				Globa_1->Charger_Over_Rmb_flag = 0;
        if(Globa_1->gun_link != 1 ){
					count_wait++;
					if(count_wait > 300){//等待时间超时60S
						memcpy(&busy_frame.car_sn[0], &Globa_1->VIN, sizeof(busy_frame.car_sn));
						if(id !=0){
							pthread_mutex_lock(&busy_db_pmutex);
							Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
							pthread_mutex_unlock(&busy_db_pmutex);
							usleep(20000);
						//插入到需要上传的交易记录数据库中
							pthread_mutex_lock(&busy_db_pmutex);
							all_id = 0;
							Insertflag = Insert_Charger_Busy_DB(&busy_frame, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
							if((Insertflag != -1)&&(all_id != 0)){//表示插入数据成功，其他的则需要重新插入
								Delete_Record_busy_DB(id);
								if(DEBUGLOG) RUN_GUN1_LogOut("一号枪交易数据,插入到正式数据库中并删除临时数据库记录 ID = %d",id);  
								id = 0;
							}else{
								if(DEBUGLOG) RUN_GUN1_LogOut("一号枪交易数据,插入到正式数据库中失败 ID = %d",id);  
							}
							pthread_mutex_unlock(&busy_db_pmutex);
							
						  if(id != 0){
								pthread_mutex_lock(&busy_db_pmutex);
								for(i=0;i<5;i++){
									usleep(20000);
									if(Select_NO_Over_Record_1(id,0)!= -1){//表示有没有上传的记录
										Select_NO_Over_count = 0;
										memcpy(&pre_old_busy_frame, &busy_frame, sizeof(pre_old_busy_frame));
										pre_old_id = id;
										Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
									}else{
										if(DEBUGLOG) RUN_GUN1_LogOut("%s %d","一号枪拔枪交易数据,更新upm=0x00成功 ID=",id);  
										Select_NO_Over_count = 0;
										pre_old_id = 0;	
										id = 0;
										break;
									}
								}
							  pthread_mutex_unlock(&busy_db_pmutex);
							}
						}
					
						if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
							Voicechip_operation_function(0x09);//09H     “谢谢惠顾，再见”
						}else{
							Voicechip_operation_function(0x11);//11H	谢谢使用再见.wav
						}
						Select_NO_Over_count = 0;
						count_wait = 0;
  			
            Globa_1->BMS_Power_V = 0;
						Globa_1->Operation_command = 2;
						
					  if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
							msg.type = 0x150;
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);						
							QT_timer_stop(500);
							Globa_1->QT_Step = 0x03; //自动充电分开的AB通道
						}else{//单枪
							//防止按键处理不同步，此处同步一下
							msg.type = 0x150;//
							msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa_2->QT_Step = 0x03; //主界面
							
							msg.type = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选着界面，才有此按钮功能）
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa_1->QT_Step = 0x02; //主界面
						}
						break;//超时必须退出
					}
				}else{
					count_wait = 0;
				}
				
				if(Globa_1->Start_Mode == VIN_START_TYPE)
				{
					#ifdef CHECK_BUTTON			
						if((msg.type == 0x1300)||(msg.type == 0x1310)){//收到该界面消息
							switch(msg.argv[0]){//判断 在当前界面下 用户操作的按钮
								case 0x01:{//1#通道选着按钮
								 Globa_1->checkout = 3;
								 break;
								}
							}
						}
					#else 
						sleep(2);
						Globa_1->checkout = 3;
					#endif					
				}
				if(Globa_1->checkout != 0){//1通道检测到充电卡 0：未知 1：灰锁 2：补充交易 3：卡片正常
      		msg.type = 0x95;//解除屏保
  				msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
          count_wait = 0;
    			if(Globa_1->checkout == 1){//交易处理 灰锁
    				//不动作，继续等待刷卡
    			}else if(Globa_1->checkout == 2){//交易处理 补充交易
						memcpy(&busy_frame.flag[0], "02", sizeof(busy_frame.flag));//"00" 补充交易
						pthread_mutex_lock(&busy_db_pmutex);
						if(Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00) == -1){
							usleep(10000);
							Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
						}
						pthread_mutex_unlock(&busy_db_pmutex);
    			}else if(Globa_1->checkout == 3){//交易处理 正常
						memcpy(&busy_frame.flag[0], "00", sizeof(busy_frame.flag));//"00" 成功
  					memcpy(&busy_frame.car_sn[0], &Globa_1->VIN, sizeof(busy_frame.car_sn));
						pthread_mutex_lock(&busy_db_pmutex);
						if(Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00) == -1){
							usleep(10000);
							Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
						}
						pthread_mutex_unlock(&busy_db_pmutex);

						//------------结算终于结束，退出结算界面--------------------------
	          if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
							Voicechip_operation_function(0x09);//09H     “谢谢惠顾，再见”
						}else{
							Voicechip_operation_function(0x11);//11H	谢谢使用再见.wav
						}
						
					//插入到需要上传的交易记录数据库中
						pthread_mutex_lock(&busy_db_pmutex);
						all_id = 0;
						Insertflag = Insert_Charger_Busy_DB(&busy_frame, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
						if((Insertflag != -1)&&(all_id != 0)){//表示插入数据成功，其他的则需要重新插入
							Delete_Record_busy_DB(id);
							if(DEBUGLOG) RUN_GUN1_LogOut("一号枪交易数据,插入到正式数据库中并删除临时数据库记录 ID = %d",id);  
							id = 0;
						}else{
							if(DEBUGLOG) RUN_GUN1_LogOut("一号枪交易数据,插入到正式数据库中失败 ID = %d",id);  
						}
						pthread_mutex_unlock(&busy_db_pmutex);

						if(id != 0){
							pthread_mutex_lock(&busy_db_pmutex);
							for(i=0;i<5;i++){
								if(Select_NO_Over_Record_1(id,0)!= -1){//表示有没有上传的记录
									Select_NO_Over_count = 0;
									memcpy(&pre_old_busy_frame, &busy_frame, sizeof(pre_old_busy_frame));
									pre_old_id = id;
									Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
								}else{
									if(DEBUGLOG) RUN_GUN1_LogOut("%s %d","一号枪交易数据,更新upm=0x00成功 ID=",id);  
									Select_NO_Over_count = 0;
									pre_old_id = 0;	
									id = 0;
									break;
								}
								sleep(1);
							}
							pthread_mutex_unlock(&busy_db_pmutex);
						}else{
							pre_old_id = 0;	
							id = 0;
						}
	
            Globa_1->BMS_Power_V = 0;
					  Select_NO_Over_count = 0;
						Globa_1->Operation_command = 2;//进行解锁
					  if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
							msg.type = 0x150;
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);						
							QT_timer_stop(500);
							Globa_1->QT_Step = 0x03; //自动充电分开的AB通道
						}else{//单枪
							//防止按键处理不同步，此处同步一下
							msg.type = 0x150;//
							msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa_2->QT_Step = 0x03; //主界面
							
							msg.type = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选着界面，才有此按钮功能）
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa_1->QT_Step = 0x02; //主界面
						}
    			}
    			Globa_1->checkout = 0;
    		}
				break;
			}
		  case 0x24:{//双枪对一辆车充电模式
        card_falg_count ++;
				if(card_falg_count > 20){
					Globa_1->pay_step = 3; //允许刷卡 --刷卡停止充电
				}
				Globa_1->meter_stop_KWH = (Globa_1->meter_KWH >= Globa_1->meter_stop_KWH)? Globa_1->meter_KWH:Globa_1->meter_stop_KWH;//取初始电表示数
				Globa_1->kwh = Globa_1->meter_stop_KWH - Globa_1->meter_start_KWH;
				
				Globa_2->meter_stop_KWH = Globa_2->meter_KWH;//取初始电表示数
				Globa_2->kwh = Globa_2->meter_stop_KWH - Globa_2->meter_start_KWH;
				
				g_ulcharged_kwh[pre_price_index] = Globa_1->kwh - ul_start_kwh;//累加当前时段的耗电量	
				g_ulcharged_kwh_2[pre_price_index] = Globa_2->kwh - ul_start_kwh_2;//累加当前时段的耗电量	

				cur_price_index = GetCurPriceIndex(); //获取当前时间
				if(cur_price_index != pre_price_index){
					ul_start_kwh = Globa_1->kwh - g_ulcharged_kwh[cur_price_index];//load new start kwh,需要减去时段之前累加的电量，否则该时段之前累加的电量丢失----20160312
					ul_start_kwh_2 = Globa_2->kwh - g_ulcharged_kwh_2[cur_price_index];//load new start kwh,需要减去时段之前累加的电量，否则该时段之前累加的电量丢失----20160312
					pre_price_index = cur_price_index;							
				}	
				
				for(i = 0;i<4;i++){
					g_ulcharged_rmb[i] = g_ulcharged_kwh[i]*share_time_kwh_price[i];
					g_ulcharged_rmb_2[i] = g_ulcharged_kwh_2[i]*share_time_kwh_price[i];
				}
				
				for(i = 0;i<4;i++){
					g_ulcharged_rmb[i] = (g_ulcharged_rmb[i] + COMPENSATION_VALUE)/100;
					g_ulcharged_rmb_2[i] = (g_ulcharged_rmb_2[i] + COMPENSATION_VALUE)/100;
				}
				
	      Globa_1->rmb = g_ulcharged_rmb[0] + g_ulcharged_rmb[1] + g_ulcharged_rmb[2] + g_ulcharged_rmb[3]; 
	      Globa_2->rmb = g_ulcharged_rmb_2[0] + g_ulcharged_rmb_2[1] + g_ulcharged_rmb_2[2] + g_ulcharged_rmb_2[3]; 
		  
				if(Serv_Type == 1){ //按次数收
					Globa_1->total_rmb = (Globa_1->rmb + Globa_1->Charger_param.Serv_Price);//已充电电量对应的金额
					Globa_2->total_rmb = Globa_1->rmb;//(Globa_2->rmb/100 + Globa_1->Charger_param.Serv_Price);//已充电电量对应的金额
				}else if(Serv_Type == 2){//按时间收
					Globa_1->total_rmb = (Globa_1->rmb + Globa_1->Time*Globa_1->Charger_param.Serv_Price/600);//已充电电量对应的金额
					Globa_2->total_rmb = Globa_1->rmb;//(Globa_1->rmb/100 + Globa_1->Time*Globa_1->Charger_param.Serv_Price/600);//已充电电量对应的金额
				}else{
				  //Globa_1->total_rmb = (Globa_1->rmb + Globa_1->kwh*Globa_1->Charger_param.Serv_Price/100);//已充电电量对应的金额
					//Globa_2->total_rmb = (Globa_2->rmb + Globa_2->kwh*Globa_1->Charger_param.Serv_Price/100);//已充电电量对应的金额
					for(i = 0;i<4;i++){
						g_ulcharged_rmb_Serv[i] = g_ulcharged_kwh[i]*share_time_kwh_price_serve[i];
						g_ulcharged_rmb_Serv_2[i] = g_ulcharged_kwh_2[i]*share_time_kwh_price_serve[i];
					}
					
					for(i = 0;i<4;i++){
						g_ulcharged_rmb_Serv[i] = (g_ulcharged_rmb_Serv[i] + COMPENSATION_VALUE)/100;
						g_ulcharged_rmb_Serv_2[i] = (g_ulcharged_rmb_Serv_2[i]+ COMPENSATION_VALUE)/100;
					}
					Globa_1->total_rmb = Globa_1->rmb + g_ulcharged_rmb_Serv[0] + g_ulcharged_rmb_Serv[1] + g_ulcharged_rmb_Serv[2] + g_ulcharged_rmb_Serv[3]; 
					Globa_2->total_rmb = Globa_2->rmb + g_ulcharged_rmb_Serv_2[0] + g_ulcharged_rmb_Serv_2[1] + g_ulcharged_rmb_Serv_2[2] + g_ulcharged_rmb_Serv_2[3]; 
				}
			
 
				Globa_1->ISO_8583_rmb_Serv = Globa_1->total_rmb + Globa_2->total_rmb - Globa_1->rmb - Globa_2->rmb;               //总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
	      Globa_1->ISO_8583_rmb = Globa_1->rmb + Globa_2->rmb; //电量消费金额 0.01yuan
	
				if((Globa_1->kwh - temp_kwh) > 70){//每隔0.5度电左右保存一次数据;
					temp_kwh = Globa_1->kwh;

					sprintf(&temp_char[0], "%08d", Globa_1->meter_stop_KWH);				
					memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

					sprintf(&temp_char[0], "%08d", Globa_1->kwh);				
					memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
					
					sprintf(&temp_char[0], "%08d", Globa_1->rmb);				
					memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));
	        if(Globa_1->kwh != 0){
	          sprintf(&temp_char[0], "%08d", (Globa_1->rmb*100/Globa_1->kwh));  
	        }else{
	          sprintf(&temp_char[0], "%08d", 0);  
	        }
					memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));
					
					sprintf(&temp_char[0], "%08d", Globa_1->total_rmb);				
					memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));

					sprintf(&temp_char[0], "%08d", Globa_1->total_rmb-Globa_1->rmb);				
					memcpy(&busy_frame.server_rmb[0], &temp_char[0], sizeof(busy_frame.server_rmb));

					systime = time(NULL);         //获得系统的当前时间 
					localtime_r(&systime, &tm_t);  //结构指针指向当前时间	       				
					sprintf(&temp_char[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
			                             tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
					memcpy(&busy_frame.end_time[0], &temp_char[0], sizeof(busy_frame.end_time));
					tmp_value = g_ulcharged_rmb[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.sharp_allrmb[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb));

					tmp_value = g_ulcharged_rmb[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame.peak_allrmb[0], &temp_char[0], sizeof(busy_frame.peak_allrmb));
	
					tmp_value = g_ulcharged_rmb[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame.flat_allrmb[0], &temp_char[0], sizeof(busy_frame.flat_allrmb));
				
					tmp_value = g_ulcharged_rmb[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.valley_allrmb[0], &temp_char[0], sizeof(busy_frame.valley_allrmb));
					
					tmp_value = g_ulcharged_rmb_Serv[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.sharp_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb_Serv));

					tmp_value = g_ulcharged_rmb_Serv[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame.peak_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.peak_allrmb_Serv));
	
					tmp_value = g_ulcharged_rmb_Serv[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame.flat_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.flat_allrmb_Serv));
				
					tmp_value = g_ulcharged_rmb_Serv[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.valley_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.valley_allrmb_Serv));
					
					tmp_value =0;
					pthread_mutex_lock(&busy_db_pmutex);
					Update_Charger_Busy_DB_1(id, &busy_frame, 1, 0x55);
					pthread_mutex_unlock(&busy_db_pmutex);
				}else if((Globa_2->kwh - temp_kwh_2) > 80){//每隔0.8度电左右保存一次数据;
					temp_kwh_2 = Globa_2->kwh;
					
					sprintf(&temp_char[0], "%08d", Globa_2->meter_stop_KWH);				
					memcpy(&busy_frame_2.end_kwh[0], &temp_char[0], sizeof(busy_frame_2.end_kwh));

					sprintf(&temp_char[0], "%08d", Globa_2->kwh);				
					memcpy(&busy_frame_2.kwh[0], &temp_char[0], sizeof(busy_frame_2.kwh));
					
					sprintf(&temp_char[0], "%08d", Globa_2->rmb);				
					memcpy(&busy_frame_2.rmb[0], &temp_char[0], sizeof(busy_frame_2.rmb));
	        if(Globa_2->kwh != 0){
	          sprintf(&temp_char[0], "%08d", (Globa_2->rmb*100/Globa_2->kwh));  
	        }else{
	          sprintf(&temp_char[0], "%08d", 0);  
	        }
					memcpy(&busy_frame_2.rmb_kwh[0], &temp_char[0], sizeof(busy_frame_2.rmb_kwh));
					
					sprintf(&temp_char[0], "%08d", Globa_2->total_rmb);				
					memcpy(&busy_frame_2.total_rmb[0], &temp_char[0], sizeof(busy_frame_2.total_rmb));

					sprintf(&temp_char[0], "%08d", Globa_2->total_rmb-Globa_2->rmb);				
					memcpy(&busy_frame_2.server_rmb[0], &temp_char[0], sizeof(busy_frame_2.server_rmb));

					systime = time(NULL);         //获得系统的当前时间 
					localtime_r(&systime, &tm_t);  //结构指针指向当前时间	       				
					sprintf(&temp_char[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
			                             tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
					memcpy(&busy_frame_2.end_time[0], &temp_char[0], sizeof(busy_frame_2.end_time));
					tmp_value = g_ulcharged_rmb_2[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame_2.sharp_allrmb[0], &temp_char[0], sizeof(busy_frame_2.sharp_allrmb));

					tmp_value = g_ulcharged_rmb_2[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame_2.peak_allrmb[0], &temp_char[0], sizeof(busy_frame_2.peak_allrmb));
	
					tmp_value = g_ulcharged_rmb_2[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame_2.flat_allrmb[0], &temp_char[0], sizeof(busy_frame_2.flat_allrmb));
				
					tmp_value = g_ulcharged_rmb_2[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame_2.valley_allrmb[0], &temp_char[0], sizeof(busy_frame_2.valley_allrmb));
					
					tmp_value = g_ulcharged_rmb_Serv_2[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame_2.sharp_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame_2.sharp_allrmb_Serv));

					tmp_value = g_ulcharged_rmb_Serv_2[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame_2.peak_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame_2.peak_allrmb_Serv));
	
					tmp_value = g_ulcharged_rmb_Serv_2[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame_2.flat_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame_2.flat_allrmb_Serv));
				
					tmp_value = g_ulcharged_rmb_Serv_2[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame_2.valley_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame_2.valley_allrmb_Serv));
					
					tmp_value =0;
					pthread_mutex_lock(&busy_db_pmutex);
					Update_Charger_Busy_DB_2(id_NO_2, &busy_frame_2, 2, 0x55);
					pthread_mutex_unlock(&busy_db_pmutex);
				}

				//------------------ 刷卡停止充电 --------------------------------------
				if(((Globa_1->checkout == 3)&&(Globa_1->card_type == 2))|| \
					 ((Globa_1->checkout == 1)&&(Globa_1->card_type == 1))||(Globa_1->Electric_pile_workstart_Enable == 1)||((Globa_1->Start_Mode == VIN_START_TYPE)&&(msg.argv[0] == 1)&&(msg.type == 0x1133))){//用户卡灰锁 或正常员工卡
				  RunEventLogOut_2("计费QT判断用户主动停止充电-双枪对一辆车模式");

     			Globa_1->Manu_start = 0;
          sleep(3);
					Globa_1->meter_stop_KWH = Globa_1->meter_KWH;//取初始电表示数
					Globa_1->kwh = Globa_1->meter_stop_KWH - Globa_1->meter_start_KWH;
					
					Globa_2->meter_stop_KWH = Globa_2->meter_KWH;//取初始电表示数
					Globa_2->kwh = Globa_2->meter_stop_KWH - Globa_2->meter_start_KWH;
					
					g_ulcharged_kwh[pre_price_index] = Globa_1->kwh - ul_start_kwh;//累加当前时段的耗电量	
					g_ulcharged_kwh_2[pre_price_index] = Globa_2->kwh - ul_start_kwh_2;//累加当前时段的耗电量	

					cur_price_index = GetCurPriceIndex(); //获取当前时间
					if(cur_price_index != pre_price_index){
						ul_start_kwh = Globa_1->kwh - g_ulcharged_kwh[cur_price_index];//load new start kwh,需要减去时段之前累加的电量，否则该时段之前累加的电量丢失----20160312
						ul_start_kwh_2 = Globa_2->kwh - g_ulcharged_kwh_2[cur_price_index];//load new start kwh,需要减去时段之前累加的电量，否则该时段之前累加的电量丢失----20160312
						pre_price_index = cur_price_index;							
					}	
					
					for(i = 0;i<4;i++){
						g_ulcharged_rmb[i] = g_ulcharged_kwh[i]*share_time_kwh_price[i];
						g_ulcharged_rmb_2[i] = g_ulcharged_kwh_2[i]*share_time_kwh_price[i];
					}
					
					for(i = 0;i<4;i++){
						g_ulcharged_rmb[i] = (g_ulcharged_rmb[i] + COMPENSATION_VALUE)/100;
						g_ulcharged_rmb_2[i] = (g_ulcharged_rmb_2[i] + COMPENSATION_VALUE)/100;
					}
					
					Globa_1->rmb = g_ulcharged_rmb[0] + g_ulcharged_rmb[1] + g_ulcharged_rmb[2] + g_ulcharged_rmb[3]; 
					Globa_2->rmb = g_ulcharged_rmb_2[0] + g_ulcharged_rmb_2[1] + g_ulcharged_rmb_2[2] + g_ulcharged_rmb_2[3]; 
				
					if(Serv_Type == 1){ //按次数收
						Globa_1->total_rmb = (Globa_1->rmb + Globa_1->Charger_param.Serv_Price);//已充电电量对应的金额
						Globa_2->total_rmb = Globa_2->rmb;//(Globa_2->rmb/100 + Globa_1->Charger_param.Serv_Price);//已充电电量对应的金额
					}else if(Serv_Type == 2){//按时间收
						Globa_1->total_rmb = (Globa_1->rmb + Globa_1->Time*Globa_1->Charger_param.Serv_Price/600);//已充电电量对应的金额
						Globa_2->total_rmb = Globa_2->rmb;//(Globa_1->rmb/100 + Globa_1->Time*Globa_1->Charger_param.Serv_Price/600);//已充电电量对应的金额
					}else{
						//Globa_1->total_rmb = (Globa_1->rmb + Globa_1->kwh*Globa_1->Charger_param.Serv_Price/100);//已充电电量对应的金额
						//Globa_2->total_rmb = (Globa_2->rmb + Globa_2->kwh*Globa_1->Charger_param.Serv_Price/100);//已充电电量对应的金额
						for(i = 0;i<4;i++){
							g_ulcharged_rmb_Serv[i] = g_ulcharged_kwh[i]*share_time_kwh_price_serve[i];
							g_ulcharged_rmb_Serv_2[i] = g_ulcharged_kwh_2[i]*share_time_kwh_price_serve[i];
						}
						
						for(i = 0;i<4;i++){
							g_ulcharged_rmb_Serv[i] = (g_ulcharged_rmb_Serv[i] + COMPENSATION_VALUE)/100;
							g_ulcharged_rmb_Serv_2[i] = (g_ulcharged_rmb_Serv_2[i]+ COMPENSATION_VALUE)/100;
						}
						Globa_1->total_rmb = Globa_1->rmb + g_ulcharged_rmb_Serv[0] + g_ulcharged_rmb_Serv[1] + g_ulcharged_rmb_Serv[2] + g_ulcharged_rmb_Serv[3]; 
						Globa_2->total_rmb = Globa_2->rmb + g_ulcharged_rmb_Serv_2[0] + g_ulcharged_rmb_Serv_2[1] + g_ulcharged_rmb_Serv_2[2] + g_ulcharged_rmb_Serv_2[3]; 
					}

					Globa_1->ISO_8583_rmb_Serv = Globa_1->total_rmb + Globa_2->total_rmb - Globa_1->rmb - Globa_2->rmb;               //总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
					Globa_1->ISO_8583_rmb = Globa_1->rmb + Globa_2->rmb; //电量消费金额 0.01yuan
		
					//-------------------扣费 更新业务进度数据----------------------------
					sprintf(&temp_char[0], "%08d", Globa_1->meter_stop_KWH);				
					memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

					sprintf(&temp_char[0], "%08d", Globa_1->kwh);				
					memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
					
					sprintf(&temp_char[0], "%08d", Globa_1->rmb);				
					memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));
					if(Globa_1->kwh != 0){
						sprintf(&temp_char[0], "%08d", (Globa_1->rmb*100/Globa_1->kwh));  
					}else{
						sprintf(&temp_char[0], "%08d", 0);  
					}
					memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));
					
					sprintf(&temp_char[0], "%08d", Globa_1->total_rmb);				
					memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));

					sprintf(&temp_char[0], "%08d", Globa_1->total_rmb-Globa_1->rmb);				
					memcpy(&busy_frame.server_rmb[0], &temp_char[0], sizeof(busy_frame.server_rmb));

					systime = time(NULL);         //获得系统的当前时间 
					localtime_r(&systime, &tm_t);  //结构指针指向当前时间	       				
					sprintf(&temp_char[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
																	 tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
					memcpy(&busy_frame.end_time[0], &temp_char[0], sizeof(busy_frame.end_time));

					tmp_value = g_ulcharged_rmb[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.sharp_allrmb[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb));

					tmp_value = g_ulcharged_rmb[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame.peak_allrmb[0], &temp_char[0], sizeof(busy_frame.peak_allrmb));
					
					tmp_value = g_ulcharged_rmb[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame.flat_allrmb[0], &temp_char[0], sizeof(busy_frame.flat_allrmb));
				
					tmp_value = g_ulcharged_rmb[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.valley_allrmb[0], &temp_char[0], sizeof(busy_frame.valley_allrmb));
					
					tmp_value = g_ulcharged_rmb_Serv[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.sharp_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb_Serv));

					tmp_value = g_ulcharged_rmb_Serv[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame.peak_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.peak_allrmb_Serv));

					tmp_value = g_ulcharged_rmb_Serv[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame.flat_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.flat_allrmb_Serv));
				
					tmp_value = g_ulcharged_rmb_Serv[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.valley_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.valley_allrmb_Serv));
					
					tmp_value =0;
					Page_1300.cause = 11;//直接手动结束的 Globa_1->charger_over_flag;
					sprintf(&temp_char[0], "%02d", Page_1300.cause);		
					memcpy(&busy_frame.End_code[0], &temp_char[0], sizeof(busy_frame.End_code));//结束原因代码
					
					pthread_mutex_lock(&busy_db_pmutex);
					Update_Charger_Busy_DB_1(id, &busy_frame, 1, 0x66);
					pthread_mutex_unlock(&busy_db_pmutex);
					
					sprintf(&temp_char[0], "%08d", Globa_2->meter_stop_KWH);				
					memcpy(&busy_frame_2.end_kwh[0], &temp_char[0], sizeof(busy_frame_2.end_kwh));

					sprintf(&temp_char[0], "%08d", Globa_2->kwh);				
					memcpy(&busy_frame_2.kwh[0], &temp_char[0], sizeof(busy_frame_2.kwh));
					
					sprintf(&temp_char[0], "%08d", Globa_2->rmb);				
					memcpy(&busy_frame_2.rmb[0], &temp_char[0], sizeof(busy_frame_2.rmb));
	        if(Globa_2->kwh != 0){
	          sprintf(&temp_char[0], "%08d", (Globa_2->rmb*100/Globa_2->kwh));  
	        }else{
	          sprintf(&temp_char[0], "%08d", 0);  
	        }
					memcpy(&busy_frame_2.rmb_kwh[0], &temp_char[0], sizeof(busy_frame_2.rmb_kwh));
					
					sprintf(&temp_char[0], "%08d", Globa_2->total_rmb);				
					memcpy(&busy_frame_2.total_rmb[0], &temp_char[0], sizeof(busy_frame_2.total_rmb));

					sprintf(&temp_char[0], "%08d", Globa_2->total_rmb-Globa_2->rmb);				
					memcpy(&busy_frame_2.server_rmb[0], &temp_char[0], sizeof(busy_frame_2.server_rmb));

					systime = time(NULL);         //获得系统的当前时间 
					localtime_r(&systime, &tm_t);  //结构指针指向当前时间	       				
					sprintf(&temp_char[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
			                             tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
					memcpy(&busy_frame_2.end_time[0], &temp_char[0], sizeof(busy_frame_2.end_time));
					tmp_value = g_ulcharged_rmb_2[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame_2.sharp_allrmb[0], &temp_char[0], sizeof(busy_frame_2.sharp_allrmb));

					tmp_value = g_ulcharged_rmb_2[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame_2.peak_allrmb[0], &temp_char[0], sizeof(busy_frame_2.peak_allrmb));
	
					tmp_value = g_ulcharged_rmb_2[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame_2.flat_allrmb[0], &temp_char[0], sizeof(busy_frame_2.flat_allrmb));
				
					tmp_value = g_ulcharged_rmb_2[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame_2.valley_allrmb[0], &temp_char[0], sizeof(busy_frame_2.valley_allrmb));
					
					tmp_value = g_ulcharged_rmb_Serv_2[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame_2.sharp_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame_2.sharp_allrmb_Serv));

					tmp_value = g_ulcharged_rmb_Serv_2[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame_2.peak_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame_2.peak_allrmb_Serv));
	
					tmp_value = g_ulcharged_rmb_Serv_2[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame_2.flat_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame_2.flat_allrmb_Serv));
				
					tmp_value = g_ulcharged_rmb_Serv_2[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame_2.valley_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame_2.valley_allrmb_Serv));
					tmp_value =0;
					pthread_mutex_lock(&busy_db_pmutex);
					Update_Charger_Busy_DB_2(id_NO_2, &busy_frame_2, 2, 0x66);
					pthread_mutex_unlock(&busy_db_pmutex);
	
					//-------------------界面显示数据内容---------------------------------
					Page_1300.cause = 11;//直接手动结束的 Globa_1->charger_over_flag;
					Page_1300.Capact = Globa_1->BMS_batt_SOC;
					Page_1300.elapsed_time[0] =  Globa_1->Time;
					Page_1300.elapsed_time[1] =  Globa_1->Time>>8;
					Page_1300.KWH[0] =  (Globa_1->kwh + Globa_2->kwh);
					Page_1300.KWH[1] =  (Globa_1->kwh + Globa_2->kwh)>>8;
					Page_1300.RMB[0] =  (Globa_1->total_rmb+Globa_2->total_rmb);
					Page_1300.RMB[1] =  (Globa_1->total_rmb+Globa_2->total_rmb)>>8;
					Page_1300.RMB[2] =  (Globa_1->total_rmb+Globa_2->total_rmb)>>16;
					Page_1300.RMB[3] =  (Globa_1->total_rmb+Globa_2->total_rmb)>>24;
					memcpy(&Page_1300.SN[0], &Globa_1->card_sn[0], sizeof(Page_1300.SN));
					if(Globa_1->Start_Mode == VIN_START_TYPE)
				  {	
				    #ifdef CHECK_BUTTON			
							Page_1300.APP = 3;   //充电启动方式 1:刷卡  2：手机APP	
						#else 
							Page_1300.APP = 1;
						#endif							
					}else{
						Page_1300.APP = 1;   //充电启动方式 1:刷卡  2：手机APP 
					}
					memcpy(&msg.argv[0], &Page_1300.reserve1, sizeof(PAGE_1300_FRAME));
					msg.type = 0x1310;//单枪结束界面
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_1300_FRAME), IPC_NOWAIT);
          Globa_1->total_rmb = Globa_1->total_rmb + Globa_2->total_rmb;//总扣费情况

					Globa_1->QT_Step = 0x25; // 2      充电结束 扣费界面 2.3----------------
					Globa_1->charger_state = 0x05;
				  Globa_2->charger_state = 0x05;
					Globa_1->checkout = 0;
					if(Globa_1->Start_Mode == VIN_START_TYPE)
				  {	
					  Globa_1->pay_step = 0;//1通道在等待刷卡扣费
					}else{
					  Globa_1->pay_step = 4;//1通道在等待刷卡扣费
					}
          Globa_1->Charger_Over_Rmb_flag = 0;
					count_wait = 0;
					break;
				}else if(Globa_1->checkout != 0){//无用的检测结果清零
					Globa_1->checkout = 0;
				}

    		if(Globa_1->charger_start == 0){//充电彻底结束处理
					Globa_1->Manu_start = 0;
          sleep(3);
	
					Globa_1->meter_stop_KWH = (Globa_1->meter_KWH >= Globa_1->meter_stop_KWH)? Globa_1->meter_KWH:Globa_1->meter_stop_KWH;//取初始电表示数
				  Globa_1->kwh = Globa_1->meter_stop_KWH - Globa_1->meter_start_KWH;
				
				  Globa_2->meter_stop_KWH = Globa_2->meter_KWH;//取初始电表示数
				  Globa_2->kwh = Globa_2->meter_stop_KWH - Globa_2->meter_start_KWH;
				
				  g_ulcharged_kwh[pre_price_index] = Globa_1->kwh - ul_start_kwh;//累加当前时段的耗电量	
				  g_ulcharged_kwh_2[pre_price_index] = Globa_2->kwh - ul_start_kwh_2;//累加当前时段的耗电量	

					cur_price_index = GetCurPriceIndex(); //获取当前时间
					if(cur_price_index != pre_price_index){
						ul_start_kwh = Globa_1->kwh - g_ulcharged_kwh[cur_price_index];//load new start kwh,需要减去时段之前累加的电量，否则该时段之前累加的电量丢失----20160312
						ul_start_kwh_2 = Globa_2->kwh - g_ulcharged_kwh_2[cur_price_index];//load new start kwh,需要减去时段之前累加的电量，否则该时段之前累加的电量丢失----20160312
						pre_price_index = cur_price_index;							
					}	
					
					for(i = 0;i<4;i++){
						g_ulcharged_rmb[i] = g_ulcharged_kwh[i]*share_time_kwh_price[i];
						g_ulcharged_rmb_2[i] = g_ulcharged_kwh_2[i]*share_time_kwh_price[i];
					}
					for(i = 0;i<4;i++){
						g_ulcharged_rmb[i] = (g_ulcharged_rmb[i] + COMPENSATION_VALUE)/100;
						g_ulcharged_rmb_2[i] =(g_ulcharged_rmb_2[i] + COMPENSATION_VALUE)/100;
					}
					Globa_1->rmb = g_ulcharged_rmb[0] + g_ulcharged_rmb[1] + g_ulcharged_rmb[2] + g_ulcharged_rmb[3]; 
					Globa_2->rmb = g_ulcharged_rmb_2[0] + g_ulcharged_rmb_2[1] + g_ulcharged_rmb_2[2] + g_ulcharged_rmb_2[3]; 
				
					if(Serv_Type == 1){ //按次数收
						Globa_1->total_rmb = (Globa_1->rmb + Globa_1->Charger_param.Serv_Price);//已充电电量对应的金额
						Globa_2->total_rmb = Globa_2->rmb;//(Globa_2->rmb/100 + Globa_1->Charger_param.Serv_Price);//已充电电量对应的金额
					}else if(Serv_Type == 2){//按时间收
						Globa_1->total_rmb = (Globa_1->rmb + Globa_1->Time*Globa_1->Charger_param.Serv_Price/600);//已充电电量对应的金额
						Globa_2->total_rmb = Globa_2->rmb;//(Globa_1->rmb/100 + Globa_1->Time*Globa_1->Charger_param.Serv_Price/600);//已充电电量对应的金额
					}else{
					//	Globa_1->total_rmb = (Globa_1->rmb + Globa_1->kwh*Globa_1->Charger_param.Serv_Price/100);//已充电电量对应的金额
						//Globa_2->total_rmb = (Globa_2->rmb + Globa_2->kwh*Globa_1->Charger_param.Serv_Price/100);//已充电电量对应的金额
						for(i = 0;i<4;i++){
							g_ulcharged_rmb_Serv[i] = g_ulcharged_kwh[i]*share_time_kwh_price_serve[i];
							g_ulcharged_rmb_Serv_2[i] = g_ulcharged_kwh_2[i]*share_time_kwh_price_serve[i];
						}
						for(i = 0;i<4;i++){
							g_ulcharged_rmb_Serv[i] = g_ulcharged_rmb_Serv[i]/100;
							g_ulcharged_rmb_Serv_2[i] = g_ulcharged_rmb_Serv_2[i]/100;
						}
						Globa_1->total_rmb = Globa_1->rmb + g_ulcharged_rmb_Serv[0] + g_ulcharged_rmb_Serv[1] + g_ulcharged_rmb_Serv[2] + g_ulcharged_rmb_Serv[3]; 
						Globa_2->total_rmb = Globa_2->rmb + g_ulcharged_rmb_Serv_2[0] + g_ulcharged_rmb_Serv_2[1] + g_ulcharged_rmb_Serv_2[2] + g_ulcharged_rmb_Serv_2[3]; 
					}
					
					Globa_1->ISO_8583_rmb_Serv = Globa_1->total_rmb + Globa_2->total_rmb - Globa_1->rmb - Globa_2->rmb;               //总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
					Globa_1->ISO_8583_rmb = Globa_1->rmb + Globa_2->rmb; //电量消费金额 0.01yuan
		
					//-------------------扣费 更新业务进度数据----------------------------
					sprintf(&temp_char[0], "%08d", Globa_1->meter_stop_KWH);				
					memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

					sprintf(&temp_char[0], "%08d", Globa_1->kwh);				
					memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
					
					sprintf(&temp_char[0], "%08d", Globa_1->rmb);				
					memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));
					if(Globa_1->kwh != 0){
						sprintf(&temp_char[0], "%08d", (Globa_1->rmb*100/Globa_1->kwh));  
					}else{
						sprintf(&temp_char[0], "%08d", 0);  
					}
					memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));
					
					sprintf(&temp_char[0], "%08d", Globa_1->total_rmb);				
					memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));

					sprintf(&temp_char[0], "%08d", Globa_1->total_rmb-Globa_1->rmb);				
					memcpy(&busy_frame.server_rmb[0], &temp_char[0], sizeof(busy_frame.server_rmb));

					systime = time(NULL);         //获得系统的当前时间 
					localtime_r(&systime, &tm_t);  //结构指针指向当前时间	       				
					sprintf(&temp_char[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
																	 tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
					memcpy(&busy_frame.end_time[0], &temp_char[0], sizeof(busy_frame.end_time));

					tmp_value = g_ulcharged_rmb[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.sharp_allrmb[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb));

					tmp_value = g_ulcharged_rmb[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame.peak_allrmb[0], &temp_char[0], sizeof(busy_frame.peak_allrmb));
					
					tmp_value = g_ulcharged_rmb[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame.flat_allrmb[0], &temp_char[0], sizeof(busy_frame.flat_allrmb));
				
					tmp_value = g_ulcharged_rmb[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.valley_allrmb[0], &temp_char[0], sizeof(busy_frame.valley_allrmb));
					
					tmp_value = g_ulcharged_rmb_Serv[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.sharp_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.sharp_allrmb_Serv));

					tmp_value = g_ulcharged_rmb_Serv[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame.peak_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.peak_allrmb_Serv));

					tmp_value = g_ulcharged_rmb_Serv[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame.flat_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.flat_allrmb_Serv));
				
					tmp_value = g_ulcharged_rmb_Serv[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame.valley_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame.valley_allrmb_Serv));
					
					tmp_value =0;
					if(Globa_1->Charger_Over_Rmb_flag == 1){
						Page_1300.cause = 13; 
						sent_warning_message(0x94, Page_1300.cause, 1, 0);
					}else if(Globa_1->Charger_Over_Rmb_flag == 3){
						Page_1300.cause = 19; 
						sent_warning_message(0x94, Page_1300.cause, 1, 0);
					}else if(Globa_1->Charger_Over_Rmb_flag == 4){
						Page_1300.cause = 62; 
						sent_warning_message(0x94, Page_1300.cause, 1, 0);
					}else if (Globa_1->Charger_Over_Rmb_flag == 5){
						Page_1300.cause = 65;
						sent_warning_message(0x94, Page_1300.cause, 1, 0);
					}else if (Globa_1->Charger_Over_Rmb_flag == 6){//达到SOC限制条件
						Page_1300.cause = 9;
						sent_warning_message(0x94, Page_1300.cause, 1, 0);
					}else if(Globa_1->Charger_Over_Rmb_flag == 7){//读取到的电表电量异常
						Page_1300.cause = 8;
						sent_warning_message(0x94, Page_1300.cause, 1, 0);							
					}else if(Globa_1->Charger_Over_Rmb_flag == 8){//充电电流超过SOC能允许充电的值
						Page_1300.cause = 7;
						sent_warning_message(0x94, Page_1300.cause, 1, 0);							
					}else{
						Page_1300.cause = Globa_1->charger_over_flag; 
					}
					RunEventLogOut_1("计费QT判断充电结束:Page_1300.cause =%d,Globa_1->charger_over_flag =%d ,Charger_Over_Rmb_flag =%d ",Page_1300.cause,Globa_1->charger_over_flag,Globa_1->Charger_Over_Rmb_flag);

					sprintf(&temp_char[0], "%02d", Page_1300.cause);		
					memcpy(&busy_frame.End_code[0], &temp_char[0], sizeof(busy_frame.End_code));//结束原因代码
					
					tmp_value =0;
					pthread_mutex_lock(&busy_db_pmutex);
					Update_Charger_Busy_DB_1(id, &busy_frame, 1, 0x66);
					pthread_mutex_unlock(&busy_db_pmutex);
					
					sprintf(&temp_char[0], "%08d", Globa_2->meter_stop_KWH);				
					memcpy(&busy_frame_2.end_kwh[0], &temp_char[0], sizeof(busy_frame_2.end_kwh));

					sprintf(&temp_char[0], "%08d", Globa_2->kwh);				
					memcpy(&busy_frame_2.kwh[0], &temp_char[0], sizeof(busy_frame_2.kwh));
					
					sprintf(&temp_char[0], "%08d", Globa_2->rmb);				
					memcpy(&busy_frame_2.rmb[0], &temp_char[0], sizeof(busy_frame_2.rmb));
					if(Globa_2->kwh != 0){
						sprintf(&temp_char[0], "%08d", (Globa_2->rmb*100/Globa_2->kwh));  
					}else{
						sprintf(&temp_char[0], "%08d", 0);  
					}
					memcpy(&busy_frame_2.rmb_kwh[0], &temp_char[0], sizeof(busy_frame_2.rmb_kwh));
					
					sprintf(&temp_char[0], "%08d", Globa_2->total_rmb);				
					memcpy(&busy_frame_2.total_rmb[0], &temp_char[0], sizeof(busy_frame_2.total_rmb));

					sprintf(&temp_char[0], "%08d", Globa_2->total_rmb-Globa_2->rmb);				
					memcpy(&busy_frame_2.server_rmb[0], &temp_char[0], sizeof(busy_frame_2.server_rmb));

					systime = time(NULL);         //获得系统的当前时间 
					localtime_r(&systime, &tm_t);  //结构指针指向当前时间	       				
					sprintf(&temp_char[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
																	 tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
					memcpy(&busy_frame_2.end_time[0], &temp_char[0], sizeof(busy_frame_2.end_time));
					tmp_value = g_ulcharged_rmb_2[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame_2.sharp_allrmb[0], &temp_char[0], sizeof(busy_frame_2.sharp_allrmb));

					tmp_value = g_ulcharged_rmb_2[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame_2.peak_allrmb[0], &temp_char[0], sizeof(busy_frame_2.peak_allrmb));
	
					tmp_value = g_ulcharged_rmb_2[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame_2.flat_allrmb[0], &temp_char[0], sizeof(busy_frame_2.flat_allrmb));
				
					tmp_value = g_ulcharged_rmb_2[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame_2.valley_allrmb[0], &temp_char[0], sizeof(busy_frame_2.valley_allrmb));
					
					tmp_value = g_ulcharged_rmb_Serv_2[0];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame_2.sharp_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame_2.sharp_allrmb_Serv));

					tmp_value = g_ulcharged_rmb_Serv_2[1];
					sprintf(&temp_char[0], "%08d", tmp_value);
					memcpy(&busy_frame_2.peak_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame_2.peak_allrmb_Serv));
	
					tmp_value = g_ulcharged_rmb_Serv_2[2];
					sprintf(&temp_char[0], "%08d", tmp_value);		
					memcpy(&busy_frame_2.flat_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame_2.flat_allrmb_Serv));
				
					tmp_value = g_ulcharged_rmb_Serv_2[3];
					sprintf(&temp_char[0], "%08d", tmp_value);	
					memcpy(&busy_frame_2.valley_allrmb_Serv[0], &temp_char[0], sizeof(busy_frame_2.valley_allrmb_Serv));
					tmp_value =0;
					memcpy(&busy_frame_2.End_code[0], &busy_frame.End_code[0], sizeof(busy_frame.End_code));//结束原因代码

					pthread_mutex_lock(&busy_db_pmutex);
					Update_Charger_Busy_DB_2(id_NO_2, &busy_frame_2, 2, 0x66);
					pthread_mutex_unlock(&busy_db_pmutex);
					
          Globa_1->Charger_Over_Rmb_flag = 0;
					Page_1300.Capact = Globa_1->BMS_batt_SOC;
					Page_1300.elapsed_time[0] =  Globa_1->Time;
					Page_1300.elapsed_time[1] =  Globa_1->Time>>8;
					Page_1300.KWH[0] =  (Globa_1->kwh + Globa_2->kwh);
					Page_1300.KWH[1] =  (Globa_1->kwh + Globa_2->kwh)>>8;
					Page_1300.RMB[0] =  (Globa_1->total_rmb+Globa_2->total_rmb);
					Page_1300.RMB[1] =  (Globa_1->total_rmb+Globa_2->total_rmb)>>8;
					Page_1300.RMB[2] =  (Globa_1->total_rmb+Globa_2->total_rmb)>>16;
					Page_1300.RMB[3] =  (Globa_1->total_rmb+Globa_2->total_rmb)>>24;
					memcpy(&Page_1300.SN[0], &Globa_1->card_sn[0], sizeof(Page_1300.SN));
					
					if(Globa_1->Start_Mode == VIN_START_TYPE)
				  {	
						#ifdef CHECK_BUTTON			
							Page_1300.APP = 3;   //充电启动方式 1:刷卡  2：手机APP	
						#else 
							Page_1300.APP  = 1;
						#endif							
            Globa_1->pay_step = 0;//1通道在等待刷卡扣费
					}else{
				  	Page_1300.APP = 1;   //充电启动方式 1:刷卡  2：手机APP
						Globa_1->pay_step = 4;//1通道在等待刷卡扣费
					}

					memcpy(&msg.argv[0], &Page_1300.reserve1, sizeof(PAGE_1300_FRAME));
				
					msg.type = 0x1310;//单枪结束界面

					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_1300_FRAME), IPC_NOWAIT);
          Globa_1->total_rmb = Globa_1->total_rmb + Globa_2->total_rmb;//总扣费情况
					
					Globa_1->QT_Step = 0x25; // 2      充电结束 结算界面 2.3----------------

					Globa_1->charger_state = 0x05;
					Globa_2->charger_state = 0x05;
					
					Globa_1->checkout = 0;
					//Globa_1->pay_step = 4;//1通道在等待刷卡扣费
          Globa_1->Charger_Over_Rmb_flag = 0;
					if(Globa_1->Start_Mode != VIN_START_TYPE)
				  {		
						if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
							Voicechip_operation_function(0x07);//07H     “充电已完成，请刷卡结算”
						}else{
							Voicechip_operation_function(0x10);//10H	请刷卡结算.wav
						}
					}
					count_wait = 0;
					break;
				}
        
				if(Globa_1->Start_Mode != VIN_START_TYPE)
				{
					if((Globa_1->qiyehao_flag == 0)&&(Globa_1->card_type == 1)&&((Globa_1->total_rmb + 50) >= Globa_1->last_rmb)){//用户卡余额不足,企业号不进行余额判断
						Globa_1->Manu_start = 0;
						Globa_1->Charger_Over_Rmb_flag = 1;
					}
				  if((Globa_1->BMS_Info_falg == 1)&&(Bind_BmsVin_falg == 0)&&(Globa_1->have_hart == 1)){
						Bind_BmsVin_falg = 1; 
						if((0 != memcmp(&Globa_1->Bind_BmsVin[0], "00000000000000000", sizeof(Globa_1->Bind_BmsVin)))&&(0 != memcmp(&Globa_1->VIN[0], "00000000000000000", sizeof(Globa_1->VIN)))&&(!((Globa_1->VIN[0] == 0xFF)&&(Globa_1->VIN[1] == 0xFF)&&(Globa_1->VIN[2] == 0xFF)))){
							if(0 != memcmp(&Globa_1->Bind_BmsVin[0],&Globa_1->VIN[0], sizeof(Globa_1->Bind_BmsVin))){
								Globa_1->Manu_start = 0;
								Globa_1->Charger_Over_Rmb_flag = 4;//与绑定的VIN不一样
							}
						}
					}else if(Globa_1->BMS_Info_falg == 0){
						Bind_BmsVin_falg = 0;
					}
			  }
				
	
				if(Globa_1->Time >  120)//120秒，充电启动2分种之后对电流判断
				{
					if(Globa_1->Charger_param.CurrentVoltageJudgmentFalg == 1)
					{ 						
						if(((abs(Globa_1->Output_current - Globa_1->meter_Current_A)*100 > METER_CURR_THRESHOLD_VALUE*Globa_1->Output_current)&&(Globa_1->Output_current >= LIMIT_CURRENT_VALUE))||
							((abs(Globa_1->Output_current - Globa_1->meter_Current_A) > 1000)&&(Globa_1->Output_current < LIMIT_CURRENT_VALUE)&&(Globa_1->Output_current >= MIN_LIMIT_CURRENT_VALUE)))
						{
							if(abs(Current_Systime - meter_Curr_Threshold_count) > METER_CURR_COUNT)//超过1分钟了，需停止充电
							{
								Globa_1->Manu_start = 0;
								Globa_1->Charger_Over_Rmb_flag = 5;//电流计量有误差
								meter_Curr_Threshold_count = Current_Systime;
								RunEventLogOut_1("计费QT判断计量有误差 Globa_1->meter_Current_A =%d Globa_1->Output_current=%d",Globa_1->meter_Current_A,Globa_1->Output_current);

							}
						}
						else
						{
							meter_Curr_Threshold_count = Current_Systime;
						}
					}
					
					if(0 == check_current_low_st)
					{
						if(Globa_1->BMS_batt_SOC >= Globa_1->Charger_param.SOC_limit   )//
						{						
							if((Globa_1->Output_current + Globa_2->Output_current)  < (Globa_1->Charger_param.min_current*1000))//电流低于限值
							{
								current_low_timestamp = Current_Systime;
								check_current_low_st = 1;
							}
						}	
					}else//电流低持续是否达到1分钟
					{
						if((Globa_1->Output_current + Globa_2->Output_current) > (Globa_1->Charger_param.min_current*1000 + 1000))//1A回差
						{
							check_current_low_st = 0;//clear						
						}
						
						if(Globa_1->BMS_batt_SOC < Globa_1->Charger_param.SOC_limit)
							check_current_low_st = 0;//clear
						
						if((Globa_1->Output_current + Globa_2->Output_current) < (Globa_1->Charger_param.min_current*1000))
						{
							if(abs(Current_Systime - current_low_timestamp) > SOCLIMIT_CURR_COUNT)//超过1分钟了，需停止充电
							{
								Globa_1->Manu_start = 0;	
								Globa_1->Charger_Over_Rmb_flag = 6;//达到SOC限制条件
								current_low_timestamp = Current_Systime;
								RunEventLogOut_1("计费QT判断达到soc限制条件");
							}
						}				
					}
					
					if((Globa_1->BMS_Info_falg == 1)&&(Globa_1->Battery_Rate_KWh > 200))//收到BMs额定信息,额定电度需要大于20kwh才进行判断
					{
						if(Globa_1->kwh >= ((100 - Globa_1->BMS_batt_Start_SOC + MAX_LIMIT_OVER_RATEKWH)*Globa_1->Battery_Rate_KWh/10))
						{
							if(abs(Current_Systime - current_overkwh_timestamp) > OVER_RATEKWH_COUNT)//超过1分钟了，需停止充电
							{
								Globa_1->Manu_start = 0;	
								Globa_1->Charger_Over_Rmb_flag = 8;//达到SOC限制条件
								RunEventLogOut_1("计费QT判断达到根据总电量和起始SOC算出需要的总电量：Battery_Rate_KWh =%d.%d  充电电量 Globa_1->kwh = %d.%d ：startsoc=%d endsoc=%d",Globa_1->Battery_Rate_KWh/10,Globa_1->Battery_Rate_KWh%10, Globa_1->kwh/100,Globa_1->kwh%100,Globa_1->BMS_batt_Start_SOC,Globa_1->BMS_batt_SOC);
                current_overkwh_timestamp = Current_Systime;
							}
						}else{
							current_overkwh_timestamp = Current_Systime;
						}
					}else{
						current_overkwh_timestamp = Current_Systime;
					}
				}	
	
				if(Globa_1->order_chg_flag == 1){//有序充电时，进行试驾判断，当时间大于
				  if(ISorder_chg_control()){
						Globa_1->Manu_start = 0;
						Globa_1->Charger_Over_Rmb_flag = 3;//有序充电时间到
					}
				}

    		if(Globa_1->Card_charger_type == 3){//按金额充电
    			if((Globa_1->total_rmb/100) >= Globa_1->QT_charge_money){
    				Globa_1->Manu_start = 0;
    			}
    		}else if(Globa_1->Card_charger_type == 1){//按电量充电
    			if((Globa_1->kwh/100) >= Globa_1->QT_charge_kwh){
    				Globa_1->Manu_start = 0;
    			}
    		}else if(Globa_1->Card_charger_type == 2){//按时间充电
    			if(Globa_1->user_sys_time >= Globa_1->QT_charge_time){
    				Globa_1->Manu_start = 0;
    			}
    		}

				count_wait++;
				if((count_wait %5  == 0)&&(count_wait1 == 0)){//等待时间超时500mS
          count_wait1 = 1;
    			Page_2020.BATT_type = Globa_1->BATT_type;		
					Page_2020.BATT_V[0] = Globa_1->Output_voltage/100;
					Page_2020.BATT_V[1] = (Globa_1->Output_voltage/100)>>8;
					Page_2020.BATT_C[0] = Globa_1->Output_current/100;
					Page_2020.BATT_C[1] = (Globa_1->Output_current/100)>>8;
					Page_2020.KWH[0] =  Globa_1->kwh;
					Page_2020.KWH[1] =  Globa_1->kwh>>8;
		      
					Page_2020.BATT_V_2[0] = Globa_2->Output_voltage/100;
					Page_2020.BATT_V_2[1] = (Globa_2->Output_voltage/100)>>8;
					Page_2020.BATT_C_2[0] = Globa_2->Output_current/100;
					Page_2020.BATT_C_2[1] = (Globa_2->Output_current/100)>>8;
					Page_2020.KWH_2[0] =  Globa_2->kwh;
					Page_2020.KWH_2[1] =  Globa_2->kwh>>8;
					
					Page_2020.Capact =  Globa_1->BMS_batt_SOC;
					Page_2020.run  = (Globa_1->Output_current > 1000)?1:0;//有充电电流 》1A

					Page_2020.elapsed_time[0] = Globa_1->Time;
					Page_2020.elapsed_time[1] = Globa_1->Time>>8;
					Page_2020.need_time[0] = Globa_1->BMS_need_time;
					Page_2020.need_time[1] = Globa_1->BMS_need_time>>8;
					Page_2020.cell_HV[0] =  Globa_1->BMS_cell_HV;
					Page_2020.cell_HV[1] =  Globa_1->BMS_cell_HV>>8;
				
					Page_2020.money[0] =  Globa_1->total_rmb;
					Page_2020.money[1] =  Globa_1->total_rmb>>8;
          Page_2020.money[2] =  Globa_1->total_rmb>>16;
				  Page_2020.money[3] =  Globa_1->total_rmb>>24;
			  
					Page_2020.money_2[0] =  Globa_2->total_rmb;
					Page_2020.money_2[1] =  Globa_2->total_rmb>>8;
          Page_2020.money_2[2] =  Globa_2->total_rmb>>16;
				  Page_2020.money_2[3] =  Globa_2->total_rmb>>24;
					Page_2020.charging = Page_2020.run;
					Page_2020.link =  (Globa_1->gun_link == 1)?1:0;
					memcpy(&Page_2020.SN[0], &Globa_1->card_sn[0], sizeof(Page_2020.SN));
					
					if(Globa_1->Start_Mode != VIN_START_TYPE)
					{
						 Page_2020.APP  = 1;   //充电启动方式 1:刷卡  2：手机APP
					}
					else
					{
						Page_2020.APP  = 3;
					}
						
	        Page_2020.BMS_12V_24V = Globa_1->BMS_Power_V;
         	memcpy(&msg.argv[0], &Page_2020.reserve1, sizeof(PAGE_2020_FRAME));
					msg.type = 0x1133;//充电界面---双枪的时候
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_2020_FRAME), IPC_NOWAIT);
					QT_timer_start(500);
    		}else if((count_wait %5 == 0)&&(count_wait1 != 0)){//平均2S发一次数据
			  	count_wait1 = 0;
					Page_BMS_data.BMS_Info_falg = Globa_1->BMS_Info_falg;
				  Page_BMS_data.BMS_H_vol[0]=  Globa_1->BMS_H_voltage/100;
				  Page_BMS_data.BMS_H_vol[1]=  (Globa_1->BMS_H_voltage/100)>>8;
          Page_BMS_data.BMS_H_cur[0]=  Globa_1->BMS_H_current/100;
				  Page_BMS_data.BMS_H_cur[1]=  (Globa_1->BMS_H_current/100)>>8;
					Page_BMS_data.BMS_cell_LT[0]=  Globa_1->BMS_cell_LT;
				  Page_BMS_data.BMS_cell_LT[1]=  Globa_1->BMS_cell_LT>>8; 
					Page_BMS_data.BMS_cell_HT[0]=  Globa_1->BMS_cell_HT;
				  Page_BMS_data.BMS_cell_HT[1]=  Globa_1->BMS_cell_HT>>8;
	
					Page_BMS_data.need_voltage[0]=  Globa_1->need_voltage/100;
				  Page_BMS_data.need_voltage[1]=  (Globa_1->need_voltage/100)>>8;
					Page_BMS_data.need_current[0]=  (Globa_1->need_current/100);
				  Page_BMS_data.need_current[1]=  (Globa_1->need_current/100)>>8;
					
					memcpy(&Page_BMS_data.BMS_Vel[0],&Globa_1->BMS_Vel[0],sizeof(Page_BMS_data.BMS_Vel));
					
					Page_BMS_data.Battery_Rate_Vol[0]=  Globa_1->BMS_rate_voltage;
				  Page_BMS_data.Battery_Rate_Vol[1]=  (Globa_1->BMS_rate_voltage)>>8;
					Page_BMS_data.Battery_Rate_AH[0]=  Globa_1->BMS_rate_AH;
				  Page_BMS_data.Battery_Rate_AH[1]=  (Globa_1->BMS_rate_AH)>>8;
					Page_BMS_data.Battery_Rate_KWh[0]=  (Globa_1->Battery_Rate_KWh);
				  Page_BMS_data.Battery_Rate_KWh[1]=  (Globa_1->Battery_Rate_KWh)>>8;
				
 				  Page_BMS_data.Cell_H_Cha_Vol[0]=  (Globa_1->Cell_H_Cha_Vol);
				  Page_BMS_data.Cell_H_Cha_Vol[1]=  (Globa_1->Cell_H_Cha_Vol)>>8;
					printf("\n-------------Globa_1->Cell_H_Cha_Vol = %d-----%d-----------\n",Globa_1->Cell_H_Cha_Vol,(Page_BMS_data.Cell_H_Cha_Vol[1]<<8|Page_BMS_data.Cell_H_Cha_Vol[0]));

				  Page_BMS_data.Cell_H_Cha_Temp =  Globa_1->Cell_H_Cha_Temp;
				  Page_BMS_data.Charge_Mode =  Globa_1->Charge_Mode;
				  Page_BMS_data.Charge_Gun_Temp =  Globa_1->Charge_Gun_Temp;
				  Page_BMS_data.BMS_cell_HV_NO =  Globa_1->BMS_cell_HV_NO;
				  Page_BMS_data.BMS_cell_LT_NO =  Globa_1->BMS_cell_LT_NO;
				  Page_BMS_data.BMS_cell_HT_NO =  Globa_1->BMS_cell_HT_NO;
					memcpy(&Page_BMS_data.VIN[0],&Globa_1->VIN[0],sizeof(Page_BMS_data.VIN));
  				memcpy(&msg.argv[0], &Page_BMS_data.reserve1, sizeof(PAGE_BMS_FRAME));
					msg.type = 0x1599;
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_BMS_FRAME), IPC_NOWAIT);
				}  
				break;
			}
			case 0x25:{//---------------2      充电结束 结算界面      2.3-------------
				Globa_1->Charger_Over_Rmb_flag = 0;
        if(Globa_1->gun_link != 1 ){
					count_wait++;
					if(count_wait > 300){//等待时间超时60S
						memcpy(&busy_frame.car_sn[0], &Globa_1->VIN, sizeof(busy_frame.car_sn));
						memcpy(&busy_frame_2.car_sn[0], &Globa_1->VIN, sizeof(busy_frame_2.car_sn));
						if(id !=0){
							pthread_mutex_lock(&busy_db_pmutex);
							Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
							pthread_mutex_unlock(&busy_db_pmutex);
							usleep(20000);
						//插入到需要上传的交易记录数据库中
							pthread_mutex_lock(&busy_db_pmutex);
							all_id = 0;
							Insertflag = Insert_Charger_Busy_DB(&busy_frame, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
							if((Insertflag != -1)&&(all_id != 0)){//表示插入数据成功，其他的则需要重新插入
								Delete_Record_busy_DB(id);
								if(DEBUGLOG) RUN_GUN1_LogOut("一号枪交易数据,插入到正式数据库中并删除临时数据库记录 ID = %d",id);  
								id = 0;
							}else{
								if(DEBUGLOG) RUN_GUN1_LogOut("一号枪交易数据,插入到正式数据库中失败 ID = %d",id);  
							}
							pthread_mutex_unlock(&busy_db_pmutex);
							
						  if(id != 0){
								pthread_mutex_lock(&busy_db_pmutex);
								for(i=0;i<5;i++){
									usleep(20000);
									if(Select_NO_Over_Record_1(id,0)!= -1){//表示有没有上传的记录
										Select_NO_Over_count = 0;
										memcpy(&pre_old_busy_frame, &busy_frame, sizeof(pre_old_busy_frame));
										pre_old_id = id;
										Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
									}else{
										if(DEBUGLOG) RUN_GUN1_LogOut("%s %d","一号枪拔枪交易数据,更新upm=0x00成功 ID=",id);  
										Select_NO_Over_count = 0;
										pre_old_id = 0;	
										id = 0;
										break;
									}
								}
							  pthread_mutex_unlock(&busy_db_pmutex);
							}
						}
						if(id_NO_2 !=0){
							pthread_mutex_lock(&busy_db_pmutex);
							Update_Charger_Busy_DB_2(id_NO_2, &busy_frame_2, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
							pthread_mutex_unlock(&busy_db_pmutex);
							usleep(20000);
						//插入到需要上传的交易记录数据库中
							pthread_mutex_lock(&busy_db_pmutex);
							all_id = 0;
							Insertflag = Insert_Charger_Busy_DB(&busy_frame, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
							if((Insertflag != -1)&&(all_id != 0)){//表示插入数据成功，其他的则需要重新插入
								Delete_Record_busy_DB_2(id_NO_2);
								if(DEBUGLOG) RUN_GUN1_LogOut("一号枪交易数据,插入到正式数据库中并删除临时数据库记录 ID = %d",id);  
								id_NO_2 = 0;
							}else{
								if(DEBUGLOG) RUN_GUN1_LogOut("一号枪交易数据,插入到正式数据库中失败 ID = %d",id);  
							}
							pthread_mutex_unlock(&busy_db_pmutex);
						}
						
						if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
							Voicechip_operation_function(0x09);//09H     “谢谢惠顾，再见”
						}else{
							Voicechip_operation_function(0x11);//11H	谢谢使用再见.wav
						}
						Select_NO_Over_count = 0;
						count_wait = 0;
  			
            Globa_1->BMS_Power_V = 0;
						Globa_1->Operation_command = 2;
						
					  if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
							msg.type = 0x150;
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);						
							QT_timer_stop(500);
							Globa_1->QT_Step = 0x03; //自动充电分开的AB通道
						}else{//单枪
							//防止按键处理不同步，此处同步一下
							msg.type = 0x150;//
							msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa_2->QT_Step = 0x03; //主界面
							
							msg.type = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选着界面，才有此按钮功能）
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa_1->QT_Step = 0x02; //主界面
						}
						break;//超时必须退出
					}
				}else{
					count_wait = 0;
				}
				
				if(Globa_1->Start_Mode == VIN_START_TYPE)
				{
#ifdef   CHECK_BUTTON					
					if(msg.type == 0x1310){//收到该界面消息
						switch(msg.argv[0]){//判断 在当前界面下 用户操作的按钮
							case 0x01:{//1#通道选着按钮
							 Globa_1->checkout = 3;
							 break;
							}
						}
					}
#else 
	      sleep(2);
	      Globa_1->checkout = 3;
#endif	
				}

				if(Globa_1->checkout != 0){//1通道检测到充电卡 0：未知 1：灰锁 2：补充交易 3：卡片正常
      		msg.type = 0x95;//解除屏保
  				msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
          count_wait = 0;
    			if(Globa_1->checkout == 1){//交易处理 灰锁
    				//不动作，继续等待刷卡
    			}else if(Globa_1->checkout == 2){//交易处理 补充交易
						memcpy(&busy_frame.flag[0], "02", sizeof(busy_frame.flag));//"00" 补充交易
						pthread_mutex_lock(&busy_db_pmutex);
						if(Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00) == -1){
							usleep(10000);
							Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
						}
						pthread_mutex_unlock(&busy_db_pmutex);
    			}else if(Globa_1->checkout == 3){//交易处理 正常
						memcpy(&busy_frame.flag[0], "00", sizeof(busy_frame.flag));//"00" 成功
  					memcpy(&busy_frame.car_sn[0], &Globa_1->VIN, sizeof(busy_frame.car_sn));
					  
						memcpy(&busy_frame_2.flag[0], "00", sizeof(busy_frame_2.flag));//"00" 成功
  					memcpy(&busy_frame_2.car_sn[0], &Globa_1->VIN, sizeof(busy_frame_2.car_sn));
					
  					pthread_mutex_lock(&busy_db_pmutex);
						if(Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00) == -1){
							usleep(10000);
							Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
						}
						pthread_mutex_unlock(&busy_db_pmutex);
  					
						pthread_mutex_lock(&busy_db_pmutex);
						if(Update_Charger_Busy_DB_2(id_NO_2, &busy_frame_2, 2, 0x00) == -1){
							usleep(10000);
							Update_Charger_Busy_DB_2(id_NO_2, &busy_frame_2, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
						}
						pthread_mutex_unlock(&busy_db_pmutex);
						//------------结算终于结束，退出结算界面--------------------------
	          if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
							Voicechip_operation_function(0x09);//09H     “谢谢惠顾，再见”
						}else{
							Voicechip_operation_function(0x11);//11H	谢谢使用再见.wav
						}
						
					//插入到需要上传的交易记录数据库中
						pthread_mutex_lock(&busy_db_pmutex);
						all_id = 0;
						Insertflag = Insert_Charger_Busy_DB(&busy_frame, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
						if((Insertflag != -1)&&(all_id != 0)){//表示插入数据成功，其他的则需要重新插入
							Delete_Record_busy_DB(id);
							if(DEBUGLOG) RUN_GUN1_LogOut("一号枪交易数据,插入到正式数据库中并删除临时数据库记录 ID = %d",id);  
							id = 0;
						}else{
							if(DEBUGLOG) RUN_GUN1_LogOut("一号枪交易数据,插入到正式数据库中失败 ID = %d",id);  
						}
						pthread_mutex_unlock(&busy_db_pmutex);

						if(id != 0){
							pthread_mutex_lock(&busy_db_pmutex);
							for(i=0;i<5;i++){
								if(Select_NO_Over_Record_1(id,0)!= -1){//表示有没有上传的记录
									Select_NO_Over_count = 0;
									memcpy(&pre_old_busy_frame, &busy_frame, sizeof(pre_old_busy_frame));
									pre_old_id = id;
									Update_Charger_Busy_DB_1(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
								}else{
									if(DEBUGLOG) RUN_GUN1_LogOut("%s %d","一号枪交易数据,更新upm=0x00成功 ID=",id);  
									Select_NO_Over_count = 0;
									pre_old_id = 0;	
									id = 0;
									break;
								}
								sleep(1);
							}
							pthread_mutex_unlock(&busy_db_pmutex);
						}else{
							pre_old_id = 0;	
							id = 0;
						}
						
						pthread_mutex_lock(&busy_db_pmutex);
						all_id = 0;
						Insertflag = Insert_Charger_Busy_DB(&busy_frame_2, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
						if((Insertflag != -1)&&(all_id != 0)){//表示插入数据成功，其他的则需要重新插入
							Delete_Record_busy_DB_2(id_NO_2);
							if(DEBUGLOG) RUN_GUN1_LogOut("2号枪交易数据,插入到正式数据库中并删除临时数据库记录 ID = %d",id);  
							id_NO_2 = 0;
						}else{
							if(DEBUGLOG) RUN_GUN1_LogOut("2号枪交易数据,插入到正式数据库中失败 ID = %d",id);  
						}
						//pthread_mutex_unlock(&busy_db_pmutex);
		        if(id_NO_2 != 0){
						//	pthread_mutex_lock(&busy_db_pmutex);
							for(i=0;i<5;i++){
								if(Select_NO_Over_Record_2(id_NO_2,0)!= -1){//表示有没有上传的记录							
									Update_Charger_Busy_DB_2(id_NO_2, &busy_frame_2, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
								}else{
									id_NO_2 = 0;	
									break;
								}
								usleep(200000);
							}
						
						}
						pthread_mutex_unlock(&busy_db_pmutex);
	
            Globa_1->BMS_Power_V = 0;
					  Select_NO_Over_count = 0;
						Globa_1->Operation_command = 2;//进行解锁
					  if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
							msg.type = 0x150;
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);						
							QT_timer_stop(500);
							Globa_1->QT_Step = 0x03; //自动充电分开的AB通道
						}else{//单枪
							//防止按键处理不同步，此处同步一下
							msg.type = 0x150;//
							msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa_2->QT_Step = 0x03; //主界面
							
							msg.type = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选着界面，才有此按钮功能）
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa_1->QT_Step = 0x02; //主界面
						}
    			}
    			Globa_1->checkout = 0;
    		}

				break;
			}
  		case 0x30:{//等待刷卡--等待刷维护卡才能进入系统管理状态
				count_wait++;
				if(count_wait > 100){//等待时间超时20S
    			count_wait = 0;
  				msg.type = 0x100;
  				msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
  				Globa_1->QT_Step = 0x02;
  				break;//超时必须退出
  			}
  			
				if(Globa_1->checkout == 7){//
     			msg.type = 0x3000;//系统管理界面
		    	msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
		    	Globa_1->QT_Step = 0x31; // 系统管理界面
		    	count_wait = 0;
		      Globa_1->checkout = 0;
					Globa_1->pay_step = 0;
    			break;
  			}else{
  				Globa_1->checkout = 0;//清除刷卡结果标志
  			}
				break;
			}
			case 0x31:{//---------------3     系统管理 选着  状态  3.1----------------
    		if(msg.type == 0x3000){//收到该界面消息
    			switch(msg.argv[0]){//判断 在当前界面下 用户操作的按钮
    				case 0x01:{//实时数据 按钮
							memcpy(&msg.argv[0], &AC_measure_data.reserve1, sizeof(AC_measure_data));
							msg.type = 0x3100;//实时数据
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(AC_measure_data), IPC_NOWAIT);

							Globa_1->QT_Step = 0x312;//3    系统管理 实时数据  状态  3.1.2

							QT_timer_start(500);

    					break;
    				}
    				case 0x02:{//网络设置 按钮
    					Page_3203.reserve1 = 1;
		
    					memcpy(&Page_3203.Server_IP[0], &Globa_1->ISO8583_NET_setting.ISO8583_Server_IP[0], sizeof(Globa_1->ISO8583_NET_setting.ISO8583_Server_IP));
    					memcpy(&Page_3203.port[0], &Globa_1->ISO8583_NET_setting.ISO8583_Server_port[0], sizeof(Globa_1->ISO8583_NET_setting.ISO8583_Server_port));
    					memcpy(&Page_3203.addr[0], &Globa_1->ISO8583_NET_setting.addr[0], sizeof(Globa_1->ISO8583_NET_setting.addr));  					

							memcpy(&msg.argv[0], &Page_3203.reserve1, sizeof(Page_3203));
     					msg.type = 0x3200;
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(Page_3203), IPC_NOWAIT);
							
							Globa_1->QT_Step = 0x311;//3    系统管理 具体设置  状态  3.1.1
    					break;
    				}
    				case 0x03:{//报警查询 按钮
							msg.type = 0x3300;
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							
							Globa_1->QT_Step = 0x311;//3    系统管理 具体设置  状态  3.1.1
    					break;
    				}
    				case 0x04:{//USB模块 按钮
							msg.type = 0x3400;
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							
							Globa_1->QT_Step = 0x311;//3    系统管理 具体设置  状态  3.1.1
    					break;
    				}
    				case 0x05:{//参数设置 按钮
    					Page_3503.reserve1 = 1;

							Page_3503.num1 = Globa_1->Charger_param.Charge_rate_number1;
							Page_3503.num2 = Globa_1->Charger_param.Charge_rate_number2;

							Page_3503.rate_v[0] = (Globa_1->Charger_param.Charge_rate_voltage/1000)&0xff;
							Page_3503.rate_v[1] = ((Globa_1->Charger_param.Charge_rate_voltage/1000)>>8)&0xff; 
							Page_3503.single_rate_c = Globa_1->Charger_param.model_rate_current/1000;	  //单模块额定电流 1A

    					memcpy(&Page_3503.SN[0], &Globa_1->Charger_param.SN[0], sizeof(Globa_1->Charger_param.SN));
							Page_3503.meter_config = Globa_1->Charger_param.meter_config_flag;
							Page_3503.AC_Meter_config = Globa_1->Charger_param.AC_Meter_config;
							Page_3503.AC_Meter_CT = Globa_1->Charger_param.AC_Meter_CT;

							Page_3503.output_over_limit[0] = (Globa_1->Charger_param.output_over_limit/100)&0xff;
							Page_3503.output_over_limit[1] = ((Globa_1->Charger_param.output_over_limit/100)>>8)&0xff;
							Page_3503.output_lown_limit[0] = (Globa_1->Charger_param.output_lown_limit/100)&0xff;
							Page_3503.output_lown_limit[1] = ((Globa_1->Charger_param.output_lown_limit/100)>>8)&0xff;

							Page_3503.input_over_limit[0] = (Globa_1->Charger_param.input_over_limit/100)&0xff;
							Page_3503.input_over_limit[1] = ((Globa_1->Charger_param.input_over_limit/100)>>8)&0xff;
							Page_3503.input_lown_limit[0] = (Globa_1->Charger_param.input_lown_limit/100)&0xff;
							Page_3503.input_lown_limit[1] = ((Globa_1->Charger_param.input_lown_limit/100)>>8)&0xff;
							Page_3503.current_limit = Globa_1->Charger_param.current_limit;

							Page_3503.BMS_work = Globa_1->Charger_param.BMS_work;
							Page_3503.BMS_CAN_ver = Globa_1->Charger_param.BMS_CAN_ver;

							Page_3503.Price[0] = (Globa_1->Charger_param.Price)&0xff;
							Page_3503.Price[1] = ((Globa_1->Charger_param.Price)>>8)&0xff;
							Page_3503.Serv_Type = Serv_Type;
							Page_3503.Serv_Price[0] = (Globa_1->Charger_param.Serv_Price)&0xff;
							Page_3503.Serv_Price[1] = ((Globa_1->Charger_param.Serv_Price)>>8)&0xff;
            
							Page_3503.DC_Shunt_Range[0] = (Globa_1->Charger_param.DC_Shunt_Range)&0xff;
							Page_3503.DC_Shunt_Range[1] = ((Globa_1->Charger_param.DC_Shunt_Range)>>8)&0xff;
							
							Page_3503.charge_modluetype = Globa_1->Charger_param.charge_modluetype;
							Page_3503.APP_enable = Globa_1->Charger_param.APP_enable;
		      		Page_3503.NEW_VOICE_Flag = Globa_1->Charger_param.NEW_VOICE_Flag;
    					memcpy(&Page_3503.Power_meter_addr1[0], &Globa_1->Charger_param.Power_meter_addr1[0], sizeof(Globa_1->Charger_param.Power_meter_addr1));
    					memcpy(&Page_3503.Power_meter_addr2[0], &Globa_1->Charger_param.Power_meter_addr2[0], sizeof(Globa_1->Charger_param.Power_meter_addr2));
							Page_3503.gun_allowed_temp = (Globa_1->Charger_param.gun_allowed_temp)&0xff;
							Page_3503.Charging_gun_type = Globa_1->Charger_param.Charging_gun_type;
              Page_3503.Input_Relay_Ctl_Type = Globa_1->Charger_param.Input_Relay_Ctl_Type;
						  Page_3503.System_Type = Globa_1->Charger_param.System_Type;
              Page_3503.Serial_Network = Globa_1->Charger_param.Serial_Network;
							Page_3503.DTU_Baudrate = Globa_1->Charger_param.DTU_Baudrate;
							Page_3503.couple_flag = Globa_1->Charger_param.couple_flag;
							Page_3503.Control_board_Baud_rate = Globa_1->Charger_param.Control_board_Baud_rate;
              
							Page_3503.Start_time_on_hour = Globa_1->Charger_param.Light_Control_Data.Start_time_on_hour;
	 						Page_3503.Start_time_on_min = Globa_1->Charger_param.Light_Control_Data.Start_time_on_min;
	 						Page_3503.End_time_off_hour = Globa_1->Charger_param.Light_Control_Data.End_time_off_hour;
	 						Page_3503.End_time_off_min = Globa_1->Charger_param.Light_Control_Data.End_time_off_min;
							Page_3503.Car_Lock_number = dev_cfg.dev_para.car_lock_num;
							Page_3503.Car_Lock_addr1 = dev_cfg.dev_para.car_lock_addr[0];
							Page_3503.Car_Lock_addr2 = dev_cfg.dev_para.car_lock_addr[1];
						  Page_3503.LED_Type_Config = Globa_1->Charger_param.LED_Type_Config;
							
							Page_3503.Model_Type = 	Globa_1->Charger_param.Model_Type;//模块类型
							Page_3503.heartbeat_idle_period = Globa_1->Charger_param.heartbeat_idle_period;     //空闲心跳周期 秒
							Page_3503.heartbeat_busy_period = Globa_1->Charger_param.heartbeat_busy_period;     //充电心跳周期 秒
							Page_3503.charge_modlue_index = Globa_1->Charger_param.charge_modlue_index;
							Page_3503.SOC_limit = Globa_1->Charger_param.SOC_limit;	//SOC达到多少时，判断电流低于min_current后停止充电,值95表示95%
							Page_3503.min_current =  Globa_1->Charger_param.min_current ;//单位A  值30表示低于30A后持续1分钟停止充电
							Page_3503.CurrentVoltageJudgmentFalg = Globa_1->Charger_param.CurrentVoltageJudgmentFalg;//0-不判断 1-进行判断。收到的电压电流和CCU发送过来的进行判断
							
							memcpy(&msg.argv[0], &Page_3503.reserve1, sizeof(Page_3503));
     					msg.type = 0x3500;
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(Page_3503), IPC_NOWAIT);
							
							Globa_1->QT_Step = 0x311;//3    系统管理 具体设置  状态  3.1.1
    					break;
    				}
    			
    		  	case 0x06:{//手动充电按钮
          		Page_3600.reserve1 = 1;//设置按钮值，刷新全部数据
          		Page_3600.NEED_V[0] =  Globa_1->Charger_param.set_voltage/100;
          		Page_3600.NEED_V[1] = (Globa_1->Charger_param.set_voltage/100)>>8;
          		Page_3600.NEED_C[0] =  Globa_1->Charger_param.set_current/100;
          		Page_3600.NEED_C[1] = (Globa_1->Charger_param.set_current/100)>>8;
          		Page_3600.time[0]  = 0x0f;  //时间 分
          		Page_3600.time[1]  = 0x27;  //默认 9999
          		Page_3600.select = 1;       //选择的充电枪    默认 1        

          		Page_3600.BATT_V[0] = 0;
          		Page_3600.BATT_V[1] = 0;
          		Page_3600.BATT_C[0] = 0;
          		Page_3600.BATT_C[1] = 0;
          
          		Page_3600.Time[0] = 0;
          		Page_3600.Time[1] = 0;
          		Page_3600.KWH[0]  = 0;
          		Page_3600.KWH[1]  = 0;

          		msg.type = 0x3600;//手动充电 界面

          		memcpy(&msg.argv[0], &Page_3600.reserve1, sizeof(PAGE_3600_FRAME));
          		msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_3600_FRAME), IPC_NOWAIT);
          
          		QT_timer_start(500);
          		Globa_1->QT_Step = 0x313; // 2 手动充电-手动充电界面 状态  2.3
          		

							Globa_1->kwh = 0;
							Globa_1->soft_KWH =0;
							Globa_1->Time = 0;
							
						  Globa_1->meter_start_KWH = Globa_1->meter_KWH;//取初始电表示数

    					break;
    				}
    				
    				case 0x07:{//退出 按钮
							msg.type = 0x100;
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							
							Globa_1->QT_Step = 0x02; // 0 主界面 空闲( 可操作 )状态  0.2

    					break;
    				}
						case 0x08:{//费率计算
							msg.type = 0x3700;
	            int i =0;
	            for(i = 0;i<4;i++){
								memcpy(&Page_3703.share_time_kwh_price[i][0],&Globa_1->Charger_param2.share_time_kwh_price[i],4*sizeof(char));
								memcpy(&Page_3703.share_time_kwh_price_serve[i][0],&Globa_1->Charger_param2.share_time_kwh_price_serve[i],4*sizeof(char));
								Page_3703.Card_reservation_price[i] = 0;
							}
							Page_3703.Stop_Car_price[0] = dev_cfg.dev_para.car_park_price;
							Page_3703.Stop_Car_price[1] = dev_cfg.dev_para.car_park_price>>8;
							Page_3703.Stop_Car_price[2] = dev_cfg.dev_para.car_park_price>>16;
							Page_3703.Stop_Car_price[3] = dev_cfg.dev_para.car_park_price>>24;
							Page_3703.time_zone_num = Globa_1->Charger_param2.time_zone_num;
							for(i=0;i<12;i++){
							  memcpy(&Page_3703.time_zone_tabel[i][0],&Globa_1->Charger_param2.time_zone_tabel[i],2*sizeof(char));
						  	memcpy(&Page_3703.time_zone_feilv[i],&Globa_1->Charger_param2.time_zone_feilv[i],sizeof(char));
							}
          		memcpy(&msg.argv[0], &Page_3703.reserve1, sizeof(Page_3703));
              msgsnd(Globa_1->arm_to_qt_msg_id, &msg,sizeof(Page_3703), IPC_NOWAIT);
						
							Globa_1->QT_Step = 0x311; // 0 主界面 空闲( 可操作 )状态  0.2
							break;
						}
						case 0x09:{//告警屏蔽功能
							msg.type = 0x7000;
							msg.argv[0] = 0;
          		memcpy(&msg.argv[1], &Globa_1->Charger_Shileld_Alarm, sizeof(Globa_1->Charger_Shileld_Alarm));
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, (sizeof(Globa_1->Charger_Shileld_Alarm) + 1), IPC_NOWAIT);
							Globa_1->QT_Step = 0x70; // 0 主界面 空闲( 可操作 )状态  0.2
    					break;
    				}
					
    				default:{
    					break;
    				}
    			}
    		}

				break;
			}
			case 0x311:{//--------------3    系统管理 具体设置  状态  3.1.1-----------
  			switch(msg.type){//判断当前所在界面
  				case 0x3300: //系统管理 ->报警查询
  				case 0x3400:{ //系统管理 ->USB模块
  					switch(msg.argv[0]){
      				case 0x01:{  //返回上一层按钮
   							msg.type = 0x3000;
      					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);  				

      					Globa_1->QT_Step = 0x31; // 3     系统管理 选着  状态  3.1
    
      					break;
      				}
      				case 0x02:{ //返回主页  
      					msg.type = 0x100;
      					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);				

      					Globa_1->QT_Step = 0x02; // 0 主界面 空闲( 可操作 )状态  0.1
    
      					break;
      				}
      				default:{
      					break;
      				}
      			}
  					
  					break;
  				}
  				case 0x3200:{ //系统管理 ->网络设置
  					switch(msg.argv[0]){
      				case 0x01:{  //返回上一层按钮
   							msg.type = 0x3000;
      					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);				

      					Globa_1->QT_Step = 0x31; // 3     系统管理 选着  状态  3.1
    
      					break;
      				}
      				case 0x02:{ //返回主页  
      					msg.type = 0x100;
      					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);

      					Globa_1->QT_Step = 0x02; // 0 主界面 空闲( 可操作 )状态  0.1
    
      					break;
      				}
      				case 0x03:{ //网络设置 保存按钮
  							PAGE_3203_FRAME *Page_3203_temp = (PAGE_3203_FRAME *)&msg.argv[0];
				
    						memcpy(&Globa_1->ISO8583_NET_setting.ISO8583_Server_IP[0], &Page_3203_temp->Server_IP[0], sizeof(Globa_1->ISO8583_NET_setting.ISO8583_Server_IP));
    						memcpy(&Globa_1->ISO8583_NET_setting.ISO8583_Server_port[0], &Page_3203_temp->port[0], sizeof(Globa_1->ISO8583_NET_setting.ISO8583_Server_port));
    						memcpy(&Globa_1->ISO8583_NET_setting.addr[0], &Page_3203_temp->addr[0], sizeof(Globa_1->ISO8583_NET_setting.addr));
 
    						printf("服务器端地址: %s\n", Globa_1->ISO8583_NET_setting.ISO8583_Server_IP);
    						printf("服务器端口号: %d\n", Globa_1->ISO8583_NET_setting.ISO8583_Server_port[0]+(Globa_1->ISO8583_NET_setting.ISO8583_Server_port[1]<<8)); 
    						printf("本机公共地址: %d\n", Globa_1->ISO8583_NET_setting.addr[0]+(Globa_1->ISO8583_NET_setting.addr[1]<<8)); 

    						ISO8583_NET_setting_save();//保存网络参数
    						system("reboot");

      					break;
      				}
      				default:{
      					break;
      				}
      			}

  					break;
  				}
  				case 0x3500:{//系统管理 ->参数设置
  					switch(msg.argv[0]){
      				case 0x01:{  //返回上一层按钮
   							msg.type = 0x3000;
      					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);				

      					Globa_1->QT_Step = 0x31; // 3     系统管理 选着  状态  3.1
    
      					break;
      				}
      				case 0x02:{ //返回主页  
      					msg.type = 0x100;
      					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);

      					Globa_1->QT_Step = 0x02; // 0 主界面 空闲( 可操作 )状态  0.1
    
      					break;
      				}
      				case 0x03:{ //模块设置 保存按钮
  							PAGE_3503_FRAME *Page_3503_temp = (PAGE_3503_FRAME *)&msg.argv[0];

      					Globa_1->Charger_param.Charge_rate_number1 = Page_3503_temp->num1;
      					Globa_1->Charger_param.Charge_rate_number2 = Page_3503_temp->num2;

      					Globa_1->Charger_param.Charge_rate_voltage = ((Page_3503_temp->rate_v[1]<<8)+Page_3503_temp->rate_v[0])*1000;	//充电机额定电压  0.001V
      					Globa_1->Charger_param.model_rate_current = Page_3503_temp->single_rate_c*1000;
      					Globa_1->Charger_param.Charge_rate_current1 = Globa_1->Charger_param.Charge_rate_number1*Globa_1->Charger_param.model_rate_current;		  //充电机额定电流  0.001A
      					Globa_1->Charger_param.Charge_rate_current2 = Globa_1->Charger_param.Charge_rate_number2*Globa_1->Charger_param.model_rate_current;		  //充电机额定电流  0.001A

      					Globa_1->Charger_param.BMS_CAN_ver = Page_3503_temp->BMS_CAN_ver;
      					Globa_1->Charger_param.meter_config_flag = Page_3503_temp->meter_config;
      					Globa_1->Charger_param.BMS_work = Page_3503_temp->BMS_work;
      					Globa_1->Charger_param.AC_Meter_config = Page_3503_temp->AC_Meter_config;
      					Globa_1->Charger_param.AC_Meter_CT = Page_3503_temp->AC_Meter_CT;

      					Globa_1->Charger_param.Price = (Page_3503_temp->Price[1]<<8)+Page_3503_temp->Price[0];
      					if(Globa_1->have_hart != 1){ //与后台无通讯
									for(i = 0;i<4;i++){
										share_time_kwh_price[i]= Globa_1->Charger_param.Price;  //单价,默认为非时电价
									}
	                memset(&Globa_1->Charger_param2.price_type_ver[0], '9', sizeof(Globa_1->Charger_param2.price_type_ver));
									
	                Globa_1->Charger_param.Serv_Price = (Page_3503_temp->Serv_Price[1]<<8)+Page_3503_temp->Serv_Price[0];	//充电机额定电压  0.001V
								  Globa_1->Charger_param.Serv_Type = Page_3503_temp->Serv_Type;	
									for(i = 0;i<4;i++){
										Globa_1->Charger_param2.share_time_kwh_price_serve[i]= Globa_1->Charger_param.Serv_Price;  //单价,默认为非时电价
									}
									Globa_1->Charger_param2.show_price_type = 0;
									System_param2_save();
								}
	
                Globa_1->Charger_param.charge_modluetype = Page_3503_temp->charge_modluetype;
                Globa_1->Charger_param.APP_enable = Page_3503_temp->APP_enable;

      					Globa_1->Charger_param.output_over_limit = ((Page_3503_temp->output_over_limit[1]<<8)+Page_3503_temp->output_over_limit[0])*100;
      					Globa_1->Charger_param.output_lown_limit = ((Page_3503_temp->output_lown_limit[1]<<8)+Page_3503_temp->output_lown_limit[0])*100;

      					Globa_1->Charger_param.input_over_limit = ((Page_3503_temp->input_over_limit[1]<<8)+Page_3503_temp->input_over_limit[0])*100;
      					Globa_1->Charger_param.input_lown_limit = ((Page_3503_temp->input_lown_limit[1]<<8)+Page_3503_temp->input_lown_limit[0])*100;			
							 	Globa_1->Charger_param.current_limit = Page_3503_temp->current_limit;

    						memcpy(&Globa_1->Charger_param.SN[0], &Page_3503_temp->SN[0], sizeof(Globa_1->Charger_param.SN));
    						memcpy(&Globa_1->Charger_param.Power_meter_addr1[0], &Page_3503_temp->Power_meter_addr1[0], sizeof(Globa_1->Charger_param.Power_meter_addr1)); 
    						memcpy(&Globa_1->Charger_param.Power_meter_addr2[0], &Page_3503_temp->Power_meter_addr2[0], sizeof(Globa_1->Charger_param.Power_meter_addr2)); 
	              Globa_1->Charger_param.Charging_gun_type = Page_3503_temp->Charging_gun_type;
								Globa_1->Charger_param.gun_allowed_temp = Page_3503_temp->gun_allowed_temp;
                Globa_1->Charger_param.Input_Relay_Ctl_Type = Page_3503_temp->Input_Relay_Ctl_Type;
							  Globa_1->Charger_param.System_Type = Page_3503_temp->System_Type;
							 
							 if((Globa_1->Charger_param.System_Type != 0)&&(Globa_1->Charger_param.System_Type != 4)){
      					  Globa_1->Charger_param.Charge_rate_number2 = Globa_1->Charger_param.Charge_rate_number1;
								}
								
  							/*if(Globa_1->Charger_param.charge_modluetype <= 4 ){//重新添加R95021G1 --20KW 21A R50040G1 20kW 
									Globa_1->Charger_param.Model_Type = 0; //华为模块
								}else if((Globa_1->Charger_param.charge_modluetype >= 5)&&(Globa_1->Charger_param.charge_modluetype <= 8)){
									Globa_1->Charger_param.Model_Type = 3; //EAST 模块
								}else if((Globa_1->Charger_param.charge_modluetype >= 9)&&(Globa_1->Charger_param.charge_modluetype <= 20)){//英可瑞
									Globa_1->Charger_param.Model_Type  = 1;
								}else if((Globa_1->Charger_param.charge_modluetype > 20)&&(Globa_1->Charger_param.charge_modluetype <= 26)){//英飞源
									Globa_1->Charger_param.Model_Type = 2; 
								}else if((Globa_1->Charger_param.charge_modluetype > 26)&&(Globa_1->Charger_param.charge_modluetype <= 30)){//永联模块-用的协议还是英可瑞的
									Globa_1->Charger_param.Model_Type = 1; 
								}else if((Globa_1->Charger_param.charge_modluetype >= 31)){//中兴
									Globa_1->Charger_param.Model_Type = 4; 
								}*/

							  Globa_1->Charger_param.Serial_Network = Page_3503_temp->Serial_Network; 
                Globa_1->Charger_param.DTU_Baudrate = Page_3503_temp->DTU_Baudrate;
			      	  Globa_1->Charger_param.NEW_VOICE_Flag =  Page_3503_temp->NEW_VOICE_Flag;
						    Globa_1->Charger_param.couple_flag =  Page_3503_temp->couple_flag;
						  	Globa_1->Charger_param.Control_board_Baud_rate = Page_3503_temp->Control_board_Baud_rate;
							  Globa_1->Charger_param.LED_Type_Config = Page_3503_temp->LED_Type_Config;
							  Globa_1->Charger_param.Model_Type = Page_3503_temp->Model_Type;//模块类型
						    Globa_1->Charger_param.heartbeat_idle_period = 	Page_3503_temp->heartbeat_idle_period;     //空闲心跳周期 秒
						    Globa_1->Charger_param.heartbeat_busy_period = Page_3503_temp->heartbeat_busy_period;     //充电心跳周期 秒
							  Globa_1->Charger_param.charge_modlue_index = 	Page_3503_temp->charge_modlue_index ; //具体模块索引
							  Globa_1->Charger_param.SOC_limit = Page_3503_temp->SOC_limit;	//SOC达到多少时，判断电流低于min_current后停止充电,值95表示95%
							  Globa_1->Charger_param.min_current = Page_3503_temp->min_current;//单位A  值30表示低于30A后持续1分钟停止充电
							  Globa_1->Charger_param.CurrentVoltageJudgmentFalg = Page_3503_temp->CurrentVoltageJudgmentFalg;//0-不判断 1-进行判断。收到的电压电流和CCU发送过来的进行判断
							
								
								if((dev_cfg.dev_para.car_lock_num != Page_3503_temp->Car_Lock_number)
									||(dev_cfg.dev_para.car_lock_addr[0] != Page_3503_temp->Car_Lock_addr1)
							    ||(dev_cfg.dev_para.car_lock_addr[1] != Page_3503_temp->Car_Lock_addr2)
							  ){
								  dev_cfg.dev_para.car_lock_num = Page_3503_temp->Car_Lock_number;
								  dev_cfg.dev_para.car_lock_addr[0] = Page_3503_temp->Car_Lock_addr1;
									dev_cfg.dev_para.car_lock_addr[1] = Page_3503_temp->Car_Lock_addr2;
									System_CarLOCK_Param_save();
								}
    				/*	printf("设置模块数量: %d\n", Globa_1->Charger_param.Charge_rate_number1); 
    						printf("设置模块数量: %d\n", Globa_1->Charger_param.Charge_rate_number2); 
 								printf("Globa_1->Charger_param.System_Type: %d\n", Globa_1->Charger_param.System_Type); 

    						printf("设置系统额定电压: %d\n", Globa_1->Charger_param.Charge_rate_voltage);
    						printf("设置单模块额定电流: %d\n", Globa_1->Charger_param.model_rate_current); 
    						printf("BMS VER: %d\n", Globa_1->Charger_param.BMS_CAN_ver); 
    						char i;
    						printf("SN:");
    						for(i=0;i<14;i++){
    							printf(" %d", Page_3503_temp->SN[i]); 
    						}
     						printf("\r\n");
    						
    						printf("price: %d\n", Globa_1->Charger_param.Price); 
    						printf("电表配置: %d\n", Globa_1->Charger_param.meter_config_flag); 
    						printf("设置输出过压点: %d\n", Globa_1->Charger_param.output_over_limit); 
    						printf("设置输出欠压点: %d\n", Globa_1->Charger_param.output_lown_limit); 
    						printf("设置输入过压点: %d\n", Globa_1->Charger_param.input_over_limit); 
    						printf("设置输入欠压点: %d\n", Globa_1->Charger_param.input_lown_limit); 					
    						printf("设置服务费: %d\n", Globa_1->Charger_param.Serv_Price); 
    						printf("设置服务费收费方式: %d\n", Serv_Type);  						
*/
    						System_param_save();//保存系统参数
	              usleep(100000);
								system("sync");
								usleep(100000);
    						system("reboot");
      					break;
      				}
      				
							case 0x04:{ //设置参数直流分流器量程
      				  Globa_1->Charger_param.DC_Shunt_Range = msg.argv[2]<<8|msg.argv[1];
								Globa_2->Charger_param.DC_Shunt_Range = msg.argv[2]<<8|msg.argv[1];

      				  Dc_shunt_Set_meter_flag_1 = 1;
      				  Dc_shunt_Set_meter_flag_2 = 1;
                Dc_shunt_Set_CL_flag_1 = 1;
                Dc_shunt_Set_CL_flag_2 = 1;
      				  sleep(5);//等待4秒等待反馈数据
      				  msg.type = 0x93;
      				  msg.argv[0] = 1;
      				  msg.argv[1] = 1 - (Dc_shunt_Set_meter_flag_1&Dc_shunt_Set_CL_flag_1&Dc_shunt_Set_meter_flag_2&Dc_shunt_Set_CL_flag_2);
			 		     	if(msg.argv[1] == 1){
									System_param_save();//保存系统参数
								}
							  msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 2, IPC_NOWAIT);
      				  break;
      				}
      			  
							case 0x05:{ //修改M1密钥
								Globa_1->Charger_param.Key[0] = 0xFF;
								Globa_1->Charger_param.Key[1] = 0xFF;
								Globa_1->Charger_param.Key[2] = 0xFF;
								Globa_1->Charger_param.Key[3] = 0xFF;
								Globa_1->Charger_param.Key[4] = 0xFF;
								Globa_1->Charger_param.Key[5] = 0xFF;
								System_param_save();//保存系统参数
						    usleep(100000);
								system("sync");
								break;
							}
      				
							case 0x06:
							{//下发灯箱
							  Globa_1->Charger_param.Light_Control_Data.Start_time_on_hour = msg.argv[1];
								Globa_1->Charger_param.Light_Control_Data.Start_time_on_min = msg.argv[2];
								Globa_1->Charger_param.Light_Control_Data.End_time_off_hour = msg.argv[3];
								Globa_1->Charger_param.Light_Control_Data.End_time_off_min = msg.argv[4];
								System_param_save();//保存系统参数
						    usleep(100000);
								system("sync");
								break;
							}
							default:{
      					break;
      				}
      			}
  					break;
  				}  				
  				case 0x3700:{//系统参数--费率设置
						switch(msg.argv[0]){
      				case 0x01:{  //返回上一层按钮
   							msg.type = 0x3000;
      					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);				
      					Globa_1->QT_Step = 0x31; // 3     系统管理 选着  状态  3.1
      					break;
      				}
      				case 0x02:{ //返回主页  
      					msg.type = 0x100;
      					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
      					Globa_1->QT_Step = 0x02; // 0 主界面 空闲( 可操作 )状态  0.1
      					break;
      				}
      				case 0x03:{ //保存按钮
  							PAGE_3703_FRAME *Page_3703_temp = (PAGE_3703_FRAME *)&msg.argv[0];
      					if(Globa_1->have_hart != 1){ //与后台无通讯
								 for(i = 0;i<4;i++){
										memcpy(&Globa_1->Charger_param2.share_time_kwh_price[i],&Page_3703_temp->share_time_kwh_price[i][0],4*sizeof(char));
										memcpy(&Globa_1->Charger_param2.share_time_kwh_price_serve[i],&Page_3703_temp->share_time_kwh_price_serve[i][0],4*sizeof(char));
										//Page_3703.Card_reservation_price[i] = 0;
										//Page_3703.Stop_Car_price[i] = 0;
										memcpy(&Globa_2->Charger_param2.share_time_kwh_price[i],&Page_3703_temp->share_time_kwh_price[i][0],4*sizeof(char));
										memcpy(&Globa_2->Charger_param2.share_time_kwh_price_serve[i],&Page_3703_temp->share_time_kwh_price_serve[i][0],4*sizeof(char));
									
									}
									Globa_1->Charger_param2.time_zone_num = Page_3703_temp->time_zone_num;
									Globa_2->Charger_param2.time_zone_num = Page_3703_temp->time_zone_num;

									for(i=0;i<12;i++){
										memcpy(&Globa_1->Charger_param2.time_zone_tabel[i],&Page_3703_temp->time_zone_tabel[i][0],2*sizeof(char));
										memcpy(&Globa_1->Charger_param2.time_zone_feilv[i],&Page_3703_temp->time_zone_feilv[i],sizeof(char));
									
  									memcpy(&Globa_2->Charger_param2.time_zone_tabel[i],&Page_3703_temp->time_zone_tabel[i][0],2*sizeof(char));
										memcpy(&Globa_2->Charger_param2.time_zone_feilv[i],&Page_3703_temp->time_zone_feilv[i],sizeof(char));
									}
								  memset(&Globa_1->Charger_param2.price_type_ver[0], '9', sizeof(Globa_1->Charger_param2.price_type_ver));

									System_param2_save();
								  usleep(200000);
								  system("sync");
									system("reboot");
								}
      					break;
      				}
					  
						}
					}
					default:{
  					break;
  				}
  			}
				break;
			}
			case 0x312:{//--------------3    系统管理 实时数据  状态  3.1.2-----------
  			if(msg.type == 0x3100){//判断当前所在界面
  				switch(msg.argv[0]){
    				case 0x01:{  //返回上一层按钮
    					QT_timer_stop(500);
 							msg.type = 0x3000;
    					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);  				

    					Globa_1->QT_Step = 0x31; // 3     系统管理 选着  状态  3.1
  
    					break;
    				}
    				case 0x02:{ //返回主页 
    					QT_timer_stop(500); 

    					msg.type = 0x100;
    					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);				

    					Globa_1->QT_Step = 0x02; // 0 主界面 空闲( 可操作 )状态  0.1
  
    					break;
    				}
    				default:{
    					break;
    				}
      		}
    		}else if(QT_timer_tick(500) == 1){
					memcpy(&msg.argv[0], &AC_measure_data.reserve1, sizeof(AC_measure_data));
					msg.type = 0x3100;//实时数据
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(AC_measure_data), IPC_NOWAIT);
					
					QT_timer_start(500);
    		}

				break;
			}
		 	case 0x313:{//---------------2 手动充电-》 手动充电界面       2.3----------
				Globa_1->kwh = (Globa_1->meter_KWH - Globa_1->meter_start_KWH);
				
    		if(msg.type == 0x3600){//收到该界面消息
    			switch(msg.argv[0]){//判断 在当前界面下 用户操作的按钮
    				case 0x01:{//设置按钮
							Globa_1->Charger_param.set_voltage = ((msg.argv[2]<<8)+msg.argv[1])*100;
							Globa_1->Charger_param.set_current = ((msg.argv[4]<<8)+msg.argv[3])*100;

          		Globa_1->QT_charge_time = (msg.argv[6]<<8)+msg.argv[5];  //时间    分
          		Globa_1->QT_gun_select = msg.argv[7];    //充电枪选择 1：充电枪1 2: 充电枪2

  						//System_param_save();//保存系统参数
  						printf("手动设置电压: %d\n", Globa_1->Charger_param.set_voltage);
  						printf("手动设置电流: %d\n", Globa_1->Charger_param.set_current); 
  						printf("预设充电时间: %d\n", Globa_1->QT_charge_time);

  						//界面保持不变，不回应数据给界面 
    
    					break;
    				}   
    				case 0x02:{//开始充电按钮
						  if(Globa_1->QT_gun_select == 1)
							{
								Globa_1->Manu_start = 2;
								Globa_1->charger_start = 2;
								Globa_1->charger_state = 0x04;
							}else if(Globa_1->QT_gun_select == 2)
							{
								Globa_2->Manu_start = 2;
								Globa_2->charger_start = 2;
								Globa_2->charger_state = 0x04;
							}

    					break;
    				}
    				case 0x03:{//停止充电按钮
							if(Globa_1->QT_gun_select == 1)
							{
								Globa_1->Manu_start = 0;
							}else if(Globa_1->QT_gun_select == 2)
							{
								Globa_2->Manu_start = 0;
							}

    					break;
    				}
    				case 0x04:{//退出 按钮
							Globa_1->Manu_start = 0;
							Globa_2->Manu_start = 0;

							msg.type = 0x3000;
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);					

							Globa_1->QT_Step = 0x31;

							Globa_1->charger_state = 0x03;
							Globa_2->charger_state = 0x03;

							break;
    				}
    				default:{
    					break;
    				}
    			}

    			break;//必须退出，不然下面同时要数新数据的话，会导致界面无法返回
    		}

				if(QT_timer_tick(500) == 1){
    			//定时刷新数据
    			Page_3600.reserve1 = 2;//只刷新实时数据
					if(Globa_1->QT_gun_select == 1)
					{
						Page_3600.NEED_V[0] =  Globa_1->Charger_param.set_voltage/100;
						Page_3600.NEED_V[1] = (Globa_1->Charger_param.set_voltage/100)>>8;
						Page_3600.NEED_C[0] =  Globa_1->Charger_param.set_current/100;
						Page_3600.NEED_C[1] = (Globa_1->Charger_param.set_current/100)>>8;
						Page_3600.time[0] = Globa_1->QT_charge_time;
						Page_3600.time[1] = Globa_1->QT_charge_time>>8;
						Page_3600.select = 1; //Globa->QT_gun_select;    //充电枪选择 1：充电枪1 2: 充电枪2

						Page_3600.BATT_V[0] = Globa_1->Output_voltage/100;
						Page_3600.BATT_V[1] = (Globa_1->Output_voltage/100)>>8;
						Page_3600.BATT_C[0] = Globa_1->Output_current/100;
						Page_3600.BATT_C[1] = (Globa_1->Output_current/100)>>8;

						Page_3600.Time[0] = Globa_1->Time;
						Page_3600.Time[1] = Globa_1->Time>>8;
						Page_3600.KWH[0] =  Globa_1->kwh;
						Page_3600.KWH[1] =  Globa_1->kwh>>8;
					}else if(Globa_1->QT_gun_select == 2)
					{
						Page_3600.NEED_V[0] =  Globa_2->Charger_param.set_voltage/100;
						Page_3600.NEED_V[1] = (Globa_2->Charger_param.set_voltage/100)>>8;
						Page_3600.NEED_C[0] =  Globa_2->Charger_param.set_current/100;
						Page_3600.NEED_C[1] = (Globa_2->Charger_param.set_current/100)>>8;
						Page_3600.time[0] = Globa_2->QT_charge_time;
						Page_3600.time[1] = Globa_2->QT_charge_time>>8;
						Page_3600.select = 2; //Globa->QT_gun_select;    //充电枪选择 1：充电枪1 2: 充电枪2

						Page_3600.BATT_V[0] = Globa_2->Output_voltage/100;
						Page_3600.BATT_V[1] = (Globa_2->Output_voltage/100)>>8;
						Page_3600.BATT_C[0] = Globa_2->Output_current/100;
						Page_3600.BATT_C[1] = (Globa_2->Output_current/100)>>8;

						Page_3600.Time[0] = Globa_2->Time;
						Page_3600.Time[1] = Globa_2->Time>>8;
						Page_3600.KWH[0] =  Globa_2->kwh;
						Page_3600.KWH[1] =  Globa_2->kwh>>8;
					}
          msg.type = 0x3600;
					memcpy(&msg.argv[0], &Page_3600.reserve1, sizeof(PAGE_3600_FRAME));
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_3600_FRAME), IPC_NOWAIT);
					
					QT_timer_start(500);
					
					break;
    		}

				break;
			}
			case 0x41://---------------4   查看帮助 状态  4.1 -----------------------
			case 0x42:{//->关于	
    		if((msg.type == 0x4000)||(msg.type == 0x2000)){//收到该界面消息
				  if(msg.type == 0x2000){
						if(msg.argv[0] == 3){
							msg.type = 0x3300;
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa_1->QT_Step = 0x311;//3    系统管理 具体设置  状态  3.1.1
					  	break;
						}else if(msg.argv[0] == 4){//进入参数设置界面
					   	msg.type = 0x3000;//系统管理界面
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa_1->QT_Step = 0x31; // 系统管理界面
							count_wait = 0;
							Globa_1->checkout = 0;
							Globa_1->pay_step = 0;
					  	break;
						}
					}
					msg.type = 0x100;
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
					Globa_1->QT_Step = 0x02; // 0 主界面 空闲( 可操作 )状态  0.1
    		}
				break;
			}
			case 0x43:{//系统忙暂停使用
				if(msg.type == 0x5000){
					if(msg.argv[0] == 1){
						msg.type = 0x3000;//系统管理界面
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_1->QT_Step = 0x31; // 系统管理界面
						count_wait = 0;
						Globa_1->checkout = 0;
						Globa_1->pay_step = 0;
						break;
					}
				}
			  if(Globa_1->Electric_pile_workstart_Enable == 0){ //禁止本桩工作---TST
					msg.type = 0x100;//在自动充电的界面切换中，只有此处才可以返回主界面（两个同时处于通道选着界面，才有此按钮功能）
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
					Globa_1->QT_Step = 0x02; //主界面
					//防止按键处理不同步，此处同步一下
					msg.type = 0x150;//
					msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
					Globa_2->QT_Step = 0x03; //主界面
					break;
				}
				break;
			}
			case 0x50:{//查询余额还是卡解锁
				if(msg.type == 0x5300){//判断是否为   主界面
    			switch(msg.argv[0]){//判断 在当前界面下 用户操作的按钮
    				case 0x01:{//卡余额查询
							msg.type = 0x200;//等待刷卡界面
							memset(&Page_200.reserve1, '0', sizeof(PAGE_200_FRAME));
							Page_200.state = 1;//1 没有检测到用户卡
							memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
							
							count_wait = 0;
							memset(&Globa_1->card_sn[0], '0', sizeof(Globa_1->card_sn));
							memset(&Globa_1->tmp_card_sn[0], '0', sizeof(Globa_1->tmp_card_sn));
							memset(&busy_frame, '0', sizeof(busy_frame));
							Globa_1->QT_Step = 0x51; // 2 自动充电-等待刷卡界面  2.0
							Globa_1->checkout = 0;
							Globa_1->pay_step = 6;
						  if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
						  	Voicechip_operation_function(0x04);//04H     “请刷卡”
							}else{
								Voicechip_operation_function(0x00);//00H	"欢迎光临.wav 300s 请刷卡1.wav"
							}
							break;
						}
						case 0x02:{ //支付卡解锁功能
							msg.type = 0x5400;//解锁界面
							memset(&Page_5200.reserve1, '0', sizeof(PAGE_5200_FRAME));
							Page_5200.Result_Type = 0;    	
							memset(&Page_5200.card_lock_time, '0', sizeof(Page_5200.card_lock_time));
							memset(&Page_5200.Deduct_money, '0', sizeof(Page_5200.Deduct_money));
							memset(&Page_5200.busy_SN, '0', sizeof(Page_5200.busy_SN));
							memcpy(&msg.argv[0], &Page_5200.reserve1, sizeof(PAGE_5200_FRAME));
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_5200_FRAME), IPC_NOWAIT);
							Globa_1->QT_Step = 0x52; // 2 支付卡解锁功能
							count_wait = 0;
							memset(&Globa_1->card_sn[0], '0', sizeof(Globa_1->card_sn));
							memset(&Globa_1->tmp_card_sn[0], '0', sizeof(Globa_1->tmp_card_sn));
							memset(&Globa_1->User_Card_Lock_Info, '0', sizeof(Globa_1->User_Card_Lock_Info));
							Globa_1->User_Card_Lock_Info.Unlock_card_setp = 0;
							Globa_1->User_Card_Lock_Info.Card_unlock_Query_Results = 0;
						  Globa_1->User_Card_Lock_Info.User_card_unlock_Requestfalg = 0;
							Globa_1->User_Card_Lock_Info.User_card_unlock_Data_Requestfalg = 0;	
							Globa_1->QT_Step = 0x52; // 2 支付卡解锁功能
		          Globa_1->checkout = 0;
							Globa_1->pay_step = 7;
              if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
						  	Voicechip_operation_function(0x04);//04H     “请刷卡”
							}else{
								Voicechip_operation_function(0x00);//00H	"欢迎光临.wav 300s 请刷卡1.wav"
							}
							break;
						}
						case 0x00:{//返回主界面
							msg.type = 0x100;
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa_1->QT_Step = 0x02; // 0 主界面 空闲( 可操作 )状态  0.1
							memset(&Globa_1->card_sn[0], '0', sizeof(Globa_1->card_sn));
							memset(&Globa_1->tmp_card_sn[0], '0', sizeof(Globa_1->tmp_card_sn));
							break;
						}
					}
				}
				break;	
			}
			case 0x51:{//->查看卡余额	
    		count_wait++;
				if(count_wait > 150){//等待时间超时30S
    			count_wait = 0;
		      msg.type = 0x5300;//帮助界面
    			msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
    			Globa_1->QT_Step = 0x50; // 支付卡查询余额/解锁
					Globa_1->checkout = 0;
					Globa_1->pay_step = 0;
  				break;//超时必须退出
  			}

				if(Globa_1->checkout == 0){//卡片处理失败，请重新操作
     			//继续寻卡
    			break;
  			}else if(Globa_1->checkout == 4){//无法识别充电卡
					if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
					  Voicechip_operation_function(0x0d);//0dH     “卡片未注册，请联系发卡单位处理”
					}
					Globa_1->checkout = 0;//清除刷卡结果标志
					count_wait = 0;
     			//等待30S超时
     			msg.type = 0x200;//等待刷卡界面
     			Page_200.state = 2;
					memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
    			msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
    			break;
  			}else if((Globa_1->checkout == 5)||(Globa_1->checkout == 6)||(Globa_1->checkout == 7)){//卡片状态异常
     			msg.type = 0x200;//等待刷卡界面
     			if(Globa_1->checkout == 5){//卡片状态异常
						if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
     			  	Voicechip_operation_function(0x0b);//0bH     “卡片已灰锁，请解锁”
					  }
						Page_200.state = 4;
     			}else if(Globa_1->checkout == 6){//卡片已使用
     				Page_200.state = 9;
     			}else if(Globa_1->checkout == 7){//非充电卡
     				Page_200.state = 8;
     			}

					memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
    			msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
    			sleep(3);
			
				  msg.type = 0x5300;//帮助界面
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
					Globa_1->QT_Step = 0x50; // 支付卡查询余额/解锁
					break;
      	}else if((Globa_1->checkout == 1)||(Globa_1->checkout == 3)){//卡片预处理成功 员工卡
					if(valid_card_check(&Globa_1->card_sn[0]) > 0){//黑卡
      			msg.type = 0x200;//等待刷卡界面
     				Page_200.state = 7;
						memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
    				msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
						if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
    				  Voicechip_operation_function(0x0c);//0cH     “卡片已挂失，请联系发卡单位处理”
					  }
						sleep(3);
						msg.type = 0x5300;//帮助界面
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_1->QT_Step = 0x50; // 支付卡查询余额/解锁
    				break;//退出，不能充电
					}

					if(Globa_1->checkout == 1){//卡片状态异常 灰锁
						Globa_1->card_type = 1; //卡类型 0:未知 1：用户卡 2：员工卡 3：维护卡卡类型 1：用户卡 2：员工卡
					}else{
						Globa_1->card_type = 2; //卡类型 0:未知 1：用户卡 2：员工卡 3：维护卡卡类型 1：用户卡 2：员工卡
					}

					msg.type = 0x200;//等待刷卡界面
     			
   				if(Globa_1->card_type == 1){
   					Page_200.state = 5;
   				}else{
   					Page_200.state = 6;
   				}

     			memcpy(&Page_200.sn[0], &Globa_1->card_sn[0], sizeof(Page_200.sn));
					sprintf(&temp_char[0], "%08d", Globa_1->last_rmb);				
     			memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
     			
					memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
    			msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
					
    			sleep(3);//时间可调整
					Globa_1->checkout = 0;
					Globa_1->pay_step = 0;//0：空闲 1：刷卡启动 2：刷卡停止预约 3：刷卡停止充电 4：刷卡结算 5，管理员卡，6，查询余额
					count_wait = 0;
					msg.type = 0x5300;//帮助界面
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
					Globa_1->QT_Step = 0x50; // 支付卡查询余额/解锁
					break;
  			}else{
  				Globa_1->checkout = 0;//清除刷卡结果标志
  			}
				break;
			}
		
		  case 0x52:{//支付卡解锁功能
			  if(msg.type == 0x5400){//判断是否为   主界面
    			switch(msg.argv[0]){//判断 在当前界面下 用户操作的按钮
    				case 0x00:{//退出界面
							count_wait = 0;
							msg.type = 0x5300;//帮助界面
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa_1->QT_Step = 0x50; // 支付卡查询余额/解锁
							
							Globa_1->checkout = 0;
					    Globa_1->pay_step = 0;
						  break;
					  }
						case 0x01:{//得到数据之后进行解锁
						  msg.type = 0x5400;//解锁界面
							memset(&Page_5200.reserve1, '0', sizeof(PAGE_5200_FRAME));
							Page_5200.Result_Type = 8;     //结果类型 0--请刷卡,1--卡片正常,2--数据核对查询当中,3--查询成功显示从后台或本地获取的信息（显示相应的数据，其他的都隐藏），4--查询成功未找到充电记录 5-查询失败--卡号无效 ，6--查询失败--无效充电桩序列号 7，MAC 出差或终端为未对时	8,支付卡解锁中9,解锁完成
							memset(&Page_5200.card_lock_time, '0', sizeof(Page_5200.card_lock_time));
							memset(&Page_5200.Deduct_money, '0', sizeof(Page_5200.Deduct_money));
							memset(&Page_5200.busy_SN, '0', sizeof(Page_5200.busy_SN));
							memcpy(&msg.argv[0], &Page_5200.reserve1, sizeof(PAGE_5200_FRAME));
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_5200_FRAME), IPC_NOWAIT);
							Globa_1->QT_Step = 0x53; // 2 支付卡解锁功能
							Globa_1->checkout = 0;
							Globa_1->pay_step = 7;
							Globa_1->User_Card_Lock_Info.Unlock_card_setp = 1;  //处于解锁步骤
							Globa_1->User_Card_Lock_Info.Start_Unlock_flag = 1; //解锁开始标志
              if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
						  	Voicechip_operation_function(0x04);//04H     “请刷卡”
							}else{
								Voicechip_operation_function(0x00);//00H	"欢迎光临.wav 300s 请刷卡1.wav"
							}
							break;
					  }
					}
				  break;
				}
				if(Globa_1->checkout == 0){//卡片处理失败，请重新操作
     			//继续寻卡
    			break;
  			}else if(Globa_1->checkout == 3){//无锁卡记录
				  msg.type = 0x5400;//解锁界面
					memset(&Page_5200.reserve1, '0', sizeof(PAGE_5200_FRAME));
					Page_5200.Result_Type = 1;     //结果类型 0--请刷卡,1--卡片正常,2--数据核对查询当中,3--查询成功显示从后台或本地获取的信息（显示相应的数据，其他的都隐藏），4--查询成功未找到充电记录 5-查询失败--卡号无效 ，6--查询失败--无效充电桩序列号 7，MAC 出差或终端为未对时	
  			  memset(&Page_5200.card_lock_time, '0', sizeof(Page_5200.card_lock_time));
  			  memset(&Page_5200.Deduct_money, '0', sizeof(Page_5200.Deduct_money));
  			  memset(&Page_5200.busy_SN, '0', sizeof(Page_5200.busy_SN));
					memcpy(&msg.argv[0], &Page_5200.reserve1, sizeof(PAGE_5200_FRAME));
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_5200_FRAME), IPC_NOWAIT);
					sleep(4);
					count_wait = 0;
					msg.type = 0x5300;//帮助界面
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
					Globa_1->QT_Step = 0x50; // 支付卡查询余额/解锁
					Globa_1->checkout = 0;
					Globa_1->pay_step = 0;
					break;
				}else if(Globa_1->checkout == 1){//查询到卡片有锁卡记录，之后需要显示出来，
					if(Globa_1->User_Card_Lock_Info.Card_unlock_Query_Results == 1){//查询成功有记录--显示数据出来
						msg.type = 0x5400;//解锁界面
						memset(&Page_5200.reserve1, '0', sizeof(PAGE_5200_FRAME));
						Page_5200.Result_Type = 3;     
						memcpy(&Page_5200.card_lock_time, &Globa_1->User_Card_Lock_Info.card_lock_time, sizeof(Page_5200.card_lock_time));
						Page_5200.Deduct_money[0] = Globa_1->User_Card_Lock_Info.Deduct_money&0xFF; 
						Page_5200.Deduct_money[1] = Globa_1->User_Card_Lock_Info.Deduct_money>>8; 
						Page_5200.Deduct_money[2] = Globa_1->User_Card_Lock_Info.Deduct_money>>16; 
						Page_5200.Deduct_money[3] = Globa_1->User_Card_Lock_Info.Deduct_money>>24; 
						
					//	memcpy(&Page_5200.Deduct_money, &Globa_1->User_Card_Lock_Info.Deduct_money, sizeof(Page_5200.Deduct_money));
						memcpy(&Page_5200.busy_SN, &Globa_1->User_Card_Lock_Info.busy_SN, sizeof(Page_5200.busy_SN));
						memcpy(&msg.argv[0], &Page_5200.reserve1, sizeof(PAGE_5200_FRAME));
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_5200_FRAME), IPC_NOWAIT);
						break;
					}else if(Globa_1->User_Card_Lock_Info.Card_unlock_Query_Results == 4){//未找到记录
					  msg.type = 0x5400;//解锁界面
					 	memset(&Page_5200.reserve1, '0', sizeof(PAGE_5200_FRAME));
						Page_5200.Result_Type = 4;    
						memset(&Page_5200.card_lock_time, '0', sizeof(Page_5200.card_lock_time));
						memset(&Page_5200.Deduct_money, '0', sizeof(Page_5200.Deduct_money));
						memset(&Page_5200.busy_SN, '0', sizeof(Page_5200.busy_SN));
						memcpy(&msg.argv[0], &Page_5200.reserve1, sizeof(PAGE_5200_FRAME));
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_5200_FRAME), IPC_NOWAIT);
						if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
						  Voicechip_operation_function(0x12);//12H     “卡片解锁失败，请联系发卡单位处理“
					  }
						sleep(3);				
						msg.type = 0x5300;//帮助界面
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_1->QT_Step = 0x50; // 支付卡查询余额/解锁
						break;
					}else if(Globa_1->User_Card_Lock_Info.Card_unlock_Query_Results == 2){//失败，卡号无效
					  msg.type = 0x5400;//解锁界面
						memset(&Page_5200.reserve1, '0', sizeof(PAGE_5200_FRAME));
						Page_5200.Result_Type = 5;     
						memset(&Page_5200.card_lock_time, '0', sizeof(Page_5200.card_lock_time));
						memset(&Page_5200.Deduct_money, '0', sizeof(Page_5200.Deduct_money));
						memset(&Page_5200.busy_SN, '0', sizeof(Page_5200.busy_SN));
						memcpy(&msg.argv[0], &Page_5200.reserve1, sizeof(PAGE_5200_FRAME));
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_5200_FRAME), IPC_NOWAIT);
						if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
						  Voicechip_operation_function(0x12);//12H     “卡片解锁失败，请联系发卡单位处理“
					  }
						sleep(3);				
						msg.type = 0x5300;//帮助界面
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_1->QT_Step = 0x50; // 支付卡查询余额/解锁
						break;
					}else if(Globa_1->User_Card_Lock_Info.Card_unlock_Query_Results == 3){//无效充电桩序列号
					  msg.type = 0x5400;//解锁界面
						memset(&Page_5200.reserve1, '0', sizeof(PAGE_5200_FRAME));
						Page_5200.Result_Type = 6;     
						memset(&Page_5200.card_lock_time, '0', sizeof(Page_5200.card_lock_time));
						memset(&Page_5200.Deduct_money, '0', sizeof(Page_5200.Deduct_money));
						memset(&Page_5200.busy_SN, '0', sizeof(Page_5200.busy_SN));
						memcpy(&msg.argv[0], &Page_5200.reserve1, sizeof(PAGE_5200_FRAME));
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_5200_FRAME), IPC_NOWAIT);
	          if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
						  Voicechip_operation_function(0x12);//12H     “卡片解锁失败，请联系发卡单位处理“
					  }
						sleep(3);				
						msg.type = 0x5300;//帮助界面
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_1->QT_Step = 0x50; // 支付卡查询余额/解锁
						break;
					}else if(Globa_1->User_Card_Lock_Info.Card_unlock_Query_Results == 5){//MAC 出差或终端为未对时
					  msg.type = 0x5400;//解锁界面
					  memset(&Page_5200.reserve1, '0', sizeof(PAGE_5200_FRAME));
						Page_5200.Result_Type = 7;     
						memset(&Page_5200.Deduct_money, '0', sizeof(Page_5200.Deduct_money));
						memset(&Page_5200.busy_SN, '0', sizeof(Page_5200.busy_SN));
						memcpy(&msg.argv[0], &Page_5200.reserve1, sizeof(PAGE_5200_FRAME));
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_5200_FRAME), IPC_NOWAIT);
	          if(Globa_1->Charger_param.NEW_VOICE_Flag == 1){
						  Voicechip_operation_function(0x12);//12H     “卡片解锁失败，请联系发卡单位处理“
					  }
						sleep(3);				
						msg.type = 0x5400;//帮助界面
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_1->QT_Step = 0x50; // 支付卡查询余额/解锁
						break;
					}else{
				    msg.type = 0x5400;//解锁界面
					  memset(&Page_5200.reserve1, '0', sizeof(PAGE_5200_FRAME));
						Page_5200.Result_Type = 2;     
						memset(&Page_5200.Deduct_money, '0', sizeof(Page_5200.Deduct_money));
						memset(&Page_5200.busy_SN, '0', sizeof(Page_5200.busy_SN));
						memcpy(&msg.argv[0], &Page_5200.reserve1, sizeof(PAGE_5200_FRAME));
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_5200_FRAME), IPC_NOWAIT);
					}
				}
			  break;
			}
			case 0x53:{
				if(msg.type == 0x5400){//判断是否为   主界面
					switch(msg.argv[0]){//判断 在当前界面下 用户操作的按钮
						case 0x00:{//退出界面
							count_wait = 0;
							msg.type = 0x5300;//帮助界面
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa_1->QT_Step = 0x50; // 支付卡查询余额/解锁
							Globa_1->checkout = 0;
					    Globa_1->pay_step = 0;
							break;
						}
					}
				}
		    if(Globa_1->checkout == 8){//两次卡号不一样
					msg.type = 0x5400;//解锁界面
					memset(&Page_5200.reserve1, 0, sizeof(PAGE_5200_FRAME));
				  Page_5200.Result_Type = 9;    	
					memset(&Page_5200.card_lock_time, 0, sizeof(Page_5200.card_lock_time));
					memset(&Page_5200.Deduct_money, 0, sizeof(Page_5200.Deduct_money));
					memset(&Page_5200.busy_SN, '0', sizeof(Page_5200.busy_SN));
					memcpy(&msg.argv[0], &Page_5200.reserve1, sizeof(PAGE_5200_FRAME));
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_5200_FRAME), IPC_NOWAIT);
					if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
					  Voicechip_operation_function(0x12);//12H     “卡片解锁失败，请联系发卡单位处理“
					}
					sleep(3);				
					msg.type = 0x5300;//帮助界面
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
					Globa_1->QT_Step = 0x50; // 支付卡查询余额/解锁
				}else if(Globa_1->checkout == 3){//解锁成功
					msg.type = 0x5400;//解锁界面
					memset(&Page_5200.reserve1, 0, sizeof(PAGE_5200_FRAME));
				  Page_5200.Result_Type = 10;    	
					memset(&Page_5200.card_lock_time, 0, sizeof(Page_5200.card_lock_time));
					memset(&Page_5200.Deduct_money, 0, sizeof(Page_5200.Deduct_money));
					memset(&Page_5200.busy_SN, '0', sizeof(Page_5200.busy_SN));
					memcpy(&msg.argv[0], &Page_5200.reserve1, sizeof(PAGE_5200_FRAME));
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_5200_FRAME), IPC_NOWAIT);
					if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
					  Voicechip_operation_function(0x11);//11H     “卡片解锁成功”
					}
					sleep(3);				
					msg.type = 0x5300;//帮助界面
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
					Globa_1->QT_Step = 0x50; // 支付卡查询余额/解锁
					Globa_1->checkout = 0;
					Globa_1->pay_step = 0;
				}
				break;
			}
			case 0x60:{//检测到升级包提示用户是否进行升级		
				if(msg.type == 0x6000){//判断是否为   主界面
    			switch(msg.argv[0]){//判断 在当前界面下 用户操作的按钮
    				case 0x01:{//进行升级处理
							msg.type = 0x6000;					
							Page_6100.reserve1 = 0;        //保留
							Page_6100.Display_prompt = 1;        //显示进度条等数据
							if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4)){
							  Page_6100.file_number = 2;          //下发文件数量
							}else{
								Page_6100.file_number = 1;
							}
							Page_6100.file_no = 1;          //当前下发文件
							Page_6100.file_Bytes[0]= 0;    //总字节数
							Page_6100.file_Bytes[1]= 0;    //总字节数
							Page_6100.file_Bytes[2]= 0;    //总字节数
							Page_6100.sendfile_Bytes[0]= 0;          //成功下发的字节数/
							Page_6100.sendfile_Bytes[1]= 0;          //成功下发的字节数/
							Page_6100.sendfile_Bytes[2]= 0;           //成功下发的字节数/
							Page_6100.over_flag[0] = 0;      //控制板1,2,升级步骤标志  0--等待升级 1--下发升级中 2--下发完成（进行结果判定）
							Page_6100.over_flag[1] = 0;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
							Page_6100.info_success[0] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
							Page_6100.info_success[1] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功

							memcpy(&msg.argv[0], &Page_6100.reserve1, sizeof(PAGE_6100_FRAME));
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_6100_FRAME), IPC_NOWAIT);
							if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
							  Voicechip_operation_function(0x13);//13H    “系统维护中，暂停使用 ”
							}
							Globa_1->QT_Step = 0x61; //退出到主界面
							timer_start_1(5000);
				      timer_start_1(250);
							count_wait = 0;
							Upgrade_Firmware_time = 0;
							Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x00;
						  Globa_2->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x00;
							Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setflg = 1;
							Globa_2->Control_Upgrade_Firmware_Info.firmware_info_setflg = 1;
					   	break;
						}
						case 0x00:
					  case 0x02:{//进行升级处理
							Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setflg = 0;
							Globa_2->Control_Upgrade_Firmware_Info.firmware_info_setflg = 0;
							Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x00;
						  Globa_2->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x00;
						  msg.type = 0x100;//退出结束界面
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);				
							Globa_1->QT_Step = 0x02; //退出到主界面
							count_wait = 0;
					   	break;
						}
	          default:
						break;
					}
				}
				count_wait++;
				if(count_wait > 100){//等待时间超时20S ---按默认升级处理
    		 count_wait = 0;
		    /* msg.type = 0x100;//退出结束界面
				 msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);				
				 Globa_1->QT_Step = 0x02; //退出到主界面
				 */
				  msg.type = 0x6000;					
					Page_6100.reserve1 = 0;        //保留
					Page_6100.Display_prompt = 1;        //显示进度条等数据
					if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4)){
						Page_6100.file_number = 2;          //下发文件数量
					}else{
						Page_6100.file_number = 1;
					}
					Page_6100.file_no = 1;          //当前下发文件
					Page_6100.file_Bytes[0]= 0;    //总字节数
					Page_6100.file_Bytes[1]= 0;    //总字节数
					Page_6100.file_Bytes[2]= 0;    //总字节数
					Page_6100.sendfile_Bytes[0]= 0;          //成功下发的字节数/
					Page_6100.sendfile_Bytes[1]= 0;          //成功下发的字节数/
					Page_6100.sendfile_Bytes[2]= 0;           //成功下发的字节数/
					Page_6100.over_flag[0] = 0;      //控制板1,2,升级步骤标志  0--等待升级 1--下发升级中 2--下发完成（进行结果判定）
					Page_6100.over_flag[1] = 0;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
					Page_6100.info_success[0] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
					Page_6100.info_success[1] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功

					memcpy(&msg.argv[0], &Page_6100.reserve1, sizeof(PAGE_6100_FRAME));
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_6100_FRAME), IPC_NOWAIT);
	        if( Globa_1->Charger_param.NEW_VOICE_Flag == 1){
					  Voicechip_operation_function(0x13);//13H    “系统维护中，暂停使用 ”
					}
					Globa_1->QT_Step = 0x61; //退出到主界面
					timer_start_1(5000);
					timer_start_1(250);
					count_wait = 0;
					Upgrade_Firmware_time = 0;
					Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x00;
					Globa_2->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x00;
					Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setflg = 1;
					Globa_2->Control_Upgrade_Firmware_Info.firmware_info_setflg = 1;
				 
  			 break;//超时必须退出
  			}
				break;
			}
			case 0x61:{//程序下发升级包过程
				count_wait++;
				if(Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setStep != 0x10){
					if(count_wait >= 5){//等待时间超时1S  ----每1S发送一次
						count_wait = 0;
						msg.type = 0x6000;					
						Page_6100.reserve1 = 0;        //保留
						Page_6100.Display_prompt = 1;        //显示进度条等数据
					/*	if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4)){
							Page_6100.file_number = 2;          //下发文件数量
						}else{
							Page_6100.file_number = 1;
						}
						if(Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setStep <= 0x02){
							Page_6100.file_no = 1;          //当前下发文件
						}else if(Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setStep <= 0x05){
							Page_6100.file_no = 2;          //当前下发文件
						}*/
		        
						if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4)){
							Page_6100.file_number = 2;          //下发文件数量
							if(Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setStep <= 0x02){
								Page_6100.file_no = 1;          //当前下发文件
							}else if(Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setStep <= 0x05){
								Page_6100.file_no = 2;          //当前下发文件
							}
						}else{
							Page_6100.file_number = 1;
							Page_6100.file_no = 1;          //当前下发文件
						}
						
						
						if(Page_6100.file_no == 1){
							Page_6100.file_Bytes[0]= Globa_1->Control_Upgrade_Firmware_Info.Firmware_ALL_Bytes;    //总字节数
							Page_6100.file_Bytes[1]= Globa_1->Control_Upgrade_Firmware_Info.Firmware_ALL_Bytes>>8;    //总字节数
							Page_6100.file_Bytes[2]= Globa_1->Control_Upgrade_Firmware_Info.Firmware_ALL_Bytes>>16;    //总字节数
							Page_6100.sendfile_Bytes[0]= Globa_1->Control_Upgrade_Firmware_Info.Send_Tran_All_Bytes_0K;          //成功下发的字节数/
							Page_6100.sendfile_Bytes[1]= Globa_1->Control_Upgrade_Firmware_Info.Send_Tran_All_Bytes_0K>>8;          //成功下发的字节数/
							Page_6100.sendfile_Bytes[2]= Globa_1->Control_Upgrade_Firmware_Info.Send_Tran_All_Bytes_0K>>16;           //成功下发的字节数/
							Page_6100.over_flag[0] = 1;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
							Page_6100.over_flag[1] = 0;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
							Page_6100.info_success[0] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
							Page_6100.info_success[1] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
						}else{
							Page_6100.file_Bytes[0]= Globa_2->Control_Upgrade_Firmware_Info.Firmware_ALL_Bytes;    //总字节数
							Page_6100.file_Bytes[1]= Globa_2->Control_Upgrade_Firmware_Info.Firmware_ALL_Bytes>>8;    //总字节数
							Page_6100.file_Bytes[2]= Globa_2->Control_Upgrade_Firmware_Info.Firmware_ALL_Bytes>>16;    //总字节数
							Page_6100.sendfile_Bytes[0]= Globa_2->Control_Upgrade_Firmware_Info.Send_Tran_All_Bytes_0K;          //成功下发的字节数/
							Page_6100.sendfile_Bytes[1]= Globa_2->Control_Upgrade_Firmware_Info.Send_Tran_All_Bytes_0K>>8;          //成功下发的字节数/
							Page_6100.sendfile_Bytes[2]= Globa_2->Control_Upgrade_Firmware_Info.Send_Tran_All_Bytes_0K>>16;           //成功下发的字节数/
							Page_6100.over_flag[0] = 2;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
							Page_6100.over_flag[1] = 1;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
							Page_6100.info_success[0] = Globa_1->Control_Upgrade_Firmware_Info.firmware_info_success;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
							Page_6100.info_success[1] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
						}
						memcpy(&msg.argv[0], &Page_6100.reserve1, sizeof(PAGE_6100_FRAME));
						msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_6100_FRAME), IPC_NOWAIT);
           }
				}else{
					msg.type = 0x6000;					
					Page_6100.reserve1 = 0;        //保留
					Page_6100.Display_prompt = 2;        //显示进度条等数据并且通知结果
					if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4)){
						Page_6100.file_number = 2;          //下发文件数量
					}else{
						Page_6100.file_number = 1;
					}
					if(Page_6100.file_no == 1){
						Page_6100.file_Bytes[0]= Globa_1->Control_Upgrade_Firmware_Info.Firmware_ALL_Bytes;    //总字节数
						Page_6100.file_Bytes[1]= Globa_1->Control_Upgrade_Firmware_Info.Firmware_ALL_Bytes>>8;    //总字节数
						Page_6100.file_Bytes[2]= Globa_1->Control_Upgrade_Firmware_Info.Firmware_ALL_Bytes>>16;    //总字节数
						Page_6100.sendfile_Bytes[0]= Globa_1->Control_Upgrade_Firmware_Info.Send_Tran_All_Bytes_0K;          //成功下发的字节数/
						Page_6100.sendfile_Bytes[1]= Globa_1->Control_Upgrade_Firmware_Info.Send_Tran_All_Bytes_0K>>8;          //成功下发的字节数/
						Page_6100.sendfile_Bytes[2]= Globa_1->Control_Upgrade_Firmware_Info.Send_Tran_All_Bytes_0K>>16;           //成功下发的字节数/
					 
						Page_6100.over_flag[0] = 2;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
						Page_6100.over_flag[1] = 0;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
						Page_6100.info_success[0] = Globa_1->Control_Upgrade_Firmware_Info.firmware_info_success;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
						Page_6100.info_success[1] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
					}else{
						Page_6100.file_Bytes[0]= Globa_2->Control_Upgrade_Firmware_Info.Firmware_ALL_Bytes;    //总字节数
						Page_6100.file_Bytes[1]= Globa_2->Control_Upgrade_Firmware_Info.Firmware_ALL_Bytes>>8;    //总字节数
						Page_6100.file_Bytes[2]= Globa_2->Control_Upgrade_Firmware_Info.Firmware_ALL_Bytes>>16;    //总字节数
						Page_6100.sendfile_Bytes[0]= Globa_2->Control_Upgrade_Firmware_Info.Send_Tran_All_Bytes_0K;          //成功下发的字节数/
						Page_6100.sendfile_Bytes[1]= Globa_2->Control_Upgrade_Firmware_Info.Send_Tran_All_Bytes_0K>>8;          //成功下发的字节数/
						Page_6100.sendfile_Bytes[2]= Globa_2->Control_Upgrade_Firmware_Info.Send_Tran_All_Bytes_0K>>16;           //成功下发的字节数/
						Page_6100.over_flag[0] = 2;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
						Page_6100.over_flag[1] = 2;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
						Page_6100.info_success[0] = Globa_1->Control_Upgrade_Firmware_Info.firmware_info_success;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
						Page_6100.info_success[1] = Globa_2->Control_Upgrade_Firmware_Info.firmware_info_success;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
					}
					memcpy(&msg.argv[0], &Page_6100.reserve1, sizeof(PAGE_6100_FRAME));
					msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_6100_FRAME), IPC_NOWAIT);
					sleep(4);
					
					if(Upgrade_Firmware_time == 0){//着手升级第二次，
					  if(((Page_6100.file_no == 1)&&(Page_6100.info_success[0] == 1))||((Page_6100.file_no == 2)&&((Page_6100.info_success[0] == 1)||(Page_6100.info_success[1] == 1)))){
							Upgrade_Firmware_time = 1;
							Page_6100.Display_prompt = 3;        //提示是否重新升级
							memcpy(&msg.argv[0], &Page_6100.reserve1, sizeof(PAGE_6100_FRAME));
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_6100_FRAME), IPC_NOWAIT);
							Globa_1->QT_Step = 0x62; // 支付卡查询余额/解锁
						}else{
							system("rm -rf /mnt/Nand1/update/EASE_DC_CTL.bin");
							system("echo Upgrade ok >> /mnt/systemlog&& date >> /mnt/systemlog");
							system("reboot");
						}
					}else{
						if(((Page_6100.file_no == 1)&&(Page_6100.info_success[0] == 1))||((Page_6100.file_no == 2)&&((Page_6100.info_success[0] == 1)||(Page_6100.info_success[1] == 1)))){
		          system("rm -rf /mnt/Nand1/update/EASE_DC_CTL.bin");
							system("echo Upgrade failed >> /mnt/systemlog&& date >> /mnt/systemlog");
							system("reboot");
						}else{
							system("rm -rf /mnt/Nand1/update/EASE_DC_CTL.bin");
							system("echo Upgrade ok >> /mnt/systemlog&& date >> /mnt/systemlog");
							system("reboot");
						}
					}
			  }
			  break;	
			}
			case 0x62:{//结果判定，是否进行第二次升级
				if(msg.type == 0x6000){//判断是否为   主界面
    			switch(msg.argv[0]){//判断 在当前界面下 用户操作的按钮
    				case 0x03:{//进行再次升级
							msg.type = 0x6000;					
							Page_6100.reserve1 = 0;        //保留
							Page_6100.Display_prompt = 1;        //显示进度条等数据
							if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4)){
							  Page_6100.file_number = 2;          //下发文件数量
							}else{
								Page_6100.file_number = 1;
							}
							Page_6100.file_Bytes[0]= 0;    //总字节数
							Page_6100.file_Bytes[1]= 0;    //总字节数
							Page_6100.file_Bytes[2]= 0;    //总字节数
							Page_6100.sendfile_Bytes[0]= 0;          //成功下发的字节数/
							Page_6100.sendfile_Bytes[1]= 0;          //成功下发的字节数/
							Page_6100.sendfile_Bytes[2]= 0;           //成功下发的字节数/
							if(Page_6100.file_no == 1){
								Page_6100.over_flag[0] = 0;      //控制板1,2,升级步骤标志  0--等待升级 1--下发升级中 2--下发完成（进行结果判定）
								Page_6100.over_flag[1] = 0;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.info_success[0] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[1] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
						  }else{
								Page_6100.over_flag[0] = 2;      //控制板1,2,升级步骤标志  0--等待升级 1--下发升级中 2--下发完成（进行结果判定）
								Page_6100.over_flag[1] = 0;      //控制板1,2,升级步骤标志  0--等待下发 1--下发中 2--下发完成（进行结果判定）
								Page_6100.info_success[0] = 2;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
								Page_6100.info_success[1] = 0;  //控制板1,2,下发情况,结果 0--未进行判断 1---升级失败 2---升级成功
							}
							memcpy(&msg.argv[0], &Page_6100.reserve1, sizeof(PAGE_6100_FRAME));
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, sizeof(PAGE_6100_FRAME), IPC_NOWAIT);
							
	            if(Globa_1->Charger_param.NEW_VOICE_Flag == 1){
							  Voicechip_operation_function(0x13);//13H    “系统维护中，暂停使用 ”
							}
							Globa_1->QT_Step = 0x61; //退出到主界面
							timer_start_1(5000);
				      timer_start_1(250);
							count_wait = 0;
					
							if(Page_6100.file_no == 1){
								Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x00;
						    Globa_2->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x00;
							}else{
								Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x03;
								Globa_2->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x03;
							}
							Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setflg = 1;
							Globa_2->Control_Upgrade_Firmware_Info.firmware_info_setflg = 1;
					   	break;
						}
						case 0x04:{
						  system("rm -rf /mnt/Nand1/update/EASE_DC_CTL.bin");
						  system("echo Upgrade failed >> /mnt/systemlog&& date >> /mnt/systemlog");
						  system("reboot");
							break;
						}
				  }
				}
				break;
			}
			case 0x70:{//调试参数界面
				if(msg.type == 0x7000){//判断是否为   主界面
    			switch(msg.argv[0]){//判断 在当前界面下 用户操作的按钮
    				case 0x02:{
							PAGE_7000_FRAME *Page_7000_temp = (PAGE_7000_FRAME *)&msg.argv[0];
						  if(0 != memcmp(&Globa_1->Charger_Shileld_Alarm, &Page_7000_temp->Charger_Shileld_Alarm, sizeof(Globa_1->Charger_Shileld_Alarm))){
								memcpy(&Globa_1->Charger_Shileld_Alarm,&Page_7000_temp->Charger_Shileld_Alarm, (sizeof(Globa_1->Charger_Shileld_Alarm)));
							  System_Charger_Shileld_Alarm_save();
								usleep(200000);
								system("sync");
							  system("reboot");
							}
							break;
					  }
						case 0x00:{
						  msg.type = 0x100;//退出结束界面
							msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);				
							Globa_1->QT_Step = 0x02; //退出到主界面
							count_wait = 0;
					   	break;
						}
						case 0x01:{//返回上一层
							msg.type = 0x3000;
      				msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);  				
      				Globa_1->QT_Step = 0x31; // 3     系统管理 选着  状态  3.1
					   	break;
						}
					}
				}
				break;
			}
			default:{
				printf("操作界面进入未知界面状态!!!!!!!!!!!!!!\n");
				break;
			}
		}
	}
}
