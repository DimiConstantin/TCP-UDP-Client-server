#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include <netinet/tcp.h>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

#include "utils.h"
#include "common.h"

#define MAXCONN 512

void notify_subscribers(const char *topic_name, const char *response, size_t response_len ,std::unordered_map<std::string, std::unordered_set<std::string>> &topic_to_client_ids,
                         std::unordered_map<std::string, client_t> &id_to_client, std::vector<struct pollfd> &pfds,
                         std::unordered_map<std::string, client_t> &id_to_client_map, std::unordered_map<int, client_t> &fd_to_client)
{
    auto it = topic_to_client_ids.find(topic_name);
    if (it == topic_to_client_ids.end()) {
        return;
    }
    
    for (const auto &client_id : it->second) {
        auto cit = id_to_client.find(client_id);
        if (cit != id_to_client.end() && cit->second.is_connected) {
            int client_fd = cit->second.fd;
            send_tcp_msg(
                client_fd,
                TCP_MSG_SERVER_RESPONSE,
                (const uint8_t*)response,
                response_len
            );
        }
    }
}

void run_server(int tcp_socket_fd, int udp_socket_fd, std::unordered_map<std::string, client_t> &id_to_client,
                std::unordered_map<int, client_t> &fd_to_client, std::unordered_map<std::string, std::unordered_set<std::string>> &topic_to_client_ids)
{
    // listen for incoming connections
    std::vector<struct pollfd> pfds;

    pfds.push_back({tcp_socket_fd, POLLIN, 0});
    pfds.push_back({udp_socket_fd, POLLIN, 0});
    pfds.push_back({STDIN_FILENO, POLLIN, 0});

    while (true)
    {
        int rc = poll(pfds.data(), pfds.size(), -1);
        DIE(rc < 0, "poll");

        for (size_t i = 0; i < pfds.size(); ++i)
        {
            if (pfds[i].revents & POLLIN)
            {
                if (pfds[i].fd == tcp_socket_fd)
                {
                    // handle TCP connection
                    struct sockaddr_in client_addr;
                    socklen_t addr_len = sizeof(client_addr);
                    int client_fd = accept(tcp_socket_fd, (struct sockaddr *)&client_addr, &addr_len);
                    DIE(client_fd < 0, "accept");

                    // disable Nagle algorithm
                    int enable = 1;
                    int rc = setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
                    DIE(rc < 0, "setsockopt");

                    tcp_msg_t msg;
                    rc = recv_tcp_msg(client_fd, &msg);
                    DIE(rc < 0, "recv");
                    // std::cout << "Received message of type " << (int)msg.type << "and length" << ntohs(msg.len) << '\n';

                    std::string client_id = std::string((char *)msg.payload);
                    client_id[msg.len - 1] = '\0';

                    // std::cout << "Client ID: " << client_id << '\n';

                    // create a new client entry
                    struct client_t client;
                    client.fd = client_fd;
                    client.id = std::string(client_id);
                    client.is_connected = true;
                    client.tcp_port = ntohs(client_addr.sin_port);

                    auto it = id_to_client.find(client_id);
                    if (it != id_to_client.end() && it->second.is_connected)
                    {
                        std::cout << "Client " << client_id << " already connected." << std::endl;
                        close(client_fd);
                        continue;
                    }

                    pfds.push_back({client_fd, POLLIN, 0});
                    fd_to_client[client_fd] = client;
                    id_to_client[client_id] = client;
                    std::cout << "New client " << client_id << " connected from "
                              << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << std::endl;
                }
                else if (pfds[i].fd == udp_socket_fd)
                {
                    // handle UDP packet
                    struct sockaddr_in client_addr;
                    socklen_t addr_len = sizeof(client_addr);
                    char buffer[UDP_PKT_SIZE];
                    memset(buffer, 0, sizeof(buffer));
                    ssize_t bytes_received = recvfrom(udp_socket_fd, buffer, UDP_PKT_SIZE, 0,
                                                      (struct sockaddr *)&client_addr, &addr_len);
                    DIE(bytes_received < 0, "recvfrom");
                    
                    topic_t *topic = (topic_t *)buffer;
                    char *topic_name = topic->topic;
                    unsigned int topic_type = (unsigned int)topic->type;
                    // std::cout << "received UDP packet with topic " << topic_name << " and type " << topic_type << "and value " << topic->payload << '\n';
                    char response[UDP_PKT_SIZE];
                    memset(response, 0, sizeof(response));
                    size_t response_len;
                    switch (topic_type)
                    {
                    case INT:
                     {
                        // handle integer topic
                        int8_t sign = (uint8_t)topic->payload[0];
                        int aux;
                        memcpy(&aux, topic->payload + 1, sizeof(int));
                        int value = ntohl(aux);
                        if (sign == 1)
                        {
                            value = -value;
                        }
                        // std::cout << "Received integer topic: " << topic_name << " with value: " << value << '\n';
                        snprintf(response, sizeof(response), "%s:%d - %s - %s - %d", inet_ntoa(client_addr.sin_addr),
                             ntohs(client_addr.sin_port), topic_name, "INT", value);
                        // std::cout << "Response: " << response << '\n';
                        break;
                     }
                    case SHORT_REAL:
                    {
                        // handle short real topic
                        uint16_t aux;
                        memcpy(&aux, topic->payload, sizeof(uint16_t));
                        double value = ntohs(aux) / 100.0;
                        // std::cout << "Received short real topic: " << topic_name << " with value: " << value << '\n';
                        snprintf(response, sizeof(response), "%s:%d - %s - %s - %.2f", inet_ntoa(client_addr.sin_addr),
                             ntohs(client_addr.sin_port), topic_name, "SHORT_REAL", value);
                        // std::cout << "Response: " << response << '\n';
                        break;
                    }
                    case FLOAT:
                    {
                        // handle float topic
                        uint8_t sign = (uint8_t)topic->payload[0];
                        uint32_t aux;
                        memcpy(&aux, topic->payload + 1, sizeof(uint32_t));
                        uint32_t num = ntohl(aux);
                        uint8_t exponent = (uint8_t)topic->payload[5];
                        double value = (double)num;
                        while (exponent--)
                            value /= 10.0;
                        if (sign == 1)
                        {
                            value = -value;
                        }

                        // std::cout << "Received float topic: " << topic_name << " with value: " << value << '\n';
                        snprintf(response, sizeof(response), "%s:%d - %s - %s - %.15g", inet_ntoa(client_addr.sin_addr),
                             ntohs(client_addr.sin_port), topic_name, "FLOAT", value);
                        // std::cout << "Response: " << response << '\n';
                        break;
                    }
                    case STRING:
                    {
                        // handle string topic
                        std::string res(topic->payload);
                        res[res.size()] = '\0';
                        // std::cout << "Received string topic: " << topic_name << " with value: " << res << '\n';
                        snprintf(response, sizeof(response), "%s:%d - %s - %s - %s", inet_ntoa(client_addr.sin_addr),
                             ntohs(client_addr.sin_port), topic_name, "STRING", res.c_str());
                        // std::cout << "Response: " << response << '\n';
                        break;
                    }
                    default:
                        std::cout << "Unknown topic type: " << topic_type << std::endl;
                        break;
                    }
                    
                    // char response[UDP_PKT_SIZE];
                    response_len = strlen(response);
                    response[response_len] = '\0';
                    notify_subscribers(topic_name, response, response_len, topic_to_client_ids, id_to_client, pfds, id_to_client, fd_to_client);
                             
                }
                else if (pfds[i].fd == STDIN_FILENO)
                {
                    // handle stdin input
                    char buffer[1024];
                    fgets(buffer, sizeof(buffer), stdin);

                    if (strncmp(buffer, "exit", 4) == 0)
                    {
                        // std::cout << "Exiting server...\n";
                        for (auto &entry : pfds)
                        {
                            if (entry.fd != tcp_socket_fd && entry.fd != udp_socket_fd)
                            {
                                close(entry.fd);
                            }
                        }
                        return;
                    }
                }
                else
                {
                    // handle TCP client message
                    int client_fd = pfds[i].fd;
                    tcp_msg_t packet;
                    rc = recv_tcp_msg(client_fd, &packet);
                    if (rc <= 0)
                    {
                        // client disconnected
                        // std::cout << "Client disconnected\n";
                        client_t client = fd_to_client[client_fd];
                        // std::cout << "Client ID: " << client.id << "cu connected " << client.is_connected << '\n';
                        client.is_connected = false;
                        id_to_client[client.id].is_connected = false;
                        close(client_fd);
                        pfds.erase(pfds.begin() + i);
                        continue;
                    }

                    // std::cout << "Received message of type " << (int)packet.type << " and length " << ntohs(packet.len) << '\n';
                    client_t client = fd_to_client[client_fd];
                    switch (packet.type)
                    {
                    case TCP_MSG_SUBSCRIBE:
                    {
                        // handle subscribe message
                        // std::cout << "Client " << client.id << " subscribed to topic: " << packet.payload << '\n';
                        std::string topic;
                        size_t len = ntohs(packet.len) - 1;
                        topic.assign((char *)packet.payload, len);
                        // topic[len] = '\0';
                        // std::cout << "Topic: " << topic << '\n';
                        client.topics.insert(topic);
                        topic_to_client_ids[topic].insert(client.id);

                        // for (const auto &c : topic_to_client_ids[topic])
                        // {
                        //     std::cout << "Client " << c << " is subscribed to topic: " << topic << '\n';
                        // }

                        break;
                    }
                    case TCP_MSG_UNSUBSCRIBE:
                    {
                        // handle unsubscribe message
                        // std::cout << "Client " << client.id << " unsubscribed from topic: " << packet.payload << '\n';
                        std::string topic;
                        size_t len = ntohs(packet.len) - 1;
                        topic.assign((char *)packet.payload, len);
                        // std::cout << "Unsubscribing from topic " << topic << "\n";
                        client.topics.erase(topic);
                        topic_to_client_ids[topic].erase(client.id);
                        break;
                    }
                    case TCP_MSG_EXIT:
                    {
                        // handle exit message
                        std::cout << "Client " << client.id << " disconnected." << std::endl;
                        client.is_connected = false;
                        id_to_client[client.id].is_connected = false;
                        close(client_fd);
                        pfds.erase(pfds.begin() + i);
                        break;
                    }
                    default:
                        break;
                    }
                }
            }
        }
    }
}

