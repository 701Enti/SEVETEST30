
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

 // 该文件归属701Enti组织，SEVETEST30开发团队应该提供责任性维护，对语言文字显示的硬件字库GT32L32S0140的访问支持，包含对字体特别是中文汉字的搜索工作
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // 计算方法来自高通字库 https://www.hmi.gaotongfont.cn/ 非常感谢
 // github: https://github.com/701Enti
 // bilibili: 701Enti

#include "gt32l32s0140.h"
#include "board_pins_config.h"
#include "board_ctrl.h"
#include "driver/gpio.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"

spi_device_handle_t fonts_chip_handle = NULL;


/// @brief 通用字库芯片基本初始化工作
/// @param board_device_handle
/// @return ESP_OK/ESP_FAIL
esp_err_t fonts_chip_init()
{
    if (fonts_chip_handle) {
        ESP_LOGE("fonts_chip_init", "字库设备已经初始化");
        return ESP_FAIL;
    }

    esp_err_t ret = ESP_OK;
    //配置spi总线
    spi_bus_config_t bus_config = {
        .mosi_io_num = -1,
        .miso_io_num = -1,
        .sclk_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SOC_SPI_MAXIMUM_BUFFER_SIZE,
        .flags = SPICOMMON_BUSFLAG_MASTER,
    };
    spi_device_interface_config_t interface_config = {
        .command_bits = FONT_READ_COMMAND_BITS,
        .address_bits = FONT_READ_ADDRESS_BITS,
        .dummy_bits = FONT_READ_DUMMY_BITS,
        .mode = FONT_SPI_MODE,
        .clock_speed_hz = FONT_SPI_FREQ,
        .spics_io_num = -1,
        .queue_size = 7,
    };

    ret = get_spi_pins(&bus_config, &interface_config);

    ret = spi_bus_initialize(FONTS_CHIP_SPI_ID, &bus_config, SPI_DMA_CH_AUTO);

    //载入字库芯片设备
    ret = spi_bus_add_device(FONTS_CHIP_SPI_ID, &interface_config, &(fonts_chip_handle));

    //设置片选CS的GPIO
    gpio_set_level(interface_config.spics_io_num, 1);
    gpio_config_t spics_cfg = {
        .pin_bit_mask = BIT64(interface_config.spics_io_num),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&spics_cfg);

    return ret;
}

/// @brief 从字库读取12x12汉字字模,可以作为绘制函数的字模参数
/// @param Unicode 汉字字模的Unicode编码
/// @param dest 导入读取缓存位置,内部不会自动清理缓存,缓存必须足够FONT_READ_CN_12X_BYTES
void fonts_read_zh_CN_12x(uint32_t Unicode,uint8_t* dest)
{
    const char* TAG = "fonts_read_zh_CN_12x";
    esp_err_t ret = ESP_OK;
    
    if (!fonts_chip_handle) {
        ESP_LOGE(TAG, "字库设备未初始化");
        return;
    }

    //获取对应GB2312编码
    uint32_t GB2312_buf = 0;
    GB2312_buf = UnicodeToGB2312(Unicode);

    spi_transaction_t transaction;

    //计算内部地址
    if ((GB2312_buf >> 8) >= 0xA1 && (GB2312_buf >> 8) <= 0XA9 && (GB2312_buf & 0xFF) >= 0xA1)
        transaction.addr = (((GB2312_buf >> 8) - 0xA1) * 94 + ((GB2312_buf & 0xFF) - 0xA1)) * 24 + FONT_ZH_CN_12x_BASE_ADD;
    else if ((GB2312_buf >> 8) >= 0xB0 && (GB2312_buf >> 8) <= 0xF7 && (GB2312_buf & 0xFF) >= 0xA1)
        transaction.addr = (((GB2312_buf >> 8) - 0xB0) * 94 + ((GB2312_buf & 0xFF) - 0xA1) + 846) * 24 + FONT_ZH_CN_12x_BASE_ADD;

    transaction.length = FONT_READ_ZH_CN_12X_BYTES * 8;
    transaction.rxlength = 0;
    transaction.cmd = FONT_READ_CMD;
    transaction.flags = 0;
    transaction.rx_buffer = dest;
    transaction.tx_buffer = NULL;

    //获取CS引脚GPIO_NUM
    spi_device_interface_config_t interface_config;
    get_spi_pins(NULL, &interface_config);

    //通讯开始
    gpio_set_level(interface_config.spics_io_num, 0);

    //通讯传输
    ret |= spi_device_polling_start(fonts_chip_handle,&transaction,portMAX_DELAY);
    ret |= spi_device_polling_end(fonts_chip_handle, pdMS_TO_TICKS(FONT_READ_TIMEOUT_MS));

    //通讯结束
    gpio_set_level(interface_config.spics_io_num, 1);

    if (ret != ESP_OK){
        ESP_LOGE(TAG, "与字库芯片通讯时发现问题 描述： %s", esp_err_to_name(ret));
    }
}

