/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2014 Gergely Nagy <algernon@balabit.hu>
 * Copyright (c) 2015 Balazs Scheidler <balazs.scheidler@balabit.com>
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
#include "python-value-pairs.h"
#include "python-helpers.h"
#include "python-types.h"
#include "messages.h"


/** Value pairs **/
static gboolean
python_worker_vp_add_one(const gchar *name,
                         LogMessageValueType type, const gchar *value, gsize value_len,
                         gpointer user_data)
{
  const LogTemplateOptions *template_options = (const LogTemplateOptions *)((gpointer *)user_data)[0];
  PyObject *dict = (PyObject *)((gpointer *)user_data)[1];

  PyObject *obj = py_obj_from_log_msg_value(value, value_len, type);
  if (!obj)
    {
      gchar buf[256];

      msg_error("python-value-pairs: error converting a name-value pair to a Python object",
                evt_tag_str("exception", _py_format_exception_text(buf, sizeof(buf))));
      _py_finish_exception_handling();

      return type_cast_drop_helper(template_options->on_error, value, value_len, log_msg_value_type_to_str(type));
    }

  PyDict_SetItemString(dict, name, obj);
  Py_DECREF(obj);

  return FALSE;
}

/** Main code **/

gboolean
py_value_pairs_apply(ValuePairs *vp, LogTemplateEvalOptions *options, LogMessage *msg,
                     PyObject **dict)
{
  gpointer args[2];
  gboolean vp_ok;

  *dict = PyDict_New();

  args[0] = (gpointer) options->opts;
  args[1] = *dict;

  vp_ok = value_pairs_foreach(vp, python_worker_vp_add_one,
                              msg, options, args);
  if (!vp_ok)
    {
      Py_DECREF(*dict);
      *dict = NULL;
    }
  return vp_ok;
}
