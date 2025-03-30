
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

 // 包含各种SE30对外部RTC芯片 BL5372的支持
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // 敬告：文件本体不包含i2c通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行
 // github: https://github.com/701Enti
 // bilibili: 701Enti

#pragma once

#include "esp_types.h"

// I2C相关配置宏定义在board_def.h下
#define BL5372_DEVICE_ADD 0x32

// BL5372内部寄存器地址表，请通过BL5372数据手册了解这些寄存器功能细节
enum
{
    BL5372_REG_TIME_SEC = 0x00,
    BL5372_REG_TIME_MIN = 0x01,
    BL5372_REG_TIME_HOUR,
    BL5372_REG_TIME_WEEK,
    BL5372_REG_TIME_DAY,
    BL5372_REG_TIME_MON,
    BL5372_REG_TIME_YEAR,
    BL5372_REG_CTRL_COMPENSATE,
    BL5372_REG_ALARM_A_MIN,
    BL5372_REG_ALARM_A_HOUR,
    BL5372_REG_ALARM_A_PLAN,//闹钟A循环计划
    BL5372_REG_ALARM_B_MIN,
    BL5372_REG_ALARM_B_HOUR,
    BL5372_REG_ALARM_B_PLAN,//闹钟B循环计划
    BL5372_REG_CTRL_1,
    BL5372_REG_CTRL_2,
};

// BL5372闹钟选择
typedef enum
{
    BL5372_ALARM_A = 0,
    BL5372_ALARM_B = 1,
} BL5372_alarm_select_t;

// 中断相关引脚的输出模式 SL2 SL1
typedef enum
{
    BL5372_INTR_OUT_MODE0 = 0x00, // 闹钟A,闹钟B，周期性中断都从INTRA引脚输出
    BL5372_INTR_OUT_MODE1 = 0x01, // INTRA引脚输出 闹钟A 周期性中断 /// INTRB引脚输出 闹钟B 32KHz时钟脉冲
    BL5372_INTR_OUT_MODE2,        // INTRA引脚输出 闹钟A,闹钟B     ///  INTRB引脚输出 周期性中断 32KHz时钟脉冲
    BL5372_INTR_OUT_MODE3,        // INTRA引脚输出 闹钟A          ///  INTRB引脚输出 闹钟A,闹钟B 周期性中断 32KHz时钟脉冲
} BL5372_intr_out_mode_t;

// 周期性中断(INT)模块的模式 CT2 CT1 CT0
typedef enum
{
    BL5372_INT_MOD_MODE0 = 0x00, // 周期性中断接入的引脚为高电平(不输出)
    BL5372_INT_MOD_MODE1 = 0x01, // 周期性中断接入的引脚为低电平
    BL5372_INT_MOD_MODE2,        // 周期性中断接入的引脚输出2Hz占空比50%的脉冲(脉冲模式)
    BL5372_INT_MOD_MODE3,        // 周期性中断接入的引脚输出1Hz占空比50%的脉冲(脉冲模式)
    BL5372_INT_MOD_MODE4,        // 当 秒 进位 周期性中断接入的引脚输出低电平
    BL5372_INT_MOD_MODE5,        // 当 分 进位 周期性中断接入的引脚输出低电平
    BL5372_INT_MOD_MODE6,        // 当 时 进位 周期性中断接入的引脚输出低电平
    BL5372_INT_MOD_MODE7,        // 当 月 进位 周期性中断接入的引脚输出低电平
} BL5372_INT_module_mode_t;

