#include "util.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>
#include <errno.h>
#include <string.h>

#ifndef __linux__
// iOS
#include <mach/mach_time.h>
#endif

static const char* const g_hex_str[] =
{
"00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "0a", "0b", "0c", "0d", "0e", "0f",
"10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "1a", "1b", "1c", "1d", "1e", "1f",
"20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "2a", "2b", "2c", "2d", "2e", "2f",
"30", "31", "32", "33", "34", "35", "36", "37", "38", "39", "3a", "3b", "3c", "3d", "3e", "3f",
"40", "41", "42", "43", "44", "45", "46", "47", "48", "49", "4a", "4b", "4c", "4d", "4e", "4f",
"50", "51", "52", "53", "54", "55", "56", "57", "58", "59", "5a", "5b", "5c", "5d", "5e", "5f",
"60", "61", "62", "63", "64", "65", "66", "67", "68", "69", "6a", "6b", "6c", "6d", "6e", "6f",
"70", "71", "72", "73", "74", "75", "76", "77", "78", "79", "7a", "7b", "7c", "7d", "7e", "7f",
"80", "81", "82", "83", "84", "85", "86", "87", "88", "89", "8a", "8b", "8c", "8d", "8e", "8f",
"90", "91", "92", "93", "94", "95", "96", "97", "98", "99", "9a", "9b", "9c", "9d", "9e", "9f",
"a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "a8", "a9", "aa", "ab", "ac", "ad", "ae", "af",
"b0", "b1", "b2", "b3", "b4", "b5", "b6", "b7", "b8", "b9", "ba", "bb", "bc", "bd", "be", "bf",
"c0", "c1", "c2", "c3", "c4", "c5", "c6", "c7", "c8", "c9", "ca", "cb", "cc", "cd", "ce", "cf",
"d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "d9", "da", "db", "dc", "dd", "de", "df",
"e0", "e1", "e2", "e3", "e4", "e5", "e6", "e7", "e8", "e9", "ea", "eb", "ec", "ed", "ee", "ef",
"f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "fa", "fb", "fc", "fd", "fe", "ff"
};

static const char g_printable_char[] =
{
'.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.',
'.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.',
' ', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/',
'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',
'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~', '.',
'.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.',
'.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.',
'.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.',
'.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.',
'.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.',
'.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.',
'.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.',
'.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.'
};

int to_printable(const void *data, uint32_t len, char *output, uint32_t output_len)
{
    return __to_printable(data, len, output, output_len, NULL);
}

int __to_printable(const void *data, uint32_t len, char *output, uint32_t output_len, uint32_t *written_char_count)
{
    if (!output_len)
    {
        return -1;
    }

    uint32_t i;
    for (i = 0; i < len && i < output_len - 1; ++ i)
    {
        output[i] = g_printable_char[((uint8_t*)data)[i]];
    }

    output[i] = '\0';
    if (written_char_count)
    {
        *written_char_count = i;
    }
    return (i == len) ? 0 : 1;
}

const char* dump_hex(const void *data, uint32_t len, char *output, uint32_t output_len)
{
    __dump_hex1(data, len, output, output_len, 1, NULL);
    return output;
}

// 0:  all content output
// 1:  portion content output
// -1: error
int dump_hex1(const void *data, uint32_t len, char *output, uint32_t output_len, int with_blank_space)
{
    return __dump_hex1(data, len, output, output_len, with_blank_space, NULL);
}

int strip2int(const char *ip)
{
    return inet_addr(ip);
}


int tcp_connect_server(const char* server_ip, int port, int nonblock)
{
  int sockfd, status, save_errno;
  struct sockaddr_in server_addr;

  memset(&server_addr, 0, sizeof(server_addr) );

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  status = inet_aton(server_ip, &server_addr.sin_addr);

  if( status == 0 ) //the server_ip is not valid value
  {
    errno = EINVAL;
    return -1;
  }

  sockfd = socket(PF_INET, SOCK_STREAM, 0);
  if( sockfd == -1 )
    return sockfd;
  
  status = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr) );

  if( status == -1 )
  {
    save_errno = errno;
    close(sockfd);
    errno = save_errno; //the close may be error
    return -1;
  }

  if(nonblock == 1)
  {
     set_nonblocking(sockfd,1);
  }
  return sockfd;
}


