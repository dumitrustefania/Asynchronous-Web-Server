CPPFLAGS = -DDEBUG -DLOG_LEVEL=LOG_DEBUG -I. -I.. -I../..
CFLAGS = -Wall -g

.PHONY: all clean

build: all

all: epoll_echo_server http_reply_once

epoll_echo_server: epoll_echo_server.o ../sock_util.o

epoll_echo_server.o: epoll_echo_server.c ../sock_util.h ../../debug.h ../../util.h

http_reply_once: http_reply_once.o ../sock_util.o ../../http-parser/http_parser.o

http_reply_once.o: http_reply_once.c ../sock_util.h ../../debug.h ../../util.h

../sock_util.o: ../sock_util.c ../sock_util.h ../../debug.h ../../util.h

../../http-parser/http_parser.o: ../../http-parser/http_parser.c ../../http-parser/http_parser.h
	make -C ../../http-parser/ http_parser.o

clean:
	-rm -f *~
	-rm -f *.o
	-rm -f ../sock_util.o
	-rm -f ../../http-parser/http_parser.o
	-rm -f epoll_echo_server http_reply_once