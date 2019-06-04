/*******************************************************************************
  Copyright(C)    2014-2015,      EAST Co.Ltd.
  File name :     bms.c
  Author :        dengjh
  Version:        1.0
  Date:           2014.5.11
  Description:
  Other:
  Function List:
  History:
          <author>   <time>   <version>   <desc>
            dengjh    2014.4.11   1.0      Create
*******************************************************************************/
#include <getopt.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include<time.h>

#include "globalvar.h"
#include "MC_can.h"

#define PF_CAN   29

static UINT32 count_250ms = 0, count_350ms = 0, count_500ms = 0, count_650ms = 0, count_1s = 0, count_3s_or5s = 0, count_5s = 0, count_5s1 = 0, count_5s2 = 0, count_1min = 0;
static UINT32 count_250ms_flag = 0, count_350ms_flag = 0, count_500ms_flag = 0, count_650ms_flag = 0, count_1s_flag = 0, count_3s_or5s_flag = 0, count_5s_flag = 0, count_5s1_flag = 0, count_5s2_flag = 0, count_1min_flag = 0;
static UINT32 count_250ms_enable = 0, count_350ms_enable = 0, count_500ms_enable = 0, count_650ms_enable = 0, count_1s_enable = 0, count_3s_or5s_enable = 0, count_5s_enable = 0, count_5s1_enable = 0, count_5s2_enable = 0, count_1min_enable = 0;

extern MC_ALL_FRAME MC_All_Frame_1;
extern UINT32 MC_Read_Watchdog;
extern UINT32 MC_Manage_Watchdog_1;
extern UINT32 Dc_shunt_Set_CL_flag_1;
extern MC_ALL_FRAME MC_All_Frame_2;
//初始化CAN通信的文件描述符
int BMS_can_fd_init(unsigned char *can_id)
{
    struct ifreq ifr;
    struct sockaddr_can addr;
    int s;
    s = socket(PF_CAN, SOCK_RAW, CAN_RAW);

    if (s < 0)
    {
        perror("can socket");
        return (-1);
    }

    strcpy(ifr.ifr_name, can_id);

    if (ioctl(s, SIOCGIFINDEX, &ifr))
    {
        perror("can ioctl");
        return (-1);
    }

    addr.can_family = PF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("can bind");
        return (-1);
    }

    //原始套接字选项 CAN_RAW_LOOPBACK 为了满足众多应用程序的需要，本地回环功能默认是开启的, 但是在一些嵌入式应用场景中（比如只有一个用户在使用CAN总线），回环功能可以被关闭（各个套接字之间是独立的）
    int loopback = 0; // 0 = disabled, 1 = enabled (default)
    setsockopt(s, SOL_CAN_RAW, CAN_RAW_LOOPBACK, &loopback, sizeof(loopback));
    return (s);
}

static void MC_Read_Task(void)
{
    int fd;
    struct can_filter rfilter[2];
    int i, Ret;
    fd_set rfds;
    struct timeval tv;
    struct can_frame frame;

    if ((fd = BMS_can_fd_init(GUN_CAN_COM_ID1)) < 0)
    {
        perror("BMS_Read_Task BMS_can_fd_init GUN_CAN_COM_ID1");

        while (1);
    }

    //<received_can_id> & mask == can_id
    rfilter[0].can_id   = 0x00008AF6;
    rfilter[0].can_mask = 0x0000FFFF;
    rfilter[1].can_id   = 0x00008AF7;
    rfilter[1].can_mask = 0x0000FFFF;

    if (setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter)) != 0)
    {
        perror("setsockopt filter");

        while (1);
    }

    make_CRC32Table();//生产CRC校验码
    prctl(PR_SET_NAME, (unsigned long)"MC_Read_Task"); //设置线程名字

    while (1)
    {
        usleep(5000);//1ms
        MC_Read_Watchdog = 1;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = 10000;//10ms
        Ret = select(fd + 1, &rfds, NULL, NULL, &tv);

        if (Ret < 0)
        {
            perror("can raw socket read ");
            usleep(100000);//100ms
        }
        else if (Ret > 0)
        {
            if (FD_ISSET(fd, &rfds))
            {
                Ret = read(fd, &frame, sizeof(frame));

                if (Ret == sizeof(frame))
                {
                    if (frame.data[0] == 1)
                    {
                        MC_read_deal_1(frame);//1号控制板发过来的数据帧处理
                    }
                    else
                    {
                        if ((Globa_1->Charger_param.System_Type <= 1) || (Globa_1->Charger_param.System_Type == 4)) //双枪的时候{//同时充电和轮流充电才运行
                        {
                            MC_read_deal_2(frame);//2号控制板发过来的数据帧处理
                        }
                    }
                }
            }
        }
    }

    close(fd);
    exit(0);
}

