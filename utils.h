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

struct __attribute__((packed)) topic_t 
{
    char topic[51];
    uint8_t type;
    char payload[1501];
};

struct client_t
{
	std::string id;
	int fd;
	int tcp_port;
	bool is_connected;
	std::unordered_set<std::string> topics;
};