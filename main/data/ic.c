/*******************************************************************************
  Copyright(C)    EAST Co.Ltd.
  File name :     ic.c
  Author :        
  Version:        1.0
  Date:          
  Description:    铭特MT625 M1卡和CPU卡合并程序
  Other:          
  Function List:  
  History:        
          <author>   <time>   <version>   <desc>
                                 1.0      Create
*******************************************************************************/
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "common.h"
#include "globalvar.h"
#define MT625_DEBUG 1        //调试使能
extern int valid_card_check(unsigned char *sn);
/**------------------------------------------------------------------------------
M1卡指令         
**------------------------------------------------------------------------------*/
//寻卡
unsigned char IC_Search_Cmd[ ]  = {0x02,0x00,0x02,0x34,0x30,0x03,0x07};

//验证扇区0 Key_A密码                        
unsigned char IC_0KeyA_Cmd[ ] = {0x02,0x00,0x09,0x34,0x32,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x03,0x0E};
//验证扇区1 Key_A密码                        
unsigned char IC_1KeyA_Cmd[ ] = {0x02,0x00,0x09,0x34,0x32,0x01,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x03,0x0F};
//验证扇区2 Key_A密码                        
unsigned char IC_2KeyA_Cmd[ ] = {0x02,0x00,0x09,0x34,0x32,0x02,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x03,0x0C};
//验证扇区3 Key_A密码                        
unsigned char IC_3KeyA_Cmd[ ] = {0x02,0x00,0x09,0x34,0x32,0x03,0x24,0x66,0x99,0x74,0x65,0x47,0x03,0x80};

//验证扇区4 Key_A密码 
unsigned char IC_4KeyA_Cmd[ ] = {0x02,0x00,0x09,0x34,0x32,0x04,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x03,0x0E};

//读扇区0块1充电卡号
unsigned char IC_01_read_Cmd[ ] = {0x02,0x00,0x04,0x34,0x33,0x00,0x01,0x03,0x03};
//读扇区1块0卡密码
unsigned char IC_10_read_Cmd[ ] = {0x02,0x00,0x04,0x34,0x33,0x01,0x00,0x03,0x03};
//读扇区1块1锁卡标志
unsigned char IC_11_read_Cmd[ ] = {0x02,0x00,0x04,0x34,0x33,0x01,0x01,0x03,0x02};
//读扇区1块2最后一次操作的桩序列号
unsigned char IC_12_read_Cmd[ ] = {0x02,0x00,0x04,0x34,0x33,0x01,0x02,0x03,0x01};
//读扇区2块0存卡余额
unsigned char IC_20_read_Cmd[ ] = {0x02,0x00,0x04,0x34,0x33,0x02,0x00,0x03,0x00};
//读扇区3块0代理商密钥
unsigned char IC_30_read_Cmd[ ] = {0x02,0x00,0x04,0x34,0x33,0x03,0x00,0x03,0x01};

//读扇区4块0代理商密钥
unsigned char IC_40_read_Cmd[ ] = {0x02,0x00,0x04,0x34,0x33,0x04,0x00,0x03,0x01};

//写扇区1块1数据0xAA，启动充电时写0xAA表明卡已上锁                        0    1   2    3    4    5    6    7    8    9   10   11   12   13   14   15
unsigned char IC_0xAA_write_Cmd[ ]= {0x02,0x00,0x14,0x34,0x34,0x01,0x01,0xAA,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0xBF};
//写扇区1块1数据0x55，充电结束写完余额后写0x55表明卡已解锁                0    1   2    3    4    5    6    7    8    9   10   11   12   13   14   15
unsigned char IC_0x55_write_Cmd[ ]= {0x02,0x00,0x14,0x34,0x34,0x01,0x01,0x55,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x40};
//写操作，对扇区2块0的金额进行扣费后写入                                  0    1   2    3    4    5    6    7    8    9   10   11   12   13   14   15           
unsigned char IC_Pay_Delete_Cmd[ ]= {0x02,0x00,0x14,0x34,0x34,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x07};

/**------------------------------------------------------------------------------
CPU卡指令         
**------------------------------------------------------------------------------*/
//寻卡
unsigned char IC_SPG_Select_Cmd[ ]  = {0x02,0x00,0x02,0x51,0x37,0x03,0x65};
//预处理
//unsigned char IC_SPG_ProDeal_Cmd[ ] = {0x02,0x00,0x09,0x51,0x36,0x20,0x14,0x05,0x08,0x08,0x08,0x08,0x03,0x5E};
//                                       0    1   2    3    4    5    6    7    8    9   10   11   12   13   14   15   16     
unsigned char IC_SPG_ProDeal_Cmd[ ] = {0x02,0x00,0x0C,0x51,0x36,0x20,0x14,0x05,0x08,0x08,0x08,0x08,0x12,0x34,0x56,0x03,0x5E};
//开始加电
unsigned char IC_SPG_Start_Cmd[ ]   = {0x02,0x00,0x09,0x51,0x30,0x20,0x14,0x05,0x08,0x08,0x08,0x08,0x03,0x58};
//结束加电
unsigned char IC_SPG_Stop_Cmd[ ]    = {0x02,0x00,0x0D,0x51,0x34,0x00,0x00,0x00,0x00,0x20,0x14,0x05,0x08,0x08,0x08,0x08,0x03,0x58};
//补充交易
unsigned char IC_SPG_Supp_Cmd[ ]    = {0x02,0x00,0x02,0x51,0x35,0x03,0x67};
//弹卡
unsigned char IC_SPG_Push_Cmd[ ]    = {0x02,0x00,0x02,0x32,0x40,0x03,0x71};

unsigned char IC_com_Find_flag = 0;
/******************************************************************************
** Function name:       MT625_BCC 异或校验
** Descriptions:	    计算一组数据的异或值
** Input parameters:	buf---数据缓冲区指针
						          len---数据的字节长度
** Output parameters:	
** Returned value:		数据的异或值        
** Created by:		
** Created Date:		
**------------------------------------------------------------------------------
** Modified by:            
** Modified date:          
**------------------------------------------------------------------------------
*******************************************************************************/
static UINT8 MT625_BCC(UINT8 *buf, INT32 len)
{
 INT32 i;
 UINT32 checksum = 0;

 for(i = 0; i < len; i++) {
  checksum ^= *buf;
  buf++;
 }

 return ((UINT8)checksum);
}

/******************************************************************************
** Function name:       MT625_Card_Search
** Descriptions:	     M1卡、CPU卡  搜索卡
** Input parameters:						
** Output parameters:	 
** Returned value:		        
** Created by:		
** Created Date:		
**------------------------------------------------------------------------------
** 修改部分:刷卡器添加了一个M1卡和CPU卡区别      
** Modified date:          
**------------------------------------------------------------------------------
*******************************************************************************/
int MT625_Card_Search(int fd)
{
	int Ret = 0, Count = 0;
	unsigned char Buf[MAX_MODBUS_FRAMESIZE] = {0};
	usleep(20000);
	Count = write(fd, IC_Search_Cmd, sizeof(IC_Search_Cmd));
	if(Count == sizeof(IC_Search_Cmd)){
		Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 15000);
    //printf("搜索卡:Count = %d Buf[5]= %d\n",Count,Buf[5]);
		if(Count == 8){
			if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
				IC_com_Find_flag = 0;
				if(Buf[5] == 0x59){
					return (1);//寻找到M1或者CPU卡
				}else if(Buf[5] == 0x4E){
					return (2);//寻卡不成功
				}
			}
		}
	}
	IC_com_Find_flag = 1;
	return (0);

}

/******************************************************************************
** Function name:       MT625_Card_number
** Descriptions:	    M1卡 读扇区0块1充电卡号
** Input parameters:	
                        					
** Output parameters:	
** Returned value:		      
** Created by:		
** Created Date:		
**------------------------------------------------------------------------------
** Modified by:            
** Modified date:          
**------------------------------------------------------------------------------
*******************************************************************************/
int MT625_Card_number(int fd,unsigned char *sn)
{   
  int Ret = 0, Count = 0,Count1 = 0;
	unsigned char Buf[MAX_MODBUS_FRAMESIZE] = {0};
	Count = write(fd, IC_0KeyA_Cmd, sizeof(IC_0KeyA_Cmd));
	if(Count == sizeof(IC_0KeyA_Cmd)){
		Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
		while(Count == 8 ){//防止收到的数据为其他的
			Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
			//printf("M1卡 验证扇区0块密钥:Count = %d Buf[6]= %d\n",Count,Buf[6]);
		}
		//printf("M1卡 验证扇区0块密钥:Count = %d Buf[6]= %d\n",Count,Buf[6]);
			if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
				if(Count == 9){
					if(Buf[6] == 0x59){
						usleep(30000);
						Count1= write(fd, IC_01_read_Cmd, sizeof(IC_01_read_Cmd));
						if(Count1 == sizeof(IC_01_read_Cmd)){
							Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
						//	printf("M1卡 读扇区0块1充电卡号:Count = %d Buf[7]= %d\n",Count,Buf[7]);
							if(Count == 26){
								if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
									if(Buf[7] == 0x59){
										 bcd_to_str(sn,  &Buf[8], 8);
										 if(Buf[8] == 0x69){//管理卡
											 if(MT625_DEBUG) printf("MT625_Card_Manager_Card_number %x\r\n", Buf[8]);
											 return (3);
										 }else if(Buf[8] == 0x68){//员工卡
											 if(MT625_DEBUG) printf("MT625_worker_Card_number %x\r\n", Buf[8]);
												 return (4);
										 }else{//if((Buf[8] >= 0x60)&&((Buf[8] < 0x68))||(Buf[8] == 0x00)
											 if(MT625_DEBUG)  printf("MT625_User_Card_number %x\r\n", Buf[8]);
											 return (5);
										 }
									}else{
										if(MT625_DEBUG)printf("MT625_Card_number fail\r\n");
										return (6);
									}//充电卡号读取失败
								}
							}
						}
					}else{ 
						if(MT625_DEBUG) printf("MT625_Card_number fail 000000 =%d \r\n",Buf[6]);
						return (7);
					}//验证扇区0 KeyA失败
				}
			}
    //}
  }
	return (0);

}


/******************************************************************************
** Function name:       MT625_Card_Password
** Descriptions:	    M1卡 读扇区1块0卡密码
** Input parameters:	
                       					
** Output parameters:	
** Returned value:		     
** Created by:		
** Created Date:		
**------------------------------------------------------------------------------
** Modified by:            
** Modified date:          
**------------------------------------------------------------------------------
*******************************************************************************/
int MT625_Card_Password(int fd,unsigned char *pw)
{  
  int Ret = 0, Count = 0, Count1 = 0;
	unsigned char Buf[MAX_MODBUS_FRAMESIZE] = {0};
	memcpy(&IC_1KeyA_Cmd[6], &Globa_1->Charger_param.Key[0], 6);
	IC_1KeyA_Cmd[13] = MT625_BCC(IC_1KeyA_Cmd, 13);
	Count = write(fd, IC_1KeyA_Cmd, sizeof(IC_1KeyA_Cmd));
	if(Count == sizeof(IC_1KeyA_Cmd)){
		Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
    if(Count == 9){
			if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
				if(Buf[6] == 0x59){
					usleep(30000);
				  Count1= write(fd, IC_10_read_Cmd, sizeof(IC_10_read_Cmd));
				  if(Count1 == sizeof(IC_10_read_Cmd)){
		        Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
						if(Count == 26){
			        if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
							  if(Buf[7] == 0x59){
								  bcd_to_str(pw, &Buf[8], 6);
								  if(MT625_DEBUG) printf("MT625_Card_password ok\r\n");
								  return(8);//成功读取用户密码
			          }else{
								  if(MT625_DEBUG) printf("MT625_Card_password fail\r\n");
				          return(9);
							  }//读取用户密码失败
				      }
	          }
	        }
			  }else{
					if(MT625_DEBUG) printf("MT625_Card_password fail 111111\r\n");
		      return (10);
				}//验证扇区1 KeyA失败
      }
    }
  }
	return (0);

}

