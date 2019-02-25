#ifndef __PACKET_HEADER__
#define __PACKET_HEADER__

#include <stdint.h>

#pragma pack(push, 8)

typedef struct queue_packet_header_t
{
    uint32_t key_len;
    uint32_t cmd_len;
}queue_packet_header_t;


#pragma pack(pop)

#endif

