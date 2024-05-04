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

// 该文件归属701Enti组织，SEVETEST30开发团队应该提供责任性维护，包含各种SE30对外部RTC芯片 BL5372的支持
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体不包含i2c通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行
// 目前没有考虑12小时制的读取,因为24小时制的运行更加利于SWEDA的时间同步和写入，而24小时制可以在UI显示时很方便的操作显示为12小时制而无需调度硬件
// BL5372在SEVETEST30控制板使用的是电池供电，调试时请保证其电源的供应，连接电池或接入VCC
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

    if(!time){
        ESP_LOGE(TAG,"导入了为空的数据地址");
        return;
    }

    uint8_t write_buf[8] = {0};

    // 设置内部寄存器地址和传输配置0000
    write_buf[0] = BL5372_REG_TIME_SEC << 4;
    // 映射写入的值
    write_buf[1] = BCD8421_transform_decode(time->second);
    write_buf[2] = BCD8421_transform_decode(time->minute);
    write_buf[3] = BCD8421_transform_decode(time->hour);
    write_buf[4] = BCD8421_transform_decode(time->week);
    write_buf[5] = BCD8421_transform_decode(time->day);
    write_buf[6] = BCD8421_transform_decode(time->month);
    write_buf[7] = BCD8421_transform_decode(time->year);

    esp_err_t err = i2c_master_write_to_device(DEVICE_I2C_PORT, BL5372_DEVICE_ADD, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);

    if (err != ESP_OK)
        ESP_LOGE(TAG, "与外部离线RTC通讯时发现问题,请检查电池电源连接 描述： %s", esp_err_to_name(err));
    else
        ESP_LOGI(TAG, "外部离线RTC实时时间已更新");
}

/// @brief 获取BL5372实时时间
/// @param time 保存实时时间的数据的地址
void BL5372_time_now_get(BL5372_time_t *time)
{
    const char *TAG = "BL5372_time_now_get";

    if(!time){
        ESP_LOGE(TAG,"导入了为空的数据地址");
        return;
    }

    uint8_t read_buf[7] = {0};                    // 读取缓存
    uint8_t write_buf = BL5372_REG_TIME_SEC << 4; // 高4位设置起始地址，低4位设置传输模式0000
    esp_err_t err = i2c_master_write_read_device(DEVICE_I2C_PORT, BL5372_DEVICE_ADD, &write_buf, sizeof(write_buf), read_buf, sizeof(read_buf), 1000 / portTICK_PERIOD_MS);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "与外部离线RTC通讯时发现问题 请检查电池电源连接 描述： %s", esp_err_to_name(err));

    time->second = BCD8421_transform_recode(read_buf[0]);
    time->minute = BCD8421_transform_recode(read_buf[1]);
    time->hour = BCD8421_transform_recode(read_buf[2]);
    time->week = BCD8421_transform_recode(read_buf[3]);
    time->day = BCD8421_transform_recode(read_buf[4]);
    time->month = BCD8421_transform_recode(read_buf[5]);
    time->year = BCD8421_transform_recode(read_buf[6]);

    ESP_LOGI(TAG, "20%d %d %d  %d  %d %d %d",time->year,time->month,time->day,time->week,time->hour,time->minute,time->second);   

}


/// @brief 设置任意闹钟的设置时间,设置完成后不会进行打开关闭闹钟操作，需要自行调用BL5372_alarm_status_set
/// @param alarm 选择闹钟，这是一个枚举类型
/// @param time 要设置的时间数据的地址,除了 时 分 其他都是无效的
/// @param cycle_plan 闹钟重复计划数据的地址
void BL5372_time_alarm_set(BL5372_alarm_select_t alarm, BL5372_time_t *time,BL5372_alarm_cycle_plan_t *cycle_plan)
{
    const char *TAG = "BL5372_time_alarm_set";

    if(!time || !cycle_plan){
        ESP_LOGE(TAG,"导入了为空的数据地址");
        return;
    }

    uint8_t write_buf[4] = {0};

    // 设置内部寄存器地址和传输配置0000
    if (alarm == BL5372_ALARM_A)
        write_buf[0] = BL5372_REG_ALARM_A_MIN << 4;
    else if (alarm == BL5372_ALARM_B)
        write_buf[0] = BL5372_REG_ALARM_B_MIN << 4;
    else
    {
        ESP_LOGE(TAG, "不存在的闹钟计时器");
        return;
    }

    // 映射写入的值
    write_buf[1] = BCD8421_transform_decode(time->minute);
    write_buf[2] = BCD8421_transform_decode(time->hour);

    for(int idx=0;idx<=6;idx++){
    bool* bit_buf;
    bit_buf = (bool*)cycle_plan + idx;
    write_buf[3] |= *bit_buf << (6 - idx); 
    }


    if(write_buf[3] == 0x00)ESP_LOGW(TAG,"设置了没有正确预定响铃周期的无效闹钟");

    esp_err_t err = i2c_master_write_to_device(DEVICE_I2C_PORT, BL5372_DEVICE_ADD, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);

    if (err != ESP_OK)
        ESP_LOGE(TAG, "与外部离线RTC通讯时发现问题 请检查电池电源连接 描述： %s", esp_err_to_name(err));
    else
    {
        if (alarm == BL5372_ALARM_A)
            ESP_LOGI(TAG, "外部离线RTC闹钟A时间已更新");
        else if (alarm == BL5372_ALARM_B)
            ESP_LOGI(TAG, "外部离线RTC闹钟B时间已更新");
    }
}

