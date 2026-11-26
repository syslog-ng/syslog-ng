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

#include <criterion/criterion.h>
#include "libtest/grab-logging.h"

#include "pcre-match-limits.h"
#include "multi-line/multi-line-pattern.h"
#include "apphook.h"

#include <string.h>

static void
setup(void)
{
  app_startup();
  start_grabbing_messages();
  pcre_match_limits_reset_log_rate_limit();
}

static void
teardown(void)
{
  stop_grabbing_messages();
  reset_grabbed_messages();
  app_shutdown();
}

TestSuite(pcre_match_limits, .init = setup, .fini = teardown);

static pcre2_code *
_compile(const gchar *pattern, gboolean jit)
{
  gint rc;
  PCRE2_SIZE erroffset;
  pcre2_code *code = pcre2_compile((PCRE2_SPTR) pattern, PCRE2_ZERO_TERMINATED, 0, &rc, &erroffset, NULL);

  cr_assert_not_null(code);
  if (jit)
    pcre2_jit_compile(code, PCRE2_JIT_COMPLETE);
  return code;
}

/* n times 'a' followed by a 'b' */
static gchar *
_subject(gsize n)
{
  gchar *s = g_malloc(n + 2);

  memset(s, 'a', n);
  s[n] = 'b';
  s[n + 1] = 0;
  return s;
}

static gsize
_count_grabbed(const gchar *needle)
{
  GString *all = g_string_new("");
  gsize count = 0;

  format_grabbed_messages(all);
  for (const gchar *p = strstr(all->str, needle); p; p = strstr(p + 1, needle))
    count++;
  g_string_free(all, TRUE);
  return count;
}

static gboolean
_grabbed_log_contains(const gchar *needle)
{
  return _count_grabbed(needle) > 0;
}

Test(pcre_match_limits, catastrophic_backtracking_hits_the_match_limit_with_jit)
{
  pcre2_code *code = _compile("(a+)+$", TRUE);
  pcre2_match_data *md = pcre2_match_data_create_from_pattern(code, NULL);
  gchar *subject = _subject(40);

  cr_assert_eq(pcre_match_limited(code, subject, strlen(subject), 0, 0, md, "(a+)+$"), PCRE2_ERROR_MATCHLIMIT);

  g_free(subject);
  pcre2_match_data_free(md);
  pcre2_code_free(code);
}

Test(pcre_match_limits, catastrophic_backtracking_hits_the_match_limit_without_jit)
{
  pcre2_code *code = _compile("(a+)+$", FALSE);
  pcre2_match_data *md = pcre2_match_data_create_from_pattern(code, NULL);
  gchar *subject = _subject(40);

  cr_assert_eq(pcre_match_limited(code, subject, strlen(subject), 0, 0, md, "(a+)+$"), PCRE2_ERROR_MATCHLIMIT);

  g_free(subject);
  pcre2_match_data_free(md);
  pcre2_code_free(code);
}

/*
 * "(a+)+$" on 20 'a' and a 'b' needs ~2.6 million interpreter steps to
 * conclude "no match": below the PCRE2 default limit of 10 million, above ours.
 * It finishes with a context that allows 20 million, so the failure is caused
 * by our limit and not by the pattern.
 */
Test(pcre_match_limits, the_limit_comes_from_our_context)
{
  pcre2_code *code = _compile("(a+)+$", FALSE);
  pcre2_match_data *md = pcre2_match_data_create_from_pattern(code, NULL);
  gchar *subject = _subject(20);

  pcre2_match_context *generous = pcre2_match_context_create(NULL);
  pcre2_set_match_limit(generous, 20000000);
  cr_assert_eq(pcre2_match(code, (PCRE2_SPTR) subject, strlen(subject), 0, 0, md, generous), PCRE2_ERROR_NOMATCH);
  pcre2_match_context_free(generous);

  cr_assert_eq(pcre_match_limited(code, subject, strlen(subject), 0, 0, md, "(a+)+$"), PCRE2_ERROR_MATCHLIMIT);

  g_free(subject);
  pcre2_match_data_free(md);
  pcre2_code_free(code);
}

