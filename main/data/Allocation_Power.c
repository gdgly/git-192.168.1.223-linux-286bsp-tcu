/*******************************************************************************
  Copyright(C)    2019,      EAST Co.Ltd.
  File name :     Allocation_Power.c
  Author :        dengjh
  Version:        1.0
  Date:           2019.4.09
  Description:    //功率分配申请  
  Other:          
  Function List:  
  History:        
          <author>   <time>   <version>   <desc>
            dengjh     2019.4.09   1.0      Create
*******************************************************************************/
#include "Allocation_Power.h"
#define  ALLOCATION_DEBUG  0 
#define  RELAY_NO_MAXFAULT_COUNT 3 //一个充电循环最大可以尝试5次，还是失败则不在进行闭合
#define  SWITCHTIMEOUT_COUNT 2 //一个充电循环最大可以尝试5次，还是失败则不在进行闭合

#define  WAITSELECTIONCONFIG    //先把一级目录选择出来，之后再进行排列，之后再从这个排列里面提取需要并机的模块组  ，另一种方式就是直接根据顺序进行投切
#define  ALLOCATION_DEBUG1 

MODGROUP_DIAGRAM_LIST gGroup_Diagram_list[MODULE_INPUT_GROUP];//每个模块组的关系图
//从继电器号获取对应的母线号
UINT8 Gets_BUS_number(UINT8 contactor_no,UINT8*contactor_bus)
{
	UINT8 i=0;
	for(i = 0;i<Globa->Charger_param.Contactor_Line_Num;i++)
	{
		if((contactor_no == Globa->Charger_Contactor_param_data[i].contactor_no))
		{
			contactor_bus[0] =  Globa->Charger_Contactor_param_data[i].contactor_bus_P;
			contactor_bus[1] =  Globa->Charger_Contactor_param_data[i].contactor_bus_N;
			return 1;
		}
  }
	return 0;//获取失败
}

//从继电器号获取实际对应的设备控制的对应的序列号
INT16 Gets_contactor_control_unit_no_number(UINT8 contactor_no)
{
	UINT8 i=0;
	for(i = 0;i<Globa->Charger_param.Contactor_Line_Num;i++)
	{
		if((contactor_no == Globa->Charger_Contactor_param_data[i].contactor_no))
		{
			return ((Globa->Charger_Contactor_param_data[i].Correspond_control_unit_address << 8)|Globa->Charger_Contactor_param_data[i].Correspond_control_unit_no);//高位为控制模块地址，低位为该控制模块的序列号
		}
  }
	return (-1);//获取失败
}
//从对应的母线号获取组号
UINT8 Gets_ModGroup_number(UINT8 modbule_bus_Gno)
{
	UINT8 i=0;
	for(i = 0;i<Globa->Group_Modules_Num;i++)
	{
		if((modbule_bus_Gno == Globa->Charger_gunbus_param_data[i].Bus_no))
		{
		  return Globa->Charger_gunbus_param_data[i].Bus_to_mod_Gno;
		}
  }
	return 0;//获取失败
}

//从对应组号获取组号母线号
INT8 Gets_busccu_number(UINT8 ModGroup_Gno)
{
	UINT8 i=0;
	for(i = 0;i<Globa->Group_Modules_Num;i++)
	{
		if((ModGroup_Gno == Globa->Charger_gunbus_param_data[i].Bus_to_mod_Gno))
		{
		  return Globa->Charger_gunbus_param_data[i].Bus_no;
		}
  }
	return 0;//获取失败
}


//从对应CCU号获取组号母线号/模块组号(直连的)
INT8 Gets_busno_mod_Gno_number(UINT8 CCUno)
{
	UINT8 i=0;
	for(i = 0;i<Globa->Group_Modules_Num;i++)
	{
		if(((CCUno + 1) == Globa->Charger_gunbus_param_data[i].Bus_to_ccu_no)&&(Globa->Charger_gunbus_param_data[i].Bus_to_ccu_no != -1))
		{
		  return Globa->Charger_gunbus_param_data[i].Bus_to_mod_Gno;
		}
  }
	return 0;//获取失败
}
//从对应模块组号获取该存储数组中的对应的数据。
INT8 Gets_modbule_Group_No(UINT8 ModGroup_Gno)
{
	UINT8 i=0;
	for(i = 0;i<Globa->Group_Modules_Num;i++)
	{
		if((ModGroup_Gno == Globa->Charger_gunbus_param_data[i].Bus_to_mod_Gno))
		{
		  return i;
		}
  }
	return 0;//获取失败
}

//从对应的母线号获取模块组号获取该存储数组中的对应的数据。
INT8 Gets_modbule_Group_No_bybusNO(UINT8 modbule_bus_Gno)
{
	UINT8 i=0;
	for(i = 0;i<Globa->Group_Modules_Num;i++)
	{
		if((modbule_bus_Gno == Globa->Charger_gunbus_param_data[i].Bus_to_mod_Gno))
		{
		  return i;
		}
  }
	return 0;//获取失败
}



//从对应母线号获取母线配置中的序号
INT8 Gets_busno_Gno_No(UINT8 busno)
{
	UINT8 i=0;
	for(i = 0;i<Globa->Group_Modules_Num;i++)
	{
		if((busno == Globa->Charger_gunbus_param_data[i].Bus_no))
		{
		  return i;
		}
  }
	return 0;//获取失败
}

//从母线配置中对应组号获取母线配置中的序号，从而获取枪号--直接获取的为抢号
INT8 Gets_busno_Gno_No_By_MOdgrno(UINT8 ModGroup_Gno)
{
	UINT8 i=0;
	for(i = 0;i<Globa->Group_Modules_Num;i++)
	{
		if((ModGroup_Gno == Globa->Charger_gunbus_param_data[i].Bus_to_mod_Gno))
		{
		  return Globa->Charger_gunbus_param_data[i].Bus_to_ccu_no;
		}
  }
	return 0;//获取失败
}



