/*
* @file     Queue.c.
* @brief    消息队列
* @dtails.
* @author   chenls.
* @date     2020-07-26
* @version  V001.
* @par      COPYRIGHT (C) 2020, East Group Co.,Ltd
*
* @par Change Logs:
* Date          Version    Author      Notes
* 2020-07-26    V001       chenls      the first version
*/

#include "Queue.h"
#include <string.h>
#include <pthread.h>

#pragma pack(1) /* 1字节对齐开始 */

/*------------------------------------------------------------------------------
Section: 消息队列头信息
------------------------------------------------------------------------------*/
typedef struct 
{
    char             cName[QUEUE_NAME_SIZE + 1];        /*< 设备名 */
    unsigned int     QueueSize;                         /*< 消息队列缓存大小 */
    unsigned int     FirstNodeAddr;                     /*< 头节点地址 */
    unsigned int     LastNodeAddr;                      /*< 尾节点地址 */
    pthread_mutex_t	 tLock ;				            /*< 互斥锁 */
}QUEUE_HEAD;

/*------------------------------------------------------------------------------
Section: 消息盒子
------------------------------------------------------------------------------*/
typedef struct 
{
    unsigned int     Queue_Cnt;                         /*< 已注册设备数 */
    QUEUE_HEAD       Queue[QUEUE_BOX_SIZE];             /*< 已注册设备消息队列头 */
}QUEUE_BOX;

/*------------------------------------------------------------------------------
Section: 消息结构（单链表）
------------------------------------------------------------------------------*/
typedef struct _QUEUE_NOTE
{
    struct _QUEUE_NOTE *pNext;                          /*< 下一个节点 */
    QUEUE_MSG           MsgBody;                        /*< 消息体 */
 
}QUEUE_NODE;

#pragma pack()  /* 1字节对齐结束 */

/*------------------------------------------------------------------------------
Section: 本地变量
------------------------------------------------------------------------------*/
static QUEUE_BOX  s_QueueBox = { 0 };                   /*< 消息盒子 */


/**
******************************************************************************
* @brief       获取消息队列头信息
* @param[in]   char *Name           设备名
* @param[out]  NONE
* @retval      返回消息队列头
* @details     1. 根据设备名查找消息队列头
* @note        NONE
******************************************************************************
*/
static QUEUE_HEAD *Get_QueueHead(char *DevName)
{
    unsigned char len = strlen(DevName);
    QUEUE_BOX *pQueueBox = &s_QueueBox;
    QUEUE_HEAD *pQueueHead = NULL;
    unsigned int index = 0;

    len = (len >= QUEUE_NAME_SIZE) ? QUEUE_NAME_SIZE : len;

    /* 判断是否有设备注册 */
    if (0 == pQueueBox->Queue_Cnt)
    {
        return NULL;
    }

    /* 在注册列表中获取已注册设备消息队列头 */
    for (index = 0; index < pQueueBox->Queue_Cnt; index++)
    {
        pQueueHead = &pQueueBox->Queue[index];

        if (!memcmp(DevName, pQueueHead->cName, len))
        {
            return pQueueHead;
        }
    }

    return NULL;
}

/**
******************************************************************************
* @brief       注册消息队列
* @param[in]   char *Name           设备名
* @param[out]  NONE
* @retval      返回消息队列头
* @details     
* @note        1. 设备名长度不能超过8个字节
******************************************************************************
*/
static QUEUE_HEAD *Register_to_QueueBox(char *DevName)
{
    QUEUE_HEAD * pQueueHead = NULL;
    unsigned char len = strlen(DevName);

    len = (len >= QUEUE_NAME_SIZE) ? QUEUE_NAME_SIZE : len;

    /* 判断是已设备满了 */
    if (s_QueueBox.Queue_Cnt >= QUEUE_BOX_SIZE)
    {
        return NULL;
    }

    /* 获取消息队列头 */
    pQueueHead = Get_QueueHead(DevName);

    /* 如果没有注册过 */
    if (NULL == pQueueHead)
    {
        /* 在删除队列信息的时候，注意将后面的队列往前移动 */
        pQueueHead = &s_QueueBox.Queue[s_QueueBox.Queue_Cnt];
        s_QueueBox.Queue_Cnt++;
        memcpy(pQueueHead->cName, DevName, len);

        /* 初始化互斥锁 */
	    if (0 != pthread_mutex_init(&pQueueHead->tLock ,NULL))
	    {
	    	return pQueueHead;
	    }
    }

    return NULL;
}

