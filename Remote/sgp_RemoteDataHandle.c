#include "globalvar.h"
#include "Data.h"
#include "sgp_RemoteDataHandle.h"


_sRemoteMsgQueue sgp_Msg_Queue;

typedef struct _s_old_new_stop_reason
{
    int old_reason;                 // 一代协议停止原因
    int new_reason;                 // 二代协议停止原因
}_sOldNewStopReason;


/**********************************************************************/
//  功能：  充电控制板通信中断处理
//  参数：  i：枪编号;data:数据指针
//  返回：  0:无变化；1：有变化
/**********************************************************************/
int ctrl_connect_lost_err_handle(int i,T_SFaultInfo* data)
{
    if(Globa->Control_DC_Infor_Data[i].Error.pre_ctrl_connect_lost != Globa->Control_DC_Infor_Data[i].Error.ctrl_connect_lost)
    {
        Globa->Control_DC_Infor_Data[i].Error.pre_ctrl_connect_lost= Globa->Control_DC_Infor_Data[i].Error.ctrl_connect_lost;
        data->faultId = 0x02 << 24;                 // 原因类型
        data->faultId = 0x45 << 16;                 // 原因编码
        data->faultSource = 1;                      // 故障源 01：整桩 02：枪
        data->faultLevel = 4;                       // 故障等级	01： 紧急故障02：一般故障03：严重告警04：一般告警

        if((Globa->Control_DC_Infor_Data[i].Error.ctrl_connect_lost != 0))
        {
            data->faultStat = 1;                    // 故障状态 发生
        }else
        {//消失
            data->faultStat = 2;                    // 故障状态 结束
        }

        return 1;
    }
    else
    {
        return 0;
    }
}


/**********************************************************************/
//  功能：  电表通信中断处理
//  参数：  i：枪编号;data:数据指针
//  返回：  0:无变化；1：有变化
/**********************************************************************/
int meter_connect_lost_err_handle(int i,T_SFaultInfo* data)
{
    if(Globa->Control_DC_Infor_Data[i].Error.pre_meter_connect_lost != Globa->Control_DC_Infor_Data[i].Error.meter_connect_lost)
    {
        Globa->Control_DC_Infor_Data[i].Error.pre_meter_connect_lost= Globa->Control_DC_Infor_Data[i].Error.meter_connect_lost;
        
        data->faultId = 0x10 << 24;                 // 原因类型
        data->faultId = 0x05 << 16;                 // 原因编码
        data->faultSource = 1;                      // 故障源 01：整桩 02：枪
        data->faultLevel = 2;                       // 故障等级	01： 紧急故障02：一般故障03：严重告警04：一般告警
        
        if((Globa->Control_DC_Infor_Data[i].Error.meter_connect_lost != 0))
        {
           data->faultStat = 1;                     // 故障状态 发生
        }else
        {//消失
            data->faultStat = 2;                    // 故障状态 结束
        }
        return 1;
    }
    else
    {
        return 0;
    }
}


/**********************************************************************/
//  功能：  交流模块通信中断处理
//  参数：  i：枪编号;data:数据指针
//  返回：  0:无变化；1：有变化
/**********************************************************************/
int AC_connect_lost_err_handle(int i,T_SFaultInfo* data)
{
    if(i==0)//仅在1号控制板检测的====7个
    {
        if(Globa->Control_DC_Infor_Data[i].Error.pre_AC_connect_lost != Globa->Control_DC_Infor_Data[i].Error.AC_connect_lost)
        {
            Globa->Control_DC_Infor_Data[i].Error.pre_AC_connect_lost= Globa->Control_DC_Infor_Data[i].Error.AC_connect_lost;
            
            data->faultId = 0x02 << 24;                 // 原因类型
            data->faultId = 0x38 << 16;                 // 原因编码
            data->faultSource = 1;                      // 故障源 01：整桩 02：枪
            data->faultLevel = 2;                       // 故障等级	01： 紧急故障02：一般故障03：严重告警04：一般告警

            if((Globa->Control_DC_Infor_Data[i].Error.AC_connect_lost != 0))
            {
                data->faultStat = 1;                     // 故障状态 发生
            }else
            {//消失
                data->faultStat = 2;                    // 故障状态 结束
            }
            return 1;
        }
    }
    else
    {
        return 0;
    } 
}


/**********************************************************************/
//  功能：  急停故障处理
//  参数：  i：枪编号;data:数据指针
//  返回：  0:无变化；1：有变化
/**********************************************************************/
int emergency_switch_err_handle(int i,T_SFaultInfo* data)
{
    if(Globa->Control_DC_Infor_Data[i].Error.pre_emergency_switch != Globa->Control_DC_Infor_Data[i].Error.emergency_switch)
    { 
        Globa->Control_DC_Infor_Data[i].Error.pre_emergency_switch = Globa->Control_DC_Infor_Data[i].Error.emergency_switch;
        
        data->faultId = 0x06 << 24;                 // 原因类型
        data->faultId = 0x01 << 16;                 // 原因编码
        data->faultSource = 1;                      // 故障源 01：整桩 02：枪
        data->faultLevel = 1;                       // 故障等级	01： 紧急故障02：一般故障03：严重告警04：一般告警

        if(Globa->Control_DC_Infor_Data[i].Error.emergency_switch != 0)
        {
            data->faultStat = 1;                     // 故障状态 发生
        }else
        {//消失
            data->faultStat = 2;                    // 故障状态 结束
        }
        return  1;
    }
    else
    {
        return 0;
    }
}


/**********************************************************************/
//  功能：  交流输入过压告警
//  参数：  i：枪编号;data:数据指针
//  返回：  0:无变化；1：有变化
/**********************************************************************/
int input_over_limit_err_handle(int i,T_SFaultInfo* data)
{
    if(Globa->Control_DC_Infor_Data[i].Error.pre_input_over_limit != Globa->Control_DC_Infor_Data[i].Error.input_over_limit)
    {
        Globa->Control_DC_Infor_Data[i].Error.pre_input_over_limit = Globa->Control_DC_Infor_Data[i].Error.input_over_limit;	
        
        data->faultId = 0x02 << 24;                 // 原因类型
        data->faultId = 0x0B << 16;                 // 原因编码
        data->faultSource = 1;                      // 故障源 01：整桩 02：枪
        data->faultLevel = 2;                       // 故障等级	01：紧急故障02:一般故障03：严重告警04：一般告警

        if(Globa->Control_DC_Infor_Data[i].Error.input_over_limit != 0)
        {
            data->faultStat = 1;                     // 故障状态 发生
        }else
        {//消失
            data->faultStat = 2;                    // 故障状态 结束
        }
        return 1;
    }
    else
    {
        return 0;
    }
    
}


