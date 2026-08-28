
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
// 音频数据获取与硬件调度，以支持TTS,语音识别，音乐API播放音乐时的硬件驱动等工作
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：参考了官方提供的pipeline_baidu_speech_mp3例程,非常感谢ESPRESSIF
// github: https://github.com/701Enti

#include "sevetest30_sound.h"
#include "esp_resample.h"
#include "sevetest30_IWEDA.h"
#include "sevetest30_UI.h"

#include "esp_system.h"

#include "esp_mac.h"

#include "board_ctrl.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "audio_common.h"
#include "audio_element.h"
#include "audio_event_iface.h"
#include "audio_pipeline.h"

#include "esp_afe_sr_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "model_path.h"
#include "string.h"

#include "filter_resample.h"
#include "http_stream.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"
#include "raw_stream.h"

#include "board.h"
#include "board_ctrl.h"

#include "audio_mem.h"
#include "esp_http_client.h"
#include "esp_peripherals.h"
#include "esp_vad.h"
#include "periph_wifi.h"

#include "esp_http_client.h"

#include "esp_timer.h"
#include <ctype.h>

bool volatile sevetest30_music_running_flag = false;
bool volatile sevetest30_asr_running_flag = false;
int volatile running_i2s_port = 0;

//// 音频元素句柄
static audio_pipeline_handle_t pipeline = NULL;
static audio_element_handle_t http = NULL;
static audio_element_handle_t i2s = NULL;
static audio_element_handle_t mp3 = NULL;
static audio_element_handle_t filter = NULL;
static audio_element_handle_t raw = NULL;

// 音频句柄
//  音频元素配置
static audio_pipeline_cfg_t pipeline_cfg;
static http_stream_cfg_t http_cfg;
static i2s_stream_cfg_t i2s_cfg;
static mp3_decoder_cfg_t mp3_cfg;
static rsp_filter_cfg_t rsp_cfg;
static raw_stream_cfg_t raw_cfg;

// 事件监听
static audio_event_iface_handle_t common_mp3_evt = NULL;

// 服务定义
static TTS_cfg_t tts_cfg = {0};
static ASR_cfg_t asr_cfg = {0};

static char *baidu_api_access_token = NULL;

// 是否自动根据MP3信息同步filter源信息
static bool volatile is_filter_auto_sync_src_info_from_mp3 = false;

// 是否自动根据MP3信息同步I2S源信息
static bool volatile is_i2s_auto_sync_src_info_from_mp3 = false;

// 记录当前I2S总线音频数据，使用自定义element -
// current_sound_collecter嵌入pipeline读取 格式为ADF的标准PCM,直接反映音频波形

current_sound_collecter_t current_sound_collecter = {0};
current_sound_collecter_handle_t collecter_handle = &current_sound_collecter;

void element_cfg_data_reset();
esp_err_t audio_element_all_init(const char *link_tag[], int link_num);
void common_mp3_running_event();
void common_asr_running_event();
void http_i2s_mp3_music_start(TaskFunction_t running_event,
                              UBaseType_t priority);
void i2s_filter_raw_start(TaskFunction_t running_event, UBaseType_t priority);

int _short_TTS_hook(http_stream_event_msg_t *msg);

static int _current_sound_collecter_process(audio_element_handle_t self,
                                            char *in_buffer, int in_len) {
  const char *TAG = "_current_sound_collecter_process";

  int read_size = audio_element_input(self, in_buffer, in_len);

  if (read_size <= 0) {
    collecter_handle->collecter_running_flag = false;
    return read_size;
  }

  if (read_size > 0) {
    if (collecter_handle->collecter_running_flag == false) {
      audio_element_report_info(self);
      collecter_handle->collecter_running_flag = true;
    }
    if (xSemaphoreTake(collecter_handle->collecter_buf_mutex,
                       pdMS_TO_TICKS(CURRENT_SOUND_BUF_WRITE_WAIT_TIME_MS)) ==
        pdTRUE) {
      if (collecter_handle->collecter_element_info.sample_rates > 0 &&
          collecter_handle->collecter_element_info.channels > 0 &&
          collecter_handle->collecter_element_info.bits > 0) {
        // 确定要写入的字节数
        int bytes_to_write = read_size;

        if (bytes_to_write + collecter_handle->collecter_write_index >=
            CURRENT_SOUND_BUF_SIZE) {
          bytes_to_write =
              CURRENT_SOUND_BUF_SIZE - collecter_handle->collecter_write_index;
          if (bytes_to_write > 0) {
            // 缓冲区将会溢出，记录时间
            collecter_handle->collecter_buf_overflow_time =
                esp_timer_get_time();
          }
          if (bytes_to_write == 0) {
            // 缓冲区已经溢出，到达指定时间清理缓冲区
            collecter_handle->collecter_buf_overflow_flag = true;
            if ((esp_timer_get_time() -
                 collecter_handle->collecter_buf_overflow_time) /
                    1000 >
                CURRENT_SOUND_BUF_OVERFLOW_CLEAR_TIME_MS) {
              collecter_handle->collecter_write_index = 0;
              collecter_handle->collecter_buf_drop_in_bytes = 0;
              memset(collecter_handle->collecter_buf, 0,
                     CURRENT_SOUND_BUF_SIZE * sizeof(char));
              collecter_handle->collecter_buf_overflow_flag = false;
              bytes_to_write = read_size;
              if (bytes_to_write + collecter_handle->collecter_write_index >=
                  CURRENT_SOUND_BUF_SIZE) {
                vTaskDelay(pdMS_TO_TICKS(500));
                ESP_LOGW(
                    TAG,
                    "当前音频读取缓冲区 CURRENT_SOUND_BUF_SIZE 大小设置过低");
                bytes_to_write = CURRENT_SOUND_BUF_SIZE -
                                 collecter_handle->collecter_write_index;
              }
            }
          }
        }

        // 写入缓冲区
        memcpy(collecter_handle->collecter_buf +
                   collecter_handle->collecter_write_index,
               in_buffer, bytes_to_write);
        collecter_handle->collecter_write_index += bytes_to_write;

        collecter_handle->collecter_buf_drop_in_bytes =
            read_size - bytes_to_write;
      }

      xSemaphoreGive(collecter_handle->collecter_buf_mutex);
    }
  }

  audio_element_output(self, in_buffer, read_size);

  return read_size;
}

