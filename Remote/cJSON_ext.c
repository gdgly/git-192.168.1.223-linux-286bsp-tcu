#include "cJSON_ext.h"

const char * base64char = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char * base64_encode( const unsigned char * bindata, char * base64, int binlength )
{
    int i, j;
    unsigned char current;

    for ( i = 0, j = 0 ; i < binlength ; i += 3 )
    {
        current = (bindata[i] >> 2) ;
        current &= (unsigned char)0x3F;
        base64[j++] = base64char[(int)current];

        current = ( (unsigned char)(bindata[i] << 4 ) ) & ( (unsigned char)0x30 ) ;
        if ( i + 1 >= binlength )
        {
            base64[j++] = base64char[(int)current];
            base64[j++] = '=';
            base64[j++] = '=';
            break;
        }
        current |= ( (unsigned char)(bindata[i+1] >> 4) ) & ( (unsigned char) 0x0F );
        base64[j++] = base64char[(int)current];

        current = ( (unsigned char)(bindata[i+1] << 2) ) & ( (unsigned char)0x3C ) ;
        if ( i + 2 >= binlength )
        {
            base64[j++] = base64char[(int)current];
            base64[j++] = '=';
            break;
        }
        current |= ( (unsigned char)(bindata[i+2] >> 6) ) & ( (unsigned char) 0x03 );
        base64[j++] = base64char[(int)current];

        current = ( (unsigned char)bindata[i+2] ) & ( (unsigned char)0x3F ) ;
        base64[j++] = base64char[(int)current];
    }
    base64[j] = '\0';
    return base64;
}

int base64_decode( const char * base64, unsigned char * bindata )
{
    int i, j;
    unsigned char k;
    unsigned char temp[4];
    for ( i = 0, j = 0; base64[i] != '\0' ; i += 4 )
    {
        memset( temp, 0xFF, sizeof(temp) );
        for ( k = 0 ; k < 64 ; k ++ )
        {
            if ( base64char[k] == base64[i] )
                temp[0]= k;
        }
        for ( k = 0 ; k < 64 ; k ++ )
        {
            if ( base64char[k] == base64[i+1] )
                temp[1]= k;
        }
        for ( k = 0 ; k < 64 ; k ++ )
        {
            if ( base64char[k] == base64[i+2] )
                temp[2]= k;
        }
        for ( k = 0 ; k < 64 ; k ++ )
        {
            if ( base64char[k] == base64[i+3] )
                temp[3]= k;
        }

        bindata[j++] = ((unsigned char)(((unsigned char)(temp[0] << 2))&0xFC)) |
                ((unsigned char)((unsigned char)(temp[1]>>4)&0x03));
        if ( base64[i+2] == '=' )
            break;

        bindata[j++] = ((unsigned char)(((unsigned char)(temp[1] << 4))&0xF0)) |
                ((unsigned char)((unsigned char)(temp[2]>>2)&0x0F));
        if ( base64[i+3] == '=' )
            break;

        bindata[j++] = ((unsigned char)(((unsigned char)(temp[2] << 6))&0xF0)) |
                ((unsigned char)(temp[3]&0x3F));
    }
    return j;
}

int cJSON_GetInt(cJSON*obj,char *key,int *value)
{
    if(obj && key)
    {
        cJSON*item=cJSON_GetObjectItem(obj,key);
        if(item && item->type==cJSON_Number)
        {
            if(value)
                *value=item->valueint;
            return item->valueint;
        }
    }
    return 0;
}
int cJSON_GetShort(cJSON*obj,char *key,short *value)
{
    if(obj && key)
    {
        cJSON*item=cJSON_GetObjectItem(obj,key);
        if(item && item->type==cJSON_Number)
        {
            if(value)
                *value=item->valueint;
            return item->valueint;
        }
    }
    return -1;
}
int cJSON_GetChar(cJSON*obj,char *key,char *value)
{
    if(obj && key)
    {
        cJSON*item=cJSON_GetObjectItem(obj,key);
        if(item && item->type==cJSON_Number)
        {
            if(value)
                *value=item->valueint;
            return item->valueint;
        }
    }
    return 0;
}

int cJSON_GetString(cJSON*obj,char *key,char *value,int size)
{
    if(obj && key)
    {
        cJSON*item=cJSON_GetObjectItem(obj,key);
        if(item && item->type==cJSON_String)
        {
            int len=strlen(item->valuestring);
            if(len<size)
            {
                memcpy(value,item->valuestring,len);
                return len;
            }
            else
            {
                memcpy(value,item->valuestring,size);
                return size;
            }

        }
    }

    return 0;
}

int cJSON_GetBytes(cJSON*obj,char *key,void *value,int size)
{
    int ret=0;
    if(obj && key)
    {
        cJSON*item=cJSON_GetObjectItem(obj,key);
        if(item && item->type==cJSON_String)
        {
            int len=strlen(item->valuestring);
            char *p=calloc(1,len);
            ret=base64_decode(item->valuestring,p);
            if(ret>=size)
            {
                memcpy(value,p,size);
            }
            else
            {
                memcpy(value,p,ret);
            }
            free(p);
            return ret;
        }
    }
    return ret;
}

float cJSON_GetFloat(cJSON*obj,char *key, float *value)
{
    if(obj && key)
    {
        cJSON*item=cJSON_GetObjectItem(obj,key);
        if(item && item->type==cJSON_Number)
        {
            if(value)
                *value=item->valuedouble;
            return item->valuedouble;
        }
    }
    return 0;
}

double cJSON_GetDouble(cJSON*obj,char *key, double *value)
{
    if(obj && key)
    {
        cJSON*item=cJSON_GetObjectItem(obj,key);
        if(item && item->type==cJSON_Number)
        {
            if(value)
                *value=item->valuedouble;
            return item->valuedouble;
        }
    }
    return 0;
}

void cJSON_PutInt(cJSON*obj,char *key,int value)
{
    if(obj && key)
    {
        cJSON_AddItemToObject(obj,key,cJSON_CreateNumber(value));
    }
}
void cJSON_PutShort(cJSON*obj,char *key,short value)
{
    if(obj && key)
    {
        cJSON_AddItemToObject(obj,key,cJSON_CreateNumber(value));
    }
}
void cJSON_PutChar(cJSON*obj,char *key,unsigned char value)
{
    if(obj && key)
    {
        cJSON_AddItemToObject(obj,key,cJSON_CreateNumber(value));
    }
}

void cJSON_PutString(cJSON*obj,char *key,char *value)
{
    if(obj && key)
    {
       cJSON_AddItemToObject(obj,key,cJSON_CreateString(value));
    }
}

void cJSON_PutBytes(cJSON*obj,char *key,char *value,int size)
{
    char buf[50]={0};
    memcpy(buf,value,size);
    if(obj && key)
    {
        char *p=calloc(1,size*2);
        base64_encode(value,p,size);
        cJSON_AddItemToObject(obj,key,cJSON_CreateString(p));
        free(p);
    }
}

void cJSON_PutDouble(cJSON*obj,char *key, double value)
{
    if(obj && key)
    {
        cJSON_AddItemToObject(obj,key,cJSON_CreateNumber(value));
    }
}

void cJSON_PutFloat(cJSON*obj,char *key, float value)
{
    if(obj && key)
    {
        cJSON_AddItemToObject(obj,key,cJSON_CreateNumber(value));
    }
}

