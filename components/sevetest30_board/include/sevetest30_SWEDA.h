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

// 该文件归属701Enti组织，主要由SEVETEST30开发团队维护，包含一些sevetest30的 离线环境 数据获取（SWEDA）
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：有效的数据存储变量都封装在该库下，不需要在外部函数定义一个数据结构体缓存作为参数，直接读取公共变量，主要为了方便FreeRTOS的任务支持
//       该文件对于硬件的配置针对sevetest30,使用前请参考兼容性问题
//       文件本体不包含i2c通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行,库中有仅为字库SPI通讯提供的SPI配置函数
// github: https://github.com/701Enti
// bilibili: 701Enti

#ifndef _SEVETEST30_SWEDA_H_
#define _SEVETEST30_SWEDA_H_
#endif

#include <stdio.h>
#include <stdbool.h>
#include "board_def.h"
#include "AHT21.h"
#include "BL5372.h"
#include "lsm6ds3trc.h"



typedef struct systemtime_t{
  int year;
  int month;
  int day;
  int week;
  int hour;
  int minute;
  int second;
}systemtime_t;

typedef struct battery_data_t{
  bool charge_flag;//正在充电标识(正在充电为 true)
  bool finished_flag;//充电完成标识(完成充电为 true)
}battery_data_t;


typedef AHT21_result_handle_t env_temp_hum_data_t;

extern systemtime_t systemtime_data;
extern battery_data_t battery_data;
extern env_temp_hum_data_t env_temp_hum_data;

extern uint8_t IMU_Gx_L[IMU_FIFO_DEFAULT_READ_NUM];
extern uint8_t IMU_Gx_H[IMU_FIFO_DEFAULT_READ_NUM];
extern uint8_t IMU_Gy_L[IMU_FIFO_DEFAULT_READ_NUM];
extern uint8_t IMU_Gy_H[IMU_FIFO_DEFAULT_READ_NUM];
extern uint8_t IMU_Gz_L[IMU_FIFO_DEFAULT_READ_NUM];
extern uint8_t IMU_Gz_H[IMU_FIFO_DEFAULT_READ_NUM];
extern uint8_t IMU_XLx_L[IMU_FIFO_DEFAULT_READ_NUM];
extern uint8_t IMU_XLx_H[IMU_FIFO_DEFAULT_READ_NUM];
extern uint8_t IMU_XLy_L[IMU_FIFO_DEFAULT_READ_NUM];
extern uint8_t IMU_XLy_H[IMU_FIFO_DEFAULT_READ_NUM];
extern uint8_t IMU_XLz_L[IMU_FIFO_DEFAULT_READ_NUM];
extern uint8_t IMU_XLz_H[IMU_FIFO_DEFAULT_READ_NUM];



void refresh_battery_data();
void refresh_systemtime_data();

esp_err_t refresh_IMU_FIFO_data(IMU_reg_mapping_t* FIFO_database,int map_num,int read_num);


void start_ext_rtc_alarm(BL5372_alarm_select_t alarm, systemtime_t *time, BL5372_alarm_cycle_plan_t *cycle_plan);



void sync_systemtime_to_ext_rtc();

void sync_systemtime_from_ext_rtc();