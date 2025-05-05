#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <iostream>

#include "common.h"
#include "utils.h"

void subscriber(int sockfd)
{
    struct pollfd pfds[2];
    pfds[0].fd = STDIN_FILENO;
    pfds[0].events = POLLIN;
    pfds[1].fd = sockfd;
    pfds[1].events = POLLIN;

    char line[1600];
    char *cmd;
    char *arg;

    // read messages from server
    while (true)
    {
        int rc = poll(pfds, 2, -1);
        DIE(rc < 0, "poll");

        if (pfds[0].revents & POLLIN)
        {
            if (!fgets(line, sizeof(line), stdin))
            {
                break;
            }

            size_t len = strlen(line);
            line[len - 1] = '\0';

            cmd = strtok(line, " \n");
            DIE(!cmd, "strtok failed");

            if (strcmp(cmd, "exit") == 0)
            {
                // send exit message to server
                send_tcp_msg(sockfd, TCP_MSG_EXIT, NULL, 0);
                break;
            }
            else if (strcmp(cmd, "subscribe") == 0)
            {
                arg = strtok(NULL, " ");
                send_tcp_msg(sockfd, TCP_MSG_SUBSCRIBE, (const uint8_t *)arg, strlen(arg));
                std::cout << "Subscribed to topic " << arg << std::endl;
            }
            else if (strcmp(cmd, "unsubscribe") == 0)
            {
                arg = strtok(NULL, " ");
                send_tcp_msg(sockfd, TCP_MSG_UNSUBSCRIBE, (const uint8_t *)arg, strlen(arg));
                std::cout << "Unsubscribed from topic: " << arg << std::endl;
            }
        }
        if (pfds[1].revents & POLLIN)
        {
            // read message from server
            tcp_msg_t msg;
            rc = recv_tcp_msg(sockfd, &msg);
            if (rc <= 0)
                break;
            std::cout << msg.payload << std::endl;
        }

    }
}

    int main(int argc, char *argv[])
    {

        if (argc != 4)
        {
            fprintf(stderr, "Usage: %s <ID_CLIENT> <IP_SERVER> <PORT_SERVER>\n", argv[0]);
            return EXIT_FAILURE;
        }

        // deactivate stdout buffering
        setvbuf(stdout, NULL, _IONBF, BUFSIZ);


        // read arguments
        char *id_client = argv[1];
        id_client[strlen(id_client)] = '\0';

        const char *ip_server = argv[2];

        int port = atoi(argv[3]);
        DIE(port < 1024, "Invalid port number");

        // create TCP socket
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        DIE(sockfd < 0, "TCP socket");

        // set up the server address structure
        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
        inet_pton(AF_INET, ip_server, &serv_addr.sin_addr);

        // connect to server
        int rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
        DIE(rc < 0, "connect");

        // disable Nagle algorithm
        int flag = 1;
        setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

        // send ID_CLIENT to server using tcp message

        rc = send_tcp_msg(sockfd, TCP_MSG_INIT_ID, (const uint8_t *)id_client, strlen(id_client));
        DIE(rc < 0, "send ID");

        subscriber(sockfd);

        close(sockfd);
        return 0;
    }
