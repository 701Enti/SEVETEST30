// 该文件由701Enti整合，包含一些sevetest30的  低功耗蓝牙BLE环境中  数据获取（BWEDA）以及其他设备的蓝牙交互活动
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：有效的数据存储变量都封装在该库下，不需要在外部函数定义一个数据结构体缓存作为参数，直接读取公共变量，主要为了方便FreeRTOS的任务支持
// 敬告：ESP32S3目前只支持BLE,不支持经典蓝牙的音频传输，这里没有音频通讯的功能
// 敬告：蓝牙配置操作使用了ESP-IDF官方例程并进行修改，非常感谢,
// 敬告：以下注释为个人理解，不是实际意义
// SIG官方提供的包含外观特征值 UUID 等定义的文档链接(2.6.2节-外观特征值 3.4.2节-UUID) https://www.bluetooth.com/specifications/assigned-numbers/
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.11.5

#include "sevetest30_BWEDA.h"
#include "board_def.h"
#include "board_ctrl.h"
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

#define ESP_APP_ID 0x55

#define SVC_INST_ID 0

#define ADV_CONFIG_FLAG (1 << 0)
#define SCAN_RSP_CONFIG_FLAG (1 << 1)

static uint8_t adv_config_done = 0; // 广播状态

// 准备写入事件缓存类型
typedef struct
{
    uint8_t *prepare_buf;
    int prepare_len;
} prepare_type_env_t;

// GATT服务器用户配置存储类型
struct gatts_profile_inst
{
    esp_gatts_cb_t gatts_cb;       // 用于用户配置事件处理的回调函数
    uint16_t gatts_if;             // gatts接口类型
    uint16_t app_id;               // 应用ID
    uint16_t conn_id;              // 连接ID
    uint16_t service_handle;       // 指定的服务句柄
    esp_gatt_srvc_id_t service_id; // 指定的服务集合ID
    uint16_t char_handle;          // 指定的特性句柄
    esp_bt_uuid_t char_uuid;       // 指定的特性UUID
    esp_gatt_perm_t perm;          //
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

static prepare_type_env_t prepare_write_env;

////////////////////////////////////////////////////////////////服务配置///////////////////////////////////////////////////////////////////////////////

// 应用表APP注册顺序
typedef enum
{
    IO_CTRL_PROFILE_APP = 0,
    MEDIA_CTRL_PROFILE_APP,

    PROFILE_NUM,
};

static void IO_CTRL_SERVICE_profile_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void MEDIA_CTRL_SERVICE_profile_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

// 应用表
static struct gatts_profile_inst app_profile_table[PROFILE_NUM] = {
    [IO_CTRL_PROFILE_APP] = {
        .gatts_cb = IO_CTRL_SERVICE_profile_handler,
        .gatts_if = ESP_GATT_IF_NONE,
    },

    [MEDIA_CTRL_PROFILE_APP] = {
        .gatts_cb = MEDIA_CTRL_SERVICE_profile_handler,
        .gatts_if = ESP_GATT_IF_NONE,
    },

};

/////////////////////SIG规定的预定义UUID//////////////////////////
// 服务UUID,重复可能性较低，这里所有服务UUID统一用SIG规定的
#define SERV_UUID_AUTOMATION_IO 0x1815         // 自动IO控制
#define SERV_UUID_GENERIC_MEDIA_CONTROL 0x1849 // 通用媒体控制

// 特征UUID，因为可能同一个服务有多个相同属性特征，所以一般使用下面的自定义特征UUID
#define CHAR_UUID_ACS_DATA_OUT_NOTIFY 0x2B31
#define CHAR_UUID_ACS_DATA_OUT_INDICATE 0x2B32
///////////////////////////////////////////////////////////////////

////////////////////////自定义特征UUID////////////////////////
typedef enum
{
    SE30_CHAR_UUID_MIN = 0,

    SE30_CHAR_UUID_1,
    SE30_CHAR_UUID_2,
    SE30_CHAR_UUID_3,
    SE30_CHAR_UUID_4,
    SE30_CHAR_UUID_5,
    SE30_CHAR_UUID_6,
    SE30_CHAR_UUID_7,
    SE30_CHAR_UUID_8,
    SE30_CHAR_UUID_9,
    SE30_CHAR_UUID_10,
    SE30_CHAR_UUID_11,
    SE30_CHAR_UUID_12,
    SE30_CHAR_UUID_13,
    SE30_CHAR_UUID_14,
    SE30_CHAR_UUID_15,

    SE30_CHAR_UUID_MAX,
};
////////////////////////////////////////////////////////

// 公共缓存
static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
// static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

// static const uint8_t char_prop_read                =  ESP_GATT_CHAR_PROP_BIT_READ;
// static const uint8_t char_prop_write               = ESP_GATT_CHAR_PROP_BIT_WRITE;
static const uint8_t char_prop_read_write_notify = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
// static const uint8_t char_prop_read_write_notify_indicate = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY | ESP_GATT_CHAR_PROP_BIT_INDICATE;
/*******************************************************自动IO服务************************************************************************/
// 16bits服务UUID
#define BLE_SERVICE_IO_CTRL_UUID16 (SERV_UUID_AUTOMATION_IO)

// 服务元素列表
enum
{
    IO_CTRL_SERVICE, // 服务

