/****************************************Copyright (c)****************************************************
**                               DongGuan EAST Co.,LTD.
**                                     
**                                 http://www.EASTUPS.com
**
**--------------File Info---------------------------------------------------------------------------------
** File Name:               Time_Utity.c 
** Last modified Date:      2014.3.20
** Last Version:            1.0
** Description:             ������ʱ������������ص�ʱ�书�ܺ�����ʱ���ʽȫ���������ƣ�������BCD��
** 
**--------------------------------------------------------------------------------------------------------
** Created By:              
** Created date:            2014.3.20
** Version:                 1.0
** Descriptions:            The original version ��ʼ�汾
**
**--------------------------------------------------------------------------------------------------------
*********************************************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "Time_Utity.h"

//һ����ÿ������
static uint8_t days_per_month[2][13] = { {0,31,28,31,30,31,30,31,31,30,31,30,31},//not leap year
								   {0,31,29,31,30,31,30,31,31,30,31,30,31}};//leap year
										  
static uint16_t Month_Days_Accu_C[13] = {0,31,59,90,120,151,181,212,243,273,304,334,365};//ƽ�������ۻ�������
static uint16_t Month_Days_Accu_L[13] = {0,31,60,91,121,152,182,213,244,274,305,335,366};//���������ۻ�������

#define SecsPerDay      	86400//(3600*24)
#define SecsPerComYear  	365*SecsPerDay//(365*3600*24)//ƽ��ÿ�������
#define SecsPerLeapYear 	366*SecsPerDay//(366*3600*24)//����ÿ�������
#define SecsPerFourYear 	126230400//((365*3600*24)*3+(366*3600*24))


//2��32�η���������������Ա�ʾԼ136�꣬�����ƫ�ƴ�2000�꿪ʼ��Ҳ���ǿ��Ա�ʾ2000�굽2136��
//����BIN_TIME��ʾ��ʱ����2000��1��1��0��0��0����������
uint32_t RTC_TIME2Secs(BIN_TIME* new_time)
{
	uint32_t LeapY, ComY, TotSeconds, TotDays;	
	
	if(new_time -> year == 0)//ƫ��0��ʾ2000��
	  LeapY = 0;
	else
	  LeapY = (new_time -> year - 1)/4 + 1;

	ComY = (new_time -> year) -(LeapY);

	if (new_time -> year % 4)//common year		
		TotDays = LeapY*366 + ComY*365 + Month_Days_Accu_C[new_time -> month-1] + (new_time -> day_of_month-1); 
	else//leap year		
		TotDays = LeapY*366 + ComY*365 + Month_Days_Accu_L[new_time -> month-1] + (new_time -> day_of_month-1); 

	TotSeconds = TotDays*SecsPerDay + (new_time -> hour*3600 + new_time -> minute*60 + new_time -> second);
	
	return TotSeconds;
}

uint32_t Year_Secs_Accu[5]={0,
                      31622400,
                      63158400,
                      94694400,
                      126230400};

uint32_t Month_Secs_Accu_C[13] = { 0,
                            2678400,
                            5097600,
                            7776000,
                            10368000,
                            13046400,
                            15638400,
                            18316800,
                            20995200,
                            23587200,
                            26265600,
                            28857600,
                            31536000};
uint32_t Month_Secs_Accu_L[13] = {0,
                            2678400,
                            5184000,
                            7862400,  
                            10454400,
                            13132800,
                            15724800,
                            18403200,
                            21081600,
                            23673600,
                            26352000,
                            28944000,
                            31622400};
//���������2000��1��1��0��0��0����������ת��Ϊ��Ӧ��RTCʱ��
BIN_TIME  Secs2RTC_TIME(uint32_t  TotSeconds)
{
	BIN_TIME  new_time;
	uint32_t TY = 0, TM = 1, TD = 0;
	int32_t Num4Y,NumY, OffSec, Off4Y = 0;
	uint32_t i;
	int32_t NumDay; //OffDay;

	uint32_t THH = 0, TMM = 0, TSS = 0;

	Num4Y = TotSeconds/SecsPerFourYear;
	OffSec = TotSeconds%SecsPerFourYear;

	i=1;
	while(OffSec > Year_Secs_Accu[i++])
	  Off4Y++;

	/* Numer of Complete Year */
	NumY = Num4Y*4 + Off4Y;
	  /* 2000,2001,...~2000+NumY-1 complete year before, so this year is 2000+NumY*/
	//TY = 2000+NumY;
	TY = NumY;

	OffSec = OffSec - Year_Secs_Accu[i-2];

	/* Month (TBD with OffSec)*/
	i=0;
	if(TY%4)
	{// common year
	  while(OffSec > Month_Secs_Accu_C[i++]);
	  TM = i-1;
	  OffSec = OffSec - Month_Secs_Accu_C[i-2];
	}
	else
	{// leap year
	  while(OffSec > Month_Secs_Accu_L[i++]);
	  TM = i-1;
	  OffSec = OffSec - Month_Secs_Accu_L[i-2];
	}

	/* Date (TBD with OffSec) */
	NumDay = OffSec/SecsPerDay;
	OffSec = OffSec%SecsPerDay;
	TD = NumDay+1;

	/* Compute  hours */
	THH = OffSec/3600;
	/* Compute minutes */
	TMM = (OffSec % 3600)/60;
	/* Compute seconds */
	TSS = (OffSec % 3600)% 60;		
	
	new_time.second 	= TSS;
	new_time.minute 	= TMM;
	new_time.hour   	= THH;
	new_time.day_of_month   	= TD;
	new_time.week_day   = 0;//��ʱ����������
	new_time.month  	= TM;
	new_time.year   	= TY;
	
	return new_time;
}


