/*******************************************************************************
  Copyright(C)    2014-2015,      EAST Co.Ltd.
  File name :     bms.c
  Author :        dengjh
  Version:        1.0
  Date:           2014.5.11
  Description:    
  Other:          
  Function List:  
  History:        
          <author>   <time>   <version>   <desc>
            dengjh    2014.4.11   1.0      Create
*******************************************************************************/
#include <getopt.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include<time.h>
#include <sys/prctl.h>
#include "globalvar.h"
#include "MC_can.h"
#include "Power_Proces_Task.h"
#include "Allocation_Power.h"


#define PF_CAN   29
#define MAX_MODEL_CURRENT_LIMINT      95  //模块节能模式时电流控制点默认95%
#define CHARGE_PROCES_DEBUG 0
extern UINT32 Dc_shunt_Set_CL_flag[CONTROL_DC_NUMBER];

MC_ALL_FRAME    MC_All_Frame[CONTROL_DC_NUMBER];
MC_CONTROL_VAR  mc_control_var[CONTROL_DC_NUMBER];

#define  PGN16256_ID_CM_1 0x18108AF6  //	控制器-功率分配
unsigned char ls_need_model_num_temp[CONTROL_DC_NUMBER]= {0};
unsigned char host_recv_flag[CONTROL_DC_NUMBER] = {0};
unsigned char host_congji[CONTROL_DC_NUMBER][CONTROL_DC_NUMBER];
unsigned int model_temp_v[CONTROL_DC_NUMBER];
unsigned int model_temp_a[CONTROL_DC_NUMBER];
unsigned char host_model_num[CONTROL_DC_NUMBER];
unsigned char debug = 0;
unsigned char pre_charge_num[CONTROL_DC_NUMBER] = {0};
unsigned char PGN3840_ID_MC_SentwaitCount[CONTROL_DC_NUMBER] = {0};
unsigned char Delay_Start_falg[CONTROL_DC_NUMBER] = {0};

SWITCH_MODGROUP_LIST gSwitch_ModGroup_list[CONTROL_DC_NUMBER];
//五枪变量定义
#define max_gun_num CONTROL_DC_NUMBER //宏定义系统的枪数量便于数组的定义
unsigned char GUN_MAX  = max_gun_num;   //该变量根据系统类型进行赋初值 

#define USER_TICK_PERIOD    10 //计数器递增1代表的时长，单位ms
#define RELAY_CTL_ST_WAIT_PLUG_IN   0 //等待插枪
#define RELAY_CTL_ST_WAIT_PLUG_CHG  1 //已插枪等待充电

#define RELAY_CTL_ST_WAIT_CHG_END   2//已经充电等待充电结束
#define RELAY_CTL_ST_CHG_END_DELAY  3//充电结束延迟关模块或风机
#define RELAY_CTL_ST_WAIT_PLUG_OUT  4 //等待充电超时或1次充电完成等待拔枪-------此状态下已申请关闭模块输入



//Gun_No 0---3 启动一个定时器定时time指定时长，单位ms
//返回值1表示有空闲定时器，定时器初始化成功
UINT32 MC_timer_start(int Gun_No, int time)
{

  int i;
	int gun_config = 0;
	if((Globa->Charger_param.gun_config == 2)&&(Globa->Charger_param.support_double_gun_power_allocation ==1))
	{
		gun_config = 4;
	}else{
		gun_config = Globa->Charger_param.gun_config;//4;
	}
	
	if(Gun_No >= gun_config)
		return 0;
	for(i=0;i<USER_TIMER_NO;i++)
	{
		//if(0==mc_control_var[Gun_No].mc_timers[i].timer_enable)
		{
			if( (mc_control_var[Gun_No].mc_timers[i].timer_set_in_ms == 0)||//0表示该计数器未初始化可用=========
				(mc_control_var[Gun_No].mc_timers[i].timer_set_in_ms == time)//之前已初始化过，重新开始计数
			)
			{
				mc_control_var[Gun_No].mc_timers[i].timer_end_flag = 0;
				mc_control_var[Gun_No].mc_timers[i].timer_tick_cnt = 0;
				mc_control_var[Gun_No].mc_timers[i].timer_stop_tick = time/USER_TICK_PERIOD;//设置定时器终点计数
				mc_control_var[Gun_No].mc_timers[i].timer_set_in_ms = time;
				mc_control_var[Gun_No].mc_timers[i].timer_enable = 1;//start
				return 1;
				//break;//break at first match
			}
		}
	}
	return 0;//no idle timer
}

//停止指定时长的定时器
//返回值1表示停止成功
UINT32 MC_timer_stop(int Gun_No, int time)
{

	int i;
		int gun_config = 0;
	if((Globa->Charger_param.gun_config == 2)&&(Globa->Charger_param.support_double_gun_power_allocation ==1))
	{
		gun_config = 4;
	}else{
		gun_config = Globa->Charger_param.gun_config;//4;
	}
	
	if(Gun_No >= gun_config)
		return 0;
	for(i=0;i<USER_TIMER_NO;i++)
	{
		if(mc_control_var[Gun_No].mc_timers[i].timer_enable)
		{
			if(mc_control_var[Gun_No].mc_timers[i].timer_set_in_ms == time)//match
			{
				mc_control_var[Gun_No].mc_timers[i].timer_enable = 0;//stop
				mc_control_var[Gun_No].mc_timers[i].timer_end_flag = 0;
				mc_control_var[Gun_No].mc_timers[i].timer_tick_cnt = 0;
				mc_control_var[Gun_No].mc_timers[i].timer_stop_tick = 0;//reset
				mc_control_var[Gun_No].mc_timers[i].timer_set_in_ms = 0;//reset
				return 1;//break at first match
				//break;
			}
		}
	}
	return 0;
}

//初始化指定充电枪的所有定时器
void MC_Init_timers(int Gun_No)
{

	int i;
		int gun_config = 0;
	if((Globa->Charger_param.gun_config == 2)&&(Globa->Charger_param.support_double_gun_power_allocation ==1))
	{
		gun_config = 4;
	}else{
		gun_config = Globa->Charger_param.gun_config;//4;
	}
	
	if(Gun_No >= gun_config)
		return 0;
	for(i=0;i<USER_TIMER_NO;i++)
		memset(&mc_control_var[Gun_No].mc_timers[i], 0, sizeof(MC_TIMER));
	
}
//运行指定充电枪的定时器
void MC_timer_run(int Gun_No)
{

	int i;
		int gun_config = 0;
	if((Globa->Charger_param.gun_config == 2)&&(Globa->Charger_param.support_double_gun_power_allocation ==1))
	{
		gun_config = 4;
	}else{
		gun_config = Globa->Charger_param.gun_config;//4;
	}
	
	if(Gun_No >= gun_config)
		return 0;
	for(i=0;i<USER_TIMER_NO;i++)
	{
		if(mc_control_var[Gun_No].mc_timers[i].timer_enable)
		{
			if(0 == mc_control_var[Gun_No].mc_timers[i].timer_end_flag)
			{
				mc_control_var[Gun_No].mc_timers[i].timer_tick_cnt++;				
				if(mc_control_var[Gun_No].mc_timers[i].timer_tick_cnt >= mc_control_var[Gun_No].mc_timers[i].timer_stop_tick)
					mc_control_var[Gun_No].mc_timers[i].timer_end_flag = 1;
			}
		}
	}	
}

//返回值1表示对应的定时器定时时间到
UINT32 MC_timer_flag(int Gun_No, int time)
{
	UINT32 flag=0;
	int gun_config = 0;
	if((Globa->Charger_param.gun_config == 2)&&(Globa->Charger_param.support_double_gun_power_allocation ==1))
	{
		gun_config = 4;
	}else{
		gun_config = Globa->Charger_param.gun_config;//4;
	}
	
	if(Gun_No >= gun_config)
		return 0;
	int i;
	for(i=0;i<USER_TIMER_NO;i++)
	{
		if(mc_control_var[Gun_No].mc_timers[i].timer_enable)
		{
			if(mc_control_var[Gun_No].mc_timers[i].timer_set_in_ms == time)//match
			{
				flag = mc_control_var[Gun_No].mc_timers[i].timer_end_flag;
				break;//break at first match
			}
		}
	}
	return (flag);
}

UINT32 MC_timer_cur_tick(int Gun_No ,int time)
{

	int i;
		int gun_config = 0;
	if((Globa->Charger_param.gun_config == 2)&&(Globa->Charger_param.support_double_gun_power_allocation ==1))
	{
		gun_config = 4;
	}else{
		gun_config = Globa->Charger_param.gun_config;//4;
	}
	
	if(Gun_No >= gun_config)
		return 0;
	for(i=0;i<USER_TIMER_NO;i++)
	{
		if(mc_control_var[Gun_No].mc_timers[i].timer_enable)
		{
			if(mc_control_var[Gun_No].mc_timers[i].timer_set_in_ms == time)//match
			{
				return mc_control_var[Gun_No].mc_timers[i].timer_tick_cnt;
			}
		}
	}
	return 0;		
}

