// 该文件由701Enti编写，对语言文字显示的硬件字库的访问支持，包含对字体特别是中文汉字的搜索工作
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体不包含spi通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.12.9

#pragma once

#include "esp_err.h"
#include "board_ctrl.h"

esp_err_t fonts_chip_init(board_device_handle_t* board_device_handle);

uint8_t *fonts_read_zh_CN_12x(board_device_handle_t* board_device_handle);