/**********************************************************************/
//  功能：  交流输入过压保护
//  参数：  i：枪编号;data:数据指针
//  返回：  0:无变化；1：有变化
/**********************************************************************/
int input_over_protect_err_handle(int i,T_SFaultInfo* data)
{
    if(Globa->Control_DC_Infor_Data[i].Error.pre_input_over_protect != Globa->Control_DC_Infor_Data[i].Error.input_over_protect)
    {
        Globa->Control_DC_Infor_Data[i].Error.pre_input_over_protect = Globa->Control_DC_Infor_Data[i].Error.input_over_protect;
        
        memcpy(alarm_data->alarm_id_h[ALARM_INDEX],"01017",5);//交流输入过压保护
        memcpy(alarm_data->alarm_id_l[ALARM_INDEX],"001",3);
        
        alarm_data->alarm_id_l[ALARM_INDEX][2] = '1' + i;//枪号或单元号
        
        alarm_data->alarm_type[ALARM_INDEX][0] = '0';//"01"
        alarm_data->alarm_type[ALARM_INDEX][1] = '1';
        
        if(Globa->Control_DC_Infor_Data[i].Error.input_over_protect != 0)
        {
            alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"01"发生
            alarm_data->alarm_sts[ALARM_INDEX][1] = '1';
        }else
        {//消失
            alarm_data->alarm_sts[ALARM_INDEX][0] = '0';//"02"消失
            alarm_data->alarm_sts[ALARM_INDEX][1] = '2';
        }
        return 1;
    }
    else
    {
        return 0;
    }
}


/**********************************************************************/
//  功能：  交流输入欠压告警
//  参数：  i：枪编号;data:数据指针
//  返回：  0:无变化；1：有变化
/**********************************************************************/
int input_lown_limit_err_handle(int i,T_SFaultInfo* data)
{
    if(Globa->Control_DC_Infor_Data[i].Error.pre_input_lown_limit != Globa->Control_DC_Infor_Data[i].Error.input_lown_limit)
    { 
        Globa->Control_DC_Infor_Data[i].Error.pre_input_lown_limit = Globa->Control_DC_Infor_Data[i].Error.input_lown_limit;
        
        data->faultId = 0x02 << 24;                 // 原因类型
        data->faultId = 0x0C << 16;                 // 原因编码
        data->faultSource = 1;                      // 故障源 01：整桩 02：枪
        data->faultLevel = 2;                       // 故障等级	01：紧急故障02:一般故障03：严重告警04：一般告警
        
        if(Globa->Control_DC_Infor_Data[i].Error.input_lown_limit != 0)
        {
            data->faultStat = 1;                     // 故障状态 发生
        }else
        {//消失
            data->faultStat = 2;                    // 故障状态 结束
        }
        return 1;
    }
    else
    {
        return 0;
    }
}


/**********************************************************************/
//  功能：  交流输入开关跳闸
//  参数：  i：枪编号;data:数据指针
//  返回：  0:无变化；1：有变化
/**********************************************************************/
int input_switch_off_err_handle(int i,T_SFaultInfo* data)
{
    if(Globa->Control_DC_Infor_Data[i].Error.pre_input_switch_off != Globa->Control_DC_Infor_Data[i].Error.input_switch_off)
    { 
        Globa->Control_DC_Infor_Data[i].Error.pre_input_switch_off = Globa->Control_DC_Infor_Data[i].Error.input_switch_off;
        
        data->faultId = 0x02 << 24;                 // 原因类型
        data->faultId = 0x02 << 16;                 // 原因编码
        data->faultSource = 1;                      // 故障源 01：整桩 02：枪
        data->faultLevel = 1;                       // 故障等级	01：紧急故障02:一般故障03：严重告警04：一般告
        
        if(Globa->Control_DC_Infor_Data[i].Error.input_switch_off != 0)
        {
            data->faultStat = 1;                     // 故障状态 发生
        }else
        {//消失
            data->faultStat = 2;                    // 故障状态 结束
        }
        ALARM_INDEX++;
    }
}


/**********************************************************************/
//  功能：  交流防雷器跳闸
//  参数：  i：枪编号;data:数据指针
//  返回：  0:无变化；1：有变化
/**********************************************************************/
int fanglei_off_err_handle(int i,T_SFaultInfo* data)
{
    if(Globa->Control_DC_Infor_Data[i].Error.pre_fanglei_off != Globa->Control_DC_Infor_Data[i].Error.fanglei_off)
    { 
        Globa->Control_DC_Infor_Data[i].Error.pre_fanglei_off = Globa->Control_DC_Infor_Data[i].Error.fanglei_off;
        
        data->faultId = 0x06 << 24;                 // 原因类型
        data->faultId = 0x04 << 16;                 // 原因编码
        data->faultSource = 1;                      // 故障源 01：整桩 02：枪
        data->faultLevel = 2;                       // 故障等级	01：紧急故障02:一般故障03：严重告警04：一般告

        if(Globa->Control_DC_Infor_Data[i].Error.fanglei_off != 0)
        {
            data->faultStat = 1;                     // 故障状态 发生
        }else
        {//消失
            data->faultStat = 2;                    // 故障状态 结束
        }
        return 1;
    }
    else
    {
        return 0;
    }
}


/**********************************************************************/
//  功能：  充电模块通信中断
//  参数：  i：枪编号;data:数据指针
//  返回：  0:无变化；1：有变化
/**********************************************************************/
int charger_connect_lost_err_handle(int i,T_SFaultInfo* data)
{
    Globa->Control_DC_Infor_Data[i].Error.charger_connect_lost = 0;
    for(j=0;j<=Globa->Charger_param.Charge_rate_number[i];j++)	//检查当前充电枪所属模块是否有失联
    {
        Globa->Control_DC_Infor_Data[i].Error.charger_connect_lost |= Globa->Control_DC_Infor_Data[i].Error.charg_connect_lost[j];
    }
        
    if(Globa->Control_DC_Infor_Data[i].Error.pre_charger_connect_lost != Globa->Control_DC_Infor_Data[i].Error.charger_connect_lost)
    {
        Globa->Control_DC_Infor_Data[i].Error.pre_charger_connect_lost = Globa->Control_DC_Infor_Data[i].Error.charger_connect_lost;
        
        data->faultId = 0x06 << 24;                 // 原因类型
        data->faultId = 0x26 << 16;                 // 原因编码
        data->faultSource = 2;                      // 故障源 01：整桩 02：枪
        data->faultLevel = 4;                       // 故障等级	01：紧急故障02:一般故障03：严重告警04：一般告
        
        if(Globa->Control_DC_Infor_Data[i].Error.charger_connect_lost != 0)
        {
            data->faultStat = 1;                     // 故障状态 发生
        }else
        {//消失
            data->faultStat = 2;                    // 故障状态 结束
        }
        return 1;
    }
    else
    {
        return 0;
    }  
}


