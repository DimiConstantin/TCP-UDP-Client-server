#include <errno.h>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#define DIE(assertion, call_description)				\
	do {								\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",			\
					__FILE__, __LINE__);		\
			perror(call_description);			\
			exit(errno);					\
		}							\
	} while (0)

#define UDP_PKT_SIZE 1551

#define INT 0
#define SHORT_REAL 1
#define FLOAT 2
#define STRING 3

struct __attribute__((packed)) topic_t 
{
    char topic[50];
    uint8_t type;
    char payload[1500];
};

struct client_t
{
	std::string id;
	int fd;
	int tcp_port;
	bool is_connected;
	std::unordered_set<std::string> topics;
	std::vector<std::vector<std::string>> patterns;
};
