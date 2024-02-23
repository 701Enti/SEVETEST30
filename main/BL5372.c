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

// 该文件归属701Enti组织，主要由SEVETEST30开发团队维护，包含各种SE30对外部RTC芯片 BL5372的支持
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体不包含i2c通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行
// github: https://github.com/701Enti
// bilibili: 701Enti

#include "BL5372.h"
#include "esp_log.h"
#include "board_def.h"
#include "driver/i2c.h"

/// @brief 设置BL5372实时时间
/// @param time 作为设置实时时间的数据的地址
void BL5372_time_now_set(BL5372_time_t *time)
{
    const char *TAG = "BL5372_time_now_set";

    uint8_t write_buf[8] = {0};

    // 设置内部寄存器地址和传输配置0000
    write_buf[0] = BL5372_REG_TIME_SEC << 4;
    //映射写入的值
    write_buf[1] = BCD8421_transform_decode(time->second);
    write_buf[2] = BCD8421_transform_decode(time->minute);
    write_buf[3] = BCD8421_transform_decode(time->hour);
    write_buf[4] = BCD8421_transform_decode(time->week);
    write_buf[5] = BCD8421_transform_decode(time->day);
    write_buf[6] = BCD8421_transform_decode(time->month);
    write_buf[7] = BCD8421_transform_decode(time->year);

    esp_err_t err = i2c_master_write_to_device(DEVICE_I2C_PORT,BL5372_DEVICE_ADD, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);

    if (err != ESP_OK)
        ESP_LOGE(TAG, "与外部离线RTC通讯时发现问题 描述： %s", esp_err_to_name(err));
    else
        ESP_LOGI(TAG, "外部离线RTC实时时间已更新");
}

/// @brief 获取BL5372实时时间
/// @param time 保存实时时间的数据的地址
void BL5372_time_now_get(BL5372_time_t *time)
{
    const char *TAG = "BL5372_time_now_get";

    uint8_t read_buf[7] = {0};                    // 读取缓存
    uint8_t write_buf = BL5372_REG_TIME_SEC << 4; // 高4位设置起始地址，低4位设置传输模式0000
    esp_err_t err = i2c_master_write_read_device(DEVICE_I2C_PORT, BL5372_DEVICE_ADD, &write_buf, sizeof(write_buf), read_buf, sizeof(read_buf), 1000 / portTICK_PERIOD_MS);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "与外部离线RTC通讯时发现问题 描述： %s", esp_err_to_name(err));

    time->second = BCD8421_transform_recode(read_buf[0]);
    time->minute = BCD8421_transform_recode(read_buf[1]);
    time->hour = BCD8421_transform_recode(read_buf[2]);
    time->week = BCD8421_transform_recode(read_buf[3]);
    time->day = BCD8421_transform_recode(read_buf[4]);
    time->month = BCD8421_transform_recode(read_buf[5]);
    time->year = BCD8421_transform_recode(read_buf[6]);

    ESP_LOGI(TAG, "%d %d %d %d %d %d %d", time->year, time->month, time->day, time->week, time->hour, time->minute, time->second);
}

/// @brief 设置BL5372的运行配置
/// @param rtc_cfg 作为运行配置的数据的地址
void BL5372_config_set(BL5372_cfg_t *rtc_cfg)
{
    const char *TAG = "BL5372_config_set";

    uint8_t write_buf[3] = {0}; // 写入缓存

    // 设置内部寄存器地址和传输配置0000
    write_buf[0] = BL5372_REG_CTRL_1 << 4;
    // 映射写入的值
    write_buf[1] |= rtc_cfg->INT_mode;
    write_buf[1] |= rtc_cfg->test_en << 3;
    write_buf[1] |= rtc_cfg->intr_out_mode << 4;
    write_buf[1] |= rtc_cfg->alarm_B_en << 6;
    write_buf[1] |= rtc_cfg->alarm_A_en << 7;
    write_buf[2] |= rtc_cfg->alarm_B_out_flag_or_out_keep;
    write_buf[2] |= rtc_cfg->alarm_A_out_flag_or_out_keep << 1;
    write_buf[2] |= rtc_cfg->INT_out_flag_or_out_keep << 2;
    write_buf[2] |= rtc_cfg->out_32KHz_false << 3;
    write_buf[2] |= rtc_cfg->adj_en_or_xstp << 4;
    write_buf[2] |= rtc_cfg->hour_24_clock_en << 5;

    ESP_LOGI(TAG, "%d %d", write_buf[1], write_buf[2]);

    esp_err_t err = i2c_master_write_to_device(DEVICE_I2C_PORT, BL5372_DEVICE_ADD, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);

    if (err != ESP_OK)
        ESP_LOGE(TAG, "与外部离线RTC通讯时发现问题 描述： %s", esp_err_to_name(err));
    else
        ESP_LOGI(TAG, "外部离线RTC配置已更新");
}

