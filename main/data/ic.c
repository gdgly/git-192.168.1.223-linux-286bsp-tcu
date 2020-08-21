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
#include <sys/prctl.h>

#include "globalvar.h"
#define MT625_DEBUG 1        //调试使能

#define MAX_IC_READER_TIMEOUTCNT  10
int ic_reader_no_resp_cnt = 0;


int Find_DZ_Step = 0xFF;
int card_pwr_on_st = 0xFF;
int card_pwr_Off_st = 0xFF;
char  card_state[4];
SEND_CARD_INFO Command_Card_Info; //操作卡片命令（上电和解锁）
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

//unsigned char IC_com_Find_flag = 0;

int DES_to_min(unsigned char *sourceData, unsigned int sourceSize, unsigned char *keyStr, unsigned char *DesData)
{
	unsigned char* resultData;
	unsigned int resultSize;

	//解密
	resultData = (unsigned char* )DES_Decrypt(sourceData, sourceSize, keyStr, &resultSize);
	memcpy(DesData, resultData, sourceSize);
	free(resultData);//上面DES解密返回数据是申请的动态内存，此处必须释放

	return (resultSize);
}

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
				//IC_com_Find_flag = 0;
				if(Buf[5] == 0x59){
					//return (1);//寻找到M1或者CPU卡
					Ret = 1;
				}else if(Buf[5] == 0x4E){
					//return (2);//寻卡不成功
					Ret = 2;
				}
			}
		}
	}
	//IC_com_Find_flag = 1;
	if(0 == Ret)
	{
		ic_reader_no_resp_cnt++;
		if(ic_reader_no_resp_cnt > MAX_IC_READER_TIMEOUTCNT)
		{
			ic_reader_no_resp_cnt = 0;
			if(Globa->Control_DC_Infor_Data[0].Error.IC_connect_lost == 0)
			{
				Globa->Control_DC_Infor_Data[0].Error.IC_connect_lost = 1;							
				sent_warning_message(0x99, 55, 1, 0);//1为枪号						
				Globa->Control_DC_Infor_Data[0].Warn_system++;
			}
		}
	}
	else//ok
	{
		ic_reader_no_resp_cnt = 0;
		
		if(Globa->Control_DC_Infor_Data[0].Error.IC_connect_lost == 1)
		{
			Globa->Control_DC_Infor_Data[0].Error.IC_connect_lost = 0;							
			sent_warning_message(0x98, 55, 1, 0);//1为枪号						
			Globa->Control_DC_Infor_Data[0].Warn_system++;
		}
	}
	return (Ret);

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
	memcpy(&IC_1KeyA_Cmd[6], &Globa->Charger_param.Key[0], 6);
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
	memcpy(&IC_1KeyA_Cmd[6], &Globa->Charger_param.Key[0], 6);
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
										Globa->User_Card_Lock_Info.card_lock_time.year_H = bcd_to_int_1byte(Buf[9]);//2016年的0x20
										Globa->User_Card_Lock_Info.card_lock_time.year_L = bcd_to_int_1byte(Buf[10]);//2016年的0x16
										Globa->User_Card_Lock_Info.card_lock_time.month = bcd_to_int_1byte(Buf[11]);
										Globa->User_Card_Lock_Info.card_lock_time.day_of_month = bcd_to_int_1byte(Buf[12]);
										Globa->User_Card_Lock_Info.card_lock_time.hour = bcd_to_int_1byte(Buf[13]);
										Globa->User_Card_Lock_Info.card_lock_time.minute = bcd_to_int_1byte(Buf[14]);
										Globa->User_Card_Lock_Info.card_lock_time.second = bcd_to_int_1byte(Buf[15]);
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
	memcpy(&IC_1KeyA_Cmd[6], &Globa->Charger_param.Key[0], 6);
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
	memcpy(&IC_2KeyA_Cmd[6], &Globa->Charger_param.Key[0],6);
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
	if(Count == sizeof(IC_3KeyA_Cmd))
	{
		Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 15000);
		if(Count == 9)
		{
			if(MT625_BCC(Buf, Count-1) == Buf[Count-1])
			{
				if(Buf[6] == 0x59)
				{
					usleep(30000);
					Count1= write(fd, IC_30_read_Cmd, sizeof(IC_30_read_Cmd));
					if(Count1 == sizeof(IC_30_read_Cmd))
					{
						Count = read_datas_tty(fd, Buf, MAX_MODBUS_FRAMESIZE, 1000000, 15000);
						if(Count == 26)
						{
							if(MT625_BCC(Buf, Count-1) == Buf[Count-1])
							{
								if(Buf[7] == 0x59)
								{
									Ret1 = DES_to_min(&Buf[8], 8,  KEY, DData);
									if(0 != memcmp(&Globa->Charger_param.Key[0], &DData[0], sizeof(DData)))//卡片密钥与本机不匹配
									{
										old_Key[0] = 0xFF;
										old_Key[1] = 0xFF;
										old_Key[2] = 0xFF;
										old_Key[3] = 0xFF;
										old_Key[4] = 0xFF;
										old_Key[5] = 0xFF;
										if(0 == memcmp(&Globa->Charger_param.Key[0], &old_Key[0], sizeof(Globa->Charger_param.Key)))
										{//本机密钥当前未初始化
											memcpy(&Globa->Charger_param.Key[0], &DData[0], sizeof(DData));
											System_param_save();
											if(MT625_DEBUG) printf("MT625_Card_key System_param_save ok \r\n");
											return (21);
										}else
										{//读取代理商密钥与本机密钥不匹配
											return(22);
										}
									}else//match
									{
										if(MT625_DEBUG) printf("MT625_Card_key ok \r\n");
										return(21);//成功读取扇区3块0卡代理商密钥
									}
								}else
								{
									if(MT625_DEBUG)printf("MT625_Card_key fail \r\n");
									return(22);
								}//读取代理商密钥失败
							}
						}
					}
				}else
				{
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
	
	Globa->User_Card_Lock_Info.card_lock_time.year_H = ((tm_t.tm_year+1900)/100);//2016年的0x20
	Globa->User_Card_Lock_Info.card_lock_time.year_L = ((tm_t.tm_year+1900)%100);//2016年的0x16
	Globa->User_Card_Lock_Info.card_lock_time.month = tm_t.tm_mon + 1;
	Globa->User_Card_Lock_Info.card_lock_time.day_of_month = tm_t.tm_mday;
	Globa->User_Card_Lock_Info.card_lock_time.hour = tm_t.tm_hour;
	Globa->User_Card_Lock_Info.card_lock_time.minute = tm_t.tm_min;
	Globa->User_Card_Lock_Info.card_lock_time.second = tm_t.tm_sec; //为了保持锁卡时间和开始时间一致
	
  memcpy(&IC_1KeyA_Cmd[6], &Globa->Charger_param.Key[0], 6);
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
	
  memcpy(&IC_1KeyA_Cmd[6], &Globa->Charger_param.Key[0], 6);
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
	memcpy(&IC_2KeyA_Cmd[6], &Globa->Charger_param.Key[0], 6);
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
int IC_SPG_ProDeal(int fd, unsigned char *sn, unsigned int *rmb,unsigned int *Card_State)
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
						*Card_State = Buf[7];
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
	
	Globa->User_Card_Lock_Info.card_lock_time.year_H = ((tm_t->tm_year+1900)/100);//2016年的0x20
	Globa->User_Card_Lock_Info.card_lock_time.year_L = ((tm_t->tm_year+1900)%100);//2016年的0x16
	Globa->User_Card_Lock_Info.card_lock_time.month = tm_t->tm_mon + 1;
	Globa->User_Card_Lock_Info.card_lock_time.day_of_month = tm_t->tm_mday;
	Globa->User_Card_Lock_Info.card_lock_time.hour = tm_t->tm_hour;
	Globa->User_Card_Lock_Info.card_lock_time.minute = tm_t->tm_min;
	Globa->User_Card_Lock_Info.card_lock_time.second = tm_t->tm_sec; //为了保持锁卡时间和开始时间一致
	
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
				  Globa->User_Card_Lock_Info.card_lock_time.year_H = bcd_to_int_1byte(rx_buf[23]);//2016年的0x20
					Globa->User_Card_Lock_Info.card_lock_time.year_L = bcd_to_int_1byte(rx_buf[24]);//2016年的0x16
					Globa->User_Card_Lock_Info.card_lock_time.month = bcd_to_int_1byte(rx_buf[25]);
					Globa->User_Card_Lock_Info.card_lock_time.day_of_month = bcd_to_int_1byte(rx_buf[26]);
					Globa->User_Card_Lock_Info.card_lock_time.hour = bcd_to_int_1byte(rx_buf[27]);
					Globa->User_Card_Lock_Info.card_lock_time.minute = bcd_to_int_1byte(rx_buf[28]);
					Globa->User_Card_Lock_Info.card_lock_time.second = bcd_to_int_1byte(rx_buf[29]);
					return 0;
		    }
		  }
		}
	}
	return ( -1);
}

