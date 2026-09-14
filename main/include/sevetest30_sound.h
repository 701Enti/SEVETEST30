
/*
 * 701Enti MIT License
 *
 * Copyright © 2024 <701Enti organization>
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

// 包含一些sevetest30的
// 音频数据获取与硬件调度，以支持TTS,语音识别，百度文心一言ERNIE Bot 4.0对话支持
// 音乐API播放音乐时的硬件驱动等工作
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：参考了官方提供的pipeline_baidu_speech_mp3例程,非常感谢ESPRESSIF
// github: https://github.com/701Enti

#pragma once

#include "audio_element.h"
#include "esp_vad.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdbool.h"

#define ELEMENT_MP3_DECODER_TASK_CORE (0)

// 运行堆栈大小(Byte)-mp3编码器任务(音频元素)
#define ELEMENT_MP3_DECODER_TASK_STACK_SIZE (4 * 1024)

// 循环缓冲区大小(Byte)-mp3编码器(音频元素)
#define ELEMENT_MP3_DECODER_RINGBUFFER_SIZE (8 * 1024)

// 循环缓冲区大小(Byte)-I2S(音频元素)
#define ELEMENT_I2S_STREAM_RINGBUFFER_SIZE (8 * 1024)

// 循环缓冲区大小(Byte)-HTTP音频流(音频元素)
#define ELEMENT_HTTP_STREAM_RINGBUFFER_SIZE (64 * 1024)

// 循环缓冲区大小(Byte)-RAW原始音频流(音频元素)
#define ELEMENT_RAW_STREAM_RINGBUFFER_SIZE (8 * 1024)

// 音频播放功能
#define MUSIC_EVT_TASK_CORE (0)
#define MUSIC_EVT_TASK_STACK_SIZE (4 * 1024)

// ASR-自动语音识别功能

// 说话停顿超时时长(ms)-说话停顿超过该时长后认为话说完了,停止识别语音-语音识别功能的说话检测
#define ASR_TIMEOUT_MS 10000

// 缓存大小(Byte)-语音识别功能的http响应结果的缓存大小
#define ASR_HTTP_RESPONSE_BUF_MAX 1024

// 运行在的CPU核心-语音识别功能的事件监听和处理任务
#define ASR_EVT_TASK_CORE (0)

// 运行堆栈大小(Byte)-语音识别功能的事件监听和处理任务
#define ASR_EVT_TASK_STACK_SIZE (4 * 1024)

// 识别数据帧时长(ms)-语音识别功能的语音数据打包时长
#define ASR_FRAME_LENGTH 1000

// 百度ASR-语音识别
#define BAIDU_ASR_URL "http://vop.baidu.com/server_api"

// 百度ASR极速版-语音识别
#define BAIDU_ASR_PRO_URL "http://vop.baidu.com/pro_api"

// 百度TTS-文本转语音(语音合成)
#define BAIDU_SHORT_TTS_ENDPOINT "http://tsn.baidu.com/text2audio"

// 百度TTS-文本转语音(语音合成)-最大允许strlen()数值
// (用于限制文本长度,超过直接取消启动TTS,不会截断,防止API报错,短文本合成有字数限制)
#define BAIDU_SHORT_TTS_TEXT_STRLEN_MAX 512

// 记录当前I2S总线音频数据，使用自定义element -
// current_sound_collecter嵌入pipeline读取 格式为ADF的标准PCM,直接反映音频波形

// 运行堆栈大小(Byte)-当前音频采集任务(音频元素)
#define CURRENT_SOUND_COLLECTER_TASK_STACK_SIZE (8 * 1024)

// 缓冲区大小，单位字节
#define CURRENT_SOUND_BUF_SIZE (16 * 1024)

// 缓冲区锁竞争超时，0表示不等待锁，单位ms
#define CURRENT_SOUND_BUF_WRITE_WAIT_TIME_MS 0

// 当缓冲区溢出后超过多长时间清空所有数据并开始覆盖新的数据，单位ms
#define CURRENT_SOUND_BUF_OVERFLOW_CLEAR_TIME_MS 100

typedef enum {
  SHORT_TTS_SAMPLING_RATE_8K = 8000,
  SHORT_TTS_SAMPLING_RATE_16K = 16000,
} short_tts_sampling_rate_t;

typedef enum {
  SHORT_TTS_SAMPLING_BITS_16 = 16,
} short_tts_sampling_bits_t;

typedef enum {
  LONG_TTS_SAMPLING_RATE_16K = 16000,
  LONG_TTS_SAMPLING_RATE_48K = 48000,
} long_tts_sampling_rate_t;

typedef enum {
  LONG_TTS_SAMPLING_BITS_DEFAULT = 16,
} long_tts_sampling_bits_t;

// TTS设定配置
typedef struct TTS_cfg_t {
  char *tex;   // 合成的文本，使用UTF-8编码
  uint8_t spd; // 语速，取值0-15
  uint8_t pit; // 音调，取值0-15
  uint8_t vol; // 音量，取值0-15
  int per;     // 发音人代码
  union {
    // TTS采样率
    short_tts_sampling_rate_t short_tts_rate;
    long_tts_sampling_rate_t long_tts_rate;
  };
  union {
    // TTS位深
    short_tts_sampling_bits_t short_tts_bits;
    long_tts_sampling_bits_t long_tts_bits;
  };
  int output_rate; // 音频播放输出采样率，将传递给filter元素
  int output_bits; // 音频播放输出位深，将传递给filter元素
} TTS_cfg_t;
#define SHORT_TTS_DEFAULT_CONFIG(tex_in, per_in)                               \
  {                                                                            \
      .tex = tex_in,                                                           \
      .spd = 5,                                                                \
      .pit = 5,                                                                \
      .vol = 15,                                                               \
      .per = per_in,                                                           \
      .short_tts_rate = SHORT_TTS_SAMPLING_RATE_16K,                           \
      .short_tts_bits = SHORT_TTS_SAMPLING_BITS_16,                            \
      .output_rate = 44100,                                                    \
      .output_bits = 16,                                                       \
  }
#define LONG_TTS_DEFAULT_CONFIG(tex_in, per_in)                                \
  {                                                                            \
      .tex = tex_in,                                                           \
      .spd = 5,                                                                \
      .pit = 5,                                                                \
      .vol = 15,                                                               \
      .per = per_in,                                                           \
      .long_tts_rate = LONG_TTS_SAMPLING_RATE_16K,                             \
      .long_tts_bits = LONG_TTS_SAMPLING_BITS_DEFAULT,                         \
      .output_rate = 44100,                                                    \
      .output_bits = 16,                                                       \
  }

typedef enum {
  ASR_SAMPLING_RATE_8K = 8000,
  ASR_SAMPLING_RATE_16K = 16000,
} asr_sampling_rate_t; // 识别采样率

typedef enum {
  ASR_SAMPLING_BITS_16 = 16,
} asr_sampling_bits_t; // 识别位深

typedef enum {
  ASR_PID_CM_NEAR_PRO = 80001, // 近场 中文-普通话 极速版
  ASR_PID_CM_NEAR = 1537,      // 近场 中文-普通话 标准版
  ASR_PID_CC = 1637,           // 中文-粤语
  ASR_PID_CS = 1837,           // 中文-四川话
  ASR_PID_EN = 1737,           // 英语
} asr_pid_t;                   // 识别模型

// ASR语音转文字设定配置，注释参考了官方API文档
typedef struct ASR_cfg_t {
  int asr_one_frame_ms; // 识别数据帧时长(ms)-语音识别功能的语音数据打包时长
  asr_sampling_rate_t sampling_rate; // 识别采样速率(需要根据API文档设置)
  asr_sampling_bits_t sampling_bits; // 识别位深(需要根据API文档设置)
  int input_rate;                    // 音频播放输入采样率，将传递给filter元素
  int input_bits;                    // 音频播放输入位深，将传递给filter元素
  asr_pid_t dev_pid;                 // 识别模型
  int stop_threshold; // 停止延时长度，在经过stop_threshold个周期没有监测到语音，认为语音活动停止，暂停录音
  int send_threshold; // 发送阈值，连续send_threshold个周期监测到语音,暂停录音后，这段语音将被发送处理
  int record_save_times_max; // 最大录制record_save_times_max帧数据后强制发送
  vad_mode_t vad_mode;       // VAD模式
  int vad_one_frame_ms;      // VAD 帧时长(单位ms)
  int vad_min_speech_ms;     // 最小语音时长(单位ms)
  int vad_min_noise_ms;      // 最小噪声时长(单位ms)
} ASR_cfg_t;

#define ASR_DEFAULT_CONFIG(max_save, pid, mode_vad)                            \
  {                                                                            \
      .asr_one_frame_ms = 300,                                                 \
      .sampling_rate = ASR_SAMPLING_RATE_16K,                                  \
      .sampling_bits = ASR_SAMPLING_BITS_16,                                   \
      .input_rate = 48000,                                                     \
      .input_bits = 16,                                                        \
      .dev_pid = pid,                                                          \
      .stop_threshold = 5,                                                     \
      .send_threshold = 5,                                                     \
      .record_save_times_max = max_save,                                       \
      .vad_mode = mode_vad,                                                    \
      .vad_one_frame_ms = 30,                                                  \
      .vad_min_speech_ms = 100,                                                \
      .vad_min_noise_ms = 30,                                                  \
  }

typedef struct current_sound_collecter_t {
  // 实时音频收集缓冲区互斥锁，外部需要竞争并拿到锁再访问
  xSemaphoreHandle collecter_buf_mutex;

  // 实时音频收集缓冲区，外部需要竞争并拿到锁再访问
  char *collecter_buf;

  // 实时音频收集运行中标识
  bool volatile collecter_running_flag;

  // 实时音频收集缓冲区是否溢出标志位
  bool volatile collecter_buf_overflow_flag;

  // 实时音频收集音频信息，包含采样率,声道数,位深,比特率等
  audio_element_info_t collecter_element_info;

  // 当前实时音频收集缓冲区实时断流字节数
  uint32_t collecter_buf_drop_in_bytes;

  // 当前实时音频收集缓冲区溢出时的系统时间，单位us
  int64_t collecter_buf_overflow_time;

  // 是否自动根据MP3信息同步实时音频收集音频信息
  bool volatile is_collecter_info_auto_sync_from_mp3;

  // 是否锁定实时音频收集采样率
  bool volatile is_collecter_info_locked_sample_rates;

  // 是否锁定实时音频收集声道数
  bool volatile is_collecter_info_locked_channels;

  // 是否锁定实时音频收集位深
  bool volatile is_collecter_info_locked_bits;

  // 自定义element - current_sound_collecter
  audio_element_handle_t collecter_element;

  // 实时音频收集写指针索引
  int collecter_write_index;
} current_sound_collecter_t;

typedef struct current_sound_collecter_t *current_sound_collecter_handle_t;

extern current_sound_collecter_handle_t collecter_handle;

extern int volatile running_i2s_port;               // 运行的I2S配置
extern bool volatile sevetest30_music_running_flag; // 音乐播放/TTS语音合成运行标志
extern bool volatile sevetest30_asr_running_flag; // 语音识别运行标志

// 外部通用功能运行

void tts_service_play_short(TTS_cfg_t *tts_cfg, UBaseType_t priority);
void tts_service_play_long(TTS_cfg_t *tts_cfg, UBaseType_t priority,
                           int timeout_ms);

void music_uri_or_url_play(const char *uri, UBaseType_t priority);

esp_err_t asr_service_begin(ASR_cfg_t *asr_cfg, UBaseType_t priority);

// 外部扩展功能API
uint64_t mp3_decoder_play_time_get();
