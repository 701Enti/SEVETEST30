// 该文件由701Enti编写，包含一些sevetest30的离线方式即通过传感器进行环境数据获取（SWEDA）
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：有效的数据存储变量都封装在该库下，不需要在外部函数定义一个数据结构体缓存作为参数，直接读取公共变量，主要为了方便FreeRTOS的任务支持
//       该文件对于硬件的配置连接完全针对sevetest30,使用前请参考兼容性问题
// 邮箱：   3044963040@qq.com
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.8.13

#ifndef _SEVETEST30_SWEDA_H_
#define _SEVETEST30_SWEDA_H_
#endif

#include <stdio.h>
#include <stdbool.h>

typedef struct systemtime_t{
  int year;
  int month;
  int day;
  int week;
  int hour;
  int minute;
  int second;
}systemtime_t;

typedef struct battery_data_t{
  uint32_t voltage;//实时电池电压(端电压 单位 mV)
  uint16_t  percentage;//电池电量百分比 0-100 (只检测端电压,使用平均滤波简单地平衡电量百分比(精确到十分位，所以保存到该成员前乘上了10))
  bool charge_flag;//正在充电标识(正在充电为 true)
  bool finished_flag;//充电完成标识(完成充电为 true)
}battery_data_t;


extern systemtime_t systemtime_data;
extern battery_data_t battery_data;

void init_battery_voltage_data_ADC();



void refresh_battery_data();

void refresh_time_data();