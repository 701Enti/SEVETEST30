// 该文件由701Enti编写，包含一些sevetest30的 离线环境 数据获取（SWEDA）
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：有效的数据存储变量都封装在该库下，不需要在外部函数定义一个数据结构体缓存作为参数，直接读取公共变量，主要为了方便FreeRTOS的任务支持
//       该文件对于硬件的配置针对sevetest30,使用前请参考兼容性问题
//       文件本体不包含i2c通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行,库中有仅为字库SPI通讯提供的SPI配置函数
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.8.13

#include "sevetest30_SWEDA.h"

#include "esp_sntp.h"
#include "esp_log.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#include "sevetest30_gpio.h"

systemtime_t systemtime_data;
battery_data_t battery_data;

esp_adc_cal_characteristics_t adc_chars;

// 刷新缓存的ESP32S3内部系统时间，该函数需要频繁调用，以获取不断改变的内部系统时间，内部系统时间来源于ESP32S3内部RTC，掉电数据将丢失，需要NTP对时(网络对时的初始化函数在 sevetest30_IWEDA.h)
void refresh_time_data()
{
    const char *TAG = "refresh_time_data";

    // 获取内部系统时间，参考了官方文档  https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.4/esp32/api-reference/system/system_time.html?highlight=time
    time_t now_time;
    char time_buf[64] = {0};
    struct tm time_info;

    time(&now_time);
    setenv("TZ", "CST-8", 1);                               // 设定时区
    localtime_r(&now_time, &time_info);                     // 读取本地时间
    strftime(time_buf, sizeof(time_buf), "%c", &time_info); // 对时间数据格式化

    // 解析保存系统时间
    char week_buf[10] = {0};
    char month_buf[10] = {0};
    // 数据截取
    sscanf(time_buf, " %s %s %d %d:%d:%d %d", week_buf, month_buf, &systemtime_data.day,
           &systemtime_data.hour, &systemtime_data.minute, &systemtime_data.second,
           &systemtime_data.year);
    // 由于星期和月份数据是字符串，转换成数字以便显示和分析，switch表达式不支持字符串，但是可以通过字母的各个对比确定

    switch (week_buf[0])
    {
    case 'M':
        systemtime_data.week = 1; // 星期一 Monday
    break;

    case 'T':
        if(week_buf[1] == 'u')
           systemtime_data.week = 2; // 星期二 Tuesday
        if(week_buf[1] == 'h')
          systemtime_data.week = 4; // 星期四 Thursday
    break;
   
    case 'W':
        systemtime_data.week = 3; // 星期三 Wednesday
    break;

    case 'F':
        systemtime_data.week = 5; // 星期五 Friday
    break;

    case 'S':
        if(week_buf[1] == 'a')
          systemtime_data.week = 6; // 星期六 Saturday
      
        if(week_buf[1] == 'u')
          systemtime_data.week = 7; // 星期日 Sunday
    break;

    default:
        ESP_LOGI(TAG,"无法判断的数据 %s",week_buf);
        systemtime_data.week = 0;
    break;
    }

    switch (month_buf[0])
    {
    case 'J':
      if (month_buf[1]=='a')
        systemtime_data.month = 1;//一月 January
      if (month_buf[1]=='u'){
        if (month_buf[2]=='n')
            systemtime_data.month = 6;//六月 June
        if (month_buf[2]=='l')
            systemtime_data.month = 7;//七月 July
      }
    break;
    
    case 'M':
      if (month_buf[2]=='r')
        systemtime_data.month = 3;//三月 March
      if (month_buf[2]=='y')
        systemtime_data.month = 5;//五月 May
    break;
    
    case 'A':
      if (month_buf[1]=='u')
        systemtime_data.month = 8;//八月 August
      if (month_buf[1]=='p')
        systemtime_data.month = 4;//四月 April
    break;
 
    case 'F':
      systemtime_data.month = 2;//二月 February
    break;

    case 'S':
        systemtime_data.month = 9;//九月 September
    break;

    case 'O':
        systemtime_data.month = 10;//十月 October
    break;

    case 'N':
        systemtime_data.month = 11;//十一月 November
    break;

    case 'D':
        systemtime_data.month = 12;//十二月 December
    break;

    default:
       ESP_LOGI(TAG,"无法判断的数据 %s",month_buf);
       systemtime_data.month = 0;
    break;
    }
}

// 刷新缓存的电池数据，充电状态，该函数需要频繁调用
void refresh_battery_data(){
    const char *TAG = "refresh_battery_data";
    //电池数据请求
    
    //充电状态
 
}