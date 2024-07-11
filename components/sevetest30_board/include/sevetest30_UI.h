
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

 // 该文件归属701Enti组织，SEVETEST30开发团队应该提供责任性维护，包含对传感器数据，网络API等数据的 整理显示与动画交互工作
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // 敬告：该库自动调用 IWEDA库 SWEDA库 BWEDA库 读取数据，无需任何干涉，因此需要依赖一些库获取缓存变量，图像数据将只在文件函数内生效来节省内存，不会声明
 // 显示UI根据sevetest30实际定制，特别是图像坐标，如果需要改变屏幕大小，建议自行设计修改
 // github: https://github.com/701Enti
 // bilibili: 701Enti

#pragma once

#include <string.h>
#include <stdbool.h>
#include <esp_err.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <driver/i2s.h>
#include "board_ctrl.h"

#define FFT_UI_TASK_CORE (1)     // FFT UI绘制任务核心
#define FFT_UI_TASK_STACK_SIZE (3*1024)//FFT UI绘制任务堆栈大小

#define DSP_MAX_FFT_SIZE    32768     //最大FFT大小

// FFT 点数N 取2的整数次幂,测试发现4096及以下点数运行较稳定,8192及以上任务运行过程中可能崩溃
#define FFT_N_SAMPLES 4096     

#define FFT_DAMPEN_MULTIPLES 50 // FFT 音频数据衰减倍数
#define FFT_VIEW_DATA_MIN 0   // FFT UI视口音频数据最小值
#define FFT_VIEW_DATA_MAX 500// FFT UI视口音频数据最大值
#define FFT_VIEW_WIDTH_MAX 100 //// FFT UI视口宽度最大值

#define CARTOON_PARAM_RENDER_PRECISION 10000 //动画渲染参数计算精度
#define CARTOON_KEY_FRAME_PCT_MAX 10000 //动画关键帧百分位置标识的最大值 1标识动画开始一帧 [CARTOON_KEY_FRAME_PCT_MAX]标识最后一帧

typedef struct UI_color_visual_cfg_t
{
   uint8_t value_max;  // 映射结果的最大值
   int high;           // 较高的值
   int medium;         // 中等的值
   int low;            // 较低的值
   int public_divisor; // 公共除数，抑制数据变化动态，过大过小都会出现全部填充value_max的情况，导致色彩单一,可从 (high - low)/2 开始 调整到可视化动态效果最加即可
   float x_multiples;  // 视口横向缩放倍数,控制视口的横向放大缩小 缩小<1 放大>1 不进行 (如横向视口放大到200% x_multiples = 2)
   float x_move;       // 视口偏移系数  左偏移<0 右偏移>0 不进行 0 (如把视口向右横向偏移屏幕的一半 x_move = 0.5)
} UI_color_visual_cfg_t;

typedef struct music_FFT_UI_cfg_t
{
   int16_t x;///起始位置X坐标
   int16_t y;///起始位置Y坐标
   uint8_t change;///亮度调制
   int width;///视口宽度 图谱显示会自动适应视口宽度
   int height;///显示高度 会裁切不在显示高度的图谱
   UI_color_visual_cfg_t visual_cfg;//频谱颜色可视化配置
}music_FFT_UI_cfg_t;


typedef struct cartoon_ctrl_param_t {
   int32_t x;//x轴坐标
   int32_t y;//y轴坐标
   uint8_t color[3];//颜色
   uint8_t change;//亮度
}cartoon_ctrl_param_t;


//控制对象
typedef struct cartoon_ctrl_object_t {
   uint32_t* pstep;//显示函数自增的step步进值数据位置(可0),hook函数只读     
   int32_t* px;//需要控制的x轴坐标数据位置,hook函数只写     
   int32_t* py;//需要控制的y轴坐标数据位置,hook函数只写 
   uint8_t* pcolor;//需要控制的颜色数据位置,hook函数只写 
   uint8_t* pchange;//需要控制的亮度数据位置,hook函数只写 
}cartoon_ctrl_object_t;


//动画关键帧属性选择
typedef enum {
   //图像层关键帧
   KEY_FRAME_ATTR_LINEAR = 0,//线性关键帧,线性地调整参量直到设置水平
   KEY_FRAME_ATTR_EASE_IN,//缓动关键帧-缓入 至少对应一个标记其前的拟合关键帧,否则被识别为线性关键帧
   KEY_FRAME_ATTR_EASE_OUT,//缓动关键帧-缓出 至少对应一个标记其后的拟合关键帧,否则被识别为线性关键帧
   KEY_FRAME_ATTR_CONTINUOUS,//连续关键帧,使用在两关键帧之间,保持这两关键帧最终效果但期间向连续关键帧偏移   
   KEY_FRAME_ATTR_HOLD,//保持关键帧,开始保持设置的状态直到下一关键帧  
   //非图像层关键帧
   //数据类 
   KEY_FRAME_ATTR_FITTING,//拟合关键帧-创建在缓动关键帧周围(缓入之前,缓出之后)用于进一步拟合缓动参量变化函数,至少一个缓动关键帧对应一个,否则被识别为线性关键帧    
   //操作类
   KEY_FRAME_ATTR_STEGANOGRAPHY,//隐写关键帧-无法被系统识别,其他参数不以key_frame_t规定格式填写,完全不参与渲染,以某种方式隐秘传递数据的伪造关键帧
}key_frame_attr_t;

