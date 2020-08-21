#ifndef REMOTECOMM_H
#define REMOTECOMM_H
#include "TcuComm.h"
#include "ParamKey.h"

#define OEM_CODE 0x03        //厂商代码，易事特固定为0x03

#define GUN_NUM_MAX 16

#define LOGIN_INIT -1
#define LOGIN_SUCCESS 5 //登陆成功
#define LOGIN_NOT 0     //没有登陆

#define NO_FAULT 0 //无故障
#define FAULTING 1 //有故障

#pragma pack(1)

enum
{
    VIN_CHARGE_SPECIAL = 10,
};

enum
{
    GUN_TYPE_DC = 1,
    GUN_TYPE_AC = 2,
    GUN_TYPE_MAX
};

enum
{
    HEARTBEAT_GUN_STATUS_FAULST = 1,        //故障
    HEARTBEAT_GUN_STATUS_WARNING,           //告警
    HEARTBEAT_GUN_STATUS_EMPTY,             //空闲
    HEARTBEAT_GUN_STATUS_CHARGING,          //充电中
    HEARTBEAT_GUN_STATUS_CHARGING_COMPLETE, //充电完成
    HEARTBEAT_GUN_STATUS_BOOK,              //预约中
    HEARTBEAT_GUN_STATUS_WAITTING,          //等待（插枪未充）
    HEARTBEAT_GUN_STATUS_DISABLE,           //停止服务
    HEARTBEAT_GUN_STATUS_OUTLINE = 10,      //离线
    HEARTBEAT_GUN_STATUS_START_CHARGING,    //启动中
    HEARTBEAT_GUN_STATUS_STOP_CHARGING,     //停止中
    HEARTBEAT_GUN_STATUS_FIXING,            //维护中
};

enum
{
    HEARTBEAT_SYS_STATUS_FAULT = 1, //01：故障
    HEARTBEAT_SYS_STATUS_WARNING = 2,  //02：告警
    HEARTBEAT_SYS_STATUS_EMPTY = 3, //03：空闲
    HEARTBEAT_SYS_STATUS_USING = 4, //04：使用中
    HEARTBEAT_SYS_STATUS_STOP = 8,   //08：停止服务
    HEARTBEAT_SYS_STATUS_OUTLINE = 10, //10：离线
};

enum
{
    FAULT_SOURCE_TCU = 1,
    FAULT_SOURCE_GUN = 2,
};

enum
{
    FAULT_STATE_HANPPEN = 1,
    FAULT_STATE_RESUME = 2,
};

enum
{
    REMOTE_CCU_SETTINGS = 0,
    REMOTE_SYS_SETTINGS,
    REMOTE_OTHER_SETTINGS,
};

//故障信息上报
typedef struct R_SFaultInfo
{
    unsigned int uniqueId;      //用于应答时候确认哪个故障
    unsigned char gunNo;        //枪编号：01，表示1号枪
    unsigned char faultTime[6]; //故障时间 故障时间 格式：年-2000 月日时分秒
    unsigned int faultId;       //故障编号
    unsigned char faultStat;    //故障状态 01：表示发生； 02：表示结束
    unsigned char faultSource;  //故障源 01：整桩 02：枪
    unsigned char faultLevel;   //故障等级
} R_SFaultInfo;

//充电桩时钟调控
typedef struct R_STimeSet
{
    unsigned char time[6]; //设置时间 格式：年-2000月日时分秒
} R_STimeSet;

enum
{
    REMOTE_CHARGE_TYPE_CHARGE = 1, //1：按电量充电
    REMOTE_CHARGE_TYPE_TIME,       //2：按时间充电
    REMOTE_CHARGE_TYPE_MONEY,     //3：按金额充电
    REMOTE_CHARGE_TYPE_AUTO        //4：1：自动充满
};

enum
{
    STARTCHARGE_RESULT_OK = 0,              //00—确认
    STARTCHARGE_RESULT_GUN_DISCONNECT = 9,  //09—未插枪
    STARTCHARGE_RESULT_GUN_BOOKED,          //10—设备被占用
    STARTCHARGE_RESULT_NO_MONEY,            //11—余额不足
    STARTCHARGE_RESULT_GUN_FAULT,           //12—设备故障
    STARTCHARGE_RESULT_CAR_CONNECT_TIMEOUT, //13—车辆连接超时，请重新插枪
    STARTCHARGE_RESULT_SYS_BUSY,            //14—设备系统繁忙
};

enum
{
    STOP_CHARGE_RESULT_OK = 0,
    STOP_CHARGE_RESULT_BUSY = 14,
    STOP_CHARGE_RESULT_USER_NO_MATCH = 15,
    STOP_CHARGE_RESULT_UNKNOW = 16,
};

enum
{
    UPLOAD_BILL_CHARGE_TYPE_CARD_ONLINE = 1,
    UPLOAD_BILL_CHARGE_TYPE_USER_ONLINE,
    UPLOAD_BILL_CHARGE_TYPE_VIN,
    UPLOAD_BILL_CHARGE_TYPE_CARD_OUTLINE,
};

enum{
    REMOTE_UPGRADE_OK,
    REMOTE_UPGRADE_PWD_ERR,
    REMOTE_UPGRADE_SERVER_ADD_ERR,
    REMOTE_UPGRADE_DOWNLOAD_FAIL,
    REMOTE_UPGRADE_FORMAT_ERR,
    REMOTE_UPGRADE_OTHER_ERR
};

enum{
    OPT_FALSE,
    OPT_TRUE
};

#pragma pack()

#endif
