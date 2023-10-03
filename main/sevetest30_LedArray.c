// 该文件由701Enti编写，包含WS2812构成的LED阵列的图形与显示处理，不包含WS2812底层驱动程序
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体不包含WS2812硬件驱动代码，而是参考Espressif官方提供的led_strip例程文件同时还使用了源文件中的hsv到rgb的转换函数,非常感谢，以下是源文件的声明

    /* RMT example -- RGB LED Strip

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
    */

// 官方例程连接：https://github.com/espressif/esp-idf/tree/release/v4.4/examples/common_components/led_strip
// 官方文档链接：https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.4/esp32/api-reference/peripherals/rmt.html
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.7.13

#include "sevetest30_LedArray.h"

#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

#include <esp_log.h>
#include "driver/rmt.h"

//这是例程中使用的编码库，通过根目录CMakeLists添加 set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/led_strip) 来链接
#include "led_strip.h"



uint8_t black[3] = {0x00};
uint8_t compound_result[LINE_LED_NUMBER*3] = {0};


//一个图像可看作不同颜色的像素组合，而每个像素颜色可用红绿蓝三元色的深度（亮度）表示
//因此，我们可以将一个图像分离成三个单色图层，我们就定义三个8bit数组
//分别表示这三个图层中[12x24=288]像素R G B 三色的明暗程度，合成他们便可表示显示板上的图像
//但是考虑到硬件设计中，WS2812是一排24灯为最小操作单位写入，所以每个图层最好分割成12排
//最后，我们选择以排为单位，3个图层，每个图层12排，共3x12=36个数组，这里称为缓冲区


//SE30中VERTICAL_LED_NUMBER=12 如果您希望扩展屏幕纵向长度，务必增加足够变量 red_y(x) green_y(x) blue_y(x)
uint8_t red_y1[LINE_LED_NUMBER]  = {0x00};
uint8_t red_y2[LINE_LED_NUMBER]  = {0x00};
uint8_t red_y3[LINE_LED_NUMBER]  = {0x00};
uint8_t red_y4[LINE_LED_NUMBER]  = {0x00};
uint8_t red_y5[LINE_LED_NUMBER]  = {0x00};
uint8_t red_y6[LINE_LED_NUMBER]  = {0x00};
uint8_t red_y7[LINE_LED_NUMBER]  = {0x00};
uint8_t red_y8[LINE_LED_NUMBER]  = {0x00};
uint8_t red_y9[LINE_LED_NUMBER]  = {0x00};
uint8_t red_y10[LINE_LED_NUMBER] = {0x00};
uint8_t red_y11[LINE_LED_NUMBER] = {0x00};
uint8_t red_y12[LINE_LED_NUMBER] = {0x00};

uint8_t green_y1[LINE_LED_NUMBER]   = {0x00};
uint8_t green_y2[LINE_LED_NUMBER]   = {0x00};
uint8_t green_y3[LINE_LED_NUMBER]   = {0x00};
uint8_t green_y4[LINE_LED_NUMBER]   = {0x00};
uint8_t green_y5[LINE_LED_NUMBER]   = {0x00};
uint8_t green_y6[LINE_LED_NUMBER]   = {0x00};
uint8_t green_y7[LINE_LED_NUMBER]   = {0x00};
uint8_t green_y8[LINE_LED_NUMBER]   = {0x00};
uint8_t green_y9[LINE_LED_NUMBER]   = {0x00};
uint8_t green_y10[LINE_LED_NUMBER]  = {0x00};
uint8_t green_y11[LINE_LED_NUMBER]  = {0x00};
uint8_t green_y12[LINE_LED_NUMBER]  = {0x00};

uint8_t blue_y1[LINE_LED_NUMBER]  = {0x00};
uint8_t blue_y2[LINE_LED_NUMBER]  = {0x00};
uint8_t blue_y3[LINE_LED_NUMBER]  = {0x00};
uint8_t blue_y4[LINE_LED_NUMBER]  = {0x00};
uint8_t blue_y5[LINE_LED_NUMBER]  = {0x00};
uint8_t blue_y6[LINE_LED_NUMBER]  = {0x00};
uint8_t blue_y7[LINE_LED_NUMBER]  = {0x00};
uint8_t blue_y8[LINE_LED_NUMBER]  = {0x00};
uint8_t blue_y9[LINE_LED_NUMBER]  = {0x00};
uint8_t blue_y10[LINE_LED_NUMBER] = {0x00};
uint8_t blue_y11[LINE_LED_NUMBER] = {0x00};
uint8_t blue_y12[LINE_LED_NUMBER] = {0x00};


