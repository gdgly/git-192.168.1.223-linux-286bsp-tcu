/*******************************************************************************
  Copyright(C)    2019,      EAST Co.Ltd.
  File name :     Charge_Proces_Task.c
  Author :        dengjh
  Version:        1.0
  Date:           2019.4.09
  Description:    //每一个CCU对应一个充电处理任务  
  Other:          
  Function List:  
  History:        
          <author>   <time>   <version>   <desc>
            dengjh     2019.4.09   1.0      Create
*******************************************************************************/
#include "Power_Proces_Task.h"
#define POWER_PROCES_DEBUG 0
#define TIME_OUT_VALUE  60 //超时时间处理异常
#define MODBULE_INIT_TIME 7 //上电7s之后发送数据

UINT8 gPower_Proces_Step[MODULE_INPUT_GROUP] = {0};
UINT8 gSwitching_step[MODULE_INPUT_GROUP]= {0};
UINT8 gExit_Power_step[MODULE_INPUT_GROUP] = {0};
UINT8 Exit_Purpose_Operated_Relay_NO[MODULE_INPUT_GROUP] = {0};
UINT8 Purpose_Operated_Relay_NO[MODULE_INPUT_GROUP] = {0};
UINT8 old_Power_Proces_Step[MODULE_INPUT_GROUP] = {0},old_Sys_MobuleGrp_State[MODULE_INPUT_GROUP] = {0},old_Exit_Power_step[MODULE_INPUT_GROUP] = {0},old_gSwitching_step[MODULE_INPUT_GROUP] = {0};
UINT8 old_Modbule_On[MODULE_INPUT_GROUP] = {0};
time_t Proce_timeout[MODULE_INPUT_GROUP] = {0};

//判断投切结果
UINT8 IsSwitching_Result(UINT8 Group)
{
	if(gSwitching_step[Group] == SWITCHING_END){
		  return 1;
	}
}
//判断退出结果
UINT8 IsExit_Power_Result(UINT8 Group)
{
	if(gExit_Power_step[Group] == EXIT_POWER_END){
		  return 1;
	}
}

//激活模块
UINT8 Allocate_proces_Activate_Group(UINT8 Group_No)
{
	if((Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_State == MOD_GROUP_STATE_INACTIVE))//未激活的才可以
	{
		if(gPower_Proces_Step[Group_No] == POWER_PROCES_IDLE)//空闲状态的时候
		{
			gPower_Proces_Step[Group_No] = POWER_PROCES_ACTIVATION;
			if(POWER_PROCES_DEBUG) printf("----激活模块---Group_No =%d\n",Group_No);
		}
	}
}


//充电模块组广播调电流,调电压//模块组还没有投切过去，所以还是按照本组进行调
void MsgChargeModuleGroupBroadcastOutSetVA(UINT32 Voltage,UINT32 Current,UINT8 Modbule_Group_NO)
{
	Globa->Sys_Tcu2CcuControl_Data[Modbule_Group_NO].model_output_vol = Voltage;     
	Globa->Sys_Tcu2CcuControl_Data[Modbule_Group_NO].model_output_cur = Current;
	Globa->Sys_Tcu2CcuControl_Data[Modbule_Group_NO].model_charge_on = 1;
}

//单点模块组开/关机,直接通过地址进行退出，防止没有切换成功的时候退出不了。
void MsgChargeModuleOnOff(UINT8 Modbule_Group_NO,UINT8 modbule_onoff)
{
	Globa->Sys_Tcu2CcuControl_Data[Modbule_Group_NO].model_charge_on = modbule_onoff; //关机
	if(modbule_onoff == 0)
	{
		Globa->Sys_Tcu2CcuControl_Data[Modbule_Group_NO].model_output_vol = 0;
		Globa->Sys_Tcu2CcuControl_Data[Modbule_Group_NO].model_output_cur = 0;
	}
}


