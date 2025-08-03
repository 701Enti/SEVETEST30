
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

#include "PsP2P_DM_for_idf.h"
#include "FrequencyECD_for_idf.h"
#include "stdlib.h"
#include "string.h"
#include "esp_check.h"


static const char* PsP2P_DM_TAG = __FILE__;//PsP2P_DM标签
PsP2P_DM_node_handle_t head_node = NULL;//PsP2P_DM节点链表-头节点
PsP2P_DM_node_handle_t tail_node = NULL;//PsP2P_DM节点链表-尾节点


/// @brief 创建PsP2P_DM节点
/// @param node_name 节点名,一般使用文件名__FILE__
/// @param identity 节点身份,这是一个枚举类型
/// @param uxMessageQueueLength 消息系统-节点消息队列长度
/// @param uxMessageItemSize 消息系统-节点单个消息大小(单位:Byte)
/// @note  节点名一般是一个数据产生或使用的个体的名字,在实践中,建议使用c宏定义__FILE__作为节点名
/// @note  可以创建同名但身份不同的节点,不允许创建同名同身份节点
/// @return PsP2P_DM节点句柄 / [NULL 为空指针的节点名/内存不足/不允许创建同名同身份节点/节点链表存在头或尾缺失,维护状态异常]
PsP2P_DM_node_handle_t PsP2P_DM_node_create(const char* node_name, PsP2P_DM_identity_t identity, UBaseType_t uxMessageQueueLength) {
    if (!node_name) {
        ESP_LOGE(PsP2P_DM_TAG, "为空指针的节点名");
        return NULL;
    }

    PsP2P_DM_node_handle_t new_node = malloc(sizeof(PsP2P_DM_node_t));
    if (!new_node) {
        ESP_LOGE(PsP2P_DM_TAG, "内存不足");
        return NULL;
    }
    else {
        memset(new_node, 0, sizeof(PsP2P_DM_node_t));
        new_node->prev = NULL;
        new_node->next = NULL;

        //消息系统-创建消息队列
        new_node->messageQueue = xQueueCreate(uxMessageQueueLength, sizeof(PsP2P_DM_message_t*));
        if (!new_node->messageQueue) {
            ESP_LOGE(PsP2P_DM_TAG, "创建节点消息队列时发现问题");
            free(new_node);
            new_node = NULL;
            return NULL;
        }

        //填充其他数据
        new_node->head_product = NULL;
        new_node->tail_product = NULL;
        new_node->node_name = node_name;
        new_node->identity = identity;

        //链表连接
        if (head_node != NULL && tail_node != NULL) {
            //链表非空
            //唯一性检查
            PsP2P_DM_node_t* node = head_node;
            while (node)
            {
                if (node->identity == identity) {
                    if (!strcmp(node->node_name, node_name)) {
                        ESP_LOGE(PsP2P_DM_TAG, "不允许创建同名同身份节点");
                        free(new_node);
                        new_node = NULL;
                        return NULL;
                    }
                }
                node = node->next;
            }

            //连接
            tail_node->next = new_node;//原尾节点next指向新节点
            new_node->prev = tail_node;//新节点prev指向原尾节点
            tail_node = new_node;//现在的尾节点是新节点
        }
        else if (head_node == NULL && tail_node == NULL) {
            head_node = tail_node = new_node;//空链表,新节点成为头节点和尾节点
        }
        else {
            ESP_LOGE(PsP2P_DM_TAG, "节点链表存在头或尾缺失,维护状态异常");
            free(new_node);
            new_node = NULL;
            return NULL;
        }

        return new_node;
    }
}