INT8 ISContactor_ok(UINT8 Contactor_No)
{
	  INT16 Actual_State_no = 0,Actual_State_Addr =0;
	  if(Contactor_No == 0) return 1;
	 	Actual_State_no = Gets_contactor_control_unit_no_number(Contactor_No);//控制位对应相应的反馈位置
		if(ALLOCATION_DEBUG) printf("\n--Actual_State_no&0xff = %d Operated_Relay_NO_Fault_Count =%d \n",Actual_State_no&0xff,Globa->Operated_Relay_NO_Fault_Count[(Contactor_No - 1)]);
		if(Actual_State_no == -1) return 0;
		Actual_State_Addr = ((Actual_State_no>>8)&0xff);
		if(Actual_State_Addr == 0) return 0;
		Actual_State_Addr = Actual_State_Addr -1;
		if(ALLOCATION_DEBUG) printf(" \n Contactor_No =%d ,Actual_State_Addr =%d rContactor_Miss_fault[Actual_State_Addr]:%d,rContactor_Adhesion_faul[Actual_State_Addr]t:%d rContactor_Misoperation_fault[Actual_State_Addr]=%d \n",Contactor_No,Actual_State_Addr,Globa->rContactor_Miss_fault[Actual_State_Addr],Globa->rContactor_Adhesion_fault[Actual_State_Addr],Globa->rContactor_Misoperation_fault[Actual_State_Addr]);

 	 if((((Globa->rContactor_Miss_fault[Actual_State_Addr] >> 2*(Actual_State_no&0xff))&0x01) == 0)&&
		 (((Globa->rContactor_Miss_fault[Actual_State_Addr] >>(2*(Actual_State_no&0xff) + 1))&0x01)  == 0)&&
		 (((Globa->rContactor_Adhesion_fault[Actual_State_Addr]>>2*(Actual_State_no&0xff))&0x01) == 0)&&
		 (((Globa->rContactor_Adhesion_fault[Actual_State_Addr]>>(2*(Actual_State_no&0xff) + 1))&0x01) == 0)&&
		 (((Globa->rContactor_Misoperation_fault[Actual_State_Addr]>>2*(Actual_State_no&0xff))&0x01) == 0)&&
		 (((Globa->rContactor_Misoperation_fault[Actual_State_Addr]>>(2*(Actual_State_no&0xff) + 1))&0x01) == 0))
	  {
			if(ALLOCATION_DEBUG) printf("-return-----1---\n");
		   return 1;
		}else {
			if(ALLOCATION_DEBUG) printf("-return-----0---\n");
			if(Globa->Operated_Relay_NO_falg[Contactor_No - 1]){
				Globa->Operated_Relay_NO_Fault_Count[(Contactor_No - 1)]++;
				Globa->Operated_Relay_NO_falg[Contactor_No - 1] = 0;
			}
			return 0;
		}
}
//查询已经加入该组的模块组是否有异常的情况，由于不考虑模块组通讯异常的情况，所以正常的不会把本身的组号剔除去。
void Already_invested_module_detection(UINT32 Apply_Ccu_no, SWITCH_MODGROUP_LIST *lSwitch_Modgroup_list)
{
	int i =0,j=0,gun_ccu_no = 0,busccu_no =0,Bus_position_no = 0,Mod_GroupNO = 0,Gun_Charged_State = 0,Gunlink = 0,Gun_Output_Relay_State = 0,Errorsystem = 0,MC_connect_lost_falut = 0;
	int Priority_Gun_Charged_State = 0,Priority_Gun_Output_Relay_State= 0,Priority_Errorsystem = 0,Priority_Gunlink = 0,Priority_MC_connect_lost_falut =  0,Priority_gun_no=0;
	
	char contactor_bus[2] = {0};//接触器对应的正向反向母线号
	SWITCH_MODGROUP_LIST removed_group;
	SWITCH_MODGROUP_LIST reserved_group;//需要剔除的组号,及预留下来的模块组号
	memset(&removed_group,0,sizeof(SWITCH_MODGROUP_LIST));
	memset(&reserved_group,0,sizeof(SWITCH_MODGROUP_LIST));
	
	if(ALLOCATION_DEBUG) printf("Already_invested_module_detection:lSwitch_Modgroup_list->Modbule_Group_number =%d \n",lSwitch_Modgroup_list->Modbule_Group_number);		

	for(i=0;i<lSwitch_Modgroup_list->Modbule_Group_number;i++)//已经投入的到该组的模块组数
	{
		busccu_no = Gets_busccu_number(lSwitch_Modgroup_list->Modbule_Group_Adrr[i]); //获取母线号
		Mod_GroupNO = Gets_modbule_Group_No(lSwitch_Modgroup_list->Modbule_Group_Adrr[i]);//模块组号对应的在参数中的序列号
    
		Priority_Gun_Charged_State = 0;
		Priority_Errorsystem = 0;
		Priority_MC_connect_lost_falut =  0;
		Priority_Gun_Output_Relay_State = 0;
		Priority_Gunlink = 0;
		
		
		Bus_position_no = Gets_busno_Gno_No(busccu_no);//获取该母线参数对应的序号
		if(Globa->Charger_gunbus_param_data[Bus_position_no].Bus_to_ccu_no != -1){
			gun_ccu_no = Globa->Charger_gunbus_param_data[Bus_position_no].Bus_to_ccu_no - 1;//换成枪的序号= 枪号-1；//by 0808
		  Gunlink = 0;
			Gun_Charged_State =  Globa->Sys_RevCcu_Data[gun_ccu_no].Charge_State; //开启充电
			Gun_Output_Relay_State = (((Globa->Sys_RevCcu_Data[gun_ccu_no].Relay_State>>4)&0x0f) == 0)? 0:1; //对应的输出接触器

			MC_connect_lost_falut = Globa->Control_DC_Infor_Data[gun_ccu_no].Error.ctrl_connect_lost;
	
		  Errorsystem = Globa->Sys_RevCcu_Data[gun_ccu_no].Errorsystem;
		}else{//表示没有挂载枪CCU号
		  gun_ccu_no = -1; //代表无绑定的CCU号
			Gun_Charged_State = 0;
		  Errorsystem = 0;
			Gun_Output_Relay_State = 0;
			MC_connect_lost_falut = 0;
			Gunlink = 0;
		}

		//判断条件：1.该组模块全部异常(不需要,退出)，2该继电器异常，3该组对应的枪号连接状态
		if(ALLOCATION_DEBUG) printf(" \n查询 Already_invested_module_detection: busccu_no =%d Apply_Ccu_no=%d gun_ccu_no =%d lSwitch_Modgroup_list->Contactor_No[%d] =%d,  = %d\n",busccu_no,Apply_Ccu_no,gun_ccu_no,i,lSwitch_Modgroup_list->Contactor_No[i],Globa->Sys_MODGroup_Control_Data[0].Sys_MobuleGrp_Gun);
		if(ALLOCATION_DEBUG) printf(" \n查询 22Already_invested_module_detection: busccu_no =%d Apply_Ccu_no=%d gun_ccu_no =%d  Gun_Charged_State = %dlSwitch_Modgroup_list->Contactor_No[%d] =%d,  ok= %d\n",busccu_no,Apply_Ccu_no,gun_ccu_no,Gun_Charged_State,i,lSwitch_Modgroup_list->Contactor_No[i],ISContactor_ok(lSwitch_Modgroup_list->Contactor_No[i]));
	
		if(ALLOCATION_DEBUG) printf("\n 查询-00001-Gunlink =%d, Gun_Charged_State=%d   Gun_Output_Relay_State  =%d MC_connect_lost_falut =%d Errorsystem =%d \n",Gunlink,Gun_Charged_State,Gun_Output_Relay_State, MC_connect_lost_falut,Errorsystem);		
		if(ALLOCATION_DEBUG) printf("\n 查询-002-Priority_Gunlink =%d, Priority_Gun_Charged_State=%d   Priority_Gun_Output_Relay_State  =%d Priority_MC_connect_lost_falut =%d Priority_Errorsystem =%d \n",Priority_Gunlink,Priority_Gun_Charged_State,Priority_Gun_Output_Relay_State, Priority_MC_connect_lost_falut,Priority_Errorsystem);		

  	if(gun_ccu_no != Apply_Ccu_no){//只有不是本身的枪号才进行判断
			if(ALLOCATION_DEBUG) printf(" \n : busccu_no =%d Apply_Ccu_no=%d gun_ccu_no =%d  Gun_Charged_State = %d Contactor_No =%d  ISContactor_ok(lSwitch_Modgroup_list->Contactor_No[i]) =%d \n",busccu_no,Apply_Ccu_no,gun_ccu_no,Gun_Charged_State,lSwitch_Modgroup_list->Contactor_No[i],ISContactor_ok(lSwitch_Modgroup_list->Contactor_No[i]));

			if((ISContactor_ok(lSwitch_Modgroup_list->Contactor_No[i]) == 0)||(((Gun_Charged_State == 1)||(Gun_Output_Relay_State == 1)||(MC_connect_lost_falut == 1)||(Errorsystem == 1)||(Gunlink == 1)||(Priority_Gun_Charged_State == 1)||(Priority_Gun_Output_Relay_State == 1)||(Priority_MC_connect_lost_falut == 1)||(Priority_Gunlink == 1)||(Priority_Errorsystem == 1)||(Globa->Sys_MODGroup_Control_Data[Mod_GroupNO].Control_Timeout_Falg == 1))&&(Apply_Ccu_no != gun_ccu_no)))
			// ||(Globa->Sys_MODGroup_Control_Data[lSwitch_Modgroup_list->Modbule_Group_Adrr[i]].Sys_Grp_OKNumber == 0))只有线路异常的时候才需要退出，而通信异常这不需要
			{//该组有异常则需要剔除该组。
				 removed_group.Modbule_Group_Adrr[removed_group.Modbule_Group_number] = lSwitch_Modgroup_list->Modbule_Group_Adrr[i];
				 removed_group.Contactor_No[removed_group.Modbule_Group_number] = lSwitch_Modgroup_list->Contactor_No[i];
				 removed_group.Modbule_Group_number++;
			}
		}else {
			if(ALLOCATION_DEBUG) printf("gun_ccu_no =%d Apply_Ccu_no =%d \n",gun_ccu_no,Apply_Ccu_no);		
		}
	}
	if(ALLOCATION_DEBUG) printf(" \nremoved_group.Modbule_Group_number  =%d \n",removed_group.Modbule_Group_number);

	//检索还需要退出的模块组，及继电器
	if(removed_group.Modbule_Group_number != 0)
	{
		for(i=0;i<lSwitch_Modgroup_list->Modbule_Group_number;i++)//已经投入的到该组的模块组数
		{
			if(lSwitch_Modgroup_list->Contactor_No[i] != 0)
			{
				for(j=0;j<removed_group.Modbule_Group_number;j++)
				{
					if((lSwitch_Modgroup_list->Modbule_Group_Adrr[i] != removed_group.Modbule_Group_Adrr[j])&&(lSwitch_Modgroup_list->Contactor_No[i] !=  removed_group.Contactor_No[j]))//先判断该组没有在推出列表中
					{ 
						if(Gets_BUS_number(lSwitch_Modgroup_list->Contactor_No[i],&contactor_bus[0]))
						{
							if((Gets_ModGroup_number(contactor_bus[0]) == removed_group.Modbule_Group_Adrr[j])||
							  (Gets_ModGroup_number(contactor_bus[1]) == removed_group.Modbule_Group_Adrr[j]))
							{//组号相等，说明该组也是挂载需要退出的继电器支路上。
							  removed_group.Modbule_Group_Adrr[removed_group.Modbule_Group_number] = lSwitch_Modgroup_list->Modbule_Group_Adrr[i];
			          removed_group.Contactor_No[removed_group.Modbule_Group_number] = lSwitch_Modgroup_list->Contactor_No[i];
			          removed_group.Modbule_Group_number++;
							}
						}
					}
				}
			}
		}
		
	  //进行重新整理模块组，减去需要去除的模块组号
		for(i=0;i<lSwitch_Modgroup_list->Modbule_Group_number;i++)//已经投入的到该组的模块组数
		{
			for(j=0;j<removed_group.Modbule_Group_number;j++)
			{
				if((lSwitch_Modgroup_list->Modbule_Group_Adrr[i] == removed_group.Modbule_Group_Adrr[j])
					&&(lSwitch_Modgroup_list->Contactor_No[i] == removed_group.Contactor_No[j]))
				{
					break;
				}
			}
			
			if(j==removed_group.Modbule_Group_number)
			{
				if(ALLOCATION_DEBUG) printf("reserved_group.Modbule_Group_number =%d  i=%d j=%d \n",reserved_group.Modbule_Group_number,i,j);
			  reserved_group.Modbule_Group_Adrr[reserved_group.Modbule_Group_number] = lSwitch_Modgroup_list->Modbule_Group_Adrr[i];
			  reserved_group.Contactor_No[reserved_group.Modbule_Group_number] = lSwitch_Modgroup_list->Contactor_No[i];
			  reserved_group.Line_Directory_Level[reserved_group.Modbule_Group_number] = lSwitch_Modgroup_list->Line_Directory_Level[i];
			  reserved_group.Modbule_Group_number++;
			}
		}
		//针对需要退出的模块提交退出申请
	  for(j=0;j<removed_group.Modbule_Group_number;j++)
		{
			Mod_GroupNO = Gets_modbule_Group_No(removed_group.Modbule_Group_Adrr[j]);//模块组号对应的在参数中的序列号
			if(ALLOCATION_DEBUG) printf(" Mod_GroupNO =%d removed_group.Modbule_Group_Adrr[j] =%d \n",Mod_GroupNO,removed_group.Modbule_Group_Adrr[j]);		
			Power_Exit_Fun(Mod_GroupNO,removed_group.Contactor_No[j]);	
		  ChargeProcesLogOut(Apply_Ccu_no,"%d#ChargeProces: 针对需要退出的模块提交退出申请 需要退出的配置中模块组序号 =%d, 接触器号 =%d,",Apply_Ccu_no + 1,Mod_GroupNO,removed_group.Contactor_No[j]);
		}
		//重新赋值
    if(reserved_group.Modbule_Group_number != 0){
	    memcpy(&lSwitch_Modgroup_list->reserve,&reserved_group.reserve,sizeof(SWITCH_MODGROUP_LIST));
		}
	}
}