//功率投切
UINT8 Power_Switch_Fun(UINT8 Group_No,UINT8 Target_Group_No, UINT8 Relay_no)
{
	if(POWER_PROCES_DEBUG)printf("\n 功率投切: Group_No =%d Target_Group_No =%d Relay_no =%d \n",Group_No,Target_Group_No,Relay_no);
	if(POWER_PROCES_DEBUG)printf("\n 功率投切: Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_State =%d Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_Gun =%d Globa->Charger_gunbus_param_data[Group_No].Bus_to_mod_Gno =%d\n",Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_State,Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_Gun,Globa->Charger_gunbus_param_data[Group_No].Bus_to_mod_Gno);

  if(((Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_State == MOD_GROUP_STATE_INACTIVE)||
	  (Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_State == MOD_GROUP_STATE_ACTIVE)||
		(Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_State == MOD_GROUP_STATE_ORDER))
		&&(Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_Gun == Globa->Charger_gunbus_param_data[Group_No].Bus_to_mod_Gno))//判断该组是激活状态,并且是自身该组的)//
	{
		Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_Gun =  Globa->Charger_gunbus_param_data[Target_Group_No].Bus_to_mod_Gno;
		Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_State = MOD_GROUP_STATE_ALLOCATED_CAST_GOING;
		Purpose_Operated_Relay_NO[Group_No] =  Relay_no;//目的操作继电器
		gSwitching_step[Group_No] = SWITCHING_INIT;
		gPower_Proces_Step[Group_No] = POWER_PROCES_SWITCHING;
		PowerProcesLogOut(Group_No,"%d#模块组接受到投入命令：继电器号=%d",Group_No + 1,Purpose_Operated_Relay_NO[Group_No]);
		return 1;
	}else{
		return 0;
	}
} 

//功率退出
UINT8 Power_Exit_Fun(UINT8 Group_No,UINT8 Relay_no)
{
  if(((Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_State == MOD_GROUP_STATE_ALLOCATED_CAST_GOING)||
	  (Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_State == MOD_GROUP_STATE_ALLOCATED_OK)||
    (Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_State == MOD_GROUP_STATE_ORDER)||
		(Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_State == MOD_GROUP_STATE_USEING))
		&&(Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_Gun != Globa->Charger_gunbus_param_data[Group_No].Bus_to_mod_Gno))//判断该组是激活状态,并且是自身该组的)//
	{
	//	Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_Gun = Target_Group_No; ---暂时还不去除这个标志，
		Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_State = MOD_GROUP_STATE_ALLOCATED_EXITING;
		Exit_Purpose_Operated_Relay_NO[Group_No] =  Relay_no;//目的操作继电器
		gExit_Power_step[Group_No] = EXIT_POWER_INIT;
		gPower_Proces_Step[Group_No] = POWER_PROCES_EXIT;
		PowerProcesLogOut(Group_No,"%d#模块组接受到模块退出：继电器号=%d",Group_No + 1,Exit_Purpose_Operated_Relay_NO[Group_No]);
	  MsgChargeModuleOnOff(Group_No,EM_RECTIFIER_ONOFF_OFF); //单点关闭该组模块
		return 1;
	}else {
		return 0;
	}
} 

//获取对应的组号的最大组电压
int Get_SYS_MobuleGrp_OutVolatge(int MobuleGrp_Gun)
{
	int MaxVolatge = 0,i=0;
	for(i=0;i<Globa->Group_Modules_Num;i++)
	{
		if(MobuleGrp_Gun ==  Globa->Sys_MODGroup_Control_Data[i].Sys_MobuleGrp_Gun)
		{
			MaxVolatge = 	MAX(MaxVolatge,Globa->Sys_RevCcu_Data[i].model_Output_voltage);
		}
	}
  return MaxVolatge;
}

