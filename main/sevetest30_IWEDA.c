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
// 敬告：有效的数据存储变量都封装在该库下，不需要在外部函数定义一个数据结构体缓存作为参数，直接读取公共变量，主要为了方便FreeRTOS的任务支持
// github: https://github.com/701Enti
// bilibili: 701Enti

#include "sevetest30_IWEDA.h"
#include "sevetest30_sound.h"
#include "zlib.h"
#include "zutil.h"


#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "audio_idf_version.h"
#include "board.h"
#include "board_ctrl.h"
#include "cJSON.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "periph_wifi.h"

#include "esp_timer.h"

#include "mbedtls/base64.h"
#include "monocypher-ed25519.h"

char *ip_address;                            // 公网IP
char *sevetest30_asr_result_text = NULL;     // 语音识别结果
current_weather_data_t current_weather_data; // 当前天气数据
position_data_t position_data;               // 位置数据

esp_periph_handle_t wifi_periph_handle = NULL;

void iweda_init_http_get_request(IWEDA_handle_t iweda_handle);
void iweda_http_get_request_send(IWEDA_handle_t iweda_handle);
void iweda_change_url_if_need_redirect(IWEDA_handle_t iweda_handle);

/// @brief 创建IWEDA句柄
/// @param output_buf_size 输出缓存大小，单位字节
/// @param url_buf_size URL缓存大小，单位字节
/// @return IWEDA_handle_t IWEDA句柄
/// @note 该函数会分配内存，包括句柄本身和缓存, 如果分配失败，会返回NULL
/// @note 调用者使用后，需要调用delete_iweda_handle函数释放内存
IWEDA_handle_t new_iweda_handle(int output_buf_size, int url_buf_size) {
  static const char *TAG = "new_iweda_handle";

  if (output_buf_size <= 0 || url_buf_size <= 0) {
    ESP_LOGE(TAG, "缓存大小不能小于或等于0");
    return NULL;
  }

  IWEDA_handle_t iweda_handle = malloc(sizeof(IWEDA_t));
  if (!iweda_handle) {
    ESP_LOGE(TAG, "iweda_handle缓存分配失败");
    return NULL;
  }
  memset(iweda_handle, 0, sizeof(IWEDA_t));

  iweda_handle->output_buf_size = output_buf_size;
  iweda_handle->output_buf = malloc(output_buf_size);
  if (!iweda_handle->output_buf) {
    free(iweda_handle);
    ESP_LOGE(TAG, "output_buf缓存分配失败");
    return NULL;
  }

  iweda_handle->url_buf_size = url_buf_size;
  iweda_handle->url_buf = malloc(url_buf_size);
  if (!iweda_handle->url_buf) {
    free(iweda_handle->output_buf);
    free(iweda_handle);
    ESP_LOGE(TAG, "url_buf缓存分配失败");
    return NULL;
  }

  memset(iweda_handle->output_buf, 0, output_buf_size);
  memset(iweda_handle->url_buf, 0, url_buf_size);

  return iweda_handle;
}

/// @brief 删除IWEDA句柄
/// @param iweda_handle IWEDA句柄
/// @note 该函数会释放句柄占用的内存，包括缓存和句柄本身
void delete_iweda_handle(IWEDA_handle_t iweda_handle) {
  if (!iweda_handle) {
    return;
  }
  free(iweda_handle->output_buf);
  free(iweda_handle->url_buf);
  free(iweda_handle);
}

/// @brief WIFI外设初始化
/// @param periph_config 网络外设配置
/// @return ESP_OK / ESP_FAIL
esp_err_t wifi_init(esp_periph_config_t *periph_config) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
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
  se30_periph_set_handle =
      esp_periph_set_init(periph_config); // 获取运行配置句柄

  return ret;
}

/// @brief 通用网络连接函数
/// @param wifi_cfg 网络配置,要连接的网络SSID和密码必填
/// @return ESP_OK / ESP_FAIL
esp_err_t wifi_connect(periph_wifi_cfg_t *wifi_cfg) {
  static const char *TAG = "wifi_connect";

  if (!se30_periph_set_handle) {
    ESP_LOGE(TAG, "初始化工作未完成");
    return ESP_FAIL;
  }

  if (wifi_periph_handle) {
    ESP_LOGE(TAG, "上次的WIFI句柄未有效删除,无法连接");
    return ESP_FAIL;
  }

  wifi_periph_handle = periph_wifi_init(wifi_cfg); // 获取wifi配置句柄

  esp_periph_start(se30_periph_set_handle, wifi_periph_handle); // 启动连接任务
  return periph_wifi_wait_for_connected(
      wifi_periph_handle, pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS)); // 请求连接
}

/**
 * @brief 将标准 Base64 字符串原地转换为 Base64URL 格式
 * @param str 标准 Base64 字符串指针
 */
static void base64_to_base64url(char *str) {
  if (str == NULL) {
    return;
  }
  for (; *str != '\0'; str++) {
    if (*str == '+') {
      *str = '-';
    } else if (*str == '/') {
      *str = '_';
    } else if (*str == '=') {
      *str = '\0';
      break;
    }
  }
}

/// @brief 从Base64字符串中提取Ed25519种子
/// @param b64_key Base64编码的Ed25519私钥字符串
/// @param seed 提取到的Ed25519种子，大小为32字节
/// @return ESP_OK 成功
/// @return ESP_ERR_INVALID_ARG Base64解码失败
/// @return ESP_ERR_NOT_FOUND 未找到Ed25519 seed段
esp_err_t ed25519_b64_to_seed(const char *b64_key, uint8_t seed[32]) {
  uint8_t der[256];
  size_t der_len = sizeof(der);
  uint32_t offset;

  // Base64解码失败
  if (mbedtls_base64_decode(der, der_len, &der_len, (uint8_t *)b64_key,
                            strlen(b64_key)) != 0) {
    return ESP_ERR_INVALID_ARG;
  }

  // 仅在末尾34字节区间查找，减少误匹配
  offset = (der_len >= 34) ? (der_len - 34) : 0;

  while (offset + 34 <= der_len) {
    if (der[offset] == 0x04 && der[offset + 1] == 0x20) {
      // 向前20字节校验Ed25519专属OID 0x2b 0x65 0x70
      uint32_t check_start = (offset > 20) ? (offset - 20) : 0;
      for (uint32_t p = check_start; p < offset - 2; p++) {
        if (der[p] == 0x2b && der[p + 1] == 0x65 && der[p + 2] == 0x70) {
          memcpy(seed, der + offset + 2, 32);
          return ESP_OK;
        }
      }
    }
    offset++;
  }

  // 未找到合法Ed25519 seed段
  return ESP_ERR_NOT_FOUND;
}

/**
 * @brief 生成 JWT Token(使用ED25519)(含"Bearer "前缀)
 * @param kid 凭据 ID
 * @param sub 项目 ID
 * @param private_key_pem 私钥字符串
 * (去掉-----BEGIN/END-----、无换行空格的纯base64私钥串
 * @return 生成的 JWT Token 字符串指针，失败返回 NULL
 * @note 使用完生成的 JWT Token 后，需要手动调用 free() 函数释放内存
 * @note 生成的 JWT Token 有效期为 1 天
 */
char *generate_ed25519_jwt_token(const char *kid, const char *sub,
                                 const char *private_key) {
  static const char *TAG = "generate_ed25519_jwt_token";

  // 检查参数
  if (!kid || !sub || !private_key) {
    ESP_LOGE(TAG, "参数不能为空");
    return NULL;
  }

  // 构造 JWT Header
  char header[128];
  // snprintf(header, sizeof(header),
  // "{\"alg\":\"EdDSA\",\"typ\":\"JWT\",\"kid\":\"%s\"}", kid);
  snprintf(header, sizeof(header), "{\"alg\":\"EdDSA\",\"kid\":\"%s\"}", kid);

  // 构造 JWT Payload
  time_t now = time(NULL);
  char payload[128];
  snprintf(payload, sizeof(payload), "{\"sub\":\"%s\",\"iat\":%ld,\"exp\":%ld}",
           sub, (long)(now - 30),
           (long)(now + QWEATHER_JWT_TOKEN_TERM_OF_VALIDITY));

  // 对 Header 进行 Base64URL 编码
  unsigned char b64_header[256];
  size_t header_len = 0;
  if (mbedtls_base64_encode(b64_header, sizeof(b64_header), &header_len,
                            (unsigned char *)header, strlen(header)) != 0) {
    ESP_LOGE(TAG, "请求头 Base64 编码失败");
    return NULL;
  }
  base64_to_base64url((char *)b64_header);

  // 对 Payload 进行 Base64URL 编码
  unsigned char b64_payload[256];
  size_t payload_len = 0;
  if (mbedtls_base64_encode(b64_payload, sizeof(b64_payload), &payload_len,
                            (unsigned char *)payload, strlen(payload)) != 0) {
    ESP_LOGE(TAG, "载荷 Base64 编码失败");
    return NULL;
  }
  base64_to_base64url((char *)b64_payload);

  // 拼接待签名消息
  char message[512];
  snprintf(message, sizeof(message), "%.*s.%.*s", (int)header_len, b64_header,
           (int)payload_len, b64_payload);

  // Base64私钥解码成DER二进制
  uint8_t der_buf[256] = {0};
  size_t der_len = sizeof(der_buf);
  int mb_ret =
      mbedtls_base64_decode(der_buf, der_len, &der_len,
                            (const uint8_t *)private_key, strlen(private_key));
  if (mb_ret != 0) {
    ESP_LOGE(TAG, "私钥Base64解码失败");
    return NULL;
  }

  // 解析PKCS8提取ed25519种子
  uint8_t seed[32] = {0};
  esp_err_t ret = ed25519_b64_to_seed(private_key, seed);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "私钥解析失败 esp_err_t: %d", ret);
    return NULL;
  }

  // 派生secret_key
  uint8_t secret_key[64] = {0};
  uint8_t public_key[32] = {0};

  crypto_ed25519_key_pair(secret_key, public_key, seed);

  // 执行Ed25519签名
  unsigned char signature[64] = {0};
  size_t sig_len = 64;
  crypto_ed25519_sign(signature, secret_key, (uint8_t *)message,
                      strlen(message));

  // 对签名进行 Base64URL 编码
  unsigned char b64_signature[128];
  size_t sig_b64_len = 0;
  if (mbedtls_base64_encode(b64_signature, sizeof(b64_signature), &sig_b64_len,
                            signature, sig_len) != 0) {
    ESP_LOGE(TAG, "签名 Base64 编码失败");
    return NULL;
  }
  base64_to_base64url((char *)b64_signature);

  // 拼接最终 JWT Token,含"Bearer "前缀
  char token[512] = {0};
  snprintf(token, sizeof(token), "Bearer %.*s.%.*s.%.*s", (int)header_len,
           b64_header, (int)payload_len, b64_payload, (int)sig_b64_len,
           b64_signature);

  ESP_LOGI(TAG, "JWT 令牌生成成功 %s", token);
  return strdup(token);
}

