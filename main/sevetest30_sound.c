// 该文件由701Enti编写，包含一些sevetest30的  音频数据获取与硬件调度，以支持TTS,语音识别，网易云音乐API播放音乐时的硬件驱动等工作
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：参考了官方提供的pipeline_baidu_speech_mp3例程,非常感谢ESPRESSIF
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.12.17

#include "sevetest30_sound.h"
#include "sevetest30_IWEDA.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "board_ctrl.h"
#include "sdkconfig.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "http_stream.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"

#include "audio_mem.h"
#include "esp_peripherals.h"
#include "periph_wifi.h"
#include "board.h"
#include "esp_http_client.h"
#include "baidu_access_token.h"

#include "audio_idf_version.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
#include "esp_netif.h"
#else
#include "tcpip_adapter.h"
#endif

bool sevetest30_music_running_flag = false;

//音频元素句柄
audio_pipeline_handle_t pipeline=NULL;
audio_element_handle_t http_stream_reader=NULL;
audio_element_handle_t i2s_stream_writer=NULL;
audio_element_handle_t mp3_decoder=NULL;

//音频元素配置
audio_pipeline_cfg_t pipeline_cfg;
http_stream_cfg_t http_cfg;
i2s_stream_cfg_t i2s_cfg;
mp3_decoder_cfg_t mp3_cfg;

//事件监听
audio_event_iface_handle_t common_mp3_evt=NULL;

void common_mp3_running_event(){ 
     while (1) {
      vTaskDelay(pdMS_TO_TICKS(5000)); 
        static const char *TAG = "common_mp3_running_event";
        audio_event_iface_msg_t msg;
        //不断进行监听
        esp_err_t ret = audio_event_iface_listen(common_mp3_evt,&msg,portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "监听事件时出现问题 %d", ret);
            continue;
        }
        
        //消息性质为[音频元素类->来自mp3_decoder->音频信息报告] 进行音频硬件自适应
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT
            && msg.source == (void *) mp3_decoder
            && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            audio_element_info_t music_info = {0};
            audio_element_getinfo(mp3_decoder, &music_info);

            ESP_LOGI(TAG, "收到来自mp3_decoder音频信息, 采样率=%d, 位深=%d, 通道数=%d",
                     music_info.sample_rates, music_info.bits, music_info.channels);

            i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
            continue;
        }

        //消息性质为[音频元素类->来自i2s_stream_writer->状态报告] 内容 为 停止状态 或 完成状态 就结束播放事件
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) i2s_stream_writer
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
            && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
            ESP_LOGI(TAG,"播放完毕");
            break;
        }

    }
    //等待停止
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);
    //注销使用的元素
    audio_pipeline_unregister(pipeline, http_stream_reader);
    audio_pipeline_unregister(pipeline, i2s_stream_writer);
    audio_pipeline_unregister(pipeline, mp3_decoder);
    //删除监听
    audio_pipeline_remove_listener(pipeline);
    //暂停所有外设
    esp_periph_set_stop_all(se30_periph_set_handle);
    //删除监听总线
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(se30_periph_set_handle), common_mp3_evt);
    audio_event_iface_destroy(common_mp3_evt);
    ///重置使用的元素
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(http_stream_reader);
    audio_element_deinit(i2s_stream_writer);
    audio_element_deinit(mp3_decoder);    
    
    pipeline = NULL;
    http_stream_reader = NULL;
    i2s_stream_writer = NULL;
    mp3_decoder = NULL;

    common_mp3_evt = NULL;//释放句柄，表示任务的结束
    sevetest30_music_running_flag = false;
    vTaskDelete(NULL);
    
}

/// @brief 重置元素配置数据到默认值，之后您可以针对性修改某些参数，接着运行元素配置函数
void element_cfg_data_reset(){
    //pipeline
    audio_pipeline_cfg_t pipeline_cfg_buf=DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline_cfg = pipeline_cfg_buf;
    //http流
    http_stream_cfg_t http_cfg_buf=HTTP_STREAM_CFG_DEFAULT();
    http_cfg = http_cfg_buf;
    http_cfg.type = AUDIO_STREAM_READER;
    //I2S
    i2s_stream_cfg_t i2s_cfg_buf=I2S_STREAM_CFG_DEFAULT();
    i2s_cfg = i2s_cfg_buf;
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_cfg.i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    //MP3解码器
    mp3_decoder_cfg_t mp3_cfg_buf=DEFAULT_MP3_DECODER_CONFIG();
    mp3_cfg = mp3_cfg_buf;
}


