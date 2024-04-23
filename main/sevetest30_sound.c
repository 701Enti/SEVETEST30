
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

 // 该文件归属701Enti组织，主要由SEVETEST30开发团队维护，包含一些sevetest30的  音频数据获取与硬件调度，以支持TTS,语音识别，音乐API播放音乐时的硬件驱动等工作
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // 敬告：参考了官方提供的pipeline_baidu_speech_mp3例程,非常感谢ESPRESSIF
 // github: https://github.com/701Enti
 // bilibili: 701Enti

#include "sevetest30_sound.h"
#include "sevetest30_IWEDA.h"
#include "sevetest30_UI.h"

#include "esp_system.h"

#include "esp_log.h"
#include "esp_err.h"
#include "board_ctrl.h"
#include "sdkconfig.h"

#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"

#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "model_path.h"
#include "string.h"

#include "http_stream.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"
#include "raw_stream.h"
#include "filter_resample.h"

#include "board.h"
#include "board_ctrl.h"

#include "audio_mem.h"
#include "esp_vad.h"
#include "esp_peripherals.h"
#include "periph_wifi.h"
#include "esp_http_client.h"
#include "baidu_access_token.h"

#include "esp_http_client.h"
#include "base64_re.h"

#include "audio_idf_version.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
#include "esp_netif.h"
#else
#include "tcpip_adapter.h"
#endif

bool volatile sevetest30_music_running_flag = false;
bool volatile sevetest30_asr_running_flag = false;
int  volatile running_i2s_port = 0;

// 音频元素句柄
audio_pipeline_handle_t pipeline = NULL;
audio_element_handle_t http_stream_reader = NULL;
audio_element_handle_t i2s_stream_writer = NULL;
audio_element_handle_t i2s_stream_reader = NULL;
audio_element_handle_t mp3_decoder = NULL;
audio_element_handle_t audio_rsp_filter = NULL;
audio_element_handle_t raw_read = NULL;

// 音频元素配置
audio_pipeline_cfg_t pipeline_cfg;
http_stream_cfg_t http_cfg;
i2s_stream_cfg_t i2s_cfg;
mp3_decoder_cfg_t mp3_cfg;
rsp_filter_cfg_t rsp_cfg;
raw_stream_cfg_t raw_cfg;

// 事件监听
audio_event_iface_handle_t common_mp3_evt = NULL;

// 服务定义
baidu_TTS_cfg_t* TTS_cfg_buf = NULL;
baidu_ASR_cfg_t* ASR_cfg_buf = NULL;

char* baidu_access_token = NULL;

int _TTS_get_token_handle(http_stream_event_msg_t* msg);

