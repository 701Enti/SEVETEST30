
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

// 该文件归属701Enti组织，主要由SEVETEST30开发团队维护，
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 在此，对 乐鑫科技-ESPRESSIF ESP-IDF ESP-ADF ESP-DSP各种框架及相关创作者们 表示感谢
// 在此，对 网易云音乐 网易云音乐API及相关创作者们 表示感谢
// 在此，对 zlib及相关创作者们 表示感谢
// 在此, 对 百度智能云千帆大模型平台 API服务及相关创作者们表示感谢
// 在此，对 和风天气及相关创作者们 表示感谢
// github: https://github.com/701Enti
// bilibili: 701Enti

#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/projdefs.h"

#include "esp_log.h"

#include "board_def.h"
#include "board_ctrl.h"
#include "fonts_chip.h"
#include "audio_hal.h"

#include "sevetest30_IWEDA.h"
#include "sevetest30_SWEDA.h"
#include "sevetest30_BWEDA.h"
#include "sevetest30_LedArray.h"
#include "sevetest30_UI.h"
#include "sevetest30_sound.h"
#include "sevetest30_touch.h"
#include "TCA6416A.h"

void fresh_LED()
{
}
void app_main(void)
{
  wifi_connect();

  TCA6416A_mode_t ext_io_mode_data = TCA6416A_DEFAULT_CONFIG_MODE;
  TCA6416A_value_t ext_io_value_data = TCA6416A_DEFAULT_CONFIG_VALUE;
  // ext_io_value_data.en_led_board = 1;//关闭灯板

  board_device_handle_t board_device_handle;
  board_ctrl_t board_ctrl = {
      .p_ext_io_mode = &ext_io_mode_data,   // 存储IO模式信息的结构体的地址
      .p_ext_io_value = &ext_io_value_data, // 存储IO电平信息的结构体的地址
      .boost_voltage = BV_VOL_MAX,
      .amplifier_volume = 90,
      .amplifier_mute = false,
      .codec_audio_hal_ctrl = AUDIO_HAL_CTRL_START,
      .codec_mode = AUDIO_HAL_CODEC_MODE_BOTH,
      .codec_adc_gain = MIC_GAIN_MAX,
      .codec_dac_pin = DAC_OUTPUT_ALL,
      .codec_dac_volume = 100,
      .codec_adc_pin = ADC_INPUT_LINPUT1_RINPUT1,
  };

  sevetest30_all_board_init(&board_ctrl, &board_device_handle);



  // for(;;){
  //  fonts_read_zh_CN_12x(&board_device_handle);
  //  vTaskDelay(pdMS_TO_TICKS(500));    
  // }
  

  init_time_data_sntp();

  // music_FFT_UI_cfg_t UI_cfg = {
  //     .fft_i2s_port = CODEC_DAC_I2S_PORT,
  //     .x = 1,
  //     .y = 1,
  //     .change = 3,
  //     .width = LINE_LED_NUMBER,
  //     .height = VERTICAL_LED_NUMBER,
  //     .visual_cfg = {
  //         .value_max = 128,
  //         .high = FFT_VIEW_DATA_MAX,
  //         .medium = (FFT_VIEW_DATA_MAX - FFT_VIEW_DATA_MIN) / 2,
  //         .low = FFT_VIEW_DATA_MIN,
  //         .public_divisor = (FFT_VIEW_DATA_MAX - FFT_VIEW_DATA_MIN) / 2,
  //         .x_multiples = 2,
  //         .x_move = 0.5,
  //     },
  // };

  esp_log_level_set("gpio", ESP_LOG_NONE);

  // bluetooth_connect();

  // uint8_t color[3] = {255,255,255};
  // separation_draw(1,1,12,fonts_read_zh_CN_12x(&board_device_handle),2*12,color,1);
  // for (int i = 0; i <= 5; i++)
  //   ledarray_set_and_write(i);

  // const char *url1 = "http://m701.music.126.net/20240219233219/36d3c0abc144f688e58879ebdca112ce/jdymusic/obj/wo3DlMOGwrbDjj7DisKw/31271000729/d21b/e9a7/ccf9/610fb06e731b320af25af6b5d8cb60b0.mp3";
  // // 检查资源可用性
  // if (http_check_common_url(url1) == ESP_OK)
  // {
  //   music_url_play(url1, 1);
  //   vTaskDelay(pdMS_TO_TICKS(500));

  //   music_FFT_UI_start(&UI_cfg, 1);

  //   uint8_t color[3] = {0};

  //   while (sevetest30_fft_ui_running_flag)
  //   {

  //     if (ext_io_ctrl.auto_read_INT == true)
  //     {
  //       ext_io_ctrl.auto_read_INT = false;
  //       ext_io_value_service();
  //     }

  //     music_FFT_UI_draw(&UI_cfg);
  //     main_UI_1();

  //     for (int i = 0; i <= 5; i++)
  //     {
  //       ledarray_set_and_write(i);
  //       progress_draw_buf(i * 2 + 1, 3.0, color);
  //       progress_draw_buf(i * 2 + 2, 3.0, color);
  //     }
  //   }
  // }

  //   for(;;){
  //   vibra_motor_start();
  //   vTaskDelay(pdMS_TO_TICKS(500));
  //   vibra_motor_stop();
  //   vTaskDelay(pdMS_TO_TICKS(500));
  //  }

  // baidu_ASR_cfg_t asr_cfg;
  // asr_cfg.dev_pid = ASR_PID_CM_NEAR;
  // asr_cfg.rate = ASR_RATE_16K;
  // asr_cfg.stop_threshold = 5;
  // asr_cfg.send_threshold = 3;
  // asr_cfg.record_save_times_max = 20;

  // while (1)
  // {
  //   asr_result_tex = NULL;
  //   asr_service_start(&asr_cfg, 1);
  //   music_FFT_UI_start(&UI_cfg, 1);
  //   while (!asr_result_tex)
  //   {
  //     for (int i = 0; i <= 5; i++)
  //     {
  //       ledarray_set_and_write(i);
  //       clean_draw_buf(i * 2 + 1);
  //       clean_draw_buf(i * 2 + 2);
  //     }
  //   }

  //   sevetest30_asr_running_flag = false;

  //   char *text1 = ERNIE_Bot_4_chat_tex_exchange(asr_result_tex);
  //   if (text1)
  //   {
  //     baidu_TTS_cfg_t tts_cfg = BAIDU_TTS_DEFAULT_CONFIG(text1, 1);
  //     tts_service_play(&tts_cfg, 1);
  //     music_FFT_UI_start(&UI_cfg, 1);
  //   }

  //   while (sevetest30_music_running_flag)
  //   {
  //     for (int i = 0; i <= 5; i++)
  //     {
  //       ledarray_set_and_write(i);
  //       clean_draw_buf(i * 2 + 1);
  //       clean_draw_buf(i * 2 + 2);
  //     }
  //   }
  //   vTaskDelay(pdMS_TO_TICKS(1000));
  // }


    sync
    while(1){
      BL5372_time_t time;
      BL5372_time_now_get(&time);  
      vTaskDelay(pdMS_TO_TICKS(1000));
    }


  //   for(;;){

  //     if (ext_io_ctrl.auto_read_INT == true)
  //     {
  //       ext_io_ctrl.auto_read_INT = false;
  //       ext_io_value_service();
  //     }
  //   uint8_t color[3]={255,255,255};
  //   refresh_systemtime_data();
  //   vTaskDelay(pdMS_TO_TICKS(100));
  //   main_UI_1();
  //    for (int i=0;i<=5;i++){
  //     ledarray_set_and_write(i);
  //     clean_draw_buf(i*2 + 0);
  //     clean_draw_buf(i*2 + 1);
  //    }


  // }

  // init_time_data_sntp();

  // refresh_position_data();
  // refresh_weather_data();

  // weather_UI_1(1, 1, 1);

  // for (int i = 0; i < 6; i++)
  // ledarray_set_and_write(i);
}
