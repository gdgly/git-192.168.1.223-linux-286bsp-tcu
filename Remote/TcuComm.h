#ifndef TCU_COMM_H
#define TCU_COMM_H

//插件消息ID
#define HMI_DEV_ID 1
#define TCU_DEV_ID 2
#define PCU_DEV_ID 3
#define CCU_DEV_ID 4
#define REMOTE_DEV_ID 5
#define UPDATE_DEV_ID 6

#define SYS_DEF_A_UNIT_MAX_BUS_NUM   32    //单个单元柜母线最大64，用于偏移计算，注意同时运行代码这个必须同一，否则会导致功率分配和寻址错误
#define SYS_DEF_A_UNIT_MAX_MOD_NUM   32    //单个单元柜模块最大64，用于偏移计算，注意同时运行代码这个必须同一，否则会导致功率分配和寻址错误
#define MAX_CHARGING_DATA_LEN 50

#define CCU_MAX_NUM 16 //CCU最大数量

#define BILL_DEV_ID 1000
//#define YX_MODE

#define TCU_FIRMWARE "tcu_update"
#define PCU_FIRMWARE "pcu_update"
#define CCU_FIRMWARE "ccu_update"
#define HMI_FIRMWARE "cphmi_update"
#define REMOTE_FIRMWARE "remote_update"
#define FIRMWARE_DIR "data/update"
#define FINAL_FIRMWARE_DIR "/root/app/data/Update"
#define DOWNLOAD_PORT 8080
#define PRODUCT_ID_PATH "config/PRODUCT"


#define NONE                 "\e[0m"
#define BLACK                "\e[0;30m"
#define L_BLACK              "\e[1;30m"
#define RED                  "\e[0;31m"
#define L_RED                "\e[1;31m"
#define GREEN                "\e[0;32m"
#define L_GREEN              "\e[1;32m"
#define BROWN                "\e[0;33m"
#define YELLOW               "\e[1;33m"
#define L_YELLOW             "\e[0;33m"
#define BLUE                 "\e[0;34m"
#define L_BLUE               "\e[1;34m"
#define PURPLE               "\e[0;35m"
#define L_PURPLE             "\e[1;35m"
#define CYAN                 "\e[0;36m"
#define L_CYAN               "\e[1;36m"
#define GRAY                 "\e[0;37m"
#define WHITE                "\e[1;37m"
#define L_WHITE              "\e[0;37m"

#define BOLD                 "\e[1m"
#define UNDERLINE            "\e[4m"
#define BLINK                "\e[5m"
#define REVERSE              "\e[7m"
#define HIDE                 "\e[8m"
#define CLEAR                "\e[2J"
#define CLRLINE              "\r\e[K" //or "\e[1K\r"


//#define NONE                 \e[0m
//#define RED                  \e[0;31m
#define AppLogOutr(format, arg...) do{AppLogOut(RED format NONE,## arg);}while(0)
#define AppLogOutg(format, arg...) do{AppLogOut(GREEN format NONE,## arg);}while(0)
#define AppLogOutb(format, arg...) do{AppLogOut(BLUE format NONE,## arg);}while(0)
#define AppLogOutk(format, arg...) do{AppLogOut(BLACK format NONE,## arg);}while(0)
#define AppLogOutw(format, arg...) do{AppLogOut(WHITE format NONE,## arg);}while(0)
#define AppLogOuty(format, arg...) do{AppLogOut(YELLOW format NONE,## arg);}while(0)
#define AppLogOutc(format, arg...) do{AppLogOut(CYAN format NONE,## arg);}while(0)
#define AppLogOutp(format, arg...) do{AppLogOut(PURPLE format NONE,## arg);}while(0)

#define AppLogOutlr(format, arg...) do{AppLogOut(L_RED format NONE,## arg);}while(0)
#define AppLogOutlg(format, arg...) do{AppLogOut(L_GREEN format NONE,## arg);}while(0)
#define AppLogOutlb(format, arg...) do{AppLogOut(L_BLUE format NONE,## arg);}while(0)
#define AppLogOutlk(format, arg...) do{AppLogOut(L_BLACK format NONE,## arg);}while(0)
#define AppLogOutlw(format, arg...) do{AppLogOut(L_WHITE format NONE,## arg);}while(0)
#define AppLogOutly(format, arg...) do{AppLogOut(L_YELLOW format NONE,## arg);}while(0)
#define AppLogOutlc(format, arg...) do{AppLogOut(L_CYAN format NONE,## arg);}while(0)
#define AppLogOutlp(format, arg...) do{AppLogOut(L_PURPLE format NONE,## arg);}while(0)

