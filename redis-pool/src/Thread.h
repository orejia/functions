#ifndef __THREAD_H__
#define __THREAD_H__

#include<pthread.h>


namespace pool {

class Runnable {
public:
    virtual ~Runnable(){}

    virtual void run() = 0;
};

class Thread {
public:
    explicit Thread(Runnable* r) : task(r){thr=0;}
    Thread(): task(NULL){thr=0;}
    virtual ~Thread(){}
    
    void start(){
        bRun = true;
        pthread_create(&thr, NULL, &activate, this);
    }

    void join(){
        if (0 != thr) {
            pthread_join(thr, NULL);
            thr = 0;
        }
    }

    void stop(){
        bRun = false;
    }

    bool isRun()const{return bRun;}

protected:
    virtual void run(){}
    
    static void* activate(void* arg) {
        Thread* t = reinterpret_cast<Thread*>(arg);

        if (NULL != t->task) {
            t->task->run();
        } else {
            t->run();
        }
        
        t->thr = 0;
        t->bRun = false;
        return NULL;
    }

private:
    bool bRun;
    pthread_t thr;
    Runnable* task;
};

}

#endif

