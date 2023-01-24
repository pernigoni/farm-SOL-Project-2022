CC 		= gcc
CFLAGS 	+= -D_POSIX_C_SOURCE=200112L -std=c99 -g -pedantic -Wall -I ./includes

TARGETS	= bin/farm bin/generafile

.PHONY : clean test testloop mytest

all : $(TARGETS)

debug : CFLAGS += -D DEBUG=1
debug : all

bin/generafile : src/generafile.c
	$(CC) $(CFLAGS) $^ -o $@

bin/farm : src/farm.c objs/collector.o libs/libutils.a libs/libboundedqueue.a libs/libthreadpool.a
	$(CC) $(CFLAGS) $^ -o $@ -L ./libs/ -lutils -L ./libs/ -lboundedqueue -L ./libs/ -lthreadpool -lpthread

objs/collector.o : src/collector.c includes/collector.h libs/libutils.a
	$(CC) $(CFLAGS) -c -o $@ $< -L ./libs/ -lutils

libs/libthreadpool.a : objs/threadpool.o
	ar rvs $@ $^

objs/threadpool.o : src/threadpool.c includes/threadpool.h libs/libboundedqueue.a
	$(CC) $(CFLAGS) $< -c -o $@ -L ./libs/ -lboundedqueue

libs/libboundedqueue.a : objs/boundedqueue.o
	ar rvs $@ $^

objs/boundedqueue.o : src/boundedqueue.c includes/boundedqueue.h libs/libutils.a
	$(CC) $(CFLAGS) $< -c -o $@ -L ./libs/ -lutils

libs/libutils.a : objs/utils.o
	ar rvs $@ $^

objs/utils.o : src/utils.c includes/utils.h
	$(CC) $(CFLAGS) $< -c -o $@

clean :
	-rm -f $(TARGETS) objs/*.o libs/*.a *~ core expected.txt *.dat -r testdir

test :
	@./test.sh

testloop :
	@for i in 0 1 2 3 4 5 6 7 8 9 ; do \
		echo "=== ESEGUO I TEST ===" ; \
		./test.sh ; done

mytest :
	@./my_test.sh