static esp_err_t _current_sound_collecter_open(audio_element_handle_t self) {
  return ESP_OK;
}

static esp_err_t _current_sound_collecter_close(audio_element_handle_t self) {
  return ESP_OK;
}

static esp_err_t _current_sound_collecter_destroy(audio_element_handle_t self) {
  memset(collecter_handle, 0, sizeof(current_sound_collecter_t));
  return ESP_OK;
}

audio_element_handle_t current_sound_collecter_init() {

  memset(collecter_handle, 0, sizeof(current_sound_collecter_t));

  if (collecter_handle->collecter_buf_mutex == NULL) {
    collecter_handle->collecter_buf_mutex =
        xSemaphoreCreateMutex(); // 仅在第一次初始化创建
  }

  if (collecter_handle->collecter_buf == NULL) {
    collecter_handle->collecter_buf =
        malloc(CURRENT_SOUND_BUF_SIZE * sizeof(char)); // 仅在第一次初始化申请
    if (!collecter_handle->collecter_buf)
      return NULL;
  }
  memset(collecter_handle->collecter_buf, 0,
         CURRENT_SOUND_BUF_SIZE * sizeof(char));

  audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
  cfg.tag = "current_sound_collecter";
  cfg.task_stack = CURRENT_SOUND_COLLECTER_TASK_STACK_SIZE;
  cfg.open = _current_sound_collecter_open;
  cfg.close = _current_sound_collecter_close;
  cfg.process = _current_sound_collecter_process;
  cfg.destroy = _current_sound_collecter_destroy;
  return audio_element_init(&cfg);
}

void current_sound_collecter_music_info_sync(int sample_rates, int channels,
                                             int bits) {
  const char *TAG = "current_sound_collecter_music_info_sync";
  audio_element_getinfo(collecter_handle->collecter_element,
                        &collecter_handle->collecter_element_info);

  ESP_LOGW(TAG, "准备同步音频信息，采样率=%d, 位深=%d, 声道数=%d", sample_rates,
           bits, channels);

  if (collecter_handle->is_collecter_info_locked_sample_rates) {
    ESP_LOGW(TAG, "采样率已锁定，将保持不变");
  } else {
    collecter_handle->collecter_element_info.sample_rates = sample_rates;
  }

  if (collecter_handle->is_collecter_info_locked_channels) {
    ESP_LOGW(TAG, "声道数已锁定，将保持不变");
  } else {
    collecter_handle->collecter_element_info.channels = channels;
  }

  if (collecter_handle->is_collecter_info_locked_bits) {
    ESP_LOGW(TAG, "位深已锁定，将保持不变");
  } else {
    collecter_handle->collecter_element_info.bits = bits;
  }

  if (audio_element_setinfo(collecter_handle->collecter_element,
                            &(collecter_handle->collecter_element_info)) !=
      ESP_OK) {
    ESP_LOGE(TAG, "同步音频信息失败");
    return;
  } else {
    ESP_LOGW(TAG, "同步音频信息成功");
    return;
  }
}

static const char b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * @brief 纯原生Base64编码，输出无任何\r，\n，'\0'
 * @param src 输入二进制数据
 * @param src_len 输入字节长度
 * @param dst 输出字符串缓冲区，末尾不会补'\0'
 * @param dst_buf_len dst缓冲区总字节数
 * @return 成功返回有效base64字符长度（不含末尾'\0'）；失败返回-1
 */
int base64_encode(const uint8_t *src, int src_len, char *dst, int dst_buf_len) {
  const char *TAG = "base64_encode";

  if (src == NULL || dst == NULL) {
    ESP_LOGE(TAG, "src或dst为NULL");
    return -1;
  }

  if (src_len <= 0 || dst_buf_len <= 0) {
    ESP_LOGE(TAG, "src_len或dst_buf_len小于等于0");
    return -1;
  }

  int i = 0;
  int dst_idx = 0;
  // 计算理论输出base64字符数（不含结束符）
  int encode_len = ((src_len + 2) / 3) * 4;

  // 缓冲区空间不足判断
  if (encode_len > dst_buf_len) {
    return -1;
  }

  // 按3字节一组循环编码
  for (; i + 3 <= src_len; i += 3) {
    uint32_t triple = (src[i] << 16) | (src[i + 1] << 8) | src[i + 2];
    dst[dst_idx++] = b64_table[(triple >> 18) & 0x3F];
    dst[dst_idx++] = b64_table[(triple >> 12) & 0x3F];
    dst[dst_idx++] = b64_table[(triple >> 6) & 0x3F];
    dst[dst_idx++] = b64_table[triple & 0x3F];
  }

  // 处理剩余不足3字节的数据
  if (src_len - i == 1) {
    uint32_t triple = src[i] << 16;
    dst[dst_idx++] = b64_table[(triple >> 18) & 0x3F];
    dst[dst_idx++] = b64_table[(triple >> 12) & 0x3F];
    dst[dst_idx++] = '=';
    dst[dst_idx++] = '=';
  } else if (src_len - i == 2) {
    uint32_t triple = (src[i] << 16) | (src[i + 1] << 8);
    dst[dst_idx++] = b64_table[(triple >> 18) & 0x3F];
    dst[dst_idx++] = b64_table[(triple >> 12) & 0x3F];
    dst[dst_idx++] = b64_table[(triple >> 6) & 0x3F];
    dst[dst_idx++] = '=';
  }

  return encode_len;
}

