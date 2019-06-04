/****************************************Copyright (c)*****************************
**                               DongGuang EAST Co.,LTD.
**                                     
**                                 http://www.EASTUPS.com
**
**--------------File Info-------------------------------------------------------
** File Name:              file.h
** Last modified Date:      2011.05.09
** Last Version:            1.0
** Description:             
** 
**------------------------------------------------------------------------------
** Created By:              cwenfu 傅克文
** Created date:            2011.05.09
** Version:                 1.0
** Descriptions:   					
**file.c文件的对外函数声明
****************************************************************************/

#ifndef _FILE_H_
#define _FILE_H_

#ifdef __cplusplus             /* Maintain C++ compatibility */
extern "C" {
#endif /* __cplusplus */

//#include "globalvar.h"
/*------------------------------------------------------------------------------
Section: Macro Definitions
------------------------------------------------------------------------------*/
#define FLAG_FHEAD 0x68   /* 文件头标志 */
#define FLAG_FTEXT 0x18   /* 文件内容标志 */
#define FLAG_FEND  0x16   /* 文件结束标志 */
#define LEN_FHEAD 32      /* 文件头大小 */
#define POS_FINISH 2      /* 文件完成标志的位移 */
#define OFFSET_TEXT  32   /* 文件内容位移 */

/*------------------------------------------------------------------------------
Section: Type Definitions
------------------------------------------------------------------------------*/
typedef struct file_format
{
    UINT8 headFlag;      /* 文件头标志  0x68 */
    UINT8 headLen;       /* 文件头长度 = 8 byte*/
    UINT8 finish_flag;   /* 文件完成标志 0: 未完成 1: 已完成 */
    UINT8 textFlag;      /* 文件内容标志  0x18 */
    
    UINT8 name[16];      /* 文件名称 */
    UINT8 version[2];    /* 版本信息  如10.01 */
    UINT8 date[6];       /* 时间记录 YYMMDDHHMMSS */ 
    UINT32 textLen;      /* 文件长度 */
    
    UINT8 checkByte;     /* 总加和校验位 */
    UINT8 endFlag;       /* 文件结束标记 0x16 */
}FILE_FORMAT, *FILE_FORMAT_P ;

/*------------------------------------------------------------------------------
Section: Globals
------------------------------------------------------------------------------*/
/* None */

/*------------------------------------------------------------------------------
Section: Function Prototypes
------------------------------------------------------------------------------*/
/**********************************************************************
*
*readWholeFile: 读取整个文件内容
*
*INPUTS:
*   UINT8 *fileName: 文件名
*   UINT8 *fileText: 输出的文件内容
*   UINT32 *fileLen: 文件长度
*
*REUTRNS;
*   ERROR16: 错误   OK: 正确
*
**********************************************************************/
extern STATUS readWholeFile(UINT8 *fileName, UINT8 *fileText, UINT32 *fileLen);

/**********************************************************************
*
*readFile: 读取文件内容
*
*INPUTS:
*   UINT8 *fileName: 文件名
*   UINT8 *fileText: 输出的文件内容
*   UINT32 fileOffset: 文件位移
*   UINT32 readLen: 读入长度
*
*REUTRNS;
*   ERROR16: 错误   OK: 正确
*
**********************************************************************/
extern STATUS readFile(UINT8 *fileName, UINT8 *fileText, UINT32 fileOffset, 
UINT32 readLen);

/***********************************************************************
*
*writeFile: 写文件
* 
*INPUTS:
*   UINT8 *fileName: 文件名
*   UINT8 *fileText: 需要写入的内容
*   UINT32 fileLen: 需要写入的长度
*   UINT32 fileOffset: 写入位置
*   UINT8 write_type: 写入类型 
*    0: 写新文件 1: 部分覆盖原有文件  2: 部分替换原有文件
*    部分覆盖: 即原有文件在写入位置之后的信息全部作废
*    部分替换: 即原有文件在写入位置之后的信息依然有效
*    例子:  假设文件内容为 0 1 2 3 4
*           在2的位置写5 如果部分覆盖则文件变成 0 1 5
*                        如果部分替换则文件变成 0 1 5 3 4
*
*RETURN:
*   ERROR16: 写文件错误    OK: 写文件正常
*
***********************************************************************/
extern STATUS writeFile(UINT8 *fileName, UINT8 *fileText, UINT32 fileLen, 
UINT32 fileOffset, UINT8 write_type);

/**********************************************************************
*
*copyFile: 拷贝文件
*
*INPUTS:
*   UINT8 *srcFileName: 源文件名
*   UINT8 *desFileName: 目标文件名
*
*REUTRNS;
*   ERROR16: 错误   OK: 正确
*
**********************************************************************/
extern STATUS copyFile(UINT8 *srcFileName, UINT8 *desFileName);

extern void fileTest(void);
#ifdef __cplusplus      /* Maintain C++ compatibility */
}
#endif /* __cplusplus */

#endif /* _INFRARED_H_ */

/*-------------------------------End of file.h------------------------*/

