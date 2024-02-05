// 该文件由701Enti编写
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 邮箱：   hi_701enti@yeah.net


//      在此，望代表全体 ESPRESSIF硬件及软件框架的使用者对 乐鑫科技-ESPRESSIF ESP-IDF ESP-ADF ESP-DSP各种框架及相关创作者们 表示由衷感谢
//      在此，望代表全体 网易云音乐API的使用者对 网易云音乐 网易云音乐API及相关创作者们 表示由衷感谢
//      在此，望代表全体 zlib解压缩库的使用者对 zlib及相关创作者们 表示由衷感谢
//      在此, 望代表全体 文心一言-ERNIE-Bot 百度语音识别 百度语音合成TTS支持 等各种API使用者对 百度智能云千帆大模型平台 API服务及相关创作者们表示由衷感谢
//      在此，望代表全体 和风天气API 使用者对 和风天气及相关创作者们 表示由衷感谢


// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.6.27

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

#include "TCA6416A.h"


TCA6416A_mode_t  ext_io_mode_data; //公共使用 扩展IO输入输出模式
TCA6416A_value_t ext_io_value_data;//公共使用 扩展IO电平信息

void fresh_LED(){

}
void app_main(void)
{
  wifi_connect();
  
  board_device_handle_t board_device_handle;
  static board_ctrl_t board_ctrl = {
    .p_ext_io_mode = &ext_io_mode_data,//存储IO模式信息的结构体的地址
    .p_ext_io_value = &ext_io_value_data,//存储IO电平信息的结构体的地址
    .boost_voltage = BV_VOL_MAX,
    .amplifier_volume = AMP_VOL_MAX,
    .amplifier_mute = false,
    .codec_audio_hal_ctrl = AUDIO_HAL_CTRL_START,
    .codec_mode =  AUDIO_HAL_CODEC_MODE_BOTH, 
    .codec_adc_gain = MIC_GAIN_MAX,
    .codec_dac_pin = DAC_OUTPUT_ALL,
    .codec_dac_volume = 100,
    .codec_adc_pin = ADC_INPUT_LINPUT1_RINPUT1,
  };

    sevetest30_all_board_init(&board_ctrl,&board_device_handle);
  
//  init_time_data_sntp();

  // const char *uri1="http://m801.music.126.net/20240131235843/e01f51a9945a199c29733bd93f7db790/jdymusic/obj/wo3DlMOGwrbDjj7DisKw/31271000729/d21b/e9a7/ccf9/610fb06e731b320af25af6b5d8cb60b0.mp3";
  // music_uri_play(uri1,1);

  // char *text1 = "The audio data is typically acquired using an input Stream, processed with Codecs and in some cases with Audio Processing functions";
  // baidu_TTS_cfg_t tts_cfg = BAIDU_TTS_DEFAULT_CONFIG(text1,1);
  // tts_service_play(&tts_cfg,1);

  baidu_ASR_cfg_t asr_cfg;
  asr_cfg.rate = ASR_RATE_16K;
  asr_cfg.stop_threshold = 100;
  asr_cfg.send_threshold = 20;
  asr_cfg.record_save_times_max = 100;
  asr_service_start(&asr_cfg,1);

  //   for(;;){
  //   refresh_time_data();
  //   time_UI_2(1,1,1);
  //    for (int i=0;i<=5;i++){
  //     ledarray_set_and_write(i);
  //     vTaskDelay(pdMS_TO_TICKS(10));
  //    }
  // }



    // uint8_t color={10,10,10};
    // separation_draw(0,0,2*8,fonts_read_zh_CN_12x(&board_device_handle),12*2,color,1);

    
  
  // tts_service();


  // for(;;){
  //   if (ext_io_ctrl.auto_read_INT == true){
  //   ext_io_ctrl.auto_read_INT = false;
  //   ext_io_value_service();
  //   ESP_LOGI("ME","自动读取触发");
  //   } 
  // vTaskDelay(pdMS_TO_TICKS(1000));
  // }


 

  // init_time_data_sntp();

  // refresh_position_data();
  // refresh_weather_data();

  // weather_UI_1(1, 1, 1);
  
  // for (int i = 0; i < 6; i++)
  // ledarray_set_and_write(i); 

}
