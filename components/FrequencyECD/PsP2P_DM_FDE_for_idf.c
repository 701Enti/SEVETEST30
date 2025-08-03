
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
 // PsP2P_DM_FDE(Pseudo-P2P Data Market Fast Develop Extension)是一个PsP2P_DM的快速开发扩展库 (伪P2P数据市场 - PsP2P_DM(Pseudo-P2P Data Market) 是一个借鉴P2P设计的单机本地数据通信框架)
 // "for_idf"表示此版本为使用ESP-IDF开发平台的设备提供
 // 敬告：本库是一个扩展库,其中代码逻辑为快速开发设计,可能不适用于一些情况,其中存在的一些特殊检查逻辑,参数限制,实现方式等,可能不是PsP2P_DM的原生设定,请注意甄别
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // github: https://github.com/701Enti
 // bilibili: 701Enti


#include "PsP2P_DM_FDE_for_idf.h"
#include "esp_check.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "string.h"

/// @brief 销毁PsP2P_DM生产者
/// @return [ESP_OK 成功]
/// @return [ESP_ERR_INVALID_ARG 输入了无法处理的空指针]
/// @return [ESP_ERR_INVALID_STATE 节点链表为空链表,维护状态异常 / 产品链表存在头或尾缺失,维护状态异常]
/// @return [ESP_ERR_NOT_FINISHED 节点的产品链表不为空链表,请完成相应的产品移除操作再进行销毁]
esp_err_t PsP2P_DM_FDE_Producer_destory(PsP2P_DM_node_handle_t producer) {
    if (producer) {
        if (producer->head_product == NULL && producer->tail_product == NULL) {
            //名下目前没有上架产品,直接销毁即可
            //执行销毁
            esp_err_t ret = PsP2P_DM_node_destory(producer);
            if (ret != ESP_OK) {
                ESP_RETURN_ON_FALSE(false, ret, producer->node_name, "销毁自身失败,执行通用节点销毁PsP2P_DM_node_destory时发现异常  描述%s", esp_err_to_name(ret));
            }
            else {
                producer = NULL;
                ESP_LOGI(producer->node_name, "销毁自身成功");
                return ESP_OK;
            }
        }
        else if (producer->head_product != NULL && producer->tail_product != NULL) {
            //名下有上架产品且未下架
            //实践3.生产者通知清理(需要联络) - 生产者发送附加目标产品的PsP2P_DM_REQUEST_REFUND消息给已经购买产品的消费者,
            //      在所有消费者[退货]后, 将观测到产品的consumer_sum = 0, 生产者立即清理资源, 并设置resource = NULL
            //--注意:在使用快速开发扩展库(PsP2P_DM_FDE)时, 如果发现销毁的节点是生产者且名下有上架产品且未下架时, 
            //       快速开发扩展库(PsP2P_DM_FDE)使用上文垃圾清理实践3清理垃圾并自动完成释放(但是这只是范式模板, 并非PsP2P_DM要求)

            //创建信箱
            PsP2P_DM_message_box_handle_t message_box = NULL;
            PsP2P_DM_message_box_create_as_receiver_own_consumer(producer, message_box);//接收者都是自己的消费者

            //逐个添加内容和附件并发送,发送后阻塞
            //直到该接收者(消费者)完成退货并发送完成退货消息,才会停止阻塞,开始为下一个接收者(消费者)准备消息并发送(最大化减小临时内存消耗)
            //退货消息要包含相同attached(地址判断),否则不是有效的 
            for (int i = 0;i < message_box->total_message_amount;i++) {
                //---消息内容:请退货 
                message_box->message_list[i].content = PsP2P_DM_MESSAGE_CONTENT_PLEASE_REFUND;

                //---消息附件:来自接收者(消费者)产品链表的需退货产品句柄
                //统计附件总大小
                PsP2P_DM_product_t* consumer_product = message_box->message_list[i].receiver->head_product;
                while (consumer_product)
                {
                    PsP2P_DM_product_t* producer_product = producer->head_product;
                    while (producer_product)
                    {
                        if (!strcmp(consumer_product->product_name, producer_product->product_name)) {
                            if (consumer_product->producer_node == producer) {
                                //产品同名且生产者相同,是需退货产品
                                message_box->message_list[i].attached_total_size += sizeof(PsP2P_DM_product_handle_t);//统计附件总大小
                            }
                        }
                        producer_product = producer_product->next;
                    }

                    consumer_product = consumer_product->next;
                }

                //申请附件内存资源
                PsP2P_DM_product_handle_t* product_list = malloc(message_box->message_list[i].attached_total_size);
                if (!product_list) {
                    ESP_LOGE(producer->node_name, "[%s]-X->[%s] 异常:内存不足(描述: %s) 发送失败的内容:请退货(%d) 发送者正在等待接收者完成退货并发送完成消息",
                        producer->node_name, message_box->message_list[i].receiver->node_name, esp_err_to_name(ESP_ERR_NO_MEM), message_box->message_list[i].content);
                    continue;
                }
                memset(product_list, 0, message_box->message_list[i].attached_total_size);

                //再次遍历,填充来自接收者(消费者)产品链表的需退货产品句柄
                int product_index = 0;//填充临时索引
                consumer_product = message_box->message_list[i].receiver->head_product;
                while (consumer_product)
                {
                    PsP2P_DM_product_t* producer_product = producer->head_product;
                    while (producer_product)
                    {
                        if (!strcmp(consumer_product->product_name, producer_product->product_name)) {
                            if (consumer_product->producer_node == producer) {
                                //产品同名且生产者相同,是需退货产品
                                if (product_index * sizeof(PsP2P_DM_product_handle_t) < message_box->message_list[i].attached_total_size) {
                                    product_list[product_index] = consumer_product;//填充来自接收者(消费者)产品链表的需退货产品句柄
                                    product_index++;
                                }
                            }
                        }
                        producer_product = producer_product->next;
                    }

                    consumer_product = consumer_product->next;
                }
                message_box->message_list[i].attached = product_list;

                //填充最后编辑时间
                if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
                    //消息发送前最后一次编辑时的时间(系统微秒级实时时间) - (默认使用-在保证NTP时间同步完成,系统时间与现实时间一致情况下)
                    gettimeofday(&message_box->message_list[i].edit_time_RTC, NULL);
                    message_box->message_list[i].edit_time_is_MT = false;
                }
                else {
                    //消息发送前最后一次编辑时的时间(系统微秒级单调时间) - (备用-在如网络未连接,无法进行NTP时间同步,系统时间与现实时间可能不一致情况下)
                    message_box->message_list[i].edit_time_MT = esp_timer_get_time();
                    message_box->message_list[i].edit_time_is_MT = true;
                }

                //---发送消息:
                esp_err_t send_ret = PsP2P_DM_message_send(message_box->message_list[i].receiver, &(message_box->message_list[i]), portMAX_DELAY);
                if (send_ret != ESP_OK) {
                    free(product_list);
                    product_list = NULL;
                    ESP_LOGE(producer->node_name, "[%s]-X->[%s] 异常:消息队列已满,等待后队列仍非空闲,无法完成消息发送(描述: %s) 发送失败的内容:请退货(%d) 发送者停止尝试通知该接收者完成退货",
                        producer->node_name, message_box->message_list[i].receiver->node_name, esp_err_to_name(send_ret), message_box->message_list[i].content);
                    continue;
                }

                //---等待退货:
                //等待,直到接收到消费者发送的[完成退货消息]
                //(消费者发送的[完成退货消息]包含相同attached(地址相等))
                ESP_LOGI(producer->node_name, "[%s]--->[%s] 内容:请退货(%d) 发送者正在等待接收者完成退货并发送完成消息",
                    producer->node_name, message_box->message_list[i].receiver->node_name, message_box->message_list[i].content);



                //等待 处理中 回应
                int count = PSP2P_DM_NODE_MESSAGE_MAX_ACK_WAIT_TIME_MS / 50;//最多 等待 处理中 回应 阻塞周期数
                while (true) {
                    if (count > 0) {
                        if (message_box->message_list[i].echo == PSP2P_DM_MESSAGE_ECHO_WORKING)break;
                        vTaskDelay(pdMS_TO_TICKS(50));
                        count--;
                        continue;
                    }
                    else {
                        break;
                    }
                }
                if (count <= 0) {//等待 处理中 回应 次数到达上限,已经等待过长时间,也没有得到接收者回应
                    free(product_list);
                    product_list = NULL;
                    ESP_LOGE(producer->node_name, "[%s]WORKING?  ...[%s] 异常:接收者长时间( > %d ms )未回应需求处理中 发送者停止等待该接收者的回应",
                        producer->node_name, message_box->message_list[i].receiver->node_name, PSP2P_DM_NODE_MESSAGE_MAX_ACK_WAIT_TIME_MS);
                    continue;
                }

                //等待结果回应(任何 非 处理中 回应)
                count = PSP2P_DM_NODE_MESSAGE_MAX_RESULT_WAIT_TIME_MS / 50;//最多 等待结果回应 阻塞周期数
                while (true) {
                    if (count > 0) {
                        if (message_box->message_list[i].echo != PSP2P_DM_MESSAGE_ECHO_WORKING)break;
                        vTaskDelay(pdMS_TO_TICKS(50));
                        count--;
                        continue;
                    }
                    else {
                        break;
                    }
                }
                if (count <= 0) {//等待结果回应次数到达上限,已经等待过长时间,也没有得到接收者回应
                    free(product_list);
                    product_list = NULL;
                    ESP_LOGE(producer->node_name, "[%s]RESULT?  ...[%s] 异常:接收者长时间( > %d ms )未回应需求处理结果 发送者停止等待该接收者的回应",
                        producer->node_name, message_box->message_list[i].receiver->node_name, PSP2P_DM_NODE_MESSAGE_MAX_RESULT_WAIT_TIME_MS);
                    continue;
                }
                else {
                    ESP_LOGI(producer->node_name, "[%s]<-RESULT-[%s] 接收者已回应需求处理结果 结果echo(PsP2P_DM_message_echo_t):%d",
                        producer->node_name, message_box->message_list[i].receiver->node_name, message_box->message_list[i].echo);
                }

                //本消费者退货流程正常结束
                if (message_box->message_list[i].echo == PSP2P_DM_MESSAGE_ECHO_WORKED) {
                    free(product_list);
                    product_list = NULL;
                    ESP_LOGI(producer->node_name, "[%s]<-ALL REFUND-[%s] 消费者与本生产者全相关产品退货流程正常结束",
                        producer->node_name, message_box->message_list[i].receiver->node_name);
                    continue;
                }

                free(product_list);
                product_list = NULL;
            }

            //所有消息已经全部发出,信箱使用完毕,释放信箱本身
            free(message_box);
            message_box = NULL;

            //下架所有商品
            PsP2P_DM_off_all(producer);

            //执行销毁
            esp_err_t ret = PsP2P_DM_node_destory(producer);
            if (ret != ESP_OK) {
                ESP_RETURN_ON_FALSE(false, ret, producer->node_name, "销毁自身失败,执行通用节点销毁PsP2P_DM_node_destory时发现异常  描述%s", esp_err_to_name(ret));
            }
            else {
                producer = NULL;
                ESP_LOGI(producer->node_name, "销毁自身成功");
                return ESP_OK;
            }

        }
        else {
            ESP_RETURN_ON_FALSE(false, ESP_ERR_INVALID_STATE, producer->node_name, "产品链表存在头或尾缺失,维护状态异常  描述%s", esp_err_to_name(ESP_ERR_INVALID_STATE));
        }
    }


    return ESP_OK;
}