//1���еĵڼ���תΪ����ļ��¼���
//�������yearΪ��Ķ�����ֵ����2014
//�������day_of_yearΪ����ڼ���
//�������p_month��д����һ���Ǽ���
//�������p_day_of_month��д����һ����ĳ�µļ���
void YearDaysConvert(unsigned short year,unsigned short day_of_year,unsigned char* p_month,unsigned char* p_day_of_month)
{
	unsigned char i,leap;
	unsigned short day_of_month = day_of_year;
	leap = ((year % 4 == 0) && (year % 100 != 0)) || ( year % 400 == 0);
	for(i = 1; day_of_month > days_per_month[leap][i]; i++)
		day_of_month -= days_per_month[leap][i];
	*p_month = i;
	*p_day_of_month = (unsigned char)day_of_month;
}

//��������Ĵ�ʱ�˿������������������ڼ�,����ֵ0��ʾת��ʧ�ܣ�ֵ1��ʾת���ɹ�
unsigned char getnextday(BIN_TIME today,BIN_TIME* tomorrow)
{
	unsigned short year_of_today,year_of_tomorrow;
	unsigned char leap;
	unsigned char day_of_month,month;
	
	*tomorrow = today;
	day_of_month = today.day_of_month;
	month = today.month;
	
	if( RTC_Time_Valid_Check(&today))
	{
		year_of_today = today.year + YEAR_OFFSET;
		leap = ((year_of_today % 4 == 0) && (year_of_today % 100 != 0)) || ( year_of_today % 400 == 0);
		
		if(day_of_month < days_per_month[leap][month])//������
		{
			day_of_month++;
			year_of_tomorrow = year_of_today;
		}
		else//����Ϊ�������1��
		{
			day_of_month=1;
			if(month==12)//����Ϊ�������1��
			{
				month = 1;
				year_of_tomorrow = year_of_today+1;
			}
			else//1---11��
			{
				month++;
				year_of_tomorrow = year_of_today;
			}
		}
		tomorrow->year = year_of_tomorrow - YEAR_OFFSET;
		tomorrow->month = month;
		tomorrow->day_of_month = day_of_month;
		tomorrow->week_day = day2weekday(year_of_tomorrow,month,day_of_month);
		return 1;
	}
	else
		return 0;	
	
}

