/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#include "logproto-server.h"
#include "messages.h"
#include "cfg.h"
#include "plugin.h"
#include "plugin-types.h"

/**
 * Find the character terminating the buffer.
 *
 * NOTE: when looking for the end-of-message here, it either needs to be
 * terminated via NUL or via NL, when terminating via NL we have to make
 * sure that there's no NUL left in the message. This function iterates over
 * the input data and returns a pointer to the first occurence of NL or NUL.
 *
 * It uses an algorithm similar to what there's in libc memchr/strchr.
 *
 * NOTE: find_eom is not static as it is used by a unit test program.
 **/
const guchar *
find_eom(const guchar *s, gsize n)
{
  const guchar *char_ptr;
  const gulong *longword_ptr;
  gulong longword, magic_bits, charmask;
  gchar c;

  c = '\n';

  /* align input to long boundary */
  for (char_ptr = s; n > 0 && ((gulong) char_ptr & (sizeof(longword) - 1)) != 0; ++char_ptr, n--)
    {
      if (*char_ptr == c || *char_ptr == '\0')
        return char_ptr;
    }

  longword_ptr = (gulong *) char_ptr;

#if GLIB_SIZEOF_LONG == 8
  magic_bits = 0x7efefefefefefeffL;
#elif GLIB_SIZEOF_LONG == 4
  magic_bits = 0x7efefeffL;
#else
#error "unknown architecture"
#endif
  memset(&charmask, c, sizeof(charmask));

  while (n > sizeof(longword))
    {
      longword = *longword_ptr++;
      if ((((longword + magic_bits) ^ ~longword) & ~magic_bits) != 0 ||
          ((((longword ^ charmask) + magic_bits) ^ ~(longword ^ charmask)) & ~magic_bits) != 0)
        {
          gint i;

          char_ptr = (const guchar *) (longword_ptr - 1);

          for (i = 0; i < sizeof(longword); i++)
            {
              if (*char_ptr == c || *char_ptr == '\0')
                return char_ptr;
              char_ptr++;
            }
        }
      n -= sizeof(longword);
    }

  char_ptr = (const guchar *) longword_ptr;

  while (n-- > 0)
    {
      if (*char_ptr == c || *char_ptr == '\0')
        return char_ptr;
      ++char_ptr;
    }

  return NULL;
}

gboolean
log_proto_server_validate_options_method(LogProtoServer *s)
{
  return TRUE;
}

void
log_proto_server_free_method(LogProtoServer *s)
{
  log_transport_free(s->transport);
}

void
log_proto_server_free(LogProtoServer *s)
{
  if (s->free_fn)
    s->free_fn(s);
  g_free(s);
}

void
log_proto_server_init(LogProtoServer *self, LogTransport *transport, const LogProtoServerOptions *options)
{
  self->validate_options = log_proto_server_validate_options_method;
  self->free_fn = log_proto_server_free_method;
  self->options = options;
  self->transport = transport;
}

gboolean
log_proto_server_options_set_encoding(LogProtoServerOptions *self, const gchar *encoding)
{
  GIConv convert;

  g_free(self->encoding);
  self->encoding = g_strdup(encoding);

  /* validate encoding */
  convert = g_iconv_open("utf-8", encoding);
  if (convert == (GIConv) -1)
    return FALSE;
  g_iconv_close(convert);
  return TRUE;
}

void
log_proto_server_options_defaults(LogProtoServerOptions *options)
{
  memset(options, 0, sizeof(*options));
  options->max_msg_size = -1;
  options->init_buffer_size = -1;
  options->max_buffer_size = -1;
  options->position_tracking_enabled = FALSE;
}

void
log_proto_server_options_init(LogProtoServerOptions *options, GlobalConfig *cfg)
{
  if (options->initialized)
    return;

  if (options->max_msg_size == -1)
    {
      options->max_msg_size = cfg->log_msg_size;
    }
  if (options->max_buffer_size == -1)
    {
      if (options->encoding)
        {
          /* Based on the implementation of LogProtoTextServer, the buffer is yielded as
             a complete message when max_msg_size is reached and there is no EOM in the buffer.
             In worst case, the buffer contains max_msg_size - 1 bytes before the next fetch,
             which can be 6 times max_msg_size due to the utf8 conversion.
             And additional space is required because of the possible leftover bytes.
          */
          options->max_buffer_size = 8 * options->max_msg_size;
        }
      else
        options->max_buffer_size = options->max_msg_size;
    }
  if (options->init_buffer_size == -1)
    options->init_buffer_size = MIN(options->max_msg_size, options->max_buffer_size);
  options->initialized = TRUE;
}

void
log_proto_server_options_destroy(LogProtoServerOptions *options)
{
  g_free(options->encoding);
  if (options->destroy)
    options->destroy(options);
  options->initialized = FALSE;
}

LogProtoServerFactory *
log_proto_server_get_factory(PluginContext *context, const gchar *name)
{
  Plugin *plugin;

  plugin = plugin_find(context, LL_CONTEXT_SERVER_PROTO, name);
  if (plugin && plugin->construct)
    return plugin->construct(plugin);
  return NULL;
}
