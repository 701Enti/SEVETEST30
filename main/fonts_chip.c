// 该文件由701Enti编写，对语言文字显示的硬件字库的访问支持，包含对字体特别是中文汉字的搜索工作
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.12.9

#include "fonts_chip.h"
#include "board_pins_config.h"
#include "board_ctrl.h"
#include "driver/gpio.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"

uint8_t zh_CN_12x_buf[FONT_READ_CN_12X_BYTES] = {0};


uint8_t *fonts_read_zh_CN_12x(board_device_handle_t* board_device_handle)
{
    memset(zh_CN_12x_buf,255,FONT_READ_CN_12X_BYTES);

    spi_transaction_t transaction;

    transaction.length = FONT_READ_COMMAND_BITS + FONT_READ_ADDRESS_BITS + FONT_READ_DUMMY_BITS + FONT_READ_CN_12X_BYTES * 8; //12x12字模，每行占用2个Byte,共12行
    transaction.rxlength = 0;
    transaction.cmd = FONT_READ_CMD;
    transaction.addr = 0x0FFFFF;
 
    transaction.rx_buffer = zh_CN_12x_buf;
    transaction.tx_buffer = NULL;
    transaction.flags = 0;

    //获取CS引脚GPIO_NUM
    spi_device_interface_config_t interface_config;
    get_spi_pins(NULL,&interface_config);

    //通讯开始
    gpio_set_level(interface_config.spics_io_num,0);

    //通讯传输
    esp_err_t ret = spi_device_polling_transmit(board_device_handle->fonts_chip_handle,&transaction);

    vTaskDelay(pdMS_TO_TICKS(100));    

    gpio_set_level(interface_config.spics_io_num,1);

    if(ret!=ESP_OK)ESP_LOGE("fonts_read_zh_CN_12x","与字库芯片通讯时发现问题 描述： %s",esp_err_to_name(ret));
    return zh_CN_12x_buf;
}



/// @brief 通用字库芯片基本初始化工作
/// @param board_device_handle
/// @return ESP_OK/ESP_FAIL
esp_err_t fonts_chip_init(board_device_handle_t* board_device_handle)
{
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
        .queue_size  = 7,
    };

    ret = get_spi_pins(&bus_config,&interface_config);

    ret = spi_bus_initialize(FONTS_CHIP_SPI_ID,&bus_config,SPI_DMA_CH_AUTO);

    //载入字库芯片设备
    ret = spi_bus_add_device(FONTS_CHIP_SPI_ID,&interface_config,&(board_device_handle->fonts_chip_handle));

    //设置片选CS的GPIO
    gpio_set_level(interface_config.spics_io_num,1);
    gpio_config_t spics_cfg = {
        .pin_bit_mask = BIT64(interface_config.spics_io_num),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&spics_cfg);

   return ret;
}