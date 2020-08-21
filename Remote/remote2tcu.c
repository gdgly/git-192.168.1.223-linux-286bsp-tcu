/*******************************************************************************
  Copyright(C)    2017-2017,      EAST Co.Ltd.
  File name :     Task_Server_Comm.c
  Author :
  Version:        1.0
  Date:           2018.4.8
  Description:
  Other:
  Function List:
  History:
          <author>   <time>   <version>   <desc>
                    2018.4.8   1.0      Create
*******************************************************************************/

#include    "RemoteComm.h"
//#include "mongoose.h"
#include    "cJSON_ext.h"
//#include "tcu2remote.h"
#include    "RemoteDataTransform.h"
#include    "Data.h"
#include    "TcuComm.h"
#define TAG "remote2tcu"

static time_t preRemoteConnectTime=0;
static unsigned short g_PotocolVer = 0x0100;

static void Send2Tcu(int msgId, void *data, int dataSize)
{
    SCommEventData cdata;
    memset(&cdata, 0, sizeof(cdata));

    cdata.selfCommVer = g_PotocolVer;
    cdata.msg_dest_dev_id = REMOTE_DEV_ID;
    cdata.msg_id = msgId;
    cdata.msg_data_byte_len = dataSize;
    memcpy(cdata.msg_data_arry, data, cdata.msg_data_byte_len);
    DevCallback(REMOTE_DEV_ID, msgId, (void *)&cdata, sizeof(cdata)); //执行回调
}

static void remote2tcuMsg_startCharge(cJSON *obj) // ok
{
    if (obj)
    {
        T_SChargeData data;

        memset(&data,0,sizeof(data));
        cJSON_GetInt(obj,"gunNo",&data.uiAddr);
        cJSON_GetString(obj,"user",data.user,sizeof(data.user));
        cJSON_GetChar(obj,"bmsBoostingV",&data.BmsVoltage);
        cJSON_GetChar(obj,"chg_type",&data.chargeType);
        cJSON_GetFloat(obj,"controlArg",&data.chargeValue);
        cJSON_GetFloat(obj,"user_money_left",&data.balance);
        cJSON_GetString(obj,"busy_sn",data.remoteOrder,sizeof(data.remoteOrder));
        cJSON_GetString(obj,"stopCode",data.stopPwd,sizeof(data.stopPwd));
        cJSON_GetChar(obj,"vipFlag",&data.vipLevel);
        cJSON_GetChar(obj,"vipLevel",&data.vipSubLevel);
        cJSON_GetChar(obj,"recommendPower",&data.recommendPower);
        
        cJSON_GetChar(obj,"isParallels",&data.parallelsType);
        cJSON_GetChar(obj,"OfflineChargingFalg",&data.outlineChargeEnable);
        cJSON_GetFloat(obj,"outlineChargeKwh",&data.outlineChargeKwh);
        cJSON_GetChar(obj,"getBusyOrder",&data.getBusyOrder);

        Send2Tcu(REMOTE2TCU_REMOTE_START, &data, sizeof(data));

    }

}
static void remote2tcuMsg_stopCharge(cJSON *obj) // ok
{

    if (obj)
    {
        T_SStopChargeData data;

        memset(&data,0,sizeof(data));
        cJSON_GetInt(obj,"gunNo",&data.uiAddr);
        cJSON_GetString(obj,"user",data.user,sizeof(data.user));
        cJSON_GetString(obj,"busy_sn",data.remoteOrder,sizeof(data.remoteOrder));
        cJSON_GetString(obj,"localOrder",data.localOrder,sizeof(data.localOrder));
        cJSON_GetChar(obj,"ForcedStopFlag", &data.forcedStopFlag);
        cJSON_GetChar(obj,"isJudgeUser", &data.isJudgeUser);

        Send2Tcu(REMOTE2TCU_REMOTE_STOP, &data, sizeof(data));
    }
}
static void remote2tcuMsg_LandLock(cJSON*obj) // ok
{
    T_SGroundLock data = { 0 };


    cJSON_GetChar(obj,"lockNo",&data.lockNo);
    cJSON_GetString(obj, "user", data.user, sizeof(data.user));
    cJSON_GetString(obj,"busy_sn", data.remoteOrder,sizeof(data.remoteOrder));

    Send2Tcu(REMOTE2TCU_LAND_LOCK, &data, sizeof(data));
}



