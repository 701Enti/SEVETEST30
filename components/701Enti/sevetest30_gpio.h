// 该文件由701Enti整合，包含一些sevetest30的GPIO配置工作(内部+扩展)
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体只针对sevetest30的硬件设计
// 邮箱：   3044963040@qq.com
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.7.10

#include "TCA6416A.h"


#ifndef _SEVETEST30_GPIO_H_
#define _SEVETEST30_GPIO_H_
#endif 

#define TURN_GPIO GPIO_NUM_10//主按键IO

extern TCA6416A_mode_t  ext_io_mode_data; //扩展IO输入输出模式（公共变量）
extern TCA6416A_value_t ext_io_value_data;//扩展IO电平信息，写入和回读通用（公共变量）

void sevetest30_gpio_init();

void ext_io_value_service();

void ext_io_mode_service();

//以下是内部使用函数，一般无需调用

//外部IO的初始化工作已经会在gpio_init函数调用时一起执行
void ext_io_init();




