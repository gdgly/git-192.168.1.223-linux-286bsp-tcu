#ifndef COMMON_H
#define COMMON_H
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/msg.h>
#include <time.h>
#include <sys/time.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <pthread.h>
#include <stdarg.h>
#include <netinet/in.h>
#include <dlfcn.h>

#define BUILD_YEAR_CH0 (__DATE__[ 7])
#define BUILD_YEAR_CH1 (__DATE__[ 8])
#define BUILD_YEAR_CH2 (__DATE__[ 9])
#define BUILD_YEAR_CH3 (__DATE__[10])


#define BUILD_MONTH_IS_JAN (__DATE__[0] == 'J' && __DATE__[1] == 'a' && __DATE__[2] == 'n')
#define BUILD_MONTH_IS_FEB (__DATE__[0] == 'F')
#define BUILD_MONTH_IS_MAR (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'r')
#define BUILD_MONTH_IS_APR (__DATE__[0] == 'A' && __DATE__[1] == 'p')
#define BUILD_MONTH_IS_MAY (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'y')
#define BUILD_MONTH_IS_JUN (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'n')
#define BUILD_MONTH_IS_JUL (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'l')
#define BUILD_MONTH_IS_AUG (__DATE__[0] == 'A' && __DATE__[1] == 'u')
#define BUILD_MONTH_IS_SEP (__DATE__[0] == 'S')
#define BUILD_MONTH_IS_OCT (__DATE__[0] == 'O')
#define BUILD_MONTH_IS_NOV (__DATE__[0] == 'N')
#define BUILD_MONTH_IS_DEC (__DATE__[0] == 'D')


#define BUILD_MONTH_CH0 \
((BUILD_MONTH_IS_OCT || BUILD_MONTH_IS_NOV || BUILD_MONTH_IS_DEC) ? '1' : '0')

#define BUILD_MONTH_CH1 \
( \
(BUILD_MONTH_IS_JAN) ? '1' : \
(BUILD_MONTH_IS_FEB) ? '2' : \
(BUILD_MONTH_IS_MAR) ? '3' : \
(BUILD_MONTH_IS_APR) ? '4' : \
(BUILD_MONTH_IS_MAY) ? '5' : \
(BUILD_MONTH_IS_JUN) ? '6' : \
(BUILD_MONTH_IS_JUL) ? '7' : \
(BUILD_MONTH_IS_AUG) ? '8' : \
(BUILD_MONTH_IS_SEP) ? '9' : \
(BUILD_MONTH_IS_OCT) ? '0' : \
(BUILD_MONTH_IS_NOV) ? '1' : \
(BUILD_MONTH_IS_DEC) ? '2' : \
/* error default */ '?' \
)

#define BUILD_DAY_CH0 ((__DATE__[4] >= '0') ? (__DATE__[4]) : '0')
#define BUILD_DAY_CH1 (__DATE__[ 5])



// Example of __TIME__ string: "21:06:19"
// 01234567

#define BUILD_HOUR_CH0 (__TIME__[0])
#define BUILD_HOUR_CH1 (__TIME__[1])

#define BUILD_MIN_CH0 (__TIME__[3])
#define BUILD_MIN_CH1 (__TIME__[4])

#define BUILD_SEC_CH0 (__TIME__[6])
#define BUILD_SEC_CH1 (__TIME__[7])
#define _TIME_ZONE_OFFSET               8*60*60



#ifndef MAX
#define MAX(x, y)   (((x) < (y)) ? (y) : (x))
#endif

#ifndef MIN
#define MIN(x, y)   (((x) < (y)) ? (x) : (y))
#endif

#define BCD2HEX(x) (((x) >> 4) * 10 + ((x) & 0x0F))         /*20H -> 20*/
#define HEX2BCD(x) (((x) % 10) + ((((x) / 10) % 10) << 4))  /*20 -> 20H*/

#define BYTESWAP(x) ((MSB(x) | (LSB(x) << 8)))
#define BITS(x,y) (((x)>>(y))&0x00000001)   /* 判断某位是否为1 */
#define SETBITS(x,y,n) (x) = (n) ? ((x)|(1 << (y))) : ((x) &(~(1 << (y))));
#define SETBITSTO0(x,y) (x) = ((x) &(~(1 << (y))));
#define SETBITSTO1(x,y) (x) = ((x)|(1 << (y)));
#define INVERSE(x,y)    (x) = ((x)^(1<<y));  /* 给某位置反 */

