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

 //这是一个已修改的文件,原作者信息见上方声明,在原程序基础上,
 //更改为项目需要的形式或设置
 //这个修改为适应硬件环境的一个项目需求, 不是否认原作者设计的可靠性
 //为了明确原作者信息,此文件API帮助及相关内容不在文档中显示

#include "audio_hal.h"
#include "driver/i2c.h"

//蓝牙配置
#define SEVETEST30_BLE_DEVICE_NAME               "SEVETEST30" //蓝牙设备名称
#define SEVETEST30_BLE_DEVICE_APPEARANCE_VALUE    0x0100      //蓝牙设备外观特征值(类别：0x004 外观特征值取值范围：0x0100 to 0x013F  0x0100-Generic Clock)
#define SEVETEST30_BLE_LOCAL_MTU                   100        //本地最大可传输单元MTU限制大小

//音频相关
#define FUNC_AUDIO_CODEC_EN       (true)
#define CODEC_DAC_I2S_PORT        (1)

#define CODEC_ADC_INPUT_MIC_ON_BOARD   ADC_INPUT_LINPUT1_RINPUT1 //使用电路板上的板载麦克风
#define CODEC_ADC_INPUT_MIC_HEADPHONE   ADC_INPUT_LINPUT2_RINPUT2 //使用连接的3.5mm耳机上带有的麦克风
#define CODEC_ADC_I2S_PORT        (0)
#define CODEC_ADC_BITS_PER_SAMPLE I2S_DATA_BIT_WIDTH_16BIT
#define CODEC_ADC_SAMPLE_RATE     (48000)

#define RECORD_HARDWARE_AEC       (false)
#define BOARD_PA_GAIN             (-30) /* Power amplifier gain defined by board (dB) */
#define PA_ENABLE_GPIO            -1 //SEVETEST30的功放使能只由board_ctrl控制

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

//I2C配置-默认为音频设备控制提供(以下定义被board_pins_config为ES8388提供的回调函数应用)
#define AUDIO_I2C_PORT      I2C_NUM_0
#define AUDIO_I2C_SDA_IO    GPIO_NUM_48
#define AUDIO_I2C_SCL_IO    GPIO_NUM_47
//(通信频率在ES8838.c固定100kHz)

//I2C配置-默认为其他设备控制提供
#define DEVICE_I2C_PORT      I2C_NUM_0
#define DEVICE_I2C_SDA_IO    GPIO_NUM_48
#define DEVICE_I2C_SCL_IO    GPIO_NUM_47
//SEVETEST30中设备与音频共用一个I2C_NUM_0,通信频率在ES8838.c固定100kHz
//为了使该设置在board_ctrl生效,需要使用board_ctrl的device_i2c_init()
#define DEVICE_I2C_DEFAULT_FREQ_HZ (100*1000) 


//I2S总线通信相关
#define I2S_MCK_IO GPIO_NUM_11;
#define I2S_BCK_IO GPIO_NUM_12;
#define I2S_WS_IO  GPIO_NUM_14;
#define I2S_DAC_DATA_IO GPIO_NUM_13;
#define I2S_ADC_DATA_IO GPIO_NUM_21;


//SPI总线通信相关
#define SPI_CS_IO   GPIO_NUM_0;
#define SPI_MOSI_IO GPIO_NUM_36;
#define SPI_MISO_IO GPIO_NUM_35;
#define SPI_SCLK_IO GPIO_NUM_37;

//字库芯片设置(挂载在SPI总线)
#define FONT_CHIP_SPI_ID SPI2_HOST
#define FONT_CHIP_SPI_FREQ (1 * 1000 * 1000)//SPI通讯频率(单位HZ)
#define FONT_CHIP_SPI_QUEUE_SIZE 7//SPI队列大小

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

//以下是针对SE30硬件的特殊部分的定义

//线性马达模块
#define VIBRA_IN1_IO GPIO_NUM_9  //线性马达模块的驱动信号输入1
#define VIBRA_IN2_IO GPIO_NUM_10 //线性马达模块的驱动信号输入2

//数字电位器(TPL0401B)-音量控制 
//寄存器设置值(step - 步数) = (AMP_VOL_MAX - [当前设置的音量]) * AMP_STEP_VOL
//[当前设置的音量]允许范围为0 - AMP_VOL_MAX
#define  AMP_DP_ADD    0x3E  //I2C地址
#define  AMP_DP_COMMAND 0x00 //操作命令    部分数字电位器操作需要一个固有命令，在寄存器设置的8位数据之前发送
#define  AMP_STEP_VOL  0x01  //单位步长度(每一个单位,改变那么多寄存器设置值)    由于可以设置的阻值范围是比较大的，而屏幕大小有限，为了方便用户调节，将DC音量能够识别到的电压范围对应的阻值范围映射到到0-VOL_MAX个单位，其中一个单位所对应的寄存器设置值(step-步数)为STEP_VOL
#define  AMP_VOL_MAX   100   //最大单位个数(最大音量值)  可以调整的最大单位个数,意味着音量有从0到AMP_VOL_MAX的那么多种选择
//数字电位器(TPL0401A)-辅助电压5V 下调控制
//寄存器设置值(step - 步数) = (BV_VOL_MAX - [当前设置的下调量]) * BV_STEP_VOL
//[当前设置的下调量]允许范围为0 - BV_VOL_MAX
#define  BV_DP_ADD    0x2E  //I2C地址    
#define  BV_DP_COMMAND 0x00 //操作命令     部分数字电位器操作需要一个固有命令，在寄存器设置的8位数据之前发送
#define  BV_STEP_VOL  0x01  //单位步长度(每一个单位,改变那么多寄存器设置值)   由于调压精度可以不用太高，为了方便调节，把寄存器设置值(step-步数)每下调STEP_VOL视为调压了一个单位
#define  BV_VOL_MAX 100     //最大单位个数(最大下调量) 可以调整的最大单位个数,意味着电压有从0到AMP_VOL_MAX的那么多种选择

//电池接入控制
#define BAT_IN_CTRL_IO   GPIO_NUM_2

// TCA6416A的中断信号输出
#define TCA6416A_IO_INT  GPIO_NUM_1

//姿态传感器 lsm6ds3trc 
#define IMU_FIFO_DEFAULT_READ_NUM 3//默认FIFO周期读取个数(默认方式)

//温湿度传感器 AHT21
#define AHT21_DEFAULT_MEASURE_DELAY 200//默认触发后进行读取操作间隔的延时,单位ms

//TCA6416A 控制IO端口次序名称定义
// 默认模式 0=输出模式 1=输入模式
// 默认关闭 闹钟中断 充电标识信号 红外发射 红外接收

#define SEVETEST30_TCA6416A_DEFAULT_CONFIG_MODE   {\  
.p00 = 1, \
.p01 = 0, \
.p02 = 1, \
.p03 = 0, \
.p04 = 0, \
.p05 = 0, \
.p06 = 0, \
.p07 = 0, \
.p10 = 0, \
.p11 = 1, \
.p12 = 1, \
.p13 = 1, \
.p14 = 1, \
.p15 = 0, \
.p16 = 0, \
.p17 = 1, \
.addr = 0, \
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
