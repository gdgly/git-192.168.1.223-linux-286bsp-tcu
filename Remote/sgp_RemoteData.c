//#include <sys/types.h>
#include "globalvar.h"
#include "TcuComm.h"
#include "Data.h"
#include "string.h"
#include "Deal_Handler.h"
#include "sgp_RemoteDataHandle.h"


/**********************************************************************/
//  功能：  发送登录信息 0x1700
//  参数：  无
//  返回：  无
//  周期为5S
/**********************************************************************/
void sgp_SendLoginInfo()
{
    static time_t last_send_time = 0;          // 上次发送时间
    time_t now = time(NULL);                   // 系统当前时间

    if(now - last_send_time >= 5)
    {
        T_SSysLogin data;

        memset(&data, 0, sizeof(data));

        SetOperationManagePort(data.operationUrl,sizeof(data.operationUrl));                // 设置运营地址              
        data.operationPort = GetOperationManagePort()                                       // 获取运营端
        SetOperationManagePort(data.operationManageUrl,sizeof(data.operationManageUrl));    // 设置运维地址
        data.operationManagePort = GetOperationManagePort();                                // 获取运维端口
        memcpy(data.commVersion,Globa->protocol_ver,sizeof(data.commVersion));              // 设置通信协议版本
        memecpy(data.poductId,Globa->Charger_param.SN,sizeof(Globa->Charger_param.SN));     // 设置通道(TCU)出厂序列号
        memecpy(data.property,Globa->Charger_param.SN,sizeof(Globa->Charger_param.SN));     // 通道(TCU)运营商资产编码
        data.pilePower = Globa->limit_kw*100;                                               // 获取充电枪功率
        data.gunNum = Globa->Charger_param.gun_config;                                      // 获取充电枪总个数

        for (int i = 0; i < data.gunNum; i++)
        {
            data.gunInfo[i].gunNo = i;
            data.gunInfo[i].gunType = 2;
            data.gunInfo[i].power = 0;
            memecpy(data.gunInfo[i].gunVersion,&Globa->contro_ver[i][0],sizeof(data.gunInfo[i].gunVersion));// 设置ccu软件版本号 格式为：XX.XX.XX
        }

        CommPostEvent(TCU2REMOTE_PARA_REPORT,&data, sizeof(data));

        last_send_time = now;
    }
}

/**********************************************************************/
//  功能：  发送心跳
//  参数：  rightNow：是否立即发送标志
//  返回：  无
/**********************************************************************/
void SendHeartBeat(int rightNow)
{
    T_STcu2RemoteHeartBeat data;

    memset(&data, 0, sizeof(data));

    data.sysStat = GetSysStatus();                              // 获取设备状态

    data.gunNum = Globa->Charger_param.gun_config;              // 枪总数

    for (int i = 0; i < data.gunNum; i++)
    {
        data.gunInfo[i].gunNo = i;
        data.gunInfo[i].gunConnectStat = Globa->Control_DC_Infor_Data[i].gun_link;      // 充电枪车辆连接状态               
        data.gunInfo[i].gunStat = Globa->Control_DC_Infor_Data[i].gun_state;            // 充电枪状态
    }

    data.rightNow = rightNow;

    CommPostEvent(TCU2REMOTE_HEART_BEAT,&data, sizeof(data));
}


/**********************************************************************/
//  功能：  发送遥测
//  参数：  devId：枪编号；rightNow：是否立即发送标志
//  返回：  无
/**********************************************************************/
void SendYaoCe(int devId, int rightNow)
{ 
    T_SYaoCE data;
    memset(&data, 0, sizeof(data));

    data.uiAddr = devId + 1;                // 枪编号：01，表示1号枪

    data.Avoltage = 0;                      // A相输出电压
    data.Bvoltage = 0;                      // B相输出电压
    data.Cvoltage = 0;                      // C相输出电压
    data.Acurrent = 0;                      // A相输出电流
    data.Bcurrent = 0;                      // B相输出电流
    data.Ccurrent = 0;                      // C相输出电流
    data.outputPower = 0;                   // 输出功率
    data.gunTotal = Globa->Control_DC_Infor_Data[devId].kwh;                                // 已充电量
    data.meterTotal = Globa->Control_DC_Infor_Data[devId].meter_KWH;                        // 充电位计量表度数
    data.outputInsideVoltage = Globa->Control_DC_Infor_Data[devId].Output_voltage/10.0;     // 充电输出内侧电压
    data.outputOutsideVoltage = Globa->Control_DC_Infor_Data[devId].Output_voltage/10.0;    // 充电输出外侧电压
    data.outputCurrent = Globa->Control_DC_Infor_Data[devId].Output_current/10.0;           // 充电输出电流 0.1->0.01;
    data.temperature = 2500.0;                                                              // 充电机环境温度 0.1->0.01
    data.guideVoltage = Globa->Control_DC_Infor_Data[devId].line_v  *10.0;                  // 充电导引电压 0.1->0.01
    data.availablePower = 0;                                                                // 可用功率
    data.gainPower = 0;                                                                     // 获得功率
    data.outputVolt = Globa->Control_DC_Infor_Data[devId].Output_voltage / 10.0;            // 充电输出电压

    data.rightNow = rightNow;

    CommPostEvent(TCU2REMOTE_YAO_CE,&data, sizeof(data));
    
}

