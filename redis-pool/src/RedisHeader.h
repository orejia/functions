#ifndef __REDISHEADER_H__
#define __REDISHEADER_H__

#include<string>
#include<vector>
#include<hiredis/hiredis.h>
#include<hiredis/async.h>


namespace pool {

class DbPool;
class AsynDbPool;
class ThreadLock;

enum CTX_STATUS{
    CTX_OK = 0,
    CTX_INIT,
    CTX_ERR,
    CTX_CLOSE
};

static const int DB_CONN_TIMEOUT = 3;
static const int MAX_REDIS_DB_CNT = 1;
static const int MAX_VNODE_CNT = 4;
static const int MAX_ASYNC_TASK_DISCONN_CNT = 10000;


class RedisCtx{
private:
    RedisCtx();
    ~RedisCtx();

friend class DbBase;

public:
    int getStatus()const{return status;}
    bool reconnSafe();
    void setTimeout(int sec){timeout=sec;}
    void setHost(const std::string& ip, int port, int index);
    void setAuthPasswd(const std::string& strPasswd);
    redisReply* operation(const char format[], va_list ap);
    redisReply* operationCmd(const char cmd[]);
    bool appendCmd(const char format[], va_list ap);
    bool commit();
    bool queryDebug(const char cmd[]);
    bool delPatternKeys(const char pattern[]);
    bool delPatternKeys(const char pattern[], const char except[]);

private: 
    bool chkValid();
    bool reconn();
    bool authDb();
    bool conn();
    void close();
    bool execute(const char cmd[]);
    redisReply* executeV(const char format[], va_list ap);
    bool appendV(const char format[], va_list ap);
    bool selectDb();
    bool getReplies();
    void infoReply(redisReply* r);
    bool getPatternKeys(const char pattern[], std::vector<std::string>& vec);
    bool getPatternKeys(const char pattern[], const char except[], std::vector<std::string>& vec);
    bool delKeys(const std::vector<std::string>& vec);
    
private:  
    int timeout;
    int m_port;
    int m_index;
    int status;
    unsigned int m_appCnt;
    std::string m_key;
    std::string m_ip;
    std::string m_passwd;
    redisContext* ctx;
    ThreadLock* m_lock;
    static const unsigned int MAX_COMMIT_CNT = 5000;
};

class AsynCtx {
private:
    AsynCtx();
    ~AsynCtx();

friend class AsynDbPool;

public:
    int getStatus(){return status;}
    const std::string& getKey()const{return m_key;}
    void setTimeout(int sec){timeout=sec;}
    void setHost(const std::string& ip, int port, int index);
    bool operation(const char format[], va_list ap);
    bool reconnSafe();
    bool closeSafe();

private:   
    bool chkValid();
    bool selectDb();
    bool reconn();
    bool conn();
    bool close();

private:
    static void callbackCmdSet(redisAsyncContext *c,
        void *reply, void *privdata);
    static void callbackSelect(redisAsyncContext *c, 
        void *r, void *data);
    
private:   
    int timeout;
    int m_port;
    int status;
    int m_index;
    unsigned int m_task_cnt;
    std::string m_key;
    std::string m_ip;
    redisAsyncContext* ctx;
    ThreadLock* m_lock;
};

extern std::string genKey(const std::string& ip, int port, 
    int index, int vnode);

}

#endif


