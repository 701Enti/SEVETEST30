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

// 包含ICND2038S+ICND2013构成的LED阵列的图形与显示处理,RGB彩色显示由软件实现
// ICND2038S硬件只能控制灯的开关,所以这里使用BCM调光算法实现0-255灰度RGB彩色显示
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体包含硬件驱动代码
// 绘制函数本身不会刷新屏幕,需要运行屏幕刷新,才会在屏幕上点亮
// 绘制函数规定使用字模点阵的左上角的点作为其坐标表示点,长边方向向右为X轴正方向,短边方向向下为Y轴正方向,
// 原点坐标为(1,1)，即如果要让图像显示在左上角应填入函数 x = 1,y = 1
// 将使用SPI外设+软件IO控制ICND2038S,软件IO控制ICND2013,来实现灯的亮灭
// BCM调光算法实现0-255灰度RGB彩色显示
// [separation_draw三色分离方式]
// 取模适配PCtoLCD2002
// 取模说明：从第一行开始向右每取8个点作为一个字节，如果最后不足8个点就补满8位。
// 取模顺序是从高到低，即第一个点作为最高位。如*-------取为10000000
// [direct_draw彩色图像直显方式]
// 取模方式适配Img2Lcd
// 水平扫描，从左到右，从顶到底扫描，24位真彩（RGB顺序），需要图像数据头
// 图像编辑可以用系统自带的画板工具，像素调到合适值如如屏幕尺寸32x16,不显示的地方要填充黑色
// 选择带数据头的图案数据，长宽会自动获取
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// github: https://github.com/701Enti

#pragma once

#include <sevetest30_UI.h>

#define FIGURE_BREATH 4 // 数字的宽度 4x7
#define FIGURE_HEIGHT 7 // 数字的高度 4x7

#define WEATHER_ICON_BREATH 9 // 天气图标的宽度 9x9
#define WEATHER_ICON_HEIGHT 9 // 天气图标的高度 9x9



// 矩形字模数据区位置,需要填入矩形的位置
#define RECTANGLE_MATRIX(pRECTANGLE) (pRECTANGLE + sizeof(uint64_t))

// 矩形最大允许数据字节数，这决定矩形生成函数可以生成多大矩形
#define RECTANGLE_SIZE_MAX 1024



// 字库打印函数最大单次打印字符数
#define FONT_CHIP_PRINT_NUM_MAX 128

// 字库打印函数格式化缓存大小,缓存使用char类型(占一个字节),UTF-8最多用6个字节表达一个字符+预留
#define FONT_CHIP_PRINT_FMT_BUF_SIZE (FONT_CHIP_PRINT_NUM_MAX * 10)



// 屏幕刷新任务运行核心,请设置完全闲置的核心，刷新任务将完全占有CPU
#define LEDARRAY_REFRESH_TASK_CORE 1

// 屏幕刷新任务优先级
#define LEDARRAY_REFRESH_TASK_PRIO 10

// 屏幕刷新任务堆栈大小
#define LEDARRAY_REFRESH_TASK_STACK_SIZE 1024 * 8



// 硬件刷新函数竞争刷新锁最大超时时间(单位ms)
#define LEDARRAY_REFRESH_MUTEX_SHOW_TAKE_TIMEOUT_MS 0

// 硬件刷新资源管理函数竞争刷新锁最大超时时间(单位ms)
#define LEDARRAY_REFRESH_MUTEX_MANAGE_TAKE_TIMEOUT_MS 1000



// BCM调光算法 - 单位时延对应的NOP空指令个数
#define LEDARRAY_REFRESH_BCM_DELAY_NOP_NUM 1

typedef enum {
  LEDARRAY_AUTO_REFRESH_DISABLE = 0, // 禁用屏幕自动刷新
  LEDARRAY_AUTO_REFRESH_ALL_ONCE, //[单次全刷]一次性刷新整个屏幕所有行,全屏刷新之后才发生延时
} ledarray_auto_refresh_mode_t; //

// 初始化时设置的默认屏幕刷新模式
#define LEDARRAY_REFRESH_INIT_MODE (LEDARRAY_AUTO_REFRESH_ALL_ONCE)

// 刷新锁，外部需要抢到并完成所有操作后释放以允许屏幕刷新
extern xSemaphoreHandle refresh_ledarray_task_mutex; 

// 数字 0-9
extern const uint8_t matrix_1[7];
extern const uint8_t matrix_2[7];
extern const uint8_t matrix_3[7];
extern const uint8_t matrix_4[7];
extern const uint8_t matrix_5[7];
extern const uint8_t matrix_6[7];
extern const uint8_t matrix_7[7];
extern const uint8_t matrix_8[7];
extern const uint8_t matrix_9[7];

void ledarray_set_auto_refresh_mode(ledarray_auto_refresh_mode_t mode);

// 以下函数将数据存储到缓冲区，不包含发送

// separation_draw三色分离方式
// 取模适配PCtoLCD2002
// 取模说明：从第一行开始向右每取8个点作为一个字节，如果最后不足8个点就补满8位。
// 取模顺序是从高到低，即第一个点作为最高位。如*-------取为10000000

esp_err_t separation_draw(int x, int y, uint64_t breadth, const uint8_t *p,
                          uint64_t byte_number, uint8_t in_color[3]);

// direct_draw彩色图像直显方式
// 取模方式适配Img2Lcd
// 水平扫描，从左到右，从顶到底扫描，24位真彩（RGB顺序），需要图像数据头
// 图像编辑可以用系统自带的画板工具，像素调到合适值如12x24,不显示的地方要填充黑色
// 选择带数据头的图案数据，长宽会自动获取

esp_err_t direct_draw(int x, int y, const uint8_t *p);

/// 清除屏幕上的所有图案以及数据缓存
void clean_all_draw_buf();

void clean_draw_buf(int y);

void progress_draw_buf(int y, uint8_t step, uint8_t *color);

uint8_t *rectangle(int32_t breadth, int32_t height);

uint64_t matrix_size(uint8_t *matrix_data);

void print_number(int x, int y, int8_t figure, uint8_t color[3]);

void font_roll_print_12x(int x, int y, uint8_t color[3],
                         cartoon_handle_t cartoon_handle, char *format, ...);

void font_raw_print_12x(int x, int y, uint8_t color[3], char *format, ...);

void font_roll_print_16x(int x, int y, uint8_t color[3],
                         cartoon_handle_t cartoon_handle, char *format, ...);

void font_raw_print_16x(int x, int y, uint8_t color[3], char *format, ...);

esp_err_t ledarray_init();

esp_err_t ledarray_deinit();

esp_err_t ledarray_show_frame();

void color_input(int x, int y, uint8_t *data);

void color_output(int x, int y, uint8_t *data);
