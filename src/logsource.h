/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
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
  
#ifndef LOGSOURCE_H_INCLUDED
#define LOGSOURCE_H_INCLUDED

#include "logpipe.h"

typedef struct _LogSourceOptions
{
  gint init_window_size;
  GAtomicCounter window_size;
  const gchar *group_name;
  gboolean keep_timestamp;
  gboolean keep_hostname;
  gboolean chain_hostnames;
  gboolean normalize_hostnames;
  gboolean use_dns;
  gboolean use_fqdn;
  gboolean use_dns_cache;
  gchar *program_override;
  gint program_override_len;
  gchar *host_override;
  gint host_override_len;
  guint source_group_tag;

} LogSourceOptions;

/**
 * LogSource:
 *
 * This structure encapsulates an object which generates messages without
 * defining how those messages are accepted by peers. The most prominent
 * derived class is LogReader which is an extended RFC3164 capable syslog
 * message processor used everywhere.
 **/
typedef struct _LogSource
{
  LogPipe super;
  LogSourceOptions *options;
  gint stats_level;
  guint16 stats_source;
  gchar *stats_id;
  gchar *stats_instance;
  guint32 *last_message_seen;
  guint32 *recvd_messages;
} LogSource;

static inline gboolean
log_source_free_to_send(LogSource *self)
{
  return g_atomic_counter_get(&self->options->window_size) > 0;
}

gboolean log_source_init(LogPipe *s);
gboolean log_source_deinit(LogPipe *s);

void log_source_set_options(LogSource *self, LogSourceOptions *options, gint stats_level, gint stats_source, const gchar *stats_id, const gchar *stats_instance);
void log_source_mangle_hostname(LogSource *self, LogMessage *msg);
void log_source_init_instance(LogSource *self);
void log_source_options_defaults(LogSourceOptions *options);
void log_source_options_init(LogSourceOptions *options, GlobalConfig *cfg, const gchar *group_name);
void log_source_options_destroy(LogSourceOptions *options);
void log_source_free(LogPipe *s);


#endif
