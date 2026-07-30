
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

// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// github: https://github.com/701Enti
// bilibili: 701Enti

#include <string.h>
#include <stdbool.h>

#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_peripherals.h"

#include "esp_wifi.h"
#include "lwip/dns.h"

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

// 临时测试包含
#include "hscdtd008a.h"
#include "calibration_tools.h"
#include "math_tools.h"

void app_main(void)
{

  vTaskDelay(pdMS_TO_TICKS(1000));

  i2c_config_t device_i2c_config = {
      .mode = I2C_MODE_MASTER,
      .sda_pullup_en = GPIO_PULLUP_DISABLE,
      .scl_pullup_en = GPIO_PULLUP_DISABLE,
      .master.clk_speed = DEVICE_I2C_DEFAULT_FREQ_HZ,
  };
  get_i2c_pins(DEVICE_I2C_PORT, &device_i2c_config);

  TCA6416A_mode_t ext_io_mode_data = TCA6416A_DEFAULT_CONFIG_MODE;
  TCA6416A_level_t ext_io_value_data = TCA6416A_DEFAULT_CONFIG_VALUE;
  board_ctrl_t board_ctrl = {
      .p_i2c_device_config = &device_i2c_config,
      .p_ext_io_mode = &ext_io_mode_data,   // 存储IO模式信息的结构体的地址
      .p_ext_io_value = &ext_io_value_data, // 存储IO电平信息的结构体的地址
      .amplifier_volume = 90,
      .amplifier_mute = true,
      .amplifier_sd = true,
      .codec_audio_hal_ctrl = AUDIO_HAL_CTRL_START,
      .codec_mode = AUDIO_HAL_CODEC_MODE_BOTH,
      .codec_adc_gain = MIC_GAIN_12DB,
      .codec_dac_pin = DAC_OUTPUT_ALL,
      .codec_dac_volume = 100,
      .codec_adc_pin = CODEC_ADC_INPUT_MIC_ON_BOARD,
  };

  sevetest30_all_device_init(&board_ctrl);

  esp_periph_config_t wifi_periph_config = DEFAULT_ESP_PERIPH_SET_CONFIG();
  wifi_init(&wifi_periph_config);
  // 载入wifi信息
  periph_wifi_cfg_t wifi_cfg = {
      .disable_auto_reconnect = false,
      .wifi_config.sta.ssid = CONFIG_WIFI_SSID,
      .wifi_config.sta.password = CONFIG_WIFI_PASSWORD,
  };
  if (wifi_connect(&wifi_cfg) != ESP_OK)
  {
    ESP_LOGE("MAIN", "网络连接失败");
  }
  else
  {
    ESP_LOGI("MAIN", "已连接到网络 - %s", wifi_cfg.wifi_config.sta.ssid);
    // 关闭wifi省电模式
    esp_wifi_set_ps(WIFI_PS_NONE);
    // 配置DNS服务器
    ip_addr_t dns_server;
    ipaddr_aton(CONFIG_DNS_SERVER, &dns_server);
    dns_setserver(0, &dns_server);
  }

  /// 时间模块初始化
  init_time_data_sntp(5000);
  // sync_systemtime_to_ext_rtc();
  // sync_systemtime_from_ext_rtc();

  // for (int q = 2; q < 6; q++) {
  //   //logo显示
  //   direct_draw(1, 3, sign_701, q);
  //   // for (int i = 0; i < 6; i++) //启动自动刷新服务后,无需手动刷新
  //   //   ledarray_set_and_write(i);
  // }
  // for (int q = 6; q > 1; q--) {
  //   //logo显示
  //   direct_draw(1, 3, sign_701, q);
  //   // for (int i = 0; i < 6; i++) //启动自动刷新服务后,无需手动刷新
  //   //   ledarray_set_and_write(i);
  // }

  // 打开屏幕显示
  board_ctrl_t *b = board_status_get();
  b->p_ext_io_value->EN_LED_BOARD = 0;
  sevetest30_board_ctrl(b, BOARD_CTRL_EXT_IO);

  // // 基础屏幕测试,仅测试能否显示变化矩形(启动自动刷新服务后,无需手动刷新)
  // uint8_t color[3] = {255,0,0};
  // while (1)
  // {
  //     vTaskDelay(pdMS_TO_TICKS(1000));
  //     for (int i = 0; i <= LINE_LED_NUMBER; i++)
  //     {
  //         clean_all_draw_buf();
  //         uint8_t *rp1 = rectangle(i, i);
  //         if (rp1 != NULL)
  //         {
  //             separation_draw(1, 1, i, RECTANGLE_MATRIX(rp1), matrix_size(rp1), color);
  //             free(rp1);
  //         }
  //         vTaskDelay(pdMS_TO_TICKS(500));
  //     }
  //     vTaskDelay(pdMS_TO_TICKS(1000));
  //     for (int i = LINE_LED_NUMBER; i >= 0; i--)
  //     {
  //         clean_all_draw_buf();
  //         uint8_t *rp1 = rectangle(i, i);
  //         if (rp1 != NULL)
  //         {
  //             separation_draw(1, 1, i, RECTANGLE_MATRIX(rp1), matrix_size(rp1), color);
  //             free(rp1);
  //         }
  //         vTaskDelay(pdMS_TO_TICKS(500));
  //     }
  // }

  // ///屏幕动画测试+字库测试
  // ///其他参数渲染与多关键帧支持待完善,隐写关键帧正在测试阶段
  // uint8_t color[3] = {255, 255, 0};
  // while (1)
  // {
  //     cartoon_handle_t cartoon1 = cartoon_new(CARTOON_RUN_MODE_PRE_RENDER, true, false, false, false, 10);
  //     if (cartoon1)
  //     {
  //         add_new_key_frame(cartoon1, KEY_FRAME_ATTR_LINEAR, CARTOON_KEY_FRAME_PCT_MAX * 0, false, 1, 1, color, 1);
  //         add_new_key_frame(cartoon1, KEY_FRAME_ATTR_LINEAR, (float)CARTOON_KEY_FRAME_PCT_MAX * 0.5, false, -100, 1, color, 1);
  //         uint32_t c1steg1 =
  //             add_new_key_frame(cartoon1, KEY_FRAME_ATTR_LINEAR, CARTOON_KEY_FRAME_PCT_MAX * 1, false, 1, 1, color, 1);
  //         add_new_key_frame(cartoon1, KEY_FRAME_ATTR_STEGANOGRAPHY, STEGANOGRAPHY_MODE_MAPPING_SUBTRACTION, c1steg1, (int32_t)&cartoon1->cartoon_plan.total_step_buf, NULL, NULL, NULL);

  //         font_roll_print_16x(1, 1, color, cartoon1, "hi,701Enti,美好皆于不懈尝试之中,热爱终在不断追逐之下,trying entire,trying all time!");

  //         cartoon_delete(cartoon1);
  //     }
  // }

  // // 实时时间显示
  // while (1)
  // {
  //   refresh_systemtime_data();
  //   if (xSemaphoreTake(refresh_Task_Mutex, portMAX_DELAY) == pdTRUE)
  //   {
  //     clean_all_draw_buf();
  //     time_UI_h_m_s(1, 1, 1);
  //     xSemaphoreGive(refresh_Task_Mutex);
  //   }
  //     vTaskDelay(pdMS_TO_TICKS(1000));
  // }

  // /// 歌词获取
  // char lrc[5000] = {'\0'};
  // get_music_lyric_by_url("https://music.163.com/api/song/media?id=28892408",lrc,5000);
  // ESP_LOGE("main", "%s",lrc);

  // // lsm6ds3trc全部使用例子
  // //  (输出XYZ分量总是一致，待优化)FIFO
  // while (1)
  // {
  //   refresh_IMU_FIFO_data(NULL, 0, 0);
  //   for (int i = 0; i < 3; i++)
  //   {
  //     if ((IMU_XLx_L[i] | IMU_XLx_H[i] << 8) < 0x7FF0){
  //       ESP_LOGI("main", "X轴加速度:%d", (int16_t)(IMU_XLx_L[i] | IMU_XLx_H[i] << 8));
  //     }
  //     else{
  //       ESP_LOGI("main", "X轴加速度:---");
  //     }

  //     if ((IMU_XLy_L[i] | IMU_XLy_H[i] << 8) < 0x7FF0){
  //       ESP_LOGI("main", "Y轴加速度:%d", (int16_t)(IMU_XLy_L[i] | IMU_XLy_H[i] << 8));
  //     }
  //     else{
  //       ESP_LOGI("main", "Y轴加速度:---");
  //     }

  //     if ((IMU_XLz_L[i] | IMU_XLz_H[i] << 8) < 0x7FF0){
  //       ESP_LOGI("main", "Z轴加速度:%d", (int16_t)(IMU_XLz_L[i] | IMU_XLz_H[i] << 8));
  //     }
  //     else{
  //       ESP_LOGI("main", "Z轴加速度:---");
  //     }
  //   }
  //   vTaskDelay(pdMS_TO_TICKS(500));
  // }
  // // 姿态数据
  // while (1)
  // {
  //   vTaskDelay(pdMS_TO_TICKS(500));
  //   ESP_LOGI("main", "---------------------------");

  //   IMU_acceleration_value_t acceleration = lsm6ds3trc_gat_now_acceleration();
  //   ESP_LOGI("main", "加速度 X:%d Y:%d Z:%d", acceleration.x, acceleration.y, acceleration.z);

  //   IMU_angular_rate_value_t angular_rate = lsm6ds3trc_gat_now_angular_rate();
  //   ESP_LOGI("main", "角速度 X:%d Y:%d Z:%d", angular_rate.x, angular_rate.y, angular_rate.z);
  // }
  // // (偏移标志位不会自动设置为0，待优化)自动记录
  // while (1)
  // {
  //   vTaskDelay(pdMS_TO_TICKS(1000));
  //   IMU_D6D_data_value_t value = lsm6ds3trc_get_D6D_data_value(true);
  //   ESP_LOGI("main", "D6D反向偏移标识 [%d<-X轴->%d] [%d<-Y轴->%d] [%d<-Z轴->%d]", value.XL, value.XH, value.YL, value.YH, value.ZL, value.ZH);
  //   ESP_LOGI("main", "温度 %.3f ℃", (double)lsm6ds3trc_get_now_temperature() / 1000);
  //   if (lsm6ds3trc_get_free_fall_status())
  //     ESP_LOGW("main", "自由落体");
  // }

  // //(经常出现校准失败，待优化) hscdtd008a

  // hscdtd008a_mode_set(GS_MODE_ACTIVE);
  // hscdtd008a_state_set(GS_STATE_NORMAL);

  // ESP_LOGI("main", "5s后开始校准");
  // vTaskDelay(pdMS_TO_TICKS(5000));

  // GS_calibration_static_model_t static_model;
  // esp_err_t ret = generate_GS_calibration_static_model(&static_model, 200, 100);

  // if (ret == ESP_OK) {
  //   GS_output_data_t output;
  //   GS_magnetic_flux_density_data_t mfd;
  //   GS_angle_data_t angle;
  //   while (1)
  //   {
  //     hscdtd008a_output_data_get(&output);
  //     to_magnetic_flux_density_data(&output, &mfd);
  //     calculate_calibrated_GS_only_by_static_model(&static_model, &mfd);
  //     to_angle_data(GS_UNIT_OF_ANGLE_DEGREES, &mfd, &angle);
  //     ESP_LOGI("main", "方位---[%f]--- 俯仰|%f|", angle.azimuth, angle.pitch);
  //     ESP_LOGI("main", "x-[%f] y-[%f] z-[%f]", mfd.Bx, mfd.By, mfd.Bz);
  //     vTaskDelay(pdMS_TO_TICKS(1000));
  //   }
  // }

  // bluetooth_connect();

  music_FFT_UI_cfg_t FFT_UI_cfg = {
      .x = 1,
      .y = 1,
      .change = 1,
      .lr_switch = 0,
      .color_visual_cfg = {
          .value_max = 255,
          .high = 4096,
          .medium = (4096 - 0) / 2,
          .low = 0,
          .public_divisor = (4096 - 0) / 2,
      },
      .dampen_multiples = 20,
      .data_max = 4096,
      .data_min = -4096,
      .x_multiples = 5,
      .x_move = 2.8,
      .width = LINE_LED_NUMBER,
      .show_height_max = VERTICAL_LED_NUMBER,
  };

  // // 网络音乐播放
  // // 官方测试音频 "https://dl.espressif.cn/dl/audio/ff-16b-2c-44100hz.mp3";
  // char *url1 = "https://dl.espressif.cn/dl/audio/ff-16b-2c-44100hz.mp3";
  // change_url_if_need_redirect(&url1);

  // // 检查资源可用性
  // if (http_check_common_url(url1) == ESP_OK)
  // {

  //   board_ctrl_t *b = board_status_get();
  //   b->amplifier_mute = false;
  //   b->amplifier_sd = true;
  //   b->amplifier_volume = 80;
  //   sevetest30_board_ctrl(b, BOARD_CTRL_AMPLIFIER);

  //   music_uri_or_url_play(url1, 1);

  //   music_FFT_UI_handle_t handle = music_FFT_UI_start(&FFT_UI_cfg, 1);
  //   if (!handle)
  //   {
  //       ESP_LOGE("main", "handle 为空,无法绘制任务");
  //       return;
  //   }

  //   while (1)
  //   {
  //     if (handle && xSemaphoreTake(refresh_Task_Mutex, pdMS_TO_TICKS(10)) == pdTRUE)
  //     {
  //       clean_all_draw_buf();
  //       music_FFT_UI_draw(handle);
  //       xSemaphoreGive(refresh_Task_Mutex);
  //     }
  //     vTaskDelay(pdMS_TO_TICKS(10));
  //     if(!sevetest30_music_running_flag){
  //       music_FFT_UI_stop(handle);
  //     }
  //   }
  // }

  // while (1)
  // {
  //   if (ext_io_ctrl.auto_read_INT)
  //   {
  //     if (ext_io_level_service() == ESP_OK)
  //     {
  //       ESP_LOGW("main", "扩展GPIO自动读取中断触发成功");
  //       ext_io_ctrl.auto_read_INT = false;
  //     }
  //   }
  //   vTaskDelay(pdMS_TO_TICKS(100));
  // }

  // 震动马达
  //    for(;;){
  //    vibra_motor_start();
  //    vTaskDelay(pdMS_TO_TICKS(500));
  //    vibra_motor_stop();
  //    vTaskDelay(pdMS_TO_TICKS(500));
  //   }

  // // 获取当前连接wifi的信号强度(RSSI)
  // wifi_ap_record_t ap_info;
  // if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
  // {
  //   ESP_LOGW("main", "当前连接wifi的信号强度(RSSI): %d", ap_info.rssi);
  // }

  // // TTS在线文字转语音
  // board_ctrl_t *board_tts = board_status_get();
  // board_tts->amplifier_mute = false;
  // board_tts->amplifier_sd = true;
  // board_tts->amplifier_volume = 80;
  // sevetest30_board_ctrl(board_tts, BOARD_CTRL_AMPLIFIER);

  // TTS_cfg_t tts_cfg = TTS_DEFAULT_CONFIG("你好，我是SEVETEST30，你可以和我聊天", 0);
  // tts_service_play(&tts_cfg, 1); // 启动TTS服务

  // music_FFT_UI_handle_t handle = music_FFT_UI_start(&FFT_UI_cfg, 1);
  // if (!handle)
  // {
  //   ESP_LOGE("main", "handle 为空,无法启动绘制任务");
  // }

  // while (1)
  // {
  //   if (handle && xSemaphoreTake(refresh_Task_Mutex, pdMS_TO_TICKS(10)) == pdTRUE)
  //   {
  //     clean_all_draw_buf();
  //     music_FFT_UI_draw(handle);
  //     xSemaphoreGive(refresh_Task_Mutex);
  //   }
  //   vTaskDelay(pdMS_TO_TICKS(10));
  //   if (!sevetest30_music_running_flag)
  //   {
  //     music_FFT_UI_stop(handle);
  //   }
  // }

  // // AI交流例程
  // ASR_cfg_t asr_cfg;
  // asr_cfg.dev_pid = ASR_PID_CM_NEAR;
  // asr_cfg.sampling_rate = ASR_SAMPLING_RATE_16K;
  // asr_cfg.sampling_bits = ASR_SAMPLING_BITS_16;
  // asr_cfg.input_rate = 48000;
  // asr_cfg.input_bits = 16;
  // asr_cfg.asr_one_frame_ms = 1000;
  // asr_cfg.stop_threshold = 1;
  // asr_cfg.send_threshold = 1;
  // asr_cfg.record_save_times_max = 20;
  // asr_cfg.vad_mode = VAD_MODE_3;
  // asr_cfg.vad_one_frame_ms = 30;
  // asr_cfg.vad_min_speech_ms = 500;
  // asr_cfg.vad_min_noise_ms = 50;

  // asr_service_begin(&asr_cfg, 1);

  // music_FFT_UI_handle_t handle = music_FFT_UI_start(&FFT_UI_cfg, 1);
  // if (!handle)
  // {
  //   ESP_LOGE("main", "FFT handle 为空,无法绘制任务");
  //   return;
  // }

  // GPT_chat_handle_t GPT_chat_handle = GPT_chat_start(ERNIE_BOT_URL, CONFIG_GPT_CHAT_ACCESS_KEY, "ernie-4.5-turbo-128k", "", 60000);
  // if (GPT_chat_handle == NULL)
  // {
  //   ESP_LOGE("main", "GPT_chat_handle 为空,无法绘制任务");
  //   return;
  // }

  // while (1)
  // {
  //   while (1)
  //   {
  //     vTaskDelay(pdMS_TO_TICKS(100));
  //     if (handle && xSemaphoreTake(refresh_Task_Mutex, pdMS_TO_TICKS(10)) == pdTRUE)
  //     {
  //       clean_all_draw_buf();
  //       music_FFT_UI_draw(handle);
  //       xSemaphoreGive(refresh_Task_Mutex);
  //     }
  //     // 如果内存申请并且数据有效,退出
  //     if (sevetest30_asr_result_text)
  //     {
  //       if (sevetest30_asr_result_text[1] != 0)
  //       {
  //         // 获取到样本,强制终止ASR服务
  //         sevetest30_asr_running_flag = false;
  //         break;
  //       }
  //     }
  //   }

  //   // 发送文本内容给GPT
  //   if (GPT_chat_update_user_content(GPT_chat_handle, sevetest30_asr_result_text) != ESP_OK)
  //   {
  //     GPT_chat_stop(GPT_chat_handle);
  //     ESP_LOGE("main", "GPT_chat_update_user_content 失败");
  //     return;
  //   }
  //   if (GPT_chat_text_exchange(GPT_chat_handle, 1) != ESP_OK)
  //   {
  //     GPT_chat_stop(GPT_chat_handle);
  //     ESP_LOGE("main", "GPT_chat_text_exchange 失败");
  //     return;
  //   }

  //   // 如果正常回复,TTS语音播放
  //   if (GPT_chat_handle->result)
  //   {
  //     if (GPT_chat_handle->result[1] != 0)
  //     {
  //       board_ctrl_t *b = board_status_get();
  //       b->amplifier_mute = false;
  //       b->amplifier_sd = true;
  //       b->amplifier_volume = 80;
  //       sevetest30_board_ctrl(b, BOARD_CTRL_AMPLIFIER);

  //       TTS_cfg_t tts_cfg = TTS_DEFAULT_CONFIG(GPT_chat_handle->result, 4189);
  //       // 启动TTS服务
  //       tts_service_play(&tts_cfg, 1);

  //       while (sevetest30_music_running_flag)
  //       {
  //         vTaskDelay(pdMS_TO_TICKS(100));
  //         if (handle && xSemaphoreTake(refresh_Task_Mutex, pdMS_TO_TICKS(10)) == pdTRUE)
  //         {
  //           clean_all_draw_buf();
  //           music_FFT_UI_draw(handle);
  //           xSemaphoreGive(refresh_Task_Mutex);
  //         }
  //       }
  //     }
  //   }

  //   memset(sevetest30_asr_result_text, 0, sizeof(ASR_RESULT_TEX_BUF_MAX * sizeof(char)));
  //   asr_service_begin(&asr_cfg, 1);
  // }

  // refresh_position_data();
  // refresh_weather_data();

  // weather_UI_1(1, 1, 1);

  // for (int i = 0; i < 6; i++)
  // ledarray_set_and_write(i);
}
