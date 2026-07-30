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
// 敬告：有效的数据存储变量都封装在该库下，不需要在外部函数定义一个数据结构体缓存作为参数，直接读取公共变量，主要为了方便FreeRTOS的任务支持
// github: https://github.com/701Enti
// bilibili: 701Enti

#include "zlib.h"
#include "zutil.h"
#include "inftrees.h"
#include "inflate.h"

#include <sys/time.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "sevetest30_IWEDA.h"
#include "sevetest30_sound.h"

#include "baidu_access_token.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "periph_wifi.h"
#include "board.h"
#include "board_ctrl.h"
#include "cJSON.h"
#include "audio_idf_version.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"

#include "esp_timer.h"

char http_output_buf[HTTP_BUF_MAX] = {0}; // 输出数据缓存
char http_url_buf[HTTP_BUF_MAX] = {0};    // url缓存,留着调用时候可以用
char *ip_address;                         // 公网IP

char *sevetest30_asr_result_text = NULL; // 语音识别结果

Real_time_weather *real_time_weather_data;
ip_position *ip_position_data;

esp_http_client_handle_t http_client_handle = NULL;
esp_periph_handle_t se30_wifi_periph_handle = NULL;

void transform_ip_address();
void transform_postcode();
void transform_lng_lat();
void transform_locationID();
void transform_real_time_weather_data();

/// @brief WIFI外设初始化
/// @param periph_config 网络外设配置
/// @return ESP_OK / ESP_FAIL
esp_err_t wifi_init(esp_periph_config_t *periph_config)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 初始化TCP/IP协议栈
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
    ESP_ERROR_CHECK(esp_netif_init());
#else
    tcpip_adapter_init();
#endif

    // 初始化网络外设
    se30_periph_set_handle = esp_periph_set_init(periph_config); // 获取运行配置句柄

    return ret;
}

/// @brief 通用网络连接函数
/// @param wifi_cfg 网络配置,要连接的网络SSID和密码必填
/// @return ESP_OK / ESP_FAIL
esp_err_t wifi_connect(periph_wifi_cfg_t *wifi_cfg)
{
    static const char *TAG = "wifi_connect";

    if (!se30_periph_set_handle)
    {
        ESP_LOGE(TAG, "初始化工作未完成");
        return ESP_FAIL;
    }

    if (se30_wifi_periph_handle)
    {
        ESP_LOGE(TAG, "上次的WIFI句柄未有效删除,无法连接");
        return ESP_FAIL;
    }

    se30_wifi_periph_handle = periph_wifi_init(wifi_cfg); // 获取wifi配置句柄

    esp_periph_start(se30_periph_set_handle, se30_wifi_periph_handle);                                      // 启动连接任务
    return periph_wifi_wait_for_connected(se30_wifi_periph_handle, pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS)); // 请求连接
}

/// @brief 百度API获取AccessToken,保存到char数组
/// @param client_id client_id 字符串
/// @param client_secret client_secret 字符串
/// @param AccessToken char数组地址
/// @return ESP_FAIL / ESP_OK
esp_err_t baidu_get_AccessToken(char *client_id, char *client_secret, char *AccessToken)
{
    const char *TAG = "baidu_get_AccessToken";

    if (!AccessToken)
    {
        ESP_LOGE(TAG, "需要导入一个char数组的地址,而导入的为空指针");
        return ESP_FAIL;
    }

    if (periph_wifi_is_connected(se30_wifi_periph_handle) != PERIPH_WIFI_CONNECTED)
    {
        ESP_LOGE(TAG, "网络未连接");
        return ESP_FAIL;
    }

    // 发送请求
    bool Task_comp_flag = false;                                                                // 任务是否完成标识
    snprintf(http_url_buf, HTTP_BUF_MAX, BAIDU_GET_ACCESS_TOKEN_URL, client_id, client_secret); // 确定请求URL
    http_init_get_request();
    esp_http_client_set_timeout_ms(http_client_handle, 10000);
    xTaskCreatePinnedToCore(&http_get_request_send, "http_get_request_send", 8192, &Task_comp_flag, HTTP_TASK_PRIO, NULL, HTTP_TASK_CORE); // 启动http传输任务,GET方式
    while (!Task_comp_flag)
        vTaskDelay(pdMS_TO_TICKS(200));

    // 检查是否得到请求响应的结果
    if (!strcasecmp(http_output_buf, ""))
        return ESP_FAIL;
    else
    {
        // 解析数据
        cJSON *root_data = NULL;
        root_data = cJSON_Parse(http_output_buf);
        cJSON *cjson_AccessToken = cJSON_GetObjectItem(root_data, "access_token");

        memset(AccessToken, 0, ACCESSTOKEN_SIZE_MAX * sizeof(char));                       // 清空之前的存储
        snprintf(AccessToken, ACCESSTOKEN_SIZE_MAX, "%s", cjson_AccessToken->valuestring); // 复制AccessToken

        cJSON_Delete(root_data);

        return ESP_OK;
    }
}

// 封装好的网络信息API请求服务，包含信息解析，并存储到对应结构体或缓冲变量

// 刷新位置数据
void refresh_position_data()
{

    bool Task_comp_flag = false; // 任务是否完成标识

    // 获取公网IP
    Task_comp_flag = false;
    sprintf(http_url_buf, GET_IP_ADDRESS_API_URL);
    http_init_get_request();
    xTaskCreatePinnedToCore(&http_get_request_send, "http_get_request_send", 8192, &Task_comp_flag, HTTP_TASK_PRIO, NULL, HTTP_TASK_CORE);
    while (!Task_comp_flag)
        vTaskDelay(pdMS_TO_TICKS(200));
    transform_ip_address();

    // 获取IP归属地邮政编码
    Task_comp_flag = false;
    snprintf(http_url_buf, HTTP_BUF_MAX, IP_POSITION_API_URL, ip_address);
    http_init_get_request();
    esp_http_client_set_header(http_client_handle, "token", CONFIG_IP_138_TOKEN);
    xTaskCreatePinnedToCore(&http_get_request_send, "http_get_request_send", 8192, &Task_comp_flag, HTTP_TASK_PRIO, NULL, HTTP_TASK_CORE); // 启动http传输任务,GET方式
    while (!Task_comp_flag)
        vTaskDelay(pdMS_TO_TICKS(200));
    transform_postcode();

    // ip_position_data.postcode = "343100";//调试用,并将上面这块注释来避免调试时的花费

    // 通过邮政编码获取经纬度以进行城市搜索
    // 原因有三点
    // 1.cilent库似乎没有URL中文解码，在URL装载时出现错误
    // 2.IP138的API返回可能不包含"县""市"等字，如此处返回的是两个吉安，因为这里县和市的名字一样被认定为模糊搜索，通过GeoAPI返回的是市里下级的所有县
    // 3.邮政编码大概率直接对应一个县或区，并且IP138API网页还可找到一个免费还不用鉴权的"行政区划"查询服务，会返回一个经纬度，这是GeoAPI支持的搜索关键词
    //  免费还不用鉴权的行政区划查询服务，支持多种关键词搜索： https://quhua.ipchaxun.com/
    Task_comp_flag = false;
    snprintf(http_url_buf, HTTP_BUF_MAX, TO_LNG_LAT_API_URL, ip_position_data->postcode); // 确定请求URL
    http_init_get_request();
    xTaskCreatePinnedToCore(&http_get_request_send, "http_get_request_send", 8192, &Task_comp_flag, HTTP_TASK_PRIO, NULL, HTTP_TASK_CORE); // 启动http传输任务,GET方式
    while (!Task_comp_flag)
        vTaskDelay(pdMS_TO_TICKS(200));
    transform_lng_lat();

    // 城市搜索，获取locationID
    Task_comp_flag = false;
    snprintf(http_url_buf, HTTP_BUF_MAX, GEO_API_URL, ip_position_data->lng, ip_position_data->lat, CONFIG_WEATHER_API_KEY);
    http_init_get_request();
    xTaskCreatePinnedToCore(&http_get_request_send, "http_get_request_send", 8192, &Task_comp_flag, HTTP_TASK_PRIO, NULL, HTTP_TASK_CORE); // 启动http传输任务,GET方式
    while (!Task_comp_flag)
        vTaskDelay(pdMS_TO_TICKS(200));
    transform_locationID();
}

