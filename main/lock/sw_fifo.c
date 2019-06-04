/****************************************Copyright (c)****************************************************
**                               DongGuan EAST Co.,LTD.
**                                     
**                                 http://www.EASTUPS.com
**
**--------------File Info---------------------------------------------------------------------------------
** File Name:               sw_fifo.c
** Last modified Date:      2012.12.7
** Last Version:            1.0
** Description:             
** 
**--------------------------------------------------------------------------------------------------------
** Created By:              
** Created date:            2012.12.7
** Version:                 1.0
** Descriptions:            The original version 初始版本
**
**--------------------------------------------------------------------------------------------------------
*********************************************************************************************************/



//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C" {
#endif

//#include "includes.h"
#include "../data/globalvar.h"
#include "sw_fifo.h"
//定义临界区保护方法
static void sw_enter_critical_section(void)
{
	pthread_mutex_unlock(&sw_fifo);

	//OS_ENTER_CRITICAL();
}

//定义退出临界区保护方法
static void sw_exit_critical_section(void)
{
//	OS_EXIT_CRITICAL();	
	pthread_mutex_unlock(&sw_fifo);

}


/******************************************************************************
** Function name:       sw_fifo_init
** Descriptions:	    初始化队列控制单元
** Input parameters:	sw_fifo_ctl_unit--软件FIFO的控制单元结构体变量指针
						fifo_width--队列中元素的字节宽度
						fifo_depth---队列的深度
						ptr_fifo_element_begin--队列元素管理单元起始地址
						ptr_fifo_data_mem--存放队列实际数据的内存指针
						
** Output parameters:	
** Returned value:		初始化结果        
** Created by:		
** Created Date:		
**------------------------------------------------------------------------------
** Modified by:            
** Modified date:          
**------------------------------------------------------------------------------
*******************************************************************************/
eFifo_Status  sw_fifo_init(SW_FIFO_CTL*  sw_fifo_ctl_unit,  unsigned short fifo_width,unsigned short fifo_depth, FIFO_DATA_ELEMENT*  ptr_fifo_element_begin,void* ptr_fifo_data_mem)
{
	unsigned short i;
	FIFO_DATA_ELEMENT* ptr_fifo_data_element;
	unsigned char*  ptr_fifo_mem = (unsigned char* )ptr_fifo_data_mem;
	
	sw_fifo_ctl_unit->rd_index = 0;
	sw_fifo_ctl_unit->wr_index = 0;
	sw_fifo_ctl_unit->full = 0;
	sw_fifo_ctl_unit->empty = 1;
	
	sw_fifo_ctl_unit->width_cfg = fifo_width;
	sw_fifo_ctl_unit->depth_cfg = fifo_depth;
	
	sw_fifo_ctl_unit->valid_elements_num = 0;
	sw_fifo_ctl_unit->ptr_fifo_element_begin = ptr_fifo_element_begin;	
	
	ptr_fifo_data_element = ptr_fifo_element_begin;
	for(i=0;i<fifo_depth;i++)
	{
		(ptr_fifo_data_element+i)->ptr_fifo_data = (void*)(ptr_fifo_mem + i * fifo_width);
		(ptr_fifo_data_element+i)->len = 0;
	}
	return FIFO_INIT_OK;
}