//申请激活模块
void Activate_Group(UINT8 Group_No)
{
	if(Globa->Sys_MODGroup_Control_Data[Group_No].Sys_MobuleGrp_State  == MOD_GROUP_STATE_INACTIVE)
	{
	  Allocate_proces_Activate_Group(Group_No);
  }
}
//初始化数据配置
void ModGroup_data_init()//根据系统配置关系选择数据
{
	int i =0;
	
	if(Globa->Charger_param.gun_config == 1)
	{
			Globa->Group_Modules_Num = 1;
			for(i =0;i<Globa->Group_Modules_Num;i++)
			{
				Globa->Charger_gunbus_param_data[i].Data_valid = 1; //有效位
				Globa->Charger_gunbus_param_data[i].Bus_no = (i+1);//母线号
				Globa->Charger_gunbus_param_data[i].Bus_to_mod_Gno = (i+1);//母线固定模块组号
				Globa->Charger_gunbus_param_data[i].Bus_to_ccu_no = (i+1);//母线对应的枪号 ---- (-1则无CCU)，不连接枪号
				Globa->Charger_gunbus_param_data[i].Bus_max_current = 250;//母线最高充电电流
			}
	}
	else if(Globa->Charger_param.gun_config == 2)//普通的2枪
	{
		if(Globa->Charger_param.support_double_gun_power_allocation == 0)
		{
					for(i=0;i<Globa->Charger_param.Contactor_Line_Num;i++)
					{
						Globa->Charger_Contactor_param_data[i].Data_valid = 1; //有效范位
						Globa->Charger_Contactor_param_data[i].contactor_no = (i+1);//接触器编号 ---就是两个母线之间的连接线路
						Globa->Charger_Contactor_param_data[i].contactor_bus_P = (Globa->Charger_param.Contactor_Line_Position[i]>>4)&0x0f;//正向关联母线号(目前母线号和组号是一一对应的)
						Globa->Charger_Contactor_param_data[i].contactor_bus_N = Globa->Charger_param.Contactor_Line_Position[i]&0x0f;//反向关联母线号 （代表那两个CCU可以并机在一起）
						Globa->Charger_Contactor_param_data[i].contactor_maxcurrent = 250;//接触器额定电流
						Globa->Charger_Contactor_param_data[i].allow_reverse = 1;//允许反向标记(反向并机)
						Globa->Charger_Contactor_param_data[i].Correspond_control_unit_address  = (Globa->Charger_param.Contactor_Line_Position[i]>>4)&0x0f;//对应控制单元逻辑位号地址(IO）---A枪控制及检测
						Globa->Charger_Contactor_param_data[i].Correspond_control_unit_no = 0;//对应控制单元逻辑位号地址(IO）				
					}
					Globa->Group_Modules_Num = 2;
					for(i =0;i<Globa->Group_Modules_Num;i++)
					{
						Globa->Charger_gunbus_param_data[i].Data_valid = 1; //有效位
						Globa->Charger_gunbus_param_data[i].Bus_no = (i+1);//母线号
						Globa->Charger_gunbus_param_data[i].Bus_to_mod_Gno = (i+1);//母线固定模块组号
						Globa->Charger_gunbus_param_data[i].Bus_to_ccu_no = (i+1);//母线对应的枪号 ---- (-1则无CCU)，不连接枪号
						Globa->Charger_gunbus_param_data[i].Bus_max_current = 250;//母线最高充电电流
					}
		}else{//带功率分配的时候则有2把枪4个控制板进行模块控制(A,B枪)， A-C-D-B
				for(i=0;i<Globa->Charger_param.Contactor_Line_Num;i++)
				{
					Globa->Charger_Contactor_param_data[i].Data_valid = 1; //有效范位
					Globa->Charger_Contactor_param_data[i].contactor_no = (i+1);//接触器编号 ---就是两个母线之间的连接线路
					Globa->Charger_Contactor_param_data[i].contactor_bus_P = (Globa->Charger_param.Contactor_Line_Position[i]>>4)&0x0f;//正向关联母线号(目前母线号和组号是一一对应的)
					Globa->Charger_Contactor_param_data[i].contactor_bus_N = Globa->Charger_param.Contactor_Line_Position[i]&0x0f;//反向关联母线号 （代表那两个CCU可以并机在一起）
					Globa->Charger_Contactor_param_data[i].contactor_maxcurrent = 250;//接触器额定电流
					Globa->Charger_Contactor_param_data[i].allow_reverse = 1;//允许反向标记(反向并机)
					Globa->Charger_Contactor_param_data[i].Correspond_control_unit_address  = (Globa->Charger_param.Contactor_Line_Position[i]>>4)&0x0f;//对应控制单元逻辑位号地址(IO）---A枪控制及检测
					Globa->Charger_Contactor_param_data[i].Correspond_control_unit_no = 0;//对应控制单元逻辑位号地址(IO）				
				}
			
				Globa->Group_Modules_Num = 4;
				for(i =0;i<Globa->Group_Modules_Num;i++)
				{
					Globa->Charger_gunbus_param_data[i].Data_valid = 1; //有效位
					Globa->Charger_gunbus_param_data[i].Bus_no = (i+1);//母线号
					Globa->Charger_gunbus_param_data[i].Bus_to_mod_Gno = (i+1);//母线固定模块组号
					Globa->Charger_gunbus_param_data[i].Bus_to_ccu_no = (i+1);//母线对应的枪号 ---- (-1则无CCU)，不连接枪号
					Globa->Charger_gunbus_param_data[i].Bus_max_current = 250;//母线最高充电电流
				}
	  }
	}else	if(Globa->Charger_param.gun_config == 3)//普通的3枪 A-B-C-A
	{

			for(i=0;i<Globa->Charger_param.Contactor_Line_Num;i++)
			{
				Globa->Charger_Contactor_param_data[i].Data_valid = 1; //有效范位
				Globa->Charger_Contactor_param_data[i].contactor_no = (i+1);//接触器编号 ---就是两个母线之间的连接线路
				Globa->Charger_Contactor_param_data[i].contactor_bus_P = (Globa->Charger_param.Contactor_Line_Position[i]>>4)&0x0f;//正向关联母线号(目前母线号和组号是一一对应的)
				Globa->Charger_Contactor_param_data[i].contactor_bus_N = Globa->Charger_param.Contactor_Line_Position[i]&0x0f;//反向关联母线号 （代表那两个CCU可以并机在一起）
				Globa->Charger_Contactor_param_data[i].contactor_maxcurrent = 250;//接触器额定电流
				Globa->Charger_Contactor_param_data[i].allow_reverse = 1;//允许反向标记(反向并机)
				Globa->Charger_Contactor_param_data[i].Correspond_control_unit_address  = (Globa->Charger_param.Contactor_Line_Position[i]>>4)&0x0f;//对应控制单元逻辑位号地址(IO）---A枪控制及检测
				Globa->Charger_Contactor_param_data[i].Correspond_control_unit_no = 0;//对应控制单元逻辑位号地址(IO）				
			}
		
			Globa->Group_Modules_Num = 3;
			for(i =0;i<Globa->Group_Modules_Num;i++)
			{
				Globa->Charger_gunbus_param_data[i].Data_valid = 1; //有效位
				Globa->Charger_gunbus_param_data[i].Bus_no = (i+1);//母线号
				Globa->Charger_gunbus_param_data[i].Bus_to_mod_Gno = (i+1);//母线固定模块组号
				Globa->Charger_gunbus_param_data[i].Bus_to_ccu_no = (i+1);//母线对应的枪号 ---- (-1则无CCU)，不连接枪号
				Globa->Charger_gunbus_param_data[i].Bus_max_current = 250;//母线最高充电电流
			}
	}else	if(Globa->Charger_param.gun_config == 4)//普通的4枪 A-C-D-B-A
	{
			for(i=0;i<Globa->Charger_param.Contactor_Line_Num;i++)
			{
				Globa->Charger_Contactor_param_data[i].Data_valid = 1; //有效范位
				Globa->Charger_Contactor_param_data[i].contactor_no = (i+1);//接触器编号 ---就是两个母线之间的连接线路
				Globa->Charger_Contactor_param_data[i].contactor_bus_P = (Globa->Charger_param.Contactor_Line_Position[i]>>4)&0x0f;//正向关联母线号(目前母线号和组号是一一对应的)
				Globa->Charger_Contactor_param_data[i].contactor_bus_N = Globa->Charger_param.Contactor_Line_Position[i]&0x0f;//反向关联母线号 （代表那两个CCU可以并机在一起）
				Globa->Charger_Contactor_param_data[i].contactor_maxcurrent = 250;//接触器额定电流
				Globa->Charger_Contactor_param_data[i].allow_reverse = 1;//允许反向标记(反向并机)
				Globa->Charger_Contactor_param_data[i].Correspond_control_unit_address  = (Globa->Charger_param.Contactor_Line_Position[i]>>4)&0x0f;//对应控制单元逻辑位号地址(IO）---A枪控制及检测
				Globa->Charger_Contactor_param_data[i].Correspond_control_unit_no = 0;//对应控制单元逻辑位号地址(IO）				
			}
			Globa->Group_Modules_Num = 4;
			for(i =0;i<Globa->Group_Modules_Num;i++)
			{
				Globa->Charger_gunbus_param_data[i].Data_valid = 1; //有效位
				Globa->Charger_gunbus_param_data[i].Bus_no = (i+1);//母线号
				Globa->Charger_gunbus_param_data[i].Bus_to_mod_Gno = (i+1);//母线固定模块组号
				Globa->Charger_gunbus_param_data[i].Bus_to_ccu_no = (i+1);//母线对应的枪号 ---- (-1则无CCU)，不连接枪号
				Globa->Charger_gunbus_param_data[i].Bus_max_current = 250;//母线最高充电电流
			}
	}else	if(Globa->Charger_param.gun_config == 5)//普通的3枪 A-C--E-D-B-A
	{
	
			for(i=0;i<Globa->Charger_param.Contactor_Line_Num;i++)
			{
				Globa->Charger_Contactor_param_data[i].Data_valid = 1; //有效范位
				Globa->Charger_Contactor_param_data[i].contactor_no = (i+1);//接触器编号 ---就是两个母线之间的连接线路
				Globa->Charger_Contactor_param_data[i].contactor_bus_P = (Globa->Charger_param.Contactor_Line_Position[i]>>4)&0x0f;//正向关联母线号(目前母线号和组号是一一对应的)
				Globa->Charger_Contactor_param_data[i].contactor_bus_N = Globa->Charger_param.Contactor_Line_Position[i]&0x0f;//反向关联母线号 （代表那两个CCU可以并机在一起）
				Globa->Charger_Contactor_param_data[i].contactor_maxcurrent = 250;//接触器额定电流
				Globa->Charger_Contactor_param_data[i].allow_reverse = 1;//允许反向标记(反向并机)
				Globa->Charger_Contactor_param_data[i].Correspond_control_unit_address  = (Globa->Charger_param.Contactor_Line_Position[i]>>4)&0x0f;//对应控制单元逻辑位号地址(IO）---A枪控制及检测
				Globa->Charger_Contactor_param_data[i].Correspond_control_unit_no = 0;//对应控制单元逻辑位号地址(IO）				
			}
		
			Globa->Group_Modules_Num = 5;
			for(i =0;i<Globa->Group_Modules_Num;i++)
			{
				Globa->Charger_gunbus_param_data[i].Data_valid = 1; //有效位
				Globa->Charger_gunbus_param_data[i].Bus_no = (i+1);//母线号
				Globa->Charger_gunbus_param_data[i].Bus_to_mod_Gno = (i+1);//母线固定模块组号
				Globa->Charger_gunbus_param_data[i].Bus_to_ccu_no = (i+1);//母线对应的枪号 ---- (-1则无CCU)，不连接枪号
				Globa->Charger_gunbus_param_data[i].Bus_max_current = 250;//母线最高充电电流
			}
	}
}

 //初始化每组CCU数，模块组，继电器等，第一级目录关联关系
