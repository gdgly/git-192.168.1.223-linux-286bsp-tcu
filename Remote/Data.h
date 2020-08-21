
/*****************************************************************
 *Copyright:East Group Co.,Ltd
 *FileName:MsgData.h
 *Author:RM
 *Version:0.1
 *Date:2018-12-24
 *Description: 消息数据类，负责定义消息内容、格式、长度、操作方法。
 *
 *History:
 * 1、2018-12-24 create by rm
 * *************************************************************/

#ifndef DATA_H
#define DATA_H

//#include "EventTable.h"

#define ORDER_MAX_SIZE 64
#define USER_MAX_SIZE 32
#define VERSION_SIZE 20

enum
{
    CHARGE_BILL_ID_ORDER = 1,             //订单号
    CHARGE_BILL_ID_USER_TYPE,             //账户类型
    CHARGE_BILL_ID_USER,                  //卡号或者用户名
    CHARGE_BILL_ID_VIN,                   //车辆码
    CHARGE_BILL_ID_DEV_ID,                //枪号
    CHARGE_BILL_ID_CHARGE_TYPE,           //充电方式
    CHARGE_BILL_ID_BILL_STATE,            //交易状态
    CHARGE_BILL_ID_CHARGE_START_TIME,     //充电开始时间
    CHARGE_BILL_ID_CHARGE_END_TIME,       //充电结束时间或者当前时间
    CHARGE_BILL_ID_START_POWER,           //起始电量
    CHARGE_BILL_ID_CUR_POWER,             //当前电量
    CHARGE_BILL_ID_TOTAL_POWER,           //总电量
    CHARGE_BILL_ID_JPRICE,                //尖电价
    CHARGE_BILL_ID_JPOWER,                //尖时用电量
    CHARGE_BILL_ID_JCONSUME,              //尖时金额
    CHARGE_BILL_ID_FPRICE,                //峰电价
    CHARGE_BILL_ID_FPOWER,                //峰时用电量
    CHARGE_BILL_ID_FCONSUME,              //峰时金额
    CHARGE_BILL_ID_PPRICE,                //平电价
    CHARGE_BILL_ID_PPOWER,                //平时用电量
    CHARGE_BILL_ID_PCONSUME,              //平时金额
    CHARGE_BILL_ID_GPRICE,                //谷电价
    CHARGE_BILL_ID_GPOWER,                //谷时用电量
    CHARGE_BILL_ID_GCONSUME,              //谷时金额
    CHARGE_BILL_ID_START_BALANCE,         //充电前余额
    CHARGE_BILL_ID_END_BALANCE,           //扣费后余额
    CHARGE_BILL_ID_CUR_PRICE,             //消费电价
    CHARGE_BILL_ID_TOTAL_POWER_CONSUME,   //充电消费总金额
    CHARGE_BILL_ID_JSERVICE_PRICE,        //尖服务单价
    CHARGE_BILL_ID_JSERVICE_CONSUME,      //尖服务金额
    CHARGE_BILL_ID_FSERVICE_PRICE,        //峰服务单价
    CHARGE_BILL_ID_FSERVICE_CONSUME,      //峰服务金额
    CHARGE_BILL_ID_PSERVICE_PRICE,        //平服务单价
    CHARGE_BILL_ID_PSERVICE_CONSUME,      //平服务金额
    CHARGE_BILL_ID_GSERVICE_PRICE,        //谷服务单价
    CHARGE_BILL_ID_GSERVICE_CONSUME,      //谷服务金额
    CHARGE_BILL_ID_TOTAL_SERVICE_CONSUME, //服务消费总金额
    CHARGE_BILL_ID_APPOINT_PRICE,         //预约单价
    CHARGE_BILL_ID_APPOINT_CONSUME,       //预约金额
    CHARGE_BILL_ID_STOP_PRICE,            //停车费
    CHARGE_BILL_ID_STOP_CONSUME,          //停车金额
    CHARGE_BILL_ID_TOTAL_CONSUME,         //总消费金额
    CHARGE_BILL_ID_START_SOC,             //充电前SOC
    CHARGE_BILL_ID_END_SOC,               //充电后SOC
    CHARGE_BILL_ID_CHARGE_PRICE,          //充电单价
    CHARGE_BILL_ID_SERVICE_PRICE,         //服务费单价
    CHARGE_BILL_ID_STATE,                 //计费状态
    CHARGE_BILL_ID_CHARGE_TIME,           //充电时间
    CHARGE_BILL_ID_CHARING_DATA,          //正在充电数据
    CHARGE_BILL_ID_BILL_DATA,             //计费数据
    CHARGE_BILL_ID_VIN_SPECIAL,           //VIN特殊标记
    CHARGE_BILL_ID_MAX,
};

enum{
    OUTLINE = 0,
    ONLINE = 1
};

enum
{
    CCU_FAULT_ERROR = 1,
    CCU_WORKING,
    PARAM_ERROR,
    CCU_NO_FIND_ERROR,
    TIMEOUT_ERROR = 253,
    UNKNOW_ERROR = 254,
    ERROR_ID_MAX
};

enum
{
    ACK_TYPE_ACK,  //应答帧
    ACK_TYPE_DONE, //完成帧
};

enum
{
    START_CHARGE_ERROR_FAIL = 1,           //CCU正常但是启动失败，失败原因来自于启动帧
    START_CHARGE_ERROR_FAIL2 = 2,          //CCU正常但是启动失败，失败原因来自于启动完成帧
    START_CHARGE_ERROR_GUN_USING = 3,      //枪正在被占用
    START_CHARGE_ERROR_CCU_FAULT = 4,      //CCU故障
    START_CHARGE_ERROR_TIMEOUT = 5,        //启动超时
    START_CHARGE_ERROR_SERVICE_STOP = 6,   //CCU服务停止
    START_CHARGE_ERROT_TCU_FAULT = 7,      //TCU故障
    START_CHARGE_ERROT_GUN_DISCONNECT = 8, //枪未连接
    START_CHARGE_ERROT_NO_MONEY = 9,       //余额不足
    START_CHARGE_ERROT_PARALLELS_CFG = 10, //并机配置错误
    START_CHARGE_ERROR_ID_MAX
};

enum
{
    STOP_CHARGE_ERROR_FAIL = 1,            //CCU回复停止充电失败
    STOP_CHARGE_ERROR_TCU_FAULT = 2,       //TCU故障
    STOP_CHARGE_ERROR_BILL_STATTE_ERR = 3, //计费状态出错
    STOP_CHARGE_ERROR_USER_NO_MATCH = 4,   //用户不匹配
    STOP_CHARGE_ERROR_TIMEOUT = 5,         //超时
    STOP_CHARGE_ERROR_CCU_FAULT = 6,       //CCU故障
    STOP_CHARGE_ERROR_ID_MAX,
};

enum
{
    BOOK_GUN_ERROR_GUN_USING = 1, //枪被占用
    BOOK_GUN_ERROR_TCU_FAULT = 2, //TCU故障
    BOOK_GUN_ERROR_ID_MAX,
};

enum
{
    CANCEL_BOOK_GUN_ERROR_ORDER_NO_MATCH = 1, //订单号不匹配
    CANCEL_BOOK_GUN_ERROR_USER_NO_MATCH = 2,  //用户名不匹配
    CANCEL_BOOK_GUN_ERROR_TCU_FAULT = 3,      //TCU故障
    CANCEL_BOOK_GUN_ERROR_NO_USING = 4,       //不在预约中
    CANCEL_BOOK_GUN_ERROR_ID_MAX,
};

enum
{
    STOP_CHARGE_FLAG_CARD = 1, //刷卡停止
    STOP_CHARGE_FLAG_PWD = 2,  //停止码停止
};

enum
{
    TCU2HMI,
    HMI2TCU,
    TCU2REMOTE,
    REMOTE2TCU
};