// 刷新天气数据
void refresh_weather_data()
{
    bool Task_comp_flag = false;                                                                         // 任务是否完成标识
    snprintf(http_url_buf, HTTP_BUF_MAX, WEATHER_API_URL, ip_position_data->id, CONFIG_WEATHER_API_KEY); // 确定请求URL
    http_init_get_request();
    xTaskCreatePinnedToCore(&http_get_request_send, "http_get_request_send", 8192, &Task_comp_flag, HTTP_TASK_PRIO, NULL, HTTP_TASK_CORE); // 启动http传输任务,GET方式
    while (!Task_comp_flag)
        vTaskDelay(pdMS_TO_TICKS(200));
    transform_real_time_weather_data();
}

/// @brief 初始化系统时间数据,sntp方式
/// @brief 调用这个函数后，系统需要等待NTP服务器响应,本函数会阻塞
/// @brief 直到NTP服务器响应完成或超过设置的超时时间
/// @param timeout_ms 超时时间,单位:ms
/// @return ESP_OK 初始化成功
/// @return ESP_FAIL 请求初始化SNTP失败
/// @return ESP_ERR_TIMEOUT 初始化失败,NTP服务器响应超时
/// @return ESP_ERR_NOT_FINISHED 初始化未完成,NTP服务器在超时时间内仍然处于同步中(可能开启了平滑时间过渡模式或超时时间过短)
/// @return ESP_ERR_INVALID_STATE 网络未连接
esp_err_t init_time_data_sntp(uint32_t timeout_ms)
{
    const char *TAG = "init_time_data_sntp";
    if (periph_wifi_is_connected(se30_wifi_periph_handle) != PERIPH_WIFI_CONNECTED)
    {
        ESP_LOGE(TAG, "网络未连接");
        return ESP_ERR_INVALID_STATE;
    }

    esp_sntp_config_t sntp_cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(3, ESP_SNTP_SERVER_LIST(CONFIG_NTP_SERVER_0, CONFIG_NTP_SERVER_1, CONFIG_NTP_SERVER_2));
    esp_err_t init_ret = esp_netif_sntp_init(&sntp_cfg);
    if (init_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "请求初始化SNTP失败 %s", esp_err_to_name(init_ret));
        return init_ret;
    }

    esp_err_t sync_ret = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(timeout_ms));
    if (sync_ret == ESP_ERR_TIMEOUT)
    {
        ESP_LOGE(TAG, "初始化系统时间数据失败,NTP服务器响应超时");
        return ESP_ERR_TIMEOUT;
    }
    else if (sync_ret == ESP_ERR_NOT_FINISHED)
    {
        ESP_LOGW(TAG, "初始化系统时间数据未完成,NTP服务器在超时时间内仍然处于同步中(可能开启了平滑时间过渡模式或超时时间过短)");
        return ESP_ERR_NOT_FINISHED;
    }
    else if (sync_ret == ESP_OK)
    {
        ESP_LOGI(TAG, "初始化系统时间数据成功");
        return ESP_OK;
    }
    ESP_LOGE(TAG, "初始化系统时间数据失败 %s", esp_err_to_name(sync_ret));
    return sync_ret;
}

/// @brief 检查响应内容是不是可以直接获取资源
/// @param client_handle 连接句柄
/// @return ESP_OK 可以直接获取资源 否则(不可用/需要重定向)返回 响应码
int http_check_response_content(esp_http_client_handle_t client_handle)
{
    const char *TAG = "http_check_response_content";

    esp_http_client_fetch_headers(client_handle);                //   接收消息头
    int status = esp_http_client_get_status_code(client_handle); // 获取消息头中的响应状态信息
    int len = esp_http_client_get_content_length(client_handle); // 获取消息头中的总数据大小信息

    // 检查是否为临时重定向
    if (status == 302 || status == 307)
    {
        if (esp_http_client_is_chunked_response(client_handle) == true)
            ESP_LOGW(TAG, "需要临时重定向，响应状态-> %d ,本次传输响应数据已分块", status);
        else
            ESP_LOGW(TAG, "需要临时重定向，响应状态-> %d ，响应数据共 %d bytes", status, len);

        return status;
    }

    if (status != 200)
    {
        if (esp_http_client_is_chunked_response(client_handle) == true)
            ESP_LOGE(TAG, "本次传输响应数据已分块 但是处于不正常的响应状态 -> %d 数据将不会保存", status);
        else
            ESP_LOGE(TAG, "不正常的响应状态 -> %d 共接收到 -> %d bytes 数据将不会保存", status, len);

        return status;
    }
    else
    {
        if (esp_http_client_is_chunked_response(client_handle) == true)
            ESP_LOGI(TAG, "连接就绪，响应状态-> %d ,本次传输响应数据已分块", status);
        else
            ESP_LOGI(TAG, "连接就绪，响应状态-> %d ，响应数据共 %d bytes", status, len);

        return ESP_OK;
    }
}

/// @brief 检测URL的可用性，进行针对如音频资源的可用检查而无其他复杂上下文
/// @param url 需要检测的URL
/// @return ESP_FAIL 无法连接 ESP_OK 正确可用  否则返回 响应错误码
int http_check_common_url(const char *url)
{
    const char *TAG = "http_check_common_url";
    int ret = ESP_OK;

    if (periph_wifi_is_connected(se30_wifi_periph_handle) != PERIPH_WIFI_CONNECTED)
    {
        ESP_LOGE("http_check_common_url", "网络未连接");
        return ESP_FAIL;
    }

    sprintf(http_url_buf, url);
    http_init_get_request();

    if (esp_http_client_open(http_client_handle, 0) != ESP_OK)
    {
        ESP_LOGE(TAG, "无法打开连接的URL -> %s", url);
        esp_http_client_cleanup(http_client_handle);
        return ESP_FAIL;
    }

    // 校验响应状态与数据
    ret = http_check_response_content(http_client_handle);
    if (ret != ESP_OK)
    {
        if (ret == 302 || ret == 307)
        {
            ESP_LOGW(TAG, "需要重定向处理的URL \n-> %s", url);
        }
        else
        {
            ESP_LOGE(TAG, "响应信息发现异常的URL \n-> %s", url);
        }
    }
    esp_http_client_cleanup(http_client_handle);
    return ret;
}

