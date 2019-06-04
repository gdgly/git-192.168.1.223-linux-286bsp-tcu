/******************************************************************************/
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <execinfo.h>
#include <limits.h>

#include "./data/globalvar.h"
#include "./lock/Carlock_task.h"

/***************************************************************************/
UINT32 Meter_Read_Write_Watchdog = 0;
UINT32 PC_Watchdog = 0;
UINT32 MC_Read_Watchdog = 0;
UINT32 MC_Manage_Watchdog_1 = 0;
UINT32 MC_Manage_Watchdog_2 = 0;
UINT32 ISO8583_Heart_Watchdog = 0;
UINT32 ISO8583_Busy_Watchdog = 0;
UINT32 Main_1_Watchdog = 0;
UINT32 Main_2_Watchdog = 0;

void data_init(void);


/***************************************************************************/
void safety_Thread(void)
{
    UINT8 textBuf[200];
    Meter_Read_Write_Watchdog = 0;
    PC_Watchdog = 1;
    MC_Read_Watchdog = 0;
    MC_Manage_Watchdog_1 = 0;
    MC_Manage_Watchdog_2 = 0;
    ISO8583_Heart_Watchdog = 0;
    ISO8583_Busy_Watchdog = 0;
    Main_1_Watchdog = 0;
    Main_2_Watchdog = 0;
    prctl(PR_SET_NAME, (unsigned long)"safety_Thread"); //设置线程名字

    while (1)
    {
        sleep(120);
        PC_Watchdog = 1;

        if ((Meter_Read_Write_Watchdog == 1) && (PC_Watchdog == 1)\
            && (MC_Read_Watchdog == 1) && (MC_Manage_Watchdog_1 == 1) && (MC_Manage_Watchdog_2 == 1)\
            && (ISO8583_Heart_Watchdog == 1) && (ISO8583_Busy_Watchdog == 1) && (Main_1_Watchdog == 1) && (Main_2_Watchdog == 1))
        {
            Meter_Read_Write_Watchdog = 0;
            PC_Watchdog = 0;
            MC_Read_Watchdog = 0;
            MC_Manage_Watchdog_1 = 0;
            MC_Manage_Watchdog_2 = 0;
            ISO8583_Heart_Watchdog = 0;
            ISO8583_Busy_Watchdog = 0;
            Main_1_Watchdog = 0;
            Main_2_Watchdog = 0;
        }
        else
        {
            printf("Meter_Read_Write_Watchdog=%d,PC_Watchdog=%d, \
			        MC_Read_Watchdog=%d,MC_Manage_Watchdog_1=%d,MC_Manage_Watchdog_2=%d, \
							ISO8583_Heart_Watchdog=%d,ISO8583_Busy_Watchdog=%d,Main_1_Watchdog=%d,Main_2_Watchdog=%d\n", \
                   Meter_Read_Write_Watchdog, PC_Watchdog,
                   MC_Read_Watchdog, MC_Manage_Watchdog_1, MC_Manage_Watchdog_2, \
                   ISO8583_Heart_Watchdog, ISO8583_Busy_Watchdog, Main_1_Watchdog, Main_2_Watchdog);
            sprintf(textBuf, "echo system safety_Thread happen Meter_Read_Write_Watchdog=%d,PC_Watchdog=%d, \
			        MC_Read_Watchdog=%d,MC_Manage_Watchdog_1=%d,MC_Manage_Watchdog_2=%d, \
			        ISO8583_Heart_Watchdog=%d,ISO8583_Busy_Watchdog=%d,Main_1_Watchdog=%d,Main_2_Watchdog=%d >> /mnt/systemlog", \
                    Meter_Read_Write_Watchdog, PC_Watchdog, \
                    MC_Read_Watchdog, MC_Manage_Watchdog_1, MC_Manage_Watchdog_2, \
                    ISO8583_Heart_Watchdog, ISO8583_Busy_Watchdog, Main_1_Watchdog, Main_2_Watchdog);
            system(textBuf);
            system("date >> /mnt/systemlog");
            sleep(3);
            system("reboot");
        }

        *process_dog_flag = *process_dog_flag | 0x00000001;
    }
}
/***************************************************************************/
void safety_Thread_init(void)
{
    pthread_t td1;
    int ret, stacksize = PTHREAD_STACK_MIN;//16348
    pthread_attr_t attr;
    ret = pthread_attr_init(&attr);

    if (ret != 0)
    {
        return;
    }

    ret = pthread_attr_setstacksize(&attr, stacksize);

    if (ret != 0)
    {
        return;
    }

    /* 创建安全监视线程 */
    if (0 != pthread_create(&td1, &attr, (void *)safety_Thread, NULL))
    {
        printf("####pthread_create safety_Thread ERROR ####\n\n");
    }
    else
    {
        printf("####pthread_create safety_Thread sucess ####\n\n");
    }

    ret = pthread_attr_destroy(&attr);

    if (ret != 0)
    {
        return;
    }
}