/// @brief 以现在的存储的配置初始化所有音频元素
void audio_element_all_init(){
  if (pipeline==NULL){
  pipeline = audio_pipeline_init(&pipeline_cfg);
  mem_assert(pipeline);    
  }

  if (http_stream_reader==NULL)
  http_stream_reader = http_stream_init(&http_cfg);

  if (i2s_stream_writer==NULL)
  i2s_stream_writer = i2s_stream_init(&i2s_cfg);

  if (mp3_decoder==NULL)
  mp3_decoder = mp3_decoder_init(&mp3_cfg);

  audio_pipeline_register(pipeline, http_stream_reader, "http");
  audio_pipeline_register(pipeline, mp3_decoder,        "mp3");
  audio_pipeline_register(pipeline, i2s_stream_writer,  "i2s");
}


void http_i2s_mp3_music_start(const char* uri,TaskFunction_t running_event,UBaseType_t priority){
  //配置音乐uri
  audio_element_set_uri(http_stream_reader,uri);
  //设置事件监听
  audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
  common_mp3_evt = audio_event_iface_init(&evt_cfg);
  audio_pipeline_set_listener(pipeline,common_mp3_evt);
  audio_event_iface_set_listener(esp_periph_set_get_event_iface(se30_periph_set_handle),common_mp3_evt);
  //运行音频通道
  audio_pipeline_run(pipeline);
  //初始化clk,之后真实的音频播放格式由mp3文件自动变更
  i2s_stream_set_clk(i2s_stream_writer, 44100, 16, 2);
  // 事件监听任务启动
  xTaskCreatePinnedToCore(running_event, "http_i2s_mp3_music_run",MUSIC_PLAY_EVT_TASK_STACK_SIZE,NULL,priority,NULL,MUSIC_PLAY_EVT_TASK_CORE);
}


/// @brief 网易云音乐API_uri播放
/// @param uri 导入音乐uri
/// @param priority 任务优先级
void  NetEase_music_uri_play(const char* uri,UBaseType_t priority){
  //common_mp3_evt不为空说明还有音频事件运行中，进行等待再继续
  if (common_mp3_evt!=NULL){
   ESP_LOGE("NetEase_music_uri_play","播放繁忙中，无法准备新播放任务");
   return;
  }
  common_mp3_evt = audio_calloc(1, sizeof(int));//立即申请内存使得common_mp3_evt不为空来锁住其他任务
  sevetest30_music_running_flag = true;

  element_cfg_data_reset();
  audio_element_all_init();

  const char *link_tag[3] = {"http", "mp3", "i2s"};
  audio_pipeline_link(pipeline, &link_tag[0], 3);

  http_i2s_mp3_music_start(uri,&common_mp3_running_event,priority);
}




//以下参考了官方提供的 https://github.com/espressif/esp-adf/tree/master/examples/cloud_services/pipeline_baidu_speech_mp3 例程

// int _baiduTTS_get_token_handle(http_stream_event_msg_t *msg)
// {
//     esp_http_client_handle_t http_client = (esp_http_client_handle_t)msg->http_client;

//     if (msg->event_id != HTTP_STREAM_PRE_REQUEST) {
//         return ESP_OK;
//     }

//     if (baidu_access_token == NULL) {
//         baidu_access_token = baidu_get_access_token(CONFIG_BAIDU_ACCESS_KEY, CONFIG_BAIDU_SECRET_KEY);
//     }

//     if (baidu_access_token == NULL) {
//         ESP_LOGE(TAG, "获取token时发现问题");
//         return ESP_FAIL;
//     }

//     int data_len = snprintf(request_data,2048, "lan=%s&cuid=ESP32&ctp=1&per=1&vol=15&tok=%s&tex=%s",baidu_access_token,);
//     esp_http_client_set_post_field(http_client, request_data, data_len);
//     esp_http_client_set_method(http_client, HTTP_METHOD_POST);
//     return ESP_OK;
// }