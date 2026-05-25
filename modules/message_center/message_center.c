/**
 * @file    message_center.c
 * @author  Xun Wei
 * @brief   仅保留每个话题最新值的消息中心实现
 * @date    2026-04-06
 *
 * @note    该文件负责完成最新值消息中心的初始化、话题注册、订阅者注册、消息发布、
 *          消息读取以及运行时统计信息导出。当前实现采用静态池 + 版本号的方式管理消息，
 *          不依赖堆内存，也不维护历史队列。
 */

#include "message_center.h"

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "bsp_log.h"

/**
 * 消息中心的全局内部状态。
 *
 * @note  该结构体汇总保存整个消息中心的静态对象池、当前使用量以及注册冻结状态，
 *        属于模块内部实现细节，不对外暴露。
 */
typedef struct
{
    Publisher_t topics[MAX_TOPIC_COUNT];         /**< 静态话题对象池 */
    Subscriber_t subscribers[MAX_SUBSCRIBER_COUNT]; /**< 静态订阅者对象池 */
    uint16_t topic_count;                        /**< 当前已分配的话题数量 */
    uint16_t subscriber_count;                   /**< 当前已分配的订阅者数量 */
    uint8_t registry_frozen;                     /**< 注册阶段是否已经关闭 */
    uint8_t initialized;                         /**< 消息中心是否已经完成初始化 */
} MessageCenter_t;

static MessageCenter_t g_mc; /**< 消息中心单例对象 */

/**
 * @brief 进入消息中心内部临界区
 * @retval 无
 * @note  当前实现直接使用 FreeRTOS 的 `taskENTER_CRITICAL()`，
 *        目的是保护静态池与话题状态在多任务环境下的一致性。
 */
static void MCEnterCritical(void)
{
    taskENTER_CRITICAL();
}

/**
 * @brief 离开消息中心内部临界区
 * @retval 无
 * @note  该函数与 `MCEnterCritical()` 配对使用。
 */
static void MCExitCritical(void)
{
    taskEXIT_CRITICAL();
}

/**
 * @brief 记录致命错误并停机
 * @param msg 以 `\0` 结尾的错误描述字符串
 * @retval 无
 * @note  对于消息中心这类基础设施模块，当前实现选择“记录错误并进入死循环保护”，
 *        以避免系统在资源或配置错误条件下继续以不确定状态运行。
 */
static void MCFatal(const char *msg)
{
    LOGERROR("[message_center] %s", msg);
    while (1)
    {
    }
}

/**
 * @brief 检查话题名称是否合法
 * @param name 以 `\0` 结尾的话题名称
 * @retval 无
 * @note  当前规则要求名称不能为空，且长度必须位于 1 到 `MAX_TOPIC_NAME_LEN` 之间。
 */
static void MCCheckName(const char *name)
{
    size_t name_len;

    if (name == NULL)
    {
        MCFatal("话题名称为空");
    }

    name_len = strnlen(name, MAX_TOPIC_NAME_LEN + 1u);
    if ((name_len == 0u) || (name_len > MAX_TOPIC_NAME_LEN))
    {
        MCFatal("话题名称长度非法");
    }

    /* 这里把名称检查集中收口，目的是让后续注册逻辑默认运行在“输入已经合法”的前提下，
       避免在多个路径重复散落判空和判长代码。 */
}

/**
 * @brief 检查话题数据长度是否合法
 * @param data_len 数据长度，单位为字节
 * @retval 无
 * @note  当前实现要求数据长度必须大于 0，且不能超过 `MAX_DATA_LEN`。
 */
static void MCCheckDataLen(uint8_t data_len)
{
    if ((data_len == 0u) || (data_len > MAX_DATA_LEN))
    {
        MCFatal("话题数据长度非法");
    }
}

/**
 * @brief 按名称查找已注册的话题
 * @param name 以 `\0` 结尾的话题名称
 * @retval 找到时返回对应话题指针，否则返回 NULL
 * @note  当前实现采用顺序遍历静态话题池的方式查找，考虑到话题数量上限较小，
 *        该复杂度在现阶段是可以接受的。
 */
