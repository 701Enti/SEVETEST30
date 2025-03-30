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

 // 包含一些sevetest30的  X轴线性振动马达启动以及振动马达事务合成API
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // github: https://github.com/701Enti
 // bilibili: 701Enti

#pragma once

#include "hal/gpio_types.h"
#include "driver/mcpwm.h"

//线性马达模块
//驱动信号输入引脚在board_def配置
// #define VIBRA_IN1_IO GPIO_NUM_X  //线性马达模块的驱动信号输入1
// #define VIBRA_IN2_IO GPIO_NUM_X //线性马达模块的驱动信号输入2
#define VIBRA_MOTOR_MCPWM_UNIT  MCPWM_UNIT_0 //选择MCPWM外设的UNIT
#define VIBRA_MOTOR_MCPWM_TIMER MCPWM_TIMER_0 //选择MCPWM外设的TIMER
#define VIBRA_MOTOR_MCPWM_DT_MODE MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE //死区模式
#define VIBRA_MOTOR_MCPWM_RED  1 //上升沿死区时间
#define VIBRA_MOTOR_MCPWM_FED  1 //下降沿死区时间


esp_err_t vibra_motor_init(gpio_num_t in1_gpio, gpio_num_t in2_gpio);

void vibra_motor_start();

void vibra_motor_stop();

void vibra_motor_set_frequency(uint32_t frequency);