/**********************************************************************/
//  功能：  充电模块过温告警
//  参数：  i：枪编号;data:数据指针
//  返回：  0:无变化；1：有变化
/**********************************************************************/
int charger_over_tmp_err_handle(int i,T_SFaultInfo* data)
{
    Globa->Control_DC_Infor_Data[i].Error.charger_over_tmp = 0;
    for(j=0;j<=Globa->Charger_param.Charge_rate_number[i];j++)
    {
        Globa->Control_DC_Infor_Data[i].Error.charger_over_tmp |= Globa->Control_DC_Infor_Data[i].Error.charg_over_tmp[j];
    }
   
    if(Globa->Control_DC_Infor_Data[i].Error.pre_charger_over_tmp != Globa->Control_DC_Infor_Data[i].Error.charger_over_tmp)
    { 
        Globa->Control_DC_Infor_Data[i].Error.pre_charger_over_tmp = Globa->Control_DC_Infor_Data[i].Error.charger_over_tmp;
        
        data->faultId = 0x04 << 24;                 // 原因类型
        data->faultId = 0x05 << 16;                 // 原因编码
        data->faultSource = 2;                      // 故障源 01：整桩 02：枪
        data->faultLevel = 4;                       // 故障等级	01：紧急故障02:一般故障03：严重告警04：一般告
       
        if(Globa->Control_DC_Infor_Data[i].Error.charger_over_tmp != 0)
        {
            data->faultStat = 1;                     // 故障状态 发生
        }else
        {//消失
            data->faultStat = 2;                    // 故障状态 结束
        }
        return 1;
    }
    else
    {
        return 0;
    }
}


/**********************************************************************/
//  功能：  充电模块过压关机告警
//  参数：  i：枪编号;data:数据指针
//  返回：  0:无变化；1：有变化
/**********************************************************************/
int charger_OV_down_err_handle(int i,T_SFaultInfo* data)
{
    Globa->Control_DC_Infor_Data[i].Error.charger_OV_down = 0;
    for(j=0;j<=Globa->Charger_param.Charge_rate_number[i];j++)
        Globa->Control_DC_Infor_Data[i].Error.charger_OV_down |= Globa->Control_DC_Infor_Data[i].Error.charg_OV_down[j];
        
    if(Globa->Control_DC_Infor_Data[i].Error.pre_charger_OV_down != Globa->Control_DC_Infor_Data[i].Error.charger_OV_down)
    { 
        Globa->Control_DC_Infor_Data[i].Error.pre_charger_OV_down = Globa->Control_DC_Infor_Data[i].Error.charger_OV_down;
        
        data->faultId = 0x04 << 24;                 // 原因类型
        data->faultId = 0x10 << 16;                 // 原因编码
        data->faultSource = 2;                      // 故障源 01：整桩 02：枪
        data->faultLevel = 4;                       // 故障等级	01：紧急故障02:一般故障03：严重告警04：一般告
        
        if(Globa->Control_DC_Infor_Data[i].Error.charger_OV_down != 0)
        {
            data->faultStat = 1;                     // 故障状态 发生
        }else
        {//消失
            data->faultStat = 2;                    // 故障状态 结束
        }
        return 1;
    }
    else
    {
        return 0;
    }  
}


/**********************************************************************/
//  功能：  模块风扇告警
//  参数：  i：枪编号;data:数据指针
//  返回：  0:无变化；1：有变化
/**********************************************************************/
int charger_fun_err_handle(int i,T_SFaultInfo* data)
{	
    Globa->Control_DC_Infor_Data[i].Error.charger_fun_fault = 0;
    for(j=0;j<=Globa->Charger_param.Charge_rate_number[i];j++)
        Globa->Control_DC_Infor_Data[i].Error.charger_fun_fault |= Globa->Control_DC_Infor_Data[i].Error.charg_fun_fault[j];
        
    if(Globa->Control_DC_Infor_Data[i].Error.pre_charger_fun_fault != Globa->Control_DC_Infor_Data[i].Error.charger_fun_fault)
    { 
        Globa->Control_DC_Infor_Data[i].Error.pre_charger_fun_fault = Globa->Control_DC_Infor_Data[i].Error.charger_fun_fault;
        
        data->faultId = 0x06 << 24;                 // 原因类型
        data->faultId = 0x27 << 16;                 // 原因编码
        data->faultSource = 2;                      // 故障源 01：整桩 02：枪
        data->faultLevel = 4;                       // 故障等级	01：紧急故障02:一般故障03：严重告警04：一般告
        
        if(Globa->Control_DC_Infor_Data[i].Error.charger_fun_fault != 0)
        {
            data->faultStat = 1;                     // 故障状态 发生
        }else
        {//消失
            data->faultStat = 2;                    // 故障状态 结束
        }
        return 1;
    }
    else
    {
        return 0;
    }
}


/**********************************************************************/
//  功能：  模块其他告警
//  参数：  i：枪编号;data:数据指针
//  返回：  0:无变化；1：有变化
/**********************************************************************/
int charger_other_err_handle(int i,T_SFaultInfo* data)
{
    Globa->Control_DC_Infor_Data[i].Error.charger_other_alarm = 0;
    for(j=0;j<=Globa->Charger_param.Charge_rate_number[i];j++)
        Globa->Control_DC_Infor_Data[i].Error.charger_other_alarm |= Globa->Control_DC_Infor_Data[i].Error.charg_other_alarm[j];
        
    if(Globa->Control_DC_Infor_Data[i].Error.pre_charger_other_alarm != Globa->Control_DC_Infor_Data[i].Error.charger_other_alarm)
    { 
        Globa->Control_DC_Infor_Data[i].Error.pre_charger_other_alarm = Globa->Control_DC_Infor_Data[i].Error.charger_other_alarm;
        
        data->faultId = 0x06 << 24;                 // 原因类型
        data->faultId = 0x36 << 16;                 // 原因编码
        data->faultSource = 2;                      // 故障源 01：整桩 02：枪
        data->faultLevel = 2;                       // 故障等级	01：紧急故障02:一般故障03：严重告警04：一般告

        if(Globa->Control_DC_Infor_Data[i].Error.charger_other_alarm != 0)
        {
            data->faultStat = 1;                     // 故障状态 发生
        }else
        {//消失
            data->faultStat = 2;                    // 故障状态 结束
        }
        return 1;
    }
    else
    {
        return 0;
    }
}