//设置默认的系统时间,如2018-12-12 12:12:12
void Set_Default_Time(char *time_str)
{
    char shell_cmd[256];
    snprintf(shell_cmd, 255, "date -s \"%s\"", time_str);
    system(shell_cmd);
    system("hwclock --systohc --utc");
}

/***************************************************************************/
main(void)
{
    UINT8 textBuf[120];
    int i = 0, j = 0;
    FILE *ku = NULL;
    time_t systime = 0;
    struct tm tm_t;
    time_t time_tmp1;
    time_t time_tmp2;
    systime = time(NULL);         //获得系统的当前时间
    localtime_r(&systime, &tm_t);  //结构指针指向当前时间

    if ((tm_t.tm_year + 1900) < 2019) //系统时间不对
    {
        Set_Default_Time("2019-5-05 00:00:00");
    }

    //创建目录
    mkdir(F_PATH_Nand1, 0777);
    mkdir(F_PATH, 0777);
    mkdir(F_INIT, 0777);
    mkdir(F_UPDATE, 0777);
    mkdir(F_PARAMPATH, 0777);
    mkdir(F_DATAPATH, 0777);
    mkdir(F_DATA_LOGPATH, 0777);
    mkdir(F_BACKUPPATH, 0777);
    chmod("/tmp", 0755);
    strcpy(textBuf, F_BACKUPPATH);
    strcat(textBuf, "/param");
    mkdir(textBuf, 0777);
    System_init();
    *process_dog_flag = 0;
    system("ifconfig can0 down&&ifconfig can1 down");
    sleep(1);
    system("ip link set can0 type can restart-ms 200");
    system("ip link set can1 type can restart-ms 200");

    //判断内核版本用的，文件不存在是老版本，新版本有版本后
    if (access("/etc/kuversion", F_OK) == 0) //F_OK   测试文件是否存在 R_OK  测试读权限 W_OK 测试写权限 X_OK 测试执行权
    {
        if (Globa_1->Charger_param.Control_board_Baud_rate == 0)
        {
            system("canconfig can0 bitrate 250000");//动态改变波特率，必训在ifconfig can up之前有效
            system("canconfig can1 bitrate 250000");
        }
        else
        {
            system("canconfig can0 bitrate 125000");//动态改变波特率，必训在ifconfig can up之前有效
            system("canconfig can1 bitrate 125000");
        }
    }
    else
    {
        if (Globa_1->Charger_param.Control_board_Baud_rate == 0)
        {
            system("echo 250000 >/sys/devices/platform/FlexCAN.0/bitrate");//动态改变波特率，必训在ifconfig can up之前有效
            system("echo 250000 >/sys/devices/platform/FlexCAN.1/bitrate");
        }
        else
        {
            system("echo 125000 >/sys/devices/platform/FlexCAN.0/bitrate");//动态改变波特率，必训在ifconfig can up之前有效
            system("echo 125000 >/sys/devices/platform/FlexCAN.1/bitrate");
        }
    }

    sleep(1);
    system("ifconfig can0 up&&ifconfig can1 up");

    if ((ku = fopen("/etc/kuversion", "r")) != NULL)
    {
        fgets(textBuf, 6, ku);//最大得到5个有效字符，第六个为'\0'
        sprintf(&Globa_1->kernel_ver[0], "%.5s", textBuf);//初始化内核版本 最大得到5个有效字符，第六个为'\0'
        fclose(ku);
    }
    else
    {
        sprintf(&Globa_1->kernel_ver[0], "%s", "01.01");
    }

    sprintf(&Globa_1->protocol_ver[0], "%s", PROTOCOL_VER);//协议版本
    data_init();
    system("echo system start at >> /mnt/systemlog&&date >> /mnt/systemlog");
    Globa_1->user_sys_time = 0;
    Globa_2->user_sys_time = 0;
    Globa_1->time_update_flag = 0;

    while (1)
    {
        Globa_1->user_sys_time++;
        Globa_2->user_sys_time++;
        sleep(1);
        i++;

        if (i >= 300) //5 min check
        {
            printf("main i =%d \n", i);
            i = 0;

            if ((*process_dog_flag & 0x00000001) == 0x00000001)
            {
                *process_dog_flag = 0;
            }
            else
            {
                printf("process_dog_flag = %d \n", *process_dog_flag);
                sprintf(textBuf, "%s%x%s", "echo system process dog happen ", *process_dog_flag, " >> /mnt/systemlog");
                sprintf(textBuf, "%s&&date >> /mnt/systemlog&&reboot", textBuf);
                system(textBuf);
            }
        }

        j++;

        if (j >= 3600) //one hour check
        {
            j = 0;
            //sprintf(textBuf, "%s", "find /mnt/Nand1/ea_app/data  -type f -mtime +366 -name \"*.*\" -exec rm -rf {} \\;");
            sprintf(textBuf, "hwclock --hctosys --utc");
            system(textBuf);
        }

        if (Globa_1->time_update_flag == 1)
        {
            systime = time(NULL);         //获得系统的当前时间
            localtime_r(&systime, &tm_t); //结构指针指向当前时间
            time_tmp1 = (tm_t.tm_year + 1900) * 365 * 24 * 3600 + (tm_t.tm_mon + 1) * 30 * 24 * 3600 + tm_t.tm_mday * 24 * 3600 + \
                        tm_t.tm_hour * 3600 + tm_t.tm_min * 60 + tm_t.tm_sec;
            time_tmp2 = (Globa_1->year) * 365 * 24 * 3600 + (Globa_1->month) * 30 * 24 * 3600 + Globa_1->date * 24 * 3600 + \
                        Globa_1->hour * 3600 + Globa_1->minute * 60 + Globa_1->second;

            if (abs(time_tmp1 - time_tmp2) > 20) //这种时间处理也是粗略处理 超过20 秒需校准时间
            {
                sprintf(textBuf, "date -s %04d.%02d.%02d-%02d:%02d:%02d", Globa_1->year, Globa_1->month, Globa_1->date, \
                        Globa_1->hour, Globa_1->minute, Globa_1->second);
                system(textBuf);
                system("hwclock --systohc --utc");
            }

            Globa_1->time_update_flag = 0;
        }
    }

    return EXIT_SUCCESS;
}