/// @brief 获取BL5372的运行配置
/// @param rtc_cfg 保存运行配置的数据的地址
void BL5372_config_get(BL5372_cfg_t *rtc_cfg)
{

    const char *TAG = "BL5372_config_get";

    uint8_t read_buf[2] = {0};                  // 读取缓存
    uint8_t write_buf = BL5372_REG_CTRL_1 << 4; // 高4位设置起始地址，低4位设置传输模式0000
    esp_err_t err = i2c_master_write_read_device(DEVICE_I2C_PORT, BL5372_DEVICE_ADD, &write_buf, sizeof(write_buf), read_buf, sizeof(read_buf), 1000 / portTICK_PERIOD_MS);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "与外部离线RTC通讯时发现问题 描述： %s", esp_err_to_name(err));

    ESP_LOGI(TAG, "%d %d", read_buf[0], read_buf[1]);

    // 映射得到的值
    rtc_cfg->alarm_A_en = 0x01 & (read_buf[0] >> 7);
    rtc_cfg->alarm_B_en = 0x01 & (read_buf[0] >> 6);
    rtc_cfg->intr_out_mode = 0x03 & (read_buf[0] >> 4);
    rtc_cfg->test_en = 0x01 & (read_buf[0] >> 3);
    rtc_cfg->INT_mode = 0x07 & read_buf[0];
    rtc_cfg->hour_24_clock_en = 0x01 & (read_buf[1] >> 5);
    rtc_cfg->adj_en_or_xstp = 0x01 & (read_buf[1] >> 4);
    rtc_cfg->out_32KHz_false = 0x01 & (read_buf[1] >> 3);
    rtc_cfg->INT_out_flag_or_out_keep = 0x01 & (read_buf[1] >> 2);
    rtc_cfg->alarm_A_out_flag_or_out_keep = 0x01 & (read_buf[1] >> 1);
    rtc_cfg->alarm_B_out_flag_or_out_keep = 0x01 & read_buf[1];
}

/// @brief 由8bits 8421BCD码解码为十进制数
/// @param bin_dat 8421BCD码,必须小于或等于8个bit
/// @return 十进制数,小于或等于两位
uint8_t BCD8421_transform_recode(uint8_t bin_dat)
{
    uint8_t dat_buf1 = 0x0f & (bin_dat >> 4); // 高四位表示十进制十位
    uint8_t dat_buf0 = 0x0f & bin_dat;        // 低四位表示十进制个位
    uint8_t dec_buf = dat_buf1 * 10 + dat_buf0;
    return dec_buf;
}

/// @brief 由十进制数编码为8bits 8421BCD码
/// @param dec_dat 十进制数,必须小于或等于两位的数
/// @return 8421BCD码8bits
uint8_t BCD8421_transform_decode(uint8_t dec_dat)
{
    uint8_t dat_buf1 = dec_dat / 10;       // 十位转换为8bits二进制高四位
    uint8_t dat_buf0 = dec_dat - dat_buf1; // 个位转换为8bits二进制低四位
    uint8_t bin_buf = (dat_buf1 << 4) | dat_buf0;
    return bin_buf;
}


// 以默认配置初始化BL5372的运行配置
void BL5372_config_init()
{
    BL5372_cfg_t cfg_buf = BL5372_DEFAULT_INIT_CONFIG;
    BL5372_config_set(&cfg_buf);
}