/**********************************************************************/
//  功能：  发送遥脉
//  参数：  devId：枪编号；rightNow：是否立即发送标志
//  返回：  无
/**********************************************************************/
void SendYaoMai(int devId, int rightNow)
{
    T_SYaoMai data;
    memset(&data, 0, sizeof(data));

    data.uiAddr = devId + 1;                                                    // 枪编号
    data.charge = Globa->Control_DC_Infor_Data[devId].kwh;                      // 已充电量 KWh
    data.chargeConsume = Globa->Control_DC_Infor_Data[devId].rmb;               // 已充电的电费
    data.serviceConsume = Globa->Control_DC_Infor_Data[devId].service_rmb;      // 已充电的服务费
    data.soc = Globa->Control_DC_Infor_Data[devId].BMS_batt_SOC;                // 电池SOC百分比数，数值为整数，格式为85%。交流设备默认为0
    int sec = Globa->Control_DC_Infor_Data[devId].Time;                         // 充电时长  s
    data.chargeHour = sec / 3600;                                               // 已充电时间小时
    data.chargeMin = sec % 3600 / 60;                                           // 已充电时间分钟
    data.chargeSec = sec % 3600 % 60;                                           // 已充电时间秒
    data.remainTime = Globa->Control_DC_Infor_Data[i].BMS_need_time/60;         // 剩余充电时长单位：分钟
    data.startChargePower = Globa->Control_DC_Infor_Data[i].meter_start_KWH;    // 开始充电的底度	KW 
    data.rightNow = rightNow;

    CommPostEvent(TCU2REMOTE_YAO_MAI,&data, sizeof(data));
}

/**********************************************************************/
//  功能：  发送遥信
//  参数：  devId：枪编号；rightNow：是否立即发送标志
//  返回：  无
/**********************************************************************/
void SendYaoXin(int devId, int rightNow)
{
    T_SYaoXin data;
    memset(&data, 0, sizeof(data));

    data.uiAddr = devId + 1;                                                                // 枪编号：01，表示1号枪
    data.gunInsert = Globa->Control_DC_Infor_Data[devId].gun_link;                          // 连接状态
    data.gunWorkStat = Globa->Control_DC_Infor_Data[devId].gun_state;                       // 工作状态
    data.ouputVaildPower =0;                                                                // 输出有功功率kw
    data.ouputInvaildPower = 0;                                                             // 输出无功功率kw
    data.sysFault = GetSysFaultStatus(devId);                                               // 系统故障状态  0：正常 1：故障
    data.chargeModFault = GeteModelFaultStatus(devId);                                      // 充电模块故障状态 0：正常 1：故障
    data.chargeSysFault = GetChargeSysFaultStatus(devId);                                   // 充电系统故障状态 0：正常 1：故障
    data.batteryFault = 0;                                                                  // 电池系统故障状态 0：正常 1：故障
    data.dcOutputSwitch = GetDCOutputSwitchStatus(devId);                                   // 直流输出开关状态 0：断开 1：闭合，其他：无效
    data.dcOutputContactor = GetDCContactStatus(devId);                                     // 直流输出接触器状态 0：断开 1：闭合，其他：无效
    data.gunDoor = GetRepairDoorStatus(devId);                                              // 枪门状态 0：断开 1：闭合，其他：无效
    data.gunBack = GetGunHomingStatus(devId);                                               // 枪归位状态 0：归位 1：未归位，其他：无效
    data.catportStat = 0;                                                                   // 车位状态 0：不支持状态监测1：空闲，2：占用其他：无效
    data.rightNow = rightNow;

    CommPostEvent(TCU2REMOTE_YAO_XIN,&data, sizeof(data));
}

