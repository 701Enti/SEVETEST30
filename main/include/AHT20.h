
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

 // 包含各种SE30对温湿度传感器 AHT20的支持
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // 敬告：文件本体不包含i2c通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行
 // AHT20的CRC校验计算支持,来自奥松电子官方的实例程序,非常感谢 http://www.aosong.com/products-99.html
 // github: https://github.com/701Enti
 // bilibili: 701Enti

#pragma once

#include "esp_err.h"
#include "sevetest30_config.h"

#define AHT20_DEVICE_ADD 0x38 //AHT20通讯地址
#define AHT20_I2C_PORT    (DEVICE_I2C_PORT)//AHT20通讯端口
#define AHT20_I2C_FREQ_HZ 10*1000 //AHT20通讯频率
#define AHT20_I2C_TIMEOUT_MS 1000 //AHT20通讯超时时间(单位ms)

typedef struct AHT20_result_t
{
  double temperature;//温度,摄氏度
  double humidity;//湿度,%RH
}AHT20_result_t;

esp_err_t AHT20_init();

void AHT20_trigger();

uint8_t AHT20_get_status();

void AHT20_get_result(AHT20_result_t* dest);