void common_mp3_running_event()
{
  const char* TAG = "common_mp3_running_event";
  board_ctrl_t* ctrl_buf = board_status_get();
  if (ctrl_buf != NULL)
  {
    ESP_LOGI(TAG, "即将播放 解码器音量 %d 功放音量 %d", ctrl_buf->codec_dac_volume, ctrl_buf->amplifier_volume);
    if (ctrl_buf->amplifier_mute == true)
      ESP_LOGE(TAG, "静音状态");
  }
  while (sevetest30_music_running_flag)
  {
    audio_event_iface_msg_t msg;
    // 不断进行监听
    esp_err_t ret = audio_event_iface_listen(common_mp3_evt, &msg, portMAX_DELAY);
    if (ret != ESP_OK)
    {
      ESP_LOGE(TAG, "监听事件时出现问题 %d", ret);
      continue;
    }

    // 消息性质为[音频元素类->来自mp3_decoder->音频信息报告] 进行音频硬件自适应
    if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void*)mp3_decoder && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO)
    {
      audio_element_info_t music_info = { 0 };
      audio_element_getinfo(mp3_decoder, &music_info);

      ESP_LOGI(TAG, "收到来自mp3_decoder音频, 采样率=%d, 位深=%d, 通道数=%d",
        music_info.sample_rates, music_info.bits, music_info.channels);

      i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
      continue;
    }

    // 消息性质为[音频元素类->来自i2s_stream_writer->运行状态报告] 内容 为 停止状态 或 完成状态 就结束播放事件
    if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void*)i2s_stream_writer && msg.cmd == AEL_MSG_CMD_REPORT_STATUS && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED)))
    {
      ESP_LOGI(TAG, "播放完毕");
      break;
    }
  }

  //禁止外部任务对该I2S端口访问
  running_i2s_port = -1;

  // 等待停止
  audio_pipeline_stop(pipeline);
  audio_pipeline_wait_for_stop(pipeline);
  audio_pipeline_terminate(pipeline);
  // 注销使用的元素
  audio_pipeline_unregister(pipeline, http_stream_reader);
  audio_pipeline_unregister(pipeline, i2s_stream_writer);
  audio_pipeline_unregister(pipeline, mp3_decoder);
  // 删除监听
  audio_pipeline_remove_listener(pipeline);
  // 暂停所有外设
  esp_periph_set_stop_all(se30_periph_set_handle);
  // 删除监听总线
  audio_event_iface_remove_listener(esp_periph_set_get_event_iface(se30_periph_set_handle), common_mp3_evt);
  audio_event_iface_destroy(common_mp3_evt);
  // 重置使用的元素
  audio_pipeline_deinit(pipeline);
  audio_element_deinit(http_stream_reader);
  audio_element_deinit(i2s_stream_writer);
  audio_element_deinit(mp3_decoder);

  pipeline = NULL;
  http_stream_reader = NULL;
  i2s_stream_writer = NULL;
  mp3_decoder = NULL;


  sevetest30_music_running_flag = false;


  ESP_LOGW(TAG, "http_i2s_mp3_music音频播放任务关闭");

  common_mp3_evt = NULL; // 释放句柄，表示任务的结束

  vTaskDelete(NULL);
}

