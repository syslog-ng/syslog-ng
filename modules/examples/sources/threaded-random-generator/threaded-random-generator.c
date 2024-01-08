/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2018 László Várady <laszlo.varady@balabit.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "threaded-random-generator.h"
#include "logthrsource/logthrsourcedrv.h"
#include "atomic.h"
#include "messages.h"
#include "str-format.h"

#include <strings.h>
#include <sys/random.h>
#include <unistd.h>

struct ThreadedRandomGeneratorSourceDriver
{
  LogThreadedSourceDriver super;
  GAtomicCounter exit_requested;

  guint freq;
  guint bytes;
  guint flags;
};

static gboolean
_generate_random_bytes(guint8 *random, guint size, guint flags)
{
  guint length = 0;

  while (length < size)
    {
      gssize rc = getrandom(random + length, size - length, flags);

      if (rc < 0)
        {
          msg_error("Could not generate random bytes", evt_tag_error("error"));
          return FALSE;
        }

      length += rc;
    }

  return TRUE;
}

static void
_request_exit(ThreadedRandomGeneratorSourceDriver *self)
{
  g_atomic_counter_set(&self->exit_requested, TRUE);
}

/* runs in a dedicated thread */
static void
_worker_run(LogThreadedSourceWorker *w)
{
  ThreadedRandomGeneratorSourceDriver *control = (ThreadedRandomGeneratorSourceDriver *) w->control;

  guint8 *random_bytes = g_malloc(control->bytes);

  const gsize random_hex_str_size = control->bytes * 2 + 1;
  gchar *random_hex_str = g_malloc(random_hex_str_size);

  while (!g_atomic_counter_get(&control->exit_requested))
    {
      if (_generate_random_bytes(random_bytes, control->bytes, control->flags))
        {
          format_hex_string(random_bytes, control->bytes, random_hex_str, random_hex_str_size);

          LogMessage *msg = log_msg_new_empty();
          log_msg_set_value(msg, LM_V_MESSAGE, random_hex_str, -1);

          log_threaded_source_worker_blocking_post(w, msg);
        }

      usleep(control->freq * 1000);
    }

  g_free(random_hex_str);
  g_free(random_bytes);
}

static void
_worker_request_exit(LogThreadedSourceWorker *w)
{
  ThreadedRandomGeneratorSourceDriver *control = (ThreadedRandomGeneratorSourceDriver *) w->control;

  _request_exit(control);
}

static gboolean
_init(LogPipe *s)
{
  ThreadedRandomGeneratorSourceDriver *self = (ThreadedRandomGeneratorSourceDriver *) s;

  if (!self->bytes)
    {
      msg_error("The bytes() option for random-generator() is mandatory", log_pipe_location_tag(s));
      return FALSE;
    }

  if (!log_threaded_source_driver_init_method(s))
    return FALSE;

  return TRUE;
}

static void
_format_stats_key(LogThreadedSourceDriver *s, StatsClusterKeyBuilder *kb)
{
  ThreadedRandomGeneratorSourceDriver *self = (ThreadedRandomGeneratorSourceDriver *) s;

  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("driver", "random-generator"));

  gchar num[64];
  g_snprintf(num, sizeof(num), "%u", self->bytes);
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("bytes", num));

  g_snprintf(num, sizeof(num), "%u", self->freq);
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("freq", num));

  g_snprintf(num, sizeof(num), "%u", self->flags);
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("flags", num));
}

static LogThreadedSourceWorker *
_construct_worker(LogThreadedSourceDriver *s, gint worker_index)
{
  LogThreadedSourceWorker *worker = g_new0(LogThreadedSourceWorker, 1);
  log_threaded_source_worker_init_instance(worker, s, worker_index);

  worker->run = _worker_run;
  worker->request_exit = _worker_request_exit;

  return worker;
}

void
threaded_random_generator_sd_set_freq(LogDriver *s, gdouble freq)
{
  ThreadedRandomGeneratorSourceDriver *self = (ThreadedRandomGeneratorSourceDriver *) s;
  self->freq = (guint) (freq * 1000);
}

void
threaded_random_generator_sd_set_bytes(LogDriver *s, guint bytes)
{
  ThreadedRandomGeneratorSourceDriver *self = (ThreadedRandomGeneratorSourceDriver *) s;
  self->bytes = bytes;
}

gboolean
threaded_random_generator_sd_set_type(LogDriver *s, const gchar *type)
{
  ThreadedRandomGeneratorSourceDriver *self = (ThreadedRandomGeneratorSourceDriver *) s;

  if (strcasecmp(type, "random") == 0)
    self->flags = GRND_RANDOM;
  else if (strcasecmp(type, "urandom") == 0)
    self->flags = 0;
  else
    return FALSE;

  return TRUE;
}

LogDriver *
threaded_random_generator_sd_new(GlobalConfig *cfg)
{
  ThreadedRandomGeneratorSourceDriver *self = g_new0(ThreadedRandomGeneratorSourceDriver, 1);
  log_threaded_source_driver_init_instance(&self->super, cfg);

  self->freq = 1000;
  self->flags = GRND_RANDOM;
  g_atomic_counter_set(&self->exit_requested, FALSE);

  self->super.super.super.super.init = _init;
  self->super.format_stats_key = _format_stats_key;
  self->super.worker_construct = _construct_worker;

  return &self->super.super.super;
}
