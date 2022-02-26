CC = gcc
CFLAGS = -g -c
AR = ar -rc
RANLIB = ranlib

SCHED = RR

all: worker.a

worker.a: worker.o
	$(AR) libworker.a worker.o
	$(RANLIB) libworker.a

worker.o: worker.h

ifeq ($(SCHED), RR)
	$(CC) -pthread $(CFLAGS) worker.c
else ifeq ($(SCHED), MLFQ)
	$(CC) -pthread $(CFLAGS) -DMLFQ worker.c
else
	echo "no such scheduling algorithm"
endif

clean:
	rm -rf testfile *.o *.a