//查询卡初始化-----
void Init_Find_St()
{
	Find_DZ_Step = 0x01;
	memset(&Globa->MUT100_Dz_Card_Info.IC_type,0,sizeof(Globa->MUT100_Dz_Card_Info));//test
}

UINT32 IsFind_Idle_OK(void)
{
	if(0xFF == Find_DZ_Step)
		return 1;
	else
		return 0;	
}
void Init_Find_St_End()
{
	Find_DZ_Step = 0xFF;	
	//memset(&Globa->MUT100_Dz_Card_Info.IC_type,0,sizeof(Globa->MUT100_Dz_Card_Info));
}

//查询卡片
void Find_DZ_Card(int fd)
{
	int Ret = 0;
	char card_sn[16] = {"\0"};
	switch(Find_DZ_Step)
	{
		case 0xFF:{//IDLE---空闲状态
		 	usleep(20000);
			break;
		}
		case 0x01:{//进行寻卡
			Ret = MT625_Card_Search(fd);
			if(Ret == 1){//表示查询到卡片
				Find_DZ_Step  = 0x02;
				memset(&card_sn[0], '0', sizeof(card_sn));
			}
			break;
		}
		case 0x02:{//进行卡片区分，区分其为CPU还是M1卡
		  Ret = MT625_Card_number(fd, &card_sn[0]);//读取卡号(fd);
			if((Ret == 5)||(Ret == 3)||(Ret == 4)){ //M1卡
			  if((card_sn[0] == '6')&&(card_sn[1] == '8')){//员工卡
					Globa->MUT100_Dz_Card_Info.IC_type = 2;
					Globa->MUT100_Dz_Card_Info.IC_state = 0x30;	 //卡片状态----灰锁--31 或正常--30
					memcpy(&Globa->MUT100_Dz_Card_Info.IC_ID[0], &card_sn[0], sizeof(Globa->MUT100_Dz_Card_Info.IC_ID));//先保存一下临时卡号
					Globa->MUT100_Dz_Card_Info.IC_money= 0;
					Find_DZ_Step = 0x04;
					break;
				}else if(!((card_sn[0] == '6')&&((card_sn[1] == '8')||(card_sn[1] == '9')))){//用户卡//(((card_sn[0] == '6')&&(card_sn[1] != '8')&&(card_sn[1] != '9'))||((card_sn[0] == '0')&&(card_sn[1] == '0'))){//用户卡
					Globa->MUT100_Dz_Card_Info.IC_type = 1;
					Globa->MUT100_Dz_Card_Info.IC_money = 0;
					Find_DZ_Step = 0x03;
					memcpy(&Globa->MUT100_Dz_Card_Info.IC_ID[0], &card_sn[0], sizeof(Globa->MUT100_Dz_Card_Info.IC_ID));//先保存一下临时卡号
					break;
				}else if((card_sn[0] == '6')&&(card_sn[1] == '9')){//管理员卡
					Globa->MUT100_Dz_Card_Info.IC_type = 3;
					if(21 == MT625_Card_Key(fd))//管理卡密钥匹配
						Globa->MUT100_Dz_Card_Info.IC_type = 3;
					else
						Globa->MUT100_Dz_Card_Info.IC_type = 33;//test
					Globa->MUT100_Dz_Card_Info.IC_Read_OK = 1;
					memcpy(&Globa->MUT100_Dz_Card_Info.IC_ID[0], &card_sn[0], sizeof(Globa->MUT100_Dz_Card_Info.IC_ID));//先保存一下临时卡号
					Globa->MUT100_Dz_Card_Info.IC_state = 0x30;	 
					Find_DZ_Step = 0xCC;
					break;
				}else{ //非充电卡，
					Globa->MUT100_Dz_Card_Info.IC_type = 4;	 //非充电卡
					Globa->MUT100_Dz_Card_Info.IC_Read_OK = 1;
					Globa->MUT100_Dz_Card_Info.IC_money= 0;
					Globa->MUT100_Dz_Card_Info.IC_state = 0x30;	 
					Find_DZ_Step = 0xCC;
					break;
				}
			}else if((Ret == 6)||(Ret == 7)){//CPU卡
				Find_DZ_Step = 0x06;
			}
			break;
		}
		case 0x03:{//读卡状态
		  Ret = MT625_Card_number(fd, &card_sn[0]);//读取卡号(fd);
			if((Ret == 5)||(Ret == 3)||(Ret == 4)){ //M1卡
				if(0 == memcmp(&card_sn[0], &Globa->MUT100_Dz_Card_Info.IC_ID[0], sizeof(Globa->MUT100_Dz_Card_Info.IC_ID))){
					Ret = MT625_Card_keysign(fd);//读卡锁卡标志，判断卡处于上锁还是解锁
					if(Ret == 12){//正常状态
						Globa->MUT100_Dz_Card_Info.IC_state = 0x30;	 //卡片状态----灰锁--31 或正常--30
						Find_DZ_Step = 0x04;
						printf("----寻卡成功----卡片状态----正常----Find_DZ_Step ret=%d \n", Find_DZ_Step);
					}else if(Ret == 11){//锁卡状态
						Globa->MUT100_Dz_Card_Info.IC_state = 0x31;	 //卡片状态----灰锁--31 或正常--30
						Find_DZ_Step = 0x04;
						printf("----寻卡成功----卡片状态----灰锁----Find_DZ_Step ret=%d \n", Find_DZ_Step);
					}
				}else{
					Find_DZ_Step = 0x02; 
				}
			}
			break;
		}
		case 0x04:{//读金额
			Ret = MT625_Card_number(fd, &card_sn[0]);//读取卡号(fd);
			if((Ret == 5)||(Ret == 3)||(Ret == 4)){ //M1卡
				if(0 == memcmp(&card_sn[0], &Globa->MUT100_Dz_Card_Info.IC_ID[0], sizeof(Globa->MUT100_Dz_Card_Info.IC_ID))){
					Ret = 0;
					int  Ret_rmb_1 = 0;
					int  Ret_rmb_2 = 0;
					int  temp_rmb = 0;
					int  ls_temp_rmb_1 = 0;
					int  ls_temp_rmb_2 = 0;
					usleep(20000);
					Ret = MT625_Card_Money(fd, &temp_rmb);
					usleep(20000);
					Ret_rmb_1 = MT625_Card_Money(fd, &ls_temp_rmb_1);
					usleep(20000);
					Ret_rmb_2 = MT625_Card_Money(fd, &ls_temp_rmb_2);//读卡锁卡标志，判断卡处于上锁还是解锁
					if((Ret == 18)&&(Ret_rmb_1 == 18)&&(Ret_rmb_2 == 18)&&(ls_temp_rmb_1 == temp_rmb)&&(ls_temp_rmb_1 == ls_temp_rmb_2))
					{
						Globa->MUT100_Dz_Card_Info.IC_money = temp_rmb;
						//Globa->MUT100_Dz_Card_Info.IC_money = 100000;//test
						//读卡特殊标记
						usleep(20000);
						if(8 == MT625_Card_State(fd, card_state))
						{
							//Globa->MUT100_Dz_Card_Info.qiyehao_flag = 0x55;//test
							Globa->MUT100_Dz_Card_Info.qiyehao_flag       = card_state[0];//0xAA或0x55
							Globa->MUT100_Dz_Card_Info.private_price_flag = card_state[1];
							Globa->MUT100_Dz_Card_Info.order_chg_flag     = card_state[2];
							Globa->MUT100_Dz_Card_Info.offline_chg_flag   = card_state[3];
							
							printf("------qiyehao_flag      = 0x%X----\n", card_state[0]);
							printf("------private_price_flag= 0x%X----\n", card_state[1]);
							printf("------order_chg_flag    = 0x%X----\n", card_state[2]);
							printf("------offline_chg_flag  = 0x%X----\n", card_state[3]);
							Globa->MUT100_Dz_Card_Info.IC_Read_OK = 1;
							Find_DZ_Step = 0xcc; 							
						}else
						{
							
							
						}					
						
					}else{
						//if(DEBUGLOG) LogWrite(ERROR,"1号枪三次读出的余额为%d :%d :%d 结果反馈%d :%d :%d",temp_rmb,ls_temp_rmb_1,ls_temp_rmb_2,Ret,Ret_rmb_1,Ret_rmb_2);  
					}
				}else{
					Find_DZ_Step = 0x02; 
				}
			}				
			break;
		}
		case 0x06:{//cpu
		  int  temp_rmb = 0,Card_State = 0;
			Ret = IC_SPG_ProDeal(fd, &card_sn[0], &temp_rmb, &Card_State); //CPU卡预处理
			if((Ret == 5)||(Ret == 6)||(Ret == 7)){
				if((card_sn[0] == '6')&&(card_sn[1] == '8')){//员工卡
					Globa->MUT100_Dz_Card_Info.IC_type = 2;
				}else if(!((card_sn[0] == '6')&&((card_sn[1] == '8')||(card_sn[1] == '9')))){//用户卡//if(((card_sn[0] == '6')&&(card_sn[1] != '8')&&(card_sn[1] != '9'))||((card_sn[0] == '0')&&(card_sn[1] == '0'))){//用户卡
					Globa->MUT100_Dz_Card_Info.IC_type = 1;
				}else if((card_sn[0] == '6')&&(card_sn[1] == '9')){//管理员卡
					Globa->MUT100_Dz_Card_Info.IC_type = 3;
				}else{ //非充电卡，
					Globa->MUT100_Dz_Card_Info.IC_type = 4;	 //非充电卡
				}
				Globa->MUT100_Dz_Card_Info.IC_state = Card_State;	 //卡片状态----灰锁--31 或正常--30
				memcpy(&Globa->MUT100_Dz_Card_Info.IC_ID[0], &card_sn[0], sizeof(Globa->MUT100_Dz_Card_Info.IC_ID));//先保存一下临时卡号
				Globa->MUT100_Dz_Card_Info.IC_money = temp_rmb;
				Globa->MUT100_Dz_Card_Info.IC_Read_OK = 1;
				Find_DZ_Step = 0xFF;//IDLE
			}else{
				Globa->MUT100_Dz_Card_Info.IC_type = 4;	 //非充电卡
				Globa->MUT100_Dz_Card_Info.IC_Read_OK = 1;
				Find_DZ_Step = 0xFF;//IDLE
			}
			break ;
		}
		case 0xcc:{//释放互斥
			Find_DZ_Step = 0xFF;//IDLE
		}
		default:
			break;
	}
}

