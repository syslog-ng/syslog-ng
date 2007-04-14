/*
 * Copyright (c) 2002, 2003, 2004 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef SGROUP_H_INCLUDED
#define SGROUP_H_INCLUDED

#include "syslog-ng.h"
#include "logpipe.h"
#include "driver.h"

typedef struct _LogSourceGroup
{
  LogPipe super;
  GString *name;
  LogDriver *drivers;
  
  gboolean chain_hostnames;
  gboolean normalize_hostnames;
  gboolean keep_hostname;
  gboolean use_dns;
  gboolean use_fqdn;
  gboolean use_dns_cache;
  
  guint32 *processed_messages;
  
} LogSourceGroup;

static inline LogSourceGroup *
log_sgrp_ref(LogSourceGroup *s)
{
  return (LogSourceGroup *) log_pipe_ref((LogPipe *) s);
}

static inline void
log_sgrp_unref(LogSourceGroup *s)
{
  log_pipe_unref((LogPipe *) s);
}

LogSourceGroup *log_source_group_new(gchar *name, LogDriver *drivers);

#endif
