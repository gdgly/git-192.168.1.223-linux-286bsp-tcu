#ifndef     _QUEUE_H
#define     _QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TRUE
    typedef unsigned char  bool;
#define TRUE    1
#define FALSE   0
#endif


/*------------------------------------------------------------------------------
Section: 设备名长度
------------------------------------------------------------------------------*/
#define     QUEUE_NAME_SIZE     8       

/*------------------------------------------------------------------------------
Section: 消息队列个数
------------------------------------------------------------------------------*/
#define     QUEUE_BOX_SIZE      5

/*------------------------------------------------------------------------------
Section: 消息体
------------------------------------------------------------------------------*/
typedef struct 
{
    int                 MsgID;          /* 消息ID */
    unsigned int        DataLen;        /* 消息长度 */
    unsigned char      *pData;          /* 消息内容*/
}QUEUE_MSG;

/*------------------------------------------------------------------------------
Section: 外部接口
------------------------------------------------------------------------------*/
/* 创建队列 */
bool Creat_Queue(char *DevName, void *pQueue, unsigned int QueueSize);

/* 插入节点 */
bool Push_Node(char *DevName, QUEUE_MSG *pMsg);

/* 取出节点 */
bool Pob_Node(char *DevName, QUEUE_MSG *pMsg, unsigned int maxBufSize);

/* 摧毁队列 */
void Destroy_Queue(char *DevName);

#ifdef __cplusplus
    }
#endif

#endif /* _QUEUE_H */







