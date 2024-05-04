/*
 * 701Enti MIT License
 *
 * Copyright © 2024 <701Enti organization>
 *
 * Permission is hereby granted, free of charge, to any person obtaining 
 * a copy of this software and associated documentation files (the “Software”), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

// 该文件归属701Enti组织，SEVETEST30开发团队应该提供责任性维护，包含一些sevetest30的GPIO配置工作(内部+扩展)，主要为了TCA6416A控制函数的解耦
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体只针对sevetest30的硬件设计，该库代码中的配置是否合理会影响到设备能否正常运行，请谨慎修改
//      此处没有使用TCA6416A的RESET引脚，但并不代表RESET可以悬空，请将其上拉到VCC,并在RESET连接一个1uF左右电容到GND(这不是对TCA6416A的使用建议)
// github: https://github.com/701Enti
// bilibili: 701Enti

#include "TCA6416A.h"
#ifndef _SEVETEST30_GPIO_H_
#define _SEVETEST30_GPIO_H_
#endif 


#define EXT_IO_READ_INTR_FLAG         (ESP_INTR_FLAG_LEVEL3)
#define EXT_IO_READ_EVT_CORE           (0)
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