typedef enum
{
    USER_TYPE_MANAGE, //0：管理卡
    USER_TYPE_STAFF,  //1：员工卡
    USER_TYPE_CUSTOM, //2：用户卡
    USER_TYPE_APP,    //3：APP账户
    USER_TYPE_VIN,    //4：VIN账户
    USER_TYPE_FINGER, //5：指纹用户
    USER_TYPE_FACE,   //6：刷脸用户
    USER_TYPE_FORCE,  //7、客服强制启动
    USER_TYPE_TEST,   //8、测试用户
    USER_TYPE_FREE,   //9、免费用户
    USER_TYPE_OTP,    //10、OTP用户
    USER_TYPE_ENTERPRISE,  //企业卡
    USER_TYPE_RESERVE //保留
} USER_TYPE;

typedef enum
{
    CHARGE_TYPE_AUTO = 1, //1：自动充满
    CHARGE_TYPE_TIME,     //2：按时间充电
    CHARGE_TYPE_CHARGE,   //3：按电量充电
    CHARGE_TYPE_MONEY     //4：按金额充电
} CHARGE_TYPE;

typedef enum
{
    USER_STATE_NORMAL = 0,             //0：正常允许允许充电
    USER_STATE_LOCKED,                 //1：账户锁定
    USER_STATE_BLACK,                  //2：黑名单
    USER_STATE_LOST,                   //3：账户挂失
    USER_STATE_CANCELED,               //4：已注销
    USER_STATE_NOT_REGISTER,           //5：未注册
    USER_STATE_RETRY,                  //6：重试
    USER_STATE_PWD_ERROR,              //7：密码错误
    USER_STATE_OUTLINE,                //8：离线
    USER_STATE_NO_MONEY,               //9：没钱
    USER_STATE_RESERVE,                //预留
    USER_STATEE_VERIFY_TIMEOUT = 0xff, //ff：验证超时
} USER_STATE;

typedef enum
{
    GUN_STATE_NO_CONNECT = 0, //0：未连接
    GUN_STATE_CONNECTED,      //1：已连接
    GUN_STATE_CHARGING,       //2：充电中
    GUN_STATE_PAUSE,          //3：车端充电暂停
    GUN_STATE_BOOKED,         //4：预约中
    GUN_STATEE_WAITING,       //5：等待中
    GUN_STATE_STOP,           //6：充电完成
    GUN_STATE_FAULT,          //7：故障
    GUN_STATE_DISABLE,        //8：禁用
    GUN_STATE_CHARGE_START,   //9：启动中
    GUN_STATE_CHARGE_STOP,    //10：停止中
    GUN_STATE_ACCOUNTING,     //11：结算中
    GUN_STATE_ACCOUNT_DONE,   //12：结算中
    GUN_STATE_CHARGE_COMPLETE //13：充电结束
} GUN_STATE;

enum
{
    SINGLE = 0,
    HOST = 1,
    SLAVE = 2,
};

#pragma pack(1)

typedef struct
{
    unsigned int id;             //用户ID
    unsigned int eventID;        //事件ID
    unsigned long long sendTime; //记录消息的发送时间，用于检测是否需要是否应答
    int vaildTime;
    int dstAddr;
    unsigned int resvere[8];
    unsigned int dataSize;
    unsigned char data[512];
} CmdAckData;

/****************下面是tcu系统自身使用的结构体*****************/

/***********************人机交互****************************/

//系统状态
typedef struct
{
    unsigned int uiAddr;       //单元地址
    unsigned char devName[16]; //设备名
    unsigned char res[20];     //预留
    unsigned char gunState;    //充电枪状态
    unsigned char lockState;   //枪故障状态
} T_SRealState;

typedef struct
{
    unsigned int addr;//从机地址
    unsigned char order[ORDER_MAX_SIZE];//从机订单号
}ChargeSlaveInfo;

typedef enum
{
    KWH_MODE = 1,//按电量
    TIME_MODE = 2,//按时间
    MONEY_MODE = 3,//按金额
    AUTO_MODE = 4,//自动充电
    DELAY_CHG_MODE = 5,//延迟启动
}eCharge_Mode;

//启动充电
typedef struct
{
    unsigned int uiAddr;           //单元地址,并机充电为主机地址
    unsigned char res[20];         //预留
    unsigned char userType;        //账户类型
    unsigned char user[USER_MAX_SIZE];        //账户号
    float balance;                 //账户余额
    unsigned char startFlag;       //启动标志
    unsigned char chargeType;      //充电方式
    float chargeValue;             //充电限定值
    float chargeVolt;              //设定电压
    float chargeCurr;              //设定电流
    unsigned char startTime[7];    //预约开始时间
    unsigned char endTime[7];      //预约开始时间
    unsigned char BmsVoltage;      //BMS辅助电压 01：12V电压，默认值；02：24V电压
    unsigned char loaclOrder[ORDER_MAX_SIZE];  //本地订单号
    unsigned char remoteOrder[ORDER_MAX_SIZE]; //远程订单号
    unsigned char stopPwd[16];     //停机控制码 字符串（数字），默认4位密码，多余四位补0 即：xxxx0000
    unsigned char vipLevel;        //用于运营特殊用户特权管理。数据越小优先级越高，VIP优先级=1,0XFF表示无效，其他值无效
    unsigned char vipSubLevel;     //用于运营区分不同VIP类型等级，1~254，数字越小优先级越高
    unsigned char recommendPower;  //无符号数，数据分辨率： 1kW /位，0kW偏移量。0XFFFF无效
    unsigned char sumOrder[ORDER_MAX_SIZE];    //并机汇总订单号
    unsigned char parallelsType;   //并机类型
    unsigned char slaveNum;        //从机数量
    ChargeSlaveInfo slaveInfo[4];  //从机信息
    unsigned char outlineChargeEnable;//断网可充标记
    float outlineChargeKwh;           //断网可充电量
    unsigned char isOutline;//是否离线
    unsigned char getBusyOrder; //是否获取订单
} T_SChargeData;


//启动充电应答
typedef struct
{
    unsigned char ackResult;       //应答结果
    unsigned int uiAddr;           //单元地址
    unsigned char remoteOrder[ORDER_MAX_SIZE]; //充电订单号
    unsigned char localOrder[ORDER_MAX_SIZE];  //本地账单号
    unsigned int failReason;       //失败原因
    unsigned char ackType;         //区分应答帧还是完成帧
    unsigned char parallelsType;   //并机类型
    unsigned char user[USER_MAX_SIZE];        //账户号
    unsigned char userType;        //账户类型
    unsigned char chargeType;      //充电方式
    float chargeValue;             //充电限定值
    unsigned char vin[17];
    unsigned char vinPwd[10];
    unsigned char getBusyOrder;
} T_SChargeDataAck;

//账户认证
typedef struct
{
    unsigned int uiAddr;    //单元地址
    unsigned char res[20];  //预留
    unsigned char user[USER_MAX_SIZE]; //账户号
    unsigned char userType;
    unsigned char pwd[16];
} T_SCheckCardData;

//账户认证应答
typedef struct
{
    unsigned int uiAddr;     //单元地址
    unsigned char res[20];   //预留
    unsigned char userType;  //账户类型
    unsigned char user[USER_MAX_SIZE];  //账户号
    float balance;           //账户余额
    unsigned char userState; //账户状态
    unsigned char vin[17];   //vin
    unsigned char pwd[16];   
}T_SCheckCardDataAck;

//结算数据
typedef struct
{

    unsigned int uiAddr;      //单元地址
    unsigned char res[20];    //预留
    unsigned char chargeType; //充电方式
    unsigned char user[USER_MAX_SIZE];   //充电账号
	unsigned char userType;     //充电方式
    unsigned int chargeTime;  //充电时间
    float charge;             //充电电量
    float consume;            //消费金额
    float charge_consume;     //充电金额
    float service_consume;    //服务费金额
    unsigned char soc;        //终止S0C
    unsigned int finalCase;   //结束原因
} T_SFinalData;