//隐写关键帧:无法被系统识别,其他参数不以key_frame_t规定格式填写,完全不参与渲染,以某种方式隐秘传递数据的伪造关键帧
//隐写关键帧是动画支持工作下无函数快速操作数据,运行命令的一种方式,隐写关键帧的单位数据类型为key_frame_t,但是对应关系改变,如下
// typedef struct key_frame_t {
//    key_frame_attr_t frame_attribute;//关键帧属性,必须填写为KEY_FRAME_ATTR_STEGANOGRAPHY
//    int32_t percentage;//模式选择,取值[CARTOON_KEY_FRAME_PCT_MAX + 1] - [CARTOON_KEY_FRAME_PCT_MAX + n],表示隐写操作的模式
//    int32_t x;//[隐写数据位x - 4字节]
//    int32_t y;//[隐写数据位y - 4字节]
//    int32_t color[3];//[隐写数据位color[0]color[1]color[2] - 每个4字节]
//    int32_t change;//[隐写数据位change - 4字节]
//    int32_t step_buf;//[隐写数据位step_buf - 4字节]
// }key_frame_t;
//percentage参数的标定使得可以运行多种隐写模式,实现多种操作,隐写模式类型如下,枚举选择需要的隐写模式


typedef enum{
 //隐写模式:映射 - 映射内存中的数据以填充任意关键帧
 //x y color[3] change 存储要读取的内存地址(为NULL的不读取),映射根据x(隐写数据位x)地址读到的数据到目标关键帧数据单元的x成员上,以此类推
 //step_buf不同,存储的是要映射到的对象关键帧在key_frame_database的角标
 STEGANOGRAPHY_MODE_MAPPING = CARTOON_KEY_FRAME_PCT_MAX + 1, 


}steganography_mode_t;//隐写操作模式类型



//动画关键帧类型
typedef struct key_frame_t {
   key_frame_attr_t frame_attribute;//关键帧属性
   int32_t percentage;//关键帧百分位置,取值0-[CARTOON_KEY_FRAME_PCT_MAX],表示播放步数百分比(末两位数表示小数部分)
   int32_t x;//x轴坐标
   int32_t y;//y轴坐标
   int32_t color[3];//颜色
   int32_t change;//亮度
   int32_t step_buf;//关键帧步位置缓存,该关键帧在第step步映射(可为0)    
}key_frame_t;



//动画计划类型
typedef struct cartoon_plan_t {
   key_frame_t* key_frame_database;//动画计划关键帧数据库   
   bool x_en;//启用x轴坐标控制
   bool y_en;//启用y轴坐标控制
   bool color_en;//启用颜色控制
   bool change_en;//启用亮度控制
   uint32_t total_key_frame;//实际总关键帧数缓存(私有缓存,用户函数请勿赋值)
   uint32_t total_step_buf;//实际总步数缓存(私有缓存,用户函数请勿赋值) 
}cartoon_plan_t;



//动画生成回调函数类型,根据动画计划转换为控制参量表格式
typedef void(*cartoon_create_callback_func_t)(int,uint32_t);

//动画控制钩子类型,用于控制和运用动画参量
typedef void(*cartoon_ctrl_hook_func_t)(int,cartoon_ctrl_object_t*);


typedef struct cartoon_support_t {
   cartoon_plan_t cartoon_plan;//动画计划,需要提前设置
   cartoon_ctrl_param_t* ctrl_param_list;//控制参量表,不同处理策略的控制参量表,钩子和回调都不一
   cartoon_create_callback_func_t create_callback;//动画生成回调,不同处理策略的控制参量表,钩子和回调都不一
   cartoon_ctrl_hook_func_t ctrl_hook;//动画控制的钩子函数,不同处理策略的控制参量表,钩子和回调都不一 
}cartoon_support_t;

typedef cartoon_support_t* cartoon_handle_t;


typedef enum {
   CARTOON_RUN_MODE_PRE_RENDER = 0,//预渲染,在动画运行前保存每步图像到参量表,适合时长短且关键帧数量非常多的动画计划(比较消耗内存)
   CARTOON_RUN_MODE_REAL_TIME_RENDER,//实时渲染,在动画运行时,钩子函数直接通过关键帧的数据实时渲染,适合简单常规的动画计划(动画运行时比较占用CPU)
}cartoon_run_mode_t;



extern bool volatile sevetest30_fft_ui_running_flag;

// 数据可视化函数
void data_to_color(int data, UI_color_visual_cfg_t* visual_cfg, uint8_t* high, uint8_t* comfort, uint8_t* low);

void temp_to_color(int temp, uint8_t value_max, uint8_t* high, uint8_t* comfort, uint8_t* low);

//动画支持适配函数-模式:预渲染

void _PRE_RENDER_create_callback(cartoon_handle_t handle, int64_t total_step);
void _PRE_RENDER_ctrl_hook(cartoon_handle_t handle, cartoon_ctrl_object_t* object);

//动画支持通用函数

cartoon_handle_t cartoon_new(cartoon_run_mode_t run_mode,bool en_x,bool en_y,bool en_color,bool en_change,int key_frame_max);
void add_new_key_frame(cartoon_handle_t handle,key_frame_attr_t attr,uint32_t pct,int32_t x,int32_t y,uint8_t color[3],uint8_t change);




void weather_UI_1(int16_t x, int16_t y, uint8_t change);

void time_UI_1(int16_t x, int16_t y, uint8_t change);

void time_UI_2(int16_t x, int16_t y, uint8_t change);

void music_FFT_UI_draw(music_FFT_UI_cfg_t* UI_cfg);

void main_UI_1();

void music_FFT_UI_start(music_FFT_UI_cfg_t* UI_cfg, UBaseType_t priority);


