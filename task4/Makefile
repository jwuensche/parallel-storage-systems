CC     = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -g
LDLIBS = -lm

all: checkpoint dummyfs

checkpoint: CFLAGS += -fopenmp
checkpoint: checkpoint.c

dummyfs: CFLAGS += $(shell pkg-config --cflags fuse3)
dummyfs: LDLIBS += $(shell pkg-config --libs fuse3)
dummyfs: dummyfs.c

clean:
	$(RM) checkpoint
	$(RM) dummyfs
