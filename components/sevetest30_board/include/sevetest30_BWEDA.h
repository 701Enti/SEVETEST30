
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

// 该文件归属701Enti组织，SEVETEST30开发团队应该提供责任性维护，包含一些sevetest30的  低功耗蓝牙BLE环境中  数据获取（BWEDA）以及其他设备的蓝牙交互活动
// 蓝牙配置使用了ESP-IDF官方例程并进行修改，ESP-IDF项目地址 https://github.com/espressif/esp-idf 
// 使用的例程地址 https://github.com/espressif/esp-idf/tree/release/v4.4/examples/bluetooth/bluedroid/ble/gatt_server_service_table
// 此处修改了 服务属性数据库 服务数量 以及 gatts_event_handler回调 根据某个成员的理解添加了中文注释 添加 外部源文件board_ctrl的控制数据联动 提供了蓝牙数据存储与全局控制API的对接思路
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：有效的数据存储变量都封装在该库下，不需要在外部函数定义一个数据结构体缓存作为参数，直接读取公共变量，主要为了方便FreeRTOS的任务支持
// 敬告：ESP32S3目前只支持BLE,不支持经典蓝牙的音频传输，这里没有音频通讯的功能
// 敬告：蓝牙配置操作使用了ESP-IDF官方例程并进行修改，非常感谢
// 敬告：以下注释为某个成员的理解，不是实际意义
// SIG官方提供的包含外观特征值 UUID 等定义的文档链接(2.6.2节-外观特征值 3.4.2节-UUID) https://www.bluetooth.com/specifications/assigned-numbers/
// github: https://github.com/701Enti
// bilibili: 701Enti

#ifndef _SEVETEST30_BWEDA_H_
#define _SEVETEST30_BWEDA_H_
#endif

#include <esp_err.h>

//蓝牙配置
#define BLE_DEVICE_NAME               (SEVETEST30_BLE_DEVICE_NAME) //蓝牙设备名称
#define BLE_DEVICE_APPEARANCE_VALUE   (SEVETEST30_BLE_DEVICE_APPEARANCE_VALUE)     //蓝牙设备外观特征值

#define BLE_CONNECT_SLAVE_LATENCY       0          //从设备连接延迟事件数，在IOS系统中最大限制为4，安卓系统需最大限制根据版本和制造商
#define BLE_CONNECT_MIN_INTERVAL        0x10       //最小连接间隔 0x10 *1.25ms = 20ms,不同系统存在限制规范
#define BLE_CONNECT_MAX_INTERVAL        0x20       //最大连接间隔  0x20*1.25ms = 40ms,不同系统存在限制规范
#define BLE_CONNECT_TIMEOUT             400        //连接超时       400*10ms = 4000ms

#define BLE_GATTS_CHAR_VAL_LEN_MAX      500        //特征值存储最大长度
#define BLE_PREPARE_BUF_SIZE_MAX        256       //写入准备缓存最大大小
#define BLE_LOCAL_MTU                  (SEVETEST30_BLE_LOCAL_MTU)       //本地最大可传输单元MTU限制大小

//完整服务UUID,包含 基本UUID + 16bits服务UUID 
#define BLE_SERVICE_UUID_BASE(UUID_16BITS){0xfb,0x34,0x9b,0x5f,0x80,0x00,0x00,0x80,0x00,0x10,0x00,0x00,UUID_16BITS,(UUID_16BITS >> 8),0x00,0x00,}//顺序 第128位 <----------- 第1位


esp_err_t bluetooth_connect();

void sevetest30_ble_attr_value_push();