void common_asr_running_event()
{
  const char* TAG = "common_asr_running_event";

  // 准备POST请求

  // 获取设备MAC地址
  int mac = 0;
  esp_base_mac_addr_get((uint8_t*)&mac);

  // 初始化http_client
  esp_http_client_config_t http_config;
  memset(&http_config, 0, sizeof(http_config));
  if (ASR_cfg_buf->dev_pid == ASR_PID_CM_NEAR_PRO)
    http_config.url = BAIDU_ASR_PRO_URL; // 导入url
  else
    http_config.url = BAIDU_ASR_URL;
  http_config.method = HTTP_METHOD_POST; // 使用POST请求
  esp_http_client_handle_t client_handle = esp_http_client_init(&http_config);

  // 设置HTTP-HEADER
  esp_http_client_set_header(client_handle, "Content-Type", "application/json");

  // 准备HTTP-BODY
  char* request_body_buf = NULL; // 请求体空间 = 最大音频数据大小（PCM转换为base64会增大1/3） + 其他附加数据大小
  request_body_buf = (char*)malloc(ASR_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(char) * ASR_cfg_buf->record_save_times_max / 3 * 4 + 2048 * sizeof(char));
  while (!request_body_buf && sevetest30_asr_running_flag)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGE(TAG, "申请request_body_buf资源发现问题 正在重试");
    request_body_buf = (char*)malloc(ASR_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(char) * ASR_cfg_buf->record_save_times_max / 3 * 4 + 2048 * sizeof(char));
  }
  memset(request_body_buf, 0, ASR_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(char) * ASR_cfg_buf->record_save_times_max / 3 * 4 + 2048 * sizeof(char));

  // {"format":"pcm","rate":%d,"channel":1,"token":"%s","cuid":"%d","speech":" - - - - -    ","len":%d}
  snprintf(request_body_buf, ASR_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * ASR_cfg_buf->record_save_times_max / 3 * 4 + 2048,
    "{\"format\":\"pcm\",\"rate\":%d,\"channel\":1,\"token\":\"%s\",\"cuid\":\"%d\",\"dev_pid\":%d,\"speech\":\"", ASR_cfg_buf->rate, baidu_access_token, mac, ASR_cfg_buf->dev_pid);

  int record_save_times = 0; // 录制并保存音频数据的次数（防溢出）
  int asr_len = 0;           // 实际原始音频数据长度，作为ASR时的len参数

  // 配置VAD 语音活动监测
  int vad_stop_counter = 0; // 记录没有监测到语音的周期数
  int vad_keep_counter = 0; // 记录监测到语音的周期数

  vad_handle_t vad_inst = vad_create(VAD_MODE_4);
  vad_state_t vad_state = VAD_SILENCE;

  int16_t* vad_buf = NULL;
  vad_buf = (int16_t*)malloc(VAD_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(short));
  while (!vad_buf && sevetest30_asr_running_flag)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGE(TAG, "申请vad_buf资源发现问题 正在重试");
    vad_buf = (int16_t*)malloc(VAD_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(short));
  }

  // 申请识别数据帧缓存
  char* asr_data_buf = NULL;
  asr_data_buf = (char*)malloc(ASR_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(char));
  while (!asr_data_buf && sevetest30_asr_running_flag)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGE(TAG, "申请asr_data_buf资源发现问题 正在重试");
    asr_data_buf = (char*)malloc(ASR_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(char));
  }

  // 申请请求体拼接内容缓存
  char* body_end_buf = NULL;
  body_end_buf = (char*)malloc(50 * sizeof(char));
  while (!body_end_buf && sevetest30_asr_running_flag)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGE(TAG, "申请body_end_buf资源发现问题 正在重试");
    body_end_buf = (char*)malloc(50 * sizeof(char));
  }
  memset(body_end_buf, 0, 50 * sizeof(char));

  // 申请响应数据缓存
  char* response_buf = NULL;
  response_buf = (char*)malloc(ASR_HTTP_RESPONSE_BUF_MAX * sizeof(char));
  while (!response_buf && sevetest30_asr_running_flag)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGE(TAG, "申请response_buf资源发现问题 正在重试");
    response_buf = (char*)malloc(ASR_HTTP_RESPONSE_BUF_MAX * sizeof(char));
  }
  memset(response_buf, 0, ASR_HTTP_RESPONSE_BUF_MAX * sizeof(char));

  ESP_LOGW(TAG, "正在监听麦克风阵列");
  // 进入任务主循环
  while (sevetest30_asr_running_flag)
  {
  RESTART:
    vTaskDelay(pdMS_TO_TICKS(10)); // 为idle提供的必须延时

    // 读取raw数据并输入VAD
    while (raw_stream_read(raw_read, (char*)vad_buf, VAD_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(short)) == ESP_FAIL && sevetest30_asr_running_flag)
    {
      vTaskDelay(pdMS_TO_TICKS(1000));
      ESP_LOGE(TAG, "读取音频数据发现问题 正在重试 raw_read_state %d", audio_element_get_state(raw_read));
    }
    vad_state = vad_process(vad_inst, vad_buf, ASR_cfg_buf->rate, VAD_FRAME_LENGTH);

    if (vad_state == VAD_SPEECH)
    {
      ESP_LOGI(TAG, "语音活动进行中");

      vad_stop_counter = 0; // 语音活动继续而不停止
      vad_keep_counter++;   // 语音保持时间增加，直到发送阈值

      // 读取识别数据帧
      raw_stream_read(raw_read, asr_data_buf, ASR_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(char));

      // 保存到请求体
      size_t base64_out_len = 0;
      char* base64_buf = base64_encode_re(asr_data_buf, ASR_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(char), &base64_out_len);
      strcat(request_body_buf, base64_buf);

      asr_len += ASR_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(char);
      record_save_times++;
    }

    if (vad_state == VAD_SILENCE)
    {
      vad_stop_counter++; // 语音活动有停止的趋势

      // 如果停顿时间超时 语音活动结束
      if (vad_stop_counter >= ASR_cfg_buf->stop_threshold)
      {
        vad_stop_counter = 0; // 复位
        // 如果录制的语音达到发送阈值
        if (vad_keep_counter >= ASR_cfg_buf->send_threshold)
        {
        POST_SEND:

          vad_keep_counter = 0; // 复位

          // 完善请求体
          //  {"format":"pcm","rate":%d,"channel":1,"token":"%s","cuid":"%d","speech":" - - - - -    ","len":%d}
          snprintf(body_end_buf, 50, "\",\"len\":%d}", asr_len);
          strcat(request_body_buf, body_end_buf);

          // 对服务器发送请求
          esp_err_t err_flag = ESP_OK;

          err_flag |= esp_http_client_set_timeout_ms(client_handle, ASR_TIMEOUT_MS);

          err_flag |= esp_http_client_open(client_handle, strlen(request_body_buf));

          if (err_flag != ESP_OK)
          {
            ESP_LOGE(TAG, "配置连接时出现问题 -> %s", http_config.url);
            esp_http_client_close(client_handle); // 关闭连接
            // 重置数据
            record_save_times = 0;
            asr_len = 0;
            memset(request_body_buf, 0, ASR_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(char) * ASR_cfg_buf->record_save_times_max / 3 * 4 + 2048 * sizeof(char));
            sprintf(request_body_buf, "{\"format\":\"pcm\",\"rate\":%d,\"channel\":1,\"token\":\"%s\",\"cuid\":\"%d\",\"speech\":\"", ASR_cfg_buf->rate, baidu_access_token, mac);
            esp_http_client_close(client_handle); // 关闭连接
            goto RESTART;
          }

          ESP_LOGW(TAG, "等待识别完成");
          esp_http_client_write(client_handle, request_body_buf, strlen(request_body_buf));

          if (http_check_response_content(client_handle) != ESP_OK)
          {
            ESP_LOGE(TAG, "识别出现问题，请提高音量降低语速并重试");
            // 重置数据
            record_save_times = 0;
            asr_len = 0;
            memset(request_body_buf, 0, ASR_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(char) * ASR_cfg_buf->record_save_times_max / 3 * 4 + 2048 * sizeof(char));
            sprintf(request_body_buf, "{\"format\":\"pcm\",\"rate\":%d,\"channel\":1,\"token\":\"%s\",\"cuid\":\"%d\",\"speech\":\"", ASR_cfg_buf->rate, baidu_access_token, mac);
            esp_http_client_close(client_handle); // 关闭连接
            goto RESTART;
          }

          esp_http_client_read_response(client_handle, response_buf, ASR_HTTP_RESPONSE_BUF_MAX);

          asr_data_save_result(response_buf);

          esp_http_client_close(client_handle); // 关闭连接

          // 重置数据
          record_save_times = 0;
          asr_len = 0;
          memset(request_body_buf, 0, ASR_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(char) * ASR_cfg_buf->record_save_times_max / 3 * 4 + 2048 * sizeof(char));
          // {"format":"pcm","rate":%d,"channel":1,"token":"%s","cuid":"%d","speech":" - - - - -    ","len":%d}
          sprintf(request_body_buf, "{\"format\":\"pcm\",\"rate\":%d,\"channel\":1,\"token\":\"%s\",\"cuid\":\"%d\",\"speech\":\"", ASR_cfg_buf->rate, baidu_access_token, mac);
        }
      }
    }
    // 数据大小达到最大 强制发送
    if (record_save_times >= ASR_cfg_buf->record_save_times_max)
    {
      ESP_LOGW(TAG, "语音数据大小达到最大值");
      goto POST_SEND;
    }
  }

  //禁止外部任务对该I2S端口访问
  running_i2s_port = -1;

  // 清理VAD
  vad_destroy(vad_inst);

  // 清理连接缓存
  esp_http_client_cleanup(client_handle);

  // 释放数据缓存
  free(vad_buf);
  free(asr_data_buf);
  free(body_end_buf);
  free(response_buf);
  free(request_body_buf);
  vad_buf = NULL;
  asr_data_buf = NULL;
  body_end_buf = NULL;
  response_buf = NULL;
  request_body_buf = NULL;

  // 清理音频通道
  audio_pipeline_stop(pipeline);
  audio_pipeline_wait_for_stop(pipeline);
  audio_pipeline_terminate(pipeline);
  audio_pipeline_remove_listener(pipeline);

  audio_pipeline_unregister(pipeline, i2s_stream_reader);
  audio_pipeline_unregister(pipeline, audio_rsp_filter);
  audio_pipeline_unregister(pipeline, raw_read);

  audio_pipeline_deinit(pipeline);
  audio_element_deinit(i2s_stream_reader);
  audio_element_deinit(audio_rsp_filter);
  audio_element_deinit(raw_read);

  pipeline = NULL;
  i2s_stream_reader = NULL;
  audio_rsp_filter = NULL;
  raw_read = NULL;

  vTaskDelete(NULL);
}

