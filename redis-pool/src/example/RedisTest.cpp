#include<iostream>
#include<string>
#include<cstring>
#include<unistd.h>
#include<sys/time.h>
#include<signal.h>
#include<sys/select.h>
#include<vector>
#include"DbConn.h"
#include"DbDebug.h"
#include"Log.h"


using namespace std;

typedef vector<string> vecString;
typedef vector<vecString> quotesVec;

static const string HQ_DETAIL_TBL = "H_stockQuote:stockDetails:";
static const string BLANK_DELIM_TOKEN = " ";

void parseQuoteItem(vecString& items, const string& data) {
    const string LINE_DELIM = "|";
    const string FIELD_DELIM = "=";
    size_t beg = 0;
    size_t end = 0;
    size_t found = 0;
    string line;
    string key;
    string val;

    if (data.empty()) {
        return;
    }
    
    do {
        end = data.find(LINE_DELIM, beg);
        if (string::npos != end) {
            line = data.substr(beg, end-beg);
            beg = end+1;
        } else {
            line = data.substr(beg);
            beg = end;
        }

        found = line.find(FIELD_DELIM);
        if (string::npos != found) {
            key = line.substr(0, found);
            val = line.substr(found+1);
            items.push_back(val);
            MyTestLog("key=%s| val=%s|", 
                key.c_str(),
                val.c_str());
        } 
    } while (string::npos != beg);

    return;
}

int getDetailQuotes(const string& key, const string& tbl,
    const vector<string>& vec, quotesVec& quotes) {
    int ret = 0;
    string txt = "HMGET ";
    string data;
    redisReply* reply = NULL;

    quotes.clear();
    
    txt += tbl;
    for (int i=0; i<(int)vec.size(); i++) {
        txt += BLANK_DELIM_TOKEN + vec[i];
    }

    if (!vec.empty()) {
        reply = pool::DbRead::instance()->operationCmd(key.c_str(),
            txt.c_str());
        if(reply != NULL) {
            if(reply->type == REDIS_REPLY_ARRAY) {
                quotes.resize(reply->elements);
                
                for (int i=0; i<(int)(reply->elements); i++) {
                    if (REDIS_REPLY_STRING == reply->element[i]->type) {
                        data.assign(reply->element[i]->str);
                        parseQuoteItem(quotes[i], data);
                    }
                }

                MyTestLog("get detail ok: cmd=%s| reply_size=%d|",
                    txt.c_str(),
                    (int)(reply->elements));
            } else {
                ret = -1;
                MyTestLog("get detail reply type error: cmd=%s| reply_type=%d|",
                    txt.c_str(),
                    reply->type);
            }

            freeReplyObject(reply);
        } else {
            ret = -1;
            MyTestLog("get detail error: cmd=%s|", txt.c_str());
        }
    }

    return ret;
}

int getAllDetailQuotes(const string& key,
    const string& tbl, quotesVec& quotes) {
    int ret = 0;
    string txt = "HVALS ";
    redisReply* reply = NULL;
    string data;

    quotes.clear();
    
    txt += tbl;
    reply = pool::DbRead::instance()->operationCmd(key.c_str(),
        txt.c_str());
    if(reply != NULL) {
        if(reply->type == REDIS_REPLY_ARRAY) {
            quotes.resize(reply->elements);
            
            for (int i=0; i<(int)(reply->elements); i++) {
                if (REDIS_REPLY_STRING == reply->element[i]->type) {
                    data.assign(reply->element[i]->str);
                    parseQuoteItem(quotes[i], data);
                } 
            }

            MyTestLog("get detail ok: cmd=%s| reply_size=%d|",
                txt.c_str(),
                (int)(reply->elements));
        } else {
            ret = -1;
            MyTestLog("get detail reply type error: cmd=%s| reply_type=%d|",
                txt.c_str(),
                reply->type);
        }

        freeReplyObject(reply);
    } else {
        ret = -1;
    }

    return ret;
}

int getSingleDetailQuotes(const string& key,
    const string& tbl, string code, vecString& item) {
    int ret = 0;
    string txt = "HGET ";
    redisReply* reply = NULL;
    string data;

    item.clear();
    
    txt += tbl 
        + BLANK_DELIM_TOKEN + code;
    reply = pool::DbRead::instance()->operationCmd(key.c_str(),
        txt.c_str());
    if(reply != NULL) {
        if(reply->type == REDIS_REPLY_STRING) {   
            data.assign(reply->str);
            parseQuoteItem(item, data);
        } 

        MyTestLog("get detail ok: cmd=%s| reply=%s|",
            txt.c_str(),
            reply->str);

        freeReplyObject(reply);
    } else {
        ret = -1;
    }

    return ret;
}