/**********************************************************************/
//  功能：  绝缘告警
//  参数：  i：枪编号;data:数据指针
//  返回：  0:无变化；1：有变化
/**********************************************************************/
int sistent_err_handle(int i,T_SFaultInfo* data)
{
    if(Globa->Control_DC_Infor_Data[i].Error.pre_sistent_alarm != Globa->Control_DC_Infor_Data[i].Error.sistent_alarm)
    { 
        Globa->Control_DC_Infor_Data[i].Error.pre_sistent_alarm = Globa->Control_DC_Infor_Data[i].Error.sistent_alarm;
        
        data->faultId = 0x02 << 24;                 // 原因类型
        data->faultId = 0x34 << 16;                 // 原因编码
        data->faultSource = 2;                      // 故障源 01：整桩 02：枪
        data->faultLevel = 4;                       // 故障等级	01：紧急故障02:一般故障03：严重告警04：一般告
        
        if(Globa->Control_DC_Infor_Data[i].Error.sistent_alarm != 0)
        {
            data->faultStat = 1;                     // 故障状态 发生
        }else
        {//消失
            data->faultStat = 2;                    // 故障状态 结束
        }
        return 1;
    }
    else
    {
        return 0;
    }
}


/**********************************************************************/
//  功能：  系统输出过压告警
//  参数：  i：枪编号;data:数据指针
//  返回：  0:无变化；1：有变化
/**********************************************************************/
int output_over_limit_err_handle(int i,T_SFaultInfo* data)
{
    if(Globa->Control_DC_Infor_Data[i].Error.pre_output_over_limit!= Globa->Control_DC_Infor_Data[i].Error.output_over_limit)
    { 
        Globa->Control_DC_Infor_Data[i].Error.pre_output_over_limit = Globa->Control_DC_Infor_Data[i].Error.output_over_limit;
       
        data->faultId = 0x06 << 24;                 // 原因类型
        data->faultId = 0x0F << 16;                 // 原因编码
        data->faultSource = 2;                      // 故障源 01：整桩 02：枪
        data->faultLevel = 2;                       // 故障等级	01：紧急故障02:一般故障03：严重告警04：一般告
        
        if(Globa->Control_DC_Infor_Data[i].Error.output_over_limit != 0)
        {
            data->faultStat = 1;                     // 故障状态 发生
        }else
        {//消失
            data->faultStat = 2;                    // 故障状态 结束
        }
        return 1;
    }
    else
    {
        return 0;
    }
}


/**********************************************************************/
//  功能：  系统输出欠压告警
//  参数：  i：枪编号;data:数据指针
//  返回：  0:无变化；1：有变化
/**********************************************************************/
int output_low_limit_err_handle(int i,T_SFaultInfo* data)
{
    if(Globa->Control_DC_Infor_Data[i].Error.pre_output_low_limit!= Globa->Control_DC_Infor_Data[i].Error.output_low_limit)
    { 
        Globa->Control_DC_Infor_Data[i].Error.pre_output_low_limit = Globa->Control_DC_Infor_Data[i].Error.output_low_limit;
        
        data->faultId = 0x06 << 24;                 // 原因类型
        data->faultId = 0x10 << 16;                 // 原因编码
        data->faultSource = 2;                      // 故障源 01：整桩 02：枪
        data->faultLevel = 2;                       // 故障等级	01：紧急故障02:一般故障03：严重告警04：一般告
        
        if(Globa->Control_DC_Infor_Data[i].Error.output_low_limit != 0)
        {
            data->faultStat = 1;                     // 故障状态 发生
        }else
        {//消失
            data->faultStat = 2;                    // 故障状态 结束
        }
        return 1;
    }
    else
    {
        return 0;
    } 
}


/**********************************************************************/
//  功能：  直流输出开关跳闸
//  参数：  i：枪编号;data:数据指针
//  返回：  0:无变化；1：有变化
/**********************************************************************/
int output_switch_off_err_handle(int i,T_SFaultInfo* data)
{
    if(Globa->Control_DC_Infor_Data[i].Error.pre_output_switch_off[0] != Globa->Control_DC_Infor_Data[i].Error.output_switch_off[0])
    { 
        Globa->Control_DC_Infor_Data[i].Error.pre_output_switch_off[0] = Globa->Control_DC_Infor_Data[i].Error.output_switch_off[0];
        
        data->faultId = 0x01 << 24;                 // 原因类型
        data->faultId = 0x23 << 16;                 // 原因编码
        data->faultSource = 2;                      // 故障源 01：整桩 02：枪
        data->faultLevel = 4;                       // 故障等级	01：紧急故障02:一般故障03：严重告警04：一般告
        
        if(Globa->Control_DC_Infor_Data[i].Error.output_switch_off[0] != 0)
        {
            data->faultStat = 1;                     // 故障状态 发生
        }else
        {//消失
            data->faultStat = 2;                    // 故障状态 结束
        }
        return 1;
    }
    else
    {
        return 0;
    }
}


/**********************************************************************/
//  功能：  充电桩过温故障
//  参数：  i：枪编号;data:数据指针
//  返回：  0:无变化；1：有变化
/**********************************************************************/
int sys_over_tmp_err_handle(int i,T_SFaultInfo* data)
{
    if(Globa->Control_DC_Infor_Data[i].Error.pre_sys_over_tmp != Globa->Control_DC_Infor_Data[i].Error.sys_over_tmp)
    { 
        Globa->Control_DC_Infor_Data[i].Error.pre_sys_over_tmp = Globa->Control_DC_Infor_Data[i].Error.sys_over_tmp;
        
        data->faultId = 0x02 << 24;                 // 原因类型
        data->faultId = 0x35 << 16;                 // 原因编码
        data->faultSource = 1;                      // 故障源 01：整桩 02：枪
        data->faultLevel = 4;                       // 故障等级	01：紧急故障02:一般故障03：严重告警04：一般告
        
        if(Globa->Control_DC_Infor_Data[i].Error.sys_over_tmp != 0)
        {
            data->faultStat = 1;                     // 故障状态 发生
        }else
        {//消失
            data->faultStat = 2;                    // 故障状态 结束
        }
        return 1;
    }
    else
    {
        return 0;
    }
}


