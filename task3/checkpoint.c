/* ************************************************************************ */
/* Include standard header file.                                            */
/* ************************************************************************ */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <omp.h>

#define N 360

/* time measurement variables */
struct timeval start_time;       /* time when program started               */
struct timeval comp_time;        /* time when calculation complet           */

size_t io_bytes;
uint64_t io_ops;
double io_time;

/* ************************************************************************ */
/*  allocate matrix of size N x N                                           */
/* ************************************************************************ */
static
double**
alloc_matrix (void)
{
	double** matrix;
	int i;

	matrix = malloc(N * sizeof(double*));

	if (matrix == NULL)
	{
		printf("Allocating matrix lines failed!\n");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < N; i++)
	{
		matrix[i] = malloc(N * sizeof(double));

		if (matrix[i] == NULL)
		{
			printf("Allocating matrix elements failed!\n");
			exit(EXIT_FAILURE);
		}
	}

	return matrix;
}

/* ************************************************************************* */
/*  init matrix 																                             */
/* ************************************************************************* */
static
void
init_matrix (double** matrix)
{
	int i, j;

	for (i = 0; i < N; i++)
	{
		for (j = 0; j < N; j++)
		{
			matrix[i][j] = 10 * j;
		}
	}
}

/* ************************************************************************ */
/*  caluclate                                                               */
/* ************************************************************************ */
static
void
calculate (double** matrix, int iterations, int threads)
{
	int i, j, k, l;
	int tid;
	int lines, from, to;
	int fd;
	size_t lnb = 0;
	uint64_t io_counter = 0;
	double iotime_counter = 0.0;

	tid = 0;
	lines = from = to = -1;

	// Explicitly disable dynamic teams
	omp_set_dynamic(0);
	omp_set_num_threads(threads);

	fd = open("matrix.out", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	if (fd == -1)
	{
		fprintf(stderr, "Could not open checkpoint ...\n");
		exit(EXIT_FAILURE);
	}

	#pragma omp parallel firstprivate(tid, lines, from, to) \
	                     private(k, l, i, j) \
	                     reduction(+:io_counter, iotime_counter, lnb)
	{
		struct timeval io_start_time;
		struct timeval io_end_time;

		iotime_counter = 0.0;
		lnb = 0;
		io_counter = 0;

		tid = omp_get_thread_num();

		lines = (tid < (N % threads)) ? ((N / threads) + 1) : (N / threads);
		from =  (tid < (N % threads)) ? (lines * tid) : ((lines * tid) + (N % threads));
		to = from + lines;

		for (k = 1; k <= iterations; k++)
		{
			for (i = from; i < to; i++)
			{
				for (j = 0; j < N; j++)
				{
					for (l = 1; l <= 4; l++)
					{
						matrix[i][j] = cos(matrix[i][j]) * sin(matrix[i][j]) * sqrt(matrix[i][j]) / tan(matrix[i][j]) / log(matrix[i][j]) * k * l;
					}
				}
			}

			gettimeofday(&io_start_time, NULL);

			{ // write
				size_t nb = 0;

				for (i = from; i < to; i++)
				{
					nb = 0;

					while (nb < (N * sizeof(double)))
					{
						nb += pwrite(fd, (char*)matrix[i] + nb, (N * sizeof(double)) - nb, (i * N * sizeof(double)) + nb);
						io_counter++;
					}

					lnb += nb;
				}
			}

			gettimeofday(&io_end_time, NULL);
			iotime_counter += (io_end_time.tv_sec - io_start_time.tv_sec) + (io_end_time.tv_usec - io_start_time.tv_usec) * 1e-6;

			#pragma omp barrier
		}
	}

	io_ops = io_counter;
	io_time = iotime_counter / threads;
	io_bytes = lnb;

	close(fd);
}

/* ************************************************************************ */
/*  displayStatistics: displays some statistics                             */
/* ************************************************************************ */
static
void
displayStatistics (void)
{
	double time = (comp_time.tv_sec - start_time.tv_sec) + (comp_time.tv_usec - start_time.tv_usec) * 1e-6;

	printf("Runtime:    %fs\n", time - io_time);
	printf("I/O time:   %fs\n", io_time);
	printf("Throughput: %f MB/s\n", io_bytes / 1024 / 1024 / io_time);
	printf("IOPS:       %f Op/s\n", io_ops / io_time);
}

/* ************************************************************************ */
/*  main                                                                    */
/* ************************************************************************ */
int
main (int argc, char** argv)
{
	int threads, iterations;
	double** matrix;

	if (argc < 3)
	{
		printf("Usage: %s threads iterations\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	else
	{
		sscanf(argv[1], "%d", &threads);
		sscanf(argv[2], "%d", &iterations);
	}

	matrix = alloc_matrix();
	init_matrix(matrix);

	gettimeofday(&start_time, NULL);
	calculate(matrix, iterations, threads);
	gettimeofday(&comp_time, NULL);

	displayStatistics();

	return 0;
}
