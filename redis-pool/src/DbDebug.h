#ifndef __DBDEBUG_H__
#define __DBDEBUG_H__

#include<string>

namespace pool{

class DbDebug{
    class _internal;
public:
    DbDebug();
    bool init(const std::string& path);
    void locate(const std::string& key);

private:    
    bool add(const std::string& ip, int port);
    bool add(const std::string& ip, int port, int cnt);
    RedisConf* getConf(const std::string& path);
private:
    _internal* m_nodes;
    RedisConf m_conf;
    RedisConf* m_pConf;
};

}


#endif

