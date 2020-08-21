/* 		tcp_modbus.h

*/

#ifndef MODBUSTCP_H
#define MODBUSTCP_H


#define  REG_04INPUT_NREGS  1024//256//1588
#define  REG_02INPUT_NREGS  256//3232  //寄存器访问界线
#define  REG_06INPUT_NREGS  128

//----modbus报文头格式------------
typedef enum modbus_mbap{
    MBAP_flag,                //事务处理标识符
    MBAP_proto=2,             //协议标识符
    MBAP_length=4,            //长度
    MBAP_unit=6,              //单元标识符
    MBAP_fun                 //功能码
}MODBUS_MBAP;

//----modbus 异常响应格式------------
typedef enum modbus_exmb{
    EXMB_fun=7,                //异常功能码
    EXMB_state                //异常码
}MODBUS_EXMB;

//----modbus 02 读离散量输入格式------------
typedef enum modbus_cmb02{
    CMB02_fun=7,               //功能码
    CMB02_addrH,               //起始地址 高字节
    CMB02_addrL,               //起始地址 低字节
    CMB02_regnumH,             //寄存器个数  高字节
    CMB02_regnumL             //寄存器个数  低字节
}MODBUS_CMB02;


//----modbus 02 响应格式------------
typedef enum modbus_rmb02{
    RMB02_fun=7,                 //功能码
    RMB02_regnum,                //数据字节数
    RMB02_regdata               //寄存器数据 = n
}MODBUS_RMB02;


//----modbus 03 读输入寄存器 格式------------
typedef enum modbus_cmb03{
    CMB03_fun=7,               //功能码
    CMB03_addrH,               //起始地址 高字节
    CMB03_addrL,               //起始地址 低字节
    CMB03_regnumH,             //寄存器个数  高字节
    CMB03_regnumL            //寄存器个数  低字节
}MODBUS_CMB03;
//----modbus 03 响应格式------------
typedef enum modbus_rmb03{
    RMB03_fun=7,                  //功能码
    RMB03_regnum,                 //数据字节数
    RMB03_regdata                //寄存器数据 = 寄存器个数*2
}MODBUS_RMB03;

//----modbus 04 读输入寄存器 格式------------
typedef enum modbus_cmb04{
    CMB04_fun=7,               //功能码
    CMB04_addrH,               //起始地址 高字节
    CMB04_addrL,               //起始地址 低字节
    CMB04_regnumH,             //寄存器个数  高字节
    CMB04_regnumL            //寄存器个数  低字节
}MODBUS_CMB04;
//----modbus 04 响应格式------------
typedef enum modbus_rmb04{
    RMB04_fun=7,                  //功能码
    RMB04_regnum,                 //数据字节数
    RMB04_regdata                //寄存器数据 = 寄存器个数*2
}MODBUS_RMB04;

//----modbus 06 读输入寄存器 格式------------
typedef enum modbus_cmb06{
    CMB06_fun=7,               //功能码
    CMB06_addrH,               //起始地址 高字节
    CMB06_addrL,               //起始地址 低字节
    CMB06_regdataH,             //寄存器值
    CMB06_regdataL             //寄存器值
}MODBUS_CMB06;
//----modbus 06 响应格式------------
typedef enum modbus_rmb06{
    RMB06_fun=7,                  //功能码
    RMB06_addrH,               //起始地址 高字节
    RMB06_addrL,               //起始地址 低字节
    RMB06_regdataH,             //寄存器值
    RMB06_regdataL            //寄存器值
}MODBUS_RMB06;


//----modbus 10 读输入寄存器 格式------------
typedef enum modbus_cmb10{
    CMB10_fun=7,               //功能码
    CMB10_addrH,               //起始地址 高字节
    CMB10_addrL,               //起始地址 低字节
    CMB10_regnumH,             //寄存器个数  高字节
    CMB10_regnumL,             //寄存器个数  低字节
    CMB10_datanum,             //数据字节数 = 寄存器个数*2
    CMB10_regdata            //寄存器数据 = 寄存器个数*2
}MODBUS_CMB10;

//----modbus 10 响应格式------------
typedef enum modbus_rmb10{
    RMB10_fun=7,                  //功能码
    RMB10_addrH,               //起始地址 高字节
    RMB10_addrL,               //起始地址 低字节
    RMB10_regnumH,             //寄存器个数  高字节
    RMB10_regnumL             //寄存器个数  低字节
}MODBUS_RMB10;

INT32 Tcp_modbus_Deal(unsigned int fd, unsigned char *msg, unsigned char length);

extern void TCP_MODBUS_thread(void);
#endif  /* MODBUSTCP_H */
