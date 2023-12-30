// 该文件由701Enti编写
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 邮箱：   hi_701enti@yeah.net


//      在此，701Enti望代表全体 ESPRESSIF硬件及软件框架的使用者对 乐鑫科技-ESPRESSIF ESP-IDF ESP-ADF ESP-DSP各种框架及相关创作者们 表示由衷感谢
//      在此，701Enti望代表全体 网易云音乐API的使用者对 网易云音乐 网易云音乐API及相关创作者们 表示由衷感谢
//      在此，701Enti望代表全体 zlib解压缩库的使用者对 zlib及相关创作者们 表示由衷感谢
//      在此, 701Enti望代表全体 文心一言-ERNIE-Bot 百度语音识别 百度语音合成TTS支持 等各种API使用者对 百度智能云千帆大模型平台 API服务及相关创作者们表示由衷感谢
//      在此，701Enti望代表全体 和风天气API 使用者对 和风天气及相关创作者们 表示由衷感谢


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
#include "api_baiduTTS.h"
#include "fonts_chip.h"

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
  char *ssid = "CMCC-102";
  char *password = CONFIG_WIFI_PASSWORD;
  wifi_connect(ssid, password);

 

  board_device_handle_t board_device_handle;
  board_ctrl_t board_ctrl = {
    .p_ext_io_mode = &ext_io_mode_data,//存储IO模式信息的结构体的地址
    .p_ext_io_value = &ext_io_value_data,//存储IO电平信息的结构体的地址
    .boost_voltage = BV_VOL_MAX,
    .amplifier_volume =AMP_VOL_MAX,
  };

    sevetest30_all_board_init(&board_ctrl,&board_device_handle);
  
 init_time_data_sntp();

  const char *uri1="http://m7.music.126.net/20240101235954/ea28f6a802dba05bb92aae38c50c8e13/ymusic/035e/045a/0459/44715fce9730587377e3336e3413f69c.mp3";
  NetEase_music_uri_play(uri1,1);


    TickType_t waketime;
    waketime = xTaskGetTickCount();

    for(;;){
    refresh_time_data();
    time_UI_2(1,1,1);  
    for (int i = 1; i < 5; i++){
      ledarray_set_and_write(i);
      vTaskDelayUntil(&waketime,pdMS_TO_TICKS(1000)); 
    }
  }


 






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
