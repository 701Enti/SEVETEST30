
/*
 * 701Enti MIT License
 *
 * Copyright © 2024 <701Enti organization>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the “Software”),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// 包含对传感器数据，网络API等数据的 整理显示与动画交互工作
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：该库自动调用 IWEDA库 SWEDA库 BWEDA库
// 读取数据，无需任何干涉，因此需要依赖一些库获取缓存变量，图像数据将只在文件函数内生效来节省内存，不会声明
// 敬告: 如果图案对象是向右移动的,则x轴坐标增加,向左则减少,
// 但注意,图案向上移动y轴坐标是减少的,向下是增加的
// 显示UI根据sevetest30实际定制，特别是图像坐标，如果需要改变屏幕大小，建议自行设计修改
// github: https://github.com/701Enti

#include "sevetest30_config.h"
#include <audio_mem.h>
#include <board_def.h>
#include <default_UI_icon_data.h>
#include <esp_dsp.h>
#include <esp_log.h>
#include <esp_random.h>
#include <math.h>
#include <sevetest30_IWEDA.h>
#include <sevetest30_LedArray.h>
#include <sevetest30_SWEDA.h>
#include <sevetest30_UI.h>
#include <sevetest30_sound.h>
#include <sevetest30_touch.h>
#include <soc/soc_caps.h>

#define LOW_TEMP_MULTIPLE 20 // 低于BLUE_TEMP多少倍将达到设定的最高白色亮度

bool weather_change_flag = 0;
float *src_data = NULL; // 源数据
float *wind = NULL;     // 窗口系数

uint8_t unit_led_color[FFT_VIEW_WIDTH_MAX * 3] = {0};
uint8_t unit_led_height[FFT_VIEW_WIDTH_MAX] = {0};

__attribute__((aligned(16))) float y_cf[FFT_N_SAMPLES * 2];

void music_FFT_UI_refresh_Task(music_FFT_UI_handle_t handle);
void steganography_service(cartoon_handle_t handle, int idx);

/// @brief
/// 帧参量渲染函数-x轴坐标,根据关键帧渲染得到所有帧的x轴坐标数据保存到param_list_buf
/// @param plan
/// 导入动画计划,如果是步数不定动画,需要外部预处理好关键帧的实际步位置
/// @param param_list_buf 选择将写入到的控制参量表,需要外部申请好足够缓存
void frame_param_render_x(cartoon_plan_t *plan,
                          cartoon_ctrl_param_t *param_list_buf) {
  const char *TAG = "frame_param_render";
  if (!plan || !param_list_buf) {
    ESP_LOGE(TAG, "导入了为空的指针");
    return;
  }

  int64_t next_dp = 0; // 下一帧增加/减少的参量值(斜率)

  // 1-找到所有线性关键帧和缓动关键帧,先计算间隔帧将拟合的一次函数,渲染间隔帧

  // 初始值赋值-1而不是0以防止k=0就符合关键帧判断条件,之后使得(k_buf ==
  // 0)两次执行丢失关键帧数据的情况
  int k_buf = -1; // 缓存上一个焦点关键帧位置
  for (int k = 0; k < plan->total_key_frame; k++) {
    if (plan->key_frame_database[k].frame_attribute == KEY_FRAME_ATTR_LINEAR ||
        plan->key_frame_database[k].frame_attribute == KEY_FRAME_ATTR_EASE_IN ||
        plan->key_frame_database[k].frame_attribute ==
            KEY_FRAME_ATTR_EASE_OUT) {
      if (k_buf ==
          -1) { // 初始值赋值-1而不是0以防止k=0就符合关键帧判断条件,之后使得(k_buf
                // == 0)两次执行丢失关键帧数据的情况
        k_buf = k; // 第一个线性关键帧或缓动关键帧
      } else {

        // 计算参量变化斜率,遍历填充两关键帧之间的帧，保存参量到param_list_buf
        if (plan->key_frame_database[k].step_buf >=
            plan->key_frame_database[k_buf].step_buf) {
          next_dp = ((plan->key_frame_database[k].x -
                      plan->key_frame_database[k_buf].x) *
                     CARTOON_PARAM_RENDER_PRECISION) /
                    (int64_t)(plan->key_frame_database[k].step_buf -
                              plan->key_frame_database[k_buf].step_buf);
          for (int s = plan->key_frame_database[k_buf].step_buf;
               s <= plan->key_frame_database[k].step_buf; s++) {
            param_list_buf[s].x =
                plan->key_frame_database[k_buf].x +
                next_dp * (s - plan->key_frame_database[k_buf].step_buf) /
                    CARTOON_PARAM_RENDER_PRECISION;
          }
        } else { // 变化量是末始值减初始值,这里step大的为末始关键帧,不是根据param_list_buf的角标前后判断
          next_dp = ((plan->key_frame_database[k_buf].x -
                      plan->key_frame_database[k].x) *
                     CARTOON_PARAM_RENDER_PRECISION) /
                    (int64_t)(plan->key_frame_database[k_buf].step_buf -
                              plan->key_frame_database[k].step_buf);
          for (int s = plan->key_frame_database[k].step_buf;
               s <= plan->key_frame_database[k_buf].step_buf; s++) {
            param_list_buf[s].x =
                plan->key_frame_database[k].x +
                next_dp * (s - plan->key_frame_database[k].step_buf) /
                    CARTOON_PARAM_RENDER_PRECISION;
          }
        }

        k_buf = k;
      }
    }
  }

  // 2-找到缓动关键帧,如果缓入其前或缓出其后紧接拟合关键帧,拟合该缓动关键帧的缓动曲线
  // 以此调整该缓动关键帧附近的有效缓动帧数据(如果无紧接拟合关键帧,忽略)

  // 3-找到连续关键帧,将其视为线性关键帧与相邻两线性关键帧或缓动关键帧(视为线性关键帧)分别拟合两条一次函数
  // 得到偏移帧,与之前间隔帧取参数平均值(如果无相邻两线性关键帧或缓动关键帧,忽略)

  // 4-找到保持关键帧与最近的一关键帧(线性关键帧或拟合关键帧或缓动关键帧),向两者所有间隔帧填充保持关键帧数据
  // 如果其后找不到这样的关键帧,填充直到最后一帧
}

/// @brief 预渲染运行模式-动画生成回调
/// @param handle 动画句柄
/// @param total_step 实际显示总细分步个数
void _PRE_RENDER_create_callback(cartoon_handle_t handle, int32_t total_step) {
  const char *TAG = "_PRE_RENDER_create_callback";
  if (!handle) {
    ESP_LOGE(TAG, "无效的空动画句柄");
    return;
  }

  if (total_step <= 0) {
    ESP_LOGE(TAG, "总步数异常的无效动画");
    return;
  } else {
    handle->cartoon_plan.total_step_buf = total_step;
  }
  // 申请控制参量表缓存
  cartoon_ctrl_param_t *ctrl_param_list = NULL;
  ctrl_param_list = (cartoon_ctrl_param_t *)malloc(
      sizeof(cartoon_ctrl_param_t) * handle->cartoon_plan.total_step_buf);
  if (!ctrl_param_list) {
    ESP_LOGE(TAG, "申请ctrl_param_list资源发现问题");
    return;
  }
  memset(ctrl_param_list, 0,
         sizeof(cartoon_ctrl_param_t) * handle->cartoon_plan.total_step_buf);

  // 计算所有关键帧的所在步数
  for (int idx = 0; idx < handle->cartoon_plan.total_key_frame; idx++) {
    if (handle->cartoon_plan.key_frame_database[idx].percentage >= 1 &&
        handle->cartoon_plan.key_frame_database[idx].percentage <=
            CARTOON_KEY_FRAME_PCT_MAX) {
      // 关键帧数据库渲染区
      handle->cartoon_plan.key_frame_database[idx]
          .step_buf // handle->cartoon_plan.total_step_buf-1 为
                    // step_buf的最大允许值
          = handle->cartoon_plan.key_frame_database[idx].percentage *
            (handle->cartoon_plan.total_step_buf - 1) /
            CARTOON_KEY_FRAME_PCT_MAX;
    } else {
      // 关键帧数据库非渲染区
      // 隐写关键帧
      if (handle->cartoon_plan.key_frame_database[idx].frame_attribute ==
          KEY_FRAME_ATTR_STEGANOGRAPHY) {
        steganography_service(handle, idx);
      }
    }
  }

  // 渲染得到所有帧数据保存到控制参量表
  if (handle->cartoon_plan.x_en)
    frame_param_render_x(&handle->cartoon_plan, ctrl_param_list);

  handle->ctrl_param_list = ctrl_param_list;
}

/// @brief 预渲染运行模式-动画运行控制钩子
/// @param handle 动画句柄
/// @param object 控制对象，需要显示函数填写信息再导入
void _PRE_RENDER_ctrl_hook(cartoon_handle_t handle,
                           cartoon_ctrl_object_t *object) {
  const char *TAG = "_PRE_RENDER_ctrl_hook";
  if (handle) {
    if (handle->cartoon_plan.x_en) {
      *(object->px) = handle->ctrl_param_list[*(object->pstep)].x;
    }
    if (handle->cartoon_plan.y_en) {
      *(object->py) = handle->ctrl_param_list[*(object->pstep)].y;
    }
    if (handle->cartoon_plan.color_en) {
      (object->pcolor)[0] = handle->ctrl_param_list[*(object->pstep)].color[0];
      (object->pcolor)[1] = handle->ctrl_param_list[*(object->pstep)].color[1];
      (object->pcolor)[2] = handle->ctrl_param_list[*(object->pstep)].color[2];
    }

    // 动画显示完毕,释放参量表内存
    if (*(object->pstep) == handle->cartoon_plan.total_step_buf - 1) {
      free(handle->ctrl_param_list);
      handle->ctrl_param_list = NULL;
    }
  } else {
    ESP_LOGE(TAG, "无效的空动画句柄");
    return;
  }
}

/// @brief 创建新动画
/// @param run_mode 运行模式，这是一个枚举类型
/// @param en_x 打开x轴数据的渲染开关
/// @param en_y 打开y轴数据的渲染开关
/// @param en_color 打开颜色数据的渲染开关
/// @param key_frame_max 最大关键帧个数，必须大于0
/// @return 动画的全局句柄
cartoon_handle_t cartoon_new(cartoon_run_mode_t run_mode, bool en_x, bool en_y,
                             bool en_color, int key_frame_max) {
  const char *TAG = "cartoon_new";

  if (key_frame_max <= 0) {
    ESP_LOGE(TAG, "输入参数key_frame_max必须大于0");
    return NULL;
  }

  // 申请动画支持缓存
  cartoon_support_t *support = NULL;
  support = (cartoon_support_t *)malloc(sizeof(cartoon_support_t));
  if (!support) {
    ESP_LOGE(TAG, "申请support资源发现问题");
    return NULL;
  }
  memset(support, 0, sizeof(cartoon_support_t));

  // 确定回调与钩子方案
  switch (run_mode) {
  case CARTOON_RUN_MODE_PRE_RENDER:
    support->create_callback =
        (cartoon_create_callback_func_t)_PRE_RENDER_create_callback;
    support->ctrl_hook = (cartoon_ctrl_hook_func_t)_PRE_RENDER_ctrl_hook;
    break;
    // case CARTOON_RUN_MODE_REAL_TIME_RENDER:
    //     break;

  default:
    ESP_LOGE(TAG, "无法识别的运行类型");
    free(support);
    support = NULL;
    return NULL;
    break;
  }

  // 申请关键帧数据库缓存
  key_frame_t *kf_db = NULL;
  kf_db = (key_frame_t *)malloc(sizeof(key_frame_t) * key_frame_max);
  if (!kf_db) {
    ESP_LOGE(TAG, "申请key_frame_database资源发现问题");
    free(support);
    support = NULL;
    return NULL;
  }
  memset(kf_db, 0, sizeof(key_frame_t) * key_frame_max);
  support->cartoon_plan.key_frame_database = kf_db;

  // 打开渲染开关
  support->cartoon_plan.x_en = en_x;
  support->cartoon_plan.y_en = en_y;
  support->cartoon_plan.color_en = en_color;
  return (cartoon_handle_t)support;
}

/// @brief 删除动画
/// @param handle 动画句柄
void cartoon_delete(cartoon_handle_t handle) {
  const char *TAG = "cartoon_delete";
  if (handle) {
    if (handle->cartoon_plan.key_frame_database) {
      free(handle->cartoon_plan.key_frame_database);
      handle->cartoon_plan.key_frame_database = NULL;
    }
    free(handle);
    handle = NULL;
  } else {
    ESP_LOGE(TAG, "无效的空动画句柄");
    return;
  }
}

/// @brief 添加新的关键帧到动画中(建议先添加渲染关键帧,后添加非渲染关键帧)
/// @param handle 动画句柄
/// @param attr 关键帧属性
/// @param pct
/// 对渲染关键帧:关键帧百分位置,取值0-[CARTOON_KEY_FRAME_PCT_MAX],表示播放步数百分比(末两位数表示小数部分)
/// 对非渲染关键帧:含义特殊,需要参考上下文
/// @param step 对渲染关键帧:无效参数填写任意值
/// 对非渲染关键帧:含义特殊,需要参考上下文
/// @param x x轴坐标
/// @param y y轴坐标
/// @param color 颜色
/// @param change 亮度0-100
/// @return 这次添加完成的关键帧的角标位置
uint32_t add_new_key_frame(cartoon_handle_t handle, key_frame_attr_t attr,
                           uint32_t pct, int32_t step, int32_t x, int32_t y,
                           uint8_t color[3]) {
  const char *TAG = "add_new_key_frame";
  if (handle) {
    handle->cartoon_plan
        .key_frame_database[handle->cartoon_plan.total_key_frame]
        .frame_attribute = attr;
    handle->cartoon_plan
        .key_frame_database[handle->cartoon_plan.total_key_frame]
        .percentage = pct;
    handle->cartoon_plan
        .key_frame_database[handle->cartoon_plan.total_key_frame]
        .step_buf = step;
    handle->cartoon_plan
        .key_frame_database[handle->cartoon_plan.total_key_frame]
        .x = x;
    handle->cartoon_plan
        .key_frame_database[handle->cartoon_plan.total_key_frame]
        .y = y;
    if (color) {
      handle->cartoon_plan
          .key_frame_database[handle->cartoon_plan.total_key_frame]
          .color[0] = color[0];
      handle->cartoon_plan
          .key_frame_database[handle->cartoon_plan.total_key_frame]
          .color[1] = color[1];
      handle->cartoon_plan
          .key_frame_database[handle->cartoon_plan.total_key_frame]
          .color[2] = color[2];
    }
    handle->cartoon_plan
        .total_key_frame++; // 自增总关键帧数,这将使得下次调用该函数操作的是下一关键帧
    return handle->cartoon_plan.total_key_frame -
           1; // 这次添加完成的关键帧的角标位置
  } else {
    ESP_LOGE(TAG, "无效的空动画句柄");
    return 0;
  }
}

// 将温度数据体现在颜色上(可以通过Kconfig修改，人体炎热寒热和舒适的对应温度)
// 摄氏温度值+最大颜色分量大小(一般取255，这不会影响亮度大小)+输出按顺序是 R G
// B三个分量 0 - (value_max)
// 以下函数是比较低级的数据可视化，对数据效果有很大损耗
// 我们通过temp-26并获取绝对值，来得到temp与26的差值，再映射到0-1之间并用1去减它，
// 再乘上 value_max 得到绿色分量的值，这样越接近26，绿色分量的值更大，更显示绿色
// 而对于红色则是用temp-40，蓝色则用temp-12，按相同算法。
// 如果是低于12摄氏度，则颜色始终为纯白色，如果高于40，始终为纯红色。
// 为了颜色的合理与均匀，我们不得不设40和26和12摄氏度作为R G B
// 分界阈值，损失12摄氏度以下可视化的机会，当然，这可以通过Kconfig修改适应不同气候

/// @brief
/// 将温度数据体现在颜色上(可以通过Kconfig修改，人体炎热寒热和舒适的对应温度)
/// @param temp 温度值
/// @param value_max 最大映射值
/// @param high 高值分量 越趋近预设高温 向高值分量侧倾
/// @param comfort 适宜分量 越趋近预设适宜温度 向适宜分量侧倾
/// @param low 低值分量 越趋近预设低温 向低值分量侧倾
void temp_to_color(int temp, uint8_t value_max, uint8_t *high, uint8_t *comfort,
                   uint8_t *low) {
  float value_buf = 0;

  // 如果在自由变化范围
  if (temp >= CONFIG_LOW_TEMP && temp <= CONFIG_HIGH_TEMP) {
    value_buf = abs(temp - CONFIG_HIGH_TEMP);
    *high = (1 - value_buf / CONFIG_PUBLIC_DIVISOR) * value_max;

    value_buf = abs(temp - CONFIG_COMFORT_TEMP);
    *comfort = (1 - value_buf / CONFIG_PUBLIC_DIVISOR) * value_max;

    value_buf = abs(temp - CONFIG_LOW_TEMP);
    *low = (1 - value_buf / CONFIG_PUBLIC_DIVISOR) * value_max;
  }
  // 高温
  if (temp > CONFIG_HIGH_TEMP)
    *high = value_max; // 只写入R
  // 低温
  if (temp < CONFIG_LOW_TEMP) {
    *high = value_max;
    *comfort = value_max;
    *low = value_max;
  }
}

/// @brief 将数据大小体现在颜色上
/// @param data 数据值
/// @param visual_cfg 可视化配置
/// @param high 高值分量 越趋近预设高温 向高值分量侧倾
/// @param comfort 适宜分量 越趋近预设适宜温度 向适宜分量侧倾
/// @param low 低值分量 越趋近预设低温 向低值分量侧倾
void data_to_color(int data, UI_color_visual_cfg_t *visual_cfg, uint8_t *high,
                   uint8_t *comfort, uint8_t *low) {
  float value_buf = 0;

  // 如果在自由变化范围
  if (data >= visual_cfg->low && data <= visual_cfg->high) {
    value_buf = abs(data - visual_cfg->high);
    *high =
        (1 - value_buf / visual_cfg->public_divisor) * visual_cfg->value_max;

    value_buf = abs(data - visual_cfg->medium);
    *comfort =
        (1 - value_buf / visual_cfg->public_divisor) * visual_cfg->value_max;

    value_buf = abs(data - visual_cfg->low);
    *low = (1 - value_buf / visual_cfg->public_divisor) * visual_cfg->value_max;
  }
  // 过高
  if (data > visual_cfg->high)
    *high = visual_cfg->value_max; // 只写入R
  // 过低
  if (data < visual_cfg->low) {
    *high = visual_cfg->value_max;
    *comfort = visual_cfg->value_max;
    *low = visual_cfg->value_max;
  }
}

/// @brief 显示表情
/// @param x 起始坐标x
/// @param y 起始坐标y
/// @param emotion_label
/// 情绪标签(英语单词),对应不同表情,normal->正常(无特别情绪),happy->愉快,like->喜爱,angry->愤怒,disgusting->厌恶,fearful->恐惧,sad->悲伤
void facial_expression_show(int x, int y, char *emotion_label) {
  const char *TAG = "facial_expression_show";

  if (emotion_label == NULL) {
    ESP_LOGE(TAG, "lable为NULL");
    return;
  }

  if (strcmp(emotion_label, "normal") == 0) {
    ESP_LOGI(TAG, "显示正常表情");
    direct_draw(x, y, gImage_normal);
  } else if (strcmp(emotion_label, "happy") == 0) {
    ESP_LOGI(TAG, "显示愉快表情");
    direct_draw(x, y, gImage_happy);
  } else if (strcmp(emotion_label, "like") == 0) {
    ESP_LOGI(TAG, "显示喜爱表情");
    direct_draw(x, y, gImage_like);
  } else if (strcmp(emotion_label, "angry") == 0) {
    ESP_LOGI(TAG, "显示愤怒表情");
    direct_draw(x, y, gImage_angry);
  } else if (strcmp(emotion_label, "disgusting") == 0) {
    ESP_LOGI(TAG, "显示厌恶表情");
    direct_draw(x, y, gImage_disgusting);
  } else if (strcmp(emotion_label, "fearful") == 0) {
    ESP_LOGI(TAG, "显示恐惧表情");
    direct_draw(x, y, gImage_fearful);
  } else if (strcmp(emotion_label, "sad") == 0) {
    ESP_LOGI(TAG, "显示悲伤表情");
    direct_draw(x, y, gImage_sad);
  } else {
    ESP_LOGI(TAG, "无法识别表情标签 %s", emotion_label);
  }
}

/// @brief 显示天气图标(9x9)+温度
/// @param x 起始坐标x
/// @param y 起始坐标y
void weather_icon_temperature(int x, int y) {
  const char *TAG = "weather_icon_temperature";
  weather_change_flag = 0;

  // 获取天气图标数据
  unsigned char icon_data[251] = {0};
  int ret =
      get_weather_icon_data(icon_data, current_weather_data.condition_code);
  if (ret == -1) {
    weather_change_flag = 0;
    ESP_LOGE(TAG, "获取天气图标数据失败");
    return; // 如果获取失败，退出
  } else {
    weather_change_flag = ret;
    direct_draw(x + ((LINE_LED_NUMBER / 2) - WEATHER_ICON_BREATH) / 2,
                y + (VERTICAL_LED_NUMBER - WEATHER_ICON_HEIGHT) / 2, icon_data);
  }

  // 测试
  current_weather_data.temperature = -7;

  // 温度显示
  int temp_buf = abs((int)round(
      current_weather_data
          .temperature)); // 因为这里温度只能识别到数字，并且只显示整数部分，四舍五入为整数后取一下绝对值
  if (temp_buf >= 100) {
    ESP_LOGE(TAG, "不合理的温度绝对值 %d", temp_buf);
    return; // 如果绝对值大于99显示都是问题了，大可能是传错了，退出
  }

  uint8_t color[3] = {0};
  temp_to_color(current_weather_data.temperature, 255, &color[0], &color[1],
                &color[2]); // 数据可以通过颜色可视化

  // 取出每位上的数
  int8_t tens = temp_buf / 10;         // 十位
  int8_t uints = temp_buf - tens * 10; // 个位

  // 确定要不要带负号
  int minus_breath = 2;
  int minus_height = 1;
  if (current_weather_data.temperature < 0) {
    uint8_t *p = rectangle(minus_breath, minus_height);
    separation_draw(x + (LINE_LED_NUMBER / 2) + 1,
                    y + 1 + (VERTICAL_LED_NUMBER - minus_height) / 2,
                    minus_breath, RECTANGLE_MATRIX(p), *p, color);
    free(p);
    print_number(x + (LINE_LED_NUMBER / 2) + 1 + minus_breath + 1,
                 y + 1 + (VERTICAL_LED_NUMBER - FIGURE_HEIGHT) / 2, tens,
                 color);
    print_number(
        x + (LINE_LED_NUMBER / 2) + 1 + minus_breath + 1 + FIGURE_BREATH + 1,
        y + 1 + (VERTICAL_LED_NUMBER - FIGURE_HEIGHT) / 2, uints, color);
  } else {
    print_number(x + (LINE_LED_NUMBER / 2) + 1,
                 y + 1 + (VERTICAL_LED_NUMBER - FIGURE_HEIGHT) / 2, tens,
                 color);
    print_number(x + (LINE_LED_NUMBER / 2) + 1 + FIGURE_BREATH + 1,
                 y + 1 + (VERTICAL_LED_NUMBER - FIGURE_HEIGHT) / 2, uints,
                 color);
  }
}

/// @brief 显示了当前系统时间  时 分
/// @param x 起始坐标x
/// @param y 起始坐标y
/// @param change 亮度值0-100% 为0不会任何进行显示操作
void time_UI_h_m(int x, int y) {
  static uint8_t color[3] = {0};
  static int8_t minute_buf = 80;

  if (minute_buf != systemtime_data.minute) {
    // 时间的颜色使用随机
    esp_fill_random(&color[0], 1);
    esp_fill_random(&color[1], 1);
    esp_fill_random(&color[2], 1);
    minute_buf = systemtime_data.minute;
  }

  int8_t hour_tens = systemtime_data.hour / 10;              // 十位
  int8_t hour_uints = systemtime_data.hour - hour_tens * 10; // 个位
  print_number(
      x + LINE_LED_NUMBER / 4 * 0 + (LINE_LED_NUMBER / 4 - FIGURE_BREATH) / 2,
      y + VERTICAL_LED_NUMBER / 2 - FIGURE_HEIGHT / 2, hour_tens, color);
  print_number(
      x + LINE_LED_NUMBER / 4 * 1 + (LINE_LED_NUMBER / 4 - FIGURE_BREATH) / 2,
      y + VERTICAL_LED_NUMBER / 2 - FIGURE_HEIGHT / 2, hour_uints, color);

  int8_t minute_tens = systemtime_data.minute / 10;                // 十位
  int8_t minute_uints = systemtime_data.minute - minute_tens * 10; // 个位
  print_number(
      x + LINE_LED_NUMBER / 4 * 2 + (LINE_LED_NUMBER / 4 - FIGURE_BREATH) / 2,
      y + VERTICAL_LED_NUMBER / 2 - FIGURE_HEIGHT / 2, minute_tens, color);
  print_number(
      x + LINE_LED_NUMBER / 4 * 3 + (LINE_LED_NUMBER / 4 - FIGURE_BREATH) / 2,
      y + VERTICAL_LED_NUMBER / 2 - FIGURE_HEIGHT / 2, minute_uints, color);
}

/// @brief 显示了当前系统时间  秒
/// @param x 起始坐标x
/// @param y 起始坐标y
/// @param change 亮度值0-100% 为0不会任何进行显示操作
void time_UI_s(int x, int y) {
  static uint8_t color[3] = {0};
  static int8_t second_buf = 80;

  if (second_buf != systemtime_data.second) {
    // 时间的颜色使用随机
    esp_fill_random(&color[0], 1);
    esp_fill_random(&color[1], 1);
    esp_fill_random(&color[2], 1);
    second_buf = systemtime_data.second;
  }

  int8_t second_tens = systemtime_data.second / 10;                // 十位
  int8_t second_uints = systemtime_data.second - second_tens * 10; // 个位
  print_number(x + LINE_LED_NUMBER / 2 - FIGURE_BREATH - 1,
               y + VERTICAL_LED_NUMBER / 2 - FIGURE_HEIGHT / 2, second_tens,
               color); // 数字字模的尺寸为4x7
  print_number(x + LINE_LED_NUMBER / 2 + 1,
               y + VERTICAL_LED_NUMBER / 2 - FIGURE_HEIGHT / 2, second_uints,
               color);
}

/// @brief 显示了当前系统时间  时 分 秒
/// @param x 起始坐标x
/// @param y 起始坐标y
/// @param change 亮度值0-100% 为0不会任何进行显示操作
void time_UI_h_m_s(int x, int y) {
  static uint8_t color[3] = {0};
  static int8_t second_buf = 80;

  if (second_buf != systemtime_data.second) {
    // 时间的颜色使用随机
    esp_fill_random(&color[0], 1);
    esp_fill_random(&color[1], 1);
    esp_fill_random(&color[2], 1);
    second_buf = systemtime_data.second;
  }

  int8_t hour_tens = systemtime_data.hour / 10;              // 十位
  int8_t hour_uints = systemtime_data.hour - hour_tens * 10; // 个位
  print_number(
      x + LINE_LED_NUMBER / 6 * 0 + (LINE_LED_NUMBER / 6 - FIGURE_BREATH) / 2,
      y + VERTICAL_LED_NUMBER / 2 - FIGURE_HEIGHT / 2, hour_tens, color);
  print_number(
      x + LINE_LED_NUMBER / 6 * 1 + (LINE_LED_NUMBER / 6 - FIGURE_BREATH) / 2,
      y + VERTICAL_LED_NUMBER / 2 - FIGURE_HEIGHT / 2, hour_uints, color);

  int8_t minute_tens = systemtime_data.minute / 10;                // 十位
  int8_t minute_uints = systemtime_data.minute - minute_tens * 10; // 个位
  print_number(
      x + LINE_LED_NUMBER / 6 * 2 + (LINE_LED_NUMBER / 6 - FIGURE_BREATH) / 2,
      y + VERTICAL_LED_NUMBER / 2 - FIGURE_HEIGHT / 2, minute_tens, color);
  print_number(
      x + LINE_LED_NUMBER / 6 * 3 + (LINE_LED_NUMBER / 6 - FIGURE_BREATH) / 2,
      y + VERTICAL_LED_NUMBER / 2 - FIGURE_HEIGHT / 2, minute_uints, color);

  int8_t second_tens = systemtime_data.second / 10;                // 十位
  int8_t second_uints = systemtime_data.second - second_tens * 10; // 个位
  print_number(x + LINE_LED_NUMBER / 6 * 4 +
                   (LINE_LED_NUMBER / 6 - FIGURE_BREATH) / 2,
               y + VERTICAL_LED_NUMBER / 2 - FIGURE_HEIGHT / 2, second_tens,
               color); // 数字字模的尺寸为4x7
  print_number(
      x + LINE_LED_NUMBER / 6 * 5 + (LINE_LED_NUMBER / 6 - FIGURE_BREATH) / 2,
      y + VERTICAL_LED_NUMBER / 2 - FIGURE_HEIGHT / 2, second_uints, color);
}

/// @brief
/// 启动音频频谱UI绘制任务(必须有音频任务进行中才可以启动),检测到音频活动任务结束会暂停监视,直到新的音频活动出现
/// @param UI_cfg FFT的UI配置
/// @param priority 任务优先级
/// @return music_FFT_UI_handle_t 句柄,用于后续操作UI / NULL
/// 表示申请失败,请检查内存是否足够
music_FFT_UI_handle_t music_FFT_UI_start(music_FFT_UI_cfg_t *UI_cfg,
                                         UBaseType_t priority) {
  const char *TAG = "music_FFT_UI_start";

  music_FFT_UI_handle_t handle = calloc(1, sizeof(music_FFT_UI_t));
  if (!handle) {
    ESP_LOGE(TAG, "申请handle资源时发现问题 需要 %d 字节",
             sizeof(music_FFT_UI_t));
    return NULL;
  }

  if (!collecter_handle->collecter_buf) {
    ESP_LOGE(TAG, "当前音频缓存为空,无法启动FFT_UI");
    return NULL;
  }

  memcpy(&(handle->cfg), UI_cfg, sizeof(music_FFT_UI_cfg_t));
  if (handle->cfg.dampen_multiples <= 0) {
    ESP_LOGW(TAG, "数据衰减倍数必须大于0,已自动调整为1");
    handle->cfg.dampen_multiples = 1;
  }
  if (handle->cfg.x_multiples < 0) {
    ESP_LOGW(TAG, "视口横向缩放倍数小于0,已自动调整为1");
    handle->cfg.x_multiples = 1;
  }
  if (handle->cfg.width > FFT_VIEW_WIDTH_MAX) {
    ESP_LOGW(TAG,
             "视口宽度超过最大宽度 FFT_VIEW_WIDTH_MAX ,已自动调整为最大宽度");
    handle->cfg.width = FFT_VIEW_WIDTH_MAX;
  }

  handle->running_flag = true;

  xTaskCreatePinnedToCore((TaskFunction_t)&music_FFT_UI_refresh_Task,
                          "music_FFT_UI_refresh_Task", FFT_UI_TASK_STACK_SIZE,
                          handle, priority, NULL, FFT_UI_TASK_CORE);

  return handle;
}

/// @brief 停止音频频谱UI绘制任务
/// @param handle 句柄
/// @return esp_err_t ESP_OK 表示成功 / ESP_ERR_INVALID_ARG 表示参数柄为空
esp_err_t music_FFT_UI_stop(music_FFT_UI_handle_t handle) {
  const char *TAG = "music_FFT_UI_stop";
  if (!handle) {
    ESP_LOGE(TAG, "handle 为空,无法停止任务");
    return ESP_ERR_INVALID_ARG;
  }
  handle->running_flag = false;
  return ESP_OK;
}

/// @brief 利用 FFT 刷新音频频谱任务,需要创建任务调用,参考了ESP-DSP fft例程
/// @param UI_cfg UI配置
void music_FFT_UI_refresh_Task(music_FFT_UI_handle_t handle) {
  const char *TAG = "music_FFT_UI_refresh_Task";
  __attribute__((aligned(16))) float *y1_cf = &y_cf[0]; // 指向合成缓存
  int N = FFT_N_SAMPLES;                                // FFT 点数 N

  // FFT数据缓存 通过__attribute__((aligned(16)))请求16字节对齐格式
  src_data = malloc(N * sizeof(float)); // 源数据
  wind = malloc(N * sizeof(float));     // 窗口系数
  if (!src_data || !wind) {
    ESP_LOGE(TAG, "申请FFT内存 src_data wind 时发现问题 需要 %d 字节",
             N * sizeof(float) * 2);
    handle->running_flag = false;
  }

  // 源数据缓存
  int read_len_in_bytes = 0; // 已读取长度(单位字节)
  int16_t *sample_buf = NULL;
  sample_buf = malloc(FFT_N_SAMPLES * sizeof(int16_t));
  if (!sample_buf) {
    ESP_LOGE(TAG, "申请sample_buf资源时发现问题 需要 %d 字节",
             FFT_N_SAMPLES * sizeof(int16_t));
    handle->running_flag = false;
  }

  // 申请图谱绘制缓存
  float *unit_data_max = NULL; // 存储每个单位频率范围下较大的幅度
  unit_data_max = malloc(handle->cfg.width * sizeof(float));
  if (!unit_data_max) {
    ESP_LOGE(TAG, "申请unit_data_max资源时发现问题 需要 %d 字节",
             handle->cfg.width * sizeof(float));
    handle->running_flag = false;
  }
  float *unit_data_min = NULL; // 存储每个单位频率范围下较小的幅度
  unit_data_min = malloc(handle->cfg.width * sizeof(float));
  if (!unit_data_min) {
    ESP_LOGE(TAG, "申请unit_data_min资源时发现问题 需要 %d 字节",
             handle->cfg.width * sizeof(float));
    handle->running_flag = false;
  }

  if (handle->running_flag) {
    memset(sample_buf, 0, FFT_N_SAMPLES * sizeof(int16_t));
    memset(unit_data_max, 0, handle->cfg.width * sizeof(float));
    memset(unit_data_min, 0, handle->cfg.width * sizeof(float));
    memset(src_data, 0, N * sizeof(float));
    memset(wind, 0, N * sizeof(float));
    if (dsps_fft2r_init_fc32(NULL, DSP_MAX_FFT_SIZE) != ESP_OK) {
      ESP_LOGE(TAG, "初始化FFT模块时发现问题");
      handle->running_flag = false;
    }
    dsps_wind_hann_f32(wind, N);
  }

  ESP_LOGW(TAG, "FFT监视已启动");

  while (handle->running_flag) {
    vTaskDelay(pdMS_TO_TICKS(10));

    if (collecter_handle->collecter_running_flag == false) {
      // 如果音频活动未进行,仅清理屏幕缓存并等待
      memset(unit_led_height, 0, FFT_VIEW_WIDTH_MAX);
      memset(unit_led_color, 0, FFT_VIEW_WIDTH_MAX * 3);
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    } else {
      if (collecter_handle->collecter_buf_overflow_flag ==
          false) {
        // 如果音频活动进行中,且缓存未溢出,则等待缓存溢出
        continue;
      }
    }

    if (xSemaphoreTake(collecter_handle->collecter_buf_mutex,
                       pdMS_TO_TICKS(FFT_CURRENT_SOUND_BUF_WAIT_TIME_MS)) !=
        pdTRUE) {
      continue;
    }

    // 清理之前缓存的数据
    memset(unit_data_max, 0, handle->cfg.width * sizeof(float));
    memset(unit_data_min, 0, handle->cfg.width * sizeof(float));

    if (N * sizeof(int16_t) > CURRENT_SOUND_BUF_SIZE / 2) {
      read_len_in_bytes = CURRENT_SOUND_BUF_SIZE / 2;
    } else {
      read_len_in_bytes = N * sizeof(int16_t);
    }

    if (collecter_handle->collecter_element_info.bits == 16) {
      for (int i = 0; i < read_len_in_bytes / sizeof(int16_t); i++) {
        sample_buf[i] =
            ((int16_t *)(collecter_handle
                             ->collecter_buf))[i * 2 + handle->cfg.lr_switch];
      }
    } else if (collecter_handle->collecter_element_info.bits ==
               24) {
      for (int i = 0; i < read_len_in_bytes / sizeof(int16_t); i++) {
        sample_buf[i] =
            ((int32_t *)(collecter_handle
                             ->collecter_buf))[i * 2 + handle->cfg.lr_switch] /
            0x7FFFFF * INT16_MAX;
      }
    } else if (collecter_handle->collecter_element_info.bits ==
               32) {
      for (int i = 0; i < read_len_in_bytes / sizeof(int16_t); i++) {
        sample_buf[i] =
            ((int32_t *)(collecter_handle
                             ->collecter_buf))[i * 2 + handle->cfg.lr_switch] /
            INT32_MAX * INT16_MAX;
      }
    } else if (collecter_handle->collecter_element_info.bits ==
               8) {
      for (int i = 0; i < read_len_in_bytes / sizeof(int16_t); i++) {
        sample_buf[i] =
            ((int8_t *)(collecter_handle
                            ->collecter_buf))[i * 2 + handle->cfg.lr_switch] /
            INT8_MAX * INT16_MAX;
      }
    } else {
      ESP_LOGW(TAG, "不支持的音频位深度 %d",
               collecter_handle->collecter_element_info.bits);
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    xSemaphoreGive(collecter_handle->collecter_buf_mutex);

    for (int i = 0; i < N; i++) {
      src_data[i] = (float)(sample_buf[i]) / handle->cfg.dampen_multiples;
    }

    // // 转换为复向量
    for (int i = 0; i < N; i++) {
      y_cf[i * 2 + 0] = src_data[i] * wind[i];
      y_cf[i * 2 + 1] = 0;
    }

    dsps_fft2r_fc32(y_cf, N);    // 运行FFT
    dsps_bit_rev_fc32(y_cf, N);  // 位翻转
    dsps_cplx2reC_fc32(y_cf, N); // 转换为两个复向量

    for (int i = 0; i < N / 2; i++)
      y1_cf[i] = 10 * log10f((y1_cf[i * 2 + 0] * y1_cf[i * 2 + 0] +
                              y1_cf[i * 2 + 1] * y1_cf[i * 2 + 1]) /
                                 N +
                             1e-10);

    // // 调试时可使用官方提供的显示API,将以图表形式把数据打印到 ESP-IDF
    // Monitor上 dsps_view(y1_cf, N, 24, 12, handle->cfg.data_min,
    // handle->cfg.data_max, '|'); vTaskDelay(pdMS_TO_TICKS(100));

    // 处理FFT数据即处理 y1_cf数组数据
    // y1_cf数组的角标数值映射到频率X轴 每个单位值的大小映射到幅度Y轴
    // 然而LedArray尺寸有限，需要缩放到点阵下
    // 我们可以由 0 到 N-1 遍历整个y1_cf查看数据，伴随 unit_data_max 数组 由 0
    // 遍历 到 视口宽度-1
    // 存储每一纵列的数据最大值（视口横向缩放倍数为1不缩放时）
    // 之后我们不再关心y1_cf，而是使用unit_data_max中的数据
    // 根据视口预设的数据最大值最小值计算灯光柱高度颜色并显示

    // 遍历 y1_cf 中的数据

    int unit_select = 0;
    for (int i = 0; i < N / handle->cfg.x_multiples; i++) {
      unit_select = handle->cfg.width *
                    (i / (N / handle->cfg.x_multiples)); // 映射值由 0 到 width
      // 在有效数据宽度进行
      if (unit_select < handle->cfg.width && unit_select >= 0) {
        // 取出每个单位频率范围下较大的幅度
        int y1_cf_select = i + N / handle->cfg.x_multiples *
                                   handle->cfg.x_move; // 按照预设偏移
        if (y1_cf_select >= 0 && y1_cf_select < N) {
          if (unit_data_max[unit_select] < y1_cf[y1_cf_select])
            unit_data_max[unit_select] = y1_cf[y1_cf_select];
          if (unit_data_min[unit_select] > y1_cf[y1_cf_select])
            unit_data_min[unit_select] = y1_cf[y1_cf_select];
        }
        // 排除过高值
        if (unit_data_max[unit_select] > handle->cfg.data_max)
          unit_data_max[unit_select] = handle->cfg.data_max;
        // 排除过低值
        if (unit_data_min[unit_select] < handle->cfg.data_min)
          unit_data_min[unit_select] = handle->cfg.data_min;
      }
    }

    // 遍历 unit_data_max 中的数据
    for (int j = 0; j < handle->cfg.width; j++) {
      // 计算灯光柱颜色
      data_to_color((unit_data_max[j]), &(handle->cfg.color_visual_cfg),
                    &unit_led_color[j * 3 + 0], &unit_led_color[j * 3 + 1],
                    &unit_led_color[j * 3 + 2]);

      // 计算灯光柱高度
      unit_led_height[j] =
          VERTICAL_LED_NUMBER * (unit_data_max[j] - unit_data_min[j]) /
          (handle->cfg.data_max -
           handle->cfg.data_min); // 映射值由 0 到 VERTICAL_LED_NUMBER

      // 排除过高值
      if (unit_led_height[j] > handle->cfg.show_height_max)
        unit_led_height[j] = handle->cfg.show_height_max;
    }
  }

  ESP_LOGW(TAG, "FFT绘制任务关闭");

  memset(unit_led_color, 0, FFT_VIEW_WIDTH_MAX * 3);
  memset(unit_led_height, 0, FFT_VIEW_WIDTH_MAX);

  free(unit_data_min);
  free(unit_data_max);
  free(sample_buf);
  free(src_data);
  free(wind);
  unit_data_max = NULL;
  unit_data_min = NULL;
  sample_buf = NULL;
  src_data = NULL;
  wind = NULL;

  dsps_fft2r_deinit_fc32();

  vTaskDelete(NULL);
}

void music_FFT_UI_draw(music_FFT_UI_handle_t handle) {
  const char *TAG = "music_FFT_UI_draw";

  if (!handle) {
    ESP_LOGE(TAG, "无效的空句柄");
    return;
  }

  int x = handle->cfg.x;
  int y = handle->cfg.y;

  for (int j = 0; j < handle->cfg.width; j++) {
    // 显示数据到缓存
    uint8_t *p = rectangle(1, unit_led_height[j]);
    if (p) {
      uint8_t color_buf[3] = {0};
      color_buf[0] = unit_led_color[j * 3 + 0];
      color_buf[1] = unit_led_color[j * 3 + 1];
      color_buf[2] = unit_led_color[j * 3 + 2];
      separation_draw(x + j, y + handle->cfg.height - unit_led_height[j], 1,
                      RECTANGLE_MATRIX(p), *p, color_buf);
      free(p);
      p = NULL;
    }
  }
}

/// @brief 隐写支持服务
/// @param handle 动画句柄
/// @param idx 触发隐写的隐写关键帧在关键帧数据库的角标位置
void steganography_service(cartoon_handle_t handle, int idx) {
  const char *TAG = "steganography_service";
  if (!handle) {
    ESP_LOGE(TAG, "无效的空动画句柄");
    return;
  }
  switch (handle->cartoon_plan.key_frame_database[idx].percentage) {
    // 隐写模式:映射 -
    // [警告:只有隐写关键帧设置在目标关键帧之后数据才能映射]映射内存中的数据以填充任意关键帧
    // x y color[3]
    // change等存储要读取的内存地址(为NULL的不读取),根据x(隐写数据位x)地址读到的数据保存到目标关键帧数据单元的x成员上,以此类推
    // step_buf不同,存储的是要映射到的对象关键帧在key_frame_database的角标[警告:只有隐写关键帧设置在目标关键帧之后数据才能映射]
  case STEGANOGRAPHY_MODE_MAPPING_EQUATION:
    // 相等映射,映射关系为相等,源数据直接覆盖目标位置
    for (int pselect = 0;
         pselect < ((sizeof(key_frame_t) - sizeof(key_frame_attr_t) -
                     sizeof(int32_t) - sizeof(int32_t)) /
                    sizeof(int32_t));
         pselect++) {
      if ((int32_t *)(*(
              &(handle->cartoon_plan.key_frame_database[idx].x) +
              pselect *
                  sizeof(
                      int32_t)))) { // 隐写关键帧对应数据位地址->对应数据位数据(即预设地址)
        // 对应数据位数据(预设地址数据X)非空
        *(&(handle->cartoon_plan
                .key_frame_database[handle->cartoon_plan.key_frame_database[idx]
                                        .step_buf]
                .x) +
          pselect * sizeof(int32_t)) // 对应变量的地址->对应变量
            = *((int32_t *)(*(
                &(handle->cartoon_plan.key_frame_database[idx].x) +
                pselect *
                    sizeof(
                        int32_t)))); // 隐写关键帧对应数据位地址->对应数据位数据(即预设地址)->该(预设地址)对应的变量值
      }
    }
    break;
  case STEGANOGRAPHY_MODE_MAPPING_ADDITION:
    // 加法映射,覆盖目标位置的值为 [映射前目标位置的值 加上 源数据]
    for (int pselect = 0;
         pselect < ((sizeof(key_frame_t) - sizeof(key_frame_attr_t) -
                     sizeof(int32_t) - sizeof(int32_t)) /
                    sizeof(int32_t));
         pselect++) {
      if ((int32_t *)(*(
              &(handle->cartoon_plan.key_frame_database[idx].x) +
              pselect *
                  sizeof(
                      int32_t)))) { // 隐写关键帧对应数据位地址->对应数据位数据(即预设地址)
        // 对应数据位数据(预设地址数据X)非空
        *(&(handle->cartoon_plan
                .key_frame_database[handle->cartoon_plan.key_frame_database[idx]
                                        .step_buf]
                .x) +
          pselect * sizeof(int32_t)) // 对应变量的地址->对应变量
            += *((int32_t *)(*(
                &(handle->cartoon_plan.key_frame_database[idx].x) +
                pselect *
                    sizeof(
                        int32_t)))); // 隐写关键帧对应数据位地址->对应数据位数据(即预设地址)->该(预设地址)对应的变量值
      }
    }
    break;
  case STEGANOGRAPHY_MODE_MAPPING_SUBTRACTION:
    // 减法映射,覆盖目标位置的值为 [映射前目标位置的值 减去 源数据]
    for (int pselect = 0;
         pselect < ((sizeof(key_frame_t) - sizeof(key_frame_attr_t) -
                     sizeof(int32_t) - sizeof(int32_t)) /
                    sizeof(int32_t));
         pselect++) {
      if ((int32_t *)(*(
              &(handle->cartoon_plan.key_frame_database[idx].x) +
              pselect *
                  sizeof(
                      int32_t)))) { // 隐写关键帧对应数据位地址->对应数据位数据(即预设地址)
        // 对应数据位数据(预设地址数据X)非空
        *(&(handle->cartoon_plan
                .key_frame_database[handle->cartoon_plan.key_frame_database[idx]
                                        .step_buf]
                .x) +
          pselect * sizeof(int32_t)) // 对应变量的地址->对应变量
            -= *((int32_t *)(*(
                &(handle->cartoon_plan.key_frame_database[idx].x) +
                pselect *
                    sizeof(
                        int32_t)))); // 隐写关键帧对应数据位地址->对应数据位数据(即预设地址)->该(预设地址)对应的变量值
      }
    }
    break;
  case STEGANOGRAPHY_MODE_MAPPING_MULTIPLICATION:
    // 乘法映射,覆盖目标位置的值为[映射前目标位置的值 乘上 源数据]
    for (int pselect = 0;
         pselect < ((sizeof(key_frame_t) - sizeof(key_frame_attr_t) -
                     sizeof(int32_t) - sizeof(int32_t)) /
                    sizeof(int32_t));
         pselect++) {
      if ((int32_t *)(*(
              &(handle->cartoon_plan.key_frame_database[idx].x) +
              pselect *
                  sizeof(
                      int32_t)))) { // 隐写关键帧对应数据位地址->对应数据位数据(即预设地址)
        // 对应数据位数据(预设地址数据X)非空
        *(&(handle->cartoon_plan
                .key_frame_database[handle->cartoon_plan.key_frame_database[idx]
                                        .step_buf]
                .x) +
          pselect * sizeof(int32_t)) // 对应变量的地址->对应变量
            *= *((int32_t *)(*(
                &(handle->cartoon_plan.key_frame_database[idx].x) +
                pselect *
                    sizeof(
                        int32_t)))); // 隐写关键帧对应数据位地址->对应数据位数据(即预设地址)->该(预设地址)对应的变量值
      }
    }
    break;
  case STEGANOGRAPHY_MODE_MAPPING_DIVISION:
    // 除法映射,覆盖目标位置的值为[映射前目标位置的值 除以 源数据]
    for (int pselect = 0;
         pselect < ((sizeof(key_frame_t) - sizeof(key_frame_attr_t) -
                     sizeof(int32_t) - sizeof(int32_t)) /
                    sizeof(int32_t));
         pselect++) {
      if ((int32_t *)(*(
              &(handle->cartoon_plan.key_frame_database[idx].x) +
              pselect *
                  sizeof(
                      int32_t)))) { // 隐写关键帧对应数据位地址->对应数据位数据(即预设地址)
        // 对应数据位数据(预设地址数据X)非空
        *(&(handle->cartoon_plan
                .key_frame_database[handle->cartoon_plan.key_frame_database[idx]
                                        .step_buf]
                .x) +
          pselect * sizeof(int32_t)) // 对应变量的地址->对应变量
            /= *((int32_t *)(*(
                &(handle->cartoon_plan.key_frame_database[idx].x) +
                pselect *
                    sizeof(
                        int32_t)))); // 隐写关键帧对应数据位地址->对应数据位数据(即预设地址)->该(预设地址)对应的变量值
      }
    }
    break;
  default:
    ESP_LOGE(TAG, "未知的隐写模式");
    break;
  }
}
