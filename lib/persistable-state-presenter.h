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
#ifndef PERSISTABLE_STATE_PRESENTER_H_INCLUDED
#define PERSISTABLE_STATE_PRESENTER_H_INCLUDED

#include "syslog-ng.h"
#include "persist-state.h"
#include "presented-persistable-state.h"
#include "persistable-state-header.h"

struct _PersistableStatePresenter
{
  gboolean (*dump)(PersistableStateHeader *state, PresentedPersistableState *representation);
  gboolean (*load)(PersistableStateHeader *state, PresentedPersistableState *representation);
  PersistEntryHandle (*alloc)(PersistState *state, const gchar *name);
};

typedef struct _PersistableStatePresenter PersistableStatePresenter;

static inline gboolean
persistable_state_presenter_load(PersistableStatePresenter *self, PersistableStateHeader *state,
                                 PresentedPersistableState *representation)
{
  g_assert(self->load != NULL);
  return self->load(state, representation);
}

static inline gboolean
persistable_state_presenter_dump(PersistableStatePresenter *self, PersistableStateHeader *state,
                                 PresentedPersistableState *representation)
{
  g_assert(self->dump != NULL);
  return self->dump(state, representation);
}

static inline PersistEntryHandle
persistable_state_presenter_alloc(PersistableStatePresenter *self, PersistState *state, const gchar *name)
{
  g_assert(self->alloc != NULL);
  return self->alloc(state, name);
}

typedef PersistableStatePresenter *(*PersistableStatePresenterConstructFunc)(const gchar *name);

PersistableStatePresenterConstructFunc persistable_state_presenter_get_constructor_by_prefix(const gchar *prefix);
void persistable_state_presenter_register_constructor(const gchar *prefix,
                                                      PersistableStatePresenterConstructFunc handler);

#endif
