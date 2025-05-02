#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>

#include "common.h"
#include "utils.h"

int recv_all(int sockfd, void *buffer, int len) {

	int bytes_received = 0;
	int bytes_remaining = len;
	char *buff = (char *)buffer;

  	while (bytes_remaining > 0) {
		int rc = recv(sockfd, buff + bytes_received, bytes_remaining, 0);
		DIE(rc == -1, "recv_all() failed");

		if (!rc)
			return 0;

		bytes_received += rc;
		bytes_remaining -= rc;
  	}

	return bytes_received;
}

int send_all(int sockfd, void *buffer, int len) {
	int bytes_sent = 0;
	int bytes_remaining = len;
	char *buff = (char *)buffer;

	while (bytes_remaining > 0) {
		int rc = send(sockfd, buff + bytes_sent, bytes_remaining, 0);
		DIE(rc == -1, "send_all() failed");

		bytes_sent += rc;
		bytes_remaining -= rc;
	}

  	return bytes_sent;
}

bool send_tcp_msg(int sockfd, uint8_t type, const uint8_t *payload, size_t payload_size) {
    if (payload_size > MAX_TCP_MSG_SIZE)
        return false;
    
    tcp_msg_t msg;
    uint16_t pkt_len = 1 + payload_size;
    msg.len = htons(pkt_len);
    msg.type = type;
    if (payload_size > 0)
        memcpy(msg.payload, payload, payload_size);
    msg.payload[payload_size] = '\0';

    size_t to_send = sizeof(msg.len) + sizeof(msg.type) + payload_size;
    int sent = send_all(sockfd, &msg, to_send);

    return sent == (int)to_send;
}


bool recv_tcp_msg(int sockfd, tcp_msg_t *msg) {
    uint16_t net_len;

    int rc = recv_all(sockfd, &net_len, sizeof(net_len));
    if (rc <= 0)
        return false;

    uint16_t pkt_len = ntohs(net_len);

    if (pkt_len < 1)
        return false;

    size_t payload_size = pkt_len - 1;
    if (payload_size > MAX_TCP_MSG_SIZE)
        return false;
    
    msg->len = net_len;

    rc = recv_all(sockfd, &msg->type, 1);
    if (rc != 1)
        return false;
    
    if (payload_size > 0) {
        rc = recv_all(sockfd, msg->payload, payload_size);
        if (rc != (int)payload_size)
            return false;
    }
    msg->payload[payload_size] = '\0';
    return true;
}