/**********************************************************************/
//  功能：  充电模块故障
//  参数：  i：枪编号;data:数据指针
//  返回：  0:无变化；1：有变化
/**********************************************************************/
int charger_err_handle(int i,T_SFaultInfo* data)
{
    Globa->Control_DC_Infor_Data[i].Error.charger_fault = 0;
    for(j=0;j<=Globa->Charger_param.Charge_rate_number[i];j++)		
        Globa->Control_DC_Infor_Data[i].Error.charger_fault |= Globa->Control_DC_Infor_Data[i].Error.charg_fault[j];
    
    if(Globa->Control_DC_Infor_Data[i].Error.pre_charger_fault != Globa->Control_DC_Infor_Data[i].Error.charger_fault)
    { 
        Globa->Control_DC_Infor_Data[i].Error.pre_charger_fault = Globa->Control_DC_Infor_Data[i].Error.charger_fault;
        
        data->faultId = 0x04 << 24;                 // 原因类型
        data->faultId = 0x01 << 16;                 // 原因编码
        data->faultSource = 1;                      // 故障源 01：整桩 02：枪
        data->faultLevel = 2;                       // 故障等级	01：紧急故障02:一般故障03：严重告警04：一般告

        if(Globa->Control_DC_Infor_Data[i].Error.charger_fault != 0)
        {
            data->faultStat = 1;                     // 故障状态 发生
        }else
        {//消失
            data->faultStat = 2;                    // 故障状态 结束
        }
        return 1;
    }
    else
    {
        return 0;
    }
}



/**********************************************************************/
//  功能：  根据一代协议储存的停止原因转换为二代停止原因编码
//  参数：  val:一代协议中停止原因
//  返回：  二代协议中停止原因编码
/**********************************************************************/
int GetStopReasonByOld(int val)
{
    static _sOldNewStopReason  reference_table[54] = 
    {
        {7,     0x020048},          // 充电电量超过起始SOC最大可充电量
        {8,     0X020149},          // 持续读取电度值比上次小
        {9,     0X020150},          // 达到SOC限制条件
        {16,    0x020138},          // 充电电流为0，结束充电
        {17,    0x030008},          // 手动终止
        {18,    0x030001},          // 达到手动设定条件
        {19,    0x030003},          // 账户余额不足
        {22,    0x03011A},          // 充电控制板响应超时
        {23,    0x020135},          // 超出了系统输出电压能力
        {24,    0x050123},          // 检测电池电压误差比较大
        {32,    0x020151},          // 预充阶段接触器两端电压相差太大
        {33,    0x050129},          // 车载BMS未响应
        {34,    0x050129},          // 车载BMS响应超时
        {35,    0x050128},          // 电池反接 
        {36,    0x050126},          // 未检测到电池电压
        {37,    0x05011C},          // BMS未准备就绪
        {38,    0x020127},          // 充电枪连接断开
        {39,    0x020111},          // 枪锁电子锁异常
        {40,    0x020144},          // 自检输出电压异常
        {41,    0x050122},          // 自检前检测到外侧电压异常
        {49,    0x02011D},          // 充电电压过压告警
        {50,    0x02011F},          // 充电电流过流告警
        {51,    0x020120},          // 输出短路故障
        {52,    0x02011D},          // 系统输出过压保护
        {53,    0x020112},          // 系统绝缘故障
        {54,    0x020117},          // 输出接触器异常
        {55,    0x02011C},          // 辅助电源接触器异常
        {57,    0x020110},          // 充电枪温度过高
        {64,    0x030114},          // 系统故障，请查询告警记录
        {65,    0x050002},          // BMS终止: 达到所需求的SOC
        {66,    0x050003},          // BMS终止: 达到总电压的设定值
        {67,    0x050004},          // BMS终止: 达到单体电压的设定值
        {68,    0x05014E},          // BMS终止: BMS绝缘故障
        {69,    0x050112},          // BMS终止: 输出连接器过温故障
        {70,    0x05010D},          // BMS终止: BMS元件过温故障
        {71,    0x050111},          // BMS终止: 充电连接器故障
        {72,    0x050109},          // BMS终止: 电池组温度过高故障
        {73,    0x050110},          // BMS终止: 高压继电器故障
        {80,    0x05010C},          // BMS终止: 监测点2电压检测故障
        {81,    0x050113},          // BMS终止: 其他故障
        {82,    0x05010E},          // BMS终止: 充电电流过大
        {83,    0x05010F},          // BMS终止: 充电电压异常
        {84,    0x050113},          // BMS终止: 未知
        {86,    0x050104},          // BMS状态: 单体电压过高
        {87,    0x050106},          // BMS状态: SOC过高
        {88,    0x050113},          // BMS状态: 温度过高
        {89,    0x050108},          // BMS状态: 电流过流
        {96,    0x050109},          // BMS状态: 绝缘异常
        {97,    0x050126},          // BMS状态: 输出连接器异常
        {99,    0x03011F},          // 数据库存储异常，请联系管理员
        {100,   0x020154},          // 系统判断BMS单体电压持续过压
        {101,   0x020153},          // 电表计量采集数据异常
        {117,   0x020146},          // VIN鉴权超时或失败
        {118,   0x020152},          // 充电模块未就绪
    };

    int i = 0;
    for(i = 0; i < 54; i++)
    {
        if(reference_table[i].old_reason == val)
        {
            return reference_table[i].new_reason;
        }
    }

    return 0;
}


