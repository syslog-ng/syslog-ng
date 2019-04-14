/*
 * Event Logging API
 * Copyright (c) 2003 BalaBit IT Ltd.
 * All rights reserved.
 * Author: Balazs Scheidler
 *
 * $Id: evtsyslog.c,v 1.3 2004/08/20 20:27:54 bazsi Exp $
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

#include "evt_internals.h"

#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>

EVTCONTEXT *syslog_context;
EVTSYSLOGOPTS syslog_opts;

void
evt_openlog(const char *ident, int options, int facility)
{
  syslog_context = evt_ctx_init(ident, facility);

  /* NOTE: we save the legacy syslog option value, so that our local target
   * can use it */

  syslog_opts.es_options = options;
}

void
evt_closelog(void)
{
}

void
evt_vsyslog(int pri, const char *format, va_list ap)
{
  EVTREC *e;
  char msgbuf[1024];

  vsnprintf(msgbuf, sizeof(msgbuf), format, ap);
  e = evt_rec_init(syslog_context, pri, msgbuf);
  evt_log(e);
}

void
evt_syslog(int pri, const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  evt_vsyslog(pri, format, ap);
  va_end(ap);
}

#if defined (ENABLE_DLSYM_WRAPPER)

#include <dlfcn.h>

void
openlog(const char *ident, int option, int facility)
{
  evt_openlog(ident, option, facility);
}

void
syslog(int pri, const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  evt_vsyslog(pri, format, ap);
  va_end(ap);
}

void
closelog(void)
{
  evt_closelog();
}

void
evt_syslog_wrapper_init(void)
{
  static int initialized = 0;

  if (!initialized)
    {
      syslog_opts.es_openlog = dlsym(RTLD_NEXT, "openlog");
      syslog_opts.es_closelog = dlsym(RTLD_NEXT, "closelog");
      syslog_opts.es_syslog = dlsym(RTLD_NEXT, "syslog");
      syslog_opts.es_options = LOG_PID | LOG_NOWAIT;
      initialized = 1;
    }
}

#else

void
evt_syslog_wrapper_init(void)
{
  static int initialized = 0;

  if (!initialized)
    {
      syslog_opts.es_openlog = openlog;
      syslog_opts.es_closelog = closelog;
      syslog_opts.es_syslog = syslog;
      syslog_opts.es_options = LOG_PID | LOG_NOWAIT;
      initialized = 1;
    }
}


#endif