/// @brief 如果这个url需要重定向,更改url内容
/// @brief 原来url数据不会清除, 只是参数url指代位置切换到location_url缓存
/// @brief 请尽快使用或复制该url, 在下次change_url_if_need_redirect调用之前
/// @param url
void change_url_if_need_redirect(char **url)
{
    const char *TAG = "change_url_if_need_redirect";

    static char location_url_buf[HTTP_BUF_MAX] = {0}; // location_url缓存
    if (periph_wifi_is_connected(se30_wifi_periph_handle) == PERIPH_WIFI_CONNECTED)
    {
        sprintf(http_url_buf, *url);
        http_init_get_request();
        if (esp_http_client_open(http_client_handle, 0) == ESP_OK)
        {
            // 校验响应状态与数据
            int ret = http_check_response_content(http_client_handle);
            if (ret == 302 || ret == 307)
            {
                // 需要重定向
                char *location = "NULL";
                esp_http_client_get_header(http_client_handle, "Location", &location);
                if (location != NULL)
                {
                    strncpy(location_url_buf, location, HTTP_BUF_MAX);
                    *url = location_url_buf;
                    ESP_LOGW(TAG, "重定向到 %s", *url);
                }
                else
                {
                    ESP_LOGE(TAG, "尝试获取重定向信息时发现问题,需要重定向但无法完成更改");
                }
            }
        }
        esp_http_client_cleanup(http_client_handle);
    }
}

/// @brief 初始化GET请求
void http_init_get_request()
{
    if (periph_wifi_is_connected(se30_wifi_periph_handle) != PERIPH_WIFI_CONNECTED)
    {
        ESP_LOGE("http_init_get_request", "网络未连接");
        return;
    }

    // 配置http传输信息
    esp_http_client_config_t http_config;
    memset(&http_config, 0, sizeof(http_config)); // 对参数初始化为0
    http_config.buffer_size_tx = HTTP_BUF_MAX;    // 发送缓冲区大小
    http_config.buffer_size = HTTP_BUF_MAX;       // 接收缓冲区大小
    http_config.url = &http_url_buf[0];           // 导入URL

    // 配置传输任务，GET方式
    http_client_handle = esp_http_client_init(&http_config);         // 获取连接句柄，之后读取状态和响应都需要这个
    esp_http_client_set_method(http_client_handle, HTTP_METHOD_GET); // GET方式

    // 清除http_get_out_buf之前的残留数据
    strcpy(http_output_buf, "");
    // URL也清一下
    strcpy(http_url_buf, "");
}

// 发送GET请求，传入flag来确定任务是否结束（结束为true，也有可能是非正常的结束），
// 输出会保存到 http_get_out_buf，本函数不提供任何实时解码，特殊API参考手册将在外部进行解析前预处理
// 如果服务器响应的数据使用chunked编码发送或gzip压缩等传输处理，不会发生报错，并且这是在设计考虑范围之内的，
// 如果出现响应数据无法解析的问题，考虑解码方式是否对应合理，本库中包含对gzip的便捷解压支持函数
// 有效的分析方式是通过抓包，参考详细响应的消息头，极少数API文档也许不会提供这些信息
void http_get_request_send(bool *flag)
{
    while (1)
    {
        const char *TAG = "http_get_request_send";
        // 对服务器发送连接请求
        esp_err_t err_flag = esp_http_client_open(http_client_handle, 0); // get请求无需额外添加报文数据
        if (err_flag != ESP_OK)
        {
            ESP_LOGE(TAG, "请求连接服务器时出现问题 -> %s", &http_url_buf[0]);
            esp_http_client_cleanup(http_client_handle); // 释放数据缓存
            *flag = true;
            vTaskDelete(NULL); // 终止任务
        }

        // 校验响应状态与数据
        if (http_check_response_content(http_client_handle) != ESP_OK)
        {
            esp_http_client_cleanup(http_client_handle);
            *flag = true;
            vTaskDelete(NULL); // 终止任务
        }

        // 读取响应内容
        esp_http_client_read_response(http_client_handle, http_output_buf, HTTP_BUF_MAX);

        esp_http_client_cleanup(http_client_handle); // 关闭连接 释放数据缓存

        *flag = true;
        vTaskDelete(NULL); // 完成，终止任务
    }
}

// 对gzip压缩后的响应，解压回原来的JSON格式，外部函数需要对传入参数有效性负责
// 输入数据选择，输出数据缓冲区选择，输入数据最大允许长度
// 对于 和风天气+ESP32 通过zlib解压gzip数据的思路可以参考这位大佬的博客，甚有帮助非常感谢：https://yuanze.wang/posts/esp32-unzip-gzip-http-response/
void gzip_decompress(void *input, void *output, int len)
{
    const char *TAG = "zlib_gzip_decompress";
    int flag = 0; // 解压状态标识
    // 配置zlib数据流
    z_stream stream_config = {0};
    stream_config.next_in = input;   // 输入数据
    stream_config.next_out = output; // 输出数据
    stream_config.avail_in = 0;      // 当前输入数据的有效字节数
    stream_config.zalloc = NULL;     // 内部内存分配状态标识，不需要
    stream_config.zfree = NULL;      // 内部内存释放状态标识，不需要
    stream_config.opaque = NULL;     // 给上述两个内存标识的私有数据对象，没有

    flag = inflateInit2(&stream_config, ZLIB_WINDOW_MAX); // 传入参数
    if (flag != Z_OK)
    {
        ESP_LOGE(TAG, "配置解压参数时出错");
        return;
    }

    // 开始解压
    while (stream_config.total_in < len)
    {
        stream_config.avail_in = 1;                 // 解压1字节
        stream_config.avail_out = 1;                // 解压1字节
        flag = inflate(&stream_config, Z_NO_FLUSH); // 解压并获取返回状态
        // 如果解压完成，正常退出解压循环，如果解压未完成却出现非正常标识，报告问题并退出函数，如果是正常标识，继续解压循环
        if (flag == Z_STREAM_END)
            break;
        else if (flag != Z_OK)
        {
            ESP_LOGE(TAG, "解压数据时出现问题，在 %lx -> %lx 时", stream_config.total_in, stream_config.total_out);
            return;
        }
    }

    ESP_LOGI(TAG, "数据解压完成 %lx -> %lx", stream_config.total_in, stream_config.total_out);

    // 最后一件事，在输出末尾添加结束标识以便识别
    ((char *)output)[stream_config.total_out] = '\0';
}

/// @brief 获取JSON_Line数据中的有效数据单元个数
/// @param data 要扫描的JSON_Line数据字符串或字符数组首地址
/// @param len 要扫描的字符长度
/// @return 合法的数据单元个数
int json_line_unit_num_get(char *data, int len)
{

    // 此处的处理思路(以下数据仅为演示,不一定为真实响应数据)
    // 当一个" { "出现,表示json数据中一个对象开始表达,出现新焦点focus_num++
    // 当一个" } "出现,表示json数据中一个对象停止表达,关闭焦点focus_num--
    //{"is_end":false,"result":"当然可以！","usage":{"prompt_tokens":5,"completion_tokens":0,"total_tokens":5}}
    //{"is_end":false,"result":"这是一个经典的笑话。","usage":{"prompt_tokens":5,"completion_tokens":0,"total_tokens":5}}
    // 假设通过参数data传入一个上面的数据整体,此函数遍历该文本数据时,仅关注"{"和"}"
    // 按照上面的逻辑,focus_num变化为[单元0] 0(变量初始化后为0) - 1 - 2 - 1 - 0 | [单元1] 0(单元1末尾变成0)  - 1 - 2 - 1 - 0
    // 显然地,focus_num在"关闭焦点"时跳变到0一次,就有一个单元;跳变到0两次,就有两个单元(保存到缓存unit_num中),以此类推,只要原数据合理,数据单元计算将不受到数据长度和复杂度的干涉

    int focus_num = 0; // 焦点个数
    int unit_num = 0;  // 扫描到的数据单元个数

    for (int idx = 0; idx < len; idx++)
    {
        if (data[idx] == '{')
            focus_num++; // 出现新焦点
        if (data[idx] == '}')
        {
            focus_num--; // 关闭焦点
            if (focus_num == 0)
                unit_num++; // focus_num在"关闭焦点"时跳变到0一次,就有一个单元
        }

        // 不合法的JSON_Line数据 - 源数据末尾被截断
        if (idx == len - 1 && focus_num != 0)
        {
            return unit_num;
        }

        // 不合法的JSON_Line数据 - 格式错误
        if (focus_num < 0)
        {
            return unit_num;
        }
    }
    return unit_num;
}

