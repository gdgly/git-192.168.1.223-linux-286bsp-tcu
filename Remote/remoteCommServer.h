#ifndef REMOTECOMMSERVER_H
#define REMOTECOMMSERVER_H
#include "mongoose.h"
void SendRemoteData(void *nc,char *data,int len);
void SendHmiData(void *nc,char *data,int len);

void *DriverRun(void *arg);
#endif
