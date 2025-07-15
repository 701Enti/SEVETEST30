
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

 // 包含各种传感器校准工作
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // github: https://github.com/701Enti
 // bilibili: 701Enti

#pragma once

#include "esp_err.h"


#define MALLOC_CAP_DEFAULT_CALIBRATION_TOOLS MALLOC_CAP_SPIRAM //默认内存申请位置


/// @brief 地磁传感器静态校准模型(解决固有误差问题,会被存储到Flash,反复使用)
typedef struct GS_static_calibration_model_t {
    struct timeval generate_time;//模型的生成时间 
    float SSR;//椭球拟合残差平方和 
    float A, B, C, D, E, F, G, H, I;//椭球参数
    float sx, sy, sz;//缩放量
    float x0, y0, z0;//偏移量
}GS_calibration_static_model_t;
#define VERSION_OF_GS_CALIBRATION_STATIC_MODEL_T 1 //数据结构版本-地磁传感器静态校准模型



/// @brief 地磁传感器动态校准模型(解决动态误差问题,仅临时变量,不会被存储到Flash)
typedef struct GS_dynamic_calibration_model_t {
    float temperature_coeff;//温度系数
    float hard_iron_x;//x轴硬磁补偿
    float hard_iron_y;//y轴硬磁补偿
    float pitch;//俯仰角
    float roll;//横滚角
    float k_omega;//陀螺仪补偿
}GS_dynamic_calibration_model_t;



typedef struct GS_calibration_t {
    GS_calibration_static_model_t static_model;//静态校准模型
    GS_dynamic_calibration_model_t dynamic_model;//动态校准模型

}GS_calibration_t;

esp_err_t GS_calibration_static_model_generate(GS_calibration_static_model_t* static_model, int sample_size, int sample_delay);










