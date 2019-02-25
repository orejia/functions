#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <sys/epoll.h>
#include "Log.h"

#ifndef EPOLLRDHUP
enum __EPOLL_EVENTS
{
    EPOLLRDHUP = 0x2000,
    #define EPOLLRDHUP EPOLLRDHUP
};
#endif

#include "redis_client.h"

static const int MAX_REDIS_NUM = 100;
static const int MAX_REDIS_DB_CNT = 1;
static const int MAX_VNODE_CNT = 4;

static const int MAX_COMMAND_STACK_LEN = 1024 * 128;

static string genKey1(const std::string& ip, int port, int index, int vnode){
    char buf[256] = {0};

    snprintf(buf, sizeof(buf), "REDIS:%s:%d:%d-VNODE:%d", 
        ip.c_str(),
        port,
        index,
        vnode);
    return buf;
}


class __scoped_lock_t
{
public:
    explicit __scoped_lock_t(pthread_mutex_t *mutex) : m_mutex(mutex)
    {
        pthread_mutex_lock(m_mutex);
    }
    ~__scoped_lock_t()
    {
        pthread_mutex_unlock(m_mutex);
    }

private:
    pthread_mutex_t *m_mutex;
};

class __scoped_spin_rwlock_t
{
public:
    __scoped_spin_rwlock_t(pthread_rwlock_t *lock, bool lock_type) : m_lock(lock), m_lock_type(lock_type)
    {
        if (m_lock_type)
        {
            pthread_rwlock_wrlock(m_lock);
        }
        else
        {
            pthread_rwlock_rdlock(m_lock);
        }
    }
    ~__scoped_spin_rwlock_t()
    {
        if (m_lock_type)
        {
            pthread_rwlock_unlock(m_lock);
        }
        else
        {
            pthread_rwlock_unlock(m_lock);
        }
    }

private:
    pthread_rwlock_t *m_lock;
    bool m_lock_type;
};

class __scoped_spinlock_t
{
public:
    explicit __scoped_spinlock_t(pthread_spinlock_t *lock_)
    {
        lock(lock_);
    }
    ~__scoped_spinlock_t()
    {
        unlock();
    }

    void lock(pthread_spinlock_t *lock)
    {
        m_lock = lock;
        pthread_spin_lock(m_lock);
    }

    void unlock()
    {
        if (m_lock)
        {
            pthread_spin_unlock(m_lock);
            m_lock = NULL;
        }
    }
private:
    pthread_spinlock_t *m_lock;
};


uint32_t get_memory_pool_block_count(uint32_t socket_count)
{
    if (socket_count <= 1000)
    {
        return 3000;
    }
    else if (socket_count <= 50000)
    {
        return 3000 + (socket_count - 1000) * 2;
    }
    else if (socket_count <= 500000)
    {
        return 101000 + (socket_count - 50000);
    }
    else
    {
        return 551000;
    }
}


redis_client_t::redis_client_t()
{
    m_tid = 0;
    m_epoll_fd = -1;
    m_nodes = new redis_connection_t();
    m_cfg = new RedisConf(); 
    pthread_mutex_init(&m_append_mutex, NULL);

}

redis_client_t::~redis_client_t()
{

    if(m_nodes)
    {
        delete m_nodes;
        m_nodes = NULL;
    }

    if(m_evs)
    {
        delete m_evs;
        m_evs = NULL;
    }

    if(m_cfg)
    {
        delete m_cfg;
        m_cfg = NULL;
    }
    pthread_mutex_destroy(&m_append_mutex);

}


/*
redis_client_t* redis_client_t::instance()
{
    if (NULL != g_instance)
    {
        return g_instance;
    }
    else
    {
        g_instance = new redis_client_t();
        return g_instance;
    }
}
*/


RedisConf* redis_client_t::getConf(const std::string& path) 
{
    int ret = 0;
    ret = conf_.parse(path);
    m_cfg = &conf_;

    if (0 != ret) {
        m_cfg = NULL;
    }

    return m_cfg;
}


