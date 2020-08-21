#ifndef ALLOCATION_POWER_H
#define ALLOCATION_POWER_H
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

typedef struct _MODGROUP_DIAGRAM_LIST{
  UINT8   reserve;//保留
	UINT8   Priority_falg;     //优先选择标志
	UINT8   Modbule_Group_NO; //模块组号
	UINT8   diagram_number; //该组的关系数
	UINT8   diagram_Modbule_Group_Adrr[MODULE_INPUT_GROUP];//投入的模块组号   0--代表无
	UINT8   diagram_Contactor_No[MODULE_INPUT_GROUP];//投入该模块组对应的继电器号-即线路-退出时根据继电器来判断其情况 0-表示直连，或无继电器
}MODGROUP_DIAGRAM_LIST;


typedef struct _WAIT_SELECTION_DATA{
  UINT8   reserve;//保留
	UINT8   Priority_falg; //优先选择标志
	UINT8   Data_falg;//该值数据是否有效
	UINT8   MobuleGrp_NO; //最开始的一级表
	UINT8   MobuleGrpNO;//模块组号
	UINT8   modbule_number;//可以投入的模块数量
	UINT8   Modbule_Group_Adrr; //投入的模块地址
	UINT8   Contactor_No;//投入的接触器号
	UINT8   Line_Directory_Level;//投入的线路等级
	UINT8   Sys_MobuleGrp_State;//模块状态
}WAIT_SELECTION_DATA;

typedef struct _MODGROUP_WAIT_SELECTION_LIST{
  UINT8   reserve;//保留
	UINT8   WaitSelectionNum;//待选择组号
  WAIT_SELECTION_DATA  WaitWiaitSelectionData[MODULE_INPUT_GROUP];
}MODGROUP_WAIT_SELECTION_LIST;

typedef struct _SWITCH_MODGROUP_LIST{
  UINT8   reserve;//保留
	UINT8   Modbule_Group_number;      //已经投入的模块组数
	UINT8   Modbule_Group_Adrr[MODULE_INPUT_GROUP];//投入的模块组号   0--代表无
	UINT8   Contactor_No[MODULE_INPUT_GROUP];//投入该模块组对应的继电器号-即线路-退出时根据继电器来判断其情况 0-表示直连，或无继电器
	UINT8   Line_Directory_Level[MODULE_INPUT_GROUP];//连接到该CCU组处于的几级目录。
}SWITCH_MODGROUP_LIST;
enum CHARGE_PROCES_STEP
{
	CHARGE_PROCES_IDLE  = 0,//空闲状态
	CHARGE_PROCES_PREPARE = 1,//准备状态
	CHARGE_PROCES_CHARGING = 2,//充电中状态
	CHARGE_PROCES_END = 3//充电结束状态
};
UINT8 Gets_BUS_number(UINT8 contactor_no,UINT8*contactor_bus);
UINT8 Gets_ModGroup_number(UINT8 modbule_bus_Gno);
INT8 Gets_busccu_number(UINT8 ModGroup_Gno);
INT8 Gets_busno_mod_Gno_number(UINT8 CCUno); ////从对应CCU号获取组号母线号/模块组号(直连的)
INT8 Gets_modbule_Group_No(UINT8 ModGroup_Gno);
INT8 ISContactor_ok(UINT8 Contactor_No);
INT16 Gets_contactor_control_unit_no_number(UINT8 contactor_no);
void Already_invested_module_detection(UINT32 Ccu_no,SWITCH_MODGROUP_LIST *gSwitch_ModGroup_list);
void Activate_Group(UINT8 Group_No);
void ModGroup_Diagram_list_init();
void Allocation_Power_Applied_Function(UINT32 Apply_Ccu_no,UINT8 Need_Number,SWITCH_MODGROUP_LIST *gSwitch_Modgroup_list);
UINT8 IS_Subordinate_Directory(UINT32 Modgroup,UINT8 number, UINT8 *Contactor_No);
void Allocation_Power_Exit_Function(UINT32 Apply_Ccu_no,UINT8 Exit_Number,SWITCH_MODGROUP_LIST *gSwitch_Modgroup_list);
void Allocation_Power_Exit_full_Function(UINT32 Ccu_no,SWITCH_MODGROUP_LIST *gSwitch_Modgroup_list);

#endif // CHARGE_PROCES_TASK_H