/// @brief 选定JSON_Line数据中的一个数据单元复制到外部字符缓存区
/// @param dest 外部字符缓存区,需要在外部函数提前申请好连续内存
/// @param src JSON_Line格式源数据字符串
/// @param unit_id 选定复制的数据单元ID,第一个单元为0,第二个单元为1....
/// @param max_len 最多复制max_len个字符
void json_line_unit_copy(char *dest, char *src, int unit_id, int max_len)
{

    // 此处的处理思路(以下数据仅为演示,不一定为真实响应数据)(基于上面的json_line_unit_num_get函数思路)
    // 当一个" { "出现,表示json数据中一个对象开始表达,出现新焦点focus_num++
    // 当一个" } "出现,表示json数据中一个对象停止表达,关闭焦点focus_num--
    //{"is_end":false,"result":"当然可以！","usage":{"prompt_tokens":5,"completion_tokens":0,"total_tokens":5}}
    //{"is_end":false,"result":"这是一个经典的笑话。","usage":{"prompt_tokens":5,"completion_tokens":0,"total_tokens":5}}
    // 假设传入一个上面的数据整体,此函数遍历该文本数据时,仅关注"{"和"}"
    // 按照上面的逻辑,focus_num变化为[单元0] 0(变量初始化后为0) - 1 - 2 - 1 - 0 | [单元1] 0(单元1末尾变成0)  - 1 - 2 - 1 - 0
    // 显然地,数据中任意单元开始位置是"出现新焦点"并且focus_num由0跳变到1的位置,结束位置是"关闭焦点"并且focus_num由1跳变到0的位置
    // 记录这些"坐标"以及间距,利用strncpy标准C函数的特性复制目标区域即可

    int focus_num = 0; // 焦点个数
    int unit_num = 0;  // 扫描到的数据单元个数
    int start = 0;     // 开始位置
    int total_len = 0; // 选定单元的实际总长度

    for (int idx = 0; idx < max_len; idx++)
    {
        if (src[idx] == '{')
        {
            focus_num++;                               // 出现新焦点
            if (focus_num == 1 && unit_num == unit_id) // 扫描到指定的数据单元
                start = idx;                           // 开始位置是"出现新焦点"并且focus_num由0跳变到1的位置
        }
        if (src[idx] == '}')
        {
            focus_num--; // 关闭焦点
            if (focus_num == 0)
            {
                // 现在在指定的数据单元
                if (unit_num == unit_id)
                {
                    total_len = idx - start + 1; // 选定单元的实际总长度
                    if (total_len >= max_len)
                        total_len = max_len;               // 最多复制max_len个字符
                    strncpy(dest, &src[start], total_len); // 利用strncpy标准C函数的特性复制目标区域即可
                    return;
                }
                unit_num++; // focus_num在"关闭焦点"时跳变到0一次,一个单元被扫过
            }
        }
        if (focus_num < 0)
        {
            ESP_LOGE("json_line_unit_copy", "不合法的JSON_Line数据 终止字符的下标位置-%d", idx);
            return;
        }
    }
}

