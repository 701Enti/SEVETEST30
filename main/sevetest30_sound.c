// 该文件由701Enti编写，包含一些sevetest30的  音频数据获取与硬件调度，以支持TTS,语音识别，音乐API播放音乐时的硬件驱动等工作
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：参考了官方提供的pipeline_baidu_speech_mp3例程,非常感谢ESPRESSIF
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.12.17

#include "sevetest30_sound.h"
#include "sevetest30_IWEDA.h"

#include "esp_system.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
#include "wav_encoder.h"
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
#include "base64.h"

#include "audio_recorder.h"
#include "recorder_encoder.h"

#include "audio_idf_version.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
#include "esp_netif.h"
#else
#include "tcpip_adapter.h"
#endif

bool volatile sevetest30_music_running_flag = false;
bool volatile sevetest30_asr_running_flag = false;

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
audio_event_iface_handle_t voice_encode_evt = NULL;

// 服务定义
baidu_TTS_cfg_t *TTS_cfg_buf = NULL;
baidu_ASR_cfg_t *ASR_cfg_buf = NULL;

char *baidu_access_token = NULL;

enum _rec_msg_id {
    REC_START = 1,
    REC_STOP,
    REC_CANCEL,
};

int _TTS_get_token_handle(http_stream_event_msg_t *msg);
static esp_err_t rec_engine_cb(audio_rec_evt_t *event, void *user_data);

void common_mp3_running_event()
{
  static const char *TAG = "common_mp3_running_event";
  board_ctrl_t *ctrl_buf = board_status_get();
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
    if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)mp3_decoder && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO)
    {
      audio_element_info_t music_info = {0};
      audio_element_getinfo(mp3_decoder, &music_info);

      ESP_LOGI(TAG, "收到来自mp3_decoder音频, 采样率=%d, 位深=%d, 通道数=%d",
               music_info.sample_rates, music_info.bits, music_info.channels);

      i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
      continue;
    }

    // 消息性质为[音频元素类->来自i2s_stream_writer->状态报告] 内容 为 停止状态 或 完成状态 就结束播放事件
    if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)i2s_stream_writer && msg.cmd == AEL_MSG_CMD_REPORT_STATUS && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED)))
    {
      ESP_LOGI(TAG, "播放完毕");
      break;
    }
  }
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
  /// 重置使用的元素
  // audio_pipeline_deinit(pipeline);
  // audio_element_deinit(http_stream_reader);
  // audio_element_deinit(i2s_stream_writer);
  // audio_element_deinit(mp3_decoder);

  pipeline = NULL;
  http_stream_reader = NULL;
  i2s_stream_writer = NULL;
  i2s_stream_reader = NULL; // 解锁，允许初始化
  mp3_decoder = NULL;
  audio_rsp_filter = NULL;

  common_mp3_evt = NULL; // 释放句柄，表示任务的结束
  sevetest30_music_running_flag = false;
  vTaskDelete(NULL);
}

