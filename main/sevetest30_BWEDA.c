// 该文件由701Enti整合，包含一些sevetest30的  蓝牙环境中  数据获取（BWEDA）以及其他设备的蓝牙交互活动
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：有效的数据存储变量都封装在该库下，不需要在外部函数定义一个数据结构体缓存作为参数，直接读取公共变量，主要为了方便FreeRTOS的任务支持
// 敬告：ESP32S3目前只支持BLE,不支持经典蓝牙的音频传输，这里没有音频通讯的功能
// 敬告：蓝牙配置操作使用了ESP-IDF官方例程并进行修改，非常感谢
// SIG官方提供的包含外观特征值 UUID 等定义的文档链接 https://www.bluetooth.com/specifications/assigned-numbers/
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.11.5

#include "sevetest30_BWEDA.h"
#include "board_def.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

#define PROFILE_NUM 1 // 用户配置数据单元个数
#define PROFILE_APP_IDX 0
#define ESP_APP_ID 0x55

#define SVC_INST_ID 0

#define ADV_CONFIG_FLAG (1 << 0)
#define SCAN_RSP_CONFIG_FLAG (1 << 1)

#define GATTS_TABLE_TAG "GATTS_TABLE"

uint16_t heart_rate_handle_table[HRS_IDX_NB];

// 广播配置状态
static uint8_t adv_config_done = 0;

typedef struct
{
    uint8_t *prepare_buf;
    int prepare_len;
} prepare_type_env_t;

static prepare_type_env_t prepare_write_env;

// 完整服务UUID
static uint8_t service_uuid[16] = BLE_SERVICE_UUID_BASE(BLE_SERVICE_UUID_16BITS);

// 蓝牙广播数据
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,                                                // 以下内容不是为扫描响应配置的
    .include_name = true,                                                 // 在广播数据中添加设备名称
    .include_txpower = true,                                              // 在广播数据中添加发射功率
    .min_interval = 0x0006,                                               // 从设备连接窗口最小间隔数,最小间隔时间为min_interval * 1.25毫秒
    .max_interval = 0x0010,                                               // 从设备连接窗口最大间隔数,最大间隔时间为max_interval * 1.25毫秒
    .appearance = BLE_DEVICE_APPEARANCE_VALUE,                            // 蓝牙设备外观特征值，描述产品基本类型
    .manufacturer_len = 0,                                                // 制造商数据长度
    .p_manufacturer_data = NULL,                                          // 导入制造商数据
    .service_data_len = 0,                                                // 服务数据长度
    .p_service_data = NULL,                                               // 导入服务数据
    .service_uuid_len = sizeof(service_uuid),                             // 服务UUID长度
    .p_service_uuid = service_uuid,                                       // 导入服务UUID
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT), // 广播模式标识
};

// 扫描响应数据,广播启动后，当ESP32接收到其他设备扫描请求，回复扫描响应，这往往是广播数据的补充，使用了与广播数据一样的数据结构处理
static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp = true,    // 以下内容是为扫描响应配置的
    .include_name = true,    // 在扫描响应中添加设备名称
    .include_txpower = true, // 在扫描响应中添加发射功率
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .appearance = BLE_DEVICE_APPEARANCE_VALUE, // 蓝牙设备外观特征值，描述产品基本类型
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(service_uuid),
    .p_service_uuid = service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// 蓝牙广播参数
static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,                                    // 广播最小间隔数
    .adv_int_max = 0x40,                                    // 广播最大间隔数
    .adv_type = ADV_TYPE_IND,                               // 广播报文类型为 可连接非定向 通用广播指示
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,                  // 广播地址类型为公共地址 非随机地址，合法设备的公共地址独一无二
    .channel_map = ADV_CHNL_ALL,                            /// 允许在所有广播信道（37.38.39）下进行广播活动
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY, // 允许任何设备扫描和连接
};