//充电中数据
typedef struct
{
    unsigned int iHmiUniqueId;
    unsigned int uiAddr;            //单元地址
    unsigned char res[20];          //预留
    unsigned char userType;         //账户类型
    unsigned char user[USER_MAX_SIZE];         //账户号
    float balance;                  //账户余额
    unsigned char passwd[16];       //强制停止密码
    unsigned char chargeState;      //充电状态
    unsigned char chargeType;       //充电方式
    float chargeVolt;               //充电电压
    float chargeCurr;               //充电电流
    unsigned int chargeTime;        //充电时间
    float charge;                   //充电电量
    float consume;                  //消费金额
    float chargeConsume;            //电量金额
    float serviceConsume;           //服务费金额
    unsigned char soc;              //当前电池剩余容量
    unsigned short remainTime;      //剩余时间
    float singHVolt;                //单体最高电压
    float HChargeVolt;              //最高充电电压
    float HChargeCurr;              //最高充电电流
    float needVolt;                 //需求电压
    float needCurr;                 //需求电流
    float bmsSinglePVolt;           //BMS单体保护电压
    short batteryTempH;             //电池最高温度
    short batteryTempL;             //电池最低温度
    short bmsSinglePTemp;           //BMS单体保护温度
    unsigned char bmsVersion[3];    //BMS协议版本
    unsigned short batteryRateCap;  //电池额定容量
    unsigned short batteryRateVolt; //电池额定电压
    unsigned short batteryRateKwh;  //电池额定电量
    unsigned char bmsSingleHVoltNo; //BMS单体最高电压序号
    unsigned char bmsSingleTempLNo; //BMS单体最低温度序号
    unsigned char bmsSingleTempHNo; //BMS单体最高温度序号
    short gunTemp;                  //充电枪温度
    unsigned char carVIN[17];       //车辆VIN码
    unsigned char batteryType;      //电池型号
    unsigned int hostId;      //主机号
} T_SChargingData;

typedef struct
{
    unsigned int uiAddr;        //单元地址
    unsigned char res[20];      //预留
    unsigned char startTime[7]; //预约开始时间
    unsigned char endTime[7];   //预约结束时间
    unsigned int finishSec;     //距离预约到期到期还需多长
    unsigned char userType;     //账号类型
    unsigned char user[USER_MAX_SIZE];     //账号
} T_SOrderChargeInfo;

typedef struct
{
    unsigned int uiAddr;           //单元地址
    unsigned char user[USER_MAX_SIZE];        //账号
    unsigned short bookTime;       //距离预约到期到期还需多长
    unsigned char remoteOrder[ORDER_MAX_SIZE]; //远程账单
} T_SBookGun;

typedef struct
{
    float VoltA;                //交流输入A相电压
    float VoltB;                //交流输入B相电压
    float VoltC;                //交流输入C相电压
    float CurrA;                //交流输入A相电流
    float CurrB;                //交流输入B相电流
    float CurrC;                //交流输入C相电流
    unsigned char switchState1; //交流输入开关状态
    unsigned char switchState2; //交流输入断路器状态
    unsigned char Hz;           //交流输入频率
    unsigned char Kwh;          //交流输入电量
} T_SAcInputData;


typedef struct
{
    double totalKwh;
    double jKwh;
    double fKwh;
    double pKwh;
    double gKwh;
}T_MeterInfo;

typedef struct
{
    unsigned char gunNum;         //充电枪总数
    T_MeterInfo info[16];
} T_SMeterData;

typedef struct
{
    unsigned char product[32];       //序列号
    unsigned char property[32];      //资产号
    unsigned char tcuVersion[3];    //计费单元版本信息
    unsigned char hmiVersion[3];    //人机交互版本信息
    unsigned char ccuVersion[3];    //CCU版本信息
    unsigned char pcuVersion[3];    //矩阵控制器版本信息
    unsigned char remoteVersion[3]; //通讯协议版本
} T_SSysVersionInfo;

typedef struct
{
    unsigned short startTime;  //时段开始时间
    unsigned char timeSegType; //时段类型
} T_SElecPriceSegation;

typedef struct
{
    unsigned int index;
    unsigned int faultId;       //告警序号
    unsigned char mainDevType;  //所属主设备类型
    unsigned char mainDevId;    //主设备ID
    unsigned char subDevType;   //所属子设备类型
    unsigned char subDevId;     //子设备ID
    unsigned char faultLevel;   //告警等级
    unsigned char startTime[7]; //开始时间
    unsigned char endTime[7];   //结束时间
    unsigned char faultStat;    //故障状态 01：表示发生； 02：表示结束
    unsigned char faultSource;  //故障源 01：整桩 02：枪
}T_SFaultInfo;

typedef struct
{
    unsigned int index;
    unsigned int eventId;       //告警序号
    unsigned char mainDevType;  //所属主设备类型
    unsigned char mainDevId;    //主设备ID
    unsigned char subDevType;   //所属子设备类型
    unsigned char subDevId;     //子设备ID
    unsigned char startTime[7]; //开始时间
    unsigned char eventSource;  //故障源 01：整桩 02：枪
} T_SEventInfo;

typedef struct
{
    unsigned int index;             //序号
    unsigned char chargeSerial[20]; //充电桩编号
    unsigned int gunNo;             //枪号
    unsigned char chargeType;       //账户类型
    unsigned char user[USER_MAX_SIZE];         //账户号
    float balance;                  //账户余额
    float totalConsume;             //消费总金额
    float charge;                   //充电电量
    unsigned char startTime[7];     //开始时间
    unsigned char endTime[7];       //结束时间
    unsigned int stopCase;          //停止原因
    unsigned char hasUpload;        //是否上传
} T_SQueryBillData;

typedef struct
{

    unsigned int uiAddr;   //单元地址
    unsigned char res[20]; //预留
    unsigned char obj;     //查询项
    unsigned int start;    //记录查询开始序号
    unsigned int end;      //记录查询结束序号
} T_SQueryHistoryData;

typedef struct
{
    unsigned int uiAddr;        //单元地址
    unsigned char res[20];      //预留
    unsigned char obj;          //查询对象
    unsigned char startTime[7]; //查询的时间起点
    unsigned char endTime[7];   //查询的时间终点
} T_SConditionQueryData;

typedef struct
{
    unsigned int uiAddr;        //单元地址
    unsigned char res[20];      //预留
    unsigned char obj;          //查询对象
    unsigned char startTime[7]; //查询的时间起点
    unsigned char endTime[7];   //查询的时间终点
    unsigned int total;         //查询总条数
} T_SConditionQueryDataAck;

typedef struct
{
    unsigned char alarmId;      //告警ID
    unsigned char alarmLevel;   //告警等级
    unsigned char startTime[7]; //发生时间
} T_SQueryFaultInfo;

typedef struct
{
    unsigned int alarmShield;
} T_SAlarmShieldConfig; //告警屏蔽设

typedef struct
{
    float JPrice;                    //尖电价
    float FPrice;                    //峰电价
    float PPrice;                    //平电价
    float GPrice;                    //谷电价
    unsigned char serverPriceEnable; //服务费使用
    float JServicePrice;             //尖服务费
    float FServicePrice;             //峰服务费
    float PServicePrice;             //平服务费
    float GServicePrice;             //谷服务费

} T_STcuBillCfg;

//类型价格  尖峰平谷
typedef struct
{
    unsigned char totalSeg;        //总时段数
    T_SElecPriceSegation data[48]; //价格
} T_STcuTimePrice;

typedef struct
{
    unsigned short startTime; //时段开始时间
    unsigned short endTime;   //时段开始时间
    float chargePrice;        //电价
    float servicePrice;       //服务费
} T_SRealPriceSegation;

//实际价格
typedef struct
{
    unsigned char totalSeg;         //总时段数
    T_SRealPriceSegation price[48]; //价格
} T_SRealPrice;

