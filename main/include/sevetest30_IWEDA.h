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

// 包含一些sevetest30的  互联网环境中  数据获取（IWEDA）
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 附加 1  github - zlib项目 链接 https://github.com/madler/zlib
//      2  和风天气API开发文档：   https://dev.qweather.com/docs/api
// 敬告：有效的数据存储变量都封装在该库下，不需要在外部函数定义一个数据结构体缓存作为参数，直接读取公共变量，主要为了方便FreeRTOS的任务支持
// github: https://github.com/701Enti

#pragma once

#include "esp_err.h"
#include "esp_http_client.h"
#include "periph_wifi.h"
#include "sevetest30_sound.h"
#include "stdbool.h"
#include <stdint.h>

#define WIFI_CONNECT_TIMEOUT_MS 30000 // WIFI连接等待超时时间

// zlib数据解压窗口允许大小
#define ZLIB_WINDOW_MAX 47

// JSON数据转换函数内，如果额外附加对JSON数据的预处理（解压或删改）,其缓冲的数组下标允许大小
#define PRE_CJSON_BUF_MAX 1024

#define HTTP_TASK_CORE 0 // http任务运行核心
#define HTTP_TASK_PRIO 1 // http任务优先级

#define IWEDA_DEFAULT_URL_BUF_SIZE 2048    // 默认URL缓存大小
#define IWEDA_DEFAULT_OUTPUT_BUF_SIZE 2048 // 默认输出缓存大小

// ASR语音识别结果缓存大小
#define ASR_RESULT_TEX_BUF_MAX 4096

// 百度长文本语音合成API
#define BAIDU_LONG_TTS_CREATE_URL                                              \
  "https://aip.baidubce.com/rpc/2.0/tts/v1/create?access_token=%s"
#define BAIDU_LONG_TTS_TASK_LOOKUP_URL                                         \
  "https://aip.baidubce.com/rpc/2.0/tts/v1/query?access_token=%s"

#define BAIDU_API_ACCESSTOKEN_SIZE_MAX 100 // 百度API access_token缓存大小
#define BAIDU_API_ACCESSTOKEN_REFRESH_TIME 86400 / 2 // 12小时刷新一次

#define GPT_CHAT_RESPONSE_BUF_SIZE 128 * 1024         // GPT聊天响应缓存大小
#define GPT_CHAT_HTTP_REQUEST_BODY_BUF_SIZE 16 * 1024 // GPT聊天请求体缓存大小
#define GPT_CHAT_TASK_CORE 0                          // GPT聊天任务运行核心
#define GPT_CHAT_TASK_STACK_SIZE 4 * 1024             // GPT聊天任务堆栈大小

// 各种API的URL，字符由%s替代

// 查询IP的API
#define GET_IP_ADDRESS_API_URL "http://myip.ipip.net/s"

// IP138 IP归属地查询API
#define IP138_IP_POSITION_API_URL                                              \
  "https://api.ip138.com/ipdata/?ip=%s&datatype=jsonp&callback=find"

// 高德地图 搜索POI API
#define AMAP_SEARCH_POI_API_URL                                                \
  "https://restapi.amap.com/v3/place/"                                         \
  "text?keywords=%s&offset=%d&page=%d&key=%s"

// 和风天气
#define QWEATHER_GEO_CITY_LOOKUP_API_URL                                       \
  "https://%s/geo/v2/city/lookup?location=%s,%s" // GeoAPI-城市搜索
#define QWEATHER_CURRENT_WEATHER_API_URL                                       \
  "https://%s/weather/v1/current/%.2f/%.2f" // 实时天气API
#define QWEATHER_JWT_TOKEN_TERM_OF_VALIDITY                                    \
  86400 / 2 // JWT令牌有效期，单位秒，这里设为12小时

// 百度获取access_token API
#define BAIDU_GET_ACCESS_TOKEN_URL                                             \
  "https://aip.baidubce.com/oauth/2.0/"                                        \
  "token?client_id=%s&client_secret=%s&grant_type=client_credentials"

// 百度API-对话情绪识别
#define BAIDU_NLP_EMOTION_API_URL                                              \
  "https://aip.baidubce.com/rpc/2.0/nlp/v1/emotion?access_token=%s"

// 百度文心一言 ERNIE-Bot API
#define ERNIE_BOT_URL "https://qianfan.baidubce.com/v2/chat/completions"