static void remote2tcuMsg_VinCharge(cJSON*obj)  // ok
{
    T_SVinChargeAck data;
    memset(&data,0,sizeof(data));

    cJSON_GetInt(obj,"gunNo",&data.uiAddr);
    cJSON_GetChar(obj, "result", &data.ackResult);
    cJSON_GetString(obj, "busy_sn", data.remoteOrder, sizeof(data.remoteOrder));
    cJSON_GetFloat(obj, "balance", &data.balance);
    cJSON_GetString(obj, "user", data.user, sizeof(data.user));

    Send2Tcu(REMOTE2TCU_VIN_CHARGE_ACK, &data, sizeof(data));
}

static void remote2tcuMsg_CardCharge(cJSON*obj) // ok
{
    T_SCardChargeAck data;
    memset(&data,0,sizeof(data));

    cJSON_GetInt(obj,"gunNo",&data.uiAddr);
    cJSON_GetChar(obj,"result",&data.ackResult);
    cJSON_GetString(obj,"busy_sn",data.remoteOrder,sizeof(data.remoteOrder));

    Send2Tcu(REMOTE2TCU_CARD_CHARGE_ACK, &data, sizeof(data));
}

static void remote2tcuMsg_OrderCharge(cJSON*obj)  // ok
{
    T_SOrderChargeData data;
    memset(&data,0,sizeof(data));

    cJSON_GetInt(obj,"gunNo",&data.uiAddr);
    cJSON_GetString(obj,"user",data.user,sizeof(data.user));
    cJSON_GetInt(obj,"duration",&data.bookTime);
    cJSON_GetString(obj,"busy_sn",data.remoteOrder,sizeof(data.remoteOrder));

    data.bookTime = data.bookTime * 60;
    data.bookStart = time(NULL);

    Send2Tcu(REMOTE2TCU_ORDER_CHARGE, &data, sizeof(data));
}

static void remote2tcuMsg_CancelOrder(cJSON*obj) // ok
{
    T_SOrderChargeCancel data;
    memset(&data,0,sizeof(data));

    cJSON_GetInt(obj,"gunNo",&data.uiAddr);
    cJSON_GetString(obj,"user",data.user,sizeof(data.user));
    cJSON_GetString(obj,"busy_sn",data.remoteOrder,sizeof(data.remoteOrder));

    Send2Tcu(REMOTE2TCU_CANCEL_ORDER, &data, sizeof(data));
}

static void remote2tcuMsg_QR_SET(cJSON *obj) // ok
{
    if (obj)
    {
        T_SQrcodeUpdate data = {0};
            
        cJSON_GetChar(obj, "Gun_No", &data.gunNo);
        cJSON_GetInt(obj,"qrlen", &data.qrLen);
        cJSON_GetString(obj,"qrcode", data.qrCode, sizeof(data.qrCode));
        cJSON_GetChar(obj, "isWhole", &data.isWhole);
        
        Send2Tcu(REMOTE2TCU_QR_SET, &data, sizeof(data));
    }
}

static void remote2tcuMsg_TimeSet(cJSON*obj)  //ok
{
    T_STimeSet data = { 0 };

    cJSON_GetBytes(obj,"sysTime",data.time,6);

    Send2Tcu(REMOTE2TCU_TIME_SYNC, &data, sizeof(data));
}

static void remote2tcuMsg_Price_Set(cJSON *obj) //ok
{
    if (obj)
    {
        T_SSetChargePrice data;
        memset(&data,0,sizeof(data));

        cJSON_GetInt(obj,"version",&data.version);//计费版本
        cJSON_GetChar(obj,"hasViewTotal",&data.isShowUnitPrice);//是否显示总单价
        cJSON_GetChar(obj,"hasTimePrice",&data.isHasNewPrice);//是否有定时电价
        cJSON_GetBytes(obj,"timePriceVaildTime",data.newPriceVaildTime,sizeof(data.newPriceVaildTime));//定时电价生效时间
        cJSON_GetChar(obj,"hasStopPrice",&data.isHasStopPrice);//有无停车费
        cJSON_GetChar(obj,"segNum",&data.priceCount);//费用区间数

        cJSON* array = cJSON_GetObjectItem(obj,"priceInfo");
        int i;
        for(i=0;i<data.priceCount;i++)
        {
            char No[10]={0};
            snprintf(No,sizeof(No),"%d",i);
            cJSON_GetBytes(array,No,(char *)&data.price[i],sizeof(T_SChargePrice));//起始时间
        }

        DataRemote2Tcu(REMOTE2TCU_PRICE_SET, &data, &data, sizeof(data));

        Send2Tcu(REMOTE2TCU_PRICE_SET, &data, sizeof(data));
    }
}