/// @brief 获取和风天气 JWT Token,含"Bearer "前缀
/// @return JWT Token 字符串指针
/// @note 自动管理，只要令牌有效期剩余一半，就刷新令牌，无需手动调用 free()
/// 函数释放内存
char *get_qweather_jwt_token() {
  static const char *TAG = "get_qweather_jwt_token";
  static char *token = NULL;
  static time_t last_time = 0;
  // 只要令牌有效期剩余一半，就刷新令牌
  if (token != NULL &&
      time(NULL) - last_time > QWEATHER_JWT_TOKEN_TERM_OF_VALIDITY / 2) {
    free(token);
    token = NULL;
    last_time = time(NULL);
  }
  if (token == NULL) {
    token = generate_ed25519_jwt_token(
        CONFIG_QWEATHER_API_JWT_CREDENTIAL_ID,
        CONFIG_QWEATHER_API_JWT_PROJECT_ID,
        CONFIG_QWEATHER_API_JWT_PRIVATE_KEY_WITHOUT_HEADER_FOOTER);
    if (token == NULL) {
      return NULL;
    } else {
      last_time = time(NULL);
      return token;
    }
  } else {
    ESP_LOGI(TAG, "使用已存在的JWT 令牌(生成时间：%ld)", (long)last_time);
    return token;
  }
}

/// @brief 百度API获取AccessToken,保存到char数组
/// @param client_id client_id 字符串
/// @param client_secret client_secret 字符串
/// @param AccessToken char数组地址
/// @return ESP_FAIL / ESP_OK
esp_err_t baidu_api_get_access_token(char *client_id, char *client_secret,
                                     char *AccessToken) {
  const char *TAG = "baidu_api_get_access_token";

  if (!AccessToken) {
    ESP_LOGE(TAG, "需要导入一个char数组的地址,而导入的为空指针");
    return ESP_FAIL;
  }

  if (!client_id || !client_secret) {
    ESP_LOGE(TAG, "client_id或client_secret为空");
    return ESP_FAIL;
  }

  if (periph_wifi_is_connected(wifi_periph_handle) != PERIPH_WIFI_CONNECTED) {
    ESP_LOGE(TAG, "网络未连接");
    return ESP_FAIL;
  }

  IWEDA_handle_t iweda_handle = new_iweda_handle(IWEDA_DEFAULT_OUTPUT_BUF_SIZE,
                                                 IWEDA_DEFAULT_URL_BUF_SIZE);
  if (!iweda_handle) {
    ESP_LOGE(TAG, "iweda_handle创建失败");
    return ESP_FAIL;
  }

  snprintf(iweda_handle->url_buf, iweda_handle->url_buf_size,
           BAIDU_GET_ACCESS_TOKEN_URL, client_id,
           client_secret); // 确定请求URL
  iweda_init_http_get_request(iweda_handle);
  esp_http_client_set_timeout_ms(iweda_handle->http_client_handle, 10000);
  xTaskCreatePinnedToCore((TaskFunction_t)iweda_http_get_request_send, TAG, 8192,
                          iweda_handle, HTTP_TASK_PRIO, NULL,
                          HTTP_TASK_CORE); // 启动http传输任务,GET方式
  while (!iweda_handle->is_completed)
    vTaskDelay(pdMS_TO_TICKS(200));

  // 检查是否得到请求响应的结果
  if (!strcasecmp(iweda_handle->output_buf, "")) {
    delete_iweda_handle(iweda_handle);
    return ESP_FAIL;
  } else {
    // 解析数据
    cJSON *root_data = NULL;
    cJSON *cjson_AccessToken = NULL;

    root_data = cJSON_Parse(iweda_handle->output_buf);
    if (root_data) {
      cjson_AccessToken = cJSON_GetObjectItem(root_data, "access_token");
      if (cjson_AccessToken) {
        memset(AccessToken, 0,
               BAIDU_API_ACCESSTOKEN_SIZE_MAX * sizeof(char)); // 清空之前的存储
        if (cjson_AccessToken->valuestring) {
          snprintf(AccessToken, BAIDU_API_ACCESSTOKEN_SIZE_MAX, "%s",
                   cjson_AccessToken->valuestring); // 复制AccessToken
        }
      }
      cJSON_Delete(root_data);
    }

    delete_iweda_handle(iweda_handle);
    return ESP_OK;
  }
}

/// @brief 初始化系统时间数据,sntp方式
/// @brief 调用这个函数后，系统需要等待NTP服务器响应,本函数会阻塞
/// @brief 直到NTP服务器响应完成或超过设置的超时时间
/// @param timeout_ms 超时时间,单位:ms
/// @return ESP_OK 初始化成功
/// @return ESP_FAIL 请求初始化SNTP失败
/// @return ESP_ERR_TIMEOUT 初始化失败,NTP服务器响应超时
/// @return ESP_ERR_NOT_FINISHED
/// 初始化未完成,NTP服务器在超时时间内仍然处于同步中(可能开启了平滑时间过渡模式或超时时间过短)
/// @return ESP_ERR_INVALID_STATE 网络未连接
esp_err_t init_time_data_sntp(uint32_t timeout_ms) {
  const char *TAG = "init_time_data_sntp";

  if (periph_wifi_is_connected(wifi_periph_handle) != PERIPH_WIFI_CONNECTED) {
    ESP_LOGE(TAG, "网络未连接");
    return ESP_ERR_INVALID_STATE;
  }

  esp_sntp_config_t sntp_cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(
      3, ESP_SNTP_SERVER_LIST(CONFIG_NTP_SERVER_0, CONFIG_NTP_SERVER_1,
                              CONFIG_NTP_SERVER_2));
  esp_err_t init_ret = esp_netif_sntp_init(&sntp_cfg);
  if (init_ret != ESP_OK) {
    ESP_LOGE(TAG, "请求初始化SNTP失败 %s", esp_err_to_name(init_ret));
    return init_ret;
  }

  esp_err_t sync_ret = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(timeout_ms));
  if (sync_ret == ESP_ERR_TIMEOUT) {
    ESP_LOGE(TAG, "初始化系统时间数据失败,NTP服务器响应超时");
    return ESP_ERR_TIMEOUT;
  } else if (sync_ret == ESP_ERR_NOT_FINISHED) {
    ESP_LOGW(TAG,
             "初始化系统时间数据未完成,NTP服务器在超时时间内仍然处于同步中("
             "可能开启了平滑时间过渡模式或超时时间过短)");
    return ESP_ERR_NOT_FINISHED;
  } else if (sync_ret == ESP_OK) {
    ESP_LOGI(TAG, "初始化系统时间数据成功");
    return ESP_OK;
  }
  ESP_LOGE(TAG, "初始化系统时间数据失败 %s", esp_err_to_name(sync_ret));
  return sync_ret;
}

/// @brief 检查响应内容是不是可以直接获取资源
/// @param iweda_handle IWEDA句柄
/// @return ESP_OK 可以直接获取资源 否则(不可用/需要重定向)返回 响应码
int iweda_check_response_content(IWEDA_handle_t iweda_handle) {
  const char *TAG = "iweda_check_response_content";

  if (!iweda_handle) {
    ESP_LOGE(TAG, "iweda_handle为NULL");
    return ESP_FAIL;
  }

  esp_http_client_fetch_headers(
      iweda_handle->http_client_handle); //   接收消息头
  int status = esp_http_client_get_status_code(
      iweda_handle->http_client_handle); // 获取消息头中的响应状态信息
  int len = esp_http_client_get_content_length(
      iweda_handle->http_client_handle); // 获取消息头中的总数据大小信息
  esp_http_client_read_response(iweda_handle->http_client_handle,
                                iweda_handle->output_buf,
                                iweda_handle->output_buf_size); // 接收消息体

  // 检查是否为临时重定向
  if (status == 302 || status == 307) {
    if (esp_http_client_is_chunked_response(iweda_handle->http_client_handle) ==
        true)
      ESP_LOGW(TAG, "[%s] 需要临时重定向，响应状态[%d] ,本次传输响应数据已分块",
               iweda_handle->url_buf, status);
    else
      ESP_LOGW(TAG,
               "[%s] 需要临时重定向，响应状态[%d] ，响应数据(共 %d bytes)-> %s",
               iweda_handle->url_buf, status, len, iweda_handle->output_buf);

    return status;
  }

  if (status != 200) {
    if (esp_http_client_is_chunked_response(iweda_handle->http_client_handle) ==
        true)
      ESP_LOGE(TAG,
               "[%s] 本次传输响应数据已分块 但是处于不正常的响应状态[%d] "
               "数据将不会保存，响应数据(共 %d bytes)-> %s",
               iweda_handle->url_buf, status, len, iweda_handle->output_buf);
    else
      ESP_LOGE(TAG, "[%s] 不正常的响应状态[%d]，响应数据(共 %d bytes)-> %s",
               iweda_handle->url_buf, status, len, iweda_handle->output_buf);

    return status;
  } else {
    if (esp_http_client_is_chunked_response(iweda_handle->http_client_handle) ==
        true)
      ESP_LOGI(TAG, "[%s] 连接就绪，响应状态-> %d ,本次传输响应数据已分块",
               iweda_handle->url_buf, status);
    else
      ESP_LOGI(TAG, "[%s] 连接就绪，响应状态-> %d ，响应数据共 %d bytes",
               iweda_handle->url_buf, status, len);

    return ESP_OK;
  }
}

