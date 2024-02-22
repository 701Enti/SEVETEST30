// 该文件归属701Enti组织，由SEVETEST30开发团队维护，包含WS2812构成的LED阵列的图形与显示处理，不包含WS2812底层驱动程序
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体不包含WS2812硬件驱动代码，而是参考Espressif官方提供的led_strip例程文件同时还使用了源文件中的hsv到rgb的转换函数,非常感谢
// ESP-IDF项目地址 https://github.com/espressif/esp-idf
// 官方例程连接：https://github.com/espressif/esp-idf/tree/release/v4.4/examples/common_components/led_strip
// 官方文档链接：https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.4/esp32/api-reference/peripherals/rmt.html
// github: https://github.com/701Enti
// bilibili: 701Enti

#include "sevetest30_LedArray.h"
#include "led_strip.h"
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <esp_log.h>
#include "driver/rmt.h"
#include "hal/rmt_types.h"


uint8_t black[3] = {0x00};
uint8_t compound_result[LINE_LED_NUMBER * 3] = {0}; // 发送给WS2812的格式化数据缓存，GRB格式

// 一个图像可看作不同颜色的像素组合，而每个像素颜色可用红绿蓝三元色的深度（亮度）表示
// 因此，我们可以将一个图像分离成三个单色图层，我们就定义三个8bit数组
// 分别表示这三个图层中[12x24=288]像素R G B 三色的明暗程度，合成他们便可表示显示板上的图像
// 但是考虑到硬件设计中，WS2812是一排24灯为最小操作单位写入，所以每个图层最好分割成12排
// 最后，我们选择以排为单位，3个图层，每个图层12排，共3x12=36个数组，这里称为缓冲区

// SE30中VERTICAL_LED_NUMBER=12 如果您希望扩展屏幕纵向长度，务必增加足够变量 red_y(x) green_y(x) blue_y(x)
uint8_t red_y1[LINE_LED_NUMBER] = {0x00};
uint8_t red_y2[LINE_LED_NUMBER] = {0x00};
uint8_t red_y3[LINE_LED_NUMBER] = {0x00};
uint8_t red_y4[LINE_LED_NUMBER] = {0x00};
uint8_t red_y5[LINE_LED_NUMBER] = {0x00};
uint8_t red_y6[LINE_LED_NUMBER] = {0x00};
uint8_t red_y7[LINE_LED_NUMBER] = {0x00};
uint8_t red_y8[LINE_LED_NUMBER] = {0x00};
uint8_t red_y9[LINE_LED_NUMBER] = {0x00};
uint8_t red_y10[LINE_LED_NUMBER] = {0x00};
uint8_t red_y11[LINE_LED_NUMBER] = {0x00};
uint8_t red_y12[LINE_LED_NUMBER] = {0x00};

uint8_t green_y1[LINE_LED_NUMBER] = {0x00};
uint8_t green_y2[LINE_LED_NUMBER] = {0x00};
uint8_t green_y3[LINE_LED_NUMBER] = {0x00};
uint8_t green_y4[LINE_LED_NUMBER] = {0x00};
uint8_t green_y5[LINE_LED_NUMBER] = {0x00};
uint8_t green_y6[LINE_LED_NUMBER] = {0x00};
uint8_t green_y7[LINE_LED_NUMBER] = {0x00};
uint8_t green_y8[LINE_LED_NUMBER] = {0x00};
uint8_t green_y9[LINE_LED_NUMBER] = {0x00};
uint8_t green_y10[LINE_LED_NUMBER] = {0x00};
uint8_t green_y11[LINE_LED_NUMBER] = {0x00};
uint8_t green_y12[LINE_LED_NUMBER] = {0x00};

uint8_t blue_y1[LINE_LED_NUMBER] = {0x00};
uint8_t blue_y2[LINE_LED_NUMBER] = {0x00};
uint8_t blue_y3[LINE_LED_NUMBER] = {0x00};
uint8_t blue_y4[LINE_LED_NUMBER] = {0x00};
uint8_t blue_y5[LINE_LED_NUMBER] = {0x00};
uint8_t blue_y6[LINE_LED_NUMBER] = {0x00};
uint8_t blue_y7[LINE_LED_NUMBER] = {0x00};
uint8_t blue_y8[LINE_LED_NUMBER] = {0x00};
uint8_t blue_y9[LINE_LED_NUMBER] = {0x00};
uint8_t blue_y10[LINE_LED_NUMBER] = {0x00};
uint8_t blue_y11[LINE_LED_NUMBER] = {0x00};
uint8_t blue_y12[LINE_LED_NUMBER] = {0x00};