typedef struct IWEDA_t {
  esp_http_client_handle_t http_client_handle;
  char *output_buf;    // 输出数据缓存
  int output_buf_size; // output_buf的大小
  char *url_buf;       // url缓存
  int url_buf_size;    // url缓存的大小
  bool is_completed;   // 请求完成标识
} IWEDA_t;

typedef struct IWEDA_t *IWEDA_handle_t;

  // 启用多轮对话时
  // context_json
  // 示例内容（字符串片段，注意开头允许直接是对象，对象之间用逗号分隔）
  // {"role": "system","content": "You are a helpful assistant."},
  // {"role": "user","content": "你好"},
  // {"role": "assistant","content": "你好啊"},
  // {"role": "user","content": "xxx"},
  // {"role": "assistant","content": "xxxxx"},
  //
  // 模块职责：维护一段消息JSON片段字符串；仅支持过期校验、尾部追加完整一轮(user+assistant)、整体释放。
  // 【重要约束】本模块不会做任何JSON序列化/反序列化，不解析内部结构，不支持头部/中间插入、修改、删除。
  // system消息、自定义头部内容，全部由上层调用者负责在对话任务开始前手动写入/更新context_json。
  //
  // 以下为本模块自身行为
  //
  // 1.【对话任务进行前-更新用户内容时】
  //    步骤1:如果context_json不为NULL,检查 context_json 是否过期，或已达到上下文最大字节长度
  //         满足任意条件：仅释放旧的context_json内存,设置context_json为NULL,设置context_json_update_time为0
  //    步骤2:如果context_json不为NULL,将 context_json直接拼接到API请求体的messages数组内部
  //
  // 2.【对话任务进行时（发API请求）】
  //    正常按API请求体缓存发送请求,不涉及本模块内容
  //
  // 3.【一次完整交互结束后（运行完全正常且已拿到完整回复）】
  //    输入待追加片段：{"role":"user","content":"xxx"},{"role":"assistant","content":"xxxxx"},
  //    注意，含末尾","
  //    a) 预估：旧字符串长度(如果context_json为NULL,则长度为0) + 待追加片段长度 是否超过 context_json_max_len
  //       -
  //       若会超限：仅释放旧的context_json内存,设置context_json为NULL,设置context_json_update_time为0,本轮交互不写入历史。
  //       -
  //       若不会超限：分配一块大小为预估长度的新内存，
  //                 如果context_json不为NULL：拷贝原有context_json内容，并在尾部追加新消息片段,释放旧的context_json内存
  //                 如果context_json为NULL：仅追加新消息片段
  //                 将context_json指向新分配的内存
  //                 更新context_json_update_time
  //
  // 4. system、自定义头部消息说明：
  //    如果需要加入system或其他自定义role消息，上层调用方必须在每次对话任务发起前，
  //    手动构造合法的JSON片段赋值给
  //    context_json。本模块不感知system，不负责维护system。
  //
  // 异常兜底：任意异常发生时(仅限上下文相关异常)立即调用关闭函数退化为单轮对话模式，对话交换不受其他影响

  typedef struct GPT_chat_context_t {
    bool is_enable_multi_round_chat;  // 启用多轮对话
    int context_json_max_len;         // 交互上下文最大长度
    int context_json_expire_ms;       // 交互上下文过期时间,单位毫秒    
    int64_t context_json_update_time; // 交互上下文更新时间,距离开机的微秒时间    
    char *context_json;               // 交互上下文(获得result后更新)
  } GPT_chat_context_t;

typedef struct GPT_chat_t {
  bool is_completed; // 本轮交互完成标识
  int timeout_ms;    // 超时时间,单位毫秒

  char *url;        // API地址
  char *access_key; // API密钥
  char *model;      // 模型名称

  char *user_content; // 本轮交互用户输入
  char *result;       // 本轮交互模型输出

  GPT_chat_context_t context;

  TaskHandle_t task_handle;               // 本轮交互任务句柄
  esp_err_t err;                          // 本轮交互错误码
  esp_http_client_handle_t client_handle; // 本轮交互http客户端句柄
  char *auth_header_buf;                  // 本轮交互http认证头缓存
  char *request_body_buf;                 // 本轮交互http请求体缓存
  char *response_buf;                     // 本轮交互http响应缓存
  char *json_buf;                         // 本轮交互json数据缓存
} GPT_chat_t;

typedef struct GPT_chat_t *GPT_chat_handle_t;

