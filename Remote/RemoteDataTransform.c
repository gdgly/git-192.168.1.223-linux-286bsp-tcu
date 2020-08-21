/*****************************************************************
 *Copyright:East Group Co.,Ltd
 *FileName:RemoteDataTransform.c
 *Author:sugr
 *Version:0.1
 *Date:2019-4-1
 *Description: TCU和远程数据交换
 *
 *
 *History:
 * 1、2019-4-1 version:0.1 create by sugr
 * *************************************************************/

#include "api.h"
#include "Data.h"
#include "TcuComm.h"
#include "RemoteComm.h"
#include "RemoteDataTransform.h"

#define lg_min(a, b) (a > b ? b : a)

static T_SCcuCommCfg g_CcuCfg;

void CcuCfgInit(T_SCcuCommCfg *cfg)
{
    memcpy(&g_CcuCfg, cfg, sizeof(g_CcuCfg));
}

const T_SCcuCommCfg *GetCcuCfg(void)
{
    return &g_CcuCfg;
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

static void Time6ToTime7(Time7Bits *time7, Time6Bits *time6)
{
    time7->year = time6->year;
    time7->month = time6->month;
    time7->date = time6->date;
    time7->hour = time6->hour;
    time7->minute = time6->minute;
    time7->ms = time6->sec * 1000;
}

static int ChargeTypeTranslate(int direction, int srcType)
{
    int dstType = 1;

    if (direction == TCU2REMOTE)
    {
        if (srcType == CHARGE_TYPE_AUTO) //自动充满
        {
            dstType = REMOTE_CHARGE_TYPE_AUTO;
        }
        else if (srcType == CHARGE_TYPE_TIME) //按时间
        {
            dstType = REMOTE_CHARGE_TYPE_TIME;
        }
        else if (srcType == CHARGE_TYPE_CHARGE) //按电量
        {
            dstType = REMOTE_CHARGE_TYPE_CHARGE;
        }
        else if (srcType == CHARGE_TYPE_MONEY) //按金额
        {
            dstType = REMOTE_CHARGE_TYPE_MONEY;
        }
    }
    else if (direction == REMOTE2TCU)
    {
        if (srcType == REMOTE_CHARGE_TYPE_AUTO) //自动充满
        {
            dstType = CHARGE_TYPE_AUTO;
        }
        else if (srcType == REMOTE_CHARGE_TYPE_TIME) //按时间
        {
            dstType = CHARGE_TYPE_TIME;
        }
        else if (srcType == REMOTE_CHARGE_TYPE_CHARGE) //按电量
        {
            dstType = CHARGE_TYPE_CHARGE;
        }
        else if (srcType == REMOTE_CHARGE_TYPE_MONEY) //按金额
        {
            dstType = CHARGE_TYPE_MONEY;
        }
    }

    return dstType;
}

static int GunNoTranslate(int direction, unsigned int srcNo)
{
    int dstNo = 0;

    if (direction == TCU2REMOTE)
    {
        int i = 0;
        for (i = 0; i < g_CcuCfg.ccuNum; i++)
        {
            if (srcNo == g_CcuCfg.ccuInfo[i].addr)
            {
                dstNo = g_CcuCfg.ccuInfo[i].commAddr;
                break;
            }
        }
    }
    else if (direction == REMOTE2TCU)
    {
        int i = 0;
        for (i = 0; i < g_CcuCfg.ccuNum; i++)
        {
            if (srcNo == g_CcuCfg.ccuInfo[i].commAddr)
            {
                dstNo = g_CcuCfg.ccuInfo[i].addr;
                break;
            }
        }
    }

    return dstNo;
}

static int StartResultTransform(int ackResult)
{
    int ret = 0;

    switch (ackResult)
    {
        case START_CHARGE_ERROR_FAIL:         //CCU正常但是启动失败，失败原因来自于启动帧
        case START_CHARGE_ERROR_FAIL2:        //CCU正常但是启动失败，失败原因来自于启动完成帧
        case START_CHARGE_ERROR_CCU_FAULT:    //CCU故障
        case START_CHARGE_ERROR_TIMEOUT:      //启动超时
        case START_CHARGE_ERROR_SERVICE_STOP: //CCU服务停止
        case START_CHARGE_ERROT_PARALLELS_CFG:
        {
            ret = STARTCHARGE_RESULT_GUN_FAULT;
            break;
        }
        case START_CHARGE_ERROR_GUN_USING: //枪正在被占用
        {
            ret = STARTCHARGE_RESULT_GUN_BOOKED;
            break;
        }
        case START_CHARGE_ERROT_TCU_FAULT: //TCU故障
        {
            ret = STARTCHARGE_RESULT_SYS_BUSY;
            break;
        }
        case START_CHARGE_ERROT_GUN_DISCONNECT: //枪未连接
        {
            ret = STARTCHARGE_RESULT_GUN_DISCONNECT;
            break;
        }
        case START_CHARGE_ERROT_NO_MONEY: //余额不足
        {
            ret = STARTCHARGE_RESULT_NO_MONEY;
            break;
        }
    }

    return ret;
}

//获取本地流水号
static void GetLocalOrder(unsigned char *order, unsigned int devId, unsigned char type)
{
    time_t time_seconds = time(NULL);
    struct tm time;
    localtime_r(&time_seconds, &time);

    char timeBuf[32] = {0};
    sprintf(timeBuf, "%d%02d%02d%02d%02d%02d",
            time.tm_year + 1900,
            time.tm_mon + 1,
            time.tm_mday,
            time.tm_hour,
            time.tm_min,
            time.tm_sec);

    char product[64] = {0};
    FILE *fp = fopen(PRODUCT_ID_PATH, "r");
    if (fp)
    {
        fgets(product, 32, fp);
        char *p = strchr(product, '\n');
        if (p)
            *p = 0;
        fclose(fp);

        strcpy((char *)order, (const char *)product);

        int len = strlen(product);

        order[len++] = (GunNoTranslate(TCU2REMOTE, devId) >> 4) + '0'; //枪号
        order[len++] = GunNoTranslate(TCU2REMOTE, devId) + '0';

        order[len++] = '0';
        order[len++] = type + '0';

        memcpy(&order[len ++], timeBuf, strlen(timeBuf));
    }
    else
    {
        memcpy(order, timeBuf, strlen(timeBuf));
    }
}

//启动充电数据转换
static void StartChargeChange(void *tcuData, void *remoteData, unsigned int direction)
{
    if (direction == REMOTE2TCU)
    {
        T_SChargeData *dst = (T_SChargeData *)tcuData;

        dst->uiAddr = GunNoTranslate(direction, dst->uiAddr);
        dst->userType = USER_TYPE_APP;
        dst->chargeType = ChargeTypeTranslate(direction, dst->chargeType);
        dst->startFlag = START_CHARGE_RIGHT_NOW;
        dst->isOutline = ONLINE;
        if(dst->parallelsType == 1)
        {
            T_SParallelsCfg parallelsfg;

            GetDevMem(TCU_DEV_ID, TCU_POINT_PARALLELS_CFG, sizeof(parallelsfg), &parallelsfg);

            int i = 0;
            for(i = 0; i < parallelsfg.num; i++)
            {
                if(parallelsfg.info[i].host == dst->uiAddr)
                {
                    dst->slaveNum = parallelsfg.info[i].slaveNum;
                    int j = 0;
                    for(j = 0; j < parallelsfg.info[i].slaveNum; j++)
                    {
                        dst->slaveInfo[i].addr = parallelsfg.info[i].slave[j];
                        GetLocalOrder(dst->slaveInfo[i].order, dst->slaveInfo[i].addr, 2);
                    }

                    break;
                }
            }
            dst->parallelsType = HOST;
            GetLocalOrder(dst->loaclOrder, dst->uiAddr, 2);
            GetLocalOrder(dst->sumOrder, dst->uiAddr, 1);
        }
        else
        {
            dst->parallelsType = SINGLE;
            GetLocalOrder(dst->loaclOrder, dst->uiAddr, 0);
        }
    }
    else if (direction == TCU2REMOTE)
    {
        T_SChargeDataAck *src = (T_SChargeDataAck *)tcuData;

        src->ackResult = StartResultTransform(src->ackResult);
        src->uiAddr = GunNoTranslate(TCU2REMOTE, src->uiAddr);
    }
}

static int StopResultTransform(int ackResult)
{
    int ret = STOP_CHARGE_RESULT_OK;

    switch (ackResult)
    {
        case STOP_CHARGE_ERROR_FAIL:            //CCU回复停止充电失败
        case STOP_CHARGE_ERROR_TCU_FAULT:       //TCU故障
        case STOP_CHARGE_ERROR_BILL_STATTE_ERR: //计费状态出错
        case STOP_CHARGE_ERROR_TIMEOUT:         //超时
        case STOP_CHARGE_ERROR_CCU_FAULT:       //CCU故障
        {
            ret = STOP_CHARGE_RESULT_UNKNOW;
            break;
        }
        case STOP_CHARGE_ERROR_USER_NO_MATCH: //用户不匹配
        {
            ret = STOP_CHARGE_RESULT_USER_NO_MATCH;
            break;
        }
    }

    return ret;
}

//停止充电数据转换
static void StopChargeChange(void *tcuData, void *remoteData, unsigned int direction)
{
    if (direction == REMOTE2TCU)
    {
        T_SStopChargeData *dst = (T_SStopChargeData *)tcuData;

        dst->uiAddr = GunNoTranslate(REMOTE2TCU, dst->uiAddr);

        dst->stopFlag = 1;
    }
    else if (direction == TCU2REMOTE)
    {
        T_SStopChargeDataAck *src = (T_SStopChargeDataAck *)tcuData;

        src->ackResult = StopResultTransform(src->ackResult);
        src->uiAddr = GunNoTranslate(TCU2REMOTE, src->uiAddr);
    }
}

//功率控制数据转换
static void PowerCltChange(void *tcuData, void *remoteData, unsigned int direction)
{
    if (direction == REMOTE2TCU)
    {
        T_SPowerCtrl *dst = (T_SPowerCtrl *)tcuData;

        dst->uiAddr = GunNoTranslate(REMOTE2TCU, dst->uiAddr);
    }
    else if (direction == TCU2REMOTE)
    {
        T_SPowerCtrlAck *src = (T_SPowerCtrlAck *)tcuData;

        src->uiAddr = GunNoTranslate(TCU2REMOTE, src->uiAddr);
    }
}

//停止服务数据转换
static void StopServiceChange(void *tcuData, void *remoteData, unsigned int direction)
{
    if (direction == REMOTE2TCU)
    {
        T_SChargeService *dst = (T_SChargeService *)tcuData;

        dst->uiAddr = GunNoTranslate(REMOTE2TCU, dst->uiAddr);
        dst->cmd = 1;
    }
    else if (direction == TCU2REMOTE)
    {
        T_SChargeServiceAck *src = (T_SChargeServiceAck *)tcuData;

        src->uiAddr = GunNoTranslate(TCU2REMOTE, src->uiAddr);
    }
}

//超时取消预约数据转换
static void TimeoutCancelGun(void *tcuData, void *remoteData, unsigned int direction)
{
    if (direction == TCU2REMOTE)
    {
        T_SOrderChargeCancel *src = (T_SOrderChargeCancel *)tcuData;

        src->uiAddr = GunNoTranslate(REMOTE2TCU, src->uiAddr);
    }
}

//恢复服务数据转换
static void StartServiceChange(void *tcuData, void *remoteData, unsigned int direction)
{
    if (direction == REMOTE2TCU)
    {
        T_SChargeService *dst = (T_SChargeService *)tcuData;

        dst->uiAddr = GunNoTranslate(REMOTE2TCU, dst->uiAddr);
        dst->cmd = 2;
    }
    else if (direction == TCU2REMOTE)
    {
        T_SChargeServiceAck *src = (T_SChargeServiceAck *)tcuData;

        src->uiAddr = GunNoTranslate(TCU2REMOTE, src->uiAddr);
    }
}

//预约枪数据转换
static void OrderGunChange(void *tcuData, void *remoteData, unsigned int direction)
{
    if (direction == REMOTE2TCU)
    {
        T_SOrderChargeData *dst = (T_SOrderChargeData *)tcuData;

        dst->uiAddr = GunNoTranslate(REMOTE2TCU, dst->uiAddr);
        dst->bookTime = dst->bookTime * 60;
        dst->bookStart = time(NULL);
        dst->userType = USER_TYPE_APP;
    }
    else if (direction == TCU2REMOTE)
    {;
        T_SOrderChargeDataAck *src = (T_SOrderChargeDataAck *)tcuData;

        src->uiAddr = GunNoTranslate(TCU2REMOTE, src->uiAddr);
    }
}

//取消预约枪数据转换
static void CancelOrderGunChange(void *tcuData, void *remoteData, unsigned int direction)
{
    if (direction == REMOTE2TCU)
    {
        T_SOrderChargeCancel *dst = (T_SOrderChargeCancel *)tcuData;

        dst->uiAddr = GunNoTranslate(REMOTE2TCU, dst->uiAddr);
    }
    else if (direction == TCU2REMOTE)
    {
        T_SOrderChargeCancelAck *src = (T_SOrderChargeCancelAck *)tcuData;

        src->uiAddr = GunNoTranslate(TCU2REMOTE, src->uiAddr);
    }
}

//刷卡充电上报
static void CardChargeChange(void *tcuData, void *remoteData, unsigned int direction)
{
    if (direction == REMOTE2TCU)
    {
        T_SCardChargeAck *dst = (T_SCardChargeAck *)tcuData;

        dst->uiAddr = GunNoTranslate(REMOTE2TCU, dst->uiAddr);
    }
    else if (direction == TCU2REMOTE)
    {
        T_SChargeDataAck *src = (T_SChargeDataAck *)tcuData;

        src->uiAddr = GunNoTranslate(TCU2REMOTE, src->uiAddr);
        src->chargeType = ChargeTypeTranslate(TCU2REMOTE, src->chargeType);
        src->parallelsType = src->parallelsType == SINGLE ? 0 :1;
    }
}

//VIN充电上报
static void VinChargeChange(void *tcuData, void *remoteData, unsigned int direction)
{
    if (direction == REMOTE2TCU)
    {
        T_SVinChargeAck *dst = (T_SVinChargeAck *)tcuData;

        dst->uiAddr = GunNoTranslate(REMOTE2TCU, dst->uiAddr);
    }
    else if (direction == TCU2REMOTE)
    {
        T_SVinCharge *src = (T_SVinCharge *)tcuData;

        src->uiAddr = GunNoTranslate(TCU2REMOTE, src->uiAddr);
        src->chargeType = ChargeTypeTranslate(TCU2REMOTE, src->chargeType);
        src->parallelsType = src->parallelsType == SINGLE ? 0 :1;
    }
}

//地锁数据转换
static void LandLockChange(void *tcuData, void *remoteData, unsigned int direction)
{
    if (direction == REMOTE2TCU)
    {
    }
    else if (direction == TCU2REMOTE)
    {
    }
}

enum {
    CHECK_CARD_OK = 0,                 //00：表示可以启动；
    CHECK_CARD_USER_NOT_REGISTER = 20, //20：用户卡未注册
    CHECK_CARD_PWD_ERROR = 1,          //1：密码错误
    CHECK_CARD_RETRY =  2,             //2：平台处理失败，请重试
    CHECK_CARD_BLACKLIST = 3,          //3：黑名单卡
    CHECK_CARD_NO_MONEY = 4,           //4：资金池额度不足
    CHECK_CARD_LOCK = 5,               //5：有未完成订单
    CHECK_CARD_UNKNOW = 6,             //6：其他
    CHECK_OUTLINE = 0xFF,
};

//用户验证应答
static void CheckCardChange(void *tcuData, void *remoteData, unsigned int direction)
{
    if (direction == REMOTE2TCU)
    {
        T_SCheckCardDataAck *dst = (T_SCheckCardDataAck *)tcuData;

        if(dst->userState == CHECK_CARD_OK)
        {
            dst->userState = 0;
        }
        else if(dst->userState == CHECK_CARD_USER_NOT_REGISTER)
        {
            dst->userState = USER_STATE_NOT_REGISTER;
        }
        else if(dst->userState == CHECK_CARD_PWD_ERROR)
        {
            dst->userState = USER_STATE_PWD_ERROR;
        }
        else if(dst->userState == CHECK_CARD_RETRY)
        {
            dst->userState = USER_STATE_RETRY;
        }
        else if(dst->userState == CHECK_CARD_BLACKLIST)
        {
            dst->userState = USER_STATE_BLACK;
        }
        else if(dst->userState == CHECK_CARD_NO_MONEY)
        {
            dst->userState = USER_STATE_NO_MONEY;
        }
        else if(dst->userState == CHECK_CARD_LOCK)
        {
            dst->userState = USER_STATE_LOCKED;
        }
        else if(dst->userState == CHECK_OUTLINE)
        {
            dst->userState = USER_STATE_OUTLINE;
        }
        else
        {
            dst->userState = USER_STATE_RESERVE;
        }

    }
    else if (direction == TCU2REMOTE)
    {
        T_SCheckCardData *src = (T_SCheckCardData *)tcuData;
        T_SCheckCardData *dst = (T_SCheckCardData *)remoteData;

        if(src->userType == USER_TYPE_FREE)
        {
            dst->userType = 1;
            memcpy(dst->pwd, src->user, sizeof(dst->pwd));
        }
        else
        {
            dst->userType = 0;
        }
    }
}

//设置时间数据转换
static void SetTimeChange(void *tcuData, void *remoteData, unsigned int direction)
{
    if (direction == REMOTE2TCU)
    {
        T_STimeSet *dst = (T_STimeSet *)tcuData;
        R_STimeSet *src = (R_STimeSet *)remoteData;

        Time6ToTime7((Time7Bits *)dst->time, (Time6Bits *)src->time);
    }
    else if (direction == TCU2REMOTE)
    {
    }
}

//设置二维码数据转换
static void SetQrcode(void *tcuData, void *remoteData, unsigned int direction)
{
    if (direction == REMOTE2TCU)
    {
        T_SQrcodeUpdate *dst = (T_SQrcodeUpdate *)tcuData;

        dst->uiAddr = GunNoTranslate(REMOTE2TCU, dst->uiAddr);
    }
    else if (direction == TCU2REMOTE)
    {
    }
}

//告警上报数据转换
static void FaultChange(void *tcuData, void *remoteData, unsigned int direction)
{
    if (direction == REMOTE2TCU)
    {
    }
    else if (direction == TCU2REMOTE)
    {
        R_SFaultInfo *dst = (R_SFaultInfo *)remoteData;
        T_SFaultInfo *src = (T_SFaultInfo *)tcuData;

        dst->uniqueId = src->index;
        dst->faultId = src->faultId;
        dst->faultSource = src->faultSource;
        dst->faultStat = src->faultStat;
        if (src->faultStat == FAULT_STATE_RESUME)
        {
            Time7ToTime6((Time7Bits *)src->endTime, (Time6Bits *)dst->faultTime);
        }
        else
        {
            Time7ToTime6((Time7Bits *)src->startTime, (Time6Bits *)dst->faultTime);
        }
        dst->gunNo = GunNoTranslate(TCU2REMOTE, src->mainDevId);
        dst->faultLevel = src->faultLevel;
    }
}

//价格信息数据转换
static void PriceChange(void *tcuData, void *remoteData, unsigned int direction)
{
    if (direction == REMOTE2TCU)
    {
        T_SSetChargePrice *dst = (T_SSetChargePrice *)tcuData;
        int i = 0;
        
        for (i = 0; i < dst->priceCount; i++)
        {
            char *p = ( char *)&dst->price[i].startTime;
            
            dst->price[i].startTime = *p * 100 + *(p + 1);
            p = ( char *)&dst->price[i].endTime;
            dst->price[i].endTime = *p * 100 + *(p + 1);
        }
    }
}

//停车费数据转换
static void StopPriceChange(void *tcuData, void *remoteData, unsigned int direction)
{
    if (direction == REMOTE2TCU)
    {
    }
}

//遥脉数据转换
static void YaoMaiChange(void *tcuData, void *remoteData, unsigned int direction)
{
    if (direction == TCU2REMOTE)
    {
        T_SYaoMai *src = (T_SYaoMai *)tcuData;

        src->uiAddr = GunNoTranslate(direction, src->uiAddr);
    }
}

//BMS实时数据转换
static void BmsRealChange(void *tcuData, void *remoteData, unsigned int direction)
{
    if (direction == TCU2REMOTE)
    {
        T_SBmsData *src = (T_SBmsData *)tcuData;

        src->uiAddr = GunNoTranslate(TCU2REMOTE, src->uiAddr);                         //枪编号：01，表示1号枪
    }
}

//BMS额定数据转换
static void BmsRateChange(void *tcuData, void *remoteData, unsigned int direction)
{
    if (direction == TCU2REMOTE)
    {
        T_SBmsData *src = (T_SBmsData *)tcuData;

        src->uiAddr = GunNoTranslate(TCU2REMOTE, src->uiAddr);                           //枪编号：01，表示1号枪
    }
}

enum
{
    SYS_STATUS_FAULT,    //故障
    SYS_STATUS_WARNING,  //告警
    SYS_STATUS_EMPTY,    //空闲
    SYS_STATUS_USING,    //使用中
    SYS_STATUS_STOP,     //停止服务
    SYS_STATUS_OUTLINE,  //离线
    SYS_STATUS_TYPE_MAX, //心跳状态数量
};

//枪状态
static int GunStateTransform(int billState)
{
    int gunStat = 0;

    switch (billState)
    {
        case GUN_STATE_NO_CONNECT: //未连接
        {
            gunStat = HEARTBEAT_GUN_STATUS_EMPTY;
            break;
        }
        case GUN_STATE_CONNECTED: //已连接
        {
            gunStat = HEARTBEAT_GUN_STATUS_WAITTING;
            break;
        }
        case GUN_STATE_CHARGING: //充电中
        {
            gunStat = HEARTBEAT_GUN_STATUS_CHARGING;
            break;
        }
        case GUN_STATE_PAUSE: //暂停中
        {
            gunStat = HEARTBEAT_GUN_STATUS_CHARGING;
            break;
        }
        case GUN_STATE_BOOKED: //预约中
        {
            gunStat = HEARTBEAT_GUN_STATUS_BOOK;
            break;
        }
        case GUN_STATEE_WAITING: //定时中
        {
            gunStat = HEARTBEAT_GUN_STATUS_CHARGING;
            break;
        }
        case GUN_STATE_STOP: //充电完成
        {
            gunStat = HEARTBEAT_GUN_STATUS_CHARGING_COMPLETE;
            break;
        }
        case GUN_STATE_FAULT: //故障
        {
            gunStat = HEARTBEAT_GUN_STATUS_FAULST;
            break;
        }
        case GUN_STATE_DISABLE: //禁用
        {
            gunStat = HEARTBEAT_GUN_STATUS_DISABLE;
            break;
        }
        case GUN_STATE_CHARGE_START: //启动中
        {
            gunStat = HEARTBEAT_GUN_STATUS_START_CHARGING;
            break;
        }
        case GUN_STATE_CHARGE_STOP: //停止中
        {
            gunStat = HEARTBEAT_GUN_STATUS_STOP_CHARGING;
            break;
        }
        case GUN_STATE_ACCOUNTING: //计费中
        case GUN_STATE_ACCOUNT_DONE: //计费完成
        case GUN_STATE_CHARGE_COMPLETE: //充电结束
        {
            gunStat = HEARTBEAT_GUN_STATUS_CHARGING_COMPLETE;
            break;
        }
        default:
            break;
    }

    return gunStat;
}

//心跳信息数据转换
static void HeartBeatChange(void *tcuData, void *remoteData, unsigned int direction)
{
    if (direction == TCU2REMOTE)
    {
        T_STcu2RemoteHeartBeat *dst = (T_STcu2RemoteHeartBeat *)tcuData;
        int sysStat[SYS_STATUS_TYPE_MAX] = {0};

        dst->sysStat = HEARTBEAT_SYS_STATUS_EMPTY;
        int i = 0;
        for (i = 0; i < dst->gunNum; i++)
        {
            int devId = dst->gunInfo[i].gunNo;
            dst->gunInfo[i].gunNo = GunNoTranslate(TCU2REMOTE, dst->gunInfo[i].gunNo);
            dst->gunInfo[i].gunStat = GunStateTransform(dst->gunInfo[i].gunStat);

            if(dst->gunInfo[i].gunStat == HEARTBEAT_GUN_STATUS_START_CHARGING)
            {
                int flag = GetDevValue(BILL_DEV_ID + devId, CHARGE_BILL_ID_VIN_SPECIAL);

                if(flag == 1)
                {
                    dst->gunInfo[i].gunStat = HEARTBEAT_GUN_STATUS_WAITTING;
                }
            }

            if (dst->gunInfo[i].gunStat == HEARTBEAT_GUN_STATUS_CHARGING
                || dst->gunInfo[i].gunStat == HEARTBEAT_GUN_STATUS_CHARGING_COMPLETE
                || dst->gunInfo[i].gunStat == HEARTBEAT_GUN_STATUS_BOOK
                || dst->gunInfo[i].gunStat == HEARTBEAT_GUN_STATUS_WAITTING
                || dst->gunInfo[i].gunStat == HEARTBEAT_GUN_STATUS_START_CHARGING
                || dst->gunInfo[i].gunStat == HEARTBEAT_GUN_STATUS_STOP_CHARGING
            ) //使用中
            {
                sysStat[SYS_STATUS_USING]++;
            }
            else if (dst->gunInfo[i].gunStat == HEARTBEAT_GUN_STATUS_FAULST) //故障
            {
                sysStat[SYS_STATUS_FAULT]++;
            }
            else if (dst->gunInfo[i].gunStat == HEARTBEAT_GUN_STATUS_DISABLE) //停止服务
            {
                sysStat[SYS_STATUS_STOP]++;
            }
        }

        if (sysStat[SYS_STATUS_USING] == dst->gunNum) //使用中
        {
            dst->sysStat = HEARTBEAT_SYS_STATUS_USING;
        }
        else if (sysStat[SYS_STATUS_STOP] == dst->gunNum) //停止服务
        {
            dst->sysStat = HEARTBEAT_SYS_STATUS_STOP;
        }
        else if (sysStat[SYS_STATUS_FAULT] == dst->gunNum) //故障
        {
            dst->sysStat = HEARTBEAT_SYS_STATUS_FAULT;
        }
        else
        {
            //三种状态都有的时候
            if(sysStat[SYS_STATUS_USING] + sysStat[SYS_STATUS_STOP] + sysStat[SYS_STATUS_FAULT] == dst->gunNum)
            {
                //如果有正在使用中那就使用正在使用中
                if(sysStat[SYS_STATUS_USING])
                {
                    dst->sysStat = HEARTBEAT_SYS_STATUS_USING;
                }
                else if(sysStat[SYS_STATUS_STOP]) //如果有正在停止服务那就使用正在停止服务
                {
                    dst->sysStat = HEARTBEAT_SYS_STATUS_STOP;
                }
                else
                {
                    dst->sysStat = HEARTBEAT_SYS_STATUS_FAULT;
                }
            }
        }
    }
}

//遥测数据转换
static void YaoCeChange(void *tcuData, void *remoteData, unsigned int direction)
{
    if (direction == TCU2REMOTE)
    {
        T_SYaoCE *src = (T_SYaoCE *)tcuData;

        src->uiAddr = GunNoTranslate(TCU2REMOTE, src->uiAddr);                    //枪编号：01，表示1号枪
    }
}

//遥信数据转换
static void YaoXinChange(void *tcuData, void *remoteData, unsigned int direction)
{
    if (direction == TCU2REMOTE)
    {
        T_SYaoXin *src = (T_SYaoXin *)tcuData;

        int devId = src->uiAddr;
        src->uiAddr = GunNoTranslate(TCU2REMOTE, src->uiAddr); //枪编号：01，表示1号枪
        src->gunWorkStat = GunStateTransform(src->gunWorkStat);
        if (src->gunWorkStat == HEARTBEAT_GUN_STATUS_START_CHARGING)
        {
            int flag = GetDevValue(BILL_DEV_ID + devId, CHARGE_BILL_ID_VIN_SPECIAL);

            if (flag == 1)
            {
                src->gunWorkStat = HEARTBEAT_GUN_STATUS_WAITTING;
            }
        }
    }
}

float CalcTotalValue(float add1, float add2, int step)
{
    float t = 0.0;
    int tmp, tmp1 = 0;
    if(step == 100)
    {
        tmp = (add1 + 0.005) * 100;
        tmp1 = (add2 + 0.005) * 100;
        t = tmp * 0.01 + tmp1 * 0.01;
    }
    else if(step == 1000)
    {
        tmp = (add1 + 0.0005) * 1000;
        tmp1 = (add2 + 0.0005) * 1000;
        t = tmp * 0.001 + tmp1 * 0.001;
    }

    return t;
}

//账单上报数据转换
static void BillReportChange(void *tcuData, void *remoteData, unsigned int direction)
{
    if (direction == TCU2REMOTE)
    {
        T_SBillData *src = (T_SBillData *)tcuData;

        src->uiAddr = GunNoTranslate(direction, src->uiAddr);

        if (src->userType == USER_TYPE_APP)
        {
            src->userType = UPLOAD_BILL_CHARGE_TYPE_USER_ONLINE;
        }
        else if (src->userType == USER_TYPE_VIN)
        {
            src->userType = UPLOAD_BILL_CHARGE_TYPE_VIN;
        }
        else
        {
            if (src->isOutline == ONLINE)
            {
                src->userType = UPLOAD_BILL_CHARGE_TYPE_CARD_ONLINE;
            }
            else
            {
                src->userType = UPLOAD_BILL_CHARGE_TYPE_CARD_OUTLINE;
            }
        }

        if (src->billState == eBillState_Finish_Fail)
        {
            src->billState = 1;
        }
        else
        {
            src->billState = 0;
        }

        float power,chargeConsume,serviceConsume = 0.0;
        int i = 0;
        //处理分段电费精度问题
        for (i = 0; i < src->chargePriceCount; i++)
        {
            power = CalcTotalValue(power, src->chargeCosume[i].charge, 1000);
            chargeConsume = CalcTotalValue(chargeConsume, src->chargeCosume[i].chargeCosume, 100);
            serviceConsume = CalcTotalValue(serviceConsume, src->chargeCosume[i].serviceConsume, 100);
        }

        if(src->isParallelsOrder == 0
          && src->isParallels == 1)
        {
            power = CalcTotalValue(power, src->slaveCharge, 1000);
            chargeConsume = CalcTotalValue(chargeConsume, src->slaveChrConsume, 100);
            serviceConsume = CalcTotalValue(serviceConsume, src->slaveSrvConsume, 100);
        }

		if(abs(src->totalPower - power) < 0.5
			|| abs(src->totalConsume - chargeConsume) < 0.5
			|| abs(src->totalServiceConsume - serviceConsume) < 0.5
			)
		{
			src->totalPower = power;
			src->totalConsume = chargeConsume;
			src->totalServiceConsume = serviceConsume;
		}
		else
		{
			AppLogOut("电费误差过大");
		}

    }
    else if (direction == REMOTE2TCU)
    {
        T_SUploadBillAck *dst = (T_SUploadBillAck *)tcuData;

        dst->uiAddr = GunNoTranslate(REMOTE2TCU, dst->uiAddr);
    }
}

static void *DataMalloc(int size)
{
    void *p = malloc(size);
    memset(p, 0, size);
    return p;
}

void DataRemote2Tcu(int msgid, void *remoteData, void *tcuData, int tcuSize)
{
    switch (msgid)
    {
        case REMOTE2TCU_REMOTE_START: //充电启动
        {
            StartChargeChange(tcuData, remoteData, REMOTE2TCU);
            break;
        }
        case REMOTE2TCU_REMOTE_STOP: //充电停止
        {
            StopChargeChange(tcuData, remoteData, REMOTE2TCU);

            break;
        }
        case REMOTE2TCU_PWR_ADJUST: //充电最大功率控制
        {
            PowerCltChange(tcuData, remoteData, REMOTE2TCU);
            break;
        }
        case REMOTE2TCU_STOP_SERVICE: //暂停服务
        {
            StopServiceChange(tcuData, remoteData, REMOTE2TCU);
            break;
        }
        case REMOTE2TCU_RESUME_SERVICE: //恢复服务
        {
            StartServiceChange(tcuData, remoteData, REMOTE2TCU);
            break;
        }
        case REMOTE2TCU_ORDER_CHARGE: //预约充电
        {
            OrderGunChange(tcuData, remoteData, REMOTE2TCU);
            break;
        }
        case REMOTE2TCU_CANCEL_ORDER: //取消预约
        {
            CancelOrderGunChange(tcuData, remoteData, REMOTE2TCU);
            break;
        }
        case REMOTE2TCU_BILL_REPORT_ACK: //交易记录确认
        {
            BillReportChange(tcuData, remoteData, REMOTE2TCU);
            break;
        }
        case REMOTE2TCU_PRICE_SET:     //费率和时段设置
        {
            PriceChange(tcuData, remoteData, REMOTE2TCU);
            break;
        }
        case REMOTE2TCU_STOP_PRICE_SET:     //停车费设置
        {
            StopPriceChange(tcuData, remoteData, REMOTE2TCU);
            break;
        }
        case REMOTE2TCU_LAND_LOCK: //下放地锁
        {
            LandLockChange(tcuData, remoteData, REMOTE2TCU);
            break;
        }
        case REMOTE2TCU_TIME_SYNC: //报文周期设置
        {
            SetTimeChange(tcuData, remoteData, REMOTE2TCU);
            break;
        }
        case REMOTE2TCU_QR_SET: //下发二维码
        {
            SetQrcode(tcuData, remoteData, REMOTE2TCU);
            break;
        }
        case REMOTE2TCU_CHECK_CARD_ACK:
        {
            CheckCardChange(tcuData, remoteData, REMOTE2TCU);
            break;
        }
        case REMOTE2TCU_VIN_CHARGE_ACK:
        {
            VinChargeChange(tcuData, remoteData, REMOTE2TCU);
            break;
        }
        case REMOTE2TCU_CARD_CHARGE_ACK:
        {
            CardChargeChange(tcuData, remoteData, REMOTE2TCU);
            break;
        }
        default:
        {
            memcpy(tcuData, remoteData, tcuSize);
            break;
        }
    }
}

void DataTcu2Remote(int msgid, void *tcuData, void *remoteData, int remoteSize)
{
    switch (msgid)
    {
        case TCU2REMOTE_REMOTE_START_ACK:
        {
            StartChargeChange(tcuData, remoteData, TCU2REMOTE);
            break;
        }
        case TCU2REMOTE_REMOTE_STOP_ACK:
        {
            StopChargeChange(tcuData, remoteData, TCU2REMOTE);
            break;
        }
        case TCU2REMOTE_YAO_MAI:
        {
            YaoMaiChange(tcuData, remoteData, TCU2REMOTE);
            break;
        }
        case TCU2REMOTE_BMS_REAL_DATA:
        {
            BmsRealChange(tcuData, remoteData, TCU2REMOTE);
            break;
        }
        //case TCU2REMOTE_BMS_RATE_DATA:
        //{
        //    BmsRateChange(tcuData, remoteData, TCU2REMOTE);
        //    break;
        //}
        case TCU2REMOTE_HEART_BEAT:
        {
            HeartBeatChange(tcuData, remoteData, TCU2REMOTE);
            break;
        }
        case TCU2REMOTE_YAO_CE:
        {
            YaoCeChange(tcuData, remoteData, TCU2REMOTE);
            break;
        }
        case TCU2REMOTE_YAO_XIN:
        {
            YaoXinChange(tcuData, remoteData, TCU2REMOTE);
            break;
        }
        case TCU2REMOTE_PWR_ADJUST_ACK:
        {
            PowerCltChange(tcuData, remoteData, TCU2REMOTE);
            break;
        }
        case TCU2REMOTE_RESUME_SERVICE_ACK:
        {
            StartServiceChange(tcuData, remoteData, TCU2REMOTE);
            break;
        }
        case TCU2REMOTE_STOP_SERVICE_ACK:
        {
            StopServiceChange(tcuData, remoteData, TCU2REMOTE);
            break;
        }
        case TCU2REMOTE_ORDER_TIMEOUT:
        {
            TimeoutCancelGun(tcuData, remoteData, TCU2REMOTE);
            break;
        }
        case TCU2REMOTE_CANCEL_ORDER_ACK:
        {
            CancelOrderGunChange(tcuData, remoteData, TCU2REMOTE);
            break;
        }
        case TCU2REMOTE_ORDER_CHARGE_ACK:
        {
            OrderGunChange(tcuData, remoteData, TCU2REMOTE);
            break;
        }
        case TCU2REMOTE_CARD_CHARGE:
        {
            CardChargeChange(tcuData, remoteData, TCU2REMOTE);
            break;
        }
        case TCU2REMOTE_VIN_CHARGE:
        {
            VinChargeChange(tcuData, remoteData, TCU2REMOTE);
            break;
        }
        case TCU2REMOTE_LAND_LOCK_ACK:
        {
            LandLockChange(tcuData, remoteData, TCU2REMOTE);
            break;
        }
        case TCU2REMOTE_FAULT_MSG:
        {
            FaultChange(tcuData, remoteData, TCU2REMOTE);
            break;
        }
        case TCU2REMOTE_BILL_REPORT:
        {
            BillReportChange(tcuData, remoteData, TCU2REMOTE);
            break;
        }
        case TCU2REMOTE_CHECK_CARD:
        {
            CheckCardChange(tcuData, remoteData, TCU2REMOTE);
            break;
        }
        default:
        {
            memcpy(remoteData, tcuData, remoteSize);
            break;
        }
    }
}

void cJSON_PutDouble(cJSON*obj,char *key, double value)
{
    if(obj && key)
    {
        cJSON_AddItemToObject(obj,key,cJSON_CreateNumber(value));
    }
}

double cJSON_GetDouble(cJSON*obj,char *key, double *value)
{
    if(obj && key)
    {
        cJSON*item=cJSON_GetObjectItem(obj,key);
        if(item && item->type==cJSON_Number)
        {
            if(value)
                *value=item->valuedouble;
            return item->valuedouble;
        }
    }
    return 0;
}