// sevetest30支持两种显示解析 三色分离方式 和 彩色图像直显方式
// 前者如上面的思路，将彩色图像分成三个图层处理，便于图形变换
// 后者直接读取存储了RGB数据的数组，直接写入WS2812,便于显示复杂，颜色丰富的图形

// 三色分离方式 取模适配PCtoLCD2002
// 取模说明：从第一行开始向右每取8个点作为一个字节，如果最后不足8个点就补满8位。
// 取模顺序是从高到低，即第一个点作为最高位。如*-------取为10000000
// 以下是生成的基本单色字库，支持数字（4x7）,汉字（12x12）,字母（5x8）

// 数字 0-9
const uint8_t matrix_0[7] = {0xF0, 0x90, 0x90, 0x90, 0x90, 0x90, 0xF0};
const uint8_t matrix_1[7] = {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};
const uint8_t matrix_2[7] = {0xF0, 0x10, 0x10, 0xF0, 0x80, 0x80, 0xF0};
const uint8_t matrix_3[7] = {0xF0, 0x10, 0x10, 0xF0, 0x10, 0x10, 0xF0};
const uint8_t matrix_4[7] = {0x10, 0x30, 0x50, 0xF0, 0x10, 0x10, 0x10};
const uint8_t matrix_5[7] = {0xF0, 0x80, 0x80, 0xF0, 0x10, 0x10, 0xF0};
const uint8_t matrix_6[7] = {0xF0, 0x80, 0x80, 0xF0, 0x90, 0x90, 0xF0};
const uint8_t matrix_7[7] = {0xF0, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10};
const uint8_t matrix_8[7] = {0xF0, 0x90, 0x90, 0xF0, 0x90, 0x90, 0xF0};
const uint8_t matrix_9[7] = {0xF0, 0x90, 0x90, 0xF0, 0x10, 0x10, 0xF0};

// 大写字母 A-Z

// 小写字母 a-z

// 基本汉字（宋体）

// 彩色图像直显方式 取模方式适配Img2Lcd
// 水平扫描，从左到右，从顶到底扫描，24位真彩（RGB顺序），需要图像数据头
// 以下是彩色图案数据，仅供彩色图像直显方式，
// 图像编辑可以用系统自带的画板工具，像素调到合适值

// 24x12图像
const uint8_t sign_701[872] = {
	0X00,
	0X18,
	0X18,
	0X00,
	0X0C,
	0X00,
	0X00,
	0X1B,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XA2,
	0XE8,
	0X10,
	0XA9,
	0XEA,
	0X00,
	0XA3,
	0XE8,
	0X00,
	0XA2,
	0XE8,
	0X00,
	0X00,
	0X00,
	0XFF,
	0X7F,
	0X27,
	0XFF,
	0X7F,
	0X27,
	0XFF,
	0X7F,
	0X27,
	0XFF,
	0X7F,
	0X27,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XC9,
	0X0E,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XA2,
	0XE8,
	0X00,
	0X00,
	0X00,
	0XFF,
	0X7F,
	0X27,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XFF,
	0X7F,
	0X27,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XC9,
	0X0E,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XA2,
	0XE8,
	0X00,
	0X00,
	0X00,
	0XFF,
	0X7F,
	0X27,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XFF,
	0X7F,
	0X27,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XC9,
	0X0E,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XA2,
	0XE8,
	0X00,
	0X00,
	0X00,
	0XFF,
	0X7F,
	0X27,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XFF,
	0X7F,
	0X27,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XC9,
	0X0E,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XFF,
	0XFF,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XFF,
	0XFF,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XA2,
	0XE8,
	0X00,
	0X00,
	0X00,
	0XFF,
	0X7F,
	0X27,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XFF,
	0X7F,
	0X27,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XC9,
	0X0E,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XFF,
	0XFF,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XFF,
	0XFF,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XA2,
	0XE8,
	0X00,
	0X00,
	0X00,
	0XFF,
	0X7F,
	0X27,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XFF,
	0X7F,
	0X27,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XC9,
	0X0E,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XFF,
	0XFF,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XA2,
	0XE8,
	0X00,
	0X00,
	0X00,
	0XFF,
	0X7F,
	0X27,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XFF,
	0X7F,
	0X27,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XC9,
	0X0E,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XFF,
	0XFF,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XFF,
	0XFF,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XFF,
	0XFF,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XFF,
	0XFF,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XFF,
	0XFF,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XA2,
	0XE8,
	0X00,
	0X00,
	0X00,
	0XFF,
	0X7F,
	0X27,
	0XFF,
	0X7F,
	0X27,
	0XFF,
	0X7F,
	0X27,
	0XFF,
	0X7F,
	0X27,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XC9,
	0X0E,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XFF,
	0XFF,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XFF,
	0XFF,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0XFF,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XFF,
	0XFF,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
};