//sevetest30支持两种显示解析 三色分离方式 和 彩色图像直显方式
//前者如上面的思路，将彩色图像分成三个图层处理，便于图形变换
//后者直接读取存储了RGB数据的数组，直接写入WS2812,便于显示复杂，颜色丰富的图形

//三色分离方式 取模适配PCtoLCD2002
//取模说明：从第一行开始向右每取8个点作为一个字节，如果最后不足8个点就补满8位。
//取模顺序是从高到低，即第一个点作为最高位。如*-------取为10000000
//以下是生成的基本单色字库，支持数字（4x7）,汉字（12x12）,字母（5x8）


//数字 0-9
const uint8_t matrix_0 [7] = {0xF0,0x90,0x90,0x90,0x90,0x90,0xF0};
const uint8_t matrix_1 [7] = {0x20,0x20,0x20,0x20,0x20,0x20,0x20};
const uint8_t matrix_2 [7] = {0xF0,0x10,0x10,0xF0,0x80,0x80,0xF0};
const uint8_t matrix_3 [7] = {0xF0,0x10,0x10,0xF0,0x10,0x10,0xF0};
const uint8_t matrix_4 [7] = {0x10,0x30,0x50,0xF0,0x10,0x10,0x10};
const uint8_t matrix_5 [7] = {0xF0,0x80,0x80,0xF0,0x10,0x10,0xF0};
const uint8_t matrix_6 [7] = {0xF0,0x80,0x80,0xF0,0x90,0x90,0xF0};
const uint8_t matrix_7 [7] = {0xF0,0x10,0x10,0x10,0x10,0x10,0x10};
const uint8_t matrix_8 [7] = {0xF0,0x90,0x90,0xF0,0x90,0x90,0xF0};
const uint8_t matrix_9 [7] = {0xF0,0x90,0x90,0xF0,0x10,0x10,0xF0};

//大写字母 A-Z

//小写字母 a-z

//基本汉字（宋体）


//彩色图像直显方式 取模方式适配Img2Lcd
//水平扫描，从左到右，从顶到底扫描，24位真彩（RGB顺序），需要图像数据头
//以下是彩色图案数据，仅供彩色图像直显方式，
//图像编辑可以用系统自带的画板工具，像素调到合适值