// GATTS 用户配置存储类型定义
struct gatts_profile_inst
{
    esp_gatts_cb_t gatts_cb; // 用于用户配置事件处理的回调函数
    uint16_t gatts_if;       // gatts接口类型
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
                                        esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

static struct gatts_profile_inst heart_rate_profile_tab[PROFILE_NUM] = {
    [PROFILE_APP_IDX] = {
        .gatts_cb = gatts_profile_event_handler, // 安装用户配置事件回调
        .gatts_if = ESP_GATT_IF_NONE,            // ESP_GATTS_REG_EVT触发后，gatts接口类型将被设定
    },
};

/* Service */
static const uint16_t GATTS_SERVICE_UUID_TEST = 0x00FF;
static const uint16_t GATTS_CHAR_UUID_TEST_A = 0xFF01;
static const uint16_t GATTS_CHAR_UUID_TEST_B = 0xFF02;
static const uint16_t GATTS_CHAR_UUID_TEST_C = 0xFF03;

static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t char_prop_read = ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t char_prop_write = ESP_GATT_CHAR_PROP_BIT_WRITE;
static const uint8_t char_prop_read_write_notify = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t heart_measurement_ccc[2] = {0x00, 0x00};
static const uint8_t char_value[4] = {0x11, 0x22, 0x33, 0x44};

// 数据库，可以包含BLE中服务和特性的属性数据以及响应模式，注意，这里的属性不是指代BLE中特征值的读写权限属性，而是所有描述属性
// 数据库使用结构体数组加枚举类型索引，数组每个单元由 响应模式控制类型 和 属性描述结构体 构成，
// 属性描述结构体包含服务或特性的 UUID长度 + 指向UUID的指针(uint8_t *) + 读写权限属性 +  数据值最大大小 +  数据值实际大小 + 指向数据值的指针(uint8_t *)构成
//(UUID 数据值单位 都需要遵循 SIG 的定义)
static const esp_gatts_attr_db_t gatt_db[HRS_IDX_NB] =
    {
        // Service Declaration
        [IDX_SVC] =
            {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ, sizeof(uint16_t), sizeof(GATTS_SERVICE_UUID_TEST), (uint8_t *)&GATTS_SERVICE_UUID_TEST}},

        /* Characteristic Declaration */
        [IDX_CHAR_A] =
            {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_read_write_notify}},

        /* Characteristic Value */
        [IDX_CHAR_VAL_A] =
            {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_TEST_A, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, BLE_GATTS_CHAR_VAL_LEN_MAX, sizeof(char_value), (uint8_t *)char_value}},

        /* Client Characteristic Configuration Descriptor */
        [IDX_CHAR_CFG_A] =
            {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(uint16_t), sizeof(heart_measurement_ccc), (uint8_t *)heart_measurement_ccc}},

        /* Characteristic Declaration */
        [IDX_CHAR_B] =
            {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_read}},

        /* Characteristic Value */
        [IDX_CHAR_VAL_B] =
            {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_TEST_B, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, BLE_GATTS_CHAR_VAL_LEN_MAX, sizeof(char_value), (uint8_t *)char_value}},

        /* Characteristic Declaration */
        [IDX_CHAR_C] =
            {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_write}},

        /* Characteristic Value */
        [IDX_CHAR_VAL_C] =
            {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_TEST_C, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, BLE_GATTS_CHAR_VAL_LEN_MAX, sizeof(char_value), (uint8_t *)char_value}},

};

/// @brief gap相关事件处理回调函数
/// @param event 回调事件
/// @param param 回调参数
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~ADV_CONFIG_FLAG);
        if (adv_config_done == 0)
        {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
        if (adv_config_done == 0)
        {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        /* advertising start complete event to indicate advertising start successfully or failed */
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(GATTS_TABLE_TAG, "advertising start failed");
        }
        else
        {
            ESP_LOGI(GATTS_TABLE_TAG, "advertising start successfully");
        }
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(GATTS_TABLE_TAG, "Advertising stop failed");
        }
        else
        {
            ESP_LOGI(GATTS_TABLE_TAG, "Stop adv successfully\n");
        }
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(GATTS_TABLE_TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                 param->update_conn_params.status,
                 param->update_conn_params.min_int,
                 param->update_conn_params.max_int,
                 param->update_conn_params.conn_int,
                 param->update_conn_params.latency,
                 param->update_conn_params.timeout);
        break;
    default:
        break;
    }
}

