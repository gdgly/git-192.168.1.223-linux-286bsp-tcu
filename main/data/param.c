/*******************************************************************************
  Copyright(C)    2014-2015,      EAST Co.Ltd.
  File name :     ic.c
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
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <signal.h>
#include <netinet/ip.h>

#include "globalvar.h"
#include "file.h"
#include "../lock/Carlock_task.h"

pthread_mutex_t *globa_pmutex;
///////////////////////////////////////////////////////
//程序代码、函数功能说明、参数说明、及返回值
///////////////////////////////////////////////////////
/*********************************************************
**description：系统共享内存的建立，应该放在主程序的初始化中
***parameter  :none
***return		  :none
**********************************************************/
void	share_memory(void)
{
	int  shmid_sysCfg;
	char *ch_sysCfg = NULL;
	int	 ret;
	pthread_mutexattr_t mutexattr;//互斥对象属性 
	
	/*globavar1共享内存申请share memory*/ 
	if((shmid_sysCfg = shmget(IPC_PRIVATE,sizeof(GLOBAVAL ),IPC_CREAT|0666))<0){
		printf("###share memory ERROR 1###\n\n");
		exit(1);
	}
	if((( ch_sysCfg =(char*)shmat(shmid_sysCfg,NULL,0)))==(char *)-1)
	{
		printf("###share memory ERROR 2###\n\n");
		exit(2);
	}

	Globa_1 = (GLOBAVAL *)ch_sysCfg;
	bzero(Globa_1, sizeof(GLOBAVAL));

	/*globavar2共享内存申请share memory*/ 
	if((shmid_sysCfg = shmget(IPC_PRIVATE,sizeof(GLOBAVAL ),IPC_CREAT|0666))<0){
		printf("###share memory ERROR 1###\n\n");
		exit(1);
	}
	if((( ch_sysCfg =(char*)shmat(shmid_sysCfg,NULL,0)))==(char *)-1)
	{
		printf("###share memory ERROR 2###\n\n");
		exit(2);
	}

	Globa_2 = (GLOBAVAL *)ch_sysCfg;
	bzero(Globa_2, sizeof(GLOBAVAL));

	/*全局进程看门狗变量 共享内存*/ 
	if((shmid_sysCfg = shmget(IPC_PRIVATE,sizeof(UINT32),IPC_CREAT|0666))<0){
		printf("###share memory ERROR 1 smtp_param_pmutex###\n\n");
		exit(1);
	}
	if((( ch_sysCfg =(char*)shmat(shmid_sysCfg,NULL,0)))==(char *)-1){
		printf("###share memory ERROR 2 smtp_param_pmutex###\n\n");
		exit(2);
	}
	process_dog_flag = (UINT32 *)ch_sysCfg;

	/*全局变量共享内存锁，globa_pmutex*/ 
	if((shmid_sysCfg = shmget(IPC_PRIVATE,sizeof(pthread_mutex_t),IPC_CREAT|0666))<0){
		printf("###share memory ERROR 1 globa_pmutex###\n\n");
		exit(1);
	}
	if((( ch_sysCfg =(char*)shmat(shmid_sysCfg,NULL,0)))==(char *)-1){
		printf("###share memory ERROR 2 globa_pmutex###\n\n");
		exit(2);
	}
	globa_pmutex = (pthread_mutex_t *)ch_sysCfg;

	pthread_mutexattr_init(&mutexattr);//初始化互斥对象属性PTHREAD_PROCESS_SHARED
	ret = pthread_mutexattr_setpshared(&mutexattr,PTHREAD_PROCESS_SHARED);//设置互斥对象为PTHREAD_PROCESS_SHARED共享，即可以在多个进程的线程访问,PTHREAD_PROCESS_PRIVATE为同一进程的线程共享  
	pthread_mutex_init(globa_pmutex, &mutexattr);//初始化互斥对象

}