    IO_CTRL_EN_DISPLAY_CHAR,

    IO_CTRL_EN_DISPLAY_VALUE,

    IO_CTRL_IDX_NB,
};

// 服务句柄列表
uint16_t io_ctrl_service_handle_table[IO_CTRL_IDX_NB];

// 服务相关数据缓存
static const uint16_t io_ctrl_service_uuid = BLE_SERVICE_IO_CTRL_UUID16;

static const uint16_t io_ctrl_uuid_buf_1 = SE30_CHAR_UUID_1;

static uint8_t io_ctrl_display_en = true;

// 服务数据库，可以包含BLE中一个服务和n个特性的属性数据以及响应模式，注意，这里的属性不是指代BLE中特征值的读写权限属性，而是所有描述属性
// 数据库使用结构体数组加枚举类型索引，数组每个单元由 响应模式控制类型 和 属性描述结构体 构成，
// 属性描述结构体包含属性的 UUID长度 +指向UUID的指针(uint8_t *) + 属性许可 +  数据值最大大小 +  数据值实际大小 + 指向数据值的指针(uint8_t *)构成
//(UUID 数据值单位 都需要遵循 SIG 的定义)(以上来自个人理解不是实际意义)
static const esp_gatts_attr_db_t io_ctrl_gatt_database[IO_CTRL_IDX_NB] =
    {
        [IO_CTRL_SERVICE] = {
            {ESP_GATT_AUTO_RSP},
            {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid,
             ESP_GATT_PERM_READ,
             sizeof(uint16_t), sizeof(io_ctrl_service_uuid), (uint8_t *)&io_ctrl_service_uuid}},

        [IO_CTRL_EN_DISPLAY_CHAR] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_read_write_notify}},

        [IO_CTRL_EN_DISPLAY_VALUE] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&io_ctrl_uuid_buf_1, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&io_ctrl_display_en}},

};
/**********************************************************************************************************************************/

/*******************************************************通用媒体控制服务************************************************************************/
// 16bits服务UUID
#define BLE_SERVICE_MEDIA_CTRL_UUID16 SERV_UUID_GENERIC_MEDIA_CONTROL

// 服务元素列表
enum
{
    MEDIA_CTRL_SERVICE, // 服务

    MEDIA_CTRL_VOL_AMP_CHAR,  // 扬声器-功率放大器音量
    MEDIA_CTRL_VOL_AMP_VALUE, // 扬声器-功率放大器音量-值
    MEDIA_CTRL_VOL_AMP_CFG,

    MEDIA_CTRL_MUTE_AMP_CHAR,  // 扬声器-功率放大器音量
    MEDIA_CTRL_MUTE_AMP_VALUE, // 扬声器-功率放大器音量-值
    MEDIA_CTRL_MUTE_AMP_CFG,

    MEDIA_CTRL_IDX_NB,
};

// 服务句柄列表
uint16_t media_ctrl_service_handle_table[MEDIA_CTRL_IDX_NB];

// 服务相关数据缓存
static const uint16_t media_ctrl_service_uuid = BLE_SERVICE_MEDIA_CTRL_UUID16;

static const uint16_t media_ctrl_uuid_buf_1 = SE30_CHAR_UUID_2;
static const uint16_t media_ctrl_uuid_buf_2 = SE30_CHAR_UUID_3;

static uint8_t media_ctrl_amp_vol_buf = 0x00;
static uint8_t media_ctrl_amp_mute_buf = false;

static uint16_t media_ctrl_cfg_buf1 = 0x0000;
static uint16_t media_ctrl_cfg_buf2 = 0x0000;

