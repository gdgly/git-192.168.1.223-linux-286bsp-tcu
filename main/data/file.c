/***************************************Copyright (c)*****************************
**                               DongGuang EAST Co.,LTD.
**                                     
**                                 http://www.EASTUPS.com
**
**--------------File Info-------------------------------------------------------
** File Name:              file.c
** Last modified Date:      2011.05.09
** Last Version:            1.0
** Description:             
** 
**------------------------------------------------------------------------------
** Created By:              cwenfu 傅克文
** Created date:            2011.05.09
** Version:                 1.0
** Descriptions:   					
**file.c文件：提供系统中对于二进制文件的读写API函数
****************************************************************************/
/*------------------------------------------------------------------------------
Section: Includes
------------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


#include "globalvar.h"
#include "file.h"
/*------------------------------------------------------------------------------
Section: Constant Definitions
------------------------------------------------------------------------------*/
/* None */

/*------------------------------------------------------------------------------
Section: Type Definitions
------------------------------------------------------------------------------*/
/* None */

/*------------------------------------------------------------------------------
Section: Local Variable
------------------------------------------------------------------------------*/
/* None */

/*------------------------------------------------------------------------------
Section: Local Function Prototypes
------------------------------------------------------------------------------*/
/* None */


/**********************************************************************
*
*writeFileFlag: 写文件完成标志
*
*INPUTS:
*    FILE *fp: 文件指针   
*    UINT8 flag: 写入的标志
*
*RETURN:
*    ERROR16: 错误      OK: 正确
*
**********************************************************************/
static STATUS writeFileFlag(FILE *fp, UINT8 flag)
{
    int nwrite;
    
    /* 将标志写入文件 */
    fseek(fp,  POS_FINISH * sizeof(UINT8), SEEK_SET);
    nwrite = fwrite(&flag, 1, sizeof(UINT8), fp);
    if (nwrite !=  sizeof(UINT8)){
        printf("writeFile: write finish flag error\n");
        return ERROR16;
    }

    return OK;
}
/******************************************************************************
*
*  chkSum - 计算和校验
*
*   计算和校验前应确保f_aPacket不为空，而且sizeof(f_aPacket) >= f_nf_nLen
*
******************************************************************************/
extern UINT8 chkSum(UINT8 *f_aPacket, UINT32 f_nLen)
{
    UINT32 i;
    UINT8 sum = 0;

    for(i = 0; i < f_nLen; i ++){
        sum += f_aPacket[i];
    }
    return sum;
}
void lock_set(int fd, int type)
{
	struct flock lock;

	/*赋值lock结构体，加锁整个文件*/
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;

	while(1){
		lock.l_type = type;

		//根据不同的type来给文件加锁或解锁，如果成功，则返回0，失败则返回-1。
		//举例：如果一个文件原来已经建立了互斥锁，那么再调用fcntl建立锁就会失败，返回-1。
		if((fcntl(fd, F_SETLK, &lock)) == 0) {
			if(lock.l_type == F_RDLCK){			   //如果是共享锁
				//printf("read only set by %d\n", getpid());
			}else if(lock.l_type == F_WRLCK){   //如果是互斥锁
				//printf("write lock set by %d\n", getpid());
			}else if(lock.l_type == F_UNLCK){
				//printf("release lock by %d\n", getpid());
			}

			return;
		}else{
			//获得lock的描述，也就是将文件fd的加锁信息存入到lock结构中,如果成功则返回0,如果失败则返回-1
			if((fcntl(fd, F_GETLK, &lock)) == 0){
				if(lock.l_type != F_UNLCK){
					if(lock.l_type == F_RDLCK){
						//printf("read lock already set by %d\n", lock.l_pid);
					}else if(lock.l_type == F_WRLCK){
						//printf("write lock already set by %d\n", lock.l_pid);
					}
					sleep(1);//
				}
			}else{
				printf("cannot get the description of struck flock.\n");
			}
		}
	}
}
/**********************************************************************
*
*readFileInfo: 读取并检查文件头正确性
*
*INPUTS:
*   FILE *fp: 文件指针
*   FILE_FORMAT *file: 文件信息结构体
*   UINT8 *textBuf: 读入文件缓冲 NULL: 不读 readLen: 0: 文件的全部内容 
*                                                    其余: 所要读取的长度
*
*REUTRNS;
*   ERROR16: 错误   OK: 正确
*
**********************************************************************/
static STATUS readFileInfo(FILE *fp, FILE_FORMAT *file, UINT8 *textBuf, UINT32 fileOffset, UINT32 readLen)
{
    UINT8 check;
    UINT32 nread;
    UINT8 *pMem = NULL;

    if (NULL == fp){
        printf("readFileInfo: fp is NULL\n");
        return ERROR16;
    }
    
    rewind(fp);
    /* 检查文件头标志 */
    nread = fread(&file->headFlag, 1, sizeof(UINT8), fp);
	//printf("headFlag: [0x%x]\n", file->headFlag);
    if (FLAG_FHEAD != file->headFlag){
        printf("readFileInfo: headFlag error\n");
        return ERROR16;
    }

    /* 检查头文件长度 */
    nread = fread(&file->headLen, 1, sizeof(UINT8), fp);
	//printf("headLen: [0x%x]\n", file->headLen);
    if (LEN_FHEAD != file->headLen){
        printf("readFileInfo: head len error\n");
        return ERROR16;
    }
    
    /* 检查文件完成标志 */
    nread = fread(&file->finish_flag, 1, sizeof(UINT8), fp);
	//printf("finish_flag: [0x%x]\n", file->finish_flag);
    if (1 != file->finish_flag){
        printf("readFileInfo: finish flag error\n");
        return ERROR16;
    }

    /* 检查文件内容开始标志 */
    nread = fread(&file->textFlag, 1, sizeof(UINT8), fp);
	//printf("textFlag: [0x%x]\n", file->textFlag);
    if (FLAG_FTEXT != file->textFlag){
        printf("readFileInfo: textFlag error\n");
        return ERROR16;
    }

    /* 读取文件名称，版本，修改日期 */
    nread = fread(&file->name, 1, 24*sizeof(UINT8), fp);
	//printf("textFlag: [0x%x]\n", file->textFlag);
    if (nread != 24*sizeof(UINT8)){
        printf("readFileInfo: name error\n");
        return ERROR16;
    }

    
    /* 读取文件长度 */
    nread = fread(&file->textLen, 1, sizeof(UINT32), fp);
	//printf("textLen: [%d]\n", file->textLen);

    if (sizeof(UINT32) != nread){
        printf("readFileInfo: read textlen error\n");
        return ERROR16;
    }

    /* 读取文件校验码 */
    fseek(fp, file->textLen, SEEK_CUR);
    nread = fread(&file->checkByte, 1, sizeof(UINT8), fp);
    file->checkByte++;          /* 完成标志变化0->1 所以要+1 */
	//printf("checkByte: [0x%x]\n", file->checkByte);
    if (sizeof(UINT8) != nread){
        printf("readFileInfo: read checksum error\n");
        return ERROR16;
    }

    /* 确认校验和的正确性 */
    pMem = malloc(file->textLen * sizeof(UINT8));
    if (NULL == pMem){
        printf("readFileInfo: malloc error\n");
        return ERROR16;
    }
    fseek(fp, OFFSET_TEXT, SEEK_SET);
    nread = fread(pMem, 1, file->textLen, fp);
	//printbuffer("Filetext: ", pMem, file->textLen);
    if (nread != file->textLen){
        printf("readFileInfo: read text error\n");
        free(pMem);
        return ERROR16;
    }
    
    /* 计算校验和 */
    check = chkSum(pMem, file->textLen) + chkSum((UINT8 *)file, OFFSET_TEXT);
	//printbuffer("info: ", (UINT8 *)file, OFFSET_TEXT);
	//printf("checktext: [0x%x]\n", chkSum(pMem, file->textLen));
	//printf("checkheadinfo: [0x%x]\n", chkSum((UINT8 *)&file, OFFSET_TEXT));
	//printf("check: [0x%x]\n", check);

    if (file->checkByte != check){  
        printf("readFileInfo: check sum error!!check=%x\n file->checkByte=%x",check,file->checkByte);
        free(pMem);
        return ERROR16;
    }

    /* 读文件结束标志 */
    fseek(fp, sizeof(UINT8), SEEK_CUR);
    nread = fread(&file->endFlag, 1, sizeof(UINT8), fp);
	//printf("endFlag: [0x%x]\n", file->endFlag);
    if (FLAG_FEND != file->endFlag){
        printf("readFileInfo: end flag error\n");
        free(pMem);
        return ERROR16;
    }

    /* 读取文件 */
    if (NULL != textBuf){
        if (0 == readLen){  /* 读取文件的全部内容 */
            memcpy(textBuf, pMem, file->textLen);
        }else{  /* 读取文件的部分内容 */
            if (fileOffset + readLen > file->textLen){
                printf("readFileInfo: read text Len error\n");
                free(pMem);
                return ERROR16;
            }
            memcpy(textBuf, (pMem + fileOffset), readLen);
        }
    }
    
    free(pMem);
    return OK;
}

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
extern STATUS readWholeFile(UINT8 *fileName, UINT8 *fileText, UINT32 *fileLen)
{
	FILE *fp;
	int fd;
	FILE_FORMAT file;
	UINT8 Name[100];
	UINT16 ret;

	sprintf((void *)Name, "%s", fileName);
	fp = fopen((void *)Name, "rb+");
	if (NULL == fp){
		printf("readWholeFile: open file error\n");
		return ERROR16;
	}

	/*给文件加读取锁*/
	fd = fileno(fp);
	lock_set(fd, F_RDLCK);
	ret = readFileInfo(fp, &file, fileText, 0, 0);
	if (ERROR16 == ret){
		printf("readWholeFile: readFileInfo error\n");
		lock_set(fd, F_UNLCK);//给文件解锁
		fclose(fp);
		return ERROR16;
	}

	*fileLen = file.textLen;
	fflush(fp);
	fsync(fd);

	/*给文件解锁*/
	lock_set(fd, F_UNLCK);
	fclose(fp);
	return OK;
}

