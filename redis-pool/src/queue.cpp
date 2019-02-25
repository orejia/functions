#include "queue.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

#include <stdio.h>

#include <sched.h>

// size must be a power of 2
#define ACTUAL_POS(pos, size) ((pos) & ((size) - 1))

#define ROUND_UP(a, b) (((a) + (b) - 1) / (b) * (b))
#define ROUND_DOWN(a, b) ((a) / (b) * (b))

#define SPIN_COUNT 1024

void queue_t::uninit()
{
    if (m_queue)
    {
        free(m_queue);
        m_queue = NULL;
    }
}

static bool is_power_of_2(uint32_t size)
{
    for (int i = 0; i < 32; ++ i)
    {
        if (size == (1U << i))
        {
            return true;
        }
    }

    return false;
}

int queue_t::init(uint32_t queue_size, void (*on_enqueued)(void *param), void *param)
{
    if (queue_size < 1024 || queue_size > 1024U * 1024 * 1024 * 3 || !is_power_of_2(queue_size))
    {
        return -1;
    }

    m_queue = (char*)malloc(queue_size);
    if (!m_queue)
    {
        return -1;
    }

    m_queue_size = queue_size;

    m_enqueue_pos = 0;
    m_pre_enqueue_pos = 0;
    m_dequeue_pos = 0;
    m_pre_dequeue_pos = 0;

    m_on_enqueued = on_enqueued;
    m_param = param;

    return 0;
}

static void* memcpy_v(void *dest, const void * const *src_v, const uint32_t *len_v, uint32_t count, uint32_t len, uint32_t *begin, uint32_t *offset)
{
    char *dst = (char*)dest;
    uint32_t i = *begin;
    for (; i < count; ++ i)
    {
        uint32_t len_to_copy = (len_v[i] > *offset ? (len_v[i] - *offset) : 0);
        if (unlikely(len_to_copy > len))
        {
            len_to_copy = len;
        }

        if (len_to_copy > 0)
        {
            memcpy(dst, (char*)src_v[i] + *offset, len_to_copy);
            dst += len_to_copy;
            len -= len_to_copy;
            *offset += len_to_copy;
        }

        if (!len)
        {
            break;
        }

        *offset = 0;
    }

    *begin = i;
    return dest;
}

int queue_t::enqueue(const void * const *data_v, const uint32_t *len_v, uint32_t count)
{
    uint32_t len = 0;
    for (uint32_t i = 0; i < count; ++ i)
    {
        if (unlikely(len_v[i] > 1024U * 1024 * 1024))
        {
            return -2;
        }
        len += len_v[i];
        if (unlikely(len > 1024U * 1024 * 1024))
        {
            return -2;
        }
    }

    if (unlikely(!len))
    {
        return -2;
    }

    uint32_t actual_len = ROUND_UP(len, (uint32_t)sizeof(header_t)) + (uint32_t)sizeof(header_t);

    //bool is_empty;
    header_t hdr = {len};
    uint64_t enqueue_pos;
    do
    {
        enqueue_pos = m_pre_enqueue_pos;
        uint64_t dequeue_pos = m_dequeue_pos;
        //is_empty = (enqueue_pos == dequeue_pos);
        //if (unlikely(actual_len > m_queue_size - 1 - (enqueue_pos - dequeue_pos)))
        if (unlikely(actual_len > m_queue_size - (enqueue_pos - dequeue_pos)))
        {
            printf("actual_len = %u, qsize = %u, enqueue_pos = %lu, dequeue_pos = %lu\n",
                actual_len, m_queue_size, enqueue_pos, dequeue_pos);
            return -1;
        }
    }
    while (!((m_pre_enqueue_pos == enqueue_pos) && __sync_bool_compare_and_swap(&m_pre_enqueue_pos, enqueue_pos, enqueue_pos + actual_len)));

    // fill memory
    uint32_t actual_enqueue_pos = (uint32_t)ACTUAL_POS(enqueue_pos, m_queue_size);
    uint32_t actual_enqueue_pos_end = (uint32_t)ACTUAL_POS(enqueue_pos + actual_len, m_queue_size);
    if (likely(actual_enqueue_pos_end > actual_enqueue_pos))
    {
        *(header_t*)&m_queue[actual_enqueue_pos] = hdr;
        uint32_t begin = 0, offset = 0;
        memcpy_v(&m_queue[actual_enqueue_pos + (uint32_t)sizeof(header_t)], data_v, len_v, count, len, &begin, &offset);
    }
    else
    {
        *(header_t*)&m_queue[actual_enqueue_pos] = hdr;
        actual_enqueue_pos += (uint32_t)sizeof(header_t);

        uint32_t begin = 0, offset = 0;
        if (likely(actual_enqueue_pos < m_queue_size))
        {
            uint32_t len_to_copy = m_queue_size - actual_enqueue_pos;
            if (likely(len_to_copy > len))
            {
                len_to_copy = len;
            }
            memcpy_v(&m_queue[actual_enqueue_pos], data_v, len_v, count, len_to_copy, &begin, &offset);
            len -= len_to_copy;
        }

        if (unlikely(len))
        {
            memcpy_v(m_queue, data_v, len_v, count, len, &begin, &offset);
        }
    }

    for (;;)
    {
        if ((m_enqueue_pos == enqueue_pos) && __sync_bool_compare_and_swap(&m_enqueue_pos, enqueue_pos, enqueue_pos + actual_len))
        {
            goto __OUT;
        }

        for (uint32_t i = 1; i <= SPIN_COUNT; i <<= 1)
        {
            for (uint32_t j = 0; j < i; ++ j)
            {
                __asm__ __volatile__("pause");
            }

            if ((m_enqueue_pos == enqueue_pos) && __sync_bool_compare_and_swap(&m_enqueue_pos, enqueue_pos, enqueue_pos + actual_len))
            {
                goto __OUT;
            }
        }

        sched_yield();
    }

__OUT:
    if (m_on_enqueued)
    {
        m_on_enqueued(m_param);
    }

    return 0;
}

