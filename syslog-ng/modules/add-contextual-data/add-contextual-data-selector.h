/*
 * Copyright (c) 2016 Balabit
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

#ifndef ADD_CONTEXTUAL_DATA_SELECTOR_H_INCLUDED
#define ADD_CONTEXTUAL_DATA_SELECTOR_H_INCLUDED

#include "logmsg/logmsg.h"
#include "context-info-db.h"

typedef struct _AddContextualDataSelector AddContextualDataSelector;

struct _AddContextualDataSelector
{
  gboolean ordering_required;
  gchar *(*resolve)(AddContextualDataSelector *self, LogMessage *msg);
  void (*free)(AddContextualDataSelector *self);
  AddContextualDataSelector *(*clone)(AddContextualDataSelector *self, GlobalConfig *cfg);
  gboolean (*init)(AddContextualDataSelector *self, GList *ordered_selectors);
};

static inline gchar *
add_contextual_data_selector_resolve(AddContextualDataSelector *self, LogMessage *msg)
{
  if (self && self->resolve)
    {
      return self->resolve(self, msg);
    }

  return NULL;
}

static inline void
add_contextual_data_selector_free(AddContextualDataSelector *self)
{
  if (self && self->free)
    {
      self->free(self);
    }
  g_free(self);
}

static inline gboolean
add_contextual_data_selector_init(AddContextualDataSelector *self, GList *ordered_selectors)
{
  if (self && self->init)
    {
      return self->init(self, ordered_selectors);
    }

  return FALSE;
}

static inline AddContextualDataSelector *
add_contextual_data_selector_clone(AddContextualDataSelector *self, GlobalConfig *cfg)
{
  if (self && self->clone)
    {
      return self->clone(self, cfg);
    }

  return NULL;
}

static inline gboolean
add_contextual_data_selector_is_ordering_required(AddContextualDataSelector *self)
{
  return self->ordering_required;
}

#endif
