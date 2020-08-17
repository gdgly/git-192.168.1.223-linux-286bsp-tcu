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
#include "common.h"

extern UINT32 Main_2_Watchdog;
extern int  AP_APP_Sent_2_falg[2];            //2号.枪APP数据反馈及反馈结果

extern int valid_card_check(unsigned char *sn);
//自动充电-》充电枪连接确认
typedef struct _PAGE_1000_FRAME
{                                        
	UINT8   reserve1;        //保留

	UINT8   other_link;        //高位为有效符，低位为第二把枪的连接状态;        //保留
	UINT8   link;            //1:连接  0：断开
	UINT8   VIN_Support;        //保留
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

	UINT8   sn[16];          //卡号 数字字符
	UINT8   rmb[8];          //余额 数字字符 精确到分

}PAGE_200_FRAME;
static PAGE_200_FRAME Page_200;

/*
typedef struct _setmod{
    unsigned char nc;
    unsigned char mun1;
    unsigned char mun2;
}setmod;

充电方式 ：按金额，电量，时间
mun1低字节 mun2 高字节
单位依次： 元 度 分钟
如：mun1＝0x44; mun2＝0x1; 表示300
预约充电
mun1 为小时
mun2 为分钟
如：mun1＝0x14; mun2＝0x1; 表示 20：01
*/
typedef struct _PAGE_BMS_FRAME
{  
   UINT8 reserve1;                               
	 UINT8 BMS_Info_falg;
	 UINT8 BMS_H_vol[2];      //BMS最高允许充电电压  0.1V
	 UINT8 BMS_H_cur[2];      //BMS最高允许充电电流  0.01A
	 UINT8 BMS_cell_LT[2];       //5	电池组最低温度	8	BIN	2Byte	精确到小数点后0位 -50℃ ~ +200℃
	 UINT8 BMS_cell_HT[2];      //6	电池组最高温度	10BIN	2Byte	精确到小数点后0位 -50℃ ~ +200℃
	 UINT8 need_voltage[2];   //12	充电输出电压	2	BIN	2Byte	精确到小数点后1位0V - 950V
   UINT8 need_current[2];   //13	充电输出电流	4	BIN	2Byte	精确到小数点后2位0A - 400A
	 UINT8 BMS_Vel[3];         //BMS协议版本                                    
	 UINT8 Battery_Rate_AH[2]; //2	整车动力蓄电池额定容量0.1Ah
	 UINT8 Battery_Rate_Vol[2]; //整车动力蓄电池额定总电压
	 UINT8 Battery_Rate_KWh[2]; //动力蓄电池标称总能量KWH
	 UINT8 Cell_H_Cha_Vol[2];     //单体动力蓄电池最高允许充电电压 0.01 0-24v
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

//自动充电界面 一把枪还是两把枪共用一个界面
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


//倒计时界面
typedef struct _PAGE_DAOJISHI_FRAME
{                                        
   UINT8   reserve1;        //保留

   INT8   hour;            //剩余小时
   INT8   min;             //剩余分钟
   INT8   second;          //剩余秒
	 UINT8   type;            //类型 0---显示倒计时间，1--不显示到计时
	 UINT8   card_sn[16];     //卡号 16位
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
	
	cur_time_zone_num = Globa_2->Charger_param2.time_zone_num;
	
	if(Globa_2->Special_price_APP_data_falg == 1){
		cur_time_zone_num = Globa_2->Special_price_APP_data.time_zone_num;
		for(i=0;i<cur_time_zone_num;i++){
			if((cur_time >= Globa_2->Special_price_APP_data.time_zone_tabel[i]) && (cur_time < Globa_2->Special_price_APP_data.time_zone_tabel[(i+1)%cur_time_zone_num]))//开始点为闭区间,可以取等[1段开始,1段结束)，[2段开始,2段结束)..."1段结束"值=="2段开始"值
				break;
		}
		if(i<cur_time_zone_num){
			time_zone_index = i + 1;//since 1 
		}else{//最后1个时段
			time_zone_index = cur_time_zone_num;
		}
		cur_price_index = Globa_2->Special_price_APP_data.time_zone_feilv[time_zone_index -1];//取得第1个时间段的费率号
  }else	if((Globa_2->private_price_flag == 1)&&(Globa_2->private_price_acquireOK == 1)){//私有时区//获取私有电价
		cur_time_zone_num = Globa_2->Special_price_data.time_zone_num;
		for(i=0;i<cur_time_zone_num;i++){
			if((cur_time >= Globa_2->Special_price_data.time_zone_tabel[i]) && (cur_time < Globa_2->Special_price_data.time_zone_tabel[(i+1)%cur_time_zone_num]))//开始点为闭区间,可以取等[1段开始,1段结束)，[2段开始,2段结束)..."1段结束"值=="2段开始"值
				break;
		}
		if(i<cur_time_zone_num){
			time_zone_index = i + 1;//since 1 
		}else{//最后1个时段
			time_zone_index = cur_time_zone_num;
		}
		cur_price_index = Globa_2->Special_price_data.time_zone_feilv[time_zone_index -1];//取得第1个时间段的费率号
	}else{//通用时区
		cur_time_zone_num = Globa_2->Charger_param2.time_zone_num;
		for(i=0;i<cur_time_zone_num;i++){
			if((cur_time >= Globa_2->Charger_param2.time_zone_tabel[i]) && (cur_time < Globa_2->Charger_param2.time_zone_tabel[(i+1)%cur_time_zone_num]))//开始点为闭区间,可以取等[1段开始,1段结束)，[2段开始,2段结束)..."1段结束"值=="2段开始"值
				break;
		}
		if(i<cur_time_zone_num){
			time_zone_index = i + 1;//since 1 
		}else{//最后1个时段
			time_zone_index = cur_time_zone_num;
		}
		cur_price_index = Globa_2->Charger_param2.time_zone_feilv[time_zone_index -1];//取得第1个时间段的费率号
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


int Insert_Charger_Busy_DB_2(ISO8583_AP_charg_busy_Frame *frame, unsigned int *id, unsigned int up_num)
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
	rc = sqlite3_open_v2(F_CHARGER_2_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE |
									     SQLITE_OPEN_CREATE,
									     NULL);
	if(rc){
		//printf("Can't create F_CHARGER_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		if(DEBUGLOG) RUN_GUN2_LogOut("%s","打开数据库失败:");  
		return -1;
	}

	sprintf(sql,"CREATE TABLE if not exists Charger_Busy_Complete_Data(ID INTEGER PRIMARY KEY autoincrement, card_sn TEXT, flag TEXT, rmb TEXT, kwh TEXT, server_rmb TEXT,total_rmb TEXT, type TEXT, Serv_Type TEXT, Busy_SN TEXT, last_rmb TEXT, ter_sn TEXT, start_kwh TEXT, end_kwh TEXT, rmb_kwh TEXT, sharp_rmb_kwh TEXT, sharp_allrmb TEXT, peak_rmb_kwh TEXT, peak_allrmb TEXT, flat_rmb_kwh TEXT, flat_allrmb TEXT, valley_rmb_kwh TEXT, valley_allrmb TEXT,sharp_rmb_kwh_Serv TEXT, sharp_allrmb_Serv TEXT, peak_rmb_kwh_Serv TEXT, peak_allrmb_Serv TEXT, flat_rmb_kwh_Serv TEXT, flat_allrmb_Serv TEXT, valley_rmb_kwh_Serv TEXT, valley_allrmb_Serv TEXT,end_time TEXT, car_sn TEXT, start_time TEXT, ConnectorId TEXT, End_code TEXT, up_num INTEGER);");
			
	if(sqlite3_exec(db, sql, 0, 0, &zErrMsg) != SQLITE_OK){
		//printf("CREATE TABLE Charger_Busy_Complete_Data error: %s\n", sqlite3_errmsg(db));
		sqlite3_free(zErrMsg);
	}

	if(TRANSACTION_FLAG) sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);
	
	sprintf(sql, "INSERT INTO Charger_Busy_Complete_Data VALUES (NULL, '%.16s', '%.2s', '%.8s', '%.8s', '%.8s', '%.8s', '%.2s', '%.2s', '%.20s', '%.8s', '%.12s', '%.8s', '%.8s', '%.8s', '%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.14s', '%.17s', '%.14s', '%.2s', '%.2s', '%d');",frame->card_sn, frame->flag, frame->rmb, frame->kwh, frame->server_rmb, frame->total_rmb, frame->type, frame->Serv_Type, frame->Busy_SN, frame->last_rmb, frame->ter_sn, frame->start_kwh,frame->end_kwh, frame->rmb_kwh, frame->sharp_rmb_kwh, frame->sharp_allrmb, frame->peak_rmb_kwh, frame->peak_allrmb, frame->flat_rmb_kwh, frame->flat_allrmb,frame->valley_rmb_kwh, frame->valley_allrmb,frame->sharp_rmb_kwh_Serv, frame->sharp_allrmb_Serv,frame->peak_rmb_kwh_Serv, frame->peak_allrmb_Serv, frame->flat_rmb_kwh_Serv, frame->flat_allrmb_Serv,frame->valley_rmb_kwh_Serv, frame->valley_allrmb_Serv,frame->end_time, frame->car_sn,frame->start_time, frame->ConnectorId, frame->End_code, up_num);
  
 	if(DEBUGLOG) RUN_GUN2_LogOut("2号枪新建数据：%s",sql);  
	if(sqlite3_exec(db , sql , 0 , 0 , &zErrMsg) != SQLITE_OK){
		printf("Insert DI error: %s\n", sqlite3_errmsg(db));
		sqlite3_free(zErrMsg);
		if(TRANSACTION_FLAG) sqlite3_exec(db, "rollback transaction", NULL, NULL, NULL);
	  //if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
		if(DEBUGLOG) RUN_GUN2_LogOut("%s %s","插入命令执行失败枪号:",frame->ConnectorId);  
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

int Update_Charger_Busy_DB_2(unsigned int id, ISO8583_AP_charg_busy_Frame *frame, unsigned int flag, unsigned int up_num)
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

	rc = sqlite3_open_v2(F_CHARGER_2_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE |
									     SQLITE_OPEN_CREATE,
									     NULL);
	if(rc){
		printf("Can't create F_CHARGER_2_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
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
		  sprintf(car_sn,"%s","00000000000000000");
		}else{
      memcpy(&car_sn, &frame->car_sn, sizeof(car_sn)-1);
		}
		
		//sprintf(car_sn,"%s","00000000000000000");

		sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET rmb = '%.8s', kwh = '%.8s', rmb_kwh = '%.8s', server_rmb = '%.8s', total_rmb = '%.8s', sharp_allrmb= '%.8s', peak_allrmb= '%.8s', flat_allrmb= '%.8s', valley_allrmb= '%.8s', sharp_allrmb_Serv= '%.8s', peak_allrmb_Serv= '%.8s', flat_allrmb_Serv= '%.8s', valley_allrmb_Serv= '%.8s',start_kwh = '%.8s',end_kwh = '%.8s', end_time = '%.14s',flag = '%.2s',End_code = '%.2s',car_sn = '%.17s',up_num = '%d' WHERE ID = '%d' ;",frame->rmb, frame->kwh, frame->rmb_kwh, frame->server_rmb, frame->total_rmb, frame->sharp_allrmb, frame->peak_allrmb, frame->flat_allrmb,frame->valley_allrmb, frame->sharp_allrmb_Serv, frame->peak_allrmb_Serv, frame->flat_allrmb_Serv,frame->valley_allrmb_Serv, frame->start_kwh,frame->end_kwh, frame->end_time,frame->flag,frame->End_code,car_sn, up_num, id);
	  
		if(up_num == 0){//最后一次更新数据价格
			if(DEBUGLOG) RUN_GUN2_LogOut("2号枪更新记录：%s",sql);  
		}
		
		if(sqlite3_exec(db, sql, 0, 0, &zErrMsg) != SQLITE_OK){
  		printf("UPDATE Charger_Busy_Complete_Data error: %s\n", sqlite3_errmsg(db));
  		sqlite3_free(zErrMsg);
		if(TRANSACTION_FLAG)	sqlite3_exec(db, "rollback transaction", NULL, NULL, NULL);
	   //if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
			sqlite3_close(db);
			if(DEBUGLOG) RUN_GUN2_LogOut("%s %s","充电中更新数据失败:",sql);  
			return -1;
  	}
	}
	if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
	sqlite3_close(db);
	return 0;
}

//查询有没有正常结束的
int Select_NO_Over_Record_2(int id,int up_num)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;


	char sql[512];

	rc = sqlite3_open_v2(F_CHARGER_2_BUSY_COMPLETE_DB, &db,
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
int Delete_Record_busy_DB_2(unsigned int id)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;

	char sql[512];


	rc = sqlite3_open_v2(F_CHARGER_2_BUSY_COMPLETE_DB, &db,
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
static int Select_Busy_NO_insert_2(ISO8583_AP_charg_busy_Frame *frame)//查询需要上传的业务记录的最大 ID 值
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;

	int id = 0;

	char sql[512];


	rc = sqlite3_open_v2(F_CHARGER_2_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		//printf("Can't create F_CHARGER_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

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

	rc = sqlite3_open_v2(F_CHARGER_2_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		printf("Can't create F_CHARGER_2_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	sprintf(sql, "SELECT MAX(ID) FROM Charger_Busy_Complete_Data WHERE up_num = 85 OR up_num = 102 ;");
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

					*up_num = atoi(azResult[ncolumn*1+36]);
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

	rc = sqlite3_open_v2(F_CHARGER_2_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		printf("Can't create F_CHARGER_2_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);

	//sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET rmb = '%.8s', kwh = '%.8s', total_rmb = '%.8s', end_kwh = '%.8s',up_num = '%d' WHERE ID = '%d' ;",frame->rmb, frame->kwh, frame->total_rmb, frame->end_kwh, 0x00, id);
	sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET rmb = '%.8s', kwh = '%.8s', rmb_kwh = '%.8s', server_rmb = '%.8s', total_rmb = '%.8s', sharp_allrmb= '%.8s', peak_allrmb= '%.8s', flat_allrmb= '%.8s', valley_allrmb= '%.8s', sharp_allrmb_Serv= '%.8s', peak_allrmb_Serv= '%.8s', flat_allrmb_Serv= '%.8s', valley_allrmb_Serv= '%.8s',start_kwh = '%.8s',end_kwh = '%.8s', end_time = '%.14s',End_code = '%.2s',up_num = '%d' WHERE ID = '%d' ;",frame->rmb, frame->kwh, frame->rmb_kwh, frame->server_rmb, frame->total_rmb, frame->sharp_allrmb, frame->peak_allrmb, frame->flat_allrmb,frame->valley_allrmb, frame->sharp_allrmb_Serv, frame->peak_allrmb_Serv, frame->flat_allrmb_Serv,frame->valley_allrmb_Serv, frame->start_kwh,frame->end_kwh, frame->end_time,frame->End_code, up_num, id);

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

static int Set_0X66_Busy_ID(int id)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;

	char sql[512];

	rc = sqlite3_open_v2(F_CHARGER_2_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		printf("Can't create F_CHARGER_2_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
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

static int Set_0X550X66_TO0X00_Busy_ID_Gun_2(int gun)
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
	rc = sqlite3_open_v2(F_CHARGER_2_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		printf("Can't create F_CHARGER_2_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
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
		return -1;
	}
 if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
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
	rc = sqlite3_open_v2(F_CHARGER_2_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		printf("Can't create F_CHARGER_2)_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
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

int ISorder_chg_control_2()
{
	time_t systime=0;
	struct tm tm_t;
	systime = time(NULL);        //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
	unsigned int cur_time,End_time_on = 0;//当前时间=分来计数的
	cur_time = tm_t.tm_hour*60 +	tm_t.tm_min	;//运行的当前时间	
	End_time_on = Globa_2->order_card_end_time[0]*60 + Globa_2->order_card_end_time[1];//	
	if(cur_time >= End_time_on){//先部用考虑跨天的情况
		return 1;
	}else{
		return 0;
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
    cur_time_zone_num = Globa_2->Charger_param2.time_zone_num;
		for(i=0;i<cur_time_zone_num;i++){
			if((cur_time >= Globa_2->Charger_param2.time_zone_tabel[i]) && (cur_time < Globa_2->Charger_param2.time_zone_tabel[(i+1)%cur_time_zone_num]))//开始点为闭区间,可以取等[1段开始,1段结束)，[2段开始,2段结束)..."1段结束"值=="2段开始"值
				break;
		}
		if(i<cur_time_zone_num){
			time_zone_index = i + 1;//since 1 
		}else{//最后1个时段
			time_zone_index = cur_time_zone_num;
		}
		cur_price_index = Globa_2->Charger_param2.time_zone_feilv[time_zone_index -1];//取得第1个时间段的费率号
	  return cur_price_index;
}

int Main_Task_2()
{
	int fd, Ret=0, Ret1=0,Busy_NO_insert_id = -1;
	int all_id = 0;
	int Insertflag = 0;
	UINT32	IC_check_count1 = 0;
	UINT32 count_wait=0,count_wait1=0,card_falg_count = 0;
	UINT32 charger_type = 0;
	UINT32 temp1 = 0,i =0;
	UINT32 consume_flag = 0,Select_NO_Over_count = 0,Select_Busy_count = 0;

	UINT32 cur_price_index = 0;  //当前处于收费电价阶段
  UINT32 pre_price_index = 0;  //之前处于的那个时刻
  UINT32 ul_start_kwh =0; //每个峰的起始时刻
  UINT32 g_ulcharged_kwh[4] = {0};
  UINT32 tmp_value = 0;
	UINT32 g_ulcharged_rmb[4] = {0}; //添加各个时区的电价总额。----By dengjh
	UINT32 g_ulcharged_rmb_Serv[4] = {0}; //添加各个时区的电价总额。----By dengjh
  
	UINT32 share_time_kwh_price[4] = {0};
	UINT32 share_time_kwh_price_serve[4] = {0}; //服务
  UINT32 Serv_Type = 0; //服务类型 
	UINT8 Sing_pushbtton_count1 = 0;
	UINT8 Sing_pushbtton_count2 = 0;
	UINT32 kwh_charged_rmbkwh = 0,charged_rmb = 0, kwh_charged_rmb = 0, kwh_charged_rmbkwh_serv = 0, charged_rmb_serv = 0, kwh_charged_rmb_serv = 0, delta_kwh = 0;
	UINT8 check_current_low_st = 0;//判断SOC达到设定限值后，电流低于设定值后停机
	time_t Current_Systime=0,current_low_timestamp = 0,meter_Curr_Threshold_count= 0,current_overkwh_timestamp = 0,Electrical_abnormality = 0;

	CHARGER_MSG msg;

	ISO8583_AP_charg_busy_Frame busy_frame;
	ISO8583_AP_charg_busy_Frame pre_old_busy_frame;//上次充电完成数据
	UINT8 card_sn[16];     //用户卡号
	UINT32 last_rmb = 0;   //充电前卡余额（以分为单位）
	UINT32 temp_rmb = 0;   //
	UINT32 temp_kwh = 0;
	int id = -1,pre_old_id = 0;
	UINT32 up_num = 0,Bind_BmsVin_falg = 0;

	UINT8 temp_char[256];  //

	time_t systime=0;
	

	struct tm tm_t;
	prctl(PR_SET_NAME,(unsigned long)"QT2_Task");//设置线程名字 

	Globa_2->Manu_start = 0;
	Globa_2->QT_Step = 0x03;

	Globa_2->charger_state = 0x03;  //终端状态 0x01-故障  0x02-告警  03-空闲 0x04-充电中 0x05-完成 0x06-预约 
	Globa_2->gun_state = 0x03;     //枪2状态 取值只有  03-空闲 0x04-充电中 0x05-完成 0x07-等待
	Globa_2->APP_Start_Account_type = 0;
  msg.type = 0x150;//通道选着界面;
	msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);

	sleep(6);//等待电表通信正常

	//1: 查询以外掉电的不完整数据记录，然后根据条件更新数据记录

	pthread_mutex_lock(&busy_db_pmutex);
	id = Select_0X55_0X66_Busy_ID(&busy_frame, &up_num);//查询没有流水号的最大记录 ID 值
	if(id != -1){
		printf("xxxxxxxxxxxxxxxxxxxxxx   Select_0X55_0X66_Busy_ID ok id=%d up_num =%d Globa_1->Error.meter_connect_lost =%d \n", id,up_num,Globa_1->Error.meter_connect_lost);
		if((up_num == 0x55)||(up_num == 0x66)){
			if(Globa_2->Error.meter_connect_lost == 0){//表计通信正常
				sprintf(&temp_char[0], "%.8s", &busy_frame.end_kwh[0]);	
				temp_kwh = atoi(&temp_char[0]);

				if((Globa_2->meter_KWH >= temp_kwh)&&(Globa_2->meter_KWH < (temp_kwh+100))){//当前电量大于等于记录结束电量，且不大于1度电
					unsigned short last_end_hhmm = (busy_frame.end_time[8]-'0')*10+(busy_frame.end_time[9]-'0');//hour
			    last_end_hhmm <<= 8;
			    last_end_hhmm |= ((busy_frame.end_time[10]-'0')*10+(busy_frame.end_time[11]-'0'));//mm
			    cur_price_index = GetTimePriceIndex(last_end_hhmm);
					delta_kwh = Globa_2->meter_KWH - temp_kwh;		
					sprintf(&temp_char[0], "%.8s", &busy_frame.start_kwh[0]);	
		
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
					temp_kwh = Globa_2->meter_KWH - atoi(&temp_char[0]);
					if(temp_kwh != 0){
					  sprintf(&temp_char[0], "%.8s", &busy_frame.rmb[0]);	
					  sprintf(&temp_char[0], "%08d", (atoi(&temp_char[0]))/temp_kwh);	
						memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));
					}
					sprintf(&temp_char[0], "%08d", temp_kwh);				
					memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
					sprintf(&temp_char[0], "%08d", Globa_2->meter_KWH);				
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
	
	sleep(1);//等待电表通信正常
	pthread_mutex_lock(&busy_db_pmutex);
	Set_0X550X66_Busy_ID();
	pthread_mutex_unlock(&busy_db_pmutex);

	id = 0;
	while(1){
		usleep(200000);//10ms
		Main_2_Watchdog = 1;
		Current_Systime = time(NULL);
    if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4)){//双枪同时充电和双枪轮流充电才需要这个
			if(Globa_2->BMS_Power_V == 1){
				GROUP2_BMS_Power24V_ON;
			}else{
				GROUP2_BMS_Power24V_OFF;
			}
			if(QT_count_500ms_enable == 1){//使能500MS 计时
				if(QT_count_500ms_flag != 1){//计时时间未到
					QT_count_500ms++;
					if(QT_count_500ms >= 2){
						QT_count_500ms_flag = 1;
					}
				}
			}

			if(QT_count_5000ms_enable == 1){//使能5S 计时
				if(QT_count_5000ms_flag != 1){//计时时间未到
					QT_count_5000ms++;
					if(QT_count_5000ms >= 25){
						QT_count_5000ms_flag = 1;
					}
				}
			}

			msg.type = 0x00;//每次读之前初始化
			Ret = msgrcv(Globa_2->qt_to_arm_msg_id, &msg, sizeof(CHARGER_MSG)-sizeof(long), 0, IPC_NOWAIT);//接收QT发送过来的消息队列
			if(Ret >= 0){
				//printf("xxxxxx ret=%d,%0x,%0x\n", Ret, msg.type, msg.argv[0]);
			}

			//判断枪状态 取值只有  03-空闲 0x04-充电中 0x05-完成 0x07-等待
			if(Globa_2->gun_link != 1){  //枪连接断开
				Globa_2->gun_state = 0x03; //充电枪连接只要断开就为 03-空闲
			}else{
				if(Globa_2->gun_state == 0x03){ //枪状态 原来为 03-空闲 那现在改变为  0x07-等待
					Globa_2->gun_state = 0x07;
				}else if((Globa_2->gun_state == 0x07)&&(Globa_2->charger_state == 0x04)){ //枪状态 原来为 0x07-等待 那现在改变为  0x04-充电中
					Globa_2->gun_state = 0x04;
				}else if((Globa_2->gun_state == 0x04)&&(Globa_2->charger_state != 0x04)){ //枪状态 原来为 0x04-充电中 那现在改变为  0x05-完成
					Globa_2->gun_state = 0x05;
				}else if((Globa_2->gun_state == 0x05)&&(Globa_2->charger_state == 0x04)){ //枪状态 原来为 0x05-完成 那现在改变为  0x04-充电中 可能不拔枪又开始充电
					Globa_2->gun_state = 0x04;
				}
			}

			switch(Globa_2->QT_Step){//判断 UI 所在界面
				case 0x03:{//--------------- 自动充电分开的AB通道 ----------
					if(Globa_2->charger_state == 0x05){//工作状态	BIN码	1Byte	0x01-故障  0x02-告警  03-空闲 0x04-充电中 0x05-完成 0x06-预约
						if(Globa_2->gun_link != 1){
							Globa_2->charger_state = 0x03;
							
							Globa_2->Time = 0;
							Globa_2->kwh = 0;
						}
					}else{
						if(Globa_1->Double_Gun_To_Car_falg != 1){
							Globa_2->charger_state = 0x03;
							Globa_2->Time = 0;
							Globa_2->kwh = 0;
						}
					}
					Globa_2->private_price_flag  = 0;
					Globa_2->Enterprise_Card_flag = 0;//企业卡号成功充电上传标志
					Globa_2->qiyehao_flag = 0;//企业号标记，域43
					Globa_2->order_chg_flag = 0;
					card_falg_count = 0;
					
				  if(!((Globa_1->QT_gun_select == 2)&&(Globa_1->QT_Step == 0x313)))
					{
						Globa_2->Manu_start = 0;
						Globa_2->charger_start = 0;
					}

					Globa_2->private_price_acquireOK = 0;
	
					Globa_2->order_card_end_time[0] = 99;
			    Globa_2->order_card_end_time[1] = 99;
					QT_timer_stop(500);
          Globa_2->Start_Mode = 0;   
					charger_type = 0;
					Globa_2->card_type = 0;//卡类型 0:未知 1：用户卡 2：员工卡 3：维护卡
					Globa_2->checkout = 0;
					Globa_2->pay_step = 0;

					Globa_2->QT_charge_money = 0;
					Globa_2->QT_charge_kwh = 0;
					Globa_2->QT_charge_time = 0;  
					Globa_2->QT_pre_time = 0;

					Globa_2->User_Card_check_flag = 0;
					Globa_2->User_Card_check_result = 0;
			    Bind_BmsVin_falg = 0;
          Globa_2->Special_card_offline_flag = 0;
				  Globa_2->Charger_Over_Rmb_flag = 0;

					if((Globa_1->Double_Gun_To_Car_falg != 1)||(Globa_1->Charger_param.System_Type != 4)){
						Globa_2->BATT_type = 0;
						Globa_2->BMS_batt_SOC = 0;
						Globa_2->BMS_need_time = 0;
						Globa_2->BMS_batt_SOC = 0;
						Globa_2->BMS_cell_HV = 0;
						Globa_2->ISO_8583_rmb_Serv = 0;//总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
						Globa_2->ISO_8583_rmb = 0;                     //电量消费金额 0.01yuan
						Globa_2->BMS_Info_falg = 0;
						
						if(pre_old_id != 0){
							Select_NO_Over_count ++;
							if(Select_NO_Over_count >= 10){
								if(Select_NO_Over_Record_2(pre_old_id,0)!= -1){//表示有没有上传的记录
									pthread_mutex_lock(&busy_db_pmutex);
									Update_Charger_Busy_DB_2(pre_old_id, &pre_old_busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
									pthread_mutex_unlock(&busy_db_pmutex);
									Select_NO_Over_count = 0;
								}else{
									if(DEBUGLOG) RUN_GUN2_LogOut("%s %d","2号枪交易数据,进行再次更新成功 ID=",pre_old_id);  
								}
								pthread_mutex_lock(&busy_db_pmutex);
								if(Select_Busy_BCompare_ID(&pre_old_busy_frame) == -1 ){//表示有记录没有插入到新的数据库中
									all_id = 0;
									Insertflag = Insert_Charger_Busy_DB(&pre_old_busy_frame, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
									if((Insertflag != -1)&&(all_id != 0)){//表示插入数据成功，其他的则需要重新插入
										if(Delete_Record_busy_DB_2(pre_old_id) != -1){
											if(DEBUGLOG) RUN_GUN2_LogOut("2号枪交易数据,已经再次插入到正式数据库中并删除临时数据库记录 ID = %d",pre_old_id);  
											id = 0;
											pre_old_id = 0;
											Select_NO_Over_count = 0;
											memset(&pre_old_busy_frame, '0', sizeof(pre_old_busy_frame));
										}
									}else{
										if(DEBUGLOG) RUN_GUN2_LogOut("2号枪存在交易数据,再次插入到正式数据库中失败 数据类型 pre_old_id = %d",pre_old_id);  
									}
								}else{//表示已经存在了，则直接删除该文件
									Delete_Record_busy_DB_2(pre_old_id);
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
								 Busy_NO_insert_id = Select_Busy_NO_insert_2(&pre_old_busy_frame);
								if(Busy_NO_insert_id != -1 ){//防止中途掉电的时候出现异常的现象,表示有记录没有插入到新的数据库中
								//进行数据库对比，确定是否已经插入到了正式的数据跨中。
									pthread_mutex_lock(&busy_db_pmutex);
									if(Select_Busy_BCompare_ID(&pre_old_busy_frame) == -1){
										all_id = 0;
										Insertflag = Insert_Charger_Busy_DB(&pre_old_busy_frame, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
										if((Insertflag != -1)&&(all_id != 0)){//表示插入数据成功，其他的则需要重新插入
											if(Delete_Record_busy_DB_2(Busy_NO_insert_id) != -1){
												Busy_NO_insert_id = 0;
												pre_old_id = 0;
												Select_NO_Over_count = 0;
												memset(&pre_old_busy_frame, '0', sizeof(pre_old_busy_frame));
											}
											if(DEBUGLOG) RUN_GUN2_LogOut("查询到2号枪存在交易数据,再次插入到正式数据库");  
										}else{
											if(DEBUGLOG) RUN_GUN2_LogOut("查询到2号枪存在交易数据,再次插入到正式数据库中失败 数据类型  pre_old_id = %d",pre_old_id);  
										}
									}else{//表示已经存在了，则直接删除该文件
										Delete_Record_busy_DB_2(Busy_NO_insert_id);
										Busy_NO_insert_id = 0;
										pre_old_id = 0;
										Select_NO_Over_count = 0;
										memset(&pre_old_busy_frame, '0', sizeof(pre_old_busy_frame));
									}
									pthread_mutex_unlock(&busy_db_pmutex);
								}
							}
						}
					}
					if(Globa_2->Electric_pile_workstart_Enable == 1){ //禁止本桩工作
						continue ;
					}
					
					if((Globa_1->QT_Step == 0x03)||(Globa_1->QT_Step == 0x201)||(Globa_1->QT_Step == 0x21)||(Globa_1->QT_Step == 0x23)||(Globa_1->QT_Step == 0x02)||(Globa_1->QT_Step == 0x22)){//枪1界面在半屏状态，或者在主界面状态
						if(Globa_2->App.APP_start == 1){//手机启动充电
							count_wait = 0;
			
							Select_NO_Over_count = 0;
              if(pre_old_id != 0){
								pthread_mutex_lock(&busy_db_pmutex);
								Set_0X550X66_TO0X00_Busy_ID_Gun_2(2); //强制性更新上传标志
								pthread_mutex_unlock(&busy_db_pmutex);
								pre_old_id = 0;
								Select_NO_Over_count = 0;
								memset(&pre_old_busy_frame, '0', sizeof(pre_old_busy_frame));
							}
							
							memset(&busy_frame, '0', sizeof(busy_frame));
							memset(&Page_BMS_data,0,sizeof(PAGE_BMS_FRAME));
							memset(Globa_2->VIN, '0', sizeof(Globa_2->VIN));

							//-------------------     新业务进度数据------------------------------
							temp_rmb = 0;
							Globa_2->kwh = 0;
							Globa_2->rmb = 0;
							Globa_2->total_rmb = 0;
							ul_start_kwh = 0;
				      pre_old_id = 0;
							for(i=0;i<4;i++){
								g_ulcharged_rmb[i] = 0;
								g_ulcharged_kwh[i] = 0;
								g_ulcharged_rmb_Serv[i] = 0;
								
							}
							if(Globa_2->Special_price_APP_data_falg == 1){
								for(i=0;i<4;i++){
									share_time_kwh_price[i] = Globa_2->Special_price_APP_data.share_time_kwh_price[i];
									share_time_kwh_price_serve[i] = Globa_2->Special_price_APP_data.share_time_kwh_price_serve[i]; //服务
								}
								Serv_Type = Globa_2->Special_price_APP_data.Serv_Type;
							}else{
								for(i=0;i<4;i++){
									share_time_kwh_price[i] = Globa_2->Charger_param2.share_time_kwh_price[i];
									share_time_kwh_price_serve[i] = Globa_2->Charger_param2.share_time_kwh_price_serve[i]; //服务
								}
								Serv_Type = Globa_2->Charger_param.Serv_Type;
							}
							cur_price_index = GetCurPriceIndex(); //获取当前时间
							pre_price_index = cur_price_index;
				
							if(Globa_2->BMS_Power_V == 1){
								GROUP2_BMS_Power24V_ON;
							}else{
								GROUP2_BMS_Power24V_OFF;
							}
							temp_kwh = 0;
							Globa_2->soft_KWH = 0;
							Globa_2->Time = 0;
							Globa_2->BMS_need_time = 0;
							Globa_2->BMS_batt_SOC = 0;
							Globa_2->BMS_cell_HV = 0;
							Globa_2->Start_Mode = APP_START_TYPE;//app

							memcpy(&busy_frame.card_sn[0], &Globa_2->App.ID[0], sizeof(busy_frame.card_sn));
							memcpy(&busy_frame.Busy_SN[0], &Globa_2->App.busy_SN[0], sizeof(busy_frame.Busy_SN));

							systime = time(NULL);         //获得系统的当前时间 
							localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
							sprintf(&temp_char[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
																						 tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
								
							memcpy(&busy_frame.start_time[0], &temp_char[0], sizeof(busy_frame.start_time));         
							memcpy(&busy_frame.end_time[0], &temp_char[0], sizeof(busy_frame.end_time));
							
							memcpy(&busy_frame.flag[0], "00", sizeof(busy_frame.flag));//员工卡，交易标志直接标记完成

							sprintf(&temp_char[0], "%08d", Globa_2->App.last_rmb);				
							memcpy(&busy_frame.last_rmb[0], &temp_char[0], sizeof(busy_frame.last_rmb));

							memcpy(&busy_frame.ter_sn[0],  &Globa_2->Charger_param.SN[0], sizeof(busy_frame.ter_sn));


							Globa_2->meter_start_KWH = Globa_2->meter_KWH;//取初始电表示数
							Globa_2->meter_stop_KWH = Globa_2->meter_start_KWH;
							
							sprintf(&temp_char[0], "%08d", Globa_2->meter_start_KWH);    
							memcpy(&busy_frame.start_kwh[0], &temp_char[0], sizeof(busy_frame.start_kwh));
							
							sprintf(&temp_char[0], "%08d", Globa_2->meter_stop_KWH);				
							memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

							sprintf(&temp_char[0], "%08d", Globa_2->Charger_param.Price);    
							memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));

							sprintf(&temp_char[0], "%08d", Globa_2->kwh);				
							memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
							
							sprintf(&temp_char[0], "%08d", Globa_2->rmb);				
							memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));
							//起始总费用等于电量费用，暂时只做安电量收服务费
							sprintf(&temp_char[0], "%08d", Globa_2->total_rmb);
							memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));
							sprintf(&temp_char[0], "%08d", Globa_2->total_rmb - Globa_2->rmb);
							memcpy(&busy_frame.server_rmb[0], &temp_char[0], sizeof(busy_frame.server_rmb));
							busy_frame.type[0] = '0';//充电方式（01：刷卡，02：手机）
							busy_frame.type[1] = '2';
							sprintf(&temp_char[0], "%02d", Globa_2->Charger_param.Serv_Type);//服务费收费类型 01：按次收费；02：按时间收费(单位：分/10分钟；03：按电量	9	N	N2
							memcpy(&busy_frame.Serv_Type[0], &temp_char[0], sizeof(busy_frame.Serv_Type));
							
							sprintf(&temp_char[0], "%.17s",Globa_2->VIN);//服务费收费类型 01：按次收费；02：按时间收费(单位：分/10分钟；03：按电量	9	N	N2
						//	memcpy(&busy_frame.car_sn[0], &Globa_2->VIN[0], sizeof(busy_frame.car_sn));//
							memcpy(&busy_frame.car_sn[0], &temp_char[0], sizeof(busy_frame.car_sn));

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
							busy_frame.ConnectorId[1] = '2';
							busy_frame.End_code[0] = '1';//结束代码
							busy_frame.End_code[1] = '1';
							
							pthread_mutex_lock(&busy_db_pmutex);
							id = 0;
						  Insertflag =  Insert_Charger_Busy_DB_2(&busy_frame, &id, 0x55);//写新数据库记录 有流水号 传输标志0x55不能上传
							pthread_mutex_unlock(&busy_db_pmutex);
							if(DEBUGLOG) RUN_GUN2_LogOut("%s %d","二号枪:APP启动充电并插入交易记录中的ID=",id);  
	         
							if(Globa_1->QT_Step == 0x02){
								msg.type = 0x150;//充电枪启动选着界面
								msgsnd(Globa_1->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
								Globa_1->QT_Step = 0x03; // 系统自动充电分开的AB通道。
							}
							usleep(10000);
						  if(Insertflag == -1){
							  AP_APP_Sent_2_falg[1] = 6;
						  	AP_APP_Sent_2_falg[0] = 1;

								Page_1300.cause = 63;
								Page_1300.Capact = Globa_2->BMS_batt_SOC;
								Page_1300.elapsed_time[0] =  Globa_2->Time;
								Page_1300.elapsed_time[1] =  Globa_2->Time>>8;
								Page_1300.KWH[0] =  Globa_2->kwh;
								Page_1300.KWH[1] =  Globa_2->kwh>>8;
								Page_1300.RMB[0] =  Globa_2->total_rmb;
								Page_1300.RMB[1] =  Globa_2->total_rmb>>8;
								Page_1300.RMB[2] =  Globa_2->total_rmb>>16;
								Page_1300.RMB[3] =  Globa_2->total_rmb>>24;
								
								memcpy(&Page_1300.SN[0], &busy_frame.card_sn[0], sizeof(Page_1300.SN));
								Page_1300.APP = 2;   //充电启动方式 1:刷卡  2：手机APP
		            Globa_2->Manu_start = 0;
							  Globa_2->charger_start = 0;
								Globa_2->App.APP_start = 0;
								memcpy(&msg.argv[0], &Page_1300.reserve1, sizeof(PAGE_1300_FRAME));
								msg.type = 0x1300;//充电结束结算界面
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_1300_FRAME), IPC_NOWAIT);
								sleep(1);//不用延时太久，APP控制，需要立即上传业务数据

								msg.type = 0x150;
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);				
								Globa_2->QT_Step = 0x03; //自动充电分开的AB通道
								Globa_2->BMS_Power_V = 0;
								Globa_2->Operation_command = 2;//进行解锁
								memset(&Globa_2->App.ID, '0', sizeof(Globa_2->App.ID));
								break;
						  }
							AP_APP_Sent_2_falg[0] = 1;
							Globa_2->Manu_start = 1;
							Globa_2->charger_start = 1;
							Globa_2->QT_gun_select = 2;	
							
							msg.type = 0x1111;
						  memset(&Page_1111.reserve1, 0x00, sizeof(PAGE_1111_FRAME));									
							Page_1111.APP  = 2;   //充电启动方式 1:刷卡  2：手机APP
							memcpy(&Page_1111.SN[0], &busy_frame.card_sn[0], sizeof(Page_1111.SN));
							memcpy(&msg.argv[0], &Page_1111.reserve1, sizeof(PAGE_1111_FRAME));
							msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_1111_FRAME), IPC_NOWAIT);
							
							QT_timer_start(500);
							Globa_2->QT_Step = 0x22;   // 2 自动充电-》 APP 充电界面 状态 2.2-
              meter_Curr_Threshold_count= 0;
							if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
								Voicechip_operation_function(0x05);  //05H     “系统自检中，请稍候”
							}
							Globa_2->charger_state = 0x04;
							count_wait = 0;
							count_wait1 = 0; 
			        meter_Curr_Threshold_count = Current_Systime;
						  current_low_timestamp = Current_Systime;
						  check_current_low_st = 0;
              Electrical_abnormality = Current_Systime;	
							current_overkwh_timestamp = Current_Systime;
							break;
						}
					}
					
				  if(Globa_1->Charger_param.System_Type == 1 ){//双枪轮流充电才需要这个
						if((Globa_1->QT_Step == 0x21)||(Globa_1->QT_Step == 0x22)){//表示1号枪在充电
							Sing_pushbtton_count2++;
							if(Sing_pushbtton_count2 <= 2){
								msg.type = 0x88;
								msg.argv[0] = 1; //可以预约
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							}else{
								Sing_pushbtton_count2 = 3;
								Sing_pushbtton_count1 = 0;
							}
						}else{
							Sing_pushbtton_count1++;
							if(Sing_pushbtton_count1 <= 2){
								msg.type = 0x88;
								msg.argv[0] = 0;
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							}else{
								Sing_pushbtton_count1 = 3;
								Sing_pushbtton_count2 = 0;
							}
						}
					}
					
					if(msg.type == 0x150){//收到该界面消息
						switch(msg.argv[0]){//判断 在当前界面下 用户操作的按钮
							case 0x01:{//2#通道选着按钮
								if(Globa_2->Error_system == 0){//没有系统故障
									if((Globa_1->QT_Step == 0x03)||(Globa_1->QT_Step == 0x201)||(Globa_1->QT_Step == 0x21)||(Globa_1->QT_Step == 0x23)||(Globa_1->QT_Step == 0x22)){//枪1界面在半屏状态
										Page_1000.link = (Globa_2->gun_link == 1)?1:0;  //1:连接  0：断开
										Page_1000.VIN_Support = (((Globa_1->have_hart == 1)&&(Globa_2->VIN_Support_falg == 1)) == 1)? 1:0;
                    Page_1000.other_link = 0;
										msg.type = 0x1000;
										memcpy(&msg.argv[0], &Page_1000.reserve1, sizeof(PAGE_1000_FRAME));
										msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_1000_FRAME), IPC_NOWAIT);
			
										QT_timer_start(500);//需要刷新充电枪的状态
										Globa_2->QT_Step = 0x11; // 1 自动充电-》充电枪连接确认 状态  1.1
										Globa_1->page_flag = 1;
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
				        memset(&Globa_2->Bind_BmsVin[0], '0', sizeof(Globa_2->Bind_BmsVin));
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
								Globa_2->QT_gun_select = 2;				
								Globa_2->BMS_Power_V = msg.argv[1];
								if(Globa_2->BMS_Power_V == 1){
									GROUP2_BMS_Power24V_ON;
								}else{
									GROUP2_BMS_Power24V_OFF;
								}
								msg.type = 0x1100;// 充电方式选择界面
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 2, IPC_NOWAIT);//发送两个字节
								
								Globa_2->QT_Step = 0x121; // 1 自动充电-》充电方式选着 状态  1.2.1 	

								if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
									Voicechip_operation_function(0x02);  //02H"    “请选择充电方式"
								}						
								QT_timer_stop(500);
								
								if(pre_old_id != 0){
									for(i=0;i<5;i++){
										if(Select_NO_Over_Record_2(pre_old_id,0)!= -1){//表示有没有上传的记录
											pthread_mutex_lock(&busy_db_pmutex);
											Update_Charger_Busy_DB_2(pre_old_id, &pre_old_busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
											pthread_mutex_unlock(&busy_db_pmutex);
											Select_NO_Over_count = 0;
										}else{
											pre_old_id = 0;
											Select_NO_Over_count = 0;
											memset(&pre_old_busy_frame, '0', sizeof(pre_old_busy_frame));
											break;
										}
										sleep(1);
									}
								}else{
									Select_NO_Over_count = 0;
								}
							
  							break;
							}
							case 0x02:{//退出按钮
								msg.type = 0x150;
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);						

								QT_timer_stop(500);
								Globa_2->QT_Step = 0x03; //自动充电分开的AB通道

								break;
							}
							case 0x03:{//VIN启动
								Globa_2->QT_gun_select = 2;				
								Globa_2->BMS_Power_V = msg.argv[1];
								if(Globa_2->BMS_Power_V == 1){
									GROUP2_BMS_Power24V_ON;
								}else{
									GROUP2_BMS_Power24V_OFF;
								}
							  Globa_2->kwh = 0;
								ul_start_kwh = 0;
								cur_price_index = GetCurPriceIndex(); //获取当前时间
								pre_price_index = cur_price_index;
								for(i=0;i<4;i++){
									g_ulcharged_rmb[i] = 0;
									g_ulcharged_kwh[i] = 0;
									g_ulcharged_rmb_Serv[i] = 0;
									share_time_kwh_price[i] = 0;
									share_time_kwh_price_serve[i] = 0; //服务
								}
								Globa_2->rmb = 0;
								Globa_2->total_rmb = 0;
								Globa_2->ISO_8583_rmb_Serv = 0;//总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
								Globa_2->ISO_8583_rmb = 0;                     //电量消费金额 0.01yuan
								
								temp_kwh = 0;
								Globa_2->soft_KWH = 0;
								Globa_2->Time = 0;
								Globa_2->card_type = 0;//卡类型 0:未知 1：用户卡 2：员工卡 3：维护卡

								charger_type = 1;
								Globa_2->Card_charger_type = 4;
								msg.type = 0x200;//等待刷卡界面
								memset(&Page_200.reserve1, '0', sizeof(PAGE_200_FRAME));
								Page_200.state = 40;//1 系统启动中,获取VIN吗
								memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
								Globa_2->Special_card_offline_flag = 0;
								Globa_2->QT_Step = 0x19; // 2 自动充电-等待刷卡界面  2.0
								Globa_2->VIN_Judgment_Result = 0;

								count_wait = 0;
								memset(&Globa_2->card_sn[0], '0', sizeof(Globa_2->card_sn));
								memset(&Globa_2->tmp_card_sn[0], '0', sizeof(Globa_2->tmp_card_sn));
								memset(&busy_frame, '0', sizeof(busy_frame));
								memset(Globa_2->VIN, '0', sizeof(Globa_2->VIN));
								Globa_2->User_Card_check_flag = 0;
								Globa_2->User_Card_check_result = 0;
								Select_NO_Over_count = 0;
								Globa_2->Start_Mode = VIN_START_TYPE;//VIN启动
								Globa_2->Manu_start = 0;
								if(pre_old_id != 0){
									pthread_mutex_lock(&busy_db_pmutex);
									Set_0X550X66_TO0X00_Busy_ID_Gun_2(2); //强制性更新上传标志
									pthread_mutex_unlock(&busy_db_pmutex);
									pre_old_id = 0;
									Select_NO_Over_count = 0;
									memset(&pre_old_busy_frame, '0', sizeof(pre_old_busy_frame));
								}
								Globa_2->checkout = 0;
								Globa_2->pay_step = 0;
								
								if((Globa_2->private_price_acquireOK == 1)){//专有电价
									for(i=0;i<4;i++){
										share_time_kwh_price[i] = Globa_2->Special_price_data.share_time_kwh_price[i];
										share_time_kwh_price_serve[i] = Globa_2->Special_price_data.share_time_kwh_price_serve[i]; //服务
									}
									Serv_Type = Globa_2->Special_price_data.Serv_Type;
								}else{//通用电价
									for(i=0;i<4;i++){
										share_time_kwh_price[i] = Globa_2->Charger_param2.share_time_kwh_price[i];
										share_time_kwh_price_serve[i] = Globa_2->Charger_param2.share_time_kwh_price_serve[i]; //服务
									}
									Serv_Type = Globa_2->Charger_param.Serv_Type;
								}
								memcpy(&busy_frame.card_sn[0], &Globa_2->card_sn[0], sizeof(busy_frame.card_sn));

							
								systime = time(NULL);         //获得系统的当前时间 
								localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
								sprintf(&temp_char[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
																							 tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
										
								memcpy(&busy_frame.start_time[0], &temp_char[0], sizeof(busy_frame.start_time));   
								
								memcpy(&busy_frame.end_time[0], &temp_char[0], sizeof(busy_frame.end_time));

								memcpy(&busy_frame.flag[0], "00", sizeof(busy_frame.flag));//员工卡直接标志位完成，不灰锁
							
								sprintf(&temp_char[0], "%08d", Globa_2->last_rmb);		
								memcpy(&busy_frame.last_rmb[0], &temp_char[0], sizeof(busy_frame.last_rmb));

								memcpy(&busy_frame.ter_sn[0],  &Globa_2->Charger_param.SN[0], sizeof(busy_frame.ter_sn));

								Globa_2->meter_start_KWH = Globa_2->meter_KWH;//取初始电表示数
								Globa_2->meter_stop_KWH = Globa_2->meter_start_KWH;
							

								sprintf(&temp_char[0], "%08d", Globa_2->meter_start_KWH);    
								memcpy(&busy_frame.start_kwh[0], &temp_char[0], sizeof(busy_frame.start_kwh));
								
								sprintf(&temp_char[0], "%08d", Globa_2->meter_stop_KWH);				
								memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

								sprintf(&temp_char[0], "%08d", Globa_2->Charger_param.Price);    
								memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));

								sprintf(&temp_char[0], "%08d", Globa_2->kwh);				
								memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
								
								sprintf(&temp_char[0], "%08d", Globa_2->rmb);				
								memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));

								//起始总费用等于电量费用，暂时只做安电量收服务费
								sprintf(&temp_char[0], "%08d", Globa_2->total_rmb);
								memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));
								sprintf(&temp_char[0], "%08d", Globa_2->total_rmb-Globa_2->rmb);
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
								busy_frame.ConnectorId[1] = '2';
								
								busy_frame.End_code[0] = '1';//结束代码
								busy_frame.End_code[1] = '1';
								
								tmp_value =0;
								id = 0;
								pthread_mutex_lock(&busy_db_pmutex);
								Insertflag = Insert_Charger_Busy_DB_2(&busy_frame, &id, 0x55);//写新数据库记录 无流水号 传输标志0x55不能上传
								pthread_mutex_unlock(&busy_db_pmutex);
								if(DEBUGLOG) RUN_GUN2_LogOut("%s %d","2号枪:刷卡启动充电并插入交易记录中的ID=",id);  
								if(Insertflag == -1){
									Page_1300.cause = 63;
									Page_1300.Capact = Globa_2->BMS_batt_SOC;
									Page_1300.elapsed_time[0] =  Globa_2->Time;
									Page_1300.elapsed_time[1] =  Globa_2->Time>>8;
									Page_1300.KWH[0] =  Globa_2->kwh;
									Page_1300.KWH[1] =  Globa_2->kwh>>8;
									Page_1300.RMB[0] =  Globa_2->total_rmb;
									Page_1300.RMB[1] =  Globa_2->total_rmb>>8;
									Page_1300.RMB[2] =  Globa_2->total_rmb>>16;
									Page_1300.RMB[3] =  Globa_2->total_rmb>>24;
							
									memcpy(&Page_1300.SN[0], &Globa_2->card_sn[0], sizeof(Page_1300.SN));
									Page_1300.APP = 3;   //充电启动方式 1:刷卡  2：手机APP

									memcpy(&msg.argv[0], &Page_1300.reserve1, sizeof(PAGE_1300_FRAME));
									msg.type = 0x1300;//充电结束结算界面
									msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_1300_FRAME), IPC_NOWAIT);

									Globa_2->QT_Step = 0x23; // 2      充电结束 结算界面 2.3----------------
									Globa_2->charger_state = 0x05;
									Globa_2->checkout = 0;
									Globa_2->pay_step = 0;//2通道在等待刷卡扣费
									Globa_2->Charger_Over_Rmb_flag = 0;
									break;
								}
				
						    Globa_2->VIN_Judgment_Result = 0;
						    Globa_2->Manu_start = 1;
						    Globa_2->charger_start = 1;
						    Globa_2->charger_state = 0x04; //启动充电
								break;
							}
							default:{
								break;
							}
						}
					}else if(QT_timer_tick(500) == 1){  //定时500MS
						Page_1000.link = Globa_2->gun_link;//(Globa_2->gun_link == 1)?1:0;  //1:连接  0：断开
						Page_1000.VIN_Support = (((Globa_1->have_hart == 1)&&(Globa_2->VIN_Support_falg == 1)) == 1)? 1:0;
						Page_1000.other_link = 0;
						msg.type = 0x1000;
						memcpy(&msg.argv[0], &Page_1000.reserve1, sizeof(PAGE_1000_FRAME));
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_1000_FRAME), IPC_NOWAIT);
						QT_timer_start(500);
					}

					break;
				}
				case 0x121:{//--------------1 自动充电-》 充电方式选着 状态  1.2.1--------
					if(msg.type == 0x1100){//收到该界面消息
						switch(msg.argv[0]){//判断 在当前界面下 用户操作的按钮
							case 0x01:{//自动充电 按钮
								charger_type = 1;
								Globa_2->Card_charger_type = 4;
								msg.type = 0x200;//等待刷卡界面
								memset(&Page_200.reserve1, '0', sizeof(PAGE_200_FRAME));
								Page_200.state = 1;//1 没有检测到用户卡
								memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
                Globa_2->Special_card_offline_flag = 0;
								Globa_2->QT_Step = 0x20; // 2 自动充电-等待刷卡界面  2.0

								Globa_2->checkout = 0;
								Globa_2->pay_step = 1;
							  Globa_2->Start_Mode = CARD_START_TYPE;//刷卡启动

								count_wait = 0;
								memset(&Globa_2->card_sn[0], '0', sizeof(Globa_2->card_sn));
								memset(&Globa_2->tmp_card_sn[0], '0', sizeof(Globa_2->tmp_card_sn));
								memset(&busy_frame, '0', sizeof(busy_frame));
								memset(Globa_2->VIN, '0', sizeof(Globa_2->VIN));

								if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
								 Voicechip_operation_function(0x04);//04H     “请刷卡”
								}else{
									Voicechip_operation_function(0x00);//00H	"欢迎光临.wav 300s 请刷卡1.wav"	
								}
								Globa_2->User_Card_check_flag = 0;
								Globa_2->User_Card_check_result = 0;
							  Select_NO_Over_count = 0;
                if(pre_old_id != 0){
								  pthread_mutex_lock(&busy_db_pmutex);
								  Set_0X550X66_TO0X00_Busy_ID_Gun_2(2); //强制性更新上传标志
								  pthread_mutex_unlock(&busy_db_pmutex);
								  pre_old_id = 0;
							  	Select_NO_Over_count = 0;
							  	memset(&pre_old_busy_frame, '0', sizeof(pre_old_busy_frame));
							  }
								break;
							}
							case 0x02:{//按金额充电 按钮
								msg.type = 0x1120; // 按金额充电界面
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);

								Globa_2->QT_Step = 0x131; // 1 自动充电-》 输入 状态  1.3.1---

								break;
							}
							case 0x03:{//按电量充电 按钮
								msg.type = 0x1130; // 按电量充电界面
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);

								Globa_2->QT_Step = 0x131; // 1 自动充电-》 输入 状态  1.3.1---

								break;
							}
							case 0x04:{//按时间充电 按钮
								msg.type = 0x1140; // 按时间充电界面
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);

								Globa_2->QT_Step = 0x131; // 1 自动充电-》 输入 状态  1.3.1---

								break;
							}
							case 0x05:{//预约充电 按钮
								msg.type = 0x1150; // 预约充电界面
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);

								Globa_2->QT_Step = 0x131;//1 自动充电-》 输入 状态  1.3.1---

								break;
							}
							case 0x07:{//退出按钮
								msg.type = 0x150;
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);					

								Globa_2->QT_Step = 0x03; //自动充电分开的AB通道

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
									Globa_2->QT_charge_money = (msg.argv[2]<<8) + msg.argv[1];
									charger_type = 2;
									Globa_2->Card_charger_type = 3;
								}else if(msg.type == 0x1130){//按电量充电
									Globa_2->QT_charge_kwh =   (msg.argv[2]<<8) + msg.argv[1];
									charger_type = 3;
									Globa_2->Card_charger_type = 1;
								}else if(msg.type == 0x1140){//按时间充电
									Globa_2->QT_charge_time =  ((msg.argv[2]<<8) + msg.argv[1])*60 + Globa_2->user_sys_time;//此处保存的是需要结束充电的时间
									 Globa_2->APP_Card_charge_time_limit =  ((msg.argv[2]<<8) + msg.argv[1])*60;
									charger_type = 4;
									Globa_2->Card_charger_type = 2;
								}else if(msg.type == 0x1150){//预约时间
									time_t systime1;
									struct tm tm_t1;
									systime1 = time(NULL);        //获得系统的当前时间 
									localtime_r(&systime1, &tm_t1);  //结构指针指向当前时间	
								  if((tm_t1.tm_min + tm_t1.tm_hour*60)> (msg.argv[2] + msg.argv[1]*60)){//需要隔天 ---倒计时总时间
										Globa_2->QT_pre_time = systime1 + (24*3600 - (tm_t1.tm_hour*3600 + tm_t1.tm_min*60 + tm_t1.tm_sec)) + (msg.argv[2] + msg.argv[1]*60)*60;//最终目标值
									}else{
										Globa_2->QT_pre_time = systime1 + (msg.argv[2] + msg.argv[1]*60)*60 - (tm_t1.tm_sec + tm_t1.tm_min*60 + tm_t1.tm_hour*3600);//此处保存的是需要开始充电的时间  
									}
									charger_type = 5;
									Globa_2->Card_charger_type = 5;
								}
								
					  	  Select_NO_Over_count = 0;
                if(pre_old_id != 0){
								  pthread_mutex_lock(&busy_db_pmutex);
								  Set_0X550X66_TO0X00_Busy_ID_Gun_2(2); //强制性更新上传标志
								  pthread_mutex_unlock(&busy_db_pmutex);
								  pre_old_id = 0;
							  	Select_NO_Over_count = 0;
							  	memset(&pre_old_busy_frame, '0', sizeof(pre_old_busy_frame));
							  }
								
								msg.type = 0x200;//等待刷卡界面
								memset(&Page_200.reserve1, '0', sizeof(PAGE_200_FRAME));
								Page_200.state = 1;//1 没有检测到用户卡
								memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
                Globa_2->Special_card_offline_flag = 0;
								Globa_2->QT_Step = 0x20; // 2 自动充电-等待刷卡界面  2.0

								Globa_2->checkout = 0;
								Globa_2->pay_step = 1;
							  Globa_2->Start_Mode = CARD_START_TYPE;//刷卡启动

								count_wait = 0;
								memset(&Globa_2->card_sn[0], '0', sizeof(Globa_2->card_sn));
								memset(&Globa_2->tmp_card_sn[0], '0', sizeof(Globa_2->tmp_card_sn));
								memset(&busy_frame, '0', sizeof(busy_frame));
								memset(Globa_2->VIN, '0', sizeof(Globa_2->VIN));
								

							 if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
								 Voicechip_operation_function(0x04);//04H     “请刷卡”
								}else{
									Voicechip_operation_function(0x00);//00H	"欢迎光临.wav 300s 请刷卡1.wav"	
								}
								Globa_2->User_Card_check_flag = 0;
								Globa_2->User_Card_check_result = 0;
								break;
							}
							case 0x02:{//返回按钮
								msg.type = 0x1100;
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 2, IPC_NOWAIT);					
								if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
									Voicechip_operation_function(0x02);  //02H"    “请选择充电方式"
								}
								Globa_2->QT_Step = 0x121; //1 自动充电-》充电方式选着 状态  1.2.1

								break;
							}
							default:{
								break;
							}
						}
					}

					break;
				}
				case 0x19:{//---------------2 自动充电-》 等待刷卡界面   2.0--------------
					if(Globa_1->have_hart == 1){
						if((Globa_2->User_Card_check_result == 0)&&(Globa_2->User_Card_check_flag == 0)){
							if(Globa_2->BMS_Info_falg == 1) //正确获取到了VIN码
							{
								if((0 != memcmp(&Globa_2->VIN[0],"00000000000000000", sizeof(Globa_2->VIN)))&&(!((Globa_2->VIN[0] == 0xFF)&&(Globa_2->VIN[1] == 0xFF)&&(Globa_2->VIN[2] == 0xFF))))
								{
									msg.type = 0x200;//等待刷卡界面
									Page_200.state = 41;//卡片验证中...请保持刷卡状态！
								//	memcpy(&Page_200.sn[0], &busy_frame.card_sn[0], sizeof(Page_200.sn));
									memcpy(&Page_200.sn[0], &Globa_2->VIN[0], sizeof(Page_200.sn));
									memcpy(&Page_200.rmb[0], &busy_frame.last_rmb[0], sizeof(Page_200.rmb));
									memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
									msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
									memcpy(&Globa_2->User_Card_check_card_ID[0], &Globa_2->card_sn[0], sizeof(Globa_2->User_Card_check_card_ID));
									Globa_2->User_Card_check_flag = 1;
									break;
								}else{
									Globa_2->User_Card_check_flag = 0;
									msg.type = 0x200;//等待刷卡界面
									Page_200.state = 19;//14;//MAC校验错误
									memcpy(&Page_200.sn[0], &Globa_2->card_sn[0], sizeof(Page_200.sn));
									sprintf(&temp_char[0], "%08d", Globa_2->last_rmb);	
									memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
									memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
									msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
									Globa_2->Operation_command = 2; //解锁
									memcpy(&busy_frame.car_sn[0],Globa_2->VIN,sizeof(Globa_2->VIN));//
									Globa_2->VIN_Judgment_Result = 2;
									Globa_2->Manu_start = 0;
								 if(id != 0){
									pthread_mutex_lock(&busy_db_pmutex);
									if(Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00) == -1){
										sleep(1);
										Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
									}
									pthread_mutex_unlock(&busy_db_pmutex);
									sleep(1);
										//插入到需要上传的交易记录数据库中
									pthread_mutex_lock(&busy_db_pmutex);
									 all_id = 0;
									 Insertflag = Insert_Charger_Busy_DB(&busy_frame, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
									if((Insertflag != -1)&&(all_id != 0)){//表示插入数据成功，其他的则需要重新插入
										Delete_Record_busy_DB_2(id);
										if(DEBUGLOG) RUN_GUN2_LogOut("2号枪交易数据,插入到正式数据库中并删除临时数据库记录 ID = %d",id);  
										id = 0;
									}else{
										if(DEBUGLOG) RUN_GUN2_LogOut("2号枪交易数据,插入到正式数据库中失败 ID = %d",id);  
									}
									pthread_mutex_unlock(&busy_db_pmutex);

									if(id != 0){
										pthread_mutex_lock(&busy_db_pmutex);
										for(i=0;i<5;i++){
											if(Select_NO_Over_Record_2(id,0)!= -1){//表示有没有上传的记录
												Select_NO_Over_count = 0;
												memcpy(&pre_old_busy_frame, &busy_frame, sizeof(pre_old_busy_frame));
												pre_old_id = id;
												if(Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00) == -1){
													sleep(1);
													Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
												}
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
								 }
									//sleep(2);
									msg.type = 0x150;
									msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
									Globa_2->QT_Step = 0x03; //自动充电分开的AB通道
									break;//退出，不能充电
								}	
							}
						}
					}
				/*	else{
						Globa_2->VIN_Judgment_Result = 2;
						Globa_2->User_Card_check_flag = 0;
						Globa_2->User_Card_check_result = 0;
						Globa_2->Manu_start = 0;
						msg.type = 0x200;//等待刷卡界面
						Page_200.state = 42 ;//离线不能获取结果
						memcpy(&Page_200.sn[0], &busy_frame.card_sn[0], sizeof(Page_200.sn));
						memcpy(&Page_200.rmb[0], &busy_frame.last_rmb[0], sizeof(Page_200.rmb));
						memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
						Globa_2->Operation_command = 2; //解锁
						memcpy(&busy_frame.car_sn[0],Globa_2->VIN,sizeof(Globa_2->VIN));//

						if(id != 0){
							pthread_mutex_lock(&busy_db_pmutex);
							if(Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00) == -1){
								sleep(1);
								Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
							}
							pthread_mutex_unlock(&busy_db_pmutex);
							sleep(1);
							
								//插入到需要上传的交易记录数据库中
							pthread_mutex_lock(&busy_db_pmutex);
							 all_id = 0;
							 Insertflag = Insert_Charger_Busy_DB(&busy_frame, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
							if((Insertflag != -1)&&(all_id != 0)){//表示插入数据成功，其他的则需要重新插入
								Delete_Record_busy_DB_2(id);
								if(DEBUGLOG) RUN_GUN2_LogOut("2号枪交易数据,插入到正式数据库中并删除临时数据库记录 ID = %d",id);  
								id = 0;
							}else{
								if(DEBUGLOG) RUN_GUN2_LogOut("2号枪交易数据,插入到正式数据库中失败 ID = %d",id);  
							}
							pthread_mutex_unlock(&busy_db_pmutex);

							if(id != 0){
								pthread_mutex_lock(&busy_db_pmutex);
								for(i=0;i<5;i++){
									if(Select_NO_Over_Record_2(id,0)!= -1){//表示有没有上传的记录
										Select_NO_Over_count = 0;
										memcpy(&pre_old_busy_frame, &busy_frame, sizeof(pre_old_busy_frame));
										pre_old_id = id;
										if(Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00) == -1){
											sleep(1);
											Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
										}
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
					  }
						sleep(2);
						msg.type = 0x150;
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_2->QT_Step = 0x03; //自动充电分开的AB通道
						Globa_2->Manu_start = 0;
					}
			*/
			
					if((Globa_2->User_Card_check_result >= 2)&&(Globa_2->User_Card_check_result <= 14)){
						Globa_2->User_Card_check_flag = 0;
						msg.type = 0x200;//等待刷卡界面
						Page_200.state = 17 + (Globa_2->User_Card_check_result - 2 );//14;//MAC校验错误
						memcpy(&Page_200.sn[0], &Globa_2->card_sn[0], sizeof(Page_200.sn));
						sprintf(&temp_char[0], "%08d", Globa_2->last_rmb);	
						memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
						memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
						Globa_2->Operation_command = 2; //解锁
					  memcpy(&busy_frame.car_sn[0],Globa_2->VIN,sizeof(Globa_2->VIN));//
						Globa_2->VIN_Judgment_Result = 2;
            Globa_2->Manu_start = 0;
		       if(id != 0){
						pthread_mutex_lock(&busy_db_pmutex);
						if(Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00) == -1){
							sleep(1);
							Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
						}
						pthread_mutex_unlock(&busy_db_pmutex);
						sleep(1);
							//插入到需要上传的交易记录数据库中
						pthread_mutex_lock(&busy_db_pmutex);
						 all_id = 0;
						 Insertflag = Insert_Charger_Busy_DB(&busy_frame, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
						if((Insertflag != -1)&&(all_id != 0)){//表示插入数据成功，其他的则需要重新插入
							Delete_Record_busy_DB_2(id);
							if(DEBUGLOG) RUN_GUN2_LogOut("2号枪交易数据,插入到正式数据库中并删除临时数据库记录 ID = %d",id);  
							id = 0;
						}else{
							if(DEBUGLOG) RUN_GUN2_LogOut("2号枪交易数据,插入到正式数据库中失败 ID = %d",id);  
						}
						pthread_mutex_unlock(&busy_db_pmutex);

						if(id != 0){
							pthread_mutex_lock(&busy_db_pmutex);
							for(i=0;i<5;i++){
								if(Select_NO_Over_Record_2(id,0)!= -1){//表示有没有上传的记录
									Select_NO_Over_count = 0;
									memcpy(&pre_old_busy_frame, &busy_frame, sizeof(pre_old_busy_frame));
									pre_old_id = id;
									if(Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00) == -1){
										sleep(1);
										Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
									}
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
					 }
						//sleep(2);
						msg.type = 0x150;
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_2->QT_Step = 0x03; //自动充电分开的AB通道
						break;//退出，不能充电
					}
					else if(Globa_2->User_Card_check_result == 1){//验证成功请在次刷卡
						Globa_2->User_Card_check_flag = 0;
						Globa_2->User_Card_check_result = 0;
						if((0 == memcmp(&Globa_2->card_sn[0], "0000000000000000", sizeof(Globa_2->card_sn))))
					  {//获取vin 对应的卡号
				    	Globa_2->Manu_start = 0;
							msg.type = 0x200;//等待刷卡界面
							Page_200.state = 42;//VIN 获取对应的账号失败
							memcpy(&Page_200.sn[0], &Globa_2->card_sn[0], sizeof(Page_200.sn));
							sprintf(&temp_char[0], "%08d", Globa_2->last_rmb);	
							memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
							memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
							msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
							Globa_2->Operation_command = 2; //解锁
							memcpy(&busy_frame.car_sn[0],Globa_2->VIN,sizeof(Globa_2->VIN));//
							Globa_2->VIN_Judgment_Result = 2;
						  sleep(3);
							if(id != 0){
								pthread_mutex_lock(&busy_db_pmutex);
								if(Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00) == -1){
									sleep(1);
									Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
								}
								pthread_mutex_unlock(&busy_db_pmutex);
								sleep(1);
								//插入到需要上传的交易记录数据库中
								pthread_mutex_lock(&busy_db_pmutex);
								all_id = 0;
								Insertflag = Insert_Charger_Busy_DB(&busy_frame, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
								if((Insertflag != -1)&&(all_id != 0)){//表示插入数据成功，其他的则需要重新插入
									Delete_Record_busy_DB_2(id);
									if(DEBUGLOG) RUN_GUN2_LogOut("2号枪交易数据,插入到正式数据库中并删除临时数据库记录 ID = %d",id);  
									id = 0;
								}else{
									if(DEBUGLOG) RUN_GUN2_LogOut("2号枪交易数据,插入到正式数据库中失败 ID = %d",id);  
								}
								pthread_mutex_unlock(&busy_db_pmutex);

								if(id != 0){
									pthread_mutex_lock(&busy_db_pmutex);
									for(i=0;i<5;i++){
										if(Select_NO_Over_Record_2(id,0)!= -1){//表示有没有上传的记录
											Select_NO_Over_count = 0;
											memcpy(&pre_old_busy_frame, &busy_frame, sizeof(pre_old_busy_frame));
											pre_old_id = id;
											if(Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00) == -1){
												sleep(1);
												Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
											}
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
							}
							msg.type = 0x150;
							msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa_2->QT_Step = 0x03; //自动充电分开的AB通道
							break;//退出，不能充电
						}
						
						Globa_2->card_type = 2; //卡类型 0:未知 1：用户卡 2：员工卡 3：维护卡卡类型 1：用户卡 2：员工卡
						Globa_2->pay_step = 0;//进入空闲状态
				
						if((Globa_2->private_price_acquireOK == 1)){//专有电价
							for(i=0;i<4;i++){
								share_time_kwh_price[i] = Globa_2->Special_price_data.share_time_kwh_price[i];
								share_time_kwh_price_serve[i] = Globa_2->Special_price_data.share_time_kwh_price_serve[i]; //服务
							}
							Serv_Type = Globa_2->Special_price_data.Serv_Type;
						}else{//通用电价
							for(i=0;i<4;i++){
								share_time_kwh_price[i] = Globa_2->Charger_param2.share_time_kwh_price[i];
								share_time_kwh_price_serve[i] = Globa_2->Charger_param2.share_time_kwh_price_serve[i]; //服务
							}
							Serv_Type = Globa_2->Charger_param.Serv_Type;
						}
						memcpy(&busy_frame.card_sn[0], &Globa_2->card_sn[0], sizeof(busy_frame.card_sn));

					
						systime = time(NULL);         //获得系统的当前时间 
						localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
			
						memcpy(&busy_frame.end_time[0], &temp_char[0], sizeof(busy_frame.end_time));

						memcpy(&busy_frame.flag[0], "00", sizeof(busy_frame.flag));//员工卡直接标志位完成，不灰锁
					
						sprintf(&temp_char[0], "%08d", Globa_2->last_rmb);		
						memcpy(&busy_frame.last_rmb[0], &temp_char[0], sizeof(busy_frame.last_rmb));

						memcpy(&busy_frame.ter_sn[0],  &Globa_2->Charger_param.SN[0], sizeof(busy_frame.ter_sn));

						Globa_2->meter_stop_KWH = Globa_2->meter_start_KWH;
					

						sprintf(&temp_char[0], "%08d", Globa_2->meter_stop_KWH);				
						memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

						sprintf(&temp_char[0], "%08d", Globa_2->Charger_param.Price);    
						memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));

						sprintf(&temp_char[0], "%08d", Globa_2->kwh);				
						memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
						
						sprintf(&temp_char[0], "%08d", Globa_2->rmb);				
						memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));

						//起始总费用等于电量费用，暂时只做安电量收服务费
						sprintf(&temp_char[0], "%08d", Globa_2->total_rmb);
						memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));
						sprintf(&temp_char[0], "%08d", Globa_2->total_rmb-Globa_2->rmb);
						memcpy(&busy_frame.server_rmb[0], &temp_char[0], sizeof(busy_frame.server_rmb));
						busy_frame.type[0] = '0';//充电方式（01：刷卡，02：手机）
						busy_frame.type[1] = '3';
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
						busy_frame.ConnectorId[1] = '2';
						
						busy_frame.End_code[0] = '1';//结束代码
						busy_frame.End_code[1] = '1';
						memcpy(&busy_frame.car_sn[0],Globa_2->VIN,sizeof(Globa_2->VIN));//

						tmp_value =0;
				    if(id != 0 ){
					  	pthread_mutex_lock(&busy_db_pmutex);
						  Delete_Record_busy_DB_2(id);
					  	pthread_mutex_unlock(&busy_db_pmutex);
							sleep(1);
					  }
						
						id = 0;
						pthread_mutex_lock(&busy_db_pmutex);
						Insertflag = Insert_Charger_Busy_DB_2(&busy_frame, &id, 0x55);//写新数据库记录 无流水号 传输标志0x55不能上传
						pthread_mutex_unlock(&busy_db_pmutex);
						if(DEBUGLOG) RUN_GUN2_LogOut("%s %d","2号枪:刷卡启动充电并插入交易记录中的ID=",id);  
						if(Insertflag == -1){
							Globa_2->Manu_start = 0;
							Page_1300.cause = 63;
							Page_1300.Capact = Globa_2->BMS_batt_SOC;
							Page_1300.elapsed_time[0] =  Globa_2->Time;
							Page_1300.elapsed_time[1] =  Globa_2->Time>>8;
							Page_1300.KWH[0] =  Globa_2->kwh;
							Page_1300.KWH[1] =  Globa_2->kwh>>8;
							Page_1300.RMB[0] =  Globa_2->total_rmb;
							Page_1300.RMB[1] =  Globa_2->total_rmb>>8;
							Page_1300.RMB[2] =  Globa_2->total_rmb>>16;
							Page_1300.RMB[3] =  Globa_2->total_rmb>>24;
					
							memcpy(&Page_1300.SN[0], &Globa_2->card_sn[0], sizeof(Page_1300.SN));
							//Page_1300.APP = 3;   //充电启动方式 1:刷卡  2：手机APP
	            #ifdef CHECK_BUTTON	
								Globa_2->pay_step = 0;
								Page_1300.APP = 3;   //充电启动方式 1:刷卡  2：手机APP
							#else 
								Globa_2->pay_step = 0;
								Page_1300.APP = 1;   //充电启动方式 1:刷卡  2：手机APP
							#endif
							memcpy(&msg.argv[0], &Page_1300.reserve1, sizeof(PAGE_1300_FRAME));
							msg.type = 0x1300;//充电结束结算界面
							msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_1300_FRAME), IPC_NOWAIT);
							sleep(2);
		           //防止按键处理不同步，此处同步一下
							msg.type = 0x150;//
							msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
							Globa_2->QT_Step = 0x03; //主界面
							//Globa_2->QT_Step = 0x23; // 2      充电结束 结算界面 2.3----------------
							Globa_2->charger_state = 0x05;
							Globa_2->checkout = 0;
							Globa_2->pay_step = 0;//2通道在等待刷卡扣费
							Globa_2->Charger_Over_Rmb_flag = 0;
							break;
						}

						if((Globa_2->Charger_param.System_Type == 1)&&(Globa_1->charger_start == 1)){//轮流充电的时候出现一把枪正在充电则另一把需要等待
							memset(&Page_daojishi.reserve1, 0x00, sizeof(PAGE_DAOJISHI_FRAME));
							memcpy(&Page_daojishi.card_sn[0], &Globa_2->card_sn[0],sizeof(Globa_2->card_sn));
							Page_daojishi.type = 1;
							memcpy(&msg.argv[0], &Page_daojishi.reserve1, sizeof(PAGE_DAOJISHI_FRAME));
							msg.type = 0x1151;//倒计时界面
							msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_DAOJISHI_FRAME), IPC_NOWAIT); 
							sleep(3);
							QT_timer_start(500);
							Globa_2->QT_Step = 0x201; // 1 自动充电-》倒计时界面  2.0.1
							Globa_2->charger_state = 0x06;
							Globa_2->checkout = 0;
							//Globa_2->pay_step = 2;//0：空闲 1：刷卡启动 2：刷卡停止预约 3：刷卡停止充电 4：刷卡结算
							count_wait = 0;
							card_falg_count = 0;
							break;
						}else{
							Globa_2->VIN_Judgment_Result = 1;
							msg.type = 0x1111;
							memset(&Page_1111.reserve1, 0x00, sizeof(PAGE_1111_FRAME));									
							Page_1111.APP  = 3;   //充电启动方式 1:刷卡  2：手机APP
							
							memcpy(&Page_1111.SN[0], &Globa_2->card_sn[0], sizeof(Page_1111.SN));
							memcpy(&msg.argv[0], &Page_1111.reserve1, sizeof(PAGE_1111_FRAME));
							msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_1111_FRAME), IPC_NOWAIT);
				
							
							QT_timer_start(500);
							Globa_2->QT_Step = 0x21;   // 2 自动充电-》 充电界面 状态 2.1-
							if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
								Voicechip_operation_function(0x05);  //05H     “系统自检中，请稍候”
							}
							Globa_2->charger_state = 0x04;
							Globa_2->checkout = 0;
							//Globa_2->pay_step = 3;//0：空闲 1：刷卡启动 2：刷卡停止预约 3：刷卡停止充电 4：刷卡结算
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
					
					
				  if(Globa_2->charger_start == 0){//充电彻底结束处理
						//-------------------扣费 更新业务进度数据----------------------------
						sleep(3);
						Globa_2->meter_stop_KWH = (Globa_2->meter_KWH >= Globa_2->meter_stop_KWH)? Globa_2->meter_KWH:Globa_2->meter_stop_KWH;//取初始电表示数
					  Globa_2->kwh = (Globa_2->meter_stop_KWH - Globa_2->meter_start_KWH);

						g_ulcharged_kwh[pre_price_index] = Globa_2->kwh - ul_start_kwh;//累加当前时段的耗电量	
						cur_price_index = GetCurPriceIndex(); //获取当前时间
						if(cur_price_index != pre_price_index){
							ul_start_kwh = Globa_2->kwh - g_ulcharged_kwh[cur_price_index];//load new start kwh,需要减去时段之前累加的电量，否则该时段之前累加的电量丢失----20160312
							pre_price_index = cur_price_index;							
						}	
						for(i = 0;i<4;i++){
							g_ulcharged_rmb[i] = g_ulcharged_kwh[i]*share_time_kwh_price[i];
						}
						
						for(i = 0;i<4;i++){
							g_ulcharged_rmb[i] = (g_ulcharged_rmb[i] + COMPENSATION_VALUE)/100;
						}
						
						Globa_2->rmb = g_ulcharged_rmb[0] + g_ulcharged_rmb[1] + g_ulcharged_rmb[2] + g_ulcharged_rmb[3]; 

						if(Globa_2->Charger_param.Serv_Type == 1){ //按次数收
							Globa_2->total_rmb = (Globa_2->rmb + Globa_2->Charger_param.Serv_Price);//已充电电量对应的金额
						}else if(Globa_2->Charger_param.Serv_Type == 2){//按时间收
							Globa_2->total_rmb = (Globa_2->rmb + Globa_2->Time*Globa_2->Charger_param.Serv_Price/600);//已充电电量对应的金额
						}else{
							//Globa_2->total_rmb = (Globa_2->rmb + Globa_2->kwh*Globa_2->Charger_param.Serv_Price/100);//已充电电量对应的金额
							for(i = 0;i<4;i++){
								g_ulcharged_rmb_Serv[i] = g_ulcharged_kwh[i]*share_time_kwh_price_serve[i];
							}
							for(i = 0;i<4;i++){
								g_ulcharged_rmb_Serv[i] =( g_ulcharged_rmb_Serv[i] + COMPENSATION_VALUE)/100;
							}
							Globa_2->total_rmb = Globa_2->rmb + g_ulcharged_rmb_Serv[0] + g_ulcharged_rmb_Serv[1] + g_ulcharged_rmb_Serv[2] + g_ulcharged_rmb_Serv[3]; 
						}
		        
				    Globa_2->ISO_8583_rmb_Serv = Globa_2->total_rmb - Globa_2->rmb ;               //总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
						Globa_2->ISO_8583_rmb = Globa_2->rmb;                     //电量消费金额 0.01yuan
						
						sprintf(&temp_char[0], "%08d", Globa_2->meter_stop_KWH);				
						memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

						sprintf(&temp_char[0], "%08d", Globa_2->kwh);				
						memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
						
						sprintf(&temp_char[0], "%08d", Globa_2->rmb);				
						memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));
						if(Globa_2->kwh != 0){
							sprintf(&temp_char[0], "%08d", (Globa_2->rmb*100/Globa_2->kwh));  
						}else{
							sprintf(&temp_char[0], "%08d", 0);  
						}
						memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));
						sprintf(&temp_char[0], "%08d", Globa_2->total_rmb);				
						memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));

						sprintf(&temp_char[0], "%08d", Globa_2->total_rmb-Globa_2->rmb);				
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
						
						sprintf(&temp_char[0], "%.17s",Globa_2->VIN);//服务费收费类型 01：按次收费；02：按时间收费(单位：分/10分钟；03：按电量	9	N	N2
						//memcpy(&busy_frame.car_sn[0], &Globa_2->VIN[0], sizeof(busy_frame.car_sn));//
						memcpy(&busy_frame.car_sn[0], &temp_char[0], sizeof(busy_frame.car_sn));

						tmp_value =0;
						
						if(Globa_2->Charger_Over_Rmb_flag == 1){
							Page_1300.cause = 13;
						  sent_warning_message(0x94, Page_1300.cause, 2, 0);							
						}else if(Globa_2->Charger_Over_Rmb_flag == 3){
						  Page_1300.cause = 19;
						  sent_warning_message(0x94, Page_1300.cause, 2, 0);							
						}else if(Globa_2->Charger_Over_Rmb_flag == 4){
						  Page_1300.cause = 62; 
							sent_warning_message(0x94, Page_1300.cause, 2, 0);
						}else if(Globa_2->Charger_Over_Rmb_flag == 5){
						  Page_1300.cause = 65; 
							sent_warning_message(0x94, Page_1300.cause, 2, 0);
						}else if(Globa_2->Charger_Over_Rmb_flag == 6){//达到SOC限制条件
						  Page_1300.cause = 9;
							sent_warning_message(0x94, Page_1300.cause, 2, 0);							
					  }else if(Globa_2->Charger_Over_Rmb_flag == 7){//读取到的电表电量异常
						  Page_1300.cause = 8;
						  sent_warning_message(0x94, Page_1300.cause, 2, 0);							
					  }else{
							Page_1300.cause = Globa_2->charger_over_flag; 
					  }
					
					  sprintf(&temp_char[0], "%02d", Page_1300.cause);		
						memcpy(&busy_frame.End_code[0], &temp_char[0], sizeof(busy_frame.End_code));//结束原因代码
						
						pthread_mutex_lock(&busy_db_pmutex);
						if(Update_Charger_Busy_DB_2(id, &busy_frame, 1, 0x66) == -1){
							sleep(1);
							Update_Charger_Busy_DB_2(id, &busy_frame, 1, 0x66);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
						}
						pthread_mutex_unlock(&busy_db_pmutex);
					
						//Page_1300.cause = Globa_2->charger_over_flag;
						Page_1300.Capact = Globa_2->BMS_batt_SOC;
						Page_1300.elapsed_time[0] =  Globa_2->Time;
						Page_1300.elapsed_time[1] =  Globa_2->Time>>8;
						Page_1300.KWH[0] =  Globa_2->kwh;
						Page_1300.KWH[1] =  Globa_2->kwh>>8;
						Page_1300.RMB[0] =  Globa_2->total_rmb;
						Page_1300.RMB[1] =  Globa_2->total_rmb>>8;
						Page_1300.RMB[2] =  Globa_2->total_rmb>>16;
						Page_1300.RMB[3] =  Globa_2->total_rmb>>24;
						
						memcpy(&Page_1300.SN[0], &Globa_2->card_sn[0], sizeof(Page_1300.SN));
						
            if(Globa_2->Start_Mode == VIN_START_TYPE)
						{
						  Globa_2->pay_step = 0;
							//Page_1300.APP = 3;   //充电启动方式 1:刷卡  2：手机APP
							#ifdef CHECK_BUTTON	
								Globa_2->pay_step = 0;
								Page_1300.APP = 3;   //充电启动方式 1:刷卡  2：手机APP
							#else 
								Globa_2->pay_step = 0;
								Page_1300.APP = 1;   //充电启动方式 1:刷卡  2：手机APP
							#endif
						}
						else
						{
							Globa_2->pay_step = 4;//1通道在等待刷卡扣费
							Page_1300.APP = 1;   //充电启动方式 1:刷卡  2：手机APP
							if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
								Voicechip_operation_function(0x07);//07H     “充电已完成，请刷卡结算”
							}else{
								Voicechip_operation_function(0x10);//10H	请刷卡结算.wav
							}
			
						}	
						memcpy(&msg.argv[0], &Page_1300.reserve1, sizeof(PAGE_1300_FRAME));
						msg.type = 0x1300;//充电结束结算界面
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_1300_FRAME), IPC_NOWAIT);

						Globa_2->QT_Step = 0x23; // 2      充电结束 结算界面 2.3----------------
						Globa_2->charger_state = 0x05;
						Globa_2->checkout = 0;
						Globa_2->Charger_Over_Rmb_flag = 0;
						count_wait = 0;
						count_wait1 = 0;
						break;
					}
					
					
					break;
				}
				case 0x20:{//---------------2 自动充电-》 等待刷卡界面   2.0--------------
					Globa_2->kwh = 0;
					ul_start_kwh = 0;
					cur_price_index = GetCurPriceIndex(); //获取当前时间
					pre_price_index = cur_price_index;
					for(i=0;i<4;i++){
						g_ulcharged_rmb[i] = 0;
						g_ulcharged_kwh[i] = 0;
						g_ulcharged_rmb_Serv[i] = 0;
						share_time_kwh_price[i] = 0;
						share_time_kwh_price_serve[i] = 0; //服务
					}
					Globa_2->rmb = 0;
					Globa_2->total_rmb = 0;
				  Globa_2->ISO_8583_rmb_Serv = 0;//总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
					Globa_2->ISO_8583_rmb = 0;                     //电量消费金额 0.01yuan
					
					temp_kwh = 0;
					Globa_2->soft_KWH = 0;
					Globa_2->Time = 0;
					Globa_2->card_type = 0;//卡类型 0:未知 1：用户卡 2：员工卡 3：维护卡

					count_wait++;
					if(count_wait > 150){//等待时间超时30S
						count_wait = 0;
		
						msg.type = 0x150;
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);

						Globa_2->QT_Step = 0x03; //自动充电分开的AB通道

						break;//超时必须退出
					}

					/*if(Globa_1->checkout == 0){//卡片处理失败，请重新操作
						//继续寻卡
						break;
					}else*/
					if(Globa_2->checkout == 4){//无法识别充电卡
						Globa_2->checkout = 0;//清除刷卡结果标志
						count_wait = 0;
						//Voicechip_operation_function(0x0d);//0dH     “卡片未注册，请联系发卡单位处理”
						//等待30S超时
						msg.type = 0x200;//等待刷卡界面
						Page_200.state = 2;
						memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);

						break;
					}else if((Globa_2->checkout == 5)||(Globa_2->checkout == 6)||(Globa_2->checkout == 7)){//卡片状态异常
						msg.type = 0x200;//等待刷卡界面
						if(Globa_2->checkout == 5){//卡片状态异常
							Page_200.state = 4;
							if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
								Voicechip_operation_function(0x0b);//0bH     “卡片已灰锁，请解锁”
							}
						}else if(Globa_2->checkout == 6){//卡片已使用
							Page_200.state = 9;
						}else if(Globa_2->checkout == 7){//非充电卡
							Page_200.state = 8;
						}

						memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);

						sleep(3);

						msg.type = 0x150;
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_2->QT_Step = 0x03; //自动充电分开的AB通道

						break;
					}else if((Globa_2->checkout == 1)||(Globa_2->checkout == 3)||(Globa_2->checkout == 12)){//卡片预处理成功 员工卡
						if(Globa_1->have_hart == 1){
							if((Globa_2->User_Card_check_result == 0)&&(Globa_2->User_Card_check_flag == 0)){
								msg.type = 0x200;//等待刷卡界面
								Page_200.state = 12;//卡片验证中...请保持刷卡状态！
								memcpy(&Page_200.sn[0], &busy_frame.card_sn[0], sizeof(Page_200.sn));
								memcpy(&Page_200.rmb[0], &busy_frame.last_rmb[0], sizeof(Page_200.rmb));
								memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
								memcpy(&Globa_2->User_Card_check_card_ID[0], &Globa_2->card_sn[0], sizeof(Globa_2->User_Card_check_card_ID));
								Globa_2->User_Card_check_flag = 1;
								break;
							}
						}else{
						 if((Globa_2->qiyehao_flag == 1)&&(Globa_2->Special_card_offline_flag == 0)){//企业标号---离线无法启动
								msg.type = 0x200;//等待刷卡界面
								Page_200.state = 35;//企业标号---离线无法启动
								Page_200.reserve1 = 0;				
								memcpy(&Page_200.sn[0], &Globa_2->card_sn[0], sizeof(Page_200.sn));
								sprintf(&temp_char[0], "%08d", Globa_2->last_rmb);	
								memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
								memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
					
								sleep(2);
								msg.type = 0x150;
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
								Globa_2->QT_Step = 0x03; //自动充电分开的AB通道
								break;//退出，不能充电
							}else if((Globa_2->order_chg_flag == 1)&&(Globa_2->Special_card_offline_flag == 0)){////有序充电用户卡标记---离线无法启动
								msg.type = 0x200;//等待刷卡界面
								Page_200.state = 36;//企业标号---有序充电用户卡标记---离线无法启动
								Page_200.reserve1 = 0;				
								memcpy(&Page_200.sn[0], &Globa_2->card_sn[0], sizeof(Page_200.sn));
								sprintf(&temp_char[0], "%08d", Globa_2->last_rmb);	
								memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
								memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
			
								sleep(2);
								msg.type = 0x150;
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
								Globa_2->QT_Step = 0x03; //自动充电分开的AB通道
								break;//退出，不能充电
							}
							Globa_2->User_Card_check_flag = 0;
							Globa_2->User_Card_check_result = 1;
						}
					}else if(Globa_2->checkout == 8){//余额不足
						msg.type = 0x200;//等待刷卡界面
						Page_200.state = 11;//卡里余额过低，停止充电
						Page_200.reserve1 = 0;				
						memcpy(&Page_200.sn[0], &Globa_2->card_sn[0], sizeof(Page_200.sn));
						sprintf(&temp_char[0], "%08d", Globa_2->last_rmb);	
						memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
						memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
						if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
							Voicechip_operation_function(0x0a);//0aH     “余额不足，请充值”
						}
						sleep(2);
						msg.type = 0x150;
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_2->QT_Step = 0x03; //自动充电分开的AB通道
						break;//退出，不能充电
					}else if(Globa_2->checkout == 9){
						msg.type = 0x200;//等待刷卡界面
						Page_200.state = 7;
						memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
						if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
							Voicechip_operation_function(0x0c);//0cH     “卡片已挂失，请联系发卡单位处理”
						}
						sleep(3);
						msg.type = 0x150;
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_2->QT_Step = 0x03; //自动充电分开的AB通道
						break;//退出，不能充电
					}else{
						Globa_2->checkout = 0;//清除刷卡结果标志
					}
				
					if((Globa_2->User_Card_check_result >= 2)&&(Globa_2->User_Card_check_result <= 14)){
						Globa_2->User_Card_check_flag = 0;
						msg.type = 0x200;//等待刷卡界面
						Page_200.state = 17 + (Globa_2->User_Card_check_result - 2 );//14;//MAC校验错误
						memcpy(&Page_200.sn[0], &Globa_2->card_sn[0], sizeof(Page_200.sn));
						sprintf(&temp_char[0], "%08d", Globa_2->last_rmb);	
						memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
						memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
						if(Globa_2->User_Card_check_result == 4){
							if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
								Voicechip_operation_function(0x0d);//0dH     “卡片未注册，请联系发卡单位处理”
							}
						}
						sleep(2);
						msg.type = 0x150;
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						Globa_2->QT_Step = 0x03; //自动充电分开的AB通道
						break;//退出，不能充电
					}
					else if(Globa_2->User_Card_check_result == 1){//验证成功请在次刷卡
						Globa_2->User_Card_check_flag = 0;
						msg.type = 0x200;//等待刷卡界面
						Page_200.state = 15;//验证成功请保持刷卡状态
						memcpy(&Page_200.sn[0], &busy_frame.card_sn[0], sizeof(Page_200.sn));
						memcpy(&Page_200.rmb[0], &busy_frame.last_rmb[0], sizeof(Page_200.rmb));
						memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
						
						if((Globa_2->checkout == 3)||(Globa_2->checkout == 1)){
							Globa_2->User_Card_check_flag = 0;
							Globa_2->User_Card_check_result = 0;
							if(Globa_2->checkout == 1){//卡片状态异常 灰锁
								Globa_2->card_type = 1; //卡类型 0:未知 1：用户卡 2：员工卡 3：维护卡卡类型 1：用户卡 2：员工卡
							}else{
								Globa_2->card_type = 2; //卡类型 0:未知 1：用户卡 2：员工卡 3：维护卡卡类型 1：用户卡 2：员工卡
							}
							Globa_2->pay_step = 0;//进入空闲状态
							msg.type = 0x200;//等待刷卡界面
							if(Globa_2->card_type == 1){
								Page_200.state = 5;
							}else{
								Page_200.state = 6;
							}
							if(Globa_2->qiyehao_flag){//为企业卡号
								Page_200.state = 17;
								Globa_2->card_type = 2;
							}
							memcpy(&Page_200.sn[0], &Globa_2->card_sn[0], sizeof(Page_200.sn));
							sprintf(&temp_char[0], "%08d", Globa_2->last_rmb);	
							memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
							memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
							msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
							sleep(1);
							
							if((Globa_2->private_price_acquireOK == 1)&&(Globa_2->private_price_flag == 1)){//专有电价
								for(i=0;i<4;i++){
									share_time_kwh_price[i] = Globa_2->Special_price_data.share_time_kwh_price[i];
									share_time_kwh_price_serve[i] = Globa_2->Special_price_data.share_time_kwh_price_serve[i]; //服务
								}
								Serv_Type = Globa_2->Special_price_data.Serv_Type;
							}else{//通用电价
								for(i=0;i<4;i++){
									share_time_kwh_price[i] = Globa_2->Charger_param2.share_time_kwh_price[i];
									share_time_kwh_price_serve[i] = Globa_2->Charger_param2.share_time_kwh_price_serve[i]; //服务
								}
								Serv_Type = Globa_2->Charger_param.Serv_Type;
							}
							memcpy(&busy_frame.card_sn[0], &Globa_2->card_sn[0], sizeof(busy_frame.card_sn));

							if(Globa_2->card_type == 1){
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

							if(Globa_2->card_type == 1){
								memcpy(&busy_frame.flag[0], "01", sizeof(busy_frame.flag));//用户卡标志灰锁
							}else{
								memcpy(&busy_frame.flag[0], "00", sizeof(busy_frame.flag));//员工卡直接标志位完成，不灰锁
							}

							sprintf(&temp_char[0], "%08d", Globa_2->last_rmb);		
							memcpy(&busy_frame.last_rmb[0], &temp_char[0], sizeof(busy_frame.last_rmb));

							memcpy(&busy_frame.ter_sn[0],  &Globa_2->Charger_param.SN[0], sizeof(busy_frame.ter_sn));

					
							Globa_2->meter_start_KWH = Globa_2->meter_KWH;//取初始电表示数
							Globa_2->meter_stop_KWH = Globa_2->meter_start_KWH;
						

							sprintf(&temp_char[0], "%08d", Globa_2->meter_start_KWH);    
							memcpy(&busy_frame.start_kwh[0], &temp_char[0], sizeof(busy_frame.start_kwh));
							
							sprintf(&temp_char[0], "%08d", Globa_2->meter_stop_KWH);				
							memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

							sprintf(&temp_char[0], "%08d", Globa_2->Charger_param.Price);    
							memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));

							sprintf(&temp_char[0], "%08d", Globa_2->kwh);				
							memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
							
							sprintf(&temp_char[0], "%08d", Globa_2->rmb);				
							memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));

							//起始总费用等于电量费用，暂时只做安电量收服务费
							sprintf(&temp_char[0], "%08d", Globa_2->total_rmb);
							memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));
							sprintf(&temp_char[0], "%08d", Globa_2->total_rmb-Globa_2->rmb);
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
							busy_frame.ConnectorId[1] = '2';
							
							busy_frame.End_code[0] = '1';//结束代码
							busy_frame.End_code[1] = '1';
							
							tmp_value =0;
							id = 0;
							pthread_mutex_lock(&busy_db_pmutex);
						  Insertflag = Insert_Charger_Busy_DB_2(&busy_frame, &id, 0x55);//写新数据库记录 无流水号 传输标志0x55不能上传
							pthread_mutex_unlock(&busy_db_pmutex);
							if(DEBUGLOG) RUN_GUN2_LogOut("%s %d","2号枪:刷卡启动充电并插入交易记录中的ID=",id);  
			        if(Insertflag == -1){
								Page_1300.cause = 63;
								Page_1300.Capact = Globa_2->BMS_batt_SOC;
								Page_1300.elapsed_time[0] =  Globa_2->Time;
								Page_1300.elapsed_time[1] =  Globa_2->Time>>8;
								Page_1300.KWH[0] =  Globa_2->kwh;
								Page_1300.KWH[1] =  Globa_2->kwh>>8;
								Page_1300.RMB[0] =  Globa_2->total_rmb;
						    Page_1300.RMB[1] =  Globa_2->total_rmb>>8;
						    Page_1300.RMB[2] =  Globa_2->total_rmb>>16;
						    Page_1300.RMB[3] =  Globa_2->total_rmb>>24;
						
								memcpy(&Page_1300.SN[0], &Globa_2->card_sn[0], sizeof(Page_1300.SN));
								Page_1300.APP = 1;   //充电启动方式 1:刷卡  2：手机APP

								memcpy(&msg.argv[0], &Page_1300.reserve1, sizeof(PAGE_1300_FRAME));
								msg.type = 0x1300;//充电结束结算界面
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_1300_FRAME), IPC_NOWAIT);

								Globa_2->QT_Step = 0x23; // 2      充电结束 结算界面 2.3----------------

								Globa_2->charger_state = 0x05;
								Globa_2->checkout = 0;
								Globa_2->pay_step = 4;//2通道在等待刷卡扣费
								Globa_2->Charger_Over_Rmb_flag = 0;
								break;
						  }
							//------------------------------------------------------------------
							if((Globa_2->private_price_acquireOK == 0)&&(Globa_2->private_price_flag == 1)){//提示用户无法享受优惠电价，
								msg.type = 0x200;//等待刷卡界面
								Page_200.state = 37;
								memcpy(&Page_200.sn[0], &Globa_2->card_sn[0], sizeof(Page_200.sn));
								sprintf(&temp_char[0], "%08d", Globa_2->last_rmb);	
								memcpy(&Page_200.rmb[0], &temp_char[0], sizeof(Page_200.rmb));
								memcpy(&msg.argv[0], &Page_200.reserve1, sizeof(PAGE_200_FRAME));
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_200_FRAME), IPC_NOWAIT);
								sleep(3);//时间可调整
							}

							if(Globa_2->Card_charger_type == 5){//倒计时界面
								memset(&msg.argv[0], 0x00, sizeof(PAGE_DAOJISHI_FRAME));
							  memcpy(&Page_daojishi.card_sn[0], &Globa_2->card_sn[0],sizeof(Globa_2->card_sn));
								Page_daojishi.type = 0;
								count_wait = 0;
							  time_t systime1;
								systime1 = time(NULL);        //获得系统的当前时间 
								int daojishi_time = Globa_2->QT_pre_time - systime1;
								daojishi_time  = (daojishi_time > 0)? daojishi_time:0;
								Page_daojishi.hour = daojishi_time/3600;		
								Page_daojishi.min  = (daojishi_time%3600)/60;
								Page_daojishi.second =(daojishi_time%3600)%60;
								msg.type = 0x1151;//倒计时界面
								memcpy(&msg.argv[0], &Page_daojishi.reserve1, sizeof(PAGE_DAOJISHI_FRAME));
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_DAOJISHI_FRAME), IPC_NOWAIT); 
			
								QT_timer_start(500);
								Globa_2->QT_Step = 0x201; // 1 自动充电-》倒计时界面  2.0.1
								
								Globa_2->charger_state = 0x06;
								Globa_2->checkout = 0;
								//Globa_2->pay_step = 2;//0：空闲 1：刷卡启动 2：刷卡停止预约 3：刷卡停止充电 4：刷卡结算
						    card_falg_count = 0;
							}else{
								if((Globa_2->Charger_param.System_Type == 1)&&(Globa_1->charger_start == 1)){//轮流充电的时候出现一把枪正在充电则另一把需要等待
									memset(&Page_daojishi.reserve1, 0x00, sizeof(PAGE_DAOJISHI_FRAME));
									memcpy(&Page_daojishi.card_sn[0], &Globa_2->card_sn[0],sizeof(Globa_2->card_sn));
									Page_daojishi.type = 1;
									memcpy(&msg.argv[0], &Page_daojishi.reserve1, sizeof(PAGE_DAOJISHI_FRAME));
									msg.type = 0x1151;//倒计时界面
									msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_DAOJISHI_FRAME), IPC_NOWAIT); 
									sleep(3);
									QT_timer_start(500);
									Globa_2->QT_Step = 0x201; // 1 自动充电-》倒计时界面  2.0.1
									Globa_2->charger_state = 0x06;
									Globa_2->checkout = 0;
									//Globa_2->pay_step = 2;//0：空闲 1：刷卡启动 2：刷卡停止预约 3：刷卡停止充电 4：刷卡结算
									count_wait = 0;
									card_falg_count = 0;
									break;
								}else{
									Globa_2->Manu_start = 1;
									Globa_2->charger_start = 1;
									if(Globa_2->qiyehao_flag){//为企业卡号
										Globa_2->Enterprise_Card_flag = 1;
									}else{
										Globa_2->Enterprise_Card_flag = 0;
									}
									msg.type = 0x1111;
									memset(&Page_1111.reserve1, 0x00, sizeof(PAGE_1111_FRAME));									
									Page_1111.APP  = 2;   //充电启动方式 1:刷卡  2：手机APP
									memcpy(&Page_1111.SN[0], &Globa_2->card_sn[0], sizeof(Page_1111.SN));
									memcpy(&msg.argv[0], &Page_1111.reserve1, sizeof(PAGE_1111_FRAME));
									msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_1111_FRAME), IPC_NOWAIT);
						
									QT_timer_start(500);
									Globa_2->QT_Step = 0x21;   // 2 自动充电-》 充电界面 状态 2.1-
									if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
										Voicechip_operation_function(0x05);  //05H     “系统自检中，请稍候”
									}
									Globa_2->charger_state = 0x04;
									Globa_2->checkout = 0;
									//Globa_2->pay_step = 3;//0：空闲 1：刷卡启动 2：刷卡停止预约 3：刷卡停止充电 4：刷卡结算
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
							break;
						}
					}
					break;
				}
				case 0x201:{//--------------2 自动充电-》 倒计时界面  2.0.1---------------
					count_wait++;
					card_falg_count ++;
					if (card_falg_count > 20)
					{
					    Globa_2->pay_step = 2; //允许刷卡 --刷卡停止预约
					}
					if(Globa_2->checkout != 0){//1通道检测到充电卡 0：未知 1：灰锁 2：补充交易 3：卡片正常
						msg.type = 0x95;//解除屏保
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);

						if(Globa_2->checkout == 1){//交易处理 灰锁
							//不动作，继续等待刷卡
						}else if(Globa_2->checkout == 2){//交易处理 补充交易
							memcpy(&busy_frame.flag[0], "02", sizeof(busy_frame.flag));//"00" 补充交易
							pthread_mutex_lock(&busy_db_pmutex);
							if(Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00) == -1){
								usleep(10000);
								Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00);//更新数据 无流水号 传输标志0x55 不能上传
							}					
							pthread_mutex_unlock(&busy_db_pmutex);
						}else if(Globa_2->checkout == 3){//交易处理 正常
							memcpy(&busy_frame.flag[0], "00", sizeof(busy_frame.flag));//"00" 成功
							pthread_mutex_lock(&busy_db_pmutex);
							if(Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00) == -1){
								usleep(10000);
								Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00);//更新数据 无流水号 传输标志0x55 不能上传
							}
							pthread_mutex_unlock(&busy_db_pmutex);

							//------------结算终于结束，退出结算界面--------------------------
							if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
								Voicechip_operation_function(0x09);//09H     “谢谢惠顾，再见”			
							}
							msg.type = 0x150;
							msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);				

							Globa_2->QT_Step = 0x03; //自动充电分开的AB通道
						}
						
						Globa_2->checkout = 0;
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
							if((Globa_2->Charger_param.System_Type == 1)&&(Globa_1->charger_start ==0)&&(!((Globa_1->QT_Step == 0x21)||(Globa_1->QT_Step == 0x22)))){//轮流充电的时候出现一把枪正在充电则另一把需要等待
								sleep(3);
								Globa_2->meter_start_KWH = Globa_2->meter_KWH;//取初始电表示数
								Globa_2->meter_stop_KWH = Globa_2->meter_start_KWH;
								sprintf(&temp_char[0], "%08d", Globa_2->meter_start_KWH);    
								memcpy(&busy_frame.start_kwh[0], &temp_char[0], sizeof(busy_frame.start_kwh));
								sprintf(&temp_char[0], "%08d", Globa_2->meter_stop_KWH);				
								memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));
								
								charger_type = 1;
								Globa_2->Card_charger_type = 4;
								Globa_2->Manu_start = 1;
								Globa_2->charger_start = 1;
								if(Globa_2->qiyehao_flag){//为企业卡号
									Globa_2->Enterprise_Card_flag = 1;
								}else{
									Globa_2->Enterprise_Card_flag = 0;
								}
								msg.type = 0x1111;
								memset(&msg.argv[0], 0x00, sizeof(PAGE_1111_FRAME));
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_1111_FRAME), IPC_NOWAIT);
								QT_timer_start(500);
								Globa_2->QT_Step = 0x21;   // 2 自动充电-》 充电界面 状态 2.1-
								if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
									Voicechip_operation_function(0x05);  //05H     “系统自检中，请稍候”
								}
								Globa_2->charger_state = 0x04;
								Globa_2->checkout = 0;
								Globa_2->pay_step = 3;
								count_wait = 0;
								count_wait1 = 0;
								meter_Curr_Threshold_count = Current_Systime;
								current_low_timestamp = Current_Systime;
								check_current_low_st = 0;
								Electrical_abnormality = Current_Systime;
								current_overkwh_timestamp = Current_Systime;
								break;
							}
						}else{
					   // if((Page_daojishi.hour <= 0)&&(Page_daojishi.min <= 0)&&(Page_daojishi.second <= 0)){//(tm_t.tm_min + tm_t.tm_hour*60) >= Globa_1->QT_pre_time){
							if(Globa_2->QT_pre_time <= systime){
								if((Globa_1->Charger_param.System_Type == 1)&&((Globa_1->charger_start ==1)||(Globa_1->App.APP_start ==1))){//轮流充电的时候出现一把枪正在充电则另一把需要等待
									Page_daojishi.type = 1;
									memcpy(&Page_daojishi.card_sn[0], &Globa_2->card_sn[0],sizeof(Globa_2->card_sn));
									memcpy(&msg.argv[0], &Page_daojishi.reserve1, sizeof(PAGE_DAOJISHI_FRAME));
									msg.type = 0x1151;//倒计时界面
									msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_DAOJISHI_FRAME), IPC_NOWAIT);
								}else{
									Globa_2->meter_start_KWH = Globa_2->meter_KWH;//取初始电表示数
									Globa_2->meter_stop_KWH = Globa_2->meter_start_KWH;
									sprintf(&temp_char[0], "%08d", Globa_2->meter_start_KWH);    
									memcpy(&busy_frame.start_kwh[0], &temp_char[0], sizeof(busy_frame.start_kwh));
									sprintf(&temp_char[0], "%08d", Globa_2->meter_stop_KWH);				
									memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));
									
									charger_type = 1;
									Globa_2->Card_charger_type = 4;
									Globa_2->Manu_start = 1;
									Globa_2->charger_start = 1;
				
									msg.type = 0x1111;
									memset(&msg.argv[0], 0x00, sizeof(PAGE_1111_FRAME));
									msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_1111_FRAME), IPC_NOWAIT);
				
									QT_timer_start(500);
									Globa_2->QT_Step = 0x21;   // 2 自动充电-》 充电界面 状态 2.1-
									Globa_2->charger_state = 0x04;
									Globa_2->checkout = 0;
									Globa_2->pay_step = 3;	
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
								//int daojishi_time = (Globa_2->QT_pre_time*60 -(tm_t.tm_sec + tm_t.tm_min*60 + tm_t.tm_hour*3600));
								int daojishi_time = Globa_2->QT_pre_time - systime;
								daojishi_time  = (daojishi_time > 0)? daojishi_time:0;
								Page_daojishi.hour = daojishi_time/3600;		
								Page_daojishi.min  = (daojishi_time%3600)/60;
								Page_daojishi.second =(daojishi_time%3600)%60;
								memcpy(&msg.argv[0], &Page_daojishi.reserve1, sizeof(PAGE_DAOJISHI_FRAME));

								msg.type = 0x1151;//倒计时界面
								msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_DAOJISHI_FRAME), IPC_NOWAIT);

								break;
							}
						}
					}
					break;
				}
				case 0x21:{//---------------2 自动充电-》 自动充电界面 状态  2.1----------
          card_falg_count ++;
				  if((card_falg_count > 20)&&(Globa_2->Start_Mode != VIN_START_TYPE))
					{
					    Globa_2->pay_step = 3; //允许刷卡 --刷卡停止充电
					}
					
					//Globa_2->meter_stop_KWH = Globa_2->meter_KWH;//取初始电表示数

					if(Globa_2->meter_KWH >= Globa_2->meter_stop_KWH)
					{
						Globa_2->meter_stop_KWH = Globa_2->meter_KWH;//取初始电表示数
						Electrical_abnormality = Current_Systime;
					}else{//连续3s获取到的数据都比较起始数据小，则直接停止充电
						if(abs(Current_Systime - Electrical_abnormality) > METER_ELECTRICAL_ERRO_COUNT)//超过1分钟了，需停止充电
						{
              Electrical_abnormality = Current_Systime;
							Globa_2->Manu_start = 0;
						  Globa_2->Charger_Over_Rmb_flag = 7;//读取电量有异常
							RunEventLogOut_2("计费QT判断读取电量有异常：Globa_2->meter_KWH=%d Globa_2->meter_stop_KWH=%d",Globa_2->meter_KWH,Globa_2->meter_stop_KWH);
						}
					}
					
					Globa_2->kwh = (Globa_2->meter_stop_KWH - Globa_2->meter_start_KWH);

					g_ulcharged_kwh[pre_price_index] = Globa_2->kwh - ul_start_kwh;//累加当前时段的耗电量	
					cur_price_index = GetCurPriceIndex(); //获取当前时间
					if(cur_price_index != pre_price_index){
						ul_start_kwh = Globa_2->kwh - g_ulcharged_kwh[cur_price_index];//load new start kwh,需要减去时段之前累加的电量，否则该时段之前累加的电量丢失----20160312
						pre_price_index = cur_price_index;							
					}	
					for(i = 0;i<4;i++){
						g_ulcharged_rmb[i] = g_ulcharged_kwh[i]*share_time_kwh_price[i];
					}
				
					for(i = 0;i<4;i++){
						g_ulcharged_rmb[i] = (g_ulcharged_rmb[i] + COMPENSATION_VALUE)/100;
					}
					
					Globa_2->rmb = g_ulcharged_rmb[0] + g_ulcharged_rmb[1] + g_ulcharged_rmb[2] + g_ulcharged_rmb[3]; 

					if(Globa_2->Charger_param.Serv_Type == 1){ //按次数收
						Globa_2->total_rmb = (Globa_2->rmb + Globa_2->Charger_param.Serv_Price);//已充电电量对应的金额
					}else if(Globa_2->Charger_param.Serv_Type == 2){//按时间收
						Globa_2->total_rmb = (Globa_2->rmb + Globa_2->Time*Globa_2->Charger_param.Serv_Price/600);//已充电电量对应的金额
					}else{
					//	Globa_2->total_rmb = (Globa_2->rmb + Globa_2->kwh*Globa_2->Charger_param.Serv_Price/100);//已充电电量对应的金额
						for(i = 0;i<4;i++){
							g_ulcharged_rmb_Serv[i] = g_ulcharged_kwh[i]*share_time_kwh_price_serve[i];
						}
						for(i = 0;i<4;i++){
							g_ulcharged_rmb_Serv[i] =( g_ulcharged_rmb_Serv[i] + COMPENSATION_VALUE)/100;
						}
						Globa_2->total_rmb = Globa_2->rmb + g_ulcharged_rmb_Serv[0] + g_ulcharged_rmb_Serv[1] + g_ulcharged_rmb_Serv[2] + g_ulcharged_rmb_Serv[3]; 
					}
					
					Globa_2->ISO_8583_rmb_Serv = (Globa_2->total_rmb - Globa_2->rmb);//总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
					Globa_2->ISO_8583_rmb = Globa_2->rmb;                     //电量消费金额 0.01yuan
					
					//充电过程中实时更新业务数据记录;
					//若意外掉电，则上电时判断最后一条数据记录的“上传标示”是否为“0x55”;
					//若是，以此时电表示数作为上次充电结束时的示数;
					if((Globa_2->kwh - temp_kwh) > 50){//每隔0.5度电左右保存一次数据;
						temp_kwh = Globa_2->kwh;
						sprintf(&temp_char[0], "%08d", Globa_2->meter_stop_KWH);				
						memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

						sprintf(&temp_char[0], "%08d", Globa_2->kwh);				
						memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
						
						sprintf(&temp_char[0], "%08d", Globa_2->rmb);				
						memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));
						if(Globa_2->kwh != 0){
							sprintf(&temp_char[0], "%08d", (Globa_2->rmb*100/Globa_2->kwh));  
						}else{
							sprintf(&temp_char[0], "%08d", 0);  
						}
						memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));
						
						sprintf(&temp_char[0], "%08d", Globa_2->total_rmb);				
						memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));

						sprintf(&temp_char[0], "%08d", Globa_2->total_rmb-Globa_2->rmb);				
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
						if(Update_Charger_Busy_DB_2(id, &busy_frame, 1, 0x55) == -1){
						 sleep(1);
						 Update_Charger_Busy_DB_2(id, &busy_frame, 1, 0x55);//更新数据 无流水号 传输标志0x55 不能上传
						}
						pthread_mutex_unlock(&busy_db_pmutex);
					}

					//------------------ 刷卡停止充电 --------------------------------------
					if(((Globa_2->checkout == 3)&&(Globa_2->card_type == 2))|| \
						 ((Globa_2->checkout == 1)&&(Globa_2->card_type == 1))||(Globa_2->Electric_pile_workstart_Enable == 1)||((Globa_2->Start_Mode == VIN_START_TYPE)&&(msg.argv[0] == 1)&&(msg.type == 0x1111))){//用户卡灰锁 或正常员工卡
				  	RunEventLogOut_2("计费QT判断用户主动停止充电");

						Globa_2->Manu_start = 0;
            sleep(2);
						Globa_2->meter_stop_KWH = (Globa_2->meter_KWH >= Globa_2->meter_stop_KWH)? Globa_2->meter_KWH:Globa_2->meter_stop_KWH;//取初始电表示数
					  Globa_2->kwh = (Globa_2->meter_stop_KWH - Globa_2->meter_start_KWH);

						g_ulcharged_kwh[pre_price_index] = Globa_2->kwh - ul_start_kwh;//累加当前时段的耗电量	
						cur_price_index = GetCurPriceIndex(); //获取当前时间
						if(cur_price_index != pre_price_index){
							ul_start_kwh = Globa_2->kwh - g_ulcharged_kwh[cur_price_index];//load new start kwh,需要减去时段之前累加的电量，否则该时段之前累加的电量丢失----20160312
							pre_price_index = cur_price_index;							
						}	
						for(i = 0;i<4;i++){
							g_ulcharged_rmb[i] = g_ulcharged_kwh[i]*share_time_kwh_price[i];
						}
						
					for(i = 0;i<4;i++){
						g_ulcharged_rmb[i] = (g_ulcharged_rmb[i] + COMPENSATION_VALUE)/100;
					}
					
					Globa_2->rmb = g_ulcharged_rmb[0] + g_ulcharged_rmb[1] + g_ulcharged_rmb[2] + g_ulcharged_rmb[3]; 

					if(Globa_2->Charger_param.Serv_Type == 1){ //按次数收
						Globa_2->total_rmb = (Globa_2->rmb + Globa_2->Charger_param.Serv_Price);//已充电电量对应的金额
					}else if(Globa_2->Charger_param.Serv_Type == 2){//按时间收
						Globa_2->total_rmb = (Globa_2->rmb + Globa_2->Time*Globa_2->Charger_param.Serv_Price/600);//已充电电量对应的金额
					}else{
						//Globa_2->total_rmb = (Globa_2->rmb + Globa_2->kwh*Globa_2->Charger_param.Serv_Price/100);//已充电电量对应的金额
						for(i = 0;i<4;i++){
							g_ulcharged_rmb_Serv[i] = g_ulcharged_kwh[i]*share_time_kwh_price_serve[i];
						}
						for(i = 0;i<4;i++){
							g_ulcharged_rmb_Serv[i] =( g_ulcharged_rmb_Serv[i] + COMPENSATION_VALUE)/100;
						}
						Globa_2->total_rmb = Globa_2->rmb + g_ulcharged_rmb_Serv[0] + g_ulcharged_rmb_Serv[1] + g_ulcharged_rmb_Serv[2] + g_ulcharged_rmb_Serv[3]; 
					}
				    
						Globa_2->ISO_8583_rmb_Serv = Globa_2->total_rmb - Globa_2->rmb ;               //总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
						Globa_2->ISO_8583_rmb = Globa_2->rmb;                     //电量消费金额 0.01yuan
						//-------------------扣费 更新业务进度数据----------------------------
						sprintf(&temp_char[0], "%08d", Globa_2->meter_stop_KWH);				
						memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

						sprintf(&temp_char[0], "%08d", Globa_2->kwh);				
						memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
						
						sprintf(&temp_char[0], "%08d", Globa_2->rmb);				
						memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));
						if(Globa_2->kwh != 0){
							sprintf(&temp_char[0], "%08d", (Globa_2->rmb*100/Globa_2->kwh));  
						}else{
							sprintf(&temp_char[0], "%08d", 0);  
						}
						memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));
						
						sprintf(&temp_char[0], "%08d", Globa_2->total_rmb);				
						memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));
						
						printf("QT MT625_Pay_Delete= %d \r\n",Globa_2->last_rmb,Globa_2->total_rmb);

						sprintf(&temp_char[0], "%08d", Globa_2->total_rmb-Globa_2->rmb);				
						memcpy(&busy_frame.server_rmb[0], &temp_char[0], sizeof(busy_frame.server_rmb));
						printf("QT MT625_Pay_Delete= %d \r\n",Globa_2->last_rmb,Globa_2->total_rmb);

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
						
						pthread_mutex_lock(&busy_db_pmutex);
						if(Update_Charger_Busy_DB_2(id, &busy_frame, 1, 0x66) == -1){
              sleep(1);
							Update_Charger_Busy_DB_2(id, &busy_frame, 1, 0x66);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
						}
						pthread_mutex_unlock(&busy_db_pmutex);
				
						/*if(i >= 5){
							i = 0;
							sleep(1);
							pthread_mutex_lock(&busy_db_pmutex);
							UINT32 ls_Kwh =0;
							UINT32 ls_total_rmb = 0;
							if(Select_Busy_ID_Data(id,&ls_Kwh,&ls_total_rmb,&Globa_2->card_sn[0]) != -1){
								if(DEBUGLOG) RUN_GUN2_LogOut("%s %d ls_Kwh = %d ls_total_rmb = %d  Globa_2->kwh = %d Globa_2->total_rmb %d card_sn = %.16s","2号枪交易数据查询该ID下数据库存储中的数据，从而使其进行同步 ID=",id,ls_Kwh,ls_total_rmb,Globa_2->kwh,Globa_2->total_rmb,Globa_2->card_sn);  
								if((ls_Kwh != 0 )&&(ls_total_rmb !=0)){
									Globa_2->kwh = ls_Kwh;
									Globa_2->total_rmb = ls_total_rmb;
								}
							}
							pthread_mutex_unlock(&busy_db_pmutex);
						}*/

						//-------------------界面显示数据内容---------------------------------
						Page_1300.cause = 11;//直接手动结束的 Globa_2->charger_over_flag;
						Page_1300.Capact = Globa_2->BMS_batt_SOC;
						Page_1300.elapsed_time[0] =  Globa_2->Time;
						Page_1300.elapsed_time[1] =  Globa_2->Time>>8;
						Page_1300.KWH[0] =  Globa_2->kwh;
						Page_1300.KWH[1] =  Globa_2->kwh>>8;
						Page_1300.RMB[0] =  Globa_2->total_rmb;
						Page_1300.RMB[1] =  Globa_2->total_rmb>>8;
						Page_1300.RMB[2] =  Globa_2->total_rmb>>16;
						Page_1300.RMB[3] =  Globa_2->total_rmb>>24;
				
						memcpy(&Page_1300.SN[0], &Globa_2->card_sn[0], sizeof(Page_1300.SN));
						if(Globa_2->Start_Mode == VIN_START_TYPE)
						{	
							// Page_1300.APP = 3;   //充电启动方式 1:刷卡  2：手机APP
							 	#ifdef CHECK_BUTTON	
								Globa_2->pay_step = 0;
								Page_1300.APP = 3;   //充电启动方式 1:刷卡  2：手机APP
							#else 
								Globa_2->pay_step = 0;
								Page_1300.APP = 1;   //充电启动方式 1:刷卡  2：手机APP
							#endif
						}else{
							Page_1300.APP = 1;   //充电启动方式 1:刷卡  2：手机APP 
						}
						memcpy(&msg.argv[0], &Page_1300.reserve1, sizeof(PAGE_1300_FRAME));
						msg.type = 0x1300;//充电结束界面
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_1300_FRAME), IPC_NOWAIT);

						Globa_2->QT_Step = 0x23; // 2      充电结束 扣费界面 2.3----------------
						
						Globa_2->charger_state = 0x05;
						Globa_2->Charger_Over_Rmb_flag = 0;
					
						count_wait = 0;
						count_wait1 = 0;
						Globa_2->checkout = 0;
						if(Globa_2->Start_Mode == VIN_START_TYPE)
						{
						  Globa_2->pay_step = 0;
						}
						else
						{
							Globa_2->pay_step = 4;//1通道在等待刷卡扣费
						}
						break;
					}else if(Globa_2->checkout != 0){//无用的检测结果清零
						Globa_2->checkout = 0;
					}

					if(Globa_2->charger_start == 0){//充电彻底结束处理
						//-------------------扣费 更新业务进度数据----------------------------
						sleep(3);
						Globa_2->meter_stop_KWH = (Globa_2->meter_KWH >= Globa_2->meter_stop_KWH)? Globa_2->meter_KWH:Globa_2->meter_stop_KWH;//取初始电表示数
					  Globa_2->kwh = (Globa_2->meter_stop_KWH - Globa_2->meter_start_KWH);

						g_ulcharged_kwh[pre_price_index] = Globa_2->kwh - ul_start_kwh;//累加当前时段的耗电量	
						cur_price_index = GetCurPriceIndex(); //获取当前时间
						if(cur_price_index != pre_price_index){
							ul_start_kwh = Globa_2->kwh - g_ulcharged_kwh[cur_price_index];//load new start kwh,需要减去时段之前累加的电量，否则该时段之前累加的电量丢失----20160312
							pre_price_index = cur_price_index;							
						}	
						for(i = 0;i<4;i++){
							g_ulcharged_rmb[i] = g_ulcharged_kwh[i]*share_time_kwh_price[i];
						}
						
						for(i = 0;i<4;i++){
							g_ulcharged_rmb[i] = (g_ulcharged_rmb[i] + COMPENSATION_VALUE)/100;
						}
						
						Globa_2->rmb = g_ulcharged_rmb[0] + g_ulcharged_rmb[1] + g_ulcharged_rmb[2] + g_ulcharged_rmb[3]; 

						if(Globa_2->Charger_param.Serv_Type == 1){ //按次数收
							Globa_2->total_rmb = (Globa_2->rmb + Globa_2->Charger_param.Serv_Price);//已充电电量对应的金额
						}else if(Globa_2->Charger_param.Serv_Type == 2){//按时间收
							Globa_2->total_rmb = (Globa_2->rmb + Globa_2->Time*Globa_2->Charger_param.Serv_Price/600);//已充电电量对应的金额
						}else{
							//Globa_2->total_rmb = (Globa_2->rmb + Globa_2->kwh*Globa_2->Charger_param.Serv_Price/100);//已充电电量对应的金额
							for(i = 0;i<4;i++){
								g_ulcharged_rmb_Serv[i] = g_ulcharged_kwh[i]*share_time_kwh_price_serve[i];
							}
							for(i = 0;i<4;i++){
								g_ulcharged_rmb_Serv[i] =( g_ulcharged_rmb_Serv[i] + COMPENSATION_VALUE)/100;
							}
							Globa_2->total_rmb = Globa_2->rmb + g_ulcharged_rmb_Serv[0] + g_ulcharged_rmb_Serv[1] + g_ulcharged_rmb_Serv[2] + g_ulcharged_rmb_Serv[3]; 
						}
		        
				    Globa_2->ISO_8583_rmb_Serv = Globa_2->total_rmb - Globa_2->rmb ;               //总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
						Globa_2->ISO_8583_rmb = Globa_2->rmb;                     //电量消费金额 0.01yuan
						
						sprintf(&temp_char[0], "%08d", Globa_2->meter_stop_KWH);				
						memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

						sprintf(&temp_char[0], "%08d", Globa_2->kwh);				
						memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
						
						sprintf(&temp_char[0], "%08d", Globa_2->rmb);				
						memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));
						if(Globa_2->kwh != 0){
							sprintf(&temp_char[0], "%08d", (Globa_2->rmb*100/Globa_2->kwh));  
						}else{
							sprintf(&temp_char[0], "%08d", 0);  
						}
						memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));
						sprintf(&temp_char[0], "%08d", Globa_2->total_rmb);				
						memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));

						sprintf(&temp_char[0], "%08d", Globa_2->total_rmb-Globa_2->rmb);				
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
						
						sprintf(&temp_char[0], "%.17s",Globa_2->VIN);//服务费收费类型 01：按次收费；02：按时间收费(单位：分/10分钟；03：按电量	9	N	N2
						//memcpy(&busy_frame.car_sn[0], &Globa_2->VIN[0], sizeof(busy_frame.car_sn));//
						memcpy(&busy_frame.car_sn[0], &temp_char[0], sizeof(busy_frame.car_sn));

						tmp_value =0;
						
						if(Globa_2->Charger_Over_Rmb_flag == 1){
							Page_1300.cause = 13; 
						  sent_warning_message(0x94, Page_1300.cause, 2, 0);
						}else if(Globa_2->Charger_Over_Rmb_flag == 3){
						  Page_1300.cause = 19; 
						  sent_warning_message(0x94, Page_1300.cause, 2, 0);
						}else if(Globa_2->Charger_Over_Rmb_flag == 4){
						  Page_1300.cause = 62; 
							sent_warning_message(0x94, Page_1300.cause, 2, 0);
						}else if(Globa_2->Charger_Over_Rmb_flag == 5){
						  Page_1300.cause = 65;
						  sent_warning_message(0x94, Page_1300.cause, 2, 0);							
						}else if(Globa_2->Charger_Over_Rmb_flag == 6){//达到SOC限制条件
						  Page_1300.cause = 9;
						  sent_warning_message(0x94, Page_1300.cause, 2, 0);							
					  }else if(Globa_2->Charger_Over_Rmb_flag == 7){//读取到的电表电量异常
						  Page_1300.cause = 8;
						  sent_warning_message(0x94, Page_1300.cause, 2, 0);							
					  }else if(Globa_2->Charger_Over_Rmb_flag == 8){//充电电流超过SOC能允许充电的值
							Page_1300.cause = 7;
							sent_warning_message(0x94, Page_1300.cause, 2, 0);							
						}else{
							Page_1300.cause = Globa_2->charger_over_flag; 
					  }
						RunEventLogOut_2("计费QT判断充电结束:Page_1300.cause =%d,Globa_2->charger_over_flag =%d ,Charger_Over_Rmb_flag =%d ",Page_1300.cause,Globa_2->charger_over_flag,Globa_2->Charger_Over_Rmb_flag);

					  sprintf(&temp_char[0], "%02d", Page_1300.cause);		
						memcpy(&busy_frame.End_code[0], &temp_char[0], sizeof(busy_frame.End_code));//结束原因代码
						
						pthread_mutex_lock(&busy_db_pmutex);
						if(Update_Charger_Busy_DB_2(id, &busy_frame, 1, 0x66) == -1){
							sleep(1);
							Update_Charger_Busy_DB_2(id, &busy_frame, 1, 0x66);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
						}
						pthread_mutex_unlock(&busy_db_pmutex);
					
						//Page_1300.cause = Globa_2->charger_over_flag;
						Page_1300.Capact = Globa_2->BMS_batt_SOC;
						Page_1300.elapsed_time[0] =  Globa_2->Time;
						Page_1300.elapsed_time[1] =  Globa_2->Time>>8;
						Page_1300.KWH[0] =  Globa_2->kwh;
						Page_1300.KWH[1] =  Globa_2->kwh>>8;
						Page_1300.RMB[0] =  Globa_2->total_rmb;
						Page_1300.RMB[1] =  Globa_2->total_rmb>>8;
						Page_1300.RMB[2] =  Globa_2->total_rmb>>16;
						Page_1300.RMB[3] =  Globa_2->total_rmb>>24;
						
						memcpy(&Page_1300.SN[0], &Globa_2->card_sn[0], sizeof(Page_1300.SN));
						
            if(Globa_2->Start_Mode == VIN_START_TYPE)
						{
							#ifdef CHECK_BUTTON	
								Globa_2->pay_step = 0;
								Page_1300.APP = 3;   //充电启动方式 1:刷卡  2：手机APP
							#else 
								Globa_2->pay_step = 0;
								Page_1300.APP = 1;   //充电启动方式 1:刷卡  2：手机APP
							#endif
						}
						else
						{
							Globa_2->pay_step = 4;//1通道在等待刷卡扣费
							Page_1300.APP = 1;   //充电启动方式 1:刷卡  2：手机APP
							if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
								Voicechip_operation_function(0x07);//07H     “充电已完成，请刷卡结算”
							}else{
								Voicechip_operation_function(0x10);//10H	请刷卡结算.wav
							}
						}	
						memcpy(&msg.argv[0], &Page_1300.reserve1, sizeof(PAGE_1300_FRAME));
						msg.type = 0x1300;//充电结束结算界面
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_1300_FRAME), IPC_NOWAIT);

						Globa_2->QT_Step = 0x23; // 2      充电结束 结算界面 2.3----------------

						Globa_2->charger_state = 0x05;
						Globa_2->checkout = 0;
						Globa_2->Charger_Over_Rmb_flag = 0;
						count_wait = 0;
						count_wait1 = 0;
						break;
					}
					
					if(Globa_2->Start_Mode != VIN_START_TYPE)
				  {
						if((Globa_2->qiyehao_flag == 0)&&(Globa_2->card_type == 1)&&((Globa_2->total_rmb + 50) >= Globa_2->last_rmb)){//用户卡余额不足,企业号不进行余额判断
							Globa_2->Manu_start = 0;
							Globa_2->Charger_Over_Rmb_flag = 1;
						}
						
						if((Globa_2->BMS_Info_falg == 1)&&(Bind_BmsVin_falg == 0)&&(Globa_1->have_hart == 1)){
							Bind_BmsVin_falg = 1; 
							if((0 != memcmp(&Globa_2->Bind_BmsVin[0], "00000000000000000", sizeof(Globa_2->Bind_BmsVin)))&&(0 != memcmp(&Globa_2->VIN[0], "00000000000000000", sizeof(Globa_2->VIN)))&&(!((Globa_2->VIN[0] == 0xFF)&&(Globa_2->VIN[1] == 0xFF)&&(Globa_2->VIN[2] == 0xFF)))){
								if(0 != memcmp(&Globa_2->Bind_BmsVin[0],&Globa_2->VIN[0], sizeof(Globa_2->Bind_BmsVin))){
									Globa_2->Manu_start = 0;
									Globa_2->Charger_Over_Rmb_flag = 4;//与绑定的VIN不一样
								}
							}
						}else if(Globa_2->BMS_Info_falg == 0){
							Bind_BmsVin_falg = 0;
						}
					}
					
					if(Globa_2->order_chg_flag == 1){//有序充电时，进行试驾判断，当时间大于
						if(ISorder_chg_control_2()){
							Globa_2->Manu_start = 0;
							Globa_2->Charger_Over_Rmb_flag = 3;//有序充电时间到
						}
					}
					
	 			

					if(Globa_2->Time >=  120)//120秒，充电启动2分种之后对电流判断
					{
						  if(Globa_2->Charger_param.CurrentVoltageJudgmentFalg == 1)
							{ 						
								if(((abs(Globa_2->Output_current - Globa_2->meter_Current_A)*100 > METER_CURR_THRESHOLD_VALUE*Globa_2->Output_current)&&(Globa_2->Output_current >= LIMIT_CURRENT_VALUE))||
									((abs(Globa_2->Output_current - Globa_2->meter_Current_A) > 1000)&&(Globa_2->Output_current < LIMIT_CURRENT_VALUE)&&(Globa_2->Output_current >= MIN_LIMIT_CURRENT_VALUE)))
								{
									if(abs(Current_Systime - meter_Curr_Threshold_count) > METER_CURR_COUNT)//超过1分钟了，需停止充电
									{
										Globa_2->Manu_start = 0;
										Globa_2->Charger_Over_Rmb_flag = 5;//电流计量有误差
										meter_Curr_Threshold_count = Current_Systime;
										RunEventLogOut_2("计费QT判断计量有误差 Globa_2->meter_Current_A =%d Globa_2->Output_current=%d",Globa_2->meter_Current_A,Globa_2->Output_current);
									}
								}
								else
								{
									meter_Curr_Threshold_count = Current_Systime;
								}
							}
						
						
						if(0 == check_current_low_st)
						{
							if(Globa_2->BMS_batt_SOC >= Globa_2->Charger_param.SOC_limit   )//
							{						
								if(Globa_2->Output_current < (Globa_2->Charger_param.min_current*1000))//电流低于限值
								{
									current_low_timestamp = Current_Systime;
									check_current_low_st = 1;
								}
							}	
						}else//电流低持续是否达到1分钟
						{
							if(Globa_2->Output_current > (Globa_2->Charger_param.min_current*1000 + 1000))//1A回差
							{
								check_current_low_st = 0;//clear						
							}
							
							if(Globa_2->BMS_batt_SOC < Globa_2->Charger_param.SOC_limit   )
								check_current_low_st = 0;//clear
							
							if(Globa_2->Output_current < (Globa_2->Charger_param.min_current*1000))
							{
								if(abs(Current_Systime - current_low_timestamp) > SOCLIMIT_CURR_COUNT)//超过1分钟了，需停止充电
								{
									Globa_2->Manu_start = 0;	
							  	Globa_2->Charger_Over_Rmb_flag = 6;//达到SOC限制条件
									RunEventLogOut_2("计费QT判断达到soc限制条件");
                  current_low_timestamp = Current_Systime;
								}
							}				
						}
						
					if((Globa_2->BMS_Info_falg == 1)&&(Globa_2->Battery_Rate_KWh > 200))//收到BMs额定信息,额定电度需要大于20kwh才进行判断
						{
							if(Globa_2->kwh >= ((100 - Globa_2->BMS_batt_Start_SOC + MAX_LIMIT_OVER_RATEKWH)*Globa_2->Battery_Rate_KWh/10))
							{
								if(abs(Current_Systime - current_overkwh_timestamp) > OVER_RATEKWH_COUNT)//超过1分钟了，需停止充电
								{
									Globa_2->Manu_start = 0;	
									Globa_2->Charger_Over_Rmb_flag = 8;//达到SOC限制条件
									RunEventLogOut_2("计费QT判断达到根据总电量和起始SOC算出需要的总电量：Battery_Rate_KWh =%d.%d  充电电量 Globa_2->kwh = %d.%d ：startsoc=%d endsoc=%d",Globa_2->Battery_Rate_KWh/10,Globa_2->Battery_Rate_KWh%10, Globa_2->kwh/100,Globa_2->kwh%100,Globa_2->BMS_batt_Start_SOC,Globa_2->BMS_batt_SOC);

									current_overkwh_timestamp = Current_Systime;
								}
							}else{
								current_overkwh_timestamp = Current_Systime;
							}
						}else{
							current_overkwh_timestamp = Current_Systime;
						}
					}
					
					if(Globa_2->Card_charger_type == 3){//按金额充电
						if((Globa_2->total_rmb + 50) >= Globa_2->QT_charge_money*100){
							Globa_2->Manu_start = 0;
						}
					}else if(Globa_2->Card_charger_type == 1){//按电量充电
						if((Globa_2->kwh/100) >= Globa_2->QT_charge_kwh){
							Globa_2->Manu_start = 0;
						}
					}else if(Globa_2->Card_charger_type == 2){//按时间充电
						if(Globa_2->user_sys_time >= Globa_2->QT_charge_time){
							Globa_2->Manu_start = 0;
						}
					}

					count_wait++;
					if((count_wait %5  == 0)&&(count_wait1 == 0)){//等待时间超时500mS
						count_wait1 =1;
						Page_1111.BATT_type = Globa_2->BATT_type;		
						Page_1111.BATT_V[0] = Globa_2->Output_voltage/100;
						Page_1111.BATT_V[1] = (Globa_2->Output_voltage/100)>>8;
						Page_1111.BATT_C[0] = Globa_2->Output_current/100;
						Page_1111.BATT_C[1] = (Globa_2->Output_current/100)>>8;
						Page_1111.KWH[0] =  Globa_2->kwh;
						Page_1111.KWH[1] =  Globa_2->kwh>>8;

						Page_1111.Capact =  Globa_2->BMS_batt_SOC;
						Page_1111.run    = (Globa_2->Output_current > 1000)?1:0;//有充电电流 》1A

						Page_1111.elapsed_time[0] = Globa_2->Time;
						Page_1111.elapsed_time[1] = Globa_2->Time>>8;
						Page_1111.need_time[0] = Globa_2->BMS_need_time;
						Page_1111.need_time[1] = Globa_2->BMS_need_time>>8;
						Page_1111.cell_HV[0] =  Globa_2->BMS_cell_HV;
						Page_1111.cell_HV[1] =  Globa_2->BMS_cell_HV>>8;
						Page_1111.money[0] =  Globa_2->total_rmb;
						Page_1111.money[1] =  Globa_2->total_rmb>>8;
            Page_1111.money[2] =  Globa_2->total_rmb>>16;
						Page_1111.money[3] =  Globa_2->total_rmb>>24;

						Page_1111.charging = Page_1111.run;
						Page_1111.link =  (Globa_2->gun_link == 1)?1:0;
					  if(Globa_2->Start_Mode != VIN_START_TYPE)
				    {
						   Page_1111.APP  = 1;   //充电启动方式 1:刷卡  2：手机APP
					  }
						else
						{
							Page_1111.APP  = 3;
						}
						Page_1111.BMS_12V_24V = Globa_2->BMS_Power_V;
						
						memcpy(&Page_1111.SN[0], &Globa_2->card_sn[0], sizeof(Page_1111.SN));
						memcpy(&msg.argv[0], &Page_1111.reserve1, sizeof(PAGE_1111_FRAME));
						msg.type = 0x1111;//充电界面
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_1111_FRAME), IPC_NOWAIT);
						
						QT_timer_start(500);
					}	else if((count_wait %5  == 0)&&(count_wait1 != 0)){//等待时间超时500mS
						count_wait1 = 0;
						Page_BMS_data.BMS_Info_falg = Globa_2->BMS_Info_falg;
						Page_BMS_data.BMS_H_vol[0]=  Globa_2->BMS_H_voltage/100;
						Page_BMS_data.BMS_H_vol[1]=  (Globa_2->BMS_H_voltage/100)>>8;
						Page_BMS_data.BMS_H_cur[0]=  Globa_2->BMS_H_current/100;
						Page_BMS_data.BMS_H_cur[1]=  (Globa_2->BMS_H_current/100)>>8;
						Page_BMS_data.BMS_cell_LT[0]=  Globa_2->BMS_cell_LT;
						Page_BMS_data.BMS_cell_LT[1]=  Globa_2->BMS_cell_LT>>8; 
						Page_BMS_data.BMS_cell_HT[0]=  Globa_2->BMS_cell_HT;
						Page_BMS_data.BMS_cell_HT[1]=  Globa_2->BMS_cell_HT>>8;
		
						Page_BMS_data.need_voltage[0]=  Globa_2->need_voltage/100;
						Page_BMS_data.need_voltage[1]=  (Globa_2->need_voltage/100)>>8;
						Page_BMS_data.need_current[0]=  (Globa_2->need_current/100);
						Page_BMS_data.need_current[1]=  (Globa_2->need_current/100)>>8;
						
						memcpy(&Page_BMS_data.BMS_Vel[0],&Globa_2->BMS_Vel[0],sizeof(Page_BMS_data.BMS_Vel));
						
						Page_BMS_data.Battery_Rate_Vol[0]=  Globa_2->BMS_rate_voltage;
						Page_BMS_data.Battery_Rate_Vol[1]=  (Globa_2->BMS_rate_voltage)>>8;
						Page_BMS_data.Battery_Rate_AH[0]=  Globa_2->BMS_rate_AH;
						Page_BMS_data.Battery_Rate_AH[1]=  (Globa_2->BMS_rate_AH)>>8;
						Page_BMS_data.Battery_Rate_KWh[0]=  (Globa_2->Battery_Rate_KWh);
						Page_BMS_data.Battery_Rate_KWh[1]=  (Globa_2->Battery_Rate_KWh)>>8;
					
						Page_BMS_data.Cell_H_Cha_Vol[0]=  (Globa_2->Cell_H_Cha_Vol);
						Page_BMS_data.Cell_H_Cha_Vol[1]=  (Globa_2->Cell_H_Cha_Vol)>>8;
						Page_BMS_data.Cell_H_Cha_Temp =  Globa_2->Cell_H_Cha_Temp;
						Page_BMS_data.Charge_Mode =  Globa_2->Charge_Mode;
						Page_BMS_data.Charge_Gun_Temp =  Globa_2->Charge_Gun_Temp;
						Page_BMS_data.BMS_cell_HV_NO =  Globa_2->BMS_cell_HV_NO;
						Page_BMS_data.BMS_cell_LT_NO =  Globa_2->BMS_cell_LT_NO;
						Page_BMS_data.BMS_cell_HT_NO =  Globa_2->BMS_cell_HT_NO;
						memcpy(&Page_BMS_data.VIN[0],&Globa_2->VIN[0],sizeof(Page_BMS_data.VIN));
						memcpy(&msg.argv[0], &Page_BMS_data.reserve1, sizeof(PAGE_BMS_FRAME));
						msg.type = 0x1188;//充电界面
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_BMS_FRAME), IPC_NOWAIT);
					}
					break;
				}
				case 0x22:{//---------------2 自动充电-》 APP 充电界面 状态  2.2----------

					//Globa_2->meter_stop_KWH = Globa_2->meter_KWH;//取初始电表示数
					if(Globa_2->meter_KWH >= Globa_2->meter_stop_KWH)
					{
						Globa_2->meter_stop_KWH = Globa_2->meter_KWH;//取初始电表示数
						Electrical_abnormality = Current_Systime;
					}else{//连续3s获取到的数据都比较起始数据小，则直接停止充电
						if(abs(Current_Systime - Electrical_abnormality) > METER_ELECTRICAL_ERRO_COUNT)//超过1分钟了，需停止充电
						{
              Electrical_abnormality = Current_Systime;
							Globa_2->Manu_start = 0;
						  Globa_2->Charger_Over_Rmb_flag = 7;//读取电量有异常
							RunEventLogOut_2("计费QT判断读取电量有异常：Globa_2->meter_KWH=%d Globa_2->meter_stop_KWH=%d",Globa_2->meter_KWH,Globa_2->meter_stop_KWH);
						}
					}
					Globa_2->kwh = (Globa_2->meter_stop_KWH - Globa_2->meter_start_KWH);
					
					g_ulcharged_kwh[pre_price_index] = Globa_2->kwh - ul_start_kwh;//累加当前时段的耗电量	
					cur_price_index = GetCurPriceIndex(); //获取当前时间
					if(cur_price_index != pre_price_index){
						ul_start_kwh = Globa_2->kwh - g_ulcharged_kwh[cur_price_index];//load new start kwh,需要减去时段之前累加的电量，否则该时段之前累加的电量丢失----20160312
						pre_price_index = cur_price_index;							
					}	
					for(i = 0;i<4;i++){
						g_ulcharged_rmb[i] = g_ulcharged_kwh[i]*share_time_kwh_price[i];
					}
				 
					for(i = 0;i<4;i++){
						g_ulcharged_rmb[i] = (g_ulcharged_rmb[i] + COMPENSATION_VALUE)/100;
					}
					
					Globa_2->rmb = g_ulcharged_rmb[0] + g_ulcharged_rmb[1] + g_ulcharged_rmb[2] + g_ulcharged_rmb[3]; 

					if(Globa_2->Charger_param.Serv_Type == 1){ //按次数收
						Globa_2->total_rmb = (Globa_2->rmb + Globa_2->Charger_param.Serv_Price);//已充电电量对应的金额
					}else if(Globa_2->Charger_param.Serv_Type == 2){//按时间收
						Globa_2->total_rmb = (Globa_2->rmb + Globa_2->Time*Globa_2->Charger_param.Serv_Price/600);//已充电电量对应的金额
					}else{
						//Globa_2->total_rmb = (Globa_2->rmb + Globa_2->kwh*Globa_2->Charger_param.Serv_Price/100);//已充电电量对应的金额
						for(i = 0;i<4;i++){
							g_ulcharged_rmb_Serv[i] = g_ulcharged_kwh[i]*share_time_kwh_price_serve[i];
						}
						for(i = 0;i<4;i++){
							g_ulcharged_rmb_Serv[i] =( g_ulcharged_rmb_Serv[i] + COMPENSATION_VALUE)/100;
						}
						Globa_2->total_rmb = Globa_2->rmb + g_ulcharged_rmb_Serv[0] + g_ulcharged_rmb_Serv[1] + g_ulcharged_rmb_Serv[2] + g_ulcharged_rmb_Serv[3]; 
					}
					
				  Globa_2->ISO_8583_rmb_Serv = Globa_2->total_rmb - Globa_2->rmb ;               //总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
					Globa_2->ISO_8583_rmb = Globa_2->rmb;                     //电量消费金额 0.01yuan

					//充电过程中实时更新业务数据记录;
					//若意外掉电，则上电时判断最后一条数据记录的“上传标示”是否为“0x55”;
					//若是，以此时电表示数作为上次充电结束时的示数;
					if((Globa_2->kwh - temp_kwh) > 50){//每隔0.5度电左右保存一次数据;
						temp_kwh = Globa_2->kwh;

						sprintf(&temp_char[0], "%08d", Globa_2->meter_stop_KWH);				
						memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

						sprintf(&temp_char[0], "%08d", Globa_2->kwh);				
						memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
						
						sprintf(&temp_char[0], "%08d", Globa_2->rmb);				
						memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));
						if(Globa_2->kwh != 0){
							sprintf(&temp_char[0], "%08d", (Globa_2->rmb*100/Globa_2->kwh));  
						}else{
							sprintf(&temp_char[0], "%08d", 0);  
						}
						memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));
						sprintf(&temp_char[0], "%08d", Globa_2->total_rmb);				
						memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));

						sprintf(&temp_char[0], "%08d", Globa_2->total_rmb - Globa_2->rmb);				
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
						if(Update_Charger_Busy_DB_2(id, &busy_frame, 1, 0x55) == -1){
						 usleep(1);
						 Update_Charger_Busy_DB_2(id, &busy_frame, 1, 0x55);//更新数据 有流水号 传输标志0x55 不能上传					 
						}
						pthread_mutex_unlock(&busy_db_pmutex);
					}

					//手机终止充电
					if((Globa_2->App.APP_start == 0)||(Globa_2->Electric_pile_workstart_Enable == 1)||((Globa_2->APP_Start_Account_type == 1)&&(Globa_1->have_hart == 0))){
						Globa_2->Manu_start = 0;
						RunEventLogOut_2("计费QT判断用户主远程动停止充电");

						Globa_2->Special_price_APP_data_falg = 0; 
						Globa_2->App.APP_start = 0;
						Globa_2->APP_Start_Account_type = 0;
						sleep(3);
						Globa_2->meter_stop_KWH = (Globa_2->meter_KWH >= Globa_2->meter_stop_KWH)? Globa_2->meter_KWH:Globa_2->meter_stop_KWH;//取初始电表示数
						Globa_2->kwh = (Globa_2->meter_stop_KWH - Globa_2->meter_start_KWH);
						g_ulcharged_kwh[pre_price_index] = Globa_2->kwh - ul_start_kwh;//累加当前时段的耗电量	
						cur_price_index = GetCurPriceIndex(); //获取当前时间
						if(cur_price_index != pre_price_index){
							ul_start_kwh = Globa_2->kwh - g_ulcharged_kwh[cur_price_index];//load new start kwh,需要减去时段之前累加的电量，否则该时段之前累加的电量丢失----20160312
							pre_price_index = cur_price_index;							
						}	
						for(i = 0;i<4;i++){
							g_ulcharged_rmb[i] = g_ulcharged_kwh[i]*share_time_kwh_price[i];
						}
					 
						for(i = 0;i<4;i++){
							g_ulcharged_rmb[i] = (g_ulcharged_rmb[i] + COMPENSATION_VALUE)/100;
						}
						
						Globa_2->rmb = g_ulcharged_rmb[0] + g_ulcharged_rmb[1] + g_ulcharged_rmb[2] + g_ulcharged_rmb[3]; 

						if(Globa_2->Charger_param.Serv_Type == 1){ //按次数收
							Globa_2->total_rmb = (Globa_2->rmb + Globa_2->Charger_param.Serv_Price);//已充电电量对应的金额
						}else if(Globa_2->Charger_param.Serv_Type == 2){//按时间收
							Globa_2->total_rmb = (Globa_2->rmb + Globa_2->Time*Globa_2->Charger_param.Serv_Price/600);//已充电电量对应的金额
						}else{
							//Globa_2->total_rmb = (Globa_2->rmb + Globa_2->kwh*Globa_2->Charger_param.Serv_Price/100);//已充电电量对应的金额
							for(i = 0;i<4;i++){
								g_ulcharged_rmb_Serv[i] = g_ulcharged_kwh[i]*share_time_kwh_price_serve[i];
							}
							for(i = 0;i<4;i++){
								g_ulcharged_rmb_Serv[i] =( g_ulcharged_rmb_Serv[i] + COMPENSATION_VALUE)/100;
							}
							Globa_2->total_rmb = Globa_2->rmb + g_ulcharged_rmb_Serv[0] + g_ulcharged_rmb_Serv[1] + g_ulcharged_rmb_Serv[2] + g_ulcharged_rmb_Serv[3]; 
						}
				    Globa_2->ISO_8583_rmb_Serv = Globa_2->total_rmb - Globa_2->rmb ;               //总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
						Globa_2->ISO_8583_rmb = Globa_2->rmb;                     //电量消费金额 0.01yuan
						
						//-------------------扣费 更新业务进度数据----------------------------
						sprintf(&temp_char[0], "%08d", Globa_2->meter_stop_KWH);				
						memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

						sprintf(&temp_char[0], "%08d", Globa_2->kwh);				
						memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
						
						sprintf(&temp_char[0], "%08d", Globa_2->rmb);				
						memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));
						if(Globa_2->kwh != 0){
							sprintf(&temp_char[0], "%08d", (Globa_2->rmb*100/Globa_2->kwh));  
						}else{
							sprintf(&temp_char[0], "%08d", 0);  
						}
						memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));
			
						sprintf(&temp_char[0], "%08d", Globa_2->total_rmb);				
						memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));

						sprintf(&temp_char[0], "%08d", Globa_2->total_rmb - Globa_2->rmb);				
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
						Page_1300.cause = 11;//直接手动结束的 ;
						sprintf(&temp_char[0], "%02d", Page_1300.cause);		
						memcpy(&busy_frame.End_code[0], &temp_char[0], sizeof(busy_frame.End_code));//结束原因代码
				
 				   sprintf(&temp_char[0], "%.17s",Globa_2->VIN);//服务费收费类型 01：按次收费；02：按时间收费(单位：分/10分钟；03：按电量	9	N	N2
						//memcpy(&busy_frame.car_sn[0], &Globa_2->VIN[0], sizeof(busy_frame.car_sn));//
						memcpy(&busy_frame.car_sn[0], &temp_char[0], sizeof(busy_frame.car_sn));

						pthread_mutex_lock(&busy_db_pmutex);
						if(Update_Charger_Busy_DB_2(id, &busy_frame, 1, 0x66) == -1){
							sleep(1);
							Update_Charger_Busy_DB_2(id, &busy_frame, 1, 0x66);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
						}
						pthread_mutex_unlock(&busy_db_pmutex);
			
						Page_1300.cause = 11; 
					  sprintf(&temp_char[0], "%02d", Page_1300.cause);		
					  memcpy(&busy_frame.End_code[0], &temp_char[0], sizeof(busy_frame.End_code));//结束原因代码
            Globa_2->Charger_Over_Rmb_flag = 0;
						
						pthread_mutex_lock(&busy_db_pmutex);	
						
						if(Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00) == -1){
							sleep(1);
							Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
						}
						pthread_mutex_unlock(&busy_db_pmutex);

						//-------------------界面显示数据内容---------------------------------
						if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
							Voicechip_operation_function(0x09);//09H     “谢谢惠顾，再见”
						}else{
							Voicechip_operation_function(0x11);//11H	谢谢使用再见.wav
						}
						Globa_2->charger_state = 0x03;//为了使手机控制停止后可以再启动，此处充电机置为空闲状态
						Globa_2->gun_state = 0x03;
					 
						Page_1300.cause = 11;//直接手动结束的 ;
						Page_1300.Capact = Globa_2->BMS_batt_SOC;
						Page_1300.elapsed_time[0] =  Globa_2->Time;
						Page_1300.elapsed_time[1] =  Globa_2->Time>>8;
						Page_1300.KWH[0] =  Globa_2->kwh;
						Page_1300.KWH[1] =  Globa_2->kwh>>8;
						Page_1300.RMB[0] =  Globa_2->total_rmb;
						Page_1300.RMB[1] =  Globa_2->total_rmb>>8;
						Page_1300.RMB[2] =  Globa_2->total_rmb>>16;
						Page_1300.RMB[3] =  Globa_2->total_rmb>>24;
						memcpy(&Page_1300.SN[0], &busy_frame.card_sn[0], sizeof(Page_1300.SN));
						Page_1300.APP = 2;   //充电启动方式 1:刷卡  2：手机APP

						memcpy(&msg.argv[0], &Page_1300.reserve1, sizeof(PAGE_1300_FRAME));
						msg.type = 0x1300;//充电结束界面
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_1300_FRAME), IPC_NOWAIT);
						sleep(1);//不用延时太久，APP控制，需要立即上传业务数据
						
						pthread_mutex_lock(&busy_db_pmutex);
					  all_id = 0;
						Insertflag = Insert_Charger_Busy_DB(&busy_frame, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
						if((Insertflag != -1)&&(all_id != 0)){//表示插入数据成功，其他的则需要重新插入
							Delete_Record_busy_DB_2(id);
							if(DEBUGLOG) RUN_GUN2_LogOut("2号枪交易数据,插入到正式数据库中并删除临时数据库记录 ID = %d",id);  
							id = 0;
						}else{
							if(DEBUGLOG) RUN_GUN2_LogOut("2号枪交易数据,插入到正式数据库中失败 ID = %d",id);  
						}
						pthread_mutex_unlock(&busy_db_pmutex);
						
					  if(id != 0 ){
							pthread_mutex_lock(&busy_db_pmutex);
							for(i=0;i<5;i++){
								if(Select_NO_Over_Record_2(id,0)!= -1){//表示有没有上传的记录
									Select_NO_Over_count = 0;
									memcpy(&pre_old_busy_frame, &busy_frame, sizeof(pre_old_busy_frame));
									pre_old_id = id;
									if(Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00) == -1){
										sleep(1);
										Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
									}
								}else{
									if(DEBUGLOG) RUN_GUN2_LogOut("%s %d","二号枪交易数据,更新upm=0x00成功 ID=",id);  
									Select_NO_Over_count = 0;
									pre_old_id = 0;	
									id = 0;
									break;
								}
								sleep(1);
							}
							pthread_mutex_unlock(&busy_db_pmutex);
					  }
						msg.type = 0x150;
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);				
						Globa_2->QT_Step = 0x03; //自动充电分开的AB通道
						Globa_2->BMS_Power_V = 0;
						Globa_2->Operation_command = 2;//进行解锁
						memset(&Globa_2->App.ID, '0', sizeof(Globa_2->App.ID));
						break;
					}

					if(Globa_2->charger_start == 0){//充电彻底结束处理
						Globa_2->App.APP_start = 0;
						Globa_2->Special_price_APP_data_falg = 0; 
						sleep(2);
						Globa_2->meter_stop_KWH = (Globa_2->meter_KWH >= Globa_2->meter_stop_KWH)? Globa_2->meter_KWH:Globa_2->meter_stop_KWH;//取初始电表示数
						//Globa_2->meter_stop_KWH = Globa_2->meter_KWH;//取初始电表示数
						Globa_2->kwh = (Globa_2->meter_stop_KWH - Globa_2->meter_start_KWH);
						g_ulcharged_kwh[pre_price_index] = Globa_2->kwh - ul_start_kwh;//累加当前时段的耗电量	
						cur_price_index = GetCurPriceIndex(); //获取当前时间
						if(cur_price_index != pre_price_index){
							ul_start_kwh = Globa_2->kwh - g_ulcharged_kwh[cur_price_index];//load new start kwh,需要减去时段之前累加的电量，否则该时段之前累加的电量丢失----20160312
							pre_price_index = cur_price_index;							
						}	
						for(i = 0;i<4;i++){
							g_ulcharged_rmb[i] = g_ulcharged_kwh[i]*share_time_kwh_price[i];
						}
					 
						for(i = 0;i<4;i++){
							g_ulcharged_rmb[i] = (g_ulcharged_rmb[i] + COMPENSATION_VALUE)/100;
						}
						
						Globa_2->rmb = g_ulcharged_rmb[0] + g_ulcharged_rmb[1] + g_ulcharged_rmb[2] + g_ulcharged_rmb[3]; 

						if(Globa_2->Charger_param.Serv_Type == 1){ //按次数收
							Globa_2->total_rmb = (Globa_2->rmb + Globa_2->Charger_param.Serv_Price);//已充电电量对应的金额
						}else if(Globa_2->Charger_param.Serv_Type == 2){//按时间收
							Globa_2->total_rmb = (Globa_2->rmb + Globa_2->Time*Globa_2->Charger_param.Serv_Price/600);//已充电电量对应的金额
						}else{
							//Globa_2->total_rmb = (Globa_2->rmb + Globa_2->kwh*Globa_2->Charger_param.Serv_Price/100);//已充电电量对应的金额
							for(i = 0;i<4;i++){
								g_ulcharged_rmb_Serv[i] = g_ulcharged_kwh[i]*share_time_kwh_price_serve[i];
							}
							for(i = 0;i<4;i++){
								g_ulcharged_rmb_Serv[i] =( g_ulcharged_rmb_Serv[i] + COMPENSATION_VALUE)/100;
							}
							Globa_2->total_rmb = Globa_2->rmb + g_ulcharged_rmb_Serv[0] + g_ulcharged_rmb_Serv[1] + g_ulcharged_rmb_Serv[2] + g_ulcharged_rmb_Serv[3]; 
						}

				    Globa_2->ISO_8583_rmb_Serv = Globa_2->total_rmb - Globa_2->rmb ;               //总费用, 包括服务费（暂时按照电量收）和电量消费金额 精确到分
						Globa_2->ISO_8583_rmb = Globa_2->rmb;                     //电量消费金额 0.01yuan
	
						//-------------------扣费 更新业务进度数据----------------------------
						sprintf(&temp_char[0], "%08d", Globa_2->meter_stop_KWH);				
						memcpy(&busy_frame.end_kwh[0], &temp_char[0], sizeof(busy_frame.end_kwh));

						sprintf(&temp_char[0], "%08d", Globa_2->kwh);				
						memcpy(&busy_frame.kwh[0], &temp_char[0], sizeof(busy_frame.kwh));
						
						sprintf(&temp_char[0], "%08d", Globa_2->rmb);				
						memcpy(&busy_frame.rmb[0], &temp_char[0], sizeof(busy_frame.rmb));
						if(Globa_2->kwh != 0){
							sprintf(&temp_char[0], "%08d", (Globa_2->rmb*100/Globa_2->kwh));  
						}else{
							sprintf(&temp_char[0], "%08d", 0);  
						}
						memcpy(&busy_frame.rmb_kwh[0], &temp_char[0], sizeof(busy_frame.rmb_kwh));
	
						sprintf(&temp_char[0], "%08d", Globa_2->total_rmb);				
						memcpy(&busy_frame.total_rmb[0], &temp_char[0], sizeof(busy_frame.total_rmb));

						sprintf(&temp_char[0], "%08d", Globa_2->total_rmb - Globa_2->rmb);				
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
						sprintf(&temp_char[0], "%.17s",Globa_2->VIN);//服务费收费类型 01：按次收费；02：按时间收费(单位：分/10分钟；03：按电量	9	N	N2
						//memcpy(&busy_frame.car_sn[0], &Globa_2->VIN[0], sizeof(busy_frame.car_sn));//
						memcpy(&busy_frame.car_sn[0], &temp_char[0], sizeof(busy_frame.car_sn));

	          if(Globa_2->Charger_Over_Rmb_flag == 1){
							Page_1300.cause = 13;
						  sent_warning_message(0x94, Page_1300.cause, 2, 0);
						}else if(Globa_2->Charger_Over_Rmb_flag == 5){
							Page_1300.cause = 65;
							sent_warning_message(0x94, Page_1300.cause, 2, 0);
						}else if(Globa_2->Charger_Over_Rmb_flag == 6){//达到SOC限制条件
						  Page_1300.cause = 9; 
							sent_warning_message(0x94, Page_1300.cause, 2, 0);
					  }else if(Globa_2->Charger_Over_Rmb_flag == 7){//读取到的电表电量异常
						  Page_1300.cause = 8;
						  sent_warning_message(0x94, Page_1300.cause, 2, 0);							
					  }else if(Globa_2->Charger_Over_Rmb_flag == 8){//充电电量超过SOC能允许充电的值
							Page_1300.cause = 7;
							sent_warning_message(0x94, Page_1300.cause, 2, 0);							
						}else{
							Page_1300.cause = Globa_2->charger_over_flag; 
						}
						RunEventLogOut_2("计费QT判断充电结束:Page_1300.cause =%d,Globa_2->charger_over_flag =%d ,Charger_Over_Rmb_flag =%d ",Page_1300.cause,Globa_2->charger_over_flag,Globa_2->Charger_Over_Rmb_flag);

					  sprintf(&temp_char[0], "%02d", Page_1300.cause);		
					  memcpy(&busy_frame.End_code[0], &temp_char[0], sizeof(busy_frame.End_code));//结束原因代码
						
						pthread_mutex_lock(&busy_db_pmutex);
						if(Update_Charger_Busy_DB_2(id, &busy_frame, 1, 0x66) == -1){
							sleep(1);
							Update_Charger_Busy_DB_2(id, &busy_frame, 1, 0x66);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
						}
						pthread_mutex_unlock(&busy_db_pmutex);
						
					
            Globa_1->Charger_Over_Rmb_flag = 0;
						
						pthread_mutex_lock(&busy_db_pmutex);					
						if(Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00) == -1){
							usleep(100000);
							Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
						}
						pthread_mutex_unlock(&busy_db_pmutex);


						//-------------------界面显示数据内容---------------------------------
						if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
							Voicechip_operation_function(0x09);//09H     “谢谢惠顾，再见”
						}else{
							Voicechip_operation_function(0x11);//11H	谢谢使用再见.wav
						}
						Globa_2->charger_state = 0x03;//为了使手机控制停止后可以再启动，此处充电机置为空闲状态
			
						Globa_2->gun_state = 0x03;

						//Page_1300.cause = Globa_2->charger_over_flag;
						Page_1300.Capact = Globa_2->BMS_batt_SOC;
						Page_1300.elapsed_time[0] =  Globa_2->Time;
						Page_1300.elapsed_time[1] =  Globa_2->Time>>8;
						Page_1300.KWH[0] =  Globa_2->kwh;
						Page_1300.KWH[1] =  Globa_2->kwh>>8;
						Page_1300.RMB[0] =  Globa_2->total_rmb;
						Page_1300.RMB[1] =  Globa_2->total_rmb>>8;
						Page_1300.RMB[2] =  Globa_2->total_rmb>>16;
						Page_1300.RMB[3] =  Globa_2->total_rmb>>24;
						memcpy(&Page_1300.SN[0], &busy_frame.card_sn[0], sizeof(Page_1300.SN));
						Page_1300.APP = 2;   //充电启动方式 1:刷卡  2：手机APP

						memcpy(&msg.argv[0], &Page_1300.reserve1, sizeof(PAGE_1300_FRAME));
						msg.type = 0x1300;//充电结束结算界面
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_1300_FRAME), IPC_NOWAIT);
						sleep(1);//不用延时太久，APP控制，需要立即上传业务数据
						
						pthread_mutex_lock(&busy_db_pmutex);
						 all_id = 0;
						 Insertflag = Insert_Charger_Busy_DB(&busy_frame, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
						if((Insertflag != -1)&&(all_id != 0)){//表示插入数据成功，其他的则需要重新插入
							Delete_Record_busy_DB_2(id);
							if(DEBUGLOG) RUN_GUN2_LogOut("2号枪交易数据,插入到正式数据库中并删除临时数据库记录 ID = %d",id);  
							id = 0;
						}else{
							if(DEBUGLOG) RUN_GUN2_LogOut("2号枪交易数据,插入到正式数据库中失败 ID = %d",id);  
						}
						pthread_mutex_unlock(&busy_db_pmutex);
						
					  if(id != 0 ){
							pthread_mutex_lock(&busy_db_pmutex);
							for(i=0;i<5;i++){
								usleep(200000);
								if(Select_NO_Over_Record_2(id,0)!= -1){//表示有没有上传的记录
									Select_NO_Over_count = 0;
									memcpy(&pre_old_busy_frame, &busy_frame, sizeof(pre_old_busy_frame));
									pre_old_id = id;
									if(Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00) == -1){
										usleep(100000);
										Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
									}
								}else{
									if(DEBUGLOG) RUN_GUN2_LogOut("%s %d","二号枪交易数据,更新upm=0x00成功 ID=",id);  
									Select_NO_Over_count = 0;
									pre_old_id = 0;	
									id = 0;
									break;
								}
							}

							pthread_mutex_unlock(&busy_db_pmutex);
						}
						msg.type = 0x150;
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);				
						Globa_2->QT_Step = 0x03; //自动充电分开的AB通道
						Globa_2->BMS_Power_V = 0;
						Globa_2->Operation_command = 2;//进行解锁
						memset(&Globa_2->App.ID, '0', sizeof(Globa_2->App.ID));
						Globa_2->Charger_Over_Rmb_flag = 0;

						break;
					}

					if((Globa_2->total_rmb + 50) >= Globa_2->App.last_rmb){//用户卡余额不足
						Globa_2->Manu_start = 0;
						Globa_2->Charger_Over_Rmb_flag = 1;
					}
					
					if(Globa_2->Time >=  120)//120秒，充电启动2分种之后对电流判断
					{
						
						if(Globa_2->Charger_param.CurrentVoltageJudgmentFalg == 1)
						{ 						
							if(((abs(Globa_2->Output_current - Globa_2->meter_Current_A)*100 > METER_CURR_THRESHOLD_VALUE*Globa_2->Output_current)&&(Globa_2->Output_current >= LIMIT_CURRENT_VALUE))
								||((abs(Globa_2->Output_current - Globa_2->meter_Current_A) > 1000)&&(Globa_2->Output_current < LIMIT_CURRENT_VALUE)&&(Globa_2->Output_current >= MIN_LIMIT_CURRENT_VALUE)))
							{
								if(abs(Current_Systime - meter_Curr_Threshold_count) > METER_CURR_COUNT)//超过1分钟了，需停止充电
								{
									Globa_2->Manu_start = 0;
									Globa_2->Charger_Over_Rmb_flag = 5;//电流计量有误差
									meter_Curr_Threshold_count = Current_Systime;
									RunEventLogOut_2("计费QT判断计量有误差 Globa_2->meter_Current_A =%d Globa_2->Output_current=%d",Globa_2->meter_Current_A,Globa_2->Output_current);
								}
							}
							else
							{
								meter_Curr_Threshold_count = Current_Systime;
							}
						}			
					
						if(0 == check_current_low_st)
						{
							if(Globa_2->BMS_batt_SOC >= Globa_2->Charger_param.SOC_limit   )//
							{						
								if(Globa_2->Output_current < (Globa_2->Charger_param.min_current*1000))//电流低于限值
								{
									current_low_timestamp = Current_Systime;
									check_current_low_st = 1;
								}
							}	
						}else//电流低持续是否达到1分钟
						{
							if(Globa_2->Output_current > (Globa_2->Charger_param.min_current*1000 + 1000))//1A回差
							{
								check_current_low_st = 0;//clear						
							}
							
							if(Globa_2->BMS_batt_SOC < Globa_2->Charger_param.SOC_limit   )
								check_current_low_st = 0;//clear
							
							if(Globa_2->Output_current < (Globa_2->Charger_param.min_current*1000))
							{
								if(abs(Current_Systime - current_low_timestamp) > SOCLIMIT_CURR_COUNT)//超过30s钟了，需停止充电
								{
									Globa_2->Manu_start = 0;	
							  	Globa_2->Charger_Over_Rmb_flag = 6;//达到SOC限制条件
									current_low_timestamp = Current_Systime;
									RunEventLogOut_2("计费QT判断达到soc限制条件");
								}
							}				
						}
						
					  if((Globa_2->BMS_Info_falg == 1)&&(Globa_2->Battery_Rate_KWh > 200))//收到BMs额定信息,额定电度需要大于20kwh才进行判断
						{
							if(Globa_2->kwh >= ((100 - Globa_2->BMS_batt_Start_SOC + MAX_LIMIT_OVER_RATEKWH)*Globa_2->Battery_Rate_KWh/10))
							{
								if(abs(Current_Systime - current_overkwh_timestamp) > OVER_RATEKWH_COUNT)//超过1分钟了，需停止充电
								{
									Globa_2->Manu_start = 0;	
									Globa_2->Charger_Over_Rmb_flag = 8;//达到SOC限制条件
									RunEventLogOut_2("计费QT判断达到根据总电量和起始SOC算出需要的总电量：Battery_Rate_KWh =%d.%d  充电电量 Globa_2->kwh = %d.%d ：startsoc=%d endsoc=%d",Globa_2->Battery_Rate_KWh/10,Globa_2->Battery_Rate_KWh%10, Globa_2->kwh/100,Globa_2->kwh%100,Globa_2->BMS_batt_Start_SOC,Globa_2->BMS_batt_SOC);
									current_overkwh_timestamp = Current_Systime;
								}
							}else{
								current_overkwh_timestamp = Current_Systime;
							}
						}else{
							current_overkwh_timestamp = Current_Systime;
						}
					}	
							
					if(Globa_2->App.APP_charger_type == 1){//按电量充电
						if(Globa_2->kwh >= Globa_2->App.APP_charge_kwh){
							Globa_2->Manu_start = 0;
						}
					}else if(Globa_2->App.APP_charger_type == 2){//按时间充电
						if(Globa_2->user_sys_time >= Globa_2->App.APP_charge_time){
							Globa_2->Manu_start = 0;
						}
					}else if(Globa_2->App.APP_charger_type == 3){//按金额充电
						if((Globa_2->total_rmb + 50) >= Globa_2->App.APP_charge_money){
							Globa_2->Manu_start = 0;
						}
					}

					count_wait++;
        	if((count_wait %5  == 0)&&(count_wait1 == 0)){//等待时间超时500mS
						count_wait1 = 1;
						Page_1111.BATT_type = Globa_2->BATT_type;		
						Page_1111.BATT_V[0] = Globa_2->Output_voltage/100;
						Page_1111.BATT_V[1] = (Globa_2->Output_voltage/100)>>8;
						Page_1111.BATT_C[0] = Globa_2->Output_current/100;
						Page_1111.BATT_C[1] = (Globa_2->Output_current/100)>>8;
						Page_1111.KWH[0] =  Globa_2->kwh;
						Page_1111.KWH[1] =  Globa_2->kwh>>8;

						Page_1111.Capact =  Globa_2->BMS_batt_SOC;
						Page_1111.run    = (Globa_2->Output_current > 1000)?1:0;//有充电电流 》1A

						Page_1111.elapsed_time[0] = Globa_2->Time;
						Page_1111.elapsed_time[1] = Globa_2->Time>>8;
						Page_1111.need_time[0] = Globa_2->BMS_need_time;
						Page_1111.need_time[1] = Globa_2->BMS_need_time>>8;
						Page_1111.cell_HV[0] =  Globa_2->BMS_cell_HV;
						Page_1111.cell_HV[1] =  Globa_2->BMS_cell_HV>>8;
			
            Page_1111.money[0] =  Globa_2->total_rmb;
						Page_1111.money[1] =  Globa_2->total_rmb>>8;
            Page_1111.money[2] =  Globa_2->total_rmb>>16;
						Page_1111.money[3] =  Globa_2->total_rmb>>24;

						Page_1111.charging = Page_1111.run;
						Page_1111.link =  (Globa_2->gun_link == 1)?1:0;
						Page_1111.BMS_12V_24V = Globa_2->BMS_Power_V;
						
						memcpy(&Page_1111.SN[0], &busy_frame.card_sn[0], sizeof(Page_1111.SN));
						Page_1111.APP = 2;   //充电启动方式 1:刷卡  2：手机APP
						
						memcpy(&msg.argv[0], &Page_1111.reserve1, sizeof(PAGE_1111_FRAME));
						msg.type = 0x1111;//充电界面
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_1111_FRAME), IPC_NOWAIT);
					}else	if((count_wait %5  == 0)&&(count_wait1 != 0)){//等待时间超时500mS
						count_wait1 = 0;
						Page_BMS_data.BMS_Info_falg = Globa_2->BMS_Info_falg;
						Page_BMS_data.BMS_H_vol[0]=  Globa_2->BMS_H_voltage/100;
						Page_BMS_data.BMS_H_vol[1]=  (Globa_2->BMS_H_voltage/100)>>8;
						Page_BMS_data.BMS_H_cur[0]=  Globa_2->BMS_H_current/100;
						Page_BMS_data.BMS_H_cur[1]=  (Globa_2->BMS_H_current/100)>>8;
						Page_BMS_data.BMS_cell_LT[0]=  Globa_2->BMS_cell_LT;
						Page_BMS_data.BMS_cell_LT[1]=  Globa_2->BMS_cell_LT>>8; 
						Page_BMS_data.BMS_cell_HT[0]=  Globa_2->BMS_cell_HT;
						Page_BMS_data.BMS_cell_HT[1]=  Globa_2->BMS_cell_HT>>8;
		
						Page_BMS_data.need_voltage[0]=  Globa_2->need_voltage/100;
						Page_BMS_data.need_voltage[1]=  (Globa_2->need_voltage/100)>>8;
						Page_BMS_data.need_current[0]=  (Globa_2->need_current/100);
						Page_BMS_data.need_current[1]=  (Globa_2->need_current/100)>>8;
						
						memcpy(&Page_BMS_data.BMS_Vel[0],&Globa_2->BMS_Vel[0],sizeof(Page_BMS_data.BMS_Vel));
						
						Page_BMS_data.Battery_Rate_Vol[0]=  Globa_2->BMS_rate_voltage;
						Page_BMS_data.Battery_Rate_Vol[1]=  (Globa_2->BMS_rate_voltage)>>8;
						Page_BMS_data.Battery_Rate_AH[0]=  Globa_2->BMS_rate_AH;
						Page_BMS_data.Battery_Rate_AH[1]=  (Globa_2->BMS_rate_AH)>>8;
						Page_BMS_data.Battery_Rate_KWh[0]=  (Globa_2->Battery_Rate_KWh);
						Page_BMS_data.Battery_Rate_KWh[1]=  (Globa_2->Battery_Rate_KWh)>>8;
					
						Page_BMS_data.Cell_H_Cha_Vol[0]=  (Globa_2->Cell_H_Cha_Vol);
						Page_BMS_data.Cell_H_Cha_Vol[1]=  (Globa_2->Cell_H_Cha_Vol)>>8;
						Page_BMS_data.Cell_H_Cha_Temp =  Globa_2->Cell_H_Cha_Temp;
						Page_BMS_data.Charge_Mode =  Globa_2->Charge_Mode;
						Page_BMS_data.Charge_Gun_Temp =  Globa_2->Charge_Gun_Temp;
						Page_BMS_data.BMS_cell_HV_NO =  Globa_2->BMS_cell_HV_NO;
						Page_BMS_data.BMS_cell_LT_NO =  Globa_2->BMS_cell_LT_NO;
						Page_BMS_data.BMS_cell_HT_NO =  Globa_2->BMS_cell_HT_NO;
						memcpy(&Page_BMS_data.VIN[0],&Globa_2->VIN[0],sizeof(Page_BMS_data.VIN));
						memcpy(&msg.argv[0], &Page_BMS_data.reserve1, sizeof(PAGE_BMS_FRAME));
						msg.type = 0x1188;//充电界面
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, sizeof(PAGE_BMS_FRAME), IPC_NOWAIT);
					}

					break;
				}
				case 0x23:{//---------------2      充电结束 结算界面      2.3-------------
					Globa_2->Charger_Over_Rmb_flag = 0;
					if(Globa_2->gun_link != 1 ){
						count_wait++;
						if(count_wait > 300){//等待时间超时60S
							sprintf(&temp_char[0], "%.17s",Globa_2->VIN);//服务费收费类型 01：按次收费；02：按时间收费(单位：分/10分钟；03：按电量	9	N	N2
						  //memcpy(&busy_frame.car_sn[0], &Globa_2->VIN[0], sizeof(busy_frame.car_sn));//
					  	memcpy(&busy_frame.car_sn[0], &temp_char[0], sizeof(busy_frame.car_sn));
              if(id !=0){
								pthread_mutex_lock(&busy_db_pmutex);
								if(Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00) == -1){
									usleep(10000);
									Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
								}
								pthread_mutex_unlock(&busy_db_pmutex);
								usleep(20000);
								
								pthread_mutex_lock(&busy_db_pmutex);
							
								pthread_mutex_lock(&busy_db_pmutex);
								 all_id = 0;
								 Insertflag = Insert_Charger_Busy_DB(&busy_frame, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
								if((Insertflag != -1)&&(all_id != 0)){//表示插入数据成功，其他的则需要重新插入
									Delete_Record_busy_DB_2(id);
									if(DEBUGLOG) RUN_GUN2_LogOut("2号枪交易数据,插入到正式数据库中并删除临时数据库记录 ID = %d",id);  
									id = 0;
								}else{
									if(DEBUGLOG) RUN_GUN2_LogOut("2号枪交易数据,插入到正式数据库中失败 ID = %d",id);  
								}
								pthread_mutex_unlock(&busy_db_pmutex);
								
								if(id != 0){
									for(i=0;i<5;i++){
										usleep(20000);
										if(Select_NO_Over_Record_2(id,0)!= -1){//表示有没有上传的记录
											Select_NO_Over_count = 0;
											memcpy(&pre_old_busy_frame, &busy_frame, sizeof(pre_old_busy_frame));
											pre_old_id = id;
											if(Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00) == -1){
												usleep(10000);
												Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
											}
										}else{
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
							msg.type = 0x150;
							msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);				
							Globa_2->QT_Step = 0x03; //自动充电分开的AB通道
							Globa_2->BMS_Power_V = 0;
							Globa_2->Operation_command = 2;//进行解锁
							break;//超时必须退出
						}
					}else{
						count_wait = 0;
					}
					
					if(Globa_2->Start_Mode == VIN_START_TYPE)
					{
	          #ifdef CHECK_BUTTON			
					 		if(msg.type == 0x1300){//收到该界面消息
								switch(msg.argv[0]){//判断 在当前界面下 用户操作的按钮
									case 0x01:{//1#通道选着按钮
									 Globa_2->checkout = 3;
									 break;
									}
								}
						  }
					#else 
						sleep(2);
						Globa_2->checkout = 3;
					#endif						
					}
					
					if(Globa_2->checkout != 0){//1通道检测到充电卡 0：未知 1：灰锁 2：补充交易 3：卡片正常
						msg.type = 0x95;//解除屏保
						msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);
						count_wait = 0;
						if(Globa_2->checkout == 1){//交易处理 灰锁
							//不动作，继续等待刷卡
						}else if(Globa_2->checkout == 2){//交易处理 补充交易
							memcpy(&busy_frame.flag[0], "02", sizeof(busy_frame.flag));//"00" 补充交易
							pthread_mutex_lock(&busy_db_pmutex);
							if(Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00) == -1){
								usleep(10000);
								Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
							}						
							pthread_mutex_unlock(&busy_db_pmutex);
						}else if(Globa_2->checkout == 3){//交易处理 正常
							memcpy(&busy_frame.flag[0], "00", sizeof(busy_frame.flag));//"00" 成功
							sprintf(&temp_char[0], "%.17s",Globa_2->VIN);//服务费收费类型 01：按次收费；02：按时间收费(单位：分/10分钟；03：按电量	9	N	N2
					  	//memcpy(&busy_frame.car_sn[0], &Globa_2->VIN[0], sizeof(busy_frame.car_sn));//
					  	memcpy(&busy_frame.car_sn[0], &temp_char[0], sizeof(busy_frame.car_sn));

              if(id != 0){
								pthread_mutex_lock(&busy_db_pmutex);
								if(Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00) == -1){
									sleep(1);
									Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
								}
								pthread_mutex_unlock(&busy_db_pmutex);
								sleep(1);
									//插入到需要上传的交易记录数据库中
								pthread_mutex_lock(&busy_db_pmutex);
								 all_id = 0;
								 Insertflag = Insert_Charger_Busy_DB(&busy_frame, &all_id, 0);//写新数据库记录 有流水号 传输标志0x55不能上传
								if((Insertflag != -1)&&(all_id != 0)){//表示插入数据成功，其他的则需要重新插入
									Delete_Record_busy_DB_2(id);
									if(DEBUGLOG) RUN_GUN2_LogOut("2号枪交易数据,插入到正式数据库中并删除临时数据库记录 ID = %d",id);  
									id = 0;
								}else{
									if(DEBUGLOG) RUN_GUN2_LogOut("2号枪交易数据,插入到正式数据库中失败 ID = %d",id);  
								}
								pthread_mutex_unlock(&busy_db_pmutex);

								if(id != 0){
									pthread_mutex_lock(&busy_db_pmutex);
									for(i=0;i<5;i++){
										if(Select_NO_Over_Record_2(id,0)!= -1){//表示有没有上传的记录
											Select_NO_Over_count = 0;
											memcpy(&pre_old_busy_frame, &busy_frame, sizeof(pre_old_busy_frame));
											pre_old_id = id;
											if(Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00) == -1){
												sleep(1);
												Update_Charger_Busy_DB_2(id, &busy_frame, 2, 0x00);//更新交易标志，传输标志0x00 交易数据完整，可以再获取流水号后上传
											}
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
							}
							
						  if( Globa_1->Charger_param.NEW_VOICE_Flag ==1){
								Voicechip_operation_function(0x09);//09H     “谢谢惠顾，再见”
							}else{
								Voicechip_operation_function(0x11);//11H	谢谢使用再见.wav
							}
							msg.type = 0x150;
							msgsnd(Globa_2->arm_to_qt_msg_id, &msg, 1, IPC_NOWAIT);				
							Globa_2->BMS_Power_V = 0;
							Globa_2->QT_Step = 0x03; //自动充电分开的AB通道
							Select_NO_Over_count = 0;
							Globa_2->Operation_command = 2;//进行解锁
						}
						Globa_2->checkout = 0;
					}else{
					//	Globa_2->checkout = 0;
					//	Globa_2->pay_step = 4;//2通道在等待刷卡扣费
					}

					break;
				}
				default:{
					printf("操作界面进入未知界面状态!!!!!!!!!!!!!!\n");
					break;
				}
			}
		}else{//单枪，
		  sleep(10);//10ms
		}
	}
}

/*********************************************************
**description：
***parameter  :none
***return		  :none
**********************************************************/
extern void Main_Thread_2(void)
{
	pthread_t td1;
	int ret ,stacksize = 2*1024*1024;
	pthread_attr_t attr;

	ret = pthread_attr_init(&attr);
	if (ret != 0)
		return;

	ret = pthread_attr_setstacksize(&attr, stacksize);
	if(ret != 0)
		return;
	
	/* 创建自动串口抄收线程 */
	if(0 != pthread_create(&td1, &attr, (void *)Main_Task_2, NULL)){
		perror("####pthread_create Main_Task_2 ERROR ####\n\n");
	}

	ret = pthread_attr_destroy(&attr);
	if(ret != 0)
		return;
}

