#include "PlatProt_2_0.h"
#include "Queue.h"
#include "globalvar.h"
#include "Data.h"
#include <stdlib.h>
#include <string.h>
#include "Deal_Handler.h"
#include "RemoteComm.h"
#include "TcuComm.h"

/*------------------------------------------------------------------------------
Section: 外部函数调用声明
------------------------------------------------------------------------------*/
extern STATUS System_param2_save(void);                 // param.c
extern STATUS System_param_save(void);                  // param.c

extern int Make_Local_SN(char *DevSn,                   // globalvar.c
                         unsigned char gunID,
                         unsigned char parallelFlag, 
                         char *localSn, 
                         unsigned short maxLen); 

/*------------------------------------------------------------------------------
Section: 内部变量
------------------------------------------------------------------------------*/
static ISO8583_charge_db_data  s_busy_db_data;          //数据库中的交易记录
static int                     s_Busy_Need_Up_id = -1;  //最后1次上报的记录的id

/**
******************************************************************************
* @brief       心跳接收
* @param[in]   unsigned char* pData     接收到的数据
* @param[in]   unsigned int len         数据长度
* @param[out]  NONE
* @retval      NONE
* @details
* @note        状态5时才为接收
******************************************************************************
*/
static void PlatProt_RecvHeart(unsigned char* pData,  unsigned int len)
{
    
}

/**
******************************************************************************
* @brief       获取黑名单
* @param[in]   unsigned char* pData     接收到的数据
* @param[in]   unsigned int len         数据长度
* @param[out]  NONE
* @retval      NONE
* @details     
* @note        
******************************************************************************
*/
static void PlatProt_RecvBlackListAck(unsigned char* pData,  unsigned int len)
{

    // 由远端程序统一管理，本地不用管

}

/**
******************************************************************************
* @brief       对时
* @param[in]   unsigned char* pData     接收到的数据
* @param[in]   unsigned int len         数据长度
* @param[out]  NONE
* @retval      NONE
* @details     
* @note        Globa->time_update_flag 主函数中去使用这个标记去写系统时间
******************************************************************************
*/
static void PlatProt_RecvTimeSyn(unsigned char* pData,  unsigned int len)
{
    T_STimeSetAck data;
    T_STimeSet *pTime = (T_STimeSet *)pData;

    Globa->year = pTime->time[0] + 2000;
    Globa->month = pTime->time[1];
    Globa->date = pTime->time[2];
    Globa->hour = pTime->time[3];
    Globa->minute = pTime->time[4];
    Globa->second = pTime->time[5];

    // 时间更新标志
    Globa->time_update_flag = 1; 

    /* 处理应答数据 */
    data.ackResult = 00;  // 立即生效
    CommPostEvent(TCU2REMOTE_TIME_SYNC_ACK, &data, sizeof(data));
}

/**
******************************************************************************
* @brief       费率和时段设置
* @param[in]   unsigned char* pData     接收到的数据
* @param[in]   unsigned int len         数据长度
* @param[out]  NONE
* @retval      NONE
* @details     
* @note        价格从没有获取到之前，采取默认价格，否则在获取到最新价格之前，采取上次获取的价格
               todo：因为二代协议费率时段数与一代都不相等，所以注意后面用这个参数的相关变量大小的
               调整
******************************************************************************
*/
static void PlatProt_RecvPriceSet(unsigned char* pData,  unsigned int len)
{
    T_SSetChargePriceAck data;
    T_SSetChargePrice *pChargePrice = (T_SSetChargePrice *)pData;
    unsigned char index = 0;
    unsigned char copyLen = 0;

    // 防止在拷贝的时候越界
    copyLen = sizeof(Globa->Charger_param2.price_type_ver);
    copyLen = (sizeof(pChargePrice->version) >= copyLen) ? copyLen : sizeof(pChargePrice->version);

    // 版本更新则更新电价
    if (0 != memcmp(Globa->Charger_param2.price_type_ver, &pChargePrice->version, copyLen))
    {
        memcpy(Globa->Charger_param2.price_type_ver, &pChargePrice->version, copyLen);
        Globa->Charger_param2.show_price_type = pChargePrice->isShowUnitPrice;
        Globa->Charger_param2.time_zone_num = pChargePrice->priceCount;

        for (index = 0; index < pChargePrice->priceCount; index++)
        {
            /*  notice：之所以只看开始时间不看结束时间，是因为已向平台确认下发的时段是连续的，
            下一个时段的开始时间就是本时段的结束时间,参照国网的计费模型设计方式  */

            Globa->Charger_param2.time_zone_tabel[index] = pChargePrice->price[index].startTime;
            Globa->Charger_param2.time_zone_feilv[index] = index;

            Globa->Charger_param2.share_time_kwh_price_serve[index] =
                pChargePrice->price[index].servicePrice * 100000;

            Globa->Charger_param2.share_time_kwh_price[index] =
                pChargePrice->price[index].chargePrice * 100000;
        }

        // 保存价格
        System_param2_save();
    }

    /* 处理应答数据 */
    data.ackResult = 00;  // 立即生效
    CommPostEvent(TCU2REMOTE_PRICE_SET_ACK, &data, sizeof(data));
}

