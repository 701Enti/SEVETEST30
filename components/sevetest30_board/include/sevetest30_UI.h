// 该文件由701Enti编写，包含对传感器数据，网络API等数据的 整理显示与动画交互工作
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：该库自动调用 IWEDA库 SWEDA库 BWEDA库 读取数据，无需任何干涉，因此需要依赖一些库获取缓存变量，图像数据将只在文件函数内生效来节省内存，不会声明 
// 显示UI根据sevetest30实际定制，特别是图像坐标，如果需要改变屏幕大小，建议自行设计修改
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.8.2

#ifndef _SEVETEST_UI_H_
#define _SEVETEST_UI_H_
#endif 

#include <string.h>
#include <stdbool.h>


//UI快捷绘制函数，以下函数自动解析处理数据，直接绘制一个独立的界面，将图案（私有的）和数据拼凑 并且起始坐标可控 连接动画控制非常方便
//change控制显示亮度，正数为增加值，负数时，为减少值，0时为原图亮度


void weather_UI_1(int16_t x,int16_t y,uint8_t change);

void time_UI_1(int16_t x,int16_t y,uint8_t change);

void time_UI_2(int16_t x,int16_t y,uint8_t change);
