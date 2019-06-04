/****************************************Copyright (c)****************************************************
**                               DongGuan EAST Co.,LTD.
**                                     
**                                 http://www.EASTUPS.com
**
**--------------File Info---------------------------------------------------------------------------------
** File Name:               Carlock_task.c 
** Last modified Date:      2016.9.29
** Last Version:            1.0
** Description:             与地锁模块通信
** 
**--------------------------------------------------------------------------------------------------------
** Created By:              
** Created date:            2016.9.29
** Version:                 1.0
** Descriptions:            The original version 初始版本
**
**--------------------------------------------------------------------------------------------------------
*********************************************************************************************************/
//#include "type_def.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <termio.h>
#include <sys/time.h>
#include <time.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <sys/shm.h> 
#include "Carlock_task.h"
#include "../data/globalvar.h"
#include "sw_fifo.h"
#include "../../inc/sqlite/sqlite3.h"

DEV_CFG dev_cfg;
BIN_TIME g_rtc_time;
int g_deal_seq = 0;
typedef enum
{
	LOCK_OPEN_LOCK           = 0X01,//开锁： CMD 0X01
	LOCK_SHUT_LOCK           = 0X02,//闭锁： CMD 0x02
	LOCK_GET_FIRMWARE_VER    = 0x03,//软件版本号查询：   CMD  0X1A
	LOCK_GET_Equipment_ID    = 0X04,//查询设备ID CMD 0x1D 其地址位为0xff；
	LOCK_GET_LOCK_ST         = 0X05,//读取锁状态： CMD 0X06
	LOCK_GET_ULTRASONIC_CYC  = 0X06,//读取超声波检测周期 CMD  0X08
	LOCK_GET_ULTRASONIC_TIME = 0X07,//读取超声波滤波时间 CMD  0X0A
	LOCK_GET_ULTRASONIC_PARA = 0X08,//超声波运行参数查询 CMD  0X14
	LOCK_SET_ULTRASONIC_CYC  = 0X09,//设置超声波检测周期： CMD 0X07
	LOCK_SET_ULTRASONIC_TIME = 0X10,//设置超声波滤波时间： CMD 0X09
   LOCK_SET_BEEP            = 0X0A,//蜂鸣器设置或查询：   CMD 0X15
	LOCK_SET_ULTRASONIC      = 0X0B,//设置超声波探测开关或查询： CMD 0X1B
	LOCK_SET_ADDR            = 0X0C,//ADDR设置：           CMD 0X1C
	LOCK_SET_BAUD            = 0X0D,//波特率设置：         CMD 0X1E
	LOCK_DEF                 = 0X00,
}ecar_Query_ST;

#define  CARLOCK_ULTRASONIC_CYC             1 //超声波检测周期，单位秒
#define  CARLOCK_ULTRASONIC_FILTER_TIME     5 //超声波滤波时间，单位秒



#define carlock_RECV_IDLE   0
#define carlock_RECV_READY  1
#define carlock_RECV_CHECK  2

#define BYTES_WAIT_BUS_IDLE   50//10
#define PLUG_CHGING 0X04
#define PLUG_CHECKOUTDONE 0X05
#define PLUG_CHECKOUTING  1

#define  APP_CAR_LOCK_UP 7
#define  APP_CAR_LOCK_DOWN 6

#define  PARK_BY_APP 1
#define  PARK_BY_CARD 2
#define  USER_TICKS_PER_SEC 1
#define   USER_TICKS_PER_MS  1 
unsigned char g_plug_work_st[2];
//unsigned char g_app_car_lock_flag = 0;
unsigned char g_card_car_lock_flag;//值APP_CAR_LOCK_UP和APP_CAR_LOCK_DOWN表升起和放下地锁
unsigned char g_card_car_lock_port;//刷卡控制地锁号，值0表示控制地锁1，值1表示控制地锁2	
unsigned char g_chg_user_ID [8];// ???????
unsigned char g_car_lock_user_id[2][16];


/*********************************************************************************************************
  静态全局变量定义区
*********************************************************************************************************/
static volatile unsigned char carlock_recv_len;   //串口接收字节计数
unsigned char carlock_recv_st = carlock_RECV_IDLE;

//static struct   M3_UART_DCB     carlock_dcb;//保存当前的串口参数


static unsigned char carlock_comm_fault_flag;//2个车位锁通信故障标志,比特0为第1个...
static unsigned char cur_poll_index;//0----1最大查询2个从机
#define  MAX_RETRY_CNT    5// 10
static unsigned char poll_retry_cnt[MAX_CARLOCK_NUM];//时间控制相关变量
static volatile time_t  pre_rx_timestamp;//先前记录的时刻
static volatile unsigned short carlock_min_ticks_in_idle;//总线空闲的最小时间，空闲状态下计时如果不够该数值收到的串口数据将予以忽略,至少4倍该波特率下的字符传输时间
#define  CARLOCK_CTL_PERIOD    60 //每5秒控制1次地锁
volatile time_t  carlock_last_ctl_timestamp[MAX_CARLOCK_NUM];//最后1次操作地锁的时刻
unsigned char carlock_ctl_cmd[MAX_CARLOCK_NUM];//地锁控制

#define  carlock_RX_FIFO_EMS      3 //接收队列元素个数
FIFO_DATA_ELEMENT   g_carlock_rx_fifo_em[carlock_RX_FIFO_EMS];
SW_FIFO_CTL         g_carlock_rx_fifo_ctl;//接收队列控制结构
#define carlock_RX_FIFO_WIDTH     60  //32//100 //100个字节1个单元

unsigned char carlock_rx_buf[carlock_RX_FIFO_EMS][carlock_RX_FIFO_WIDTH];//接收队列的数据缓冲区

unsigned short carlock_rx_len;//从接收队列中读取的字节数
unsigned char  carlock_rx_data[carlock_RX_FIFO_WIDTH];//保存从接收队列读取的数据
//////////////////////////////////////////////////////////////////////////////////////

#define carlock_SEND_BUF_SIZE    32
unsigned char carlock_recv_buf[carlock_RX_FIFO_WIDTH];//接收缓冲区
unsigned char carlock_send_buf[carlock_SEND_BUF_SIZE];//发送缓冲区
unsigned char G_id[2];
void car_recv_normal_proc(void);//响应帧OK处理
void car_recv_exec_proc(void);//响应帧异常处理

time_t Recv_timeout;
unsigned char Car_comexec_count[MAX_CARLOCK_NUM]; //累计通讯异常次数


unsigned short g_car_idle_min_distance[MAX_CARLOCK_NUM];


volatile time_t  carlock_no_car_timestamp[MAX_CARLOCK_NUM];//无车计时，地锁放下后检测到无车超过指定时间后升起地锁
unsigned char carlock_no_car_st[MAX_CARLOCK_NUM] = {0,0};

unsigned char carpark_st[MAX_CARLOCK_NUM] = {0,0};