//全部设备类型定义
enum Enum_DevTypeDef{
    DEV_TYPE_TCU = 0,  //tcu
    DEV_TYPE_CCU_BUS,   //ccu
    DEV_TYPE_PCU,       //pcu
    DEV_TYPE_HMI,
    DEV_TYPE_REMOTE,
    DEV_TYPE_MAX
};

//TCU内部点表
enum
{
    TCU_POINT_REMOTE_STATUS = 10,         //远程状态
    TCU_POINT_PCU_DATA_ENABLE,            //PCU数据使能
    TCU_POINT_TCU_SYS_STATUS,             //TCU系统状态
    TCU_POINT_TCU_POWER_STATUS,           //TCU系统掉电状态 0表示上电中 1表示掉电
    TCU_POINT_TCU_DISK_STATUS,            //磁盘空间状态 0表示空间足够 1 表示不足
    TCU_POINT_PCU_FAULT_STAT,             //PCU系统故障
    TCU_POINT_CHARGE_FAULT_STAT,          //充电系统故障
    TCU_POINT_TCU_FAULT_STAT,             //TCU系统故障
    TCU_POINT_MOD_FAULT_STAT,             //模块故障
    TCU_POINT_BMS_FAULT_STAT,             //BMS故障
    TCU_POINT_PCU_NUM,                    //pcu数量
    TCU_POINT_PCU_COMM_ADDR,              //pcu通讯地址
    TCU_POINT_GROUP_ADD,                  //群充地址
    TCU_POINT_CCU_CFG,                    //CCU配置
    TCU_POINT_PARALLELS_CFG,              //并机配置
    TCU_POINT_PCU_SERVER_FLAG,            //PCU插件服务初始化标记

    TCU_POINT_USB_TMP_PCU_VERSION,        //新PCU版本
    TCU_POINT_USB_TMP_REMOTE_VERSION,     //新REMOTE版本
    TCU_POINT_USB_TMP_CCU_VERSION,        //新CCU版本
    TCU_POINT_USB_TMP_HMI_VERSION,        //新HMI版本
    TCU_POINT_USB_TMP_TCU_VERSION,        //新TCU版本

    TCU_POINT_NEW_PCU_VERSION,            //新PCU版本
    TCU_POINT_NEW_PCU_VERSION_MD5,        //新PCU版本md5
    TCU_POINT_NEW_REMOTE_VERSION,         //新REMOTE版本
    TCU_POINT_NEW_REMOTE_VERSION_MD5,     //新REMOTE版本md5
    TCU_POINT_NEW_CCU_VERSION,            //新CCU版本
    TCU_POINT_NEW_CCU_VERSION_MD5,        //新CCU版本md5
    TCU_POINT_NEW_HMI_VERSION,            //新HMI版本
    TCU_POINT_NEW_HMI_VERSION_MD5,        //新HMI版本md5
    TCU_POINT_NEW_TCU_VERSION,            //新TCU版本
    TCU_POINT_NEW_TCU_VERSION_MD5,        //新TCU版本md5

    TCU_POINT_GET_FIRMWARE_LIST,
    TCU_POINT_CCU_WORK_STATUS,           //CCU总的工作状态
    TCU_POINT_BMS_COLLECT_ENABLE,        //BMS采集的开关 采用账单流水号来决定
    TCU_POINT_UPGRADE_CHECK,             //升级检测状态标记
    TCU_POINT_BILL_RECORD_ENABLE,        //订单刷新点表开关

    TCU_POINT_TCP_RECORD_ENABLE,        //TCP记录开关
    TCU_POINT_UDP_RECORD_ENABLE,        //UDP记录开关

    TCU_POINT_TCU_FILE_SYSTEM_STATUS,   //TCU文件系统状态
};

enum{
    TCU_UPDATE_IMPORT_FIRMWARE = 1,//导入固件
    TCU_UPDATE_CONFIRM_FIRMWARE,//确认固件
};