//获取充电资费信息应答
typedef struct
{
    unsigned int version;              //充电资费版本号
    unsigned char isShowUnitPrice;      //是否同意显示总单价：00：表示显示详细单价 01：表示只显示一个总单价
    unsigned char isHasNewPrice;        //是否有定时电价：00：表示没有，01：表示有。如果有定时电价，需要再次请求电价信息。
    unsigned char newPriceVaildTime[6]; //如果没有定时电价，那么给全0 格式：年-2000月日时分秒
    unsigned char isHasStopPrice;       //是否有停车费：00:表示没有停车费信息 01：表示有停车费信息
    unsigned char priceCount;           //费用阶段数
    T_SRealPriceSegation price[48];     //价格信息
} T_SRealPriceCfg;

typedef struct
{
    T_STcuBillCfg billCfg;
    T_STcuTimePrice timePrice;
} T_SElecPrice;

//停车费
typedef struct
{
    unsigned int version;       //版本
    float price;                 //价格
    unsigned char isFreeStop;    //是否免费
    unsigned short freeStopTime; //免费时长
} T_SStopPrice;

typedef struct
{
    unsigned char productId[32];    //产品编号
    unsigned char propertyId[32];    //产品编号
    unsigned char commId[32];    //后台通信
    unsigned char enablePlayAdv;  //是否播放广告
    unsigned char screenWaitTime; //屏蔽待机时间
    unsigned char systemMark;     //系统商标
    unsigned short parallelType;  //并机类型
    unsigned int groupAddr; //堆地址
    unsigned int tcuAddr;  //TCU本地地址
} T_STcuSysCfg;

typedef struct
{
    unsigned char dhcpSet;        //dhcp
    unsigned int ipAddr;          //ip
    unsigned int netmask;         //掩码
    unsigned int gateway;         //网关
    unsigned char mac[6];         //mac
    unsigned int dns1;
    unsigned int dns2;
} T_STcuNetCfg;


typedef struct
{
    unsigned char user[16];
    unsigned char passwd[16];
    unsigned int level;
} T_STcuUserCfg;

typedef struct
{
    unsigned int heapAddr;         //堆地址
    unsigned int addrList[16];     //本机柜阵地址列表
    unsigned int ipList[16];       //ip 列表
    unsigned short portList[16];   //端口列表
    unsigned short CNTRNoList[16]; //柜号列表
    unsigned int pcuNum;           //本机柜阵地址列表
    unsigned int commAddr[16];     //广播通讯列表
    unsigned int addr[16];     //广播通讯列表
} T_SPcuCommCfg;

typedef struct
{
    unsigned int addr;         //逻辑地址
    unsigned int commAddr;     //通讯地址
    unsigned char ccuName[16]; //别名
    unsigned char gunType;     //2:交流，1:直流
    unsigned char ccuVer[3];  //ccu版本
} T_SCcuInfo;

typedef struct
{
    unsigned int port;    //端口号
    unsigned char ccuNum; //ccu数量
    T_SCcuInfo ccuInfo[16];
} T_SCcuCommCfg;

typedef struct
{
    unsigned char enable;      //是否配置
    unsigned char portName[8]; //端口名
    unsigned short baund;      //波特率
    unsigned short dataBits;   //数据位
    unsigned short parity;     //校验位
    unsigned short porotol;    //电表协议
    //电表配置
} T_SMeterCommConfig;

typedef struct
{
    unsigned char operationUrl[128];       //运营域名/IP
    unsigned short operationPort;          //运营端口
    unsigned char operationManageUrl[128]; //运维域名/IP
    unsigned short operationManagePort;    //运维端口
    unsigned int dns1;                     //主DNS
    unsigned int dns2;                     //备用DNS
    //运营运维平台配置
} T_SOperatePlatformCfg;

typedef struct
{
    unsigned short uAcVoltOver;   // 交流输入过压值
    unsigned short uAcVoltLess;   // 交流输入欠压值
    unsigned short uAcVoltDelta;  // 交流输入过/欠回差值
    unsigned short uAcVoltPhLost; // 交流输入缺相值
    unsigned short uDcVoltOver;   // 直流输出过压值
    unsigned short uDcVoltLess;   // 直流输出欠压值
    unsigned short uDcVoltDelta;  // 直流输出过/欠回差值
    unsigned short uDcCurrDelta;  // 直流输出过流回差值
    unsigned short iChgTempOver;  // 充电机过温保护值
    unsigned short iChgTempAlarm; // 充电机过温告警值
    unsigned short uDcCurrOver;  // 充电机最大充电电流
    unsigned short uGunType;      // 枪的类型
    unsigned short iGunTempAlarm; // 充电枪过温告警值
    unsigned short iGunTempOver;  // 充电枪过温保护值
    unsigned char uModNum;        // 模块数量
    unsigned char uModType;       // 模块类型
    unsigned char uACEnable;      // 是否配置
    unsigned short uACCT;         // 电流CT比值
    unsigned char uLedEnable;     // 是否配置
    unsigned char uLedType;       // 灯板类型
    unsigned char uMeterEnable;   // 电能表是否配置
    unsigned char uMeterType;     // 电能表类型
    unsigned short sMeterBaund;   // 电能表波特率
    unsigned char sMeterDataBits; // 电能表数据位
    unsigned char sMeterParity;   // 电能表校验位
    unsigned short uDcCtRange;    // 分流器量程
    unsigned short uDcVoltRate;   // 直流额定电压值 0.1
    unsigned short uDcCurrRate;   // 直流额定电流值 0.1
    unsigned short uAcCtRange;   // 直流额定电压值 0.1
    unsigned short uDcMeterRange;   // 直流额定电流值 0.1
} T_SCcuCfg;

typedef struct
{
    unsigned char uMask[8];       // 功能屏蔽码
} T_SCcuMaskCfg;

typedef struct
{
    unsigned int uiAddr;           //单元地址
    unsigned char res[20];         //预留
    unsigned char user[USER_MAX_SIZE];        //账户号
    unsigned char stopFlag;        //停止标志
    unsigned char forcedStopFlag; //强制停止
    unsigned char remoteOrder[ORDER_MAX_SIZE]; //充电订单号
    unsigned char localOrder[ORDER_MAX_SIZE];  //充电标识号
    unsigned char isJudgeUser; //停止充电是否判断用户名
} T_SStopChargeData;

typedef struct
{
    unsigned char ackResult;       //应答结果
    unsigned int uiAddr;           //单元地址
    unsigned char user[USER_MAX_SIZE];        //账户号
    unsigned char remoteOrder[ORDER_MAX_SIZE]; //充电订单号
    unsigned char localOrder[ORDER_MAX_SIZE];  //充电标识号
    unsigned char stopReason;      //停止原因
    unsigned char ackType;         //区分应答帧还是完成帧
    unsigned char soc;
} T_SStopChargeDataAck;

typedef struct
{
    unsigned int uiAddr;           //单元地址
    unsigned char res[20];         //预留
    unsigned char userType;        //账户类型
    unsigned char user[USER_MAX_SIZE];        //预约账号
    unsigned char remoteOrder[ORDER_MAX_SIZE]; //订单号
} T_SOrderChargeCancel;

typedef struct
{
    unsigned char ackResult;       //应答结果
    unsigned int uiAddr;           //单元地址
    unsigned char user[USER_MAX_SIZE];        //预约账号
    unsigned char remoteOrder[ORDER_MAX_SIZE]; //订单号
} T_SOrderChargeCancelAck;

// 设备预约超时取消预约应答
typedef struct
{
    unsigned char ackResult;       //应答结果
    unsigned int uiAddr;           //单元地址
    unsigned char remoteOrder[ORDER_MAX_SIZE]; //订单号
} T_SChargeCancelOrderAck;



typedef struct
{
    unsigned int uiAddr; //单元地址
    unsigned char res[20];
    unsigned char user[USER_MAX_SIZE];        //预约账号
    unsigned char userType;         //用户类型
    unsigned int bookTime;         //预约时长
    unsigned char remoteOrder[ORDER_MAX_SIZE]; //远程账单
    unsigned long long bookStart;  //开始时间
    unsigned char startTime[7];  //开始时间
} T_SOrderChargeData;

