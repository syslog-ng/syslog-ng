/*
 * Copyright (c) 2013, 2014 Balabit
 * Copyright (c) 2013, 2014 Gergely Nagy <algernon@balabit.hu>
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

#ifndef LOGTHRDESTDRV_H
#define LOGTHRDESTDRV_H

#include "syslog-ng.h"
#include "driver.h"
#include "stats/stats-registry.h"
#include "stats/aggregator/stats-aggregator.h"
#include "stats/stats-compat.h"
#include "stats/stats-cluster-key-builder.h"
#include "logqueue.h"
#include "seqnum.h"
#include "mainloop-threaded-worker.h"
#include "timeutils/misc.h"
#include "template/templates.h"

#include <iv.h>
#include <iv_event.h>

typedef enum
{
  /* flush modes */

  /* flush the infligh messages */
  LTF_FLUSH_NORMAL,

  /* expedite flush, to be used at reload, when the persistency of the queue
   * contents is ensured */
  LTF_FLUSH_EXPEDITE,
} LogThreadedFlushMode;

typedef enum
{
  LTR_DROP,
  LTR_ERROR,
  LTR_EXPLICIT_ACK_MGMT,
  LTR_SUCCESS,
  LTR_QUEUED,
  LTR_NOT_CONNECTED,
  LTR_RETRY,
  LTR_MAX
} LogThreadedResult;

enum
{
  LTDF_SEQNUM_ALL = 0x0001,
  LTDF_SEQNUM = 0x0002,
  /* NOTE: everything >= 0x1000 is driver specific */
};

typedef struct _LogThreadedDestDriver LogThreadedDestDriver;
typedef struct _LogThreadedDestWorker LogThreadedDestWorker;

struct _LogThreadedDestWorker
{
  MainLoopThreadedWorker thread;
  LogQueue *queue;
  struct iv_task  do_work;
  struct iv_event wake_up_event;
  struct iv_event shutdown_event;
  struct iv_timer timer_reopen;
  struct iv_timer timer_throttle;
  struct iv_timer timer_flush;

  LogThreadedDestDriver *owner;

  gint worker_index;
  gboolean connected;
  gint batch_size;
  gint rewound_batch_size;
  gint retries_on_error_counter;
  guint retries_counter;
  gint32 seq_num;
  struct timespec last_flush_time;
  gboolean enable_batching;
  gboolean suspended;
  time_t time_reopen;

  struct
  {
    GString *last_key;
  } partitioning;

  struct
  {
    StatsClusterKey *output_event_bytes_sc_key;
    StatsClusterKey *output_unreachable_key;
    StatsClusterKey *message_delay_sample_key;
    StatsClusterKey *message_delay_sample_age_key;

    StatsByteCounter written_bytes;
    StatsCounterItem *output_unreachable;
    StatsCounterItem *message_delay_sample;
    StatsCounterItem *message_delay_sample_age;

    gint64 last_delay_update;
  } metrics;

  gboolean (*init)(LogThreadedDestWorker *s);
  void (*deinit)(LogThreadedDestWorker *s);
  gboolean (*connect)(LogThreadedDestWorker *s);
  void (*disconnect)(LogThreadedDestWorker *s);
  LogThreadedResult (*insert)(LogThreadedDestWorker *s, LogMessage *msg);
  LogThreadedResult (*flush)(LogThreadedDestWorker *s, LogThreadedFlushMode mode);
  void (*free_fn)(LogThreadedDestWorker *s);
};

const gchar *log_threaded_result_to_str(LogThreadedResult self);

struct _LogThreadedDestDriver
{
  LogDestDriver super;

  struct
  {
    StatsClusterKey *output_events_sc_key;
    StatsClusterKey *processed_sc_key;
    StatsClusterKey *output_event_retries_sc_key;

    StatsCounterItem *dropped_messages;
    StatsCounterItem *processed_messages;
    StatsCounterItem *written_messages;
    StatsCounterItem *output_event_retries;

    gboolean raw_bytes_enabled;

    StatsAggregator *max_message_size;
    StatsAggregator *average_messages_size;
    StatsAggregator *max_batch_size;
    StatsAggregator *average_batch_size;
    StatsAggregator *CPS;
  } metrics;

  gint batch_lines;
  gint batch_timeout;
  gboolean under_termination;
  time_t time_reopen;
  gint retries_on_error_max;
  guint retries_max;

  struct
  {
    LogThreadedDestWorker *(*construct)(LogThreadedDestDriver *s, gint worker_index);

    /* this is a compatibility layer that can be removed once all drivers have
     * been migrated to the use of LogThreadedDestWorker based interface.
     * Right now, if a driver is not overriding the Worker instance, we would
     * be calling these methods from the functions named `_compat_*()`. */
    LogThreadedDestWorker instance;
    void (*thread_init)(LogThreadedDestDriver *s);
    void (*thread_deinit)(LogThreadedDestDriver *s);
    gboolean (*connect)(LogThreadedDestDriver *s);
    void (*disconnect)(LogThreadedDestDriver *s);
    LogThreadedResult (*insert)(LogThreadedDestDriver *s, LogMessage *msg);
    LogThreadedResult (*flush)(LogThreadedDestDriver *s);
  } worker;

  LogThreadedDestWorker **workers;
  gint num_workers;
  gint created_workers;
  guint last_worker;

  gboolean flush_on_key_change;
  LogTemplate *worker_partition_key;
  gint stats_source;

  /* this counter is not thread safe if there are multiple worker threads,
   * in that case, one needs to use LogThreadedDestWorker->seq_num, which is
   * static for a single insert() invocation, whereas this might be
   * increased in parallel by the multiple threads. */

