/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
 * Copyright (c) 2022 Balázs Scheidler <bazsi77@gmail.com>
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

#ifndef MAINLOOP_THREADED_WORKER_H_INCLUDED
#define MAINLOOP_THREADED_WORKER_H_INCLUDED

#include "mainloop-worker.h"

typedef void (*MainLoopThreadedWorkerFunc)(gpointer user_data);
typedef struct _MainLoopThreadedWorker MainLoopThreadedWorker;
struct _MainLoopThreadedWorker
{
  gpointer data;
  MainLoopWorkerType worker_type;
  GThread *thread;
  GMutex lock;
  struct
  {
    GCond cond;
    gboolean finished;
    gboolean result;
  } startup;
  gboolean (*thread_init)(MainLoopThreadedWorker *s);
  void (*thread_deinit)(MainLoopThreadedWorker *s);
  void (*request_exit)(MainLoopThreadedWorker *self);
  void (*run)(MainLoopThreadedWorker *self);
};

gboolean main_loop_threaded_worker_start(MainLoopThreadedWorker *self);

void main_loop_threaded_worker_init(MainLoopThreadedWorker *self,
                                    MainLoopWorkerType worker_type, gpointer data);
void main_loop_threaded_worker_clear(MainLoopThreadedWorker *self);

#endif
