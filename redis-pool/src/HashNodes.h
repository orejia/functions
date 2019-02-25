#ifndef __HASHNODES_H__
#define __HASHNODES_H__
#include<string>
#include<map>
//#include"Log.h"

namespace pool{

class HashNodes {
    
    union Data2Long{
        unsigned long long l;
        char sz[8];
    };
    
    typedef std::map<unsigned int, void*> typeMap;
    typedef std::pair<unsigned int, void*> typePair;
    
public:
    void putNode(const std::string& key, void* node){
        unsigned int l = 0;

        l = hash(key);
        m_map.insert(typePair(l, node));

        //MyInfoLog("put node: key=%s| hash=%u| node=%p|\n", key.c_str(), l, node);
    }   
    
    void* getNode(const std::string& key){
        unsigned int l = 0;
        typeMap::iterator itr;
        
        l = hash(key);
        itr = m_map.find(l);
        if (m_map.end() == itr) {
            itr = m_map.upper_bound(l);

            /* if the key is the biggest, then return the first */
            if ( m_map.end() == itr ) {
                itr = m_map.begin();
            }
        }
        
        return itr->second;
    }

    void clear() {
        m_map.clear();
    }
    
    int size() {
        return (int)m_map.size();
    }

    void dsp(){
        for (typeMap::iterator itr=m_map.begin(); itr!=m_map.end(); itr++) {
            //MyInfoLog("=>key=%u| val=%p|\n",
               // itr->first,
                //itr->second);
        }
    }
    
private:
	unsigned int hash(const std::string& key){
        Data2Long stu;
    	unsigned long long seed = 0x1234ABCDL;
    	unsigned long long m = 0xc6a4a7935bd1e995LL;
    	unsigned long long h = seed ^ (key.size() * m);
    	unsigned long long k = 0L;
    	int r = 47;
    	int n = 0;
    	
    	for ( n=0; n+8 <= (int)key.size(); n+=8 ){
            key.copy(stu.sz, 8, n);
    		k = stu.l;
    		k *= m;
    		k ^= k >> r;
    		k *= m;
    		h ^= k;
    		h *= m;
        }

        if ( n < (int)key.size() ){
            for(int i=0; i<(int)sizeof(stu.sz); i++) {
                stu.sz[i] = 0;
            }
            
            key.copy(stu.sz, 8, n);
            k = stu.l;
            h ^= k;
            h *= m;
        }

        h ^= h >> r;
        h *= m;
        h ^= h >> r;
        
        return (unsigned int)h;
    }

private:
    typeMap m_map;
};

}

#endif

