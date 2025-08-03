
/*
 * 701Enti MIT License
 *
 * Copyright © 2025 <701Enti organization>
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
 // 伪P2P数据市场 - PsP2P_DM(Pseudo-P2P Data Market) 是一个借鉴P2P设计的单机本地数据通信框架
 // "for_idf"表示此版本为使用ESP-IDF开发平台的设备提供
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // github: https://github.com/701Enti
 // bilibili: 701Enti


 // [简介]---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 // 伪P2P数据市场 - PsP2P_DM(Pseudo-P2P Data Market) 是一个借鉴P2P设计的单机本地数据通信框架
 // 在如物联接入与中心化数据存储等需要数据聚合的场景,传统的简易化开发逻辑可能存在调用过多其他程序数据访问API以获取全局数据的麻烦,这可能不利于开源智能硬件的维护与后续的迭代工作
 // 例如,一些开发者并不需要维护硬件中的某些传感器的程序部分,因此可能选择不上件,然而这样的正常需求将可能导致大量的报错,因为这些API无法访问到传感器,同时,即使注释代码,在推送更改时也需要额外的撤销步骤,很不方便
 // 因此,通过去中心化准分布式设计的伪P2P数据市场-PsP2P_DM(Pseudo-P2P Data Market),每个程序模块都可以随时随地独立地从任何程序模块获取动态更新的封装资源,而不是传统的API访问
 // 这样传感器模块在发现传感器未上件,将不发布数据,物联接入与中心化数据存储未发现这些数据也不会理会,这样就不会因为直接去调用API而产生错误
 // 同时,这种强容错弹性设计方便了冗余部署,在某个组分异常时,其他健康冗余部分可以发现到并快速介入处理,比如Flash空间不足,其他存储介质(可能多种)介入存储等场景,而传统冗余可能需要更复杂的逻辑


 // [部分相关概念与逻辑]-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 // 节点(node):一个数据产生或使用的个体,一个函数库的一个身份占用一个节点,节点可以根据身份分为生产者或消费者
 // 产品(product):持有数据资源resource和消费者引用等各种与产品有关数据的数据句柄
 // 节点名(node_name):1.节点名一般是一个数据产生或使用的个体的名字,在实践中,建议使用c宏定义__FILE__作为节点名
 //                  2.可以创建同名但身份不同的节点,不允许创建同名同身份节点
 // 产品名(product_name):1.产品名一般是产品数据资源resource存储对象名,比如把结构体变量的指针作为产品数据资源resource上架,那么产品名一般是结构体变量的变量名
 //                     2.同一生产者不可以有完全同名产品, 上架时会检查产品名, 若产品名被使用过(根据内容比较)不允许上架
 //                     3.消费者可以购买产品名相同,但是生产者名不同的产品,同一生产者不可以有完全同名产品,所以消费者不可能买到它们
 // 节点链表:所有节点创建时就被加入节点链表,使得任何节点实现自主访问
 // 产品链表:每个节点都有一个产品链表,对生产者,产品链表包含它上架且未下架的产品,对消费者,产品链表包含它购买且未退货的产品,每个节点的产品链表各自唯一且独立,伴随节点整个生命周期
 //         消费者购买产品获得的产品句柄是克隆而非引用,同一产品会占用不同节点的内存资源, 互不干涉
 // 生产者(producer):产生数据的个体,比如传感器库,上架/下架产品(即数据访问句柄)供消费者利用,生产者的本质是身份为生产者的节点,生产者持有自身节点句柄,并通过节点句柄的链表特性访问任何节点
 // 消费者(consumer):消费数据的个体,比如数据持久化存储库,消费者的本质是身份为消费者的节点,消费者持有自身节点句柄,并通过节点句柄的链表特性访问任何节点
 // 多身份:同一个函数库的一个身份占用一个节点,节点可以是生产者或消费者,通过创建多个身份不同的节点,分别维护,可以实现函数库个体多身份
 // 数据访问:生产者上架(put)产品->消费者购买(buy)产品->[消费者内部自己实现:对指定产品的数据资源resource访问]
 // 上架(put):这将使得新产品添加到生产者的产品链表中,之后消费者可以从自己的节点遍历链表,找到需要的生产者(依据node_name),选择需要的产品(依据product_name)完成购买
 // 购买(buy):这将使得购买的产品添加到消费者的产品链表中,之后消费者可以随时从产品链表选择需要的产品(依据product_name)进行资源访问
 // 消费者节点列表(consumer_node_list):消费者持有消费者节点列表的引用,从而可以被任何人访问,但是由生产者管理列表占用的内存资源,由消费者自行加入自己到列表中(buy中实现),其中元素角标与该元素即消费者的consumer_id相等
 // 垃圾清理:垃圾清理是指清理产品的resource(如调用free),PsP2P_DM不会进行任何垃圾清理操作,也不会管理数据访问以确保安全,不同场景下,需要由使用者完成清理的最佳实践
 //           --注意:可以有以下实践,PsP2P_DM不会规定必须选择某个实践方式,也不会规定必须要实现的细节,只是一些建议
 //         实践1.消费者及时清理(无需联络) - 如果数据只会被一个消费者持有利用, 可以由消费者在使用完后直接清理资源, 实现及时清理(建议清理前设置生产者方产品resource = NULL,防止途中突发购买操作产生野指针)
 //         实践2.生产者及时清理(无需联络) - 等待,观测到产品的consumer_sum由正值跳变为0一段时间(无消费者持有),生产者清理资源(建议清理前设置生产者方产品resource = NULL,防止途中突发购买操作产生野指针)
 //         实践3.生产者通知清理(需要联络) - 生产者发送附加目标产品的PsP2P_DM_REQUEST_REFUND消息给已经购买产品的消费者,在所有消费者[退货]后,将观测到产品的consumer_sum=0,生产者清理资源(建议清理前设置生产者方产品resource = NULL,防止途中突发购买操作产生野指针)
 //         实践n......
 //           --注意:垃圾清理有时通过让消费者[退货]保证引用丢失,提升安全性,但是PsP2P_DM不会规定产品垃圾清理必须让消费者退货,这只是一种实践方式,而不是固有机制
 //           --注意:垃圾清理完毕也不一定所有消费者都退货即consumer_sum=0,consumer_sum=0只是一定实践中的数据观测指标,在consumer_sum不为0的情况下清理资源可能也是合理的
 // 下架(off):完成垃圾清理(resource=NULL)后,PsP2P_DM允许下架操作,这将使得产品从生产者的产品链表中移除,之后消费者无法购买,除非再次上架
 // 退货(refund):这将使得产品从消费者的产品链表中移除,产品的消费者记录consumer_sum减少1,consumer_node_list将停止引用此消费者节点句柄
 // 消息系统:PsP2P_DM自带简易的点对点消息系统,并提供一些通用格式化消息类型,方便节点通信
 // 资源更新:产品的resource是一个指针,因此无需显式调用任何更新操作,直接修改resource指向的数据区内容即可完成跨节点资源更新,
 //         对于需要原子化或加锁的场景, 可以对resource指向的数据本身使用原子化类型, 对使用resource的代码块加互斥锁,对于极其严苛场景,可以考虑垃圾清理后更新
 //         PsP2P_DM并不会访问产品的resource数据区,因此这些操作不必用于PsP2P_DM
 // 销毁节点(node_destory):1.销毁节点会使得节点不能被搜索遍历到,除非之后再次创建
 //                       2.将要销毁的节点的同名而身份不同的节点不受影响
 //                       3.销毁节点不会释放数据资源resource,PsP2P_DM通过以下固有机制在零接触资源的条件确保资源释放和引用安全
 //                       > 需要释放的资源只会有 1.每个节点的:产品标识资源 2.共享但是只由一方管理的:产品消费者节点列表资源(生产者管理),产品数据资源(生产者管理/消费者管理)
 //                       > 节点的产品链表为空链表则允许销毁节点(PsP2P_DM确定能否销毁的唯一判断指标),意味着如果即将销毁的节点是生产者需要下架所有产品,如果即将销毁的节点是消费者需要退货所有产品
 //                       > 如果即将销毁的节点是生产者 -> 需要下架产品[产品标识资源释放+产品消费者节点列表资源释放] -> 需要自己垃圾清理或者消费者清理(最终都是resource=NULL)[产品数据资源释放]
 //                            --注意:在使用快速开发扩展库(PsP2P_DM_FDE)时,如果发现销毁的节点是生产者且名下有上架产品且未下架时,
 //                                   快速开发扩展库使用上文垃圾清理实践3实现产品所有下架(垃圾需用户在之后自行清理)(但是这只是范式模板, 并非PsP2P_DM要求)
 //                       > 如果即将销毁的节点是消费者 -> 需要退货产品[产品标识资源释放+数据资源引用停止]



#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "stdbool.h"
#include "esp_err.h"


#define PSP2P_DM_PRODUCT_NOW_CONSUMER_QUANTITY_MAX 50 //产品的当前消费者最大个数

typedef enum {
    PsP2P_DM_IDENTITY_PRODUCER,//生产者
    PsP2P_DM_IDENTITY_CONSUMER,//消费者
}PsP2P_DM_identity_t;//PsP2P_DM身份

typedef struct PsP2P_DM_node_t* PsP2P_DM_node_handle_t;
typedef struct PsP2P_DM_product_t* PsP2P_DM_product_handle_t;
typedef struct PsP2P_DM_node_t {

    struct PsP2P_DM_node_t* prev;//前驱指针

    const char* node_name;//节点名
    PsP2P_DM_identity_t identity;//身份
    union
    {//根据身份二择一
        _Atomic int put_sum;//当前上架有效产品数量(节点是生产者)
        _Atomic int buy_sum;//当前购买有效产品数量(节点是消费者)
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
        _Atomic int consumer_sum;//当前消费者数量(对生产者)
        int consumer_id;//持有的消费者自增id,从0开始,始终分配最小id
    };
    PsP2P_DM_node_handle_t producer_node;//生产者节点,生产该产品的生产者节点句柄
    PsP2P_DM_node_handle_t* consumer_node_list;//产品当前消费者节点列表  

    struct PsP2P_DM_product_t* next;//后继指针 

}PsP2P_DM_product_t;

typedef enum {
    PsP2P_DM_MESSAGE_CONTENT_PLEASE_REFUND = 1,//请退货(发送该消息时,请在消息附件中填充 来自接收者(消费者)产品链表的需退货商品句柄 构成的PsP2P_DM_product_handle_t数组以确定需退货商品)
    PsP2P_DM_MESSAGE_CONTENT_PLEASE_BUY,//请购买(发送该消息时,请在消息附件中填充 发送者(生产者)产品链表的需购买商品句柄 构成的PsP2P_DM_product_handle_t数组以确定需购买商品)
}PsP2P_DM_message_content_t;//PsP2P_DM消息内容

typedef enum {
    PSP2P_DM_MESSAGE_ECHO_NONE = 0,//暂无回应(默认初始值)
    PSP2P_DM_MESSAGE_ECHO_WORKING,//回应:处理中
    PSP2P_DM_MESSAGE_ECHO_WORKED,//回应:已完成处理
    PSP2P_DM_MESSAGE_ECHO_ERROR,//回应:发现问题
}PsP2P_DM_message_echo_t;

typedef struct PsP2P_DM_message_t {
    PsP2P_DM_node_handle_t receiver;
    PsP2P_DM_message_content_t content;//消息内容,这是一个枚举类型[注意:PsP2P_DM不会检查该数据的合理性]
    union {
        //以下两个编辑时间根据具体情况二择一使用,并通过edit_time_is_MT标注选择(仅仅只是标识注释,不是选择参数),其为true是表示单调时间,为false是表示实时时间
        struct timeval edit_time_RTC;//消息发送前最后一次编辑时的时间(系统微秒级实时时间) - (默认使用-在保证NTP时间同步完成,系统时间与现实时间一致情况下)
        int64_t edit_time_MT;//消息发送前最后一次编辑时的时间(系统微秒级单调时间) - (备用-在如网络未连接,无法进行NTP时间同步,系统时间与现实时间可能不一致情况下)        
    };
    bool edit_time_is_MT;//以上两个编辑时间根据具体情况二择一使用,并通过edit_time_is_MT标注选择(仅仅只是标识注释,不是选择参数),其为true是表示单调时间,为false是表示实时时间
    void* attached;//附件,按照消息上下文添加附件,如果不需要附加,设置为NULL[注意:PsP2P_DM不会检查该数据的合理性]
    int attached_total_size;//附件数据总大小(单位:Byte),如果不需要告知,设置为0[注意:PsP2P_DM不会检查该数据的合理性]
    PsP2P_DM_message_echo_t echo;//消息回应(接收者可更改该变量以进行简单回应),这是一个枚举类型[注意:PsP2P_DM不会检查该数据的合理性]
}PsP2P_DM_message_t;//PsP2P_DM消息

typedef struct PsP2P_DM_message_t* PsP2P_DM_message_list_t;


typedef struct PsP2P_DM_message_box_t {
    PsP2P_DM_message_list_t message_list;//消息列表
    int total_message_amount;//总消息数量
}PsP2P_DM_message_box_t;//PsP2P_DM消息信箱

typedef struct PsP2P_DM_message_box_t* PsP2P_DM_message_box_handle_t;



PsP2P_DM_node_handle_t PsP2P_DM_node_create(const char* node_name, PsP2P_DM_identity_t identity, UBaseType_t uxMessageQueueLength);



esp_err_t PsP2P_DM_node_destory(PsP2P_DM_node_handle_t node_to_destory);



esp_err_t PsP2P_DM_put(PsP2P_DM_node_handle_t producer, void* resource, const char* product_name);


esp_err_t PsP2P_DM_off(PsP2P_DM_node_handle_t producer, const char* product_name);


esp_err_t PsP2P_DM_off_all(PsP2P_DM_node_handle_t producer);


esp_err_t PsP2P_DM_buy(PsP2P_DM_node_handle_t consumer, const char* producer_name, const char* product_name);


esp_err_t PsP2P_DM_message_send(PsP2P_DM_node_handle_t receiver, PsP2P_DM_message_t* message, TickType_t xTicksToWait);


esp_err_t PsP2P_DM_message_box_create_as_receiver_own_consumer(PsP2P_DM_node_handle_t producer, PsP2P_DM_message_box_handle_t* result);    