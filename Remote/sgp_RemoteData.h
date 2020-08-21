#include "Data.h"

enum
{
    SEND_PERIOD = 0,
    SEND_RIGHT_NOW = 1,
};


extern void sgp_SendLoginInfo(void);                                    // 发送登录信息
extern void SendHeartBeat(int rightNow);                            // 发送心跳
extern void SendYaoCe(int devId, int rightNow);                     // 发送遥测
extern void SendYaoMai(int devId, int rightNow);                    // 发送遥脉
extern void SendYaoXin(int devId, int rightNow);                    // 发送遥信
extern void SendBmsStatus(int devId, int rightNow);                 // 发送BMS充电实时数据
extern void SendEventUpload(int devId);                             // 启动事件上报
extern void CancelOrderByTimeout(int devId);                        // 超时取消充电枪的预约
extern void SendSettingRequest();                                   // 发送配置请求
extern void sgp_ChargeStartAckHandle();                                 // 充电指令响应处理
extern void sgp_UserCardCheckHandle();                                  // 刷卡鉴权处理
extern int RcvCardCheckAckHandle(T_SCheckCardDataAck* data);       // 刷卡鉴权应答处理
extern void sgp_VinStartChargeHandle();                                 // VIN码启动充电处理
extern int RcvVinStartAckHandle(T_SVinChargeAck* data);            // vin码启动应答处理
extern void RcvHeartDataDeal(T_SRemoteHeart* data);                 // 接收到心跳后的数据处理
extern void GunStatChangedHandle();                                 // 充电枪状态改变时的处理
extern void SendRealInfo();                                         // 实时数据发送
extern void CheckGunStatChange(int devId);                          // 枪状态变化时的处理
extern int CheckFaultChange(int devId,T_SFaultInfo* data);          // 检查是否有故障变化
extern void HeartBeatPeriodSendHandle();                            // 心跳周期发送处理
extern void YaoCePeriodSendHandle();                                // 遥测周期发送处理
extern void YaoXinPeriodSendHandle();                               // 遥信周期发送处理
extern void YaoMaiPeriodSendHandle();                               // 遥脉周期发送处理
extern void BmsStatusPeriodSendHandle();                            // BMS实时信息周期发送处理
extern void BillInfoUpload();                                       // 充电账单上送
extern void RcvBillComfirmHandle(T_SUploadBillAck* data);           // 接收到充电账单上送应答处理
extern void GunErrChangedHandle();                                  // 充电枪故障信息上报处理
extern void RcvFaultMsgHandle(T_SFaultInfoAck* data);               // 收到故障信息应答
extern void RemoteServerDataInit(void);                             // 远程服务数据初始化