// 该文件由701Enti编写，包含一些sevetest30的离线方式即通过传感器进行环境数据获取（SWEDA）
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：有效的数据存储变量都封装在该库下，不需要在外部函数定义一个数据结构体缓存作为参数，直接读取公共变量，主要为了方便FreeRTOS的任务支持
//       该文件对于硬件的配置针对sevetest30,使用前请参考兼容性问题
// 邮箱：   3044963040@qq.com
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

// 刷新缓存的系统时间，该函数需要频繁调用，以同步地获取不断改变的系统时间(初始化NTP矫正函数在 sevetest30_IWEDA.h)
void refresh_time_data()
{
    const char *TAG = "refresh_time_data";

    // 获取系统时间，参考了官方文档  https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.4/esp32/api-reference/system/system_time.html?highlight=time
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

// 刷新缓存的电池数据，包括电池电压，充电状态，该函数需要频繁调用，以同步地获取不断改变的电池电压
void refresh_battery_data(){

    const char *TAG = "refresh_battery_data";

    //实时电压
    uint32_t raw  = adc1_get_raw(ADC1_CHANNEL_8);
    battery_data.voltage = esp_adc_cal_raw_to_voltage(raw,&adc_chars) * 2;//sevetest30硬件设计中，检测端电压为实际电压一半
    
    //充电状态
    battery_data.charge_flag   = ! ext_io_value_data.charge_SIGN;// 正在充电信号 0有效
    battery_data.finished_flag = ! ext_io_value_data.finished_SIGN;// 充电完成信号 0有效
}

//初始化电池ADC电压监测硬件,ADC方式,开机只要调用一次
void init_battery_voltage_data_ADC()
{ 
  //启动ADC电源
  adc_power_acquire();

  //设置采样位数
  adc_bits_width_t width_bit = ADC_WIDTH_BIT_12;
  adc1_config_width(width_bit);

  //设置通道与测量分辨率（采样前端衰减）
  adc_channel_t channel = ADC_CHANNEL_8;//对应的GPIO为电压检测端
  adc_atten_t atten= ADC_ATTEN_DB_11;   //范围扩展到150 mV ~ 2450 mV
  adc1_config_channel_atten(channel,atten);

  adc_unit_t adc_num = ADC_UNIT_1;

  //数据整合，获取结果时需要
  esp_adc_cal_characterize(adc_num,atten,width_bit,2450,&adc_chars);//2450为参考电压，但是项目使用ESP32S3,这个数据其实是无效的，只是为了占位
}