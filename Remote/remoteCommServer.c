#include "remoteCommServer.h"
#include "remote2tcu.h"
#include "tcu2remote.h"
#include "RemoteComm.h"
#include "kernel_list.h"
#include "cJSON_ext.h"
#include "api.h"

//以下内容为测试时显示的设备简要信息
char Info[] = {
    "后台通信处理模块    \n"
    "版本："APP_GIT_VERSION"\n"
    "项目地址："APP_GIT_PATH"\n"
    "最后提交时间：       "APP_GIT_COMMIT_TIME"\n"
    "程序设计者:       陶粤兵\n"
    "开发日期： 2019.04.17      \n"
    "本程序最后编译时间：	                             \n"    // 请勿在此后增加信息!
    "                             \n"    // 请保留此行(分配内存用)
};

/*****************************************************************/
// 函数名称：DLLInfo();
// 功能描述：动态库版本中将信息包 Info 输出，以作版本信息等标志。
// 输入参数：Info--版本信息数组
// 输出参数：
// 返回：    版本信息数组
// 其他：
/*****************************************************************/
char* DLLInfo( )
{
    int nStrLen = strlen( Info );
    char buf[30]={0};
    sprintf(buf,"%s %s\n",__DATE__,__TIME__);
    int len=strlen(buf);
    sprintf( Info+nStrLen-len-1, "%s ",buf);

    return Info;
}
typedef struct
{
    char topic[100];
    char *data;
    int size;
} SmqttMsg;


typedef struct RemoteAckNode
{
    struct list_head glist;
    int count;
    pthread_mutex_t lock;
    SCommEventData *data;
}RemoteAckNode;
static RemoteAckNode *ackList=NULL;

static int mqttConnectState=-1;
static struct mg_mqtt_topic_expression s_topic_xpr = {NULL, 2};

#define TOPIC_REMOTE "topic_remote"
#define TOPIC_TCU "topic_tcu"

static void publishHeart(struct mg_connection *nc)
{
    char buf[1024]={0};
    if (mqttConnectState > 0)
    {
        cJSON *obj=cJSON_CreateObject();
        cJSON_PutInt(obj,"id",TCU2REMOTE_HEART_BEAT);

        char *p=cJSON_Print(obj);
        snprintf(buf,sizeof(buf),"%s",p);
        int size=strlen(p);
        mg_mqtt_publish(nc, TOPIC_TCU, 65, MG_MQTT_QOS(0), buf, size);
        free(p);
        cJSON_Delete(obj);
    }
}
void AppendMsgToList(char *data,int len)
{

}

void SendRemoteData(void *nc,char *data,int len)
{
    if (mqttConnectState > 0)
    {
        mg_mqtt_publish(nc, TOPIC_TCU, ackList->count, MG_MQTT_QOS(0), data,
                        len);
    }
}

static void report_handler(struct mg_connection *nc, int ev, void *p)
{
    struct mg_mqtt_message *msg = (struct mg_mqtt_message *)p;
    (void *)nc;

    switch (ev)
    {
        case MG_EV_CONNECT:
        {
            struct mg_send_mqtt_handshake_opts opts;
            memset(&opts, 0, sizeof(opts));
            opts.user_name = "";
            opts.password = "";
            opts.keep_alive=60;
            mg_set_protocol_mqtt(nc);
            char name[50]={0};
            snprintf(name,sizeof(name),"tcu-%s", GetSettings2("product", "000000"));
            mg_send_mqtt_handshake_opt(nc,name, opts);
            break;
        }

        case MG_EV_MQTT_CONNACK:
        {
            if (msg->connack_ret_code != MG_EV_MQTT_CONNACK_ACCEPTED)
            {
                //error
                AppLogOut("mqtt connect error=%d", msg->connack_ret_code);
                break;
            }

            s_topic_xpr.topic = TOPIC_REMOTE;
            mg_mqtt_subscribe(nc, &s_topic_xpr, 1, 43);
            AppLogOut("tcu mqtt client subscribe!");
            mqttConnectState = 1;
            break;
        }
        
        case MG_EV_MQTT_PUBACK:
        {
            break;
        }
        case MG_EV_MQTT_SUBACK:
        {
            break;
        }
        case MG_EV_TIMER:
        {
            double next = mg_time() + 10;
            mg_set_timer(nc, next);
       //     publishHeart(nc);
            break;
        }
        case MG_EV_MQTT_PUBLISH:
        {
            if (msg->payload.len > 0)
            {
                // printf("Got incoming message %.*s: %.*s\n", (int) msg->topic.len,
                //             msg->topic.p, (int) msg->payload.len, msg->payload.p);
                if (mg_vcmp(&msg->topic, TOPIC_REMOTE) == 0)
                {
                    remote2tcu_MsgParse((char *)msg->payload.p,msg->payload.len);
                }
            }
            //AppLogOut("%.*s",msg->payload.len,msg->payload);
            break;
        }
        case MG_EV_MQTT_DISCONNECT:
        {
            mqttConnectState = -1;
            AppLogOut("tcu mqtt client disconnect!");
            break;
        }
        case MG_EV_CLOSE:
        {
            mqttConnectState = -1;
            AppLogOut("tcu mqtt client disconnect!");
            break;
        }
    }

}

