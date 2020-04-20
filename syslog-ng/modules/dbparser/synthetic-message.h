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
#ifndef PATTERNDB_SYNTHETIC_MESSAGE_H_INCLUDED
#define PATTERNDB_SYNTHETIC_MESSAGE_H_INCLUDED

#include "syslog-ng.h"
#include "correllation-context.h"
#include "template/templates.h"

typedef enum
{
  RAC_MSG_INHERIT_NONE,
  RAC_MSG_INHERIT_LAST_MESSAGE,
  RAC_MSG_INHERIT_CONTEXT
} SyntheticMessageInheritMode;

typedef struct _SyntheticMessage
{
  SyntheticMessageInheritMode inherit_mode;
  GArray *tags;
  GPtrArray *values;
} SyntheticMessage;

LogMessage *synthetic_message_generate_without_context(SyntheticMessage *self, LogMessage *msg);
LogMessage *synthetic_message_generate_with_context(SyntheticMessage *self, CorrellationContext *context);


void synthetic_message_apply(SyntheticMessage *self, CorrellationContext *context, LogMessage *msg);
gboolean synthetic_message_add_value_template_string(SyntheticMessage *self, GlobalConfig *cfg, const gchar *name,
                                                     const gchar *value, GError **error);
void synthetic_message_set_inherit_mode(SyntheticMessage *self, SyntheticMessageInheritMode inherit_mode);
void synthetic_message_set_inherit_properties_string(SyntheticMessage *self, const gchar *inherit_properties,
                                                     GError **error);
gboolean synthetic_message_set_inherit_mode_string(SyntheticMessage *self, const gchar *inherit_mode_name,
                                                   GError **error);
void synthetic_message_add_value_template(SyntheticMessage *self, const gchar *name, LogTemplate *value);
void synthetic_message_add_tag(SyntheticMessage *self, const gchar *text);
void synthetic_message_init(SyntheticMessage *self);
void synthetic_message_deinit(SyntheticMessage *self);
SyntheticMessage *synthetic_message_new(void);
void synthetic_message_free(SyntheticMessage *self);

gint synthetic_message_lookup_inherit_mode(const gchar *inherit_mode);

#endif