/******************************************************************************
** Function name:       MT625_Card_keysign
** Descriptions:	    M1卡 读扇区1块1锁卡标志 及对应时间
** Input parameters:	
                       					
** Output parameters:	
** Returned value:		       
** Created by:		
** Created Date:		
**------------------------------------------------------------------------------
** Modified by:            
** Modified date:          
**------------------------------------------------------------------------------
*******************************************************************************/
int MT625_Card_keysign(int fd)
{   
  int Ret = 0, Count = 0, Count1 = 0;
	unsigned char Buf[MAX_MODBUS_FRAMESIZE] = {0};
	memcpy(&IC_1KeyA_Cmd[6], &Globa_1->Charger_param.Key[0], 6);
	IC_1KeyA_Cmd[13] = MT625_BCC(IC_1KeyA_Cmd, 13);
	
	Count = write(fd, IC_1KeyA_Cmd, sizeof(IC_1KeyA_Cmd));
	if(Count == sizeof(IC_1KeyA_Cmd)){
		Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
    if(Count == 9){
			if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
				if(Buf[6] == 0x59){
					usleep(30000);
				  Count1= write(fd, IC_11_read_Cmd, sizeof(IC_11_read_Cmd));
				  if(Count1 == sizeof(IC_11_read_Cmd)){
						Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
				    if(Count == 26){
			        if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
							  if(Buf[7] == 0x59){
								  if(Buf[8] == 0xAA){//表明此卡已被锁，未结算
										if(MT625_DEBUG) printf("MT625_Card lock\r\n");
										Globa_1->User_Card_Lock_Info.card_lock_time.year_H = bcd_to_int_1byte(Buf[9]);//2016年的0x20
										Globa_1->User_Card_Lock_Info.card_lock_time.year_L = bcd_to_int_1byte(Buf[10]);//2016年的0x16
										Globa_1->User_Card_Lock_Info.card_lock_time.month = bcd_to_int_1byte(Buf[11]);
										Globa_1->User_Card_Lock_Info.card_lock_time.day_of_month = bcd_to_int_1byte(Buf[12]);
										Globa_1->User_Card_Lock_Info.card_lock_time.hour = bcd_to_int_1byte(Buf[13]);
										Globa_1->User_Card_Lock_Info.card_lock_time.minute = bcd_to_int_1byte(Buf[14]);
										Globa_1->User_Card_Lock_Info.card_lock_time.second = bcd_to_int_1byte(Buf[15]);
								    return(11);
									}else if(Buf[8] == 0x55){
								  	if(MT625_DEBUG)printf("----MT625_Card unlock------\r\n");
								    return(12);
									}//表明此卡处于解锁，已结算
		            }else{
								   if(MT625_DEBUG)printf("---MT625_Card lock fail---\r\n");
				          return(13);
							  }//读取锁卡标志失败
				      }
	          }
	        }
			  }else{ 
				  if(MT625_DEBUG) printf("MT625_Card lock fail 000000\r\n");
		      return (14);
				}//验证扇区1 KeyA失败
      }
    }
  }
  return (0);
}		

/******************************************************************************
** Function name:       MT625_Card_ZXLH
** Descriptions:	    M1卡 读扇区1块2最后一次操作的桩序列号
** Input parameters:	
                       					
** Output parameters:	
** Returned value:		      
** Created by:		
** Created Date:		
**------------------------------------------------------------------------------
** Modified by:            
** Modified date:          
**------------------------------------------------------------------------------
*******************************************************************************/
int MT625_Card_ZXLH(int fd,unsigned char *XLH)
{   
  int Ret = 0, Count = 0, Count1 = 0;
	unsigned char Buf[MAX_MODBUS_FRAMESIZE] = {0};
	memcpy(&IC_1KeyA_Cmd[6], &Globa_1->Charger_param.Key[0], 6);
	IC_1KeyA_Cmd[13] = MT625_BCC(IC_1KeyA_Cmd, 13);
	Count = write(fd, IC_1KeyA_Cmd, sizeof(IC_1KeyA_Cmd));
	if(Count == sizeof(IC_1KeyA_Cmd)){
		Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
    if(Count == 9){
			if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
				if(Buf[6] == 0x59){
					usleep(30000);
				  Count1= write(fd, IC_12_read_Cmd, sizeof(IC_12_read_Cmd));
				  if(Count1 == sizeof(IC_12_read_Cmd)){
						Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
				    if(Count == 26){
			        if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
						    if(Buf[7] == 0x59){
								  bcd_to_str(XLH, &Buf[8], 12);
									if(MT625_DEBUG) printf("MT625_Card_ZXLH ok\r\n");
								  return(15);//成功读取扇区1块2最后一次操作的桩序列号
		            }else{ 
								  if(MT625_DEBUG)  printf("MT625_Card_ZXLH fail\r\n");
				          return(16);
							  }//读取最后一次操作的桩序列号失败
				      }
	          }
	        }
			  }else{ 
   				if(MT625_DEBUG)  printf("MT625_Card_ZXLH fail 111111\r\n");
		      return (17);
			  }//验证扇区1 KeyA失败
      }
    }
  }
	return (0);
}

/******************************************************************************
** Function name:       MT625_Card_Money
** Descriptions:	    M1卡 读扇区2块0存卡余额	
** Input parameters:	
                       					
** Output parameters:	
** Returned value:		      
** Created by:		
** Created Date:		
**------------------------------------------------------------------------------
** Modified by:            
** Modified date:          
**------------------------------------------------------------------------------
*******************************************************************************/
int MT625_Card_Money(int fd,unsigned int *rmb)
{   
  int Ret = 0, Count = 0, Count1 = 0;
	unsigned char Buf[MAX_MODBUS_FRAMESIZE] = {0};
	memcpy(&IC_2KeyA_Cmd[6], &Globa_1->Charger_param.Key[0],6);
	IC_2KeyA_Cmd[13] = MT625_BCC(IC_2KeyA_Cmd, 13);
	Count = write(fd, IC_2KeyA_Cmd, sizeof(IC_2KeyA_Cmd));
	if(Count == sizeof(IC_2KeyA_Cmd)){
		Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
    if(Count == 9){
			if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
				if(Buf[6] == 0x59){
					usleep(30000);
				  Count1= write(fd, IC_20_read_Cmd, sizeof(IC_20_read_Cmd));
				  if(Count1 == sizeof(IC_20_read_Cmd)){
		        Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
				    if(Count == 26){
			        if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
							  if(Buf[7] == 0x59){
								  *rmb = (Buf[11]<<24)+(Buf[10]<<16)+(Buf[9]<<8)+(Buf[8]);
								  if(MT625_DEBUG) printf("MT625_Card_rmb ok \r\n");
									 return(18);//成功读取扇区2块0卡金额
		            }else{
									if(MT625_DEBUG)printf("MT625_Card_rmb fail \r\n");
				          return(19);
							  }//读取卡金额失败
				      }
	          }
	        }
			  }else{ 
				  if(MT625_DEBUG)printf("MT625_Card_rmb fail 111111\r\n");
		      return (20);
			  }//验证扇区2 KeyA失败
      }
    }
  }
	return (0);

}		

/******************************************************************************
** Function name:       MT625_Card_Key
** Descriptions:	    M1卡 读扇区3块0代理商密钥
** Input parameters:	
                       					
** Output parameters:	
** Returned value:		   
** Created by:		
** Created Date:		
**------------------------------------------------------------------------------
** Modified by:            
** Modified date:          
**------------------------------------------------------------------------------
*******************************************************************************/
int MT625_Card_Key(int fd)
{   
  int Ret1 = 0, Count = 0, Count1 = 0,i;
	UINT8  KEY[]={0x56,0x86,0x63,0x20,0x08,0x16,0x42,0x29};
	UINT8  old_Key[6];
	UINT8  DData[6];
	unsigned char Buf[MAX_MODBUS_FRAMESIZE] = {0};
	Count = write(fd, IC_3KeyA_Cmd, sizeof(IC_3KeyA_Cmd));
	if(Count == sizeof(IC_3KeyA_Cmd)){
		Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
    if(Count == 9){
			if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
				if(Buf[6] == 0x59){
					usleep(30000);
				  Count1= write(fd, IC_30_read_Cmd, sizeof(IC_30_read_Cmd));
				  if(Count1 == sizeof(IC_30_read_Cmd)){
		        Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
				    if(Count == 26){
			        if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
							  if(Buf[7] == 0x59){
								  Ret1 = DES_to_min(&Buf[8], 8,  KEY, DData);
								  if(0 != memcmp(&Globa_1->Charger_param.Key[0], &DData[0], sizeof(DData))){
											old_Key[0] = 0xFF;
											old_Key[1] = 0xFF;
											old_Key[2] = 0xFF;
											old_Key[3] = 0xFF;
											old_Key[4] = 0xFF;
											old_Key[5] = 0xFF;
										if(0 == memcmp(&Globa_1->Charger_param.Key[0], &old_Key[0], sizeof(Globa_1->Charger_param.Key))){
											memcpy(&Globa_1->Charger_param.Key[0], &DData[0], sizeof(DData));
											System_param_save();
											if(MT625_DEBUG) printf("MT625_Card_key System_param_save ok \r\n");
										}else{
											for(i=0;i<sizeof(Globa_1->Charger_param.Key);i++){
											  if(MT625_DEBUG) printf("Globa_1->Charger_param.Key[%d]= %x \n",i,Globa_1->Charger_param.Key[i]);
												if(MT625_DEBUG) printf("old_Key[%d]= %x \n",i,old_Key[i]);
												if(MT625_DEBUG) printf("DData[%d]= %x \n",i,DData[i]);

											}
											return(22);
										}
								  }
								  if(MT625_DEBUG) printf("MT625_Card_key ok \r\n");
								  return(21);//成功读取扇区3块0卡代理商密钥
		            }else{
									if(MT625_DEBUG)printf("MT625_Card_key fail \r\n");
				          return(22);
				        }//读取代理商密钥失败
				      }
	          }
	        }
			  }else{
					if(MT625_DEBUG)printf("MT625_Card_key fail 111111\r\n");
		      return (23);
				}//验证扇区3 KeyA失败
      }
    }
  }
	return (0);
}	
	
/******************************************************************************
** Function name:       MT625_Card_Write0xAA
** Descriptions:	    M1卡 写扇区1块1数据0xAA，启动充电时写0xAA表明卡已上锁   
** Input parameters:	
                       					
** Output parameters:	
** Returned value:		       
** Created by:		
** Created Date:		
**------------------------------------------------------------------------------
** Modified by:            
** Modified date:          
**------------------------------------------------------------------------------
*******************************************************************************/
int MT625_Card_Write0xAA(int fd)
{   
  int Ret =0, Count =0, Count1=0;
	unsigned char Buf[MAX_MODBUS_FRAMESIZE] = {0};
	
	time_t systime=0;
	struct tm tm_t;

	systime = time(NULL);         //获得系统的当前时间 
	localtime_r(&systime,&tm_t);  //结构指针指向当前时间	
	IC_0xAA_write_Cmd[8] = int_to_bcd_1byte((tm_t.tm_year+1900)/100);
	IC_0xAA_write_Cmd[9] = int_to_bcd_1byte((tm_t.tm_year+1900)%100);
	IC_0xAA_write_Cmd[10] = int_to_bcd_1byte(tm_t.tm_mon+1);
	IC_0xAA_write_Cmd[11] = int_to_bcd_1byte(tm_t.tm_mday);
	IC_0xAA_write_Cmd[12] = int_to_bcd_1byte(tm_t.tm_hour);
	IC_0xAA_write_Cmd[13] = int_to_bcd_1byte(tm_t.tm_min);	
	IC_0xAA_write_Cmd[14] = int_to_bcd_1byte(tm_t.tm_sec);
	
	Globa_1->User_Card_Lock_Info.card_lock_time.year_H = ((tm_t.tm_year+1900)/100);//2016年的0x20
	Globa_1->User_Card_Lock_Info.card_lock_time.year_L = ((tm_t.tm_year+1900)%100);//2016年的0x16
	Globa_1->User_Card_Lock_Info.card_lock_time.month = tm_t.tm_mon + 1;
	Globa_1->User_Card_Lock_Info.card_lock_time.day_of_month = tm_t.tm_mday;
	Globa_1->User_Card_Lock_Info.card_lock_time.hour = tm_t.tm_hour;
	Globa_1->User_Card_Lock_Info.card_lock_time.minute = tm_t.tm_min;
	Globa_1->User_Card_Lock_Info.card_lock_time.second = tm_t.tm_sec; //为了保持锁卡时间和开始时间一致
	
  memcpy(&IC_1KeyA_Cmd[6], &Globa_1->Charger_param.Key[0], 6);
	IC_1KeyA_Cmd[13] = MT625_BCC(IC_1KeyA_Cmd, 13);
	IC_0xAA_write_Cmd[24] = MT625_BCC(IC_0xAA_write_Cmd, 24);
	Count = write(fd, IC_1KeyA_Cmd, sizeof(IC_1KeyA_Cmd));
	if(Count == sizeof(IC_1KeyA_Cmd)){
		Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
    if(Count == 9){
			if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
				if(Buf[6] == 0x59){
					usleep(30000);
				  Count1= write(fd, IC_0xAA_write_Cmd, sizeof(IC_0xAA_write_Cmd));
				  if(Count1 == sizeof(IC_0xAA_write_Cmd)){
		        Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
				    if(Count == 10){
			        if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
							  if((Buf[7] == 0x59)){
								 if(MT625_DEBUG) printf("MT625_Card_Write0xAA ok\r\n");
								  return(24);//成功写扇区1块1数据0xAA，启动充电时写0xAA
			          }else{
								 if(MT625_DEBUG) printf("MT625_Card_Write0xAA fail\r\n");
				          return(25);
								}//写扇区1块1数据0xAA，启动充电时写0xAA失败
				
				      }
	          }
	        }
			  }else{
				  if(MT625_DEBUG)printf("MT625_Card_Write0xAA fail 111111\r\n");
		      return (26);
		    }//验证扇区1 KeyA失败
      }
    }
  }
	return (0);
}