/**********************************************************************/
//  功能：  发送BMS充电实时数据
//  参数：  devId：枪编号；rightNow：是否立即发送标志
//  返回：  无
/**********************************************************************/
void SendBmsStatus(int devId, int rightNow)
{
    T_SBmsData data;
    memset(&data, 0, sizeof(data));
 
    data.uiAddr = devId + 1;                                                                        // 枪编号：01，表示1号枪
    data.voltageNeed = Globa->Control_DC_Infor_Data[devId].need_voltage)/10.0(devId);               // 充电需求电压值
    data.currentNeed = Globa->Control_DC_Infor_Data[devId].need_current)/10.0;                      // 充电需求电流值
    data.voltageReal = Globa->Control_DC_Infor_Data[i].Output_voltage)/10.0;                        // 充电电压测量值
    data.currentReal = Globa->Control_DC_Infor_Data[i].Output_current)/10.0;                        // 充电电流测量值
    data.singleBatChargeVoltMax = Globa->Control_DC_Infor_Data[devId].BMS_cell_HV;                  // 单体电池最高电压
    data.singleBatChargeVoltMaxId = Globa->Control_DC_Infor_Data[devId].BMS_cell_HV_NO;             // 单体电池最高电压序号
    data.soc = Globa->Control_DC_Infor_Data[devId].BMS_batt_SOC;                                    // 电池SOC
    data.singleBatteryTempHigh = Globa->Control_DC_Infor_Data[devId].BMS_cell_HT*100;               // 单体电池最高温度
    data.singleBatteryTempHighId = Globa->Control_DC_Infor_Data[devId].BMS_cell_HT_NO;              // 单体电池最高温度序号
    data.singleBatteryTempLess = Globa->Control_DC_Infor_Data[devId].BMS_cell_LT*100;               // 单体电池最低温度
    data.singleBatteryTempLessId = Globa->Control_DC_Infor_Data[devId].BMS_cell_LT_NO;              // 单体电池最低温度序号
    data.gunTemp = Globa->Control_DC_Infor_Data[devId].Charge_Gun_Temp*100;                         // 充电枪温度
    memcpy(data.vin,&Globa->Control_DC_Infor_Data[devId].VIN[0],sizeof(Globa->Control_DC_Infor_Data[devId].VIN));  
    data.batType = Globa->Control_DC_Infor_Data[devId].BATT_type;                                   // 电池类型
    data.batGroupRatedVoltage = Globa->Control_DC_Infor_Data[devId].BMS_rate_voltage)/10.0;         // 电池组额定总电压
    data.batChargeVoltageMax = Globa->Control_DC_Infor_Data[devId].BMS_H_voltage)/10.0;             // 最高允许充电电压
    data.batChargeCurrentMax = Globa->Control_DC_Infor_Data[devId].BMS_H_current)/10.0;             // 最高允许充电电流
    data.singlBatAllowVoltageMax = Globa->Control_DC_Infor_Data[devId].Cell_H_Cha_Vol;              // 单体电池允许最高充电电压
    data.batGroupRatedPower = Globa->Control_DC_Infor_Data[devId].BMS_rate_AH;                      // 蓄电池组额定容量
    data.batRatedPower = Globa->Control_DC_Infor_Data[devId].Battery_Rate_KWh*100;                  // 电池额定电量 
    data.batTempMax = Globa->Control_DC_Infor_Data[gun_num].Cell_H_Cha_Temp*100;                    // 电池最高允许温度                                         
    memcpy(data.localOrder,Globa->Control_DC_Infor_Data[devId].local_SigGunBusySn,
        sizeof(Globa->Control_DC_Infor_Data[devId].local_SigGunBusySn)); // 桩交易流水号

    data.rightNow = rightNow;

    CommPostEvent(TCU2REMOTE_BMS_REAL_DATA,&data, sizeof(data));
}


/**********************************************************************/
//  功能：  启动事件上报
//  参数：  gun_Num：枪号
//  返回：  无
/**********************************************************************/
void SendEventUpload(int gun_Num)
{
    T_SChargeDataAck data;
    memset(data,0,sizeof(data));

    data.uiAddr = gun_Num + 1;                  // 枪号
    memcpy(data.user,Globa->Control_DC_Infor_Data[gun_Num].card_sn,sizeof(data.user));      // 用户卡号
    data.chargeType = Globa->Control_DC_Infor_Data[gun_Num].charge_mode;                    // 充电方式

    if(1 == data.chargeType)        // 电量控制充电 
    {
        data.chargeValue = Globa->Control_DC_Infor_Data[gun_Num].charge_kwh_limit;
    }
    else if(2 == data.chargeType)   // 时间控制充电
    {
        data.chargeValue = Globa->Control_DC_Infor_Data[gun_Num].pre_time;
    }
    else if(3 == data.chargeType)   // 金额控制充电
    {
        data.chargeValue = Globa->Control_DC_Infor_Data[gun_Num].charge_money_limit;
    }
    else if(4 == data.chargeType)   // 充满为止
    {
        data.chargeValue = 0;
    }
    
//    CreateLocalOrder(gun_Num,&Globa->Control_DC_Infor_Data[gun_Num].localOrder);                            // 创建交易流水号
    Make_Local_SN();        // fortest 
    memcpy(data.localOrder,Globa->Control_DC_Infor_Data[gun_Num].local_SigGunBusySn,
        sizeof(Globa->Control_DC_Infor_Data[gun_Num].local_SigGunBusySn));       // 桩交易流水号
    CommPostEvent(TCU2REMOTE_CARD_CHARGE,&data, sizeof(data));
}


/**********************************************************************/
//  功能：  超时取消充电枪的预约
//  参数：  gun_Num：枪号
//  返回：  无
//  附加：必须在赋值 Globa 之后再进行启动
/**********************************************************************/
void CancelOrderByTimeout(int gun_Num)
{
    T_SOrderChargeCancel data;
    memset(&data,0,sizeof(data));

    data.uiAddr = gun_Num + 1;          // 充电枪编号（从1开始编码）
    memcpy(data.user,Globa->Control_DC_Infor_Data[gun_Num].user,sizeof(data.user))  // 用户标识
    memcpy(data.remoteOrder,Globa->Control_DC_Infor_Data[gun_Num].busy_sn,sizeof(data.remoteOrder)) // 订单号

    CommPostEvent(TCU2REMOTE_ORDER_TIMEOUT,&data, sizeof(data));
}