enum{
    DOWNLOAD_ACK_OK = 101,
    DOWNLOAD_ACK_DOING,
    DOWNLOAD_ACK_FAIL,
};

typedef enum
{
    HMI2TCU_START_CHARGE                = 0x1101, //控制启动充电
    HMI2TCU_STOP_CHARGE                 = 0x1102, //控制停止充电
    HMI2TCU_CANCEL_ORDER_CHARGE         = 0x1103, //控制取消预约充电
    HMI2TCU_DISABLE_CHARGE              = 0x1104, //控制禁用充电
    HMI2TCU_USER_STATE_CHECK            = 0x1105, //账户状态验证
    HMI2TCU_CONTROL_GUN_LOCK            = 0x1106, //控制枪锁状态
    HMI2TCU_MANUAL_INSULATION_CHECK     = 0x1107, //手动绝缘控制
    HMI2TCU_CHARGE_SERVICE_START_STOP   = 0x1108, //充电服务启停控制
    HMI2TCU_CHARGE_POWER_CONTROL        = 0x1109, //充电功率调节控制指令
    HMI2TCU_MANUAL_START_CHARGE         = 0x1110, //手动启动充电
    HMI2TCU_MANUAL_STOP_CHARGE          = 0x1111, //手动停止充电（失效）
    HMI2TCU_FINAL_DATA                  = 0x1112, //结算状态数据确认
    HMI2TCU_GET_QRCODE                  = 0x1113, //获取二维码
    HMI2TCU_GET_USER_RECENT_BILL        = 0x1114, //获取该用户最近一次充电账单
    TCU2HMI_CONTROL_ACK                 = 0x1181, //控制命令统一响应
    TCU2HMI_USER_STATE_CHEKC            = 0x1182, //账户验证响应
    TCU2HMI_GET_QRCODE_ACK              = 0x1183, //获取二维码
    TCU2HMI_GET_USER_RECENT_BILL_ACK    = 0x1184, //获取该用户最近一次充电账单
    HMI2TCU_UNLOCK_CARD_CONFIRM         = 0x1115, //解卡确认
    TCU2HMI_UNLOCK_CARD_CONFIRM_ACK     = 0x1185, //解卡确认应答
    HMI2TCU_GET_FIRMWARE_LIST           = 0x1116, //获取固件列表
    TCU2HMI_GET_FIRMWARE_LIST_ACK       = 0x1186, //获取固件列表应答
    TCU2HMI_FIRMWARE_INFO               = 0x1187, //推送固件新固件信息
    HMI2TCU_FIRMWARE_INFO_ACK           = 0x1117, //推送固件新固件信息
    HMI2TCU_CHOOSE_FIRMWARE_UPDATE      = 0x1118, //选择固件
    TCU2HMI_CHOOSE_FIRAMWARE_UPDATE_ACK = 0x1188, //选择固件应答
    TCU2HMI_REBOOT_HMI                  = 0x1189, //TCU重启HMI
    HMI2TCU_CLEAR_ALL_ALARM             = 0x111A, //TCU重启HMI
    TCU2HMI_CLEAR_ALL_ALARM_ACK         = 0x118A, //TCU重启HMI
    TCU2HMI_UPDATE_RESULT               = 0x118B, //TCU发送升级结果
    HMI2TCU_LOGIN                       = 0X1600, //HMI请求登陆
    TCU2HMI_LOGIN_ACK                   = 0X1680, //TCU请求登陆应答
    HMI2TCU_HEART                       = 0x1601, //HMI心跳
    TCU2HMI_HEART                       = 0x1681, //TCU心跳
} HMI_CMD_ID;

typedef enum
{
    HMI2TCU_PCU_STATE           = 0x1201, //矩阵分配状态
    TCU2HMI_FINAL_DATA          = 0x1202, //结算状态的数据
    TCU2HMI_CHARGEING_DATA      = 0x1203, //充电中的状态数据
    TCU2HMI_REAL_DATA           = 0x1204, //实时状态的数据
    TCU2HMI_ORDER_CHARGE_DATA   = 0x1205, //充电枪预约信息
    TCU2HMI_CUR_WARNNING_DATA   = 0x1206, //当前告警数据
    TCU2HMI_PCU_STATE           = 0x1282, //矩阵功率分配状态
    TCU2HMI_MOD_STATE           = 0x1283, //矩阵模块分配状态
    TCU2HMI_RELAY_STATE         = 0x1284, //矩阵继电器分配状态
    TCU2HMI_MONITOR_DATA        = 0x1287, //监控数据
} STATE_ID;