static void remote2tcuMsg_StopPriceSet(cJSON *obj) //ok
{
    T_SSetStopPrice data;
    memset(&data,0,sizeof(data));

    cJSON_GetInt(obj,"version",&data.version);
    cJSON_GetFloat(obj,"price",&data.price);
    cJSON_GetChar(obj,"isFree",&data.isFreeStop);
    cJSON_GetShort(obj,"freeTime",&data.freeStopTime);

    Send2Tcu(REMOTE2TCU_STOP_PRICE_SET, &data, sizeof(data));
}


static void remote2tcuMsg_MsgTimeSpan(cJSON *obj) //ok
{
    if(!obj)return;

    T_SSetMsgCycle data;
    memset(&data,0,sizeof(data));

    cJSON_GetShort(obj, "standby_msg_period", &data.standyMsgTime);
    cJSON_GetShort(obj, "chgbusy_msg_period", &data.chargeMsgTime);

    Send2Tcu(REMOTE2TCU_MSG_PERIOD, &data, sizeof(data));
}


static void remote2tcuMsg_Reboot(cJSON *obj)  // ok
{
    if(!obj)return;

    T_SReset data;
    memset(&data,0,sizeof(data));

    cJSON_GetChar(obj, "cmd",  &data.cmd);

    Send2Tcu(REMOTE2TCU_RMOETE_REBOOT, &data, sizeof(data));
}


static void remote2tcuMsg_service_start(cJSON *obj) // ok
{
    if(!obj)return;

    T_SChargeService data;
    memset(&data,0,sizeof(data));

    cJSON_GetInt(obj,"gunNo",&data.uiAddr);
    cJSON_GetChar(obj,"rightNow",&data.rightNow);

    Send2Tcu(REMOTE2TCU_RESUME_SERVICE, &data, sizeof(data));
}

static void remote2tcuMsg_service_stop(cJSON *obj) // ok
{
    if(!obj)return;

    T_SChargeService data;
    memset(&data,0,sizeof(data));

    cJSON_GetInt(obj,"gunNo",&data.uiAddr);
    cJSON_GetChar(obj,"rightNow",&data.rightNow);

    Send2Tcu(REMOTE2TCU_STOP_SERVICE, &data, sizeof(data));
}

static void remote2tcuMsg_bill_comfirm(cJSON *obj) // ok
{
    if(!obj)return;

    T_SUploadBillAck data;
    memset(&data,0,sizeof(data));

    cJSON_GetInt(obj,"gunNo",&data.uiAddr);
    cJSON_GetChar(obj,"result",&data.ackResult);
    cJSON_GetString(obj,"busy_sn",data.remoteOrder,sizeof(data.remoteOrder));
    cJSON_GetString(obj,"localOrder",data.localOrder,sizeof(data.localOrder));

    Send2Tcu(REMOTE2TCU_BILL_REPORT_ACK, &data, sizeof(data));
}

static void remote2tcuMsg_stopBillComfirm(cJSON*obj)
{

}

static void remote2tcuMsg_CheckCard(cJSON*obj)  // ok
{
    T_SCheckCardDataAck data;
    memset(&data,0,sizeof(data));

    cJSON_GetChar(obj,"result",&data.userState);
    cJSON_GetString(obj,"user", data.user, sizeof(data.user));
    cJSON_GetString(obj,"vin", data.vin, sizeof(data.vin));
    cJSON_GetString(obj,"passwd", data.pwd, sizeof(data.pwd));

    Send2Tcu(REMOTE2TCU_CHECK_CARD_ACK, &data, sizeof(data));
}

