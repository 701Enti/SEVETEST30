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

 // 包含WS2812构成的LED阵列的图形与显示处理，使用的WS2812底层驱动程序为官方组件
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // 敬告：文件本体不包含WS2812硬件驱动代码，而是参考ESP-IDF的例程文件,使用官方led_strip组件依赖,同时还使用了源文件中的hsv到rgb的转换函数,非常感谢
 // 绘制函数本身不会刷新屏幕,需要运行屏幕刷新,才会在屏幕上点亮
 // 绘制函数规定使用字模点阵的左上角的点作为其坐标表示点,LED的延伸方向为X轴正方向,沿线方向的灯排对应的gpio号在ledarray_gpio_info数组的先后顺序指代其在Y轴正向出现顺序,对应角标+1为其对应的Y轴坐标
 // 在标准SEVETEST30-LedArray板下,正视"701Enti"标志,此时坐标原点应为最左上角像素点,X轴正方向向右,Y轴正方向向下,原点坐标为(1,1)即填入函数 x = 1,y = 1
 // 本库WS2812使用硬件RMT驱动,占用两个RMT模块,由ESP-IDF内部驱动程序动态分配RMT模块,无需手动指定
 // 如果是12排分为6组(0-5),RMT输出引脚将在这些连接的GPIO之间来回切换,实现阵列的刷新,同时提供局部刷新服务模式提高刷新效率
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // github: https://github.com/701Enti
 // bilibili: 701Enti



#include <string.h>
#include "led_strip_types.h"
#include <sevetest30_UI.h>
#include "esp_types.h"

#ifndef _SEVETEST30_LEDARRAY_H_
#define _SEVETEST30_LEDARRAY_H_
#endif 

#define FIGURE_BREATH 4                    //数字的宽度 4x7

#define LEDARRAY_LED_MODEL LED_MODEL_WS2812 //选择LED的型号
#define LEDARRAY_LED_COLOR_COMPONENT_FMT LED_STRIP_COLOR_COMPONENT_FMT_GRB //LED的颜色格式
#define LEDARRAY_LED_INVERT_OUT (false)//将输出信号反向
#define LEDARRAY_LED_RMT_RESOLUTION_HZ (10*1000*1000)//RMT信号分辨率
#define LEDARRAY_LED_RMT_CLK_SRC RMT_CLK_SRC_DEFAULT//时钟源
#define LEDARRAY_LED_WITH_DMA (true) //传输时使用DMA提高效率
#define LEDARRAY_LED_RMT_MEM_BLOCK_SYMBOLS 64 //如果未启用DMA,表示专用内存块大小(至少64),如果启用DMA,表示最大化控制内部DMA缓冲区的大小



#define LINE_LED_NUMBER  24         //灯板横向长度(每一根通讯线连接的WS2812数量) 此处“横向”始终表示通讯线延伸方向
#define VERTICAL_LED_NUMBER 12      //灯板纵向长度(通讯线数量)                  此处“纵向”始终垂直通讯线延伸方向

#define RECTANGLE_MATRIX(pRECTANGLE) (pRECTANGLE+0x08) // 矩形字模数据区位置,需要填入矩形的位置
#define RECTANGLE_SIZE_MAX 36//矩形最大允许数据字节数，这决定矩形生成函数可以生成多大矩形，这里设定其为整个显示面板大小，假设希望最大大小是 12x25,25是矩形横向长度，计算25/8约为4（不足1就进1），得到12x4=48


#define FONT_CHIP_PRINT_NUM_MAX 128 //字库打印函数最大单次打印字符数
#define FONT_CHIP_PRINT_FMT_BUF_SIZE (FONT_CHIP_PRINT_NUM_MAX*10)//字库打印函数格式化缓存大小,缓存使用char类型(占一个字节),UTF-8最多用6个字节表达一个字符+预留


