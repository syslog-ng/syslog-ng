/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */
#ifndef MAINLOOP_H_INCLUDED
#define MAINLOOP_H_INCLUDED

#include "syslog-ng.h"
#include "pthread.h"

#ifdef _WIN32
typedef DWORD ThreadId;
#else
typedef pthread_t ThreadId;
#endif

extern volatile gboolean main_loop_io_workers_quit;
extern gboolean syntax_only;
extern gboolean generate_persist_file;
extern gboolean server_mode;
extern ThreadId main_thread_handle;
extern guint32 g_run_id;

#include <iv_work.h>

typedef gpointer (*MainLoopTaskFunc)(gpointer user_data);

typedef struct _MainLoopIOWorkerFinishCallback
{
  struct iv_list_head list;
  MainLoopTaskFunc func;
  gpointer user_data;
} MainLoopIOWorkerFinishCallback;

static inline void
main_loop_io_worker_finish_callback_init(MainLoopIOWorkerFinishCallback *self)
{
  INIT_IV_LIST_HEAD(&self->list);
}

typedef struct _MainLoopIOWorkerJob
{
  void (*work)(gpointer user_data);
  void (*completion)(gpointer user_data);
  gpointer user_data;
  gboolean working:1;
  struct iv_work_item work_item;

  /* function to be called back when the current job is finished. */
  struct iv_list_head finish_callbacks;
} MainLoopIOWorkerJob;

static inline gboolean
main_loop_io_worker_job_quit(void)
{
  return main_loop_io_workers_quit;
}


void main_loop_add_quit_callback_list_element(gpointer func, gpointer user_data);
gpointer main_loop_external_thread_quit(gpointer user_data);
void main_loop_external_thread_started(void);
void main_loop_maximalize_worker_threads(int max_threads);
void main_loop_io_worker_set_thread_id(gint id);
gint main_loop_io_worker_thread_id(void);
void main_loop_io_worker_job_init(MainLoopIOWorkerJob *self);
void main_loop_io_worker_job_submit(MainLoopIOWorkerJob *self);
void main_loop_io_worker_register_finish_callback(MainLoopIOWorkerFinishCallback *cb);

static inline ThreadId
get_thread_id()
{
#ifndef _WIN32
  return pthread_self();
#else
  return GetCurrentThreadId();
#endif
}

static inline int
threads_equal(ThreadId thread_a, ThreadId thread_b)
{
#ifndef _WIN32
  return pthread_equal(thread_a, thread_b);
#else
  return thread_a == thread_b;
#endif
}


static inline void
main_loop_assert_main_thread(void)
{
#if ENABLE_DEBUG
  g_assert(threads_equal(main_thread_handle, get_thread_id()));
#endif
}

static inline gboolean
main_loop_is_main_thread(void)
{
  return threads_equal(main_thread_handle, get_thread_id());
}

typedef void(*_BeforeStart)(GlobalConfig *);

gpointer main_loop_call(MainLoopTaskFunc func, gpointer user_data, gboolean wait);
int main_loop_init(gchar *config_string);
int  main_loop_run(void);
void main_loop_terminate();

void main_loop_add_options(GOptionContext *ctx);

#endif
