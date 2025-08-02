
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

 // 包含一些sevetest30的  音频数据获取与硬件调度，以支持TTS,语音识别，百度文心一言ERNIE Bot 4.0对话支持 音乐API播放音乐时的硬件驱动等工作
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // 敬告：参考了官方提供的pipeline_baidu_speech_mp3例程,非常感谢ESPRESSIF
 // github: https://github.com/701Enti
 // bilibili: 701Enti
#pragma once

#include "stdbool.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "audio_event_iface.h"

//元素配置
#define ELEMENT_MP3_DECODER_TASK_CORE (1) //运行在的CPU核心-mp3编码器任务(音频元素)
#define ELEMENT_MP3_DECODER_TASK_STACK_SIZE (4 * 1024) //运行堆栈大小(Byte)-mp3编码器任务(音频元素)
#define ELEMENT_MP3_DECODER_RINGBUFFER_SIZE (4 * 1024) //循环缓冲区大小(Byte)-mp3编码器(音频元素)

#define ELEMENT_RAW_STREAM_RINGBUFFER_SIZE (8 * 1024) //循环缓冲区大小(Byte)-RAW原始音频流(音频元素)

//MUSIC_PLAY-音频播放功能
#define MUSIC_PLAY_EVT_TASK_CORE (1) //运行在的CPU核心-音频播放功能的事件监听和处理任务
#define MUSIC_PLAY_EVT_TASK_STACK_SIZE (4 * 1024) //运行堆栈大小(Byte)-音频播放功能的事件监听和处理任务

//ASR-自动语音识别功能
#define ASR_TIMEOUT_MS  10000//说话停顿超时时长(ms)-说话停顿超过该时长后认为话说完了,停止识别语音-语音识别功能的说话检测
#define ASR_HTTP_RESPONSE_BUF_MAX 1024 //缓存大小(Byte)-语音识别功能的http响应结果的缓存大小
#define ASR_EVT_TASK_CORE (0) //运行在的CPU核心-语音识别功能的事件监听和处理任务
#define ASR_EVT_TASK_STACK_SIZE (3*1024) //运行堆栈大小(Byte)-音频播放功能的事件监听和处理任务
#define VAD_FRAME_LENGTH 30// VAD 帧时长(ms)-语音识别功能的说话检测
#define ASR_FRAME_LENGTH 300//识别数据帧时长(ms)-语音识别功能的语音数据打包
#define BAIDU_ASR_URL      "http://vop.baidu.com/server_api" //百度ASR uri
#define BAIDU_ASR_PRO_URL  "http://vop.baidu.com/pro_api"//百度ASR极速版 uri

//TTS-文本转语音(语音合成)功能
#define BAIDU_TTS_ENDPOINT "http://tsn.baidu.com/text2audio"//百度TTS uri





//百度TTS设定配置，注释参考了官方API文档
typedef struct baidu_TTS_cfg_t
{
  char* tex;   // 必填	合成的文本，使用UTF-8编码。不超过60个汉字或者字母数字
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
  ASR_PID_CM_FAR = 1936,//远场 中文-普通话 标准版
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

extern int volatile running_i2s_port;//运行的I2S配置
extern bool volatile sevetest30_music_running_flag;//音乐播放/TTS语音合成运行标志
extern bool volatile sevetest30_asr_running_flag;//语音识别运行标志


// 外部通用功能运行
void tts_service_play(baidu_TTS_cfg_t* tts_cfg, UBaseType_t priority);
void music_uri_or_url_play(const char* uri, UBaseType_t priority);
void asr_service_begin(baidu_ASR_cfg_t* asr_cfg, UBaseType_t priority);

//外部扩展功能API
uint64_t mp3_decoder_play_time_get();