/***************************************************************************/
/***************************************************************************/
void dump(int signo)
{
    void *array[256];
    size_t size;
    char **strings;
    size_t i;
    signal(SIGSEGV, SIG_IGN);
    size = backtrace(array, 256);
    strings = backtrace_symbols(array, size);
    printf("Obtained %zd stack frames.\n", size);

    for (i = 0; i < size; i++)
    {
        printf("%s\n", strings[i]);
    }

    free(strings);
    system("ps >> /mnt/systemlog");
    exit(0);//  system("reboot");
}

void signal_SIGSEGV_deal(int sig)
{
    printf("OUCH11111111! - I got signal_SIGSEGV_deal %d\n", sig);
    system("echo OUCH11111111! - I got signal_SIGSEGV_deal >> /mnt/systemlog&&date >> /mnt/systemlog");
    signal(SIGSEGV, SIG_IGN);
    //  sleep(3);
    //  exit(0);//  system("reboot");
}

void signal_SIGBUS_deal(int sig)
{
    printf("OUCH22222222! - I got signal_SIGBUS_deal %d\n", sig);
    system("echo OUCH11111111! - I got signal_SIGBUS_deal >> /mnt/systemlog&&date >> /mnt/systemlog");
    signal(SIGBUS, SIG_IGN);
    //  sleep(3);
    //exit(0);//    system("reboot");
}

