/*
 * Copyright (c) 2019 Balabit
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

#ifndef PERSIST_TOOL_H
#define PERSIST_TOOL_H 1

#include "cfg.h"
#include "persist-state.h"


#define DEFAULT_PERSIST_FILE "syslog-ng.persist"

typedef enum _PersistStateMode
{
  persist_mode_normal = 0,
  persist_mode_dump,
  persist_mode_edit
} PersistStateMode;

typedef struct _PersistTool
{
  PersistState *state;
  PersistStateMode mode;
  GlobalConfig *cfg;
  gchar *persist_filename;
} PersistTool;

PersistTool *persist_tool_new(gchar *persist_filename, PersistStateMode open_mode);

void persist_tool_free(PersistTool *self);

typedef struct _NameValueContainer NameValueContainer;

struct _NameValueContainer
{
  struct json_object *container;
};

typedef struct _StateHandler StateHandler;

struct _StateHandler
{
  gpointer state;
  gchar *name;
  gsize size;
  guint8 version;
  PersistEntryHandle persist_handle;
  PersistState *persist_state;
  NameValueContainer *(*dump_state)(StateHandler *self);
  PersistEntryHandle (*alloc_state)(StateHandler *self);
  gboolean (*load_state)(StateHandler *self, NameValueContainer *new_state, GError **error);
  void (*free_fn)(StateHandler *self);
};

typedef StateHandler *(*STATE_HANDLER_CONSTRUCTOR)(PersistState *persist_state, const gchar *name);

STATE_HANDLER_CONSTRUCTOR state_handler_get_constructor_by_prefix(const gchar *prefix);
void state_handler_register_constructor(const gchar *prefix,
                                        STATE_HANDLER_CONSTRUCTOR handler);

StateHandler *persist_tool_get_state_handler(PersistTool *self, gchar *name);

void persist_tool_revert_changes(PersistTool *self);


#endif
