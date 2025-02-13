
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

 // 对语言文字显示的硬件字库GT32L32S0140的访问支持，包含对字体特别是中文汉字的搜索工作
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // 计算方法来自高通字库 https://www.hmi.gaotongfont.cn/ 非常感谢
 // github: https://github.com/701Enti
 // bilibili: 701Enti

#pragma once
#include "esp_err.h"
#include "board_ctrl.h"

//SPI通讯 GPIO SPI_ID 时钟频率 在board.def中设置

#define FONT_SPI_MODE  0//SPI通讯模式

#define FONT_READ_CMD 0x03 //普通读命令码
#define FONT_READ_COMMAND_BITS 8       //普通读 命令 位长度
#define FONT_READ_ADDRESS_BITS 8 * 3   //普通读 地址 位长度
#define FONT_READ_DUMMY_BITS  0     //普通读 假时钟 位长度，主机发送读取指令时的延时时钟位数，用于调整通讯质量
#define FONT_READ_TIMEOUT_MS 1000   //读取通讯超时时间,单位ms

#define FONT_GB2312_MAP_BASE_ADD 0x3E618B //Unicode到GB2312映射表基地址
#define FONT_ZH_CN_12x_BASE_ADD  0x09670E //12x12点阵GB2312汉字&字符基地址
#define FONT_ASCII_6X12_BASE_ADD 0x080900 //6x12点阵ASCII字符基地址

#define FONT_READ_ZH_CN_12X_BYTES 12 * 2 //一个12x12中文字模数据字节数 
#define FONT_READ_ASCII_6X12_BYTES 12 //一个6x12ASCII字模数据字节数 

esp_err_t fonts_chip_init();

void fonts_read_zh_CN_12x(uint32_t Unicode, uint8_t* dest);

void fonts_read_ASCII_6x12(uint32_t Unicode, uint8_t* dest);

uint32_t UTF8_Unicode_get(char* utf_dat, uint32_t* Unicode_dest, int dest_len);

uint32_t UnicodeToGB2312(uint32_t code);

uint32_t ASCII_number_count(uint32_t* unicode_buf, int total_unit);