//初始卡上电步骤
void Init_Card_PowerOn_St(SEND_CARD_INFO *send_card_comd_info)
{
  //card_pwr_on_st = 0x01;	//直接从验证卡片密码开始	
  printf("-------初始卡上电步骤------\n");
 	memcpy(&Command_Card_Info.Card_Command, &send_card_comd_info->Card_Command, sizeof(SEND_CARD_INFO));//先保存一下临时卡号
  card_pwr_on_st = 0x01;	//直接从验证卡片密码开始	
}

//返回值1表示卡片加电OK
UINT32 IsCard_PwrON_OK(void)
{
	if(0xFF == card_pwr_on_st){
		return 1;
  }else{
		return 0;
	}
}
//返回加锁结果
UINT32 IsCard_PwrON_Result(void)
{
	return Command_Card_Info.Command_Result;
}

void Init_Power_On_End()
{
	card_pwr_on_st = 0xFF;	
  memset(&Command_Card_Info.Card_Command,0,sizeof(SEND_CARD_INFO));
}

/********/
void Card_Power_On_Card(int fd)
{	
	char card_sn[16] = {"\0"};
	int Ret =0;
	int  temp_rmb = 0;
	switch(card_pwr_on_st){			
		case 0xFF:{
			usleep(20000);
			break;
		}
   
	  case 0x01:{//进行寻卡
			Ret = MT625_Card_Search(fd);
			if(Ret == 1){//表示查询到卡片
				card_pwr_on_st  = 0x02;
				memset(&card_sn[0], '0', sizeof(card_sn));
			}
			break;
		}
		case 0x02:{//进行卡片加电
			Ret = MT625_Card_number(fd, &card_sn[0]);//读取卡号(fd);
			if((Ret == 5)||(Ret == 3)||(Ret == 4)){ //M1卡
			  if((0 == memcmp(&card_sn[0], &Command_Card_Info.IC_ID[0], sizeof(Command_Card_Info.IC_ID)))&&(Command_Card_Info.Card_Command == 1)){
					usleep(10000);
					Ret = MT625_Card_Write0xAA(fd);//启动充电时写0xAA使卡上锁
					if(Ret == 24){//卡上锁成功
						usleep(10000);
						if(MT625_Card_keysign(fd) == 11){//读卡锁卡标志，上锁成功
						// if(DEBUGLOG) LogWrite(INFO,"%s","1号枪刷卡之后读卡锁卡标志，上锁成功");  
							Command_Card_Info.Command_Result = 1;//刷卡操作结果标志 
              Command_Card_Info.Card_Command  = 0;							
							card_pwr_on_st  = 0x03;
						}		
					}
				}else{
					Command_Card_Info.Command_Result = 2;//刷卡操作结果标志 
					Command_Card_Info.Card_Command  = 0;							
					card_pwr_on_st  = 0x03;
				}
			}else if((Ret == 6)||(Ret == 7)){//CPU卡
				memset(&card_sn[0], '0', sizeof(card_sn));
				int Card_State = 0;
				Ret = IC_SPG_ProDeal(fd, &card_sn[0], &temp_rmb,&Card_State);
			  if((0 == memcmp(&card_sn[0], &Command_Card_Info.IC_ID[0], sizeof(Command_Card_Info.IC_ID)))&&(Command_Card_Info.Card_Command == 1)){
				  time_t systime=0;
					struct tm tm_t;
				  systime = time(NULL);         //获得系统的当前时间 
					localtime_r(&systime, &tm_t);  //结构指针指向当前时间
					
					if(IC_SPG_Start(fd, &temp_rmb, &tm_t) == 1){//开始加电成功
			      Command_Card_Info.Command_Result = 1;//刷卡操作结果标志 
            Command_Card_Info.Card_Command  = 0;
						card_pwr_on_st  = 0x03;
					}
				}else{
					Command_Card_Info.Command_Result = 2;//刷卡操作结果标志 
					Command_Card_Info.Card_Command  = 0;							
					card_pwr_on_st  = 0x03;
				}
			}
			break;
	  }
		case 0x03:{//M1进行加电
			card_pwr_on_st = 0xFF;//done	
			break;
		}			
		default:
			break;
	}
}