//队列中插入1条记录，返回操作结果
eFifo_Status  sw_fifo_in(SW_FIFO_CTL*  sw_fifo_ctl_unit,unsigned short data_in_len,const void* data_src)
{
	eFifo_Status  eFifo_result = FIFO_INSERT_OK;
	FIFO_DATA_ELEMENT*   ptr_fifo_data_element;
	//sw_enter_critical_section();
	if(sw_fifo_ctl_unit->full)//队列满，不能插入记录
		eFifo_result = FIFO_IS_FULL;
	else
	{
		if(data_in_len > sw_fifo_ctl_unit->width_cfg)//插入数据长度超限
			eFifo_result = FIFO_INSET_DATA_TOO_LONG;
		else//长度未超限，可插入
		{
			ptr_fifo_data_element = (sw_fifo_ctl_unit->ptr_fifo_element_begin) + (sw_fifo_ctl_unit->wr_index);//locate to fifo data element
			
			memcpy(ptr_fifo_data_element->ptr_fifo_data,data_src,data_in_len);
			ptr_fifo_data_element->len = data_in_len;
			sw_fifo_ctl_unit->valid_elements_num++;			
			sw_fifo_ctl_unit->wr_index++;
			sw_fifo_ctl_unit->empty = 0;
			if(sw_fifo_ctl_unit->wr_index >= sw_fifo_ctl_unit->depth_cfg)//roll back
				sw_fifo_ctl_unit->wr_index = 0;
			if(sw_fifo_ctl_unit->wr_index == sw_fifo_ctl_unit->rd_index)//写指针追上读指针，队列满			
				sw_fifo_ctl_unit->full = 1;				
			
		}
		
	}
	//sw_exit_critical_section();
	return eFifo_result;
}

eFifo_Status  sw_fifo_out(SW_FIFO_CTL*  sw_fifo_ctl_unit,unsigned short* data_out_len,unsigned short data_dest_size,void* data_dest)//队列中读取1条记录到data_dest指向的缓冲区
{
	eFifo_Status  eFifo_result = FIFO_READ_OK;
	FIFO_DATA_ELEMENT*   ptr_fifo_data_element;
	sw_enter_critical_section();
	if(sw_fifo_ctl_unit->empty)//队列空，无数据可读
		eFifo_result = FIFO_IS_EMPTY;
	else
	{
		ptr_fifo_data_element = (sw_fifo_ctl_unit->ptr_fifo_element_begin) + (sw_fifo_ctl_unit->rd_index);//locate to fifo data element
			
		if(ptr_fifo_data_element->len  > data_dest_size)//存储数据的缓冲区长度不足,所需的缓冲区长度在data_out_len中告知调用者
		{
			eFifo_result = FIFO_OUT_DATA_TOO_LONG;
			*data_out_len = ptr_fifo_data_element->len;//tell caller how much memory need to storage data
		}
		else//长度未超限，可返回数据给调用者
		{			
			memcpy(data_dest, ptr_fifo_data_element->ptr_fifo_data,ptr_fifo_data_element->len);
			*data_out_len = ptr_fifo_data_element->len;//tell caller how much data readded
			if(sw_fifo_ctl_unit->valid_elements_num > 0)//必须成立
				sw_fifo_ctl_unit->valid_elements_num--;
			
			sw_fifo_ctl_unit->rd_index++;
			sw_fifo_ctl_unit->full = 0;
			if(sw_fifo_ctl_unit->rd_index >= sw_fifo_ctl_unit->depth_cfg)//roll back
				sw_fifo_ctl_unit->rd_index = 0;
			if(sw_fifo_ctl_unit->rd_index == sw_fifo_ctl_unit->wr_index)//读指针追上写指针，队列空				
				sw_fifo_ctl_unit->empty = 1;
			
		}
		
	}
	sw_exit_critical_section();
	return eFifo_result;
	
	
	
}

eFifo_Status  sw_fifo_clear(SW_FIFO_CTL*  sw_fifo_ctl_unit)//清空队列
{
	sw_enter_critical_section();
	
	sw_fifo_ctl_unit->rd_index = 0;
	sw_fifo_ctl_unit->wr_index = 0;
	sw_fifo_ctl_unit->full = 0;
	sw_fifo_ctl_unit->empty = 1;
	
	sw_fifo_ctl_unit->valid_elements_num = 0;
	
	sw_exit_critical_section();
	return FIFO_INIT_OK;
}

unsigned short sw_fifo_query(SW_FIFO_CTL*  sw_fifo_ctl_unit)//查询队列中有效元素个数
{
	unsigned short fifo_valid_data_nums=0;
	sw_enter_critical_section();
	
	fifo_valid_data_nums = sw_fifo_ctl_unit->valid_elements_num;
	
	sw_exit_critical_section();
	return fifo_valid_data_nums;
}


