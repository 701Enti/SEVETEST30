// 该文件由701Enti编写，包含对传感器数据，网络API等数据的 整理显示与动画交互工作
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：图像数据将只在文件函数内生效来节省内存
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
#include <esp_err.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <sevetest30_LedArray.h>
#include <driver/i2s.h>
#include "board_ctrl.h"

#define FFT_UI_TASK_CORE (1)     // FFT UI绘制任务核心
#define FFT_UI_TASK_STACK_SIZE (1*1024)//FFT UI绘制任务堆栈大小

#define DSP_MAX_FFT_SIZE    32768     //最大FFT大小
#define FFT_N_SAMPLES 8192      // FFT 点数N 取2的整数次幂
#define FFT_DAMPEN_MULTIPLES 50 // FFT 音频数据衰减倍数
#define FFT_VIEW_DATA_MIN 0   // FFT UI视口音频数据最小值
#define FFT_VIEW_DATA_MAX 500// FFT UI视口音频数据最大值
#define FFT_VIEW_WIDTH_MAX 100 //// FFT UI视口宽度最大值

extern bool volatile sevetest30_fft_ui_running_flag;

typedef struct UI_color_visual_cfg_t
{
   uint8_t value_max;  // 映射结果的最大值
   int high;           // 较高的值
   int medium;         // 中等的值
   int low;            // 较低的值
   int public_divisor; // 公共除数，抑制数据变化动态，过大过小都会出现全部填充value_max的情况，导致色彩单一,可从 (high - low)/2 开始 调整到可视化动态效果最加即可
   float x_multiples;  // 视口横向缩放倍数,控制视口的横向放大缩小 缩小<1 放大>1 不进行 (如横向视口放大到200% x_multiples = 2)
   float x_move;       // 视口偏移系数  左偏移<0 右偏移>0 不进行 0 (如把视口向右横向偏移屏幕的一半 x_move = 0.5)
} UI_color_visual_cfg_t;

typedef struct music_FFT_UI_cfg_t
{
   i2s_port_t fft_i2s_port;//需要监视的I2S端口
   int16_t x;///起始位置X坐标
   int16_t y;///起始位置Y坐标
   uint8_t change;///亮度
   int width;///视口宽度 图谱显示会自动适应视口宽度
   int height;///显示高度 会裁切不在显示高度的图谱
   UI_color_visual_cfg_t visual_cfg;//频谱颜色可视化配置
}music_FFT_UI_cfg_t;


// 数据可视化函数
void data_to_color(int data, UI_color_visual_cfg_t *visual_cfg, uint8_t *high, uint8_t *comfort, uint8_t *low);

void temp_to_color(int temp, uint8_t value_max, uint8_t *high, uint8_t *comfort, uint8_t *low);

// UI快捷绘制函数，以下函数自动解析处理数据，直接绘制一个独立的界面，将图案（私有的）和数据拼凑 并且起始坐标可控 连接动画控制非常方便
// change控制显示亮度，正数为增加值，负数时，为减少值，0时为原图亮度

//可以直接调用的
void weather_UI_1(int16_t x, int16_t y, uint8_t change);

void time_UI_1(int16_t x, int16_t y, uint8_t change);

void time_UI_2(int16_t x, int16_t y, uint8_t change);

void music_FFT_UI_draw(music_FFT_UI_cfg_t *UI_cfg);

void main_UI_1();

//需要创建任务调用的
void music_FFT_UI_start(music_FFT_UI_cfg_t* UI_cfg, UBaseType_t priority);