const uint8_t sign_se30[872] = {
	0X00,
	0X18,
	0X18,
	0X00,
	0X0C,
	0X00,
	0X00,
	0X1B,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XFF,
	0X7F,
	0X27,
	0XFF,
	0X7F,
	0X27,
	0XFF,
	0X7F,
	0X27,
	0X00,
	0X00,
	0X00,
	0X00,
	0XA2,
	0XE8,
	0X00,
	0XA2,
	0XE8,
	0X00,
	0XA2,
	0XE8,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XC9,
	0X0E,
	0XFF,
	0XC9,
	0X0E,
	0XFF,
	0XC9,
	0X0E,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XF2,
	0X00,
	0XFF,
	0XF2,
	0X00,
	0XFF,
	0XF2,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X3F,
	0X48,
	0XCC,
	0X00,
	0X00,
	0X00,
	0X3F,
	0X48,
	0XCC,
	0X3F,
	0X48,
	0XCC,
	0X3F,
	0X48,
	0XCC,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XFF,
	0X7F,
	0X27,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XA2,
	0XE8,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XC9,
	0X0E,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XF2,
	0X00,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XF2,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X3F,
	0X48,
	0XCC,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X3F,
	0X48,
	0XCC,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XFF,
	0X7F,
	0X27,
	0XFF,
	0X7F,
	0X27,
	0XFF,
	0X7F,
	0X27,
	0X00,
	0X00,
	0X00,
	0X00,
	0XA2,
	0XE8,
	0X00,
	0XA2,
	0XE8,
	0X00,
	0XA2,
	0XE8,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XC9,
	0X0E,
	0XFF,
	0XC9,
	0X0E,
	0XFF,
	0XC9,
	0X0E,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XF2,
	0X00,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XF2,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X3F,
	0X48,
	0XCC,
	0X00,
	0X00,
	0X00,
	0X3F,
	0X48,
	0XCC,
	0X00,
	0X00,
	0X00,
	0X3F,
	0X48,
	0XCC,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XFF,
	0X7F,
	0X27,
	0X00,
	0X00,
	0X00,
	0X00,
	0XA2,
	0XE8,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XC9,
	0X0E,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XF2,
	0X00,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XF2,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X3F,
	0X48,
	0XCC,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0XFF,
	0X7F,
	0X27,
	0XFF,
	0X7F,
	0X27,
	0XFF,
	0X7F,
	0X27,
	0X00,
	0X00,
	0X00,
	0X00,
	0XA2,
	0XE8,
	0X00,
	0XA2,
	0XE8,
	0X00,
	0XA2,
	0XE8,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XC9,
	0X0E,
	0XFF,
	0XC9,
	0X0E,
	0XFF,
	0XC9,
	0X0E,
	0X00,
	0X00,
	0X00,
	0XFF,
	0XF2,
	0X00,
	0XFF,
	0XF2,
	0X00,
	0XFF,
	0XF2,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X3F,
	0X48,
	0XCC,
	0X3F,
	0X48,
	0XCC,
	0X3F,
	0X48,
	0XCC,
	0X3F,
	0X48,
	0XCC,
	0X3F,
	0X48,
	0XCC,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
	0X00,
};

