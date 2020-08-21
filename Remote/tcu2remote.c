#include "tcu2remote.h"
#include "RemoteComm.h"
#include "api.h"
#include "remoteCommServer.h"
#include "cJSON_ext.h"
#include "Data.h"
#include "RemoteDataTransform.h"

#define TAG "tcu2remote"

void tcu2remote_heart(void *nc, SCommEventData *cdata)
{
    T_STcu2RemoteHeartBeat data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

//    DataTcu2Remote(TCU2REMOTE_HEART_BEAT, &data, &data, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_HEART_BEAT);
    cJSON_PutInt(json, "tick", time(NULL));
    cJSON_AddItemToObject(json, "data", obj);
    cJSON_PutInt(obj, "devState", data.sysStat); //设备状态
    cJSON_PutInt(obj, "gunNum", data.gunNum);    //枪数
    cJSON_PutChar(obj, "initFlag", GetSettingi("initFlag"));
    cJSON_PutChar(obj, "reportNow", data.rightNow);
    int i;
    char key[50] = {0};
    for (i = 0; i < data.gunNum; i++)
    {
        snprintf(key, sizeof(key), "gunNo%d", i); //枪编号
        cJSON_PutInt(obj, key, data.gunInfo[i].gunNo);
        snprintf(key, sizeof(key), "carLinkState%d", i); //车连接状态
        cJSON_PutInt(obj, key, data.gunInfo[i].gunConnectStat);
        snprintf(key, sizeof(key), "gunState%d", i); //枪当前状态
        cJSON_PutInt(obj, key, data.gunInfo[i].gunStat);
    }

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}

void tcu2remote_YaoCe(void *nc, SCommEventData *cdata)
{
    T_SYaoCE data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

//    DataTcu2Remote(TCU2REMOTE_YAO_CE, &data, &data, sizeof(data));

    unsigned char *pMeterTotal = (unsigned char *)&data.meterTotal;
    int origin_kwh_H = 0;
    int origin_kwh_L = 0;

    memcpy(&origin_kwh_L, pMeterTotal, 4);
    memcpy(&origin_kwh_H, pMeterTotal + 4, 4);

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_YAO_CE);
    cJSON_PutInt(json, "tick", time(NULL));
    cJSON_AddItemToObject(json, "data", obj);
    cJSON_PutInt(obj, "gunNo", data.uiAddr);
    cJSON_PutFloat(obj, "volt_a", data.Avoltage);                     //A相电压
    cJSON_PutFloat(obj, "volt_b", data.Bvoltage);                     //B相电压
    cJSON_PutFloat(obj, "volt_c", data.Cvoltage);                     //C相电压
    cJSON_PutFloat(obj, "curr_a", data.Acurrent);                     //A相电流
    cJSON_PutFloat(obj, "curr_b", data.Bcurrent);                     //B相电流
    cJSON_PutFloat(obj, "curr_c", data.Ccurrent);                     //C相电流
    cJSON_PutFloat(obj, "out_kw", data.outputPower);                  //输出功率
    cJSON_PutFloat(obj, "out_kwh", data.gunTotal);                    //输出有功总电量
    cJSON_PutDouble(obj, "origin_kwh", data.meterTotal);              //充电位计量表底度
    cJSON_PutFloat(obj, "outputInsideV", data.outputInsideVoltage);   //充电输出内侧电压
    cJSON_PutFloat(obj, "outputOutsiteV", data.outputOutsideVoltage); //充电输出外侧电压
    cJSON_PutFloat(obj, "outputA", data.outputCurrent);               //充电输出电流
    cJSON_PutFloat(obj, "temp", data.temperature);                    //充电机环境温度
    cJSON_PutFloat(obj, "guideV", data.guideVoltage);                 //充电导引电压
    cJSON_PutFloat(obj, "availablePower", data.availablePower);       //可用功率
    cJSON_PutFloat(obj, "gainPower", data.gainPower);                 //获取功率
    cJSON_PutFloat(obj, "outputVolt", data.outputVolt);               //充电输出电压
    cJSON_PutChar(obj, "reportNow", data.rightNow);

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}
void tcu2remoteMsg_YaoMai(void *nc, SCommEventData *cdata)
{
    T_SYaoMai data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

//    DataTcu2Remote(TCU2REMOTE_YAO_MAI, &data, &data, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_YAO_MAI);
    cJSON_AddItemToObject(json, "data", obj);
    cJSON_PutChar(obj, "gunNo", data.uiAddr);
    cJSON_PutFloat(obj, "charged_kwh", data.charge);         //已充电量
    cJSON_PutFloat(obj, "charged_rmb", data.chargeConsume);  //已充电费
    cJSON_PutFloat(obj, "service_rmb", data.serviceConsume); //已充电费
    cJSON_PutChar(obj, "soc", data.soc);
    cJSON_PutChar(obj, "charged_hours", data.chargeHour);     //已充时长
    cJSON_PutChar(obj, "charged_minutes", data.chargeMin);   //已充时长
    cJSON_PutChar(obj, "charged_secs", data.chargeSec);      //已充时长
    cJSON_PutShort(obj, "left_charge_minutes", data.remainTime); //剩余充电时长
    cJSON_PutChar(obj, "reportNow", data.rightNow);
    cJSON_PutDouble(obj, "startCharge", data.startChargePower); //已充电费

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}