/// @brief 重置元素配置数据到默认值，之后您可以针对性修改某些参数，接着运行元素配置函数
void element_cfg_data_reset()
{
  // pipeline
  audio_pipeline_cfg_t pipeline_cfg_buf = DEFAULT_AUDIO_PIPELINE_CONFIG();
  pipeline_cfg = pipeline_cfg_buf;

  // http流
  http_stream_cfg_t http_cfg_buf = HTTP_STREAM_CFG_DEFAULT();
  http_cfg = http_cfg_buf;

  http_cfg.type = AUDIO_STREAM_READER;

  // I2S
  i2s_stream_cfg_t i2s_cfg_buf = I2S_STREAM_CFG_DEFAULT();
  i2s_cfg = i2s_cfg_buf;

  i2s_cfg.i2s_config.sample_rate = CODEC_ADC_SAMPLE_RATE;
  i2s_cfg.i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  i2s_cfg.i2s_config.dma_buf_len = I2S_DMA_BUF_SIZE;

  // MP3解码器
  mp3_decoder_cfg_t mp3_cfg_buf = DEFAULT_MP3_DECODER_CONFIG();
  mp3_cfg = mp3_cfg_buf;

  mp3_cfg.task_core = MUSIC_MP3_DECODER_TASK_CORE;

  // 语音滤波
  rsp_filter_cfg_t rsp_cfg_buf = DEFAULT_RESAMPLE_FILTER_CONFIG();
  rsp_cfg = rsp_cfg_buf;
  rsp_cfg.src_rate = CODEC_ADC_SAMPLE_RATE;
  rsp_cfg.src_ch = 2;
  rsp_cfg.dest_rate = CODEC_ADC_SAMPLE_RATE;
  rsp_cfg.dest_ch = 2;

  // 原始流读取
  raw_cfg.out_rb_size = 8 * 1024;
  raw_cfg.type = AUDIO_STREAM_READER;
}