static void remote2tcuMsg_UpdateReq(cJSON*obj)
{
    T_RemoteUpgradeDownload data;
    memset(&data,0,sizeof(data));
    cJSON_GetChar(obj,"result",&data.result);
    Send2Tcu(REMOTE2TCU_UPGRADE_REQ, &data, sizeof(data));
}
static void remote2tcuMsg_GetLog(cJSON*obj)
{
    T_GenernalAck data;
    memset(&data,0,sizeof(data));
    cJSON_GetChar(obj,"result",&data.result);
    Send2Tcu(REMOTE2TCU_GETLOG_REQ, &data, sizeof(data));
}

static void remote2tcuMsg_Param_Report(cJSON *obj)
{
    if(!obj)return;
    T_GenernalAck data;
    memset(&data,0,sizeof(data));

    //cJSON_GetInt(obj,"gunNo",&data.gunNo);

    Send2Tcu(REMOTE2TCU_PARA_REPORT_ACK, &data, sizeof(data));
}

static void remote2tcuMsg_DownloadRemoteAck(cJSON *obj)
{
    if(!obj)return;
    T_FirmwareDownloadInfoAck data;
    memset(&data,0,sizeof(data));

    cJSON_GetChar(obj,"result",&data.result);

    Send2Tcu(REMOTE2TCU_DOWNLOAD_REMOTE_ACK, &data, sizeof(data));
}

static void remote2tcuMsg_UpgradeResultAck(cJSON *obj)
{
    if(!obj)return;
    T_FirmwareDownloadInfoAck data;
    memset(&data,0,sizeof(data));

    cJSON_GetChar(obj,"result",&data.result);

    Send2Tcu(REMOTE2TCU_UPGRADE_RESULT_ACK, &data, sizeof(data));
}


static void remote2tcuMsg_SetSettings(cJSON *obj)
{
 
}

static void remote2tcuMsg_SetRemoteSettingsAck(cJSON *obj)
{
    if(!obj)return;
    T_ReomteSetSettingsAck data;
    memset(&data,0,sizeof(data));

    cJSON_GetChar(obj, "result", &data.result);
    cJSON_GetInt(obj, "gunNo", &data.devId);
    cJSON_GetChar(obj, "type", &data.settingType);

    Send2Tcu(REMOTE2TCU_SET_REMOTE_SETTINGS_ACK, &data, sizeof(data));
}

static void remote2tcuMsg_GetSettingsAck(cJSON *obj)
{
    //if(!obj)return;
    //T_ReomteGetSettingsAck data;
    //memset(&data,0,sizeof(data));

    //cJSON_GetChar(obj, "result", &data.result);
    //if(data.result == 0)
    //{
    //    cJSON_GetInt(obj, "gunNo", &data.devId);
    //    cJSON_GetInt(obj, "count", &data.count);
    //    cJSON_GetChar(obj, "type", &data.settingType);
    //    int i = 0;
    //    for(i = 0; i < data.count; i++)
    //    {
    //        char key[16] = {0};
    //        char keyValue[32] = {0};
    //        char valueKey[32] = {0};
    //        sprintf(key, "key%d", i);
    //        sprintf(valueKey, "keyValue%d", i);
    //        cJSON_GetString(obj, key, keyValue, sizeof(keyValue)); //获取参数的key
    //        if(data.settingType == REMOTE_OTHER_SETTINGS)
    //        {
    //            if(!strcmp(keyValue, g_paramKey[PARAM_OCPP_WEBURL])) //OCPP握手需要内容
    //            {
    //                strcpy(data.key[i], keyValue);
    //                cJSON_GetString(obj, valueKey, data.keyValue[i], sizeof(data.keyValue[i]));
    //            }
    //            else if(!strcmp(keyValue, g_paramKey[PARAM_OCPP_OFFLINEMODEENABLE])) //界面是否显示离线模式
    //            {
    //                strcpy(data.key[i], keyValue);
    //                cJSON_GetChar(obj, valueKey, &data.keyValue[i][0]);
    //            }
    //            else if(!strcmp(keyValue, g_paramKey[PARAM_OCPP_OFFLINEPASSWORD])) //离线模式时候的启动密码
    //            {
    //                strcpy(data.key[i], keyValue);
    //                cJSON_GetString(obj, valueKey, data.keyValue[i], sizeof(data.keyValue[i]));
    //            }
    //            else if(!strcmp(keyValue, g_paramKey[PARAM_OCPP_CHARGEPOINTMODEL])) //OCPP握手需要内容
    //            {
    //                strcpy(data.key[i], keyValue);
    //                cJSON_GetString(obj, valueKey, data.keyValue[i], sizeof(data.keyValue[i]));
    //            }
    //            else
    //            {
    //                AppLogOut("%s 非法key %s", TAG, key);
    //            }
    //        }
    //    }
    //}
    //Send2Tcu(REMOTE2TCU_GET_REMOTE_SETTINGS_ACK, &data, sizeof(data));
}