/// @brief 从字库读取6x12 ASCII字符字模,可以作为绘制函数的字模参数
/// @param Unicode ASCII字符字模的Unicode编码
/// @param dest 导入读取缓存位置,内部不会自动清理缓存,缓存必须足够FONT_READ_ASCII_6X12_BYTES
void fonts_read_ASCII_6x12(uint32_t Unicode,uint8_t* dest) {
    const char* TAG = "fonts_read_ASCII_6x12";
    esp_err_t ret = ESP_OK;

    if (!fonts_chip_handle) {
        ESP_LOGE(TAG, "字库设备未初始化");
        return;
    }

    spi_transaction_t transaction;

    if ((Unicode >= 0x20) && (Unicode <= 0x7E))
        transaction.addr =  (Unicode - 0x20) * 12 + FONT_ASCII_6X12_BASE_ADD;

    transaction.length = FONT_READ_ASCII_6X12_BYTES * 8;
    transaction.rxlength = 0;
    transaction.cmd = FONT_READ_CMD;
    transaction.flags = 0;
    transaction.rx_buffer = dest;
    transaction.tx_buffer = NULL;

    //获取CS引脚GPIO_NUM
    spi_device_interface_config_t interface_config;
    get_spi_pins(NULL, &interface_config);

    //通讯开始
    gpio_set_level(interface_config.spics_io_num, 0);

    //通讯传输
    ret |= spi_device_polling_start(fonts_chip_handle,&transaction,portMAX_DELAY);
    ret |= spi_device_polling_end(fonts_chip_handle, pdMS_TO_TICKS(FONT_READ_TIMEOUT_MS));

    //通讯结束
    gpio_set_level(interface_config.spics_io_num, 1);

    if (ret != ESP_OK){
        ESP_LOGE(TAG, "与字库芯片通讯时发现问题 描述： %s", esp_err_to_name(ret));
    }
}


//获取 UTF-8再编码数据 对应到的 Unicode编码数据此处思路
// 1.由于utf_idx累加,某时刻找到非[10]开头的字节,
//   如果为0开头,直接导入unicode_dest[dest_idx],dest_idx++,继续utf_idx累加
//   否则根据其获取 [剩余]字节数last_unit
//   同时缓存这个时候的utf_idx值保存到utf_idx_buf
// 2.根据last_unit +1 使用位与获取头字节内含的有效数据
//   如[111]0 0110 保存uint8_t ubuf1为0000 0110 
// 3.utf_idx+=last_unit之后,索引utf_dat用0x3F取出字节有效数据,
//  从最低位开始,保存到unicode_dest[dest_idx],每个有效数据是6位,
//   unicode_dest[dest_idx] |=  有效数据<<((last_unit - (utf_idx - utf_idx_buf))*6) 无间隔拼接
// 4.每取一次结束 之后 utf_idx--
//   直到utf_idx=utf_idx_buf时不再像刚才一样取
//   执行unicode_dest[dest_idx] |=  ubuf1 << (last_unit*6) 
// 5.utf_idx+=last_unit;
//   dest_idx++;
//   ubuf1=0;
//   last_unit=0 ;
//   utf_idx_buf = 0;
// 6.回到1

