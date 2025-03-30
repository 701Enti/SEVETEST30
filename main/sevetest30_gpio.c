
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

 // 包含一些sevetest30的GPIO配置工作(内部+扩展)，主要为了TCA6416A控制函数的解耦
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // 敬告：文件本体只针对sevetest30的硬件设计，该库代码中的配置是否合理会影响到设备能否正常运行，请谨慎修改
 //      此处没有使用TCA6416A的RESET引脚，但并不代表RESET可以悬空，请将其上拉到VCC,并在RESET连接一个1uF左右电容到GND(这不是对TCA6416A的使用建议)
 // github: https://github.com/701Enti
 // bilibili: 701Enti

#include "sevetest30_gpio.h"
#include "board_def.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"

ext_io_ctrl_t ext_io_ctrl = {
  .auto_read_EN = true,
  .auto_read_INT = false,
};

TCA6416A_mode_t* P_ext_io_mode_data = NULL; //扩展IO输入输出模式
TCA6416A_level_t* P_ext_io_value_data = NULL;//扩展IO电平信息，写入和回读通用

//中断参数传flag地址以保证flag值同步，该变量存储了这个地址，作为ISR参数
uint32_t P_ext_io_auto_read_flag;

static xQueueHandle tca6416a_int_queue = NULL;

static void IRAM_ATTR tca6416a_int_isr_handler(void* arg) {
  uint32_t P_auto_read_flag = (uint32_t)arg;
  xQueueSendFromISR(tca6416a_int_queue, &P_auto_read_flag, NULL);
}


static void tca6416a_int_task(void* arg) {
  bool flag_buf = true;
  uint32_t P_auto_read_flag;
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(500));
    if (xQueueReceive(tca6416a_int_queue, &P_auto_read_flag, portMAX_DELAY)) {
      //将自动读取标志值的地址作为ISR参数进行传输
      flag_buf = *(bool*)P_auto_read_flag;
      if (flag_buf) {
        ext_io_ctrl.auto_read_INT = true;
      }
    }
  }
}



/// @brief 扩展GPIO电平值服务，读写通用，当有电平变化，系统中断触发会自动调用该函数，所以主要用来写操作
/// @brief 写：先对ext_io_value_data这个全局公共结构体变量修改值，等待系统大循环调用该函数写入IO电平
/// @brief 读：（当中断读取使能，只要电平变化系统中断触发，将全自动读取，一般无需调用）如果需要主动读取：先调用该函数刷新ext_io_value_data，之后直接读取ext_io_value_data内的成员变量值 */
/// @return [ESP_OK 成功]  
/// @return [ESP_ERR_INVALID_ARG 参数错误] 
/// @return [ESP_FAIL 发送命令时发现问题, TCA6416A未应答] 
/// @return [ESP_ERR_INVALID_STATE I2C driver 未安装或没有运行在主机模式] 
/// @return [ESP_ERR_TIMEOUT 操作超时因为总线忙]
esp_err_t ext_io_level_service() {
  return TCA6416A_gpio_service(P_ext_io_value_data);
}



/// @brief 扩展GPIO输入输出模式服务
/// @brief 写：先对ext_io_mode_data这个全局公共结构体变量写入需要修改的值，再调用该函数控制IO输入输出模式
/// @brief 读：直接读取ext_io_mode_data内的成员变量值，不需要调用该函数 */
/// @return [ESP_OK 成功]  
/// @return [ESP_ERR_INVALID_ARG 参数错误] 
/// @return [ESP_FAIL 发送命令时发现问题, TCA6416A未应答] 
/// @return [ESP_ERR_INVALID_STATE I2C driver 未安装或没有运行在主机模式] 
/// @return [ESP_ERR_TIMEOUT 操作超时因为总线忙]
esp_err_t ext_io_mode_service() {
  //在将外部IO模式由输出模式转向输入模式时，可能发生意外中断问题，通过flag在模式设置时屏蔽中断自动读取，可以代价较低地避免可能性极小的重复读取问题的发生
  ext_io_ctrl.auto_read_EN = false;
  esp_err_t ret = TCA6416A_mode_set(P_ext_io_mode_data);
  ext_io_ctrl.auto_read_EN = true;
  return ret;
}



/// @brief 扩展GPIO(TCA6416A)重置到默认，在gpio_init函数调用时该函数就会被执行
/// @param p_mode 缓存位置-模式配置(比如定义一个TCA6416A_mode_t结构体作为缓存位置导入,之后可直接改变结构体成员值来控制扩展IO)
/// @param p_level 缓存位置-电平数据(比如定义一个TCA6416A_level_t结构体作为缓存位置导入,之后可直接改变结构体成员值来控制扩展IO)
/// @return [ESP_OK 成功]  
/// @return [ESP_ERR_INVALID_ARG 参数错误] 
/// @return [ESP_FAIL 发送命令时发现问题, TCA6416A未应答] 
/// @return [ESP_ERR_INVALID_STATE I2C driver 未安装或没有运行在主机模式] 
/// @return [ESP_ERR_TIMEOUT 操作超时因为总线忙]
esp_err_t ext_io_reset_to_default(TCA6416A_mode_t* p_mode, TCA6416A_level_t* p_level) {
  const char* TAG = "ext_io_reset_to_default";

  //此处没有使用TCA6416A的RESET引脚，但并不代表RESET可以悬空
  //请将其上拉到VCC,并在RESET连接一个1uF左右电容到GND(这不是对TCA6416A的使用建议)
  TCA6416A_mode_t mode_buf = TCA6416A_DEFAULT_CONFIG_MODE;
  TCA6416A_level_t value_buf = TCA6416A_DEFAULT_CONFIG_VALUE;

  *p_mode = mode_buf;
  *p_level = value_buf;
  P_ext_io_mode_data = p_mode;
  P_ext_io_value_data = p_level;

  esp_err_t ret = ESP_OK;
  ret = ext_io_mode_service();
  ESP_RETURN_ON_ERROR(ret, TAG, "重置扩展GPIO电平到默认时发现问题");
  ret = ext_io_level_service();
  ESP_RETURN_ON_ERROR(ret, TAG, "重置扩展GPIO电平到默认时发现问题");
  return ESP_OK;
}



