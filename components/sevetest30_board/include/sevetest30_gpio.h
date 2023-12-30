// 该文件由701Enti整合，包含一些sevetest30的GPIO配置工作(内部+扩展)，主要为了TCA6416A控制函数的解耦
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体只针对sevetest30的硬件设计，该库代码中的配置是否合理会影响到设备能否正常运行，请谨慎修改
//      此处没有使用TCA6416A的RESET引脚，但并不代表RESET可以悬空，请将其上拉到VCC,并在RESET连接一个1uF左右电容到GND(这不是对TCA6416A的使用建议)
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.7.16

#include "TCA6416A.h"
#ifndef _SEVETEST30_GPIO_H_
#define _SEVETEST30_GPIO_H_
#endif 

#define BAT_IN_CTRL_GPIO GPIO_NUM_10//电池接入控制

//此处没有使用TCA6416A的RESET引脚，但并不代表RESET可以悬空，请将其上拉到VCC,并在RESET连接一个1uF左右电容到GND(这不是对TCA6416A的使用建议)
//#define TCA6416A_IO_RESET  GPIO_NUM_X 
#define TCA6416A_IO_INT   GPIO_NUM_1   // TCA6416A的中断信号输出

#define EXT_IO_READ_INTR_FLAG         (ESP_INTR_FLAG_LEVEL3)

#define EXT_IO_READ_EVT_CORE           (1)
#define EXT_IO_READ_EVT_PRIO           (2)



typedef struct ext_io_ctrl_t
{
 bool auto_read_EN; //读中断信号输出使能，true运行，false屏蔽
 bool auto_read_INT;//读中断信号         true表示存在读取需求
}ext_io_ctrl_t;

extern ext_io_ctrl_t ext_io_ctrl;

void sevetest30_gpio_init(TCA6416A_mode_t* p_ext_mode,TCA6416A_value_t* p_ext_value);

void ext_io_value_service();

void ext_io_mode_service();

//以下是内部使用函数，一般无需调用

//外部IO的初始化工作已经会在gpio_init函数调用时一起执行
void ext_io_init(TCA6416A_mode_t* p_mode,TCA6416A_value_t* p_value);




