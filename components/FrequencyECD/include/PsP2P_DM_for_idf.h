#pragma once

#include "freertos/queue.h"

#define PSP2P_DM_PRODUCT_NOW_CONSUMER_QUANTITY_MAX 50 //产品的当前消费者最大个数

typedef enum {
    PsP2P_DM_IDENTITY_PRODUCER,//生产者
    PsP2P_DM_IDENTITY_CONSUMER,//消费者
}PsP2P_DM_identity_t;//PsP2P_DM身份

typedef enum {
    PsP2P_DM_MESSAGE_CONTENT_PLEASE_REFUND = 1,//请退货(发送该消息时,请在消息附件中填充 接收者(消费者)产品链表的需退货商品句柄 构成的PsP2P_DM_product_handle_t数组以确定需退货商品)
    PsP2P_DM_MESSAGE_CONTENT_OK_REFUND,//完成退货
    PsP2P_DM_MESSAGE_CONTENT_PLEASE_BUY,//请购买(发送该消息时,请在消息附件中填充 发送者(生产者)产品链表的需购买商品句柄 构成的PsP2P_DM_product_handle_t数组以确定需购买商品)
    PsP2P_DM_MESSAGE_CONTENT_OK_BUY,//完成购买
}PsP2P_DM_message_content_t;//PsP2P_DM消息内容

typedef struct PsP2P_DM_message_t {
    PsP2P_DM_message_content_t message;//消息内容,这是一个枚举类型[注意:PsP2P_DM不会检查该数据的合理性]
    void* attached;//附件,按照消息上下文添加附件,如果不需要附加,设置为NULL[注意:PsP2P_DM不会检查该数据的合理性]
    int attached_total_size;//附件数据总大小(单位:Byte),如果不需要告知,设置为0[注意:PsP2P_DM不会检查该数据的合理性]
}PsP2P_DM_message_t;//PsP2P_DM消息数据


struct PsP2P_DM_node_t;
typedef struct PsP2P_DM_node_t* PsP2P_DM_node_handle_t;
struct PsP2P_DM_product_t;
typedef struct PsP2P_DM_product_t* PsP2P_DM_product_handle_t;

typedef struct PsP2P_DM_node_t {

    struct PsP2P_DM_node_t* prev;//前驱指针

    const char* node_name;//节点名
    PsP2P_DM_identity_t identity;//身份
    union
    {
        int put_sum;//当前上架有效产品数量(对生产者)
        int buy_sum;//当前购买有效产品数量(对消费者)
    };
    PsP2P_DM_product_handle_t head_product;//头产品
    PsP2P_DM_product_handle_t tail_product;//尾产品
    QueueHandle_t messageQueue;

    struct PsP2P_DM_node_t* next;//后继指针    

}PsP2P_DM_node_t;


typedef struct PsP2P_DM_product_t {

    struct PsP2P_DM_product_t* prev;//前驱指针

    const char* product_name;//产品名
    void* resource;//资源
    union
    {
        int consumer_sum;//当前消费者数量(对生产者)
        int consumer_id;//持有的消费者自增id,从0开始,始终分配最小id
    };
    PsP2P_DM_node_handle_t producer_node;//生产者节点,生产该产品的生产者节点句柄
    PsP2P_DM_node_handle_t* consumer_node_list;//产品当前消费者节点列表  

    struct PsP2P_DM_product_t* next;//后继指针 

}PsP2P_DM_product_t;





PsP2P_DM_node_handle_t PsP2P_DM_node_create(const char* node_name, PsP2P_DM_identity_t identity);

esp_err_t PsP2P_DM_node_destory(PsP2P_DM_node_handle_t node_to_destory);

esp_err_t PsP2P_DM_put(PsP2P_DM_node_handle_t producer, void* resource, const char* product_name);

esp_err_t PsP2P_DM_buy(PsP2P_DM_node_handle_t consumer, const char* producer_name, const char* product_name);