/*********************************************************
**description：系统参数初始化函数
***parameter  :none
***return		  :OK，表示配置成功，ERROR16，表示初始化失败
**********************************************************/
void	System_Param_reset(void)
{
	Globa_1->Charger_param.charge_modluetype = 0;

	Globa_1->Charger_param.Charge_rate_voltage = 750*1000;		  //充电机额定电压  0.001V
	Globa_1->Charger_param.Charge_rate_number1 = 2;		          //模块数量1
	Globa_1->Charger_param.Charge_rate_number2 = 2;		          //模块数量2
	Globa_1->Charger_param.model_rate_current = 20*1000;	      //单模块额定电流  0.001A
	Globa_1->Charger_param.Charge_rate_current1 = Globa_1->Charger_param.Charge_rate_number1*Globa_1->Charger_param.model_rate_current;		  //充电机1额定电流  0.001A
	Globa_1->Charger_param.Charge_rate_current2 = Globa_1->Charger_param.Charge_rate_number2*Globa_1->Charger_param.model_rate_current;		  //充电机2额定电流  0.001A
	Globa_1->Charger_param.set_voltage = Globa_1->Charger_param.Charge_rate_voltage*4/5;
	Globa_1->Charger_param.set_current = Globa_1->Charger_param.Charge_rate_current1*1/2;
  Globa_1->Charger_param.System_Type = 0; //系统类型---0--双枪同时充 ---1双枪轮流充电 2--单枪立式 3-壁挂式

	Globa_1->Charger_param.gun_config  = 3; 
	Globa_1->Charger_param.Input_Relay_Ctl_Type = 0; 
	memcpy(&Globa_1->Charger_param.SN[0], "000000000001", sizeof(Globa_1->Charger_param.SN));//充电终端序列号    41	N	n12

	Globa_1->Charger_param.couple_flag = 1;		          //充电机单、并机标示 1：单机 2：并机
	Globa_1->Charger_param.charge_addr = 1;		          //充电机地址偏移，组要是在并机模式时用，1、N

	Globa_1->Charger_param.limit_power = 9999;         //出功率限制点(离线时用的默认功率限制值)  9999kw
	Globa_1->Charger_param.meter_config_flag = 1;      //电能表配置标志  1：有，0：无
  Globa_1->Charger_param.AC_Meter_config = 0;
	Globa_1->Charger_param.AC_Meter_CT = 80;
	Globa_1->Charger_param.input_over_limit = 445*1000;		    //输入过压点  0.001V 380*1.17 445
	Globa_1->Charger_param.input_lown_limit = 300*1000;		    //输入欠压点  0.001V 380*0.79 300
	Globa_1->Charger_param.output_over_limit = Globa_1->Charger_param.Charge_rate_voltage + 5*1000;	 //输出过压点  0.001V
	Globa_1->Charger_param.output_lown_limit =  199*1000;		  //输出欠压点  0.001V
	Globa_1->Charger_param.current_limit = 250;
	Globa_1->Charger_param.BMS_work = 1;		                  //系统工作模式  1：正常，2：测试
	Globa_1->Charger_param.BMS_CAN_ver = 1;		               //BMS的协议版本 1：国家标准 2：普天标准-深圳版本
	Globa_1->Charger_param.APP_enable = 1;
	
 
	Globa_1->Charger_param.Price = 100;                //单价,精确到分 1元
	Globa_1->Charger_param.Serv_Type = 3;              //服务费收费类型 1：按次收费；2：按时间收费(单位：分/10分钟；3：按电量
	Globa_1->Charger_param.Serv_Price = 50;            //服务费,精确到分 默认安电量收
	Globa_1->Charger_param.DC_Shunt_Range = 300;       //直流分流器量程设置MA
	memcpy(&Globa_1->Charger_param.Power_meter_addr1[0], "165254100522", sizeof(Globa_1->Charger_param.Power_meter_addr1));//电能表地址1 6字节
	memcpy(&Globa_1->Charger_param.Power_meter_addr2[0], "163001407696", sizeof(Globa_1->Charger_param.Power_meter_addr2));//电能表地址2 6字节
  Globa_1->Charger_param.Key[0] = 0xFF;
  Globa_1->Charger_param.Key[1] = 0xFF;
  Globa_1->Charger_param.Key[2] = 0xFF;
  Globa_1->Charger_param.Key[3] = 0xFF;
  Globa_1->Charger_param.Key[4] = 0xFF;
  Globa_1->Charger_param.Key[5] = 0xFF;
	Globa_1->Charger_param.Model_Type = 0;
	Globa_1->Charger_param.Charging_gun_type = 0;
	Globa_1->Charger_param.gun_allowed_temp = 95;
	Globa_1->Charger_param.Serial_Network  = 0;//后台协议走的是DTU还是网络 1--DTU 0--网络
  Globa_1->Charger_param.DTU_Baudrate = 2;
	Globa_1->Charger_param.NEW_VOICE_Flag = 1;
  Globa_1->Charger_param.Control_board_Baud_rate = 0;
	Globa_1->Charger_param.LED_Type_Config = 0;
	Globa_1->Charger_param.Light_Control_Data.Start_time_on_hour = 0;
	Globa_1->Charger_param.Light_Control_Data.Start_time_on_min  = 0;
	Globa_1->Charger_param.Light_Control_Data.End_time_off_hour  = 0;
	Globa_1->Charger_param.Light_Control_Data.End_time_off_min   = 0;
}


extern STATUS System_param_save(void)
{
	UINT16 ret;
	UINT32 fileLen;
	
	fileLen = sizeof(Globa_1->Charger_param);

	ret = writeFile(F_UPSPARAM, (UINT8 *)&(Globa_1->Charger_param), fileLen, 0, 0);	
	if (ERROR16 == ret){
		perror("Save Charger_param: write File error\n");
		return ERROR16;
        }
	else
		printf("Charger_param_save:write File success\n");

	ret = copyFile(F_UPSPARAM, F_BAK_UPSPARAM);
	if (ERROR16 == ret){
		printf("Charger_param_save:copy File error\n");
	}
	else
		printf("Charger_param_save:copy file success\n");

    return OK;
}


