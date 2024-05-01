
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

 // 该文件归属701Enti组织，主要由SEVETEST30开发团队维护，包含各种SE30对温湿度传感器 AHT21的支持
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // 敬告：文件本体不包含i2c通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行
 // github: https://github.com/701Enti
 // bilibili: 701Enti

#include <AHT21.h>
#include "esp_log.h"
#include "board_def.h"
#include "driver/i2c.h"

void AHT21_begin() {
    const char* TAG = "AHT21_begin";
    //校验状态字    
    if ((AHT21_get_status() & 0x18) == 0x18) {
        ESP_LOGI(TAG, "传感器可以正常运行");
    }
    else {
        ESP_LOGE(TAG, "传感器需要关键的初始化操作");
    }
}

/// @brief 获取运行状态字
/// @return 一个字节的状态字
uint8_t AHT21_get_status() {
    const char* TAG = "AHT21_get_status";
    uint8_t read_buf = 0;// 读取缓存
    uint8_t write_buf = AHT21_STATUS_GET_COMMAND;
    esp_err_t err = i2c_master_write_read_device(DEVICE_I2C_PORT, AHT21_DEVICE_ADD, &write_buf, sizeof(write_buf), &read_buf, sizeof(read_buf), 1000 / portTICK_PERIOD_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "与温湿度传感器AHT21通讯时发现问题 描述： %s", esp_err_to_name(err));
    }
    return read_buf;
}

/// @brief 触发测量
void AHT21_trigger() {
    const char* TAG = "AHT21_trigger";
    uint8_t write_buf[3] = { 0 };
    write_buf[0] = AHT21_TRIGGER_COMMAND;
    write_buf[1] = 0x33;
    write_buf[2] = 0x00;
    esp_err_t err = i2c_master_write_to_device(DEVICE_I2C_PORT, AHT21_DEVICE_ADD, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);

    if (err != ESP_OK)
        ESP_LOGE(TAG, "与温湿度传感器AHT21通讯时发现问题 描述： %s", esp_err_to_name(err));

}

/// @brief 获取测量结果
/// @param dest 目标存储区域,同时会读取其中配置
void AHT21_get_result(AHT21_result_handle_t* dest) {
    const char* TAG = "AHT21_get_result";

    if ((AHT21_get_status() & 0x80)) {
        ESP_LOGE(TAG, "温湿度传感器AHT21正在测量中,无法读取");
        return;
    }
    else {
        uint8_t read_buf[8] = {0};// 读取缓存
        uint8_t write_buf = AHT21_STATUS_GET_COMMAND;
        esp_err_t err = ESP_OK;

        if (dest->flag_crc)
            err = i2c_master_write_read_device(DEVICE_I2C_PORT, AHT21_DEVICE_ADD, &write_buf, sizeof(write_buf), &read_buf, sizeof(read_buf), 1000 / portTICK_PERIOD_MS);
        else
            err = i2c_master_write_read_device(DEVICE_I2C_PORT, AHT21_DEVICE_ADD, &write_buf, sizeof(write_buf), &read_buf, sizeof(read_buf) - 1, 1000 / portTICK_PERIOD_MS);

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "与温湿度传感器AHT21通讯时发现问题 描述： %s", esp_err_to_name(err));
            return;
        }
        else{
            dest->temp = ((float)(((read_buf[3]&0x0F)<<16)|(read_buf[4]<<8)|(read_buf[5]))/(1048576)*200)-50;
            dest->hum =  ((float)((read_buf[1]<<12)|(read_buf[2]<<4)|(read_buf[3]>>4))/(1048576))*100;
            ESP_LOGI(TAG, "温度 %f\u2103     湿度 %f%%RH",dest->temp,dest->hum);
        }
        

    }
    return;
}