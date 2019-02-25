#include<cstring>
#include<cstdio>
#include"RedisNodes.h"
#include"RedisHeader.h"
#include"Log.h"
#include"DbBase.h"
#include"DbDebug.h"


namespace pool{

struct NodeName{
    std::string name;
};

class SyncRedisNode : public RedisNodes<RedisCtx>{};
class DbDebug::_internal : public RedisNodes<NodeName>{};

RedisConf* DbBase::getConf(const std::string& path) {
    int ret = 0;
    ret = m_conf.parse(path);
    m_pConf = &m_conf;

    if (0 != ret) {
        m_pConf = NULL;
    }

    return m_pConf;
}

DbBase::DbBase(){
    m_nodes = new SyncRedisNode();
}

DbBase::~DbBase(){
    if (NULL != m_nodes){
        delete m_nodes;
    }

    clear();
}

void DbBase::clear(){
    RedisCtx* redis = NULL;
    
    for (int i=0; i<(int)m_vec.size(); i++){
        redis = m_vec[i];
        if (NULL != redis){
            delete[] redis;
        }
    }

    m_vec.clear();
}

bool DbBase::add(const std::string& ip, int port) {
    RedisCtx* redis = NULL;
    std::string key;
    bool bOk = true;

    redis = new RedisCtx[MAX_REDIS_DB_CNT];
    m_vec.push_back(redis);
    
    for (int i=0; i<MAX_REDIS_DB_CNT; i++){
        redis[i].setTimeout(DB_CONN_TIMEOUT);
        redis[i].setHost(ip, port, i);        
        redis[i].setAuthPasswd(m_authPasswd);

        for (int j=0; j<MAX_VNODE_CNT; j++) {
            key = genKey(ip, port, i, j);
            m_nodes->putNode(key, &redis[i]);
        }

        bOk = redis[i].reconnSafe();
    }
    
    return bOk;
}

RedisCtx* DbBase::getNode(const std::string& key){
    RedisCtx* p = NULL;
    
    p = m_nodes->getNode(key);
    if (NULL != p) {
    } else {
        MyErrorLog("not found redis_node: key=%s|", key.c_str());
    }

    return p;
}

redisReply* DbBase::operation(const char key[],
    const char format[], va_list ap){
    RedisCtx* redis = NULL;
    redisReply* reply = NULL;

    redis = getNode(key);
    if (NULL != redis) {
        reply = redis->operation(format, ap);
    } 

    return reply;
}

redisReply* DbBase::operationCmd(const char key[], const char cmd[]) {
    RedisCtx* redis = NULL;
    redisReply* reply = NULL;

    redis = getNode(key);
    if (NULL != redis) {
        reply = redis->operationCmd(cmd);
    } 

    return reply;
}

bool DbBase::chkPattern(const char pattern[]) {
    const char BLANK_DELIM = ' ';
    const char WILD_CARD_CHAR = '*';
    bool bOk = false;

    for (int i=0; i<(int)(strlen(pattern)); i++) {
        if (BLANK_DELIM != pattern[i]
            && WILD_CARD_CHAR != pattern[i]) {
            bOk = true;
            break;
        }
    }

    return bOk;
}

void DbBase::delPatternKeys(const char pattern[], const char except[]) {
    RedisCtx* redis = NULL;
    bool bOk = true;

    bOk = chkPattern(pattern);
    if (bOk) {
        for (int i=0; i<(int)m_vec.size(); i++){
            redis = m_vec[i];

            if (NULL != redis){
                for (int j=0; j<MAX_REDIS_DB_CNT; j++){
                    redis[j].delPatternKeys(pattern, except);
                }
            }
        }
    } else {
        MyErrorLog("del_pattern_keys_error| pattern=%s| msg=check false|",
            pattern);
    }
}

void DbBase::delPatternKeys(const char pattern[]) {
    RedisCtx* redis = NULL;
    bool bOk = true;

    bOk = chkPattern(pattern);
    if (bOk) {
        for (int i=0; i<(int)m_vec.size(); i++){
            redis = m_vec[i];

            if (NULL != redis){
                for (int j=0; j<MAX_REDIS_DB_CNT; j++){
                    redis[j].delPatternKeys(pattern);
                }
            }
        }
    } else {
        MyErrorLog("del_pattern_keys_error| pattern=%s| msg=check false|",
            pattern);
    }
}

void DbBase::queryDebug(const char key[], const char cmd[]){
    RedisCtx* redis = NULL;

    redis = getNode(key);
    if (NULL != redis) {
        redis->queryDebug(cmd);
    } 
}

bool DbBase::appendCmd(const char key[], 
    const char format[], va_list ap){
    bool bOk = true;
    RedisCtx* redis = NULL;

    redis = getNode(key);
    if (NULL != redis) {
        bOk = redis->appendCmd(format, ap);
    } else {
        bOk = false;
    }

    return bOk;
}

void DbBase::commit(const char key[]){
    RedisCtx* redis = NULL;

    redis = getNode(key);
    if (NULL != redis) {
        redis->commit();
    } 
}

void DbBase::commit(){
    RedisCtx* redis = NULL;

    for (int i=0; i<(int)m_vec.size(); i++){
        redis = m_vec[i];

        if (NULL != redis){
            for (int j=0; j<MAX_REDIS_DB_CNT; j++){
                redis[j].commit();
            }
        }
    }
}

bool DbBase::add(const std::string& ip, int port, int cnt){
    bool bOk = true;

    if (!ip.empty() && 0 < port && 0 < cnt) {
        for (int i=0; i<cnt; i++){
            bOk = add(ip, port+i);
            if (!bOk){
                break;
            }
        }
    } else {
        bOk = false;

        MyErrorLog("add redis err: ip=%s| port=%d| cnt=%d|", 
            ip.c_str(),
            port,
            cnt);
    }

    return bOk;
}

bool DbBase::init(const std::string& path){
    RedisConf* pConf = NULL;
    bool bOk = false;

    pConf = getConf(path);

    if (NULL != pConf) {
        m_authPasswd = pConf->getAuthPasswd();
        
        const typeVecHost& hosts = pConf->getHosts();
        
        for (int i=0; i<(int)hosts.size(); i++){
            const HostInfo& h = hosts[i];
            bOk = add(h.m_ip, h.m_port, h.m_cnt);
            if (!bOk) {
                break;
            }
        }
    } 

    return bOk;
}

DbDebug::DbDebug(){
    m_nodes = new _internal();
}

RedisConf* DbDebug::getConf(const std::string& path) {
    int ret = 0;
    ret = m_conf.parse(path);
    m_pConf = &m_conf;

    if (0 != ret) {
        m_pConf = NULL;
    }

    return m_pConf;

}
bool DbDebug::init(const std::string& path){
    RedisConf* pConf = NULL;

    pConf = getConf(path);
    if (NULL != pConf) {
        const typeVecHost& hosts = pConf->getHosts();

        for (int i=0; i<(int)hosts.size(); i++){
            const HostInfo& h = hosts[i];
            add(h.m_ip, h.m_port, h.m_cnt);
        }

        return true;
    } else {
        return false;
    }
}

bool DbDebug::add(const std::string& ip, int port, int cnt){
    bool bOk = true;

    if (!ip.empty() && 0 < port && 0 < cnt) {
        for (int i=0; i<cnt; i++){
            add(ip, port+i);
        }
    } else {
        bOk = false;

        MyTestLog("add redis err: ip=%s| port=%d| cnt=%d|", 
            ip.c_str(),
            port,
            cnt);
    }

    return bOk;
}

bool DbDebug::add(const std::string& ip, int port) {
    std::string key;
    NodeName* nodes = NULL;
    bool bOk = true;

    nodes = new NodeName[MAX_REDIS_DB_CNT];
    for (int i=0; i<MAX_REDIS_DB_CNT; i++){       
        nodes[i].name = genKey(ip, port, i, MAX_REDIS_DB_CNT);
        
        for (int j=0; j<MAX_VNODE_CNT; j++) {
            key = genKey(ip, port, i, j);
            m_nodes->putNode(key, &nodes[i]);
        }
    }
    
    return bOk;
}

void DbDebug::locate(const std::string& key){
    NodeName* p = NULL;
    
    p = m_nodes->getNode(key);
    if (NULL != p) {
        MyTestLog("found node: key=%s| node=%s|",
            key.c_str(), p->name.c_str());
    } else {
        MyTestLog("not found node: key=%s|", key.c_str());
    }
}

}


