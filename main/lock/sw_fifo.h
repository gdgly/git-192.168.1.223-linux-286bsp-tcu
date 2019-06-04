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
** Descriptions:            The original version 初始版本
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



//  定义通用FIFO存储单元结构
typedef struct
{	
	void*  ptr_fifo_data;//存储队列单个元素的数据指针	
	unsigned short len;  // 队列单个元素的有效数据字节长度	小于等于单个元素的宽度
}FIFO_DATA_ELEMENT;


typedef struct{
	
	unsigned char full;//fifo满标志,1有效
	unsigned char empty;//fifo空标志,1有效
	unsigned short valid_elements_num;//当前队列中元素个数
	
	unsigned short rd_index;//读指针(head)
	unsigned short wr_index;//写指针(tail)
	
	unsigned short width_cfg;//fifo单个元素最大宽度
	unsigned short depth_cfg;//fifo总深度
		
	FIFO_DATA_ELEMENT*  ptr_fifo_element_begin;//指向FIFO存储元素的首地址
}SW_FIFO_CTL;




typedef enum
{
	FIFO_IS_EMPTY = 0,//FIFO空
	FIFO_IS_FULL = 1,//FIFO满	
	FIFO_INIT_OK = 2,//FIFO初始化OK
	FIFO_INIT_FAIL = 3,//FIFO初始化失败
	FIFO_INSERT_OK = 4,//插入1条记录OK
	FIFO_READ_OK = 5,//读取1条记录OK
	FIFO_INSET_DATA_TOO_LONG = 6,//插入队列的数据长度超限
	FIFO_OUT_DATA_TOO_LONG = 7,//读取的队列数据比给定的数据缓冲区大，应用程序需使用更大的缓冲区来保存队列中读取的数据
}eFifo_Status;


extern eFifo_Status  sw_fifo_init(SW_FIFO_CTL*  sw_fifo_ctl_unit,  unsigned short fifo_width,unsigned short fifo_depth, FIFO_DATA_ELEMENT*  ptr_fifo_element_begin,void* ptr_fifo_data_mem);
extern eFifo_Status  sw_fifo_in(SW_FIFO_CTL*  sw_fifo_ctl_unit,unsigned short data_in_len,const void* data_src);//队列中插入1条记录
extern eFifo_Status  sw_fifo_out(SW_FIFO_CTL*  sw_fifo_ctl_unit,unsigned short* data_out_len,unsigned short data_dest_size,void* data_dest);//队列中读取1条记录到data_dest指向的缓冲区
extern eFifo_Status  sw_fifo_clear(SW_FIFO_CTL*  sw_fifo_ctl_unit);//清空队列
extern unsigned short sw_fifo_query(SW_FIFO_CTL*  sw_fifo_ctl_unit);//查询队列中有效元素个数

#ifdef __cplusplus
    }
#endif

#endif //__SW_FIFO_H__