Test(pcre_match_limits, ordinary_patterns_match_as_before_and_do_not_log)
{
  pcre2_code *code = _compile("(\\d+)\\.(\\d+) (?<word>\\w+)", TRUE);
  pcre2_match_data *md = pcre2_match_data_create_from_pattern(code, NULL);
  const gchar *subject = "host 10.20 hello";

  cr_assert_eq(pcre_match_limited(code, subject, strlen(subject), 0, 0, md, "p"), 4);
  cr_assert_eq(pcre_match_limited(code, "nothing", 7, 0, 0, md, "p"), PCRE2_ERROR_NOMATCH);
  /* start offset and options are passed through */
  cr_assert_eq(pcre_match_limited(code, subject, strlen(subject), 7, PCRE2_NOTEMPTY, md, "p"), PCRE2_ERROR_NOMATCH);

  cr_assert_eq(_count_grabbed("match limit"), 0);

  pcre2_match_data_free(md);
  pcre2_code_free(code);
}

Test(pcre_match_limits, limit_hit_is_logged_with_the_pattern_but_never_with_the_subject)
{
  pcre2_code *code = _compile("(a+)+$", TRUE);
  pcre2_match_data *md = pcre2_match_data_create_from_pattern(code, NULL);
  gchar *subject = _subject(40);

  pcre_match_limited(code, subject, strlen(subject), 0, 0, md, "(a+)+$");

  cr_assert(_grabbed_log_contains("exceeded the match limit"));
  cr_assert(_grabbed_log_contains("regexp='(a+)+$'"));
  cr_assert(_grabbed_log_contains("subject_length='41'"));
  cr_assert_not(_grabbed_log_contains("aaaaaaaaaa"));

  g_free(subject);
  pcre2_match_data_free(md);
  pcre2_code_free(code);
}

Test(pcre_match_limits, limit_hits_are_logged_at_most_once_per_interval_and_the_rest_counted)
{
  pcre2_code *code = _compile("(a+)+$", TRUE);
  pcre2_match_data *md = pcre2_match_data_create_from_pattern(code, NULL);
  gchar *subject = _subject(40);

  for (gint i = 0; i < 5; i++)
    cr_assert_eq(pcre_match_limited(code, subject, strlen(subject), 0, 0, md, "(a+)+$"), PCRE2_ERROR_MATCHLIMIT);

  cr_assert_eq(_count_grabbed("exceeded the match limit"), 1);
  cr_assert(_grabbed_log_contains("suppressed_since_last_report='0'"));

  /* a new interval starts: logged again, with the 4 suppressed hits counted */
  pcre_match_limits_age_log_rate_limit(PCRE_MATCH_LIMIT_LOG_INTERVAL_SEC + 1);
  pcre_match_limited(code, subject, strlen(subject), 0, 0, md, "(a+)+$");
  cr_assert_eq(_count_grabbed("exceeded the match limit"), 2);
  cr_assert(_grabbed_log_contains("suppressed_since_last_report='4'"));

  g_free(subject);
  pcre2_match_data_free(md);
  pcre2_code_free(code);
}

Test(pcre_match_limits, multi_line_pattern_treats_a_limit_hit_as_no_match)
{
  MultiLinePattern *re = multi_line_pattern_compile("(a+)+$", NULL);
  gchar *subject = _subject(40);
  gint start, end;

  cr_assert_not(multi_line_pattern_match(re, (const guchar *) subject, strlen(subject)));
  cr_assert_not(multi_line_pattern_find(re, (const guchar *) subject, strlen(subject), &start, &end));
  cr_assert(_grabbed_log_contains("regexp='(a+)+$'"));

  /* ordinary use is unchanged */
  cr_assert(multi_line_pattern_match(re, (const guchar *) "xxaaa", 5));
  cr_assert(multi_line_pattern_find(re, (const guchar *) "xxaaa", 5, &start, &end));
  cr_assert_eq(start, 2);
  cr_assert_eq(end, 5);

  g_free(subject);
  multi_line_pattern_unref(re);
}