/**********************************************************************/
//  功能：  发送配置请求
//  参数：  无
//  返回：  无
/**********************************************************************/
void SendSettingRequest()       // fortest
{
    T_ReomteGetSettings data;
    memset(&data, 0, sizeof(T_ReomteGetSettings));

    data.devId = 0;

    data.settingType = REMOTE_OTHER_SETTINGS;

    const char *key[] = {
        g_paramKey[PARAM_OCPP_WEBURL],
        g_paramKey[PARAM_OCPP_OFFLINEMODEENABLE],
        g_paramKey[PARAM_OCPP_OFFLINEPASSWORD],
        g_paramKey[PARAM_OCPP_CHARGEPOINTMODEL],
    };
    for(int i = 0; i < sizeof(key) / sizeof(key[0]); i++)
    {
        strcpy(data.key[i], key[i]);
        data.count++;
    }

    CommPostEvent(TCU2REMOTE_GET_REMOTE_SETTINGS,&data,sizeof(data));
}


/**********************************************************************/
//  功能：  充电指令响应处理
//  参数：  无
//  返回：  无
/**********************************************************************/
void sgp_ChargeStartAckHandle()
{
    int gun_num = Globa->Charger_param.gun_config;
    int i = 0;

    for(i = 0; i < gun_num; i++)
    {
        if(1 == Globa->Control_DC_Infor_Data[i].AP_APP_Sent_flag[0])        // 发枪x的APP启动应答
        {
            T_SChargeDataAck data;
            memset(&data,0 sizeof(data));

            int flag = Globa->Control_DC_Infor_Data[i].AP_APP_Sent_flag[1];
            FillChargeStartAck(i,flag,&data);                               // 填写启动应答数据

            CommPostEvent(TCU2REMOTE_REMOTE_START_ACK,&data,sizeof(data));

            Globa->Control_DC_Infor_Data[i].AP_APP_Sent_flag[0] = 0;        // 清发送标记
            break;                                                          // 一次只处理一个
        }
    }
}


/**********************************************************************/
//  功能：  刷卡鉴权处理
//  参数：  无
//  返回：  无
/**********************************************************************/
void sgp_UserCardCheckHandle()
{
    static time_t last_send_time = 0;
    
    T_SCheckCardData data;
 
    if(0 == Globa->sgp_card_check_send_flag)            // 没有在发送刷卡鉴权
    {
        int gun_num = Globa->Charger_param.gun_config;
        int i = 0;
        for(i=0;i<gun_num;i++)//检查是哪把枪在请求鉴权====1次只能有1把枪刷卡鉴权
        {
            if(Globa->Control_DC_Infor_Data[i].User_Card_check_flag)
            {
                memset(&data,0,sizeof(data));
                FillCardCheckData(i,&data);
                CommPostEvent(TCU2REMOTE_CHECK_CARD,&data,sizeof(data));

                last_send_time = time(NULL);
                Globa->sgp_card_check_send_flag = 1;
                Globa->sgp_card_check_send_num = i + 1;
                break;
            }
        }
    }
    else
    {
        time_t now = time(NULL);
        if(now - last_send_time > 10)       // 10s无应答则认位超时
        {
            int i = Globa->sgp_card_check_send_num - 1;
            Globa->Control_DC_Infor_Data[i].User_Card_check_flag = 0;       // 超时后不再发送 

            Globa->sgp_card_check_send_num = 0;
            Globa->sgp_card_check_send_flag = 0;  
        }
    }
}


/**********************************************************************/
//  功能：  刷卡鉴权应答处理
//  参数：  无
//  返回：  无
//  备注：  接收刷卡鉴权响应后调用
/**********************************************************************/
int RcvCardCheckAckHandle(T_SCheckCardDataAck* data)
{
    int i = data->uiAddr - 1;
    if(1 == Globa->sgp_card_check_send_flag)
    {
        Globa->sgp_card_check_send_flag = 0;                                            // 刷卡鉴权报文发送中标志位复位
        Globa->sgp_card_check_send_num = 0;                                             // 刷卡鉴权枪号复位

        Globa->Control_DC_Infor_Data[i].User_Card_check_flag = 0;                       // 卡片校验标志清除
        Globa->Control_DC_Infor_Data[i].User_Card_check_result = data->userState;       // 卡片校验结果

        if(0 == data->userState)
        {
            Globa->sgp_card_ok_wait_charge = 1;                                         // 刷卡鉴权成功等待启动充电状态
        }
        return 1;
    }
    return 0;
}


/**********************************************************************/
//  功能：  VIN码启动充电处理
//  参数：  无
//  返回：  无
/**********************************************************************/
void sgp_VinStartChargeHandle()
{
    static time_t last_send_time = 0;
    
    T_SVinCharge data;
 
    if(0 == Globa->sgp_vin_charge_send_flag)            // 没有在发送vin码启动
    {
        int gun_num = Globa->Charger_param.gun_config;
        int i = 0;
        for(i=0;i<gun_num;i++)      // 请求VIN鉴权
        {
            if(1 == Globa->Control_DC_Infor_Data[i].VIN_auth_req)
            {
                memset(&data,0,sizeof(data));
                FillVinStartData(i,&data);
                CommPostEvent(TCU2REMOTE_VIN_CHARGE,&data, sizeof(data));

                last_send_time = time(NULL);
                Globa->sgp_vin_charge_send_flag = 1;
                Globa->sgp_vin_charge_send_num = i + 1;
                break;
            }
        }
    }
    else
    {
        time_t now = time(NULL);
        if(now - last_send_time > 10)       // 10s无应答则认为超时
        {
            int i = Globa->sgp_vin_charge_send_num - 1;
 
            Globa->Control_DC_Infor_Data[i].VIN_auth_req = 0;       // 超时后不再发送
            Globa->Control_DC_Infor_Data[i].VIN_auth_success = 0;

            Globa->sgp_vin_charge_send_flag = 0;
            Globa->sgp_vin_charge_send_num = 0;
        }
    }
}