void signal_SIGPIPE_deal(int sig)
{
    printf("OUCH33333333! - I got signal_SIGPIPE_deal %d\n", sig);
    system("echo OUCH11111111! - I got signal_SIGPIPE_deal >> /mnt/systemlog&&date >> /mnt/systemlog");
    signal(SIGPIPE, SIG_IGN);
    //sleep(3);
    //exit(0);//    system("reboot");
}

void data_init(void)
{
    prctl(PR_SET_NAME, (unsigned long)"daemon process"); //设置线程名字
    int pid;
    pid = fork();

    if (pid == -1)
    {
        perror("fork data_init");
        exit(EXIT_FAILURE);
    }

    if (pid == 0)
    {
        prctl(PR_SET_NAME, (unsigned long)"event_handling"); //设置线程名字
        printf("son proccess data_init pid = %d\n\r", getpid());
        struct sigaction sact;
        sact.sa_handler = signal_SIGSEGV_deal;
        sact.sa_flags = 0;

        if (sigaction(SIGSEGV, &sact, NULL) == 0)
        {
            printf("SIGSEGV ignore");
            //MSG_FPUTS("my_SIGSEGV_deal init",fdtest);
        }

        sact.sa_handler = signal_SIGBUS_deal;
        sact.sa_flags = 0;

        if (sigaction(SIGBUS, &sact, NULL) == 0)
        {
            printf("SIGBUS ignore");
            //MSG_FPUTS("my_SIGBUS_deal init",fdtest);
        }

        sact.sa_handler = signal_SIGPIPE_deal;
        sact.sa_flags = 0;

        if (sigaction(SIGPIPE, &sact, NULL) == 0)
        {
            printf("SIGPIPE ignore");
        }

        safety_Thread_init(); //启动保护线程
        IC_Thread();

        //  PC_Thread();
        if (Globa_1->Charger_param.meter_config_flag == 1)
        {
            DC_2007Meter_Read_Write_Thread();
        }
        else
        {
            Meter_Read_Write_Thread();
        }

        if (Globa_1->Charger_param.AC_Meter_config == 1)
        {
            AC_2007Meter_Read_Write_Thread();
        }

        //COUPLE_Host_Device_Task_Thread();
        MC_Read_Thread();
        MC_Manage_Thread_1();
        MC_Manage_Thread_2();
        ISO8583_Heart_Thread();
        ISO8583_Busy_Thread();

        //      Software_update_thread();
        if (Globa_1->Charger_param.Serial_Network != 1)
        {
            ISO8583_Update_Thread();//网线升级
        }

        if (Globa_1->Charger_param.LED_Type_Config == 0)
        {
            LED_C_Interface_Thread(); //灯板
        }

        if (dev_cfg.dev_para.car_lock_num != 0)
        {
            carlock_Thread();//地锁
        }

        Main_Thread_2();
        Main_Task_1();

        while (1)
        {
            sleep(1);
        }

        system("reboot");
        exit(0);
    }
    else
    {
        sleep(5);
    }
}

