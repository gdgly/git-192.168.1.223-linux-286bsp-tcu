#ifndef _PLAT_PROT_H
#define _PLAT_PROT_H

#ifdef __cplusplus
extern "C" {
#endif



/* ������Ϣ���� */
extern bool PlatProt_Init(void);

/* ��Ϣ���н��� */
void PlatProt_MsgRecvPoll(unsigned char* pRecv_buf, unsigned short buf_size);



#ifdef __cplusplus
    }
#endif

#endif