/// @brief 全局GPIO初始化,包括内部IO和扩展IO
/// @param p_mode 缓存位置-模式配置(比如定义一个TCA6416A_mode_t结构体作为缓存位置导入,之后可直接改变结构体成员值来控制扩展IO)
/// @param p_level 缓存位置-电平数据(比如定义一个TCA6416A_level_t结构体作为缓存位置导入,之后可直接改变结构体成员值来控制扩展IO)
/// @return [ESP_OK 成功]  
/// @return [ESP_ERR_INVALID_ARG 输入参数错误 / 选择了错误的GPIO] 
/// @return [ESP_FAIL 发送命令时发现问题, TCA6416A未应答] 
/// @return [ESP_ERR_INVALID_STATE ISR服务已经安装 / ISR服务安装失败 /  I2C driver 未安装或没有运行在主机模式] 
/// @return [ESP_ERR_TIMEOUT 操作超时因为总线忙]
/// @return [ESP_ERR_NO_MEM 安装ISR服务时发现内存不足]
/// @return [ESP_ERR_NOT_FOUND 没有找到特定FLAG的空闲interrupt]
esp_err_t sevetest30_gpio_init(TCA6416A_mode_t* p_ext_mode, TCA6416A_level_t* p_ext_value)
{
  const char* TAG = "sevetest30_gpio_init";
  esp_err_t ret = ESP_OK;

  //电池输入控制GPIO
  gpio_config_t battery_in_ctrl_io_config = {
  .pin_bit_mask = 1ULL << BAT_IN_CTRL_IO,
  .mode = GPIO_MODE_OUTPUT,
  .pull_up_en = GPIO_PULLUP_DISABLE,
  .pull_down_en = GPIO_PULLDOWN_DISABLE,
  .intr_type = GPIO_INTR_DISABLE,
  };
  ret = gpio_config(&battery_in_ctrl_io_config);//配置GPIO
  ESP_RETURN_ON_ERROR(ret, TAG, "配置电池输入控制GPIO时发现问题");
  ret = gpio_set_level(BAT_IN_CTRL_IO, 1);//设置GPIO初始电平
  ESP_RETURN_ON_ERROR(ret, TAG, "设置电池输入控制GPIO初始电平时发现问题");

  //TCA6416A的INT信号GPIO
  gpio_config_t TCA6416A_int_config = {
  .pin_bit_mask = 1ULL << TCA6416A_IO_INT,
  .mode = GPIO_MODE_INPUT,
  .pull_up_en = GPIO_PULLUP_DISABLE,
  .pull_down_en = GPIO_PULLDOWN_DISABLE,
  .intr_type = GPIO_INTR_NEGEDGE,
  };
  ret = gpio_config(&TCA6416A_int_config);//配置GPIO
  ESP_RETURN_ON_ERROR(ret, TAG, "配置TCA6416A的INT信号GPIO时发现问题");

  //INT中断配置
  tca6416a_int_queue = xQueueCreate(10, sizeof(uint32_t));//队列创建
  xTaskCreatePinnedToCore(tca6416a_int_task, "tca6416a_int_task", 2048, NULL, EXT_IO_READ_EVT_PRIO, NULL, EXT_IO_READ_EVT_CORE);//中断识别任务创建
  ret = gpio_install_isr_service(EXT_IO_READ_INTR_FLAG);//安装GPIO ISR服务
  ESP_RETURN_ON_ERROR(ret, TAG, "安装TCA6416A的INT信号GPIO的ISR服务时发现问题");
  P_ext_io_auto_read_flag = &ext_io_ctrl.auto_read_EN;//将自动读取标志值的地址作为ISR参数进行传输
  ret = gpio_isr_handler_add(TCA6416A_IO_INT, tca6416a_int_isr_handler, (void*)P_ext_io_auto_read_flag);//为选定的GPIO添加ISR句柄
  ESP_RETURN_ON_ERROR(ret, TAG, "添加TCA6416A的INT信号GPIO的ISR句柄时发现问题");

  // 此处没有使用TCA6416A的RESET引脚，但并不代表RESET可以悬空，请将其上拉到VCC,并在RESET连接一个1uF左右电容到GND(这不是对TCA6416A的使用建议)

  ret = ext_io_reset_to_default(p_ext_mode, p_ext_value);//扩展gpio初始化
  ESP_RETURN_ON_ERROR(ret, TAG, "扩展GPIO重置到默认时发现问题");

  return ESP_OK;
}