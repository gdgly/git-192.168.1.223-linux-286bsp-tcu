#ifndef CJSON_EXT_H
#define CJSON_EXT_H

#include "cJSON.h"
#include "mongoose.h"
int cJSON_GetInt(cJSON*obj,char *key,int *value);
int cJSON_GetShort(cJSON*obj,char *key,short *value);
int cJSON_GetChar(cJSON*obj,char *key,char *value);
int cJSON_GetString(cJSON*obj,char *key,char *value,int size);
int cJSON_GetBytes(cJSON*obj, char *key, void *value, int size);
void cJSON_PutInt(cJSON*obj,char *key,int value);
void cJSON_PutShort(cJSON*obj,char *key,short value);
void cJSON_PutBytes(cJSON*obj,char *key,char *value,int size);
void cJSON_PutString(cJSON*obj,char *key,char *value);
void cJSON_PutChar(cJSON*obj,char *key,unsigned char value);
float cJSON_GetFloat(cJSON*obj,char *key, float *value);
double cJSON_GetDouble(cJSON*obj,char *key, double *value);
void cJSON_PutFloat(cJSON*obj,char *key,float value);
void cJSON_PutDouble(cJSON*obj,char *key, double value);
#endif