/**********************************************************************/
//  功能：  vin码启动应答处理
//  参数：  无
//  返回：  无
//  备注：  接收vin码启动响应后调用
/**********************************************************************/
int RcvVinStartAckHandle(T_SVinChargeAck* data)
{
    int i = data->uiAddr - 1;           // 枪号
    if(1 == Globa->sgp_vin_charge_send_flag)        // 在超时时间内
    {
        Globa->sgp_vin_charge_send_flag = 0;
        Globa->sgp_vin_charge_send_num = 0;
        Globa->Control_DC_Infor_Data[i].VIN_auth_req = 0;
                    
        if(0 == data->ackResult)
            Globa->Control_DC_Infor_Data[i].VIN_auth_success = 1;           //1--成功,0-失败
        else
            Globa->Control_DC_Infor_Data[i].VIN_auth_success = 0;
        
        Globa->Control_DC_Infor_Data[i].VIN_auth_failed_reason = 0;
        
        Globa->Control_DC_Infor_Data[i].VIN_auth_money = data->balance;     // 余额信息
        
        Globa->Control_DC_Infor_Data[i].VIN_auth_ack = 1;
        return 1;
    }
    return 0;
}


/**********************************************************************/
//  功能：  刷卡启动充电
//  参数：  无
//  返回：  无
/**********************************************************************/
void sgp_UserCardStartChargeHandle()        // 暂未实现 等待方案 fortest
{
    static time_t last_send_time = 0;

    T_SCardCharge data;
    if(0 == Globa->sgp_User_Card_Charge_flag)         // 还未刷卡充电启动
    {
        if(1 == Globa->sgp_card_ok_wait_charge)         // 已经鉴权成功
        {
            // 首先检查两次刷卡之间时间有没有超时
            time_t now = time(NULL);
            if(now - last_send_time > 60)               // 暂定超时为60s
            {
                Globa->sgp_card_ok_wait_charge = 0;
                Globa->sgp_card_check_send_num = 0;
                break;
            }

            int gun_num = Globa->Charger_param.gun_config;
            int i = 0;
            for(i = 0;i < gun_num; i++)
            {
                if(1 == Globa->Control_DC_Infor_Data[i].sgp_User_Card_Charge_flag)
                {
                    // 填充数据
                    memset(&data,0,sizeof(data));
                    FillCardStartChargeData(i,&data);
                    CommPostEvent(TCU2REMOTE_CARD_CHARGE,&data, sizeof(data));

                    last_send_time = now;
                    Globa->sgp_card_check_send_num = i + 1;
                    Globa->sgp_card_ok_wait_charge = 0;
                    Globa->sgp_User_Card_Charge_flag = 1;
                    Globa->Control_DC_Infor_Data[i].User_Card_check_result = 0;
                    break;
                }
            }
        }
        else
        {
            last_send_time = time(NULL);
        }
    }
    else
    {
        time_t now = time(NULL);
        if(now - last_send_time > 10)
        {
            Globa->sgp_card_check_send_num = 0;
            Globa->sgp_card_ok_wait_charge = 0;
            Globa->sgp_User_Card_Charge_flag = 0;
        }
    }
}


/**********************************************************************/
//  功能：  刷卡启动充电应答处理
//  参数：  无
//  返回：  无
//  备注：  接收刷卡启动响应后调用
/**********************************************************************/
void RcvUserCardStartChargeAckHandle(T_SCardChargeAck* data)
{
    int i = data->uiAddr - 1;
    // 在超时的时间内
    Globa->sgp_card_check_send_num = 0;
    Globa->sgp_card_ok_wait_charge = 0;
    Globa->sgp_User_Card_Charge_flag = 0;

    Globa->Control_DC_Infor_Data[i].User_Card_check_result = data->ackResult;
}


/**********************************************************************/
//  功能：  接收到心跳后的数据处理
//  参数：  data：心跳数据
//  返回：  无
/**********************************************************************/
void RcvHeartDataDeal(T_SRemoteHeart* data)
{
    if (Globa->sgp_IsLogin != data->loginStat)
    {
        Globa->sgp_IsLogin = data->loginStat;

        //登陆之后马上发送信息
        if (Globa->sgp_IsLogin == LOGIN_SUCCESS)
        {
            SendRealInfo();
        }
    }
}