static Publisher_t *MCFindTopicByName(const char *name)
{
    uint16_t i;

    /* 话题数量上限很小，当前直接顺序查找比引入额外索引结构更简单，
       对实时性和可维护性来说是更合适的折中。 */
    for (i = 0u; i < g_mc.topic_count; ++i)
    {
        if (strncmp(g_mc.topics[i].topic_name, name, MAX_TOPIC_NAME_LEN) == 0)
        {
            return &g_mc.topics[i];
        }
    }

    return NULL;
}

/**
 * @brief 从静态话题池中分配一个话题对象
 * @retval 返回新分配的话题对象指针
 * @note  若话题池已经耗尽，当前实现会进入死循环保护。
 */
static Publisher_t *MCAllocTopic(void)
{
    Publisher_t *topic;

    if (g_mc.topic_count >= MAX_TOPIC_COUNT)
    {
        MCFatal("话题池耗尽");
    }

    topic = &g_mc.topics[g_mc.topic_count++];
    memset(topic, 0, sizeof(*topic));
    /* 分配后立即清零，保证后续逻辑不依赖未定义初值，
       也让“尚未收到消息”的默认状态天然成立。 */
    return topic;
}

/**
 * @brief 从静态订阅者池中分配一个订阅者对象
 * @retval 返回新分配的订阅者对象指针
 * @note  若订阅者池已经耗尽，当前实现会进入死循环保护。
 */
static Subscriber_t *MCAllocSubscriber(void)
{
    Subscriber_t *subscriber;

    if (g_mc.subscriber_count >= MAX_SUBSCRIBER_COUNT)
    {
        MCFatal("订阅者池耗尽");
    }

    subscriber = &g_mc.subscribers[g_mc.subscriber_count++];
    memset(subscriber, 0, sizeof(*subscriber));
    /* 订阅者同样采用清零初始化，这样首次读取版本号默认为 0，
       后面就可以直接用版本差判断是否读到过更新。 */
    return subscriber;
}

/**
 * @brief 在已进入临界区的前提下，注册或获取话题对象
 * @param name 以 `\0` 结尾的话题名称
 * @param data_len 话题数据长度，单位为字节
 * @retval 返回对应的话题对象指针
 * @note  若同名话题已存在，则会校验数据长度一致性；若不存在则新建话题。
 */
static Publisher_t *MCRegisterTopicInternal(const char *name, uint8_t data_len)
{
    Publisher_t *topic;
    size_t name_len;

    topic = MCFindTopicByName(name);
    if (topic != NULL)
    {
        /* 同名话题被视为同一条消息通道，因此必须强制要求数据长度一致；
           否则不同模块会按不同结构解释同一块数据，风险比直接报错更大。 */
        if (topic->data_len != data_len)
        {
            MCFatal("同名话题的数据长度不一致");
        }
        return topic;
    }

    if (g_mc.registry_frozen != 0u)
    {
        MCFatal("注册表已冻结");
    }

    topic = MCAllocTopic();
    name_len = strnlen(name, MAX_TOPIC_NAME_LEN);
    memcpy(topic->topic_name, name, name_len);
    topic->topic_name[name_len] = '\0';
    topic->data_len = data_len;
    /* 话题对象一旦创建后结构就固定下来，后面只修改运行态数据，
       这样可以配合“冻结注册表”把系统切分成初始化期和运行期。 */
    return topic;
}

/**
 * @brief 若消息中心尚未初始化，则执行惰性初始化
 * @retval 无
 * @note  该函数用于容忍上层遗漏显式初始化的情况，但仍建议系统初始化阶段主动调用
 *        `MessageCenterInit()`，以保证初始化时序清晰可控。
 */
static void MCEnsureInitialized(void)
{
    if (g_mc.initialized == 0u)
    {
        memset(&g_mc, 0, sizeof(g_mc));
        g_mc.initialized = 1u;
        /* 保留惰性初始化是为了提高容错性，防止上层漏调显式初始化时直接踩野状态；
           但系统设计上仍然建议在启动阶段主动调用初始化接口。 */
    }
}