/// @brief 销毁PsP2P_DM节点
/// @param node_handle PsP2P_DM节点句柄
/// @note   节点的产品链表为空链表则允许销毁节点,意味着对于生产者销毁需要下架所有产品,对于消费者销毁需要退货所有产品
/// @return [ESP_OK 成功]
/// @return [ESP_ERR_INVALID_ARG 输入了无法处理的空指针]
/// @return [ESP_ERR_INVALID_STATE 节点链表为空链表,维护状态异常]
/// @return [ESP_ERR_NOT_FINISHED 节点的产品链表不为空链表,请完成相应的产品移除操作再进行销毁]
esp_err_t PsP2P_DM_node_destory(PsP2P_DM_node_handle_t node_to_destory) {
    ESP_RETURN_ON_FALSE(node_to_destory, ESP_ERR_INVALID_ARG, PsP2P_DM_TAG, "输入了无法处理的空指针 描述%s", esp_err_to_name(ESP_ERR_INVALID_ARG));
    ESP_RETURN_ON_FALSE(head_node && tail_node, ESP_ERR_INVALID_STATE, PsP2P_DM_TAG, "节点链表为空链表,维护状态异常 描述%s", esp_err_to_name(ESP_ERR_INVALID_STATE));
    ESP_RETURN_ON_FALSE(node_to_destory->head_product == NULL && node_to_destory->tail_product == NULL, ESP_ERR_NOT_FINISHED,
        PsP2P_DM_TAG, "节点的产品链表不为空链表,请完成相应的产品移除操作再进行销毁 描述%s", esp_err_to_name(ESP_ERR_NOT_FINISHED));
    //从链表移除
    if (node_to_destory == head_node) {
        //销毁节点是头节点
        if (node_to_destory->next) {
            head_node = node_to_destory->next;//有后继节点,新头节点是后继节点
            head_node->prev = NULL;//头节点无前驱节点,新头节点停止对销毁节点的引用
        }
        else {
            //无后继节点,销毁节点又是头节点,那么销毁后,链表为空链表
            head_node = NULL;
            tail_node = NULL;
        }
    }
    else if (node_to_destory == tail_node) {
        //销毁节点是尾节点
        if (node_to_destory->prev) {
            tail_node = node_to_destory->prev;//有前驱节点,新尾节点是前驱节点
            tail_node->next = NULL;//尾节点无后继节点,新尾节点停止对销毁节点的引用
        }
        else {
            //无前驱节点,销毁节点又是尾节点,那么销毁后,链表为空链表
            head_node = NULL;
            tail_node = NULL;
        }
    }
    else {
        //销毁节点是中间节点
        node_to_destory->prev->next = node_to_destory->next;//跳过销毁节点,销毁节点的前驱节点直接指向销毁节点的后继节点
        node_to_destory->next->prev = node_to_destory->prev;//跳过销毁节点,销毁节点的后继节点直接指向销毁节点的前驱节点
    }
    //释放节点资源
    vQueueDelete(node_to_destory->messageQueue);
    free(node_to_destory);
    node_to_destory = NULL;

    return ESP_OK;
}

