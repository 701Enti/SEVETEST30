// 该文件由701Enti编写，包含WS2812构成的LED阵列的图形与显示处理，不包含WS2812底层驱动程序
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体不包含WS2812硬件驱动代码，而是参考Espressif官方提供的led_strip例程文件，同时还使用了源文件中的hsv到rgb的转换函数,非常感谢
// 官方例程连接：https://github.com/espressif/esp-idf/tree/release/v4.4/examples/common_components/led_strip
// 官方文档链接：https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.4/esp32/api-reference/peripherals/rmt.html
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.7.13

#include <string.h>

#ifndef _SEVETEST30_LEDARRAY_H_
#define _SEVETEST30_LEDARRAY_H_
#endif 

#define FIGURE 4                    //数字 4x7
#define LETTER 5                    //字母 5x8
#define CHINESE 12                  //中文 12x12
#define LINE_LED_NUMBER  24         //灯板横向长度（每一根通讯线连接的WS2812数量） 此处“横向”始终表示通讯线延伸方向
#define VERTICAL_LED_NUMBER 12      //灯板纵向长度（通讯线数量）                  此处“纵向”始终垂直通讯线延伸方向
#define RECTANGLE_MATRIX(pRECTANGLE) (pRECTANGLE+0x01)
#define RECTANGLE_SIZE_MAX 36//矩形最大允许数据字节数，这决定矩形生成函数可以生成多大矩形，这里设定其为整个显示面板大小，假设希望最大大小是 12x25,25是矩形横向长度，计算25/8约为4（不足1就进1），得到12x4=48

//数字 0-9
extern const uint8_t matrix_1 [7];
extern const uint8_t matrix_2 [7];
extern const uint8_t matrix_3 [7];
extern const uint8_t matrix_4 [7];
extern const uint8_t matrix_5 [7];
extern const uint8_t matrix_6 [7];
extern const uint8_t matrix_7 [7];
extern const uint8_t matrix_8 [7];
extern const uint8_t matrix_9 [7];

extern const uint8_t sign_se30[872];
extern const uint8_t sign_701[872];

extern uint8_t compound_result[LINE_LED_NUMBER*3];


    // 以下函数将数据存储到缓冲区，不包含发送
    //sevetest30支持两种显示解析 三色分离方式 和 彩色图像直显方式
    //前者如上面的思路，将彩色图像分成三个图层处理，便于图形变换
    //后者直接读取存储了RGB数据的数组，直接写入WS2812,便于显示复杂，颜色丰富的图形


//三色分离方式 
    //取模适配PCtoLCD2002
    //取模说明：从第一行开始向右每取8个点作为一个字节，如果最后不足8个点就补满8位。
    //取模顺序是从高到低，即第一个点作为最高位。如*-------取为10000000


void separation_draw(int16_t x, int16_t y, uint8_t breadth,const uint8_t *p, uint8_t byte_number, uint8_t* color_in,uint8_t change);


//彩色图像直显方式 
   //取模方式适配Img2Lcd
   //水平扫描，从左到右，从顶到底扫描，24位真彩（RGB顺序），需要图像数据头
   //图像编辑可以用系统自带的画板工具，像素调到合适值如12x24,不显示的地方要填充黑色
   //选择带数据头的图案数据，长宽会自动获取


void direct_draw(int16_t x, int16_t y,const uint8_t *p,uint8_t change);

void clean_draw_buf(int8_t y);

void progress_draw_buf(int8_t y,float step, uint8_t *color);

uint8_t *rectangle(int8_t breadth, int8_t length);

void print_number(int16_t x,int16_t y,int8_t figure,uint8_t* color,uint8_t change);

void ledarray_init();

void ledarray_deinit();

//以下函数处理上面的函数产生的颜色数据，配置传输数据发送


void color_compound(uint8_t line_sw);

void ledarray_set_and_write(uint8_t group_sw);


//以下是内部私有函数，一般不会在外部调用

void color_input(int8_t x, int8_t y, uint8_t* dat);

void color_output(int8_t x, int8_t y, uint8_t* dat);

double value_max(double value1,double value2,double value3);


double value_min(double value1,double value2,double value3);


void ledarray_intensity_change(uint8_t *r,uint8_t *g,uint8_t *b,uint8_t intensity);

void rgb_to_hvs(uint8_t red_buf, uint8_t green_buf, uint8_t blue_buf,uint32_t *p_h, uint32_t *p_s, uint32_t *p_v);

    //以下函数来自ESP-IDFv4.4 led_strip.c 例程文件，以及源文件声明

/**
 * @brief Simple helper function, converting HSV color space to RGB color space
 *
 * Wiki: https://en.wikipedia.org/wiki/HSL_and_HSV
 *
 */
void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b);



