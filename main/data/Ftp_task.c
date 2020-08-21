/********************************
                 FTP 协议
相比其他协议，如 HTTP 协议，FTP 协议要复杂一些。与一般的 C/S 应用不同点在于一般的C/S 应用程序一般只会建立一�?Socket 连接，这个连接同时处理服务器端和客户端的连接命令和数据传输。而FTP协议中将命令与数据分开传送的方法提高了效率�?FTP 使用 2 个端口，一个数据端口和一个命令端口（也叫做控制端口）。这两个端口一般是21 （命令端口）�?20 （数据端口）。控�?Socket 用来传送命令，数据 Socket 是用于传送数据。每一�?FTP 命令发送之后，FTP 服务器都会返回一个字符串，其中包括一个响应代码和一些说明信息。其中的返回码主要是用于判断命令是否被成功执行了�?命令端口
一般来说，客户端有一�?Socket 用来连接 FTP 服务器的相关端口，它负责 FTP 命令的发送和接收返回的响应信息。一些操作如“登录”、“改变目录”、“删除文件”，依靠这个连接发送命令就可完成�?数据端口
对于有数据传输的操作，主要是显示目录列表，上传、下载文件，我们需要依靠另一�?Socket来完成�?如果使用被动模式，通常服务器端会返回一个端口号。客户端需要用另开一�?Socket 来连接这个端口，然后我们可根据操作来发送命令，数据会通过新开的一个端口传输�?如果使用主动模式，通常客户端会发送一个端口号给服务器端，并在这个端口监听。服务器需要连接到客户端开启的这个数据端口，并进行数据的传输�?下面�?FTP 的主动模式和被动模式做一个简单的介绍�?主动模式 (PORT)
主动模式下，客户端随机打开一个大�?1024 的端口向服务器的命令端口 P，即 21 端口，发起连接，同时开放N +1 端口监听，并向服务器发出 “port N+1�?命令，由服务器从它自己的数据端口 (20) 主动连接到客户端指定的数据端�?(N+1)�?FTP 的客户端只是告诉服务器自己的端口号，让服务器来连接客户端指定的端口。对于客户端的防火墙来说，这是从外部到内部的连接，可能会被阻塞�?被动模式 (PASV)
为了解决服务器发起到客户的连接问题，有了另一�?FTP 连接方式，即被动方式。命令连接和数据连接都由客户端发起，这样就解决了从服务器到客户端的数据端口的连接被防火墙过滤的问题�?被动模式下，当开启一�?FTP 连接时，客户端打开两个任意的本地端�?(N > 1024 �?N+1) �?第一个端口连接服务器�?21 端口，提�?PASV 命令。然后，服务器会开启一个任意的端口 (P > 1024 )，返回如�?27 entering passive mode (127,0,0,1,4,18)”�?它返回了 227 开头的信息，在括号中有以逗号隔开的六个数字，前四个指服务器的地址，最后两个，将倒数第二个乘 256 再加上最后一个数字，这就�?FTP 服务器开放的用来进行数据传输的端口。如得到 227 entering passive mode (h1,h2,h3,h4,p1,p2)，那么端口号�?p1*256+p2，ip 地址为h1.h2.h3.h4。这意味着在服务器上有一个端口被开放。客户端收到命令取得端口号之�? 会通过 N+1 号端口连接服务器的端�?P，然后在两个端口之间进行数据传输�?这是ftp协议的基本了解�?*******************************/
/*****************************************
主要用到�?FTP 命令
USER: 指定用户名。通常是控制连接后第一个发出的命令。“USER gaoleyi\r\n”： 用户名为gaoleyi 登录�?PASS: 指定用户密码。该命令紧跟 USER 命令后。“PASS gaoleyi\r\n”：密码�?gaoleyi�?CWD: 改变工作目录。如：“CWD dirname\r\n”�?PASV: 让服务器在数据端口监听，进入被动模式。如：“PASV\r\n”�?PORT: 告诉 FTP 服务器客户端监听的端口号，让 FTP 服务器采用主动模式连接客户端。如：“PORT h1,h2,h3,h4,p1,p2”�?RETR: 下载文件。“RETR file.txt \r\n”：下载文件 file.txt�?STOR: 上传文件。“STOR file.txt\r\n”：上传文件 file.txt�?QUIT: 关闭与服务器的连接�?******************************************/
/*************************
FTP 响应�?客户端发�?FTP 命令后，服务器返回响应码�?响应码用三位数字编码表示�?第一个数字给出了命令状态的一般性指示，比如响应成功、失败或不完整�?第二个数字是响应类型的分类，�?2 代表跟连接有关的响应�? 代表用户认证�?第三个数字提供了更加详细的信息�?第一个数字的含义如下�?1 表示服务器正确接收信息，还未处理�?2 表示服务器已经正确处理信息�?3 表示服务器正确接收信息，正在处理�?4 表示信息暂时错误�?5 表示信息永久错误�?第二个数字的含义如下�?0 表示语法�?1 表示系统状态和信息�?2 表示连接状态�?3 表示与用户认证有关的信息�?4 表示未定义�?5 表示与文件系统有关的信息�?********************************************************/
/*******************************************************
 Socket 编程的几个重要步�?Socket 客户端编程主要步骤如下：
1.socket() 创建一�?Socket
2.connect() 与服务器连接
3.write() �?read() 进行会话
4.close() 关闭 Socket
********************************************************/
/*************************编辑本段协议结构**************************
   命令                                     描述
ABOR                               中断数据连接程序
ACCT <account>                     系统特权帐号
ALLO <bytes>                       为服务器上的文件存储器分配字�?APPE <filename>                    添加文件到服务器同名文件
CDUP <dir path>                    改变服务器上的父目录
CWD <dir path>                     改变服务器上的工作目�?DELE <filename>                    删除服务器上的指定文�?HELP <command>                     返回指定命令信息
LIST <name>                        如果是文件名列出文件信息，如果是目录则列出文件列�?MODE <mode>                        传输模式（S=流模式，B=块模式，C=压缩模式�?MKD <directory>                    在服务器上建立指定目�?NLST <directory>                   列出指定目录内容
NOOP                               无动作，除了来自服务器上的承�?PASS <password>                    系统登录密码
PASV                               请求服务器等待数据连�?PORT <address>IP                   地址和两字节的端�?ID
PWD                                显示当前工作目录
QUIT�?FTP                         服务器上退出登�?REIN                               重新初始化登录状态连�?REST <offset>                      由特定偏移量重启文件传�?RETR <filename>                    从服务器上找回（复制）文�?RMD <directory>                    在服务器上删除指定目�?RNFR <old path>                    对旧路径重命�?RNTO <new path>                    对新路径重命�?SITE <params>                      由服务器提供的站点特殊参�?SMNT <pathname>                    挂载指定文件结构
STAT <directory>                   在当前程序或目录上返回信�?STOR <filename>                    储存（复制）文件到服务器�?STOU <filename>                    储存文件到服务器名称�?STRU <type>                        数据结构（F=文件，R=记录，P=页面�?SYST                               返回服务器使用的操作系统
TYPE <data type>                   数据类型（A=ASCII，E=EBCDIC，I=binary�?USER <username>>                   系统登录的用户名

标准 FTP 信息如下
响应代码                         解释说明
110                         新文件指示器上的重启标记
120                         服务器准备就绪的时间（分钟数�?125                         打开数据连接，开始传�?150                         打开连接
200                         成功
202                         命令没有执行
211                         系统状态回�?212                         目录状态回�?213                         文件状态回�?214                         帮助信息回复
215                         系统类型回复
220                         服务就绪
221                         退出网�?225                         打开数据连接
226                         结束数据连接
227                         进入被动模式（IP 地址、ID 端口�?230                         登录因特�?250                         文件行为完成
257                         路径名建�?331                         要求密码
332                         要求帐号
350                         文件行为暂停
421                         服务关闭
425                         无法打开数据连接
426                         结束连接
450                         文件不可�?451                         遇到本地错误
452                         磁盘空间不足
500                         无效命令
501                         错误参数
502                         命令没有执行
503                         错误指令序列
504                         无效命令参数
530                         未登录网�?532                         存储文件需要帐�?550                         文件不可�?551                         不知道的页类�?552                         超过存储分配
553                         文件名不允许
**********************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>

#define GETDIAGNOSTICSFTPUSER               "USER cims\r\n"
#define GETDIAGNOSTICSFTPPASS               "PASS cims@123\r\n"
#define GETDIAGNOSTICSFTPPATH               "CWD /resin-pro-4.0.41/webapps/cims/WebClient/ftp/diagnostics\r\n"

#define GETDIAGNOSTICSFTPCWD1               "CWD resin-pro-4.0.41/webapps/cims/WebClient/ftp/updatefirmware/ST/Nucleo-f207zg\r\n"
#define GETDIAGNOSTICSFTPCWD                "CWD resin-pro-4.0.41/webapps/cims/WebClient/ftp/diagnostics/CP3\r\n"
#define GETDIAGNOSTICSFTPPWD                "PWD \r\n"
#define GETDIAGNOSTICSFTPPASV               "PASV \r\n"
#define GETDIAGNOSTICSFTP                   "TYPE A\r\n"
#define CONSTANT                            "256"
//#define GETDIAGNOSTICSUPLOAD                "STOR Diagnostics_Log.tar.gz\r\n"  //上传命令/mnt/Nand1/ea_app/data/busy_coplete.db
#define GETDIAGNOSTICSFTPQUIT               "QUIT \r\n"

#define DEFAULT_UPGRADE_FILENAME            "DC_Charger.tar"   //RETR --- 下载命令
#define UPLOAD                              "/mnt/Nand1/update/Diagnostics_Log.tar.gz"  //上传命令
#define GETDIAGNOSTICSUPLOAD                "Diagnostics_Log.tar.gz"  //上传命令/mnt/Nand1/ea_app/data/busy_coplete.db

//#define FTP_CMD_PORT    21
//#define FTP_DATA_PORT   22
#include "md5.h"
#include "Ftp_task.h"

#include "globalvar.h"
#include "common.h"

typedef	struct
{	
	unsigned char charger_SN[16];         //充电桩编�?字符�?	unsigned char VV;//�?xAA有效，表示后续数据有�?	unsigned char ftp_ip[4];//�?xc0 02 03 04表示192.2.3.4
	unsigned char ftp_port[1];//
	unsigned char ftp_user[10];//用户名，字符�?x00结束
	unsigned char ftp_password[10];//密码，字符串0x00结束
	unsigned char ftp_path[50];//字符�?x00结束
	unsigned char ftp_filename[50];//字符�?x00结束
	unsigned char factory_code[10];//字符�?x00结束
	unsigned char hw_ver[30];//字符�?x00结束
	unsigned char sw_ver[20];//字符�?x00结束
}FTP_UPGRADE_INFO;//服务器下发远程升级启�?
FTP_UPGRADE_INFO  g_ftp_upgrade_info;

typedef	struct _FTP_DATA
{ 
	unsigned char Ftp_Server_IP[32]; 
	unsigned char User[20]; 
	unsigned char Password[20]; 
	unsigned char down_file_name[50];//下载的文件名
	unsigned char path[60];
	unsigned int  port;

	unsigned char retries;    //下载尝试次数
	unsigned char retryInterval;//重试间隔时间

	unsigned char ftp_need_down ;//需要下�?	unsigned char ftp_down_Setp;//ftp下载步骤	
	unsigned char FirmwareStatus;//ftp下载状�?}FTP_UPGRADE_DATA;

typedef	struct _FTP_DATA_UP
{ 
	unsigned char Ftp_Server_IP[32]; 
	unsigned char User[20]; 
	unsigned char Password[20]; 
	unsigned char path[60];
	unsigned int  port;
	unsigned char retries;    //下载尝试次数
	unsigned char retryInterval;//重试间隔时间
	unsigned int  startTime_Year; 
	unsigned char startTime_Month;
	unsigned char startTime_Date;
	unsigned char startTime_Hours;
	unsigned char startTime_Minutes;
	unsigned char startTime_Seconds;
	unsigned int  stopTime_Year; 
	unsigned char stopTime_Month;
	unsigned char stopTime_Date;
	unsigned char stopTime_Hours;
	unsigned char stopTime_Minutes;
	unsigned char stopTime_Seconds;
	unsigned char ftp_UP_Setp;//ftp下载步骤
	unsigned char ftp_need_UP;//FTP上传数据
	unsigned char diagnostics_Status;//ftp上传状�?}FTP_DATA_UP;

FTP_UPGRADE_DATA  Ftp_Down_Data;
FTP_DATA_UP       Ftp_UP_Data;
//创建一个socket并返�?int socket_connect(char *host,int port)
{
		int rc = -1,clifd;
    struct sockaddr_in address;
    int s, opvalue;
    socklen_t slen;
    
    opvalue = 8;
    slen = sizeof(opvalue);
    memset(&address, 0, sizeof(address));
        
	struct addrinfo *result = NULL;
	struct addrinfo hints = {0, AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP, 0, NULL, NULL, NULL};

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0 ||
        setsockopt(s, IPPROTO_IP, IP_TOS, &opvalue, slen) < 0)
        return -1;
    
    //设置接收和发送超�?    struct timeval timeo = {15, 0};
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &timeo, sizeof(timeo));
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeo, sizeof(timeo));


	if ((rc = getaddrinfo(host, NULL, &hints, &result)) == 0)//域名解析法，也可以是数字
	{
		struct addrinfo* res = result;
		/* prefer ip4 addresses */
		while (res)
		{
			if (res->ai_family == AF_INET)
			{
				result = res;
				break;
			}
			res = res->ai_next;
		}

		if (result->ai_family == AF_INET)
		{
			address.sin_port = htons(port);
			address.sin_family = AF_INET;
			address.sin_addr = ((struct sockaddr_in*)(result->ai_addr))->sin_addr;
		}
		else
			rc = -1;

		freeaddrinfo(result);
	}
	
		
    //address.sin_family = AF_INET;
    //address.sin_port = htons((unsigned short)port);
    
   // struct hostent* server = gethostbyname(host);
   // if (!server)
    //    return -1;
    
   // memcpy(&address.sin_addr.s_addr, server->h_addr, server->h_length);
    
    if (connect(s, (struct sockaddr*) &address, sizeof(address)) == -1)
        return -1;
    
    return s;
}

