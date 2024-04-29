/*
 * Copyright (c) 2023 shifter
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

#include "cfg-parser.h"
#include "plugin.h"
#include "plugin-types.h"
#include "filterx/filterx-object.h"
#include "scratch-buffers.h"
#include "example-filterx-func-plugin.h"
#include "filterx/object-primitive.h"

static FilterXObject *
echo(GPtrArray *args)
{
  GString *buf = scratch_buffers_alloc();
  LogMessageValueType t;

  if (args == NULL ||
      args->len < 1)
    {
      return NULL;
    }

  for (int i = 0; i < args->len; i++)
    {
      if (!filterx_object_marshal(args->pdata[i], buf, &t))
        goto exit;
      msg_debug("FILTERX EXAMPLE ECHO",
                evt_tag_str("value", buf->str),
                evt_tag_str("type", log_msg_value_type_to_str(t)));
    }
  if (args->len > 0)
    return filterx_object_ref(args->pdata[0]);
exit:
  return filterx_boolean_new(FALSE);
}

gpointer
example_filterx_simple_func_construct_echo(Plugin *self)
{
  return (gpointer) &echo;
}