/// @brief uri/url音乐播放
/// @param uriurl 导入音乐uri/url
/// @param priority 任务优先级
void music_uri_or_url_play(const char *uri_or_url, UBaseType_t priority) {
  const char *TAG = "music_uri_or_url_play";

  // common_mp3_evt不为空说明还有音频事件运行中
  if (common_mp3_evt != NULL) {
    ESP_LOGE(TAG, "播放繁忙中，无法准备新播放任务");
    return;
  } else {
    if (periph_wifi_is_connected(wifi_periph_handle) != PERIPH_WIFI_CONNECTED) {
      ESP_LOGE(TAG, "网络未连接");
      return;
    }

    common_mp3_evt = audio_calloc(
        1,
        sizeof(
            audio_event_iface_handle_t)); // 立即申请内存使得common_mp3_evt不为空来锁住其他任务
  }

  element_cfg_data_reset();

  i2s_cfg.chan_cfg.id = CODEC_DAC_I2S_PORT;
  running_i2s_port = i2s_cfg.chan_cfg.id;

  const char *link_tag[4] = {"http", "mp3", "current_sound_collecter", "i2s"};

  if (audio_element_all_init(link_tag, 4) != ESP_OK) {
    ESP_LOGE(TAG, "初始化音频元素时发现问题,任务无法启动");
    return;
  }

  audio_pipeline_link(pipeline, link_tag, 4);
  audio_element_set_uri(http, uri_or_url);

  collecter_handle->is_collecter_info_auto_sync_from_mp3 = true;

  sevetest30_music_running_flag = true;
  http_i2s_mp3_music_start(&common_mp3_running_event, priority);
}

/// @brief 短文本语音合成TTS任务设置的hook
int _short_TTS_hook(http_stream_event_msg_t *msg) {
  const char *TAG = "_short_TTS_hook";
  static char *request_data = NULL;

  if (msg->event_id == HTTP_STREAM_FINISH_REQUEST) {
    free(request_data);
    request_data = NULL;
    return 0;
  }

  if (msg->event_id != HTTP_STREAM_PRE_REQUEST) {
    return 0;
  }

  esp_http_client_handle_t http_client =
      (esp_http_client_handle_t)msg->http_client;

  // URL编码后文本缓冲区，URL编码最大膨胀3倍
  size_t encoded_text_buf_size = strlen(tts_cfg.tex) * 3 + 1;
  char *encoded_text_buf = (char *)malloc(encoded_text_buf_size);
  if (encoded_text_buf == NULL) {
    ESP_LOGE(TAG, "申请编码文本缓冲区失败");
    return 0;
  }
  memset(encoded_text_buf, 0, encoded_text_buf_size);

  esp_err_t ret =
      url_encode(tts_cfg.tex, encoded_text_buf, encoded_text_buf_size, true);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "文本URL编码失败, ret=%s", esp_err_to_name(ret));
    free(encoded_text_buf);
    encoded_text_buf = NULL;
    return 0;
  }

  int request_data_size = 2048 + strlen(encoded_text_buf);
  request_data = (char *)malloc(request_data_size);
  if (request_data == NULL) {
    ESP_LOGE(TAG, "申请request_data内存失败");
    free(encoded_text_buf);
    encoded_text_buf = NULL;
    return 0;
  }
  memset(request_data, 0, request_data_size);

  uint8_t mac[6] = {0};
  esp_efuse_mac_get_default(mac);

  int data_len = snprintf(
      request_data, request_data_size,
      "lan=zh&ctp=1&tok=%s&cuid=%02x:%02x:%02x:%02x:%02x:%02x&vol=%d&spd=%d&"
      "pit=%d&per=%d&aue=3&tex=%s",
      baidu_api_access_token, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
      tts_cfg.vol, tts_cfg.spd, tts_cfg.pit, tts_cfg.per, encoded_text_buf);

  free(encoded_text_buf);
  encoded_text_buf = NULL;

  if ((size_t)data_len >= request_data_size) {
    ESP_LOGE(TAG, "request_data大小不足");
    return 0;
  }

  esp_http_client_set_method(http_client, HTTP_METHOD_POST);
  esp_http_client_set_post_field(http_client, request_data, data_len);
  esp_http_client_set_header(http_client, "Content-Type",
                             "application/x-www-form-urlencoded");

  return 0;
}

/// @brief 请求TTS服务并播放-短文本语音合成
/// @param tts_cfg  tts配置
/// @param priority 任务优先级
void tts_service_play_short(TTS_cfg_t *cfg, UBaseType_t priority) {
  const char *TAG = "tts_service_play_short";

  if (cfg == NULL) {
    ESP_LOGE(TAG, "cfg为NULL");
    return;
  }

  if (cfg->tex == NULL) {
    ESP_LOGE(TAG, "cfg->tex为NULL");
    return;
  }

  // common_mp3_evt不为空说明还有音频事件运行中，进行等待再继续
  if (common_mp3_evt != NULL) {
    ESP_LOGE(TAG, "播放繁忙中，无法准备新播放任务");
    return;
  } else {
    if (periph_wifi_is_connected(wifi_periph_handle) != PERIPH_WIFI_CONNECTED) {
      ESP_LOGE(TAG, "网络未连接");
      return;
    }
    common_mp3_evt = audio_calloc(
        1,
        sizeof(
            audio_event_iface_handle_t)); // 立即申请内存使得common_mp3_evt不为空来锁住其他任务
  }

  // 检查文本长度是否超过最大允许strlen()数值
  if (strlen(cfg->tex) > BAIDU_SHORT_TTS_TEXT_STRLEN_MAX) {
    ESP_LOGE(TAG, "文本长度超过最大允许strlen()数值 %d,无法启动TTS",
             BAIDU_SHORT_TTS_TEXT_STRLEN_MAX);
    return;
  }

  element_cfg_data_reset();

  i2s_cfg.chan_cfg.id = CODEC_DAC_I2S_PORT;
  i2s_cfg.type = AUDIO_STREAM_WRITER;

  running_i2s_port = i2s_cfg.chan_cfg.id;

  rsp_cfg.mode = RESAMPLE_DECODE_MODE;

  rsp_cfg.src_ch = 1;
  rsp_cfg.src_rate = cfg->short_tts_rate;
  rsp_cfg.src_bits = cfg->short_tts_bits;

  rsp_cfg.dest_ch = 2;
  rsp_cfg.dest_rate = cfg->output_rate;
  rsp_cfg.dest_bits = cfg->output_bits;

  http_cfg.event_handle = _short_TTS_hook;

  memcpy(&tts_cfg, cfg, sizeof(TTS_cfg_t));

  baidu_api_access_token = get_baidu_api_access_token();
  if (baidu_api_access_token == NULL) {
    ESP_LOGE(TAG, "获取百度API的AccessToken失败");
    return;
  }

  const char *link_tag[5] = {"http", "mp3", "filter", "current_sound_collecter",
                             "i2s"};
  if (audio_element_all_init(link_tag, 5) != ESP_OK) {
    ESP_LOGE(TAG, "准备音频元素时发现问题");
    return;
  }

  audio_pipeline_link(pipeline, link_tag, 5);
  audio_element_set_uri(http, BAIDU_SHORT_TTS_ENDPOINT);

  current_sound_collecter_music_info_sync(cfg->output_rate, 2,
                                          cfg->output_bits);

  is_filter_auto_sync_src_info_from_mp3 = true;

  sevetest30_music_running_flag = true;
  http_i2s_mp3_music_start(&common_mp3_running_event, priority);
}

