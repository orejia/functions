#include<cstring>
#include<cstdio>
#include"Log.h"
#include"DbBase.h"
#include"DbConn.h"


namespace pool{

DbPool* DbPool::g_pool = NULL;
DbRead* DbRead::g_pool = NULL;
DbWrite* DbWrite::g_pool = NULL;

DbPool::DbPool(){
    m_write = new DbBase();
    m_read = new DbBase();
}

DbPool::~DbPool(){
    if (NULL != m_read){
        delete m_read;
    }

    if (NULL != m_write){
        delete m_write;
    }
}

redisReply* DbPool::operation(const char key[], const char format[], ...){
    redisReply* reply = NULL;
    va_list ap;

    va_start(ap, format);
    reply = m_read->operation(key, format, ap);
    va_end(ap);

    return reply;
}

void DbPool::queryDebug(const char key[], const char cmd[]){
    m_read->queryDebug(key, cmd);
}


void DbPool::delPatternKeys(const char pattern[], const char except[]) {
    m_read->delPatternKeys(pattern, except);
}

void DbPool::delPatternKeys(const char pattern[]) {
    m_read->delPatternKeys(pattern);
}

bool DbPool::appendCmd(const char key[], const char format[], ...){
    bool bOk = true;
    va_list ap;

    va_start(ap, format);
    bOk = m_write->appendCmd(key, format, ap);
    va_end(ap);    

    return bOk;
}

void DbPool::commit(const char key[]){
    m_write->commit(key);
}

void DbPool::commit(){
    m_write->commit();
}

DbPool* DbPool::instance(){
    if (NULL != g_pool){
        return g_pool;
    } else {
        g_pool = new DbPool();
        return g_pool;
    }
}

bool DbPool::init(const std::string& path){
    bool bOk = true;

    bOk = m_read->init(path);
    if (bOk) {
        bOk = m_write->init(path);
    }
    
    return bOk;
}

DbRead* DbRead::instance(){
    if (NULL != g_pool){
        return g_pool;
    } else {
        g_pool = new DbRead();
        return g_pool;
    }
}

DbRead::DbRead(){
    m_base = new DbBase();
}

DbRead::~DbRead(){
    if (NULL != m_base){
        delete m_base;
    }
}

bool DbRead::init(const std::string& path){
    bool bOk = true;

    bOk = m_base->init(path);
    return bOk;
}

redisReply* DbRead::operation(const char key[], const char format[], ...){
    redisReply* reply = NULL;
    va_list ap;

    va_start(ap, format);
    reply = m_base->operation(key, format, ap);
    va_end(ap);

    return reply;
}

redisReply* DbRead::operationCmd(const char key[], const char cmd[]) {
    return m_base->operationCmd(key, cmd);
}

void DbRead::queryDebug(const char key[], const char cmd[]){
    m_base->queryDebug(key, cmd);
}

DbWrite* DbWrite::instance(){
    if (NULL != g_pool){
        return g_pool;
    } else {
        g_pool = new DbWrite();
        return g_pool;
    }
}

DbWrite::DbWrite(){
    m_base = new DbBase();
}

DbWrite::~DbWrite(){
    if (NULL != m_base){
        delete m_base;
    }
}

bool DbWrite::init(const std::string& path){
    bool bOk = true;

    bOk = m_base->init(path);
    return bOk;
}

bool DbWrite::appendCmd(const char key[], const char format[], ...){
    bool bOk = true;
    va_list ap;

    va_start(ap, format);
    bOk = m_base->appendCmd(key, format, ap);
    va_end(ap);    

    return bOk;
}

void DbWrite::commit(const char key[]){
    m_base->commit(key);
}

void DbWrite::commit(){
    m_base->commit();
}

}

