CC     = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -g
LDLIBS = -lm

all: metadata memoryfs design

metadata: CFLAGS += $(shell pkg-config --cflags glib-2.0 gthread-2.0)
metadata: LDLIBS += $(shell pkg-config --libs glib-2.0 gthread-2.0)
metadata: metadata.c

memoryfs: CFLAGS += $(shell pkg-config --cflags fuse3)
memoryfs: LDLIBS += $(shell pkg-config --libs fuse3) -lstdc++ -lm
memoryfs: memoryfs.c swisstable/libswisstable.a

swisstable/libswisstable.a:
	git clone https://github.com/taviso/swisstable.git
	cd swisstable && git clone https://github.com/abseil/abseil-cpp.git
	cd swisstable && cmake . && make

design:
	@exec ./build.sh

clean:
	$(RM) metadata
	$(RM) memoryfs
	$(RM) -r swisstable
	$(RM) design.pdf || true