// 解析返回的IP地址数据，保存到ip_address
void transform_ip_address()
{

    const char *TAG = "transform_ip_address";

    ip_address = malloc(30);
    memset(ip_address, 0, 30);
    char *buf = "0";
    uint8_t i = 0;
    while (http_output_buf[i] != '\0')
    {
        switch (http_output_buf[i])
        {
        case '0':
            buf = "0";
            break;

        case '1':
            buf = "1";
            break;

        case '2':
            buf = "2";
            break;

        case '3':
            buf = "3";
            break;

        case '4':
            buf = "4";
            break;

        case '5':
            buf = "5";
            break;

        case '6':
            buf = "6";
            break;

        case '7':
            buf = "7";
            break;

        case '8':
            buf = "8";
            break;

        case '9':
            buf = "9";
            break;

        case '.':
            buf = ".";
            break;

        default:
            goto OK;
            break;
        }
        strcat(ip_address, buf);
        i++;
    }
OK:
    buf = "\0";
    strcat(ip_address, buf);
    ESP_LOGI(TAG, "解析完毕,获取到公网IP %s (总字符数 %d)", ip_address, i);
}
// 解析返回的IP归属地址邮政编码信息，保存到ip_position_data
void transform_postcode()
{
    const char *TAG = "transform_postcode";

    // 因为返回数据中被一个find（）扩住了，json解析不了，想办法缓存有用的数据再解析,注意其中特征 "find("的位置    ")"始终为结束字符
    // find({"ret":"ok","ip":"39.158.160.240","data":["中国","江西","吉安","吉安","移动","343100","0796"]})
    char json_buf[PRE_CJSON_BUF_MAX] = {0};
    uint8_t i = 5;
    while (http_output_buf[i] != ')')
    {
        json_buf[i - 5] = http_output_buf[i];
        i++;
    }
    http_output_buf[i] = '\0';

    // 提取完成开始json解析
    cJSON *root_data = NULL;
    root_data = cJSON_Parse(json_buf);
    cJSON *cjson_data = cJSON_GetObjectItem(root_data, "data");

    cJSON *cjson_postcode = cJSON_GetArrayItem(cjson_data, 5);
    ip_position_data->postcode = cjson_postcode->valuestring;

    ESP_LOGI(TAG, "解析完毕,获取到IP归属地邮政编码 %s", cjson_postcode->valuestring);

    // cJSON_Delete(root_data); // 完成数据解析，释放cJSON，但是由于外部需要使用其中字符串数据，不进行释放
}
// 解析经纬度数据,保存到ip_position_data
void transform_lng_lat()
{
    const char *TAG = "transform_lng_lat";

    cJSON *root_data = NULL;
    root_data = cJSON_Parse(http_output_buf);

    cJSON *cjson_data = cJSON_GetObjectItem(root_data, "data");
    cJSON *cjson_results = cJSON_GetObjectItem(cjson_data, "results");

    cJSON *cjson_results_root = cJSON_GetArrayItem(cjson_results, 0);

    cJSON *cjson_lng = cJSON_GetObjectItem(cjson_results_root, "lng");
    cJSON *cjson_lat = cJSON_GetObjectItem(cjson_results_root, "lat");

    ip_position_data->lng = cjson_lng->valuestring;
    ip_position_data->lat = cjson_lat->valuestring;

    ESP_LOGI(TAG, "获取到归属地 经度为%s 纬度为%s", ip_position_data->lng, ip_position_data->lat);
    // cJSON_Delete(root_data); // 完成数据解析，释放cJSON，但是由于外部需要使用其中字符串数据，不进行释放
}
// 解析locationID数据,保存到ip_position_data,同时会进一步完善ip_position_data数据
void transform_locationID()
{
    const char *TAG = "transform_locationID";

    char json_buf[PRE_CJSON_BUF_MAX] = {0};                   // 缓存JOSN数据
    gzip_decompress(http_output_buf, json_buf, HTTP_BUF_MAX); // 由于目前和风天气响应数据经过gzip压缩，需要zlib库支持，这个解压函数是要自行按需求封装的，请看本文件该函数的声明

    cJSON *root_data = NULL;
    root_data = cJSON_Parse(json_buf);
    cJSON *cjson_location = cJSON_GetObjectItem(root_data, "location");
    cJSON *cjson_location_root = cJSON_GetArrayItem(cjson_location, 0);

    cJSON *cjson_name = cJSON_GetObjectItem(cjson_location_root, "name");
    cJSON *cjson_id = cJSON_GetObjectItem(cjson_location_root, "id");
    cJSON *cjson_adm2 = cJSON_GetObjectItem(cjson_location_root, "adm2");
    cJSON *cjson_adm1 = cJSON_GetObjectItem(cjson_location_root, "adm1");
    cJSON *cjson_country = cJSON_GetObjectItem(cjson_location_root, "country");

    ip_position_data->country = cjson_country->valuestring;
    ip_position_data->adm1 = cjson_adm1->valuestring;
    ip_position_data->adm2 = cjson_adm2->valuestring;
    ip_position_data->name = cjson_name->valuestring;
    ip_position_data->id = cjson_id->valuestring;

    ESP_LOGI(TAG, "获取到locationID %s", ip_position_data->id);
    ESP_LOGI(TAG, "详细地址：%s - %s - %s  - %s ", ip_position_data->country, ip_position_data->adm1,
             ip_position_data->adm2, ip_position_data->name);
    // cJSON_Delete(root_data); // 完成数据解析，释放cJSON，但是由于外部需要使用其中字符串数据，不进行释放
}
// 解析实时天气，保存到全局 real_time_weather_data
void transform_real_time_weather_data()
{
    const char *TAG = "transform_real_time_weather_data";

    char json_buf[PRE_CJSON_BUF_MAX] = {0};                   // 缓存JOSN数据
    gzip_decompress(http_output_buf, json_buf, HTTP_BUF_MAX); // 由于目前和风天气响应数据经过gzip压缩，需要zlib库支持，这个解压函数是要自行按需求封装的，请看本文件该函数的声明

    // JSON数据解析,当前仅需要 now 的数据 15个 全部解析
    cJSON *root_data = NULL;
    root_data = cJSON_Parse(json_buf);
    cJSON *cjson_now = cJSON_GetObjectItem(root_data, "now"); // 选定参数为为 now

    cJSON *cjson_obsTime = cJSON_GetObjectItem(cjson_now, "obsTime");     // 数据观测时间
    cJSON *cjson_temp = cJSON_GetObjectItem(cjson_now, "temp");           // 温度，摄氏度
    cJSON *cjson_feelsLike = cJSON_GetObjectItem(cjson_now, "feelsLike"); // 体感温度，摄氏度
    cJSON *cjson_icon = cJSON_GetObjectItem(cjson_now, "icon");           // 天气状况代码
    cJSON *cjson_text = cJSON_GetObjectItem(cjson_now, "text");           // 天气文字描述例如多云
    cJSON *cjson_wind360 = cJSON_GetObjectItem(cjson_now, "wind360");     // 360度风向
    cJSON *cjson_windDir = cJSON_GetObjectItem(cjson_now, "windDir");     // 风向文字描述
    cJSON *cjson_windScale = cJSON_GetObjectItem(cjson_now, "windScale"); // 风力等级
    cJSON *cjson_windSpeed = cJSON_GetObjectItem(cjson_now, "windSpeed"); // 风速
    cJSON *cjson_humidity = cJSON_GetObjectItem(cjson_now, "humidity");   // 相对湿度，百分比
    cJSON *cjson_precip = cJSON_GetObjectItem(cjson_now, "precip");       // 当前每小时降水量，毫米
    cJSON *cjson_pressure = cJSON_GetObjectItem(cjson_now, "pressure");   // 大气压强
    cJSON *cjson_vis = cJSON_GetObjectItem(cjson_now, "vis");             // 能见度,KM
    cJSON *cjson_cloud = cJSON_GetObjectItem(cjson_now, "cloud");         // 云量，可能为空
    cJSON *cjson_dew = cJSON_GetObjectItem(cjson_now, "dew");             // 露点温度，可能为空

    // 存储到 real_time_weather_data

    // 传来的是字符型，因为后续分析需要，转换成整型存储起来
    // weather_UI_1
    sscanf(cjson_temp->valuestring, "%d", &real_time_weather_data->temp);
    sscanf(cjson_icon->valuestring, "%d", &real_time_weather_data->icon);
    // weather_UI_2
    sscanf(cjson_humidity->valuestring, "%d", &real_time_weather_data->humidity);

    // real_time_weather_data.obsTime = cjson_obsTime->type;
    // real_time_weather_data.feelsLike = cjson_feelsLike->type;
    // real_time_weather_data.text = cjson_text->type;
    // real_time_weather_data.wind360 = cjson_wind360->type;
    // real_time_weather_data.windDir = cjson_windDir->type;
    // real_time_weather_data.windScale = cjson_windScale->type;
    // real_time_weather_data.windSpeed = cjson_windSpeed->type;

    // real_time_weather_data.precip = cjson_precip->type;
    // real_time_weather_data.pressure = cjson_pressure->type;
    // real_time_weather_data.vis = cjson_vis->type;

    // //可能为空
    // real_time_weather_data.cloud = cjson_cloud->type;
    // real_time_weather_data.dew = cjson_dew->type;

    ESP_LOGI(TAG, "解析完毕,实时天气数据已保存到real_time_weather_data");
    // cJSON_Delete(root_data); // 完成数据解析，释放cJSON，但是由于外部需要使用其中字符串数据，不进行释放
}

/// @brief 解析GPT 返回的数据(单个json格式数据/[流式传输]JSON_Line数据中的一个数据单元)，缓存识别结果追加到 result(自动申请内存)
/// @param line_response 单个json格式数据/[流式传输]JSON_Line数据中的一个数据单元
/// @param result 收集结果的字符串地址
void GPT_chat_transform_collect(char *line_response, char **result)
{
    static const char *TAG = "GPT_chat_transform_collect";
    if (!line_response)
    {
        ESP_LOGE(TAG, "传入了为空的输入数据");
        return;
    }

    // 如果缓存为空,申请缓存
    if (!*result)
    {
        *result = (char *)malloc(GPT_CHAT_RESPONSE_BUF_SIZE * sizeof(char));
        while (!*result)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            ESP_LOGE(TAG, "申请result资源发现问题 正在重试");
            *result = (char *)malloc(GPT_CHAT_RESPONSE_BUF_SIZE * sizeof(char));
        }
        memset(*result, 0, sizeof(GPT_CHAT_RESPONSE_BUF_SIZE * sizeof(char)));
    }

    cJSON *root_data = NULL;
    cJSON *cjson_choices = NULL;
    cJSON *cjson_choices_item = NULL;
    cJSON *cjson_delta = NULL;
    cJSON *cjson_content = NULL;

    root_data = cJSON_Parse(line_response);

    if (root_data)
        cjson_choices = cJSON_GetObjectItem(root_data, "choices");

    if (cjson_choices)
        cjson_choices_item = cJSON_GetArrayItem(cjson_choices, 0);

    if (cjson_choices_item)
        cjson_delta = cJSON_GetObjectItem(cjson_choices_item, "delta");

    if (cjson_delta)
        cjson_content = cJSON_GetObjectItem(cjson_delta, "content");

    if (!cjson_content)
    {

        ESP_LOGE(TAG, "交互出现问题,无法解析,响应内容-> %s", line_response);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    else
    {
        strncat(*result, cjson_content->valuestring, GPT_CHAT_RESPONSE_BUF_SIZE - strlen(*result) - 1);
    }

    if (root_data)
    {
        cJSON_Delete(root_data);
    }
    return;
}

