/*
 * Copyright (c) 2015 BalaBit
 * Copyright (c) 2015 Tibor Benke
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

#include "parser.h"
#include <assert.h>

struct NativeParserProxy;

__attribute__((visibility("hidden"))) void
native_parser_proxy_free(struct NativeParserProxy *this);

__attribute__((visibility("hidden"))) gboolean
native_parser_proxy_set_option(struct NativeParserProxy *self, const gchar *key, const gchar *value);

__attribute__((visibility("hidden"))) gboolean
native_parser_proxy_process(struct NativeParserProxy *this, LogParser *super, LogMessage *pmsg, const gchar *input);

__attribute__((visibility("hidden"))) int
native_parser_proxy_init(struct NativeParserProxy *s);

__attribute__((visibility("hidden"))) int
native_parser_proxy_deinit(struct NativeParserProxy *s);

__attribute__((visibility("hidden"))) struct NativeParserProxy *
native_parser_proxy_new(GlobalConfig *cfg);

__attribute__((visibility("hidden"))) struct NativeParserProxy *
native_parser_proxy_clone(struct NativeParserProxy *self);

typedef struct _ParserNative
{
  LogParser super;
  struct NativeParserProxy *native_object;
} ParserNative;

static LogPipe *
native_parser_clone(LogPipe *s);

static gboolean
native_parser_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input,
                      gsize input_len)
{
  ParserNative *self = (ParserNative *) s;

  LogMessage *writable_msg = log_msg_make_writable(pmsg, path_options);
  return native_parser_proxy_process(self->native_object, &self->super, writable_msg, input);
}

gboolean
native_parser_set_option(LogParser *s, gchar *key, gchar *value)
{
  ParserNative *self = (ParserNative *)s;

  return native_parser_proxy_set_option(self->native_object, key, value);
}

static gboolean
native_parser_init(LogPipe *s)
{
  ParserNative *self = (ParserNative *) s;

  return log_parser_init_method(s) && native_parser_proxy_init(self->native_object);
}

static gboolean
native_parser_deinit(LogPipe *s)
{
  ParserNative *self = (ParserNative *) s;

  if (!native_parser_proxy_deinit(self->native_object))
    return FALSE;
  return log_parser_deinit_method(s);
}

static void
native_parser_free(LogPipe *s)
{
  ParserNative *self = (ParserNative *)s;

  native_parser_proxy_free(self->native_object);
  log_parser_free_method(s);
}

static void
__setup_callback_methods(ParserNative *self)
{
  self->super.process = native_parser_process;
  self->super.super.free_fn = native_parser_free;
  self->super.super.clone = native_parser_clone;
  self->super.super.init = native_parser_init;
  self->super.super.deinit = native_parser_deinit;
}

static LogPipe *
native_parser_clone(LogPipe *s)
{
  ParserNative *self = (ParserNative *) s;
  ParserNative *cloned;

  cloned = (ParserNative *) g_new0(ParserNative, 1);
  log_parser_init_instance(&cloned->super, s->cfg);
  cloned->native_object = native_parser_proxy_clone(self->native_object);
  assert(self != cloned);

  if (!cloned->native_object)
    {
      g_free(cloned);
      return NULL;
    }

  __setup_callback_methods(cloned);

  return &cloned->super.super;
}

__attribute__((visibility("hidden"))) LogParser *
native_parser_new(GlobalConfig *cfg)
{
  ParserNative *self = (ParserNative *) g_new0(ParserNative, 1);

  log_parser_init_instance(&self->super, cfg);
  self->native_object = native_parser_proxy_new(cfg);

  if (!self->native_object)
    {
      g_free(self);
      return NULL;
    }

  __setup_callback_methods(self);

  return &self->super;
}