//初始化CAN通信的文件描述符
int BMS_can_fd_init(unsigned char *can_id)
{
	struct ifreq ifr;
	struct sockaddr_can addr;
	int s;

	s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	if(s < 0) {
		perror("can socket");
		return (-1);
	}

	strcpy(ifr.ifr_name, can_id);
	if(ioctl(s, SIOCGIFINDEX, &ifr)){
		perror("can ioctl");
		return (-1);
	}

	addr.can_family = PF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
	if(bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("can bind");
		return (-1);
	}

	//原始套接字选项 CAN_RAW_LOOPBACK 为了满足众多应用程序的需要，本地回环功能默认是开启的, 但是在一些嵌入式应用场景中（比如只有一个用户在使用CAN总线），回环功能可以被关闭（各个套接字之间是独立的）
	int loopback = 0; // 0 = disabled, 1 = enabled (default)
	setsockopt(s, SOL_CAN_RAW, CAN_RAW_LOOPBACK, &loopback, sizeof(loopback));

	return (s);
}

static void MC_Read_Task(void)
{
	int fd;
	struct can_filter rfilter[2];

	int i = 0,Ret = 0,gun_config = 0;
	fd_set rfds;
	struct timeval tv;
	struct can_frame frame;
	int MC_no_data_cnt = 0;
	if((fd = BMS_can_fd_init(GUN_CAN_COM_ID1)) < 0){
		perror("BMS_Read_Task BMS_can_fd_init GUN_CAN_COM_ID1");
		while(1);
	}
		
	//<received_can_id> & mask == can_id
	rfilter[0].can_id   = 0x00008AF6;
	rfilter[0].can_mask = 0x0000FFFF;
	rfilter[1].can_id   = 0x00008AF7;
	rfilter[1].can_mask = 0x0000FFFF;

	if(setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter)) != 0){
		perror("setsockopt filter");
		while(1);
	}
	make_CRC32Table();//生成CRC校验码
	prctl(PR_SET_NAME,(unsigned long)"MC_Read_Task");//设置线程名字
  if((Globa->Charger_param.gun_config == 2)&&(Globa->Charger_param.support_double_gun_power_allocation ==1))
	{
		gun_config = 4;
	}else{
		gun_config = Globa->Charger_param.gun_config;//4;
	}
	
	while(1)
	{
		usleep(1000);//1ms

		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		tv.tv_sec = 0;
		tv.tv_usec = 10000;//10ms
		//printf("MC_Read_Task\n");
		Ret = select(fd + 1, &rfds, NULL, NULL, &tv);
		if(Ret < 0)
		{
			perror("can raw socket read ");
			usleep(100000);//100ms			
		}else if(Ret > 0)
		{
			MC_no_data_cnt = 0;
			if(FD_ISSET(fd, &rfds))
			{
			  Ret = read(fd, &frame, sizeof(frame));
			  if(Ret == sizeof(frame))
			  {
					if((frame.data[0] > 0) && (frame.data[0] <= gun_config))					
					{
						MC_read_deal(frame,frame.data[0]-1);//x号控制板发过来的数据帧处理
					}					
			  }
			}
		}else//==0
		{
			//printf("can no data recv \n");//test
			//usleep(100000);//100ms-----取消，避免can升级很慢?
			MC_no_data_cnt++;
			if(MC_no_data_cnt > 1000)//10秒钟无数据
			{
				MC_no_data_cnt = 0;
				AppLogOut("直流CAN口10秒内未接收到数据，重启CAN----");
				system("ifconfig can1 down");//直流CAN口重启---会不会有副作用
				sleep(1);
				system("ifconfig can1 up");
				sleep(1);
			}
		}
	}
	close(fd);
	exit(0);
}

/*********************************************************
**description：BMS读线程初始化及建立
***parameter  :none
***return		  :none
**********************************************************/
extern void MC_Read_Thread(void)
{
	pthread_t td1;
	int ret ,stacksize = 1024*1024;
	pthread_attr_t attr;

	ret = pthread_attr_init(&attr);
	if (ret != 0)
		return;

	ret = pthread_attr_setstacksize(&attr, stacksize);
	if(ret != 0)
		return;
	
	/* 创建自动串口抄收线程 */
	if(0 != pthread_create(&td1, &attr, (void *)MC_Read_Task, NULL)){
		perror("####pthread_create IC_Read_Write_Task ERROR ####\n\n");
	}

	ret = pthread_attr_destroy(&attr);
	if(ret != 0)
		return;
}



time_t Delay_Proce_Time[MODULE_INPUT_GROUP] = {0},Delay_Start_Time[MODULE_INPUT_GROUP] = {0},BMSProce_time[MODULE_INPUT_GROUP] = {0};
int gWorkState[MODULE_INPUT_GROUP] = {0};
int preModbule_Group_number[MODULE_INPUT_GROUP] = {0};
int preneed_model_num[MODULE_INPUT_GROUP] =  {0};
time_t Exit_Power_Keep_Time[MODULE_INPUT_GROUP] =  {0}; //退出等待时间
extern MODGROUP_DIAGRAM_LIST gGroup_Diagram_list[MODULE_INPUT_GROUP];//每个模块组的关系图

//修改为需求模块数量由屏幕计算。
//need_vol---需求电压V,系数0.1 5000表示500.0V
//need_cur---需求电流A,系数1  200表示200A
unsigned int Energy_ECO_num_trmp(unsigned short need_vol,unsigned short need_cur,unsigned char gun_no)
{
	unsigned int need_modbule_number = 1;
	unsigned int needallkw = 0;//需求总功率
	unsigned int cell_kw = 15000;  //单个功率,默认15K
	unsigned int need_cur_temp;//当需求电流大于该枪最大允许充电电流时，用最大充电电流计算模块数量,单位A
	need_cur_temp = need_cur/100;
	
	unsigned long hen_power = 0;//恒功率模块
	if((HUAWEI_MODULE == Globa->Charger_param.charge_modlue_factory)&&
		(GR95021 == Globa->Charger_param.charge_modlue_index))
		{
			hen_power = 1;
			cell_kw = 19950;//950*21
		}

	if( (INFY_MODULE == Globa->Charger_param.charge_modlue_factory)&&
		(REG75050 == Globa->Charger_param.charge_modlue_index))
		{
			hen_power = 1;
			cell_kw = 20000;//20kw
		}
	
	//if(Globa->Charger_param.Charge_rate_voltage  == 950000)
	if(hen_power)
	{
		if(need_cur_temp > Globa->Charger_param.current_limit[gun_no])//限制当枪的需求大于枪的最大输出时，用最大输出能力计算模块
			need_cur_temp = Globa->Charger_param.current_limit[gun_no];	
		//	need_modbule_number = (need_current*100)/((Globa.Huawei_Max_Current_Limt_Value*Globa.Charger_param.model_rate_current/1000)*MAX_MODEL_CURRENT_LIMINT);//以85%计算需要模块数
		needallkw = (need_vol/10)*(need_cur_temp);//w --需求总功率
		//利用模块电压、电流计算单个模块的可输出功率
		//cell_kw = (Globa->Charger_param.Charge_rate_voltage/1000)*((Globa->Charger_param.model_rate_current/1000));
		//注释掉上面一行，否则REG75050的单个模块功率就是750×50=37500w，超过了实际能力，实际750V的时候根本输出不到50A!!!
		need_modbule_number = needallkw*1000/(cell_kw*MAX_MODEL_CURRENT_LIMINT);
	}
	else
	{
		if(need_cur_temp > Globa->Charger_param.current_limit[gun_no])//限制当枪的需求大于枪的最大输出时，用最大输出能力计算模块
			need_cur_temp = Globa->Charger_param.current_limit[gun_no];
		need_modbule_number = need_cur_temp*10/(Globa->Charger_param.model_rate_current/1000);
		if(debug == 1) printf("need_cur_temp = %d\n",need_cur_temp);
		if(debug == 1) printf("Globa->Charger_param.model_rate_current/1000 = %d\n",Globa->Charger_param.model_rate_current/1000);
		if(debug == 1) printf("need_modbule_number = %d\n",need_modbule_number);
	}
	//上面将模块需求数量放大了10倍!	
	if((need_modbule_number%10) > 1)
	{
		need_modbule_number = (need_modbule_number/10) + 1;
	}
	else
	{
		need_modbule_number = (need_modbule_number/10);
	}	
  return  need_modbule_number;
}

//CCU控制
void CcuControl_Proces_Fun(int Ccu_no)
{
	Power_Proces_Fun(Ccu_no); //功率处理任务
	Charge_Proces_Fun(Ccu_no); //充电处理
}