/// @brief 请求TTS服务并播放-长文本语音合成
/// @param cfg  tts配置
/// @param priority 任务优先级
/// @param timeout_ms 超时时间,单位毫秒
void tts_service_play_long(TTS_cfg_t *cfg, UBaseType_t priority,
                           int timeout_ms) {
  const char *TAG = "tts_service_play_long";

  if (cfg == NULL) {
    ESP_LOGE(TAG, "cfg为NULL");
    return;
  }

  if (cfg->tex == NULL) {
    ESP_LOGE(TAG, "cfg->tex为NULL");
    return;
  }

  // common_mp3_evt不为空说明还有音频事件运行中，进行等待再继续
  if (common_mp3_evt != NULL) {
    ESP_LOGE(TAG, "播放繁忙中，无法准备新播放任务");
    return;
  } else {
    if (periph_wifi_is_connected(wifi_periph_handle) != PERIPH_WIFI_CONNECTED) {
      ESP_LOGE(TAG, "网络未连接");
      return;
    }
    common_mp3_evt = audio_calloc(
        1,
        sizeof(
            audio_event_iface_handle_t)); // 立即申请内存使得common_mp3_evt不为空来锁住其他任务
  }

  char speech_url[2048] = {0};
  if (fetch_long_tts_speech_url(speech_url, sizeof(speech_url), cfg,
                                timeout_ms) != ESP_OK) {
    ESP_LOGE(TAG, "获取语音URL失败,无法启动");
    return;
  }

  element_cfg_data_reset();

  i2s_cfg.chan_cfg.id = CODEC_DAC_I2S_PORT;
  i2s_cfg.type = AUDIO_STREAM_WRITER;

  running_i2s_port = i2s_cfg.chan_cfg.id;

  rsp_cfg.mode = RESAMPLE_DECODE_MODE;

  rsp_cfg.src_ch = 1;
  rsp_cfg.src_rate = cfg->short_tts_rate;
  rsp_cfg.src_bits = cfg->short_tts_bits;

  rsp_cfg.dest_ch = 2;
  rsp_cfg.dest_rate = cfg->output_rate;
  rsp_cfg.dest_bits = cfg->output_bits;

  memcpy(&tts_cfg, cfg, sizeof(TTS_cfg_t));

  const char *link_tag[5] = {"http", "mp3", "filter", "current_sound_collecter",
                             "i2s"};
  if (audio_element_all_init(link_tag, 5) != ESP_OK) {
    ESP_LOGE(TAG, "准备音频元素时发现问题");
    return;
  }

  audio_pipeline_link(pipeline, link_tag, 5);

  audio_element_set_uri(http, speech_url);

  current_sound_collecter_music_info_sync(cfg->output_rate, 2,
                                          cfg->output_bits);

  is_filter_auto_sync_src_info_from_mp3 = true;

  sevetest30_music_running_flag = true;
  http_i2s_mp3_music_start(&common_mp3_running_event, priority);
}

/// @brief
/// 启动语音识别服务，启动后不断地自动监听并完成识别,是一个不断循环识别的任务
/// @param asr_cfg asr配置
/// @param priority 任务优先级
void asr_service_begin(ASR_cfg_t *cfg, UBaseType_t priority) {
  const char *TAG = "asr_service_begin";

  if (sevetest30_asr_running_flag) {
    ESP_LOGE(TAG, "识别繁忙中，无法准备新识别任务");
    return;
  } else {
    if (periph_wifi_is_connected(wifi_periph_handle) != PERIPH_WIFI_CONNECTED) {
      ESP_LOGE(TAG, "网络未连接");
      return;
    }
    sevetest30_asr_running_flag = true;
  }

  element_cfg_data_reset();

  i2s_cfg.chan_cfg.id = CODEC_ADC_I2S_PORT;
  i2s_cfg.type = AUDIO_STREAM_READER;

  running_i2s_port = i2s_cfg.chan_cfg.id;

  rsp_cfg.mode = RESAMPLE_DECODE_MODE;

  rsp_cfg.src_ch = 2;
  rsp_cfg.src_bits = cfg->input_bits;
  rsp_cfg.src_rate = cfg->input_rate;

  rsp_cfg.dest_ch = 1;
  rsp_cfg.dest_bits = cfg->sampling_bits;
  rsp_cfg.dest_rate = cfg->sampling_rate;

  memcpy(&asr_cfg, cfg, sizeof(ASR_cfg_t));

  baidu_api_access_token = get_baidu_api_access_token();
  if (baidu_api_access_token == NULL) {
    ESP_LOGE(TAG, "获取百度API的AccessToken失败");
    return;
  }

  const char *link_tag[4] = {"i2s", "current_sound_collecter", "filter", "raw"};

  if (audio_element_all_init(link_tag, 4) != ESP_OK) {
    ESP_LOGE(TAG, "准备音频元素时发现问题");
    sevetest30_asr_running_flag = false;
    return;
  }

  audio_pipeline_link(pipeline, link_tag, 4);

  current_sound_collecter_music_info_sync(cfg->sampling_rate, 1,
                                          cfg->sampling_bits);

  i2s_filter_raw_start(common_asr_running_event, priority);
}