// SE30中VERTICAL_LED_NUMBER=12 如果您希望扩展屏幕纵向长度，或者切换每行WS2812数据传输IO口，务必修改这个数组内的值满足需求
const int ledarray_gpio_info[VERTICAL_LED_NUMBER] = {4, 5, 6, 7, 17, 18, 8, 42, 41, 40, 39, 38}; // ws2812数据线连接的GPIO信息 第一行 到 最后一行

rmt_config_t *rmt_cfg0;
rmt_config_t *rmt_cfg1;

led_strip_t *strip0 = NULL;
led_strip_t *strip1 = NULL;

/// @brief RGB三色分离方式绘制,只支持单色绘制
/// @brief 取模方式请参考头文件
/// @param x 图案横坐标(无范围限制，超出不显示)，灯板左上角设为原点（1，1），由左到右绘制
/// @param y 图案纵坐标(无范围限制，超出不显示)，灯板左上角设为原点（1，1），由上到下绘制
/// @param breadth 图案宽度（已定义的：FIGURE-数字 LETTER-字母 CHINESE-汉字）
/// @param p       导入字模指针
/// @param byte_number 总数据长度(Byte)
/// @param in_color 注入颜色 （RGB）
/// @param change   亮度调整（1-100）
void separation_draw(int16_t x, int16_t y, uint8_t breadth, const uint8_t *p, uint8_t byte_number, uint8_t in_color[3], uint8_t change)
{
	uint8_t Dx = 0, Dy = 0; // xy的增加量
	uint8_t dat = 0x00;		// 临时数据存储
	uint8_t i = 0;			// 临时变量i
	int16_t sx = 0;			// 临时存储选定的横坐标
	bool flag = 0;			// 该像素是否需要点亮
	if (p == NULL)
	{
		ESP_LOGE("separation_draw", "输入了无法处理的空指针");
		return;
	}

	uint8_t color[3] = {in_color[0], in_color[1], in_color[2]};			// 因为数组本质也是指针，所以下级改动，上级数据也会破坏，所以需要隔离
	ledarray_intensity_change(&color[0], &color[1], &color[2], change); // 亮度调制

	p--; // 地址初始补偿
	while (byte_number)
	{
		p++; // 地址被动偏移
		for (i = 0; i <= 7; i++)
		{
			// 数据解析
			dat = *p;				  // 读取数据
			flag = (dat << i) & 0x80; // 位移取出一个bit数据，flag显示了选定的像素要不要点亮

			// 存储到缓冲区
			sx = x + Dx - 1;

			if (flag)
				color_input(sx, y + Dy, color);
			else
				color_input(sx, y + Dy, black);

			if (Dx == breadth - 1)
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

//  起始坐标xy 导入字模(请选择带数据头的图案数据，长宽会自动获取)
// change控制显示亮度，0-100%
// 为了支持动画效果，允许起始坐标可以没有范围甚至为负数

/// @brief RGB彩色图像直显方式，自动获取图像头参数
/// @brief 取模方式请参考头文件
/// @param x 图案横坐标(无范围限制，超出不显示)，灯板左上角设为原点（1，1），由左到右绘制
/// @param y 图案纵坐标(无范围限制，超出不显示)，灯板左上角设为原点（1，1），由上到下绘制
/// @param p        导入图像指针
/// @param change   亮度调整（1-100）
void direct_draw(int16_t x, int16_t y, const uint8_t *p, uint8_t change)
{
	uint8_t Dx = 0, Dy = 0;				  // xy的增加量
	uint8_t *pT1 = p, *pT2 = p, *pT3 = p; // 临时指针
	int16_t sx = 0;						  // 临时存储选定的横坐标
	if (p == NULL)
	{
		ESP_LOGE("direct_draw", "输入了无法处理的空指针");
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
	length = (dat[3] << 8) | dat[2];
	// 图像解析
	p += 0x02;				   // 偏移到图像数据区
	uint8_t color[3] = {0x00}; // 临时数据存储
	while (length)
	{
		// 获取颜色数据
		pT1 = p;
		pT2 = p + 0x01;
		pT3 = p + 0x02;
		color[0] = *pT1;
		color[1] = *pT2;
		color[2] = *pT3;

		ledarray_intensity_change(&color[0], &color[1], &color[2], change);

		sx = x + Dx - 1;
		color_input(sx, y + Dy, color);

		if (Dx == breadth - 1)
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

/// @brief 清空指定行的缓存
/// @param y 指定行纵坐标
void clean_draw_buf(int8_t y)
{
	uint8_t dat[3] = {0};
	for (int i = 0; i < LINE_LED_NUMBER; i++)
		color_input(i, y, dat);
}

/// @brief 渐进指定行的缓存，使得缓存颜色向目标颜色以步进值偏移一步，这只会对绘制状态非活动的区域起作用
/// @param y 指定行纵坐标
/// @param step 步进值
/// @param color 目标颜色
void progress_draw_buf(int8_t y,float step, uint8_t *color)
{
	float dat[3] = {0};
	uint8_t dat_buf[3] = {0};
	for (int i = 0; i < LINE_LED_NUMBER; i++)
	{
		memset(dat_buf, 0, 3 * sizeof(uint8_t));
		color_output(i, y, dat_buf);
		for (int j = 0; j < 3; j++)
		{
			dat[j] = (float)dat_buf[j];

			if (color[j] >= 0 && color[j] <= 255)
			{
				if (dat[j] < color[j])
				{
					if (255 - dat[j] >= step)
						dat[j] += step;
					else
						dat[j] = color[j];
				}
				if (dat[j] > color[j])
				{
					if (dat[j] >= step)
						dat[j] -= step;
					else
						dat[j] = color[j];
				}
			}
			else
			{
				dat[j] = color[j];
			}

			dat_buf[j] = (uint8_t)dat[j];
		}
		color_input(i, y, dat_buf);
	}
}

/// @brief 生成一个矩形字模(需要释放)
/// @param breadth 矩形横向长度(1-LINE_LED_NUMBER)
/// @param length  矩形纵向长度(1-VERTICAL_LED_NUMBER)
/// @return 返回值 rectangle_data 为矩形数据地址 *rectangle_data 为 总数据大小（Byte） RECTANGLE_MATRIX(rectangle_data) 为 矩形字模
/// @return 例 返回值为p separation_draw(x,y,b,RECTANGLE_MATRIX(p),*p,color,change); free(p);
uint8_t *rectangle(int8_t breadth, int8_t length)
{
	if (breadth < 0 || length < 0)
		return NULL;

	uint8_t x_byte_num = 1, entire_byte_num = 1; // 横向字节个数，总数据有效字节个数（不包含entire_byte_num段）
	uint8_t Dx = 0;								 // 临时存储一下横向偏移长度，这只是用于计算。纵向偏移长度由绘制函数获取，不需要,
	bool flag = 0;								 // 即将写入的位数据值

	static uint8_t *pT1 = NULL;
	static uint8_t *p = NULL;

	// 进一法，最后不足8个点就补满8位。
	// 因为ceil传入的是浮点数，全部提前转换，防止整型相除而向下取整，否则ceil在这里就没意义了
	x_byte_num = ceil(breadth * 1.0 / 8.0);
	entire_byte_num = sizeof(uint8_t) * x_byte_num * length;

	uint8_t *rectangle_data = (uint8_t *)malloc(RECTANGLE_SIZE_MAX * sizeof(uint8_t));
	memset(rectangle_data, 0, RECTANGLE_SIZE_MAX * sizeof(uint8_t));

	*rectangle_data = entire_byte_num; // 装载entire_byte_num
	p = rectangle_data;
	pT1 = rectangle_data + 0x01; // 获取到数据的起始地址

	// 先进行全图填充
	flag = 1;
	for (uint8_t i = 0; i < entire_byte_num; i++)
	{
		if (pT1 - p > entire_byte_num)
		{
			ESP_LOGE("rectangle", "计算总数据大小出错");
			return NULL; // 越界退出
		}
		for (uint8_t j = 0; j < 8; j++)
		{
			*pT1 |= flag << (7 - j); // 写入
			if (Dx == breadth - 1)
			{
				Dx = 0;
				j = 8; // 一行写完强制退出，写下一个，实际就是回车，因为下一个字节就是下一行的了
			}
			else
				Dx++;
		}
		pT1++; // 地址偏移
	}
	return rectangle_data;
}

// 打印函数，但是只能是单一字符或数字
// 一样为了支持动画效果，允许起始坐标可以没有范围甚至为负数

// 显示一个数字，起始坐标xy,输入整型0-9数字，不支持负数，颜色color,亮度0-100%
void print_number(int16_t x, int16_t y, int8_t figure, uint8_t color[3], uint8_t change)
{
	uint8_t *p = NULL;
	// 将p指向对应数字字模
	switch (figure)
	{
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
	if (p == NULL)
	{
		ESP_LOGE("print_number", "输入了0-9之外的数字");
		return;
	}
	separation_draw(x, y, FIGURE, p, sizeof(matrix_7), color, change); // 因为，数字字模数据大小一样，随便输入一个字模就可以
}

// 颜色导入(x为绝对坐标值，0 到 LINE_LED_NUMBER-1)
// SE30中VERTICAL_LED_NUMBER=12 如果您希望扩展屏幕纵向长度，务必修改这个函数
void color_input(int8_t x, int8_t y, uint8_t *dat)
{
	if (x < 0 || x > LINE_LED_NUMBER)
		return; // 不在显示范围退出即可，允许在范围外但不报告
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

// 颜色导出(x为绝对坐标值，0 到 LINE_LED_NUMBER-1)
// SE30中VERTICAL_LED_NUMBER=12 如果您希望扩展屏幕纵向长度，务必修改这个函数
void color_output(int8_t x, int8_t y, uint8_t *dat)
{
	if (x < 0 || x > LINE_LED_NUMBER)
		return; // 不在显示范围退出即可，允许在范围外但不报告
	switch (y)
	{
	case 1:
		dat[0] = red_y1[x];
		dat[1] = green_y1[x];
		dat[2] = blue_y1[x];
		break;

	case 2:
		dat[0] = red_y2[x];
		dat[1] = green_y2[x];
		dat[2] = blue_y2[x];
		break;

	case 3:
		dat[0] = red_y3[x];
		dat[1] = green_y3[x];
		dat[2] = blue_y3[x];
		break;

	case 4:
		dat[0] = red_y4[x];
		dat[1] = green_y4[x];
		dat[2] = blue_y4[x];
		break;

	case 5:
		dat[0] = red_y5[x];
		dat[1] = green_y5[x];
		dat[2] = blue_y5[x];
		break;

	case 6:
		dat[0] = red_y6[x];
		dat[1] = green_y6[x];
		dat[2] = blue_y6[x];
		break;

	case 7:
		dat[0] = red_y7[x];
		dat[1] = green_y7[x];
		dat[2] = blue_y7[x];
		break;

	case 8:
		dat[0] = red_y8[x];
		dat[1] = green_y8[x];
		dat[2] = blue_y8[x];
		break;

	case 9:
		dat[0] = red_y9[x];
		dat[1] = green_y9[x];
		dat[2] = blue_y9[x];
		break;

	case 10:
		dat[0] = red_y10[x];
		dat[1] = green_y10[x];
		dat[2] = blue_y10[x];
		break;

	case 11:
		dat[0] = red_y11[x];
		dat[1] = green_y11[x];
		dat[2] = blue_y11[x];
		break;

	case 12:
		dat[0] = red_y12[x];
		dat[1] = green_y12[x];
		dat[2] = blue_y12[x];
		break;

	default:
		break;
	}
}

// 颜色数据合成,线路选择1-VERTICAL_LED_NUMBER
// SE30中VERTICAL_LED_NUMBER=12 如果您希望扩展屏幕纵向长度，务必修改这个函数
// 将R：red_y(x) G:green_y(x) B:blue_y(x) 合成为 为WS2812发送的数据
void color_compound(uint8_t line_sw)
{
	uint8_t i = 0;
	// 初始化
	uint8_t *red = NULL;
	uint8_t *green = NULL;
	uint8_t *blue = NULL;
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
	if (!red || !green || !blue)
		return;
	// 填充数据
	for (i = 0; i < LINE_LED_NUMBER; i++)
	{
		// GRB顺序
		compound_result[i * 3 + 0] = *green;
		compound_result[i * 3 + 1] = *red;
		compound_result[i * 3 + 2] = *blue;
		red++, green++, blue++; // 地址偏移
	}
}

/// @brief 初始化灯板阵列
void ledarray_init()
{
	static rmt_config_t rmt_cfg0_buf = RMT_DEFAULT_CONFIG_TX(ledarray_gpio_info[0], 0); // 使用默认通道配置模板，通道0
	static rmt_config_t rmt_cfg1_buf = RMT_DEFAULT_CONFIG_TX(ledarray_gpio_info[1], 1); // 使用默认通道配置模板，通道1
	rmt_cfg0_buf.clk_div = 2;															// 修改成员，设定计数器分频，如果频率不适配，是无法运行的
	rmt_cfg1_buf.clk_div = 2;

	rmt_cfg0_buf.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
	rmt_cfg1_buf.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;

	rmt_config(&rmt_cfg0_buf); // 传输配置参数
	rmt_config(&rmt_cfg1_buf);
	// 控制器安装  （通道选择，接收内存块数量（发送模式使用0个），中断标识）
	rmt_driver_install(rmt_cfg0_buf.channel, 0, 0);
	rmt_driver_install(rmt_cfg1_buf.channel, 0, 0);

	// 安装 ws2812控制
	led_strip_config_t strip_cfg0 = LED_STRIP_DEFAULT_CONFIG(LINE_LED_NUMBER, (led_strip_dev_t)rmt_cfg0_buf.channel);
	led_strip_config_t strip_cfg1 = LED_STRIP_DEFAULT_CONFIG(LINE_LED_NUMBER, (led_strip_dev_t)rmt_cfg1_buf.channel);
	strip0 = led_strip_new_rmt_ws2812(&strip_cfg0);
	strip1 = led_strip_new_rmt_ws2812(&strip_cfg1);

	rmt_cfg0 = &rmt_cfg0_buf;
	rmt_cfg1 = &rmt_cfg0_buf;

	gpio_reset_pin(ledarray_gpio_info[0]);
	gpio_reset_pin(ledarray_gpio_info[1]);

	rmt_tx_stop(0);
	rmt_tx_stop(1);
}

/// @brief 重置灯板阵列
void ledarray_deinit()
{
	strip0->del(strip0);
	strip0 = NULL;
	strip1->del(strip1);
	strip1 = NULL;
	rmt_driver_uninstall(0);
	rmt_driver_uninstall(1);
}

/// @brief 灯板阵列选定并写入，未通过ledarray_init()初始化ledarray，函数内会自动初始化
/// @param group_sw 选定要刷新的组，每组有两串WS2812,如12行WS2812,共6组,取值为0-5
void ledarray_set_and_write(uint8_t group_sw)
{
	if (group_sw > VERTICAL_LED_NUMBER / 2 - 1)
	{
		ESP_LOGE("ledarray_set_and_write", "输入了不在显示范围的组别");
		return;
	}

	if (strip0 == NULL || strip1 == NULL)
	{
		ledarray_init();
	}

	// 记录了上次调用函数刷新的组的输出IO
	static gpio_num_t former_select0 = ledarray_gpio_info[0];
	static gpio_num_t former_select1 = ledarray_gpio_info[1];

	// strip0
	gpio_reset_pin(former_select0);											   // 解除对上次刷新IO的绑定
	color_compound(group_sw * 2 + 1);										   // 合成数据
	rmt_set_gpio(0, RMT_MODE_TX, ledarray_gpio_info[group_sw * 2 + 0], false); // 绑定要写入的新输出IO
	vTaskDelay(pdMS_TO_TICKS(10));
	for (uint8_t j = 0; j < LINE_LED_NUMBER * 3; j += 3)
		strip0->set_pixel(strip0, j / 3, compound_result[j + 1], compound_result[j + 0], compound_result[j + 2]); // 设置即将刷新的数据
	rmt_tx_start(0, false);
	strip0->refresh(strip0, 100); // 对现在绑定的IO写入数据
	rmt_tx_stop(0);
	former_select0 = ledarray_gpio_info[group_sw * 2 + 0];

	// strip1
	gpio_reset_pin(former_select1);											   // 解除对上次刷新IO的绑定
	color_compound(group_sw * 2 + 2);										   // 合成数据
	rmt_set_gpio(1, RMT_MODE_TX, ledarray_gpio_info[group_sw * 2 + 1], false); // 绑定要写入的新输出IO
	vTaskDelay(pdMS_TO_TICKS(10));
	for (uint8_t j = 0; j < LINE_LED_NUMBER * 3; j += 3)
		strip1->set_pixel(strip1, j / 3, compound_result[j + 1], compound_result[j + 0], compound_result[j + 2]); // 设置即将刷新的数据
	rmt_tx_start(1, false);
	strip1->refresh(strip1, 100); // 对现在绑定的IO写入数据
	rmt_tx_stop(1);
	former_select1 = ledarray_gpio_info[group_sw * 2 + 1];
}

// RGB亮度调制  导入r g b数值地址+亮度
void ledarray_intensity_change(uint8_t *r, uint8_t *g, uint8_t *b, uint8_t intensity)
{
	// 注意，RGB和HSV的取值范围并不一致，标准定义是 R G B 为 0-255  H 为 0-360 S V 为0-1（为了方便计算，这里 S V 映射到 0-100）

	if (*r == 0 && *g == 0 && *b == 0)
		return; //	倘若 r g b 三个分量值都是0，显然客观上不需要变换，增加明度只会干扰数值

	if (intensity > 100)
	{
		ESP_LOGE("ledarray_intensity_change", "错误的亮度数值 %d", intensity);
		*r = 0x00;
		*g = 0x00;
		*b = 0x00;
		return;
	}
	uint32_t h = 0, s = 0, v = 0;
	rgb_to_hvs(*r, *g, *b, &h, &s, &v);
	v = intensity;
	led_strip_hsv2rgb(h, s, v, (uint32_t *)r, (uint32_t *)g, (uint32_t *)b);
}

// 因为max和min好像是c++的，这里手写一个，效果一样
// 取三个double元素最大的那个
double value_max(double value1, double value2, double value3)
{
	double buffer[3] = {value1, value2, value3};
	uint8_t a = 0, b = 0;
	for (a = 0; a < 2; a++)
	{
		for (b = a + 1; b < 3; b++)
		{
			if (buffer[a] > buffer[b])
			{
				double save = buffer[b];
				buffer[b] = buffer[a];
				buffer[a] = save;
			}
		}
	}
	return buffer[2];
}

// 取三个double元素最小的那个
double value_min(double value1, double value2, double value3)
{
	double buffer[3] = {value1, value2, value3};
	uint8_t a = 0, b = 0;
	for (a = 0; a < 2; a++)
	{
		for (b = a + 1; b < 3; b++)
		{
			if (buffer[a] > buffer[b])
			{
				double save = buffer[b];
				buffer[b] = buffer[a];
				buffer[a] = save;
			}
		}
	}
	return buffer[0];
}

// 注意，RGB和HSV的取值范围并不一致，标准定义是 R G B 为 0-255  H 为 0-360 S V 为0-1
// 然而，为了方便计算，这里 S V 映射到 0-100
// 将RGB转换到HSV颜色空间,计算方法是网上随便找的
void rgb_to_hvs(uint8_t red_buf, uint8_t green_buf, uint8_t blue_buf, uint32_t *p_h, uint32_t *p_s, uint32_t *p_v)
{
	// HSV需要浮点存储
	double h = 0, s = 0, v = 0;

	// 将RGB映射到0 - 1之间,并由浮点变量 r g b 存储
	double r = 0, g = 0, b = 0;
	r = red_buf / 255.0;
	g = green_buf / 255.0;
	b = blue_buf / 255.0;

	// 计算V
	v = value_max(r, g, b);

	// 计算S
	if (v != 0)
	{
		s = v - value_min(r, g, b);
		s = s / v;
	}
	else
		s = 0;

	// 计算H
	if (v == r)
		h = 60 * (g - b) / (v - value_min(r, g, b));
	if (v == g)
		h = 120 + 60 * (b - r) / (v - value_min(r, g, b));
	if (v == b)
		h = 240 + 60 * (r - g) / (v - value_min(r, g, b));

	if (h < 0)
		h = h + 360;

	// 映射到需求范围 0 - 100
	s = s * 100;
	v = v * 100;

	static uint32_t out_h = 0, out_s = 0, out_v = 0;
	// 类型转换，随便四舍五入一下
	out_h = ceil(h);
	out_s = ceil(s);
	out_v = ceil(v);
	// 数据输出
	*p_h = out_h;
	*p_s = out_s;
	*p_v = out_v;
}

// 以下函数来自ESP-IDFv4.4 led_strip.c 例程文件

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

	switch (i)
	{
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