// 和风天气API-实时天气,顺序是在UI页面的展示顺序，靠近的数据表示他们应该显示在同一个页面
typedef struct current_weather_data_t {

  // 数值单位见https://dev.qweather.com/docs/resource/unit/

  char *metadata_tag; // 数据唯一标识
  char *
      metadata_attributions_raw_json; //(array对象，以原始json格式字符串保存)数据归因信息或声明，必须与当前数据共同显示

  char *condition_text; // 天气现象的本地化描述
  int condition_code;   // 天气现象代码

  double temperature; // 温度
  double feelsLike;   // 体感温度

  double humidity; // 相对湿度，取值范围 [0, 1]

  double wind_direction_degree; // 风向，取值范围 [0, 359]
  char *wind_direction_compass; // 风向的描述，可选值: n, nne, ne, ene, e, ese,
                                // se, sse, s, ssw, sw, wsw, w, wnw, nw, nnw,
                                // none, vrb
  double wind_speed;            // 风速
  double wind_scale;            // 蒲福风级
  double windGust;              // 阵风风速

  double precipitation_amount;    // 累计一小时降水量
  double precipitation_intensity; // 降水强度
  char *precipitation_type;       // 降水类型代码

  double pressure; // 海平面气压

  double visibility; // 能见度

  double dewPoint; // 露点温度

  double cloudCover; // 云量，取值范围 [0, 1]

  double uvIndex; // 紫外线指数，取值范围 [0, 15]

} current_weather_data_t;
// IP归属地信息
typedef struct ip_position_t {
  char *longitude; // 经度
  char *latitude;  // 纬度

  char *country; // 国家
  char *adm1;    // adm2的上一级行政区划 （省,若为直辖市则为直辖市名）
  char *adm2;    // name的上一级行政区划  (市)
  char *name;    //(区、县)

  char *qweather_location_id; // 和风天气城市数字ID号码
} position_data_t;

// 有效的数据存储变量都封装在该库下，不需要在外部函数定义一个数据结构体缓存作为参数，直接读取以下公共变量，主要为了方便FreeRTOS的任务支持

extern char *ip_address;                 // 公网IP
extern char *sevetest30_asr_result_text; // 语音识别结果
extern position_data_t ip_position_data;
extern current_weather_data_t current_weather_data;
extern esp_periph_handle_t wifi_periph_handle;

esp_err_t wifi_init(esp_periph_config_t *periph_config);
esp_err_t wifi_connect(periph_wifi_cfg_t *wifi_cfg);

esp_err_t init_time_data_sntp(uint32_t timeout_ms);

IWEDA_handle_t new_iweda_handle(int output_buf_size, int url_buf_size);
void delete_iweda_handle(IWEDA_handle_t iweda_handle);

int iweda_check_common_url(IWEDA_handle_t iweda_handle);
int iweda_check_response_content(IWEDA_handle_t iweda_handle);
void iweda_change_url_if_need_redirect(IWEDA_handle_t iweda_handle);

void gzip_decompress(void *input, int input_len, void *output);
int json_line_unit_num_get(char *data, int len);
void json_line_unit_copy(char *dest, char *src, int unit_id, int max_len);
void asr_data_save_result(char *asr_response);

esp_err_t url_encode(const char *src, char *dest, size_t dest_len,
                     bool use_plus_for_space);
void base64_to_base64url(char *str);
char *build_safe_json_string(const char* src);



void refresh_position_data();
void refresh_current_weather_data();

esp_err_t fetch_music_lyric_by_url(char *url, char *dest, int len_max);
esp_err_t fetch_text_emotion(const char *text, char *emotion_lable,
                             int lable_buf_size, int timeout_ms);
esp_err_t fetch_long_tts_speech_url(char *speech_url, int speech_url_size,
                                    TTS_cfg_t *cfg, int timeout_ms);

GPT_chat_handle_t GPT_chat_start(char *url, char *access_key, char *model,
                                 char *user_content, int timeout_ms);

esp_err_t GPT_chat_enable_multi_round_chat(GPT_chat_handle_t chat_handle,
                                           int context_json_max_len,
                                           int context_json_expire_ms);

esp_err_t GPT_chat_disable_multi_round_chat(GPT_chat_handle_t chat_handle);                           

esp_err_t GPT_chat_update_user_content(GPT_chat_handle_t chat_handle,
                                       char *user_content);

esp_err_t GPT_chat_text_exchange(GPT_chat_handle_t chat_handle, int task_prio);

esp_err_t GPT_chat_stop(GPT_chat_handle_t GPT_chat_handle);

char *get_baidu_api_access_token();