/// @brief gatts相关事件处理回调函数
/// @param event 回调触发事件类型
/// @param gatts_if gatts接口类型
/// @param param    回调触发的参数
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{

    // 存储事件下，为所有用户配置单元设定gatts接口类型
    if (event == ESP_GATTS_REG_EVT)
    {
        if (param->reg.status == ESP_GATT_OK)
        {
            heart_rate_profile_tab[PROFILE_APP_IDX].gatts_if = gatts_if;
        }
        else
        {
            ESP_LOGE(GATTS_TABLE_TAG, "reg app failed, app_id %04x, status %d",
                     param->reg.app_id,
                     param->reg.status);
            return;
        }
    }
    do
    {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++)
        {
            /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
            if (gatts_if == ESP_GATT_IF_NONE || gatts_if == heart_rate_profile_tab[idx].gatts_if)
            {
                if (heart_rate_profile_tab[idx].gatts_cb)
                {
                    heart_rate_profile_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}

/// @brief 准备写入 事件，在回调函数中使用，为写操作缓存数据
/// @param gatts_if gatts接口类型
/// @param prepare_write_env 准备写入配置
/// @param param 回调参数
void prepare_write_event(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param)
{
    const char *TAG = "prepare_write_event";

    ESP_LOGI(TAG, "准备写入,数据长度 %d", param->write.len);

    esp_gatt_status_t status = ESP_GATT_OK;

    // 如果缓存区没有被申请，为写入数据申请缓存
    if (!prepare_write_env->prepare_buf)
    {

        prepare_write_env->prepare_len = 0;

        prepare_write_env->prepare_buf = (uint8_t *)malloc(BLE_PREPARE_BUF_SIZE_MAX * sizeof(uint8_t));
        if (!prepare_write_env->prepare_buf)
        {
            ESP_LOGE(TAG, "申请prepare_buf缓存时发现问题");
            status = ESP_GATT_NO_RESOURCES;
        }
    }
    else
    {
        // 如果缓存区已经被申请，判断写入状态
        if (param->write.offset > BLE_PREPARE_BUF_SIZE_MAX)
        {
            status = ESP_GATT_INVALID_OFFSET;
        }
        else if ((param->write.offset + param->write.len) > BLE_PREPARE_BUF_SIZE_MAX)
        {
            status = ESP_GATT_INVALID_ATTR_LEN;
        }
    }

    // 存在响应需求，传输响应
    if (param->write.need_rsp)
    {
        // 申请内存保存gatt参数
        esp_gatt_rsp_t *gatt_rsp = (esp_gatt_rsp_t *)malloc(sizeof(esp_gatt_rsp_t));
        if (gatt_rsp)
        {

            gatt_rsp->attr_value.handle = param->write.handle;      // 写入句柄
            gatt_rsp->attr_value.len = param->write.len;            // 写入长度
            gatt_rsp->attr_value.offset = param->write.offset;      // 写入偏移
            gatt_rsp->attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE; // 身份验证

            // 把参数中的值写入响应数据中
            memcpy(gatt_rsp->attr_value.value, param->write.value, param->write.len);

            // 传输响应
            esp_err_t response_err = esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, gatt_rsp);
            if (response_err != ESP_OK)
            {
                ESP_LOGE(TAG, "传输响应时发现问题");
            }
            free(gatt_rsp);
        }
        else
        {
            ESP_LOGE(TAG, "申请gatt_rsp缓存时发现问题");
        }
    }

    if (status != ESP_GATT_OK)
    {
        return;
    }

    // 准备缓存资源，处理响应需求完成，映射写入的值
    memcpy(prepare_write_env->prepare_buf + param->write.offset, param->write.value, param->write.len);

    // 已准备长度累加
    prepare_write_env->prepare_len += param->write.len;
}

/// @brief 执行写入 事件，在回调函数中使用，为写操作判断是否执行
/// @param prepare_write_env 准备写入配置
/// @param param 回调参数
void execute_write_event(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param)
{

    const char *TAG = "execute_write_event";

    // 如果写入标识为执行状态，并且准备写入缓存非空
    if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC && prepare_write_env->prepare_buf)
    {
        esp_log_buffer_hex(TAG, prepare_write_env->prepare_buf, prepare_write_env->prepare_len); // 以16进制打印写入内容
    }
    else
    {
        ESP_LOGI(TAG, "写入取消");
    }

    // 清理准备写入缓存
    if (prepare_write_env->prepare_buf)
    {
        free(prepare_write_env->prepare_buf);
        prepare_write_env->prepare_buf = NULL;
    }

    prepare_write_env->prepare_len = 0;
}

/// @brief 为用户配置事件处理提供的回调函数
/// @param event 回调触发事件类型
/// @param gatts_if gatts接口类型
/// @param param    回调触发的参数
static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    const char *TAG = "gatts_profile_event_handler";
    switch (event)
    {
    // 存储应用
    case ESP_GATTS_REG_EVT:
    {
        if (esp_ble_gap_set_device_name(BLE_DEVICE_NAME))
            ESP_LOGE(TAG, "设置设备名称时发现问题");

        if (esp_ble_gap_config_adv_data(&adv_data))
            ESP_LOGE(TAG, "配置广播时发现问题");
        adv_config_done |= ADV_CONFIG_FLAG;

        if (esp_ble_gap_config_adv_data(&scan_rsp_data))
            ESP_LOGE(TAG, "配置扫描响应时发现问题");
        adv_config_done |= SCAN_RSP_CONFIG_FLAG;

        if (esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, HRS_IDX_NB, SVC_INST_ID))
            ESP_LOGE(TAG, "创建属性表时发现问题");
    }
    break;

    // 属性表创建
    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
    {
        if (param->add_attr_tab.status != ESP_GATT_OK)
        {
            ESP_LOGE(TAG, "创建属性表时发现问题- 0x%x", param->add_attr_tab.status);
        }
        else if (param->add_attr_tab.num_handle != HRS_IDX_NB)
        {
            ESP_LOGE(TAG, "属性表只创建了一部分 %d / %d", param->add_attr_tab.num_handle, HRS_IDX_NB);
        }
        else
        {
            ESP_LOGI(TAG, "成功创建属性表 属性表句柄数 %d", param->add_attr_tab.num_handle);
            // 完善句柄
            memcpy(heart_rate_handle_table, param->add_attr_tab.handles, sizeof(heart_rate_handle_table));
            // 启动服务
            esp_ble_gatts_start_service(heart_rate_handle_table[IDX_SVC]);
        }
        break;
    }

    // 读操作
    case ESP_GATTS_READ_EVT:
        ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_READ_EVT");
        break;

    // 写操作
    case ESP_GATTS_WRITE_EVT:
        if (!param->write.is_prep)
        {

            ESP_LOGI(TAG, "写入长度 %d,以下以HEX格式展示写入的值", param->write.len);
            esp_log_buffer_hex(TAG, param->write.value, param->write.len);

            // 特征数据传输方式识别，当客户端设定传输方式运行
            if (heart_rate_handle_table[IDX_CHAR_CFG_A] == param->write.handle && param->write.len == 2)
            {
                uint16_t descr_value = param->write.value[1] << 8 | param->write.value[0];
                if (descr_value == 0x0001)
                {

                    ESP_LOGI(TAG, "notify启用");

                    // esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, heart_rate_handle_table[IDX_CHAR_VAL_A],
                    //                         sizeof(notify_data), notify_data, false);
                }
                else if (descr_value == 0x0002)
                {

                    ESP_LOGI(TAG, "indicate启用");

                    // esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, heart_rate_handle_table[IDX_CHAR_VAL_A],
                    //                     sizeof(indicate_data), indicate_data, true);
                }
                else if (descr_value == 0x0000)
                {
                    ESP_LOGI(TAG, "notify/indicate关闭");
                }
                else
                {
                    ESP_LOGE(TAG, "未知的传输描述：");
                    esp_log_buffer_hex(TAG, param->write.value, param->write.len);
                }
            }
            // 如果存在响应需求，传输响应信息
            if (param->write.need_rsp)
            {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
        }
        else
        {
            // 准备写操作
            prepare_write_event(gatts_if, &prepare_write_env, param);
        }
        break;

    ////执行写操作
    case ESP_GATTS_EXEC_WRITE_EVT:
        ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_EXEC_WRITE_EVT");
        execute_write_event(&prepare_write_env, param);
        break;

    // 连接
    case ESP_GATTS_CONNECT_EVT:

        ESP_LOGI(TAG, "蓝牙连接 连接ID %d", param->connect.conn_id);
        esp_log_buffer_hex(TAG, param->connect.remote_bda, 6);

        // 上传连接参数
        esp_ble_conn_update_params_t conn_params = {0};
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        conn_params.latency = BLE_CONNECT_SLAVE_LATENCY;
        conn_params.min_int = BLE_CONNECT_MIN_INTERVAL;
        conn_params.max_int = BLE_CONNECT_MAX_INTERVAL;
        conn_params.timeout = BLE_CONNECT_TIMEOUT;
        esp_ble_gap_update_conn_params(&conn_params);

        break;

    // 断开连接
    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "连接断开, 因为 0x%x", param->disconnect.reason);
        esp_ble_gap_start_advertising(&adv_params); // 回到广播态
        break;

    case ESP_GATTS_MTU_EVT:
        break;
    case ESP_GATTS_CONF_EVT:
        break;
    case ESP_GATTS_START_EVT:
        break;
    case ESP_GATTS_STOP_EVT:
        break;
    case ESP_GATTS_OPEN_EVT:
        break;
    case ESP_GATTS_CANCEL_OPEN_EVT:
        break;
    case ESP_GATTS_CLOSE_EVT:
        break;
    case ESP_GATTS_LISTEN_EVT:
        break;
    case ESP_GATTS_CONGEST_EVT:
        break;
    case ESP_GATTS_UNREG_EVT:
        break;
    case ESP_GATTS_DELETE_EVT:
        break;
    default:
        break;
    }
}

// 通用蓝牙接入函数
void bluetooth_connect()
{

    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 因为只使用BLE,释放经典蓝牙的缓存，这个操作不可逆
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

    // 初始化蓝牙硬件控制器
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);

    // 使能蓝牙硬件控制器
    esp_bt_controller_enable(ESP_BT_MODE_BLE);

    // 初始化蓝牙API并申请资源
    esp_bluedroid_init();

    // 启动蓝牙API
    esp_bluedroid_enable();

    // 为gatts安装回调函数
    esp_ble_gatts_register_callback(gatts_event_handler);

    // 为gap安装回调函数
    esp_ble_gap_register_callback(gap_event_handler);

    // 注册应用标识 app_id
    esp_ble_gatts_app_register(ESP_APP_ID);

    // 设置最大可传输单元MTU大小
    esp_ble_gatt_set_local_mtu(500);
}
