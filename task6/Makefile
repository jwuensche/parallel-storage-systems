CC     = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -g
LDLIBS = -lm

ABSEIL_VERSION="8f92175783c9685045c50f227e7c10f1cddb4d58"

all: data pssfs design

data: CFLAGS += $(shell pkg-config --cflags glib-2.0 gthread-2.0)
data: LDLIBS += $(shell pkg-config --libs glib-2.0 gthread-2.0)
data: data.c

pssfs: CFLAGS += $(shell pkg-config --cflags fuse3 glib-2.0)
pssfs: LDLIBS += $(shell pkg-config --libs fuse3 glib-2.0) -lstdc++ -lm
pssfs: src/pssfs.c block_distributor.o swisstable/libswisstable.a
	$(CC) src/pssfs.c block_distributor.o swisstable/libswisstable.a $(CFLAGS) $(LDLIBS) -o pssfs

block_distributor.o: CFLAGS += $(shell pkg-config --cflags glib-2.0)
block_distributor.o: src/block_distributor.c include/block_distributor.h
	$(CC) -c src/block_distributor.c $(CFLAGS) $(LDLIBS)

swisstable/libswisstable.a:
	git clone https://github.com/jwuensche/swisstable.git
	cd swisstable && git clone https://github.com/abseil/abseil-cpp.git && cd abseil-cpp && git checkout $(ABSEIL_VERSION)
	cd swisstable && cmake . && make -j 8

design:
	@exec ./build.sh

clean:
	$(RM) data
	$(RM) pssfs
	$(RM) -r swisstable
