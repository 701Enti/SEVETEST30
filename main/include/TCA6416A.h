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

// 包含一些ESP32_S3通过硬件外设与TCA6416建立配置与扩展IO数据的通讯
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 本库特性：1 由于IO控制时，有随时需要调用TCA6416A写入函数的需求，本库不会出现调用一次函数归定只能改一个IO或读一个IO还要传一系列参数的尴尬问题，而是一齐读写,同时还会保存实时IO数据，因此没有用到电平反转寄存器
//          2 使用时直接修改公共变量以在项目非常方便使用，加之，可以像sevetest30_gpio.c封装后使用FreeRTOS支持，并添加中断支持，一但IO电平变化就读取，没有变就不读，客观上可以大大提高资源利用率
// 读写原理：   运用结构体地址一般为结构体中第一个成员变量地址，并且本例中，成员类型均为bool,地址递加从而可以方便地扫描所有成员，
// 敬告： 0 为更加方便后续开发或移植，本库不包含关于FreeRTOS支持的封装，公共变量修改方式的服务封装，以及中断服务的封装，如果需要参考，请参照sevetest30_gpio.c
//       1 文件本体不包含i2c通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行
//       2 请注意外部引脚模式设置，错误的配置可能导致您的设备损坏，我们不建议修改这些默认配置
//       3 对于设计现实的不同，您可以更改结构体成员变量名，但是必须确保对应的IO次序不变如 P00 P01 P02 P03 以此类推
//         同时成员变量名是上级程序识别操作引脚的关键，如果需要使用其上级程序而不仅仅是TCA6416A库函数，结构体成员变量名不应该随意修改，对当前硬件的更新必须修改上层代码
//       5.TCA6416A库不包含TCA6416A的INT和RESET引脚相关操作或实现,项目在sevetest30_gpio.c实现
// github: https://github.com/701Enti


#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "sevetest30_config.h"


#define TCA6416A_ADDR_PIN_LEVEL (SEVETEST30_TCA6416A_ADDR_PIN_LEVEL) //TCA6416A的ADDR引脚电平 0 / 1，用于设置主机地址
#define TCA6416A_DEVICE_ADD     (0x20 | (TCA6416A_ADDR_PIN_LEVEL))//TCA6416A通讯地址
#define TCA6416A_I2C_PORT    (DEVICE_I2C_PORT)//TCA6416A通讯端口
#define TCA6416A_I2C_FREQ_HZ 10*1000 //TCA6416A通讯频率
#define TCA6416A_I2C_TIMEOUT_MS 1000 //TCA6416A通讯超时时间(单位ms)

#define TCA6416A_DEFAULT_CONFIG_MODE SEVETEST30_TCA6416A_DEFAULT_CONFIG_MODE //TCA6416A的默认配置模式，0=输出模式 1=输入模式
#define TCA6416A_DEFAULT_CONFIG_VALUE SEVETEST30_TCA6416A_DEFAULT_CONFIG_VALUE //TCA6416A的默认引脚电平值，0=低电平 1=高电平


// 两个输入值寄存器，只读有效
#define TCA6416A_IN1 0x00
#define TCA6416A_IN2 0x01
// 两个输出值寄存器
#define TCA6416A_OUT1 0x02
#define TCA6416A_OUT2 0x03
// 两个极性反转设置寄存器
#define TCA6416A_PI1 0x04
#define TCA6416A_PI2 0x05
// 两个模式寄存器，置0使能输出，用于初始化工作
#define TCA6416A_MODE1 0x06
#define TCA6416A_MODE2 0x07

typedef struct TCA6416A_mode_t // 模式配置，0=输出模式 1=输入模式（未写入默认为输入模式）
{
    bool p00;
    bool p01;
    bool p02;
    bool p03;
    bool p04;
    bool p05;
    bool p06;
    bool p07;
    bool p10;
    bool p11;
    bool p12;
    bool p13;
    bool p14;
    bool p15;
    bool p16;
    bool p17;
} TCA6416A_mode_t;

// 对于设计现实的不同，您可以更改结构体成员变量名，独立应用于您的程序
// 但是必须确保定义时成员对应的IO次序不变，定义顺序必须为引脚顺序p00-p17
typedef struct TCA6416A_level_t
{

    bool QC_TOUCH_L;     // 输入,左触摸信号,高电平表示触摸,无锁
    bool EN_LED_BOARD;   // 输出,高电平将使得灯板关闭
    bool HP_DETECT;      // 输入,耳机已插入信号 高电平表示检测到耳机插入
    bool BAT_QSTRT;      // 输出,电量计量QSTRT快速启动信号
    bool BAT_ALRT;       // 输入,电量计量ALRT提醒信号
    bool EMF_DRDY;       // 输入,地磁传感器DRDY信号
    bool amplifier_MUTE; // 输出,功放静音   高电平静音
    bool amplifier_SD;   // 输出,功放使能   高电平使能
    bool IMU_INT2;       // 输入,姿态传感器中断信号2
    bool IMU_INT1;       // 输入,姿态传感器中断信号1
    bool ALS_INT;        // 输入,环境光传感器-中断信号
    bool thumbwheel_CW; // 输入,拨轮开关-0表示逆时针转动 （CCW和CW同时0表示按下-PUSH）
    bool thumbwheel_CCW;  // 输入,拨轮开关-0表示顺时针转动 （CCW和CW同时0表示按下-PUSH）
    bool OTG_EN;         // 输出,OTG电源供应 1启动 启动后SE30充电活动将被硬件性禁止
    bool charge_SIGN;    // 输入,正在充电信号 0表示正在充电
    bool QC_TOUCH_R;     // 输入,右触摸信号,高电平表示触摸,无锁
} TCA6416A_level_t;

esp_err_t TCA6416A_init();

esp_err_t TCA6416A_gpio_mode_set(TCA6416A_mode_t *pTCA6416Amode);

esp_err_t TCA6416A_gpio_level_service(TCA6416A_level_t *pTCA6416Avalue,TCA6416A_mode_t *pTCA6416Amode);

esp_err_t TCA6416A_gpio_global_inversion_set(bool inversion);