void tcu2remote_YaoXin(void *nc, SCommEventData *cdata)
{
    T_SYaoXin data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

//    DataTcu2Remote(TCU2REMOTE_YAO_XIN, &data, &data, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_YAO_XIN);
    cJSON_PutInt(json, "tick", time(NULL));
    cJSON_AddItemToObject(json, "data", obj);
    cJSON_PutChar(obj, "gunNo", data.uiAddr);
    cJSON_PutChar(obj, "gunLinkState", data.gunInsert);               //枪状态
    cJSON_PutChar(obj, "state", data.gunWorkStat);                    //枪状态
    cJSON_PutFloat(obj, "outputActivePower", data.ouputVaildPower);     //输出有功功率
    cJSON_PutFloat(obj, "outputReactivePower", data.ouputInvaildPower); //输出无功功率
    cJSON_PutChar(obj, "sysBadState", data.sysFault);                 //系统故障状态
    cJSON_PutChar(obj, "modBadState", data.chargeModFault);           //充电系统故障状态
    cJSON_PutChar(obj, "chargingState", data.chargeSysFault);         //充电系统故障状态
    cJSON_PutChar(obj, "batteryState", data.batteryFault);            //电池系统故障状态
    cJSON_PutChar(obj, "dcState", data.dcOutputSwitch);               //直流输出开关状态
    cJSON_PutChar(obj, "dcOutputState", data.dcOutputContactor);      //直流输出接触器状态
    cJSON_PutChar(obj, "gunDoorState", data.gunDoor);                 //枪门状态
    cJSON_PutChar(obj, "gunResetState", data.gunBack);                //枪归位状态
    cJSON_PutChar(obj, "carBitState", data.catportStat);              //车位状态
    cJSON_PutChar(obj, "reportNow", data.rightNow);

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}

static void tcu2remoteMsg_BmsRealData(void *nc, SCommEventData *cdata)
{
    T_SBmsData data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

//    DataTcu2Remote(TCU2REMOTE_BMS_REAL_DATA, &data, &data, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_BMS_REAL_DATA);
    cJSON_PutInt(json, "tick", time(NULL));
    cJSON_AddItemToObject(json, "data", obj);
    cJSON_PutInt(obj, "gunNo", data.uiAddr);
    cJSON_PutFloat(obj, "needVolt", data.voltageNeed);                        //充电需求电压，0
    cJSON_PutFloat(obj, "needCur", data.currentNeed);                         //充电需求电流
    cJSON_PutFloat(obj, "chargeVolt", data.voltageReal);                      //充电电压测量值
    cJSON_PutFloat(obj, "chargeCur", data.currentReal);                       //充电电流测量值
    cJSON_PutFloat(obj, "singHVolt", data.singleBatChargeVoltMax);           //单体最高电压，0
    cJSON_PutChar(obj, "bmsSingleHVoltNo", data.singleBatChargeVoltMaxId); //BMS单体最高电压序号
    cJSON_PutChar(obj, "soc", data.soc);                                    //当前电池剩余容量
    cJSON_PutFloat(obj, "bmsSingleHTemp", data.singleBatteryTempHigh);        //单体电池最高温度
    cJSON_PutChar(obj, "bmsSingleTempHNo", data.singleBatteryTempHighId);   //单体电池最高温度序号
    cJSON_PutFloat(obj, "bmsSingleLTemp", data.singleBatteryTempLess);        //单体电池最低温度
    cJSON_PutChar(obj, "bmsSingleTempLNo", data.singleBatteryTempLessId);   //单体电池最低温度序号
    cJSON_PutFloat(obj, "gunTemp", data.gunTemp);                         //充电枪温度
    cJSON_PutChar(obj, "batteryType", data.batType);                     //电池类型;
    cJSON_PutFloat(obj, "batteryTeamRateVolt", data.batGroupRatedVoltage); //电池组额定总电压
    cJSON_PutFloat(obj, "chargeHVolt", data.batChargeVoltageMax);          //最高允许充电电压
    cJSON_PutFloat(obj, "chargeHCur", data.batChargeCurrentMax);           //最高允许充电电流
    cJSON_PutFloat(obj, "singAllowHVolt", data.singlBatAllowVoltageMax);       //单体最高允许电压
    cJSON_PutFloat(obj, "batteryTeamRateCap", data.batGroupRatedPower);    //电池组额定容量
    cJSON_PutFloat(obj, "batteryRatePower", data.batRatedPower);           //电池额定电量
    cJSON_PutFloat(obj, "batteryHTemp", data.batTempMax);                  //电池最高允许温度
    cJSON_PutFloat(obj, "gunPosTemp", data.gunPosTemp);                  //枪正极温度
    cJSON_PutFloat(obj, "gunNegTemp", data.gunNegTemp);                  //枪负极温度

    cJSON_PutChar(obj, "reportNow", data.rightNow);

    cJSON_PutString(obj, "carVIN", (char *)data.vin);
    cJSON_PutString(obj, "localOrder", (char *)data.localOrder);
    cJSON_PutBytes(obj, "chargerBmsVer", (char *)data.chargerBmsVer, sizeof(data.chargerBmsVer));
    cJSON_PutBytes(obj, "carBmsVer", (char *)data.carBmsVer, sizeof(data.carBmsVer));

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}