/******************************************************************************
** Function name:       MT625_Card_Write0x55
** Descriptions:	    M1卡 写扇区1块1数据0x55，充电结束写完余额后写0x55表明卡已解锁  
** Input parameters:	
                       					
** Output parameters:	
** Returned value:		        
** Created by:		
** Created Date:		
**------------------------------------------------------------------------------
** Modified by:            
** Modified date:          
**------------------------------------------------------------------------------
*******************************************************************************/
int MT625_Card_Write0x55(int fd)
{   
  int Ret = 0, Count = 0, Count1 = 0;
	unsigned char Buf[MAX_MODBUS_FRAMESIZE] = {0};
	
	time_t systime=0;
	struct tm tm_t;

	systime = time(NULL);         //获得系统的当前时间 
	localtime_r(&systime,&tm_t);  //结构指针指向当前时间	
	
  IC_0x55_write_Cmd[8] = int_to_bcd_1byte((tm_t.tm_year+1900)/100);
	IC_0x55_write_Cmd[9] = int_to_bcd_1byte((tm_t.tm_year+1900)%100);
	IC_0x55_write_Cmd[10] = int_to_bcd_1byte(tm_t.tm_mon+1);
	IC_0x55_write_Cmd[11] = int_to_bcd_1byte(tm_t.tm_mday);
	IC_0x55_write_Cmd[12] = int_to_bcd_1byte(tm_t.tm_hour);
	IC_0x55_write_Cmd[13] = int_to_bcd_1byte(tm_t.tm_min);	
	IC_0x55_write_Cmd[14] = int_to_bcd_1byte(tm_t.tm_sec);
	
  memcpy(&IC_1KeyA_Cmd[6], &Globa_1->Charger_param.Key[0], 6);
	IC_1KeyA_Cmd[13] = MT625_BCC(IC_1KeyA_Cmd, 13);
	IC_0x55_write_Cmd[24] = MT625_BCC(IC_0x55_write_Cmd, 24);
	Count = write(fd, IC_1KeyA_Cmd, sizeof(IC_1KeyA_Cmd));
	if(Count == sizeof(IC_1KeyA_Cmd)){
		Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
    if(Count == 9){
			if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
				if(Buf[6] == 0x59){
					usleep(30000);
				  Count1= write(fd, IC_0x55_write_Cmd, sizeof(IC_0x55_write_Cmd));
				  if(Count1 == sizeof(IC_0x55_write_Cmd)){
		        Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
						if(Count == 10){
							if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
								if((Buf[7] == 0x59)){
									if(MT625_DEBUG)printf("MT625_Card_Write0x55 ok\r\n");
									return(27);//成功写扇区1块1数据0x55，充电结束写完余额后写0x55表明卡已解锁
								}else{
									if(MT625_DEBUG)printf("MT625_Card_Write0x55 fail\r\n");
									return(28);
								}//写扇区1块1数据0x55，充电结束写完余额后写0x55失败
							}
						}
					}
			  }else{
				 if(MT625_DEBUG)printf("MT625_Card_Write0x55 fail 111111\r\n");
		      return (29);
			  }//验证扇区1 KeyA失败
      }
    }
  }
  return (0);
}

/******************************************************************************
** Function name:       MT625_Pay_Delete
** Descriptions:	    M1卡 写操作，扣扇区2块0存卡余额后写入该扇区	
** Input parameters:	
                       					
** Output parameters:	
** Returned value:		     
** Created by:		
** Created Date:		
**------------------------------------------------------------------------------
** Modified by:            
** Modified date:          
**------------------------------------------------------------------------------
*******************************************************************************/
int MT625_Pay_Delete(int fd,unsigned int rmb,unsigned int rmb1)
{  
  int Ret = 0, Count = 0, Count1 = 0;
  int rmb2 = 0;
	unsigned char Buf[MAX_MODBUS_FRAMESIZE] = {0};
	memcpy(&IC_2KeyA_Cmd[6], &Globa_1->Charger_param.Key[0], 6);
	IC_2KeyA_Cmd[13] = MT625_BCC(IC_2KeyA_Cmd, 13);
	Count = write(fd, IC_2KeyA_Cmd, sizeof(IC_2KeyA_Cmd));
	if(Count == sizeof(IC_2KeyA_Cmd)){
		Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
    if(Count == 9){
			if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
				if(Buf[6] == 0x59){
					usleep(30000);
			  	Count1= write(fd, IC_20_read_Cmd, sizeof(IC_20_read_Cmd));
				  if(Count1 == sizeof(IC_20_read_Cmd)){
		        Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
						if(Count == 26){
			        if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
							  if(Buf[7] == 0x59){
								  //*rmb = (Buf[11]<<24)+(Buf[10]<<16)+(Buf[9]<<8)+(Buf[8]);
								  rmb2 = rmb - rmb1;
									if(rmb2 < 0){
										rmb2 = 0;
									}
								//	if(MT625_DEBUG)printf("IC_Pay_Delete_Cmd ok--rmb2 = %d,rmb=%d, rmb1=%d \r\n",rmb2,rmb,rmb1);

									IC_Pay_Delete_Cmd[10] = (rmb2>>24)&0xff;
	                IC_Pay_Delete_Cmd[9] = (rmb2>>16)&0xff;
									IC_Pay_Delete_Cmd[8] = (rmb2>> 8)&0xff;
	                IC_Pay_Delete_Cmd[7] = rmb2&0xff;
									IC_Pay_Delete_Cmd[24] = MT625_BCC(IC_Pay_Delete_Cmd, 24);
								  usleep(30000);
									Count1= write(fd, IC_Pay_Delete_Cmd, sizeof(IC_Pay_Delete_Cmd));
				          if(Count1 == sizeof(IC_Pay_Delete_Cmd)){
		                Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
						        if(Count == 10){
			                if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
								        if((Buf[7] == 0x59)){//扣费成功	   
								          if(MT625_DEBUG)printf("IC_Pay_Delete_Cmd ok---扣费成功\r\n");
								          return(30);
										    }else if(Buf[7] == 0x30){//无卡
					                if(MT625_DEBUG)printf("MT625_Card_Delete fail\r\n");
											    return (31);
				                }else{//扣费失败
					                if(MT625_DEBUG)printf("MT625_Card_Delete fail 111111\r\n");
										      return (32);
										    }
				              }
	                  }
	                }
			          }
			        }
		        }
		    	}
			  }else{ 
				 if(MT625_DEBUG)printf("MT625_Card_Delete fail 222222\r\n");
		      return (33);
				}//验证扇区2 KeyA失败
      }
    }
  }
  return (0);
}	




/******************************************************************************
** Function name:       MT625_Card_Money
** Descriptions:	      M1卡 读扇区4块0存卡一些状态
** Input parameters:	
                       					
** Output parameters:	
** Returned value:		      
** Created by:		
** Created Date:		
**------------------------------------------------------------------------------
** Modified by:            
** Modified date:          
**------------------------------------------------------------------------------
*******************************************************************************/
int MT625_Card_State(int fd, unsigned char *state)
{   
  int Ret = 0, Count = 0,Count1 = 0;
	unsigned char Buf[MAX_MODBUS_FRAMESIZE] = {0};
	IC_4KeyA_Cmd[13] = MT625_BCC(IC_4KeyA_Cmd, 13);

	Count = write(fd, IC_4KeyA_Cmd, sizeof(IC_4KeyA_Cmd));
	if(Count == sizeof(IC_4KeyA_Cmd)){
		Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
    if(Count == 9){
			if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
				if(Buf[6] == 0x59){
				  IC_40_read_Cmd[8] = MT625_BCC(IC_40_read_Cmd,8);
					usleep(30000);
					Count1= write(fd, IC_40_read_Cmd, sizeof(IC_40_read_Cmd));
					if(Count1 == sizeof(IC_40_read_Cmd)){
						Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
						if(Count == 26){
							if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
								if(Buf[7] == 0x59){
									state[0] = Buf[8];//企业卡号标记
									state[1] = Buf[9];//特定电价卡号标记
									state[2] = Buf[10];//特定时间卡号标记
									state[3] = Buf[11];//是否允许特殊卡进行离线充电
								  if(MT625_DEBUG) printf("卡号标记 %x  %x %x %x\r\n",state[0],state[1],state[2],state[3]);
									return (8);
								}else{
									if(MT625_DEBUG)printf("MT625_Card_number fail\r\n");
									return (6);
								}//充电卡号读取失败
							}
						}
					}
	      }else{ 
				  if(MT625_DEBUG) printf("MT625_Card_number fail 000000 =%d \r\n",Buf[6]);
	        return (7);
				}//验证扇区0 KeyA失败
      }
    }
  }
	return (0);
}

// CPU卡 寻卡
int IC_SPG_Check(int fd)
{
	int Ret, Count;
	unsigned char Buf[MAX_MODBUS_FRAMESIZE] = {0};

	Count = write(fd, IC_SPG_Select_Cmd, sizeof(IC_SPG_Select_Cmd));
	if(Count == sizeof(IC_SPG_Select_Cmd)){
		Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
    if(Count == 10){
			if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
				if(Buf[6] == 0x30){
					if(Buf[7] == 0x30){
						return (1);//有系统卡卡
					}else{
						return (3);//有非系统卡
					}
				}else if(Buf[6] == 0x31){
					return (2);//无卡
				}
			}
		}
	}

	return (2);
}

//结论：预处理命令:
/*如果没有卡： 相应时间大概在60到70ms之间，等待时间和超时时间加起来早50和60ms时成功率高估计是因为命令响应依次延后造成的
有卡：正确处理在500ms左右；如卡放置时间太短，报告处理失败的结果要500ms以上，此时如果等待时间少于500MS就会出现无响应的现象
*/

//CPU卡 预处理
int IC_SPG_ProDeal(int fd, unsigned char *sn, unsigned int *rmb)
{
	int Ret, Count,i;
	unsigned char Buf[MAX_MODBUS_FRAMESIZE];

	time_t systime=0;
	struct tm tm_t;

	systime = time(NULL);         //获得系统的当前时间 
	localtime_r(&systime,&tm_t);  //结构指针指向当前时间	

	IC_SPG_ProDeal_Cmd[5] = int_to_bcd_1byte((tm_t.tm_year+1900)/100);
	IC_SPG_ProDeal_Cmd[6] = int_to_bcd_1byte((tm_t.tm_year+1900)%100);
	IC_SPG_ProDeal_Cmd[7] = int_to_bcd_1byte(tm_t.tm_mon+1);
	IC_SPG_ProDeal_Cmd[8] = int_to_bcd_1byte(tm_t.tm_mday);
	IC_SPG_ProDeal_Cmd[9] = int_to_bcd_1byte(tm_t.tm_hour);
	IC_SPG_ProDeal_Cmd[10] = int_to_bcd_1byte(tm_t.tm_min);	
	IC_SPG_ProDeal_Cmd[11] = int_to_bcd_1byte(tm_t.tm_sec);

	IC_SPG_ProDeal_Cmd[16] = MT625_BCC(IC_SPG_ProDeal_Cmd, sizeof(IC_SPG_ProDeal_Cmd)-1);

	Count = write(fd, IC_SPG_ProDeal_Cmd, sizeof(IC_SPG_ProDeal_Cmd));
	if(Count == sizeof(IC_SPG_ProDeal_Cmd)){
		Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
    if(Count == 28){
			if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
				if(Buf[5] == 0x59){
					if(Buf[6] == 0x01){//用户卡
						if(Buf[7] == 0x30){
							if(MT625_DEBUG)printf("IC_SPG_ProDeal 55555 card ok\r\n");
							
							bcd_to_str(sn,  &Buf[8], 8);
							*rmb = (Buf[16]<<24)+(Buf[17]<<16)+(Buf[18]<<8)+(Buf[19]);

							return (5);
						}else if(Buf[7] == 0x31){
							bcd_to_str(sn,  &Buf[8], 8);
							*rmb = (Buf[16]<<24)+(Buf[17]<<16)+(Buf[18]<<8)+(Buf[19]);
							
							if(MT625_DEBUG)printf("IC_SPG_ProDeal 666666666666666 card locked %0x\r\n", Buf[7]);//卡灰锁
							return (6);
						}else if(Buf[7] == 0x32){
							bcd_to_str(sn,  &Buf[8], 8);
							*rmb = (Buf[16]<<24)+(Buf[17]<<16)+(Buf[18]<<8)+(Buf[19]);
							
							if(MT625_DEBUG)printf("IC_SPG_ProDeal 7777777777777777 need busy  %0x\r\n", Buf[7]);//卡需补充交易
							return (7);
						}
					}else{
						bcd_to_str(sn,  &Buf[8], 8);
						*rmb = 0;
						
						if(MT625_DEBUG)printf("IC_SPG_ProDeal 444444444444444 not user card %0x\r\n", Buf[6]);
						return (4);
					}
				}else if(Buf[5] == 0x57){
					if(MT625_DEBUG)printf("IC_SPG_ProDeal 11111111111111 NO user card %0x\r\n", Buf[5]);
					return (1);
				}else if((Buf[5] == 0x23)||(Buf[5] == 0x83)){
					if(MT625_DEBUG)printf("IC_SPG_ProDeal 2222222222222222 invalid user card %0x\r\n", Buf[5]);
					return (2);
				}else if((Buf[5] == 0x47)||(Buf[5] == 0x48)||(Buf[5] == 0x4E)){
					//if(MT625_DEBUG)printf("IC_SPG_ProDeal 3333333333 process failed %0x\r\n", Buf[5]);
					return (3);
				}
			}
		}
	}

	if(MT625_DEBUG) printf("IC_SPG_ProDeal 11111111111111 NO response\r\n");

	return (1);
}

