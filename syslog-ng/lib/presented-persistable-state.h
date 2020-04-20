/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 2013 Viktor Juhasz
 * Copyright (c) 2013 Viktor Tusa
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
#ifndef PRESENTED_PERSISTABLE_STATE_H_INCLUDED
#define PRESENTED_PERSISTABLE_STATE_H_INCLUDED
#include "syslog-ng.h"

typedef struct _PresentedPersistableState PresentedPersistableState;

struct _PresentedPersistableState
{
  void (*add_string)(PresentedPersistableState *self, gchar *name, gchar *value);
  void (*add_boolean)(PresentedPersistableState *self, gchar *name, gboolean value);
  void (*add_int64)(PresentedPersistableState *self, gchar *name, gint64 value);
  void (*add_int)(PresentedPersistableState *self, gchar *name, gint value);

  const gchar *(*get_string)(PresentedPersistableState *self, gchar *name);
  gboolean (*get_boolean)(PresentedPersistableState *self, gchar *name);
  gint (*get_int)(PresentedPersistableState *self, gchar *name);
  gint64 (*get_int64)(PresentedPersistableState *self, gchar *name);

  void (*foreach)(PresentedPersistableState *self, void
                  (callback)(gchar *name, const gchar *value, gpointer user_data), gpointer user_data);
  gboolean (*does_name_exist)(PresentedPersistableState *self, gchar *name);
  void (*free)(PresentedPersistableState *self);
};

static inline void
presented_persistable_state_add_string(PresentedPersistableState *self, gchar *name, gchar *value)
{
  self->add_string(self, name, value);
}

static inline void
presented_persistable_state_add_boolean(PresentedPersistableState *self, gchar *name, gboolean value)
{
  self->add_boolean(self, name, value);
}

static inline void
presented_persistable_state_add_int(PresentedPersistableState *self, gchar *name, gint value)
{
  self->add_int(self, name, value);
}

static inline void
presented_persistable_state_add_int64(PresentedPersistableState *self, gchar *name, gint64 value)
{
  self->add_int64(self, name, value);
}

static inline const gchar *
presented_persistable_state_get_string(PresentedPersistableState *self, gchar *name)
{
  return self->get_string(self, name);
}

static inline gboolean
presented_persistable_state_get_boolean(PresentedPersistableState *self, gchar *name)
{
  return self->get_boolean(self, name);
}

static inline gint
presented_persistable_state_get_int(PresentedPersistableState *self, gchar *name)
{
  return self->get_int(self, name);
}

static inline gint64
presented_persistable_state_get_int64(PresentedPersistableState *self, gchar *name)
{
  return self->get_int64(self, name);
}

static inline void
presented_persistable_state_foreach(PresentedPersistableState *self, void (callback)(gchar *name, const gchar *value,
                                    gpointer user_data), gpointer user_data)
{
  self->foreach(self, callback, user_data);
}

static inline gboolean
presented_persistable_state_does_name_exist(PresentedPersistableState *self, gchar *name)
{
  return self->does_name_exist(self, name);
}

static inline void
presented_persistable_state_free(PresentedPersistableState *self)
{
  self->free(self);
}

#endif
