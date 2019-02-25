#ifndef __FRAME_MEMORY_POOL__
#define __FRAME_MEMORY_POOL__

#include <stdint.h>
#include <limits.h>
#include <pthread.h>

#include "element_set.h"

#ifndef NULL
    #ifdef __cplusplus
        #define NULL 0
    #else
        #define NULL ((void*)0)
    #endif
#endif

#define POOL_BUCKET_COUNT 22

class memory_pool_t
{
public:
    memory_pool_t()
    {
        pthread_mutex_init(&m_mutex, NULL);
    }
    ~memory_pool_t()
    {
        pthread_mutex_destroy(&m_mutex);
    }
    int init(uint32_t max_block_count);
    void uninit();
    void* alloc(uint32_t requested_size, uint32_t *actual_size, uint32_t *cookie);
    int release(uint32_t actual_size, uint32_t cookie);

private:
    typedef struct pool_bucket_t
    {
        uint32_t block_size;
        uint32_t first_free;
        uint32_t free_block_count;
        uint32_t total_block_count;
    } pool_bucket_t;
    typedef struct pool_element_t
    {
        void *ptr;
        uint32_t next;
    } pool_element_t;
    element_set_t<pool_element_t> m_elements;

    uint32_t get_fit_size(uint32_t block_size, uint32_t *bucket_index);
    uint32_t get_block_count_delta(uint32_t block_size);
    int expand_pool(uint32_t block_size, uint32_t bucket_index);
    void recycle_certain(uint32_t bucket_index, uint32_t recycle_threshold);
    pool_bucket_t m_buckets[POOL_BUCKET_COUNT];   // 1KB ~ 2GB
    pthread_mutex_t m_mutex;
    uint32_t m_allocated_size;
};

#endif
