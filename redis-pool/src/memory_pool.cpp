#include "memory_pool.h"
#include "util.h"

#include <stdlib.h>

#define SMALL_BLOCK_SIZE           (uint32_t)(1024 * 128)
#define MEDIUM_BLOCK_SIZE          (uint32_t)(1024 * 1024 * 8)
#define MEDIUM_BLOCK_SIZE_DELTA    (uint32_t)(MEDIUM_BLOCK_SIZE * 2)

#define SMALL_BLOCK_COUNT_DELTA    128
#define BIG_BLOCK_COUNT_DELTA        1

#define MAX_POOL_SIZE   (1024U * 1024 * 1024 * 2)

#define INVALID_BLOCK_SIZE UINT_MAX

int memory_pool_t::init(uint32_t max_block_count)
{
    if (-1 == m_elements.init(max_block_count))
    {
        return -1;
    }

    for (int i = 0; i < POOL_BUCKET_COUNT; ++ i)
    {
        m_buckets[i].block_size = (1 << (10 + i));
        m_buckets[i].free_block_count = 0;
        m_buckets[i].total_block_count = 0;
        m_buckets[i].first_free = element_set_t<pool_element_t>::END_OF_ELEMENT;
    }

    m_allocated_size = 0;

    return 0;
}

void memory_pool_t::uninit()
{
    for (uint32_t i = 0; i < POOL_BUCKET_COUNT; ++ i)
    {
        for (uint32_t j = m_buckets[i].first_free; j != element_set_t<pool_element_t>::END_OF_ELEMENT; j = m_elements[j].next)
        {
            free(m_elements[j].ptr);
        }
    }

    m_elements.uninit();
}

uint32_t memory_pool_t::get_fit_size(uint32_t block_size, uint32_t *bucket_index)
{
    for (uint32_t i = 0; i < POOL_BUCKET_COUNT; ++ i)
    {
        if (block_size <= m_buckets[i].block_size)
        {
            *bucket_index = i;
            return m_buckets[i].block_size;
        }
    }

    return INVALID_BLOCK_SIZE;
}

uint32_t memory_pool_t::get_block_count_delta(uint32_t block_size)
{
    if (block_size <= SMALL_BLOCK_SIZE)
    {
        return SMALL_BLOCK_COUNT_DELTA;
    }
    else if (block_size <= MEDIUM_BLOCK_SIZE)
    {
        return MEDIUM_BLOCK_SIZE_DELTA / block_size;
    }
    else
    {
        return BIG_BLOCK_COUNT_DELTA;
    }
}

void memory_pool_t::recycle_certain(uint32_t bucket_index, uint32_t recycle_threshold)
{
    uint32_t recycle_count = 0;
    if (m_buckets[bucket_index].block_size * m_buckets[bucket_index].free_block_count > SMALL_BLOCK_SIZE * 2)
    {
        if (m_buckets[bucket_index].block_size > MEDIUM_BLOCK_SIZE)
        {
            recycle_count = (m_buckets[bucket_index].free_block_count + 1) / 2;
        }
        else
        {
            if (m_buckets[bucket_index].free_block_count > m_buckets[bucket_index].total_block_count / 2
                || m_buckets[bucket_index].block_size * m_buckets[bucket_index].free_block_count > recycle_threshold)
            {
                recycle_count = m_buckets[bucket_index].free_block_count / 2;
            }
        }
    }

    for (uint32_t j = 0; j < recycle_count && m_buckets[bucket_index].first_free != element_set_t<pool_element_t>::END_OF_ELEMENT; ++ j)
    {
        uint32_t next = m_elements[m_buckets[bucket_index].first_free].next;
        free(m_elements[m_buckets[bucket_index].first_free].ptr);
        m_elements.put_elem(m_buckets[bucket_index].first_free);
        m_buckets[bucket_index].first_free = next;

        -- m_buckets[bucket_index].total_block_count;
        -- m_buckets[bucket_index].free_block_count;

        m_allocated_size -= m_buckets[bucket_index].block_size;
    }
}

int memory_pool_t::expand_pool(uint32_t block_size, uint32_t bucket_index)
{
    uint32_t block_count_delta = get_block_count_delta(block_size);
    uint32_t pool_size_delta = block_size * block_count_delta;
    if (m_allocated_size + pool_size_delta > MAX_POOL_SIZE)
    {
        for (uint32_t i = 0; i < POOL_BUCKET_COUNT; ++ i)
        {
            recycle_certain(i, MEDIUM_BLOCK_SIZE_DELTA);
        }
    }

    if (m_allocated_size + pool_size_delta > MAX_POOL_SIZE)
    {
        return -1;
    }

    for (uint32_t i = 0; i < block_count_delta; ++ i)
    {
        void *ptr = malloc(m_buckets[bucket_index].block_size);
        if (!ptr)
        {
            if (!i)
            {
                return -1;
            }
            else
            {
                break;
            }
        }

        uint32_t elem = m_elements.get_elem();
        if (element_set_t<pool_element_t>::END_OF_ELEMENT == elem)
        {
            free(ptr);
            if (!i)
            {
                return -1;
            }
            else
            {
                break;
            }
        }

        m_elements[elem].ptr = ptr;
        m_elements[elem].next = m_buckets[bucket_index].first_free;

        m_buckets[bucket_index].first_free = elem;

        ++ m_buckets[bucket_index].total_block_count;
        ++ m_buckets[bucket_index].free_block_count;

        m_allocated_size += m_buckets[bucket_index].block_size;
    }

    return 0;
}

void* memory_pool_t::alloc(uint32_t requested_size, uint32_t *actual_size, uint32_t *cookie)
{
    uint32_t bucket_index;
    uint32_t block_size = get_fit_size(requested_size, &bucket_index);
    if (unlikely(INVALID_BLOCK_SIZE == block_size))
    {
        return NULL;
    }

    pthread_mutex_lock(&m_mutex);

    if (element_set_t<pool_element_t>::END_OF_ELEMENT == m_buckets[bucket_index].first_free)
    {
        if (unlikely(-1 == expand_pool(block_size, bucket_index)))
        {
            pthread_mutex_unlock(&m_mutex);
            return NULL;
        }
    }

    void *saved_ptr = m_elements[m_buckets[bucket_index].first_free].ptr;
    *actual_size = block_size;
    *cookie = m_buckets[bucket_index].first_free;

    m_buckets[bucket_index].first_free = m_elements[m_buckets[bucket_index].first_free].next;
    -- m_buckets[bucket_index].free_block_count;

    pthread_mutex_unlock(&m_mutex);
    return saved_ptr;
}

int memory_pool_t::release(uint32_t actual_size, uint32_t cookie)
{
    uint32_t bucket_index;
    if (unlikely(INVALID_BLOCK_SIZE == get_fit_size(actual_size, &bucket_index)))
    {
        return -1;
    }

    pthread_mutex_lock(&m_mutex);

    m_elements[cookie].next = m_buckets[bucket_index].first_free;
    m_buckets[bucket_index].first_free = cookie;

    ++ m_buckets[bucket_index].free_block_count;

    recycle_certain(bucket_index, MAX_POOL_SIZE / 2);

    pthread_mutex_unlock(&m_mutex);
    return 0;
}