//目前Ccu_no 和虚拟模块组号是一样的，因为模块是绑定在控制板上的
void Charge_Proces_Fun(int Ccu_no)
{
	int  Actual_State_no = 0,HSwitch_Mod_Num = 0,NSwitch_Mod_Num = 0,Gun_No = 0,HSwitch_Mod_ComoK_Num = 0,ls_Group_No = 0,i=0;
	time_t current_time = time(NULL);
	int  Mod_GroupNO = Ccu_no;//模块组号和CCU号是一起的，因为模块是挂在CCU上
  UINT32 SetVoltage =0,SetCurrent = 0,rContactor_Actual_State = 0;
	char tmp[100] = {0};
	switch (gWorkState[Ccu_no])
	{
		case CHARGE_PROCES_IDLE://空闲状态
		{
			Globa->Sys_Tcu2CcuControl_Data[Ccu_no].Module_Readyok = 0;
			if(Globa->Sys_RevCcu_Data[Ccu_no].Gunlink == 1)//枪连接 //枪连接号和本身处理函数一一对应关于
			{
				gWorkState[Ccu_no] = CHARGE_PROCES_PREPARE;
				Delay_Proce_Time[Ccu_no] = current_time;
				Delay_Start_falg[Ccu_no] = 1;
				ChargeProcesLogOut(Ccu_no,"%d#ChargeProces:从空闲阶段进入充电准备中Gunlink =%d",Ccu_no + 1,Globa->Sys_RevCcu_Data[Ccu_no].Gunlink);
			}
			break;
		}
		case CHARGE_PROCES_PREPARE://充电准备
		{
			if(Globa->Sys_RevCcu_Data[Ccu_no].Gunlink == 1)//枪连接
			{
				if((Globa->Sys_MODGroup_Control_Data[Mod_GroupNO].Sys_MobuleGrp_State > MOD_GROUP_STATE_ACTIVE)
					&&(Globa->Sys_MODGroup_Control_Data[Mod_GroupNO].Sys_MobuleGrp_Gun != Globa->Charger_gunbus_param_data[Mod_GroupNO].Bus_to_mod_Gno))//不是该组的
				{//等待被占用的功率退出
					Delay_Start_falg[Ccu_no] = 1;
					Delay_Start_Time[Ccu_no] = current_time;
				}else{
					if(Globa->Sys_MODGroup_Control_Data[Mod_GroupNO].Sys_MobuleGrp_State == MOD_GROUP_STATE_ACTIVE)
					{
						if(((Globa->Sys_RevCcu_Data[Ccu_no].Charge_State == 1))&&((Globa->Error_system[Ccu_no] == 0)))//充电开启,及无故障
						{
							if(Delay_Start_falg[Ccu_no] == 1 ){
								for(i=0;i<gGroup_Diagram_list[Mod_GroupNO].diagram_number;i++)//再次强制关闭其关联的继电器
								{
									Actual_State_no = Gets_contactor_control_unit_no_number(gGroup_Diagram_list[Mod_GroupNO].diagram_Contactor_No[i]);
									printf("\n %d 再次强制关闭其关联的继电器:-Actual_State_no=%0x 继电器号：%d  Mod_GroupNO =%d \n",i,Actual_State_no,gGroup_Diagram_list[Mod_GroupNO].diagram_Contactor_No[i],Mod_GroupNO);
									//Globa->Control_panel_Relay_Control[Actual_State_no&0xff] = 0;
									Globa->Sys_Tcu2CcuControl_Data[((Actual_State_no>>8)&0xff) -1].WeaverRelay_control = 0;
									Globa->Operated_Relay_NO_falg[gGroup_Diagram_list[Mod_GroupNO].diagram_Contactor_No[i] - 1] = 0;
								}
								Delay_Proce_Time[Ccu_no] = current_time;
								Delay_Start_falg[Ccu_no] = 0;
							}
							
							rContactor_Actual_State = 0;//查询是否有闭合的接触器。
							for(i=0;i<gGroup_Diagram_list[Mod_GroupNO].diagram_number;i++)//再次强制关闭其关联的继电器
							{
								Actual_State_no = Gets_contactor_control_unit_no_number(gGroup_Diagram_list[Mod_GroupNO].diagram_Contactor_No[i]);
                if(((((Globa->rContactor_Actual_State[((Actual_State_no>>8)&0xff) - 1] >> 2*(Actual_State_no&0xff))&0x01) == 0)&&(((Globa->rContactor_Actual_State[((Actual_State_no>>8)&0xff) - 1] >> (2*(Actual_State_no&0xff) + 1))&0x01) == 0 )))//防止模块通信还没有完全通上,额外加上一个相关联的接触器状态判断
								{
									
								}else{//查询到有闭合的情况。
									rContactor_Actual_State = 1;
									break;
								}
							}

							if((abs(current_time - Delay_Proce_Time[Ccu_no]) >= 3)&&(rContactor_Actual_State == 0))
							{
								Delay_Start_Time[Ccu_no] = current_time;
								gWorkState[Ccu_no] = CHARGE_PROCES_CHARGING;
								Globa->Sys_MODGroup_Control_Data[Mod_GroupNO].Sys_MobuleGrp_State = MOD_GROUP_STATE_USEING;
								HSwitch_Mod_Num = Globa->Charger_param.Charge_rate_number[Mod_GroupNO];//已经投切的模块组
								NSwitch_Mod_Num = 0;//需求投切模块数
								HSwitch_Mod_ComoK_Num = Globa->Charger_param.Charge_rate_number[Mod_GroupNO];
								
								memset(&gSwitch_ModGroup_list[Ccu_no].reserve, 0,sizeof(SWITCH_MODGROUP_LIST));

								gSwitch_ModGroup_list[Ccu_no].Modbule_Group_number = 1;//加入本组
								gSwitch_ModGroup_list[Ccu_no].Modbule_Group_Adrr[0] = Globa->Charger_gunbus_param_data[Mod_GroupNO].Bus_to_mod_Gno;
								gSwitch_ModGroup_list[Ccu_no].Contactor_No[0] = 0;
								gSwitch_ModGroup_list[Ccu_no].Line_Directory_Level[0] = 0;
						
								Globa->Sys_MODGroup_Control_Data[Mod_GroupNO].Control_Timeout_Count = 0;
								Globa->Sys_MODGroup_Control_Data[Mod_GroupNO].Control_Timeout_Falg = 0;
								Globa->Sys_Tcu2CcuControl_Data[Ccu_no].Module_Readyok = 1;
								preModbule_Group_number[Ccu_no] = 1;
								ChargeProcesLogOut(Ccu_no,"%d#ChargeProces:准备进入进入充电中 已经投切的模块数量 HSwitch_Mod_Num =%d ,",Ccu_no + 1,HSwitch_Mod_Num);
							}
						}
					}
				}
			}else{
				gWorkState[Ccu_no] = CHARGE_PROCES_IDLE;
				ChargeProcesLogOut(Ccu_no,"%d#ChargeProces:从充电准备中阶段退出到空闲阶段Gunlink =%d",Ccu_no + 1,Globa->Sys_RevCcu_Data[Ccu_no].Gunlink);
			}
			break;
		}
		
		case CHARGE_PROCES_CHARGING://充电中
		{
			if((Globa->Sys_RevCcu_Data[Ccu_no].Charge_State == 0)||(Globa->Error_system[Ccu_no] != 0))//充电结束
			{
				gWorkState[Ccu_no] = CHARGE_PROCES_END;
				
				for(i=0;i<gSwitch_ModGroup_list[Ccu_no].Modbule_Group_number;i++)
				{
					ls_Group_No = Gets_modbule_Group_No(gSwitch_ModGroup_list[Ccu_no].Modbule_Group_Adrr[i]);
					MsgChargeModuleOnOff(ls_Group_No,EM_RECTIFIER_ONOFF_OFF);//关闭该组模块
				}
				
				ChargeProcesLogOut(Ccu_no,"%d#ChargeProces:准备进入充电结束:块广播控制关模块",Ccu_no+1);
			}else{	
			 // printf("\n !!!!Ccu_no =%d 充电中---- Globa->Sys_RevCcu_Data[Ccu_no].Modbule_On =%d  %d %d \n",Ccu_no,Globa->Sys_RevCcu_Data[Ccu_no].Modbule_On,EM_RECTIFIER_ONOFF_OFF,EM_RECTIFIER_ONOFF_ON);
				if(Globa->Sys_RevCcu_Data[Ccu_no].Modbule_On == EM_RECTIFIER_ONOFF_OFF)//关机状态
				{
				//  ChargeProcesLogOut(Ccu_no,"%d#ChargeProces:块广播控制关模块",Ccu_no + 1);
					for(i=0;i<gSwitch_ModGroup_list[Ccu_no].Modbule_Group_number;i++)
					{
						ls_Group_No = Gets_modbule_Group_No(gSwitch_ModGroup_list[Ccu_no].Modbule_Group_Adrr[i]);
						MsgChargeModuleOnOff(ls_Group_No,EM_RECTIFIER_ONOFF_OFF);//关闭该组模块
					}
					
				}else if(Globa->Sys_RevCcu_Data[Ccu_no].Modbule_On == EM_RECTIFIER_ONOFF_ON)
				{
					//查询已经加入该组的模块组是否有异常的情况
					preModbule_Group_number[Ccu_no] = gSwitch_ModGroup_list[Ccu_no].Modbule_Group_number;
					if(gSwitch_ModGroup_list[Ccu_no].Modbule_Group_number !=0 ){
						if(abs(current_time - Delay_Start_Time[Ccu_no]) >= 1){
							Delay_Start_Time[Ccu_no] = current_time;
							Already_invested_module_detection(Ccu_no,&gSwitch_ModGroup_list[Ccu_no].reserve);  //有异常的需要剔除并且从新排列出来。
							
							if(gSwitch_ModGroup_list[Ccu_no].Modbule_Group_number != preModbule_Group_number[Ccu_no])//表示已经申请到功率，则退出之后需要2分钟之后退出
							{
								ChargeProcesLogOut(Ccu_no,"%d#ChargeProces: 检测到有异常需要退出模块组 模块组数：原先组数 =%d, 退出之后组数 =%d,",Ccu_no + 1,preModbule_Group_number[Ccu_no],gSwitch_ModGroup_list[Ccu_no].Modbule_Group_number);
							}
							if(CHARGE_PROCES_DEBUG) printf("检测已经投入的组是否有异常:end :gSwitch_ModGroup_list[Ccu_no].Modbule_Group_number[Ccu_no = %d] =%d \n",Ccu_no,gSwitch_ModGroup_list[Ccu_no].Modbule_Group_number);		
						}
					}

					if(preneed_model_num[Ccu_no]!= Globa->Sys_RevCcu_Data[Ccu_no].need_model_num)
					{
						preneed_model_num[Ccu_no] = Globa->Sys_RevCcu_Data[Ccu_no].need_model_num;
						BMSProce_time[Ccu_no] = current_time - 5;//5s调节一次
					}
					
					if(gSwitch_ModGroup_list[Ccu_no].Modbule_Group_number != preModbule_Group_number[Ccu_no]){
						BMSProce_time[Ccu_no] = current_time - 1;//2s调节一次
						preModbule_Group_number[Ccu_no] = gSwitch_ModGroup_list[Ccu_no].Modbule_Group_number;
					}
	
					if(abs(current_time - BMSProce_time[Ccu_no]) >= 4)
					{
						HSwitch_Mod_ComoK_Num = 0;
						for(i =0;i<gSwitch_ModGroup_list[Ccu_no].Modbule_Group_number;i++)
						{
							ls_Group_No = Gets_modbule_Group_No(gSwitch_ModGroup_list[Ccu_no].Modbule_Group_Adrr[i]);
							HSwitch_Mod_ComoK_Num += Globa->Sys_RevCcu_Data[ls_Group_No].Charger_good_num;
						 if(CHARGE_PROCES_DEBUG) printf("[i = %d] ls_Group_No=%d  HSwitch_Mod_ComoK_Num =%d  Modbule_Group_Adrr =%d \n",i,ls_Group_No,HSwitch_Mod_ComoK_Num,gSwitch_ModGroup_list[Ccu_no].Modbule_Group_Adrr[i]);		

						}

						
						if(CHARGE_PROCES_DEBUG) printf("[%d] HSwitch_Mod_ComoK_Num =%d gSwitch_ModGroup_list[Ccu_no].Modbule_Group_number =%d \n",Ccu_no,HSwitch_Mod_ComoK_Num,gSwitch_ModGroup_list[Ccu_no].Modbule_Group_number);		
						//把本身组排除掉了，
						NSwitch_Mod_Num = Globa->Sys_RevCcu_Data[Ccu_no].need_model_num;//根据需求电压电流，获取当前的总需要模块数 
						if(CHARGE_PROCES_DEBUG) printf("[%d] NSwitch_Mod_Num =%d\n",Ccu_no,NSwitch_Mod_Num);		

						//根据模块通讯正常的数据来进行对比
						preModbule_Group_number[Ccu_no] = gSwitch_ModGroup_list[Ccu_no].Modbule_Group_number;//申请功率前进行辅助

						if(NSwitch_Mod_Num > HSwitch_Mod_ComoK_Num)//提交功率//是否需要把好的模块计算过去，HSwitch_Mod_ComoK_Num 
						{
							Allocation_Power_Applied_Function(Ccu_no,(NSwitch_Mod_Num - HSwitch_Mod_ComoK_Num),&gSwitch_ModGroup_list[Ccu_no].reserve);
							if(gSwitch_ModGroup_list[Ccu_no].Modbule_Group_number > preModbule_Group_number[Ccu_no])//表示已经申请到功率，则退出之后需要2分钟之后退出
							{
								Exit_Power_Keep_Time[Ccu_no] = current_time;
								for(i =0;i<gSwitch_ModGroup_list[Ccu_no].Modbule_Group_number;i++)
								{
									sprintf(&tmp[strlen(tmp)],"[%d]",gSwitch_ModGroup_list[Ccu_no].Modbule_Group_Adrr[i]);
						    }
								ChargeProcesLogOut(Ccu_no,"%d#ChargeProces: 申请到功率了 模块组数：原先组数 =%d, 申请之后组数 =%d [%s], 总需求 NSwitch_Mod_Num =%d：原先已经投切通信正常数：HSwitch_Mod_ComoK_Num =%d 需求模块个数 =%d ",Ccu_no + 1,preModbule_Group_number[Ccu_no],gSwitch_ModGroup_list[Ccu_no].Modbule_Group_number,tmp,NSwitch_Mod_Num,HSwitch_Mod_ComoK_Num,preneed_model_num[Ccu_no]);
							}
						}else if((NSwitch_Mod_Num < HSwitch_Mod_ComoK_Num)&&(HSwitch_Mod_ComoK_Num > Globa->Charger_param.Charge_rate_number[Mod_GroupNO]))//减少功率需求,并且已经投切过来的组数模块需要大于本身组的才可以进行
						{			
							if(abs(current_time - Exit_Power_Keep_Time[Ccu_no]) >= 120){//超过2分钟才退出功率
								Allocation_Power_Exit_Function(Ccu_no,(HSwitch_Mod_ComoK_Num - NSwitch_Mod_Num),&gSwitch_ModGroup_list[Ccu_no].reserve);
								if(gSwitch_ModGroup_list[Ccu_no].Modbule_Group_number != preModbule_Group_number[Ccu_no])//表示已经申请到功率，则退出之后需要2分钟之后退出
								{
									Exit_Power_Keep_Time[Ccu_no] = current_time;
									ChargeProcesLogOut(Ccu_no,"%d#ChargeProces: 退出多余的功率 模块组数：原先组数 =%d, 退出之后组数 =%d, 总需求 NSwitch_Mod_Num =%d：目前已经投切通信正常数：HSwitch_Mod_ComoK_Num =%d 需求模块个数 =%d ",Ccu_no + 1,preModbule_Group_number[Ccu_no],gSwitch_ModGroup_list[Ccu_no].Modbule_Group_number,NSwitch_Mod_Num,HSwitch_Mod_ComoK_Num,preneed_model_num[Ccu_no]);
								}
							}
						}
												//也可以充这个列表中获取参数(by)。
						for(i=0;i<gSwitch_ModGroup_list[Ccu_no].Modbule_Group_number;i++)
						{
							ls_Group_No = Gets_modbule_Group_No(gSwitch_ModGroup_list[Ccu_no].Modbule_Group_Adrr[i]);

							if(Globa->Sys_MODGroup_Control_Data[ls_Group_No].Sys_MobuleGrp_State == MOD_GROUP_STATE_ALLOCATED_OK)//已分配完成的组置位使用状态
							{
								Globa->Sys_MODGroup_Control_Data[ls_Group_No].Sys_MobuleGrp_State = MOD_GROUP_STATE_USEING;
							}
							
						  if(CHARGE_PROCES_DEBUG)printf("i=%d ls_Group_No =%d  Sys_MobuleGrp_State =%d ",i,ls_Group_No,Globa->Sys_MODGroup_Control_Data[ls_Group_No].Sys_MobuleGrp_State);
							//分配电压电流。
							if(Globa->Sys_MODGroup_Control_Data[ls_Group_No].Sys_MobuleGrp_State == MOD_GROUP_STATE_USEING)
							{
							  SetVoltage = Globa->Sys_RevCcu_Data[Ccu_no].Need_Voltage;
								SetCurrent = (Globa->Sys_RevCcu_Data[ls_Group_No].Charger_good_num*Globa->Sys_RevCcu_Data[Ccu_no].Need_Current)/HSwitch_Mod_ComoK_Num;
                MsgChargeModuleGroupBroadcastOutSetVA(SetVoltage,SetCurrent,ls_Group_No);
							 if(CHARGE_PROCES_DEBUG) printf("i=%d SetVoltage =%d  SetCurrent =%d HSwitch_Mod_ComoK_Num =%d Need_Current =%d Charger_good_num =%d \n",i,SetVoltage,SetCurrent,HSwitch_Mod_ComoK_Num,Globa->Sys_RevCcu_Data[Ccu_no].Need_Current,Globa->Sys_RevCcu_Data[ls_Group_No].Charger_good_num);

								
							}
						}
				  }
			  }
			}
			break;
		}
		case CHARGE_PROCES_END://充电结束
		{
			for(i=0;i<gSwitch_ModGroup_list[Ccu_no].Modbule_Group_number;i++)
			{
				ls_Group_No = Gets_modbule_Group_No(gSwitch_ModGroup_list[Ccu_no].Modbule_Group_Adrr[i]);
        MsgChargeModuleOnOff(EM_RECTIFIER_ONOFF_OFF,ls_Group_No);//关闭该组模块
			}
			
			Allocation_Power_Exit_full_Function(Ccu_no,&gSwitch_ModGroup_list[Ccu_no].reserve);//全部退出功率
			gWorkState[Ccu_no] = CHARGE_PROCES_IDLE;//进入空闲阶段
			memset(&gSwitch_ModGroup_list[Ccu_no].reserve, 0,sizeof(SWITCH_MODGROUP_LIST));
			Globa->Sys_MODGroup_Control_Data[Ccu_no].Sys_MobuleGrp_State = MOD_GROUP_STATE_ACTIVE;
			for(i=0;i<gGroup_Diagram_list[Mod_GroupNO].diagram_number;i++)
			{
				Globa->Operated_Relay_NO_falg[gGroup_Diagram_list[Mod_GroupNO].diagram_Contactor_No[i] - 1] = 0;
				Globa->Operated_Relay_NO_Fault_Count[gGroup_Diagram_list[Mod_GroupNO].diagram_Contactor_No[i] - 1] = 0;
			}
			break;
		}
		default:
			break;
	}
}