int redis_client_t::init(const string &filepath)
{
    int ret = 0;

    m_epoll_fd = epoll_create(128);
    {
        if(m_epoll_fd < 0)
        {
            MyErrorLog("failed to epoll_create, error info: %s\n",strerror(errno));
            return -1;
        }
    }
  
    if(-1 == init_node(filepath))
    {
        MyErrorLog("Failed to init_node");
        return -1;
    }

    m_evs = (struct epoll_event*)malloc(sizeof(struct epoll_event) * MAX_REDIS_NUM * MAX_REDIS_DB_CNT);
    if (!m_evs)
    {
        MyErrorLog("Failed to allocate memory for epoll events");
        return -1;
    }

    if(-1 == m_queue.init(1024 * 1024 * 128,NULL,NULL))
    {
        MyErrorLog("queue init failed, \n");
        return -1;
    }

    if (-1 == m_send_memory_pool.init(get_memory_pool_block_count(MAX_REDIS_NUM * MAX_REDIS_DB_CNT)))
    {
        MyErrorLog("Failed to init send memory pool");
        return -1;
    }

    if(-1 == m_recv_memory_pool.init(get_memory_pool_block_count(MAX_REDIS_NUM * MAX_REDIS_DB_CNT)))
    {
        MyErrorLog("Failed to init recv memory pool");
        return -1;
    }

    ret = pthread_create(&m_tid, NULL, dispatch_thread_proc_wrapper, this);
    if (ret)
    {
        MyErrorLog("Failed to create master thread: %s", strerror(errno));
        return -1;
    }

    return 0;

}


//push redis request command to queue, apply to caller
int redis_client_t::append_cmd(const void *key, const char *format, ...)
{
    if(key == NULL)
    {
        return -100;
    }

    /*
    char *cmd = NULL;

    va_list ap;
    int len;
    va_start(ap,format);
    len = redisvFormatCommand(&cmd,format,ap);
    va_end(ap);
    if(len < 0)
    {
        //parse redis request command failed.
        char format_buf[1024 * 16];
        vsprintf(format_buf, format, ap);
        MyErrorLog("redis command format error.key = %s, cmd=%s\n", (char*)key, format_buf);
        return -1;
    }
    
    redis_request_ptr new_ptr(new redis_request_t());
    new_ptr->str_key.assign((char *)key);
    new_ptr->str_cmd.assign((char *)cmd);
    m_req_queue.push(new_ptr);
    free(cmd);*/

    int key_len = (int)strlen((char*)key);
    char *cmd = NULL;

    va_list ap;
    va_start(ap,format);
    int cmd_len = redisvFormatCommand(&cmd,format,ap);
    va_end(ap);
    if(cmd_len < 0)
    {
        //parse redis request command failed.
        char format_buf[1024 * 1024];
        vsprintf(format_buf, format, ap);
        MyErrorLog("redis command format error.key = %s, cmd=%s\n", (char*)key, format_buf);
        return -200;
    }

    queue_packet_header_t hdr;
    hdr.key_len = (uint32_t)key_len;
    hdr.cmd_len = (uint32_t)cmd_len;
    char buf[1024 * 1024];
    memcpy(buf, key, key_len);
    memcpy(buf+key_len, cmd, cmd_len);
    free(cmd);
    const void *data_v[2] = {&hdr, buf};
    uint32_t len_v[2] = {(uint32_t)sizeof(hdr), key_len + cmd_len};
    pthread_mutex_lock(&m_append_mutex);
    int result = m_queue.enqueue(data_v, len_v, 2);
    pthread_mutex_unlock(&m_append_mutex);
    return result;
}

int redis_client_t::init_node(const string &filepath)
{
    m_cfg->parse(filepath);
    typeVecHost hosts = m_cfg->getHosts();
    m_authPasswd = m_cfg->getAuthPasswd();
    MyInfoLog("hosts size = %d, m_authPasswd = %s\n",(int)hosts.size(),m_authPasswd.c_str());
    for(int i=0; i<(int)hosts.size(); i++)
    {
        const HostInfo &h = hosts[i];
        MyErrorLog(":hosts[%d] ip = %s, port = %d, cnt = %d\n", i,  hosts[i].m_ip.c_str(),hosts[i].m_port,hosts[i].m_cnt);
        for(int j = 0; j < h.m_cnt; j++)
        {
            int port_ = h.m_port+j;
            if(add(h.m_ip, port_)<0)
            {
                return -1;
            }
        }
    }

    return 0;
}

