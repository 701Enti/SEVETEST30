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

// 该文件归属701Enti组织，SEVETEST30开发团队应该提供责任性维护，包含一些sevetest30的  X轴线性振动马达启动以及振动马达事务合成API
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// github: https://github.com/701Enti
// bilibili: 701Enti

#pragma once

#include "hal/gpio_types.h"
#include "driver/mcpwm.h"

#define VIBRA_MOTOR_MCPWM_UNIT  MCPWM_UNIT_0
#define VIBRA_MOTOR_MCPWM_TIMER MCPWM_TIMER_0
#define VIBRA_MOTOR_MCPWM_DT_MODE MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE //死区模式
#define VIBRA_MOTOR_MCPWM_RED  1 //上升沿死区时间
#define VIBRA_MOTOR_MCPWM_FED  1 //下降沿死区时间


void vibra_motor_init(gpio_num_t in1_gpio,gpio_num_t in2_gpio);

void vibra_motor_start();

void vibra_motor_stop();

void vibra_motor_set_frequency(uint32_t frequency);