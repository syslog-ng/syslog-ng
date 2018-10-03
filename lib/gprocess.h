/*
 * Copyright (c) 2002-2011 Balabit
 * Copyright (c) 1998-2011 Balázs Scheidler
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

#ifndef GPROCESS_H_INCLUDED
#define GPROCESS_H_INCLUDED

#include "syslog-ng.h"

#include <sys/types.h>

#if SYSLOG_NG_ENABLE_LINUX_CAPS
#  include <sys/capability.h>
#endif

typedef enum
{
  G_PM_FOREGROUND,
  G_PM_BACKGROUND,
  G_PM_SAFE_BACKGROUND,
} GProcessMode;

#if SYSLOG_NG_ENABLE_LINUX_CAPS

gboolean g_process_enable_cap(const gchar *cap_name);
gboolean g_process_is_cap_enabled(void);
cap_t g_process_cap_save(void);
void g_process_cap_restore(cap_t r);

#else

typedef gpointer cap_t;

#define g_process_enable_cap(cap)
#define g_process_cap_save() NULL
#define g_process_cap_restore(cap) cap = cap

#endif

void g_process_message(const gchar *fmt, ...);

void g_process_set_mode(GProcessMode mode);
GProcessMode g_process_get_mode(void);
void g_process_set_name(const gchar *name);
void g_process_set_user(const gchar *user);
void g_process_set_group(const gchar *group);
void g_process_set_chroot(const gchar *chroot);
void g_process_set_pidfile(const gchar *pidfile);
void g_process_set_pidfile_dir(const gchar *pidfile_dir);
void g_process_set_working_dir(const gchar *cwd);
void g_process_set_caps(const gchar *caps);
void g_process_set_argv_space(gint argc, gchar **argv);
void g_process_set_use_fdlimit(gboolean use);
void g_process_set_check(gint check_period, gboolean (*check_fn)(void));

gboolean g_process_check_cap_syslog(void);

void g_process_start(void);
void g_process_startup_failed(guint ret_num, gboolean may_exit);
void g_process_startup_ok(void);
void g_process_finish(void);

void g_process_add_option_group(GOptionContext *ctx);


#endif