void ModGroup_Diagram_list_init()
{
  UINT32 i =0,j=0,Modbule_Group_NO_P =0,Modbule_Group_NO_N =0;

	for(i=0;i<Globa->Group_Modules_Num;i++)
	{
		gGroup_Diagram_list[i].Modbule_Group_NO = Globa->Charger_gunbus_param_data[i].Bus_to_mod_Gno;//参数中的组号
		
		for(j = 0;j<Globa->Charger_param.Contactor_Line_Num;j++)
	  {

			Modbule_Group_NO_P = Gets_ModGroup_number(Globa->Charger_Contactor_param_data[j].contactor_bus_P);
			Modbule_Group_NO_N = Gets_ModGroup_number(Globa->Charger_Contactor_param_data[j].contactor_bus_N);
		//  printf("---------------ModGroup_Diagram_list_init--------j=%d  Modbule_Group_NO_P =%d Modbule_Group_NO_N  =%d\n\r", j,Modbule_Group_NO_P,Modbule_Group_NO_N);

			if(gGroup_Diagram_list[i].Modbule_Group_NO == Modbule_Group_NO_P)
			{
				gGroup_Diagram_list[i].diagram_Modbule_Group_Adrr[gGroup_Diagram_list[i].diagram_number] = Modbule_Group_NO_N;
				gGroup_Diagram_list[i].diagram_Contactor_No[gGroup_Diagram_list[i].diagram_number] = Globa->Charger_Contactor_param_data[j].contactor_no;
				gGroup_Diagram_list[i].diagram_number ++;
			}
			else if(gGroup_Diagram_list[i].Modbule_Group_NO == Modbule_Group_NO_N)
			{
				gGroup_Diagram_list[i].diagram_Modbule_Group_Adrr[gGroup_Diagram_list[i].diagram_number] = Modbule_Group_NO_P;
				gGroup_Diagram_list[i].diagram_Contactor_No[gGroup_Diagram_list[i].diagram_number] = Globa->Charger_Contactor_param_data[j].contactor_no;
				gGroup_Diagram_list[i].diagram_number ++;
			}
	  }
	}
}

