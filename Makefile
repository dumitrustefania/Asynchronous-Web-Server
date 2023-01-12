CC = gcc
CPPFLAGS = -DDEBUG -DLOG_LEVEL=LOG_DEBUG -I. -I.. -I../..
CFLAGS = -Wall -Wextra -g
LDLIBS = -laio

.PHONY: build clean

build: aws

aws: aws.o utils/sock_util.o http-parser/http_parser.o

aws.o: aws.c utils/sock_util.h utils/debug.h utils/util.h

sock_util.o: sock_util.c utils/sock_util.h utils/debug.h utils/util.h

http-parser/http_parser.o: http-parser/http_parser.c http-parser/http_parser.h
	make -C http-parser/ http_parser.o

clean:
	-rm -f *~
	-rm -f *.o
	-rm -f utils/sock_util.o
	-rm -f http-parser/http_parser.o
	-rm -f aws