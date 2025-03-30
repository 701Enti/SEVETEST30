/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2019 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

 //这是一个已修改的文件,原作者信息见上方声明,在原程序基础上,
 //更改为项目需要的形式或设置
 //这个修改为适应硬件环境的一个项目需求, 不是否认原作者设计的可靠性
 //为了明确原作者信息,此文件API帮助及相关内容不在文档中显示

#include "esp_log.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include <string.h>
#include "board.h"
#include "audio_error.h"
#include "audio_mem.h"
#include "soc/io_mux_reg.h"
#include "soc/soc_caps.h"

#include "board_def.h"

static const char* TAG = "SEVETEST30_BOARD";

esp_err_t get_i2c_pins(i2c_port_t port, i2c_config_t* i2c_config)
{
    AUDIO_NULL_CHECK(TAG, i2c_config, return ESP_FAIL);
    if (port == AUDIO_I2C_PORT) {
        i2c_config->sda_io_num = AUDIO_I2C_SDA_IO;
        i2c_config->scl_io_num = AUDIO_I2C_SCL_IO;
    }
    else if (port == DEVICE_I2C_PORT) {
        i2c_config->sda_io_num = DEVICE_I2C_SDA_IO;
        i2c_config->scl_io_num = DEVICE_I2C_SCL_IO;
    }
    else {
        i2c_config->sda_io_num = -1;
        i2c_config->scl_io_num = -1;
        ESP_LOGE(TAG, "i2c port %d is not supported", port);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t get_i2s_pins(i2s_port_t port, board_i2s_pin_t* i2s_config)
{
    AUDIO_NULL_CHECK(TAG, i2s_config, return ESP_FAIL);
    if (port == I2S_NUM_0 || port == I2S_NUM_1) {
        i2s_config->mck_io_num = I2S_MCK_IO;
        i2s_config->bck_io_num = I2S_BCK_IO;
        i2s_config->ws_io_num = I2S_WS_IO;
        i2s_config->data_out_num = I2S_DAC_DATA_IO;
        i2s_config->data_in_num = I2S_ADC_DATA_IO;
    }
    else {
        memset(i2s_config, -1, sizeof(board_i2s_pin_t));
        ESP_LOGE(TAG, "i2s port %d is not supported", port);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t get_spi_pins(spi_bus_config_t* spi_config, spi_device_interface_config_t* spi_device_interface_config)
{
    //获取为字库芯片提供的SPI通讯IO定义
    if (spi_device_interface_config == NULL)return ESP_FAIL;
    spi_device_interface_config->spics_io_num = SPI_CS_IO;

    if (spi_config == NULL)return ESP_FAIL;
    spi_config->mosi_io_num = SPI_MOSI_IO;
    spi_config->miso_io_num = SPI_MISO_IO;
    spi_config->sclk_io_num = SPI_SCLK_IO;

    return ESP_OK;
}

int8_t get_pa_enable_gpio(void)
{
    return PA_ENABLE_GPIO;
}

int8_t get_vibra_motor_IN1_gpio(void)
{
    return VIBRA_IN1_IO;
}

int8_t get_vibra_motor_IN2_gpio(void)
{
    return VIBRA_IN2_IO;
}
