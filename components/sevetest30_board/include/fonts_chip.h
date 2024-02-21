// 该文件由701Enti编写，对语言文字显示的硬件字库的访问支持，包含对字体特别是中文汉字的搜索工作
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体不包含spi通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行
// 以下所有程序都是为GT32L32S0140编写的，您可以在www.hobos.com.cn通过以下链接下载其包含程序实现的数据手册
// http://www.hobos.com.cn/upload/datasheet/GT32L32S0140.pdf
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.12.9

#pragma once
#include "esp_err.h"
#include "board_ctrl.h"

//SPI通讯 GPIO SPI_ID 时钟频率 在board.def中设置，通过board_pins_config中函数读取，如果您需要宏定义设置，这需要修改以下函数引脚配置相关

#define FONT_READ_CN_12X_BYTES 12 * 2 //一个12x12中文汉字数据字节数 

#define FONT_SPI_MODE 0 //SPI通讯模式


#define FONT_READ_CMD 0x03 //普通读命令码
#define FONT_READ_COMMAND_BITS 8       //普通读 命令 位长度
#define FONT_READ_ADDRESS_BITS 8 * 3   //普通读 地址 位长度

#define FONT_READ_DUMMY_BITS  0      //普通读 假时钟 位长度，主机发送读取指令时的延时时钟位数，用于调整通讯质量




esp_err_t fonts_chip_init(board_device_handle_t* board_device_handle);

uint8_t *fonts_read_zh_CN_12x(board_device_handle_t* board_device_handle);