/// @brief PsP2P_DM产品上架
/// @param producer 生产者句柄
/// @param resource 产品的数据资源
/// @param product_name 产品名
/// @note  产品名一般是产品数据资源source存储对象名,比如把结构体变量的指针作为产品数据资源source上架,那么产品名一般是结构体变量的变量名
/// @note  同一生产者不可以有完全同名产品,上架时会检查产品名,若产品名被使用过(根据内容比较)不允许上架
/// @return [ESP_OK 成功]
/// @return [ESP_ERR_NO_MEM 内存不足]
/// @return [ESP_ERR_INVALID_ARG 输入了无法处理的空指针 / 同一生产者不可以有完全同名产品,对本生产者,产品名在自己的产品链表中已经被使用过]
/// @return [ESP_ERR_INVALID_STATE 产品链表存在头或尾缺失,维护状态异常]
/// @return [ESP_ERR_NOT_SUPPORTED 非生产者不可使用上架(put)操作]
esp_err_t PsP2P_DM_put(PsP2P_DM_node_handle_t producer, void* resource, const char* product_name) {
    ESP_RETURN_ON_FALSE(producer && resource && product_name, ESP_ERR_INVALID_ARG, PsP2P_DM_TAG, "输入了无法处理的空指针 描述%s", esp_err_to_name(ESP_ERR_INVALID_ARG));
    ESP_RETURN_ON_FALSE(producer->identity == PsP2P_DM_IDENTITY_PRODUCER, ESP_ERR_NOT_SUPPORTED, PsP2P_DM_TAG, "非生产者不可使用上架(put)操作 描述%s", esp_err_to_name(ESP_ERR_NOT_SUPPORTED));

    PsP2P_DM_product_handle_t new_product = malloc(sizeof(PsP2P_DM_product_t));//要上架的产品
    ESP_RETURN_ON_FALSE(new_product, ESP_ERR_NO_MEM, PsP2P_DM_TAG, "内存不足  描述%s", esp_err_to_name(ESP_ERR_NO_MEM));
    memset(new_product, 0, sizeof(PsP2P_DM_product_t));
    new_product->prev = NULL;
    new_product->next = NULL;

    PsP2P_DM_node_handle_t* consumer_node_list = malloc(PSP2P_DM_PRODUCT_NOW_CONSUMER_QUANTITY_MAX * sizeof(PsP2P_DM_node_handle_t));
    ESP_RETURN_ON_FALSE(consumer_node_list, ESP_ERR_NO_MEM, PsP2P_DM_TAG, "内存不足  描述%s", esp_err_to_name(ESP_ERR_NO_MEM));
    memset(consumer_node_list, 0, PSP2P_DM_PRODUCT_NOW_CONSUMER_QUANTITY_MAX * sizeof(PsP2P_DM_node_handle_t));

    //填充数据
    new_product->product_name = product_name;
    new_product->resource = resource;
    new_product->consumer_sum = 0;
    new_product->producer_node = producer;
    consumer_node_list = consumer_node_list;

    //链表连接
    if (producer->head_product != NULL && producer->tail_product != NULL) {
        //链表非空
        //唯一性检查
        PsP2P_DM_product_t* product = producer->head_product;
        while (product)
        {
            if (!strcmp(product->product_name, product_name)) {
                free(new_product);
                free(consumer_node_list);
                new_product = NULL;
                consumer_node_list = NULL;
                ESP_RETURN_ON_FALSE(false, ESP_ERR_INVALID_ARG, PsP2P_DM_TAG,
                    "同一生产者不可以有完全同名产品,对本生产者,产品名在自己的产品链表中已经被使用过 描述%s", esp_err_to_name(ESP_ERR_INVALID_ARG));
            }
            product = product->next;
        }

        //连接
        producer->tail_product->next = new_product;//原尾产品next指向新产品
        new_product->prev = producer->tail_product;//新产品prev指向原尾产品
        producer->tail_product = new_product;//现在的尾产品是新产品
        producer->put_sum++;
    }
    else if (producer->head_product == NULL && producer->tail_product == NULL) {
        producer->head_product = producer->tail_product = new_product;//空链表,新产品成为头产品和尾产品
        producer->put_sum++;
    }
    else {
        free(new_product);
        free(consumer_node_list);
        new_product = NULL;
        consumer_node_list = NULL;
        ESP_RETURN_ON_FALSE(false, ESP_ERR_INVALID_STATE, PsP2P_DM_TAG, "产品链表存在头或尾缺失,维护状态异常  描述%s", esp_err_to_name(ESP_ERR_INVALID_STATE));
    }

    return ESP_OK;
}

/// @brief PsP2P_DM产品下架
/// @param producer 生产者句柄
/// @param product_name 产品名
/// @note  产品名一般是产品数据资源source存储对象名,比如把结构体变量的指针作为产品数据资源source上架,那么产品名一般是结构体变量的变量名
/// @note  同一生产者不可以有完全同名产品,上架时会检查产品名,若产品名被使用过(根据内容比较)不允许上架
/// @return [ESP_OK 成功]
/// @return [ESP_ERR_NOT_FOUND 产品链表是非空链表,但是未找到指定的产品 / 产品链表是空链表,不可能找到指定的产品]
/// @return [ESP_ERR_INVALID_ARG 输入了无法处理的空指针]
/// @return [ESP_ERR_INVALID_STATE 产品链表存在头或尾缺失,维护状态异常]
/// @return [ESP_ERR_NOT_SUPPORTED 非生产者不可使用下架(off)操作]
esp_err_t PsP2P_DM_off(PsP2P_DM_node_handle_t producer, const char* product_name) {
    ESP_RETURN_ON_FALSE(producer && product_name, ESP_ERR_INVALID_ARG, PsP2P_DM_TAG, "输入了无法处理的空指针 描述%s", esp_err_to_name(ESP_ERR_INVALID_ARG));
    ESP_RETURN_ON_FALSE(producer->identity == PsP2P_DM_IDENTITY_PRODUCER, ESP_ERR_NOT_SUPPORTED, PsP2P_DM_TAG, "非生产者不可使用下架(off)操作 描述%s", esp_err_to_name(ESP_ERR_NOT_SUPPORTED));

    //检索产品并编辑产品链表,同时释放产品资源,并自减当前上架有效产品数量
    if (producer->head_product != NULL && producer->tail_product != NULL) {
        //链表非空
        //检索产品
        PsP2P_DM_product_t* product = producer->head_product;
        while (product)
        {
            if (!strcmp(product->product_name, product_name)) {
                break;
            }
            product = product->next;
        }
        if (!product) {
            ESP_RETURN_ON_FALSE(false, ESP_ERR_NOT_FOUND, PsP2P_DM_TAG, "产品链表是非空链表,但是未找到指定的产品 描述%s", esp_err_to_name(ESP_ERR_NOT_FOUND));
        }

        //编辑产品链表
        product->prev->next = product->next;
        product->next->prev = product->prev;

        //自减当前上架有效产品数量
        producer->put_sum--;

        //释放产品资源
        free(product);
        product = NULL;
    }
    else if (producer->head_product == NULL && producer->tail_product == NULL) {
        ESP_RETURN_ON_FALSE(false, ESP_ERR_NOT_FOUND, PsP2P_DM_TAG, "产品链表是空链表,不可能找到指定的产品 描述%s", esp_err_to_name(ESP_ERR_NOT_FOUND));
    }
    else {
        ESP_RETURN_ON_FALSE(false, ESP_ERR_INVALID_STATE, PsP2P_DM_TAG, "产品链表存在头或尾缺失,维护状态异常  描述%s", esp_err_to_name(ESP_ERR_INVALID_STATE));
    }

    return ESP_OK;
}