// 服务数据库，可以包含BLE中一个服务和n个特性的属性数据以及响应模式，注意，这里的属性不是指代BLE中特征值的读写权限属性，而是所有描述属性
// 数据库使用结构体数组加枚举类型索引，数组每个单元由 响应模式控制类型 和 属性描述结构体 构成，
// 属性描述结构体包含服务或特性的 UUID长度 +指向UUID的指针(uint8_t *) + 属性许可 +  数据值最大大小 +  数据值实际大小 + 指向数据值的指针(uint8_t *)构成
//(UUID 数据值单位 都需要遵循 SIG 的定义)(以上来自个人理解不是实际意义)
static const esp_gatts_attr_db_t media_ctrl_gatt_database[MEDIA_CTRL_IDX_NB] =
    {
        [MEDIA_CTRL_SERVICE] = {
            {ESP_GATT_AUTO_RSP},
            {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid,
             ESP_GATT_PERM_READ,
             sizeof(uint16_t), sizeof(media_ctrl_service_uuid), (uint8_t *)&media_ctrl_service_uuid}},

        [MEDIA_CTRL_VOL_AMP_CHAR] = {{ESP_GATT_AUTO_RSP},
         {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid,
          ESP_GATT_PERM_READ,
           sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_read_write_notify}},

        [MEDIA_CTRL_VOL_AMP_VALUE] = {{ESP_GATT_AUTO_RSP},
         {ESP_UUID_LEN_16,
          (uint8_t *)&media_ctrl_uuid_buf_1,
           ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&media_ctrl_amp_vol_buf}},

        [MEDIA_CTRL_VOL_AMP_CFG] = {{ESP_GATT_AUTO_RSP},
         {ESP_UUID_LEN_16,
          (uint8_t *)&character_client_config_uuid,
           ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            sizeof(uint16_t), sizeof(uint16_t), (uint8_t *)&media_ctrl_cfg_buf1}},

        [MEDIA_CTRL_MUTE_AMP_CHAR] = {{ESP_GATT_AUTO_RSP},
         {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid,
          ESP_GATT_PERM_READ,
           sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_read_write_notify}},

        [MEDIA_CTRL_MUTE_AMP_VALUE] = {{ESP_GATT_AUTO_RSP},
         {ESP_UUID_LEN_16,(uint8_t *)&media_ctrl_uuid_buf_2,
           ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&media_ctrl_amp_mute_buf}},

        [MEDIA_CTRL_MUTE_AMP_CFG] = {{ESP_GATT_AUTO_RSP},
         {ESP_UUID_LEN_16,(uint8_t *)&character_client_config_uuid,
           ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            sizeof(uint16_t), sizeof(uint16_t), (uint8_t *)&media_ctrl_cfg_buf2}}};
/**********************************************************************************************************************************/

// 广播传输的服务UUID，BLE可以设置多个服务，然而，在广播时只传输一个关键服务的UUID,方便其他设备快速了解功能，这个UUID是否存在不影响基本功能
static uint8_t adv_key_service_uuid[ESP_UUID_LEN_128] = BLE_SERVICE_UUID_BASE(BLE_SERVICE_IO_CTRL_UUID16);

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
    .service_uuid_len = 0,                                                // 关键服务UUID长度，BLE可以设置多个服务，然而，在广播时只传输一个关键服务的UUID,方便其他设备快速了解功能，这个UUID是否存在不影响基本功能                                               // 关键服务UUID长度，BLE可以设置多个服务，然而，在广播时只传输一个关键服务的UUID,方便其他设备快速了解功能，这个UUID是否存在不影响基本功能
    .p_service_uuid = NULL,                                               // 导入关键服务UUID
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT), // 广播模式标识
};

// 扫描响应数据,广播启动后，当ESP32接收到其他设备扫描请求，回复扫描响应，这往往是广播数据的补充，使用了与广播数据一样的数据结构处理
static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp = true,     // 以下内容是为扫描响应配置的
    .include_name = false,    // 在扫描响应中添加设备名称
    .include_txpower = false, // 在扫描响应中添加发射功率
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .appearance = BLE_DEVICE_APPEARANCE_VALUE, // 蓝牙设备外观特征值，描述产品基本类型
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(adv_key_service_uuid), // 关键服务UUID长度，BLE可以设置多个服务，然而，在广播时只传输一个关键服务的UUID,方便其他设备快速了解功能，这个UUID是否存在不影响基本功能
    .p_service_uuid = adv_key_service_uuid,           // 导入关键服务UUID
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

