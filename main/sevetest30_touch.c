// 该文件由701Enti编写，包含一些sevetest30的  X轴线性振动马达启动以及振动马达事务合成API
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.12.17

#include "sevetest30_touch.h"
#include "driver/mcpwm.h"
#include "board_pins_config.h"

void vibra_motor_init(gpio_num_t in1_gpio,gpio_num_t in2_gpio){
    mcpwm_config_t mcpwm_config;
    mcpwm_config.cmpr_a = 50.0;
    mcpwm_config.cmpr_b = 50.0; 
    mcpwm_config.duty_mode = MCPWM_DUTY_MODE_0;
    mcpwm_config.counter_mode = MCPWM_UP_COUNTER;
    mcpwm_config.frequency = 240;
    //初始化
    mcpwm_gpio_init(MCPWM_UNIT_0,MCPWM0A,in1_gpio);
    gpio_pad_select_gpio(in2_gpio);
    gpio_set_direction(in2_gpio,GPIO_MODE_OUTPUT);
    gpio_set_level(in2_gpio,0);
    //MCPWM_TIMER_0作用MCPWM0A
    mcpwm_init(MCPWM_UNIT_0,MCPWM_TIMER_0,&mcpwm_config);//在MCPWM_UNIT_0上启用MCPWM_TIMER_0
    mcpwm_set_signal_low(MCPWM_UNIT_0,MCPWM_TIMER_0,MCPWM0A);                                                                                                                                                                                                                                                                                                                                                                                                                                                 
}
