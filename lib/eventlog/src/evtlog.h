/*
 * Event Logging API
 * Copyright (c) 2003 BalaBit IT Ltd.
 * All rights reserved.
 * Author: Balazs Scheidler
 *
 * EventLog library public functions.
 *
 * $Id: evtlog.h,v 1.5 2004/08/20 19:53:52 bazsi Exp $
 *
 * Some of the ideas are based on the discussions on the log-analysis
 * mailing list (http://www.loganalysis.org/).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of BalaBit nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BALABIT AND CONTRIBUTORS `AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef __EVTLOG_H_INCLUDED
#define __EVTLOG_H_INCLUDED

#ifndef _MSC_VER
# include <syslog.h>
#endif
#include <stdarg.h>

#include "evtmaps.h"

#define EVT_PRI_EMERG       0       /* system is unusable */
#define EVT_PRI_ALERT       1       /* action must be taken immediately */
#define EVT_PRI_CRIT        2       /* critical conditions */
#define EVT_PRI_ERR         3       /* error conditions */
#define EVT_PRI_WARNING     4       /* warning conditions */
#define EVT_PRI_NOTICE      5       /* normal but significant condition */
#define EVT_PRI_INFO        6       /* informational */
#define EVT_PRI_DEBUG       7       /* debug-level messages */

#define EVT_FAC_KERN        (0<<3)  /* kernel messages */
#define EVT_FAC_USER        (1<<3)  /* random user-level messages */
#define EVT_FAC_MAIL        (2<<3)  /* mail system */
#define EVT_FAC_DAEMON      (3<<3)  /* system daemons */
#define EVT_FAC_AUTH        (4<<3)  /* security/authorization messages */
#define EVT_FAC_SYSLOG      (5<<3)  /* messages generated internally by syslogd */
#define EVT_FAC_LPR         (6<<3)  /* line printer subsystem */
#define EVT_FAC_NEWS        (7<<3)  /* network news subsystem */
#define EVT_FAC_UUCP        (8<<3)  /* UUCP subsystem */
#define EVT_FAC_CRON        (9<<3)  /* clock daemon */
#define EVT_FAC_AUTHPRIV    (10<<3) /* security/authorization messages (private) */
#define EVT_FAC_FTP         (11<<3) /* ftp daemon */

/* other codes through 15 reserved for system use */
#define EVT_FAC_LOCAL0      (16<<3) /* reserved for local use */
#define EVT_FAC_LOCAL1      (17<<3) /* reserved for local use */
#define EVT_FAC_LOCAL2      (18<<3) /* reserved for local use */
#define EVT_FAC_LOCAL3      (19<<3) /* reserved for local use */
#define EVT_FAC_LOCAL4      (20<<3) /* reserved for local use */
#define EVT_FAC_LOCAL5      (21<<3) /* reserved for local use */
#define EVT_FAC_LOCAL6      (22<<3) /* reserved for local use */
#define EVT_FAC_LOCAL7      (23<<3) /* reserved for local use */

#ifdef __GNUC__
#define EVT_GNUC_PRINTF_FUNC(format_idx, first_arg_idx)  __attribute__((format(printf, format_idx, first_arg_idx)))
#else
#define EVT_GNUC_PRINTF_FUNC(format_idx, first_arg_idx)
#endif

/* EVTCONTEXT encapsulates logging specific parameters like the
 * program name and facility to use */
typedef struct __evtcontext EVTCONTEXT;

/* EVTREC is an event log record, contains a description and one or more
 * name/value pairs */
typedef struct __evtrec EVTREC;

/* EVTTAG is a name value pair, comprising an event record */
typedef struct __evttag EVTTAG;

/* eventlog contexts */

/**
 * evt_ctx_init:
 * @prog: program name to use to identify this process as
 * @syslog_fac: syslog facility code, like EVT_FAC_AUTH
 *
 * This function creates a new eventlog context.
 *
 * Returns: the new context, or NULL on failure
 **/
EVTCONTEXT *evt_ctx_init(const char *prog, int syslog_fac);

/**
 * evt_ctx_free:
 * @ctx: context to free
 *
 * This function frees an eventlog context.
 **/
void evt_ctx_free(EVTCONTEXT *ctx);

/**
 * evt_ctx_tag_hook_add:
 **/
int evt_ctx_tag_hook_add(EVTCONTEXT *ctx, int (*func)(EVTREC *e, void *user_ptr), void *user_ptr);

/* event record manipulation */
EVTREC *evt_rec_init(EVTCONTEXT *ctx, int syslog_pri, const char *desc);
void evt_rec_add_tag(EVTREC *e, EVTTAG *tag);
void evt_rec_add_tagsv(EVTREC *e, va_list tags);
void evt_rec_add_tags(EVTREC *e, EVTTAG *first, ...);
int evt_rec_get_syslog_pri(EVTREC *e);
void evt_rec_free(EVTREC *e);


/**
 * evt_rec_tag_*:
 * @tag: specifies tag name as string
 * @value: specifies a value in the given type
 *
 * Adds the specified tag/value pair to EVTREC.
 *
 * Return value: 0 to indicate failure and 1 for success
 **/
EVTTAG *evt_tag_str(const char *tag, const char *value);
EVTTAG *evt_tag_int(const char *tag, int value);
EVTTAG *evt_tag_long(const char *tag, long value);
EVTTAG *evt_tag_errno(const char *tag, int err);
EVTTAG *evt_tag_printf(const char *tag, const char *format, ...) EVT_GNUC_PRINTF_FUNC(2, 3);

/**
 * evt_format:
 * @e: event record
 *
 * Formats the given event as specified by the current configuration.
 *
 * Return value: returns a newly allocated string. The caller is responsible
 * for freeing the returned value.
 **/
char *evt_format(EVTREC *e);

/**
 * evt_log:
 * @e: event record
 *
 * Formats and sends the given event as specified by the current
 * configuration. This function blocks and will not return until the message
 * is sent. The function consumes its argument, that is the caller does not
 * need to free the event record after passing it to evt_log().
 *
 * Return value: 0 to indicate failure and 1 for success
 *
 **/
int evt_log(EVTREC *e);

/* syslog wrapper */
void evt_openlog(const char *ident, int option, int facility);
void evt_closelog(void);
void evt_vsyslog(int pri, const char *format, va_list ap);
void evt_syslog(int pri, const char *format, ...) EVT_GNUC_PRINTF_FUNC(2, 3);

#ifdef EVENTLOG_SYSLOG_MACROS

#define openlog evt_openlog
#define syslog evt_syslog
#define vsyslog evt_vsyslog
#define closelog evt_closelog

#endif

#endif