//连接到一个ftp的服务器，返回socket
int connect_server( char *host, int port )
{    
    int       ctrl_sock;
    char      buf[1024];
    int       result;
    ssize_t   len;
    
    ctrl_sock = socket_connect(host, port);
    if (ctrl_sock == -1) {
        return -1;
    }
    
    len = recv( ctrl_sock, buf, 1024, 0 );
    buf[len] = 0;
    sscanf( buf, "%d", &result );
    if ( result != 220 ) {
        close( ctrl_sock );
        return -1;
    }
    
    return ctrl_sock;
}

//发送命�?返回结果
int ftp_sendcmd_re( int sock, char *cmd, void *re_buf, ssize_t *len)
{
    char        buf[1024];
    ssize_t     r_len;
    
    if (send( sock, cmd, strlen(cmd), 0 ) == -1 )
        return -1;
    	 
	 printf("PATH---发送命�?ftp_sendcmd_re :%s \n",cmd);

    r_len = recv( sock, buf, 1024, 0 );
    if ( r_len < 1 ) return -1;
    buf[r_len] = 0;
    
    if (len != NULL) *len = r_len;
    if (re_buf != NULL) sprintf(re_buf, "%s", buf);
   
 	 printf("PATH---发送命�?返回结果 :%s \n",re_buf);

    return 0;
}

