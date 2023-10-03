// 该文件由701Enti整合，包含一些sevetest30的GPIO配置工作(内部+扩展)，主要为了TCA6416A控制函数的解耦
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体只针对sevetest30的硬件设计，该库代码中的配置是否合理会影响到设备能否正常运行，请谨慎修改
//      此处没有使用TCA6416A的RESET引脚，但并不代表RESET可以悬空，请将其上拉到VCC,并在RESET连接一个1uF左右电容到GND(这不是对TCA6416A的使用建议)
//      库包含一个中断，输入方式的扩展IO电平变化，自动读取将会触发，默认开启,中断触发会将 read_ext_io 置为 1，需要外部大循环检测到进行处理并置为0，这主要为了防止堆栈溢出
//      如果外部程序不希望触发自动读取，可以直接通过 ext_io_auto_read_flag = false; 进行直接性屏蔽,结束后必须 ext_io_auto_read_flag = true; 置回
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.7.16

#include "sevetest30_gpio.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"


//在将外部IO模式由输出模式转向输入模式时，可能发生意外中断问题，通过flag在模式设置时屏蔽中断自动读取，可以代价较低地避免可能性极小的重复读取问题的发生
//另外的，如果外部程序不希望触发自动读取，可以直接通过 ext_io_auto_read_flag = false; 进行屏蔽,结束后必须 ext_io_auto_read_flag = true; 置回
bool ext_io_auto_read_flag = true;

//中断触发会将 read_ext_io 置为 1，需要外部大循环检测到进行处理并置为0
bool read_ext_io = true;

//中断参数传flag地址以保证flag值同步，该变量存储了这个地址，作为ISR参数
uint32_t P_ext_io_auto_read_flag;

TCA6416A_mode_t  ext_io_mode_data; //公共使用 扩展IO输入输出模式
TCA6416A_value_t ext_io_value_data;//公共使用 扩展IO电平信息

static xQueueHandle tca6416a_int_queue = NULL;

static void IRAM_ATTR tca6416a_int_isr_handler(void* arg){
  uint32_t P_auto_read_flag = (uint32_t)arg;
  xQueueSendFromISR(tca6416a_int_queue,&P_auto_read_flag,NULL);
}

/* 扩展gpio电平值服务，读写通用，但当有电平变化，系统中断触发会自动调用该函数，所以主要用来写操作
写：先对ext_io_value_data这个全局公共结构体变量修改值，等待系统大循环调用该函数写入IO电平
读：（当中断读取使能，只要电平变化系统中断触发，将全自动读取，一般无需调用）如果需要主动读取：先调用该函数刷新ext_io_value_data，之后直接读取ext_io_value_data内的成员变量值 */
void ext_io_value_service(){
  TCA6416A_gpio_service(&ext_io_value_data);
}

/* 扩展gpio输入输出模式服务
写：先对ext_io_mode_data这个全局公共结构体变量写入需要修改的值，再调用该函数控制IO输入输出模式
读：直接读取ext_io_mode_data内的成员变量值，不需要调用该函数 */
void ext_io_mode_service(){
  //在将外部IO模式由输出模式转向输入模式时，可能发生意外中断问题，通过flag在模式设置时屏蔽中断自动读取，可以代价较低地避免可能性极小的重复读取问题的发生
  ext_io_auto_read_flag = false;
  TCA6416A_mode_set(&ext_io_mode_data);
  ext_io_auto_read_flag = true;
}

// 扩展gpio(TCA6416A)初始化，在gpio_init函数调用时该函数就会被执行
void ext_io_init(){

  //此处没有使用TCA6416A的RESET引脚，但并不代表RESET可以悬空，请将其上拉到VCC,并在RESET连接一个1uF左右电容到GND(这不是对TCA6416A的使用建议)
  // //RESET操作触发需要拉低RESET线
  // gpio_set_level(TCA6416A_IO_RESET,0);  
  // gpio_set_level(TCA6416A_IO_RESET,1);

  TCA6416A_mode_t mode_buf = TCA6416A_DEFAULT_CONFIG_MODE;
  TCA6416A_value_t value_buf = TCA6416A_DEFAULT_CONFIG_VALUE;  
  ext_io_mode_data = mode_buf;
  ext_io_value_data = value_buf;

  ext_io_mode_service();
  ext_io_value_service();
}

static void tca6416a_int_task(void* arg){
  bool flag_buf = true;
  uint32_t P_auto_read_flag;
  while(true){ 
    vTaskDelay(pdMS_TO_TICKS(10));
    if(xQueueReceive(tca6416a_int_queue,&P_auto_read_flag,portMAX_DELAY)){
      //将自动读取标志值的地址作为ISR参数进行传输
      flag_buf = *(bool*)P_auto_read_flag;
      if(flag_buf){
        read_ext_io = 1;
      }
    }
  }
}

//gpio初始化,包括内部IO和扩展IO
void sevetest30_gpio_init()
{
   //按键
  gpio_config_t turn_io_config = { 
  .pin_bit_mask = 1ULL << TURN_GPIO,
  .mode = GPIO_MODE_INPUT_OUTPUT_OD,  
  .pull_up_en = GPIO_PULLUP_DISABLE, 
  .pull_down_en =GPIO_PULLDOWN_DISABLE,
  .intr_type =GPIO_INTR_DISABLE,
  };
  gpio_config(&turn_io_config);//传参
  gpio_set_level(TURN_GPIO,0);//设置IO,初始电平


  //TCA6416A INT引脚
  gpio_config_t TCA6416A_int_config = { 
  .pin_bit_mask = 1ULL << TCA6416A_IO_INT,
  .mode = GPIO_MODE_INPUT,  
  .pull_up_en = GPIO_PULLUP_DISABLE, 
  .pull_down_en =GPIO_PULLDOWN_DISABLE,
  .intr_type =GPIO_INTR_NEGEDGE,
  };
  gpio_config(&TCA6416A_int_config);//传参


  //INT中断配置
  tca6416a_int_queue = xQueueCreate(10,sizeof(uint32_t));//队列创建
  xTaskCreate(tca6416a_int_task,"tca6416a_int_task",2048,NULL,12,NULL);//中断识别任务创建，选择了第12优先级（中优先级）
  gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);//安装服务时选择了第3优先级（中优先级）
  P_ext_io_auto_read_flag = &ext_io_auto_read_flag;//将自动读取标志值的地址作为ISR参数进行传输
  gpio_isr_handler_add(TCA6416A_IO_INT,tca6416a_int_isr_handler,(void*)P_ext_io_auto_read_flag);//为选定的GPIO添加ISR句柄

  //此处没有使用TCA6416A的RESET引脚，但并不代表RESET可以悬空，请将其上拉到VCC,并在RESET连接一个1uF左右电容到GND(这不是对TCA6416A的使用建议)

  ext_io_init();//扩展gpio初始化
}