//筛选合适的点出来，反馈为讯号。
int Modgroup_Wait_Selecton(UINT8 Need_Number,MODGROUP_WAIT_SELECTION_LIST*lsWait_Selecton_list_data)
{
	int wait_NO = -1,i=0;
	if(lsWait_Selecton_list_data->WaitSelectionNum == 0) return -1;
	for(i = 0;i<lsWait_Selecton_list_data->WaitSelectionNum;i++)
	{
		if((1 == lsWait_Selecton_list_data->WaitWiaitSelectionData[i].Priority_falg)&&(lsWait_Selecton_list_data->WaitWiaitSelectionData[i].Data_falg == 1))
		{
			wait_NO = i;
			break;
		}else if((Need_Number >= lsWait_Selecton_list_data->WaitWiaitSelectionData[i].modbule_number)&&(lsWait_Selecton_list_data->WaitWiaitSelectionData[i].Data_falg == 1))
		{
			wait_NO = i;
			break;
		}
	}
	
	if(i>= lsWait_Selecton_list_data->WaitSelectionNum)//说明一直没有找到则返回上一个
	{
		if((lsWait_Selecton_list_data->WaitWiaitSelectionData[lsWait_Selecton_list_data->WaitSelectionNum -1].Data_falg == 1))
		{
			wait_NO = lsWait_Selecton_list_data->WaitSelectionNum -1;
		}
	}
	
	return wait_NO;
	
}
/****************************申请功率*************
1.判断可投切的条件：
1.该模块组不上全部通讯异常(投切的时候可以不投切，但是退出的可以不退出)
2.该模块组是空闲状态
3.该模块组对应的线路无异常情况
4.该模块组对应的CCU枪连接状态为未连接
5.优先关系，在同级目录下优先并符合的模块数量--by1129
Apply_Ccu_no :枪号申请
Sys_MobuleGrp_Gun:需要并过去的模块组号
Need_Number：需要数量
lSwitch_Modgroup_list :原先已经投切的列表数据 
SWITCH_MODGROUP_WIAIT_SELECTION_LIST;//带选择投切目录，就是从一级目录中找到相等的目录，如果没有找到则插入到这个预选目录中。
***************************************************/
void Allocation_Power_Applied_Function(UINT32 Apply_Ccu_no,UINT8 Need_Number,SWITCH_MODGROUP_LIST *lSwitch_Modgroup_list)
{
	
	int i =0,j=0,k =0,z=0,Wait_Selecton_NO = -1,busccu_no = 0,Sys_MobuleGrp_NO = 0,gun_ccu_no = 0,Bus_position_no = 0,Gun_Charged_State = 0,Gunlink = 0,Gun_Output_Relay_State = 0,Errorsystem = 0,MC_connect_lost_falut = 0;
	UINT8 NeedNumber = Need_Number,MobuleGrp_NO = 0,MobuleGrpNO = 0,Priority_falg = 0;
	int Priority_Gun_Charged_State = 0,Priority_Gun_Output_Relay_State= 0,Priority_Gunlink = 0,Priority_Errorsystem = 0,Priority_MC_connect_lost_falut =  0,Priority_gun_no=0;
	MODGROUP_WAIT_SELECTION_LIST Modgroup_Wait_Selecton_List;
  WAIT_SELECTION_DATA  Wait_Selecton_data;

	UINT32 Modbule_Group_NO = Gets_busno_mod_Gno_number(Apply_Ccu_no);//ccu 对应的母线直连的模块组号
	UINT32 Switch_Mod_GroupNO = Gets_modbule_Group_No(Modbule_Group_NO);//模块组号对应的在参数中的序列号,改变的组号就是这个序列号的组号,需要切换到的组序号
	if(NeedNumber == 0) return;

	//先从关系列表中查询第一级目录
	for(j =0;j<lSwitch_Modgroup_list->Modbule_Group_number;j++)
	{
		MobuleGrp_NO = Gets_modbule_Group_No(lSwitch_Modgroup_list->Modbule_Group_Adrr[j]);
		//MobuleGrp_NO = lSwitch_Modgroup_list->Modbule_Group_Adrr[j] - 1;
#ifdef WAITSELECTIONCONFIG		
		memset(&Modgroup_Wait_Selecton_List,0,sizeof(MODGROUP_WAIT_SELECTION_LIST));//清楚关系列表
		memset(&Wait_Selecton_data,0,sizeof(WAIT_SELECTION_DATA));//清楚关系列表
#endif 
		for(i=0;i<gGroup_Diagram_list[MobuleGrp_NO].diagram_number;i++)//一级目录
		{
		//	gun_ccu_no = gGroup_Diagram_list[MobuleGrp_NO].diagram_Modbule_Group_Adrr[i] - 1;
		  if(ALLOCATION_DEBUG) printf("\n 11--i= %d :gGroup_Diagram_list[%d].diagram_number =%d,gGroup_Diagram_list[MobuleGrp_NO].diagram_Modbule_Group_Adrr[i] =%d  \n",i,MobuleGrp_NO,gGroup_Diagram_list[MobuleGrp_NO].diagram_number,gGroup_Diagram_list[MobuleGrp_NO].diagram_Modbule_Group_Adrr[i]);		

			MobuleGrpNO = Gets_modbule_Group_No(gGroup_Diagram_list[MobuleGrp_NO].diagram_Modbule_Group_Adrr[i]);

			busccu_no = Gets_busccu_number(gGroup_Diagram_list[MobuleGrp_NO].diagram_Modbule_Group_Adrr[i]);//母线组号
		
  	  Bus_position_no = Gets_busno_Gno_No(busccu_no);//获取该母线号对应的参数位置序号
			if(Globa->Charger_gunbus_param_data[Bus_position_no].Bus_to_ccu_no != -1){
			   gun_ccu_no = Globa->Charger_gunbus_param_data[Bus_position_no].Bus_to_ccu_no -1;//换成枪的序号= 枪号-1；//by 0808
			  
				 Gunlink = 0;				 
				 Gun_Charged_State =  Globa->Sys_RevCcu_Data[gun_ccu_no].Charge_State; //开启充电
			   Gun_Output_Relay_State = (((Globa->Sys_RevCcu_Data[gun_ccu_no].Relay_State>>4)&0x0f) == 0)? 0:1; //对应的输出接触器

				Errorsystem = Globa->Sys_RevCcu_Data[gun_ccu_no].Errorsystem;
				MC_connect_lost_falut = Globa->Control_DC_Infor_Data[gun_ccu_no].Error.ctrl_connect_lost;
			}else{//表示没有挂载枪CCU号
			  gun_ccu_no = -1;
				Gunlink = 0;
				Errorsystem = 0;
				MC_connect_lost_falut = 0;
				Gun_Charged_State = 0;
				Gun_Output_Relay_State = 0;
			}
	
			Priority_Gun_Charged_State = 0;
			Priority_Errorsystem = 0;
			Priority_MC_connect_lost_falut =  0;
			Priority_Gun_Output_Relay_State = 0;
			Priority_Gunlink = 0;
			Priority_falg = 0;
			
			
		  if(ALLOCATION_DEBUG) printf("i=%d   gGroup_Diagram_list[MobuleGrp_NO].diagram_number =%d -start \n",i,gGroup_Diagram_list[MobuleGrp_NO].diagram_number);
			if(ALLOCATION_DEBUG) printf("\n -0- MobuleGrpNO =%d Globa->Sys_MODGroup_Control_Data[MobuleGrpNO].Sys_MobuleGrp_State =%d \n",MobuleGrpNO,Globa->Sys_MODGroup_Control_Data[MobuleGrpNO].Sys_MobuleGrp_State);		
			if(ALLOCATION_DEBUG) printf("\n -1-Globa->Sys_MODGroup_Control_Data[MobuleGrpNO].Sys_Grp_OKNumber  =%d  \n",Globa->Sys_MODGroup_Control_Data[MobuleGrpNO].Sys_Grp_OKNumber);		
			
			if(ALLOCATION_DEBUG) printf("\n -2-MobuleGrp_NO=%d   gGroup_Diagram_list[MobuleGrp_NO].diagram_Contactor_No[i] =%d ISContactor_ok =%d  \n",MobuleGrp_NO,gGroup_Diagram_list[MobuleGrp_NO].diagram_Contactor_No[i],ISContactor_ok(gGroup_Diagram_list[MobuleGrp_NO].diagram_Contactor_No[i]));		
			
			if(ALLOCATION_DEBUG) printf("\n -3-Gunlink =%d, Gun_Charged_State=%d   Gun_Output_Relay_State  =%d MC_connect_lost_falut =%d Errorsystem =%d \n",Gunlink,Gun_Charged_State,Gun_Output_Relay_State, MC_connect_lost_falut,Errorsystem);		
			if(ALLOCATION_DEBUG) printf("\n -4-Priority_Gunlink =%d, Priority_Gun_Charged_State=%d   Priority_Gun_Output_Relay_State  =%d Priority_MC_connect_lost_falut =%d Priority_Errorsystem =%d \n",Priority_Gunlink,Priority_Gun_Charged_State,Priority_Gun_Output_Relay_State, Priority_MC_connect_lost_falut,Priority_Errorsystem);		
			if(ALLOCATION_DEBUG) printf("\n -5- MobuleGrpNO =%d  Globa->Sys_MODGroup_Control_Data[MobuleGrpNO].Control_Timeout_Count =%d  Globa->Operated_Relay_NO_Fault_Count[%d]  =%d \n",MobuleGrpNO,Globa->Sys_MODGroup_Control_Data[MobuleGrpNO].Control_Timeout_Count,gGroup_Diagram_list[MobuleGrp_NO].diagram_Contactor_No[i] - 1,Globa->Operated_Relay_NO_Fault_Count[gGroup_Diagram_list[MobuleGrp_NO].diagram_Contactor_No[i] - 1]);		

			if((Globa->Sys_MODGroup_Control_Data[MobuleGrpNO].Sys_MobuleGrp_State <= MOD_GROUP_STATE_ACTIVE)
				&&((Globa->Sys_MODGroup_Control_Data[MobuleGrpNO].Sys_Grp_OKNumber != 0)||(Globa->Sys_MODGroup_Control_Data[MobuleGrpNO].Sys_MobuleGrp_State == MOD_GROUP_STATE_INACTIVE))
				&&(ISContactor_ok(gGroup_Diagram_list[MobuleGrp_NO].diagram_Contactor_No[i]) == 1)
				&&(Gunlink == 0)&&(Gun_Charged_State == 0)&&(Gun_Output_Relay_State == 0)&&(MC_connect_lost_falut == 0)&&(Errorsystem == 0)
			  &&(Priority_Gunlink == 0)&&(Priority_Gun_Charged_State == 0)&&(Priority_Gun_Output_Relay_State == 0)&&(Priority_MC_connect_lost_falut == 0)&&(Priority_Errorsystem == 0)
				&&(Globa->Operated_Relay_NO_Fault_Count[gGroup_Diagram_list[MobuleGrp_NO].diagram_Contactor_No[i] - 1] <= RELAY_NO_MAXFAULT_COUNT)
				&&(Globa->Sys_MODGroup_Control_Data[MobuleGrpNO].Control_Timeout_Count <= SWITCHTIMEOUT_COUNT))
			{//判断其条件是否满足
				if(ALLOCATION_DEBUG) printf("\n 判断其条件满足\n");

#ifdef WAITSELECTIONCONFIG
				Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Modgroup_Wait_Selecton_List.WaitSelectionNum].Data_falg = 1;
				Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Modgroup_Wait_Selecton_List.WaitSelectionNum].Priority_falg = Priority_falg;
				Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Modgroup_Wait_Selecton_List.WaitSelectionNum].MobuleGrp_NO = MobuleGrp_NO;
				Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Modgroup_Wait_Selecton_List.WaitSelectionNum].MobuleGrpNO = MobuleGrpNO;
			  if((Globa->Sys_MODGroup_Control_Data[MobuleGrpNO].Sys_MobuleGrp_State == MOD_GROUP_STATE_ACTIVE)){
				  Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Modgroup_Wait_Selecton_List.WaitSelectionNum].modbule_number = Globa->Sys_MODGroup_Control_Data[MobuleGrpNO].Sys_Grp_OKNumber;
				}else if((Globa->Sys_MODGroup_Control_Data[MobuleGrpNO].Sys_MobuleGrp_State == MOD_GROUP_STATE_INACTIVE)){//未激活不知道模块通讯数量
				  Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Modgroup_Wait_Selecton_List.WaitSelectionNum].modbule_number = Globa->Charger_param.Charge_rate_number[MobuleGrpNO];
				}
				Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Modgroup_Wait_Selecton_List.WaitSelectionNum].Modbule_Group_Adrr = gGroup_Diagram_list[MobuleGrp_NO].diagram_Modbule_Group_Adrr[i];
				Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Modgroup_Wait_Selecton_List.WaitSelectionNum].Contactor_No = gGroup_Diagram_list[MobuleGrp_NO].diagram_Contactor_No[i];
				Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Modgroup_Wait_Selecton_List.WaitSelectionNum].Line_Directory_Level = lSwitch_Modgroup_list->Line_Directory_Level[j] + 1;
				Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Modgroup_Wait_Selecton_List.WaitSelectionNum].Sys_MobuleGrp_State = MOD_GROUP_STATE_ORDER;
        Modgroup_Wait_Selecton_List.WaitSelectionNum++;


