
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

// 该文件归属701Enti组织，主要由SEVETEST30开发团队维护，对语言文字显示的硬件字库的访问支持，包含对字体特别是中文汉字的搜索工作
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// github: https://github.com/701Enti
// bilibili: 701Enti

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