/// @brief PsP2P_DM下架所有产品
/// @param producer 生产者句柄
/// @return [ESP_OK 成功]
/// @return [ESP_ERR_NOT_FOUND 产品链表是空链表,不可能找到指定的产品]
/// @return [ESP_ERR_INVALID_ARG 输入了无法处理的空指针]
/// @return [ESP_ERR_INVALID_STATE 产品链表存在头或尾缺失,维护状态异常]
/// @return [ESP_ERR_NOT_SUPPORTED 非生产者不可使用下架所有产品(off_all)操作]
esp_err_t PsP2P_DM_off_all(PsP2P_DM_node_handle_t producer) {
    ESP_RETURN_ON_FALSE(producer, ESP_ERR_INVALID_ARG, PsP2P_DM_TAG, "输入了无法处理的空指针 描述%s", esp_err_to_name(ESP_ERR_INVALID_ARG));
    ESP_RETURN_ON_FALSE(producer->identity == PsP2P_DM_IDENTITY_PRODUCER, ESP_ERR_NOT_SUPPORTED, PsP2P_DM_TAG, "非生产者不可使用下架所有产品(off_all)操作 描述%s", esp_err_to_name(ESP_ERR_NOT_SUPPORTED));

    //检索产品并编辑产品链表,同时释放产品资源,并自减当前上架有效产品数量
    if (producer->head_product != NULL && producer->tail_product != NULL) {
        //链表非空

        PsP2P_DM_product_t* product = producer->head_product;

        //强制设置为空列表,避免其他任务此时遍历而发生意外
        producer->head_product = NULL;
        producer->tail_product = NULL;

        //下架所有产品
        PsP2P_DM_product_t* product_buf = NULL;
        while (product)
        {
            //缓存下一个产品
            product_buf = product->next;

            //释放产品资源
            free(product);
            product = NULL;
            product = product_buf;

            //自减当前上架有效产品数量
            producer->put_sum--;
        }
    }
    else if (producer->head_product == NULL && producer->tail_product == NULL) {
        ESP_RETURN_ON_FALSE(false, ESP_ERR_NOT_FOUND, PsP2P_DM_TAG, "产品链表是空链表,不可能找到指定的产品 描述%s", esp_err_to_name(ESP_ERR_NOT_FOUND));
    }
    else {
        ESP_RETURN_ON_FALSE(false, ESP_ERR_INVALID_STATE, PsP2P_DM_TAG, "产品链表存在头或尾缺失,维护状态异常  描述%s", esp_err_to_name(ESP_ERR_INVALID_STATE));
    }

    return ESP_OK;
}



