/*
 * Event Logging API
 * Copyright (c) 2003 BalaBit IT Ltd.
 * All rights reserved.
 * Author: Balazs Scheidler
 *
 * $Id: evtfmt.c,v 1.3 2004/08/20 19:46:28 bazsi Exp $
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
 * Formatting implementations. This module is responsible for formatting
 * EVENT records. Each formatter has a unique ID and the administrator is
 * free to select which output format to use.
 */

#include "evt_internals.h"

#include <stdlib.h>
#include <string.h>

static char *
evtrec_format_plain(EVTREC *e)
{
  EVTSTR *es;
  EVTTAG *et;
  char *res;

  if (!(es = evt_str_init(128)))
    return NULL;

  evt_str_append_escape_bs(es, e->ev_desc, strlen(e->ev_desc), ';');
  evt_str_append(es, ";");
  if (e->ev_pairs)
    evt_str_append(es, " ");
  for (et = e->ev_pairs; et; et = et->et_next)
    {
      evt_str_append(es, et->et_tag);
      evt_str_append(es, "='");
      evt_str_append_escape_bs(es, et->et_value, strlen(et->et_value), '\'');
      if (et->et_next)
        evt_str_append(es, "', ");
      else
        evt_str_append(es, "'");
    }
  res = evt_str_get_str(es);
  evt_str_free(es, 0);
  return res;
}

static struct
{
  char *ef_name;
  char *(*ef_formatter)(EVTREC *e);
} evt_formatters[] =
{
  { "plain", evtrec_format_plain },
  { NULL, NULL }
};

char *
evt_format(EVTREC *e)
{
  EVTCONTEXT *ctx = e->ev_ctx;

  if (!ctx->ec_formatter_fn)
    {
      int i;
      for (i = 0; evt_formatters[i].ef_name; i++)
        {
          if (strcmp(evt_formatters[i].ef_name, ctx->ec_formatter) == 0)
            {
              ctx->ec_formatter_fn = evt_formatters[i].ef_formatter;
              break;
            }
        }
      if (evt_formatters[i].ef_name == NULL)
        ctx->ec_formatter_fn = evtrec_format_plain;
    }
  return (*ctx->ec_formatter_fn)(e);
}
