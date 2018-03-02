/*
 * Event Logging API
 * Copyright (c) 2003 BalaBit IT Ltd.
 * All rights reserved.
 * Author: Balazs Scheidler
 *
 * $Id: evttags.c,v 1.4 2004/08/20 19:46:29 bazsi Exp $
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

/*
 * This module implements tag support functions.
 */

#include "evt_internals.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#ifdef _MSC_VER
#ifndef snprintf
#define snprintf _snprintf
#endif
#endif

void
evt_tag_free(EVTTAG *et)
{
  free(et->et_tag);
  free(et->et_value);
  free(et);
}

EVTTAG *
evt_tag_str(const char *tag, const char *value)
{
  EVTTAG *p;

  /* neither tag nor value can be NULL */
  assert(tag);
  if (!value)
    value = "(null)";

  p = (EVTTAG *) malloc(sizeof(EVTTAG));
  if (p)
    {
      p->et_tag = strdup(tag);
      p->et_value = strdup(value);
    }
  return p;
}

EVTTAG *
evt_tag_int(const char *tag, int value)
{
  char buf[32]; /* a 64 bit int fits into 20 characters */

  snprintf(buf, sizeof(buf), "%d", value);
  return evt_tag_str(tag, buf);
}

EVTTAG *
evt_tag_long(const char *tag, long value)
{
  char buf[32]; /* a 64 bit int fits into 20 characters */

  snprintf(buf, sizeof(buf), "%ld", value);
  return evt_tag_str(tag, buf);
}

EVTTAG *
evt_tag_errno(const char *tag, int err)
{
  char buf[128];

  snprintf(buf, sizeof(buf), "%s (%d)", strerror(err), err);
  return evt_tag_str(tag, buf);
}

EVTTAG *
evt_tag_printf(const char *tag, const char *format, ...)
{
  va_list ap;
  char buf[1024];

  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);
  return evt_tag_str(tag, buf);
}

EVTTAG *
evt_tag_inaddr(const char *tag, const struct in_addr *addr)
{
  char buf[64];

  if (addr)
    inet_ntop(AF_INET, addr, buf, sizeof(buf));
  else
    strncpy(buf, "none", sizeof(buf));

  return evt_tag_str(tag, buf);
}

EVTTAG *
evt_tag_inaddr6(const char *tag, const struct in6_addr *addr)
{
  char buf[128];

  if (addr)
    inet_ntop(AF_INET6, addr, buf, sizeof(buf));
  else
    strncpy(buf, "none", sizeof(buf));
  return evt_tag_str(tag, buf);
}
