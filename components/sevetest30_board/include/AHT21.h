
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

 // 包含各种SE30对温湿度传感器 AHT21的支持
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // 敬告：文件本体不包含i2c通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行
 // AHT21的CRC校验计算支持,来自奥松电子官方的实例程序,非常感谢 http://www.aosong.com/products-99.html
 // github: https://github.com/701Enti
 // bilibili: 701Enti

#pragma once

#include "esp_types.h"
#include "esp_err.h"

// I2C相关配置宏定义在board_def.h下
#define AHT21_DEVICE_ADD 0x38

#define AHT21_STATUS_GET_COMMAND 0x71 //获取状态字命令
#define AHT21_TRIGGER_COMMAND 0xAC //触发读取命令

typedef struct AHT21_result_t
{
  bool  flag_crc;//启用CRC校验
  bool  data_true;//数据有效(无效的话可能是数据读取失败或者CRC校验后发现问题)
  float temp;//温度,摄氏度
  float hum;//湿度,%RH
}AHT21_result_t;

esp_err_t AHT21_begin();

void AHT21_trigger();

uint8_t AHT21_get_status();

void AHT21_get_result(AHT21_result_t* dest);


