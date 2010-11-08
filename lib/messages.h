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
  
#ifndef MESSAGES_H_INCLUDED
#define MESSAGES_H_INCLUDED

#include "syslog-ng.h"
#include <evtlog.h>

extern int debug_flag;
extern int verbose_flag;
extern int trace_flag;
extern int log_stderr;

typedef void (*MsgPostFunc)(LogMessage *msg);

void msg_set_context(LogMessage *msg);
gboolean msg_limit_internal_message(void);
EVTREC *msg_event_create(gint prio, const char *desc, EVTTAG *tag1, ...);
void msg_event_send(EVTREC *e);

void msg_set_post_func(MsgPostFunc func);
void msg_redirect_to_syslog(const gchar *program_name);
void msg_init(gboolean interactive);
void msg_deinit(void);

void msg_add_option_group(GOptionContext *ctx);

/* fatal->warning goes out to the console during startup, notice and below
 * comes goes to the log even during startup */
#define msg_fatal(desc, tag1, tags...) do { if (msg_limit_internal_message()) { msg_event_send(msg_event_create(EVT_PRI_CRIT, desc, tag1, ##tags )); } } while (0)
#define msg_error(desc, tag1, tags...) do { if (msg_limit_internal_message()) { msg_event_send(msg_event_create(EVT_PRI_ERR, desc, tag1, ##tags )); } } while (0)
#define msg_warning(desc, tag1, tags...) do { if (msg_limit_internal_message()) { msg_event_send(msg_event_create(EVT_PRI_WARNING, desc, tag1, ##tags )); } } while (0)
#define msg_notice(desc, tag1, tags...) do { if (msg_limit_internal_message()) { msg_event_send(msg_event_create(EVT_PRI_NOTICE, desc, tag1, ##tags )); } } while (0)
#define msg_info(desc, tag1, tags...) do { if (msg_limit_internal_message()) { msg_event_send(msg_event_create(EVT_PRI_INFO, desc, tag1, ##tags )); } } while (0)

/* just like msg_info, but prepends the message with a timestamp -- useful in interactive
 * tools with long running time to provide some feedback */
#define msg_progress(desc, tag1, tags...) \
        do { \
          if (msg_limit_internal_message()) { \
            time_t t; \
            char *timestamp, *newdesc; \
            t = time(0); \
            timestamp = ctime(&t); \
            timestamp[strlen(timestamp) - 1] = 0; \
            newdesc = g_strdup_printf("[%s] %s", timestamp, desc); \
            msg_event_send(msg_event_create(EVT_PRI_INFO, newdesc, tag1, ##tags )); \
            g_free(newdesc); \
           }\
        } while (0)

#define msg_verbose(desc, tag1, tags...) \
	do { \
	  if (G_UNLIKELY(verbose_flag))    \
	    msg_info(desc, tag1, ##tags ); \
	} while (0)

#define msg_debug(desc, tag1, tags...) \
	do { \
	  if (G_UNLIKELY(debug_flag))                                   \
	    msg_event_send(msg_event_create(EVT_PRI_DEBUG, desc, tag1, ##tags )); \
	} while (0)

#if ENABLE_DEBUG
#define msg_trace(desc, tag1, tags...) \
	do { \
	  if (G_UNLIKELY(trace_flag))                                   \
            msg_event_send(msg_event_create(EVT_PRI_DEBUG, desc, tag1, ##tags )); \
	} while (0)
#else
#define msg_trace(desc, tag1, tags...)
#endif

void msg_post_message(LogMessage *msg);


#endif
