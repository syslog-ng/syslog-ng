/*
 * Event Logging API
 * Copyright (c) 2003 BalaBit IT Ltd.
 * All rights reserved.
 * Author: Balazs Scheidler
 *
 * EventLog library internal functions/typedefs.
 *
 * $Id: evt_internals.h,v 1.4 2004/08/20 19:46:28 bazsi Exp $
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
 * THIS SOFTWARE IS PROVIDED BY BALABIT AND CONTRIBUTORS S IS'' AND
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

#ifndef __EVT_INTERNALS_H_INCLUDED
#define __EVT_INTERNALS_H_INCLUDED

#include "evtlog.h"

#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef _MSC_VER
#include <windows.h>
#endif

#include <sys/types.h>

/* whether to add the given default tag */
#define EF_ADD_PID  0x0001
#define EF_ADD_PROG 0x0002
#define EF_ADD_ISOSTAMP 0x0004
#define EF_ADD_UTCSTAMP 0x0008
#define EF_ADD_TIMEZONE 0x0010
#define EF_ADD_MSGID  0x0020
#define EF_ADD_ALL  0x003F

#define EF_INITIALIZED  0x8000

typedef struct __evttaghook EVTTAGHOOK;
typedef struct __evtsyslogopts EVTSYSLOGOPTS;
typedef struct __evtstr EVTSTR;

struct __evtsyslogopts
{
  void (*es_openlog)(const char *ident, int option, int facility);
  void (*es_closelog)(void);
  void (*es_syslog)(int priority, const char *format, ...);
  int es_options;
};

struct __evtcontext
{
  int ec_ref;
  char ec_formatter[32];
  char *(*ec_formatter_fn)(EVTREC *e);
  char ec_outmethod[32];
  int (*ec_outmethod_fn)(EVTREC *e);
  char *ec_prog;
  int ec_syslog_fac;
  EVTTAGHOOK *ec_tag_hooks;
  unsigned long ec_flags;
};

struct __evttaghook
{
  struct __evttaghook *et_next;
  int (*et_callback)(EVTREC *e, void *user_ptr);
  void *et_userptr;
};

struct __evtrec
{
  int ev_ref;
  int ev_syslog_pri;
  char *ev_desc;
  EVTTAG *ev_pairs;
  EVTTAG *ev_last_pair;
  EVTCONTEXT *ev_ctx;
};

struct __evttag
{
  EVTTAG *et_next;
  char *et_tag;
  char *et_value;
};

struct __evtstr
{
  size_t es_allocated; /* number of allocated characters in es_buf */
  size_t es_length;    /* length of string without trailing NUL */
  char *es_buf;
};

/* internal functions */

/* event context */
EVTCONTEXT *evt_ctx_ref(EVTCONTEXT *ctx);

/* event records */
EVTREC *evt_rec_ref(EVTREC *e);

/* event tag */
void evt_tag_free(EVTTAG *et);

/* event strings */
EVTSTR *evt_str_init(size_t init_alloc);
void evt_str_free(EVTSTR *es, int free_buf);
int evt_str_append(EVTSTR *es, char *str);
int evt_str_append_len(EVTSTR *es, char *str, size_t len);
int evt_str_append_escape_bs(EVTSTR *es, char *unescaped, size_t unescaped_len, char escape_char);
char *evt_str_get_str(EVTSTR *es);

/* syslog linked wrapper */
extern EVTSYSLOGOPTS syslog_opts;
void evt_syslog_wrapper_init(void);

#endif
