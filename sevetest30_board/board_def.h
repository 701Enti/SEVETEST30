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

 //这是一个已修改的文件,非常感谢原作者!
 //在原程序基础上,更改为项目需要的数值设置,并添加一些项目个性化需要的定义
 //为了明确原作者信息,此文件API帮助及相关内容不在文档中显示

#pragma once

//音频相关
#define FUNC_AUDIO_CODEC_EN       (true)
#define CODEC_DAC_I2S_PORT        (1)

#define CODEC_ADC_INPUT_MIC_ON_BOARD   ADC_INPUT_LINPUT1_RINPUT1 //使用电路板上的板载麦克风
#define CODEC_ADC_INPUT_MIC_HEADPHONE   ADC_INPUT_LINPUT2_RINPUT2 //使用连接的3.5mm耳机上带有的麦克风
#define CODEC_ADC_I2S_PORT        (0)


//无用参数,不会生效
#define RECORD_HARDWARE_AEC       (false)
#define BOARD_PA_GAIN             (-30) /* Power amplifier gain defined by board (dB) */
#define PA_ENABLE_GPIO            0 //SEVETEST30的功放使能只由board_ctrl控制

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

//I2S总线通信相关
#define I2S_MCK_IO GPIO_NUM_11;
#define I2S_BCK_IO GPIO_NUM_12;
#define I2S_WS_IO  GPIO_NUM_14;
#define I2S_DAC_DATA_IO GPIO_NUM_13;
#define I2S_ADC_DATA_IO GPIO_NUM_21;

//I2C配置-默认为音频设备控制提供(以下定义被board_pins_config为ES8388提供的回调函数应用)
#define AUDIO_I2C_PORT      I2C_NUM_0
#define AUDIO_I2C_SDA_IO    GPIO_NUM_18
#define AUDIO_I2C_SCL_IO    GPIO_NUM_17

//音频编码芯片设置
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