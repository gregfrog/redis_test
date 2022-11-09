all: server

server: server.c
	gcc -fstack-protector-all -pthread -lpthread -o server server.c

clean:
	rm server
