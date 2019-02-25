#ifndef __COMMON_QUEUE__
#define __COMMON_QUEUE__

#include <stdint.h>

#ifndef NULL
    #ifdef __cplusplus
        #define NULL 0
    #else
        #define NULL ((void*)0)
    #endif
#endif

// 4:10 threads
// Io 3.0
// iO 5.5
// io 4.2
// IO 3.3

#pragma pack(push, 8)
class queue_t
{
public:
    queue_t() : m_queue(NULL)
    {
    }

    int init(uint32_t queue_size, void (*on_enqueued)(void *param) = NULL, void *param = NULL);
    void uninit();
    int enqueue(const void * const *data_v, const uint32_t *len_v, uint32_t count = 1);
    int dequeue(void *data, void **ptr, uint32_t *len);
    void free_data(void *data);

private:
    typedef struct header_t
    {
        uint32_t len;
        uint32_t __padding;
    } header_t;

    char *m_queue;
    void (*m_on_enqueued)(void *param);
    void *m_param;
    volatile uint64_t m_enqueue_pos;
    volatile uint64_t m_pre_enqueue_pos;
    volatile uint64_t m_dequeue_pos;
    volatile uint64_t m_pre_dequeue_pos;
    uint32_t m_queue_size;
};
#pragma pack(pop)

#endif