static void remote2tcuMsg_Heart(cJSON*obj) //ok
{
    T_SRemoteHeart data = { 0 };
    char version[50] = { 0 };
    char initFlag=0;
   
    cJSON_GetChar(obj,"initFlag",&initFlag);
    cJSON_GetChar(obj,"loginState",&data.loginStat);
    cJSON_GetString(obj,"remoteVersion",version,sizeof(version));
    char *p = strchr(version, '-');//去掉-后面的字符
    if(p) *p = 0;
    sscanf(version, "%hhu.%hhu.%hhu", &data.ver[0], &data.ver[1], &data.ver[2]);
    cJSON_GetShort(obj,"commVersion",&data.srcCommVer);
    data.selfCommVer = g_PotocolVer;

    Send2Tcu(REMOTE2TCU_HEART_BEAT, &data, sizeof(data));
}

static void remote2tcuMsg_FaultMsgAck(cJSON*obj)
{
    if(!obj)return;
    T_SFaultInfoAck data;
    memset(&data,0,sizeof(data));

    cJSON_GetChar(obj,"result",&data.ackResult);
    cJSON_GetInt(obj,"uniqueId",&data.uniqueId);

    Send2Tcu(REMOTE2TCU_FAULT_MSG_ACK, &data, sizeof(data));

}
static void remote2tcuMsg_Pwr_Adjust(cJSON *obj) // OK
{
    if(!obj)return;
    T_SPowerCtrl data;
    memset(&data,0,sizeof(data));

    cJSON_GetInt(obj,"gunNo",&data.uiAddr);
    cJSON_GetChar(obj,"controlType",&data.type);
    cJSON_GetFloat(obj,"controlValue",&data.value);

    Send2Tcu(REMOTE2TCU_PWR_ADJUST, &data, sizeof(data));
}

static void remote2tcuMsg_GetBlackList(cJSON*obj) // ok
{

}

static void remote2tcuMsg_OrderTimeout(cJSON*obj) // ok
{
    T_SChargeCancelOrderAck data = { 0 };

    cJSON_GetChar(obj, "result",    &data.ackResult);
    cJSON_GetChar(obj, "gunNo",     &data.uiAddr);
    cJSON_GetString(obj, "busy_sn", data.remoteOrder, sizeof(data.remoteOrder));

    Send2Tcu(REMOTE2TCU_ORDER_TIMEOUT_ACK, &data, sizeof(data));
}




