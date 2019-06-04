#ifndef __CAR_LOCK_H__
#define __CAR_LOCK_H__

#ifdef __cplusplus
 extern "C" {
#endif
#define  MAX_CARLOCK_NUM    2 //最大2个车位锁
#define  NEW_PROTOCOL     //新的协议
#ifdef NEW_PROTOCOL
typedef enum
	{
		GET_FIRMWARE_VER    = 0x62,//软件版本号查询：   CMD  0X62
		GET_Equipment_ID    = 0X1D,//查询设备ID CMD 0x1D 其地址位为0xff；
		OPEN_LOCK           = 0X25,//开锁(DOWN)： CMD 0X25
		SHUT_LOCK           = 0X26,//闭锁(UP)： CMD 0x26
		GET_LOCK_ST         = 0X21,//读取锁状态： CMD 0X21
		GET_ULTRASONIC_CYC  = 0X08,//读取超声波检测周期 CMD  0X08
		GET_ULTRASONIC_TIME = 0X0A,//读取超声波滤波时间 CMD  0X0A
		GET_ULTRASONIC_PARA = 0X14,//超声波运行参数查询 CMD  0X14
		SET_ULTRASONIC_CYC  = 0X07, //设置超声波检测周期： CMD 0X07
		SET_ULTRASONIC_TIME = 0X09,//设置超声波滤波时间： CMD 0X09
		SET_ULTRASONIC      = 0X1B,//设置超声波探测开关或查询： CMD 0X1B
		SET_PARAM_GET       = 0X83,//新的 13.传感器的参数设置和查询(0x83)：55 ADDR 05 83 DATA Para1 Para2 Para3 CRC AA
	                          //	DATA: 0—设置，1—查询，查询状态下Para1 Para2 Para3 无意义，00即可
                            // Para1：传感器开关 0—关闭 1—打开（默认打开）
                            // Para2：锁臂降下后不检测的时间，10s为一单位（默认10s）
                             //Para3：无车延时升起时间，1s为一单位
		SET_BEEP            = 0X22,//蜂鸣器设置或查询：   CMD 0X22
		SET_ADDR            = 0X40,//ADDR设置：           CMD 0X40
		SET_BAUD            = 0X42,//波特率设置：         CMD 0X42
	}ecar_LOCK_CMD_SET;
#else 
	typedef enum
	{
		GET_FIRMWARE_VER    = 0x1A,//软件版本号查询：   CMD  0X1A
		GET_Equipment_ID    = 0X1D,//查询设备ID CMD 0x1D 其地址位为0xff；
		OPEN_LOCK           = 0X01,//开锁(DOWN)： CMD 0X01
		SHUT_LOCK           = 0X02,//闭锁(UP)： CMD 0x02
		GET_LOCK_ST         = 0X06,//读取锁状态： CMD 0X06
		GET_ULTRASONIC_CYC  = 0X08,//读取超声波检测周期 CMD  0X08
		GET_ULTRASONIC_TIME = 0X0A,//读取超声波滤波时间 CMD  0X0A
		GET_ULTRASONIC_PARA = 0X14,//超声波运行参数查询 CMD  0X14
		SET_ULTRASONIC_CYC  = 0X07, //设置超声波检测周期： CMD 0X07
		SET_ULTRASONIC_TIME = 0X09,//设置超声波滤波时间： CMD 0X09
		SET_BEEP            = 0X15,//蜂鸣器设置或查询：   CMD 0X15
		SET_ULTRASONIC      = 0X1B,//设置超声波探测开关或查询： CMD 0X1B
		SET_ADDR            = 0X1C,//ADDR设置：           CMD 0X1C
		SET_BAUD            = 0X1E,//波特率设置：         CMD 0X1E
	}ecar_LOCK_CMD_SET;
#endif 

typedef struct
{
	unsigned short FIRMWARE_VER[2];//软件/硬件版本
	unsigned char module_ID;//设备ID号
	unsigned char lock_st;//0x00闭锁状态，0x01开锁状态，0x02下降遇阻(会维持,此时发升起指令升起后02状态解除)，0x03上升遇阻并且恢复(此时锁是放下的，在未发开锁指令前，一直维持0x03间接表示可能有车)，0x88运动状态
						  //0x10当前开锁状态并且检测到上面无车（地锁请求无车升起）
	unsigned char ULTRASONIC_CYC;//超声波检测周期
	unsigned char ULTRASONIC_TIME;//读取超声波滤波时间
	unsigned char ULTRASONIC_PARA[2];//超声波运行参数查询,
									//CSB_TIME超声波的检测周期计时，CSB_NUM连续检测上面无车计时	
	unsigned char BEEP;		//蜂鸣器设置或查询：
  unsigned char ULTRASONIC ;//超声波开关状态	
	unsigned char	CSB; //CSB—车辆传感器状态，01为有车，00为无车02为变化中 03为未知
	unsigned char	BAT_SOC; //BAT—电池电量，03满格，02中间，01低电 00没电
}ecarlock_park_st;//车位锁设备相关状态



//查询设备ID CMD 0x1D 其地址位为0xff；
//开锁： CMD 0X01 
//闭锁： CMD 0x02
//读取锁状态： CMD 0X06
//读取超声波检测周期 CMD  0X08
//读取超声波滤波时间 CMD  0X0A
//超声波运行参数查询 CMD  0X14
//软件版本号查询：   CMD  0X1A
typedef struct 
{
	//报文头 Ans8
	unsigned char Start_Sign;   //报文起始标志2个字节
	unsigned char Address;     //帧长度
	unsigned char Length;
	unsigned char CMD;   //地址
	unsigned char CHK;
	unsigned char END;
}CAR_AP01_Frame;

//设置超声波检测周期： CMD 0X07
//设置超声波滤波时间： CMD 0X09
//蜂鸣器设置或查询：   CMD 0X15
//设置超声波探测开关或查询： CMD 0X1B
//ADDR设置：           CMD 0X1C
//波特率设置：         CMD 0X1E
typedef struct 
{
	//报文头 Ans8
	unsigned char Start_Sign;   //报文起始标志2个字节
	unsigned char Address;     //帧长度
	unsigned char Length;
	unsigned char CMD;   //地址
	unsigned char DATA;
	unsigned char CHK;
	unsigned char END;
}CAR_AP02_Frame;


extern unsigned char Fill_AP02_Frame(unsigned char *SendBuf,unsigned char address,unsigned char CMD,unsigned char data);
extern unsigned char Fill_AP01_Frame(unsigned char *SendBuf,unsigned char address,unsigned char CMD);
extern unsigned char CAR_LOCK_Rx_Handler(unsigned char* rxbuf,unsigned char rx_len );

extern unsigned char g_carlock_addr[MAX_CARLOCK_NUM];
extern ecarlock_park_st g_car_st[MAX_CARLOCK_NUM];
#ifdef __cplusplus
}
#endif

#endif