/**********************************************************************
*
*readFile: 读取文件内容
*
*INPUTS:
*   UINT8 *fileName: 文件名
*   UINT8 *fileText: 读取后的文件内容存放位置
*   UINT32 fileOffset: 读取的位置偏移
*   UINT32 readLen: 读入长度
*
*REUTRNS;
*   ERROR16: 错误   OK: 正确
*
**********************************************************************/
extern STATUS readFile(UINT8 *fileName, UINT8 *fileText, UINT32 fileOffset, UINT32 readLen)
{
    FILE *fp;
		int fd;
    FILE_FORMAT file;
    UINT8 Name[100];
    UINT16 ret;

    sprintf((void *)Name, "%s", fileName);
    fp = fopen((void *)Name, "rb+");
    if (NULL == fp){
        printf("readFile: open file error\n");
        return ERROR16;
    }

    if (0 == readLen){
        printf("readFile: readLen = 0 error\n");
        fclose(fp);
        return ERROR16;
    }

		/*给文件加读取锁*/
		fd = fileno(fp);
		lock_set(fd, F_RDLCK);
    ret = readFileInfo(fp, &file, fileText, fileOffset, readLen);
    if (ERROR16 == ret){
        printf("readFile: readFileInfo error\n");
        lock_set(fd, F_UNLCK);//给文件解锁
        fclose(fp);
        return ERROR16;
    }
    fflush(fp);
    fsync(fd);

		/*给文件解锁*/
		lock_set(fd, F_UNLCK);
    fclose(fp);
    return OK;
}


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
extern STATUS writeFile(UINT8 *fileName, UINT8 *fileText, UINT32 fileLen, UINT32 fileOffset, UINT8 write_type)
{
    FILE *fp;
		int fd;
    UINT8 Name[100];
    UINT8 *pMem = NULL;
    UINT32 nwrite, nread;
    FILE_FORMAT file;

    sprintf((void *)Name, "%s", fileName);

    /* 写新文件 */
    if (0 == write_type){
        fp = fopen((void *)Name, "wb+");
        if (NULL == fp){
            printf("writeFile: open file error\n");
            return ERROR16;
        }

				/*给文件加写入锁*/
				fd = fileno(fp);
				lock_set(fd, F_WRLCK);

        /* 文件头 */
        file.headFlag = FLAG_FHEAD;  /* 文件头标志 */
        file.headLen = LEN_FHEAD;    /* 文件头长度 */
       // strcpy(file.name, fileName);  /* 文件名称 */
        file.finish_flag = 0;  /* 文件未完成 */
        file.textFlag = FLAG_FTEXT;   /* 文件内容标志 */

        /* 文件内容 */
        file.textLen = fileLen;  /* 文件长度 */
        rewind(fp); /* 将文件指针指向文件头 */
        /* 将文件信息写入文件 */
        nwrite = fwrite(&file, 1, OFFSET_TEXT, fp);
        if (nwrite != OFFSET_TEXT){
            printf("writeFile: write file info error\n");
            lock_set(fd, F_UNLCK);//给文件解锁
            fclose(fp);
            return ERROR16;
        }

        /* 将文件内容写入文件 */
        nwrite = fwrite(fileText, 1, fileLen, fp);
        if (nwrite != fileLen){
            printf("writeFile: write file text error\n");
            lock_set(fd, F_UNLCK);//给文件解锁
            fclose(fp);
            return ERROR16;
        }

        /* 文件校验和 */
        file.checkByte = chkSum(fileText, fileLen) + chkSum((UINT8 *)&file, OFFSET_TEXT);
        file.endFlag = FLAG_FEND;
        /* 将文件尾写入文件 */
        nwrite = fwrite(&file.checkByte, 1, 2 * sizeof(UINT8), fp);
        if (nwrite != 2 * sizeof(UINT8)){
            printf("writeFile: write file end error\n");
            lock_set(fd, F_UNLCK);//给文件解锁
            fclose(fp);
            return ERROR16;
        }
        /* 将完成标志写入文件 */
        if (ERROR16 == writeFileFlag(fp, 1)){
            printf("writeFile: write file flag error\n");
            lock_set(fd, F_UNLCK);//给文件解锁
            fclose(fp);
            return ERROR16;
        }
        fflush(fp);
        fsync(fd);

				/*给文件解锁*/
				lock_set(fd, F_UNLCK);
        fclose(fp);
        return OK;
     }

	/*-----------------------------------------------------*/
     /* 在原有基础上写文件 */
    fp = fopen((void *)Name, "rb+");
    if (NULL == fp){
        printf("writeFile: open file error\n");
        return ERROR16;
    }

		/*给文件加写入锁*/
		fd = fileno(fp);
		lock_set(fd, F_WRLCK);

    /* 检查文件正确性 */
    if (ERROR16 == readFileInfo(fp, &file, NULL, 0, 0)){
        printf("writeFile: file info error\n");
        lock_set(fd, F_UNLCK);
        fclose(fp);
        return ERROR16;
    }

    file.finish_flag = 0;  /* 标志操作未完成 */
    if (1 == write_type){     /* 覆盖 */
        file.textLen = fileLen + fileOffset;  /* 文件长度 */
    }else if (2 == write_type){   /* 替换 */
        if (fileOffset + fileLen > file.textLen){  /* 替换不能超过原文件大小 */
            printf("writeFile: File Len error\n");
            lock_set(fd, F_UNLCK);
            fclose(fp);
            return ERROR16;
        }
    }else{
        printf("writeFile: No such write_type\n");
        lock_set(fd, F_UNLCK);
        fclose(fp);
        return ERROR16;
    }
    
    rewind(fp); /* 将文件指针指向文件头 */
    /* 将文件信息写入文件 */
    nwrite = fwrite(&file, 1, OFFSET_TEXT, fp);
    if (nwrite != OFFSET_TEXT){
        printf("writeFile: write file info error\n");
        lock_set(fd, F_UNLCK);
        fclose(fp);
        return ERROR16;
    }

    /* 确定写入文件的位置 */
    fseek(fp, OFFSET_TEXT + fileOffset, SEEK_SET);
    /* 写文件 */
    nwrite = fwrite(fileText, 1, fileLen, fp);
    if (nwrite != fileLen){
        printf("writeFile: write file text error\n");
        lock_set(fd, F_UNLCK);
        fclose(fp);
        return ERROR16;
    }

	//printf("-------file.textLen + OFFSET_TEXT = %d-----------\n", file.textLen + OFFSET_TEXT);
    /* 计算校验和 */
		pMem = malloc(file.textLen + OFFSET_TEXT);
    if (NULL == pMem){
        printf("writeFile: malloc error\n");
       	lock_set(fd, F_UNLCK);
        fclose(fp);
        return ERROR16;
    }
    rewind(fp);
    nread = fread(pMem, 1, file.textLen + OFFSET_TEXT, fp);
    if ((file.textLen + OFFSET_TEXT) != nread){
        printf("writeFile: File read error\n");
        free(pMem);
       	lock_set(fd, F_UNLCK);
        fclose(fp);
        return ERROR16;
    }
    file.checkByte = chkSum(pMem, file.textLen + OFFSET_TEXT);
	//printf("checkByte = %x\n", file.checkByte);
    free(pMem);
    file.endFlag = FLAG_FEND;

    /* 写文件尾 */
    fseek(fp, file.textLen + OFFSET_TEXT, SEEK_SET);
    nwrite = fwrite(&file.checkByte, 1, 2 * sizeof(UINT8), fp);
    if (nwrite != 2 * sizeof(UINT8)){
        printf("writeFile: write file end error\n");
        lock_set(fd, F_UNLCK);
        fclose(fp);
        return ERROR16;
    }

    /* 将完成标志写入文件 */
    if (ERROR16 == writeFileFlag(fp, 1)){
        printf("writeFile: write file flag error\n");
        lock_set(fd, F_UNLCK);
        fclose(fp);
        return ERROR16;
    }
    fflush(fp);
    fsync(fd);

		/*给文件解锁*/
		lock_set(fd, F_UNLCK);
    fclose(fp);
    return OK;
}

