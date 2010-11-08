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

#include <iv_work.h>

extern volatile gboolean main_loop_io_workers_quit;
extern gboolean syntax_only;
extern __thread gboolean in_main_thread;

typedef gpointer (*MainLoopTaskFunc)(gpointer user_data);

typedef struct _MainLoopIOWorkerJob
{
  void (*work)(gpointer user_data);
  void (*completion)(gpointer user_data);
  gpointer user_data;
  gboolean working:1;
  struct iv_work_item work_item;
} MainLoopIOWorkerJob;

static inline gboolean
main_loop_io_worker_job_quit(void)
{
  return main_loop_io_workers_quit;
}

void main_loop_io_worker_job_init(MainLoopIOWorkerJob *self);
void main_loop_io_worker_job_submit(MainLoopIOWorkerJob *self);

static inline void
main_loop_assert_main_thread(void)
{
  g_assert(in_main_thread == TRUE);
}

gpointer main_loop_call(MainLoopTaskFunc func, gpointer user_data, gboolean wait);
int main_loop_init(void);
int  main_loop_run(void);

void main_loop_add_options(GOptionContext *ctx);

#endif
