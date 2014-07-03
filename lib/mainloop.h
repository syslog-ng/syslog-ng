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
#include "thread-utils.h"

extern gboolean syntax_only;
extern gboolean __main_loop_is_terminating;
extern gboolean generate_persist_file;
extern gboolean server_mode;
extern ThreadId main_thread_handle;
extern guint32 g_run_id;

typedef gpointer (*MainLoopTaskFunc)(gpointer user_data);

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

static inline gboolean
main_loop_is_terminating(void)
{
  return __main_loop_is_terminating;
}


typedef void(*_BeforeStart)(GlobalConfig *);

int main_loop_init(gchar *config_string);
int  main_loop_run(void);
void main_loop_terminate();
void main_loop_reload();

void main_loop_add_options(GOptionContext *ctx);

#endif