/**
******************************************************************************
* @brief       功率调节控制
* @param[in]   unsigned char* pData     接收到的数据
* @param[in]   unsigned int len         数据长度
* @param[out]  NONE
* @retval      NONE
* @details     
* @note        
               看程序了解到功率限制TCU也存了份参数，但是没用起来，考虑到限功率的事情
               由充电控制器做，TCU实时转发就好了，没有存的必要，就不存了。
******************************************************************************
*/
static void PlatProt_RecvPowerAdjust(unsigned char* pData,  unsigned int len)
{
    T_SPowerCtrlAck data;
    T_SPowerCtrl *pPowerCtrl = (T_SPowerCtrl *)pData;
    unsigned char index = 0;

    if (pPowerCtrl->uiAddr > Globa->Charger_param.gun_config)
    {
        return;
    }

    // 设置所有充电桩
    if (0 == pPowerCtrl->uiAddr)
    {
        for (index = 0; index < Globa->Charger_param.gun_config; index++)
        {
            Globa->Charger_param.limit_type[index] = pPowerCtrl->type;

            if (1 == pPowerCtrl->type)
            {
                // 绝对值调节，扩大10倍
                Globa->Charger_param.limit_VAL[index]  = pPowerCtrl->value * 10;
            }
            else if (2 == pPowerCtrl->type)
            {
                Globa->Charger_param.limit_VAL[index]  = pPowerCtrl->value;
            } 
        }
    }
    else // 设置具体充电桩
    {    // -1是因为数据缓存是从0开始，而地址是从1开始。
        Globa->Charger_param.limit_type[pPowerCtrl->uiAddr - 1] = pPowerCtrl->type;

        if (1 == pPowerCtrl->type)
        {
            Globa->Charger_param.limit_VAL[pPowerCtrl->uiAddr - 1]  = pPowerCtrl->value * 10;
        }
        else if (2 == pPowerCtrl->type)
        {
            Globa->Charger_param.limit_VAL[pPowerCtrl->uiAddr - 1]  = pPowerCtrl->value;
        }   
    }

    System_param_save();

    /* 处理应答数据 */
    data.ackResult = 00;
    data.uiAddr = pPowerCtrl->uiAddr;
    CommPostEvent(TCU2REMOTE_PWR_ADJUST_ACK, &data, sizeof(data));

}

/**
******************************************************************************
* @brief       停车费设置
* @param[in]   unsigned char* pData     接收到的数据
* @param[in]   unsigned int len         数据长度
* @param[out]  NONE
* @retval      NONE
* @details     
* @note        
******************************************************************************
*/
static void PlatProt_RecvStopPriceSet(unsigned char* pData,  unsigned int len)
{
    T_SSetStopPriceAck data;
    T_SSetStopPrice *pStopPrice = (T_SSetStopPrice *)pData;

    // 版本更新则更新停车费
    if (Globa->Charger_param2.charge_stop_price.stop_price_ver != pStopPrice->version)
    {
        Globa->Charger_param2.charge_stop_price.stop_price_ver = pStopPrice->version;
        Globa->Charger_param2.charge_stop_price.stop_price = (unsigned int)(pStopPrice->price * 100000);
        Globa->Charger_param2.charge_stop_price.is_free_when_charge = pStopPrice->isFreeStop;
        Globa->Charger_param2.charge_stop_price.free_time = pStopPrice->freeStopTime;

        //保存停车费
        System_param2_save();
    }

    /* 处理应答数据 */
    data.ackResult = 00;
    CommPostEvent(TCU2REMOTE_STOP_PRICE_SET_ACK, &data, sizeof(data));
}

/**
******************************************************************************
* @brief       报文周期设置
* @param[in]   unsigned char* pData     接收到的数据
* @param[in]   unsigned int len         数据长度
* @param[out]  NONE
* @retval      NONE
* @details     
* @note        看了程序里在上电初始化里，对这个时间有限制，不能太快也不能太慢，这个区间是否
               合理后面要多注意一下，另外这个时间是指除心跳外其它周期报文的发送周期限制，不要被
               名字迷惑。  todo: 需要和司恩波合一下这个变量，他单独定义了
******************************************************************************
*/
static void PlatProt_RecvTimeSpan(unsigned char* pData,  unsigned int len)
{
    T_SSetMsgCycleAck data;
    T_SSetMsgCycle *pMsgCycle = (T_SSetMsgCycle *)pData;

    // 变化保存
    if (Globa->Charger_param.heartbeat_idle_period != pMsgCycle->standyMsgTime ||
        Globa->Charger_param.heartbeat_busy_period != pMsgCycle->chargeMsgTime)
    {
        Globa->Charger_param.heartbeat_idle_period = pMsgCycle->standyMsgTime;
        Globa->Charger_param.heartbeat_busy_period = pMsgCycle->chargeMsgTime;

        //保存报文周期
        System_param_save();
    }

    /* 处理应答数据 */
    data.ackResult = 0x00;
    CommPostEvent(TCU2REMOTE_MSG_PERIOD_ACK, &data, sizeof(data));
}

/**
******************************************************************************
* @brief       重启控制
* @param[in]   unsigned char* pData     接收到的数据
* @param[in]   unsigned int len         数据长度
* @param[out]  NONE
* @retval      NONE
* @details     
* @note        其实重启这个命令可以延时重启和立刻重启，也可以指定特定设备重启。
               目前接收到的任何重启方式，都采取整个设备重启。
******************************************************************************
*/
static void PlatProt_RecvReboot(unsigned char* pData,  unsigned int len)
{

    T_SResetAck data;
     T_SReset *pReset = (T_SReset *)pData;

     if (0x01 == pReset->cmd)
     {
        if((Globa->QT_Step == 0x02)||(Globa->QT_Step == 0x43))//允许重启
        {				
            Globa->sys_restart_flag = 1;
            Globa->system_upgrade_busy = 1;//避免桩被使用

            data.ackResult = 00; // 立即重启
        }
        else
        {				
            Globa->sys_restart_flag = 0;

            data.ackResult = 28; // 无法重启
        }
     }

     CommPostEvent(TCU2REMOTE_RMOETE_REBOOT_ACK, &data, sizeof(data));
}

/**
******************************************************************************
* @brief       停止服务控制
* @param[in]   unsigned char* pData     接收到的数据
* @param[in]   unsigned int len         数据长度
* @param[out]  NONE
* @retval      NONE
* @details     
* @note        
******************************************************************************
*/
static void PlatProt_RecvStopService(unsigned char* pData,  unsigned int len)
{
    T_SChargeServiceAck data;
    T_SChargeService *pChargeService = (T_SChargeService *)pData;
    unsigned int index = 0;

    if (pChargeService->uiAddr > Globa->Charger_param.gun_config)
    {
        return;
    }

    // 整桩禁用
    if (0 == pChargeService->uiAddr)
    {
        for (index = 0; index < Globa->Charger_param.gun_config; index++)
        {
            Globa->Electric_pile_workstart_Enable[index] = 0;
        }
    }
    else
    {
        Globa->Electric_pile_workstart_Enable[pChargeService->uiAddr - 1] = 0;
    }

    data.ackResult = 0x00;
    data.uiAddr = pChargeService->uiAddr;
    CommPostEvent(TCU2REMOTE_STOP_SERVICE_ACK, &data, sizeof(data));

}

