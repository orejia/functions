#ifndef __REDISCONF_H__
#define __REDISCONF_H__

#include<string>
#include<vector>
#include<json/json.h>

namespace pool {

struct HostInfo{
    std::string m_ip;
    int m_port;
    int m_cnt;

    bool isValid(){
        return (!m_ip.empty() && 0 < m_port && 0 < m_cnt);
    }
};

typedef std::vector<HostInfo> typeVecHost;

static const std::string DEF_REDIS_HOST_SECTION = "redis_hosts";
static const std::string DEF_REDIS_IP_ITEM = "redis_ip";
static const std::string DEF_REDIS_PORT_ITEM = "redis_port";
static const std::string DEF_REDIS_CNT_ITEM = "redis_num";
static const std::string DEF_REDIS_PASSWD_ITEM = "redis_passwd";

class RedisConf {
public:
    int parse(const std::string& path);
    const typeVecHost& getHosts()const;
    const std::string& getAuthPasswd();

private:
    int readHosts();
    int readPasswd();
    int load(const std::string& path);
    int Asc2Num(const std::string& str);
    
private:
    std::string m_path;
    std::string m_passwd;
    typeVecHost m_vec; 
    Json::Value m_data;
};

}

#endif

