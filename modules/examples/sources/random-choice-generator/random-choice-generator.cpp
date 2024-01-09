/*
 * Copyright (c) 2023 Attila Szakacs
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

#include "random-choice-generator.hpp"

#include "compat/cpp-start.h"
#include "string-list.h"
#include "compat/cpp-end.h"

#include <unistd.h>

#define get_SourceDriver(s) (((RandomChoiceGeneratorSourceDriver *) (s))->cpp)
#define get_SourceWorker(s) (((RandomChoiceGeneratorSourceWorker *) (s))->cpp)

using namespace syslogng::examples::random_choice_generator;

/* C++ Implementations */

SourceDriver::SourceDriver(RandomChoiceGeneratorSourceDriver *s)
  : super(s)
{
}

void
SourceDriver::set_choices(GList *choices_)
{
  for (GList *elem = g_list_first(choices_); elem; elem = elem->next)
    {
      const gchar *choice = (const gchar *) elem->data;
      choices.push_back(choice);
    }

  string_list_free(choices_);
}

void
SourceDriver::set_freq(gdouble freq_)
{
  freq = freq_;
}

void
SourceDriver::request_exit()
{
  exit_requested = true;
}

void
SourceDriver::format_stats_key(StatsClusterKeyBuilder *kb)
{
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("driver", "random-choice-generator"));
}

gboolean
SourceDriver::init()
{
  if (choices.size() == 0)
    {
      msg_error("random-choice-generator: choices() option is mandatory",
                log_pipe_location_tag(&super->super.super.super.super));
      return FALSE;
    }

  return log_threaded_source_driver_init_method(&super->super.super.super.super);
}

gboolean
SourceDriver::deinit()
{
  return log_threaded_source_driver_deinit_method(&super->super.super.super.super);
}

SourceWorker::SourceWorker(RandomChoiceGeneratorSourceWorker *s, SourceDriver &d)
  : super(s), driver(d)
{
}

void
SourceWorker::run()
{
  while (!driver.exit_requested)
    {
      std::string random_choice = driver.choices[rand() % driver.choices.size()];

      LogMessage *msg = log_msg_new_empty();
      log_msg_set_value(msg, LM_V_MESSAGE, random_choice.c_str(), -1);

      log_threaded_source_worker_blocking_post(&super->super, msg);

      usleep((useconds_t)(driver.freq * 1000));
    }
}

void
SourceWorker::request_exit()
{
  driver.request_exit();
}

/* C Wrappers */

void
random_choice_generator_set_choices(LogDriver *s, GList *choices)
{
  get_SourceDriver(s)->set_choices(choices);
}

void
random_choice_generator_set_freq(LogDriver *s, gdouble freq)
{
  get_SourceDriver(s)->set_freq(freq);
}

static void
_worker_run(LogThreadedSourceWorker *s)
{
  get_SourceWorker(s)->run();
}

static void
_worker_request_exit(LogThreadedSourceWorker *s)
{
  get_SourceWorker(s)->request_exit();
}

static void
_format_stats_key(LogThreadedSourceDriver *s, StatsClusterKeyBuilder *kb)
{
  get_SourceDriver(s)->format_stats_key(kb);
}

static gboolean
_init(LogPipe *s)
{
  return get_SourceDriver(s)->init();
}

static gboolean
_deinit(LogPipe *s)
{
  return get_SourceDriver(s)->deinit();
}

static void
_free(LogPipe *s)
{
  delete get_SourceDriver(s);
  log_threaded_source_driver_free_method(s);
}

static void
_worker_free(LogPipe *s)
{
  delete get_SourceWorker(s);
  log_threaded_source_worker_free(s);
}

static LogThreadedSourceWorker *
_construct_worker(LogThreadedSourceDriver *s, gint worker_index)
{
  RandomChoiceGeneratorSourceWorker *worker = g_new0(RandomChoiceGeneratorSourceWorker, 1);
  log_threaded_source_worker_init_instance(&worker->super, s, worker_index);

  worker->cpp = new SourceWorker(worker, *get_SourceDriver(s));

  worker->super.run = _worker_run;
  worker->super.request_exit = _worker_request_exit;
  worker->super.super.super.free_fn = _worker_free;

  return &worker->super;
}

LogDriver *
random_choice_generator_sd_new(GlobalConfig *cfg)
{
  RandomChoiceGeneratorSourceDriver *s = g_new0(RandomChoiceGeneratorSourceDriver, 1);
  log_threaded_source_driver_init_instance(&s->super, cfg);

  s->cpp = new SourceDriver(s);

  s->super.super.super.super.init = _init;
  s->super.super.super.super.deinit = _deinit;
  s->super.super.super.super.free_fn = _free;

  s->super.format_stats_key = _format_stats_key;
  s->super.worker_construct = _construct_worker;

  return &s->super.super.super;
}