/**
******************************************************************************
* @brief       恢复服务控制
* @param[in]   unsigned char* pData     接收到的数据
* @param[in]   unsigned int len         数据长度
* @param[out]  NONE
* @retval      NONE
* @details     
* @note        
******************************************************************************
*/
static void PlatProt_RecvStartService(unsigned char* pData,  unsigned int len)
{
    T_SChargeServiceAck data;
    T_SChargeService *pChargeService = (T_SChargeService *)pData;
    unsigned int index = 0;

    if (pChargeService->uiAddr > Globa->Charger_param.gun_config)
    {
        return;
    }

    // 整桩启用
    if (0 == pChargeService->uiAddr)
    {
        for (index = 0; index < Globa->Charger_param.gun_config; index++)
        {
            Globa->Electric_pile_workstart_Enable[index] = 1;
        }
    }
    else
    {
        Globa->Electric_pile_workstart_Enable[pChargeService->uiAddr - 1] = 1;
    }

    data.ackResult = 0x00;
    data.uiAddr = pChargeService->uiAddr;
    CommPostEvent(TCU2REMOTE_RESUME_SERVICE_ACK, &data, sizeof(data));
}

/**
******************************************************************************
* @brief       二维码设置
* @param[in]   unsigned char* pData     接收到的数据
* @param[in]   unsigned int len         数据长度
* @param[out]  NONE
* @retval      NONE
* @details     
* @note        二维码组成数据已存储，其它需要二维码，自己去拼接
******************************************************************************
*/
static void PlatProt_RecvQrCode(unsigned char* pData,  unsigned int len)
{
    T_SQrcodeUpdateAck data;
    T_SQrcodeUpdate *pQrcodeUpdate = (T_SQrcodeUpdate *)pData;
    unsigned short index = 0;

    if (pQrcodeUpdate->gunNo > Globa->Charger_param.gun_config)
    {
        return;
    }

    if (0 == pQrcodeUpdate->gunNo)
    {
        for (index = 0; index < Globa->Charger_param.gun_config; index++)
        {
            snprintf(Globa->Charger_param.gunQrCode[index], sizeof(Globa->Charger_param.gunQrCode[index]), 
            "%s", pQrcodeUpdate->qrCode);

            Globa->Charger_param.IsWhloeFlag[index] = pQrcodeUpdate->isWhole;
        }
    }
    else
    {
        snprintf(Globa->Charger_param.gunQrCode[pQrcodeUpdate->gunNo - 1], 
        sizeof(Globa->Charger_param.gunQrCode[pQrcodeUpdate->gunNo - 1]),  "%s", pQrcodeUpdate->qrCode);

        Globa->Charger_param.IsWhloeFlag[pQrcodeUpdate->gunNo - 1] = pQrcodeUpdate->isWhole;
    }

    //保存二维码
    System_param_save();

    data.ackResult = 0x00;
    CommPostEvent(TCU2REMOTE_QR_SET_ACK, &data, sizeof(data));
}