typedef enum
{
    HMI2TCU_MEASURE_DATA_QUERY      = 0x1301, //测量数据查询命令
    TCU2HMI_MEASURE_DATA_QUERY_ACK  = 0x1381, //测量数据查询命令
} MEASURE_ID;

typedef enum
{
    HMI2TCU_QUERY_CONDITION             = 0x1401, //按条件查询记录
    HMI2TCU_QUERY_CONDITION_DATA        = 0x1402, //条件数据查询
    TCU2HMI_QUERY_CONDITION_ACK         = 0x1481, //按条件查询记录响应
    TCU2HMI_QUERY_CONDITION_DATA_ACK    = 0x1482, //条件数据查询相应
} QUERY_ID;

typedef enum
{
    HMI2TCU_GET_CONFIG = 0x1501,     //获取配置参数
    HMI2TCU_SET_CONFIG = 0x1502,     //设置配置参数
    TCU2HMI_GET_CONFIG_ACK = 0x1581, //获取配置参数响应
    TCU2HMI_SET_CONFIG_ACK = 0x1582  //设置配置参数响应
} CONFIG_ID;

typedef enum _REMOTE_CMD
{
    //登录(0x1700-0x170F)
    REMOTE2TCU_PARA_REPORT_ACK      = 0x1700,   //参数上报
    REMOTE2TCU_HEART_BEAT           = 0x1701,   //心跳报文=======周期上报                    有应答
    //监控(0x1710-0x172F)
    REMOTE2TCU_FAULT_MSG_ACK        = 0x1714,   //故障时上报故障信息=========                无应答
    //设置(0x1730-0x175F)
    REMOTE2TCU_TIME_SYNC            = 0x1730,   //充电桩时间设置
    REMOTE2TCU_PRICE_SET            = 0x1731,   //费率和时段设置
    REMOTE2TCU_STOP_PRICE_SET       = 0x1732,   //停车费设置
    REMOTE2TCU_MSG_PERIOD           = 0x1733,   //报文周期设置
    REMOTE2TCU_QR_SET               = 0x1734,   //下发二维码
    REMOTE2TCU_GETBLACKLIST_ACK     = 0x1735,   //黑名单
    REMOTE2TCU_GET_TCU_SETTINGS     = 0x1736,   //远程获取TCU参数
    REMOTE2TCU_SET_TCU_SETTINGS     = 0X1737,   //远程设置TCU参数
    REMOTE2TCU_GET_REMOTE_SETTINGS_ACK     = 0x1738, //远程应答TCU获取参数
    REMOTE2TCU_SET_REMOTE_SETTINGS_ACK     = 0X1739, //远程应答TCU设置参数
    //控制(0x1760-0x178F)
    REMOTE2TCU_PWR_ADJUST           = 0x1760,   //充电最大功率控制
    REMOTE2TCU_STOP_SERVICE         = 0x1761,   //暂停服务
    REMOTE2TCU_RESUME_SERVICE       = 0x1762,   //恢复服务
    REMOTE2TCU_RMOETE_REBOOT        = 0x1763,   //远程重启
    //业务(0x1790-0x17BF)
    REMOTE2TCU_REMOTE_START         = 0x1790,   //充电启动相关报文
    REMOTE2TCU_REMOTE_STOP          = 0x1791,   //充电停止相关报文
    REMOTE2TCU_VIN_CHARGE_ACK       = 0x1792,   //VIN码充电
    REMOTE2TCU_CARD_CHARGE_ACK      = 0x1793,   //刷卡充电
    REMOTE2TCU_ORDER_CHARGE         = 0x1794,   //预约充电
    REMOTE2TCU_CANCEL_ORDER         = 0x1795,   //取消预约
    REMOTE2TCU_ORDER_TIMEOUT_ACK    = 0x1796,   //预约超时应答
    REMOTE2TCU_LAND_LOCK            = 0x1797,   //下放地锁
    REMOTE2TCU_BILL_REPORT_ACK      = 0x1798,   //交易记录确认
    REMOTE2TCU_STOP_BILL_REPORT_ACK = 0x1799,   //停车费确认报文
    REMOTE2TCU_CHECK_CARD_ACK       = 0x179A,   //刷卡充电鉴权
    //其他(0x17C0-)
    REMOTE2TCU_UPGRADE_REQ          = 0x17C0,   //固件升级
    REMOTE2TCU_UPGRADE_RESULT_ACK   = 0x17C1,   //远程升级结果应答
    REMOTE2TCU_DOWNLOAD_REMOTE_ACK  = 0x17C3,   //升级远程
    REMOTE2TCU_GETLOG_REQ           = 0x17C5,   //请求充电桩上传日志文件
    REMOTE2TCU_GETLOG_ACK           = 0x17C6,   //上传回复

    //插件
    TCU2REMOTE_PLUGIN_INIT          = 0x1600,   //插件初始化信息
    //登录(0x1700-0x170F)
    TCU2REMOTE_PARA_REPORT          = 0x1700,   //参数上报
    TCU2REMOTE_HEART_BEAT           = 0x1701,   //心跳报文=======周期上报                    有应答
    //监控(0x1710-0x172F)
    TCU2REMOTE_YAO_CE               = 0x1710,   //充电中定时遥测===========充电中周期上报    无应答
    TCU2REMOTE_YAO_MAI              = 0x1711,   //充电中定时遥脉===========充电中周期上报    无应答
    TCU2REMOTE_YAO_XIN              = 0x1712,   //充电桩状态变化上报遥信===========状态变化  无应答
    TCU2REMOTE_BMS_REAL_DATA        = 0x1713,   //bms实时数据
    TCU2REMOTE_FAULT_MSG            = 0x1714,   //故障时上报故障信息=========                无应答
    //设置(0x1730-0x175F)
    TCU2REMOTE_TIME_SYNC_ACK        = 0x1730,   //充电桩时间设置
    TCU2REMOTE_PRICE_SET_ACK        = 0x1731,   //费率和时段设置
    TCU2REMOTE_STOP_PRICE_SET_ACK   = 0x1732,   //停车费设置
    TCU2REMOTE_MSG_PERIOD_ACK       = 0x1733,   //报文周期设置
    TCU2REMOTE_QR_SET_ACK           = 0x1734,   //下发二维码
    TCU2REMOTE_GETBLACKLIST         = 0x1735,   //黑名单
    TCU2REMOTE_GET_TCU_SETTINGS_ACK = 0x1736,   //TCU应答远程参数获取
    TCU2REMOTE_SET_TCU_SETTINGS_ACK = 0X1737,   //TCU应答远程参数设置
    TCU2REMOTE_GET_REMOTE_SETTINGS  = 0x1738,   //TCU获取远程参数
    TCU2REMOTE_SET_REMOTE_SETTINGS  = 0X1739,   //TCU设置远程参数
    //控制(0x1760-0x178F)
    TCU2REMOTE_PWR_ADJUST_ACK       = 0x1760,   //充电最大功率控制
    TCU2REMOTE_STOP_SERVICE_ACK     = 0x1761,   //暂停服务
    TCU2REMOTE_RESUME_SERVICE_ACK   = 0x1762,   //恢复服务
    TCU2REMOTE_RMOETE_REBOOT_ACK    = 0x1763,   //远程重启
    //业务(0x1790-0x17BF)
    TCU2REMOTE_REMOTE_START_ACK     = 0x1790,   //充电启动相关报文
    TCU2REMOTE_REMOTE_STOP_ACK      = 0x1791,   //充电停止相关报文
    TCU2REMOTE_VIN_CHARGE           = 0x1792,   //VIN码充电
    TCU2REMOTE_CARD_CHARGE          = 0x1793,   //刷卡充电
    TCU2REMOTE_ORDER_CHARGE_ACK     = 0x1794,   //预约充电
    TCU2REMOTE_CANCEL_ORDER_ACK     = 0x1795,   //取消预约
    TCU2REMOTE_ORDER_TIMEOUT        = 0x1796,   //预约超时
    TCU2REMOTE_LAND_LOCK_ACK        = 0x1797,   //下放地锁
    TCU2REMOTE_BILL_REPORT          = 0x1798,   //交易记录上送报文
    TCU2REMOTE_STOP_BILL_REPORT     = 0x1799,   //停车费上报
    TCU2REMOTE_CHECK_CARD           = 0x179A,   //刷卡充电鉴权
    //其他(0x17C0-)
    TCU2REMOTE_UPGRADE_REQ_ACK      = 0x17C0,   //遠程升級
    TCU2REMOTE_UPGRADE_RESULT       = 0x17C1,   //主站确认固件升级结果响应
    TCU2REMOTE_DOWNLOAD_UPDATE      = 0x17C2,   //下载升级包
    TCU2REMOTE_DOWNLOAD_REMOTE      = 0x17C3,   //通知remote下载固件
    TCU2REMOTE_REBOOT_REMOTE        = 0x17C4,   //升级远程
    TCU2REMOTE_GETLOG_ACK           = 0x17C6,   //上传回复
}REMOTE_CMD;