/// @brief gap相关事件处理回调函数,GAP与广播活动相关，一般无需个性化的修改
/// @param event 回调事件
/// @param param 回调参数
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    const char *TAG = "gap_event_handler";

    switch (event)
    {
    // 广播数据设置完成
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~ADV_CONFIG_FLAG);
        if (adv_config_done == 0)
        {
            esp_ble_gap_start_advertising(&adv_params); // 设置正常，广播启动
        }
        break;

    // 扫描响应数据设置完成
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
        if (adv_config_done == 0)
        {
            esp_ble_gap_start_advertising(&adv_params); // 设置正常，发送响应
        }
        break;

    // 广播启动工作结束
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "启动广播时发现问题");
        }
        else
        {
            ESP_LOGI(TAG, "启动广播成功");
        }
        break;

    // 广播终止工作结束
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "终止广播时发现问题");
        }
        else
        {
            ESP_LOGI(TAG, "终止广播成功");
        }
        break;

    // 上传连接参数
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(TAG, "上传连接参数 状态-%d 最小连接间隔-%d 最大连接间隔-%d 连接间隔-%d 从机延迟-%d 超时时间-%d",
                 param->update_conn_params.status,
                 param->update_conn_params.min_int,
                 param->update_conn_params.max_int,
                 param->update_conn_params.conn_int,
                 param->update_conn_params.latency,
                 param->update_conn_params.timeout);
        break;
    default:
        ESP_LOGW(TAG, "其他事件触发了,但是处理方式没有被设置");
        break;
    }
}

/// 以下gatts_event_handler这个回调函数有多种写法，这里对以下讲解,
/// 我们这里 一个服务对应了一个应用，每个应用对应不同 gatts_if接口
/// BLE刚刚启动时，应用没有分配接口，没有创建属性表，没有启动应用对应服务，但是应用配置回调是确定的,以下仅包含预调用活动
/// T1 - (第1次运行)被系统内部调用了 - 事件: ESP_GATTS_REG_EVT
/// T2 - (第1次运行)开始 预调度 活动，(预调度是为了每个应用顺利地 创建属性表 启动应用对应服务)
/// T3 - (第1次运行)调度 应用表第 0 个应用运行对应回调 创建属性表
/// T4 - (第1次运行)结束
/// T5 - (第2次运行)被系统内部调用了 - 事件：ESP_GATTS_CREAT_ATTR_TAB_EVT ,因为应用0创建属性表完成了
/// T6 - (第2次运行)调度 应用表第 0 个应用运行对应回调 报告创建结果并打开服务
/// T7 - (第2次运行)为第 0 个应用分配接口
/// T8 - (第2次运行)调用esp_ble_gatts_app_register希望系统创建应用1
/// T9 - (第2次运行)结束
/// T10 -(第3次运行)被系统内部调用了并且参数gatts_if更新了 - 事件: ESP_GATTS_REG_EVT,因为得到了创建应用的请求，
/// T11 -(第3次运行)开始 预调度 活动
/// T12 -(第3次运行)调度 应用表第 1 个应用运行对应回调 创建属性表
/// T13 -(第3次运行)结束
/// T14 -(第4次运行)被系统内部调用了 - 事件：ESP_GATTS_CREAT_ATTR_TAB_EVT ,因为应用1创建属性表完成了
/// T15 -(第4次运行)调度 应用表第 1 个应用运行对应回调 报告创建结果并打开服务
/// T16 -(第4次运行)为第 1 个应用分配接口
/// T17 -(第4次运行)结束，但是没有调用esp_ble_gatts_app_register希望系统创建应用2，因为没有应用2 -> 预调度 活动结束
/// Tn  -(第n次运行)根据系统提供的gatts_if接口，运行特定的回调