/// @brief 获取MP3解码器播放实时时间进度
/// @return 播放运行的实时时间(单位ms)
uint64_t mp3_decoder_play_time_get() {
  audio_element_info_t info;
  audio_element_getinfo(mp3, &info);
  return (1000 * info.byte_pos * 8 / info.bps);
}

void common_mp3_running_event() {
  const char *TAG = "common_mp3_running_event";
  board_ctrl_t *ctrl_buf = board_status_get();
  if (ctrl_buf != NULL) {
    ESP_LOGI(TAG, "即将播放 解码器音量 %d 功放音量 %d",
             ctrl_buf->codec_dac_volume, ctrl_buf->amplifier_volume);
    if (ctrl_buf->amplifier_sd == false)
      ESP_LOGW(TAG, "功放未使能");
    if (ctrl_buf->amplifier_mute == true)
      ESP_LOGW(TAG, "静音状态");
  }

  while (sevetest30_music_running_flag) {
    audio_event_iface_msg_t msg;

    audio_event_iface_listen(common_mp3_evt, &msg, pdMS_TO_TICKS(500));

    if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT &&
        msg.source == (void *)mp3 && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
      audio_element_info_t music_info = {0};
      audio_element_getinfo(mp3, &music_info);

      ESP_LOGW(TAG, "[mp3_decoder的音频信息]采样率=%d, 位深=%d, 声道数=%d",
               music_info.sample_rates, music_info.bits, music_info.channels);

      if (collecter_handle->is_collecter_info_auto_sync_from_mp3) {
        ESP_LOGW(TAG, "is_current_sound_collecter_info_auto_sync_from_mp3-"
                      "自动同步为开启状态");
        current_sound_collecter_music_info_sync(
            music_info.sample_rates, music_info.channels, music_info.bits);
        ESP_LOGW(TAG, "音频信息已同步到current_sound_collecter");
      }

      if (is_i2s_auto_sync_src_info_from_mp3) {
        ESP_LOGW(TAG, "is_i2s_auto_sync_src_info_from_mp3-自动同步为开启状态");
        if (i2s_stream_set_clk(i2s, music_info.sample_rates, music_info.bits,
                               music_info.channels) == ESP_OK) {
          ESP_LOGW(TAG, "I2S流配置已更新");
        } else {
          ESP_LOGE(TAG, "更改I2S流配置失败");
          break;
        }
      }

      if (is_filter_auto_sync_src_info_from_mp3) {
        ESP_LOGW(TAG,
                 "is_filter_auto_sync_src_info_from_mp3-自动同步为开启状态");
        if (rsp_filter_change_src_info(filter, music_info.sample_rates,
                                       music_info.channels,
                                       music_info.bits) == ESP_OK) {
          ESP_LOGW(TAG, "音频信息已同步到filter");
        } else {
          ESP_LOGE(TAG, "更改filter流配置失败");
          break;
        }
      }
      continue;
    }

    if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT &&
        msg.source == (void *)(collecter_handle->collecter_element) &&
        msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
      audio_element_info_t music_info = {0};
      audio_element_getinfo(collecter_handle->collecter_element, &music_info);

      ESP_LOGI(
          TAG,
          "[current_sound_collecter的音频信息]采样率=%d, 位深=%d, 声道数=%d",
          music_info.sample_rates, music_info.bits, music_info.channels);

      if (i2s_stream_set_clk(i2s, music_info.sample_rates, music_info.bits,
                             music_info.channels) != ESP_OK) {
        ESP_LOGE(TAG, "更改I2S流配置失败");
        break;
      } else {
        ESP_LOGW(TAG, "I2S流配置已更新");
      }
      continue;
    }

    if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT &&
        msg.source == (void *)i2s && msg.cmd == AEL_MSG_CMD_REPORT_STATUS &&
        (((int)msg.data == AEL_STATUS_STATE_STOPPED) ||
         ((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
      ESP_LOGI(TAG, "播放完毕");
      break;
    }

    if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT &&
        msg.cmd == AEL_MSG_CMD_REPORT_STATUS) {
      int status = (int)msg.data;
      if (status == AEL_STATUS_ERROR_OPEN ||
          status == AEL_STATUS_ERROR_PROCESS ||
          status == AEL_STATUS_ERROR_CLOSE ||
          status == AEL_STATUS_ERROR_UNKNOWN) {
        ESP_LOGE(TAG, "ppipeline元素错误,播放强制停止, status: %d, source: %p",
                 status, msg.source);
        break;
      }
    }
  }

  // 禁止外部任务对该I2S端口访问
  running_i2s_port = -1;

  audio_pipeline_stop(pipeline);
  audio_pipeline_wait_for_stop(pipeline);
  audio_pipeline_terminate(pipeline);
  audio_pipeline_remove_listener(pipeline);

  esp_periph_set_stop_all(se30_periph_set_handle);
  audio_event_iface_remove_listener(
      esp_periph_set_get_event_iface(se30_periph_set_handle), common_mp3_evt);
  audio_event_iface_destroy(common_mp3_evt);

  audio_pipeline_unregister(pipeline, http);
  audio_pipeline_unregister(pipeline, mp3);
  if (filter != NULL) {
    audio_pipeline_unregister(pipeline, filter);
  }
  if (collecter_handle->collecter_element != NULL) {
    audio_pipeline_unregister(pipeline, collecter_handle->collecter_element);
  }
  audio_pipeline_unregister(pipeline, i2s);

  audio_pipeline_deinit(pipeline);
  audio_element_deinit(http);
  audio_element_deinit(mp3);
  if (filter != NULL) {
    audio_element_deinit(filter);
  }
  if (collecter_handle->collecter_element != NULL) {
    audio_element_deinit(collecter_handle->collecter_element);
  }
  audio_element_deinit(i2s);

  pipeline = NULL;
  http = NULL;
  mp3 = NULL;
  if (filter != NULL) {
    filter = NULL;
  }
  if (collecter_handle->collecter_element != NULL) {
    collecter_handle->collecter_element = NULL;
  }
  i2s = NULL;

  sevetest30_music_running_flag = false;

  ESP_LOGW(TAG, "http_i2s_mp3_music音频播放任务关闭");

  common_mp3_evt = NULL; // 释放句柄，表示任务的结束

  vTaskDelete(NULL);
}

