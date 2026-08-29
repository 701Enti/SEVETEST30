
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

// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// github: https://github.com/701Enti

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_peripherals.h"

#include "esp_sleep.h"
#include "esp_wifi.h"
#include "freertos/idf_additions.h"
#include "lwip/dns.h"

#include "OPT3001.h"
#include "audio_hal.h"
#include "board_ctrl.h"
#include "board_def.h"
#include "board_pins_config.h"
#include "gt32l32s0140.h"

#include "HSCDTD008A.h"
#include "TCA6416A.h"
#include "calibration_tools.h"
#include "esp_timer.h"
#include "math_tools.h"
#include "sevetest30_BWEDA.h"
#include "sevetest30_IWEDA.h"
#include "sevetest30_LedArray.h"
#include "sevetest30_SWEDA.h"
#include "sevetest30_UI.h"
#include "sevetest30_config.h"
#include "sevetest30_gpio.h"
#include "sevetest30_sound.h"
#include "sevetest30_touch.h"

board_ctrl_t board_ctrl = {0};

xSemaphoreHandle update_ui_data_mutex = NULL;

char *system_json_head_prompt =
    "你是一个部署在SEVETEST30智能闹钟上的AI语音助手,"
    "你的名字是:07(零七,中文谐音寓意trying,数字取自701Enti),"
    "SEVETEST30是github上的701Enti组织归属的非盈利开源项目."
    "你可以和我聊天,尽量用简短文字回答问题,文学创作除外."
    "以下信息来自我的SEVETEST30闹钟通过网络API或硬件传感器获取的数据."
    "实时更新,我们的聊天可以不涉及."
    "注意:你需要根据时区获取当前时间,这里不会提供."
    "注意:位置数据来自IP定位,不要引起用户不必要的恐慌,经纬度数据可能不准确."
    "注意:用户通过语音识别聊天,文字书写可能错误,请重点关注拼音和上下文,"
    "忽略多音字歧义.";

void init(void);
void test(void);

void AI_chat(void);

/// @brief
/// 立即执行关机,全局设备通过断电/失能/低功耗模式/深度睡眠使得设备进入软关机状态(硬件不支持除外)
void sevetest30_shutdown(void) {
  ESP_LOGW("main", "关机...");
  vibra_motor_start();
  vTaskDelay(pdMS_TO_TICKS(3000));
  sevetest30_all_device_deep_sleep();
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_1, 0);
  esp_deep_sleep_start();
}