STATUS System_param_init(void)
{
	UINT16 i;
	UINT16 ret, ret1;
	UINT32 fileLen;

	memset(&(Globa_1->Charger_param), 0, sizeof(Globa_1->Charger_param));
	ret = readWholeFile(F_UPSPARAM, (UINT8 *)&(Globa_1->Charger_param), &fileLen); /* 第一次读取参数文件 */
	if(OK == ret){
		printf("read  Charger_param File sucess\n");
		return OK;
	}

    printf("read  Charger_param back File \n");
	ret = readWholeFile(F_BAK_UPSPARAM, (UINT8 *)&(Globa_1->Charger_param), &fileLen);   /* 若读取错误，读取备份文件 */
	if(OK == ret){
		ret1 = copyFile(F_BAK_UPSPARAM, F_UPSPARAM);   /* 拷贝副本到正本 *///add by dengjh 20110822
		if(ERROR16 == ret1){
			printf("read  Charger_param back File error\n");
		}else{
			printf("read  Charger_param back File ok\n");
		}
		return OK;
	}else{
		System_Param_reset();    /*备份文件同样出错，按照默认初始化内存 */
		printf("read Charger_param back File error, auto reset\n");

		if(ERROR16 == System_param_save()){
			printf("ERROR save parameters!\n");
		}

		return OK;
	}
}


/*********************************************************
**description：系统参数初始化函数
***parameter  :none
***return		  :OK，表示配置成功，ERROR16，表示初始化失败
**********************************************************/
void	System_Param2_reset(void)
{
	UINT32 i = 0;
	memset(&Globa_1->Charger_param2.invalid_card_ver[0], '0', sizeof(Globa_1->Charger_param2.invalid_card_ver));
	memset(&Globa_1->Charger_param2.price_type_ver[0], '0', sizeof(Globa_1->Charger_param2.price_type_ver));
	memset(&Globa_1->Charger_param2.APP_ver[0], '0', sizeof(Globa_1->Charger_param2.APP_ver));
	memset(&Globa_1->Charger_param2.Software_Version[0], '0', sizeof(Globa_1->Charger_param2.Software_Version));

	sprintf(&Globa_1->Charger_param2.http[0], "%s", "http://120.24.6.21/appinfos.do?sn=");
	for(i = 0;i<4;i++){
	  Globa_1->Charger_param2.share_time_kwh_price[i]= Globa_1->Charger_param.Price;  //单价,默认为非时电价
	  Globa_1->Charger_param2.share_time_kwh_price_serve[i]= Globa_1->Charger_param.Serv_Price;  //单价,默认为非时电价
	}
  Globa_1->Charger_param2.time_zone_num = 1;
  for(i = 0;i<12;i++){
	  Globa_1->Charger_param2.time_zone_tabel[i]= 0;  //单价,默认为非时电价
	  Globa_1->Charger_param2.time_zone_feilv[i]= 0;  //单价,默认为非时电价
	}
	Globa_1->Charger_param2.show_price_type = 0;//电价显示方式 0--详细显示 1--总电价显示
	Globa_1->Charger_param2.IS_fixed_price = 0;
}
extern STATUS System_param2_save(void)
{
	UINT16 ret;
	UINT32 fileLen;
	
	fileLen = sizeof(Globa_1->Charger_param2);

	ret = writeFile(F_UPSPARAM2, (UINT8 *)&(Globa_1->Charger_param2), fileLen, 0, 0);	
	if (ERROR16 == ret){
		perror("Save Charger_param2: write File error\n");
		return ERROR16;
        }
	else
		printf("Charger_param2_save:write File success\n");

	ret = copyFile(F_UPSPARAM2, F_BAK_UPSPARAM2);
	if (ERROR16 == ret){
		printf("Charger_param2_save:copy File error\n");
	}
	else
		printf("Charger_param2_save:copy file success\n");

    return OK;
}

STATUS System_param2_init(void)
{
	UINT16 i;
	UINT16 ret, ret1;
	UINT32 fileLen;

	memset(&(Globa_1->Charger_param2), 0, sizeof(Globa_1->Charger_param2));
	ret = readWholeFile(F_UPSPARAM2, (UINT8 *)&(Globa_1->Charger_param2), &fileLen); /* 第一次读取参数文件 */
	if(OK == ret){
		printf("read  Charger_param2 File sucess\n");
		return OK;
	}

    printf("read  Charger_param2 back File \n");
	ret = readWholeFile(F_BAK_UPSPARAM2, (UINT8 *)&(Globa_1->Charger_param2), &fileLen);   /* 若读取错误，读取备份文件 */
	if(OK == ret){
		ret1 = copyFile(F_BAK_UPSPARAM2, F_UPSPARAM2);   /* 拷贝副本到正本 *///add by dengjh 20110822
		if(ERROR16 == ret1){
			printf("read  Charger_param2 back File error\n");
		}else{
			printf("read  Charger_param2 back File ok\n");
		}
		return OK;
	}else{
		System_Param2_reset();    /*备份文件同样出错，按照默认初始化内存 */
		printf("read Charger_param2 back File error, auto reset\n");

		if(ERROR16 == System_param2_save()){
			printf("ERROR save parameters2!\n");
		}

		return OK;
	}
}