/// @brief 检测URL的可用性，进行针对如音频资源的可用检查而无其他复杂上下文
/// @param iweda_handle IWEDA句柄
/// @return ESP_FAIL 无法连接 ESP_OK 正确可用  否则返回 响应错误码
int iweda_check_common_url(IWEDA_handle_t iweda_handle) {
  const char *TAG = "iweda_check_common_url";
  int ret = ESP_OK;

  if (!iweda_handle) {
    ESP_LOGE(TAG, "iweda_handle为NULL");
    return ESP_FAIL;
  }

  if (periph_wifi_is_connected(wifi_periph_handle) != PERIPH_WIFI_CONNECTED) {
    ESP_LOGE(TAG, "网络未连接");
    return ESP_FAIL;
  }

  iweda_init_http_get_request(iweda_handle);

  if (esp_http_client_open(iweda_handle->http_client_handle, 0) != ESP_OK) {
    ESP_LOGE(TAG, "无法打开连接的URL -> %s", iweda_handle->url_buf);
    esp_http_client_cleanup(iweda_handle->http_client_handle);
    return ESP_FAIL;
  }

  // 校验响应状态与数据
  ret = iweda_check_response_content(iweda_handle);
  if (ret != ESP_OK) {
    if (ret == 302 || ret == 307) {
      ESP_LOGW(TAG, "需要重定向处理的URL \n-> %s", iweda_handle->url_buf);
    } else {
      ESP_LOGE(TAG, "响应信息发现异常的URL \n-> %s", iweda_handle->url_buf);
    }
  }
  esp_http_client_cleanup(iweda_handle->http_client_handle);
  return ret;
}

/// @brief 如果当前URL需要重定向,更改URL内容
/// @param iweda_handle IWEDA句柄
void iweda_change_url_if_need_redirect(IWEDA_handle_t iweda_handle) {
  const char *TAG = "iweda_change_url_if_need_redirect";

  if (!iweda_handle) {
    ESP_LOGE(TAG, "iweda_handle为NULL");
    return;
  }

  if (periph_wifi_is_connected(wifi_periph_handle) == PERIPH_WIFI_CONNECTED) {
    iweda_init_http_get_request(iweda_handle);
    if (esp_http_client_open(iweda_handle->http_client_handle, 0) == ESP_OK) {
      // 校验响应状态与数据
      int ret = iweda_check_response_content(iweda_handle);
      if (ret == 302 || ret == 307) {
        // 需要重定向
        char *location = "NULL";
        esp_http_client_get_header(iweda_handle->http_client_handle, "Location",
                                   &location);
        if (location != NULL) {
          snprintf(iweda_handle->url_buf, iweda_handle->url_buf_size, "%s",
                   location);
          ESP_LOGW(TAG, "重定向到 %s", iweda_handle->url_buf);
        } else {
          ESP_LOGE(TAG,
                   "尝试获取重定向信息时发现问题,需要重定向但无法完成更改");
        }
      }
    }
    esp_http_client_cleanup(iweda_handle->http_client_handle);
  }
}

/**
 * @brief 将 UTF-8 字符串进行 URL 编码
 *
 * @param src 源字符串指针
 * @param dest 目标字符数组（存放编码后的结果）
 * @param dest_len 目标数组的总大小
 * @return esp_err_t ESP_OK 成功，ESP_ERR_NO_MEM 空间不足，ESP_ERR_INVALID_ARG
 * 参数错误
 */
esp_err_t url_encode(const char *src, char *dest, size_t dest_len) {

  if (src == NULL || dest == NULL || dest_len == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  static const char *hex = "0123456789ABCDEF";
  size_t i = 0; // 源字符串索引
  size_t j = 0; // 目标字符串索引

  while (src[i] != '\0') {
    unsigned char c = src[i];

    // 判断是否是安全字符（字母、数字、- _ . ~）
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      // 检查空间：安全字符需要 1 个字节 + 1 个结束符
      if (j + 2 > dest_len) {
        return ESP_ERR_NO_MEM;
      }
      dest[j++] = c;
    } else {
      // 检查空间：非安全字符需要 3 个字节(如 %E5) + 1 个结束符
      if (j + 4 > dest_len) {
        return ESP_ERR_NO_MEM;
      }
      dest[j++] = '%';
      dest[j++] = hex[c >> 4];
      dest[j++] = hex[c & 0x0F];
    }
    i++;
  }

  dest[j] = '\0'; // 安全补上字符串结束符
  return ESP_OK;
}

/// @brief 初始化GET请求
/// @param iweda_handle IWEDA句柄
void iweda_init_http_get_request(IWEDA_handle_t iweda_handle) {

  if (!iweda_handle)
    return;

  if (periph_wifi_is_connected(wifi_periph_handle) != PERIPH_WIFI_CONNECTED) {
    ESP_LOGE("iweda_init_http_get_request", "网络未连接");
    return;
  }

  esp_http_client_config_t config;
  memset(&config, 0, sizeof(config)); // 对参数初始化为0
  config.url = iweda_handle->url_buf; // 导入URL

  iweda_handle->http_client_handle = esp_http_client_init(&config);
  esp_http_client_set_method(iweda_handle->http_client_handle, HTTP_METHOD_GET);

  // 清除残留数据
  memset(iweda_handle->output_buf, 0, iweda_handle->output_buf_size);
  strcpy(iweda_handle->output_buf, "");

  iweda_handle->is_completed = false;
  return;
}

/// @brief 发送GET请求
/// @note 响应内容将保存到iweda_handle->output_buf中
/// @note 可以查看iweda_handle->is_completed判断是否完成请求
/// @param iweda_handle IWEDA句柄
void iweda_http_get_request_send(IWEDA_handle_t iweda_handle) {
  while (1) {
    const char *TAG = "iweda_http_get_request_send";

    if (!iweda_handle) {
      ESP_LOGE(TAG, "iweda_handle为NULL");
      vTaskDelete(NULL); // 终止任务
    }

    // 对服务器发送连接请求
    esp_err_t err_flag =
        esp_http_client_open(iweda_handle->http_client_handle, 0);
    if (err_flag != ESP_OK) {
      ESP_LOGE(TAG, "请求连接服务器时出现问题 -> %s", iweda_handle->url_buf);
      esp_http_client_cleanup(iweda_handle->http_client_handle);
      iweda_handle->is_completed = true;
      vTaskDelete(NULL); // 终止任务
    }

    // 校验响应状态与数据
    if (iweda_check_response_content(iweda_handle) != ESP_OK) {
      esp_http_client_cleanup(iweda_handle->http_client_handle);
      iweda_handle->is_completed = true;
      vTaskDelete(NULL); // 终止任务
    }

    // 读取响应内容
    esp_http_client_read_response(iweda_handle->http_client_handle,
                                  iweda_handle->output_buf,
                                  iweda_handle->output_buf_size);

    esp_http_client_cleanup(
        iweda_handle->http_client_handle); // 关闭连接 释放数据缓存

    iweda_handle->is_completed = true;
    vTaskDelete(NULL); // 完成，终止任务
  }
}

/// @brief 解压gzip压缩后的响应数据
/// @param input 输入数据指针
/// @param input_len 输入数据长度
/// @param output 输出数据指针
void gzip_decompress(void *input, int input_len, void *output) {
  const char *TAG = "gzip_decompress";
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
  if (flag != Z_OK) {
    ESP_LOGE(TAG, "配置解压参数时出错");
    return;
  }

  // 开始解压
  while (stream_config.total_in < input_len) {
    stream_config.avail_in = 1;                 // 解压1字节
    stream_config.avail_out = 1;                // 解压1字节
    flag = inflate(&stream_config, Z_NO_FLUSH); // 解压并获取返回状态
    // 如果解压完成，正常退出解压循环，如果解压未完成却出现非正常标识，报告问题并退出函数，如果是正常标识，继续解压循环
    if (flag == Z_STREAM_END)
      break;
    else if (flag != Z_OK) {
      ESP_LOGE(TAG, "解压数据时出现问题，在 0x%lx -> 0x%lx 时",
               stream_config.total_in, stream_config.total_out);
      return;
    }
  }

  ESP_LOGI(TAG, "数据解压完成 0x%lx -> 0x%lx", stream_config.total_in,
           stream_config.total_out);

  // 最后一件事，在输出末尾添加结束标识以便识别
  ((char *)output)[stream_config.total_out] = '\0';
}