void common_asr_running_event()
{
  static const char *TAG = "common_asr_running_event";

  // 准备POST请求

  // 获取设备MAC地址
  int mac = 0;
  esp_base_mac_addr_get((uint8_t *)&mac);

  // 初始化http_client
  esp_http_client_config_t http_config;
  memset(&http_config, 0, sizeof(http_config));
  http_config.url = BAIDU_ASR_URL;       // 导入url
  http_config.method = HTTP_METHOD_POST; // 使用POST请求
  esp_http_client_handle_t client_handle = esp_http_client_init(&http_config);

  // 设置HTTP-HEADER
  esp_http_client_set_header(client_handle, BAIDU_ASR_HEADER_KEY, BAIDU_ASR_HEADER_VALUE);

  // 准备HTTP-BODY
  char *request_body_buf = NULL; // 请求体空间 = 最大音频数据大小（PCM转换为base64会增大1/3） + 其他附加数据大小
  request_body_buf = (char *)malloc(VAD_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(char) * ASR_cfg_buf->record_save_times_max / 3 * 4 + 2048 * sizeof(char));
  while (!request_body_buf && sevetest30_asr_running_flag)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGE(TAG, "申请request_body_buf资源发现问题 正在重试");
    request_body_buf = (char *)malloc(VAD_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(char) * ASR_cfg_buf->record_save_times_max / 3 * 4 + 2048 * sizeof(char));
  }
  memset(request_body_buf, 0, VAD_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(char) * ASR_cfg_buf->record_save_times_max / 3 * 4 + 2048 * sizeof(char));

  // 获取token
  if (!baidu_access_token)
    baidu_access_token = baidu_get_access_token(CONFIG_BAIDU_ACCESS_KEY, CONFIG_BAIDU_SECRET_KEY);
  if (!baidu_access_token)
    ESP_LOGE(TAG, "获取token时发现问题");

  // {"format":"wav","rate":%d,"channel":1,"token":"%s","cuid":"%d","speech":" - - - - -    ","len":%d}
  sprintf(request_body_buf, "{\"format\":\"wav\",\"rate\":%d,\"channel\":1,\"token\":\"%s\",\"cuid\":\"%d\",\"speech\":\"", ASR_cfg_buf->rate, baidu_access_token, mac);

  ESP_LOGE(TAG, "%s", request_body_buf);

  int record_save_times = 0; // 录制并保存音频数据的次数（防溢出）
  int asr_len = 0;           // 实际原始音频数据长度，作为ASR时的len参数

  // 配置VAD 语音活动监测
  int vad_stop_counter = 0; // 记录没有监测到语音的周期数
  int vad_keep_counter = 0; // 记录监测到语音的周期数

  vad_handle_t vad_inst = vad_create(VAD_MODE_4);
  vad_state_t vad_state = VAD_SILENCE;

  char *vad_buf = NULL;
  vad_buf = (int16_t *)malloc(VAD_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(char));
  while (!vad_buf && sevetest30_asr_running_flag)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGE(TAG, "申请vad_buf资源发现问题 正在重试");
    vad_buf = (int16_t *)malloc(VAD_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(char));
  }

  // 配置 audio_recorder 语音录制引擎
  // 初始化WAV编码器
  wav_encoder_cfg_t asr_wav_encoder_cfg = DEFAULT_WAV_ENCODER_CONFIG();
  audio_element_handle_t asr_wav_encoder_element_handle = wav_encoder_init(&asr_wav_encoder_cfg);
  // 初始化录制编码
  recorder_encoder_cfg_t recorder_encoder_cfg = {
      .resample = audio_rsp_filter,
      .encoder = asr_wav_encoder_element_handle,
  };
  recorder_encoder_iface_t *encoder_iface;
  recorder_encoder_handle_t rec_encoder_handle = recorder_encoder_create(&recorder_encoder_cfg, &encoder_iface);
  // 初始化录制引擎
  audio_rec_cfg_t asr_rec_cfg = {
      .pinned_core = ASR_REC_TASK_CORE,
      .task_prio = ASR_REC_TASK_PRIORITY,
      .task_size = ASR_REC_TASK_STACK_SIZE,
      .event_cb  = rec_engine_cb,
      .encoder_handle = rec_encoder_handle,
      .encoder_iface = encoder_iface,
  };
  audio_rec_handle_t asr_rec_handle = audio_recorder_create(&asr_rec_cfg);
  // 申请wav_data_buf
  char *wav_data_buf = NULL;
  wav_data_buf = (int16_t *)malloc(VAD_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(char));
  while (!wav_data_buf && sevetest30_asr_running_flag)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGE(TAG, "申请wav_data_buf资源发现问题 正在重试");
    wav_data_buf = (int16_t *)malloc(VAD_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(char));
  }
  memset(wav_data_buf, 0, VAD_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(char));

  // 进入任务主循环
  while (sevetest30_asr_running_flag)
  {
    vTaskDelay(pdMS_TO_TICKS(10)); // 为idle提供的必须延时

    // 读取raw数据并输入VAD
    raw_stream_read(raw_read, vad_buf, VAD_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(char));
    vad_state = vad_process(vad_inst, (int16_t *)vad_buf, ASR_cfg_buf->rate, VAD_FRAME_LENGTH);

    // 声音不总是连续的，尽管人类的语言听上去是连贯的，但麦克风收到的声音其实是断断续续的
    // 因为VAD非常灵敏，他可以识别到极小的停顿，所以vad_state会在 VAD_SPEECH VAD_SILENCE 不断跳变
    // 我们可以累加VAD_SILENCE次数，监测它出现的频率，判断是不是真的停止语音还是说话的正常停顿

    if (vad_state == VAD_SPEECH)
    {

      vad_stop_counter = 0; // 语音活动继续而不停止
      vad_keep_counter++;   // 语音保持时间增加，直到发送阈值

      audio_recorder_trigger_start(asr_rec_handle);

      // 读取WAV数据
      audio_recorder_data_read(asr_rec_handle,wav_data_buf,VAD_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(char),portMAX_DELAY);
      // 保存到请求体
      size_t base64_out_len = 0;
      strcat(request_body_buf, base64_encode(wav_data_buf, VAD_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(char), &base64_out_len));
      asr_len += VAD_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(char);
      record_save_times++;
    }

    if (vad_state == VAD_SILENCE)
    {
      vad_stop_counter++; // 语音活动有停止的趋势
      audio_recorder_trigger_stop(asr_rec_handle);


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
          //  {"format":"wav","rate":%d,"channel":1,"token":"%s","cuid":"%d","speech":" - - - - -    ","len":%d}
          char *body_end_buf = (char *)malloc(50 * sizeof(char));
          memset(body_end_buf, 0, 50 * sizeof(char));
          snprintf(body_end_buf, 50, "\",\"len\":%d}", asr_len);
          strcat(request_body_buf, body_end_buf);

          // 对服务器发送请求
          esp_err_t err_flag = ESP_OK;

          err_flag |= esp_http_client_set_timeout_ms(client_handle, portMAX_DELAY);

          err_flag |= esp_http_client_open(client_handle, strlen(request_body_buf));

          if (err_flag != ESP_OK)
          {
            ESP_LOGE(TAG, "配置连接时出现问题 -> %s", BAIDU_ASR_URL);
            esp_http_client_close(client_handle); // 关闭连接
          }

          esp_http_client_write(client_handle, request_body_buf, strlen(request_body_buf));

          ESP_LOGE(TAG, "%s", request_body_buf);

          // 校验响应状态与数据
          esp_http_client_fetch_headers(client_handle);                //   接收消息头
          int status = esp_http_client_get_status_code(client_handle); // 获取消息头中的响应状态信息
          int len = esp_http_client_get_content_length(client_handle); // 获取消息头中的总数据大小信息

          if (status != 200)
          {
            if (esp_http_client_is_chunked_response(client_handle) == true)
              ESP_LOGE(TAG, "本次传输响应数据已分块 但是处于不正常的响应状态 -> %d 数据将不会保存", status);
            else
              ESP_LOGE(TAG, "不正常的响应状态 -> %d 共接收到 -> %d 数据将不会保存", status, len);
          }
          else
          {
            if (esp_http_client_is_chunked_response(client_handle) == true)
              ESP_LOGI(TAG, "连接就绪，响应状态-> %d ,本次传输响应数据已分块", status);
            else
              ESP_LOGI(TAG, "连接就绪，响应状态-> %d ，响应数据共 %d", status, len);
          }

          char *response_buf = (char *)malloc(ASR_HTTP_RESPONSE_BUF_MAX * sizeof(char));
          memset(response_buf, 0, ASR_HTTP_RESPONSE_BUF_MAX * sizeof(char));
          esp_http_client_read_response(client_handle, response_buf, len);

          ESP_LOGE(TAG, "%s", response_buf);

          esp_http_client_close(client_handle); // 关闭连接

          // 重置数据
          record_save_times = 0;
          asr_len = 0;
          memset(request_body_buf, 0, VAD_FRAME_LENGTH * ASR_cfg_buf->rate / 1000 * sizeof(char) * ASR_cfg_buf->record_save_times_max / 3 * 4 + 2048 * sizeof(char));
          // {"format":"wav","rate":%d,"channel":1,"token":"%s","cuid":"%d","speech":" - - - - -    ","len":%d}
          sprintf(request_body_buf, "{\"format\":\"wav\",\"rate\":%d,\"channel\":1,\"token\":\"%s\",\"cuid\":\"%d\",\"speech\":\"", ASR_cfg_buf->rate, baidu_access_token, mac);

          // free(.)//debugdebugdebugdebugdebugdebugdebugdebugdebug
          vTaskDelete(NULL); // debugdebugdebugdebugdebugdebugdebugdebugdebug
        }
      }
    }
    // 数据大小达到最大 强制发送
    if (record_save_times >= ASR_cfg_buf->record_save_times_max)
    {
      ESP_LOGW(TAG, "语音数据大小达到最大值");
      audio_recorder_trigger_stop(asr_rec_handle);
      goto POST_SEND;
    }
  }
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

  // 原始流读取
  raw_cfg.out_rb_size = 8 * 1024;
  raw_cfg.type = AUDIO_STREAM_READER;
}

