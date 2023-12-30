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

#define FONT_READ_CMD 0x03
#define SPI_ID SPI2_HOST
uint8_t *fonts_read_zh_CN_12x(board_device_handle_t* board_device_handle)
{
    spi_transaction_t transaction;

    //获取CS引脚GPIO_NUM
    spi_device_interface_config_t interface_config;
    get_spi_pins(NULL,&interface_config);

    //通讯开始
    gpio_set_level(interface_config.spics_io_num,0);
    
    memset(&transaction,0,sizeof(transaction));
    transaction.length = 2*12*8; //12x12字模，每行占用2个Byte,共12行
    transaction.flags = SPI_TRANS_USE_RXDATA;
    transaction.user = (void*)1;
    transaction.cmd = FONT_READ_CMD;
    transaction.addr = 0x1111;
    esp_err_t ret = spi_device_polling_transmit(board_device_handle->fonts_chip_handle,&transaction);
   
   //通讯结束
    gpio_set_level(interface_config.spics_io_num,1);

    if(ret!=ESP_OK)ESP_LOGE("fonts_read_zh_CN_12x","与字库芯片通讯时发现问题 描述： %s",esp_err_to_name(ret));
    ESP_LOGE("fonts_read_zh_CN_12x"," %d",transaction.rx_data[0]);
    return &transaction.rx_data[0];
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
        .mode = 3,
        .clock_speed_hz = SPI_MASTER_FREQ_10M,
        .spics_io_num = -1,
        .queue_size  = 7,
    };
    ret = get_spi_pins(&bus_config,&interface_config);
    ret = spi_bus_initialize(SPI_ID,&bus_config,SPI_DMA_CH_AUTO);

    //载入字库芯片设备
    ret = spi_bus_add_device(SPI_ID,&interface_config,&board_device_handle->fonts_chip_handle);
    gpio_pad_select_gpio(interface_config.spics_io_num);
    ret = gpio_set_direction(interface_config.spics_io_num,GPIO_MODE_OUTPUT);
   return ret;
}