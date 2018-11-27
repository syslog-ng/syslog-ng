/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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
#include <errno.h>
#include <evtlog.h>

extern int startup_debug_flag;
extern int debug_flag;
extern int verbose_flag;
extern int trace_flag;
extern int log_stderr;

typedef void (*MsgPostFunc)(LogMessage *msg);

void msg_set_context(LogMessage *msg);
EVTREC *msg_event_create(gint prio, const char *desc, EVTTAG *tag1, ...) __attribute__((nonnull(2)));
EVTREC *msg_event_create_from_desc(gint prio, const char *desc);
void msg_event_free(EVTREC *e);
void msg_event_send(EVTREC *e);
void msg_event_suppress_recursions_and_send(EVTREC *e);
void msg_event_print_event_to_stderr(EVTREC *e);


void msg_set_post_func(MsgPostFunc func);
void msg_init(gboolean interactive);
void msg_deinit(void);

void msg_add_option_group(GOptionContext *ctx);

#define evt_tag_error(tag) evt_tag_errno(tag, __local_copy_of_errno)

#define CAPTURE_ERRNO(lambda) do {\
  int __local_copy_of_errno G_GNUC_UNUSED = errno; \
  lambda; \
} while(0)

/* fatal->warning goes out to the console during startup, notice and below
 * comes goes to the log even during startup */
#define msg_fatal(desc, tags...)    CAPTURE_ERRNO(\
    msg_event_suppress_recursions_and_send(msg_event_create(EVT_PRI_CRIT, desc, ##tags, NULL )))
#define msg_error(desc, tags...)    CAPTURE_ERRNO(\
    msg_event_suppress_recursions_and_send(msg_event_create(EVT_PRI_ERR, desc, ##tags, NULL )))
#define msg_warning(desc, tags...)  CAPTURE_ERRNO(\
    msg_event_suppress_recursions_and_send(msg_event_create(EVT_PRI_WARNING, desc, ##tags, NULL )))
#define msg_notice(desc, tags...)   CAPTURE_ERRNO(\
    msg_event_suppress_recursions_and_send(msg_event_create(EVT_PRI_NOTICE, desc, ##tags, NULL )))
#define msg_info(desc, tags...)     CAPTURE_ERRNO(\
    msg_event_suppress_recursions_and_send(msg_event_create(EVT_PRI_INFO, desc, ##tags, NULL )))

/* just like msg_info, but prepends the message with a timestamp -- useful in interactive
 * tools with long running time to provide some feedback */
#define msg_progress(desc, tags...)             \
        do {                    \
          time_t t;                 \
          char *timestamp, *newdesc;              \
                                                                                  \
          t = time(0);                        \
          timestamp = ctime(&t);              \
          timestamp[strlen(timestamp) - 1] = 0;           \
          newdesc = g_strdup_printf("[%s] %s", timestamp, desc);      \
          msg_event_send(msg_event_create(EVT_PRI_INFO, newdesc, ##tags, NULL )); \
          g_free(newdesc);                \
        } while (0)

#define msg_verbose(desc, tags...)                                          \
  do {                                                                      \
    if (G_UNLIKELY(verbose_flag))                                           \
      msg_info(desc, ##tags );                                        \
  } while (0)

#define msg_debug(desc, tags...)              \
  do {                    \
    if (G_UNLIKELY(debug_flag))                                             \
      msg_event_suppress_recursions_and_send(                               \
            msg_event_create(EVT_PRI_DEBUG, desc, ##tags, NULL ));          \
  } while (0)

#define msg_trace(desc, tags...)              \
  do {                    \
    if (G_UNLIKELY(trace_flag))                     \
      msg_event_suppress_recursions_and_send(                               \
            msg_event_create(EVT_PRI_DEBUG, desc, ##tags, NULL ));          \
  } while (0)

#define msg_diagnostics(desc, tags...)              \
  do {                    \
    if (G_UNLIKELY(trace_flag))                     \
      msg_event_print_event_to_stderr(              \
            msg_event_create(EVT_PRI_DEBUG, desc, ##tags, NULL ));          \
  } while (0)

#define __once()          \
        ({            \
          static gboolean __guard = TRUE;   \
          gboolean __current_guard = __guard;   \
          __guard = FALSE;        \
          __current_guard;        \
        })

#define msg_warning_once(desc, tags...) \
        do {        \
          if (__once())     \
            msg_warning(desc, ##tags ); \
        } while (0)

void msg_post_message(LogMessage *msg);
void msg_send_formatted_message(int prio, const char *msg);

#endif