/// @brief [流式传输]根据GPT返回的数据(HTTP原始响应数据) 获取聊天传输状态是否结束
/// @param http_response HTTP原始响应数据
/// @return true 结束 / false 进行中
bool GPT_stream_chat_over_status(char *http_response)
{
    static const char *TAG = "GPT_stream_chat_over_status";
    if (!http_response)
    {
        ESP_LOGE(TAG, "传入了为空的输入数据");
        return false;
    }

    char buf[10] = {0};
    if (strlen(http_response) - strlen("[DONE]\n\n") >= 0)
        strncpy(buf, http_response + strlen(http_response) - strlen("[DONE]\n\n"), sizeof(buf) - 1);

    if (strcmp(buf, "[DONE]\n\n") == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/// @brief GPT聊天传输任务,使用POST请求
/// @param chat_handle 传入聊天句柄
void GPT_chat_http_Task(GPT_chat_handle_t chat_handle)
{
    static const char *TAG = "GPT_chat_http_Task";

    while (1)
    {
        esp_err_t err_flag = ESP_OK;
        int64_t start_time = esp_timer_get_time();

        ///初始化http_client
        esp_http_client_config_t http_config;
        memset(&http_config, 0, sizeof(http_config));
        http_config.url = chat_handle->url;    // 导入url
        http_config.method = HTTP_METHOD_POST; // 使用POST请求
        chat_handle->client_handle = esp_http_client_init(&http_config);

        // 设置HTTP-HEADER
        esp_http_client_set_header(chat_handle->client_handle, "Content-Type", "application/json");
        esp_http_client_set_header(chat_handle->client_handle, "Authorization", chat_handle->auth_header_buf);

        // 设置超时时间
        esp_http_client_set_timeout_ms(chat_handle->client_handle, chat_handle->timeout_ms);

        // 对服务器发送连接请求
        err_flag = esp_http_client_open(chat_handle->client_handle, strlen(chat_handle->request_body_buf));

        if (err_flag != ESP_OK)
        {
            chat_handle->err = err_flag;
            ESP_LOGE(TAG, "连接时出现问题 -> %s", chat_handle->url);

            esp_http_client_cleanup(chat_handle->client_handle);            
            chat_handle->task_handle = NULL;
            chat_handle->is_completed = true;
            chat_handle->client_handle = NULL;

            vTaskDelete(NULL);
        }

        ESP_LOGW(TAG, "等待回答");

        // 写入请求体
        esp_http_client_write(chat_handle->client_handle, chat_handle->request_body_buf, strlen(chat_handle->request_body_buf));

        // 校验响应
        if (http_check_response_content(chat_handle->client_handle) != ESP_OK)
        {
            chat_handle->err = ESP_FAIL;
            ESP_LOGE(TAG, "校验响应时发现问题");
            esp_http_client_read_response(chat_handle->client_handle, chat_handle->response_buf, GPT_CHAT_RESPONSE_BUF_SIZE * sizeof(char));
            ESP_LOGE(TAG, "响应体内容 -> %s", chat_handle->response_buf);

            esp_http_client_close(chat_handle->client_handle);
            esp_http_client_cleanup(chat_handle->client_handle);            
            chat_handle->task_handle = NULL;
            chat_handle->is_completed = true;
            chat_handle->client_handle = NULL;

            vTaskDelete(NULL);
        }

        int read_index = 0;   // response_buf读取索引
        int complete_num = 0; // 实时已经读取的单元个数,可以为0
        int right_num = 0;    // 实时合法的单元个数,可以为0

        // 根据单元总数,解析并拼接单元内容
        while (1)
        {

            vTaskDelay(pdMS_TO_TICKS(100));

            // 获取合法单元个数
            right_num = json_line_unit_num_get(chat_handle->response_buf, GPT_CHAT_RESPONSE_BUF_SIZE);

            // 此前未读取任何响应数据,循环开始时right_num必然为0,第一次读取响应发生在下面的等待下

            // 如果有未读单元,执行读取
            if (complete_num < right_num)
            {
                memset(chat_handle->json_buf, 0, GPT_CHAT_RESPONSE_BUF_SIZE * sizeof(char));
                json_line_unit_copy(chat_handle->json_buf, chat_handle->response_buf, complete_num, GPT_CHAT_RESPONSE_BUF_SIZE); // 复制一个数据单元
                GPT_chat_transform_collect(chat_handle->json_buf, &chat_handle->result);                                         // 解析并拼接保存
                complete_num++;
            }

            //(right_num=0 complete_num=0)或者(读完最后一个单元)[已读个数 = 总个数],检验,如果数据没有完全读取完成,需要等待并读取更多下文
            if (complete_num == right_num)
            {
                if (GPT_stream_chat_over_status(chat_handle->response_buf) == false)
                {
                    while (1)
                    {
                        if (GPT_CHAT_RESPONSE_BUF_SIZE - read_index <= 0)
                        {
                            chat_handle->err = ESP_ERR_NO_MEM;
                            ESP_LOGE(TAG, "响应缓存内存空间不足 已读取内容 -> %s", chat_handle->response_buf);

                            esp_http_client_close(chat_handle->client_handle);
                            esp_http_client_cleanup(chat_handle->client_handle);                            
                            chat_handle->task_handle = NULL;
                            chat_handle->is_completed = true;
                            chat_handle->client_handle = NULL;

                            vTaskDelete(NULL);
                        }

                        read_index += esp_http_client_read_response(chat_handle->client_handle, chat_handle->response_buf + read_index, GPT_CHAT_RESPONSE_BUF_SIZE - read_index);

                        // 如果有新的发现,退出等待
                        if (json_line_unit_num_get(chat_handle->response_buf, GPT_CHAT_RESPONSE_BUF_SIZE) > right_num)
                            break;

                        vTaskDelay(pdMS_TO_TICKS(1000));
                        if ((esp_timer_get_time() - start_time) / 1000 >= chat_handle->timeout_ms)
                        {
                            chat_handle->err = ESP_ERR_TIMEOUT;
                            ESP_LOGE(TAG, "等待回复超时 已读取内容 -> %s", chat_handle->response_buf);

                            esp_http_client_close(chat_handle->client_handle);
                            esp_http_client_cleanup(chat_handle->client_handle);                            
                            chat_handle->task_handle = NULL;
                            chat_handle->is_completed = true;
                            chat_handle->client_handle = NULL;

                            vTaskDelete(NULL);
                        }
                    }
                }
                else
                {
                    if (chat_handle->result)
                    {
                        ESP_LOGI(TAG, "%s", chat_handle->result);
                        ESP_LOGI(TAG, "解析完成,共拼接%d个数据单元", complete_num);
                        chat_handle->err = ESP_OK;

                        esp_http_client_close(chat_handle->client_handle);
                        esp_http_client_cleanup(chat_handle->client_handle);                        
                        chat_handle->task_handle = NULL;
                        chat_handle->is_completed = true;
                        chat_handle->client_handle = NULL;
                    
                        vTaskDelete(NULL);
                    }
                    else
                    {
                        ESP_LOGE(TAG, "解析任务内部运行异常");
                        chat_handle->err = ESP_ERR_INVALID_STATE;

                        esp_http_client_close(chat_handle->client_handle);
                        esp_http_client_cleanup(chat_handle->client_handle);
                        chat_handle->task_handle = NULL;
                        chat_handle->is_completed = true;
                        chat_handle->client_handle = NULL;

                        vTaskDelete(NULL);
                    }
                }
            }
        }
    }
}

/// @brief [使用流式传输模式]GPT文本交互
/// @param chat_handle GPT对话句柄
/// @param task_stack 任务栈大小(单位字节)
/// @param task_core 任务核心号
/// @param task_prio 任务优先级
/// @note 这是一个阻塞函数,直到GPT文本交互完成,才会返回
/// @return
/// [ESP_OK 成功]
/// [ESP_FAIL HTTP访问错误]
/// [ESP_ERR_HTTP_CONNECT HTTP连接错误]
/// [ESP_ERR_INVALID_ARG 传入了为空的输入数据 / 空的用户内容]
/// [ESP_ERR_INVALID_STATE 解析任务内部运行异常 / 网络未连接]
/// [ESP_ERR_NO_MEM 内存不足]
/// [ESP_ERR_TIMEOUT 等待回复超时]
esp_err_t GPT_chat_text_exchange(GPT_chat_handle_t chat_handle, int task_prio)
{
    const char *TAG = "GPT_chat_text_exchange";

    if (chat_handle == NULL)
    {
        ESP_LOGE(TAG, "传入了为空的输入数据");
        return ESP_ERR_INVALID_ARG;
    }

    if (!strcasecmp(chat_handle->user_content, ""))
    {
        ESP_LOGE(TAG, "空的用户内容");
        return ESP_ERR_INVALID_ARG;
    }

    if (periph_wifi_is_connected(se30_wifi_periph_handle) != PERIPH_WIFI_CONNECTED)
    {
        ESP_LOGE(TAG, "网络未连接");
        return ESP_ERR_INVALID_STATE;
    }

    if (chat_handle->result)
    {
        free(chat_handle->result);
        chat_handle->result = NULL;
    }
    memset(chat_handle->response_buf, 0, GPT_CHAT_RESPONSE_BUF_SIZE * sizeof(char));
    memset(chat_handle->json_buf, 0, GPT_CHAT_HTTP_REQUEST_BODY_BUF_SIZE * sizeof(char));

    chat_handle->is_completed = false;

    xTaskCreatePinnedToCore(&GPT_chat_http_Task, "GPT_chat_http_Task", GPT_CHAT_TASK_STACK_SIZE, chat_handle, task_prio, &chat_handle->task_handle, GPT_CHAT_TASK_CORE);

    while (!chat_handle->is_completed)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (chat_handle->err != ESP_OK)
    {
        ESP_LOGE(TAG, "GPT文本交互任务运行异常 错误:%s", esp_err_to_name(chat_handle->err));
        return chat_handle->err;
    }

    return ESP_OK;
}

/// @brief 更新用户内容
/// @param chat_handle GPT文本交互句柄
/// @param user_content 新用户内容
/// @return ESP_OK 成功
/// @return ESP_ERR_INVALID_ARG 传入了无效的参数
/// @return ESP_ERR_INVALID_STATE 初始化未完成,request_body_buf为空
esp_err_t GPT_chat_update_user_content(GPT_chat_handle_t chat_handle, char *user_content)
{
    const char *TAG = "GPT_chat_update_user_content";

    if (chat_handle == NULL)
    {
        ESP_LOGE(TAG, "传入了为空的输入数据");
        return ESP_ERR_INVALID_ARG;
    }

    if (!chat_handle->request_body_buf)
    {
        ESP_LOGE(TAG, "初始化未完成,request_body_buf为空");
        return ESP_ERR_INVALID_STATE;
    }

    if (!user_content || !strcasecmp(user_content, ""))
    {
        ESP_LOGW(TAG, "缺少必填参数");
    }

    if (chat_handle->user_content)
    {
        free(chat_handle->user_content);
        chat_handle->user_content = NULL;
    }

    chat_handle->user_content = strdup(user_content);

    memset(chat_handle->request_body_buf, 0, GPT_CHAT_HTTP_REQUEST_BODY_BUF_SIZE * sizeof(char));
    snprintf(chat_handle->request_body_buf, GPT_CHAT_HTTP_REQUEST_BODY_BUF_SIZE,
             "{\"model\":\"%s\",\"web_search\":{\"enable\":true},\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],\"stream\":true}", chat_handle->model, chat_handle->user_content);

    ESP_LOGW(TAG, "更新用户内容:%s", chat_handle->user_content);

    return ESP_OK;
}

/// @brief 启动GPT对话
/// @param url API地址(必填)
/// @param access_key 访问密钥(必填)
/// @param model 模型名称(必填)
/// @param user_content 用户内容(选填，但是不能为空，可以为“”)
/// @param timeout_ms 超时时间(必填)(单位ms)
/// @return GPT对话句柄
/// @note 会自动申请内存，所有参数均会拷贝克隆一份到GPT对话句柄中
GPT_chat_handle_t GPT_chat_start(char *url, char *access_key, char *model, char *user_content, int timeout_ms)
{
    const char *TAG = "GPT_chat_start";

    if (url == NULL || access_key == NULL || model == NULL || user_content == NULL)
    {
        ESP_LOGE(TAG, "传入了为NULL的输入数据");
        return NULL;
    }

    if (!strcasecmp(url, "") || !strcasecmp(access_key, "") || !strcasecmp(model, ""))
    {
        ESP_LOGE(TAG, "缺少必填参数");
        return NULL;
    }

    GPT_chat_handle_t chat_handle = (GPT_chat_handle_t)malloc(sizeof(GPT_chat_t));
    if (!chat_handle)
    {
        ESP_LOGE(TAG, "申请GPT_chat_handle资源发现问题");
        return NULL;
    }
    memset(chat_handle, 0, sizeof(GPT_chat_t));

    chat_handle->url = strdup(url);
    chat_handle->access_key = strdup(access_key);
    chat_handle->model = strdup(model);

    if (!chat_handle->url || !chat_handle->access_key || !chat_handle->model)
    {
        ESP_LOGE(TAG, "克隆字符串失败，请检查内存是否足够");
        GPT_chat_stop(chat_handle);
        return NULL;
    }

    chat_handle->is_completed = false;
    chat_handle->timeout_ms = timeout_ms;
    chat_handle->err = ESP_OK;

    ESP_LOGW(TAG, "准备访问:%s", chat_handle->url);
    ESP_LOGW(TAG, "使用模型:%s", chat_handle->model);
    ESP_LOGW(TAG, "超时时间:%dms", chat_handle->timeout_ms);

    // 申请响应数据缓存
    chat_handle->response_buf = (char *)malloc(GPT_CHAT_RESPONSE_BUF_SIZE * sizeof(char));
    while (!chat_handle->response_buf)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGE(TAG, "申请response_buf资源发现问题 正在重试");
        chat_handle->response_buf = (char *)malloc(GPT_CHAT_RESPONSE_BUF_SIZE * sizeof(char));
    }
    memset(chat_handle->response_buf, 0, GPT_CHAT_RESPONSE_BUF_SIZE * sizeof(char));
    strcpy(chat_handle->response_buf, "");

    // 申请json_line格式解析缓存
    chat_handle->json_buf = (char *)malloc(GPT_CHAT_RESPONSE_BUF_SIZE * sizeof(char));
    while (!chat_handle->json_buf)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGE(TAG, "申请json_buf资源发现问题 正在重试");
        chat_handle->json_buf = (char *)malloc(GPT_CHAT_RESPONSE_BUF_SIZE * sizeof(char));
    }
    memset(chat_handle->json_buf, 0, GPT_CHAT_RESPONSE_BUF_SIZE * sizeof(char));

    // 准备Authorization头
    const char *prefix = "Bearer ";
    size_t auth_len = strlen(prefix) + strlen(chat_handle->access_key) + 1;
    chat_handle->auth_header_buf = (char *)malloc(auth_len * sizeof(char));
    while (!chat_handle->auth_header_buf)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGE(TAG, "申请auth_header_buf资源发现问题 正在重试");
        chat_handle->auth_header_buf = (char *)malloc(auth_len * sizeof(char));
    }
    memset(chat_handle->auth_header_buf, 0, auth_len * sizeof(char));
    snprintf(chat_handle->auth_header_buf, auth_len, "%s%s", prefix, chat_handle->access_key);

    // 准备HTTP-BODY
    chat_handle->request_body_buf = (char *)malloc(GPT_CHAT_HTTP_REQUEST_BODY_BUF_SIZE * sizeof(char));
    while (!chat_handle->request_body_buf)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGE(TAG, "申请request_body_buf资源发现问题 正在重试");
        chat_handle->request_body_buf = (char *)malloc(GPT_CHAT_HTTP_REQUEST_BODY_BUF_SIZE * sizeof(char));
    }
    GPT_chat_update_user_content(chat_handle,user_content);

    ESP_LOGW(TAG, "GPT文本交互开始准备已完成");

    return chat_handle;
}