//24x12图像
const uint8_t sign_701[872] = { 0X00,0X18,0X18,0X00,0X0C,0X00,0X00,0X1B,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0XA2,0XE8,0X10,0XA9,0XEA,0X00,0XA3,0XE8,0X00,0XA2,0XE8,0X00,0X00,0X00,0XFF,
0X7F,0X27,0XFF,0X7F,0X27,0XFF,0X7F,0X27,0XFF,0X7F,0X27,0X00,0X00,0X00,0XFF,0XC9,
0X0E,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0XA2,0XE8,0X00,0X00,0X00,0XFF,0X7F,0X27,0X00,0X00,0X00,0X00,0X00,0X00,
0XFF,0X7F,0X27,0X00,0X00,0X00,0XFF,0XC9,0X0E,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0XA2,0XE8,0X00,0X00,0X00,0XFF,
0X7F,0X27,0X00,0X00,0X00,0X00,0X00,0X00,0XFF,0X7F,0X27,0X00,0X00,0X00,0XFF,0XC9,
0X0E,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0XA2,0XE8,0X00,0X00,0X00,0XFF,0X7F,0X27,0X00,0X00,0X00,0X00,0X00,0X00,
0XFF,0X7F,0X27,0X00,0X00,0X00,0XFF,0XC9,0X0E,0X00,0X00,0X00,0XFF,0XFF,0XFF,0XFF,
0XFF,0XFF,0XFF,0XFF,0XFF,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0XFF,0XFF,0XFF,0X00,0X00,0X00,0X00,0X00,0X00,0XFF,0XFF,0XFF,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0XA2,0XE8,0X00,0X00,0X00,0XFF,
0X7F,0X27,0X00,0X00,0X00,0X00,0X00,0X00,0XFF,0X7F,0X27,0X00,0X00,0X00,0XFF,0XC9,
0X0E,0X00,0X00,0X00,0XFF,0XFF,0XFF,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0XFF,0XFF,0XFF,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0XA2,0XE8,0X00,0X00,0X00,0XFF,0X7F,0X27,0X00,0X00,0X00,0X00,0X00,0X00,
0XFF,0X7F,0X27,0X00,0X00,0X00,0XFF,0XC9,0X0E,0X00,0X00,0X00,0XFF,0XFF,0XFF,0XFF,
0XFF,0XFF,0XFF,0XFF,0XFF,0X00,0X00,0X00,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,
0XFF,0X00,0X00,0X00,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0X00,0X00,0X00,0XFF,0XFF,0XFF,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0XA2,0XE8,0X00,0X00,0X00,0XFF,
0X7F,0X27,0X00,0X00,0X00,0X00,0X00,0X00,0XFF,0X7F,0X27,0X00,0X00,0X00,0XFF,0XC9,
0X0E,0X00,0X00,0X00,0XFF,0XFF,0XFF,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0XFF,0XFF,0XFF,0X00,0X00,0X00,0XFF,0XFF,0XFF,0X00,0X00,0X00,0XFF,0XFF,0XFF,0X00,
0X00,0X00,0X00,0X00,0X00,0XFF,0XFF,0XFF,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0XA2,0XE8,0X00,0X00,0X00,0XFF,0X7F,0X27,0XFF,0X7F,0X27,0XFF,0X7F,0X27,
0XFF,0X7F,0X27,0X00,0X00,0X00,0XFF,0XC9,0X0E,0X00,0X00,0X00,0XFF,0XFF,0XFF,0XFF,
0XFF,0XFF,0XFF,0XFF,0XFF,0X00,0X00,0X00,0XFF,0XFF,0XFF,0X00,0X00,0X00,0XFF,0XFF,
0XFF,0X00,0X00,0X00,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0X00,0X00,0X00,0XFF,0XFF,0XFF,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
};

const uint8_t sign_se30[872] = { 0X00,0X18,0X18,0X00,0X0C,0X00,0X00,0X1B,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0XFF,0X7F,0X27,0XFF,0X7F,
0X27,0XFF,0X7F,0X27,0X00,0X00,0X00,0X00,0XA2,0XE8,0X00,0XA2,0XE8,0X00,0XA2,0XE8,
0X00,0X00,0X00,0XFF,0XC9,0X0E,0XFF,0XC9,0X0E,0XFF,0XC9,0X0E,0X00,0X00,0X00,0XFF,
0XF2,0X00,0XFF,0XF2,0X00,0XFF,0XF2,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X3F,0X48,
0XCC,0X00,0X00,0X00,0X3F,0X48,0XCC,0X3F,0X48,0XCC,0X3F,0X48,0XCC,0X00,0X00,0X00,
0X00,0X00,0X00,0XFF,0X7F,0X27,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0XA2,0XE8,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0XFF,0XC9,0X0E,0X00,0X00,0X00,0XFF,0XF2,0X00,0X00,0X00,0X00,0XFF,0XF2,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X3F,0X48,0XCC,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X3F,0X48,0XCC,0X00,0X00,0X00,0X00,0X00,0X00,0XFF,0X7F,0X27,0XFF,0X7F,
0X27,0XFF,0X7F,0X27,0X00,0X00,0X00,0X00,0XA2,0XE8,0X00,0XA2,0XE8,0X00,0XA2,0XE8,
0X00,0X00,0X00,0XFF,0XC9,0X0E,0XFF,0XC9,0X0E,0XFF,0XC9,0X0E,0X00,0X00,0X00,0XFF,
0XF2,0X00,0X00,0X00,0X00,0XFF,0XF2,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X3F,0X48,
0XCC,0X00,0X00,0X00,0X3F,0X48,0XCC,0X00,0X00,0X00,0X3F,0X48,0XCC,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0XFF,0X7F,0X27,0X00,0X00,0X00,0X00,
0XA2,0XE8,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0XFF,0XC9,0X0E,0X00,0X00,0X00,0XFF,0XF2,0X00,0X00,0X00,0X00,0XFF,0XF2,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X3F,0X48,0XCC,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0XFF,0X7F,0X27,0XFF,0X7F,
0X27,0XFF,0X7F,0X27,0X00,0X00,0X00,0X00,0XA2,0XE8,0X00,0XA2,0XE8,0X00,0XA2,0XE8,
0X00,0X00,0X00,0XFF,0XC9,0X0E,0XFF,0XC9,0X0E,0XFF,0XC9,0X0E,0X00,0X00,0X00,0XFF,
0XF2,0X00,0XFF,0XF2,0X00,0XFF,0XF2,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X3F,0X48,
0XCC,0X3F,0X48,0XCC,0X3F,0X48,0XCC,0X3F,0X48,0XCC,0X3F,0X48,0XCC,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
};