//Big-Endian
#define BigtoLittle16(A) ((((UINT16)(A) & 0xff00) >> 8)|(((UINT16)(A) & 0x00ff) << 8))
#define BigtoLittle32(A) ((((UINT32)(A) & 0xff000000) >> 24)|(((UINT32)(A) & 0x00ff0000) >> 8)|(((UINT32)(A) & 0x0000ff00) << 8)|(((UINT32)(A) & 0x000000ff) << 24))

//Little-Endian to Big-Endian
#define LittletoBigEndian16(A) ((((UINT16)(A) & 0xff00) >> 8)|(((UINT16)(A) & 0x00ff) << 8))
#define LittletoBigEndian32(A) ((((UINT32)(A) & 0xff000000) >> 24)|(((UINT32)(A) & 0x00ff0000) >> 8)|(((UINT32)(A) & 0x0000ff00) << 8)|(((UINT32)(A) & 0x000000ff) << 24))
//本系统ARM9260使用小端模式


#define OK 0
#define ERROR8 0xFF
#define ERROR16 0xFFFF
#define ERROR32 0xFFFFFFFF

#define MSG_QUEUE_KEY_SDOG  0x200
#define MSG_QUEUE_KEY_COLLECTION 0x201
#define USB_PATH "/media/usb"
#define BATTERY_LOG_FILE    "/home/root/log/bat.log"
#define DEBUG_LOG_FILE      "/home/root/log/debug.log"
#define EAST_LOG_FILE       "/home/root/log/EAST.log"  // 日志文件,
#define MEM_LOG_FILE        "/home/root/log/mem.log"
#define TCPMODBUS_LOG_FILE  "/home/root/log/TcpModbus.log"
#define CONTROL_LOG_FILE    "/home/root/log/control.log"  // 日志文件,

#define RUN_GUN1_LOG_FILE      "/home/root/log/RunGun1.log"  //运行1号枪信息
#define RUN_GUN2_LOG_FILE      "/home/root/log/RunGun2.log"  //运行2号枪信息
#define GLOBA_RUN_LOG_FILE     "/home/root/log/GlobaRun.log" //全局运行信息
#define CARD_RUN_LOG_FILE     "/home/root/log/CardRun.log" //全局运行信息

#define COMM_RUN_LOG_FILE     "/home/root/log/CommRun.log" //与后台交互运行信息
#define RUN_EVENT_1_LOG_FILE  "/home/root/log/RunEv1.log" //与后台交互运行信息
#define RUN_EVENT_2_LOG_FILE  "/home/root/log/RunEv2.log" //与后台交互运行信息

#define CARLOCK_RUN_LOG_FILE  "/home/root/log/CARLOCK.log" //与后台交互运行信息

#define MAXLINELEN 4096             // LogOut一行数据最大长度
#define APP_LOG_MAX_SIZE 512*1024

typedef struct _ReportClient
{
    struct sockaddr_in addr;
    time_t loginTime;
}ReportClient;
typedef int (*ENUMSIGNALPROC)(int channel, float value, void* p) ;
typedef int (*QUERYPROC)(void* fd, int id, ENUMSIGNALPROC, void* p) ;
typedef int (*TESTPROC)(void* fd, int id, ENUMSIGNALPROC, void* p) ;
typedef int (*CONTROLPROC)(void* fd, int id, void* pCmdStr) ;

void AppLogOut(const char *fmt, ...);
void MemLogOut(const char *fmt, ...);
void ControlLogOut(const char *fmt, ...);
void DebugLogOut(const char *fmt, ...);

void RUN_GUN1_LogOut(const char *fmt, ...);
void RUN_GUN2_LogOut(const char *fmt, ...);
void GLOBA_RUN_LogOut(const char *fmt, ...);
void CARD_RUN_LogOut(const char *fmt, ...);
void TcpModbusLog(const char *fmt, ...);
void writeLog(char *logfile,char *header,char *data,int maxsize);
void BatteryLogOut(const char *fmt, ...);
int LedRelayControl_init();
void GPIO(int pin,int value);
struct tm * my_localtime_r(const time_t *srctime,struct tm *tm_time);
char *getTimeStr(char *buf,time_t t);
char *getDateStr(char *buf,time_t t);
int testProlib(char *name,char *errmsg);
#endif