/// @brief 获取任意闹钟的设置时间
/// @param alarm 选择闹钟，这是一个枚举类型
/// @param time 保存设置时间的数据的地址,除了 时 分 其他都是无效的
/// @param cycle_plan 保存闹钟重复数据的地址
void BL5372_time_alarm_get(BL5372_alarm_select_t alarm, BL5372_time_t *time,BL5372_alarm_cycle_plan_t *cycle_plan)
{
    const char *TAG = "BL5372_time_alarm_get";

    if(!time || !cycle_plan){
        ESP_LOGE(TAG,"导入了为空的数据地址");
        return;
    }

    // 设置内部寄存器地址和传输配置0000
    uint8_t write_buf = 0;
    if (alarm == BL5372_ALARM_A)
        write_buf = BL5372_REG_ALARM_A_MIN << 4;
    else if (alarm == BL5372_ALARM_B)
        write_buf = BL5372_REG_ALARM_B_MIN << 4;
    else
    {
        ESP_LOGE(TAG, "不存在的闹钟计时器");
        return;
    }

    uint8_t read_buf[3] = {0}; // 读取缓存

    esp_err_t err = i2c_master_write_read_device(DEVICE_I2C_PORT, BL5372_DEVICE_ADD, &write_buf, sizeof(write_buf), read_buf, sizeof(read_buf), 1000 / portTICK_PERIOD_MS);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "与外部离线RTC通讯时发现问题 请检查电池电源连接 描述： %s", esp_err_to_name(err));

    time->minute = BCD8421_transform_recode(read_buf[0]);
    time->hour = BCD8421_transform_recode(read_buf[1]);

    if(read_buf[2] == 0x00)ESP_LOGW(TAG,"设置了没有正确预定响铃周期的无效闹钟");

    for(int idx=0;idx<=6;idx++){
      bool* bit_buf;
      bit_buf = (bool*)cycle_plan + idx;
      *bit_buf = (read_buf[2] << idx) & 0x40;
    }

    time->week = -1;
    time->second = -1;
    time->day = -1;
    time->month = -1;
    time->year = -1;
}

/// @brief 设置任意闹钟的打开状态
/// @param alarm 选择闹钟，这是一个枚举类型 
/// @param status 设置的状态,设置为true打开闹钟
void BL5372_alarm_status_set(BL5372_alarm_select_t alarm, bool status)
{

    const char *TAG = "BL5372_alarm_status_set";

    BL5372_cfg_t cfg_buf;
    BL5372_config_get(&cfg_buf);

    if (alarm == BL5372_ALARM_A)
        cfg_buf.alarm_A_en = status;
    else if (alarm == BL5372_ALARM_B)
        cfg_buf.alarm_B_en = status;
    else
    {
        ESP_LOGE(TAG, "不存在的闹钟计时器");
        return;
    }

    BL5372_config_set(&cfg_buf);
}


/// @brief 获取任意闹钟的打开状态
/// @param alarm 选择闹钟，这是一个枚举类型
/// @return 闹钟的状态，true为闹钟打开
bool BL5372_alarm_status_get(BL5372_alarm_select_t alarm)
{
    const char *TAG = "BL5372_alarm_status_get";

    BL5372_cfg_t cfg_buf;
    BL5372_config_get(&cfg_buf);

    if (alarm == BL5372_ALARM_A)
        return cfg_buf.alarm_A_en;
    else if (alarm == BL5372_ALARM_B)
        return cfg_buf.alarm_B_en;
    else
    {
        ESP_LOGE(TAG, "不存在的闹钟计时器");
        return cfg_buf.alarm_B_en;
    }
}