/**
******************************************************************************
* @brief       创建空节点
* @param[in]   QUEUE_HEAD *pHead        消息队列头
* @param[in]   void *pNode              当前节点处
* @param[in]   unsigned int QueueSize   消息队列缓存区大小
* @param[out]  NONE
* @retval      返回空节点
* @details     
* @note        注意不要溢出
******************************************************************************
*/
static QUEUE_NODE* Create_NullNode(QUEUE_HEAD *pHead ,void *pNode ,unsigned int QueueSize)
{
    QUEUE_NODE *pCurNode = (QUEUE_NODE *)pNode;

    /* 判断创建空节点后是否会溢出 */
	if (((((unsigned int)pCurNode) - pHead->FirstNodeAddr) + sizeof(QUEUE_NODE)) > QueueSize)
	{
		return NULL ;
	}

    /* 初始化空节点 */
    pCurNode->pNext = NULL;
    pCurNode->MsgBody.MsgID = 0;
    pCurNode->MsgBody.DataLen = 0;
    pCurNode->MsgBody.pData = (unsigned char *)pCurNode + sizeof(QUEUE_NODE);

	return pCurNode ;
}

/**
******************************************************************************
* @brief       创建消息队列
* @param[in]   char *DevName            设备名
* @param[in]   void *pQueue             消息队列缓存区
* @param[in]   unsigned int QueueSize   消息队列缓存区大小
* @param[out]  NONE
* @retval      TRUE-成功， FALSE-失败
* @details     
* @note       
******************************************************************************
*/
bool Creat_Queue(char *DevName, void *pQueue, unsigned int QueueSize)
{
    QUEUE_HEAD *pQueueHead = NULL;
    QUEUE_NODE *pCurNode = NULL;

    /* 在消息队列盒子中注册消息队列相关信息 */
    pQueueHead == Register_to_QueueBox(DevName);

    /* 注册失败或者已经注册 */
    if (NULL == pQueueHead)
    {
        return FALSE;
    }

    /* 尾节点永远为空节点 */
    pCurNode = Create_NullNode(pQueueHead, pQueue, QueueSize);

    if (NULL == pCurNode)
    {
        return FALSE;
    }

    /* 尾节点永远为空节点 */
    pQueueHead->QueueSize = QueueSize;
    pQueueHead->FirstNodeAddr = (unsigned int)pQueue;
    pQueueHead->LastNodeAddr = (unsigned int)pQueue;
    return TRUE;
}

/**
******************************************************************************
* @brief       插入节点
* @param[in]   char *DevName            设备名
* @param[in]   QUEUE_MSG *pMsg          消息信息
* @param[out]  NONE
* @retval      TRUE-成功， FALSE-失败
* @details     
* @note       
******************************************************************************
*/
bool Push_Node(char *DevName, QUEUE_MSG *pMsg)
{
    QUEUE_HEAD *pQueueHead = NULL;
    QUEUE_NODE *pTailNode = NULL;
    QUEUE_NODE *pNullNode = NULL;
    bool bRet = FALSE;

    if (NULL == pMsg)
    {
        return bRet;
    }

    /* 获取消息队列头 */
    pQueueHead = Get_QueueHead(DevName);

    if (NULL == pQueueHead)
    {
        return bRet;
    }

    /* 互斥锁加锁 */
	pthread_mutex_lock(&pQueueHead->tLock);

    /* 获取尾节点 */
    pTailNode = (QUEUE_NODE *)pQueueHead->LastNodeAddr;

    /* 在尾节点后创建空节点 */
    pNullNode = Create_NullNode(pQueueHead, (void *)((unsigned char *)pTailNode +\
             sizeof(QUEUE_NODE) + pMsg->DataLen), pQueueHead->QueueSize);

     /* 创建成功则 */
    if (NULL != pNullNode)
    {
        pTailNode->MsgBody.MsgID = pMsg->MsgID;
        pTailNode->MsgBody.DataLen = pMsg->DataLen;
        memcpy(pTailNode->MsgBody.pData, pMsg->pData, pMsg->DataLen);
        pTailNode->pNext = pNullNode;

        /* 插入成功，更新尾节点信息 */
        pQueueHead->LastNodeAddr = (unsigned int)pNullNode;
        bRet = TRUE;
    }

    /* 互斥锁解锁 */
    pthread_mutex_unlock(&pQueueHead->tLock);

    return bRet;
}

