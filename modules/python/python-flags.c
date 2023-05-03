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

#include "python-flags.h"
#include "python-types.h"
#include "msg-format.h"

typedef struct FlagMapping_
{
  const gchar *name;
  gboolean value;
} FlagMapping;

static void
_set_flag(PyObject *dict, FlagMapping *mapping)
{
  PyObject *value = py_boolean_from_boolean(mapping->value);

  if (PyDict_SetItemString(dict, mapping->name, value) < 0)
    msg_error("python-flags: Failed to set flag", evt_tag_str("name", mapping->name));

  Py_DECREF(value);
}

PyObject *
python_source_flags_new(guint32 flags)
{
  PyObject *dict = PyDict_New();
  if (!dict)
    {
      msg_error("python-flags: Failed to create flags dict");
      return NULL;
    }

  FlagMapping mappings[] =
  {
    { "parse",                    ~flags & LP_NOPARSE },
    { "check-hostname",            flags & LP_CHECK_HOSTNAME },
    { "syslog-protocol",           flags & LP_SYSLOG_PROTOCOL },
    { "assume-utf8",               flags & LP_ASSUME_UTF8 },
    { "validate-utf8",             flags & LP_VALIDATE_UTF8 },
    { "sanitize-utf8",             flags & LP_SANITIZE_UTF8 },
    { "multi-line",               ~flags & LP_NO_MULTI_LINE },
    { "store-legacy-msghdr",       flags & LP_STORE_LEGACY_MSGHDR },
    { "store-raw-message",         flags & LP_STORE_RAW_MESSAGE },
    { "expect-hostname",           flags & LP_EXPECT_HOSTNAME },
    { "guess-timezone",            flags & LP_GUESS_TIMEZONE },
    { "header",                   ~flags & LP_NO_HEADER },
    { "rfc3164-fallback",         ~flags & LP_NO_RFC3164_FALLBACK },
  };

  for (gint i = 0; i < G_N_ELEMENTS(mappings); i++)
    _set_flag(dict, &mappings[i]);

  return dict;
}
