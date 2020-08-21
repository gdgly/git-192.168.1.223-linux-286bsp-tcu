/*****************************************************************
 *Copyright:East Group Co.,Ltd
 *FileName:RemoteDataTransform.h
 *Author:sugr
 *Version:0.1
 *Date:2019-4-1
 *Description: TCU和远程数据交换
 *
 *
 *History:
 * 1、2019-4-1 version:0.1 create by sugr
 * *************************************************************/

#ifndef RemoteDataTransform_H
#define RemoteDataTransform_H

#include "cJSON.h"
#include "Data.h"

void DataRemote2Tcu(int msgid, void *remoteData, void *tcuData, int tcuSize);
void DataTcu2Remote(int msgid, void *tcuData, void *remoteData, int remoteSize);
void CcuCfgInit(T_SCcuCommCfg *cfg);
const T_SCcuCommCfg *GetCcuCfg(void);
void cJSON_PutDouble(cJSON*obj,char *key, double value);
double cJSON_GetDouble(cJSON*obj,char *key, double *value);
#endif
