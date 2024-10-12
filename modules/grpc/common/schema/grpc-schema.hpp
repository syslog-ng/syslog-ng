/*
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 2024 Attila Szakacs <attila.szakacs@axoflow.com>
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

#ifndef GRPC_SCHEMA_HPP
#define GRPC_SCHEMA_HPP

#include "syslog-ng.h"

#include "compat/cpp-start.h"
#include "template/templates.h"
#include "compat/cpp-end.h"

namespace syslogng {
namespace grpc {

struct NameValueTemplatePair
{
  std::string name;
  LogTemplate *value;

  NameValueTemplatePair(std::string name_, LogTemplate *value_)
    : name(name_), value(log_template_ref(value_)) {}

  NameValueTemplatePair(const NameValueTemplatePair &a)
    : name(a.name), value(log_template_ref(a.value)) {}

  NameValueTemplatePair &operator=(const NameValueTemplatePair &a)
  {
    name = a.name;
    log_template_unref(value);
    value = log_template_ref(a.value);

    return *this;
  }

  ~NameValueTemplatePair()
  {
    log_template_unref(value);
  }

};

class Schema
{
};

}
}

#endif
