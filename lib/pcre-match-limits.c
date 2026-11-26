/*
 * Copyright (c) 2026 Akos Zalavary <akos.zalavary@logiqum.com>
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
 *
 */

#include "pcre-match-limits.h"
#include "messages.h"

/* monotonic second of the last logged limit hit, 0 = never */
static gint last_log_sec;
/* limit hits not logged since the last logged one */
static gint suppressed_count;

pcre2_match_context *
pcre_match_limits_get_context(void)
{
  static gsize context_initialized;
  static pcre2_match_context *context;

  if (g_once_init_enter(&context_initialized))
    {
      context = pcre2_match_context_create(NULL);
      g_assert(context);
      pcre2_set_match_limit(context, PCRE_MATCH_LIMIT);
      pcre2_set_depth_limit(context, PCRE_DEPTH_LIMIT);
      g_once_init_leave(&context_initialized, 1);
    }
  return context;
}

gboolean
pcre_match_limits_is_limit_error(gint rc)
{
  return rc == PCRE2_ERROR_MATCHLIMIT || rc == PCRE2_ERROR_DEPTHLIMIT;
}

void
pcre_match_limits_reset_log_rate_limit(void)
{
  g_atomic_int_set(&last_log_sec, 0);
  g_atomic_int_set(&suppressed_count, 0);
}

void
pcre_match_limits_age_log_rate_limit(gint seconds)
{
  g_atomic_int_add(&last_log_sec, -seconds);
}

static void
_report_limit_hit(gint rc, const gchar *pattern, gsize subject_len)
{
  gint now = MAX((gint)(g_get_monotonic_time() / G_USEC_PER_SEC), 1);
  gint last = g_atomic_int_get(&last_log_sec);

  if ((last != 0 && now - last < PCRE_MATCH_LIMIT_LOG_INTERVAL_SEC) ||
      !g_atomic_int_compare_and_exchange(&last_log_sec, last, now))
    {
      g_atomic_int_inc(&suppressed_count);
      return;
    }

  msg_warning("Regular expression exceeded the match limit, treating the message as not matching. "
              "Consider making the pattern less ambiguous (nested or overlapping quantifiers)",
              evt_tag_str("regexp", pattern ? pattern : "<unknown>"),
              evt_tag_str("limit", rc == PCRE2_ERROR_DEPTHLIMIT ? "depth" : "match"),
              evt_tag_long("limit_value", rc == PCRE2_ERROR_DEPTHLIMIT ? PCRE_DEPTH_LIMIT : PCRE_MATCH_LIMIT),
              evt_tag_long("subject_length", subject_len),
              evt_tag_int("suppressed_since_last_report", g_atomic_int_get(&suppressed_count)));
  g_atomic_int_set(&suppressed_count, 0);
}

gint
pcre_match_limited(const pcre2_code *code, const gchar *subject, gsize subject_len, gsize start_offset,
                   guint32 options, pcre2_match_data *match_data, const gchar *pattern)
{
  gint rc = pcre2_match(code, (PCRE2_SPTR) subject, (PCRE2_SIZE) subject_len, (PCRE2_SIZE) start_offset,
                        options, match_data, pcre_match_limits_get_context());

  if (pcre_match_limits_is_limit_error(rc))
    _report_limit_hit(rc, pattern, subject_len);
  return rc;
}