/**********************************************************************/
//  功能：  根据数据库中充电账单填写需要上传的账单数据
//  参数：  src：数据库中读出来的账单；dst：要写入的账单
//  返回：  无
/**********************************************************************/
void CoverBillDataByDb(ISO8583_charge_db_data* src,T_SBillData *dst)
{
    memset(dst,0,sizeof(T_SBillData));

    dst->uiAddr = src->chg_port;                                            // 枪号

    if(0 == src->chg_start_by)                // 刷卡
    {
        dst->chargeType = 1;
        memcpy(dst->user,src->card_sn,sizeof(src->card_sn));                // 用户卡号或APP账号
    }
    else if(1 == src->chg_start_by)           // app
    {
        dst->chargeType = 2;
        memcpy(dst->user,src->card_sn,sizeof(src->card_sn));                // 用户卡号或APP账号
    }
    else if(100 == src->chg_start_by)           // VIN码
    {
        dst->chargeType = 3;
        memcpy(dst->user,src->car_VIN,sizeof(src->car_VIN));                // VIN码
        memcpy(dst->vin,src->car_VIN,sizeof(src->car_VIN)); 
    }

    if(0 == src->chg_start_by)              // 刷卡时要求填写交易结果
    {
        dst->billState = src->card_deal_result;                             // 交易结果
    }

    dst->stopCase = GetStopReasonByOld(src->end_reason);                    // 停止原因

    TransferStr2Time6(&dst->chargeStart,src->start_time);                   // 起始时间
    TransferStr2Time6(&dst->chargeEnd,src->end_time);                       // 结束时间
    dst->startChargePower = src->start_kwh;                                 // 充电开始电表读数
    dst->currChargePower = src->end_kwh;                                    // 充电结束电表读数
    dst->startSoc = src->Start_Soc;                                         // 充电开始电池SOC
    dst->endSoc = src->End_Soc;                                             // 充电结束电池SOC
    dst->totalPower = src->kwh_total_charged;                               // 本次充电总电量
    dst->jPrice = src->kwh1_price;                                          // 尖电价
    dst->jPower = src->kwh1_charged;                                        // 尖时段用电量
    dst->jConsume = src->kwh1_money;                                        // 尖时金额
    dst->fPrice = src->kwh2_price;                                          // 峰电价
    dst->fPower = src->kwh2_charged;                                        // 峰时段用电量
    dst->fConsume = src->kwh2_money;                                        // 峰时金额
    dst->pPrice = src->kwh3_price;                                          // 平电价
    dst->pPower = src->kwh3_charged;                                        // 平时段用电量
    dst->pConsume = src->kwh3_money;                                        // 平时金额
    dst->gPrice = src->kwh4_price;                                          // 谷电价
    dst->gPower = src->kwh4_charged;                                        // 谷时段用电量
    dst->gConsume = src->kwh4_money;                                        // 谷时金额
    dst->startBalance = src->last_rmb;                                      // 充电前余额
    dst->endBalance = src->money_left;                                      // 扣费后余额
    dst->curPrice = src->rmb_kwh;                                           // 消费电价
    dst->totalChargeConsume = src->total_kwh_money;                         // 充电消费总金额
    dst->jServicePrice = src->kwh1_service_price;                           // 尖服务单价
    dst->jServiceConsume = src->kwh1_service_money;                         // 尖服务金额
    dst->fServicePrice = src->kwh2_service_price;                           // 峰服务单价
    dst->fServiceConsume = src->kwh2_service_money;                         // 峰服务金额
    dst->pServicePrice = src->kwh3_service_price;                           // 平服务单价
    dst->pServiceConsume = src->kwh3_service_money;                         // 平服务金额
    dst->gServicePrice = src->kwh4_service_price;                           // 谷服务单价
    dst->gServiceConsume = src->kwh4_service_money;                         // 谷服务金额
    dst->totalServiceConsume = dst->jServiceConsume + dst->fServiceConsume + dst->pServiceConsume + dst->gServiceConsume;
    dst->appointPrice = src->book_price;                                    // 预约单价
    dst->appointConsume = src->book_money;                                  // 预约金额
    dst->stopPrice = src->park_price;                                       // 停车费
    dst->stopConsume = src->park_money;                                     // 停车金额
    dst->totalConsume = src->all_chg_money;                                 // 总消费金额
    dst->curChargePrice = dst->totalChargeConsume/dst->totalPower;          // 充电电价
    dst->curServicePrice = dst->totalServiceConsume/dst->totalPower;        // 服务费电价
    dst->isUpload = src->up_num;                                            // 上传标记

    memcpy(dst->order,src->deal_SN,sizeof(src->deal_SN));                   // 交易流水号
    memcpy(dst->remoteOrder,src->sgp_busy_sn,sizeof(src->sgp_busy_sn));     // 云平台订单号 fortest 需增加

    dst->chargePriceCount = src->sgp_chargePriceCount;                      // 费用阶段数
    memcpy(dst->chargeCosume,src->sgp_chargePriceCount,sizeof(dst->chargeCosume));  //区间电费信息
    dst->isParallels = src->sgp_isParallels;                                // 是否并机
    dst->isParallelsOrder = src->sgp_isParallelsOrder;                      // 是否并机汇总账单
    dst->isHost = src->sgp_isHost;                                          // 是否是主机
    dst->slaveCharge = src->sgp_slaveCharge;                                // 从机电量
    dst->slaveChrConsume = src->sgp_slaveChrConsume;                        // 从机充电消费
    dst->slaveSrvConsume = src->sgp_slaveSrvConsume;                        // 从机服务费
}



/**********************************************************************/
//  功能：  创建交易流水号
//  参数：  devId：枪号；data:数据储存地址
//  返回：  无
/**********************************************************************/
//void CreateLocalOrder(int devId,void* data)         // fortest
//{
//    unsigned char temp[64];
//    memset(temp,0,64);
//    int length = 0;
//    time_t systime = time(NULL);            // 获得系统的当前时间
//    struct tm tm_t;
//    localtime_r(&systime, &tm_t);           // 结构指针指向当前时间
//
//
//    memcpy(&temp[length],Globa->Charger_param.SN,sizeof(Globa->Charger_param.SN));       // 12位的SN，后4位用0补齐
//    length += sizeof(Globa->Charger_param.SN);
//    memset(&temp[length],0,4);
//    length += 4;
//    sprintf(&tmp[length], "%02d", devId + 1);
//    length + =2;
//    sprintf(&tmp[length], "%02d%02d%02d%02d%02d%02d", tm_t.tm_year-100, tm_t.tm_mon+1, tm_t.tm_mday, tm_t.tm_hour, tm_t.tm_min, tm_t.tm_sec);
//
//    memcpy(data,temp,64);
//}


/**********************************************************************/
//  功能：  设置运维地址
//  参数：  addr：接收数据的起始地址;len:长度
//  返回：  无
/**********************************************************************/
void SetOperationManagePort(void * addr,int len)
{
    memeset(addr,0,len);

    memcpy(addr,Globa->ISO8583_NET_setting.ISO8583_Server_IP,sizeof(Globa->ISO8583_NET_setting.ISO8583_Server_IP));
}

/**********************************************************************/
//  功能：  获取运维端口
//  参数：  无
//  返回：  运维端口
/**********************************************************************/
unsigned short GetOperationManagePort(void)
{
    return Globa->ISO8583_NET_setting.ISO8583_Server_port[1]*256 + Globa->ISO8583_NET_setting.ISO8583_Server_port[0];
}


