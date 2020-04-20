/*
 * Event Logging API
 * Copyright (c) 2003 BalaBit IT Ltd.
 * All rights reserved.
 * Author: Balazs Scheidler
 *
 * $Id: evtrec.c,v 1.4 2004/08/20 19:46:29 bazsi Exp $
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
 * THIS SOFTWARE IS PROVIDED BY BALABIT AND CONTRIBUTORS ``AS IS'' AND
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h> /* for snprintf */

void
evt_rec_add_tag(EVTREC *e, EVTTAG *tag)
{
  /* make it the last in list */
  tag->et_next = NULL;

  if (e->ev_last_pair)
    e->ev_last_pair->et_next = tag;
  else
    e->ev_pairs = tag;

  e->ev_last_pair = tag;
}

void
evt_rec_add_tagsv(EVTREC *e, va_list tags)
{
  EVTTAG *t;

  t = va_arg(tags, EVTTAG *);
  while (t)
    {
      evt_rec_add_tag(e, t);
      t = va_arg(tags, EVTTAG *);
    }
}

void
evt_rec_add_tags(EVTREC *e, EVTTAG *first, ...)
{
  va_list ap;

  va_start(ap, first);
  evt_rec_add_tag(e, first);
  evt_rec_add_tagsv(e, ap);
  va_end(ap);
}

int
evt_rec_get_syslog_pri(EVTREC *e)
{
  return e->ev_syslog_pri;
}

static int
evt_rec_call_hooks(EVTREC *e)
{
  EVTTAGHOOK *et;
  int res = 1;

  for (et = e->ev_ctx->ec_tag_hooks; et; et = et->et_next)
    {
      if (!et->et_callback(e, et->et_userptr))
        res = 0;
    }
  return res;
}

EVTREC *
evt_rec_init(EVTCONTEXT *ctx, int syslog_pri, const char *desc)
{
  EVTREC *e;

  e = (EVTREC *) malloc(sizeof(EVTREC));
  if (e)
    {
      e->ev_ctx = evt_ctx_ref(ctx);
      e->ev_desc = strdup(desc);
      e->ev_pairs = NULL;
      e->ev_last_pair = NULL;
      e->ev_ref = 1;
      e->ev_syslog_pri = syslog_pri;
      if (!evt_rec_call_hooks(e))
        {
          free(e);
          e = NULL;
        }
    }
  return e;
}

EVTREC *
evt_rec_ref(EVTREC *e)
{
  e->ev_ref++;
  return e;
}

void
evt_rec_free(EVTREC *e)
{
  EVTTAG *p, *p_next;

  if (--e->ev_ref == 0)
    {
      free(e->ev_desc);
      for (p = e->ev_pairs; p; p = p_next)
        {
          p_next = p->et_next;
          evt_tag_free(p);
        }
      evt_ctx_free(e->ev_ctx);
      free(e);
    }
}