/**
******************************************************************************
* @brief       接收到启动充电
* @param[in]   unsigned char* pData     接收到的数据
* @param[in]   unsigned int len         数据长度
* @param[out]  NONE
* @retval      NONE
* @details     
* @note        
               
******************************************************************************
*/
static void PlatProt_RecvChgStart(unsigned char* pData,  unsigned int len)
{
    T_SChargeDataAck data = { 0 };
    T_SChargeData *pChargeData = (T_SChargeData *)pData;
    unsigned char gun_id = 0;

    Globa->Control_DC_Infor_Data[gun_id].AP_APP_Sent_flag[0] = 0;

    // 判读枪号是否有效
    if (pChargeData->uiAddr > Globa->Charger_param.gun_config || 0 == pChargeData->uiAddr)
    {
        return;
    }

    gun_id = pChargeData->uiAddr - 1;

    // 枪未插好
    if (1 != Globa->Control_DC_Infor_Data[gun_id].gun_link)
    {
        data.ackResult = 9;
    }
    // 设备被占用或重复接收到启动命令
    else if (1 == Globa->Control_DC_Infor_Data[gun_id].App.APP_start ||
             1 == Globa->Control_DC_Infor_Data[gun_id].App.APP_book_start)
    {
        if (0 != memcmp(Globa->Control_DC_Infor_Data[gun_id].App.ID,
            &pChargeData->user, sizeof(Globa->Control_DC_Infor_Data[gun_id].App.ID)))
        {
            data.ackResult = 10;
        }
        else // 重复接收到启动命令，应答成功
        {
            data.ackResult = 0;
        }
    }
    // 余额不足
    else if (((int)(pChargeData->balance * 100)) < MIN_USER_MONEY_NEED)
    {
        data.ackResult = 11;
    }
    // 设备故障
    else if ((Globa->Control_DC_Infor_Data[gun_id].Error_system != 0) ||
            (Globa->Electric_pile_workstart_Enable[gun_id] == 0))
    {
        data.ackResult = 12;
    }
    // 设备忙
    else if (!((Globa->QT_Step == 0x03) || (Globa->QT_Step == 0x02) ||
             (Globa->QT_Step == 0x202) || (Globa->QT_Step == 0x21)))
    {
        data.ackResult = 14;
    }
    // 设备升级中按设备忙处理
    else if (Globa->system_upgrade_busy)
    {
        data.ackResult = 14;
    }
    else /* 允许充电,处理好相关数据及标记 */
    {
        // 用户标识
        memcpy(Globa->Control_DC_Infor_Data[gun_id].App.ID, pChargeData->user,
            sizeof(Globa->Control_DC_Infor_Data[gun_id].App.ID));
        // 用户类型
        Globa->Control_DC_Infor_Data[gun_id].App.userType = pChargeData->userType;
        // 用户余额
        Globa->Control_DC_Infor_Data[gun_id].App.last_rmb = ((int)(pChargeData->balance * 100));

        /* 充电模式 */
        if (0x01 == pChargeData->chargeType)      // 按电量
        {
            Globa->Control_DC_Infor_Data[gun_id].App.APP_charge_mode = KWH_MODE;
            Globa->Control_DC_Infor_Data[gun_id].App.APP_charge_kwh_limit =
                pChargeData->chargeValue * 100;
        }
        else if (0x02 == pChargeData->chargeType) // 按时间
        {
            Globa->Control_DC_Infor_Data[gun_id].App.APP_charge_mode = TIME_MODE;
            Globa->Control_DC_Infor_Data[gun_id].App.APP_charge_sec_limit =
                pChargeData->chargeValue * 60;
        }
        else if (0x03 == pChargeData->chargeType) // 按金额
        {
            Globa->Control_DC_Infor_Data[gun_id].App.APP_charge_mode = MONEY_MODE;
            Globa->Control_DC_Infor_Data[gun_id].App.APP_charge_money_limit =
                pChargeData->chargeValue * 100;
        }
        else if (0x04 == pChargeData->chargeType) // 自动充满
        {
            Globa->Control_DC_Infor_Data[gun_id].App.APP_charge_mode = AUTO_MODE;
        }
        else if (0x05 == pChargeData->chargeType) // 延时充电
        {
            Globa->Control_DC_Infor_Data[gun_id].App.APP_charge_mode = DELAY_CHG_MODE;
            Globa->Control_DC_Infor_Data[gun_id].App.APP_pre_time = pChargeData->chargeValue;
        }

        /* 辅助电源 */
        if (0x01 == pChargeData->BmsVoltage) // 12V
        {
            Globa->Control_DC_Infor_Data[gun_id].BMS_Power_V = 0;
        }
        else // 24V
        {
            Globa->Control_DC_Infor_Data[gun_id].BMS_Power_V = 1;
        }

        /* 用户类型 默认为本系统用户*/
        Globa->Control_DC_Infor_Data[gun_id].App.APP_Start_Account_type = 0; 

        /* 交易流水号 */
        mempy(Globa->Control_DC_Infor_Data[gun_id].App.busy_SN, pChargeData->remoteOrder,
            sizeof(Globa->Control_DC_Infor_Data[gun_id].App.busy_SN));

        /* 本地流水号生成失败，则认为设备故障 */
        if (-1 == Make_Local_SN(Globa->Charger_param.SN, pChargeData->uiAddr,
            0, Globa->Control_DC_Infor_Data[gun_id].local_SigGunBusySn,
            sizeof(Globa->Control_DC_Infor_Data[gun_id].local_SigGunBusySn)))
        {
            data.ackResult = 12;
            Globa->Control_DC_Infor_Data[gun_id].AP_APP_Sent_flag[0] = 0;
        }
        else // 所有的前期工作都准备完成
        {
            data.ackResult = 0x00;
            Globa->Control_DC_Infor_Data[gun_id].App.APP_book_start = 0; // 清掉预约标记
            Globa->Control_DC_Infor_Data[gun_id].App.APP_start = 1;//所有参数设置完后置标志位
            Globa->Control_DC_Infor_Data[gun_id].AP_APP_Sent_flag[0] = 1; // 表示数据库写完之后，才应答
        }
    }

    // 立即响应
    if (0 == Globa->Control_DC_Infor_Data[gun_id].AP_APP_Sent_flag[0])
    {
        // 单元地址
        data.uiAddr = pChargeData->uiAddr;
        // 交易流水号
        memcpy(data.remoteOrder, Globa->Control_DC_Infor_Data[gun_id].App.busy_SN,
            sizeof(Globa->Control_DC_Infor_Data[gun_id].App.busy_SN));
        // 本地订单号
        memcpy(data.localOrder, Globa->Control_DC_Infor_Data[gun_id].local_SigGunBusySn,
            sizeof(Globa->Control_DC_Infor_Data[gun_id].local_SigGunBusySn));
        // 并机类型 APP启动不并机
        data.parallelsType = 0; 
        // 用户ID
        memcpy(data.user, Globa->Control_DC_Infor_Data[gun_id].App.ID,
            sizeof(Globa->Control_DC_Infor_Data[gun_id].App.ID));
        // 账户类型
        data.userType = Globa->Control_DC_Infor_Data[gun_id].App.userType;
        // 充电方式
        data.chargeType = pChargeData->chargeType;
        data.chargeValue = pChargeData->chargeValue;

        CommPostEvent(TCU2REMOTE_REMOTE_START_ACK, &data, sizeof(data));
    }

    return;
}

/**
******************************************************************************
* @brief       接收到停止充电
* @param[in]   unsigned char* pData     接收到的数据
* @param[in]   unsigned int len         数据长度
* @param[out]  NONE
* @retval      NONE
* @details
* @note        
******************************************************************************
*/
static void PlatProt_RecvChgStop(unsigned char* pData, unsigned int len)
{
    T_SStopChargeDataAck data = { 0 };
    T_SStopChargeData *pChargeStop = (T_SStopChargeData *)pData;
    unsigned char gun_id = 0;

    // 判读枪号是否有效
    if (pChargeStop->uiAddr > Globa->Charger_param.gun_config || 0 == pChargeStop->uiAddr)
    {
        return;
    }

    gun_id = pChargeStop->uiAddr - 1;

    // 判断云平台订单号
    if (0 != memcmp(Globa->Control_DC_Infor_Data[gun_id].App.busy_SN, pChargeStop->remoteOrder,
        sizeof(Globa->Control_DC_Infor_Data[gun_id].App.busy_SN)))
    {
        data.ackResult = 15; 
    }
    // 判断本地交易流水号
    else if (0 != memcmp(Globa->Control_DC_Infor_Data[gun_id].local_SigGunBusySn, pChargeStop->localOrder,
        sizeof(Globa->Control_DC_Infor_Data[gun_id].local_SigGunBusySn)))
    {
        data.ackResult = 15;
    }
    // 判断用户标识
    else if (0 != memcmp(Globa->Control_DC_Infor_Data[gun_id].App.ID, pChargeStop->user,
        sizeof(Globa->Control_DC_Infor_Data[gun_id].App.ID)))
    {
        data.ackResult = 15;
    }
    else
    {
        data.ackResult = 0x00;
        Globa->Control_DC_Infor_Data[gun_id].App.APP_start = 0;
    }

    /* 应答数据处理 */
    data.uiAddr = pChargeStop->uiAddr;
    // 账户号
    memcpy(data.user, pChargeStop->user, sizeof(data.user));
    // 云平台订单号
    memcpy(data.remoteOrder, pChargeStop->remoteOrder, sizeof(data.remoteOrder));
    // 桩交易流水号
    memcpy(data.localOrder, pChargeStop->localOrder, sizeof(data.localOrder));
    
    CommPostEvent(TCU2REMOTE_REMOTE_STOP_ACK, &data, sizeof(data));
    return;
}