/// @brief gatt服务器相关事件处理回调函数
/// @param event 回调触发事件类型
/// @param gatts_if  GATT服务器访问接口
/// @param param    回调触发的参数
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{

    const char *TAG = "gatts_event_handler";

    static int idx = 0;

    switch (event)
    {
    case ESP_GATTS_REG_EVT:
    {
        if (param->reg.status == ESP_GATT_OK)
        {
            // 引导GAP进行活动,只进行一次
            if (idx == PROFILE_NUM - 1)
            {
                if (esp_ble_gap_set_device_name(BLE_DEVICE_NAME))
                    ESP_LOGE(TAG, "设置设备名称时发现问题");
                if (esp_ble_gap_config_adv_data(&adv_data))
                    ESP_LOGE(TAG, "配置广播时发现问题");
                adv_config_done |= ADV_CONFIG_FLAG;
                if (esp_ble_gap_config_adv_data(&scan_rsp_data))
                    ESP_LOGE(TAG, "配置扫描响应时发现问题");
                adv_config_done |= SCAN_RSP_CONFIG_FLAG;
            }

            if (app_profile_table[idx].gatts_if == ESP_GATT_IF_NONE)
                app_profile_table[idx].gatts_cb(event, gatts_if, param);
        }
        else
        {
            ESP_LOGE(TAG, "存储应用时发现问题, APP_ID %04x, 状态 %d", param->reg.app_id, param->reg.status);
        }
        return;
    }
    break;

    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
    {
        // 继续选定现在的应用，完成服务启动工作
        app_profile_table[idx].gatts_cb(event, gatts_if, param);

        // 分配接口
        app_profile_table[idx].gatts_if = gatts_if;

        // 引导下一个应用
        idx++;

        if (idx < PROFILE_NUM)
            esp_ble_gatts_app_register(idx); // 注意，注册下一个应用时，系统会分配新的接口，下一个应用将占用这个接口

        return;
    }
    break;

    case ESP_GATTS_DISCONNECT_EVT:
        esp_ble_gap_start_advertising(&adv_params); // 回到广播态
        break;

    default:
        break;
    }

    for (int i = 0; i < PROFILE_NUM; i++)
    {
        // 每次gatt服务器回调触发，包含一个特定的gatts_if,不同应用对应不同gatts_if，所以会运行特定的应用回调，
        // 在一个应用完成 预调度 后，将可以被这样自由地调用
        // 应用APP_ID不处在默认时的ESP_GATT_IF_NONE时，表示完成 预调度
        if (gatts_if != ESP_GATT_IF_NONE && gatts_if == app_profile_table[i].gatts_if)
        {
            if (app_profile_table[i].gatts_cb)
                app_profile_table[i].gatts_cb(event, gatts_if, param);
        }
    }
}

/// @brief 为IO_CTRL_SERVICE配置事件处理提供的回调函数，由gatts_event_handler调度
/// @param event 回调触发事件类型
/// @param gatts_if  GATT服务器访问接口
/// @param param    回调触发的参数
static void IO_CTRL_SERVICE_profile_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    const char *TAG = "IO_CTRL_SERVICE_profile_handler";

    switch (event)
    {
    // 存储应用
    case ESP_GATTS_REG_EVT:
    {
        esp_ble_gatts_create_attr_tab(io_ctrl_gatt_database, gatts_if, IO_CTRL_IDX_NB, SVC_INST_ID);
    }
    break;

    // 服务属性表创建完成
    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
    {
        if (param->add_attr_tab.status != ESP_GATT_OK)
            ESP_LOGE(TAG, "创建IO_CTRL_SERVICE的服务属性表时发现问题- 0x%x", param->add_attr_tab.status);
        else
        {
            ESP_LOGI(TAG, "成功创建IO_CTRL_SERVICE的服务属性表 已创建句柄数 %d", param->add_attr_tab.num_handle);

            // 完善句柄
            memcpy(io_ctrl_service_handle_table, param->add_attr_tab.handles, sizeof(io_ctrl_service_handle_table));
            if (esp_ble_gatts_start_service(io_ctrl_service_handle_table[IO_CTRL_SERVICE]))
                ESP_LOGE(TAG, "IO_CTRL_SERVICE 服务启动失败");
            else
            {
                ESP_LOGI(TAG, "IO_CTRL_SERVICE 服务启动成功");
            }
        }
    }
    break;

    // 读操作
    case ESP_GATTS_READ_EVT:
        ESP_LOGI(TAG, "读操作");
        break;

    // 写操作
    case ESP_GATTS_WRITE_EVT:
        if (!param->write.is_prep)
        {

            ESP_LOGI(TAG, "目标句柄 %d - 写入长度 %d,以下以HEX格式展示写入的值", param->write.handle, param->write.len);
            esp_log_buffer_hex(TAG, param->write.value, param->write.len);

            // // 特征传输配置，当客户端设定传输方式运行
            // if (param->write.handle == io_ctrl_service_handle_table[IDX_CHAR_CFG_A] && param->write.len == 2)
            // {
            //     uint16_t descr_value = param->write.value[1] << 8 | param->write.value[0];
            //     if (descr_value == 0x0001)
            //     {

            //         ESP_LOGI(TAG, "notify启用");

            //         // esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, heart_rate_handle_table[IDX_CHAR_VAL_A],
            //         //                         sizeof(notify_data), notify_data, false);
            //     }
            //     else if (descr_value == 0x0002)
            //     {

            //         ESP_LOGI(TAG, "indicate启用");

            //         // esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, heart_rate_handle_table[IDX_CHAR_VAL_A],
            //         //                     sizeof(indicate_data), indicate_data, true);
            //     }
            //     else if (descr_value == 0x0000)
            //     {
            //         ESP_LOGI(TAG, "notify/indicate关闭");
            //     }
            //     else
            //     {
            //         ESP_LOGE(TAG, "未知的传输描述：");
            //         esp_log_buffer_hex(TAG, param->write.value, param->write.len);
            //     }
            // }

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
        ESP_LOGI(TAG, "执行写操作");
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
        //-----------------------------------------------------------------------------------------------------------------------------------------//复位传输配置
        break;

    case ESP_GATTS_MTU_EVT: // MTU设置完成
        break;
    case ESP_GATTS_CONF_EVT: // 接收到确认信息
        break;
    case ESP_GATTS_START_EVT: // 启动接收完成
        break;
    case ESP_GATTS_STOP_EVT: // 终止接收完成
        break;
    case ESP_GATTS_OPEN_EVT: // 组对连接
        break;
    case ESP_GATTS_CANCEL_OPEN_EVT: // 断开组对
        break;
    case ESP_GATTS_CLOSE_EVT: // 服务关闭
        break;
    case ESP_GATTS_LISTEN_EVT: // 监听
        break;
    case ESP_GATTS_CONGEST_EVT: // 拥堵
        break;
    case ESP_GATTS_UNREG_EVT: // 应用ID未存储
        break;
    case ESP_GATTS_DELETE_EVT: // 服务删除完成
        break;
    case ESP_GATTS_SET_ATTR_VAL_EVT: // 属性设置完成
        break;

    default:
        ESP_LOGW(TAG, "其他事件触发了,但是处理方式没有被设置 - %d", event);
        break;
    }
}