typedef enum
{
    PCU2TCU_Heart = 0x2201,
    TCU2PCU_Heart = 0x2202,
    TCU2PCU_TimeSync = 0x2101,
    PCU2TCU_TimeSync = 0x2102,
    TCU2PCU_WorkParamQuery = 0x2103,
    PCU2TCU_WorkParamQuery = 0x2104,
    PCU2TCU_DetailFaultInfo = 0x2105,
    PCU2TCU_AC_CONFIG = 0x2106,
    PCU2TCU_DC_CONFIG = 0x2108,
    PCU2TCU_CHARGE_DATA = 0x2300,
    PCU2TCU_POWER_ALLOC_DATA = 0x2301,
    PCU2TCU_MOD_DATA = 0x2302,
    PCU2TCU_AC_CONCACT_DATA = 0x2303,
    PCU2TCU_DC_CONCACT_DATA = 0x2304,
    PCU2TCU_SYS_DATA = 0x2305,
    PCU2TCU_AC_DATA = 0x2306,
} PCU_CMD;

typedef enum
{
    TCU2HMI_PCU_CHARGE_DATA = 0x1385,       //充电中数据
    TCU2HMI_PCU_POWER_ALLOC_DATA = 0x1386,  //功率分配数据
    TCU2HMI_PCU_MOD_DATA = 0x1387,          //模块数据
    TCU2HMI_PCU_DC_CONCACT_DATA = 0x1389,   //直流接触器状态
    TCU2HMI_PCU_AC_CONCACT_DATA = 0x1388,   //交流接触器状态
    TCU2HMI_PCU_SYS_DATA = 0x138A,          //系统数据
    TCU2HMI_PCU_AC_DATA = 0x138B,           //系统数据
    TCU2HMI_PCU_DC_CONCACT_CONFIG = 0x1584, //参数
    TCU2HMI_PCU_AC_CONCACT_CONFIG = 0x1583, //参数
} TCU2HMI_PCU_CMD;