//���ݲ��չ�ʽ����ĳ���ĳ��ĳ�������ڼ�������ֵ0--�����죬1--����һ...6--������
//����Ϊ�����Ƹ�ʽ������BCD��ʽ,year�ɴ﹫Ԫ65535��---
unsigned char day2weekday(unsigned short year,unsigned char month,unsigned char day_of_month)
{
	unsigned short century;
	unsigned char y;//���ʮλ�Ͱ�λ�Ķ�����ֵ
	short weekday;//
	//year�������1583
	if(year > 1583)
	{
		century = year / 100;//ȡ��,��2004���centuryΪ20(ע�ⲻ��21),2012���century��Ϊ20
		y = year % 100;
		if(month < 3)
		{
			if(y > 0)
				y = y - 1;
			else//��2000��1��1��
			{
				y = 99;//����ʱ��1999,��century���ü�ȥ1
				century -= 1;				
			}
			weekday = y + y/4 + century/4 + 26 * ( month + 12 + 1) / 10 + day_of_month - 1 - 2*century;
		}
		else
			weekday = y + y/4 + century/4 + 26 * ( month + 1) / 10 + day_of_month - 1 - 2*century;	
		
		//weekday += 70;//��7�����������Ϊ������������7��ģ
		do{
			if(weekday < 0)
				weekday += 7;
		}while( weekday < 0 );//��7�����������Ϊ������������+7��ģ		
		weekday %= 7;
		return (unsigned char)weekday;
	}
	else
		return 8;//��ݴ��󣬷�����Ч������
	
}




//��RTCʱ��ת��Ϊ�ַ�����ʽ��ʱ�䣬��"2014-7-17 10:10:10"
//ע���������buf��ָ��Ļ����������㹻������20�ֽ�
void Time2Str(char* buf,BIN_TIME* new_time)
{		
	sprintf(buf,"%04d-%02d-%02d %02d:%02d:%02d",
												new_time->year+YEAR_OFFSET,
												new_time->month,
												new_time->day_of_month,
												new_time->hour,
												new_time->minute,
												new_time->second);
}

//�ж�һ��RTC_TIME��ֵ�Ƿ�Ϊ��Ч---δ�ж�weekday
//��Ч�����棬���򷵻ؼ�
tBoolean RTC_Time_Valid_Check(BIN_TIME* new_time)
{
	unsigned short full_year;
	unsigned char leap;
	full_year = new_time->year + YEAR_OFFSET;
	leap = ((full_year % 4 == 0) && (full_year % 100 != 0)) || ( full_year % 400 == 0);//0--not leap year,1--leap year
	if(new_time->day_of_month > days_per_month[leap][new_time->month] )//�������е��µ��������
		return false;
	if( ((new_time->month > 0) && (new_time->month < 13)) &&//1---12��
		((new_time->day_of_month > 0) && (new_time->day_of_month < 32)) &&//1---31��
		( (new_time->hour < 24)) &&//0---23
		( (new_time->minute < 60)) &&//0---59
		( (new_time->second < 60))//0---59
		)//time is ok
			return true;
		else//something is wrong
			return false;

}

tBoolean Str2Time(char* buf, BIN_TIME* new_time)//������"2014-07-17 10:10:10"��ʱ��ת��ΪRTCʱ��
{
	BIN_TIME  temp_time;
	unsigned short full_year;
	
	if(strlen(buf)!= 19)//input error
		return false;
	full_year = (buf[0]-'0')*1000 + (buf[1]-'0')*100 + (buf[2]-'0')*10 + buf[3]-'0';
	if((full_year < YEAR_OFFSET)   || (full_year > 2135))
		return false;
		
    temp_time.year = full_year - YEAR_OFFSET;//year since YEAR_OFFSET
	temp_time.month = (buf[5]-'0')*10 + buf[6]-'0';
	temp_time.day_of_month = (buf[8]-'0')*10 + buf[9]-'0';
	temp_time.hour = (buf[11]-'0')*10 + buf[12]-'0';
	temp_time.minute = (buf[14]-'0')*10 + buf[15]-'0';
	temp_time.second = (buf[17]-'0')*10 + buf[18]-'0';
	temp_time.week_day = day2weekday(full_year,temp_time.month,temp_time.day_of_month);//
	if(temp_time.week_day == 0)//������
		temp_time.week_day = 7;//BIN_TIME��7��ʾ������
		
	if( RTC_Time_Valid_Check(&temp_time))//time is ok
	{
		*new_time = temp_time;
		return true;
	}
	else		
		return false;//

}

