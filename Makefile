all: server hiredis.o

CC=gcc
CFLAGS=-fstack-protector-all -I ../redis_sentinel -I ../hiredis -g
LDFLAGS=-pthread -lpthread -lhiredis
VPATH=../redis_sentinel
MAKE=make

.c.o:
	$(CC) -c $(CFLAGS) -lz $< -o $@

../hiredis/hiredis.o:
	+$(MAKE) -C ../hiredis all

../hiredis/libhiredis.so:
	+$(MAKE) -C ../hiredis install

redisexample: ../hiredis/hiredis.o 
	+$(MAKE) -C ../hiredis hiredis-example
	docker stop some-redis
	docker run -p 6379:6379 --rm --name some-redis -d redis
	../hiredis/examples/hiredis-example

server: server.o ../redis_sentinel/redis_sentinel.o ../hiredis/libhiredis.so
	$(CC) -L ../hiredis  $< $(LDFLAGS) -o $@

test: server
	- docker stop some-redis
	docker run -p 6379:6379 --rm --name some-redis -d redis
	gdb ./server 

clean:
	rm server
