#include<cstring>
#include<cstdio>
#include<cstdlib>
#include"RedisHeader.h"
#include"Log.h"
#include"AsynDbConn.h"
#include"ThreadLock.h"


namespace pool {

RedisCtx::RedisCtx(){
    m_index = 0;
    timeout = 3;
    ctx = NULL;
    status = CTX_OK;
    m_port= 0;
    m_appCnt = 0;
    m_passwd.clear();
    m_lock = new ThreadLock();
}

RedisCtx::~RedisCtx(){
    close();

    if (NULL != m_lock){
        delete m_lock;
    }
}

void RedisCtx::setHost(const std::string& ip, int port, int index){    
    m_ip=ip;
    m_port=port;
    m_index = index;
    m_key = genKey(ip, port, index, MAX_VNODE_CNT);
}

void RedisCtx::setAuthPasswd(const std::string& strPasswd){
    m_passwd = strPasswd;
}

bool RedisCtx::reconn() {
    close();
    return conn();
}

bool RedisCtx::reconnSafe(){
    GuardLock guard(m_lock);

    return reconn();
}

bool RedisCtx::conn() {
    bool bOk = true;
    struct timeval tv;

    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    ctx = redisConnectWithTimeout(m_ip.c_str(), m_port, tv);
    if (NULL == ctx) {
        MyErrorLog("conn error: ip=%s| port=%d| timeout=%d|",
            m_ip.c_str(), m_port, timeout);

        close();
        return false;
    }

    if (0 != ctx->err) {
        MyErrorLog("conn error: err=%d| ip=%s| port=%d| timeout=%d| msg=%s|",
            ctx->err, m_ip.c_str(), m_port, timeout, ctx->errstr);

        close();
        return false;
    }

    status = CTX_OK;
    bOk = authDb();
    if (bOk) {
        bOk = selectDb();
    }

    if (bOk) {
        MyTestLog("++db ready: ctx=%p| db=%s|", ctx, m_key.c_str());
    }
    return bOk;
}

bool RedisCtx::selectDb(){
    bool bOk = true;
    char cmd[64] = {0};

    snprintf(cmd, 64, "select %d", m_index);
    bOk = execute(cmd);
    if (bOk) {
        status = CTX_OK;
    } else {
        close();
    }
    
    return bOk;
}

bool RedisCtx::authDb(){
    bool bOk = true;
    char cmd[64] = {0};

    if (!m_passwd.empty()) {
        snprintf(cmd, 64, "auth %s", m_passwd.c_str());
        bOk = execute(cmd);
        if (bOk) {
            status = CTX_OK;
        } else {
            close();
            MyErrorLog("auth_redis_err| passwd_size=%d|"
                " msg=invalid passwd|",
                (int)(m_passwd.size()));
        }
    }
    
    return bOk;
}

redisReply* RedisCtx::operation(const char format[], va_list ap){
    bool bOk = false;
    redisReply* reply = NULL;
    GuardLock guard(m_lock);

    bOk = chkValid();
    if (bOk) {
        reply = executeV(format, ap);
    } else {
        MyErrorLog("operationV status err: ctx=[%p]%s| format=%s|"
            " msg=reconnect first, and then try again.", 
            ctx,
            m_key.c_str(),
            format);
    }

    if (NULL == reply) {
        reconn();
    }
    
    return reply;
}

redisReply* RedisCtx::operationCmd(const char cmd[]){
    bool bOk = false;
    redisReply* reply = NULL;
    GuardLock guard(m_lock);

    bOk = chkValid();
    if (bOk) {
        reply = (redisReply*)redisCommand(ctx, cmd);
        if (NULL != reply) {        
            MyTestLog("operation reply ok: type=%d| format=%s|"
                " ctx=[%p]%s| status=%d|", 
                reply->type,
                cmd,
                ctx,
                m_key.c_str(),
                status);
        } else {
            MyErrorLog("operation reply err: reply=NULL| format=%s|"
                " ctx=[%p]%s| status=%d|", 
                cmd,
                ctx,
                m_key.c_str(),
                status);
        }
    } else {
        MyErrorLog("operation status err: ctx=[%p]%s| format=%s|"
            " msg=reconnect first, and then try again.", 
            ctx,
            m_key.c_str(),
            cmd);
    }

    if (NULL == reply) {
        reconn();
    }
    
    return reply;
}

bool RedisCtx::delPatternKeys(const char pattern[], const char except[]) {
    bool bOk = true;
    std::vector<std::string> vec;
    GuardLock guard(m_lock);

    bOk = chkValid();
    if (bOk) {
        bOk = getPatternKeys(pattern, except, vec);
        if (bOk) {
            bOk = delKeys(vec);
        }
    } else {
        MyErrorLog("del_pattern_keys status err: ctx=[%p]%s| pattern=%s|"
            " msg=reconnect first, and then try again.", 
            ctx,
            m_key.c_str(),
            pattern);
    }

    if (!bOk) {
        reconn();
    }

    return bOk;
}

bool RedisCtx::delPatternKeys(const char pattern[]) {
    bool bOk = true;
    std::vector<std::string> vec;
    GuardLock guard(m_lock);

    bOk = chkValid();
    if (bOk) {
        bOk = getPatternKeys(pattern, vec);
        if (bOk) {
            bOk = delKeys(vec);
        }
    } else {
        MyErrorLog("del_pattern_keys status err: ctx=[%p]%s| pattern=%s|"
            " msg=reconnect first, and then try again.", 
            ctx,
            m_key.c_str(),
            pattern);
    }

    if (!bOk) {
        reconn();
    }

    return bOk;
}

bool RedisCtx::getPatternKeys(const char pattern[], const char except[],
    std::vector<std::string>& vec) {
    bool bOk = false;
    redisReply* reply = NULL;
    std::string exceptStr = except;

    reply = (redisReply*)redisCommand(ctx, "KEYS %s", pattern);
    if (NULL != reply) {
        bOk = true;
        
        if (REDIS_REPLY_ARRAY == reply->type) {
            for (int i=0; i<(int)(reply->elements); i++) {
                std::string strTmp = reply->element[i]->str;
                if(exceptStr.empty())
                {
                    vec.push_back(strTmp);
                }
                else if(-1 == (int)strTmp.find(except))
                {
                    vec.push_back(strTmp);
                }
            }
        }
        
        freeReplyObject(reply);

        MyTestLog("get_pattern_keys_ok| pattern=%s| size=%d|",
            pattern,
            (int)(vec.size()));
    }

    return bOk;
}

bool RedisCtx::getPatternKeys(const char pattern[], 
    std::vector<std::string>& vec) {
    bool bOk = false;
    redisReply* reply = NULL;

    reply = (redisReply*)redisCommand(ctx, "KEYS %s", pattern);
    if (NULL != reply) {
        bOk = true;
        
        if (REDIS_REPLY_ARRAY == reply->type) {
            for (int i=0; i<(int)(reply->elements); i++) {
                vec.push_back(reply->element[i]->str);
            }
        }
        
        freeReplyObject(reply);

        MyTestLog("get_pattern_keys_ok| pattern=%s| size=%d|",
            pattern,
            (int)(vec.size()));
    }

    return bOk;
}

bool RedisCtx::delKeys(const std::vector<std::string>& vec) {
    bool bOk = true;
    redisReply* reply = NULL;

    for (int i=0; i<(int)(vec.size()); i++) {
        reply = (redisReply*)redisCommand(ctx, "DEL %s", vec[i].c_str());
        if (NULL != reply) {
            MyTestLog("del_key_ok| node=%s| key[%d]=%s| ret=%d|",
                m_key.c_str(),
                i,
                vec[i].c_str(),
                (int)(reply->integer));

            freeReplyObject(reply);
        } else {
            MyTestLog("del_key_err| node=%s| key[%d]=%s| ret=null|",
                m_key.c_str(),
                i,
                vec[i].c_str());
            bOk = false;
            break;
        }
    }

    return bOk;
}

bool RedisCtx::appendCmd(const char format[], va_list ap){
    bool bOk = true;
    GuardLock guard(m_lock);

    bOk = chkValid();
    if (bOk) {
        bOk = appendV(format, ap);
    }else {
        MyErrorLog("append status err: cmd=%s| cur_cnt=%u|"
            " ctx=[%p]%s|  status=%d|"
            " msg=appendV status err.", 
            format,
            m_appCnt,
            ctx,
            m_key.c_str(),
            status);
    }

    if (!bOk) {
        reconn();
    }

    return bOk;
}

bool RedisCtx::queryDebug(const char cmd[]){
    bool bOk = true;
    GuardLock guard(m_lock);

    bOk = chkValid();
    if (bOk) {
        bOk = execute(cmd);
    }else {
        MyTestLog("query debug status err: cmd=%s| cur_cnt=%u|"
            " ctx=[%p]%s|  status=%d|"
            " msg=query debug status err.", 
            cmd,
            m_appCnt,
            ctx,
            m_key.c_str(),
            status);
    }

    if (!bOk) {
        reconn();
    }

    return bOk;
}

redisReply* RedisCtx::executeV(const char format[], va_list ap){
    redisReply* reply = NULL;
    
    reply = (redisReply*)redisvCommand(ctx,format,ap);
    if (NULL != reply) {        
        MyTestLog("executeV reply ok: type=%d| reply=%s| format=%s|"
            " ctx=[%p]%s| status=%d|", 
            reply->type,
            NULL == reply->str ? "NULL" : reply->str,
            format,
            ctx,
            m_key.c_str(),
            status);
    } else {
        MyTestLog("executeV reply err: reply=NULL| format=%s|"
            " ctx=[%p]%s| status=%d|", 
            format,
            ctx,
            m_key.c_str(),
            status);
    }
    
    return reply;
}

bool RedisCtx::chkValid(){
    if (NULL != ctx && CTX_OK == status){
        return true;
    } else {
        return false;
    }
}

bool RedisCtx::appendV(const char format[], va_list ap){
    int ret = 0;
    bool bOk = true;
    
    ret = redisvAppendCommand(ctx,format,ap);
    if (REDIS_OK == ret) {
        m_appCnt++;

        MyTestLog("append ok: ret=%d| cmd=%s| cur_cnt=%u|"
            " ctx=[%p]%s|  status=%d|"
            " msg=appendV cmd ok.", 
            ret,
            format,
            m_appCnt,
            ctx,
            m_key.c_str(),
            status);

        if (MAX_COMMIT_CNT <= m_appCnt) {
            bOk = getReplies();
        }
    } else {
        bOk = false;
        MyTestLog("*******************************"
            " appendV err: ret=%d| cmd=%s| cur_cnt=%u|"
            " ctx=[%p]%s|  status=%d|"
            " msg=appendV cmd execute err.", 
            ret,
            format,
            m_appCnt,
            ctx,
            m_key.c_str(),
            status);
    }

    return bOk;
}

bool RedisCtx::commit(){
    bool bOk = true;
    GuardLock guard(m_lock);
    
    bOk = chkValid();
    if (bOk) {
        bOk = getReplies();
    } else {
        MyErrorLog("commit status err: cur_cnt=%u|"
            " ctx=[%p]%s| status=%d|"
            " msg=fail to commit and reconnect.", 
            m_appCnt,
            ctx,
            m_key.c_str(),
            status);
    }

    if (!bOk) {
        reconn();
    }

    return bOk;
}

bool RedisCtx::getReplies(){
    int ret = 0;
    unsigned int cnt = 0;
    bool bOk = true;
    redisReply** replies = NULL;
    
    if (0 < m_appCnt) {
        replies = (redisReply**)malloc(sizeof(redisReply*) * m_appCnt);
        for (cnt=0; cnt<m_appCnt; cnt++) {
            ret = redisGetReply(ctx, (void**)&replies[cnt]);
            if (REDIS_OK == ret) {
            } else {
                break;
            }
        }

        for (unsigned int i=0; i<cnt; i++) {
            if (NULL != replies[i]) {
                infoReply(replies[i]);
                freeReplyObject(replies[i]);
            }
        }

        free(replies);                
        if (REDIS_OK == ret) {
            bOk = true;
            MyTestLog("commit ok: ret=%d| total_cmd=%u| cur_cnt=%u|"
                " ctx=[%p]%s| status=%d|"
                " msg=commit ok.|", 
                ret,
                m_appCnt,
                cnt,
                ctx,
                m_key.c_str(),
                status);
        } else {
            bOk = false;
            MyTestLog("commit err: ret=%d| total_cmd=%u| cur_cnt=%u|"
                " ctx=[%p]%s| status=%d|"
                " msg=commit err.|", 
                ret,
                m_appCnt,
                cnt,
                ctx,
                m_key.c_str(),
                status);
        }
        
        m_appCnt = 0;
    }else {
        bOk = true;
        MyTestLog("commit ok: cur_cnt=%u|"
            " ctx=[%p]%s| status=%d| msg=no data to commmit|", 
            m_appCnt,
            ctx,
            m_key.c_str(),
            status);
    }

    return bOk;
}

void RedisCtx::infoReply(redisReply* r){
    if(r->type == REDIS_REPLY_ERROR)
    {
        MyErrorLog("error reply: type=%d| reply=%s|",
            r->type,
            NULL == r->str ? "NULL" : r->str);
    }
    else
    {
        MyTestLog("info reply: type=%d| reply=%s|",
            r->type,
            NULL == r->str ? "NULL" : r->str);
    }
}

bool RedisCtx::execute(const char cmd[]) {
    bool bOk = true;
    redisReply* reply = NULL;
   
    reply = (redisReply*)redisCommand(ctx, cmd);
    if (NULL != reply) {  
        if (REDIS_REPLY_ERROR != reply->type) {
            MyTestLog("execute reply: type=%d| reply=%s| cmd=%s| node=%s|", 
                reply->type,
                NULL == reply->str ? "NULL" : reply->str,
                cmd,
                m_key.c_str());
        } else {
            bOk = false;
            MyTestLog("execute reply err: type=%d| reply=%s| cmd=%s| node=%s|", 
                reply->type,
                reply->str,
                cmd,
                m_key.c_str());
        }

        freeReplyObject(reply);
    } else {
        MyTestLog("execute reply err: cmd=%s| node=%s|", cmd, m_key.c_str());
        bOk = false;
    }
    
    return bOk;
}

void RedisCtx::close(){
    if (NULL != ctx){
        redisFree(ctx);
        ctx = NULL;
    }

    status = CTX_CLOSE;
    m_appCnt = 0;
}

AsynCtx::AsynCtx() {
    timeout = 0;
    ctx = NULL;
    status = CTX_INIT;
    m_port= 0;
    m_task_cnt = 0;
    m_lock = new ThreadLock();
}

AsynCtx::~AsynCtx() {
    close();

    if (NULL != m_lock){
        delete m_lock;
    }
}

void AsynCtx::setHost(const std::string& ip, int port, int n){    
    m_ip = ip;
    m_port = port;
    m_index = n;
    m_key = genKey(m_ip, m_port, m_index, MAX_VNODE_CNT);
}

bool AsynCtx::reconn() {
    close();
    return conn();
}

bool AsynCtx::reconnSafe() {
    GuardLock guard(m_lock);

    return reconn();
}

bool AsynCtx::chkValid() {
    if (NULL != ctx && CTX_OK == status){
        return true;
    } else {
        return false;
    }
}

bool AsynCtx::conn() {
    bool bOk = true;
    
    ctx = redisAsyncConnect(m_ip.c_str(), m_port);
    if (NULL == ctx) {
        MyTestLog("asynconn conn error: ip=%s| port=%d| timeout=%d|",
            m_ip.c_str(), m_port, timeout);

        return false;
    }

    if (0 != ctx->err) {
        MyTestLog("asynconn conn error: err=%d| ip=%s| port=%d| timeout=%d|",
            ctx->err, m_ip.c_str(), m_port, timeout);

        close();
        return false;
    }
    
    status = CTX_OK;
    AsynDbPool::registerEvent(ctx, this);

    MyTestLog("++start to wait conn: redis=%p| ctx=%p| db=%s|",
        this, ctx, m_key.c_str());
    
    bOk = selectDb();
    if (bOk) {
        MyTestLog("++start to wait conn: redis=%p| ctx=%p| db=%s|",
            this, ctx, m_key.c_str());
    } else {
        MyTestLog("---conn err: redis=%p| ctx=%p| db=%s|",
            this, ctx, m_key.c_str());

        close();
    }
    return bOk;
}

bool AsynCtx::close(){
    if (NULL != ctx) {
        redisAsyncDisconnect(ctx);
        ctx = NULL;
    }

    status = CTX_CLOSE;
    m_task_cnt = 0;
    return true;
}

bool AsynCtx::closeSafe(){
    GuardLock guard(m_lock);
    
    return close();
}

bool AsynCtx::selectDb() {
    int ret = 0;

    ret = redisAsyncCommand(ctx, &AsynCtx::callbackSelect,
            this, "select %d", m_index);
    if (REDIS_OK == ret) {
        m_task_cnt++;
        return true;
    } else {
        return false;
    }
}

bool AsynCtx::operation(const char format[], va_list ap) { 
    int ret = 0;
    bool bOk = true;
    GuardLock guard(m_lock);
    
    bOk = chkValid();
    if (bOk) {
        ret = redisvAsyncCommand(ctx, &AsynCtx::callbackCmdSet, 
            this, format, ap);
        if (REDIS_OK == ret) {
            m_task_cnt++;
        } else {
            bOk = false;
        }
    }

    if (bOk) {
        MyTestLog("execute cmd ok: cnt=%u|"
            " ctx=%p[%s]| status=%d| format=%s|"
            " msg=ok.", 
            m_task_cnt,
            ctx,
            m_key.c_str(),
            status,
            format);
    } else {
        MyTestLog("execute cmd err: cnt=%u"
            " ctx=%p[%p]| status=%d| format=%s|"
            " msg=error.", 
            m_task_cnt,
            ctx,
            this,
            status,
            format);

        reconn();
    }

    return bOk;
}

void AsynCtx::callbackSelect(redisAsyncContext *c, void *r, void *data){
    AsynCtx* ac = reinterpret_cast<AsynCtx*>(data);
    redisReply *reply = reinterpret_cast<redisReply*>(r);
    int type = -1;
    std::string rsp;
    bool bOk = false;

    if (NULL != reply) {
        type = reply->type;
        
        if (NULL != reply->str) {
            rsp = reply->str;
            if (0 == strcmp(reply->str, "OK")) {
                bOk = true;
            }
        }
    }

    if (!bOk) {
        ac->closeSafe();
    }

     MyTestLog("select callback: ctx=%p[%p]| type=%d|"
        " reply=%s| db=%s|", 
        c,
        ac, 
        type,
        rsp.c_str(),
        ac->getKey().c_str());
}

void AsynCtx::callbackCmdSet(redisAsyncContext *c, void *r, void *data){
    AsynCtx* ac = reinterpret_cast<AsynCtx*>(data);
    redisReply *reply = reinterpret_cast<redisReply*>(r);
    int type = -1;
    std::string rsp;

    if (NULL != reply) {
        type = reply->type;
        
        if (NULL != reply->str) {
            rsp = reply->str;
        }
    } else {
        ac->closeSafe();
    }

    MyTestLog("cmd callback: ctx=%p[%p]| type=%d|"
        " reply=%s| db=%s|", 
        c,
        ac, 
        type,
        rsp.c_str(),
        ac->getKey().c_str());
}

std::string genKey(const std::string& ip, int port, int index, int vnode){
    char buf[256] = {0};

    snprintf(buf, sizeof(buf), "REDIS:%s:%d:%d-VNODE:%d", 
        ip.c_str(),
        port,
        index,
        vnode);
    return buf;
}

}

