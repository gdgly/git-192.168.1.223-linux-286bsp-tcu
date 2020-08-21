#include "Data.h"

#ifndef REMOTE_QUEUE_SIZE
#define REMOTE_QUEUE_SIZE 20
#endif

// 承载报文消息的结构体
typedef struct _s_remote_data_msg
{
    struct _s_remote_data_msg * pre;
    struct _s_remote_data_msg * next;

    int msg_id;                         // 报文ID
    int length;                         // 报文长度
    char msg_data_arry[2048];           // 报文储存区
}SCommEventData;

// 远程任务消息队列
typedef struct _s_remote_msg_queue
{
    int size;                   // 消息储存区个数
    int store_num;              // 已经储存个数

    SCommEventData* store_pointer;         // 储存指针
    SCommEventData* fetch_pointer;         // 取指针
    SCommEventData  data_hub[REMOTE_QUEUE_SIZE];          // 数据仓库
}_sRemoteMsgQueue;


extern int ctrl_connect_lost_err_handle(int i,T_SFaultInfo* data);              // 充电控制板通信中断处理
extern int meter_connect_lost_err_handle(int i,T_SFaultInfo* data);             // 电表通信中断处理
extern int AC_connect_lost_err_handle(int i,T_SFaultInfo* data);                // 交流模块通信中断处理
extern int emergency_switch_err_handle(int i,T_SFaultInfo* data);               // 急停故障处理
extern int input_over_limit_err_handle(int i,T_SFaultInfo* data);               // 交流输入过压告警
extern int input_over_protect_err_handle(int i,T_SFaultInfo* data);             // 交流输入过压保护
extern int input_lown_limit_err_handle(int i,T_SFaultInfo* data);               // 交流输入欠压告警
extern int input_switch_off_err_handle(int i,T_SFaultInfo* data);               // 交流输入开关跳闸
extern int fanglei_off_err_handle(int i,T_SFaultInfo* data);                    // 交流防雷器跳闸
extern int charger_connect_lost_err_handle(int i,T_SFaultInfo* data);           // 充电模块通信中断
extern int charger_over_tmp_err_handle(int i,T_SFaultInfo* data);               // 充电模块过温告警
extern int charger_OV_down_err_handle(int i,T_SFaultInfo* data);                // 充电模块过压关机告警
extern int charger_fun_err_handle(int i,T_SFaultInfo* data);                    // 模块风扇告警
extern int charger_other_err_handle(int i,T_SFaultInfo* data);                  // 模块其他告警
extern int sistent_err_handle(int i,T_SFaultInfo* data);                        // 绝缘告警
extern int output_over_limit_err_handle(int i,T_SFaultInfo* data);              // 系统输出过压告警
extern int output_low_limit_err_handle(int i,T_SFaultInfo* data);               // 系统输出欠压告警
extern int output_switch_off_err_handle(int i,T_SFaultInfo* data);              // 直流输出开关跳闸
extern int sys_over_tmp_err_handle(int i,T_SFaultInfo* data);                   // 充电桩过温故障
extern int charger_err_handle(int i,T_SFaultInfo* data);                        // 充电模块故障
extern int GetStopReasonByOld(int val);                                         // 根据一代协议储存的停止原因转换为二代停止原因编码
extern void CoverBillDataByDb(ISO8583_charge_db_data* src,T_SBillData *dst);    // 根据数据库中充电账单填写需要上传的账单数据
//extern void CreateLocalOrder(int devId,void* data);                             // 创建交易流水号
extern void SetOperationManagePort(void * addr,int len);                        // 设置运维地址
extern unsigned short GetOperationManagePort(void);                             // 获取运维端口
extern unsigned char GetSysStatus(void);                                        // 获取设备状态
extern unsigned char GetSysFaultStatus(int i);                                  // 系统故障状态
extern unsigned char GeteModelFaultStatus(int i);                               // 充电模块故障状态
extern unsigned char GetChargeSysFaultStatus(int i);                            // 充电系统故障状态
extern unsigned char GetDCOutputSwitchStatus(int i);                            // 直流输出开关状态
extern unsigned char GetDCContactStatus(int i);                                 // 直流输出接触器状态
extern unsigned char GetRepairDoorStatus(int i);                                // 枪门状态
extern unsigned char GetGunHomingStatus(int i);                                 // 枪归位状态
extern void FillChargeStartAck(int i,int flag,T_SChargeDataAck* data);          // 启动充电应答报文填充
extern int FillCardCheckData(int i,T_SCheckCardData* data);                     // 启动刷卡鉴权报文填充
extern void FillVinStartData(int i,T_SVinCharge* data);                         // vin码充电数据填充
extern void FillCardStartChargeData(int i,T_SCardCharge* data);                 // 刷卡启动充电数据填充
extern void TransferStr2Time6(Time6Bits* dst,char* src);                        // 将yyyyMMddHHmmss形式的时间转换为Time6Bits格式
extern sgp_Task_sleep(unsigned int ms);                                         // 线程休眠
extern int CommPostEvent(int cmd,void *data,int len);                           // 将报文发送到远程发送任务
extern int CommFetchEvent(SCommEventData * data);                               // 从远程任务消息队列中取消息