#ifndef __FRAME_CLIENT_SET__
#define __FRAME_CLIENT_SET__

#include <stdint.h>
#include <unistd.h>
#include <limits.h>

#include "cache.h"
#include "element_set.h"
//#include "timeout.h"


typedef struct socket_t
    {
        bool valid()
        {
            return (-1 != fd);
        }

        void init(memory_pool_t *recv_mp, memory_pool_t *send_mp)
        {
            fd = -1;
            pending_recv_buf = NULL;
            rc.set_mp(recv_mp);
            sc.set_mp(send_mp);
        }

        void clear()
        {
            if (-1 != fd)
            {
                close(fd);
                fd = -1;
                rc.skip(rc.len());
                sc.skip(sc.len());
                if (pending_recv_buf)
                {
                    free(pending_recv_buf);
                    pending_recv_buf = NULL;
                }
            }
        }


        cache_t rc;
        cache_t sc;
        void *pending_recv_buf;
        uint32_t pending_recv_buf_len;
        uint32_t ip;
        int fd;
        uint16_t port;
    } socket_t;


int reconnect(socket_t *s);

int send_data(socket_t *s, const void *data, uint32_t len);

int send_data_cache(socket_t *s);

int recv_data(socket_t *s);



#endif