//SE30中VERTICAL_LED_NUMBER=12 如果您希望扩展屏幕纵向长度，或者切换每行WS2812数据传输IO口，务必修改这个数组内的值满足需求
const int ledarray_gpio_info[VERTICAL_LED_NUMBER] = {4, 5, 6, 7, 17, 18, 8, 38, 39, 40, 41, 42}; // ws2812数据线连接的GPIO信息 第一行 到 最后一行

// 以下函数将数据存储到缓冲区，不包含发送
// 三色分离方式 起始坐标xy,图案横向宽度值,应该等于字模实际宽度（已定义的：FIGURE-数字 LETTER-字母 CHINESE-汉字），
// 导入字模，sizeof()获取数据长度，颜色RGB
// change控制显示亮度，0-100%
// 为了支持动画效果，允许起始坐标可以没有范围甚至为负数
void separation_draw(int16_t x, int16_t y, uint8_t breadth,uint8_t *p, uint8_t byte_number, uint8_t in_color[3],uint8_t change)
{
	uint8_t Dx = 0, Dy = 0; // xy的增加量
	uint8_t dat = 0x00;		// 临时数据存储
	uint8_t i = 0;			// 临时变量i
	int16_t sx = 0;			// 临时存储选定的横坐标
	bool flag = 0;			// 该像素是否需要点亮
	if (p == NULL){
		ESP_LOGI("separation_draw","输入了无法处理的空指针");
		return;
	}
    
	uint8_t color[3] = {in_color[0],in_color[1],in_color[2]};//因为数组本质也是指针，所以下级改动，上级数据也会破坏，所以需要隔离
    ledarray_intensity_change(&color[0],&color[1],&color[2],change);//亮度调制

	p--;					// 地址初始补偿
	while (byte_number)
	{
		p++; // 地址被动偏移
		for (i = 0; i <= 7; i++)
		{
			// 数据解析
			dat = *p;	// 读取数据 
			flag = (dat << i) & 0x80 ; // 位移取出一个bit数据，flag显示了选定的像素要不要点亮	

			// 存储到缓冲区
			sx = x + Dx - 1;
         
			if (flag) 
			color_input(sx, y + Dy, color);
			else
			color_input(sx, y + Dy, black);	 
				

			if (Dx == breadth-1)
			{
				Dx = 0; // 横向写入最后一个像素完毕，回车
				Dy++;	// 横向写入最后一个像素完毕，回车
				i = 8;	// 横向写入最后一个像素完毕，强制退出，等待地址偏移
			}
			else
				Dx++; // 确定写入完成一个像素
		}
		byte_number--; // 一个字节写入完成
	}
}