typedef enum
{
    START_CHARGE_RIGHT_NOW = 1, //1：立即充电
    START_CHARGE_DELAY          //2：延时充电
} START_CHARGE_FLAG;

typedef enum
{
    SYS_STATE_NORMAL = 1, //1：正常
    SYS_STATE_FAULT,      //2：故障
    SYS_STATE_DISABLE     //3：禁用
} SYS_STATE;

enum
{
    BILL_JPRICE = 1, //尖峰
    BILL_FPRICE,     //高峰
    BILL_PPRICE,     //平段
    BILL_GPRICE      //低谷
} ;

#define MAX_COMM_DATA_SIZE 2048
#pragma pack(1)

/*
typedef struct
{
    int msg_valid_flag;                              //消息有效标记，0xAA55AA55-valid
    int msg_id;                                      //消息ID
    int msg_no;                                      //消息序号
    unsigned short selfCommVer;
    unsigned short srcCommVer;
    int msg_dest_dev_id;                             //消息设备地址
    int msg_data_byte_len;                           //消息数据长度
    unsigned char msg_data_sum_check;                //数据和校验
    unsigned char msg_data_arry[MAX_COMM_DATA_SIZE]; //消息
} SCommEventData;                                    //通讯事件数据

#define PLUGIN_MSG_SIZE(size) (sizeof(SCommEventData))
#define PLUGIN_MSG_HEAD_SIZE sizeof(SCommEventData)
*/

typedef struct
{
    unsigned int uiAddr;    //单元地址
    unsigned int result; //应答结果
} SGenerAck;

#pragma pack()
#endif