/**
******************************************************************************
* @brief       取出节点
* @param[in]   char *DevName            设备名
* @param[in]   unsigned int maxBufSize  外部拷贝数据得缓存区大小
* @param[out]  QUEUE_MSG *pMsg          取出得消息信息
* @retval      TRUE-成功， FALSE-失败
* @details     
* @note        todo: 可以加timeout时间优化
******************************************************************************
*/
bool Pob_Node(char *DevName, QUEUE_MSG *pMsg, unsigned int maxBufSize)
{
    unsigned int copyLen = 0, dataLen = 0;;
	QUEUE_HEAD *pQueueHead = Get_QueueHead(DevName);
    QUEUE_NODE  strNextNode ;
    QUEUE_NODE *pNode = NULL;
    QUEUE_NODE *pNewNode = NULL;

    if (NULL == pQueueHead || NULL == pMsg)
    {
        return FALSE;
    }

    /* 获取头节点 */
    pNode = (QUEUE_NODE *)pQueueHead->FirstNodeAddr;

    if (NULL == pNode->pNext)
    {
        return FALSE;
    }

    /* 互斥锁加锁 */
	pthread_mutex_lock(&pQueueHead->tLock);

    /* 防止外部缓存区溢出 */
    copyLen = pNode->MsgBody.DataLen;
    copyLen = (copyLen >= maxBufSize) ? maxBufSize : copyLen;

    /* 拷出取出的消息 */
    pMsg->MsgID = pNode->MsgBody.MsgID;
    pMsg->DataLen = copyLen;
    memcpy(pMsg->pData, pNode->MsgBody.pData, copyLen);

    /* 备份下头节点信息 */
    strNextNode = pNode[0];

    /* 在头节点处创建空节点 */
    pNewNode = Create_NullNode(pQueueHead, (void *)pNode, pQueueHead->QueueSize);

    /* 后面节点往前挪 */
    for (;;)
    {
        pNode = strNextNode.pNext;

        if (NULL == pNode || NULL == pNode->pNext || NULL == pNewNode)
        {
            break;
        }

        pNewNode->MsgBody.MsgID = pNode->MsgBody.MsgID;
        pNewNode->MsgBody.DataLen = pNode->MsgBody.DataLen;
        pNewNode->MsgBody.pData = (unsigned char *)pNewNode + sizeof(QUEUE_NODE);

        /* 后一个节点的数据区长度 */
        dataLen = (unsigned int)pNode->pNext - (unsigned int)pNode->MsgBody.pData;

        memmove((void *)pNewNode->MsgBody.pData, (void *)pNode->MsgBody.pData, dataLen);

        pNewNode->pNext = Create_NullNode(pQueueHead, (void *)((unsigned int)pNewNode->MsgBody.pData
        + dataLen), pQueueHead->QueueSize);

        pNewNode = pNewNode->pNext;
    }

    /* 互斥锁解锁 */
    pthread_mutex_unlock(&pQueueHead->tLock);

    return TRUE;
}

/**
******************************************************************************
* @brief       删除消息队列
* @param[in]   char *DevName            设备名
* @param[out]  NONE
* @retval      NONE
* @details     
* @note        todo: 在摧毁队列的时候，需要加互斥量保护
******************************************************************************
*/
void Destroy_Queue(char *DevName)
{
    unsigned char len = strlen(DevName);
    QUEUE_BOX *pQueueBox = &s_QueueBox;
    QUEUE_HEAD *pQueueHead = NULL;
    unsigned int index = 0;

    len = (len >= QUEUE_NAME_SIZE) ? QUEUE_NAME_SIZE : len;

    /* 判断是否有设备注册 */
    if (0 == pQueueBox->Queue_Cnt)
    {
        return;
    }

    /* 在注册列表中获取已注册设备消息队列头 */
    for (index = 0; index < pQueueBox->Queue_Cnt; index++)
    {
        pQueueHead = &pQueueBox->Queue[index];

        if (!memcmp(DevName, pQueueHead->cName, len))
        {
            pthread_mutex_destroy(&pQueueHead->tLock);
            memset(pQueueHead, 0x00 ,sizeof(QUEUE_HEAD));
            memmove(pQueueHead, pQueueHead + 1, sizeof(QUEUE_HEAD) * (QUEUE_BOX_SIZE - index - 1));
            memset(&pQueueBox->Queue[QUEUE_BOX_SIZE - 1], 0x00, sizeof(QUEUE_HEAD));
            s_QueueBox.Queue_Cnt--;
            break;
        }
    }
}

















