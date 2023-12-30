// 该文件由701Enti编写，包含一些ESP32_S3通过硬件外设与TCA6416建立配置与扩展IO数据的通讯
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 本库特性：1 由于IO控制时，有随时需要调用TCA6416A写入函数的需求，本库不会出现调用一次函数归定只能改一个IO或读一个IO还要传一系列参数的尴尬问题，而是一齐读写,同时还会保存实时IO数据，因此没有用到电平反转寄存器
//          2 使用时直接修改公共变量不需调用函数以在项目非常方便使用，加之，可以像sevetest30_gpio.c封装后使用FreeRTOS支持，并添加中断支持，一但IO电平变化就读取，没有变就不读，客观上可以大大提高资源利用率
// 原理：   运用结构体地址一般为结构体中第一个成员变量地址，并且本例中，成员类型均为bool,地址递加从而可以方便地扫描所有成员，
// 敬告： 0 为更加方便后续开发或移植，本库不包含关于FreeRTOS支持的封装，公共变量修改方式的服务封装，以及中断服务的封装，如果需要参考，请参照sevetest30_gpio.c
//       1 本库会保存实时IO数据，因此没有用到电平反转寄存器            
//       2 文件本体不包含i2c通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行
//       3 对于设计现实的不同，您可以更改存储IO电平结构体成员变量名，但是必须确保实际对应的IO次序不变以 P00 P01 P02 P03 。。。
//       4 我们已在Pre-a 2.0版本移动上述结构体定义到board_def.h文件
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.7.9

// #ifndef _TCA6416A_H_
// #define _TCA6416A_H_
// #endif

#pragma once

#include <string.h>
#include <stdbool.h>

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
    bool p00;
    bool p01;
    bool p02;
    bool p03;
    bool p04;
    bool p05;
    bool p06;
    bool p07;
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

//对于设计现实的不同，您可以更改结构体成员变量名，独立应用于您的程序
//但是必须确保定义时成员对应的IO次序不变，定义顺序必须为引脚顺序p00-p17
typedef struct TCA6416A_value_t
{

   bool main_button; //主按键低电平表示按下
   bool en_led_board;//高电平将使得灯板关闭
   bool hp_detect;   //耳机已插入信号 1表示检测到耳机插入
   bool s2;//s(x)为红外接收 0表示接收到红外信号
   bool s1;
   bool ir;//红外发射 1发射红外光
   bool s4;
   bool s3;
   bool amplifier_SD;//功放使能   1使能
   bool IMU_INT;//姿态传感器-惯性测量中断信号 
   bool ALS_INT;//环境光传感器-中断信号
   bool thumbwheel_CCW;//拨轮开关-0表示逆时针转动 （CCW和CW同时0表示按下-PUSH）
   bool thumbwheel_CW; //拨轮开关-0表示顺时针转动 （CCW和CW同时0表示按下-PUSH）
   bool OTG_EN;//OTG电源供应 1启动 启动后SE30充电活动将被硬件性禁止
   bool charge_SIGN;//正在充电信号 0表示正在充电
   bool ALARM_INT;//闹钟中断信号 0表示设定的中断事务进行中 该信号还会在关机状态下唤醒SE30

   bool addr; /// ADDR引脚电平，用于设置主机地址
}TCA6416A_value_t;

void TCA6416A_mode_set(TCA6416A_mode_t *pTCA6416Amode);

void TCA6416A_gpio_service(TCA6416A_value_t *pTCA6416Avalue);






//结合开发需求，我们已在Pre-a 2.0版本移动以下定义到board_def.h文件
//如果您不需要这样，以下注释内容可能是您需要加入该文件的一种写法示例

// 默认模式 0=输出模式 1=输入模式(斜杠与注释冲突，因此略去)
// #define TCA6416A_DEFAULT_CONFIG_MODE   {
//     .p00 = 0,                           
// ...                                     
//     .p17 = 0,                           
//     .addr=0,                            
// }

// 默认电平值(斜杠与注释冲突，因此略去)
// #define TCA6416A_DEFAULT_CONFIG_VALUE  {   
//     .l4 = 1,                               
//     .s4 = 0,                             
//     .l3 = 1,                               
// ...                                    
//     .addr=0,                             
// }