/**
 * @brief 将消息中心重置为空状态
 * @retval 无
 * @note  该函数会清空所有话题、订阅者、统计信息以及注册状态。
 *        通常应在系统初始化阶段、开始注册话题之前调用。
 */
void MessageCenterInit(void)
{
    MCEnterCritical();
    memset(&g_mc, 0, sizeof(g_mc));
    g_mc.initialized = 1u;
    MCExitCritical();
}

/**
 * @brief 冻结注册表，禁止后续继续注册新话题或新订阅者
 * @retval 无
 * @note  该接口用于强制约束“所有注册行为只允许发生在初始化阶段”。
 *        调用后若仍尝试注册新对象，当前实现会进入死循环保护。
 */
void MessageCenterFreezeRegistry(void)
{
    MCEnterCritical();
    MCEnsureInitialized();
    g_mc.registry_frozen = 1u;
    /* 冻结注册表的目的不是省事，而是明确禁止运行期动态扩容，
       这样消息中心后续只需要处理收发并发，不必再处理注册期结构变更。 */
    MCExitCritical();
}

/**
 * @brief 查询注册表是否已经被冻结
 * @retval 非零表示已冻结，返回 0 表示仍允许注册
 * @note  该接口适合在系统初始化流程结束前做断言检查，确认是否已经进入运行态。
 */
uint8_t MessageCenterRegistryFrozen(void)
{
    uint8_t frozen;

    MCEnterCritical();
    frozen = g_mc.registry_frozen;
    MCExitCritical();

    return frozen;
}

/**
 * @brief 注册一个可发布的话题，或获取已存在的话题对象
 * @param name 以 `\0` 结尾的话题名称
 * @param data_len 该话题数据长度，单位为字节
 * @retval 返回对应的话题对象指针
 * @note  如果同名话题已经存在，则会校验数据长度是否一致，并直接返回已有对象；
 *        如果不存在，则从静态话题池中分配一个新话题。
 */
Publisher_t *PubRegister(const char *name, uint8_t data_len)
{
    Publisher_t *topic;

    MCCheckName(name);
    MCCheckDataLen(data_len);

    MCEnterCritical();
    MCEnsureInitialized();
    topic = MCRegisterTopicInternal(name, data_len);
    MCExitCritical();

    return topic;
}

/**
 * @brief 为指定话题注册一个订阅者
 * @param name 以 `\0` 结尾的话题名称
 * @param data_len 该话题数据长度，必须与话题定义一致
 * @retval 返回订阅者对象指针
 * @note  如果话题尚不存在，则会先创建话题，再从静态订阅者池中分配订阅者并挂接。
 */
Subscriber_t *SubRegister(const char *name, uint8_t data_len)
{
    Publisher_t *topic;
    Subscriber_t *subscriber;

    MCCheckName(name);
    MCCheckDataLen(data_len);

    MCEnterCritical();
    MCEnsureInitialized();

    topic = MCRegisterTopicInternal(name, data_len);
    subscriber = MCAllocSubscriber();
    subscriber->topic = topic;
    subscriber->last_seen_version = 0u;
    subscriber->next_subscriber = topic->first_subscriber;
    topic->first_subscriber = subscriber;
    topic->subscriber_count++;
    /* 新订阅者采用头插法挂链，原因是这里不关心订阅者顺序，
       头插比尾插少一次遍历，初始化成本更低。 */
    MCExitCritical();

    return subscriber;
}

/**
 * @brief 判断本次发布前，是否存在至少一个订阅者尚未消费当前版本消息
 * @param topic 当前要发布的话题
 * @retval 存在未读订阅者时返回 1，否则返回 0
 * @note  该判断用于在最新值模型下统计“消息覆盖”行为，即上一版本尚未被消费就被新版本顶掉。
 */