/**********************************************************************/
//  功能：  充电枪状态改变时的处理
//  参数：  无
//  返回：  无
//  功能：  枪状态发生改变后，立刻发送更新后的遥控帧以及心跳帧
/**********************************************************************/
void GunStatChangedHandle()
{
    int gun_num = Globa->Charger_param.gun_config;

    int i = 0;
    int changed_count = 0;

    for(i = 0; i < gun_num; i++)
    {
        if(1 == CheckGunStatChange(i))
        {
            SendYaoXin(i,SEND_RIGHT_NOW);
            changed_count++;
            break;
        }
    }

    if(changed_count > 0)
    {
        SendHeartBeat(SEND_RIGHT_NOW);
    }
}

/**********************************************************************/
//  功能：  实时数据发送
//  参数：  无
//  返回：  无
//  功能：  枪状态发生改变后，立刻发送更新后的遥控帧以及心跳帧
/**********************************************************************/
void SendRealInfo()
{
    int gun_num = Globa->Charger_param.gun_config;
    int i = 0;

    for(i = 0; i < gun_num; i++)
    {
        int gunState = GetGunStatus(i);

        SendYaoCe(i, SEND_RIGHT_NOW);
        SendYaoXin(i, SEND_RIGHT_NOW);

        if (gunState == GUN_STATE_CHARGING)
        {
            SendYaoMai(i, SEND_RIGHT_NOW);

            SendBmsStatus(i, SEND_RIGHT_NOW);
        }

        SendHeartBeat(SEND_RIGHT_NOW);
        //SendSettingRequest();
    }
}


/**********************************************************************/
//  功能：  枪状态变化时的处理
//  参数：  gun_Num：枪号
//  返回：  0:状态没改变；1：状态改变
/**********************************************************************/
void CheckGunStatChange(int gun_Num)
{
    int last_gun_stat[CONTROL_DC_NUMBER];

    if(gun_Num >= CONTROL_DC_NUMBER)
    {
        return 0;
    }
    else
    {
        if(Globa->Control_DC_Infor_Data[gun_Num].gun_state != last_gun_stat[gun_Num])
        {
            last_gun_stat[gun_Num] = Globa->Control_DC_Infor_Data[gun_Num].gun_state;
            return 1;
        }
        else
        {
            return 0;
        }
    }
}


/**********************************************************************/
//  功能：  检查是否有故障变化
//  参数：  devId：枪编号;data:数据指针
//  返回：  0:无变化；1：有变化
/**********************************************************************/
int CheckFaultChange(int devId,T_SFaultInfo* data)
{
    if(ctrl_connect_lost_err_handle(devId,data))            // 充电控制板通信中断
    {
        return 1;
    }
    if(meter_connect_lost_err_handle(devId,data))           // 电表表通信中断
    {
        return 1;
    }
    if(AC_connect_lost_err_handle(i,data))                  // 交流模块通信中断
    {
        return 1;
    }
    if(emergency_switch_err_handle(i,data))                 // 急停故障
    {
        return 1;
    }
    if(input_over_limit_err_handle(i,data))                 // 交流输入过压
    {
        return 1;
    }
    if(input_lown_limit_err_handle(i,data))                 // 交流输入欠压
    {
        return 1;
    }
    if(input_switch_off_err_handle(i,data))                 // 交流输入开关跳闸
    {
        return 1;
    }
    if(fanglei_off_err_handle(i,data))                      // 防雷器故障
    {
        return 1;
    }
    if(charger_connect_lost_err_handle(i,data))             // 充电模块通信中断
    {
        return 1;
    }
    if(int charger_over_tmp_err_handle(i,data))             // 充电模块过温告警
    {
        return 1;
    }
    if(charger_OV_down_err_handle(i,data))                  // 充电模块过压关机告警
    {
        return 1;
    }
    if(charger_fun_err_handle(i,data))                      // 模块风扇告警
    {
        return 1;
    }
    if(charger_other_err_handle(i,data))                    // 模块其他告警
    {
        return 1；
    }
    if(sistent_err_handle(i,data))                          // 绝缘告警
    {
        return 1;
    }
    if(output_over_limit_err_handle(i,data))                // 系统输出过压告警
    {
        return 1;
    }
    if(output_low_limit_err_handle(i,data))                 // 系统输出欠压告警
    {
        return 1;
    }
    if(output_switch_off_err_handle(i,data))                // 直流输出开关跳闸
    {
        return 1;
    }
    if(sys_over_tmp_err_handle(i,data))                     // 充电桩过温故障
    {
        return 1;
    }
    if(charger_err_handle(i,data))                          // 充电模块故障
    {
        return 1;
    }

    return 0;
}


/**********************************************************************/
//  功能：  心跳周期发送处理
//  参数：  无
//  返回：  无
/**********************************************************************/
void HeartBeatPeriodSendHandle()
{
    static time_t last_send_time;        // 记录上次发送时间
    time_t now = time(NULL);

    if(now - last_time >= 5) 
    {
        SendHeartBeat(SEND_PERIOD);
        last_send_time = now;
    }
}
    
