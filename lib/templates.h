/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
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
  
#ifndef TEMPLATES_H_INCLUDED
#define TEMPLATES_H_INCLUDED

#include "syslog-ng.h"
#include "timeutils.h"
#include "logmsg.h"

#define LTZ_LOCAL 0
#define LTZ_SEND  1
#define LTZ_MAX   2

#define LOG_TEMPLATE_ERROR log_template_error_quark()

GQuark log_template_error_quark(void);

enum LogTemplateError
{
  LOG_TEMPLATE_ERROR_FAILED,
  LOG_TEMPLATE_ERROR_COMPILE,
};

typedef struct _LogTemplate
{
  gint ref_cnt;
  gchar *name;
  gchar *template;
  GList *compiled_template;
  gboolean escape;
  gboolean def_inline;
  GlobalConfig *cfg;
  GPtrArray *arg_bufs;
} LogTemplate;

typedef struct _LogTemplateOptions
{
  gint ts_format;
  gchar *time_zone[LTZ_MAX];
  TimeZoneInfo *time_zone_info[LTZ_MAX];
  gint frac_digits;
} LogTemplateOptions;

typedef struct _LogMacroDef
{
  char *name;
  int id;
} LogMacroDef;

extern LogMacroDef macros[];

/* appends the formatted output into result */
typedef void (*LogTemplateFunction)(GString *result, LogMessage *msg, gint argc, GString *argv[]);

void log_template_set_escape(LogTemplate *self, gboolean enable);
gboolean log_template_compile(LogTemplate *self, GError **error);
void log_template_format(LogTemplate *self, LogMessage *lm, LogTemplateOptions *opts, gint tz, gint32 seq_num, GString *result);
void log_template_append_format(LogTemplate *self, LogMessage *lm, LogTemplateOptions *opts, gint tz, gint32 seq_num, GString *result);

gboolean log_macro_expand(GString *result, gint id, gboolean escape, LogTemplateOptions *opts, gint tz, gint32 seq_num, LogMessage *msg);

LogTemplate *log_template_new(GlobalConfig *cfg, gchar *name, const gchar *template);
LogTemplate *log_template_ref(LogTemplate *s);
void log_template_unref(LogTemplate *s);


void log_template_options_init(LogTemplateOptions *options, GlobalConfig *cfg);
void log_template_options_destroy(LogTemplateOptions *options);
void log_template_options_defaults(LogTemplateOptions *options);


#endif
