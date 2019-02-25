#ifndef __COMMON_UTIL__
#define __COMMON_UTIL__

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <uuid/uuid.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef container_of
/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})
#endif

#ifndef arraysize
#define arraysize(x) (sizeof(x) / sizeof(((x)[0])))
#endif

int set_nonblocking(int fd, int nonblocking);
int set_reuse(int fd, int reuse);
int listen_on1(const char *ip, uint16_t port, int nonblocking, int backlog);
int listen_on2(uint32_t ip, uint16_t port, int nonblocking, int backlog);
static inline const char* ip2str_r(uint32_t ip, char *buf)
{
    sprintf(buf, "%hhu.%hhu.%hhu.%hhu",
        ((uint8_t*)&ip)[0], ((uint8_t*)&ip)[1],
        ((uint8_t*)&ip)[2], ((uint8_t*)&ip)[3]);

    return buf;
}

int strip2int(const char *ip);



int tcp_connect_server(const char* server_ip, int port, int noblock);


int get_ipv4_address(const char *addr, uint32_t *ip, uint16_t *port, char *err_msg, size_t err_msg_len);

int get_ipv4_by_name(const char *name, uint32_t *ip, char *err_msg, size_t err_msg_len);

int to_printable(const void *data, uint32_t len, char *output, uint32_t output_len);
int __to_printable(const void *data, uint32_t len, char *output, uint32_t output_len, uint32_t *written_char_count);

const char* dump_hex(const void *data, uint32_t len, char *output, uint32_t output_len);

int dump_hex1(const void *data, uint32_t len, char *output, uint32_t output_len, int with_blank_space);
int __dump_hex1(const void *data, uint32_t len, char *output, uint32_t output_len, int with_blank_space, uint32_t *written_char_count);

int dump_hex2(const void *data, uint32_t len, char *output, uint32_t output_len);

static inline int str2i64(const char *str, int len, int64_t *i64, char *err_msg, size_t err_msg_len)
{
    if (-1 == len)
    {
        len = (int)strlen(str);
    }

    if (!len)
    {
        if (err_msg)
        {
            snprintf(err_msg, err_msg_len, "String is empty");
        }
        return -1;
    }

    char *endptr;
    errno = 0;
    int64_t ll = strtoll(str, &endptr, 10);
    if (errno)
    {
        if (err_msg)
        {
            snprintf(err_msg, err_msg_len, "%s", strerror(errno));
        }
        return -2;
    }
    if (endptr != &str[len])
    {
        if (err_msg)
        {
            snprintf(err_msg, err_msg_len, "One or more trailing character(s) is/are invalid");
        }
        return -3;
    }

    *i64 = ll;
    return 0;
}

static inline int str2i32(const char *str, int len, int32_t *i32, char *err_msg, size_t err_msg_len)
{
    int64_t i64;
    int result = str2i64(str, len, &i64, err_msg, err_msg_len);
    if (result < 0)
    {
        return result;
    }

    if (i64 < INT_MIN || i64 > INT_MAX)
    {
        if (err_msg)
        {
            snprintf(err_msg, err_msg_len, "%s", strerror(ERANGE));
        }
        return -2;
    }

    *i32 = (int32_t)i64;
    return 0;
}

static inline int str2ui32(const char *str, int len, uint32_t *ui32, char *err_msg, size_t err_msg_len)
{
    int64_t i64;
    int result = str2i64(str, len, &i64, err_msg, err_msg_len);
    if (result < 0)
    {
        return result;
    }

    if (i64 < 0 || i64 > UINT_MAX)
    {
        if (err_msg)
        {
            snprintf(err_msg, err_msg_len, "%s", strerror(ERANGE));
        }
        return -2;
    }

    *ui32 = (uint32_t)i64;
    return 0;
}

static inline int str2ui16(const char *str, int len, uint16_t *ui16, char *err_msg, size_t err_msg_len)
{
    int64_t i64;
    int result = str2i64(str, len, &i64, err_msg, err_msg_len);
    if (result < 0)
    {
        return result;
    }

    if (i64 < 0 || i64 > USHRT_MAX)
    {
        if (err_msg)
        {
            snprintf(err_msg, err_msg_len, "%s", strerror(ERANGE));
        }
        return -2;
    }
    
    *ui16 = (uint16_t)i64;
    return 0;
}

static inline int str2i16(const char *str, int len, int16_t *i16, char *err_msg, size_t err_msg_len)
{
    int64_t i64;
    int result = str2i64(str, len, &i64, err_msg, err_msg_len);
    if (result < 0)
    {
        return result;
    }

    if (i64 < SHRT_MIN || i64 > SHRT_MAX)
    {
        if (err_msg)
        {
            snprintf(err_msg, err_msg_len, "%s", strerror(ERANGE));
        }
        return -2;
    }
    
    *i16 = (int16_t)i64;
    return 0;
}

static inline int str2ui8(const char *str, int len, uint8_t *ui8, char *err_msg, size_t err_msg_len)
{
    int64_t i64;
    int result = str2i64(str, len, &i64, err_msg, err_msg_len);
    if (result < 0)
    {
        return result;
    }

    if (i64 < 0 || i64 > UCHAR_MAX)
    {
        if (err_msg)
        {
            snprintf(err_msg, err_msg_len, "%s", strerror(ERANGE));
        }
        return -2;
    }

    *ui8 = (uint8_t)i64;
    return 0;
}

static inline int str2i8(const char *str, int len, int8_t *i8, char *err_msg, size_t err_msg_len)
{
    int64_t i64;
    int result = str2i64(str, len, &i64, err_msg, err_msg_len);
    if (result < 0)
    {
        return result;
    }

    if (i64 < SCHAR_MIN || i64 > SCHAR_MAX)
    {
        if (err_msg)
        {
            snprintf(err_msg, err_msg_len, "%s", strerror(ERANGE));
        }
        return -2;
    }
    
    *i8 = (int8_t)i64;
    return 0;
}

int str2hex(const char *str, int len, void *hex);
void lowerstr(char *dst, const char *src);

static inline int gcd(int a, int b)
{
    int c;
    for (c = a % b; c > 0; c = a % b)
    {
        a = b;
        b = c;
    }

    return b;
}

int ignore_signals(const int *signals, size_t count);

uint64_t uptime();
//void generate_token128(void *token);

#ifdef __linux__
int redirect_stdfilenos();
int daemonize(int noclose, const char *new_cwd);
int get_cpu_core_count();
int get_exe_dir(char *dir, int dir_len);

#define likely(x)        __builtin_expect(!!(x), 1)
#define unlikely(x)      __builtin_expect(!!(x), 0)
#else
#define likely(x)        (x)
#define unlikely(x)      (x)
#endif

#ifdef __cplusplus
}
#endif

#endif  // __FRAME_UTIL__
