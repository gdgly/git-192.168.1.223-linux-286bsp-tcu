/****************************************Copyright (c)****************************************************
**                               DongGuan EAST Co.,LTD.
**                                     
**                                 http://www.EASTUPS.com
**
**--------------File Info---------------------------------------------------------------------------------
** File Name:               sw_fifo.h
** Last modified Date:      2012.12.7
** Last Version:            1.0
** Description:             
** 
**--------------------------------------------------------------------------------------------------------
** Created By:              
** Created date:            2012.12.7
** Version:                 1.0
** Descriptions:            The original version ��ʼ�汾
**
**--------------------------------------------------------------------------------------------------------
*********************************************************************************************************/

#ifndef  __SW_FIFO_H__
#define  __SW_FIFO_H__


//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C" {
#endif



//  ����ͨ��FIFO�洢��Ԫ�ṹ
typedef struct
{	
	void*  ptr_fifo_data;//�洢���е���Ԫ�ص�����ָ��	
	unsigned short len;  // ���е���Ԫ�ص���Ч�����ֽڳ���	С�ڵ��ڵ���Ԫ�صĿ��
}FIFO_DATA_ELEMENT;


typedef struct{
	
	unsigned char full;//fifo����־,1��Ч
	unsigned char empty;//fifo�ձ�־,1��Ч
	unsigned short valid_elements_num;//��ǰ������Ԫ�ظ���
	
	unsigned short rd_index;//��ָ��(head)
	unsigned short wr_index;//дָ��(tail)
	
	unsigned short width_cfg;//fifo����Ԫ�������
	unsigned short depth_cfg;//fifo�����
		
	FIFO_DATA_ELEMENT*  ptr_fifo_element_begin;//ָ��FIFO�洢Ԫ�ص��׵�ַ
}SW_FIFO_CTL;




typedef enum
{
	FIFO_IS_EMPTY = 0,//FIFO��
	FIFO_IS_FULL = 1,//FIFO��	
	FIFO_INIT_OK = 2,//FIFO��ʼ��OK
	FIFO_INIT_FAIL = 3,//FIFO��ʼ��ʧ��
	FIFO_INSERT_OK = 4,//����1����¼OK
	FIFO_READ_OK = 5,//��ȡ1����¼OK
	FIFO_INSET_DATA_TOO_LONG = 6,//������е����ݳ��ȳ���
	FIFO_OUT_DATA_TOO_LONG = 7,//��ȡ�Ķ������ݱȸ��������ݻ�������Ӧ�ó�����ʹ�ø���Ļ���������������ж�ȡ������
}eFifo_Status;


extern eFifo_Status  sw_fifo_init(SW_FIFO_CTL*  sw_fifo_ctl_unit,  unsigned short fifo_width,unsigned short fifo_depth, FIFO_DATA_ELEMENT*  ptr_fifo_element_begin,void* ptr_fifo_data_mem);
extern eFifo_Status  sw_fifo_in(SW_FIFO_CTL*  sw_fifo_ctl_unit,unsigned short data_in_len,const void* data_src);//�����в���1����¼
extern eFifo_Status  sw_fifo_out(SW_FIFO_CTL*  sw_fifo_ctl_unit,unsigned short* data_out_len,unsigned short data_dest_size,void* data_dest);//�����ж�ȡ1����¼��data_destָ��Ļ�����
extern eFifo_Status  sw_fifo_clear(SW_FIFO_CTL*  sw_fifo_ctl_unit);//��ն���
extern unsigned short sw_fifo_query(SW_FIFO_CTL*  sw_fifo_ctl_unit);//��ѯ��������ЧԪ�ظ���

#ifdef __cplusplus
    }
#endif

#endif //__SW_FIFO_H__

