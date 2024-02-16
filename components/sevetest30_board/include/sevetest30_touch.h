// 该文件由701Enti编写，包含一些sevetest30的  X轴线性振动马达启动以及振动马达事务合成API
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.12.17

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