//初始卡上电步骤
void Init_Card_PowerOff_St(SEND_CARD_INFO*send_card_comd_info)
{
  card_pwr_Off_st = 0x01;	//直接从验证卡片密码开始	
 	memcpy(&Command_Card_Info.Card_Command, &send_card_comd_info->Card_Command, sizeof(SEND_CARD_INFO));//先保存一下临时卡号
}

//返回值1表示卡片加电OK
UINT32 IsCard_PwrOff_OK(void)
{
	if(0xFF == card_pwr_Off_st){
		return 1;
  }else{
		return 0;
	}
}
//返回解锁结果
UINT32 IsCard_PwrOff_Result(void)
{
	return Command_Card_Info.Command_Result;
}

void Init_Power_Off_End()
{
	card_pwr_Off_st = 0xFF;	
  memset(&Command_Card_Info.Card_Command,0,sizeof(SEND_CARD_INFO));
}


/********/
void Card_Power_Off_Card(int fd)
{	
  char card_sn[16] = {"\0"};
	int temp_rmb = 0,Card_State= 0;
	int Ret = 0;
	switch(card_pwr_Off_st){			
		case 0xFF:{
			usleep(20000);
			break;
		}
	  case 0x01:{//进行寻卡
			Ret = MT625_Card_Search(fd);
			if(Ret == 1){//表示查询到卡片
				card_pwr_Off_st  = 0x02;
				memset(&card_sn[0], '0', sizeof(card_sn));
			}
			break;
		}
		case 0x02:{//进行卡片扣费
			Ret = MT625_Card_number(fd, &card_sn[0]);//读取卡号(fd);
			if((Ret == 5)||(Ret == 3)||(Ret == 4)){ //M1卡
			  if((0 == memcmp(&card_sn[0], &Command_Card_Info.IC_ID[0], sizeof(Command_Card_Info.IC_ID)))&&(Command_Card_Info.Card_Command == 2)){
					usleep(10000);
					Ret = MT625_Pay_Delete(fd, Command_Card_Info.Last_IC_money,Command_Card_Info.Command_IC_money);//扣费
					//if(DEBUGLOG) LogWrite(INFO,"卡号:%s 1号枪需要扣费情况: 余额:%d 消费:%d",&Command_Card_Info.IC_ID[0],Command_Card_Info.Last_IC_money,Command_Card_Info.Command_IC_money);  
					if(Ret == 30){//扣费成功
						usleep(10000);
						Ret = 0;
						int Ret_rmb_1 = 0;
						int Ret_rmb_2 = 0;
						int ls_temp_rmb_1 = 0;
						int ls_temp_rmb_2 = 0;
						usleep(20000);
						Ret = MT625_Card_Money(fd, &temp_rmb);
						usleep(20000);
						Ret_rmb_1 = MT625_Card_Money(fd, &ls_temp_rmb_1);
						usleep(20000);
						Ret_rmb_2 = MT625_Card_Money(fd, &ls_temp_rmb_2);//读卡锁卡标志，判断卡处于上锁还是解锁
						if((Ret == 18)&&(Ret_rmb_1 == 18)&&(Ret_rmb_2 == 18)&&(ls_temp_rmb_1 == temp_rmb)&&(ls_temp_rmb_1 == ls_temp_rmb_2)){
							int lsrmb1 = (Command_Card_Info.Last_IC_money - Command_Card_Info.Command_IC_money);
							lsrmb1 = (lsrmb1<0)?0:lsrmb1; 
							if(temp_rmb == lsrmb1){//读取卡金额和扣费之后的金额一样
								usleep(10000);
								Ret = MT625_Card_Write0x55(fd);//扣费后解锁
								if(Ret == 27){
									usleep(10000);
									if(MT625_Card_keysign(fd) == 12){//解锁成功
									 Command_Card_Info.Command_Result = 1;//刷卡操作结果标志  
									 card_pwr_Off_st  = 0x03;
									}
								}
							}
						}
					}
				}else{ //卡号不一样
					Command_Card_Info.Command_Result = 2;//刷卡操作结果标志  
					card_pwr_Off_st  = 0x03;
				}
			}else if((Ret == 6)||(Ret == 7)){//CPU卡
				memset(&card_sn[0], '0', sizeof(card_sn));
				Ret = IC_SPG_ProDeal(fd, &card_sn[0], &temp_rmb,&Card_State);
			  if((0 == memcmp(&card_sn[0], &Command_Card_Info.IC_ID[0], sizeof(Command_Card_Info.IC_ID)))&&(Command_Card_Info.Card_Command == 2))
			  {
				if(IC_SPG_Stop(fd, Command_Card_Info.Command_IC_money) == 1)
				{
					Command_Card_Info.Command_Result = 1;//刷卡操作结果标志  
					  card_pwr_Off_st  = 0x03;
				}
			  }else{
					Command_Card_Info.Command_Result = 2;//刷卡操作结果标志  
				  card_pwr_Off_st  = 0x03;
				}
			}
			break;
	  }
		case 0x03:{//M1进行加电
			card_pwr_Off_st = 0xFF;//done	
			break;
		}			
		default:
			break;
	}
}


