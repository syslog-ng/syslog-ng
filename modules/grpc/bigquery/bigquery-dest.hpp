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

#ifndef BIGQUERY_DEST_HPP
#define BIGQUERY_DEST_HPP

#include "bigquery-dest.h"

#include "compat/cpp-start.h"
#include "syslog-ng.h"
#include "template/templates.h"
#include "compat/cpp-end.h"

#include <string>

namespace syslog_ng {
namespace bigquery {

class DestinationDriver final
{
public:
  DestinationDriver(BigQueryDestDriver *s);
  ~DestinationDriver();
  bool init();
  bool deinit();
  const gchar *format_persist_name();
  const gchar *format_stats_instance();

  LogTemplateOptions& get_template_options()
  {
    return this->template_options;
  }

  void set_url(std::string u)
  {
    this->url = u;
  }

  void set_project(std::string p)
  {
    this->project = p;
  }

  void set_dataset(std::string d)
  {
    this->dataset = d;
  }

  void set_table(std::string t)
  {
    this->table = t;
  }

private:
  BigQueryDestDriver *super;
  LogTemplateOptions template_options;

  std::string url;
  std::string project;
  std::string dataset;
  std::string table;
};

}
}

#endif
