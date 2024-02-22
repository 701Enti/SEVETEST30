// 该文件归属701Enti组织，由SEVETEST30开发团队维护，包含各种SE30针对性硬件控制
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体不包含i2c通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行
///----注意：控制数据只有在完成sevetest30_board_ctrl工作之后，才会保存到board_ctrl_buf缓存中，如果果您只是外部定义了一个board_ctrl_t类型变量存储您的更改，
//     但是没有调用sevetest30_board_ctrl,board_ctrl_buf缓存数据将不会更新,而系统缓存的位置是board_ctrl_buf，而不是您自己定义的外部缓存，
///    意味着系统比如蓝牙读取，读到的数据将不是更新的数据，所以如果您要进行控制数据更改，务必保证sevetest30_board_ctrl工作进行了
///    如果只是单纯希望修改控制数据可以调用board_status_get直接获取系统缓存结构体指针进行修改，这样其他API读到数据将是更新的数据,但是只有sevetest30_board_ctrl被调用，硬件才会与控制数据同步
// github: https://github.com/701Enti
// bilibili: 701Enti

#pragma once

#include <string.h>
#include <stdbool.h>
#include "audio_hal.h"
#include "esxxx_common.h"
#include "board_def.h"
#include "sevetest30_gpio.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "esp_peripherals.h"

extern esp_periph_set_handle_t se30_periph_set_handle;

typedef struct board_device_handle_t
{
   spi_device_handle_t fonts_chip_handle;

} board_device_handle_t;

typedef enum
{
   BOARD_CTRL_ALL = 1,
   
   BOARD_CTRL_AMPLIFIER,
   BOARD_CTRL_BOOST,

   BOARD_CTRL_CODEC_MODE_AND_STATUS,//响应board_ctrl_t下的 codec_mode codec_audio_hal_ctrl 成员设定
   BOARD_CTRL_CODEC_DAC_PIN,//响应board_ctrl_t下的 codec_dac_pin 成员设定
   BOARD_CTRL_CODEC_DAC_VOL,//响应board_ctrl_t下的 codec_dac_volume成员设定
   BOARD_CTRL_CODEC_ADC_PIN,//响应board_ctrl_t下的 codec_adc_pin 成员设定
   BOARD_CTRL_CODEC_ADC_GAIN,//响应board_ctrl_t下的 codec_gain 成员设定

   BOARD_CTRL_EXT_IO,//根据p_ext_io_mode p_ext_io_value刷新外部IO,如果您没有在之前调整p_ext_io_mode p_ext_io_value的信息，这不会改变EXT_IO任何数值，但是会同步外部IO数据到这两个成员下
} board_ctrl_select_t;

//控制配置数据类型
///----注意：控制数据只有在完成sevetest30_board_ctrl工作之后，才会保存到board_ctrl_buf缓存中，如果果您只是外部定义了一个board_ctrl_t类型变量存储您的更改，
//     但是没有调用sevetest30_board_ctrl,board_ctrl_buf缓存数据将不会更新,而系统缓存的位置是board_ctrl_buf，而不是您自己定义的外部缓存，
///    意味着系统比如蓝牙读取，读到的数据将不是更新的数据，所以如果您要进行控制数据更改，务必保证sevetest30_board_ctrl工作进行了
///    如果只是单纯希望修改控制数据可以调用board_status_get直接获取系统缓存结构体指针进行修改，这样其他API读到数据将是更新的数据,但是只有sevetest30_board_ctrl被调用，硬件才会与控制数据同步
typedef struct board_ctrl_t
{
   TCA6416A_mode_t *p_ext_io_mode;   // 存储IO模式信息的结构体的地址，数据是保持的
   TCA6416A_value_t *p_ext_io_value; // 存储IO电平信息的结构体的地址，数据是保持的

   uint8_t amplifier_volume;         // 功放音量，取值为 0 - VOL_MAX（24）,等于 0 时将使得功放进入低功耗关断状态
   bool amplifier_mute;              // 功放静音使能，true/false

   uint8_t boost_voltage;            // 辅助电压，电压调整值 取值为 0 - (board_def.h中常量BV_VOL_MAX的值(24))

   audio_hal_codec_mode_t codec_mode;//音频编解码器模式
   audio_hal_ctrl_t codec_audio_hal_ctrl;//音频编解码器状态
   es_dac_output_t codec_dac_pin;//音频输出端选择
   int codec_dac_volume; //音频输出音量 0-100，如果设置为0,音频编解码器音频输出将静音
   es_mic_gain_t   codec_adc_gain;//麦克风增益
   es_adc_input_t  codec_adc_pin;//麦克风端选择
} board_ctrl_t;

void sevetest30_all_board_init(board_ctrl_t *board_ctrl, board_device_handle_t *board_device_handle);
void sevetest30_board_ctrl(board_ctrl_t *board_ctrl, board_ctrl_select_t ctrl_select);
void codechip_set(board_ctrl_t *board_ctrl);




/// @brief 外部函数获取控制数据缓存
///--------注意：控制数据只有在完成sevetest30_board_ctrl工作之后，才会保存到board_ctrl_buf缓存中，如果果您只是外部定义了一个board_ctrl_t类型变量存储您的更改，
//     但是没有调用sevetest30_board_ctrl,board_ctrl_buf缓存数据将不会更新,而系统缓存的位置是board_ctrl_buf，而不是您自己定义的外部缓存，
///    意味着系统比如蓝牙读取，读到的数据将不是更新的数据，所以如果您要进行控制数据更改，务必保证sevetest30_board_ctrl工作进行了
///    如果只是单纯希望修改控制数据可以调用board_status_get直接获取系统缓存结构体指针进行修改，这样其他API读到数据将是更新的数据,但是只有sevetest30_board_ctrl被调用，硬件才会与控制数据同步
/// @return 控制数据缓存结构体指针
board_ctrl_t *board_status_get();
