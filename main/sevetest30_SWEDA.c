
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

 // 包含一些sevetest30的 离线环境 数据获取（SWEDA）
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // 敬告：有效的数据存储变量都封装在该库下，不需要在外部函数定义一个数据结构体缓存作为参数，直接读取公共变量，主要为了方便FreeRTOS的任务支持
 //       该文件对于硬件的配置针对sevetest30,使用前请参考兼容性问题
 //       文件本体不包含i2c通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行,库中有仅为字库SPI通讯提供的SPI配置函数
 // github: https://github.com/701Enti
 // bilibili: 701Enti

#include "sevetest30_SWEDA.h"
#include "esp_log.h"
#include "time.h"
#include "sevetest30_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

systemtime_t systemtime_data = { 0 };
battery_data_t battery_data = { 0 };
env_temp_hum_data_t env_temp_hum_data = { 0 };
env_TVOC_data_t env_TVOC_data = { 0 };

uint8_t IMU_Gx_L[IMU_FIFO_DEFAULT_READ_NUM] = { 0 };
uint8_t IMU_Gx_H[IMU_FIFO_DEFAULT_READ_NUM] = { 0 };
uint8_t IMU_Gy_L[IMU_FIFO_DEFAULT_READ_NUM] = { 0 };
uint8_t IMU_Gy_H[IMU_FIFO_DEFAULT_READ_NUM] = { 0 };
uint8_t IMU_Gz_L[IMU_FIFO_DEFAULT_READ_NUM] = { 0 };
uint8_t IMU_Gz_H[IMU_FIFO_DEFAULT_READ_NUM] = { 0 };
uint8_t IMU_XLx_L[IMU_FIFO_DEFAULT_READ_NUM] = { 0 };
uint8_t IMU_XLx_H[IMU_FIFO_DEFAULT_READ_NUM] = { 0 };
uint8_t IMU_XLy_L[IMU_FIFO_DEFAULT_READ_NUM] = { 0 };
uint8_t IMU_XLy_H[IMU_FIFO_DEFAULT_READ_NUM] = { 0 };
uint8_t IMU_XLz_L[IMU_FIFO_DEFAULT_READ_NUM] = { 0 };
uint8_t IMU_XLz_H[IMU_FIFO_DEFAULT_READ_NUM] = { 0 };

/********************************全局数据刷新函数 数据保存至全局变量************************************/

/// @brief 初始化本地时区
void init_timezone()
{
    setenv("TZ", CONFIG_LOCAL_TZ, 1);
    tzset();
}


/// @brief 刷新系统时间数据，保存至全局变量
void refresh_systemtime_data()
{
    static const char* TAG = "refresh_time_data";
    time_t time_sec = time(NULL);
    struct tm time_info;

    // 读取本地时区时间
    if (localtime_r(&time_sec, &time_info) == NULL)
    {
        ESP_LOGE(TAG, "localtime_r get time failed");
        return;
    }

    // 直接映射数字，无字符串、无switch
    systemtime_data.year  = time_info.tm_year + 1900;
    systemtime_data.month = time_info.tm_mon + 1; // 0~11 → 1~12
    systemtime_data.day   = time_info.tm_mday;
    systemtime_data.hour  = time_info.tm_hour;
    systemtime_data.minute= time_info.tm_min;
    systemtime_data.second= time_info.tm_sec;

    // 0=周日，1=周一，2=周二 ... 6=周六，原生值直接存
    systemtime_data.week  = time_info.tm_wday;
}

/// @brief 刷新缓存的电池数据，充电状态，该函数需要频繁调用
/// @brief 数据保存至全局变量
void refresh_battery_data()
{
  const char* TAG = "refresh_battery_data";
  // 电池数据请求
  // 充电状态
}

/// @brief 刷新当前环境的温度湿度数据,使用硬件传感器
/// @param crc_flag 启用CRC校验
void refresh_env_temp_hum_data() {
  AHT20_trigger();
  vTaskDelay(pdMS_TO_TICKS(AHT20_DEFAULT_MEASURE_DELAY));
  AHT20_get_result(&env_temp_hum_data);
}

/// @brief 刷新当前环境的空气质量数据,使用硬件传感器
/// @param crc_flag 启用CRC校验
void refresh_env_TVOC_data(bool crc_flag) {
  env_TVOC_data.flag_crc = crc_flag;
  AGS10_TVOC_result_get(&env_TVOC_data);
}