/// @brief 现在选择的闹钟是否在响铃
/// @param alarm 选择闹钟，这是一个枚举类型
/// @return true表示正在响铃
bool BL5372_alarm_is_ringing(BL5372_alarm_select_t alarm){
    const char *TAG = "BL5372_alarm_is_ringing";

    BL5372_cfg_t cfg_buf;
    BL5372_config_get(&cfg_buf);

    if (alarm == BL5372_ALARM_A)
        return cfg_buf.alarm_A_out_flag_or_out_keep;
    else if (alarm == BL5372_ALARM_B)
        return cfg_buf.alarm_B_out_flag_or_out_keep;
    else
    {
        ESP_LOGE(TAG, "不存在的闹钟计时器");
        return false;
    }  
}

/// @brief 让选择的闹钟停止响铃,但是不会关闭闹钟，需要自行调用BL5372_alarm_status_set
/// @param alarm 选择闹钟，这是一个枚举类型
void BL5372_alarm_stop_ringing(BL5372_alarm_select_t alarm){
   
    const char *TAG = "BL5372_alarm_stop_ringing";

    BL5372_cfg_t cfg_buf;
    BL5372_config_get(&cfg_buf);

    if (alarm == BL5372_ALARM_A)
        cfg_buf.alarm_A_out_flag_or_out_keep = false;
    else if (alarm == BL5372_ALARM_B)
        cfg_buf.alarm_B_out_flag_or_out_keep = false;
    else
    {
        ESP_LOGE(TAG, "不存在的闹钟计时器");
        return;
    }

    BL5372_config_set(&cfg_buf); 
}

// 以默认配置初始化BL5372的运行配置
void BL5372_config_init()
{
    BL5372_cfg_t cfg_buf = BL5372_DEFAULT_INIT_CONFIG;
    BL5372_config_set(&cfg_buf);
}


/// @brief 设置BL5372的运行配置
/// @param rtc_cfg 作为运行配置的数据的地址
void BL5372_config_set(BL5372_cfg_t *rtc_cfg)
{
    const char *TAG = "BL5372_config_set";

    if(!rtc_cfg){
        ESP_LOGE(TAG,"导入了为空的数据地址");
        return;
    }

    uint8_t write_buf[3] = {0}; // 写入缓存

    // 设置内部寄存器地址和传输配置0000
    write_buf[0] = BL5372_REG_CTRL_1 << 4;
    // 映射写入的值
    write_buf[1] |= rtc_cfg->INT_module_mode;
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

    esp_err_t err = i2c_master_write_to_device(DEVICE_I2C_PORT, BL5372_DEVICE_ADD, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);

    if (err != ESP_OK)
        ESP_LOGE(TAG, "与外部离线RTC通讯时发现问题 请检查电池电源连接 描述： %s", esp_err_to_name(err));
    else
        ESP_LOGI(TAG, "外部离线RTC配置已更新");
}



/// @brief 获取BL5372的运行配置
/// @param rtc_cfg 保存运行配置的数据的地址
void BL5372_config_get(BL5372_cfg_t *rtc_cfg)
{

    const char *TAG = "BL5372_config_get";

    if(!rtc_cfg){
        ESP_LOGE(TAG,"导入了为空的数据地址");
        return;
    }

    uint8_t read_buf[2] = {0};                  // 读取缓存
    uint8_t write_buf = BL5372_REG_CTRL_1 << 4; // 高4位设置起始地址，低4位设置传输模式0000
    esp_err_t err = i2c_master_write_read_device(DEVICE_I2C_PORT, BL5372_DEVICE_ADD, &write_buf, sizeof(write_buf), read_buf, sizeof(read_buf), 1000 / portTICK_PERIOD_MS);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "与外部离线RTC通讯时发现问题 请检查电池电源连接 描述： %s", esp_err_to_name(err));

    // 映射得到的值
    rtc_cfg->alarm_A_en = 0x01 & (read_buf[0] >> 7);
    rtc_cfg->alarm_B_en = 0x01 & (read_buf[0] >> 6);
    rtc_cfg->intr_out_mode = 0x03 & (read_buf[0] >> 4);
    rtc_cfg->test_en = 0x01 & (read_buf[0] >> 3);
    rtc_cfg->INT_module_mode = 0x07 & read_buf[0];
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
    uint8_t dat_buf1 = dec_dat / 10;            // 十位转换为8bits二进制高四位
    uint8_t dat_buf0 = dec_dat - dat_buf1 * 10; // 个位转换为8bits二进制低四位
    uint8_t bin_buf = (dat_buf1 << 4) | dat_buf0;
    return bin_buf;
}