//发送命�?返回编号
int ftp_sendcmd( int sock, char *cmd )
{
    char     buf[1024];
    int      result;
    ssize_t  len;
    
    result = ftp_sendcmd_re(sock, cmd, buf, &len);
		
    if (result == 0)
    {
        sscanf( buf, "%d", &result );
    }
    
    return result;
}

//登录ftp服务�?int login_server( int sock, char *user, char *pwd )
{
    char    buf[128];
    int     result;

    sprintf(buf, "USER %s\r\n",user);
    result = ftp_sendcmd(sock, buf );
    if ( result == 230 ){
			return 0;
		}else if ( result == 331 ) {
		 sprintf(buf, "PASS %s\r\n", pwd);
			if ( ftp_sendcmd(sock, buf ) != 230 ){
				return -1;
			}
       return 0;
    }else{
      return -1;
		}
}

int create_datasock( int ctrl_sock )
{
    int     lsn_sock;
    int     port;
    int     len;
    struct sockaddr_in sin;
    char    cmd[128];
    
    lsn_sock = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP );
    if ( lsn_sock == -1 ) return -1;
    memset( (char *)&sin, 0, sizeof(sin) );
    sin.sin_family = AF_INET;
    if( bind(lsn_sock, (struct sockaddr *)&sin, sizeof(sin)) == -1 ) {
        close( lsn_sock );
        return -1;
    }
    
    if( listen(lsn_sock, 2) == -1 ) {
        close( lsn_sock );
        return -1;
    }
    
    len = sizeof( struct sockaddr );
    if ( getsockname( lsn_sock, (struct sockaddr *)&sin, (socklen_t *)&len ) == -1 )
    {
        close( lsn_sock );
        return -1;
    }
    port = sin.sin_port;
    
    if( getsockname( ctrl_sock, (struct sockaddr *)&sin, (socklen_t *)&len ) == -1 )
    {
        close( lsn_sock );
        return -1;
    }
    
    sprintf( cmd, "PORT %d,%d,%d,%d,%d,%d\r\n",
            sin.sin_addr.s_addr&0x000000FF,
            (sin.sin_addr.s_addr&0x0000FF00)>>8,
            (sin.sin_addr.s_addr&0x00FF0000)>>16,
            (sin.sin_addr.s_addr&0xFF000000)>>24,
            port>>8, port&0xff );
    
    if ( ftp_sendcmd( ctrl_sock, cmd ) != 200 ) {
        close( lsn_sock );
        return -1;
    }
    return lsn_sock;
}

