
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

// 该文件归属701Enti组织，主要由SEVETEST30开发团队维护，包含一些sevetest30的  X轴线性振动马达启动以及振动马达事务合成API
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// github: https://github.com/701Enti
// bilibili: 701Enti

#include "sevetest30_touch.h"
#include "board_pins_config.h"

/// @brief 初始化线性振动马达
/// @param in1_gpio 驱动信号1 GPIO
/// @param in2_gpio 驱动信号2 GPIO
void vibra_motor_init(gpio_num_t in1_gpio,gpio_num_t in2_gpio){
    mcpwm_config_t mcpwm_config;
    mcpwm_config.cmpr_a = 50.0;
    mcpwm_config.cmpr_b = 50.0; 
    mcpwm_config.duty_mode = MCPWM_DUTY_MODE_0;
    mcpwm_config.counter_mode = MCPWM_UP_COUNTER;
    mcpwm_config.frequency = 230;
    //驱动信号1引脚绑定
    mcpwm_gpio_init(VIBRA_MOTOR_MCPWM_UNIT,MCPWM0A,in1_gpio);
    //驱动信号2引脚绑定
    mcpwm_gpio_init(VIBRA_MOTOR_MCPWM_UNIT,MCPWM0B,in2_gpio);
    //初始化
    mcpwm_init(VIBRA_MOTOR_MCPWM_UNIT,VIBRA_MOTOR_MCPWM_TIMER,&mcpwm_config);
    //设置波形互补和死区时间
    mcpwm_deadtime_enable(VIBRA_MOTOR_MCPWM_UNIT,VIBRA_MOTOR_MCPWM_TIMER,VIBRA_MOTOR_MCPWM_DT_MODE,VIBRA_MOTOR_MCPWM_RED,VIBRA_MOTOR_MCPWM_FED);

    vibra_motor_stop();                                                                                                                                                                                                                                                                                                                                                                                                                                              
}

/// @brief 修改线性振动马达运行频率
/// @param frequency 频率(单位HZ)
void vibra_motor_set_frequency(uint32_t frequency){
    mcpwm_set_frequency(VIBRA_MOTOR_MCPWM_UNIT,VIBRA_MOTOR_MCPWM_TIMER,frequency);
}

/// @brief 启动线性振动马达
void vibra_motor_start(){
    mcpwm_start(VIBRA_MOTOR_MCPWM_UNIT,VIBRA_MOTOR_MCPWM_TIMER); 
}

/// @brief 暂停线性振动马达
void vibra_motor_stop(){
    mcpwm_stop(VIBRA_MOTOR_MCPWM_UNIT,VIBRA_MOTOR_MCPWM_TIMER);   
}
