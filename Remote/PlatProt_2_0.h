#ifndef _PLAT_PROT_H
#define _PLAT_PROT_H

#ifdef __cplusplus
extern "C" {
#endif



/* 创建消息队列 */
extern bool PlatProt_Init(void);

/* 消息队列接收 */
void PlatProt_MsgRecvPoll(unsigned char* pRecv_buf, unsigned short buf_size);



#ifdef __cplusplus
    }
#endif

#endif