/**
******************************************************************************
* @brief       接收到VIN鉴权应答
* @param[in]   unsigned char* pData     接收到的数据
* @param[in]   unsigned int len         数据长度
* @param[out]  NONE
* @retval      NONE
* @details
* @note
******************************************************************************
*/
static void PlatProt_RecvVinAuthAck(unsigned char* pData, unsigned int len)
{
    T_SVinChargeAck *pVinAuthAck = (T_SVinChargeAck *)pData;
    unsigned char gun_id = 0;
    unsigned char busySN[20] = { 0 };

    // 判读枪号是否有效
    if (pVinAuthAck->uiAddr > Globa->Charger_param.gun_config || 0 == pVinAuthAck->uiAddr)
    {
        return;
    }

    gun_id = pVinAuthAck->uiAddr - 1;
    
    // 鉴权结果
    if (00 == pVinAuthAck->ackResult)
    {
        Globa->Control_DC_Infor_Data[gun_id].VIN_auth_success = 1;  // 成功
    }
    else
    {
        Globa->Control_DC_Infor_Data[gun_id].VIN_auth_success = 0;  // 失败
    }

    // VIN鉴权失败原因
    Globa->Control_DC_Infor_Data[gun_id].VIN_auth_failed_reason = pVinAuthAck->ackResult;

    // VIN余额
    Globa->Control_DC_Infor_Data[gun_id].VIN_auth_money = pVinAuthAck->balance * 100;

    // VIN关联的卡号
    memcpy(Globa->Control_DC_Infor_Data[gun_id].VIN_User_Id, pVinAuthAck->user,
        sizeof(Globa->Control_DC_Infor_Data[gun_id].VIN_User_Id));

    // 云平台订单号
    memcpy(busySN, pVinAuthAck->remoteOrder, sizeof(busySN));

    //写入数据库
    if (1 != Globa->Control_DC_Infor_Data[gun_id].VIN_auth_ack)
    {
        pthread_mutex_lock(&busy_db_pmutex);
        //添加数据库该条充电记录的流水号
        Set_Temp_Busy_SN(gun_id + 1, busySN);
        pthread_mutex_unlock(&busy_db_pmutex);
    }

    // VIN鉴权已应答
    Globa->Control_DC_Infor_Data[gun_id].VIN_auth_ack = 1;
    return;
}

/**
******************************************************************************
* @brief       接收启动事件上报应答
* @param[in]   unsigned char* pData     接收到的数据
* @param[in]   unsigned int len         数据长度
* @param[out]  NONE
* @retval      NONE
* @details
* @note
******************************************************************************
*/ 
static void PlatProt_RecvStartEventReportAck(unsigned char* pData, unsigned int len)
{
    T_SCardChargeAck *pCardAuthAck = (T_SCardChargeAck *)pData;
    unsigned char gun_id = 0;
    unsigned char busySN[20] = { 0 };

    // 判读枪号是否有效
    if (pCardAuthAck->uiAddr > Globa->Charger_param.gun_config || 0 == pCardAuthAck->uiAddr)
    {
        return;
    }

    gun_id = pCardAuthAck->uiAddr - 1;

    if (0 == pCardAuthAck->ackResult)
    {
        Globa->Control_DC_Infor_Data[gun_id].User_Card_check_result = 1;
    }
    else
    {
        Globa->Control_DC_Infor_Data[gun_id].User_Card_check_result = pCardAuthAck->ackResult;
    }

    // 云平台订单号
    memcpy(busySN, pCardAuthAck->remoteOrder, sizeof(busySN));

    if (0 != Globa->Control_DC_Infor_Data[gun_id].User_Card_check_flag)
    {
        pthread_mutex_lock(&busy_db_pmutex);
        //添加数据库该条充电记录的流水号
        Set_Temp_Busy_SN(gun_id, busySN, 0);
        pthread_mutex_unlock(&busy_db_pmutex);
    }

    Globa->Control_DC_Infor_Data[gun_id].User_Card_check_flag = 0;
    return;
}

