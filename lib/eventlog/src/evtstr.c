/*
 * Event Logging API
 * Copyright (c) 2003 BalaBit IT Ltd.
 * All rights reserved.
 * Author: Balazs Scheidler
 *
 * $Id: evtstr.c,v 1.3 2004/08/20 19:46:29 bazsi Exp $
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
 * A couple of string support functions which make it easy to output
 * escaped strings. It is used by event formatting functions.
 */
#include "evt_internals.h"

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

/* event string handling */

static inline int
evt_str_grow(EVTSTR *es, size_t new_alloc)
{
  es->es_buf = realloc(es->es_buf, new_alloc);
  if (!es->es_buf)
    return 0;

  es->es_allocated = new_alloc;
  return 1;
}

int
evt_str_append_len(EVTSTR *es, char *str, size_t len)
{
  /* make sure we have room new string + trailing zero */

  if (es->es_allocated < es->es_length + len + 1)
    {
      if (!evt_str_grow(es, es->es_length + len + 1))
        return 0;
    }
  memcpy(es->es_buf + es->es_length, str, len);
  es->es_length += len;
  es->es_buf[es->es_length] = 0; /* trailing zero */
  return 1;
}

int
evt_str_append(EVTSTR *es, char *str)
{
  return evt_str_append_len(es, str, strlen(str));
}

int
evt_str_append_escape_bs(EVTSTR *es,
                         char *unescaped, size_t unescaped_len,
                         char escape_char)
{
  /* a single character is escaped to at most 4 characters: \xXX */
  size_t escaped_char_max_len = 4;

  char escaped_buffer[128];
  size_t escaped_buffer_capacity = sizeof(escaped_buffer) / sizeof(escaped_buffer[0]);
  size_t escaped_buffer_length = 0;

  int i;

  for (i = 0; i < unescaped_len; i++)
    {
      unsigned c = (unsigned) unescaped[i];

      if (c < 32 && c != '\t')
        {
          sprintf(escaped_buffer + escaped_buffer_length, "\\x%02x", (unsigned char) unescaped[i]);
          escaped_buffer_length += 4;
        }
      else if (unescaped[i] == escape_char)
        {
          escaped_buffer[escaped_buffer_length++] = '\\';
          escaped_buffer[escaped_buffer_length++] = escape_char;
        }
      else
        {
          escaped_buffer[escaped_buffer_length++] = unescaped[i];
        }

      if (escaped_buffer_capacity < escaped_buffer_length + escaped_char_max_len)
        {
          if (!evt_str_append_len(es, escaped_buffer, escaped_buffer_length))
            return 0;

          escaped_buffer_length = 0;
        }
    }

  return evt_str_append_len(es, escaped_buffer, escaped_buffer_length);
}

int
evt_str_append_escape_xml_attr(EVTSTR *es,
                               char *unescaped, size_t unescaped_len)
{
  /* a single character is escaped to at most 6 characters: '&#xXX;' or '&quot;' */
  size_t escaped_char_max_len = 6;

  char escaped_buffer[128];
  size_t escaped_buffer_capacity = sizeof(escaped_buffer) / sizeof(escaped_buffer[0]);
  size_t escaped_buffer_length = 0;

  int i;

  for (i = 0; i < unescaped_len; i++)
    {
      if ((unsigned) unescaped[i] < 32)
        {
          sprintf(escaped_buffer + escaped_buffer_length, "&#x%02x;", (unsigned char) unescaped[i]);
          escaped_buffer_length += 6;
        }
      else if (unescaped[i] == '"')
        {
          strcpy(escaped_buffer + escaped_buffer_length, "&quot;");
          escaped_buffer_length += 6;
        }
      else
        {
          escaped_buffer[escaped_buffer_length++] = unescaped[i];
        }

      if (escaped_buffer_capacity < escaped_buffer_length + escaped_char_max_len)
        {
          if (!evt_str_append_len(es, escaped_buffer, escaped_buffer_length))
            return 0;

          escaped_buffer_length = 0;
        }
    }

  return evt_str_append_len(es, escaped_buffer, escaped_buffer_length);
}

int
evt_str_append_escape_xml_pcdata(EVTSTR *es,
                                 char *unescaped, size_t unescaped_len)
{
  /* a single character is escaped to at most 6 characters: '&#xXX;' or '&gt;' or '&lt;' */
  size_t escaped_char_max_len = 6;

  char escaped_buffer[128];
  size_t escaped_buffer_capacity = sizeof(escaped_buffer) / sizeof(escaped_buffer[0]);
  size_t escaped_buffer_length = 0;

  int i;

  for (i = 0; i < unescaped_len; i++)
    {
      if ((unsigned) unescaped[i] < 32)
        {
          sprintf(escaped_buffer + escaped_buffer_length, "&#x%02x;", (unsigned char) unescaped[i]);
          escaped_buffer_length += 6;
        }
      else if (unescaped[i] == '<')
        {
          strcpy(escaped_buffer + escaped_buffer_length, "&lt;");
          escaped_buffer_length += 4;
        }
      else if (unescaped[i] == '>')
        {
          strcpy(escaped_buffer + escaped_buffer_length, "&gt;");
          escaped_buffer_length += 4;
        }
      else
        {
          escaped_buffer[escaped_buffer_length++] = unescaped[i];
        }

      if (escaped_buffer_capacity < escaped_buffer_length + escaped_char_max_len)
        {
          if (!evt_str_append_len(es, escaped_buffer, escaped_buffer_length))
            return 0;

          escaped_buffer_length = 0;
        }
    }

  return evt_str_append_len(es, escaped_buffer, escaped_buffer_length);
}

char *
evt_str_get_str(EVTSTR *es)
{
  return es->es_buf;
}

EVTSTR *
evt_str_init(size_t init_alloc)
{
  EVTSTR *es;
  es = (EVTSTR *) malloc(sizeof(EVTSTR));
  if (es)
    {
      /* make room for init_alloc characters + trailing zero */
      init_alloc++;
      es->es_allocated = init_alloc;
      es->es_length = 0;
      es->es_buf = malloc(init_alloc);
      es->es_buf[0] = 0;
    }
  return es;
}

void
evt_str_free(EVTSTR *es, int free_buf)
{
  if (free_buf)
    free(es->es_buf);
  free(es);
}