int main(int argc, char *argv[])
{

    if (argc != 2)
    {
        perror("Usage: ./server <port>");
        return 1;
    }

    
    std::ios::sync_with_stdio(true);
    // fiecare << pe cout face și flush automat
    std::cout.setf(std::ios::unitbuf);

    // deactivate stdout buffering
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    std::unordered_map<std::string, client_t> id_to_client;
    std::unordered_map<int, client_t> fd_to_client;
    std::unordered_map<std::string, std::unordered_set<std::string>> topic_to_client_ids;

    // read port from command line
    uint16_t port;
    int rc = sscanf(argv[1], "%hu", &port);
    DIE(port < 1024 || rc != 1, "invalid port");

    // std:: cout << "Starting server on port " << port << '\n';

    // create udp and tcp sockets
    const int tcp_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcp_socket_fd < 0, "tcp socket");

    const int udp_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udp_socket_fd < 0, "udp socket");

    // set up the server address structure
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    // set socket options
    int enable = 1;
    rc = setsockopt(tcp_socket_fd, IPPROTO_TCP, TCP_NODELAY, (char *)&enable, sizeof(int));
    DIE(rc < 0, "setsockopt tcp");

    rc = bind(tcp_socket_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "bind tcp socket");

    rc = bind(udp_socket_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "bind udp socket");

    listen(tcp_socket_fd, MAXCONN);

    run_server(tcp_socket_fd, udp_socket_fd, id_to_client, fd_to_client, topic_to_client_ids);

    // close sockets
    rc = close(tcp_socket_fd);
    DIE(rc < 0, "close tcp socket");

    rc = close(udp_socket_fd);
    DIE(rc < 0, "close udp socket");

    rc = close(STDIN_FILENO);
    DIE(rc < 0, "close stdin");

    return 0;
}