/**********************************************************************/
//  功能：  遥测周期发送处理
//  参数：  无
//  返回：  无
/**********************************************************************/
void YaoCePeriodSendHandle()
{
    static time_t last_send_time[CONTROL_DC_NUMBER];        // 记录上次发送时间
    time_t now = time(NULL);

    int gun_num = Globa->Charger_param.gun_config;
    int i = 0;
    for(i = 0; i < gun_num; i++)
    {
        int gun_stat = GetGunStatus(i);
        int period = 0;
        if(gun_stat == GUN_STATE_CHARGING)
        {
            period = Globa->Charger_param.sgp_chgbusy_msg_period;       // 充电时消息间隔
        }
        else
        {
            period = Globa->Charger_param.sgp_standby_msg_period;       // 待机的消息间隔
        }

        if(now - last_send_time[i] >= period) 
        {
            SendYaoCe(i,SEND_PERIOD);
            last_time[i] = now;
        }
    }
}

/**********************************************************************/
//  功能：  遥信周期发送处理
//  参数：  无
//  返回：  无
/**********************************************************************/
void YaoXinPeriodSendHandle()
{
    static time_t last_send_time[CONTROL_DC_NUMBER];        // 记录上次发送时间
    time_t now = time(NULL);

    int gun_num = Globa->Charger_param.gun_config;
    int i = 0;
    for(i = 0; i < gun_num; i++)
    {
        int gun_stat = GetGunStatus(i);
        int period = 0;
        if(gun_stat == GUN_STATE_CHARGING)
        {
            period = Globa->Charger_param.sgp_chgbusy_msg_period;       // 充电时消息间隔
        }
        else
        {
            period = Globa->Charger_param.sgp_standby_msg_period;       // 待机的消息间隔
        }

        if(now - last_send_time[i] >= period) 
        {
            SendYaoXin(i,SEND_PERIOD);
            last_send_time[i] = now;
        }
    }
}

/**********************************************************************/
//  功能：  遥脉周期发送处理
//  参数：  无
//  返回：  无
/**********************************************************************/
void YaoMaiPeriodSendHandle()
{
    static time_t last_send_time[CONTROL_DC_NUMBER];        // 记录上次发送时间
    time_t now = time(NULL);

    int gun_num = Globa->Charger_param.gun_config;
    int i = 0;
    for(i = 0; i < gun_num; i++)
    {
        int gun_stat = GetGunStatus(i);
        int period = 0;
        if(gun_stat == GUN_STATE_CHARGING)
        {
            period = Globa->Charger_param.sgp_chgbusy_msg_period;       // 充电时消息间隔

            if(now - last_time[i] >= period) 
            {
                SendYaoMai(i,SEND_PERIOD);
                last_send_time[i] = now;
            }
        }
    }
}

/**********************************************************************/
//  功能：  BMS实时信息周期发送处理
//  参数：  无
//  返回：  无
/**********************************************************************/
void BmsStatusPeriodSendHandle()
{
    static time_t last_send_time[CONTROL_DC_NUMBER];        // 记录上次发送时间
    time_t now = time(NULL);

    int gun_num = Globa->Charger_param.gun_config;
    int i = 0;
    for(i = 0; i < gun_num; i++)
    {
        int gun_stat = GetGunStatus(i);
        int period = 0;
        if(gun_stat == GUN_STATE_CHARGING)
        {
            period = Globa->Charger_param.sgp_chgbusy_msg_period;       // 充电时消息间隔

            if(now - last_time[i] >= period) 
            {
                SendBmsStatus(i,SEND_PERIOD);
                last_send_time[i] = now;
            }
        }
    }
}



/**********************************************************************/
//  功能：  充电账单上送
//  参数：  无
//  返回：  无
/**********************************************************************/
void BillInfoUpload()
{
    static time_t  last_send_time;          // 保存上次发送时间
    static T_SBillData data;
    
    time_t now = time(NULL);

    if(1 == Globa->sgp_billSendingFlag)        // 有充电账单在发送中
    {
        if(now - last_send_time > 2)   // 超过2S发送周期
        {
            CommPostEvent(TCU2REMOTE_BILL_REPORT,&data, sizeof(data));
            
            last_send_time = time(NULL);                // 发送时间
            Globa->billSendingCount++;                 // 发送次数
        }
    }
    else  
    {
        int store_num = 0;
        pthread_mutex_lock(&busy_db_pmutex);
        Globa->sgp_g_Busy_Need_Up_id = Select_Busy_Need_Up(0,&Globa->sgp_bill_data_db,&store_num);// 查询需要上传的业务记录的最大 ID 值
        pthread_mutex_unlock(&busy_db_pmutex);

        if(-1 != Globa->sgp_g_Busy_Need_Up_id)                 // 有本地储存未上传的记录
        {
            CoverBillDataByDb(&Globa->sgp_bill_data_db,&data);//数据库内容转协议发送的交易记录报文
            CommPostEvent(TCU2REMOTE_BILL_REPORT,&data, sizeof(data));

            last_send_time = time(NULL);            // 发送时间
            Globa->sgp_billSendingFlag = 1;         // 发送标志
            Globa->billSendingCount = 1;            // 发送次数
        }
    }
}

