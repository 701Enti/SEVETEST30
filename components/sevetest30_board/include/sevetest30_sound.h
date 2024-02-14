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

#define MUSIC_PLAY_EVT_TASK_CORE (0)
#define MUSIC_PLAY_EVT_TASK_STACK_SIZE (8 * 1024)

#define MUSIC_MP3_DECODER_TASK_CORE (1)

#define VAD_FRAME_LENGTH 30// VAD 帧时长(ms)

#define ASR_EVT_TASK_CORE (0)
#define ASR_EVT_TASK_STACK_SIZE (8*1024)

#define ASR_HTTP_RESPONSE_BUF_MAX 1024
#define ASR_FRAME_LENGTH 300//识别数据帧时长(ms)
#define ASR_TIMEOUT_MS  30000//识别超时时长(ms)

#define BAIDU_TTS_ENDPOINT "http://tsn.baidu.com/text2audio"//百度TTS url
#define BAIDU_ASR_URL      "http://vop.baidu.com/server_api" //百度ASR url
#define BAIDU_ASR_PRO_URL  "http://vop.baidu.com/pro_api"//百度ASR极速版 url




//百度TTS设定配置，注释参考了官方API文档
typedef struct baidu_TTS_cfg_t
{
  char *tex;   // 必填	合成的文本，使用UTF-8编码。不超过60个汉字或者字母数字
  uint8_t spd; // 选填	语速，取值0-15，默认为5中语速
  uint8_t pit; // 选填	音调，取值0-15，默认为5中语调
  uint8_t vol; // 选填	音量，取值0-15
  int per;     // 选填	度小宇=1，度小美=0，度逍遥（基础）=3，度丫丫=4  //  度逍遥（精品）=5003，度小鹿=5118，度博文=106，度小童=110，度小萌=111，度米朵=103，度小娇=5
} baidu_TTS_cfg_t;
#define BAIDU_TTS_DEFAULT_CONFIG(text, person) \
  {                                            \
    .tex = text,                               \
    .spd = 5,                                  \
    .pit = 5,                                  \
    .vol = 15,                                 \
    .per = person,                             \
  }

typedef enum
{
  ASR_RATE_8K = 8000,
  ASR_RATE_16K = 16000,
}asr_rate_t;//识别采样率

typedef enum
{
  ASR_PID_CM_NEAR_PRO = 80001, //近场 中文-普通话 极速版
  ASR_PID_CM_NEAR = 1537,//近场 中文-普通话 标准版
  ASR_PID_CM_FAR  = 1936,//远场 中文-普通话 标准版
  ASR_PID_CC = 1637,//中文-粤语 
  ASR_PID_CS = 1837,//中文-四川话
  ASR_PID_EN = 1737,//英语
}asr_pid_t;//识别模型

//百度ASR设定配置，注释参考了官方API文档
typedef struct baidu_ASR_cfg_t
{
  asr_rate_t   rate;        // 采样速率，目前填入 16000 \ 8000
  asr_pid_t    dev_pid;    //识别模型
  int  stop_threshold;   // 停止延时长度，在经过stop_threshold个周期没有监测到语音，暂停录音
  int  send_threshold;    //发送阈值，连续send_threshold个周期监测到语音,暂停录音后，这段语音将被发送处理
  int  record_save_times_max;          //最大录制record_save_times_max帧数据后强制发送
}baidu_ASR_cfg_t;



extern bool volatile sevetest30_music_running_flag;
extern bool volatile sevetest30_asr_running_flag;

void element_cfg_data_reset();

esp_err_t audio_element_all_init(const char *link_tag[], int link_num);

// 外部功能函数
void tts_service_play(baidu_TTS_cfg_t* tts_cfg, UBaseType_t priority);
void music_url_play(const char *url, UBaseType_t priority);
void asr_service_start(baidu_ASR_cfg_t* asr_cfg,UBaseType_t priority);
