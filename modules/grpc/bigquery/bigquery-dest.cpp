/*
 * Copyright (c) 2023 László Várady
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

#include "bigquery-dest.hpp"
#include "bigquery-worker.hpp"

#include "compat/cpp-start.h"
#include "logthrdest/logthrdestdrv.h"
#include "compat/cpp-end.h"

using syslog_ng::bigquery::DestinationDriver;

struct _BigQueryDestDriver
{
  LogThreadedDestDriver super;
  DestinationDriver *cpp;
};


DestinationDriver::DestinationDriver(BigQueryDestDriver *s) : super(s)
{
  log_template_options_defaults(&this->template_options);
}

DestinationDriver::~DestinationDriver()
{
  log_template_options_destroy(&this->template_options);
}

bool
DestinationDriver::init()
{
  GlobalConfig *cfg = log_pipe_get_config(&this->super->super.super.super.super);
  log_template_options_init(&this->template_options, cfg);

  return log_threaded_dest_driver_init_method(&this->super->super.super.super.super);
}

bool
DestinationDriver::deinit()
{
  return log_threaded_dest_driver_deinit_method(&this->super->super.super.super.super);
}

const gchar *
DestinationDriver::format_persist_name()
{
  static gchar persist_name[1024];

  LogPipe *s = &this->super->super.super.super.super;
  if (s->persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "bigquery.%s", s->persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "bigquery(%s,%s,%s,%s)", this->url.c_str(),
               this->project.c_str(), this->dataset.c_str(), this->table.c_str());

  return persist_name;
}

const gchar *
DestinationDriver::format_stats_instance()
{
  static gchar stats[1024];

  g_snprintf(stats, sizeof(stats), "bigquery,%s,%s,%s,%s", this->url.c_str(),
             this->project.c_str(), this->dataset.c_str(), this->table.c_str());

  return stats;
}


/* C Wrappers */

static const gchar *
_format_persist_name(const LogPipe *s)
{
  BigQueryDestDriver *self = (BigQueryDestDriver *) s;
  return self->cpp->format_persist_name();
}

static const gchar *
_format_stats_instance(LogThreadedDestDriver *s)
{
  BigQueryDestDriver *self = (BigQueryDestDriver *) s;
  return self->cpp->format_stats_instance();
}

void
bigquery_dd_set_url(LogDriver *d, const gchar *url)
{
  BigQueryDestDriver *self = (BigQueryDestDriver *) d;
  self->cpp->set_url(url);
}

void
bigquery_dd_set_project(LogDriver *d, const gchar *project)
{
  BigQueryDestDriver *self = (BigQueryDestDriver *) d;
  self->cpp->set_project(project);
}

void
bigquery_dd_set_dataset(LogDriver *d, const gchar *dataset)
{
  BigQueryDestDriver *self = (BigQueryDestDriver *) d;
  self->cpp->set_dataset(dataset);
}

void bigquery_dd_set_table(LogDriver *d, const gchar *table)
{
  BigQueryDestDriver *self = (BigQueryDestDriver *) d;
  self->cpp->set_table(table);
}

LogTemplateOptions *
bigquery_dd_get_template_options(LogDriver *d)
{
  BigQueryDestDriver *self = (BigQueryDestDriver *) d;
  return &self->cpp->get_template_options();
}

static gboolean
_init(LogPipe *s)
{
  BigQueryDestDriver *self = (BigQueryDestDriver *) s;
  return self->cpp->init();
}

static gboolean
_deinit(LogPipe *s)
{
  BigQueryDestDriver *self = (BigQueryDestDriver *) s;
  return self->cpp->deinit();
}

static void
_free(LogPipe *s)
{
  BigQueryDestDriver *self = (BigQueryDestDriver *) s;
  delete self->cpp;

  log_threaded_dest_driver_free(s);
}

LogDriver *
bigquery_dd_new(GlobalConfig *cfg)
{
  BigQueryDestDriver *self = g_new0(BigQueryDestDriver, 1);

  log_threaded_dest_driver_init_instance(&self->super, cfg);

  self->cpp = new DestinationDriver(self);

  self->super.super.super.super.init = _init;
  self->super.super.super.super.deinit = _deinit;
  self->super.super.super.super.free_fn = _free;
  self->super.super.super.super.generate_persist_name = _format_persist_name;

  self->super.format_stats_instance = _format_stats_instance;
  self->super.stats_source = stats_register_type("bigquery");

  self->super.worker.construct = bigquery_dw_new;

  return &self->super.super.super;
}
