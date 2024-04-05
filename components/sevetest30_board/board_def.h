/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2022 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#pragma once

#include "audio_hal.h"
#include "driver/i2c.h"
#include "esp_gatts_api.h"

//蓝牙配置
#define SEVETEST30_BLE_DEVICE_NAME               "SEVETEST30" //蓝牙设备名称
#define SEVETEST30_BLE_DEVICE_APPEARANCE_VALUE    0x0100      //蓝牙设备外观特征值(类别：0x004 外观特征值取值范围：0x0100 to 0x013F  0x0100-Generic Clock)
#define SEVETEST30_BLE_LOCAL_MTU                   100        //本地最大可传输单元MTU限制大小

//音频相关
#define FUNC_AUDIO_CODEC_EN       (true)
#define CODEC_ADC_I2S_PORT        (0)
#define CODEC_DAC_I2S_PORT        (1)
#define I2S_DMA_BUF_SIZE          (1024)
#define CODEC_ADC_BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT
#define CODEC_ADC_SAMPLE_RATE     (48000)
#define RECORD_HARDWARE_AEC       (false)
#define BOARD_PA_GAIN             (10) /* Power amplifier gain defined by board (dB) */
#define PA_ENABLE_GPIO            -1

#define FUNC_SDCARD_EN            (false)
#define SDCARD_OPEN_FILE_NUM_MAX  5
#define SDCARD_INTR_GPIO         -1
#define ESP_SD_PIN_CLK           -1
#define ESP_SD_PIN_CMD           -1
#define ESP_SD_PIN_D0            -1
#define ESP_SD_PIN_D1            -1
#define ESP_SD_PIN_D2            -1
#define ESP_SD_PIN_D3            -1
#define ESP_SD_PIN_D4            -1
#define ESP_SD_PIN_D5            -1
#define ESP_SD_PIN_D6            -1
#define ESP_SD_PIN_D7            -1
#define ESP_SD_PIN_CD            -1
#define ESP_SD_PIN_WP            -1

//默认为音频设备控制提供
#define AUDIO_I2C_PORT      I2C_NUM_0
#define AUDIO_I2C_SDA_IO    GPIO_NUM_48
#define AUDIO_I2C_SCL_IO    GPIO_NUM_47

//默认为其他设备控制提供
#define DEVICE_I2C_PORT      I2C_NUM_0
#define DEVICE_I2C_SDA_IO    GPIO_NUM_48
#define DEVICE_I2C_SCL_IO    GPIO_NUM_47
#define DEVICE_I2C_FREQ_HZ   (100 * 1000)

#define I2S_MCK_IO GPIO_NUM_11;
#define I2S_BCK_IO GPIO_NUM_12;
#define I2S_WS_IO  GPIO_NUM_14;
#define I2S_DAC_DATA_IO GPIO_NUM_13;
#define I2S_ADC_DATA_IO GPIO_NUM_21;

#define SPI_CS_IO   GPIO_NUM_0;
#define SPI_MOSI_IO GPIO_NUM_36;
#define SPI_MISO_IO GPIO_NUM_35;
#define SPI_SCLK_IO GPIO_NUM_37;
#define FONTS_CHIP_SPI_ID SPI2_HOST
#define FONT_SPI_FREQ (1 * 1000000)//SPI通讯频率(单位HZ)

#define AUDIO_CODEC_DEFAULT_CONFIG(){                   \
        .adc_input  = AUDIO_HAL_ADC_INPUT_LINE1,        \
        .dac_output = AUDIO_HAL_DAC_OUTPUT_ALL,         \
        .codec_mode = AUDIO_HAL_CODEC_MODE_BOTH,        \
        .i2s_iface = {                                  \
            .mode = AUDIO_HAL_MODE_SLAVE,               \
            .fmt = AUDIO_HAL_I2S_NORMAL,                \
            .samples = AUDIO_HAL_48K_SAMPLES,           \
            .bits = AUDIO_HAL_BIT_LENGTH_16BITS,        \
        },                                              \
};

//以下是针对SE30硬件的特殊部分的定义

//数字电位器-音量控制
#define  AMP_DP_ADD    0x3E  //I2C地址     数字电位器访问地址 这里用的是TPL0401B，它性价比足够高，可以尽量使用完全一样型号的数字电位器，因为这可能涉及通讯时是否需要命令的特殊问题，程序可能不兼容
#define  AMP_DP_COMMAND 0x00 //操作命令    部分数字电位器操作需要一个固有命令，在寄存器设置的8位数据之前发送，如TPL0401B需要0x00
#define  AMP_STEP_VOL  0x01  //单位步长度    由于可以设置的阻值范围是比较大的，而屏幕大小有限，为了方便用户调节，将DC音量能够识别到的电压范围对应的阻值范围缩小到0-VOL_MAX单位，其中一个单位所对应的寄存器设置值为STEP_VOL
#define  AMP_VOL_MAX   100   //总映射步数  由于可以设置的阻值范围是比较大的，而屏幕大小有限，为了方便用户调节，将DC音量能够识别到的电压范围对应的阻值范围缩小到0-VOL_MAX单位,需要修改则STEP_VOL也要改


//数字电位器-辅助电压5V 下调控制
#define  BV_DP_ADD    0x2E  //I2C地址     使用了TPL0401A
#define  BV_DP_COMMAND 0x00 //操作命令
#define  BV_STEP_VOL  0x01  //单位步长度
#define  BV_VOL_MAX 100      //总映射步数

//线性马达模块
#define VIBRA_IN1_IO GPIO_NUM_9
#define VIBRA_IN2_IO GPIO_NUM_10

//电池接入控制
#define BAT_IN_CTRL_IO   GPIO_NUM_2

// TCA6416A的中断信号输出
#define TCA6416A_IO_INT  GPIO_NUM_1

//姿态传感器默认FIFO周期读取个数(默认方式)
#define IMU_FIFO_DEFAULT_READ_NUM 3


//TCA6416A 控制IO端口次序名称定义
// 默认模式 0=输出模式 1=输入模式
// 默认关闭 闹钟中断 充电标识信号 红外发射 红外接收

#define SEVETEST30_TCA6416A_DEFAULT_CONFIG_MODE   {\  
    .p00 = 1,                           \
    .p01 = 0,                           \
    .p02 = 1,                           \
    .p03 = 0,                           \
    .p04 = 0,                           \
    .p05 = 0,                           \
    .p06 = 0,                           \
    .p07 = 0,                           \
    .p10 = 0,                           \
    .p11 = 1,                           \
    .p12 = 1,                           \
    .p13 = 1,                           \
    .p14 = 1,                           \
    .p15 = 0,                           \
    .p16 = 0,                           \
    .p17 = 0,                           \
    .addr=0,                            \
}

// 默认电平值
#define SEVETEST30_TCA6416A_DEFAULT_CONFIG_VALUE  {\
   .main_button=1,                      \
   .en_led_board=0,                     \
   .hp_detect=0,                        \
   .s2=1,                               \
   .s1=1,                               \
   .ir=0,                               \
   .s4=1,                               \
   .s3=1,                               \
   .amplifier_SD=1,                     \
   .IMU_INT=1,                          \
   .ALS_INT=1,                          \
   .thumbwheel_CCW=1,                   \
   .thumbwheel_CW=1,                    \
   .OTG_EN=1,                           \
   .charge_SIGN=1,                      \
   .ALARM_INT=1,                        \
   .addr=0,                             \
}
