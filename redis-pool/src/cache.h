#ifndef __FRAME_CACHE__
#define __FRAME_CACHE__

#include <stdint.h>

class memory_pool_t;

class cache_t
{
public:
    void set_mp(memory_pool_t *memory_pool)
    {
        mp = memory_pool;
    }

    cache_t() : offset(0), total_len(0), data_len(0)
    {
    }

    ~cache_t()
    {
        skip(data_len);
    }

    void skip(uint32_t len);

    int append(const void *data, uint32_t len);

    inline const void* data()
    {
        return (char*)ptr + offset;
    }

    inline uint32_t len()
    {
        return data_len;
    }

private:
    memory_pool_t *mp;
    void *ptr;
    uint32_t offset;
    uint32_t total_len;
    uint32_t data_len;

    uint32_t cookie;
};

#endif
