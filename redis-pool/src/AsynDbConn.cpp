#include<cstring>
#include<cstdio>
#include<cstdlib>
#include<sstream>
#include<unistd.h>
#include<sys/select.h>
#include<hiredis/adapters/libevent.h>
#include<hiredis/async.h>
#include"AsynDbConn.h"
#include"RedisNodes.h"
#include"RedisHeader.h"
#include"Log.h"


namespace pool{

AsynDbPool* AsynDbPool::g_instance = NULL;

static struct event_base* g_base = NULL;

class AsynRedisNode : public RedisNodes<AsynCtx> {};

AsynDbPool::AsynDbPool(){
    m_nodes = new AsynRedisNode();
}

AsynDbPool::~AsynDbPool(){
    if(NULL != m_nodes){
        delete m_nodes;
    } 
}

AsynDbPool* AsynDbPool::instance(){
    if (NULL == g_instance){
        g_instance = new AsynDbPool();
        g_base = event_base_new();
    }

    return g_instance;
}

bool AsynDbPool::add(const std::string& ip, int port) {
    AsynCtx* redis = NULL;
    std::string key;
    bool bOk = true;

    redis = new AsynCtx[MAX_REDIS_DB_CNT];
    for(int i=0; i<MAX_REDIS_DB_CNT; i++) {
        redis[i].setTimeout(DB_CONN_TIMEOUT);
        redis[i].setHost(ip, port, i);

        for (int j=0; j<MAX_VNODE_CNT; j++) {
            key = genKey(ip, port, i, j);
            m_nodes->putNode(key, &redis[i]);
        }

        bOk = redis[i].reconnSafe();
    }
    
    return bOk;
}

void AsynDbPool::reset(const std::string& key) {
    AsynCtx* redis = NULL;

    redis = getNode(key);
    if (NULL != redis) {    
        redis->reconnSafe(); 
    }
}

bool AsynDbPool::operation(const char key[], const char format[], ...){
    bool bOk = true;
    AsynCtx* redis = NULL;

    redis = getNode(key);
    if (NULL != redis) {
        va_list ap;

        va_start(ap, format);
        bOk = redis->operation(format, ap);
        va_end(ap);
    } else {
        bOk = false;
    }

    return bOk;
}

int AsynDbPool::getStatus(const std::string& key){
    AsynCtx* redis = NULL;
    
    redis = getNode(key);
    if (NULL != redis) {
        return redis->getStatus();
    } else {
        return -1;
    }
}

void AsynDbPool::registerEvent(void* key, AsynCtx* val) {
    redisAsyncContext* ctx = reinterpret_cast<redisAsyncContext*>(key);

    if (NULL != key && NULL != val) {
        MyTestLog("register event: key=%p| val=%p|",
            key, val);
        
        AsynDbPool::instance()->m_nodes->putData(key, val);

        //event format
        redisLibeventAttach(ctx,g_base);

        redisAsyncSetConnectCallback(ctx, connectCallback);
        redisAsyncSetDisconnectCallback(ctx, disconnectCallback);
    } else {
        MyTestLog("register event null: key=%p| val=%p|",
            key, val);
    }
}

void AsynDbPool::eraseEvent(void* key) {
    if (NULL != key) {
        AsynDbPool::instance()->m_nodes->eraseData(key);
        MyTestLog("erase event: key=%p|", key);
    }    
}

AsynCtx* AsynDbPool::getNode(const std::string& key){
    AsynCtx* p = NULL;
    
    p = m_nodes->getNode(key);
    if (NULL != p) {
        MyTestLog("find node: key=%s| node=[%p]%s|", 
            key.c_str(), p, p->getKey().c_str());
    } else {
        MyTestLog("not found node: key=%s|", key.c_str());
    }
    
    return p;
}

void AsynDbPool::run(){
    struct timeval timeout;
    
    MyTestLog("start to run ev_loop.");

    while (isRun()) {
        MyTestLog("ev_loop in run.");

        event_base_dispatch(g_base);
        
        //wait for 10 seconds
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        select(0, NULL, NULL, NULL, &timeout);
    }

    MyTestLog("exit the ev_loop.");
}

void AsynDbPool::connectCallback(
    const redisAsyncContext *c, int status){
    AsynCtx* pCtx = NULL;

    pCtx = g_instance->findOf(c);
    if (NULL != pCtx) {
        if ( 0 == status ) {
            MyTestLog("==connect callback ok: redis=%p|"
                " ctx=%p| status=%d| node=%s|", 
                pCtx, c, status, pCtx->getKey().c_str()); 
        } else {            
            MyTestLog("==connect callback error: redis=%p|"
                " ctx=%p| status=%d| node=%s|", 
                pCtx, c, status, pCtx->getKey().c_str());
        }        
    } else {
        MyTestLog("==connect callback find null: c=%p| status=%d|", c, status);
    }
}

void AsynDbPool::disconnectCallback(
    const redisAsyncContext *c, int status){
    MyTestLog("===disconnect callback: c=%p| status=%d|", c, status);
}

AsynCtx* AsynDbPool::findOf(const redisAsyncContext *c){

    AsynCtx* pCtx = m_nodes->findData(c);

    if (NULL != pCtx) {
        MyTestLog("++found ctx: %p-%p", c, pCtx);
    } else {
        MyTestLog("--not found ctx: cxt=%p", c);
    }
    
    return pCtx;
}

}


