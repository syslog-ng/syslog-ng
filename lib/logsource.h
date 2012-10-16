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
  
#ifndef LOGSOURCE_H_INCLUDED
#define LOGSOURCE_H_INCLUDED

#include "logpipe.h"
#include "stats.h"
#include "logproto.h"
#ifndef G_OS_WIN32
#include <iv_event.h>
#endif

#define LOF_POS_TRACKING 0x00000001

typedef struct _LogSourceOptions
{
  guint32 flags;
  gint init_window_size;
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
  LogTagId source_group_tag;
  GArray *tags;
  GList *source_queue_callbacks;
} LogSourceOptions;

typedef struct _LogSource LogSource;

/**
 * LogSource:
 *
 * This structure encapsulates an object which generates messages without
 * defining how those messages are accepted by peers. The most prominent
 * derived class is LogReader which is an extended RFC3164 capable syslog
 * message processor used everywhere.
 **/
struct _LogSource
{
  LogPipe super;
  LogSourceOptions *options;
  guint16 stats_level;
  guint16 stats_source;
  gboolean threaded;
  gchar *stats_id;
  gchar *stats_instance;
  GAtomicCounter window_size;
  StatsCounterItem *last_message_seen;
  StatsCounterItem *recvd_messages;
  guint32 last_ack_count;
  guint32 ack_count;
  glong window_full_sleep_nsec;
  struct timespec last_ack_rate_time;
  AckData *ack_list;
  guint32 msg_id;
  guint64 last_sent;
  GStaticMutex g_mutex_ack;

  void (*wakeup)(LogSource *s);
  gboolean (*ack)(LogSource *s,gpointer user_data, gboolean need_to_save);
  void (*get_state)(LogSource *s,gpointer user_data);
};

static inline gboolean
log_source_free_to_send(LogSource *self)
{
  return g_atomic_counter_get(&self->window_size) > 0;
}

static inline void
log_source_get_state(LogSource *self, guint64 pos)
{
  if (self->get_state)
    self->get_state(self,(gpointer)&(self->ack_list[pos]));
}

gboolean log_source_init(LogPipe *s);
gboolean log_source_deinit(LogPipe *s);

void log_source_set_options(LogSource *self, LogSourceOptions *options, gint stats_level, gint stats_source, const gchar *stats_id, const gchar *stats_instance, gboolean threaded);
void log_source_mangle_hostname(LogSource *self, LogMessage *msg);
void log_source_init_instance(LogSource *self);
void log_source_options_defaults(LogSourceOptions *options);
void log_source_options_init(LogSourceOptions *options, GlobalConfig *cfg, const gchar *group_name);
void log_source_options_destroy(LogSourceOptions *options);
void log_source_options_set_tags(LogSourceOptions *options, GList *tags);
void log_source_free(LogPipe *s);
void log_source_wakeup(LogSource *self);

void log_source_global_init(void);


#endif
