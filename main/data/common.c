#include "common.h"
pthread_mutex_t gLogMutex;


//my_localtime_r转自（http://blog.csdn.net/yaxf999/article/details/8136712）
struct tm * my_localtime_r(const time_t *srctime,struct tm *tm_time)
{
    long int n32_Pass4year,n32_hpery;

    //        // 每个月的天数  非闰年
    const static char Days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    //                // 一年的小时数
    const static int ONE_YEAR_HOURS = 8760; // 365 * 24 (非闰年)
    //                        //计算时差8*60*60 固定北京时间
    time_t time = *srctime;
    time=time+28800;
    tm_time->tm_isdst=0;
    if(time < 0)
    {
        time = 0;
    }

    //取秒时间
    tm_time->tm_sec=(int)(time % 60);
    time /= 60;

    //取分钟时间
    tm_time->tm_min=(int)(time % 60);
    time /= 60;

    //计算星期
    tm_time->tm_wday=(time/24+4)%7;

    //取过去多少个四年，每四年有 1461*24 小时
    n32_Pass4year=((unsigned int)time / (1461L * 24L));

    //计算年份
    tm_time->tm_year=(n32_Pass4year << 2)+70;

    //四年中剩下的小时数
    time %= 1461L * 24L;
    //计算在这一年的天数
    tm_time->tm_yday=(time/24)%365;

    //校正闰年影响的年份，计算一年中剩下的小时数
    for (;;)
    {
        //一年的小时数
        n32_hpery = ONE_YEAR_HOURS;

        //判断闰年
        if ((tm_time->tm_year & 3) == 0)
        {
            //是闰年，一年则多24小时，即一天
            n32_hpery += 24;
        }

        if (time < n32_hpery)
        {
            break;
        }

        tm_time->tm_year++;
        time -= n32_hpery;
    }
    //小时数
    tm_time->tm_hour=(int)(time % 24);

    //一年中剩下的天数
    time /= 24;

    //假定为闰年
    time++;

    //校正润年的误差，计算月份，日期
    if ((tm_time->tm_year & 3) == 0)
    {
        if (time > 60)
        {
            time--;
        }
        else
        {
            if (time == 60)
            {
                tm_time->tm_mon = 1;
                tm_time->tm_mday = 29;
                return tm_time;
            }
        }
    }

    //计算月日
    for (tm_time->tm_mon = 0;Days[tm_time->tm_mon] < time;tm_time->tm_mon++)
    {
        time -= Days[tm_time->tm_mon];
    }

    tm_time->tm_mday = (int)(time);
    return tm_time;
}


int backlight_control_msg_init()
{

    static int con_sd_id=-1;
    static int msg_id=-1;
    if(con_sd_id<0)
    {
        if((con_sd_id = ftok("/home/root/backlight", 'm')) < 0)
        {//ftok:系统IPC键值的格式转换函数,系统建立IPC通讯 （消息队列、信号量和共享内存） 时必须指定一个ID值。通常情况下，该id值通过ftok函数得到
            printf("------------ Create blacklight id error!--------------\n");
        }
    }
    if(msg_id<0)
    {
        msg_id=msgget(con_sd_id, IPC_CREAT|0660);
        if(msg_id<0)
        {
            printf("------------ get blacklight id error!--------------\n");

        }
    }


    return msg_id;
}

char *getTimeStr(char *buf,time_t t)
{
    struct tm local;
    localtime_r(&t,&local);
    snprintf(buf,20,"%04d-%02d-%02d %02d:%02d:%02d",
             local.tm_year+1900,
             local.tm_mon+1,
             local.tm_mday,
             local.tm_hour,
             local.tm_min,
             local.tm_sec);
    return buf;
}
char *getDateStr(char *buf,time_t t)
{
    struct tm local;
    localtime_r(&t,&local);
    snprintf(buf,20,"%04d-%02d-%02d",
             local.tm_year+1900,
             local.tm_mon+1,
             local.tm_mday
             );
    return buf;
}

/**
 * @brief
 *
 * @param logfile
 * @param header log数据的头
 * @param data log数据
 * @param maxsize 指的是log文件的最大长度
 */
