/*
 * Copyright (c) 2002-2013, 2015 Balabit
 * Copyright (c) 1998-2013, 2015 Balázs Scheidler
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
#include "correllation-key.h"
#include "logmsg/logmsg.h"
#include <string.h>


/*********************************************************
 * CorrellationKey, is the key in the state hash table
 *********************************************************/

guint
correllation_key_hash(gconstpointer k)
{
  CorrellationKey *key = (CorrellationKey *) k;
  guint hash;

  hash = (key->scope << 30);
  switch (key->scope)
    {
    case RCS_PROCESS:
      hash += g_str_hash(key->pid);
    case RCS_PROGRAM:
      hash += g_str_hash(key->program);
    case RCS_HOST:
      hash += g_str_hash(key->host);
    case RCS_GLOBAL:
      break;
    default:
      g_assert_not_reached();
      break;
    }
  return hash + g_str_hash(key->session_id);
}

gboolean
correllation_key_equal(gconstpointer k1, gconstpointer k2)
{
  CorrellationKey *key1 = (CorrellationKey *) k1;
  CorrellationKey *key2 = (CorrellationKey *) k2;

  if (key1->scope != key2->scope)
    return FALSE;

  switch (key1->scope)
    {
    case RCS_PROCESS:
      if (strcmp(key1->pid, key2->pid) != 0)
        return FALSE;
    case RCS_PROGRAM:
      if (strcmp(key1->program, key2->program) != 0)
        return FALSE;
    case RCS_HOST:
      if (strcmp(key1->host, key2->host) != 0)
        return FALSE;
    case RCS_GLOBAL:
      break;
    default:
      g_assert_not_reached();
      break;
    }
  if (strcmp(key1->session_id, key2->session_id) != 0)
    return FALSE;
  return TRUE;
}

/* fills a CorrellationKey structure with borrowed values */
void
correllation_key_init(CorrellationKey *self, CorrellationScope scope, LogMessage *msg, gchar *session_id)
{
  memset(self, 0, sizeof(*self));
  self->scope = scope;
  self->session_id = session_id;

  /* NVTable ensures that builtin name-value pairs are always NUL terminated */
  switch (scope)
    {
    case RCS_PROCESS:
      self->pid = log_msg_get_value(msg, LM_V_PID, NULL);
    case RCS_PROGRAM:
      self->program = log_msg_get_value(msg, LM_V_PROGRAM, NULL);
    case RCS_HOST:
      self->host = log_msg_get_value(msg, LM_V_HOST, NULL);
    case RCS_GLOBAL:
      break;
    default:
      g_assert_not_reached();
      break;
    }
}

gint
correllation_key_lookup_scope(const gchar *scope)
{
  if (strcasecmp(scope, "global") ==  0)
    return RCS_GLOBAL;
  else if (strcasecmp(scope, "host") == 0)
    return RCS_HOST;
  else if (strcasecmp(scope, "program") == 0)
    return RCS_PROGRAM;
  else if (strcasecmp(scope, "process") == 0)
    return RCS_PROCESS;
  return -1;
}