/// @brief 以现在的存储的配置初始化所有选定link的音频元素
esp_err_t audio_element_all_init(const char* link_tag[], int link_num)
{
  const char* TAG = "audio_element_all_init";

  if (!pipeline)
  {
    pipeline = audio_pipeline_init(&pipeline_cfg);
    if (!pipeline)
      return ESP_FAIL;
  }

  for (int i = 0; i < link_num; i++)
  {
    if (!strcasecmp(link_tag[i], "http") && !http_stream_reader)
    {
      http_stream_reader = http_stream_init(&http_cfg);
      if (!http_stream_reader)
        return ESP_FAIL;
      else
        audio_pipeline_register(pipeline, http_stream_reader, "http");
    }

    if (!strcasecmp(link_tag[i], "mp3") && !mp3_decoder)
    {
      mp3_decoder = mp3_decoder_init(&mp3_cfg);
      if (!mp3_decoder)
        return ESP_FAIL;
      else
        audio_pipeline_register(pipeline, mp3_decoder, "mp3");
    }

    if (!strcasecmp(link_tag[i], "filter") && !audio_rsp_filter)
    {
      audio_rsp_filter = rsp_filter_init(&rsp_cfg);
      if (!audio_rsp_filter)
        return ESP_FAIL;
      else
        audio_pipeline_register(pipeline, audio_rsp_filter, "filter");
    }

    if (!strcasecmp(link_tag[i], "raw") && !raw_read)
    {
      raw_read = raw_stream_init(&raw_cfg);
      if (!raw_read)
        return ESP_FAIL;
      else
        audio_pipeline_register(pipeline, raw_read, "raw");
    }

    if (!strcasecmp(link_tag[i], "i2s") && i2s_stream_writer == NULL && i2s_stream_reader != NULL)
    {
      i2s_cfg.i2s_port = CODEC_DAC_I2S_PORT;
      i2s_cfg.type = AUDIO_STREAM_WRITER;
      i2s_stream_writer = i2s_stream_init(&i2s_cfg);
      if (!i2s_stream_writer)
        return ESP_FAIL;
      else
        audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");
    }
    else if (!strcasecmp(link_tag[i], "i2s") && i2s_stream_reader == NULL && i2s_stream_writer != NULL)
    {
      i2s_cfg.i2s_port = CODEC_ADC_I2S_PORT;
      i2s_cfg.type = AUDIO_STREAM_READER;
      i2s_stream_reader = i2s_stream_init(&i2s_cfg);
      if (!i2s_stream_reader)
        return ESP_FAIL;
      else
        audio_pipeline_register(pipeline, i2s_stream_reader, "i2s");
    }
    else if (!strcasecmp(link_tag[i], "i2s") && i2s_stream_reader != NULL && i2s_stream_writer != NULL)
    {
      ESP_LOGE(TAG, "初始化i2s_stream元素时发现问题");
      return ESP_FAIL;
    }
  }

  return ESP_OK;
}