void tcu2remoteMsg_faultReport(void *nc, SCommEventData *cdata)
{
//    T_SFaultInfo msgData;
//    memcpy(&msgData, cdata->msg_data_arry, sizeof(msgData));

    R_SFaultInfo data;
    memcpy(&data,cdata->msg_data_arry, sizeof(data));
//    DataTcu2Remote(TCU2REMOTE_FAULT_MSG, &msgData, &data, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_FAULT_MSG);
    cJSON_PutInt(json, "tick", time(NULL));
    cJSON_AddItemToObject(json, "data", obj);
    cJSON_PutInt(obj, "uniqueId", data.uniqueId);
    cJSON_PutInt(obj, "faultId", data.faultId);
    cJSON_PutChar(obj, "gunNo", data.gunNo);
    cJSON_PutBytes(obj, "startTime", data.faultTime, sizeof(data.faultTime));
    cJSON_PutChar(obj, "faultState", data.faultStat);
    cJSON_PutChar(obj, "faultSource", data.faultSource);
    cJSON_PutChar(obj, "faultLevel", data.faultLevel);

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}
void tcu2remote_PwrAdjust(void *nc, SCommEventData *cdata)
{
    T_SPowerCtrlAck data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

    DataTcu2Remote(TCU2REMOTE_PWR_ADJUST_ACK, &data, &data, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_PWR_ADJUST_ACK);
    cJSON_AddItemToObject(json, "data", obj);
    cJSON_PutChar(obj, "gunNo", data.uiAddr);
    cJSON_PutChar(obj, "result", data.ackResult);

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}
void tcu2remote_CardCharge(void *nc, SCommEventData *cdata)
{
    T_SChargeDataAck data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

//    DataTcu2Remote(TCU2REMOTE_CARD_CHARGE, &data, &data, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_CARD_CHARGE);
    cJSON_AddItemToObject(json, "data", obj);
    cJSON_PutInt(obj, "gunNo", data.uiAddr);
    cJSON_PutString(obj, "user", data.user);
    cJSON_PutChar(obj, "chargeType", data.chargeType);
    cJSON_PutFloat(obj, "controlArg", data.chargeValue);
    cJSON_PutString(obj, "localOrder", data.localOrder);
    cJSON_PutChar(obj, "isParallels", data.parallelsType);
    cJSON_PutChar(obj, "userType", data.userType);

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}

void tcu2remote_timeSync(void *nc, SCommEventData *cdata)
{
    T_GenernalAck data;
    memcpy(&data, cdata->msg_data_arry, cdata->msg_data_byte_len);

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_TIME_SYNC_ACK);
    cJSON_AddItemToObject(json, "data", obj);
    cJSON_PutChar(obj, "result", data.result);

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}

void tcu2remote_PriceSet(void *nc, SCommEventData *cdata)
{
    T_SSetChargePriceAck data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_PRICE_SET_ACK);
    cJSON_AddItemToObject(json, "data", obj);
    cJSON_PutChar(obj, "result", data.ackResult);

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}
void tcu2remoteMsg_stopPriceSet(void *nc, SCommEventData *cdata)
{
    T_SSetStopPriceAck data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_STOP_PRICE_SET_ACK);
    cJSON_AddItemToObject(json, "data", obj);

    cJSON_PutChar(obj, "result", data.ackResult);

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}

void tcu2remote_MsgPeriod(void *nc, SCommEventData *cdata)
{
    T_SSetMsgCycleAck data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_MSG_PERIOD_ACK);
    cJSON_AddItemToObject(json, "data", obj);

    cJSON_PutChar(obj, "result", data.ackResult);

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}
void tcu2remote_BookMsg(void *nc, SCommEventData *cdata)
{
    T_SOrderChargeDataAck data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

    DataTcu2Remote(TCU2REMOTE_ORDER_CHARGE_ACK, &data, &data, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_ORDER_CHARGE_ACK);
    cJSON_AddItemToObject(json, "data", obj);
    cJSON_PutInt(obj, "gunNo", data.uiAddr);
    cJSON_PutChar(obj, "result", data.ackResult);
    cJSON_PutString(obj, "busy_sn", data.remoteOrder);

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}
void tcu2remote_CancelOrder(void *nc, SCommEventData *cdata)
{
    T_SOrderChargeCancelAck data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

    DataTcu2Remote(TCU2REMOTE_CANCEL_ORDER_ACK, &data, &data, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_CANCEL_ORDER_ACK);
    cJSON_AddItemToObject(json, "data", obj);
    cJSON_PutInt(obj, "gunNo", data.uiAddr);
    cJSON_PutChar(obj, "result", data.ackResult);
    cJSON_PutString(obj, "busy_sn", data.remoteOrder);

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}
void tcu2remote_CheckCard(void *nc, SCommEventData *cdata)
{
    T_SCheckCardData data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

//    DataTcu2Remote(TCU2REMOTE_CHECK_CARD, &data, &data, sizeof(data));

    char buf[50] = {0};
    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_CHECK_CARD);
    cJSON_AddItemToObject(json, "data", obj);

    cJSON_PutString(obj, "user", data.user);
    cJSON_PutString(obj, "passwd", data.pwd);
    cJSON_PutChar(obj, "userType", data.userType);

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}

