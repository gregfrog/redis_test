all: server

server: server.c
	gcc -pthread -lpthread -o server server.c

clean:
	rm server
	