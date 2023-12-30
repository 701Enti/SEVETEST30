// 该文件由701Enti编写，包含各种SE30针对性硬件控制
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体不包含i2c通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.11.25

#pragma once

#include <string.h>
#include <stdbool.h>
#include "audio_hal.h"
#include "board_def.h"
#include "sevetest30_gpio.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "esp_peripherals.h"

extern esp_periph_set_handle_t se30_periph_set_handle;


typedef struct board_device_handle_t
{
   spi_device_handle_t fonts_chip_handle;

}board_device_handle_t;


typedef struct board_ctrl_t
{
   TCA6416A_mode_t* p_ext_io_mode;//存储IO模式信息的结构体的地址
   TCA6416A_value_t* p_ext_io_value;//存储IO电平信息的结构体的地址
   uint8_t amplifier_volume;//功放音量，取值为 0 - VOL_MAX（24）,等于 0 时将使得功放进入低功耗关断状态
   uint8_t boost_voltage;//辅助电压，电压调整值 取值为 0 - (board_def.h中常量BV_VOL_MAX的值(24))

}board_ctrl_t;


void sevetest30_all_board_init(board_ctrl_t* board_ctrl,board_device_handle_t* board_device_handle);

void codechip_set(audio_hal_codec_mode_t mode, audio_hal_ctrl_t audio_hal_ctrl);

void amplifier_set(board_ctrl_t* board_ctrl);

void boost_voltage_set(board_ctrl_t* board_ctrl);


