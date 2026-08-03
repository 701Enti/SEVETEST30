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

// 包含一些sevetest30的  互联网环境中  数据获取（IWEDA）
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 附加 1  github - zlib项目 链接 https://github.com/madler/zlib
//      2  和风天气API开发文档：   https://dev.qweather.com/docs/api
// 敬告：有效的数据存储变量都封装在该库下，不需要在外部函数定义一个数据结构体缓存作为参数，直接读取公共变量，主要为了方便FreeRTOS的任务支持
// github: https://github.com/701Enti
// bilibili: 701Enti

#ifndef _SEVETEST30_IWEDA_H_
#define _SEVETEST30_IWEDA_H_
#endif

#include "periph_wifi.h"
#include "esp_http_client.h"
#include "esp_err.h"
#include "stdbool.h"

#define WIFI_CONNECT_TIMEOUT_MS 30000 // WIFI连接等待超时时间

#define HTTP_BUF_MAX 2048      // http输出与URL数据缓存允许大小
#define ZLIB_WINDOW_MAX 47     // zlib数据解压窗口允许大小
#define PRE_CJSON_BUF_MAX 1024 // JSON数据转换函数内，如果额外附加对JSON数据的预处理（解压或删改）,其缓冲的数组下标允许大小

#define HTTP_TASK_CORE (0) // http任务运行核心
#define HTTP_TASK_PRIO (1) // http任务优先级

#define ASR_RESULT_TEX_BUF_MAX (4096)

#define ACCESSTOKEN_SIZE_MAX (100)

#define GPT_CHAT_RESPONSE_BUF_SIZE (128 * 1024)         // GPT聊天响应缓存大小
#define GPT_CHAT_HTTP_REQUEST_BODY_BUF_SIZE (16 * 1024) // GPT聊天请求体缓存大小
#define GPT_CHAT_TASK_CORE (0)                          // GPT聊天任务运行核心
#define GPT_CHAT_TASK_STACK_SIZE (16 * 1024)            // GPT聊天任务堆栈大小

// 各种API的URL，字符由%s替代

// 查询IP的API
#define GET_IP_ADDRESS_API_URL "http://myip.ipip.net/s"

// IP138 IP归属地查询API
#define IP138_IP_POSITION_API_URL "https://api.ip138.com/ipdata/?ip=%s&datatype=jsonp&callback=find"

// 高德地图 搜索POI API
#define AMAP_SEARCH_POI_API_URL "https://restapi.amap.com/v3/place/text?keywords=%s&offset=%d&page=%d&key=%s"

// 和风天气
#define QWEATHER_GEO_CITY_LOOKUP_API_URL "https://%s/geo/v2/city/lookup?location=%s,%s" // GeoAPI-城市搜索
#define QWEATHER_CURRENT_WEATHER_API_URL "https://%s/weather/v1/current/%.2f/%.2f"      // 实时天气API
#define QWEATHER_JWT_TOKEN_TERM_OF_VALIDITY 86400 / 2                                   // JWT令牌有效期，单位秒，这里设为12小时

// 百度获取access_token API
#define BAIDU_GET_ACCESS_TOKEN_URL "https://aip.baidubce.com/oauth/2.0/token?client_id=%s&client_secret=%s&grant_type=client_credentials"

// 百度文心一言 ERNIE-Bot API
#define ERNIE_BOT_URL "https://qianfan.baidubce.com/v2/chat/completions"

typedef struct GPT_chat_t
{
    bool is_completed;
    int timeout_ms;

    char *url;
    char *access_key;
    char *model;
    char *user_content;

    char *result;

    TaskHandle_t task_handle;
    esp_err_t err;
    esp_http_client_handle_t client_handle;
    char *auth_header_buf;
    char *request_body_buf;
    char *response_buf;
    char *json_buf;
} GPT_chat_t;

typedef struct GPT_chat_t *GPT_chat_handle_t;

