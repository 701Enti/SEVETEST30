
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

 // 该文件归属701Enti组织，SEVETEST30开发团队应该提供责任性维护，
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

#include "esp_peripherals.h"

#include "board_def.h"
#include "board_ctrl.h"
#include "board_pins_config.h"
#include "gt32l32s0140.h"
#include "audio_hal.h"

#include "sevetest30_IWEDA.h"
#include "sevetest30_SWEDA.h"
#include "sevetest30_BWEDA.h"
#include "sevetest30_LedArray.h"
#include "sevetest30_UI.h"
#include "sevetest30_sound.h"
#include "sevetest30_touch.h"
#include "TCA6416A.h"

void app_main(void)
{
  TCA6416A_mode_t ext_io_mode_data = TCA6416A_DEFAULT_CONFIG_MODE;
  TCA6416A_value_t ext_io_value_data = TCA6416A_DEFAULT_CONFIG_VALUE;
  // ext_io_value_data.en_led_board = 1;//关闭灯板

  i2c_config_t device_i2c_config = {
    .mode = I2C_MODE_MASTER,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = DEVICE_I2C_DEFAULT_FREQ_HZ,
  };
  get_i2c_pins(DEVICE_I2C_PORT, &device_i2c_config);

  board_ctrl_t board_ctrl = {
      .p_i2c_device_config = &device_i2c_config,
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




  esp_periph_config_t wifi_periph_config = DEFAULT_ESP_PERIPH_SET_CONFIG();
  wifi_init(&wifi_periph_config);

  // 载入wifi信息
  periph_wifi_cfg_t wifi_cfg = { .disable_auto_reconnect = false,.wifi_config.sta.ssid = CONFIG_WIFI_SSID,.wifi_config.sta.password = CONFIG_WIFI_PASSWORD, };


  if (wifi_connect(&wifi_cfg) != ESP_OK)
    ESP_LOGE("MAIN", "网络连接失败");
  else
    ESP_LOGI("MAIN", "已连接到网络 - %s", wifi_cfg.wifi_config.sta.ssid);


  sevetest30_all_device_init(&board_ctrl);



  esp_log_level_set("gpio", ESP_LOG_NONE);


  // for (int q = 2; q < 6; q++) {
  //   //logo显示
  //   direct_draw(1, 3, sign_701, q);
  //   for (int i = 0; i < 6; i++)
  //     ledarray_set_and_write(i);
  // }
  // for (int q = 6; q > 1; q--) {
  //   //logo显示
  //   direct_draw(1, 3, sign_701, q);
  //   for (int i = 0; i < 6; i++)
  //     ledarray_set_and_write(i);
  // }





  // gpio_set_level(BAT_IN_CTRL_IO,0);//关机

  uint8_t color[3] = { 25,25,25 };
  font_roll_print_12x(0,0,color, 1, "hi,701Enti,美好皆于不懈尝试之中,热爱终在不断追逐之下,trying entire,trying all time!");  

	vTaskDelay(pdMS_TO_TICKS(2500));
  ledarray_set_refresh_mode(LEDARRAY_REFRESH_ALL_ONCE);
  font_roll_print_12x(0,0,color, 1, "hi,701Enti,美好皆于不懈尝试之中,热爱终在不断追逐之下,trying entire,trying all time!");    






  //初始化结束后

  // char lrc[5000] = {'\0'};
  // get_music_lyric_by_url("https://music.163.com/api/song/media?id=28892408",lrc,5000);
  // ESP_LOGE("ME", "%s",lrc);



    // music_FFT_UI_cfg_t UI_cfg = {
    // .x = 1,
    // .y = 1,
    // .change = 1,
    // .width = LINE_LED_NUMBER,
    // .height = VERTICAL_LED_NUMBER,
    // .visual_cfg = {
    //     .value_max = 128,
    //     .high = FFT_VIEW_DATA_MAX,
    //     .medium = (FFT_VIEW_DATA_MAX - FFT_VIEW_DATA_MIN) / 2,
    //     .low = FFT_VIEW_DATA_MIN,
    //     .public_divisor = (FFT_VIEW_DATA_MAX - FFT_VIEW_DATA_MIN) / 2,
    //     .x_multiples = 2,
    //     .x_move = 0.5,
    // },
    // };




    //lsm6ds3trc全部使用例子
    //FIFO
    // for(;;){
    //   refresh_IMU_FIFO_data(NULL,0,0);
    //   for(int i=0;i<3;i++){
    //     if((IMU_XLz_L[i] | IMU_XLz_H[i]<<8) < 0x7FF0)
    //     ESP_LOGI("ME","%d",(int16_t)(IMU_XLz_L[i] | IMU_XLz_H[i]<<8));  
    //   }
    //   vTaskDelay(pdMS_TO_TICKS(500));
    // }
    //数据
    // vTaskDelay(pdMS_TO_TICKS(50));
    // IMU_acceleration_value_t value = lsm6ds3trc_gat_now_acceleration();
    // IMU_angular_rate_value_t value = lsm6ds3trc_gat_now_angular_rate();
    // if (value.x != 0 || value.y != 0 || value.z != 0)
    //   ESP_LOGE("ME", "%d %d %d", value.x, value.y, value.z);
    //自动记录
    // IMU_D6D_data_value_t value = lsm6ds3trc_get_D6D_data_value();
    // ESP_LOGE("ME", "%d %d %d %d %d %d",value.XL,value.XH,value.YL,value.YH,value.ZL,value.ZH);
    // ESP_LOGE("ME", "%d",lsm6ds3trc_get_now_temperature());
    // if(lsm6ds3trc_get_free_fall_status())ESP_LOGE("ME", "自由落体");







// ///时间模块初始化
//   init_time_data_sntp();
//   vTaskDelay(pdMS_TO_TICKS(5000));
//   // sync_systemtime_to_ext_rtc();
//   // sync_systemtime_from_ext_rtc();


//   //实时时间显示
//   for (;;)
//   {
//     if (ext_io_ctrl.auto_read_INT == true)
//     {
//       ext_io_ctrl.auto_read_INT = false;
//       ext_io_value_service();
//     }
//     uint8_t color[3] = { 255, 255, 255 };
//     refresh_systemtime_data();
//     time_UI_1(1, 1, 1);
//     vTaskDelay(pdMS_TO_TICKS(100));
//     clean_draw();
//   }




  // bluetooth_connect();


  // for (int i = 0; i <= 5; i++)
  //   ledarray_set_and_write(i);


// //网络音乐播放
// const char* url1 = "http://m801.music.126.net/20240512231007/5cb8f90dc01396a4ee9e26c2324bc5f0/jdymusic/obj/wo3DlMOGwrbDjj7DisKw/31271000729/d21b/e9a7/ccf9/610fb06e731b320af25af6b5d8cb60b0.mp3";
// // 检查资源可用性
// if (http_check_common_url(url1) == ESP_OK)
// {
//   music_uri_play(url1, 1);
//   vTaskDelay(pdMS_TO_TICKS(500));
//   music_FFT_UI_start(&UI_cfg, 1);

//   uint8_t color[3] = { 0 };

//   while (sevetest30_music_running_flag)
//   {

//     mp3_duration_calculate();

//       if (ext_io_ctrl.auto_read_INT == true)
//       {
//         ext_io_ctrl.auto_read_INT = false;
//         ext_io_value_service();
//       }

//     music_FFT_UI_draw(&UI_cfg);
//     main_UI_1();

//     for (int i = 0; i <= 5; i++)
//     {
//       ledarray_set_and_write(i);
//       progress_draw_buf(i * 2 + 1, 1, color);
//       progress_draw_buf(i * 2 + 2, 1, color);
//     }
//   }
//   sevetest30_music_running_flag = false;

// }




//震动马达
    //   for(;;){
    //   vibra_motor_start();
    //   vTaskDelay(pdMS_TO_TICKS(500));
    //   vibra_motor_stop();
    //   vTaskDelay(pdMS_TO_TICKS(500));
    //  }


  // //AI交流例程
  //   uint8_t color[3] = { 0 };
  //   baidu_ASR_cfg_t asr_cfg;
  //   asr_cfg.dev_pid = ASR_PID_CM_NEAR;
  //   asr_cfg.rate = ASR_RATE_16K;
  //   asr_cfg.stop_threshold = 5;
  //   asr_cfg.send_threshold = 3;
  //   asr_cfg.record_save_times_max = 20;

  //   // 启动ASR服务
  //   asr_service_start(&asr_cfg, 1);

  //   vTaskDelay(pdMS_TO_TICKS(500));

  //   //运行FFT任务,全程监视
  //   music_FFT_UI_start(&UI_cfg, 1);

  //   char* text1 = NULL;

  //   while (1)
  //   {

  //     while (1)
  //     {
  //       music_FFT_UI_draw(&UI_cfg);
  //       //刷新屏幕
  //       for (int i = 0; i <= 5; i++)
  //       {
  //         ledarray_set_and_write(i);
  //         progress_draw_buf(i * 2 + 1, 1.0, color);
  //         progress_draw_buf(i * 2 + 2, 1.0, color);
  //       }

  //       //如果内存申请并且数据有效,退出
  //       if (sevetest30_asr_result_tex) {
  //         if (sevetest30_asr_result_tex[1] != 0) {
  //           //获取到样本,强制终止ASR服务
  //           sevetest30_asr_running_flag = false;
  //           break;
  //         }
  //       }
  //     }

  //     //发送文本内容给GPT
  //     text1 = ERNIE_Bot_4_chat_tex_exchange(sevetest30_asr_result_tex);
  //     //如果正常回复,TTS语音播放
  //     if (text1)
  //       if (text1[1] != 0)
  //       {
  //         baidu_TTS_cfg_t tts_cfg = BAIDU_TTS_DEFAULT_CONFIG(text1, 1);
  //         //启动TTS服务
  //         tts_service_play(&tts_cfg, 1);

  //         while (sevetest30_music_running_flag)
  //         {
  //           music_FFT_UI_draw(&UI_cfg);
  //           for (int i = 0; i <= 5; i++)
  //           {
  //             ledarray_set_and_write(i);
  //             progress_draw_buf(i * 2 + 1, 1.0, color);
  //             progress_draw_buf(i * 2 + 2, 1.0, color);
  //           }
  //         }
  //       }
  //     //重置缓存
  //     if (text1)memset(text1, 0, sizeof(ERNIE_BOT_4_CHAT_RESPONSE_BUF_MAX * sizeof(char)));

  //     memset(sevetest30_asr_result_tex, 0, sizeof(ASR_RESULT_TEX_BUF_MAX * sizeof(char)));
  //     asr_service_start(&asr_cfg, 1);
  //   }









    // init_time_data_sntp();

    // refresh_position_data();
    // refresh_weather_data();

    // weather_UI_1(1, 1, 1);

    // for (int i = 0; i < 6; i++)
    // ledarray_set_and_write(i);
}