#else				
				if((Globa->Sys_MODGroup_Control_Data[MobuleGrpNO].Sys_MobuleGrp_State == MOD_GROUP_STATE_ACTIVE)){
					if(NeedNumber <= Globa->Sys_MODGroup_Control_Data[MobuleGrpNO].Sys_Grp_OKNumber)
					{
						NeedNumber = 0;
					}else{
						NeedNumber = NeedNumber - Globa->Sys_MODGroup_Control_Data[MobuleGrpNO].Sys_Grp_OKNumber;
				  }
				}else if((Globa->Sys_MODGroup_Control_Data[MobuleGrpNO].Sys_MobuleGrp_State == MOD_GROUP_STATE_INACTIVE)){//未激活不知道模块通讯数量
					if(NeedNumber <= Globa->Charger_param.Charge_rate_number[MobuleGrpNO])
					{
						NeedNumber = 0;
					}else{
						NeedNumber = NeedNumber - Globa->Charger_param.Charge_rate_number[MobuleGrpNO];
				  }
				}
				lSwitch_Modgroup_list->Modbule_Group_Adrr[lSwitch_Modgroup_list->Modbule_Group_number] = gGroup_Diagram_list[MobuleGrp_NO].diagram_Modbule_Group_Adrr[i];
				lSwitch_Modgroup_list->Contactor_No[lSwitch_Modgroup_list->Modbule_Group_number] = gGroup_Diagram_list[MobuleGrp_NO].diagram_Contactor_No[i];
				lSwitch_Modgroup_list->Line_Directory_Level[lSwitch_Modgroup_list->Modbule_Group_number] = lSwitch_Modgroup_list->Line_Directory_Level[j] + 1;
				lSwitch_Modgroup_list->Modbule_Group_number++;
				Globa->Sys_MODGroup_Control_Data[MobuleGrpNO].Sys_MobuleGrp_State = MOD_GROUP_STATE_ORDER;//置被预定状态-防止再次被其他分配掉
				Power_Switch_Fun(MobuleGrpNO,Switch_Mod_GroupNO, gGroup_Diagram_list[MobuleGrp_NO].diagram_Contactor_No[i]);
				if(ALLOCATION_DEBUG) printf("\n MobuleGrpNO =%d NeedNumber=%d  Switch_Mod_GroupNO =%d \n",MobuleGrpNO,NeedNumber,Switch_Mod_GroupNO);	
		  	if(NeedNumber == 0)
				{
					break;
				}
#endif				
			}else{
				if(ALLOCATION_DEBUG) printf("\n 判断其条件bu 满足\n");		
			}
		}