// 彩色图像直显方式 起始坐标xy 导入字模(请选择带数据头的图案数据，长宽会自动获取)
//change控制显示亮度，0-100%
// 为了支持动画效果，允许起始坐标可以没有范围甚至为负数
void direct_draw(int16_t x, int16_t y,uint8_t *p,uint8_t change)
{
	uint8_t Dx = 0, Dy = 0;				  // xy的增加量
	uint8_t *pT1 = p, *pT2 = p, *pT3 = p; // 临时指针
	int16_t sx = 0;						  // 临时存储选定的横坐标
	if (p == NULL){
		ESP_LOGI("direct_draw","输入了无法处理的空指针");
		return;
	}
	// 获取图案长宽数据
	uint8_t length = 0, breadth = 0; // 长宽信息
	uint8_t dat[4] = {0x00};		 // 临时数据存储
	p += 0x02;						 // 偏移到长宽数据区
	for (uint8_t i = 0; i < 4; i++)
	{
		dat[i] = *p;
		p++;
	}
	breadth = (dat[1] << 8) | dat[0];
	length =  (dat[3] << 8) | dat[2];
	// 图像解析
	p += 0x02;				   // 偏移到图像数据区
	uint8_t color[3] = {0x00}; // 临时数据存储
	while (length)
	{
		//获取颜色数据
		pT1 = p;
		pT2 = p + 0x01;
		pT3 = p + 0x02;
		color[0] = *pT1;
		color[1] = *pT2;
		color[2] = *pT3;
       
        ledarray_intensity_change(&color[0],&color[1],&color[2],change);
		
		sx = x + Dx - 1;
		color_input(sx, y + Dy, color);

		if (Dx == breadth-1)
		{
			Dx = 0; // 横向写入最后一个像素完毕，回车
			Dy++;	// 横向写入最后一个像素完毕，回车
			length--;
		}
		else
			Dx++;
		p += 0x03; // 地址被动偏移
	}
}

//  生成一个矩形字模,横向长度1-LINE_LED_NUMBER,纵向长度1-VERTICAL_LED_NUMBER
// 返回指针指向数据区首地址，可以直接当字模用，
// p-0x01 指向 entire_byte_num 的值，用于支持显示函数，表示总数据字节数
uint8_t *rectangle(uint8_t breadth, uint8_t length)
{
	uint8_t  x_byte_num = 1,entire_byte_num = 1; // 横向字节个数，总数据有效字节个数（不包含entire_byte_num段）
	uint8_t  Dx = 0;//临时存储一下横向偏移长度，这只是用于计算。纵向偏移长度由绘制函数获取，不需要,
	bool   flag = 0;//即将写入的位数据值

	static uint8_t *p,*pT1;//主数据指针，临时数据存储指针,注意static,否则指针数据大有可能在函数结束后失效，即使它是返回值
	//进一法，最后不足8个点就补满8位。
	//因为ceil传入的是浮点数，全部提前转换，防止整型相除而向下取整，否则ceil在这里就没意义了
	x_byte_num = ceil(breadth * 1.0 / 8.0);
    entire_byte_num = sizeof(uint8_t)*x_byte_num*length;

    // //我们要的内存区域大小是一个变量，不好用数组，用内存分配，效果应该差不多,但是效率出奇的低，这是引用内存申请一般的弊端，没法用，并且malloc并不被认可是稳定安全的方式
    // p = (uint8_t*)malloc(entire_byte_num+1);
    // if (p == NULL)return p;//得到空指针，申请失败，结束函数
	// pT1 = p + 0x01;//存储一下获取到数据的起始地址
    // memset(p,0x00,entire_byte_num+1);//初始化一波
    // *p = entire_byte_num;

    //这种情况索性直接用个数足够大的数组，不仅无需关心free问题，速度可以快几倍，因为数组也等效于一个指针，使用非常轻松.总字节entire_byte_num还是可以被显示函数识别，不会产生多加载数组空白区域的显示问题
	static uint8_t rectangle_data[RECTANGLE_SIZE_MAX] =  {0x00};
	for (int i=0;i<RECTANGLE_SIZE_MAX;i++) 
	    rectangle_data[i] = 0x00;//为使用过的rectangle_data全部置零
	p =  rectangle_data;
	pT1 = p + 0x01;//获取到数据的起始地址
	*p = entire_byte_num;//装载entire_byte_num，其实就是rectangle_data[0] = entire_byte_num;


	// 先进行全图填充
	flag = 1;
	for (uint8_t i = 0; i < entire_byte_num; i++)
	{
      p++;//地址被动偏移 ，第一次执行就是跳到总数据有效字节
	 
	  if (p-pT1 > entire_byte_num)return NULL;//越界退出，当然这个可能性极小
      for(uint8_t j=0;j<8;j++){
	   *p  |= flag << (7 - j);//写入
       if (Dx == breadth-1) {
		Dx = 0;
        j = 8;//一行写完强制退出，写下一个，实际就是回车，因为下一个字节就是下一行的了
	   }
	   else 
	   Dx++;
	  } 
	 
	}
	return pT1;//返回指针指向数据区首地址，可以直接当字模用，但是用完要释放内存 假设p是调用是产生的，p = rectangle(xxxxxx);用完之后要free(p);
}

