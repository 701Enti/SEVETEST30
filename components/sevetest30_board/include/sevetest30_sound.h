// 该文件由701Enti编写，包含一些sevetest30的  音频数据获取与硬件调度，以支持TTS,语音识别，网易云音乐API播放音乐时的硬件驱动等工作
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.12.17

#pragma once

#include "stdbool.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "audio_event_iface.h"

#define MUSIC_PLAY_EVT_TASK_STACK_SIZE    (8* 1024)
#define MUSIC_PLAY_EVT_TASK_CORE          (1)

typedef struct baidu_TTS_cfg_t
{
  
}baidu_TTS_cfg_t;

extern bool sevetest30_music_running_flag;


///@brief 重置元素配置数据到默认值，之后您可以针对性修改某些参数，接着运行元素配置函数
void element_cfg_data_reset();

/// @brief 以现在的存储的配置初始化所有音频元素
void audio_element_all_init();


void http_i2s_mp3_music_start(const char* uri,TaskFunction_t running_event,UBaseType_t priority);


void NetEase_music_uri_play(const char* uri,UBaseType_t priority);