tBoolean Str2Time2(char* buf, BIN_TIME* new_time)//������"20140717101010"��ʱ��ת��ΪRTCʱ��
{
	BIN_TIME  temp_time;
	unsigned short full_year;
	
	if(strlen(buf)!= 14)//input error
		return false;
	full_year = (buf[0]-'0')*1000 + (buf[1]-'0')*100 + (buf[2]-'0')*10 + buf[3]-'0';
	if((full_year < YEAR_OFFSET)   || (full_year > 2135))
		return false;
		
    temp_time.year = full_year - YEAR_OFFSET;//year since YEAR_OFFSET
	temp_time.month = (buf[4]-'0')*10 + buf[5]-'0';
	temp_time.day_of_month = (buf[6]-'0')*10 + buf[7]-'0';
	temp_time.hour = (buf[8]-'0')*10 + buf[9]-'0';
	temp_time.minute = (buf[10]-'0')*10 + buf[11]-'0';
	temp_time.second = (buf[12]-'0')*10 + buf[13]-'0';
	temp_time.week_day = day2weekday(full_year,temp_time.month,temp_time.day_of_month);//
	if(temp_time.week_day == 0)//������
		temp_time.week_day = 7;//BIN_TIME��7��ʾ������
		
	if( RTC_Time_Valid_Check(&temp_time))//time is ok
	{
		*new_time = temp_time;
		return true;
	}
	else		
		return false;//

}

//PCF8563��BCD��ʽת��ΪSTM32F207�ڲ�RTC��ʽ
void BCD_Time2BIN_Time(BCD_TIME bcd_time, BIN_TIME* bin_time)
{
	bin_time->year = (bcd_time.year >> 4)*10 + (bcd_time.year & 0x0f);
	bin_time->month = (bcd_time.month >>4)*10 + (bcd_time.month & 0x0f);
	bin_time->day_of_month = (bcd_time.day_of_month >>4)*10 + (bcd_time.day_of_month & 0x0f);
	bin_time->hour = (bcd_time.hour >>4)*10 + (bcd_time.hour & 0x0f);
	bin_time->minute = (bcd_time.minute >>4)*10 + (bcd_time.minute & 0x0f);
	bin_time->second = (bcd_time.second >>4)*10 + (bcd_time.second & 0x0f);
	
	bin_time->week_day = day2weekday(bin_time->year+YEAR_OFFSET, bin_time->month,bin_time->day_of_month);
	if(bin_time->week_day == 0)//PCF8563��0��ʾ����
		bin_time->week_day = 7;//

}

//STM32F207�ڲ�RTC��ʽת��ΪPCF8563��BCD��ʽ
void BIN_Time2BCD_Time(BIN_TIME bin_time, BCD_TIME* bcd_time)
{
	bcd_time->year = (bin_time.year/10)*16 + (bin_time.year%10);
	bcd_time->month = (bin_time.month/10)*16 + (bin_time.month%10);
	bcd_time->day_of_month = (bin_time.day_of_month/10)*16 + (bin_time.day_of_month%10);
	bcd_time->hour = (bin_time.hour/10)*16 + (bin_time.hour%10);
	bcd_time->minute = (bin_time.minute/10)*16 + (bin_time.minute%10);
	bcd_time->second = (bin_time.second/10)*16 + (bin_time.second%10);
	
	bcd_time->week_day = day2weekday(bin_time.year+YEAR_OFFSET, bin_time.month,bin_time.day_of_month);
	if(bcd_time->week_day == 0)//������
		bcd_time->week_day = 7;
}
