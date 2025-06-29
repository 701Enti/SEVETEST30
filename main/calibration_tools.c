
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


#include "calibration_tools.h"
#include <string.h>
#include <sys/time.h>
#include "math_tools.h"
#include "math.h"
#include "esp_check.h"
#include "esp_log.h"
#include "hscdtd008a.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/projdefs.h"

static const char* TOP_TAG = "calibration_tools";

/// @brief 生成磁传感器校准模型
/// @param static_model 导入静态模型结果存储位置
/// @param sample_size 计算样本量,一定范围下,越高校准模型计算越精确,必须大于9(构造超定方程组),建议大于或等于50
/// @param sample_delay 采样间隔(单位ms),采集完成一个数据样本后,等待sample_delay(单位ms)时间后,继续采样,直到样本量达到sample_size时
/// @return [ESP_OK 生成成功]
/// @return [ESP_ERR_INVALID_ARG static_model = NULL / sample_size小于或等于0 / sample_size小于或等于9 / 无法进行传感器读取,通信参数设置错误]
/// @return [ESP_FAIL 发送传感器读取命令时发现问题,未应答]
/// @return [ESP_ERR_INVALID_STATE 运算时失败,进行椭球方程拟合时,矩阵UT * U(或者说XT * X)无法求逆,请检查数据有效性 / 无法进行传感器读取,I2C driver 未安装或没有运行在主机模式] 
/// @return [ESP_ERR_TIMEOUT 无法进行传感器读取,操作超时,因为总线忙]
/// @return [ESP_ERR_NO_MEM  内存不足] 
esp_err_t GS_calibration_static_model_generate(GS_calibration_static_model_t* static_model, int sample_size, int sample_delay) {
    const char* TAG = "GS_calibration_static_model_generate";

    //参数检查
    ESP_RETURN_ON_FALSE(static_model != NULL, ESP_ERR_INVALID_ARG, TOP_TAG, "输入了无法处理的空指针, 描述%s", esp_err_to_name(ESP_ERR_INVALID_ARG));
    ESP_RETURN_ON_FALSE(sample_size > 0, ESP_ERR_INVALID_ARG, TOP_TAG, "错误,因为参数sample_size小于或等于0,它必须大于9,建议大于或等于50, 描述%s", esp_err_to_name(ESP_ERR_INVALID_ARG));
    ESP_RETURN_ON_FALSE(sample_size > 9, ESP_ERR_INVALID_ARG, TOP_TAG, "错误,因为参数sample_size小于或等于9,它必须大于9,建议大于或等于50, 描述%s", esp_err_to_name(ESP_ERR_INVALID_ARG));

    //通知用户完成有效校准动作
    ESP_LOGI(TAG, "数据采集开始,现在请开始完成有效校准动作");

    //初始化缓存
    float p[9];//参数向量p,对应椭球方程中的参数,他们在数组p的对应顺序为{A,B,C,D,E,F,G,H,I},数学表达中对应顺序表示为p=[A,B,C,D,E,F,G,H,I]T (T代表转置)    
    float* U = (float*)heap_caps_aligned_alloc(16, sample_size * 9 * sizeof(float), MATH_TOOLS_MALLOC_CAP_DEFAULT); //设计矩阵U,行优先,每行输入数据对应[x^2,y^2,z^2,2xy,2xz,2yz,2x,2y,2z]
    float* y = (float*)heap_caps_aligned_alloc(16, sample_size * sizeof(float), MATH_TOOLS_MALLOC_CAP_DEFAULT);//观测向量y
    float* r = (float*)heap_caps_aligned_alloc(16, sample_size * sizeof(float), MATH_TOOLS_MALLOC_CAP_DEFAULT);//结果残差r
    if (!U || !y || !r) {
        heap_caps_free(U);
        heap_caps_free(y);
        heap_caps_free(r);
        U = NULL;
        y = NULL;
        r = NULL;
        ESP_RETURN_ON_FALSE(false, ESP_ERR_NO_MEM, TOP_TAG, "内存不足 描述%s", esp_err_to_name(ESP_ERR_NO_MEM));
    }
    //设置观测向量y为全1向量  
    for (int i = 0;i < sample_size;i++) {
        y[i] = 1.0f;
    }

    //读取传感器,收集数据,并填充到设计矩阵,准备进行椭球方程拟合  
    GS_output_data_t output;//原始数据
    GS_magnetic_flux_density_data_t mfd;//磁感应强度数据
    int index = 0;
    for (;;) {
        //获取磁感应强度数据
        esp_err_t ret = ESP_OK;
        ret = hscdtd008a_output_data_get(&output);//读取原始数据
        ESP_RETURN_ON_ERROR(ret, TOP_TAG, "读取传感器数据时发现问题 描述%s", esp_err_to_name(ret));
        ret = to_magnetic_flux_density_data(&output, &mfd);//转换为磁感应强度数据
        ESP_RETURN_ON_ERROR(ret, TOP_TAG, "将原始数据转换为磁感应强度数据时发现问题 描述%s", esp_err_to_name(ret));
        ESP_LOGI("MFD", "x-[%f] y-[%f] z-[%f]", mfd.Bx, mfd.By, mfd.Bz);
        //输入到构造矩阵
        U[index * 9 + 0] = mfd.Bx * mfd.Bx;//x ^ 2
        U[index * 9 + 1] = mfd.By * mfd.By;//y ^ 2
        U[index * 9 + 2] = mfd.Bz * mfd.Bz;//z ^ 2
        U[index * 9 + 3] = 2.0f * mfd.Bx * mfd.By;//2xy
        U[index * 9 + 4] = 2.0f * mfd.Bx * mfd.Bz; //2xz
        U[index * 9 + 5] = 2.0f * mfd.By * mfd.Bz; //2yz
        U[index * 9 + 6] = 2.0f * mfd.Bx; //2x
        U[index * 9 + 7] = 2.0f * mfd.By; //2y
        U[index * 9 + 8] = 2.0f * mfd.Bz; //2z

        if (index >= sample_size - 1) {
            break;//最后一个样本完成读取,退出
        }

        vTaskDelay(pdMS_TO_TICKS(sample_delay));
        index++;
    }

    //调用math_tools库API进行椭球方程拟合
    esp_err_t solve_ret = solve_overdet_system_ols_mlr(U, y, p, sample_size, 9);
    if (solve_ret != ESP_OK) {
        heap_caps_free(U);
        heap_caps_free(y);
        heap_caps_free(r);
        U = NULL;
        y = NULL;
        r = NULL;
        ESP_RETURN_ON_FALSE(false, solve_ret, TOP_TAG, "运算过程中发现异常,椭球方程拟合失败 描述%s", esp_err_to_name(solve_ret));
    }

    //拟合成功,进行结果品质评估(残差评估)
    float SSR = 0;//残差平方和
    appraisal_residual_linear_model(r, &SSR, U, y, p, sample_size, 9);

    ESP_LOGI(TAG, "椭球方程拟合成功");
    ESP_LOGI(TAG, "残差平方和SSR=%f", SSR);
    ESP_LOGI(TAG, "椭球参数A=%f,B=%f,C=%f,D=%f,E=%f,F=%f,G=%f,H=%f,I=%f", p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8]);

    //存储结果到static_model
    gettimeofday(&static_model->generate_time, NULL);//模型的生成时间 
    static_model->SSR = SSR;//椭球拟合残差平方和 
    //椭球参数
    static_model->A = p[0];
    static_model->B = p[1];
    static_model->C = p[2];
    static_model->D = p[3];
    static_model->E = p[4];
    static_model->F = p[5];
    static_model->G = p[6];
    static_model->H = p[7];
    static_model->I = p[8];
    //缩放量
    static_model->sx = 1 / sqrtf(static_model->A);
    static_model->sy = 1 / sqrtf(static_model->B);
    static_model->sz = 1 / sqrtf(static_model->C);
    //偏移量
    static_model->x0 = -(static_model->G / static_model->A);
    static_model->y0 = -(static_model->H / static_model->B);
    static_model->z0 = -(static_model->I / static_model->C);

    //释放内存,返回
    heap_caps_free(U);
    heap_caps_free(y);
    U = NULL;
    y = NULL;
    return ESP_OK;
}




