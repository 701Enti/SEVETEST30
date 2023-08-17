// 该文件由701Enti编写
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 邮箱：   3044963040@qq.com
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.6.27

#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/projdefs.h"

#include "esp_log.h"

#include "sevetest30_IWEDA.h"
#include "sevetest30_SWEDA.h"
#include "sevetest30_gpio.h"
#include "sevetest30_BusConf.h"
#include "sevetest30_LedArray.h"
#include "sevetest30_UI.h"

#include "amplifier.h"

void app_main(void)
{

  // char *ssid = "CMCC-102";
  // char *password = CONFIG_WIFI_PASSWORD;
  // wifi_connect(ssid, password);

  I2C_init(); //I2C初始化
  sevetest30_gpio_init();//全局GPIO初始化

  // init_battery_voltage_data_ADC();
  // for (;;){
  //   refresh_battery_data(); 
  //   // amplifier_set(false,false,24);
  //   if(battery_data.charge_flag == true)ESP_LOGI("ME","正在充电");
  //   vTaskDelay(pdMS_TO_TICKS(5000));   
  // }

  // init_time_data_sntp();

  // refresh_position_data();
  // refresh_weather_data();

  // weather_UI_1(1, 1, 1);
  
  // for (int i = 0; i < 6; i++)
  //   ledarray_set_and_write(i); 
}