  gint32 shared_seq_num;
  guint32 flags;

  const gchar *(*format_stats_key)(LogThreadedDestDriver *s, StatsClusterKeyBuilder *kb);
};

static inline gboolean
log_threaded_dest_worker_init(LogThreadedDestWorker *self)
{
  if (self->init)
    return self->init(self);
  return TRUE;
}

static inline void
log_threaded_dest_worker_deinit(LogThreadedDestWorker *self)
{
  if (self->deinit)
    self->deinit(self);
}

static inline gboolean
log_threaded_dest_worker_connect(LogThreadedDestWorker *self)
{
  if (self->connect)
    self->connected = self->connect(self);
  else
    self->connected = TRUE;


  stats_counter_set(self->metrics.output_unreachable, !self->connected);
  return self->connected;
}

static inline void
log_threaded_dest_worker_disconnect(LogThreadedDestWorker *self)
{
  if (self->disconnect)
    self->disconnect(self);
  self->connected = FALSE;
  stats_counter_set(self->metrics.output_unreachable, !self->connected);
}

static inline LogThreadedResult
log_threaded_dest_worker_insert(LogThreadedDestWorker *self, LogMessage *msg)
{
  if ((self->owner->flags & LTDF_SEQNUM) &&
      ((self->owner->flags & LTDF_SEQNUM_ALL) || (msg->flags & LF_LOCAL)))
    {
      if (self->owner->num_workers > 1)
        self->seq_num = step_sequence_number_atomic(&self->owner->shared_seq_num);
      else
        self->seq_num = step_sequence_number(&self->owner->shared_seq_num);
    }
  else
    self->seq_num = 0;

  LogThreadedResult result = self->insert(self, msg);

  if (self->metrics.message_delay_sample
      && (result == LTR_QUEUED || result == LTR_SUCCESS || result == LTR_EXPLICIT_ACK_MGMT))
    {
      UnixTime now;

      unix_time_set_now(&now);
      gint64 diff_msec = unix_time_diff_in_msec(&now, &msg->timestamps[LM_TS_RECVD]);

      if (self->metrics.last_delay_update != now.ut_sec)
        {
          stats_counter_set_time(self->metrics.message_delay_sample, diff_msec);
          stats_counter_set_time(self->metrics.message_delay_sample_age, now.ut_sec);
          self->metrics.last_delay_update = now.ut_sec;
        }
    }

  return result;
}

static inline LogThreadedResult
log_threaded_dest_worker_flush(LogThreadedDestWorker *self, LogThreadedFlushMode mode)
{
  LogThreadedResult result = LTR_SUCCESS;

  if (self->flush)
    result = self->flush(self, mode);
  iv_validate_now();
  self->last_flush_time = iv_now;
  return result;
}

/* function for drivers that are not yet using the worker API */
static inline LogThreadedResult
log_threaded_dest_driver_flush(LogThreadedDestDriver *self)
{
  return log_threaded_dest_worker_flush(&self->worker.instance, LTF_FLUSH_NORMAL);
}

void log_threaded_dest_worker_ack_messages(LogThreadedDestWorker *self, gint batch_size);
void log_threaded_dest_worker_drop_messages(LogThreadedDestWorker *self, gint batch_size);
void log_threaded_dest_worker_rewind_messages(LogThreadedDestWorker *self, gint batch_size);
void log_threaded_dest_worker_wakeup_when_suspended(LogThreadedDestWorker *self);
gboolean log_threaded_dest_worker_init_method(LogThreadedDestWorker *self);
void log_threaded_dest_worker_deinit_method(LogThreadedDestWorker *self);
void log_threaded_dest_worker_init_instance(LogThreadedDestWorker *self,
                                            LogThreadedDestDriver *owner,
                                            gint worker_index);
void log_threaded_dest_worker_free_method(LogThreadedDestWorker *self);
void log_threaded_dest_worker_free(LogThreadedDestWorker *self);

void log_threaded_dest_worker_written_bytes_add(LogThreadedDestWorker *self, gsize b);
void log_threaded_dest_driver_insert_msg_length_stats(LogThreadedDestDriver *self, gsize len);
void log_threaded_dest_driver_insert_batch_length_stats(LogThreadedDestDriver *self, gsize len);
void log_threaded_dest_driver_register_aggregated_stats(LogThreadedDestDriver *self);
void log_threaded_dest_driver_unregister_aggregated_stats(LogThreadedDestDriver *self);

gboolean log_threaded_dest_driver_deinit_method(LogPipe *s);
gboolean log_threaded_dest_driver_init_method(LogPipe *s);
gboolean log_threaded_dest_driver_start_workers(LogPipe *s);

void log_threaded_dest_driver_init_instance(LogThreadedDestDriver *self, GlobalConfig *cfg);
void log_threaded_dest_driver_free(LogPipe *s);

void log_threaded_dest_driver_set_max_retries_on_error(LogDriver *s, gint max_retries);
void log_threaded_dest_driver_set_num_workers(LogDriver *s, gint num_workers);
void log_threaded_dest_driver_set_worker_partition_key_ref(LogDriver *s, LogTemplate *key);
void log_threaded_dest_driver_set_flush_on_worker_key_change(LogDriver *s, gboolean f);
void log_threaded_dest_driver_set_batch_lines(LogDriver *s, gint batch_lines);
void log_threaded_dest_driver_set_batch_timeout(LogDriver *s, gint batch_timeout);
void log_threaded_dest_driver_set_time_reopen(LogDriver *s, time_t time_reopen);
gboolean log_threaded_dest_driver_process_flag(LogDriver *driver, const gchar *flag);

#endif