/**********************************************************************/
//  功能：  接收到充电账单上送应答处理
//  参数：  data：报文地址
//  返回：  无
/**********************************************************************/
void RcvBillComfirmHandle(T_SUploadBillAck* data)
{
    // 查看响应结果
    if(0 == data->ackResult)           // 响应成功
    {
        pthread_mutex_lock(&busy_db_pmutex);
        Set_Busy_UP_num(0,Globa->sgp_g_Busy_Need_Up_id, 0xAA);//标记该条充电记录上传成功
        int i = 0;
        for(i=0;i<5;i++)                //再检测5次看是否没成功
        {
            sgp_Task_sleep(200);         // fortest 不建议在这里使用休眠
            if(Select_NO_Over_Record(0,Globa->sgp_g_Busy_Need_Up_id,0xAA)!= -1) //表示有没有上传的记录
            {
                Set_Busy_UP_num(0,Globa->sgp_g_Busy_Need_Up_id, 0xAA);
            }
            else
                break;
        }
        
        if(Globa->sgp_bill_data_db.card_deal_result==1)//锁卡的记录上送后修改锁卡标记为2，后续在本机解锁，必须本机在线，因为记录已上送，解锁后要发解锁成功标记给后台
            Set_Unlock_flag(Globa->sgp_g_Busy_Need_Up_id, 2);
        pthread_mutex_unlock(&busy_db_pmutex);
        Globa->sgp_billSendingFlag = 0;
        AppLogOut("=====收到交易记录号%d的成功应答报文=====!",Globa->sgp_g_Busy_Need_Up_id);
    }
    else
    {
        pthread_mutex_lock(&busy_db_pmutex);
        Set_Busy_UP_num(0,Globa->sgp_g_Busy_Need_Up_id, 0x44);//标记该条充电记录停止上送
        int i = 0;
        for(i=0;i<5;i++)        //再检测5次看是否没成功
        {
            sgp_Task_sleep(200);
            if(Select_NO_Over_Record(0,Globa->sgp_g_Busy_Need_Up_id,0x44)!= -1)//表示有没有上传的记录
            {
                Set_Busy_UP_num(0,Globa->sgp_g_Busy_Need_Up_id, 0x44);
            }
            else
                break;
        }
        pthread_mutex_unlock(&busy_db_pmutex);
        Globa->sgp_billSendingFlag = 0;
        AppLogOut("=====收到交易记录号%d的应答报文,没有上传成功,停止上送本条记录=====!,原因：%d",Globa->sgp_g_Busy_Need_Up_id,data->localOrder);
    }
}


/**********************************************************************/
//  功能：  充电枪故障信息上报处理
//  参数：  无
//  返回：  无
//  功能：  异步上传，无应答时继续上传
/**********************************************************************/
void GunErrChangedHandle()
{
    static time_t last_send_time;             // 保存上次发送时间
    static T_SFaultInfo data;

    time_t now = time(NULL);

    if(1 == Globa->sgp_FaultSendingFlag)                    // 有故障信息在发送中
    {
        if(now - last_send_time > 5)           // 超过5S发送周期
        {
            CommPostEvent(TCU2REMOTE_FAULT_MSG,&data,sizeof(data));
            
            last_send_time = time(NULL);                    // 发送时间
        }
    }
    else
    {
        int gun_num = Globa->Charger_param.gun_config;
        int i = 0;

        for(i = 0;i < gun_num; i++)
        {
            if(1 == CheckFaultChange(i,&data))
            {
                data.index = i + 1;

                if(1 == data.faultSource)               // 故障源为整桩
                {
                    data.index = 0;
                }

                time_t systime = time(NULL);            // 获得系统的当前时间
                struct tm tm_t;
                Time6Bits tm_table;
                localtime_r(&systime, &tm_t);           // 结构指针指向当前时间

                tm_table.year = tm_t.tm_year - 100;     // 年
                tm_table.month = tm_t.tm_mon+1;         // 月
                tm_table.date = tm_t.tm_mday;           // 日
                tm_table.hour = tm_t.tm_hour;           // 时
                tm_table.minute = tm_t.tm_min;          // 分
                tm_table.sec = tm_t.tm_sec;             // 秒

                memcpy(&data.faultTime, &tm_table,sizeof(data.faultTime));            // 故障变化时间
                CommPostEvent(TCU2REMOTE_FAULT_MSG,&data, sizeof(T_SFaultInfo));
                Globa->sgp_FaultSendingFlag = 1;

                last_send_time = time(NULL);                // 发送时间
                break;
            }
        }
    }  
}


/**********************************************************************/
//  功能：  收到故障信息应答
//  参数：  data：报文指针
//  返回：  无
/**********************************************************************/
void RcvFaultMsgHandle(T_SFaultInfoAck* data)
{
    Globa->sgp_FaultSendingFlag = 0;
}


/**********************************************************************/
//  功能：  远程服务数据初始化
//  参数：  无
//  返回：  无
/**********************************************************************/
void RemoteServerDataInit(void)
{
	int i;
	for(i=0;i<Globa->Charger_param.gun_config;i++)
	{
		Globa->Control_DC_Infor_Data[i].AP_APP_Sent_flag[0]=0;
		Globa->Control_DC_Infor_Data[i].AP_APP_Sent_flag[1]=0;
	}
	Globa->have_hart = 0;
	Globa->Software_Version_change  = 0;
}


