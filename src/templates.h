/*
 * Copyright (c) 2002, 2003, 2004 BalaBit IT Ltd, Budapest, Hungary
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

#ifndef TEMPLATES_H_INCLUDED
#define TEMPLATES_H_INCLUDED

#include "syslog-ng.h"
#include "logmsg.h"

#define LT_ESCAPE 0x0001

typedef struct _LogTemplate
{
  gint ref_cnt;
  GString *name;
  GString *template;
  GList *compiled_template;
  guint flags;
  gboolean def_inline;
} LogTemplate;

void log_template_set_escape(LogTemplate *self, gboolean enable);
void log_template_format(LogTemplate *self, LogMessage *lm, guint macro_flags, gint ts_format, glong zone_offset, gint frac_digits, GString *result);

LogTemplate *log_template_new(gchar *name, gchar *template);
LogTemplate *log_template_ref(LogTemplate *s);
void log_template_unref(LogTemplate *s);


#endif