//打印函数，但是只能是单一字符或数字
//一样为了支持动画效果，允许起始坐标可以没有范围甚至为负数

//显示一个数字，起始坐标xy,输入整型0-9数字，不支持负数，颜色color,亮度0-100%
void print_number(int16_t x,int16_t y,int8_t figure,uint8_t color[3],uint8_t change){
 uint8_t *p = NULL;
 //将p指向对应数字字模
 switch(figure){
	case 0:
	p = matrix_0;
	break;

	case 1:
	p = matrix_1;
	break;

	case 2:
	p = matrix_2;
	break;
	
	case 3:
	p = matrix_3;
	break;
	
	case 4:
	p = matrix_4;
	break;
	
	
	case 5:
	p = matrix_5;
	break;
	
	
	case 6:
	p = matrix_6;
	break;
	
	
	case 7:
	p = matrix_7;
	break;
	
	
	case 8:
	p = matrix_8;
	break;
	
	
	case 9:
	p = matrix_9;
	break;

 }
 if (p == NULL){
		ESP_LOGI("print_number","输入了0-9之外的数字");
		return;
  }
  separation_draw(x,y,FIGURE,p,sizeof(matrix_7),color,change);//因为，数字字模数据大小一样，随便输入一个字模就可以
}



// 颜色导入(x为绝对坐标值，0 到 LINE_LED_NUMBER-1)
//SE30中VERTICAL_LED_NUMBER=12 如果您希望扩展屏幕纵向长度，务必修改这个函数
void color_input(int8_t x, int8_t y, uint8_t dat[3])
{
	if (x<0||x>LINE_LED_NUMBER)return;//不在显示范围退出即可，但允许在范围外但不报告
	switch (y)
	{
	case 1:
		red_y1[x] = dat[0];
		green_y1[x] = dat[1];
		blue_y1[x] = dat[2];
		break;

	case 2:
		red_y2[x] = dat[0];
		green_y2[x] = dat[1];
		blue_y2[x] = dat[2];
		break;

	case 3:
		red_y3[x] = dat[0];
		green_y3[x] = dat[1];
		blue_y3[x] = dat[2];
		break;

	case 4:
		red_y4[x] = dat[0];
		green_y4[x] = dat[1];
		blue_y4[x] = dat[2];
		break;

	case 5:
		red_y5[x] = dat[0];
		green_y5[x] = dat[1];
		blue_y5[x] = dat[2];
		break;

	case 6:
		red_y6[x] = dat[0];
		green_y6[x] = dat[1];
		blue_y6[x] = dat[2];
		break;

	case 7:
		red_y7[x] = dat[0];
		green_y7[x] = dat[1];
		blue_y7[x] = dat[2];
		break;

	case 8:
		red_y8[x] = dat[0];
		green_y8[x] = dat[1];
		blue_y8[x] = dat[2];
		break;

	case 9:
		red_y9[x] = dat[0];
		green_y9[x] = dat[1];
		blue_y9[x] = dat[2];
		break;

	case 10:
		red_y10[x] = dat[0];
		green_y10[x] = dat[1];
		blue_y10[x] = dat[2];
		break;

	case 11:
		red_y11[x] = dat[0];
		green_y11[x] = dat[1];
		blue_y11[x] = dat[2];
		break;

	case 12:
		red_y12[x] = dat[0];
		green_y12[x] = dat[1];
		blue_y12[x] = dat[2];
		break;

	default:
		break;
	}
}
//颜色数据合成,线路选择1-VERTICAL_LED_NUMBER 
//SE30中VERTICAL_LED_NUMBER=12 如果您希望扩展屏幕纵向长度，务必修改这个函数
//将R：red_y(x) G:green_y(x) B:blue_y(x) 合成为 为WS2812发送的数据
void color_compound(uint8_t line_sw)
{
	uint8_t i = 0;
	//初始化
	uint8_t* red=NULL;
	uint8_t* green=NULL;
	uint8_t* blue=NULL;
	switch (line_sw)
	{
	case 1:
		red = red_y1;
		green = green_y1;
		blue = blue_y1;
		break;
	case 2:
		red = red_y2;
		green = green_y2;
		blue = blue_y2;
		break;
	case 3:
		red = red_y3;
		green = green_y3;
		blue = blue_y3;
		break;
	case 4:
		red = red_y4;
		green = green_y4;
		blue = blue_y4;
		break;
	case 5:
		red = red_y5;
		green = green_y5;
		blue = blue_y5;
		break;
	case 6:
		red = red_y6;
		green = green_y6;
		blue = blue_y6;
		break;
	case 7:
		red = red_y7;
		green = green_y7;
		blue = blue_y7;
		break;
	case 8:
		red = red_y8;
		green = green_y8;
		blue = blue_y8;
		break;
	case 9:
		red = red_y9;
		green = green_y9;
		blue = blue_y9;
		break;
	case 10:
		red = red_y10;
		green = green_y10;
		blue = blue_y10;
		break;
	case 11:
		red = red_y11;
		green = green_y11;
		blue = blue_y11;
		break;
	case 12:
		red = red_y12;
		green = green_y12;
		blue = blue_y12;
		break;
	default:
		break;
	}
    if (red==NULL||green==NULL||blue==NULL)return;
    //填充数据
	for (i=0;i<LINE_LED_NUMBER;i++){
	  //GRB顺序
	  compound_result[i*3+0] = *green;
	  compound_result[i*3+1] = *red;
	  compound_result[i*3+2] = *blue;
      red++,green++,blue++;//地址偏移
	}
}

