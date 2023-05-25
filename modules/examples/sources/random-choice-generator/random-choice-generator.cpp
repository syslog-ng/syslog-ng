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

#include <unistd.h>

#include "random-choice-generator.h"
#include "random-choice-generator.hpp"

#include "compat/cpp-start.h"
#include "string-list.h"
#include "compat/cpp-end.h"

#define get_RandomChoiceGeneratorCpp(s) (((RandomChoiceGeneratorSourceDriver *) (s))->cpp)

/* C++ Implementations */

RandomChoiceGeneratorCpp::RandomChoiceGeneratorCpp(RandomChoiceGeneratorSourceDriver *s)
  : super(s)
{
}

void
RandomChoiceGeneratorCpp::run()
{
  while (!exit_requested)
    {
      std::string random_choice = choices[rand() % choices.size()];

      LogMessage *msg = log_msg_new_empty();
      log_msg_set_value(msg, LM_V_MESSAGE, random_choice.c_str(), -1);

      log_threaded_source_blocking_post(&super->super, msg);

      usleep((useconds_t)(freq * 1000));
    }
}

void
RandomChoiceGeneratorCpp::set_choices(GList *choices_)
{
  for (GList *elem = g_list_first(choices_); elem; elem = elem->next)
    {
      const gchar *choice = (const gchar *) elem->data;
      choices.push_back(choice);
    }

  string_list_free(choices_);
}

void
RandomChoiceGeneratorCpp::set_freq(gdouble freq_)
{
  freq = freq_;
}

void
RandomChoiceGeneratorCpp::request_exit()
{
  exit_requested = true;
}

const gchar *
RandomChoiceGeneratorCpp::format_stats_instance()
{
  static gchar persist_name[1024];

  if (super->super.super.super.super.persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "random_choice_generator,%s",
               super->super.super.super.super.persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "random_choice_generator");

  return persist_name;
}

gboolean
RandomChoiceGeneratorCpp::init()
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
RandomChoiceGeneratorCpp::deinit()
{
  return log_threaded_source_driver_deinit_method(&super->super.super.super.super);
}

/* C Wrappers */

static void
_run(LogThreadedSourceDriver *s)
{
  get_RandomChoiceGeneratorCpp(s)->run();
}

void
random_choice_generator_set_choices(LogDriver *s, GList *choices)
{
  get_RandomChoiceGeneratorCpp(s)->set_choices(choices);
}

void
random_choice_generator_set_freq(LogDriver *s, gdouble freq)
{
  get_RandomChoiceGeneratorCpp(s)->set_freq(freq);
}

static void
_request_exit(LogThreadedSourceDriver *s)
{
  get_RandomChoiceGeneratorCpp(s)->request_exit();
}

static const gchar *
_format_stats_instance(LogThreadedSourceDriver *s)
{
  return get_RandomChoiceGeneratorCpp(s)->format_stats_instance();
}

static gboolean
_init(LogPipe *s)
{
  return get_RandomChoiceGeneratorCpp(s)->init();
}

static gboolean
_deinit(LogPipe *s)
{
  return get_RandomChoiceGeneratorCpp(s)->deinit();
}

static void
_free(LogPipe *s)
{
  delete get_RandomChoiceGeneratorCpp(s);
  log_threaded_source_driver_free_method(s);
}

LogDriver *
random_choice_generator_sd_new(GlobalConfig *cfg)
{
  RandomChoiceGeneratorSourceDriver *s = g_new0(RandomChoiceGeneratorSourceDriver, 1);
  log_threaded_source_driver_init_instance(&s->super, cfg);

  s->cpp = new RandomChoiceGeneratorCpp(s);

  s->super.super.super.super.init = _init;
  s->super.super.super.super.deinit = _deinit;
  s->super.super.super.super.free_fn = _free;

  s->super.format_stats_instance = _format_stats_instance;
  s->super.run = _run;
  s->super.request_exit = _request_exit;

  return &s->super.super.super;
}
