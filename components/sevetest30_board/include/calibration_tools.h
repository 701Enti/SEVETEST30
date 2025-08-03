
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

#include "freertos/FreeRTOS.h"
#include "hscdtd008a.h"
#include "stdbool.h"
#include "esp_err.h"

#define VERSION_OF_GS_CALIBRATION_STATIC_MODEL_T 1

#define MALLOC_CAP_DEFAULT_CALIBRATION_TOOLS MALLOC_CAP_SPIRAM //默认内存申请位置
#define MESSAGE_QUEUE_LENGTH_PRODUCER_PSP2P_DM_CALIBRATION_TOOLS 20 //PsP2P生产者节点消息队列长度

/// @brief 地磁传感器静态校准模型(解决固有误差问题,会被存储到Flash,反复使用)
typedef struct GS_static_calibration_model_t {

    float SSR;//椭球拟合残差平方和 
    float A, B, C, D, E, F, G, H, I;//椭球参数
    float sx, sy, sz;//缩放量
    float x0, y0, z0;//偏移量

    union {
        //以下两个模型生成时间根据具体情况二择一使用,并通过generate_time_is_MT标注选择(仅仅只是标识注释,不是选择参数),其为true是表示单调时间,为false是表示实时时间
        struct timeval generate_time_RTC;//模型的生成时间(系统微秒级实时时间) - (默认使用-在保证NTP时间同步完成,系统时间与现实时间一致情况下)
        int64_t generate_time_MT;//模型的生成时间(系统微秒级单调时间) - (备用-在如网络未连接,无法进行NTP时间同步,系统时间与现实时间可能不一致情况下)        
    };
    bool generate_time_is_MT;//以上两个模型生成时间根据具体情况二择一使用,并通过generate_time_is_MT标注选择(仅仅只是标识注释,不是选择参数),其为true是表示单调时间,为false是表示实时时间

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

esp_err_t generate_GS_calibration_static_model(GS_calibration_static_model_t* static_model, int sample_size, int sample_delay);

esp_err_t calculate_calibrated_GS_only_by_static_model(const GS_calibration_static_model_t* static_model, GS_magnetic_flux_density_data_t* mfd);


void calibration_tools_init_PsP2P_DM_Producer();

esp_err_t put_GS_calibration_static_model(GS_calibration_static_model_t* static_model);











