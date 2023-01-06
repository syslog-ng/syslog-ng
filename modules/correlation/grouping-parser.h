/*
 * Copyright (c) 2015 BalaBit
 * Copyright (c) 2023 Balazs Scheidler <bazsi77@gmail.com>
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
#ifndef CORRELATION_GROUPING_PARSER_H_INCLUDED
#define CORRELATION_GROUPING_PARSER_H_INCLUDED

#include "stateful-parser.h"
#include "correlation.h"
#include <iv.h>

typedef struct _GroupingParser GroupingParser;

struct _GroupingParser
{
  StatefulParser super;
  struct iv_timer tick;
  CorrelationState *correlation;
  LogTemplate *key_template;
  LogTemplate *sort_key_template;
  gint timeout;
  CorrelationScope scope;
  gboolean (*filter_messages)(GroupingParser *self, LogMessage **pmsg, const LogPathOptions *path_options);
  CorrelationContext *(*construct_context)(GroupingParser *self, CorrelationKey *key);
  gboolean (*is_context_complete)(GroupingParser *self, CorrelationContext *context);
  LogMessage *(*aggregate_context)(GroupingParser *self, CorrelationContext *context);
};

static inline gboolean
grouping_parser_filter_messages(GroupingParser *self, LogMessage **pmsg, const LogPathOptions *path_options)
{
  if (self->filter_messages)
    return self->filter_messages(self, pmsg, path_options);
  return TRUE;
}

static inline gboolean
grouping_parser_is_context_complete(GroupingParser *self, CorrelationContext *context)
{
  if (self->is_context_complete)
    return self->is_context_complete(self, context);
  return FALSE;
}

static inline CorrelationContext *
grouping_parser_construct_context(GroupingParser *self, CorrelationKey *key)
{
  if (self->construct_context)
    return self->construct_context(self, key);
  return correlation_context_new(key);
}

LogMessage *grouping_parser_aggregate_context(GroupingParser *self, CorrelationContext *context);

void grouping_parser_set_key_template(LogParser *s, LogTemplate *key_template);
void grouping_parser_set_sort_key_template(LogParser *s, LogTemplate *sort_key);
void grouping_parser_set_scope(LogParser *s, CorrelationScope scope);
void grouping_parser_set_timeout(LogParser *s, gint timeout);


CorrelationContext *grouping_parser_lookup_or_create_context(GroupingParser *self, LogMessage *msg);
void grouping_parser_perform_grouping(GroupingParser *s, LogMessage *msg);

gboolean
grouping_parser_process_method(LogParser *s,
                               LogMessage **pmsg, const LogPathOptions *path_options,
                               const char *input, gsize input_len);
gboolean grouping_parser_init_method(LogPipe *s);
gboolean grouping_parser_deinit_method(LogPipe *s);

void grouping_parser_free_method(LogPipe *s);
void grouping_parser_init_instance(GroupingParser *self, GlobalConfig *cfg);

void grouping_parser_global_init(void);

#endif