void	System_ParamFixedpirce_reset(void)
{
	UINT32 i = 0;

	for(i = 0;i<4;i++){
	  Globa_1->charger_fixed_price.share_time_kwh_price[i]= Globa_1->Charger_param2.share_time_kwh_price[i];  //单价,默认为非时电价
		Globa_1->charger_fixed_price.share_time_kwh_price_serve[i]= Globa_1->Charger_param2.share_time_kwh_price_serve[i];  //单价,默认为非时电价
	}
  Globa_1->charger_fixed_price.time_zone_num = 1;
  for(i = 0;i<12;i++){
	  Globa_1->charger_fixed_price.time_zone_tabel[i] = Globa_1->Charger_param2.time_zone_tabel[i];  //单价,默认为非时电价
	  Globa_1->charger_fixed_price.time_zone_feilv[i] = Globa_1->Charger_param2.time_zone_feilv[i];  //单价,默认为非时电价
	}
	for(i=0;i<7;i++){
		Globa_1->charger_fixed_price.fixed_price_effective_time[i] = 0;
	}
	Globa_1->show_price_type = Globa_1->Charger_param2.show_price_type;
}
extern STATUS System_param_Fixedpirce_save(void)
{
	UINT16 ret;
	UINT32 fileLen;
	
	fileLen = sizeof(Globa_1->charger_fixed_price);

	ret = writeFile(F_PARAMFixedpirce, (UINT8 *)&(Globa_1->charger_fixed_price), fileLen, 0, 0);	
	if (ERROR16 == ret){
		perror("Save Charger_paramFixedpirce: write File error\n");
		return ERROR16;
        }
	else
		printf("Charger_paramFixedpirce_save:write File success\n");

	ret = copyFile(F_PARAMFixedpirce, F_BAK_PARAMFixedpirce);
	if (ERROR16 == ret){
		printf("Charger_paramFixedpirce_save:copy File error\n");
	}
	else
		printf("Charger_paramFixedpirce_save:copy file success\n");

    return OK;
}

STATUS System_paramFixedpirce_init(void)
{
	UINT16 i;
	UINT16 ret, ret1;
	UINT32 fileLen;

	memset(&(Globa_1->charger_fixed_price), 0, sizeof(Globa_1->charger_fixed_price));
	ret = readWholeFile(F_PARAMFixedpirce, (UINT8 *)&(Globa_1->charger_fixed_price), &fileLen); /* 第一次读取参数文件 */
	if(OK == ret){
		printf("read  charger_fixed_price File sucess\n");
		return OK;
	}

    printf("read  Charger_paramFixedpirce back File \n");
	ret = readWholeFile(F_BAK_PARAMFixedpirce, (UINT8 *)&(Globa_1->charger_fixed_price), &fileLen);   /* 若读取错误，读取备份文件 */
	if(OK == ret){
		ret1 = copyFile(F_BAK_PARAMFixedpirce, F_PARAMFixedpirce);   /* 拷贝副本到正本 *///add by dengjh 20110822
		if(ERROR16 == ret1){
			printf("read  Charger_param Fixedpirce back File error\n");
		}else{
			printf("read  Charger_param Fixedpirce back File ok\n");
		}
		return OK;
	}else{
		System_Param2_reset();    /*备份文件同样出错，按照默认初始化内存 */
		printf("read Charger_param Fixedpirce back File error, auto reset\n");

		if(ERROR16 == System_param_Fixedpirce_save()){
			printf("ERROR save parameters Fixedpirce!\n");
		}
		return OK;
	}
}

