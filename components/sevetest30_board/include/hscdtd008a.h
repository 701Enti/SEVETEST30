
/*
 * 701Enti MIT License
 *
 * Copyright © 2025 <701Enti organization>
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

 // 包含各种SE30对地磁传感器HSCDTD008A的访问与控制
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // github: https://github.com/701Enti
 // bilibili: 701Enti

#pragma once

#include "esp_err.h"

// I2C相关配置宏定义在board_def.h下
#define HSCDTD008A_DEVICE_ADDRESS 0x0C //设备地址
#define HSCDTD008A_1LSB_MAGNETIC_FLUX_DENSITY 0.15 //输出数据的一个LSB代表的磁感应强度(单位: uT)

//寄存器地址库
enum
{
    SELFTEST_RESPONSE = 0x0C,
    MORE_INFO_VERSION,
    MORE_INFO_ALPS,
    WHO_I_AM,
    OUTPUT_X_LSB,
    OUTPUT_X_MSB,
    OUTPUT_Y_LSB,
    OUTPUT_Y_MSB,
    OUTPUT_Z_LSB,
    OUTPUT_Z_MSB,
    STATUS = 0x18,
    FIFO_POINTER_STATUS,
    CONTROL_1 = 0x1B,
    CONTROL_2,
    CONTROL_3,
    CONTROL_4,
    OFFSET_X_LSB = 0x20,
    OFFSET_X_MSB,
    OFFSET_Y_LSB,
    OFFSET_Y_MSB,
    OFFSET_Z_LSB,
    OFFSET_Z_MSB,
    JTHR_L,
    JTHR_H,
    TEMPERATURE_VALUE = 0x31,
};

//模式( = CTRL1.PC )
typedef enum {
    GS_MODE_STAND_BY = 0,//(上电默认)低功耗待机,限制control3(CTRL3)寄存器控制位写入(包括自检启动[STC],温度测量启动[TCS],Force State下的手动触发[FRC]([FORCE]),详见手册),但不限制任何读取
    GS_MODE_ACTIVE,//活跃,无限制读写,可以开始测量,可切换state选择测量方式(上电默认,需要外部命令来手动触发测量)
    // GS_MODE_OFF,(掉电模式,此处不提供支持的模式)    
} GS_mode_t;

//状态(在Active Mode下)( = CTRL1.FS )
typedef enum {
    GS_STATE_NORMAL = 0,//会自动根据配置触发测量
    GS_STATE_FORCE,//(上电默认)需要设置control3(CTRL3)寄存器控制位STC来手动触发测量
} GS_state_t;

//数据量程( = CTRL4.RS )
typedef enum {
    GS_RANGE_OF_OUTPUT_14BIT = 0, //(上电默认) -8192LSB ~ +8191LSB ,1LSB = 0.15uT  (量程: -8192*0.15uT ~ +8191*0.15uT)
    GS_RANGE_OF_OUTPUT_15BIT, // -16384LSB ~ +16383LSB, 1LSB = 0.15uT (量程: -16384*0.15uT ~ +16383*0.15uT)
} GS_range_of_output_t;

//单个数据

//输出数据(未处理的源数据)
typedef struct GS_output_data_t
{
    GS_range_of_output_t range; //数据测量时的量程
    uint8_t raw_output_x_LSB;
    uint8_t raw_output_x_MSB;
    uint8_t raw_output_y_LSB;
    uint8_t raw_output_y_MSB;
    uint8_t raw_output_z_LSB;
    uint8_t raw_output_z_MSB;
} GS_output_data_t;

//磁感应强度数据(已处理的数据,单位: uT)
typedef struct GS_magnetic_flux_density_data_t
{
    GS_range_of_output_t range; //数据测量时的量程
    double Bx; //x轴磁感应强度,单位: uT
    double By; //y轴磁感应强度,单位: uT
    double Bz; //z轴磁感应强度,单位: uT
    // double B0; //磁感应强度的模,与方向无关,表示磁场总强度,单位: uT, B0 = sqrt(dest->Bx * dest->Bx + dest->By * dest->By + dest->Bz * dest->Bz);
} GS_magnetic_flux_density_data_t;

//角数据单位
typedef enum {
    GS_UNIT_OF_ANGLE_DEGREES = 0,//单位:角度    
    GS_UNIT_OF_ANGLE_RADIANS,//单位:弧度
} GS_unit_of_angle_data_t;

//角数据(已处理的数据,单位取决于成员unit)
typedef struct GS_angle_data_t
{
    GS_range_of_output_t range; //数据测量时的量程
    GS_unit_of_angle_data_t unit;//角数据单位
    double azimuth;//方位角,单位取决于成员unit,(空间的磁感应强度(这里看作向量)在x-y平面上的投影向量与x轴正半轴的夹角,根据这个数据,后期可以据此判断方向是东,南,西,北等方向)
    double pitch;//俯仰角,单位取决于成员unit,(空间的磁感应强度(这里看作向量)与x-y平面的夹角,后期可以据此判断俯仰姿态)
}GS_angle_data_t;




esp_err_t hscdtd008a_selftest();

esp_err_t hscdtd008a_mode_get(GS_mode_t* mode);
esp_err_t hscdtd008a_mode_set(GS_mode_t  mode);

esp_err_t hscdtd008a_state_get(GS_state_t* state);
esp_err_t hscdtd008a_state_set(GS_state_t  state);

esp_err_t hscdtd008a_output_data_get(GS_output_data_t* data);

esp_err_t to_magnetic_flux_density_data(GS_output_data_t* src, GS_magnetic_flux_density_data_t* dest);
esp_err_t to_angle_data(GS_unit_of_angle_data_t unit, GS_magnetic_flux_density_data_t* src, GS_angle_data_t* dest);