/**
******************************************************************************
* @brief       接收预约充电枪
* @param[in]   unsigned char* pData     接收到的数据
* @param[in]   unsigned int len         数据长度
* @param[out]  NONE
* @retval      NONE
* @details
* @note
******************************************************************************
*/
static void PlatProt_RecvOrderGun(unsigned char* pData, unsigned int len)
{
    T_SOrderChargeDataAck data = {0};
    T_SOrderChargeData *pOrderGun = (T_SOrderChargeData *)pData;
    unsigned char gun_id = 0;

    // 判读枪号是否有效
    if (pOrderGun->uiAddr > Globa->Charger_param.gun_config || 0 == pOrderGun->uiAddr)
    {
        return;
    }

    gun_id = pOrderGun->uiAddr - 1;

    // 已插枪不能预约
    if (1 == Globa->Control_DC_Infor_Data[gun_id].gun_link)
    {
        data.ackResult = 10;
    }
    // 已预约
    else if (1 == Globa->Control_DC_Infor_Data[gun_id].App.APP_book_start)
    {
        // 当用户ID相同则应答成功
        if (0 == memcmp(Globa->Control_DC_Infor_Data[gun_id].App.ID, pOrderGun->user,
            sizeof(Globa->Control_DC_Infor_Data[gun_id].App.ID)))
        {
            data.ackResult = 0;
        }
        else // 不然应答设备忙
        {
            data.ackResult = 10;
        }
    }
    // 设备禁用或者设备故障
    else if ((Globa->Control_DC_Infor_Data[gun_id].Error_system != 0) ||
        (0 == Globa->Electric_pile_workstart_Enable[gun_id]))
    {
        data.ackResult = 12;
    }
    // 设备忙
    else if ((Globa->QT_Step > 0x03))
    {
        data.ackResult = 12;
    }
    // 设备忙
    else if (Globa->system_upgrade_busy)
    {
        data.ackResult = 12;
    }
    else
    {
        // 云平台订单号
        memcpy(Globa->Control_DC_Infor_Data[gun_id].App.busy_SN, pOrderGun->remoteOrder,
            sizeof(Globa->Control_DC_Infor_Data[gun_id].App.busy_SN));

        // 用户ID
        memcpy(Globa->Control_DC_Infor_Data[gun_id].App.ID, pOrderGun->user,
            sizeof(Globa->Control_DC_Infor_Data[gun_id].App.ID));

        // 相关控制变量保存
        Globa->Control_DC_Infor_Data[gun_id].App.APP_book_start = 1;
        Globa->Control_DC_Infor_Data[gun_id].App.APP_book_time = pOrderGun->bookTime;
        Globa->Control_DC_Infor_Data[gun_id].App.APP_book_Start = pOrderGun->bookStart;
    }

    /* 应答数据处理 */
    data.uiAddr = pOrderGun->uiAddr;
    /* 云平台订单号 */
    memcpy(data.remoteOrder, pOrderGun->remoteOrder, sizeof(pOrderGun->remoteOrder));
    /* 用户号 */
    memcpy(data.user, pOrderGun->user, sizeof(pOrderGun->user));
    CommPostEvent(TCU2REMOTE_ORDER_CHARGE_ACK, &data, sizeof(data));
    return;
}

/**
******************************************************************************
* @brief       接收取消预约充电枪
* @param[in]   unsigned char* pData     接收到的数据
* @param[in]   unsigned int len         数据长度
* @param[out]  NONE
* @retval      NONE
* @details
* @note        因为协议响应码定的太简单，所以目前全部填的0
******************************************************************************
*/
static void PlatProt_RecvCancleOrderGun(unsigned char* pData, unsigned int len,)
{
    T_SOrderChargeCancelAck data;
    T_SOrderChargeCancel *pCancleOrderGun = (T_SOrderChargeCancel *)pData;
    unsigned char gun_id = 0;

    // 判读枪号是否有效
    if (pCancleOrderGun->uiAddr > Globa->Charger_param.gun_config || 
        0 == pCancleOrderGun->uiAddr)
    {
        return;
    }

    gun_id = pCancleOrderGun->uiAddr - 1;

    // 回确认吧
    if (1 != Globa->Control_DC_Infor_Data[gun_id].App.APP_book_start)
    {
        data.ackResult = 0;
    }
    // 用户ID不相同
    else if (0 != memcmp(Globa->Control_DC_Infor_Data[gun_id].App.ID, pCancleOrderGun->user,
        sizeof(Globa->Control_DC_Infor_Data[gun_id].App.ID)))
    {
        data.ackResult = 0;
    }
    // 订单号不相同
    else if (0 != memcmp(Globa->Control_DC_Infor_Data[gun_id].App.busy_SN, pCancleOrderGun->remoteOrder,
        sizeof(Globa->Control_DC_Infor_Data[gun_id].App.busy_SN)))
    {
        data.ackResult = 0;
    }

    Globa->Control_DC_Infor_Data[gun_id].App.APP_book_start = 0;
    Globa->Control_DC_Infor_Data[gun_id].App.APP_book_time = 0;
    Globa->Control_DC_Infor_Data[gun_id].App.APP_book_Start = 0;

    /* 应答数据处理 */
    data.uiAddr = pCancleOrderGun->uiAddr;
    memcpy(data.user, pCancleOrderGun->user, sizeof(data.user));
    memcpy(data.remoteOrder, pCancleOrderGun->remoteOrder, sizeof(data.remoteOrder));
    CommPostEvent(TCU2REMOTE_CANCEL_ORDER_ACK, &data, sizeof(data));
    return;
}

/**
******************************************************************************
* @brief       接收超时取消预约充电枪应答
* @param[in]   unsigned char* pData     接收到的数据
* @param[in]   unsigned int len         数据长度
* @param[out]  NONE
* @retval      NONE
* @details
* @note        
******************************************************************************
*/
static void PlatProt_RecvCancleOrderGunAck(unsigned char* pData, unsigned int len)
{
    T_SChargeCancelOrderAck *pChargeCancleOrderGunAck = (T_SChargeCancelOrderAck *)pData;
    unsigned char gun_id = 0;

    // 判读枪号是否有效
    if (pChargeCancleOrderGunAck->uiAddr > Globa->Charger_param.gun_config ||
        0 == pChargeCancleOrderGunAck->uiAddr)
    {
        return;
    }

    gun_id = pChargeCancleOrderGunAck->uiAddr - 1;

    // 应答失败
    if (0 != pChargeCancleOrderGunAck->ackResult)
    {
        return;
    }

    // 订单号不相同
    if (0 != memcmp(Globa->Control_DC_Infor_Data[gun_id].App.busy_SN, pChargeCancleOrderGunAck->remoteOrder,
        sizeof(Globa->Control_DC_Infor_Data[gun_id].App.busy_SN)))
    {
        return;
    }

    Globa->Control_DC_Infor_Data[gun_id].App.APP_book_start = 0;
    Globa->Control_DC_Infor_Data[gun_id].App.APP_book_time = 0;
    Globa->Control_DC_Infor_Data[gun_id].App.APP_book_Start = 0;
}