void	System_Charger_Shileld_Alarm_reset(void)
{
	Globa_1->Charger_Shileld_Alarm.Shileld_PE_Fault = 0;	       //屏蔽接地故障告警 ----非停机故障，指做显示	    
	Globa_1->Charger_Shileld_Alarm.Shileld_AC_connect_lost = 0;	//屏蔽交流输通讯告警	---非停机故障，只提示告警信息    
	Globa_1->Charger_Shileld_Alarm.Shileld_AC_input_switch_off = 0;//屏蔽交流输入开关跳闸-----非停机故障交流接触器
	Globa_1->Charger_Shileld_Alarm.Shileld_AC_fanglei_off = 0;    //屏蔽交流防雷器跳闸---非停机故障，只提示告警信息 
	Globa_1->Charger_Shileld_Alarm.Shileld_circuit_breaker_trip = 0;//交流断路器跳闸.2---非停机故障，只提示告警信息 
	Globa_1->Charger_Shileld_Alarm.Shileld_circuit_breaker_off = 0;// 交流断路器断开.2---非停机故障，只提示告警信息 
	Globa_1->Charger_Shileld_Alarm.Shileld_AC_input_over_limit = 0;//屏蔽交流输入过压保护 ----  ---非停机故障，只提示告警信息 
	Globa_1->Charger_Shileld_Alarm.Shileld_AC_input_lown_limit = 0; //屏蔽交流输入欠压 ---非停机故障，只提示告警信息 
	
	Globa_1->Charger_Shileld_Alarm.Shileld_GUN_LOCK_FAILURE = 0;  //屏蔽枪锁检测未通过
	Globa_1->Charger_Shileld_Alarm.Shileld_AUXILIARY_POWER = 0;   //屏蔽辅助电源检测未通过
	Globa_1->Charger_Shileld_Alarm.Shileld_OUTSIDE_VOLTAGE = 0;    //屏蔽输出接触器外侧电压检测（标准10V，放开100V）
	Globa_1->Charger_Shileld_Alarm.Shileld_SELF_CHECK_V = 0;      //屏蔽自检输出电压异常
  Globa_1->Charger_Shileld_Alarm.Shileld_CHARG_OUT_OVER_V = 0;  //屏蔽输出过压停止充电   ----  data2
  Globa_1->Charger_Shileld_Alarm.Shileld_CHARG_OUT_Under_V = 0;  //屏蔽输出欠压停止充电   ----  data2
	Globa_1->Charger_Shileld_Alarm.Shileld_CHARG_OUT_OVER_C = 0;  //屏蔽输出过流
	Globa_1->Charger_Shileld_Alarm.Shileld_OUT_SHORT_CIRUIT = 0;  //屏蔽输出短路
	
	Globa_1->Charger_Shileld_Alarm.Shileld_GUN_OVER_TEMP = 0;     //屏蔽充电枪温度过高
	Globa_1->Charger_Shileld_Alarm.Shileld_INSULATION_CHECK = 0;   //屏蔽绝缘检测
	Globa_1->Charger_Shileld_Alarm.Shileld_OUT_SWITCH = 0;       //屏蔽输出开关检测
	Globa_1->Charger_Shileld_Alarm.Shileld_OUT_NO_CUR = 0;       //屏蔽输出无电流显示检测	
	Globa_1->Charger_Shileld_Alarm.Shileld_BMS_WARN_Monomer_vol_anomaly = 0;          //屏蔽BMS状态:单体电压异常
	Globa_1->Charger_Shileld_Alarm.Shileld_BMS_WARN_Soc_anomaly = 0;          //屏蔽BMS状态:SOC异常
	Globa_1->Charger_Shileld_Alarm.Shileld_BMS_WARN_over_temp_anomaly = 0;          //屏蔽BMS状态:过温告警
  Globa_1->Charger_Shileld_Alarm.Shileld_BMS_WARN_over_Cur_anomaly = 0;          //屏蔽BMS状态:BMS状态:过流告警
	
	Globa_1->Charger_Shileld_Alarm.Shileld_BMS_WARN_Insulation_anomaly = 0;          //BMS状态:绝缘异常
	Globa_1->Charger_Shileld_Alarm.Shileld_BMS_WARN_Outgoing_conn_anomaly = 0;          //BMS状态:输出连接器异常
	
	Globa_1->Charger_Shileld_Alarm.Shileld_BMS_OUT_OVER_V = 0;          //BMS状态:输出连接器异常
	Globa_1->Charger_Shileld_Alarm.Shileld_output_fuse_switch = 0;          ////输出熔丝段

	Globa_1->Charger_Shileld_Alarm.Shileld_keep_data = 0;          //保留
	
	Globa_1->Charger_Shileld_Alarm.Shileld_CELL_V_falg = 0;          //是否上送单体电压标志
	Globa_1->Charger_Shileld_Alarm.Shileld_CELL_T_falg = 0;          //是否上送单体温度标志
	Globa_1->Charger_Shileld_Alarm.Shileld_Control_board_comm = 0;// 控制板通信异常
	Globa_1->Charger_Shileld_Alarm.Shileld_meter_comm = 0;       //      电表通信异常
	Globa_1->Charger_Shileld_Alarm.Shileld_keep_comm = 0;       //  监控屏上的其他异常

  Globa_1->Charger_Shileld_Alarm.Shileld_keep[0] = 0;
  Globa_1->Charger_Shileld_Alarm.Shileld_keep[1] = 0;
  Globa_1->Charger_Shileld_Alarm.Shileld_keep[2] = 0;
  Globa_1->Charger_Shileld_Alarm.Shileld_keep[3] = 0;

}
extern STATUS System_Charger_Shileld_Alarm_save(void)
{
	UINT16 ret;
	UINT32 fileLen;
	
	fileLen = sizeof(Globa_1->Charger_Shileld_Alarm);

	ret = writeFile(F_Charger_Shileld_Alarm, (UINT8 *)&(Globa_1->Charger_Shileld_Alarm), fileLen, 0, 0);	
	if (ERROR16 == ret){
		perror("Save Charger_Shileld_Alarm: write File error\n");
		return ERROR16;
  }
	else
	 printf("Charger_Shileld_Alarm:write File success\n");

	ret = copyFile(F_Charger_Shileld_Alarm, F_BAK_Charger_Shileld_Alarm);
	if (ERROR16 == ret){
		printf("Charger_Shileld_Alarm_save:copy File error\n");
	}
	else
		printf("Charger_Shileld_Alarm_save:copy file success\n");

    return OK;
}

