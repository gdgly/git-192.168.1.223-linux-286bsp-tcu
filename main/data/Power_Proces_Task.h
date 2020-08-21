#ifndef POWER_PROCES_TASK_H
#define POWER_PROCES_TASK_H
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <termio.h>
#include <sys/time.h>
#include <time.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <sys/shm.h> 
#include "globalvar.h"
enum POWER_PROCES_STEP
{
	POWER_PROCES_IDLE  = 0,//空闲状态
	POWER_PROCES_ACTIVATION  = 1,//激活状态
	POWER_PROCES_SWITCHING = 2,//投切处理
	POWER_PROCES_EXIT = 3//退出处理
};
enum SWITCHING_STEP
{
	SWITCHING_IDLE  = 0,//空闲状态
	SWITCHING_INIT  = 1,//初始化状态
	SWITCHING_PREPAR  = 2,//投切准备
	SWITCHING_PRECHARGE  = 3,//投切预充
	SWITCHING_CONTROL_TRANSFER  = 4,//投切控制权移交
	SWITCHING_END  = 5,//投切完成
	SWITCHING_TIMEOUT= 6//投切c超时
};
enum EXIT_POWER_STEP
{
	EXIT_POWER_IDLE  = 0,//空闲状态
  EXIT_POWER_INIT  = 1,//初始化状态
	EXIT_POWER_OFF = 2,//关闭相应模块及接触器
	EXIT_POWER_CONTROL_TRANSFER  = 3,//投切控制权移交
	EXIT_POWER_END  = 4//退出完毕
};

void Power_Proces_Task(int Grup_no);

#endif // CHARGE_PROCES_TASK_H