/// @brief PsP2P_DM购买产品
/// @param consumer 消费者句柄
/// @param producer_name 生产者名
/// @param product_name 产品名
/// @note  产品名一般是产品数据资源source存储对象名,比如把结构体变量的指针作为产品数据资源source上架,那么产品名一般是结构体变量的变量名
/// @note  消费者可以购买产品名相同,但是生产者名不同的产品,同一生产者不可以有完全同名产品,所以消费者不可能买到它们
/// @return [ESP_OK 成功]
/// @return [ESP_ERR_INVALID_ARG 输入了无法处理的空指针]
/// @return [ESP_ERR_NOT_SUPPORTED 非消费者不可使用购买(buy)操作]
/// @return [ESP_ERR_INVALID_STATE 产品被过多消费者购买 / 节点链表为空链表,维护状态异常 / 该产品消费者节点列表存在未成功删除的消费者节点引用,维护状态异常 / 该生产者的产品链表存在头或尾缺失,维护状态异常]
/// @return [ESP_ERR_NOT_FOUND 未找到指定的生产者/找到生产者,但未找到指定的产品]
/// @return [ESP_ERR_NO_MEM 内存不足]
esp_err_t PsP2P_DM_buy(PsP2P_DM_node_handle_t consumer, const char* producer_name, const char* product_name) {
    ESP_RETURN_ON_FALSE(consumer && producer_name && product_name, ESP_ERR_INVALID_ARG, PsP2P_DM_TAG, "输入了无法处理的空指针 描述%s", esp_err_to_name(ESP_ERR_INVALID_ARG));
    ESP_RETURN_ON_FALSE(consumer->identity == PsP2P_DM_IDENTITY_CONSUMER, ESP_ERR_NOT_SUPPORTED, PsP2P_DM_TAG, "非消费者不可使用购买(buy)操作 描述%s", esp_err_to_name(ESP_ERR_NOT_SUPPORTED));
    ESP_RETURN_ON_FALSE(head_node && tail_node, ESP_ERR_INVALID_STATE, PsP2P_DM_TAG, "节点链表为空链表,维护状态异常 描述%s", esp_err_to_name(ESP_ERR_INVALID_STATE));

    //找到目标生产者
    PsP2P_DM_node_t* aim_producer;
    aim_producer = head_node;
    while (true)
    {
        if (!aim_producer) {
            //节点链表遍历结束,未找到指定的生产者
            ESP_RETURN_ON_FALSE(false, ESP_ERR_NOT_FOUND, PsP2P_DM_TAG, "未找到指定的生产者 描述%s", esp_err_to_name(ESP_ERR_NOT_FOUND));
        }
        if (aim_producer->identity == PsP2P_DM_IDENTITY_PRODUCER) {
            if (!strcmp(aim_producer->node_name, producer_name)) {
                break;
            }
        }
        aim_producer = aim_producer->next;
    }

    //找到目标产品
    PsP2P_DM_product_t* aim_product;
    aim_product = aim_producer->head_product;
    while (true)
    {
        if (!aim_product) {
            //产品链表是空链表,未找到指定的产品 / 产品链表遍历结束,未找到指定的产品source
            ESP_RETURN_ON_FALSE(false, ESP_ERR_NOT_FOUND, PsP2P_DM_TAG, "找到生产者,但未找到指定的产品 描述%s", esp_err_to_name(ESP_ERR_NOT_FOUND));
        }
        if (!strcmp(aim_product->product_name, product_name)) {
            break;
        }
        aim_product = aim_product->next;
    }

    //判断当前消费者数据是否过多
    ESP_RETURN_ON_FALSE(aim_product->consumer_sum < PSP2P_DM_PRODUCT_NOW_CONSUMER_QUANTITY_MAX, ESP_ERR_INVALID_STATE,
        PsP2P_DM_TAG, "产品被过多消费者购买 生产者名[%s] 产品名[%s] 当前消费者数量[%d/%d] 描述%s", aim_product->producer_node->node_name, aim_product->product_name,
        aim_product->consumer_sum, PSP2P_DM_PRODUCT_NOW_CONSUMER_QUANTITY_MAX, esp_err_to_name(ESP_ERR_INVALID_STATE));

    PsP2P_DM_product_handle_t buy_product = malloc(sizeof(PsP2P_DM_product_t));//要购买并存储到消费者产品链表的产品
    ESP_RETURN_ON_FALSE(buy_product, ESP_ERR_NO_MEM, PsP2P_DM_TAG, "内存不足 描述%s", esp_err_to_name(ESP_ERR_NO_MEM));
    memset(buy_product, 0, sizeof(PsP2P_DM_product_t));
    buy_product->prev = NULL;
    buy_product->next = NULL;

    //填充数据
    buy_product->product_name = aim_product->product_name;
    buy_product->resource = aim_product->resource;
    buy_product->consumer_node_list = aim_product->consumer_node_list;//消费者只是持有消费者节点列表的引用
    buy_product->consumer_id = -1;//持有的消费者自增id,从0开始,始终分配最小id
    //始终分配最小id
    for (int i = 0;i < PSP2P_DM_PRODUCT_NOW_CONSUMER_QUANTITY_MAX;i++) {
        if (aim_product->consumer_node_list[i] == NULL) {
            buy_product->consumer_id = i;
            break;
        }
    }
    if (buy_product->consumer_id == -1) {
        free(buy_product);
        buy_product = NULL;
        ESP_RETURN_ON_FALSE(false, ESP_ERR_INVALID_STATE,
            PsP2P_DM_TAG, "生产者[%s]的产品[%s]存在异常,其消费者节点列表存在未成功删除的消费者节点引用,维护状态异常 描述%s",
            aim_producer->node_name, aim_product->product_name, esp_err_to_name(ESP_ERR_INVALID_STATE));
    }

    //生产者端的产品消费者记录同步
    aim_product->consumer_node_list[buy_product->consumer_id] = consumer;
    aim_product->consumer_sum++;

    //链表连接
    if (aim_producer->head_product != NULL && aim_producer->tail_product != NULL) {
        //链表非空
        aim_producer->tail_product->next = buy_product;//原尾产品next指向新产品
        buy_product->prev = aim_producer->tail_product;//新产品prev指向原尾产品
        aim_producer->tail_product = buy_product;//现在的尾产品是新产品
    }
    else if (aim_producer->head_product == NULL && aim_producer->tail_product == NULL) {
        aim_producer->head_product = aim_producer->tail_product = buy_product;//空链表,新产品成为头产品和尾产品
    }
    else {
        free(buy_product);
        buy_product = NULL;
        ESP_RETURN_ON_FALSE(false, ESP_ERR_INVALID_STATE,
            PsP2P_DM_TAG, "生产者[%s]产品链表存在头或尾缺失,维护状态异常 描述%s", aim_producer->node_name, esp_err_to_name(ESP_ERR_INVALID_STATE));
    }

    return ESP_OK;
}