/// @brief 以现在的存储的配置初始化所有选定link的音频元素
esp_err_t audio_element_all_init(const char *link_tag[], int link_num)
{
  const char *TAG = "audio_element_all_init";

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

void i2s_filter_raw_VAD_start(TaskFunction_t running_event, UBaseType_t priority)
{
  board_ctrl_t board_ctrl;
  board_ctrl.codec_audio_hal_ctrl = AUDIO_HAL_CTRL_START;
  board_ctrl.codec_mode = AUDIO_HAL_CODEC_MODE_ENCODE;
  sevetest30_board_ctrl(&board_ctrl, BOARD_CTRL_CODEC_MODE_AND_STATUS);

  audio_pipeline_run(pipeline);
  xTaskCreatePinnedToCore(running_event, "i2s_filter_raw_VAD_run", ASR_EVT_TASK_STACK_SIZE, NULL, priority, NULL, ASR_EVT_TASK_CORE);
}

/// @brief uri音乐播放
/// @param uri 导入音乐uri
/// @param priority 任务优先级
void music_uri_play(const char *uri, UBaseType_t priority)
{
  // common_mp3_evt不为空说明还有音频事件运行中，进行等待再继续
  if (common_mp3_evt != NULL)
  {
    ESP_LOGE("music_uri_play", "播放繁忙中，无法准备新播放任务");
    return;
  }
  else
  {
    common_mp3_evt = audio_calloc(1, sizeof(audio_event_iface_handle_t)); // 立即申请内存使得common_mp3_evt不为空来锁住其他任务
    sevetest30_music_running_flag = true;
  }

  element_cfg_data_reset();

  if (i2s_stream_reader == NULL)
    i2s_stream_reader = audio_calloc(1, 1); // 锁定i2s_stream_reader，禁止初始化

  const char *link_tag[3] = {"http", "mp3", "i2s"};

  if (audio_element_all_init(&link_tag[0], 3) != ESP_OK)
  {
    ESP_LOGE("music_uri_play", "准备音频元素时发现问题");
    return;
  }

  audio_pipeline_link(pipeline, &link_tag[0], 3);
  audio_element_set_uri(http_stream_reader, uri);

  http_i2s_mp3_music_start(&common_mp3_running_event, priority);
}

/// @brief 请求TTS服务并播放
/// @param tts_cfg  tts配置
/// @param priority 任务优先级
void tts_service_play(baidu_TTS_cfg_t *tts_cfg, UBaseType_t priority)
{

  // common_mp3_evt不为空说明还有音频事件运行中，进行等待再继续
  if (common_mp3_evt != NULL)
  {
    ESP_LOGE("tts_service_play", "播放繁忙中，无法准备新播放任务");
    return;
  }
  else
  {
    common_mp3_evt = audio_calloc(1, sizeof(audio_event_iface_handle_t)); // 立即申请内存使得common_mp3_evt不为空来锁住其他任务
    sevetest30_music_running_flag = true;
  }

  element_cfg_data_reset();

  http_cfg.event_handle = _TTS_get_token_handle;
  if (TTS_cfg_buf == NULL)
    TTS_cfg_buf = audio_calloc(1, sizeof(baidu_TTS_cfg_t));
  TTS_cfg_buf->tex = tts_cfg->tex;
  TTS_cfg_buf->spd = tts_cfg->spd;
  TTS_cfg_buf->pit = tts_cfg->pit;
  TTS_cfg_buf->vol = tts_cfg->vol;
  TTS_cfg_buf->per = tts_cfg->per;
  i2s_stream_reader = audio_calloc(1, 1); // 锁定i2s_stream_reader，禁止初始化

  const char *link_tag[3] = {"http", "mp3", "i2s"};
  if (audio_element_all_init(&link_tag[0], 3) != ESP_OK)
  {
    ESP_LOGE("tts_service_play", "准备音频元素时发现问题");
    return;
  }

  audio_pipeline_link(pipeline, &link_tag[0], 3);
  audio_element_set_uri(http_stream_reader, BAIDU_TTS_ENDPOINT);
  http_i2s_mp3_music_start(&common_mp3_running_event, priority);
}

/// @brief 启动语音识别服务，启动后不断地自动监听并完成识别
/// @param asr_cfg asr配置
/// @param priority 任务优先级
void asr_service_start(baidu_ASR_cfg_t *asr_cfg, UBaseType_t priority)
{
  // voice_encode_evt不为空说明还有音频事件运行中，进行等待再继续
  if (voice_encode_evt != NULL)
  {
    ESP_LOGE("asr_service_start", "识别繁忙中，无法准备新识别任务");
    return;
  }
  else
  {
    voice_encode_evt = audio_calloc(1, sizeof(int)); // 立即申请内存使得voice_encode_evt不为空来锁住其他任务
    sevetest30_asr_running_flag = true;
  }

  element_cfg_data_reset();

  i2s_cfg.type = AUDIO_STREAM_READER;

  rsp_cfg.src_rate = asr_cfg->rate;
  rsp_cfg.dest_rate = asr_cfg->rate;
  i2s_stream_writer = audio_calloc(1, 1); // 锁定i2s_stream_writer，禁止初始化

  if (ASR_cfg_buf == NULL)
    ASR_cfg_buf = audio_calloc(1, sizeof(baidu_ASR_cfg_t));
  ASR_cfg_buf->rate = asr_cfg->rate;
  ASR_cfg_buf->stop_threshold = asr_cfg->stop_threshold;
  ASR_cfg_buf->send_threshold = asr_cfg->send_threshold;
  ASR_cfg_buf->record_save_times_max = asr_cfg->record_save_times_max;

  const char *link_tag[3] = {"i2s", "filter", "raw"};

  if (audio_element_all_init(&link_tag[0], 3) != ESP_OK)
  {
    ESP_LOGE("asr_service_start", "准备音频元素时发现问题");
    return;
  }

  audio_pipeline_link(pipeline, &link_tag[0], 3);

  i2s_filter_raw_VAD_start(common_asr_running_event, priority);
}

// 以下使用了官方提供的 https://github.com/espressif/esp-adf/tree/master/examples/cloud_services/pipeline_baidu_speech_mp3 例程,通过post进行的设定部分进行了修改,非常感谢

/// @brief TTS任务设置的hook
int _TTS_get_token_handle(http_stream_event_msg_t *msg)
{
  static char request_data[4096];

  // 从消息中获取句柄
  esp_http_client_handle_t http_client = (esp_http_client_handle_t)msg->http_client;

  // 如果事件属性不是请求前调用，跳出
  if (msg->event_id != HTTP_STREAM_PRE_REQUEST)
  {
    return ESP_OK;
  }

  // 调用ESPADF框架内函数获取token
  if (!baidu_access_token)
    baidu_access_token = baidu_get_access_token(CONFIG_BAIDU_ACCESS_KEY, CONFIG_BAIDU_SECRET_KEY);
  if (!baidu_access_token)
  {
    ESP_LOGE("_TTS_get_token_handle", "获取token时发现问题");
    return ESP_FAIL;
  }

  // 设定语音合成配置
  int mac = 0;
  esp_base_mac_addr_get((uint8_t *)&mac);

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

//以下使用了官方提供的speech_recognition/wwe 例程,非常感谢
static esp_err_t rec_engine_cb(audio_rec_evt_t *event, void *user_data)
{
  static bool voice_reading = false;

  const char* TAG = "rec_engine_cb";
    if (AUDIO_REC_WAKEUP_START == event->type) {
        // recorder_sr_wakeup_result_t *wakeup_result = event->event_data;
        ESP_LOGI(TAG, "rec_engine_cb - REC_EVENT_WAKEUP_START");
        // ESP_LOGI(TAG, "wakeup: vol %f, mod idx %d, word idx %d", wakeup_result->data_volume, wakeup_result->wakenet_model_index, wakeup_result->wake_word_index);
        // esp_audio_sync_play(player, tone_uri[TONE_TYPE_DINGDONG], 0);
        // if (voice_reading) {
        //     int msg = REC_CANCEL;
        //     if (xQueueSend(rec_q, &msg, 0) != pdPASS) {
        //         ESP_LOGE(TAG, "rec cancel send failed");
        //     }
        // }
    } else if (AUDIO_REC_VAD_START == event->type) {
        ESP_LOGI(TAG, "rec_engine_cb - REC_EVENT_VAD_START");
        // if (!voice_reading) {
        //     int msg = REC_START;
        //     if (xQueueSend(rec_q, &msg, 0) != pdPASS) {
        //         ESP_LOGE(TAG, "rec start send failed");
        //     }
        // }
    } else if (AUDIO_REC_VAD_END == event->type) {
        ESP_LOGI(TAG, "rec_engine_cb - REC_EVENT_VAD_STOP");
        // if (voice_reading) {
        //     int msg = REC_STOP;
        //     if (xQueueSend(rec_q, &msg, 0) != pdPASS) {
        //         ESP_LOGE(TAG, "rec stop send failed");
        //     }
        // }

    } else if (AUDIO_REC_WAKEUP_END == event->type) {
        // ESP_LOGI(TAG, "rec_engine_cb - REC_EVENT_WAKEUP_END");
        // AUDIO_MEM_SHOW(TAG);
    } else if (AUDIO_REC_COMMAND_DECT <= event->type) {
        // recorder_sr_mn_result_t *mn_result = event->event_data;

        ESP_LOGI(TAG, "rec_engine_cb - AUDIO_REC_COMMAND_DECT");
        // ESP_LOGW(TAG, "command %d, phrase_id %d, prob %f, str: %s"
        //     , event->type, mn_result->phrase_id, mn_result->prob, mn_result->str);
        // esp_audio_sync_play(player, tone_uri[TONE_TYPE_HAODE], 0);
    } else {
        ESP_LOGE(TAG, "Unkown event");
    }
    return ESP_OK;
}