void queue_t::free_data(void *data)
{
    free(data);
}

int queue_t::dequeue(void *data, void **ptr, uint32_t *len)
{
    header_t hdr;
    uint64_t dequeue_pos;
    uint32_t actual_len;
    uint32_t actual_dequeue_pos;
    do
    {
        dequeue_pos = m_pre_dequeue_pos;
        if (unlikely(dequeue_pos == m_enqueue_pos))
        {
            return -1;
        }

        actual_dequeue_pos = (uint32_t)ACTUAL_POS(dequeue_pos, m_queue_size);
        hdr = *(header_t*)&m_queue[actual_dequeue_pos];
        actual_len = ROUND_UP(hdr.len, (uint32_t)sizeof(header_t)) + (uint32_t)sizeof(header_t);
    }
    while (!((m_pre_dequeue_pos == dequeue_pos) && __sync_bool_compare_and_swap(&m_pre_dequeue_pos, dequeue_pos, dequeue_pos + actual_len)));

    uint32_t actual_dequeue_pos_end;

    int return_code = 0;
    if (unlikely(*len < hdr.len))
    {
        data = malloc(hdr.len);
        if (unlikely(!data))
        {
            // the packet will be lost, since the operation can't be rolled back
            *len = hdr.len;

            return_code = -2;
            goto __RETURN;
        }
    }

    *ptr = data;
    *len = hdr.len;
    // read memory
    actual_dequeue_pos_end = (uint32_t)ACTUAL_POS(dequeue_pos + actual_len, m_queue_size);
    if (likely(actual_dequeue_pos_end > actual_dequeue_pos))
    {
        memcpy(data, &m_queue[actual_dequeue_pos + (uint32_t)sizeof(header_t)], hdr.len);
    }
    else
    {
        actual_dequeue_pos += (uint32_t)sizeof(header_t);

        uint32_t len_to_copy = 0;
        if (likely(actual_dequeue_pos < m_queue_size))
        {
            len_to_copy = m_queue_size - actual_dequeue_pos;
            if (likely(len_to_copy > hdr.len))
            {
                len_to_copy = hdr.len;
            }
            memcpy(data, &m_queue[actual_dequeue_pos], len_to_copy);
            hdr.len -= len_to_copy;
        }

        if (unlikely(hdr.len))
        {
            memcpy((char*)data + len_to_copy, m_queue, hdr.len);
        }
    }

__RETURN:
    for (;;)
    {
        if ((m_dequeue_pos == dequeue_pos) && __sync_bool_compare_and_swap(&m_dequeue_pos, dequeue_pos, dequeue_pos + actual_len))
        {
            return return_code;
        }

        for (uint32_t i = 1; i <= SPIN_COUNT; i <<= 1)
        {
            for (uint32_t j = 0; j < i; ++ j)
            {
                __asm__ __volatile__("pause");
            }

            if ((m_dequeue_pos == dequeue_pos) && __sync_bool_compare_and_swap(&m_dequeue_pos, dequeue_pos, dequeue_pos + actual_len))
            {
                return return_code;
            }
        }

        sched_yield();
    }
}