STATUS System_Charger_Shileld_Alarm_init(void)
{
	UINT16 i;
	UINT16 ret, ret1;
	UINT32 fileLen;

	memset(&(Globa_1->Charger_Shileld_Alarm), 0, sizeof(Globa_1->Charger_Shileld_Alarm));
	ret = readWholeFile(F_Charger_Shileld_Alarm, (UINT8 *)&(Globa_1->Charger_Shileld_Alarm), &fileLen); /* 第一次读取参数文件 */
	if(OK == ret){
		printf("read  Charger_Shileld_Alarm File sucess\n");
		return OK;
	}
  printf("read  Charger_Shileld_Alarm back File \n");
	ret = readWholeFile(F_BAK_Charger_Shileld_Alarm, (UINT8 *)&(Globa_1->Charger_Shileld_Alarm), &fileLen);   /* 若读取错误，读取备份文件 */
	if(OK == ret){
		ret1 = copyFile(F_BAK_Charger_Shileld_Alarm, F_Charger_Shileld_Alarm);   /* 拷贝副本到正本 *///add by dengjh 20110822
		if(ERROR16 == ret1){
			printf("read  Charger_param Fixedpirce back File error\n");
		}else{
			printf("read  Charger_param Fixedpirce back File ok\n");
		}
		return OK;
	}else{
		System_Charger_Shileld_Alarm_reset();    /*备份文件同样出错，按照默认初始化内存 */
		printf("read Charger_Shileld_Alarm  back File error, auto reset\n");

		if(ERROR16 == System_Charger_Shileld_Alarm_save()){
			printf("ERROR save parameters !\n");
		}
		return OK;
	}
}

extern STATUS ISO8583_NET_setting_save(void)
{
	UINT16 ret;
	UINT32 fileLen;

	fileLen = sizeof(Globa_1->ISO8583_NET_setting);

	ret = writeFile(F_SETNET_104, (UINT8 *)&(Globa_1->ISO8583_NET_setting), fileLen, 0, 0);	
	if (ERROR16 == ret){
		perror("Save net104 setting: write File error\n");
		return ERROR16;
        }
	else
		printf("Save net104 setting:write File success\n");

	ret = copyFile(F_SETNET_104, F_BAK_SETNET_104);    
	if (ERROR16 == ret){
		printf("Save net104 setting:copy File error\n");
	}
	else
		printf("Save net104 setting:copy file success\n");

    return OK;
}

void ISO8583_NET_setting_reset(void)
{
	sprintf(Globa_1->ISO8583_NET_setting.ISO8583_Server_IP, "%s", "120.25.153.93");
	Globa_1->ISO8583_NET_setting.ISO8583_Server_port[0] = 2407&0xff;
	Globa_1->ISO8583_NET_setting.ISO8583_Server_port[1] = (2407>>8)&0xff;
	Globa_1->ISO8583_NET_setting.addr[0] = 1;
	Globa_1->ISO8583_NET_setting.addr[1] = 0;
}

STATUS ISO8583_NET_setting_init(void)
{
	UINT16 i;
	UINT16 ret, ret1;
	UINT32 fileLen;	
	
	memset(&(Globa_1->ISO8583_NET_setting), 0, sizeof(Globa_1->ISO8583_NET_setting));
	ret = readWholeFile(F_SETNET_104, (UINT8 *)&(Globa_1->ISO8583_NET_setting), &fileLen); 
	if(OK == ret){		
		printf("read net104 setting File sucess\n");
		return OK;
	}

	printf("read net104 setting back File \n");
	ret = readWholeFile(F_BAK_SETNET_104, (UINT8 *)&(Globa_1->ISO8583_NET_setting), &fileLen);   /* 若读取错误，读取备份文件 */
	if(OK == ret){
		ret1 = copyFile(F_BAK_SETNET_104, F_SETNET_104);   /* 拷贝副本到正本 *///add by dengjh 20110822
		if(ERROR16 == ret1){
			printf("copy net104 setting back File error\n");
		}else{
			printf("copy net104 setting back File ok\n");
		}
		return OK;
	}else{
		ISO8583_NET_setting_reset();    /*备份文件同样出错，按照默认初始化内存 */
		printf("read net104 setting back File error, auto reset\n");	
		if(ERROR16 == ISO8583_NET_setting_save())
			printf("ERROR save net104 setting!\n");
		return OK;
	}
}

