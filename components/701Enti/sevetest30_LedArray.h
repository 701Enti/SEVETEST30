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
// 邮箱：   3044963040@qq.com
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

#define RECTANGLE_SIZE_MAX 36//矩形最大允许数据字节数，这决定矩形生成函数可以生成多大矩形，这里设定其为整个显示面板大小，假设希望最大大小是 12x25,25是矩形横向长度，计算25/8约为4（不足1就进1），得到12x4=48

//v5.1下需要这个宏定义支持
// #define RMT_RESOLUTION_HZ 10000000 

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

//大写字母 A-Z

//小写字母 a-z

//基本汉字（宋体）


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
// 起始坐标xy,图案横向宽度值,应该等于字模实际宽度（已定义的：FIGURE-数字 LETTER-字母 CHINESE-汉字）
// 导入字模，sizeof()获取数据长度[如果是通过库内函数生成的图形，此处填入p-0x01]  颜色RGB
// change控制显示亮度，0-100%
// 为了支持动画效果，允许起始坐标可以没有范围甚至为负数
void separation_draw(int16_t x, int16_t y, uint8_t breadth,uint8_t *p, uint8_t byte_number, uint8_t color_in[3],uint8_t change);


//彩色图像直显方式 
   //取模方式适配Img2Lcd
   //水平扫描，从左到右，从顶到底扫描，24位真彩（RGB顺序），需要图像数据头
   //图像编辑可以用系统自带的画板工具，像素调到合适值如12x24,不显示的地方要填充黑色
//起始坐标xy 导入字模(请选择带数据头的图案数据，长宽会自动获取)
//change控制显示亮度，0-100%
// 为了支持动画效果，允许起始坐标可以没有范围甚至为负数
void direct_draw(int16_t x, int16_t y,uint8_t *p,uint8_t change);


    //图形生成函数 p-0x01 指向 entire_byte_num 的值，用于支持显示函数，表示总数据字节数


// 生成一个矩形字模,横向长度1-LINE_LED_NUMBER,纵向长度1-VERTICAL_LED_NUMBER
// 返回 指针指向数据区首地址，可以直接当字模用，
// p-0x01 指向 entire_byte_num 的值，用于支持显示函数，表示总数据字节数
uint8_t *rectangle(uint8_t breadth, uint8_t length);





    //打印函数，但是只能是单一字符或数字
    //一样为了支持动画效果，允许起始坐标可以没有范围甚至为负数



//显示一个数字，起始坐标xy,输入整型0-9数字，不支持负数，颜色color
void print_number(int16_t x,int16_t y,int8_t figure,uint8_t color[3],uint8_t change);




    //以下函数处理上面的函数产生的颜色数据，配置传输数据发送



// 颜色数据合成,线路选择1-12，选择不存在线路会定向到 y12 
//将R：red_y(x) G:green_y(x) B:blue_y(x) 合成为 为WS2812发送的数据
void color_compound(uint8_t line_sw);

//灯板阵列配置写入，自动根据line_sw进行0和1通道一同分组切换，line_sw表示组别，取0-5
//为每两行分组是为支持压缩显示提高效率
void ledarray_set_and_write(uint8_t group_sw);





    //以下是内部私有函数，一般不会在外部调用




//颜色导入(x为绝对坐标值，0-23)
void color_input(int8_t x,int8_t y,uint8_t color[3]);

//取三个double元素最大的那个
double value_max(double value1,double value2,double value3);

//取三个double元素最小的那个
double value_min(double value1,double value2,double value3);

//RGB亮度调制  导入r g b数值地址+亮度
void ledarray_intensity_change(uint8_t *r,uint8_t *g,uint8_t *b,uint8_t intensity);

//注意，RGB和HSV的取值范围并不一致，标准定义是 R G B 为 0-255  H 为 0-360 S V 为0-1
//然而，为了方便计算，这里 S V 映射到 0-100
//将RGB转换到HSV颜色空间,计算方法是网上随便找的
void rgb_to_hvs(uint8_t red_buf, uint8_t green_buf, uint8_t blue_buf,uint32_t *p_h, uint32_t *p_s, uint32_t *p_v);

    //以下函数来自ESP-IDFv4.4 led_strip.c 例程文件，以及源文件声明

/**
 * @brief Simple helper function, converting HSV color space to RGB color space
 *
 * Wiki: https://en.wikipedia.org/wiki/HSL_and_HSV
 *
 */
void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b);