void http_i2s_mp3_music_start(TaskFunction_t running_event, UBaseType_t priority)
{
  // 设置事件监听
  audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
  common_mp3_evt = audio_event_iface_init(&evt_cfg);
  audio_pipeline_set_listener(pipeline, common_mp3_evt);
  audio_event_iface_set_listener(esp_periph_set_get_event_iface(se30_periph_set_handle), common_mp3_evt);
  // 运行音频通道
  audio_pipeline_run(pipeline);
  // 初始化clk,之后真实的音频播放配置由mp3文件自动变更
  i2s_stream_set_clk(i2s_stream_writer, 44100, 16, 2);
  // 事件监听任务启动
  xTaskCreatePinnedToCore(running_event, "http_i2s_mp3_music_run", MUSIC_PLAY_EVT_TASK_STACK_SIZE, NULL, priority, NULL, MUSIC_PLAY_EVT_TASK_CORE);
}

void i2s_filter_raw_start(TaskFunction_t running_event, UBaseType_t priority)
{
  board_ctrl_t board_ctrl;
  board_ctrl.codec_audio_hal_ctrl = AUDIO_HAL_CTRL_START;
  board_ctrl.codec_mode = AUDIO_HAL_CODEC_MODE_ENCODE;
  sevetest30_board_ctrl(&board_ctrl, BOARD_CTRL_CODEC_MODE_AND_STATUS);

  audio_pipeline_run(pipeline);
  xTaskCreatePinnedToCore(running_event, "i2s_filter_raw_run", ASR_EVT_TASK_STACK_SIZE, NULL, priority, NULL, ASR_EVT_TASK_CORE);
}