//灯板阵列配置，自动根据group_sw进行0和1通道一同分组切换，group_sw表示组别，0-5
//为每两行分组是为支持压缩显示提高效率
void ledarray_set_and_write(uint8_t group_sw){
	if(group_sw>VERTICAL_LED_NUMBER/2-1)return;
  
    rmt_config_t config0 = RMT_DEFAULT_CONFIG_TX(ledarray_gpio_info[group_sw*2+0], 0); // 使用默认通道配置模板，通道0
    rmt_config_t config1 = RMT_DEFAULT_CONFIG_TX(ledarray_gpio_info[group_sw*2+1], 1); // 使用默认通道配置模板，通道1，
	config0.clk_div = 2;  //修改配置模板成员，设定计数器频率到40MHz，如果频率不适配，是无法运行的
	config1.clk_div = 2;
	rmt_config(&config0); //传输配置参数
	rmt_config(&config1);
    //控制器安装  （通道选择，接收内存块数量（发送模式使用0个），中断标识）
	rmt_driver_install(config0.channel, 0, 0); 
	rmt_driver_install(config1.channel, 0, 0);

    //安装 ws2812控制
	led_strip_config_t strip_config0=LED_STRIP_DEFAULT_CONFIG(LINE_LED_NUMBER,(led_strip_dev_t)config0.channel);
    led_strip_config_t strip_config1=LED_STRIP_DEFAULT_CONFIG(LINE_LED_NUMBER,(led_strip_dev_t)config1.channel);
    led_strip_t *strip0=led_strip_new_rmt_ws2812(&strip_config0);
    led_strip_t *strip1=led_strip_new_rmt_ws2812(&strip_config1);
  
	color_compound(group_sw*2+1);//合成通道0数据  
	for(uint8_t j=0;j<LINE_LED_NUMBER*3;j+=3)
	    strip0->set_pixel(strip0,j/3,compound_result[j+1],compound_result[j+0],compound_result[j+2]);//写入 数据存放地址 数据索引 R G B
    
    color_compound(group_sw*2+2);//合成通道1数据
    for(uint8_t j=0;j<LINE_LED_NUMBER*3;j+=3)
		strip1->set_pixel(strip1,j/3,compound_result[j+1],compound_result[j+0],compound_result[j+2]);//写入 数据存放地址 数据索引 R G B

	strip0->refresh(strip0, 100);//刷新	（灯带选择 超时时间）
	strip1->refresh(strip1, 100); 

    gpio_reset_pin(ledarray_gpio_info[group_sw*2+0]);
    gpio_reset_pin(ledarray_gpio_info[group_sw*2+1]);

    //删除通道准备下次选择发送
    rmt_driver_uninstall(config0.channel);
    rmt_driver_uninstall(config1.channel);
}