void tcu2remote_OrderTimeout(void *nc, SCommEventData *cdata)
{
    T_SOrderChargeCancel data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

//    DataTcu2Remote(TCU2REMOTE_ORDER_TIMEOUT, &data, &data, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_ORDER_TIMEOUT);
    cJSON_AddItemToObject(json, "data", obj);
    cJSON_PutInt(obj, "gunNo", data.uiAddr);
    cJSON_PutString(obj, "user", data.user);
    cJSON_PutString(obj, "busy_sn", data.remoteOrder);
    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}
void tcu2remote_LandLock(void *nc, SCommEventData *cdata)
{
}
void tcu2remote_RemoteReboot(void *nc, SCommEventData *cdata)
{
    T_SResetAck data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_RMOETE_REBOOT_ACK);
    cJSON_AddItemToObject(json, "data", obj);
    cJSON_PutChar(obj, "result", data.ackResult);

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}

void Ver2Str(cJSON *obj, char *key, unsigned char *version)
{
    char buf[32] = {0};
    sprintf(buf, "%d.%d.%d", version[0], version[1], version[2]);
    cJSON_PutString(obj, key, buf);
}

void tcu2remote_UpgradeResult(void *nc, SCommEventData *cdata)
{
    T_RemoteUpgradeAck data;
    memcpy(&data, cdata->msg_data_arry, cdata->msg_data_byte_len);

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_UPGRADE_RESULT);
    cJSON_AddItemToObject(json, "data", obj);
    cJSON_PutChar(obj, "result", data.result);
    Ver2Str(obj, "commVersion", data.commVersion);
    Ver2Str(obj, "pcuVersion", data.pcuVersion);
    Ver2Str(obj, "hmiVersion", data.hmiVersion);
    Ver2Str(obj, "remoteVersion", data.remoteVersion);
    Ver2Str(obj, "tcuVersion", data.tcuVersion);

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}

void tcu2remote_GetLogAck(void *nc, SCommEventData *cdata)
{
    T_GenernalAck data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_GETLOG_ACK);
    cJSON_AddItemToObject(json, "data", obj);
    cJSON_PutChar(obj, "result", data.result);
    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}

void tcu2remote_GetSettings(void *nc, SCommEventData *cdata)
{
    T_ReomteGetSettings data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();

    cJSON_PutInt(json, "id", TCU2REMOTE_GET_REMOTE_SETTINGS);
    cJSON_AddItemToObject(json, "data", obj);
    cJSON_PutChar(obj, "gunNo", data.devId);
    cJSON_PutInt(obj, "type", data.settingType);
    cJSON_PutInt(obj, "count", data.count);
    int i = 0;
    for(i = 0; i < data.count; i++)
    {
        char key[16] = {0};
        sprintf(key, "key%d", i);
        cJSON_PutString(obj, key, data.key[i]);
    }

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}

void tcu2remote_SetTcuSettingsAck(void *nc, SCommEventData *cdata)
{
    T_ReomteSetSettingsAck data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();

    cJSON_PutInt(json, "id", TCU2REMOTE_SET_TCU_SETTINGS_ACK);
    cJSON_AddItemToObject(json, "data", obj);

    cJSON_PutChar(obj, "result", data.result);
    cJSON_PutChar(obj, "gunNo", data.devId);
    cJSON_PutInt(obj, "type", data.settingType);

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}