/// @brief PsP2P_DM消息发送
/// @param receiver 接收节点
/// @param message 消息
/// @param xTicksToWait 发送超时时间,将传递给xQueueSend
/// @return [ESP_OK 成功]
/// @return [ESP_ERR_INVALID_STATE 消息队列已满,等待后队列仍非空闲,无法完成消息发送]
esp_err_t PsP2P_DM_message_send(PsP2P_DM_node_handle_t receiver, PsP2P_DM_message_t* message, TickType_t xTicksToWait) {
    ESP_RETURN_ON_FALSE(
        xQueueSend(receiver->messageQueue, (void*)&message, xTicksToWait) == pdTRUE,
        ESP_ERR_INVALID_STATE,
        PsP2P_DM_TAG, "接收者节点[%s]消息队列已满,等待后队列仍非空闲,无法完成消息发送 描述%s",
        receiver->node_name, esp_err_to_name(ESP_ERR_INVALID_STATE));
    return ESP_OK;
}




/// @brief PsP2P_DM创建信箱,信箱中消息接收者是生产者(发送者)自己的消费者
/// @param producer 生产者(发送者)
/// @param result 创建结果存储到句柄(句柄变量在外部定义一个即可)
/// @note  信箱使用完后记得调用free()释放信箱资源和消息资源(没有明确附件,它们不由这里申请内存)
/// @note  如果是调用create(包括本函数)或外部malloc等堆内存申请,信箱,消息和附件内存资源都要单独调用相关free释放,否则不需要
/// @return [ESP_OK 成功]
/// @return [ESP_ERR_NOT_FOUND 不需要发送消息,因为符合条件的接收者数量为0]
/// @return [ESP_ERR_NO_MEM 内存不足]
esp_err_t PsP2P_DM_message_box_create_as_receiver_own_consumer(PsP2P_DM_node_handle_t producer, PsP2P_DM_message_box_handle_t* result) {
    PsP2P_DM_product_t* product = NULL;//产品遍历指针

    //获取生产者当前最大总消费者数量,实际消费者数量 <= 最大总消费者数量
    int max_total_consumer_sum = 0;//生产者当前最大总消费者数量
    product = producer->head_product;
    while (product) {
        max_total_consumer_sum += product->consumer_sum;
        product = product->next;
    }

    ESP_RETURN_ON_FALSE(max_total_consumer_sum != 0, ESP_ERR_NOT_FOUND, PsP2P_DM_TAG, "不需要发送消息,因为符合条件的接收者数量为0  描述%s", esp_err_to_name(ESP_ERR_NOT_FOUND));

    //申请存储所有消费者节点句柄的临时列表,列表大小足够存储最大总消费者数量情况下句柄数据
    PsP2P_DM_node_handle_t* all_consumer = malloc(max_total_consumer_sum * sizeof(PsP2P_DM_node_handle_t));
    ESP_RETURN_ON_FALSE(all_consumer, ESP_ERR_NO_MEM, PsP2P_DM_TAG, "内存不足  描述%s", esp_err_to_name(ESP_ERR_NO_MEM));
    memset(all_consumer, 0, max_total_consumer_sum * sizeof(PsP2P_DM_node_handle_t));

    //遍历生产者所有产品的消费者节点列表,提取消费者节点加入临时列表,如果它已经在临时列表,就不要重复加入
    //同时我们便可以得到实际消费者数量,就等于加入次数
    int real_total_consumer_sum = 0;//生产者当前实际总消费者数量
    product = producer->head_product;
    while (product) {
        if (product->consumer_sum != 0) {
            //产品消费者节点列表不是空的
            for (int i = 0;i < product->consumer_sum;i++) {
                //遍历其中每个消费者节点
                PsP2P_DM_node_handle_t consumer = product->consumer_node_list[i];
                if (real_total_consumer_sum != 0) {
                    //当前临时列表已加入一些消费者节点,判断是否已经存在这个消费者节点,如果它已经在临时列表,就不要重复加入
                    bool is_in_list = false;//是否已经存在
                    for (int j = 0;j < real_total_consumer_sum;j++) {
                        if (all_consumer[j] == consumer) {//基于句柄判断
                            is_in_list = true;//存在
                            break;//已经明确存在,没有必要继续判断
                        }
                    }
                    if (is_in_list) {
                        continue;//已经明确存在,不需要任何操作,下一个
                    }
                    else {
                        //可以加入
                        if (consumer) {//防止加入NULL
                            all_consumer[real_total_consumer_sum] = consumer;
                            real_total_consumer_sum++;
                        }
                    }
                }
                else {
                    //当前临时列表没有消费者节点,直接加入,不用担心重复加入
                    if (consumer) {//防止加入NULL
                        all_consumer[0] = consumer;
                        real_total_consumer_sum = 1;
                    }
                }
            }
        }
        product = product->next;
    }

    //申请消息内存资源和消息信箱内存资源,都要提供给外部,这里不释放
    PsP2P_DM_message_t* message_list = malloc(real_total_consumer_sum * sizeof(PsP2P_DM_message_t));
    PsP2P_DM_message_box_t* message_box = malloc(sizeof(PsP2P_DM_message_box_t));
    if (!message_list || !message_box) {
        free(message_list);
        free(message_box);
        free(all_consumer);
        message_list = NULL;
        message_box = NULL;
        all_consumer = NULL;
        ESP_LOGE(PsP2P_DM_TAG, "内存不足  描述%s", esp_err_to_name(ESP_ERR_NO_MEM));
        return ESP_ERR_NO_MEM;
    }
    memset(message_list, 0, max_total_consumer_sum * sizeof(PsP2P_DM_message_t));
    memset(message_box, 0, sizeof(PsP2P_DM_message_box_t));

    //根据临时列表,向消息填充接收者
    for (int c = 0;c < real_total_consumer_sum;c++) {
        message_list[c].receiver = all_consumer[c];
    }

    //构建消息信箱
    message_box->message_list = (PsP2P_DM_message_list_t)message_list;
    message_box->total_message_amount = real_total_consumer_sum;

    //释放临时缓存
    free(all_consumer);
    all_consumer = NULL;

    //填充结果
    *result = (PsP2P_DM_message_box_handle_t)message_box;
    return ESP_OK;
}