#include "redis_protocol.h"

int parse_cmd(char **target, const char *format, ...) 
{
    va_list ap;
    int len;
    va_start(ap,format);
    len = redisvFormatCommand(target,format,ap);
    va_end(ap);

    /* The API says "-1" means bad result, but we now also return "-2" in some
     * cases.  Force the return value to always be -1. */
    if (len < 0)
        len = -1;
    return len;
}

int append_cmd(const void *key, const char *format, ...)
{
    if(key == NULL)
    {
        return -1;
    }
    
    char *cmd = NULL;

    va_list ap;
    int len;
    va_start(ap,format);
    len = redisvFormatCommand(&cmd,format,ap);
    va_end(ap);
    if(len < 0)
    {
        printf("redis command format error.\n");
        return -1;
    }
    printf("len = %d\n", len);
    free(cmd);
    return 0;
}