/*copyFile: 拷贝文件**********************************************************
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
extern STATUS copyFile(UINT8 *srcFileName, UINT8 *desFileName)
{
    FILE *fp;
 		int fd;
    UINT8 fileName[100];
    UINT8 *pMem = NULL;
    UINT32 textLen, nread, nwrite;

    /* 打开原文件 */
    sprintf((void *)fileName, "%s", srcFileName);
    fp = fopen((void *)fileName, "rb");
    if (NULL == fp){
        printf("copyFile: open srcfile error\n");
        return ERROR16;
    }

		/*给文件加读取锁*/
		fd = fileno(fp);
		lock_set(fd, F_RDLCK);

    /* 读取文件长度 */
    rewind(fp);
    fseek(fp, 28 * sizeof(UINT8), SEEK_SET);
    nread = fread(&textLen, 1, sizeof(UINT32), fp);
    if (sizeof(UINT32) != nread){
        printf("copyFile: read srcfile textlen error\n");
        lock_set(fd, F_UNLCK);
        fclose(fp);
        return ERROR16;
    }

    textLen += 34;//add by  dengjh 20110822
    /* 申请内存 */
    pMem = malloc(textLen);
    if (NULL == pMem){
        printf("copyFile: malloc mem fail\n");
        lock_set(fd, F_UNLCK);
        fclose(fp);
        return ERROR16;
    }
    /* 读取源文件内容到内存中 */
    rewind(fp);
    nread = fread(pMem, 1, textLen, fp);
    if (textLen != nread){
        printf("copyFile: read srcfile file error\n");
        free(pMem);
        lock_set(fd, F_UNLCK);
        fclose(fp);
        return ERROR16;
    }
    /* 关闭源文件 */
    fflush(fp);
    fsync(fd);

		/*给文件解锁*/
		lock_set(fd, F_UNLCK);
    fclose(fp);

    /* 将源文件内容写入目标文件中去 */
    /* 打开目标文件 */
    sprintf((void *)fileName, "%s", desFileName);
    fp = fopen((void *)fileName, "wb+");
    if (NULL == fp){
        printf("copyFile: open desfile error\n");
        free(pMem);
        return ERROR16;
    }

		/*给文件加写入锁*/
		fd = fileno(fp);
		lock_set(fd, F_WRLCK);

    /* 写入内容 */
    nwrite = fwrite(pMem, 1, textLen, fp);
    if (textLen != nwrite){
        printf("copyFile: write desfile error\n");
        free(pMem);
        lock_set(fd, F_UNLCK);
    		fclose(fp);
        return ERROR16;
    }
    /* 释放内存 */
    free(pMem);
    /* 关闭目标文件 */
    fflush(fp);
    fsync(fd);

		/*给文件解锁*/
		lock_set(fd, F_UNLCK);
    fclose(fp);   
    return OK;
}

/*******************************End of file.c ***********************/
/*
fileno()函数
功    能：把文件流指针转换成文件描述符
相关函数：open, fopen
表头文件：#include <stdio.h>
定义函数：int fileno(FILE *stream)
函数说明：fileno()用来取得参数stream指定的文件流所使用的文件描述词
返回值  ：返回和stream文件流对应的文件描述符。如果失败，返回-1。
范例：
#include <stdio.h>
main()
{
     FILE   *fp;
     int   fd;
     fp = fopen("/etc/passwd", "r");
     fd = fileno(fp);
     printf("fd = %d\n", fd);
     fclose(fp);
} */