//CPU卡 开始加电
int IC_SPG_Start(int fd, unsigned int *rmb, struct tm *tm_t)
{
	int Ret, Count;
	unsigned char Buf[MAX_MODBUS_FRAMESIZE];

	IC_SPG_Start_Cmd[5] = int_to_bcd_1byte((tm_t->tm_year+1900)/100);
	IC_SPG_Start_Cmd[6] = int_to_bcd_1byte((tm_t->tm_year+1900)%100);
	IC_SPG_Start_Cmd[7] = int_to_bcd_1byte(tm_t->tm_mon+1);
	IC_SPG_Start_Cmd[8] = int_to_bcd_1byte(tm_t->tm_mday);
	IC_SPG_Start_Cmd[9] = int_to_bcd_1byte(tm_t->tm_hour);
	IC_SPG_Start_Cmd[10] = int_to_bcd_1byte(tm_t->tm_min);	
	IC_SPG_Start_Cmd[11] = int_to_bcd_1byte(tm_t->tm_sec);
	
	Globa_1->User_Card_Lock_Info.card_lock_time.year_H = ((tm_t->tm_year+1900)/100);//2016年的0x20
	Globa_1->User_Card_Lock_Info.card_lock_time.year_L = ((tm_t->tm_year+1900)%100);//2016年的0x16
	Globa_1->User_Card_Lock_Info.card_lock_time.month = tm_t->tm_mon + 1;
	Globa_1->User_Card_Lock_Info.card_lock_time.day_of_month = tm_t->tm_mday;
	Globa_1->User_Card_Lock_Info.card_lock_time.hour = tm_t->tm_hour;
	Globa_1->User_Card_Lock_Info.card_lock_time.minute = tm_t->tm_min;
	Globa_1->User_Card_Lock_Info.card_lock_time.second = tm_t->tm_sec; //为了保持锁卡时间和开始时间一致
	
	IC_SPG_Start_Cmd[13] = MT625_BCC(IC_SPG_Start_Cmd, sizeof(IC_SPG_Start_Cmd)-1);

	Count = write(fd, IC_SPG_Start_Cmd, sizeof(IC_SPG_Start_Cmd));
	if(Count == sizeof(IC_SPG_Start_Cmd)){
		Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
    if(Count == 12){
			if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
				if(Buf[5] == 0x59){
					if(MT625_DEBUG)printf("IC_SPG_Start 11111111111111 start charger ok\r\n");

					*rmb = (Buf[6]<<24)+(Buf[7]<<16)+(Buf[8]<<8)+(Buf[9]);

					return (1);
				}else if(Buf[5] == 0x4E){
					if(MT625_DEBUG)printf("IC_SPG_Start 2222222222222222 start charger failed\r\n");
					return (2);
				}else if(Buf[5] == 0x57){
					if(MT625_DEBUG)printf("IC_SPG_Start 333333333333333 no  card\r\n");
					return (3);
				}
			}
		}
	}

	return (3);
}

//CPU卡 结束加电
int IC_SPG_Stop(int fd, unsigned int rmb)
{
	int Ret, Count;
	unsigned char Buf[MAX_MODBUS_FRAMESIZE];

	time_t systime=0;
	struct tm tm_t;

	systime = time(NULL);         //获得系统的当前时间 
	localtime_r(&systime,&tm_t);  //结构指针指向当前时间	

	IC_SPG_Stop_Cmd[5] = (rmb>>24)&0xff;
	IC_SPG_Stop_Cmd[6] = (rmb>>16)&0xff;
	IC_SPG_Stop_Cmd[7] = (rmb>> 8)&0xff;
	IC_SPG_Stop_Cmd[8] =  rmb&0xff;
	
	IC_SPG_Stop_Cmd[9] = int_to_bcd_1byte((tm_t.tm_year+1900)/100);
	IC_SPG_Stop_Cmd[10] = int_to_bcd_1byte((tm_t.tm_year+1900)%100);
	IC_SPG_Stop_Cmd[11] = int_to_bcd_1byte(tm_t.tm_mon+1);
	IC_SPG_Stop_Cmd[12] = int_to_bcd_1byte(tm_t.tm_mday);
	IC_SPG_Stop_Cmd[13] = int_to_bcd_1byte(tm_t.tm_hour);
	IC_SPG_Stop_Cmd[14] = int_to_bcd_1byte(tm_t.tm_min);	
	IC_SPG_Stop_Cmd[15] = int_to_bcd_1byte(tm_t.tm_sec);

	IC_SPG_Stop_Cmd[17] = MT625_BCC(IC_SPG_Stop_Cmd, sizeof(IC_SPG_Stop_Cmd)-1);

	Count = write(fd, IC_SPG_Stop_Cmd, sizeof(IC_SPG_Stop_Cmd));
	if(Count == sizeof(IC_SPG_Stop_Cmd)){
		Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
    if(Count == 12){
			if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
				if(Buf[5] == 0x59){
					if(MT625_DEBUG)printf("IC_SPG_Stop 1111111111111111 stop charger ok\r\n");
					return (1);
				}else if(Buf[5] == 0x4E){
					if(MT625_DEBUG)printf("IC_SPG_Stop 2222222222222222 stop charger failed\r\n");
					return (2);
				}else if(Buf[5] == 0x43){
					if(MT625_DEBUG)printf("IC_SPG_Stop 3333333333333333 MAC3 NOT OK \r\n");
					return (3);
				}else if(Buf[5] == 0x57){
					if(MT625_DEBUG)printf("IC_SPG_Stop 3333333333333333 NO CARD \r\n");
					return (4);
				}
			}
		}
	}

	return (4);
}

//CPU卡 补充交易
int IC_SPG_Supp(int fd)
{
	int Ret, Count;
	unsigned char Buf[MAX_MODBUS_FRAMESIZE];

	Count = write(fd, IC_SPG_Supp_Cmd, sizeof(IC_SPG_Supp_Cmd));
	if(Count == sizeof(IC_SPG_Supp_Cmd)){
		Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
    if(Count == 50){
			if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
				if(Buf[5] == 0x59){
					if(MT625_DEBUG)printf("IC_SPG_Supp 3333333333333333 ok \r\n");
					return (1);
				}else if(Buf[5] == 0x4E){
					if(MT625_DEBUG)printf("IC_SPG_Supp 3333333333333333 NO CARD\r\n");
					return (2);
				}else if(Buf[5] == 0x57){
					if(MT625_DEBUG)printf("IC_SPG_Supp 3333333333333333 NO CARD\r\n");
					return (3);
				}
			}
		}
	}

	return (3);
}

//CPU卡 弹卡
int IC_SPG_Push(int fd)
{
	int Ret, Count;
	unsigned char Buf[MAX_MODBUS_FRAMESIZE] = {0};

	Count = write(fd, IC_SPG_Push_Cmd, sizeof(IC_SPG_Push_Cmd));
	if(Count == sizeof(IC_SPG_Push_Cmd)){
		Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
    if(Count == 10){
			if(MT625_BCC(Buf, Count-1) == Buf[Count-1]){
				if(Buf[5] == 0x30){
					return (1);
				}else if(Buf[7] == 0x31){
					return (2);
				}
			}
		}
	}

	return (3);
}

//读取锁卡时间
//填充APDU帧
int IC_SPG_MT318_Read_unlock_time(int fd)
{
	unsigned char len=0,tx_buf[100];
	int Ret, Count;
	unsigned char rx_buf[MAX_MODBUS_FRAMESIZE];
	tx_buf[0] = 0x02;//MT318_FRAME_STX;
	tx_buf[1] = 0x00;//LENGTH HIGH BYTE
	tx_buf[2] = 0x09;//LENGTH  LOW BYTE
	tx_buf[3] = 0x34;//CMD命令字
	tx_buf[4] = 0x41;//CMD命令参数字
	tx_buf[5] = 0x00;//APDU长度高字节
	tx_buf[6] = 0x05;//APDU长度低字节
	tx_buf[7] = 0xE0;//APDU
	tx_buf[8] = 0xCA;//APDU
	tx_buf[9] = 0x00;//APDU
	tx_buf[10] = 0x00;//APDU
	tx_buf[11] = 0x1E;//APDU
	tx_buf[12] = 0x03;//MT318_FRAME_ETX;
	tx_buf[13] = 0x4C;//BCC
	Count = write(fd, tx_buf, 14);
	if(Count == 14){
		Count = read_datas_tty(fd, rx_buf, MAX_MODBUS_FRAMESIZE, 1000000, 50000);
    if(Count == 42){//响应帧长度OK						
			if(rx_buf[5] == 0x59){								
		  	if(0x01 == rx_buf[8]){//卡灰锁
				  Globa_1->User_Card_Lock_Info.card_lock_time.year_H = bcd_to_int_1byte(rx_buf[23]);//2016年的0x20
					Globa_1->User_Card_Lock_Info.card_lock_time.year_L = bcd_to_int_1byte(rx_buf[24]);//2016年的0x16
					Globa_1->User_Card_Lock_Info.card_lock_time.month = bcd_to_int_1byte(rx_buf[25]);
					Globa_1->User_Card_Lock_Info.card_lock_time.day_of_month = bcd_to_int_1byte(rx_buf[26]);
					Globa_1->User_Card_Lock_Info.card_lock_time.hour = bcd_to_int_1byte(rx_buf[27]);
					Globa_1->User_Card_Lock_Info.card_lock_time.minute = bcd_to_int_1byte(rx_buf[28]);
					Globa_1->User_Card_Lock_Info.card_lock_time.second = bcd_to_int_1byte(rx_buf[29]);
					return 0;
		    }
		  }
		}
	}
	return ( -1);
}


