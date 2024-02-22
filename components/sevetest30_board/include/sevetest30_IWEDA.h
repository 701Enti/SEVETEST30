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

// 该文件归属701Enti组织，由SEVETEST30开发团队维护，包含一些sevetest30的  互联网环境中  数据获取（IWEDA）
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 附加 1  github - zlib项目 链接 https://github.com/madler/zlib
//      2  和风天气API开发文档：   https://dev.qweather.com/docs/api
// 敬告：有效的数据存储变量都封装在该库下，不需要在外部函数定义一个数据结构体缓存作为参数，直接读取公共变量，主要为了方便FreeRTOS的任务支持
// github: https://github.com/701Enti
// bilibili: 701Enti

#ifndef _SEVETEST30_IWEDA_H_
#define _SEVETEST30_IWEDA_H_
#endif

#include "esp_peripherals.h"
#include "esp_http_client.h"
#include "esp_err.h"
#include "stdbool.h"

#define HTTP_BUF_MAX 2048 //http输出与URL数据缓存允许大小
#define ZLIB_WINDOW_MAX 47 //zlib数据解压窗口允许大小
#define PRE_CJSON_BUF_MAX 1024//JSON数据转换函数内，如果额外附加对JSON数据的预处理（解压或删改）,其缓冲的数组下标允许大小

#define HTTP_TASK_CORE           (0)
#define HTTP_TASK_PRIO           (2)

#define ASR_RESULT_TEX_BUF_MAX (4096)

#define ERNIE_BOT_4_CHAT_RESPONSE_BUF_MAX 8192
#define ERNIE_BOT_4_CHAT_TIMEOUT_MS      10000

//各种API的URL，字符由%s替代

//查询IP的API
#define GET_IP_ADDRESS_API_URL "http://myip.ipip.net/s"

//IP归属地查询API
#define IP_POSITION_API_URL "https://api.ip138.com/ip/?ip=%s&datatype=jsonp&callback=find"

//邮政编码查询经纬度API
#define TO_LNG_LAT_API_URL "https://quhua.ipchaxun.com/api/areas/data?zip=%s"


//和风天气GeoAPI(地址ID搜索，使用相同的KEY)
#define GEO_API_URL "https://geoapi.qweather.com/v2/city/lookup?location=%s,%s&key=%s"


//和风天气实时天气API
#if CONFIG_WEATHER_API_VIP_URL
#define WEATHER_API_URL "https://api.qweather.com/v7/weather/now?location=%s&key=%s"
#else
#define WEATHER_API_URL "https://devapi.qweather.com/v7/weather/now?location=%s&key=%s"
#endif

#define BAIDU_GET_ACCESS_TOKEN_URL "https://aip.baidubce.com/oauth/2.0/token?client_id=%s&client_secret=%s&grant_type=client_credentials"

//百度文心一言 ERNIE-Bot 4.0 API
#define ERNIE_BOT_4_URL "https://aip.baidubce.com/rpc/2.0/ai_custom/v1/wenxinworkshop/chat/completions_pro?access_token=%s"

//和风天气API-实时天气,顺序是在GUI页面的展示顺序，靠近的数据表示他们应该显示在同一个页面
typedef struct Real_time_weather{

    int icon;//天气状况代码，匹配合适的图案
    int text;//天气文字描述例如多云

    int temp;//温度，摄氏度
    int feelsLike;//体感温度，摄氏度

    int vis;//能见度,KM

    int humidity;//相对湿度，百分比
    int precip;//当前每小时降水量，毫米

    int windDir;//风向文字描述
    int wind360;//360度风向

    int windScale;//风力等级
    int windSpeed;//风速
    
    int pressure;//大气压强
    
    int cloud;//云量，可能为空

    int dew;//露点温度，可能为空（搜索了一下，可以理解是空气中水蒸气变为露珠时的温度,越低于环境温度，越不易液化结露，天气就越干燥）

    int obsTime;//数据观测时间 
}Real_time_weather;
//IP归属地信息
typedef struct ip_position
{
    char* postcode;//邮政编码
    char* lng;//经度
    char* lat;//纬度  

    char* country;//国家
    char* adm1;//adm2的上一级行政区划 （省,若为直辖市则为直辖市名）
    char* adm2;//name的上一级行政区划  (市)   
    char* name; //(区、县)

    char* id;//城市数字ID号码,由于天气查询
}ip_position;


//有效的数据存储变量都封装在该库下，不需要在外部函数定义一个数据结构体缓存作为参数，直接读取以下公共变量，主要为了方便FreeRTOS的任务支持

extern char http_get_out_buf[HTTP_BUF_MAX]; // 输出数据缓存
extern char http_get_url_buf[HTTP_BUF_MAX]; // url缓存,留着调用时候可以用
extern char *ip_address;//公网IP
extern char *asr_result_tex;//语音识别结果

extern ip_position *ip_position_data;
extern Real_time_weather *real_time_weather_data;

//内外部共享函数

void gzip_decompress(void *input,void *output, int len);

int http_check_common_url(const char* url);

int http_check_response_content(esp_http_client_handle_t client_handle);

void http_init_get_request();

void http_get_request_send(bool *flag);

//库定制函数

void asr_data_save_result(char* asr_response);

//外部自由调用功能函数
esp_err_t wifi_connect();

void init_time_data_sntp();

void refresh_position_data();

void refresh_weather_data();

char *ERNIE_Bot_4_chat_tex_exchange(char *user_content);