/// @brief url音乐播放
/// @param url 导入音乐url
/// @param priority 任务优先级
void music_url_play(const char* url, UBaseType_t priority)
{
  const char* TAG = "music_url_play";

  // common_mp3_evt不为空说明还有音频事件运行中
  if (common_mp3_evt != NULL) {
    ESP_LOGE(TAG, "播放繁忙中，无法准备新播放任务");
    return;
  }
  else
  {
    sevetest30_music_running_flag = true;
    common_mp3_evt = audio_calloc(1, sizeof(audio_event_iface_handle_t)); // 立即申请内存使得common_mp3_evt不为空来锁住其他任务
  }

  element_cfg_data_reset();

  i2s_cfg.i2s_port = CODEC_DAC_I2S_PORT;
  running_i2s_port = i2s_cfg.i2s_port;

  i2s_stream_reader = audio_calloc(1, sizeof(audio_element_handle_t)); // 锁定i2s_stream_reader，禁止初始化

  const char* link_tag[3] = { "http", "mp3", "i2s" };


  if (audio_element_all_init(&link_tag[0], 3) != ESP_OK)
  {
    ESP_LOGE(TAG, "初始化音频元素时发现问题,任务无法启动");
    sevetest30_music_running_flag = false;
    return;
  }

  i2s_stream_reader = NULL;

  audio_pipeline_link(pipeline, &link_tag[0], 3);
  audio_element_set_uri(http_stream_reader, url);

  http_i2s_mp3_music_start(&common_mp3_running_event, priority);
}

/// @brief 请求TTS服务并播放
/// @param tts_cfg  tts配置
/// @param priority 任务优先级
void tts_service_play(baidu_TTS_cfg_t* tts_cfg, UBaseType_t priority)
{
  // common_mp3_evt不为空说明还有音频事件运行中，进行等待再继续
  if (common_mp3_evt != NULL)
  {
    ESP_LOGE("tts_service_play", "播放繁忙中，无法准备新播放任务");
    return;
  }
  else
  {
    sevetest30_music_running_flag = true;
    common_mp3_evt = audio_calloc(1, sizeof(audio_event_iface_handle_t)); // 立即申请内存使得common_mp3_evt不为空来锁住其他任务
  }

  element_cfg_data_reset();

  i2s_cfg.i2s_port = CODEC_DAC_I2S_PORT;
  running_i2s_port = i2s_cfg.i2s_port;

  http_cfg.event_handle = _TTS_get_token_handle;
  if (TTS_cfg_buf == NULL)
    TTS_cfg_buf = audio_calloc(1, sizeof(baidu_TTS_cfg_t));
  TTS_cfg_buf->tex = tts_cfg->tex;
  TTS_cfg_buf->spd = tts_cfg->spd;
  TTS_cfg_buf->pit = tts_cfg->pit;
  TTS_cfg_buf->vol = tts_cfg->vol;
  TTS_cfg_buf->per = tts_cfg->per;
  i2s_stream_reader = audio_calloc(1, sizeof(audio_element_handle_t)); // 锁定i2s_stream_reader，禁止初始化

  const char* link_tag[3] = { "http", "mp3", "i2s" };
  if (audio_element_all_init(&link_tag[0], 3) != ESP_OK)
  {
    ESP_LOGE("tts_service_play", "准备音频元素时发现问题");
    sevetest30_music_running_flag = false;
    return;
  }

  i2s_stream_reader = NULL;

  audio_pipeline_link(pipeline, &link_tag[0], 3);
  audio_element_set_uri(http_stream_reader, BAIDU_TTS_ENDPOINT);

  http_i2s_mp3_music_start(&common_mp3_running_event, priority);
}



