/****************************************Copyright (c)****************************************************
**                               DongGuan EAST Co.,LTD.
**                                     
**                                 http://www.EASTUPS.com
**
**--------------File Info---------------------------------------------------------------------------------
** File Name:               Time_Utity.h 
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
	unsigned char year;//�� since 2000,��ֵΪ12��ʾʵ��Ϊ2012��	
	unsigned char month;//�� 1---12
	unsigned char day_of_month;//ĳ���е�ĳ�գ���3��2��, 1---31
	unsigned char week_day;//���ڼ� 1---7  7-sunday,1-monday....6-saturday	----STM32F207ʹ��ֵ7��ʾ������
	unsigned char hour;//Сʱ0---23
	unsigned char minute;//���� 0---59
	unsigned char second;//�� 0---59
	
}BIN_TIME;	//��ֵΪ�����Ƹ�ʽ		  

typedef struct
{
	unsigned char year;//���ʮλ�͸�λ	
	unsigned char month;//�� 1---12
	unsigned char day_of_month;//ĳ���е�ĳ�գ���3��2��, 1---31
	unsigned char week_day;//���ڼ� 0---6  0-sunday,1-monday....6-saturday
	unsigned char hour;//Сʱ0---23
	unsigned char minute;//���� 0---59
	unsigned char second;//�� 0---59
}BCD_TIME;//PCF8563��RTCʱ���ʽ,ע�ⶼΪBCD��ʽ

typedef unsigned char tBoolean;

#ifndef false
#define false 0
#endif 

#ifndef true
#define true 1
#endif 

#define   YEAR_OFFSET    2000//���2000ƫ�ƿ�ʼ

//����RTC_TIME��ʾ��ʱ����2000��1��1��0��0��0����������
uint32_t RTC_TIME2Secs(BIN_TIME* new_time);
//���������2000��1��1��0��0��0����������ת��Ϊ��Ӧ��RTCʱ��
BIN_TIME  Secs2RTC_TIME(uint32_t  TotSeconds);

//1���еĵڼ���תΪ����ļ��¼���
//yearΪ��Ķ�����ֵ����2014
void YearDaysConvert(unsigned short year,unsigned short day_of_year,unsigned char* p_month,unsigned char* p_day_of_month);
//���ݲ��չ�ʽ����ĳ���ĳ��ĳ�������ڼ�������ֵ0--�����죬1--����һ...6--������
//����Ϊ�����ƽ��Ƹ�ʽ������BCD��ʽ,year�ɴ﹫Ԫ65535��---
unsigned char day2weekday(unsigned short year,unsigned char month,unsigned char day_of_month);

//����������������������,����ֵ0��ʾת��ʧ�ܣ�ֵ1��ʾת���ɹ�
unsigned char getnextday(BIN_TIME today,BIN_TIME* tomorrow);

//�ж�һ��RTC_TIME��ֵ�Ƿ�Ϊ��Ч---δ�ж�weekday
//��Ч�����棬���򷵻ؼ�
tBoolean RTC_Time_Valid_Check(BIN_TIME* new_time);
//��RTCʱ��ת��Ϊ�ַ�����ʽ��ʱ�䣬��"2014-07-17 10:10:10"
//ע���������buf��ָ��Ļ����������㹻������20�ֽ�
void Time2Str(char* buf, BIN_TIME* new_time);
tBoolean Str2Time(char* buf, BIN_TIME* new_time);//������"2014-07-17 10:10:10"��ʱ��ת��ΪRTCʱ��
tBoolean Str2Time2(char* buf, BIN_TIME* new_time);//������"20140717101010"��ʱ��ת��ΪRTCʱ��

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