// 0:  all content output
// 1:  portion content output
// -1: error
int __dump_hex1(const void *data, uint32_t len, char *output, uint32_t output_len, int with_blank_space, uint32_t *written_char_count)
{
    if (!output_len)
    {
        return -1;
    }

    uint32_t i, j, k;
    for (i = 0, j = 0, k = j; i < len && j + 3 <= output_len; ++ i)
    {
        *(uint16_t*)&output[j] = *(uint16_t*)g_hex_str[((uint8_t*)data)[i]];
        j += 2;
        k = j;
        if (with_blank_space)
        {
            output[j ++] = ' ';
        }
    }

    output[k] = '\0';
    if (written_char_count)
    {
        *written_char_count = k;
    }
    return (i == len) ? 0 : 1;
}

// 0:  all content output
// 1:  portion content output
// -1: error
int dump_hex2(const void *data, uint32_t len, char *output, uint32_t output_len)
{
/* output format
0000: 00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f  ................
0010: 10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f  ................
*/
    if (!output_len)
    {
        return -1;
    }

    uint32_t base_addr_width = 2;
    if (len)
    {
        uint32_t i;
        for (base_addr_width = 2, i = len - 1; (i >>= 8); base_addr_width += 2)
        {
            // empty
        }
    }

    int offset = 0;
    uint32_t j, byte_count;
    for (j = 0; j < len; j += byte_count)
    {
        byte_count = len - j;
        if (byte_count > 16)
        {
            byte_count = 16;
        }

        if (!output_len)
        {
            return 1;
        }
        int result = snprintf(&output[offset], output_len, "%0*x: ", base_addr_width, j);
        if (result >= output_len)
        {
            // truncated
            return 1;
        }

        offset += result;
        output_len -= result;

        uint32_t written_char_count;
        result = __dump_hex1((char*)data + j, byte_count, &output[offset], output_len, 1, &written_char_count);
        if (result)
        {
            return 1;
        }

        offset += written_char_count;
        output_len -= written_char_count;

        if (output_len < (16 - byte_count) * 3 + 3)
        {
            return 1;
        }

        memset(&output[offset], ' ', (16 - byte_count) * 3);
        offset += (16 - byte_count) * 3;
        output_len -= (16 - byte_count) * 3;

        output[offset ++] = ' ';
        output[offset ++] = ' ';
        output[offset] = '\0';
        output_len -= 2;

        result = __to_printable((char*)data + j, byte_count, &output[offset], output_len, &written_char_count);
        if (result)
        {
            return 1;
        }

        offset += written_char_count;
        output_len -= written_char_count;

        if (output_len < 2)
        {
            return 1;
        }

        output[offset ++] = '\n';
        -- output_len;
    }

    output[offset ++] = '\0';

    return 0;
}

int str2hex(const char *str, int len, void *hex)
{
    if (-1 == len)
    {
        len = (int)strlen(str);
    }

    char ch;
    unsigned char val;
    int state = 1, i;
    int byte_count = 0;
    unsigned char *temp = (unsigned char*)hex;

    for (i = 0; i < len; ++ i)
    {
        ch = str[i];

        if (' ' == ch)
        {
            if (2 == state)
            {
                return -1;
            }
            continue;
        }
        else if (ch >= '0' && ch <= '9')
        {
            val = ch - '0';
        }
        else if (ch >= 'A' && ch <= 'F')
        {
            val = ch - 'A' + 10;
        }
        else if (ch >= 'a' && ch <= 'f')
        {
            val = ch - 'a' + 10;
        }
        else
        {
            return -2;
        }

        if (1 == state)
        {
            temp[byte_count] = val * 16;
            state = 2;
        }
        else
        {
            temp[byte_count ++] += val;
            state = 1;
        }
    }

    if (2 == state)
    {
        return -1;
    }
    
    return byte_count;
}

