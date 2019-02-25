#ifndef __ASYNDBCONN_H__
#define __ASYNDBCONN_H__

#include<hiredis/hiredis.h>
#include<hiredis/async.h>
#include<string>
#include<vector>
#include"Thread.h"


namespace pool {

class AsynCtx;
class AsynRedisNode;

class AsynDbPool : public Thread{    
public:
    static AsynDbPool* instance();
    bool add(const std::string& ip, int port);
    void reset(const std::string& key);
    bool operation(const char key[], const char format[], ...)
#if defined (__GNUC__)
  __attribute__((format(printf, 3, 4)))
#endif
;
    int getStatus(const std::string& key);
    
private:
    AsynDbPool();
    ~AsynDbPool();
    void run();
    AsynCtx* findOf(const redisAsyncContext *c);
    AsynCtx* getNode(const std::string& key);

public:   
    static void registerEvent(void* key, AsynCtx* val);
    static void eraseEvent(void* redis);
    static void connectCallback(const redisAsyncContext *c, int status);
    static void disconnectCallback(const redisAsyncContext *c, int status);
    
private:
    static AsynDbPool* g_instance;
    AsynRedisNode* m_nodes;
};

}

#endif
