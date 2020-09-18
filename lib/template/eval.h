/*
 * Copyright (c) 2020 Balabit
 * Copyright (c) 2020 Balazs Scheidler <bazsi77@gmail.com>
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

#ifndef TEMPLATE_EVAL_H_INCLUDED
#define TEMPLATE_EVAL_H_INCLUDED 1

#include "syslog-ng.h"
#include "common-template-typedefs.h"
#include "timeutils/zoneinfo.h"

#define LTZ_LOCAL 0
#define LTZ_SEND  1
#define LTZ_MAX   2

/* template expansion options that can be influenced by the user and
 * is static throughout the runtime for a given configuration. There
 * are call-site specific options too, those are specified as
 * arguments to log_template_format() */
struct _LogTemplateOptions
{
  gboolean initialized;
  /* timestamp format as specified by ts_format() */
  gint ts_format;
  /* number of digits in the fraction of a second part, specified using frac_digits() */
  gint frac_digits;
  gboolean use_fqdn;

  /* timezone for LTZ_LOCAL/LTZ_SEND settings */
  gchar *time_zone[LTZ_MAX];
  TimeZoneInfo *time_zone_info[LTZ_MAX];

  /* Template error handling settings */
  gint on_error;
};

typedef struct _LogTemplateEvalOptions
{
  /* options for recursive template evaluation, inherited from the parent */
  const LogTemplateOptions *opts;
  gint tz;
  gint seq_num;
  const gchar *context_id;
} LogTemplateEvalOptions;

#define DEFAULT_TEMPLATE_EVAL_OPTIONS ((LogTemplateEvalOptions){NULL, LTZ_LOCAL, 0, NULL})

void log_template_format(LogTemplate *self, LogMessage *lm, LogTemplateEvalOptions *options, GString *result);
void log_template_append_format(LogTemplate *self, LogMessage *lm, LogTemplateEvalOptions *options, GString *result);
void log_template_append_format_with_context(LogTemplate *self, LogMessage **messages, gint num_messages,
                                             LogTemplateEvalOptions *options, GString *result);
void log_template_format_with_context(LogTemplate *self, LogMessage **messages, gint num_messages,
                                      LogTemplateEvalOptions *options, GString *result);

#endif
