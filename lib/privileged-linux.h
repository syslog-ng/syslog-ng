/*
 * Copyright (c) 2002-2015 BalaBit IT Ltd, Budapest, Hungary
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

#ifndef PRIVILEGED_H_INCLUDED
#define PRIVILEGED_H_INCLUDED

#include <glib.h>
#include <sys/capability.h>

void set_process_dumpable();
void set_keep_caps_flag(const gchar *caps);
gboolean check_syslog_cap();
gboolean setup_permitted_caps(const gchar *caps);

gint restore_caps(cap_t caps);
gint raise_caps(const gchar* new_caps, cap_t *old_caps);

#endif