/// @brief 停止GPT文本交互
/// @param chat_handle GPT文本交互句柄
/// @note 如果你不确定用这个句柄调用的GPT_chat_text_exchange是否已经返回，就不要调用这个函数，这非常危险
/// @return ESP_OK 成功
/// @return ESP_ERR_INVALID_ARG 传入了无效的参数柄
esp_err_t GPT_chat_stop(GPT_chat_handle_t chat_handle)
{
    const char *TAG = "GPT_chat_stop";
    if (chat_handle == NULL)
    {
        ESP_LOGE(TAG, "传入了为空的输入数据");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGW(TAG, "GPT文本交互停止");

    esp_http_client_close(chat_handle->client_handle);
    esp_http_client_cleanup(chat_handle->client_handle);

    chat_handle->is_completed = true;

    free(chat_handle->auth_header_buf);
    chat_handle->auth_header_buf = NULL;

    free(chat_handle->request_body_buf);
    chat_handle->request_body_buf = NULL;

    free(chat_handle->response_buf);
    chat_handle->response_buf = NULL;

    free(chat_handle->json_buf);
    chat_handle->json_buf = NULL;

    free(chat_handle->url);
    chat_handle->url = NULL;

    free(chat_handle->access_key);
    chat_handle->access_key = NULL;

    free(chat_handle->model);
    chat_handle->model = NULL;

    free(chat_handle->user_content);
    chat_handle->user_content = NULL;

    free(chat_handle->result);
    chat_handle->result = NULL;

    free(chat_handle);
    chat_handle = NULL;

    return ESP_OK;
}

/// @brief 解析百度语音识别响应的数据，缓存识别结果到 asr_result_tex
/// @param asr_response 语音识别响应的数据
void asr_data_save_result(char *asr_response)
{
    static const char *TAG = "asr_data_get_result";
    if (asr_response == NULL)
    {
        ESP_LOGE(TAG, "传入了为空的输入数据");
        return;
    }
    cJSON *root_data = NULL;
    root_data = cJSON_Parse(asr_response);
    cJSON *cjson_err_msg = cJSON_GetObjectItem(root_data, "err_msg");
    if (strcasecmp(cjson_err_msg->valuestring, "success."))
    {
        ESP_LOGE(TAG, "不是成功的响应信息 [%s]", cjson_err_msg->valuestring);
        return;
    }

    cJSON *cjson_result = cJSON_GetObjectItem(root_data, "result");
    cJSON *cjson_result_root = cJSON_GetArrayItem(cjson_result, 0);
    ESP_LOGI(TAG, "响应数据: %s", asr_response);
    ESP_LOGI(TAG, "识别结果: %s", cjson_result_root->valuestring);

    if (!sevetest30_asr_result_text)
    {
    ASR_RESULT_TEX_MALLOC:
        sevetest30_asr_result_text = (char *)malloc(ASR_RESULT_TEX_BUF_MAX * sizeof(char));
        while (!sevetest30_asr_result_text)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            ESP_LOGE(TAG, "申请sevetest30_asr_result_tex资源发现问题 正在重试");
            sevetest30_asr_result_text = (char *)malloc(ASR_RESULT_TEX_BUF_MAX * sizeof(char));
        }
        memset(sevetest30_asr_result_text, 0, sizeof(ASR_RESULT_TEX_BUF_MAX * sizeof(char)));
    }
    else
    {
        free(sevetest30_asr_result_text);
        sevetest30_asr_result_text = NULL;
        goto ASR_RESULT_TEX_MALLOC;
    }
    strncpy(sevetest30_asr_result_text, cjson_result_root->valuestring, ASR_RESULT_TEX_BUF_MAX);

    cJSON_Delete(root_data);
    return;
}