void common_asr_running_event() {
  const char *TAG = "common_asr_running_event";

  // 获取设备MAC地址
  uint8_t mac[6] = {0};
  esp_efuse_mac_get_default(mac);

  // 初始化http_client
  esp_http_client_config_t http_config;
  memset(&http_config, 0, sizeof(http_config));
  if (asr_cfg.dev_pid == ASR_PID_CM_NEAR_PRO) {
    http_config.url = BAIDU_ASR_PRO_URL;
  } else {
    http_config.url = BAIDU_ASR_URL;
  }
  http_config.method = HTTP_METHOD_POST; // 使用POST请求
  esp_http_client_handle_t client_handle = esp_http_client_init(&http_config);

  esp_http_client_set_header(client_handle, "Content-Type", "application/json");

  int request_body_size = asr_cfg.asr_one_frame_ms * asr_cfg.sampling_rate /
                              1000 * sizeof(char) *
                              asr_cfg.record_save_times_max / 3 * 4 +
                          2048 * sizeof(char);
  char *request_body_buf = NULL;
  request_body_buf = (char *)malloc(request_body_size);
  while (!request_body_buf && sevetest30_asr_running_flag) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGE(TAG, "申请request_body_buf资源发现问题 正在重试");
    request_body_buf = (char *)malloc(request_body_size);
  }
  memset(request_body_buf, 0, request_body_size);

  int record_save_times = 0; // 录制并保存音频数据的次数（防溢出）
  int asr_len = 0;           // 实际原始音频数据长度，作为ASR时的len参数

  // 配置VAD 语音活动监测
  int vad_stop_counter = 0; // 记录没有监测到语音的周期数
  int vad_keep_counter = 0; // 记录监测到语音的周期数

  vad_handle_t vad_handle = vad_create_with_param(
      asr_cfg.vad_mode, asr_cfg.sampling_rate, asr_cfg.vad_one_frame_ms,
      asr_cfg.vad_min_speech_ms, asr_cfg.vad_min_noise_ms);
  vad_state_t vad_state = VAD_SILENCE;

  int vad_buf_size =
      asr_cfg.vad_one_frame_ms * asr_cfg.sampling_rate / 1000 * sizeof(int16_t);
  int16_t *vad_buf = NULL;
  vad_buf = (int16_t *)malloc(vad_buf_size);
  while (!vad_buf && sevetest30_asr_running_flag) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGE(TAG, "申请vad_buf资源发现问题 正在重试");
    vad_buf = (int16_t *)malloc(vad_buf_size);
  }
  memset(vad_buf, 0, vad_buf_size);

  // 申请识别数据帧缓存
  int asr_data_buf_size =
      asr_cfg.asr_one_frame_ms * asr_cfg.sampling_rate / 1000 * sizeof(char);
  char *asr_data_buf = NULL;
  asr_data_buf = (char *)malloc(asr_data_buf_size);
  while (!asr_data_buf && sevetest30_asr_running_flag) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGE(TAG, "申请asr_data_buf资源发现问题 正在重试");
    asr_data_buf = (char *)malloc(asr_data_buf_size);
  }
  memset(asr_data_buf, 0, asr_data_buf_size);

  // 申请响应数据缓存
  int response_buf_size = ASR_HTTP_RESPONSE_BUF_MAX * sizeof(char);
  char *response_buf = NULL;
  response_buf = (char *)malloc(response_buf_size);
  while (!response_buf && sevetest30_asr_running_flag) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGE(TAG, "申请response_buf资源发现问题 正在重试");
    response_buf = (char *)malloc(response_buf_size);
  }
  memset(response_buf, 0, response_buf_size);

  ESP_LOGW(TAG, "正在监听麦克风阵列");

