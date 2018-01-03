/*
 * Copyright (c) 2011-2014 Balabit
 * Copyright (c) 2011-2014 Gergely Nagy <algernon@balabit.hu>
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

#ifndef AFSMTP_H_INCLUDED
#define AFSMTP_H_INCLUDED

#include "driver.h"

typedef enum
{
  AFSMTP_RCPT_TYPE_NONE,
  AFSMTP_RCPT_TYPE_TO,
  AFSMTP_RCPT_TYPE_CC,
  AFSMTP_RCPT_TYPE_BCC,
  AFSMTP_RCPT_TYPE_REPLY_TO,
  AFSMTP_RCPT_TYPE_SENDER,
} afsmtp_rcpt_type_t;

LogDriver *afsmtp_dd_new(GlobalConfig *cfg);

void afsmtp_dd_set_host(LogDriver *d, const gchar *host);
void afsmtp_dd_set_port(LogDriver *d, gint port);

void afsmtp_dd_set_subject(LogDriver *d, LogTemplate *subject);
void afsmtp_dd_set_from(LogDriver *d, LogTemplate *phrase, LogTemplate *mbox);
void afsmtp_dd_add_rcpt(LogDriver *d, afsmtp_rcpt_type_t type,
                        LogTemplate *phrase, LogTemplate *mbox);
void afsmtp_dd_set_body(LogDriver *d, LogTemplate *body);
gboolean afsmtp_dd_add_header(LogDriver *d, const gchar *header,
                              LogTemplate *value);
LogTemplateOptions *afsmtp_dd_get_template_options(LogDriver *d);

#endif