/// @brief 启动语音识别服务，启动后不断地自动监听并完成识别,是一个不断循环识别的任务
/// @param asr_cfg asr配置
/// @param priority 任务优先级
void asr_service_start(baidu_ASR_cfg_t* asr_cfg, UBaseType_t priority)
{
  if (sevetest30_asr_running_flag)
  {
    ESP_LOGE("asr_service_start", "识别繁忙中，无法准备新识别任务");
    return;
  }
  else
  {
    sevetest30_asr_running_flag = true;
  }

  // 获取token
  while (!baidu_access_token)
  {
    baidu_access_token = baidu_get_access_token(CONFIG_BAIDU_SPEECH_ACCESS_KEY, CONFIG_BAIDU_SPEECH_SECRET_KEY);
    if (!baidu_access_token)
    {
      ESP_LOGE("asr_service_start", "获取token时发现问题");
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  element_cfg_data_reset();

  i2s_cfg.i2s_port = CODEC_ADC_I2S_PORT;
  running_i2s_port = i2s_cfg.i2s_port;

  i2s_cfg.type = AUDIO_STREAM_READER;
  rsp_cfg.dest_ch = 1;
  rsp_cfg.dest_rate = asr_cfg->rate;
  i2s_stream_writer = audio_calloc(1, sizeof(audio_element_handle_t)); // 锁定i2s_stream_writer，禁止初始化

  if (ASR_cfg_buf == NULL)
    ASR_cfg_buf = audio_calloc(1, sizeof(baidu_ASR_cfg_t));
  ASR_cfg_buf->rate = asr_cfg->rate;
  ASR_cfg_buf->dev_pid = asr_cfg->dev_pid;
  ASR_cfg_buf->stop_threshold = asr_cfg->stop_threshold;
  ASR_cfg_buf->send_threshold = asr_cfg->send_threshold;
  ASR_cfg_buf->record_save_times_max = asr_cfg->record_save_times_max;

  const char* link_tag[3] = { "i2s", "filter", "raw" };

  if (audio_element_all_init(&link_tag[0], 3) != ESP_OK)
  {
    ESP_LOGE("asr_service_start", "准备音频元素时发现问题");
    sevetest30_asr_running_flag = false;
    return;
  }

  i2s_stream_writer = NULL;

  audio_pipeline_link(pipeline, &link_tag[0], 3);

  i2s_filter_raw_start(common_asr_running_event, priority);


}

// 以下使用了官方提供的 https://github.com/espressif/esp-adf/tree/master/examples/cloud_services/pipeline_baidu_speech_mp3 例程,通过post进行的设定部分进行了修改,非常感谢

/// @brief TTS任务设置的hook
int _TTS_get_token_handle(http_stream_event_msg_t* msg)
{
  static char request_data[4096];

  // 从消息中获取句柄
  esp_http_client_handle_t http_client = (esp_http_client_handle_t)msg->http_client;

  // 如果事件属性不是请求前调用，跳出
  if (msg->event_id != HTTP_STREAM_PRE_REQUEST)
  {
    return ESP_OK;
  }

  // 获取token
  while (!baidu_access_token)
  {
    baidu_access_token = baidu_get_access_token(CONFIG_BAIDU_SPEECH_ACCESS_KEY, CONFIG_BAIDU_SPEECH_SECRET_KEY);
    if (!baidu_access_token)
    {
      ESP_LOGE("_TTS_get_token_handle", "获取token时发现问题");
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  // 设定语音合成配置
  int mac = 0;
  esp_base_mac_addr_get((uint8_t*)&mac);

  if (!TTS_cfg_buf)
  {
    ESP_LOGE("_TTS_get_token_handle", "TTS_cfg_buf未成功存储配置数据");
    return ESP_FAIL;
  }

  int data_len = snprintf(request_data, 4096, "lan=zh&ctp=1&tok=%s&cuid=%d&vol=%d&spd=%d&pit=%d&per=%d&tex=%s", baidu_access_token, mac,
    TTS_cfg_buf->vol, TTS_cfg_buf->spd, TTS_cfg_buf->pit, TTS_cfg_buf->per, TTS_cfg_buf->tex);

  esp_http_client_set_post_field(http_client, request_data, data_len);
  esp_http_client_set_method(http_client, HTTP_METHOD_POST);

  return ESP_OK;
}
