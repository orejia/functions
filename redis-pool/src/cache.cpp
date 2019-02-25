#include "cache.h"

#include "memory_pool.h"
#include "util.h"

#include <string.h>

void cache_t::skip(uint32_t len)
{
    if (unlikely(!total_len))
    {
        return;
    }

    if (len >= data_len)
    {
        mp->release(total_len, cookie);
        offset = 0;
        total_len = 0;
        data_len = 0;
    }
    else
    {
        offset += len;
        data_len -= len;
    }
}

int cache_t::append(const void *data, uint32_t len)
{
    if (unlikely(UINT_MAX - len < data_len))
    {
        return -1;
    }

    if (data_len + len > total_len)
    {
        uint32_t actual_len, new_cookie;
        void *new_ptr = mp->alloc(data_len + len, &actual_len, &new_cookie);
        if (unlikely(!new_ptr))
        {
            return -1;
        }

        if (total_len)
        {
            if (data_len)
            {
                memcpy(new_ptr, (char*)ptr + offset, data_len);
            }

            mp->release(total_len, cookie);
        }

        cookie = new_cookie;
        ptr = new_ptr;
        offset = 0;
        total_len = actual_len;
    }

    if (offset + data_len + len <= total_len)
    {
        memcpy((char*)ptr + offset + data_len, data, len);
    }
    else
    {
        memmove(ptr, (char*)ptr + offset, data_len);
        memcpy((char*)ptr + data_len, data, len);
        offset = 0;
    }

    data_len += len;
    return 0;
}
