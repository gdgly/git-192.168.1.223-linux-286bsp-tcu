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

UINT32 crc32_table[256];
UINT32  ulPolynomial = 0x04C11DB7;

//���ߵ���ֱ�������ĳ���
//ͬ��Ҫ����һ���ߵ����ص��ӳ���
UINT32  Reflect(UINT32  ref, char ch)
{
	UINT32  value=0;
	int i;
	// ����bit0��bit7��bit1��bit6������
	for(i = 1; i < (ch + 1); i++)
	{
		if(ref & 1)
		value |= 1 << (ch - i);
		ref >>= 1;
	}
	return value;
}
// ����CRC32�������ѯ��
void make_CRC32Table(void)
{
	UINT32  crc,temp;
	int i,j;
	UINT32  t1,t2;
	UINT32  flag;
	for(i = 0; i <= 0xFF; i++) // ����CRC32�������ѯ��
	{
		temp=Reflect(i, 8);
		crc32_table[i]= temp<< 24;
		for (j = 0; j < 8; j++)
		{	
			flag=crc32_table[i] & 0x80000000;
			t1=(crc32_table[i] << 1);
			if(flag==0)
		 		t2=0;
			else
				t2=ulPolynomial;

			crc32_table[i] = t1^t2 ;
		}
		crc=crc32_table[i];
		crc32_table[i] = Reflect(crc32_table[i], 32);
	}
}

//����CRC32ֵ
//pDataΪ����������ݻ�����ָ��
//lenΪ���������ݵ��ֽڳ���
extern UINT32 crc32_calc(const unsigned char*  pData, UINT32 len,UINT32 flag)
{
	UINT32 CRC32;
	static UINT32 m_CRC = 0xFFFFFFFF; //�Ĵ�����Ԥ�ó�ʼֵ
	unsigned long ii;
	const unsigned char *p;
	p = pData;
  if(flag == 0){
		for(ii=0; ii <len; ii++)
		{
			m_CRC = crc32_table[( m_CRC^(*(p+ii)) ) & 0xff] ^ (m_CRC >> 8); //����
		}
  }else{
		m_CRC = 0xFFFFFFFF;
	}
	CRC32= ~m_CRC; //ȡ������WINRAR�Աȣ�CRC32ֵ��ȷ����
	return CRC32;
}