void lowerstr(char *dst, const char *src)
{
    while (*src)
    {
        if (*src >= 'A' && *src <= 'Z')
        {
            *dst = *src + ('a' - 'A');
        }
        else
        {
            *dst = *src;
        }
        ++ src;
        ++ dst;
    }

    *dst = '\0';
}

int set_reuse(int fd, int reuse)
{
    int option;
    socklen_t len = (socklen_t)sizeof(option);
    if (-1 == getsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &option, &len))
    {
        return -1;
    }

    if (reuse)
    {
        if (!option)
        {
            option = 1;
            if (-1 == setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &option, (socklen_t)sizeof(option)))
            {
                return -1;
            }
        }
    }
    else
    {
        if (option)
        {
            option = 0;
            if (-1 == setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &option, (socklen_t)sizeof(option)))
            {
                return -1;
            }
        }
    }
    return 0;
}

int set_nonblocking(int fd, int nonblocking)
{
    int flag = fcntl(fd, F_GETFL);
    if (-1 == flag)
    {
        return -1;
    }

    if (nonblocking)
    {
        if (!(flag & O_NONBLOCK))
        {
            if (fcntl(fd, F_SETFL, flag | O_NONBLOCK))
            {
                return -1;
            }
        }
    }
    else
    {
        if (flag & O_NONBLOCK)
        {
            if (fcntl(fd, F_SETFL, flag & (~O_NONBLOCK)))
            {
                return -1;
            }
        }
    }
    return 0;
}

int listen_on2(uint32_t ip, uint16_t port, int nonblocking, int backlog)
{
    int fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (-1 == fd)
    {
        return -1;
    }

    if (-1 == set_reuse(fd, 1))
    {
        close(fd);
        return -1;
    }

    if (-1 == set_nonblocking(fd, nonblocking))
    {
        close(fd);
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ip;
    addr.sin_port = htons(port);
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)))
    {
        close(fd);
        return -1;
    }

    if (-1 == listen(fd, backlog))
    {
        close(fd);
        return -1;
    }

    return fd;
}

int listen_on1(const char *ip, uint16_t port, int nonblocking, int backlog)
{
    struct in_addr addr;
    if (!inet_aton(ip, &addr))
    {
        return -1;
    }
    return listen_on2(addr.s_addr, port, nonblocking, backlog);
}

int get_ipv4_by_name(const char *name, uint32_t *ip, char *err_msg, size_t err_msg_len)
{
    struct addrinfo hint;
    memset(&hint, 0x00, sizeof(hint));

    hint.ai_family = PF_INET;

    struct addrinfo *res;
    int result = getaddrinfo(name, NULL, &hint, &res);
    if (result)
    {
        snprintf(err_msg, err_msg_len, "Failed to getaddrinfo: %s", gai_strerror(result));
        return -1;
    }

    *ip = ((struct sockaddr_in*)(res->ai_addr))->sin_addr.s_addr;
    freeaddrinfo(res);
    return 0;
}

int get_ipv4_address(const char *addr, uint32_t *ip, uint16_t *port, char *err_msg, size_t err_msg_len)
{
    char *temp_addr = strdup(addr);
    if (!temp_addr)
    {
        snprintf(err_msg, err_msg_len, "Failed to duplicate string");
        return -1;
    }

    int len = (int)strlen(temp_addr);
    int i;
    for (i = 0; i < len; ++ i)
    {
        if (':' == temp_addr[i])
        {
            temp_addr[i] = '\0';
            break;
        }
    }

    uint32_t temp_ip;
    struct in_addr inp;
    if (!inet_aton(temp_addr, &inp))
    {
        if (-1 == get_ipv4_by_name(temp_addr, &temp_ip, err_msg, err_msg_len))
        {
            free(temp_addr);
            return -1;
        }
    }
    else
    {
        temp_ip = inp.s_addr;
    }

    if (i + 1 < len)
    {
        errno = 0;
        char *endptr;
        int64_t value = strtol(temp_addr + i + 1, &endptr, 10);
        if (errno || endptr != &temp_addr[len] || value < 1 || value > 65535)
        {
            free(temp_addr);
            snprintf(err_msg, err_msg_len, "Invalid port '%s'", temp_addr + i + 1);
            return -1;
        }

        *port = (uint16_t)value;
    }
    else
    {
        *port = 0;
    }

    free(temp_addr);

    *ip = temp_ip;
    
    return 0;
}

