#ifndef __REDIS_CLIENT_H__
#define __REDIS_CLIENT_H__
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<errno.h>
#include<unistd.h>

#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include <event.h>
//#include<event2/event.h>

//#include<event2/util.h>
#include <iostream>
#include <string>
#include <vector>


#include "queue.h"
#include "redis_protocol.h"

#include "util.h"
#include "RedisNodes.h"
#include "socket_set.h"
#include "RedisConf.h"
#include "memory_pool.h"
#include "packet_header.h"


using namespace std;
using namespace pool;

struct epoll_event;

#define DEQUEUE_BUFFER_SIZE (1024 * 1024 * 16)




typedef struct _redis_request_t
{
    uint32_t id;
    string str_key;
    string str_cmd;
}redis_request_t;

uint32_t get_memory_pool_block_count(uint32_t socket_count);


class redis_connection_t: public RedisNodes<socket_t>{};

class redis_client_t
{
    public:

        
        int append_cmd(const void *key, const char *format, ...)
#if defined (__GNUC__)
   __attribute__((format(printf, 3, 4)))
#endif
;
        int init(const string &filepath);

        RedisConf* getConf(const std::string& path);

        //static redis_client_t* instance();
        
        int init_node(const string &filepath);
        socket_t *get_node(const string &key);
        //int put_node(const string &key);
        int add(const string &strip, int port);
        int write_comand(const redis_client_t &cmd);

        static void* dispatch_thread_proc_wrapper(void *param);
        int dispatch_thread_proc();
        void process_request_queue(char *dequeue_buf);

    public:
        redis_client_t();
        ~redis_client_t();
    private:
        //static redis_client_t *g_instance;
        
        redis_connection_t *m_nodes;
        //queque_t<redis_request_ptr> m_req_queue; 
        queue_t m_queue;
        memory_pool_t m_recv_memory_pool;
        memory_pool_t m_send_memory_pool;
        
        RedisConf *m_cfg;
        RedisConf conf_;
        string m_strpath;
        int m_epoll_fd;
        struct epoll_event *m_evs;
        pthread_t m_tid;
        string m_authPasswd;
        pthread_mutex_t m_append_mutex;
       
};

#endif