socket_t* redis_client_t::get_node(const string &key)
{
   return (socket_t *)m_nodes->getNode(key);
}

//int redis_client_t::put_node(const string &key);

int redis_client_t::add(const string &strip, int port)
{
    //printf("[%s %d]: port = %d\n", __FUNCTION__, __LINE__, port);
    socket_t *s = new socket_t[MAX_REDIS_DB_CNT];
    int ret = 0;
    string key;
    for (int i=0; i<MAX_REDIS_DB_CNT; i++)
    {
        s[i].init(&m_recv_memory_pool, &m_send_memory_pool);
        s[i].ip = strip2int(strip.c_str());
        s[i].port = (uint16_t)port;
        s[i].fd = tcp_connect_server(strip.c_str(), port, true);
        if(s[i].fd < 0)
        {
            MyErrorLog("Failed to connect redis server, ip = %s, port = %d, error.%s\n", strip.c_str(), port, strerror(errno));
            ret = -1;
        }

        struct epoll_event ev;
        //ev.ptr = s;
        ev.data.ptr = s;
        ev.events = EPOLLIN|EPOLLOUT|EPOLLRDHUP|EPOLLET;
        int r = epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, s[i].fd, &ev);
        if(r == -1)
        {
            MyErrorLog("Failed to call epoll_ctl, fd = %d,add event error.%s\n", s[i].fd, strerror(errno));
            close(s[i].fd);
            return -2;
        }
        
        for (int j=0; j<MAX_VNODE_CNT; j++) 
        {
            /*

            char buf[256] = {0};

            snprintf(buf, sizeof(buf), "REDIS:%s:%d:%d-VNODE:%d", 
            strip.c_str(),
            port,
            i,
            j);
            key.assign(buf);
            */
            key = genKey1(strip, port, i, j);
            
            m_nodes->putNode(key, &s[i]);
        }
    }
    
    return ret;
    
}

void* redis_client_t::dispatch_thread_proc_wrapper(void *param)
{
    redis_client_t* const This = (redis_client_t*)param;
    return (void *)This->dispatch_thread_proc();
}