/**********************************************************************/
//  功能：  获取设备状态
//  参数：  无
//  返回：  设备状态(01：故障02：告警03：空闲04：使用中08：停止服务10：离线)
/**********************************************************************/
unsigned char GetSysStatus(void)
{
    //遍历所有枪
    int gun_num = Globa->Charger_param.gun_config;
    int i = 0;
    int err_count = 0;
    int warn_count = 0;
    int using_count = 0;

    for(i = 0;i < gun_num;i++)
    {
        switch(Globa->Control_DC_Infor_Data[i].charger_state)
        {
            case 0x01:          // 故障
            {
                err_count++;
                break;
            }
            case 0x02:          // 告警
            {
                warn_count++;
                break;
            }
            case 03:            // 空闲
            {
                return 03;              // 有一个空闲直接报空闲
                break;
            }
            case 0x04:          // 充电中
            {
                using_count++;
                break;
            }
            case 0x05:         // 完成
            {
                return 03;              // 有一个空闲直接报空闲
                break;
            }
            case 0x06:         // 预约
            {
                using_count++;
                break;
            }
            default:
            break;
        }
    }

    if(err_count == gun_num)
    {
        return 0x01;        // 设备故障
    }
    else if(warn_count == gun_num)
    {
        return 0x02;        // 设备告警
    }
    else if(using_count == gun_num)
    {
        return 0x04;        // 使用中
    }

}

