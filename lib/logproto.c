/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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


#include "logproto.h"
#include "messages.h"
#include "persist-state.h"
#include "serialize.h"
#include "compat.h"
#include "misc.h"
#include "plugin.h"

#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <limits.h>
#include <regex.h>

gboolean
log_proto_set_encoding(LogProto *self, const gchar *encoding)
{
  if (self->convert != (GIConv) -1)
    {
      g_iconv_close(self->convert);
      self->convert = (GIConv) -1;
    }
  if (self->encoding)
    {
      g_free(self->encoding);
      self->encoding = NULL;
    }

  self->convert = g_iconv_open("utf-8", encoding);
  if (self->convert == (GIConv) -1)
    return FALSE;

  self->encoding = g_strdup(encoding);
  return TRUE;
}

void
log_proto_free(LogProto *s)
{
  if (s->free_fn)
    s->free_fn(s);
  if (s->convert != (GIConv) -1)
    g_iconv_close(s->convert);
  if (s->encoding)
    g_free(s->encoding);
  if (s->transport)
    log_transport_free(s->transport);
  g_free(s);
}

LogProtoFactory *
log_proto_get_factory(GlobalConfig *cfg,LogProtoType type, const gchar *name)
{
  Plugin *plugin;
  gint plugin_type = type == LPT_SERVER ? LL_CONTEXT_SERVER_PROTO : LL_CONTEXT_CLIENT_PROTO;
  plugin = plugin_find(cfg, plugin_type, name);
  if (plugin && plugin->construct)
    {
      return plugin->construct(plugin,cfg,plugin_type,name);
    }
  else
    {
      return NULL;
    }
}