int redis_client_t::dispatch_thread_proc()
{

    int epoll_timeout = 0;
    char *dequeue_buf = (char*)malloc(sizeof(queue_packet_header_t) + DEQUEUE_BUFFER_SIZE);
    if(NULL == dequeue_buf)
    {
        MyErrorLog("malloc memroy for dequeue_buf failed\n");
        return -1;
    }
    while(true)
    {
       epoll_timeout = 5000;
     __REEPOLL:
        process_request_queue(dequeue_buf);  
        int result = epoll_wait(m_epoll_fd, m_evs, MAX_REDIS_NUM * MAX_REDIS_DB_CNT, epoll_timeout);
        if(-1 == result)
        {
            if (EINTR != errno)
            {
                // this case seems to be impossible
                printf("Failed to call epoll_wait: %s", strerror(errno));
                usleep(50000);
            }
            goto __REEPOLL;
        }

        if(result > 0)
        {
            for(int i = 0; i < result; i++)
            {
                bool is_close = false;
                if(m_evs[i].events & EPOLLIN)
                {
                    //read event
                    socket_t *s = (socket_t *)m_evs[i].data.ptr;
                    int recv_cb = recv_data(s);
                    if(unlikely(recv_cb < 0))
                    {
                        is_close = true;
                        goto __CLOSE;
                    }
                    else
                    {
                        //printf("recv %d bytes from peer: %s:%hu\n", recv_cb, ip2str_r(s->ip, ipstr), s->port);
                    }
                    
                }
                else if(m_evs[i].events & EPOLLOUT)
                {
                    //write event
                    socket_t *s = (socket_t *)m_evs[i].data.ptr;
                    if(unlikely(s == NULL))
                    {
                        is_close = true;
                        goto __CLOSE;
                    }
                    
                    int so_error;
                    socklen_t optlen = (socklen_t)sizeof(so_error);
                    if(unlikely(getsockopt(s->fd, SOL_SOCKET, SO_ERROR, &so_error, &optlen)))
                    {
                        char ip_buf[16];
                        printf("(address %s:%hu, fd %d): Failed to call getsockopt: %s", ip2str_r(s->ip, ip_buf), s->port, s->fd, strerror(errno));
                        is_close = true;
                        goto __CLOSE;
                    }
                    else
                    {
                        if (unlikely(so_error))
                        {
                            // error occurred
                            char ip_buf[16];
                            printf("(address %s:%hu, fd %d): Failed to connect, getsockopt reports an error of %d(%s)\n", ip2str_r(s->ip, ip_buf), s->port, s->fd, so_error, strerror(so_error));
                            is_close = true;
                            goto __CLOSE;
                            
                        }
                        else
                        {
                            //connected
                        }
                    }
                    
                    int r = send_data_cache(s);
                    if(r < 0)
                    {
                        if(s->valid())
                        {
                            close(s->fd);
                        }
                        is_close = true;
                        goto __CLOSE;
                    }
                    
                }
                else if(m_evs[i].events & EPOLLRDHUP)
                {
                    //the peer shuts down its TX channel?
                    socket_t *s = (socket_t *)m_evs[i].data.ptr;
                    char ipstr[16];
                    MyErrorLog("EPOLLRDHUP event.peer info[%s:%hu]\n", ip2str_r(s->ip, ipstr),ntohs(s->port));
                    is_close = true;
                    goto __CLOSE;
                }
            __CLOSE:
                if(true == is_close)
                {
                    //
                    char ipstr[16];
                    socket_t *s = (socket_t *)m_evs[i].data.ptr;
                    if(m_evs[i].events & EPOLLIN)
                    {
                        printf("EPOLLIN event\n");
                    }
                    else if(m_evs[i].events & EPOLLOUT)
                    {
                        printf("EPOLLOUT event\n");
                    }
                    else if(m_evs[i].events & EPOLLRDHUP)
                    {
                        printf("EPOLLRDHUP event\n");
                    }
                    else
                    {
                        printf("other event\n");
                    }
                    printf("[%s %d]:close socket, peer info:%s:%hu\n", __FUNCTION__, __LINE__, ip2str_r(s->ip,ipstr), s->port);
                    if(s->valid())
                    {
                        close(s->fd);
                        s->fd = -1;
                        
                    }
                    
                    if(reconnect(s) > 0)
                    {
                        struct epoll_event ev;
                        ev.data.ptr = s;
                        ev.events = EPOLLIN|EPOLLOUT|EPOLLRDHUP|EPOLLERR|EPOLLET;
                        if(epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, s[i].fd, &ev) < 0)
                        {
                            MyErrorLog("[%s %d]:Failed to call epoll_ctl, fd = %d,add event error.%s\n",__FUNCTION__, __LINE__, s[i].fd, strerror(errno));
                        }
                    }
                    else
                    {
                        MyErrorLog("reconnect to redis failed, redis info[%s:%hu]\n", ip2str_r(s->ip,ipstr), s->port);
                    }
                }
                
            }
        }
    }
    
    free(dequeue_buf);
    return 0;
}

