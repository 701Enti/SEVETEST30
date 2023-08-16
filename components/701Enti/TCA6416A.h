// 该文件由701Enti编写，包含一些ESP32_S3通过硬件外设与TCA6416建立配置与扩展IO数据的通讯
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告： 1 文件本体不包含i2c通讯的任何初始化配置，若您未进行配置，这可能无法运行，或许可参考同一项目组中sevetest30_BusConf.h，其中包含粗略易用的配置工作
//       2 请注意外部引脚模式设置，错误的配置可能导致您的设备损坏，我们不建议修改这些默认配置 
//       3 对于设计现实的不同，您可以更改结构体成员变量名，但是必须确保对应的IO次序不变如 P00 P01 P02 P03 以此类推
//         同时成员变量名是上级程序识别操作引脚的关键，如果需要使用其上级程序而不仅仅是TCA6416A库函数，结构体成员变量名不应该修改，即使是对当前硬件的更新
// 电路特性： l(x)为红外发射 1有效 s(x)为红外发射接收 0表示接收到红外信号
//           opt3001_INT为环境光中断信号，有效电平自由配置
//           charge_SIGN finished_SIGN为充电信号，0有效
//           ns4268_SD 功放使能   1有效
//           ns4268_MUTE 功放静音 1有效
//           hpin  耳机已插入信号  1表示检测到耳机插入
//           addr ADDR引脚电平，用于设置主机地址
// 邮箱：   3044963040@qq.com
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.7.9

#ifndef _TCA6416A_H_
#define _TCA6416A_H_
#endif

#include <string.h>
#include <stdbool.h>

#define TCA6416A_IO_INT   GPIO_NUM_1   // TCA6416A的中断信号输出
#define TCA6416A_IO_RESET GPIO_NUM_2 // TCA6416A的复位信号输入

// 下面列出TCA6416A中一些对sevetest30工程具有价值的寄存器，您可以添加需要的寄存器,详细信息可参考TCA6416A规格书
// 两个输入值寄存器，只读有效
#define TCA6416A_IN1 0x00
#define TCA6416A_IN2 0x01
// 两个输出值寄存器
#define TCA6416A_OUT1 0x02
#define TCA6416A_OUT2 0x03
// 两个模式寄存器，置0使能输出，用于初始化工作
#define TCA6416A_MODE1 0x06
#define TCA6416A_MODE2 0x07

typedef struct TCA6416A_mode_t // 模式配置，0=输出模式 1=输入模式（未写入默认为）
{
    // 写入mode_reg1的位值，低到高，根据sevetest30电路设计特性，配置为 0101 1010
    bool p00;
    bool p01;
    bool p02;
    bool p03;
    bool p04;
    bool p05;
    bool p06;
    bool p07;
    // 写入mode_reg2的位值，低到高，根据sevetest30电路设计特性，配置为 1111 1100
    bool p10;
    bool p11;
    bool p12;
    bool p13;
    bool p14;
    bool p15;
    bool p16;
    bool p17;

    bool addr; /// ADDR引脚电平，用于设置主机地址
} TCA6416A_mode_t;

#define TCA6416A_DEFAULT_CONFIG_MODE   {\
    .p00 = 0,                           \
    .p01 = 1,                           \
    .p02 = 0,                           \
    .p03 = 1,                           \
    .p04 = 1,                           \
    .p05 = 0,                           \
    .p06 = 1,                           \
    .p07 = 0,                           \
    .p10 = 1,                           \
    .p11 = 1,                           \
    .p12 = 1,                           \
    .p13 = 1,                           \
    .p14 = 1,                           \
    .p15 = 1,                           \
    .p16 = 0,                           \
    .p17 = 0,                           \
    .addr=0,                            \
}

//对于设计现实的不同，您可以更改结构体成员变量名，独立应用于您的程序，但是
//必须确保定义时成员对应的IO次序不变，必须按照 P00 P01 P02 P03....,比如这里定义顺序为 bool l4,bool s4,bool l3,bool s3....以此类推,因为写入时按照成员地址的顺序，不能乱
//同时变量名是上级程序识别操作引脚的关键，如果需要使用其上级程序而不仅仅是TCA6416A库函数，结构体成员变量名不应该修改，即使是对当前硬件的更新
typedef struct TCA6416A_value_t
{
    // 红外传感器的发射（L） x4 红外传感器的接收（S） x4 （P00-P07) 
    bool l4;// l(x)为红外发射 1有效
    bool s4;//s(x)为红外发射接收 0表示接收到红外信号
    bool l3;// l(x)为红外发射 1有效
    bool s3;//s(x)为红外发射接收 0表示接收到红外信号
    bool s1;//s(x)为红外发射接收 0表示接收到红外信号
    bool l1;// l(x)为红外发射 1有效
    bool s2;//s(x)为红外发射接收 0表示接收到红外信号
    bool l2;// l(x)为红外发射 1有效

    // 悬空引脚（P10-P11)
    bool nc10; // 悬空引脚1，可自由配置使用
    bool nc11; // 悬空引脚2，可自由配置使用

    // 各种外设(P12-P17)
    bool opt3001_INT;   // OPT3001的中断信号 有效电平自由配置
    bool finished_SIGN; // 充电完成信号 0有效
    bool charge_SIGN;   // 正在充电信号 0有效
    bool hpin;          // 耳机已插入信号 1表示检测到耳机插入
    bool ns4268_MUTE;   // NS4268的MUTE静音配置引脚 1有效
    bool ns4268_SD;     // NS4268的SD低功耗配置引脚 1有效

    bool addr; /// ADDR引脚电平，用于设置主机地址
}TCA6416A_value_t;

#define TCA6416A_DEFAULT_CONFIG_VALUE  {\
    .l4 = 0,                           \
    .s4 = 0,                           \
    .l3 = 0,                           \
    .s3 = 0,                           \
    .s1 = 0,                           \
    .l1 = 0,                           \
    .s2 = 0,                           \
    .l2 = 0,                           \
    .nc10 = 0,                         \
    .nc11 = 0,                         \
    .opt3001_INT = 0,                  \
    .finished_SIGN = 1,                \
    .charge_SIGN = 1,                  \
    .hpin = 0,                         \
    .ns4268_MUTE = 0,                  \
    .ns4268_SD = 1,                    \
    .addr=0,                           \
}


void TCA6416A_mode_set(TCA6416A_mode_t *pTCA6416Amode);

void TCA6416A_gpio_service(TCA6416A_value_t *pTCA6416Avalue);
