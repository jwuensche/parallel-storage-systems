/*
 * Copyright (c) 2010-2013 Michael Kuhn
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define _POSIX_C_SOURCE 200809L

#include <glib.h>
#include <glib-unix.h>

#include <math.h>
#include <pthread.h>
#include <string.h>

struct BenchmarkResult
{
	gdouble read;
	gdouble write;
};

typedef struct BenchmarkResult BenchmarkResult;

struct BenchmarkStatistics
{
	gdouble read_sum;
	gdouble read_sum2;
	gdouble write_sum;
	gdouble write_sum2;
};

typedef struct BenchmarkStatistics BenchmarkStatistics;

struct ThreadData
{
	guint id;
	GRand* rand;
	BenchmarkResult result;
};

typedef struct ThreadData ThreadData;

static gint opt_block_count = 65536;
static gint opt_block_size = 4096;
static gboolean opt_shared = FALSE;
static gboolean opt_random = FALSE;
static gboolean opt_sync = FALSE;
static gboolean opt_sync_on_close = FALSE;
static gint opt_iterations = 1;
static gint opt_threads = 1;
static gchar* opt_command = NULL;

static gchar* opt_path = NULL;

static pthread_barrier_t thread_barrier;

static
void
print_statistics (BenchmarkResult* result, BenchmarkStatistics* statistics)
{
	static gboolean printed_header = FALSE;

	gdouble write_thp;
	gdouble read_thp;
	guint64 bytes;

	if (result == NULL)
	{
		if (opt_iterations == 1)
		{
			return;
		}

		g_print("# configuration[n/p] write_mean[MB/s] write_stddev[MB/s] read_mean[MB/s] read_stddev[MB/s]\n");

		g_print("%d ", opt_threads);
		g_print("%.3f ", statistics->write_sum / opt_iterations);
		g_print("%.3f ", sqrt(((opt_iterations * statistics->write_sum2) - (statistics->write_sum * statistics->write_sum)) / (opt_iterations * (opt_iterations - 1))));
		g_print("%.3f ", statistics->read_sum / opt_iterations);
		g_print("%.3f\n", sqrt(((opt_iterations * statistics->read_sum2) - (statistics->read_sum * statistics->read_sum)) / (opt_iterations * (opt_iterations - 1))));

		return;
	}

	result->read /= opt_threads;
	result->write /= opt_threads;

	bytes = (guint64)opt_block_size * opt_block_count * opt_threads;

	if (!printed_header)
	{
		g_print("# configuration[n/p] write_time[s] write_throughput[MB/s] read_time[s] read_throughput[MB/s]\n");
		printed_header = TRUE;
	}

	write_thp = bytes / result->write / (1024 * 1024);
	read_thp = bytes / result->read / (1024 * 1024);

	g_print("%d ", opt_threads);
	g_print("%.3f ", result->write);
	g_print("%.3f ", write_thp);
	g_print("%.3f ", result->read);
	g_print("%.3f\n", read_thp);

	statistics->read_sum += read_thp;
	statistics->read_sum2 += read_thp * read_thp;
	statistics->write_sum += write_thp;
	statistics->write_sum2 += write_thp * write_thp;
}

static
void
reset_rand (ThreadData* thread_data)
{
	if (!opt_random)
	{
		return;
	}

	g_rand_set_seed(thread_data->rand, 42 + thread_data->id);
}

static
guint64
get_offset (ThreadData* thread_data, guint64 iteration)
{
	if (opt_random)
	{
		iteration = g_rand_int_range(thread_data->rand, 0, opt_block_count);
	}

	if (!opt_shared)
	{
		return opt_block_size * iteration;
	}
	else
	{
		return opt_block_size * ((iteration * opt_threads) + thread_data->id);
	}
}

static
gboolean
manage_file (ThreadData* thread_data)
{
	return (!opt_shared || thread_data->id == 0);
}

static
void
barrier (ThreadData* thread_data)
{
	(void)thread_data;

	pthread_barrier_wait(&thread_barrier);
}

static
gchar*
get_name (ThreadData* thread_data)
{
	gchar* name;

	if (opt_shared)
	{
		name = g_strdup("small-access");
	}
	else
	{
		name = g_strdup_printf("small-access-%d", thread_data->id);
	}

	return name;
}

static
gpointer
small_access_posix (gpointer data)
{
	ThreadData* thread_data = data;

	gchar buf[opt_block_size];

	GTimer* timer;
	gchar* name;
	gchar* path;
	gint fd;
	guint64 iteration = 0;

	name = get_name(thread_data);
	path = g_build_filename(opt_path, name, NULL);
	timer = g_timer_new();
	g_free(name);

	barrier(thread_data);

	if (manage_file(thread_data))
	{
		fd = open(path, O_RDWR | O_CREAT, 0600);

		if (fd == -1)
		{
			g_printerr("Cannot create file on path %s\n", path);
		}

		close(fd);
	}

	barrier(thread_data);
	reset_rand(thread_data);

	g_timer_start(timer);
	fd = open(path, O_RDWR);

	if (fd == -1)
	{
		g_error("Cannot write on %s.\n", path);
	}

	for (iteration = 0; iteration < (guint)opt_block_count; iteration += 1000)
	{
		guint remaining;

		remaining = MIN(1000, opt_block_count - iteration);

		for (guint i = 0; i < remaining; i++)
		{
			guint64 bytes;

			bytes = pwrite(fd, buf, opt_block_size, get_offset(thread_data, iteration + i));
			g_assert(bytes == (guint)opt_block_size);

			if (opt_sync)
			{
				fsync(fd);
			}
		}

		barrier(thread_data);
	}

	if (opt_sync_on_close)
	{
		fsync(fd);
	}

	close(fd);
	thread_data->result.write = g_timer_elapsed(timer, NULL);

	barrier(thread_data);

	if (opt_command != NULL && thread_data->id == 0)
	{
		system(opt_command);
	}

	barrier(thread_data);
	reset_rand(thread_data);

	g_timer_start(timer);
	fd = open(path, O_RDWR);

	if (fd == -1)
	{
		g_error("Cannot read.\n");
	}

	for (iteration = 0; iteration < (guint)opt_block_count; iteration += 1000)
	{
		guint remaining;

		remaining = MIN(1000, opt_block_count - iteration);

		for (guint i = 0; i < remaining; i++)
		{
			guint64 bytes;

			bytes = pread(fd, buf, opt_block_size, get_offset(thread_data, iteration + i));
			g_assert(bytes == (guint)opt_block_size);
		}

		barrier(thread_data);
	}

	close(fd);
	thread_data->result.read = g_timer_elapsed(timer, NULL);

	g_timer_destroy(timer);

	if (manage_file(thread_data))
	{
		unlink(path);
	}

	g_free(path);

	return thread_data;
}

int
main (int argc, char** argv)
{
	GError* error = NULL;
	GOptionContext* context;

	GOptionEntry entries[] = {
		{ "block-count", 0, 0, G_OPTION_ARG_INT, &opt_block_count, "Block count", "65536" },
		{ "block-size", 0, 0, G_OPTION_ARG_INT, &opt_block_size, "Block size", "4096" },
		{ "shared", 0, 0, G_OPTION_ARG_NONE, &opt_shared, "Use shared access", NULL },
		{ "random", 0, 0, G_OPTION_ARG_NONE, &opt_random, "Use random access", NULL },
		{ "sync",  0, 0, G_OPTION_ARG_NONE, &opt_sync, "Use synchronisation", NULL },
		{ "sync-on-close",  0, 0, G_OPTION_ARG_NONE, &opt_sync_on_close, "Use synchronisation on close", NULL },
		{ "iterations", 0, 0, G_OPTION_ARG_INT, &opt_iterations, "Number of iterations to perform", "1" },
		{ "threads", 0, 0, G_OPTION_ARG_INT, &opt_threads, "Number of threads to use", "1" },
		{ "command", 0, 0, G_OPTION_ARG_STRING, &opt_command, "Command to execute between write and read phases", NULL },
		{ "path", 0, 0, G_OPTION_ARG_STRING, &opt_path, "Path to use", NULL },
		{ NULL, 0, 0, 0, NULL, NULL, NULL }
	};

	BenchmarkStatistics statistics;
	GThread** threads;

	context = g_option_context_new(NULL);
	g_option_context_add_main_entries(context, entries, NULL);

	if (!g_option_context_parse(context, &argc, &argv, &error))
	{
		g_option_context_free(context);

		if (error)
		{
			g_printerr("%s\n", error->message);
			g_error_free(error);
		}

		return 1;
	}

	if (opt_path == NULL)
	{
		gchar* help;

		help = g_option_context_get_help(context, TRUE, NULL);
		g_option_context_free(context);

		g_print("%s", help);
		g_free(help);

		return 1;
	}

	g_option_context_free(context);

	pthread_barrier_init(&thread_barrier, NULL, opt_threads);
	threads = g_new(GThread*, opt_threads);

	statistics.read_sum = 0.0;
	statistics.read_sum2 = 0.0;
	statistics.write_sum = 0.0;
	statistics.write_sum2 = 0.0;

	for (gint i = 0; i < opt_iterations; i++)
	{
		BenchmarkResult global_result;

		global_result.read = 0.0;
		global_result.write = 0.0;

		for (gint j = 0; j < opt_threads; j++)
		{
			ThreadData* thread_data;

			thread_data = g_slice_new(ThreadData);
			thread_data->id = j;
			thread_data->rand = g_rand_new_with_seed(42 + thread_data->id);

			threads[j] = g_thread_new("small-access", small_access_posix, thread_data);
		}

		for (gint j = 0; j < opt_threads; j++)
		{
			ThreadData* thread_data;

			thread_data = g_thread_join(threads[j]);

			global_result.read += thread_data->result.read;
			global_result.write += thread_data->result.write;

			g_rand_free(thread_data->rand);
			g_slice_free(ThreadData, thread_data);
		}

		print_statistics(&global_result, &statistics);
	}

	print_statistics(NULL, &statistics);

	pthread_barrier_destroy(&thread_barrier);

	g_free(opt_command);

	return 0;
}
