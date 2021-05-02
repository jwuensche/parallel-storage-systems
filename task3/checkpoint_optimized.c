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
#include <stdatomic.h>

#include <omp.h>

#define N 360
#define SEC_TO_NANO 1000000000

/* time measurement variables */
struct timeval start_time;       /* time when program started               */
struct timeval comp_time;        /* time when calculation complet           */

/*
** Put all measurement values into a dedicated structure, this makes handling them more readable
*/
struct MatrixInfo {
	size_t io_bytes;
	uint64_t io_ops;
	float io_time;
	float io_fsync;
	int thread_count;
	int iteration;
	/*
	** Values describing containing the absolute time used in nanoseconds
	** Should be enough for round about 500 years
	*/
	unsigned long long iotime_counter;
	unsigned long long fsynctime_counter;
};

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

inline unsigned long long nano_diff(struct timespec l, struct timespec r) {
	return (r.tv_sec - l.tv_sec) * SEC_TO_NANO + (r.tv_nsec - l.tv_nsec);
}

/* ************************************************************************ */
/*  caluclate                                                               */
/* ************************************************************************ */
static
void
calculate (double** matrix, struct MatrixInfo* info) {
	int i, j, k, l;
	int tid;
	int lines, from, to;
	int fd;
	size_t lnb = 0;
	uint64_t io_counter = 0;
	// In nano seconds this is atleast valid for 500+ years, so we shoulde be fine...
	unsigned long long iotime_counter = 0;
	unsigned long long fsynctime_counter = 0;

	tid = 0;
	lines = from = to = -1;

	// Explicitly disable dynamic teams
	omp_set_dynamic(0);
	omp_set_num_threads(info->thread_count);

	fd = open("matrix.out", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	if (fd == -1)
	{
		fprintf(stderr, "Could not open checkpoint ...\n");
		exit(EXIT_FAILURE);
	}

	#pragma omp parallel firstprivate(tid, lines, from, to) \
	                     private(k, l, i, j) \
	                     reduction(+:io_counter, iotime_counter, fsynctime_counter, lnb)
	{
		struct timespec io_start_time;
		struct timespec io_end_time;
		struct timespec fsync_start_time;
		struct timespec fsync_end_time;


		lnb = 0;
		io_counter = 0;

		tid = omp_get_thread_num();

		lines = (tid < (N % info->thread_count)) ? ((N / info->thread_count) + 1) : (N / info->thread_count);
		from =  (tid < (N % info->thread_count)) ? (lines * tid) : ((lines * tid) + (N % info->thread_count));
		to = from + lines;

		for (k = 1; k <= info->iteration; k++)
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

			clock_gettime(CLOCK_TAI, &io_start_time);

			{ // write
				// size_t nb = 0;

				for (i = from; i < to; i++)
				{
					/*
					** This part can be greatly optimized by reducing the amount of write calls necessary to the file descriptor.
					** Each call adds some overhead especially since the changes which have been get updated in parallel and each iteration calls
					** fsync.
					**
					** The first step would be simply write down the results in a manner which does not require writing every single double value
					** out singularly, after this the synchronization of the file descriptor maybe be moved to the end.
					** Additionally only one thread may call the synchronization as more are not necessary.
					*/
					// nb = 0;
					// while (nb < (N * sizeof(double)))
					// {
					// 	nb += pwrite(fd, (char*)matrix[i] + nb, (N * sizeof(double)) - nb, (i * N * sizeof(double)) + nb);
					// 	clock_gettime(CLOCK_TAI, &fsync_start_time);
					// 	fsync(fd);
					// 	clock_gettime(CLOCK_TAI, &fsync_end_time);
					// 	fsynctime_counter += ((unsigned long long)(fsync_end_time.tv_sec - fsync_start_time.tv_sec))
					// 		* SEC_TO_NANO
					// 		+ (unsigned long long)nano_diff(fsync_start_time.tv_nsec, fsync_end_time.tv_nsec);
					// 	io_counter++;
					// }
					// lnb += nb

					lnb += pwrite(fd, matrix[i], N * sizeof(double), (i * N * sizeof(double)));
					io_counter++;
				}
			}

			#pragma omp barrier

			/*
			** Synchronize on first thread to reduce it to a minimum.
			** In this case we simply take advantage of the cache, which allows for non-yet performed
			** write operations, as our buffer and flush this buffer in the end with a single `fsync`.
			**
			** This carries the advantage compared to the previous implementation of lesser syncs in total reducing overhead
			** and by this increasing efficiency.
			*/
			if (tid == 0) {
				clock_gettime(CLOCK_TAI, &fsync_start_time);
				fsync(fd);
				clock_gettime(CLOCK_TAI, &fsync_end_time);
				fsynctime_counter += nano_diff(fsync_start_time, fsync_end_time);
			}

			clock_gettime(CLOCK_TAI, &io_end_time);
			iotime_counter += nano_diff(io_start_time, io_end_time);
		}
	}

	info->io_ops = io_counter;
	info->io_time = (float)iotime_counter / SEC_TO_NANO;
	info->io_fsync = (float)fsynctime_counter / SEC_TO_NANO;
	info->io_bytes = lnb;
	info->iotime_counter = iotime_counter;
	info->fsynctime_counter = fsynctime_counter;

	// printf("Time spend waiting (total): %llus %lluns\n", iotime_counter / SEC_TO_NANO, iotime_counter % SEC_TO_NANO);
	// printf("Time spend waiting (fsync): %llus %lluns\n", fsynctime_counter / SEC_TO_NANO, fsynctime_counter % SEC_TO_NANO);
	// printf("Proportionally: %f\n",(float) fsynctime_counter / (float) iotime_counter);

	close(fd);
}

/* ************************************************************************ */
/*  displayStatistics: displays some statistics                             */
/* ************************************************************************ */
static
void
displayStatistics (struct MatrixInfo* info)
{
	double time = (comp_time.tv_sec - start_time.tv_sec) + (comp_time.tv_usec - start_time.tv_usec) * 1e-6;

	/*
	** For better usage lets use unix style combinations here, the output from stdout from this
	** application can be piped later on to a file or similar...
	**
	** All outputs are in units, seconds whereever applicable.
	** Optionally we might output
	*/
	printf("threads,iterations,wallclock,io_time,fsync_time,io_bytes,iop\n");
	printf("%d,%d,%f,%f,%f,%lu,%lu\n",
		   info->thread_count,
		   info->iteration,
		   time,
		   info->io_time,
		   info->io_fsync,
		   info->io_bytes,
		   info->io_ops
		   );
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

	struct MatrixInfo info;
	info.thread_count = threads;
	info.iteration = iterations;

	matrix = alloc_matrix();
	init_matrix(matrix);

	gettimeofday(&start_time, NULL);
	calculate(matrix, &info);
	gettimeofday(&comp_time, NULL);

	displayStatistics(&info);

	return 0;
}