//每个任务管理一个模块组，0-5分别对应管理序号为0-5组模块,其实也是CCU
void Power_Proces_Fun(int Group_No)
{	

	UINT32 AC_Relay_control =0;
	UINT32 i=0,j=0;
	UINT32 AC_Relay_Ctl_Count = 0,SetVolage = 0, SetCurrent = 50,SYS_MobuleGrp_OutVolatge = 0;	
	UINT32 modbule_Groupno_Pno = 0,modbule_Groupno_Nno = 0,ControlCmd_Relay_falg = 0;
	UINT8  contactor_to_busno[2] = {0};
  time_t current_time = 0;
	INT16 Actual_State_no = 0;
	UINT32 CcuNo = 0;//对应的枪序号,及该枪的状态
	
	time(&current_time);
	if(old_Power_Proces_Step[Group_No] != gPower_Proces_Step[Group_No])
	{
		old_Power_Proces_Step[Group_No] = gPower_Proces_Step[Group_No];
		PowerProcesLogOut(Group_No,"%d#模块组任务处理步骤 gPower_Proces_Step=%d",Group_No + 1,gPower_Proces_Step[Group_No]);
	}
					
	if(old_Sys_MobuleGrp_State[Group_No] != Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_State)
	{
		old_Sys_MobuleGrp_State[Group_No] = Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_State;
		PowerProcesLogOut(Group_No,"%d#模块组状态 =%d ",Group_No + 1,Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_State);
	}
	if( old_gSwitching_step[Group_No] != gSwitching_step[Group_No]){
		old_gSwitching_step[Group_No] = gSwitching_step[Group_No];
		PowerProcesLogOut(Group_No,"%d#模块组投切步骤 =%d 继电器号=%d",Group_No + 1,old_gSwitching_step[Group_No],Purpose_Operated_Relay_NO[Group_No]);
	}
	if( old_Exit_Power_step[Group_No] != gExit_Power_step[Group_No]){
		old_Exit_Power_step[Group_No] = gExit_Power_step[Group_No];
		PowerProcesLogOut(Group_No,"%d#模块组退出步骤 =%d 继电器号=%d",Group_No + 1,old_Exit_Power_step[Group_No],Exit_Purpose_Operated_Relay_NO[Group_No]);
	}
	
	switch (gPower_Proces_Step[Group_No])
	{
		case POWER_PROCES_IDLE://空闲状态
		{
			if(Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_State == MOD_GROUP_STATE_INACTIVE)//未激活状态
			{
				//由充电服务进行激活处理
				AC_Relay_control = 0;
				for(i =0;i<Globa->Charger_param.gun_config;i++)
				{
					AC_Relay_control |= Globa->Control_DC_Infor_Data[i].AC_Relay_control; 
				}
				if(AC_Relay_control == 1)
				{
					Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_State = MOD_GROUP_STATE_ACTIVE;
				}
			}else if (Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_State == MOD_GROUP_STATE_ACTIVE)
			{
				MsgChargeModuleOnOff(Group_No,EM_RECTIFIER_ONOFF_OFF); //单点关闭该组模块
				
				AC_Relay_control = 0;
				for(i =0;i<Globa->Charger_param.gun_config;i++)
				{
					AC_Relay_control |= Globa->Control_DC_Infor_Data[i].AC_Relay_control; 
				}
				if(AC_Relay_control == 0)
				{
					Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_State = MOD_GROUP_STATE_INACTIVE;
				}
			}else{//被使用状态
				
			}
			break;
		}
		case POWER_PROCES_SWITCHING://投切处理
		{
			switch (gSwitching_step[Group_No])
			{
				case SWITCHING_IDLE://空闲状态
				{
					 gPower_Proces_Step[Group_No] = POWER_PROCES_IDLE; 
					 break;
				}
				case SWITCHING_INIT://初始化状态
				{
					 gSwitching_step[Group_No] = SWITCHING_PREPAR ;
					 Proce_timeout[Group_No] = current_time;
					 break;
				}
				case SWITCHING_PREPAR://等待投切准备 -- 主要是针对模块组没有激活的时候
				{
		    //  if((Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_State == MOD_GROUP_STATE_ACTIVE)||(Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_State == MOD_GROUP_STATE_ORDER))
				//	{
						//是否需要等待其完全激活
						gSwitching_step[Group_No] = SWITCHING_PRECHARGE;
						Actual_State_no = 0;
						Proce_timeout[Group_No] = current_time;
						SetVolage = 0;
						ControlCmd_Relay_falg = 0;
						MsgChargeModuleOnOff(Group_No,EM_RECTIFIER_ONOFF_OFF); //单点关闭该组模块						
						CcuNo = Gets_busno_Gno_No_By_MOdgrno(Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_Gun);//需要投切过去的组号对应的枪号
						if(POWER_PROCES_DEBUG) printf("\n\n--666- CcuNo =%d Globa->Sys_MODGroup_Control_Data[Group_No=%d].Sys_MobuleGrp_Gun =%d\n",CcuNo,Group_No,Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_Gun);
						CcuNo = (CcuNo -1)>0? (CcuNo -1):0;
						if(POWER_PROCES_DEBUG) printf("\n\n--666- CcuNo =%d\n",CcuNo);
						if(POWER_PROCES_DEBUG) printf("\n\n--777-  Globa->Sys_RevCcu_Data[CcuNo].Modbule_On =%d\n", Globa->Sys_RevCcu_Data[CcuNo].Modbule_On);
						old_Modbule_On[Group_No] = Globa->Sys_RevCcu_Data[CcuNo].Modbule_On;
					//}
					break;
				}
				case SWITCHING_PRECHARGE://投切预充 ,直接投切本地的是没有预充之说，
				{
					if((Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_Gun == Globa->Charger_gunbus_param_data[Group_No].Bus_to_mod_Gno)){//如果是投切本身的，则直接开机，不需要预充阶段
						 if(1){//直连的
							 gSwitching_step[Group_No] = SWITCHING_CONTROL_TRANSFER;
							// MsgLocalModGroupBroadcastAddrSet(Group_No,Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_Gun);//设置模块组，防止掉电
							Proce_timeout[Group_No] = current_time;
						 }else{
							 //下发该组投入到本CCU的相应继电器--指的是模块组刚好和CCU没有连接在一起的情况
						 }
					}else{//投切到其他的组中
						//从组号里或取对应的CCU号，
						SetVolage = Get_SYS_MobuleGrp_OutVolatge(Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_Gun); //djh0922
						
						if(POWER_PROCES_DEBUG) printf("\n Group_No =%d----SetVolage = %d Group_No=%d Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_Gun=%d\n " ,Group_No,SetVolage, Group_No, Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_Gun);
	        	
						CcuNo = Gets_busno_Gno_No_By_MOdgrno(Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_Gun);//需要投切过去的组号对应的枪号
						CcuNo = (CcuNo -1)>0? (CcuNo -1):0;
						
  					if(old_Modbule_On[Group_No] != Globa->Sys_RevCcu_Data[CcuNo].Modbule_On)
						{
							old_Modbule_On[Group_No] = Globa->Sys_RevCcu_Data[CcuNo].Modbule_On;
						}
					
					  if(POWER_PROCES_DEBUG) printf("\n 该组old_Modbule_On[Group_No] =%d----投入目标CcuNo = %d 该组Group_No=%d 目标Globa->Sys_RevCcu_Data[CcuNo].Modbule_On=%d\n " ,old_Modbule_On[Group_No],CcuNo, Group_No,Globa->Sys_RevCcu_Data[CcuNo].Modbule_On);

						if(old_Modbule_On[Group_No] == EM_RECTIFIER_ONOFF_ON)
						{
							if(SetVolage != 0){
								MsgChargeModuleGroupBroadcastOutSetVA(SetVolage,SetCurrent,Group_No);
								if(POWER_PROCES_DEBUG)  printf("\n----进行模块开机动作及设置电压电流: Group_No =%d SetCurrent =%d SetVolage=%d\n",Group_No,SetCurrent,SetVolage);
							}else {
								if(POWER_PROCES_DEBUG)  printf("\n---目标电压为0不进行开关模块: Group_No =%d SetCurrent =%d SetVolage=%d\n",Group_No,SetCurrent,SetVolage);
							}
						}else if(old_Modbule_On[Group_No] == EM_RECTIFIER_ONOFF_OFF)
						{
							MsgChargeModuleOnOff(Group_No,EM_RECTIFIER_ONOFF_OFF); //单点关闭该组模块
						}
						
						if(old_Modbule_On[Group_No] == EM_RECTIFIER_ONOFF_ON)
						{							
							if(POWER_PROCES_DEBUG) printf("-----Group_No =%d,SetVolage =%d Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_Gun=%d\n",Group_No,SetVolage,Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_Gun);
							//从接触器号，查找两个相连近的母线号。
							/*if(Gets_BUS_number(Purpose_Operated_Relay_NO[Group_No],contactor_to_busno) != -1)
							{
								modbule_Groupno_Pno = Gets_modbule_Group_No_bybusNO(contactor_to_busno[0]);
								modbule_Groupno_Nno = Gets_modbule_Group_No_bybusNO(contactor_to_busno[1]);//从母线号获取对应的模块组的序列号
							}*/
							if(POWER_PROCES_DEBUG) printf("-----SetVolage =%d,Group_No =%d Globa->Sys_MODGroup_Control_Data[Group_No].SYS_MobuleGrp_OutVolatge =%d \n",SetVolage,Group_No,Globa->Sys_MODGroup_Control_Data[Group_No].SYS_MobuleGrp_OutVolatge);

							if(abs(Globa->Sys_MODGroup_Control_Data[Group_No].SYS_MobuleGrp_OutVolatge - SetVolage) <= 100)  //djh0922
							{
								Globa->Operated_Relay_NO |= (1 << Purpose_Operated_Relay_NO[Group_No]);//闭合继电器
								//下发继电器命令
								Actual_State_no = Gets_contactor_control_unit_no_number(Purpose_Operated_Relay_NO[Group_No]);//控制位对应相应的反馈位置
								if(Purpose_Operated_Relay_NO[Group_No] != 0){
									Globa->Operated_Relay_NO_falg[Purpose_Operated_Relay_NO[Group_No] - 1] = 1;
									Globa->Sys_Tcu2CcuControl_Data[((Actual_State_no>>8)&0xff) -1].WeaverRelay_control = 1;
								}
								ControlCmd_Relay_falg = 1;
							}
							
							if(ControlCmd_Relay_falg == 1){//表示下发了继电器控制，之后进行判断
								if(Purpose_Operated_Relay_NO[Group_No] != 0){//检测继电器已经动作了
									Actual_State_no = Gets_contactor_control_unit_no_number(Purpose_Operated_Relay_NO[Group_No]);//控制位对应相应的反馈位置
							    if(POWER_PROCES_DEBUG) printf("-----Actual_State_no =%0x Globa->rContactor_Actual_State[((Actual_State_no>>8)&0xff) -1] =%d \n",Actual_State_no,Globa->rContactor_Actual_State[((Actual_State_no>>8)&0xff) -1]);

									if((((Globa->rContactor_Actual_State[((Actual_State_no>>8)&0xff) -1] >> 2*(Actual_State_no&0xff))&0x01) == 1)&&
										(((Globa->rContactor_Actual_State[((Actual_State_no>>8)&0xff) -1] >> (2*(Actual_State_no&0xff) + 1))&0x01) == 1 ))
									{//判断该接触器已经动作了，
										gSwitching_step[Group_No]= SWITCHING_END;										
										ControlCmd_Relay_falg = 0;
										Proce_timeout[Group_No] = current_time;
										break;
									} 
								}
							}
							if(POWER_PROCES_DEBUG) 	printf("进行模块开机动作及设置电压电流 Group_No=%d current_time =%d \n",Group_No,current_time);
						}
		
						//超时处理，待完善
						if(abs(current_time - Proce_timeout[Group_No])> TIME_OUT_VALUE)
						{
							Proce_timeout[Group_No] = current_time;
							MsgChargeModuleOnOff(Group_No,EM_RECTIFIER_ONOFF_OFF); //单点关闭该组模块
							gSwitching_step[Group_No] = SWITCHING_TIMEOUT;
							if(POWER_PROCES_DEBUG) 	printf("超时处理 Group_No=%d abs(current_time - Proce_timeout[Group_No]) =%d \n",Group_No,abs(current_time - Proce_timeout[Group_No]));

						}
					}
					break;
				}
				case SWITCHING_END://投切完成
				{
					Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_State = MOD_GROUP_STATE_ALLOCATED_OK;
					gSwitching_step[Group_No] = SWITCHING_IDLE;
					break;
				}
				case SWITCHING_TIMEOUT://投切超时
				{ 
					Globa->Sys_MODGroup_Control_Data[Group_No].Control_Timeout_Falg = 1;
					Globa->Sys_MODGroup_Control_Data[Group_No].Control_Timeout_Count++;
					gSwitching_step[Group_No] = SWITCHING_IDLE;
					break;
				}
				default:
					break;
			}
			break;
		}
		case POWER_PROCES_EXIT://退出处理
		{
			switch (gExit_Power_step[Group_No])
			{
				case EXIT_POWER_IDLE://空闲状态
				{
					 gPower_Proces_Step[Group_No] = POWER_PROCES_IDLE; 
					 Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_State = MOD_GROUP_STATE_ACTIVE; //完成之后置激活状态
					 if(POWER_PROCES_DEBUG) printf("----jihuo Group_No =%d",Group_No);
					 break;
				}
				case EXIT_POWER_INIT://退出状态初始化
				{
					Proce_timeout[Group_No] = current_time;
					gExit_Power_step[Group_No] = EXIT_POWER_OFF;
					MsgChargeModuleOnOff(Group_No,EM_RECTIFIER_ONOFF_OFF); //单点关闭该组模块
					Globa->Sys_MODGroup_Control_Data[Group_No].Control_Timeout_Falg = 0;
					ControlCmd_Relay_falg = 0;
					break;
				}
				case EXIT_POWER_OFF://关闭相应模块及接触器
				{
					MsgChargeModuleOnOff(Group_No,EM_RECTIFIER_ONOFF_OFF); //单点关闭该组模块
					if((Globa->Sys_MODGroup_Control_Data[Group_No].SYS_MobuleGrp_OutVolatge <= 2000)&&(Globa->Sys_RevCcu_Data[Group_No].model_onoff == 1))//模块组电压小于该电压的时候
					{
						if(Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_Gun != Globa->Charger_gunbus_param_data[Group_No].Bus_to_mod_Gno)
						{
							Globa->Operated_Relay_NO |= (0 << Exit_Purpose_Operated_Relay_NO[Group_No]);//断开继电器

							Actual_State_no = Gets_contactor_control_unit_no_number(Exit_Purpose_Operated_Relay_NO[Group_No]);
						//	Globa->Control_panel_Relay_Control[Actual_State_no&0xff] = 0;
					  	Globa->Sys_Tcu2CcuControl_Data[((Actual_State_no>>8)&0xff) -1].WeaverRelay_control = 0;
							Globa->Operated_Relay_NO_falg[Purpose_Operated_Relay_NO[Group_No] - 1] = 0;
							ControlCmd_Relay_falg = 1;							
						}
						else
						{
							//gExit_Power_step[Group_No] = EXIT_POWER_CONTROL_TRANSFER;
							//break;
						}
						
						if(Purpose_Operated_Relay_NO[Group_No] != 0){
							if(ControlCmd_Relay_falg == 1)
							{
								if(Exit_Purpose_Operated_Relay_NO[Group_No] == Purpose_Operated_Relay_NO[Group_No])
								{
									Actual_State_no = Gets_contactor_control_unit_no_number(Exit_Purpose_Operated_Relay_NO[Group_No]);//控制位对应相应的反馈位置
									if(Actual_State_no != -1){
										if((((Globa->rContactor_Actual_State[((Actual_State_no>>8)&0xff) - 1] >> 2*(Actual_State_no&0xff))&0x01) == 0)&&
											(((Globa->rContactor_Actual_State[((Actual_State_no>>8)&0xff) - 1] >> (2*(Actual_State_no&0xff) + 1))&0x01) == 0 ))
										{//读该继电器状态判断其是否已经断开了继电器
											 gExit_Power_step[Group_No] = EXIT_POWER_END;
											 Proce_timeout[Group_No] = current_time;
											 if(POWER_PROCES_DEBUG) printf("关闭相应模块及接触器---ok Group_No =%d  Actual_State_no =%d == Exit_Purpose_Operated_Relay_NO[Group_No]) =%d \n",Group_No,Actual_State_no,Exit_Purpose_Operated_Relay_NO[Group_No]);
                       usleep(200000);//额外再延时200ms
										}
									}
								}
							}
						}else {
							gExit_Power_step[Group_No] = EXIT_POWER_END;
							Proce_timeout[Group_No] = current_time;
							usleep(200000);//额外再延时200ms
						}
					}
					//超时处理,待完善
					if(abs(current_time - Proce_timeout[Group_No]) > TIME_OUT_VALUE)//异常超时，
					{
						Proce_timeout[Group_No] = current_time;
						
					  Globa->Operated_Relay_NO |= (0 << Exit_Purpose_Operated_Relay_NO[Group_No]);//断开继电器
						Actual_State_no = Gets_contactor_control_unit_no_number(Exit_Purpose_Operated_Relay_NO[Group_No]);
						Globa->Sys_Tcu2CcuControl_Data[((Actual_State_no>>8)&0xff) -1].WeaverRelay_control = 0;
						Globa->Operated_Relay_NO_falg[Purpose_Operated_Relay_NO[Group_No] - 1] = 0;
						PowerProcesLogOut(Group_No,"%d#模块组退出异常，进入强制退出阶段 V = %d model_onoff =%d ",Group_No + 1,Globa->Sys_MODGroup_Control_Data[Group_No].SYS_MobuleGrp_OutVolatge,Globa->Sys_RevCcu_Data[Group_No].model_onoff);

						gExit_Power_step[Group_No] = EXIT_POWER_END;
					  Proce_timeout[Group_No] = current_time;
						//处理异常-进入处理异常阶段
						usleep(200000);//额外再延时200ms
					}
					break;
				}
				case EXIT_POWER_END://退出完毕
				{
					Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_Gun =  Globa->Charger_gunbus_param_data[Group_No].Bus_to_mod_Gno;
					Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_State = MOD_GROUP_STATE_EXITOK;
					gExit_Power_step[Group_No] = EXIT_POWER_IDLE;
					if(POWER_PROCES_DEBUG) printf("退出完毕退出完毕=%d",Group_No);
					Exit_Purpose_Operated_Relay_NO[Group_No] = 0;
					Purpose_Operated_Relay_NO[Group_No] = 0;
					break;
				}
				default:
					break;
			}				
			break;
		}
		default:
			break;
	}
}
