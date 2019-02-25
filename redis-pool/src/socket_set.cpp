#include "socket_set.h"

#include "memory_pool.h"
#include "queue.h"
#include "util.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <sys/time.h>
#include <sys/epoll.h>
#include <arpa/inet.h>


int reconnect(socket_t *s)
{
    if(!s)
    {
        return -1;
    }

    if(s->valid())
    {
        return 0;
    }

    char ipstr[16];
    ip2str_r(s->ip,ipstr);
    int fd = tcp_connect_server(ip2str_r(s->ip,ipstr),s->port, 1);
    if(fd < 0)
    {
        printf("tcp_connect_server error, ip = %s, port = %d, error:%s\n",ip2str_r(s->ip,ipstr), s->port, strerror(errno));
        s->fd = -1;
        return -1;
    }
    s->fd = fd;
    return 0;
    
}

//
int send_data(socket_t *s, const void *data, uint32_t len)
{
    if(unlikely(!s))
    {
        return -2;
    }

    if(unlikely(!s->valid()))
    {
         return -3;
    }
    
    int result = 0;
    char *remain_data = (char *)data;
    uint32_t remain_len = 0;
    
    if(unlikely(s->sc.len()))
    {
        //first:send data in cache
        __WRITE_CACHE:
        int r = write(s->fd, s->sc.data(), s->sc.len());
        if(unlikely(-1 == r))
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // Resource temporarily unavailable, write data next time
                goto __OUT;
            }
            else if(errno == EINTR)
            {
                goto __WRITE_CACHE;
            }
            else
            {
                //socket error
                s->sc.skip(s->sc.len());
                return -4;
            }
        }
        else
        {
            if(unlikely(r < (int)s->sc.len()))
            {
                //data in cache has not been sent over, 
                s->sc.skip(r);
                goto __OUT;
            }
            else
            {
                //data in cache has been sent over, send data that pass from param
            __WRITE1:
                r = write(s->fd, data, len);
                if(unlikely(-1 == r))
                {
                    if(errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        goto __OUT;
                    }
                    else if(errno == EINTR)
                    {
                        goto __WRITE1;
                    }
                    else
                    {
                        //socket error
                        return -4;
                    }
                }
                else
                {
                    result += r;
                    if(unlikely(r < (int)len))
                    {
                        //add remaining data to cache
                        remain_len = len - r;
                        remain_data += r;
                        goto __OUT;
                    }
                    else
                    {
                        //all data has been sent success!!!
                        return result;
                    }
                }
            }

        }
    }
    else
    {
        //cache has no data, send data that pass by param
    __WRITE2:
        result = write(s->fd, data, len);
        if(unlikely(-1 == result))
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                goto __OUT;
            }
            else if(errno == EINTR)
            {
                goto __WRITE2;
            }
            else
            {
                //socket error
                return -4;
            }
        }
        else
        {
            if(unlikely(result < (int)len))
            {
                remain_len = len - result;
                remain_data += result;
                goto __OUT;
            }
        }
        
    }
    
__OUT:
    if(remain_len > 0)
    if(unlikely(-1 == s->sc.append(remain_data, remain_len)))
    {
        printf("cache has not memroy to append remian data\n");
        return -5;
    }

     return result;
}


//send data that in cache
int send_data_cache(socket_t *s)
{
    if(unlikely(!s))
    {
        return -2;
    }

    if(unlikely(!s->valid()))
    {
        return -3;
    }

    int result = 0;
    if(likely(s->sc.len()))
    {
        //first:send data in cache
        __WRITE:
        int r = write(s->fd, s->sc.data(), s->sc.len());
        if(unlikely(-1 == r))
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // Resource temporarily unavailable, write data next time
                goto __OUT;
            }
            else if(errno == EINTR)
            {
                goto __WRITE;
            }
            else
            {
                //socket error
                s->sc.skip(s->sc.len());
                return -4;
            }
        }
        else
        {
            result += r;
            s->sc.skip(r);
        }
    }
    else
    {
        return 0;
    }

__OUT:
     return result;
}


int recv_data(socket_t *s)
{
    if(unlikely(!s))
    {
        return -3;
    }

    if(unlikely(!s->valid()))
    {
        return -4;
    }
    
    if(unlikely(s->rc.len()))
    {
        //ignore recev data
        s->rc.skip(s->rc.len());
    }

    char buf[1024 *128];
    int n = 0;
__READ:
    int read_cb = read(s->fd, buf, 1024 *128);
    if(unlikely(-1 == read_cb))
    {
        if (EAGAIN == errno || EWOULDBLOCK == errno)
        {
            //no data in Recv-Q
            if (s->rc.len())
            {
                // reidentify
                read_cb = 0;
                goto __OUT;
            }
            return 0;
        }
        else if (EINTR == errno)
        {
            goto __READ;
        }
        else
        {
            char ipstr[16];
            printf("(address %s:%hu, fd %d): Failed to read: %s", ip2str_r(s->ip, ipstr), ntohs(s->port), s->fd, strerror(errno));
            return -4;
        }
    }
    else if(unlikely(!read_cb))
    {
        char ipstr[16];
        printf("(address %s:%hu, fd %d): Connection closed by peer.", ip2str_r(s->ip, ipstr), ntohs(s->port), s->fd);
        return -4;
    }
    else
    {
        n += read_cb;
        if(likely(read_cb < (int)sizeof(buf)))
        {
            // all data in buffer read
            if(-1 == s->rc.append(buf, read_cb))
            {
                return -5;
            }
        }
        else
        {
            if(-1 == s->rc.append(buf, read_cb))
            {
                return -5;
            }
            goto __READ;
        }
    }

__OUT:
    if(s->rc.len())
    {
        s->rc.skip(s->rc.len());
    }

    return n;
        
}