extern STATUS System_CarLOCK_Param_save(void)
{
	UINT16 ret;
	UINT32 fileLen;

	fileLen = sizeof(DEV_CFG);

	ret = writeFile(F_CarLOCK_Param, (UINT8 *)&(dev_cfg.dev_para.plug_lock_ctl), fileLen, 0, 0);	
	if (ERROR16 == ret){
		perror("Save CarLOCK setting: write File error\n");
		return ERROR16;
        }
	else
		printf("Save CarLOCK setting:write File success\n");

	ret = copyFile(F_CarLOCK_Param, F_BAK_CarLOCK_Param);    
	if (ERROR16 == ret){
		printf("Save CarLOCK_Param setting:copy File error\n");
	}
	else
		printf("Save CarLOCK_Param setting:copy file success\n");

    return OK;
}

void System_CarLOCK_Param_reset(void)
{
	dev_cfg.dev_para.plug_lock_ctl = 1;//是否启用枪锁控制,值0不启用，值1启用且检测锁状态，值2启用但不检测状态	
	dev_cfg.dev_para.car_lock_addr[0] = 1;
  dev_cfg.dev_para.car_lock_addr[1] = 2;

	dev_cfg.dev_para.car_lock_num = 0;//车位锁数量0---2
	dev_cfg.dev_para.car_lock_time = 1;//检测到无车的时候，车位上锁的时间,单位分钟，默认3分钟，在降下车锁后开始计时，超时后仍然无车则锁上车位
	dev_cfg.dev_para.car_park_price = 100;//每10分钟的停车费，单位元，系数0.01
  dev_cfg.dev_para.free_minutes_after_charge = 3*60 ;//充电完成后还可免费停车的时长,单位分钟
	dev_cfg.dev_para.free_when_charge = 0;//充电期间是否免停车费，值1免费，值0不免费
}

STATUS System_CarLOCK_Param_init(void)
{
	UINT16 i;
	UINT16 ret, ret1;
	UINT32 fileLen;	
	
	memset(&(dev_cfg.dev_para.plug_lock_ctl), 0, sizeof(DEV_CFG));
	ret = readWholeFile(F_CarLOCK_Param, (UINT8 *)&(dev_cfg.dev_para.plug_lock_ctl), &fileLen); 
	if(OK == ret){		
		printf("read CarLOCK_Param File sucess\n");
		return OK;
	}

	printf("read CarLOCK_Param back File \n");
	ret = readWholeFile(F_BAK_CarLOCK_Param, (UINT8 *)&(dev_cfg.dev_para.plug_lock_ctl), &fileLen);   /* 若读取错误，读取备份文件 */
	if(OK == ret){
		ret1 = copyFile(F_BAK_CarLOCK_Param, F_CarLOCK_Param);   /* 拷贝副本到正本 *///add by dengjh 20110822
		if(ERROR16 == ret1){
			printf("copy CarLOCK_Param back File error\n");
		}else{
			printf("copy CarLOCK_Param back File ok\n");
		}
		return OK;
	}else{
		System_CarLOCK_Param_reset();    /*备份文件同样出错，按照默认初始化内存 */
		printf("read CarLOCK_Param back File error, auto reset\n");	
		if(ERROR16 == System_CarLOCK_Param_save())
			printf("ERROR CarLOCK_Param setting!\n");
		return OK;
	}
}



extern STATUS System_BackgroupIssued_Param_save(void)
{
	UINT16 ret;
	UINT32 fileLen;

	fileLen = sizeof(BackgroupIssued);

	ret = writeFile(F_BackgroupIssued_Param, (UINT8 *)&(gBackgroupIssued.Electric_pile_workstart_Enable), fileLen, 0, 0);	
	if (ERROR16 == ret){
		perror("Save BackgroupIssued_Param setting: write File error\n");
		return ERROR16;
        }
	else
		printf("Save BackgroupIssued_Param setting:write File success\n");

	ret = copyFile(F_BackgroupIssued_Param, F_BAK_BackgroupIssued_Param);    
	if (ERROR16 == ret){
		printf("Save BackgroupIssued_Param setting:copy File error\n");
	}
	else
		printf("Save BackgroupIssued_Param setting:copy file success\n");

    return OK;
}

void System_BackgroupIssued_reset(void)
{
  gBackgroupIssued.Electric_pile_workstart_Enable = 0;
	gBackgroupIssued.keepdata[0] = 0;
	gBackgroupIssued.keepdata[1] = 0;
	gBackgroupIssued.keepdata[2] = 0;
	gBackgroupIssued.keepdata1[0] = 0;
	gBackgroupIssued.keepdata1[1] = 0;
	gBackgroupIssued.keepdata1[2] = 0;
}

