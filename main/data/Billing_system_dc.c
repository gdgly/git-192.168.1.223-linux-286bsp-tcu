/*******************************************************************************
  Copyright(C)    2017-2017,      EAST Co.Ltd.
  File name :     core.c
  Author :         dengjh
  Version:        1.0
  Date:           2017.4.11
  Description:    
  Other:          
  Function List:  
  History:        
          <author>   <time>   <version>   <desc>
             dengjh    2017.5.21   1.0      Create
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
#include <sys/prctl.h>
#include "globalvar.h"
#include "iso8583.h"
#include "../../inc/sqlite/sqlite3.h"
 
#include "common.h"
#include "Deal_Handler.h"
unsigned char pre_SOC[5];
extern unsigned char ac_out;//交流节能当前输出状态
//用当前分时电价初始化私有电价
void InitDefaultPrivatePrice(unsigned char port)
{	
	int i=0;

	Globa->Control_DC_Infor_Data[port].Special_price_data.Serv_Type = 3;  //服务费收费类型 1：按次收费；2：按时间收费(单位：分/10分钟；3：按电量
	
	for(i=0;i<4;i++)
	{
		Globa->Control_DC_Infor_Data[port].Special_price_data.share_time_kwh_price[i]       = Globa->Charger_param2.share_time_kwh_price[i];//分时电价
		Globa->Control_DC_Infor_Data[port].Special_price_data.share_time_kwh_price_serve[i] = Globa->Charger_param2.share_time_kwh_price_serve[i]; //分时服务费
	}
	Globa->Control_DC_Infor_Data[port].Special_price_data.time_zone_num = Globa->Charger_param2.time_zone_num ;  //时段数
	
	for(i=0;i<12;i++)
	{
		Globa->Control_DC_Infor_Data[port].Special_price_data.time_zone_tabel[i] = Globa->Charger_param2.time_zone_tabel[i];//时段表
		Globa->Control_DC_Infor_Data[port].Special_price_data.time_zone_feilv[i] = Globa->Charger_param2.time_zone_feilv[i];//时段表对应的价格索引		
	}	
}
//获取当前充电枪当前时段的费率号0---3	
//port为枪号索引 0开始
static unsigned char GetCurPriceIndex(unsigned char port)
{
	unsigned char cur_price_index = 0,i,cur_time_zone_num = 0,time_zone_index = 0;
	unsigned short cur_time;//高字节为当前小时数，低字节为当前分钟数，BIN码

	time_t systime;
	struct tm tm_t;
	systime = time(NULL);        //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	

	cur_time = (tm_t.tm_hour<<8 ) | tm_t.tm_min;
	{//通用时区
		cur_time_zone_num = Globa->Control_DC_Infor_Data[port].Special_price_data.time_zone_num;
		for(i=0;i<cur_time_zone_num;i++){
			if((cur_time >= Globa->Control_DC_Infor_Data[port].Special_price_data.time_zone_tabel[i]) && (cur_time < Globa->Control_DC_Infor_Data[port].Special_price_data.time_zone_tabel[(i+1)%cur_time_zone_num]))//开始点为闭区间,可以取等[1段开始,1段结束)，[2段开始,2段结束)..."1段结束"值=="2段开始"值
				break;
		}
		if(i<cur_time_zone_num){
			time_zone_index = i + 1;//since 1 
		}else{//最后1个时段
			time_zone_index = cur_time_zone_num;
		}
		cur_price_index = Globa->Control_DC_Infor_Data[port].Special_price_data.time_zone_feilv[time_zone_index -1];//取得第1个时间段的费率号 0----3
	}	
	
	return cur_price_index;
	
}


//获取当前充电枪当前时段的费率号0---3	
//port为枪号索引 0开始
static unsigned char GetTimePriceIndex(unsigned char port,unsigned short cur_time)
{
	unsigned char cur_price_index = 0,i,cur_time_zone_num = 0,time_zone_index = 0;
	//unsigned short cur_time;//高字节为当前小时数，低字节为当前分钟数，BIN码

	{//通用时区
		cur_time_zone_num = Globa->Control_DC_Infor_Data[port].Special_price_data.time_zone_num;
		for(i=0;i<cur_time_zone_num;i++){
			if((cur_time >= Globa->Control_DC_Infor_Data[port].Special_price_data.time_zone_tabel[i]) && (cur_time < Globa->Control_DC_Infor_Data[port].Special_price_data.time_zone_tabel[(i+1)%cur_time_zone_num]))//开始点为闭区间,可以取等[1段开始,1段结束)，[2段开始,2段结束)..."1段结束"值=="2段开始"值
				break;
		}
		if(i<cur_time_zone_num){
			time_zone_index = i + 1;//since 1 
		}else{//最后1个时段
			time_zone_index = cur_time_zone_num;
		}
		cur_price_index = Globa->Control_DC_Infor_Data[port].Special_price_data.time_zone_feilv[time_zone_index -1];//取得第1个时间段的费率号 0----3
	}	
	
	return cur_price_index;
	
}


//返回值1表示VIN字符有效
unsigned char ValidVIN_Check(unsigned char VIN[])
{
	int i,j=0;
	for(i=0;i<17;i++)
	{
		if( ((VIN[i] >= '0')&&(VIN[i] <= '9'))||//字符0到9
			((VIN[i] >= 'A')&&(VIN[i] <= 'Z'))||
			((VIN[i] >= 'a')&&(VIN[i] <= 'z'))||
			(VIN[i] == '-'))//模拟器发的带该字符，实际不应该有
			{
				j++;
			}
	}
	if(j == 17)//all valid
	{
		if(0==memcmp(VIN,"00000000000000000",17))//无效的VIN		
			return 0;		
		else
			return 1;
	}
	else
		return 0;
	
}

//初始化ISO8583_charge_db_data结构的变量,port从0到3
static void InitChgingBusyData(unsigned char port,unsigned char chg_start_by,ISO8583_charge_db_data* p_busy_frame)
{
	int i;
	time_t systime;
	struct tm tm_t;
	char temp_char[64];
	memset(p_busy_frame,0,sizeof(ISO8583_charge_db_data));
	memcpy(p_busy_frame->charger_SN, Globa->Charger_param.SN, 16);//桩编号12位数字
	
	p_busy_frame->chg_port = port + 1;	//交易记录枪号从1开始
	memset(p_busy_frame->deal_SN ,  '0'  , 20);//预置流水号字符全0
	if(CHG_BY_APP == chg_start_by)//记住APP账户和充电前余额
	{
		memcpy(p_busy_frame->deal_SN ,  Globa->Control_DC_Infor_Data[port].App.busy_SN  , 20);//流水号20位
		p_busy_frame->chg_start_by = CHG_BY_APP;
		memcpy(p_busy_frame->card_sn , Globa->Control_DC_Infor_Data[port].App.ID ,16);	//用户号16位
		p_busy_frame->end_reason = BY_POWER_DOWN;//防止断电或复位结束原因为0值	
		p_busy_frame->last_rmb = Globa->Control_DC_Infor_Data[port].App.last_rmb;
		p_busy_frame->money_left = p_busy_frame->last_rmb;
		systime = time(NULL);         //获得系统的当前时间 
		localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
		sprintf(temp_char, "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
			tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
			
	}
	if(CHG_BY_CARD == chg_start_by)
	{
		p_busy_frame->chg_start_by = CHG_BY_CARD;
		p_busy_frame->end_reason = BY_POWER_DOWN;//防止断电或复位结束原因为0值
		memcpy(p_busy_frame->card_sn, Globa->Control_DC_Infor_Data[port].card_sn, 16);
		p_busy_frame->last_rmb = Globa->Control_DC_Infor_Data[port].last_rmb;
		p_busy_frame->money_left = p_busy_frame->last_rmb;
		//用锁卡时间
		if((Globa->Control_DC_Infor_Data[port].card_type == 1)&&(0xAA != Globa->Control_DC_Infor_Data[port].qiyehao_flag))//用户卡且没有企业卡属性
		{
			p_busy_frame->card_deal_result = 1;//充电锁卡中
			sprintf(temp_char, "%02d%02d%02d%02d%02d%02d%02d", 
					Globa->User_Card_Lock_Info.card_lock_time.year_H, 
					Globa->User_Card_Lock_Info.card_lock_time.year_L, 
					Globa->User_Card_Lock_Info.card_lock_time.month,
					Globa->User_Card_Lock_Info.card_lock_time.day_of_month , 
					Globa->User_Card_Lock_Info.card_lock_time.hour, 
					Globa->User_Card_Lock_Info.card_lock_time.minute,
					Globa->User_Card_Lock_Info.card_lock_time.second);
		}else
		{
			systime = time(NULL);         //获得系统的当前时间 
			localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
			sprintf(temp_char, "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
														   tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
		}
	}
	if(CHG_BY_VIN == chg_start_by)//VIN鉴权
	{
		p_busy_frame->chg_start_by = CHG_BY_VIN;
		p_busy_frame->end_reason = BY_POWER_DOWN;//防止断电或复位结束原因为0值
		Globa->Control_DC_Infor_Data[port].last_rmb = Globa->Control_DC_Infor_Data[port].VIN_auth_money;
		p_busy_frame->last_rmb = Globa->Control_DC_Infor_Data[port].last_rmb;
		p_busy_frame->money_left = p_busy_frame->last_rmb;
		systime = time(NULL);         //获得系统的当前时间 
		localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
		sprintf(temp_char, "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
														   tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
		
	}
	if(1 == p_busy_frame->card_deal_result )//用户卡锁卡---add 190929
	{
		sprintf(Globa->Control_DC_Infor_Data[port].charge_start_time,"%02d%02d-%02d-%02d %02d:%02d:%02d",
					Globa->User_Card_Lock_Info.card_lock_time.year_H, 
					Globa->User_Card_Lock_Info.card_lock_time.year_L, 
					Globa->User_Card_Lock_Info.card_lock_time.month,
					Globa->User_Card_Lock_Info.card_lock_time.day_of_month , 
					Globa->User_Card_Lock_Info.card_lock_time.hour, 
					Globa->User_Card_Lock_Info.card_lock_time.minute,
					Globa->User_Card_Lock_Info.card_lock_time.second);
	}	
	else
		sprintf(Globa->Control_DC_Infor_Data[port].charge_start_time,"%04d-%02d-%02d %02d:%02d:%02d",tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
														   tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
	memcpy(p_busy_frame->start_time,temp_char,14);
	memcpy(p_busy_frame->end_time,temp_char,14);	
		
	p_busy_frame->start_kwh = Globa->Control_DC_Infor_Data[port].meter_start_KWH;
	p_busy_frame->end_kwh   = Globa->Control_DC_Infor_Data[port].meter_stop_KWH;
	
	p_busy_frame->kwh1_price = Globa->Control_DC_Infor_Data[port].Special_price_data.share_time_kwh_price[0];
	p_busy_frame->kwh2_price = Globa->Control_DC_Infor_Data[port].Special_price_data.share_time_kwh_price[1];
	p_busy_frame->kwh3_price = Globa->Control_DC_Infor_Data[port].Special_price_data.share_time_kwh_price[2];
	p_busy_frame->kwh4_price = Globa->Control_DC_Infor_Data[port].Special_price_data.share_time_kwh_price[3];
	
	p_busy_frame->kwh1_service_price = Globa->Control_DC_Infor_Data[port].Special_price_data.share_time_kwh_price_serve[0];
	p_busy_frame->kwh2_service_price = Globa->Control_DC_Infor_Data[port].Special_price_data.share_time_kwh_price_serve[1];
	p_busy_frame->kwh3_service_price = Globa->Control_DC_Infor_Data[port].Special_price_data.share_time_kwh_price_serve[2];
	p_busy_frame->kwh4_service_price = Globa->Control_DC_Infor_Data[port].Special_price_data.share_time_kwh_price_serve[3];
	
	p_busy_frame->book_price = Globa->Charger_param2.book_price;
	p_busy_frame->park_price = Globa->Charger_param2.charge_stop_price.stop_price;
	p_busy_frame->Start_Soc = 0;
	p_busy_frame->End_Soc = 0;
	
		
}

//更新充电中的交易记录信息
static void UpdateChgingBusyData(unsigned char port,ISO8583_charge_db_data* p_busy_frame)
{
	int i,money_left;
	time_t systime;
	struct tm tm_t;
	char temp_char[64];
	unsigned long long ull_temp;
	if(CHG_BY_VIN == p_busy_frame->chg_start_by)//VIN鉴权
	{
		Globa->Control_DC_Infor_Data[port].last_rmb = Globa->Control_DC_Infor_Data[port].VIN_auth_money;
		p_busy_frame->last_rmb = Globa->Control_DC_Infor_Data[port].last_rmb;
	}
	if(ValidVIN_Check(Globa->Control_DC_Infor_Data[port].VIN))//全程更新1次====test
		memcpy(p_busy_frame->car_VIN ,Globa->Control_DC_Infor_Data[port].VIN , 17)  ;//
	
	p_busy_frame->end_kwh =  Globa->Control_DC_Infor_Data[port].meter_stop_KWH;
	p_busy_frame->kwh_total_charged = Globa->Control_DC_Infor_Data[port].kwh;
	
	p_busy_frame->total_kwh_money = Globa->Control_DC_Infor_Data[port].rmb;
	
	if(Globa->Control_DC_Infor_Data[port].kwh != 0)	
		p_busy_frame->rmb_kwh = (Globa->Control_DC_Infor_Data[port].rmb*100*METER_READ_MULTIPLE/(Globa->Control_DC_Infor_Data[port].kwh)); //平均电价 
	else
		p_busy_frame->rmb_kwh = 0; 

	p_busy_frame->all_chg_money = Globa->Control_DC_Infor_Data[port].total_rmb;
	p_busy_frame->park_money = Globa->Control_DC_Infor_Data[port].park_rmb;
	
	
	systime = time(NULL);         //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	       				
	sprintf(temp_char, "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
						 tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
	memcpy(p_busy_frame->end_time, temp_char, 14);
	sprintf(Globa->Control_DC_Infor_Data[port].charge_end_time,"%04d-%02d-%02d %02d:%02d:%02d",tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
														   tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
	//更新分时充电量，充电费用，充电服务费
	p_busy_frame->kwh1_charged = Globa->Control_DC_Infor_Data[port].ulcharged_kwh[0];
	p_busy_frame->kwh2_charged = Globa->Control_DC_Infor_Data[port].ulcharged_kwh[1];
	p_busy_frame->kwh3_charged = Globa->Control_DC_Infor_Data[port].ulcharged_kwh[2];
	p_busy_frame->kwh4_charged = Globa->Control_DC_Infor_Data[port].ulcharged_kwh[3];
	
	p_busy_frame->kwh1_money = Globa->Control_DC_Infor_Data[port].ulcharged_rmb[0];
	p_busy_frame->kwh2_money = Globa->Control_DC_Infor_Data[port].ulcharged_rmb[1];
	p_busy_frame->kwh3_money = Globa->Control_DC_Infor_Data[port].ulcharged_rmb[2];
	p_busy_frame->kwh4_money = Globa->Control_DC_Infor_Data[port].ulcharged_rmb[3];
	
	p_busy_frame->kwh1_service_money = Globa->Control_DC_Infor_Data[port].ulcharged_service_rmb[0];
	p_busy_frame->kwh2_service_money = Globa->Control_DC_Infor_Data[port].ulcharged_service_rmb[1];
	p_busy_frame->kwh3_service_money = Globa->Control_DC_Infor_Data[port].ulcharged_service_rmb[2];
	p_busy_frame->kwh4_service_money = Globa->Control_DC_Infor_Data[port].ulcharged_service_rmb[3];
	
	if( p_busy_frame->last_rmb > 0)	
		p_busy_frame->money_left = p_busy_frame->last_rmb - Globa->Control_DC_Infor_Data[port].total_rmb;
	else
		p_busy_frame->money_left = 0;
	
	p_busy_frame->Start_Soc = Globa->Control_DC_Infor_Data[port].BMS_batt_start_SOC; 
	if(0 != Globa->Control_DC_Infor_Data[port].BMS_batt_SOC)//防止起始soc有，结束soc为0??
	{
		if(Globa->Control_DC_Infor_Data[port].BMS_batt_SOC >= Globa->Control_DC_Infor_Data[port].BMS_batt_start_SOC)
			p_busy_frame->End_Soc = Globa->Control_DC_Infor_Data[port].BMS_batt_SOC;
		else
			p_busy_frame->End_Soc = p_busy_frame->Start_Soc;
	}		
	else
	{
		if(p_busy_frame->End_Soc < p_busy_frame->Start_Soc)//小于再赋值20200310 防止BMS_batt_SOC为0的时候直接将start赋值给end
			p_busy_frame->End_Soc = p_busy_frame->Start_Soc;
	}
}

void BMS_Volt_Sel(UINT32 Gun_No)
{
	switch(Gun_No)
	{
		case 0:
			if(Globa->Control_DC_Infor_Data[0].BMS_Power_V == 1)//24V
				GROUP1_BMS_Power24V_ON;
			else
				GROUP1_BMS_Power24V_OFF;	
		break;
		case 1:
			if(Globa->Control_DC_Infor_Data[1].BMS_Power_V == 1)//24V
				GROUP2_BMS_Power24V_ON;
			else
				GROUP2_BMS_Power24V_OFF;	
		break;
		case 2:
			if(Globa->Control_DC_Infor_Data[2].BMS_Power_V == 1)//24V
				GROUP3_BMS_Power24V_ON;
			else
				GROUP3_BMS_Power24V_OFF;	
		break;
		case 3:
			if(Globa->Control_DC_Infor_Data[3].BMS_Power_V == 1)//24V
				GROUP4_BMS_Power24V_ON;
			else
				GROUP4_BMS_Power24V_OFF;	
		break;
	}
}


int Get_rand()
{
	static  unsigned long pre_count = 0;
	
	if(abs(GetTickCount() - pre_count) >= 2)
  {
		pre_count = GetTickCount();
		return rand();
	}else{
		return 0;
	}
}

int Billing_system_DC_Task(void* gun_no)
{
	int Ret=0, OLD_Metering_Step=0;

	UINT32 Gun_No = *((UINT32*)gun_no);
	UINT32 i =0;
	
	UINT8  chg_db_sel = Gun_No+1;//充电枪交易记录数据库编号从1开始
	if(chg_db_sel > CONTROL_DC_NUMBER)
		return 0;
	ISO8583_charge_db_data s_busy_frame,pre_old_busy_frame;
	int abnormal_Log_en = 1;

	UINT32 temp_kwh = 0,check_db_count= 0;
	UINT32 cur_meter_kwh;
	UINT32 delta_kwh,share_kwh_sum,delta_kwh2;
	int id = 0;//当前充电枪的记录号
	int all_id = 0;//插入总表的记录号
	int Insertflag = 0;
	UINT32 up_num = 0,count_wait= 0,need_wait_module_poweron,soft_KWH;
	UINT8 temp_char[256];  //
	UINT8 check_bmsVin = 0;//是否判断卡与车的VIN是否匹配
	UINT8 first_get_bms_info = 0;//充电后首次获取到VIN
	UINT8 check_current_low_st = 0;//判断电流低
	UINT8 check_current_low_st2 = 0;//判断SOC达到设定限值后，电流低于设定值后停机
	UINT32 current_low_timestamp = 0,current_low_timestamp2=0,current_overkwh_timestamp = 0,Electrical_abnormality =  0;
	UINT32 check_meter_volt_st;
	UINT32 check_meter_current_st;
	UINT32 volt_abnormal_timestamp,current_abnormal_timestamp;
	//////////////////////////////////////////////
	//当前使用的资费策略
	UINT32 share_time_kwh_price[4] = {0,0,0,0};
	//UINT32 service_price;
	UINT32 share_service_price[4]={0,0,0,0};
	UINT32 park_price;
	UINT32 cur_price_index = 0;  //当前处于收费电价阶段
	UINT32 pre_price_index = 0;  //之前处于的那个时刻
	UINT32 ul_start_kwh =0; //每个峰的起始时刻
	unsigned long long ull_charged_rmb[4];//0.01kwh*0.00001yuan，32位变量在2元/度时，214度多就会导致32位变量溢出!
	unsigned long long ull_kwh_rmb,ull_service_rmb[4],ull_total_service_rmb,ull_temp;//服务费分时 7位小数
	unsigned long ul_kwh_charge_rmb,ul_kwh_service_rmb;//充电电费和服务费 0.01元
	///////////////////////////////////////////////
	
	sprintf(temp_char,"DC%d_Bill_task",chg_db_sel);
	prctl(PR_SET_NAME,(unsigned long)temp_char);//设置线程名字
	
	Globa->Control_DC_Infor_Data[Gun_No].Metering_Step = 0;
	Globa->Control_DC_Infor_Data[Gun_No].gun_state = 0x03;
	time_t systime=0;
	struct tm tm_t;
	//GROUP1_FAULT_LED_OFF;
	do{
		sleep(1);//等待电表通信正常
	}while(0 == Globa->Control_DC_Infor_Data[Gun_No].meter_KWH_update_flag);
	
	//1: 查询意外掉电的不完整数据记录，然后根据条件更新数据记录
	pthread_mutex_lock(&busy_db_pmutex);
	id = Select_0X55_0X66_Busy_ID(chg_db_sel,&s_busy_frame, &up_num);//查询意外掉电的不完整数据记录
	if(id != -1)
	{	
		if(up_num == 0x55)
		{
			cur_meter_kwh = Globa->Control_DC_Infor_Data[Gun_No].meter_KWH;
			temp_kwh = s_busy_frame.end_kwh;
			//根据结束时间取出最后充电的电价区间，将电量统计到对应时段去==============
			unsigned short last_end_hhmm = (s_busy_frame.end_time[8]-'0')*10+(s_busy_frame.end_time[9]-'0');//hour
			last_end_hhmm <<= 8;
			last_end_hhmm |= ((s_busy_frame.end_time[10]-'0')*10+(s_busy_frame.end_time[11]-'0'));//mm
			InitDefaultPrivatePrice(Gun_No);//加载时段====如果是私有电价那就可能时段不对
			cur_price_index = GetTimePriceIndex(Gun_No,last_end_hhmm);
			printf("cur_meter_kwh = %d  temp_kwh =%d \n",cur_meter_kwh,temp_kwh);
			if((cur_meter_kwh >= temp_kwh)&&(cur_meter_kwh < (temp_kwh+100*METER_READ_MULTIPLE)))  //小于1读电才更新
			{//当前电量大于等于记录结束电量，且不大于1度电
				delta_kwh = cur_meter_kwh - temp_kwh;			
				s_busy_frame.kwh_total_charged = cur_meter_kwh - s_busy_frame.start_kwh;
				printf("delta_kwh = %d  s_busy_frame.kwh_total_charged =%d \n",delta_kwh,s_busy_frame.kwh_total_charged);

				//	printf("22 = %d  %d %d %d\n",s_busy_frame.kwh1_charged,s_busy_frame.kwh2_charged,s_busy_frame.kwh3_charged,s_busy_frame.kwh4_charged);

				if(0 == cur_price_index)
				{
					s_busy_frame.kwh1_charged += delta_kwh;					
				}
				if(1 == cur_price_index)
				{
					s_busy_frame.kwh2_charged += delta_kwh;					
				}
				if(2 == cur_price_index)
				{
					s_busy_frame.kwh3_charged += delta_kwh;
				}
				if(3 == cur_price_index)
				{
					s_busy_frame.kwh4_charged += delta_kwh;					
				}
				
				//	printf("11 = %d  %d %d %d\n",s_busy_frame.kwh1_charged,s_busy_frame.kwh2_charged,s_busy_frame.kwh3_charged,s_busy_frame.kwh4_charged);

				
				
				delta_kwh2 = s_busy_frame.kwh_total_charged - (s_busy_frame.kwh1_charged + s_busy_frame.kwh2_charged + s_busy_frame.kwh3_charged + s_busy_frame.kwh4_charged);
					
				//	printf("delta_kwh2 = %d  s_busy_frame.kwh_total_charged =%d \n",delta_kwh2,s_busy_frame.kwh_total_charged);

				if(delta_kwh2 > 0)//总电量超过分时的和===异常
				{
					if(0 == cur_price_index)
					{
						s_busy_frame.kwh1_charged += delta_kwh2;					
					}
					if(1 == cur_price_index)
					{
						s_busy_frame.kwh2_charged += delta_kwh2;					
					}
					if(2 == cur_price_index)
					{
						s_busy_frame.kwh3_charged += delta_kwh2;
					}
					if(3 == cur_price_index)
					{
						s_busy_frame.kwh4_charged += delta_kwh2;					
					}
					AppLogOut("=======充电枪%d掉电恢复记录，分时电量的和小于总电量===异常!",Gun_No+1);
				}
				//	printf("333 = %d  %d %d %d\n",s_busy_frame.kwh1_charged,s_busy_frame.kwh2_charged,s_busy_frame.kwh3_charged,s_busy_frame.kwh4_charged);

				if(delta_kwh2 < 0)//分时电量的和超过总电量===异常
				{
					delta_kwh = abs(delta_kwh2);
					if(s_busy_frame.kwh1_charged > delta_kwh)
						s_busy_frame.kwh1_charged -= delta_kwh;
					else if(s_busy_frame.kwh2_charged > delta_kwh)
						s_busy_frame.kwh2_charged -= delta_kwh;
					else if(s_busy_frame.kwh3_charged > delta_kwh)
						s_busy_frame.kwh3_charged -= delta_kwh;
					else if(s_busy_frame.kwh4_charged > delta_kwh)
						s_busy_frame.kwh4_charged -= delta_kwh;
					AppLogOut("=======充电枪%d掉电恢复记录，分时电量的和超过总电量===异常!",Gun_No+1);
				}				
			//	printf("44 = %d  %d %d %d\n",s_busy_frame.kwh1_charged,s_busy_frame.kwh2_charged,s_busy_frame.kwh3_charged,s_busy_frame.kwh4_charged);

				ull_charged_rmb[0] = (s_busy_frame.kwh1_charged/METER_READ_MULTIPLE)*s_busy_frame.kwh1_price;
				ull_service_rmb[0] = (s_busy_frame.kwh1_charged/METER_READ_MULTIPLE)*s_busy_frame.kwh1_service_price;
				
				ull_charged_rmb[1] = (s_busy_frame.kwh2_charged/METER_READ_MULTIPLE)*s_busy_frame.kwh2_price;
				ull_service_rmb[1] = (s_busy_frame.kwh2_charged/METER_READ_MULTIPLE)*s_busy_frame.kwh2_service_price;	

				ull_charged_rmb[2] = (s_busy_frame.kwh3_charged/METER_READ_MULTIPLE)*s_busy_frame.kwh3_price;
				ull_service_rmb[2] = (s_busy_frame.kwh3_charged/METER_READ_MULTIPLE)*s_busy_frame.kwh3_service_price;
				
				ull_charged_rmb[3] = (s_busy_frame.kwh4_charged/METER_READ_MULTIPLE)*s_busy_frame.kwh4_price;
				ull_service_rmb[3] = (s_busy_frame.kwh4_charged/METER_READ_MULTIPLE)*s_busy_frame.kwh4_service_price;

				for(i = 0;i<4;i++)//分时电费和服务费0.01				
				{
					Globa->Control_DC_Infor_Data[Gun_No].ulcharged_rmb[i] = (ull_charged_rmb[i]  + COMPENSATION_VALUE)/ PRICE_DIVID_FACTOR;//转0.01元
					Globa->Control_DC_Infor_Data[Gun_No].ulcharged_service_rmb[i] = (ull_service_rmb[i] + COMPENSATION_VALUE)/ PRICE_DIVID_FACTOR;//转0.01元
				}
				s_busy_frame.kwh1_money = Globa->Control_DC_Infor_Data[Gun_No].ulcharged_rmb[0];
				s_busy_frame.kwh2_money = Globa->Control_DC_Infor_Data[Gun_No].ulcharged_rmb[1];
				s_busy_frame.kwh3_money = Globa->Control_DC_Infor_Data[Gun_No].ulcharged_rmb[2];
				s_busy_frame.kwh4_money = Globa->Control_DC_Infor_Data[Gun_No].ulcharged_rmb[3];
				
				s_busy_frame.kwh1_service_money = Globa->Control_DC_Infor_Data[Gun_No].ulcharged_service_rmb[0];
				s_busy_frame.kwh2_service_money = Globa->Control_DC_Infor_Data[Gun_No].ulcharged_service_rmb[1];
				s_busy_frame.kwh3_service_money = Globa->Control_DC_Infor_Data[Gun_No].ulcharged_service_rmb[2];
				s_busy_frame.kwh4_service_money = Globa->Control_DC_Infor_Data[Gun_No].ulcharged_service_rmb[3];
				
				
				ul_kwh_charge_rmb  = 0;//充电总电费 0.01元
				ul_kwh_service_rmb = 0;//充电总服务费 0.01元
				for(i = 0;i<4;i++)
				{
					ul_kwh_charge_rmb  += Globa->Control_DC_Infor_Data[Gun_No].ulcharged_rmb[i];
					ul_kwh_service_rmb += Globa->Control_DC_Infor_Data[Gun_No].ulcharged_service_rmb[i];
				}
				s_busy_frame.total_kwh_money = ul_kwh_charge_rmb;//总电费
				//s_busy_frame.service_money = ul_kwh_service_rmb;//总服务费
				s_busy_frame.all_chg_money = ul_kwh_charge_rmb + ul_kwh_service_rmb + s_busy_frame.book_money + s_busy_frame.park_money;

				if(s_busy_frame.last_rmb > 0)
					s_busy_frame.money_left = s_busy_frame.last_rmb - s_busy_frame.all_chg_money; 	
				
				s_busy_frame.end_kwh = cur_meter_kwh;	
				s_busy_frame.end_reason = BY_POWER_DOWN;
				if(s_busy_frame.kwh_total_charged > 0)
					s_busy_frame.rmb_kwh = (s_busy_frame.total_kwh_money*100/(s_busy_frame.kwh_total_charged/METER_READ_MULTIPLE)); //平均电价 0.01元/度
				
				
				//如果是用户卡充电，标记交易失败card_deal_result=1
				//if(CHG_BY_CARD == s_busy_frame.chg_start_by)//刷卡启动的，掉电后卡片应该灰锁
					//s_busy_frame.card_deal_result = 1; 
			}				
			
			Ret = Restore_0X55_Busy_ID(chg_db_sel,&s_busy_frame, id);
			
			if(Ret == -1){//失败
				printf("xxxxxxxxxxxxxxxxxxx Restore_0X55_Busy_ID failed\n");
			}else{//成功
				printf("yyyyyyyyyyyyyyyyyyy Restore_0X55_Busy_ID sucess\n");
				AppLogOut("=======充电枪%d掉电记录恢复完成!=====",Gun_No+1);
			}
		}else if(up_num == 0x66){//充电已结束的记录===
			Ret = Set_0X66_Busy_ID(chg_db_sel,id);
			if(Ret == -1){//失败
				printf("xxxxxxxxxxxxxxxxxxx Set_0X66_Busy_ID failed\n");
			}else{//成功
				printf("yyyyyyyyyyyyyyyyyyy Set_0X66_Busy_ID sucess\n");
			}
		}
	}
	
	
	pthread_mutex_unlock(&busy_db_pmutex);
	
	sleep(2);//
	pthread_mutex_lock(&busy_db_pmutex);
	Set_0X550X66_Busy_ID(chg_db_sel);//强制更新up_num为0可上传
	pthread_mutex_unlock(&busy_db_pmutex);
	id = 0;	
	
	Globa->Control_DC_Infor_Data[Gun_No].deal_restore_end_flag = 1;//记录恢复完毕可联网
	Globa->Control_DC_Infor_Data[Gun_No].BMS_Power_V = 0;//默认BMS辅助电源12V
	
	while(1)
	{
		usleep(10000);//10ms

		BMS_Volt_Sel(Gun_No);

		//判断枪状态 取值只有  03-空闲 0x04-充电中 0x05-完成 0x07-等待
		if(Globa->Control_DC_Infor_Data[Gun_No].gun_link != 1){  //枪连接断开
			Globa->Control_DC_Infor_Data[Gun_No].gun_state = 0x03; //充电枪连接只要断开就为 03-空闲
		}else{
			if(Globa->Control_DC_Infor_Data[Gun_No].gun_state == 0x03){ //枪状态 原来为 03-空闲 那现在改变为  0x07-等待
				Globa->Control_DC_Infor_Data[Gun_No].gun_state = 0x07;
			}else if((Globa->Control_DC_Infor_Data[Gun_No].gun_state == 0x07)&&((Globa->Control_DC_Infor_Data[Gun_No].charger_state == 0x04))){ //枪状态 原来为 0x07-等待 那现在改变为  0x04-充电中
				Globa->Control_DC_Infor_Data[Gun_No].gun_state = 0x04;
			}else if((Globa->Control_DC_Infor_Data[Gun_No].gun_state == 0x04)&&((Globa->Control_DC_Infor_Data[Gun_No].charger_state != 0x04))){ //枪状态 原来为 0x04-充电中 那现在改变为  0x05-完成
				Globa->Control_DC_Infor_Data[Gun_No].gun_state = 0x05;
			}else if((Globa->Control_DC_Infor_Data[Gun_No].gun_state == 0x05)&&((Globa->Control_DC_Infor_Data[Gun_No].charger_state == 0x03))){//0x04))){ //枪状态 原来为 0x05-完成 那现在改变为  0x04-充电中 可能不拔枪又开始充电
				Globa->Control_DC_Infor_Data[Gun_No].gun_state = 0x07;//等待//0x04;
			}
		}


		if(Globa->Control_DC_Infor_Data[Gun_No].Metering_Step != OLD_Metering_Step)
		{
			printf("---%d号枪计量-- Metering_Step=0x%d---------\n",
			Gun_No+1, 
			Globa->Control_DC_Infor_Data[Gun_No].Metering_Step	);
			printf("---%d号枪charger_state=%d---gun_state=%d--\n",Gun_No+1,
				Globa->Control_DC_Infor_Data[Gun_No].charger_state,
				Globa->Control_DC_Infor_Data[Gun_No].gun_state
				);
			OLD_Metering_Step = Globa->Control_DC_Infor_Data[Gun_No].Metering_Step;
		}
		
		switch(Globa->Control_DC_Infor_Data[Gun_No].Metering_Step)
		{//判断计费逻辑状态机
			case 0x00:{//---------------( 初始 )状态  ------------------
    		//该线程所用变量准备初始化
				Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
				Globa->Control_DC_Infor_Data[Gun_No].charger_start = 0;
				Globa->Control_DC_Infor_Data[Gun_No].App.APP_start = 0;
				Globa->Control_DC_Infor_Data[Gun_No].VIN_start = 0;
				Globa->Control_DC_Infor_Data[Gun_No].charger_state = 0x03;
		
				
				Globa->Control_DC_Infor_Data[Gun_No].charge_mode = AUTO_MODE;
				Globa->Control_DC_Infor_Data[Gun_No].card_type = 0;//卡类型 0:未知 1：用户卡 2：员工卡 3：维护卡
				Globa->Control_DC_Infor_Data[Gun_No].charge_money_limit = 0;
				Globa->Control_DC_Infor_Data[Gun_No].charge_kwh_limit = 0;
				Globa->Control_DC_Infor_Data[Gun_No].charge_sec_limit = 0;  
				Globa->Control_DC_Infor_Data[Gun_No].pre_time = 0;
				Globa->Control_DC_Infor_Data[Gun_No].BMS_batt_start_SOC = 0;
				Globa->Control_DC_Infor_Data[Gun_No].BMS_batt_SOC = 0;
                //pre_SOC[Gun_No] = 0; 
				Globa->Control_DC_Infor_Data[Gun_No].BATT_type = 0;
				Globa->Control_DC_Infor_Data[Gun_No].BMS_need_time = 0;
				Globa->Control_DC_Infor_Data[Gun_No].BMS_cell_HV = 0;
				Globa->Control_DC_Infor_Data[Gun_No].BMS_cell_LV = 0;
				Globa->Control_DC_Infor_Data[Gun_No].BMS_cell_HT = 0;
				Globa->Control_DC_Infor_Data[Gun_No].BMS_cell_LT = 0;
				Globa->Control_DC_Infor_Data[Gun_No].BMS_Info_falg = 0;//clear
				Globa->Control_DC_Infor_Data[Gun_No].Bind_BmsVin_flag = 0;	 			
				
				//Globa->Control_DC_Infor_Data[Gun_No].charger_start_way = CHG_BY_CARD;
				Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag = 0;
				
				Globa->Control_DC_Infor_Data[Gun_No].report_chg_started = 0;//===========
				Globa->Control_DC_Infor_Data[Gun_No].chg_start_result = 0;//=============
				Globa->Control_DC_Infor_Data[Gun_No].VIN_chg_start_result = 0;//===========
				Globa->Control_DC_Infor_Data[Gun_No].report_VIN_chg_started = 0;//=============
				Globa->Control_DC_Infor_Data[Gun_No].VIN_auth_req = 0;//请求VIN鉴权
				Globa->Control_DC_Infor_Data[Gun_No].VIN_auth_ack = 0;
				Globa->Control_DC_Infor_Data[Gun_No].VIN_auth_success = 0;//1--成功,0-失败
				Globa->Control_DC_Infor_Data[Gun_No].VIN_auth_failed_reason = 0;
				Globa->Control_DC_Infor_Data[Gun_No].VIN_Stop = 0;
				Globa->Control_DC_Infor_Data[Gun_No].pre_kwh = 0;

				Globa->Control_DC_Infor_Data[Gun_No].User_Card_check_flag = 0;
				Globa->Control_DC_Infor_Data[Gun_No].User_Card_check_result = 0;
				memset(&Globa->Control_DC_Infor_Data[Gun_No].Bind_BmsVin[0],0,17);
				Globa->Control_DC_Infor_Data[Gun_No].private_price_flag  = 0;		
				Globa->Control_DC_Infor_Data[Gun_No].Enterprise_Card_flag = 0;//企业卡号成功充电上传标志
				Globa->Control_DC_Infor_Data[Gun_No].qiyehao_flag = 0;//企业号标记，域43
				Globa->Control_DC_Infor_Data[Gun_No].order_chg_flag = 0;
				Globa->Control_DC_Infor_Data[Gun_No].private_price_acquireOK = 0;
				Globa->Control_DC_Infor_Data[Gun_No].order_card_end_time[0] = 99;
				Globa->Control_DC_Infor_Data[Gun_No].order_card_end_time[1] = 99;
			  
				Globa->Control_DC_Infor_Data[Gun_No].Battery_Parameter_Sent_Flag = 0;//清除发送4050报文标志				
				abnormal_Log_en = 1;
				first_get_bms_info = 0;
				check_current_low_st = 0;
				check_current_low_st2 = 0;
				check_meter_volt_st = 0; 
				check_meter_current_st = 0;
				ul_start_kwh = 0;
				InitDefaultPrivatePrice(Gun_No);
				cur_price_index = GetCurPriceIndex(Gun_No); //获取当前时间
				pre_price_index = cur_price_index;
				for(i=0;i<4;i++)
				{
					Globa->Control_DC_Infor_Data[Gun_No].ulcharged_rmb[i] = 0;
					Globa->Control_DC_Infor_Data[Gun_No].ulcharged_kwh[i] = 0;
					Globa->Control_DC_Infor_Data[Gun_No].ulcharged_service_rmb[i] = 0;					
				}
				Globa->Control_DC_Infor_Data[Gun_No].kwh = 0;
				Globa->Control_DC_Infor_Data[Gun_No].rmb = 0;
				Globa->Control_DC_Infor_Data[Gun_No].service_rmb = 0;
				Globa->Control_DC_Infor_Data[Gun_No].total_rmb = 0;
				Globa->Control_DC_Infor_Data[Gun_No].park_rmb = 0;
				temp_kwh = 0;
				Globa->Control_DC_Infor_Data[Gun_No].soft_KWH = 0;
				Globa->Control_DC_Infor_Data[Gun_No].Time = 0;
				
				Globa->Control_DC_Infor_Data[Gun_No].Metering_Step = 0x01;
				memset(&Globa->Control_DC_Infor_Data[Gun_No].card_sn[0], '0', sizeof(Globa->Control_DC_Infor_Data[Gun_No].card_sn));
				Globa->Control_DC_Infor_Data[Gun_No].Credit_card_success_flag = 0;
				Globa->Control_DC_Infor_Data[Gun_No].MT625_Card_Start_ok_flag = 0;
				Globa->Control_DC_Infor_Data[Gun_No].MT625_Card_End_ok_flag = 0;
				check_db_count = 99;//next马上检测
				Globa->Control_DC_Infor_Data[Gun_No].remote_stop = 0;
				Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_falg = 0;//退出同充状态190929
				Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Hostgun = 0;//(主枪标志)
				Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Othergun = 0;
				Electrical_abnormality = GetTickCount();

				break;
			}
			case 0x01:{//---------------系统等待启动  ------------
				check_db_count++;
				if(check_db_count == 100)//检测有无记录未转存总表有则转存
				{
					check_db_count = 0;	
					pthread_mutex_lock(&busy_db_pmutex);				
					id = Select_MAX_ID_Busy(chg_db_sel,&pre_old_busy_frame);
					if(-1 != id)//有记录未转存到总表
					{
						if(0==Select_Busy_BCompare_ID(TOTAL_DEAL_DB_SEL,&pre_old_busy_frame))//总表中已经有相同的记录了
						{							
							Delete_Record_busy_DB(chg_db_sel, id);	//删除本地记录						
						}else
						{
							//插入到总的交易记录数据库中
							//pthread_mutex_lock(&busy_db_pmutex);
							all_id = 0;
							Insertflag = Insert_Charger_Busy_DB(TOTAL_DEAL_DB_SEL,&pre_old_busy_frame, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
							if((Insertflag != -1)&&(all_id != 0))
							{//表示插入数据成功，其他的则需要重新插入
								Delete_Record_busy_DB(chg_db_sel, id);
								if(DEBUGLOG) GLOBA_RUN_LogOut("%d号枪交易数据,插入到正式数据库中并删除临时数据库记录 ID = %d",chg_db_sel,id);  
							}else
							{
								if(DEBUGLOG) GLOBA_RUN_LogOut("%d号枪交易数据,插入到正式数据库中失败 ID = %d",chg_db_sel,id);  
							}
							//pthread_mutex_unlock(&busy_db_pmutex);						
						}					
					}
					else//无记录了
						check_db_count = 100;//避免频繁访问数据库
					pthread_mutex_unlock(&busy_db_pmutex);
				}
				first_get_bms_info = 0;
				if( Globa->Control_DC_Infor_Data[Gun_No].Error_system != 0)//test
					 Globa->Control_DC_Infor_Data[Gun_No].charger_state = 0x01;//故障
				 else
					Globa->Control_DC_Infor_Data[Gun_No].charger_state = 0x03;
				
				if(Globa->Control_DC_Infor_Data[Gun_No].MT625_Card_Start_ok_flag == 1)
				{//卡片成功处理--刷卡启动
					if(INPUT_RELAY_CTL_MODULE == Globa->Charger_param.Input_Relay_Ctl_Type)//模块节能控制
					{	
						if(ac_out == 0)//模块还没上电
							need_wait_module_poweron = 1;
						else
							need_wait_module_poweron =  0;
						Globa->Control_DC_Infor_Data[Gun_No].AC_Relay_control = 1;//确保模块开启
					//	if(need_wait_module_poweron)
					}
					check_bmsVin = 1;//判断卡与车的是否匹配
					check_current_low_st = 0;
					check_current_low_st2 = 0;
					check_meter_volt_st = 0; 
				  check_meter_current_st = 0;
					count_wait = 0;
					Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = 0;
					Globa->Control_DC_Infor_Data[Gun_No].kwh = 0;
					Globa->Control_DC_Infor_Data[Gun_No].rmb = 0;
					Globa->Control_DC_Infor_Data[Gun_No].service_rmb = 0;
					Globa->Control_DC_Infor_Data[Gun_No].total_rmb = 0;
					Globa->Control_DC_Infor_Data[Gun_No].park_rmb = 0;
					ul_start_kwh = 0;
		
					for(i=0;i<4;i++)
					{
						Globa->Control_DC_Infor_Data[Gun_No].ulcharged_rmb[i] = 0;
						Globa->Control_DC_Infor_Data[Gun_No].ulcharged_kwh[i] = 0;
						Globa->Control_DC_Infor_Data[Gun_No].ulcharged_service_rmb[i] = 0;										
					}
					//如果该卡没有私有电价
					if(0 == Globa->Control_DC_Infor_Data[Gun_No].private_price_acquireOK)
						InitDefaultPrivatePrice(Gun_No);//初始化电价
					for(i=0;i<4;i++)
					{
						share_time_kwh_price[i] = Globa->Control_DC_Infor_Data[Gun_No].Special_price_data.share_time_kwh_price[i] ;//分时电价
						share_service_price[i] = Globa->Control_DC_Infor_Data[Gun_No].Special_price_data.share_time_kwh_price_serve[i] ; //分时服务费						
					}
					park_price = Globa->Charger_param2.charge_stop_price.stop_price;
					cur_price_index = GetCurPriceIndex(Gun_No); //获取当前时间
					pre_price_index = cur_price_index;
					
					BMS_Volt_Sel(Gun_No);
										
					temp_kwh = 0;
					Globa->Control_DC_Infor_Data[Gun_No].soft_KWH = 0;
					Globa->Control_DC_Infor_Data[Gun_No].Time = 0;
					Globa->Control_DC_Infor_Data[Gun_No].BMS_need_time = 0;
					Globa->Control_DC_Infor_Data[Gun_No].BMS_batt_SOC = 0;
					Globa->Control_DC_Infor_Data[Gun_No].BMS_pre_batt_SOC = 0;
					Globa->Control_DC_Infor_Data[Gun_No].BMS_batt_start_SOC = 0;//190710
          pre_SOC[Gun_No] = 0;
					Globa->Control_DC_Infor_Data[Gun_No].BMS_cell_HV = 0; 
          current_overkwh_timestamp = GetTickCount();	
          Electrical_abnormality = GetTickCount();
					
					if((Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_falg == 1)&&(Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Hostgun == 0))//辅枪----VIN不清0
					{
					}
					else//单枪充或双枪的主枪
						memset(Globa->Control_DC_Infor_Data[Gun_No].VIN, '0', sizeof(Globa->Control_DC_Infor_Data[Gun_No].VIN));
				
					Globa->Control_DC_Infor_Data[Gun_No].MT625_Card_End_ok_flag = 0;
					Globa->Control_DC_Infor_Data[Gun_No].MT625_Card_Start_ok_flag = 0;
					Globa->Control_DC_Infor_Data[Gun_No].Credit_card_success_flag = 0;//防止枪之前处于双枪同充状态时，辅枪停了拔枪后，主枪刷卡停止，会把辅枪的该标志置位，导致下次刷卡启动该枪的时候直接提示刷卡停止了!
					Globa->Control_DC_Infor_Data[Gun_No].meter_start_KWH = Globa->Control_DC_Infor_Data[Gun_No].meter_KWH;//取初始电表示数
					Globa->Control_DC_Infor_Data[Gun_No].meter_stop_KWH  = Globa->Control_DC_Infor_Data[Gun_No].meter_start_KWH;
				
					Globa->Control_DC_Infor_Data[Gun_No].charger_start_way = CHG_BY_CARD; //刷卡启动
					if(0xAA==Globa->Control_DC_Infor_Data[Gun_No].qiyehao_flag)
						Globa->Control_DC_Infor_Data[Gun_No].last_rmb = 0;//企业号充电不判断余额
					
					InitChgingBusyData(Gun_No,CHG_BY_CARD,&s_busy_frame);					
					
					id = 0;
					pthread_mutex_lock(&busy_db_pmutex);
					Insertflag = Insert_Charger_Busy_DB(chg_db_sel,&s_busy_frame, &id, 0x55);//写新数据库记录 无流水号 传输标志0x55不能上传
					pthread_mutex_unlock(&busy_db_pmutex);
				   	
					
					if((Insertflag  == -1)&&(id == 0))
					{//数据库有异常
						Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = BY_DATABASE_FAULT;//数据库异常结束
						Globa->Control_DC_Infor_Data[Gun_No].charger_state = 5;//通知QT充电已结束
						Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
						Globa->Control_DC_Infor_Data[Gun_No].charger_start = 0;
						if(DEBUGLOG) GLOBA_RUN_LogOut("%d号枪:刷卡启动充电插入交易记录失败",chg_db_sel);  
						//sleep(5);
						Globa->Control_DC_Infor_Data[Gun_No].Metering_Step = 0x03; // 等待界面结算完成
						
					}else//数据库插入正常
					{						
						if(Globa->Control_DC_Infor_Data[Gun_No].charge_mode == DELAY_CHG_MODE)
						{//表示本地预约===倒计时充电
							Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
							Globa->Control_DC_Infor_Data[Gun_No].charger_start = 0;
							Globa->Control_DC_Infor_Data[Gun_No].charger_state = 0x06;
							Globa->Control_DC_Infor_Data[Gun_No].App.APP_start = 0;
							Globa->Control_DC_Infor_Data[Gun_No].Metering_Step = 0x04;   //倒计时充电状态							
						}else
						{
							Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 1;//
							Globa->Control_DC_Infor_Data[Gun_No].charger_start = 1;//
							Globa->Control_DC_Infor_Data[Gun_No].Metering_Step = 0x02;   // 2 自动充电-》 充电界面 状态 2.1-
							Globa->Control_DC_Infor_Data[Gun_No].charger_state = 0x04;
							Globa->Control_DC_Infor_Data[Gun_No].App.APP_start = 0;							
						}
						if(0xAA==Globa->Control_DC_Infor_Data[Gun_No].qiyehao_flag)
							Globa->Control_DC_Infor_Data[Gun_No].Enterprise_Card_flag = 1;//发企业卡充电启动通知给服务器
						else
							Globa->Control_DC_Infor_Data[Gun_No].Enterprise_Card_flag = 0;
					}
				}
				else
				{					
					if(Globa->Control_DC_Infor_Data[Gun_No].App.APP_start == 1)
					{//手机启动充电
						if(INPUT_RELAY_CTL_MODULE == Globa->Charger_param.Input_Relay_Ctl_Type)//模块节能控制
						{
							if(ac_out == 0)//模块还没上电
								need_wait_module_poweron = 1;
							else
								need_wait_module_poweron =  0;
							Globa->Control_DC_Infor_Data[Gun_No].AC_Relay_control = 1;//确保模块开启
							//if(need_wait_module_poweron)
						}
			
						count_wait = 0;
						check_current_low_st = 0;
						check_current_low_st2 = 0;
						Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = 0;
						//-------------------     新业务进度数据------------------------------
						Globa->Control_DC_Infor_Data[Gun_No].kwh = 0;
						Globa->Control_DC_Infor_Data[Gun_No].rmb = 0;
						Globa->Control_DC_Infor_Data[Gun_No].service_rmb = 0;
						Globa->Control_DC_Infor_Data[Gun_No].total_rmb = 0;
						ul_start_kwh = 0;
				   	check_meter_volt_st = 0; 
				    check_meter_current_st = 0;
						for(i=0;i<4;i++)
						{
							Globa->Control_DC_Infor_Data[Gun_No].ulcharged_rmb[i] = 0;
							Globa->Control_DC_Infor_Data[Gun_No].ulcharged_kwh[i] = 0;
							Globa->Control_DC_Infor_Data[Gun_No].ulcharged_service_rmb[i] = 0;							
						}
						//如果APP没有私有电价
						if(0 == Globa->Control_DC_Infor_Data[Gun_No].private_price_acquireOK)
							InitDefaultPrivatePrice(Gun_No);//初始化电价
						for(i=0;i<4;i++)
						{
							share_time_kwh_price[i] = Globa->Control_DC_Infor_Data[Gun_No].Special_price_data.share_time_kwh_price[i] ;//分时电价
							share_service_price[i] = Globa->Control_DC_Infor_Data[Gun_No].Special_price_data.share_time_kwh_price_serve[i] ; //分时服务费						
						}
						park_price = Globa->Charger_param2.charge_stop_price.stop_price;
						
						cur_price_index = GetCurPriceIndex(Gun_No); //获取当前时间
						pre_price_index = cur_price_index;
					  current_overkwh_timestamp = GetTickCount();	
            Electrical_abnormality = GetTickCount();
						BMS_Volt_Sel(Gun_No);
						
						temp_kwh = 0;
						Globa->Control_DC_Infor_Data[Gun_No].soft_KWH = 0;
						Globa->Control_DC_Infor_Data[Gun_No].Time = 0;
						Globa->Control_DC_Infor_Data[Gun_No].BMS_need_time = 0;
						Globa->Control_DC_Infor_Data[Gun_No].BMS_batt_SOC = 0;
						Globa->Control_DC_Infor_Data[Gun_No].BMS_pre_batt_SOC = 0;
						Globa->Control_DC_Infor_Data[Gun_No].BMS_batt_start_SOC = 0;//190710
            pre_SOC[Gun_No] = 0;
						Globa->Control_DC_Infor_Data[Gun_No].BMS_cell_HV = 0;   
					
						memset(Globa->Control_DC_Infor_Data[Gun_No].VIN, '0', sizeof(Globa->Control_DC_Infor_Data[Gun_No].VIN));
	        
						Globa->Control_DC_Infor_Data[Gun_No].MT625_Card_End_ok_flag = 0;
						Globa->Control_DC_Infor_Data[Gun_No].MT625_Card_Start_ok_flag = 0;
										
						Globa->Control_DC_Infor_Data[Gun_No].meter_start_KWH = Globa->Control_DC_Infor_Data[Gun_No].meter_KWH;//取初始电表示数
						Globa->Control_DC_Infor_Data[Gun_No].meter_stop_KWH = Globa->Control_DC_Infor_Data[Gun_No].meter_start_KWH;
						//app启动---先置标志，InitChgingBusyData会使用该标志
						Globa->Control_DC_Infor_Data[Gun_No].charger_start_way = CHG_BY_APP; 
						
						InitChgingBusyData(Gun_No,CHG_BY_APP,&s_busy_frame);
						
						
						id = 0;
						pthread_mutex_lock(&busy_db_pmutex);
						Insertflag = Insert_Charger_Busy_DB(chg_db_sel,&s_busy_frame, &id, 0x55);//写新数据库记录 无流水号 传输标志0x55不能上传
						pthread_mutex_unlock(&busy_db_pmutex);						
						if(Insertflag == -1)//failed
						{
							Globa->Control_DC_Infor_Data[Gun_No].chg_start_result = CHG_START_FAILED;
							Globa->Control_DC_Infor_Data[Gun_No].report_chg_started = 1;
							Globa->Control_DC_Infor_Data[Gun_No].AP_APP_Sent_flag[1] = 6;//APP启动结果,故障不能充电
							Globa->Control_DC_Infor_Data[Gun_No].AP_APP_Sent_flag[0] = 1;
							//Globa->Control_DC_Infor_Data[Gun_No].Metering_Step = ;
							//Globa->Control_DC_Infor_Data[Gun_No].charger_state = 3;
							Globa->Control_DC_Infor_Data[Gun_No].App.APP_start = 0;
							//sleep(5);//
							GLOBA_RUN_LogOut("%d号枪APP启动插入记录失败",chg_db_sel);
							Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = BY_DATABASE_FAULT;//数据库异常结束
							Globa->Control_DC_Infor_Data[Gun_No].charger_state = 5;//通知QT充电已结束
							Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
							Globa->Control_DC_Infor_Data[Gun_No].charger_start = 0;							 
							Globa->Control_DC_Infor_Data[Gun_No].Metering_Step = 0x03; // 等待界面结算完成
							break;
						}
						
						Globa->Control_DC_Infor_Data[Gun_No].chg_start_result = CHG_START_SUCCESS;
						Globa->Control_DC_Infor_Data[Gun_No].report_chg_started = 1;
						
						Globa->Control_DC_Infor_Data[Gun_No].AP_APP_Sent_flag[1] = 0 ;//APP启动结果,0成功
						Globa->Control_DC_Infor_Data[Gun_No].AP_APP_Sent_flag[0] = 1;
						
						//usleep(10000);
						
						Globa->Control_DC_Infor_Data[Gun_No].charge_mode = Globa->Control_DC_Infor_Data[Gun_No].App.APP_charge_mode;
						Globa->Control_DC_Infor_Data[Gun_No].charge_kwh_limit = Globa->Control_DC_Infor_Data[Gun_No].App.APP_charge_kwh_limit;//0.01kwh
						Globa->Control_DC_Infor_Data[Gun_No].charge_sec_limit = Globa->Control_DC_Infor_Data[Gun_No].App.APP_charge_sec_limit;//
						Globa->Control_DC_Infor_Data[Gun_No].charge_money_limit = Globa->Control_DC_Infor_Data[Gun_No].App.APP_charge_money_limit;//0.01元
						Globa->Control_DC_Infor_Data[Gun_No].last_rmb = Globa->Control_DC_Infor_Data[Gun_No].App.last_rmb;//0.01元
						memcpy(Globa->Control_DC_Infor_Data[Gun_No].card_sn,Globa->Control_DC_Infor_Data[Gun_No].App.ID, 16);
						
						
						Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 1;
						Globa->Control_DC_Infor_Data[Gun_No].charger_start = 1;
						Globa->Control_DC_Infor_Data[Gun_No].Metering_Step = 0x02;
						Globa->Control_DC_Infor_Data[Gun_No].charger_state = 0x04;
					}else//check vin鉴权
					{						
						if(Globa->Control_DC_Infor_Data[Gun_No].VIN_start == 1)//尝试VIN启动充电
						{
							if(INPUT_RELAY_CTL_MODULE == Globa->Charger_param.Input_Relay_Ctl_Type)//模块节能控制
							{
								if(ac_out == 0)//模块还没上电
									need_wait_module_poweron = 1;
								else
									need_wait_module_poweron =  0;
								Globa->Control_DC_Infor_Data[Gun_No].AC_Relay_control = 1;//确保模块开启
								//if(need_wait_module_poweron)
							}
							//Globa->Control_DC_Infor_Data[Gun_No].VIN_start = 0;//clear
							count_wait = 0;
							check_current_low_st = 0;
							check_current_low_st2 = 0;
							check_meter_volt_st = 0; 
			      	check_meter_current_st = 0;
							Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = 0;
							//-------------------     新业务进度数据------------------------------
							Globa->Control_DC_Infor_Data[Gun_No].kwh = 0;
							Globa->Control_DC_Infor_Data[Gun_No].rmb = 0;
							Globa->Control_DC_Infor_Data[Gun_No].service_rmb = 0;
							Globa->Control_DC_Infor_Data[Gun_No].total_rmb = 0;
							ul_start_kwh = 0;
							current_overkwh_timestamp = GetTickCount();	
							Electrical_abnormality = GetTickCount();
							for(i=0;i<4;i++)
							{
								Globa->Control_DC_Infor_Data[Gun_No].ulcharged_rmb[i] = 0;
								Globa->Control_DC_Infor_Data[Gun_No].ulcharged_kwh[i] = 0;
								Globa->Control_DC_Infor_Data[Gun_No].ulcharged_service_rmb[i] = 0;								
							}
				
							if(0 == Globa->Control_DC_Infor_Data[Gun_No].private_price_acquireOK)
								InitDefaultPrivatePrice(Gun_No);//初始化电价
							for(i=0;i<4;i++)
							{
								share_time_kwh_price[i] = Globa->Control_DC_Infor_Data[Gun_No].Special_price_data.share_time_kwh_price[i] ;//分时电价
								share_service_price[i] = Globa->Control_DC_Infor_Data[Gun_No].Special_price_data.share_time_kwh_price_serve[i] ; //分时服务费						
							}
							park_price = Globa->Charger_param2.charge_stop_price.stop_price;
							
							cur_price_index = GetCurPriceIndex(Gun_No); //获取当前时间
							pre_price_index = cur_price_index;
							
							BMS_Volt_Sel(Gun_No);
							
							temp_kwh = 0;
							Globa->Control_DC_Infor_Data[Gun_No].soft_KWH = 0;
							Globa->Control_DC_Infor_Data[Gun_No].Time = 0;
							Globa->Control_DC_Infor_Data[Gun_No].BMS_need_time = 0;
							Globa->Control_DC_Infor_Data[Gun_No].BMS_batt_SOC = 0;
							Globa->Control_DC_Infor_Data[Gun_No].BMS_pre_batt_SOC = 0;
							Globa->Control_DC_Infor_Data[Gun_No].BMS_batt_start_SOC = 0;//190710
              pre_SOC[Gun_No] = 0;  
							Globa->Control_DC_Infor_Data[Gun_No].BMS_cell_HV = 0;   
							if((Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_falg == 1)&&(Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Hostgun == 0))//辅枪----VIN不清0
							{
							}
							else//单枪充或双枪的主枪
								memset(Globa->Control_DC_Infor_Data[Gun_No].VIN, '0', sizeof(Globa->Control_DC_Infor_Data[Gun_No].VIN));
				
							Globa->Control_DC_Infor_Data[Gun_No].MT625_Card_End_ok_flag = 0;
							Globa->Control_DC_Infor_Data[Gun_No].MT625_Card_Start_ok_flag = 0;
											
							Globa->Control_DC_Infor_Data[Gun_No].meter_start_KWH = Globa->Control_DC_Infor_Data[Gun_No].meter_KWH;//取初始电表示数
							Globa->Control_DC_Infor_Data[Gun_No].meter_stop_KWH = Globa->Control_DC_Infor_Data[Gun_No].meter_start_KWH;
							
							Globa->Control_DC_Infor_Data[Gun_No].charger_start_way = CHG_BY_VIN; //VIN启动
							Globa->Control_DC_Infor_Data[Gun_No].VIN_Stop = 0;
							InitChgingBusyData(Gun_No,CHG_BY_VIN,&s_busy_frame);
							
							
							id = 0;
							pthread_mutex_lock(&busy_db_pmutex);
							Insertflag = Insert_Charger_Busy_DB(chg_db_sel,&s_busy_frame, &id, 0x55);//写新数据库记录 无流水号 传输标志0x55不能上传
							pthread_mutex_unlock(&busy_db_pmutex);						
							if(Insertflag == -1)//failed
							{
								//Globa->Control_DC_Infor_Data[Gun_No].VIN_chg_start_result = CHG_START_FAILED;
								//Globa->Control_DC_Infor_Data[Gun_No].report_VIN_chg_started = 1;
								//Globa->Control_DC_Infor_Data[Gun_No].Metering_Step = ;
								//Globa->Control_DC_Infor_Data[Gun_No].charger_state = 3;
								Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason =BY_DATABASE_FAULT;//数据库异常结束
								Globa->Control_DC_Infor_Data[Gun_No].charger_state = 5;//通知QT充电已结束
								Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
								Globa->Control_DC_Infor_Data[Gun_No].charger_start = 0;	
								//sleep(5);//
								GLOBA_RUN_LogOut("%d号枪VIN启动插入记录失败",chg_db_sel);
								Globa->Control_DC_Infor_Data[Gun_No].Metering_Step = 0x03; // 等待界面结算完成
								break;
							}							
							
							//usleep(10000);						  
							Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 1;							
							Globa->Control_DC_Infor_Data[Gun_No].charger_start = 1;//直接启动，VIN鉴权不过再停==180917
							if((Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_falg == 1)&&(Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Hostgun == 0))
							{
								//更新VIN码对应的账号
								if(id != 0)
								{
									pthread_mutex_lock(&busy_db_pmutex);
									if(Update_VIN_CardSN(chg_db_sel,id,Globa->Control_DC_Infor_Data[Gun_No].VIN_User_Id ) == -1)
									{
										usleep(10000);
										Update_VIN_CardSN(chg_db_sel,id,Globa->Control_DC_Infor_Data[Gun_No].VIN_User_Id );//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
									}
									pthread_mutex_unlock(&busy_db_pmutex);
								}
								Globa->Control_DC_Infor_Data[Gun_No].VIN_chg_start_result = CHG_START_SUCCESS;
								Globa->Control_DC_Infor_Data[Gun_No].report_VIN_chg_started = 1;
				
								//Globa->Control_DC_Infor_Data[Gun_No].VIN_start =  0;
								Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 1;					
								Globa->Control_DC_Infor_Data[Gun_No].charger_start = 1;
								Globa->Control_DC_Infor_Data[Gun_No].Metering_Step = 0x02;//副枪直接进充电，因为主枪已鉴权通过了
								
								Globa->Control_DC_Infor_Data[Gun_No].charger_state = 0x04;
								
							}else//主枪
							{
								Globa->Control_DC_Infor_Data[Gun_No].Metering_Step = 0x05;//等待VIN鉴权或超时//0x02;
							}
							Globa->Control_DC_Infor_Data[Gun_No].charger_state = 0x04;
						}						
					}
				}
			   break;
			}
			case 0x02:{//---------------2 自动充电-》 自动充电界面 状态  2.1----------
				{//================================充电费用计算===========================================
					//Globa->Control_DC_Infor_Data[Gun_No].meter_stop_KWH = Globa->Control_DC_Infor_Data[Gun_No].meter_KWH;//取初始电表示数
					if(Globa->Control_DC_Infor_Data[Gun_No].meter_KWH >= Globa->Control_DC_Infor_Data[Gun_No].meter_stop_KWH)
					{
					  Globa->Control_DC_Infor_Data[Gun_No].meter_stop_KWH = Globa->Control_DC_Infor_Data[Gun_No].meter_KWH;//取初始电表示数
						Electrical_abnormality = GetTickCount();
					}else{//连续3s获取到的数据都比较起始数据小，则直接停止充电
						if(abs(GetTickCount() - Electrical_abnormality) > METER_ELECTRICAL_ERRO_COUNT)//超过1分钟了，需停止充电
						{
              Electrical_abnormality = GetTickCount();
							Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
							Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = BY_DC_READ_KWH_ERR;
							s_busy_frame.end_reason = BY_DC_READ_KWH_ERR;
							SysControlLogOut("%d#枪计费QT判断读取电量有异常：meter_KWH=%d meter_stop_KWH=%d",Globa->Control_DC_Infor_Data[Gun_No].meter_KWH, Globa->Control_DC_Infor_Data[Gun_No].meter_stop_KWH);
						}
					}
					
					
					
					Globa->Control_DC_Infor_Data[Gun_No].kwh = (Globa->Control_DC_Infor_Data[Gun_No].meter_stop_KWH - Globa->Control_DC_Infor_Data[Gun_No].meter_start_KWH);
					soft_KWH = Globa->Control_DC_Infor_Data[Gun_No].kwh;//电量放大到3位小数 0.001kwh
					
					if(Globa->Charger_param.meter_config_flag != DCM_DJS3366)//不是3位小数的电表
					{
						if((Globa->Control_DC_Infor_Data[Gun_No].Output_current/100) > 20)//5.0A以上再弄
						{
							
							soft_KWH += Get_rand()%10;//会不会出现电量倒退?
							if(soft_KWH < Globa->Control_DC_Infor_Data[Gun_No].pre_kwh )
								soft_KWH = Globa->Control_DC_Infor_Data[Gun_No].pre_kwh;//不加防止进位了 + rand()%10;
							else
								Globa->Control_DC_Infor_Data[Gun_No].pre_kwh = soft_KWH;
						}
					}
					Globa->Control_DC_Infor_Data[Gun_No].soft_KWH = soft_KWH;
					
					
					Globa->Control_DC_Infor_Data[Gun_No].ulcharged_kwh[pre_price_index] = Globa->Control_DC_Infor_Data[Gun_No].kwh - ul_start_kwh;//累加当前时段的耗电量	
					cur_price_index = GetCurPriceIndex(Gun_No); //获取当前时间
					if(cur_price_index != pre_price_index)
					{
						ul_start_kwh = Globa->Control_DC_Infor_Data[Gun_No].kwh - Globa->Control_DC_Infor_Data[Gun_No].ulcharged_kwh[cur_price_index];//load new start kwh,需要减去时段之前累加的电量，否则该时段之前累加的电量丢失----20160312
						pre_price_index = cur_price_index;							
					}
					share_kwh_sum = 0;					
					for(i=0;i<4;i++)
						share_kwh_sum += Globa->Control_DC_Infor_Data[Gun_No].ulcharged_kwh[i];
					if(share_kwh_sum < Globa->Control_DC_Infor_Data[Gun_No].kwh)
					{
						delta_kwh = Globa->Control_DC_Infor_Data[Gun_No].kwh - share_kwh_sum;
						sprintf(temp_char,"=======分时电量累加和小于电表当前读数与起始充电读数的差，少%d.%02dkwh!\n",delta_kwh/1000,delta_kwh%1000);
						printf(temp_char);
						if(abnormal_Log_en)
						{
							AppLogOut(temp_char);
							abnormal_Log_en = 0;//1次充电只记录1次
						}
						Globa->Control_DC_Infor_Data[Gun_No].ulcharged_kwh[pre_price_index] +=   delta_kwh;//强制增加test
					}
					for(i = 0;i<4;i++)
					{
						ull_temp = Globa->Control_DC_Infor_Data[Gun_No].ulcharged_kwh[i];//0.01kwh
						ull_charged_rmb[i] = (ull_temp/METER_READ_MULTIPLE)*share_time_kwh_price[i];
						ull_service_rmb[i] = (ull_temp/METER_READ_MULTIPLE)*share_service_price[i];   //金额最终为两位小数，而电量以及是三位小数了，为了兼容计算就把电量换算成原来的2位小数,并防止后台计算出现误差
					}
					ull_kwh_rmb = 0;
					ull_total_service_rmb = 0;
					for(i = 0;i<4;i++)
					{	
						ull_kwh_rmb += ull_charged_rmb[i];//总电费，7位小数
						ull_total_service_rmb += ull_service_rmb[i];//总服务费，7位小数
					}				
					
					for(i = 0;i<4;i++)//分时电费和服务费0.01				
					{
						Globa->Control_DC_Infor_Data[Gun_No].ulcharged_rmb[i] = (ull_charged_rmb[i] + COMPENSATION_VALUE)/ PRICE_DIVID_FACTOR;//转0.01元
						Globa->Control_DC_Infor_Data[Gun_No].ulcharged_service_rmb[i] = (ull_service_rmb[i] + COMPENSATION_VALUE)/ PRICE_DIVID_FACTOR;//转0.01元
					}
					//Globa->Control_DC_Infor_Data[Gun_No].rmb = ull_kwh_rmb/PRICE_DIVID_FACTOR;//总电费,0.01元					
					
					ul_kwh_charge_rmb  = 0;//充电总电费 0.01元
					ul_kwh_service_rmb = 0;//充电总服务费 0.01元
					for(i = 0;i<4;i++)
					{
						ul_kwh_charge_rmb  += Globa->Control_DC_Infor_Data[Gun_No].ulcharged_rmb[i];
						ul_kwh_service_rmb += Globa->Control_DC_Infor_Data[Gun_No].ulcharged_service_rmb[i];
					}
					Globa->Control_DC_Infor_Data[Gun_No].rmb  = ul_kwh_charge_rmb;
					Globa->Control_DC_Infor_Data[Gun_No].service_rmb = ul_kwh_service_rmb;
					
					//Globa->Control_DC_Infor_Data[Gun_No].park_rmb = (Globa->Control_DC_Infor_Data[Gun_No].Time/600)*park_price/(PRICE_DIVID_FACTOR/100);//0.01元
					//ull_temp = (ull_kwh_rmb + ull_total_service_rmb)/PRICE_DIVID_FACTOR;	
					ull_temp = ul_kwh_charge_rmb + ul_kwh_service_rmb;//
										
					//ull_temp += Globa->Control_DC_Infor_Data[Gun_No].park_rmb;//add停车费
					Globa->Control_DC_Infor_Data[Gun_No].total_rmb = ull_temp;//总费用(电费，服务费，停车费)
				}
				UINT32 ul_output_kw;
				ull_temp = (Globa->Control_DC_Infor_Data[Gun_No].Output_voltage)/10;//0.01V
				ull_temp = ull_temp * ((Globa->Control_DC_Infor_Data[Gun_No].Output_current)/10);//0.01A
				ull_temp = ull_temp/10000;//VA
				ul_output_kw = ull_temp/100;//0.1kw //(Globa->Control_DC_Infor_Data[Gun_No].Output_voltage/1000) * (Globa->Control_DC_Infor_Data[Gun_No].Output_current/1000);//VA
				Globa->Control_DC_Infor_Data[Gun_No].output_kw = ul_output_kw ;
				
				//-------------------计费----------------------------
				UpdateChgingBusyData(Gun_No,&s_busy_frame);
				
				if((0 == first_get_bms_info)&&(Globa->Control_DC_Infor_Data[Gun_No].BMS_Info_falg ))
					first_get_bms_info = 1;
				
				//充电过程中实时更新业务数据记录;
				//若意外掉电，则上电时判断最后一条数据记录的“上传标示”是否为“0x55”;
				//若是，以此时电表示数作为上次充电结束时的示数;
				if(((Globa->Control_DC_Infor_Data[Gun_No].kwh - temp_kwh) > 50*METER_READ_MULTIPLE) || (1==first_get_bms_info))
				{//每隔0.5度电左右保存一次数据;
					if(1==first_get_bms_info)
					{
						if(ValidVIN_Check(Globa->Control_DC_Infor_Data[Gun_No].VIN))//VIN有效
						{
							printf("=========首次获取到充电枪%d的车辆VIN：%.17s，写入交易记录数据库!\n",Gun_No+1,Globa->Control_DC_Infor_Data[Gun_No].VIN);	
							UpdateChgingBusyData(Gun_No,&s_busy_frame);
							AppLogOut("========获取到充电枪%d的当前充电车辆的VIN：%.17s，已写入数据库!\n",Gun_No+1,Globa->Control_DC_Infor_Data[Gun_No].VIN);
							
							first_get_bms_info = 2;//避免重复写记录，获取到VIN后立即写1次
						}else
						{
							printf("=========获取到充电枪%d的车辆VIN，有无效的字符!\n",Gun_No+1);
							AppLogOut("========充电枪%d,当前充电车辆的VIN：%.17s 有非法字符!\n",Gun_No+1,Globa->Control_DC_Infor_Data[Gun_No].VIN);
							first_get_bms_info = 2;//避免重复写记录，获取到VIN后立即写1次
						}
					}
					temp_kwh = Globa->Control_DC_Infor_Data[Gun_No].kwh;
					//-------------------计费 更新数据库业务进度数据----------------------------
					pthread_mutex_lock(&busy_db_pmutex);
					if(Update_Charger_Busy_DB(chg_db_sel,id, &s_busy_frame,  0x55) == -1)
					{
						usleep(10000);
						Update_Charger_Busy_DB(chg_db_sel,id, &s_busy_frame, 0x55);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
					}
					pthread_mutex_unlock(&busy_db_pmutex);
					
				}
				
				if(Globa->Control_DC_Infor_Data[Gun_No].charger_start_way == CHG_BY_CARD)
				{
					if(Globa->Control_DC_Infor_Data[Gun_No].BMS_Info_falg)//已获取到VIN
					{
						if((Globa->Control_DC_Infor_Data[Gun_No].Bind_BmsVin_flag) && (1 == check_bmsVin))//已获取到VIN-只判断1次
						{
							check_bmsVin = 0;
							printf("========卡号%.16s绑定的VIN是%.17s,当前充电车辆的VIN是%.17s \n",Globa->Control_DC_Infor_Data[Gun_No].card_sn,Globa->Control_DC_Infor_Data[Gun_No].Bind_BmsVin,Globa->Control_DC_Infor_Data[Gun_No].VIN);
							//if( 0 != memcmp(Globa->Control_DC_Infor_Data[Gun_No].Bind_BmsVin,"00000000000000000",17))//有绑定卡与VIN
							{
								if( 0 != memcmp(Globa->Control_DC_Infor_Data[Gun_No].Bind_BmsVin,Globa->Control_DC_Infor_Data[Gun_No].VIN,17))
								{
									AppLogOut("========卡号%.16s绑定的VIN是%.17s,当前充电车辆的VIN是%.17s不匹配! \n",Globa->Control_DC_Infor_Data[Gun_No].card_sn,Globa->Control_DC_Infor_Data[Gun_No].Bind_BmsVin,Globa->Control_DC_Infor_Data[Gun_No].VIN);
								
									//VIN不匹配，停止充电
									Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
									Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = BY_VIN_MISMATCH;									
									s_busy_frame.end_reason = BY_VIN_MISMATCH;//VIN不匹配
								}
							}
							
						}
					}					
					
					if(Globa->Control_DC_Infor_Data[Gun_No].Credit_card_success_flag == 1)
					{//表示查询到卡片
						printf("------查询到刷卡停止------\n");

						Globa->Control_DC_Infor_Data[Gun_No].Credit_card_success_flag = 0;
						
						Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;//置充电停止标志等待充电停止
						Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = BY_SWIPE_CARD;//刷卡主动停止						
						s_busy_frame.end_reason = BY_SWIPE_CARD;//刷卡结束	
						
						Globa->Control_DC_Infor_Data[Gun_No].MT625_Card_Start_ok_flag = 0;
					}
				
					//判断有序充电卡是否达到有序充电卡的终止时间=====
					if(Globa->Control_DC_Infor_Data[Gun_No].order_chg_flag==0xAA)
					{
						if((Globa->Control_DC_Infor_Data[Gun_No].order_card_end_time[0] != 99)&&
						   (Globa->Control_DC_Infor_Data[Gun_No].order_card_end_time[1] != 99)
						  )
						  {
								systime = time(NULL);         //获得系统的当前时间 
								localtime_r(&systime, &tm_t);  //结构指针指向当前时间
								int cur_hhmm = (tm_t.tm_hour<<8)|tm_t.tm_min;
								int end_hhmm = (Globa->Control_DC_Infor_Data[Gun_No].order_card_end_time[0] << 8)| Globa->Control_DC_Infor_Data[Gun_No].order_card_end_time[1];
								if( cur_hhmm >= end_hhmm)//充电时间到
								{
									printf("------有序充电卡充电时间到，充电停止------\n");
									Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;//置充电停止标志等待充电停止
									Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = BY_ORDER_CARD_TIME_OVER;//有序充电卡时间到停止充电
									s_busy_frame.end_reason = BY_ORDER_CARD_TIME_OVER;//有序充电卡时间到停止充电
								}
						  }
					}
				
				}
				if(Globa->Control_DC_Infor_Data[Gun_No].charger_start_way == CHG_BY_APP)//APP启动
				{
					if(Globa->Control_DC_Infor_Data[Gun_No].App.APP_start == 0)//手机终止充电
					{	
						printf("------手机终止充电停止------\n");
						Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
						Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = BY_APP_END;//
						s_busy_frame.end_reason = BY_APP_END;//结束原因代码
					}
				}
				//VIN鉴权充电====只能按充电条件停止了，或急停或余额不足停
				if(Globa->Control_DC_Infor_Data[Gun_No].charger_start_way == CHG_BY_VIN)//VIN启动
				{
					if(Globa->Control_DC_Infor_Data[Gun_No].VIN_Stop >= 2)//点2次，实际上1次就可以了
					{
						Globa->Control_DC_Infor_Data[Gun_No].VIN_Stop = 0;
						printf("------VIN充电点停止按钮停止充电------\n");
						Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
						Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = BY_PRESS_STOP_BUTTON;//
						s_busy_frame.end_reason = BY_PRESS_STOP_BUTTON;//结束原因代码
					}
				}
				if(Globa->Control_DC_Infor_Data[Gun_No].remote_stop)
				{
					Globa->Control_DC_Infor_Data[Gun_No].remote_stop = 0;
					Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
					Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = BY_REMOTE_STOP;
					s_busy_frame.end_reason = BY_REMOTE_STOP;//服务器下发停止
				}
				if(Globa->Control_DC_Infor_Data[Gun_No].charger_start == 0)
				{//充电彻底结束处理
					Globa->Control_DC_Infor_Data[Gun_No].App.APP_start= 0;
					Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
					printf("------表示充电彻底结束处理------\n");
				}
				
				if(Globa->Electric_pile_workstart_Enable[Gun_No] == 0)//服务器远程禁用
				{	
					printf("------服务器远程禁用充电停止------\n");
					Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
					Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = BY_REMOTE_STOP;//
					s_busy_frame.end_reason = BY_REMOTE_STOP;//结束原因代码
				}

				if(((Globa->Control_DC_Infor_Data[Gun_No].charger_start_way == CHG_BY_CARD))||//用户卡启动
					(Globa->Control_DC_Infor_Data[Gun_No].charger_start_way == CHG_BY_APP)||
					(Globa->Control_DC_Infor_Data[Gun_No].charger_start_way == CHG_BY_VIN)
				)
				{
					if(Globa->Control_DC_Infor_Data[Gun_No].last_rmb > 0)//不是离线刷卡充电时才判断
					{
						if(Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_falg == 1)
						{
							if(Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Hostgun == 1)//双枪同时充需判断总金额
							{
								if((Globa->Control_DC_Infor_Data[Gun_No].total_rmb + Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Othergun].total_rmb )>= (Globa->Control_DC_Infor_Data[Gun_No].last_rmb - 100))//改到提前1元终止，防止余额为负190115 //20))//提前0.20元终止
								{//余额不足提前结束
									Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
									Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = BY_LACK_OF_MONEY;
									s_busy_frame.end_reason = BY_LACK_OF_MONEY;									
								}
							}
						}
						else 
						{
							if((Globa->Control_DC_Infor_Data[Gun_No].total_rmb )>= (Globa->Control_DC_Infor_Data[Gun_No].last_rmb - 100))//改到提前1元终止，防止余额为负190115 //20))//提前0.20元终止
							{//余额不足提前结束
								Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
								Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = BY_LACK_OF_MONEY;
								s_busy_frame.end_reason = BY_LACK_OF_MONEY;
								printf("------余额不足停止充电-----\n");
							}
					  }
					}
				}
				
				if(Globa->Control_DC_Infor_Data[Gun_No].charge_mode == MONEY_MODE)
				{//按金额充电
					//if(Globa->Control_DC_Infor_Data[Gun_No].rmb >= (Globa->Control_DC_Infor_Data[Gun_No].charge_money_limit- 20))
					if((Globa->Control_DC_Infor_Data[Gun_No].total_rmb )>= (Globa->Control_DC_Infor_Data[Gun_No].charge_money_limit- 20))//提前0.2元结束
					{
						Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
						Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = BY_MONEY_REACH;
						s_busy_frame.end_reason = BY_MONEY_REACH;
						printf("------按金额充电结束-----\n");
					}
				}else if(Globa->Control_DC_Infor_Data[Gun_No].charge_mode == KWH_MODE)
				{//按电量充电
					if((Globa->Control_DC_Infor_Data[Gun_No].kwh) >= Globa->Control_DC_Infor_Data[Gun_No].charge_kwh_limit*METER_READ_MULTIPLE){
						Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
						Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = BY_KWH_REACH;
						s_busy_frame.end_reason = BY_KWH_REACH;
						printf("------按电量充电结束-----\n");
					}
				}else if(Globa->Control_DC_Infor_Data[Gun_No].charge_mode == TIME_MODE)
				{//按时间充电					
					if(Globa->Control_DC_Infor_Data[Gun_No].Time >= Globa->Control_DC_Infor_Data[Gun_No].charge_sec_limit-1)//提前1秒终止
					{
						Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
						Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = BY_TIME_OVER;
						s_busy_frame.end_reason = BY_TIME_OVER;
						printf("------按时间充电结束-----\n");
					}
				}
				if(Globa->Control_DC_Infor_Data[Gun_No].Error_system != 0)//系统有故障
				{
					Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
					s_busy_frame.end_reason = BY_CHARGER_FAULT;					
					if(Globa->Control_DC_Infor_Data[Gun_No].Error.emergency_switch )
						s_busy_frame.end_reason = BY_EMERGY_STOP;
					else if(Globa->Control_DC_Infor_Data[Gun_No].Error.meter_connect_lost)
						s_busy_frame.end_reason = BY_DC_METER_FAIL;
					else if( Globa->Control_DC_Infor_Data[Gun_No].Error.Door_No_Close_Fault)
						s_busy_frame.end_reason = BY_DOOR_OPEN;
					else if(Globa->Control_DC_Infor_Data[Gun_No].Error.gun_lock_Fault)
						s_busy_frame.end_reason = BY_DC_PLUGLOCK_FAIL;
					else if(Globa->Control_DC_Infor_Data[Gun_No].Error.output_switch[0])
						s_busy_frame.end_reason = BY_DC_CONTACT_FAULT;
					else if(Globa->Control_DC_Infor_Data[Gun_No].Error.COUPLE_switch)
						s_busy_frame.end_reason = BY_BINGJI_RELAY_FAIL;	
					Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = s_busy_frame.end_reason;
				}

 
        if( 1 == Globa->Charger_param.CurrentVoltageJudgmentFalg)		
				{//电表电压异常判断	
					if(0 == check_meter_volt_st)//检查电表电压与控制板电压压差
					{
						if(Globa->Control_DC_Infor_Data[Gun_No].Time >  120)//2分钟后判断电表电压与控制板电压差异
						{						
							if(abs(Globa->Control_DC_Infor_Data[Gun_No].Output_voltage - Globa->Control_DC_Infor_Data[Gun_No].meter_voltage) > 50000)//电压差异大于50.000V
							{
								volt_abnormal_timestamp = GetTickCount();
								check_meter_volt_st = 1;
							}
						}	
					}else//电压异常是否达到1分钟
					{
						if(abs(Globa->Control_DC_Infor_Data[Gun_No].Output_voltage - Globa->Control_DC_Infor_Data[Gun_No].meter_voltage) < 20000)//压差小于20V
						{
							check_meter_volt_st = 0;//clear						
						}
						if(abs(Globa->Control_DC_Infor_Data[Gun_No].Output_voltage - Globa->Control_DC_Infor_Data[Gun_No].meter_voltage) > 50000)//电压差异大于50.000V
						{
							if((GetTickCount() - volt_abnormal_timestamp) > 60)//超过1分钟了，需停止充电
							{
								Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
								Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = BY_METER_VOLT_ABNORMAL;
								s_busy_frame.end_reason = BY_METER_VOLT_ABNORMAL;
								printf("------电表电压与控制板电压相差太大，充电结束-----\n");							
							}
						}
					}


					if(0 == check_meter_current_st)
					{
						if(Globa->Control_DC_Infor_Data[Gun_No].Time >  120)//2分钟后判断电表电流与控制板电流差异
						{			
							if(((abs(Globa->Control_DC_Infor_Data[Gun_No].Output_current - Globa->Control_DC_Infor_Data[Gun_No].meter_current)*100 > METER_CURR_THRESHOLD_VALUE*Globa->Control_DC_Infor_Data[Gun_No].Output_current)&&(Globa->Control_DC_Infor_Data[Gun_No].Output_current >= LIMIT_CURRENT_VALUE))
							||((abs(Globa->Control_DC_Infor_Data[Gun_No].Output_current - Globa->Control_DC_Infor_Data[Gun_No].meter_current) > 1000)&&(Globa->Control_DC_Infor_Data[Gun_No].Output_current < LIMIT_CURRENT_VALUE)&&(Globa->Control_DC_Infor_Data[Gun_No].Output_current >= MIN_LIMIT_CURRENT_VALUE)))
							{					
								current_abnormal_timestamp = GetTickCount();
								check_meter_current_st = 1;
							}
						}	
					}else//电流异常是否达到1分钟
					{
						if(abs(Globa->Control_DC_Infor_Data[Gun_No].Output_current - Globa->Control_DC_Infor_Data[Gun_No].meter_current) < 10000)//
						{
							check_meter_current_st = 0;//clear						
						}
						
						if(((abs(Globa->Control_DC_Infor_Data[Gun_No].Output_current - Globa->Control_DC_Infor_Data[Gun_No].meter_current)*100 > METER_CURR_THRESHOLD_VALUE*Globa->Control_DC_Infor_Data[Gun_No].Output_current)&&(Globa->Control_DC_Infor_Data[Gun_No].Output_current >= LIMIT_CURRENT_VALUE))
							||((abs(Globa->Control_DC_Infor_Data[Gun_No].Output_current - Globa->Control_DC_Infor_Data[Gun_No].meter_current) > 1000)&&(Globa->Control_DC_Infor_Data[Gun_No].Output_current < LIMIT_CURRENT_VALUE)&&(Globa->Control_DC_Infor_Data[Gun_No].Output_current >= MIN_LIMIT_CURRENT_VALUE)))
						{
							if((GetTickCount() - current_abnormal_timestamp) > 60)//超过1分钟了，需停止充电
							{
								Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
								Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = BY_METER_CURRENT_ABNORMAL;
								s_busy_frame.end_reason = BY_METER_CURRENT_ABNORMAL;
								printf("------电表电流与控制板电流相差太大，充电结束-----\n");							
							}
						}
					}
				}

			
			if(Globa->Control_DC_Infor_Data[Gun_No].Time >  300)//5分钟以后后判断
			{
				if( Globa->Control_DC_Infor_Data[Gun_No].kwh < 20*METER_READ_MULTIPLE)//0.20度以下
				{
					Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
					Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = BY_METER_CURRENT_ABNORMAL;
					s_busy_frame.end_reason = BY_METER_CURRENT_ABNORMAL;
					printf("------电表电量计量异常-----\n");	
				}
			}
			
			if(Globa->Control_DC_Infor_Data[Gun_No].Time >  120)//2分钟以后后判断
			{
				if(0 == check_current_low_st2)
				{
					if(Globa->Control_DC_Infor_Data[Gun_No].BMS_batt_SOC >= Globa->Charger_param.SOC_limit)//
					{						
						if(Globa->Control_DC_Infor_Data[Gun_No].Output_current < (Globa->Charger_param.min_current*1000))//电流低于限值
						{
							current_low_timestamp2 = GetTickCount();
							check_current_low_st2 = 1;
						}
					}	
				}else//电流低持续是否达到1分钟
				{
					if(Globa->Control_DC_Infor_Data[Gun_No].Output_current > (Globa->Charger_param.min_current*1000 + 1000))//1A回差
					{
						check_current_low_st2 = 0;//clear						
					}
					if(Globa->Control_DC_Infor_Data[Gun_No].BMS_batt_SOC < Globa->Charger_param.SOC_limit   )
						check_current_low_st2 = 0;//clear
					if(Globa->Control_DC_Infor_Data[Gun_No].Output_current < (Globa->Charger_param.min_current*1000))
					{
						if((GetTickCount() - current_low_timestamp2) > 30)//超过30s，需停止充电
						{
							Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
							Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = BY_DC_LimitSOC_Value;
							s_busy_frame.end_reason = BY_DC_LimitSOC_Value;
						}
					}				
				}
#if 1
				if(Globa->Control_DC_Infor_Data[Gun_No].BMS_Info_falg == 1)//收到BMs额定信息
				{
					if(Globa->Control_DC_Infor_Data[Gun_No].kwh >= ((100 - Globa->Control_DC_Infor_Data[Gun_No].BMS_batt_start_SOC + MAX_LIMIT_OVER_RATEKWH)*Globa->Control_DC_Infor_Data[Gun_No].Battery_Rate_KWh))
					{
						if(abs(GetTickCount() - current_overkwh_timestamp) > OVER_RATEKWH_COUNT)//超过1分钟了，需停止充电
						{
							Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
							Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = BY_DC_Over_KWH;
							s_busy_frame.end_reason = BY_DC_Over_KWH;
							
							SysControlLogOut("%d#枪计费QT判断达到根据总电量和起始SOC算出需要的总电量：Battery_Rate_KWh =%d.%d  充电电量 kwh = %d.%d ：startsoc=%d endsoc=%d",Gun_No+1,Globa->Control_DC_Infor_Data[Gun_No].Battery_Rate_KWh/10,Globa->Control_DC_Infor_Data[Gun_No].Battery_Rate_KWh%10,Globa->Control_DC_Infor_Data[Gun_No].kwh/1000,Globa->Control_DC_Infor_Data[Gun_No].kwh%1000,Globa->Control_DC_Infor_Data[Gun_No].BMS_batt_start_SOC,Globa->Control_DC_Infor_Data[Gun_No].BMS_batt_SOC);
							current_overkwh_timestamp = GetTickCount();
						}
					}else{
						current_overkwh_timestamp = GetTickCount();
					}
				}else{
					current_overkwh_timestamp = GetTickCount();
				}
#endif
			}
			
				if(Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_falg == 1)
				{
					if(Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Othergun].gun_link == 0)//另1把枪拔枪，本枪也要停
					{
						Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = BY_DC_PLUG_OUT;
						s_busy_frame.end_reason = Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason;
						Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;							
						//Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Othergun].charge_end_reason = Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason;							
						//Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Othergun].Manu_start = 0;
						//printf("=====Globa->Control_DC_Infor_Data[%d].charge_end_reason=%d",Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Othergun,Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason);
					}
				}	
				if(Globa->Control_DC_Infor_Data[Gun_No].Manu_start == 0)//充电需要结束
				{
					Globa->Control_DC_Infor_Data[Gun_No].Metering_Step = 0x06;//等待充电控制板充电结束
				  
					SysControlLogOut("%d#枪计费结束原因：0%d",Gun_No+1,s_busy_frame.end_reason);


					if(Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_falg == 1)
					{
						if(Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Hostgun == 1)//主枪要停了，辅枪也一定要停
						{
							s_busy_frame.end_reason = Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason;
							Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Othergun].charge_end_reason = Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason;							
							Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Othergun].Manu_start = 0;
							printf("=====Globa->Control_DC_Infor_Data[%d].charge_end_reason=%d",Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Othergun,Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason);
						}
					}
				}
				break;
			}
			
			case 0x03:{//---------------2 充电结束 (应该是等待结算---自动结束或者刷卡，但是没有结算)      2.3-------------
				// if(Globa->Control_DC_Infor_Data[Gun_No].gun_link == 1)
					// break;//没拔枪状态不走
				if((CHG_BY_CARD == Globa->Control_DC_Infor_Data[Gun_No].charger_start_way )||
					(CHG_BY_VIN == Globa->Control_DC_Infor_Data[Gun_No].charger_start_way )
					)//刷卡或VIN启动，等待充电工抄数据后再走------
				{
					if(Globa->Control_DC_Infor_Data[Gun_No].gun_link != 1)//已拔枪
					{
						if(0==Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_falg )//单独充再走
						{
							count_wait++;
							if(count_wait > 1000)//10秒
							{//等待时间超时
								count_wait = 0;
								Globa->Control_DC_Infor_Data[Gun_No].charger_state = 0x03;
								Globa->Control_DC_Infor_Data[Gun_No].Metering_Step = 0x00;
								//break;//超时必须退出
							}
						}else//双枪同充等主枪点完确定再走
						{
							
							
						}
					}
					if(Globa->Control_DC_Infor_Data[Gun_No].gun_link == 1)
					{
						if((Globa->Control_DC_Infor_Data[Gun_No].kwh > 0)&&(Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason != BY_VIN_AUTH_FAILED))//有电量等待拔枪或在结算界面点确定后再走
							if(Globa->Control_DC_Infor_Data[Gun_No].MT625_Card_End_ok_flag == 1)
								break;//没拔枪状态不走
					}
					//printf("Globa->Control_DC_Infor_Data[%d].MT625_Card_End_ok_flag=%d\n",Gun_No,Globa->Control_DC_Infor_Data[Gun_No].MT625_Card_End_ok_flag);//test
					if(Globa->Control_DC_Infor_Data[Gun_No].MT625_Card_End_ok_flag >= 2)//1)//不在卡上扣费，直接走
					{//等待刷卡结束成功
						if(1==s_busy_frame.card_deal_result)//用户卡
						{
							s_busy_frame.card_deal_result = 0;//交易成功，已扣费解锁												
							if(id != 0)
							{
								pthread_mutex_lock(&busy_db_pmutex);
								Update_Charger_Busy_DB(chg_db_sel, id, &s_busy_frame,  0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
								pthread_mutex_unlock(&busy_db_pmutex);											
							}
						}
						//------------结算终于结束，退出结算界面--------------------------
						
						Globa->Control_DC_Infor_Data[Gun_No].Credit_card_success_flag = 0;
						Globa->Control_DC_Infor_Data[Gun_No].MT625_Card_End_ok_flag = 0;
						Globa->Control_DC_Infor_Data[Gun_No].MT625_Card_Start_ok_flag = 0;
						Globa->Control_DC_Infor_Data[Gun_No].Metering_Step = 0x00;
						Globa->Control_DC_Infor_Data[Gun_No].charger_state = 0x03;
						Globa->Control_DC_Infor_Data[Gun_No].Operation_command = 2;//进行解锁
						count_wait = 0;
					}
				}else//APP或VIN
				{	
					sleep(3);//test let qt show checkout
					//------------结算终于结束，退出结算界面--------------------------
									
					Globa->Control_DC_Infor_Data[Gun_No].Metering_Step = 0x00;
					Globa->Control_DC_Infor_Data[Gun_No].charger_state = 0x03;
					Globa->Control_DC_Infor_Data[Gun_No].Operation_command = 2;//进行解锁
					count_wait = 0;					
				}
					
				
				if(0x00 == Globa->Control_DC_Infor_Data[Gun_No].Metering_Step)//退出结算界面--------------------------
				{
					if(Globa->Charger_param.NEW_VOICE_Flag == 1)
						Voicechip_operation_function(0x09);//09H     “谢谢惠顾，再见”					
					else
						Voicechip_operation_function(0x11);//11H	谢谢使用再见.wav					
				}
				break;
			}
			case 0x04:{//倒计时充电===
				count_wait++;
				if(count_wait > 30)
				{//等待时间超时300mS
					count_wait = 0;
					//判断预约时间是否到
					time_t systime;
					struct tm tm_t;

					systime = time(NULL);        //获得系统的当前时间 
					localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
					if(Globa->Control_DC_Infor_Data[Gun_No].gun_link != 1)
					{
						s_busy_frame.end_reason = BY_DC_PLUG_OUT;//刷卡结束预约
						Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = s_busy_frame.end_reason;
						Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
						Globa->Control_DC_Infor_Data[Gun_No].Metering_Step = 0x06; // 
						Globa->Control_DC_Infor_Data[Gun_No].MT625_Card_Start_ok_flag = 0;							
						Globa->Control_DC_Infor_Data[Gun_No].Credit_card_success_flag = 0;
						break;
					}else if(Globa->Control_DC_Infor_Data[Gun_No].Error_system != 0)//系统有故障
					{	
						s_busy_frame.end_reason = BY_CHARGER_FAULT;//
						if(Globa->Control_DC_Infor_Data[Gun_No].Error.emergency_switch )
							s_busy_frame.end_reason = BY_EMERGY_STOP;
						else if(Globa->Control_DC_Infor_Data[Gun_No].Error.meter_connect_lost)
							s_busy_frame.end_reason = BY_DC_METER_FAIL;
						else if( Globa->Control_DC_Infor_Data[Gun_No].Error.Door_No_Close_Fault)
							s_busy_frame.end_reason = BY_DOOR_OPEN;
						
						Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = s_busy_frame.end_reason;
						
						Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
						Globa->Control_DC_Infor_Data[Gun_No].Metering_Step = 0x06; // 
						Globa->Control_DC_Infor_Data[Gun_No].MT625_Card_Start_ok_flag = 0;							
						Globa->Control_DC_Infor_Data[Gun_No].Credit_card_success_flag = 0;		
						break;
					}
					
					if(Globa->Control_DC_Infor_Data[Gun_No].charger_start_way == CHG_BY_CARD)
					{
						if(Globa->Control_DC_Infor_Data[Gun_No].Credit_card_success_flag == 1)
						{//表示查询到刷卡取消预约
							s_busy_frame.end_reason = BY_SWIPE_CARD;//刷卡结束预约
							Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = s_busy_frame.end_reason;
							Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
							Globa->Control_DC_Infor_Data[Gun_No].Metering_Step = 0x06; // 
							Globa->Control_DC_Infor_Data[Gun_No].MT625_Card_Start_ok_flag = 0;							
							Globa->Control_DC_Infor_Data[Gun_No].Credit_card_success_flag = 0;
							break;
						}
					}
					if(Globa->Electric_pile_workstart_Enable[Gun_No] == 0)//服务器远程禁用
					{	
						printf("------服务器远程禁用充电桩，预约充电停止------\n");
						Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
						Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = BY_REMOTE_STOP;//
						s_busy_frame.end_reason = BY_REMOTE_STOP;//结束原因代码
						Globa->Control_DC_Infor_Data[Gun_No].Metering_Step = 0x06; // 
						break;
					}
					if((tm_t.tm_min + tm_t.tm_hour*60) == Globa->Control_DC_Infor_Data[Gun_No].pre_time)
					{
						Globa->Control_DC_Infor_Data[Gun_No].charge_mode = AUTO_MODE;//1;
						Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 1;
						Globa->Control_DC_Infor_Data[Gun_No].charger_start = 1;
						Globa->Control_DC_Infor_Data[Gun_No].charger_state = 0x04;
						Globa->Control_DC_Infor_Data[Gun_No].App.APP_start = 0;
						Globa->Control_DC_Infor_Data[Gun_No].Metering_Step = 0x02;   // 2 自动充电-》 充电界面 状态 2.1-
						count_wait = 0;
						if(INPUT_RELAY_CTL_MODULE == Globa->Charger_param.Input_Relay_Ctl_Type)//模块节能控制
						{
							if(ac_out == 0)//模块还没上电
								need_wait_module_poweron = 1;
							else
								need_wait_module_poweron =  0;
							Globa->Control_DC_Infor_Data[Gun_No].AC_Relay_control = 1;//确保模块开启
						}
						break;
					}else
					{
						//定时刷新数据
						if(Globa->Control_DC_Infor_Data[Gun_No].pre_time*60 >= (tm_t.tm_sec + tm_t.tm_min*60 + tm_t.tm_hour*3600))
						{
							Globa->Control_DC_Infor_Data[Gun_No].remain_hour =    (Globa->Control_DC_Infor_Data[Gun_No].pre_time*60 -(tm_t.tm_sec + tm_t.tm_min*60 + tm_t.tm_hour*3600))/3600;		
							Globa->Control_DC_Infor_Data[Gun_No].remain_min  =   ((Globa->Control_DC_Infor_Data[Gun_No].pre_time*60 -(tm_t.tm_sec + tm_t.tm_min*60 + tm_t.tm_hour*3600))%3600)/60;
							Globa->Control_DC_Infor_Data[Gun_No].remain_second = ((Globa->Control_DC_Infor_Data[Gun_No].pre_time*60 -(tm_t.tm_sec + tm_t.tm_min*60 + tm_t.tm_hour*3600))%3600)%60;
						}else
						{
							Globa->Control_DC_Infor_Data[Gun_No].remain_hour =    (Globa->Control_DC_Infor_Data[Gun_No].pre_time*60 + 24*3600 -(tm_t.tm_sec + tm_t.tm_min*60 + tm_t.tm_hour*3600))/3600;		
							Globa->Control_DC_Infor_Data[Gun_No].remain_min  =   ((Globa->Control_DC_Infor_Data[Gun_No].pre_time*60 + 24*3600  -(tm_t.tm_sec + tm_t.tm_min*60 + tm_t.tm_hour*3600))%3600)/60;
							Globa->Control_DC_Infor_Data[Gun_No].remain_second = ((Globa->Control_DC_Infor_Data[Gun_No].pre_time*60 + 24*3600  -(tm_t.tm_sec + tm_t.tm_min*60 + tm_t.tm_hour*3600))%3600)%60;
						}
						break;
					}
				}
				break;
			}
			
			case 0x05://等待VIN鉴权或超时===1分钟超时
			{
				count_wait = 0;
				do{
					sleep(1);
					 count_wait++;
					 if(count_wait > 70)//120)
						 break;
					 if(Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag > 0)//控制板已结束
						break;
					 if(Globa->Control_DC_Infor_Data[Gun_No].Error.emergency_switch )
						break;
				}while(1==Globa->Control_DC_Infor_Data[Gun_No].VIN_start);
				
				if((Globa->Control_DC_Infor_Data[Gun_No].Error.emergency_switch )||
					(Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag > 0)||
					(2 != Globa->Control_DC_Infor_Data[Gun_No].VIN_start))//not chargeable ,end charge
				{
					printf("VIN auth failed stop chg...\n");
					if( (Globa->Control_DC_Infor_Data[Gun_No].VIN_start > 2)&&
						(Globa->Control_DC_Infor_Data[Gun_No].VIN_start < 6)//6--没获取到VIN,7-等待VIN期间按了急停
					)//发送过鉴权再报启动结果
					{
						Globa->Control_DC_Infor_Data[Gun_No].VIN_chg_start_result = CHG_START_FAILED;
						Globa->Control_DC_Infor_Data[Gun_No].report_VIN_chg_started = 1;
					}
					Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
					Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = BY_VIN_AUTH_FAILED;
					s_busy_frame.end_reason = BY_VIN_AUTH_FAILED;//	
					
					if(Globa->Control_DC_Infor_Data[Gun_No].Error.emergency_switch )
					{
						Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = BY_EMERGY_STOP;
						s_busy_frame.end_reason = BY_EMERGY_STOP;//	
					}else	if(Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag > 0)//控制板已结束
					{
						s_busy_frame.end_reason = Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag + DC_CTRL_STOP_REASON_OFFSET;
						Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = s_busy_frame.end_reason;						
					}
					
					Globa->Control_DC_Infor_Data[Gun_No].Metering_Step = 0x06;//等待充电控制板充电结束
					Globa->Control_DC_Infor_Data[Gun_No].Operation_command = 2;//进行解锁						
				}
				else//急停没按下，且返回鉴权成功
				{
					printf("VIN auth ok start chg...\n");
					if(id != 0)
					{
						pthread_mutex_lock(&busy_db_pmutex);
						if(Update_VIN_CardSN(chg_db_sel,id,Globa->Control_DC_Infor_Data[Gun_No].VIN_User_Id ) == -1)
						{
							usleep(10000);
							Update_VIN_CardSN(chg_db_sel,id,Globa->Control_DC_Infor_Data[Gun_No].VIN_User_Id );//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
						}
						pthread_mutex_unlock(&busy_db_pmutex);
					}
					Globa->Control_DC_Infor_Data[Gun_No].VIN_chg_start_result = CHG_START_SUCCESS;
					Globa->Control_DC_Infor_Data[Gun_No].report_VIN_chg_started = 1;
	
					//Globa->Control_DC_Infor_Data[Gun_No].VIN_start =  0;
					Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 1;					
					Globa->Control_DC_Infor_Data[Gun_No].charger_start = 1;
					Globa->Control_DC_Infor_Data[Gun_No].Metering_Step = 0x02;//开始计费
					Globa->Control_DC_Infor_Data[Gun_No].charger_state = 0x04;
				}
			}
			break;
			
			case 0x06://等待控制单元充电结束=====无超时test
			{
				s_busy_frame.end_reason = Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason;//
				if(Globa->Control_DC_Infor_Data[Gun_No].charger_start == 0)
				{//充电彻底结束处理					
					Globa->Control_DC_Infor_Data[Gun_No].Manu_start = 0;
					printf("------表示充电彻底结束处理---%d---\n",Gun_No);
					sleep(2);
					{//================================充电费用计算===========================================
						Globa->Control_DC_Infor_Data[Gun_No].meter_stop_KWH = (Globa->Control_DC_Infor_Data[Gun_No].meter_KWH >= Globa->Control_DC_Infor_Data[Gun_No].meter_stop_KWH)? Globa->Control_DC_Infor_Data[Gun_No].meter_KWH:Globa->Control_DC_Infor_Data[Gun_No].meter_stop_KWH;//取初始电表示数
						Globa->Control_DC_Infor_Data[Gun_No].kwh = (Globa->Control_DC_Infor_Data[Gun_No].meter_stop_KWH - Globa->Control_DC_Infor_Data[Gun_No].meter_start_KWH);
						
						soft_KWH = Globa->Control_DC_Infor_Data[Gun_No].kwh;//电量放大到3位小数 0.001kwh
						if(Globa->Charger_param.meter_config_flag != DCM_DJS3366)//不是3位小数的电表
						{
							
							if((soft_KWH/10) <= (Globa->Control_DC_Infor_Data[Gun_No].pre_kwh/10))
								soft_KWH = Globa->Control_DC_Infor_Data[Gun_No].pre_kwh;//不加防止进位了 + rand()%10;
							else{
								soft_KWH += Get_rand()%5;//会不会出现电量倒退?
								Globa->Control_DC_Infor_Data[Gun_No].pre_kwh = soft_KWH;
							}
						}
						
						Globa->Control_DC_Infor_Data[Gun_No].soft_KWH = soft_KWH;
			
						Globa->Control_DC_Infor_Data[Gun_No].ulcharged_kwh[pre_price_index] = Globa->Control_DC_Infor_Data[Gun_No].kwh - ul_start_kwh;//累加当前时段的耗电量	
						cur_price_index = GetCurPriceIndex(Gun_No); //获取当前时间
						if(cur_price_index != pre_price_index)
						{
							ul_start_kwh = Globa->Control_DC_Infor_Data[Gun_No].kwh - Globa->Control_DC_Infor_Data[Gun_No].ulcharged_kwh[cur_price_index];//load new start kwh,需要减去时段之前累加的电量，否则该时段之前累加的电量丢失----20160312
							pre_price_index = cur_price_index;							
						}
						share_kwh_sum = 0;					
						for(i=0;i<4;i++)
							share_kwh_sum += Globa->Control_DC_Infor_Data[Gun_No].ulcharged_kwh[i];
						if(share_kwh_sum < Globa->Control_DC_Infor_Data[Gun_No].kwh)
						{
							delta_kwh = Globa->Control_DC_Infor_Data[Gun_No].kwh - share_kwh_sum;
							sprintf(temp_char,"=======分时电量累加和小于电表当前读数与起始充电读数的差，少%d.%02dkwh,错误!\n",delta_kwh/1000,delta_kwh%1000);
							printf(temp_char);
							if(abnormal_Log_en)
							{
								AppLogOut(temp_char);
								abnormal_Log_en = 0;//1次充电只记录1次
							}
							Globa->Control_DC_Infor_Data[Gun_No].ulcharged_kwh[pre_price_index] +=   delta_kwh;//强制增加test
						}
						for(i = 0;i<4;i++)
						{
							ull_temp = Globa->Control_DC_Infor_Data[Gun_No].ulcharged_kwh[i];//0.01kwh
							ull_charged_rmb[i] = (ull_temp/METER_READ_MULTIPLE)*share_time_kwh_price[i];
							ull_service_rmb[i] = (ull_temp/METER_READ_MULTIPLE)*share_service_price[i];
						}
						ull_kwh_rmb = 0;
						ull_total_service_rmb = 0;
						for(i = 0;i<4;i++)
						{	
							ull_kwh_rmb += ull_charged_rmb[i];//总电费，7位小数
							ull_total_service_rmb += ull_service_rmb[i];//总服务费，7位小数
						}				
						
						for(i = 0;i<4;i++)//分时电费和服务费0.01				
						{
							Globa->Control_DC_Infor_Data[Gun_No].ulcharged_rmb[i] = (ull_charged_rmb[i] + COMPENSATION_VALUE)/ PRICE_DIVID_FACTOR;//转0.01元
							Globa->Control_DC_Infor_Data[Gun_No].ulcharged_service_rmb[i] = (ull_service_rmb[i] + COMPENSATION_VALUE)/ PRICE_DIVID_FACTOR;//转0.01元
						}
						//Globa->Control_DC_Infor_Data[Gun_No].rmb = ull_kwh_rmb/PRICE_DIVID_FACTOR;//总电费,0.01元					
						
						ul_kwh_charge_rmb  = 0;//充电总电费 0.01元
						ul_kwh_service_rmb = 0;//充电总服务费 0.01元
						for(i = 0;i<4;i++)
						{
							ul_kwh_charge_rmb  += Globa->Control_DC_Infor_Data[Gun_No].ulcharged_rmb[i];
							ul_kwh_service_rmb += Globa->Control_DC_Infor_Data[Gun_No].ulcharged_service_rmb[i];
						}
						Globa->Control_DC_Infor_Data[Gun_No].rmb  = ul_kwh_charge_rmb;
						Globa->Control_DC_Infor_Data[Gun_No].service_rmb = ul_kwh_service_rmb;
						
						//Globa->Control_DC_Infor_Data[Gun_No].park_rmb = (Globa->Control_DC_Infor_Data[Gun_No].Time/600)*park_price/(PRICE_DIVID_FACTOR/100);//0.01元
						//ull_temp = (ull_kwh_rmb + ull_total_service_rmb)/PRICE_DIVID_FACTOR;	
						ull_temp = ul_kwh_charge_rmb + ul_kwh_service_rmb;//
											
						//ull_temp += Globa->Control_DC_Infor_Data[Gun_No].park_rmb;//add停车费
						Globa->Control_DC_Infor_Data[Gun_No].total_rmb = ull_temp;//总费用(电费，服务费，停车费)
					}				
				
					if(!((Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_falg == 1)&&(Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Hostgun == 0)))
					{	
						if(Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason == 0)//不是主动结束的
						{					 
							s_busy_frame.end_reason = Globa->Control_DC_Infor_Data[Gun_No].charger_over_flag + DC_CTRL_STOP_REASON_OFFSET;
							Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = s_busy_frame.end_reason;
							if(s_busy_frame.end_reason == BY_DC_FAULT_STOP)//可能是急停按下了
							{
								if(1 == Globa->Control_DC_Infor_Data[Gun_No].Error.emergency_switch)
									s_busy_frame.end_reason = BY_EMERGY_STOP;
							}
						}
					}
					else //辅枪
					{
						if(Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason == 0)//不是主动结束的
						{					 
							s_busy_frame.end_reason = Globa->Control_DC_Infor_Data[Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Othergun].charger_over_flag + DC_CTRL_STOP_REASON_OFFSET;
							Globa->Control_DC_Infor_Data[Gun_No].charge_end_reason = s_busy_frame.end_reason;
							if(s_busy_frame.end_reason == BY_DC_FAULT_STOP)//可能是急停按下了
							{
								if(1 == Globa->Control_DC_Infor_Data[Gun_No].Error.emergency_switch)
									s_busy_frame.end_reason = BY_EMERGY_STOP;
							}
						}
					}
					
					//-------------------扣费 更新业务进度数据----------------------------
					UpdateChgingBusyData(Gun_No,&s_busy_frame);					
					
					pthread_mutex_lock(&busy_db_pmutex);
					if(Update_Charger_Busy_DB(chg_db_sel,id, &s_busy_frame,  0) == -1)
					{
						usleep(10000);
						Update_Charger_Busy_DB(chg_db_sel,id, &s_busy_frame,  0);
					}
					pthread_mutex_unlock(&busy_db_pmutex);
					
					Globa->Control_DC_Infor_Data[Gun_No].Metering_Step = 0x03; // 等待界面结算完成					
					Globa->Control_DC_Infor_Data[Gun_No].charger_state = 0x05;//充电已完成
					Globa->Control_DC_Infor_Data[Gun_No].Credit_card_success_flag = 0;
					Globa->Control_DC_Infor_Data[Gun_No].MT625_Card_Start_ok_flag = 0;
					Globa->Control_DC_Infor_Data[Gun_No].App.APP_start= 0;
					
					if((Globa->Control_DC_Infor_Data[Gun_No].charger_start_way == CHG_BY_APP)||//手机APP充电
						(Globa->Control_DC_Infor_Data[Gun_No].charger_start_way == CHG_BY_VIN))//VIN鉴权充电
					{						
						Globa->Control_DC_Infor_Data[Gun_No].Operation_command = 2;//进行解锁
						break;
					}else
					{//刷卡	未结算不 解锁

						break;
					}
				}
			}
			break;
			
			default:{
				printf("枪%d+1计费逻辑进入未知状态!!!!!!!!!!!!!!\n",Gun_No);
				break;
			}
		}
	}
}


/*********************************************************
**description：直流计费系统
***parameter  :none
***return	  :none
**********************************************************/
extern void 	Billing_system_DC_Thread(void* gun_no)
{
	pthread_t td1;
	int ret ,stacksize = 1024*1024;
	pthread_attr_t attr;

	ret = pthread_attr_init(&attr);
	if(ret != 0){
		return;
	}

	ret = pthread_attr_setstacksize(&attr, stacksize);
	if(ret != 0){
		return;
	}
	
	/* 创建工作线程 */
	if(0 != pthread_create(&td1, &attr, (void *)Billing_system_DC_Task, gun_no))
	{
		perror("####pthread_create Billing_system_DC_Task ERROR ####\n\n");
	}

	ret = pthread_attr_destroy(&attr);
	if(ret != 0){
		return;
	}
}

