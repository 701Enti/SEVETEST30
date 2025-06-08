
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

#include "hscdtd008a.h"
#include "esp_log.h"
#include "board_def.h"
#include "driver/i2c.h"
#include "esp_check.h"
#include <math.h>

/// @brief 获取HSCDTD008A当前模式
/// @param mode 读取当前模式并保存到mode
/// @return [ESP_OK 读取完成并保存到mode]  
/// @return [ESP_ERR_INVALID_ARG mode为NULL / 无法进行读取,通信参数错误] 
/// @return [ESP_FAIL 无法进行读取,发送命令时发现问题,未应答] 
/// @return [ESP_ERR_INVALID_STATE 无法进行读取,I2C driver 未安装或没有运行在主机模式] 
/// @return [ESP_ERR_TIMEOUT 无法进行读取,操作超时,因为总线忙]
esp_err_t hscdtd008a_mode_get(GS_mode_t* mode) {
    const char* TAG = "hscdtd008a_mode_get";
    if (!mode) {
        ESP_LOGE(TAG, "输入了无法处理的空指针");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t control1_address = CONTROL_1;
    uint8_t control1_data = 0;

    //读取模式数据(读取CTRL1的PC位,mode本身就完全等于PC)
    esp_err_t ret = i2c_master_write_read_device(DEVICE_I2C_PORT, HSCDTD008A_DEVICE_ADDRESS, &control1_address, 1, &control1_data, 1, portMAX_DELAY);
    ESP_RETURN_ON_ERROR(ret, TAG, "无法进行读取, 读取模式数据 时发现通信问题 描述 %s", esp_err_to_name(ret));

    *mode = (GS_mode_t)((control1_data >> 7) & 0x01);

    return ESP_OK;
}

/// @brief 设置HSCDTD008A当前模式
/// @param mode 设置当前模式为mode
/// @return [ESP_OK 设置完成]  
/// @return [ESP_ERR_INVALID_ARG 无法进行设置,通信参数错误] 
/// @return [ESP_FAIL 无法进行设置,发送命令时发现问题,未应答] 
/// @return [ESP_ERR_INVALID_STATE 无法进行设置,I2C driver 未安装或没有运行在主机模式] 
/// @return [ESP_ERR_TIMEOUT 无法进行设置,操作超时,因为总线忙]
esp_err_t hscdtd008a_mode_set(GS_mode_t  mode) {
    const char* TAG = "hscdtd008a_mode_set";
    esp_err_t ret = ESP_OK;

    //缓存设置前数据,在设置前数据基础上修改,避免对非需要位改动
    uint8_t control1_address = CONTROL_1;
    uint8_t control1_data = 0;
    ret = i2c_master_write_read_device(DEVICE_I2C_PORT, HSCDTD008A_DEVICE_ADDRESS, &control1_address, 1, &control1_data, 1, portMAX_DELAY);
    ESP_RETURN_ON_ERROR(ret, TAG, "无法设置,缓存 设置前数据 时发现通信问题 描述 %s", esp_err_to_name(ret));

    //设置HSCDTD008A当前模式(写入CTRL1的PC位,mode本身就完全等于PC)
    uint8_t control1_buf[2] = { CONTROL_1,(control1_data & 0x7F) | (mode << 7) };
    ret = i2c_master_write_to_device(DEVICE_I2C_PORT, HSCDTD008A_DEVICE_ADDRESS, control1_buf, 2, portMAX_DELAY);
    ESP_RETURN_ON_ERROR(ret, TAG, "无法设置,发现通信问题 描述 %s", esp_err_to_name(ret));
    if (mode == GS_MODE_ACTIVE) {
        ESP_LOGI(TAG, "切换到 [ACTIVE_MODE] ,活跃,无限制读写,可以开始测量,可切换state选择测量方式");
        GS_state_t state;
        if (hscdtd008a_state_get(&state) == ESP_OK) {
            if (state == GS_STATE_NORMAL)ESP_LOGI(TAG, "--------当前测量状态 [NORMAL_STATE] ,传感器会自动根据配置触发测量");
            if (state == GS_STATE_FORCE)ESP_LOGI(TAG, "--------当前测量状态 [FORCE_STATE] ,需要手动触发测量");
        }
    }
    if (mode == GS_MODE_STAND_BY) {
        ESP_LOGI(TAG, "切换到 [STAND_BY_MODE] ,低功耗待机,限制控制 自检启动,温度测量启动,手动触发测量,但不限制任何get读取操作");
    }
    return ESP_OK;
}

/// @brief 获取HSCDTD008A状态的设置(不一定是当前状态,因为它指的是ACTIVE_MODE下的状态设置)
/// @param state 读取状态的设置并保存到state
/// @return [ESP_OK 读取完成并保存到state]  
/// @return [ESP_ERR_INVALID_ARG state为NULL / 无法进行读取,通信参数错误] 
/// @return [ESP_FAIL 无法进行读取,发送命令时发现问题,未应答] 
/// @return [ESP_ERR_INVALID_STATE 无法进行读取,I2C driver 未安装或没有运行在主机模式] 
/// @return [ESP_ERR_TIMEOUT 无法进行读取,操作超时,因为总线忙]
esp_err_t hscdtd008a_state_get(GS_state_t* state) {
    const char* TAG = "hscdtd008a_state_get";
    if (!state) {
        ESP_LOGE(TAG, "输入了无法处理的空指针");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t control1_address = CONTROL_1;
    uint8_t control1_data = 0;

    //读取状态数据(读取CTRL1的FS位,state本身就完全等于FS)
    esp_err_t ret = i2c_master_write_read_device(DEVICE_I2C_PORT, HSCDTD008A_DEVICE_ADDRESS, &control1_address, 1, &control1_data, 1, portMAX_DELAY);
    ESP_RETURN_ON_ERROR(ret, TAG, "无法进行读取, 读取状态数据 时发现通信问题 描述 %s", esp_err_to_name(ret));

    *state = (GS_state_t)((control1_data >> 1) & 0x01);

    return ESP_OK;
}

/// @brief 设置HSCDTD008A测量状态(不一定是当前状态,因为它指的是ACTIVE_MODE下的状态设置)
/// @param state 设置ACTIVE_MODE下的状态为state
/// @return [ESP_OK 设置完成]  
/// @return [ESP_ERR_INVALID_ARG 无法进行设置,通信参数错误] 
/// @return [ESP_FAIL 无法进行设置,发送命令时发现问题,未应答] 
/// @return [ESP_ERR_INVALID_STATE 无法进行设置,I2C driver 未安装或没有运行在主机模式] 
/// @return [ESP_ERR_TIMEOUT 无法进行设置,操作超时,因为总线忙]
esp_err_t hscdtd008a_state_set(GS_state_t  state) {
    const char* TAG = "hscdtd008a_state_set";
    esp_err_t ret = ESP_OK;

    //缓存设置前数据,在设置前数据基础上修改,避免对非需要位改动
    uint8_t control1_address = CONTROL_1;
    uint8_t control1_data = 0;
    ret = i2c_master_write_read_device(DEVICE_I2C_PORT, HSCDTD008A_DEVICE_ADDRESS, &control1_address, 1, &control1_data, 1, portMAX_DELAY);
    ESP_RETURN_ON_ERROR(ret, TAG, "无法设置,缓存 设置前数据 时发现通信问题 描述 %s", esp_err_to_name(ret));

    //设置HSCDTD008A当前模式(写入CTRL1的FS位,state本身就完全等于FS)
    uint8_t control1_buf[2] = { CONTROL_1,(control1_data & 0xFD) | (state << 1) };
    ret = i2c_master_write_to_device(DEVICE_I2C_PORT, HSCDTD008A_DEVICE_ADDRESS, control1_buf, 2, portMAX_DELAY);
    ESP_RETURN_ON_ERROR(ret, TAG, "无法设置,发现通信问题 描述 %s", esp_err_to_name(ret));
    if (state == GS_STATE_NORMAL)ESP_LOGI(TAG, "设置测量状态 [NORMAL_STATE] ,传感器会自动根据配置触发测量");
    if (state == GS_STATE_FORCE)ESP_LOGI(TAG, "设置测量状态 [FORCE_STATE] ,需要手动触发测量");
    return ESP_OK;
}

/// @brief HSCDTD008A自我检测
/// @return [ESP_OK 自检通过]  
/// @return [ESP_ERR_INVALID_ARG 无法进行自检,参数错误] 
/// @return [ESP_FAIL 无法进行自检,发送命令时发现问题,未应答 / 自检不通过] 
/// @return [ESP_ERR_INVALID_STATE 无法进行自检,I2C driver 未安装或没有运行在主机模式 / 因为在非ACTIVE_MODE,无法启动自检] 
/// @return [ESP_ERR_TIMEOUT 无法进行自检,操作超时,因为总线忙]
esp_err_t hscdtd008a_selftest() {
    const char* TAG = "hscdtd008a_selftest";
    esp_err_t ret = ESP_OK;
    uint8_t control3_buf[2] = { CONTROL_3,0x10 };
    uint8_t response_register_address = SELFTEST_RESPONSE;
    uint8_t response_0 = 0x00;
    uint8_t response_1 = 0x00;
    uint8_t response_2 = 0x00;

    //查看当前模式是否为ACTIVE_MODE(在非ACTIVE_MODE无法启动自检)
    GS_mode_t mode = GS_MODE_STAND_BY;
    ret = hscdtd008a_mode_get(&mode);
    ESP_RETURN_ON_ERROR(ret, TAG, "无法自检, 查看当前模式 时发现通信问题 描述 %s", esp_err_to_name(ret));
    if (mode != GS_MODE_ACTIVE) {
        ret = ESP_ERR_INVALID_STATE;
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "无法自检,因为在 非ACTIVE_MODE 而无法启动自检 描述 %s [请执行以切换: hscdtd008a_mode_set(GS_MODE_ACTIVE);]", esp_err_to_name(ret));


    //查看自检前结果(读取SELFTEST_RESPONSE)
    ret = i2c_master_write_read_device(DEVICE_I2C_PORT, HSCDTD008A_DEVICE_ADDRESS, &response_register_address, 1, &response_0, 1, portMAX_DELAY);
    ESP_RETURN_ON_ERROR(ret, TAG, "无法自检, 查看自检前结果 时发现通信问题 描述 %s", esp_err_to_name(ret));

    //启动自检(写入CONTROL3)
    ret = i2c_master_write_to_device(DEVICE_I2C_PORT, HSCDTD008A_DEVICE_ADDRESS, control3_buf, 2, portMAX_DELAY);
    ESP_RETURN_ON_ERROR(ret, TAG, "无法自检, 启动自检 时发现通信问题 描述 %s", esp_err_to_name(ret));

    vTaskDelay(pdMS_TO_TICKS(100));

    //查看自检结果(读取SELFTEST_RESPONSE)
    ret = i2c_master_write_read_device(DEVICE_I2C_PORT, HSCDTD008A_DEVICE_ADDRESS, &response_register_address, 1, &response_1, 1, portMAX_DELAY);
    ESP_RETURN_ON_ERROR(ret, TAG, "无法自检, 查看自检结果 时发现通信问题 描述 %s", esp_err_to_name(ret));

    //确保自检完成(再次读取SELFTEST_RESPONSE)
    ret = i2c_master_write_read_device(DEVICE_I2C_PORT, HSCDTD008A_DEVICE_ADDRESS, &response_register_address, 1, &response_2, 1, portMAX_DELAY);
    ESP_RETURN_ON_ERROR(ret, TAG, "无法自检, 确保自检完成 时发现通信问题 描述 %s", esp_err_to_name(ret));

    if (response_0 != 0X55 || response_1 != 0xAA || response_2 != 0X55) {
        ESP_LOGE(TAG, "HSCDTD008A明确自检不通过 SELFTEST_RESPONSE: [0x%x] -> [0x%x] -> [0x%x]", response_0, response_1, response_2);
        return ESP_FAIL;
    }
    else {
        ESP_LOGI(TAG, "HSCDTD008A明确自检通过");
        return ESP_OK;
    }
}

/// @brief 获取输出数据(未处理的源数据)
/// @note 还会读取CTRL4.RS作为量程标识,写入data,不仅仅读取OUTPUT_*_*寄存器
/// @param data 读取输出数据,并按照GS_output_data_t格式,保存到data
/// @return [ESP_OK 读取完成并保存到data]  
/// @return [ESP_ERR_INVALID_ARG data为NULL / 无法进行获取,通信参数错误] 
/// @return [ESP_FAIL 无法进行获取,发送命令时发现问题,未应答] 
/// @return [ESP_ERR_INVALID_STATE 无法进行获取,I2C driver 未安装或没有运行在主机模式] 
/// @return [ESP_ERR_TIMEOUT 无法进行获取,操作超时,因为总线忙]
esp_err_t hscdtd008a_output_data_get(GS_output_data_t* data) {
    const char* TAG = "hscdtd008a_output_data_get";
    if (!data) {
        ESP_LOGE(TAG, "输入了无法处理的空指针");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_OK;

    //读取CTRL4.RS,从而获取量程标识
    uint8_t control4_address = CONTROL_4;
    uint8_t control4_data = 0;
    ret = i2c_master_write_read_device(DEVICE_I2C_PORT, HSCDTD008A_DEVICE_ADDRESS, &control4_address, 1, &control4_data, 1, portMAX_DELAY);
    ESP_RETURN_ON_ERROR(ret, TAG, "读取CTRL4.RS作为量程标识时发现通信问题 描述%s", esp_err_to_name(ret));
    data->range = (GS_range_of_output_t)((control4_data >> 4) & 0x01);

    //读取OUTPUT_* _* 寄存器,从而获取输出数据
    uint8_t output[6] = { 0 };
    uint8_t address[6] =
    {
        OUTPUT_X_LSB,
        OUTPUT_X_MSB,
        OUTPUT_Y_LSB,
        OUTPUT_Y_MSB,
        OUTPUT_Z_LSB,
        OUTPUT_Z_MSB
    };
    for (int idx = 0;idx < 6;idx++) {
        ret = i2c_master_write_read_device(DEVICE_I2C_PORT, HSCDTD008A_DEVICE_ADDRESS, &address[idx], 1, &output[idx], 1, portMAX_DELAY);
        ESP_RETURN_ON_ERROR(ret, TAG, "读取OUTPUT_*_*寄存器时发现通信问题 描述%s", esp_err_to_name(ret));
    }

    data->raw_output_x_LSB = output[0];
    data->raw_output_x_MSB = output[1];
    data->raw_output_y_LSB = output[2];
    data->raw_output_y_MSB = output[3];
    data->raw_output_z_LSB = output[4];
    data->raw_output_z_MSB = output[5];

    return ESP_OK;
}


/// @brief 磁感应强度数据转换为角数据(输出数据的单位由输入参数unit设置)
/// @param unit 选择输出数据的单位,这是一个枚举类型
/// @param src 导入磁感应强度数据
/// @param dest 转换结果保存到dest
/// @note 为了获取角数据,需要磁感应强度数据,这意味着需要先调用to_magnetic_flux_density_data()获取
/// @return [ESP_OK 转换完成并保存到dest]  
/// @return [ESP_ERR_INVALID_ARG 参数错误, src为NULL / dest为NULL / 选择了不支持的单位(输入的unit值异常)] 
esp_err_t to_angle_data(GS_unit_of_angle_data_t unit, GS_magnetic_flux_density_data_t* src, GS_angle_data_t* dest) {
    const char* TAG = "to_angle_data";
    esp_err_t ret = ESP_OK;

    if (!src) {
        ESP_LOGE(TAG, "输入了无法处理的空指针 - src");
        ret = ESP_ERR_INVALID_ARG;
    }
    if (!dest) {
        ESP_LOGE(TAG, "输入了无法处理的空指针 - dest");
        ret = ESP_ERR_INVALID_ARG;
    }
    if (ret != ESP_OK)return ret;

    if (unit == GS_UNIT_OF_ANGLE_DEGREES) {
        //选择输出单位:角度
        dest->unit = unit;

        //计算角数据,并转换为角度单位,并填充数据
        dest->azimuth = (double)(180.0 / M_PI) * atan2(src->By, src->Bx);//方位角(空间的磁感应强度(这里看作向量)在x-y平面上的投影向量与x轴正半轴的夹角,根据这个数据,后期可以据此判断方向是东,南,西,北等方向)
        dest->pitch = (double)(180.0 / M_PI) * atan2(src->Bz, sqrt(src->Bx * src->Bx + src->By * src->By));//俯仰角(空间的磁感应强度(这里看作向量)与x-y平面的夹角,后期可以据此判断俯仰姿态)

        //填充量程
        dest->range = src->range;

        ret = ESP_OK;
        return ret;
    }
    else if (unit == GS_UNIT_OF_ANGLE_RADIANS) {
        //选择输出单位:弧度
        dest->unit = unit;

        //计算角数据(计算结果就是弧度单位,不需转换单位),并填充数据
        dest->azimuth = atan2(src->By, src->Bx);//方位角(空间的磁感应强度(这里看作向量)在x-y平面上的投影向量与x轴正半轴的夹角,根据这个数据,后期可以据此判断方向是东,南,西,北等方向)
        dest->pitch = atan2(src->Bz, sqrt(src->Bx * src->Bx + src->By * src->By));//俯仰角(空间的磁感应强度(这里看作向量)与x-y平面的夹角,后期可以据此判断俯仰姿态)

        //填充量程
        dest->range = src->range;

        ret = ESP_OK;
        return ret;
    }
    else {
        ESP_LOGE(TAG, "选择了不支持的单位(输入的unit值异常, unit = %d )", (int)unit);
        ret = ESP_ERR_INVALID_ARG;
        return ret;
    }
}



/// @brief 将output(未处理的源数据)转换为磁感应强度数据(单位: uT)
/// @param src 导入output(未处理的源数据)
/// @param dest 转换结果保存到dest
/// @return [ESP_OK 转换完成并保存到dest]  
/// @return [ESP_ERR_INVALID_ARG 参数错误, src为NULL / dest为NULL / 无法处理的量程] 
/// @return [ESP_FAIL 无法进行转换,源数据存在格式疑问,因为在GS_RANGE_OF_OUTPUT_14BIT的情况下,output_*_MSB的bit6和bit7不相等,无法明确符号位] 
esp_err_t to_magnetic_flux_density_data(GS_output_data_t* src, GS_magnetic_flux_density_data_t* dest) {
    const char* TAG = "to_magnetic_flux_density_data";
    esp_err_t ret = ESP_OK;

    if (!src) {
        ESP_LOGE(TAG, "输入了无法处理的空指针 - src");
        ret = ESP_ERR_INVALID_ARG;
    }
    if (!dest) {
        ESP_LOGE(TAG, "输入了无法处理的空指针 - dest");
        ret = ESP_ERR_INVALID_ARG;
    }
    if (ret != ESP_OK)return ret;

    if (src->range == GS_RANGE_OF_OUTPUT_14BIT) {
        //数据格式检查
        if (((src->raw_output_x_MSB >> 6) & 0x01) != ((src->raw_output_x_MSB >> 7) & 0x01)) {
            ESP_LOGE(TAG, "无法进行转换, 源数据存在格式疑问,因为在GS_RANGE_OF_OUTPUT_14BIT的情况下, OUTPUT_X_MSB的bit6和bit7不相等,无法明确符号位");
            ret = ESP_FAIL;
        }
        if (((src->raw_output_y_MSB >> 6) & 0x01) != ((src->raw_output_y_MSB >> 7) & 0x01)) {
            ESP_LOGE(TAG, "无法进行转换, 源数据存在格式疑问,因为在GS_RANGE_OF_OUTPUT_14BIT的情况下, OUTPUT_Y_MSB的bit6和bit7不相等,无法明确符号位");
            ret = ESP_FAIL;
        }
        if (((src->raw_output_z_MSB >> 6) & 0x01) != ((src->raw_output_z_MSB >> 7) & 0x01)) {
            ESP_LOGE(TAG, "无法进行转换, 源数据存在格式疑问,因为在GS_RANGE_OF_OUTPUT_14BIT的情况下, OUTPUT_Z_MSB的bit6和bit7不相等,无法明确符号位");
            ret = ESP_FAIL;
        }
        if (ret != ESP_OK)return ret;

        //获取符号位(在GS_RANGE_OF_OUTPUT_14BIT的情况下,OUTPUT_*_MSB的符号标识bit6和bit7相等,其值为0代表正数,其值为1代表负数)
        bool sign_x = (src->raw_output_x_MSB >> 7) & 0x01;
        bool sign_y = (src->raw_output_y_MSB >> 7) & 0x01;
        bool sign_z = (src->raw_output_z_MSB >> 7) & 0x01;

        //OUTPUT_*_MSB的符号标识bit6和bit7相等,其值为0代表正数,其值为1代表负数,负数用补码表示,处理时会取反变为0,故无需屏蔽处理

        //解析x轴数据并填充
        if (sign_x) {
            dest->Bx = (double)0 - (double)(((((int)((~src->raw_output_x_MSB) & 0xFF) << 8) | ((~src->raw_output_x_LSB) & 0xFF)) + 1) * HSCDTD008A_1LSB_MAGNETIC_FLUX_DENSITY); //负数为补码格式,取反加1,获得绝对值,最后添加符号
        }
        else {
            dest->Bx = (double)((((int)(src->raw_output_x_MSB & 0x3F) << 8) | src->raw_output_x_LSB) * HSCDTD008A_1LSB_MAGNETIC_FLUX_DENSITY);//正数取有效位即可
        }

        //解析y轴数据并填充
        if (sign_y) {
            dest->By = (double)0 - (double)(((((int)((~src->raw_output_y_MSB) & 0xFF) << 8) | ((~src->raw_output_y_LSB) & 0xFF)) + 1) * HSCDTD008A_1LSB_MAGNETIC_FLUX_DENSITY); //负数为补码格式,取反加1,获得绝对值,最后添加符号
        }
        else {
            dest->By = (double)((((int)(src->raw_output_y_MSB & 0x3F) << 8) | src->raw_output_y_LSB) * HSCDTD008A_1LSB_MAGNETIC_FLUX_DENSITY);//正数取有效位即可
        }

        //解析z轴数据并填充
        if (sign_z) {
            dest->Bz = (double)0 - (double)(((((int)((~src->raw_output_z_MSB) & 0xFF) << 8) | ((~src->raw_output_z_LSB) & 0xFF)) + 1) * HSCDTD008A_1LSB_MAGNETIC_FLUX_DENSITY); //负数为补码格式,取反加1,获得绝对值,最后添加符号
        }
        else {
            dest->Bz = (double)((((int)(src->raw_output_z_MSB & 0x3F) << 8) | src->raw_output_z_LSB) * HSCDTD008A_1LSB_MAGNETIC_FLUX_DENSITY);//正数取有效位即可
        }

        //计算磁感应强度的模并填充
        dest->B0 = sqrt(dest->Bx * dest->Bx + dest->By * dest->By + dest->Bz * dest->Bz);

        //填充量程
        dest->range = src->range;
    }
    else if (src->range == GS_RANGE_OF_OUTPUT_15BIT) {
        //获取符号位(在GS_RANGE_OF_OUTPUT_15BIT的情况下,OUTPUT_*_MSB的符号标识为bit7,其值为0代表正数,其值为1代表负数)
        bool sign_x = (src->raw_output_x_MSB >> 7) & 0x01;
        bool sign_y = (src->raw_output_y_MSB >> 7) & 0x01;
        bool sign_z = (src->raw_output_z_MSB >> 7) & 0x01;

        //解析x轴数据并填充
        if (sign_x) {
            dest->Bx = (double)0 - (double)(((((int)((~src->raw_output_x_MSB) & 0xFF) << 8) | ((~src->raw_output_x_LSB) & 0xFF)) + 1) * HSCDTD008A_1LSB_MAGNETIC_FLUX_DENSITY); //负数为补码格式,取反加1,获得绝对值,最后添加符号
        }
        else {
            dest->Bx = (double)((((int)(src->raw_output_x_MSB & 0x7F) << 8) | src->raw_output_x_LSB) * HSCDTD008A_1LSB_MAGNETIC_FLUX_DENSITY);//正数取有效位即可
        }

        //解析y轴数据并填充
        if (sign_y) {
            dest->By = (double)0 - (double)(((((int)((~src->raw_output_y_MSB) & 0xFF) << 8) | ((~src->raw_output_y_LSB) & 0xFF)) + 1) * HSCDTD008A_1LSB_MAGNETIC_FLUX_DENSITY); //负数为补码格式,取反加1,获得绝对值,最后添加符号
        }
        else {
            dest->By = (double)((((int)(src->raw_output_y_MSB & 0x7F) << 8) | src->raw_output_y_LSB) * HSCDTD008A_1LSB_MAGNETIC_FLUX_DENSITY);//正数取有效位即可
        }

        //解析z轴数据并填充
        if (sign_z) {
            dest->Bz = (double)0 - (double)(((((int)((~src->raw_output_z_MSB) & 0xFF) << 8) | ((~src->raw_output_z_LSB) & 0xFF)) + 1) * HSCDTD008A_1LSB_MAGNETIC_FLUX_DENSITY); //负数为补码格式,取反加1,获得绝对值,最后添加符号
        }
        else {
            dest->Bz = (double)((((int)(src->raw_output_z_MSB & 0x7F) << 8) | src->raw_output_z_LSB) * HSCDTD008A_1LSB_MAGNETIC_FLUX_DENSITY);//正数取有效位即可
        }

        //计算磁感应强度的模并填充
        dest->B0 = sqrt(dest->Bx * dest->Bx + dest->By * dest->By + dest->Bz * dest->Bz);

        //填充量程
        dest->range = src->range;
    }
    else {
        ESP_LOGE(TAG, "无法处理的量程 src->range = %d", (int)src->range);
        ret = ESP_ERR_INVALID_ARG;
        return ret;
    }

    return ret;
}




