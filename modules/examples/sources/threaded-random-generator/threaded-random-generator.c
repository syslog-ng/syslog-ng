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


/* runs in a dedicated thread */
static void
_run(LogThreadedSourceDriver *s)
{
  ThreadedRandomGeneratorSourceDriver *self = (ThreadedRandomGeneratorSourceDriver *) s;

  guint8 *random_bytes = g_malloc(self->bytes);

  const gsize random_hex_str_size = self->bytes * 2 + 1;
  gchar *random_hex_str = g_malloc(random_hex_str_size);

  while (!g_atomic_counter_get(&self->exit_requested))
    {
      if (_generate_random_bytes(random_bytes, self->bytes, self->flags))
        {
          format_hex_string(random_bytes, self->bytes, random_hex_str, random_hex_str_size);

          LogMessage *msg = log_msg_new_empty();
          log_msg_set_value(msg, LM_V_MESSAGE, random_hex_str, -1);

          log_threaded_source_blocking_post(s, msg);
        }

      usleep(self->freq * 1000);
    }

  g_free(random_hex_str);
  g_free(random_bytes);
}

static void
_request_exit(LogThreadedSourceDriver *s)
{
  ThreadedRandomGeneratorSourceDriver *self = (ThreadedRandomGeneratorSourceDriver *) s;

  g_atomic_counter_set(&self->exit_requested, TRUE);
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

  return log_threaded_source_driver_init_method(s);
}

static const gchar *
_format_stats_instance(LogThreadedSourceDriver *s)
{
  ThreadedRandomGeneratorSourceDriver *self = (ThreadedRandomGeneratorSourceDriver *) s;
  static gchar persist_name[1024];

  if (s->super.super.super.persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "random-generator,%s", s->super.super.super.persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "random-generator,%u,%u,%u", self->bytes, self->freq, self->flags);

  return persist_name;
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

  log_threaded_source_driver_set_worker_run_func(&self->super, _run);
  log_threaded_source_driver_set_worker_request_exit_func(&self->super, _request_exit);

  self->super.super.super.super.init = _init;
  self->super.format_stats_instance = _format_stats_instance;

  return &self->super.super.super;
}