/// @brief 为MEDIA_CTRL_SERVICE配置事件处理提供的回调函数，由gatts_event_handler调度
/// @param event 回调触发事件类型
/// @param gatts_if  GATT服务器访问接口
/// @param param    回调触发的参数
static void MEDIA_CTRL_SERVICE_profile_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    const char *TAG = "MEDIA_CTRL_SERVICE_profile_handler";

    static uint16_t media_ctrl_conn_id_buf1 = 0;

    switch (event)
    {
    // 存储应用
    case ESP_GATTS_REG_EVT:
    {
        esp_ble_gatts_create_attr_tab(media_ctrl_gatt_database, gatts_if, MEDIA_CTRL_IDX_NB, SVC_INST_ID);
    }
    break;

    // 服务属性表创建完成
    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
    {

        if (param->add_attr_tab.status != ESP_GATT_OK)
            ESP_LOGE(TAG, "创建MEDIA_CTRL_SERVICE的服务属性表时发现问题- 0x%x", param->add_attr_tab.status);
        else
        {
            ESP_LOGI(TAG, "成功创建MEDIA_CTRL_SERVICE的服务属性表 已创建句柄数 %d", param->add_attr_tab.num_handle);

            // 完善句柄
            memcpy(media_ctrl_service_handle_table, param->add_attr_tab.handles, sizeof(media_ctrl_service_handle_table));
            if (esp_ble_gatts_start_service(media_ctrl_service_handle_table[MEDIA_CTRL_SERVICE]))
                ESP_LOGE(TAG, "MEDIA_CTRL_SERVICE 服务启动失败");
            else
            {
                ESP_LOGI(TAG, "MEDIA_CTRL_SERVICE 服务启动成功");
            }
        }

        // 数据映射到缓存
        board_ctrl_t *board_ctrl = NULL;
        board_ctrl = board_status_get();
        if (board_ctrl)
        {
            esp_ble_gatts_set_attr_value(media_ctrl_service_handle_table[MEDIA_CTRL_VOL_AMP_VALUE], sizeof(uint8_t), &(board_ctrl->amplifier_volume));
            esp_ble_gatts_set_attr_value(media_ctrl_service_handle_table[MEDIA_CTRL_MUTE_AMP_VALUE], sizeof(uint8_t), &(board_ctrl->amplifier_mute));
        }
    }
    break;

    // 读操作
    case ESP_GATTS_READ_EVT:
    {
        ESP_LOGI(TAG, "读操作");
    }
    break;

    // 写操作
    case ESP_GATTS_WRITE_EVT:
    {
        if (!param->write.is_prep)
        {
            // 特征传输配置，当客户端设定传输方式运行
            uint16_t descr_value = param->write.value[1] << 8 | param->write.value[0];
            media_ctrl_conn_id_buf1 = param->write.conn_id;
            if (param->write.handle == media_ctrl_service_handle_table[MEDIA_CTRL_VOL_AMP_CFG] && param->write.len == 2)
            {
                if (descr_value == 0x0000)
                    media_ctrl_cfg_buf1 = 0x0000;                
                if (descr_value == 0x0001)
                    media_ctrl_cfg_buf1 = CHAR_UUID_ACS_DATA_OUT_NOTIFY;
                if (descr_value == 0x0002)
                    media_ctrl_cfg_buf1 = CHAR_UUID_ACS_DATA_OUT_INDICATE;
            }
            if (param->write.handle == media_ctrl_service_handle_table[MEDIA_CTRL_MUTE_AMP_CFG] && param->write.len == 2)
            {
                if (descr_value == 0x0000)
                    media_ctrl_cfg_buf2 = 0x0000;                
                if (descr_value == 0x0001)
                    media_ctrl_cfg_buf2 = CHAR_UUID_ACS_DATA_OUT_NOTIFY;
                if (descr_value == 0x0002)
                    media_ctrl_cfg_buf2 = CHAR_UUID_ACS_DATA_OUT_INDICATE;
            }


            // 同步控制数据
            board_ctrl_t* board_ctrl = NULL;
            board_ctrl = board_status_get();
            if(board_ctrl){
              if(param->write.handle == media_ctrl_service_handle_table[MEDIA_CTRL_VOL_AMP_VALUE]){
                board_ctrl->amplifier_volume = param->write.value[0];
                sevetest30_board_ctrl(board_ctrl,BOARD_CTRL_AMPLIFIER);//完成配置后，函数会推送数据更新，特征值会保存在本地
              }
              if(param->write.handle == media_ctrl_service_handle_table[MEDIA_CTRL_MUTE_AMP_VALUE]){
                board_ctrl->amplifier_mute = param->write.value[0];
                sevetest30_board_ctrl(board_ctrl,BOARD_CTRL_AMPLIFIER);//完成配置后，函数会推送数据更新，特征值会保存在本地
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
    }
    break;

    ////执行写操作
    case ESP_GATTS_EXEC_WRITE_EVT:
        ESP_LOGI(TAG, "执行写操作");
        execute_write_event(&prepare_write_env, param);

        ESP_LOGI(TAG, "%d", media_ctrl_amp_vol_buf);
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
        media_ctrl_cfg_buf1 = 0x0000; // 复位传输配置
        media_ctrl_cfg_buf2 = 0x0000; // 复位传输配置
        break;

    case ESP_GATTS_MTU_EVT: // MTU设置完成
        break;
    case ESP_GATTS_CONF_EVT: // 接收到确认信息
        break;
    case ESP_GATTS_START_EVT: // 启动接收完成
        break;
    case ESP_GATTS_STOP_EVT: // 终止接收完成
        break;
    case ESP_GATTS_OPEN_EVT: // 组对连接
        break;
    case ESP_GATTS_CANCEL_OPEN_EVT: // 断开组对
        break;
    case ESP_GATTS_CLOSE_EVT: // 服务关闭
        break;
    case ESP_GATTS_LISTEN_EVT: // 监听
        break;
    case ESP_GATTS_CONGEST_EVT: // 拥堵
        break;
    case ESP_GATTS_UNREG_EVT: // 应用ID未存储
        break;
    case ESP_GATTS_DELETE_EVT: // 服务删除完成
        break;
    case ESP_GATTS_SET_ATTR_VAL_EVT: // 属性设置完成
    {

        // 在notify或indicate机制下主动发送数据
        if (media_ctrl_cfg_buf1 == CHAR_UUID_ACS_DATA_OUT_NOTIFY || media_ctrl_cfg_buf1 == CHAR_UUID_ACS_DATA_OUT_INDICATE)
        {
            ESP_LOGI(TAG, "扬声器音量 - %d",media_ctrl_amp_vol_buf);
            esp_ble_gatts_send_indicate(gatts_if, media_ctrl_conn_id_buf1, media_ctrl_service_handle_table[MEDIA_CTRL_VOL_AMP_VALUE],
                                        sizeof(uint8_t), &(media_ctrl_amp_vol_buf),(media_ctrl_cfg_buf1 == CHAR_UUID_ACS_DATA_OUT_INDICATE));
        }   
        if (media_ctrl_cfg_buf2 == CHAR_UUID_ACS_DATA_OUT_NOTIFY || media_ctrl_cfg_buf2 == CHAR_UUID_ACS_DATA_OUT_INDICATE)
        {
            ESP_LOGI(TAG, "静音扬声器 - %d",media_ctrl_amp_mute_buf);
            esp_ble_gatts_send_indicate(gatts_if, media_ctrl_conn_id_buf1, media_ctrl_service_handle_table[MEDIA_CTRL_MUTE_AMP_VALUE],
                                        sizeof(uint8_t), &(media_ctrl_amp_mute_buf),(media_ctrl_cfg_buf2 == CHAR_UUID_ACS_DATA_OUT_INDICATE));  
        }   


    }
    break;

    default:
        ESP_LOGW(TAG, "其他事件触发了,但是处理方式没有被设置 - %d", event);
        break;
    }
}

/// @brief 通用蓝牙接入函数
/// @return ESP_OK / ESP_FAIL
esp_err_t bluetooth_connect()
{
    const char *TAG = "bluetooth_connect";

    // 初始化NVS存储
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 因为只使用BLE,释放经典蓝牙的缓存，这个操作不可逆
    if (esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT))
    {
        ESP_LOGE(TAG, "释放经典蓝牙的缓存时发现问题");
        return ESP_FAIL;
    }

    // 初始化蓝牙硬件控制器
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if (esp_bt_controller_init(&bt_cfg))
    {
        ESP_LOGE(TAG, "初始化蓝牙硬件控制器时发现问题");
        return ESP_FAIL;
    }

    // 使能蓝牙硬件控制器
    if (esp_bt_controller_enable(ESP_BT_MODE_BLE))
    {
        ESP_LOGE(TAG, "使能蓝牙硬件控制器时发现问题");
        return ESP_FAIL;
    }

    // 初始化蓝牙API并申请资源
    if (esp_bluedroid_init())
    {
        ESP_LOGE(TAG, "初始化蓝牙API并申请资源时发现问题");
        return ESP_FAIL;
    }

    // 启动蓝牙API
    if (esp_bluedroid_enable())
    {
        ESP_LOGE(TAG, "启动蓝牙API时发现问题");
        return ESP_FAIL;
    }

    // 为GATTS安装回调函数
    if (esp_ble_gatts_register_callback(gatts_event_handler))
    {
        ESP_LOGE(TAG, "为GATTS安装回调时发现问题");
        return ESP_FAIL;
    }

    // 为GAP安装回调函数
    if (esp_ble_gap_register_callback(gap_event_handler))
    {
        ESP_LOGE(TAG, "为GAP安装回调时发现问题");
        return ESP_FAIL;
    }

    // 注册应用标识APP_ID,这是一个自用识别参数
    if (esp_ble_gatts_app_register(ESP_APP_ID))
    {
        ESP_LOGE(TAG, "注册应用标识APP_ID时发现问题");
        return ESP_FAIL;
    }

    // 设置最大可传输单元MTU限制大小
    if (esp_ble_gatt_set_local_mtu(BLE_LOCAL_MTU))
    {
        ESP_LOGE(TAG, "设置本地MTU时发现问题");
        return ESP_FAIL;
    }

    return ESP_OK;
}

/// @brief 如果notify或indicate机制启动，会直接向客户端推送更新的特征值，否则它将只保存在本地属性表中,推送工作实际发生在服务配置回调下,推送数据来源为board_ctrl.c申请的堆缓存board_ctrl_buf
void sevetest30_ble_attr_value_push()
{
    board_ctrl_t *board_ctrl = NULL;
    board_ctrl = board_status_get();
    if (board_ctrl)
    {
        // 数据映射到缓存,属性设置完成触发ESP_GATTS_SET_ATTR_VAL_EVT事件，在对应配置回调再向客户端主动推送数据
        media_ctrl_amp_vol_buf = board_ctrl->amplifier_volume;
        esp_ble_gatts_set_attr_value(media_ctrl_service_handle_table[MEDIA_CTRL_VOL_AMP_VALUE], sizeof(uint8_t), &(media_ctrl_amp_vol_buf));

        media_ctrl_amp_mute_buf = board_ctrl->amplifier_mute;
        esp_ble_gatts_set_attr_value(media_ctrl_service_handle_table[MEDIA_CTRL_MUTE_AMP_VALUE], sizeof(uint8_t), &(media_ctrl_amp_mute_buf));
        
        
    }
}