void syncTest(const char path[]){
    string key;
    string cmd;

    pool::DbRead::instance()->init(path);
    
    do {
        cout << "enter a key[0-exit] and cmd to query:";
        cin >> key;
        if ('0' == key[0]) {
            break;
        }
        
        cin.ignore(256, '\n');
        getline(cin, cmd);
        
        pool::DbRead::instance()->queryDebug(key.c_str(), cmd.c_str());
    } while (true);
}

void usage(const char program[]){
    cout << "usage: " << program << " path_of_config.redis\n";
}

void locateKey(const char path[]){
    pool::DbDebug d;
    bool bOk = true;
    string key;    

    bOk = d.init(path);
    while (bOk) {
        cout << "enter a key to locate node:";
        cin >> key;

        d.locate(key);
    }
}

void syncDelPattern(const char path[]) {
    string pattern;
    struct timeval begTm, endTm;
    
    pool::DbPool::instance()->init(path);
    
    cout << "enter a pattern to del:";
    cin >> pattern;

    gettimeofday(&begTm, NULL);

    pool::DbPool::instance()->delPatternKeys(pattern.c_str());

    gettimeofday(&endTm, NULL);

    cout << "time=" << endTm.tv_sec - begTm.tv_sec << endl;
}

void syncDetail(const char path[]) {
    vector<string> vec;
    quotesVec mp;
    vecString item;
    struct timeval begTm, endTm;
    
    pool::DbRead::instance()->init(path);

    int n = 0;
    int fdcnt = 2;
    string key, tbl;

    cout << "enter a key and tbl:";
    cin >> key >> tbl;

    cout << "enter fields:";
    vec.resize(fdcnt);
    for (int i=0; i<fdcnt; i++) {
        cin >> vec[i];
    }
    
    cout << "enter a count:";
    cin >> n;

    gettimeofday(&begTm, NULL);
    while (n-- > 0) {
        getDetailQuotes(key, tbl, vec, mp);
        getAllDetailQuotes(key, tbl, mp);
        getSingleDetailQuotes(key, tbl, vec[0], item);
    }

    gettimeofday(&endTm, NULL);

    cout << "time=" << endTm.tv_sec - begTm.tv_sec << endl;
}

void syncLoop(const char path[]) {
    string tbl = "H_stockQuote:stockDetails:sh#600724";
    redisReply *reply = NULL;
    struct timeval begTm, endTm;
    
    pool::DbRead::instance()->init(path);

    int n = 0;

    cout << "enter a tbl:";
    cin >> tbl;
    
    cout << "enter a count:";
    cin >> n;

    gettimeofday(&begTm, NULL);
    while (n-- > 0) {
        /*
        reply = pool::DbRead::instance()->operation(tbl.c_str(), 
            "HGETALL %s", 
            tbl.c_str());
        
        
        reply = pool::DbRead::instance()->operation(tbl.c_str(), 
            "ZREVRANGE %s %d %d",
            tbl.c_str(),
            0, 20);
        */

        string txt = (string)"HMGET " + tbl + " change_percent unique_code date";
        reply = pool::DbRead::instance()->operationCmd(tbl.c_str(), 
            txt.c_str());
        
        if (NULL != reply) {
            if(reply->type == REDIS_REPLY_ARRAY) {
                for (int i=0; i<(int)(reply->elements); i++) {
                    MyTestLog("--item=%s", reply->element[i]->str);
                }
            } else {
                cout << "reply type =" << reply->type << endl;
            }

            freeReplyObject(reply);
        } else {
            cout << "reply is null." << endl;
        }
    }
    gettimeofday(&endTm, NULL);

    cout << "time=" << endTm.tv_sec - begTm.tv_sec << endl;
}

int main(int argc, char* argv[]) {
    int opt = 0;
    
    if (2 != argc){
        usage(argv[0]);
        return -1;
    }

    cout << "choose option[0-locate_key, 1-sync_test, 2-del]:";
    cin >> opt;
    
    if ( 1 == opt ){
        syncDetail(argv[1]);
    } else if (2 == opt) {
        syncDelPattern(argv[1]);
    } else {
        locateKey(argv[1]);
    }
    
    cout << "exit here." << endl;
    return 0;
}