/// @brief 通过特定URL提取json数据中音乐歌词(LRC格式数据)
/// @param url  获取音乐歌词使用的完整URL
/// @param dest 读取保存到的位置
/// @param len_max 最大允许保存的字符长度
esp_err_t get_music_lyric_by_url(char *url, char *dest, int len_max)
{
    const char *TAG = "get_music_lyric_by_url";
    bool Task_comp_flag = false;               // 任务是否完成标识
    snprintf(http_url_buf, HTTP_BUF_MAX, url); // 确定请求URL
    http_init_get_request();
    xTaskCreatePinnedToCore(&http_get_request_send, "http_get_request_send", 8192, &Task_comp_flag, HTTP_TASK_PRIO, NULL, HTTP_TASK_CORE); // 启动http传输任务,GET方式
    while (!Task_comp_flag)
        vTaskDelay(pdMS_TO_TICKS(200));

    // 开始json解析
    cJSON *root_data = NULL;
    root_data = cJSON_Parse(http_output_buf);
    cJSON *cjson_lyric = cJSON_GetObjectItem(root_data, "lyric");
    if (cjson_lyric)
    {
        if (strlen(cjson_lyric->valuestring) < len_max)
        {
            strcpy(dest, cjson_lyric->valuestring);
            cJSON_Delete(root_data);
            return ESP_OK;
        }
        else
        {
            ESP_LOGE(TAG, "歌词数据长度过大");
            cJSON_Delete(root_data);
            return ESP_FAIL;
        }
    }
    else
    {
        ESP_LOGW(TAG, "发现歌词获取对选定的音乐不支持或该音乐无歌词");
        cJSON_Delete(root_data);
        return ESP_FAIL;
    }
}
