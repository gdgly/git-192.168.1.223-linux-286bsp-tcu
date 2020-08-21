/*******************************************************************************
  Copyright(C)    2017-2017,      EAST Co.Ltd.
  File name :     Task_Server_Comm.h
  Author :        
  Version:        1.0
  Date:           2018.4.8
  Description:    
  Other:          
  Function List:  
  History:        
          <author>   <time>   <version>   <desc>
                    2018.4.8   1.0      Create
*******************************************************************************/
#ifndef  __TASK_SERVER_COMM_H__
#define  __TASK_SERVER_COMM_H__


//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C" {
#endif

void remote2tcu_MsgParse(char *data,int size);
void RemoteInit();

#ifdef __cplusplus
    }
#endif

#endif //__TASK_SERVER_COMM_H__
