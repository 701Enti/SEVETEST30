/*
 * 701Enti MIT License
 *
 * Copyright © 2026 <701Enti organization>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the “Software”),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// 对电池电量计量传感器MAX17048的支持
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体不包含I2C通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行

#pragma once

#include "esp_err.h"
#include "stdbool.h"
#include "sevetest30_config.h"

#define MAX17048_DEVICE_ADD     0x36//MAX17048通讯地址
#define MAX17048_I2C_PORT    (DEVICE_I2C_PORT)//MAX17048通讯端口
#define MAX17048_I2C_FREQ_HZ 10*1000 //MAX17048通讯频率
#define MAX17048_I2C_TIMEOUT_MS 1000 //MAX17048通讯超时时间(单位ms)

typedef struct MAX17048_result_t {
  float battery_soc;//电池电量百分比(0-100,-1表示获取失败)
  float battery_voltage;//电池电压(单位mV,-1表示获取失败)
}MAX17048_result_t;


esp_err_t MAX17048_init();

void MAX17048_result_fetch(MAX17048_result_t* dest);