static uint8_t MCHasUnreadSubscriber(const Publisher_t *topic)
{
    Subscriber_t *subscriber;

    /* 这里逐个检查订阅者是否落后于当前版本，目的不是阻止发布，
       而是统计“上一版消息还没被消费就被覆盖”的情况，便于调试任务频率是否匹配。 */
    for (subscriber = topic->first_subscriber; subscriber != NULL; subscriber = subscriber->next_subscriber)
    {
        if ((topic->has_message != 0u) && (subscriber->last_seen_version != topic->version))
        {
            return 1u;
        }
    }

    return 0u;
}

/**
 * @brief 向话题发布一份新的最新消息
 * @param pub 由 PubRegister() 返回的话题对象
 * @param data_ptr 指向待发布数据的指针
 * @retval 成功返回 1，参数无效时返回 0
 * @note  新数据会覆盖该话题此前保存的数据，并使版本号加一。
 *        若此前版本尚未被至少一个订阅者读取，则会累计一次覆盖计数。
 */
uint8_t PubPushMessage(Publisher_t *pub, const void *data_ptr)
{
    if ((pub == NULL) || (data_ptr == NULL))
    {
        return 0u;
    }

    MCEnterCritical();

    if (MCHasUnreadSubscriber(pub) != 0u)
    {
        pub->overwrite_count++;
    }

    /* 这里直接覆盖最新缓存而不是入队，是因为该消息中心的目标语义就是“只保留最新值”，
       这样控制类消息不会因为排队而累积时延。 */
    memcpy(pub->latest_data, data_ptr, pub->data_len);
    pub->version++;
    pub->publish_count++;
    pub->has_message = 1u;

    MCExitCritical();
    return 1u;
}

/**
 * @brief 获取订阅者尚未读取的最新消息
 * @param sub 由 SubRegister() 返回的订阅者对象
 * @param data_ptr 用于接收数据的缓冲区指针
 * @retval 读取到新消息时返回 1，否则返回 0
 * @note  当话题版本号比订阅者最近读取版本号更新时，函数会将最新数据复制到
 *        `data_ptr`，并把订阅者状态推进到最新版本。
 */
uint8_t SubGetMessage(Subscriber_t *sub, void *data_ptr)
{
    Publisher_t *topic;
    uint32_t version_delta;

    if ((sub == NULL) || (data_ptr == NULL))
    {
        return 0u;
    }

    topic = sub->topic;
    if (topic == NULL)
    {
        return 0u;
    }

    MCEnterCritical();
    if ((topic->has_message == 0u) || (sub->last_seen_version == topic->version))
    {
        MCExitCritical();
        return 0u;
    }

    /* 先复制数据，再推进版本号，保证调用者一旦拿到返回值 1，
       读到的就是与该版本号对应的完整最新数据。 */
    memcpy(data_ptr, topic->latest_data, topic->data_len);
    version_delta = topic->version - sub->last_seen_version;
    if ((sub->last_seen_version != 0u) && (version_delta > 1u))
    {
        /* 这里累计遗漏次数而不尝试补发旧消息，是因为本模块设计目标本来就不是历史重放，
           而是帮助上层发现“消费速度跟不上发布速度”的问题。 */
        sub->missed_update_count += (version_delta - 1u);
    }
    sub->last_seen_version = topic->version;
    sub->received_count++;
    MCExitCritical();
    return 1u;
}

/**
 * @brief 查看当前话题保存的最新消息，但不更新订阅者读取状态
 * @param sub 由 SubRegister() 返回的订阅者对象
 * @param data_ptr 用于接收数据的缓冲区指针
 * @retval 当前话题存在有效消息时返回 1，否则返回 0
 * @note  该接口适合调试观察或非消费式读取场景，不会改变 `last_seen_version`。
 */
