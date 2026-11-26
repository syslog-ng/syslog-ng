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

#ifndef PCRE_MATCH_LIMITS_H_INCLUDED
#define PCRE_MATCH_LIMITS_H_INCLUDED

#include "syslog-ng.h"
#include "compat/pcre.h"

/*
 * Explicit PCRE2 work limits for matching a (trusted, configured) pattern
 * against a (potentially attacker-influenced) log message.
 *
 * PCRE2 counts a "match limit" unit for every internal backtracking point it
 * tries and a "depth limit" unit for every level of nested backtracking, in
 * both the interpreter and in JIT compiled code (JIT ignores the depth limit).
 * Without a match context, the limits are whatever the PCRE2 library was built
 * with (10 million by default, but distribution dependent): a pathological
 * pattern/subject pair such as "(a+)+$" against "aaaa...ab" then keeps a
 * worker thread busy for 0.04 s (JIT) to 0.25 s (interpreter) per message.
 *
 * 1 million is far above what ordinary patterns need, which is bounded by
 * about the subject length for the same shape of patterns: a pattern like
 * "^(?:.*\n)*zzz" needs ~65600 units for a 64 KiB subject, typical patterns
 * need less than 100. It is 10 times lower than the PCRE2 default, so a
 * pathological pattern costs ~4 ms (JIT) to ~25 ms (interpreter) at most.
 */
#define PCRE_MATCH_LIMIT 1000000
#define PCRE_DEPTH_LIMIT 1000000

/* Minimum time between two "limit reached" log messages, others are counted. */
#define PCRE_MATCH_LIMIT_LOG_INTERVAL_SEC 10

/* The shared, read-only match context carrying the limits above (thread safe). */
pcre2_match_context *pcre_match_limits_get_context(void);

/*
 * pcre2_match() with pcre_match_limits_get_context(). Returns the pcre2_match()
 * result unchanged, so callers keep their existing handling of every code
 * (PCRE2_ERROR_MATCHLIMIT and PCRE2_ERROR_DEPTHLIMIT are errors, that is "no
 * match", like before). When a limit is hit it logs a rate limited warning
 * naming @pattern, the subject length, but never the subject itself.
 * Callers must not log PCRE2_ERROR_MATCHLIMIT/DEPTHLIMIT again.
 */
gint pcre_match_limited(const pcre2_code *code, const gchar *subject, gsize subject_len, gsize start_offset,
                        guint32 options, pcre2_match_data *match_data, const gchar *pattern);

gboolean pcre_match_limits_is_limit_error(gint rc);

/* Test hooks: forget the last log time / pretend it was @seconds ago. */
void pcre_match_limits_reset_log_rate_limit(void);
void pcre_match_limits_age_log_rate_limit(gint seconds);

#endif
