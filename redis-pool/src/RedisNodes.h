#ifndef __REDISNODES_H__
#define __REDISNODES_H__

#include<string>
#include<map>
#include"HashNodes.h"


namespace pool {

template <typename T>
class RedisNodes {
    struct PointerCmp {
        bool operator() (const void* x, const void* y)const{
            return (unsigned long long)x < (unsigned long long)y;
        }
    };
    
    typedef std::map<const void*, void*, PointerCmp> typeData;
    typedef std::pair<const void*, void*> typeDataPair;
    
public:
    T* getNode(const std::string& key){
        T* val = NULL;

        val = reinterpret_cast<T*>(m_pool.getNode(key));
        return val;
    }
    
    void putNode(const std::string& key, T* val){
        m_pool.putNode(key, val);
    }

    void putData(const void* p, T* data){
        m_datas.insert(typeDataPair(p, data));
    }

    void eraseData(const void* p) {
        m_datas.erase(p);
    }

    T* findData(const void* p){
        T* val = NULL;
        typename typeData::iterator itr = m_datas.find(p);
        
        if (m_datas.end() != itr) {
            val = reinterpret_cast<T*>(itr->second);
        }
    
        return val;
    }
    
    void dsp(){m_pool.dsp();}
    void clear(){m_pool.clear(); m_datas.clear();}
    int size(){return m_pool.size();}

private:
    HashNodes m_pool;
    typeData m_datas;
};


}

#endif