USER_PARK_INFO   g_user_park_info[MAX_CARLOCK_NUM];
unsigned char    g_park_user_id[MAX_CARLOCK_NUM][16];
CHG_DEAL_REC     g_chg_car_park_data[MAX_CARLOCK_NUM]; //停车业务
unsigned  int    g_chg_car_park_id[MAX_CARLOCK_NUM];//停车业务数据库存储id
/*********************************************************************************************************
  函数定义区
*********************************************************************************************************/
CHG_BIN_TIME   RtcTime2ParkTime(BIN_TIME  cur_rtc_time)
{
	CHG_BIN_TIME  park_time;
	
	park_time.year           = cur_rtc_time.year;
	park_time.month          = cur_rtc_time.month;
	park_time.day_of_month   = cur_rtc_time.day_of_month;
	park_time.week_day       = cur_rtc_time.week_day;
	park_time.hour           = cur_rtc_time.hour;
	park_time.minute         = cur_rtc_time.minute;
	park_time.second         = cur_rtc_time.second;
	
	return park_time;
}

//将16字节的ascii字符串user_id_in转换为8字节的BCD输出
//如"0011223344556677"转换为0x00,0x11,0x22,0x33....0x77
static void ascii2bcd(unsigned char * user_bcd_id_out,unsigned char *user_id_in)
{
	unsigned long i;
	for(i=0;i<16;)
	{
		user_bcd_id_out[i/2] = ((user_id_in[i] - 0x30)<<4) | (user_id_in[i+1] - 0x30);
		i += 2 ;
	}	
}

