/*
 * THIS SOLUTION IS INCLUDED FOR REFERENCE AS THE SOLUTION FOR TASK 2.3
 * CHANGED HOW THE THREADS SAVE THEIR DATA BELOW THE PARALLEL WRITING
 * CAN BE SEEN AS REQUESTED BY TASK 2.1
*/


/* ************************************************************************ */
/* Include standard header file.                                            */
/* ************************************************************************ */
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <omp.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <unistd.h>

/* CONSTANTS */
#define N 360
#define COULD_NOT_OPEN 2
#define COULD_NOT_WRITE 3
#define BROKEN_FORMAT 4
#define INVALID_PARAM 5

/* time measurement variables */
struct timeval start_time;       /* time when program started               */
struct timeval comp_time;        /* time when calculation complet           */

struct MatrixInfo {
	int threads;
	int iteration;
	int iterations_completed;
};

/* ************************************************************************ */
/*  Define log functions                                                    */
/* ************************************************************************ */
int log_color(char* code, char* msg) {
	// TODO: Check for negative values here
	fprintf(stderr, "%s", code);
	fprintf(stderr, "%s", msg);
	fprintf(stderr, "\x1b[0m\n");
	return 0;
}

int log_warn(char* msg) {
	return log_color("\x1b[1;33m", msg);
}

int log_err(char* msg) {
	return log_color("\x1b[1;31m", msg);
}

int log_info(char* msg) {
	return log_color("\x1b[32m", msg);
}

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

/* ************************************************************************ */
/*  offsets                                                                 */
/* ************************************************************************ */

const int isch = sizeof(int) * 3;

unsigned long calc_offset(int row) {
	return row * N * sizeof(double) + isch;
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

// I will not hold this as a global value, I want well-defined functions
static
void
read_matrix (double** matrix, struct MatrixInfo* info, char* path)
{
	int file;
	if ((file = open(path, O_RDONLY)) == -1) {
		log_warn("Could not open checkpoint file, assuming it is uninitalized..");
	}

	int threads = 0;
	int iteration_total = 0;
	int iteration_completed = 0;

	if ((pread(file, &threads, sizeof(int), 0)) == -1) {
		log_err("Reading of threads in file failed");
		exit(BROKEN_FORMAT);
	}

	if (threads != info->threads) {
		log_err("Thread amount differes from checkpoint file");
		exit(INVALID_PARAM);
	}

	if (pread(file, &iteration_total, sizeof(int), sizeof(int)) == -1) {
		log_err("Reading of iteration total in file failed");
		exit(BROKEN_FORMAT);
	}

	if (pread(file, &iteration_completed, sizeof(int), sizeof(int) * 2) == -1) {
		log_err("Reading of iterations completed in file failed");
		exit(BROKEN_FORMAT);
	}

	info->iterations_completed = iteration_completed;

	// If the amount of completed iterations are lower than the total iterations in the header, continue...

	if (iteration_completed >= iteration_total && info->iteration <= iteration_total) {
		log_err("All iterations are completed and new iteration count given is smaller or equal than exisiting count");
		exit(INVALID_PARAM);
	}

	if (iteration_completed >= iteration_total && info->iteration > iteration_total) {
		log_info("Checkpoints have been completed before, but the current invocation increases the total iterations");
	}

	for (size_t row = 0; row < N; row += 1) {
		// printf("Reading row %ld\n", row);
		// printf("p: %p, read: %ld, offset: %ld\n", (void*)matrix[row], N * sizeof(double), calc_offset(row));
		if (pread(file, matrix[row], N * sizeof(double), calc_offset(row)) == -1) {
			printf("Reason (errno): %d\n", errno);
			log_err("Could not read matrix values");
			exit(BROKEN_FORMAT);
		}
	}
	close(file);
}

/* ************************************************************************ */
/*  calculate                                                               */
/* ************************************************************************ */
static
void
calculate (double** matrix, struct MatrixInfo* info, char* path)
{
	int i, j, k, l;
	int tid;
	int lines, from, to;

	tid = 0;
	lines = from = to = -1;
	int file_descriptor;
	// open is WELL defined
	if ((file_descriptor = open(path, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR, 420/69)) == -1) {
		log_err("Could not open checkpoint file for writing");
		exit(COULD_NOT_OPEN);
	}

	// Write amount of threads
	if (pwrite(file_descriptor, &info->threads, sizeof(int), 0) == -1) {
		log_err("Could not write to checkpoints");
		exit(COULD_NOT_WRITE);
	}

	// Write total iterations
	if (pwrite(file_descriptor, &info->iteration, sizeof(int), sizeof(int) * 1) == -1) {
		log_err("Could not write to checkpoints");
		exit(COULD_NOT_WRITE);
	}

	// Explicitly disable dynamic teams
	omp_set_dynamic(0);
	omp_set_num_threads(info->threads);

	#pragma omp parallel firstprivate(tid, lines, from, to) private(k, l, i, j)
	{
		tid = omp_get_thread_num();

		lines = (tid < (N % info->threads)) ? ((N / info->threads) + 1) : (N / info->threads);
		from =  (tid < (N % info->threads)) ? (lines * tid) : ((lines * tid) + (N % info->threads));
		to = from + lines;

		for (k = info->iterations_completed; k <= info->iteration; k++)
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

			// We will use pwrite in this case due to the advantages in parallel environments as described in `checkpoint-answers.txt`
			// Also since formatting is no problem, we will simply use byte offsets to differentiate between numbers and provide no human readable
			for (i = from; i < to; i++) {
				// printf("matrix[%d]: %p, size: %ld, offset: %ld, tid: %d\n", i, (void*)matrix[i], sizeof(double)*N, (i * N * sizeof(double)) + isch, tid);
				if (pwrite(file_descriptor, matrix[i], sizeof(double) * N, calc_offset(i)) == -1) {
					log_err("Could not write to checkpoints");
					exit(COULD_NOT_WRITE);
				}
			}
			#pragma omp barrier
			// write by first thread, this just preserves use from heaving T write access for a single opeation which block each other
			if (tid == 0) {
				if (pwrite(file_descriptor, &k, sizeof(int), sizeof(int) * 2) == -1) {
					log_err("Could not write to checkpoints");
					exit(COULD_NOT_WRITE);
				}
			}
		}
	}
	close(file_descriptor);
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
	char* path = calloc(256, sizeof(char));
	double** matrix;


	if (argc < 4)
	{
		printf("Usage: %s threads iterations path\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	else
	{
		sscanf(argv[1], "%d", &threads);
		sscanf(argv[2], "%d", &iterations);
		sscanf(argv[3], "%s", path);
	}

	struct MatrixInfo info = {threads, iterations, 1};

	matrix = alloc_matrix();

	int file;
	if ((file = open(path, 0)) != -1)
	{
		close(file);
		read_matrix(matrix, &info, path);
	}
	else
	{
		init_matrix(matrix);
	}

	gettimeofday(&start_time, NULL);
	calculate(matrix, &info, path);
	gettimeofday(&comp_time, NULL);

	displayStatistics();

	free_matrix(matrix);
	free(path);

	return 0;
}