void tcu2remote_SetRemoteSettings(void *nc, SCommEventData *cdata)
{
    T_ReomteSetSettings data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();

    cJSON_PutInt(json, "id", TCU2REMOTE_SET_REMOTE_SETTINGS);
    cJSON_AddItemToObject(json, "data", obj);
    cJSON_PutChar(obj, "gunNo", data.devId);
    cJSON_PutInt(obj, "type", data.settingType);
    cJSON_PutInt(obj, "count", data.count);
    int i = 0;
    for(i = 0; i < data.count; i++)
    {
        char key[16] = {0};
        char valueKey[32] = {0};
        sprintf(key, "key%d", i);
        sprintf(valueKey, "keyValue%d", i);
        if(data.settingType == REMOTE_OTHER_SETTINGS)
        {
            if(!strcmp(data.key[i], g_paramKey[PARAM_OCPP_WEBURL])) //OCPP握手需要内容
            {
                cJSON_PutString(obj, key, data.key[i]);
                cJSON_PutString(obj, valueKey, data.keyValue[i]);
            }
            else if(!strcmp(data.key[i], g_paramKey[PARAM_OCPP_OFFLINEMODEENABLE])) //界面是否显示离线模式
            {
                cJSON_PutString(obj, key, data.key[i]);
                cJSON_PutChar(obj, valueKey, data.keyValue[i][0]);
            }
            else if(!strcmp(data.key[i], g_paramKey[PARAM_OCPP_OFFLINEPASSWORD])) //离线模式时候的启动密码
            {
                cJSON_PutString(obj, key, data.key[i]);
                cJSON_PutString(obj, valueKey, data.keyValue[i]);
            }
            else if(!strcmp(data.key[i], g_paramKey[PARAM_OCPP_CHARGEPOINTMODEL])) //OCPP握手需要内容
            {
                cJSON_PutString(obj, key, data.key[i]);
                cJSON_PutString(obj, valueKey, data.keyValue[i]);
            }
            else
            {
                AppLogOut("%s 非法key %s", TAG, key);
            }
        }
    }

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}

void tcu2remote_StopService(void *nc, SCommEventData *cdata)
{
    T_SChargeServiceAck data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

    DataTcu2Remote(TCU2REMOTE_STOP_SERVICE_ACK, &data, &data, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_STOP_SERVICE_ACK);
    cJSON_AddItemToObject(json, "data", obj);
    cJSON_PutInt(obj, "gunNo", data.uiAddr);
    cJSON_PutChar(obj, "result", data.ackResult);

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}
void tcu2remote_ResumeService(void *nc, SCommEventData *cdata)
{
    T_SChargeServiceAck data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

    DataTcu2Remote(TCU2REMOTE_RESUME_SERVICE_ACK, &data, &data, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_RESUME_SERVICE_ACK);
    cJSON_AddItemToObject(json, "data", obj);
    cJSON_PutInt(obj, "gunNo", data.uiAddr);
    cJSON_PutChar(obj, "result", data.ackResult);

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}
void tcu2remote_RemoteStart(void *nc, SCommEventData *cdata)
{
    T_SChargeDataAck data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

//    DataTcu2Remote(TCU2REMOTE_REMOTE_START_ACK, &data, &data, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_REMOTE_START_ACK);
    cJSON_AddItemToObject(json, "data", obj);
    cJSON_PutInt(obj, "gunNo", data.uiAddr);
    cJSON_PutChar(obj, "result", data.ackResult);
    cJSON_PutString(obj, "busy_sn", data.remoteOrder);
    cJSON_PutString(obj, "localOrder", data.localOrder);

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}
void tcu2remote_RemoteStop(void *nc, SCommEventData *cdata)
{
    T_SStopChargeDataAck data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

    DataTcu2Remote(TCU2REMOTE_REMOTE_STOP_ACK, &data, &data, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_REMOTE_STOP_ACK);
    cJSON_AddItemToObject(json, "data", obj);

    cJSON_PutInt(obj, "gunNo", data.uiAddr);
    cJSON_PutChar(obj, "result", data.ackResult);
    cJSON_PutString(obj, "busy_sn", data.remoteOrder);

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}
void tcu2remoteMsg_VinCharge(void *nc, SCommEventData *cdata)
{
    T_SVinCharge data;
    memcpy(&data, cdata->msg_data_arry, cdata->msg_data_byte_len);

//    DataTcu2Remote(TCU2REMOTE_VIN_CHARGE, &data, &data, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_VIN_CHARGE);
    cJSON_AddItemToObject(json, "data", obj);

    cJSON_PutInt(obj, "gunNo", data.uiAddr);
    cJSON_PutString(obj,"vinCode",data.vin);
    cJSON_PutString(obj,"vinPasswd",data.vinPwd);
    cJSON_PutChar(obj,"chargeType",data.chargeType);
    cJSON_PutFloat(obj,"controlArg",data.chargeValue);
    cJSON_PutString(obj,"localOrder",data.localOrder);
    cJSON_PutChar(obj,"parallelsType",data.parallelsType);

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}

static void Time7ToTime6(Time7Bits *time7, Time6Bits *time6)
{
    time6->year = time7->year;
    time6->month = time7->month;
    time6->date = time7->date;
    time6->hour = time7->hour;
    time6->minute = time7->minute;
    time6->sec = time7->ms / 1000;
}

typedef struct
{
    unsigned char startTime[6]; //起始时间
    float charge;               //电量
    float chargeCosume;         //电量消费
    float serviceConsume;       //服务费
} T_SConsume;

