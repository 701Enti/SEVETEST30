
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

 // 包含各种SE30针对性硬件控制
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // 敬告：文件包含 DEVICE_I2C_PORT i2c通讯的初始化配置,需要调用device_i2c_init(),此处音频和其他设备共用端口，在audio_board_init()初始化，不需初始化
 //     API规范: sevetest30_board_ctrl :  外部函数应该调用board_status_get获取控制缓存变量,修改值后导入
 ///----注意：控制数据只有在完成sevetest30_board_ctrl工作之后，才会保存到board_ctrl_buf缓存中，如果果您只是外部定义了一个board_ctrl_t类型变量存储您的更改，
 //     但是没有调用sevetest30_board_ctrl,board_ctrl_buf缓存数据将不会更新,而系统缓存的位置是board_ctrl_buf，而不是您自己定义的外部缓存，
 ///    意味着系统比如蓝牙读取，读到的数据将不是更新的数据，所以如果您要进行控制数据更改，务必保证sevetest30_board_ctrl工作进行了
 ///    如果只是单纯希望修改控制数据可以调用board_status_get直接获取系统缓存结构体指针进行修改，这样其他API读到数据将是更新的数据,但是只有sevetest30_board_ctrl被调用，硬件才会与控制数据同步
 // github: https://github.com/701Enti
 // bilibili: 701Enti

#pragma once

#include "esp_types.h"
#include <string.h>
#include "board_def.h"
#include "audio_hal.h"
#include "esxxx_common.h"
#include "sevetest30_gpio.h"
#include "driver/i2c.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "esp_peripherals.h"
#include "BL5372.h"


extern esp_periph_set_handle_t se30_periph_set_handle;


typedef enum
{
   BOARD_CTRL_ALL = 1,//对结构体存储的参数全部生效,包括没有在结构体初始化后修改值的存储参数

   BOARD_CTRL_DEVICE_I2C,//设备I2C通讯相关(设备I2C端口号在board_def.h指定)

   BOARD_CTRL_AMPLIFIER,//音频功率放大器相关
   BOARD_CTRL_BOOST,//5V辅助电源调整

   BOARD_CTRL_CODEC_MODE_AND_STATUS,//响应board_ctrl_t下的 codec_mode codec_audio_hal_ctrl 成员设定
   BOARD_CTRL_CODEC_DAC_PIN,//响应board_ctrl_t下的 codec_dac_pin 成员设定
   BOARD_CTRL_CODEC_DAC_VOL,//响应board_ctrl_t下的 codec_dac_volume成员设定
   BOARD_CTRL_CODEC_ADC_PIN,//响应board_ctrl_t下的 codec_adc_pin 成员设定
   BOARD_CTRL_CODEC_ADC_GAIN,//响应board_ctrl_t下的 codec_gain 成员设定

   BOARD_CTRL_EXT_IO,//根据p_ext_io_mode p_ext_io_value刷新外部IO,如果您没有在之前调整p_ext_io_mode p_ext_io_value的信息，这不会改变EXT_IO任何数值，但是会同步外部IO数据到这两个成员下
} board_ctrl_select_t;

//控制配置数据类型
typedef struct board_ctrl_t
{

   i2c_config_t* p_i2c_device_config;   //设备I2C配置信息的地址,如果使用BOARD_CTRL_ALL,I2C配置在所有控制事务最前发生

   TCA6416A_mode_t* p_ext_io_mode;   // 存储IO模式信息的结构体的地址，数据是保持的
   TCA6416A_value_t* p_ext_io_value; // 存储IO电平信息的结构体的地址，数据是保持的

   uint8_t amplifier_volume;         // 功放音量，取值为 0 - (board_def.h中常量AMP_VOL_MAX的值),等于 0 时将使得功放进入低功耗关断状态
   bool amplifier_mute;              // 功放静音使能，true/false

   uint8_t boost_voltage;            // 辅助电压，电压调整值 取值为 0 - (board_def.h中常量BV_VOL_MAX的值)

   audio_hal_codec_mode_t codec_mode;//音频编解码器模式
   audio_hal_ctrl_t codec_audio_hal_ctrl;//音频编解码器状态
   es_dac_output_t codec_dac_pin;//音频输出端选择
   int codec_dac_volume; //音频输出音量 0-100，如果设置为0,音频编解码器音频输出将静音
   es_mic_gain_t   codec_adc_gain;//麦克风增益
   es_adc_input_t  codec_adc_pin;//麦克风端选择
} board_ctrl_t;


esp_err_t sevetest30_all_device_init(board_ctrl_t* board_ctrl);
void sevetest30_board_ctrl(board_ctrl_t* board_ctrl, board_ctrl_select_t ctrl_select);
void codechip_set(board_ctrl_t* board_ctrl);
esp_err_t device_i2c_init();



/// @brief 外部函数获取控制数据缓存
///--------注意：控制数据只有在完成sevetest30_board_ctrl工作之后，才会保存到board_ctrl_buf缓存中，如果果您只是外部定义了一个board_ctrl_t类型变量存储您的更改，
//     但是没有调用sevetest30_board_ctrl,board_ctrl_buf缓存数据将不会更新,而系统缓存的位置是board_ctrl_buf，而不是您自己定义的外部缓存，
///    意味着系统比如蓝牙读取，读到的数据将不是更新的数据，所以如果您要进行控制数据更改，务必保证sevetest30_board_ctrl工作进行了
///    如果只是单纯希望修改控制数据可以调用board_status_get直接获取系统缓存结构体指针进行修改，这样其他API读到数据将是更新的数据,但是只有sevetest30_board_ctrl被调用，硬件才会与控制数据同步
/// @return 控制数据缓存结构体指针
board_ctrl_t* board_status_get();