/*
int redis_client_t::dispatch_thread_proc()
{

    int epoll_timeout = 0;
    char *dequeue_buf = (char*)malloc(sizeof(queue_packet_header_t) + DEQUEUE_BUFFER_SIZE);
    if(NULL == dequeue_buf)
    {
        MyErrorLog("malloc memroy for dequeue_buf failed\n");
        return -1;
    }
    while(true)
    {
    
        epoll_timeout = 500;
     __REEPOLL:
        
        process_request_queue(dequeue_buf);
        
        int result = epoll_wait(m_epoll_fd, m_evs, MAX_REDIS_NUM * MAX_REDIS_DB_CNT, epoll_timeout);
        if(-1 == result)
        {
            if (EINTR != errno)
            {
                // this case seems to be impossible
                printf("Failed to call epoll_wait: %s", strerror(errno));
                usleep(50000);
            }
            goto __REEPOLL;
        }

        if(result > 0)
        {
            bool is_close = false;
            for(int i = 0; i < result; i++)
            {
                if(m_evs[i].events & EPOLLIN)
                {
                    //read event
                    int n = 0;
                    int read_cb = 0;
                    bool has_data = false;
                    char buf[1024 *128];
                    socket_t *s = (socket_t *)m_evs[i].data.ptr;
                    if(unlikely(s == NULL))
                    {
                        //it seems be impossible
                        continue;
                    }

                    if(unlikely(!s->valid()))
                    {
                        continue;
                    }
                    if(s->rc.len())
                    {
                        //printf("recv cache has data, len = %u\n", s->rc.len());
                        //ignore recev data, do nothing
                        s->rc.skip(s->rc.len());
                    }
                    
                __READ:
                    read_cb = read(s->fd, buf, sizeof(buf));
                    if(likely(read_cb > 0))
                    {
                        has_data = true;
                        n += read_cb;
                        if(likely(read_cb < 1024 *128))
                        {
                            char ipstr[16];
                            //all data read in buf, and ignore recev data, do nothing
                            if(-1 == s->rc.append(buf,read_cb))
                            {
                                printf("append to cache failed. has no memroy,cache data len = %u, read_cb = %d\n", s->rc.len(), read_cb);
                            }
                        }
                        else
                        {
                            //continue read Recv-Q data.
                            if(-1 == s->rc.append(buf,read_cb))
                            {
                                printf("append to cache failed. has no memroy,cache data len = %u, read_cb = %d\n", s->rc.len(), read_cb);
                            }
                            goto __READ;
                        }
                    }
                    else if(read_cb < 0)
                    {
                        
                        if(EINTR == errno)
                        {
                            goto __READ;
                        }
                        else if(EAGAIN==errno || EWOULDBLOCK == errno)
                        {
                            // Resource temporarily unavailable, read data next time
                            continue;
                        }
                        else
                        {
                            //scoket error, need reconnect
                            is_close = true;
                            goto __CLOSE;
                        }
                    }
                    else
                    {
                        if(s->rc.len())
                        {
                            s->rc.skip(s->rc.len());
                        }
                        
                        char ipstr[16];
                        //disconnect , 
                        MyErrorLog("connection fd[%d] closed by peer, peer info: [%s:%hu]\n", s->fd, ip2str_r(s->ip, ipstr), s->port);
                        is_close = true;
                        goto __CLOSE;

                    }
                    
                }
                else if(m_evs[i].events & EPOLLOUT)
                {
                    //write event
                    socket_t *s = (socket_t *)m_evs[i].data.ptr;
                    int r = send_data_cache(s);
                    if(r < 0)
                    {
                        if(s->valid())
                        {
                            close(s->fd);
                        }
                        is_close = true;
                        goto __CLOSE;
                    }
                    
                }
                else if(m_evs[i].events & EPOLLRDHUP)
                {
                    //the peer shuts down its TX channel?
                    socket_t *s = (socket_t *)m_evs[i].data.ptr;
                    char ipstr[16];
                    MyErrorLog("EPOLLRDHUP event.peer info[%s:%hu]\n", ip2str_r(s->ip, ipstr),ntohs(s->port));
                    is_close = true;
                    goto __CLOSE;
                }
            __CLOSE:
                if(true == is_close)
                {
                    //
                    char ipstr[16];
                    socket_t *s = (socket_t *)m_evs[i].data.ptr;
                    if(m_evs[i].events & EPOLLIN)
                    {
                        printf("EPOLLIN event\n");
                    }
                    else if(m_evs[i].events & EPOLLOUT)
                    {
                        printf("EPOLLOUT event\n");
                    }
                    else if(m_evs[i].events & EPOLLRDHUP)
                    {
                        printf("EPOLLRDHUP event\n");
                    }
                    else
                    {
                        printf("other event\n");
                    }
                    printf("[%s %d]:close socket, peer info:%s:%hu\n", __FUNCTION__, __LINE__, ip2str_r(s->ip,ipstr), s->port);
                    if(reconnect(s) > 0)
                    {
                        struct epoll_event ev;
                        ev.data.ptr = s;
                        ev.events = EPOLLIN|EPOLLOUT|EPOLLRDHUP|EPOLLET;
                        if(epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, s[i].fd, &ev) < 0)
                        {
                            MyErrorLog("[%s %d]:Failed to call epoll_ctl, fd = %d,add event error.%s\n",__FUNCTION__, __LINE__, s[i].fd, strerror(errno));
                        }
                    }
                    else
                    {
                        MyErrorLog("reconnect to redis failed, redis info[%s:%hu]\n", ip2str_r(s->ip,ipstr), s->port);
                    }
                }
                
            }
        }
    }
    free(dequeue_buf);
    return 0;
}

*/