/*********************************************************
**description：BMS读线程初始化及建立
***parameter  :none
***return         :none
**********************************************************/
extern void MC_Read_Thread(void)
{
    pthread_t td1;
    int ret, stacksize = 1024 * 1024;
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

    /* 创建自动串口抄收线程 */
    if (0 != pthread_create(&td1, &attr, (void *)MC_Read_Task, NULL))
    {
        perror("####pthread_create IC_Read_Write_Task ERROR ####\n\n");
    }

    ret = pthread_attr_destroy(&attr);

    if (ret != 0)
    {
        return;
    }
}

extern void timer_start_1(int time)
{
    switch (time)
    {
    case 250:
    {
        count_250ms = 0;
        count_250ms_flag = 0;
        count_250ms_enable = 1;
        break;
    }

    case 350:
    {
        count_350ms = 0;
        count_350ms_flag = 0;
        count_350ms_enable = 1;
        break;
    }

    case 500:
    {
        count_500ms = 0;
        count_500ms_flag = 0;
        count_500ms_enable = 1;
        break;
    }

    case 650:
    {
        count_650ms = 0;
        count_650ms_flag = 0;
        count_650ms_enable = 1;
        break;
    }

    case 1000:
    {
        count_1s = 0;
        count_1s_flag = 0;
        count_1s_enable = 1;
        break;
    }

    case 3000:
    {
        count_3s_or5s = 0;
        count_3s_or5s_flag = 0;
        count_3s_or5s_enable = 1;
        break;
    }

    case 5000:
    {
        count_5s = 0;
        count_5s_flag = 0;
        count_5s_enable = 1;
        //  printf("5000开始计时\n");
        break;
    }

    case 5001:
    {
        count_5s1 = 0;
        count_5s1_flag = 0;
        count_5s1_enable = 1;
        break;
    }

    case 5002:
    {
        count_5s2 = 0;
        count_5s2_flag = 0;
        count_5s2_enable = 1;
        break;
    }

    case 60000:
    {
        count_1min = 0;
        count_1min_flag = 0;
        count_1min_enable = 1;
        break;
    }

    default:
    {
        break;
    }
    }
}
extern void timer_stop_1(int time)
{
    switch (time)
    {
    case 250:
    {
        count_250ms = 0;
        count_250ms_flag = 0;
        count_250ms_enable = 0;
        break;
    }

    case 350:
    {
        count_350ms = 0;
        count_350ms_flag = 0;
        count_350ms_enable = 0;
        break;
    }

    case 500:
    {
        count_500ms = 0;
        count_500ms_flag = 0;
        count_500ms_enable = 0;
        break;
    }

    case 650:
    {
        count_650ms = 0;
        count_650ms_flag = 0;
        count_650ms_enable = 0;
        break;
    }

    case 1000:
    {
        count_1s = 0;
        count_1s_flag = 0;
        count_1s_enable = 0;
        break;
    }

    case 3000:
    {
        count_3s_or5s = 0;
        count_3s_or5s_flag = 0;
        count_3s_or5s_enable = 0;
        break;
    }

    case 5000:
    {
        count_5s = 0;
        count_5s_flag = 0;
        count_5s_enable = 0;
        break;
    }

    case 5001:
    {
        count_5s1 = 0;
        count_5s1_flag = 0;
        count_5s1_enable = 0;
        break;
    }

    case 5002:
    {
        count_5s2 = 0;
        count_5s2_flag = 0;
        count_5s2_enable = 0;
        break;
    }

    case 60000:
    {
        count_1min = 0;
        count_1min_flag = 0;
        count_1min_enable = 0;
        break;
    }

    default:
    {
        break;
    }
    }
}
extern UINT32 timer_tick_1(int time)
{
    UINT32 flag = 0;

    switch (time)
    {
    case 250:
    {
        flag = count_250ms_flag;
        break;
    }

    case 350:
    {
        flag = count_350ms_flag;
        break;
    }

    case 500:
    {
        flag = count_500ms_flag;
        break;
    }

    case 650:
    {
        flag = count_650ms_flag;
        break;
    }

    case 1000:
    {
        flag = count_1s_flag;
        break;
    }

    case 3000:
    {
        flag = count_3s_or5s_flag;
        break;
    }

    case 5000:
    {
        flag = count_5s_flag;
        break;
    }

    case 5001:
    {
        flag = count_5s1_flag;
        break;
    }

    case 5002:
    {
        flag = count_5s2_flag;
        break;
    }

    case 60000:
    {
        flag = count_1min_flag;
        break;
    }

    default:
    {
        break;
    }
    }

    return (flag);
}

