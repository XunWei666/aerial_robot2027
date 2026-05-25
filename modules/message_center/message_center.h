/**
 * @file    message_center.h
 * @author  Xun Wei
 * @brief   仅保留每个话题最新值的消息中心对外接口声明
 * @date    2026-04-06
 *
 * @note    该文件属于 module 层通用基础设施，用于在不同应用或模块之间传递“只关心最新值”的
 *          状态消息与控制消息。与传统 FIFO 队列不同，本消息中心对每个话题仅保存一份最新数据，
 *          订阅者通过版本号判断是否存在尚未读取的新消息。
 */

#ifndef MESSAGE_CENTER_H
#define MESSAGE_CENTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_TOPIC_NAME_LEN 32u   /**< 话题名称允许的最大有效字符数，不包含结尾的 `\0` */
#define MAX_TOPIC_COUNT 12u      /**< 最多允许注册的话题数量，超出后进入死循环保护 */
#define MAX_SUBSCRIBER_COUNT 24u /**< 最多允许注册的订阅者数量，超出后进入死循环保护 */
#define MAX_DATA_LEN 64u         /**< 单个话题允许的最大数据长度，单位为字节 */

typedef struct Publisher Publisher_t;
typedef struct Subscriber Subscriber_t;

/**
 * 话题运行时统计信息。
 *
 * @note  该结构体用于把某个话题当前的运行状态导出给调试逻辑或监视模块，
 *        便于判断发布频率、消息覆盖情况以及订阅关系是否符合预期。
 */
typedef struct
{
    uint32_t publish_count;    /**< 该话题累计成功发布的次数 */
    uint32_t overwrite_count;  /**< 上一份消息尚未被全部相关订阅者读取时，就被新消息覆盖的次数 */
    uint32_t subscriber_count; /**< 当前挂接到该话题的订阅者数量 */
    uint32_t version;          /**< 当前最新消息的版本号，每成功发布一次递增一次 */
    uint8_t has_message;       /**< 当前是否已经至少存在一份有效消息，0 表示无，1 表示有 */
    uint8_t data_len;          /**< 该话题的数据长度，单位为字节 */
} MessageCenterTopicStats_t;

/**
 * 订阅者运行时统计信息。
 *
 * @note  该结构体用于观察某个订阅者的收消息情况，尤其适合在任务频率调整时检查
 *        是否出现“生产速度高于消费速度”导致的漏读更新问题。
 */
typedef struct
{
    uint32_t last_seen_version;   /**< 订阅者最近一次成功读取到的话题版本号 */
    uint32_t received_count;      /**< 订阅者成功读取到新消息的累计次数 */
    uint32_t missed_update_count; /**< 两次成功读取之间被跳过的更新次数 */
    uint8_t has_new_message;      /**< 当前是否存在尚未读取的新消息，0 表示无，1 表示有 */
} MessageCenterSubscriberStats_t;

/**
 * 消息中心中的话题对象。
 *
 * @note  每个话题只维护一份最新数据缓存。发布者写入新数据时会直接覆盖旧数据，
 *        同时把版本号加一；订阅者不保存自己的数据副本，而是只记录自己读取到了哪个版本。
 */
typedef struct Publisher
{
    char topic_name[MAX_TOPIC_NAME_LEN + 1u]; /**< 以 `\0` 结尾的话题名称 */
    uint8_t data_len;                         /**< 该话题数据长度，单位为字节 */
    uint8_t has_message;                      /**< 当前是否保存着一份有效消息 */
    uint16_t subscriber_count;                /**< 当前订阅该话题的订阅者数量 */
    uint32_t version;                         /**< 每次成功发布都会递增的版本号 */
    uint32_t publish_count;                   /**< 成功发布总次数 */
    uint32_t overwrite_count;                 /**< 未读数据被覆盖的累计次数 */
    uint8_t latest_data[MAX_DATA_LEN];        /**< 用于保存该话题最新消息的缓冲区 */
    Subscriber_t *first_subscriber;           /**< 该话题订阅者单向链表的头指针 */
} Publisher_t;

/**
 * 消息中心中的订阅者对象。
 *
 * @note  订阅者本身不保存消息实体，只保存对目标话题的引用以及最近一次已经消费到的版本号。
 */
typedef struct Subscriber
{
    Publisher_t *topic;              /**< 当前订阅的话题对象 */
    Subscriber_t *next_subscriber;   /**< 指向同一话题下一个订阅者的指针 */
    uint32_t last_seen_version;      /**< 最近一次成功读取到的话题版本号 */
    uint32_t received_count;         /**< 成功读取新消息的累计次数 */
    uint32_t missed_update_count;    /**< 累计遗漏的话题更新次数 */
} Subscriber_t;

void MessageCenterInit(void);

void MessageCenterFreezeRegistry(void);

uint8_t MessageCenterRegistryFrozen(void);

Publisher_t *PubRegister(const char *name, uint8_t data_len);

Subscriber_t *SubRegister(const char *name, uint8_t data_len);

uint8_t PubPushMessage(Publisher_t *pub, const void *data_ptr);

uint8_t SubGetMessage(Subscriber_t *sub, void *data_ptr);

uint8_t SubPeekMessage(Subscriber_t *sub, void *data_ptr);

uint8_t SubHasNewMessage(Subscriber_t *sub);

uint8_t PubGetTopicStats(const Publisher_t *pub, MessageCenterTopicStats_t *stats);

uint8_t SubGetSubscriberStats(const Subscriber_t *sub, MessageCenterSubscriberStats_t *stats);

uint16_t MessageCenterTopicCount(void);

uint16_t MessageCenterSubscriberCount(void);

#ifdef __cplusplus
}
#endif

#endif
