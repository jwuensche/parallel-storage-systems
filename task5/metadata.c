/*
 * Copyright (c) 2013-2014 Michael Kuhn
 * Copyright (c) 2013 Sandra Schröder
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
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

struct BenchmarkResult
{
	gdouble create;
	gdouble delete;
	gdouble open;
	gdouble stat;
};

typedef struct BenchmarkResult BenchmarkResult;

struct BenchmarkStatistics
{
	gdouble create_sum;
	gdouble create_sum2;
	gdouble delete_sum;
	gdouble delete_sum2;
	gdouble open_sum;
	gdouble open_sum2;
	gdouble stat_sum;
	gdouble stat_sum2;
};

typedef struct BenchmarkStatistics BenchmarkStatistics;

struct ThreadData
{
	guint id;
	BenchmarkResult result;
};

typedef struct ThreadData ThreadData;

static gint opt_objects = 1000000;
static gboolean opt_shared = FALSE;
static gboolean opt_sync = FALSE;
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

	guint64 objects;

	if (result == NULL)
	{
		if (opt_iterations == 1)
		{
			return;
		}

		g_print("# configuration[n/p] create_mean[ops/s] create_stddev[ops/s] delete_mean[ops/s] delete_stddev[ops/s] open_mean[ops/s] open_stddev[ops/s] stat_mean[ops/s] stat_stddev[ops/s]\n");

		g_print("%d ", opt_threads);
		g_print("%.3f ", statistics->create_sum / opt_iterations);
		g_print("%.3f ", sqrt(((opt_iterations * statistics->create_sum2) - (statistics->create_sum * statistics->create_sum)) / (opt_iterations * (opt_iterations - 1))));
		g_print("%.3f ", statistics->delete_sum / opt_iterations);
		g_print("%.3f ", sqrt(((opt_iterations * statistics->delete_sum2) - (statistics->delete_sum * statistics->delete_sum)) / (opt_iterations * (opt_iterations - 1))));
		g_print("%.3f ", statistics->open_sum / opt_iterations);
		g_print("%.3f ", sqrt(((opt_iterations * statistics->open_sum2) - (statistics->open_sum * statistics->open_sum)) / (opt_iterations * (opt_iterations - 1))));
		g_print("%.3f ", statistics->stat_sum / opt_iterations);
		g_print("%.3f\n", sqrt(((opt_iterations * statistics->stat_sum2) - (statistics->stat_sum * statistics->stat_sum)) / (opt_iterations * (opt_iterations - 1))));

		return;
	}

	result->create /= opt_threads;
	result->delete /= opt_threads;
	result->open /= opt_threads;
	result->stat /= opt_threads;

	objects = (guint64)opt_objects * opt_threads;

	if (!printed_header)
	{
		g_print("# configuration[n/p] create_time[s] create_speed[ops/s] delete_time[s] delete_speed[ops/s] open_time[s] open_speed[ops/s] stat_time[s] stat_speed[ops/s]\n");
		printed_header = TRUE;
	}

	g_print("%d ", opt_threads);
	g_print("%.3f ", result->create);
	g_print("%.3f ", objects / result->create);
	g_print("%.3f ", result->delete);
	g_print("%.3f ", objects / result->delete);
	g_print("%.3f ", result->open);
	g_print("%.3f ", objects / result->open);
	g_print("%.3f ", result->stat);
	g_print("%.3f\n", objects / result->stat);

	statistics->create_sum += objects / result->create;
	statistics->create_sum2 += (objects / result->create) * (objects / result->create);
	statistics->delete_sum += objects / result->delete;
	statistics->delete_sum2 += (objects / result->delete) * (objects / result->delete);
	statistics->open_sum += objects / result->open;
	statistics->open_sum2 += (objects / result->open) * (objects / result->open);
	statistics->stat_sum += objects / result->stat;
	statistics->stat_sum2 += (objects / result->stat) * (objects / result->stat);
}

static
gboolean
manage_directory (ThreadData* thread_data)
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
get_directory_name (ThreadData* thread_data)
{
	gchar* name;

	if (opt_shared)
	{
		name = g_strdup("metadata");
	}
	else
	{
		name = g_strdup_printf("metadata-%d", thread_data->id);
	}

	return name;
}

static
gchar*
get_path_name (ThreadData* thread_data, guint64 index)
{
	gchar* name;

	if (opt_shared)
	{
		name = g_strdup_printf("metadata/metadata-%d-%" G_GUINT64_FORMAT, thread_data->id, index);
	}
	else
	{
		name = g_strdup_printf("metadata-%d/metadata-%" G_GUINT64_FORMAT, thread_data->id, index);
	}

	return name;
}

static
void
execute_command (ThreadData* thread_data)
{
	if (opt_command != NULL && thread_data->id == 0)
	{
		system(opt_command);
	}
}

static
gpointer
metadata_posix (gpointer data)
{
	ThreadData* thread_data = data;

	GTimer* timer;
	gchar* name = NULL;
	gchar* path = NULL;
	gint fd;
	gint ret;
	struct stat stat_buf;

	timer = g_timer_new();

	barrier(thread_data);

	if (manage_directory(thread_data))
	{
		name = get_directory_name(thread_data);
		path = g_build_filename(opt_path, name, NULL);

		ret = mkdir(path, 0700);

		if (ret != 0)
		{
			g_printerr("Cannot create directory %s\n", path);
			goto error;
		}

		g_free(path);
		g_free(name);
	}

	barrier(thread_data);

	g_timer_start(timer);

	for (guint i = 0; i < (guint)opt_objects; i++)
	{
		name = get_path_name(thread_data, i);
		path = g_build_filename(opt_path, name, NULL);

		fd = open(path, O_CREAT, 0600);

		if (fd == -1)
		{
			g_printerr("Can not create file %s.\n", name);
			goto error;
		}

		if (opt_sync)
		{
			fsync(fd);
		}

		close(fd);

		g_free(path);
		g_free(name);
	}

	thread_data->result.create = g_timer_elapsed(timer, NULL);

	barrier(thread_data);
	execute_command(thread_data);
	barrier(thread_data);

	g_timer_start(timer);

	for (guint i = 0; i < (guint)opt_objects; i++)
	{
		name = get_path_name(thread_data, i);
		path = g_build_filename(opt_path, name, NULL);

		fd = open(path, O_RDWR);

		if (fd == -1)
		{
			g_printerr("Can not open file %s.\n", name);
			goto error;
		}

		close(fd);

		g_free(path);
		g_free(name);
	}

	thread_data->result.open = g_timer_elapsed(timer, NULL);

	barrier(thread_data);
	execute_command(thread_data);
	barrier(thread_data);

	g_timer_start(timer);

	for (guint i = 0; i < (guint)opt_objects; i++)
	{
		name = get_path_name(thread_data, i);
		path = g_build_filename(opt_path, name, NULL);

		ret = stat(path, &stat_buf);

		if (ret == -1)
		{
			g_printerr("Can not stat file %s.\n", name);
			goto error;
		}

		g_free(path);
		g_free(name);
	}

	thread_data->result.stat = g_timer_elapsed(timer, NULL);

	barrier(thread_data);
	execute_command(thread_data);
	barrier(thread_data);

	g_timer_start(timer);

	for (guint i = 0; i < (guint)opt_objects; i++)
	{
		name = get_path_name(thread_data, i);
		path = g_build_filename(opt_path, name, NULL);

		ret = unlink(path);

		if (ret == -1)
		{
			g_printerr("Can not delete file %s.\n", name);
			goto error;
		}

		g_free(path);
		g_free(name);
	}

	thread_data->result.delete = g_timer_elapsed(timer, NULL);

	barrier(thread_data);

	if (manage_directory(thread_data))
	{
		name = get_directory_name(thread_data);
		path = g_build_filename(opt_path, name, NULL);

		ret = rmdir(path);

		if (ret != 0)
		{
			g_printerr("Cannot remove directory %s\n", path);
			goto error;
		}

		g_free(path);
		g_free(name);
	}

	g_timer_destroy(timer);

	return thread_data;

error:
	g_timer_destroy(timer);

	g_free(path);
	g_free(name);

	return NULL;
}

int
main (int argc, char** argv)
{
	GError* error = NULL;
	GOptionContext* context;

	GOptionEntry entries[] = {
		{ "objects", 0, 0, G_OPTION_ARG_INT, &opt_objects, "Objects count", "1000000" },
		{ "shared", 0, 0, G_OPTION_ARG_NONE, &opt_shared, "Use shared access", NULL },
		{ "sync",  0, 0, G_OPTION_ARG_NONE, &opt_sync, "Use synchronisation", NULL },
		{ "iterations", 0, 0, G_OPTION_ARG_INT, &opt_iterations, "Number of iterations to perform", "1" },
		{ "threads", 0, 0, G_OPTION_ARG_INT, &opt_threads, "Number of threads to use", "1" },
		{ "command", 0, 0, G_OPTION_ARG_STRING, &opt_command, "Command to execute between phases", NULL },
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

	statistics.create_sum = 0.0;
	statistics.create_sum2 = 0.0;
	statistics.delete_sum = 0.0;
	statistics.delete_sum2 = 0.0;
	statistics.open_sum = 0.0;
	statistics.open_sum2 = 0.0;
	statistics.stat_sum = 0.0;
	statistics.stat_sum2 = 0.0;

	for (gint i = 0; i < opt_iterations; i++)
	{
		BenchmarkResult global_result;

		global_result.create = 0.0;
		global_result.delete = 0.0;
		global_result.open = 0.0;
		global_result.stat = 0.0;

		for (gint j = 0; j < opt_threads; j++)
		{
			ThreadData* thread_data;

			thread_data = g_slice_new(ThreadData);
			thread_data->id = j;

			threads[j] = g_thread_new("metadata", metadata_posix, thread_data);
		}

		for (gint j = 0; j < opt_threads; j++)
		{
			ThreadData* thread_data;

			thread_data = g_thread_join(threads[j]);

			if (thread_data == NULL)
			{
				goto error;
			}

			global_result.create += thread_data->result.create;
			global_result.delete += thread_data->result.delete;
			global_result.open += thread_data->result.open;
			global_result.stat += thread_data->result.stat;

			g_slice_free(ThreadData, thread_data);
		}

		print_statistics(&global_result, &statistics);
	}

	print_statistics(NULL, &statistics);

	pthread_barrier_destroy(&thread_barrier);

	g_free(opt_command);

	return 0;

error:
	pthread_barrier_destroy(&thread_barrier);

	g_free(opt_command);

	return 1;
}
