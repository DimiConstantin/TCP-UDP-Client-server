CC = g++
CFLAGS = -Werror -g

build: server subscriber

server: server.cpp common.cpp
	$(CC) $(CFLAGS) -o server server.cpp common.cpp

subscriber: subscriber.cpp common.cpp
	$(CC) $(CFLAGS) -o subscriber subscriber.cpp common.cpp

clean:
	rm -f server subscriber

