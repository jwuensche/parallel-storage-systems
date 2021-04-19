/* ************************************************************************ */
/* Include standard header file.                                            */
/* ************************************************************************ */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <omp.h>

#define N 360

/* time measurement variables */
struct timeval start_time;       /* time when program started               */
struct timeval comp_time;        /* time when calculation complet           */

/* ************************************************************************ */
/*  allocate matrix of size N x N                                           */
/* ************************************************************************ */
static
double**
alloc_matrix (void)
{
	double** matrix;
	int i;

	// allocate pointer array to the beginning of each row
	// rows
	matrix = malloc(N * sizeof(double*));

	if (matrix == NULL)
	{
		printf("Allocating matrix lines failed!\n");
		exit(EXIT_FAILURE);
	}

	// allocate memory for each row and assig it to the frist dimensions pointer
	// columns
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
/*  free matrix								                                 */
/* ************************************************************************* */
static void free_matrix (double **matrix)
{
	int i;

	// free memory for allocated for each row
	for (i = 0; i < N; i++)
	{
		free(matrix[i]);
	}
	// free pointer array to the beginning of each row
	free(matrix);
}

/* ************************************************************************* */
/*  init matrix									                             */
/* ************************************************************************* */
static
void
init_matrix (double** matrix)
{
	int i, j;

	srand(time(NULL));

	for (i = 0; i < N; i++)
	{
		for (j = 0; j < N; j++)
		{
			matrix[i][j] = 10 * j;
		}
	}
}

/* ************************************************************************* */
/*  init matrix reading file					         	                             */
/* ************************************************************************* */
static
void
read_matrix (double** matrix)
{
	(void)matrix;
	// TODO read
}

/* ************************************************************************ */
/*  calculate                                                               */
/* ************************************************************************ */
static
void
calculate (double** matrix, int iterations, int threads)
{
	int i, j, k, l;
	int tid;
	int lines, from, to;

	tid = 0;
	lines = from = to = -1;

	// Explicitly disable dynamic teams
	omp_set_dynamic(0);
	omp_set_num_threads(threads);

	#pragma omp parallel firstprivate(tid, lines, from, to) private(k, l, i, j)
	{
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

			// TODO write

			#pragma omp barrier
		}
	}
}

/* ************************************************************************ */
/*  displayStatistics: displays some statistics                             */
/* ************************************************************************ */
static
void
displayStatistics (void)
{
	double time = (comp_time.tv_sec - start_time.tv_sec) + (comp_time.tv_usec - start_time.tv_usec) * 1e-6;

	printf("Runtime:    %fs\n", time);
	printf("Throughput: %f MB/s\n", 0.0);
	printf("IOPS:       %f Op/s\n", 0.0);
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

	if (0)
	{
		read_matrix(matrix);
	}
	else
	{
		init_matrix(matrix);
	}

	gettimeofday(&start_time, NULL);
	calculate(matrix, iterations, threads);
	gettimeofday(&comp_time, NULL);

	displayStatistics();

	free_matrix(matrix);

	return 0;
}