//判断是否是局域网IP
unsigned char IsReservedIp(int ip_addr[])
{
	if( (ip_addr[0] == 10)||
		((ip_addr[0] == 172)&&(ip_addr[1] >= 16)&&(ip_addr[1] <= 31))||
		((ip_addr[0] == 192)&&(ip_addr[1] == 168))
		)
		return 1;
	else
		return 0;
}

//连接到PASV接口
int ftp_pasv_connect( int c_sock )
{
    int     r_sock;
    int     send_re;
    ssize_t len;
    int     addr[6];
    char    buf[1024];
    char    re_buf[1024];
    int data_port;
    //设置PASV被动模式
    bzero(buf, sizeof(buf));
    sprintf( buf, "PASV\r\n");
    send_re = ftp_sendcmd_re( c_sock, buf, re_buf, &len);
    if (send_re == 0) 
	{
        sscanf(re_buf, "%*[^(](%d,%d,%d,%d,%d,%d)",&addr[0],&addr[1],&addr[2],&addr[3],&addr[4],&addr[5]);
		data_port = addr[4]*256+addr[5];
    }else
		return -1;
    
    //连接PASV端口--服务器返回来 的新端口
    bzero(buf, sizeof(buf));
    //sprintf( buf, "%d.%d.%d.%d",addr[0],addr[1],addr[2],addr[3]);
    //r_sock = socket_connect(buf,addr[4]*256+addr[5]);
	//r_sock = socket_connect(Ftp_Down_Data.Ftp_Server_IP, addr[4]*256+addr[5]);//改用协议指定的FTP地址，避免服务器下发内网地址过来不可�?    
	if(IsReservedIp(addr))//如果是内网IP
	{
		if(Ftp_UP_Data.ftp_need_UP == 1)
			r_sock = socket_connect(Ftp_UP_Data.Ftp_Server_IP, data_port);//改用协议指定的FTP地址，避免服务器下发内网地址过来不可�?		else 
			r_sock = socket_connect(Ftp_Down_Data.Ftp_Server_IP, data_port);//改用协议指定的FTP地址，避免服务器下发内网地址过来不可�?	}
	else
	{
		sprintf( buf, "%d.%d.%d.%d",addr[0],addr[1],addr[2],addr[3]);
		r_sock = socket_connect(buf, data_port);
	}
	
    return r_sock;
}

//表示类型
int ftp_type( int c_sock, char mode )
{
    char    buf[128];
    sprintf( buf, "TYPE %c\r\n", mode );
    if ( ftp_sendcmd( c_sock, buf ) != 200 )
        return -1;
    else
        return 0;
}

//改变工作目录
int ftp_cwd( int c_sock, char *path )
{
    char    buf[128];
    int     re;
    sprintf( buf, "CWD %s\r\n", path );
    re = ftp_sendcmd( c_sock, buf );
    if ( re != 250 )
        return -1;
    else
        return 0;
}

//回到上一层目�?int ftp_cdup( int c_sock )
{
    int     re;
    re = ftp_sendcmd( c_sock, "CDUP\r\n" );
    if ( re != 250 )
        return re;
    else
        return 0;
}

//创建目录
int ftp_mkd( int c_sock, char *path )
{
    char    buf[1024];
    int     re;
    sprintf( buf, "MKD %s\r\n", path );
    re = ftp_sendcmd( c_sock, buf );
    if ( re != 257 )
        return re;
    else
        return 0;
}

//列表
int ftp_list( int c_sock, char *path, void **data, unsigned int *data_len)
{
    int     d_sock;
    char    buf[1024];
    int     send_re;
    int     result;
    ssize_t     len,buf_len,total_len;
    
    //连接到PASV接口
    d_sock = ftp_pasv_connect(c_sock);
    if (d_sock == -1) {
        return -1;
    }
    
    //发送LIST命令
    bzero(buf, sizeof(buf));
    sprintf( buf, "LIST %s\r\n", path);
    send_re = ftp_sendcmd( c_sock, buf );
    if (send_re >= 300 || send_re == 0)
        return send_re;
    
    len=total_len = 0;
    buf_len = 1024;
    void *re_buf = malloc(buf_len);
    while ( (len = recv( d_sock, buf, 1024, 0 )) > 0 )
    {
        if (total_len+len > buf_len)
        {
            buf_len *= 2;
            void *re_buf_n = malloc(buf_len);
            memcpy(re_buf_n, re_buf, total_len);
            free(re_buf);
            re_buf = re_buf_n;
        }
        memcpy(re_buf+total_len, buf, len);
        total_len += len;
    }
    close( d_sock );
    
    //向服务器接收返回�?    bzero(buf, sizeof(buf));
    len = recv( c_sock, buf, 1024, 0 );
    buf[len] = 0;
    sscanf( buf, "%d", &result );
    if ( result != 226 )
    {
        free(re_buf);
        return result;
    }
    
    *data = re_buf;
    *data_len = total_len;
    
    return 0;
}

//下载文件
/****************************
*s  ----下载文件�?--源地址
*d  ----本地存储文件地址 --目的地址
*stor_size  --文件大小
*stop  --
****************************/
int ftp_retrfile( int c_sock, char *s, char *d ,unsigned int *stor_size, int *stop)
{
    int     d_sock;
    ssize_t     len,write_len;
    char    buf[1024];
    int     handle;
    int     result;
    
    //打开本地文件
    handle = open( d,  O_WRONLY|O_CREAT|O_TRUNC, S_IREAD|S_IWRITE );
    if ( handle == -1 ) return -1;
    
    //设置传输模式
    ftp_type(c_sock, 'I');
    //ftp_type(c_sock, 'A');

    //连接到PASV接口
    d_sock = ftp_pasv_connect(c_sock);
    if (d_sock == -1)
    {
        close(handle);
        return -1;
    }
     printf("PATH---连接到PASV接口:%d\n",d_sock);

    //发送RETR命令 ----  下载命令
    bzero(buf, sizeof(buf));
    sprintf( buf, "RETR %s\r\n", s);
    result = ftp_sendcmd(c_sock, buf);
	  printf("PATH---!!!!发送RETR命令:%s  result=%d\n",buf,result);

    if (result >= 300 || result == 0)
    {
        close(handle);
				return -1;
       // return result;
    }
     printf("PATH---发送RETR命令:OK\n");

    //开始向PASV读取数据
    bzero(buf, sizeof(buf));
    while ( (len = recv( d_sock, buf, 1024, 0 )) > 0 ) 
	{
		//printf("PATH---读取到服务器发送的数据: len = %d\n",len);
        write_len = write( handle, buf, len );
        if (write_len != len )//|| (stop != NULL && *stop))
        {
			//printf("PATH---读取到服务器发送的数据:OK\n");
            close( d_sock );
            close( handle );
            return -1;
        }
        
        if (stor_size != NULL)
        {
            *stor_size += write_len;
        }
		//usleep(100000000);//test delay===
    }
	printf("PATH---读取到服务器发送的数据--数据下载完毕:OK\n");
  	fsync(handle);
    close(d_sock );
    close(handle );
    if(len < 0)
	{//表示接收数据有问�?		 return  -1;
	}else
	{
		//向服务器接收返回�? -
		bzero(buf, sizeof(buf));
		len = recv(c_sock, buf, 1024, 0 );
		buf[len] = 0;
		sscanf(buf, "%d", &result );
		printf("PATH---读取到服务器发送的数据 接收服务器下发返回值状态result = :%d\n",result);
		return 1; //下载成功

		if (result >= 300 ) 
		{
				return result;
		}
	}
    return 0;
}

//保存文件//上传文件
/****************************
*s  ----下载文件�?--源地址
*d  ----本地存储文件地址 --目的地址
*stor_size  --文件大小
*stop  --
****************************/
int ftp_storfile( int c_sock, char *s, char *d ,unsigned int *stor_size, int *stop)
{
    int     d_sock;
    ssize_t     len,send_len;
    char    buf[1024];
    int     handle;
    int send_re;
    int result;
    
    //打开本地文件
    handle = open( s,  O_RDONLY);
    if ( handle == -1 ) return -1;
    
    //设置传输模式
    ftp_type(c_sock, 'I');
   // ftp_type(c_sock, 'A');

    //连接到PASV接口
    d_sock = ftp_pasv_connect(c_sock);
    if (d_sock == -1)
    {
        close(handle);
        return -1;
    }
    
    //发送STOR命令
    bzero(buf, sizeof(buf));
    sprintf(buf, "STOR %s\r\n",d);
    send_re = ftp_sendcmd(c_sock, buf);
    if (send_re >= 300 || send_re == 0)
    {
        close(handle);
				return -1;
        //return send_re;
    }
    
    //开始向PASV通道写数�?    bzero(buf, sizeof(buf));
    while ((len = read( handle, buf, 1024)) > 0)
    {
        send_len = send(d_sock, buf, len, 0);
        if (send_len != len)// ||
          //  (stop != NULL && *stop))
        {
            close( d_sock );
            close( handle );
            return -1;
        }
        
        if (stor_size != NULL)
        {
            *stor_size += send_len;
        }
    }
    close( d_sock );
    close( handle );
    
    //向服务器接收返回�?    bzero(buf, sizeof(buf));
    len = recv( c_sock, buf, 1024, 0 );
    buf[len] = 0;
    sscanf( buf, "%d", &result );
	 return 1;
    if ( result >= 300 ) {
        return result;
    }
    return 0;
}

//修改文件�?移动目录
int ftp_renamefile( int c_sock, char *s, char *d )
{
    char    buf[1024];
    int     re;
    
    sprintf(buf, "RNFR %s\r\n", s);
    re = ftp_sendcmd( c_sock, buf);
    if ( re != 350 ) return re;
    sprintf(buf, "RNTO %s\r\n", d);
    re = ftp_sendcmd( c_sock, buf);
    if ( re != 250 ) return re;
    return 0;
}

//删除文件
int ftp_deletefile( int c_sock, char *s )
{
    char    buf[1024];
    int     re;
    
    sprintf( buf, "DELE %s\r\n", s );
    re = ftp_sendcmd( c_sock, buf );
    if ( re != 250 ) return re;
    return 0;
}

//删除目录
int ftp_deletefolder( int c_sock, char *s )
{
    char    buf[1024];
    int     re;
    
    sprintf( buf, "RMD %s\r\n", s );
    re = ftp_sendcmd( c_sock, buf );
    if ( re != 250 ) return re;
    return 0;
}

//链接服务�?int ftp_connect( char *host, int port, char *user, char *pwd )
{
    int  c_sock;
    c_sock = connect_server(host, port );
	 printf("PATH CHANGED---建立连接�?");
    if ( c_sock == -1 ) return -1;
    if ( login_server( c_sock, user, pwd ) == -1 ) {
        close( c_sock );
        return -1;
    }
    return c_sock;
}

//断开服务�?int ftp_quit( int c_sock)
{
    int re = 0;
    re = ftp_sendcmd( c_sock, "QUIT\r\n" );
    close( c_sock );
    return re;
}


void Ftp_Task()
{
	//  char *ftpbuf;
	int receiverbef_len=1000,stor_size =0,stop = 1,retries =0,i=0;  
	int Cmd_sock_fd=-1,Result = 0,find_name_count = 0;//命令
	time_t current_now=0,retryIntervaltime = 0;
	char read_buf[1024], temp_char[1024];
	char buf[100];
	char buf2[100];
	char md5[100];
	char charger_md5[33],qt_md5[33],control_md5[33];
	FILE* fd;
	int qt_need_upgrade=0,charger_need_upgrade=0,control_need_upgrade=0;
	char *p;

	prctl(PR_SET_NAME,(unsigned long)"Ftp_Task");//设置线程名字 
	
	memset(&Ftp_Down_Data,0,sizeof(FTP_UPGRADE_DATA));
	memset(&g_ftp_upgrade_info,0,sizeof(FTP_UPGRADE_INFO));
	Globa->system_upgrade_busy = 0;
	while(1)
	{
		usleep(100000);
		time(&current_now);

		if( (0xAA == g_ftp_upgrade_info.VV) && (0 == Ftp_Down_Data.ftp_need_down) )
		{
			sprintf(Ftp_Down_Data.Ftp_Server_IP,"%d.%d.%d.%d",
						g_ftp_upgrade_info.ftp_ip[0],
						g_ftp_upgrade_info.ftp_ip[1],
						g_ftp_upgrade_info.ftp_ip[2],
						g_ftp_upgrade_info.ftp_ip[3]);
			
			Ftp_Down_Data.port = g_ftp_upgrade_info.ftp_port[0];//
			my_strncpy(Ftp_Down_Data.User, g_ftp_upgrade_info.ftp_user ,sizeof(Ftp_Down_Data.User));
			my_strncpy(Ftp_Down_Data.Password, g_ftp_upgrade_info.ftp_password ,sizeof(Ftp_Down_Data.Password));
			my_strncpy(Ftp_Down_Data.down_file_name, g_ftp_upgrade_info.ftp_filename ,sizeof(Ftp_Down_Data.down_file_name));
			my_strncpy(Ftp_Down_Data.path, g_ftp_upgrade_info.ftp_path ,sizeof(Ftp_Down_Data.path));
			
			g_ftp_upgrade_info.VV = 0;//clear
			Ftp_Down_Data.ftp_need_down = 1;
			Ftp_Down_Data.ftp_down_Setp = 1;//start
		}

		if(Ftp_Down_Data.ftp_need_down == 1)
		{//需要下�?		
			switch(Ftp_Down_Data.ftp_down_Setp)
			{
				case 0x00:
				
				break;
				case 0x01:
				{	
					Ftp_Down_Data.FirmwareStatus = 1;						
					
					Cmd_sock_fd = ftp_connect(Ftp_Down_Data.Ftp_Server_IP,Ftp_Down_Data.port,Ftp_Down_Data.User,Ftp_Down_Data.Password);//创建一个socket并登录ftp服务�?--命令帧用21端口
					printf("---表示登录成功并建立连接了: %d\n",Cmd_sock_fd);
					if(Cmd_sock_fd == -1)
					{
						Ftp_Down_Data.ftp_down_Setp = 0x10;//连接失败
						retries ++;
						sleep(2);
						retryIntervaltime = current_now;
					}else
					{
						Ftp_Down_Data.ftp_down_Setp = 0x02;//连接成功
					}
				  break;
				}
				case 0x02:
				{
					if(strlen(Ftp_Down_Data.path) > 0)
					{
						if(ftp_cwd(Cmd_sock_fd,Ftp_Down_Data.path) == 0)
						{
							Ftp_Down_Data.ftp_down_Setp = 0x03;//
						}else
						{
							sleep(2);
							Ftp_Down_Data.ftp_down_Setp = 0x10;//下载失败
							retries ++;
							retryIntervaltime = current_now;
						}
					}else
					{
					  Ftp_Down_Data.ftp_down_Setp = 0x03;//
					}
					break;
				}
				case 0x03:
				{//下载文件函数
					if(strlen(Ftp_Down_Data.down_file_name) > 0)
					{
						Result =  ftp_retrfile(Cmd_sock_fd,Ftp_Down_Data.down_file_name, F_UPDATE_Charger_Package ,&stor_size,&stop);
					}else
					{
						Result =  ftp_retrfile(Cmd_sock_fd,DEFAULT_UPGRADE_FILENAME, F_UPDATE_Charger_Package ,&stor_size,&stop);
					}
					
					printf("PATH CHANGED---下载完毕:%d\n",stor_size);
					if(Result == 1)
					{//下载成功
						Ftp_Down_Data.FirmwareStatus = 3;
						Ftp_Down_Data.ftp_down_Setp = 0x04;//接下来处理下载的文件
						sleep(2);
						find_name_count = 0;
					}else
					{
						sleep(2);
						Ftp_Down_Data.ftp_down_Setp = 0x10;//下载失败
					//	Ftp_Down_Data.FirmwareStatus = 2;
						retryIntervaltime = current_now;
						retries ++;
					}
					 ftp_quit(Cmd_sock_fd);
					 Cmd_sock_fd = -1;
					break;
				}
			
  				case 0x04:
				{//进行解压
				  	find_name_count++;
					if(access(F_UPDATE_Charger_Package,F_OK) != -1)
					{//存在更新文件
					 	Ftp_Down_Data.FirmwareStatus = 4;
						printf("tar -zvxf /mnt/Nand1/update/DC_Charger.tar.gz  -C  /mnt/Nand1/update/ \n");
						system("tar -zvxf /mnt/Nand1/update/DC_Charger.tar.gz  -C  /mnt/Nand1/update/");
						Ftp_Down_Data.ftp_down_Setp = 0x05; //先校验升级包
						sleep(4);
						find_name_count = 0;
						//system(sync);
					}else if(find_name_count>=3)
					{
						find_name_count = 0;
						Ftp_Down_Data.FirmwareStatus = 6;
						Ftp_Down_Data.ftp_down_Setp = 0x10; //安装失败
						retries ++;
						time(&current_now);
						time(&retryIntervaltime);
						printf("rm -rf /mnt/Nand1/update/DC_Charger.tar.gz\n");
						system("rm -rf /mnt/Nand1/update/DC_Charger.tar.gz");
						sleep(1);
					}
					break;
				}
				case 0x05://判断升级包的完整�?				{
					control_need_upgrade = 0;
					qt_need_upgrade = 0;
					charger_need_upgrade = 0;
					if(access("/mnt/Nand1/update/update.info",F_OK)==0)//升级文件信息
					{
						printf("fopen /mnt/Nand1/update/update.info\n");
						fd=fopen("/mnt/Nand1/update/update.info","r");
						if(fd)
						{
							memset(charger_md5,0,33);
							memset(qt_md5,0,33);
							memset(control_md5,0,33);
							i=0;
							while(fgets(buf,100, fd) > 0)
							{
								if(NULL !=strstr(buf,"charger"))
									memcpy(charger_md5,buf,32);
								if(NULL !=strstr(buf,"Electric_Charge_Pile"))
									memcpy(qt_md5,buf,32);
								if(NULL !=strstr(buf,"EASE_DC_CTL.bin"))
									memcpy(control_md5,buf,32);	
								i++;
								if(i>=3)//目前�?个文�?									break;
							}
							fclose(fd);	
							
							if(access(F_UPDATE_Charger,F_OK) != -1)
							{	
								MD5File(F_UPDATE_Charger,buf2);
								p=md5;												
								for(i=0;i<16;i++)
								{
									sprintf(p,"%02x",buf2[i]);
									p+=2;
								}
								AppLogOut("Update_Charger: MD5=%s,checked MD5=%s",charger_md5,md5);
								if(strncmp(charger_md5,md5,strlen(md5))!=0)
								{	
									system("rm -rf /mnt/Nand1/update/charger"); //文件校验异常
								}
								else
								{
									charger_need_upgrade = 1;
								}
							}
							if(access(F_UPDATE_Electric_Charge_Pile,F_OK) != -1)
							{	
								MD5File(F_UPDATE_Electric_Charge_Pile,buf2);
								p=md5;
								for(i=0;i<16;i++)
								{
									sprintf(p,"%02x",buf2[i]);
									p+=2;
								}
								AppLogOut("Electric_Charge_Pile: MD5=%s,checked MD5=%s",qt_md5,md5);
								if(strncmp(qt_md5,md5,strlen(md5))!=0)
								{	
									system("rm -rf /mnt/Nand1/update/Electric_Charge_Pile"); //文件校验异常
								}
								else
								{
									qt_need_upgrade = 1;
								}
								
							}
							if(access(F_UPDATE_EAST_DC_CTL,F_OK) != -1)
							{
								MD5File(F_UPDATE_EAST_DC_CTL,buf2);
								p=md5;													
								for(i=0;i<16;i++)
								{
									sprintf(p,"%02x",buf2[i]);
									p+=2;
								}
								AppLogOut("EASE_DC_CTL.bin  MD5=%s,checked MD5=%s",control_md5,md5);
								if(strncmp(control_md5,md5,strlen(md5))!=0)
								{	
									system("rm -rf /mnt/Nand1/update/EASE_DC_CTL.bin"); //文件校验异常
								}
								else
								{
									control_need_upgrade = 1;
								}
								
							}
							
							fclose(fd);
							if((control_need_upgrade == 1)||(qt_need_upgrade == 1)||(charger_need_upgrade == 1))
								Ftp_Down_Data.ftp_down_Setp = 0x06; //进行升级
							else
								Ftp_Down_Data.ftp_down_Setp = 0x11; //文件异常，升级结�?						}							
					}else
						Ftp_Down_Data.ftp_down_Setp = 0x11; //文件异常，升级结�?				}

				case 0x06:
				{
					if((access(F_UPDATE_Electric_Charge_Pile,F_OK) != -1)||
						(access(F_UPDATE_Charger,F_OK) != -1)||
						(access(F_UPDATE_EAST_DC_CTL,F_OK) != -1)
					)
					{//存在更新文件
						Ftp_Down_Data.ftp_down_Setp = 0x07; //进行升级
					}else
					{
						Ftp_Down_Data.FirmwareStatus = 6;
						Ftp_Down_Data.ftp_down_Setp = 0x10; //安装失败
						retries ++;
						time(&current_now);
						time(&retryIntervaltime);
						sleep(1);
					}
					printf("Ftp_Down_Data.ftp_down_Setp=6\n");					
					break;
				}
				case 0x07:
				{//等待系统重启升级
					if((Globa->QT_Step == 0x02)|| (Globa->QT_Step == 0x43))
					{//系统处于空闲的时�?						Globa->system_upgrade_busy = 1;//升级锁定
						printf("Ftp_Down_Data.ftp_down_Setp=7\n");
						FILE* fp_new = fopen(F_UPDATE_Charger_Conf, "r");
						if(fp_new != NULL)
						{
							memset(read_buf,0,1024);
							while(NULL != fgets(read_buf,1024, fp_new))
							{
								snprintf(&temp_char[0], 100,"%s", read_buf);	
								//printf("读取文件内容--= %s-\n",temp_char);																
								system(temp_char);	
								usleep(500000);								
							}
							fclose(fp_new);
						}
						//else
						{
							if(access(F_UPDATE_Electric_Charge_Pile,F_OK) != -1)
							{//存在更新文件
								system("cp -rf /mnt/Nand1/update/Electric_Charge_Pile /home/root/");
								sleep(4);
								system("chmod 777 /home/root/Electric_Charge_Pile");
								sleep(1);
								system("rm -rf /mnt/Nand1/update/Electric_Charge_Pile");
								sleep(1);
							}
							if(access(F_UPDATE_Charger,F_OK) != -1)
							{//存在更新文件
								system("cp -rf /mnt/Nand1/update/charger /home/root/");
								sleep(4);
								system("chmod 777 /home/root/charger");
								sleep(1);
								system("rm -rf /mnt/Nand1/update/charger");
								sleep(1);
							}
						}
						Ftp_Down_Data.FirmwareStatus = 5;
						Globa->Charger_param.ftp_upgrade_result = 1;//success
						Globa->Charger_param.ftp_upgrade_notify = 0x55AA;
						System_param_save();
						sleep(3);
						Ftp_Down_Data.ftp_down_Setp = 0;
						memset(Ftp_Down_Data.Ftp_Server_IP,0,sizeof(FTP_UPGRADE_DATA));
						system("sync");
						system("reboot");						
					}
						break;
				}
				case 0x10:
				{//失败
					Ftp_Down_Data.FirmwareStatus = 2;
					printf("Ftp_Down_Data.ftp_down_Setp=0x10\n");	
					if(retries >= Ftp_Down_Data.retries)
					{
						sleep(4);
						memset(Ftp_Down_Data.Ftp_Server_IP,0,(sizeof(FTP_UPGRADE_DATA)));
						retries = 0;
						Ftp_Down_Data.ftp_need_down = 0;//clear
						//sleep(3);
						//置失败，上报发送升级失败结�?
						Globa->Charger_param.ftp_upgrade_result = 0;//failed
						Globa->Charger_param.ftp_upgrade_notify = 0x55AA;
						System_param_save();
					}else
					{
						if(abs(current_now - retryIntervaltime)>= Ftp_Down_Data.retryInterval)
						{
							Ftp_Down_Data.ftp_down_Setp = 0x01;
							retryIntervaltime = current_now;
						}
					}
					break;
				}
				case 0x11://升级文件异常
				{
					memset(Ftp_Down_Data.Ftp_Server_IP,0,(sizeof(FTP_UPGRADE_DATA)));
					retries = 0;
					Ftp_Down_Data.ftp_need_down = 0;//clear
					//置失败，上报发送升级失败结�?					Globa->Charger_param.ftp_upgrade_result = 0;//failed
					Globa->Charger_param.ftp_upgrade_notify = 0x55AA;
					System_param_save();
					printf("Ftp_Down_Data.ftp_down_Setp=0x11\n");
					Ftp_Down_Data.ftp_down_Setp = 0;
				}
				break;
			}
			
		}
		else//idle
			sleep(5);
		
	}
}
extern void Ftp_Thread()
{
	pthread_t td1;
	int ret ,stacksize = 1024*1024;//根据线程的实际情况设定大�?	pthread_attr_t attr;

	ret = pthread_attr_init(&attr);
	if (ret != 0)
		return;

	ret = pthread_attr_setstacksize(&attr, stacksize);
	if(ret != 0)
		return;
	
	if(0 != pthread_create(&td1, &attr, (void *)Ftp_Task, NULL)){
		return;
	}

	if(0 != pthread_attr_destroy(&attr)){
		return;
	}
}