
/*
 * 701Enti MIT License
 *
 * Copyright © 2026 <701Enti organization>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the “Software”),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include "board_def.h"

// 蓝牙配置
#define SEVETEST30_BLE_DEVICE_NAME "SEVETEST30" // 蓝牙设备名称
#define SEVETEST30_BLE_DEVICE_APPEARANCE_VALUE                                 \
  0x0100 // 蓝牙设备外观特征值(类别：0x004 外观特征值取值范围：0x0100 to 0x013F
         // 0x0100-Generic Clock)
#define SEVETEST30_BLE_LOCAL_MTU 100 // 本地最大可传输单元MTU限制大小

// I2C配置-默认为其他设备控制提供
#define DEVICE_I2C_PORT (AUDIO_I2C_PORT)
#define DEVICE_I2C_SDA_IO GPIO_NUM_18
#define DEVICE_I2C_SCL_IO GPIO_NUM_17
#define DEVICE_I2C_CLK_SRC (I2C_CLK_SRC_DEFAULT)
#define DEVICE_I2C_DEFAULT_FREQ_HZ (100 * 1000)

// 以下是针对SE30硬件的特殊部分的定义

// LedArray灯板阵列相关定义

#define LINE_LED_NUMBER 32     // 灯板横向(长边)LED灯数目，必须是8的倍数
#define VERTICAL_LED_NUMBER 16 // 灯板纵向(短边)LED灯数目，必须是2的倍数

#define LEDARRAY_SPI_ID SPI3_HOST
#define LEDARRAY_SPI_FREQ (10 * 1000 * 1000) // SPI通讯频率(单位HZ)

#define LEDARRAY_SPI_MOSI_IO GPIO_NUM_4 // ICND2013驱动-数据SIN
#define LEDARRAY_SPI_SCLK_IO GPIO_NUM_5 // ICND2013驱动-时钟CLK
#define LEDARRAY_LE_IO GPIO_NUM_6       // ICND2013驱动-数据锁存LE
#define LEDARRAY_OE_IO GPIO_NUM_7       // ICND2013驱动-输出使能OE

#define LEDARRAY_CSE_IO GPIO_NUM_8   // ICND2013驱动-片选(两片接法)
#define LEDARRAY_CSA0_IO GPIO_NUM_2  // ICND2013驱动-地址线A0
#define LEDARRAY_CSA1_IO GPIO_NUM_44 // ICND2013驱动-地址线A1
#define LEDARRAY_CSA2_IO GPIO_NUM_42 // ICND2013驱动-地址线A2

// 字库芯片设置(挂载在SPI总线)
#define FONT_CHIP_SPI_ID SPI2_HOST
#define FONT_CHIP_SPI_FREQ (1 * 1000 * 1000) // SPI通讯频率(单位HZ)
#define FONT_CHIP_SPI_CS_IO GPIO_NUM_41;
#define FONT_CHIP_SPI_MOSI_IO GPIO_NUM_39;
#define FONT_CHIP_SPI_MISO_IO GPIO_NUM_38;
#define FONT_CHIP_SPI_SCLK_IO GPIO_NUM_40;

// 线性马达模块
#define VIBRA_IN1_IO GPIO_NUM_10 // 线性马达模块的驱动信号输入1
#define VIBRA_IN2_IO GPIO_NUM_9  // 线性马达模块的驱动信号输入2

// 数字电位器(TPL0401B)-音量控制
// 寄存器设置值(step - 步数) = (AMP_VOL_MAX - [当前设置的音量]) * AMP_STEP_VOL
//[当前设置的音量]允许范围为0 - AMP_VOL_MAX
#define AMP_STEP_VOL                                                           \
  0x01 // 单位步长度(每一个单位,改变那么多寄存器设置值)
       // 由于可以设置的阻值范围是比较大的，而屏幕大小有限，为了方便用户调节，将DC音量能够识别到的电压范围对应的阻值范围映射到到0-VOL_MAX个单位，其中一个单位所对应的寄存器设置值(step-步数)为STEP_VOL
#define AMP_VOL_MAX                                                            \
  100 // 最大单位个数(最大音量值)
      // 可以调整的最大单位个数,意味着音量有从0到AMP_VOL_MAX的那么多种选择

// TCA6416A 扩展IO芯片相关配置
#define TCA6416A_INT_IO GPIO_NUM_1

// 姿态传感器 LSM6DS3TRC
#define IMU_FIFO_DEFAULT_READ_NUM 3 // 默认FIFO周期读取个数(默认方式)

// 温湿度传感器 AHT20
#define AHT20_DEFAULT_MEASURE_DELAY                                            \
  200 // 默认触发后进行读取操作间隔的延时,单位ms

// TCA6416A 扩展IO芯片相关配置

#define SEVETEST30_TCA6416A_ADDR_PIN_LEVEL 0 // ADDR引脚电平，用于设置主机地址

// 默认模式配置，0=输出模式 1=输入模式（未写入默认为输入模式）
#define SEVETEST30_TCA6416A_DEFAULT_CONFIG_MODE                                \
  {                                                                            \
      \  
.p00 = 1,                                                                      \
      .p01 = 0,                                                                \
      .p02 = 1,                                                                \
      .p03 = 0,                                                                \
      .p04 = 1,                                                                \
      .p05 = 1,                                                                \
      .p06 = 0,                                                                \
      .p07 = 0,                                                                \
      .p10 = 1,                                                                \
      .p11 = 1,                                                                \
      .p12 = 1,                                                                \
      .p13 = 1,                                                                \
      .p14 = 1,                                                                \
      .p15 = 0,                                                                \
      .p16 = 1,                                                                \
      .p17 = 1,                                                                \
  }

// 默认电平值( 0=低电平 1=高电平)
#define SEVETEST30_TCA6416A_DEFAULT_CONFIG_VALUE                               \
  {                                                                            \
      .QC_TOUCH_L = 0,                                                         \
      .DISABLE_LED_BOARD = 1,                                                  \
      .HP_DETECT = 0,                                                          \
      .BAT_QSTRT = 0,                                                          \
      .BAT_ALRT = 1,                                                           \
      .EMF_DRDY = 0,                                                           \
      .amplifier_MUTE = 1,                                                     \
      .amplifier_SD = 1,                                                       \
      .IMU_INT2 = 1,                                                           \
      .IMU_INT1 = 1,                                                           \
      .ALS_INT = 1,                                                            \
      .thumbwheel_CCW = 1,                                                     \
      .thumbwheel_CW = 1,                                                      \
      .OTG_EN = 0,                                                             \
      .charge_SIGN = 1,                                                        \
      .QC_TOUCH_R = 0,                                                         \
  }