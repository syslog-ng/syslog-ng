/*
 * Event Logging API
 * Copyright (c) 2003 BalaBit IT Ltd.
 * All rights reserved.
 * Author: Balazs Scheidler
 *
 * $Id: evtout.c,v 1.4 2004/08/20 19:46:29 bazsi Exp $
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
 * Output implementations. An output method is responsible for delivering a
 * message. Each output method has a unique ID and the administrator is
 * free to select which method to use.
 */
#include "evt_internals.h"

#include <stdlib.h>
#include <syslog.h>
#include <string.h>

/* local method implementation */
static int
evt_output_local(EVTREC *e)
{
  static int initialized = 0;
  char *msg;
  EVTCONTEXT *ctx = e->ev_ctx;

  if (!initialized)
    {
      /*
       * there's a small window of race here, if this is used in a
       * multithreaded program, but it's a small race, and can only occur
       * when the first message is sent
       */
      initialized = 1;
      syslog_opts.es_openlog(ctx->ec_prog, syslog_opts.es_options, ctx->ec_syslog_fac);
    }
  msg = evt_format(e);
  syslog_opts.es_syslog(e->ev_syslog_pri, "%s", msg);
  free(msg);
  return 1;
}

static struct
{
  char *eo_name;
  int (*eo_outmethod_fn)(EVTREC *e);
} evt_outmethods[] =
{
  { "local", evt_output_local },
  { NULL, NULL }
};

int
evt_log(EVTREC *e)
{
  int res;
  EVTCONTEXT *ctx = e->ev_ctx;

  if (!ctx->ec_outmethod_fn)
    {
      int i;
      for (i = 0; evt_outmethods[i].eo_name; i++)
        {
          if (strcmp(evt_outmethods[i].eo_name, ctx->ec_outmethod) == 0)
            {
              ctx->ec_outmethod_fn = evt_outmethods[i].eo_outmethod_fn;
              break;
            }
        }
      if (evt_outmethods[i].eo_name == NULL)
        ctx->ec_outmethod_fn = evt_output_local;
    }
  res = (*ctx->ec_outmethod_fn)(e);
  evt_rec_free(e);
  return res;
}