RESTART:
  // {"format":"pcm","rate":%d,"channel":1,"token":"%s","cuid":"%d","speech":"
  // -
  // - - - -    ","len":%d}
  memset(request_body_buf, 0, request_body_size);
  int request_body_len =
      snprintf(request_body_buf, request_body_size,
               "{\"format\":\"pcm\",\"rate\":%d,\"channel\":1,\"token\":\"%s\","
               "\"cuid\":"
               "\"%02x:%02x:%02x:%02x:%02x:%02x\",\"dev_pid\":%d,\"speech\":\"",
               asr_cfg.sampling_rate, baidu_api_access_token, mac[0], mac[1],
               mac[2], mac[3], mac[4], mac[5], asr_cfg.dev_pid);
  if (request_body_len >= request_body_size) {
    ESP_LOGE(TAG, "request_body内存不足");
    sevetest30_asr_running_flag = false;
  }

  // 进入任务主循环
  while (sevetest30_asr_running_flag) {

    vTaskDelay(pdMS_TO_TICKS(10)); // 为idle提供的必须延时

    // 读取raw数据并输入VAD
    memset(vad_buf, 0, vad_buf_size);
    while (sevetest30_asr_running_flag) {
      int read_index = 0;
      read_index += raw_stream_read(raw, ((char *)vad_buf) + read_index,
                                    vad_buf_size - read_index);
      if (vad_buf_size - read_index <= 0) {
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    vad_state = vad_process_with_trigger(vad_handle, vad_buf);

    if (vad_state == VAD_SPEECH) {
      ESP_LOGI(TAG, "语音活动进行中 request_body_len %.2f KB",
               (float)request_body_len / 1000);
      vad_stop_counter = 0; // 语音活动继续而不停止
      vad_keep_counter++;   // 语音保持时间增加，直到发送阈值
      memset(asr_data_buf, 0, asr_data_buf_size);

      if (asr_data_buf_size > vad_buf_size) {
        memcpy(asr_data_buf, vad_buf, vad_buf_size);
        raw_stream_read(raw, asr_data_buf + vad_buf_size,
                        asr_data_buf_size - vad_buf_size);
      } else {
        memcpy(asr_data_buf, vad_buf, asr_data_buf_size);
      }

      int out_size = base64_encode((uint8_t *)asr_data_buf, asr_data_buf_size,
                                   request_body_buf + request_body_len,
                                   request_body_size - request_body_len);
      request_body_len += out_size;
      if (request_body_size - request_body_len <= 0) {
        ESP_LOGE(TAG, "request_body设置的缓存过小");
        sevetest30_asr_running_flag = false;
      }
      if (out_size < 0) {
        ESP_LOGE(TAG, "base64_encode 编码异常");
        sevetest30_asr_running_flag = false;
      }
      asr_len += asr_data_buf_size;
      record_save_times++;
    } else if (vad_state == VAD_SILENCE) {
      vad_stop_counter++; // 语音活动有停止的趋势

      // 如果停顿时间超时 语音活动结束
      if (vad_stop_counter >= asr_cfg.stop_threshold) {
        vad_stop_counter = 0; // 复位

        // 如果录制的语音达到发送阈值
        if (vad_keep_counter >= asr_cfg.send_threshold) {
        POST_SEND:
          vad_keep_counter = 0; // 复位

          // 完善请求体
          //  {"format":"pcm","rate":%d,"channel":1,"token":"%s","cuid":"%d","speech":"
          //  - - - - -    ","len":%d}
          request_body_len += snprintf(request_body_buf + request_body_len,
                                       request_body_size - request_body_len,
                                       "\",\"len\":%d}", asr_len);
          if (request_body_len >= request_body_size) {
            ESP_LOGE(TAG, "request_body内存不足");
            sevetest30_asr_running_flag = false;
          }

          esp_err_t err = ESP_OK;
          err |= esp_http_client_set_timeout_ms(client_handle, ASR_TIMEOUT_MS);
          err |= esp_http_client_open(client_handle, request_body_len);

          if (err != ESP_OK) {
            ESP_LOGE(TAG, "配置连接时出现问题 -> %s", http_config.url);
            esp_http_client_close(client_handle); // 关闭连接
            esp_http_client_close(client_handle); // 关闭连接
            record_save_times = 0;
            asr_len = 0;
            goto RESTART;
          }

          ESP_LOGW(TAG, "等待识别完成");
          esp_http_client_write(client_handle, request_body_buf,
                                request_body_len);
          esp_http_client_fetch_headers(client_handle);
          int status = esp_http_client_get_status_code(client_handle);

          if (status != 200) {
            ESP_LOGE(TAG, "识别出现问题，请提高音量降低语速并重试");
          } else {
            esp_http_client_read_response(client_handle, response_buf,
                                          ASR_HTTP_RESPONSE_BUF_MAX);
            asr_data_save_result(response_buf);
          }

          esp_http_client_close(client_handle);
          record_save_times = 0;
          asr_len = 0;
          goto RESTART;
        }
      }
    }

    // 数据大小达到最大 强制发送
    if (record_save_times >= asr_cfg.record_save_times_max) {
      ESP_LOGW(TAG, "语音数据大小达到最大值");
      goto POST_SEND;
    }
  }

  // 禁止外部任务对该I2S端口访问
  running_i2s_port = -1;

  // 清理VAD
  vad_destroy(vad_handle);

  // 清理连接缓存
  esp_http_client_cleanup(client_handle);

  // 释放数据缓存
  free(vad_buf);
  free(asr_data_buf);
  free(response_buf);
  free(request_body_buf);
  vad_buf = NULL;
  asr_data_buf = NULL;
  response_buf = NULL;
  request_body_buf = NULL;

  audio_pipeline_stop(pipeline);
  audio_pipeline_wait_for_stop(pipeline);
  audio_pipeline_terminate(pipeline);
  audio_pipeline_remove_listener(pipeline);

  audio_pipeline_unregister(pipeline, i2s);
  audio_pipeline_unregister(pipeline, filter);
  if (collecter_handle->collecter_element != NULL) {
    audio_pipeline_unregister(pipeline, collecter_handle->collecter_element);
  }
  audio_pipeline_unregister(pipeline, raw);

  audio_pipeline_deinit(pipeline);
  audio_element_deinit(i2s);
  audio_element_deinit(filter);
  if (collecter_handle->collecter_element != NULL) {
    audio_element_deinit(collecter_handle->collecter_element);
  }
  audio_element_deinit(raw);

  pipeline = NULL;
  i2s = NULL;
  filter = NULL;
  if (collecter_handle->collecter_element != NULL) {
    collecter_handle->collecter_element = NULL;
  }
  raw = NULL;

  vTaskDelete(NULL);
}

/// @brief
/// 重置元素配置数据到默认值，之后您可以针对性修改某些参数，接着运行元素配置函数
void element_cfg_data_reset() {

  // 之后可以针对性继续修改某些参数来进一步自定义配置

  is_filter_auto_sync_src_info_from_mp3 = false;
  is_i2s_auto_sync_src_info_from_mp3 = false;

  // pipeline
  static audio_pipeline_cfg_t pipeline_cfg_buf =
      DEFAULT_AUDIO_PIPELINE_CONFIG();
  pipeline_cfg = pipeline_cfg_buf;

  // http流
  static http_stream_cfg_t http_cfg_buf = HTTP_STREAM_CFG_DEFAULT();
  http_cfg = http_cfg_buf;

  http_cfg.type = AUDIO_STREAM_READER;
  http_cfg.out_rb_size = ELEMENT_HTTP_STREAM_RINGBUFFER_SIZE;

  // I2S
  static i2s_stream_cfg_t i2s_cfg_buf = I2S_STREAM_CFG_DEFAULT();
  i2s_cfg = i2s_cfg_buf;

  i2s_cfg.out_rb_size = ELEMENT_I2S_STREAM_RINGBUFFER_SIZE;

  // MP3解码器
  static mp3_decoder_cfg_t mp3_cfg_buf = DEFAULT_MP3_DECODER_CONFIG();
  mp3_cfg = mp3_cfg_buf;

  mp3_cfg.task_core = ELEMENT_MP3_DECODER_TASK_CORE;
  mp3_cfg.task_stack = ELEMENT_MP3_DECODER_TASK_STACK_SIZE;
  mp3_cfg.out_rb_size = ELEMENT_MP3_DECODER_RINGBUFFER_SIZE;

  // filter
  static rsp_filter_cfg_t rsp_cfg_buf = DEFAULT_RESAMPLE_FILTER_CONFIG();
  rsp_cfg = rsp_cfg_buf;

  // RAW原始音频流读取
  static raw_stream_cfg_t raw_cfg_buf = RAW_STREAM_CFG_DEFAULT();
  raw_cfg = raw_cfg_buf;
  raw_cfg.out_rb_size = ELEMENT_RAW_STREAM_RINGBUFFER_SIZE;
  raw_cfg.type = AUDIO_STREAM_READER;
}

/// @brief 以现在的存储的配置初始化所有选定link的音频元素
esp_err_t audio_element_all_init(const char *link_tag[], int link_num) {
  const char *TAG = "audio_element_all_init";

  if (!pipeline) {
    pipeline = audio_pipeline_init(&pipeline_cfg);
    if (!pipeline)
      return ESP_FAIL;
  }

  for (int i = 0; i < link_num; i++) {
    if (!strcmp(link_tag[i], "current_sound_collecter")) {
      if (collecter_handle->collecter_element == NULL) {
        collecter_handle->collecter_element = current_sound_collecter_init();
        if (!(collecter_handle->collecter_element)) {
          ESP_LOGE(TAG, "current_sound_collecter元素初始化失败");
          return ESP_FAIL;
        } else {
          audio_pipeline_register(pipeline, collecter_handle->collecter_element,
                                  "current_sound_collecter");
        }
      } else {
        ESP_LOGE(TAG, "current_sound_collecter元素不为NULL,资源未释放");
        return ESP_FAIL;
      }
    }

    if (!strcmp(link_tag[i], "http")) {
      if (http == NULL) {
        http = http_stream_init(&http_cfg);
        if (!http) {
          ESP_LOGE(TAG, "http元素初始化失败");
          return ESP_FAIL;
        } else {
          audio_pipeline_register(pipeline, http, "http");
        }
      } else {
        ESP_LOGE(TAG, "http元素不为NULL,资源未释放");
        return ESP_FAIL;
      }
    }

    if (!strcmp(link_tag[i], "mp3")) {
      if (mp3 == NULL) {
        mp3 = mp3_decoder_init(&mp3_cfg);
        if (!mp3) {
          ESP_LOGE(TAG, "mp3元素初始化失败");
          return ESP_FAIL;
        } else {
          audio_pipeline_register(pipeline, mp3, "mp3");
        }
      } else {
        ESP_LOGE(TAG, "mp3元素不为NULL,资源未释放");
        return ESP_FAIL;
      }
    }

    if (!strcmp(link_tag[i], "filter")) {
      if (filter == NULL) {
        filter = rsp_filter_init(&rsp_cfg);
        if (!filter) {
          ESP_LOGE(TAG, "filter元素初始化失败");
          return ESP_FAIL;
        } else {
          audio_pipeline_register(pipeline, filter, "filter");
        }
      } else {
        ESP_LOGE(TAG, "filter元素不为NULL,资源未释放");
        return ESP_FAIL;
      }
    }

    if (!strcmp(link_tag[i], "raw")) {
      if (raw == NULL) {
        raw = raw_stream_init(&raw_cfg);
        if (!raw) {
          ESP_LOGE(TAG, "raw元素初始化失败");
          return ESP_FAIL;
        } else {
          audio_pipeline_register(pipeline, raw, "raw");
        }
      } else {
        ESP_LOGE(TAG, "raw元素不为NULL,资源未释放");
        return ESP_FAIL;
      }
    }

    if (!strcmp(link_tag[i], "i2s")) {
      if (i2s == NULL) {
        i2s = i2s_stream_init(&i2s_cfg);
        if (!i2s) {
          ESP_LOGE(TAG, "i2s元素初始化失败");
          return ESP_FAIL;
        } else {
          audio_pipeline_register(pipeline, i2s, "i2s");
        }
      } else {
        ESP_LOGE(TAG, "i2s元素不为NULL,资源未释放");
        return ESP_FAIL;
      }
    }
  }

  return ESP_OK;
}

void http_i2s_mp3_music_start(TaskFunction_t running_event,
                              UBaseType_t priority) {
  // 设置事件监听
  audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
  common_mp3_evt = audio_event_iface_init(&evt_cfg);
  audio_pipeline_set_listener(pipeline, common_mp3_evt);
  audio_event_iface_set_listener(
      esp_periph_set_get_event_iface(se30_periph_set_handle), common_mp3_evt);
  // 运行音频通道
  audio_pipeline_run(pipeline);
  // 事件监听任务启动
  xTaskCreatePinnedToCore(running_event, "http_i2s_mp3_music_run",
                          MUSIC_EVT_TASK_STACK_SIZE, NULL, priority, NULL,
                          MUSIC_EVT_TASK_CORE);
}

void i2s_filter_raw_start(TaskFunction_t running_event, UBaseType_t priority) {
  board_ctrl_t board_ctrl;
  board_ctrl.codec_audio_hal_ctrl = AUDIO_HAL_CTRL_START;
  board_ctrl.codec_mode = AUDIO_HAL_CODEC_MODE_ENCODE;
  sevetest30_board_ctrl(&board_ctrl, BOARD_CTRL_CODEC_MODE_AND_STATUS);

  audio_pipeline_run(pipeline);
  xTaskCreatePinnedToCore(running_event, "i2s_filter_raw_run",
                          ASR_EVT_TASK_STACK_SIZE, NULL, priority, NULL,
                          ASR_EVT_TASK_CORE);
}