/*********************************************************
**description：MC管理线程
***parameter  :none
***return         :none
**********************************************************/
static void MC_Manage_Task_1(void)
{
    INT32 fd;
    FILE *fp;
    UINT32 temp1 = 0xff, temp2 = 0xff, GROUP1_2_RUN_LED_falg = 0;
    UINT32 Error_equal = 0, ctrl_connect_count = 0;
    time_t systime = 0, last_systime = 0;
    UINT32 AC_Relay_Ctl_Count = 0, AC_Relay_Ctl_Count_falg = 0;
    UINT32 time_count = 1;
    UINT32 Operation_command_cont = 0;//为了兼容以前的，最多连续发送10次
    timer_stop_1(250);
    timer_stop_1(500);//控制灯闪烁
    timer_stop_1(1000);//启动电量累计功能
    timer_stop_1(3000);
    timer_stop_1(5000);
    timer_stop_1(5001);//充电控制器故障统计计时
    timer_start_1(5002);//遥测帧计时
    timer_stop_1(60000);
    timer_stop_1(350);//控制灯闪烁
    timer_stop_1(650);//电子锁解锁
    timer_start_1(350);
    Globa_1->charger_start = 0;//充电停止
    GROUP1_RUN_LED_OFF;
    GROUP2_RUN_LED_OFF;
    Globa_1->Error_charger = 0;//充电错误尝试次数
    Globa_1->Time = 0;//充电时间
    Globa_1->SYS_Step = 0x00;

    if ((fd = BMS_can_fd_init(GUN_CAN_COM_ID1)) < 0)
    {
        while (1);
    }

    //在指定的CAN_RAW套接字上禁用接收过滤规则,则该线程只发送，不会接收数据，省内存和CPU时间
    setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FILTER, NULL, 0);
    prctl(PR_SET_NAME, (unsigned long)"MC1_Manage_Task"); //设置线程名字

    while (1)
    {
        MC_Manage_Watchdog_1 = 1;

        if (time_count == 5)
        {
            usleep(10000);//做基准时钟调度10ms
        }
        else
        {
            time_count = 1;
            usleep(50000);//做基准时钟调度10ms
        }

        if (count_250ms_enable == 1) //使能250MS 计时
        {
            if (count_250ms_flag != 1) //计时时间未到
            {
                count_250ms++;

                if (count_250ms >= 5 * time_count)
                {
                    count_250ms_flag = 1;
                }
            }
        }

        if (count_350ms_enable == 1) //使能250MS 计时
        {
            if (count_350ms_flag != 1) //计时时间未到
            {
                count_350ms++;

                if (count_350ms >= 7 * time_count)
                {
                    count_350ms_flag = 1;
                }
            }
        }

        if (count_500ms_enable == 1) //使能500MS 计时
        {
            if (count_500ms_flag != 1) //计时时间未到
            {
                count_500ms++;

                if (count_500ms >= 10 * time_count)
                {
                    Globa_1->led_run_flag = !Globa_1->led_run_flag;
                    count_500ms_flag = 1;
                }
            }
        }

        if (count_650ms_enable == 1) //使能650MS 计时
        {
            if (count_650ms_flag != 1) //计时时间未到
            {
                count_650ms++;

                if (count_650ms >= 13 * time_count)
                {
                    count_650ms_flag = 1;
                }
            }
        }

        if (count_1s_enable == 1) //使能 1秒 计时
        {
            if (count_1s_flag != 1) //计时时间未到
            {
                systime = time(NULL);        //获得系统的当前时间,从1970.1.1 00:00:00 到现在的秒数

                if (systime != last_systime)
                {
                    last_systime = systime;
                    count_1s_flag = 1;
                }
            }
        }

        if (count_3s_or5s_enable == 1) //使能 3秒 计时 --
        {
            if (count_3s_or5s_flag != 1) //计时时间未到
            {
                count_3s_or5s++;

                if (count_3s_or5s >= 200 * time_count) //600
                {
                    count_3s_or5s_flag = 1;
                }
            }
        }

        if (count_5s_enable == 1) //使能 5秒 计时
        {
            if (count_5s_flag != 1) //计时时间未到
            {
                count_5s++;

                //printf("遥测响应帧 超过5秒 count_5s =%d \n",count_5s);
                if (count_5s >= 100 * time_count)
                {
                    count_5s_flag = 1;
                }
            }
        }

        if (count_5s1_enable == 1) //使能 5秒 计时
        {
            if (count_5s1_flag != 1) //计时时间未到
            {
                count_5s1++;

                if (count_5s1 >= 100 * time_count)
                {
                    count_5s1_flag = 1;
                }
            }
        }

        if (count_5s2_enable == 1) //使能 5秒 计时
        {
            if (count_5s2_flag != 1) //计时时间未到
            {
                count_5s2++;

                if (count_5s2 >= 100 * time_count)
                {
                    count_5s2_flag = 1;
                }
            }
        }

        if (count_1min_enable == 1) //使能 60秒 计时
        {
            if (count_1min_flag != 1) //计时时间未到
            {
                count_1min++;

                if (count_1min >= 1200 * time_count)
                {
                    count_1min_flag = 1;
                }
            }
        }

        if (Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setflg == 1) //进行升级标志
        {
            time_count = 5;

            switch (Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setStep)
            {
            case 0x00:
            {
                if (MC_All_Frame_1.MC_Recved_PGN21248_Flag == 1) //升级请求报文响应报文
                {
                    MC_All_Frame_1.MC_Recved_PGN21248_Flag = 0;
                    timer_start_1(5000);
                    //printf("11111下升级请求报文响应报文\n");
                    Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x01;
                    Globa_2->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x01;
                    Globa_1->Control_Upgrade_Firmware_Info.Send_Tran_All_Bytes_0K = 0; //重新计数
                    break;
                }

                if (timer_tick_1(5000) == 1) //遥测响应帧 超过5秒
                {
                    timer_stop_1(5000);
                    Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x10;
                    Globa_2->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x10;
                    Globa_1->Control_Upgrade_Firmware_Info.firmware_info_success = 1;
                    Globa_2->Control_Upgrade_Firmware_Info.firmware_info_success = 1;
                    //printf("遥测响应帧 超过5秒\n");
                    break;
                }

                if (timer_tick_1(250) == 1) //下发升级请求报文
                {
                    timer_start_1(250);
                    MC_sent_data_1(fd, PGN20992_ID_MC, 1, 1);  //下发升级请求报文
                    Globa_1->Control_Upgrade_Firmware_Info.Send_Tran_All_Bytes_0K = 0; //重新计数
                    Globa_1->Control_Upgrade_Firmware_Info.firmware_info_success = 0;
                    Globa_2->Control_Upgrade_Firmware_Info.firmware_info_success = 0;
                    //printf("11111下发升级请求报文-------\n");
                }

                break;
            }

            case 0x01:
            {
                if (MC_All_Frame_1.MC_Recved_PGN21504_Flag == 1) //请求升级数据报文帧
                {
                    MC_All_Frame_1.MC_Recved_PGN21504_Flag = 0;
                    timer_start_1(5000);
                    timer_start_1(250);
                    MC_sent_data_1(fd, PGN21760_ID_MC, 0, 1);
                }

                if (MC_All_Frame_1.MC_Recved_PGN22016_Flag == 1) //请求升级数据报文帧
                {
                    MC_All_Frame_1.MC_Recved_PGN22016_Flag = 0;
                    timer_start_1(5000);
                    MC_sent_data_1(fd, PGN21760_ID_MC, 1, 1);
                    Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x02;
                    Globa_2->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x02;
                    timer_start_1(250);
                }

                if (timer_tick_1(5000) == 1) //遥测响应帧 超过5秒
                {
                    timer_stop_1(5000);
                    Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x10;
                    Globa_2->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x10;
                    Globa_1->Control_Upgrade_Firmware_Info.firmware_info_success = 1;
                    Globa_2->Control_Upgrade_Firmware_Info.firmware_info_success = 1;
                    break;
                }

                break;
            }

            case 0x02: //升级包文件下发
            {
                if (MC_All_Frame_1.MC_Recved_PGN22016_Flag == 1) //请求升级数据报文帧
                {
                    MC_All_Frame_1.MC_Recved_PGN22016_Flag = 0;
                    timer_start_1(5000);
                    timer_start_1(250);
                    MC_sent_data_1(fd, PGN21760_ID_MC, 1, 1);
                }

                if (timer_tick_1(5000) == 1) //遥测响应帧 超过5秒
                {
                    timer_stop_1(5000);
                    Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x10;
                    Globa_2->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x10;
                    Globa_1->Control_Upgrade_Firmware_Info.firmware_info_success = 1;
                    Globa_2->Control_Upgrade_Firmware_Info.firmware_info_success = 1;
                    break;
                }
                else if (timer_tick_1(250) == 1) //如果下面控制板还没有
                {
                    timer_start_1(250);
                    MC_sent_data_1(fd, PGN21760_ID_MC, 1, 1);
                    break;
                }

                break;
            }

            case 0x03: //第二块板子进行升级
            {
                if (MC_All_Frame_2.MC_Recved_PGN21248_Flag == 1) //升级请求报文响应报文
                {
                    MC_All_Frame_2.MC_Recved_PGN21248_Flag = 0;
                    timer_start_1(5000);
                    Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x04;
                    Globa_2->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x04;
                    Globa_2->Control_Upgrade_Firmware_Info.Send_Tran_All_Bytes_0K = 0; //重新计数
                    break;
                }

                if (timer_tick_1(5000) == 1) //遥测响应帧 超过5秒
                {
                    timer_stop_1(5000);
                    Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x10;
                    Globa_2->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x10;
                    Globa_2->Control_Upgrade_Firmware_Info.firmware_info_success = 1;
                    break;
                }

                if (timer_tick_1(250) == 1) //下发升级请求报文
                {
                    timer_start_1(250);
                    MC_sent_data_2(fd, PGN20992_ID_MC, 1, 2);  //下发升级请求报文
                    Globa_2->Control_Upgrade_Firmware_Info.Send_Tran_All_Bytes_0K = 0; //重新计数
                    Globa_2->Control_Upgrade_Firmware_Info.firmware_info_success = 0;
                }

                break;
            }

            case 0x04:
            {
                if (MC_All_Frame_2.MC_Recved_PGN21504_Flag == 1) //请求升级数据报文帧
                {
                    MC_All_Frame_2.MC_Recved_PGN21504_Flag = 0;
                    timer_start_1(5000);
                    timer_start_1(250);
                    MC_sent_data_2(fd, PGN21760_ID_MC, 0, 2);
                }

                if (MC_All_Frame_2.MC_Recved_PGN22016_Flag == 1) //请求升级数据报文帧
                {
                    MC_All_Frame_2.MC_Recved_PGN22016_Flag = 0;
                    timer_start_1(5000);
                    MC_sent_data_2(fd, PGN21760_ID_MC, 1, 2);
                    Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x05;
                    Globa_2->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x05;
                    timer_start_1(250);
                }

                if (timer_tick_1(5000) == 1) //遥测响应帧 超过5秒
                {
                    timer_stop_1(5000);
                    Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x10;
                    Globa_2->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x10;
                    Globa_2->Control_Upgrade_Firmware_Info.firmware_info_success = 1;
                    break;
                }

                break;
            }

            case 0x05: //升级包文件下发
            {
                if (MC_All_Frame_2.MC_Recved_PGN22016_Flag == 1) //请求升级数据报文帧
                {
                    MC_All_Frame_2.MC_Recved_PGN22016_Flag = 0;
                    timer_start_1(5000);
                    timer_start_1(250);
                    MC_sent_data_2(fd, PGN21760_ID_MC, 1, 2);
                }

                if (timer_tick_1(5000) == 1) //遥测响应帧 超过5秒
                {
                    timer_stop_1(5000);
                    Globa_1->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x10;
                    Globa_2->Control_Upgrade_Firmware_Info.firmware_info_setStep = 0x10;
                    Globa_2->Control_Upgrade_Firmware_Info.firmware_info_success = 1;
                    break;
                }
                else if (timer_tick_1(250) == 1) //如果下面控制板还没有
                {
                    timer_start_1(250);
                    MC_sent_data_2(fd, PGN21760_ID_MC, 1, 2);
                    break;
                }

                break;
            }

            case 0x10: //升级完毕，不管是失败还是成功
            {
                break;
            }

            default:
                break;
            }
        }
        else
        {
            time_count = 1;

            if (((Globa_1->Error_ctl == 0x00) && (Globa_1->ctl_state == 0x00)) || ((Globa_1->Error_ctl != 0x00) && (Globa_1->ctl_state == 0x01))) //判断计量单元统计的控制器的故障数量是否与控制器的状态一致
            {
                Error_equal = 0;
                timer_stop_1(5001);
            }
            else
            {
                if (Error_equal == 0)
                {
                    Error_equal = 1;
                    timer_start_1(5001);
                }
            }

            /*if(timer_tick_1(5001) == 1){//超时5秒
                memset(&Globa_1->Error.emergency_switch, 0x00, (&Globa_1->Error.reserve30 - &Globa_1->Error.emergency_switch));//为了保持与计量单元的故障记录计数同步，把标志全部清零
                //Globa_1->Error_ctl = 0;

                system("echo Globa_1->Error_ctl != 0x00)&&(Globa_1->ctl_state != 0x11)) >> /mnt/systemlog");
                sleep(3);
                system("reboot");
            }*/

            if ((MC_All_Frame_1.MC_Recved_PGN8704_Flag == 0x01) || (MC_All_Frame_1.MC_Recved_PGN8448_Flag == 1)) //遥测帧
            {
                if (MC_All_Frame_1.MC_Recved_PGN8704_Flag == 1)
                {
                    MC_All_Frame_1.MC_Recved_PGN8704_Flag = 0;
                    MC_sent_data_1(fd, PGN8960_ID_MC, 1, 1);  //遥测响应帧

                    if ((Globa_1->line_v > 32) && (Globa_1->line_v < 48))
                    {
                        Globa_1->gun_link = 1;
                    }
                    else
                    {
                        Globa_1->gun_link = 0;
                    }
                }

                MC_All_Frame_1.MC_Recved_PGN8448_Flag = 0;
                timer_start_1(5002);
                ctrl_connect_count = 0;

                if (Globa_1->Error.ctrl_connect_lost != 0)
                {
                    Globa_1->Error.ctrl_connect_lost = 0;

                    //sent_warning_message(0x98, 53, 1, 0);
                    if ((Globa_1->Charger_param.System_Type == 0) || (Globa_1->Charger_param.System_Type == 4)) //双枪的时候
                    {
                        sent_warning_message(0x98, 53, 1, 0);
                    }
                    else
                    {
                        sent_warning_message(0x98, 53, 0, 0);
                    }

                    Globa_1->Error_system--;
                }
            }
            else if (timer_tick_1(5002) == 1) //超时5秒
            {
                ctrl_connect_count++;
                timer_start_1(5002);

                if (ctrl_connect_count >= 2)
                {
                    if (Globa_1->Error.ctrl_connect_lost != 1)
                    {
                        Globa_1->Error.ctrl_connect_lost = 1;

                        if ((Globa_1->Charger_param.System_Type == 0) || (Globa_1->Charger_param.System_Type == 4))
                        {
                            sent_warning_message(0x99, 53, 1, 0);
                        }
                        else
                        {
                            sent_warning_message(0x99, 53, 0, 0);
                        }

                        Globa_1->Error_system++;
                    }

                    ctrl_connect_count = 0;
                }
            }

            if (MC_All_Frame_1.MC_Recved_PGN2816_Flag == 0x01) //请求充电参数信息
            {
                MC_All_Frame_1.MC_Recved_PGN2816_Flag = 0;
                MC_sent_data_1(fd, PGN2304_ID_MC, 1, 1);  //下发充电参数信息
            }

            if (Globa_1->Operation_command == 2) //进行解锁动作
            {
                if (MC_All_Frame_1.MC_Recved_PGN3840_Flag == 1) //收到下发电子锁控制反馈
                {
                    timer_stop_1(650);
                    MC_All_Frame_1.MC_Recved_PGN3840_Flag = 0;
                    Globa_1->Operation_command = 0;
                    Operation_command_cont = 0;//为了兼容以前的，最多连续发送10次
                }
                else
                {
                    if (timer_tick_1(650) == 1)  //定时350MS
                    {
                        MC_sent_data_1(fd, PGN3584_ID_MC, 2, 1);  //下发充电参数信息
                        timer_start_1(650);
                        Operation_command_cont++;

                        if (Operation_command_cont >= 10)
                        {
                            Operation_command_cont = 0;
                            Globa_1->Operation_command = 0;
                            timer_stop_1(650);
                            MC_All_Frame_1.MC_Recved_PGN3840_Flag = 0;
                        }
                    }
                }
            }
            else
            {
                timer_start_1(650);
            }

            switch (Globa_1->SYS_Step)
            {
            case 0x00: //---------------------------系统空闲状态00 -------------------
            {
                if (GROUP1_2_RUN_LED_falg == 1)
                {
                    GROUP2_RUN_LED_OFF;
                    GROUP1_2_RUN_LED_falg   = 0;
                }

                GROUP1_RUN_LED_OFF;
                timer_start_1(350);//下发VIN
                timer_start_1(250);
                timer_stop_1(500);
                timer_stop_1(1000);
                timer_stop_1(3000);
                timer_stop_1(5000);
                timer_stop_1(60000);
                Globa_1->Error_charger = 0;//充电错误尝试次数
                Globa_1->SYS_Step = 0x01;
                Globa_1->soft_KWH  = 0;
                break;
            }

            case 0x01: //--------------------系统就绪状态01---------------------------
            {
                if ((Globa_1->charger_start == 1) || (Globa_1->charger_start == 2)) //启动自动充电或手动充电
                {
                    timer_stop_1(250);
                    timer_start_1(500);//控制灯闪烁
                    timer_start_1(1000);//启动电量累计功能
                    Globa_1->SYS_Step = 0x11;
                    Globa_1->MC_Step  = 0x01;
                    timer_start_1(350);//控制交流采集盒继电器
                    Globa_1->gun_link_over = 0x01;
                    //if((Globa_1->Double_Gun_To_Car_falg == 1)&&(Globa_1->Charger_param.System_Type == 4)){//表示启动双枪模式：
                    //    GROUP1_2_RUN_LED_falg = 1;
                    //}
                }

                if (MC_All_Frame_1.MC_Recved_PGN3328_Flag == 1) //收到下发直流分流器量程变比的反馈信息
                {
                    Dc_shunt_Set_CL_flag_1 = 0;
                    MC_All_Frame_1.MC_Recved_PGN3328_Flag = 0;
                }
                else
                {
                    if (Dc_shunt_Set_CL_flag_1 == 1) //下发CT变比
                    {
                        if (timer_tick_1(250) == 1)  //定时250MS
                        {
                            MC_sent_data_1(fd, PGN3072_ID_MC, 1, 1);  //下发充电参数信息
                            timer_start_1(250);
                        }
                    }
                }

                break;
            }

            case 0x11: //--------------------系统充电状态 ----------------------------
            {
                if (timer_tick_1(1000) == 1) //定时1秒
                {
                    if (Globa_1->Charger_param.System_Type == 3) //壁挂是没有电表的
                    {
                        Globa_1->soft_KWH  += (Globa_1->Output_voltage / 100) * (Globa_1->Output_current / 100) / 36000; // 1s  累计一次 电量度  4位小数
                        Globa_1->meter_KWH = Globa_1->soft_KWH / 100;
                    }

                    Globa_1->Time += 1;
                    timer_start_1(1000);
                }

                if (timer_tick_1(500) == 1)  //定时500MS
                {
                    timer_start_1(500);

                    if (Globa_1->led_run_flag != 0) //点亮（灯闪烁）
                    {
                        /* if(GROUP1_2_RUN_LED_falg == 1){
                           GROUP2_RUN_LED_ON;
                         }*/
                        GROUP1_RUN_LED_ON;
                    }
                    else
                    {
                        GROUP1_RUN_LED_OFF;
                        /*if(GROUP1_2_RUN_LED_falg == 1){
                            GROUP2_RUN_LED_OFF;
                        }*/
                    }
                }

                MC_control_1(fd);//充电应用逻辑控制（包括与控制器的通信、刷卡计费的处理、上层系统故障的处理）
                break;
            }

            case 0x21: //--------------------充电结束状态处理 ------------------------
            {
                Globa_1->charger_start = 0;//充电停止
                Globa_1->SYS_Step = 0x00;
                printf("-TTTTST---mccan--停止完成帧-OVER------\n");
                break;
            }

            default:
            {
                Globa_1->charger_start = 0;//充电停止
                GROUP1_RUN_LED_OFF;

                while (1);

                break;
            }
            }
        }
    }

    close(fd);
    exit(0);
}

/*********************************************************
**description：MC管理线程初始化及建立
***parameter  :none
***return         :none
**********************************************************/
extern void     MC_Manage_Thread_1(void)
{
    pthread_t td1;
    int ret, stacksize = 1024 * 1024;
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

    /* 创建自动串口抄收线程 */
    if (0 != pthread_create(&td1, &attr, (void *)MC_Manage_Task_1, NULL))
    {
        perror("####pthread_create IC_Read_Write_Task ERROR ####\n\n");
    }

    ret = pthread_attr_destroy(&attr);

    if (ret != 0)
    {
        return;
    }
}