typedef struct
{
    unsigned char ackResult;       //应答结果
    unsigned int uiAddr;           //单元地址
    unsigned char user[USER_MAX_SIZE];        //预约账号
    unsigned char remoteOrder[ORDER_MAX_SIZE]; //远程账单
} T_SOrderChargeDataAck;

typedef struct
{

    unsigned int uiAddr;        //单元地址
    unsigned char res[20];      //预留
    unsigned char startTime[7]; //禁用开始时间
    unsigned char endTime[7];   //禁用枪号
    unsigned char gunNo;        //禁用枪号
    unsigned char cmd;          //禁用指令
} T_SDisableChargeData;

typedef struct
{
    unsigned int uiAddr;   //单元地址
    unsigned char res[20]; //预留
    unsigned char cmd;     //控制命令
    unsigned short volt;   //绝缘电压
} T_SManualInsulationCheckData;

typedef struct
{
    unsigned int uiAddr;   //单元地址
    unsigned char res[20]; //预留
    unsigned char opt;     //操作
} T_SGunLock;

typedef struct
{
    unsigned int uiAddr;   //单元地址
    unsigned char opt;       // 操作指令 1-上锁 2-解锁
    unsigned char retsult;      // 操作结果反馈     1-操作成功 2-操作失败
    unsigned char failReason; // 失败原因
} T_SGunLockAck;

typedef struct
{
    unsigned int uiAddr;   //单元地址
    unsigned char res[20]; //预留
    unsigned char cmd;     //操作
    unsigned char rightNow;
} T_SChargeService;

typedef struct
{
    unsigned char ackResult;  //应答结果
    unsigned int uiAddr;      //单元地址
    unsigned char cmd;        //操作
    unsigned char failReason; //失败原因
} T_SChargeServiceAck;

typedef struct
{
    unsigned int uiAddr;   //单元地址
    unsigned char res[20]; //预留
    unsigned char type;    //类型
    float value;  //有效值
} T_SPowerCtrl;

typedef struct
{
    unsigned char ackResult;  //应答结果
    unsigned int uiAddr;      //单元地址
    unsigned char failReason; //失败原因
} T_SPowerCtrlAck;

typedef struct
{
    unsigned int uiAddr;   //单元地址
    unsigned char res[20]; //预留
    unsigned int volt;     //操作
    unsigned int curr;     //操作
} T_SManualStartCharge;

typedef struct
{
    unsigned char ackResult;  //应答结果
    unsigned int uiAddr;   //单元地址
} T_SManualStartChargeAck;

typedef struct
{
    unsigned int uiAddr;   //单元地址
    unsigned char res[20]; //预留
} T_SManualStopCharge;

typedef struct
{
    unsigned int uiAddr;     //单元地址
    unsigned char res[20];   //预留
    unsigned char OptSwitch; //
    unsigned char OptCode;   //
} T_SDebugCharge;

typedef struct
{
    unsigned int faultId;      //告警ID
    unsigned char mainDevType; //所属主设备类型
    unsigned char mainDevId;   //主设备ID
    unsigned char subDevType;  //所属子设备类型
    unsigned char subDevId;    //子设备ID
    unsigned char faultLevel;  //告警等级
    unsigned char startTime[7];
} T_SRealAlarm;

typedef struct
{
    unsigned char ackResult;       //应答结果
    unsigned int uiAddr;           //单元地址
    unsigned char remoteOrder[ORDER_MAX_SIZE]; //远程订单
    unsigned char userType;        //用户类型
} T_SBillComfirmAck;

//VIN启动充电
typedef struct
{
    unsigned int uiAddr;          //枪编号：01，表示1号枪
    unsigned char vin[17];        //用户标识
    unsigned char vinPwd[3];      //VIN密码
    unsigned char chargeType;     //01：电量控制充电；
                                  //02：时间控制充电；
                                  //03：金额控制充电；
                                  //04：自动充满
                                  //05：定时启动充电
    float chargeValue;            //类型有效值
    unsigned char localOrder[ORDER_MAX_SIZE]; //本地账单号
    unsigned char parallelsType;   //并机类型
} T_SVinCharge;

//VIN启动充电应答
typedef struct
{
    unsigned char ackResult;       //结果
    unsigned int uiAddr;           //枪编号：01，表示1号枪
    unsigned char remoteOrder[ORDER_MAX_SIZE]; //充电订单号
    float balance;                 //账户余额
    unsigned char user[USER_MAX_SIZE];   //用户标识
} T_SVinChargeAck;

//充电鉴权
typedef struct
{
    unsigned int uiAddr;      //枪编号：01，表示1号枪
    unsigned char user[USER_MAX_SIZE];   //用户标识
    unsigned char chargeType; //01：电量控制充电；
                              //02：时间控制充电；
                              //03：金额控制充电；
                              //04：自动充满
                              //05：定时启动充电
    float chargeValue;
    unsigned char localOrder[ORDER_MAX_SIZE]; //本地账单号
} T_SCardCharge;

//充电鉴权应答
typedef struct
{
    unsigned int uiAddr;           //枪编号：01，表示1号枪
    unsigned char ackResult;       //结果
    unsigned char remoteOrder[ORDER_MAX_SIZE]; //充电订单号
} T_SCardChargeAck;

//下放地锁
typedef struct
{
    unsigned char lockNo;          //地锁编号：01，表示1号
    unsigned char user[USER_MAX_SIZE];        //用户标识
    unsigned char remoteOrder[ORDER_MAX_SIZE]; //充电订单号
} T_SGroundLock;

//下放地锁应答
typedef struct
{
    unsigned char lockNo;          //地锁编号：01，表示1号
    unsigned char ackResult; //00—确认
                             //10—设备被占用
                             //22—无地锁
                             //12—设备故障
} T_SGroundLockAck;

typedef struct
{
    unsigned char commVersion[3];   //通信协议版本  格式为：XX.XX.XX
    unsigned char ctrlVersion[3];   //控制版本        格式为：XX.XX.XX
    unsigned char showVersion[3];   //显示版本号   格式为：XX.XX.XX
    unsigned char kernelVersion[3]; //内核版本号   格式为：XX.XX.XX
    unsigned char swVersion[3];     //软件版本号   格式为：XX.XX.XX
    unsigned char cpuId[64];        //CPUID
    unsigned char poductId[32];     //产品序列号
    unsigned char poductSubId[32];  //资产号
} T_SVersionInfo;

typedef struct
{
    unsigned int uiAddr;    //单元地址
    unsigned char res[20];  //预留
    unsigned char user[USER_MAX_SIZE]; //账户号
} T_SGetBillData;

typedef struct
{
    unsigned char result;           //查询结果 0 成功 1 失败
    unsigned char chargeSerial[20]; //充电桩编号
    unsigned int gunNo;             //枪号
    unsigned char userType;         //账户类型
    unsigned char user[USER_MAX_SIZE];         //账户号
    float balance;                  //消费前余额
    float totalConsume;             //消费总金额
    float charge;                  //充电电量
    unsigned char startTime[7];     //开始时间
    unsigned char endTime[7];       //结束时间
    unsigned int stopCase;          //停止原因
    unsigned char billStat;         //订单状态 0 已经结算 1 未结算
} T_SGetBillDataACK;

typedef struct
{
    unsigned int uiAddr;    //单元地址
    unsigned char res[20];  //账户号
    unsigned char user[USER_MAX_SIZE]; //账户号
} T_SGetQRcode;

typedef struct
{
    unsigned int uiAddr;       //单元地址
    unsigned short qrcodeSize; //长度 数据存在表里面，不存在程序内存
} T_SQRcode;

//心跳固定20个字节
typedef struct
{
    unsigned char cardReaderState; //读卡器状态
    unsigned char pcuDataEnable;   //PCU数据使能
    unsigned char res[18];         //预留
} T_SHmiHeartBeat;

//心跳固定20个字节
typedef struct
{
    unsigned char sysState;          //系统状态
    unsigned char remoteState;       //远程状态
    unsigned char updateState;        //升级状态
    unsigned char usbState;          //usb插入状态
    unsigned char res[16];           //预留
} T_STcuHeartBeat;

