
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

 // 包含各种基于ESP-DSP的数学工具函数
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // github: https://github.com/701Enti
 // bilibili: 701Enti

#include "math_tools.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_dsp.h"
#include "esp_cpu.h"
#include <stdbool.h>
#include <string.h>


const char* TOP_TAG = "math_tools";

/// @brief 求解超定方程组 - 基于普通最小二乘法(OLS)实现的多元线性回归(MLR)
/// @note  [注意]当用于普通线性回归,多项式回归等需要获取截距beta[0]的情况,设计向量左侧必须为全一列以获取beta[0];需要强制无截距时,或者有其他偏移决定(如椭球方程一次项)而无需显式beta[0]的情况,设计向量全部都是直接填充正常样本数据即可
/// @brief [适用问题]适用于求解m个形如 y = beta[0] + beta[1]*x[1] + beta[2]*x[2] + ... + beta[n]*x[n] (可以没有beta[0]) 的方程构成的超定方程组 X * beta = y (超定方程组即方程组满足m>n,但建议要有m>>n以避免过拟合)
/// @brief [适用领域]可由于传感器校准(椭球方程求逆等)和数据分析,机器学习等领域
/// @brief [运算原理] 对于超定方程组, 给定 X* beta = y, 最小二乘解即为 beta = (XT * X)^{-1} * XT * y, 其中T代表转置 , ^{-1}代表矩阵求逆(inverse)运算
/// @param X 设计矩阵 (m x n,行优先,必须有m>n,并且建议要有m>>n以避免过拟合,当需要获取截距beta[0]时,左侧必须为全一列以获取beta[0])
/// @param y 观测向量 (m x 1)
/// @param beta 输出参数向量 (n x 1) , 其结构和意义由设计向量决定
/// @param m 样本个数(即方程个数,行数) 
/// @param n 参数个数(列数,当需要获取截距beta[0]时,beta[0]占一个参数个数)
/// @param check_result 是否在运算结束后运行结果检查 
/// @return [ESP_OK 运算完成]  
/// @return [ESP_ERR_INVALID_ARG X为NULL / y为NULL / beta为NULL / m <= 0 / n <= 0] 
/// @return [ESP_ERR_INVALID_STATE 运算时失败,矩阵 XT * X 无法求逆]
/// @return [ESP_ERR_NOT_SUPPORTED m<=n 的方程组]
/// @return [ESP_ERR_NO_MEM  内存不足] 
esp_err_t solve_overdet_system_ols_mlr(const float* X, const float* y, float* beta, int m, int n) {
    const char* TAG = "solve_overdet_system_ols_mlr";

    //入参检查
    ESP_RETURN_ON_FALSE(X && y && beta && m <= 0 && n <= 0, ESP_ERR_INVALID_ARG, TOP_TAG, "传入参数异常 描述%s", esp_err_to_name(ESP_ERR_INVALID_ARG));
    ESP_RETURN_ON_FALSE(m > n, ESP_ERR_NOT_SUPPORTED, TOP_TAG, "不支持 m<=n 的方程组 描述%s", esp_err_to_name(ESP_ERR_NOT_SUPPORTED));

    // [运算原理] 对于超定方程组, 给定 X* beta = y, 最小二乘解即为 beta = (XT * X)^{-1} * XT * y, 其中T代表转置 , ^{-1}代表矩阵求逆(inverse)运算

    //申请内存
    float* XT = (float*)heap_caps_aligned_alloc(16, n * m * sizeof(float), MATH_TOOLS_MALLOC_CAP_DEFAULT); // XT
    float* XTy = (float*)heap_caps_aligned_alloc(16, n * sizeof(float), MATH_TOOLS_MALLOC_CAP_DEFAULT);// XT * y
    float* XTX = (float*)heap_caps_aligned_alloc(16, n * n * sizeof(float), MATH_TOOLS_MALLOC_CAP_DEFAULT);// XT * X
    float* inverse_XTX = (float*)heap_caps_aligned_alloc(16, n * n * sizeof(float), MATH_TOOLS_MALLOC_CAP_DEFAULT);// (XT * X)^{-1}

    //检查内存
    if (!XT || !XTy || !XTX || !inverse_XTX) {
        ESP_LOGE(TAG, "内存不足");
        heap_caps_free(XT);
        heap_caps_free(XTy);
        heap_caps_free(XTX);
        heap_caps_free(inverse_XTX);
        XT = NULL;
        XTy = NULL;
        XTX = NULL;
        inverse_XTX = NULL;
        return ESP_ERR_NO_MEM;
    }

    //内存初始化
    memset(XT, 0, n * m * sizeof(float));
    memset(XTy, 0, n * sizeof(float));
    memset(XTX, 0, n * n * sizeof(float));
    memset(inverse_XTX, 0, n * n * sizeof(float));

    //计算 XT (T代表转置)
    dspm_trans_f32(X, XT, m, n);

    //计算 XT * y (T代表转置)
    for (int i = 0;i < n;i++) {
        for (int j = 0;j < m;j++) {
            XTy[i] += XT[i * m + j] * y[j];
        }
    }

    //计算 XT * X (T代表转置)
    dspm_mult_f32(XT, X, XTX, n, m, n);

    //计算 (XT * X)^{-1} (T代表转置,^{-1}代表矩阵求逆)
    esp_err_t inv_ret = dspm_mat_inv_f32(XTX, inverse_XTX, n);
    if (inv_ret != ESP_OK) {
        ESP_LOGE(TAG, "运算时失败,矩阵 XT * X 无法求逆");
        heap_caps_free(XT);
        heap_caps_free(XTy);
        heap_caps_free(XTX);
        heap_caps_free(inverse_XTX);
        XT = NULL;
        XTy = NULL;
        XTX = NULL;
        inverse_XTX = NULL;
        return inv_ret;
    }

    //计算(XT * X) ^ -1 * XT * y , 其结果为beta
    dspm_mulc_f32(inverse_XTX, XTy, beta, n, n, 1);

    //释放内存
    heap_caps_free(XT);
    heap_caps_free(XTy);
    heap_caps_free(XTX);
    heap_caps_free(inverse_XTX);
    XT = NULL;
    XTy = NULL;
    XTX = NULL;
    inverse_XTX = NULL;

    //运算完成,返回 ESP_OK
    return ESP_OK;
}