// 和风天气API-实时天气,顺序是在UI页面的展示顺序，靠近的数据表示他们应该显示在同一个页面
typedef struct current_weather_data_t
{

    //数值单位见https://dev.qweather.com/docs/resource/unit/

    char* metadata_tag;//数据唯一标识
    char* metadata_attributions_raw_json;//(array对象，以原始json格式字符串保存)数据归因信息或声明，必须与当前数据共同显示

    char *condition_text;  // 天气现象的本地化描述
    int condition_code; // 天气现象代码

    double temperature; // 温度
    double feelsLike;   // 体感温度

    double humidity; // 相对湿度，取值范围 [0, 1]

    double wind_direction_degree;  // 风向，取值范围 [0, 359]
    char *wind_direction_compass; // 风向的描述，可选值: n, nne, ne, ene, e, ese, se, sse, s, ssw, sw, wsw, w, wnw, nw, nnw, none, vrb
    double wind_speed;             // 风速
    double wind_scale;             // 蒲福风级
    double windGust;               // 阵风风速

    double precipitation_amount;    // 累计一小时降水量
    double precipitation_intensity; // 降水强度
    char *precipitation_type;      // 降水类型代码

    double pressure; // 海平面气压

    double visibility; // 能见度

    double dewPoint; // 露点温度

    double cloudCover; // 云量，取值范围 [0, 1]

    double uvIndex; // 紫外线指数，取值范围 [0, 15]

} current_weather_data_t;
// IP归属地信息
typedef struct ip_position_t
{
    char *longitude; // 经度
    char *latitude;  // 纬度

    char *country; // 国家
    char *adm1;    // adm2的上一级行政区划 （省,若为直辖市则为直辖市名）
    char *adm2;    // name的上一级行政区划  (市)
    char *name;    //(区、县)

    char *qweather_location_id; // 和风天气城市数字ID号码
} position_data_t;

// 有效的数据存储变量都封装在该库下，不需要在外部函数定义一个数据结构体缓存作为参数，直接读取以下公共变量，主要为了方便FreeRTOS的任务支持

extern char http_output_buf[HTTP_BUF_MAX]; // 输出数据缓存
extern char http_url_buf[HTTP_BUF_MAX];    // url缓存,留着调用时候可以用
extern char *ip_address;                   // 公网IP
extern char *sevetest30_asr_result_text;   // 语音识别结果

extern position_data_t ip_position_data;
extern current_weather_data_t current_weather_data;
extern esp_periph_handle_t wifi_periph_handle;

// 内外部共享函数

void gzip_decompress(void *input, void *output, int len);

int json_line_unit_num_get(char *data, int len);

void json_line_unit_copy(char *dest, char *src, int unit_id, int max_len);

int http_check_common_url(const char *url);

int http_check_response_content(esp_http_client_handle_t client_handle);

void change_url_if_need_redirect(char **url);

// 库定制函数

void asr_data_save_result(char *asr_response);

esp_err_t get_music_lyric_by_url(char *url, char *dest, int len_max);

// 外部自由调用功能函数
esp_err_t wifi_init(esp_periph_config_t *periph_config);

esp_err_t wifi_connect(periph_wifi_cfg_t *wifi_cfg);

esp_err_t init_time_data_sntp(uint32_t timeout_ms);

void refresh_position_data();

void refresh_current_weather_data();

GPT_chat_handle_t GPT_chat_start(char *url, char *access_key, char *model, char *user_content, int timeout_ms);

esp_err_t GPT_chat_update_user_content(GPT_chat_handle_t chat_handle, char *user_content);

esp_err_t GPT_chat_text_exchange(GPT_chat_handle_t chat_handle, int task_prio);

esp_err_t GPT_chat_stop(GPT_chat_handle_t GPT_chat_handle);

esp_err_t baidu_get_AccessToken(char *client_id, char *client_secret, char *AccessToken);