/**
******************************************************************************
* @brief       接收下放地锁
* @param[in]   unsigned char* pData     接收到的数据
* @param[in]   unsigned int len         数据长度
* @param[out]  NONE
* @retval      NONE
* @details
* @note
******************************************************************************
*/
static void PlatProt_RecvLandLockOn(unsigned char* pData, unsigned int len)
{
    T_SGroundLockAck data = { 0 };
    T_SGroundLock *pGroundLock = (T_SGroundLock *)pData;
    unsigned char gun_id = 0;

    // 判读枪号是否有效
    if (pGroundLock->lockNo > Globa->Charger_param.gun_config ||
        0 == pGroundLock->lockNo)
    {
        return;
    }

    gun_id = pGroundLock->lockNo - 1;

    memcpy(Globa->Control_DC_Infor_Data[gun_id].landLockBusySN, pGroundLock->remoteOrder,
        sizeof(Globa->Control_DC_Infor_Data[gun_id].landLockBusySN));

    memcpy(Globa->Control_DC_Infor_Data[gun_id].landLockUserID, pGroundLock->user,
        sizeof(Globa->Control_DC_Infor_Data[gun_id].landLockUserID));

    Globa->Control_DC_Infor_Data[gun_id].landLockOnResult = 22;  // 应答无地锁

    /*处理应答数据*/
    data.lockNo = pGroundLock->lockNo;
    data.ackResult = Globa->Control_DC_Infor_Data[gun_id].landLockOnResult;
    CommPostEvent(TCU2REMOTE_LAND_LOCK_ACK, &data, sizeof(data));
    return;
}

/**
******************************************************************************
* @brief       接收订单号上报应答
* @param[in]   unsigned char* pData     接收到的数据
* @param[in]   unsigned int len         数据长度
* @param[out]  NONE
* @retval      NONE
* @details
* @note
******************************************************************************
*/
static void PlatProt_RecvUploadBillAck(unsigned char* pData, unsigned int len)
{
    T_SUploadBillAck *pUploadBillAck = (T_SUploadBillAck *)pData;
    int upNum = 0;
    int i = 0;
   
    // 判读枪号是否有效
    if (pUploadBillAck->uiAddr > Globa->Charger_param.gun_config ||
        0 == pUploadBillAck->uiAddr)
    {
        return;
    }

    pthread_mutex_lock(&busy_db_pmutex);

    upNum = (00 == pUploadBillAck->ackResult) ? 0xAA : 0x44;

    Set_Busy_UP_num(0, s_Busy_Need_Up_id, upNum);//标记该条充电记录上传成功

    //再检测5次看是否没成功
    for (i = 0; i < 5; i ++) 
    {
        usleep(200000);

        //查询该记录是否上传成功
        if (Select_NO_Over_Record(0, s_Busy_Need_Up_id, upNum) != -1)
        {
            Set_Busy_UP_num(0, s_Busy_Need_Up_id, upNum);
        }
        else
        {
            break;
        }      
    }

    // 锁卡的记录上送后修改锁卡标记为2，后续在本机解锁，必须本机在线，
    // 因为记录已上送，解锁后要发解锁成功标记给后台
    if (1 == s_busy_db_data.card_deal_result)
    {
        Set_Unlock_flag(s_Busy_Need_Up_id, 2);
    }
        
    pthread_mutex_unlock(&busy_db_pmutex);

    return;
}

/**
******************************************************************************
* @brief       接收刷卡鉴权应答
* @param[in]   unsigned char* pData     接收到的数据
* @param[in]   unsigned int len         数据长度
* @param[out]  NONE
* @retval      NONE
* @details
* @note        todo:这一块还需要确定
******************************************************************************
*/
static void PlatProt_RecvCardAuthAck(unsigned char* pData, unsigned int len)
{
    return;
}

#if 0 // 暂时用不上，没有开发完成
static void PlatProt_RecvSetTcuSettings(unsigned char* pData, unsigned int len)
{
    T_ReomteSetSettings *pReomteSetSettings = (T_ReomteSetSettings *)pData;
    unsigned char index = 0;
    float fTemp;
    int   nTemp;

    for (index = 0; index < pReomteSetSettings->count; index++)
    {
        if (REMOTE_CCU_SETTINGS == pReomteSetSettings->settingType)
        {
            switch (pReomteSetSettings->key[index][0])
            {
            case eCCUPara_AcVoltOver:  // 交流输入过压值
            {
                memcpy(&fTemp,pReomteSetSettings->keyValue[index], sizeof(float));
                fTemp *= 1000;
                Globa->Charger_param.input_over_limit = (UINT32)fTemp;
                break;
            }
            case eCCUPara_AcVoltLess:  // 交流输入欠压值
            {
                memcpy(&fTemp, pReomteSetSettings->keyValue[index], sizeof(float));
                fTemp *= 1000;
                Globa->Charger_param.input_lown_limit = (UINT32)fTemp;
                break;
            }
            case eCCUPara_AcVoltDelta:  // 交流输入过欠压回差值
            {
                memcpy(&fTemp, pReomteSetSettings->keyValue[index], sizeof(float));
                fTemp *= 1000;
                Globa->Charger_param.input_Volt_Delta = (UINT32)fTemp;
                break;
            }
            case eCCUPara_AcVoltPhLost:  // 交流输入缺相值
            {
                memcpy(&fTemp, pReomteSetSettings->keyValue[index], sizeof(float));
                fTemp *= 1000;
                Globa->Charger_param.input_Volt_phLost = (UINT32)fTemp;
                break;
            }
            case eCCUPara_DcVoltOver:   // 直流输出过压值
            {
                memcpy(&fTemp, pReomteSetSettings->keyValue[index], sizeof(float));
                fTemp *= 1000;
                Globa->Charger_param.output_over_limit = (UINT32)fTemp;
                break;
            }
            case eCCUPara_DcVoltLess:   // 直流输出欠压值
            {
                memcpy(&fTemp, pReomteSetSettings->keyValue[index], sizeof(float));
                fTemp *= 1000;
                Globa->Charger_param.output_lown_limit = (UINT32)fTemp;
                break;
            }
            case eCCUPara_DcVoltDelta:   // 直流输出过欠压回差值
            {
                memcpy(&fTemp, pReomteSetSettings->keyValue[index], sizeof(float));
                fTemp *= 1000;
                Globa->Charger_param.output_Volt_Delta = (UINT32)fTemp;
                break;
            }
            case eCCUPara_GunType:      // 枪类型
            {
                memcpy(&nTemp, pReomteSetSettings->keyValue[index], sizeof(int));
                Globa->Charger_param.output_Volt_Delta = (UINT32)fTemp;
                break;
            }
            }

        }

    }
}
#endif