typedef struct
{
    unsigned char localOrder[ORDER_MAX_SIZE]; //账单号
} T_UnlockCardConfirm;

typedef struct
{
    unsigned char result;         //解卡应答结果
    unsigned char localOrder[ORDER_MAX_SIZE]; //账单号
} T_UnlockCardConfirmAck;

typedef struct
{
    unsigned int uiAddr;       //单元地址
    unsigned char res[20];     //预留
    unsigned short qrcodeSize; //长度
} T_SGetQRcodeACK;

typedef struct
{
    unsigned int uiAddr;   //单元地址
    unsigned char res[20]; //预留
    unsigned char result;  //结算结果
} T_SFinalDataAck;

typedef struct
{
    unsigned int uiAddr;   //单元地址
    unsigned char res[20]; //预留
    unsigned int Num;      //数量
} T_UdpAlarmHead;

typedef enum
{
    eBillState_Counting,      //计算中
    eBillState_Finish_Fail,   //账单结算失败
    eBillState_Finish_Success //账单结算成功
} eBillState;

typedef struct T_SChargeConsume
{
    unsigned char startTime[6]; //起始时间
    float charge;               //电量
    float chargeCosume;         //电量消费
    float serviceConsume;       //服务费
    unsigned char priceIndex;   //用于记录拿一段的价格
} T_SChargeConsume;

typedef struct
{
    unsigned short ms;
    unsigned char minute;
    unsigned char hour;
    unsigned char date : 5;
    unsigned char week : 3;
    unsigned char month;
    unsigned char year;
} Time7Bits;

typedef struct
{
    unsigned char year;
    unsigned char month;
    unsigned char date;
    unsigned char hour;
    unsigned char minute;
    unsigned char sec;
} Time6Bits;

typedef struct
{
    char order[ORDER_MAX_SIZE];        //订单号
    unsigned char userType;         //账户类型
    char user[USER_MAX_SIZE];         //卡号或者用户名
    char vin[17];                      //车辆VIN码
    unsigned int uiAddr;            //枪号
    unsigned char chargeType;       //充电方式
    unsigned char startFlag;        //启动标志
    float chargeValue;              //充电限定值
    Time6Bits bookStartTime;        //预约开始时间
    Time6Bits bookEndTime;          //预约结束时间
    eBillState billState;           //交易状态
    Time6Bits chargeStart;          //充电开始时间
    Time6Bits chargeEnd;            //充电结束时间或者当前时间
    double startChargePower;        //起始电量
    double currChargePower;         //当前电量
    float totalPower;               //总电量
    float jPrice;                   //尖电价
    float jPower;                   //尖时用电量
    float jConsume;                 //尖时金额
    float fPrice;                   //峰电价
    float fPower;                   //峰时用电量
    float fConsume;                 //峰时金额
    float pPrice;                   //平电价
    float pPower;                   //平时用电量
    float pConsume;                 //平时金额
    float gPrice;                   //谷电价
    float gPower;                   //谷时用电量
    float gConsume;                 //谷时金额
    float startBalance;             //充电前余额
    float endBalance;               //扣费后余额
    float curPrice;                 //消费电价
    float curChargePrice;           //充电电价
    float curServicePrice;          //服务费电价
    float totalChargeConsume;       //充电消费总金额
    float jServicePrice;            //尖服务单价
    float jServiceConsume;          //尖服务金额
    float fServicePrice;            //峰服务单价
    float fServiceConsume;          //峰服务金额
    float pServicePrice;            //平服务单价
    float pServiceConsume;          //平服务金额
    float gServicePrice;            //谷服务单价
    float gServiceConsume;          //谷服务金额
    float totalServiceConsume;      //服务消费总金额
    float appointPrice;             //预约单价
    float appointConsume;           //预约金额
    float stopPrice;                //停车费
    float stopConsume;              //停车金额
    float totalConsume;             //总消费金额
    unsigned char startSoc;         //充电前soc
    unsigned char endSoc;           //充电后soc
    unsigned int stopCase;          //停止原因
    unsigned int chargeTime;        //充电时间
    unsigned short remainTime;      //剩余时间
    unsigned char isUpload;         //上传标记
    char remoteOrder[ORDER_MAX_SIZE];  //远程订单号
    unsigned char chargePriceCount; //费用阶段数
    T_SChargeConsume chargeCosume[48];//区间电费信息
    unsigned char isParallels;//是否并机
    unsigned char isParallelsOrder;//是否并机汇总账单
    unsigned char isHost;//是否是主枪
    float slaveCharge; //从机电量
    float slaveChrConsume;//从机充电消费
    float slaveSrvConsume;//从机服务费
    unsigned int priceVersion;//计费版本
    unsigned char isOutline;//是否离线
} T_SBillData;

typedef struct
{
    unsigned char loginStat;
    unsigned char ver[3];
    unsigned short selfCommVer;
    unsigned short srcCommVer;
} T_SRemoteHeart;

typedef struct
{
    unsigned char ackResult; //00—确认通过 08—故障码不存在
    unsigned int uniqueId;
} T_SFaultInfoAck;

//充电桩时钟调控
typedef struct
{
    unsigned char time[6]; //设置时间 格式：年-2000月日时分秒
} T_STimeSet;

typedef struct
{
    unsigned short startTime; //起始时间：格式：时:分 00:00
    unsigned short endTime;   //截止时间：格式：时:分 00:00
    float chargePrice;        //电费单价：单位：元/度 精度：0.0001
    float servicePrice;       //服务费单价：单位：元/度 精度：0.01
} T_SChargePrice;

//设置充电资费信息
typedef struct
{
    unsigned int version;              //充电资费版本号
    unsigned char isShowUnitPrice;      //是否同意显示总单价：00：表示显示详细单价 01：表示只显示一个总单价
    unsigned char isHasNewPrice;        //是否有定时电价：00：表示没有，01：表示有。如果有定时电价，需要再次请求电价信息。
    unsigned char newPriceVaildTime[6]; //如果没有定时电价，那么给全0 格式：年-2000月日时分秒
    unsigned char isHasStopPrice;       //是否有停车费：00:表示没有停车费信息 01：表示有停车费信息
    unsigned char priceCount;           //费用阶段数
    T_SChargePrice price[48];           //价格信息
} T_SSetChargePrice;

//设置充电资费信息
typedef struct
{
    unsigned char ackResult; //00—确认，立即生效 24—充电中,延时生效
} T_SSetChargePriceAck;

//设置停车资费信息
typedef struct
{
    unsigned int version;       //停车费版本号
    float price;                 //停车费单价 单位：元/10分钟 精度：0.0001
    unsigned char isFreeStop;    //充电期间是否免停车费 01：表示免费，02：不免费
    unsigned short freeStopTime; //充电前后免停车费的时长 单位：分钟
} T_SSetStopPrice;

//设置停车资费信息应答
typedef struct
{
    unsigned char ackResult; //00—确认，立即生效 24—充电中,延时生效
} T_SSetStopPriceAck;

//充电桩重启控制
typedef struct
{
    unsigned char cmd; //重启指令 01：重启
    unsigned char type;//0 全部重启
    unsigned int devId;
    unsigned char rightNow;
} T_SReset;

//充电桩重启控制应答
typedef struct
{
    unsigned char ackResult; //00—确认，立即生效 27—确认，延时订单结束后重启 28—无法重启
} T_SResetAck;

//充电桩二维码信息更新
typedef struct
{
    // unsigned int uiAddr;
    // unsigned int qrcodeLen;       //二维码信息长度
    // unsigned char qrcodeData[512]; //二维码信息内容

    unsigned char   gunNo;     // 0-桩，1~255表示枪号       
    unsigned short  qrLen;     // 二维码长度
    unsigned char   qrCode[256];    // 充电桩在该内容后面补充设备序列号（14位）+TCU编号（2位）+枪编号（2位）
    unsigned char   isWhole    // 是否拼接    0-需要拼接   1-不需要拼接
} T_SQrcodeUpdate;

