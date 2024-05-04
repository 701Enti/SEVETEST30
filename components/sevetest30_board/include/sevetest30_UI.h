
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

// 该文件归属701Enti组织，SEVETEST30开发团队应该提供责任性维护，包含对传感器数据，网络API等数据的 整理显示与动画交互工作
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：该库自动调用 IWEDA库 SWEDA库 BWEDA库 读取数据，无需任何干涉，因此需要依赖一些库获取缓存变量，图像数据将只在文件函数内生效来节省内存，不会声明
// 显示UI根据sevetest30实际定制，特别是图像坐标，如果需要改变屏幕大小，建议自行设计修改
// github: https://github.com/701Enti
// bilibili: 701Enti

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
#define FFT_UI_TASK_STACK_SIZE (3*1024)//FFT UI绘制任务堆栈大小

#define DSP_MAX_FFT_SIZE    32768     //最大FFT大小

// FFT 点数N 取2的整数次幂,测试发现4096及以下点数运行较稳定,8192及以上任务运行过程中可能崩溃
#define FFT_N_SAMPLES 4096     

#define FFT_DAMPEN_MULTIPLES 50 // FFT 音频数据衰减倍数
#define FFT_VIEW_DATA_MIN 0   // FFT UI视口音频数据最小值
#define FFT_VIEW_DATA_MAX 500// FFT UI视口音频数据最大值
#define FFT_VIEW_WIDTH_MAX 100 //// FFT UI视口宽度最大值

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
   int16_t x;///起始位置X坐标
   int16_t y;///起始位置Y坐标
   uint8_t change;///亮度
   int width;///视口宽度 图谱显示会自动适应视口宽度
   int height;///显示高度 会裁切不在显示高度的图谱
   UI_color_visual_cfg_t visual_cfg;//频谱颜色可视化配置
}music_FFT_UI_cfg_t;


extern bool volatile sevetest30_fft_ui_running_flag;


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

void music_FFT_UI_start(music_FFT_UI_cfg_t* UI_cfg, UBaseType_t priority);


