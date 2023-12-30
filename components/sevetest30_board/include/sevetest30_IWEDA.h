// 该文件由701Enti编写，包含一些sevetest30的  互联网环境中  数据获取（IWEDA）
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 附加：1 对于 和风天气+ESP32 通过zlib解压gzip数据可以参考这位大佬的博客，甚有帮助非常感谢：https://yuanze.wang/posts/esp32-unzip-gzip-http-response/
//      2  github - zlib项目 链接 https://github.com/madler/zlib
//      3  和风天气API开发文档：   https://dev.qweather.com/docs/api
// 敬告：有效的数据存储变量都封装在该库下，不需要在外部函数定义一个数据结构体缓存作为参数，直接读取公共变量，主要为了方便FreeRTOS的任务支持
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.7.30

#ifndef _SEVETEST30_IWEDA_H_
#define _SEVETEST30_IWEDA_H_
#endif

#include "esp_peripherals.h"
#include "esp_err.h"
#include "stdbool.h"

#define HTTP_BUF_MAX 2048 //http输出与URL数据缓存允许大小
#define ZLIB_WINDOW_MAX 47 //zlib数据解压窗口允许大小
#define PRE_CJSON_BUF_MAX 1024//JSON数据转换函数内，如果额外附加对JSON数据的预处理（解压或删改）,其缓冲的数组下标允许大小

#define HTTP_TASK_CORE           (1)
#define HTTP_TASK_PRIO           (2)

//各种API的URL参考，字符由%s替代

//查询IP的API,网上有很多这种
#define GET_IP_ADDRESS_API "http://myip.ipip.net/s"

//IP归属地查询API
#define IP_POSITION_API "https://api.ip138.com/ip/?ip=%s&datatype=jsonp&callback=find"

//邮政编码查询经纬度API
#define TO_LNG_LAT_API "https://quhua.ipchaxun.com/api/areas/data?zip=%s"


//和风天气GeoAPI(地址ID搜索，使用相同的KEY)
#define GEO_API_URL "https://geoapi.qweather.com/v2/city/lookup?location=%s,%s&key=%s"


//和风天气实时天气API
#if CONFIG_WEATHER_API_VIP_URL
#define WEATHER_API_URL "https://api.qweather.com/v7/weather/now?location=%s&key=%s"
#else
#define WEATHER_API_URL "https://devapi.qweather.com/v7/weather/now?location=%s&key=%s"
#endif

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

extern char *get_headers_key;//请求头 - 键
extern char *get_headers_value;//请求头 - 值

extern ip_position *ip_position_data;
extern Real_time_weather *real_time_weather_data;



esp_err_t wifi_connect(char* ssid,char* password);



void refresh_position_data();

void refresh_weather_data();

void init_time_data_sntp();

void http_send_get_request(bool *flag);

void gzip_decompress(void *input,void *output, int len);

void transform_ip_address();

void transform_postcode();

void transform_lng_lat();

void transform_locationID();

void transform_real_time_weather_data();