/**********************************************************************/
//  功能：  系统故障状态
//  参数：  i
//  返回：  系统故障状态  0：正常 1：故障
/**********************************************************************/
unsigned char GetSysFaultStatus(int i)
{
    if(Globa->Control_DC_Infor_Data[i].Error_system > 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
    
}



/**********************************************************************/
//  功能：  充电模块故障状态
//  参数：  i
//  返回：  充电模块故障状态  0：正常 1：故障
/**********************************************************************/
unsigned char GeteModelFaultStatus(int i)
{
    return Globa->Control_DC_Infor_Data[i].Error.charger_fault;
}

/**********************************************************************/
//  功能：  充电系统故障状态
//  参数：  i
//  返回：  充电系统故障状态  0：正常 1：故障
/**********************************************************************/
unsigned char GetChargeSysFaultStatus(int i)
{
    return Globa->Control_DC_Infor_Data[i].ctl_state;
}

/**********************************************************************/
//  功能：  直流输出开关状态
//  参数：  i
//  返回：  直流输出开关状态  0：断开 1：闭合，其他：无效
/**********************************************************************/
unsigned char GetDCOutputSwitchStatus(int i)
{
    if(Globa->Control_DC_Infor_Data[i].Error.output_switch_off[0] 
    ||Globa->Control_DC_Infor_Data[i].Error.output_switch_off[1])
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

/**********************************************************************/
//  功能：  直流输出接触器状态
//  参数：  i
//  返回：  直流输出接触器状态  0：断开 1：闭合，其他：无效
/**********************************************************************/
unsigned char GetDCContactStatus(int i)
{
    if(4 == Globa->Control_DC_Infor_Data[i].gun_state)      // 充电中
    {
        return 1;
    }
    else
    {
         return 0;
    }
    
}

/**********************************************************************/
//  功能：  枪门状态
//  参数：  i
//  返回：  枪门状态  0：断开 1：闭合，其他：无效
/**********************************************************************/
unsigned char GetRepairDoorStatus(int i)
{
    return 1 - Globa->Control_DC_Infor_Data[i].Error.Door_No_Close_Fault;
}

/**********************************************************************/
//  功能：  枪归位状态
//  参数：  i
//  返回：  枪归位状态  0：归位 1：未归位，其他：无效
/**********************************************************************/
unsigned char GetGunHomingStatus(int i)
{
    if(4 == Globa->Control_DC_Infor_Data[i].gun_state)      // 充电中
    {
        return 1;
    }
    else
    {
        return 0;
    } 
}


/**********************************************************************/
//  功能：  启动充电应答报文填充
//  参数：  i：枪号；flag：启动标志;data:数据指针
//  返回：  无
/**********************************************************************/
void FillChargeStartAck(int i,int flag,T_SChargeDataAck* data)
{
    data->uiAddr = i + 1;                       // 充电枪编号

    //云平台订单号
    memcpy(data->remoteOrder,Globa->Control_DC_Infor_Data[i].App.busy_SN,sizeof(Globa->Control_DC_Infor_Data[i].App.busy_SN));
   // CreateLocalOrder(i,Globa->Control_DC_Infor_Data[i].localOrder);       // 创建流水号
    Make_Local_SN();        // fortest 
    memcpy(data->localOrder,Globa->Control_DC_Infor_Data[i].local_SigGunBusySn,
        sizeof(Globa->Control_DC_Infor_Data[i].local_SigGunBusySn));

    if(0 == flag)       // 允许充电
    {
        data->ackResult = 0;        // 响应结果 （启动成功）
    }
    else if(6 == flag)
    {
        data->ackResult = 12;       // 响应结果 （设备故障）
    }
}


/**********************************************************************/
//  功能：  启动刷卡鉴权报文填充
//  参数：  i：枪号;data:数据指针
//  返回：  无
/**********************************************************************/
int FillCardCheckData(int i,T_SCheckCardData* data)
{
    data->uiAddr = i + 1;
    // 用户卡号
    memcpy(data->user,Globa->Control_DC_Infor_Data[i].User_Card_check_card_ID,sizeof(Globa->Control_DC_Infor_Data[i].User_Card_check_card_ID));
}


/**********************************************************************/
//  功能：  vin码充电数据填充
//  参数：  i：枪号;data:数据指针
//  返回：  无
/**********************************************************************/
void FillVinStartData(int i,T_SVinCharge* data)
{
    data->uiAddr = i + 1;       // 充电枪编号（从1开始）
    memcpy(data->vin,Globa->Control_DC_Infor_Data[i].VIN,sizeof(data.vin));        // VIN码	VIN码若无法获取默认全为0
    data->chargeType = Globa->Control_DC_Infor_Data[i].charge_mode;                // 充电方式	01：电量控制充电；02：时间控制充电；03：金额控制充电；04：自动充满05：定时启动充电

    if(1 == data->chargeType)        // 电量控制充电 
    {
        data->chargeValue = Globa->Control_DC_Infor_Data[i].charge_kwh_limit;
    }
    else if(2 == data->chargeType)   // 时间控制充电
    {
        data->chargeValue = Globa->Control_DC_Infor_Data[i].pre_time;
    }
    else if(3 == data->chargeType)   // 金额控制充电
    {
        data->chargeValue = Globa->Control_DC_Infor_Data[i].charge_money_limit;
    }
    else if(4 == data.chargeType)   // 充满为止
    {
        data->chargeValue = 0;
    }

    //CreateLocalOrder(i,&Globa->Control_DC_Infor_Data[i].localOrder);                                    // 创建交易流水号
    Make_Local_SN();        // fortest 
    memcpy(data->localOrder,Globa->Control_DC_Infor_Data[i].local_SigGunBusySn,
        sizeof(Globa->Control_DC_Infor_Data[i].local_SigGunBusySn));        // 桩交易流水号
}


/**********************************************************************/
//  功能：  刷卡启动充电数据填充
//  参数：  i：枪号;data:数据指针
//  返回：  无
/**********************************************************************/
void FillCardStartChargeData(int i,T_SCardCharge* data)
{
    data->uiAddr = i + 1;
    memcpy(data->user,Globa->Control_DC_Infor_Data[i].card_sn,sizeof(Globa->Control_DC_Infor_Data[i].card_sn));// 卡号
    data->chargeType = Globa->Control_DC_Infor_Data[i].charge_mode;                // 充电方式	01：电量控制充电；02：时间控制充电；03：金额控制充电；04：自动充满05：定时启动充电

    if(1 == data->chargeType)        // 电量控制充电 
    {
        data->chargeValue = Globa->Control_DC_Infor_Data[i].charge_kwh_limit;
    }
    else if(2 == data->chargeType)   // 时间控制充电
    {
        data->chargeValue = Globa->Control_DC_Infor_Data[i].pre_time;
    }
    else if(3 == data->chargeType)   // 金额控制充电
    {
        data->chargeValue = Globa->Control_DC_Infor_Data[i].charge_money_limit;
    }
    else if(4 == data.chargeType)   // 充满为止
    {
        data->chargeValue = 0;
    }

    //CreateLocalOrder(i,&Globa->Control_DC_Infor_Data[i].localOrder);                                    // 创建交易流水号
    Make_Local_SN();        // fortest 
    memcpy(data->localOrder,Globa->Control_DC_Infor_Data[i].local_SigGunBusySn,
        sizeof(Globa->Control_DC_Infor_Data[i].local_SigGunBusySn));        // 桩交易流水号
}



/**********************************************************************/
//  功能：  将yyyyMMddHHmmss形式的时间转换为Time6Bits格式
//  参数：  dst：目标地址；src：源数据地址
//  返回：  无
/**********************************************************************/
void TransferStr2Time6(Time6Bits* dst,char* src)
{
    // 年份
    int year = (src[0] - "0")*1000;
    year += (src[1] - "0")*100;
    year += (src[2] - "0")*10;
    year += src[3] - "0";
    dst->year = (year - 1900);

    // 月份
    int month = (src[4] - "0")*10;
    month += src[5] - "0";
    dst->month = month;

    // 日期
    int date = (src[6] - "0")*10;
    date += src[7] - "0";
    dst->date = date;

    // 时
    int hour = (src[8] - "0")*10;
    hour += src[9] - "0";
    dst->hour = hour;

    // 分
    int min = (src[10] - "0")*10;
    min += src[11] - "0";
    dst->minute = min;

    // 秒
    int sec = (src[12] - "0")*10;
    sec += src[13] - "0";
    dst->sec = sec;
}


/**********************************************************************/
//  功能：  线程休眠
//  参数：  ms：休眠时长（单位ms）
//  返回：  无
/**********************************************************************/
void sgp_Task_sleep(unsigned int ms)
{
    usleep(ms*1000);
}


/**********************************************************************/
//  功能：  初始化远程任务消息队列
//  参数：  无
//  返回：  无
//  初始化远程任务时需要调用
/**********************************************************************/
void SgpMsgQueueInitial()
{
    sgp_Msg_Queue.size = REMOTE_QUEUE_SIZE;            // 储存数据个数
    sgp_Msg_Queue.store_num = 0;        // 已储存个数

    sgp_Msg_Queue.store_pointer = &sgp_Msg_Queue.data_hub[0];       // 存储指针
    sgp_Msg_Queue.fetch_pointer = &sgp_Msg_Queue.data_hub[0];       // 获取指针

    // 将储存区初始化位环形链表
    memset(sgp_Msg_Queue.data_hub,0,sizeof(sgp_Msg_Queue.data_hub));
    int i = 0;

    for(i = 0; i < REMOTE_QUEUE_SIZE - 1; i++)
    {
        sgp_Msg_Queue.data_hub[i]->next = &sgp_Msg_Queue.data_hub[i+1];
    }
    sgp_Msg_Queue.data_hub[REMOTE_QUEUE_SIZE - 1]->next = &sgp_Msg_Queue.data_hub[0];
}

/**********************************************************************/
//  功能：  将报文发送到发送线程
//  参数：  cmd:报文ID；data：报文数据起始地址；len:报文长度
//  返回：  0：成功，1：失败
/**********************************************************************/
int CommPostEvent(int cmd,void *data,int len)
{
    if(sgp_Msg_Queue.store_num < REMOTE_QUEUE_SIZE)
    {
        // 使用存取指针
        SCommEventData* msg_data = sgp_Msg_Queue.store_pointer;

        msg_data->msg_id = cmd;
        msg_data->length = len;
        memcpy(msg_data->msg_data_arry,data,len);

        sgp_Msg_Queue.store_pointer = sgp_Msg_Queue.store_pointer->next;
        sgp_Msg_Queue.store_num++;

        return 1;
    }
    return 1;
}


/**********************************************************************/
//  功能：  从远程任务消息队列中取消息
//  参数：  cmd:报文ID；data：报文数据起始地址；len:报文长度
//  返回：  0:无消息，1：有消息
/**********************************************************************/
int CommFetchEvent(SCommEventData * data)
{
    if(sgp_Msg_Queue.store_num > 0)
    {
        // 复制数据
        memcpy(data,sgp_Msg_Queue.fetch_pointer,sizeof(SCommEventData));

        sgp_Msg_Queue.fetch_pointer = sgp_Msg_Queue.fetch_pointer->next;
        sgp_Msg_Queue.store_num--;

        return 1;
    }
    return 0;
}


