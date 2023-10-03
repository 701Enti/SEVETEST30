// 该文件由701Enti整合，包含一些sevetest30的GPIO配置工作(内部+扩展)，主要为了TCA6416A控制函数的解耦
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体只针对sevetest30的硬件设计，该库代码中的配置是否合理会影响到设备能否正常运行，请谨慎修改
//      此处没有使用TCA6416A的RESET引脚，但并不代表RESET可以悬空，请将其上拉到VCC,并在RESET连接一个1uF左右电容到GND(这不是对TCA6416A的使用建议)
//      库包含一个中断，输入方式的扩展IO电平变化，自动读取将会触发，默认开启,中断触发会将 read_ext_io 置为 1，需要外部大循环检测到进行处理并置为0，这主要为了防止堆栈溢出
//      如果外部程序不希望触发自动读取，可以直接通过 ext_io_auto_read_flag = false; 进行直接性屏蔽,结束后必须 ext_io_auto_read_flag = true; 置回
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.7.16

#include "TCA6416A.h"


#ifndef _SEVETEST30_GPIO_H_
#define _SEVETEST30_GPIO_H_
#endif 

#define TURN_GPIO GPIO_NUM_10//主按键IO

//此处没有使用TCA6416A的RESET引脚，但并不代表RESET可以悬空，请将其上拉到VCC,并在RESET连接一个1uF左右电容到GND(这不是对TCA6416A的使用建议)
//#define TCA6416A_IO_INT  GPIO_NUM_X 
#define TCA6416A_IO_INT   GPIO_NUM_1   // TCA6416A的中断信号输出

extern TCA6416A_mode_t  ext_io_mode_data; //扩展IO输入输出模式（公共变量）
extern TCA6416A_value_t ext_io_value_data;//扩展IO电平信息，写入和回读通用（公共变量）

extern bool ext_io_auto_read_flag;//如果外部程序不希望触发自动读取，可以直接通过 ext_io_auto_read_flag = false; 进行屏蔽,结束后必须 ext_io_auto_read_flag = true; 置回
extern bool read_ext_io;
void sevetest30_gpio_init();

void ext_io_value_service();

void ext_io_mode_service();

//以下是内部使用函数，一般无需调用

//外部IO的初始化工作已经会在gpio_init函数调用时一起执行
void ext_io_init();