uint8_t SubPeekMessage(Subscriber_t *sub, void *data_ptr)
{
    Publisher_t *topic;

    if ((sub == NULL) || (data_ptr == NULL))
    {
        return 0u;
    }

    topic = sub->topic;
    if (topic == NULL)
    {
        return 0u;
    }

    MCEnterCritical();
    if (topic->has_message == 0u)
    {
        MCExitCritical();
        return 0u;
    }

    /* Peek 只读不消费，适合调试观察或某些需要旁路查看状态的场景，
       因而刻意不修改订阅者的 last_seen_version。 */
    memcpy(data_ptr, topic->latest_data, topic->data_len);
    MCExitCritical();
    return 1u;
}

/**
 * @brief 判断订阅者当前是否存在未读取的新消息
 * @param sub 由 SubRegister() 返回的订阅者对象
 * @retval 存在新消息时返回 1，否则返回 0
 * @note  该接口只做状态判断，不会读取消息内容，也不会改变订阅者状态。
 */
uint8_t SubHasNewMessage(Subscriber_t *sub)
{
    Publisher_t *topic;
    uint8_t has_new;

    if (sub == NULL)
    {
        return 0u;
    }

    topic = sub->topic;
    if (topic == NULL)
    {
        return 0u;
    }

    MCEnterCritical();
    has_new = (uint8_t)((topic->has_message != 0u) && (sub->last_seen_version != topic->version));
    MCExitCritical();

    return has_new;
}

/**
 * @brief 获取指定话题的运行时统计信息
 * @param pub 需要查询的话题对象
 * @param stats 用于接收统计结果的结构体指针
 * @retval 成功返回 1，参数无效时返回 0
 * @note  该接口会把当前统计信息拷贝到输出结构体，适合周期性调试或监视任务调用。
 */
uint8_t PubGetTopicStats(const Publisher_t *pub, MessageCenterTopicStats_t *stats)
{
    if ((pub == NULL) || (stats == NULL))
    {
        return 0u;
    }

    MCEnterCritical();
    stats->publish_count = pub->publish_count;
    stats->overwrite_count = pub->overwrite_count;
    stats->subscriber_count = pub->subscriber_count;
    stats->version = pub->version;
    stats->has_message = pub->has_message;
    stats->data_len = pub->data_len;
    MCExitCritical();

    return 1u;
}

/**
 * @brief 获取指定订阅者的运行时统计信息
 * @param sub 需要查询的订阅者对象
 * @param stats 用于接收统计结果的结构体指针
 * @retval 成功返回 1，参数无效时返回 0
 * @note  该接口常用于检查订阅者是否存在明显漏读，以及当前是否还有未消费的新版本。
 */
uint8_t SubGetSubscriberStats(const Subscriber_t *sub, MessageCenterSubscriberStats_t *stats)
{
    Publisher_t *topic;

    if ((sub == NULL) || (stats == NULL))
    {
        return 0u;
    }

    topic = sub->topic;

    MCEnterCritical();
    stats->last_seen_version = sub->last_seen_version;
    stats->received_count = sub->received_count;
    stats->missed_update_count = sub->missed_update_count;
    /* 统计接口直接现算 has_new_message，而不是在订阅者对象里长期缓存一份，
       这样可以减少状态冗余，避免多处维护同一语义。 */
    stats->has_new_message = (uint8_t)((topic != NULL) && (topic->has_message != 0u) &&
                                       (sub->last_seen_version != topic->version));
    MCExitCritical();

    return 1u;
}

/**
 * @brief 获取当前已注册的话题数量
 * @retval 返回当前话题数量
 * @note  该接口仅读取当前静态池使用量，不涉及额外遍历。
 */
uint16_t MessageCenterTopicCount(void)
{
    uint16_t count;

    MCEnterCritical();
    count = g_mc.topic_count;
    MCExitCritical();

    return count;
}

/**
 * @brief 获取当前已注册的订阅者数量
 * @retval 返回当前订阅者数量
 * @note  该接口仅读取当前静态池使用量，不涉及额外遍历。
 */
uint16_t MessageCenterSubscriberCount(void)
{
    uint16_t count;

    MCEnterCritical();
    count = g_mc.subscriber_count;
    MCExitCritical();

    return count;
}
