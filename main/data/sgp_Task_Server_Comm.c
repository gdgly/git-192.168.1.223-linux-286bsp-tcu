/*******************************************************************************
本文档用于替换原有的“Task_Server_Comm.c”
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
#include <sys/prctl.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <time.h>
#include <errno.h>
#include <sys/wait.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <dirent.h>
#include <execinfo.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <termio.h>
#include <sys/time.h>
#include <getopt.h>
#include <libgen.h>
#include <signal.h>
#include <limits.h>
#include <sys/uio.h>
#include <netdb.h>
#include "globalvar.h"
#include "Time_Utity.h"
#include "Deal_Handler.h"
#include "InDTU332W_Driver.h"
#include "sgp_Task_Server_Comm.h"
#include "../Remote/sgp_RemoteData.h"
#include "sgp_RemoteDataHandle.h"


//**********************定时电价更新条件
//返回值1表示可以采用定时电价
int IS_Regular_price_update(void)
{
	time_t systime=0,log_time =0,current_time,update_time;
	struct tm tm_t;
	//systime = time(NULL);         //获得系统的当前时间 
	//localtime_r(&systime, &tm_t);  //结构指针指向当前时间	

	if(((Globa->QT_Step == 2)||(Globa->QT_Step == 3))&&(Globa->charger_fixed_price.falg == 1)&&(Globa->charger_fixed_price.fixed_price_effective_time[0] != 0)\
	  &&(Globa->charger_fixed_price.fixed_price_effective_time[1] != 0)&&(Globa->charger_fixed_price.fixed_price_effective_time[2] != 0)&&(Globa->charger_fixed_price.fixed_price_effective_time[3] != 0))
	{
		tm_t.tm_year = (Globa->charger_fixed_price.fixed_price_effective_time[0]*100 + Globa->charger_fixed_price.fixed_price_effective_time[1]) -1900;
		tm_t.tm_mon = (Globa->charger_fixed_price.fixed_price_effective_time[2]) - 1;
		
		tm_t.tm_mday = (Globa->charger_fixed_price.fixed_price_effective_time[3]);
		
		tm_t.tm_hour = (Globa->charger_fixed_price.fixed_price_effective_time[4]);

		tm_t.tm_min = (Globa->charger_fixed_price.fixed_price_effective_time[5]);
		tm_t.tm_sec = (Globa->charger_fixed_price.fixed_price_effective_time[6]);

		update_time = mktime(&tm_t);
		systime = time(NULL);         //获得系统的当前时间 

		if(systime > update_time){
			printf("定时电价更新 update_time  =%d =%d\n ", update_time,systime);
			return 1;
		}
	}
	return 0;
}


//进行定时电价更新
void Regular_price_update(void)
{
	int i=0;
	for(i=0;i<7;i++)
		Globa->charger_fixed_price.fixed_price_effective_time[i] = 0;

	Globa->charger_fixed_price.falg = 0;            //存在定时电价，需要更新
	Globa->Charger_param.Serv_Type = Globa->charger_fixed_price.Serv_Type;  //服务费收费类型 1：按次收费；2：按时间收费(单位：分/10分钟；3：按电量
	Globa->Charger_param.Serv_Price = Globa->charger_fixed_price.Serv_Price;              //服务费,精确到分 默认安电量收
	Globa->Charger_param.Price = Globa->charger_fixed_price.Price;                   //单价,精确到分
	for(i=0;i<4;i++)
	{
		Globa->Charger_param2.share_time_kwh_price[i] =  Globa->charger_fixed_price.share_time_kwh_price[i];
		Globa->Charger_param2.share_time_kwh_price_serve[i] = Globa->charger_fixed_price.share_time_kwh_price_serve[i]; //分时服务电价
	}
	Globa->Charger_param2.time_zone_num = Globa->charger_fixed_price.time_zone_num;  //时段数
	for(i=0;i<12;i++)
	{
		Globa->Charger_param2.time_zone_tabel[i] = Globa->charger_fixed_price.time_zone_tabel[i];//时段表
		Globa->Charger_param2.time_zone_feilv[i] = Globa->charger_fixed_price.time_zone_feilv[i];//时段表位于的价格		
	}
	Globa->Charger_param2.show_price_type = Globa->charger_fixed_price.show_price_type;//电价显示方式 0--详细显示 1--总电价显示
	//Globa->Charger_param2.IS_fixed_price = Globa->charger_fixed_price.IS_fixed_price;	
	System_param_save();
	System_param2_save();
	System_param_Fixedpirce_save();
}

//检测是否需要切换费率
void SwitchChgPrice(void)
{
	if(IS_Regular_price_update())
	{//定时电价更新
		Regular_price_update();
		if(SGP_DEBUG)  printf("定时电价更新定时电价更新 = %d\n");
	}
}

void delete_oldest_deal(void)
{
	static unsigned long last_delete_timestamp = 0;
	if((Globa->user_sys_time - last_delete_timestamp) >  60)//每60秒检测一次数据库是否已写满
	{
		pthread_mutex_lock(&busy_db_pmutex);
		delete_over_2000_busy();
		pthread_mutex_unlock(&busy_db_pmutex);
		
		last_delete_timestamp = Globa->user_sys_time;
	}
}

/**********************************************************************/
//  功能：  后台任务处理线程
//  参数：  无
//  返回：  无
/**********************************************************************/
int sgp_Server_Comm_Task(void)
{
    //开机10秒后才开始连后台，等待硬件信息初始化
    sgp_Task_sleep(10000);
    RemoteServerDataInit();                             // 数据初始化

    while (1)
    {
		SwitchChgPrice();                               // 检测是否需要切换费率
		delete_oldest_deal();                           // 删除多余订单
		Fimware_Upgrade_Check();                        // 检查是否进入升级 fortest 需要修改

        if(Globa->sgp_IsLogin != LOGIN_SUCCESS)         // 还未登录
        {
            sgp_SendLoginInfo();                        // 发送登录信息
        }
        else
        {
			sgp_ChargeStartAckHandle();                 // 充电指令响应处理
			sgp_UserCardCheckHandle();                  // 刷卡充电鉴处理
			sgp_VinStartChargeHandle();                 // VIN码启动充电处理
			sgp_UserCardStartChargeHandle();            // 刷卡启动充电
			GunStatChangedHandle();                     // 枪状态改变
			GunErrChangedHandle();                      // 枪故障信息上报

			HeartBeatPeriodSendHandle()；               // 心跳报文周期发送处理
			YaoCePeriodSendHandle();                    // 遥测报文周期发送处理
			YaoXinPeriodSendHandle();                   // 遥信息报文周期发送处理
			YaoMaiPeriodSendHandle();                   // 遥脉息报文周期发送处理
			BmsStatusPeriodSendHandle()；               // BMS实时信息周期发送处理

			BillInfoUpload();                           // 账单上送处理
        }

        if(Globa->Software_Version_change)      //下载升级文件中，加速
            sgp_Task_sleep(5);                       
		else
			sgp_Task_sleep(100);        //100ms
    }
}


/*********************************************************
**  后台通信协议任务创建
***parameter  :none
***return	  :none
**********************************************************/
void Server_Handle_Thread(void)
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

    /* 创建线程 */
    if(0 != pthread_create(&td1, &attr, (void *)sgp_Server_Comm_Task, NULL)){
        perror("####pthread_create sgp_Server_Comm_Task ERROR ####\n\n");
    }

    ret = pthread_attr_destroy(&attr);
    if(ret != 0){
        return;
    }
}