#ifdef WAITSELECTIONCONFIG
   //对已经入选的列表进行排列，按照模块的数量进行排序。
	 	for(k=0;k<Modgroup_Wait_Selecton_List.WaitSelectionNum;k++)//冒泡排序,大的再数组前面，小的往后面
		{
			for(z=0;z<(Modgroup_Wait_Selecton_List.WaitSelectionNum - k- 1);z++)
			{
				if((Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z].modbule_number < Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z + 1].modbule_number)||
				 (Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z].Priority_falg < Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z + 1].Priority_falg))//优先级先启动
				{
					Wait_Selecton_data.Data_falg =  Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z + 1].Data_falg;
					Wait_Selecton_data.Priority_falg =  Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z + 1].Priority_falg;

					Wait_Selecton_data.MobuleGrp_NO = Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z + 1].MobuleGrp_NO ;
					Wait_Selecton_data.MobuleGrpNO =  Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z + 1].MobuleGrpNO;
					Wait_Selecton_data.modbule_number = 	Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z + 1].modbule_number;
					Wait_Selecton_data.Modbule_Group_Adrr = Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z + 1].Modbule_Group_Adrr;
					Wait_Selecton_data.Contactor_No = Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z + 1].Contactor_No;
					Wait_Selecton_data.Line_Directory_Level = Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z + 1].Line_Directory_Level;
					Wait_Selecton_data.Sys_MobuleGrp_State = Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z + 1].Sys_MobuleGrp_State;
					
					Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z + 1].Data_falg =  Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z].Data_falg;
					Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z + 1].Priority_falg =  Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z].Priority_falg;

					Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z + 1].MobuleGrp_NO = Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z].MobuleGrp_NO ;
					Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z + 1].MobuleGrpNO = Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z].MobuleGrpNO;
				  Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z + 1].modbule_number = Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z].modbule_number;
					Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z + 1].Modbule_Group_Adrr = Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z].Modbule_Group_Adrr;
					Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z + 1].Contactor_No = Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z].Contactor_No;
				  Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z + 1].Line_Directory_Level = Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z].Line_Directory_Level;
					Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z + 1].Sys_MobuleGrp_State = 	Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z].Sys_MobuleGrp_State;
					
					Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z].Data_falg =  Wait_Selecton_data.Data_falg;
					Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z].Priority_falg =  Wait_Selecton_data.Priority_falg;

					Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z].MobuleGrp_NO = Wait_Selecton_data.MobuleGrp_NO;
					Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z].MobuleGrpNO = Wait_Selecton_data.MobuleGrpNO;
				  Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z].modbule_number = Wait_Selecton_data.modbule_number;
					Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z].Modbule_Group_Adrr = Wait_Selecton_data.Modbule_Group_Adrr;
					Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z].Contactor_No = Wait_Selecton_data.Contactor_No;
				  Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z].Line_Directory_Level = Wait_Selecton_data.Line_Directory_Level;
					Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[z].Sys_MobuleGrp_State = Wait_Selecton_data.Sys_MobuleGrp_State;
				}	
			}
		}
		
		if(ALLOCATION_DEBUG)
		{				
			 printf("WAITSELECTIONCONFIG：Switch_Mod_GroupNO = %d  lSwitch_Modgroup_list->Modbule_Group_number =%d , Modgroup_Wait_Selecton_List.WaitSelectionNum =%d \n",Switch_Mod_GroupNO,lSwitch_Modgroup_list->Modbule_Group_number ,Modgroup_Wait_Selecton_List.WaitSelectionNum);
			 for(k=0;k<Modgroup_Wait_Selecton_List.WaitSelectionNum;k++)//冒泡排序,大的再数组前面，小的往后面
			{
				printf("WAITSELECTIONCONFIG：1----k=%d  Data_falg =%d  Priority_falg=%d   WaitWiaitSelectionData[k].MobuleGrp_NO =%d  .WaitWiaitSelectionData[k].MobuleGrpNO =%d  .WaitWiaitSelectionData[k].modbule_number  =%d \n ",k,Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[k].Data_falg,Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[k].Priority_falg,Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[k].MobuleGrp_NO,Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[k].MobuleGrpNO,Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[k].modbule_number);
				printf("WAITSELECTIONCONFIG：2----k=%d  WaitWiaitSelectionData[k].Modbule_Group_Adrr =%d  .WaitWiaitSelectionData[k].Contactor_No =%d  .WaitWiaitSelectionData[k].Line_Directory_Level  =%d  Sys_MobuleGrp_State =%d \n ",k,Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[k].Modbule_Group_Adrr,Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[k].Contactor_No,Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[k].Line_Directory_Level,Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[k].Sys_MobuleGrp_State);
				printf("\n");
			}
		}
		
	 	for(k=0;k<Modgroup_Wait_Selecton_List.WaitSelectionNum;k++){
		  if(ALLOCATION_DEBUG) printf("   k= %d NeedNumber = %d \n",k,NeedNumber);
      Wait_Selecton_NO = Modgroup_Wait_Selecton(NeedNumber,&Modgroup_Wait_Selecton_List);
		  if(ALLOCATION_DEBUG) printf("  k= %d Wait_Selecton_NO = %d \n",k,Wait_Selecton_NO);

  	  if(Wait_Selecton_NO != -1)
		  {
				if(NeedNumber <= Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Wait_Selecton_NO].modbule_number)
				{
					NeedNumber = 0;
				}else{
					NeedNumber = NeedNumber - Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Wait_Selecton_NO].modbule_number;
				}
			  if(ALLOCATION_DEBUG)	printf("：Wait_Selecton_NO =%d 1--- Data_falg =%d  Priority_falg=%d   WaitWiaitSelectionData[k].MobuleGrp_NO =%d  .WaitWiaitSelectionData[k].MobuleGrpNO =%d  .WaitWiaitSelectionData[k].modbule_number  =%d \n ",Wait_Selecton_NO,Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Wait_Selecton_NO].Data_falg,Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Wait_Selecton_NO].Priority_falg,Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Wait_Selecton_NO].MobuleGrp_NO,Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Wait_Selecton_NO].MobuleGrpNO,Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Wait_Selecton_NO].modbule_number);
			  if(ALLOCATION_DEBUG)	printf("：Wait_Selecton_NO =%d 2--- WaitWiaitSelectionData[k].Modbule_Group_Adrr =%d  .WaitWiaitSelectionData[k].Contactor_No =%d  .WaitWiaitSelectionData[k].Line_Directory_Level  =%d  Sys_MobuleGrp_State =%d \n ",Wait_Selecton_NO,Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Wait_Selecton_NO].Modbule_Group_Adrr,Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Wait_Selecton_NO].Contactor_No,Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Wait_Selecton_NO].Line_Directory_Level,Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Wait_Selecton_NO].Sys_MobuleGrp_State);
			  
				
				lSwitch_Modgroup_list->Modbule_Group_Adrr[lSwitch_Modgroup_list->Modbule_Group_number] = Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Wait_Selecton_NO].Modbule_Group_Adrr;
				lSwitch_Modgroup_list->Contactor_No[lSwitch_Modgroup_list->Modbule_Group_number]  = Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Wait_Selecton_NO].Contactor_No;
				lSwitch_Modgroup_list->Line_Directory_Level[lSwitch_Modgroup_list->Modbule_Group_number] = Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Wait_Selecton_NO].Line_Directory_Level;
	
				Globa->Sys_MODGroup_Control_Data[Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Wait_Selecton_NO].MobuleGrpNO].Sys_MobuleGrp_State = Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Wait_Selecton_NO].Sys_MobuleGrp_State;
			 	lSwitch_Modgroup_list->Modbule_Group_number++;
			 	Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Wait_Selecton_NO].Data_falg = 0;//代表已经被用了
				Power_Switch_Fun(Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Wait_Selecton_NO].MobuleGrpNO,Switch_Mod_GroupNO,Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Wait_Selecton_NO].Contactor_No);
				if(ALLOCATION_DEBUG) printf("\n WAITSELECTIONCONFIG：MobuleGrpNO =%d  Switch_Mod_GroupNO =%d NeedNumber=%d Contactor_No=%d \n",Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Wait_Selecton_NO].MobuleGrpNO,Switch_Mod_GroupNO,NeedNumber,Modgroup_Wait_Selecton_List.WaitWiaitSelectionData[Wait_Selecton_NO].Contactor_No);	
		  	if(NeedNumber == 0)
				{
					break;
				}
		  }
		}
#endif
		if(NeedNumber == 0)
		{
			break;
		}
  }
}