void tcu2remote_BillReport(void *nc, SCommEventData *cdata)
{
    T_SBillData data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

//    DataTcu2Remote(TCU2REMOTE_BILL_REPORT, &data, &data, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_BILL_REPORT);
    cJSON_AddItemToObject(json, "data", obj);

    cJSON_PutInt(obj, "gunNo", data.uiAddr);
    cJSON_PutString(obj, "user", data.user);
    cJSON_PutChar(obj, "chargeType", data.userType);
    cJSON_PutChar(obj, "billResult", data.billState);
    cJSON_PutInt(obj, "stopReason", data.stopCase);
    Time6Bits t;
//    Time7ToTime6(&data.chargeStart, &t);
    cJSON_PutBytes(obj, "startTime", (char *)&t, sizeof(t));
//    Time7ToTime6(&data.chargeEnd, &t);
    cJSON_PutBytes(obj, "endTime", (char *)&t, sizeof(t));
    cJSON_PutDouble(obj, "startCharge", data.startChargePower);
    cJSON_PutDouble(obj, "endCharge", data.currChargePower);
    cJSON_PutChar(obj, "startSoc", data.startSoc);
    cJSON_PutChar(obj, "endSoc", data.endSoc);
    cJSON_PutFloat(obj, "totalCharge", data.totalPower);
    cJSON_PutFloat(obj, "totalChargeCosume", data.totalChargeConsume);
    cJSON_PutFloat(obj, "totalServiceConsume", data.totalServiceConsume);
    cJSON_PutString(obj, "order", data.remoteOrder);
    cJSON_PutString(obj, "vin", data.vin);
    cJSON_PutString(obj, "localOrder", data.order);
    cJSON_PutChar(obj, "chargePriceCount", data.chargePriceCount);
    cJSON_PutChar(obj, "isParallels", data.isParallels);
    cJSON_PutChar(obj, "isParallelsOrder", data.isParallelsOrder);
    cJSON_PutChar(obj, "isHost", data.isHost);
    cJSON_PutFloat(obj, "startBalance", data.startBalance);
    cJSON_PutInt(obj, "priceVersion", data.priceVersion);

    cJSON *array = cJSON_CreateObject();
    int i;
    for (i = 0; i < data.chargePriceCount; i++)
    {
        T_SConsume t;
        char No[10] = {0};
        snprintf(No, sizeof(No), "%d", i);
        memcpy(t.startTime, data.chargeCosume[i].startTime, sizeof(t.startTime));
        t.charge = data.chargeCosume[i].charge;
        t.chargeCosume = data.chargeCosume[i].chargeCosume;
        t.serviceConsume = data.chargeCosume[i].serviceConsume;
        cJSON_PutBytes(array, No, (char *)&t, sizeof(T_SConsume)); //起始时间
    }
    cJSON_AddItemToObject(obj, "consumeInfo", array); //区间电费信息


    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}

void tcu2remoteMsg_StopBillReport(void *nc, SCommEventData *cdata)
{
}

void tcu2remote_QrSet(void *nc, SCommEventData *cdata)
{
    T_SQrcodeUpdateAck data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

    DataTcu2Remote(TCU2REMOTE_QR_SET_ACK, &data, &data, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_QR_SET_ACK);
    cJSON_AddItemToObject(json, "data", obj);

    cJSON_PutChar(obj, "result", data.ackResult);

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}