//充电桩二维码信息更新应答
typedef struct
{
    unsigned char ackResult; //00—确认
} T_SQrcodeUpdateAck;

//充电账单上送应答
typedef struct
{
    unsigned char ackResult;       //00—确认
                                   //17—平台订单号已存在
                                   //18—设备订单号已存在
                                   //19—订单处理中
                                   //20—用户卡未注册
    unsigned int uiAddr;           //枪编号：01，表示1号枪
    unsigned char localOrder[ORDER_MAX_SIZE];  //本地账单
    unsigned char remoteOrder[ORDER_MAX_SIZE]; //订单号 对于卡的离线账单，该字段允许为全0
} T_SUploadBillAck;

//获取充电资费信息应答
typedef struct
{
    unsigned char ackResult;            //00—确认下发
    unsigned char version;              //充电资费版本号
    unsigned char isShowUnitPrice;      //是否同意显示总单价：00：表示显示详细单价 01：表示只显示一个总单价
    unsigned char isHasNewPrice;        //是否有定时电价：00：表示没有，01：表示有。如果有定时电价，需要再次请求电价信息。
    unsigned char newPriceVaildTime[6]; //如果没有定时电价，那么给全0 格式：年-2000月日时分秒
    unsigned char isHasStopPrice;       //是否有停车费：00:表示没有停车费信息 01：表示有停车费信息
    unsigned char priceCount;           //费用阶段数
    T_SChargePrice price[48];           //价格信息
} T_SGetChargePriceAck;

//获取停车资费信息应答
typedef struct
{
    unsigned char ackResult;     //00—确认下发
    unsigned char version;       //停车费版本号
    unsigned int price;          //停车费单价 单位：元/10分钟 精度：0.01
    unsigned char isFreeStop;    //充电期间是否免停车费 01：表示免费，02：不免费
    unsigned short freeStopTime; //充电前后免停车费的时长 单位：分钟
} T_SGetStopPriceAck;

//定时遥脉信息
typedef struct
{
    unsigned int uiAddr;       //枪编号：01，表示1号枪
    float charge;              //已充电量
    float chargeConsume;       //已充电的电费
    float serviceConsume;      //已充电的服务费
    unsigned char soc;         //电池SOC
    unsigned char chargeHour;  //已充电时长 时
    unsigned char chargeMin;   //已充电时长 分
    unsigned char chargeSec;   //已充电时长 秒
    unsigned short remainTime; //剩余充电时长 单位：分钟 交流设备默认为0
    unsigned char rightNow;    //是否立即发送
    double startChargePower;
} T_SYaoMai;

//BMS充电实时数据
typedef struct
{
    unsigned int uiAddr;                     //枪编号：01，表示1号枪
    float voltageNeed;                       //充电需求电压值
    float currentNeed;                       //充电需求电流值
    float voltageReal;                       //充电电压测量值
    float currentReal;                       //充电电流测量值
    float singleBatChargeVoltMax;            //单体电池最高电压
    unsigned char singleBatChargeVoltMaxId;  //单体电池最高电压序号
    unsigned char soc;                       //电池SOC
    int singleBatteryTempHigh;               //单体电池最高温度
    unsigned char singleBatteryTempHighId;   //单体电池最高温度序号
    int singleBatteryTempLess;               //单体电池最低温度
    unsigned char singleBatteryTempLessId;   //单体电池最低温度序号
    int gunTemp;                             //充电枪温度
    char vin[18];                            //VIN码 VIN码若无法获取默认全为0
    unsigned char batType;                   //电池类型
    float batGroupRatedVoltage;              //电池组额定总电压
    float batChargeVoltageMax;               //最高允许充电电压
    float batChargeCurrentMax;               //最高允许充电电流
    float singlBatAllowVoltageMax;           //单体电池允许最高充电电压
    float batGroupRatedPower;                //蓄电池组额定容量
    float batRatedPower;                     //电池额定电量
    float batTempMax;                        //电池最高允许温度
    unsigned char localOrder[ORDER_MAX_SIZE];            //本地账单号
    float gunPosTemp;                        //枪正极温度
    float gunNegTemp;                        //枪负极温度
    unsigned char chargerBmsVer[3];          //充电桩与BMS通信协议版本号
    unsigned char carBmsVer[3];              //BMS与充电桩通信协议版本号
    unsigned char rightNow;                  //是否立即发送
} T_SBmsData;

//心跳枪信息
typedef struct
{
    unsigned char gunNo;          //枪编号：01，表示1号枪
    unsigned char gunConnectStat; //充电枪车辆连接状态：1-在线 2-离线
    unsigned char gunStat;        //充电枪状态
} T_SHeartBeatGunInfo;

//心跳枪信息
typedef struct
{
    unsigned char sysStat;
    unsigned char gunNum;                     //枪编号：01，表示1号枪
    T_SHeartBeatGunInfo gunInfo[16]; //枪信息
    unsigned char rightNow;                   //是否立即发送
} T_STcu2RemoteHeartBeat;

//定时遥测信息
typedef struct
{
    unsigned int uiAddr;                //枪编号：01，表示1号枪
    float Avoltage;             //A相电压
    float Bvoltage;            //B相电压
    float Cvoltage;              //C相电压
    float Acurrent;             //A相电流
    float Bcurrent;             //B相电流
    float Ccurrent;             //C相电流
    float outputPower;          //输出功率
    float gunTotal;             //充电枪有功总电量
    double meterTotal;          //充电位计量表底度
    float outputInsideVoltage;  //充电输出内侧电压
    float outputOutsideVoltage; //充电输出外侧电压
    float outputCurrent;        //充电输出电流
    float temperature;          //充电机环境温度
    float guideVoltage;         //充电导引电压
    float availablePower;       //可用功率
    float gainPower;            //获得功率
    float outputVolt;           //充电输出电压
    unsigned char rightNow;     //是否立即发送
} T_SYaoCE;

//充电中定时遥脉信息
typedef struct
{
    unsigned int uiAddr;             //枪编号：01，表示1号枪
    unsigned char gunInsert;         //插枪状态 00：未连接 01：已连接
    unsigned char gunWorkStat;       //工作状态
    float ouputVaildPower;    //输出有功功率kw
    float ouputInvaildPower;  //输出无功功率kw
    unsigned char sysFault;          //系统故障状态  0：正常 1：故障
    unsigned char chargeModFault;    //充电模块故障状态 0：正常 1：故障
    unsigned char chargeSysFault;    //充电系统故障状态 0：正常 1：故障
    unsigned short batteryFault;     //电池系统故障状态 0：正常 1：故障
    unsigned char dcOutputSwitch;    //直流输出开关状态 0：断开 1：闭合，其他：无效
    unsigned char dcOutputContactor; //直流输出接触器状态 0：断开 1：闭合，其他：无效
    unsigned char gunDoor;           //枪门状态 0：断开 1：闭合，其他：无效
    unsigned char gunBack;           //枪归位状态 0：归位 1：未归位，其他：无效
    unsigned char catportStat;       //车位状态 0：不支持状态监测1：空闲，2：占用其他：无效
    unsigned char rightNow;          //是否立即发送
} T_SYaoXin;

//故障信息上报
typedef struct
{
    unsigned int uniqueId;      //用于应答时候确认哪个故障
    unsigned char gunNo;        //枪编号：01，表示1号枪
    unsigned char faultTime[6]; //故障时间 故障时间 格式：年-2000 月日时分秒
    unsigned int faultId;       //故障编号
    unsigned char faultStat;    //故障状态 01：表示发生； 02：表示结束
    unsigned char faultSource;  //故障源 01：整桩 02：枪
    unsigned char faultLevel;   //故障等级
} T_STcu2RemoteFaultInfo;

//充电桩时钟调控应答
typedef struct
{
    unsigned char ackResult; //00—确认，立即生效 23—延时生效
} T_STimeSetAck;