void app_main(void) {

  init();

  refresh_battery_data();
  refresh_env_temp_hum_data();
  refresh_env_TVOC_data(true);

  refresh_position_data();
  refresh_current_weather_data();

  if (xSemaphoreTake(update_ui_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    clean_all_draw_buf();
    xSemaphoreGive(update_ui_data_mutex);
  }

  test();

  int UI_switch = 0;
  bool UI_changed = false;

  while (1) {

    vTaskDelay(pdMS_TO_TICKS(10));

    refresh_systemtime_data();

    if (ext_io_ctrl.auto_read_INT) {
      if (ext_io_level_service() == ESP_OK) {
        ext_io_ctrl.auto_read_INT = false;

        if (board_ctrl.p_ext_io_value->thumbwheel_CW == 0 &&
            board_ctrl.p_ext_io_value->thumbwheel_CCW == 1) {
          if (board_ctrl.p_ext_io_value->DISABLE_LED_BOARD) {
            continue;
          }
          vibra_motor_start();
          vTaskDelay(pdMS_TO_TICKS(50));
          vibra_motor_stop();
          UI_switch++;
          UI_changed = true;
        } else if (board_ctrl.p_ext_io_value->thumbwheel_CW == 1 &&
                   board_ctrl.p_ext_io_value->thumbwheel_CCW == 0) {
          if (board_ctrl.p_ext_io_value->DISABLE_LED_BOARD) {
            continue;
          }
          vibra_motor_start();
          vTaskDelay(pdMS_TO_TICKS(50));
          vibra_motor_stop();
          UI_switch--;
          UI_changed = true;
        } else if (board_ctrl.p_ext_io_value->thumbwheel_CW == 0 &&
                   board_ctrl.p_ext_io_value->thumbwheel_CCW == 0) {

          vTaskDelay(pdMS_TO_TICKS(200));
          ext_io_level_service();

          if (!(board_ctrl.p_ext_io_value->thumbwheel_CW == 0 &&
                board_ctrl.p_ext_io_value->thumbwheel_CCW == 0)) {
            board_ctrl.p_ext_io_value->DISABLE_LED_BOARD =
                !(board_ctrl.p_ext_io_value->DISABLE_LED_BOARD);
            sevetest30_board_ctrl(&board_ctrl, BOARD_CTRL_EXT_IO);
            vibra_motor_start();
            vTaskDelay(pdMS_TO_TICKS(100));
            vibra_motor_stop();
            continue;
          }

          if (board_ctrl.p_ext_io_value->DISABLE_LED_BOARD) {
            continue;
          }

          if (xSemaphoreTake(update_ui_data_mutex, pdMS_TO_TICKS(100)) ==
              pdTRUE) {
            clean_all_draw_buf();
            facial_expression_show(1, 1, "normal");
            xSemaphoreGive(update_ui_data_mutex);
          }

          vibra_motor_start();
          vTaskDelay(pdMS_TO_TICKS(50));
          vibra_motor_stop();
          vTaskDelay(pdMS_TO_TICKS(100));
          vibra_motor_start();
          vTaskDelay(pdMS_TO_TICKS(50));
          vibra_motor_stop();

          int64_t start_time = esp_timer_get_time();
          while (board_ctrl.p_ext_io_value->thumbwheel_CW == 0 &&
                 board_ctrl.p_ext_io_value->thumbwheel_CCW == 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            ext_io_level_service();
            if (esp_timer_get_time() - start_time > 5 * 1000 * 1000) {
              sevetest30_shutdown();
            }
          }

          AI_chat();
          UI_changed = true;
        }
      }
    }
    if (xSemaphoreTake(update_ui_data_mutex, 100) == pdTRUE) {
      switch (UI_switch) {
      case 0:
        clean_all_draw_buf();
        time_UI_h_m_s(1, 1);
        break;
      case 1:
        clean_all_draw_buf();
        weather_icon_temperature(1, 1);
        break;
      case 2:
        clean_all_draw_buf();
        if (UI_changed) {
          refresh_battery_data();
        }
        battery_UI(1, 1);
        if (board_ctrl.p_ext_io_value->charge_SIGN == 0) {
          static int i = 0;
          static uint8_t rectangle_data[VERTICAL_LED_NUMBER * sizeof(uint8_t) +
                                        sizeof(uint32_t)];
          uint8_t color[3] = {255, 255, 255};

          build_rectangle(1, i / 100.0f * (int)(VERTICAL_LED_NUMBER / 2),
                          rectangle_data, sizeof(rectangle_data));
          separation_draw(LINE_LED_NUMBER, (VERTICAL_LED_NUMBER / 2), 1,
                          RECTANGLE_MATRIX(rectangle_data),
                          matrix_size(rectangle_data), color);
          i++;
          if (i > 100) {
            i = 0;
          }
        }
        break;
      case 3:
        clean_all_draw_buf();
        time_UI_h_m(1, 1);
        break;
      case 4:
        clean_all_draw_buf();
        time_UI_s(1, 1);
        break;
      default:
        UI_switch = 0;
        break;
      }
      xSemaphoreGive(update_ui_data_mutex);

      if (UI_changed) {
        UI_changed = false;
      }
    }
  }

  return;
}

void init(void) {
  const char *TAG = "init";

  static TCA6416A_mode_t ext_io_mode_data = TCA6416A_DEFAULT_CONFIG_MODE;
  static TCA6416A_level_t ext_io_value_data = TCA6416A_DEFAULT_CONFIG_VALUE;

  board_ctrl.p_ext_io_mode = &ext_io_mode_data;
  board_ctrl.p_ext_io_value = &ext_io_value_data;
  board_ctrl.amplifier_volume = 60;
  board_ctrl.amplifier_mute = true;
  board_ctrl.amplifier_sd = false;
  board_ctrl.codec_audio_hal_ctrl = AUDIO_HAL_CTRL_START;
  board_ctrl.codec_mode = AUDIO_HAL_CODEC_MODE_BOTH;
  board_ctrl.codec_adc_gain = MIC_GAIN_9DB;
  board_ctrl.codec_dac_pin = DAC_OUTPUT_ALL;
  board_ctrl.codec_dac_volume = 100;
  board_ctrl.codec_adc_pin = CODEC_ADC_INPUT_MIC_ON_BOARD;

  sevetest30_all_device_init(&board_ctrl);

  update_ui_data_mutex = xSemaphoreCreateMutex();
  if (update_ui_data_mutex == NULL) {
    ESP_LOGE(TAG, "update_ui_data_mutex创建失败");
    return;
  }

  // 打开屏幕显示
  board_ctrl_t *b = board_status_get();
  b->p_ext_io_value->DISABLE_LED_BOARD = 0;
  sevetest30_board_ctrl(b, BOARD_CTRL_EXT_IO);

  ext_io_ctrl.auto_read_EN = true;

  show_701Enti_sign(1, 1);

  esp_periph_config_t wifi_periph_config = DEFAULT_ESP_PERIPH_SET_CONFIG();
  wifi_init(&wifi_periph_config);
  // 载入wifi信息
  periph_wifi_cfg_t wifi_cfg = {
      .disable_auto_reconnect = false,
      .wifi_config.sta.ssid = CONFIG_WIFI_SSID,
      .wifi_config.sta.password = CONFIG_WIFI_PASSWORD,
  };
  if (wifi_connect(&wifi_cfg) != ESP_OK) {
    ESP_LOGE(TAG, "网络连接失败");
  } else {
    ESP_LOGI(TAG, "已连接到网络 - %s", wifi_cfg.wifi_config.sta.ssid);
    // // 关闭wifi省电模式
    // esp_wifi_set_ps(WIFI_PS_NONE);
    // 配置DNS服务器
    ip_addr_t dns_server;
    ipaddr_aton(CONFIG_DNS_SERVER, &dns_server);
    dns_setserver(0, &dns_server);
  }

  init_timezone();
  init_time_data_sntp(20000);

  show_se30_sign(1, 1);
}

void test(void) {
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
  //             separation_draw(1, 1, i, RECTANGLE_MATRIX(rp1),
  //             matrix_size(rp1), color); free(rp1);
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
  //             separation_draw(1, 1, i, RECTANGLE_MATRIX(rp1),
  //             matrix_size(rp1), color); free(rp1);
  //         }
  //         vTaskDelay(pdMS_TO_TICKS(500));
  //     }
  // }

  // /// 屏幕动画测试+字库测试+UI库动画API测试
  // /// 其他参数渲染与多关键帧支持待完善,隐写关键帧正在测试阶段
  // uint8_t color[3] = {255, 255, 0};
  // while (1)
  // {
  //   cartoon_handle_t cartoon1 = cartoon_new(CARTOON_RUN_MODE_PRE_RENDER,
  //   true, false, false, 10); if (cartoon1)
  //   {
  //     add_new_key_frame(cartoon1, KEY_FRAME_ATTR_LINEAR,
  //     CARTOON_KEY_FRAME_PCT_MAX * 0, false, 1, 1, color);
  //     add_new_key_frame(cartoon1, KEY_FRAME_ATTR_LINEAR,
  //     (float)CARTOON_KEY_FRAME_PCT_MAX * 0.5, false, -100, 1, color);
  //     uint32_t c1steg1 =
  //         add_new_key_frame(cartoon1, KEY_FRAME_ATTR_LINEAR,
  //         CARTOON_KEY_FRAME_PCT_MAX * 1, false, 1, 1, color);
  //     add_new_key_frame(cartoon1, KEY_FRAME_ATTR_STEGANOGRAPHY,
  //     STEGANOGRAPHY_MODE_MAPPING_SUBTRACTION, c1steg1,
  //     (int32_t)&cartoon1->cartoon_plan.total_step_buf, NULL, NULL);

  //     font_roll_print_16x(1, 1, color, cartoon1,
  //     "hi,701Enti,美好皆于不懈尝试之中,热爱终在不断追逐之下,trying
  //     entire,trying all time!");

  //     cartoon_delete(cartoon1);
  //   }
  // }

  // // 实时时间显示
  // while (1)
  // {
  //   refresh_systemtime_data();
  //   if (xSemaphoreTake(update_ui_data_mutex, pdMS_TO_TICKS(100)) ==
  //   pdTRUE)
  //   {
  //     clean_all_draw_buf();
  //     time_UI_h_m_s(1, 1);
  //     xSemaphoreGive(update_ui_data_mutex);
  //   }
  //   vTaskDelay(pdMS_TO_TICKS(1000));
  // }

  // /// 歌词获取
  // char lrc[5000] = {0};
  // fetch_music_lyric_by_url("",lrc,5000);
  // ESP_LOGE("main", "%s",lrc);

  // // LSM6DS3TRC全部使用例子
  // //  (输出XYZ分量总是一致，待优化)FIFO
  // while (1)
  // {
  //   refresh_IMU_FIFO_data(NULL, 0, 0);
  //   for (int i = 0; i < 3; i++)
  //   {
  //     if ((IMU_XLx_L[i] | IMU_XLx_H[i] << 8) < 0x7FF0){
  //       ESP_LOGI("main", "X轴加速度:%d", (int16_t)(IMU_XLx_L[i] |
  //       IMU_XLx_H[i] << 8));
  //     }
  //     else{
  //       ESP_LOGI("main", "X轴加速度:---");
  //     }

  //     if ((IMU_XLy_L[i] | IMU_XLy_H[i] << 8) < 0x7FF0){
  //       ESP_LOGI("main", "Y轴加速度:%d", (int16_t)(IMU_XLy_L[i] |
  //       IMU_XLy_H[i] << 8));
  //     }
  //     else{
  //       ESP_LOGI("main", "Y轴加速度:---");
  //     }

  //     if ((IMU_XLz_L[i] | IMU_XLz_H[i] << 8) < 0x7FF0){
  //       ESP_LOGI("main", "Z轴加速度:%d", (int16_t)(IMU_XLz_L[i] |
  //       IMU_XLz_H[i] << 8));
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

  //   IMU_acceleration_value_t acceleration =
  //   LSM6DS3TRC_gat_now_acceleration(); ESP_LOGI("main", "加速度 X:%d Y:%d
  //   Z:%d", acceleration.x, acceleration.y, acceleration.z);

  //   IMU_angular_rate_value_t angular_rate =
  //   LSM6DS3TRC_gat_now_angular_rate(); ESP_LOGI("main", "角速度 X:%d Y:%d
  //   Z:%d", angular_rate.x, angular_rate.y, angular_rate.z);
  // }
  // // (偏移标志位不会自动设置为0，待优化)自动记录
  // while (1)
  // {
  //   vTaskDelay(pdMS_TO_TICKS(1000));
  //   IMU_D6D_data_value_t value = LSM6DS3TRC_get_D6D_data_value(true);
  //   ESP_LOGI("main", "D6D反向偏移标识 [%d<-X轴->%d] [%d<-Y轴->%d]
  //   [%d<-Z轴->%d]", value.XL, value.XH, value.YL, value.YH, value.ZL,
  //   value.ZH); ESP_LOGI("main", "温度 %.3f ℃",
  //   (double)LSM6DS3TRC_get_now_temperature() / 1000); if
  //   (LSM6DS3TRC_get_free_fall_status())
  //     ESP_LOGW("main", "自由落体");
  // }

  // //(经常出现校准失败，待优化) HSCDTD008A

  // HSCDTD008A_mode_set(GS_MODE_ACTIVE);
  // HSCDTD008A_state_set(GS_STATE_NORMAL);

  // ESP_LOGI("main", "5s后开始校准");
  // vTaskDelay(pdMS_TO_TICKS(5000));

  // GS_calibration_static_model_t static_model;
  // esp_err_t ret = generate_GS_calibration_static_model(&static_model, 200,
  // 100);

  // if (ret == ESP_OK) {
  //   GS_output_data_t output;
  //   GS_magnetic_flux_density_data_t mfd;
  //   GS_angle_data_t angle;
  //   while (1)
  //   {
  //     HSCDTD008A_output_data_get(&output);
  //     to_magnetic_flux_density_data(&output, &mfd);
  //     calculate_calibrated_GS_only_by_static_model(&static_model, &mfd);
  //     to_angle_data(GS_UNIT_OF_ANGLE_DEGREES, &mfd, &angle);
  //     ESP_LOGI("main", "方位---[%f]--- 俯仰|%f|", angle.azimuth,
  //     angle.pitch); ESP_LOGI("main", "x-[%f] y-[%f] z-[%f]", mfd.Bx, mfd.By,
  //     mfd.Bz); vTaskDelay(pdMS_TO_TICKS(1000));
  //   }
  // }

  // // 通过AHT21获取环境温度和湿度数据
  // refresh_env_temp_hum_data(true);
  // if (env_temp_hum_data.valid)
  // {
  //   ESP_LOGI("main", "环境温度:%.2f ℃, 环境湿度:%.2f%%",
  //   env_temp_hum_data.temp, env_temp_hum_data.hum);
  // }
  // else
  // {
  //   ESP_LOGE("main", "环境温度或湿度数据无效");
  // }

  // // 通过AGS10获取环境TVOC数据
  // ESP_LOGW("main", "等待预热中(每次完全掉电后上电需要预热,预计两分钟)...");
  // vTaskDelay(pdMS_TO_TICKS(120000));
  // while (1)
  // {
  //   vTaskDelay(pdMS_TO_TICKS(1000));
  //   refresh_env_TVOC_data(true);
  //   if (env_TVOC_data.valid)
  //   {
  //     ESP_LOGI("main", "环境TVOC:%" PRIu32 "ppb", env_TVOC_data.TVOC_value);
  //   }
  //   else
  //   {
  //     ESP_LOGE("main", "环境TVOC数据无效");
  //   }
  // }

  // // 通过OPT3001获取环境光照数据
  // while (1) {
  //   vTaskDelay(pdMS_TO_TICKS(1000));
  //   refresh_env_lux_data();
  //   if (env_lux_data >= 0.0f) {
  //     ESP_LOGI("main", "环境光照:%.2f lx", env_lux_data);
  //   } else {
  //     ESP_LOGE("main", "环境光照数据无效");
  //   }
  // }

  // // 通过MAX17048获取电池数据
  // while (1) {
  //   vTaskDelay(pdMS_TO_TICKS(1000));
  //   refresh_battery_data();
  //   if (battery_data.charge_flag) {
  //     ESP_LOGI("main", "电池充电中");
  //   } else {
  //     ESP_LOGI("main", "电池不处于充电状态");
  //   }
  //   if (battery_data.result.battery_voltage >= 0.0f) {
  //     ESP_LOGI("main", "电池电压:%.2fmV",
  //     battery_data.result.battery_voltage);
  //   } else {
  //     ESP_LOGE("main", "电池数据无效");
  //   }
  //   if (battery_data.result.battery_soc >= 0.0f) {
  //     ESP_LOGI("main", "电池SOC:%.2f%%", battery_data.result.battery_soc);
  //   } else {
  //     ESP_LOGE("main", "电池数据无效");
  //   }
  // }

  // bluetooth_connect();

  // music_FFT_UI_cfg_t FFT_UI_cfg = FFT_UI_DEFAULT_CONFIG();

  // // 网络音乐播放
  // // // espressif官方测试音频
  // // "https://dl.espressif.cn/dl/audio/ff-16b-2c-44100hz.mp3";
  // char *url1 ="https://dl.espressif.cn/dl/audio/ff-16b-2c-44100hz.mp3";

  // IWEDA_handle_t music_play_IWEDA_handle = new_iweda_handle(2048, 2048);
  // if (!music_play_IWEDA_handle) {
  //   ESP_LOGE("main", "music_play_IWEDA_handle 为空,无法绘制任务");
  //   return;
  // }

  // int url_len = snprintf(music_play_IWEDA_handle->url_buf,
  //                        music_play_IWEDA_handle->url_buf_size, "%s", url1);
  // if (url_len >= music_play_IWEDA_handle->url_buf_size) {
  //   ESP_LOGE("main", "url_len 超出 url_buf_size");
  //   return;
  // }
  // iweda_change_url_if_need_redirect(music_play_IWEDA_handle);

  // // 检查资源可用性
  // if (iweda_check_common_url(music_play_IWEDA_handle) == ESP_OK) {

  //   board_ctrl_t *b = board_status_get();
  //   b->amplifier_mute = false;
  //   b->amplifier_sd = true;
  //   b->amplifier_volume = 60;
  //   sevetest30_board_ctrl(b, BOARD_CTRL_AMPLIFIER);

  //   music_uri_or_url_play(music_play_IWEDA_handle->url_buf, 1);

  //   music_FFT_UI_handle_t handle = music_FFT_UI_start(&FFT_UI_cfg, 1);
  //   if (!handle) {
  //     ESP_LOGE("main", "handle 为空,无法绘制任务");
  //     return;
  //   }

  //   while (1) {
  //     if (handle &&
  //         xSemaphoreTake(update_ui_data_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
  //         {
  //       clean_all_draw_buf();
  //       music_FFT_UI_draw(handle);
  //       xSemaphoreGive(update_ui_data_mutex);
  //     }
  //     vTaskDelay(pdMS_TO_TICKS(10));
  //     if (!sevetest30_music_running_flag) {
  //       music_FFT_UI_stop(handle);
  //       delete_iweda_handle(music_play_IWEDA_handle);
  //       music_play_IWEDA_handle = NULL;
  //       break;
  //     }
  //   }
  // }

  // // 震动马达-震动测试
  //    while(1){
  //    vibra_motor_start();
  //    vTaskDelay(pdMS_TO_TICKS(500));
  //    vibra_motor_stop();
  //    vTaskDelay(pdMS_TO_TICKS(500));
  //   }

  // // 震动马达-震动测试+实时时间显示，模拟闹钟响铃
  // while (1)
  // {
  //   refresh_systemtime_data();
  //   if (xSemaphoreTake(update_ui_data_mutex, pdMS_TO_TICKS(100)) ==
  //   pdTRUE)
  //   {
  //     clean_all_draw_buf();
  //     time_UI_h_m_s(1, 1);
  //     xSemaphoreGive(update_ui_data_mutex);
  //   }
  //   vibra_motor_start();
  //   vTaskDelay(pdMS_TO_TICKS(500));
  //   vibra_motor_stop();
  //   vTaskDelay(pdMS_TO_TICKS(500));
  // }

  // // // 震动马达-触感反馈测试
  // vTaskDelay(pdMS_TO_TICKS(1000));
  // ext_io_ctrl.auto_read_EN = true;
  // ext_io_ctrl.auto_read_INT = false;
  // while (1)
  // {
  //   if (ext_io_ctrl.auto_read_INT)
  //   {
  //     if (ext_io_level_service() == ESP_OK)
  //     {
  //       ext_io_ctrl.auto_read_INT = false;
  //       ESP_LOGW("main", "扩展GPIO自动读取中断触发成功");
  //       vibra_motor_start();
  //       vTaskDelay(pdMS_TO_TICKS(50));
  //       vibra_motor_stop();
  //     }
  //   }
  //   vTaskDelay(pdMS_TO_TICKS(100));
  // }

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
  // board_tts->amplifier_volume = 60;
  // sevetest30_board_ctrl(board_tts, BOARD_CTRL_AMPLIFIER);

  // TTS_cfg_t tts_cfg =
  // TTS_DEFAULT_CONFIG("你好，我是SEVETEST30，你可以和我聊天", 0);
  // tts_service_play_short(&tts_cfg, 1); // 启动TTS服务

  // music_FFT_UI_handle_t FFT_UI_handle = music_FFT_UI_start(&FFT_UI_cfg, 1);
  // if (!FFT_UI_handle)
  // {
  //   ESP_LOGE("main", "FFT_UI_handle 为空,无法启动绘制任务");
  //   return;
  // }

  // while (1)
  // {
  //   if (FFT_UI_handle && xSemaphoreTake(update_ui_data_mutex,
  //   pdMS_TO_TICKS(10)) == pdTRUE)
  //   {
  //     clean_all_draw_buf();
  //     music_FFT_UI_draw(FFT_UI_handle);
  //     xSemaphoreGive(update_ui_data_mutex);
  //   }
  //   vTaskDelay(pdMS_TO_TICKS(10));
  //   if (!sevetest30_music_running_flag)
  //   {
  //     music_FFT_UI_stop(FFT_UI_handle);
  //   }
  // }

  // // AI交流例程
  // AI_chat();
}

void _main_chat_wait_cb(void) {
  static uint8_t loading_bar[(LINE_LED_NUMBER / 8 + 1) * sizeof(uint8_t) +
                             sizeof(uint32_t)] = {0};
  if (xSemaphoreTake(update_ui_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    uint8_t color[3] = {0};
    static float H = 0.0f;
    H += 1.0f;
    if (H >= 360.0f) {
      H = 0.0f;
    }
    ui_tool_hsv2rgb(H, 1.0f, 1.0f, color);
    build_rectangle(LINE_LED_NUMBER, 1, loading_bar, sizeof(loading_bar));
    separation_draw(1, VERTICAL_LED_NUMBER, LINE_LED_NUMBER,
                    RECTANGLE_MATRIX(loading_bar), matrix_size(loading_bar),
                    color);
    xSemaphoreGive(update_ui_data_mutex);
  }
  vTaskDelay(pdMS_TO_TICKS(10));
}

void AI_chat(void) {
  const char *TAG = "AI_chat";
  ESP_LOGI(TAG, "AI_chat 开始");

  ASR_cfg_t asr_cfg = ASR_DEFAULT_CONFIG(50, ASR_PID_CM_NEAR, VAD_MODE_3);
  asr_service_begin(&asr_cfg, 1);

  GPT_chat_handle_t GPT_chat_handle =
      GPT_chat_start(ERNIE_BOT_URL, CONFIG_GPT_CHAT_ACCESS_KEY,
                     "ernie-4.5-turbo-128k", "", 60000);
  if (GPT_chat_handle == NULL) {
    ESP_LOGE(TAG, "GPT_chat_handle 为空,无法绘制任务");
    return;
  }

  // 启用多轮对话
  if (GPT_chat_enable_multi_round_chat(GPT_chat_handle, 8192, 10 * 60 * 1000) !=
      ESP_OK) {
    GPT_chat_stop(GPT_chat_handle);
    ESP_LOGE(TAG, "GPT_chat_enable_multi_round_chat 失败\n");
    return;
  }

  // 设置系统JSON字段
  char system_json_field[2048] = {0};

  if (GPT_chat_set_system_json_field(GPT_chat_handle, system_json_field) !=
      ESP_OK) {
    GPT_chat_stop(GPT_chat_handle);
    ESP_LOGE(TAG, "GPT_chat_set_system_json_field 失败");
    return;
  }

  while (1) {

    if (xSemaphoreTake(update_ui_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      clean_all_draw_buf();
      facial_expression_show(1, 1, "normal");
      xSemaphoreGive(update_ui_data_mutex);
    }

    int64_t start_time = esp_timer_get_time();
    while (1) {
      vTaskDelay(pdMS_TO_TICKS(500));

      int64_t current_time = esp_timer_get_time();
      if (current_time - start_time > 30 * 1000 * 1000) {
        sevetest30_asr_running_flag = false;
        GPT_chat_stop(GPT_chat_handle);
        ESP_LOGW(TAG, "长时间未获取到样本,自动退出对话");
        return;
      }

      if (ext_io_ctrl.auto_read_INT) {
        if (ext_io_level_service() == ESP_OK) {
          ext_io_ctrl.auto_read_INT = false;
          if (board_ctrl.p_ext_io_value->thumbwheel_CW == 0 &&
              board_ctrl.p_ext_io_value->thumbwheel_CCW == 0) {
            sevetest30_asr_running_flag = false;
            GPT_chat_stop(GPT_chat_handle);
            ESP_LOGW(TAG, "用户退出对话");
            return;
          }
        }
      }

      if (sevetest30_asr_result_text) {
        if (sevetest30_asr_result_text[0] != 0) {
          sevetest30_asr_running_flag = false; // 获取到样本,强制终止ASR服务
          break;
        }
      }
    }
    sevetest30_asr_running_flag = false;

    refresh_battery_data();
    refresh_env_TVOC_data(true);

    // 添加system内容
    int system_json_field_len = snprintf(
        system_json_field, sizeof(system_json_field),
        "{\"role\": \"system\",\"content\": "
        "\""
        "%s"
        "time_zone:%s,"
        "以下是来自闹钟硬件传感器的数据"
        "battery_data.charge_flag:%d(0:不在充电,1:充电中),"
        "battery_data.result.battery_soc:%.2f%%(-1表示获取失败),"
        "battery_data.result.battery_voltage:%.2fmV(-1表示获取失败),"
        "env_TVOC_data:%" PRId64 "ppb(-1表示预热中,不可用,大概需2分钟)"
        "env_env_temp_hum_data.temperature:%.2f℃,"
        "env_env_temp_hum_data.humidity:%.2f,"
        "以下是来自网络API的数据"
        "(如current_weather_data来自天气API),"
        "current_weather_data.temperature:%.2f℃,"
        "current_weather_data.humidity:%.2f,"
        "current_weather_data.uvIndex:%.2f,"
        "position_data.longitude:%s,"
        "position_data.latitude:%s,"
        "position_data.country:%s,"
        "position_data.adm1:%s,"
        "position_data.adm2:%s,"
        "position_data.name:%s,"
        "\"},",
        system_json_head_prompt, CONFIG_LOCAL_TZ, battery_data.charge_flag,
        battery_data.result.battery_soc, battery_data.result.battery_voltage,
        (env_TVOC_data.valid == false) ? -1 : (int64_t)env_TVOC_data.TVOC_value,
        env_temp_hum_data.temperature, env_temp_hum_data.humidity,
        current_weather_data.temperature, current_weather_data.humidity,
        current_weather_data.uvIndex,
        (position_data.longitude == NULL) ? "" : position_data.longitude,
        (position_data.latitude == NULL) ? "" : position_data.latitude,
        (position_data.country == NULL) ? "" : position_data.country,
        (position_data.adm1 == NULL) ? "" : position_data.adm1,
        (position_data.adm2 == NULL) ? "" : position_data.adm2,
        (position_data.name == NULL) ? "" : position_data.name);

    if (system_json_field_len >= sizeof(system_json_field)) {
      ESP_LOGE(TAG, "system_json_field 内存不足");
      memset(system_json_field, 0, sizeof(system_json_field));
    }

    if (GPT_chat_update_user_content(GPT_chat_handle,
                                     sevetest30_asr_result_text) != ESP_OK) {
      GPT_chat_stop(GPT_chat_handle);
      ESP_LOGE(TAG, "GPT_chat_update_user_content 失败");
      return;
    }
    if (GPT_chat_text_exchange(GPT_chat_handle, 1, _main_chat_wait_cb) !=
        ESP_OK) {
      GPT_chat_stop(GPT_chat_handle);
      ESP_LOGE(TAG, "GPT_chat_text_exchange 失败");
      return;
    }

    // 如果正常回复,TTS语音播放
    if (GPT_chat_handle->result) {
      if (GPT_chat_handle->result[0] != 0) {
        board_ctrl_t *b = board_status_get();
        b->amplifier_mute = false;
        b->amplifier_sd = true;
        b->amplifier_volume = 60;
        sevetest30_board_ctrl(b, BOARD_CTRL_AMPLIFIER);

        if (GPT_chat_handle->result != NULL) {

          // 启动TTS服务
          if (strlen(GPT_chat_handle->result) >
              BAIDU_SHORT_TTS_TEXT_STRLEN_MAX) {
            TTS_cfg_t tts_cfg =
                LONG_TTS_DEFAULT_CONFIG(GPT_chat_handle->result, 4189);
            tts_service_play_long(&tts_cfg, 1, 120000);
          } else {
            TTS_cfg_t tts_cfg =
                SHORT_TTS_DEFAULT_CONFIG(GPT_chat_handle->result, 4189);
            tts_service_play_short(&tts_cfg, 1);
          }

          // 显示表情
          char emotion_lable[512] = {0};
          if (fetch_text_emotion(GPT_chat_handle->result, emotion_lable,
                                 sizeof(emotion_lable), 10000) == ESP_OK) {
            ESP_LOGI(TAG, "情绪标签:%s", emotion_lable);
            if (xSemaphoreTake(update_ui_data_mutex, pdMS_TO_TICKS(100)) ==
                pdTRUE) {
              clean_all_draw_buf();
              facial_expression_show(1, 1, emotion_lable);
              xSemaphoreGive(update_ui_data_mutex);
            }
          } else {
            ESP_LOGW(TAG, "fetch_text_emotion 失败,显示正常表情");
            if (xSemaphoreTake(update_ui_data_mutex, pdMS_TO_TICKS(100)) ==
                pdTRUE) {
              clean_all_draw_buf();
              facial_expression_show(1, 1, "normal");
              xSemaphoreGive(update_ui_data_mutex);
            }
          }

          while (sevetest30_music_running_flag) {
            if (ext_io_ctrl.auto_read_INT) {
              if (ext_io_level_service() == ESP_OK) {
                ext_io_ctrl.auto_read_INT = false;
                if (board_ctrl.p_ext_io_value->thumbwheel_CW == 0 &&
                    board_ctrl.p_ext_io_value->thumbwheel_CCW == 1) {
                  sevetest30_music_running_flag = false;
                  if (xSemaphoreTake(update_ui_data_mutex,
                                     pdMS_TO_TICKS(100)) == pdTRUE) {
                    clean_all_draw_buf();
                    xSemaphoreGive(update_ui_data_mutex);
                  }
                  vTaskDelay(pdMS_TO_TICKS(3000)); // 等待结束
                  break;
                }
              }
            }
            vTaskDelay(pdMS_TO_TICKS(500));
          }
          sevetest30_music_running_flag = false;
        }
      }
    }

    memset(sevetest30_asr_result_text, 0,
           ASR_RESULT_TEX_BUF_MAX * sizeof(char));
    asr_service_begin(&asr_cfg, 1);
  }
}