//tcu上电后发送给远程服务
void tcu2remote_ParaReport(void *nc, SCommEventData *cdata)
{
    T_SSysLogin data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

//    DataTcu2Remote(TCU2REMOTE_PARA_REPORT, &data, &data, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_PARA_REPORT);
    cJSON_AddItemToObject(json, "data", obj);
    cJSON_PutString(obj, "operationUrl", (char *)data.operationUrl);                     //运营域名/IP
    cJSON_PutInt(obj, "operationPort", data.operationPort);                   //运营端口
    cJSON_PutString(obj, "operationManageUrl", (char *)data.operationManageUrl);         //运维域名/IP
    cJSON_PutInt(obj, "operationManagePort", data.operationManagePort);       //运维端口
//    cJSON_PutInt(obj, "standby_msg_period", GetSettingi2("standby_msg_period", 60)); //待机消息间隔
//    cJSON_PutInt(obj, "chgbusy_msg_period", GetSettingi2("chgbusy_msg_period", 5));  //充电中消息间隔
    Ver2Str(obj, "commVersion", (char *)data.commVersion);         //通信协议版本
    Ver2Str(obj, "pcuVersion", data.pcuVersion);           //控制版本
    Ver2Str(obj, "hmiVersion", data.hmiVersion);           //控制版本
    Ver2Str(obj, "remoteVersion", data.remoteVersion);           //控制版本
    Ver2Str(obj, "tcuVersion", data.tcuVersion);           //控制版本
    cJSON_PutString(obj, "cpuId", (char *)data.cpuId);//cpuid
    cJSON_PutFloat(obj, "pilePower", data.pilePower);      //桩当前功率
    cJSON_PutString(obj, "product", (char *)data.poductId);   //序列号
    cJSON_PutString(obj, "mqttUser", (char *)data.mqttUser);   //mqtt账号
    cJSON_PutString(obj, "mqttPwd", (char *)data.mqttPwd);   //mqtt密码
    cJSON_PutString(obj, "propertyNo", (char *)data.property); //资产号
    cJSON_PutInt(obj, "gunNum", data.gunNum);
    int i;
    for (i = 0; i < data.gunNum; i++)
    {
        char key[50] = {0};
        char keyValue[32] = {0};
        snprintf(key, sizeof(key), "gunNo%d", i); //枪编号
        cJSON_PutInt(obj, key, data.gunInfo[i].gunNo);
        snprintf(key, sizeof(key), "gunPower%d", i); //枪当前功率
        cJSON_PutFloat(obj, key, data.gunInfo[i].power);
        snprintf(key, sizeof(key), "gunType%d", i); //枪类型
        cJSON_PutInt(obj, key, data.gunInfo[i].gunType);
        snprintf(key, sizeof(key), "gunVersion%d", i); //枪版本
        sprintf(keyValue, "%d.%d.%d", data.gunInfo[i].gunVersion[0], data.gunInfo[i].gunVersion[1], data.gunInfo[i].gunVersion[2]);
        cJSON_PutString(obj, key, keyValue);
    }

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}
//下载后台服务器的升级包
void tcu2remote_downloadUpdate(void *nc,SCommEventData*cdata)
{
    cJSON *json=cJSON_CreateObject();
    cJSON *obj=cJSON_CreateObject();
    cJSON_PutInt(json,"id",TCU2REMOTE_DOWNLOAD_UPDATE);
    cJSON_PutInt(json,"tick",time(NULL));
    cJSON_AddItemToObject(json,"data",obj);

    char *p=cJSON_Print(json);
    int size=strlen(p);
    SendRemoteData(nc,p, size);
    free(p);
    cJSON_Delete(json);
}

static void Send2Tcu(int msgId, void *data, int dataSize)
{
    SCommEventData cdata;
    memset(&cdata, 0, sizeof(cdata));

    cdata.msg_dest_dev_id = REMOTE_DEV_ID;
    cdata.msg_id = msgId;
    cdata.msg_data_byte_len = dataSize;
    memcpy(cdata.msg_data_arry, data, cdata.msg_data_byte_len);
    DevCallback(REMOTE_DEV_ID, msgId, (void *)&cdata, sizeof(cdata)); //执行回调
}

void tcu2remote_updateRemote(void *nc, SCommEventData*cdata)
{
    T_FirmwareDownloadInfo data;
    memcpy(&data, cdata->msg_data_arry, sizeof(data));

    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_DOWNLOAD_REMOTE);
    cJSON_AddItemToObject(json, "data", obj);

    cJSON_PutBytes(obj, "version", data.version, sizeof(data.version));
    cJSON_PutString(obj, "md5", data.md5);
    cJSON_PutString(obj, "url", data.url);

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}

void tcu2remote_rebootRemote(void *nc, SCommEventData*cdata)
{
    cJSON *json = cJSON_CreateObject();
    cJSON *obj = cJSON_CreateObject();
    cJSON_PutInt(json, "id", TCU2REMOTE_REBOOT_REMOTE);
    cJSON_AddItemToObject(json, "data", obj);

    char *p = cJSON_Print(json);
    int size = strlen(p);
    SendRemoteData(nc, p, size);
    free(p);
    cJSON_Delete(json);
}

void tcu2remotePluginInit(void *nc, SCommEventData *cdata)
{
    T_SCcuCommCfg data;
    memcpy(&data, cdata->msg_data_arry, cdata->msg_data_byte_len);

    CcuCfgInit(&data);
}

