
/*
 * 701Enti MIT License
 *
 * Copyright © 2025 <701Enti organization>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the “Software”),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or put copies of the Software,
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
#include "math_tools_fp64.h"
#include "math.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "esp_check.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/projdefs.h"
#include "PsP2P_DM_for_idf.h"
#include "cJSON.h"

static const char* calibration_tools_TAG = __FILE__;
static const char* name_of_PsP2P_DM_Producer = __FILE__;
PsP2P_DM_node_handle_t PsP2P_DM_Producer = NULL;




/// @brief 生成磁传感器静态校准模型
/// @param static_model 导入静态校准模型结果存储位置
/// @param sample_size 计算样本量,一定范围下,越高校准模型计算越精确,必须大于9(构造超定方程组),建议大于或等于50
/// @param sample_delay 采样间隔(单位ms),采集完成一个数据样本后,等待sample_delay(单位ms)时间后,继续采样,直到样本量达到sample_size时
/// @return [ESP_OK 生成成功]
/// @return [ESP_ERR_INVALID_ARG static_model = NULL / sample_size小于或等于0 / sample_size小于或等于9 / 无法进行传感器读取,通信参数设置错误]
/// @return [ESP_FAIL 发送传感器读取命令时发现问题,未应答]
/// @return [ESP_ERR_INVALID_STATE 运算时失败,进行椭球方程拟合时,矩阵UT * U(或者说XT * X)无法求逆,请检查数据有效性 / 无法进行传感器读取,I2C driver 未安装或没有运行在主机模式] 
/// @return [ESP_ERR_TIMEOUT 无法进行传感器读取,操作超时,因为总线忙]
/// @return [ESP_ERR_NO_MEM  内存不足] 
esp_err_t generate_GS_calibration_static_model(GS_calibration_static_model_t* static_model, int sample_size, int sample_delay) {
    const char* TAG = "generate_GS_calibration_static_model";

    //参数检查
    ESP_RETURN_ON_FALSE(static_model != NULL, ESP_ERR_INVALID_ARG, calibration_tools_TAG, "输入了无法处理的空指针, 描述%s", esp_err_to_name(ESP_ERR_INVALID_ARG));
    ESP_RETURN_ON_FALSE(sample_size > 0, ESP_ERR_INVALID_ARG, calibration_tools_TAG, "错误,因为参数sample_size小于或等于0,它必须大于9,建议大于或等于50, 描述%s", esp_err_to_name(ESP_ERR_INVALID_ARG));
    ESP_RETURN_ON_FALSE(sample_size > 9, ESP_ERR_INVALID_ARG, calibration_tools_TAG, "错误,因为参数sample_size小于或等于9,它必须大于9,建议大于或等于50, 描述%s", esp_err_to_name(ESP_ERR_INVALID_ARG));

    //通知用户完成有效校准动作
    ESP_LOGI(TAG, "数据采集开始,现在请开始完成有效校准动作");

    //初始化缓存
    double p[9];//参数向量p,对应椭球方程中的参数,他们在数组p的对应顺序为{A,B,C,D,E,F,G,H,I},数学表达中对应顺序表示为p=[A,B,C,D,E,F,G,H,I]T (T代表转置)    
    double* U = (double*)heap_caps_aligned_alloc(16, sample_size * 9 * sizeof(double), MALLOC_CAP_DEFAULT_CALIBRATION_TOOLS); //设计矩阵U,行优先,每行输入数据对应[x^2,y^2,z^2,2xy,2xz,2yz,2x,2y,2z]
    double* y = (double*)heap_caps_aligned_alloc(16, sample_size * sizeof(double), MALLOC_CAP_DEFAULT_CALIBRATION_TOOLS);//观测向量y
    double* r = (double*)heap_caps_aligned_alloc(16, sample_size * sizeof(double), MALLOC_CAP_DEFAULT_CALIBRATION_TOOLS);//结果残差r
    if (!U || !y || !r) {
        heap_caps_free(U);
        heap_caps_free(y);
        heap_caps_free(r);
        U = NULL;
        y = NULL;
        r = NULL;
        ESP_RETURN_ON_FALSE(false, ESP_ERR_NO_MEM, calibration_tools_TAG, "内存不足 描述%s", esp_err_to_name(ESP_ERR_NO_MEM));
    }
    //设置观测向量y为全1向量  
    for (int i = 0;i < sample_size;i++) {
        y[i] = 1.0;
    }

    //读取传感器,收集数据,并填充到设计矩阵,准备进行椭球方程拟合  
    GS_output_data_t output;//原始数据
    GS_magnetic_flux_density_data_t mfd;//磁感应强度数据
    int index = 0;
    for (;;) {
        //获取磁感应强度数据
        esp_err_t ret = ESP_OK;
        ret = hscdtd008a_output_data_get(&output);//读取原始数据
        ESP_RETURN_ON_ERROR(ret, calibration_tools_TAG, "读取传感器数据时发现问题 描述%s", esp_err_to_name(ret));
        ret = to_magnetic_flux_density_data(&output, &mfd);//转换为磁感应强度数据
        ESP_RETURN_ON_ERROR(ret, calibration_tools_TAG, "将原始数据转换为磁感应强度数据时发现问题 描述%s", esp_err_to_name(ret));
        ESP_LOGI("MFD", "样本[%d]: x-[%f] y-[%f] z-[%f] | 进度: %.2f %c", index, mfd.Bx, mfd.By, mfd.Bz, ((double)(index + 1) / sample_size) * 100, '%');
        //输入到构造矩阵
        U[index * 9 + 0] = mfd.Bx * mfd.Bx;//x ^ 2
        U[index * 9 + 1] = mfd.By * mfd.By;//y ^ 2
        U[index * 9 + 2] = mfd.Bz * mfd.Bz;//z ^ 2
        U[index * 9 + 3] = 2.0 * mfd.Bx * mfd.By;//2xy
        U[index * 9 + 4] = 2.0 * mfd.Bx * mfd.Bz; //2xz
        U[index * 9 + 5] = 2.0 * mfd.By * mfd.Bz; //2yz
        U[index * 9 + 6] = 2.0 * mfd.Bx; //2x
        U[index * 9 + 7] = 2.0 * mfd.By; //2y
        U[index * 9 + 8] = 2.0 * mfd.Bz; //2z

        if (index >= sample_size - 1) {
            break;//最后一个样本完成读取,退出
        }

        vTaskDelay(pdMS_TO_TICKS(sample_delay));
        index++;
    }

    //调用math_tools库API进行椭球方程拟合
    esp_err_t solve_ret = solve_overdet_system_ols_mlr_fp64(U, y, p, sample_size, 9);
    if (solve_ret != ESP_OK) {
        heap_caps_free(U);
        heap_caps_free(y);
        heap_caps_free(r);
        U = NULL;
        y = NULL;
        r = NULL;
        ESP_RETURN_ON_FALSE(false, solve_ret, calibration_tools_TAG, "运算过程中发现异常,椭球方程拟合失败 描述%s", esp_err_to_name(solve_ret));
    }

    ESP_LOGI(TAG, "椭球方程拟合成功");
    ESP_LOGI(TAG, "椭球参数A=%f,B=%f,C=%f,D=%f,E=%f,F=%f,G=%f,H=%f,I=%f", p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8]);

    //拟合完成,进行数据基本合理性校验
    if (!isfinite(p[0]) || !isfinite(p[1]) || !isfinite(p[2])) {
        heap_caps_free(U);
        heap_caps_free(y);
        heap_caps_free(r);
        U = NULL;
        y = NULL;
        r = NULL;
        ESP_RETURN_ON_FALSE(false, ESP_FAIL, calibration_tools_TAG, "椭球方程拟合成功,但是运算结果不合理(存在非有限数),建议提升样本量已获得合理结果, 描述%s", esp_err_to_name(ESP_FAIL));
    }
    if (p[0] < 1e-8 || p[1] < 1e-8 || p[2] < 1e-8) {
        heap_caps_free(U);
        heap_caps_free(y);
        heap_caps_free(r);
        U = NULL;
        y = NULL;
        r = NULL;
        ESP_RETURN_ON_FALSE(false, ESP_FAIL, calibration_tools_TAG, "椭球方程拟合成功,但是运算结果不合理(A,B,C中存在小于1e-8的数,判定为存在非正数),建议提升样本量已获得合理结果, 描述%s", esp_err_to_name(ESP_FAIL));
    }

    //进行结果品质评估(残差评估)
    double SSR = 0;//残差平方和
    appraisal_residual_linear_model_fp64(r, &SSR, U, y, p, sample_size, 9);
    ESP_LOGI(TAG, "残差平方和SSR=%f", SSR);

    //存储结果到static_model

    //模型的生成时间  
    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
        //模型的生成时间(系统微秒级实时时间) - (默认使用-在保证NTP时间同步完成,系统时间与现实时间一致情况下)
        gettimeofday(&static_model->generate_time_RTC, NULL);
        static_model->generate_time_is_MT = false;
        ESP_LOGI(TAG, "系统NTP时间已同步,因此模型的生成时间使用 实时时间 RTC : [%lld s + %ld us]", static_model->generate_time_RTC.tv_sec, static_model->generate_time_RTC.tv_usec);
    }
    else {
        //模型的生成时间(系统微秒级单调时间) - (备用-在如网络未连接,无法进行NTP时间同步,系统时间与现实时间可能不一致情况下)
        static_model->generate_time_MT = esp_timer_get_time();
        static_model->generate_time_is_MT = true;
        ESP_LOGI(TAG, "系统NTP时间未同步,因此模型的生成时间使用 单调时间 MT : [%lld us]", static_model->generate_time_MT);
    }

    //椭球拟合残差平方和 
    static_model->SSR = SSR;

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

    ESP_LOGI(TAG, "结果已存储到static_model");
    ESP_LOGI(TAG, "缩放量 sx=%f,sy=%f,sz=%f", static_model->sx, static_model->sy, static_model->sz);
    ESP_LOGI(TAG, "偏移量 x0=%f,y0=%f,z0=%f", static_model->x0, static_model->y0, static_model->z0);

    //释放内存,返回
    heap_caps_free(U);
    heap_caps_free(y);
    U = NULL;
    y = NULL;
    return ESP_OK;
}

// esp_err_t calibration_tools_generate_GS_calibration_dynamic_model() {

// }


/// @brief 计算已校准的地磁传感器磁感应强度数据(仅通过静态校准模型)
/// @param static_model 输入静态校准模型
/// @param mfd 导入需要修改的磁感应强度数据
/// @return [ESP_OK 成功]
/// @return [ESP_ERR_INVALID_ARG 输入了无法处理的空指针]
esp_err_t calculate_calibrated_GS_only_by_static_model(const GS_calibration_static_model_t* static_model, GS_magnetic_flux_density_data_t* mfd) {
    ESP_RETURN_ON_FALSE(static_model && mfd, ESP_ERR_INVALID_ARG, calibration_tools_TAG, "输入了无法处理的空指针 描述 %s", esp_err_to_name(ESP_ERR_INVALID_ARG));

    //临时变量
    const float x = mfd->Bx - static_model->x0;
    const float y = mfd->By - static_model->y0;
    const float z = mfd->Bz - static_model->z0;

    //计算校准后数据,直接计算,不引入矩阵运算
    mfd->Bx = static_model->sx * (static_model->A * x + static_model->D * y + static_model->E * z) - static_model->G;
    mfd->By = static_model->sy * (static_model->D * x + static_model->B * y + static_model->F * z) - static_model->H;
    mfd->Bz = static_model->sz * (static_model->E * x + static_model->F * y + static_model->C * z) - static_model->I;
    return ESP_OK;
}





/// @brief 初始化本库的PsP2P_DM生产者节点
void calibration_tools_init_PsP2P_DM_Producer() {
    PsP2P_DM_Producer = PsP2P_DM_node_create(name_of_PsP2P_DM_Producer, PsP2P_DM_IDENTITY_PRODUCER, MESSAGE_QUEUE_LENGTH_PRODUCER_PSP2P_DM_CALIBRATION_TOOLS);
}

/// @brief 在PsP2P_DM生产者上架静态校准模型
/// @param static_model 静态校准模型
/// @return [ESP_OK 成功]
/// @return [ESP_ERR_INVALID_ARG   同一生产者不可以有完全同名产品,对本生产者,产品名在自己的产品链表中已经被使用过 / 输入了无法处理的空指针]
/// @return [ESP_ERR_INVALID_STATE 生产者未完成初始化 / 产品链表存在头或尾缺失,维护状态异常]
/// @return [ESP_ERR_NOT_SUPPORTED 非生产者不可使用上架(put)操作]
/// @return [ESP_ERR_NO_MEM 内存不足]
esp_err_t put_GS_calibration_static_model(GS_calibration_static_model_t* static_model) {
    ESP_RETURN_ON_FALSE(static_model, ESP_ERR_INVALID_ARG, calibration_tools_TAG, "输入了无法处理的空指针 描述 %s", esp_err_to_name(ESP_ERR_INVALID_ARG));
    ESP_RETURN_ON_FALSE(PsP2P_DM_Producer, ESP_ERR_INVALID_STATE, calibration_tools_TAG, "生产者未完成初始化 描述 %s", esp_err_to_name(ESP_ERR_INVALID_STATE));
    ///封装数据产品
    cJSON* root = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(root, ESP_ERR_NO_MEM, calibration_tools_TAG, "内存不足, 描述 %s", esp_err_to_name(ESP_ERR_NO_MEM));

#if VERSION_OF_GS_CALIBRATION_STATIC_MODEL_T == 1
    cJSON* generate_time = cJSON_CreateObject();//struct timeval generate_time;//模型的生成时间
    ESP_RETURN_ON_FALSE(generate_time, ESP_ERR_NO_MEM, calibration_tools_TAG, "内存不足, 描述 %s", esp_err_to_name(ESP_ERR_NO_MEM));
    cJSON_AddNumberToObject(generate_time, "tv_sec", (double)static_model->generate_time_RTC.tv_sec);
    cJSON_AddNumberToObject(generate_time, "tv_usec", (double)static_model->generate_time_RTC.tv_usec);
    cJSON_AddItemToObject(root, "generate_time", generate_time);
    //float SSR;//椭球拟合残差平方和 
    cJSON_AddNumberToObject(root, "SSR", (double)static_model->SSR);
    //float A, B, C, D, E, F, G, H, I;//椭球参数
    cJSON_AddNumberToObject(root, "A", (double)static_model->A);
    cJSON_AddNumberToObject(root, "B", (double)static_model->B);
    cJSON_AddNumberToObject(root, "C", (double)static_model->C);
    cJSON_AddNumberToObject(root, "D", (double)static_model->D);
    cJSON_AddNumberToObject(root, "E", (double)static_model->E);
    cJSON_AddNumberToObject(root, "F", (double)static_model->F);
    cJSON_AddNumberToObject(root, "G", (double)static_model->G);
    cJSON_AddNumberToObject(root, "H", (double)static_model->H);
    cJSON_AddNumberToObject(root, "I", (double)static_model->I);
    //float sx, sy, sz;//缩放量
    cJSON_AddNumberToObject(root, "sx", (double)static_model->sx);
    cJSON_AddNumberToObject(root, "sy", (double)static_model->sy);
    cJSON_AddNumberToObject(root, "sz", (double)static_model->sz);
    //float x0, y0, z0;//偏移量
    cJSON_AddNumberToObject(root, "x0", (double)static_model->x0);
    cJSON_AddNumberToObject(root, "y0", (double)static_model->y0);
    cJSON_AddNumberToObject(root, "z0", (double)static_model->z0);
#endif
    return PsP2P_DM_put(PsP2P_DM_Producer, (void*)root, "GS_calibration_static_model");
}