/// @brief 获取JSON_Line数据中的有效数据单元个数
/// @param data 要扫描的JSON_Line数据字符串或字符数组首地址
/// @param len 要扫描的字符长度
/// @return 合法的数据单元个数
int json_line_unit_num_get(char *data, int len) {

  // 此处的处理思路(以下数据仅为演示,不一定为真实响应数据)
  // 当一个" { "出现,表示json数据中一个对象开始表达,出现新焦点focus_num++
  // 当一个" } "出现,表示json数据中一个对象停止表达,关闭焦点focus_num--
  //{"is_end":false,"result":"当然可以！","usage":{"prompt_tokens":5,"completion_tokens":0,"total_tokens":5}}
  //{"is_end":false,"result":"这是一个经典的笑话。","usage":{"prompt_tokens":5,"completion_tokens":0,"total_tokens":5}}
  // 假设通过参数data传入一个上面的数据整体,此函数遍历该文本数据时,仅关注"{"和"}"
  // 按照上面的逻辑,focus_num变化为[单元0] 0(变量初始化后为0) - 1 - 2 - 1 - 0 |
  // [单元1] 0(单元1末尾变成0)  - 1 - 2 - 1 - 0
  // 显然地,focus_num在"关闭焦点"时跳变到0一次,就有一个单元;跳变到0两次,就有两个单元(保存到缓存unit_num中),以此类推,只要原数据合理,数据单元计算将不受到数据长度和复杂度的干涉

  int focus_num = 0; // 焦点个数
  int unit_num = 0;  // 扫描到的数据单元个数

  for (int idx = 0; idx < len; idx++) {
    if (data[idx] == '{')
      focus_num++; // 出现新焦点
    if (data[idx] == '}') {
      focus_num--; // 关闭焦点
      if (focus_num == 0)
        unit_num++; // focus_num在"关闭焦点"时跳变到0一次,就有一个单元
    }

    // 不合法的JSON_Line数据 - 源数据末尾被截断
    if (idx == len - 1 && focus_num != 0) {
      return unit_num;
    }

    // 不合法的JSON_Line数据 - 格式错误
    if (focus_num < 0) {
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
void json_line_unit_copy(char *dest, char *src, int unit_id, int max_len) {

  // 此处的处理思路(以下数据仅为演示,不一定为真实响应数据)(基于上面的json_line_unit_num_get函数思路)
  // 当一个" { "出现,表示json数据中一个对象开始表达,出现新焦点focus_num++
  // 当一个" } "出现,表示json数据中一个对象停止表达,关闭焦点focus_num--
  //{"is_end":false,"result":"当然可以！","usage":{"prompt_tokens":5,"completion_tokens":0,"total_tokens":5}}
  //{"is_end":false,"result":"这是一个经典的笑话。","usage":{"prompt_tokens":5,"completion_tokens":0,"total_tokens":5}}
  // 假设传入一个上面的数据整体,此函数遍历该文本数据时,仅关注"{"和"}"
  // 按照上面的逻辑,focus_num变化为[单元0] 0(变量初始化后为0) - 1 - 2 - 1 - 0 |
  // [单元1] 0(单元1末尾变成0)  - 1 - 2 - 1 - 0
  // 显然地,数据中任意单元开始位置是"出现新焦点"并且focus_num由0跳变到1的位置,结束位置是"关闭焦点"并且focus_num由1跳变到0的位置
  // 记录这些"坐标"以及间距,利用strncpy标准C函数的特性复制目标区域即可

  int focus_num = 0; // 焦点个数
  int unit_num = 0;  // 扫描到的数据单元个数
  int start = 0;     // 开始位置
  int total_len = 0; // 选定单元的实际总长度

  for (int idx = 0; idx < max_len; idx++) {
    if (src[idx] == '{') {
      focus_num++;                               // 出现新焦点
      if (focus_num == 1 && unit_num == unit_id) // 扫描到指定的数据单元
        start = idx; // 开始位置是"出现新焦点"并且focus_num由0跳变到1的位置
    }
    if (src[idx] == '}') {
      focus_num--; // 关闭焦点
      if (focus_num == 0) {
        // 现在在指定的数据单元
        if (unit_num == unit_id) {
          total_len = idx - start + 1; // 选定单元的实际总长度
          if (total_len >= max_len)
            total_len = max_len; // 最多复制max_len个字符
          strncpy(dest, &src[start],
                  total_len); // 利用strncpy标准C函数的特性复制目标区域即可
          return;
        }
        unit_num++; // focus_num在"关闭焦点"时跳变到0一次,一个单元被扫过
      }
    }
    if (focus_num < 0) {
      ESP_LOGE("json_line_unit_copy",
               "不合法的JSON_Line数据 终止字符的下标位置-%d", idx);
      return;
    }
  }
}

/// @brief 解析返回的IP地址数据，保存到ip_address
/// @param iweda_handle IWEDA句柄
void transform_ip_address(IWEDA_handle_t iweda_handle) {
  const char *TAG = "transform_ip_address";

  if (!iweda_handle) {
    ESP_LOGE(TAG, "iweda_handle为NULL");
    return;
  }

  ip_address = malloc(30);
  memset(ip_address, 0, 30);
  char *buf = "0";
  uint8_t i = 0;
  while (iweda_handle->output_buf[i] != '\0') {
    switch (iweda_handle->output_buf[i]) {
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

/// @brief 解析IP138返回的IP归属地址信息，保存到position_data
/// @note API文档：https://www.ip138.com/api/
void transform_ip_position_ip138(IWEDA_handle_t iweda_handle) {
  const char *TAG = "transform_ip_position_ip138";

  if (!iweda_handle) {
    ESP_LOGE(TAG, "iweda_handle为NULL");
    return;
  }

  // 因为返回数据中被一个find（）扩住了，json解析不了，想办法缓存有用的数据再解析,注意其中特征
  // "find("的位置    ")"始终为结束字符
  // find({"ret":"ok","ip":"39.158.160.240","data":["中国","江西","吉安","吉安","移动","343100","0796"]})
  char json_buf[PRE_CJSON_BUF_MAX] = {0};
  uint8_t i = 5;
  while (iweda_handle->output_buf[i] != ')') {
    json_buf[i - 5] = iweda_handle->output_buf[i];
    i++;
    if (i > strlen(iweda_handle->output_buf)) {
      ESP_LOGE(TAG, "位置信息无法解析");
      return;
    }
  }
  iweda_handle->output_buf[i] = '\0';

  // 提取完成开始json解析
  cJSON *root_data = NULL;
  cJSON *cjson_data = NULL;
  cJSON *cjson_country = NULL;
  cJSON *cjson_adm1 = NULL;
  cJSON *cjson_adm2 = NULL;
  cJSON *cjson_name = NULL;

  root_data = cJSON_Parse(json_buf);

  if (root_data) {
    cjson_data = cJSON_GetObjectItem(root_data, "data");
    if (cjson_data) {
      cjson_country = cJSON_GetArrayItem(cjson_data, 0);
      cjson_adm1 = cJSON_GetArrayItem(cjson_data, 1);
      cjson_adm2 = cJSON_GetArrayItem(cjson_data, 2);
      cjson_name = cJSON_GetArrayItem(cjson_data, 3);
      if (cjson_country && cjson_country->valuestring) {
        if (position_data.country != NULL) {
          free(position_data.country);
          position_data.country = NULL;
        }
        position_data.country = strdup(cjson_country->valuestring);
        ESP_LOGI(TAG, "获取到IP归属地国家 %s", position_data.country);
      }
      if (cjson_adm1 && cjson_adm1->valuestring) {
        if (position_data.adm1 != NULL) {
          free(position_data.adm1);
          position_data.adm1 = NULL;
        }
        position_data.adm1 = strdup(cjson_adm1->valuestring);
        ESP_LOGI(TAG, "获取到IP归属地adm1 %s", position_data.adm1);
      }
      if (cjson_adm2 && cjson_adm2->valuestring) {
        if (position_data.adm2 != NULL) {
          free(position_data.adm2);
          position_data.adm2 = NULL;
        }
        position_data.adm2 = strdup(cjson_adm2->valuestring);
        ESP_LOGI(TAG, "获取到IP归属地adm2 %s", position_data.adm2);
      }
      if (cjson_name && cjson_name->valuestring) {
        if (position_data.name != NULL) {
          free(position_data.name);
          position_data.name = NULL;
        }
        position_data.name = strdup(cjson_name->valuestring);
        ESP_LOGI(TAG, "获取到IP归属地name %s", position_data.name);
      }
    }
    cJSON_Delete(root_data);
  }
}

/// @brief 通过高德地图API-POI搜索，获取高德地图经纬度数据,保存到position_data
/// @param iweda_handle IWEDA句柄
/// @note API文档：https://lbs.amap.com/api/webservice/guide/api-advanced/search
void transform_lng_lat_amap(IWEDA_handle_t iweda_handle) {
  const char *TAG = "transform_lng_lat_amap";

  if (!iweda_handle) {
    ESP_LOGE(TAG, "iweda_handle为NULL");
    return;
  }

  cJSON *root_data = NULL;
  cJSON *cjson_pois = NULL;
  cJSON *cjson_pois_item = NULL;
  cJSON *cjson_location = NULL;

  root_data = cJSON_Parse(iweda_handle->output_buf);

  if (root_data) {
    cjson_pois = cJSON_GetObjectItem(root_data, "pois");
    if (cjson_pois) {
      cjson_pois_item = cJSON_GetArrayItem(cjson_pois, 0);
      if (cjson_pois_item) {
        cjson_location = cJSON_GetObjectItem(cjson_pois_item, "location");
        if (cjson_location) {
          if (cjson_location->valuestring) {
            if (position_data.longitude != NULL) {
              free(position_data.longitude);
              position_data.longitude = NULL;
            }
            if (position_data.latitude != NULL) {
              free(position_data.latitude);
              position_data.latitude = NULL;
            }
            char lon[32] = {0}; // 经度
            char lat[32] = {0}; // 纬度
            sscanf(cjson_location->valuestring, "%[^,],%s", lon, lat);
            position_data.longitude = strdup(lon);
            position_data.latitude = strdup(lat);
            ESP_LOGI(TAG, "获取到经度 %s,纬度 %s", position_data.longitude,
                     position_data.latitude);
          }
        }
      }
    }
    cJSON_Delete(root_data);
  }
}

/// @brief
/// 使用和风天气GeoAPI-城市搜索，通过经纬度获取和风天气返回的locationID,保存到position_data,同时会进一步完善position_data数据
/// @param iweda_handle IWEDA句柄
/// @note API文档：https://dev.qweather.com/docs/api/geoapi/city-lookup/
void transform_locationID_qweather(IWEDA_handle_t iweda_handle) {
  const char *TAG = "transform_locationID_qweather";

  if (!iweda_handle) {
    ESP_LOGE(TAG, "iweda_handle为NULL");
    return;
  }

  char json_buf[PRE_CJSON_BUF_MAX] = {0}; // 缓存JOSN数据
  gzip_decompress(iweda_handle->output_buf, iweda_handle->output_buf_size,
                  json_buf);

  cJSON *root_data = NULL;
  cJSON *cjson_location = NULL;
  cJSON *cjson_location_root = NULL;
  cJSON *cjson_id = NULL;

  root_data = cJSON_Parse(json_buf);
  if (root_data) {
    cjson_location = cJSON_GetObjectItem(root_data, "location");
    if (cjson_location) {
      cjson_location_root = cJSON_GetArrayItem(cjson_location, 0);
      if (cjson_location_root) {
        cjson_id = cJSON_GetObjectItem(cjson_location_root, "id");
        if (cjson_id) {
          if (cjson_id->valuestring) {
            if (position_data.qweather_location_id != NULL) {
              free(position_data.qweather_location_id);
              position_data.qweather_location_id = NULL;
            }
            position_data.qweather_location_id = strdup(cjson_id->valuestring);
            ESP_LOGI(TAG, "获取到详细地址 locationID %s",
                     position_data.qweather_location_id);
          }
        }
      }
    }
    cJSON_Delete(root_data);
  }
}

/// @brief 使用和风天气天气预报-实时天气，通过经纬度获取实时天气数据
/// @brief 保存到全局current_weather_data
/// @param iweda_handle IWEDA句柄
/// @note API文档：https://dev.qweather.com/docs/api/weather/weather-current/
void transform_current_weather_data_qweather(IWEDA_handle_t iweda_handle) {
  const char *TAG = "transform_current_weather_data_qweather";

  if (!iweda_handle) {
    ESP_LOGE(TAG, "iweda_handle为NULL");
    return;
  }

  char json_buf[PRE_CJSON_BUF_MAX] = {0}; // 缓存JOSN数据
  gzip_decompress(iweda_handle->output_buf, iweda_handle->output_buf_size,
                  json_buf);

  cJSON *root_data = NULL;

  cJSON *cjson_metadata = NULL;
  cJSON *cjson_metadata_tag = NULL;
  cJSON *cjson_metadata_attributions = NULL;

  cJSON *cjson_condition = NULL;
  cJSON *cjson_condition_text = NULL;
  cJSON *cjson_condition_code = NULL;

  cJSON *cjson_temperature = NULL;
  cJSON *cjson_temperature_value = NULL;

  cJSON *cjson_feelsLike = NULL;
  cJSON *cjson_feelsLike_value = NULL;

  cJSON *cjson_humidity = NULL;

  cJSON *cjson_wind = NULL;
  cJSON *cjson_wind_direction = NULL;
  cJSON *cjson_wind_direction_degree = NULL;
  cJSON *cjson_wind_direction_compass = NULL;
  cJSON *cjson_wind_speed = NULL;
  cJSON *cjson_wind_speed_value = NULL;
  cJSON *cjson_wind_scale = NULL;

  cJSON *cjson_windGust = NULL;
  cJSON *cjson_windGust_value = NULL;

  cJSON *cjson_precipitation = NULL;
  cJSON *cjson_precipitation_amount = NULL;
  cJSON *cjson_precipitation_amount_value = NULL;
  cJSON *cjson_precipitation_intensity = NULL;
  cJSON *cjson_precipitation_intensity_value = NULL;
  cJSON *cjson_precipitation_type = NULL;

  cJSON *cjson_pressure = NULL;
  cJSON *cjson_pressure_value = NULL;

  cJSON *cjson_visibility = NULL;
  cJSON *cjson_visibility_value = NULL;

  cJSON *cjson_dewPoint = NULL;
  cJSON *cjson_dewPoint_value = NULL;

  cJSON *cjson_cloudCover = NULL;
  cJSON *cjson_uvIndex = NULL;

  // 测试数据(包含所有字段，来自官方API文档
  // https://dev.qweather.com/docs/api/weather/weather-current/)
  //  {
  //    "metadata": {
  //      "tag":
  //      "03ec2ded05fa80a43df2664dd9e4a8f48f7cc4f97c6a81dfd736ae17098aba14",
  //      "attributions": [
  //        "https://developer.qweather.com/attribution.html"
  //      ]
  //    },
  //    "condition": {
  //      "text": "少云",
  //      "code": "102"
  //    },
  //    "temperature": {
  //      "value": 31.71,
  //      "unit": "°C"
  //    },
  //    "feelsLike": {
  //      "value": 33.64,
  //      "unit": "°C"
  //    },
  //    "humidity": 0.69,
  //    "wind": {
  //      "direction": {
  //        "degree": 226,
  //        "compass": "sw"
  //      },
  //      "speed": {
  //        "value": 4.74,
  //        "unit": "m/s"
  //      },
  //      "scale": 3
  //    },
  //    "windGust": {
  //      "value": 7.07,
  //      "unit": "m/s"
  //    },
  //    "precipitation": {
  //      "amount": {
  //        "value": 0,
  //        "unit": "mm"
  //      },
  //      "intensity": {
  //        "value": 0,
  //        "unit": "mm/h"
  //      },
  //      "type": "none"
  //    },
  //    "pressure": {
  //      "value": 1001.5,
  //      "unit": "hPa"
  //    },
  //    "visibility": {
  //      "value": 29020,
  //      "unit": "m"
  //    },
  //    "dewPoint": {
  //      "value": 25.36,
  //      "unit": "°C"
  //    },
  //    "cloudCover": 0.05,
  //    "uvIndex": 3
  //  }
  const char *test =
      "{"
      "\"metadata\": {"
      "\"tag\": "
      "\"03ec2ded05fa80a43df2664dd9e4a8f48f7cc4f97c6a81dfd736ae17098aba14\","
      "\"attributions\": ["
      "\"https://developer.qweather.com/attribution.html\""
      "]"
      "},"
      "\"condition\": {"
      "\"text\": \"少云\","
      "\"code\": \"102\""
      "},"
      "\"temperature\": {"
      "\"value\": 31.71,"
      "\"unit\": \"°C\""
      "},"
      "\"feelsLike\": {"
      "\"value\": 33.64,"
      "\"unit\": \"°C\""
      "},"
      "\"humidity\": 0.69,"
      "\"wind\": {"
      "\"direction\": {"
      "\"degree\": 226,"
      "\"compass\": \"sw\""
      "},"
      "\"speed\": {"
      "\"value\": 4.74,"
      "\"unit\": \"m/s\""
      "},"
      "\"scale\": 3"
      "},"
      "\"windGust\": {"
      "\"value\": 7.07,"
      "\"unit\": \"m/s\""
      "},"
      "\"precipitation\": {"
      "\"amount\": {"
      "\"value\": 0,"
      "\"unit\": \"mm\""
      "},"
      "\"intensity\": {"
      "\"value\": 0,"
      "\"unit\": \"mm/h\""
      "},"
      "\"type\": \"none\""
      "},"
      "\"pressure\": {"
      "\"value\": 1001.5,"
      "\"unit\": \"hPa\""
      "},"
      "\"visibility\": {"
      "\"value\": 29020,"
      "\"unit\": \"m\""
      "},"
      "\"dewPoint\": {"
      "\"value\": 25.36,"
      "\"unit\": \"°C\""
      "},"
      "\"cloudCover\": 0.05,"
      "\"uvIndex\": 3"
      "}";

  // root_data = cJSON_Parse(test);
  root_data = cJSON_Parse(json_buf);

  if (root_data) {
    cjson_metadata = cJSON_GetObjectItem(root_data, "metadata");
    if (cjson_metadata) {
      cjson_metadata_tag = cJSON_GetObjectItem(cjson_metadata, "tag");
      if (cjson_metadata_tag && cjson_metadata_tag->valuestring) {
        {
          if (current_weather_data.metadata_tag != NULL) {
            free(current_weather_data.metadata_tag);
            current_weather_data.metadata_tag = NULL;
          }
          current_weather_data.metadata_tag =
              strdup(cjson_metadata_tag->valuestring);
          ESP_LOGI(TAG, "获取到 metadata_tag %s",
                   current_weather_data.metadata_tag);
        }
      }
      cjson_metadata_attributions =
          cJSON_GetObjectItem(cjson_metadata, "attributions");
      if (cjson_metadata_attributions) {
        if (current_weather_data.metadata_attributions_raw_json != NULL) {
          free(current_weather_data.metadata_attributions_raw_json);
          current_weather_data.metadata_attributions_raw_json = NULL;
        }
        current_weather_data.metadata_attributions_raw_json =
            strdup(cJSON_PrintUnformatted(cjson_metadata_attributions));
        ESP_LOGI(TAG, "获取到 metadata_attributions_raw_json %s",
                 current_weather_data.metadata_attributions_raw_json);
      }
    }
    cjson_condition = cJSON_GetObjectItem(root_data, "condition");
    if (cjson_condition) {
      cjson_condition_text = cJSON_GetObjectItem(cjson_condition, "text");
      if (cjson_condition_text && cjson_condition_text->valuestring) {
        if (current_weather_data.condition_text != NULL) {
          free(current_weather_data.condition_text);
          current_weather_data.condition_text = NULL;
        }
        current_weather_data.condition_text =
            strdup(cjson_condition_text->valuestring);
        ESP_LOGI(TAG, "获取到 condition_text %s",
                 current_weather_data.condition_text);
      }
      cjson_condition_code = cJSON_GetObjectItem(cjson_condition, "code");
      if (cjson_condition_code && cjson_condition_code->valuestring) {
        sscanf(cjson_condition_code->valuestring, "%d",
               &current_weather_data.condition_code);
        ESP_LOGI(TAG, "获取到 condition_code %d",
                 current_weather_data.condition_code);
      }
    }
    cjson_temperature = cJSON_GetObjectItem(root_data, "temperature");
    if (cjson_temperature) {
      cjson_temperature_value = cJSON_GetObjectItem(cjson_temperature, "value");
      if (cjson_temperature_value &&
          cjson_temperature_value->type == cJSON_Number) {
        current_weather_data.temperature = cjson_temperature_value->valuedouble;
        ESP_LOGI(TAG, "获取到 temperature %lf",
                 current_weather_data.temperature);
      }
    }
    cjson_feelsLike = cJSON_GetObjectItem(root_data, "feelsLike");
    if (cjson_feelsLike) {
      cjson_feelsLike_value = cJSON_GetObjectItem(cjson_feelsLike, "value");
      if (cjson_feelsLike_value &&
          cjson_feelsLike_value->type == cJSON_Number) {
        current_weather_data.feelsLike = cjson_feelsLike_value->valuedouble;
        ESP_LOGI(TAG, "获取到 feelsLike %lf", current_weather_data.feelsLike);
      }
    }
    cjson_humidity = cJSON_GetObjectItem(root_data, "humidity");
    if (cjson_humidity && cjson_humidity->type == cJSON_Number) {
      current_weather_data.humidity = cjson_humidity->valuedouble;
      ESP_LOGI(TAG, "获取到 humidity %lf", current_weather_data.humidity);
    }
    cjson_wind = cJSON_GetObjectItem(root_data, "wind");
    if (cjson_wind) {
      cjson_wind_direction = cJSON_GetObjectItem(cjson_wind, "direction");
      if (cjson_wind_direction) {
        cjson_wind_direction_degree =
            cJSON_GetObjectItem(cjson_wind_direction, "degree");
        if (cjson_wind_direction_degree &&
            cjson_wind_direction_degree->type == cJSON_Number) {
          current_weather_data.wind_direction_degree =
              cjson_wind_direction_degree->valuedouble;
          ESP_LOGI(TAG, "获取到 wind_direction_degree %lf",
                   current_weather_data.wind_direction_degree);
        }
        cjson_wind_direction_compass =
            cJSON_GetObjectItem(cjson_wind_direction, "compass");
        if (cjson_wind_direction_compass &&
            cjson_wind_direction_compass->valuestring) {
          if (current_weather_data.wind_direction_compass != NULL) {
            free(current_weather_data.wind_direction_compass);
            current_weather_data.wind_direction_compass = NULL;
          }
          current_weather_data.wind_direction_compass =
              strdup(cjson_wind_direction_compass->valuestring);
          ESP_LOGI(TAG, "获取到 wind_direction_compass %s",
                   current_weather_data.wind_direction_compass);
        }
      }
      cjson_wind_speed = cJSON_GetObjectItem(cjson_wind, "speed");
      if (cjson_wind_speed) {
        cjson_wind_speed_value = cJSON_GetObjectItem(cjson_wind_speed, "value");
        if (cjson_wind_speed_value &&
            cjson_wind_speed_value->type == cJSON_Number) {
          current_weather_data.wind_speed = cjson_wind_speed_value->valuedouble;
          ESP_LOGI(TAG, "获取到 wind_speed %lf",
                   current_weather_data.wind_speed);
        }
      }
      cjson_wind_scale = cJSON_GetObjectItem(cjson_wind, "scale");
      if (cjson_wind_scale && cjson_wind_scale->type == cJSON_Number) {
        current_weather_data.wind_scale = cjson_wind_scale->valuedouble;
        ESP_LOGI(TAG, "获取到 wind_scale %lf", current_weather_data.wind_scale);
      }
    }
    cjson_windGust = cJSON_GetObjectItem(root_data, "windGust");
    if (cjson_windGust) {
      cjson_windGust_value = cJSON_GetObjectItem(cjson_windGust, "value");
      if (cjson_windGust_value && cjson_windGust_value->type == cJSON_Number) {
        current_weather_data.windGust = cjson_windGust_value->valuedouble;
        ESP_LOGI(TAG, "获取到 windGust %lf", current_weather_data.windGust);
      }
    }
    cjson_precipitation = cJSON_GetObjectItem(root_data, "precipitation");
    if (cjson_precipitation) {
      cjson_precipitation_amount =
          cJSON_GetObjectItem(cjson_precipitation, "amount");
      if (cjson_precipitation_amount) {
        cjson_precipitation_amount_value =
            cJSON_GetObjectItem(cjson_precipitation_amount, "value");
        if (cjson_precipitation_amount_value &&
            cjson_precipitation_amount_value->type == cJSON_Number) {
          current_weather_data.precipitation_amount =
              cjson_precipitation_amount_value->valuedouble;
          ESP_LOGI(TAG, "获取到 precipitation_amount %lf",
                   current_weather_data.precipitation_amount);
        }
      }
      cjson_precipitation_intensity =
          cJSON_GetObjectItem(cjson_precipitation, "intensity");
      if (cjson_precipitation_intensity) {
        cjson_precipitation_intensity_value =
            cJSON_GetObjectItem(cjson_precipitation_intensity, "value");
        if (cjson_precipitation_intensity_value &&
            cjson_precipitation_intensity_value->type == cJSON_Number) {
          current_weather_data.precipitation_intensity =
              cjson_precipitation_intensity_value->valuedouble;
          ESP_LOGI(TAG, "获取到 precipitation_intensity %lf",
                   current_weather_data.precipitation_intensity);
        }
      }
      cjson_precipitation_type =
          cJSON_GetObjectItem(cjson_precipitation, "type");
      if (cjson_precipitation_type &&
          cjson_precipitation_type->valuestring != NULL) {
        if (current_weather_data.precipitation_type != NULL) {
          free(current_weather_data.precipitation_type);
          current_weather_data.precipitation_type = NULL;
        }
        current_weather_data.precipitation_type =
            strdup(cjson_precipitation_type->valuestring);
        ESP_LOGI(TAG, "获取到 precipitation_type %s",
                 current_weather_data.precipitation_type);
      }
    }
    cjson_pressure = cJSON_GetObjectItem(root_data, "pressure");
    if (cjson_pressure) {
      cjson_pressure_value = cJSON_GetObjectItem(cjson_pressure, "value");
      if (cjson_pressure_value && cjson_pressure_value->type == cJSON_Number) {
        current_weather_data.pressure = cjson_pressure_value->valuedouble;
        ESP_LOGI(TAG, "获取到 pressure %lf", current_weather_data.pressure);
      }
    }
    cjson_visibility = cJSON_GetObjectItem(root_data, "visibility");
    if (cjson_visibility) {
      cjson_visibility_value = cJSON_GetObjectItem(cjson_visibility, "value");
      if (cjson_visibility_value &&
          cjson_visibility_value->type == cJSON_Number) {
        current_weather_data.visibility = cjson_visibility_value->valuedouble;
        ESP_LOGI(TAG, "获取到 visibility %lf", current_weather_data.visibility);
      }
    }
    cjson_dewPoint = cJSON_GetObjectItem(root_data, "dewPoint");
    if (cjson_dewPoint) {
      cjson_dewPoint_value = cJSON_GetObjectItem(cjson_dewPoint, "value");
      if (cjson_dewPoint_value && cjson_dewPoint_value->type == cJSON_Number) {
        current_weather_data.dewPoint = cjson_dewPoint_value->valuedouble;
        ESP_LOGI(TAG, "获取到 dewPoint %lf", current_weather_data.dewPoint);
      }
    }
    cjson_cloudCover = cJSON_GetObjectItem(root_data, "cloudCover");
    if (cjson_cloudCover && cjson_cloudCover->type == cJSON_Number) {
      current_weather_data.cloudCover = cjson_cloudCover->valuedouble;
      ESP_LOGI(TAG, "获取到 cloudCover %lf", current_weather_data.cloudCover);
    }
    cjson_uvIndex = cJSON_GetObjectItem(root_data, "uvIndex");
    if (cjson_uvIndex && cjson_uvIndex->type == cJSON_Number) {
      current_weather_data.uvIndex = cjson_uvIndex->valuedouble;
      ESP_LOGI(TAG, "获取到 uvIndex %lf", current_weather_data.uvIndex);
    }
    cJSON_Delete(root_data);
  }
}

/// @brief
/// 解析GPT返回的数据(单个json格式数据/[流式传输]JSON_Line数据中的一个数据单元)，
/// @brief 缓存识别结果追加到result(自动申请内存)
/// @param line_response
/// 单个json格式数据/[流式传输]JSON_Line数据中的一个数据单元
/// @param result 收集结果的字符串地址
void GPT_chat_transform_collect(char *line_response, char **result) {
  static const char *TAG = "GPT_chat_transform_collect";
  if (!line_response) {
    ESP_LOGE(TAG, "传入了为空的输入数据");
    return;
  }

  // 如果缓存为空,申请缓存
  if (!*result) {
    *result = (char *)malloc(GPT_CHAT_RESPONSE_BUF_SIZE * sizeof(char));
    while (!*result) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      ESP_LOGE(TAG, "申请result资源发现问题 正在重试");
      *result = (char *)malloc(GPT_CHAT_RESPONSE_BUF_SIZE * sizeof(char));
    }
    memset(*result, 0, GPT_CHAT_RESPONSE_BUF_SIZE * sizeof(char));
  }

  cJSON *root_data = NULL;
  cJSON *cjson_choices = NULL;
  cJSON *cjson_choices_item = NULL;
  cJSON *cjson_delta = NULL;
  cJSON *cjson_content = NULL;

  root_data = cJSON_Parse(line_response);

  if (root_data) {
    cjson_choices = cJSON_GetObjectItem(root_data, "choices");
    if (cjson_choices) {
      cjson_choices_item = cJSON_GetArrayItem(cjson_choices, 0);
      if (cjson_choices_item) {
        cjson_delta = cJSON_GetObjectItem(cjson_choices_item, "delta");
        if (cjson_delta) {
          cjson_content = cJSON_GetObjectItem(cjson_delta, "content");
          if (cjson_content && cjson_content->valuestring) {
            strncat(*result, cjson_content->valuestring,
                    GPT_CHAT_RESPONSE_BUF_SIZE - strlen(*result) - 1);
          }
        }
      }
    }
    cJSON_Delete(root_data);
  }
}

/// @brief [流式传输]根据GPT返回的数据(HTTP原始响应数据)
/// @brief 获取聊天传输状态是否结束
/// @param http_response HTTP原始响应数据
/// @return true 结束 / false 进行中
bool GPT_stream_chat_over_status(char *http_response) {
  static const char *TAG = "GPT_stream_chat_over_status";
  if (!http_response) {
    ESP_LOGE(TAG, "传入了为空的输入数据");
    return false;
  }

  char buf[10] = {0};
  if (strlen(http_response) - strlen("[DONE]\n\n") >= 0)
    strncpy(buf, http_response + strlen(http_response) - strlen("[DONE]\n\n"),
            sizeof(buf) - 1);

  if (strcmp(buf, "[DONE]\n\n") == 0) {
    return true;
  } else {
    return false;
  }
}

/// @brief GPT聊天传输任务,使用POST请求
/// @param chat_handle 传入聊天句柄
void GPT_chat_http_Task(GPT_chat_handle_t chat_handle) {
  static const char *TAG = "GPT_chat_http_Task";

  while (1) {
    esp_err_t err_flag = ESP_OK;
    int64_t start_time = esp_timer_get_time();

    /// 初始化http_client
    esp_http_client_config_t http_config;
    memset(&http_config, 0, sizeof(http_config));
    http_config.url = chat_handle->url;    // 导入url
    http_config.method = HTTP_METHOD_POST; // 使用POST请求
    chat_handle->client_handle = esp_http_client_init(&http_config);

    // 设置HTTP-HEADER
    esp_http_client_set_header(chat_handle->client_handle, "Content-Type",
                               "application/json");
    esp_http_client_set_header(chat_handle->client_handle, "Authorization",
                               chat_handle->auth_header_buf);

    // 设置超时时间
    esp_http_client_set_timeout_ms(chat_handle->client_handle,
                                   chat_handle->timeout_ms);

    // 对服务器发送连接请求
    err_flag = esp_http_client_open(chat_handle->client_handle,
                                    strlen(chat_handle->request_body_buf));

    if (err_flag != ESP_OK) {
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
    esp_http_client_write(chat_handle->client_handle,
                          chat_handle->request_body_buf,
                          strlen(chat_handle->request_body_buf));

    // 校验响应
    esp_http_client_fetch_headers(chat_handle->client_handle); //   接收消息头
    int status = esp_http_client_get_status_code(
        chat_handle->client_handle); // 获取消息头中的响应状态信息
    if (status != 200) {
      chat_handle->err = ESP_FAIL;
      ESP_LOGE(TAG, "校验响应时发现问题 状态码 -> %d", status);
      esp_http_client_read_response(chat_handle->client_handle,
                                    chat_handle->response_buf,
                                    GPT_CHAT_RESPONSE_BUF_SIZE * sizeof(char));
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
    while (1) {

      vTaskDelay(pdMS_TO_TICKS(100));

      // 获取合法单元个数
      right_num = json_line_unit_num_get(chat_handle->response_buf,
                                         GPT_CHAT_RESPONSE_BUF_SIZE);

      // 此前未读取任何响应数据,循环开始时right_num必然为0,第一次读取响应发生在下面的等待下

      // 如果有未读单元,执行读取
      if (complete_num < right_num) {
        memset(chat_handle->json_buf, 0,
               GPT_CHAT_RESPONSE_BUF_SIZE * sizeof(char));
        json_line_unit_copy(chat_handle->json_buf, chat_handle->response_buf,
                            complete_num,
                            GPT_CHAT_RESPONSE_BUF_SIZE); // 复制一个数据单元
        GPT_chat_transform_collect(chat_handle->json_buf,
                                   &chat_handle->result); // 解析并拼接保存
        complete_num++;
      }

      //(right_num=0 complete_num=0)或者(读完最后一个单元)[已读个数 =
      // 总个数],检验,如果数据没有完全读取完成,需要等待并读取更多下文
      if (complete_num == right_num) {
        if (GPT_stream_chat_over_status(chat_handle->response_buf) == false) {
          while (1) {
            if (GPT_CHAT_RESPONSE_BUF_SIZE - read_index <= 0) {
              chat_handle->err = ESP_ERR_NO_MEM;
              ESP_LOGE(TAG, "响应缓存内存空间不足 已读取内容 -> %s",
                       chat_handle->response_buf);

              esp_http_client_close(chat_handle->client_handle);
              esp_http_client_cleanup(chat_handle->client_handle);
              chat_handle->task_handle = NULL;
              chat_handle->is_completed = true;
              chat_handle->client_handle = NULL;

              vTaskDelete(NULL);
            }

            read_index += esp_http_client_read_response(
                chat_handle->client_handle,
                chat_handle->response_buf + read_index,
                GPT_CHAT_RESPONSE_BUF_SIZE - read_index);

            // 如果有新的发现,退出等待
            if (json_line_unit_num_get(chat_handle->response_buf,
                                       GPT_CHAT_RESPONSE_BUF_SIZE) > right_num)
              break;

            vTaskDelay(pdMS_TO_TICKS(1000));
            if ((esp_timer_get_time() - start_time) / 1000 >=
                chat_handle->timeout_ms) {
              chat_handle->err = ESP_ERR_TIMEOUT;
              ESP_LOGE(TAG, "等待回复超时 已读取内容 -> %s",
                       chat_handle->response_buf);

              esp_http_client_close(chat_handle->client_handle);
              esp_http_client_cleanup(chat_handle->client_handle);
              chat_handle->task_handle = NULL;
              chat_handle->is_completed = true;
              chat_handle->client_handle = NULL;

              vTaskDelete(NULL);
            }
          }
        } else {
          if (chat_handle->result) {
            ESP_LOGI(TAG, "%s", chat_handle->result);
            ESP_LOGI(TAG, "解析完成,共拼接%d个数据单元", complete_num);
            chat_handle->err = ESP_OK;

            esp_http_client_close(chat_handle->client_handle);
            esp_http_client_cleanup(chat_handle->client_handle);
            chat_handle->task_handle = NULL;
            chat_handle->is_completed = true;
            chat_handle->client_handle = NULL;

            vTaskDelete(NULL);
          } else {
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
esp_err_t GPT_chat_text_exchange(GPT_chat_handle_t chat_handle, int task_prio) {
  const char *TAG = "GPT_chat_text_exchange";

  if (chat_handle == NULL) {
    ESP_LOGE(TAG, "传入了为空的输入数据");
    return ESP_ERR_INVALID_ARG;
  }

  if (!strcasecmp(chat_handle->user_content, "")) {
    ESP_LOGE(TAG, "空的用户内容");
    return ESP_ERR_INVALID_ARG;
  }

  if (periph_wifi_is_connected(wifi_periph_handle) != PERIPH_WIFI_CONNECTED) {
    ESP_LOGE(TAG, "网络未连接");
    return ESP_ERR_INVALID_STATE;
  }

  if (chat_handle->result) {
    free(chat_handle->result);
    chat_handle->result = NULL;
  }
  memset(chat_handle->response_buf, 0,
         GPT_CHAT_RESPONSE_BUF_SIZE * sizeof(char));
  memset(chat_handle->json_buf, 0,
         GPT_CHAT_HTTP_REQUEST_BODY_BUF_SIZE * sizeof(char));

  chat_handle->is_completed = false;

  xTaskCreatePinnedToCore((TaskFunction_t)GPT_chat_http_Task,
                          "GPT_chat_http_Task", GPT_CHAT_TASK_STACK_SIZE,
                          chat_handle, task_prio, &chat_handle->task_handle,
                          GPT_CHAT_TASK_CORE);

  while (!chat_handle->is_completed) {
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  if (chat_handle->err != ESP_OK) {
    ESP_LOGE(TAG, "GPT文本交互任务运行异常 错误:%s",
             esp_err_to_name(chat_handle->err));
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
esp_err_t GPT_chat_update_user_content(GPT_chat_handle_t chat_handle,
                                       char *user_content) {
  const char *TAG = "GPT_chat_update_user_content";

  if (chat_handle == NULL) {
    ESP_LOGE(TAG, "传入了为空的输入数据");
    return ESP_ERR_INVALID_ARG;
  }

  if (!chat_handle->request_body_buf) {
    ESP_LOGE(TAG, "初始化未完成,request_body_buf为空");
    return ESP_ERR_INVALID_STATE;
  }

  if (!user_content || !strcasecmp(user_content, "")) {
    ESP_LOGW(TAG, "缺少必填参数");
  }

  if (chat_handle->user_content) {
    free(chat_handle->user_content);
    chat_handle->user_content = NULL;
  }

  chat_handle->user_content = strdup(user_content);

  memset(chat_handle->request_body_buf, 0,
         GPT_CHAT_HTTP_REQUEST_BODY_BUF_SIZE * sizeof(char));
  snprintf(chat_handle->request_body_buf, GPT_CHAT_HTTP_REQUEST_BODY_BUF_SIZE,
           "{\"model\":\"%s\",\"web_search\":{\"enable\":true},\"messages\":[{"
           "\"role\":\"user\",\"content\":\"%s\"}],\"stream\":true}",
           chat_handle->model, chat_handle->user_content);

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
GPT_chat_handle_t GPT_chat_start(char *url, char *access_key, char *model,
                                 char *user_content, int timeout_ms) {
  const char *TAG = "GPT_chat_start";

  if (url == NULL || access_key == NULL || model == NULL ||
      user_content == NULL) {
    ESP_LOGE(TAG, "传入了为NULL的输入数据");
    return NULL;
  }

  if (!strcasecmp(url, "") || !strcasecmp(access_key, "") ||
      !strcasecmp(model, "")) {
    ESP_LOGE(TAG, "缺少必填参数");
    return NULL;
  }

  GPT_chat_handle_t chat_handle = (GPT_chat_handle_t)malloc(sizeof(GPT_chat_t));
  if (!chat_handle) {
    ESP_LOGE(TAG, "申请GPT_chat_handle资源发现问题");
    return NULL;
  }
  memset(chat_handle, 0, sizeof(GPT_chat_t));

  chat_handle->url = strdup(url);
  chat_handle->access_key = strdup(access_key);
  chat_handle->model = strdup(model);

  if (!chat_handle->url || !chat_handle->access_key || !chat_handle->model) {
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
  chat_handle->response_buf =
      (char *)malloc(GPT_CHAT_RESPONSE_BUF_SIZE * sizeof(char));
  while (!chat_handle->response_buf) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGE(TAG, "申请response_buf资源发现问题 正在重试");
    chat_handle->response_buf =
        (char *)malloc(GPT_CHAT_RESPONSE_BUF_SIZE * sizeof(char));
  }
  memset(chat_handle->response_buf, 0,
         GPT_CHAT_RESPONSE_BUF_SIZE * sizeof(char));
  strcpy(chat_handle->response_buf, "");

  // 申请json_line格式解析缓存
  chat_handle->json_buf =
      (char *)malloc(GPT_CHAT_RESPONSE_BUF_SIZE * sizeof(char));
  while (!chat_handle->json_buf) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGE(TAG, "申请json_buf资源发现问题 正在重试");
    chat_handle->json_buf =
        (char *)malloc(GPT_CHAT_RESPONSE_BUF_SIZE * sizeof(char));
  }
  memset(chat_handle->json_buf, 0, GPT_CHAT_RESPONSE_BUF_SIZE * sizeof(char));

  // 准备Authorization头
  const char *prefix = "Bearer ";
  size_t auth_len = strlen(prefix) + strlen(chat_handle->access_key) + 1;
  chat_handle->auth_header_buf = (char *)malloc(auth_len * sizeof(char));
  while (!chat_handle->auth_header_buf) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGE(TAG, "申请auth_header_buf资源发现问题 正在重试");
    chat_handle->auth_header_buf = (char *)malloc(auth_len * sizeof(char));
  }
  memset(chat_handle->auth_header_buf, 0, auth_len * sizeof(char));
  snprintf(chat_handle->auth_header_buf, auth_len, "%s%s", prefix,
           chat_handle->access_key);

  // 准备HTTP-BODY
  chat_handle->request_body_buf =
      (char *)malloc(GPT_CHAT_HTTP_REQUEST_BODY_BUF_SIZE * sizeof(char));
  while (!chat_handle->request_body_buf) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGE(TAG, "申请request_body_buf资源发现问题 正在重试");
    chat_handle->request_body_buf =
        (char *)malloc(GPT_CHAT_HTTP_REQUEST_BODY_BUF_SIZE * sizeof(char));
  }
  GPT_chat_update_user_content(chat_handle, user_content);

  ESP_LOGW(TAG, "GPT文本交互开始准备已完成");

  return chat_handle;
}

/// @brief 停止GPT文本交互
/// @param chat_handle GPT文本交互句柄
/// @note
/// 如果你不确定用这个句柄调用的GPT_chat_text_exchange是否已经返回，就不要调用这个函数，这非常危险
/// @return ESP_OK 成功
/// @return ESP_ERR_INVALID_ARG 传入了无效的参数柄
esp_err_t GPT_chat_stop(GPT_chat_handle_t chat_handle) {
  const char *TAG = "GPT_chat_stop";
  if (chat_handle == NULL) {
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

/// @brief 解析百度语音识别响应的数据，缓存识别结果到 sevetest30_asr_result_text
/// @param asr_response 语音识别响应的数据
void asr_data_save_result(char *asr_response) {
  static const char *TAG = "asr_data_get_result";
  if (asr_response == NULL) {
    ESP_LOGE(TAG, "传入了为空的输入数据");
    return;
  }
  cJSON *root_data = NULL;
  cJSON *cjson_err_msg = NULL;
  root_data = cJSON_Parse(asr_response);

  cjson_err_msg = cJSON_GetObjectItem(root_data, "err_msg");
  if (cjson_err_msg == NULL || cjson_err_msg->valuestring == NULL) {
    ESP_LOGE(TAG, "交互出现问题,无法解析,响应内容-> %s", asr_response);
    return;
  }
  if (strcasecmp(cjson_err_msg->valuestring, "success.")) {
    ESP_LOGE(TAG, "不是成功的响应信息 [%s]", cjson_err_msg->valuestring);
    return;
  }

  cJSON *cjson_result = cJSON_GetObjectItem(root_data, "result");
  cJSON *cjson_result_root = cJSON_GetArrayItem(cjson_result, 0);
  ESP_LOGI(TAG, "响应数据: %s", asr_response);
  ESP_LOGI(TAG, "识别结果: %s", cjson_result_root->valuestring);

  if (!sevetest30_asr_result_text) {
  ASR_RESULT_TEX_MALLOC:
    sevetest30_asr_result_text =
        (char *)malloc(ASR_RESULT_TEX_BUF_MAX * sizeof(char));
    while (!sevetest30_asr_result_text) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      ESP_LOGE(TAG, "申请sevetest30_asr_result_tex资源发现问题 正在重试");
      sevetest30_asr_result_text =
          (char *)malloc(ASR_RESULT_TEX_BUF_MAX * sizeof(char));
    }
    memset(sevetest30_asr_result_text, 0,
           ASR_RESULT_TEX_BUF_MAX * sizeof(char));
  } else {
    free(sevetest30_asr_result_text);
    sevetest30_asr_result_text = NULL;
    goto ASR_RESULT_TEX_MALLOC;
  }
  strncpy(sevetest30_asr_result_text, cjson_result_root->valuestring,
          ASR_RESULT_TEX_BUF_MAX);

  cJSON_Delete(root_data);
  return;
}

/// @brief 通过特定URL提取json数据中音乐歌词(LRC格式数据)
/// @param url  获取音乐歌词使用的完整URL
/// @param dest 读取保存到的位置
/// @param len_max 最大允许保存的字符长度
/// @return ESP_OK 成功
/// @return ESP_ERR_INVALID_STATE iweda_handle创建失败
/// @return ESP_FAIL 交互出现问题 / 解析失败
esp_err_t get_music_lyric_by_url(char *url, char *dest, int len_max) {
  const char *TAG = "get_music_lyric_by_url";

  IWEDA_handle_t iweda_handle = new_iweda_handle(IWEDA_DEFAULT_OUTPUT_BUF_SIZE,
                                                 IWEDA_DEFAULT_URL_BUF_SIZE);
  if (!iweda_handle) {
    ESP_LOGE(TAG, "iweda_handle创建失败");
    return ESP_ERR_INVALID_STATE;
  }

  snprintf(iweda_handle->url_buf, iweda_handle->url_buf_size, "%s",
           url); // 确定请求URL
  iweda_init_http_get_request(iweda_handle);
  xTaskCreatePinnedToCore((TaskFunction_t)iweda_http_get_request_send, TAG, 8192,
                          iweda_handle, HTTP_TASK_PRIO, NULL,
                          HTTP_TASK_CORE); // 启动http传输任务,GET方式
  while (!iweda_handle->is_completed)
    vTaskDelay(pdMS_TO_TICKS(200));

  // 开始json解析
  cJSON *root_data = NULL;
  cJSON *cjson_lyric = NULL;

  root_data = cJSON_Parse(iweda_handle->output_buf);
  if (root_data == NULL) {
    ESP_LOGE(TAG, "交互出现问题,无法解析,响应内容-> %s",
             iweda_handle->output_buf);
    delete_iweda_handle(iweda_handle);
    return ESP_FAIL;
  }

  cjson_lyric = cJSON_GetObjectItem(root_data, "lyric");
  if (cjson_lyric && cjson_lyric->valuestring) {
    if (strlen(cjson_lyric->valuestring) < len_max) {
      strcpy(dest, cjson_lyric->valuestring);
      cJSON_Delete(root_data);
      delete_iweda_handle(iweda_handle);
      return ESP_OK;
    } else {
      ESP_LOGE(TAG, "歌词数据长度过大");
      cJSON_Delete(root_data);
      delete_iweda_handle(iweda_handle);
      return ESP_FAIL;
    }
  } else {
    ESP_LOGW(TAG, "发现歌词获取对选定的音乐不支持或该音乐无歌词");
    cJSON_Delete(root_data);
    delete_iweda_handle(iweda_handle);
    return ESP_FAIL;
  }
}

///@brief 刷新位置数据
/// @note 保存到全局position_data
void refresh_position_data() {
  const char *TAG = "refresh_position_data";

  if (periph_wifi_is_connected(wifi_periph_handle) != PERIPH_WIFI_CONNECTED) {
    ESP_LOGE(TAG, "网络未连接");
    return;
  }

  IWEDA_handle_t iweda_handle = new_iweda_handle(IWEDA_DEFAULT_OUTPUT_BUF_SIZE,
                                                 IWEDA_DEFAULT_URL_BUF_SIZE);
  if (!iweda_handle) {
    ESP_LOGE(TAG, "iweda_handle创建失败");
    return;
  }

  // 获取公网IP
  snprintf(iweda_handle->url_buf, iweda_handle->url_buf_size,
           GET_IP_ADDRESS_API_URL);
  iweda_init_http_get_request(iweda_handle);
  xTaskCreatePinnedToCore((TaskFunction_t)iweda_http_get_request_send, TAG, 8192,
                          iweda_handle, HTTP_TASK_PRIO, NULL, HTTP_TASK_CORE);
  while (!iweda_handle->is_completed)
    vTaskDelay(pdMS_TO_TICKS(200));
  transform_ip_address(iweda_handle);

  // 获取IP归属地
  if (!ip_address) {
    ESP_LOGE(TAG, "公网IP为空");
    delete_iweda_handle(iweda_handle);
    return;
  }
  snprintf(iweda_handle->url_buf, iweda_handle->url_buf_size,
           IP138_IP_POSITION_API_URL, ip_address);
  iweda_init_http_get_request(iweda_handle);
  esp_http_client_set_header(iweda_handle->http_client_handle, "token",
                             CONFIG_IP138_IP_LOOKUP_TOKEN);
  xTaskCreatePinnedToCore((TaskFunction_t)iweda_http_get_request_send, TAG, 8192,
                          iweda_handle, HTTP_TASK_PRIO, NULL,
                          HTTP_TASK_CORE); // 启动http传输任务,GET方式
  while (!iweda_handle->is_completed)
    vTaskDelay(pdMS_TO_TICKS(200));
  transform_ip_position_ip138(iweda_handle);

  // 通过IP归属地信息获取经纬度
  char keywords[512] = {0};
  char keywords_encoded[512 * 3] = {0};
  strcpy(keywords, "");
  if (position_data.country)
    strcat(keywords, position_data.country);
  if (position_data.adm1)
    strcat(keywords, position_data.adm1);
  if (position_data.adm2)
    strcat(keywords, position_data.adm2);
  if (position_data.name)
    strcat(keywords, position_data.name);

  url_encode(keywords, keywords_encoded, sizeof(keywords_encoded));
  snprintf(iweda_handle->url_buf, iweda_handle->url_buf_size,
           AMAP_SEARCH_POI_API_URL, keywords_encoded, 1, 1,
           CONFIG_AMAP_API_KEY); // 确定请求URL
  iweda_init_http_get_request(iweda_handle);
  xTaskCreatePinnedToCore((TaskFunction_t)iweda_http_get_request_send, TAG, 8192,
                          iweda_handle, HTTP_TASK_PRIO, NULL,
                          HTTP_TASK_CORE); // 启动http传输任务,GET方式
  while (!iweda_handle->is_completed)
    vTaskDelay(pdMS_TO_TICKS(200));
  transform_lng_lat_amap(iweda_handle);

  // 通过和风天气GeoAPI进行城市搜索，根据经纬度获取location数据
  if (position_data.longitude == NULL || position_data.latitude == NULL) {
    ESP_LOGE(TAG, "经纬度为空");
    delete_iweda_handle(iweda_handle);
    return;
  }
  snprintf(iweda_handle->url_buf, iweda_handle->url_buf_size,
           QWEATHER_GEO_CITY_LOOKUP_API_URL, CONFIG_QWEATHER_API_HOST,
           position_data.longitude, position_data.latitude);
  iweda_init_http_get_request(iweda_handle);
  esp_http_client_set_header(iweda_handle->http_client_handle, "Authorization",
                             get_qweather_jwt_token());
  xTaskCreatePinnedToCore((TaskFunction_t)iweda_http_get_request_send, TAG, 8192,
                          iweda_handle, HTTP_TASK_PRIO, NULL,
                          HTTP_TASK_CORE); // 启动http传输任务,GET方式
  while (!iweda_handle->is_completed)
    vTaskDelay(pdMS_TO_TICKS(200));
  transform_locationID_qweather(iweda_handle);

  delete_iweda_handle(iweda_handle);
  return;
}

///@brief 刷新当前天气数据
/// @note
/// 从和风天气天气预报-实时天气API获取当前天气数据，保存到全局current_weather_data
void refresh_current_weather_data() {
  const char *TAG = "refresh_current_weather_data";

  if (periph_wifi_is_connected(wifi_periph_handle) != PERIPH_WIFI_CONNECTED) {
    ESP_LOGE(TAG, "网络未连接");
    return;
  }

  if (position_data.longitude == NULL || position_data.latitude == NULL) {
    ESP_LOGE(TAG, "经纬度为空");
    return;
  }

  IWEDA_handle_t iweda_handle = new_iweda_handle(IWEDA_DEFAULT_OUTPUT_BUF_SIZE,
                                                 IWEDA_DEFAULT_URL_BUF_SIZE);
  if (!iweda_handle) {
    ESP_LOGE(TAG, "iweda_handle创建失败");
    return;
  }

  double lng = 0; // 经度
  double lat = 0; // 纬度
  sscanf(position_data.longitude, "%lf", &lng);
  sscanf(position_data.latitude, "%lf", &lat);

  snprintf(iweda_handle->url_buf, iweda_handle->url_buf_size,
           QWEATHER_CURRENT_WEATHER_API_URL, CONFIG_QWEATHER_API_HOST, lat,
           lng); // 确定请求URL
  iweda_init_http_get_request(iweda_handle);
  esp_http_client_set_header(iweda_handle->http_client_handle, "Authorization",
                             get_qweather_jwt_token());
  xTaskCreatePinnedToCore((TaskFunction_t)iweda_http_get_request_send, TAG, 8192,
                          iweda_handle, HTTP_TASK_PRIO, NULL,
                          HTTP_TASK_CORE); // 启动http传输任务,GET方式
  while (!iweda_handle->is_completed)
    vTaskDelay(pdMS_TO_TICKS(200));
  transform_current_weather_data_qweather(iweda_handle);

  delete_iweda_handle(iweda_handle);
  return;
}