void *Tcu2Remote_EventFilter(void *nc, void *data)
{
    SCommEventData *cdata = (SCommEventData *)data;

    switch (cdata->msg_id)
    {
        case TCU2REMOTE_PLUGIN_INIT:
        {
            tcu2remotePluginInit(nc, cdata);
            break;
        }
        case TCU2REMOTE_HEART_BEAT: //心跳报文=======周期上报                    有应答
        {
            tcu2remote_heart(nc, cdata);
            break;
        }
            //监控类型
        case TCU2REMOTE_YAO_CE: //充电中定时遥测
        {
            tcu2remote_YaoCe(nc, cdata);
            break;
        }
        case TCU2REMOTE_YAO_MAI: //充电中定时遥脉
        {
            tcu2remoteMsg_YaoMai(nc, cdata);
            break;
        }
        case TCU2REMOTE_YAO_XIN: //充电桩状态变化上报遥信
        {
            tcu2remote_YaoXin(nc, cdata);
            break;
        }
        case TCU2REMOTE_BMS_REAL_DATA:
        {
            tcu2remoteMsg_BmsRealData(nc, cdata);
            break;
        }
        case TCU2REMOTE_FAULT_MSG: //故障时上报故障信息=========                无应答
        {
            tcu2remoteMsg_faultReport(nc, cdata);
            break;
        }
            //设置类型
        case TCU2REMOTE_PWR_ADJUST_ACK: //充电最大功率控制
        {
            tcu2remote_PwrAdjust(nc, cdata);
            break;
        }
        case TCU2REMOTE_TIME_SYNC_ACK: //充电桩时间设置
        {
            tcu2remote_timeSync(nc, cdata);
            break;
        }
        case TCU2REMOTE_PRICE_SET_ACK: //费率和时段设置
        {
            tcu2remote_PriceSet(nc, cdata);
            break;
        }
        case TCU2REMOTE_STOP_PRICE_SET_ACK:
        {
            tcu2remoteMsg_stopPriceSet(nc, cdata);
            break;
        }
        case TCU2REMOTE_MSG_PERIOD_ACK: //报文周期设置
        {
            tcu2remote_MsgPeriod(nc, cdata);
            break;
        }
        case TCU2REMOTE_RMOETE_REBOOT_ACK: //远程重启
        {
            tcu2remote_RemoteReboot(nc, cdata);
            break;
        }
        case TCU2REMOTE_STOP_SERVICE_ACK: //暂停服务
        {
            tcu2remote_StopService(nc, cdata);
            break;
        }
        case TCU2REMOTE_RESUME_SERVICE_ACK: //恢复服务
        {
            tcu2remote_ResumeService(nc, cdata);
            break;
        }
        case TCU2REMOTE_QR_SET_ACK: //下发二维码
        {
            tcu2remote_QrSet(nc, cdata);
            break;
        }
            //业务类型
        case TCU2REMOTE_REMOTE_START_ACK: //充电启动相关报文
        {
            tcu2remote_RemoteStart(nc, cdata);
            break;
        }
        case TCU2REMOTE_REMOTE_STOP_ACK: //充电停止相关报文
        {
            tcu2remote_RemoteStop(nc, cdata);
            break;
        }
        case TCU2REMOTE_VIN_CHARGE: //vin码充电
        {
            tcu2remoteMsg_VinCharge(nc, cdata);
            break;
        }
        case TCU2REMOTE_CARD_CHARGE: //刷卡充电
        {
            tcu2remote_CardCharge(nc, cdata);
            break;
        }
        case TCU2REMOTE_ORDER_CHARGE_ACK: //充电桩预约锁定
        {
            tcu2remote_BookMsg(nc, cdata);
            break;
        }
        case TCU2REMOTE_CANCEL_ORDER_ACK: //主动取消预约
        {
            tcu2remote_CancelOrder(nc, cdata);
            break;
        }
        case TCU2REMOTE_ORDER_TIMEOUT: //超时预约
        {
            tcu2remote_OrderTimeout(nc, cdata);
            break;
        }
        case TCU2REMOTE_LAND_LOCK_ACK: //下发地锁
        {
            tcu2remote_LandLock(nc, cdata);
            break;
        }
        case TCU2REMOTE_BILL_REPORT: //交易记录上送报文
        {
            tcu2remote_BillReport(nc, cdata);
            break;
        }
        case TCU2REMOTE_STOP_BILL_REPORT: //上报停车费
        {
            tcu2remoteMsg_StopBillReport(nc, cdata);
            break;
        }
        case TCU2REMOTE_CHECK_CARD: //刷卡鉴权
        {
            tcu2remote_CheckCard(nc, cdata);
            break;
        }
        case TCU2REMOTE_UPGRADE_RESULT: //上报固件升级结果
        {
            tcu2remote_UpgradeResult(nc, cdata);
            break;
        }
        case TCU2REMOTE_PARA_REPORT: //参数上报
        {
            tcu2remote_ParaReport(nc, cdata);
            break;
        }
        case TCU2REMOTE_DOWNLOAD_UPDATE:
        {
            tcu2remote_downloadUpdate(nc,cdata);
            break;
        }
        case TCU2REMOTE_DOWNLOAD_REMOTE://升级远程通讯
        {
            tcu2remote_updateRemote(nc,cdata);
            break;
        }
        case TCU2REMOTE_REBOOT_REMOTE:
        {
            tcu2remote_rebootRemote(nc,cdata);
            break;
        }
        case TCU2REMOTE_GETLOG_ACK:
        {
            tcu2remote_GetLogAck(nc,cdata);
            break;
        }
        case TCU2REMOTE_GET_REMOTE_SETTINGS:
        {
            tcu2remote_GetSettings(nc,cdata);
            break;
        }
        case TCU2REMOTE_SET_TCU_SETTINGS_ACK:
        {
            tcu2remote_SetTcuSettingsAck(nc,cdata);
            break;
        }
        case TCU2REMOTE_SET_REMOTE_SETTINGS:
        {
            tcu2remote_SetRemoteSettings(nc,cdata);
            break;
        }
        default: //未知帧类型
            RunEventLogOut("Recv 未知报文类型%d", cdata->msg_id);
            break;
    }
}
