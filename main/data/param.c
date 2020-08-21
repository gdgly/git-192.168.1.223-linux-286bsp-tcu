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


pthread_mutex_t *globa_pmutex;
extern pthread_mutex_t gLogMutex;
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

	Globa = (GLOBAVAL *)ch_sysCfg;
	bzero(Globa, sizeof(GLOBAVAL));

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
	int i=0;
	Globa->Charger_param.charge_modlue_factory = HUAWEI_MODULE;//充电模块厂家
	Globa->Charger_param.charge_modlue_index = GR95021;
	

	Globa->Charger_param.Charge_rate_voltage = 950*1000;		  //充电机额定电压  0.001V

	Globa->Charger_param.Charge_rate_number[0] = 2;		          //模块数量1
	Globa->Charger_param.Charge_rate_number[1] = 2;		          //模块数量2
	Globa->Charger_param.Charge_rate_number[2] = 2;		          //模块数量3
	Globa->Charger_param.Charge_rate_number[3] = 2;		          //模块数量4
	Globa->Charger_param.Charge_rate_number[4] = 2;		          //模块数量5
	
	Globa->Charger_param.model_rate_current = 21*1000;	      //单模块额定电流  0.001A
	Globa->Charger_param.Charge_rate_current[0] = Globa->Charger_param.Charge_rate_number[0]*Globa->Charger_param.model_rate_current;		  //充电机1额定电流  0.001A
	Globa->Charger_param.Charge_rate_current[1] = Globa->Charger_param.Charge_rate_number[1]*Globa->Charger_param.model_rate_current;		  //充电机2额定电流  0.001A
	Globa->Charger_param.Charge_rate_current[2] = Globa->Charger_param.Charge_rate_number[2]*Globa->Charger_param.model_rate_current;		  //充电机1额定电流  0.001A
	Globa->Charger_param.Charge_rate_current[3] = Globa->Charger_param.Charge_rate_number[3]*Globa->Charger_param.model_rate_current;		  //充电机2额定电流  0.001A
	Globa->Charger_param.Charge_rate_current[4] = Globa->Charger_param.Charge_rate_number[4]*Globa->Charger_param.model_rate_current;		  //充电机2额定电流  0.001A

	Globa->Charger_param.set_voltage = Globa->Charger_param.Charge_rate_voltage*4/5;
	Globa->Charger_param.set_current = Globa->Charger_param.Charge_rate_current[0]*1/2;
	Globa->Charger_param.System_Type = 0; //系统类型---0--双枪同时充 ---1双枪轮流充电 2--单枪立式 3-壁挂式

	Globa->Charger_param.gun_config  = 4; 
	Globa->Charger_param.Input_Relay_Ctl_Type = 0; 
	memcpy(&Globa->Charger_param.SN[0], "0000000000000006", sizeof(Globa->Charger_param.SN));//充电终端序列号    41	N	n12

	//Globa->Charger_param.couple_flag = 1;		          //充电机单、并机标示 1：单机 2：并机
	//Globa->Charger_param.charge_addr = 1;		          //充电机地址偏移，组要是在并机模式时用，1、N

	//Globa->Charger_param.limit_power = 9999;         //出功率限制点(离线时用的默认功率限制值)  9999kw
	Globa->Charger_param.meter_config_flag = DCM3366Q;      //电能表配置标志 

	Globa->Charger_param.input_over_limit = 445*1000;		    //输入过压点  0.001V 380*1.17 445
	Globa->Charger_param.input_lown_limit = 300*1000;		    //输入欠压点  0.001V 380*0.79 300
	Globa->Charger_param.output_over_limit = Globa->Charger_param.Charge_rate_voltage + 5*1000;	 //输出过压点  0.001V
	Globa->Charger_param.output_lown_limit =  199*1000;		  //输出欠压点  0.001V
	Globa->Charger_param.current_limit[0] = 250;
	Globa->Charger_param.current_limit[1] = 250;
	Globa->Charger_param.current_limit[2] = 250;
	Globa->Charger_param.current_limit[3] = 250;
	Globa->Charger_param.current_limit[4] = 250;
	Globa->Charger_param.BMS_work = 1;		                  //系统工作模式  1：正常，2：测试
	Globa->Charger_param.BMS_CAN_ver = 1;		               //BMS的协议版本 1：国家标准 2：普天标准-深圳版本
	Globa->Charger_param.AC_DAQ_POS = 1;  //默认接在控制板


	Globa->Charger_param.Price = 100000;//1.00000元 100;                //单价,精确到分 1元
	for(i=0;i<CONTROL_DC_NUMBER;i++)
		Globa->Charger_param.DC_Shunt_Range[i] = 300;       //直流分流器量程设置A
	memcpy(&Globa->Charger_param.Power_meter_addr[0][0], "163001404491", 12);//电能表地址1 6字节
	memcpy(&Globa->Charger_param.Power_meter_addr[1][0], "163001404505", 12);//电能表地址2 6字节
	memcpy(&Globa->Charger_param.Power_meter_addr[2][0], "165254100522", 12);//电能表地址3 6字节
	memcpy(&Globa->Charger_param.Power_meter_addr[3][0], "163001407644", 12);//电能表地址4 6字节
	memcpy(&Globa->Charger_param.Power_meter_addr[4][0], "163001407645", 12);//电能表地址5 6字节
	
	memcpy(&Globa->Charger_param.AC_meter_addr[0], "AAAAAAAAAAAA", 12);

	Globa->Charger_param.NEW_VOICE_Flag = 1;//
	Globa->Charger_param.AC_Meter_CT = 120;//600:5--->120

    for (i = 0; i < Globa->Charger_param.gun_config; i++)
    {
        Globa->Electric_pile_workstart_Enable[i] = 1;//桩使能
    }

	Globa->Charger_param.Key[0] = 0xFF;
	Globa->Charger_param.Key[1] = 0xFF;
	Globa->Charger_param.Key[2] = 0xFF;
	Globa->Charger_param.Key[3] = 0xFF;
	Globa->Charger_param.Key[4] = 0xFF;
	Globa->Charger_param.Key[5] = 0xFF;

	Globa->Charger_param.Charging_gun_type = 0;
	Globa->Charger_param.gun_allowed_temp = 95;
	Globa->Charger_param.Serial_Network  = 0;//后台协议走的是DTU还是网络 1--DTU 0--网络
	Globa->Charger_param.DTU_Baudrate = 2;

	Globa->Charger_param.SOC_limit = 100;
	Globa->Charger_param.min_current = 30;
	
	Globa->Charger_param.heartbeat_idle_period = 40;
	Globa->Charger_param.heartbeat_busy_period = 20;
	
	Globa->Charger_param.COM_Func_sel[0] = CARD_READER_FUNC;//COM1 RS232
	Globa->Charger_param.COM_Func_sel[1] = COM_NOT_USED;//COM2 RS232
	Globa->Charger_param.COM_Func_sel[2] = AC_INPUT_METER_FUNC;//COM3 RS485
	Globa->Charger_param.COM_Func_sel[3] = DC_MEASURE_METER_FUNC;//COM4 RS485
	Globa->Charger_param.COM_Func_sel[4] = AC_CAIJI_FUNC;//COM5 RS485
	Globa->Charger_param.COM_Func_sel[5] = COM_NOT_USED;//COM6 RS485
	
	Globa->Charger_param.support_double_gun_to_one_car = 0;//默认启用双枪充1车
	Globa->Charger_param.support_double_gun_power_allocation = 0;
	strcpy(Globa->Charger_param.dev_location,"EAST");
	strcpy(Globa->Charger_param.dev_user_info,"FreeScale ARM Linux");
	
	Globa->Charger_param.Power_Contor_By_Mode = 1;     //功率控制方式1-通过监控屏控制，0-通过CCU/TCU控制，因为多枪一般是给4枪配置的，
	Globa->Charger_param.Contactor_Line_Num = 4;  //系统并机数量
	Globa->Charger_param.Contactor_Line_Position[0] = (1<<4|3);  //系统并机路线节点：高4位为主控点（即控制并机的ccu）地4位为连接点
	Globa->Charger_param.Contactor_Line_Position[0] = (3<<4|4);  //系统并机路线节点：高4位为主控点（即控制并机的ccu）地4位为连接点
	Globa->Charger_param.Contactor_Line_Position[0] = (4<<4|2);  //系统并机路线节点：高4位为主控点（即控制并机的ccu）地4位为连接点
	Globa->Charger_param.Contactor_Line_Position[0] = (2<<4|1);  //系统并机路线节点：高4位为主控点（即控制并机的ccu）地4位为连接点
	Globa->Charger_param.Contactor_Line_Position[0] = (0<<4|0);  //系统并机路线节点：高4位为主控点（即控制并机的ccu）地4位为连接点	
	Globa->Charger_param.CurrentVoltageJudgmentFalg = 0;
}


