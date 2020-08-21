/****************************************Copyright (c)****************************************************
**                               DongGuan EAST Co.,LTD.
**                                     
**                                 http://www.EASTUPS.com
**
**--------------File Info---------------------------------------------------------------------------------
** File Name:               Time_Utity.h 
** Last modified Date:      2014.3.20
** Last Version:            1.0
** Description:             与日历时间和秒计数器相关的时间功能函数，时间格式全部按二进制，而不是BCD码 
** 
**--------------------------------------------------------------------------------------------------------
** Created By:              
** Created date:            2014.3.20
** Version:                 1.0
** Descriptions:            The original version 初始版本
**
**--------------------------------------------------------------------------------------------------------
*********************************************************************************************************/

#ifndef  __TIME_UTITY_H__
#define  __TIME_UTITY_H__

//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
	unsigned char year;//年 since 2000,如值为12表示实际为2012年	
	unsigned char month;//月 1---12
	unsigned char day_of_month;//某月中的某日，如3月2日, 1---31
	unsigned char week_day;//星期几 1---7  7-sunday,1-monday....6-saturday	----STM32F207使用值7表示星期天
	unsigned char hour;//小时0---23
	unsigned char minute;//分钟 0---59
	unsigned char second;//秒 0---59
	
}BIN_TIME;	//数值为二进制格式		  

typedef struct
{
	unsigned char year;//年的十位和个位	
	unsigned char month;//月 1---12
	unsigned char day_of_month;//某月中的某日，如3月2日, 1---31
	unsigned char week_day;//星期几 0---6  0-sunday,1-monday....6-saturday
	unsigned char hour;//小时0---23
	unsigned char minute;//分钟 0---59
	unsigned char second;//秒 0---59
}BCD_TIME;//PCF8563的RTC时间格式,注意都为BCD格式

typedef unsigned char tBoolean;

#ifndef false
#define false 0
#endif 

#ifndef true
#define true 1
#endif 

#define   YEAR_OFFSET    2000//年从2000偏移开始

//返回RTC_TIME表示的时间与2000年1月1日0点0分0秒相距的秒数
uint32_t RTC_TIME2Secs(BIN_TIME* new_time);
//将输入的与2000年1月1日0点0分0秒相距的秒数转换为对应的RTC时间
BIN_TIME  Secs2RTC_TIME(uint32_t  TotSeconds);

//1年中的第几天转为该年的几月几日
//year为年的二进制值，如2014
void YearDaysConvert(unsigned short year,unsigned short day_of_year,unsigned char* p_month,unsigned char* p_day_of_month);
//根据蔡勒公式计算某年的某月某日是星期几，返回值0--星期天，1--星期一...6--星期六
//参数为二进制进制格式，不是BCD格式,year可达公元65535年---
unsigned char day2weekday(unsigned short year,unsigned char month,unsigned char day_of_month);

//计算明天是哪年哪月哪日,返回值0表示转换失败，值1表示转换成功
unsigned char getnextday(BIN_TIME today,BIN_TIME* tomorrow);

//判断一个RTC_TIME的值是否为有效---未判断weekday
//有效返回真，否则返回假
tBoolean RTC_Time_Valid_Check(BIN_TIME* new_time);
//将RTC时间转换为字符串形式的时间，如"2014-07-17 10:10:10"
//注意输出参数buf所指向的缓冲区必须足够大，至少20字节
void Time2Str(char* buf, BIN_TIME* new_time);
tBoolean Str2Time(char* buf, BIN_TIME* new_time);//将形如"2014-07-17 10:10:10"的时间转换为RTC时间
tBoolean Str2Time2(char* buf, BIN_TIME* new_time);//将形如"20140717101010"的时间转换为RTC时间

void BCD_Time2BIN_Time(BCD_TIME bcd_time, BIN_TIME* bin_time);
void BIN_Time2BCD_Time(BIN_TIME bin_time, BCD_TIME* bcd_time);
//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif
#endif  //  __TIME_UTITY_H__

