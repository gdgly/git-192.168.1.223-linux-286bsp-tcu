/*******************************************************************************
  Copyright(C)    2014-2015,      EAST Co.Ltd.
  File name :     Deal_Handler.c
  Author :        dengjh
  Version:        1.0
  Date:           2015.5.19
  Description:    交易记录数据库处理
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
#include "../../inc/sqlite/sqlite3.h"
#include "Deal_Handler.h"  
#include "common.h"
#include "iso8583.h"

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
int Insert_Charger_Busy_DB(unsigned char db_sel,ISO8583_charge_db_data *frame, unsigned int *id, unsigned int up_num)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;
	char db_path[128];
	char sql[2500];
	sprintf(db_path,"%s/busy_coplete%d.db",F_DATAPATH,db_sel);
	rc = sqlite3_open_v2(db_path, &db,
						 SQLITE_OPEN_NOMUTEX|
						 SQLITE_OPEN_SHAREDCACHE|
						 SQLITE_OPEN_READWRITE |
						 SQLITE_OPEN_CREATE,
						 NULL);
	if(rc){
		printf("[%s]Can't open %s database: %s\n",__FUNCTION__,db_path,sqlite3_errmsg(db));
		sqlite3_close(db);
		if(DEBUGLOG) GLOBA_RUN_LogOut("打开数据库%s失败:%s\n", db_path,sqlite3_errmsg(db));  
		return -1;
	}

	sprintf(sql,"CREATE TABLE if not exists Charger_Busy_Complete_Data(ID INTEGER PRIMARY KEY autoincrement, ");
	strcat(sql,"charger_SN TEXT, ");//1
	strcat(sql,"chg_port INTEGER, ");//2
	strcat(sql,"deal_SN TEXT, ");//3
	strcat(sql,"card_sn TEXT, ");//4
	strcat(sql,"chg_start_by INTEGER, ");//5
	strcat(sql,"card_deal_result INTEGER, ");//6
	strcat(sql,"start_time TEXT, ");//7
	strcat(sql,"end_time TEXT, ");//8
	strcat(sql,"start_kwh INTEGER, ");//9
	strcat(sql,"end_kwh INTEGER, ");//10
	strcat(sql,"kwh1_price INTEGER, ");//11
	strcat(sql,"kwh1_charged INTEGER, ");//12
	strcat(sql,"kwh1_money INTEGER, ");//13
	strcat(sql,"kwh2_price INTEGER, ");//14
	strcat(sql,"kwh2_charged INTEGER, ");//15
	strcat(sql,"kwh2_money INTEGER, ");//16
	strcat(sql,"kwh3_price INTEGER, ");//17
	strcat(sql,"kwh3_charged INTEGER, ");//18
	strcat(sql,"kwh3_money INTEGER, ");//19
	strcat(sql,"kwh4_price INTEGER, ");//20
	strcat(sql,"kwh4_charged INTEGER, ");//21
	strcat(sql,"kwh4_money INTEGER, ");//22
	
	strcat(sql,"kwh_total_charged INTEGER, ");//23
	strcat(sql,"money_left INTEGER, ");//24
	strcat(sql,"last_rmb INTEGER, ");//25
	strcat(sql,"rmb_kwh INTEGER, ");//26
	strcat(sql,"total_kwh_money INTEGER, ");//27
	strcat(sql,"car_VIN TEXT, ");//28
	
	strcat(sql,"kwh1_service_price INTEGER, ");//29
	strcat(sql,"kwh1_service_money INTEGER, ");//30
	strcat(sql,"kwh2_service_price INTEGER, ");//31
	strcat(sql,"kwh2_service_money INTEGER, ");//32
	strcat(sql,"kwh3_service_price INTEGER, ");//33
	strcat(sql,"kwh3_service_money INTEGER, ");//34
	strcat(sql,"kwh4_service_price INTEGER, ");//35
	strcat(sql,"kwh4_service_money INTEGER, ");	//36
	strcat(sql,"book_price INTEGER, ");//37
	strcat(sql,"book_money INTEGER, ");//38
	strcat(sql,"park_price INTEGER, ");//39
	strcat(sql,"park_money INTEGER, ");//40
	strcat(sql,"all_chg_money INTEGER, ");//41
	strcat(sql,"end_reason INTEGER, ");//42
	strcat(sql,"Car_AllMileage INTEGER, ");//43	
	strcat(sql,"Start_SOC INTEGER, ");//44
	strcat(sql,"End_SOC INTEGER, ");//45	
	strcat(sql,"up_num INTEGER);");//46
		
	if(sqlite3_exec(db, sql, 0, 0, &zErrMsg) != SQLITE_OK){
		//printf("CREATE TABLE Charger_Busy_Complete_Data error: %s\n", sqlite3_errmsg(db));
		sqlite3_free(zErrMsg);
	}

	if(TRANSACTION_FLAG) sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);
	
	sprintf(sql, "INSERT INTO Charger_Busy_Complete_Data VALUES (NULL, ");
	sprintf(&sql[strlen(sql)],"'%.16s',", frame->charger_SN);
	sprintf(&sql[strlen(sql)],"%d,", frame->chg_port);
	sprintf(&sql[strlen(sql)],"'%.20s',", frame->deal_SN);
	sprintf(&sql[strlen(sql)],"'%.16s',", frame->card_sn);
	sprintf(&sql[strlen(sql)],"%d,", frame->chg_start_by);
	sprintf(&sql[strlen(sql)],"%d,", frame->card_deal_result);
	sprintf(&sql[strlen(sql)],"'%.14s',", frame->start_time);
	sprintf(&sql[strlen(sql)],"'%.14s',", frame->end_time);
	sprintf(&sql[strlen(sql)],"%d,", frame->start_kwh);
	sprintf(&sql[strlen(sql)],"%d,", frame->end_kwh);	
	sprintf(&sql[strlen(sql)],"%d,", frame->kwh1_price);
	sprintf(&sql[strlen(sql)],"%d,", frame->kwh1_charged);
	sprintf(&sql[strlen(sql)],"%d,", frame->kwh1_money);	
	sprintf(&sql[strlen(sql)],"%d,", frame->kwh2_price);
	sprintf(&sql[strlen(sql)],"%d,", frame->kwh2_charged);
	sprintf(&sql[strlen(sql)],"%d,", frame->kwh2_money); 	
	sprintf(&sql[strlen(sql)],"%d,", frame->kwh3_price);
	sprintf(&sql[strlen(sql)],"%d,", frame->kwh3_charged);
	sprintf(&sql[strlen(sql)],"%d,", frame->kwh3_money);	
	sprintf(&sql[strlen(sql)],"%d,", frame->kwh4_price);
	sprintf(&sql[strlen(sql)],"%d,", frame->kwh4_charged);
	sprintf(&sql[strlen(sql)],"%d,", frame->kwh4_money);		
	sprintf(&sql[strlen(sql)],"%d,", frame->kwh_total_charged);
	sprintf(&sql[strlen(sql)],"%d,", frame->money_left);
	sprintf(&sql[strlen(sql)],"%d,", frame->last_rmb);	
	sprintf(&sql[strlen(sql)],"%d,", frame->rmb_kwh);
	sprintf(&sql[strlen(sql)],"%d,", frame->total_kwh_money);	
	sprintf(&sql[strlen(sql)],"'%.17s',", frame->car_VIN);	
	sprintf(&sql[strlen(sql)],"%d,", frame->kwh1_service_price);
	sprintf(&sql[strlen(sql)],"%d,", frame->kwh1_service_money);
	sprintf(&sql[strlen(sql)],"%d,", frame->kwh2_service_price);
	sprintf(&sql[strlen(sql)],"%d,", frame->kwh2_service_money);
	sprintf(&sql[strlen(sql)],"%d,", frame->kwh3_service_price);
	sprintf(&sql[strlen(sql)],"%d,", frame->kwh3_service_money);
	sprintf(&sql[strlen(sql)],"%d,", frame->kwh4_service_price);
	sprintf(&sql[strlen(sql)],"%d,", frame->kwh4_service_money);	
	sprintf(&sql[strlen(sql)],"%d,", frame->book_price);
	sprintf(&sql[strlen(sql)],"%d,", frame->book_money);	
	sprintf(&sql[strlen(sql)],"%d,", frame->park_price);
	sprintf(&sql[strlen(sql)],"%d,", frame->park_money);	
	sprintf(&sql[strlen(sql)],"%d,", frame->all_chg_money);
	sprintf(&sql[strlen(sql)],"%d,", frame->end_reason);	
	sprintf(&sql[strlen(sql)],"%d,", frame->Car_AllMileage);
	sprintf(&sql[strlen(sql)],"%d,", frame->Start_Soc);
	sprintf(&sql[strlen(sql)],"%d,", frame->End_Soc);

	sprintf(&sql[strlen(sql)],"%d);", up_num);

	if(0==db_sel)
 	{
		if(DEBUGLOG) GLOBA_RUN_LogOut("插入总数据库：%s",sql);  
	}
	else
	{
		if(DEBUGLOG) GLOBA_RUN_LogOut("插入枪%d数据库：%s",db_sel,sql);  
	}
	if(sqlite3_exec(db , sql , 0 , 0 , &zErrMsg) != SQLITE_OK){
		printf("Insert DI error: %s\n", sqlite3_errmsg(db));
		sqlite3_free(zErrMsg);
		if(TRANSACTION_FLAG) sqlite3_exec(db, "rollback transaction", NULL, NULL, NULL);
	  //if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
		if(DEBUGLOG) GLOBA_RUN_LogOut("插入命令执行失败枪号: %d",frame->chg_port);  
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

//更新指定id的记录中的部分字段
int Update_Charger_Busy_DB(unsigned char db_sel,unsigned int id, ISO8583_charge_db_data *frame,  unsigned int up_num)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;
	char car_VIN[18]={'\0'};

	char db_path[128];
	char sql[2500];
	sprintf(db_path,"%s/busy_coplete%d.db",F_DATAPATH,db_sel);
	rc = sqlite3_open_v2(db_path, &db,
						 SQLITE_OPEN_NOMUTEX|
						 SQLITE_OPEN_SHAREDCACHE|
						 SQLITE_OPEN_READWRITE |
						 SQLITE_OPEN_CREATE,
						 NULL);
	if(rc){
		printf("[%s]Can't open %s database: %s\n",__FUNCTION__,db_path,sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}
	
	sprintf(sql,"CREATE TABLE if not exists Charger_Busy_Complete_Data(ID INTEGER PRIMARY KEY autoincrement, ");
	strcat(sql,"charger_SN TEXT, ");//1
	strcat(sql,"chg_port INTEGER, ");//2
	strcat(sql,"deal_SN TEXT, ");//3
	strcat(sql,"card_sn TEXT, ");//4
	strcat(sql,"chg_start_by INTEGER, ");//5
	strcat(sql,"card_deal_result INTEGER, ");//6
	strcat(sql,"start_time TEXT, ");//7
	strcat(sql,"end_time TEXT, ");//8
	strcat(sql,"start_kwh INTEGER, ");//9
	strcat(sql,"end_kwh INTEGER, ");//10
	strcat(sql,"kwh1_price INTEGER, ");//11
	strcat(sql,"kwh1_charged INTEGER, ");//12
	strcat(sql,"kwh1_money INTEGER, ");//13
	strcat(sql,"kwh2_price INTEGER, ");//14
	strcat(sql,"kwh2_charged INTEGER, ");//15
	strcat(sql,"kwh2_money INTEGER, ");//16
	strcat(sql,"kwh3_price INTEGER, ");//17
	strcat(sql,"kwh3_charged INTEGER, ");//18
	strcat(sql,"kwh3_money INTEGER, ");//19
	strcat(sql,"kwh4_price INTEGER, ");//20
	strcat(sql,"kwh4_charged INTEGER, ");//21
	strcat(sql,"kwh4_money INTEGER, ");//22
	
	strcat(sql,"kwh_total_charged INTEGER, ");//23
	strcat(sql,"money_left INTEGER, ");//24
	strcat(sql,"last_rmb INTEGER, ");//25
	strcat(sql,"rmb_kwh INTEGER, ");//26
	strcat(sql,"total_kwh_money INTEGER, ");//27
	strcat(sql,"car_VIN TEXT, ");//28
	
	strcat(sql,"kwh1_service_price INTEGER, ");//29
	strcat(sql,"kwh1_service_money INTEGER, ");//30
	strcat(sql,"kwh2_service_price INTEGER, ");//31
	strcat(sql,"kwh2_service_money INTEGER, ");//32
	strcat(sql,"kwh3_service_price INTEGER, ");//33
	strcat(sql,"kwh3_service_money INTEGER, ");//34
	strcat(sql,"kwh4_service_price INTEGER, ");//35
	strcat(sql,"kwh4_service_money INTEGER, ");	//36
	strcat(sql,"book_price INTEGER, ");//37
	strcat(sql,"book_money INTEGER, ");//38
	strcat(sql,"park_price INTEGER, ");//39
	strcat(sql,"park_money INTEGER, ");//40
	strcat(sql,"all_chg_money INTEGER, ");//41	
	strcat(sql,"end_reason INTEGER, ");//42
	strcat(sql,"Car_AllMileage INTEGER, ");//43
	strcat(sql,"Start_Soc INTEGER, ");//44
	strcat(sql,"End_Soc INTEGER, ");//45	
	strcat(sql,"up_num INTEGER);");//46
	
	if(sqlite3_exec(db, sql, 0, 0, &zErrMsg) != SQLITE_OK){
		sqlite3_free(zErrMsg);
	}
	if(TRANSACTION_FLAG) sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);
  
	if(id > 0)
	{
		if((frame->car_VIN[0] == 0xFF)&&(frame->car_VIN[1] == 0xFF)&&(frame->car_VIN[2] == 0xFF))
		{
			sprintf(car_VIN,"%s","00000000000000000");
		}else
		{
			memcpy(car_VIN, &frame->car_VIN, sizeof(car_VIN)-1);
		}
			
		sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET total_kwh_money = %d, ",frame->total_kwh_money);
		sprintf(&sql[strlen(sql)],"kwh_total_charged = %d,", frame->kwh_total_charged);
		sprintf(&sql[strlen(sql)],"rmb_kwh = %d,", frame->rmb_kwh);
		sprintf(&sql[strlen(sql)],"all_chg_money = %d,", frame->all_chg_money);		
		sprintf(&sql[strlen(sql)],"book_money = %d,", frame->book_money);
		sprintf(&sql[strlen(sql)],"park_money = %d,", frame->park_money);
		sprintf(&sql[strlen(sql)],"kwh1_charged = %d,", frame->kwh1_charged);
		sprintf(&sql[strlen(sql)],"kwh1_money = %d,", frame->kwh1_money);
		sprintf(&sql[strlen(sql)],"kwh2_charged = %d,", frame->kwh2_charged);
		sprintf(&sql[strlen(sql)],"kwh2_money = %d,", frame->kwh2_money);
		sprintf(&sql[strlen(sql)],"kwh3_charged = %d,", frame->kwh3_charged);
		sprintf(&sql[strlen(sql)],"kwh3_money = %d,", frame->kwh3_money);
		sprintf(&sql[strlen(sql)],"kwh4_charged = %d,", frame->kwh4_charged);
		sprintf(&sql[strlen(sql)],"kwh4_money = %d,", frame->kwh4_money);
		
		sprintf(&sql[strlen(sql)],"kwh1_service_money = %d,", frame->kwh1_service_money);
		sprintf(&sql[strlen(sql)],"kwh2_service_money = %d,", frame->kwh2_service_money);
		sprintf(&sql[strlen(sql)],"kwh3_service_money = %d,", frame->kwh3_service_money);
		sprintf(&sql[strlen(sql)],"kwh4_service_money = %d,", frame->kwh4_service_money);
		
		sprintf(&sql[strlen(sql)],"end_kwh = %d,", frame->end_kwh);
		sprintf(&sql[strlen(sql)],"end_time = '%.14s',", frame->end_time);
		sprintf(&sql[strlen(sql)],"car_VIN = '%.17s',", car_VIN);//frame->car_VIN);
		sprintf(&sql[strlen(sql)],"end_reason = %d,", frame->end_reason);
		sprintf(&sql[strlen(sql)],"card_deal_result = %d,", frame->card_deal_result);//=====
		sprintf(&sql[strlen(sql)],"last_rmb = %d,", frame->last_rmb);//VIN先启动后获取到余额==========
		sprintf(&sql[strlen(sql)],"money_left = %d,", frame->money_left);//充电后余额==========		
		sprintf(&sql[strlen(sql)],"Car_AllMileage = %d,", frame->Car_AllMileage);//充电后余额==========		
		sprintf(&sql[strlen(sql)],"Start_Soc = %d,", frame->Start_Soc);//开始Soc==========		
		sprintf(&sql[strlen(sql)],"End_Soc = %d,", frame->End_Soc);//结束Soc==========		

		sprintf(&sql[strlen(sql)],"up_num = '%d' WHERE ID = '%d';", up_num,id);
		
			
			if(up_num == 0)
			{//最后一次更新数据
				if(DEBUGLOG) GLOBA_RUN_LogOut("%d号枪更新记录：%s",db_sel,sql);  
			}
			
			if(sqlite3_exec(db, sql, 0, 0, &zErrMsg) != SQLITE_OK)
			{
				printf("UPDATE Charger_Busy_Complete_Data error: %s\n", sqlite3_errmsg(db));
				sqlite3_free(zErrMsg);
				if(TRANSACTION_FLAG)	sqlite3_exec(db, "rollback transaction", NULL, NULL, NULL);
				//if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
				sqlite3_close(db);
				if(DEBUGLOG) GLOBA_RUN_LogOut("%s %s","充电中更新数据失败:",sql);  
				return -1;
			}
	}
	if(TRANSACTION_FLAG) sqlite3_exec(db, "commit transaction", NULL, NULL, NULL);
	sqlite3_close(db);
	return 0;
}


//删除数据库中指定id的记录
int Delete_Record_busy_DB(unsigned char db_sel,unsigned int id)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;

	char db_path[128];
	char sql[512];
	sprintf(db_path,"%s/busy_coplete%d.db",F_DATAPATH,db_sel);
	rc = sqlite3_open_v2(db_path, &db,
						 SQLITE_OPEN_NOMUTEX|
						 SQLITE_OPEN_SHAREDCACHE|
						 SQLITE_OPEN_READWRITE,
						 NULL);
	if(rc){
		printf("[%s]Can't open %s database: %s\n",__FUNCTION__,db_path,sqlite3_errmsg(db));
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
	char db_path[128];
	
	if (type == 0)
	{
		sprintf(db_path,"%s/busy_coplete%d.db",F_DATAPATH,0);
		rc = sqlite3_open_v2(db_path, &db,
							 SQLITE_OPEN_NOMUTEX|
							 SQLITE_OPEN_SHAREDCACHE|
							 SQLITE_OPEN_READWRITE,
							 NULL);
	} 
	else //停车业务流水号
	{
		strcpy(db_path,F_CarLock_Busy_DB);
		rc = sqlite3_open_v2(db_path, &db,
							 SQLITE_OPEN_NOMUTEX|
							 SQLITE_OPEN_SHAREDCACHE|
							 SQLITE_OPEN_READWRITE,
							 NULL);
	}
	if(rc)
	{
		printf("[%s]Can't open %s database: %s\n",__FUNCTION__,db_path,sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}
	if(type == 0)
	{
	   sprintf(sql, "SELECT MAX(ID) FROM Charger_Busy_Complete_Data WHERE deal_SN = '00000000000000000000' AND  up_num = 0 ;");
	}
	else  //停车业务流水号
	{
		sprintf(sql, "SELECT MAX(ID) FROM CarLock_Busy_Data WHERE deal_SN = '00000000000000000000' AND  up_num = 0 ;");
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
	char db_path[128];
	
	if (Busy_SN_Type == 0)
	{
		sprintf(db_path,"%s/busy_coplete%d.db",F_DATAPATH,0);
		rc = sqlite3_open_v2(db_path, &db,
							 SQLITE_OPEN_NOMUTEX|
							 SQLITE_OPEN_SHAREDCACHE|
							 SQLITE_OPEN_READWRITE,
							 NULL);
	}
	else
	{
		strcpy(db_path,F_CarLock_Busy_DB);
		rc = sqlite3_open_v2(db_path, &db,
							 SQLITE_OPEN_NOMUTEX|
							 SQLITE_OPEN_SHAREDCACHE|
							 SQLITE_OPEN_READWRITE,
							 NULL);
	}		
	if(rc)
	{
		printf("[%s]Can't open %s database: %s\n",__FUNCTION__,db_path,sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	if(TRANSACTION_FLAG) sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);
  
	if (Busy_SN_Type == 0)
	{
		sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET deal_SN = '%.20s' WHERE ID = '%d';", Busy_SN, id);
	}
	else 
	{
		sprintf(sql, "UPDATE CarLock_Busy_Data SET deal_SN = '%.20s' WHERE ID = '%d';", Busy_SN, id);
	}
	
	if(sqlite3_exec(db , sql , 0, 0, &zErrMsg) != SQLITE_OK){
		sqlite3_free(zErrMsg);
		if (Busy_SN_Type == 0)
		{
			if(DEBUGLOG) AppLogOut("添加充电业务流水号:%.20s 对应的数据库记录ID：%d",Busy_SN,id);  
		}
		else 
		{
			if(DEBUGLOG) AppLogOut("添加停车业务流水号:%.20s 对应的数据库记录ID：%d",Busy_SN,id);  
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

// 更新临时表的交易流水号
int Set_Temp_Busy_SN(unsigned char db_sel, unsigned char *Busy_SN)
{
    sqlite3 *db = NULL;
    char *zErrMsg = NULL;
    int rc, id = -1;
    int nrow = 0, ncolumn = 0;
    char **azResult = NULL;
    int i, j;
    char car_VIN[18] = { '\0' };
    char db_path[128];
    char sql[512];

    sprintf(db_path, "%s/busy_coplete%d.db", F_DATAPATH, db_sel);

    rc = sqlite3_open_v2(db_path, &db,
        SQLITE_OPEN_NOMUTEX |
        SQLITE_OPEN_SHAREDCACHE |
        SQLITE_OPEN_READWRITE |
        SQLITE_OPEN_CREATE,
        NULL);

    if (rc)
    {
        printf("[%s]Can't open %s database: %s\n", __FUNCTION__, db_path, sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    sprintf(sql, "SELECT MAX(ID) FROM Charger_Busy_Complete_Data WHERE deal_SN = '00000000000000000000' AND  up_num = 85 ;");

    if (SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL))
    {
        if (azResult[ncolumn * 1 + 0] != NULL)  //2行1列
        {
            id = atoi(azResult[ncolumn * 1 + 0]);
            sqlite3_free_table(azResult);

            sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET deal_SN = '%.20s' WHERE ID = '%d';", Busy_SN, id);

            if (sqlite3_exec(db, sql, 0, 0, &zErrMsg) != SQLITE_OK)
            {
                sqlite3_free(zErrMsg);
                sqlite3_close(db);
                return -1;
            }

            return 0;
        }
    }

    sqlite3_free_table(azResult);
    sqlite3_close(db);
    return -1;
}






//查询有没有该卡号锁卡记录最大记录 ID 值，有则读出相应的数据
int Select_Unlock_Info(void)
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
	char db_path[128];
	sprintf(db_path,"%s/busy_coplete%d.db",F_DATAPATH,0);
	rc = sqlite3_open_v2(db_path, &db,
						 SQLITE_OPEN_NOMUTEX|
						 SQLITE_OPEN_SHAREDCACHE|
						 SQLITE_OPEN_READWRITE,
						 NULL);
	if(rc){
		sqlite3_close(db);
		Globa->User_Card_Lock_Info.Local_Data_Baseid_ID = 0;
		Globa->User_Card_Lock_Info.Deduct_money = 0;
		memset(&Globa->User_Card_Lock_Info.busy_SN[0], '0', sizeof(Globa->User_Card_Lock_Info.busy_SN));
		

		return -1;
	}
  
	sprintf(&temp_char[0], "%02d%02d%02d%02d%02d%02d%02d", Globa->User_Card_Lock_Info.card_lock_time.year_H, Globa->User_Card_Lock_Info.card_lock_time.year_L, Globa->User_Card_Lock_Info.card_lock_time.month, Globa->User_Card_Lock_Info.card_lock_time.day_of_month , Globa->User_Card_Lock_Info.card_lock_time.hour, Globa->User_Card_Lock_Info.card_lock_time.minute,Globa->User_Card_Lock_Info.card_lock_time.second);
	//sprintf(sql, "SELECT MAX(ID) FROM Charger_Busy_Complete_Data WHERE deal_SN = '00000000000000000000' AND  flag = '01' AND  up_num < '%d' AND card_sn = '%.16s' AND start_time = '%.14s';", MAX_NUM_BUSY_UP_TIME,Globa->User_Card_Lock_Info.card_sn,temp_char);
  sprintf(sql, "SELECT MAX(ID) FROM Charger_Busy_Complete_Data WHERE  card_deal_result = 1 AND  up_num < '%d' AND card_sn = '%.16s' AND start_time = '%.14s';", 0xAA,Globa->User_Card_Lock_Info.card_sn,temp_char);
	if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL)){
		if(azResult[ncolumn*1+0] != NULL){//2行1列
			id = atoi(azResult[ncolumn*1+0]);
			//id = atoi(azResult[ncolumn*1+1]);

			sqlite3_free_table(azResult);
			sprintf(sql, "SELECT * FROM Charger_Busy_Complete_Data WHERE ID = '%d';", id);
			if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL)){
				if(azResult[ncolumn*1+0] != NULL){//2行1列
					id = atoi(azResult[ncolumn*1+0]);
					Globa->User_Card_Lock_Info.Local_Data_Baseid_ID = id;
					Globa->User_Card_Lock_Info.Deduct_money = atoi(azResult[ncolumn*1+41]);//all_chg_money
					memcpy(&Globa->User_Card_Lock_Info.busy_SN[0], azResult[ncolumn*1+3], sizeof(Globa->User_Card_Lock_Info.busy_SN));
					
					sqlite3_free_table(azResult);
					sqlite3_close(db);
					return (0);
				}else{
					sqlite3_free_table(azResult);
				}
			}else{
				sqlite3_free_table(azResult);
			}
			Globa->User_Card_Lock_Info.Local_Data_Baseid_ID = 0;
			Globa->User_Card_Lock_Info.Deduct_money = 0;
			memset(&Globa->User_Card_Lock_Info.busy_SN[0], '0', sizeof(Globa->User_Card_Lock_Info.busy_SN));
			
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
	Globa->User_Card_Lock_Info.Local_Data_Baseid_ID = 0;
	Globa->User_Card_Lock_Info.Deduct_money = 0;
	memset(&Globa->User_Card_Lock_Info.busy_SN[0], '0', sizeof(Globa->User_Card_Lock_Info.busy_SN));
	
	return -1;
}


//清除锁卡记录，设置卡片补扣或锁卡记录已上送标记
//card_deal_result--值0解锁成功先于记录上送
//值1锁卡
//值2锁卡记录已上送
//值3锁卡记录上送前本地解锁
int Set_Unlock_flag(int id,int card_deal_result)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	char **azResult = NULL;
	char sql[512];
	char db_path[128];
	sprintf(db_path,"%s/busy_coplete%d.db",F_DATAPATH,0);
	rc = sqlite3_open_v2(db_path, &db,
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
	sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET card_deal_result = %d WHERE ID = '%d';",card_deal_result, id);
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


//查询ID值最大的记录
int Select_MAX_ID_Busy(unsigned char db_sel,ISO8583_charge_db_data *frame)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;

	int id = 0;

	char db_path[128];
	char sql[512];
	sprintf(db_path,"%s/busy_coplete%d.db",F_DATAPATH,db_sel);
	rc = sqlite3_open_v2(db_path, &db,
						 SQLITE_OPEN_NOMUTEX|
						 SQLITE_OPEN_SHAREDCACHE|
						 SQLITE_OPEN_READWRITE,
						 NULL);
	if(rc){
		printf("[%s]Can't open %s database: %s\n",__FUNCTION__,db_path,sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	sprintf(sql, "SELECT MAX(ID) FROM Charger_Busy_Complete_Data ;");
	
	if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL)){
		if(azResult[ncolumn*1+0] != NULL){//2行1列
			id = atoi(azResult[ncolumn*1+0]);
			sqlite3_free_table(azResult);
			sprintf(sql, "SELECT * FROM Charger_Busy_Complete_Data WHERE ID = '%d';", id);
			if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL))
			{
				if(azResult[ncolumn*1+0] != NULL)
				{//2行1列
					id = atoi(azResult[ncolumn*1+0]);
					memcpy(&frame->charger_SN[0], azResult[ncolumn*1+1], sizeof(frame->charger_SN));
					frame->chg_port    =     atoi(azResult[ncolumn*1+2]);					
					memcpy(&frame->deal_SN[0],    azResult[ncolumn*1+3], sizeof(frame->deal_SN));					
					memcpy(&frame->card_sn[0],    azResult[ncolumn*1+4], sizeof(frame->card_sn));					
					frame->chg_start_by  =   atoi(azResult[ncolumn*1+5]);	
					frame->card_deal_result = atoi(azResult[ncolumn*1+6]);
					memcpy(&frame->start_time[0], azResult[ncolumn*1+7], sizeof(frame->start_time));
					memcpy(&frame->end_time[0],   azResult[ncolumn*1+8], sizeof(frame->end_time));					
					frame->start_kwh    = atoi(azResult[ncolumn*1+9]);
					frame->end_kwh      = atoi(azResult[ncolumn*1+10]);
					frame->kwh1_price   = atoi(azResult[ncolumn*1+11]);
					frame->kwh1_charged = atoi(azResult[ncolumn*1+12]);
					frame->kwh1_money   = atoi(azResult[ncolumn*1+13]);
					frame->kwh2_price   = atoi(azResult[ncolumn*1+14]);
					frame->kwh2_charged = atoi(azResult[ncolumn*1+15]);
					frame->kwh2_money   = atoi(azResult[ncolumn*1+16]);
					frame->kwh3_price   = atoi(azResult[ncolumn*1+17]);
					frame->kwh3_charged = atoi(azResult[ncolumn*1+18]);
					frame->kwh3_money   = atoi(azResult[ncolumn*1+19]);
					frame->kwh4_price   = atoi(azResult[ncolumn*1+20]);
					frame->kwh4_charged = atoi(azResult[ncolumn*1+21]);
					frame->kwh4_money   = atoi(azResult[ncolumn*1+22]);
					frame->kwh_total_charged  = atoi(azResult[ncolumn*1+23]);
					frame->money_left = atoi(azResult[ncolumn*1+24]);
					frame->last_rmb   = atoi(azResult[ncolumn*1+25]);
					frame->rmb_kwh    = atoi(azResult[ncolumn*1+26]);
					frame->total_kwh_money = atoi(azResult[ncolumn*1+27]);
					memcpy(&frame->car_VIN[0], azResult[ncolumn*1+28], sizeof(frame->car_VIN));
					
					frame->kwh1_service_price   = atoi(azResult[ncolumn*1+29]);
					frame->kwh1_service_money   = atoi(azResult[ncolumn*1+30]);	
					frame->kwh2_service_price   = atoi(azResult[ncolumn*1+31]);
					frame->kwh2_service_money   = atoi(azResult[ncolumn*1+32]);	
					frame->kwh3_service_price   = atoi(azResult[ncolumn*1+33]);
					frame->kwh3_service_money   = atoi(azResult[ncolumn*1+34]);	
					frame->kwh4_service_price   = atoi(azResult[ncolumn*1+35]);
					frame->kwh4_service_money   = atoi(azResult[ncolumn*1+36]);	
					
									
					frame->book_price    = atoi(azResult[ncolumn*1+37]);
					frame->book_money    = atoi(azResult[ncolumn*1+38]);					
					frame->park_price    = atoi(azResult[ncolumn*1+39]);
					frame->park_money    = atoi(azResult[ncolumn*1+40]);					
					frame->all_chg_money = atoi(azResult[ncolumn*1+41]);
									
					frame->end_reason   = atoi(azResult[ncolumn*1+42]);					
				
					frame->Car_AllMileage =  atoi(azResult[ncolumn*1+43]);
					frame->Start_Soc =  atoi(azResult[ncolumn*1+44]);		
					frame->End_Soc =  atoi(azResult[ncolumn*1+45]);		
					
	      //*up_num = atoi(azResult[ncolumn*1+44]);
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


//查询还没有上传成功的记录 ID最大值
int Select_Busy_Need_Up(unsigned char db_sel,ISO8583_charge_db_data *frame, unsigned int *up_num)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;

	int id = 0;

	char db_path[128];
	char sql[512];
	sprintf(db_path,"%s/busy_coplete%d.db",F_DATAPATH,db_sel);
	rc = sqlite3_open_v2(db_path, &db,
						 SQLITE_OPEN_NOMUTEX|
						 SQLITE_OPEN_SHAREDCACHE|
						 SQLITE_OPEN_READWRITE,
						 NULL);
	if(rc){
		//printf("[%s]Can't open %s database: %s\n",__FUNCTION__,db_path,sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	sprintf(sql, "SELECT MAX(ID) FROM Charger_Busy_Complete_Data WHERE deal_SN != '00000000000000000000' AND  up_num < '%d' ;", MAX_NUM_BUSY_UP_TIME);
	
	if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL)){
		if(azResult[ncolumn*1+0] != NULL){//2行1列
			id = atoi(azResult[ncolumn*1+0]);
			sqlite3_free_table(azResult);
			sprintf(sql, "SELECT * FROM Charger_Busy_Complete_Data WHERE ID = '%d';", id);
			if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL))
			{
				if(azResult[ncolumn*1+0] != NULL)
				{//2行1列
					id = atoi(azResult[ncolumn*1+0]);
					memcpy(&frame->charger_SN[0], azResult[ncolumn*1+1], sizeof(frame->charger_SN));
					frame->chg_port    =     atoi(azResult[ncolumn*1+2]);					
					memcpy(&frame->deal_SN[0],    azResult[ncolumn*1+3], sizeof(frame->deal_SN));					
					memcpy(&frame->card_sn[0],    azResult[ncolumn*1+4], sizeof(frame->card_sn));					
					frame->chg_start_by  =   atoi(azResult[ncolumn*1+5]);	
					frame->card_deal_result = atoi(azResult[ncolumn*1+6]);
					memcpy(&frame->start_time[0], azResult[ncolumn*1+7], sizeof(frame->start_time));
					memcpy(&frame->end_time[0],   azResult[ncolumn*1+8], sizeof(frame->end_time));					
					frame->start_kwh    = atoi(azResult[ncolumn*1+9]);
					frame->end_kwh      = atoi(azResult[ncolumn*1+10]);
					frame->kwh1_price   = atoi(azResult[ncolumn*1+11]);
					frame->kwh1_charged = atoi(azResult[ncolumn*1+12]);
					frame->kwh1_money   = atoi(azResult[ncolumn*1+13]);
					frame->kwh2_price   = atoi(azResult[ncolumn*1+14]);
					frame->kwh2_charged = atoi(azResult[ncolumn*1+15]);
					frame->kwh2_money   = atoi(azResult[ncolumn*1+16]);
					frame->kwh3_price   = atoi(azResult[ncolumn*1+17]);
					frame->kwh3_charged = atoi(azResult[ncolumn*1+18]);
					frame->kwh3_money   = atoi(azResult[ncolumn*1+19]);
					frame->kwh4_price   = atoi(azResult[ncolumn*1+20]);
					frame->kwh4_charged = atoi(azResult[ncolumn*1+21]);
					frame->kwh4_money   = atoi(azResult[ncolumn*1+22]);
					frame->kwh_total_charged  = atoi(azResult[ncolumn*1+23]);
					frame->money_left = atoi(azResult[ncolumn*1+24]);
					frame->last_rmb   = atoi(azResult[ncolumn*1+25]);
					frame->rmb_kwh    = atoi(azResult[ncolumn*1+26]);
					frame->total_kwh_money = atoi(azResult[ncolumn*1+27]);
					memcpy(&frame->car_VIN[0], azResult[ncolumn*1+28], sizeof(frame->car_VIN));
					
					frame->kwh1_service_price   = atoi(azResult[ncolumn*1+29]);
					frame->kwh1_service_money   = atoi(azResult[ncolumn*1+30]);	
					frame->kwh2_service_price   = atoi(azResult[ncolumn*1+31]);
					frame->kwh2_service_money   = atoi(azResult[ncolumn*1+32]);	
					frame->kwh3_service_price   = atoi(azResult[ncolumn*1+33]);
					frame->kwh3_service_money   = atoi(azResult[ncolumn*1+34]);	
					frame->kwh4_service_price   = atoi(azResult[ncolumn*1+35]);
					frame->kwh4_service_money   = atoi(azResult[ncolumn*1+36]);	
					
									
					frame->book_price    = atoi(azResult[ncolumn*1+37]);
					frame->book_money    = atoi(azResult[ncolumn*1+38]);					
					frame->park_price    = atoi(azResult[ncolumn*1+39]);
					frame->park_money    = atoi(azResult[ncolumn*1+40]);					
					frame->all_chg_money = atoi(azResult[ncolumn*1+41]);
									
					frame->end_reason   = atoi(azResult[ncolumn*1+42]);	
					frame->Car_AllMileage =  atoi(azResult[ncolumn*1+43]);		
	        frame->Start_Soc =  atoi(azResult[ncolumn*1+44]);		
					frame->End_Soc =  atoi(azResult[ncolumn*1+45]);	
					*up_num = atoi(azResult[ncolumn*1+46]);

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
//修改数据库指定id的记录的上传标志up_num值
//up_num值设置为0xAA表示记录已上送
int Set_Busy_UP_num(unsigned char db_sel,int id, unsigned int up_num)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;

	char db_path[128];
	char sql[512];
	sprintf(db_path,"%s/busy_coplete%d.db",F_DATAPATH,db_sel);
	rc = sqlite3_open_v2(db_path, &db,
						 SQLITE_OPEN_NOMUTEX|
						 SQLITE_OPEN_SHAREDCACHE|
						 SQLITE_OPEN_READWRITE,
						 NULL);
	if(rc){
		printf("[%s]Can't open %s database: %s\n",__FUNCTION__,db_path,sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	if(TRANSACTION_FLAG) sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);

	sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET up_num = '%d'  WHERE ID = '%d';", up_num, id);
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

//VIN鉴权启动后写入对应的充电内部账号号
int Update_VIN_CardSN(unsigned char db_sel,int id, ISO8583_charge_db_data *frame)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;

	char db_path[128];
	char sql[512];
	sprintf(db_path,"%s/busy_coplete%d.db",F_DATAPATH,db_sel);
	rc = sqlite3_open_v2(db_path, &db,
						 SQLITE_OPEN_NOMUTEX|
						 SQLITE_OPEN_SHAREDCACHE|
						 SQLITE_OPEN_READWRITE,
						 NULL);
	if(rc){
		printf("[%s]Can't open %s database: %s\n",__FUNCTION__,db_path,sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	if(TRANSACTION_FLAG) sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);

		sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET kwh1_price = %d,",frame->kwh1_price);
		sprintf(&sql[strlen(sql)],"kwh2_price = %d,", frame->kwh2_price);
		sprintf(&sql[strlen(sql)],"kwh3_price = %d,", frame->kwh3_price);
		sprintf(&sql[strlen(sql)],"kwh4_price = %d,", frame->kwh4_price);
		sprintf(&sql[strlen(sql)],"kwh1_service_price = %d,", frame->kwh1_service_price);
		sprintf(&sql[strlen(sql)],"kwh2_service_price = %d,", frame->kwh2_service_price);
		sprintf(&sql[strlen(sql)],"kwh3_service_price = %d,", frame->kwh3_service_price);
		sprintf(&sql[strlen(sql)],"kwh4_service_price = %d,", frame->kwh4_service_price);
		sprintf(&sql[strlen(sql)],"card_sn = '%.16s' WHERE ID = '%d';", frame->card_sn,id);
		
	  //sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET card_sn = '%.16s'  WHERE ID = '%d';", card_sn, id);
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


//查询id对应的交易记录是否未上送(up_num传值0xAA时)
//查询id对应的交易记录,up_num等于0x55或0x66并且记录中的up_num不等于参数up_num的记录
//返回值0表示有匹配的记录
int Select_NO_Over_Record(unsigned char db_sel,int id,int up_num)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;
	char db_path[128];
	char sql[512];
	sprintf(db_path,"%s/busy_coplete%d.db",F_DATAPATH,db_sel);
	rc = sqlite3_open_v2(db_path, &db,
						 SQLITE_OPEN_NOMUTEX|
						 SQLITE_OPEN_SHAREDCACHE|
						 SQLITE_OPEN_READWRITE,
						 NULL);
	if(rc){
		printf("[%s]Can't open %s database: %s\n",__FUNCTION__,db_path,sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	//sprintf(sql, "SELECT MAX(ID) FROM Charger_Busy_Complete_Data WHERE id = '%d' AND (up_num = 85 OR up_num = 102) AND up_num != '%d';",id,up_num);
	sprintf(sql, "SELECT MAX(ID) FROM Charger_Busy_Complete_Data WHERE id = '%d' AND up_num != '%d';",id,up_num);

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

//查看是否有与frame相匹配的记录在数据库中
//返回值0表示有匹配的记录
int Select_Busy_BCompare_ID(unsigned char db_sel,ISO8583_charge_db_data *frame)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	char db_path[128];
	char sql[512];
	sprintf(db_path,"%s/busy_coplete%d.db",F_DATAPATH,db_sel);
	rc = sqlite3_open_v2(db_path, &db,
						 SQLITE_OPEN_NOMUTEX|
						 SQLITE_OPEN_SHAREDCACHE|
						 SQLITE_OPEN_READWRITE,
						 NULL);
	if(rc){
		printf("[%s]Can't open %s database: %s\n",__FUNCTION__,db_path,sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	sprintf(sql, "SELECT MAX(ID) FROM Charger_Busy_Complete_Data WHERE start_time = '%.14s' AND end_time = '%.14s'  AND chg_port = %d;",frame->start_time,frame->end_time,frame->chg_port);
	if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL)){
		if(azResult[ncolumn*1+0] != NULL)
		{//2行1列
			
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
//将up_num值从0xB4修改为0
int Set_0XB4_Busy_ID(unsigned char db_sel)
{ 
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;

	char db_path[128];
	char sql[512];
	sprintf(db_path,"%s/busy_coplete%d.db",F_DATAPATH,db_sel);
	rc = sqlite3_open_v2(db_path, &db,
						 SQLITE_OPEN_NOMUTEX|
						 SQLITE_OPEN_SHAREDCACHE|
						 SQLITE_OPEN_READWRITE,
						 NULL);
	if(rc){
		printf("[%s]Can't open %s database: %s\n",__FUNCTION__,db_path,sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}
	sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);
	sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET up_num = 0 WHERE up_num = 180;");
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


//查询意外掉电的不完整数据记录
int Select_0X55_0X66_Busy_ID(unsigned char db_sel,ISO8583_charge_db_data *frame, unsigned int *up_num)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;
	int id;

	char db_path[128];
	char sql[512];
	sprintf(db_path,"%s/busy_coplete%d.db",F_DATAPATH,db_sel);
	rc = sqlite3_open_v2(db_path, &db,
						 SQLITE_OPEN_NOMUTEX|
						 SQLITE_OPEN_SHAREDCACHE|
						 SQLITE_OPEN_READWRITE,
						 NULL);
	if(rc){
		printf("[%s]Can't open %s database: %s\n",__FUNCTION__,db_path,sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	sprintf(sql, "SELECT MAX(ID) FROM Charger_Busy_Complete_Data WHERE up_num = 85 OR up_num = 102 ;");
	if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL)){
		if(azResult[ncolumn*1+0] != NULL){//2行1列
			id = atoi(azResult[ncolumn*1+0]);
			sqlite3_free_table(azResult);
			
			sprintf(sql, "SELECT * FROM Charger_Busy_Complete_Data WHERE ID = '%d';", id);
			if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL))
			{
				if(azResult[ncolumn*1+0] != NULL)
				{//2行1列
					id = atoi(azResult[ncolumn*1+0]);
					memcpy(&frame->charger_SN[0], azResult[ncolumn*1+1], sizeof(frame->charger_SN));
					frame->chg_port    =     atoi(azResult[ncolumn*1+2]);					
					memcpy(&frame->deal_SN[0],    azResult[ncolumn*1+3], sizeof(frame->deal_SN));					
					memcpy(&frame->card_sn[0],    azResult[ncolumn*1+4], sizeof(frame->card_sn));					
					frame->chg_start_by  =   atoi(azResult[ncolumn*1+5]);	
					frame->card_deal_result = atoi(azResult[ncolumn*1+6]);
					memcpy(&frame->start_time[0], azResult[ncolumn*1+7], sizeof(frame->start_time));
					memcpy(&frame->end_time[0],   azResult[ncolumn*1+8], sizeof(frame->end_time));					
					frame->start_kwh    = atoi(azResult[ncolumn*1+9]);
					frame->end_kwh      = atoi(azResult[ncolumn*1+10]);
					frame->kwh1_price   = atoi(azResult[ncolumn*1+11]);
					frame->kwh1_charged = atoi(azResult[ncolumn*1+12]);
					frame->kwh1_money   = atoi(azResult[ncolumn*1+13]);
					frame->kwh2_price   = atoi(azResult[ncolumn*1+14]);
					frame->kwh2_charged = atoi(azResult[ncolumn*1+15]);
					frame->kwh2_money   = atoi(azResult[ncolumn*1+16]);
					frame->kwh3_price   = atoi(azResult[ncolumn*1+17]);
					frame->kwh3_charged = atoi(azResult[ncolumn*1+18]);
					frame->kwh3_money   = atoi(azResult[ncolumn*1+19]);
					frame->kwh4_price   = atoi(azResult[ncolumn*1+20]);
					frame->kwh4_charged = atoi(azResult[ncolumn*1+21]);
					frame->kwh4_money   = atoi(azResult[ncolumn*1+22]);
					frame->kwh_total_charged  = atoi(azResult[ncolumn*1+23]);
					frame->money_left = atoi(azResult[ncolumn*1+24]);
					frame->last_rmb   = atoi(azResult[ncolumn*1+25]);
					frame->rmb_kwh    = atoi(azResult[ncolumn*1+26]);
					frame->total_kwh_money = atoi(azResult[ncolumn*1+27]);
					memcpy(&frame->car_VIN[0], azResult[ncolumn*1+28], sizeof(frame->car_VIN));
					
					frame->kwh1_service_price   = atoi(azResult[ncolumn*1+29]);
					frame->kwh1_service_money   = atoi(azResult[ncolumn*1+30]);	
					frame->kwh2_service_price   = atoi(azResult[ncolumn*1+31]);
					frame->kwh2_service_money   = atoi(azResult[ncolumn*1+32]);	
					frame->kwh3_service_price   = atoi(azResult[ncolumn*1+33]);
					frame->kwh3_service_money   = atoi(azResult[ncolumn*1+34]);	
					frame->kwh4_service_price   = atoi(azResult[ncolumn*1+35]);
					frame->kwh4_service_money   = atoi(azResult[ncolumn*1+36]);	
					
									
					frame->book_price    = atoi(azResult[ncolumn*1+37]);
					frame->book_money    = atoi(azResult[ncolumn*1+38]);					
					frame->park_price    = atoi(azResult[ncolumn*1+39]);
					frame->park_money    = atoi(azResult[ncolumn*1+40]);					
					frame->all_chg_money = atoi(azResult[ncolumn*1+41]);
									
					frame->end_reason   = atoi(azResult[ncolumn*1+42]);	
					frame->Car_AllMileage =  atoi(azResult[ncolumn*1+43]);		
					frame->Start_Soc =  atoi(azResult[ncolumn*1+44]);		
					frame->End_Soc =  atoi(azResult[ncolumn*1+45]);	
					*up_num = atoi(azResult[ncolumn*1+46]);
					

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


//更新意外掉电的不完整数据记录
int Restore_0X55_Busy_ID(unsigned char db_sel,ISO8583_charge_db_data *frame, int id)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;

	char db_path[128];
	char sql[512];
	sprintf(db_path,"%s/busy_coplete%d.db",F_DATAPATH,db_sel);
	rc = sqlite3_open_v2(db_path, &db,
						 SQLITE_OPEN_NOMUTEX|
						 SQLITE_OPEN_SHAREDCACHE|
						 SQLITE_OPEN_READWRITE,
						 NULL);
	if(rc){
		printf("[%s]Can't open %s database: %s\n",__FUNCTION__,db_path,sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);

	// sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET total_kwh_money = %d, kwh_total_charged = %d, all_chg_money = %d, end_kwh = %d,money_left = %d,end_reason = %d,up_num = '%d' WHERE ID = '%d';",
		// frame->total_kwh_money, 
		// frame->kwh_total_charged, 
		// frame->all_chg_money,	
		// frame->end_kwh, 
		// frame->money_left,
		// frame->end_reason, 		
		// 0x00, id);
	sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET total_kwh_money = %d, ",frame->total_kwh_money);
	sprintf(&sql[strlen(sql)],"kwh_total_charged = %d,", frame->kwh_total_charged);
	sprintf(&sql[strlen(sql)],"rmb_kwh = %d,", frame->rmb_kwh);
	sprintf(&sql[strlen(sql)],"all_chg_money = %d,", frame->all_chg_money);		
	sprintf(&sql[strlen(sql)],"book_money = %d,", frame->book_money);
	sprintf(&sql[strlen(sql)],"park_money = %d,", frame->park_money);
	sprintf(&sql[strlen(sql)],"kwh1_charged = %d,", frame->kwh1_charged);
	sprintf(&sql[strlen(sql)],"kwh1_money = %d,", frame->kwh1_money);
	sprintf(&sql[strlen(sql)],"kwh2_charged = %d,", frame->kwh2_charged);
	sprintf(&sql[strlen(sql)],"kwh2_money = %d,", frame->kwh2_money);
	sprintf(&sql[strlen(sql)],"kwh3_charged = %d,", frame->kwh3_charged);
	sprintf(&sql[strlen(sql)],"kwh3_money = %d,", frame->kwh3_money);
	sprintf(&sql[strlen(sql)],"kwh4_charged = %d,", frame->kwh4_charged);
	sprintf(&sql[strlen(sql)],"kwh4_money = %d,", frame->kwh4_money);
	
	sprintf(&sql[strlen(sql)],"kwh1_service_money = %d,", frame->kwh1_service_money);
	sprintf(&sql[strlen(sql)],"kwh2_service_money = %d,", frame->kwh2_service_money);
	sprintf(&sql[strlen(sql)],"kwh3_service_money = %d,", frame->kwh3_service_money);
	sprintf(&sql[strlen(sql)],"kwh4_service_money = %d,", frame->kwh4_service_money);
	
	sprintf(&sql[strlen(sql)],"end_kwh = %d,", frame->end_kwh);
	sprintf(&sql[strlen(sql)],"end_reason = %d,", frame->end_reason);
	sprintf(&sql[strlen(sql)],"last_rmb = %d,", frame->last_rmb);//
	sprintf(&sql[strlen(sql)],"money_left = %d,", frame->money_left);//		
	sprintf(&sql[strlen(sql)],"up_num = '%d' WHERE ID = '%d';", 0,id);
		
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

//将id指定的记录的up_num值更新为0===========
int Set_0X66_Busy_ID(unsigned char db_sel,int id)//
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;

	char db_path[128];
	char sql[512];
	sprintf(db_path,"%s/busy_coplete%d.db",F_DATAPATH,db_sel);
	rc = sqlite3_open_v2(db_path, &db,
						 SQLITE_OPEN_NOMUTEX|
						 SQLITE_OPEN_SHAREDCACHE|
						 SQLITE_OPEN_READWRITE,
						 NULL);
	if(rc){
		printf("[%s]Can't open %s database: %s\n",__FUNCTION__,db_path,sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);

	sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET up_num = 0 WHERE ID = '%d';",  id);
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

//将记录中up_num的值设置为0=======
int Set_0X550X66_Busy_ID(unsigned char db_sel)
{ 
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int i,j;

	char db_path[128];
	char sql[512];
	sprintf(db_path,"%s/busy_coplete%d.db",F_DATAPATH,db_sel);
	rc = sqlite3_open_v2(db_path, &db,
						 SQLITE_OPEN_NOMUTEX|
						 SQLITE_OPEN_SHAREDCACHE|
						 SQLITE_OPEN_READWRITE,
						 NULL);
	if(rc){
		printf("[%s]Can't open %s database: %s\n",__FUNCTION__,db_path,sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}
	sqlite3_exec(db, "begin transaction", NULL, NULL, NULL);
	sprintf(sql, "UPDATE Charger_Busy_Complete_Data SET up_num = 0 WHERE up_num = 85 OR up_num = 102 OR up_num = 180;");
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

//删除超过2000条的最早记录（一次只删除一条记录）
int delete_over_2000_busy(void)
{
	sqlite3 *db=NULL;
	char *zErrMsg = NULL;
	int rc;
	int nrow = 0, ncolumn = 0;
	int cnt=0;
	char **azResult = NULL;
	int id = 0, count = 0;
	
	char db_path[128];
	char sql[512];
	sprintf(db_path,"%s/busy_coplete%d.db",F_DATAPATH,0);//总表
	rc = sqlite3_open_v2(db_path, &db,
						 SQLITE_OPEN_NOMUTEX|
						 SQLITE_OPEN_SHAREDCACHE|
						 SQLITE_OPEN_READWRITE,
						 NULL);
	if(rc){
		printf("[%s]Can't open %s database: %s\n",__FUNCTION__,db_path,sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	sprintf(sql, "SELECT COUNT(*) FROM Charger_Busy_Complete_Data;");
	if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL))
	{
		if(azResult[ncolumn*1+0] != NULL)
		{//2行1列
			count = atoi(azResult[ncolumn*1+0]);
			sqlite3_free_table(azResult);

			if(count > MAX_CHARGER_BUSY_NUM_LIMINT)
			{//超过最大允许记录数目
				//sprintf(sql, "SELECT MIN(ID) FROM Charger_Busy_Complete_Data;");
				 sprintf(sql, "SELECT MAX(ID) FROM Charger_Busy_Complete_Data;");
				if(SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, NULL))
				{
					if(azResult[ncolumn*1+0] != NULL)
					{//2行1列
						id = atoi(azResult[ncolumn*1+0]);
						id = (id>MAX_CHARGER_BUSY_NUM_LIMINT)? id:MAX_CHARGER_BUSY_NUM_LIMINT;
						sqlite3_free_table(azResult);

						sprintf(sql, "DELETE FROM Charger_Busy_Complete_Data WHERE ID <= '%d';", (id - MAX_CHARGER_BUSY_NUM_LIMINT));
						if(sqlite3_exec(db , sql , 0, 0, &zErrMsg) != SQLITE_OK)
						{
							sqlite3_free(zErrMsg);
						}else
						{
							sqlite3_close(db);
							return (id);
						}
					}else
					{
						sqlite3_free_table(azResult);
					}
				}else
				{
					sqlite3_free_table(azResult);
				}
			}
		}else
		{
			sqlite3_free_table(azResult);
		}
	}else{
		sqlite3_free_table(azResult);
	}

	sqlite3_close(db);
	return -1;
}


//=====================================停车业务数据库相关=============================================

static int Set_CAR_CLEAR_Busy_ID(void)
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
int Select_Car_Stop_Busy_Need_Up(ISO8583_Car_Stop_busy_Struct *frame, unsigned int *up_num)
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

	sprintf(sql, "SELECT MAX(ID) FROM CarLock_Busy_Data WHERE deal_SN != '00000000000000000000' AND  up_num < '%d' ;", MAX_NUM_BUSY_UP_TIME);
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



//=====================================黑名单相关=============================================

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
		printf("[%s]Can't create %s database: %s\n", __FUNCTION__,F_CHARGER_CARD_DB,sqlite3_errmsg(db));
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
	while(i<count)
	{
		sprintf(sql, "INSERT INTO Card_DB VALUES (NULL, '%.16s');", &sn[i*16]);
		i++;
	  
		if(sqlite3_exec(db , sql , 0 , 0 , &zErrMsg) != SQLITE_OK)
		{
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
int Delete_card_DB(void)
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
		printf("[%s]Can't create %s database: %s\n", __FUNCTION__,F_CHARGER_CARD_DB,sqlite3_errmsg(db));
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

//查询卡片是否为黑名单中的卡号
//返回值不是-1表示卡片黑名单
int valid_card_check(unsigned char *sn)
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



//删除黑名单列表中指定sn的项
int delete_card_blist(unsigned char *sn)
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
		if(azResult[ncolumn*1+0] != NULL)
		{//2行1列
			id = atoi(azResult[ncolumn*1+0]);
			sqlite3_free_table(azResult);
			
			sprintf(sql, "DELETE FROM Card_DB WHERE ID == '%d';", id);
			if(sqlite3_exec(db , sql , 0, 0, &zErrMsg) != SQLITE_OK)
			{
				sqlite3_free(zErrMsg);
			}
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

