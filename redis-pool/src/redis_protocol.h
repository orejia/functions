#ifndef __REDIS_PROTOCOL_H__
#define __REDIS_PROTOCOL_H__

#include <hiredis/hiredis.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>

//parse redis requset 
int parse_cmd(char **target, const char *format, ...)
#if defined (__GNUC__)
  __attribute__((format(printf, 2, 3)))
#endif
;

int append_cmd(const void *key, const char *format, ...)
#if defined (__GNUC__)
  __attribute__((format(printf, 2, 3)))
#endif
;

#endif