/*********************************************************
**description：MC管理线程
***parameter  :none
***return		  :none
**********************************************************/
static void MC_Manage_Task(void* gun_no)
{	
	INT32 fd;
	UINT32 temp1=0xff,temp2=0xff,i=0;
	UINT32 Error_equal = 0,ctr_connect_count = 0,Power_Adjust_data = 0,ALL_KW = 0;
	UINT32  AC_Relay_Ctl_Count = 0,AC_Relay_ctl_st=RELAY_CTL_ST_WAIT_PLUG_IN;	
	UINT8 temp_char[32]; 
	UINT32 pre_SYS_Step,pre_MC_Step;
	UINT32 Gun_No = *((UINT32*)gun_no);//值0---4
	if(Gun_No+1 > CONTROL_DC_NUMBER)
		return ;
	MC_Init_timers(Gun_No);
	MC_timer_start(Gun_No,5002);//遥测帧定时
	MC_timer_start(Gun_No,400);
	MC_timer_start(Gun_No,150);

	Globa->Control_DC_Infor_Data[Gun_No].charger_start = 0;//充电停止
	Globa->Control_DC_Infor_Data[Gun_No].Time = 0;//已充电时长
	Globa->Control_DC_Infor_Data[Gun_No].SYS_Step = 0x00;

	if((fd = BMS_can_fd_init(GUN_CAN_COM_ID1)) < 0)
	{
		while(1);
	}
	sprintf(temp_char,"MC%d_Mange_Task",Gun_No+1);
	prctl(PR_SET_NAME,(unsigned long)temp_char);//设置线程名字

	//在指定的CAN_RAW套接字上禁用接收过滤规则,则该线程只发送，不会接收数据，省内存和CPU时间
	setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FILTER, NULL, 0);
  
	Globa->Sys_MODGroup_Control_Data[Gun_No].Sys_MobuleGrp_Gun = Globa->Charger_gunbus_param_data[Gun_No].Bus_to_mod_Gno;//初始时各自赋值本身组号
	Globa->Sys_MODGroup_Control_Data[Gun_No].Sys_MobuleGrp_State = MOD_GROUP_STATE_INACTIVE;

	while(1)
	{
		usleep(10000);//做基准时钟调度10ms
		mc_control_var[Gun_No].MC_Manage_Watchdog = 1;
		MC_timer_run(Gun_No);

		if(pre_SYS_Step != Globa->Control_DC_Infor_Data[Gun_No].SYS_Step)
		{
			printf("44444xxxxxx Plug%d MC_SYS_Step=0x%x\n",Gun_No,Globa->Control_DC_Infor_Data[Gun_No].SYS_Step);
			pre_SYS_Step = Globa->Control_DC_Infor_Data[Gun_No].SYS_Step;
		}
		if(pre_MC_Step != Globa->Control_DC_Infor_Data[Gun_No].MC_Step)
		{
			printf("55555xxxxxx Plug%d MC_Step=0x%x\n",Gun_No,Globa->Control_DC_Infor_Data[Gun_No].MC_Step);
			pre_MC_Step = Globa->Control_DC_Infor_Data[Gun_No].MC_Step;
		}
		
		
		if(Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_setflg == 1)
		{ //进行升级
		//	printf("44444xxxxxx Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_setStep = %0x\n",Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_setStep);

			switch(Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_setStep)
			{
				  case 0x00:
				  {//第x块板子进行升级
						if(Gun_No > 0)
						{
							if(Globa->Control_Upgrade_Firmware_Info[Gun_No-1].firmware_info_setStep == 0x10)//前1个板子已升级
							{//第x块板更新完毕
								if(MC_All_Frame[Gun_No].MC_Recved_PGN21248_Flag == 1)
								{//升级请求报文响应报文
									MC_All_Frame[Gun_No].MC_Recved_PGN21248_Flag = 0;
									MC_timer_start(Gun_No,5000);
									Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_setStep = 0x01;
									Globa->Control_Upgrade_Firmware_Info[Gun_No].Send_Tran_All_Bytes_0K = 0; //重新计数
									break;
								}
								if(MC_timer_flag(Gun_No,5000) == 1)
								{//遥测响应帧 超过5秒
									MC_timer_stop(Gun_No,5000);
									Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_setStep = 0x10;
									Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_success = 1;
									break;
								}
								if(MC_timer_flag(Gun_No,250) == 1)
								{//下发升级请求报文
									MC_timer_start(Gun_No,250);
									MC_sent_data(fd, PGN20992_ID_MC, 1, Gun_No+1);  //下发升级请求报文
									Globa->Control_Upgrade_Firmware_Info[Gun_No].Send_Tran_All_Bytes_0K = 0; //重新计数
									Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_success = 0;
								} 
							}else
							{
								MC_timer_start(Gun_No,250);
								MC_timer_start(Gun_No,5000);
							}
						}else//第1号板子
						{
							if(MC_All_Frame[0].MC_Recved_PGN21248_Flag == 1)
							{//升级请求报文响应报文
								MC_All_Frame[0].MC_Recved_PGN21248_Flag = 0;
								MC_timer_start(0,5000);
								Globa->Control_Upgrade_Firmware_Info[0].firmware_info_setStep = 0x01;
								Globa->Control_Upgrade_Firmware_Info[0].Send_Tran_All_Bytes_0K = 0; //重新计数
								break;
							}
							if(MC_timer_flag(0,5000) == 1)
							{//遥测响应帧 超过5秒
								MC_timer_stop(0,5000);
								Globa->Control_Upgrade_Firmware_Info[0].firmware_info_setStep = 0x10;
								Globa->Control_Upgrade_Firmware_Info[0].firmware_info_success = 1;
								break;
							}
							if(MC_timer_flag(0,250) == 1)
							{//下发升级请求报文
								MC_timer_start(0,250);
								MC_sent_data(fd, PGN20992_ID_MC, 1, 1);  //下发升级请求报文
								Globa->Control_Upgrade_Firmware_Info[0].Send_Tran_All_Bytes_0K = 0; //重新计数
								Globa->Control_Upgrade_Firmware_Info[0].firmware_info_success = 0;
							}
						}
						break;
					}
					case 0x01:
					{
						if(MC_All_Frame[Gun_No].MC_Recved_PGN21504_Flag == 1){ //请求升级数据报文帧
							MC_All_Frame[Gun_No].MC_Recved_PGN21504_Flag = 0;
							MC_timer_start(Gun_No,5000);
							MC_timer_start(Gun_No,250);
							MC_sent_data(fd, PGN21760_ID_MC, 0, Gun_No+1);
						}
						if(MC_All_Frame[Gun_No].MC_Recved_PGN22016_Flag == 1){ //请求升级数据报文帧
							MC_All_Frame[Gun_No].MC_Recved_PGN22016_Flag = 0;
							MC_timer_start(Gun_No,5000);
							MC_sent_data(fd, PGN21760_ID_MC, 1, Gun_No+1);
							Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_setStep = 0x02;
							MC_timer_start(Gun_No,250);
						}
						if(MC_timer_flag(Gun_No,5000) == 1){//遥测响应帧 超过5秒
							MC_timer_start(Gun_No,5000);
							Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_setStep = 0x10;
							Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_success = 1;
							break;
						}
						break;
					}
					case 0x02:{//升级包文件下发
						if(MC_All_Frame[Gun_No].MC_Recved_PGN22016_Flag == 1){ //请求升级数据报文帧
							MC_All_Frame[Gun_No].MC_Recved_PGN22016_Flag = 0;
							MC_timer_start(Gun_No,5000);
							MC_timer_start(Gun_No,250);
							MC_sent_data(fd, PGN21760_ID_MC, 1, Gun_No+1);
						}
						if(MC_timer_flag(Gun_No,5000) == 1){//遥测响应帧 超过5秒
							MC_timer_stop(Gun_No,5000);
							Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_setStep = 0x10;
							Globa->Control_Upgrade_Firmware_Info[Gun_No].firmware_info_success = 1;
							break;
						}else if(MC_timer_flag(Gun_No,250) == 1){//如果下面控制板还没有
							MC_timer_start(Gun_No,250);
							MC_sent_data(fd, PGN21760_ID_MC, 1, Gun_No+1);
							break;
						}
						break;
					}
					case 0x10:{//升级完毕，不管是失败还是成功

						break;
					}
				}
		}else//非升级中
		{
			if(((Globa->Control_DC_Infor_Data[Gun_No].Error_ctl == 0x00)&&(Globa->Control_DC_Infor_Data[Gun_No].ctl_state == 0x00))	||
				((Globa->Control_DC_Infor_Data[Gun_No].Error_ctl != 0x00)&&(Globa->Control_DC_Infor_Data[Gun_No].ctl_state == 0x01))
				)
				{//判断计量单元统计的控制器的故障数量是否与控制器的状态一致
					Error_equal = 0;
					MC_timer_stop(Gun_No,5001);
				}else
				{
					if(Error_equal == 0)
					{
						Error_equal = 1;
						MC_timer_start(Gun_No,5001);
					}
				}
				
  		  if(Globa->Charger_param.Power_Contor_By_Mode == 1)
			  {
					if(MC_timer_flag(Gun_No,400) == 1)//定时400ms发送一轮模块输出信息帧
					{
						MC_sent_data(fd, PGN16512_ID_PC, 1, Gun_No+1);
						MC_timer_start(Gun_No,400);
					}				
					CcuControl_Proces_Fun(Gun_No);
				}
				
				if(MC_timer_flag(Gun_No,5001) == 1)
				{//超时5秒
				//	memset(&Globa->Control_DC_Infor_Data[Gun_No].Error.emergency_switch, 0x00, (&Globa->Control_DC_Infor_Data[Gun_No].Error.reserve30 - &Globa->Control_DC_Infor_Data[Gun_No].Error.emergency_switch));//为了保持与计量单元的故障记录计数同步，把标志全部清零
					//Globa->Error_ctl = 0;

				//	system("echo Globa->Control_DC_Infor_Data[Gun_No].Error_ctl != 0x00)&&(Globa->Control_DC_Infor_Data[Gun_No].ctl_state != 0x11)) >> /mnt/systemlog");
					//sleep(3);
					//system("reboot");
				}

				if((MC_All_Frame[Gun_No].MC_Recved_PGN8704_Flag == 0x01)||
				   (MC_All_Frame[Gun_No].MC_Recved_PGN8448_Flag == 1)
				  )
				{//遥测帧
					if(MC_All_Frame[Gun_No].MC_Recved_PGN8704_Flag == 1)
					{
						MC_All_Frame[Gun_No].MC_Recved_PGN8704_Flag = 0;
						MC_sent_data(fd, PGN8960_ID_MC, 1, Gun_No+1);  //遥测响应帧
			    
  					if(Globa->Charger_param.Power_Contor_By_Mode == 1)
						{
							 if((Globa->Control_DC_Infor_Data[Gun_No].line_v > 32)&&(Globa->Control_DC_Infor_Data[Gun_No].line_v < 48)){
								Globa->Control_DC_Infor_Data[Gun_No].gun_link = 1;
							}else
							{
								Globa->Control_DC_Infor_Data[Gun_No].gun_link = 0;    		
							} 
					  }					
					}
					MC_All_Frame[Gun_No].MC_Recved_PGN8448_Flag = 0;
					MC_timer_start(Gun_No,5002);
					ctr_connect_count = 0;
					if(Globa->Control_DC_Infor_Data[Gun_No].Error.ctrl_connect_lost != 0)
					{
						Globa->Control_DC_Infor_Data[Gun_No].Error.ctrl_connect_lost = 0;
						sent_warning_message(0x98, 53, Gun_No+1, 0);
						Globa->Control_DC_Infor_Data[Gun_No].Error_system--;
					}
				}else if(MC_timer_flag(Gun_No,5002) == 1)
				{//超时5秒
					ctr_connect_count++;
					MC_timer_start(Gun_No,5002);
					if(ctr_connect_count >= 10)//TEST2)
					{
						if(Globa->Control_DC_Infor_Data[Gun_No].Error.ctrl_connect_lost != 1)
						{
							Globa->Control_DC_Infor_Data[Gun_No].Error.ctrl_connect_lost = 1;
							sent_warning_message(0x99, 53, Gun_No+1, 0);
							Globa->Control_DC_Infor_Data[Gun_No].Error_system++;
						}
						ctr_connect_count = 0;
					}
				}
				//test
				//printf("MC_timer_cur_tick(%d,5002)=%d\n",Gun_No,MC_timer_cur_tick(Gun_No,5002));
				
				if(MC_All_Frame[Gun_No].MC_Recved_PGN2816_Flag == 0x01)
				{//请求充电参数信息
					MC_All_Frame[Gun_No].MC_Recved_PGN2816_Flag = 0;

					MC_sent_data(fd, PGN2304_ID_MC, 1, Gun_No+1);  //下发充电参数信息
					MC_All_Frame[Gun_No].Power_kw_percent = 0;//重新下发下功率控制
				}
				
				if(Globa->Charger_param.Power_Contor_By_Mode == 1)
				{

					if(INPUT_RELAY_CTL_MODULE == Globa->Charger_param.Input_Relay_Ctl_Type)
					{
						switch(AC_Relay_ctl_st)
						{
							case RELAY_CTL_ST_WAIT_PLUG_IN:
								if(Globa->Control_DC_Infor_Data[Gun_No].gun_link == 1)//重新插枪了
								{
									AC_Relay_Ctl_Count = 0;
									Globa->Control_DC_Infor_Data[Gun_No].AC_Relay_control = 1;//请求合节能继电器
									AC_Relay_ctl_st = RELAY_CTL_ST_WAIT_PLUG_CHG;
								}							
								break;
							case RELAY_CTL_ST_WAIT_PLUG_CHG:
								if(Globa->Control_DC_Infor_Data[Gun_No].gun_link == 0)//拔枪了延迟关模块
								{
									AC_Relay_ctl_st = RELAY_CTL_ST_WAIT_PLUG_IN;
									AC_Relay_Ctl_Count = 0;//没充电拔枪了，直接关
									Globa->Control_DC_Infor_Data[Gun_No].AC_Relay_control = 0;
								}else//持续插枪中
								{
									if(0 != Globa->Control_DC_Infor_Data[Gun_No].charger_start)//启动充电了--自动和手动
									{
										AC_Relay_ctl_st = RELAY_CTL_ST_WAIT_CHG_END;
										AC_Relay_Ctl_Count = 0;
										Globa->Control_DC_Infor_Data[Gun_No].AC_Relay_control = 1;//请求合节能继电器
									}
									else
									{
										AC_Relay_Ctl_Count++;
										//if(AC_Relay_Ctl_Count >= 60000)//60000*10ms约600秒 10分钟600秒
										if(AC_Relay_Ctl_Count >= 9000)//90秒
										{
											//10分钟没充电										
											Globa->Control_DC_Infor_Data[Gun_No].AC_Relay_control = 0;
											AC_Relay_Ctl_Count = 0;
											AC_Relay_ctl_st = RELAY_CTL_ST_WAIT_PLUG_OUT;//超时不充电已申请关闭模块	
										}
									}
								}							
							break;
							
							
							case RELAY_CTL_ST_WAIT_CHG_END://充电中，等待充电结束
								if(0==Globa->Control_DC_Infor_Data[Gun_No].charger_start)//充电结束
								{
									AC_Relay_ctl_st = RELAY_CTL_ST_CHG_END_DELAY;
									AC_Relay_Ctl_Count = 0;	
									Globa->Control_DC_Infor_Data[Gun_No].Over_gun_link = Globa->Control_DC_Infor_Data[Gun_No].gun_link;//记住结束时的插枪状态
								}
							break;
							
							case RELAY_CTL_ST_CHG_END_DELAY://充电结束延迟关闭模块
							{
								if(Globa->Control_DC_Infor_Data[Gun_No].gun_link != Globa->Control_DC_Infor_Data[Gun_No].Over_gun_link)//状态改变
								{								
									Globa->Control_DC_Infor_Data[Gun_No].Over_gun_link = Globa->Control_DC_Infor_Data[Gun_No].gun_link;
									if(Globa->Control_DC_Infor_Data[Gun_No].gun_link == 0)//等待期间拔枪了,计时接着走，不跳出
									{
										
									}
									if(Globa->Control_DC_Infor_Data[Gun_No].gun_link == 1)//等待期间重新插枪了，取消计时,不关模块了
									{
										AC_Relay_Ctl_Count = 0;
										AC_Relay_ctl_st = RELAY_CTL_ST_WAIT_PLUG_IN;
										break;
									}
								}
								
								AC_Relay_Ctl_Count++;
								//if(AC_Relay_Ctl_Count>= 3000)//3000*10ms约30秒
								if(AC_Relay_Ctl_Count>= 6000)
								{
									AC_Relay_Ctl_Count = 0;								
									if(0==Globa->Control_DC_Infor_Data[Gun_No].charger_start)
									{
										Globa->Control_DC_Infor_Data[Gun_No].AC_Relay_control = 0;
										AC_Relay_ctl_st = RELAY_CTL_ST_WAIT_PLUG_OUT;//充电结束后关闭模块电源了，需拔枪后再次插枪
									}else//没拔枪又启动充电了
									{
										Globa->Control_DC_Infor_Data[Gun_No].AC_Relay_control = 1;
										AC_Relay_ctl_st = RELAY_CTL_ST_WAIT_CHG_END;			
                    AC_Relay_Ctl_Count = 0;										
									}
								}
							}
							break;
								
							case RELAY_CTL_ST_WAIT_PLUG_OUT://充电结束或超时不充电等待拔枪后重新来
							{
								if(Globa->Control_DC_Infor_Data[Gun_No].gun_link == 0)
									AC_Relay_ctl_st = RELAY_CTL_ST_WAIT_PLUG_IN;
								//注释掉下面几行，防止充电结束后界面没跳转首页时，一直不关模块
								// if((Globa->QT_Step == 0x03) || (Globa->QT_Step == 0x04 ))//没拔枪继续操作界面，则启动							
								// {
									// AC_Relay_ctl_st = RELAY_CTL_ST_WAIT_PLUG_IN;								
								// }
								if(0 !=Globa->Control_DC_Infor_Data[Gun_No].charger_start)//如果又充电了，如APP启动,自动和手动
								{
									AC_Relay_ctl_st = RELAY_CTL_ST_WAIT_CHG_END;
									AC_Relay_Ctl_Count = 0;
									Globa->Control_DC_Infor_Data[Gun_No].AC_Relay_control = 1;
								}
							}
							break;
							
							default:
								AC_Relay_ctl_st = RELAY_CTL_ST_WAIT_PLUG_IN;
								AC_Relay_Ctl_Count = 0;								
								break;
						}				
						
					}else if(INPUT_RELAY_CTL_FAN == Globa->Charger_param.Input_Relay_Ctl_Type)	//控制风机
					{
						Globa->Control_DC_Infor_Data[Gun_No].AC_Relay_control = Globa->Control_DC_Infor_Data[Gun_No].gun_link;//请求合节能继电器
						if(Globa->Control_DC_Infor_Data[Gun_No].charger_start == 1)
						{  //启动之后才开风扇
							Globa->Control_DC_Infor_Data[Gun_No].AC_Relay_control = 1;
							AC_Relay_Ctl_Count = 0;
						}else//计数延迟关闭风扇或模块，如果延时时间内又开启充电了，那就不关风扇
						{
							AC_Relay_Ctl_Count++;
							if(AC_Relay_Ctl_Count>= 3000)//3000*10ms约30秒
							{
								AC_Relay_Ctl_Count = 3000;
								Globa->Control_DC_Infor_Data[Gun_No].AC_Relay_control = 0;
							}
						}
					}else {
						Globa->Control_DC_Infor_Data[Gun_No].AC_Relay_control = Globa->Control_DC_Infor_Data[Gun_No].gun_link;//请求合节能继电器
					}
				}
				
				if(Globa->Control_DC_Infor_Data[Gun_No].Operation_command == 2)
				{//进行解锁动作
					if(MC_All_Frame[Gun_No].MC_Recved_PGN3840_Flag == 1)
					{ //收到下发电子锁控制反馈
						MC_timer_stop(Gun_No,650);
						MC_All_Frame[Gun_No].MC_Recved_PGN3840_Flag = 0;
						Globa->Control_DC_Infor_Data[Gun_No].Operation_command = 0;
					}else
					{
						if(MC_timer_flag(Gun_No,650) == 1)
						{  //定时650MS
							MC_sent_data(fd, PGN3584_ID_MC, 2, Gun_No+1);  //下发充电参数信息
							MC_timer_start(Gun_No,650);
						}
					}
				}else
				{
					MC_timer_start(Gun_No,650);
				}
				//------------------功率限制（暂时关闭功率控制）-------------------
#ifdef KW_LIMIT					 
				if(Globa->Control_DC_Infor_Data[Gun_No].kw_percent != 1000)
				{
					if(IS_Power_Limit_Unlocking(&Globa->Control_DC_Infor_Data[Gun_No].kw_limit_end_time[0]) == 1){
						Globa->Control_DC_Infor_Data[Gun_No].kw_percent = 1000; //100.0%
						RUN_GUN_ControlLogOut(Gun_No,"%d #功率限制时间达到 功率控制值 =%d ",Gun_No+1,Globa->Control_DC_Infor_Data[Gun_No].kw_percent);
					}
				}
					
				if(Globa->Control_DC_Infor_Data[Gun_No].kw_percent != MC_All_Frame[Gun_No].Power_kw_percent)
				{//进行功率限制
					if(MC_All_Frame[Gun_No].MC_Recved_Power_PGN4096_Flag == 1)
					{ 
					//	MC_timer_stop(Gun_No,150);
						MC_All_Frame[Gun_No].MC_Recved_Power_PGN4096_Flag = 0;
						MC_All_Frame[Gun_No].Power_kw_percent = Globa->Control_DC_Infor_Data[Gun_No].kw_percent;
						// printf("\n-----------------------收到下发进行功率限制------------- --\n");
						PGN3840_ID_MC_SentwaitCount[Gun_No] = 0;
					}
					else
					{
						if(MC_timer_flag(Gun_No,150) == 1)
						{  //定时650MS
					
					    if(PGN3840_ID_MC_SentwaitCount[Gun_No] == 0){
								MC_All_Frame[Gun_No].Power_Adjust_Command = 1;//直接用功率绝对值
								
								int cell_w =  20000;//20	kw 
								if(((Globa->Charger_param.model_rate_current/1000) == 21)&&((Globa->Charger_param.Charge_rate_voltage/1000) == 950))
								{
									cell_w =  20000;//20	kw 
								}else {
									cell_w = ((Globa->Charger_param.model_rate_current/1000)*(Globa->Charger_param.Charge_rate_voltage/1000));//w
								}
							
								if(MC_All_Frame[Gun_No].Power_Adjust_Command == 1){
									if(Globa->Charger_param.Power_Contor_By_Mode != 1)
									{
									  ALL_KW = 0;
										for(i=0;i < Globa->Charger_param.gun_config;i++)
										{
										
											ALL_KW += (Globa->Charger_param.Charge_rate_number[i]*cell_w;//w
										}
									}
									else
									{
										ALL_KW = 0;
										for(i=0;i < Globa->Charger_param.gun_config;i++)
										{
									    if(Globa->Sys_RevCcu_Data[i].Charger_good_num == 0){
												  
  										   ALL_KW += (Globa->Charger_param.Charge_rate_number[i]*cell_w);//kw
											}else {
										  	ALL_KW += (Globa->Sys_RevCcu_Data[i].Charger_good_num*cell_w);//kw
											}
										}
									} 				
							    ALL_KW	= ALL_KW /1000;	//kw	
                  if(ALL_KW == 160)	  
										ALL_KW = 150;   //160KW的桩平台登记为150kw 								
									Power_Adjust_data = (10000 + (Globa->Control_DC_Infor_Data[Gun_No].kw_percent*ALL_KW)/100); //0.1kw
								}else{
								  Power_Adjust_data = Globa->Control_DC_Infor_Data[Gun_No].kw_percent;
								}
								
							//	printf("\n-------------------------Power_Adjust_data =%d--------------Gun_No =%d   --\n",Power_Adjust_data,Gun_No);

								MC_sent_data(fd, PGN3840_ID_MC,Power_Adjust_data, Gun_No+1);  //下发充电参数信息
							}
							PGN3840_ID_MC_SentwaitCount[Gun_No]++;
							if(PGN3840_ID_MC_SentwaitCount[Gun_No] >= 6){
								PGN3840_ID_MC_SentwaitCount[Gun_No] = 0;
							}
							MC_timer_start(Gun_No,150);
						}
					}
				}
				else
				{
					PGN3840_ID_MC_SentwaitCount[Gun_No] = 0;
					MC_timer_start(Gun_No,150);
					MC_All_Frame[Gun_No].MC_Recved_Power_PGN4096_Flag = 0;
				}
				
#endif 	
			
				switch(Globa->Control_DC_Infor_Data[Gun_No].SYS_Step)
				{
					case 0x00:{//---------------------------系统空闲状态00 -------------------
						
						MC_timer_start(Gun_No,250);
						MC_timer_stop(Gun_No,500);
						MC_timer_stop(Gun_No,1000);
						MC_timer_stop(Gun_No,3000);
						MC_timer_stop(Gun_No,5000);
						MC_timer_stop(Gun_No,60000);
						
						/* if(Gun_No == 0)
							GROUP1_RUN_LED_OFF;
						else if(Gun_No == 1)
							GROUP2_RUN_LED_OFF; */
						if(Gun_No == 0)
						{
							if(Globa->Charger_param.gun_config <= 2)
									GUN2_GROUP1_RUN_LED_OFF;
							else
									GROUP1_RUN_LED_OFF;
						}	
						else if(Gun_No == 1)
						{
							if(Globa->Charger_param.gun_config <= 2)
									GUN2_GROUP2_RUN_LED_OFF;
							else
									GROUP2_RUN_LED_OFF;
						}
						else if(Gun_No == 2)
							GROUP3_RUN_LED_OFF;
						else if(Gun_No == 3)
							GROUP4_RUN_LED_OFF;
						Globa->Control_DC_Infor_Data[Gun_No].led_run_flag = 0;
						Globa->Control_DC_Infor_Data[Gun_No].SYS_Step = 0x01;
						
						Globa->Control_DC_Infor_Data[Gun_No].kw_percent = 1000; //100.0%

						break;
					}
					case 0x01:{//--------------------系统就绪状态01---------------------------
						if((Globa->Control_DC_Infor_Data[Gun_No].charger_start == 1)||
							(Globa->Control_DC_Infor_Data[Gun_No].charger_start == 2)||
							(Globa->Control_DC_Infor_Data[Gun_No].charger_start == 3)||//VIN start step1
							(Globa->Control_DC_Infor_Data[Gun_No].charger_start == 4)//VIN start step2
							)
						{//启动自动充电或手动充电 
							
						  MC_timer_stop(Gun_No,250);
							MC_timer_start(Gun_No,500);//控制灯闪烁
							MC_timer_start(Gun_No,1000);//启动电量累计功能

							Globa->Control_DC_Infor_Data[Gun_No].SYS_Step = 0x11;
							Globa->Control_DC_Infor_Data[Gun_No].MC_Step  = 0x01;
							Globa->Control_DC_Infor_Data[Gun_No].kw_percent = 1000; //100.0%
						}

						if(MC_All_Frame[Gun_No].MC_Recved_PGN3328_Flag == 1)
						{ //收到下发直流分流器量程变比的反馈信息
							Dc_shunt_Set_CL_flag[Gun_No] = 0;
							MC_All_Frame[Gun_No].MC_Recved_PGN3328_Flag = 0;
						}else
						{
							if(Dc_shunt_Set_CL_flag[Gun_No] == 1)
							{//下发分流器
								if(MC_timer_flag(Gun_No,250) == 1)
								{  //定时250MS
									MC_sent_data(fd, PGN3072_ID_MC, 1, Gun_No+1);  //下发充电参数信息
									MC_timer_start(Gun_No,250);
								}
							}
						}
				
						break;
					}
					case 0x11:{//--------------------系统充电状态 ----------------------------
						if(MC_timer_flag(Gun_No,1000) == 1)
						{	//定时1秒							
							Globa->Control_DC_Infor_Data[Gun_No].Time += 1;
							MC_timer_start(Gun_No,1000);
						}
						if(Globa->Control_DC_Infor_Data[Gun_No].charger_start == 4)
							Globa->Control_DC_Infor_Data[Gun_No].charger_start = 1;//
						 if(MC_timer_flag(Gun_No,500) == 1)
						 {  //定时500MS
							 MC_timer_start(Gun_No,500);
							 if(Globa->Control_DC_Infor_Data[Gun_No].led_run_flag == 0)
							 {//点亮（灯闪烁）===充电中常亮，避免机械继电器损坏 181228
								if(Gun_No == 0)
								{
									if(Globa->Charger_param.gun_config <= 2)
											GUN2_GROUP1_RUN_LED_ON;
									else
											GROUP1_RUN_LED_ON;
								}	
								else if(Gun_No == 1)
								{
									if(Globa->Charger_param.gun_config <= 2)
											GUN2_GROUP2_RUN_LED_ON;
									else
											GROUP2_RUN_LED_ON;
								}
								else if(Gun_No == 2)
									GROUP3_RUN_LED_ON;
								else if(Gun_No == 3)
									GROUP4_RUN_LED_ON;
								Globa->Control_DC_Infor_Data[Gun_No].led_run_flag = 1;
							 }
							  else
							 {
							 	if(Gun_No == 0)
								{
									if(Globa->Charger_param.gun_config <= 2)
											GUN2_GROUP1_RUN_LED_OFF;
									else
											GROUP1_RUN_LED_OFF;
								}	
								else if(Gun_No == 1)
								{
									if(Globa->Charger_param.gun_config <= 2)
											GUN2_GROUP2_RUN_LED_OFF;
									else
											GROUP2_RUN_LED_OFF;
								}
								else if(Gun_No == 2)
									GROUP3_RUN_LED_OFF;
								else if(Gun_No == 3)
									GROUP4_RUN_LED_OFF;
								Globa->Control_DC_Infor_Data[Gun_No].led_run_flag = 0;
							 }  								 
						 }
						if(!((Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_falg == 1)&&(Globa->Control_DC_Infor_Data[Gun_No].Double_Gun_To_Car_Hostgun == 0)))
						{
						  MC_control(fd, Gun_No);//充电应用逻辑控制（包括与控制器的通信、刷卡计费的处理、上层系统故障的处理）
						}
						else//双枪同充并且是副枪时
						{
						  if((Globa->Control_DC_Infor_Data[Gun_No].Manu_start == 0)||(Globa->Control_DC_Infor_Data[Gun_No].Error_system != 0))
							{//手动中止充电或系统故障中止充电
								Globa->Control_DC_Infor_Data[Gun_No].SYS_Step = 0x21;    //系统充电结束状态处理
								break;
							}
						}
						
						break;
					}
					case 0x21:{//--------------------充电结束状态处理 ------------------------
						Globa->Control_DC_Infor_Data[Gun_No].charger_start = 0;//充电停止
						Globa->Control_DC_Infor_Data[Gun_No].SYS_Step = 0x00;
					
						break;
					}
					default:
					{
						Globa->Control_DC_Infor_Data[Gun_No].charger_start = 0;//充电停止
						Globa->Control_DC_Infor_Data[Gun_No].SYS_Step = 0x00;
						if(Gun_No == 0)
						{
							if(Globa->Charger_param.gun_config <= 2)
									GUN2_GROUP1_RUN_LED_OFF;
							else
									GROUP1_RUN_LED_OFF;
						}	
						else if(Gun_No == 1)
						{
							if(Globa->Charger_param.gun_config <= 2)
									GUN2_GROUP2_RUN_LED_OFF;
							else
									GROUP2_RUN_LED_OFF;
						}
						else if(Gun_No == 2)
							GROUP3_RUN_LED_OFF;
						else if(Gun_No == 3)
							GROUP4_RUN_LED_OFF;
						Globa->Control_DC_Infor_Data[Gun_No].led_run_flag = 0;
						//while(1);
						break;
					}
				}
			}
	
	}
	close(fd);
	exit(0);
}


/*********************************************************
**description：MC管理线程初始化及建立
***parameter  :none
***return		  :none
**********************************************************/
extern void 	MC_Manage_Thread(void* gun_no)
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
	if(0 != pthread_create(&td1, &attr, (void *)MC_Manage_Task, gun_no)){
		perror("####pthread_create IC_Read_Write_Task ERROR ####\n\n");
	}

	ret = pthread_attr_destroy(&attr);
	if(ret != 0){
		return;
	}
}