/*
void generate_token128(void *token)
{
    uuid_t uuid;
    uuid_generate(uuid);
    memcpy(token, uuid, 16);
}
*/
int ignore_signals(const int *signals, size_t count)
{
    struct sigaction act;
    act.sa_handler = SIG_IGN;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    size_t i;
    for (i = 0; i < count; ++ i)
    {
        if (-1 == sigaction(signals[i], &act, NULL))
        {
            return -1;
        }
    }

    return 0;
}

uint64_t uptime()
{
#ifdef __linux__
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (uint64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
#else
    // iOS
    mach_timebase_info_data_t time_base;
    mach_timebase_info(&time_base);
    return mach_absolute_time() * time_base.numer / time_base.denom;
#endif
}

#ifdef __linux__
int get_cpu_core_count()
{
    errno = 0;
    long core_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (-1 == core_count && EINVAL == errno)
    {
        return -1;
    }

    return (int)core_count;
}

int get_exe_dir(char *dir, int dir_len)
{
    char buf[256];
    int result = (int)readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (-1 == result)
    {
        return -1;
    }

    int i = result - 1;
    for (; result >= 0; -- i)
    {
        if ('/' == buf[i])
        {
            buf[i] = '\0';
            break;
        }
    }

    if (i <= 0 || dir_len < i + 1)
    {
        return -1;
    }

    memcpy(dir, buf, i + 1);
    return 0;
}

int redirect_stdfilenos()
{
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    int fd = open("/dev/null", O_RDWR);
    if (-1 == fd)
    {
        return -1;
    }

    if (-1 == dup2(fd, STDIN_FILENO) || -1 == dup2(fd, STDOUT_FILENO) || -1 == dup2(fd, STDERR_FILENO))
    {
        return -1;
    }

    if (STDIN_FILENO != fd && STDOUT_FILENO != fd && STDERR_FILENO != fd)
    {
        close(fd);
    }

    return 0;
}

int daemonize(int noclose, const char *new_cwd)
{
    struct rlimit rl;
    if (-1 == getrlimit(RLIMIT_NOFILE, &rl))
    {
        return -1;
    }

    pid_t pid = fork();
    if (-1 == pid)
    {
        return -1;
    }
    if (pid > 0)
    {
        exit(0);
    }

    setsid();

    pid = fork();
    if (-1 == pid)
    {
        return -1;
    }
    if (pid > 0)
    {
        exit(0);
    }
    umask(0);

    int signals_to_ignore[] = {SIGHUP, SIGTERM, SIGINT, SIGPIPE, SIGQUIT, SIGTSTP, SIGTTIN, SIGTTOU, SIGUSR1, SIGUSR2};
    if (-1 == ignore_signals(signals_to_ignore, (int)(sizeof(signals_to_ignore) / sizeof(signals_to_ignore[0]))))
    {
        return -1;
    }

    if (new_cwd)
    {
        if (-1 == chdir(new_cwd))
        {
            return -1;
        }
    }

    if (rl.rlim_max == RLIM_INFINITY)
    {
        rl.rlim_max = 1024;
    }
    rlim_t j;
    for (j = 0; j < rl.rlim_max; ++ j)
    {
        if (STDIN_FILENO != (int)j && STDOUT_FILENO != (int)j && STDERR_FILENO != (int)j)
        {
            close((int)j);
        }
    }

    if (!noclose)
    {
        if (-1 == redirect_stdfilenos())
        {
            return -1;
        }
    }

    return 0;
}

#endif