void PlatProt_MsgRecvPoll(unsigned char* pRecv_buf, unsigned short buf_size)
{
    QUEUE_MSG QueueMsg;

    if (NULL == pRecv_buf || 0 == buf_size)
    {
        return;
    }

    QueueMsg.pData = pRecv_buf;

    if (FALSE == Pob_Node(QUEUE_DEV_TCU, &QueueMsg, buf_size))
    {
        return;
    }
   
    switch (QueueMsg.MsgID)
    {
        // 心跳接收
        case REMOTE2TCU_HEART_BEAT:         
        { 
            PlatProt_RecvHeart(QueueMsg.pData, QueueMsg.DataLen);          
            break;
        }
        
        // 时钟同步 
        case  REMOTE2TCU_TIME_SYNC:
        {
            PlatProt_RecvTimeSyn(QueueMsg.pData, QueueMsg.DataLen);          
            break;
        }

        // 费率和时段设置
        case REMOTE2TCU_PRICE_SET:
        {
            PlatProt_RecvPriceSet(QueueMsg.pData, QueueMsg.DataLen);          
            break;
        }

        // 黑名单获取 
        case REMOTE2TCU_GETBLACKLIST_ACK:   
        { 
            PlatProt_RecvBlackListAck(QueueMsg.pData, QueueMsg.DataLen);   
            break;
        }

        // 报文周期设置
        case REMOTE2TCU_MSG_PERIOD:
        {
            PlatProt_RecvTimeSpan(QueueMsg.pData, QueueMsg.DataLen);
            break;
        }

        // 二维码设置
        case REMOTE2TCU_QR_SET:
        {
            PlatProt_RecvQrCode(QueueMsg.pData, QueueMsg.DataLen);
            break;
        }
        
        // 停车费设置 
        case REMOTE2TCU_STOP_PRICE_SET:
        {
            PlatProt_RecvStopPriceSet(QueueMsg.pData, QueueMsg.DataLen);   
            break;
        }

        // 充电桩最大功率控制
        case REMOTE2TCU_PWR_ADJUST:
        {
            PlatProt_RecvPowerAdjust(QueueMsg.pData, QueueMsg.DataLen);   
            break;
        }

        // 停止服务控制
        case REMOTE2TCU_STOP_SERVICE:
        {
            PlatProt_RecvStopService(QueueMsg.pData, QueueMsg.DataLen);
            break;
        }

        // 恢复服务控制
        case REMOTE2TCU_RESUME_SERVICE:
        {
            PlatProt_RecvStartService(QueueMsg.pData, QueueMsg.DataLen);
            break;
        }

        // 启动充电
        case REMOTE2TCU_REMOTE_START:
        {
            PlatProt_RecvChgStart(QueueMsg.pData, QueueMsg.DataLen);
            break;
        }

        // 停止充电
        case REMOTE2TCU_REMOTE_STOP:
        {
            PlatProt_RecvChgStop(QueueMsg.pData, QueueMsg.DataLen);
            break;
        }

        // VIN鉴权应答
        case REMOTE2TCU_VIN_CHARGE_ACK:
        {
            PlatProt_RecvVinAuthAck(QueueMsg.pData, QueueMsg.DataLen);
            break;
        }

        // 启动事件上报应答
        case REMOTE2TCU_CARD_CHARGE_ACK:
        {
            PlatProt_RecvStartEventReportAck(QueueMsg.pData, QueueMsg.DataLen);
            break;
        }

        // 预约充电
        case REMOTE2TCU_ORDER_CHARGE:
        {
            PlatProt_RecvOrderGun(QueueMsg.pData, QueueMsg.DataLen);
            break;
        }

        // 取消预约充电枪
        case REMOTE2TCU_CANCEL_ORDER:
        {
            PlatProt_RecvCancleOrderGun(QueueMsg.pData, QueueMsg.DataLen);
            break;
        }

        // 预约超时应答
        case REMOTE2TCU_ORDER_TIMEOUT_ACK:
        {
            PlatProt_RecvCancleOrderGunAck(QueueMsg.pData, QueueMsg.DataLen);
            break;
        }

        // 下放地锁
        case REMOTE2TCU_LAND_LOCK:
        {
            PlatProt_RecvLandLockOn(QueueMsg.pData, QueueMsg.DataLen);
            break;
        }

        // 交易记录应答
        case REMOTE2TCU_BILL_REPORT_ACK:
        {
            PlatProt_RecvUploadBillAck(QueueMsg.pData, QueueMsg.DataLen);
            break;
        }

        // 刷卡充电鉴权
        case REMOTE2TCU_CHECK_CARD_ACK:
        {
            PlatProt_RecvCardAuthAck(QueueMsg.pData, QueueMsg.DataLen);
            break;
        }

        default:
            break;
    }
}


bool PlatProt_Init(void)
{
    unsigned short bufLen = 4 * 1024;
    unsigned char *buf = NULL;

    buf = (unsigned char *)calloc(1, bufLen);

    if (NULL == buf)
    {
        AppLogOut("%s 队列内存申请失败\r\n", QUEUE_DEV_TCU);
        return FALSE;
    }

    if (FALSE == Creat_Queue(QUEUE_DEV_TCU, buf, bufLen))
    {
        free(buf);
        return FALSE;
    }

    AppLogOut("%s 队列内存申请成功\r\n", QUEUE_DEV_TCU);

    return TRUE;
}































