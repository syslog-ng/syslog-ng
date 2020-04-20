/*
 * Event Logging API
 * Copyright (c) 2003 BalaBit IT Ltd.
 * All rights reserved.
 * Author: Balazs Scheidler
 *
 * $Id: evtctx.c,v 1.3 2004/08/20 19:46:28 bazsi Exp $
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
 * This is the main module which is responsible for managing the
 * configuration and to perform initialization.
 */

#include "evt_internals.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#ifdef _MSC_VER
#define getpid GetCurrentProcessId
#endif

static int
evtrec_add_standard_tags(EVTREC *e, void *userptr)
{
  time_t now;
  struct tm *tm = NULL;
  char buf[128];
  EVTCONTEXT *ctx = e->ev_ctx;

  time(&now);

  if (ctx->ec_flags & EF_ADD_PID)
    evt_rec_add_tag(e, evt_tag_int(EVT_TAG_PID, (int) getpid()));
  if (ctx->ec_flags & EF_ADD_PROG)
    evt_rec_add_tag(e, evt_tag_str(EVT_TAG_PROG, ctx->ec_prog));
  if (ctx->ec_flags & EF_ADD_ISOSTAMP)
    {
      tm = localtime(&now);
      strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S%z", tm);
      evt_rec_add_tag(e, evt_tag_str(EVT_TAG_ISOSTAMP, buf));
    }
  if (ctx->ec_flags & EF_ADD_UTCSTAMP)
    evt_rec_add_tag(e, evt_tag_int(EVT_TAG_UTCSTAMP, (int) now));
  if (ctx->ec_flags & EF_ADD_TIMEZONE)
    {
      if (!tm)
        tm = localtime(&now);
      strftime(buf, sizeof(buf), "%z", tm);
      evt_rec_add_tag(e, evt_tag_str(EVT_TAG_TIMEZONE, buf));
    }
  if (ctx->ec_flags & EF_ADD_MSGID)
    {
      evt_rec_add_tag(e, evt_tag_int(EVT_TAG_MSGID, 123456));
    }
  return 1;
}

int
evt_ctx_tag_hook_add(EVTCONTEXT *ctx, int (*func)(EVTREC *e, void *user_ptr), void *user_ptr)
{
  EVTTAGHOOK *cb = malloc(sizeof(EVTTAGHOOK));

  if (!cb)
    return 0;
  cb->et_callback = func;
  cb->et_userptr = user_ptr;
  cb->et_next = ctx->ec_tag_hooks;
  ctx->ec_tag_hooks = cb;
  return 1;
}

EVTCONTEXT *
evt_ctx_init(const char *prog, int syslog_fac)
{
  EVTCONTEXT *ctx;

  ctx = (EVTCONTEXT *) calloc(sizeof(*ctx), 1);
  if (ctx)
    {
      strcpy(ctx->ec_formatter, "plain");
      strcpy(ctx->ec_outmethod, "local");
      ctx->ec_ref = 1;
      ctx->ec_flags = EF_INITIALIZED;
      ctx->ec_prog = (char *) prog;
      ctx->ec_syslog_fac = syslog_fac;
      evt_ctx_tag_hook_add(ctx, evtrec_add_standard_tags, NULL);
#ifndef _MSC_VER
      evt_syslog_wrapper_init();
#endif
    }

  return ctx;
}

EVTCONTEXT *
evt_ctx_ref(EVTCONTEXT *ctx)
{
  assert(ctx->ec_ref > 0);
  ctx->ec_ref++;
  return ctx;
}

void
evt_ctx_free(EVTCONTEXT *ctx)
{
  assert(ctx->ec_ref > 0);
  if (--ctx->ec_ref == 0)
    {
      EVTTAGHOOK *p, *p_next;

      p = ctx->ec_tag_hooks;
      while (p)
        {
          p_next = p->et_next;
          free(p);
          p = p_next;
        }
      free(ctx);
    }
}