static void RemoteNodeDeal(struct mg_connection *handle)
{
    /*
    pthread_mutex_lock(&ackList->lock);
    RemoteAckNode *e = list_entry((&(ackList->glist))->next, typeof(*e), glist);
    if(&e->glist!=&ackList->glist)
    {
        list_del(&e->glist);
        ackList->count--;
    }
    else
    {
        e=NULL;
    }

    if(e!=NULL)
    {
        SCommEventData *data=e->data;
        Tcu2Remote_EventFilter(handle,data);
        free(data);
        free(e);
    }
    pthread_mutex_unlock(&ackList->lock);
    */
   SCommEventData data;
   if(1 == CommFetchEvent(&data))
   {
       Tcu2Remote_EventFilter(handle,&data);
   }
}

/*********************************************************
**description：后台通信协议处理
***parameter  :none
***return	  :none
**********************************************************/
void *Server_Handle_Thread(void*arg)
{
    setThreadName("Remote2Tcu_Thread");
    char port[50] = {0};

    struct mg_mgr mgr;
    mg_mgr_init(&mgr, NULL);
    struct mg_connection *handle = NULL;
    SaveSettingi("initFlag",1);
    while (1)
    {

        //断线重连处理
        if (mqttConnectState < 0)
        {
            AppLogOut("connect mqtt server!");

            //snprintf(port, sizeof(port), "%s:%s", "192.168.162.82", "1883");
            snprintf(port, sizeof(port), "%s:%s", "127.0.0.1", "1883");

            handle = mg_connect(&mgr, port, report_handler);
            
            if (handle)
            {
                mg_set_timer(handle, mg_time() + 10);
                mqttConnectState = 0;
            }
            sleep(1);
        }

        mg_mgr_poll(&mgr, 10);

        RemoteNodeDeal(handle);
    }

    mg_mgr_free(&mgr);
}
void *DriverRun(void *arg)
{

    pthread_t td1;
    int ret ,stacksize = 1024*1024;
    pthread_attr_t attr;

    ret = pthread_attr_init(&attr);
    if (ret != 0)
        return NULL;

    ret = pthread_attr_setstacksize(&attr, stacksize);
    if(ret != 0)
        return NULL;


    if(0 != pthread_create(&td1, &attr, Server_Handle_Thread, NULL)){
        perror("####Server_Handle_Thread####\n\n");
    }


    ret = pthread_attr_destroy(&attr);
    if(ret != 0)
        return NULL;
}

void *DriverInit(void *arg)
{
    ackList=calloc(1,sizeof(RemoteAckNode));
    ackList->count=0;
    INIT_LIST_HEAD(&ackList->glist);
    pthread_mutex_init(&ackList->lock,NULL);
}

void *DriverStop(void *arg)
{

}

void * EventFilter(int devId,int eventType,void *data,int dataSize)
{

    if(devId==REMOTE_DEV_ID)
    {
        pthread_mutex_lock(&ackList->lock);
        RemoteAckNode* node=(RemoteAckNode*)calloc(1,sizeof(RemoteAckNode));
        if(node)
        {
            node->data = (SCommEventData *)malloc(dataSize);
            memcpy(node->data,data,dataSize);
            list_add_tail(&node->glist,&ackList->glist);
            ackList->count++;
        }

        pthread_mutex_unlock(&ackList->lock);
    }
}