void remote2tcu_MsgParse(char *data,int size)
{
    cJSON *box=cJSON_Parse(data);
    if (box)
    {
        int func=0;
        cJSON_GetInt(box,"id",&func);
        cJSON*obj=cJSON_GetObjectItem(box,"data");

        switch(func)
        {
            case REMOTE2TCU_GETBLACKLIST_ACK://获取黑名單
            {
                remote2tcuMsg_GetBlackList(obj);
                break;
            }
            case REMOTE2TCU_HEART_BEAT://心跳报文=======周期上报                    有应答
            {
                preRemoteConnectTime=time(NULL);
                remote2tcuMsg_Heart(obj);
                break;
            }
            case REMOTE2TCU_FAULT_MSG_ACK:
            {
                remote2tcuMsg_FaultMsgAck(obj);
                break;
            }
            case REMOTE2TCU_PWR_ADJUST://充电最大功率控制
            {
                remote2tcuMsg_Pwr_Adjust(obj);
                break;
            }
            case REMOTE2TCU_TIME_SYNC://充电桩时间设置
            {
                remote2tcuMsg_TimeSet(obj);
                break;
            }
            case REMOTE2TCU_PRICE_SET://费率和时段设置
            {
                remote2tcuMsg_Price_Set(obj);
                break;
            }
            case REMOTE2TCU_STOP_PRICE_SET://停车费设置
            {
                remote2tcuMsg_StopPriceSet(obj);
                break;
            }
            case REMOTE2TCU_MSG_PERIOD ://报文周期设置
            {
                remote2tcuMsg_MsgTimeSpan(obj);
                break;
            }
            case REMOTE2TCU_RMOETE_REBOOT://远程重启
            {
                remote2tcuMsg_Reboot(obj);
                break;
            }
            case REMOTE2TCU_STOP_SERVICE://暂停服务
            {
                remote2tcuMsg_service_stop(obj);
                break;
            }
            case REMOTE2TCU_RESUME_SERVICE://恢复服务
            {
                remote2tcuMsg_service_start(obj);
                break;
            }
            case REMOTE2TCU_QR_SET://下发二维码
            {
                remote2tcuMsg_QR_SET(obj);
                break;
            }

                //业务类型
            case REMOTE2TCU_REMOTE_START://充电启动相关报文
            {
                remote2tcuMsg_startCharge(obj);
                break;
            }
            case REMOTE2TCU_REMOTE_STOP://充电停止相关报文
            {
                remote2tcuMsg_stopCharge(obj);
                break;
            }
            case REMOTE2TCU_VIN_CHARGE_ACK://vin码充电
            {
                remote2tcuMsg_VinCharge(obj);
                break;
            }
            case REMOTE2TCU_CARD_CHARGE_ACK://刷卡充电
            {
                remote2tcuMsg_CardCharge(obj);
                break;
            }
            case REMOTE2TCU_ORDER_CHARGE://预约充电
            {
                remote2tcuMsg_OrderCharge(obj);
                break;
            }
            case REMOTE2TCU_CANCEL_ORDER://取消预约
            {
                remote2tcuMsg_CancelOrder(obj);
                break;
            }
            case REMOTE2TCU_ORDER_TIMEOUT_ACK://超时
            {
                remote2tcuMsg_OrderTimeout(obj);
                break;
            }
            case REMOTE2TCU_LAND_LOCK:
            {
                remote2tcuMsg_LandLock(obj);
                break;
            }
            case REMOTE2TCU_BILL_REPORT_ACK://上传交易记录
            {
                remote2tcuMsg_bill_comfirm(obj);
                break;
            }
            case REMOTE2TCU_STOP_BILL_REPORT_ACK:
            {
                remote2tcuMsg_stopBillComfirm(obj);
                break;
            }
            case REMOTE2TCU_CHECK_CARD_ACK:
            {
                remote2tcuMsg_CheckCard(obj);
                break;
            }
            case REMOTE2TCU_UPGRADE_REQ://固件升级
            {
                remote2tcuMsg_UpdateReq(obj);
                break;
            }
            case REMOTE2TCU_GETLOG_REQ:
            {
                remote2tcuMsg_GetLog(obj);
                break;
            }
            case REMOTE2TCU_PARA_REPORT_ACK://参数上报
            {
                remote2tcuMsg_Param_Report(obj);
                break;
            }
            case REMOTE2TCU_DOWNLOAD_REMOTE_ACK:
            {
                remote2tcuMsg_DownloadRemoteAck(obj);
                break;
            }
            case REMOTE2TCU_UPGRADE_RESULT_ACK:
            {
                remote2tcuMsg_UpgradeResultAck(obj);
                break;
            }
            case REMOTE2TCU_GET_REMOTE_SETTINGS_ACK:
            {
                remote2tcuMsg_GetSettingsAck(obj);
                break;
            }
            case REMOTE2TCU_SET_TCU_SETTINGS:
            {
                remote2tcuMsg_SetSettings(obj);
                break;
            }
            case REMOTE2TCU_SET_REMOTE_SETTINGS_ACK:
            {
                remote2tcuMsg_SetRemoteSettingsAck(obj);
                break;
            }
            case REMOTE2TCU_GETLOG_ACK:
            {
                break;
            }
            default:
                AppLogOut("%s 非法命令%x", TAG, func);
                break;
        }

        cJSON_Delete(box);
    }
}

void RemoteInit()
{
}