/*********************************************************************************************************
** Function name:           IC_Task  
** Descriptions:            刷卡器通信任务	
** input parameters:        
** output parameters:          
** Returned value:           
** Created by:		    	
** Created Date:	    	
**--------------------------------------------------------------------------------------------------------
** Modified by:             	
** Modified date:           	
**--------------------------------------------------------------------------------------------------------
*********************************************************************************************************/
void IC_Task()
{
	int fd, Ret=0, Ret1=0,Ret_rmb_1=0,Ret_rmb_2=0;
	UINT32 door_open_flag=0;
  UINT32 pay_step_1 =255,pay_step_2 =255;
	CHARGER_MSG msg;

	UINT8 card_sn[16];     //用户卡号
	UINT32 temp_rmb = 0,ls_temp_rmb_1 = 0,ls_temp_rmb_2 = 0;   //
	UINT32 temp = 0,IC_com_count = 0;
  char card_state[4];
	time_t systime=0;
	struct tm tm_t;

	fd = OpenDev(IC_COMID);
	if(fd == -1) {
		while(1);
	}else{
		set_speed(fd, 115200);
		set_Parity(fd, 8, 1, 0);
		close(fd);
  }
	fd = OpenDev(IC_COMID);
	if(fd == -1) {
		while(1);
	}
	prctl(PR_SET_NAME,(unsigned long)"IC_Task");//设置线程名字 

	while(1){
		usleep(20000);
		if((Globa_1->pay_step != pay_step_1)||(Globa_2->pay_step != pay_step_2)){
			pay_step_1 = Globa_1->pay_step;
			pay_step_2 = Globa_2->pay_step;
			printf(" xxxxxxxxxxxxxxGloba_1->pay_step= %d Globa_2->pay_step= %d-----\n", Globa_1->pay_step,Globa_2->pay_step);
		}		
		// 优先级顺序 刷卡启动 ，后面三个并列（刷卡停止预约 刷卡停止充电 刷卡结算 ）
		if(Globa_1->pay_step == 1){//在等待刷卡启动界面
			Ret = MT625_Card_Search(fd); //寻卡
			if(Ret == 1){//寻卡成功
				usleep(20000);
				memset(&card_sn[0], '0', sizeof(card_sn));
			  Ret = MT625_Card_number(fd, &card_sn[0]);//读取卡号
				if(((Globa_2->pay_step == 2)||(Globa_2->pay_step == 3)||(Globa_2->pay_step == 4))&& \
    		  (0 == memcmp(&Globa_2->card_sn[0], &card_sn[0], sizeof(Globa_2->card_sn)))){
				  Globa_1->checkout = 6;//卡片已占用
           //printf("卡片已被抢2占用 7777777777777777 Globa_1->pay_step= %d,Globa_1->checkout= %d,Globa_2->pay_step= %d,Globa_2->checkout= %d,\r\n",Globa_1->pay_step,Globa_1->checkout,Globa_2->pay_step,Globa_2->checkout);
				  continue;//同一张卡不能启动同一台机器的另一把枪
    	  }
			  if((Ret == 5)||(Ret == 3)||(Ret == 4)){//5：用户卡、4：员工卡、3：管理卡 -M1卡
					if(0 == memcmp(&card_sn[0], "0000000000000000", sizeof(card_sn))){
						 continue;
					}
					if(valid_card_check(&card_sn[0]) > 0){
						 Globa_1->checkout = 9;//黑名单 
						 continue;
					}
					if(MT625_Card_State(fd, card_state) == 8){
			
					}else{
						continue;
					}
					
					if((card_sn[0] == '6')&&(card_sn[1] == '8')){//员工卡
					  if(0 == memcmp(&Globa_1->card_sn[0], "0000000000000000", sizeof(Globa_1->card_sn))){
						 usleep(20000);
						 Ret = MT625_Card_Money(fd, &temp_rmb);
					    if(Ret == 18){
							 if(card_state[3] ==  0xAA){//允许特殊卡进行离线处理 
									Globa_1->Special_card_offline_flag = 1;//允许特殊卡进行离线处理标记
								}else{
									Globa_1->Special_card_offline_flag = 0;
								}
								
								if(card_state[0] ==  0xAA){//企业号
									Globa_1->qiyehao_flag = 1;//企业号标记，
								}else{
									Globa_1->qiyehao_flag = 0;
								}
								
								if(card_state[1] ==  0xAA){//特定电价卡；
									Globa_1->private_price_flag = 1 ;//私有电价标记，
								}else{
									Globa_1->private_price_flag = 0;
								}
								
								if(card_state[2] ==  0xAA){//特定时间卡
									Globa_1->order_chg_flag = 1;//有序充电用户卡标记
								}else{
									Globa_1->order_chg_flag = 0;
								}
								
						    Globa_1->last_rmb = temp_rmb;//读取卡金额
						    Globa_1->checkout = 3;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
						  	memcpy(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn));//先保存一下卡号
								if(DEBUGLOG) CARD_RUN_LogOut("卡号：%s %s %d","1号枪刷卡之后读取卡金额 员工卡：",&Globa_1->card_sn[0],Globa_1->last_rmb);  
							}
						}
    		  }else if(!((card_sn[0] == '6')&&((card_sn[1] == '8')||(card_sn[1] == '9')))){//用户卡
					  if(0 == memcmp(&Globa_1->card_sn[0], "0000000000000000", sizeof(Globa_1->card_sn))){
							Ret = 0;
							Ret_rmb_1 = 0;
							Ret_rmb_2 = 0;
							temp_rmb = 0;
							ls_temp_rmb_1 = 0;
							ls_temp_rmb_2 = 0;
							usleep(20000);
							Ret = MT625_Card_Money(fd, &temp_rmb);
							usleep(20000);
							Ret_rmb_1 = MT625_Card_Money(fd, &ls_temp_rmb_1);
							usleep(20000);
							Ret_rmb_2 = MT625_Card_Money(fd, &ls_temp_rmb_2);
							if((Ret == 18)&&(Ret_rmb_1 == 18)&&(Ret_rmb_2 == 18)&&(ls_temp_rmb_1 == temp_rmb)&&(ls_temp_rmb_1 == ls_temp_rmb_2)){
								Globa_1->last_rmb = temp_rmb;//读取卡金额
						  	if(DEBUGLOG) CARD_RUN_LogOut("卡号：%s -1号枪刷卡之后读取卡金额-用户卡:%d",&card_sn[0],Globa_1->last_rmb);  
							  if(card_state[3] ==  0xAA){//允许特殊卡进行离线处理 
									Globa_1->Special_card_offline_flag = 1;//允许特殊卡进行离线处理标记
								}else{
									Globa_1->Special_card_offline_flag = 0;
								}
								if(card_state[0] ==  0xAA){//企业号
									Globa_1->qiyehao_flag = 1;//企业号标记，
								}else{
									Globa_1->qiyehao_flag = 0;
								}
								
								if(card_state[1] ==  0xAA){//特定电价卡；
									Globa_1->private_price_flag = 1 ;//私有电价标记，
								}else{
									Globa_1->private_price_flag = 0;
								}
								
								if(card_state[2] ==  0xAA){//特定时间卡
									Globa_1->order_chg_flag = 1;//有序充电用户卡标记
								}else{
									Globa_1->order_chg_flag = 0;
								}
		
								//if(DEBUGLOG) CARD_RUN_LogOut("卡号：%s -1号枪刷卡卡状态 %d:%d:%d",&card_sn[0],Globa_1->qiyehao_flag,Globa_1->order_chg_flag,Globa_1->private_price_flag);  
								if(Globa_1->qiyehao_flag == 0){//企业卡号不锁卡

									usleep(20000);//读卡器命令帧间隔时间需大于20MS
									Ret = MT625_Card_keysign(fd);//读卡锁卡标志，判断卡处于上锁还是解锁
									if(Ret == 12){//表明此卡处于解锁
										if(((Globa_1->last_rmb <= 50)&&(Globa_1->Charger_param.Serv_Type != 1))||(((Globa_1->last_rmb <= 50)||(Globa_1->last_rmb <= Globa_1->Charger_param.Serv_Price))&&(Globa_1->Charger_param.Serv_Type == 1))){
											memcpy(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn));//先保存一下卡号
											Globa_1->checkout = 8;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
										}else{
											Globa_1->checkout = 12;//过渡
											memcpy(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn));//先保存一下卡号
											if(Globa_1->User_Card_check_result == 1){
												usleep(20000);
												Ret = MT625_Card_Write0xAA(fd);//启动充电时写0xAA使卡上锁
												if(Ret == 24){//卡上锁成功
												//	memcpy(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn));//先保存一下卡号
													//Globa_1->checkout = 1;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
													memcpy(&Globa_1->tmp_card_sn[0], &card_sn[0], sizeof(Globa_1->tmp_card_sn));//先保存一下临时卡号
												//}
													usleep(20000);
													if(MT625_Card_keysign(fd) == 11){//读卡锁卡标志，上锁成功
														Globa_1->checkout = 1;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
													}	
												}
											}
										}
									}else if(Ret == 11){//卡灰锁
										if(!((card_sn[0] == '6')&&((card_sn[1] == '8')||(card_sn[1] == '9')))){//表明此用户卡处于灰锁
											if(0 == memcmp(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn))){//与先保存的临时卡号一致
												//memcpy(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn));//先保存一下卡号
												Globa_1->checkout = 1;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
												continue;//结束本次循环，从新开始
											}
										  Globa_1->checkout = 5;
										}
									}
							  }else{
									memcpy(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn));//先保存一下卡号
									Globa_1->checkout = 1;
								}
						  }else {
								if(DEBUGLOG) CARD_RUN_LogOut("1号枪三次读出的余额为%d :%d :%d 结果反馈%d :%d :%d",temp_rmb,ls_temp_rmb_1,ls_temp_rmb_2,Ret,Ret_rmb_1,Ret_rmb_2);  
							}
							/*else{ 
								Globa_1->checkout = 5;//置卡片异常状态
							}*/
			      }else if(0 == memcmp(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn))){
							//if(Globa_1->checkout == 12){
								if(Globa_1->qiyehao_flag == 0){//企业卡号不锁卡
									if(Globa_1->User_Card_check_result == 1){
										usleep(20000);
										Ret = MT625_Card_Write0xAA(fd);//启动充电时写0xAA使卡上锁
										if(Ret == 24){//卡上锁成功
											usleep(20000);
											if(MT625_Card_keysign(fd) == 11){//读卡锁卡标志，上锁成功
												Globa_1->checkout = 1;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
											 if(DEBUGLOG) CARD_RUN_LogOut("%s","1号枪刷卡之后读卡锁卡标志，上锁成功");  
											}										
										}
										/*else{
											Globa_1->checkout = 5;//上锁失败，刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
										}*/
									}
								}else{
									Globa_1->checkout = 1;
								}
							//}
						}
      	  }else{
      		  Globa_1->checkout = 7;//其他卡，非充电卡
    	    }
		    }else if((Ret == 6)||(Ret == 7)){//CPU卡
		      memset(&card_sn[0], '0', sizeof(card_sn));
			    Ret = IC_SPG_ProDeal(fd, &card_sn[0], &temp_rmb);
					if(0 == memcmp(&card_sn[0], "0000000000000000", sizeof(card_sn))){
						continue;
					}
					
					if((Ret == 1)||(Ret == 3)){//卡片处理失败，请重新操作
						Globa_1->checkout = 0;
					}else if(Ret == 2){//无法识别卡片
						Globa_1->checkout = 4;
					}else if((Ret == 4)||(Ret == 7)){//卡状态异常
						Globa_1->checkout = 5;
					}else if(Ret == 5){//交易处理 卡片正常
						if(((Globa_2->pay_step == 2)||(Globa_2->pay_step == 3)||(Globa_2->pay_step == 4))&& \
    		     (0 == memcmp(&Globa_2->card_sn[0], &card_sn[0], sizeof(Globa_2->card_sn)))){
				     Globa_1->checkout = 6;//卡片已占用
             //printf("卡片已被抢2占用 7777777777777777 Globa_1->pay_step= %d,Globa_1->checkout= %d,Globa_2->pay_step= %d,Globa_2->checkout= %d,\r\n",Globa_1->pay_step,Globa_1->checkout,Globa_2->pay_step,Globa_2->checkout);
				     continue;//同一张卡不能启动同一台机器的另一把枪
    	      }
						if(valid_card_check(&card_sn[0]) > 0){
						  Globa_1->checkout = 9;//黑名单 
						  continue;
					  }
						if((card_sn[0] == '6')&&(card_sn[1] == '8')){
							if(0 == memcmp(&Globa_1->card_sn[0], "0000000000000000", sizeof(Globa_1->card_sn))){
								memcpy(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn));//先保存一下卡号
								Globa_1->last_rmb = temp_rmb;
							  Globa_1->checkout = 3;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
							}
						}else if(!((card_sn[0] == '6')&&((card_sn[1] == '8')||(card_sn[1] == '9')))){
							if(0 == memcmp(&Globa_1->card_sn[0], "0000000000000000", sizeof(Globa_1->card_sn))){
								memcpy(&Globa_1->tmp_card_sn[0], &card_sn[0], sizeof(Globa_1->tmp_card_sn));//先保存一下临时卡号
								Globa_1->last_rmb = temp_rmb;
							  if(((Globa_1->last_rmb <= 50)&&(Globa_1->Charger_param.Serv_Type != 1))||(((Globa_1->last_rmb <= 50)||(Globa_1->last_rmb <= Globa_1->Charger_param.Serv_Price))&&(Globa_1->Charger_param.Serv_Type == 1))){
								  memcpy(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn));//先保存一下卡号
									Globa_1->checkout = 8;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
								}else{
									Globa_1->checkout = 12;//过渡
									memcpy(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn));//先保存一下卡号
									if(Globa_1->User_Card_check_result == 1){
										systime = time(NULL);         //获得系统的当前时间 
										localtime_r(&systime, &tm_t);  //结构指针指向当前时间
										usleep(10000);//读卡器命令帧间隔时间需大于20MS
										Ret = IC_SPG_Start(fd, &temp_rmb, &tm_t);//开始加电
										if(Ret == 1){//开始加电成功
										//	memcpy(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn));//先保存一下卡号
											Globa_1->checkout = 1;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
											if(DEBUGLOG) CARD_RUN_LogOut("%s","1号枪刷卡上锁成功");  
										}
									}
								}
							}else if(0 == memcmp(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn))){
								if(Globa_1->checkout == 12){
									if(Globa_1->User_Card_check_result == 1){
										systime = time(NULL);         //获得系统的当前时间 
										localtime_r(&systime, &tm_t);  //结构指针指向当前时间
										usleep(10000);//读卡器命令帧间隔时间需大于20MS
										Ret = IC_SPG_Start(fd, &temp_rmb, &tm_t);//开始加电
										if(Ret == 1){//开始加电成功
											//memcpy(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn));//先保存一下卡号
											Globa_1->checkout = 1;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
										}
									}
								}
							}
						}else {
							Globa_1->checkout = 7;//其他卡，
						}
					}else if(Ret == 6){//交易处理 灰锁
						if(!((card_sn[0] == '6')&&((card_sn[1] == '8')||(card_sn[1] == '9')))){
							if(0 == memcmp(&Globa_1->tmp_card_sn[0], &card_sn[0], sizeof(Globa_1->tmp_card_sn))){//与先保存的临时卡号一致
								memcpy(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn));//先保存一下卡号
								Globa_1->checkout = 1;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
								continue;//刷卡成功，结束本次循环，从新开始
							}
						}
						Globa_1->checkout = 5;//置卡片异常状态
					}
				}
			}
		/*	else if(Ret == 2){
			  Globa_1->checkout = 0;//无卡，或寻卡不成功
			}*/	
		}else if(Globa_2->pay_step == 1){//在等待刷卡启动界面
			Ret = MT625_Card_Search(fd); //寻卡
			if(Ret == 1){//寻卡成功
				usleep(20000);
				memset(&card_sn[0], '0', sizeof(card_sn));
			  Ret = MT625_Card_number(fd, &card_sn[0]);//读取卡号
				if(((Globa_1->pay_step == 2)||(Globa_1->pay_step == 3)||(Globa_1->pay_step == 4))&& \
    		  (0 == memcmp(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn)))){
				  Globa_2->checkout = 6;//卡片已占用
           //printf("卡片已被抢1占用 7777777777777777 Globa_2->pay_step= %d,Globa_12->checkout= %d,Globa_1->pay_step= %d,Globa_1->checkout= %d,\r\n",Globa_2->pay_step,Globa_2->checkout,Globa_1->pay_step,Globa_1->checkout);
				  continue;//同一张卡不能启动同一台机器的另一把枪
    	  }
			  if((Ret == 5)||(Ret == 3)||(Ret == 4)){//5：用户卡、4：员工卡、3：管理卡 -M1卡
					if(0 == memcmp(&card_sn[0], "0000000000000000", sizeof(card_sn))){
						 continue;
					}
				  if(valid_card_check(&card_sn[0]) > 0){
						Globa_2->checkout = 9;//黑名单 
					  continue;
					}
					if(MT625_Card_State(fd, card_state) == 8){
					}else{
						continue;
					}
					
					if((card_sn[0] == '6')&&(card_sn[1] == '8')){//员工卡
					  if(0 == memcmp(&Globa_2->card_sn[0], "0000000000000000", sizeof(Globa_2->card_sn))){
						 	usleep(20000);
						  Ret = MT625_Card_Money(fd, &temp_rmb);
					    if(Ret == 18){
							  if(card_state[3] ==  0xAA){//允许特殊卡进行离线处理 
									Globa_2->Special_card_offline_flag = 1;//允许特殊卡进行离线处理标记
								}else{
									Globa_2->Special_card_offline_flag = 0;
								}
								
								if(card_state[0] ==  0xAA){//企业号
									Globa_2->qiyehao_flag = 1;//企业号标记，
								}else{
									Globa_2->qiyehao_flag = 0;
								}
								
								if(card_state[1] ==  0xAA){//特定电价卡；
									Globa_2->private_price_flag = 1 ;//私有电价标记，
								}else{
									Globa_2->private_price_flag = 0;
								}
								
								if(card_state[2] ==  0xAA){//特定时间卡
									Globa_2->order_chg_flag = 1;//有序充电用户卡标记
								}else{
									Globa_2->order_chg_flag = 0;
								}
								
						    memcpy(&Globa_2->card_sn[0], &card_sn[0], sizeof(Globa_2->card_sn));//先保存一下卡号
						    Globa_2->last_rmb = temp_rmb;//读取卡金额
						    Globa_2->checkout = 3;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
						  	if(DEBUGLOG) CARD_RUN_LogOut("卡号：%s -2号枪刷卡之后读取卡金额:%d",&Globa_2->card_sn[0],Globa_2->last_rmb);  
							}
						}
    		  }else if(!((card_sn[0] == '6')&&((card_sn[1] == '8')||(card_sn[1] == '9')))){//用户卡
					  if(0 == memcmp(&Globa_2->card_sn[0], "0000000000000000", sizeof(Globa_2->card_sn))){
							Ret = 0;
							Ret_rmb_1 = 0;
							Ret_rmb_2 = 0;
							temp_rmb = 0;
							ls_temp_rmb_1 = 0;
							ls_temp_rmb_2 = 0;
							usleep(20000);
							Ret = MT625_Card_Money(fd, &temp_rmb);
							usleep(20000);
							Ret_rmb_1 = MT625_Card_Money(fd, &ls_temp_rmb_1);
							usleep(20000);
							Ret_rmb_2 = MT625_Card_Money(fd, &ls_temp_rmb_2);
							if((Ret == 18)&&(Ret_rmb_1 == 18)&&(Ret_rmb_2 == 18)&&(ls_temp_rmb_1 == temp_rmb)&&(ls_temp_rmb_1 == ls_temp_rmb_2)){
								Globa_2->last_rmb = temp_rmb;//读取卡金额
							 
							 if(card_state[3] ==  0xAA){//允许特殊卡进行离线处理 
									Globa_2->Special_card_offline_flag = 1;//允许特殊卡进行离线处理标记
								}else{
									Globa_2->Special_card_offline_flag = 0;
								}
								
								if(card_state[0] ==  0xAA){//企业号
									Globa_2->qiyehao_flag = 1;//企业号标记，
								}else{
									Globa_2->qiyehao_flag = 0;
								}
								
								if(card_state[1] ==  0xAA){//特定电价卡；
									Globa_2->private_price_flag = 1 ;//私有电价标记，
								}else{
									Globa_2->private_price_flag = 0;
								}
								
								if(card_state[2] ==  0xAA){//特定时间卡
									Globa_2->order_chg_flag = 1;//有序充电用户卡标记
								}else{
									Globa_2->order_chg_flag = 0;
								}
								
								if(Globa_2->qiyehao_flag == 0){//企业卡号不锁卡
									if(DEBUGLOG) CARD_RUN_LogOut("卡号：%s -2号枪刷卡之后读取卡金额:%d",&card_sn[0],Globa_2->last_rmb);  
									usleep(20000);//读卡器命令帧间隔时间需大于20MS
									Ret = MT625_Card_keysign(fd);//读卡锁卡标志，判断卡处于上锁还是解锁
									if(Ret == 12){//表明此卡处于解锁
										if(((Globa_2->last_rmb <= 50)&&(Globa_2->Charger_param.Serv_Type != 1))||(((Globa_2->last_rmb <= 50)||(Globa_2->last_rmb <= Globa_2->Charger_param.Serv_Price))&&(Globa_2->Charger_param.Serv_Type == 1))){
											memcpy(&Globa_2->card_sn[0], &card_sn[0], sizeof(Globa_2->card_sn));//先保存一下卡号
											Globa_2->checkout = 8;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
											if(DEBUGLOG) CARD_RUN_LogOut("%s Serv_Price=%d Serv_Type=%d ","2号枪刷卡余额不足",Globa_2->Charger_param.Serv_Price,Globa_2->Charger_param.Serv_Type);  
										}else{
											Globa_2->checkout = 12;
											memcpy(&Globa_2->card_sn[0], &card_sn[0], sizeof(Globa_2->card_sn));//先保存一下卡号
									
											if(Globa_2->User_Card_check_result == 1){
												usleep(20000);
												Ret = MT625_Card_Write0xAA(fd);//启动充电时写0xAA使卡上锁
												if(Ret == 24){//卡上锁成功
													usleep(20000);
													if(MT625_Card_keysign(fd) == 11){//读卡锁卡标志，上锁成功
														Globa_2->checkout = 1;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
														if(DEBUGLOG) CARD_RUN_LogOut("%s","2号枪刷卡上锁成功");  
													}		
													//memcpy(&Globa_2->card_sn[0], &card_sn[0], sizeof(Globa_2->card_sn));//先保存一下卡号
													memcpy(&Globa_2->tmp_card_sn[0], &card_sn[0], sizeof(Globa_2->tmp_card_sn));//先保存一下临时卡号
												//}
												}
												/*else{
													Globa_1->checkout = 5;//上锁失败，刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
												}*/
											}
								    }
									}else if(Ret == 11){//卡灰锁
										if(!((card_sn[0] == '6')&&((card_sn[1] == '8')||(card_sn[1] == '9')))){//表明此用户卡处于灰锁
											if(0 == memcmp(&Globa_2->card_sn[0], &card_sn[0], sizeof(Globa_2->card_sn))){//与先保存的临时卡号一致
												Globa_2->checkout = 1;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
												continue;//结束本次循环，从新开始
											}
										}
										Globa_2->checkout = 5;//置卡片异常状态
									}
							  }else{
									memcpy(&Globa_2->card_sn[0], &card_sn[0], sizeof(Globa_2->card_sn));//先保存一下卡号
						     // Globa_2->last_rmb = temp_rmb;//读取卡金额
									Globa_2->checkout = 1;//企业卡号不用锁操作//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
								}
					  	}else{
								if(DEBUGLOG) CARD_RUN_LogOut("2号枪三次读出的余额为%d :%d :%d 结果反馈%d :%d :%d",temp_rmb,ls_temp_rmb_1,ls_temp_rmb_2,Ret,Ret_rmb_1,Ret_rmb_2);  
							}
							/*else{ 
								Globa_1->checkout = 5;//置卡片异常状态
							}*/
			      }else if(0 == memcmp(&Globa_2->card_sn[0], &card_sn[0], sizeof(Globa_2->card_sn))){
							//if(Globa_2->checkout == 12){
								if(Globa_2->qiyehao_flag == 0){//企业卡号不锁卡
									if(Globa_2->User_Card_check_result == 1){
										usleep(20000);
										Ret = MT625_Card_Write0xAA(fd);//启动充电时写0xAA使卡上锁
										if(Ret == 24){//卡上锁成功
											usleep(20000);
											if(MT625_Card_keysign(fd) == 11){//读卡锁卡标志，上锁成功
												Globa_2->checkout = 1;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
												if(DEBUGLOG) CARD_RUN_LogOut("%s","2号枪刷卡上锁成功");  
											}		
										}
										/*else{
											Globa_1->checkout = 5;//上锁失败，刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
										}*/
									}
								}else{
									Globa_2->checkout = 1;
								}
							//}
						}
      	  }else{
      		  Globa_2->checkout = 7;//其他卡，非充电卡
    	    }
		    }else if((Ret == 6)||(Ret == 7)){//CPU卡
		      memset(&card_sn[0], '0', sizeof(card_sn));
			    Ret = IC_SPG_ProDeal(fd, &card_sn[0], &temp_rmb);
					if(0 == memcmp(&card_sn[0], "0000000000000000", sizeof(card_sn))){
						 continue;
					}
					if((Ret == 1)||(Ret == 3)){//卡片处理失败，请重新操作
						Globa_2->checkout = 0;
					}else if(Ret == 2){//无法识别卡片
						Globa_2->checkout = 4;
					}else if((Ret == 4)||(Ret == 7)){//卡状态异常
						Globa_2->checkout = 5;
					}else if(Ret == 5){//交易处理 卡片正常
						if(((Globa_1->pay_step == 2)||(Globa_1->pay_step == 3)||(Globa_1->pay_step == 4))&& \
						  (0 == memcmp(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn)))){
						   Globa_2->checkout = 6;//卡片已占用
						    //printf("卡片已被抢1占用 7777777777777777 Globa_2->pay_step= %d,Globa_12->checkout= %d,Globa_1->pay_step= %d,Globa_1->checkout= %d,\r\n",Globa_2->pay_step,Globa_2->checkout,Globa_1->pay_step,Globa_1->checkout);
						   continue;//同一张卡不能启动同一台机器的另一把枪
					  }
						if(valid_card_check(&card_sn[0]) > 0){
						  Globa_1->checkout = 9;//黑名单 
						  continue;
					  }
						if((card_sn[0] == '6')&&(card_sn[1] == '8')){
							if(0 == memcmp(&Globa_2->card_sn[0], "0000000000000000", sizeof(Globa_2->card_sn))){
								memcpy(&Globa_2->card_sn[0], &card_sn[0], sizeof(Globa_2->card_sn));//先保存一下卡号
								Globa_2->last_rmb = temp_rmb;
							  Globa_2->checkout = 3;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
							}
						}else if(!((card_sn[0] == '6')&&((card_sn[1] == '8')||(card_sn[1] == '9')))){
							if(0 == memcmp(&Globa_2->card_sn[0], "0000000000000000", sizeof(Globa_2->card_sn))){
								memcpy(&Globa_2->tmp_card_sn[0], &card_sn[0], sizeof(Globa_2->tmp_card_sn));//先保存一下临时卡号
								Globa_2->last_rmb = temp_rmb;
								if(((Globa_2->last_rmb <= 50)&&(Globa_2->Charger_param.Serv_Type != 1))||(((Globa_2->last_rmb <= 50)||(Globa_2->last_rmb <= Globa_2->Charger_param.Serv_Price))&&(Globa_2->Charger_param.Serv_Type == 1))){
								  memcpy(&Globa_2->card_sn[0], &card_sn[0], sizeof(Globa_2->card_sn));//先保存一下卡号
									Globa_2->checkout = 8;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
								}else{
									Globa_2->checkout = 12;
									memcpy(&Globa_2->card_sn[0], &card_sn[0], sizeof(Globa_2->card_sn));//先保存一下卡号
									if(Globa_2->User_Card_check_result == 1){
										systime = time(NULL);         //获得系统的当前时间 
										localtime_r(&systime, &tm_t);  //结构指针指向当前时间
										usleep(20000);//读卡器命令帧间隔时间需大于20MS
										Ret = IC_SPG_Start(fd, &temp_rmb, &tm_t);//开始加电
										if(Ret == 1){//开始加电成功
											Globa_2->checkout = 1;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
									  }
									}
								}
							}else if(0 == memcmp(&Globa_2->card_sn[0], &card_sn[0], sizeof(Globa_2->card_sn))){
								if(Globa_2->checkout == 12){
									if(Globa_2->User_Card_check_result == 1){
										systime = time(NULL);         //获得系统的当前时间 
										localtime_r(&systime, &tm_t);  //结构指针指向当前时间
										usleep(20000);//读卡器命令帧间隔时间需大于20MS
										Ret = IC_SPG_Start(fd, &temp_rmb, &tm_t);//开始加电
										if(Ret == 1){//开始加电成功
											Globa_2->checkout = 1;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
										}
									}
								}
							}
						}else {
							Globa_2->checkout = 7;//其他卡，
						}
					}else if(Ret == 6){//交易处理 灰锁
						if(!((card_sn[0] == '6')&&((card_sn[1] == '8')||(card_sn[1] == '9')))){
							if(0 == memcmp(&Globa_2->tmp_card_sn[0], &card_sn[0], sizeof(Globa_2->tmp_card_sn))){//与先保存的临时卡号一致
								memcpy(&Globa_2->card_sn[0], &card_sn[0], sizeof(Globa_2->card_sn));//先保存一下卡号
								Globa_2->checkout = 1;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
								continue;//刷卡成功，结束本次循环，从新开始
							}
						}
						Globa_2->checkout = 5;//置卡片异常状态
					}
				}
			}/*else if(Ret == 2){
			  Globa_2->checkout = 0;//无卡，或寻卡不成功
			}*/	
		}else if((Globa_1->pay_step == 2)||(Globa_1->pay_step == 3)||(Globa_1->pay_step == 4)|| \
			(Globa_2->pay_step == 2)||(Globa_2->pay_step == 3)||(Globa_2->pay_step == 4)){ //1通道或2通道需要刷卡
			Ret = MT625_Card_Search(fd);//检测刷卡
			if(Ret ==1){//寻找到卡
			  usleep(10000);
			  memset(&card_sn[0], '0', sizeof(card_sn));
			  Ret = MT625_Card_number(fd, &card_sn[0]);//读取卡号
				if((Ret == 5)||(Ret == 3)||(Ret == 4)){//5：用户卡、4：员工卡、3：管理卡
					if(((Globa_1->pay_step == 2)||(Globa_1->pay_step == 3)||(Globa_1->pay_step == 4))&& \
					  (0 == memcmp(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn)))){//与1通道启动充电卡号一致,且需刷卡
						
						if(Ret == 5){
							if(Globa_1->qiyehao_flag == 0){//企业卡号不锁卡
								usleep(20000);
								Ret = MT625_Card_keysign(fd);//读卡锁卡标志，判断卡处于上锁还是解锁
								if((Globa_1->pay_step == 2)||(Globa_1->pay_step == 4)){//1通道 预约倒计时界面 或在等待刷卡扣费界面
									usleep(20000);
									Ret = MT625_Pay_Delete(fd, Globa_1->last_rmb, Globa_1->total_rmb);//扣费
									if(DEBUGLOG) CARD_RUN_LogOut("卡号:%s 1号枪需要扣费情况: 余额:%d 消费:%d",&Globa_1->card_sn[0],Globa_1->last_rmb, Globa_1->total_rmb);  
									if(Ret == 30){//扣费成功
										usleep(20000);
										Ret = MT625_Card_Money(fd, &temp_rmb);								
										if(DEBUGLOG) CARD_RUN_LogOut("%s %d","1号枪需要扣费成功并重新读取的余额:",temp_rmb);  
										if(Ret == 18){
											int lsrmb1 = (Globa_1->last_rmb - Globa_1->total_rmb);
												 lsrmb1 = (lsrmb1<0)?0:lsrmb1; 
											if(temp_rmb == lsrmb1){//读取卡金额和扣费之后的金额一样
												usleep(20000);
												Ret = MT625_Card_Write0x55(fd);//扣费后解锁
												if(Ret == 27){
													usleep(20000);
													if(MT625_Card_keysign(fd) == 12){//读卡锁卡标志，判断卡处于上锁还是解锁
														Globa_1->checkout = 3;//解锁成功，卡正常
														if(DEBUGLOG) CARD_RUN_LogOut("%s","1号枪解锁成功，卡正常:");  
													}else{
														Globa_1->checkout = 1;
													}
												}else{
													Globa_1->checkout = 1;
												}
											}else{
												if(DEBUGLOG) CARD_RUN_LogOut("%s 理论=%d 实际=%d","1号枪扣费之后的余额和理想扣费之后的余额不一样:",lsrmb1,temp_rmb);  
											}
										}										
									}else{
										Globa_1->checkout = 1;
									}
								}else if(Globa_1->pay_step == 3){//在充电界面，刷卡停止充电
									if(Ret == 12){//卡正常
										Globa_1->checkout = 3;
									}else if(Ret == 11){//灰锁
										 Globa_1->checkout = 1;
									}
								}
							}else{
							 Globa_1->checkout = 3;
							}
						}else if(Ret == 4){//员工卡
						 Globa_1->checkout = 3;
						}
					}else if(((Globa_2->pay_step == 2)||(Globa_2->pay_step == 3)||(Globa_2->pay_step == 4))&& \
					  (0 == memcmp(&Globa_2->card_sn[0], &card_sn[0], sizeof(Globa_2->card_sn)))){//与1通道启动充电卡号一致,且需刷卡
						if(Ret == 5){
							//printf("Globa_2->qiyehao_flag = %d \r\n",Globa_2->qiyehao_flag);
							if(Globa_2->qiyehao_flag == 0){//企业卡号不锁卡
								usleep(20000);
								Ret = MT625_Card_keysign(fd);//读卡锁卡标志，判断卡处于上锁还是解锁
								//printf("Globa_2->pay_step = %d \r\n",Globa_2->pay_step);

								if((Globa_2->pay_step == 2)||(Globa_2->pay_step == 4)){//1通道 预约倒计时界面 或在等待刷卡扣费界面
									/*if(Ret == 11){//表明此卡处于上锁，未结算
										Globa_2->checkout = 1;
										 usleep(10000);//读卡器命令帧间隔时间需大于20MS
										 MT625_Card_Money(fd, &temp_rmb);
										 Globa_2->last_rmb = temp_rmb;
										if(Globa_2->total_rmb >= temp_rmb){//费用超过剩余金额，按剩余金额扣费
											 Globa_2->total_rmb = temp_rmb;
										}
										 usleep(10000);
										 Ret = MT625_Pay_Delete(fd, &temp_rmb, Globa_2->total_rmb);//扣费
										if(Ret == 30){//扣费成功
										 usleep(10000);
										 Ret = MT625_Card_Write0x55(fd);//扣费后解锁
										 if(Ret == 27)
											Globa_2->checkout = 3;//解锁成功，卡正常		
										}
									}*/
									usleep(20000);
								//	printf("MT625_Pay_Delete= %d \r\n",Globa_2->last_rmb,Globa_2->total_rmb);
									Ret = MT625_Pay_Delete(fd, Globa_2->last_rmb, Globa_2->total_rmb);//扣费
									if(DEBUGLOG) CARD_RUN_LogOut("卡号:%s 2号枪需要扣费情况: 余额:%d 消费:%d",&Globa_2->card_sn[0],Globa_2->last_rmb, Globa_2->total_rmb);  

									if(Ret == 30){//扣费成功
										usleep(20000);
										Ret = MT625_Card_Money(fd, &temp_rmb);
										if(DEBUGLOG) CARD_RUN_LogOut("%s %d","2号枪需要扣费成功并重新读取的余额:",temp_rmb);  

										if(Ret == 18){
											int lsrmb2 = (Globa_2->last_rmb - Globa_2->total_rmb);
												 lsrmb2 = (lsrmb2<0)?0:lsrmb2; 
											if(temp_rmb == lsrmb2){//读取卡金额和扣费之后的金额一样
												usleep(20000);
												Ret = MT625_Card_Write0x55(fd);//扣费后解锁
												if(Ret == 27){
													usleep(20000);
													if(MT625_Card_keysign(fd) == 12){//读卡锁卡标志，判断卡处于上锁还是解锁
														Globa_2->checkout = 3;//解锁成功，卡正常
														if(DEBUGLOG) CARD_RUN_LogOut("%s","2号枪解锁成功，卡正常:");  
													}else{
														Globa_2->checkout = 1;
													}
												}else{
													Globa_2->checkout = 1;
												}
											}else {
												if(DEBUGLOG) CARD_RUN_LogOut("%s 理论=%d 实际=%d","2号枪扣费之后的余额和理想扣费之后的余额不一样:",lsrmb2,temp_rmb);  
											}
										}	
									}else{
										Globa_2->checkout = 1;
									}
								}else if(Globa_2->pay_step == 3){//在充电界面，刷卡停止充电
									if(Ret == 12){//卡正常
										Globa_2->checkout = 3;
									}else if(Ret == 11){//灰锁
										 Globa_2->checkout = 1;
									}
								}
						  }else{
								Globa_2->checkout = 3;
							}
						}else if(Ret == 4){//员工卡
						 Globa_2->checkout = 3;
						}
					}
				}else if((Ret == 6)||(Ret == 7)){//CPU卡      
				  memset(&card_sn[0], '0', sizeof(card_sn));
				  Ret = IC_SPG_ProDeal(fd, &card_sn[0], &temp_rmb);//检测刷卡
				  if(((Globa_1->pay_step == 2)||(Globa_1->pay_step == 3)||(Globa_1->pay_step == 4))&& \
					  (0 == memcmp(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn)))){//与1通道启动充电卡号一致,且需刷卡
					  if((Globa_1->pay_step == 2)||(Globa_1->pay_step == 4)){//1通道 预约倒计时界面 或在等待刷卡扣费界面
						  if(Ret == 6){//交易处理 灰锁
						  	//Globa_1->checkout = 1;
						  	usleep(10000);//读卡器命令帧间隔时间需大于20MS
						  	if(Globa_1->total_rmb >= temp_rmb){//费用超过剩余金额，按剩余金额扣费
						  		Globa_1->total_rmb = temp_rmb;
						  	}
							  Ret = IC_SPG_Stop(fd, Globa_1->total_rmb);
									if(Ret == 1){//扣费成功
										Globa_1->checkout = 3;
									}else{
										Globa_1->checkout = 1;
									}
					  	}else if(Ret == 7){//交易处理 补充交易
							  Globa_1->checkout = 2;
							  usleep(10000);//读卡器命令帧间隔时间需大于20MS
						  	Ret = IC_SPG_Supp(fd);
									if(Ret == 1){//补充交易成功
										Globa_1->checkout = 3;
									}else{
										Globa_1->checkout = 1;
									}
								}else if(Ret == 5){//交易处理 卡片正常
									Globa_1->checkout = 3;
								}else{
									Globa_1->checkout = 1;
								}
					  }else if(Globa_1->pay_step == 3){//在充电界面
					  	if(Ret == 6){//交易处理 灰锁
						    Globa_1->checkout = 1;
					    }else if(Ret == 5){//交易处理 卡片正常
						    Globa_1->checkout = 3;
					    }
				    }
				  }else if(((Globa_2->pay_step == 2)||(Globa_2->pay_step == 3)||(Globa_2->pay_step == 4))&& \
    		    (0 == memcmp(&Globa_2->card_sn[0], &card_sn[0], sizeof(Globa_2->card_sn)))){//与2通道启动充电卡号一致,且需刷卡

				    if((Globa_2->pay_step == 2)||(Globa_2->pay_step == 4)){//2通道 预约倒计时界面 或在等待刷卡扣费界面
    		     	if(Ret == 6){//交易处理 灰锁
    			    	Globa_2->checkout = 1;

         		    usleep(10000);//读卡器命令帧间隔时间需大于20MS

								if(Globa_2->total_rmb >= temp_rmb){//费用超过剩余金额，按剩余金额扣费
									Globa_2->total_rmb = temp_rmb;
								}

								Ret = IC_SPG_Stop(fd, Globa_2->total_rmb);
								if(Ret == 1){//扣费成功
									Globa_2->checkout = 3;
								}
							}else if(Ret == 7){//交易处理 补充交易
								Globa_2->checkout = 2;

								usleep(10000);//读卡器命令帧间隔时间需大于20MS

								Ret = IC_SPG_Supp(fd);
								if(Ret == 1){//补充交易成功
									Globa_2->checkout = 3;
								}
							}else if(Ret == 5){//交易处理 卡片正常
								Globa_2->checkout = 3;
							} 
						}else if(Globa_2->pay_step == 3){//2通道 在充电界面
							if(Ret == 6){//交易处理 灰锁
								Globa_2->checkout = 1;
							}else if(Ret == 5){//交易处理 卡片正常
								Globa_2->checkout = 3;
							}
						}
					}
			  }
		  }
	  }else if(Globa_1->pay_step == 5){ //刷管理员卡进入参数设置
		  Ret = MT625_Card_Search(fd);//检测刷卡
		  if(Ret == 1){//寻找到M1卡
			  usleep(20000);
			  memset(&card_sn[0], '0', sizeof(card_sn));
			  Ret = MT625_Card_number(fd, &card_sn[0]);
			  if((Ret == 3)||(Ret == 4)||(Ret == 5)){//M1卡
					if((card_sn[0] == '6')&&(card_sn[1] == '9')){//管理卡
					   usleep(20000);
				
						if(MT625_Card_Key(fd) == 21){
						  Globa_1->checkout = 7;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡维修卡
						}
					}else{
					 Globa_1->checkout = 0;//其他卡，
					}
				}else if((Ret == 6)||(Ret == 7)){//CPU卡
			    memset(&card_sn[0], '0', sizeof(card_sn));
			    Ret = IC_SPG_ProDeal(fd, &card_sn[0], &temp_rmb);
				  if(Ret == 5){//交易处理 卡片正常
						if((card_sn[0] == '6')&&(card_sn[1] == '9')){//管理卡
						  Globa_1->checkout = 7;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡维修卡
						}else{
						 Globa_1->checkout = 0;//其他卡，
						}
					}else{
				    Globa_1->checkout = 0;
					}
				}
			}else if(Ret == 2){//无卡
				Globa_1->checkout = 0;
			}
		}else if(Globa_1->pay_step == 6){//查询卡余额
		  if(Globa_1->checkout == 0){//防止多次操作卡片
				Ret = MT625_Card_Search(fd); //M1卡处理
				//printf("查询卡余额--查询卡:Ret = %d \n",Ret);
				if(Ret == 1){//寻卡成功
					usleep(20000);
					memset(&card_sn[0], '0', sizeof(card_sn));
					Ret = MT625_Card_number(fd, &card_sn[0]);//读取卡号
				//	printf("读取卡号:Ret = %d \n",Ret);
					if((Ret == 5)||(Ret == 3)||(Ret == 4)){//5：用户卡、4：员工卡、3：管理卡
						if((card_sn[0] == '6')&&(card_sn[1] == '8')){//员工卡
							if(0 == memcmp(&Globa_1->card_sn[0], "0000000000000000", sizeof(Globa_1->card_sn))){
								Ret = MT625_Card_Money(fd, &temp_rmb);
								if(Ret == 18){
									memcpy(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn));//先保存一下卡号
									Globa_1->last_rmb = temp_rmb;//读取卡金额
									Globa_1->checkout = 3;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
								}
							}
						}else if(!((card_sn[0] == '6')&&((card_sn[1] == '8')||(card_sn[1] == '9')))){//用户卡
							if(0 == memcmp(&Globa_1->card_sn[0], "0000000000000000", sizeof(Globa_1->card_sn))){ 
								usleep(20000);
								Ret = MT625_Card_Money(fd, &temp_rmb);
								if(Ret == 18){
										memcpy(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn));//先保存一下临时卡号
									Globa_1->last_rmb = temp_rmb;//读取卡金额
									Globa_1->checkout = 1;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
								}
							}
						}else{
						 Globa_1->checkout = 7;//非充电卡，可能是维修卡
						}
					}else if((Ret == 6)||(Ret == 7)){//CPU卡
						memset(&card_sn[0], '0', sizeof(card_sn));
						Ret = IC_SPG_ProDeal(fd, &card_sn[0], &temp_rmb);
						if((Ret == 1)||(Ret == 3)){//卡片处理失败，请重新操作
							Globa_1->checkout = 0;
						}else if(Ret == 2){//无法识别卡片
							Globa_1->checkout = 4;
						}else if((Ret == 4)||(Ret == 7)){//卡状态异常
							Globa_1->checkout = 5;
						}else if((Ret == 5)||(Ret == 6)){//交易处理 卡片正常
							if((card_sn[0] == '6')&&(card_sn[1] == '8')){
								if(0 == memcmp(&Globa_1->card_sn[0], "0000000000000000", sizeof(Globa_1->card_sn))){
									memcpy(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn));//先保存一下卡号
									Globa_1->last_rmb = temp_rmb;
									Globa_1->checkout = 3;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
								}
							}else if(!((card_sn[0] == '6')&&((card_sn[1] == '8')||(card_sn[1] == '9')))){
								if(0 == memcmp(&Globa_1->card_sn[0], "0000000000000000", sizeof(Globa_1->card_sn))){
									memcpy(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn));//先保存一下卡号
									Globa_1->last_rmb = temp_rmb;
									Globa_1->checkout = 1;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡
								}
							}else{
								Globa_1->checkout = 7;//非充电卡，可能是维修卡
							}
						}
					}
				}
			}
		}else if(Globa_1->pay_step == 7){//解锁卡
		  switch(Globa_1->User_Card_Lock_Info.Unlock_card_setp){
			  case 0:{// 寻卡验证是否为用户卡机卡片锁卡情况
					Ret = MT625_Card_Search(fd); //M1卡处理
					if(Ret == 1){//寻卡成功
						usleep(20000);
						memset(&card_sn[0], '0', sizeof(card_sn));
						Ret = MT625_Card_number(fd, &card_sn[0]);//读取卡号
						if(Ret == 5){//5：用户卡、4：员工卡、3：管理卡
							if(0 == memcmp(&Globa_1->card_sn[0], "0000000000000000", sizeof(Globa_1->card_sn))){
								memcpy(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn));//先保存一下临时卡号
								usleep(20000);
								Ret = MT625_Card_keysign(fd);//读卡锁卡标志，判断卡处于上锁还是解锁
								if(Ret == 11){//表明此卡处于上锁
									usleep(20000);//读卡器命令帧间隔时间需大于20MS
									Ret = MT625_Card_Money(fd, &temp_rmb);
					        if(Ret == 18){
										Globa_1->last_rmb = temp_rmb;
										if(Globa_1->total_rmb >= temp_rmb){//费用超过剩余金额，按剩余金额扣费
											Globa_1->total_rmb = temp_rmb;
										}
										Globa_1->checkout = 1;
										Globa_1->User_Card_Lock_Info.Unlock_card_setp = 1;//等待解锁
										Globa_1->User_Card_Lock_Info.User_card_unlock_Requestfalg = 1;
									}
								}else{
									Globa_1->checkout = 3;	//表示无锁卡记录
								}
							}
						}else if((Ret == 6)||(Ret == 7)){//CPU卡
							memset(&card_sn[0], '0', sizeof(card_sn));
							Ret = IC_SPG_ProDeal(fd, &card_sn[0], &temp_rmb);
							if((Ret == 1)||(Ret == 3)){//卡片处理失败，请重新操作
								Globa_1->checkout = 0;
							}else if(Ret == 2){//无法识别卡片
								Globa_1->checkout = 4;
							}else if((Ret == 6)||(Ret == 7)){//卡状态异常
								if(0 == memcmp(&Globa_1->card_sn[0], "0000000000000000", sizeof(Globa_1->card_sn))){
									if(IC_SPG_MT318_Read_unlock_time(fd) != (-1)){
										memcpy(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn));//先保存一下卡号
										Globa_1->last_rmb = temp_rmb;
										Globa_1->checkout = 1;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异
									  Globa_1->User_Card_Lock_Info.Unlock_card_setp = 1;//等待解锁
										Globa_1->User_Card_Lock_Info.User_card_unlock_Requestfalg = 1;
									}
								}
							}else{
								Globa_1->checkout = 3;//刷卡操作结果标志    0：未知 1：灰锁 2：补充交易 3：卡片正常 4：无法识别卡片  5：卡片状态异 6：卡片已占用 7：非充电卡 8:卡号不一样
							}
						}else if((Ret == 3)||(Ret == 4)){//其他卡片都正确
							Globa_1->checkout = 3;	//表示无锁卡记录
						}else{//其他卡片都正确
							Globa_1->checkout = 4;	//表示无锁卡记录
						}
					}
					break;
		 	  }
				case 1:{//确卡号之后进行解锁 
					if(Globa_1->User_Card_Lock_Info.Start_Unlock_flag == 1){
						Ret = MT625_Card_Search(fd); //M1卡处理
					  if(Ret == 1){//寻卡成功
							usleep(20000);
							memset(&card_sn[0], '0', sizeof(card_sn));
							Ret = MT625_Card_number(fd, &card_sn[0]);//读取卡号
							if(Ret == 5){//5：用户卡、4：员工卡、3：管理卡
								if(0 == memcmp(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn))){//与1通道启动充电卡号一致,
									if(Globa_1->User_Card_Lock_Info.Deduct_money >= Globa_1->last_rmb){//费用超过剩余金额，按剩余金额扣费
										Globa_1->User_Card_Lock_Info.Deduct_money = Globa_1->last_rmb;
									}
									usleep(20000);
									Ret = MT625_Pay_Delete(fd, Globa_1->last_rmb, Globa_1->User_Card_Lock_Info.Deduct_money);//扣费
									if(Ret == 30){//扣费成功
										usleep(20000);
										Ret = MT625_Card_Money(fd, &temp_rmb);
					          if(Ret == 18){
									     int lsrmb3 = (Globa_1->last_rmb - Globa_1->User_Card_Lock_Info.Deduct_money);
									     lsrmb3 = (lsrmb3<0)?0:lsrmb3; 
								      if(temp_rmb == lsrmb3){//读取卡金额和扣费之后的金额一样
												usleep(20000);
												if(MT625_Card_Write0x55(fd) == 27){//扣费后解锁
												  usleep(20000);
													if(MT625_Card_keysign(fd) == 12){//读卡锁卡标志，判断卡处于上锁还是解锁
														Globa_1->checkout = 3;//解锁成功，卡正常		
														Globa_1->User_Card_Lock_Info.Start_Unlock_flag = 0;
														Globa_1->User_Card_Lock_Info.User_card_unlock_Data_Requestfalg = 1;//上传清单
													}
												}
									    }
										}
									}
								}else{//卡号不一样时的处理
									Globa_1->checkout = 8;
								}
							}else if((Ret == 6)||(Ret == 7)){//CPU卡
								memset(&card_sn[0], '0', sizeof(card_sn));
								Ret = IC_SPG_ProDeal(fd, &card_sn[0], &temp_rmb);
								if(0 == memcmp(&Globa_1->card_sn[0], &card_sn[0], sizeof(Globa_1->card_sn))){//与1通道启动充电卡号一致,
									usleep(20000);
									if((Ret == 6)||(Ret == 7)){//卡状态异常
										if(Globa_1->User_Card_Lock_Info.Deduct_money >= temp_rmb){//费用超过剩余金额，按剩余金额扣费
											Globa_1->User_Card_Lock_Info.Deduct_money = temp_rmb;
										}
										Ret = IC_SPG_Stop(fd, Globa_1->User_Card_Lock_Info.Deduct_money);
										if(Ret == 1){//扣费成功
										  usleep(20000);
											Globa_1->checkout = 3;
											Globa_1->User_Card_Lock_Info.Start_Unlock_flag = 0;
											Globa_1->User_Card_Lock_Info.User_card_unlock_Data_Requestfalg = 1;//上传清单
										}
									}
								}else{//卡号不一样时的处理
									Globa_1->checkout = 8;
								}
							}
						}
					}
					break;
				}
				default:{
					break;
				}
			}
		}else{//系统空闲
			usleep(500000);
		  MT625_Card_Search(fd); //寻卡
		}
		if(IC_com_Find_flag == 1){//表示刷卡器没有数据返回
			IC_com_count ++;
			if(IC_com_count> 20){
				if(Globa_1->Error.IC_connect_lost == 0){
					Globa_1->Error.IC_connect_lost = 1;
					sent_warning_message(0x99, 55, 0, 0);
				}
			}
		}else{
			IC_com_count = 0;
			if(Globa_1->Error.IC_connect_lost == 1){
				Globa_1->Error.IC_connect_lost = 0;
				sent_warning_message(0x98, 55, 0, 0);
			}
		}
  }
}  


extern void IC_Thread(void)
{
	pthread_t td1;
	int ret ,stacksize = 1024*1024;
	pthread_attr_t attr;

	ret = pthread_attr_init(&attr);
	if (ret != 0)
		return;

	ret = pthread_attr_setstacksize(&attr, stacksize);
	if(ret != 0)
		return;
	
	/* 创建自动串口抄收线程 */
	if(0 != pthread_create(&td1, &attr, (void *)IC_Task, NULL)){
		perror("####pthread_create IC_Read_Write_Task ERROR ####\n\n");
	}
	ret = pthread_attr_destroy(&attr);
	if(ret != 0)
		return;
}