extern STATUS System_param_save(void)
{
	UINT16 ret;
	UINT32 fileLen;
	
	fileLen = sizeof(Globa->Charger_param);

	ret = writeFile(F_UPSPARAM, (UINT8 *)&(Globa->Charger_param), fileLen, 0, 0);	
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

	memset(&(Globa->Charger_param), 0, sizeof(Globa->Charger_param));
	ret = readWholeFile(F_UPSPARAM, (UINT8 *)&(Globa->Charger_param), &fileLen); /* 第一次读取参数文件 */
	if(OK == ret){
		printf("read  Charger_param File sucess\n");
		return OK;
	}

    printf("read  Charger_param back File \n");
	ret = readWholeFile(F_BAK_UPSPARAM, (UINT8 *)&(Globa->Charger_param), &fileLen);   /* 若读取错误，读取备份文件 */
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
	memset(&Globa->Charger_param2.invalid_card_ver[0], '0', sizeof(Globa->Charger_param2.invalid_card_ver));
	memset(&Globa->Charger_param2.Software_Version[0], '0', sizeof(Globa->Charger_param2.Software_Version));
	memset(&Globa->Charger_param2.APP_ver[0], '0', sizeof(Globa->Charger_param2.APP_ver));
	memset(Globa->Charger_param2.http,0,sizeof(Globa->Charger_param2.http));
	
    /* 分时电价的默认信息 */
    memset(&Globa->Charger_param2.price_type_ver[0], '0', sizeof(Globa->Charger_param2.price_type_ver));
    Globa->Charger_param2.show_price_type = 0;  // 默认详细显示
    Globa->Charger_param2.time_zone_num = 1;  // 默认1个时段
	  
    for (i = 0; i < TIME_SHARE_SECTION_NUMBER; i++)
	{
        Globa->Charger_param2.share_time_kwh_price[i] = 100000; // 默认1元   
        Globa->Charger_param2.share_time_kwh_price_serve[i] = 100000; // 默认1元  
        Globa->Charger_param2.time_zone_tabel[0] = 0;
        Globa->Charger_param2.time_zone_feilv[0] = 0;
	}

    /* 预约费默认信息 */
    Globa->Charger_param2.book_price = 0;

    // 停车费默认免费
    Globa->Charger_param2.charge_stop_price.stop_price_ver = 0;
    Globa->Charger_param2.charge_stop_price.is_free_when_charge = 0;
    Globa->Charger_param2.charge_stop_price.stop_price = 0;
    Globa->Charger_param2.charge_stop_price.free_time = 0;
	
}
extern STATUS System_param2_save(void)
{
	UINT16 ret;
	UINT32 fileLen;
	
	fileLen = sizeof(Globa->Charger_param2);

	ret = writeFile(F_UPSPARAM2, (UINT8 *)&(Globa->Charger_param2), fileLen, 0, 0);	
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

	memset(&(Globa->Charger_param2), 0, sizeof(Globa->Charger_param2));
	ret = readWholeFile(F_UPSPARAM2, (UINT8 *)&(Globa->Charger_param2), &fileLen); /* 第一次读取参数文件 */
	if(OK == ret){
		printf("read  Charger_param2 File sucess\n");
		return OK;
	}

    printf("read  Charger_param2 back File \n");
	ret = readWholeFile(F_BAK_UPSPARAM2, (UINT8 *)&(Globa->Charger_param2), &fileLen);   /* 若读取错误，读取备份文件 */
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

//定时电价
void	System_ParamFixedpirce_reset(void)
{
	UINT32 i = 0;

	for(i = 0;i<4;i++)
	{
		Globa->charger_fixed_price.share_time_kwh_price[i]= Globa->Charger_param2.share_time_kwh_price[i];  //单价,默认为非时电价
		Globa->charger_fixed_price.share_time_kwh_price_serve[i]= Globa->Charger_param2.share_time_kwh_price_serve[i];  //单价,默认为非时电价
	}
	Globa->charger_fixed_price.time_zone_num = 1;
	for(i = 0;i<12;i++)
	{
	  Globa->charger_fixed_price.time_zone_tabel[i] = Globa->Charger_param2.time_zone_tabel[i];  //单价,默认为非时电价
	  Globa->charger_fixed_price.time_zone_feilv[i] = Globa->Charger_param2.time_zone_feilv[i];  //单价,默认为非时电价
	}
	for(i=0;i<7;i++){
		Globa->charger_fixed_price.fixed_price_effective_time[i] = 0;
	}
	Globa->charger_fixed_price.show_price_type = Globa->Charger_param2.show_price_type;
}
extern STATUS System_param_Fixedpirce_save(void)
{
	UINT16 ret;
	UINT32 fileLen;
	
	fileLen = sizeof(Globa->charger_fixed_price);

	ret = writeFile(F_PARAMFixedpirce, (UINT8 *)&(Globa->charger_fixed_price), fileLen, 0, 0);	
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

	memset(&(Globa->charger_fixed_price), 0, sizeof(Globa->charger_fixed_price));
	ret = readWholeFile(F_PARAMFixedpirce, (UINT8 *)&(Globa->charger_fixed_price), &fileLen); /* 第一次读取参数文件 */
	if(OK == ret){
		printf("read  charger_fixed_price File sucess\n");
		return OK;
	}

    printf("read  Charger_paramFixedpirce back File \n");
	ret = readWholeFile(F_BAK_PARAMFixedpirce, (UINT8 *)&(Globa->charger_fixed_price), &fileLen);   /* 若读取错误，读取备份文件 */
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
	Globa->Charger_Shileld_Alarm.Shileld_PE_Fault = 0;	       //屏蔽接地故障告警 ----非停机故障，指做显示	    
	Globa->Charger_Shileld_Alarm.Shileld_AC_connect_lost = 0;	//屏蔽交流输通讯告警	---非停机故障，只提示告警信息    
	Globa->Charger_Shileld_Alarm.Shileld_AC_input_switch_off = 0;//屏蔽交流输入开关跳闸-----非停机故障交流接触器
	Globa->Charger_Shileld_Alarm.Shileld_AC_fanglei_off = 0;    //屏蔽交流防雷器跳闸---非停机故障，只提示告警信息 
	Globa->Charger_Shileld_Alarm.Shileld_circuit_breaker_trip = 0;//交流断路器跳闸.2---非停机故障，只提示告警信息 
	Globa->Charger_Shileld_Alarm.Shileld_circuit_breaker_off = 0;// 交流断路器断开.2---非停机故障，只提示告警信息 
	Globa->Charger_Shileld_Alarm.Shileld_AC_input_over_limit = 0;//屏蔽交流输入过压保护 ----  ---非停机故障，只提示告警信息 
	Globa->Charger_Shileld_Alarm.Shileld_AC_input_lown_limit = 0; //屏蔽交流输入欠压 ---非停机故障，只提示告警信息 
	
	Globa->Charger_Shileld_Alarm.Shileld_GUN_LOCK_FAILURE = 0;  //屏蔽枪锁检测未通过
	Globa->Charger_Shileld_Alarm.Shileld_AUXILIARY_POWER = 0;   //屏蔽辅助电源检测未通过
	Globa->Charger_Shileld_Alarm.Shileld_OUTSIDE_VOLTAGE = 0;    //屏蔽输出接触器外侧电压检测（标准10V，放开100V）
	Globa->Charger_Shileld_Alarm.Shileld_SELF_CHECK_V = 0;      //屏蔽自检输出电压异常
  Globa->Charger_Shileld_Alarm.Shileld_CHARG_OUT_OVER_V = 0;  //屏蔽输出过压停止充电   ----  data2
  Globa->Charger_Shileld_Alarm.Shileld_CHARG_OUT_Under_V = 0;  //屏蔽输出欠压停止充电   ----  data2
	Globa->Charger_Shileld_Alarm.Shileld_CHARG_OUT_OVER_C = 0;  //屏蔽输出过流
	Globa->Charger_Shileld_Alarm.Shileld_OUT_SHORT_CIRUIT = 0;  //屏蔽输出短路
	
	Globa->Charger_Shileld_Alarm.Shileld_GUN_OVER_TEMP = 0;     //屏蔽充电枪温度过高
	Globa->Charger_Shileld_Alarm.Shileld_INSULATION_CHECK = 0;   //屏蔽绝缘检测
	Globa->Charger_Shileld_Alarm.Shileld_OUT_SWITCH = 0;       //屏蔽输出开关检测
	Globa->Charger_Shileld_Alarm.Shileld_OUT_NO_CUR = 0;       //屏蔽输出无电流显示检测	
	Globa->Charger_Shileld_Alarm.Shileld_BMS_WARN_Monomer_vol_anomaly = 0;          //屏蔽BMS状态:单体电压异常
	Globa->Charger_Shileld_Alarm.Shileld_BMS_WARN_Soc_anomaly = 0;          //屏蔽BMS状态:SOC异常
	Globa->Charger_Shileld_Alarm.Shileld_BMS_WARN_over_temp_anomaly = 0;          //屏蔽BMS状态:过温告警
  Globa->Charger_Shileld_Alarm.Shileld_BMS_WARN_over_Cur_anomaly = 0;          //屏蔽BMS状态:BMS状态:过流告警
	
	Globa->Charger_Shileld_Alarm.Shileld_BMS_WARN_Insulation_anomaly = 0;          //BMS状态:绝缘异常
	Globa->Charger_Shileld_Alarm.Shileld_BMS_WARN_Outgoing_conn_anomaly = 0;          //BMS状态:输出连接器异常
	
	Globa->Charger_Shileld_Alarm.Shileld_BMS_OUT_OVER_V = 0;          //BMS状态:输出连接器异常
	Globa->Charger_Shileld_Alarm.Shileld_output_fuse_switch = 0;          ////输出熔丝段

	Globa->Charger_Shileld_Alarm.Shileld_keep_data = 0;          //保留
	
	Globa->Charger_Shileld_Alarm.Shileld_CELL_V_falg = 0;          //是否上送单体电压标志
	Globa->Charger_Shileld_Alarm.Shileld_CELL_T_falg = 0;          //是否上送单体温度标志
	Globa->Charger_Shileld_Alarm.Shileld_Control_board_comm = 0;// 控制板通信异常
	Globa->Charger_Shileld_Alarm.Shileld_meter_comm = 0;       //      电表通信异常
	Globa->Charger_Shileld_Alarm.Shileld_keep_comm = 0;       //  监控屏上的其他异常

  Globa->Charger_Shileld_Alarm.Shileld_keep[0] = 0;
  Globa->Charger_Shileld_Alarm.Shileld_keep[1] = 0;
  Globa->Charger_Shileld_Alarm.Shileld_keep[2] = 0;
  Globa->Charger_Shileld_Alarm.Shileld_keep[3] = 0;

}
extern STATUS System_Charger_Shileld_Alarm_save(void)
{
	UINT16 ret;
	UINT32 fileLen;
	
	fileLen = sizeof(Globa->Charger_Shileld_Alarm);

	ret = writeFile(F_Charger_Shileld_Alarm, (UINT8 *)&(Globa->Charger_Shileld_Alarm), fileLen, 0, 0);	
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

	memset(&(Globa->Charger_Shileld_Alarm), 0, sizeof(Globa->Charger_Shileld_Alarm));
	ret = readWholeFile(F_Charger_Shileld_Alarm, (UINT8 *)&(Globa->Charger_Shileld_Alarm), &fileLen); /* 第一次读取参数文件 */
	if(OK == ret){
		printf("read  Charger_Shileld_Alarm File sucess\n");
		return OK;
	}
  printf("read  Charger_Shileld_Alarm back File \n");
	ret = readWholeFile(F_BAK_Charger_Shileld_Alarm, (UINT8 *)&(Globa->Charger_Shileld_Alarm), &fileLen);   /* 若读取错误，读取备份文件 */
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

	fileLen = sizeof(Globa->ISO8583_NET_setting);

	ret = writeFile(F_SETNET_104, (UINT8 *)&(Globa->ISO8583_NET_setting), fileLen, 0, 0);	
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
	sprintf(Globa->ISO8583_NET_setting.ISO8583_Server_IP, "%s", "218.16.97.171");
	Globa->ISO8583_NET_setting.ISO8583_Server_port[0] = 2407&0xff;
	Globa->ISO8583_NET_setting.ISO8583_Server_port[1] = (2407>>8)&0xff;
	Globa->ISO8583_NET_setting.addr[0] = 1;
	Globa->ISO8583_NET_setting.addr[1] = 0;
}

STATUS ISO8583_NET_setting_init(void)
{
	UINT16 i;
	UINT16 ret, ret1;
	UINT32 fileLen;	
	
	memset(&(Globa->ISO8583_NET_setting), 0, sizeof(Globa->ISO8583_NET_setting));
	ret = readWholeFile(F_SETNET_104, (UINT8 *)&(Globa->ISO8583_NET_setting), &fileLen); 
	if(OK == ret){		
		printf("read net104 setting File sucess\n");
		return OK;
	}

	printf("read net104 setting back File \n");
	ret = readWholeFile(F_BAK_SETNET_104, (UINT8 *)&(Globa->ISO8583_NET_setting), &fileLen);   /* 若读取错误，读取备份文件 */
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


void Power_Control_Config_init(void)
{ 
   int i =0;
	for(i=0;i<CONTROL_DC_NUMBER;i++){
		Globa->Control_DC_Infor_Data[i].kw_percent = 1000;
		Globa->Control_DC_Infor_Data[i].kw_limit_end_time[0] = 0;
		Globa->Control_DC_Infor_Data[i].kw_limit_end_time[1] = 0;
		Globa->Control_DC_Infor_Data[i].kw_limit_end_time[2] = 0;
		Globa->Control_DC_Infor_Data[i].kw_limit_end_time[3] = 15;
		Globa->Control_DC_Infor_Data[i].kw_limit_end_time[4] = 20;
		Globa->Control_DC_Infor_Data[i].kw_limit_end_time[5] = 9;
		Globa->Control_DC_Infor_Data[i].kw_limit_end_time[6] = 19;
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

    if((Globa->arm_to_qt_msg_id = msgget(send_id, IPC_CREAT|0660))<0){
      printf("Create arm_to_qt_msg_id error !\n\r" );
    }else{
      printf("Create arm_to_qt_msg_id=%d\n\r", Globa->arm_to_qt_msg_id );
    }

    if((Globa->qt_to_arm_msg_id = msgget(recv_id, IPC_CREAT|0660))<0){
      printf("Create qt_to_arm_msg_id error !\n\r" );
    }else{
      printf("Create qt_to_arm_msg_id=%d\n\r", Globa->qt_to_arm_msg_id );
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
	
	int i;
	LedRelayControl_init();

	share_memory();
	
	System_param_init();
	System_param2_init();
	
	System_Charger_Shileld_Alarm_init();//屏蔽告警功能

	if(Globa->Charger_param.gun_config > CONTROL_DC_NUMBER)
		Globa->Charger_param.gun_config = CONTROL_DC_NUMBER;//最大4把枪===

	////////////////////////////////TEST//////////////////////////
    for (i = 0; i < Globa->Charger_param.gun_config; i++)
    {
        Globa->Electric_pile_workstart_Enable[i] = 1;//桩使能
    } 
	///////////////////////////////////////////////////////////////
	for(i=0;i<CONTROL_DC_NUMBER;i++)
		memset(&Globa->Control_DC_Infor_Data[i], 0x00, sizeof(Control_DC_INFOR));

	if((Globa->Charger_param.SOC_limit > 100)||(Globa->Charger_param.SOC_limit < 20))
	{
	  Globa->Charger_param.SOC_limit = 100;	//SOC达到多少时，判断电流低于min_current后停止充电,值95表示95%
	}
	
	if( (Globa->Charger_param.min_current < 15)||(Globa->Charger_param.min_current > 250))
		Globa->Charger_param.min_current = 15;
	
	if(Globa->Charger_param.heartbeat_idle_period < 30 || Globa->Charger_param.heartbeat_idle_period > 120)
		Globa->Charger_param.heartbeat_idle_period = 40;
	if(Globa->Charger_param.heartbeat_busy_period < 5 || Globa->Charger_param.heartbeat_busy_period > 60)
		Globa->Charger_param.heartbeat_busy_period = 20;
	
	if(Globa->Charger_param.COM_Func_sel[0] > MODBUS_SLAVE_FUNC)
		Globa->Charger_param.COM_Func_sel[0] = CARD_READER_FUNC;
	if(Globa->Charger_param.COM_Func_sel[1] > MODBUS_SLAVE_FUNC)
		Globa->Charger_param.COM_Func_sel[1] = COM_NOT_USED;
	if(Globa->Charger_param.COM_Func_sel[2] > COM_END_FUNC)
		Globa->Charger_param.COM_Func_sel[2] = AC_INPUT_METER_FUNC;
	if(Globa->Charger_param.COM_Func_sel[3] > COM_END_FUNC)
		Globa->Charger_param.COM_Func_sel[3] = DC_MEASURE_METER_FUNC;
	
	if(Globa->Charger_param.COM_Func_sel[4] > COM_END_FUNC)
		Globa->Charger_param.COM_Func_sel[4] = COM_NOT_USED;
	if(Globa->Charger_param.COM_Func_sel[5] > COM_END_FUNC)
		Globa->Charger_param.COM_Func_sel[5] = COM_NOT_USED;
	
	arm_qt_msg_init();

	ISO8583_NET_setting_init();
	Power_Control_Config_init();// 功率配置表
	
  memset(Globa->Charger_Contactor_param_data,0,SYS_CONTACTOR_NUMBER*sizeof(CHARGER_CONTACTOR_PARAM));
	memset(Globa->Charger_gunbus_param_data,0,MODULE_INPUT_GROUP*sizeof(CHARGER_GUN_BUS_PARAM));
  printf("---------------Charger_Contactor_param_data---Charger_gunbus_param_data-------------memset ok!\n\r" );

  ModGroup_data_init(); //根据选择配置初始话数据
	printf("---------------ModGroup_data_init------------init ok!\n\r" );

  ModGroup_Diagram_list_init(); //初始关系表
	printf("---------------ModGroup_Diagram_list_init------------init ok!\n\r" );
	
	pthread_mutex_init(&busy_db_pmutex, NULL);//初始化互斥对象
	pthread_mutex_init(&mutex_log, NULL);//初始化互斥对象
	pthread_mutex_init(&gLogMutex, NULL);//初始化互斥对象
	pthread_mutex_init(&modbus_pc_mutex, NULL);//初始化互斥对象

}
/*******************************End of Param.c *******************************/
 
