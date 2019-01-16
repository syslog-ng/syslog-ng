/*
 * Copyright (c) 2019 Balabit
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

#ifndef STATE_H
#define STATE_H 1
#include "syslog-ng.h"
#include "jsonc/json.h"
#include "persist-tool.h"

#define RPCTID_PREFIX "next.rcptid"

typedef struct _BaseState
{
  guint8 version;
  guint8 big_endian :1;
} BaseState;

NameValueContainer *name_value_container_new(void);

gboolean name_value_container_parse_json_string(NameValueContainer *self, gchar *json_string);

void name_value_container_add_string(NameValueContainer *self, gchar *name, gchar *value);
void name_value_container_add_boolean(NameValueContainer *self, gchar *name, gboolean value);
void name_value_container_add_int(NameValueContainer *self, gchar *name, gint value);
void name_value_container_add_int64(NameValueContainer *self, gchar *name, gint64 value);
gchar *name_value_container_get_value(NameValueContainer *self, gchar *name);

const gchar *name_value_container_get_string(NameValueContainer *self, gchar *name);
gboolean name_value_container_get_boolean(NameValueContainer *self, gchar *name);
gint name_value_container_get_int(NameValueContainer *self, gchar *name);
gint64 name_value_container_get_int64(NameValueContainer *self, gchar *name);

gchar *name_value_container_get_json_string(NameValueContainer *self);
void name_value_container_foreach(NameValueContainer *self, void
                                  (callback)(gchar *name, const gchar *value, gpointer user_data), gpointer user_data);

gboolean  name_value_container_is_name_exists(NameValueContainer *self, gchar *name);
void name_value_container_free(NameValueContainer *self);

GList *persist_state_get_key_list(PersistState *self);

PersistEntryHandle state_handler_get_handle(StateHandler *self);

gchar *state_handler_get_persist_name(StateHandler *self);

guint8 state_handler_get_persist_version(StateHandler *self);

gpointer state_handler_get_state(StateHandler *self);

PersistEntryHandle state_handler_alloc_state(StateHandler *self);

void state_handler_put_state(StateHandler *self);

gboolean state_handler_load_state(StateHandler *self, NameValueContainer *new_state, GError **error);

gboolean state_handler_rename_entry(StateHandler *self, const gchar *new_name);

gboolean state_handler_entry_exists(StateHandler *self);

NameValueContainer *state_handler_dump_state(StateHandler *self);

void state_handler_free(StateHandler *self);

void state_handler_init(StateHandler *self, PersistState *persist_state, const gchar *name);

STATE_HANDLER_CONSTRUCTOR state_handler_get_constructor_by_prefix(const gchar *prefix);
void state_handler_register_constructor(const gchar *prefix,
                                        STATE_HANDLER_CONSTRUCTOR handler);

void state_handler_register_default_constructors(void);

#endif