void writeLog(char *logfile,char *header,char *data,int maxsize)
{
    char bak[32];
    memset(bak,0,32);
    FILE *file;
    sprintf(bak,"%s%s",logfile,".bak");
    file=fopen(logfile,"a+");
    if(file)
    {
        if(header)
            fputs(header,file);
        fputs(data,file);
        fputs("\r\n",file);

        // 取当前日志文件的长度
        long loffset = 0;
        if( fseek( file, 0, SEEK_END ) == 0 )
        {
            loffset = ftell( file );
        }

        fclose( file );


        // 判断日志文件是否超长，如超长，将其命名为bak文件
        if( loffset > maxsize )
        {
            //定期清理系统缓存,防止段错误
            system("echo 1>/proc/sys/vm/drop_caches");
            if( remove( bak) == -1 )
            {
                fputs( header, stderr );
                fputs( "删除备份日志文件失败!\x0D\x0A", stderr );
            }

            if( rename( logfile,bak) == -1 )
            {
                fputs( header, stderr );
                fputs( "备份日志文件失败!\x0D\x0A", stderr );
                if( remove( logfile ) == -1 )
                {
                    fputs( header, stderr );
                    fputs( "删除日志文件失败!\x0D\x0A", stderr );
                }
            }
            else
            {
                fputs( header, stderr );
                fputs( "备份日志文件成功!\x0D\x0A", stderr );
            }
        }
    }
    else
    {
        fputs( "failed to open log file\x0D\x0A", stderr );
    }
}
//*****************************************************************
// 函数名称：Log_Prn
// 功能描述：打印日志函数：
// 输入参数：logfile      -  日志文件名，为NULL时，屏幕打印
//           fmt          -  打印的字符串
//           ap           -  字符串格式
// 输出参数：
// 返    回：
// 其他：
//*****************************************************************
void Log_Prn( const char *fmt, va_list ap, char *logfile ,int maxSize)
{


    char buf[MAXLINELEN+50] = { 0 };
    if(strlen(fmt)>MAXLINELEN)
    {
        return;
    }
    char szLine[50] = {0, };
    struct timeb timebuffer;
    struct tm local;
    time_t t;
    time(&t);
    ftime(&timebuffer);
    char timeline[50]={0};
    localtime_r(&t,&local);
    snprintf(timeline,20,"%04d-%02d-%02d %02d:%02d:%02d",
             local.tm_year+1900,
             local.tm_mon+1,
             local.tm_mday,
             local.tm_hour,
             local.tm_min,
             local.tm_sec);
    snprintf(szLine,25,"%.20s:%03hu ",timeline,timebuffer.millitm);

    vsprintf( buf, fmt, ap );
    buf[MAXLINELEN-1]=0;
    //printf("%s",szLine);
    //printf("%s\n",buf);
    writeLog(logfile,szLine,buf,maxSize);


}



//*****************************************************************
// 函数名称：LogOut
// 功能描述：记录日志文件函数：
// 输入参数：fmt          -  打印的字符串
// 输出参数：
// 返    回：
// 其他：
//*****************************************************************
//void LogOut( const char *fmt, ... )
void AppLogOut(const char *fmt, ...)
{
    va_list ap;
    if(pthread_mutex_lock(&gLogMutex)!=0)
    {
        pthread_mutex_init(&gLogMutex,NULL);
        if(pthread_mutex_lock(&gLogMutex)!=0)
            return;
    }
    va_start( ap, fmt );
    Log_Prn( fmt, ap, (char *)EAST_LOG_FILE,APP_LOG_MAX_SIZE);
    va_end( ap );
    if(pthread_mutex_unlock(&gLogMutex)!=0)
    {
        pthread_mutex_init(&gLogMutex,NULL);

    }

    return;
}
void MemLogOut(const char *fmt, ...)
{
    va_list ap;
    if(pthread_mutex_lock(&gLogMutex)!=0)
    {
        pthread_mutex_init(&gLogMutex,NULL);
        if(pthread_mutex_lock(&gLogMutex)!=0)
            return;
    }
    va_start( ap, fmt );
    Log_Prn( fmt, ap, (char *)MEM_LOG_FILE,APP_LOG_MAX_SIZE);
    va_end( ap );
    if(pthread_mutex_unlock(&gLogMutex)!=0)
    {
        pthread_mutex_init(&gLogMutex,NULL);

    }

    return;
}
void TcpModbusLog(const char *fmt,...)
{
    va_list ap;
    if(pthread_mutex_lock(&gLogMutex)!=0)
    {
        pthread_mutex_init(&gLogMutex,NULL);
        if(pthread_mutex_lock(&gLogMutex)!=0)
            return;
    }
    va_start( ap, fmt );
    Log_Prn( fmt, ap, (char *)TCPMODBUS_LOG_FILE,APP_LOG_MAX_SIZE);
    va_end( ap );
    if(pthread_mutex_unlock(&gLogMutex)!=0)
    {
        pthread_mutex_init(&gLogMutex,NULL);

    }

    return;
}
void DebugLogOut(const char *fmt, ...)
{
    va_list ap;
    if(pthread_mutex_lock(&gLogMutex)!=0)
    {
        pthread_mutex_init(&gLogMutex,NULL);
        if(pthread_mutex_lock(&gLogMutex)!=0)
            return;
    }
    va_start( ap, fmt );
    Log_Prn( fmt, ap, (char *)DEBUG_LOG_FILE ,APP_LOG_MAX_SIZE);
    va_end( ap );

    if(pthread_mutex_unlock(&gLogMutex)!=0)
    {
        pthread_mutex_init(&gLogMutex,NULL);

    }
    return;
}

