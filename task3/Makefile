CC     = gcc
CFLAGS = -std=c11 -fopenmp -Wall -Wextra -Wpedantic -O2 -g
LDLIBS = -lm

default: checkpoint checkpoint_fsync checkpoint_optimized

checkpoint: checkpoint.c

checkpoint_fsync: checkpoint_fsync.c

checkpoint_optimized: checkpoint_optimized.c

clean:
	$(RM) checkpoint
	$(RM) checkpoint_fsync
	$(RM) checkpoint_optimized
