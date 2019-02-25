#include<fstream>
#include<cstdlib>
#include<errno.h>
#include<cstring>
#include"RedisConf.h"
#include"Log.h"

using namespace std;

namespace pool {
    
int RedisConf::load(const std::string& path) {
    int ret = 0;
    ifstream in;
    HostInfo info;

    in.open(path.c_str(), ifstream::in);
    if (!in.is_open()){
        MyTestLog("fail to open redis config file[%s]:%s|",
            path.c_str(),
            strerror(errno));
        return -1;
    }

    try {
        in >> m_data;
    }  catch(...) {
        ret = -1;
        MyTestLog("fail to parse redis config file[%s]:"
            " rc=%d| msg=invalid json format|",
            path.c_str(),
            ret);
    } 

    in.close();
    m_path = path;
    return ret;
}

int RedisConf::readPasswd(){
    int ret = 0;

    try {
        m_passwd = m_data[DEF_REDIS_PASSWD_ITEM].asString();
    } catch(...) {
        ret = -2;
        MyTestLog("fail to parse redis passwd[%s]:"
            " rc=%d| msg=invalid %s format|",
            m_path.c_str(),
            ret,
            DEF_REDIS_PASSWD_ITEM.c_str());
    } 

    return ret;
}

int RedisConf::readHosts(){
    int ret = 0;
    Json::Value obj;
    HostInfo info;

    m_vec.clear();
    
    try {
        obj = m_data[DEF_REDIS_HOST_SECTION];
        if (obj.isArray()) {
            for (unsigned int i=0; i<obj.size(); i++){
                const Json::Value& item = obj[i];
                info.m_ip= item[DEF_REDIS_IP_ITEM].asString();
                info.m_port = Asc2Num(item[DEF_REDIS_PORT_ITEM].asString());
                info.m_cnt = Asc2Num(item[DEF_REDIS_CNT_ITEM].asString());

                if (info.isValid()) {
                    m_vec.push_back(info);
                }
            }

            if (m_vec.empty()){
                ret = -2;
                MyTestLog("parse host[%s] err:"
                    " rc=%d| msg=empty valid host.",
                    m_path.c_str(),
                    ret);
            }
        } else {
            ret = -3;
            MyTestLog("parse redis hosts[%s] err:"
                " rc=%d| msg=invalid hosts info|",
                m_path.c_str(),
                ret);
        }
    } catch(...) {
        ret = -1;
        MyTestLog("fail to parse redis hosts[%s]:"
            " rc=%d| msg=invalid hosts format|",
            m_path.c_str(),
            ret);
    } 

    return ret;
}

int RedisConf::parse(const std::string& path){
    int ret = 0;

    ret = load(path);
    if ( 0 != ret ){
        return ret;
    }

    ret = readHosts();
    if ( 0 != ret ){
        return ret;
    }

    ret = readPasswd();
    if ( 0 != ret ){
        return ret;
    }

    return ret;
}

int RedisConf::Asc2Num(const std::string& str){
    int val = 0;

    if (!str.empty()){
        val = (int)strtol(str.c_str(), NULL, 10);
    }

    return val;
}

const typeVecHost& RedisConf::getHosts()const {
    return m_vec;
}

const std::string& RedisConf::getAuthPasswd(){
    return m_passwd;
}

}