void RUN_GUN1_LogOut(const char *fmt, ...)
{
    va_list ap;
    if(pthread_mutex_lock(&gLogMutex)!=0)
    {
        pthread_mutex_init(&gLogMutex,NULL);
        if(pthread_mutex_lock(&gLogMutex)!=0)
            return;
    }
    va_start( ap, fmt );
    Log_Prn( fmt, ap, (char *)RUN_GUN1_LOG_FILE ,APP_LOG_MAX_SIZE);
    va_end( ap );

    if(pthread_mutex_unlock(&gLogMutex)!=0)
    {
        pthread_mutex_init(&gLogMutex,NULL);

    }
    return;
}

void RUN_GUN2_LogOut(const char *fmt, ...)
{
    va_list ap;
    if(pthread_mutex_lock(&gLogMutex)!=0)
    {
        pthread_mutex_init(&gLogMutex,NULL);
        if(pthread_mutex_lock(&gLogMutex)!=0)
            return;
    }
    va_start( ap, fmt );
    Log_Prn( fmt, ap, (char *)RUN_GUN2_LOG_FILE ,APP_LOG_MAX_SIZE);
    va_end( ap );

    if(pthread_mutex_unlock(&gLogMutex)!=0)
    {
        pthread_mutex_init(&gLogMutex,NULL);

    }
    return;
}

void GLOBA_RUN_LogOut(const char *fmt, ...)
{
    va_list ap;
    if(pthread_mutex_lock(&gLogMutex)!=0)
    {
        pthread_mutex_init(&gLogMutex,NULL);
        if(pthread_mutex_lock(&gLogMutex)!=0)
            return;
    }
    va_start( ap, fmt );
    Log_Prn( fmt, ap, (char *)GLOBA_RUN_LOG_FILE ,APP_LOG_MAX_SIZE);
    va_end( ap );

    if(pthread_mutex_unlock(&gLogMutex)!=0)
    {
        pthread_mutex_init(&gLogMutex,NULL);

    }
    return;
}

void CARD_RUN_LogOut(const char *fmt, ...)
{
    va_list ap;
    if(pthread_mutex_lock(&gLogMutex)!=0)
    {
        pthread_mutex_init(&gLogMutex,NULL);
        if(pthread_mutex_lock(&gLogMutex)!=0)
            return;
    }
    va_start( ap, fmt );
    Log_Prn( fmt, ap, (char *)CARD_RUN_LOG_FILE ,APP_LOG_MAX_SIZE);
    va_end( ap );

    if(pthread_mutex_unlock(&gLogMutex)!=0)
    {
        pthread_mutex_init(&gLogMutex,NULL);

    }
    return;
}

