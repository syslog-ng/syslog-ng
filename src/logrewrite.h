/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef LOGREWRITE_H_INCLUDED
#define LOGREWRITE_H_INCLUDED

#include "logmsg.h"
#include "messages.h"
#include "logprocess.h"
#include "templates.h"
#include "logmatcher.h"

typedef struct _LogRewrite LogRewrite;

struct _LogRewrite
{
  const gchar *value_name;
  void (*process)(LogRewrite *s, LogMessage *msg);
  void (*free_fn)(LogRewrite *s);
};


LogRewrite *log_rewrite_subst_new(const gchar *replacement);
LogRewrite *log_rewrite_set_new(const gchar *new_value);
void log_rewrite_free(LogRewrite *self);
gboolean log_rewrite_set_regexp(LogRewrite *s, const gchar *regexp);
void log_rewrite_set_matcher(LogRewrite *s, LogMatcher *matcher);
void log_rewrite_set_flags(LogRewrite *s, gint flags);

LogProcessRule *log_rewrite_rule_new(const gchar *name, GList *rewrite_list);


#endif

