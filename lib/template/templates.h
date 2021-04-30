/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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
#include "eval.h"
#include "timeutils/zoneinfo.h"
#include "type-hinting.h"
#include "common-template-typedefs.h"
#include "atomic.h"

#define LOG_TEMPLATE_ERROR log_template_error_quark()

GQuark log_template_error_quark(void);

enum LogTemplateError
{
  LOG_TEMPLATE_ERROR_FAILED,
  LOG_TEMPLATE_ERROR_COMPILE,
};

typedef enum
{
  ON_ERROR_DROP_MESSAGE        = 0x01,
  ON_ERROR_DROP_PROPERTY       = 0x02,
  ON_ERROR_FALLBACK_TO_STRING  = 0x04, /* Valid for type hinting
                                          only! */
  ON_ERROR_SILENT              = 0x08
} LogTemplateOnError;

/* structure that represents an expandable syslog-ng template */
struct _LogTemplate
{
  GAtomicCounter ref_cnt;
  gchar *name;
  gchar *template;
  GList *compiled_template;
  GlobalConfig *cfg;
  guint escape:1, def_inline:1, trivial:1;
  TypeHint type_hint;
};

/* appends the formatted output into result */

void log_template_set_escape(LogTemplate *self, gboolean enable);
gboolean log_template_set_type_hint(LogTemplate *self, const gchar *hint, GError **error);
gboolean log_template_compile(LogTemplate *self, const gchar *template, GError **error);
void log_template_compile_literal_string(LogTemplate *self, const gchar *literal);
gboolean log_template_is_literal_string(const LogTemplate *self);
const gchar *log_template_get_literal_value(const LogTemplate *self, gssize *value_len);
gboolean log_template_is_trivial(LogTemplate *self);
const gchar *log_template_get_trivial_value(LogTemplate *self, LogMessage *msg, gssize *value_len);
void log_template_set_name(LogTemplate *self, const gchar *name);

LogTemplate *log_template_new(GlobalConfig *cfg, const gchar *name);
LogTemplate *log_template_ref(LogTemplate *s);
void log_template_unref(LogTemplate *s);

void log_template_options_init(LogTemplateOptions *options, GlobalConfig *cfg);
void log_template_options_destroy(LogTemplateOptions *options);
void log_template_options_defaults(LogTemplateOptions *options);

void log_template_global_init(void);
void log_template_global_deinit(void);

gboolean log_template_on_error_parse(const gchar *on_error, gint *out);
void log_template_options_set_on_error(LogTemplateOptions *options, gint on_error);

#endif
