// 该文件由701Enti整合，包含一些sevetest30的  低功耗蓝牙BLE环境中  数据获取（BWEDA）以及其他设备的蓝牙交互活动
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：有效的数据存储变量都封装在该库下，不需要在外部函数定义一个数据结构体缓存作为参数，直接读取公共变量，主要为了方便FreeRTOS的任务支持
// 敬告：ESP32S3目前只支持BLE,不支持经典蓝牙的音频传输，这里没有音频通讯的功能
// 敬告：蓝牙配置操作使用了ESP-IDF官方例程并进行修改，非常感谢,
// 敬告：以下注释为个人理解，不是实际意义
// SIG官方提供的包含外观特征值 UUID 等定义的文档链接 https://www.bluetooth.com/specifications/assigned-numbers/
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.11.5

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