//屏幕刷新任务配置("ALL_ONCE" "ALL_MULTIPLE" "PART_ONCE" "PART_MULTIPLE")
#define LEDARRAY_REFRESH_TASK_CORE           (1)//屏幕刷新任务运行核心
#define LEDARRAY_REFRESH_TASK_PRIO           (1)//屏幕刷新任务优先级
#define LEDARRAY_REFRESH_TASK_STACK_SIZE     (1024 * 2)//屏幕刷新任务堆栈大小



typedef enum
{
    LEDARRAY_AUTO_REFRESH_DISABLE = 0,//禁用屏幕自动刷新
    LEDARRAY_AUTO_REFRESH_ALL_ONCE,//[单次全刷]一次性刷新整个屏幕所有行,全屏刷新之后才发生延时
    LEDARRAY_AUTO_REFRESH_ALL_MULTIPLE,//[多次全刷]分多步进地完成刷新整个屏幕所有行,每步之后发生延时
    LEDARRAY_AUTO_REFRESH_PART_ONCE,//[单次局刷]一次性刷新所有发生绘制活动的行,刷新之后才发生延时
    LEDARRAY_AUTO_REFRESH_PART_MULTIPLE,//[多次局刷]分多步进地完成刷新所有发生绘制活动的行,每步之后发生延时
}ledarray_auto_refresh_mode_t;//

#define LEDARRAY_REFRESH_INIT_MODE LEDARRAY_AUTO_REFRESH_ALL_ONCE //初始化时设置的默认屏幕刷新模式




//数字 0-9
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
//sevetest30支持两种显示解析 三色分离方式 和 彩色图像直显方式
//前者如上面的思路，将彩色图像分成三个图层处理，便于图形变换
//后者直接读取存储了RGB数据的数组，直接写入WS2812,便于显示复杂，颜色丰富的图形


//三色分离方式 
    //取模适配PCtoLCD2002
    //取模说明：从第一行开始向右每取8个点作为一个字节，如果最后不足8个点就补满8位。
    //取模顺序是从高到低，即第一个点作为最高位。如*-------取为10000000


void separation_draw(int x, int y, uint64_t breadth, const uint8_t* p, uint8_t byte_number, uint8_t in_color[3], uint8_t change);


//彩色图像直显方式 
   //取模方式适配Img2Lcd
   //水平扫描，从左到右，从顶到底扫描，24位真彩（RGB顺序），需要图像数据头
   //图像编辑可以用系统自带的画板工具，像素调到合适值如12x24,不显示的地方要填充黑色
   //选择带数据头的图案数据，长宽会自动获取


void direct_draw(int x, int y, const uint8_t* p, uint8_t change);


///清除屏幕上的所有图案以及数据缓存
void clean_draw();


void clean_draw_buf(int y);

void progress_draw_buf(int y, uint8_t step, uint8_t* color);

uint8_t* rectangle(int32_t breadth, int32_t length);

void print_number(int x, int y, int8_t figure, uint8_t color[3], uint8_t change);

void font_roll_print_12x(int x, int y, uint8_t color[3], uint8_t change, cartoon_handle_t cartoon_handle, char* format, ...);

void font_raw_print_12x(int x, int y, uint8_t color[3], uint8_t change, char* format, ...);

esp_err_t ledarray_init();

esp_err_t ledarray_deinit();

//以下函数处理上面的函数产生的颜色数据，配置传输数据发送


void color_compound(int line_num);

void ledarray_set_and_write(int group_num);


//以下是内部私有函数，一般不会在外部调用

void color_input(int x, int y, uint8_t* data);

void color_output(int x, int y, uint8_t* data);

double value_max(double value1, double value2, double value3);


double value_min(double value1, double value2, double value3);


void ledarray_intensity_change(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t intensity);

void rgb_to_hvs(uint8_t red_buf, uint8_t green_buf, uint8_t blue_buf, uint32_t* p_h, uint32_t* p_s, uint32_t* p_v);

//以下函数来自ESP-IDFv4.4 led_strip.c 例程文件，以及源文件声明

/**
 * @brief Simple helper function, converting HSV color space to RGB color space
 *
 * Wiki: https://en.wikipedia.org/wiki/HSL_and_HSV
 *
 */
void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t* r, uint32_t* g, uint32_t* b);



