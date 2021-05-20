CC     = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -g
LDLIBS = -lm

all: metadata memoryfs design

metadata: CFLAGS += $(shell pkg-config --cflags glib-2.0 gthread-2.0)
metadata: LDLIBS += $(shell pkg-config --libs glib-2.0 gthread-2.0)
metadata: metadata.c

memoryfs: CFLAGS += $(shell pkg-config --cflags fuse3)
memoryfs: LDLIBS += $(shell pkg-config --libs fuse3)
memoryfs: memoryfs.c

design:
	@exec ./build.sh

clean:
	$(RM) metadata
	$(RM) memoryfs
	rm design.pdf || true
