// 该文件由701Enti编写，包含一些sevetest30的  蓝牙环境中  数据获取（BWEDA）
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：ESP32S3目前只支持BLE,不支持经典蓝牙的音频传输，这里没有音频通讯的功能
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.11.5

#include "sevetest30_BWEDA.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_common.h"
#include "i2s_stream.h"
#include "board.h"
#include "audio_mem.h"
#include "board_def.h"
#include "bluetooth_service.h"
