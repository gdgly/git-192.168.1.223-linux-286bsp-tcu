//***************************************************************************** 
//
// ʵ�ֶ�PC��iSearch�����������������Ӧ��
//
//*****************************************************************************

#ifndef __ISEARCH_SERVICE_H__
#define __ISEARCH_SERVICE_H__

/////////////////////////////////////////////////////////////////////////////////


#define ISEARCH_DEVICE_DISCOVERY   0xEA00  //�Զ���iSearch�����豸�ķ���������ʹ�õ�ARPЭ���е�Э�������ֶ�����������(��IP���ݱ�0x0800)����ͻ----ע����ֽڹ̶�Ϊ0xEA�����ֽ��Ǳ仯��!!


//����iSearch����֡��protocol�ֶ�ֵ
#define ISEARCH_DEVICE_DISCOVERY_REQ       0xEA01//�豸��������0xEA05����һ��
#define ISEARCH_DEVICE_DISCOVERY_ACK       0xEA02

#define ISEARCH_DEVICE_ALIVE_CHK_REQ       0xEA03//����豸�Ƿ���Ȼ����
#define ISEARCH_DEVICE_ALIVE_CHK_ACK       0xEA04

#define ISEARCH_DEVICE_GET_INFO_REQ        0xEA05//��ȡIP��ַ�����룬�˿ڵ���Ϣ
#define ISEARCH_DEVICE_GET_INFO_ACK        0xEA06

#define ISEARCH_DEVICE_SET_INFO_REQ        0xEA07//����IP��ַ�����룬�˿ڵ���Ϣ
#define ISEARCH_DEVICE_SET_INFO_ACK        0xEA08

// #define ISEARCH_DEVICE_SET_MAC_REQ         0xEA09 //�ֶ�����MAC�����к�
// #define ISEARCH_DEVICE_SET_MAC_ACK         0xEA0A

#define ISEARCH_DEVICE_SET_MAC_REQ         0xEA1A //�ֶ�����MAC
//#define ISEARCH_DEVICE_SET_MAC_ACK         0xEA1A

#define ISEARCH_DEVICE_SET_SN_REQ         0xEA19 //�������к�
//#define ISEARCH_DEVICE_SET_SN_ACK         0xEA0A

#define ISEARCH_DEVICE_RESTORE_REQ         0xEA0B  //�ָ�����ֵ
#define ISEARCH_DEVICE_RESTORE_ACK         0xEA0C  




/////////////////////////////////////////////////////////////////////////////////

extern int iSearch_Packetin(char* recv_buf,int rx_len,char* tx_buf);//������յ���iSearch�������̫��discovery������֡



#endif // __ISEARCH_SERVICE_H__
