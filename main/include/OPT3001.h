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

// 对环境光传感器OPT3001的支持
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体不包含I2C通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行

#pragma once

#include "esp_err.h"
#include "stdbool.h"
#include "sevetest30_config.h"

#define OPT3001_DEVICE_ADD     0x44//OPT3001通讯地址
#define OPT3001_I2C_PORT    (DEVICE_I2C_PORT)//OPT3001通讯端口
#define OPT3001_I2C_FREQ_HZ 10*1000 //OPT3001通讯频率
#define OPT3001_I2C_TIMEOUT_MS 1000 //OPT3001通讯超时时间(单位ms)

#define OPT3001_DEFAULT_CONFIG_REG_VALUE 0x0710//OPT3001配置寄存器值

esp_err_t OPT3001_init();

float OPT3001_fetch_lux();