/// @brief 刷新姿态传感器FIFO抽取后的数据,有(默认方式)和(自定义方式)
/// @brief 数据保存至全局变量(默认方式) / 数据保存至FIFO_database预设内存区域(自定义方式)
/// @param FIFO_database [填写参数为 NULL 使用默认数据库(默认方式)] / 导入自定义的FIFO映射数据库(自定义方式)
/// @param map_num  [使用默认数据库,忽略这个参数(默认方式)] / (必须准确)数据库的条目数量即MAP_BASE个数(自定义方式)
/// @param read_num [使用默认数据库,忽略这个参数(默认方式)] / (必须准确)读取的FIFO数据帧个数,一帧FIFO数据往往包含多个传感器的数据(自定义方式)
/// @return ESP_OK / ESP_FAIL
esp_err_t refresh_IMU_FIFO_data(IMU_reg_mapping_t* FIFO_database, int map_num, int read_num)
{
  if (FIFO_database == NULL) {
    IMU_reg_mapping_t default_database[IMU_DEFAULT_FIFO_MAPPING_DATABASE_MAP_NUM] = IMU_DEFAULT_FIFO_MAPPING_DATABASE(IMU_Gx_L, IMU_Gx_H, IMU_Gy_L, IMU_Gy_H, IMU_Gz_L, IMU_Gz_H, IMU_XLx_L, IMU_XLx_H, IMU_XLy_L, IMU_XLy_H, IMU_XLz_L, IMU_XLz_H);
    map_num = IMU_DEFAULT_FIFO_MAPPING_DATABASE_MAP_NUM;
    read_num = IMU_FIFO_DEFAULT_READ_NUM;
    return LSM6DS3TRC_FIFO_map(default_database, map_num, read_num);
  }
  else
    return LSM6DS3TRC_FIFO_map(FIFO_database, map_num, read_num);
}


// /********************************硬件操作函数************************************/

// /// @brief 启动外部离线RTC闹钟,时间和配置在电池接入正常运行时掉电保存
// /// @param alarm 选择要设置的闹钟,这是一个枚举类型
// /// @param time 要设置的时间数据的地址,除了 时 分 其他都是无效的
// /// @param cycle_plan 要设置的重复计划的数据的地址
// void start_ext_rtc_alarm(BL5372_alarm_select_t alarm, systemtime_t* time, BL5372_alarm_cycle_plan_t* cycle_plan)
// {
//   // 设置之前初始化闹钟的状态
//   BL5372_alarm_stop_ringing(alarm);
//   BL5372_alarm_status_set(alarm, false);
//   // 缓存设定的时间
//   BL5372_time_t time_buf;
//   time_buf.week = time->week;
//   time_buf.hour = time->hour;
//   time_buf.minute = time->minute;
//   // 设置闹钟时间
//   BL5372_time_alarm_set(alarm, &time_buf, cycle_plan);
//   BL5372_alarm_status_set(alarm, true); // 打开闹钟
// }

// /********************************数据同步函数************************************/
// ///@brief 同步系统实时时间到外部RTC
// void sync_systemtime_to_ext_rtc()
// {
//   refresh_systemtime_data();
//   // 缓存设定的时间
//   BL5372_time_t time_buf;
//   time_buf.year = systemtime_data.year - 2000; /// 舍去前两位，如2024年存储24
//   time_buf.month = systemtime_data.month;
//   time_buf.day = systemtime_data.day;
//   time_buf.week = systemtime_data.week;
//   time_buf.hour = systemtime_data.hour;
//   time_buf.minute = systemtime_data.minute;
//   time_buf.second = systemtime_data.second;
//   // 执行同步
//   BL5372_time_now_set(&time_buf);
// }

// ///@brief 从外部RTC获取时间数据，同步系统实时时间，这往往是无网络连接下的同步选择
// void sync_systemtime_from_ext_rtc()
// {
//   //从外部RTC获取时间数据
//   BL5372_time_t time_buf;
//   BL5372_time_now_get(&time_buf);
//   // 使用标准C库函数来进行操作和设置
//   time_t time_sec; //时间戳缓存（从Epoch(1970-01-01 00:00:00 UTC)开始的秒数）
//   time(&time_sec); // 计算当前日历时间，并转换为标准time_t类型
//   struct tm time_info;
//   // 设定本地时区
//   setenv("TZ", CONFIG_LOCAL_TZ, 1);
//   tzset();
//   gmtime_r(&time_sec, &time_info);//通过时间戳time_sec读取本地时间到time_info
//   //修改时间值
//   time_info.tm_year = time_buf.year + 100;//time_buf.year为0时实际表示2000年，而tm_year为100时表示2000年
//   time_info.tm_mon = time_buf.month - 1;//范围为0 - 11
//   time_info.tm_mday = time_buf.day;
//   time_info.tm_wday = time_buf.week;
//   time_info.tm_hour = time_buf.hour;
//   time_info.tm_min = time_buf.minute;
//   time_info.tm_sec = time_buf.second;
//   //获取修改结果时间戳
//   time_sec = mktime(&time_info);
//   //设置系统时间
//   struct timeval time_now = { .tv_sec = time_sec };
//   settimeofday(&time_now, NULL);
// }

