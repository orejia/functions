#ifndef __DBBASE_H__
#define __DBBASE_H__

#include<string>
#include<vector>
#include<hiredis/hiredis.h>
#include"RedisConf.h"

namespace pool {

class RedisCtx;
class SyncRedisNode;
class DbPool;
class DbRead;
class DbWrite;

class DbBase {
    typedef std::vector<RedisCtx*> typeVecRedis;
    
public:
    bool init(const std::string& path);
    
    redisReply* operation(const char key[], 
        const char format[],va_list ap);

    redisReply* operationCmd(const char key[], const char cmd[]);

    void delPatternKeys(const char pattern[]);
    void delPatternKeys(const char pattern[], const char except[]);
    
    bool appendCmd(const char key[], 
        const char format[], va_list ap);
    void commit(const char key[]);
    void commit();
    void queryDebug(const char key[], const char cmd[]);

friend class DbPool;
friend class DbRead;
friend class DbWrite;

private:
    DbBase();
    ~DbBase();
    bool add(const std::string& ip, int port, int cnt);
    bool add(const std::string& ip, int port);
    void clear();
    RedisCtx* getNode(const std::string& key);
    bool chkPattern(const char pattern[]);
    RedisConf* getConf(const std::string& path);
    
private:
    std::string m_authPasswd;
    SyncRedisNode* m_nodes; 
    typeVecRedis m_vec;
    RedisConf m_conf;
    RedisConf* m_pConf;
};

}

#endif

