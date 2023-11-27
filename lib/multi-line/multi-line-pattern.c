/*
 * Copyright (c) 2013 Balabit
 * Copyright (c) 2013 Balazs Scheidler <bazsi@balabit.hu>
 * Copyright (c) 2022 Balazs Scheidler <bazsi77@gmail.com>
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
 */
#include "multi-line/multi-line-pattern.h"
#include "messages.h"

MultiLinePattern *
multi_line_pattern_compile(const gchar *regexp, GError **error)
{
  MultiLinePattern *self = g_new0(MultiLinePattern, 1);
  gint rc;
  PCRE2_SIZE erroffset;

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
  self->ref_cnt = 1;

  /* compile the regexp */
  self->pattern = pcre2_compile((PCRE2_SPTR) regexp, PCRE2_ZERO_TERMINATED, 0, &rc, &erroffset, NULL);
  if (!self->pattern)
    {
      PCRE2_UCHAR error_message[128];

      pcre2_get_error_message(rc, error_message, sizeof(error_message));
      g_set_error(error, 0, 0,
                  "Error while compiling multi-line regexp as a PCRE expression, error=%s, error_at=%" G_GSIZE_FORMAT,
                  (gchar *) error_message, erroffset);
      goto error;
    }

  /* optimize regexp */
  rc = pcre2_jit_compile(self->pattern, PCRE2_JIT_COMPLETE);
  if (rc < 0)
    {
      PCRE2_UCHAR error_message[128];

      pcre2_get_error_message(rc, error_message, sizeof(error_message));
      msg_debug("multi-line-pattern: Error while JIT compiling regular expression, continuing without JIT",
                evt_tag_str("regexp", regexp),
                evt_tag_str("error", (gchar *) error_message));
    }

  return self;
error:
  if (self->pattern)
    pcre2_code_free(self->pattern);
  g_free(self);
  return NULL;
}

gint
multi_line_pattern_eval(MultiLinePattern *re, const guchar *str, gsize len, pcre2_match_data *match_data)
{
  return pcre2_match(re->pattern, (PCRE2_SPTR) str, (PCRE2_SIZE) len, 0, 0, match_data, NULL);
}

gboolean
multi_line_pattern_find(MultiLinePattern *re, const guchar *str, gsize len, gint *start, gint *end)
{
  if (!re)
    return FALSE;

  gboolean result = FALSE;
  pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(re->pattern, NULL);


  if (multi_line_pattern_eval(re, str, len, match_data) < 0)
    goto exit;

  guint32 num_matches = pcre2_get_ovector_count(match_data);
  PCRE2_SIZE *matches = pcre2_get_ovector_pointer(match_data);

  if (num_matches == 0)
    goto exit;

  *start = matches[0];
  *end = matches[1];
  result = TRUE;
exit:
  pcre2_match_data_free(match_data);
  return result;
}

gboolean
multi_line_pattern_match(MultiLinePattern *re, const guchar *str, gsize len)
{
  if (!re)
    return FALSE;

  gboolean result = FALSE;
  pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(re->pattern, NULL);

  if (multi_line_pattern_eval(re, str, len, match_data) < 0)
    goto exit;

  guint32 num_matches = pcre2_get_ovector_count(match_data);
  PCRE2_SIZE *matches = pcre2_get_ovector_pointer(match_data);

  result = num_matches > 0 && matches[0] >= 0;

exit:
  pcre2_match_data_free(match_data);
  return result;
}

MultiLinePattern *
multi_line_pattern_ref(MultiLinePattern *self)
{
  if (self)
    self->ref_cnt++;
  return self;
}

void
multi_line_pattern_unref(MultiLinePattern *self)
{
  if (self && (--self->ref_cnt == 0))
    {
      if (self->pattern)
        pcre2_code_free(self->pattern);
      g_free(self);
    }
}
