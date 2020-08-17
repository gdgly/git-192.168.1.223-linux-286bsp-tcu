/*******************************************************************************
  Copyright(C)    2014-2015,      EAST Co.Ltd.
  File name :     iso8583.c
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

#include "globalvar.h"
#include "iso8583.h"
#include "../../inc/sqlite/sqlite3.h"
#include "md5.h"  
#include "common.h"
#include "InDTU332W_Driver.h"
#include "../lock/Carlock_task.h"
#include "kernel_list.h"

unsigned char manual_key[8]={0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37};
typedef struct _Busy_dataMsgNode
{
    struct list_head glist;
    int count;
    int Busy_Need_Up_id;
    ISO8583_AP_charg_busy_Frame busy_data;
    pthread_mutex_t lock;
}Busy_dataMsgNode;

ISO8583_APP_Struct ISO8583_App_Struct;
Alarm_Info_Get    alarm_info_get;
int     AP_APP_Sent_1_falg[2] = {0};            //1号.枪APP数据反馈及反馈结果
int     AP_APP_Sent_2_falg[2] = {0};            //2号.枪APP数据反馈及反馈结果
	
extern UINT32 ISO8583_Heart_Watchdog;
extern UINT32 ISO8583_Busy_Watchdog;
UINT32  have_hart_outtime_count =0;
UINT32 curent_data_block_num = 1;
UINT32 All_Data_Num = 0;
FILE *fp_Update = NULL;
extern	unsigned char hexip[4];
extern	unsigned char hexport[2];
extern  unsigned char* server_name;
#define TEMP_OFSET  50
time_t Rcev_current_now,Rcev_pre_time;
BackgroupIssued gBackgroupIssued;

#define     InDTU332W_IDLE_ST     0
#define     SCAN_InDTU332W_ST     1 //扫描是否有GPRS模块
#define     LOGIN_InDTU332W_ST    2
#define     READ_InDTU332W_ST     3
#define     WRTIE_InDTU332W_ST    4
#define     RESET_InDTU332W_ST    5

#define     GET_InDTU332W_CONN_ST    6 //获取DTU是否连接到指定的服务器端口
#define     ACTIVE_InDTU332W_ST      7 //激活DTU模块的GPRS连接，触发美赛达DTU连接服务器

#define     WR_DNS_ADDR_ST        8//设置DNS服务器地址
#define     WR_ServIP_ADDR_ST     9//写域名地址
#define     RE_ServIP_ADDR_ST     10//写域名地址

#define     GET_InDTU332W_CSQ_ST  11//获取DTU信号强度
#define     GET_InDTU332W_CSQ_ST2 12 //激活之后再次获取DTU信号强度 
  
#define     GET_REGISTRATION_ST  13//获取DTU关于卡的注册信息
#define     GET_InDTU332W_ICCID_ST 14 //sim卡唯一对应的ID卡号20位

int MD5_DES(unsigned char *sourceData, unsigned int sourceSize, unsigned char *keyStr, unsigned char *DesData)
{
	MD5_CTX ctx;
	unsigned char dest[16], *resultData;
	unsigned int i=0, resultSize;

	//MD5
	MD5_Init(&ctx);
	MD5_Update(&ctx, sourceData, sourceSize);
	MD5_Final(dest, &ctx);
	
	for(i=1;i<8;i++){//取MD5的奇数为
		dest[i] = dest[i*2];
	}

	//DES
	resultData = (unsigned char* )DES_Encrypt(&dest, 8, keyStr, &resultSize);
	memcpy(DesData, resultData, 8);
	free(resultData);//上面DES加密返回数据是申请的动态内存，此处必须释放

	return (resultSize);
}

int DES_to_min(unsigned char *sourceData, unsigned int sourceSize, unsigned char *keyStr, unsigned char *DesData)
{
	unsigned char* resultData;
	unsigned int resultSize;

	//解密
	resultData = (unsigned char* )DES_Decrypt(sourceData, sourceSize, keyStr, &resultSize);
	memcpy(DesData, resultData, sourceSize);
	free(resultData);//上面DES解密返回数据是申请的动态内存，此处必须释放

	return (resultSize);
}

//将8字节的ASCII字符串，如"00001234"表示1234转换为16进制值
//返回16进制的数值
unsigned long convert_8ascii_2UL(unsigned char *ascii_in)
{
	unsigned char i,hex[8];
	unsigned long ul_hex=0;
	for(i=0;i<8;i++)
		hex[i] = ascii_in[i] - 0x30;	
	ul_hex = hex[0]*10000000+
			hex[1]*1000000+
			hex[2]*100000+
			hex[3]*10000+
			hex[4]*1000+
			hex[5]*100+
			hex[6]*10+
			hex[7];
	return ul_hex;	
}
unsigned char IsParkUserIDValid(unsigned char* UserID)
{
	unsigned long i = 0;
	for(i=0;i<16;i++)
		if(UserID[i] != 0)
			break;
	if(i == 16)//无效用户号
		return 0;
	else
		return 1;	
}

/*
SQLITE 的数据类型
NULL，值是NULL
INTEGER，值是有符号整形，根据值的大小以1,2,3,4,6或8字节存放
REAL，值是浮点型值，以8字节IEEE浮点数存放
TEXT，值是文本字符串，使用数据库编码（UTF-8，UTF-16BE或者UTF-16LE）存放
BLOB，只是一个数据块，完全按照输入存放（即没有准换）
*/
/*
使用下面的命令来控制事务：

· BEGIN TRANSACTION：开始事务处理。
· COMMIT：保存更改，或者可以使用 END TRANSACTION 命令。
· ROLLBACK：回滚所做的更改。
事务控制命令只与 DML 命令 INSERT、UPDATE 和 DELETE 一起使用。他们不能在创建表或删除表时使用，因为这些操作在数据库中是自动提交的。
*/
/*
三．SQLite3的线程模式
sqlite支持3种不同的线程模式：
Single-thread：这种模式下，所有的互斥被禁用，多线程使用sqlite是不安全的。
Multi-thread：这种模式下，sqlite可以安全地用于多线程，但多个线程不能共享一个database connect。
Serialized：这种模式下，sqlite可以安全地用于多线程，无限制。
 
sqlite线程模式的选择可以在编译时（当SQLitelibrary 源码被编译时）或启动时（使用SQLite的应用初始化时）或运行时（新的 database connection被创建时）。
一般来说，运行时覆盖启动时，启动时覆盖编译时，不过，Single-thread模式一旦被选择了，就不能被重改。
 
编译时设置线程模式：
用SQLITE_THREADSAFE选择线程模式，
如果SQLITE_THREADSAFE没有被设置或设置了-DSQLITE_THREADSAF=1，则是Serialized模式；
如果设置了-DSQLITE_THREADSAF=0，线程模式是Single-thread；
如果设置了-DSQLITE_THREADSAF=2，线程模式是Multi-thread.
 
启动时设置线程模式：
假设编译时线程模式被设置为非Single-thread模式，在初始化时可以调用sqlite3_config()改变线程模式，参数可以为SQLITE_CONFIG_SINGLETHREA，SQLITE_CONFIG_MULTITH，SQLITE_CONFIG_SERIALIZED.
 
运行时设置线程模式：
如果Single-thread模式在编译时和启动时都没被设置，则可以在database connections被创建时设置Multi-thread或Serialized模式，但不可以降级设置为Single-thread模式，也不可以升级编译时或启动时设置的Single-thread模式。
sqlite3_open_v2()的第三个参数决定单个databaseconnection的线程模式，SQLITE_OPEN_NOMUTEX使database connection为Multi-thread模式，SQLITE_OPEN_FULLMUTE使database connection为Serialized模式。
如果不指定模式或使用sqlite3_open()或sqlite3_open16()接口，线程模式为编译时或启动时设置的模式。


////////////////////////////////////////////////////////////////////////////////
关于编译选项的设置compile.html文档中有介绍，选项很多，慢慢看。
其中有一个SQLITE_THREADSAFE=<0 or 1 or 2>选项决定了多线程环境下的安全问题，
0 多线程不安全。
1 多线程安全。
2 可以在多线程中使用，但多线程之间不能共享database connection和任何从其派生的prepared statements。
默认SQLITE_THREADSAFE=1
如果使用的是别人编译好的dll或lib，自己不知道编译选项里设置的值，别着急，可以调用sqlite3_threadsafe()取得设置。
还可以调用sqlite3_config()修改设置。


/////////////////////////////////////////////////////////////////////////////////
这几工作需要，用到sqlite多线程功能，这几天研究了一下，验证了一下结果，供大家参考：
1、如果是SQLITE_OPEN_FULLMUTEX，也就是串行化方式，则对于连接时互斥的，只有一个连接关闭，另外一个连接才能读写
2、如果是SQLITE_OPEN_NOMUTEX，则是多线程模式，对于写是互斥的，但是如果一个连接持续写，另外一个连接是无法写入的，只能是错误或者超时返回。不过一个连接写，多个连接读，是没问题的。windows版本模式是SQLITE_OPEN_NOMUTEX
3、如果要多线程写并发，只有一个办法，就是连接->写->关闭连接，而且需要开启超时sqlite3_busy_timeout，不过这样效率很低，因为一条一条写入的话，利用不上事务
4、sqlite通过事务插入效率还可以，大约就是1000条/秒
5、sqlite的数据类型操作非常灵活，可以写入任意自定义类型
6、如果要断电等意外也完全保证数据完整性，PRAGMA synchronous=FULL，对于大数据量提交，性能和synchronous=OFF相差很小。
7、对于大数据量写入，例如一次提交100MB以上的事务，设置cache_size很有必要，默认是例如：PRAGMA cache_size=400000，有时候提交速度会成倍提升。
8、sqlite事务的insert或者update等是很快的，但是commit是很慢的，例如提交200MB的事务，在win7+酷睿T9300+4GB内存+7200转磁盘+10GB的数据库上，commit会花费5分钟左右，在usb3.0的上U盘上，花费大约15分钟，在USB2.0的U盘上，1个半小时后还没有完成（没有等到结果，因为是NTFS格式，造成U盘写入太频繁，产生了坏道，尝试了两个U盘都是如此，格式化为exFAT就没问题）
9、对于commit意外退出，大数据量的时候，例如200MB，下次再次进入的时候，哪怕只是一个select,为了保证数据完整性，sqlite都要经过很长时间（和commit完成时间差不多）的rollback才能返回select结果，如果自作主张删除临时的journal文件，则会造成数据库崩溃。

///////////////////////////////////////////////////////////////
顺带一提，要实现重试的话，可以使用sqlite3_busy_timeout()或sqlite3_busy_handler()函数。

由此可见，要想保证线程安全的话，可以有这4种方式：
SQLite使用单线程模式，用一个专门的线程访问数据库。
SQLite使用单线程模式，用一个线程队列来访问数据库，队列一次只允许一个线程执行，队列里的线程共用一个数据库连接。
SQLite使用多线程模式，每个线程创建自己的数据库连接。
SQLite使用串行模式，所有线程共用全局的数据库连接。




PRAGMA database.synchronous; 
PRAGMA database.synchronous = 0 | OFF | 1 | NORMAL | 2 | FULL;
*/
int Insert_Charger_Busy_DB(ISO8583_AP_charg_busy_Frame *frame, unsigned int *id, unsigned int up_num)
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
	rc = sqlite3_open_v2(F_CHARGER_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE |
									     SQLITE_OPEN_CREATE,
									     NULL);
	if(rc){
		//printf("Can't create F_CHARGER_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		if(DEBUGLOG) GLOBA_RUN_LogOut("%s","打开数据库失败:");  
		return -1;
	}

	sprintf(sql,"CREATE TABLE if not exists Charger_Busy_Complete_Data(ID INTEGER PRIMARY KEY autoincrement, card_sn TEXT, flag TEXT, rmb TEXT, kwh TEXT, server_rmb TEXT,total_rmb TEXT, type TEXT, Serv_Type TEXT, Busy_SN TEXT, last_rmb TEXT, ter_sn TEXT, start_kwh TEXT, end_kwh TEXT, rmb_kwh TEXT, sharp_rmb_kwh TEXT, sharp_allrmb TEXT, peak_rmb_kwh TEXT, peak_allrmb TEXT, flat_rmb_kwh TEXT, flat_allrmb TEXT, valley_rmb_kwh TEXT, valley_allrmb TEXT,sharp_rmb_kwh_Serv TEXT, sharp_allrmb_Serv TEXT, peak_rmb_kwh_Serv TEXT, peak_allrmb_Serv TEXT, flat_rmb_kwh_Serv TEXT, flat_allrmb_Serv TEXT, valley_rmb_kwh_Serv TEXT, valley_allrmb_Serv TEXT,end_time TEXT, car_sn TEXT, start_time TEXT, ConnectorId TEXT, End_code TEXT, up_num INTEGER);");
			
	if(sqlite3_exec(db, sql, 0, 0, &zErrMsg) != SQLITE_OK){
		//printf("CREATE TABLE Charger_Busy_Complete_Data error: %s\n", sqlite3_errmsg(db));
		sqlite3_free(zErrMsg);
	}

	if(TRANSACTION_FLAG) sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);
	
	sprintf(sql, "INSERT INTO Charger_Busy_Complete_Data VALUES (NULL, '%.16s', '%.2s', '%.8s', '%.8s', '%.8s', '%.8s', '%.2s', '%.2s', '%.20s', '%.8s', '%.12s', '%.8s', '%.8s', '%.8s', '%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.8s','%.14s', '%.17s', '%.14s', '%.2s', '%.2s', '%d');",frame->card_sn, frame->flag, frame->rmb, frame->kwh, frame->server_rmb, frame->total_rmb, frame->type, frame->Serv_Type, frame->Busy_SN, frame->last_rmb, frame->ter_sn, frame->start_kwh,frame->end_kwh, frame->rmb_kwh, frame->sharp_rmb_kwh, frame->sharp_allrmb, frame->peak_rmb_kwh, frame->peak_allrmb, frame->flat_rmb_kwh, frame->flat_allrmb,frame->valley_rmb_kwh, frame->valley_allrmb,frame->sharp_rmb_kwh_Serv, frame->sharp_allrmb_Serv,frame->peak_rmb_kwh_Serv, frame->peak_allrmb_Serv, frame->flat_rmb_kwh_Serv, frame->flat_allrmb_Serv,frame->valley_rmb_kwh_Serv, frame->valley_allrmb_Serv,frame->end_time, frame->car_sn,frame->start_time, frame->ConnectorId, frame->End_code, up_num);
  

 	if(DEBUGLOG) GLOBA_RUN_LogOut("插入正式数据库：%s",sql);  
	if(sqlite3_exec(db , sql , 0 , 0 , &zErrMsg) != SQLITE_OK){
		printf("Insert DI error: %s\n", sqlite3_errmsg(db));
		sqlite3_free(zErrMsg);
		if(TRANSACTION_FLAG) sqlite3_exec(db, "rollback transaction", NULL, NULL, NULL);
	  //if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
		if(DEBUGLOG) GLOBA_RUN_LogOut("%s %s","插入命令执行失败枪号:",frame->ConnectorId);  
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
			if(DEBUGLOG) GLOBA_RUN_LogOut("插入数据库ID号：%d",*id);  
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

int Update_Charger_Busy_DB(unsigned int id, ISO8583_AP_charg_busy_Frame *frame, unsigned int flag, unsigned int up_num)
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

	rc = sqlite3_open_v2(F_CHARGER_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE |
									     SQLITE_OPEN_CREATE,
									     NULL);
	if(rc){
		printf("Can't create F_CHARGER_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}
	
	sprintf(sql,"CREATE TABLE if not exists Charger_Busy_Complete_Data(ID INTEGER PRIMARY KEY autoincrement, card_sn TEXT, flag TEXT, rmb TEXT, kwh TEXT, server_rmb TEXT,total_rmb TEXT, type TEXT, Serv_Type TEXT, Busy_SN TEXT, last_rmb TEXT, ter_sn TEXT, start_kwh TEXT, end_kwh TEXT, rmb_kwh TEXT, sharp_rmb_kwh TEXT, sharp_allrmb TEXT, peak_rmb_kwh TEXT, peak_allrmb TEXT, flat_rmb_kwh TEXT, flat_allrmb TEXT, valley_rmb_kwh TEXT, valley_allrmb TEXT,sharp_rmb_kwh_Serv TEXT, sharp_allrmb_Serv TEXT, peak_rmb_kwh_Serv TEXT, peak_allrmb_Serv TEXT, flat_rmb_kwh_Serv TEXT, flat_allrmb_Serv TEXT, valley_rmb_kwh_Serv TEXT, valley_allrmb_Serv TEXT,end_time TEXT, car_sn TEXT, start_time TEXT, ConnectorId TEXT, End_code TEXT, up_num INTEGER);");
			 
	if(sqlite3_exec(db, sql, 0, 0, &zErrMsg) != SQLITE_OK){
		//printf("CREATE TABLE Charger_Busy_Complete_Data error: %s\n", sqlite3_errmsg(db));
		sqlite3_free(zErrMsg);
	}
	if(TRANSACTION_FLAG) sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);

	if((flag == 1)&&(id > 0)){//更新 充电电量 充电金额 电表结束示数 充电结束时间,传输标志

	 // memcpy(&car_sn[0], &frame->car_sn, sizeof(frame->car_sn));
		sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET rmb = '%.8s', kwh = '%.8s', rmb_kwh = '%.8s', server_rmb = '%.8s', total_rmb = '%.8s', sharp_allrmb= '%.8s', peak_allrmb= '%.8s', flat_allrmb= '%.8s', valley_allrmb= '%.8s', sharp_allrmb_Serv= '%.8s', peak_allrmb_Serv= '%.8s', flat_allrmb_Serv= '%.8s', valley_allrmb_Serv= '%.8s',start_kwh = '%.8s',end_kwh = '%.8s', end_time = '%.14s',End_code = '%.2s',up_num = '%d' WHERE ID = '%d' ;",frame->rmb, frame->kwh, frame->rmb_kwh, frame->server_rmb, frame->total_rmb, frame->sharp_allrmb, frame->peak_allrmb, frame->flat_allrmb,frame->valley_allrmb, frame->sharp_allrmb_Serv, frame->peak_allrmb_Serv, frame->flat_allrmb_Serv,frame->valley_allrmb_Serv, frame->start_kwh,frame->end_kwh, frame->end_time,frame->End_code, up_num, id);
 		if(sqlite3_exec(db, sql, 0, 0, &zErrMsg) != SQLITE_OK){
  		printf("UPDATE Charger_Busy_Complete_Data error: %s\n", sqlite3_errmsg(db));
  		sqlite3_free(zErrMsg);
		if(TRANSACTION_FLAG)	sqlite3_exec(db, "rollback transaction", NULL, NULL, NULL);
	   //if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
			sqlite3_close(db);
			if(DEBUGLOG) GLOBA_RUN_LogOut("%s %s","充电中更新数据失败:",sql);  
			return -1;
  	}
  }else if((flag == 2)&&(id > 0)){//更新 交易 传输标志
	//	printf("ISO8583 更新 交易 传输标志 frame->car_sn[0] = %d %d\n",frame->car_sn[0],frame->car_sn[1]);
	  if((frame->car_sn[0] == 0xFF)&&(frame->car_sn[1] == 0xFF)&&(frame->car_sn[2] == 0xFF)){
		  sprintf(car_sn,"%s","00000000000000000");
		}else{
      memcpy(&car_sn, &frame->car_sn, sizeof(car_sn)-1);
			//sprintf(car_sn,"%17.s",&frame->car_sn);
		//  printf("ISO8583 更新 交易 传输标志 car_sn = %s\n",car_sn);
		}
  	sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET flag = '%.2s',End_code = '%.2s',car_sn = '%.17s', up_num = '%d' WHERE ID = '%d';", frame->flag,frame->End_code,car_sn,up_num, id);
		//printf("ISO8583 更新 交易 传输标志 = %s\n",sql);

		if(sqlite3_exec(db, sql, 0, 0, &zErrMsg) != SQLITE_OK){
  		printf("Insert DI error: %s\n", sqlite3_errmsg(db));
  		sqlite3_free(zErrMsg);
			if(TRANSACTION_FLAG) sqlite3_exec(db, "rollback transaction", NULL, NULL, NULL);
	    //if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
			sqlite3_close(db);
			if(DEBUGLOG) GLOBA_RUN_LogOut("%s %s","充电完成更新数据失败:",sql);  
			return -1;
  	}
  }
	if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
	sqlite3_close(db);
	return 0;
}


//查询没有流水号的最大记录 ID 值
int Select_NO_Busy_SN(int type)
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
  if (type == 0){
		rc = sqlite3_open_v2(F_CHARGER_BUSY_COMPLETE_DB, &db,
												 SQLITE_OPEN_NOMUTEX|
												 SQLITE_OPEN_SHAREDCACHE|
												 SQLITE_OPEN_READWRITE,
												 NULL);
	} 
	else //停车业务流水号
	{
		rc = sqlite3_open_v2(F_CarLock_Busy_DB, &db,
												 SQLITE_OPEN_NOMUTEX|
												 SQLITE_OPEN_SHAREDCACHE|
												 SQLITE_OPEN_READWRITE,
												 NULL);
	}
	if(rc){
		//printf("Can't create F_CHARGER_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}
  if (type == 0)
	{
	   sprintf(sql, "SELECT MAX(ID) FROM Charger_Busy_Complete_Data WHERE Busy_SN = '00000000000000000000' AND card_sn != '0000000000000000' AND  up_num = 0 ;");
	}
	else  //停车业务流水号
	{
		sprintf(sql, "SELECT MAX(ID) FROM CarLock_Busy_Data WHERE Busy_SN = '00000000000000000000' AND  up_num = 0 ;");
	}
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

//添加充电记录的流水号
int Set_Busy_SN(int id, unsigned char *Busy_SN,int Busy_SN_Type)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;

	char sql[512];
  if (Busy_SN_Type == 0)
  {
		
		rc = sqlite3_open_v2(F_CHARGER_BUSY_COMPLETE_DB, &db,
												 SQLITE_OPEN_NOMUTEX|
												 SQLITE_OPEN_SHAREDCACHE|
												 SQLITE_OPEN_READWRITE,
												 NULL);
	}
	else
	{
		rc = sqlite3_open_v2(F_CarLock_Busy_DB, &db,
												 SQLITE_OPEN_NOMUTEX|
												 SQLITE_OPEN_SHAREDCACHE|
												 SQLITE_OPEN_READWRITE,
												 NULL);
	}		
	if(rc){
		//printf("Can't create F_CHARGER_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	if(TRANSACTION_FLAG) sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);
  
	if (Busy_SN_Type == 0)
  {
	  sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET Busy_SN = '%.20s' WHERE ID = '%d';", Busy_SN, id);
	}
	else 
	{
		sprintf(sql, "UPDATE CarLock_Busy_Data SET Busy_SN = '%.20s' WHERE ID = '%d';", Busy_SN, id);
	}
	
	if(sqlite3_exec(db , sql , 0, 0, &zErrMsg) != SQLITE_OK){
		sqlite3_free(zErrMsg);
		if (Busy_SN_Type == 0)
		{
		  if(DEBUGLOG) GLOBA_RUN_LogOut("添加流水号:%.20s 对应的数据库ID号：%d",Busy_SN,id);  
		}
		else 
		{
			if(DEBUGLOG) CARLOCK_RUN_LogOut("添加流水号:%.20s 对应的数据库ID号：%d",Busy_SN,id);  
		}
	  if(TRANSACTION_FLAG)	sqlite3_exec(db, "rollback transaction", NULL, NULL, NULL);
	  //if(TRANSACTION_FLAG)sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
		sqlite3_close(db);
		return -1;
	}
	
	if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);

	sqlite3_close(db);
	return 0;
}

//查询还没有上传成功的记录 ID最大值
int Select_Busy_Need_Up(ISO8583_AP_charg_busy_Frame *frame, unsigned int *up_num)//查询需要上传的业务记录的最大 ID 值
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;

	int id = 0;

	char sql[512];


	rc = sqlite3_open_v2(F_CHARGER_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		//printf("Can't create F_CHARGER_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	sprintf(sql, "SELECT MAX(ID) FROM Charger_Busy_Complete_Data WHERE Busy_SN != '00000000000000000000' AND card_sn != '0000000000000000' AND up_num < '%d' ;", MAX_NUM_BUSY_UP_TIME);
	
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
			sqlite3_free_table(azResult);
		}
	}else{
		sqlite3_free_table(azResult);
	}

	sqlite3_close(db);
	return -1;
}

//查询到有需要上传的交易记录插入到队列中
int Select_Busy_Need_Up_intoMsgNode(Busy_dataMsgNode *Busy_dataMsgList)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0,i =0;
	char **azResult = NULL;

	int id = 0;

	char sql[512];
  ISO8583_AP_charg_busy_Frame *frame = calloc(1,sizeof(ISO8583_AP_charg_busy_Frame));
  
	if(frame == NULL)//表示申请空间异常，
	{
		return -1;
	}
	rc = sqlite3_open_v2(F_CHARGER_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		//printf("Can't create F_CHARGER_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		free(frame);
		return -1;
	}

	sprintf(sql, "SELECT * FROM Charger_Busy_Complete_Data WHERE Busy_SN != '00000000000000000000' AND card_sn != '0000000000000000' AND up_num < '%d' ;", MAX_NUM_BUSY_UP_TIME);
	
	if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL)){
		//printf("nrow = %d,ncolumn =%d \n",nrow,ncolumn);
	  for(i =0;i<nrow;i++){
		  if(azResult[ncolumn*1+0 + ncolumn*i] != NULL){//2行1列
			  memset(frame,0,sizeof(ISO8583_AP_charg_busy_Frame));
			  id = atoi(azResult[ncolumn*1+ 0 + ncolumn*i]);
				memcpy(&frame->card_sn[0], azResult[ncolumn*1+1 + ncolumn*i], sizeof(frame->card_sn));
				memcpy(&frame->flag[0], azResult[ncolumn*1+2 + ncolumn*i], sizeof(frame->flag));
				memcpy(&frame->rmb[0], azResult[ncolumn*1+3 + ncolumn*i], sizeof(frame->rmb));
				memcpy(&frame->kwh[0], azResult[ncolumn*1+4 + ncolumn*i], sizeof(frame->kwh));
				memcpy(&frame->server_rmb[0], azResult[ncolumn*1+5 + ncolumn*i], sizeof(frame->server_rmb));
				memcpy(&frame->total_rmb[0], azResult[ncolumn*1+6 + ncolumn*i], sizeof(frame->total_rmb));
				memcpy(&frame->type[0], azResult[ncolumn*1+7 + ncolumn*i], sizeof(frame->type));
				memcpy(&frame->Serv_Type[0], azResult[ncolumn*1+8 + ncolumn*i], sizeof(frame->Serv_Type));
				memcpy(&frame->Busy_SN[0], azResult[ncolumn*1+9 + ncolumn*i], sizeof(frame->Busy_SN));
				memcpy(&frame->last_rmb[0], azResult[ncolumn*1+10 + ncolumn*i], sizeof(frame->last_rmb));
				memcpy(&frame->ter_sn[0], azResult[ncolumn*1+11 + ncolumn*i], sizeof(frame->ter_sn));
				memcpy(&frame->start_kwh[0], azResult[ncolumn*1+12 + ncolumn*i], sizeof(frame->start_kwh));
				memcpy(&frame->end_kwh[0], azResult[ncolumn*1+13 + ncolumn*i], sizeof(frame->end_kwh));
				memcpy(&frame->rmb_kwh[0], azResult[ncolumn*1+14 + ncolumn*i], sizeof(frame->rmb_kwh));

			
				memcpy(&frame->sharp_rmb_kwh[0], azResult[ncolumn*1+15 + ncolumn*i], sizeof(frame->sharp_rmb_kwh));
				memcpy(&frame->sharp_allrmb[0], azResult[ncolumn*1+16 + ncolumn*i], sizeof(frame->sharp_allrmb));
				memcpy(&frame->peak_rmb_kwh[0], azResult[ncolumn*1+17 + ncolumn*i], sizeof(frame->peak_rmb_kwh));
				memcpy(&frame->peak_allrmb[0], azResult[ncolumn*1+18 + ncolumn*i], sizeof(frame->peak_allrmb));
				memcpy(&frame->flat_rmb_kwh[0], azResult[ncolumn*1+19 + ncolumn*i], sizeof(frame->flat_rmb_kwh));
				memcpy(&frame->flat_allrmb[0], azResult[ncolumn*1+20 + ncolumn*i], sizeof(frame->flat_allrmb));
				memcpy(&frame->valley_rmb_kwh[0], azResult[ncolumn*1+21 + ncolumn*i], sizeof(frame->valley_rmb_kwh));
				memcpy(&frame->valley_allrmb[0], azResult[ncolumn*1+22 + ncolumn*i], sizeof(frame->valley_allrmb));
				
				memcpy(&frame->sharp_rmb_kwh_Serv[0], azResult[ncolumn*1+23 + ncolumn*i], sizeof(frame->sharp_rmb_kwh_Serv));
				memcpy(&frame->sharp_allrmb_Serv[0], azResult[ncolumn*1+24 + ncolumn*i], sizeof(frame->sharp_allrmb_Serv));
				memcpy(&frame->peak_rmb_kwh_Serv[0], azResult[ncolumn*1+25 + ncolumn*i], sizeof(frame->peak_rmb_kwh_Serv));
				memcpy(&frame->peak_allrmb_Serv[0], azResult[ncolumn*1+26 + ncolumn*i], sizeof(frame->peak_allrmb_Serv));
				memcpy(&frame->flat_rmb_kwh_Serv[0], azResult[ncolumn*1+27 + ncolumn*i], sizeof(frame->flat_rmb_kwh_Serv));
				memcpy(&frame->flat_allrmb_Serv[0], azResult[ncolumn*1+28 + ncolumn*i], sizeof(frame->flat_allrmb_Serv));
				memcpy(&frame->valley_rmb_kwh_Serv[0], azResult[ncolumn*1+29 + ncolumn*i], sizeof(frame->valley_rmb_kwh_Serv));
				memcpy(&frame->valley_allrmb_Serv[0], azResult[ncolumn*1+30 + ncolumn*i], sizeof(frame->valley_allrmb_Serv));
				
				memcpy(&frame->end_time[0], azResult[ncolumn*1+31 + ncolumn*i], sizeof(frame->end_time));
				memcpy(&frame->car_sn[0], azResult[ncolumn*1+32 + ncolumn*i], sizeof(frame->car_sn));
				memcpy(&frame->start_time[0], azResult[ncolumn*1+33 + ncolumn*i], sizeof(frame->start_time));
				memcpy(&frame->ConnectorId[0], azResult[ncolumn*1+34 + ncolumn*i], sizeof(frame->ConnectorId));
				memcpy(&frame->End_code[0], azResult[ncolumn*1+35 + ncolumn*i], sizeof(frame->End_code));

        pthread_mutex_lock(&Busy_dataMsgList->lock);
        Busy_dataMsgNode *node = (Busy_dataMsgNode *)calloc(1, sizeof(Busy_dataMsgNode));
        if (node)
        {
						node->Busy_Need_Up_id = id;
            memcpy(&node->busy_data, frame, sizeof(ISO8583_AP_charg_busy_Frame));
            list_add_tail(&node->glist, &Busy_dataMsgList->glist);
            Busy_dataMsgList->count++;
						//printf("Busy_dataMsgList->count = %d,node->Busy_Need_Up_id =%d \n",Busy_dataMsgList->count,node->Busy_Need_Up_id);
				  	//printf("node->busy_data.start_time = %.14s,node->busy_data.end_time =%.14s \n",node->busy_data.start_time,node->busy_data.end_time);
        }
        pthread_mutex_unlock(&Busy_dataMsgList->lock);
		  }
		}
	}
	sqlite3_free_table(azResult);
  sqlite3_close(db);
	free(frame);
	return 0;
}

int Set_Busy_UP_num(int id, unsigned int up_num)//修改数据库该条充电记录的上传标志--//并清除锁卡记录
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;

	char sql[512];

	rc = sqlite3_open_v2(F_CHARGER_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		//printf("Can't create F_CHARGER_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	if(TRANSACTION_FLAG) sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);

	sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET up_num = '%d',flag = '00' WHERE ID = '%d';", up_num, id);
	if(sqlite3_exec(db , sql , 0, 0, &zErrMsg) != SQLITE_OK){
		sqlite3_free(zErrMsg);
		
		if(TRANSACTION_FLAG) sqlite3_exec(db, "rollback transaction", NULL, NULL, NULL);
	 // if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
		sqlite3_close(db);
		return -1;
	}
	
	if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
	sqlite3_close(db);
	return 0;
}


//查询没有该卡号锁卡记录最大记录 ID 值，有则读出相应的数据
int Select_Unlock_Info()
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int cnt=0;
	int nrow = 0, ncolumn = 0;
	char **azResult = NULL;
	int i,j;
	int id;
	char sql[800];
	UINT8 temp_char[50];

	rc = sqlite3_open_v2(F_CHARGER_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		sqlite3_close(db);
		Globa_1->User_Card_Lock_Info.Local_Data_Baseid_ID = 0;
		Globa_1->User_Card_Lock_Info.Deduct_money = 0;
		memset(&Globa_1->User_Card_Lock_Info.busy_SN[0], '0', sizeof(Globa_1->User_Card_Lock_Info.busy_SN));
		Globa_1->User_Card_Lock_Info.Card_unlock_Query_Results = 4; //查询成功有记录

		return -1;
	}
  
	sprintf(&temp_char[0], "%02d%02d%02d%02d%02d%02d%02d", Globa_1->User_Card_Lock_Info.card_lock_time.year_H, Globa_1->User_Card_Lock_Info.card_lock_time.year_L, Globa_1->User_Card_Lock_Info.card_lock_time.month, Globa_1->User_Card_Lock_Info.card_lock_time.day_of_month , Globa_1->User_Card_Lock_Info.card_lock_time.hour, Globa_1->User_Card_Lock_Info.card_lock_time.minute,Globa_1->User_Card_Lock_Info.card_lock_time.second);
	//sprintf(sql, "SELECT MAX(ID) FROM Charger_Busy_Complete_Data WHERE Busy_SN = '00000000000000000000' AND  flag = '01' AND  up_num < '%d' AND card_sn = '%.16s' AND start_time = '%.14s';", MAX_NUM_BUSY_UP_TIME,Globa_1->card_sn,temp_char);
  sprintf(sql, "SELECT MAX(ID) FROM Charger_Busy_Complete_Data WHERE  flag = '01' AND  up_num < '%d' AND card_sn = '%.16s' AND start_time = '%.14s';", 0xAA,Globa_1->card_sn,temp_char);
	if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL)){
		if(azResult[ncolumn*1+0] != NULL){//2行1列
			id = atoi(azResult[ncolumn*1+0]);
			//id = atoi(azResult[ncolumn*1+1]);

			sqlite3_free_table(azResult);
			sprintf(sql, "SELECT * FROM Charger_Busy_Complete_Data WHERE ID = '%d';", id);
			if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL)){
				if(azResult[ncolumn*1+0] != NULL){//2行1列
					id = atoi(azResult[ncolumn*1+0]);
					Globa_1->User_Card_Lock_Info.Local_Data_Baseid_ID = id;
					Globa_1->User_Card_Lock_Info.Deduct_money = atoi(azResult[ncolumn*1+6]);
					memcpy(&Globa_1->User_Card_Lock_Info.busy_SN[0], azResult[ncolumn*1+9], sizeof(Globa_1->User_Card_Lock_Info.busy_SN));
					Globa_1->User_Card_Lock_Info.Card_unlock_Query_Results = 1; //查询成功有记录
					sqlite3_free_table(azResult);
					sqlite3_close(db);
					return (0);
				}else{
					sqlite3_free_table(azResult);
				}
			}else{
				sqlite3_free_table(azResult);
			}
			Globa_1->User_Card_Lock_Info.Local_Data_Baseid_ID = 0;
			Globa_1->User_Card_Lock_Info.Deduct_money = 0;
		  memset(&Globa_1->User_Card_Lock_Info.busy_SN[0], '0', sizeof(Globa_1->User_Card_Lock_Info.busy_SN));
			Globa_1->User_Card_Lock_Info.Card_unlock_Query_Results = 4; //查询成功有记录
			return (-1);
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
	Globa_1->User_Card_Lock_Info.Local_Data_Baseid_ID = 0;
	Globa_1->User_Card_Lock_Info.Deduct_money = 0;
  memset(&Globa_1->User_Card_Lock_Info.busy_SN[0], '0', sizeof(Globa_1->User_Card_Lock_Info.busy_SN));
	Globa_1->User_Card_Lock_Info.Card_unlock_Query_Results = 4; //查询成功有记录
	return -1;
}

//清除锁卡记录
int Set_Unlock_flag(int id)//修改数据库该条充电记录的上传标志--//并清除锁卡记录
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	char **azResult = NULL;
	char sql[512];

	rc = sqlite3_open_v2(F_CHARGER_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		//printf("Can't create F_CHARGER_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	if(TRANSACTION_FLAG) sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);
	sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET flag = '00' WHERE ID = '%d';", id);
	if(sqlite3_exec(db , sql , 0, 0, &zErrMsg) != SQLITE_OK){
		sqlite3_free(zErrMsg);
		if(TRANSACTION_FLAG) sqlite3_exec(db, "rollback transaction", NULL, NULL, NULL);
		//if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
		sqlite3_close(db);
		return -1;
	}
	if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
	sqlite3_close(db);
	return 0;
}


//查询有没有正常结束的
int Select_NO_Over_Record(int id,int up_num)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;


	char sql[512];

	rc = sqlite3_open_v2(F_CHARGER_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		//printf("Can't create F_CHARGER_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	//sprintf(sql, "SELECT MAX(ID) FROM Charger_Busy_Complete_Data WHERE id = '%d' AND up_num = '%d' OR up_num = '%d';",id,85,102);
	//sprintf(sql, "SELECT MAX(ID) FROM Charger_Busy_Complete_Data WHERE id = '%d' AND (up_num = 85 OR up_num = 102);",id);
	sprintf(sql, "SELECT MAX(ID) FROM Charger_Busy_Complete_Data WHERE id = '%d' AND up_num = '%d';",id,up_num);

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

//查询交易记录中存储的数据金额电量值
int Select_Busy_ID_Data(int id,int*kwh,int*rmb,int *user_id)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	char sql[512];


	rc = sqlite3_open_v2(F_CHARGER_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		//printf("Can't create F_CHARGER_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	sprintf(sql, "SELECT * FROM Charger_Busy_Complete_Data WHERE ID = '%d' AND card_sn = '%.16s';", id,user_id);
	if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL)){
		if(azResult[ncolumn*1+0] != NULL){//2行1列
			//memcpy(&frame->rmb[0], azResult[ncolumn*1+3], sizeof(frame->rmb));
			//memcpy(&kwh[0], azResult[ncolumn*1+4], sizeof(frame->kwh));
			//memcpy(&frame->total_rmb[0], azResult[ncolumn*1+6], sizeof(frame->total_rmb));
			*rmb = atoi(azResult[ncolumn*1+6]);
			*kwh = atoi(azResult[ncolumn*1+4]);
			sqlite3_free_table(azResult);
			sqlite3_close(db);
			return (0);
		}else{
			sqlite3_free_table(azResult);
		}
	}else{
		sqlite3_free_table(azResult);
	}

	sqlite3_close(db);
	return -1;
}

int Select_Busy_BCompare_ID(ISO8583_AP_charg_busy_Frame *frame)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	char sql[512];


	rc = sqlite3_open_v2(F_CHARGER_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		printf("Can't create F_CHARGER_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	sprintf(sql, "SELECT MAX(ID) FROM Charger_Busy_Complete_Data WHERE start_time = '%.14s' AND end_time = '%.14s' AND card_sn = '%.16s' AND ConnectorId = '%.2s';",frame->start_time,frame->end_time,frame->card_sn,frame->ConnectorId);
	if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL)){
		if(azResult[ncolumn*1+0] != NULL){//2行1列
			//memcpy(&frame->rmb[0], azResult[ncolumn*1+3], sizeof(frame->rmb));
			//memcpy(&kwh[0], azResult[ncolumn*1+4], sizeof(frame->kwh));
			//memcpy(&frame->total_rmb[0], azResult[ncolumn*1+6], sizeof(frame->total_rmb));
			//rmb = atoi(azResult[ncolumn*1+6]);
			//kwh = atoi(azResult[ncolumn*1+4]);
			sqlite3_free_table(azResult);
			sqlite3_close(db);
			return (0);
		}else{
			sqlite3_free_table(azResult);
		}
	}else{
		sqlite3_free_table(azResult);
	}

	sqlite3_close(db);
	return -1;
} 

static int Set_0XB4_Busy_ID()
{ 
  sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;

	char sql[512];
	rc = sqlite3_open_v2(F_CHARGER_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		printf("Can't create F_CHARGER_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}
	sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);
	sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET up_num = '%d' WHERE up_num = 180;", 0x00);
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

static int Set_CAR_CLEAR_Busy_ID()
{ 
  sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;

	char sql[512];
	rc = sqlite3_open_v2(F_CarLock_Busy_DB, &db,
												 SQLITE_OPEN_NOMUTEX|
												 SQLITE_OPEN_SHAREDCACHE|
												 SQLITE_OPEN_READWRITE,
												 NULL);
	if(rc){
		printf("Can't create CarLock_Busy_Data database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}
	sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);
	sprintf(sql, "UPDATE CarLock_Busy_Data SET up_num = '%d' WHERE up_num = 85 OR up_num = 102 OR up_num = 180;", 0x00);
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

//停车业务
//查询还没有上传成功的记录 ID最大值
int Select_Car_Stop_Busy_Need_Up(ISO8583_Car_Stop_busy_Struct *frame, unsigned int *up_num)//查询需要上传的业务记录的最大 ID 值
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;

	int id = 0;

	char sql[512];


	rc = sqlite3_open_v2(F_CarLock_Busy_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		//printf("Can't create F_CarLock_Busy_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	sprintf(sql, "SELECT MAX(ID) FROM CarLock_Busy_Data WHERE Busy_SN != '00000000000000000000' AND  up_num < '%d' ;", MAX_NUM_BUSY_UP_TIME);
	if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL)){
		if(azResult[ncolumn*1+0] != NULL){//2行1列
			id = atoi(azResult[ncolumn*1+0]);
			sqlite3_free_table(azResult);
			sprintf(sql, "SELECT * FROM CarLock_Busy_Data WHERE ID = '%d';", id);
			if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL)){
				if(azResult[ncolumn*1+0] != NULL){//2行1列
					id = atoi(azResult[ncolumn*1+0]);
					memcpy(&frame->User_Card[0], azResult[ncolumn*1+1], sizeof(frame->User_Card));
					memcpy(&frame->falg[0], azResult[ncolumn*1+2], sizeof(frame->falg));
					memcpy(&frame->charge_type[0], azResult[ncolumn*1+3], sizeof(frame->charge_type));
					memcpy(&frame->charge_BSN[0], azResult[ncolumn*1+4], sizeof(frame->charge_BSN));
					memcpy(&frame->stop_car_allrmb[0], azResult[ncolumn*1+5], sizeof(frame->stop_car_allrmb));
					memcpy(&frame->stop_car_start_time[0], azResult[ncolumn*1+6], sizeof(frame->stop_car_start_time));
					memcpy(&frame->stop_car_end_time[0], azResult[ncolumn*1+7], sizeof(frame->stop_car_end_time));
					memcpy(&frame->stop_car_rmb_price[0], azResult[ncolumn*1+8], sizeof(frame->stop_car_rmb_price));
					memcpy(&frame->charge_last_rmb[0], azResult[ncolumn*1+9], sizeof(frame->charge_last_rmb));
					memcpy(&frame->ter_sn[0], azResult[ncolumn*1+10], sizeof(frame->ter_sn));
					*up_num = atoi(azResult[ncolumn*1+12]);
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

int Set_Car_Stop_Busy_UP_num(int id, unsigned int up_num)//修改数据库该条充电记录的上传标志--//并清除锁卡记录
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;

	char sql[512];

	rc = sqlite3_open_v2(F_CarLock_Busy_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		//printf("Can't create F_CHARGER_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	if(TRANSACTION_FLAG) sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);
	
	sprintf(sql, "UPDATE CarLock_Busy_Data SET up_num = '%d' WHERE ID = '%d';",up_num, id);
	if(sqlite3_exec(db , sql , 0, 0, &zErrMsg) != SQLITE_OK){
		sqlite3_free(zErrMsg);
		printf("\n  ---停车业务更新上传失败--- up_num =%0x \n",up_num);
		if(TRANSACTION_FLAG) sqlite3_exec(db, "rollback transaction", NULL, NULL, NULL);
	 // if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
		sqlite3_close(db);
		return -1;
	}
	
	if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
	sqlite3_close(db);
	return 0;
}
//查询有没有正常结束的
int Select_Car_Stop_NO_Over_Record(int id,int up_num)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;


	char sql[512];

	rc = sqlite3_open_v2(F_CarLock_Busy_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		//printf("Can't create F_CHARGER_BUSY_COMPLETE_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	sprintf(sql, "SELECT MAX(ID) FROM CarLock_Busy_Data WHERE id = '%d' AND (up_num = 85 OR up_num = 102) AND up_num != '%d';",id,up_num);
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


//设置主位元比特值为1
static void SetPrimaryBitmap(unsigned char* Bitmap,unsigned char bit_index)
{	
	unsigned char byte_index,bit_in_byte;
	if((bit_index >= 1) && (bit_index <= 64))
	{
		byte_index = (bit_index - 1)/8;
		bit_in_byte = (bit_index - 1)%8;
		
		Bitmap[byte_index] |=  (0x80 >> bit_in_byte);
	}	
}


int ISO8583_Sent_Data(int fd, unsigned char *SendBuf, int length,int falg)
{
	int len = 0;
	if(Globa_1->Charger_param.Serial_Network !=0){
    len = write(fd,SendBuf,length); 
		usleep(200000);
	}else{
		len = send(fd, SendBuf, length,falg);
		usleep(100000);
	}
	return len;
}
//获取流水号 发送上行数据
int ISO8583_AP_Get_SN_Sent(int fd)// 充电终端业务流水号获取
{
	unsigned int ret,len;
	unsigned char SendBuf[512];  //发送数据区指针
	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针

	time_t systime=0;
	struct tm tm_t;

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_AP_Get_SN_Frame *AP_Frame;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	AP_Frame = (ISO8583_AP_Get_SN_Frame *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据填充
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'

	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Get_SN_Frame))>>8;  //报文长度
	Head_Frame->SessionLen[1] =  sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Get_SN_Frame);

	memcpy(&Head_Frame->MessgTypeIdent[0], "2010", sizeof(Head_Frame->MessgTypeIdent));//消息标识符  "2010"

	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化帧数据内容为 0x00
	Head_Frame->PrimaryBitmap[40/8 -1] |= 0x80 >> ((40-1)%8);//主位元表 40
	Head_Frame->PrimaryBitmap[41/8 -0] |= 0x80 >> ((41-1)%8);//主位元表 41
	Head_Frame->PrimaryBitmap[62/8 -0] |= 0x80 >> ((62-1)%8);//主位元表 62
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63

	AP_Frame->flag[0] = '0';//序列号类型（01：充值02：充电消费）  40 n2 
	AP_Frame->flag[1] = '2';
	memcpy(&AP_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(AP_Frame->ter_sn));//充电终端序列号

	systime = time(NULL);        //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
	sprintf(&tmp[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
	memcpy(&AP_Frame->time[0], &tmp[0], sizeof(AP_Frame->time));//终端时间（格式yyyyMMddHHmmss）	62	N	N14

	MD5_DES(&AP_Frame->flag[0], sizeof(ISO8583_AP_Get_SN_Frame)-sizeof(AP_Frame->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&AP_Frame->MD5[0], &MD5[0], 8);

	len = ISO8583_Sent_Data(fd, SendBuf, sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Get_SN_Frame), 0);
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Get_SN_Frame))){//如果发送不成功
		return (-1);
	}else{
	  COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}

//获取流水号
int ISO8583_Get_SN_Deal(unsigned int length,unsigned char *buf)
{	

  unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针
	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_AP_Get_SN_Resp_Frame *AP_Get_SN_Resp_Frame;
	Head_Frame = (ISO8583_Head_Frame *)(buf);
	AP_Get_SN_Resp_Frame = (ISO8583_AP_Get_SN_Resp_Frame *)(buf+sizeof(ISO8583_Head_Frame));
	if(length >= (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Get_SN_Resp_Frame))){//表明客户端有数据发送过来，作相应接受处理
		MD5_DES(&AP_Get_SN_Resp_Frame->Busy_SN[0], (length - sizeof(ISO8583_Head_Frame)-sizeof(AP_Get_SN_Resp_Frame->MD5)), &ISO8583_App_Struct.work_key[0], &MD5[0]);
		hex_to_str(&tmp[0], &MD5[0], 8);
		if(0 == memcmp(&tmp[0], &AP_Get_SN_Resp_Frame->MD5[0], sizeof(AP_Get_SN_Resp_Frame->MD5))){
			if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "2011", sizeof(Head_Frame->MessgTypeIdent))){
				if((AP_Get_SN_Resp_Frame->flag[0] == '0')&&(AP_Get_SN_Resp_Frame->flag[1] == '0')){
					memcpy(&ISO8583_App_Struct.ISO8583_busy_SN_Vlaue[0], &AP_Get_SN_Resp_Frame->Busy_SN[0], sizeof(AP_Get_SN_Resp_Frame->Busy_SN));//消息标识符  "0120"
					ISO8583_App_Struct.ISO8583_Get_SN_falg = 2;
					return 0;
				}else{
					ISO8583_App_Struct.ISO8583_Get_SN_falg = 0;
				}
			}
		}
	}  
	return (-1);
}

//充电数据业务数据上报   发送上行数据
int ISO8583_AP_charg_busy_sent(int fd, ISO8583_AP_charg_busy_Frame *busy_data)// 充电数据业务数据上报
{
	unsigned int ret,len;
	unsigned char SendBuf[512];  //发送数据区指针

	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针

	time_t systime=0;
	struct tm tm_t;

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_AP_charg_busy_Frame *AP_Frame;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	AP_Frame = (ISO8583_AP_charg_busy_Frame *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据填充
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'

	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_charg_busy_Frame))>>8;  //报文长度
	Head_Frame->SessionLen[1] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_charg_busy_Frame))&0xFF;
	memcpy(&Head_Frame->MessgTypeIdent[0], "1010", sizeof(Head_Frame->MessgTypeIdent));//消息标识符

	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化帧数据内容为 0x00
	Head_Frame->PrimaryBitmap[2/8 -0] |= 0x80 >> ((2-1)%8);//主位元表 2
	Head_Frame->PrimaryBitmap[3/8 -0] |= 0x80 >> ((3-1)%8);//主位元表 3
	Head_Frame->PrimaryBitmap[4/8 -0] |= 0x80 >> ((4-1)%8);//主位元表 4
	Head_Frame->PrimaryBitmap[5/8 -0] |= 0x80 >> ((5-1)%8);//主位元表 5
	Head_Frame->PrimaryBitmap[6/8 -0] |= 0x80 >> ((6-1)%8);//主位元表 6
	Head_Frame->PrimaryBitmap[7/8 -0] |= 0x80 >> ((7-1)%8);//主位元表 7
	Head_Frame->PrimaryBitmap[8/8 -1] |= 0x80 >> ((8-1)%8);//主位元表 8
	Head_Frame->PrimaryBitmap[9/8 -0] |= 0x80 >> ((9-1)%8);//主位元表 9
	Head_Frame->PrimaryBitmap[11/8 -0] |= 0x80 >> ((11-1)%8);//主位元表 11
	Head_Frame->PrimaryBitmap[30/8 -0] |= 0x80 >> ((30-1)%8);//主位元表 30
	Head_Frame->PrimaryBitmap[41/8 -0] |= 0x80 >> ((41-1)%8);//主位元表 41
	
	Head_Frame->PrimaryBitmap[42/8 -0] |= 0x80 >> ((42-1)%8);//主位元表 42
	Head_Frame->PrimaryBitmap[43/8 -0] |= 0x80 >> ((43-1)%8);//主位元表 43
	Head_Frame->PrimaryBitmap[44/8 -0] |= 0x80 >> ((44-1)%8);//主位元表 44
	Head_Frame->PrimaryBitmap[45/8 -0] |= 0x80 >> ((45-1)%8);//主位元表 45

	Head_Frame->PrimaryBitmap[46/8 -0] |= 0x80 >> ((46-1)%8);//主位元表 46
	Head_Frame->PrimaryBitmap[47/8 -0] |= 0x80 >> ((47-1)%8);//主位元表 47
	Head_Frame->PrimaryBitmap[48/8 -1] |= 0x80 >> ((48-1)%8);//主位元表 48
	
	Head_Frame->PrimaryBitmap[49/8 -0] |= 0x80 >> ((49-1)%8);//主位元表 49
	Head_Frame->PrimaryBitmap[50/8 -0] |= 0x80 >> ((50-1)%8);//主位元表 50
	Head_Frame->PrimaryBitmap[51/8 -0] |= 0x80 >> ((51-1)%8);//主位元表 51
	Head_Frame->PrimaryBitmap[52/8 -0] |= 0x80 >> ((52-1)%8);//主位元表 52
	
	Head_Frame->PrimaryBitmap[54/8 -0] |= 0x80 >> ((54-1)%8);//主位元表 54
	Head_Frame->PrimaryBitmap[55/8 -0] |= 0x80 >> ((55-1)%8);//主位元表 55
	
	Head_Frame->PrimaryBitmap[59/8 -0] |= 0x80 >> ((59-1)%8);//主位元表 59
	Head_Frame->PrimaryBitmap[60/8 -0] |= 0x80 >> ((60-1)%8);//主位元表 60
	Head_Frame->PrimaryBitmap[61/8 -0] |= 0x80 >> ((61-1)%8);//主位元表 61
	Head_Frame->PrimaryBitmap[62/8 -0] |= 0x80 >> ((62-1)%8);//主位元表 62
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63

	//填充业务数据内容
	memcpy(&AP_Frame->card_sn[0], &busy_data->card_sn[0], sizeof(ISO8583_AP_charg_busy_Frame));//复制业务数据记录内容
	memcpy(&AP_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(AP_Frame->ter_sn));//重新填充桩号，以免桩号改变以前的交易记录上传失败

	systime = time(NULL);        //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
	sprintf(&tmp[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
	memcpy(&AP_Frame->time[0], &tmp[0], sizeof(AP_Frame->time));//终端时间（格式yyyyMMddHHmmss）	62	N	N14

	MD5_DES(&AP_Frame->card_sn[0], sizeof(ISO8583_AP_charg_busy_Frame)-sizeof(AP_Frame->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&AP_Frame->MD5[0], &MD5[0], 8);

	len = ISO8583_Sent_Data(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_charg_busy_Frame)), 0);
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_charg_busy_Frame))){//如果发送不成功
		return (-1);
	}else{
		COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}



//充电数据业务数据上报
int ISO8583_Charger_Busy_Deal(unsigned int length,unsigned char *buf)
{
	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_AP_Resp_Frame *AP_Resp_Frame;

	Head_Frame = (ISO8583_Head_Frame *)(buf);
	AP_Resp_Frame = (ISO8583_AP_Resp_Frame *)(buf+sizeof(ISO8583_Head_Frame));
	
	if(length == (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Resp_Frame))){//表明客户端有数据发送过来，作相应接受处理
		if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "1011", sizeof(Head_Frame->MessgTypeIdent))){
			//printf("1011   ===== ISO8583_App_Struct.ISO8583_Busy_Need_Up_falg =%d %0x \n",ISO8583_App_Struct.ISO8583_Busy_Need_Up_falg,AP_Resp_Frame->flag[1]);
			if((AP_Resp_Frame->flag[0] == '0')&&((AP_Resp_Frame->flag[1] == '0')||(AP_Resp_Frame->flag[1] == '5'))){
				if(ISO8583_App_Struct.ISO8583_Busy_Need_Up_falg == 1){
				   ISO8583_App_Struct.ISO8583_Busy_Need_Up_falg = 2;
				}
				return (0);
			}else if((AP_Resp_Frame->flag[0] == '0')&&((AP_Resp_Frame->flag[1] == '4')||(AP_Resp_Frame->flag[1] == '3'))){
			  if(ISO8583_App_Struct.ISO8583_Busy_Need_Up_falg == 1){
				   ISO8583_App_Struct.ISO8583_Busy_Need_Up_falg = 3;
					 if(DEBUGLOG) GLOBA_RUN_LogOut("记录上送异常原因：%d",(AP_Resp_Frame->flag[1] - 0x30));  
				}
			  return (1);
			}else{
			  ISO8583_App_Struct.ISO8583_Busy_Need_Up_falg = 0;
			}
		}
	}
	return (-1);
}

//黑名单数据更新
int Insert_card_DB(unsigned char *sn, unsigned int count)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	char **azResult = NULL;
	int i,j;
	int id;

	char sql[512];

	rc = sqlite3_open_v2(F_CHARGER_CARD_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE |
									     SQLITE_OPEN_CREATE,
									     NULL);
	if(rc){
		printf("Can't create Card_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	sprintf(sql,"CREATE TABLE Card_DB(ID INTEGER PRIMARY KEY autoincrement, \
               card_sn TEXT);");

	if(sqlite3_exec(db, sql, 0, 0, &zErrMsg) != SQLITE_OK){
		//printf("CREATE TABLE Card_DB error: %s\n", sqlite3_errmsg(db));
		sqlite3_free(zErrMsg);
	}

	if(TRANSACTION_FLAG) sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);

	i = 0;
	while(i<count){
  	sprintf(sql, "INSERT INTO Card_DB VALUES (NULL, '%.16s');", &sn[i*16]);
  	i++;
  
  	if(sqlite3_exec(db , sql , 0 , 0 , &zErrMsg) != SQLITE_OK){
  		printf("Insert Card_DB error: %s\n", sqlite3_errmsg(db));
  		sqlite3_free(zErrMsg);
  		
  		if(TRANSACTION_FLAG) sqlite3_exec(db, "rollback transaction", NULL, NULL, NULL);
  				
  		sqlite3_close(db);
  		return -1;
  	}
	}
	
	if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);

	sqlite3_close(db);
	return -1;
}

//删除黑名单数据表
int Delete_card_DB()
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	char **azResult = NULL;
	int i,j;
	int id;

	char sql[512];

	rc = sqlite3_open_v2(F_CHARGER_CARD_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE |
									     SQLITE_OPEN_CREATE,
									     NULL);
	if(rc){
		printf("Can't create Card_DB database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	sprintf(sql,"CREATE TABLE Card_DB(ID INTEGER PRIMARY KEY autoincrement, \
               card_sn TEXT);");

	if(sqlite3_exec(db, sql, 0, 0, &zErrMsg) != SQLITE_OK){
		//printf("CREATE TABLE Card_DB error: %s\n", sqlite3_errmsg(db));
		sqlite3_free(zErrMsg);
	}

	if(TRANSACTION_FLAG) sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);

	sprintf(sql, "DELETE FROM Card_DB;");//清空表数据
	if(sqlite3_exec(db, sql, 0, 0, &zErrMsg) != SQLITE_OK){
		printf("DELETE TABLE Card_DB error: %s\n", sqlite3_errmsg(db));
		sqlite3_free(zErrMsg);

  	if(TRANSACTION_FLAG) sqlite3_exec(db, "rollback transaction", NULL, NULL, NULL);

		sqlite3_close(db);
		return -1;
	}

	sprintf(sql, "UPDATE sqlite_sequence SET seq=0 WHERE name='Card_DB';");//更改自增ID从0开始
	if(sqlite3_exec(db, sql, 0, 0, &zErrMsg) != SQLITE_OK){
		printf("set seq=0 error: %s\n", sqlite3_errmsg(db));
		sqlite3_free(zErrMsg);

  	if(TRANSACTION_FLAG) sqlite3_exec(db, "rollback transaction", NULL, NULL, NULL);

		sqlite3_close(db);
		return -1;
	}

	if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);

	sqlite3_close(db);
	return 0;
}

//心跳 数据包中实时数据处理
int ISO8583_AP_Heart_Real_Data(ISO8583_AP_Heart_Real_Data_Frame * Frame)
{
	UINT32 fault = 0,i = 0;
	memset(&Frame->state, 0x00, sizeof(Frame));//初始化帧数据内容为0
	
	if((Globa_1->Error_system != 0)||(Globa_2->Error_system != 0)||(Globa_1->Electric_pile_workstart_Enable == 1)||(Globa_2->Electric_pile_workstart_Enable == 1)){//一个有故障就算故障
		Frame->state = 0x01;             //工作状态	BIN码	1Byte	0x01-故障  0x02-告警  03-空闲 0x04-充电中 0x05-完成  0x06-预约
	}else if((Globa_1->charger_state == 0x04)&&(Globa_2->charger_state == 0x04)){//一个有充电中就算充电中
		Frame->state = 0x04;  //此处只有    03-空闲 0x04-充电中 0x05-完成
	}else if((Globa_1->charger_state == 0x05)&&(Globa_2->charger_state == 0x05)){//一个有完成就算完成
		Frame->state = 0x05;  //此处只有    03-空闲 0x04-充电中 0x05-完成
	}else{
		Frame->state = 0x03;  //此处只有    03-空闲 0x04-充电中 0x05-完成
	}

	Frame->gun1 = Globa_1->gun_state;
	Frame->gun2 = Globa_2->gun_state;

	Frame->Output_voltage1[0] = (Globa_1->Output_voltage/100)>>8;
	Frame->Output_voltage1[1] = Globa_1->Output_voltage/100;      //充电机输出电压 BIN码	2Byte	精确到小数点后一位
	Frame->Output_voltage2[0] = (Globa_2->Output_voltage/100)>>8;
	Frame->Output_voltage2[1] = Globa_2->Output_voltage/100;      //充电机输出电压 BIN码	2Byte	精确到小数点后一位

	Frame->Output_current1[0] = (Globa_1->Output_current/10)>>8;
	Frame->Output_current1[1] = Globa_1->Output_current/10;      //充电机输出电流 BIN码	2Byte	精确到小数点后二位
	Frame->Output_current2[0] = (Globa_2->Output_current/10)>>8;
	Frame->Output_current2[1] = Globa_2->Output_current/10;      //充电机输出电流 BIN码	2Byte	精确到小数点后二位

	Frame->batt_SOC1 = Globa_1->BMS_batt_SOC;                    //SOCBIN码	1Byte	整型
	Frame->batt_SOC2 = Globa_2->BMS_batt_SOC;                    //SOCBIN码	1Byte	整型

	Frame->batt_LT1[0] = ((Globa_1->BMS_cell_LT - (TEMP_OFSET))*10)>>8;
	Frame->batt_LT1[1] = (Globa_1->BMS_cell_LT - (TEMP_OFSET))*10;           //电池组最低温度 BIN码	2Byte	精确到小数点后一位
	Frame->batt_LT2[0] = ((Globa_2->BMS_cell_LT- (TEMP_OFSET))*10)>>8;
	Frame->batt_LT2[1] = (Globa_2->BMS_cell_LT- (TEMP_OFSET))*10;           //电池组最低温度 BIN码	2Byte	精确到小数点后一位

	Frame->batt_HT1[0] = ((Globa_1->BMS_cell_HT - (TEMP_OFSET))*10)>>8;
	Frame->batt_HT1[1] = (Globa_1->BMS_cell_HT - (TEMP_OFSET))*10;           //电池组最高温度 BIN码	2Byte	精确到小数点后一位
	Frame->batt_HT2[0] = ((Globa_2->BMS_cell_HT -(TEMP_OFSET))*10)>>8;
	Frame->batt_HT2[1] = (Globa_2->BMS_cell_HT - (TEMP_OFSET))*10;           //电池组最高温度 BIN码	2Byte	精确到小数点后一位

	Frame->cell_HV1[0] = (Globa_1->BMS_cell_HV*10)>>8;
	Frame->cell_HV1[1] = Globa_1->BMS_cell_HV*10;           //单体电池最高电压	 BIN码	2Byte	精确到小数点后三位
	Frame->cell_HV2[0] = (Globa_2->BMS_cell_HV*10)>>8;
	Frame->cell_HV2[1] = Globa_2->BMS_cell_HV*10;           //单体电池最高电压	 BIN码	2Byte	精确到小数点后三位

	Frame->cell_LV1[0] = (Globa_1->BMS_cell_LV*10)>>8;
	Frame->cell_LV1[1] = Globa_1->BMS_cell_LV*10;           //单体电池最低电压	 BIN码	2Byte	精确到小数点后三位
	Frame->cell_LV2[0] = (Globa_2->BMS_cell_LV*10)>>8;
	Frame->cell_LV2[1] = Globa_2->BMS_cell_LV*10;           //单体电池最低电压	 BIN码	2Byte	精确到小数点后三位

	Frame->KWH1[0] = (Globa_1->kwh)>>24;
	Frame->KWH1[1] = (Globa_1->kwh)>>16;
	Frame->KWH1[2] = (Globa_1->kwh)>>8;
	Frame->KWH1[3] = (Globa_1->kwh);               //有功总电度 BIN码	4Byte	精确到小数点后二位
	Frame->KWH2[0] = (Globa_2->kwh)>>24;
	Frame->KWH2[1] = (Globa_2->kwh)>>16;
	Frame->KWH2[2] = (Globa_2->kwh)>>8;
	Frame->KWH2[3] = (Globa_2->kwh);               //有功总电度 BIN码	4Byte	精确到小数点后二位

	Frame->Time1[0] = (Globa_1->Time/60)>>8;
	Frame->Time1[1] = Globa_1->Time/60;              //累计充电时间	BIN码	2Byte	单位：min
	if((Globa_1->Double_Gun_To_Car_falg == 1)&&(Globa_1->Charger_param.System_Type == 4)){
	  Frame->Time2[0] = (Globa_1->Time/60)>>8;
	  Frame->Time2[1] = Globa_1->Time/60;              //累计充电时间	BIN码	2Byte	单位：min
	}else {
	  Frame->Time2[0] = (Globa_2->Time/60)>>8;
	  Frame->Time2[1] = Globa_2->Time/60;              //累计充电时间	BIN码	2Byte	单位：min
	}
	Frame->need_time1[0] = (Globa_1->BMS_need_time/60)>>8;
	Frame->need_time1[1] = Globa_1->BMS_need_time/60;
	Frame->need_time2[0] = (Globa_2->BMS_need_time/60)>>8;
	Frame->need_time2[1] = Globa_2->BMS_need_time/60;
	
	
	Frame->Voltmeter_comm_fault_1 = 0;  //  1#电压表通信中断
  Frame->Voltmeter_comm_fault_2 = 0;  //2#电压表通信中断
  Frame->Currtmeter_comm_fault_1 = 0;  //1#电流表通信中断
  Frame->Currtmeter_comm_fault_2 = 0;  //2#电流表通信中断
  Frame->Charge_control_comm_fault_1 = Globa_1->Error.ctrl_connect_lost;  //1#接口板通信中断
  Frame->Charge_control_comm_fault_2 = Globa_2->Error.ctrl_connect_lost;  //2#接口板通信中断
	Frame->Meter_comm_fault_1 = Globa_1->Error.meter_connect_lost;  //1#电能表通信中断
  Frame->Meter_comm_fault_2 = Globa_2->Error.meter_connect_lost;  //2#电能表通信中断
	
  Frame->Card_Reader_comm_fault_1 = 0;//1#读卡器通信中断
  Frame->Card_Reader_comm_fault_2 = 0;//2#读卡器通信中断
  Frame->AC_comm_fault_1 = Globa_1->Error.AC_connect_lost;//1#交流采集模块通信中断
  Frame->AC_comm_fault_2 = 0;//2#交流采集模块通信中断
	fault = 0;
	for(i=0;i<Globa_1->Charger_param.Charge_rate_number1;i++){
		fault |= Globa_1->Error.charg_connect_lost[i];
	}
  Frame->Rectifier_comm_fault_1 = fault;//1#组充电模块通信中断
	fault = 0;
	for(i=0;i<Globa_2->Charger_param.Charge_rate_number2;i++){
		fault |= Globa_2->Error.charg_connect_lost[i];
	}
  Frame->Rectifier_comm_fault_2 = fault;//2#组充电模块通信中断
 
 	fault = 0;
	for(i=0;i<Globa_1->Charger_param.Charge_rate_number1;i++){
		fault |= Globa_1->Error.charg_fault[i];
	}
  Frame->Rectifier_fault_1 = fault;//1#组充电模块故障
	fault = 0;
	for(i=0;i<Globa_2->Charger_param.Charge_rate_number2;i++){
		fault |= Globa_2->Error.charg_fault[i];
	}
  Frame->Rectifier_fault_2 = fault;//2#组充电模块故障
 
  Frame->Emergency_stop_fault = Globa_1->Error.emergency_switch;//急停故障
	 
	fault = 0;
	for(i=0;i<Globa_1->Charger_param.Charge_rate_number1;i++){
		fault |= Globa_1->Error.charg_out_shout[i];
	}
  Frame->Rectifier_output_short_fault_1 = fault;//1#组充电机输出短路故障
	fault = 0;
	for(i=0;i<Globa_2->Charger_param.Charge_rate_number2;i++){
		fault |= Globa_2->Error.charg_out_shout[i];
	}
  Frame->Rectifier_output_short_fault_2 = fault;//2#组充电机输出短路故障
 
  Frame->System_Alarm_Insulation_fault_1 = Globa_1->Error.sistent_alarm; //1#组充电机绝缘故障
  Frame->System_Alarm_Insulation_fault_2 = Globa_2->Error.sistent_alarm; //2#组充电机绝缘故障
	
  fault = 0;
	for(i=0;i<Globa_1->Charger_param.Charge_rate_number1;i++){
		fault |= Globa_1->Error.charg_out_OV[i];
	}
  Frame->Rectifier_output_over_vol_fault_1 = fault;//1#组充电机输出过压保护
	fault = 0;
	for(i=0;i<Globa_2->Charger_param.Charge_rate_number2;i++){
		fault |= Globa_2->Error.charg_out_OV[i];
	}
  Frame->Rectifier_output_over_vol_fault_2 = fault;//2#组充电机输出过压保护
 
  fault = 0;
	for(i=0;i<Globa_1->Charger_param.Charge_rate_number1;i++){
		fault |= Globa_1->Error.charg_in_OV[i];
	}
  Frame->Rectifier_input_over_vol_fault_1 = fault;//1#组充电机输出过压保护
	fault = 0;
	for(i=0;i<Globa_2->Charger_param.Charge_rate_number2;i++){
		fault |= Globa_2->Error.charg_in_OV[i];
	}
  Frame->Rectifier_input_over_vol_fault_2 = fault;//2#组充电机输出过压保护

  Frame->system_output_overvoltage_fault_1 = Globa_1->Error.output_over_limit;//1#组充电电压过压告警
  Frame->system_output_overvoltage_fault_2 = Globa_2->Error.output_over_limit;//2#组充电电压过压告警
  Frame->system_output_over_current_fault_1 = 0;//1#1组充电电流过流告警
  Frame->system_output_over_current_fault_2 = 0;//2#1组充电电流过流告警
  Frame->Disconnect_gun_1 = Globa_1->gun_link;//1#充电枪连接断开
  Frame->Disconnect_gun_2 = Globa_2->gun_link;//2#充电枪连接断开
  Frame->BMS_comm_fault_1 = 0 ;//1#1组BMS通信异常
	
  Frame->BMS_comm_fault_2 = 0 ;//2#1组BMS通信异常
	Frame->Reverse_Battery_1 = 0;//1#1组电池反接
  Frame->Reverse_Battery_2 = 0;//2#1组电池反接
  Frame->battery_voltage_fault_1 = 0;//1#电池电压检测
  Frame->battery_voltage_fault_2 = 0;//2#电池电压检测
	Frame->BMS_fault_1= 0;//1#组BMS故障终止充电
  Frame->BMS_fault_2= 0;//2#组BMS故障终止充电
	if (dev_cfg.dev_para.car_lock_num == 0)
  {
	  Frame->ground_lock_1= 0x55;  //地锁1状态
	  Frame->ground_lock_2 = 0x55;  //地锁2状态
	}
	else
  {
		  if (Globa_1->Error.CarLock_connect_lost == 1)
		  {
			  Frame->ground_lock_1= 0x66;
		  }
		  else 
		  {
		    Frame->ground_lock_1= Get_Cardlock_state(0);  //地锁1状态
		  }
		 
		  if (dev_cfg.dev_para.car_lock_num == 2)
		  {
			  if (Globa_2->Error.CarLock_connect_lost == 1)
			  {
					Frame->ground_lock_2= 0x66;
			  }
			  else 
			  {
				  Frame->ground_lock_2= Get_Cardlock_state(1);  //地锁1状态
			  }
		  }else{
			  Frame->ground_lock_2 = 0x55;  //地锁2状态
			}
	}
}

//心跳 发送上行数据 充电状态---双枪同时
int ISO8583_AP_Heart_Charging_Sent(int fd,int gunflg)
{
	unsigned int ret,len,j;
	unsigned char SendBuf[512];  //发送数据区指针
	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针
	time_t systime=0;
	struct tm tm_t;

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_AP_Heart_Charging_Frame *AP_Heart_Frame;
	ISO8583_AP_Heart_Real_Data_Frame Frame;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	AP_Heart_Frame = (ISO8583_AP_Heart_Charging_Frame *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据初始化
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'

	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Heart_Charging_Frame))>>8;  //报文长度
	Head_Frame->SessionLen[1] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Heart_Charging_Frame))&0xFF;;
	memcpy(&Head_Frame->MessgTypeIdent[0], "1020", sizeof(Head_Frame->MessgTypeIdent));//消息标识符

	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化帧数据内容为 0x00
	Head_Frame->PrimaryBitmap[3/8 -0] |= 0x80 >> ((3-1)%8);//主位元表 3 
	Head_Frame->PrimaryBitmap[4/8 -0] |= 0x80 >> ((4-1)%8);//主位元表 4
	Head_Frame->PrimaryBitmap[5/8 -0] |= 0x80 >> ((5-1)%8);//主位元表 5
	Head_Frame->PrimaryBitmap[6/8 -0] |= 0x80 >> ((6-1)%8);//主位元表 6
	Head_Frame->PrimaryBitmap[7/8 -0] |= 0x80 >> ((7-1)%8);//主位元表 7
	Head_Frame->PrimaryBitmap[8/8 -1] |= 0x80 >> ((8-1)%8);//主位元表 8
	Head_Frame->PrimaryBitmap[9/8 -0] |= 0x80 >> ((9-1)%8);//主位元表 9

	Head_Frame->PrimaryBitmap[11/8 -0] |= 0x80 >> ((11-1)%8);//主位元表 11
	Head_Frame->PrimaryBitmap[33/8 -0] |= 0x80 >> ((33-1)%8);//主位元表 33
	Head_Frame->PrimaryBitmap[35/8 -0] |= 0x80 >> ((35-1)%8);//主位元表 35
	Head_Frame->PrimaryBitmap[36/8 -0] |= 0x80 >> ((36-1)%8);//主位元表 36
	Head_Frame->PrimaryBitmap[37/8 -0] |= 0x80 >> ((37-1)%8);//主位元表 37
  Head_Frame->PrimaryBitmap[38/8 -0] |= 0x80 >> ((38-1)%8);//主位元表 38 --NEWdjh

	Head_Frame->PrimaryBitmap[41/8 -0] |= 0x80 >> ((41-1)%8);//主位元表 41
	Head_Frame->PrimaryBitmap[62/8 -0] |= 0x80 >> ((62-1)%8);//主位元表 62
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63


	AP_Heart_Frame->type[0] = '0';//充电桩类型（01：直流充电桩，02单相交流充电桩，03三相交流充电桩）
	AP_Heart_Frame->type[1] = '5';

	AP_Heart_Frame->data_len[0] = '1';//后面数据长度域 Ans…999 所以长度域 3字节
	AP_Heart_Frame->data_len[1] = '7';
	AP_Heart_Frame->data_len[2] = '2';
	ISO8583_AP_Heart_Real_Data(&Frame);
	
	hex_to_str(AP_Heart_Frame->data, &Frame, sizeof(Frame));
 
  if(gunflg == 0){
		sprintf(&tmp[0], "%08d", Globa_1->ISO_8583_rmb);				
		memcpy(&AP_Heart_Frame->rmb[0], &tmp[0], sizeof(AP_Heart_Frame->rmb));

		sprintf(&tmp[0], "%08d", Globa_1->ISO_8583_rmb_Serv);	//服务费金额	37	N	N8			
		memcpy(&AP_Heart_Frame->serv_rmb[0], &tmp[0], sizeof(AP_Heart_Frame->serv_rmb));
    AP_Heart_Frame->gun_NO[0] = '0';
		AP_Heart_Frame->gun_NO[1] = '1';
	  if(Globa_1->App.APP_start == 1){
			printf("app心跳 发送上行数据 充电状态---双枪同时 :枪号=%d\n", gunflg); 

			AP_Heart_Frame->charge_way[0] = '0';
			AP_Heart_Frame->charge_way[1] = '2';	
			memset(&AP_Heart_Frame->card_sn[0], '0', sizeof(AP_Heart_Frame->card_sn));
			memcpy( &AP_Heart_Frame->card_sn[0],&Globa_1->App.ID[0], sizeof(Globa_1->card_sn));//先保存一下临时卡号
			 
			if(Globa_1->App.APP_charger_type == 1){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '1';	
				sprintf(&tmp[0], "%08d", Globa_1->App.APP_charge_kwh);				
				memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
			}else if(Globa_1->App.APP_charger_type == 2){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '2';	
				sprintf(&tmp[0], "%08d", Globa_1->APP_Card_charge_time_limit);				
				memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
			}else if(Globa_1->App.APP_charger_type == 3){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '3';	
				sprintf(&tmp[0], "%08d", Globa_1->App.APP_charge_money);				
				memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
			}else if(Globa_1->App.APP_charger_type == 4){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '4';	
				memset(&AP_Heart_Frame->charge_limt_data[0], '0', sizeof(AP_Heart_Frame->charge_limt_data));
			}
			memset(&AP_Heart_Frame->appointment_start_time[0], '0', sizeof(AP_Heart_Frame->appointment_start_time));
			memset(&AP_Heart_Frame->appointment_end_time[0], '0', sizeof(AP_Heart_Frame->appointment_end_time));
			
			if ((dev_cfg.dev_para.car_lock_num != 0)&&((Get_Cardlock_state(0) == 0x01)||(Get_Cardlock_state(0) == 0x02)))
			{
				memcpy(&AP_Heart_Frame->stop_car_time[0],&Globa_1->park_start_time[0], sizeof(Globa_1->park_start_time));
			}
			else
			{			
    		memset(&AP_Heart_Frame->stop_car_time, '0', sizeof(AP_Heart_Frame->stop_car_time));
			}
			memcpy(&AP_Heart_Frame->Busy_SN[0],&Globa_1->App.busy_SN[0], sizeof(Globa_1->App.busy_SN));//先保存一下临时卡号
		}else if(Globa_1->charger_start == 1){
			printf("kah 心跳 发送上行数据 充电状态---双枪同时 :枪号=%d\n", gunflg); 
		 	if(Globa_1->Start_Mode == VIN_START_TYPE){
				 AP_Heart_Frame->charge_way[0] = '0';
			   AP_Heart_Frame->charge_way[1] = '3';
			}
			else{
			  AP_Heart_Frame->charge_way[0] = '0';
			  AP_Heart_Frame->charge_way[1] = '1';	
			}
			memcpy( &AP_Heart_Frame->card_sn[0],&Globa_1->card_sn[0], sizeof(Globa_1->card_sn));//先保存一下临时卡号
			 
			if(Globa_1->Card_charger_type == 1){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '1';	
				sprintf(&tmp[0], "%08d", Globa_1->QT_charge_kwh);				
				memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
			}else if(Globa_1->Card_charger_type == 2){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '2';	
				sprintf(&tmp[0], "%08d", Globa_1->APP_Card_charge_time_limit);				
				memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
			}else if(Globa_1->Card_charger_type == 3){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '3';	
				sprintf(&tmp[0], "%08d", Globa_1->QT_charge_money);				
				memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
			}else if(Globa_1->Card_charger_type == 4){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '4';	
				memset(&AP_Heart_Frame->charge_limt_data[0], '0', sizeof(AP_Heart_Frame->charge_limt_data));
			}
			memset(&AP_Heart_Frame->appointment_start_time[0], '0', sizeof(AP_Heart_Frame->appointment_start_time));
			memset(&AP_Heart_Frame->appointment_end_time[0], '0', sizeof(AP_Heart_Frame->appointment_end_time));
	    //memset(&AP_Heart_Frame->stop_car_time, '0', sizeof(AP_Heart_Frame->stop_car_time));
			if ((dev_cfg.dev_para.car_lock_num != 0)&&((Get_Cardlock_state(0) == 0x01)||(Get_Cardlock_state(0) == 0x02)))
			{
				memcpy(&AP_Heart_Frame->stop_car_time[0],&Globa_1->park_start_time[0], sizeof(Globa_1->park_start_time));
			}
			else
			{			
    		memset(&AP_Heart_Frame->stop_car_time, '0', sizeof(AP_Heart_Frame->stop_car_time));
			}
			memset(&AP_Heart_Frame->Busy_SN[0],'0',sizeof(Globa_1->App.busy_SN));
		}
	}else if(gunflg == 1){
		sprintf(&tmp[0], "%08d", Globa_2->ISO_8583_rmb);				
		memcpy(&AP_Heart_Frame->rmb[0], &tmp[0], sizeof(AP_Heart_Frame->rmb));
	  sprintf(&tmp[0], "%08d", Globa_2->ISO_8583_rmb_Serv);	//服务费金额	37	N	N8			
		memcpy(&AP_Heart_Frame->serv_rmb[0], &tmp[0], sizeof(AP_Heart_Frame->serv_rmb));
		AP_Heart_Frame->gun_NO[0] = '0';
		AP_Heart_Frame->gun_NO[1] = '2';
		
	  if(Globa_2->App.APP_start == 1){
			AP_Heart_Frame->charge_way[0] = '0';
			AP_Heart_Frame->charge_way[1] = '2';	
			memset(&AP_Heart_Frame->card_sn[0], '0', sizeof(AP_Heart_Frame->card_sn));
			memcpy( &AP_Heart_Frame->card_sn[0],&Globa_2->App.ID[0], sizeof(Globa_2->card_sn));//先保存一下临时卡号
			 
			if(Globa_2->App.APP_charger_type == 1){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '1';	
				sprintf(&tmp[0], "%08d", Globa_2->App.APP_charge_kwh);				
				memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
			}else if(Globa_2->App.APP_charger_type == 2){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '2';	
				sprintf(&tmp[0], "%08d", Globa_2->APP_Card_charge_time_limit);				
				memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
			}else if(Globa_2->App.APP_charger_type == 3){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '3';	
				sprintf(&tmp[0], "%08d", Globa_2->App.APP_charge_money);				
				memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
			}else if(Globa_2->App.APP_charger_type == 4){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '4';	
				memset(&AP_Heart_Frame->charge_limt_data[0], '0', sizeof(AP_Heart_Frame->charge_limt_data));
			}
			memset(&AP_Heart_Frame->appointment_start_time[0], '0', sizeof(AP_Heart_Frame->appointment_start_time));
			memset(&AP_Heart_Frame->appointment_end_time[0], '0', sizeof(AP_Heart_Frame->appointment_end_time));
			//memset(&AP_Heart_Frame->stop_car_time, '0', sizeof(AP_Heart_Frame->stop_car_time));
			if ((dev_cfg.dev_para.car_lock_num == 2)&&((Get_Cardlock_state(1) == 0x01)||(Get_Cardlock_state(1) == 0x02)))
			{
				memcpy(&AP_Heart_Frame->stop_car_time[0],&Globa_2->park_start_time[0], sizeof(Globa_2->park_start_time));
			}
			else
			{			
    		memset(&AP_Heart_Frame->stop_car_time, '0', sizeof(AP_Heart_Frame->stop_car_time));
			}
			memcpy(&AP_Heart_Frame->Busy_SN[0],&Globa_2->App.busy_SN[0], sizeof(Globa_2->App.busy_SN));//先保存一下临时卡号
		}else if(Globa_2->charger_start == 1){
			if(Globa_2->Start_Mode == VIN_START_TYPE){
				 AP_Heart_Frame->charge_way[0] = '0';
			   AP_Heart_Frame->charge_way[1] = '3';
			}
			else{
			  AP_Heart_Frame->charge_way[0] = '0';
			  AP_Heart_Frame->charge_way[1] = '1';	
			}
			//AP_Heart_Frame->charge_way[0] = '0';
		  //AP_Heart_Frame->charge_way[1] = '1';	
			memcpy( &AP_Heart_Frame->card_sn[0],&Globa_2->card_sn[0], sizeof(Globa_2->card_sn));//先保存一下临时卡号
			if(Globa_2->Card_charger_type == 1){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '1';	
				sprintf(&tmp[0], "%08d", Globa_2->QT_charge_kwh);				
				memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
			}else if(Globa_2->Card_charger_type == 2){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '2';	
				sprintf(&tmp[0], "%08d", Globa_2->APP_Card_charge_time_limit);				
				memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
			}else if(Globa_2->Card_charger_type == 3){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '3';	
				sprintf(&tmp[0], "%08d", Globa_2->QT_charge_money);				
				memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
			}else if(Globa_2->Card_charger_type == 4){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '4';	
				memset(&AP_Heart_Frame->charge_limt_data[0], '0', sizeof(AP_Heart_Frame->charge_limt_data));
			}
			memset(&AP_Heart_Frame->appointment_start_time[0], '0', sizeof(AP_Heart_Frame->appointment_start_time));
			memset(&AP_Heart_Frame->appointment_end_time[0], '0', sizeof(AP_Heart_Frame->appointment_end_time));
		  //memset(&AP_Heart_Frame->stop_car_time, '0', sizeof(AP_Heart_Frame->stop_car_time));
	  	
			if((dev_cfg.dev_para.car_lock_num == 2)&&((Get_Cardlock_state(1) == 0x01)||(Get_Cardlock_state(1) == 0x02)))
			{
				memcpy(&AP_Heart_Frame->stop_car_time[0],&Globa_2->park_start_time[0], sizeof(Globa_2->park_start_time));
			}
			else
			{			
    		memset(&AP_Heart_Frame->stop_car_time, '0', sizeof(AP_Heart_Frame->stop_car_time));
			}
			memset(&AP_Heart_Frame->Busy_SN[0],'0',sizeof(Globa_1->App.busy_SN));
	  }
	}else if(gunflg == 2){//双枪启动模式的第二枪上送数据，
		sprintf(&tmp[0], "%08d", Globa_2->ISO_8583_rmb);				
		memcpy(&AP_Heart_Frame->rmb[0], &tmp[0], sizeof(AP_Heart_Frame->rmb));
	  sprintf(&tmp[0], "%08d", Globa_2->ISO_8583_rmb_Serv);	//服务费金额	37	N	N8			
		memcpy(&AP_Heart_Frame->serv_rmb[0], &tmp[0], sizeof(AP_Heart_Frame->serv_rmb));
		AP_Heart_Frame->gun_NO[0] = '0';
		AP_Heart_Frame->gun_NO[1] = '2';
		if(Globa_1->Start_Mode == VIN_START_TYPE){
			 AP_Heart_Frame->charge_way[0] = '0';
			 AP_Heart_Frame->charge_way[1] = '3';
		}
		else{
			AP_Heart_Frame->charge_way[0] = '0';
			AP_Heart_Frame->charge_way[1] = '1';	
		}
  //	AP_Heart_Frame->charge_way[0] = '0';
		//AP_Heart_Frame->charge_way[1] = '1';	
		memcpy( &AP_Heart_Frame->card_sn[0],&Globa_1->card_sn[0], sizeof(Globa_1->card_sn));//先保存一下临时卡号
		if(Globa_1->Card_charger_type == 1){
			AP_Heart_Frame->control_type[0] = '0';
			AP_Heart_Frame->control_type[1] = '0';
			AP_Heart_Frame->control_type[2] = '1';	
			sprintf(&tmp[0], "%08d", Globa_1->QT_charge_kwh);				
			memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
		}else if(Globa_1->Card_charger_type == 2){
			AP_Heart_Frame->control_type[0] = '0';
			AP_Heart_Frame->control_type[1] = '0';
			AP_Heart_Frame->control_type[2] = '2';	
			sprintf(&tmp[0], "%08d", Globa_1->APP_Card_charge_time_limit);				
			memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
		}else if(Globa_1->Card_charger_type == 3){
			AP_Heart_Frame->control_type[0] = '0';
			AP_Heart_Frame->control_type[1] = '0';
			AP_Heart_Frame->control_type[2] = '3';	
			sprintf(&tmp[0], "%08d", Globa_1->QT_charge_money);				
			memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
		}else if(Globa_1->Card_charger_type == 4){
			AP_Heart_Frame->control_type[0] = '0';
			AP_Heart_Frame->control_type[1] = '0';
			AP_Heart_Frame->control_type[2] = '4';	
			memset(&AP_Heart_Frame->charge_limt_data[0], '0', sizeof(AP_Heart_Frame->charge_limt_data));
		}
		memset(&AP_Heart_Frame->appointment_start_time[0], '0', sizeof(AP_Heart_Frame->appointment_start_time));
		memset(&AP_Heart_Frame->appointment_end_time[0], '0', sizeof(AP_Heart_Frame->appointment_end_time));
		memset(&AP_Heart_Frame->stop_car_time, '0', sizeof(AP_Heart_Frame->stop_car_time));
		memset(&AP_Heart_Frame->Busy_SN[0],'0',sizeof(Globa_1->App.busy_SN));
	}
	memcpy(&AP_Heart_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(AP_Heart_Frame->ter_sn));//充电终端序列号    41	N	n12

	systime = time(NULL);        //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
	sprintf(&tmp[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
	memcpy(&AP_Heart_Frame->time[0], &tmp[0], sizeof(AP_Heart_Frame->time));//终端时间（格式yyyyMMddHHmmss）	62	N	N14

	MD5_DES(&AP_Heart_Frame->charge_way[0], sizeof(ISO8583_AP_Heart_Charging_Frame)-sizeof(AP_Heart_Frame->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&AP_Heart_Frame->MD5[0], &MD5[0], 8);

	len = ISO8583_Sent_Data(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Heart_Charging_Frame)), 0);
  printf("心跳 发送上行数据 充电状态---双枪同时 :枪号=%d\n", gunflg); 
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Heart_Charging_Frame))){//如果发送不成功
		return (-1);
	}else{
		COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}

//心跳 发送上行数据  非充电状态---双枪同时
int ISO8583_AP_Heart_NoCharging_Sent(int fd,int gunflg)
{
	unsigned int ret,len;
	unsigned char SendBuf[512];  //发送数据区指针
	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针
	time_t systime=0;
	struct tm tm_t;

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_AP_Heart_NoCharging_Frame *AP_Heart_Frame;
	ISO8583_AP_Heart_Real_Data_Frame Frame;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	AP_Heart_Frame = (ISO8583_AP_Heart_NoCharging_Frame *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据初始化
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'

	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Heart_NoCharging_Frame))>>8;  //报文长度
	Head_Frame->SessionLen[1] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Heart_NoCharging_Frame))&0xFF;
	memcpy(&Head_Frame->MessgTypeIdent[0], "1020", sizeof(Head_Frame->MessgTypeIdent));//消息标识符

	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化帧数据内容为 0x00
  Head_Frame->PrimaryBitmap[33/8 -0] |= 0x80 >> ((33-1)%8);//主位元表 33
	Head_Frame->PrimaryBitmap[35/8 -0] |= 0x80 >> ((35-1)%8);//主位元表 35
	Head_Frame->PrimaryBitmap[36/8 -0] |= 0x80 >> ((36-1)%8);//主位元表 36
	Head_Frame->PrimaryBitmap[37/8 -0] |= 0x80 >> ((37-1)%8);//主位元表 37
  Head_Frame->PrimaryBitmap[38/8 -0] |= 0x80 >> ((38-1)%8);//主位元表 38 --NEWdjh

	Head_Frame->PrimaryBitmap[41/8 -0] |= 0x80 >> ((41-1)%8);//主位元表 41
	Head_Frame->PrimaryBitmap[62/8 -0] |= 0x80 >> ((62-1)%8);//主位元表 62
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63
	
	AP_Heart_Frame->type[0] = '0';
	AP_Heart_Frame->type[1] = '5';
	AP_Heart_Frame->data_len[0] = '1';//后面数据长度域 Ans…999 所以长度域 3字节
	AP_Heart_Frame->data_len[1] = '7';
	AP_Heart_Frame->data_len[2] = '2';
	
	ISO8583_AP_Heart_Real_Data(&Frame);

	//hex_to_str(&SendBuf[sizeof(ISO8583_Head_Frame)+5], &Frame, sizeof(Frame));
	hex_to_str(AP_Heart_Frame->data,&Frame, sizeof(Frame));
  if(gunflg == 0){
		sprintf(&tmp[0], "%08d", Globa_1->ISO_8583_rmb);				
		memcpy(&AP_Heart_Frame->rmb[0], &tmp[0], sizeof(AP_Heart_Frame->rmb));

		sprintf(&tmp[0], "%08d", Globa_1->ISO_8583_rmb_Serv);	//服务费金额	37	N	N8			
		memcpy(&AP_Heart_Frame->serv_rmb[0], &tmp[0], sizeof(AP_Heart_Frame->serv_rmb));
    AP_Heart_Frame->gun_NO[0] = '0';
		AP_Heart_Frame->gun_NO[1] = '1';
	}else  if(gunflg == 1){
		sprintf(&tmp[0], "%08d", Globa_2->ISO_8583_rmb);				
		memcpy(&AP_Heart_Frame->rmb[0], &tmp[0], sizeof(AP_Heart_Frame->rmb));

	  sprintf(&tmp[0], "%08d", Globa_2->ISO_8583_rmb_Serv);	//服务费金额	37	N	N8			
		memcpy(&AP_Heart_Frame->serv_rmb[0], &tmp[0], sizeof(AP_Heart_Frame->serv_rmb));
		AP_Heart_Frame->gun_NO[0] = '0';
		AP_Heart_Frame->gun_NO[1] = '2';
	}
	memcpy(&AP_Heart_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(AP_Heart_Frame->ter_sn));//充电终端序列号    41	N	n12
	systime = time(NULL);        //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
	sprintf(&tmp[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
	memcpy(&AP_Heart_Frame->time[0], &tmp[0], sizeof(AP_Heart_Frame->time));//终端时间（格式yyyyMMddHHmmss）	62	N	N14

	MD5_DES(&AP_Heart_Frame->type[0], sizeof(ISO8583_AP_Heart_NoCharging_Frame)-sizeof(AP_Heart_Frame->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&AP_Heart_Frame->MD5[0], &MD5[0], 8);
	len = ISO8583_Sent_Data(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Heart_NoCharging_Frame)), 0);
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Heart_NoCharging_Frame))){//如果发送不成功
		return (-1);
	}else{
		COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}

//心跳 数据包中实时数据处理----单枪的
int ISO8583_AP_Heart_Real_Data_01(ISO8583_AP_Heart_Real_Data_Frame_01 * Frame)
{	
  UINT32 fault = 0,i = 0;
	memset(&Frame->state, 0x00, sizeof(ISO8583_AP_Heart_Real_Data_Frame_01));//初始化帧数据内容为0
	
	if((Globa_1->Error_system != 0)||(Globa_1->Electric_pile_workstart_Enable == 1)){
		Frame->state = 0x01;             //工作状态	BIN码	1Byte	0x01-故障  0x02-告警  03-空闲 0x04-充电中 0x05-完成  0x06-预约
	}else{
		Frame->state =  Globa_1->gun_state ;//Globa_1->charger_state;  //此处只有    03-空闲 0x04-充电中 0x05-完成 0x06-预约
	}

	Frame->gun1 = Globa_1->gun_state;

	Frame->Output_voltage[0] = (Globa_1->Output_voltage/100)>>8;
	Frame->Output_voltage[1] = Globa_1->Output_voltage/100;      //充电机输出电压 BIN码	2Byte	精确到小数点后一位
	Frame->Output_current[0] = (Globa_1->Output_current/10)>>8;
	Frame->Output_current[1] = Globa_1->Output_current/10;      //充电机输出电流 BIN码	2Byte	精确到小数点后二位
	Frame->batt_SOC = Globa_1->BMS_batt_SOC;                    //SOCBIN码	1Byte	整型
	Frame->batt_LT[0] = ((Globa_1->BMS_cell_LT - (TEMP_OFSET))*10)>>8;
	Frame->batt_LT[1] = (Globa_1->BMS_cell_LT - (TEMP_OFSET))*10;           //电池组最低温度 BIN码	2Byte	精确到小数点后一位
	Frame->batt_HT[0] = ((Globa_1->BMS_cell_HT - (TEMP_OFSET))*10)>>8;
	Frame->batt_HT[1] = (Globa_1->BMS_cell_HT - (TEMP_OFSET))*10;           //电池组最高温度 BIN码	2Byte	精确到小数点后一位

	Frame->cell_HV[0] = (Globa_1->BMS_cell_HV*10)>>8;
	Frame->cell_HV[1] = Globa_1->BMS_cell_HV*10;           //单体电池最高电压	 BIN码	2Byte	精确到小数点后三位
	Frame->cell_LV[0] = (Globa_1->BMS_cell_LV*10)>>8;
	Frame->cell_LV[1] = Globa_1->BMS_cell_LV*10;           //单体电池最低电压	 BIN码	2Byte	精确到小数点后三位
	Frame->KWH[0] = (Globa_1->kwh)>>24;
	Frame->KWH[1] = (Globa_1->kwh)>>16;
	Frame->KWH[2] = (Globa_1->kwh)>>8;
	Frame->KWH[3] = (Globa_1->kwh);               //有功总电度 BIN码	4Byte	精确到小数点后二位
	Frame->Time[0] = (Globa_1->Time/60)>>8;
	Frame->Time[1] = Globa_1->Time/60;              //累计充电时间	BIN码	2Byte	单位：min

	Frame->need_time[0] = (Globa_1->BMS_need_time/60)>>8;
	Frame->need_time[1] = Globa_1->BMS_need_time/60;
	
	Frame->Voltmeter_comm_fault_1 = 0;  //  1#电压表通信中断
  Frame->Currtmeter_comm_fault_1 = 0;  //1#电流表通信中断
  Frame->Charge_control_comm_fault_1 = Globa_1->Error.ctrl_connect_lost;  //1#接口板通信中断
	Frame->Meter_comm_fault_1 = Globa_1->Error.meter_connect_lost;  //1#电能表通信中断
  Frame->Card_Reader_comm_fault_1 = 0;//1#读卡器通信中断
  Frame->AC_comm_fault_1 = Globa_1->Error.AC_connect_lost;//1#交流采集模块通信中断
	fault = 0;
	for(i=0;i<Globa_1->Charger_param.Charge_rate_number1;i++){
		fault |= Globa_1->Error.charg_connect_lost[i];
	}
  Frame->Rectifier_comm_fault_1 = fault;//1#组充电模块通信中断

 	fault = 0;
	for(i=0;i<Globa_1->Charger_param.Charge_rate_number1;i++){
		fault |= Globa_1->Error.charg_fault[i];
	}
  Frame->Rectifier_fault_1 = fault;//1#组充电模块故障

  Frame->Emergency_stop_fault = Globa_1->Error.emergency_switch;//急停故障
	 
	fault = 0;
	for(i=0;i<Globa_1->Charger_param.Charge_rate_number1;i++){
		fault |= Globa_1->Error.charg_out_shout[i];
	}
  Frame->Rectifier_output_short_fault_1 = fault;//1#组充电机输出短路故障

  Frame->System_Alarm_Insulation_fault_1 = Globa_1->Error.sistent_alarm; //1#组充电机绝缘故障
	
  fault = 0;
	for(i=0;i<Globa_1->Charger_param.Charge_rate_number1;i++){
		fault |= Globa_1->Error.charg_out_OV[i];
	}
  Frame->Rectifier_output_over_vol_fault_1 = fault;//1#组充电机输出过压保护
  fault = 0;
	for(i=0;i<Globa_1->Charger_param.Charge_rate_number1;i++){
		fault |= Globa_1->Error.charg_in_OV[i];
	}
  Frame->Rectifier_input_over_vol_fault_1 = fault;//1#组充电机输出过压保护
  Frame->system_output_overvoltage_fault_1 = Globa_1->Error.output_over_limit;//1#组充电电压过压告警
  Frame->system_output_over_current_fault_1 = 0;//1#1组充电电流过流告警
  Frame->Disconnect_gun_1 = Globa_1->gun_link;//1#充电枪连接断开
  Frame->BMS_comm_fault_1 = 0 ;//1#1组BMS通信异常
	Frame->Reverse_Battery_1 = 0;//1#1组电池反接
  Frame->battery_voltage_fault_1 = 0;//1#电池电压检测
	Frame->BMS_fault_1= 0;//1#组BMS故障终止充电

	if (dev_cfg.dev_para.car_lock_num == 0)
  {
	  Frame->ground_lock= 0x55;  //地锁1状态
	}
	else
  {
		  if (Globa_1->Error.CarLock_connect_lost == 1)
		  {
			  Frame->ground_lock= 0x66;
		  }
		  else 
		  {
		    Frame->ground_lock = Get_Cardlock_state(0);  //地锁1状态
				printf("\n--！！！------Frame->ground_lock =%d   %d \n",Frame->ground_lock ,Get_Cardlock_state(0));
		  }
	}
}
//心跳 发送上行数据
int ISO8583_AP_Heart_Charging_Sent_01(int fd)
{
	unsigned int ret;
	int len;
	unsigned char SendBuf[512];  //发送数据区指针
	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针

	time_t systime=0;
	struct tm tm_t;

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_AP_Heart_Frame_01 *AP_Heart_Frame;
	ISO8583_AP_Heart_Real_Data_Frame_01 Frame;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	AP_Heart_Frame = (ISO8583_AP_Heart_Frame_01 *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据初始化
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'

	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Heart_Frame_01))>>8;  //报文长度
	Head_Frame->SessionLen[1] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Heart_Frame_01))&0xFF;
	memcpy(&Head_Frame->MessgTypeIdent[0], "1020", sizeof(Head_Frame->MessgTypeIdent));//消息标识符

	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化帧数据内容为 0x00
	
  Head_Frame->PrimaryBitmap[3/8 -0] |= 0x80 >> ((3-1)%8);//主位元表 3
	Head_Frame->PrimaryBitmap[4/8 -0] |= 0x80 >> ((4-1)%8);//主位元表 4
	Head_Frame->PrimaryBitmap[5/8 -0] |= 0x80 >> ((5-1)%8);//主位元表 5
	Head_Frame->PrimaryBitmap[6/8 -0] |= 0x80 >> ((6-1)%8);//主位元表 6
	Head_Frame->PrimaryBitmap[7/8 -0] |= 0x80 >> ((7-1)%8);//主位元表 7
	Head_Frame->PrimaryBitmap[8/8 -1] |= 0x80 >> ((8-1)%8);//主位元表 8
	Head_Frame->PrimaryBitmap[9/8 -0] |= 0x80 >> ((9-1)%8);//主位元表 9

	Head_Frame->PrimaryBitmap[11/8 -0] |= 0x80 >> ((11-1)%8);//主位元表11
	Head_Frame->PrimaryBitmap[33/8 -0] |= 0x80 >> ((33-1)%8);//主位元表 33
	Head_Frame->PrimaryBitmap[35/8 -0] |= 0x80 >> ((35-1)%8);//主位元表 35
	Head_Frame->PrimaryBitmap[36/8 -0] |= 0x80 >> ((36-1)%8);//主位元表 36
	Head_Frame->PrimaryBitmap[37/8 -0] |= 0x80 >> ((37-1)%8);//主位元表 37
	Head_Frame->PrimaryBitmap[38/8 -0] |= 0x80 >> ((38-1)%8);//主位元表 38
	Head_Frame->PrimaryBitmap[41/8 -0] |= 0x80 >> ((41-1)%8);//主位元表 41
	Head_Frame->PrimaryBitmap[62/8 -0] |= 0x80 >> ((62-1)%8);//主位元表 62
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63

	if(Globa_1->App.APP_start == 1){
		AP_Heart_Frame->charge_way[0] = '0';
		AP_Heart_Frame->charge_way[1] = '2';	
		memset(&AP_Heart_Frame->card_sn, '0', sizeof(AP_Heart_Frame->card_sn));
		memcpy(&AP_Heart_Frame->card_sn[0],&Globa_1->App.ID[0], sizeof(Globa_1->App.ID));//先保存一下临时卡号
		 
		if(Globa_1->App.APP_charger_type == 1){
			AP_Heart_Frame->control_type[0] = '0';
			AP_Heart_Frame->control_type[1] = '0';
			AP_Heart_Frame->control_type[2] = '1';	
			sprintf(&tmp[0], "%08d", Globa_1->App.APP_charge_kwh);				
			memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
		}else if(Globa_1->App.APP_charger_type == 2){
			AP_Heart_Frame->control_type[0] = '0';
			AP_Heart_Frame->control_type[1] = '0';
			AP_Heart_Frame->control_type[2] = '2';	
			sprintf(&tmp[0], "%08d", Globa_1->APP_Card_charge_time_limit);				
			memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
		}else if(Globa_1->App.APP_charger_type == 3){
			AP_Heart_Frame->control_type[0] = '0';
			AP_Heart_Frame->control_type[1] = '0';
			AP_Heart_Frame->control_type[2] = '3';	
			sprintf(&tmp[0], "%08d", Globa_1->App.APP_charge_money);				
			memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
		}else if(Globa_1->App.APP_charger_type == 4){
			AP_Heart_Frame->control_type[0] = '0';
			AP_Heart_Frame->control_type[1] = '0';
			AP_Heart_Frame->control_type[2] = '4';	
			memset(&AP_Heart_Frame->charge_limt_data, '0', sizeof(AP_Heart_Frame->charge_limt_data));
		}
		memset(&AP_Heart_Frame->appointment_start_time, '0', sizeof(AP_Heart_Frame->appointment_start_time));
		memset(&AP_Heart_Frame->appointment_end_time, '0', sizeof(AP_Heart_Frame->appointment_end_time));
		memset(&AP_Heart_Frame->stop_car_time, '0', sizeof(AP_Heart_Frame->stop_car_time));
	  memcpy(&AP_Heart_Frame->Busy_SN[0],&Globa_1->App.busy_SN[0], sizeof(Globa_1->App.busy_SN));//先保存一下临时卡号
	}else if(Globa_1->charger_start == 1){
		//AP_Heart_Frame->charge_way[0] = '0';
	//	AP_Heart_Frame->charge_way[1] = '1';	
		if(Globa_1->Start_Mode == VIN_START_TYPE){
			 AP_Heart_Frame->charge_way[0] = '0';
			 AP_Heart_Frame->charge_way[1] = '3';
		}
		else{
			AP_Heart_Frame->charge_way[0] = '0';
			AP_Heart_Frame->charge_way[1] = '1';	
		}
	  memcpy( &AP_Heart_Frame->card_sn[0],&Globa_1->card_sn[0], sizeof(Globa_1->card_sn));//先保存一下临时卡号
	
		if(Globa_1->Card_charger_type == 1){
			AP_Heart_Frame->control_type[0] = '0';
			AP_Heart_Frame->control_type[1] = '0';
			AP_Heart_Frame->control_type[2] = '1';	
			sprintf(&tmp[0], "%08d", Globa_1->QT_charge_kwh);				
			memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
		}else if(Globa_1->Card_charger_type == 2){
			AP_Heart_Frame->control_type[0] = '0';
			AP_Heart_Frame->control_type[1] = '0';
			AP_Heart_Frame->control_type[2] = '2';	
			sprintf(&tmp[0], "%08d", Globa_1->APP_Card_charge_time_limit);				
			memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
		}else if(Globa_1->Card_charger_type == 3){
			AP_Heart_Frame->control_type[0] = '0';
			AP_Heart_Frame->control_type[1] = '0';
			AP_Heart_Frame->control_type[2] = '3';	
			sprintf(&tmp[0], "%08d", Globa_1->QT_charge_money);				
			memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
		}else if(Globa_1->Card_charger_type == 4){
			AP_Heart_Frame->control_type[0] = '0';
			AP_Heart_Frame->control_type[1] = '0';
			AP_Heart_Frame->control_type[2] = '4';	
			memset(&AP_Heart_Frame->charge_limt_data, '0', sizeof(AP_Heart_Frame->charge_limt_data));
		}
	  memset(&AP_Heart_Frame->appointment_start_time, '0', sizeof(AP_Heart_Frame->appointment_start_time));
		memset(&AP_Heart_Frame->appointment_end_time, '0', sizeof(AP_Heart_Frame->appointment_end_time));
		//memset(&AP_Heart_Frame->stop_car_time, '0', sizeof(AP_Heart_Frame->stop_car_time));
	
		if ((dev_cfg.dev_para.car_lock_num != 0)&&((Get_Cardlock_state(0) == 0x01)||(Get_Cardlock_state(0) == 0x02)))
		{
			memcpy(&AP_Heart_Frame->stop_car_time[0],&Globa_1->park_start_time[0], sizeof(Globa_1->park_start_time));
		}
		else
		{			
			memset(&AP_Heart_Frame->stop_car_time, '0', sizeof(AP_Heart_Frame->stop_car_time));
		}
		memset(&AP_Heart_Frame->Busy_SN[0],'0',sizeof(Globa_1->App.busy_SN));
	}

	//充电桩类型（01：单枪直流充电桩，02单相交流充电桩，03三相交流充电桩，04：双枪轮流充直流充电桩，05：双枪同时充直流充电桩）
	AP_Heart_Frame->type[0] = '0';
	AP_Heart_Frame->type[1] = '1';

	AP_Heart_Frame->data_len[0] = (2*sizeof(ISO8583_AP_Heart_Real_Data_Frame_01)/100)%10 + 0x30;
	AP_Heart_Frame->data_len[1] = (2*sizeof(ISO8583_AP_Heart_Real_Data_Frame_01)/10)%10 + 0x30;
	AP_Heart_Frame->data_len[2] = (2*sizeof(ISO8583_AP_Heart_Real_Data_Frame_01)%10) + 0x30;

	//数据包中实时数据处理
	ISO8583_AP_Heart_Real_Data_01(&Frame);
	//hex_to_str(&SendBuf[sizeof(ISO8583_Head_Frame)+5], &Frame, sizeof(Frame));
	hex_to_str(AP_Heart_Frame->data, &Frame, sizeof(Frame));

	sprintf(&tmp[0], "%08d", Globa_1->ISO_8583_rmb);				
	memcpy(&AP_Heart_Frame->rmb[0], &tmp[0], sizeof(AP_Heart_Frame->rmb));

	sprintf(&tmp[0], "%08d", Globa_1->ISO_8583_rmb_Serv);	//服务费金额	37	N	N8			
	memcpy(&AP_Heart_Frame->serv_rmb[0], &tmp[0], sizeof(AP_Heart_Frame->serv_rmb));

	AP_Heart_Frame->gun[0] = '0';
	AP_Heart_Frame->gun[1] = '1';

	memcpy(&AP_Heart_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(AP_Heart_Frame->ter_sn));//充电终端序列号    41	N	n12

	systime = time(NULL);        //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
	sprintf(&tmp[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
	memcpy(&AP_Heart_Frame->time[0], &tmp[0], sizeof(AP_Heart_Frame->time));//终端时间（格式yyyyMMddHHmmss）	62	N	N14

	MD5_DES(&AP_Heart_Frame->charge_way[0], sizeof(ISO8583_AP_Heart_Frame_01)-sizeof(AP_Heart_Frame->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&AP_Heart_Frame->MD5[0], &MD5[0], 8);

	len = ISO8583_Sent_Data(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Heart_Frame_01)), 0);
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Heart_Frame_01))){//如果发送不成功
		return (-1);
	}else{
		COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}


//心跳 发送上行数据
int ISO8583_AP_Heart_NOchargeSent_01(int fd)
{
	unsigned int ret;
	int len = 0;
	unsigned char SendBuf[512];  //发送数据区指针
	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针

	time_t systime=0;
	struct tm tm_t;

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_AP_Heart_NOchargeFrame_01 *AP_Heart_Frame;
	ISO8583_AP_Heart_Real_Data_Frame_01 Frame;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	AP_Heart_Frame = (ISO8583_AP_Heart_NOchargeFrame_01 *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据初始化
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'

	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Heart_NOchargeFrame_01))>>8;  //报文长度
	Head_Frame->SessionLen[1] = sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Heart_NOchargeFrame_01);
	memcpy(&Head_Frame->MessgTypeIdent[0], "1020", sizeof(Head_Frame->MessgTypeIdent));//消息标识符

	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化帧数据内容为 0x00
	Head_Frame->PrimaryBitmap[33/8 -0] |= 0x80 >> ((33-1)%8);//主位元表 33
	Head_Frame->PrimaryBitmap[35/8 -0] |= 0x80 >> ((35-1)%8);//主位元表 35
	Head_Frame->PrimaryBitmap[36/8 -0] |= 0x80 >> ((36-1)%8);//主位元表 36
	Head_Frame->PrimaryBitmap[37/8 -0] |= 0x80 >> ((37-1)%8);//主位元表 37
	Head_Frame->PrimaryBitmap[38/8 -0] |= 0x80 >> ((38-1)%8);//主位元表 38
	Head_Frame->PrimaryBitmap[41/8 -0] |= 0x80 >> ((41-1)%8);//主位元表 41
	Head_Frame->PrimaryBitmap[62/8 -0] |= 0x80 >> ((62-1)%8);//主位元表 62
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63

	//充电桩类型（01：单枪直流充电桩，02单相交流充电桩，03三相交流充电桩，04：双枪轮流充直流充电桩，05：双枪同时充直流充电桩）
	AP_Heart_Frame->type[0] = '0';
	AP_Heart_Frame->type[1] = '1';
	//memset(&AP_Heart_Frame->Busy_SN[0],'0',sizeof(Globa_1->App.busy_SN));

	AP_Heart_Frame->data_len[0] = (2*sizeof(ISO8583_AP_Heart_Real_Data_Frame_01)/100)%10 + 0x30;
	AP_Heart_Frame->data_len[1] = (2*sizeof(ISO8583_AP_Heart_Real_Data_Frame_01)/10)%10 + 0x30;
	AP_Heart_Frame->data_len[2] = (2*sizeof(ISO8583_AP_Heart_Real_Data_Frame_01)%10) + 0x30;

	//数据包中实时数据处理
	ISO8583_AP_Heart_Real_Data_01(&Frame);
	//hex_to_str(&SendBuf[sizeof(ISO8583_Head_Frame)+5], &Frame, sizeof(Frame));
  hex_to_str(&AP_Heart_Frame->data,&Frame, sizeof(Frame));
	sprintf(&tmp[0], "%08d", Globa_1->ISO_8583_rmb);				
	memcpy(&AP_Heart_Frame->rmb[0], &tmp[0], sizeof(AP_Heart_Frame->rmb));

	sprintf(&tmp[0], "%08d", Globa_1->ISO_8583_rmb_Serv);	//服务费金额	37	N	N8			
	memcpy(&AP_Heart_Frame->serv_rmb[0], &tmp[0], sizeof(AP_Heart_Frame->serv_rmb));

	AP_Heart_Frame->gun[0] = '0';
	AP_Heart_Frame->gun[1] = '1';

	memcpy(&AP_Heart_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(AP_Heart_Frame->ter_sn));//充电终端序列号    41	N	n12

	systime = time(NULL);        //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
	sprintf(&tmp[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
	memcpy(&AP_Heart_Frame->time[0], &tmp[0], sizeof(AP_Heart_Frame->time));//终端时间（格式yyyyMMddHHmmss）	62	N	N14

	MD5_DES(&AP_Heart_Frame->type[0], sizeof(ISO8583_AP_Heart_NOchargeFrame_01)-sizeof(AP_Heart_Frame->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&AP_Heart_Frame->MD5[0], &MD5[0], 8);

	len = ISO8583_Sent_Data(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Heart_NOchargeFrame_01)), 0);
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Heart_NOchargeFrame_01))){//如果发送不成功
		return (-1);
	}else{
		COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}


//心跳 数据包中实时数据处理--双枪轮流充电的
int ISO8583_AP_Heart_Real_Data_04(ISO8583_AP_Heart_Real_Data_Frame_04 * Frame,int gun_no)
{
	UINT32 fault = 0,i = 0;
	memset(&Frame->state, 0x00, sizeof(ISO8583_AP_Heart_Real_Data_Frame_04));//初始化帧数据内容为0
	
	if((Globa_1->Error_system != 0)||(Globa_1->Electric_pile_workstart_Enable == 1)){
		Frame->state = 0x01;             //工作状态	BIN码	1Byte	0x01-故障  0x02-告警  03-空闲 0x04-充电中 0x05-完成  0x06-预约
	}else{
		if((Globa_1->charger_state == 0x04)||(Globa_2->charger_state == 0x04)){
		  Frame->state = 0x04;  //此处只有    03-空闲 0x04-充电中 0x05-完成 0x06-预约
		}else	if(((Globa_1->charger_state == 0x05)&&(Globa_2->charger_state = 0x03))||((Globa_2->charger_state == 0x05)&&(Globa_1->charger_state = 0x03))){
		  Frame->state = 0x05;  //此处只有    03-空闲 0x04-充电中 0x05-完成 0x06-预约
		}else	if(((Globa_1->charger_state == 0x06)&&(Globa_2->charger_state = 0x03))||((Globa_2->charger_state == 0x06)&&(Globa_1->charger_state = 0x03))){
		  Frame->state = 0x06;  //此处只有    03-空闲 0x04-充电中 0x05-完成 0x06-预约
		}else{
		  Frame->state = 0x03;
		}
	}

	Frame->gun1 = Globa_1->gun_state;
	Frame->gun2 = Globa_2->gun_state;
	
  if((gun_no == 1)||(gun_no == 2)){
		Frame->Output_voltage[0] = (Globa_2->Output_voltage/100)>>8;
		Frame->Output_voltage[1] = Globa_2->Output_voltage/100;      //充电机输出电压 BIN码	2Byte	精确到小数点后一位
		Frame->Output_current[0] = (Globa_2->Output_current/10)>>8;
		Frame->Output_current[1] = Globa_2->Output_current/10;      //充电机输出电流 BIN码	2Byte	精确到小数点后二位
		Frame->batt_SOC = Globa_2->BMS_batt_SOC;                    //SOCBIN码	1Byte	整型
		//电池组最高温度 BIN码	2Byte	精确到小数点后一位
		Frame->batt_LT[0] = ((Globa_2->BMS_cell_LT - (TEMP_OFSET))*10)>>8;
		Frame->batt_LT[1] = (Globa_2->BMS_cell_LT - (TEMP_OFSET))*10;           //电池组最低温度 BIN码	2Byte	精确到小数点后一位
		Frame->batt_HT[0] = ((Globa_2->BMS_cell_HT - (TEMP_OFSET))*10)>>8;
		Frame->batt_HT[1] = (Globa_2->BMS_cell_HT - (TEMP_OFSET))*10;           //电池组最高温度 BIN码	2Byte	精确到小数点后一位

		Frame->cell_HV[0] = (Globa_2->BMS_cell_HV*10)>>8;
		Frame->cell_HV[1] = Globa_2->BMS_cell_HV*10;           //单体电池最高电压	 BIN码	2Byte	精确到小数点后三位
		Frame->cell_LV[0] = (Globa_2->BMS_cell_LV*10)>>8;
		Frame->cell_LV[1] = Globa_2->BMS_cell_LV*10;           //单体电池最低电压	 BIN码	2Byte	精确到小数点后三位
		Frame->KWH[0] = (Globa_2->kwh)>>24;
		Frame->KWH[1] = (Globa_2->kwh)>>16;
		Frame->KWH[2] = (Globa_2->kwh)>>8;
		Frame->KWH[3] = (Globa_2->kwh);               //有功总电度 BIN码	4Byte	精确到小数点后二位
		Frame->Time[0] = (Globa_2->Time/60)>>8;
		Frame->Time[1] = Globa_2->Time/60;   
		Frame->need_time[0] = (Globa_2->BMS_need_time/60)>>8;
		Frame->need_time[1] = Globa_2->BMS_need_time/60;
	}else{
		Frame->Output_voltage[0] = (Globa_1->Output_voltage/100)>>8;
		Frame->Output_voltage[1] = Globa_1->Output_voltage/100;      //充电机输出电压 BIN码	2Byte	精确到小数点后一位
		Frame->Output_current[0] = (Globa_1->Output_current/10)>>8;
		Frame->Output_current[1] = Globa_1->Output_current/10;      //充电机输出电流 BIN码	2Byte	精确到小数点后二位
		Frame->batt_SOC = Globa_1->BMS_batt_SOC;                    //SOCBIN码	1Byte	整型
		Frame->batt_LT[0] = ((Globa_1->BMS_cell_LT - (TEMP_OFSET))*10)>>8;
		Frame->batt_LT[1] = (Globa_1->BMS_cell_LT - (TEMP_OFSET))*10;           //电池组最低温度 BIN码	2Byte	精确到小数点后一位
		Frame->batt_HT[0] = ((Globa_1->BMS_cell_HT - (TEMP_OFSET))*10)>>8;
		Frame->batt_HT[1] = (Globa_1->BMS_cell_HT - (TEMP_OFSET ))*10;           //电池组最高温度 BIN码	2Byte	精确到小数点后一位

		Frame->cell_HV[0] = (Globa_1->BMS_cell_HV*10)>>8;
		Frame->cell_HV[1] = Globa_1->BMS_cell_HV*10;           //单体电池最高电压	 BIN码	2Byte	精确到小数点后三位
		Frame->cell_LV[0] = (Globa_1->BMS_cell_LV*10)>>8;
		Frame->cell_LV[1] = Globa_1->BMS_cell_LV*10;           //单体电池最低电压	 BIN码	2Byte	精确到小数点后三位
		Frame->KWH[0] = (Globa_1->kwh)>>24;
		Frame->KWH[1] = (Globa_1->kwh)>>16;
		Frame->KWH[2] = (Globa_1->kwh)>>8;
		Frame->KWH[3] = (Globa_1->kwh);               //有功总电度 BIN码	4Byte	精确到小数点后二位
		Frame->Time[0] = (Globa_1->Time/60)>>8;
		Frame->Time[1] = Globa_1->Time/60;              //累计充电时间	BIN码	2Byte	单位：min

		Frame->need_time[0] = (Globa_1->BMS_need_time/60)>>8;
		Frame->need_time[1] = Globa_1->BMS_need_time/60;
	}
	Frame->Voltmeter_comm_fault_1 = 0;  //  1#电压表通信中断
  Frame->Currtmeter_comm_fault_1 = 0;  //1#电流表通信中断
  Frame->Charge_control_comm_fault_1 = Globa_1->Error.ctrl_connect_lost|Globa_2->Error.ctrl_connect_lost;  //1#接口板通信中断
	Frame->Meter_comm_fault_1 = Globa_1->Error.meter_connect_lost;  //1#电能表通信中断
  Frame->Card_Reader_comm_fault_1 = 0;//1#读卡器通信中断
  Frame->AC_comm_fault_1 = Globa_1->Error.AC_connect_lost;//1#交流采集模块通信中断
	fault = 0;
	for(i=0;i<Globa_1->Charger_param.Charge_rate_number1;i++){
		fault |= Globa_1->Error.charg_connect_lost[i];
	}
  Frame->Rectifier_comm_fault_1 = fault;//1#组充电模块通信中断

 	fault = 0;
	for(i=0;i<Globa_1->Charger_param.Charge_rate_number1;i++){
		fault |= Globa_1->Error.charg_fault[i];
	}
  Frame->Rectifier_fault_1 = fault;//1#组充电模块故障

  Frame->Emergency_stop_fault = Globa_1->Error.emergency_switch;//急停故障
	 
	fault = 0;
	for(i=0;i<Globa_1->Charger_param.Charge_rate_number1;i++){
		fault |= Globa_1->Error.charg_out_shout[i];
	}
  Frame->Rectifier_output_short_fault_1 = fault;//1#组充电机输出短路故障

  Frame->System_Alarm_Insulation_fault_1 = Globa_1->Error.sistent_alarm; //1#组充电机绝缘故障
  Frame->System_Alarm_Insulation_fault_1 = Globa_2->Error.sistent_alarm; //1#组充电机绝缘故障

  fault = 0;
	for(i=0;i<Globa_1->Charger_param.Charge_rate_number1;i++){
		fault |= Globa_1->Error.charg_out_OV[i];
	}
  Frame->Rectifier_output_over_vol_fault_1 = fault;//1#组充电机输出过压保护
  fault = 0;
	for(i=0;i<Globa_1->Charger_param.Charge_rate_number1;i++){
		fault |= Globa_1->Error.charg_in_OV[i];
	}
  Frame->Rectifier_input_over_vol_fault_1 = fault;//1#组充电机输出过压保护
  Frame->system_output_overvoltage_fault_1 = Globa_1->Error.output_over_limit;//1#组充电电压过压告警
  Frame->system_output_over_current_fault_1 = 0;//1#1组充电电流过流告警
  Frame->Disconnect_gun_1 = Globa_1->gun_link;//1#充电枪连接断开
  Frame->Disconnect_gun_2 = Globa_2->gun_link;//1#充电枪连接断开
	Frame->BMS_comm_fault_1 = 0 ;//1#1组BMS通信异常
	Frame->Reverse_Battery_1 = 0;//1#1组电池反接
  Frame->battery_voltage_fault_1 = 0;//1#电池电压检测
	Frame->BMS_fault_1= 0;//1#组BMS故障终止充电
	if (dev_cfg.dev_para.car_lock_num == 0)
  {
	  Frame->ground_lock_1= 0x55;  //地锁1状态
	  Frame->ground_lock_2 = 0x55;  //地锁2状态
	}
	else
  {
		  if (Globa_1->Error.CarLock_connect_lost == 1)
		  {
			  Frame->ground_lock_1= 0x66;
		  }
		  else 
		  {
		    Frame->ground_lock_1= Get_Cardlock_state(0);  //地锁1状态
		  }
		 
		  if (dev_cfg.dev_para.car_lock_num == 2)
		  {
			  if (Globa_2->Error.CarLock_connect_lost == 1)
			  {
					Frame->ground_lock_2= 0x66;
			  }
			  else 
			  {
				  Frame->ground_lock_2= Get_Cardlock_state(1);  //地锁1状态
			  }
		  }else {
				Frame->ground_lock_2 = 0x55;  //地锁2状态
			}
	}
}
//心跳 发送上行数据
int ISO8583_AP_Heart_ChargeSent_04(int fd,int gun_no)
{
	unsigned int ret,j;
	int len = 0;
	unsigned char SendBuf[512];  //发送数据区指针
	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针

	time_t systime=0;
	struct tm tm_t;

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_AP_Heart_Frame_04 *AP_Heart_Frame;
	ISO8583_AP_Heart_Real_Data_Frame_04 Frame;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	AP_Heart_Frame = (ISO8583_AP_Heart_Frame_04 *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据初始化
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'

	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Heart_Frame_04))>>8;  //报文长度
	Head_Frame->SessionLen[1] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Heart_Frame_04))&0xFF;
	memcpy(&Head_Frame->MessgTypeIdent[0], "1020", sizeof(Head_Frame->MessgTypeIdent));//消息标识符

	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化帧数据内容为 0x00
	
  Head_Frame->PrimaryBitmap[3/8 -0] |= 0x80 >> ((3-1)%8);//主位元表 3
	Head_Frame->PrimaryBitmap[4/8 -0] |= 0x80 >> ((4-1)%8);//主位元表 4
	Head_Frame->PrimaryBitmap[5/8 -0] |= 0x80 >> ((5-1)%8);//主位元表 5
	Head_Frame->PrimaryBitmap[6/8 -0] |= 0x80 >> ((6-1)%8);//主位元表 6
	Head_Frame->PrimaryBitmap[7/8 -0] |= 0x80 >> ((7-1)%8);//主位元表 7
	Head_Frame->PrimaryBitmap[8/8 -1] |= 0x80 >> ((8-1)%8);//主位元表 8
	Head_Frame->PrimaryBitmap[9/8 -0] |= 0x80 >> ((9-1)%8);//主位元表 9

	Head_Frame->PrimaryBitmap[11/8 -0] |= 0x80 >> ((11-1)%8);//主位元表11
	Head_Frame->PrimaryBitmap[33/8 -0] |= 0x80 >> ((33-1)%8);//主位元表 33
	Head_Frame->PrimaryBitmap[35/8 -0] |= 0x80 >> ((35-1)%8);//主位元表 35
	Head_Frame->PrimaryBitmap[36/8 -0] |= 0x80 >> ((36-1)%8);//主位元表 36
	Head_Frame->PrimaryBitmap[37/8 -0] |= 0x80 >> ((37-1)%8);//主位元表 37
	Head_Frame->PrimaryBitmap[38/8 -0] |= 0x80 >> ((38-1)%8);//主位元表 38
	
	Head_Frame->PrimaryBitmap[41/8 -0] |= 0x80 >> ((41-1)%8);//主位元表 41
	Head_Frame->PrimaryBitmap[62/8 -0] |= 0x80 >> ((62-1)%8);//主位元表 62
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63
 
	if(gun_no == 1){
		if(Globa_2->App.APP_start == 1){
			AP_Heart_Frame->charge_way[0] = '0';
			AP_Heart_Frame->charge_way[1] = '2';	
			memcpy(&AP_Heart_Frame->card_sn[0],&Globa_2->App.ID[0], sizeof(Globa_2->App.ID));//先保存一下临时卡号
			 
			if(Globa_2->App.APP_charger_type == 1){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '1';	
				sprintf(&tmp[0], "%08d", Globa_2->App.APP_charge_kwh);				
				memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
			}else if(Globa_2->App.APP_charger_type == 2){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '2';	
				sprintf(&tmp[0], "%08d", Globa_2->APP_Card_charge_time_limit);				
				memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
			}else if(Globa_2->App.APP_charger_type == 3){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '3';	
				sprintf(&tmp[0], "%08d", Globa_2->App.APP_charge_money);				
				memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
			}else if(Globa_2->App.APP_charger_type == 4){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '4';	
				memset(&AP_Heart_Frame->charge_limt_data, '0', sizeof(AP_Heart_Frame->charge_limt_data));
			}
			memset(&AP_Heart_Frame->appointment_start_time, '0', sizeof(AP_Heart_Frame->appointment_start_time));
			memset(&AP_Heart_Frame->appointment_end_time, '0', sizeof(AP_Heart_Frame->appointment_end_time));
			memset(&AP_Heart_Frame->stop_car_time, '0', sizeof(AP_Heart_Frame->stop_car_time));
			if ((dev_cfg.dev_para.car_lock_num == 2)&&((Get_Cardlock_state(1) == 0x01)||(Get_Cardlock_state(1) == 0x02)))
			{
				memcpy(&AP_Heart_Frame->stop_car_time[0],&Globa_2->park_start_time[0], sizeof(Globa_2->park_start_time));
			}
			else
			{			
    		memset(&AP_Heart_Frame->stop_car_time, '0', sizeof(AP_Heart_Frame->stop_car_time));
			}
			memcpy(&AP_Heart_Frame->Busy_SN[0],&Globa_2->App.busy_SN[0], sizeof(Globa_2->App.busy_SN));//先保存一下临时卡号
		}else if(Globa_2->charger_start == 1){
			//AP_Heart_Frame->charge_way[0] = '0';
			//AP_Heart_Frame->charge_way[1] = '1';	
			if(Globa_2->Start_Mode == VIN_START_TYPE){
				 AP_Heart_Frame->charge_way[0] = '0';
			   AP_Heart_Frame->charge_way[1] = '3';
			}
			else{
			  AP_Heart_Frame->charge_way[0] = '0';
			  AP_Heart_Frame->charge_way[1] = '1';	
			}
			memcpy(&AP_Heart_Frame->card_sn[0],&Globa_2->card_sn[0], sizeof(Globa_2->card_sn));//先保存一下临时卡号
			
			if(Globa_2->Card_charger_type == 1){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '1';	
				sprintf(&tmp[0], "%08d", Globa_2->QT_charge_kwh);				
				memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
			}else if(Globa_2->Card_charger_type == 2){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '2';	
				sprintf(&tmp[0], "%08d", Globa_2->APP_Card_charge_time_limit);				
				memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
			}else if(Globa_2->Card_charger_type == 3){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '3';	
				sprintf(&tmp[0], "%08d", Globa_2->QT_charge_money);				
				memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
			}else if(Globa_2->Card_charger_type == 4){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '4';	
				memset(&AP_Heart_Frame->charge_limt_data, '0', sizeof(AP_Heart_Frame->charge_limt_data));
			}
			memset(&AP_Heart_Frame->appointment_start_time, '0', sizeof(AP_Heart_Frame->appointment_start_time));
			memset(&AP_Heart_Frame->appointment_end_time, '0', sizeof(AP_Heart_Frame->appointment_end_time));
			//memset(&AP_Heart_Frame->stop_car_time, '0', sizeof(AP_Heart_Frame->stop_car_time));
			if ((dev_cfg.dev_para.car_lock_num != 0)&&((Get_Cardlock_state(0) == 0x01)||(Get_Cardlock_state(0) == 0x02)))
			{
				memcpy(&AP_Heart_Frame->stop_car_time[0],&Globa_1->park_start_time[0], sizeof(Globa_1->park_start_time));
			}
			else
			{			
    		memset(&AP_Heart_Frame->stop_car_time, '0', sizeof(AP_Heart_Frame->stop_car_time));
			}
			memset(&AP_Heart_Frame->Busy_SN[0],'0',sizeof(Globa_2->App.busy_SN));
		}

		//充电桩类型（01：单枪直流充电桩，02单相交流充电桩，03三相交流充电桩，04：双枪轮流充直流充电桩，05：双枪同时充直流充电桩）
		AP_Heart_Frame->type[0] = '0';
		AP_Heart_Frame->type[1] = '4';

		AP_Heart_Frame->data_len[0] = (2*sizeof(ISO8583_AP_Heart_Real_Data_Frame_04)/100)%10 + 0x30;
		AP_Heart_Frame->data_len[1] = (2*sizeof(ISO8583_AP_Heart_Real_Data_Frame_04)/10)%10 + 0x30;
		AP_Heart_Frame->data_len[2] = (2*sizeof(ISO8583_AP_Heart_Real_Data_Frame_04)%10) + 0x30;

		//数据包中实时数据处理
		ISO8583_AP_Heart_Real_Data_04(&Frame,gun_no);
		//hex_to_str(&SendBuf[sizeof(ISO8583_Head_Frame)+5], &Frame, sizeof(Frame));
		hex_to_str(AP_Heart_Frame->data, &Frame, sizeof(Frame));

		sprintf(&tmp[0], "%08d", Globa_2->ISO_8583_rmb);				
		memcpy(&AP_Heart_Frame->rmb[0], &tmp[0], sizeof(AP_Heart_Frame->rmb));

		sprintf(&tmp[0], "%08d", Globa_2->ISO_8583_rmb_Serv);	//服务费金额	37	N	N8			
		memcpy(&AP_Heart_Frame->serv_rmb[0], &tmp[0], sizeof(AP_Heart_Frame->serv_rmb));

		AP_Heart_Frame->gun[0] = '0';
		AP_Heart_Frame->gun[1] = '2';
	}else{
		if(Globa_1->App.APP_start == 1){
			AP_Heart_Frame->charge_way[0] = '0';
			AP_Heart_Frame->charge_way[1] = '2';	
			memcpy(&AP_Heart_Frame->card_sn[0],&Globa_1->App.ID[0], sizeof(Globa_1->App.ID));//先保存一下临时卡号
			 
			if(Globa_1->App.APP_charger_type == 1){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '1';	
				sprintf(&tmp[0], "%08d", Globa_1->App.APP_charge_kwh);				
				memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
			}else if(Globa_1->App.APP_charger_type == 2){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '2';	
				sprintf(&tmp[0], "%08d", Globa_1->APP_Card_charge_time_limit);				
				memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
			}else if(Globa_1->App.APP_charger_type == 3){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '3';	
				sprintf(&tmp[0], "%08d", Globa_1->App.APP_charge_money);				
				memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
			}else if(Globa_1->App.APP_charger_type == 4){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '4';	
				memset(&AP_Heart_Frame->charge_limt_data, '0', sizeof(AP_Heart_Frame->charge_limt_data));
			}
			memset(&AP_Heart_Frame->appointment_start_time, '0', sizeof(AP_Heart_Frame->appointment_start_time));
			memset(&AP_Heart_Frame->appointment_end_time, '0', sizeof(AP_Heart_Frame->appointment_end_time));
			//memset(&AP_Heart_Frame->stop_car_time, '0', sizeof(AP_Heart_Frame->stop_car_time));
			if ((dev_cfg.dev_para.car_lock_num == 2)&&((Get_Cardlock_state(0) == 0x01)||(Get_Cardlock_state(0) == 0x02)))
			{
				memcpy(&AP_Heart_Frame->stop_car_time[0],&Globa_1->park_start_time[0], sizeof(Globa_1->park_start_time));
			}
			else
			{			
    		memset(&AP_Heart_Frame->stop_car_time, '0', sizeof(AP_Heart_Frame->stop_car_time));
			}
			memcpy(&AP_Heart_Frame->Busy_SN[0],&Globa_1->App.busy_SN[0], sizeof(Globa_1->App.busy_SN));//先保存一下临时卡号
		}else if(Globa_1->charger_start == 1){
			//AP_Heart_Frame->charge_way[0] = '0';
			//AP_Heart_Frame->charge_way[1] = '1';	
		  if(Globa_1->Start_Mode == VIN_START_TYPE){
				 AP_Heart_Frame->charge_way[0] = '0';
			   AP_Heart_Frame->charge_way[1] = '3';
			}
			else{
			  AP_Heart_Frame->charge_way[0] = '0';
			  AP_Heart_Frame->charge_way[1] = '1';	
			}
			memcpy(&AP_Heart_Frame->card_sn[0],&Globa_1->card_sn[0], sizeof(Globa_1->card_sn));//先保存一下临时卡号
			
			if(Globa_1->Card_charger_type == 1){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '1';	
				sprintf(&tmp[0], "%08d", Globa_1->QT_charge_kwh);				
				memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
			}else if(Globa_1->Card_charger_type == 2){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '2';	
				sprintf(&tmp[0], "%08d", Globa_1->APP_Card_charge_time_limit);				
				memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
			}else if(Globa_1->Card_charger_type == 3){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '3';	
				sprintf(&tmp[0], "%08d", Globa_1->QT_charge_money);				
				memcpy(&AP_Heart_Frame->charge_limt_data[0], &tmp[0], sizeof(AP_Heart_Frame->charge_limt_data));
			}else if(Globa_1->Card_charger_type == 4){
				AP_Heart_Frame->control_type[0] = '0';
				AP_Heart_Frame->control_type[1] = '0';
				AP_Heart_Frame->control_type[2] = '4';	
				memset(&AP_Heart_Frame->charge_limt_data, '0', sizeof(AP_Heart_Frame->charge_limt_data));
			}
			memset(&AP_Heart_Frame->appointment_start_time, '0', sizeof(AP_Heart_Frame->appointment_start_time));
			memset(&AP_Heart_Frame->appointment_end_time, '0', sizeof(AP_Heart_Frame->appointment_end_time));
			//memset(&AP_Heart_Frame->stop_car_time, '0', sizeof(AP_Heart_Frame->stop_car_time));
			if ((dev_cfg.dev_para.car_lock_num != 0)&&((Get_Cardlock_state(0) == 0x01)||(Get_Cardlock_state(0) == 0x02)))
			{
				memcpy(&AP_Heart_Frame->stop_car_time[0],&Globa_1->park_start_time[0], sizeof(Globa_1->park_start_time));
			}
			else
			{			
    		memset(&AP_Heart_Frame->stop_car_time, '0', sizeof(AP_Heart_Frame->stop_car_time));
			}
			memset(&AP_Heart_Frame->Busy_SN[0],'0',sizeof(Globa_1->App.busy_SN));
		}

		//充电桩类型（01：单枪直流充电桩，02单相交流充电桩，03三相交流充电桩，04：双枪轮流充直流充电桩，05：双枪同时充直流充电桩）
		AP_Heart_Frame->type[0] = '0';
		AP_Heart_Frame->type[1] = '4';

		AP_Heart_Frame->data_len[0] = (2*sizeof(ISO8583_AP_Heart_Real_Data_Frame_04)/100)%10 + 0x30;
		AP_Heart_Frame->data_len[1] = (2*sizeof(ISO8583_AP_Heart_Real_Data_Frame_04)/10)%10 + 0x30;
		AP_Heart_Frame->data_len[2] = (2*sizeof(ISO8583_AP_Heart_Real_Data_Frame_04)%10) + 0x30;

		//数据包中实时数据处理
		ISO8583_AP_Heart_Real_Data_04(&Frame,gun_no);
		//hex_to_str(&SendBuf[sizeof(ISO8583_Head_Frame)+5], &Frame, sizeof(Frame));
		hex_to_str(AP_Heart_Frame->data, &Frame, sizeof(Frame));

		sprintf(&tmp[0], "%08d", Globa_1->ISO_8583_rmb);				
		memcpy(&AP_Heart_Frame->rmb[0], &tmp[0], sizeof(AP_Heart_Frame->rmb));

		sprintf(&tmp[0], "%08d", Globa_1->ISO_8583_rmb_Serv);	//服务费金额	37	N	N8			
		memcpy(&AP_Heart_Frame->serv_rmb[0], &tmp[0], sizeof(AP_Heart_Frame->serv_rmb));

		AP_Heart_Frame->gun[0] = '0';
	  AP_Heart_Frame->gun[1] = '1';
	}
	
	memcpy(&AP_Heart_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(AP_Heart_Frame->ter_sn));//充电终端序列号    41	N	n1
	systime = time(NULL);        //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
	sprintf(&tmp[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
	memcpy(&AP_Heart_Frame->time[0], &tmp[0], sizeof(AP_Heart_Frame->time));//终端时间（格式yyyyMMddHHmmss）	62	N	N14

	MD5_DES(&AP_Heart_Frame->charge_way[0], sizeof(ISO8583_AP_Heart_Frame_04)-sizeof(AP_Heart_Frame->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&AP_Heart_Frame->MD5[0], &MD5[0], 8);

	len = ISO8583_Sent_Data(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Heart_Frame_04)), 0);
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Heart_Frame_04))){//如果发送不成功
		return (-1);
	}else{
		COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}

//心跳 发送上行数据
int ISO8583_AP_Heart_NOchargeSent_04(int fd,int gun_no)
{
	unsigned int ret;
	int len = 0;
	unsigned char SendBuf[512];  //发送数据区指针
	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针

	time_t systime=0;
	struct tm tm_t;

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_AP_Heart_NOchargeFrame_04 *AP_Heart_Frame;
	ISO8583_AP_Heart_Real_Data_Frame_04 Frame;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	AP_Heart_Frame = (ISO8583_AP_Heart_NOchargeFrame_04 *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据初始化
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'

	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Heart_NOchargeFrame_04))>>8;  //报文长度
	Head_Frame->SessionLen[1] = sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Heart_NOchargeFrame_04);
	memcpy(&Head_Frame->MessgTypeIdent[0], "1020", sizeof(Head_Frame->MessgTypeIdent));//消息标识符

	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化帧数据内容为 0x00
	Head_Frame->PrimaryBitmap[33/8 -0] |= 0x80 >> ((33-1)%8);//主位元表 33
	Head_Frame->PrimaryBitmap[35/8 -0] |= 0x80 >> ((35-1)%8);//主位元表 35
	Head_Frame->PrimaryBitmap[36/8 -0] |= 0x80 >> ((36-1)%8);//主位元表 36
	Head_Frame->PrimaryBitmap[37/8 -0] |= 0x80 >> ((37-1)%8);//主位元表 37
	Head_Frame->PrimaryBitmap[38/8 -0] |= 0x80 >> ((38-1)%8);//主位元表 38
	Head_Frame->PrimaryBitmap[41/8 -0] |= 0x80 >> ((41-1)%8);//主位元表 41
	Head_Frame->PrimaryBitmap[62/8 -0] |= 0x80 >> ((62-1)%8);//主位元表 62
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63

	//充电桩类型（01：单枪直流充电桩，02单相交流充电桩，03三相交流充电桩，04：双枪轮流充直流充电桩，05：双枪同时充直流充电桩）
	AP_Heart_Frame->type[0] = '0';
	AP_Heart_Frame->type[1] = '4';

	AP_Heart_Frame->data_len[0] = (2*sizeof(ISO8583_AP_Heart_Real_Data_Frame_04)/100)%10 + 0x30;
	AP_Heart_Frame->data_len[1] = (2*sizeof(ISO8583_AP_Heart_Real_Data_Frame_04)/10)%10 + 0x30;
	AP_Heart_Frame->data_len[2] = (2*sizeof(ISO8583_AP_Heart_Real_Data_Frame_04)%10) + 0x30;

	//数据包中实时数据处理
	ISO8583_AP_Heart_Real_Data_04(&Frame, gun_no);
	//hex_to_str(&SendBuf[sizeof(ISO8583_Head_Frame)+5], &Frame, sizeof(Frame));
	hex_to_str(&AP_Heart_Frame->data,&Frame, sizeof(Frame));

  if(gun_no == 1){
		sprintf(&tmp[0], "%08d", Globa_2->ISO_8583_rmb);				
		memcpy(&AP_Heart_Frame->rmb[0], &tmp[0], sizeof(AP_Heart_Frame->rmb));

		sprintf(&tmp[0], "%08d", Globa_2->ISO_8583_rmb_Serv);	//服务费金额	37	N	N8			
		memcpy(&AP_Heart_Frame->serv_rmb[0], &tmp[0], sizeof(AP_Heart_Frame->serv_rmb));

		AP_Heart_Frame->gun[0] = '0';
		AP_Heart_Frame->gun[1] = '2';
	}else{
		sprintf(&tmp[0], "%08d", Globa_1->ISO_8583_rmb);				
		memcpy(&AP_Heart_Frame->rmb[0], &tmp[0], sizeof(AP_Heart_Frame->rmb));

		sprintf(&tmp[0], "%08d", Globa_1->ISO_8583_rmb_Serv);	//服务费金额	37	N	N8			
		memcpy(&AP_Heart_Frame->serv_rmb[0], &tmp[0], sizeof(AP_Heart_Frame->serv_rmb));
		AP_Heart_Frame->gun[0] = '0';
		AP_Heart_Frame->gun[1] = '1';
	}
	
	memcpy(&AP_Heart_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(AP_Heart_Frame->ter_sn));//充电终端序列号    41	N	n12

	systime = time(NULL);        //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
	sprintf(&tmp[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
	memcpy(&AP_Heart_Frame->time[0], &tmp[0], sizeof(AP_Heart_Frame->time));//终端时间（格式yyyyMMddHHmmss）	62	N	N14

	MD5_DES(&AP_Heart_Frame->type[0], sizeof(ISO8583_AP_Heart_NOchargeFrame_04)-sizeof(AP_Heart_Frame->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&AP_Heart_Frame->MD5[0], &MD5[0], 8);

	len = ISO8583_Sent_Data(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Heart_NOchargeFrame_04)), 0);
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Heart_NOchargeFrame_04))){//如果发送不成功
		return (-1);
	}else{
		COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}


//心跳发送数据：
int ISO8583_AP_Heart_Sent(int fd)
{
	static unsigned gunflg = 0;
	int ret=0;
	
	if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪同时
		if(gunflg == 0){//第一把枪
			ISO8583_App_Struct.old_gun_state[0] = Globa_1->gun_state;
		  ISO8583_App_Struct.old_Car_Lock_state[0] = Get_Cardlock_state(0);
			if((Globa_1->charger_start == 1)||(Globa_1->App.APP_start == 1)){
				ret = ISO8583_AP_Heart_Charging_Sent(fd, 0);//发送心跳帧
			}else{
				ret = ISO8583_AP_Heart_NoCharging_Sent(fd, 0);//发送心跳帧
			}
			gunflg = 1;
			return (ret);
		}else if(gunflg == 1){//第二把枪
		  ISO8583_App_Struct.old_gun_state[1] = Globa_2->gun_state;
			ISO8583_App_Struct.old_Car_Lock_state[1] = Get_Cardlock_state(1);
			if((Globa_2->charger_start == 1)||(Globa_2->App.APP_start == 1)){
				ret = ISO8583_AP_Heart_Charging_Sent(fd, 1);//发送心跳帧
			}else if((Globa_2->charger_start == 0)&&(Globa_1->Charger_param.System_Type == 4 )&&(Globa_1->Double_Gun_To_Car_falg == 1)&&(Globa_1->charger_start == 1)){
				ret = ISO8583_AP_Heart_Charging_Sent(fd, 2);//发送心跳帧
			}else{
				ret = ISO8583_AP_Heart_NoCharging_Sent(fd, 1);//发送心跳帧
			}
			gunflg = 0;
			return (ret);
		}
	}else	if(Globa_1->Charger_param.System_Type == 1 ){//双枪轮流
		if(gunflg == 0){//第一把枪
			ISO8583_App_Struct.old_gun_state[0] = Globa_1->gun_state;
		  ISO8583_App_Struct.old_Car_Lock_state[0] = Get_Cardlock_state(0);

			if((Globa_1->charger_start == 1)||(Globa_1->App.APP_start == 1)){
				ret = ISO8583_AP_Heart_ChargeSent_04(fd, 0);//发送心跳帧
			}else{
				ret = ISO8583_AP_Heart_NOchargeSent_04(fd, 0);//发送心跳帧
			}
			gunflg = 1;
			return (ret);
		}else if(gunflg == 1){//第二把枪
			ISO8583_App_Struct.old_gun_state[1] = Globa_2->gun_state;
		  ISO8583_App_Struct.old_Car_Lock_state[1] = Get_Cardlock_state(1);

			if((Globa_2->charger_start == 1)||(Globa_2->App.APP_start == 1)){
				ret = ISO8583_AP_Heart_ChargeSent_04(fd, 1);//发送心跳帧
			}else{
				ret = ISO8583_AP_Heart_NOchargeSent_04(fd, 1);//发送心跳帧
			}
			gunflg = 0;
			return (ret);
		}
	}else{//单枪
		ISO8583_App_Struct.old_gun_state[0] = Globa_1->gun_state;
		ISO8583_App_Struct.old_Car_Lock_state[0] = Get_Cardlock_state(0);
		if(Globa_1->charger_start != 0){
		  ret = ISO8583_AP_Heart_Charging_Sent_01(fd);//发送心跳帧
		}else{
			ret = ISO8583_AP_Heart_NOchargeSent_01(fd);//发送心跳帧
		}
		return (ret);
	}
	return (0);
}
//心跳
int ISO8583_Heart_Socket_Rece_Deal(int ret, unsigned char *buf)
{
	int j;
	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针

	UINT32 temp_kw = 0;					      //充电机输出限制功率  kw

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_AP_Heart_Resp_Frame *AP_Heart_Resp_Frame;

	Head_Frame = (ISO8583_Head_Frame *)(buf);
	AP_Heart_Resp_Frame = (ISO8583_AP_Heart_Resp_Frame *)(buf+sizeof(ISO8583_Head_Frame));

  //if(ret == (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Heart_Resp_Frame))){//表明客户端有数据发送过来，作相应接受处理
	MD5_DES(&AP_Heart_Resp_Frame->flag[0], (ret - sizeof(ISO8583_Head_Frame)-sizeof(AP_Heart_Resp_Frame->MD5)), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&tmp[0], &MD5[0], 8);
	if(0 == memcmp(&tmp[0], &AP_Heart_Resp_Frame->MD5[0], sizeof(AP_Heart_Resp_Frame->MD5))){
	  time(&Rcev_current_now);
		Rcev_pre_time = Rcev_current_now;
		
		if((AP_Heart_Resp_Frame->flag[0] == '0')&&(AP_Heart_Resp_Frame->flag[1] == '0')){
			if((AP_Heart_Resp_Frame->work_Enable[0] == '0')&&(AP_Heart_Resp_Frame->work_Enable[1] == '1')){
				Globa_1->Electric_pile_workstart_Enable = 1;
				Globa_2->Electric_pile_workstart_Enable = 1;
			}else{
				Globa_1->Electric_pile_workstart_Enable = 0;
				Globa_2->Electric_pile_workstart_Enable = 0;
			}
			if(Globa_1->Electric_pile_workstart_Enable != gBackgroupIssued.Electric_pile_workstart_Enable){
				gBackgroupIssued.Electric_pile_workstart_Enable = Globa_1->Electric_pile_workstart_Enable;
				System_BackgroupIssued_Param_save();//保存系统参数
			}
			//------------------------功率控制 -------------------------
			sprintf(tmp, "%.4s", &AP_Heart_Resp_Frame->power_a[0]);
			temp_kw = atoi(tmp);
			if(Globa_1->Charger_param.limit_power != temp_kw){
				if((temp_kw <= 9999)&&(temp_kw >= 6)){//系统功率最小只能限制到6KW
					Globa_1->Charger_param.limit_power = temp_kw;
					System_param_save();//保存系统参数
					if(ISO8583_DEBUG)printf("avrage  power================ %d\n", Globa_1->Charger_param.limit_power);
				}
			}

			sprintf(tmp, "%.4s", &AP_Heart_Resp_Frame->power_h[0]);
			temp_kw = atoi(tmp);
			if((temp_kw <= 9999)&&(temp_kw >= Globa_1->Charger_param.limit_power)){//在线时的功率是大于等于离线时的功率的
				Globa_1->limit_kw = temp_kw;
			}
			
			//if(ISO8583_DEBUG)printf("now  power ================ %d\n", Globa_1->limit_kw);
		//	if(ISO8583_DEBUG)printf("lost power ================ %d\n", Globa_1->Charger_param.limit_power);

			//--------------------------黑名单处理-------------------------
			if(ISO8583_DEBUG)printf("黑名单处理版本 ================ %.5s %.5s\n", &AP_Heart_Resp_Frame->inval_ver[0], &Globa_1->Charger_param2.invalid_card_ver[0]);
			if(!((AP_Heart_Resp_Frame->inval_ver[0] == '0')&&(AP_Heart_Resp_Frame->inval_ver[1] == '0')&&(AP_Heart_Resp_Frame->inval_ver[2] == '0')&&(AP_Heart_Resp_Frame->inval_ver[3] == '0')&&(AP_Heart_Resp_Frame->inval_ver[4] == '0'))){
				if(0 != memcmp(&AP_Heart_Resp_Frame->inval_ver[0], &Globa_1->Charger_param2.invalid_card_ver[0], sizeof(AP_Heart_Resp_Frame->inval_ver))){
					Globa_1->have_card_change = 1;
					memcpy(&Globa_1->tmp_card_ver[0], &AP_Heart_Resp_Frame->inval_ver[0], sizeof(AP_Heart_Resp_Frame->inval_ver));//黑名单数据版本号
					if(ISO8583_DEBUG) printf("黑名单处理 更新 === %d\n", Globa_1->have_card_change);
				}
			}

			//--------------------------价格变化处理------------------------
			if(ISO8583_DEBUG)printf("价格版本 ================ %.5s %.5s\n", &AP_Heart_Resp_Frame->price_ver[0],&Globa_1->Charger_param2.price_type_ver[0]);
			if(!((AP_Heart_Resp_Frame->price_ver[0] == '0')&&(AP_Heart_Resp_Frame->price_ver[1] == '0')&&(AP_Heart_Resp_Frame->price_ver[2] == '0')&&(AP_Heart_Resp_Frame->price_ver[3] == '0')&&(AP_Heart_Resp_Frame->price_ver[4] == '0'))){
				if(0 != memcmp(&AP_Heart_Resp_Frame->price_ver[0], &Globa_1->Charger_param2.price_type_ver[0], sizeof(AP_Heart_Resp_Frame->price_ver))){
					Globa_1->have_price_change = 1;
					memcpy(&Globa_1->tmp_price_ver[0], &AP_Heart_Resp_Frame->price_ver[0], sizeof(AP_Heart_Resp_Frame->price_ver));//黑名单数据版本号
				}
			}

			//--------------------------APP地址变化------------------------
			if(ISO8583_DEBUG)printf("APP_ver ================ %.5s %.5s\n", &AP_Heart_Resp_Frame->APP_ver[0],&Globa_1->Charger_param2.APP_ver[0]);
			if(0 != memcmp(&AP_Heart_Resp_Frame->APP_ver[0], &Globa_1->Charger_param2.APP_ver[0], sizeof(AP_Heart_Resp_Frame->APP_ver))){
				Globa_1->have_APP_change = 1;
				memcpy(&Globa_1->tmp_APP_ver[0], &AP_Heart_Resp_Frame->APP_ver[0], sizeof(AP_Heart_Resp_Frame->APP_ver));//APP版本号
			}
			//--------------------------软件升级包变化-----------------------
			//if((AP_Heart_Resp_Frame->Software_Version[0] != '0')&&(AP_Heart_Resp_Frame->Software_Version[1] != '0')&&(AP_Heart_Resp_Frame->Software_Version[2] != '0')&&(AP_Heart_Resp_Frame->Software_Version[3] != '0')&&(AP_Heart_Resp_Frame->Software_Version[4] != '0')){
			if(0 != memcmp(&AP_Heart_Resp_Frame->Software_Version[0], "00000", sizeof(AP_Heart_Resp_Frame->Software_Version))){
				if(0 != memcmp(&AP_Heart_Resp_Frame->Software_Version[0], &Globa_1->Charger_param2.Software_Version[0], sizeof(AP_Heart_Resp_Frame->Software_Version))){
					Globa_1->Software_Version_change = 1;  //软件升级
					memcpy(&Globa_1->tmp_Software_Version[0], &AP_Heart_Resp_Frame->Software_Version[0], sizeof(AP_Heart_Resp_Frame->Software_Version));//软件包升级版本
					if(ISO8583_DEBUG) printf("软件升级包变化 更新 === %d\n", Globa_1->Software_Version_change);
				}
			}
			//--------------------------对时处理  -------------------------
			if(Globa_1->time_update_flag == 0){
				sprintf(tmp, "%.4s", &AP_Heart_Resp_Frame->time[0]);
				Globa_1->year = atoi(tmp);
				//if(0)printf("year ================ %d\n", Globa_1->year);
				sprintf(tmp, "%.2s", &AP_Heart_Resp_Frame->time[4]);
				Globa_1->month = atoi(tmp);
				sprintf(tmp, "%.2s", &AP_Heart_Resp_Frame->time[6]);
				Globa_1->date = atoi(tmp);
				sprintf(tmp, "%.2s", &AP_Heart_Resp_Frame->time[8]);
				Globa_1->hour = atoi(tmp);
				sprintf(tmp, "%.2s", &AP_Heart_Resp_Frame->time[10]);
				Globa_1->minute = atoi(tmp);
				sprintf(tmp, "%.2s", &AP_Heart_Resp_Frame->time[12]);
				Globa_1->second = atoi(tmp);

				Globa_1->time_update_flag = 1;
			}
			Globa_1->have_hart = 1;//心跳成功
			return (0);
		}
	}else{
		//have_hart_outtime_count ++;
	 if(ISO8583_DEBUG) printf("avrage  power============MAC校验失败====\n");
	}
//}
	return (-1);
}

//签到请求 发送上行数据
int ISO8583_AP_Random_Sent(int fd)
{
	unsigned int ret,len,j;
	unsigned char SendBuf[512];  //发送数据区指针

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_AP_Get_Random_Frame *AP_Frame;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	AP_Frame = (ISO8583_AP_Get_Random_Frame *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据初始化
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'

	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Get_Random_Frame))>>8;  //报文长度
	Head_Frame->SessionLen[1] = sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Get_Random_Frame);
	memcpy(&Head_Frame->MessgTypeIdent[0], "2020", sizeof(Head_Frame->MessgTypeIdent));//消息标识符

	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化位域内容为 0x00
	Head_Frame->PrimaryBitmap[33/8 -0] |= 0x80 >> ((33-1)%8);//主位元表 33 //充电桩类型
	Head_Frame->PrimaryBitmap[38/8 -0] |= 0x80 >> ((38-1)%8);//主位元表 38
	Head_Frame->PrimaryBitmap[39/8 -0] |= 0x80 >> ((39-1)%8);//主位元表 39
	Head_Frame->PrimaryBitmap[40/8 -1] |= 0x80 >> ((40-1)%8);//主位元表 40
	Head_Frame->PrimaryBitmap[41/8 -0] |= 0x80 >> ((41-1)%8);//主位元表 41

	if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪同时
		AP_Frame->charge_pipletype[0] = '0';
		AP_Frame->charge_pipletype[1] = '5';
	}else if(Globa_1->Charger_param.System_Type == 1 ){//轮充
		AP_Frame->charge_pipletype[0] = '0';
		AP_Frame->charge_pipletype[1] = '4';
	}else{
		AP_Frame->charge_pipletype[0] = '0';//单枪
		AP_Frame->charge_pipletype[1] = '1';
	}
	
	AP_Frame->sec[0] = '0';//加密方式 （01）	38	N	N2
	AP_Frame->sec[1] = '1';
	AP_Frame->protocol[0] = Globa_1->protocol_ver[0];//协议版本"02.1.0"
	AP_Frame->protocol[1] = Globa_1->protocol_ver[1];
	AP_Frame->protocol[2] = Globa_1->protocol_ver[3];
	AP_Frame->protocol[3] = Globa_1->protocol_ver[5];	

	AP_Frame->type[0] = '0';//终端类型（01：充电桩，02：网点）
	AP_Frame->type[1] = '1';



	memcpy(&AP_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(AP_Frame->ter_sn));//充电终端序列号    41	N	n12

	len = ISO8583_Sent_Data(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Get_Random_Frame)), 0);
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Get_Random_Frame))){//如果发送不成功
		return (-1);
	}else{
		COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}

//签到请求处理
int ISO8583_Random_Deal(int length,unsigned char *buf)
{
	int j = 0,len = 0,car_lock_time = 0;
	char tmp[9]= {0};
	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_AP_Get_Random_Resp_Frame *AP_Resp_Frame;
	Head_Frame = (ISO8583_Head_Frame *)(buf);
	AP_Resp_Frame = (ISO8583_AP_Get_Random_Resp_Frame *)(buf+sizeof(ISO8583_Head_Frame));
	if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "2021", sizeof(Head_Frame->MessgTypeIdent))){
		if((buf[length - 2] == '0')&&(buf[length - 1] == '0')){
			memcpy(&ISO8583_App_Struct.random[0], &AP_Resp_Frame->random[0], sizeof(AP_Resp_Frame->random));

			if(BITS(Head_Frame->PrimaryBitmap[0], 2)){//APP地址域有效
				len = (AP_Resp_Frame->data_len[0]- 0x30)*10 + (AP_Resp_Frame->data_len[1]- 0x30);
				if(len <= 50){
					if((0 != memcmp(&Globa_1->Charger_param2.http[0], &AP_Resp_Frame->http[0], len))|| \
						 (0 != memcmp(&Globa_1->Charger_param2.APP_ver[0], &Globa_1->tmp_APP_ver[0], sizeof(Globa_1->Charger_param2.APP_ver)))){
						Globa_1->have_APP_change1 = 1;//给qt用，以便告知
						snprintf(&Globa_1->Charger_param2.http[0], len+1, "%s", &AP_Resp_Frame->http[0]);//APP地址  注意snprintf 中N的含义，其包括\0在内 最大N个字符空间，实际有效字符N-1个
						memcpy(&Globa_1->Charger_param2.APP_ver[0], &Globa_1->tmp_APP_ver[0], sizeof(Globa_1->Charger_param2.APP_ver));//APP版本号
						System_param2_save();//保存APP地址
					}
				}
			}
			
			sprintf(&tmp[0], "%.8s", &buf[length - 10]);
			car_lock_time = atoi(&tmp[0]);

  		if(dev_cfg.dev_para.car_lock_time != car_lock_time)
			{
				if((car_lock_time > 2) && (car_lock_time < 11))//3---10分钟有效
				{
					dev_cfg.dev_para.car_lock_time = car_lock_time;
					if(DEBUGLOG) CARLOCK_RUN_LogOut("无车检测时间:%d",dev_cfg.dev_para.car_lock_time);  
					System_CarLOCK_Param_save();
				}
			}
			ISO8583_App_Struct.ISO8583_Random_Rcev_falg = 1; //表示接收到签到反馈信息
			return (0);
		}else{//签到失败
			ISO8583_App_Struct.ISO8583_Random_Rcev_falg = 2;
			return (0);
		}
	}
	return (-1);
}

//工作密钥请求 发送上行数据
int ISO8583_AP_Key_Sent(int fd)
{
	unsigned int ret,len;
	unsigned char SendBuf[512];  //发送数据区指针
	unsigned char MD5[8];  //发送数据区指针

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_AP_Get_Key_Frame *AP_Frame;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	AP_Frame = (ISO8583_AP_Get_Key_Frame *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据初始化
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'

	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Get_Key_Frame))>>8;  //报文长度
	Head_Frame->SessionLen[1] = sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Get_Key_Frame);
	memcpy(&Head_Frame->MessgTypeIdent[0], "2030", sizeof(Head_Frame->MessgTypeIdent));//消息标识符

	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化位域内容为 0x00
	Head_Frame->PrimaryBitmap[4/8 -0] |= 0x80 >> ((4-1)%8);//主位元表 4
	Head_Frame->PrimaryBitmap[6/8 -0] |= 0x80 >> ((6-1)%8);//主位元表 6
	Head_Frame->PrimaryBitmap[7/8 -0] |= 0x80 >> ((7-1)%8);//主位元表 7

	Head_Frame->PrimaryBitmap[40/8 -1] |= 0x80 >> ((40-1)%8);//主位元表 40
	Head_Frame->PrimaryBitmap[41/8 -0] |= 0x80 >> ((41-1)%8);//主位元表 41
	Head_Frame->PrimaryBitmap[42/8 -0] |= 0x80 >> ((42-1)%8);//主位元表 42
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63

	memset(&AP_Frame->random[0], '0', sizeof(ISO8583_AP_Get_Key_Frame));//初始化帧数据内容为'0'
 
	memcpy(&AP_Frame->random[0], &ISO8583_App_Struct.random[0], sizeof(AP_Frame->random));//随机字符串	4	N	N32

	AP_Frame->fenshi[0] = '0';//电费类型（01：通用，02：分时）	6	N	N2
	AP_Frame->fenshi[1] = '2';
	
	AP_Frame->ground_lock_num[0] = '0';
  AP_Frame->ground_lock_num[1] = '0';
	if(dev_cfg.dev_para.car_lock_num == 1)//有地锁
		AP_Frame->ground_lock_num[1] = '1';
	if(dev_cfg.dev_para.car_lock_num == 2)
		AP_Frame->ground_lock_num[1] = '2';
	
	AP_Frame->type[0] = '0';//终端类型（01：充电桩，02：网点）40	N		N2
	AP_Frame->type[1] = '1';
	memcpy(&AP_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(AP_Frame->ter_sn));//充电终端序列号    41	N	n12
	//unsigned char ESAM[12];    //ESAM卡号          42	n	n12

	MD5_DES(&AP_Frame->random[0], sizeof(ISO8583_AP_Get_Key_Frame)-sizeof(AP_Frame->MD5), &manual_key[0], &MD5[0]);
	hex_to_str(&AP_Frame->MD5[0], &MD5[0], 8);

	len = ISO8583_Sent_Data(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Get_Key_Frame)), 0);
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Get_Key_Frame))){//如果发送不成功
		return (-1);
	}else{
		COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}

//工作密钥请求处理 --接收数据
int ISO8583_Key_Deal(int length,unsigned char *buf)
{
	int j;

	unsigned char tmp[100];

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_AP_Get_Key_Resp_Frame *AP_Resp_Frame;

	Head_Frame = (ISO8583_Head_Frame *)(buf);
	AP_Resp_Frame = (ISO8583_AP_Get_Key_Resp_Frame *)(buf+sizeof(ISO8583_Head_Frame));
	if(length == (sizeof(ISO8583_Head_Frame)+ sizeof(ISO8583_AP_Get_Key_Resp_Frame))){//表明客户端有数据发送过来，作相应接受处理
		if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "2031", sizeof(Head_Frame->MessgTypeIdent))){
			if((AP_Resp_Frame->flag[0] == '0')&&(AP_Resp_Frame->flag[1] == '0')){
				str_to_hex(&tmp[0], &AP_Resp_Frame->KEY[0], sizeof(AP_Resp_Frame->KEY));
				DES_to_min(&tmp[0], 8, &manual_key[0], &ISO8583_App_Struct.work_key[0]);
				if(Globa_1->time_update_flag == 0){
					sprintf(tmp, "%.4s", &AP_Resp_Frame->time[0]);
					Globa_1->year = atoi(tmp);
					sprintf(tmp, "%.2s", &AP_Resp_Frame->time[4]);
					Globa_1->month = atoi(tmp);
					sprintf(tmp, "%.2s", &AP_Resp_Frame->time[6]);
					Globa_1->date = atoi(tmp);
					sprintf(tmp, "%.2s", &AP_Resp_Frame->time[8]);
					Globa_1->hour = atoi(tmp);
					sprintf(tmp, "%.2s", &AP_Resp_Frame->time[10]);
					Globa_1->minute = atoi(tmp);
					sprintf(tmp, "%.2s", &AP_Resp_Frame->time[12]);
					Globa_1->second = atoi(tmp);

					Globa_1->time_update_flag = 1;
				}			
				ISO8583_App_Struct.ISO8583_Key_Rcev_falg = 1; //表示接收到密钥到反馈信息
				return (0);
			}else {
				ISO8583_App_Struct.ISO8583_Key_Rcev_falg = 2; //表示接收到密钥到反馈信息-失败则重新进行签到
				if(ISO8583_DEBUG)printf(" MAC XXXXXXXXXXXXXXXXXXXXXXXXXXX \n");
				return (0);
			}
		}
	}
	return (-1);
}

//事件告警量上送 发送上行数据
int ISO8583_alarm_Sent(int fd, Alarm_Info_Get *alarm_data)
{
	unsigned int ret,len,data_len,offset =0,i,j;
	unsigned char SendBuf[512];  //发送数据区指针
	unsigned char MD5[8];  //发送数据区指针

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_alarm_Frame *alarm_Frame;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	alarm_Frame = (ISO8583_alarm_Frame *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据初始化
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'

	memcpy(&Head_Frame->MessgTypeIdent[0], "4040", sizeof(Head_Frame->MessgTypeIdent));//消息标识符

	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化位域内容为 0x00
	//主位元33,34,35,36,37,38,41,63置位
	Head_Frame->PrimaryBitmap[33/8 -0] |= 0x80 >> ((33-1)%8);//主位元表 33
	Head_Frame->PrimaryBitmap[34/8 -0] |= 0x80 >> ((34-1)%8);//主位元表 34
	Head_Frame->PrimaryBitmap[35/8 -0] |= 0x80 >> ((35-1)%8);//主位元表 35
	Head_Frame->PrimaryBitmap[36/8 -0] |= 0x80 >> ((36-1)%8);//主位元表 36
  Head_Frame->PrimaryBitmap[37/8 -0] |= 0x80 >> ((37-1)%8);//主位元表 37
	Head_Frame->PrimaryBitmap[38/8 -0] |= 0x80 >> ((38-1)%8);//主位元表 38
	Head_Frame->PrimaryBitmap[41/8 -0] |= 0x80 >> ((41-1)%8);//主位元表 41
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63
	
#if ISO8583_DEBUG == 1
  printf("Head_Frame->PrimaryBitmap data= ");
     for(j=0; j<8; j++)printf("%02x ",Head_Frame->PrimaryBitmap[j]);
  			printf("\n");
#endif
	
	//域33数据
	//alarm_Frame->charger_type[0] = '0';
	//alarm_Frame->charger_type[1] = '5';   //05：双枪同时充直流充电桩
	if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪同时
		alarm_Frame->charger_type[0] = '0';
		alarm_Frame->charger_type[1] = '5';
	}else if(Globa_1->Charger_param.System_Type == 1 ){//轮充
		alarm_Frame->charger_type[0] = '0';
		alarm_Frame->charger_type[1] = '4';
	}else{
		alarm_Frame->charger_type[0] = '0';//单枪
		alarm_Frame->charger_type[1] = '1';
	}
	//域34数据
	sprintf(&alarm_Frame->charger_alarm_num[0],"%03d",alarm_data->charger_alarm_num);

	//域35数据	
	data_len = alarm_data->charger_alarm_num * 5;
	sprintf(alarm_Frame->charger_data,"%03d",data_len);
  offset += 3;
	
	memcpy(&alarm_Frame->charger_data[offset],alarm_data->alarm_id_h, data_len);	
	offset += data_len;
	
	//域36数据
	data_len = alarm_data->charger_alarm_num * 3;	
	sprintf(&alarm_Frame->charger_data[offset],"%03d",data_len);
	offset += 3;
	
	memcpy(&alarm_Frame->charger_data[offset],alarm_data->alarm_id_l, data_len);	
	offset += data_len;
	
	//域37数据
	data_len = alarm_data->charger_alarm_num * 2;
	sprintf(&alarm_Frame->charger_data[offset],"%03d",data_len);
	offset += 3;
	
	memcpy(&alarm_Frame->charger_data[offset],alarm_data->alarm_type, data_len);
	offset += data_len;	
	
	//域38数据
	sprintf(&alarm_Frame->charger_data[offset],"%03d",data_len);
  offset += 3;
	
	memcpy(&alarm_Frame->charger_data[offset],alarm_data->alarm_sts,data_len);
	offset += data_len;	

	//域41数据
  sprintf(&alarm_Frame->charger_data[offset],"%02d",sizeof(Globa_1->Charger_param.SN));
	offset += 2;
	
	memcpy(&alarm_Frame->charger_data[offset], &Globa_1->Charger_param.SN[0], 12);//充电终端序列号    41	N	n12
	offset += 12;
	
	//域63数据
	MD5_DES(&alarm_Frame->charger_type[0], sizeof(ISO8583_alarm_Frame)-16, &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&alarm_Frame->charger_data[offset], &MD5[0], 8);
	offset += 16;
	
	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+offset+5)>>8;  //报文长度
	Head_Frame->SessionLen[1] = sizeof(ISO8583_Head_Frame)+offset+5;

	len = ISO8583_Sent_Data(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+offset+5), 0);
	//printf("len = %d \n", len);
#if ISO8583_DEBUG == 1
  printf("ISO8583_alarm_Sent length = %d,data= ", len);
     for(j=0; j<len; j++)printf("%02x ",SendBuf[j]);
  			printf("\n");
#endif
	//printf(" sizeof(ISO8583_Head_Frame)+offset+5 = %d \n",  sizeof(ISO8583_Head_Frame)+offset+5);
	if(len != (sizeof(ISO8583_Head_Frame)+offset+5)){//如果发送不成功
		return (-1);
	}else{
	  COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}

//事件告警实时信息
void ISO8583_alarm_get(Alarm_Info_Get *alarm_data) 
{
	#define  ALARM_INDEX  (alarm_data->charger_alarm_num)
	unsigned char i;
	ALARM_INDEX = 0;
	if(Globa_1->Error.pre_ctrl_connect_lost != Globa_1->Error.ctrl_connect_lost){
		   Globa_1->Error.pre_ctrl_connect_lost= Globa_1->Error.ctrl_connect_lost;
			 memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01004",5);
			 memcpy(alarm_data->alarm_id_l[ALARM_INDEX],"001",3);
		   alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"控制板1通信中断
		   alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			 printf("Globa_1->Error.ctrl_connect_lost = %d \n", Globa_1->Error.ctrl_connect_lost);
			if((Globa_1->Error.ctrl_connect_lost != 0)){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
    }
	 if(Globa_2->Error.pre_ctrl_connect_lost!= Globa_2->Error.ctrl_connect_lost){
		   Globa_2->Error.pre_ctrl_connect_lost = Globa_2->Error.ctrl_connect_lost;
			 memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01004",5);
			 memcpy(alarm_data->alarm_id_l[ALARM_INDEX],"002",3);
		   alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"控制板2通信中断
		   alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_2->Error.ctrl_connect_lost != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
    }
	if(Globa_1->Error.pre_meter_connect_lost != Globa_1->Error.meter_connect_lost){
		   Globa_1->Error.pre_meter_connect_lost = Globa_1->Error.meter_connect_lost;
			 memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01005",5);
			 memcpy(alarm_data->alarm_id_l[ALARM_INDEX],"001",3);
		   alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"电表1通信中断
		   alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_1->Error.meter_connect_lost != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
    }
	if(Globa_2->Error.pre_meter_connect_lost != Globa_2->Error.meter_connect_lost){
		   Globa_2->Error.pre_meter_connect_lost = Globa_2->Error.meter_connect_lost;
			 memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01005",5);
			 memcpy(alarm_data->alarm_id_l[ALARM_INDEX],"002",3);
		   alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"电表2通信中断
		   alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_2->Error.meter_connect_lost != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
    }
	if(Globa_1->Error.pre_AC_connect_lost != Globa_1->Error.AC_connect_lost){ //交流采集模块
		   Globa_1->Error.pre_AC_connect_lost = Globa_1->Error.AC_connect_lost;
			 memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01006",5);
			 memcpy(alarm_data->alarm_id_l[ALARM_INDEX],"001",3);
		   alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"交流采集模块通信中断
		   alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_1->Error.AC_connect_lost != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
    }
		Globa_1->Error.charger_connect_lost = 0;
	  for(i=0;i<=Globa_1->Charger_param.Charge_rate_number1;i++){
        Globa_1->Error.charger_connect_lost |= Globa_1->Error.charg_connect_lost[i];
		}
		if(Globa_1->Error.pre_charger_connect_lost != Globa_1->Error.charger_connect_lost){ 
		    Globa_1->Error.pre_charger_connect_lost = Globa_1->Error.charger_connect_lost;
				memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01007",5);
				sprintf(alarm_data->alarm_id_l[ALARM_INDEX],"001",3);
		    alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"1组模块通信中断
		    alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_1->Error.charger_connect_lost != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
   }

	 Globa_2->Error.charger_connect_lost = 0;
	 for(i=0;i<=Globa_2->Charger_param.Charge_rate_number2;i++){
        Globa_2->Error.charger_connect_lost |= Globa_2->Error.charg_connect_lost[i];
		}
	  if(Globa_2->Error.pre_charger_connect_lost != Globa_2->Error.charger_connect_lost){ 
		    Globa_2->Error.pre_charger_connect_lost = Globa_2->Error.charger_connect_lost;
				memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01007",5);
				sprintf(alarm_data->alarm_id_l[ALARM_INDEX],"002",3);
		    alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"2组模块通信中断
		    alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_2->Error.charger_connect_lost != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
   }
	 
	 Globa_1->Error.charger_over_tmp = 0;
	 for(i=0;i<=Globa_1->Charger_param.Charge_rate_number1;i++){
        Globa_1->Error.charger_over_tmp |= Globa_1->Error.charg_over_tmp[i];
		}
	  if(Globa_1->Error.pre_charger_over_tmp != Globa_1->Error.charger_over_tmp){ 
		    Globa_1->Error.pre_charger_over_tmp = Globa_1->Error.charger_over_tmp;
				memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01008",5);
				sprintf(alarm_data->alarm_id_l[ALARM_INDEX],"001",3);
		    alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"1组模块过温告警
		    alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_1->Error.charger_over_tmp != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
   }
	 
	 Globa_2->Error.charger_over_tmp = 0;
	 for(i=0;i<=Globa_2->Charger_param.Charge_rate_number2;i++){
        Globa_2->Error.charger_over_tmp |= Globa_2->Error.charg_over_tmp[i];
		}
	  if(Globa_2->Error.pre_charger_over_tmp != Globa_2->Error.charger_over_tmp){ 
		    Globa_2->Error.pre_charger_over_tmp = Globa_2->Error.charger_over_tmp;
				memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01008",5);
				sprintf(alarm_data->alarm_id_l[ALARM_INDEX],"002",3);
		    alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"2组模块过温告警
		    alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_2->Error.charger_over_tmp != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
   }
	 
	 Globa_1->Error.charger_OV_down = 0;
	 for(i=0;i<=Globa_1->Charger_param.Charge_rate_number1;i++){
        Globa_1->Error.charger_OV_down |= Globa_1->Error.charg_OV_down[i];
		}
	  if(Globa_1->Error.pre_charger_OV_down != Globa_1->Error.charger_OV_down){ 
		    Globa_1->Error.pre_charger_OV_down = Globa_1->Error.charger_OV_down;
				memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01009",5);
				sprintf(alarm_data->alarm_id_l[ALARM_INDEX],"001",3);
		    alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"1组模块过压关机告警
		    alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_1->Error.charger_OV_down != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
   }
	 
	 Globa_2->Error.charger_OV_down = 0; 
	 for(i=0;i<=Globa_2->Charger_param.Charge_rate_number2;i++){
        Globa_2->Error.charger_OV_down |= Globa_2->Error.charg_OV_down[i];
		}
	  if(Globa_2->Error.pre_charger_OV_down != Globa_2->Error.charger_OV_down){ 
		    Globa_2->Error.pre_charger_OV_down = Globa_2->Error.charger_OV_down;
				memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01009",5);
				sprintf(alarm_data->alarm_id_l[ALARM_INDEX],"002",3);
		    alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"2组模块过压关机告警
		    alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_2->Error.charger_OV_down != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
   }
	
	Globa_1->Error.charger_fun_fault = 0;
	for(i=0;i<=Globa_1->Charger_param.Charge_rate_number1;i++){
        Globa_1->Error.charger_fun_fault |= Globa_1->Error.charg_fun_fault[i];
		}
	  if(Globa_1->Error.pre_charger_fun_fault != Globa_1->Error.charger_fun_fault){ 
		    Globa_1->Error.pre_charger_fun_fault = Globa_1->Error.charger_fun_fault;
				memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01010",5);
				sprintf(alarm_data->alarm_id_l[ALARM_INDEX],"001",3);
		    alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"1组模块风扇告警
		    alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_1->Error.charger_fun_fault != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
   }
	
	Globa_2->Error.charger_fun_fault = 0;
	for(i=0;i<=Globa_2->Charger_param.Charge_rate_number2;i++){
        Globa_2->Error.charger_fun_fault |= Globa_2->Error.charg_fun_fault[i];
		}
	  if(Globa_2->Error.pre_charger_fun_fault != Globa_2->Error.charger_fun_fault){ 
		    Globa_2->Error.pre_charger_fun_fault = Globa_2->Error.charger_fun_fault;
				memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01010",5);
				sprintf(alarm_data->alarm_id_l[ALARM_INDEX],"002",3);
		    alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"2组模块风扇告警
		    alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_2->Error.charger_fun_fault != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
   }
	
	Globa_1->Error.charger_other_alarm = 0;
	for(i=0;i<=Globa_1->Charger_param.Charge_rate_number1;i++){
        Globa_1->Error.charger_other_alarm |= Globa_1->Error.charg_other_alarm[i];
		}
	  if(Globa_1->Error.pre_charger_other_alarm != Globa_1->Error.charger_other_alarm){ 
		    Globa_1->Error.pre_charger_other_alarm = Globa_1->Error.charger_other_alarm;
				memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01011",5);
				sprintf(alarm_data->alarm_id_l[ALARM_INDEX],"001",3);
		    alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"1组模块其他告警
		    alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_1->Error.charger_other_alarm != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
   }
	
	Globa_2->Error.charger_other_alarm = 0;
	for(i=0;i<=Globa_2->Charger_param.Charge_rate_number2;i++){
        Globa_2->Error.charger_other_alarm |= Globa_2->Error.charg_other_alarm[i];
		}
	  if(Globa_2->Error.pre_charger_other_alarm != Globa_2->Error.charger_other_alarm){ 
		    Globa_2->Error.pre_charger_other_alarm = Globa_2->Error.charger_other_alarm;
				memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01011",5);
				sprintf(alarm_data->alarm_id_l[ALARM_INDEX],"002",3);
		    alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"2组模块其他告警
		    alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_2->Error.charger_other_alarm != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
   }
	 if(Globa_1->Error.pre_emergency_switch != Globa_1->Error.emergency_switch){ 
		   Globa_1->Error.pre_emergency_switch = Globa_1->Error.emergency_switch;
			 memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01012",5);
			 memcpy(alarm_data->alarm_id_l[ALARM_INDEX],"001",3);
		   alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"急停故障
		   alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_1->Error.emergency_switch != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
    }

	if(Globa_1->Error.pre_sistent_alarm != Globa_1->Error.sistent_alarm){ 
		   Globa_1->Error.pre_sistent_alarm = Globa_1->Error.sistent_alarm;
			 memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01013",5);
			 memcpy(alarm_data->alarm_id_l[ALARM_INDEX],"001",3);
		   alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"充电机1组充电机绝缘告警
		   alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_1->Error.sistent_alarm != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
    }
	if(Globa_2->Error.pre_sistent_alarm != Globa_2->Error.sistent_alarm){ 
		   Globa_2->Error.pre_sistent_alarm = Globa_2->Error.sistent_alarm;
			 memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01013",5);
			 memcpy(alarm_data->alarm_id_l[ALARM_INDEX],"002",3);
		   alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"充电机2组充电机绝缘告警
		   alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_2->Error.sistent_alarm != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
    }				
	
	
	if(Globa_1->Error.pre_output_over_limit!= Globa_1->Error.output_over_limit){ 
		   Globa_1->Error.pre_output_over_limit = Globa_1->Error.output_over_limit;
			 memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01014",5);
			 memcpy(alarm_data->alarm_id_l[ALARM_INDEX],"001",3);
		   alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"充电机1组输出过压告警
		   alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_1->Error.output_over_limit != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
    }
	if(Globa_2->Error.pre_output_over_limit != Globa_2->Error.output_over_limit){ 
		   Globa_2->Error.pre_output_over_limit = Globa_2->Error.output_over_limit;
			 memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01014",5);
			 memcpy(alarm_data->alarm_id_l[ALARM_INDEX],"002",3);
		   alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"充电机2组输出过压告警
		   alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_2->Error.output_over_limit != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
    }
	if(Globa_1->Error.pre_output_low_limit!= Globa_1->Error.output_low_limit){ 
		   Globa_1->Error.pre_output_low_limit = Globa_1->Error.output_low_limit;
			 memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01015",5);
			 memcpy(alarm_data->alarm_id_l[ALARM_INDEX],"001",3);
		   alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"充电机1组输出欠压告警
		   alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_1->Error.output_low_limit != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
    }
	if(Globa_2->Error.pre_output_low_limit!= Globa_2->Error.output_low_limit){ 
		   Globa_2->Error.pre_output_low_limit = Globa_2->Error.output_low_limit;
			 memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01015",5);
			 memcpy(alarm_data->alarm_id_l[ALARM_INDEX],"002",3);
		   alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"充电机2组输出欠压告警
		   alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_2->Error.output_low_limit != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
    }	
	if(Globa_1->Error.pre_input_over_limit != Globa_1->Error.input_over_limit){
       Globa_1->Error.pre_input_over_limit = Globa_1->Error.input_over_limit;	
		   memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01016",5);
			 memcpy(alarm_data->alarm_id_l[ALARM_INDEX],"001",3);
		   alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"交流输入过压告警
		   alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_1->Error.input_over_limit != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
    }
	if(Globa_1->Error.pre_input_over_protect != Globa_1->Error.input_over_protect){ 
		   Globa_1->Error.pre_input_over_protect = Globa_1->Error.input_over_protect;
			 memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01017",5);
			 memcpy(alarm_data->alarm_id_l[ALARM_INDEX],"001",3);
		   alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"交流输入过压保护
		   alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_1->Error.input_over_protect != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
    }		
	if(Globa_1->Error.pre_input_lown_limit != Globa_1->Error.input_lown_limit){ 
		   Globa_1->Error.pre_input_lown_limit = Globa_1->Error.input_lown_limit;
			 memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01018",5);
			 memcpy(alarm_data->alarm_id_l[ALARM_INDEX],"001",3);
		   alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"交流输入欠压告警
		   alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_1->Error.input_lown_limit != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
    }	
  if(Globa_1->Error.pre_input_switch_off != Globa_1->Error.input_switch_off){ 
		   Globa_1->Error.pre_input_switch_off = Globa_1->Error.input_switch_off;
			 memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01019",5);
			 memcpy(alarm_data->alarm_id_l[ALARM_INDEX],"001",3);
		   alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"交流输入开关跳闸
		   alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_1->Error.input_switch_off != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
    }	
  if(Globa_1->Error.pre_fanglei_off != Globa_1->Error.fanglei_off){ 
		   Globa_1->Error.pre_fanglei_off = Globa_1->Error.fanglei_off;
			 memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01020",5);
			 memcpy(alarm_data->alarm_id_l[ALARM_INDEX],"001",3);
		   alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"交流防雷器跳闸
		   alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_1->Error.fanglei_off != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
    }
		if(Globa_1->Error.pre_output_switch_off[0] != Globa_1->Error.output_switch_off[0]){ 
		   Globa_1->Error.pre_output_switch_off[0] = Globa_1->Error.output_switch_off[0];
			 memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01021",5);
			 memcpy(alarm_data->alarm_id_l[ALARM_INDEX],"001",3);
		   alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"充电机1组直流输出开关跳闸
		   alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_1->Error.output_switch_off[0] != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
    }
	 if(Globa_2->Error.pre_output_switch_off[1] != Globa_2->Error.output_switch_off[1]){ 		   
			 Globa_2->Error.pre_output_switch_off[1] = Globa_2->Error.output_switch_off[1];
			 memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01021",5);
			 memcpy(alarm_data->alarm_id_l[ALARM_INDEX],"002",3);
		   alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"充电机2组直流输出开关跳闸  
		   alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_2->Error.output_switch_off[1] != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
    }
  if(Globa_1->Error.pre_sys_over_tmp != Globa_1->Error.sys_over_tmp){ 
		   Globa_1->Error.pre_sys_over_tmp = Globa_1->Error.sys_over_tmp;
			 memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01022",5);
			 memcpy(alarm_data->alarm_id_l[ALARM_INDEX],"001",3);
		   alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"充电机1组内部过温告警
		   alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_1->Error.sys_over_tmp != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
    }	
  if(Globa_2->Error.pre_sys_over_tmp != Globa_2->Error.sys_over_tmp){ 
		   Globa_2->Error.pre_sys_over_tmp = Globa_2->Error.sys_over_tmp;
			 memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01022",5);
			 memcpy(alarm_data->alarm_id_l[ALARM_INDEX],"002",3);
		   alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"充电机2组内部过温告警
		   alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_2->Error.sys_over_tmp != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
    }
	Globa_1->Error.charger_fault = 0;
	for(i=0;i<=Globa_1->Charger_param.Charge_rate_number1;i++){
		 Globa_1->Error.charger_fault |= Globa_1->Error.charg_fault[i];
	}
	  if(Globa_1->Error.pre_charger_fault != Globa_1->Error.charger_fault){ 
		    Globa_1->Error.pre_charger_fault = Globa_1->Error.charger_fault;
				memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01023",5);
				sprintf(alarm_data->alarm_id_l[ALARM_INDEX],"001",3);
		    alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"1组模块故障
		    alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_1->Error.charger_fault != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
   }
	
	Globa_2->Error.charger_fault = 0;
	for(i=0;i<=Globa_2->Charger_param.Charge_rate_number2;i++){
        Globa_2->Error.charger_fault |= Globa_2->Error.charg_fault[i];
		}
	  if(Globa_2->Error.pre_charger_fault != Globa_2->Error.charger_fault){ 
		    Globa_2->Error.pre_charger_fault = Globa_2->Error.charger_fault;
				memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01023",5);
				sprintf(alarm_data->alarm_id_l[ALARM_INDEX],"002",3);
		    alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"2组模块故障
		    alarm_data->alarm_type[ALARM_INDEX][1] = '1';
			if(Globa_2->Error.charger_fault != 0){
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
					alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
			}else{//消失
					alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
					alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
			}
			ALARM_INDEX++;
   }
}

//事件告警实时信息 刚上电全部状态为1，上电的时候发1次故障全部消失的帧
void ISO8583_alarm_set() 
{
		Globa_1->pre_Error_system = ~Globa_1->Error_system; //充电机故障
    Globa_2->pre_Error_system = ~Globa_2->Error_system; //充电机故障
    Globa_1->Error.pre_ctrl_connect_lost = ~Globa_1->Error.ctrl_connect_lost;//控制板1通信中断 
    Globa_2->Error.pre_ctrl_connect_lost = ~Globa_2->Error.ctrl_connect_lost;//控制板2通信中断
    Globa_1->Error.pre_meter_connect_lost = ~Globa_1->Error.meter_connect_lost;//电表1通信中断
    Globa_2->Error.pre_meter_connect_lost = ~Globa_2->Error.meter_connect_lost;//电表2通信中断
    Globa_1->Error.pre_AC_connect_lost = ~Globa_1->Error.AC_connect_lost;//交流采集模块通信中断
		Globa_1->Error.pre_charger_connect_lost = ~Globa_1->Error.charger_connect_lost;  //1组充电模块通信中断
    Globa_2->Error.pre_charger_connect_lost = ~Globa_2->Error.charger_connect_lost;  //2组充电模块通信中断		
	  Globa_1->Error.pre_charger_fun_fault = ~Globa_1->Error.charger_fun_fault;     //1组充电模块风扇故障
    Globa_2->Error.pre_charger_fun_fault = ~Globa_2->Error.charger_fun_fault;     //2组充电模块风扇故障 	 
	  Globa_1->Error.pre_charger_over_tmp = ~Globa_1->Error.charger_over_tmp;      //1组充电模块过温告警
    Globa_2->Error.pre_charger_over_tmp = ~Globa_2->Error.charger_over_tmp;      //2组充电模块过温告警	 
	  Globa_1->Error.pre_charger_OV_down = ~Globa_1->Error.charger_OV_down;       //1组充电模块过压关机告警 
    Globa_2->Error.pre_charger_OV_down = ~Globa_2->Error.charger_OV_down;       //2组充电模块过压关机告警  	 
	  Globa_1->Error.pre_charger_other_alarm = ~Globa_1->Error.charger_other_alarm;   //1组充电模块其他告警 
	  Globa_2->Error.pre_charger_other_alarm = ~Globa_2->Error.charger_other_alarm;   //2组充电模块其他告警
		Globa_1->Error.pre_emergency_switch = ~Globa_1->Error.emergency_switch;    //急停故障
		Globa_1->Error.pre_sistent_alarm = ~Globa_1->Error.sistent_alarm;       //充电机1组充电机绝缘告警
		Globa_2->Error.pre_sistent_alarm = ~Globa_2->Error.sistent_alarm;       //充电机2组充电机绝缘告警
		Globa_1->Error.pre_output_over_limit = ~Globa_1->Error.output_over_limit;        //充电机1组输出过压告警
		Globa_2->Error.pre_output_over_limit = ~Globa_2->Error.output_over_limit;        //充电机2组输出过压告警
		Globa_1->Error.pre_output_low_limit = ~Globa_1->Error.output_low_limit;          //充电机1组输出欠压告警
		Globa_2->Error.pre_output_low_limit = ~Globa_2->Error.output_low_limit;          //充电机2组输出欠压告警
		Globa_1->Error.pre_input_over_limit = ~Globa_1->Error.input_over_limit;         //交流输入过压告警
		Globa_1->Error.pre_input_over_protect = ~Globa_1->Error.input_over_protect;       //交流输入过压保护
		Globa_1->Error.pre_input_lown_limit = ~Globa_1->Error.input_lown_limit;         //交流输入欠压告警
		Globa_1->Error.pre_input_switch_off = ~Globa_1->Error.input_switch_off;         //交流输入开关跳闸
		Globa_1->Error.pre_fanglei_off = ~Globa_1->Error.fanglei_off;              //交流防雷器跳闸
		Globa_1->Error.pre_output_switch_off[0] = ~Globa_1->Error.output_switch_off[0];     //充电机1组直流输出开关跳闸
		Globa_1->Error.pre_output_switch_off[1] = ~Globa_1->Error.output_switch_off[1];     //充电机2组直流输出开关跳闸
		Globa_1->Error.pre_sys_over_tmp = ~Globa_1->Error.sys_over_tmp;             //充电机1组内部过温告警
		Globa_2->Error.pre_sys_over_tmp = ~Globa_2->Error.sys_over_tmp;             //充电机2组内部过温告警
		Globa_1->Error.pre_charger_fault = ~Globa_1->Error.charger_fault;     //1组充电模块故障
    Globa_2->Error.pre_charger_fault = ~Globa_2->Error.charger_fault;     //2组充电模块故障
}

void ISO8583_alarm_clear() 
{
		Globa_1->pre_Error_system = 0; //充电机故障
    Globa_2->pre_Error_system = 0; //充电机故障
    Globa_1->Error.pre_ctrl_connect_lost = 0;//控制板1通信中断 
    Globa_2->Error.pre_ctrl_connect_lost = 0;//控制板2通信中断
    Globa_1->Error.pre_meter_connect_lost = 0;//电表1通信中断
    Globa_2->Error.pre_meter_connect_lost = 0;//电表2通信中断
    Globa_1->Error.pre_AC_connect_lost = 0;//交流采集模块通信中断
		Globa_1->Error.pre_charger_connect_lost = 0;  //1组充电模块通信中断
    Globa_2->Error.pre_charger_connect_lost = 0;  //2组充电模块通信中断		
	  Globa_1->Error.pre_charger_fun_fault = 0;     //1组充电模块风扇故障
    Globa_2->Error.pre_charger_fun_fault = 0;     //2组充电模块风扇故障 	 
	  Globa_1->Error.pre_charger_over_tmp = 0;      //1组充电模块过温告警
    Globa_2->Error.pre_charger_over_tmp = 0;      //2组充电模块过温告警	 
	  Globa_1->Error.pre_charger_OV_down = 0;       //1组充电模块过压关机告警 
    Globa_2->Error.pre_charger_OV_down = 0;       //2组充电模块过压关机告警  	 
	  Globa_1->Error.pre_charger_other_alarm = 0;   //1组充电模块其他告警 
	  Globa_2->Error.pre_charger_other_alarm = 0;   //2组充电模块其他告警
		Globa_1->Error.pre_emergency_switch = 0;    //急停故障
		Globa_1->Error.pre_sistent_alarm = 0;       //充电机1组充电机绝缘告警
		Globa_2->Error.pre_sistent_alarm = 0;       //充电机2组充电机绝缘告警
		Globa_1->Error.pre_output_over_limit = 0;        //充电机1组输出过压告警
		Globa_2->Error.pre_output_over_limit = 0;        //充电机2组输出过压告警
		Globa_1->Error.pre_output_low_limit = 0;          //充电机1组输出欠压告警
		Globa_2->Error.pre_output_low_limit = 0;          //充电机2组输出欠压告警
		Globa_1->Error.pre_input_over_limit = 0;         //交流输入过压告警
		Globa_1->Error.pre_input_over_protect = 0;       //交流输入过压保护
		Globa_1->Error.pre_input_lown_limit = 0;         //交流输入欠压告警
		Globa_1->Error.pre_input_switch_off = 0;         //交流输入开关跳闸
		Globa_1->Error.pre_fanglei_off = 0;              //交流防雷器跳闸
		Globa_1->Error.pre_output_switch_off[0] = 0;     //充电机1组直流输出开关跳闸
		Globa_1->Error.pre_output_switch_off[1] = 0;     //充电机2组直流输出开关跳闸
		Globa_1->Error.pre_sys_over_tmp = 0;             //充电机1组内部过温告警
		Globa_2->Error.pre_sys_over_tmp = 0;             //充电机2组内部过温告警
		Globa_1->Error.pre_charger_fault = 0;             //1组充电模块故障
    Globa_2->Error.pre_charger_fault = 0;             //2组充电模块故障
}
unsigned char charge_id_1[21];
unsigned char charge_id_2[21];
//4.6.5 蓄电池参数上送接口
int ISO8583_Battery_Parameter_Sent(int fd,int GUN_NO)
{
	unsigned int ret,len,data_len,offset =0,i,j;
	unsigned char SendBuf[512];  //发送数据区指针
	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针
	unsigned char tmp1[10];
	time_t systime=0;
	static int count;
	struct tm tm_t;

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_Battery_Parameter_Request_Frame *Battery_Parameter_Frame;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	Battery_Parameter_Frame = (ISO8583_Battery_Parameter_Request_Frame *)(SendBuf+sizeof(ISO8583_Head_Frame));
	
	//数据初始化
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'

	memcpy(&Head_Frame->MessgTypeIdent[0], "4050", sizeof(Head_Frame->MessgTypeIdent));//消息标识符
	
	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_Battery_Parameter_Request_Frame))>>8;  //报文长度
	Head_Frame->SessionLen[1] = sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_Battery_Parameter_Request_Frame);

	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化位域内容为 0x00
	
	
	//主位元31,32,33,34,35,36,37,38,39,40,41,42,63置位
	for(i = 31; i <= 42; i++){
		if(i == 40 ||i == 32){
			Head_Frame->PrimaryBitmap[i/8 -1] |= 0x80 >> ((i-1)%8);//主位元表 40
		}else{
			Head_Frame->PrimaryBitmap[i/8 -0] |= 0x80 >> ((i-1)%8);//主位元表 31-42
		}
	}
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63
	
	#if ISO8583_DEBUG == 1
	  printf("Head_Frame->PrimaryBitmap data= ");
		 for(j=0; j<13; j++)printf("%02x ",Head_Frame->PrimaryBitmap[j]);
		 printf("\n");
	#endif
	count++;
	systime = time(NULL);        //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
	//1.充电标识号
	memcpy(&Battery_Parameter_Frame->charge_id[0], &Globa_1->Charger_param.SN[0], sizeof(Battery_Parameter_Frame->ter_sn));
	//sprintf(&tmp[0], "%04d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday);
	//memcpy(&Battery_Parameter_Frame->charge_id[12], &tmp[2],6);//终端时间
  sprintf(&tmp[0], "%02d%02d%02d",tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
	memcpy(&Battery_Parameter_Frame->charge_id[12], &tmp[0],6);//终端时间

	sprintf(&tmp[0], "%03d", count);
	memcpy(&Battery_Parameter_Frame->charge_id[18],&tmp[0],3);
	
	//2.充电终端序列号
	memcpy(&Battery_Parameter_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(Battery_Parameter_Frame->ter_sn));
	 
	if(GUN_NO == 0){//充电枪1号
		memcpy(&charge_id_1,&Battery_Parameter_Frame->charge_id[0],sizeof(charge_id_1));
	 //3.充电桩类型（01：单枪直流充电桩，02单相交流充电桩，03三相交流充电桩，04：双枪轮流充直流充电桩，05：双枪同时充直流充电桩）
	  if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪同时
			Battery_Parameter_Frame->type[0] = '0';
			Battery_Parameter_Frame->type[1] = '5';
		}else if(Globa_1->Charger_param.System_Type == 1 ){//轮充
			Battery_Parameter_Frame->type[0] = '0';
			Battery_Parameter_Frame->type[1] = '4';
		}else{
			Battery_Parameter_Frame->type[0] = '0';//单枪
			Battery_Parameter_Frame->type[1] = '1';
		}
		//4.蓄电池类型
		sprintf(&tmp[0], "%02d", Globa_1 ->BATT_type);				
		memcpy(&Battery_Parameter_Frame->battery_type[0], &tmp[0], sizeof(Battery_Parameter_Frame->battery_type));
		//5.蓄电池额定容量
		sprintf(&tmp[0], "%08d", Globa_1->BMS_rate_AH);//UINT32 Globa_1 -> BMS_rate_AH;	//BMS额定容量  AH				
		memcpy(&Battery_Parameter_Frame->rated_AH[0], &tmp[0], sizeof(Battery_Parameter_Frame->rated_AH));
		//6. 蓄电池组额定总电压
		sprintf(&tmp[0], "%08d", Globa_1->BMS_rate_voltage);//UINT32 BMS_rate_voltage;//BMS额定电压  0.001V			
		memcpy(&Battery_Parameter_Frame->battery_group_total_vol[0], &tmp[0], sizeof(Battery_Parameter_Frame->battery_group_total_vol));	
		//7. 最高允许充电总电压
		sprintf(&tmp[0], "%08d", Globa_1->BMS_H_voltage/100);//UINT32 BMS_H_voltage;//BMS最高允许充电电压  0.001V				
		memcpy(&Battery_Parameter_Frame->max_voltage[0], &tmp[0], sizeof(Battery_Parameter_Frame->max_voltage));
		//8. 最高允许充电电流
		sprintf(&tmp[0], "%08d", Globa_1->BMS_H_current/100);//UINT32 BMS_H_current;	//BMS最高允许充电电流  0.001A				
		memcpy(&Battery_Parameter_Frame->max_current[0], &tmp[0], sizeof(Battery_Parameter_Frame->max_current));
		//9. 单体蓄电池允许最高充电电压
		sprintf(&tmp[0], "%08d", Globa_1->Cell_H_Cha_Vol);//UINT32 Cell_H_Cha_Vol;//单体动力蓄电池最高允许充电电压 0.01 0-24v		
		memcpy(&Battery_Parameter_Frame->battery_cell_max_vol[0], &tmp[0], sizeof(Battery_Parameter_Frame->battery_cell_max_vol));
		//10.蓄电池最高允许温度
		sprintf(&tmp[0], "%08d", (Globa_1->Cell_H_Cha_Temp - 50));				
		memcpy(&Battery_Parameter_Frame->battery_max_T[0], &tmp[0], sizeof(Battery_Parameter_Frame->battery_max_T));
		//11.电池额定电量（Kwh）
		sprintf(&tmp[0], "%08d", Globa_1->Battery_Rate_KWh);//UINT32 Battery_Rate_KWh; //动力蓄电池标称总能量KWH				
		memcpy(&Battery_Parameter_Frame->battery_kwh[0], &tmp[0], sizeof(Battery_Parameter_Frame->battery_kwh));
		//12.枪口号
		Battery_Parameter_Frame->muzzle_num[0] = '0';
		Battery_Parameter_Frame->muzzle_num[1] = '1';
		//sprintf(&tmp[0], "%02d", Globa_1->BMS_rate_AH);				
		//memcpy(&Battery_Parameter_Frame->rated_AH[0], &tmp[0], sizeof(Battery_Parameter_Frame->rated_AH));
	}else if(GUN_NO == 1){//充电枪2号
  	memcpy(&charge_id_2,&Battery_Parameter_Frame->charge_id[0],sizeof(charge_id_2));
		//3.充电桩类型（01：单枪直流充电桩，02单相交流充电桩，03三相交流充电桩，04：双枪轮流充直流充电桩，05：双枪同时充直流充电桩）
		if(Globa_2->Charger_param.System_Type == 0 ){//双枪同时充电
			Battery_Parameter_Frame->type[0] = '0';
			Battery_Parameter_Frame->type[1] = '5';
		}else if(Globa_2->Charger_param.System_Type == 1 ){//轮充
			Battery_Parameter_Frame->type[0] = '0';
			Battery_Parameter_Frame->type[1] = '4';
		}else{
			Battery_Parameter_Frame->type[0] = '0';//单枪
			Battery_Parameter_Frame->type[1] = '1';
		}
		//4.蓄电池类型
		sprintf(&tmp[0], "%02d", Globa_2 ->BATT_type);				
		memcpy(&Battery_Parameter_Frame->battery_type[0], &tmp[0], sizeof(Battery_Parameter_Frame->battery_type));
		//5.蓄电池额定容量
		sprintf(&tmp[0], "%08d", Globa_2->BMS_rate_AH);//UINT32 Globa_1 -> BMS_rate_AH;	//BMS额定容量  AH				
		memcpy(&Battery_Parameter_Frame->rated_AH[0], &tmp[0], sizeof(Battery_Parameter_Frame->rated_AH));
		//6. 蓄电池组额定总电压
		sprintf(&tmp[0], "%08d", Globa_2->BMS_rate_voltage);//UINT32 BMS_rate_voltage;//BMS额定电压  0.001V			
		memcpy(&Battery_Parameter_Frame->battery_group_total_vol[0], &tmp[0], sizeof(Battery_Parameter_Frame->battery_group_total_vol));	
		//7. 最高允许充电总电压
		sprintf(&tmp[0], "%08d", Globa_2->BMS_H_voltage/100);//UINT32 BMS_H_voltage;//BMS最高允许充电电压  0.001V				
		memcpy(&Battery_Parameter_Frame->max_voltage[0], &tmp[0], sizeof(Battery_Parameter_Frame->max_voltage));
		//8. 最高允许充电电流
		sprintf(&tmp[0], "%08d", Globa_2->BMS_H_current/100);//UINT32 BMS_H_current;	//BMS最高允许充电电流  0.001A				
		memcpy(&Battery_Parameter_Frame->max_current[0], &tmp[0], sizeof(Battery_Parameter_Frame->max_current));
		//9. 单体蓄电池允许最高充电电压
		sprintf(&tmp[0], "%08d", Globa_2->Cell_H_Cha_Vol);//UINT32 Cell_H_Cha_Vol;//单体动力蓄电池最高允许充电电压 0.01 0-24v		
		memcpy(&Battery_Parameter_Frame->battery_cell_max_vol[0], &tmp[0], sizeof(Battery_Parameter_Frame->battery_cell_max_vol));
		//10.蓄电池最高允许温度
		sprintf(&tmp[0], "%08d", Globa_2->Cell_H_Cha_Temp - 50);				
		memcpy(&Battery_Parameter_Frame->battery_max_T[0], &tmp[0], sizeof(Battery_Parameter_Frame->battery_max_T));
		//11.电池额定电量（Kwh）
		sprintf(&tmp[0], "%08d", Globa_2->Battery_Rate_KWh);//UINT32 Battery_Rate_KWh; //动力蓄电池标称总能量KWH				
		memcpy(&Battery_Parameter_Frame->battery_kwh[0], &tmp[0], sizeof(Battery_Parameter_Frame->battery_kwh));
		//12.枪口号
			Battery_Parameter_Frame->muzzle_num[0] = '0';
			Battery_Parameter_Frame->muzzle_num[1] = '2';
	/*
		sprintf(&tmp[0], "%02d", Globa_2->BMS_rate_AH);				
		memcpy(&Battery_Parameter_Frame->rated_AH[0], &tmp[0], sizeof(Battery_Parameter_Frame->rated_AH));*/
	}
	//13.MAC
	MD5_DES(&Battery_Parameter_Frame->charge_id[0], sizeof(ISO8583_Battery_Parameter_Request_Frame)-sizeof(Battery_Parameter_Frame->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&Battery_Parameter_Frame->MD5[0], &MD5[0], 8);

	len = ISO8583_Sent_Data(fd, SendBuf, sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_Battery_Parameter_Request_Frame), 0);
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_Battery_Parameter_Request_Frame))){//如果发送不成功
		return (-1);
	}else{
		COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}
int ISO8583_BMS_Real_Data_Sent(int fd,int gun_no)
{
	unsigned int ret,len,data_len,offset =0,i,j;
	unsigned char SendBuf[512];  //发送数据区指针
	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针
	time_t systime=0;
	static unsigned char count;
	struct tm tm_t;
	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_BMS_Real_Data_Request_Frame *BMS_Frame;
	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	BMS_Frame = (ISO8583_BMS_Real_Data_Request_Frame *)(SendBuf+sizeof(ISO8583_Head_Frame));
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'
	memcpy(&Head_Frame->MessgTypeIdent[0], "4060", sizeof(Head_Frame->MessgTypeIdent));//消息标识符
	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_BMS_Real_Data_Request_Frame))>>8;  //报文长度
	Head_Frame->SessionLen[1] = sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_BMS_Real_Data_Request_Frame);
	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化位域内容为 0x00

	for(i = 31; i <= 47; i++){
		if(i == 40 || i == 32){
			Head_Frame->PrimaryBitmap[i/8 -1] |= 0x80 >> ((i-1)%8);//主位元表 40
		}else{
			Head_Frame->PrimaryBitmap[i/8 -0] |= 0x80 >> ((i-1)%8);//主位元表 31-42
		}
	}
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63
	/*count++;
	memcpy(&BMS_Frame->charge_id[0], &Globa_1->Charger_param.SN[0], sizeof(BMS_Frame->ter_sn));
	systime = time(NULL);        //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
	sprintf(&tmp[0], "%04d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday);
	memcpy(&BMS_Frame->charge_id[12], &tmp[2],6);//终端时间
	sprintf(&tmp[0], "%03d", count);
	memcpy(&BMS_Frame->charge_id[18],&tmp[0],3);
	*/
	memcpy(&BMS_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(BMS_Frame->ter_sn));
	if((Globa_1->Charger_param.System_Type == 0 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪同时
		BMS_Frame->type[0] = '0';
		BMS_Frame->type[1] = '5';
	}else if(Globa_1->Charger_param.System_Type == 1 ){//轮充
		BMS_Frame->type[0] = '0';
		BMS_Frame->type[1] = '4';
	}else{
		BMS_Frame->type[0] = '0';//单枪
		BMS_Frame->type[1] = '1';
	}
	if(gun_no == 0){
		memcpy(&BMS_Frame->charge_id[0],&charge_id_1,sizeof(charge_id_1));
		sprintf(&tmp[0], "%08d", Globa_1->need_voltage/100);	//UINT32 need_voltage;//电压需求  0.1V			
		memcpy(&BMS_Frame->charge_vol_require[0], &tmp[0], sizeof(BMS_Frame->charge_vol_require));
		sprintf(&tmp[0], "%08d", Globa_1->need_current/100);//UINT32 need_current;	//电流需求  0.001A		
		memcpy(&BMS_Frame->charge_cur_require[0], &tmp[0], sizeof(BMS_Frame->charge_cur_require));
		sprintf(&tmp[0], "%08d", Globa_1->Output_voltage/100);//	UINT32 meter_voltage;//计量输出电压 0.001V
		memcpy(&BMS_Frame->charge_vol_measure[0], &tmp[0], sizeof(BMS_Frame->charge_vol_measure));
		sprintf(&tmp[0], "%08d", Globa_1->Output_current/100);//UINT32 meter_current;//计量输出电流 0.001A				
		memcpy(&BMS_Frame->charge_cur_measure[0], &tmp[0], sizeof(BMS_Frame->charge_cur_measure));
		sprintf(&tmp[0], "%08d", Globa_1->BMS_cell_HV);//UINT32 BMS_cell_HV;//单体电池最高电压   0.01V			
		memcpy(&BMS_Frame->battery_cell_H_Vol[0], &tmp[0], sizeof(BMS_Frame->battery_cell_H_Vol));
		sprintf(&tmp[0], "%02d", Globa_1 -> BMS_batt_SOC);//UINT32 BMS_batt_SOC;//BMS电池容量			
		memcpy(&BMS_Frame->soc[0], &tmp[0], sizeof(BMS_Frame->soc));
		sprintf(&tmp[0], "%08d", (Globa_1 ->BMS_cell_HT- (TEMP_OFSET)));//INT16 BMS_cell_HT;//单体电池最高温度   1  //				
		memcpy(&BMS_Frame->battery_H_T[0], &tmp[0], sizeof(BMS_Frame->battery_H_T));
		sprintf(&tmp[0], "%08d", (Globa_1 ->BMS_cell_LT- (TEMP_OFSET)));//INT16 BMS_cell_LT;//单体电池最低温度   1			
		memcpy(&BMS_Frame->battery_L_T[0], &tmp[0], sizeof(BMS_Frame->battery_L_T));
	//	memcpy(&BMS_Frame->car_sn[0], &Globa_1->VIN[0], sizeof(BMS_Frame->car_sn));
		if(((Globa_1->VIN[0] == 0xFF)&&(Globa_1->VIN[1] == 0xFF)&&(Globa_1->VIN[2] == 0xFF))){
		  memcpy(&BMS_Frame->car_sn[0], "00000000000000000",sizeof(BMS_Frame->car_sn));
		}else{
		  memcpy(&BMS_Frame->car_sn[0], &Globa_1->VIN[0], sizeof(BMS_Frame->car_sn));
		}

		sprintf(&tmp[0], "%03d", Globa_1 ->BMS_cell_HV_NO);//UINT32 BMS_cell_HV_NO;//8最高单体电压				
		memcpy(&BMS_Frame->battery_cell_H_VOL_NO[0], &tmp[0], sizeof(BMS_Frame->battery_cell_H_VOL_NO));
		sprintf(&tmp[0], "%03d", Globa_1 ->BMS_cell_HT_NO);//UINT8 BMS_cell_HT_NO;//6电池组最高温度编号					
		memcpy(&BMS_Frame->battery_cell_H_T_NO[0], &tmp[0], sizeof(BMS_Frame->battery_cell_H_T_NO));
		sprintf(&tmp[0], "%03d", Globa_1 ->BMS_cell_LT_NO);//UINT8 BMS_cell_LT_NO;//5电池组最低温度编号				
		memcpy(&BMS_Frame->battery_cell_L_T_NO[0], &tmp[0], sizeof(BMS_Frame->battery_cell_L_T_NO));
		sprintf(&tmp[0], "%08d", (Globa_1->Charge_Gun_Temp - 50));//UINT32 Charge_Gun_Temp;//充电枪温度				
		memcpy(&BMS_Frame->gun_T[0], &tmp[0], sizeof(BMS_Frame->gun_T));
		
		BMS_Frame->muzzle_NO[0] = '0';
		BMS_Frame->muzzle_NO[1] = '1';
	}else{
		memcpy(&BMS_Frame->charge_id[0],&charge_id_2,sizeof(charge_id_2));
		sprintf(&tmp[0], "%08d", Globa_2->need_voltage/100);	//UINT32 need_voltage;//电压需求  0.1V			
		memcpy(&BMS_Frame->charge_vol_require[0], &tmp[0], sizeof(BMS_Frame->charge_vol_require));
		sprintf(&tmp[0], "%08d", Globa_2->need_current/100);//UINT32 need_current;	//电流需求  0.001A		
		memcpy(&BMS_Frame->charge_cur_require[0], &tmp[0], sizeof(BMS_Frame->charge_cur_require));
		sprintf(&tmp[0], "%08d", Globa_2->Output_voltage/100);//	UINT32 meter_voltage;//计量输出电压 0.001V
		memcpy(&BMS_Frame->charge_vol_measure[0], &tmp[0], sizeof(BMS_Frame->charge_vol_measure));
		sprintf(&tmp[0], "%08d", Globa_2->Output_current/100);//UINT32 meter_current;//计量输出电流 0.001A				
		memcpy(&BMS_Frame->charge_cur_measure[0], &tmp[0], sizeof(BMS_Frame->charge_cur_measure));
		sprintf(&tmp[0], "%08d", Globa_2->BMS_cell_HV);//UINT32 BMS_cell_HV;//单体电池最高电压   0.01V			
		memcpy(&BMS_Frame->battery_cell_H_Vol[0], &tmp[0], sizeof(BMS_Frame->battery_cell_H_Vol));
		sprintf(&tmp[0], "%02d", Globa_2 ->BMS_batt_SOC);//UINT32 BMS_batt_SOC;//BMS电池容量			
		memcpy(&BMS_Frame->soc[0], &tmp[0], sizeof(BMS_Frame->soc));
		sprintf(&tmp[0], "%08d", (Globa_2 ->BMS_cell_HT - (TEMP_OFSET)));//INT16 BMS_cell_HT;//单体电池最高温度   1  //				
		memcpy(&BMS_Frame->battery_H_T[0], &tmp[0], sizeof(BMS_Frame->battery_H_T));
		sprintf(&tmp[0], "%08d", (Globa_2 ->BMS_cell_LT - (TEMP_OFSET)));//INT16 BMS_cell_LT;//单体电池最低温度   1			
		memcpy(&BMS_Frame->battery_L_T[0], &tmp[0], sizeof(BMS_Frame->battery_L_T));
		if(((Globa_2->VIN[0] == 0xFF)&&(Globa_2->VIN[1] == 0xFF)&&(Globa_2->VIN[2] == 0xFF))){
		 memcpy(&BMS_Frame->car_sn[0], "00000000000000000",  sizeof(BMS_Frame->car_sn));
		}else{
		  memcpy(&BMS_Frame->car_sn[0], &Globa_2->VIN[0], sizeof(BMS_Frame->car_sn));
		}
		sprintf(&tmp[0], "%03d", Globa_2 -> BMS_cell_HV_NO);//UINT32 BMS_cell_HV_NO;//8最高单体电压				
		memcpy(&BMS_Frame->battery_cell_H_VOL_NO[0], &tmp[0], sizeof(BMS_Frame->battery_cell_H_VOL_NO));
		sprintf(&tmp[0], "%03d", Globa_2 -> BMS_cell_HT_NO);//UINT8 BMS_cell_HT_NO;//6电池组最高温度编号					
		memcpy(&BMS_Frame->battery_cell_H_T_NO[0], &tmp[0], sizeof(BMS_Frame->battery_cell_H_T_NO));
		sprintf(&tmp[0], "%03d", Globa_2 -> BMS_cell_LT_NO);//UINT8 BMS_cell_LT_NO;//5电池组最低温度编号				
		memcpy(&BMS_Frame->battery_cell_L_T_NO[0], &tmp[0], sizeof(BMS_Frame->battery_cell_L_T_NO));
		sprintf(&tmp[0], "%08d", (Globa_2->Charge_Gun_Temp - 50));//UINT32 Charge_Gun_Temp;//充电枪温度				
		memcpy(&BMS_Frame->gun_T[0], &tmp[0], sizeof(BMS_Frame->gun_T));
		
		BMS_Frame->muzzle_NO[0] = '0';
		BMS_Frame->muzzle_NO[1] = '2';
	}
	MD5_DES(&BMS_Frame->charge_id[0], sizeof(ISO8583_BMS_Real_Data_Request_Frame)-sizeof(BMS_Frame->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&BMS_Frame->MD5[0], &MD5[0], 8);
	len = ISO8583_Sent_Data(fd, SendBuf, sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_BMS_Real_Data_Request_Frame), 0);
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_BMS_Real_Data_Request_Frame))){//如果发送不成功
		return (-1);
	}else{
		COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}
/***********创建TCP连接 作为客户端 *********************************************
阻塞模式connect要花一个往返时间完成，从几毫秒的局域网到几百毫秒或几秒的广域网。
多数实现中，系统默认connect的超时时间在75秒到几分钟之间。

本程序实现步骤1： 设置非阻塞，启动连接

实现非阻塞 connect ，首先把 sockfd 设置成非阻塞的。这样调用 connect 可以立刻返回，
根据返回值和 errno 处理三种情况：
(1) 如果返回 0，表示 connect 成功。
(2) 如果返回值小于 0， errno 为 EINPROGRESS, 表示连接建立已经启动但是尚未完成。这是期望的结果，不是真正的错误。
(3) 如果返回值小于0，errno 不是 EINPROGRESS，则连接出错了。

步骤2：判断可读和可写

然后把 sockfd 加入 select 的读写监听集合，通过 select 判断 sockfd 
是否可写，处理三种情况：
(1) 如果连接建立好了，对方没有数据到达，那么 sockfd 是可写的
(2) 如果在 select 之前，连接就建立好了，而且对方的数据已到达，那么 sockfd 是可读和可写的。
(3) 如果连接发生错误，sockfd 也是可读和可写的。
判断 connect 是否成功，就得区别 (2) 和 (3)，这两种情况下 sockfd 都是
可读和可写的，区分的方法是，调用 getsockopt 检查是否出错。

步骤3：使用 getsockopt 函数检查错误

getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len)
在 sockfd 都是可读和可写的情况下，我们使用 getsockopt 来检查连接
是否出错。但这里有一个可移植性的问题。
如果发生错误，getsockopt 源自 Berkeley 的实现将在变量 error 中
返回错误，getsockopt 本身返回0；然而 Solaris 却让 getsockopt 返回 -1，
并把错误保存在 errno 变量中。所以在判断是否有错误的时候，要处理这两种情况。  
*******************************************************************************/
/*******************************************************************************
**TCP_Client_Socket_Creat: TCP的客户端Socket连接建立,非阻塞模式
***parameter  :
			ip:服务端IP地址
			port: 服务端端口号
			overtime: 连接建立超时时间，微秒(us)为单位
***return		  :
      正常: clifd
      异常: -1
*******************************************************************************/
int TCP_Client_Socket_Creat(unsigned char *ip, unsigned int port, unsigned int overtime)
{
	int ret=0, clifd=0;
	struct timeval tm;
	fd_set set;
	int error;
	socklen_t len;
	struct sockaddr_in servaddr;

	if((clifd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
		perror("create socket error!!!\n");
		return (-1);
	}

	unsigned long ul = 1;
	ioctl(clifd, FIONBIO, &ul);//系统默认为阻塞模式，此处设置为非阻塞模式

	//bind API 函数将允许地址的立即重用
	int bind_on = 1;
	setsockopt(clifd, SOL_SOCKET, SO_REUSEADDR, &bind_on, sizeof(bind_on));

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	inet_aton(ip, &servaddr.sin_addr);
	servaddr.sin_port = htons(port);

	//如果本机网络接口是断开的话，会立即连接失败返回
	if(connect(clifd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
		if(errno != EINPROGRESS){
			ret = 0;
		}else{
  		tm.tv_sec  = overtime/1000000;
  		tm.tv_usec = overtime%1000000;

  		FD_ZERO(&set);
  		FD_SET(clifd, &set);
  		if(select(clifd+1, NULL, &set, NULL, &tm) > 0){
  			error = 1;
  			len = sizeof(error);
  			getsockopt(clifd, SOL_SOCKET, SO_ERROR, &error, &len);   
  			if(error == 0){
  				ret = 1;
  			}else{
  				ret = 0;
  			}
  		}else{//超时或有错误
  			ret = 0;
  		}
		}
	}else{
		ret = 1;
	}

	if(ret){
		return (clifd);
	}else{
		close(clifd);
		return (-1);
	}
}


//APP请求 发送上行数据
int ISO8583_AP_APP_Sent(int fd, unsigned int flag,int gun)
{
	unsigned int ret,len;
	unsigned char SendBuf[512];  //发送数据区指针
	unsigned char MD5[8];  //发送数据区指针

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_AP_APP_Contol_Resp_Frame *AP_Frame;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	AP_Frame = (ISO8583_AP_APP_Contol_Resp_Frame *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据初始化
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'

	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_APP_Contol_Resp_Frame))>>8;  //报文长度
	Head_Frame->SessionLen[1] = sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_APP_Contol_Resp_Frame);
	memcpy(&Head_Frame->MessgTypeIdent[0], "3011", sizeof(Head_Frame->MessgTypeIdent));//消息标识符

	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化位域内容为 0x00
	Head_Frame->PrimaryBitmap[9/8 -0] |= 0x80 >> ((9-1)%8);//主位元表 9
	Head_Frame->PrimaryBitmap[39/8 -0] |= 0x80 >> ((39-1)%8);//主位元表 39
	Head_Frame->PrimaryBitmap[41/8 -0] |= 0x80 >> ((41-1)%8);//主位元表 41
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63

	if(gun == 1){
	  memcpy(&AP_Frame->ID[0], &ISO8583_App_Struct.App_ID_2[0], sizeof(AP_Frame->ID));
	}else{
		memcpy(&AP_Frame->ID[0], &ISO8583_App_Struct.App_ID_1[0], sizeof(AP_Frame->ID));
	}
	
	AP_Frame->flag[0] = (flag/10)%10 + 0x30;
	AP_Frame->flag[1] = flag%10 + 0x30;

	memcpy(&AP_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(AP_Frame->ter_sn));//充电终端序列号    41	N	n12

	MD5_DES(&AP_Frame->ID[0], sizeof(ISO8583_AP_APP_Contol_Resp_Frame)-sizeof(AP_Frame->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&AP_Frame->MD5[0], &MD5[0], 8);

	len = ISO8583_Sent_Data(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_APP_Contol_Resp_Frame)), 0);
	
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_APP_Contol_Resp_Frame))){//如果发送不成功
		return (-1);
	}else{
		if(gun == 1){
		  AP_APP_Sent_2_falg[0] = 0; //表示发送成功则清除标志
		  AP_APP_Sent_2_falg[1] = 0; //表示发送成功则清除标志
		}else{
			AP_APP_Sent_1_falg[0] = 0; //表示发送成功则清除标志
		  AP_APP_Sent_1_falg[1] = 0; //表示发送成功则清除标志
		}
		if(DEBUGLOG) COMM_RUN_LogOut("充电桩反馈充电启动/停止数据:枪号:%d  结果：%d APP_ID:%.16s",gun+1,flag,AP_Frame->ID);  
		COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}


//APP请求 数据处理
int ISO8583_Heart_APP_Rece_Deal(int length, unsigned char *buf)
{
	int j,i;
	int flag;
	int flag_gun=0,uitmp1,uitmp2,uitmp3,uitmp4,sendfalg = 0;

	unsigned char tmp[100],logtmp[200]={"\0"};
	unsigned char MD5[8];  //发送数据区指针

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_AP_APP_Contol_Frame *APP_Contol_Frame;
  ISO8583_AP_APP_Contol_Private_Frame *APP_Contol_Private_Frame;
	Head_Frame = (ISO8583_Head_Frame *)(buf);
	APP_Contol_Frame = (ISO8583_AP_APP_Contol_Frame *)(buf+sizeof(ISO8583_Head_Frame));
	APP_Contol_Private_Frame = (ISO8583_AP_APP_Contol_Private_Frame *)(buf+sizeof(ISO8583_Head_Frame));

	//if(DEBUGLOG) GLOBA_RUN_LogOut("%s %s %s","APP下发启动停止充电数据",Head_Frame,APP_Contol_Frame);  
	flag = 0;
	if(length >= (sizeof(ISO8583_Head_Frame) + sizeof(APP_Contol_Frame->MD5))){
		MD5_DES(&APP_Contol_Frame->start[0], (length - sizeof(ISO8583_Head_Frame)-sizeof(APP_Contol_Frame->MD5)), &ISO8583_App_Struct.work_key[0], &MD5[0]);
		hex_to_str(&tmp[0], &MD5[0], 8);
	  if(0 == memcmp(&tmp[0], &buf[length - sizeof(APP_Contol_Frame->MD5)], sizeof(APP_Contol_Frame->MD5))){
			time(&Rcev_current_now);
		  Rcev_pre_time = Rcev_current_now;
			if(length == (sizeof(ISO8583_Head_Frame) + sizeof(ISO8583_AP_APP_Contol_Frame))){
			 if(DEBUGLOG) COMM_RUN_LogOut("APP下发启动停止充电数据:命令:%x 枪号:%x appID：%.16s",APP_Contol_Frame->start[1],APP_Contol_Frame->num[1],APP_Contol_Frame->ID);  
				if((APP_Contol_Frame->start[0] == '0')&&(APP_Contol_Frame->start[1] == '1')){//启动充电
					if(APP_Contol_Frame->num[1] == '1'){ //第一把枪
						if(0 != memcmp(&tmp[0], &APP_Contol_Frame->MD5[0], sizeof(APP_Contol_Frame->MD5))){
							flag = 0x01;//01：MAC校验失败
							sendfalg = 0;
						}else if(0 != memcmp(&APP_Contol_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(APP_Contol_Frame->ter_sn))){
							flag = 0x02;//02：充电桩序列号不对应
							sendfalg = 0;
						}else if(Globa_1->gun_link != 1){
							flag = 0x03;//03：未插枪
							sendfalg = 0;
						}else if((Globa_1->App.APP_start == 1)&&((0 != memcmp(&Globa_1->App.ID, &APP_Contol_Frame->ID[0], sizeof(Globa_1->App.ID)))||(0 != memcmp(&Globa_1->App.busy_SN[0], &APP_Contol_Frame->Busy_SN[0], sizeof(Globa_1->App.busy_SN))))){//用户号和业务流水号不一样的时候启动不了
							flag = 0x04;//04：充电桩已被使用，无法充电
							sendfalg = 0;
						}else if((Globa_1->App.APP_start == 1)&&(0 == memcmp(&Globa_1->App.ID, &APP_Contol_Frame->ID[0], sizeof(Globa_1->App.ID)))&&(0 == memcmp(&Globa_1->App.busy_SN[0], &APP_Contol_Frame->Busy_SN[0], sizeof(Globa_1->App.busy_SN)))){//同一个账号,并且业务流水号一样则直接反馈正常
							flag = 0x00;//04：充电桩已被同一个账号启用，反馈成功
							sendfalg = 1;
						}else if((Globa_1->Error_system != 0)||(Globa_1->Electric_pile_workstart_Enable == 1)){
							flag = 0x06;//06：充电桩故障无法充电
							sendfalg = 0;
						}else if((Globa_2->QT_Step > 0x03)&&(Globa_1->Charger_param.System_Type == 1 )){//轮流充电的时候有一把启动了就不能启动另一把
							flag = 0x07;//07:充电桩忙
							sendfalg = 0;
						}else if((Globa_1->QT_Step > 0x03)){
							flag = 0x07;//07:充电桩忙
							sendfalg = 0;
						}else if((0 != IsParkUserIDValid(&Globa_1->Car_lock_Card[0]))&&(0 != memcmp(&Globa_1->Car_lock_Card, &APP_Contol_Frame->ID[0], sizeof(Globa_1->Car_lock_Card)))){
							flag = 0x04;//06：充电桩故障无法充电
							sendfalg = 0;
						}
						memcpy(&ISO8583_App_Struct.App_ID_1[0], &APP_Contol_Frame->ID[0], sizeof(ISO8583_App_Struct.App_ID_1));
						AP_APP_Sent_1_falg[1] = flag; //表示接收到密钥到反馈信息
						if((flag == 0)&&(sendfalg == 0)){
					  	AP_APP_Sent_1_falg[0] = 2; //表示进行收到数据并写入数据库之后在反馈
						}else{
							AP_APP_Sent_1_falg[0] = 1;
						}
						//if(ret >= 0){
						 if(DEBUGLOG) COMM_RUN_LogOut("APP下发启动停止充电数据:命令:%x 枪号:%x  结果：%d AP_APP_Sent_1_falg[0] = %d",APP_Contol_Frame->start[1],APP_Contol_Frame->num[1],flag,AP_APP_Sent_1_falg[0]);  
							if((flag == 0)&&(Globa_1->App.APP_start == 0)){//无异常，可以正常充电,防止再次赋值
									sprintf(&tmp[0], "%.8s", &APP_Contol_Frame->data[0]);
								if(APP_Contol_Frame->type[2] == '1'){
									Globa_1->App.APP_charge_kwh = atoi(&tmp[0]);
									Globa_1->App.APP_charger_type = 1;
								}else if(APP_Contol_Frame->type[2] == '2'){
									Globa_1->App.APP_charge_time = atoi(&tmp[0]) + Globa_1->user_sys_time ;
									Globa_1->APP_Card_charge_time_limit = atoi(&tmp[0]);
									Globa_1->App.APP_charger_type = 2;
								}else if(APP_Contol_Frame->type[2] == '3'){
									Globa_1->App.APP_charge_money = atoi(&tmp[0]);
									Globa_1->App.APP_charger_type = 3;
								}else if(APP_Contol_Frame->type[2] == '4'){
									Globa_1->App.APP_charger_type = 4;
								}
				
								if(APP_Contol_Frame->BMS_Power_V[1] == '2' ){
									Globa_1->BMS_Power_V = 1;
								}else{
									Globa_1->BMS_Power_V = 0;
								}
								if(APP_Contol_Frame->APP_Start_Account_type[1] == '1' ){
									Globa_1->APP_Start_Account_type = 0;//1;
								}else{
									Globa_1->APP_Start_Account_type = 0;
								}
								sprintf(&tmp[0], "%.8s", &APP_Contol_Frame->data[0]);
								if(APP_Contol_Frame->type[2] == '1'){
									Globa_1->App.APP_charge_kwh = atoi(&tmp[0]);
									Globa_1->App.APP_charger_type = 1;
								}else if(APP_Contol_Frame->type[2] == '2'){
									Globa_1->App.APP_charge_time = atoi(&tmp[0]) + Globa_1->user_sys_time ;
									Globa_1->APP_Card_charge_time_limit = atoi(&tmp[0]);
									Globa_1->App.APP_charger_type = 2;
								}else if(APP_Contol_Frame->type[2] == '3'){
									Globa_1->App.APP_charge_money = atoi(&tmp[0]);
									Globa_1->App.APP_charger_type = 3;
								}else if(APP_Contol_Frame->type[2] == '4'){
									Globa_1->App.APP_charger_type = 4;
								}
								memcpy(&Globa_1->App.busy_SN[0], &APP_Contol_Frame->Busy_SN[0], sizeof(Globa_1->App.busy_SN));
						    memcpy(&Globa_1->App.ID[0], &APP_Contol_Frame->ID[0], sizeof(Globa_1->App.ID));
								sprintf(&tmp[0], "%.8s", &APP_Contol_Frame->last_rmb[0]);
								Globa_1->App.last_rmb = atoi(&tmp[0]);
								Globa_1->App.APP_start = 1;
							}
							return (0);
						//}
					}else if(APP_Contol_Frame->num[1] == '2'){//第二把枪
						if(0 != memcmp(&tmp[0], &APP_Contol_Frame->MD5[0], sizeof(APP_Contol_Frame->MD5))){
							flag = 0x01;//01：MAC校验失败
							sendfalg = 0;
						}else if(0 != memcmp(&APP_Contol_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(APP_Contol_Frame->ter_sn))){
							flag = 0x02;//02：充电桩序列号不对应
							sendfalg = 0;
						}else if(Globa_2->gun_link != 1){
							flag = 0x03;//03：未插枪
							sendfalg = 0;
						}else if((Globa_2->App.APP_start == 1)&&((0 != memcmp(&Globa_2->App.ID, &APP_Contol_Frame->ID[0], sizeof(Globa_2->App.ID)))||(0 != memcmp(&Globa_2->App.busy_SN[0], &APP_Contol_Frame->Busy_SN[0], sizeof(Globa_2->App.busy_SN))))){//用户号和业务流水号不一样的时候启动不了
							flag = 0x04;//04：充电桩已被使用，无法充电
							sendfalg = 0;
						}else if((Globa_2->App.APP_start == 1)&&(0 == memcmp(&Globa_2->App.ID, &APP_Contol_Frame->ID[0], sizeof(Globa_2->App.ID)))&&(0 == memcmp(&Globa_2->App.busy_SN[0], &APP_Contol_Frame->Busy_SN[0], sizeof(Globa_2->App.busy_SN)))){//同一个账号,并且业务流水号一样则直接反馈正常
							flag = 0x00;//04：充电桩已被同一个账号启用，反馈成功
							sendfalg = 1;
						}else if((Globa_2->Error_system != 0)||(Globa_2->Electric_pile_workstart_Enable == 1)){
							flag = 0x06;//06：充电桩故障无法充电
							sendfalg = 0;
						}else if((Globa_1->QT_Step > 0x03)&&(Globa_1->Charger_param.System_Type == 1 )){//轮流充电的时候有一把启动了就不能启动另一把
							flag = 0x07;//07:充电桩忙
							sendfalg = 0;
						}else if((Globa_2->QT_Step != 0x03)||(!((Globa_1->QT_Step == 0x02)||(Globa_1->QT_Step == 0x03)||(Globa_1->QT_Step == 0x201)||(Globa_1->QT_Step == 0x21)||(Globa_1->QT_Step == 0x23)||(Globa_1->QT_Step == 0x22)))){
							flag = 0x07;//07:充电桩忙
							sendfalg = 0;
						}else if((0 != IsParkUserIDValid(&Globa_2->Car_lock_Card[0]))&&(0 != memcmp(&Globa_2->Car_lock_Card, &APP_Contol_Frame->ID[0], sizeof(Globa_2->Car_lock_Card)))){
							flag = 0x04;//06：充电桩故障无法充电
							sendfalg = 0;
						}
					
						if(ISO8583_DEBUG)printf("ISO8583_Heart_APP_Rece_Deal 启动充电22222222= %d Globa_2->QT_Step= %02x Globa_1->QT_Step= %02x\n", flag,Globa_2->QT_Step,Globa_1->QT_Step);
						//ret = ISO8583_AP_APP_Sent(fd, flag,1);
					  memcpy(&ISO8583_App_Struct.App_ID_2[0], &APP_Contol_Frame->ID[0], sizeof(ISO8583_App_Struct.App_ID_2));
						AP_APP_Sent_2_falg[1] = flag; //表示接收到密钥到反馈信息
						if((flag == 0)&&(sendfalg == 0)){
						AP_APP_Sent_2_falg[0] = 2; //表示接收到密钥到反馈信息---需要进数据启动判断
						}else{
							AP_APP_Sent_2_falg[0] = 1;
						}
					//	if(ret >= 0){
					 if((flag == 0)&&(Globa_2->App.APP_start == 0)){//无异常，可以正常充电,防止再次赋值
							if(APP_Contol_Frame->BMS_Power_V[1] == '2' ){
								Globa_2->BMS_Power_V = 1;
							}else{
								Globa_2->BMS_Power_V = 0;
							}
								
							if(APP_Contol_Frame->APP_Start_Account_type[1] == '1' ){
								Globa_2->APP_Start_Account_type = 0;//1;
							}else{
								Globa_2->APP_Start_Account_type = 0;
							}
							sprintf(&tmp[0], "%.8s", &APP_Contol_Frame->data[0]);
							if(APP_Contol_Frame->type[2] == '1'){
								Globa_2->App.APP_charge_kwh = atoi(&tmp[0]);
								Globa_2->App.APP_charger_type = 1;
							}else if(APP_Contol_Frame->type[2] == '2'){
								Globa_2->App.APP_charge_time = atoi(&tmp[0]) + Globa_2->user_sys_time ;
								Globa_2->App.APP_charger_type = 2;
								Globa_2->APP_Card_charge_time_limit = atoi(&tmp[0]);
							}else if(APP_Contol_Frame->type[2] == '3'){
								Globa_2->App.APP_charge_money = atoi(&tmp[0]);
								Globa_2->App.APP_charger_type = 3;
							}else if(APP_Contol_Frame->type[2] == '4'){
								Globa_2->App.APP_charger_type = 4;
							}
							memcpy(&Globa_2->App.busy_SN[0], &APP_Contol_Frame->Busy_SN[0], sizeof(Globa_2->App.busy_SN));
						  memcpy(&Globa_2->App.ID[0], &APP_Contol_Frame->ID[0], sizeof(Globa_2->App.ID));
						  sprintf(&tmp[0], "%.8s", &APP_Contol_Frame->last_rmb[0]);
						  Globa_2->App.last_rmb = atoi(&tmp[0]);
				      Globa_2->App.APP_start = 1;
						}
						if(DEBUGLOG) COMM_RUN_LogOut("APP下发启动停止充电数据:命令:%x 枪号:%x  结果：%d",APP_Contol_Frame->start[1],APP_Contol_Frame->num[1],flag);  
						return (0);
					//}
					}
				}else if((APP_Contol_Frame->start[0] == '0')&&(APP_Contol_Frame->start[1] == '2')){//停止充电
					if(0 != memcmp(&tmp[0], &APP_Contol_Frame->MD5[0], sizeof(APP_Contol_Frame->MD5))){
						flag = 0x01;//01：MAC校验失败
						if((APP_Contol_Frame->num[1] == '2'))
						{
							flag_gun = 2;
						}else{
							flag_gun = 1;
						}
					}else if(0 != memcmp(&APP_Contol_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(APP_Contol_Frame->ter_sn))){
						flag = 0x02;//02：充电桩序列号不对应
						if((APP_Contol_Frame->num[1] == '2'))
						{
							flag_gun = 2;
						}else{
							flag_gun = 1;
						}
					}else if((0 == memcmp(&APP_Contol_Frame->ID[0], &Globa_1->App.ID[0], sizeof(APP_Contol_Frame->ID)))&&((APP_Contol_Frame->num[1] == '1'))){
						flag = 0;
						flag_gun = 1;
					}else if((0 == memcmp(&APP_Contol_Frame->ID[0], &Globa_2->App.ID[0], sizeof(APP_Contol_Frame->ID)))&&(APP_Contol_Frame->num[1] == '2')){
						flag = 0;
						flag_gun = 2;
					}else{
						flag = 0x05;//05：非当前用户，无法取消充电
						if((APP_Contol_Frame->num[1] == '2'))
						{
							flag_gun = 2;
						}else{
							flag_gun = 1;
						}
					}
	
					if(flag_gun == 2){
						memcpy(&ISO8583_App_Struct.App_ID_2[0], &APP_Contol_Frame->ID[0], sizeof(ISO8583_App_Struct.App_ID_2));
						AP_APP_Sent_2_falg[1] = flag; //表示接收到密钥到反馈信息
						AP_APP_Sent_2_falg[0] = 1; //表示接收到密钥到反馈信息
					}else{
						memcpy(&ISO8583_App_Struct.App_ID_1[0], &APP_Contol_Frame->ID[0], sizeof(ISO8583_App_Struct.App_ID_1));
						AP_APP_Sent_1_falg[1] = flag; //表示接收到密钥到反馈信息
						AP_APP_Sent_1_falg[0] = 1; //表示接收到密钥到反馈信息
					}
					
					if(DEBUGLOG) COMM_RUN_LogOut("APP下发停止充电数据:命令:%x 枪号:%x 结果：%d",APP_Contol_Frame->start[1],APP_Contol_Frame->num[1],flag);  
					if((Globa_1->App.APP_start == 1)&&(flag == 0)&&(flag_gun == 1)){//已经开启充电
						Globa_1->App.APP_start = 0;
						if(ISO8583_DEBUG)printf("停止充电 xxxxxxxx111111 = %d\n", flag);
					}else if((Globa_2->App.APP_start == 1)&&(flag == 0)&&(flag_gun == 2)){//已经开启充电
						Globa_2->App.APP_start = 0;
						if(ISO8583_DEBUG)printf("停止充电 xxxxxxxx2222222222 = %d\n", flag);
					}
					return (0);
					//}
				}else{//非法指令，不处理，直接丢弃，不当做连接有问题而关闭
					return (0);
				}
			}else if(length == (sizeof(ISO8583_Head_Frame) + sizeof(ISO8583_AP_APP_Contol_Private_Frame))){//带私有电价的
				if(DEBUGLOG) COMM_RUN_LogOut("APP下发启动停止充电数据:命令:%x 枪号:%x ",APP_Contol_Private_Frame->start[1],APP_Contol_Private_Frame->num[1]);  
				if((APP_Contol_Private_Frame->start[0] == '0')&&(APP_Contol_Private_Frame->start[1] == '1')){//启动充电
					if(APP_Contol_Private_Frame->num[1] == '1'){ //第一把枪
						if(0 != memcmp(&tmp[0], &APP_Contol_Private_Frame->MD5[0], sizeof(APP_Contol_Private_Frame->MD5))){
							flag = 0x01;//01：MAC校验失败
							sendfalg = 0;
						}else if(0 != memcmp(&APP_Contol_Private_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(APP_Contol_Private_Frame->ter_sn))){
							flag = 0x02;//02：充电桩序列号不对应
							sendfalg = 0;
						}else if(Globa_1->gun_link != 1){
							flag = 0x03;//03：未插枪
							sendfalg = 0;
						}else if((Globa_1->App.APP_start == 1)&&((0 != memcmp(&Globa_1->App.ID, &APP_Contol_Frame->ID[0], sizeof(Globa_1->App.ID)))||(0 != memcmp(&Globa_1->App.busy_SN[0], &APP_Contol_Frame->Busy_SN[0], sizeof(Globa_1->App.busy_SN))))){//用户号和业务流水号不一样的时候启动不了
							flag = 0x04;//04：充电桩已被使用，无法充电
							sendfalg = 0;
						}else if((Globa_1->App.APP_start == 1)&&(0 == memcmp(&Globa_1->App.ID, &APP_Contol_Frame->ID[0], sizeof(Globa_1->App.ID)))&&(0 == memcmp(&Globa_1->App.busy_SN[0], &APP_Contol_Frame->Busy_SN[0], sizeof(Globa_1->App.busy_SN)))){//同一个账号,并且业务流水号一样则直接反馈正常
							flag = 0x00;//04：充电桩已被同一个账号启用，反馈成功
							sendfalg = 1;
						}else if((Globa_1->Error_system != 0)||(Globa_1->Electric_pile_workstart_Enable == 1)){
							flag = 0x06;//06：充电桩故障无法充电
							sendfalg = 0;
						}else if((Globa_2->QT_Step > 0x03)&&(Globa_1->Charger_param.System_Type == 1 )){//轮流充电的时候有一把启动了就不能启动另一把
							flag = 0x07;//07:充电桩忙
							sendfalg = 0;
						}else if((Globa_1->QT_Step > 0x03)){
							flag = 0x07;//07:充电桩忙
							sendfalg = 0;
						}
						
						if(ISO8583_DEBUG)printf("ISO8583_Heart_APP_Rece_Deal 启动充电 111111111 = %d Globa_1->QT_Step= %02x\n", flag,Globa_1->QT_Step);
						 memcpy(&ISO8583_App_Struct.App_ID_1[0], &APP_Contol_Private_Frame->ID[0], sizeof(ISO8583_App_Struct.App_ID_1));
						AP_APP_Sent_1_falg[1] = flag; //表示接收到密钥到反馈信息
						if((flag == 0)&&(sendfalg == 0)){
						AP_APP_Sent_1_falg[0] = 2; //表示接收到密钥到反馈信息
						}else{
							AP_APP_Sent_1_falg[0] = 1;
						}	
						if((flag == 0)&&(Globa_1->App.APP_start == 0)){//无异常，可以正常充电
							sprintf(&tmp[0], "%.8s", &APP_Contol_Private_Frame->data[0]);
							if(APP_Contol_Private_Frame->type[2] == '1'){
								Globa_1->App.APP_charge_kwh = atoi(&tmp[0]);
								Globa_1->App.APP_charger_type = 1;
							}else if(APP_Contol_Private_Frame->type[2] == '2'){
								Globa_1->App.APP_charge_time = atoi(&tmp[0]) + Globa_1->user_sys_time ;
								Globa_1->APP_Card_charge_time_limit = atoi(&tmp[0]);
								Globa_1->App.APP_charger_type = 2;
							}else if(APP_Contol_Private_Frame->type[2] == '3'){
								Globa_1->App.APP_charge_money = atoi(&tmp[0]);
								Globa_1->App.APP_charger_type = 3;
							}else if(APP_Contol_Private_Frame->type[2] == '4'){
								Globa_1->App.APP_charger_type = 4;
							}

							memcpy(&Globa_1->App.busy_SN[0], &APP_Contol_Private_Frame->Busy_SN[0], sizeof(Globa_1->App.busy_SN));
							memcpy(&Globa_1->App.ID[0], &APP_Contol_Private_Frame->ID[0], sizeof(Globa_1->App.ID));
							sprintf(&tmp[0], "%.8s", &APP_Contol_Private_Frame->last_rmb[0]);
							Globa_1->App.last_rmb = atoi(&tmp[0]);
							
							sprintf(tmp, "%.2s", &APP_Contol_Private_Frame->serv_type[0]);
							uitmp1 = atoi(tmp);
							sprintf(tmp, "%.8s", &APP_Contol_Private_Frame->serv_rmb[0]);
							uitmp2 = atoi(tmp);
							sprintf(tmp, "%.4s", &APP_Contol_Private_Frame->aver_rmb[0]);
							uitmp3 = atoi(tmp);
							
							Globa_1->Special_price_APP_data.Serv_Type = uitmp1;
							Globa_1->Special_price_APP_data.Serv_Price = uitmp2;
							Globa_1->Special_price_APP_data.Price = uitmp3;
							
		
							sprintf(tmp, "%.4s", &APP_Contol_Private_Frame->step_rmb[0]);
							uitmp1 = atoi(tmp);
							
							sprintf(tmp, "%.4s", &APP_Contol_Private_Frame->step_rmb[4]);
							uitmp2 = atoi(tmp);
							
							sprintf(tmp, "%.4s", &APP_Contol_Private_Frame->step_rmb[8]);
							uitmp3 = atoi(tmp);
							
							sprintf(tmp, "%.4s", &APP_Contol_Private_Frame->step_rmb[12]);
							uitmp4 = atoi(tmp);
						
							Globa_1->Special_price_APP_data.share_time_kwh_price[0] = uitmp1;  //分时电价1
							Globa_1->Special_price_APP_data.share_time_kwh_price[1] = uitmp2; //分时电价2
							Globa_1->Special_price_APP_data.share_time_kwh_price[2] = uitmp3;  //分时电价3
							Globa_1->Special_price_APP_data.share_time_kwh_price[3] = uitmp4;   //分时电价4
							
					
							sprintf(tmp, "%.2s", &APP_Contol_Private_Frame->step[0]);
							uitmp1 = atoi(tmp);
							Globa_1->Special_price_APP_data.time_zone_num = uitmp1;   //时段数
							if(ISO8583_DEBUG) printf("电价时段数： Globa_1->Special_price_APP_data.time_zone_num=%d\n", Globa_1->Special_price_APP_data.time_zone_num);

							if((uitmp1 > 0) && (uitmp1 <= 12)){//时段数1到12
								Globa_1->Special_price_APP_data.time_zone_num = uitmp1;
								for(i=0;i<Globa_1->Special_price_APP_data.time_zone_num;i++){
									uitmp2 = (APP_Contol_Private_Frame->timezone[i][0] -'0')*10+(APP_Contol_Private_Frame->timezone[i][1] -'0');
									uitmp3 = (APP_Contol_Private_Frame->timezone[i][2] -'0')*10+(APP_Contol_Private_Frame->timezone[i][3] -'0');
									if((uitmp2 < 24)  && (uitmp3 < 60)){
										Globa_1->Special_price_APP_data.time_zone_tabel[i] = (uitmp2<<8)|uitmp3;
										if(ISO8583_DEBUG) printf("电价时段数： GGloba_1->Special_price_APP_data.time_zone_tabel[%d]=%d\n",i, Globa_1->Special_price_APP_data.time_zone_tabel[i]);

									}
									uitmp4 = (APP_Contol_Private_Frame->timezone[i][4] -'0');
									if((uitmp4 >= 1)&&(uitmp4 <= 4)){
										Globa_1->Special_price_APP_data.time_zone_feilv[i] = uitmp4 -1;  //该时段对应的价格位置
										if(ISO8583_DEBUG) printf("电价时段数： GGloba_1->Special_price_APP_data.time_zone_feilv[i%d]=%d\n",i, Globa_1->Special_price_APP_data.time_zone_feilv[i]);
									}
								}
							}
							
							sprintf(tmp, "%.4s", &APP_Contol_Private_Frame->step_server_price[0]);
							uitmp1 = atoi(tmp);
							
							sprintf(tmp, "%.4s", &APP_Contol_Private_Frame->step_server_price[4]);
							uitmp2 = atoi(tmp);
							
							sprintf(tmp, "%.4s", &APP_Contol_Private_Frame->step_server_price[8]);
							uitmp3 = atoi(tmp);
							sprintf(tmp, "%.4s", &APP_Contol_Private_Frame->step_server_price[12]);
							uitmp4 = atoi(tmp);
							
							Globa_1->Special_price_APP_data.share_time_kwh_price_serve[0] = uitmp1;  //分时电价1
							Globa_1->Special_price_APP_data.share_time_kwh_price_serve[1] = uitmp2; //分时电价2
							Globa_1->Special_price_APP_data.share_time_kwh_price_serve[2] = uitmp3;  //分时电价3
							Globa_1->Special_price_APP_data.share_time_kwh_price_serve[3] = uitmp4;   //分时电价4
						//if(ret >= 0){
						//	if(flag == 0){//无异常，可以正常充电
							if(APP_Contol_Private_Frame->BMS_Power_V[1] == '2' ){
									Globa_1->BMS_Power_V = 1;
								}else{
									Globa_1->BMS_Power_V = 0;
								}
							if(APP_Contol_Private_Frame->APP_Start_Account_type[1] == '1' ){
								Globa_1->APP_Start_Account_type = 0;//1;
							}else{
								Globa_1->APP_Start_Account_type = 0;
							}
							
						    Globa_1->Special_price_APP_data_falg = 1;
								Globa_1->private_price_acquireOK = 1;//获取私有电价
								Globa_1->App.APP_start = 1;
							}
							if(DEBUGLOG) COMM_RUN_LogOut("APP下发启动充电数据:命令:%x 枪号:%x结果:%d",APP_Contol_Private_Frame->start[1],APP_Contol_Private_Frame->num[1],flag);  
							return (0);
						//}
					}else if(APP_Contol_Frame->num[1] == '2'){//第二把枪
						if(0 != memcmp(&tmp[0], &APP_Contol_Private_Frame->MD5[0], sizeof(APP_Contol_Private_Frame->MD5))){
							flag = 0x01;//01：MAC校验失败
							sendfalg = 0;
						}else if(0 != memcmp(&APP_Contol_Private_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(APP_Contol_Private_Frame->ter_sn))){
							flag = 0x02;//02：充电桩序列号不对应
							sendfalg = 0;
						}else if(Globa_2->gun_link != 1){
							flag = 0x03;//03：未插枪
							sendfalg = 0;
						}else if((Globa_2->App.APP_start == 1)&&((0 != memcmp(&Globa_2->App.ID, &APP_Contol_Frame->ID[0], sizeof(Globa_2->App.ID)))||(0 != memcmp(&Globa_2->App.busy_SN[0], &APP_Contol_Frame->Busy_SN[0], sizeof(Globa_2->App.busy_SN))))){//用户号和业务流水号不一样的时候启动不了
							flag = 0x04;//04：充电桩已被使用，无法充电
							sendfalg = 0;
						}else if((Globa_2->App.APP_start == 1)&&(0 == memcmp(&Globa_2->App.ID, &APP_Contol_Frame->ID[0], sizeof(Globa_2->App.ID)))&&(0 == memcmp(&Globa_2->App.busy_SN[0], &APP_Contol_Frame->Busy_SN[0], sizeof(Globa_2->App.busy_SN)))){//同一个账号,并且业务流水号一样则直接反馈正常
							flag = 0x00;//04：充电桩已被同一个账号启用，反馈成功
							sendfalg = 1;
						}else if((Globa_2->Error_system != 0)||(Globa_2->Electric_pile_workstart_Enable == 1)){
							flag = 0x06;//06：充电桩故障无法充电
							sendfalg = 0;
						}else if((Globa_1->QT_Step > 0x03)&&(Globa_1->Charger_param.System_Type == 1)){//轮流充电的时候有一把启动了就不能启动另一把
							flag = 0x07;//07:充电桩忙
							sendfalg = 0;
						}else if((Globa_2->QT_Step != 0x03)||(!((Globa_1->QT_Step == 0x02)||(Globa_1->QT_Step == 0x03)||(Globa_1->QT_Step == 0x201)||(Globa_1->QT_Step == 0x21)||(Globa_1->QT_Step == 0x23)||(Globa_1->QT_Step == 0x22)))){
							flag = 0x07;//07:充电桩忙
							sendfalg = 0;
						}
											
						if(ISO8583_DEBUG)printf("ISO8583_Heart_APP_Rece_Deal 启动充电22222222= %d Globa_2->QT_Step= %02x Globa_1->QT_Step= %02x\n", flag,Globa_2->QT_Step,Globa_1->QT_Step);
						 memcpy(&ISO8583_App_Struct.App_ID_2[0], &APP_Contol_Private_Frame->ID[0], sizeof(ISO8583_App_Struct.App_ID_2));
						AP_APP_Sent_2_falg[1] = flag; //表示接收到密钥到反馈信息
						if((flag == 0)&&(sendfalg == 0)){
						AP_APP_Sent_2_falg[0] = 2; //表示接收到密钥到反馈信息
						}else{
							AP_APP_Sent_2_falg[0] = 1;
						}
							
						if((flag == 0)&&(Globa_2->App.APP_start == 0)){//无异常，可以正常充电
							sprintf(&tmp[0], "%.8s", &APP_Contol_Private_Frame->data[0]);
							if(APP_Contol_Private_Frame->type[2] == '1'){
								Globa_2->App.APP_charge_kwh = atoi(&tmp[0]);
								Globa_2->App.APP_charger_type = 1;
							}else if(APP_Contol_Private_Frame->type[2] == '2'){
								Globa_2->App.APP_charge_time = atoi(&tmp[0]) + Globa_2->user_sys_time ;
								Globa_2->App.APP_charger_type = 2;
								Globa_2->APP_Card_charge_time_limit = atoi(&tmp[0]);
							}else if(APP_Contol_Private_Frame->type[2] == '3'){
								Globa_2->App.APP_charge_money = atoi(&tmp[0]);
								Globa_2->App.APP_charger_type = 3;
							}else if(APP_Contol_Private_Frame->type[2] == '4'){
								Globa_2->App.APP_charger_type = 4;
							}

							memcpy(&Globa_2->App.busy_SN[0], &APP_Contol_Private_Frame->Busy_SN[0], sizeof(Globa_2->App.busy_SN));
							memcpy(&Globa_2->App.ID[0], &APP_Contol_Private_Frame->ID[0], sizeof(Globa_2->App.ID));
							sprintf(&tmp[0], "%.8s", &APP_Contol_Private_Frame->last_rmb[0]);
							Globa_2->App.last_rmb = atoi(&tmp[0]);
							
							sprintf(tmp, "%.2s", &APP_Contol_Private_Frame->serv_type[0]);
							uitmp1 = atoi(tmp);
							sprintf(tmp, "%.8s", &APP_Contol_Private_Frame->serv_rmb[0]);
							uitmp2 = atoi(tmp);
							sprintf(tmp, "%.4s", &APP_Contol_Private_Frame->aver_rmb[0]);
							uitmp3 = atoi(tmp);
							
							Globa_2->Special_price_APP_data.Serv_Type = uitmp1;
							Globa_2->Special_price_APP_data.Serv_Price = uitmp2;
							Globa_2->Special_price_APP_data.Price = uitmp3;
							
		
							sprintf(tmp, "%.4s", &APP_Contol_Private_Frame->step_rmb[0]);
							uitmp1 = atoi(tmp);
							
							sprintf(tmp, "%.4s", &APP_Contol_Private_Frame->step_rmb[4]);
							uitmp2 = atoi(tmp);
							
							sprintf(tmp, "%.4s", &APP_Contol_Private_Frame->step_rmb[8]);
							uitmp3 = atoi(tmp);
							
							sprintf(tmp, "%.4s", &APP_Contol_Private_Frame->step_rmb[12]);
							uitmp4 = atoi(tmp);
						
							Globa_2->Special_price_APP_data.share_time_kwh_price[0] = uitmp1;  //分时电价1
							Globa_2->Special_price_APP_data.share_time_kwh_price[1] = uitmp2; //分时电价2
							Globa_2->Special_price_APP_data.share_time_kwh_price[2] = uitmp3;  //分时电价3
							Globa_2->Special_price_APP_data.share_time_kwh_price[3] = uitmp4;   //分时电价4
							
					
							sprintf(tmp, "%.2s", &APP_Contol_Private_Frame->step[0]);
							uitmp1 = atoi(tmp);
							Globa_2->Special_price_APP_data.time_zone_num = uitmp1;   //时段数
								if(ISO8583_DEBUG) printf("电价时段数： Globa_2->Special_price_APP_data.time_zone_num=%d\n", Globa_2->Special_price_APP_data.time_zone_num);

							if((uitmp1 > 0) && (uitmp1 <= 12)){//时段数1到12
								Globa_2->Special_price_APP_data.time_zone_num = uitmp1;
								for(i=0;i<Globa_2->Special_price_APP_data.time_zone_num;i++){
									uitmp2 = (APP_Contol_Private_Frame->timezone[i][0] -'0')*10+(APP_Contol_Private_Frame->timezone[i][1] -'0');
									uitmp3 = (APP_Contol_Private_Frame->timezone[i][2] -'0')*10+(APP_Contol_Private_Frame->timezone[i][3] -'0');
									if((uitmp2 < 24)  && (uitmp3 < 60)){
										Globa_2->Special_price_APP_data.time_zone_tabel[i] = (uitmp2<<8)|uitmp3;
												if(ISO8583_DEBUG)printf("电价时段数： GGloba_1->Special_price_APP_data.time_zone_tabel[%d]=%d\n",i, Globa_1->Special_price_APP_data.time_zone_tabel[i]);

									}
									uitmp4 = (APP_Contol_Private_Frame->timezone[i][4] -'0');
									if((uitmp4 >= 1)&&(uitmp4 <= 4)){
										Globa_2->Special_price_APP_data.time_zone_feilv[i] = uitmp4 -1;  //该时段对应的价格位置
												if(ISO8583_DEBUG)printf("电价时段数： GGloba_1->Special_price_APP_data.time_zone_feilv[i%d]=%d\n",i, Globa_1->Special_price_APP_data.time_zone_feilv[i]);
									}
								}
							}
							
							sprintf(tmp, "%.4s", &APP_Contol_Private_Frame->step_server_price[0]);
							uitmp1 = atoi(tmp);
							
							sprintf(tmp, "%.4s", &APP_Contol_Private_Frame->step_server_price[4]);
							uitmp2 = atoi(tmp);
							
							sprintf(tmp, "%.4s", &APP_Contol_Private_Frame->step_server_price[8]);
							uitmp3 = atoi(tmp);
							sprintf(tmp, "%.4s", &APP_Contol_Private_Frame->step_server_price[12]);
							uitmp4 = atoi(tmp);
							
							Globa_2->Special_price_APP_data.share_time_kwh_price_serve[0] = uitmp1;  //分时电价1
							Globa_2->Special_price_APP_data.share_time_kwh_price_serve[1] = uitmp2; //分时电价2
							Globa_2->Special_price_APP_data.share_time_kwh_price_serve[2] = uitmp3;  //分时电价3
							Globa_2->Special_price_APP_data.share_time_kwh_price_serve[3] = uitmp4;   //分时电价4

							//ret = ISO8583_AP_APP_Sent(fd, flag,1);
						//	AP_APP_Sent_2_falg[0] = 1; //表示接收到密钥到反馈信息
						//	AP_APP_Sent_2_falg[1] = flag; //表示接收到密钥到反馈信息
					//	if(ret >= 0){
						//if(flag == 0){//无异常，可以正常充电
							if(APP_Contol_Private_Frame->BMS_Power_V[1] == '2' ){
								Globa_2->BMS_Power_V = 1;
							}else{
								Globa_2->BMS_Power_V = 0;
							}
							if(APP_Contol_Private_Frame->APP_Start_Account_type[1] == '1' ){
								Globa_2->APP_Start_Account_type = 0;//1;
							}else{
								Globa_2->APP_Start_Account_type = 0;
							}
							Globa_2->Special_price_APP_data_falg = 1;
							Globa_2->private_price_acquireOK = 1;//获取私有电价
							Globa_2->App.APP_start = 1;
						}
						if(DEBUGLOG) COMM_RUN_LogOut("APP下发启动充电数据:命令:%x 枪号:%x 结果:%d",APP_Contol_Private_Frame->start[1],APP_Contol_Private_Frame->num[1],flag);  
						return (0);
					//}
					}
				}else if((APP_Contol_Private_Frame->start[0] == '0')&&(APP_Contol_Private_Frame->start[1] == '2')){//停止充电
					if(0 != memcmp(&tmp[0], &APP_Contol_Private_Frame->MD5[0], sizeof(APP_Contol_Private_Frame->MD5))){
						flag = 0x01;//01：MAC校验失败
						if((APP_Contol_Private_Frame->num[1] == '2'))
						{
							flag_gun = 2;
						}else{
							flag_gun = 1;
						}
					}else if(0 != memcmp(&APP_Contol_Private_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(APP_Contol_Private_Frame->ter_sn))){
						flag = 0x02;//02：充电桩序列号不对应
						if((APP_Contol_Private_Frame->num[1] == '2'))
						{
							flag_gun = 2;
						}else{
							flag_gun = 1;
						}
					}else if((0 == memcmp(&APP_Contol_Private_Frame->ID[0], &Globa_1->App.ID[0], sizeof(APP_Contol_Private_Frame->ID)))&&(APP_Contol_Private_Frame->num[1] == '1')){
						flag = 0;
						flag_gun = 1;
					}else if((0 == memcmp(&APP_Contol_Private_Frame->ID[0], &Globa_2->App.ID[0], sizeof(APP_Contol_Private_Frame->ID)))&&(APP_Contol_Private_Frame->num[1] == '2')){
						flag = 0;
						flag_gun = 2;
					}else{
						flag = 0x05;//05：非当前用户，无法取消充电
						if((APP_Contol_Private_Frame->num[1] == '2'))
						{
							flag_gun = 2;
						}else{
							flag_gun = 1;
						}
					}

					if(flag_gun == 2){
						memcpy(&ISO8583_App_Struct.App_ID_2[0], &APP_Contol_Private_Frame->ID[0], sizeof(ISO8583_App_Struct.App_ID_2));
						AP_APP_Sent_2_falg[1] = flag; //表示接收到密钥到反馈信息
						AP_APP_Sent_2_falg[0] = 1; //表示接收到密钥到反馈信息

					}else{
					  memcpy(&ISO8583_App_Struct.App_ID_1[0], &APP_Contol_Private_Frame->ID[0], sizeof(ISO8583_App_Struct.App_ID_1));
						AP_APP_Sent_1_falg[1] = flag; //表示接收到密钥到反馈信息
						AP_APP_Sent_1_falg[0] = 1; //表示接收到密钥到反馈信息
					}
					
					if((Globa_1->App.APP_start == 1)&&(flag == 0)&&(flag_gun == 1)){//已经开启充电
						Globa_1->App.APP_start = 0;
						if(ISO8583_DEBUG)printf("停止充电 xxxxxxxx111111 = %d\n", flag);
					}else if((Globa_2->App.APP_start == 1)&&(flag == 0)&&(flag_gun == 2)){//已经开启充电
						Globa_2->App.APP_start = 0;
						if(ISO8583_DEBUG)printf("停止充电 xxxxxxxx2222222222 = %d\n", flag);
					}
					if(DEBUGLOG) COMM_RUN_LogOut("APP下发停止充电数据:命令:%x 枪号:%x  结果：%d",APP_Contol_Private_Frame->start[1],APP_Contol_Private_Frame->num[1],flag);  
					return (0);
				}else{//非法指令，不处理，直接丢弃，不当做连接有问题而关闭
					return (0);
				}
			}
	  }else{ //MAC 校验码
			return (0);
		}
	}
	return (-1);
}

//卡片上位机验证处理
int ISO8583_User_Card_Sent_Deal(int fd)
{
	unsigned int ret,len;
	unsigned char SendBuf[512],tmp[100];  //发送数据区指针
	unsigned char MD5[8];  //发送数据区指针
  time_t systime=0;
	struct tm tm_t;
	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_User_Card_Frame *AP_Frame;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	AP_Frame = (ISO8583_User_Card_Frame *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据初始化
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'

	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_User_Card_Frame))>>8;  //报文长度
	Head_Frame->SessionLen[1] = sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_User_Card_Frame);
	memcpy(&Head_Frame->MessgTypeIdent[0], "1120", sizeof(Head_Frame->MessgTypeIdent));//消息标识符

	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化位域内容为 0x00
	Head_Frame->PrimaryBitmap[2/8 -0] |= 0x80 >> ((2-1)%8);//主位元表 9
	Head_Frame->PrimaryBitmap[42/8 -0] |= 0x80 >> ((42-1)%8);//主位元表 39
	
	Head_Frame->PrimaryBitmap[43/8 -0] |= 0x80 >> ((43-1)%8);//主位元表 43
	Head_Frame->PrimaryBitmap[44/8 -0] |= 0x80 >> ((44-1)%8);//主位元表 44
	Head_Frame->PrimaryBitmap[45/8 -0] |= 0x80 >> ((45-1)%8);//主位元表 45
  
	Head_Frame->PrimaryBitmap[62/8 -0] |= 0x80 >> ((62-1)%8);//主位元表 41
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63
  

	if(Globa_1->User_Card_check_flag == 1){
	  memcpy(&AP_Frame->User_Card[0], &Globa_1->User_Card_check_card_ID[0], sizeof(AP_Frame->User_Card));
		if(Globa_1->qiyehao_flag == 1){
		  AP_Frame->qiyehao_flag[1] = 'A';
			AP_Frame->qiyehao_flag[0] = 'A';
		}else{
		  AP_Frame->qiyehao_flag[0] = '5';
		  AP_Frame->qiyehao_flag[1] = '5';
		}
	  if(Globa_1->private_price_flag == 1){
		  AP_Frame->private_price_flag[0] = 'A';
		  AP_Frame->private_price_flag[1] = 'A';
		}else{
		  AP_Frame->private_price_flag[0] = '5';
		  AP_Frame->private_price_flag[1] = '5';
		}
	  if(Globa_1->order_chg_flag == 1){
			AP_Frame->order_chg_flag[0] = 'A';
		  AP_Frame->order_chg_flag[1] = 'A';
		}else{
		  AP_Frame->order_chg_flag[0] = '5';
	  	AP_Frame->order_chg_flag[1] = '5';
	  }
	}else if(Globa_2->User_Card_check_flag == 1){
	  memcpy(&AP_Frame->User_Card[0], &Globa_2->User_Card_check_card_ID[0], sizeof(AP_Frame->User_Card));
		if(Globa_2->qiyehao_flag == 1){
		  AP_Frame->qiyehao_flag[1] = 'A';
			AP_Frame->qiyehao_flag[0] = 'A';
		}else{
		  AP_Frame->qiyehao_flag[0] = '5';
		  AP_Frame->qiyehao_flag[1] = '5';
		}
	  if(Globa_2->private_price_flag == 1){
		  AP_Frame->private_price_flag[0] = 'A';
		  AP_Frame->private_price_flag[1] = 'A';
		}else{
		  AP_Frame->private_price_flag[0] = '5';
		  AP_Frame->private_price_flag[1] = '5';
		}
	  if(Globa_2->order_chg_flag == 1){
			AP_Frame->order_chg_flag[0] = 'A';
		  AP_Frame->order_chg_flag[1] = 'A';
		}else{
		  AP_Frame->order_chg_flag[0] = '5';
	  	AP_Frame->order_chg_flag[1] = '5';
	  }
	}
	memcpy(&AP_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(AP_Frame->ter_sn));//充电终端序列号    41	N	n12
	systime = time(NULL);        //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
	sprintf(&tmp[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
	memcpy(&AP_Frame->time[0], &tmp[0], sizeof(AP_Frame->time));//终端时间（格式yyyyMMddHHmmss）	62	N	N14

	MD5_DES(&AP_Frame->User_Card[0], sizeof(ISO8583_User_Card_Frame)-sizeof(AP_Frame->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&AP_Frame->MD5[0], &MD5[0], 8);
	len = ISO8583_Sent_Data(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_User_Card_Frame)), 0);

	if(ISO8583_DEBUG) printf("发送卡片鉴权%d %d %d\n",len,Globa_1->User_Card_check_flag,Globa_2->User_Card_check_flag);
	
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_User_Card_Frame))){//如果发送不成功
		return (-1);
	}else{
		COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}


//卡片验证接收处理函数
int ISO8583_CARD_1121_Rece_Deal(int length, unsigned char *buf)
{
	int j,i,uitmp1,uitmp2,uitmp3,uitmp4;
	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针
  unsigned char User_Card_check_result = 0;
	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_User_Card_Resp_Frame *User_Card_Resp_Frame;
	
	ISO8583_AP1120_Resp2_Frame* ISO8583_AP1120_Resp;
	ISO8583_AP1120_Resp_full_Frame* p1121_Frame;

	Head_Frame = (ISO8583_Head_Frame *)(buf);
	
	User_Card_Resp_Frame = (ISO8583_User_Card_Resp_Frame *)(buf+sizeof(ISO8583_Head_Frame));
	ISO8583_AP1120_Resp = (ISO8583_AP1120_Resp2_Frame *)(buf+sizeof(ISO8583_Head_Frame));
	p1121_Frame = (ISO8583_AP1120_Resp_full_Frame *)(buf+sizeof(ISO8583_Head_Frame));
	//if(DEBUGLOG) GLOBA_RUN_LogOut("%s %s %s","APP下发启动停止充电数据",Head_Frame,APP_Contol_Frame);  
	if(length >= (sizeof(ISO8583_Head_Frame) + sizeof(User_Card_Resp_Frame->MD5))){
		MD5_DES(&buf[sizeof(ISO8583_Head_Frame)], (length - sizeof(ISO8583_Head_Frame)-sizeof(User_Card_Resp_Frame->MD5)), &ISO8583_App_Struct.work_key[0], &MD5[0]);
		hex_to_str(&tmp[0], &MD5[0], 8);

	  if(0 == memcmp(&tmp[0], &buf[length - sizeof(User_Card_Resp_Frame->MD5)], sizeof(User_Card_Resp_Frame->MD5))){//MAC校验
	  // 	printf("length - sizeof(User_Card_Resp_Frame->MD5) - 1 %d  =  %d \n",length - sizeof(User_Card_Resp_Frame->MD5) - 1,buf[length - sizeof(User_Card_Resp_Frame->MD5) - 1]);

		 if((buf[length - sizeof(User_Card_Resp_Frame->MD5) - 1] ==  '0')&&(buf[length - sizeof(User_Card_Resp_Frame->MD5) - 2] ==  '0')){//成功
				User_Card_check_result = 1;
			}else if((buf[length - sizeof(User_Card_Resp_Frame->MD5) - 1]==  '1')&&(buf[length - sizeof(User_Card_Resp_Frame->MD5) - 2]  ==  '0')){//MAC校验错误
				User_Card_check_result = 2;
			}else if((buf[length - sizeof(User_Card_Resp_Frame->MD5) - 1] ==  '2')&&(buf[length - sizeof(User_Card_Resp_Frame->MD5) - 2] ==  '0')){//终端未对时
				User_Card_check_result = 3;
			}else if((buf[length - sizeof(User_Card_Resp_Frame->MD5) - 1] ==  '3')&&(buf[length - sizeof(User_Card_Resp_Frame->MD5)- 2] ==  '0')){//该卡未进行发卡
				User_Card_check_result = 4;
			}else if((buf[length - sizeof(User_Card_Resp_Frame->MD5) - 1]==  '4')&&(buf[length - sizeof(User_Card_Resp_Frame->MD5)- 2] ==  '0')){//无效充电桩序列号
				User_Card_check_result = 5;
			}else if((buf[length - sizeof(User_Card_Resp_Frame->MD5) - 1] ==  '5')&&(buf[length - sizeof(User_Card_Resp_Frame->MD5)- 2] ==  '0')){//黑名单
				User_Card_check_result = 6;
			}else if((buf[length - sizeof(User_Card_Resp_Frame->MD5) - 1] ==  '6')&&(buf[length - sizeof(User_Card_Resp_Frame->MD5)- 2] ==  '0')){//卡的企业号标记不同步
				User_Card_check_result = 7;
			}else if((buf[length - sizeof(User_Card_Resp_Frame->MD5) - 1] ==  '7')&&(buf[length - sizeof(User_Card_Resp_Frame->MD5)- 2] ==  '0')){//卡的私有电价标记不同步
				User_Card_check_result = 8;
			}else if((buf[length - sizeof(User_Card_Resp_Frame->MD5) - 1] ==  '8')&&(buf[length - sizeof(User_Card_Resp_Frame->MD5)- 2] ==  '0')){// 卡的有序充电标记不同步
				User_Card_check_result = 9;
			}else if((buf[length - sizeof(User_Card_Resp_Frame->MD5) - 1] ==  '9')&&(buf[length - sizeof(User_Card_Resp_Frame->MD5)- 2] ==  '0')){//非规定时间充电卡
				User_Card_check_result = 10;
			}else if((buf[length - sizeof(User_Card_Resp_Frame->MD5) - 1] ==  '0')&&(buf[length - sizeof(User_Card_Resp_Frame->MD5)- 2] ==  '1')){//企业号余额不足
				User_Card_check_result = 11;
			}else if((buf[length - sizeof(User_Card_Resp_Frame->MD5) - 1] ==  '1')&&(buf[length - sizeof(User_Card_Resp_Frame->MD5)- 2] ==  '1')){//专用时间其他卡不可充
				User_Card_check_result = 12;
			}else if((buf[length - sizeof(User_Card_Resp_Frame->MD5) - 1] ==  '2')&&(buf[length - sizeof(User_Card_Resp_Frame->MD5)- 2] ==  '1')){//充电启动次数过多
				User_Card_check_result = 13;
			}else if((buf[length - sizeof(User_Card_Resp_Frame->MD5) - 1] ==  '3')&&(buf[length - sizeof(User_Card_Resp_Frame->MD5)- 2] ==  '1')){//VIN 不属于该代理商
				User_Card_check_result = 14;
			}
			if( 0x01 == User_Card_check_result){
				if(length == (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP1120_Resp_full_Frame))){					
					//g_ISO8583_Sever_OK = 1;
					//解析电价数据
						sprintf(tmp, "%.2s", &p1121_Frame->serv_type[0]);
						uitmp1 = atoi(tmp);
						sprintf(tmp, "%.8s", &p1121_Frame->serv_rmb[0]);
						uitmp2 = atoi(tmp);
						sprintf(tmp, "%.4s", &p1121_Frame->aver_rmb[0]);
						uitmp3 = atoi(tmp);
					
					if(Globa_1->User_Card_check_flag == 1){
						Globa_1->Special_price_data.Serv_Type = uitmp1;
						Globa_1->Special_price_data.Serv_Price = uitmp2;
						Globa_1->Special_price_data.Price = uitmp3; //专有电价
					}else if(Globa_2->User_Card_check_flag == 1){
						Globa_2->Special_price_data.Serv_Type = uitmp1;
						Globa_2->Special_price_data.Serv_Price = uitmp2;
						Globa_2->Special_price_data.Price = uitmp3;
					}
				
					sprintf(tmp, "%.4s", &p1121_Frame->kwh_price1[0]);
					uitmp1 = atoi(tmp);
					
					sprintf(tmp, "%.4s", &p1121_Frame->kwh_price2[0]);
					uitmp2 = atoi(tmp);
					
					sprintf(tmp, "%.4s", &p1121_Frame->kwh_price3[0]);
					uitmp3 = atoi(tmp);
					
					sprintf(tmp, "%.4s", &p1121_Frame->kwh_price4[0]);
					uitmp4 = atoi(tmp);
				
					if(Globa_1->User_Card_check_flag == 1){
						Globa_1->Special_price_data.share_time_kwh_price[0] = uitmp1;  //分时电价1
						Globa_1->Special_price_data.share_time_kwh_price[1] = uitmp2; //分时电价2
						Globa_1->Special_price_data.share_time_kwh_price[2] = uitmp3;  //分时电价3
						Globa_1->Special_price_data.share_time_kwh_price[3] = uitmp4;   //分时电价4
						
						sprintf(tmp, "%.2s", &p1121_Frame->timezone_num[0]);
						uitmp1 = atoi(tmp);
						if((uitmp1 > 0) && (uitmp1 <= 12)){//时段数1到12
							Globa_1->Special_price_data.time_zone_num = uitmp1;
							for(i=0;i<Globa_1->Special_price_data.time_zone_num;i++){
								uitmp2 = (p1121_Frame->timezone[i][0] -'0')*10+(p1121_Frame->timezone[i][1] -'0');
								uitmp3 = (p1121_Frame->timezone[i][2] -'0')*10+(p1121_Frame->timezone[i][3] -'0');
								if((uitmp2 < 24)  && (uitmp3 < 60)){
									Globa_1->Special_price_data.time_zone_tabel[i] = (uitmp2<<8)|uitmp3;
								}
								uitmp4 = (p1121_Frame->timezone[i][4] -'0');
								if((uitmp4 >= 1)&&(uitmp4 <= 4)){
									Globa_1->Special_price_data.time_zone_feilv[i] = uitmp4 -1;  //该时段对应的价格位置
								}
							}
						}
					}else if(Globa_2->User_Card_check_flag == 1){
						Globa_2->Special_price_data.share_time_kwh_price[0] = uitmp1;  //分时电价1
						Globa_2->Special_price_data.share_time_kwh_price[1] = uitmp2; //分时电价2
						Globa_2->Special_price_data.share_time_kwh_price[2] = uitmp3;  //分时电价3
						Globa_2->Special_price_data.share_time_kwh_price[3] = uitmp4;   //分时电价4
						
						sprintf(tmp, "%.2s", &p1121_Frame->timezone_num[0]);
						uitmp1 = atoi(tmp);
						if((uitmp1 > 0) && (uitmp1 <= 12)){//时段数1到12
							Globa_2->Special_price_data.time_zone_num = uitmp1;
							for(i=0;i<Globa_2->Special_price_data.time_zone_num;i++){
								uitmp2 = (p1121_Frame->timezone[i][0] -'0')*10+(p1121_Frame->timezone[i][1] -'0');
								uitmp3 = (p1121_Frame->timezone[i][2] -'0')*10+(p1121_Frame->timezone[i][3] -'0');
								if((uitmp2 < 24)  && (uitmp3 < 60)){
									Globa_2->Special_price_data.time_zone_tabel[i] = (uitmp2<<8)|uitmp3;
								}
								uitmp4 = (p1121_Frame->timezone[i][4] -'0');
								if((uitmp4 >= 1)&&(uitmp4 <= 4)){
									Globa_2->Special_price_data.time_zone_feilv[i] = uitmp4 -1;  //该时段对应的价格位置
								}
							}
						}
					}
	
					
					sprintf(tmp, "%.4s", &p1121_Frame->kwh_service_price1[0]);
					uitmp1 = atoi(tmp);
					
					sprintf(tmp, "%.4s", &p1121_Frame->kwh_service_price2[0]);
					uitmp2 = atoi(tmp);
					
					sprintf(tmp, "%.4s", &p1121_Frame->kwh_service_price3[0]);
					uitmp3 = atoi(tmp);
					sprintf(tmp, "%.4s", &p1121_Frame->kwh_service_price4[0]);
					uitmp4 = atoi(tmp);
					
					if(Globa_1->User_Card_check_flag == 1){
						Globa_1->Special_price_data.share_time_kwh_price_serve[0] = uitmp1;  //分时电价1
						Globa_1->Special_price_data.share_time_kwh_price_serve[1] = uitmp2; //分时电价2
						Globa_1->Special_price_data.share_time_kwh_price_serve[2] = uitmp3;  //分时电价3
						Globa_1->Special_price_data.share_time_kwh_price_serve[3] = uitmp4;   //分时电价4
						Globa_1->private_price_acquireOK = 1;//获取私有电价
					}else if(Globa_2->User_Card_check_flag == 1){
						Globa_2->Special_price_data.share_time_kwh_price_serve[0] = uitmp1;  //分时电价1
						Globa_2->Special_price_data.share_time_kwh_price_serve[1] = uitmp2; //分时电价2
						Globa_2->Special_price_data.share_time_kwh_price_serve[2] = uitmp3;  //分时电价3
						Globa_2->Special_price_data.share_time_kwh_price_serve[3] = uitmp4;   //分时电价4
						Globa_2->private_price_acquireOK = 1;//获取私有电价
					}
			
					sprintf(tmp, "%.4s", &p1121_Frame->order_card_end_time[0]);
					uitmp4 = atoi(tmp);
					if(Globa_1->User_Card_check_flag == 1){
						Globa_1->order_card_end_time[0] = uitmp4/100;
						Globa_1->order_card_end_time[1] = uitmp4%100;
						memcpy(&Globa_1->Bind_BmsVin[0], &p1121_Frame->bind_bmsvin[0], sizeof(Globa_1->Bind_BmsVin));
					}else if(Globa_2->User_Card_check_flag == 1){
						Globa_2->order_card_end_time[0] = uitmp4/100;
						Globa_2->order_card_end_time[1] = uitmp4%100;
						memcpy(&Globa_2->Bind_BmsVin[0], &p1121_Frame->bind_bmsvin[0], sizeof(Globa_2->Bind_BmsVin));
					}
					
				/*if(9999!=uitmp4)
					{	
						if(Globa_1->User_Card_check_flag == 1){
							Globa_1->order_card_end_time[0] = uitmp4/100;
						  Globa_1->order_card_end_time[1] = uitmp4%100;
						}else if(Globa_2->User_Card_check_flag == 1){
						  Globa_2->order_card_end_time[0] = uitmp4/100;
						  Globa_2->order_card_end_time[1] = uitmp4%100;
						}
					}*/
				}else if(length == (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP1120_Resp2_Frame))){
					sprintf(tmp, "%.4s", &ISO8583_AP1120_Resp->order_card_end_time[0]);
					uitmp4 = atoi(tmp);
					/*if(9999!=uitmp4)
					{	
						if(Globa_1->User_Card_check_flag == 1){
							Globa_1->order_card_end_time[0] = uitmp4/100;
						  Globa_1->order_card_end_time[1] = uitmp4%100;
						}else if(Globa_2->User_Card_check_flag == 1){
						  Globa_2->order_card_end_time[0] = uitmp4/100;
						  Globa_2->order_card_end_time[1] = uitmp4%100;
						}
					}*/
					if(Globa_1->User_Card_check_flag == 1){
						Globa_1->order_card_end_time[0] = uitmp4/100;
						Globa_1->order_card_end_time[1] = uitmp4%100;
						memcpy(&Globa_1->Bind_BmsVin[0], &ISO8583_AP1120_Resp->bind_bmsvin[0], sizeof(Globa_1->Bind_BmsVin));
					}else if(Globa_2->User_Card_check_flag == 1){
						Globa_2->order_card_end_time[0] = uitmp4/100;
						Globa_2->order_card_end_time[1] = uitmp4%100;
						memcpy(&Globa_2->Bind_BmsVin[0], &ISO8583_AP1120_Resp->bind_bmsvin[0], sizeof(Globa_2->Bind_BmsVin));
					}
				}
			
			 	if(Globa_1->User_Card_check_flag == 1){
					if(Globa_1->Start_Mode  == VIN_START_TYPE)
					{//VIN鉴权报文
				     memcpy(Globa_1->card_sn, &buf[length - sizeof(User_Card_Resp_Frame->MD5) - 18],16);//VIN用户的卡号
					}
				}else if(Globa_2->User_Card_check_flag == 1){
					if(Globa_2->Start_Mode  == VIN_START_TYPE)
					{//VIN鉴权报文
				     memcpy(Globa_2->card_sn, &buf[length - sizeof(User_Card_Resp_Frame->MD5) - 18],16);//VIN用户的卡号
					}
				}
			}

			if(Globa_1->User_Card_check_flag == 1){
				if(ISO8583_DEBUG) 	printf("校验卡片结果：%d  %d \r\n",Globa_1->User_Card_check_result,User_Card_check_result);
				Globa_1->User_Card_check_result = User_Card_check_result;
				Globa_1->User_Card_check_flag = 0;
			}else if(Globa_2->User_Card_check_flag == 1){
				if(ISO8583_DEBUG) 	printf("校验卡片结果：%d  %d \r\n",Globa_2->User_Card_check_result,User_Card_check_result);
				Globa_2->User_Card_check_result = User_Card_check_result;
				Globa_2->User_Card_check_flag = 0;
			}
		}else{//非法指令，不处理，直接丢弃，不当做连接有问题而关闭
			return (0);
		}
	}
	return (-1);
}
//卡片上位机验证报文
int Fill_ISO8583_1120_VIN_Auth_Frame(int fd)
{
	int len,i;
	unsigned char tmp[100];  
	unsigned char MD5[8];  
	unsigned char bitmap[]={2,42,60,62,63};//需要置位的位元
	unsigned int ret;
	unsigned char SendBuf[512];  //发送数据区指针
	
	char log_buf[256];
	char VIN[18];
	time_t systime=0;
	struct tm tm_t;
	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_VIN_Auth_Frame *AP_Frame;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	AP_Frame = (ISO8583_VIN_Auth_Frame *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据初始化
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'
		
	len = sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_VIN_Auth_Frame);
	Head_Frame->SessionLen[0] = len>>8;  //报文长度
	Head_Frame->SessionLen[1] = len;
	
	memcpy(&Head_Frame->MessgTypeIdent[0], "1120", sizeof(Head_Frame->MessgTypeIdent));//消息标识符

	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化位域内容为 0x00
	for(i=0;i<sizeof(bitmap);i++)
		SetPrimaryBitmap(Head_Frame->PrimaryBitmap,bitmap[i]);	
	
	memset(&AP_Frame->User_Card[0],  '0', sizeof(AP_Frame->User_Card));
	
	if(Globa_1->User_Card_check_flag == 1){
	  memcpy(VIN, Globa_1->VIN ,17);
	}else if(Globa_2->User_Card_check_flag == 1){
	  memcpy(VIN, Globa_2->VIN ,17);
	}
	
	memcpy(&AP_Frame->VIN, VIN  ,17);
	
	memcpy(&AP_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(AP_Frame->ter_sn));//充电终端序列号    41	N	n12
	
	systime = time(NULL);        //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
	sprintf(&tmp[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
	memcpy(&AP_Frame->time[0], &tmp[0], sizeof(AP_Frame->time));//终端时间（格式yyyyMMddHHmmss）	62	N	N14

	MD5_DES(&AP_Frame->User_Card[0], sizeof(ISO8583_VIN_Auth_Frame)-sizeof(AP_Frame->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&AP_Frame->MD5[0], &MD5[0], 8);
	VIN[17] = 0;//end char
	len = ISO8583_Sent_Data(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_VIN_Auth_Frame)), 0);
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_VIN_Auth_Frame))){//如果发送不成功
		return (-1);
	}else{
		COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}



//企业充电企业号充电卡启动成功上报接口
unsigned short Fill_AP1130_Sent_Deal(int fd)
{
	unsigned int ret,len;
	unsigned char SendBuf[512],tmp[100];  //发送数据区指针
	unsigned char MD5[8];  //发送数据区指针
  time_t systime=0;
	struct tm tm_t;
	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_AP1130_Frame *AP_Frame;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	AP_Frame = (ISO8583_AP1130_Frame *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据初始化
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'

	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP1130_Frame))>>8;  //报文长度
	Head_Frame->SessionLen[1] = sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP1130_Frame);
	memcpy(&Head_Frame->MessgTypeIdent[0], "1130", sizeof(Head_Frame->MessgTypeIdent));//消息标识符

	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化位域内容为 0x00
	Head_Frame->PrimaryBitmap[2/8 -0] |= 0x80 >> ((2-1)%8);//主位元表2
	Head_Frame->PrimaryBitmap[38/8 -0] |= 0x80 >> ((38-1)%8);//主位元表 38
	Head_Frame->PrimaryBitmap[42/8 -0] |= 0x80 >> ((42-1)%8);//主位元表 39
	Head_Frame->PrimaryBitmap[62/8 -0] |= 0x80 >> ((62-1)%8);//主位元表 41
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63
	
	if(Globa_1->Enterprise_Card_flag == 1){
	 // memcpy(&AP_Frame->User_Card[0], &Globa_1->User_Card_check_card_ID[0], sizeof(AP_Frame->User_Card));
		memcpy(&AP_Frame->User_Card[0], &Globa_1->card_sn[0], sizeof(AP_Frame->User_Card));
		AP_Frame->plug_id[0] = '0';
		AP_Frame->plug_id[1] = '1';
	}else if(Globa_2->Enterprise_Card_flag == 1){
	//  memcpy(&AP_Frame->User_Card[0], &Globa_2->User_Card_check_card_ID[0], sizeof(AP_Frame->User_Card));
	  memcpy(&AP_Frame->User_Card[0], &Globa_2->card_sn[0], sizeof(AP_Frame->User_Card));
		AP_Frame->plug_id[0] = '0';
		AP_Frame->plug_id[1] = '2';
	}
	memcpy(&AP_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(AP_Frame->ter_sn));//充电终端序列号    41	N	n12

	systime = time(NULL);        //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
	sprintf(&tmp[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
	memcpy(&AP_Frame->time[0], &tmp[0], sizeof(AP_Frame->time));//终端时间（格式yyyyMMddHHmmss）	62	N	N14

	MD5_DES(&AP_Frame->User_Card[0], sizeof(ISO8583_AP1130_Frame)-sizeof(AP_Frame->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&AP_Frame->MD5[0], &MD5[0], 8);
	len = ISO8583_Sent_Data(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP1130_Frame)), 0);
	if(ISO8583_DEBUG) printf("企业充电企业号充电卡启动成功上报接口\r\n");

	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP1130_Frame))){//如果发送不成功
		return (-1);
	}else{
		COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}


//请求黑名单   发送上行数据
int ISO8583_AP_req_card_sent(int fd, int num)
{
	unsigned int ret,len;
	unsigned char SendBuf[512];  //发送数据区指针
	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针

	time_t systime=0;
	struct tm tm_t;

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_AP_Get_INVAL_CARD_Frame *AP_Frame;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	AP_Frame = (ISO8583_AP_Get_INVAL_CARD_Frame *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据初始化
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'

	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Get_INVAL_CARD_Frame))>>8;  //报文长度
	Head_Frame->SessionLen[1] = sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Get_INVAL_CARD_Frame);
	memcpy(&Head_Frame->MessgTypeIdent[0], "1030", sizeof(Head_Frame->MessgTypeIdent));//消息标识符

	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化帧数据内容为 0x00

	Head_Frame->PrimaryBitmap[11/8 -0] |= 0x80 >> ((11-1)%8);//主位元表 11
	Head_Frame->PrimaryBitmap[41/8 -0] |= 0x80 >> ((41-1)%8);//主位元表 41
	Head_Frame->PrimaryBitmap[52/8 -0] |= 0x80 >> ((52-1)%8);//主位元表 52
	Head_Frame->PrimaryBitmap[62/8 -0] |= 0x80 >> ((62-1)%8);//主位元表 62
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63

	AP_Frame->sn[0] = (num/100)%10 + '0';//请求数据包序列号
	AP_Frame->sn[1] = (num/10)%10 + '0';
	AP_Frame->sn[2] = num%10 + '0';

	memcpy(&AP_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(AP_Frame->ter_sn));//充电终端序列号
	memcpy(&AP_Frame->ver[0], &Globa_1->tmp_card_ver[0], sizeof(AP_Frame->ver));//黑名单数据版本号

	systime = time(NULL);        //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
	sprintf(&tmp[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
	memcpy(&AP_Frame->time[0], &tmp[0], sizeof(AP_Frame->time));//终端时间（格式yyyyMMddHHmmss）	62	N	N14

	MD5_DES(&AP_Frame->sn[0], sizeof(ISO8583_AP_Get_INVAL_CARD_Frame)-sizeof(AP_Frame->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&AP_Frame->MD5[0], &MD5[0], 8);

	len = ISO8583_Sent_Data(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Get_INVAL_CARD_Frame)), 0);
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Get_INVAL_CARD_Frame))){//如果发送不成功
		return (-1);
	}else{
		COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}


//黑名单数据更新
int ISO8583_have_card_change_Deal(unsigned int length,unsigned char *buf)
{	
  unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针
	int count,total,bag_now;
	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_AP_Get_INVAL_CARD_Resp_Frame *AP_Resp_Frame;

	Head_Frame = (ISO8583_Head_Frame *)(buf);
	AP_Resp_Frame = (ISO8583_AP_Get_INVAL_CARD_Resp_Frame *)(buf+sizeof(ISO8583_Head_Frame));
 
  MD5_DES(&AP_Resp_Frame->now_sn[0], (length - sizeof(ISO8583_Head_Frame)-sizeof(AP_Resp_Frame->MD5)), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&tmp[0], &MD5[0], 8);
	if(ISO8583_DEBUG) printf("黑名单数据更新 1%d \n",length);
	//if(0 == memcmp(&tmp[0], &AP_Resp_Frame->MD5[0], sizeof(AP_Resp_Frame->MD5))){
		if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "1031", sizeof(Head_Frame->MessgTypeIdent))){
		  if(ISO8583_DEBUG)	printf("黑名单数据更新 3\n");
		  if((AP_Resp_Frame->flag[0] == '0')&&(AP_Resp_Frame->flag[1] == '0')){
				total = (AP_Resp_Frame->tot_sn[0]-0x30)*100 + (AP_Resp_Frame->tot_sn[1]-0x30)*10 + AP_Resp_Frame->tot_sn[2]-0x30;
					if(ISO8583_DEBUG)	printf("黑名单数据更新 3 total= %d\n",total);

   			if(total > 0){
					count = ((AP_Resp_Frame->data_len[0]-0x30)*100 + (AP_Resp_Frame->data_len[1]-0x30)*10 + \
									AP_Resp_Frame->data_len[2]-0x30)/16;
					//先删除原有的黑名单记录，再添加新的记录
							if(ISO8583_DEBUG)printf("黑名单数据更新 4 count= %d\n",count);

					bag_now = (AP_Resp_Frame->now_sn[0]-0x30)*100 + (AP_Resp_Frame->now_sn[1]-0x30)*10 + AP_Resp_Frame->now_sn[2]-0x30; 
					if(bag_now == 1){
						 Delete_card_DB();
					}
					if(ISO8583_DEBUG)printf("黑名单数据更新 5 bag_now= %d\n",bag_now);

					if(ISO8583_App_Struct.invalid_card_bag_now == bag_now){
						Insert_card_DB(&AP_Resp_Frame->data[0], count);//同时插入count个黑名单卡号
						if(ISO8583_App_Struct.invalid_card_bag_now == total ){//表示全部接受完毕
							memcpy(&Globa_1->Charger_param2.invalid_card_ver[0], &Globa_1->tmp_card_ver[0], sizeof(Globa_1->tmp_card_ver));//黑名单数据版本号
							System_param2_save(); 
							Globa_1->have_card_change = 0;
							ISO8583_App_Struct.invalid_card_bag_now = 1;
							ISO8583_App_Struct.invalid_card_Rcev_falg = 0;
							return (0);
						}
						ISO8583_App_Struct.invalid_card_bag_now++;
						ISO8583_App_Struct.invalid_card_Rcev_falg = 1;
						return (0);
					}
				}else{
					memcpy(&Globa_1->Charger_param2.invalid_card_ver[0], &Globa_1->tmp_card_ver[0], sizeof(Globa_1->tmp_card_ver));//黑名单数据版本号
					System_param2_save(); 
					Globa_1->have_card_change = 0;
				  ISO8583_App_Struct.invalid_card_bag_now = 1;
					ISO8583_App_Struct.invalid_card_Rcev_falg = 0;
					Delete_card_DB();
					return (0);
				}
			}
		}
	//}
	return (-1);
}

//电价数据更新   发送上行数据 price_type --0--当前电价 1--定时电价
int ISO8583_AP_req_price_sent(int fd,int price_type)
{
	unsigned int ret,len;
	unsigned char SendBuf[512];  //发送数据区指针
	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针

	time_t systime=0;
	struct tm tm_t;

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_AP_Get_Price_Frame *AP_Frame;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	AP_Frame = (ISO8583_AP_Get_Price_Frame *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据初始化
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'

	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Get_Price_Frame))>>8;  //报文长度
	Head_Frame->SessionLen[1] = sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Get_Price_Frame);
	memcpy(&Head_Frame->MessgTypeIdent[0], "1040", sizeof(Head_Frame->MessgTypeIdent));//消息标识符

	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化帧数据内容为 0x00

	Head_Frame->PrimaryBitmap[41/8 -0] |= 0x80 >> ((41-1)%8);//主位元表 41
	Head_Frame->PrimaryBitmap[42/8 -0] |= 0x80 >> ((42-1)%8);//主位元表 41
	Head_Frame->PrimaryBitmap[62/8 -0] |= 0x80 >> ((62-1)%8);//主位元表 62
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63

	memcpy(&AP_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(AP_Frame->ter_sn));//充电终端序列号
  if(price_type == 1){//获取定时电价
		AP_Frame->price_type[0] = '0';
	  AP_Frame->price_type[1] = '1';
	}else{//获取当前电价
		AP_Frame->price_type[0] = '0';
	  AP_Frame->price_type[1] = '0';
	}
	systime = time(NULL);        //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
	sprintf(&tmp[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
	memcpy(&AP_Frame->time[0], &tmp[0], sizeof(AP_Frame->time));//终端时间（格式yyyyMMddHHmmss）	62	N	N14

	MD5_DES(&AP_Frame->ter_sn[0], sizeof(ISO8583_AP_Get_Price_Frame)-sizeof(AP_Frame->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&AP_Frame->MD5[0], &MD5[0], 8);

	len = ISO8583_Sent_Data(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Get_Price_Frame)), 0);
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Get_Price_Frame))){//如果发送不成功
		return (-1);
	}else{
		COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}

//电价数据更新
int ISO8583_have_price_change_Deal(unsigned int length,unsigned char *buf)
{
	int i= 0 ,j = 0;
	unsigned char tmp[100] ={0};
	unsigned char MD5[8] ={0};  //发送数据区指针
	unsigned int	free_when_charge = 0,car_park_price = 0,free_minutes_after_charge = 0;

	unsigned int uitmp1 = 0, uitmp2= 0, uitmp3= 0, uitmp4= 0;
	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_AP_Get_Price_Resp_Frame *AP_Resp_Frame;
	ISO8583_AP_Get_fixedPrice_Resp_Frame *fixedAP_Resp_Frame;
	Head_Frame = (ISO8583_Head_Frame *)(buf);
	AP_Resp_Frame = (ISO8583_AP_Get_Price_Resp_Frame *)(buf+sizeof(ISO8583_Head_Frame));
	fixedAP_Resp_Frame = (ISO8583_AP_Get_fixedPrice_Resp_Frame *)(buf+sizeof(ISO8583_Head_Frame));//定时电价
	
	if(length >= (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Get_Price_Resp_Frame))){//表明客户端有数据发送过来，作相应接受处理
		MD5_DES(&AP_Resp_Frame->serv_type[0], (length - sizeof(ISO8583_Head_Frame)-sizeof(AP_Resp_Frame->MD5)), &ISO8583_App_Struct.work_key[0], &MD5[0]);
		hex_to_str(&tmp[0], &MD5[0], 8);
		if(0 == memcmp(&tmp[0], &buf[length - sizeof(AP_Resp_Frame->MD5)], sizeof(AP_Resp_Frame->MD5))){
			if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "1041", sizeof(Head_Frame->MessgTypeIdent))){
				//if((AP_Resp_Frame->flag[0] == '0')&&(AP_Resp_Frame->flag[1] == '0')){
				if((buf[length - sizeof(AP_Resp_Frame->MD5) - 2] == '0')&&(buf[length - sizeof(AP_Resp_Frame->MD5) - 1] == '0')){ //炒作结果
					if((Globa_1->Apply_Price_type == 0)&&(length == (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Get_Price_Resp_Frame)))){//当前电价申请
						sprintf(tmp, "%.2s", &AP_Resp_Frame->serv_type[0]);
						uitmp1 = atoi(tmp);
						sprintf(tmp, "%.8s", &AP_Resp_Frame->serv_rmb[0]);
						uitmp2 = atoi(tmp);
						sprintf(tmp, "%.4s", &AP_Resp_Frame->aver_rmb[0]);
						uitmp3 = atoi(tmp);
						
						Globa_1->Charger_param.Serv_Type = uitmp1;
						Globa_1->Charger_param.Serv_Price = uitmp2;
						Globa_1->Charger_param.Price = uitmp3;
						
						Globa_2->Charger_param.Serv_Type = uitmp1;
						Globa_2->Charger_param.Serv_Price = uitmp2;
						Globa_2->Charger_param.Price = uitmp3;
						sprintf(tmp, "%.4s", &AP_Resp_Frame->step_rmb[0]);
						uitmp1 = atoi(tmp);
						
						sprintf(tmp, "%.4s", &AP_Resp_Frame->step_rmb[4]);
						uitmp2 = atoi(tmp);
						
						sprintf(tmp, "%.4s", &AP_Resp_Frame->step_rmb[8]);
						uitmp3 = atoi(tmp);
						
						sprintf(tmp, "%.4s", &AP_Resp_Frame->step_rmb[12]);
						uitmp4 = atoi(tmp);
					
						Globa_1->Charger_param2.share_time_kwh_price[0] = uitmp1;  //分时电价1
						Globa_1->Charger_param2.share_time_kwh_price[1] = uitmp2; //分时电价2
						Globa_1->Charger_param2.share_time_kwh_price[2] = uitmp3;  //分时电价3
						Globa_1->Charger_param2.share_time_kwh_price[3] = uitmp4;   //分时电价4
						
						Globa_2->Charger_param2.share_time_kwh_price[0] = uitmp1;  //分时电价1
						Globa_2->Charger_param2.share_time_kwh_price[1] = uitmp2; //分时电价2
						Globa_2->Charger_param2.share_time_kwh_price[2] = uitmp3;  //分时电价3
						Globa_2->Charger_param2.share_time_kwh_price[3] = uitmp4;   //分时电价4
						sprintf(tmp, "%.2s", &AP_Resp_Frame->step[0]);
						uitmp1 = atoi(tmp);
						Globa_1->Charger_param2.time_zone_num = uitmp1;   //时段数
						Globa_2->Charger_param2.time_zone_num = uitmp1;   //时段数
			   		if(ISO8583_DEBUG)  printf("电价时段数： Globa_1->Charger_param2.time_zone_num=%d\n", Globa_1->Charger_param2.time_zone_num);

						if((uitmp1 > 0) && (uitmp1 <= 12)){//时段数1到12
							Globa_1->Charger_param2.time_zone_num = uitmp1;
							for(i=0;i<Globa_1->Charger_param2.time_zone_num;i++){
								uitmp2 = (AP_Resp_Frame->timezone[i][0] -'0')*10+(AP_Resp_Frame->timezone[i][1] -'0');
								uitmp3 = (AP_Resp_Frame->timezone[i][2] -'0')*10+(AP_Resp_Frame->timezone[i][3] -'0');
								if((uitmp2 < 24)  && (uitmp3 < 60)){
									Globa_1->Charger_param2.time_zone_tabel[i] = (uitmp2<<8)|uitmp3;
									Globa_2->Charger_param2.time_zone_tabel[i] = (uitmp2<<8)|uitmp3;
										if(ISO8583_DEBUG)	printf("电价时段数： GGloba_1->Charger_param2.time_zone_tabel[%d]=%d\n",i, Globa_1->Charger_param2.time_zone_tabel[i]);

								}
								uitmp4 = (AP_Resp_Frame->timezone[i][4] -'0');
								if((uitmp4 >= 1)&&(uitmp4 <= 4)){
									Globa_1->Charger_param2.time_zone_feilv[i] = uitmp4 -1;  //该时段对应的价格位置
									Globa_2->Charger_param2.time_zone_feilv[i] = uitmp4 -1;
									if(ISO8583_DEBUG)		printf("电价时段数： GGloba_1->Charger_param2.time_zone_feilv[i%d]=%d\n",i, Globa_1->Charger_param2.time_zone_feilv[i]);
								}
							}
						}

						sprintf(tmp, "%.8s", &AP_Resp_Frame->stop_rmb[0]);
						car_park_price = atoi(tmp);
						
						//sprintf(tmp, "%.2s", &AP_Resp_Frame->stop_rmb_start[0]);
						if((AP_Resp_Frame->stop_rmb_start[0] == '0')&&(AP_Resp_Frame->stop_rmb_start[1] == '1'))
						{
							free_when_charge = 1;
						}
						else 
						{
							 free_when_charge = 0;
						}
						sprintf(tmp, "%.8s", &AP_Resp_Frame->stop_time_free[0]);
						free_minutes_after_charge = atoi(tmp);
					
					  if((dev_cfg.dev_para.car_park_price != car_park_price)||(dev_cfg.dev_para.free_when_charge != free_when_charge)
							||(dev_cfg.dev_para.free_minutes_after_charge != free_minutes_after_charge))
						{
							dev_cfg.dev_para.car_park_price = car_park_price;
							dev_cfg.dev_para.free_when_charge = free_when_charge;
							dev_cfg.dev_para.free_minutes_after_charge = free_minutes_after_charge;
							if(DEBUGLOG) CARLOCK_RUN_LogOut("地锁单价:%d 充电完成之后免费停车时间:%d  充电期间是否免费:%d",car_park_price,free_minutes_after_charge,free_when_charge);  
							System_CarLOCK_Param_save();
						}
						
						sprintf(tmp, "%.4s", &AP_Resp_Frame->step_server_price[0]);
						uitmp1 = atoi(tmp);
						
						sprintf(tmp, "%.4s", &AP_Resp_Frame->step_server_price[4]);
						uitmp2 = atoi(tmp);
						
						sprintf(tmp, "%.4s", &AP_Resp_Frame->step_server_price[8]);
						uitmp3 = atoi(tmp);
						sprintf(tmp, "%.4s", &AP_Resp_Frame->step_server_price[12]);
						uitmp4 = atoi(tmp);
						
						Globa_1->Charger_param2.share_time_kwh_price_serve[0] = uitmp1;  //分时电价1
						Globa_1->Charger_param2.share_time_kwh_price_serve[1] = uitmp2; //分时电价2
						Globa_1->Charger_param2.share_time_kwh_price_serve[2] = uitmp3;  //分时电价3
						Globa_1->Charger_param2.share_time_kwh_price_serve[3] = uitmp4;   //分时电价4
						
						Globa_2->Charger_param2.share_time_kwh_price_serve[0] = uitmp1;  //分时电价1
						Globa_2->Charger_param2.share_time_kwh_price_serve[1] = uitmp2; //分时电价2
						Globa_2->Charger_param2.share_time_kwh_price_serve[2] = uitmp3;  //分时电价3
						Globa_2->Charger_param2.share_time_kwh_price_serve[3] = uitmp4;   //分时电价4
						for(i=0;i<4;i++){
						  if(ISO8583_DEBUG)	printf("分时电价服务器1Globa_1->Charger_param2.share_time_kwh_price_serve[%d]=%d\n",i,Globa_1->Charger_param2.share_time_kwh_price_serve[i]);
						}
						sprintf(tmp, "%.2s", &AP_Resp_Frame->show_price_type[0]);
						Globa_2->Charger_param2.show_price_type = atoi(tmp);
						Globa_1->Charger_param2.show_price_type = atoi(tmp);

						sprintf(tmp, "%.2s", &AP_Resp_Frame->IS_fixed_price[0]);
			
						Globa_1->Apply_Price_type = atoi(tmp); //是否有定时电价
						
						memcpy(&Globa_1->Charger_param2.price_type_ver[0], &Globa_1->tmp_price_ver[0], sizeof(Globa_1->tmp_price_ver));//保存电价数据版本号
						memcpy(&Globa_2->Charger_param2.price_type_ver[0], &Globa_1->tmp_price_ver[0], sizeof(Globa_1->tmp_price_ver));//保存电价数据版本号

						System_param_save();//保存价格
						System_param2_save();//保存价格版本号
						Globa_1->have_price_change = 0;
					}else if((Globa_1->Apply_Price_type == 1)&&(length == (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Get_fixedPrice_Resp_Frame)))){//当前电价申请{//获取定时电价
						sprintf(tmp, "%.2s", &fixedAP_Resp_Frame->serv_type[0]);
						uitmp1 = atoi(tmp);
						sprintf(tmp, "%.8s", &fixedAP_Resp_Frame->serv_rmb[0]);
						uitmp2 = atoi(tmp);
						sprintf(tmp, "%.4s", &fixedAP_Resp_Frame->aver_rmb[0]);
						uitmp3 = atoi(tmp);
						
						Globa_1->charger_fixed_price.Serv_Type = uitmp1;
						Globa_1->charger_fixed_price.Serv_Price = uitmp2;
						Globa_1->charger_fixed_price.Price = uitmp3;
			
						sprintf(tmp, "%.4s", &fixedAP_Resp_Frame->step_rmb[0]);
						uitmp1 = atoi(tmp);
						
						sprintf(tmp, "%.4s", &fixedAP_Resp_Frame->step_rmb[4]);
						uitmp2 = atoi(tmp);
						
						sprintf(tmp, "%.4s", &fixedAP_Resp_Frame->step_rmb[8]);
						uitmp3 = atoi(tmp);
						
						sprintf(tmp, "%.4s", &fixedAP_Resp_Frame->step_rmb[12]);
						uitmp4 = atoi(tmp);
					
						Globa_1->charger_fixed_price.share_time_kwh_price[0] = uitmp1;  //分时电价1
						Globa_1->charger_fixed_price.share_time_kwh_price[1] = uitmp2; //分时电价2
						Globa_1->charger_fixed_price.share_time_kwh_price[2] = uitmp3;  //分时电价3
						Globa_1->charger_fixed_price.share_time_kwh_price[3] = uitmp4;   //分时电价4
						
		
						sprintf(tmp, "%.2s", &fixedAP_Resp_Frame->step[0]);
						uitmp1 = atoi(tmp);
						Globa_1->charger_fixed_price.time_zone_num = uitmp1;   //时段数

						if((uitmp1 > 0) && (uitmp1 <= 12)){//时段数1到12
							Globa_1->charger_fixed_price.time_zone_num = uitmp1;
							for(i=0;i<Globa_1->charger_fixed_price.time_zone_num;i++){
								uitmp2 = (fixedAP_Resp_Frame->timezone[i][0] -'0')*10+(fixedAP_Resp_Frame->timezone[i][1] -'0');
								uitmp3 = (fixedAP_Resp_Frame->timezone[i][2] -'0')*10+(fixedAP_Resp_Frame->timezone[i][3] -'0');
								if((uitmp2 < 24)  && (uitmp3 < 60)){
									Globa_1->charger_fixed_price.time_zone_tabel[i] = (uitmp2<<8)|uitmp3;
								}
								uitmp4 = (fixedAP_Resp_Frame->timezone[i][4] -'0');
								if((uitmp4 >= 1)&&(uitmp4 <= 4)){
									Globa_1->charger_fixed_price.time_zone_feilv[i] = uitmp4 -1;  //该时段对应的价格位置
								}
							}
						}
						
						/*sprintf(tmp, "%.2s", &fixedAP_Resp_Frame->stop_rmb_start[0]);
						free_when_charge = atoi(tmp);
						 sprintf(tmp, "%.8s", &AP_Resp_Frame->stop_rmb[0]);
						car_park_price = atoi(tmp);
						sprintf(tmp, "%.8s", &fixedAP_Resp_Frame->stop_time_free[0]);
						free_minutes_after_charge = atoi(tmp);
						
					  if((dev_cfg.dev_para.car_park_price != car_park_price)||(dev_cfg.dev_para.free_when_charge != free_when_charge)
							||(dev_cfg.dev_para.free_minutes_after_charge != free_minutes_after_charge))
						{
							dev_cfg.dev_para.car_park_price = car_park_price;
							dev_cfg.dev_para.free_when_charge = free_when_charge;
							dev_cfg.dev_para.free_minutes_after_charge = free_minutes_after_charge;
							System_CarLOCK_Param_save();
						}*/
						
						sprintf(tmp, "%.4s", &fixedAP_Resp_Frame->step_server_price[0]);
						uitmp1 = atoi(tmp);
						
						sprintf(tmp, "%.4s", &fixedAP_Resp_Frame->step_server_price[4]);
						uitmp2 = atoi(tmp);
						
						sprintf(tmp, "%.4s", &fixedAP_Resp_Frame->step_server_price[8]);
						uitmp3 = atoi(tmp);
						sprintf(tmp, "%.4s", &fixedAP_Resp_Frame->step_server_price[12]);
						uitmp4 = atoi(tmp);
						Globa_1->charger_fixed_price.share_time_kwh_price_serve[0] = uitmp1;  //分时电价1
						Globa_1->charger_fixed_price.share_time_kwh_price_serve[1] = uitmp2; //分时电价2
						Globa_1->charger_fixed_price.share_time_kwh_price_serve[2] = uitmp3;  //分时电价3
						Globa_1->charger_fixed_price.share_time_kwh_price_serve[3] = uitmp4;   //分时电价4
						unsigned char fixed_price_effective_time[7],i=0;
						str_to_hex(fixed_price_effective_time, &fixedAP_Resp_Frame->fixed_price_effective_time[0], sizeof(fixedAP_Resp_Frame->fixed_price_effective_time));
						for(i=0;i<7;i++){
							Globa_1->charger_fixed_price.fixed_price_effective_time[i] = bcd_to_int_1byte(fixed_price_effective_time[i]);
							if(ISO8583_DEBUG)printf("定时时间 i=%d,fixed_price_effective_time=%d,\n", i,Globa_1->charger_fixed_price.fixed_price_effective_time[i]);
						}
					//	memcpy(&Globa_1->charger_fixed_price.fixed_price_effective_time[0], &fixedAP_Resp_Frame->fixed_price_effective_time[0], sizeof(fixedAP_Resp_Frame->fixed_price_effective_time));//保存电价数据版本号
            Globa_1->charger_fixed_price.falg = 1;//表示存在定时电价
					 //针对生效时间的处理。
   				  System_param_Fixedpirce_save();//保存价格版本号
					  Globa_1->Apply_Price_type = 0;//定时电价
				   	if(ISO8583_DEBUG)printf("定时电价更新 Serv_Type=%d,Serv_Price=%d,Price=%d\n", uitmp1,uitmp2,uitmp3);
					}
					if(ISO8583_DEBUG)printf("电价更新 Globa_1->Apply_Price_type=%d,length=%d,Price=%d\n", Globa_1->Apply_Price_type,length,(sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP_Get_fixedPrice_Resp_Frame)));
					return (0);
				}else if((AP_Resp_Frame->flag[0] == '0')&&(AP_Resp_Frame->flag[1] == '4')){//无资费策略
					//memcpy(&Globa_1->Charger_param2.price_type_ver[0], &Globa_1->tmp_price_ver[0], sizeof(Globa_1->tmp_price_ver));//保存电价数据版本号
					//System_param_save();//保存价格
					//System_param2_save();//保存价格版本号
					return (0);
				}
			}
	  }
	}
	return (-1);
}

//删除超过2000条的最早记录（一次只删除一条记录）
int delete_over_2000_busy()
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;

	int id = 0, count = 0;

	char sql[512];


	rc = sqlite3_open_v2(F_CHARGER_BUSY_COMPLETE_DB, &db,
								     	 SQLITE_OPEN_NOMUTEX|
									     SQLITE_OPEN_SHAREDCACHE|
									     SQLITE_OPEN_READWRITE,
									     NULL);
	if(rc){
		sqlite3_close(db);
		return -1;
	}

	sprintf(sql, "SELECT COUNT(*) FROM Charger_Busy_Complete_Data;");
	if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL)){
		if(azResult[ncolumn*1+0] != NULL){//2行1列
			count = atoi(azResult[ncolumn*1+0]);
			sqlite3_free_table(azResult);

			if(count > MAX_CHARGER_BUSY_NUM_LIMINT){//超过最大允许记录数目
  			//sprintf(sql, "SELECT MIN(ID) FROM Charger_Busy_Complete_Data;");
			  sprintf(sql, "SELECT MAX(ID) FROM Charger_Busy_Complete_Data;");
  			if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL)){
  				if(azResult[ncolumn*1+0] != NULL){//2行1列
  					id = atoi(azResult[ncolumn*1+0]);
						id = (id>MAX_CHARGER_BUSY_NUM_LIMINT)? id:MAX_CHARGER_BUSY_NUM_LIMINT;
  					sqlite3_free_table(azResult);

       			sprintf(sql, "DELETE FROM Charger_Busy_Complete_Data WHERE ID <= '%d';", (id - MAX_CHARGER_BUSY_NUM_LIMINT));
						if(sqlite3_exec(db , sql , 0, 0, &zErrMsg) != SQLITE_OK){
							sqlite3_free(zErrMsg);
						}else{
							sqlite3_close(db);
      				return (id);
						}
  				}else{
  					sqlite3_free_table(azResult);
  				}
  			}else{
  				sqlite3_free_table(azResult);
  			}
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



//用户卡解锁请求接口   发送上行数据
int ISO8583_User_Card_Unlock_Request_sent(int fd)
{
	unsigned int ret,len;
	unsigned char SendBuf[512];  //发送数据区指针
	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针

	time_t systime=0;
	struct tm tm_t;

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_User_Card_Unlock_Request_Frame *User_Card_Unlock_Frame;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	User_Card_Unlock_Frame = (ISO8583_User_Card_Unlock_Request_Frame *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据初始化
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'

	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_User_Card_Unlock_Request_Frame))>>8;  //报文长度
	Head_Frame->SessionLen[1] = sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_User_Card_Unlock_Request_Frame);
	memcpy(&Head_Frame->MessgTypeIdent[0], "1090", sizeof(Head_Frame->MessgTypeIdent));//消息标识符

	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化帧数据内容为 0x00


	Head_Frame->PrimaryBitmap[2/8 -0] |= 0x80 >> ((2-1)%8);//主位元表 2
	Head_Frame->PrimaryBitmap[12/8 -0] |= 0x80 >> ((12-1)%8);//主位元表 12
	Head_Frame->PrimaryBitmap[42/8 -0] |= 0x80 >> ((42-1)%8);//主位元表 42
	Head_Frame->PrimaryBitmap[62/8 -0] |= 0x80 >> ((62-1)%8);//主位元表 62
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63
	
	User_Card_Unlock_Frame->data_len[0] = 0x31;
	User_Card_Unlock_Frame->data_len[1] = 0x32;
//填充业务数据内容
	memcpy(&User_Card_Unlock_Frame->card_sn[0], &Globa_1->card_sn[0], sizeof(User_Card_Unlock_Frame->card_sn));
	
	memcpy(&User_Card_Unlock_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(User_Card_Unlock_Frame->ter_sn));//重新填充桩号，以免桩号改变以前的交易记录上传失败
	sprintf(&tmp[0], "%02d%02d%02d%02d%02d%02d%02d", Globa_1->User_Card_Lock_Info.card_lock_time.year_H,Globa_1->User_Card_Lock_Info.card_lock_time.year_L, Globa_1->User_Card_Lock_Info.card_lock_time.month, Globa_1->User_Card_Lock_Info.card_lock_time.day_of_month, Globa_1->User_Card_Lock_Info.card_lock_time.hour, Globa_1->User_Card_Lock_Info.card_lock_time.minute, Globa_1->User_Card_Lock_Info.card_lock_time.second);
	memcpy(&User_Card_Unlock_Frame->lockstart_time[0],&tmp[0], sizeof(User_Card_Unlock_Frame->lockstart_time));//

	systime = time(NULL);        //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
	sprintf(&tmp[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
	memcpy(&User_Card_Unlock_Frame->time[0], &tmp[0], sizeof(User_Card_Unlock_Frame->time));//终端时间（格式yyyyMMddHHmmss）	62	N	N14

	MD5_DES(&User_Card_Unlock_Frame->card_sn[0], sizeof(ISO8583_User_Card_Unlock_Request_Frame)-sizeof(User_Card_Unlock_Frame->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&User_Card_Unlock_Frame->MD5[0], &MD5[0], 8);

	len = ISO8583_Sent_Data(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_User_Card_Unlock_Request_Frame)), 0);
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_User_Card_Unlock_Request_Frame))){//如果发送不成功
		return (-1);
	}else{
		COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}

//用户卡解锁请求接口
int ISO8583_User_card_unlock_Request_Deal(unsigned int length,unsigned char *buf)
{
	unsigned char tmp[100];
	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_User_Card_Unlock_Request_Resp_Frame *User_Card_Unlock_Request_Resp_Frame;
	ISO8583_User_Card_Unlock_Request_Resp111_Frame *User_Card_Unlock_err_Resp_Frame;
	Head_Frame = (ISO8583_Head_Frame *)(buf);
	User_Card_Unlock_Request_Resp_Frame = (ISO8583_User_Card_Unlock_Request_Resp_Frame *)(buf+sizeof(ISO8583_Head_Frame));
  User_Card_Unlock_err_Resp_Frame = (ISO8583_User_Card_Unlock_Request_Resp111_Frame *)(buf+sizeof(ISO8583_Head_Frame));
  
	if(length == (sizeof(ISO8583_User_Card_Unlock_Request_Resp_Frame)+ sizeof(ISO8583_Head_Frame))){
		if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "1091", sizeof(Head_Frame->MessgTypeIdent))){
			if((User_Card_Unlock_Request_Resp_Frame->flag[0] == '0')&&(User_Card_Unlock_Request_Resp_Frame->flag[1] == '0')){ //信息处理成功
				sprintf(tmp, "%.8s", &User_Card_Unlock_Request_Resp_Frame->total_rmb[0]);
				Globa_1->User_Card_Lock_Info.Deduct_money = atoi(tmp);
				memcpy(&Globa_1->User_Card_Lock_Info.busy_SN[0], &User_Card_Unlock_Request_Resp_Frame->Busy_SN[0], sizeof(User_Card_Unlock_Request_Resp_Frame->Busy_SN));//消息标识符  "0120"
				Globa_1->User_Card_Lock_Info.Card_unlock_Query_Results = 1; //查询成功有记录
			}else if((User_Card_Unlock_Request_Resp_Frame->flag[0] == '0')&&(User_Card_Unlock_Request_Resp_Frame->flag[1] == '3')){ //信息处理成功
				Globa_1->User_Card_Lock_Info.Deduct_money = 0;
				Globa_1->User_Card_Lock_Info.Card_unlock_Query_Results = 2; ////失败，卡号无效
			}else if((User_Card_Unlock_Request_Resp_Frame->flag[0] == '0')&&(User_Card_Unlock_Request_Resp_Frame->flag[1] == '4')){ //信息处理成功
				Globa_1->User_Card_Lock_Info.Deduct_money = 0;
				Globa_1->User_Card_Lock_Info.Card_unlock_Query_Results = 3; ////无效充电桩序列号
			}else if((User_Card_Unlock_Request_Resp_Frame->flag[0] == '0')&&(User_Card_Unlock_Request_Resp_Frame->flag[1] == '5')){ //信息处理成功
				Globa_1->User_Card_Lock_Info.Deduct_money = 0;
				Globa_1->User_Card_Lock_Info.Card_unlock_Query_Results = 4; //未找到充电记录
			}else{
				Globa_1->User_Card_Lock_Info.Deduct_money = 0;
				Globa_1->User_Card_Lock_Info.Card_unlock_Query_Results = 5; //MAC 出差或终端为未对时	
			}
		}else{
			Globa_1->User_Card_Lock_Info.Deduct_money = 0;
			Globa_1->User_Card_Lock_Info.Card_unlock_Query_Results = 4; //未找到充电记录
		}
		ISO8583_App_Struct.Card_unlock_RequestRcev_falg = 1;
		Globa_1->User_Card_Lock_Info.User_card_unlock_Requestfalg = 0;
		return (0);
	}else if(length == (sizeof(ISO8583_User_Card_Unlock_Request_Resp111_Frame)+ sizeof(ISO8583_Head_Frame))){
		if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "1091", sizeof(Head_Frame->MessgTypeIdent))){
			if((User_Card_Unlock_err_Resp_Frame->flag[0] == '0')&&(User_Card_Unlock_err_Resp_Frame->flag[1] == '0')){ //信息处理成功
				//sprintf(tmp, "%.8s", &User_Card_Unlock_Request_Resp_Frame->total_rmb[0]);
				//Globa_1->User_Card_Lock_Info.Deduct_money = atoi(tmp);
				//memcpy(&Globa_1->User_Card_Lock_Info.busy_SN[0], &User_Card_Unlock_Request_Resp_Frame->Busy_SN[0], sizeof(User_Card_Unlock_Request_Resp_Frame->Busy_SN));//消息标识符  "0120"
			//	Globa_1->User_Card_Lock_Info.Card_unlock_Query_Results = 1; //查询成功有记录
			}else if((User_Card_Unlock_err_Resp_Frame->flag[0] == '0')&&(User_Card_Unlock_err_Resp_Frame->flag[1] == '3')){ //信息处理成功
				Globa_1->User_Card_Lock_Info.Deduct_money = 0;
				Globa_1->User_Card_Lock_Info.Card_unlock_Query_Results = 2; ////失败，卡号无效
			}else if((User_Card_Unlock_err_Resp_Frame->flag[0] == '0')&&(User_Card_Unlock_err_Resp_Frame->flag[1] == '4')){ //信息处理成功
				Globa_1->User_Card_Lock_Info.Deduct_money = 0;
				Globa_1->User_Card_Lock_Info.Card_unlock_Query_Results = 3; ////无效充电桩序列号
			}else if((User_Card_Unlock_err_Resp_Frame->flag[0] == '0')&&(User_Card_Unlock_err_Resp_Frame->flag[1] == '5')){ //信息处理成功
				Globa_1->User_Card_Lock_Info.Deduct_money = 0;
				Globa_1->User_Card_Lock_Info.Card_unlock_Query_Results = 4; //未找到充电记录
			}else{
				Globa_1->User_Card_Lock_Info.Deduct_money = 0;
				Globa_1->User_Card_Lock_Info.Card_unlock_Query_Results = 5; //MAC 出差或终端为未对时	
			}
		}else{
			Globa_1->User_Card_Lock_Info.Deduct_money = 0;
			Globa_1->User_Card_Lock_Info.Card_unlock_Query_Results = 4; //未找到充电记录
		}
		ISO8583_App_Struct.Card_unlock_RequestRcev_falg = 1;
		Globa_1->User_Card_Lock_Info.User_card_unlock_Requestfalg = 0;
		return (0);
	}

	return (-1);
}


//用户卡解锁数据上报接口   发送上行数据
int ISO8583_User_Card_Unlock_Data_Request_sent(int fd)
{
	unsigned int ret,len;
	unsigned char SendBuf[512];  //发送数据区指针
	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针

	time_t systime=0;
	struct tm tm_t;

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_User_Card_Unlock_Data_Request_Frame *User_Card_Unlock_Data_Frame;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	User_Card_Unlock_Data_Frame = (ISO8583_User_Card_Unlock_Data_Request_Frame *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据初始化
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'

	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_User_Card_Unlock_Data_Request_Frame))>>8;  //报文长度
	Head_Frame->SessionLen[1] = sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_User_Card_Unlock_Data_Request_Frame);
	memcpy(&Head_Frame->MessgTypeIdent[0], "1100", sizeof(Head_Frame->MessgTypeIdent));//消息标识符

	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化帧数据内容为 0x00


	Head_Frame->PrimaryBitmap[2/8 -0] |= 0x80 >> ((2-1)%8);//主位元表 2
	Head_Frame->PrimaryBitmap[11/8 -0] |= 0x80 >> ((11-1)%8);//主位元表 12
	Head_Frame->PrimaryBitmap[12/8 -0] |= 0x80 >> ((12-1)%8);//主位元表 12
	Head_Frame->PrimaryBitmap[42/8 -0] |= 0x80 >> ((42-1)%8);//主位元表 42
	Head_Frame->PrimaryBitmap[62/8 -0] |= 0x80 >> ((62-1)%8);//主位元表 62
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63
	
	User_Card_Unlock_Data_Frame->data_len[0] = 0x31;
	User_Card_Unlock_Data_Frame->data_len[1] = 0x32;
//填充业务数据内容
	memcpy(&User_Card_Unlock_Data_Frame->card_sn[0], &Globa_1->card_sn[0], sizeof(User_Card_Unlock_Data_Frame->card_sn));
	memcpy(&User_Card_Unlock_Data_Frame->Busy_SN[0], &Globa_1->User_Card_Lock_Info.busy_SN[0], sizeof(Globa_1->User_Card_Lock_Info.busy_SN));
	memcpy(&User_Card_Unlock_Data_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(User_Card_Unlock_Data_Frame->ter_sn));//重新填充桩号，以免桩号改变以前的交易记录上传失败
//	memcpy(&User_Card_Unlock_Data_Frame->unlockend_time[0], &Globa_1->unlockend_time[0], sizeof(User_Card_Unlock_Data_Frame->unlockend_time));//

	systime = time(NULL);        //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
	sprintf(&tmp[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
	
	memcpy(&User_Card_Unlock_Data_Frame->unlock_time[0], &tmp[0], sizeof(User_Card_Unlock_Data_Frame->unlock_time));//终端时间（格式yyyyMMddHHmmss）	62	N	N14

	memcpy(&User_Card_Unlock_Data_Frame->time[0], &tmp[0], sizeof(User_Card_Unlock_Data_Frame->time));//终端时间（格式yyyyMMddHHmmss）	62	N	N14

	MD5_DES(&User_Card_Unlock_Data_Frame->card_sn[0], sizeof(ISO8583_User_Card_Unlock_Data_Request_Frame)-sizeof(User_Card_Unlock_Data_Frame->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&User_Card_Unlock_Data_Frame->MD5[0], &MD5[0], 8);

	len = ISO8583_Sent_Data(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_User_Card_Unlock_Data_Request_Frame)), 0);
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_User_Card_Unlock_Data_Request_Frame))){//如果发送不成功
		return (-1);
	}else{
		COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}

//用户卡解锁数据上报接口
int ISO8583_User_card_unlock_Data_Request_Deal(unsigned int length,unsigned char *buf)
{
	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_User_Card_Unlock_Data_Request_Resp_Frame *User_Card_Unlock_Data_Request_Resp_Frame;
  Head_Frame = (ISO8583_Head_Frame *)(buf);
	User_Card_Unlock_Data_Request_Resp_Frame = (ISO8583_User_Card_Unlock_Data_Request_Resp_Frame *)(buf+sizeof(ISO8583_Head_Frame));

	if(length > 0){//有数据响应，但不判断长度（因为长度不定）
		if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "1101", sizeof(Head_Frame->MessgTypeIdent))){
			if((User_Card_Unlock_Data_Request_Resp_Frame->flag[0] == '0')&&(User_Card_Unlock_Data_Request_Resp_Frame->flag[1] == '0')){ //信息处理成功
				Globa_1->User_Card_Lock_Info.User_card_unlock_Data_Requestfalg = 0;
				return (0);
			}
		}
	}
	return (-1);
}


//上报情况
int ISO8583_3041_sent(int fd,int flag)
{
	unsigned int ret,len;
	unsigned char SendBuf[512];  //发送数据区指针
	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针

	time_t systime=0;
	struct tm tm_t;

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_3041_Resp_Frame *ISO8583_3041_Resp_data;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	ISO8583_3041_Resp_data = (ISO8583_3041_Resp_Frame *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据初始化
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'

	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_3041_Resp_Frame))>>8;  //报文长度
	Head_Frame->SessionLen[1] = sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_3041_Resp_Frame);
	memcpy(&Head_Frame->MessgTypeIdent[0], "3041", sizeof(Head_Frame->MessgTypeIdent));//消息标识符

	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化帧数据内容为 0x00
	Head_Frame->PrimaryBitmap[39/8 -0] |= 0x80 >> ((39-1)%8);//主位元表 42
	Head_Frame->PrimaryBitmap[41/8 -0] |= 0x80 >> ((41-1)%8);//主位元表 62
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63

	memcpy(&ISO8583_3041_Resp_data->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(ISO8583_3041_Resp_data->ter_sn));//重新填充桩号，以免桩号改变以前的交易记录上传失败
  ISO8583_3041_Resp_data->flag[0] = '0';
  if(flag == 0){
	  ISO8583_3041_Resp_data->flag[1] = '0';
	}else if(flag == 1){
	  ISO8583_3041_Resp_data->flag[1] = '1';
	}else if(flag == 2){
	  ISO8583_3041_Resp_data->flag[1] = '2';
	}else if(flag == 3){
	  ISO8583_3041_Resp_data->flag[1] = '3';
	}else if(flag == 4){
	  ISO8583_3041_Resp_data->flag[1] = '4';
	}else if(flag == 5){
	  ISO8583_3041_Resp_data->flag[1] = '5';
	}
	MD5_DES(&ISO8583_3041_Resp_data->flag[0], sizeof(ISO8583_3041_Resp_Frame)-sizeof(ISO8583_3041_Resp_data->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&ISO8583_3041_Resp_data->MD5[0], &MD5[0], 8);
  
	len = ISO8583_Sent_Data(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_3041_Resp_Frame)), 0);
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_3041_Resp_Frame))){//如果发送不成功
		return (-1);
	}else{
		COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}


//充电桩上传升级请求   发送上行数据
int ISO8583_AP1060_Request_sent(int fd,UINT32 data_block_num_want)
{
	unsigned int ret,len;
	unsigned char SendBuf[512];  //发送数据区指针
	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针
	unsigned short tx_len = sizeof(ISO8583_Head_Frame) +  sizeof(ISO8583_APUPDATE_Frame);//
	time_t systime=0;
	struct tm tm_t;

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_APUPDATE_Frame *Update_Data_Frame;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	Update_Data_Frame = (ISO8583_APUPDATE_Frame *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据初始化
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'
	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_APUPDATE_Frame))>>8;  //报文长度
	Head_Frame->SessionLen[1] = sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_APUPDATE_Frame);
	memcpy(&Head_Frame->MessgTypeIdent[0], "1060", sizeof(Head_Frame->MessgTypeIdent));//消息标识符
	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化帧数据内容为 0x00

	Head_Frame->PrimaryBitmap[11/8 -0] |= 0x80 >> ((11-1)%8);//主位元表 12
	Head_Frame->PrimaryBitmap[41/8 -0] |= 0x80 >> ((41-1)%8);//主位元表 12
	Head_Frame->PrimaryBitmap[52/8 -0] |= 0x80 >> ((52-1)%8);//主位元表 42
	Head_Frame->PrimaryBitmap[62/8 -0] |= 0x80 >> ((62-1)%8);//主位元表 62
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63
	
//填充业务数据内容

	sprintf(&tmp[0], "%08d", data_block_num_want);				
	memcpy(&Update_Data_Frame->data_block_num[0], &tmp[0], sizeof(Update_Data_Frame->data_block_num));
	memcpy(&Update_Data_Frame->charger_sn[0], &Globa_1->Charger_param.SN[0], sizeof(Update_Data_Frame->charger_sn));//充电终端序列号
	memcpy(&Update_Data_Frame->chg_soft_ver_get[0], &Globa_1->tmp_Software_Version[0], sizeof(Update_Data_Frame->chg_soft_ver_get));//黑名单数据版本号

	systime = time(NULL);        //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
	sprintf(&tmp[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
	memcpy(&Update_Data_Frame->time[0], &tmp[0], sizeof(Update_Data_Frame->time));//终端时间（格式yyyyMMddHHmmss）	62	N	N14
	MD5_DES(&Update_Data_Frame->data_block_num[0], sizeof(ISO8583_APUPDATE_Frame)-sizeof(Update_Data_Frame->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&Update_Data_Frame->MD5[0], &MD5[0], 8);
	len = ISO8583_Sent_Data(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_APUPDATE_Frame)), 0);
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_APUPDATE_Frame))){//如果发送不成功
		return (-1);
	}else{
		COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}

//升级程序解锁
int ISO8583_AP1060_Request_Data_Request_Deal(unsigned int length,unsigned char *buf)
{

	int i,j,ls_length = 0;
	struct timeval tv1;
	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针
	unsigned char write_buf[320];
	
  INT32 fd_Update = 0;
  static FILE *fp_Update = NULL;
	UINT32 ls_curent_data_block_num = 0;//需要的包见

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_APUPDATE_Resp_Frame * Update_Resp_data;
	Head_Frame = (ISO8583_Head_Frame *)(buf);
	Update_Resp_data = (ISO8583_APUPDATE_Resp_Frame *)(buf+sizeof(ISO8583_Head_Frame));

	if(length > (sizeof(Update_Resp_data->MD5)+sizeof(ISO8583_Head_Frame))){//有数据响应，但不判断长度（因为长度不定）
		MD5_DES(&Update_Resp_data->curent_data_block_num[0],  (length - sizeof(Update_Resp_data->MD5)-sizeof(ISO8583_Head_Frame)), &ISO8583_App_Struct.work_key[0], &MD5[0]);
		hex_to_str(&tmp[0], &MD5[0], 8);
		if(0 == memcmp(&tmp[0], &buf[length - sizeof(Update_Resp_data->MD5)], sizeof(Update_Resp_data->MD5))){
			if((Update_Resp_data->flag[0] == '0')&&(Update_Resp_data->flag[1] == '0')){ //信息处理成功
				All_Data_Num = convert_8ascii_2UL(&Update_Resp_data->all_data_num[0]);
				ls_curent_data_block_num = convert_8ascii_2UL(&Update_Resp_data->curent_data_block_num[0]);
				//printf("xxxxxx ---MAC解密成功 All_Data_Num = %d  ls_curent_data_block_num =%d curent_data_block_num =%d\n",All_Data_Num,ls_curent_data_block_num ,curent_data_block_num);

				if(ls_curent_data_block_num == 1){//第一个数据包
					fp_Update = fopen(F_UPDATE_Charger_Package, "wb+");
					if(fp_Update == NULL){
						curent_data_block_num = 1;
						fclose(fp_Update);
						return 0;
					}else{
						//printf("xxxxxx ---MAC解密成功 curent_data_block_num = %d  ls_curent_data_block_num =%d \n",curent_data_block_num,ls_curent_data_block_num);
						if(ls_curent_data_block_num == curent_data_block_num){
							ls_length = (Update_Resp_data->data_len[0]-'0')*100 + (Update_Resp_data->data_len[1]-'0')*10 + (Update_Resp_data->data_len[2]-'0');
							str_to_hex(write_buf, &Update_Resp_data->data[0], ls_length);
							fwrite(&write_buf, sizeof(UINT8), ls_length/2, fp_Update);
							
					  	if(curent_data_block_num == All_Data_Num ){//表面数据接收完毕
								fd_Update = fileno(fp_Update);
								fsync(fd_Update);//new add
								fclose(fp_Update);
								fp_Update = NULL;
								ISO8583_App_Struct.Software_upate_flag = 2;
								return 2; //表示更新完成程序
							}else{
								curent_data_block_num ++;
								ISO8583_App_Struct.Software_upate_flag = 1;
							}
						}
					}
				}else{
					//printf("xxxxxx ---MAC curent_data_block_num = %d  ls_curent_data_block_num =%d \n",curent_data_block_num,ls_curent_data_block_num);
					if(ls_curent_data_block_num == curent_data_block_num){
						ls_length = (Update_Resp_data->data_len[0]-'0')*100 + (Update_Resp_data->data_len[1]-'0')*10 + (Update_Resp_data->data_len[2]-'0');
						str_to_hex(write_buf, &Update_Resp_data->data[0], ls_length);
						fwrite(&write_buf, sizeof(UINT8), ls_length/2, fp_Update);	
						if(curent_data_block_num == All_Data_Num ){//表面数据接收完毕
							fd_Update = fileno(fp_Update);
							fsync(fd_Update);//new add
							fclose(fp_Update);
							fp_Update = NULL;
							ISO8583_App_Struct.Software_upate_flag = 2;
							return 2; //表示更新完成程序
						}else{
							curent_data_block_num ++;
							ISO8583_App_Struct.Software_upate_flag = 1;
						}
					}
				}
				return 0;
			}else{
				ISO8583_App_Struct.Software_upate_flag = 3;
				//printf("xxxxxx ---MAC解密失败 %d \n",length);
				return 0;
			}
		}
	}
	
}

//充电桩上传升级确认   发送上行数据
int ISO8583_AP1070_Request_sent(int fd)
{
	unsigned int ret,len;
	unsigned char SendBuf[512];  //发送数据区指针
	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针
	unsigned short tx_len = sizeof(ISO8583_Head_Frame) +  sizeof(ISO8583_AP1070_Frame);//
	time_t systime=0;
	struct tm tm_t;

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_AP1070_Frame *Update_Data_Frame;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	Update_Data_Frame = (ISO8583_AP1070_Frame *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据初始化
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'
	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP1070_Frame))>>8;  //报文长度
	Head_Frame->SessionLen[1] = sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP1070_Frame);
	memcpy(&Head_Frame->MessgTypeIdent[0], "1070", sizeof(Head_Frame->MessgTypeIdent));//消息标识符
	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化帧数据内容为 0x00

	Head_Frame->PrimaryBitmap[41/8 -0] |= 0x80 >> ((41-1)%8);//主位元表 12
	Head_Frame->PrimaryBitmap[52/8 -0] |= 0x80 >> ((52-1)%8);//主位元表 42
	Head_Frame->PrimaryBitmap[62/8 -0] |= 0x80 >> ((62-1)%8);//主位元表 62
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63
	

//填充业务数据内容

	memcpy(&Update_Data_Frame->charger_sn[0], &Globa_1->Charger_param.SN[0], sizeof(Update_Data_Frame->charger_sn));//充电终端序列号
	memcpy(&Update_Data_Frame->chg_soft_ver[0], &Globa_1->Charger_param2.Software_Version[0], sizeof(Update_Data_Frame->chg_soft_ver));//黑名单数据版本号

	systime = time(NULL);        //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
	sprintf(&tmp[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
	memcpy(&Update_Data_Frame->time[0], &tmp[0], sizeof(Update_Data_Frame->time));//终端时间（格式yyyyMMddHHmmss）	62	N	N14
	MD5_DES(&Update_Data_Frame->charger_sn[0], sizeof(ISO8583_AP1070_Frame)-sizeof(Update_Data_Frame->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&Update_Data_Frame->MD5[0], &MD5[0], 8);
	len = ISO8583_Sent_Data(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP1070_Frame)), 0);
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP1070_Frame))){//如果发送不成功
		return (-1);
	}else{
		COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}


//BSM信息数据发送上行数据
int ISO8583_3051_Send_Battery_Data_sent(int fd,int gunno,int falg)
{
	unsigned int ret,len;
	unsigned char SendBuf[512];  //发送数据区指针
	unsigned char MD5[8];  //发送数据区指针
	char temp_char[9]={0};
	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_3051_Send_Battery_Frame *ISO3051_Send_Battery_data;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	ISO3051_Send_Battery_data = (ISO8583_3051_Send_Battery_Frame *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据初始化
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'

	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_3051_Send_Battery_Frame))>>8;  //报文长度
	Head_Frame->SessionLen[1] = sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_3051_Send_Battery_Frame);
	memcpy(&Head_Frame->MessgTypeIdent[0], "3051", sizeof(Head_Frame->MessgTypeIdent));//消息标识符

	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化帧数据内容为 0x00
	Head_Frame->PrimaryBitmap[39/8 -0] |= 0x80 >> ((39-1)%8);//主位元表 39
	Head_Frame->PrimaryBitmap[41/8 -0] |= 0x80 >> ((41-1)%8);//主位元表 41
	Head_Frame->PrimaryBitmap[46/8 -0] |= 0x80 >> ((46-1)%8);//主位元表 46
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63
	
	memcpy(&ISO3051_Send_Battery_data->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(ISO3051_Send_Battery_data->ter_sn));//重新填充桩号，以免桩号改变以前的交易记录上传失败

	ISO3051_Send_Battery_data->flag[0] = '0';
  if(falg == 0){
	  ISO3051_Send_Battery_data->flag[1] = '0';
		if(gunno == 1){
			sprintf(&temp_char[0], "%08d", Globa_1->meter_KWH);				
			memcpy(&ISO3051_Send_Battery_data->Now_Khw[0], &temp_char[0], sizeof(ISO3051_Send_Battery_data->Now_Khw));
		}else if(gunno == 2){
	  	sprintf(&temp_char[0], "%08d", Globa_2->meter_KWH);				
			memcpy(&ISO3051_Send_Battery_data->Now_Khw[0], &temp_char[0], sizeof(ISO3051_Send_Battery_data->Now_Khw));
		}
	}else if(falg == 1){
		sprintf(&temp_char[0], "%08d", 0);				
		memcpy(&ISO3051_Send_Battery_data->Now_Khw[0], &temp_char[0], sizeof(ISO3051_Send_Battery_data->Now_Khw));
		ISO3051_Send_Battery_data->flag[1] = '1';
	}else if(falg == 2){
		sprintf(&temp_char[0], "%08d", 0);				
		memcpy(&ISO3051_Send_Battery_data->Now_Khw[0], &temp_char[0], sizeof(ISO3051_Send_Battery_data->Now_Khw));
		ISO3051_Send_Battery_data->flag[1] = '2';
	}else if(falg == 3){
		sprintf(&temp_char[0], "%08d", 0);				
		memcpy(&ISO3051_Send_Battery_data->Now_Khw[0], &temp_char[0], sizeof(ISO3051_Send_Battery_data->Now_Khw));
		ISO3051_Send_Battery_data->flag[1] = '3';
	}
	MD5_DES(&ISO3051_Send_Battery_data->flag[0], sizeof(ISO8583_3051_Send_Battery_Frame)-sizeof(ISO3051_Send_Battery_data->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&ISO3051_Send_Battery_data->MD5[0], &MD5[0], 8);
	len = ISO8583_Sent_Data(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_3051_Send_Battery_Frame)), 0);
	
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_3051_Send_Battery_Frame))){//如果发送不成功
		return (-1);
	}else{
		COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}

int ISO8583_AP3050_Request_Data_Request_Deal(int fd, unsigned int length,unsigned char *buf)
{
	int i,j,ls_length = 0,ret=0;
	struct timeval tv1;
	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针
	unsigned char write_buf[320];
	
  INT32 fd_Update = 0;
  static FILE *fp_Update = NULL;
	UINT32 ls_curent_data_block_num = 0;//需要的包见

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_3050_Read_Battery_Frame * Read_Battery_Resp_data;
	Head_Frame = (ISO8583_Head_Frame *)(buf);
	Read_Battery_Resp_data = (ISO8583_3050_Read_Battery_Frame *)(buf+sizeof(ISO8583_Head_Frame));

	if(length > (sizeof(Read_Battery_Resp_data->MD5)+sizeof(ISO8583_Head_Frame))){//有数据响应，但不判断长度（因为长度不定）
		MD5_DES(&Read_Battery_Resp_data->ter_sn[0],  (length - sizeof(Read_Battery_Resp_data->MD5)-sizeof(ISO8583_Head_Frame)), &ISO8583_App_Struct.work_key[0], &MD5[0]);
		hex_to_str(&tmp[0], &MD5[0], 8);
		if(0 == memcmp(&tmp[0], &buf[length - sizeof(Read_Battery_Resp_data->MD5)], sizeof(Read_Battery_Resp_data->MD5))){
			time(&Rcev_current_now);
		  Rcev_pre_time = Rcev_current_now;
			if(Read_Battery_Resp_data->gunno[1] == '1'){//读取1号电表数据
        if(Globa_1->Error.meter_connect_lost == 1){
         ret = ISO8583_3051_Send_Battery_Data_sent(fd, 1,3);
				}else{
				  ret =ISO8583_3051_Send_Battery_Data_sent(fd, 1,0);
				}
			}else if(Read_Battery_Resp_data->gunno[1] == '2'){//读取1号电表数据
			  if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
          if(Globa_1->Charger_param.System_Type == 1 ){//双枪轮流
					  if(Globa_1->Error.meter_connect_lost == 1){
							ret =ISO8583_3051_Send_Battery_Data_sent(fd, 2,3);
						}else{
							ret =ISO8583_3051_Send_Battery_Data_sent(fd, 2,0);
						}
					}else{
						 if(Globa_2->Error.meter_connect_lost == 1){
							ret =ISO8583_3051_Send_Battery_Data_sent(fd, 2,3);
						}else{
							ret =ISO8583_3051_Send_Battery_Data_sent(fd, 2,0);
						}	
					}
		    }else{
					ret =ISO8583_3051_Send_Battery_Data_sent(fd, 1,3);
				}
			}else{
				ret =ISO8583_3051_Send_Battery_Data_sent(fd, 1,3);
			}	
		}else{
			ret = ISO8583_3051_Send_Battery_Data_sent(fd,0,1);
		}		
	}
	return ret;
}
//4.5.3地锁开关接口  发送上行数据
int ISO8583_Car_lock_Request_sent(int fd,int falg, char* User_Card)
{
	unsigned int ret,len;
	unsigned char SendBuf[512];  //发送数据区指针
	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针
	unsigned short tx_len = sizeof(ISO8583_Head_Frame) +  sizeof(ISO8583_Car_lock_Data_Respone_Frame);//
	time_t systime=0;
	struct tm tm_t;

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_Car_lock_Data_Respone_Frame *Car_lock_Data_Respone_Frame;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	Car_lock_Data_Respone_Frame = (ISO8583_Car_lock_Data_Respone_Frame *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据初始化
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'
	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_Car_lock_Data_Respone_Frame))>>8;  //报文长度
	Head_Frame->SessionLen[1] = sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_Car_lock_Data_Respone_Frame);
	memcpy(&Head_Frame->MessgTypeIdent[0], "3031", sizeof(Head_Frame->MessgTypeIdent));//消息标识符
	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化帧数据内容为 0x00

	Head_Frame->PrimaryBitmap[9/8 -0] |= 0x80 >> ((9-1)%8);//主位元表 9
	Head_Frame->PrimaryBitmap[39/8 -0] |= 0x80 >> ((39-1)%8);//主位元表 39
	Head_Frame->PrimaryBitmap[41/8 -0] |= 0x80 >> ((41-1)%8);//主位元表 41
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63
	

//填充业务数据内容
	memcpy(&Car_lock_Data_Respone_Frame->User_Card[0], &User_Card[0], sizeof(Car_lock_Data_Respone_Frame->User_Card));//充电终端序列号

	memcpy(&Car_lock_Data_Respone_Frame->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(Car_lock_Data_Respone_Frame->ter_sn));//充电终端序列号
	Car_lock_Data_Respone_Frame->falg[0] = '0';
	if(falg == 0){
	  Car_lock_Data_Respone_Frame->falg[1] = '0';
	}else if(falg == 1){
	  Car_lock_Data_Respone_Frame->falg[1] = '1';
	}else if(falg == 2){
	  Car_lock_Data_Respone_Frame->falg[1] = '2';
	}else if(falg == 3){
	  Car_lock_Data_Respone_Frame->falg[1] = '3';
	}else if(falg == 4){
	  Car_lock_Data_Respone_Frame->falg[1] = '4';
	}else if(falg == 5){
	  Car_lock_Data_Respone_Frame->falg[1] = '5';
	}else if(falg == 6){
	  Car_lock_Data_Respone_Frame->falg[1] = '6';
	}else if(falg == 7){
	  Car_lock_Data_Respone_Frame->falg[1] = '7';
	}else if(falg == 8){
	  Car_lock_Data_Respone_Frame->falg[1] = '8';
	}
  MD5_DES(&Car_lock_Data_Respone_Frame->User_Card[0], sizeof(ISO8583_Car_lock_Data_Respone_Frame)-sizeof(Car_lock_Data_Respone_Frame->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&Car_lock_Data_Respone_Frame->MD5[0], &MD5[0], 8);
	len = ISO8583_Sent_Data(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_Car_lock_Data_Respone_Frame)), 0);
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_Car_lock_Data_Respone_Frame))){//如果发送不成功
		return (-1);
	}else{
		COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}

//4.5.3地锁开关接口 命令数据帧下发
int ISO8583_Car_lock_Data_Request_Deal(int fd,unsigned int length,unsigned char *buf)
{
	int Resflag = 0,Cmd_Type = 0;
	char User_Card[16];
	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_Car_lock_Data_Request_Frame *User_Car_lock_Request_Resp_Frame;
	Head_Frame = (ISO8583_Head_Frame *)(buf);
	User_Car_lock_Request_Resp_Frame = (ISO8583_Car_lock_Data_Request_Frame *)(buf+sizeof(ISO8583_Head_Frame));
  
	if(User_Car_lock_Request_Resp_Frame->Car_lock_NO[1] == '1'){//地锁号1
	  if(0 == dev_cfg.dev_para.car_lock_num)
		{//无地锁
			Resflag = 5;
		}
		else	
	  {
			if(User_Car_lock_Request_Resp_Frame->Cmd_Type[1] == '2')
			{//开地
				//如果地锁已经降下
				if(IsCarLockDown(0))//地锁已降下，再次收到开锁命令，需判断是否同一人
				{
					if(Globa_1->Error.CarLock_connect_lost == 1)
					{//地锁通信故障
						Resflag = 4;
					}
					else	if((0 == memcmp(&User_Car_lock_Request_Resp_Frame->User_Card[0],&Globa_1->Car_lock_Card[0], sizeof(Globa_1->Car_lock_Card))))
					{
            Resflag = 0;
					}
					else//不是同一个用户
						Resflag = 7;
				}
				else
				{
					if(Globa_1->Error.CarLock_connect_lost == 1)
					{//地锁通信故障
						Resflag = 4;
					}else	if((0 != memcmp(&User_Car_lock_Request_Resp_Frame->User_Card[0],&Globa_1->Car_lock_Card[0], sizeof(Globa_1->Car_lock_Card)))
						      &&(0 != IsParkUserIDValid(&Globa_1->Car_lock_Card[0])))
					{
             Resflag = 7;
					}
					else
					{ 
					 Resflag = 0;
						/*if(APP_ORDER_VALID == g_App_status)//已预约
						{
							if(0 == memcmp(p_ap3030_info->user_id,g_app_order_user_id,16))//地锁控制与预约充电为同一账号	
							{
								memcpy(g_car_lock_user_id[car_lock_index], p_ap3030_info->user_id,16);					
								car_lock_operation = APP_CAR_LOCK_DOWN;
							}
							else//不是同一个用户
								car_lock_failed_reason = 7;
						}
						else
						{
							memcpy(g_car_lock_user_id[car_lock_index], p_ap3030_info->user_id,16);					
							car_lock_operation = APP_CAR_LOCK_DOWN;
						}*/
					}
				}
				if(Resflag == 0)
				{//正确执行命令
					memcpy(&Globa_1->Car_lock_Card[0], &User_Car_lock_Request_Resp_Frame->User_Card[0], sizeof(Globa_1->Car_lock_Card));//先保存一下卡号
					Globa_1->Car_lockCmd_Type = 6;//开锁
				}	
		  }
			else if(User_Car_lock_Request_Resp_Frame->Cmd_Type[1] == '1')
			{//关地锁
			//如果地锁已经降下
				if(IsCarLockDown(0))//地锁已降下，收到闭锁命令,需判断为同一个用户
				{
					if(0 == memcmp(&User_Car_lock_Request_Resp_Frame->User_Card[0],&Globa_1->Car_lock_Card[0], sizeof(User_Car_lock_Request_Resp_Frame->User_Card)))//ok
					{
						if(IsCarPresent(0))
						{//有车
							Resflag = 6;//有车不能升起
						} 
						else if(Globa_1->Error.CarLock_connect_lost == 1)
						{//地锁通信故障
							Resflag = 4;
						}
					}
					else//不是同一个用户
						Resflag = 7;
				}
				else
				{
					if(IsCarPresent(0))
					{//有车
						Resflag = 6;//有车不能升起
					} 
					else if(Globa_1->Error.CarLock_connect_lost == 1)
					{//地锁通信故障
						Resflag = 4;
					}							
				}
				if(Resflag == 0)
				{//正确执行命令
					memcpy(&Globa_1->Car_lock_Card[0], &User_Car_lock_Request_Resp_Frame->User_Card[0], sizeof(Globa_1->Car_lock_Card));//先保存一下卡号
					Globa_1->Car_lockCmd_Type = 7; //关锁
				}
			}
		}
	}else if(User_Car_lock_Request_Resp_Frame->Car_lock_NO[1] == '2'){//地锁号2
		if(2 != dev_cfg.dev_para.car_lock_num)
		{//无地锁
			Resflag = 5;
		}
		else 
		{	
			if(User_Car_lock_Request_Resp_Frame->Cmd_Type[1] == '2'){//开地锁
				//如果地锁已经降下
				if(IsCarLockDown(1))//地锁已降下，再次收到开锁命令，需判断是否同一人
				{
					if(Globa_2->Error.CarLock_connect_lost == 1)
					{//地锁通信故障
						Resflag = 4;
					} 
					else	if((0 == memcmp(&User_Car_lock_Request_Resp_Frame->User_Card[0],&Globa_2->Car_lock_Card[0], sizeof(Globa_2->Car_lock_Card))))
					{
 
					}
					else{//不是同一个用户
						Resflag = 7;
					}
				}
				else
				{
					if(Globa_2->Error.CarLock_connect_lost == 1)
					{//地锁通信故障
						Resflag = 4;
					}/*else	if((0 != memcmp(&User_Car_lock_Request_Resp_Frame->User_Card[0],&Globa_2->Car_lock_Card[0], sizeof(Globa_2->Car_lock_Card))))
					{
             Resflag = 7;
					}*/
					else	if((0 != memcmp(&User_Car_lock_Request_Resp_Frame->User_Card[0],&Globa_2->Car_lock_Card[0], sizeof(Globa_2->Car_lock_Card)))
						      &&(0 != IsParkUserIDValid(&Globa_2->Car_lock_Card[0])))
					{
             Resflag = 7;
					}
					else
					{ 
					 Resflag = 0;
						/*if(APP_ORDER_VALID == g_App_status)//已预约
						{
							if(0 == memcmp(p_ap3030_info->user_id,g_app_order_user_id,16))//地锁控制与预约充电为同一账号	
							{
								memcpy(g_car_lock_user_id[car_lock_index], p_ap3030_info->user_id,16);					
								car_lock_operation = APP_CAR_LOCK_DOWN;
							}
							else//不是同一个用户
								car_lock_failed_reason = 7;
						}
						else
						{
							memcpy(g_car_lock_user_id[car_lock_index], p_ap3030_info->user_id,16);					
							car_lock_operation = APP_CAR_LOCK_DOWN;
						}*/
					}
				}
				if(Resflag == 0){//正确执行命令
					memcpy(&Globa_2->Car_lock_Card[0], &User_Car_lock_Request_Resp_Frame->User_Card[0], sizeof(Globa_2->Car_lock_Card));//先保存一下卡号
					Globa_2->Car_lockCmd_Type = 6;//开锁
				}		
			}else if(User_Car_lock_Request_Resp_Frame->Cmd_Type[1] == '1'){//关地锁
				if(IsCarLockDown(1))//地锁已降下，收到闭锁命令,需判断为同一个用户
				{
					if(0 == memcmp(&User_Car_lock_Request_Resp_Frame->User_Card[0],&Globa_2->Car_lock_Card[0], sizeof(User_Car_lock_Request_Resp_Frame->User_Card)))//ok
					{
						if(IsCarPresent(1))
						{//有车
							Resflag = 6;//有车不能升起
						} 
						else if(Globa_2->Error.CarLock_connect_lost == 1)
						{//地锁通信故障
							Resflag = 4;
						}
					}
					else//不是同一个用户
						Resflag = 7;
				}
				else
				{
					if(IsCarPresent(1))
					{//有车
						Resflag = 6;//有车不能升起
					} 
					else if(Globa_2->Error.CarLock_connect_lost == 1)
					{//地锁通信故障
						Resflag = 4;
					}							
				}
				if(Resflag == 0)
				{//正确执行命令
					memcpy(&Globa_2->Car_lock_Card[0], &User_Car_lock_Request_Resp_Frame->User_Card[0], sizeof(Globa_2->Car_lock_Card));//先保存一下卡号
					Globa_2->Car_lockCmd_Type = 7; //关锁
				}
			}
		}
	}else{//操作的地锁不存在
	  Resflag = 0x05;
	}
	ISO8583_Car_lock_Request_sent(fd,Resflag,&User_Car_lock_Request_Resp_Frame->User_Card[0]);
	return (0);
}

//下发灯箱控制时间
int ISO8583_Light_control_Request_sent(int fd,int falg)
{
	unsigned int ret,len;
	unsigned char SendBuf[512];  //发送数据区指针
	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针
	unsigned short tx_len = sizeof(ISO8583_Head_Frame) +  sizeof(ISO8583_Car_lock_Data_Respone_Frame);//
	time_t systime=0;
	struct tm tm_t;

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_Light3061_Data_Respone_Frame *Light3061_Data;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	Light3061_Data = (ISO8583_Light3061_Data_Respone_Frame *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据初始化
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'
	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_Light3061_Data_Respone_Frame))>>8;  //报文长度
	Head_Frame->SessionLen[1] = sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_Light3061_Data_Respone_Frame);
	memcpy(&Head_Frame->MessgTypeIdent[0], "3031", sizeof(Head_Frame->MessgTypeIdent));//消息标识符
	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化帧数据内容为 0x00
	Head_Frame->PrimaryBitmap[39/8 -0] |= 0x80 >> ((39-1)%8);//主位元表 39
	Head_Frame->PrimaryBitmap[41/8 -0] |= 0x80 >> ((41-1)%8);//主位元表 41
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63
	
	memcpy(&Light3061_Data->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(Light3061_Data->ter_sn));//充电终端序列号
	Light3061_Data->falg[0] = '0';
	if(falg == 0){
	  Light3061_Data->falg[1] = '0';
	}else if(falg == 1){
	  Light3061_Data->falg[1] = '1';
	}
  MD5_DES(&Light3061_Data->falg[0], sizeof(ISO8583_Light3061_Data_Respone_Frame)-sizeof(Light3061_Data->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&Light3061_Data->MD5[0], &MD5[0], 8);
	len = ISO8583_Sent_Data(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_Light3061_Data_Respone_Frame)), 0);
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_Light3061_Data_Respone_Frame))){//如果发送不成功
		return (-1);
	}else{
		COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}
int ISO8583_Light_control_Data_Request_Deal(int fd,unsigned int length,unsigned char *buf)
{
	int Resflag = 0,Cmd_Type = 0;
	int Start_time_on_hour = 0,Start_time_on_min = 0,End_time_off_hour = 0,End_time_off_min = 0;
	char tmp[3] = {0},MD5[8]= {0};
	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_Light3060_Data_Frame *Light3060_Data_Frame;
	Head_Frame = (ISO8583_Head_Frame *)(buf);
	Light3060_Data_Frame = (ISO8583_Light3060_Data_Frame *)(buf+sizeof(ISO8583_Head_Frame));
  
	if(length >= (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_Light3060_Data_Frame)))
	{//表明客户端有数据发送过来，作相应接受处理
		MD5_DES(&Light3060_Data_Frame->Light_Control_time[0], (length - sizeof(ISO8583_Head_Frame)-sizeof(Light3060_Data_Frame->MD5)), &ISO8583_App_Struct.work_key[0], &MD5[0]);
		hex_to_str(&tmp[0], &MD5[0], 8);
		if(0 == memcmp(&tmp[0], &buf[length - sizeof(Light3060_Data_Frame->MD5)], sizeof(Light3060_Data_Frame->MD5)))
		{
			sprintf(tmp, "%.2s", &Light3060_Data_Frame->Light_Control_time[0]);
			Start_time_on_hour = atoi(tmp);
			sprintf(tmp, "%.2s", &Light3060_Data_Frame->Light_Control_time[2]);
			Start_time_on_min = atoi(tmp);
			sprintf(tmp, "%.2s", &Light3060_Data_Frame->Light_Control_time[4]);
			End_time_off_hour = atoi(tmp);
			sprintf(tmp, "%.2s", &Light3060_Data_Frame->Light_Control_time[6]);
			End_time_off_min = atoi(tmp);
			if((Globa_1->Charger_param.Light_Control_Data.Start_time_on_hour != Start_time_on_hour)
				||(Globa_1->Charger_param.Light_Control_Data.Start_time_on_min != Start_time_on_min)
				||(Globa_1->Charger_param.Light_Control_Data.End_time_off_hour != End_time_off_hour)
				||(Globa_1->Charger_param.Light_Control_Data.End_time_off_min != End_time_off_min))
			{
				Globa_1->Charger_param.Light_Control_Data.Start_time_on_hour = Start_time_on_hour;
			  Globa_1->Charger_param.Light_Control_Data.Start_time_on_min = Start_time_on_min;
				Globa_1->Charger_param.Light_Control_Data.End_time_off_hour = End_time_off_hour;
				Globa_1->Charger_param.Light_Control_Data.End_time_off_min = End_time_off_min;
				System_param_save();//保存系统参数
			}
			Resflag = 0;
		}
		else //MAC
		{
			Resflag = 1;
		}
	}
	else 
  {
		Resflag = 1;
	}
	ISO8583_Light_control_Request_sent(fd,Resflag);
	return 0;
}


//停车业务数据上传   发送上行数据
int ISO8583_Car_Stop_busy_sent(int fd, ISO8583_Car_Stop_busy_Struct *busy_data)// 充电数据业务数据上报
{
	unsigned int ret,len;
	unsigned char SendBuf[512];  //发送数据区指针

	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针

	time_t systime=0;
	struct tm tm_t;

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_Car_Stop_busy_Struct *Car_Stop_busy_data;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	Car_Stop_busy_data = (ISO8583_Car_Stop_busy_Struct *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据填充
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'

	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_Car_Stop_busy_Struct))>>8;  //报文长度
	Head_Frame->SessionLen[1] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_Car_Stop_busy_Struct));
	memcpy(&Head_Frame->MessgTypeIdent[0], "1110", sizeof(Head_Frame->MessgTypeIdent));//消息标识符

	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化帧数据内容为 0x00
	Head_Frame->PrimaryBitmap[2/8 -0] |= 0x80 >> ((2-1)%8);//主位元表 2
	Head_Frame->PrimaryBitmap[3/8 -0] |= 0x80 >> ((3-1)%8);//主位元表 3
  Head_Frame->PrimaryBitmap[8/8 -1] |= 0x80 >> ((8-1)%8);//主位元表 8
	Head_Frame->PrimaryBitmap[11/8 -0] |= 0x80 >> ((11-1)%8);//主位元表 11
	Head_Frame->PrimaryBitmap[12/8 -0] |= 0x80 >> ((12-1)%8);//主位元表 11
	Head_Frame->PrimaryBitmap[13/8 -0] |= 0x80 >> ((13-1)%8);//主位元表 11
	Head_Frame->PrimaryBitmap[14/8 -0] |= 0x80 >> ((14-1)%8);//主位元表 11
	Head_Frame->PrimaryBitmap[15/8 -0] |= 0x80 >> ((15-1)%8);//主位元表 11

	Head_Frame->PrimaryBitmap[30/8 -0] |= 0x80 >> ((30-1)%8);//主位元表 30
	Head_Frame->PrimaryBitmap[41/8 -0] |= 0x80 >> ((41-1)%8);//主位元表 41
	Head_Frame->PrimaryBitmap[62/8 -0] |= 0x80 >> ((62-1)%8);//主位元表 62
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63

	//填充业务数据内容
	memcpy(&Car_Stop_busy_data->User_Card[0], &busy_data->User_Card[0], sizeof(ISO8583_Car_Stop_busy_Struct));//复制业务数据记录内容
	memcpy(&Car_Stop_busy_data->ter_sn[0], &Globa_1->Charger_param.SN[0], sizeof(Car_Stop_busy_data->ter_sn));//重新填充桩号，以免桩号改变以前的交易记录上传失败

	systime = time(NULL);        //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
	sprintf(&tmp[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
	memcpy(&Car_Stop_busy_data->time[0], &tmp[0], sizeof(Car_Stop_busy_data->time));//终端时间（格式yyyyMMddHHmmss）	62	N	N14

	MD5_DES(&Car_Stop_busy_data->User_Card[0], sizeof(ISO8583_Car_Stop_busy_Struct)-sizeof(Car_Stop_busy_data->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&Car_Stop_busy_data->MD5[0], &MD5[0], 8);

	len = ISO8583_Sent_Data(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_Car_Stop_busy_Struct)), 0);
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_Car_Stop_busy_Struct))){//如果发送不成功
		return (-1);
	}else{
		COMM_RUN_LogOut("send 消息标识符 %.4s:",Head_Frame->MessgTypeIdent);  
		return (0);
	}
}

//充电数据业务数据上报
int ISO8583_Car_Stop_Busy_Deal(unsigned int length,unsigned char *buf)
{
	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_Car_Stop_Resp_Frame *AP_Resp_Frame;

	Head_Frame = (ISO8583_Head_Frame *)(buf);
	AP_Resp_Frame = (ISO8583_Car_Stop_Resp_Frame *)(buf+sizeof(ISO8583_Head_Frame));
	
	if(length == (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_Car_Stop_Resp_Frame))){//表明客户端有数据发送过来，作相应接受处理
		if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "1111", sizeof(Head_Frame->MessgTypeIdent))){
			if((AP_Resp_Frame->flag[0] == '0')&&((AP_Resp_Frame->flag[1] == '0')||(AP_Resp_Frame->flag[1] == '5')))
			{
				if(ISO8583_App_Struct.ISO8583_Car_Stop_Need_Up_falg == 1)
				{
				   ISO8583_App_Struct.ISO8583_Car_Stop_Need_Up_falg = 2;
				}
				return (0);
			}else if((AP_Resp_Frame->flag[0] == '0')&&((AP_Resp_Frame->flag[1] == '4')||(AP_Resp_Frame->flag[1] == '3')||(AP_Resp_Frame->flag[1] == '1')||(AP_Resp_Frame->flag[1] == '2')||(AP_Resp_Frame->flag[1] == '6'))){
			  if(ISO8583_App_Struct.ISO8583_Car_Stop_Need_Up_falg == 1)
			  {
				   ISO8583_App_Struct.ISO8583_Car_Stop_Need_Up_falg = 3;
				}
			  return (1);
			}
			else
			{
			  ISO8583_App_Struct.ISO8583_Car_Stop_Need_Up_falg = 0;
			}
		}
	}
	return (-1);
}


//解析数据
int ISO8583_Rcev_Data_Parse(int fd,char *buf,int length)
{
	ISO8583_Head_Frame  *Head_Frame;
	unsigned short fcs_check = 0,fcs_rxd  = 0;
	
	char tmp[1000]={0};
	int len=0,ret=0,j=0,i=0;
	static Rcev_Data_count  = 0;
	Head_Frame = (ISO8583_Head_Frame *)(buf);
	len|=Head_Frame->SessionLen[0]<<8;
	len|=Head_Frame->SessionLen[1]; //确定帧的长度
	Rcev_Data_count++;
	if(DEBUGLOG) COMM_RUN_LogOut("Rcev_Data 帧头计算len:%d 帧原始长度length:%d MessgTypeIdent = %x %x %x %x",len,length,Head_Frame->MessgTypeIdent[0],Head_Frame->MessgTypeIdent[1],Head_Frame->MessgTypeIdent[2],Head_Frame->MessgTypeIdent[3]);  

	if(Globa_1->Charger_param.Serial_Network == 1){
		if((buf[0] == 0xAA) && (buf[1] == 0x55) && (buf[2] == 0xAA) && (buf[3] == 0x55)){//帧头匹配
			 len = 14;
			 fcs_rxd = (buf[len-1]<<8) | buf[len-2];

			 fcs_check = pppfcs16(0xFFFF,buf,4);//头的校验
			 fcs_check = pppfcs16(fcs_check,&buf[5],len-7);//剩余字节的校验,跳过命令字和CRC
			 fcs_check ^= 0xFFFF;
			if( fcs_check == fcs_rxd)//fcs ok			
			{
				Globa_1->Signals_value_No = (((buf[11]*100)/31)+19)/20;
			// printf("接收到信号数据 %d %d\n",Globa_1->Signals_value_No,buf[11]);

			}
			//printf("接收到信号数据\n");

			if(length - len<=0){
				Rcev_Data_count = 0;
			 // printf("I解析数据111=%d \n",length - len);
				return ret;
			}else{
			 return ISO8583_Rcev_Data_Parse(fd,&buf[len],length - len); //进行粘包处理
			}
		}
	}
	
	if(len > (16*sizeof(char)+sizeof(ISO8583_Head_Frame))){
		//time(&Rcev_current_now);
		//Rcev_pre_time = Rcev_current_now;
		if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "2021", sizeof(Head_Frame->MessgTypeIdent))){//签到数据处理
			time(&Rcev_current_now);
		  Rcev_pre_time = Rcev_current_now;
			memcpy(tmp,buf,len);
			ret = ISO8583_Random_Deal(len, tmp); //签到信息
		}else if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "2031", sizeof(Head_Frame->MessgTypeIdent))){//签到数据处理
			time(&Rcev_current_now);
		  Rcev_pre_time = Rcev_current_now;
			memcpy(tmp,buf,len);
			ret = ISO8583_Key_Deal(len, tmp); //获取工作密钥信息
		}else if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "1021", sizeof(Head_Frame->MessgTypeIdent))){//心跳数据
		  //time(&Rcev_current_now);
		  //Rcev_pre_time = Rcev_current_now;
			memcpy(tmp,buf,len);
			ret=ISO8583_Heart_Socket_Rece_Deal(len, tmp); //处理心跳数据
		}else if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "3010", sizeof(Head_Frame->MessgTypeIdent))){//APP数据
			//time(&Rcev_current_now);
		 // Rcev_pre_time = Rcev_current_now;
			memcpy(tmp,buf,len);
			ret = ISO8583_Heart_APP_Rece_Deal(len, tmp); //接收到后台APP启动信息
		}else if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "4041", sizeof(Head_Frame->MessgTypeIdent))){//服务器告警处理数据
			time(&Rcev_current_now);
		  Rcev_pre_time = Rcev_current_now;
			memcpy(tmp,buf,len);
			if(0 == memcmp(&tmp[sizeof(ISO8583_Head_Frame)], "00", 2*sizeof(char))){//判断是否处理成功
				ISO8583_App_Struct.ISO8583_Alarm_Rcev_falg = 1;//收到服务器反馈回来的数据
			}
		}else if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "1121", sizeof(Head_Frame->MessgTypeIdent))){//接收到服务器反馈回来的卡片鉴权信息
			time(&Rcev_current_now);
		  Rcev_pre_time = Rcev_current_now;
			memcpy(tmp,buf,len);
			ret = ISO8583_CARD_1121_Rece_Deal(len,tmp); //返回卡片鉴权结果
		}else if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "1131", sizeof(Head_Frame->MessgTypeIdent))){//企业充电企业号充电卡启动成功上报接口
			time(&Rcev_current_now);
		  Rcev_pre_time = Rcev_current_now;
			memcpy(tmp,buf,len);
			if(Globa_1->Enterprise_Card_flag == 1){
			  if(buf[sizeof(ISO8583_Head_Frame) + 1] ==  '0'){//成功
					Globa_1->Enterprise_Card_check_result = 1;
				}else if(buf[sizeof(ISO8583_Head_Frame) + 1] ==  '1'){//MAC校验错误
					Globa_1->Enterprise_Card_check_result = 2;
				}else if(buf[sizeof(ISO8583_Head_Frame) + 1] ==  '2'){//终端未对时
					Globa_1->Enterprise_Card_check_result = 2;
				}else if(buf[sizeof(ISO8583_Head_Frame) + 1] ==  '3'){//该卡未进行发卡
					Globa_1->Enterprise_Card_check_result = 3;
				}else if(buf[sizeof(ISO8583_Head_Frame) + 1] ==  '4'){//无效充电桩序列号
					Globa_1->Enterprise_Card_check_result = 2;
				}else if(buf[sizeof(ISO8583_Head_Frame) + 1] ==  '5'){//黑名单
					Globa_1->Enterprise_Card_check_result = 4;
				}else if(buf[sizeof(ISO8583_Head_Frame) + 1] ==  '6'){//黑名单
					Globa_1->Enterprise_Card_check_result = 4;
				}else if(buf[sizeof(ISO8583_Head_Frame) + 1] ==  '7'){//黑名单
					Globa_1->Enterprise_Card_check_result = 4;
				}else if(buf[sizeof(ISO8583_Head_Frame) + 1] ==  '8'){//黑名单
					Globa_1->Enterprise_Card_check_result = 4;
				}
				Globa_1->Enterprise_Card_flag = 0;
			}else if(Globa_2->Enterprise_Card_flag == 1){
				if(buf[sizeof(ISO8583_Head_Frame) + 1] ==  '0'){//成功
					Globa_2->Enterprise_Card_check_result = 1;
				}else if(buf[sizeof(ISO8583_Head_Frame) + 1] ==  '1'){//MAC校验错误
					Globa_2->Enterprise_Card_check_result = 2;
				}else if(buf[sizeof(ISO8583_Head_Frame) + 1] ==  '2'){//终端未对时
					Globa_2->Enterprise_Card_check_result = 2;
				}else if(buf[sizeof(ISO8583_Head_Frame) + 1] ==  '3'){//该卡未进行发卡
					Globa_2->Enterprise_Card_check_result = 3;
				}else if(buf[sizeof(ISO8583_Head_Frame) + 1] ==  '4'){//无效充电桩序列号
					Globa_2->Enterprise_Card_check_result = 2;
				}else if(buf[sizeof(ISO8583_Head_Frame) + 1] ==  '5'){//黑名单
					Globa_2->Enterprise_Card_check_result = 4;
				}else if(buf[sizeof(ISO8583_Head_Frame) + 1] ==  '6'){//黑名单
					Globa_2->Enterprise_Card_check_result = 4;
				}else if(buf[sizeof(ISO8583_Head_Frame) + 1] ==  '7'){//黑名单
					Globa_2->Enterprise_Card_check_result = 4;
				}else if(buf[sizeof(ISO8583_Head_Frame) + 1] ==  '8'){//黑名单
					Globa_2->Enterprise_Card_check_result = 4;
				}
				Globa_2->Enterprise_Card_flag = 0;
			}
		}else if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "2011", sizeof(Head_Frame->MessgTypeIdent))){//获取后台协议中申请的流水序列号
			time(&Rcev_current_now);
		  Rcev_pre_time = Rcev_current_now;
			memcpy(tmp,buf,len);
			ret = ISO8583_Get_SN_Deal(len,tmp); //返回流水号
		}else if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "1011", sizeof(Head_Frame->MessgTypeIdent))){//获取后台协议中上传的交易记录
			time(&Rcev_current_now);
		  Rcev_pre_time = Rcev_current_now;
			memcpy(tmp,buf,len);
			ret = ISO8583_Charger_Busy_Deal(len,tmp); //返回交易记录情况
		}else if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "1041", sizeof(Head_Frame->MessgTypeIdent))){////电价更新
			time(&Rcev_current_now);
		  Rcev_pre_time = Rcev_current_now;
			memcpy(tmp,buf,len);
			ret = ISO8583_have_price_change_Deal(len,tmp); //电价更新
		}else if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "1031", sizeof(Head_Frame->MessgTypeIdent))){////电价更新
			time(&Rcev_current_now);
		  Rcev_pre_time = Rcev_current_now;
			memcpy(tmp,buf,len);
			ret = ISO8583_have_card_change_Deal(len, tmp); //黑名单更新
		}else if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "1091", sizeof(Head_Frame->MessgTypeIdent))){////电价更新
			time(&Rcev_current_now);
		  Rcev_pre_time = Rcev_current_now;
			memcpy(tmp,buf,len);
			ret = ISO8583_User_card_unlock_Request_Deal(len,tmp); //用户卡解锁
		}else if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "1101", sizeof(Head_Frame->MessgTypeIdent))){////电价更新
			time(&Rcev_current_now);
		  Rcev_pre_time = Rcev_current_now;
			memcpy(tmp,buf,len);
			ret = ISO8583_User_card_unlock_Data_Request_Deal(len,tmp); //用户卡解锁上报
		}else if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "3040", sizeof(Head_Frame->MessgTypeIdent))){//系统重启
		//	time(&Rcev_current_now);
		 // Rcev_pre_time = Rcev_current_now;
			if((buf[sizeof(ISO8583_Head_Frame)] ==  '0')&&(buf[sizeof(ISO8583_Head_Frame) + 1] ==  '1')){
				if((Globa_1->QT_Step == 0x02)||(Globa_1->QT_Step == 0x03)&&(Globa_2->QT_Step == 0x03)){
					ISO8583_3041_sent(fd,0);
					sleep(2);
					system("reboot");
				}else{
					ISO8583_3041_sent(fd,3);
				}
			}else{
				ISO8583_3041_sent(fd,3);
			}
		}else if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "3050", sizeof(Head_Frame->MessgTypeIdent))){//读电表数据
  		//time(&Rcev_current_now);
		  //Rcev_pre_time = Rcev_current_now;
			memcpy(tmp,buf,len);
		 // printf("xxxxxx 请求32222 接收到系统读电表数据指令\n");
			ret = ISO8583_AP3050_Request_Data_Request_Deal(fd,len,tmp);//解析升级文件之后程序
		}else if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "1061", sizeof(Head_Frame->MessgTypeIdent))){//请求升级包反馈
  		time(&Rcev_current_now);
		  Rcev_pre_time = Rcev_current_now;
			memcpy(tmp,buf,len);
			ret = ISO8583_AP1060_Request_Data_Request_Deal(len,tmp);//解析升级文件之后程序
		}else if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "1071", sizeof(Head_Frame->MessgTypeIdent))){
			time(&Rcev_current_now);
		  Rcev_pre_time = Rcev_current_now;
			if((buf[sizeof(ISO8583_Head_Frame)] == '0')&&(buf[sizeof(ISO8583_Head_Frame)+1]== '0')){ //程序升级成功
				ISO8583_App_Struct.Software_Request_flag = 1;
				ret = 0;
			}
  	}else if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "3030", sizeof(Head_Frame->MessgTypeIdent))){//控制地锁
		  time(&Rcev_current_now);
		  Rcev_pre_time = Rcev_current_now;
			memcpy(tmp,buf,len);
			ret = ISO8583_Car_lock_Data_Request_Deal(fd,len,tmp);//解析升级文件之后程序
  	}else if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "3060", sizeof(Head_Frame->MessgTypeIdent))){//灯箱时间控制
		  time(&Rcev_current_now);
		  Rcev_pre_time = Rcev_current_now;
			memcpy(tmp,buf,len);
			ret = ISO8583_Light_control_Data_Request_Deal(fd,len,tmp);//解析升级文件之后程序
  	}else if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "1111", sizeof(Head_Frame->MessgTypeIdent)))
		{//获取后台协议中上传的交易记录---停车业务数反馈
			time(&Rcev_current_now);
		  Rcev_pre_time = Rcev_current_now;
			memcpy(tmp,buf,len);
			ret = ISO8583_Car_Stop_Busy_Deal(len,tmp); //返回停车业务交易记录情况
		}
		else{//其他数据帧，丢弃，不当做连接有问题而断开连接
			return (0);
		}
	}else if(len == 0){
		//have_hart_outtime_count ++;
		return 0;
	}	
	memset(tmp,0,sizeof(tmp));
	if(Rcev_Data_count > 3){//防止一直调用，最大粘包数量为3
		Rcev_Data_count = 0;
		return 0;
	}
  if(length - len<=0){
		Rcev_Data_count = 0;
		//printf("I解析数据111=%d \n",length - len);
    return ret;
  }else{
   return ISO8583_Rcev_Data_Parse(fd,&buf[len],length - len); //进行粘包处理
  }
}

//心跳连接以及告警数据接收处理
int ISO8583_Rcev_Socket_Deal(int fd)
{
	int j,ret;
	struct timeval tv1;
	fd_set fdsr;
	unsigned char buf[1024];
	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针

	ISO8583_Head_Frame  *Head_Frame;
  ISO8583_Rcev_Alarm_Frame  *Rcev_Alarm_Frame;
	
	Head_Frame = (ISO8583_Head_Frame *)(buf);
  Rcev_Alarm_Frame = (ISO8583_Rcev_Alarm_Frame *)(buf+sizeof(ISO8583_Head_Frame));
	
	
		if(Globa_1->Charger_param.Serial_Network == 0){
		FD_ZERO(&fdsr); //每次进入循环都重建描述符集
		FD_SET(fd, &fdsr);
		tv1.tv_sec = 10;
		tv1.tv_usec = 0;
		ret = select(fd + 1, &fdsr, NULL, NULL, &tv1);
		if(ret > 0){//接收到上位机响应
			if(FD_ISSET(fd, &fdsr)){
				memset(&buf[0], 0x00, sizeof(buf));
				ret = recv(fd, buf, sizeof(buf), 0);
			}
		}
	}else{
    ret = read_datas_tty(fd, buf  , 1024, 10000000, 300000);
	}
	
#if ISO8583_DEBUG 
	if(ret > 0){
		printf("接收到后台数据 length = %d,data= ", ret);
		for(j=0; j<ret; j++)printf("%02x ",buf[j]);
		printf("\n");
	}
#endif 
	
  if((ret >= 14)){//表明客户端有 心跳数据 发送过来
		have_hart_outtime_count = 0;
		return ISO8583_Rcev_Data_Parse(fd,buf,ret);  //进行数据处理，防止粘包的发送
  }else if(ret == 0){//超时
	  //have_hart_outtime_count ++;
  	return (0);
  }
	return (-1);
}


//所有的后台数据处理。--同一个连接的
int ISO8583_Rcev_Task(void)
{
	int ret;
	prctl(PR_SET_NAME,(unsigned long)"ISO8583_Rcev_Task");//设置线程名字 

	while(1){
		usleep(100000);
		ISO8583_Busy_Watchdog = 1;
		if(ISO8583_App_Struct.Heart_TCP_state >= 0x02){//表示创建了连接
			time(&Rcev_current_now);
		  ret = ISO8583_Rcev_Socket_Deal(ISO8583_App_Struct.Heart_Socket_fd); //接收所有的命令有些是中心反馈给桩的信号，有些是中心的命令
			if(ret < 0){//连接不正常
			if(ISO8583_DEBUG)printf("ISO8583_Heart_Task ISO8583_Rcev_Socket_Deal fail\n");
				//close(ISO8583_App_Struct.Heart_Socket_fd);
			//	ISO8583_App_Struct.Heart_TCP_state = 0x01;
				//Globa_1->have_hart = 0;
			}else if (ret == 0){//没有收到心跳、告警应答
				/*if(have_hart_outtime_count >= 120){//120s还是没有数据这显示离线状态，没收到心跳应答
					if(Globa_1->Charger_param.Serial_Network == 1){//直接走c串口所以不需要初始化
						ISO8583_App_Struct.Heart_TCP_state = 0x00;
						Globa_1->have_hart = 0;
						have_hart_outtime_count = 0;
						sleep(5);
					}else{
						have_hart_outtime_count = 0;
						close(ISO8583_App_Struct.Heart_Socket_fd);
						ISO8583_App_Struct.Heart_TCP_state = 0x01;
						Globa_1->have_hart = 0;
					}
				}*/			 
			}
			if(ISO8583_DEBUG) printf("have_hart_outtime_count length = %d = %d\n", have_hart_outtime_count,abs(Rcev_current_now - Rcev_pre_time ));

			if(abs(Rcev_current_now - Rcev_pre_time ) >= 120){
				if(Globa_1->Charger_param.Serial_Network == 1){//直接走c串口所以不需要初始化
					ISO8583_App_Struct.Heart_TCP_state = 0x00;
					Globa_1->have_hart = 0;
					have_hart_outtime_count = 0;
					sleep(5);
				}else{
					have_hart_outtime_count = 0;
					close(ISO8583_App_Struct.Heart_Socket_fd);
					ISO8583_App_Struct.Heart_TCP_state = 0x01;
					Globa_1->have_hart = 0;
					//Globa_1->Signals_value_No = 0;
				}
			}			
		}else{
			time(&Rcev_current_now);
			Rcev_pre_time = Rcev_current_now;
			have_hart_outtime_count = 0;
		  sleep(5);
		}
	}
}


//**********************定时电价更新条件
int IS_Regular_price_update()
{
	time_t systime=0,log_time =0,current_time,update_time;
	struct tm tm_t;
	//systime = time(NULL);         //获得系统的当前时间 
	//localtime_r(&systime, &tm_t);  //结构指针指向当前时间	

	if(((Globa_1->QT_Step == 2)||(Globa_1->QT_Step == 3))&&(Globa_2->QT_Step == 3)&&(Globa_1->charger_fixed_price.falg == 1)&&(Globa_1->charger_fixed_price.fixed_price_effective_time[0] != 0)\
	  &&(Globa_1->charger_fixed_price.fixed_price_effective_time[1] != 0)&&(Globa_1->charger_fixed_price.fixed_price_effective_time[2] != 0)&&(Globa_1->charger_fixed_price.fixed_price_effective_time[3] != 0))
	{
		tm_t.tm_year = (Globa_1->charger_fixed_price.fixed_price_effective_time[0]*100 + Globa_1->charger_fixed_price.fixed_price_effective_time[1]) -1900;
		tm_t.tm_mon = (Globa_1->charger_fixed_price.fixed_price_effective_time[2]) - 1;
		
		tm_t.tm_mday = (Globa_1->charger_fixed_price.fixed_price_effective_time[3]);
		
		tm_t.tm_hour = (Globa_1->charger_fixed_price.fixed_price_effective_time[4]);

		tm_t.tm_min = (Globa_1->charger_fixed_price.fixed_price_effective_time[5]);
		tm_t.tm_sec = (Globa_1->charger_fixed_price.fixed_price_effective_time[6]);

		update_time = mktime(&tm_t);
		systime = time(NULL);         //获得系统的当前时间 

		if(systime > update_time){
			printf("UART1 月 from update_time  =%d =%d\n ", update_time,systime);
			return 1;
		}
	}
	return 0;
}

void Regular_price_update()//进行定时电价更新
{
	int i=0;
	for(i=0;i<7;i++)
	  Globa_1->charger_fixed_price.fixed_price_effective_time[i] = 0;
	
	Globa_1->charger_fixed_price.falg = 0;            //存在定时电价，需要更新
	Globa_1->Charger_param.Serv_Type = Globa_1->charger_fixed_price.Serv_Type;  //服务费收费类型 1：按次收费；2：按时间收费(单位：分/10分钟；3：按电量
  Globa_1->Charger_param.Serv_Price = Globa_1->charger_fixed_price.Serv_Price;              //服务费,精确到分 默认安电量收
  Globa_1->Charger_param.Price = Globa_1->charger_fixed_price.Price;                   //单价,精确到分
	for(i=0;i<4;i++){
	  Globa_1->Charger_param2.share_time_kwh_price[i] =  Globa_1->charger_fixed_price.share_time_kwh_price[i];
	  Globa_2->Charger_param2.share_time_kwh_price[i] =  Globa_1->charger_fixed_price.share_time_kwh_price[i];
		Globa_1->Charger_param2.share_time_kwh_price_serve[i] = Globa_1->charger_fixed_price.share_time_kwh_price_serve[i]; //分时服务电价
		Globa_2->Charger_param2.share_time_kwh_price_serve[i] = Globa_1->charger_fixed_price.share_time_kwh_price_serve[i]; //分时服务电价
	}
  Globa_1->Charger_param2.time_zone_num = Globa_1->charger_fixed_price.time_zone_num;  //时段数
	for(i=0;i<12;i++){
	  Globa_1->Charger_param2.time_zone_tabel[i] = Globa_1->charger_fixed_price.time_zone_tabel[i];//时段表
		Globa_1->Charger_param2.time_zone_feilv[i] = Globa_1->charger_fixed_price.time_zone_feilv[i];//时段表位于的价格
		Globa_2->Charger_param2.time_zone_feilv[i] = Globa_1->charger_fixed_price.time_zone_feilv[i];//时段表位于的价格
		Globa_2->Charger_param2.time_zone_feilv[i] = Globa_1->charger_fixed_price.time_zone_feilv[i];//时段表位于的价格
	}
	Globa_1->Charger_param2.show_price_type = Globa_1->charger_fixed_price.show_price_type;//电价显示方式 0--详细显示 1--总电价显示
	Globa_1->Charger_param2.IS_fixed_price = Globa_1->charger_fixed_price.IS_fixed_price;
	Globa_2->Charger_param2.show_price_type = Globa_1->charger_fixed_price.show_price_type;//电价显示方式 0--详细显示 1--总电价显示
	Globa_2->Charger_param2.IS_fixed_price = Globa_1->charger_fixed_price.IS_fixed_price;

	System_param_save();
	System_param2_save();
	System_param_Fixedpirce_save();
}
int ISO8583_Random_desleep(int COUNT)
{
	int i =0 ;
	for (i = 0;i< COUNT;i++)
	{
	  sleep(1);
	  time(&Rcev_current_now);
		Rcev_pre_time = Rcev_current_now;
    ISO8583_Heart_Watchdog = 1;
		if(ISO8583_App_Struct.Heart_TCP_state != 0x02)
			break;
	}
}
//主要逻辑控制交互
int ISO8583_Heart_Task(void)
{
	unsigned int up_num = 0;//标记数据的上传记录次数
	char read_buf[1024], temp_char[1024],dtu_rxdata[100]={0};
	int ret = 0,Busy_Need_Up_id = -1,id = -1,Car_Stop_Busy_Need_Up_id = -1;
	int count1= 0,count2= 0,Heart_TCP_state = 0,Software_upate_TCP_state = 0;
	int Alarm_Send_flag = 0;//0-- 发送初始告警(即全部清除以前的告警信息-上电的时候)  1--发送实时告警信息
	UINT32 data_block_num_want = 1,Hart_time_Value = 5;
	
	unsigned char dtu_send_buf[200],dtu_rx_data[200];
	unsigned char result = 0,dtu_rx_len = 0,Busy_SN_TYPE = 0;
	int tx_len = 0,i=0;	
	unsigned char g_set_ip_req = 1,Battery_Parameter_Flag_1 = 0,Battery_Parameter_Flag_2 = 0,BMS_gun_no = 0;
	unsigned char InDTU332W_Cfg_st = LOGIN_InDTU332W_ST;
	UINT32 have_hart_outtime_count =0,Domain_Name_falg = 0;
	unsigned char InDTU332W_cfg_wait_ack=0;
	unsigned char Update_EASE_DC_CTL_falg = 0;
	unsigned char Update_Electric_Charge_Pile_falg = 0;
	unsigned char Update_Charger_falg =0;
	char buf[100] = {0}, buf2[100]= {0}, md5[100]= {0};
 
  Busy_dataMsgNode *Busy_dataMsgList=NULL;
							
	UINT32 g_InDTU332W_conn_count = 0,GET_InDTU332W_CSQ_ST_Count = 0;
	UINT32 ISO8583_Random_Count = 0;
	ISO8583_App_Struct.Heart_TCP_state = 0x01;//初始化
	
	ISO8583_alarm_clear();
	ISO8583_App_Struct.Heart_Socket_fd = -1;
	Globa_1->have_hart = 0;
  have_hart_outtime_count = 0;
	ISO8583_alarm_set();
	ISO8583_alarm_get(&alarm_info_get);
	time_t current_now,pre_hart_time,pre_time,AP_Sent_time,Alarm_Sent_time,pre_card_unlock_Data_time,pre_card_unlock_time;
	time_t pre_Busy_SN_time,pre_Busy_Need_Up_time,pre_price_change_time,pre_card_change_time,pre_delete_over_time,Enterprise_Card_Sent,User_Card_Sent;
  time_t AP1070_Request_over_time,Software_upate_over_time,Signals_value_over_time = 0,BMS_time = 0,Busy_SN_start_time = 0,Busy_Need_Up_start_time = 0;
	time_t Car_Stop_Busy_Need_Up_start_time;
	time_t pre_Car_Busy_Need_Up_time ;
	time(&current_now);
	pre_hart_time = current_now;
	pre_time = current_now;
	Alarm_Sent_time = current_now;
	Enterprise_Card_Sent = current_now;
	User_Card_Sent = current_now;
	ISO8583_App_Struct.invalid_card_bag_now = 1;
	
	if(Globa_1->Charger_param.Serial_Network == 0){//走网口口模式
		ISO8583_App_Struct.Heart_TCP_state = 0x01;
	}else if(Globa_1->Charger_param.Serial_Network == 1){//走串口模式
		ISO8583_App_Struct.Heart_Socket_fd = OpenDev(DTU_COM_ID);
		if(ISO8583_App_Struct.Heart_Socket_fd == -1) {
			while(1);
		}else{
			printf("topic is -----------0000-----Globa_1->Charger_param.DTU_Baudrate = %d \n",Globa_1->Charger_param.DTU_Baudrate);
			if(Globa_1->Charger_param.DTU_Baudrate == 0){
			  set_speed(ISO8583_App_Struct.Heart_Socket_fd, 2400);	
			}else if(Globa_1->Charger_param.DTU_Baudrate == 1){
			  set_speed(ISO8583_App_Struct.Heart_Socket_fd, 4800);	
			}else if(Globa_1->Charger_param.DTU_Baudrate == 2){
			  set_speed(ISO8583_App_Struct.Heart_Socket_fd, 9600);	
			}else if(Globa_1->Charger_param.DTU_Baudrate == 3){
			  set_speed(ISO8583_App_Struct.Heart_Socket_fd, 19200);	
			}else{
		    set_speed(ISO8583_App_Struct.Heart_Socket_fd, 115200);
			}
			set_Parity(ISO8583_App_Struct.Heart_Socket_fd, 8, 1, 0);
			close(ISO8583_App_Struct.Heart_Socket_fd);
		}
		ISO8583_App_Struct.Heart_Socket_fd = OpenDev(DTU_COM_ID);
		if(ISO8583_App_Struct.Heart_Socket_fd == -1) {
			while(1);
		}
		UINT32 ipaddr = 0;
		if(strstr(Globa_1->ISO8583_NET_setting.ISO8583_Server_IP,".com") == NULL){//说明：返回指向第一次出现str2位置的指针，如果没找到则返回NULL
			ipaddr = inet_addr(Globa_1->ISO8583_NET_setting.ISO8583_Server_IP);
			if(ISO8583_DEBUG) printf("topic is -----------0000-----ipaddr = %d \n",ipaddr);
			Domain_Name_falg  = 0;//IP访问
		}else{
			ipaddr = 0;  //域名访问
			Domain_Name_falg  = 1;//域名访问
			if(ISO8583_DEBUG) 	printf("topic is ---域名访问 = %s \n",Globa_1->ISO8583_NET_setting.ISO8583_Server_IP);
			server_name = Globa_1->ISO8583_NET_setting.ISO8583_Server_IP;
			//memcpy(server_name,Globa_1->ISO8583_NET_setting.ISO8583_Server_IP,strlen(Globa_1->ISO8583_NET_setting.ISO8583_Server_IP));
		}
		hexip[3] = ipaddr>>24;
		hexip[2] = ipaddr>>16;
		hexip[1] = ipaddr>>8;
		hexip[0] = ipaddr;
		hexport[0] = Globa_1->ISO8583_NET_setting.ISO8583_Server_port[0];
		hexport[1] = Globa_1->ISO8583_NET_setting.ISO8583_Server_port[1];
		ISO8583_App_Struct.Heart_TCP_state = 0x00;
		if(DEBUGLOG) COMM_RUN_LogOut("DTU访问模式:IP = %x %x %x %x ",hexip[0],hexip[1],hexip[2],hexip[3]);  
	}
	
	pthread_mutex_lock(&busy_db_pmutex);
	Set_0XB4_Busy_ID();
	if (dev_cfg.dev_para.car_lock_num != 0)
	{
	  Set_CAR_CLEAR_Busy_ID();
	}
	pthread_mutex_unlock(&busy_db_pmutex);
	Globa_1->have_price_change = 1;
	prctl(PR_SET_NAME,(unsigned long)"ISO8583_Heart_Task");//设置线程名字 

  Busy_dataMsgList=calloc(1,sizeof(Busy_dataMsgNode));
  Busy_dataMsgList->count=0;
  INIT_LIST_HEAD(&Busy_dataMsgList->glist);
  pthread_mutex_init(&Busy_dataMsgList->lock,NULL);


	while(1){
	//	usleep(100000);//10ms
		sleep(1);
		ISO8583_Heart_Watchdog = 1;
	  time(&current_now);//获取系统当前时间

		//6:用户卡解锁请求接口
		if(Globa_1->User_Card_Lock_Info.User_card_unlock_Requestfalg == 1){//从后台或者本地查找有没有该记录的锁卡情况
			if(Globa_1->have_hart == 1){//后台通讯正常则从后台拿数据，反之直接从本地拿数据
				if(ISO8583_App_Struct.Heart_TCP_state == 0x04){
					if((abs(current_now - pre_card_unlock_time) >= 10)){ //3秒一次查询流水号
						pre_card_unlock_time = current_now;
						ISO8583_App_Struct.Card_unlock_RequestRcev_falg = 0;
						ret =  ISO8583_User_Card_Unlock_Request_sent(ISO8583_App_Struct.Heart_Socket_fd);//发送电价
						if(ret == -1){//获取失败
							if(ISO8583_DEBUG)printf("ISO8583_Get_SN_Deal failed\n");
							close(ISO8583_App_Struct.Heart_Socket_fd);
					    ISO8583_App_Struct.Heart_TCP_state = 0x01;
							sleep(5);
						}
					}
				}
			}else{//查询数据库查找该记录信息
				pthread_mutex_lock(&busy_db_pmutex);
				Select_Unlock_Info(); 
				pthread_mutex_unlock(&busy_db_pmutex);
				Globa_1->User_Card_Lock_Info.User_card_unlock_Requestfalg = 0;
			}
		}

	 //7用户卡解锁数据上报接口
		if(Globa_1->User_Card_Lock_Info.User_card_unlock_Data_Requestfalg == 1){//从后台或者本地查找有没有该记录的锁卡情况
			if(Globa_1->have_hart == 1){//后台通讯正常则从后台拿数据，反之直接从本地拿数据
				if(ISO8583_App_Struct.Heart_TCP_state == 0x04){
					if(abs(current_now - pre_card_unlock_Data_time) >= 10){ //3秒一次查询流水号
						pre_card_unlock_Data_time = current_now;
						ret =  ISO8583_User_Card_Unlock_Data_Request_sent(ISO8583_App_Struct.Heart_Socket_fd);//发送电价
					}
				}
			}else{
				pthread_mutex_lock(&busy_db_pmutex);
				if(Set_Unlock_flag(Globa_1->User_Card_Lock_Info.Local_Data_Baseid_ID) != 1){
					Globa_1->User_Card_Lock_Info.User_card_unlock_Data_Requestfalg = 0;	
				}
				pthread_mutex_unlock(&busy_db_pmutex);
			}
		}
		
    if(ISO8583_App_Struct.Heart_TCP_state != Heart_TCP_state){
			printf("ISO8583_App_Struct.Heart_TCP_state = %d\n",ISO8583_App_Struct.Heart_TCP_state);
      Heart_TCP_state = ISO8583_App_Struct.Heart_TCP_state;
		}
		
		if(IS_Regular_price_update()){//定时电价更新
		  Regular_price_update();
		 		if(ISO8583_DEBUG)  printf("定时电价更新定时电价更新 = %d\n");
		}
		//5: 填加充电交易记录限制2000条 ------------------------------------------
		
		if(abs(current_now - pre_delete_over_time) >= 10){ //3秒一次查询流水号
			pre_delete_over_time = current_now;
			pthread_mutex_lock(&busy_db_pmutex);
			delete_over_2000_busy();//删除超过2000条的最早记录（一次只删除一条记录）
			//if(ISO8583_DEBUG) printf("xxxxxxxxxx delete_over_2000_busy id = %d\n", id);
			pthread_mutex_unlock(&busy_db_pmutex);
		}
		
		switch(ISO8583_App_Struct.Heart_TCP_state){
		  case 0x00:{//初始化DTU数据
				if(!InDTU332W_cfg_wait_ack){
					switch(InDTU332W_Cfg_st){
						case LOGIN_InDTU332W_ST:
							tx_len = InDTU332W_Login_Frame(dtu_send_buf,"adm","123456");
							g_eInDTU332W_cmd = InDTU332W_LOGIN;
							printf("g_eInDTU332W_cmd=%d\n", g_eInDTU332W_cmd);
							Globa_1->Signals_value_No = 0;
							if(DEBUGLOG) COMM_RUN_LogOut("DTU 发送登录命令:g_eInDTU332W_cmd = %x InDTU332W_Cfg_st = %x ",g_eInDTU332W_cmd,InDTU332W_Cfg_st);  
							break;
						case GET_InDTU332W_CSQ_ST://读DTU连接信号强度
							GET_InDTU332W_CSQ_ST_Count ++; 
							if(GET_InDTU332W_CSQ_ST_Count >= 2)
							{
								GET_InDTU332W_CSQ_ST_Count = 0;
								InDTU332W_Cfg_st = READ_InDTU332W_ST;
								if(DEBUGLOG) COMM_RUN_LogOut("DTU 联系两次发送信号强度命令:g_eInDTU332W_cmd = %x InDTU332W_Cfg_st = %x ",g_eInDTU332W_cmd,InDTU332W_Cfg_st);  
								break;	
							}
							tx_len = InDTU332W_Status_RD_Frame(dtu_send_buf,TAG_CSQ_ST);
							g_eInDTU332W_cfg_tag = TAG_CSQ_ST;
							g_eInDTU332W_cmd = InDTU332W_RD_STATUS;
							printf("读DTU连接信号强度\n");
							if(DEBUGLOG) COMM_RUN_LogOut("DTU 发送信号强度命令:g_eInDTU332W_cmd = %x InDTU332W_Cfg_st = %x ",g_eInDTU332W_cmd,InDTU332W_Cfg_st);  
              break;	
						case GET_REGISTRATION_ST: //查看注册信息状态 
							tx_len = InDTU332W_Status_RD_Frame(dtu_send_buf,TAG_REGISTRATION_ST);
							g_eInDTU332W_cfg_tag = TAG_REGISTRATION_ST;
							g_eInDTU332W_cmd = InDTU332W_RD_STATUS;
							printf("查看注册信息状态\n");
							if(DEBUGLOG) COMM_RUN_LogOut("DTU 查看注册信息状态:g_eInDTU332W_cmd = %x InDTU332W_Cfg_st = %x ",g_eInDTU332W_cmd,InDTU332W_Cfg_st);  
              break;
						case GET_InDTU332W_ICCID_ST: //查看sim卡唯一对应的ID卡号20位
							tx_len = InDTU332W_Status_RD_Frame(dtu_send_buf,TAG_ICCID_VALUE);
							g_eInDTU332W_cfg_tag = TAG_ICCID_VALUE;
							g_eInDTU332W_cmd = InDTU332W_RD_STATUS;
							if(DEBUGLOG) COMM_RUN_LogOut("DTU 查看sim卡唯一ID:g_eInDTU332W_cmd = %x InDTU332W_Cfg_st = %x ",g_eInDTU332W_cmd,InDTU332W_Cfg_st);  
              break;
						case READ_InDTU332W_ST: //读网络IP口
							tx_len = InDTU332W_Single_RD_Frame(dtu_send_buf,TAG_GATEWAY_ADDR);
							g_eInDTU332W_cfg_tag = TAG_GATEWAY_ADDR;
							g_eInDTU332W_cmd = InDTU332W_RD_SINGLE_CFG;
							if(DEBUGLOG) COMM_RUN_LogOut("DTU 读网络IP口:g_eInDTU332W_cmd = %x InDTU332W_Cfg_st = %x ",g_eInDTU332W_cmd,InDTU332W_Cfg_st);  
							break;
						case WRTIE_InDTU332W_ST://写网络IP口
							tx_len = InDTU332W_Single_WR_Frame(dtu_send_buf,TAG_GATEWAY_ADDR);
							g_eInDTU332W_cfg_tag = TAG_GATEWAY_ADDR;
							g_eInDTU332W_cmd = InDTU332W_WR_SINGLE_CFG;
							if(DEBUGLOG) COMM_RUN_LogOut("DTU 写网络IP口:g_eInDTU332W_cmd = %x InDTU332W_Cfg_st = %x ",g_eInDTU332W_cmd,InDTU332W_Cfg_st);  
					    break;
							
						case WR_DNS_ADDR_ST://写DNS
							tx_len = InDTU332W_Single_WR_Frame(dtu_send_buf,TAG_DNS_ADDR);
							g_eInDTU332W_cfg_tag = TAG_DNS_ADDR;
							g_eInDTU332W_cmd = InDTU332W_WR_SINGLE_CFG;
							printf("888888\n");
							if(DEBUGLOG) COMM_RUN_LogOut("DTU 写DNS服务器:g_eInDTU332W_cmd = %x InDTU332W_Cfg_st = %x ",g_eInDTU332W_cmd,InDTU332W_Cfg_st);  
							break;
						case WR_ServIP_ADDR_ST://写域名
							tx_len = InDTU332W_Single_WR_Frame(dtu_send_buf,TAG_GATEWAY_NAME);
							g_eInDTU332W_cfg_tag = TAG_GATEWAY_NAME;
							g_eInDTU332W_cmd = InDTU332W_WR_SINGLE_CFG;
							printf("999999\n");
							if(DEBUGLOG) COMM_RUN_LogOut("DTU 写DNS域名:g_eInDTU332W_cmd = %x InDTU332W_Cfg_st = %x ",g_eInDTU332W_cmd,InDTU332W_Cfg_st);  
							break;
						case RESET_InDTU332W_ST:
							tx_len = InDTU332W_Reset_Frame(dtu_send_buf);							
							g_eInDTU332W_cmd = InDTU332W_RESET;
							printf("44444444\n");
							if(DEBUGLOG) COMM_RUN_LogOut("DTU 重启:g_eInDTU332W_cmd = %x InDTU332W_Cfg_st = %x ",g_eInDTU332W_cmd,InDTU332W_Cfg_st);  
							break;
						case GET_InDTU332W_CONN_ST:
							tx_len = InDTU332W_Status_RD_Frame(dtu_send_buf,TAG_GATEWAY_CONN_ST);
							g_eInDTU332W_cfg_tag = TAG_GATEWAY_CONN_ST;
							g_eInDTU332W_cmd = InDTU332W_RD_STATUS;
							printf("5555555\n");
							if(DEBUGLOG) COMM_RUN_LogOut("DTU 读取连接状态:g_eInDTU332W_cmd = %x InDTU332W_Cfg_st = %x ",g_eInDTU332W_cmd,InDTU332W_Cfg_st);  
							break;
						case ACTIVE_InDTU332W_ST:
							tx_len = InDTU332W_Active_Frame(dtu_send_buf);
							g_eInDTU332W_cmd = InDTU332W_ACTIVE;
							printf("---66666666---,tx_len =%d\n",tx_len);
							if(DEBUGLOG) COMM_RUN_LogOut("DTU 激活:g_eInDTU332W_cmd = %x InDTU332W_Cfg_st = %x ",g_eInDTU332W_cmd,InDTU332W_Cfg_st);  
							break;
						case GET_InDTU332W_CSQ_ST2://读DTU连接信号强度GET_InDTU332W_CSQ_ST2
							GET_InDTU332W_CSQ_ST_Count ++ ;
							if(GET_InDTU332W_CSQ_ST_Count >= 2)
							{
								GET_InDTU332W_CSQ_ST_Count = 0;
								InDTU332W_Cfg_st = GET_InDTU332W_CONN_ST;//先读取DTU模块是否连接到服务器，连接上以后才发协议数据
								break;	
							}
							tx_len = InDTU332W_Status_RD_Frame(dtu_send_buf,TAG_CSQ_ST);
							g_eInDTU332W_cfg_tag = TAG_CSQ_ST;
							g_eInDTU332W_cmd = InDTU332W_RD_STATUS;
							printf("222读DTU连接信号强度\n");
							if(DEBUGLOG) COMM_RUN_LogOut("DTU 再次读取信号:g_eInDTU332W_cmd = %x InDTU332W_Cfg_st = %x ",g_eInDTU332W_cmd,InDTU332W_Cfg_st);  
              break;	
						default:
							tx_len = 0;
							break;
							
					}						
					
					if(tx_len > 0){
						printf("--tx_len=%d--n.my_socket=%d-\n", tx_len,ISO8583_App_Struct.Heart_Socket_fd );
						tx_len = write(ISO8583_App_Struct.Heart_Socket_fd , dtu_send_buf, tx_len);
						printf("tx_len=%d\n", tx_len);
						for(i=0;i<tx_len;i++)
						{
							printf("dtu_send_buf[%d]=%x\n", i,dtu_send_buf[i]);
						}
						printf("tx_len=%d\n", tx_len);
						InDTU332W_cfg_wait_ack =1;
						printf("xxxxxxxxxxxxxxxxxxxxxxokokokokokokoklo");
					}
				}else{//等待响应
					dtu_rx_len = read_datas_tty(ISO8583_App_Struct.Heart_Socket_fd , dtu_rx_data  , 512, 5000000, 300000);
					printf("shoushuju");
					printf("dtu_rx_len=%d\n", dtu_rx_len);
					InDTU332W_cfg_wait_ack = 0;
					if(dtu_rx_len > 0){
						sprintf(dtu_rxdata,"DTU接收数据反馈:");
						for(i=0;i<dtu_rx_len;i++){
						  sprintf(&dtu_rxdata[strlen(dtu_rxdata)],"%0x,",dtu_rx_data[i]);
							printf("dtu_rx_data[%d]=%x\n", i,dtu_rx_data[i]);
						}
						if(DEBUGLOG) COMM_RUN_LogOut("%s",dtu_rxdata);  
						result = InDTU332W_Response_Handler(dtu_rx_data,dtu_rx_len);
						InDTU332W_cfg_wait_ack = 0;
						printf("shoudaole");
						printf("result=%d\n", result);
						if(0 == result){//响应ok
							printf("shujuOK\n");
							switch(InDTU332W_Cfg_st){
								case LOGIN_InDTU332W_ST: //登录反馈
									InDTU332W_Cfg_st = GET_InDTU332W_CSQ_ST;//READ_InDTU332W_ST;//next read gateway cfg	 进行读网关IP设置的								
									break;
								case GET_InDTU332W_CSQ_ST://进行IP读取
								  InDTU332W_Cfg_st = READ_InDTU332W_ST;
								  break;
								case READ_InDTU332W_ST:
									printf("g_InDTU332W_gateway_ip[0]=%d\n", g_InDTU332W_gateway_ip[0]);
									printf("g_InDTU332W_gateway_ip[1]=%d\n", g_InDTU332W_gateway_ip[1]);
									printf("g_InDTU332W_gateway_ip[2]=%d\n", g_InDTU332W_gateway_ip[2]);										
									printf("g_InDTU332W_gateway_ip[3]=%d\n", g_InDTU332W_gateway_ip[3]);
									printf("g_InDTU332W_gateway_port=%d\n", g_InDTU332W_gateway_port);
									//如果参数不一致，则重设
									if( (g_InDTU332W_gateway_ip[0] != hexip[0]) ||
										(g_InDTU332W_gateway_ip[1] != hexip[1]) ||
										(g_InDTU332W_gateway_ip[2] != hexip[2]) ||
										(g_InDTU332W_gateway_ip[3] != hexip[3]) ||
										(g_InDTU332W_gateway_port != ((hexport[1] << 8) + (hexport[0])) ))
									{						
												InDTU332W_Cfg_st = WRTIE_InDTU332W_ST;	//重新写网络端口								
								  }
									else//一致则不设置	
									{
										if(Domain_Name_falg == 1){//域名--需要填写域名
	                    InDTU332W_Cfg_st = WR_DNS_ADDR_ST;
										}else{//IP设置OK之后直接激活进行连接
										  InDTU332W_Cfg_st = ACTIVE_InDTU332W_ST;//先激活数据连接201708215
											//InDTU332W_Cfg_st = GET_REGISTRATION_ST;//20171204
										}
									}
									break;
								case WRTIE_InDTU332W_ST: //write ok,then restart InDTU332W
								  //InDTU332W_Cfg_st = RESET_InDTU332W_ST;									
									//break;
									if(Domain_Name_falg == 1){
                    InDTU332W_Cfg_st = WR_DNS_ADDR_ST;	
									}else{
										InDTU332W_Cfg_st = RESET_InDTU332W_ST;//ACTIVE_InDTU332W_ST;//RESET_InDTU332W_ST; 不进行重启判断，直接激活进行连接	
									}									
									break;
								case WR_DNS_ADDR_ST:// DNS设置OK
									InDTU332W_Cfg_st = WR_ServIP_ADDR_ST;//写域名地址
									printf("设置域名OK设置域名OK=%d\n", InDTU332W_Cfg_st);
									break;
								case WR_ServIP_ADDR_ST:{//设置域名OK
								  InDTU332W_Cfg_st = ACTIVE_InDTU332W_ST;//进行激活
									//InDTU332W_Cfg_st = GET_REGISTRATION_ST;
							  	printf("设置域名OK设置域名OK=%d\n", InDTU332W_Cfg_st);
								  g_InDTU332W_conn_count = 0;
	                break;
								}
								case GET_REGISTRATION_ST: //查看注册信息状态 ----目前没有可以直接激活
								  if((Globa_1->Registration_value == 0x01)||(Globa_1->Registration_value == 0x05)){
										 InDTU332W_Cfg_st = GET_InDTU332W_ICCID_ST;
							  	   printf("查看注册信息状态=%d\n", InDTU332W_Cfg_st);
										 g_InDTU332W_conn_count = 0;
									}else{
										g_InDTU332W_conn_count ++ ;
										if(g_InDTU332W_conn_count >= 120){
											InDTU332W_Cfg_st = RESET_InDTU332W_ST;//进行重启下
											g_InDTU332W_conn_count = 0;
										}
									}
									 if(DEBUGLOG) COMM_RUN_LogOut("DTU 注册信息状态反馈:Globa_1->Registration_value = %x",Globa_1->Registration_value);  
								   break;
								case GET_InDTU332W_ICCID_ST: //查看sim卡唯一对应的ID卡号20位	
							    InDTU332W_Cfg_st = ACTIVE_InDTU332W_ST;//进行激活
									printf("查看sim卡唯一对应的ID卡号20位	OK=%d\n", InDTU332W_Cfg_st);
							   	break;
								case RESET_InDTU332W_ST://进行重新启动
									InDTU332W_Cfg_st = LOGIN_InDTU332W_ST;//重新登录查看是否OK //InDTU332W_IDLE_ST;
							    sleep(5);
									//InDTU332W_cfg_start_time = g_timebase_ticks;//重新定时
									break;
								case ACTIVE_InDTU332W_ST://激活之后获取信号强度
									InDTU332W_Cfg_st = GET_InDTU332W_CSQ_ST2;//GET_InDTU332W_CONN_ST;//先读取DTU模块是否连接到服务器，连接上以后才发协议数据
									printf("InDTU332W_Cfg_st=%d\n", InDTU332W_Cfg_st);
									sleep(2);
									break;
								case GET_InDTU332W_CSQ_ST2:{//激活之后再次获取DTU信号强度
								  InDTU332W_Cfg_st = GET_InDTU332W_CONN_ST;//先读取DTU模块是否连接到服务器，连接上以后才发协议数据
									printf("InDTU332W_Cfg_st=%d\n", InDTU332W_Cfg_st);
									sleep(1);	
									break;
								}	
								case GET_InDTU332W_CONN_ST:
									if(g_InDTU332W_conn_flag)//连接服务器端口OK
									{
										InDTU332W_Cfg_st = InDTU332W_IDLE_ST;
										g_set_ip_req = 0;//取消配置状态，可以发送协议数据帧
										ISO8583_App_Struct.Heart_TCP_state = 0x01;
										printf("88888888888888888888\n");
										g_InDTU332W_conn_count = 0;
										sleep(2);
									}else{
										g_InDTU332W_conn_count ++ ;
										if(g_InDTU332W_conn_count >= 60){
											InDTU332W_Cfg_st = RESET_InDTU332W_ST;//进行重启下
											g_InDTU332W_conn_count = 0;
										}
									}
									if(DEBUGLOG) COMM_RUN_LogOut("DTU 连接服务器端口反馈:g_InDTU332W_conn_flag = %x",g_InDTU332W_conn_flag);  
								break;
							}
							
						}
					}
					usleep(500000);
				}
			  break ;
			}
			case 0x01:{//创建连接
			  if(Globa_1->Charger_param.Serial_Network != 1){//走串口模式
				  ISO8583_App_Struct.Heart_Socket_fd = TCP_Client_Socket_Creat(Globa_1->ISO8583_NET_setting.ISO8583_Server_IP, \
                                             Globa_1->ISO8583_NET_setting.ISO8583_Server_port[0]+(Globa_1->ISO8583_NET_setting.ISO8583_Server_port[1]<<8), 20*1000000);
				
				}
				
				if(ISO8583_App_Struct.Heart_Socket_fd < 0){
					sleep(5);//间隔5秒再从新建立连接（这样连接失败的间隔时间最大为(20+ 5)s，最小为5s）
					if(ISO8583_DEBUG)printf("ISO8583_Heart_Task ISO8583_Socket_Creat failed\n");
					Globa_1->have_hart = 0;
				}else{
					if(ISO8583_DEBUG)printf("111ISO8583_Heart_Task ISO8583_Socket_Creat ok\n");
					ISO8583_App_Struct.Heart_TCP_state = 0x02;
					//Globa_1->have_hart = 1;
					Globa_1->have_hart = 0;
					pre_hart_time = current_now -10;
					ISO8583_App_Struct.ISO8583_Random_Rcev_falg = 0;
					InDTU332W_Cfg_st = LOGIN_InDTU332W_ST;//ACTIVE_InDTU332W_ST;
					InDTU332W_cfg_wait_ack = 0;
					ISO8583_Random_Count = 0;
				  COMM_RUN_LogOut("进行签到请求:");  

				}
				have_hart_outtime_count = 0;
				break;
			}
			case 0x02:{//签到请求
			  if(abs(current_now - pre_hart_time) >= 10){
					ret = ISO8583_AP_Random_Sent(ISO8583_App_Struct.Heart_Socket_fd);//签到请求 发送上行数据

					pre_hart_time = current_now; //等待10S继续发下次
					//printf("-----ISO8583_Random_Deal ok 签到请求 ISO8583_Random_Count= %d\n",ISO8583_Random_Count);
					if(ret < 0){//连接不正常
						if(Globa_1->Charger_param.Serial_Network == 1){//直接走c串口所以不需要初始化
							ISO8583_App_Struct.Heart_TCP_state = 0x00;
							InDTU332W_Cfg_st = LOGIN_InDTU332W_ST;//ACTIVE_InDTU332W_ST;
							InDTU332W_cfg_wait_ack = 0;
							Globa_1->have_hart = 0;
							sleep(5);
				    }else{
							sleep(5);
							if(ISO8583_DEBUG)printf("ISO8583_Random_Deal 发送失败\n");
							close(ISO8583_App_Struct.Heart_Socket_fd);
							ISO8583_App_Struct.Heart_TCP_state = 0x01;
							Globa_1->have_hart = 0;
						}
					}
				}else if(ISO8583_App_Struct.ISO8583_Random_Rcev_falg == 1){//接收到签到返回信息
					if(ISO8583_DEBUG)printf("ISO8583_Random_Deal ok 签到请求\n");
					ISO8583_App_Struct.Heart_TCP_state = 0x03;
					//Globa_1->have_hart = 1;
					Globa_1->have_hart = 0;
					ISO8583_App_Struct.ISO8583_Random_Rcev_falg = 0;
					pre_hart_time = current_now  - 10; //等待10S继续发下次
				  ISO8583_Random_Count = 0;
				}else if(ISO8583_App_Struct.ISO8583_Random_Rcev_falg == 2)//签到失败则重新进行下次签到
				{
					ISO8583_Random_Count++;
					if((ISO8583_Random_Count > 4))
					{
						ISO8583_Random_desleep(300);//延时5min
						//进行重连机制
					  if(Globa_1->Charger_param.Serial_Network == 1){//直接走c串口所以不需要初始化
							ISO8583_App_Struct.Heart_TCP_state = 0x00;
							InDTU332W_Cfg_st = LOGIN_InDTU332W_ST;//ACTIVE_InDTU332W_ST;
							InDTU332W_cfg_wait_ack = 0;
							Globa_1->have_hart = 0;
				    }else{
							if(ISO8583_DEBUG)printf("ISO8583_Random_Deal fail\n");
							close(ISO8583_App_Struct.Heart_Socket_fd);
							ISO8583_App_Struct.Heart_TCP_state = 0x01;
							Globa_1->have_hart = 0;
						}
					}
					else 
					{
						ISO8583_App_Struct.Heart_TCP_state = 0x02;
						Globa_1->have_hart = 0;
						time(&current_now);//获取系统当前时间
						pre_hart_time = current_now - 1;
						ISO8583_App_Struct.ISO8583_Random_Rcev_falg = 0;
						if(DEBUGLOG) COMM_RUN_LogOut("连续累计签到次数:ISO8583_Random_Count = %x",ISO8583_Random_Count);  
					}
				}
				break;
			}
			case 0x03:{//工作密钥请求
				if(abs(current_now - pre_hart_time) >= 10){
					if(ISO8583_DEBUG)printf("工作密钥请求\n");
					ret = ISO8583_AP_Key_Sent(ISO8583_App_Struct.Heart_Socket_fd);//签到请求 发送上行数据
					pre_hart_time = current_now; //等待10S继续发下次
					if(ret < 0){//连接不正常
						if(Globa_1->Charger_param.Serial_Network == 1){//直接走c串口所以不需要初始化
							ISO8583_App_Struct.Heart_TCP_state = 0x00;
							InDTU332W_Cfg_st = LOGIN_InDTU332W_ST;//ACTIVE_InDTU332W_ST;
							InDTU332W_cfg_wait_ack = 0;
							Globa_1->have_hart = 0;
							sleep(5);
				    }else{
							sleep(5);
							if(ISO8583_DEBUG)printf("ISO8583_Random_Deal fail\n");
							close(ISO8583_App_Struct.Heart_Socket_fd);
							ISO8583_App_Struct.Heart_TCP_state = 0x01;
							Globa_1->have_hart = 0;
						}
					}
				}else if(ISO8583_App_Struct.ISO8583_Key_Rcev_falg == 1){//接收到签到返回信息
					if(ISO8583_DEBUG)printf("ISO8583_Random_Deal ok\n");
					ISO8583_App_Struct.Heart_TCP_state = 0x04;
					Globa_1->have_hart = 0;
					ISO8583_App_Struct.ISO8583_Key_Rcev_falg = 0;
					pre_hart_time = current_now; //等待10S继续发下次
					Alarm_Sent_time = current_now - 10;
				/*	Alarm_Send_flag = 0;
					ISO8583_alarm_clear();
					ISO8583_App_Struct.ISO8583_Alarm_Rcev_falg =  0;  //清除告警数据*/
					
					ISO8583_App_Struct.ISO8583_Busy_Need_Up_falg = 0;
					Busy_Need_Up_id = -1;
					memset(&ISO8583_App_Struct.busy_data, '0', sizeof(ISO8583_App_Struct.busy_data));
					memset(&ISO8583_App_Struct.Car_Stop_busy_data, '0', sizeof(ISO8583_App_Struct.Car_Stop_busy_data));
					ISO8583_App_Struct.ISO8583_Get_SN_falg = 0;
					id = -1;
				}else if(ISO8583_App_Struct.ISO8583_Key_Rcev_falg == 2){//工作秘密获取失败，则重新进行工作密钥获取
					ISO8583_App_Struct.ISO8583_Key_Rcev_falg = 0;
				  ISO8583_App_Struct.Heart_TCP_state = 0x02;
					Globa_1->have_hart = 0;
					pre_hart_time = current_now - 5;
					ISO8583_App_Struct.ISO8583_Random_Rcev_falg = 0;
					if(DEBUGLOG) COMM_RUN_LogOut("工作密钥获取失败");  
				}
				break;
			}
			case 0x04:{//维持心跳
				if(Globa_1->have_APP_change == 1){//APP地址改变，需要重新签到
					if(Globa_1->Charger_param.Serial_Network == 1){//直接走c串口所以不需要初始化
						ISO8583_App_Struct.Heart_TCP_state = 0x00;
						InDTU332W_Cfg_st = LOGIN_InDTU332W_ST;//ACTIVE_InDTU332W_ST;
						InDTU332W_cfg_wait_ack = 0;
						Globa_1->have_hart = 0;
						sleep(5);
						Globa_1->have_APP_change = 0;
					}else{
						sleep(5);
						Globa_1->have_APP_change = 0;
						if(ISO8583_DEBUG)printf("ISO8583_Heart_Task Globa_1->have_APP_change\n");
						ISO8583_App_Struct.Heart_TCP_state = 0x01;
						close(ISO8583_App_Struct.Heart_Socket_fd);
						break;
					}
					break;
				}
				
			  if(Alarm_Send_flag == 0){//表示初始的
					if((abs(current_now - Alarm_Sent_time)>= 10)&&(ISO8583_App_Struct.ISO8583_Alarm_Rcev_falg == 0)){
						Alarm_Sent_time = current_now;
						if(alarm_info_get.charger_alarm_num > 0){
						  ret  = ISO8583_alarm_Sent(ISO8583_App_Struct.Heart_Socket_fd,&alarm_info_get);
							if(ret < 0){//连接不正常
								//ISO8583_alarm_clear();
								if(Globa_1->Charger_param.Serial_Network == 1){//直接走c串口所以不需要初始化
									ISO8583_App_Struct.Heart_TCP_state = 0x00;
									InDTU332W_Cfg_st = LOGIN_InDTU332W_ST;//ACTIVE_InDTU332W_ST;
									InDTU332W_cfg_wait_ack = 0;
									Globa_1->have_hart = 0;
									sleep(5);
								}else{
									sleep(5);
									if(ISO8583_DEBUG)printf("ISO8583_alarm_Sent fail\n");
									ISO8583_App_Struct.Heart_TCP_state = 0x01;
									Globa_1->have_hart = 0;
									close(ISO8583_App_Struct.Heart_Socket_fd);
									break;
								}
							}
						}
					}else if(ISO8583_App_Struct.ISO8583_Alarm_Rcev_falg == 1){//表示收到服务器反馈回来的初始数据
						Alarm_Send_flag = 1; //进行初始值判断
						ISO8583_App_Struct.ISO8583_Alarm_Rcev_falg = 1;
					}
				}else{//实时告警处理
					if((abs(current_now - Alarm_Sent_time)> 10)&&(ISO8583_App_Struct.ISO8583_Alarm_Rcev_falg == 0)){
						Alarm_Sent_time = current_now;
						//printf("-----ISO8583_alarm_Sent alarm_info_get.charger_alarm_num =%d \n",alarm_info_get.charger_alarm_num);
						if(alarm_info_get.charger_alarm_num > 0){
						  ret = ISO8583_alarm_Sent(ISO8583_App_Struct.Heart_Socket_fd,&alarm_info_get);
						  if(ret < 0){//连接不正常
								//ISO8583_alarm_clear();
								if(Globa_1->Charger_param.Serial_Network == 1){//直接走c串口所以不需要初始化
									ISO8583_App_Struct.Heart_TCP_state = 0x00;
									InDTU332W_Cfg_st = LOGIN_InDTU332W_ST;//ACTIVE_InDTU332W_ST;
									InDTU332W_cfg_wait_ack = 0;
									Globa_1->have_hart = 0;
									sleep(5);
								}else{
									sleep(5);
									if(DEBUGLOG)COMM_RUN_LogOut("ISO8583_alarm_Sent fail");
									
									ISO8583_App_Struct.Heart_TCP_state = 0x01;
									Globa_1->have_hart = 0;
									close(ISO8583_App_Struct.Heart_Socket_fd);
									break;
								}
							}
						}else if((ISO8583_App_Struct.ISO8583_Alarm_Rcev_falg == 0))
						{
							ISO8583_App_Struct.ISO8583_Alarm_Rcev_falg = 1;
						}
					}else if(ISO8583_App_Struct.ISO8583_Alarm_Rcev_falg == 1){
						ISO8583_alarm_get(&alarm_info_get);
					  if(alarm_info_get.charger_alarm_num > 0){
							ISO8583_App_Struct.ISO8583_Alarm_Rcev_falg = 0;
						  ret = ISO8583_alarm_Sent(ISO8583_App_Struct.Heart_Socket_fd,&alarm_info_get);
					  	Alarm_Sent_time = current_now;
		
						  if(ret < 0){//连接不正常
								//ISO8583_alarm_clear();
								if(Globa_1->Charger_param.Serial_Network == 1){//直接走c串口所以不需要初始化
									ISO8583_App_Struct.Heart_TCP_state = 0x00;
									InDTU332W_Cfg_st = LOGIN_InDTU332W_ST;//ACTIVE_InDTU332W_ST;
									InDTU332W_cfg_wait_ack = 0;
									Globa_1->have_hart = 0;
									sleep(5);
								}else{
									sleep(5);
									if(DEBUGLOG)COMM_RUN_LogOut("ISO8583_alarm_Sent --02 fail");

									ISO8583_App_Struct.Heart_TCP_state = 0x01;
									Globa_1->have_hart = 0;
									close(ISO8583_App_Struct.Heart_Socket_fd);
									break;
								}
							}
						}
					}
				}
				
        if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪的时候
				  if((Globa_1->charger_start == 1)||(Globa_1->App.APP_start == 1)||(Globa_2->charger_start == 1)||(Globa_2->App.APP_start == 1)||(ISO8583_App_Struct.old_gun_state[0] != Globa_1->gun_state)||(ISO8583_App_Struct.old_gun_state[1] != Globa_2->gun_state)||(ISO8583_App_Struct.old_Car_Lock_state[0] != Get_Cardlock_state(0))||(ISO8583_App_Struct.old_Car_Lock_state[1] != Get_Cardlock_state(1))){
						Hart_time_Value = Globa_1->Charger_param.heartbeat_busy_period;
					}else{
					  Hart_time_Value = Globa_1->Charger_param.heartbeat_idle_period;
					}
				}else if((Globa_1->Charger_param.System_Type >= 2 )&&(Globa_1->Charger_param.System_Type < 4 )){
				  if((Globa_1->charger_start == 1)||(Globa_1->App.APP_start == 1)||(ISO8583_App_Struct.old_gun_state[0] != Globa_1->gun_state)||(ISO8583_App_Struct.old_Car_Lock_state[0] != Get_Cardlock_state(0))){
						Hart_time_Value = Globa_1->Charger_param.heartbeat_busy_period;
					}else{
					  Hart_time_Value = Globa_1->Charger_param.heartbeat_idle_period;
					}
				}
				
				if(abs(current_now - pre_hart_time) >= Hart_time_Value){ //5秒一次心跳
					pre_hart_time = current_now;
					ret = ISO8583_AP_Heart_Sent(ISO8583_App_Struct.Heart_Socket_fd);//发送心跳帧
					if(ret < 0){//连接不正常
						if(Globa_1->Charger_param.Serial_Network == 1){//直接走c串口所以不需要初始化
							ISO8583_App_Struct.Heart_TCP_state = 0x00;
							InDTU332W_Cfg_st = LOGIN_InDTU332W_ST;//ACTIVE_InDTU332W_ST;
							InDTU332W_cfg_wait_ack = 0;
							Globa_1->have_hart = 0;
							sleep(5);
						}else{
							sleep(5);
			
							if(DEBUGLOG)COMM_RUN_LogOut("发送心跳帧 lost connection");

							ISO8583_App_Struct.Heart_TCP_state = 0x01;
							Globa_1->have_hart = 0;
							close(ISO8583_App_Struct.Heart_Socket_fd);
							break;
						}
					}
				}else if((Globa_1->User_Card_check_flag == 1)||(Globa_2->User_Card_check_flag == 1)){//卡片验证
				  if(abs(current_now - User_Card_Sent) >= 5){ //5秒一次心跳
					  User_Card_Sent = current_now;
						if(((Globa_1->User_Card_check_flag == 1)&&(Globa_1->Start_Mode == VIN_START_TYPE))||((Globa_2->User_Card_check_flag == 1)&&(Globa_2->Start_Mode == VIN_START_TYPE)))
						{ 
							ret = Fill_ISO8583_1120_VIN_Auth_Frame(ISO8583_App_Struct.Heart_Socket_fd);//VIN鉴权报文
						}else 
						{
							ret = ISO8583_User_Card_Sent_Deal(ISO8583_App_Struct.Heart_Socket_fd);//发送卡片鉴权
						}
						if(ret < 0){//连接不正常
							if(Globa_1->Charger_param.Serial_Network == 1){//直接走c串口所以不需要初始化
								ISO8583_App_Struct.Heart_TCP_state = 0x00;
								InDTU332W_Cfg_st = LOGIN_InDTU332W_ST;//ACTIVE_InDTU332W_ST;
								InDTU332W_cfg_wait_ack = 0;
								Globa_1->have_hart = 0;
								sleep(5);
							}else{
								sleep(5);
								if(DEBUGLOG)COMM_RUN_LogOut("发送心跳帧 卡片验证/VIN 失败");
								ISO8583_App_Struct.Heart_TCP_state = 0x01;
								Globa_1->have_hart = 0;
								close(ISO8583_App_Struct.Heart_Socket_fd);
								break;
							}
					  }
					}
				}else if((Globa_1->Enterprise_Card_flag == 1)||(Globa_2->Enterprise_Card_flag == 1)){//发送企业号启动成功
					if(abs(current_now - Enterprise_Card_Sent) >= 5){ //5秒一次心跳
					  Enterprise_Card_Sent = current_now;
						ret = Fill_AP1130_Sent_Deal(ISO8583_App_Struct.Heart_Socket_fd);//发送企业号启动成功
						if(DEBUGLOG)COMM_RUN_LogOut("发送企业号启动：Globa_1->Enterprise_Card_flag =%d Globa_2->Enterprise_Card_flag =%d ",Globa_1->Enterprise_Card_flag,Globa_2->Enterprise_Card_flag);

						if(ret < 0){//连接不正常
							if(Globa_1->Charger_param.Serial_Network == 1){//直接走c串口所以不需要初始化
								ISO8583_App_Struct.Heart_TCP_state = 0x00;
								InDTU332W_Cfg_st = LOGIN_InDTU332W_ST;//ACTIVE_InDTU332W_ST;
								InDTU332W_cfg_wait_ack = 0;
								Globa_1->have_hart = 0;
								sleep(5);
							}else{
								sleep(5);
								if(ISO8583_DEBUG)printf("发送心跳帧 lost connection\n");
								if(DEBUGLOG)COMM_RUN_LogOut("发送企业号启动失败");

								ISO8583_App_Struct.Heart_TCP_state = 0x01;
								Globa_1->have_hart = 0;
								close(ISO8583_App_Struct.Heart_Socket_fd);
								break;
							}
					  }
					}
				}else if(AP_APP_Sent_1_falg[0] == 1){ //回应1号充电启动反馈结果
					User_Card_Sent = 0;
					Enterprise_Card_Sent =0;
				  //if(DEBUGLOG) COMM_RUN_LogOut("%s %d","准备回应1号充电启动/停止反馈结果:",AP_APP_Sent_1_falg[1]);  
					ISO8583_AP_APP_Sent(ISO8583_App_Struct.Heart_Socket_fd, AP_APP_Sent_1_falg[1],0);
				}else if(AP_APP_Sent_2_falg[0] == 1){//回应2号充电启动反馈结果
					User_Card_Sent = 0;
				  //if(DEBUGLOG) COMM_RUN_LogOut("%s %d","回应2号充电启动/停止反馈结果:",AP_APP_Sent_2_falg[1]);  
					ISO8583_AP_APP_Sent(ISO8583_App_Struct.Heart_Socket_fd, AP_APP_Sent_2_falg[1],1);
				}else{
					User_Card_Sent = 0;
				}	
				
	      if(Globa_1->charger_state != 0x04){//终端不在充电中
					Battery_Parameter_Flag_1 = 0;
				}
				
			  if(Globa_2->charger_state != 0x04){//终端不在充电中
					Battery_Parameter_Flag_2 = 0;
				}
		
				if(abs(current_now - BMS_time) >= 10){//10秒上传一次
					if((Battery_Parameter_Flag_1 == 0) && (Globa_1->BMS_Info_falg == 1)&&((Globa_1->QT_Step == 0x21)||(Globa_1->QT_Step == 0x22)||(Globa_1->QT_Step == 0x24))){
						ret = ISO8583_Battery_Parameter_Sent(ISO8583_App_Struct.Heart_Socket_fd,0);//上传蓄电池参数
						if(ret < 0){//连接不正常
							if(Globa_1->Charger_param.Serial_Network == 1){//直接走c串口所以不需要初始化
								ISO8583_App_Struct.Heart_TCP_state = 0x00;
								InDTU332W_Cfg_st = LOGIN_InDTU332W_ST;//ACTIVE_InDTU332W_ST;
								InDTU332W_cfg_wait_ack = 0;
								Globa_1->have_hart = 0;
								Battery_Parameter_Flag_1 = 0;
								sleep(5);
							}else{
								sleep(5);
								if(DEBUGLOG)COMM_RUN_LogOut("1# ISO8583_Battery_Parameter_Sent failed lost connection");

								ISO8583_App_Struct.Heart_TCP_state = 0x01;
								Globa_1->have_hart = 0;
								Battery_Parameter_Flag_1 = 0;
								close(ISO8583_App_Struct.Heart_Socket_fd);
								break;
							}
						}else{//发送成功
							Battery_Parameter_Flag_1 = 1;
						}
					}else if((Battery_Parameter_Flag_2 == 0) && (Globa_2->BMS_Info_falg == 1)&&((Globa_2->QT_Step == 0x21)||(Globa_2->QT_Step == 0x22)||(Globa_1->QT_Step == 0x24))&&((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 ))){
						ret = ISO8583_Battery_Parameter_Sent(ISO8583_App_Struct.Heart_Socket_fd,1);//上传蓄电池参数
						if(ret < 0){//连接不正常
							if(Globa_1->Charger_param.Serial_Network == 1){//直接走c串口所以不需要初始化
								ISO8583_App_Struct.Heart_TCP_state = 0x00;
								InDTU332W_Cfg_st = LOGIN_InDTU332W_ST;//ACTIVE_InDTU332W_ST;
								InDTU332W_cfg_wait_ack = 0;
								Globa_1->have_hart = 0;
								Battery_Parameter_Flag_2 = 0;
								sleep(5);
							}else{
								sleep(5);
								if(DEBUGLOG)COMM_RUN_LogOut("2#ISO8583_Battery_Parameter_Sent2 failed lost connection");

								ISO8583_App_Struct.Heart_TCP_state = 0x01;
								Globa_1->have_hart = 0;
								Battery_Parameter_Flag_2 = 0;
								close(ISO8583_App_Struct.Heart_Socket_fd);
								break;
							}
						}else{//发送成功
							Battery_Parameter_Flag_2 = 1;
						}
					}
					
					if((Globa_1->BMS_Info_falg == 1)&&(BMS_gun_no == 0)&&(Battery_Parameter_Flag_1 == 1)){
						ret = ISO8583_BMS_Real_Data_Sent(ISO8583_App_Struct.Heart_Socket_fd,BMS_gun_no);

						if(ret < 0){//连接不正常
							if(Globa_1->Charger_param.Serial_Network == 1){//直接走c串口所以不需要初始化
								ISO8583_App_Struct.Heart_TCP_state = 0x00;
								InDTU332W_Cfg_st = LOGIN_InDTU332W_ST;//ACTIVE_InDTU332W_ST;
								InDTU332W_cfg_wait_ack = 0;
								Globa_1->have_hart = 0;
								Battery_Parameter_Flag_1 = 0;
								BMS_gun_no = 0;
								sleep(5);
							}else{
								sleep(5);
								if(DEBUGLOG)COMM_RUN_LogOut("1#ISO8583_BMS_Real_Data failed ");
								ISO8583_App_Struct.Heart_TCP_state = 0x01;
								Globa_1->have_hart = 0;
								Battery_Parameter_Flag_1 = 0;
								close(ISO8583_App_Struct.Heart_Socket_fd);
								 BMS_gun_no = 0;
								break;
							}
						}
					}else if((Globa_2->BMS_Info_falg == 1)&&(BMS_gun_no == 1)&&(Battery_Parameter_Flag_2 == 1)){
						ret = ISO8583_BMS_Real_Data_Sent(ISO8583_App_Struct.Heart_Socket_fd,BMS_gun_no);

						if(ret < 0){//连接不正常
							if(Globa_1->Charger_param.Serial_Network == 1){//直接走c串口所以不需要初始化
								ISO8583_App_Struct.Heart_TCP_state = 0x00;
								InDTU332W_Cfg_st = LOGIN_InDTU332W_ST;//ACTIVE_InDTU332W_ST;
								InDTU332W_cfg_wait_ack = 0;
								Globa_1->have_hart = 0;
								Battery_Parameter_Flag_2 = 0;
								BMS_gun_no = 0;
								sleep(5);
							}else{
								sleep(5);
								if(DEBUGLOG)COMM_RUN_LogOut("2#ISO8583_BMS_Real_Data failed ");
								ISO8583_App_Struct.Heart_TCP_state = 0x01;
								Globa_1->have_hart = 0;
								Battery_Parameter_Flag_2 = 0;
								close(ISO8583_App_Struct.Heart_Socket_fd);
								 BMS_gun_no = 0;
								break;
							}
						}
					}
					if((Globa_1->Charger_param.System_Type <= 1 )||(Globa_1->Charger_param.System_Type == 4 )){//双枪
						BMS_gun_no++;
						if(BMS_gun_no >= 2){
							BMS_gun_no = 0;
						}
					}
					BMS_time = current_now;
				}
				
			 //查询流水号				
				if((abs(current_now - pre_Busy_SN_time) >= 5)&&(ISO8583_App_Struct.ISO8583_Get_SN_falg == 0)){ //3秒一次查询流水号
					pre_Busy_SN_time = current_now;
					if (dev_cfg.dev_para.car_lock_num != 0)
					{
						Busy_SN_TYPE = 1 - Busy_SN_TYPE;
					}
					else
					{
						Busy_SN_TYPE = 0;
					}
					pthread_mutex_lock(&busy_db_pmutex);
					if (Busy_SN_TYPE == 0) //查询充电流水业务
					{
						id = Select_NO_Busy_SN(0);//查询没有流水号的最大记录 ID 值 --充电业务
					}
					else 
					{
						id = Select_NO_Busy_SN(1);//查询没有流水号的最大记录 ID 值 --停车业务
					}
					pthread_mutex_unlock(&busy_db_pmutex);
					Busy_SN_start_time = current_now;
					if(id != -1){
						ISO8583_App_Struct.ISO8583_Get_SN_falg = 1; //表示需要发送数据
						pre_Busy_SN_time = current_now - 5;
						if(DEBUGLOG)COMM_RUN_LogOut("表示查询到流水号需要发送数据id=%d",id);

					}
				}else if(ISO8583_App_Struct.ISO8583_Get_SN_falg != 0){
					if(abs(current_now - Busy_SN_start_time) >= 10){//10s还没有请求到则重新请求流水号
						Busy_SN_start_time = current_now;
						pre_Busy_SN_time = current_now;
						ISO8583_App_Struct.ISO8583_Get_SN_falg = 0;
						id = -1;
					}
				}
		 
				if(id != -1){//表示查询到了没有记录的流水号
					if((abs(current_now - pre_Busy_SN_time) >= 5)&&(ISO8583_App_Struct.ISO8583_Get_SN_falg == 1)){ //3秒一次查询流水号
						pre_Busy_SN_time = current_now;
						ret =  ISO8583_AP_Get_SN_Sent(ISO8583_App_Struct.Heart_Socket_fd);// 充电终端业务流水号获取
						 if(DEBUGLOG)COMM_RUN_LogOut("表示请求流水号id=%d",id);

						if(ret < 0){//连接不正常
							if(Globa_1->Charger_param.Serial_Network == 1){//直接走c串口所以不需要初始化
								ISO8583_App_Struct.Heart_TCP_state = 0x00;
								InDTU332W_Cfg_st = LOGIN_InDTU332W_ST;//ACTIVE_InDTU332W_ST;
								InDTU332W_cfg_wait_ack = 0;
								Globa_1->have_hart = 0;
								sleep(5);
							}else{
								sleep(5);
							 if(DEBUGLOG)COMM_RUN_LogOut("表示请求流水号失败");

								ISO8583_App_Struct.Heart_TCP_state = 0x01;
								Globa_1->have_hart = 0;
								close(ISO8583_App_Struct.Heart_Socket_fd);
								break;
							}
						}
					}else if (ISO8583_App_Struct.ISO8583_Get_SN_falg == 2){//获取成功
						pthread_mutex_lock(&busy_db_pmutex);
						Set_Busy_SN(id, ISO8583_App_Struct.ISO8583_busy_SN_Vlaue,Busy_SN_TYPE);//添加数据库该条充电记录的流水号
						pthread_mutex_unlock(&busy_db_pmutex);
						ISO8583_App_Struct.ISO8583_Get_SN_falg = 0;
						id = -1;
					}
				}
				
				//充电业务数据上传
				if((Busy_dataMsgList->count == 0)&&(ISO8583_App_Struct.ISO8583_Busy_Need_Up_falg == 0))//表示发送队列中无数据发送，需要从数据库中插入导队列中，
				{
					if((abs(current_now - pre_Busy_Need_Up_time) >= 10)){ //10秒周期性查询是否需要上次记录
						pre_Busy_Need_Up_time = current_now;
						pthread_mutex_lock(&busy_db_pmutex);
						Select_Busy_Need_Up_intoMsgNode(Busy_dataMsgList);//查询需要上传的业务记录的最大 ID 值
						pthread_mutex_unlock(&busy_db_pmutex);
						Busy_Need_Up_start_time = current_now;
						ISO8583_App_Struct.ISO8583_Busy_Need_Up_falg = 0;
						if(Busy_dataMsgList->count != 0)
						{
							pre_Busy_Need_Up_time = current_now - 8;
						}
					}
				}else{//从队列中上传数据
					if((abs(current_now - pre_Busy_Need_Up_time) >= 10)){ //3秒一次查询流水号
						pre_Busy_Need_Up_time = current_now;
					
						pthread_mutex_lock(&Busy_dataMsgList->lock);
						Busy_dataMsgNode *e = list_entry((&(Busy_dataMsgList->glist))->next, typeof(*e), glist);
						if (&e->glist != &Busy_dataMsgList->glist)
						{
							list_del(&e->glist);
							Busy_dataMsgList->count--;
						}
						else
						{
								e = NULL;
								ISO8583_App_Struct.ISO8583_Busy_Need_Up_falg = 0; //队列中无数据，从新获取数据
						}
						pthread_mutex_unlock(&Busy_dataMsgList->lock);

						if (e != NULL)
						{
							Busy_Need_Up_id = e->Busy_Need_Up_id;
              ISO8583_App_Struct.ISO8583_Busy_Need_Up_falg = 1;
							if(DEBUGLOG)COMM_RUN_LogOut("准备上传的业务记录Busy_Need_Up_id =%d Busy_dataMsgList->count =%d",Busy_Need_Up_id ,Busy_dataMsgList->count);
							ret =  ISO8583_AP_charg_busy_sent(ISO8583_App_Struct.Heart_Socket_fd, &e->busy_data);//充电业务数据上传
							free(e);
							if(ret < 0){//连接不正常
								if(Globa_1->Charger_param.Serial_Network == 1){//直接走c串口所以不需要初始化
									ISO8583_App_Struct.Heart_TCP_state = 0x00;
									InDTU332W_Cfg_st = LOGIN_InDTU332W_ST;//ACTIVE_InDTU332W_ST;
									InDTU332W_cfg_wait_ack = 0;
									Globa_1->have_hart = 0;
									sleep(5);
								}else{
									sleep(5);
									if(DEBUGLOG)printf("发送心跳帧 lost connection\n");
									ISO8583_App_Struct.Heart_TCP_state = 0x01;
									Globa_1->have_hart = 0;
									close(ISO8583_App_Struct.Heart_Socket_fd);
									break;
								}
							}
						}
					}else if(ISO8583_App_Struct.ISO8583_Busy_Need_Up_falg == 2){//获取成功
						pthread_mutex_lock(&busy_db_pmutex);
						Set_Busy_UP_num(Busy_Need_Up_id, 0xAA);//标记该条充电记录上传成功
						pthread_mutex_unlock(&busy_db_pmutex);
						if(DEBUGLOG)COMM_RUN_LogOut("上传成功 Busy_Need_Up_id =%d",Busy_Need_Up_id);

						ISO8583_App_Struct.ISO8583_Busy_Need_Up_falg = 0;
						int i = 0;
					   sleep(1);
						pthread_mutex_lock(&busy_db_pmutex);
						for(i=0;i<5;i++){
							if(Select_NO_Over_Record(Busy_Need_Up_id,0xAA) != 0){//表示有没有上传的记录
								Set_Busy_UP_num(Busy_Need_Up_id, 0xAA);
								sleep(1);
							}else{
								break;
							}
						}
					  pthread_mutex_unlock(&busy_db_pmutex);
						Busy_Need_Up_id = -1;
						memset(&ISO8583_App_Struct.busy_data, '0', sizeof(ISO8583_App_Struct.busy_data));
						pre_Busy_Need_Up_time = current_now - 7; //每次间隔2s上传一次，收到回馈的话
					}else if(ISO8583_App_Struct.ISO8583_Busy_Need_Up_falg == 3){//获取成功
						pthread_mutex_lock(&busy_db_pmutex);
						Set_Busy_UP_num(Busy_Need_Up_id, 0xB4);//标记该条充电记录上传成功
						pthread_mutex_unlock(&busy_db_pmutex);
						if(DEBUGLOG)COMM_RUN_LogOut("上传 异常记录成功 Busy_Need_Up_id =%d",Busy_Need_Up_id);
            sleep(1);
						int i = 0;
						pthread_mutex_lock(&busy_db_pmutex);
						for(i=0;i<5;i++){
							if(Select_NO_Over_Record(Busy_Need_Up_id,0xB4) != 0){//表示有没有上传的记录
								Set_Busy_UP_num(Busy_Need_Up_id, 0xB4);
								sleep(1);
							}else{
								break;
							}
						}
					  pthread_mutex_unlock(&busy_db_pmutex);
						ISO8583_App_Struct.ISO8583_Busy_Need_Up_falg = 0;
						Busy_Need_Up_id = -1;
						memset(&ISO8583_App_Struct.busy_data, '0', sizeof(ISO8583_App_Struct.busy_data));
						pre_Busy_Need_Up_time = current_now - 7; //每次间隔2s上传一次，收到回馈的话
					}							
				}
				
				if (dev_cfg.dev_para.car_lock_num != 0)
				{
					//停车业务数据上传
					if((abs(current_now - pre_Car_Busy_Need_Up_time) >= 3)&&(ISO8583_App_Struct.ISO8583_Car_Stop_Need_Up_falg == 0)){ 
						pre_Car_Busy_Need_Up_time = current_now;
						pthread_mutex_lock(&busy_db_pmutex);
						Car_Stop_Busy_Need_Up_id = Select_Car_Stop_Busy_Need_Up(&ISO8583_App_Struct.Car_Stop_busy_data, &up_num);//查询需要上传的业务记录的最大 ID 值
						pthread_mutex_unlock(&busy_db_pmutex);
						Car_Stop_Busy_Need_Up_start_time = current_now;
						if(Car_Stop_Busy_Need_Up_id != -1){
							ISO8583_App_Struct.ISO8583_Car_Stop_Need_Up_falg = 1; //表示需要发送数据
							pre_Car_Busy_Need_Up_time = current_now - 10;
							if(ISO8583_DEBUG)printf("查询需要上传的停车业务记录的最大 获取成功 =%d\n",Car_Stop_Busy_Need_Up_id);
						}
					}else if(ISO8583_App_Struct.ISO8583_Car_Stop_Need_Up_falg != 0){
						if(abs(current_now - Car_Stop_Busy_Need_Up_start_time) >= 10){//10s还没有请求到则重新请求流水号
							Car_Stop_Busy_Need_Up_start_time = current_now;
							pre_Car_Busy_Need_Up_time = current_now;
							ISO8583_App_Struct.ISO8583_Car_Stop_Need_Up_falg = 0;
							Car_Stop_Busy_Need_Up_id = -1;
							memset(&ISO8583_App_Struct.Car_Stop_busy_data, '0', sizeof(ISO8583_App_Struct.Car_Stop_busy_data));
						}
					}

					if(Car_Stop_Busy_Need_Up_id != -1){//表示查询到了没有记录的流水号
						if((abs(current_now - pre_Car_Busy_Need_Up_time) >= 10)&&(ISO8583_App_Struct.ISO8583_Car_Stop_Need_Up_falg == 1)){ //3秒一次查询流水号
							pre_Car_Busy_Need_Up_time = current_now;
							if(ISO8583_DEBUG)printf("表示上传的记录 =%d\n",Car_Stop_Busy_Need_Up_id);
							ret =  ISO8583_Car_Stop_busy_sent(ISO8583_App_Struct.Heart_Socket_fd, &ISO8583_App_Struct.Car_Stop_busy_data);//停车业务数据上传
							if(ret < 0){//连接不正常
								memset(&ISO8583_App_Struct.Car_Stop_busy_data, '0', sizeof(ISO8583_App_Struct.Car_Stop_busy_data));
								if(Globa_1->Charger_param.Serial_Network == 1){//直接走c串口所以不需要初始化
									ISO8583_App_Struct.Heart_TCP_state = 0x00;
									InDTU332W_Cfg_st = LOGIN_InDTU332W_ST;//ACTIVE_InDTU332W_ST;
									InDTU332W_cfg_wait_ack = 0;
									Globa_1->have_hart = 0;
									sleep(5);
								}else{
									sleep(5);
									if(ISO8583_DEBUG)printf("发送心跳帧 lost connection\n");
									ISO8583_App_Struct.Heart_TCP_state = 0x01;
									Globa_1->have_hart = 0;
									close(ISO8583_App_Struct.Heart_Socket_fd);
									break;
								}
							}
						}else if(ISO8583_App_Struct.ISO8583_Car_Stop_Need_Up_falg == 2){//获取成功
							if(ISO8583_DEBUG)printf("表示上传的记录 获取成功 =%d\n",Car_Stop_Busy_Need_Up_id);
							pthread_mutex_lock(&busy_db_pmutex);
							Set_Car_Stop_Busy_UP_num(Car_Stop_Busy_Need_Up_id,0xAA);//标记该条充电记录上传成功
							pthread_mutex_unlock(&busy_db_pmutex);
							ISO8583_App_Struct.ISO8583_Car_Stop_Need_Up_falg = 0;
							int i = 0;
							for(i=0;i<5;i++){
								sleep(1);
								if(Select_Car_Stop_NO_Over_Record(Car_Stop_Busy_Need_Up_id,0xAA)!= -1){//表示有没有上传的记录
									Set_Car_Stop_Busy_UP_num(Car_Stop_Busy_Need_Up_id, 0xAA);
								}
							}
							Car_Stop_Busy_Need_Up_id = -1;
							memset(&ISO8583_App_Struct.busy_data, '0', sizeof(ISO8583_App_Struct.busy_data));
						}else if(ISO8583_App_Struct.ISO8583_Car_Stop_Need_Up_falg == 3){//获取成功
							if(ISO8583_DEBUG)printf("表示上传的记录 异常 获取成功 =%d\n",Busy_Need_Up_id);
							pthread_mutex_lock(&busy_db_pmutex);
							Set_Car_Stop_Busy_UP_num(Car_Stop_Busy_Need_Up_id, 0xB4);//标记该条充电记录上传成功
							pthread_mutex_unlock(&busy_db_pmutex);
							int i = 0;
							for(i=0;i<5;i++){
								sleep(1);
								if(Select_Car_Stop_NO_Over_Record(Car_Stop_Busy_Need_Up_id,0xB4)!= -1){//表示有没有上传的记录
									Set_Car_Stop_Busy_UP_num(Car_Stop_Busy_Need_Up_id, 0xB4);
								}
							}
							ISO8583_App_Struct.ISO8583_Car_Stop_Need_Up_falg = 0;
							Car_Stop_Busy_Need_Up_id = -1;
							memset(&ISO8583_App_Struct.Car_Stop_busy_data, '0', sizeof(ISO8583_App_Struct.Car_Stop_busy_data));
						}							
					}else{
						ISO8583_App_Struct.ISO8583_Car_Stop_Need_Up_falg = 0;
					}
				}
			
				
			//	if((Globa_1->QT_Step == 0x02)||(Globa_1->QT_Step == 0x03)||((Globa_2->QT_Step == 0x03)&&((Globa_1->QT_Step == 0x22)||(Globa_1->QT_Step == 0x21)||(Globa_1->QT_Step == 0x25)))){//系统空闲时再上传业务数据
				
				if((Globa_1->QT_Step == 0x02)||((Globa_1->QT_Step == 0x03)&&(Globa_2->QT_Step == 0x03))){//系统空闲时再上传业务数据
				//3: 电价数据更新   ------------------------------------------------------
					if(Globa_1->have_price_change == 1){
						if(abs(current_now - pre_price_change_time) >= 10){ //3秒一次查询流水号
							pre_price_change_time = current_now;
							ret =  ISO8583_AP_req_price_sent(ISO8583_App_Struct.Heart_Socket_fd,0);//发送电价
							if(DEBUGLOG)COMM_RUN_LogOut("电价数据更新");

							if(ret < 0){//连接不正常
								if(Globa_1->Charger_param.Serial_Network == 1){//直接走c串口所以不需要初始化
									ISO8583_App_Struct.Heart_TCP_state = 0x00;
									InDTU332W_Cfg_st = LOGIN_InDTU332W_ST;//ACTIVE_InDTU332W_ST;
									InDTU332W_cfg_wait_ack = 0;
									Globa_1->have_hart = 0;
									sleep(5);
								}else{
									sleep(5);
									if(DEBUGLOG)COMM_RUN_LogOut("电价数据更新 失败");
									ISO8583_App_Struct.Heart_TCP_state = 0x01;
									Globa_1->have_hart = 0;
									close(ISO8583_App_Struct.Heart_Socket_fd);
									break;
								}
							}
						}
					}else if(Globa_1->Apply_Price_type == 1){//获取定时电价
						if(abs(current_now - pre_price_change_time) >= 10){ //3秒一次查询流水号
							pre_price_change_time = current_now;
							ret =  ISO8583_AP_req_price_sent(ISO8583_App_Struct.Heart_Socket_fd,1);//发送电价
							if(ISO8583_DEBUG)COMM_RUN_LogOut("获取定时电价数据更新");

						  if(ret < 0){//连接不正常
								if(Globa_1->Charger_param.Serial_Network == 1){//直接走c串口所以不需要初始化
									ISO8583_App_Struct.Heart_TCP_state = 0x00;
									InDTU332W_Cfg_st = LOGIN_InDTU332W_ST;//ACTIVE_InDTU332W_ST;
									InDTU332W_cfg_wait_ack = 0;
									Globa_1->have_hart = 0;
									sleep(5);
								}else{
									sleep(5);
									if(ISO8583_DEBUG)COMM_RUN_LogOut("获取定时电价数据更新 失败");
									ISO8583_App_Struct.Heart_TCP_state = 0x01;
									Globa_1->have_hart = 0;
									close(ISO8583_App_Struct.Heart_Socket_fd);
									break;
								}
							}
						}
					}
					//黑明单更新
					if(Globa_1->have_card_change == 1){
						if((abs(current_now - pre_card_change_time) >= 10)||(ISO8583_App_Struct.invalid_card_Rcev_falg == 1)){ //3秒一次查询流水号
							pre_card_change_time = current_now;
							ISO8583_App_Struct.invalid_card_Rcev_falg = 0;
							ret = ISO8583_AP_req_card_sent(ISO8583_App_Struct.Heart_Socket_fd,ISO8583_App_Struct.invalid_card_bag_now);//发送电价
						  if(ret < 0){//连接不正常
								if(Globa_1->Charger_param.Serial_Network == 1){//直接走c串口所以不需要初始化
									ISO8583_App_Struct.Heart_TCP_state = 0x00;
									InDTU332W_Cfg_st = LOGIN_InDTU332W_ST;//ACTIVE_InDTU332W_ST;
									InDTU332W_cfg_wait_ack = 0;
									Globa_1->have_hart = 0;
									sleep(5);
								}else{
									sleep(5);
									if(ISO8583_DEBUG)printf("发送心跳帧 lost connection\n");
									ISO8583_App_Struct.Heart_TCP_state = 0x01;
									Globa_1->have_hart = 0;
									close(ISO8583_App_Struct.Heart_Socket_fd);
									break;
								}
							}
						}
					}
				}
				
				if(Globa_1->Charger_param.Serial_Network == 1){//串口的时候才走这个流程，
					switch(Software_upate_TCP_state){
						case 0x00:{
							if(Globa_1->Software_Version_change == 1){
								Software_upate_TCP_state = 0x01;
								Software_upate_over_time = current_now;
								ISO8583_App_Struct.Software_upate_flag = 0;
								ISO8583_App_Struct.Software_Request_flag = 0;
								curent_data_block_num = 1;
								Globa_1->Updat_data_value = 0;
							}
							break ;
						}
						case 0x01:{//进行升级请求
							if(ISO8583_App_Struct.Software_upate_flag == 0){
								if(abs(current_now - Software_upate_over_time) >= 10){ //3秒一次查询流水号
									Software_upate_over_time = current_now;
									ret = ISO8583_AP1060_Request_sent(ISO8583_App_Struct.Heart_Socket_fd,curent_data_block_num);
								}
							}else if(ISO8583_App_Struct.Software_upate_flag  == 1){//接收到数据
								Software_upate_over_time = current_now - 10;
								ISO8583_App_Struct.Software_upate_flag = 0;
								
								if(All_Data_Num != 0){
									Globa_1->Updat_data_value = ((curent_data_block_num*100)/All_Data_Num);
								}
							}else if(ISO8583_App_Struct.Software_upate_flag  == 2){//接收数据完毕
								memcpy(&Globa_1->Charger_param2.Software_Version[0], &Globa_1->tmp_Software_Version[0], sizeof(Globa_1->tmp_Software_Version));//软件包升级版本更新
								System_param2_save();
								Software_upate_TCP_state = 0x02; //进行升级
								AP1070_Request_over_time = current_now - 10;
								Globa_1->Updat_data_value = 100; 
							}else if(ISO8583_App_Struct.Software_upate_flag  == 3){//表示升级完成，之后进行文件的解压
								memcpy(&Globa_1->Charger_param2.Software_Version[0], &Globa_1->tmp_Software_Version[0], sizeof(Globa_1->tmp_Software_Version));//软件包升级版本更新
								System_param2_save();
								Globa_1->Updat_data_value = 0;
								Heart_TCP_state = 0x01; //进行升级
								system("rm -rf /mnt/Nand1/update/DC_Charger.tar.gz");
								Globa_1->Software_Version_change  = 0;
							}
							break;
						}
						case 0x02:{//升级之后确认
							if(ISO8583_App_Struct.Software_Request_flag == 0){
								if(abs(current_now - AP1070_Request_over_time) >= 10){ //3秒一次查询流水号
									AP1070_Request_over_time = current_now;
									ISO8583_AP1070_Request_sent(ISO8583_App_Struct.Heart_Socket_fd);
								}
							}else{
								if(access(F_UPDATE_Charger_Package,F_OK) != -1){//存在更新文件
									system("tar -zvxf /mnt/Nand1/update/DC_Charger.tar.gz  -C  /mnt/Nand1/update/");
									Software_upate_TCP_state = 0x03; //进行升级
									sleep(10);
									system("sync");
								}
							}
							break;
						}
						
						case 0x03:{
							if((access(F_UPDATE_Electric_Charge_Pile,F_OK) != -1)||(access(F_UPDATE_Charger,F_OK) != -1)||(access(F_UPDATE_EAST_DC_CTL,F_OK) != -1)){//存在更新文件
								sleep(1);
								system("rm -rf /mnt/Nand1/update/DC_Charger.tar.gz");
								Heart_TCP_state = 0x04; //进行升级
								Update_EASE_DC_CTL_falg = 0;
								Update_Electric_Charge_Pile_falg = 0;
								Update_Charger_falg =0;
							}else{
								 Globa_1->Software_Version_change  = 0;
								 system("rm -rf /mnt/Nand1/update/*");
								 sleep(1);
								 system("sync");
								 sleep(1);
								 system("reboot");
							}
							break;
						}
				    case 0x04:{//等待系统重启升级
							if(Globa_1->QT_Step == 0x02){//系统处于空闲的时候
								if(access(F_UPDATE_Charger_Conf,F_OK) != -1){
									FILE *fp_new = fopen(F_UPDATE_Charger_Conf, "r");
									if(fp_new != NULL){
										char temp_char[100],read_buf[1024];
										while(fgets(&read_buf,1024, fp_new) > 0){
											sprintf(&temp_char[0], "%s", read_buf);	
											system(temp_char);	
											usleep(500000);
										}
									}
								}else{
									if(access(F_UPDATE_CHECK_INFO,F_OK) == 0)//升级文件信息 --进行文件校验
									{
										printf("fopen /mnt/Nand1/update/update.info\n");
										FILE *fd=fopen(F_UPDATE_CHECK_INFO,"r");
										if(fd)
										{
											if(access(F_UPDATE_Charger,F_OK) != -1)
											{
													if(fgets(buf,100,fd))
													{
														MD5File(F_UPDATE_Charger,buf2);
														char *p=md5;
														char i =0;
														for(i=0;i<16;i++)
														{
															sprintf(p,"%02x",buf2[i]);
															p+=2;
														}
														if(DEBUGLOG) COMM_RUN_LogOut("Update_Charger: MD5=%s,checked MD5=%s",buf,md5);
														if(strncmp(buf,md5,strlen(md5))!=0)
														{	
															Update_Charger_falg = 1;	
															system("rm -rf /mnt/Nand1/update/charger"); //文件校验异常
														}
														else
														{
															Update_Charger_falg = 0;
														}
													}
											}
											if(access(F_UPDATE_Electric_Charge_Pile,F_OK) != -1)
											{
													if(fgets(buf,100,fd))
													{
														MD5File(F_UPDATE_Electric_Charge_Pile,buf2);
														char *p=md5;
														char i =0;
														for(i=0;i<16;i++)
														{
															sprintf(p,"%02x",buf2[i]);
															p+=2;
														}
														if(DEBUGLOG) COMM_RUN_LogOut("Electric_Charge_Pile: MD5=%s,checked MD5=%s",buf,md5);
														if(strncmp(buf,md5,strlen(md5))!=0)
														{	
															Update_Electric_Charge_Pile_falg = 1;	
															system("rm -rf /mnt/Nand1/update/Electric_Charge_Pile"); //文件校验异常
														}
														else
														{
															Update_Electric_Charge_Pile_falg = 0;
														}
													}
											}
											if(access(F_UPDATE_EAST_DC_CTL,F_OK) != -1)
											{
													if(fgets(buf,100,fd))
													{
														MD5File(F_UPDATE_EAST_DC_CTL,buf2);
														char *p=md5;
														char i =0;
														for(i=0;i<16;i++)
														{
															sprintf(p,"%02x",buf2[i]);
															p+=2;
														}
														if(DEBUGLOG) COMM_RUN_LogOut("EASE_DC_CTL.bin  MD5=%s,checked MD5=%s",buf,md5);
														if(strncmp(buf,md5,strlen(md5))!=0)
														{	
															Update_EASE_DC_CTL_falg = 1;	
															system("rm -rf /mnt/Nand1/update/EASE_DC_CTL.bin"); //文件校验异常
														}
														else
														{
															Update_EASE_DC_CTL_falg = 0;
														}
													}
											}
											fclose(fd);	
											
											if((Update_Electric_Charge_Pile_falg == 0)&&(access(F_UPDATE_Electric_Charge_Pile,F_OK) != -1)){//存在更新文件
												system("cp -rf /mnt/Nand1/update/Electric_Charge_Pile /home/root/");
												sleep(4);
												system("chmod 777 /home/root/Electric_Charge_Pile");
												sleep(1);
												system("rm -rf /mnt/Nand1/update/Electric_Charge_Pile");
												sleep(1);
											}
											if((Update_Charger_falg == 0)&&(access(F_UPDATE_Charger,F_OK) != -1)){//存在更新文件
												system("cp -rf /mnt/Nand1/update/charger /home/root/");
												sleep(4);
												system("chmod 777 /home/root/charger");
												sleep(1);
												system("rm -rf /mnt/Nand1/update/charger");
												sleep(1);
											}
											Globa_1->Software_Version_change  = 0;
											system("sync");
											system("reboot");
										}
										else
										{
											Globa_1->Software_Version_change  = 0;
											system("rm -rf /mnt/Nand1/update/*");
											sleep(1);
											system("sync");
											sleep(1);
											system("reboot");
										}
									}
									else
									{
										if(access(F_UPDATE_Electric_Charge_Pile,F_OK) != -1){//存在更新文件
											system("cp -rf /mnt/Nand1/update/Electric_Charge_Pile /home/root/");
											sleep(4);
											system("chmod 777 /home/root/Electric_Charge_Pile");
											sleep(1);
											system("rm -rf /mnt/Nand1/update/Electric_Charge_Pile");
											sleep(1);
										}
										if(access(F_UPDATE_Charger,F_OK) != -1){//存在更新文件
											system("cp -rf /mnt/Nand1/update/charger /home/root/");
											sleep(4);
											system("chmod 777 /home/root/charger");
											sleep(1);
											system("rm -rf /mnt/Nand1/update/charger");
											sleep(1);
										}
										Globa_1->Software_Version_change  = 0;
										system("sync");
										sleep(5);
										system("reboot");
									}
								}
							}
						  break;
					  }
						default:{
						 break;
						}
					}
				  if(abs(current_now - Signals_value_over_time) >= 30){
			      tx_len = InDTU332W_Status_RD_Frame(dtu_send_buf,TAG_CSQ_ST);
				    ISO8583_Sent_Data(ISO8583_App_Struct.Heart_Socket_fd , dtu_send_buf, tx_len,0);
						COMM_RUN_LogOut("send DTU信号请求:");  

					  Signals_value_over_time = current_now;
				  }
				}
				break;
			}
		}
	}
}

/*********************************************************
**description：ISO8583_Heart_Thread 线程初始化及建立
***parameter  :none
***return		  :none
**********************************************************/
extern void ISO8583_Heart_Thread(void)
{
	pthread_t td1;
	int ret ,stacksize = 2*1024*1024;//根据线程的实际情况设定大小
	pthread_attr_t attr;

	ret = pthread_attr_init(&attr);
	if (ret != 0)
		return;

	ret = pthread_attr_setstacksize(&attr, stacksize);
	if(ret != 0)
		return;
	
	if(0 != pthread_create(&td1, &attr, (void *)ISO8583_Heart_Task, NULL)){
		return;
	}

	if(0 != pthread_attr_destroy(&attr)){
		return;
	}
}

/*********************************************************
**description：ISO8583_Busy_Thread 线程初始化及建立
***parameter  :none
***return		  :none
**********************************************************/
extern void ISO8583_Busy_Thread(void)
{
	pthread_t td1;
	int ret ,stacksize = 4*1024*1024;//根据线程的实际情况设定大小
	pthread_attr_t attr;

	ret = pthread_attr_init(&attr);
	if (ret != 0)
		return;

	ret = pthread_attr_setstacksize(&attr, stacksize);
	if(ret != 0)
		return;
	
	/*if(0 != pthread_create(&td1, &attr, (void *)ISO8583_Busy_Task, NULL)){
		return;
	}*/
	
	if(0 != pthread_create(&td1, &attr, (void *)ISO8583_Rcev_Task, NULL)){
		return;
	}
	if(0 != pthread_attr_destroy(&attr)){
		return;
	}
}



//充电桩上传升级请求   发送上行数据
int Lan_ISO8583_AP1060_Request_sent(int fd,UINT32 data_block_num_want)
{
	unsigned int ret,len;
	unsigned char SendBuf[512];  //发送数据区指针
	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针
	unsigned short tx_len = sizeof(ISO8583_Head_Frame) +  sizeof(ISO8583_APUPDATE_Frame);//
	time_t systime=0;
	struct tm tm_t;

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_APUPDATE_Frame *Update_Data_Frame;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	Update_Data_Frame = (ISO8583_APUPDATE_Frame *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据初始化
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'
	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_APUPDATE_Frame))>>8;  //报文长度
	Head_Frame->SessionLen[1] = sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_APUPDATE_Frame);
	memcpy(&Head_Frame->MessgTypeIdent[0], "1060", sizeof(Head_Frame->MessgTypeIdent));//消息标识符
	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化帧数据内容为 0x00

	Head_Frame->PrimaryBitmap[11/8 -0] |= 0x80 >> ((11-1)%8);//主位元表 12
	Head_Frame->PrimaryBitmap[41/8 -0] |= 0x80 >> ((41-1)%8);//主位元表 12
	Head_Frame->PrimaryBitmap[52/8 -0] |= 0x80 >> ((52-1)%8);//主位元表 42
	Head_Frame->PrimaryBitmap[62/8 -0] |= 0x80 >> ((62-1)%8);//主位元表 62
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63
	
//填充业务数据内容

	sprintf(&tmp[0], "%08d", data_block_num_want);				
	memcpy(&Update_Data_Frame->data_block_num[0], &tmp[0], sizeof(Update_Data_Frame->data_block_num));
	memcpy(&Update_Data_Frame->charger_sn[0], &Globa_1->Charger_param.SN[0], sizeof(Update_Data_Frame->charger_sn));//充电终端序列号
	memcpy(&Update_Data_Frame->chg_soft_ver_get[0], &Globa_1->tmp_Software_Version[0], sizeof(Update_Data_Frame->chg_soft_ver_get));//黑名单数据版本号

	systime = time(NULL);        //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
	sprintf(&tmp[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
	memcpy(&Update_Data_Frame->time[0], &tmp[0], sizeof(Update_Data_Frame->time));//终端时间（格式yyyyMMddHHmmss）	62	N	N14
	MD5_DES(&Update_Data_Frame->data_block_num[0], sizeof(ISO8583_APUPDATE_Frame)-sizeof(Update_Data_Frame->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&Update_Data_Frame->MD5[0], &MD5[0], 8);
	len = send(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_APUPDATE_Frame)), 0);
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_APUPDATE_Frame))){//如果发送不成功
		return (-1);
	}else{
		return (0);
	}
}

int Lan_ISO8583_AP1060_Request_Data_Request_Deal(int fd)
{
	int i,j,ret;
	struct timeval tv1;
	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针
	fd_set fdsr;
	unsigned char buf[1024];
	unsigned char sn[20];
	unsigned char write_buf[320];
  INT32 fd_Update,length = 0;
  static FILE *fp_Update = NULL;
	int bag_now,count,total,total_tmp;
	UINT32 ls_curent_data_block_num = 0;//需要的包见

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_APUPDATE_Resp_Frame * Update_Resp_data;
	Head_Frame = (ISO8583_Head_Frame *)(buf);
	Update_Resp_data = (ISO8583_APUPDATE_Resp_Frame *)(buf+sizeof(ISO8583_Head_Frame));
	
	ret = ISO8583_AP1060_Request_sent(fd,curent_data_block_num);//发送请求帧
	if(ret < 0){//发送失败
		return (-1);
	}
	FD_ZERO(&fdsr); //每次进入循环都重建描述符集
	FD_SET(fd, &fdsr);

	tv1.tv_sec = 10;
	tv1.tv_usec = 0;
	ret = select(fd + 1, &fdsr, NULL, NULL, &tv1);
	if(ret > 0){//接收到上位机响应
  	if(FD_ISSET(fd, &fdsr)){
  		memset(&buf[0], 0x00, sizeof(buf));
  		ret = recv(fd, buf, sizeof(buf), 0);

#if 0
  			printf("ISO8583_have_card_change_Deal  recv server length = %d,data= ", ret);
  			for(j=0; j<ret; j++)printf("%02x ",buf[j]);
  			printf("\n");
#endif
			if(ret > (sizeof(Update_Resp_data->MD5)+sizeof(ISO8583_Head_Frame))){//有数据响应，但不判断长度（因为长度不定）
  			if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "1061", sizeof(Head_Frame->MessgTypeIdent))){
					MD5_DES(&Update_Resp_data->curent_data_block_num[0],  (ret - sizeof(Update_Resp_data->MD5)-sizeof(ISO8583_Head_Frame)), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	        hex_to_str(&tmp[0], &MD5[0], 8);
					if(0 == memcmp(&tmp[0], &buf[ret - sizeof(Update_Resp_data->MD5)], sizeof(Update_Resp_data->MD5))){
						if((Update_Resp_data->flag[0] == '0')&&(Update_Resp_data->flag[1] == '0')){ //信息处理成功
							All_Data_Num = convert_8ascii_2UL(&Update_Resp_data->all_data_num[0]);
							ls_curent_data_block_num = convert_8ascii_2UL(&Update_Resp_data->curent_data_block_num[0]);
							
							if(ls_curent_data_block_num == 1){//第一个数据包
								fp_Update = fopen(F_UPDATE_Charger_Package, "wb+");
								if(fp_Update == NULL){
									curent_data_block_num = 1;
									fclose(fp_Update);
									return -1;
								}else{
									if(ls_curent_data_block_num == curent_data_block_num){
										length = (Update_Resp_data->data_len[0]-'0')*100 + (Update_Resp_data->data_len[1]-'0')*10 + (Update_Resp_data->data_len[2]-'0');
										str_to_hex(write_buf, &Update_Resp_data->data[0], length);
										fwrite(&write_buf, sizeof(UINT8), length/2, fp_Update);
										curent_data_block_num ++;
									}
								}
							}else{
								if(ls_curent_data_block_num == curent_data_block_num){
									length = (Update_Resp_data->data_len[0]-'0')*100 + (Update_Resp_data->data_len[1]-'0')*10 + (Update_Resp_data->data_len[2]-'0');
									str_to_hex(write_buf, &Update_Resp_data->data[0], length);
									fwrite(&write_buf, sizeof(UINT8), length/2, fp_Update);	
									if(curent_data_block_num == All_Data_Num ){//表面数据接收完毕
										fd_Update = fileno(fp_Update);
										fsync(fd_Update);//new add
										fclose(fp_Update);
										fp_Update = NULL;
										return 2; //表示更新完成程序
									}else{
										curent_data_block_num ++;
									}
								}
							}
							return 0;
						}else{
						  return 3;
						}
					}
  			}
  		}
  	}
  }
	return -1;
}



//充电桩上传升级确认   发送上行数据
int Lan_ISO8583_AP1070_Request_sent(int fd)
{
	unsigned int ret,len;
	unsigned char SendBuf[512];  //发送数据区指针
	unsigned char tmp[100];
	unsigned char MD5[8];  //发送数据区指针
	unsigned short tx_len = sizeof(ISO8583_Head_Frame) +  sizeof(ISO8583_AP1070_Frame);//
	time_t systime=0;
	struct tm tm_t;

	ISO8583_Head_Frame  *Head_Frame;
	ISO8583_AP1070_Frame *Update_Data_Frame;

	Head_Frame = (ISO8583_Head_Frame *)(SendBuf);
	Update_Data_Frame = (ISO8583_AP1070_Frame *)(SendBuf+sizeof(ISO8583_Head_Frame));

	//数据初始化
	memset(&Head_Frame->SessionLen[0], '0', sizeof(ISO8583_Head_Frame));//初始化帧数据内容为'0'
	Head_Frame->SessionLen[0] = (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP1070_Frame))>>8;  //报文长度
	Head_Frame->SessionLen[1] = sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP1070_Frame);
	memcpy(&Head_Frame->MessgTypeIdent[0], "1070", sizeof(Head_Frame->MessgTypeIdent));//消息标识符
	memset(&Head_Frame->PrimaryBitmap[0], 0x00, sizeof(Head_Frame->PrimaryBitmap));//初始化帧数据内容为 0x00

	Head_Frame->PrimaryBitmap[41/8 -0] |= 0x80 >> ((41-1)%8);//主位元表 12
	Head_Frame->PrimaryBitmap[52/8 -0] |= 0x80 >> ((52-1)%8);//主位元表 42
	Head_Frame->PrimaryBitmap[62/8 -0] |= 0x80 >> ((62-1)%8);//主位元表 62
	Head_Frame->PrimaryBitmap[63/8 -0] |= 0x80 >> ((63-1)%8);//主位元表 63
	

//填充业务数据内容

	memcpy(&Update_Data_Frame->charger_sn[0], &Globa_1->Charger_param.SN[0], sizeof(Update_Data_Frame->charger_sn));//充电终端序列号
	memcpy(&Update_Data_Frame->chg_soft_ver[0], &Globa_1->Charger_param2.Software_Version[0], sizeof(Update_Data_Frame->chg_soft_ver));//黑名单数据版本号

	systime = time(NULL);        //获得系统的当前时间 
	localtime_r(&systime, &tm_t);  //结构指针指向当前时间	
	sprintf(&tmp[0], "%04d%02d%02d%02d%02d%02d", tm_t.tm_year+1900, tm_t.tm_mon+1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
	memcpy(&Update_Data_Frame->time[0], &tmp[0], sizeof(Update_Data_Frame->time));//终端时间（格式yyyyMMddHHmmss）	62	N	N14
	MD5_DES(&Update_Data_Frame->charger_sn[0], sizeof(ISO8583_AP1070_Frame)-sizeof(Update_Data_Frame->MD5), &ISO8583_App_Struct.work_key[0], &MD5[0]);
	hex_to_str(&Update_Data_Frame->MD5[0], &MD5[0], 8);
	len = send(fd, SendBuf, (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP1070_Frame)), 0);
	if(len != (sizeof(ISO8583_Head_Frame)+sizeof(ISO8583_AP1070_Frame))){//如果发送不成功
		return (-1);
	}else{
		return (0);
	}
}

//升级完成之后，确定指令
int Lan_ISO8583_AP1070_Deal(int fd)
{
	int i,j,ret;
	struct timeval tv1;
	unsigned char tmp[100];
	fd_set fdsr;
	unsigned char buf[1024];
	unsigned char sn[20];
	unsigned char write_buf[320];
  INT32 fd_Update,length = 0;

	int bag_now,count,total,total_tmp;

	ISO8583_Head_Frame  *Head_Frame;
	Head_Frame = (ISO8583_Head_Frame *)(buf);
	ret = ISO8583_AP1070_Request_sent(fd);//发送请求帧
	if(ret < 0){//发送失败
		return (-1);
	}
	FD_ZERO(&fdsr); //每次进入循环都重建描述符集
	FD_SET(fd, &fdsr);

	tv1.tv_sec = 10;
	tv1.tv_usec = 0;
	ret = select(fd + 1, &fdsr, NULL, NULL, &tv1);
	if(ret > 0){//接收到上位机响应
  	if(FD_ISSET(fd, &fdsr)){
  		memset(&buf[0], 0x00, sizeof(buf));
  		ret = recv(fd, buf, sizeof(buf), 0);

#if ISO8583_DEBUG == 1
  			printf("ISO8583_have_card_change_Deal  recv server length = %d,data= ", ret);
  			for(j=0; j<ret; j++)printf("%02x ",buf[j]);
  			printf("\n");
#endif
			if(ret > 0){//有数据响应，但不判断长度（因为长度不定）
  			if(0 == memcmp(&Head_Frame->MessgTypeIdent[0], "1071", sizeof(Head_Frame->MessgTypeIdent))){
					if((buf[sizeof(ISO8583_Head_Frame)] == '0')&&(buf[sizeof(ISO8583_Head_Frame)+1]== '0')){ //信息处理成功
						return 0;
					}
  			}
  		}
  	}
  }
	return -1;
}




//软件版本升级线程
int ISO8583_Update_Package_Task(void)
{
	int fd, ret,old_Heart_TCP_state =255;
	unsigned char busy_sn[ISO8583_BUSY_SN+1]={0};
	int Heart_TCP_state = 0x01,data_block_num_want = 1;
	unsigned int up_num = 0;//标记数据的上传记录次数
  char read_buf[1024], temp_char[1024];
	
	char Update_EASE_DC_CTL_falg = 0;
	char Update_Electric_Charge_Pile_falg = 0;
	char Update_Charger_falg =0;
	char buf[100] = {0}, buf2[100]= {0}, md5[100]= {0};						
  Globa_1->Updat_data_value = 0;
  Globa_1->Software_Version_change = 0;
	sleep(5);
	prctl(PR_SET_NAME,(unsigned long)"Lan_Update_Task");//设置线程名字 

	while(1){
		ISO8583_Busy_Watchdog = 1;
		if(Globa_1->have_hart == 1){
			if(Heart_TCP_state !=old_Heart_TCP_state){
			//	printf("进行升级请求中0---Heart_TCP_state =%d-\n",Heart_TCP_state);
				old_Heart_TCP_state = Heart_TCP_state;
				}
			if(Globa_1->Software_Version_change == 0){
				sleep(3);
				Heart_TCP_state = 0x01;
				/*if(access(F_UPDATE_Charger_Package,F_OK) != -1){//存在更新文件
					 system("tar -zvxf /mnt/Nand1/update/DC_Charger.tar.gz  -C  /mnt/Nand1/update/");
					 usleep(500000);
					 Heart_TCP_state = 0x04; //进行升级
					 system("sync");
					 Globa_1->Software_Version_change = 1;
				}*/
			}else{
				usleep(10000);
				switch(Heart_TCP_state){
					case 0x01:{//创建连接
						fd = TCP_Client_Socket_Creat(Globa_1->ISO8583_NET_setting.ISO8583_Server_IP, \
																			 Globa_1->ISO8583_NET_setting.ISO8583_Server_port[0]+(Globa_1->ISO8583_NET_setting.ISO8583_Server_port[1]<<8), 10*1000000);
						if(fd < 0){
							sleep(10);//间隔时间必须有，间隔5秒再从新建立连接（这样连接失败的间隔时间最大为(10+ 5)s，最小为5s）
							printf("软件版本升级线程 failed\n");
							close(fd);
						}else{//连接建立成功
							Heart_TCP_state = 0x02; //进行升级
							data_block_num_want = 1;
						}
						break ;
					}
					case 0x02:{//进行升级请求
						ret = Lan_ISO8583_AP1060_Request_Data_Request_Deal(fd);
				
						if(ret == 2){//表示升级完成，之后进行文件的解压
							memcpy(&Globa_1->Charger_param2.Software_Version[0], &Globa_1->tmp_Software_Version[0], sizeof(Globa_1->tmp_Software_Version));//软件包升级版本更新
							System_param2_save();
							Heart_TCP_state = 0x03; //进行升级
							Globa_1->Updat_data_value = 100;
						}else if(ret < 0){
							sleep(5);//间隔时间必须有，间隔5秒再从新建立连接（这样连接失败的间隔时间最大为(10+ 5)s，最小为5s）
							close(fd);
							Heart_TCP_state = 0x01; //进行升级
						}else if(ret == 3){//表示升级完成，之后进行文件的解压
	            memcpy(&Globa_1->Charger_param2.Software_Version[0], &Globa_1->tmp_Software_Version[0], sizeof(Globa_1->tmp_Software_Version));//软件包升级版本更新
							System_param2_save();
							Globa_1->Updat_data_value = 0;
							Heart_TCP_state = 0x01; //进行升级
							system("rm -rf /mnt/Nand1/update/DC_Charger.tar.gz");
							Globa_1->Software_Version_change  = 0;
						}else{
							//printf("进行升级请求中0---curent_data_block_num = %d-\n",curent_data_block_num);
						}
						if(All_Data_Num != 0){
							Globa_1->Updat_data_value = ((curent_data_block_num*100)/All_Data_Num);
						}
						break;
					}
					case 0x03:{//升级之后确认
						 ret = Lan_ISO8583_AP1070_Deal(fd);
						 if(access(F_UPDATE_Charger_Package,F_OK) != -1){//存在更新文件
							 system("tar -zvxf /mnt/Nand1/update/DC_Charger.tar.gz  -C  /mnt/Nand1/update/");
							 Heart_TCP_state = 0x04; //进行升级
							 sleep(10);
							 system("sync");
						 }
						 break;
					}
		      case 0x04:{
						if((access(F_UPDATE_Electric_Charge_Pile,F_OK) != -1)||(access(F_UPDATE_Charger,F_OK) != -1)||(access(F_UPDATE_EAST_DC_CTL,F_OK) != -1)){//存在更新文件
							sleep(1);
							system("rm -rf /mnt/Nand1/update/DC_Charger.tar.gz");
						  Heart_TCP_state = 0x05; //进行升级
						  Update_EASE_DC_CTL_falg = 0;
							Update_Electric_Charge_Pile_falg = 0;
							Update_Charger_falg =0;
						}else{
							 Globa_1->Software_Version_change  = 0;
							 system("rm -rf /mnt/Nand1/update/*");
							 sleep(1);
							 system("sync");
							 sleep(1);
							 system("reboot");
						}
						break;
					}
				  case 0x05:{//等待系统重启升级
						if(Globa_1->QT_Step == 0x02){//系统处于空闲的时候
							if(access(F_UPDATE_Charger_Conf,F_OK) != -1){
								FILE *fp_new = fopen(F_UPDATE_Charger_Conf, "r");
								if(fp_new != NULL){
									char temp_char[100],read_buf[1024];
									while(fgets(&read_buf,1024, fp_new) > 0){
										sprintf(&temp_char[0], "%s", read_buf);	
										system(temp_char);	
										usleep(500000);
									}
								}
							}else{
								if(access(F_UPDATE_CHECK_INFO,F_OK) == 0)//升级文件信息 --进行文件校验
								{
									printf("fopen /mnt/Nand1/update/update.info\n");
									FILE *fd=fopen(F_UPDATE_CHECK_INFO,"r");
									if(fd)
									{
										if(access(F_UPDATE_Charger,F_OK) != -1)
										{
												if(fgets(buf,100,fd))
												{
													MD5File(F_UPDATE_Charger,buf2);
													char *p=md5;
													char i =0;
													for(i=0;i<16;i++)
													{
														sprintf(p,"%02x",buf2[i]);
														p+=2;
													}
													if(DEBUGLOG) COMM_RUN_LogOut("Update_Charger: MD5=%s,checked MD5=%s",buf,md5);
													if(strncmp(buf,md5,strlen(md5))!=0)
													{	
                            Update_Charger_falg = 1;	
										        system("rm -rf /mnt/Nand1/update/charger"); //文件校验异常
													}
													else
													{
														Update_Charger_falg = 0;
													}
												}
										}
										if(access(F_UPDATE_Electric_Charge_Pile,F_OK) != -1)
										{
												if(fgets(buf,100,fd))
												{
													MD5File(F_UPDATE_Electric_Charge_Pile,buf2);
													char *p=md5;
													char i =0;
													for(i=0;i<16;i++)
													{
														sprintf(p,"%02x",buf2[i]);
														p+=2;
													}
													if(DEBUGLOG) COMM_RUN_LogOut("Electric_Charge_Pile: MD5=%s,checked MD5=%s",buf,md5);
													if(strncmp(buf,md5,strlen(md5))!=0)
													{	
                            Update_Electric_Charge_Pile_falg = 1;	
										        system("rm -rf /mnt/Nand1/update/Electric_Charge_Pile"); //文件校验异常
													}
													else
													{
														Update_Electric_Charge_Pile_falg = 0;
													}
												}
										}
										if(access(F_UPDATE_EAST_DC_CTL,F_OK) != -1)
										{
												if(fgets(buf,100,fd))
												{
													MD5File(F_UPDATE_EAST_DC_CTL,buf2);
													char *p=md5;
													char i =0;
													for(i=0;i<16;i++)
													{
														sprintf(p,"%02x",buf2[i]);
														p+=2;
													}
													if(DEBUGLOG) COMM_RUN_LogOut("EASE_DC_CTL.bin  MD5=%s,checked MD5=%s",buf,md5);
													if(strncmp(buf,md5,strlen(md5))!=0)
													{	
                            Update_EASE_DC_CTL_falg = 1;	
										        system("rm -rf /mnt/Nand1/update/EASE_DC_CTL.bin"); //文件校验异常
													}
													else
													{
														Update_EASE_DC_CTL_falg = 0;
													}
												}
										}
										fclose(fd);	
										
										if((Update_Electric_Charge_Pile_falg == 0)&&(access(F_UPDATE_Electric_Charge_Pile,F_OK) != -1)){//存在更新文件
											system("cp -rf /mnt/Nand1/update/Electric_Charge_Pile /home/root/");
											sleep(4);
											system("chmod 777 /home/root/Electric_Charge_Pile");
											sleep(1);
											system("rm -rf /mnt/Nand1/update/Electric_Charge_Pile");
											sleep(1);
										}
										if((Update_Charger_falg == 0)&&(access(F_UPDATE_Charger,F_OK) != -1)){//存在更新文件
											system("cp -rf /mnt/Nand1/update/charger /home/root/");
											sleep(4);
											system("chmod 777 /home/root/charger");
											sleep(1);
											system("rm -rf /mnt/Nand1/update/charger");
											sleep(1);
										}
										Globa_1->Software_Version_change  = 0;
										system("sync");
										system("reboot");
									}
									else
									{
						       	Globa_1->Software_Version_change  = 0;
										system("rm -rf /mnt/Nand1/update/*");
										sleep(1);
									  system("sync");
									  sleep(1);
									  system("reboot");
								  }
								}
								else
								{
									if(access(F_UPDATE_Electric_Charge_Pile,F_OK) != -1){//存在更新文件
										system("cp -rf /mnt/Nand1/update/Electric_Charge_Pile /home/root/");
										sleep(4);
										system("chmod 777 /home/root/Electric_Charge_Pile");
										sleep(1);
										system("rm -rf /mnt/Nand1/update/Electric_Charge_Pile");
										sleep(1);
									}
									if(access(F_UPDATE_Charger,F_OK) != -1){//存在更新文件
										system("cp -rf /mnt/Nand1/update/charger /home/root/");
										sleep(4);
										system("chmod 777 /home/root/charger");
										sleep(1);
										system("rm -rf /mnt/Nand1/update/charger");
										sleep(1);
									}
									Globa_1->Software_Version_change  = 0;
									system("sync");
									sleep(5);
									system("reboot");
								}
						  }
						}
						break;
					}
				}
			}
		}else{
			sleep(10);
		}
  }
}

/*********************************************************
**description：ISO8583_Busy_Thread 线程初始化及建立
***parameter  :none
***return		  :none
**********************************************************/
extern void ISO8583_Update_Thread(void)
{
	pthread_t td1;
	int ret ,stacksize = 4*1024*1024;//根据线程的实际情况设定大小
	pthread_attr_t attr;

	ret = pthread_attr_init(&attr);
	if (ret != 0)
		return;

	ret = pthread_attr_setstacksize(&attr, stacksize);
	if(ret != 0)
		return;
	
	if(0 != pthread_create(&td1, &attr, (void *)ISO8583_Update_Package_Task, NULL)){
		return;
	}

	if(0 != pthread_attr_destroy(&attr)){
		return;
	}
}