STATUS System_BackgroupIssued_Param_init(void)
{
	UINT16 i;
	UINT16 ret, ret1;
	UINT32 fileLen;	
	
	memset(&(gBackgroupIssued.Electric_pile_workstart_Enable), 0, sizeof(BackgroupIssued));
	ret = readWholeFile(F_BackgroupIssued_Param, (UINT8 *)&(gBackgroupIssued.Electric_pile_workstart_Enable), &fileLen); 
	if(OK == ret){		
		printf("read BackgroupIssued_Param File sucess\n");
		return OK;
	}

	printf("read BackgroupIssued_Param back File \n");
	ret = readWholeFile(F_BAK_BackgroupIssued_Param, (UINT8 *)&(gBackgroupIssued.Electric_pile_workstart_Enable), &fileLen);   /* 若读取错误，读取备份文件 */
	if(OK == ret){
		ret1 = copyFile(F_BAK_BackgroupIssued_Param, F_BackgroupIssued_Param);   /* 拷贝副本到正本 *///add by dengjh 20110822
		if(ERROR16 == ret1){
			printf("copy BackgroupIssued_Param back File error\n");
		}else{
			printf("copy BackgroupIssued_Param back File ok\n");
		}
		return OK;
	}else{
		System_BackgroupIssued_reset();    /*备份文件同样出错，按照默认初始化内存 */
		printf("read BackgroupIssued_Param back File error, auto reset\n");	
		if(ERROR16 == System_BackgroupIssued_Param_save())
			printf("ERROR BackgroupIssued_Param setting!\n");
		return OK;
	}
}



void arm_qt_msg_init(void)
{
    key_t recv_id, send_id;

    if((send_id = ftok("/home/root/", 2)) < 0){
      printf("-------------------------------------Create recv_id error !\n\r" );
    }

    if((recv_id = ftok("/home/root/", 1)) < 0){
      printf("-------------------------------------Create send_id error !\n\r" );
    }

    if((Globa_1->arm_to_qt_msg_id = msgget(send_id, IPC_CREAT|0660))<0){
      printf("Create arm_to_qt_msg_id error !\n\r" );
    }else{
      printf("Create arm_to_qt_msg_id=%d\n\r", Globa_1->arm_to_qt_msg_id );
    }

    if((Globa_1->qt_to_arm_msg_id = msgget(recv_id, IPC_CREAT|0660))<0){
      printf("Create qt_to_arm_msg_id error !\n\r" );
    }else{
      printf("Create qt_to_arm_msg_id=%d\n\r", Globa_1->qt_to_arm_msg_id );
    }

    /*********************第二把枪的通道*********************************************/
    if((send_id = ftok("/home/root/", 4)) < 0){
      printf("-------------------------------------Create recv_id error !\n\r" );
    }

    if((recv_id = ftok("/home/root/", 3)) < 0){
      printf("-------------------------------------Create send_id error !\n\r" );
    }

    if((Globa_2->arm_to_qt_msg_id = msgget(send_id, IPC_CREAT|0660))<0){
      printf("Create Globa_2->arm_to_qt_msg_id error !\n\r" );
    }else{
      printf("Create Globa_2->arm_to_qt_msg_id=%d\n\r", Globa_2->arm_to_qt_msg_id );
    }

    if((Globa_2->qt_to_arm_msg_id = msgget(recv_id, IPC_CREAT|0660))<0){
      printf("Create Globa_2->qt_to_arm_msg_id error !\n\r" );
    }else{
      printf("Create Globa_2->qt_to_arm_msg_id=%d\n\r", Globa_2->qt_to_arm_msg_id );
    }
}



/*********************************************************
**description：系统初始化函数
** 包括共享内存的申请、系统配置参数初始化、排程初始化
**串口配置初始化
***parameter  :none
***return		  :none
**********************************************************/
void System_init(void)
{
	unsigned char tmp[48];

	LedRelayControl_init();


	share_memory();
	System_param_init();
	System_param2_init();
  System_paramFixedpirce_init();
	System_Charger_Shileld_Alarm_init();//屏蔽告警功能
	System_CarLOCK_Param_init();//地锁配置
  System_BackgroupIssued_Param_init();//后台参数配置
	Globa_1->Electric_pile_workstart_Enable = gBackgroupIssued.Electric_pile_workstart_Enable;
	Globa_2->Electric_pile_workstart_Enable = gBackgroupIssued.Electric_pile_workstart_Enable;
	//必须有初值，不然后面上电签到时临时版本Globa_1->tmp_APP_ver[0]内存值未初始化
	memcpy(&Globa_1->tmp_APP_ver[0], &Globa_1->Charger_param2.APP_ver[0], sizeof(Globa_1->Charger_param2.APP_ver));//APP版本号

	memset(&Globa_1->Error, 0x00, sizeof(Globa_1->Error));

	memcpy(Globa_2, Globa_1, sizeof(GLOBAVAL));//复制全局变量Globa_1的内容到Globa_1中，此时两份一样

	arm_qt_msg_init();

	ISO8583_NET_setting_init();
	
	pthread_mutex_init(&busy_db_pmutex, NULL);//初始化互斥对象
	pthread_mutex_init(&mutex_log, NULL);//初始化互斥对象
	pthread_mutex_init(&sw_fifo, NULL);//初始化互斥对象

	GROUP1_RUN_LED_OFF;
	GROUP2_RUN_LED_OFF;

}
/*******************************End of Param.c *******************************/
 
