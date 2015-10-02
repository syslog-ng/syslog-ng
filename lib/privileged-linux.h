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

#include "file-perms.h"
#include "affile/affile-common.h"

#include <sys/stat.h>
#include <glib.h>
#include <sys/capability.h>
#include "gsockaddr.h"

#if ENABLE_SPOOF_SOURCE
#include <libnet.h>
#endif

#define BASE_CAPS "cap_net_bind_service,cap_net_broadcast,cap_net_raw," \
  "cap_dac_read_search,cap_dac_override,cap_chown,cap_fowner,"

#define BASE_CAPS_WITH_SYSLOG_CAP BASE_CAPS"cap_syslog=p "
#define BASE_CAPS_WITH_SYS_ADMIN_CAP BASE_CAPS"cap_sys_admin=p "

#define STAT_CAPS "cap_dac_read_search+e"
#define MKFIFO_CAPS "cap_dac_override+e"
#define CHANGE_FILE_RIGHTS_CAPS "cap_chown,cap_fowner+e"
#define INIT_RAW_NET_CAPS "cap_net_raw+e"
#define BIND_SERVICE_CAPS "cap_net_bind_service+e"
#define MKDIR_AND_PERM_RIGHTS_CAPS "cap_chown,cap_fowner,cap_dac_override+e"
#define SYSLOG_CAPS "cap_syslog+e"
#define SYSADMIN_CAPS "cap_sys_admin+e"
#define OPEN_AS_READ_CAPS "cap_dac_read_search+e"
#define OPEN_AS_READ_AND_WRITE_CAPS "cap_dac_override+e"
#define OPEN_CAPS(x) get_open_file_caps(x)

#ifndef CAP_SYSLOG
#define CAP_SYSLOG -1
#endif

void setup_caps();
void set_process_dumpable();
void set_keep_caps_flag(const gchar *caps);
gboolean check_syslog_cap();
gboolean setup_permitted_caps(const gchar *caps);

gint restore_caps(cap_t caps);
gint raise_caps(const gchar* new_caps, cap_t *old_caps);
const gchar* get_open_file_caps(const FileOpenOptions *open_opts);

#define PRIVILEGED_CALL(caps, function, result, ...)  \
do {                                                  \
  cap_t saved_caps;                                   \
  gint raised_caps_result;                            \
  raised_caps_result = raise_caps(caps, &saved_caps); \
  result = function(__VA_ARGS__);                     \
  if (raised_caps_result != -1)                       \
    restore_caps(saved_caps);                         \
} while(0)

#endif