// 运行配置
typedef struct BL5372_cfg_t
{
    //----------------控制寄存器1  BL5372_REG_CTRL_1
    // D7-D0
    bool alarm_A_en;                          // 设置为true将使能闹钟A AALE
    bool alarm_B_en;                          // 设置为true将使能闹钟B BALE
    BL5372_intr_out_mode_t intr_out_mode;     // 中断输出模式,请查看BL5372_intr_out_mode_t  SL2 SL1
    bool test_en;                             // 设置为true进入测试模式 TEST
    BL5372_INT_module_mode_t INT_module_mode; // 周期性中断模块的模式，请查看BL5372_INT_module_mode_t CT2 CT1 CT0

    //----------------控制寄存器2  BL5372_REG_CTRL_2
    // D5-D0
    bool hour_24_clock_en; // 设置为true进行24小时制记时 12/24

    // 写配置(只写ADJ)：设置为true启动正负30秒调整工作(秒复位00，如果原始秒处于30-59，分进一位),设置为flase表示希望进行正常工作
    // 读配置(只读XSTP)：如果读出的值为true,RTC芯片处于停振检测标志，无法保证时间数据准确,但可能在正常走时
    // 如果处于停振检测状态，XSL_ F6-F0 CT0-CT2  AALE BALE SL1-SL2 CLEN_ TEST 位复位为0 INTRA停止输出 INTRB输出32KHz脉冲
    bool adj_en_or_xstp; // ADJ --- XSTP

    bool out_32KHz_false; // 设置为true禁止32KHz脉冲输出 CLEN_

    // 读配置(读写CTFG)：读出为true表示周期性中断输出进行中
    // 写配置(读写CTFG)：设置为true允许保持现在的周期性中断输出(中断时默认为true,非中断设置为true无任何变化)
    //                  设置为false关闭现在的周期性中断输出(一次中断仅一次操作，不可逆)
    bool INT_out_flag_or_out_keep; // CTFG

    // 读配置(读写AAFG)：读出为true表示闹铃中断输出进行中
    // 写配置(读写AAFG)：设置为true允许保持现在的闹铃中断输出(中断时默认为true,非中断为false设置为true无任何变化)
    //                  设置为false关闭现在的闹铃中断输出(一次中断仅一次操作，不可逆)
    bool alarm_A_out_flag_or_out_keep; // AAFG

    bool alarm_B_out_flag_or_out_keep; // BAFG 与alarm_A_out_flag_or_out_en相同但这是针对闹钟B的，读写BAFG

} BL5372_cfg_t;

// BL5372的时间数据，以常规时间的数值，24h制/12h制 视配置而定
typedef struct BL5372_time_t
{
    int year; /// 舍去前两位，如2024年存储24
    int month;
    int day;
    int week;//0-6，指代星期日，星期一到星期五，星期六
    int hour;
    int minute;
    int second;
} BL5372_time_t;

// BL5372的闹钟周期计划数据，请勿修改其中成员的先后顺序
typedef struct BL5372_alarm_cycle_plan_t
{
    //高位到低位 D6 - D0
    bool Saturday_en;  // 设置为true星期六会响铃
    bool Friday_en;    // 设置为true星期五会响铃
    bool Thursday_en;  // 设置为true星期四会响铃
    bool Wednesday_en; // 设置为true星期三会响铃
    bool Tuesday_en;   // 设置为true星期二会响铃
    bool Monday_en;    // 设置为true星期一会响铃
    bool Sunday_en;    // 设置为true星期日会响铃
}BL5372_alarm_cycle_plan_t;

// 默认初始化配置
#define BL5372_DEFAULT_INIT_CONFIG             { \
        .alarm_A_en = false,                     \
        .alarm_B_en = false,                     \
        .intr_out_mode = BL5372_INTR_OUT_MODE2,  \
        .test_en = false,                        \
        .INT_module_mode = BL5372_INT_MOD_MODE0, \
        .hour_24_clock_en = true,                \
        .adj_en_or_xstp = false,                 \
        .out_32KHz_false = true,                 \
        .INT_out_flag_or_out_keep = false,       \
        .alarm_A_out_flag_or_out_keep = false,   \
        .alarm_B_out_flag_or_out_keep = false,   \
    }

esp_err_t BL5372_config_init();

void BL5372_time_now_set(BL5372_time_t* time);

void BL5372_time_now_get(BL5372_time_t* time);

void BL5372_time_alarm_set(BL5372_alarm_select_t alarm, BL5372_time_t* time, BL5372_alarm_cycle_plan_t* cycle_plan);

void BL5372_time_alarm_get(BL5372_alarm_select_t alarm, BL5372_time_t* time, BL5372_alarm_cycle_plan_t* cycle_plan);

void BL5372_alarm_status_set(BL5372_alarm_select_t alarm, bool status);

bool BL5372_alarm_status_get(BL5372_alarm_select_t alarm);

bool BL5372_alarm_is_ringing(BL5372_alarm_select_t alarm);

void BL5372_alarm_stop_ringing(BL5372_alarm_select_t alarm);

esp_err_t BL5372_config_set(BL5372_cfg_t* rtc_cfg);

esp_err_t BL5372_config_get(BL5372_cfg_t* rtc_cfg);

uint8_t BCD8421_transform_recode(uint8_t bin_dat);

uint8_t BCD8421_transform_decode(uint8_t dec_dat);