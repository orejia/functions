#ifndef __LOG_H__
#define __LOG_H__

#include<cstdio>

namespace pool {

#ifdef _DEBUG_
#define MyTestLog(format, ...) do{std::printf( format, ##__VA_ARGS__ );   \
    std::printf("\n");}while(false)
#define MyErrorLog MyTestLog
#define MyInfoLog MyTestLog

#elif defined(_DEBUG_T)
#define MyTestLog(format, ...) 
#define MyErrorLog MyTestLog
#define MyInfoLog MyTestLog

#else 
#define MyTestLog(format, args...) \
    logMsg( TEST, "%s:%d:%s: "format"\n", __FILE__, __LINE__, __FUNCTION__, ##args )

#define MyErrorLog(format, args...) \
    logMsg( ERROR, "%s:%d:%s: "format"\n", __FILE__, __LINE__, __FUNCTION__, ##args )

#define MyInfoLog(format, args...) \
    logMsg( INFO, "%s:%d:%s: "format"\n", __FILE__, __LINE__, __FUNCTION__, ##args )

#endif
}

typedef enum tagLogType {
    ERROR = 1,
    INFO  = 2,
    DEBUG = 3,
    TEST = 4
}LOG_TYPE;

extern void logMsg( LOG_TYPE type, const char *fmt, ... );

#endif