void COMM_RUN_LogOut(const char *fmt, ...)
{
    va_list ap;
    if(pthread_mutex_lock(&gLogMutex)!=0)
    {
        pthread_mutex_init(&gLogMutex,NULL);
        if(pthread_mutex_lock(&gLogMutex)!=0)
            return;
    }
    va_start( ap, fmt );
    Log_Prn( fmt, ap, (char *)COMM_RUN_LOG_FILE ,APP_LOG_MAX_SIZE);
    va_end( ap );
    if(pthread_mutex_unlock(&gLogMutex)!=0)
    {
        pthread_mutex_init(&gLogMutex,NULL);
    }
    return;
}
void RunEventLogOut_1(const char *fmt, ...)
{
    va_list ap;
    if(pthread_mutex_lock(&gLogMutex)!=0)
    {
        pthread_mutex_init(&gLogMutex,NULL);
        if(pthread_mutex_lock(&gLogMutex)!=0)
            return;
    }
    va_start( ap, fmt );
    Log_Prn( fmt, ap, (char *)RUN_EVENT_1_LOG_FILE ,APP_LOG_MAX_SIZE);
    va_end( ap );
    if(pthread_mutex_unlock(&gLogMutex)!=0)
    {
        pthread_mutex_init(&gLogMutex,NULL);
    }
    return;
}

void RunEventLogOut_2(const char *fmt, ...)
{
    va_list ap;
    if(pthread_mutex_lock(&gLogMutex)!=0)
    {
        pthread_mutex_init(&gLogMutex,NULL);
        if(pthread_mutex_lock(&gLogMutex)!=0)
            return;
    }
    va_start( ap, fmt );
    Log_Prn( fmt, ap, (char *)RUN_EVENT_2_LOG_FILE ,APP_LOG_MAX_SIZE);
    va_end( ap );
    if(pthread_mutex_unlock(&gLogMutex)!=0)
    {
        pthread_mutex_init(&gLogMutex,NULL);
    }
    return;
}


void CARLOCK_RUN_LogOut(const char *fmt, ...)
{
    if(pthread_mutex_lock(&gLogMutex)!=0)
    {
        pthread_mutex_init(&gLogMutex,NULL);
        if(pthread_mutex_lock(&gLogMutex)!=0)
            return;
    }
    va_list ap;
    va_start( ap, fmt );
    Log_Prn( fmt, ap, (char *)CARLOCK_RUN_LOG_FILE ,APP_LOG_MAX_SIZE);
    va_end( ap );

    if(pthread_mutex_unlock(&gLogMutex)!=0)
    {
        pthread_mutex_init(&gLogMutex,NULL);

    }
    return;
}
void ControlLogOut(const char *fmt, ...)
{
    if(pthread_mutex_lock(&gLogMutex)!=0)
    {
        pthread_mutex_init(&gLogMutex,NULL);
        if(pthread_mutex_lock(&gLogMutex)!=0)
            return;
    }
    va_list ap;
    va_start( ap, fmt );
    Log_Prn( fmt, ap, (char *)CONTROL_LOG_FILE ,APP_LOG_MAX_SIZE);
    va_end( ap );
    if(pthread_mutex_unlock(&gLogMutex)!=0)
    {
        pthread_mutex_init(&gLogMutex,NULL);

    }
    return;
}

int testProlib(char *name,char *errmsg)
{
    char szFileOfDLL[200] = { 0 };

    strcpy(szFileOfDLL,name);

    const char *msg;
    dlerror();  // Clear errors
    void *m_hLib = (void *)dlopen(szFileOfDLL, RTLD_NOW);
    if(m_hLib==NULL)
    {
        sprintf(errmsg,"error:%s\n",dlerror());
        return 0;
    }

    if( (msg = dlerror()) != NULL )
    {

        sprintf(errmsg,"加载SO库<%s>出错：%s\n", szFileOfDLL, msg);
        dlclose(m_hLib);
        return 0;
    }


    dlerror();  // Clear errors
    QUERYPROC m_procQuery = (QUERYPROC)dlsym(m_hLib, "Query" );
    if( (errmsg = dlerror()) != NULL )
    {

        dlclose(m_hLib);

        sprintf(errmsg,"函数Query不在%s中\n", szFileOfDLL);
        return 0;
    }

    dlerror();  // Clear errors
    TESTPROC m_procTest = (TESTPROC)dlsym(m_hLib, "Test" );
    if( (errmsg = dlerror()) != NULL )
    {
        printf("函数Test不在%s中\n", szFileOfDLL);
    }

    dlerror();  // Clear errors
    CONTROLPROC m_procControl =(CONTROLPROC)dlsym(m_hLib, "Control" );
    if( (errmsg = dlerror()) != NULL )
    {
        dlclose(m_hLib);
        sprintf(errmsg,"函数Control不在%s中\n", szFileOfDLL);
        return 0;
    }

}