typedef struct auto_freer_t
{
    auto_freer_t(queue_t *q, void *buf, void *data) : m_q(q), m_buf(buf), m_data(data) {}
    ~auto_freer_t()
    {
        if (m_data != m_buf)
        {
            m_q->free_data(m_data);
        }
    }
    queue_t *m_q;
    void *m_buf;
    void *m_data;
} auto_freer_t;


void redis_client_t::process_request_queue(char *dequeue_buf)
{
    uint32_t packet_num = 0;
    for (uint32_t total_len = 0; total_len < 1024 * 1024 * 64;)
    {
        void *data;
        uint32_t len = (uint32_t)sizeof(queue_packet_header_t) + DEQUEUE_BUFFER_SIZE;
        int dequeue_result = m_queue.dequeue(dequeue_buf, &data, &len);
        if (dequeue_result < 0)
        {
            if (unlikely(-2 == dequeue_result))
            {
                printf("m_queue dequeue failed: No memory");
            }
            break;
        }
        char *pdata = (char *)data;
        auto_freer_t __(&m_queue, dequeue_buf, data);
        for(uint32_t data_len = 0; data_len < len; )
        {
            const queue_packet_header_t *hdr = (queue_packet_header_t *)pdata;
            uint32_t packet_len = hdr->key_len + hdr->cmd_len + sizeof(queue_packet_header_t);
            
            string key;
            string cmd;
            key.assign(pdata + sizeof(queue_packet_header_t), hdr->key_len);
            cmd.assign((pdata + sizeof(queue_packet_header_t) + hdr->key_len), hdr->cmd_len);
            /*
            printf("len = %u,data_len = %u, hdr->key_len = %u, hdr->cmd_len = %u\n", len,data_len, hdr->key_len, hdr->cmd_len);
            printf("key = %s\n", key.c_str());
            printf("cmd = %s\n", cmd.c_str());
            */
            pdata += packet_len;
            data_len += packet_len;
            packet_num ++;
            socket_t *s = get_node(key);
            if(unlikely(s == NULL))
            {
                //this case seems impossible
                continue;
            }
            else
            {
                char ipstr[16];
                //printf("key = %s, ip = %s, port = %hu\n", key.c_str(), ip2str_r(s->ip, ipstr), s->port);
                if(unlikely(!s->valid()))
                {
                    if(reconnect(s) < 0)
                    {
                        printf("reconnect failed, peer:%s:%hu\n", ip2str_r(s->ip, ipstr), s->port);
                        continue;
                    }
                    else
                    {
                        struct epoll_event ev;
                        ev.data.ptr = s;
                        ev.events = EPOLLIN|EPOLLOUT|EPOLLRDHUP|EPOLLERR|EPOLLET;
                        int r = epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, s->fd, &ev);
                        if(-1 == r)
                        {
                            close(s->fd);
                            s->fd = -1;
                            continue;
                        }
                        else
                        {
                            if(s->rc.len())
                            {
                                s->rc.skip(s->rc.len());
                            }
                            
                            if(s->sc.len())
                            {
                                s->sc.skip(s->sc.len());
                            }
                        }
                        send_data(s, cmd.data(), hdr->cmd_len);
                    }
                }
                else
                {
                    send_data(s, cmd.data(), hdr->cmd_len);
                }
                
                
            }
        }
    }
    
    if(packet_num > 0)
    {
       // printf("process queue sucess, packet_num = %u\n", packet_num);
    }
}


