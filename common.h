#pragma once
#include <stdint.h>

#define TCP_MSG_SUBSCRIBE 0
#define TCP_MSG_UNSUBSCRIBE 1
#define TCP_MSG_EXIT 2
#define TCP_MSG_INIT_ID 3

#define MAX_TCP_MSG_SIZE 1500

typedef struct  __attribute__((packed))
{
    uint16_t len;
    uint8_t type;
    uint8_t payload[MAX_TCP_MSG_SIZE];
} tcp_msg_t;

int send_all(int sockfd, void *buffer, int len);

int recv_all(int sockfd, void *buffer, int len);

bool send_tcp_msg(int sockfd, uint8_t type, const uint8_t *payload, size_t payload_size);

bool recv_tcp_msg(int sockfd, tcp_msg_t *msg);
