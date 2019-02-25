#ifndef __DBCONN_H__
#define __DBCONN_H__

#include<string>
#include<vector>
#include<hiredis/hiredis.h>


namespace pool {

class DbBase;
class DbRead;
class DbWrite;

static const std::string DEF_REDIS_CONF_PATH = "etc/config.redis";

class DbPool {
public:
    static DbPool* instance();
    bool init(const std::string& path = DEF_REDIS_CONF_PATH);
    redisReply* operation(const char key[], const char format[], ...)
#if defined (__GNUC__)
  __attribute__((format(printf, 3, 4)))
#endif
;
    bool appendCmd(const char key[], const char format[], ...)
#if defined (__GNUC__)
  __attribute__((format(printf, 3, 4)))
#endif
;
    void commit(const char key[]);
    void commit();
    void queryDebug(const char key[], const char cmd[]);
    void delPatternKeys(const char pattern[]);
    void delPatternKeys(const char pattern[], const char except[]);

private:
    DbPool();
    ~DbPool();
    
private:
    static DbPool* g_pool;
    DbBase* m_read;
    DbBase* m_write;
};

class DbRead{
public:
    static DbRead* instance();
    bool init(const std::string& path = DEF_REDIS_CONF_PATH);
    redisReply* operation(const char key[], const char format[], ...)
#if defined (__GNUC__)
  __attribute__((format(printf, 3, 4)))
#endif
;
    redisReply* operationCmd(const char key[], const char cmd[]);
    void queryDebug(const char key[], const char cmd[]);

private:
    DbRead();
    ~DbRead();
    
private:
    static DbRead* g_pool;
    DbBase* m_base;
};

class DbWrite{
public:
    static DbWrite* instance();
    bool init(const std::string& path = DEF_REDIS_CONF_PATH);

    bool appendCmd(const char key[], const char format[], ...)
#if defined (__GNUC__)
  __attribute__((format(printf, 3, 4)))
#endif
;
    void commit(const char key[]);
    void commit();

private:
    DbWrite();
    ~DbWrite();

private:
    static DbWrite* g_pool;
    DbBase* m_base;    
};

}

#endif