/*********************************************************************************************************
** Function name:           IC_Task  
** Descriptions:            刷卡器通信任务	
** input parameters:        para---串口设备名称字符串 
** output parameters:          
** Returned value:           
** Created by:		    	
** Created Date:	    	
**--------------------------------------------------------------------------------------------------------
** Modified by:             	
** Modified date:           	
**--------------------------------------------------------------------------------------------------------
*********************************************************************************************************/
void IC_Task(void* para)
{
	int fd =0;
	int old_Find_DZ_Step = 0;
	int old_card_pwr_on_st = 0;
	int old_card_pwr_Off_st = 0;

	//UINT8 card_sn[16];     //用户卡号
	//UINT32 temp_rmb = 0,ls_temp_rmb_1 = 0,ls_temp_rmb_2 = 0;   //
	//UINT32 temp = 0;
	//time_t systime=0;
	//struct tm tm_t;
	Com_run_para com_thread_para = *((Com_run_para*)para);
	
	fd = OpenDev(com_thread_para.com_dev_name);
	if(fd == -1) {
		while(1);
	}else{
		set_speed(fd, com_thread_para.com_baud);
		//set_Parity(fd, 8, 1, 0);
		set_Parity(fd, com_thread_para.com_data_bits, com_thread_para.com_stop_bits, com_thread_para.even_odd);
		close(fd);
  }
	fd = OpenDev(com_thread_para.com_dev_name);
	if(fd == -1) {
		while(1);
	}
	prctl(PR_SET_NAME,(unsigned long)"ic_task");//设置线程名字
	while(1)
	{
		usleep(20000);
		if((Find_DZ_Step != old_Find_DZ_Step)||(card_pwr_on_st != old_card_pwr_on_st)||(card_pwr_Off_st != old_card_pwr_Off_st)){
			old_Find_DZ_Step = Find_DZ_Step;
			old_card_pwr_Off_st = card_pwr_Off_st;
			old_card_pwr_on_st = card_pwr_on_st;
			printf(" xxxxxxxxxxxxx Find_DZ_Step = %0x card_pwr_on_st = %0x-- card_pwr_Off_st = %0x---\n",old_Find_DZ_Step,old_card_pwr_on_st,old_card_pwr_Off_st);
		}
		Find_DZ_Card(fd);
		Card_Power_On_Card(fd);
		Card_Power_Off_Card(fd);
  }
}  


extern void IC_Thread(void* para)
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
	if(0 != pthread_create(&td1, &attr, (void *)IC_Task, para)){
		perror("####pthread_create IC_Read_Write_Task ERROR ####\n\n");
	}
	ret = pthread_attr_destroy(&attr);
	if(ret != 0)
		return;
}