//RGB亮度调制  导入r g b数值地址+亮度
void ledarray_intensity_change(uint8_t *r,uint8_t *g,uint8_t *b,uint8_t intensity){
  //注意，RGB和HSV的取值范围并不一致，标准定义是 R G B 为 0-255  H 为 0-360 S V 为0-1（为了方便计算，这里 S V 映射到 0-100）  
  
  if (*r == 0 && *g == 0 && *b == 0)
    return;//	倘若 r g b 三个分量值都是0，显然主观上不需要变换，增加明度只会干扰数值

  if (intensity>100){
	ESP_LOGI("ledarray_intensity_change","错误的亮度数值 %d",intensity);
	*r = 0x00;
	*g = 0x00;
	*b = 0x00;
	return;
  }
  uint32_t h=0,s=0,v=0;
  rgb_to_hvs(*r,*g,*b,&h,&s,&v);
    v = intensity;
  led_strip_hsv2rgb(h,s,v,(uint32_t *)r,(uint32_t *)g,(uint32_t *)b);
}

//因为max和min好像是c++的，这里手写一个，效果一样
//取三个double元素最大的那个
double value_max(double value1,double value2,double value3){
  double buffer[3] = {value1,value2,value3};
  uint8_t a=0,b=0;
  for (a=0;a<2;a++){
	for (b=a+1;b<3;b++){
		if (buffer[a]>buffer[b]){
			double save = buffer[b];
			buffer[b] = buffer[a];
            buffer[a] = save;
		}
	}
  }
  return buffer[2];
}

//取三个double元素最小的那个
double value_min(double value1,double value2,double value3){
  double buffer[3] = {value1,value2,value3};
  uint8_t a=0,b=0;
  for (a=0;a<2;a++){
	for (b=a+1;b<3;b++){
		if (buffer[a]>buffer[b]){
			double save = buffer[b];
			buffer[b] = buffer[a];
            buffer[a] = save;
		}
	}
  }
  return buffer[0];
}


//注意，RGB和HSV的取值范围并不一致，标准定义是 R G B 为 0-255  H 为 0-360 S V 为0-1
//然而，为了方便计算，这里 S V 映射到 0-100
//将RGB转换到HSV颜色空间,计算方法是网上随便找的
void rgb_to_hvs(uint8_t red_buf, uint8_t green_buf, uint8_t blue_buf,uint32_t *p_h, uint32_t *p_s, uint32_t *p_v)
{
   //HSV需要浮点存储
   double h=0,s=0,v=0;

   //将RGB映射到0 - 1之间,并由浮点变量 r g b 存储
   double r=0,g=0,b=0;
   r = red_buf / 255.0;
   g = green_buf / 255.0;
   b = blue_buf / 255.0;

   //计算V
   v = value_max(r,g,b);

   //计算S
   if (v != 0){
	s = v - value_min(r,g,b);
	s = s / v;
   }
   else
    s = 0;

   //计算H
    if (v == r)
		h = 60 * (g - b) / (v - value_min(r,g,b));
    if (v == g)
	    h = 120 + 60 * (b - r) / (v - value_min(r,g,b));
	if (v == b)
	    h = 240 + 60 * (r - g) / (v - value_min(r,g,b));
	  
	if (h < 0)h = h + 360;

	//映射到需求范围 0 - 100
    s = s * 100;
	v = v * 100;


	static uint32_t out_h=0,out_s=0,out_v=0; 
	//类型转换，随便四舍五入一下
	out_h = ceil(h);
	out_s = ceil(s);
	out_v = ceil(v);
	//数据输出
    *p_h = out_h;
	*p_s = out_s;
	*p_v = out_v;
}


//以下函数来自ESP-IDFv4.4 led_strip.c 例程文件

/**
 * @brief Simple helper function, converting HSV color space to RGB color space
 *
 * Wiki: https://en.wikipedia.org/wiki/HSL_and_HSV
 *
 */
void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}



