// 该文件由701Enti整合，包含一些sevetest30的  蓝牙环境中  数据获取（BWEDA）以及其他设备的蓝牙交互活动
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：有效的数据存储变量都封装在该库下，不需要在外部函数定义一个数据结构体缓存作为参数，直接读取公共变量，主要为了方便FreeRTOS的任务支持
// 敬告：ESP32S3目前只支持BLE,不支持经典蓝牙的音频传输，这里没有音频通讯的功能
// 敬告：蓝牙配置操作使用了ESP-IDF官方例程并进行修改，非常感谢
// SIG官方提供的包含外观特征值 UUID 等定义的文档链接 https://www.bluetooth.com/specifications/assigned-numbers/
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.11.5

#ifndef _SEVETEST30_BWEDA_H_
#define _SEVETEST30_BWEDA_H_
#endif

/* Attributes State Machine */
enum
{
    IDX_SVC,
    IDX_CHAR_A,
    IDX_CHAR_VAL_A,
    IDX_CHAR_CFG_A,

    IDX_CHAR_B,
    IDX_CHAR_VAL_B,

    IDX_CHAR_C,
    IDX_CHAR_VAL_C,

    HRS_IDX_NB,
};


void bluetooth_connect();