/// @brief 获取字符串即UTF-8再编码数据 对应到的 Unicode编码数据
/// @param utf_dat 导入一个字符串(本质是UTF-8代码数据链指针)或者说是UTF-8再编码数据区首地址(如果使用char数组,请在末尾单元加上'\0')
/// @param Unicode_dest Unicode编码数据缓存位置,一个uint32_t单元对应一个文字或符号的Unicode编码
/// @param dest_len Unicode_dest缓存中能够存储uint32_t单元的个数(缓存最大可映射字符数)
/// @return 总共识别到的字符数
uint32_t UTF8_Unicode_get(char* utf_dat, uint32_t* Unicode_dest, int dest_len) {
    static const char* TAG = "UTF8_Unicode_get";

    int utf_idx = 0;//UTF-8再编码数据区索引变量
    int utf_idx_buf = 0;//utf_idx的缓存
    int dest_idx = 0;//Unicode编码数据区索引变量
    int last_unit = 0;//[剩余]字节数
    uint8_t ubuf1 = 0;//Unicode编码临时缓存1

    while (utf_dat[utf_idx] != '\0' && dest_idx < dest_len)
    {
        //单字节表示 - 在0x7F内 直接等于ASCII
        if (utf_dat[utf_idx] <= 0x7F) {
            Unicode_dest[dest_idx] = utf_dat[utf_idx];
            dest_idx++;
        }
        else {
            //多字节表示 - 出现此外非10开头字节
            if ((utf_dat[utf_idx] >> 6) != 0x02) {
                //获取后接的剩余字节,UTF-8最大6字节表示一个Unicode编码
                if (utf_dat[utf_idx] >= 0xC0 && utf_dat[utf_idx] < 0xE0)last_unit = 1;
                if (utf_dat[utf_idx] >= 0xE0 && utf_dat[utf_idx] < 0xF0)last_unit = 2;
                if (utf_dat[utf_idx] >= 0xF0 && utf_dat[utf_idx] < 0xF8)last_unit = 3;
                if (utf_dat[utf_idx] >= 0xF8 && utf_dat[utf_idx] < 0xFC)last_unit = 4;
                if (utf_dat[utf_idx] >= 0xFC && utf_dat[utf_idx] < 0xFE)last_unit = 5;
                //缓存这个时候的utf_idx值
                utf_idx_buf = utf_idx;
                //缓存头标志有效数据
                if (last_unit == 1)ubuf1 = 0x1F & utf_dat[utf_idx];
                if (last_unit == 2)ubuf1 = 0x0F & utf_dat[utf_idx];
                if (last_unit == 3)ubuf1 = 0x07 & utf_dat[utf_idx];
                if (last_unit == 4)ubuf1 = 0x03 & utf_dat[utf_idx];
                if (last_unit == 5)ubuf1 = 0x01 & utf_dat[utf_idx];
                //拼接剩余数据
                utf_idx += last_unit;//跳转到字符UTF-8码末尾字节
                while (utf_idx != utf_idx_buf) {
                    Unicode_dest[dest_idx] |= (utf_dat[utf_idx] & 0x3F) << ((last_unit - (utf_idx - utf_idx_buf)) * 6);//每个有效数据是6位,无间隔拼接
                    utf_idx--;//反方向遍历

                }

                //追加头标志的有效数据,结束
                Unicode_dest[dest_idx] |= ubuf1 << (last_unit * 6);
                dest_idx++;
                utf_idx += last_unit;//跳转到字符UTF-8码末尾字节
                //复位缓存
                ubuf1 = 0;
                last_unit = 0;
                utf_idx_buf = 0;


            }
            else {
                ESP_LOGE(TAG, "处理UTF-8源数据发现解析异常");
            }
        }
        utf_idx++;
    }
    return dest_idx;//返回总字符数
}

/// @brief 将Unicode编码转换为对应GB2312编码,计算方法来自高通字库 https://www.hmi.gaotongfont.cn/
/// @param code Unicode编码
/// @return 对应GB2312编码 设备未初始化: 0xFFFF 
uint32_t UnicodeToGB2312(uint32_t code)
{
    static const char* TAG = "UnicodeToGB2312";

    if (!fonts_chip_handle) {
        ESP_LOGE(TAG, "字库设备未初始化");
        return 0xFF;
    }

    //获取索引地址
    uint32_t U_Addr;
    if (code >= 0x00a0 && code <= 0x0451) U_Addr = 2 * (code - 0xA0);
    else if (code >= 0x2010 && code <= 0x2642) U_Addr = 2 * (code - 0x2010) + 1892;
    else if (code >= 0x3000 && code <= 0x33d5) U_Addr = 2 * (code - 0x3000) + 5066;
    else if (code >= 0x4E00 && code <= 0x9FA5) U_Addr = 2 * (code - 0x4E00) + 7030;
    else if (code >= 0xfe30 && code <= 0xfe6b) U_Addr = 2 * (code - 0xfe30) + 48834;
    else if (code >= 0xff01 && code <= 0xFF5e) U_Addr = 2 * (code - 0xff01) + 48954;
    else if (code >= 0xffe0 && code <= 0xFFe5) U_Addr = 2 * (code - 0xff01) + 49142;
    else U_Addr = 0;

    //得到存储对应GB2312编码的缓存的地址 
    uint32_t dest_addr = FONT_GB2312_MAP_BASE_ADD + U_Addr;

    //读取转换结果
    uint8_t result_code[2] = { 0 };
    spi_transaction_t transaction;
    transaction.length = 2 * 8;
    transaction.rxlength = 0;
    transaction.cmd = FONT_READ_CMD;
    transaction.addr = dest_addr;
    transaction.flags = 0;
    transaction.rx_buffer = result_code;
    transaction.tx_buffer = NULL;

    //获取CS引脚GPIO_NUM
    spi_device_interface_config_t interface_config;
    get_spi_pins(NULL, &interface_config);

    //通讯开始
    gpio_set_level(interface_config.spics_io_num, 0);

    //通讯传输
    esp_err_t ret = spi_device_polling_transmit(fonts_chip_handle, &transaction);

    //通讯结束
    gpio_set_level(interface_config.spics_io_num, 1);

    if (ret != ESP_OK)ESP_LOGE(TAG, "与字库芯片通讯时发现问题 描述： %s", esp_err_to_name(ret));

    return (result_code[0] << 8) | result_code[1];
}