//查看该组是否存在下级目录，
//判断条件为：该组模块组相关联的继电号刚好存在继电器号序列中需要大于1，
UINT8 IS_Subordinate_Directory(UINT32 Modgroup,UINT8 number, UINT8 *Contactor_No)
{
	UINT32 i =0;
	 char contactor_bus[2] = {0};
	 char count = 0;
	 for(i=0;i<number;i++)
	 {
		  if(Contactor_No[i] != 0)
		  {
			  if(Gets_BUS_number(Contactor_No[i],contactor_bus))
			  {
					if((Gets_ModGroup_number(contactor_bus[0]) == Modgroup)||(Gets_ModGroup_number(contactor_bus[1]) == Modgroup))
					{
						count++;
					}
			  }
		  }
	 }
	 
	 if(count >= 2){
		  return 1; 
	 }else{
		 return 0; 
	 }
}
/****************************申请退出功率*************
退出策略：
1.先退出最后一级目录的模块组

Need_Number：需要数量
lSwitch_Modgroup_list :原先已经投切的列表数据
***************************************************/		
void Allocation_Power_Exit_Function(UINT32 Apply_Ccu_no,UINT8 Exit_Number,SWITCH_MODGROUP_LIST *lSwitch_Modgroup_list)
{
	UINT8 i =0,j=0,temp =0,MobuleGrp_NO = 0,modbule_number_ok = 0,Priority_MobuleGrp = 0,Contactor_No[SYS_CONTACTOR_NUMBER]={0};
	SWITCH_MODGROUP_LIST removed_group,reserved_group;//需要剔除的组号,及预留下来的模块组号
	memset(&removed_group,0,sizeof(SWITCH_MODGROUP_LIST));
	memset(&reserved_group,0,sizeof(SWITCH_MODGROUP_LIST));
	UINT8 max_Switch_Modgroup[MODULE_INPUT_GROUP] = {0},max_Line_Directory_Level[MODULE_INPUT_GROUP] = {0},max_Contactor_No[MODULE_INPUT_GROUP] = {0}; 
	//查询链路最大数，重新排列顺序,初始值
	for(i =0;i<lSwitch_Modgroup_list->Modbule_Group_number;i++)
	{
		max_Line_Directory_Level[i] = lSwitch_Modgroup_list->Line_Directory_Level[i];
		max_Switch_Modgroup[i] = lSwitch_Modgroup_list->Modbule_Group_Adrr[i];//模块组地址
		Contactor_No[i] = lSwitch_Modgroup_list->Contactor_No[i];//接触器号
		if(ALLOCATION_DEBUG) printf("查询链路最大数 i = %d,max_Line_Directory_Level[i] =%d ,max_Switch_Modgroup[i] =%d,Contactor_No[i] =%d:\n",i,max_Line_Directory_Level[i],max_Switch_Modgroup[i],Contactor_No[i]);
	}
	
	for(j=0;j<lSwitch_Modgroup_list->Modbule_Group_number;j++)//冒泡排序
	{
		for(i=0;i<(lSwitch_Modgroup_list->Modbule_Group_number - j- 1);i++)
		{
			if(max_Line_Directory_Level[i] < max_Line_Directory_Level[i + 1])
			{
					temp = max_Line_Directory_Level[i];
					max_Line_Directory_Level[i] = max_Line_Directory_Level[i+1];
					max_Line_Directory_Level[i+1] = temp;	//小的的下沉到数组高端	
					
					temp = max_Switch_Modgroup[i];
					max_Switch_Modgroup[i] = max_Switch_Modgroup[i+1];
					max_Switch_Modgroup[i+1] = temp;	//小的的下沉到数组高端
					
					temp = Contactor_No[i];
					Contactor_No[i] = Contactor_No[i+1];
					Contactor_No[i+1] = temp;	//小的的下沉到数组高端
			}else if(max_Line_Directory_Level[i] == max_Line_Directory_Level[i + 1])
			{

				if(Globa->Charger_param.Charge_rate_number[Gets_modbule_Group_No(max_Switch_Modgroup[i])] < Globa->Charger_param.Charge_rate_number[Gets_modbule_Group_No(max_Switch_Modgroup[i] + 1)]){//在根据模块数进行排列
					temp = max_Line_Directory_Level[i];
					max_Line_Directory_Level[i] = max_Line_Directory_Level[i+1];
					max_Line_Directory_Level[i+1] = temp;	//小的的下沉到数组高端	
					
					temp = max_Switch_Modgroup[i];
					max_Switch_Modgroup[i] = max_Switch_Modgroup[i+1];
					max_Switch_Modgroup[i+1] = temp;	//小的的下沉到数组高端
					
					temp = Contactor_No[i];
					Contactor_No[i] = Contactor_No[i+1];
					Contactor_No[i+1] = temp;	//小的的下沉到数组高端
				}
			}	
		}
	}
	
	for(i=0;i<lSwitch_Modgroup_list->Modbule_Group_number;i++)//冒泡排序
	if(ALLOCATION_DEBUG)  printf("经过排列之后的 i = %d,max_Line_Directory_Level[i] =%d ,max_Switch_Modgroup[i] =%d,Contactor_No[i] =%d:\n",i,max_Line_Directory_Level[i],max_Switch_Modgroup[i],Contactor_No[i]);

	
	for(i=0;i<lSwitch_Modgroup_list->Modbule_Group_number;i++)//进行模块组退出情况
	{
	  MobuleGrp_NO = Gets_modbule_Group_No(max_Switch_Modgroup[i]);//根据模块组号获取参数中的序号
		
 
	  Priority_MobuleGrp = 0;
		
		if((Globa->Sys_MODGroup_Control_Data[MobuleGrp_NO].Sys_MobuleGrp_State == MOD_GROUP_STATE_INACTIVE)){//未激活不知道模块通讯数量
			modbule_number_ok =  Globa->Charger_param.Charge_rate_number[MobuleGrp_NO];
		}else {
			modbule_number_ok =   Globa->Sys_MODGroup_Control_Data[MobuleGrp_NO].Sys_Grp_OKNumber;
		}
		
		
    if(ALLOCATION_DEBUG) printf("\nExit_Number =%d max_Switch_Modgroup[%d] =%d MobuleGrp_NO =%d modbule_number_ok   =%d   Contactor_No[i] =%d \n",Exit_Number,i,max_Switch_Modgroup[i],MobuleGrp_NO,modbule_number_ok,Contactor_No[i]);
		//if((Exit_Number >= Globa->Charger_Modbule_Group_param_data[MobuleGrp_NO].modbule_number)&&(Priority_MobuleGrp == 0)     //by0922 不是模块组优先分配枪号
		if((Exit_Number >= modbule_number_ok)&&(Priority_MobuleGrp == 0)     //by0922 不是模块组优先分配枪号
			&&(Contactor_No[i]!= 0)&&(IS_Subordinate_Directory(max_Switch_Modgroup[i],lSwitch_Modgroup_list->Modbule_Group_number,&Contactor_No) == 0))//可以进行退出处理:1.退出模块数大于该组模块数，2.该组没有下级目录
		{

			removed_group.Modbule_Group_Adrr[removed_group.Modbule_Group_number] = max_Switch_Modgroup[i];
			removed_group.Contactor_No[removed_group.Modbule_Group_number] = Contactor_No[i];
			removed_group.Line_Directory_Level[removed_group.Modbule_Group_number] = max_Line_Directory_Level[i];
			Exit_Number = Exit_Number - Globa->Charger_param.Charge_rate_number[Gets_modbule_Group_No(max_Switch_Modgroup[i])];
			MobuleGrp_NO = Gets_modbule_Group_No(removed_group.Modbule_Group_Adrr[removed_group.Modbule_Group_number]);
			Power_Exit_Fun(MobuleGrp_NO,removed_group.Contactor_No[removed_group.Modbule_Group_number]);
		  if(ALLOCATION_DEBUG) 	printf("\退出成功：Exit_Number =%d max_Switch_Modgroup[%d] =%d MobuleGrp_NO =%d  removed_group.Contactor_No[removed_group.Modbule_Group_number] =%d \n",Exit_Number,i,max_Switch_Modgroup[i],MobuleGrp_NO,removed_group.Contactor_No[removed_group.Modbule_Group_number]);
			removed_group.Modbule_Group_number++;
			max_Switch_Modgroup[i] = 0;
		  Contactor_No[i] = 0;
		  max_Line_Directory_Level[i] = 0; //清除该组。
		}
	}
	
	if(removed_group.Modbule_Group_number != 0)
	{
	 //进行重新整理模块组，减去需要去除的模块组号
		for(i=0;i<lSwitch_Modgroup_list->Modbule_Group_number;i++)//已经投入的到该组的模块组数
		{
			for(j=0;j<removed_group.Modbule_Group_number;j++)
			{
				if((lSwitch_Modgroup_list->Modbule_Group_Adrr[i] == removed_group.Modbule_Group_Adrr[j])
					&&(lSwitch_Modgroup_list->Contactor_No[i] == removed_group.Contactor_No[j]))
				{
					break;
				}
			}
			
			if(j==removed_group.Modbule_Group_number)
			{
				//printf("reserved_group.Modbule_Group_number =%d  i=%d j=%d \n",reserved_group.Modbule_Group_number,i,j);
			  reserved_group.Modbule_Group_Adrr[reserved_group.Modbule_Group_number] = lSwitch_Modgroup_list->Modbule_Group_Adrr[i];
			  reserved_group.Contactor_No[reserved_group.Modbule_Group_number] = lSwitch_Modgroup_list->Contactor_No[i];
			  reserved_group.Line_Directory_Level[reserved_group.Modbule_Group_number] = lSwitch_Modgroup_list->Line_Directory_Level[i];
			  reserved_group.Modbule_Group_number++;
			}
		}
	
	/*//针对需要退出的模块提交退出申请
	for(j=0;j<removed_group.Modbule_Group_number;j++)
	{
		Mod_GroupNO = Gets_modbule_Group_No(removed_group.Modbule_Group_Adrr[i]);//模块组号对应的在参数中的序列号
		Power_Exit_Fun(Mod_GroupNO,removed_group.Contactor_No[i])	
	}*/
	//重新赋值
	  if(reserved_group.Modbule_Group_number != 0){
	    memcpy(&lSwitch_Modgroup_list->reserve,&reserved_group.reserve,sizeof(SWITCH_MODGROUP_LIST));
		}
	}
}


/**********************申请退出全部功率*************
*****************************************************/
void Allocation_Power_Exit_full_Function(UINT32 Apply_Ccu_no,SWITCH_MODGROUP_LIST *lSwitch_Modgroup_list)
{
	UINT8 i=0,Mod_GroupNO = 0,Modbule_Group_NO = 0;
	for(i =0;i<lSwitch_Modgroup_list->Modbule_Group_number;i++)
	{
		Mod_GroupNO = Gets_modbule_Group_No(lSwitch_Modgroup_list->Modbule_Group_Adrr[i]);//模块组号对应的在参数中的序列号
		if(lSwitch_Modgroup_list->Contactor_No[i] != 0)
		  Power_Exit_Fun(Mod_GroupNO,lSwitch_Modgroup_list->Contactor_No[i]);
	}
}		