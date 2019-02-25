#ifndef __THREADLOCK_H__
#define __THREADLOCK_H__

#include<pthread.h>

namespace pool {

class ThreadLock {
public:
    ThreadLock(){
        m_mutex = new pthread_mutex_t;
        pthread_mutex_init(m_mutex, NULL);
    }

    ~ThreadLock(){
        if (NULL != m_mutex) {
            delete m_mutex;
        }
    }

    void lock(){
        pthread_mutex_lock(m_mutex);
    }

    void unlock(){
        pthread_mutex_unlock(m_mutex);
    }
private:
    pthread_mutex_t* m_mutex;
};

class GuardLock {
public:
    explicit GuardLock(ThreadLock* lock) : m_lock(lock){m_lock->lock();}
    ~GuardLock(){m_lock->unlock();}

private:
    ThreadLock* m_lock;
};

}

#endif