int Insert_CarLock_Busy_DB(CHG_DEAL_REC *frame, unsigned int *id, unsigned int up_num)
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
	rc = sqlite3_open_v2(F_CarLock_Busy_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE |
									     SQLITE_OPEN_CREATE,
									     NULL);
	if(rc){
		//printf("Can't create F_CHARGER_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		if(DEBUGLOG) CARLOCK_RUN_LogOut("%s","打开数据库失败:");  
		return -1;
	}
	if(TRANSACTION_FLAG) sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);
	  
	sprintf(sql,"CREATE TABLE if not exists CarLock_Busy_Data(ID INTEGER PRIMARY KEY autoincrement, ");
	strcat(sql,"User_Card TEXT, ");
	strcat(sql,"falg TEXT, ");
	strcat(sql,"charge_type TEXT, ");
	strcat(sql,"Busy_SN TEXT, ");
	strcat(sql,"stop_car_allrmb TEXT, ");
	strcat(sql,"stop_car_start_time TEXT,");
	strcat(sql,"stop_car_end_time TEXT,");
	strcat(sql,"stop_car_rmb_price TEXT,");
	strcat(sql,"charge_last_rmb TEXT, ");
	strcat(sql,"ter_sn TEXT, ");	
	strcat(sql,"ConnectorId TEXT, ");	
	strcat(sql,"up_num INTEGER);");
	
	if(sqlite3_exec(db, sql, 0, 0, &zErrMsg) != SQLITE_OK)
	{
		//printf("CREATE TABLE CarLock_Busy_Data error: %s\n", sqlite3_errmsg(db));
		sqlite3_free(zErrMsg);
	}
	
	sprintf(sql, "INSERT INTO CarLock_Busy_Data VALUES (NULL, ");
	sprintf(&sql[strlen(sql)],"'%.16s',", frame->User_Card);
	sprintf(&sql[strlen(sql)],"'%.2s',", frame->falg);
	sprintf(&sql[strlen(sql)],"'%.2s',", frame->charge_type);
	sprintf(&sql[strlen(sql)],"'%.20s',", frame->Busy_SN);
	sprintf(&sql[strlen(sql)],"'%.8s',", frame->stop_car_allrmb);
	sprintf(&sql[strlen(sql)],"'%.14s',", frame->stop_car_start_time);
	sprintf(&sql[strlen(sql)],"'%.14s',", frame->stop_car_end_time);
	sprintf(&sql[strlen(sql)],"'%.8s',", frame->stop_car_rmb_price);
	sprintf(&sql[strlen(sql)],"'%.8s',", frame->charge_last_rmb);
	sprintf(&sql[strlen(sql)],"'%.12s',", frame->ter_sn);
	sprintf(&sql[strlen(sql)],"'%.2s',", frame->ConnectorId);
	sprintf(&sql[strlen(sql)],"%d);",up_num);
  
 	if(DEBUGLOG) CARLOCK_RUN_LogOut("插入正式数据库：%s",sql);  
	if(sqlite3_exec(db , sql , 0 , 0 , &zErrMsg) != SQLITE_OK){
		printf("Insert DI error: %s\n", sqlite3_errmsg(db));
		sqlite3_free(zErrMsg);
		if(TRANSACTION_FLAG) sqlite3_exec(db, "rollback transaction", NULL, NULL, NULL);
	  //if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
		if(DEBUGLOG) CARLOCK_RUN_LogOut("%s %s","插入命令执行失败枪号:",frame->ConnectorId);  
		sqlite3_close(db);
		return -1;
	}

	sprintf(sql, "SELECT MAX(ID) FROM CarLock_Busy_Data WHERE User_Card = '%.16s';", frame->User_Card);
	if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL)){
		if(azResult[ncolumn*1+0] != NULL){//2行1列
			*id = atoi(azResult[ncolumn*1+0]);
			sqlite3_free_table(azResult);
			if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
			sqlite3_close(db);
			if(DEBUGLOG) CARLOCK_RUN_LogOut("插入数据库ID号：%d",*id);  
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

int Update_CarLock_Busy_DB(unsigned int id, CHG_DEAL_REC *frame, unsigned int flag, unsigned int up_num)
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

	rc = sqlite3_open_v2(F_CarLock_Busy_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE |
									     SQLITE_OPEN_CREATE,
									     NULL);
	if(rc){
		printf("Can't create CarLock_Busy_Data database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}
	
	sprintf(sql,"CREATE TABLE if not exists CarLock_Busy_Data(ID INTEGER PRIMARY KEY autoincrement, ");
	strcat(sql,"User_Card TEXT, ");
	strcat(sql,"falg TEXT, ");
	strcat(sql,"charge_type TEXT, ");
	strcat(sql,"Busy_SN TEXT, ");
	strcat(sql,"stop_car_allrmb TEXT, ");
	strcat(sql,"stop_car_start_time TEXT, ");
	strcat(sql,"stop_car_end_time TEXT, ");
	strcat(sql,"charge_last_rmb TEXT, ");
	strcat(sql,"ter_sn TEXT, ");	
	strcat(sql,"ConnectorId TEXT, ");	
	strcat(sql,"up_num INTEGER);");
	
	if(sqlite3_exec(db, sql, 0, 0, &zErrMsg) != SQLITE_OK){
		//printf("CREATE TABLE Charger_Busy_Complete_Data error: %s\n", sqlite3_errmsg(db));
		sqlite3_free(zErrMsg);
	}
	if(TRANSACTION_FLAG) sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);

	if((flag == 1)&&(id > 0)){//更新 充电电量 充电金额 电表结束示数 充电结束时间,传输标志
		sprintf(sql, "UPDATE CarLock_Busy_Data SET stop_car_allrmb = '%.8s',  stop_car_end_time = '%.14s',up_num = '%d' WHERE ID = '%d' ;",frame->stop_car_allrmb,frame->stop_car_end_time,up_num, id);
 		if(DEBUGLOG) CARLOCK_RUN_LogOut("%s %s","停车中更新数据:",sql);  
		if(sqlite3_exec(db, sql, 0, 0, &zErrMsg) != SQLITE_OK){
  		sqlite3_free(zErrMsg);
		 if(TRANSACTION_FLAG)	sqlite3_exec(db, "rollback transaction", NULL, NULL, NULL);
			sqlite3_close(db);
			if(DEBUGLOG) CARLOCK_RUN_LogOut("停车中更新数据失败:");  
			return -1;
  	}
  }
	else if((flag == 2)&&(id > 0))
	{//更新 交易 传输标志
			sprintf(sql, "UPDATE CarLock_Busy_Data SET stop_car_allrmb = '%.8s',stop_car_end_time = '%.14s',up_num = '%d' WHERE ID = '%d' ;",frame->stop_car_allrmb,frame->stop_car_end_time,up_num, id);
			if(DEBUGLOG) CARLOCK_RUN_LogOut("%s %s","停车完成更新数据:",sql);  
			if(sqlite3_exec(db, sql, 0, 0, &zErrMsg) != SQLITE_OK){
				sqlite3_free(zErrMsg);
				if(TRANSACTION_FLAG) sqlite3_exec(db, "rollback transaction", NULL, NULL, NULL);
				sqlite3_close(db);
				if(DEBUGLOG) CARLOCK_RUN_LogOut("充电完成更新数据失败:");  
				return -1;
			}
  }
	if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
	sqlite3_close(db);
	return 0;
}


//检查是否有有效的泊车，如在设备启动时，地锁是放下的，则会将值0作为泊车用户!此为无效用户
unsigned char IsParkBusy(unsigned char car_lock_index)
{
	unsigned long i;
	for(i=0;i<8;i++)
		if(g_user_park_info[car_lock_index].user_id[i] != 0)
			break;
		
	if(i==8)//无效用户号
		return 0;
	else
	{
		if(g_user_park_info[car_lock_index].park_busy)
			return 1;
		else
			return 0;
	}
	
}

//检查泊车用户是否有效，如在设备启动时，地锁是放下的，则会将值0作为泊车用户!此为无效用户
unsigned char IsParkUserValid2(unsigned char car_lock_index)
{
	unsigned long i;
	/*if(car_lock_index == 0)
	{
    if((0 != memcmp(&Globa_1->Car_lock_Card[0],"0000000000000000", sizeof(Globa_1->Car_lock_Card))))
		{
					return 1;
		}	
		else 
		{
					return 1;
		}	
	}
	else
	{
		if((0 != memcmp(&Globa_2->Car_lock_Card[0],"0000000000000000", sizeof(Globa_2->Car_lock_Card))))
		{
					return 1;
		}	
		else 
		{
					return 1;
		}
	}
*/
	for(i=0;i<16;i++)
		if(g_park_user_id[car_lock_index][i] != 0)
			break;
	
	if(i == 16)//无效用户号
		return 0;
	else
		return 1;	
}

//检查泊车用户是否有效，如在设备启动时，地锁是放下的，则会将值0作为泊车用户!此为无效用户
unsigned char IsParkUserValid(unsigned char car_lock_index)
{
	unsigned long i;
	for(i=0;i<16;i++)
		if(g_user_park_info[car_lock_index].user_id[i] != 0)
			break;
		
	if(i == 16)//无效用户号
		return 0;
	else
		return 1;	
}

//返回值1表示当前卡号正在泊车中，值0没有
unsigned char IsCardParked(unsigned char card_id[])
{
	unsigned long i;
	for(i=0;i<MAX_CARLOCK_NUM;i++)
	{		
		if(0 == memcmp(g_user_park_info[i].user_id,  card_id ,8) )//user match
		{
			return 1;
		}				
	}
	return 0;//not found
	
}

//获取地锁状态值
unsigned char Get_Cardlock_state(unsigned char car_lock_index)
{
	return g_car_st[car_lock_index].lock_st;
}
time_t get_ticks_elapsed(int pre_time)
{
	time_t current_now;
	time(&current_now);
  return ((current_now - pre_time)>0?(current_now - pre_time):0);
}

CHG_DEAL_REC    park_deal_rec[2];
unsigned long   g_park_deal_seq[2];

time_t  park_start_time[MAX_CARLOCK_NUM];//停车计费的起始计算时刻
time_t  charge_end_time[MAX_CARLOCK_NUM];//充电完成的时间，计算充电完成后免费停车的时长

//停车计费处理
void car_park_handler(unsigned char park_index)
{
	unsigned char temp_char [30] = {0};
	unsigned long ten_min_cnt[2] = {0};//多少个10分钟计数
	unsigned long min_cnt[2] = {0};//分钟数计数
	time_t start_sec[2] = {0},cur_sec[2] = {0};
	struct tm tm_t;
	//printf("停车计费--carpark_st[park_index] = %d \n",carpark_st[park_index]);
	switch(carpark_st[park_index])
	{
		case 0:
			if((0x01 == g_car_st[park_index].lock_st) || (0x10 == g_car_st[park_index].lock_st))//地锁放下就开始计费---
			{
				//printf("停车计费--g_user_park_info[park_index].park_busy = %d \n",g_user_park_info[park_index].park_busy);

				if(0 == g_user_park_info[park_index].park_busy)
				{
					//printf("停车计费--IsParkUserValid2(park_index) = %d \n",IsParkUserValid2(park_index));

					if(IsParkUserValid2(park_index))//泊车用户有效---不是系统复位或掉电再上电后遇到地锁是放下的状态---
					{
						g_user_park_info[park_index].park_busy = 1;
						g_user_park_info[park_index].park_start_time =  time(NULL) ;
						g_user_park_info[park_index].park_end_time =  time(NULL) ;
            if(park_index == 0)
						{
							localtime_r(&g_user_park_info[park_index].park_start_time, &tm_t);  //结构指针指向当前时间	
							sprintf(&temp_char[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
																						 tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
								
							memcpy(&Globa_1->park_start_time[0], &temp_char[0], sizeof(Globa_1->park_start_time));         
						  
							memcpy(&g_chg_car_park_data[0].stop_car_start_time, &temp_char[0], sizeof(g_chg_car_park_data[0].stop_car_end_time));         
							memcpy(&g_chg_car_park_data[0].stop_car_end_time, &temp_char[0], sizeof(g_chg_car_park_data[0].stop_car_end_time));         
						}
						else
					  {
							localtime_r(&g_user_park_info[park_index].park_start_time, &tm_t);  //结构指针指向当前时间	
							sprintf(&temp_char[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
																						 tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
								
							memcpy(&Globa_2->park_start_time[0], &temp_char[0], sizeof(Globa_2->park_start_time));         
					  	memcpy(&g_chg_car_park_data[1].stop_car_start_time, &temp_char[0], sizeof(g_chg_car_park_data[1].stop_car_end_time));         
							memcpy(&g_chg_car_park_data[1].stop_car_end_time, &temp_char[0], sizeof(g_chg_car_park_data[1].stop_car_end_time));         
						}
						g_user_park_info[park_index].park_start_valid = 1;
						
						memcpy(g_user_park_info[park_index].user_id,  g_park_user_id[park_index], 16);//可能g_park_user_id全为0!比如系统复位或启动时，地锁如果是开锁的状态下
	         	memcpy(&g_chg_car_park_data[park_index].User_Card, g_user_park_info[park_index].user_id, sizeof(g_user_park_info[park_index].user_id));         

            g_user_park_info[park_index].park_port = park_index + 1; 
						park_start_time[park_index] = g_user_park_info[park_index].park_start_time;
						
						carpark_st[park_index] = 1;
						g_user_park_info[park_index].park_rmb = 0;//从0开始
						g_user_park_info[park_index].park_price = dev_cfg.dev_para.car_park_price;
						g_user_park_info[park_index].park_card_type = 0;
						//先写入停车开始时间-----test
	         	
						memcpy(&g_chg_car_park_data[park_index].falg[0], "00", sizeof(g_chg_car_park_data[park_index].falg));//员工卡，交易标志直接标记完成
						memcpy(&g_chg_car_park_data[park_index].charge_type[0], "02", sizeof(g_chg_car_park_data[park_index].charge_type));
						memcpy(&g_chg_car_park_data[park_index].Busy_SN[0], "00000000000000000000", sizeof(g_chg_car_park_data[park_index].Busy_SN));
						
						sprintf(&temp_char[0], "%08d",g_user_park_info[park_index].park_rmb);				
					  memcpy(&g_chg_car_park_data[park_index].stop_car_allrmb[0], &temp_char[0], sizeof(g_chg_car_park_data[park_index].stop_car_allrmb));

						sprintf(&temp_char[0], "%08d",g_user_park_info[park_index].park_price);				
					  memcpy(&g_chg_car_park_data[park_index].stop_car_rmb_price[0], &temp_char[0], sizeof(g_chg_car_park_data[park_index].stop_car_rmb_price));
					  
						memcpy(&g_chg_car_park_data[park_index].charge_last_rmb[0],"00000000", sizeof(g_chg_car_park_data[park_index].charge_last_rmb));
						memcpy(&g_chg_car_park_data[park_index].ter_sn[0],&Globa_1->Charger_param.SN[0], sizeof(g_chg_car_park_data[park_index].ter_sn));
            sprintf(&temp_char[0], "%02d",park_index+1);				
					  memcpy(&g_chg_car_park_data[park_index].ConnectorId[0], &temp_char[0], sizeof(g_chg_car_park_data[park_index].ConnectorId));
						Insert_CarLock_Busy_DB(&g_chg_car_park_data[park_index], &g_chg_car_park_id[park_index], 0x55);
					}
				}
			}
			break;
		
		case 1://等待车进入或无车退出
			g_plug_work_st[0] = Globa_1->gun_state;
			g_plug_work_st[1] = Globa_2->gun_state;
			if(0x00 == g_car_st[park_index].lock_st)//地锁处于稳定的升起状态中，肯定无车了,需重新下发开锁
			{//泊车结束--写结束账单
				carpark_st[park_index] = 0;
				g_user_park_info[park_index].park_end_time = time(NULL);					
				g_user_park_info[park_index].park_end_valid = 1;
	
				if(IsParkUserValid(park_index))//泊车用户有效----写记录前已判断了，无需再判断
				{
				  cur_sec[park_index]  = time(NULL); 
				  start_sec[park_index]  = park_start_time[park_index];//停车费计费开始时刻
					if(cur_sec[park_index]  >= start_sec[park_index] )//必须成立，否则系统时间被修改过或RTC异常
					{
						min_cnt[park_index]  = (cur_sec[park_index]  - start_sec[park_index] )/60;
					  //printf("---数据停车计费--min_cnt =%d ten_min_cnt = %d g_user_park_info[park_index].park_rmb = %d\n",min_cnt[park_index] ,ten_min_cnt[park_index] ,g_user_park_info[park_index].park_rmb);
						if(min_cnt[park_index]  % 10 != 0 )//不足10分钟也加1
						{
							if(min_cnt[park_index] >= dev_cfg.dev_para.free_minutes_after_charge)//停车时间需要大于免费停车时间才计算
							{
							  g_user_park_info[park_index].park_rmb += dev_cfg.dev_para.car_park_price;
							}
						}
					}
					
					if(g_user_park_info[park_index].park_rmb >= 0)//产生停车费用---test取消，即使停车费为0也保存并上传停车费账单
					{
						if(park_index == 0)
						{
							localtime_r(&g_user_park_info[park_index].park_end_time, &tm_t);  //结构指针指向当前时间	
							sprintf(&temp_char[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
																						 tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
								
							memcpy(&g_chg_car_park_data[0].stop_car_end_time, &temp_char[0], sizeof(g_chg_car_park_data[0].stop_car_end_time));         
						}
						else
					  {
							localtime_r(&g_user_park_info[park_index].park_end_time, &tm_t);  //结构指针指向当前时间	
							sprintf(&temp_char[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
																						 tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
								
							memcpy(&g_chg_car_park_data[1].stop_car_end_time, &temp_char[0], sizeof(g_chg_car_park_data[1].stop_car_end_time));         
						}
						sprintf(&temp_char[0], "%08d",g_user_park_info[park_index].park_rmb);				
					  memcpy(&g_chg_car_park_data[park_index].stop_car_allrmb[0], &temp_char[0], sizeof(g_chg_car_park_data[park_index].stop_car_allrmb));
					  //printf("结束停车计费--g_chg_car_park_id[park_index] = %d \n",g_chg_car_park_id[park_index]);
						Update_CarLock_Busy_DB(g_chg_car_park_id[park_index],&g_chg_car_park_data[park_index],2, 0);
					}
				}
				//使用完毕clear
				memset((void*)&g_user_park_info[park_index],0,sizeof(USER_PARK_INFO));
			  memset(&g_park_user_id[park_index],0,sizeof(g_park_user_id[park_index]));
			 	memset(&g_chg_car_park_data[park_index],0,sizeof(g_chg_car_park_data[park_index]));
			  if(park_index == 0)
				{
					memset(Globa_1->Car_lock_Card,0,sizeof(Globa_1->Car_lock_Card));
				}
				else
				{
					memset(Globa_2->Car_lock_Card,0,sizeof(Globa_2->Car_lock_Card));
				}
			}
			else
			{
				cur_sec[park_index]   = time(NULL); 

				if(1 == g_user_park_info[park_index].chg_once_done_flag )//已完成1次充电后	
				{
					if(cur_sec >= (charge_end_time[park_index]  +  dev_cfg.dev_para.free_minutes_after_charge*60 ) )//超过充电完成后免费停车时长了，要开始计费了
					{
												
						
					}
					else//还不需计费--一直更新计费时刻起点达到不计费目的
						park_start_time[park_index] = time(NULL);					
				}
				start_sec[park_index]  = park_start_time[park_index];//停车费计费开始时刻
				
			//	printf("---!!!--cur_sec =%ld start_sec = %ld (cur_sec-start_sec) = %d\n",cur_sec[park_index] ,start_sec[park_index] ,cur_sec[park_index] -start_sec[park_index] );
				
				if(cur_sec[park_index]  >= start_sec[park_index] )//必须成立，否则系统时间被修改过或RTC异常
				{
					min_cnt[park_index]  = (cur_sec[park_index]  - start_sec[park_index] )/60;

				 // printf("!!!--数据停车计费--min_cnt =%d ten_min_cnt = %d g_user_park_info[park_index].park_rmb = %d\n",min_cnt[park_index] ,ten_min_cnt[park_index] ,g_user_park_info[park_index].park_rmb);
					if(0 == g_user_park_info[park_index].chg_once_done_flag )					
					{ 
						if( PLUG_CHECKOUTDONE == g_plug_work_st[park_index])//检测到已结算重新开始计时---该状态至少维持5秒，一定能检测到
						{						
							park_start_time[park_index] = time(NULL);//重新计时
							charge_end_time[park_index] = time(NULL);
							g_user_park_info[park_index].chg_once_done_flag = 1;
						}
					}
					if(1 == g_user_park_info[park_index].chg_once_done_flag )					
					{
						if( PLUG_CHGING == g_plug_work_st[park_index])//检测到再次充电重新开始计时
						{						
							park_start_time[park_index] = time(NULL);//重新计时
							g_user_park_info[park_index].chg_once_done_flag = 0;
						}
					}
					
					ten_min_cnt[park_index]  = //min_cnt;//test 每1分钟累计1次    
									min_cnt[park_index]  / 10;		
				//	 printf("----更新数据停车计费--ten_min_cnt = %d g_user_park_info[park_index].park_rmb = %d\n",ten_min_cnt[park_index] ,g_user_park_info[park_index].park_rmb);
									
					if( ten_min_cnt[park_index]  >= 1)//每累计1个计费单位
					{
						park_start_time[park_index] = time(NULL);//开始累积另1个
						if(PLUG_CHGING == g_plug_work_st[park_index])
							//如果在充电中且不充电中不收停车费
							{
								//如果当前泊车用户号全0，则使用充电账号作为泊车用户
								if(0 == IsParkUserValid(park_index))
								{								
								  //ascii2bcd(g_user_park_info[park_index].user_id,  (unsigned char*)g_chg_user_ID);
									g_user_park_info[park_index].park_card_type = PARK_BY_APP;//固定为APP停车
								}
								//停车费用不累加
								if(dev_cfg.dev_para.free_when_charge)//充电期间免费
								{
								}
								else//不免费
									g_user_park_info[park_index].park_rmb += dev_cfg.dev_para.car_park_price;
							}
							else
							{								
								g_user_park_info[park_index].park_rmb += dev_cfg.dev_para.car_park_price;
							}
						//中途更新数据	
						if(park_index == 0)
						{
							g_user_park_info[park_index].park_end_time = time(NULL);					
							localtime_r(&g_user_park_info[park_index].park_end_time, &tm_t);  //结构指针指向当前时间	
							sprintf(&temp_char[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
																						 tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
								
							memcpy(&g_chg_car_park_data[0].stop_car_end_time, &temp_char[0], sizeof(g_chg_car_park_data[0].stop_car_end_time));         
						}
						else
					  {	
					    g_user_park_info[park_index].park_end_time = time(NULL);					
							localtime_r(&g_user_park_info[park_index].park_end_time, &tm_t);  //结构指针指向当前时间	
							sprintf(&temp_char[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, \
																						 tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
								
							memcpy(&g_chg_car_park_data[1].stop_car_end_time, &temp_char[0], sizeof(g_chg_car_park_data[1].stop_car_end_time));         
						}
						sprintf(&temp_char[0], "%08d",g_user_park_info[park_index].park_rmb);				
					  memcpy(&g_chg_car_park_data[park_index].stop_car_allrmb[0], &temp_char[0], sizeof(g_chg_car_park_data[park_index].stop_car_allrmb));
					 // printf("----更新数据停车计费--g_chg_car_park_id[park_index] = %d \n",g_chg_car_park_id[park_index]);
						Update_CarLock_Busy_DB(g_chg_car_park_id[park_index],&g_chg_car_park_data[park_index],1, 0x66);
					}
				}
			}
			break;
	}
}

//返回值1表示车位锁已经降下，0表示车位锁未降下---8583控制那边有用
unsigned char IsCarLockDown(unsigned char car_lock_index)
{
	if( (0x01 == g_car_st[car_lock_index].lock_st)||//地锁处于稳定的开锁状态中
		(0x10 == g_car_st[car_lock_index].lock_st))//当前开锁状态并且检测到上面无车,可以升地锁
		return 1;
	else
		return 0;	
}


//返回值1表示车位上有车，0表示无车----8583控制那边有用
unsigned char IsCarPresent(unsigned char car_lock_index)
{
	
	if( (0x00 == g_car_st[car_lock_index].lock_st)||//地锁处于稳定的升起状态中，肯定无车
		(0x10 == g_car_st[car_lock_index].lock_st))//当前开锁状态并且检测到上面无车,可以升地锁
		return 0;
	else
		return 1;		
}

//返回值1表示车位锁通信故障，0表示正常 ----8583控制那边有用
unsigned char IsLockCommFailed(unsigned char car_lock_index)
{
	if(carlock_comm_fault_flag&(0x01<<car_lock_index))
		return 1;
	else
		return 0;	
}

time_t  car_present_timestamp[2];//是否有车停入的检测
time_t  no_car_present_timestamp[2];//无车检测防误时间	

unsigned char car_parked_flag[2];//是否有车停入过车位锁内
//地锁无车时自动升起控制
//如果检测到有车停入过，车离开后则立即升起---避免车充电走后，地锁不升起，被别人停了
//如果没检测到有车停入，在达到无车升起时间后再升起地锁
char carlock_auto_up_step[2] = {0};
void carlock_auto_up(void)
{
	unsigned char i;
	
	for(i=0;i<dev_cfg.dev_para.car_lock_num;i++)
	{
		if(carlock_no_car_st[i] !=carlock_auto_up_step[i] )
		{
			carlock_auto_up_step[i]  = carlock_no_car_st[i];
		  //printf("\n carlock_auto_up[%d] = %d \n",i,carlock_no_car_st[i]);
		}
		switch(carlock_no_car_st[i])
		{
			case 0:
				if( (0x01 == g_car_st[i].lock_st) ||//地锁是放下的
					(0x03 == g_car_st[i].lock_st) ||//上升遇阻后又放下了
					(0x10 == g_car_st[i].lock_st) )//当前开锁状态并且检测到上面无车
				{
					carlock_no_car_st[i] = 1;		
				  no_car_present_timestamp[i] = time(NULL);	
					if(0x01 == g_car_st[i].lock_st)//刚放下地锁时
						car_present_timestamp[i] = time(NULL);					
				}
				car_parked_flag[i] = 0;
				break;
			case 1:
				if( 0x00 == g_car_st[i].lock_st)//车锁已上锁了
				{
					carlock_no_car_st[i] = 0;					
				}
				else//还未锁车位
				{					
					if( 0x10 == g_car_st[i].lock_st)//无车时才可升起地锁
					{
					  if(get_ticks_elapsed(no_car_present_timestamp[i]) >  2 * USER_TICKS_PER_SEC)
						{//持续2秒的检测到无车，防止误检测
							if(car_parked_flag[i])//曾经停过车，现在变无车状态时，立即升起地锁
							{
								//控制地锁升起
								carlock_ctl_cmd[i] = APP_CAR_LOCK_UP;
								carlock_last_ctl_timestamp[i] = time(NULL) - CARLOCK_CTL_PERIOD * USER_TICKS_PER_SEC;//reset timer触发立即控制
								carlock_no_car_st[i] = 0;
						//		printf("-111控制地锁升起---重置上次时间：carlock_last_ctl_timestamp[%d] =%d\n",i,carlock_last_ctl_timestamp[i]);

							}
							else//自从放下地锁后没有检测到有车停过，等待无车升起时间后升起地锁
							{
								carlock_no_car_st[i] = 2;
								carlock_no_car_timestamp[i] = time(NULL);
							}
						}
					}
					else 
					{
						no_car_present_timestamp[i] = time(NULL);	
					}						
					
					if(0x01 == g_car_st[i].lock_st)//地锁持续放下的，可能有车停入
					{
						if(get_ticks_elapsed(car_present_timestamp[i]) >  20 * USER_TICKS_PER_SEC)//持续20秒的0x01状态表明有车停入过
							car_parked_flag[i] = 1;
					}
					else
					{
					  car_present_timestamp[i] = time(NULL);					
					}						
						
				}					
				break;
			case 2://等待无车升起时间后再升起----
				if( 0x10 == g_car_st[i].lock_st)//持续无车时才可升起地锁
				{
					if(dev_cfg.dev_para.car_lock_time == 0)
						dev_cfg.dev_para.car_lock_time = 1;//至少1分钟
					
					if(get_ticks_elapsed(carlock_no_car_timestamp[i]) >  (dev_cfg.dev_para.car_lock_time *60 - CARLOCK_ULTRASONIC_FILTER_TIME) * USER_TICKS_PER_SEC)
					{
						//控制地锁升起
						carlock_ctl_cmd[i] = APP_CAR_LOCK_UP;
						carlock_last_ctl_timestamp[i] = time(NULL) - CARLOCK_CTL_PERIOD * USER_TICKS_PER_SEC;//reset timer触发立即控制
						carlock_no_car_st[i] = 0;
						//printf("222-控制地锁升起---重置上次时间：carlock_last_ctl_timestamp[%d] =%d\n",i,carlock_last_ctl_timestamp[i]);
						
					}
				}
				else
					if( 0x00 == g_car_st[i].lock_st){//车锁已上锁了
						carlock_no_car_st[i] = 0;
						//控制地锁升起
						carlock_ctl_cmd[i] = APP_CAR_LOCK_UP;
						carlock_last_ctl_timestamp[i] = time(NULL) - CARLOCK_CTL_PERIOD * USER_TICKS_PER_SEC;//reset timer触发立即控制
						carlock_no_car_st[i] = 0;
					//	printf("222-控制地锁升起---重置上次时间：carlock_last_ctl_timestamp[%d] =%d\n",i,carlock_last_ctl_timestamp[i]);
					}else{
						if( 0x01 == g_car_st[i].lock_st)//之前无车，现在有车停入了
						{
							carlock_no_car_st[i] = 1;
							car_present_timestamp[i] = time(NULL);
							no_car_present_timestamp[i] = time(NULL);	
						}
					}
						//else//
							
				break;
		}	
	}	
	//usRegInputBuf[14] = carlock_no_car_st[0];
	//usRegInputBuf[15] = carlock_no_car_st[1];
}

static void carlock_RecvInit(void)
{	
	carlock_recv_len = 0;
	carlock_recv_st = carlock_RECV_READY;
	pre_rx_timestamp = time(NULL);
	Led_Relay_Control(9, 1);
}
void carlock_ctl_check(void)
{
	
	if((((Globa_1->Car_lockCmd_Type == APP_CAR_LOCK_UP) ||(Globa_1->Car_lockCmd_Type == APP_CAR_LOCK_DOWN))&&(cur_poll_index == 0))
		||(((Globa_2->Car_lockCmd_Type == APP_CAR_LOCK_UP) ||(Globa_2->Car_lockCmd_Type == APP_CAR_LOCK_DOWN))&&(cur_poll_index == 1))
	)
		{ 
		  if (cur_poll_index == 0)
			{
			 	carlock_ctl_cmd[cur_poll_index] = Globa_1->Car_lockCmd_Type;//save ctl cmd
			  carlock_last_ctl_timestamp[cur_poll_index] = time(NULL) - CARLOCK_CTL_PERIOD * USER_TICKS_PER_SEC;//reset timer	
			  memcpy(g_park_user_id[cur_poll_index], &Globa_1->Car_lock_Card[0], sizeof(Globa_1->Car_lock_Card));//先保存一下卡号
				g_user_park_info[cur_poll_index].park_card_type = PARK_BY_APP;
		    Globa_1->Car_lockCmd_Type = 0;
				//printf("\n ---Globa_1->Car_lockCmd_Type =%d--- carlock_ctl_cmd[cur_poll_index] =%d \n",Globa_1->Car_lockCmd_Type,carlock_ctl_cmd[cur_poll_index]);
			}
			else
			{
				carlock_ctl_cmd[cur_poll_index] = Globa_2->Car_lockCmd_Type;//save ctl cmd
			  carlock_last_ctl_timestamp[cur_poll_index] =  time(NULL) - CARLOCK_CTL_PERIOD * USER_TICKS_PER_SEC;//reset timer	//reset timer	
			  memcpy(g_park_user_id[cur_poll_index], &Globa_2->Car_lock_Card[0], sizeof(Globa_2->Car_lock_Card));//先保存一下卡号
				g_user_park_info[cur_poll_index].park_card_type = PARK_BY_APP;
			  Globa_2->Car_lockCmd_Type = 0;
				//printf("\n ---Globa_2->Car_lockCmd_Type =%d--- carlock_ctl_cmd[cur_poll_index] =%d \n",Globa_2->Car_lockCmd_Type,carlock_ctl_cmd[cur_poll_index]);
			} 
		}
		else//检测是否有刷卡控制地锁
		{
			if ((((Globa_1->Car_lockCmd_Type == APP_CAR_LOCK_UP) ||(Globa_1->Car_lockCmd_Type == APP_CAR_LOCK_DOWN))&&(cur_poll_index == 0))
				||(((Globa_2->Car_lockCmd_Type == APP_CAR_LOCK_UP) ||(Globa_2->Car_lockCmd_Type == APP_CAR_LOCK_DOWN))&&(cur_poll_index == 1))
			)
				{ 
					if (cur_poll_index == 0)
					{
						carlock_ctl_cmd[cur_poll_index] = Globa_1->Car_lockCmd_Type;//save ctl cmd
						carlock_last_ctl_timestamp[cur_poll_index]  = time(NULL) - CARLOCK_CTL_PERIOD * USER_TICKS_PER_SEC;//reset timer	;//reset timer	
						memcpy(g_park_user_id[cur_poll_index], &Globa_1->Car_lock_Card[0], sizeof(Globa_1->Car_lock_Card));//先保存一下卡号
						g_user_park_info[cur_poll_index].park_card_type = PARK_BY_APP;
						Globa_1->Car_lockCmd_Type = 0;
						//printf("\n -111--Globa_1->Car_lockCmd_Type =%d--- carlock_ctl_cmd[cur_poll_index] =%d \n",Globa_1->Car_lockCmd_Type,carlock_ctl_cmd[cur_poll_index]);

					}
					else
					{
						carlock_ctl_cmd[cur_poll_index] = Globa_2->Car_lockCmd_Type;//save ctl cmd
						carlock_last_ctl_timestamp[cur_poll_index] = time(NULL) - CARLOCK_CTL_PERIOD * USER_TICKS_PER_SEC;//reset timer	;//reset timer	
						memcpy(g_park_user_id[cur_poll_index], &Globa_2->Car_lock_Card[0], sizeof(Globa_2->Car_lock_Card));//先保存一下卡号
						g_user_park_info[cur_poll_index].park_card_type = PARK_BY_APP;
						Globa_2->Car_lockCmd_Type = 0;
						//printf("\n -222--Globa_2->Car_lockCmd_Type =%d--- carlock_ctl_cmd[cur_poll_index] =%d \n",Globa_2->Car_lockCmd_Type,carlock_ctl_cmd[cur_poll_index]);

					} 
				}
		}
}

//读取锁状态：
void carlock_poll_ctl(int fd)
{
	int status =0;
	unsigned char tx_len = 0;
	unsigned char address = g_carlock_addr[cur_poll_index];
	if(carlock_RECV_IDLE == carlock_recv_st)//上1条数据已处理
	{  
		if(get_ticks_elapsed(carlock_last_ctl_timestamp[cur_poll_index]) >  CARLOCK_CTL_PERIOD * USER_TICKS_PER_SEC)//达到地锁控制时间
		{
			if(APP_CAR_LOCK_UP == carlock_ctl_cmd[cur_poll_index]){//升起地锁
				tx_len = Fill_AP01_Frame(carlock_send_buf, address,SHUT_LOCK);
			}
			//printf("cur_poll_index = %d carlock_ctl_cmd[cur_poll_index] =%d \n",cur_poll_index,carlock_ctl_cmd[cur_poll_index]);
			if(APP_CAR_LOCK_DOWN == carlock_ctl_cmd[cur_poll_index]){//放下地锁
				tx_len = Fill_AP01_Frame(carlock_send_buf, address,OPEN_LOCK);
			}
			carlock_last_ctl_timestamp[cur_poll_index] = time(NULL);
		}
		else//查询地锁状态
		{
		  if(get_ticks_elapsed(carlock_last_ctl_timestamp[cur_poll_index])%2 == 0)//达到地锁控制时间
			  tx_len = Fill_AP01_Frame(carlock_send_buf, address ,(unsigned char) GET_LOCK_ST);				
		}
		if(tx_len > 0)
		{
			carlock_RecvInit();
			Led_Relay_Control(9, 0);
			write(fd, carlock_send_buf, tx_len);
			do{
				ioctl(fd, TIOCSERGETLSR, &status);
			} while (status!=TIOCSER_TEMT);
			usleep(10000);
  		Led_Relay_Control(9, 1);
		}
	}	
		
}

void CarDataHandler(void)
{		
	if(~carlock_comm_fault_flag&0x01){
	//	usRegInputBuf[12] = g_car_st[0].lock_st;//此处修改车锁状态的数组地址
  }else{
//		usRegInputBuf[12] = 0x66;
	}
	if(~carlock_comm_fault_flag&0x02){	
	//	usRegInputBuf[13] = g_car_st[1].lock_st;//此处修改车锁状态的数组地址
	}else{
	//	usRegInputBuf[13] = 0x66;
	}
	//目前只做了两个地锁，以后添加需更改
}

//接收正常处理
void car_recv_normal_proc(void)
{
	poll_retry_cnt[cur_poll_index] = 0;
	carlock_comm_fault_flag &= ~(0x01 << cur_poll_index);
	cur_poll_index++;
	if(cur_poll_index >= dev_cfg.dev_para.car_lock_num)
		cur_poll_index = 0;//roll back
	carlock_recv_st = carlock_RECV_IDLE;
}

//接收异常处理
void car_recv_exec_proc(void)
{
	poll_retry_cnt[cur_poll_index]++;
	if(poll_retry_cnt[cur_poll_index] >= MAX_RETRY_CNT)
	{	
		carlock_comm_fault_flag |= (0x01 << cur_poll_index);
		poll_retry_cnt[cur_poll_index] = MAX_RETRY_CNT;//0;
		cur_poll_index++;
		if(cur_poll_index >= dev_cfg.dev_para.car_lock_num)
			cur_poll_index = 0;//roll back
	}	
	carlock_recv_st = carlock_RECV_IDLE;//返回空闲后才可发送下1帧数据
}


unsigned char InitCarlock(unsigned char addr,int fd)
{
	unsigned char wait_cnt = 0;
	unsigned char tx_len = 0;
  int status =0,i=0;

	//设置地锁的部分参数
	//step1----关闭蜂鸣器 ---目前没有存在
	tx_len = Fill_AP02_Frame(carlock_send_buf, addr,SET_BEEP,0);
	carlock_RecvInit();
	Led_Relay_Control(9, 0);
	write(fd, carlock_send_buf, tx_len);
	do{
		ioctl(fd, TIOCSERGETLSR, &status);
	} while (status!=TIOCSER_TEMT);
	usleep(10000);
	Led_Relay_Control(9, 1);
  usleep(50000);
	carlock_rx_len = read_datas_tty(fd, carlock_rx_data, 100, 25000, 200000);
  if(carlock_rx_len > 0)
	{
	  if( 0 != CAR_LOCK_Rx_Handler(carlock_rx_data,carlock_rx_len))//响应错误
		return 0;
	}
	//step2----设置超声波检测周期
	wait_cnt = 0;
	tx_len = Fill_AP02_Frame(carlock_send_buf, addr,SET_ULTRASONIC_CYC,CARLOCK_ULTRASONIC_CYC);
	carlock_RecvInit();
	Led_Relay_Control(9, 0);

	write(fd, carlock_send_buf, tx_len);
	do{
		ioctl(fd, TIOCSERGETLSR, &status);
	} while (status!=TIOCSER_TEMT);
	usleep(10000);
	Led_Relay_Control(9, 1);
	carlock_rx_len = read_datas_tty(fd, carlock_rx_data, 100, 25000, 200000);

 if(carlock_rx_len > 0)
	{
	  if( 0 != CAR_LOCK_Rx_Handler(carlock_rx_data,carlock_rx_len))//响应错误
		return 0;
	}
	
	//step3----设置超声波滤波时间 ---即多久无车
	wait_cnt = 0;
	tx_len = Fill_AP02_Frame(carlock_send_buf, addr,SET_ULTRASONIC_TIME,CARLOCK_ULTRASONIC_FILTER_TIME);
	carlock_RecvInit();
	
	Led_Relay_Control(9, 0);
	write(fd, carlock_send_buf, tx_len);
	do{
		ioctl(fd, TIOCSERGETLSR, &status);
	} while (status!=TIOCSER_TEMT);
	usleep(10000);
	Led_Relay_Control(9, 1);
	carlock_rx_len = read_datas_tty(fd, carlock_rx_data, 100, 25000, 200000);
  if(carlock_rx_len > 0)
	{
	  if( 0 != CAR_LOCK_Rx_Handler(carlock_rx_data,carlock_rx_len))//响应错误
		return 0;
	}
	return 1;//all done ok
}	


/*********************************************************************************************************
** Function name:           carlock_Task   
** Descriptions:            与地锁模块通信处理	
** input parameters:        *parg
** output parameters:       无      
** Returned value:          无	 
** Created by:		    	
** Created Date:	    	
**--------------------------------------------------------------------------------------------------------
** Modified by:             	
** Modified date:           	
**--------------------------------------------------------------------------------------------------------
*********************************************************************************************************/
void carlock_Task()
{		
	unsigned char address,init_retry_cnt,CAR_LOCK_Rx_Reust;
	int  fd,j;
	carlock_recv_len  = 0;
	carlock_recv_st = carlock_RECV_IDLE;
	//初始化与通信定时相关的变量

	fd = OpenDev(CARLOCK_COM_ID);
	if(fd == -1) {
		while(1);
	}else{
		set_speed(fd, 9600);
		set_Parity(fd, 8, 1, 0);
  }

	//初始化串口接收队列
	sw_fifo_init(&g_carlock_rx_fifo_ctl,carlock_RX_FIFO_WIDTH, carlock_RX_FIFO_EMS, g_carlock_rx_fifo_em, carlock_rx_buf);	

	carlock_last_ctl_timestamp[0] = 0xFFFFFFFF;
	carlock_last_ctl_timestamp[1] = 0xFFFFFFFF;//上电后可立即处理地锁控制
	
	g_carlock_addr[0] = dev_cfg.dev_para.car_lock_addr[0];
	g_carlock_addr[1] = dev_cfg.dev_para.car_lock_addr[1];
	//printf("xxxxcarlock_comm_fault_flag=%d Error.CarLock_connect_lost =%d \n", carlock_comm_fault_flag,Globa_1->Error.CarLock_connect_lost);

	 if(dev_cfg.dev_para.car_lock_num > 0)//有地锁
	{
		
		cur_poll_index = 0;
		init_retry_cnt = 0;
		address = g_carlock_addr[cur_poll_index];
		memset((void*)&g_user_park_info[cur_poll_index],0,sizeof(USER_PARK_INFO));
		memset(&g_park_user_id[cur_poll_index],0,sizeof(g_park_user_id[cur_poll_index]));
		memset(&g_chg_car_park_data[cur_poll_index],0,sizeof(g_chg_car_park_data[cur_poll_index]));
		memset(Globa_1->Car_lock_Card,0,sizeof(Globa_1->Car_lock_Card));
#if 0		
		do{
			sleep(1);
			init_retry_cnt++;
			if(init_retry_cnt > 10)//尝试10次后跳出
				break;
		}while(0==InitCarlock(address,fd));
#endif
		if(2 == dev_cfg.dev_para.car_lock_num)
		{
			init_retry_cnt = 0;
			cur_poll_index = 1;
			address = g_carlock_addr[cur_poll_index];
			memset((void*)&g_user_park_info[cur_poll_index],0,sizeof(USER_PARK_INFO));
			memset(&g_park_user_id[cur_poll_index],0,sizeof(g_park_user_id[cur_poll_index]));
			memset(&g_chg_car_park_data[cur_poll_index],0,sizeof(g_chg_car_park_data[cur_poll_index]));
			memset(Globa_2->Car_lock_Card,0,sizeof(Globa_2->Car_lock_Card));
		#if 0		
			do{
				usleep(50000);
				init_retry_cnt++;
				if(init_retry_cnt > 10)//尝试10次后跳出
					break;
			}while(0==InitCarlock(address,fd));			
			#endif
		}
	} 
	carlock_recv_st = carlock_RECV_IDLE;
	while(1)
	{	
    sleep(1);
		g_carlock_addr[0] = dev_cfg.dev_para.car_lock_addr[0];
		g_carlock_addr[1] = dev_cfg.dev_para.car_lock_addr[1];
		if(dev_cfg.dev_para.car_lock_num > 0)//有地锁
		{		
	    if(carlock_RECV_READY == carlock_recv_st)
			{//上1条数据已处理
		    memset(carlock_rx_data,0,sizeof(carlock_rx_data));
			  carlock_rx_len = read_datas_tty(fd, carlock_rx_data, 100, 25000, 200000);//格林贝尔模块响应特别快，所以等待超时和字符间隔超时分别设为25、20MS
				if(carlock_rx_len > 0)
				{
					Car_comexec_count[cur_poll_index] = 0;
					CAR_LOCK_Rx_Reust = CAR_LOCK_Rx_Handler(carlock_rx_data,carlock_rx_len);
					if( 0 == CAR_LOCK_Rx_Reust){//响应OK
						car_recv_normal_proc();
					}else{
						car_recv_exec_proc();
					}						
				}
				else
				{
					Car_comexec_count[cur_poll_index] ++;
					if(Car_comexec_count[cur_poll_index] >= 2){
						Car_comexec_count[cur_poll_index] = 0;
						car_recv_exec_proc();
					}
				}				
			}
			carlock_ctl_check();//检查是否有控制地锁请求 --ok
			carlock_poll_ctl(fd);//发送查询或控制指令
			carlock_auto_up();//无车时地锁自动升起控制
			
			CarDataHandler();//接收地锁状态
			car_park_handler(0);//车位1停车费计算
			if(2==dev_cfg.dev_para.car_lock_num)
				car_park_handler(1);//车位2停车费计算
			
			//printf("xxxxcarlock_comm_fault_flag=%d Error.CarLock_connect_lost =%d g_car_st[0].lock_st = %d\n", carlock_comm_fault_flag,Globa_1->Error.CarLock_connect_lost,g_car_st[0].lock_st);
			
			if(carlock_comm_fault_flag&0x01){//地锁1故障
				if(Globa_1->Error.CarLock_connect_lost == 0)
				{
					Globa_1->Error.CarLock_connect_lost = 1;
					sent_warning_message(0x99, 67, 1, 0);
				}
			}else{
			  if(Globa_1->Error.CarLock_connect_lost == 1)
				{
					Globa_1->Error.CarLock_connect_lost = 0;
					sent_warning_message(0x98, 67, 1, 0);
				}
			}
			if(carlock_comm_fault_flag&0x02){//地锁2故障
				if(Globa_2->Error.CarLock_connect_lost == 0)
				{
					Globa_2->Error.CarLock_connect_lost = 1;
					sent_warning_message(0x99, 67, 2, 0);
				}
			}else{
			  if(Globa_2->Error.CarLock_connect_lost == 1)
				{
					Globa_2->Error.CarLock_connect_lost = 0;
					sent_warning_message(0x98, 67, 2, 0);
				}
			}

			if(1 == dev_cfg.dev_para.car_lock_num){//无地锁2
				//usRegInputBuf[13] = 0x55;
			}
			
		}
		else
		{
      sleep(10);
		}
	}
}


extern void carlock_Thread(void)
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
	
	/* 创建自动串口抄收线程 */
	if(0 != pthread_create(&td1, &attr, (void *)carlock_Task, NULL)){
		perror("####pthread_create IC_Read_Write_Task ERROR ####\n\n");
	}

	ret = pthread_attr_destroy(&attr);
	if(ret != 0){
		return;
	}
}


//*****************************************************************************
//
// The UART interrupt handler.
//
//*****************************************************************************