//设置充电桩消息间隔
typedef struct
{
    unsigned short standyMsgTime; //待机消息间隔 单位：秒 默认：60
    unsigned short chargeMsgTime; //工作消息间隔 单位：秒 默认：10
} T_SSetMsgCycle;

//设置充电桩消息间隔应答
typedef struct
{
    unsigned char ackResult; //00—确认
} T_SSetMsgCycleAck;

//主站确认登录枪信息
typedef struct
{
    unsigned char gunNo;         //枪编号：格式：01，表示1号枪
    unsigned char gunType;       //枪类型：01：交流 02：直流
    float power; //枪当前功率 单位：kW
    unsigned char gunVersion[3]; //版本号
} T_SSysLoginGunInfo;

//主站确认登录
typedef struct
{
    unsigned char operationUrl[128];       //运营域名/IP
    unsigned short operationPort;          //运营端口
    unsigned char operationManageUrl[128]; //运维域名/IP
    unsigned short operationManagePort;    //运维端口
    unsigned char commVersion[VERSION_SIZE];          //通信协议版本 格式为：XX.XX.XX
    unsigned char pcuVersion[3];          //PCU版本    格式为：XX.XX.XX
    unsigned char hmiVersion[3];          //HMI版本号  格式为：XX.XX.XX
    unsigned char remoteVersion[3];        //REMOTE版本号  格式为：XX.XX.XX
    unsigned char tcuVersion[3];            //TCU版本号  格式为：XX.XX.XX
    unsigned char cpuId[64];               //CPUId
    unsigned char poductId[32];            //通道(TCU)出厂序列号
    unsigned char property[32];            //通道(TCU)运营商资产编码
    float pilePower;                       //桩功率 单位：kW
    unsigned char mqttUser[USER_MAX_SIZE];       //mqtt账号
    unsigned char mqttPwd[USER_MAX_SIZE];       //mqtt密码
    unsigned char gunNum;                  //枪数量
    T_SSysLoginGunInfo gunInfo[16];     //枪信息
} T_SSysLogin;

//并机配置
typedef struct{
    unsigned int host;
    unsigned char slaveNum;
    unsigned int slave[8];
}T_SParallelsInfo;

typedef struct{
    unsigned char num;
    T_SParallelsInfo info[16];
}T_SParallelsCfg;

//监控数据
typedef struct
{
    unsigned int uiAddr;//测量点地址
    unsigned char res[20];//预留
    float bmsNeedVol;//BMS需求电压
    float bmsNeedCurr;//BMS需求电流
    float bmsVol;//BMS测量电压
    float bmsCurr;//BMS测量电流
    float outputVol;//充电输出电压
    float outputCurr;//充电输出电流
    float outsideVol;//充电机外侧电压
    float cc1Vol;//控制导引电压（CC1）
    float bmsSupportVol;//BMS辅助电源电压
    float busPosResistance;//母线正对地电阻
    float busNegResistance;//母线负对地电阻
    float gunPosTemp;//充电枪正极温度
    float gunNegTemp;//充电负极极温度
    float envTemp;//充电柜环境温度
    float ccuTemp;//充电终端环境温度
    float modMaxTemp;//模块内部最高温度
    short modMaxTempId;//模块内部最高温度地址
}T_MonitorData;

typedef struct{
    unsigned char result;
}T_GenernalAck;

#define FIRMWARE_LIST_MAX 24

enum{
    USB_STORE_FIRMWARE = 1,
    SELF_STORE_FIRMWARE,
};

enum{
    UPDATE_SINGLE,
    UPDATE_ALL,
};

typedef struct{
    unsigned char listTpe; //列表类型
}T_GetFirmwareList;

typedef struct{
    unsigned char devType;//设备类型
    unsigned int devId;//设备ID
    unsigned char curVersion[3];//当前版本
    unsigned char newVersion[3];//新版本
    unsigned char versionStatus;//固件状态
}T_FirmwareInfo;

typedef struct{
    unsigned char listTpe;//列表类型
    unsigned char count;//固件数量
    T_FirmwareInfo info[FIRMWARE_LIST_MAX];
}T_FirmwareInfoList;

typedef struct{
    unsigned char version[3];//版本
    char md5[64];//MD5
    char url[256];//固件地址
}T_FirmwareDownloadInfo;

typedef struct{
    unsigned char listTpe;///设备类型
    unsigned char updateAll;//是否全部
    unsigned char autoUpdate;//自动更新
    unsigned char devType;//设备类型
    unsigned int devId;//设备ID
}T_ChooseFirmware;

typedef struct{
    unsigned char result;//应答
}T_ChooseFirmwareAck;

typedef struct{
    unsigned int devId;
    unsigned char result;//下载固件结果
}T_FirmwareDownloadInfoAck;

typedef struct{
    unsigned char cmd;
}T_CustomCmd;

typedef struct{
    unsigned char addr;
    unsigned char hmiVer[3];
}T_HmiLogin;

typedef struct{
    unsigned char result;
}T_HmiLoginAck;

typedef struct{
    unsigned char devType;//设备类型
    unsigned int devId;//设备ID
    unsigned int result;
}T_UpdateFirmwareResult;

//远程CCU配置
typedef struct{
    T_SCcuCfg cfg;
    T_SCcuMaskCfg mask;
}T_ReomteCcuCfg;

// sienbo 与协议不匹配
//远程系统配置
typedef struct{
    T_SCcuCfg cfg;
    T_SCcuMaskCfg mask;
}T_ReomteSysCfg;

//远程其他配置
typedef struct{
    T_SCcuCfg cfg;
    T_SCcuMaskCfg mask;
}T_ReomteOtherCfg;

//远程获取配置
typedef struct{
    unsigned int devId;
    unsigned char settingType;
    int count;
    char key[8][32];
}T_ReomteGetSettings;

//远程获取配置应答
typedef struct{
    unsigned char result;
    unsigned int devId;
    unsigned char settingType;
    int count;
    char key[8][32];
    char keyValue[8][128];
}T_ReomteGetSettingsAck;

//远程设置配置
typedef struct{
    unsigned int devId;
    unsigned char settingType;
    int count;
    char key[8][32];
    char keyValue[8][128];
    unsigned char uiType;
}T_ReomteSetSettings;

//远程设置配置应答
typedef struct{
    unsigned char result;
    unsigned int devId;
    unsigned char settingType;
}T_ReomteSetSettingsAck;

//固件升级应答
typedef struct
{
    unsigned char result;        //00—确认，立即升级 23—延迟处理
                                    //01—   失败，用户名密码错误
                                    //02—   失败，sftp地址错误
                                    //03—   失败，文件下载失败
                                    //04—   失败，文件格式错误
                                    //05—   失败，其他
    unsigned char commVersion[3];   //通信协议版本    格式为：XX.XX.XX
    unsigned char pcuVersion[3];          //PCU版本    格式为：XX.XX.XX
    unsigned char hmiVersion[3];          //HMI版本号  格式为：XX.XX.XX
    unsigned char remoteVersion[3];        //REMOTE版本号  格式为：XX.XX.XX
    unsigned char tcuVersion[3];            //TCU版本号  格式为：XX.XX.XX
} T_RemoteUpgradeAck;

//固件升级应答
typedef struct
{
    unsigned char result;        //00—确认，立即升级 23—延迟处理
} T_RemoteUpgradeDownload;

//远程配置
typedef struct{
    unsigned int devId;
    unsigned char settingType;
    char value[128];
    unsigned int uniqueId;
}T_Hmi2RemoteSetSettings;

//远程配置应答
typedef struct{
    unsigned char result;
    unsigned int devId;
    unsigned char settingType;
    unsigned int uniqueId;
}T_Hmi2RemoteSetSettingsAck;

typedef struct
{
    unsigned char socProtect;
    unsigned short socTime;
} T_SSocCfg;

typedef struct
{
    unsigned char addr[32];
    unsigned short ct;
    unsigned enable;
} T_SMeterInfo;

typedef struct
{
    int num;
    T_SMeterInfo meter[16];
} T_SMeterCfg;

#pragma pack()
#endif // DATA_H


