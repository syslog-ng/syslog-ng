/*
 * Copyright (c) 2017 Balabit
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 */

#include "snmptrapd-header-parser.h"

#include "str-format.h"
#include "timeutils/timeutils.h"
#include "timeutils/cache.h"

#include <string.h>
#include <ctype.h>

typedef struct
{
  SnmpTrapdNVContext *nv_context;
  const gchar **input;
  gsize *input_len;
} SnmpTrapdHeaderParser;

typedef gboolean (*SnmpTrapdHeaderParserStep)(SnmpTrapdHeaderParser *self);


static inline void
_skip_spaces(SnmpTrapdHeaderParser *self)
{
  const gchar *current_char = *self->input;

  while (*self->input_len > 0 && *current_char == ' ')
    {
      ++current_char;
      --(*self->input_len);
    }

  *self->input = current_char;
}


static gboolean
_run_header_parser(SnmpTrapdHeaderParser *self,
                   SnmpTrapdHeaderParserStep *parser_steps, gsize parser_steps_size)
{
  SnmpTrapdHeaderParserStep parser_step;

  for (gsize step_index = 0; step_index < parser_steps_size; ++step_index)
    {
      _skip_spaces(self);

      parser_step = parser_steps[step_index];
      if (!parser_step(self))
        return FALSE;
    }

  return TRUE;
}


static inline gboolean
_expect_char(SnmpTrapdHeaderParser *self, gchar c)
{
  return scan_expect_char(self->input, (gint *) self->input_len, c);
}

static inline gboolean
_expect_newline(SnmpTrapdHeaderParser *self)
{
  return _expect_char(self, '\n');
}

static inline gboolean
_expect_newline_or_eom(SnmpTrapdHeaderParser *self)
{
  return _expect_newline(self) || *self->input_len == 0;
}

static inline gboolean
_expect_colon(SnmpTrapdHeaderParser *self)
{
  return _expect_char(self, ':');
}

static inline gboolean
_expect_tab(SnmpTrapdHeaderParser *self)
{
  return _expect_char(self, '\t');
}

static inline void
_scroll_to_eom(SnmpTrapdHeaderParser *self)
{
  while (*self->input_len > 0 || **self->input)
    {
      ++(*self->input);
      --(*self->input_len);
    }
}

static gboolean
_parse_v1_uptime(SnmpTrapdHeaderParser *self)
{
  if (!scan_expect_str(self->input, (gint *) self->input_len, "Uptime:"))
    return FALSE;

  _skip_spaces(self);

  const gchar *uptime_start = *self->input;

  const gchar *uptime_end = strchr(uptime_start, '\n');
  if (!uptime_end)
    {
      _scroll_to_eom(self);
      snmptrapd_nv_context_add_name_value(self->nv_context, "uptime", uptime_start, *self->input - uptime_start);
      return TRUE;
    }

  snmptrapd_nv_context_add_name_value(self->nv_context, "uptime", uptime_start, uptime_end - uptime_start);

  *self->input_len -= uptime_end - *self->input;
  *self->input = uptime_end;
  return TRUE;
}

static gboolean
_parse_v1_trap_type_and_subtype(SnmpTrapdHeaderParser *self)
{
  const gchar *type_start = *self->input;

  const gchar *type_end = strpbrk(type_start, "(\n");
  gboolean type_exists = type_end && *type_end == '(';

  if (!type_exists)
    return FALSE;

  const gchar *subtype_start = type_end + 1;

  if (*(type_end - 1) == ' ')
    --type_end;

  snmptrapd_nv_context_add_name_value(self->nv_context, "type", type_start, type_end - type_start);

  const gchar *subtype_end = strpbrk(subtype_start, ")\n");
  gboolean subtype_exists = subtype_end && *subtype_end == ')';

  if (!subtype_exists)
    return FALSE;

  snmptrapd_nv_context_add_name_value(self->nv_context, "subtype", subtype_start, subtype_end - subtype_start);

  *self->input_len -= (subtype_end + 1) - *self->input;
  *self->input = subtype_end + 1;
  return TRUE;
}

static gboolean
_parse_v1_enterprise_oid(SnmpTrapdHeaderParser *self)
{
  const gchar *enterprise_string_start = *self->input;
  gsize input_left = *self->input_len;

  while (*self->input_len > 0 && !g_ascii_isspace(**self->input))
    {
      ++(*self->input);
      --(*self->input_len);
    }

  gsize enterprise_string_length = input_left - *self->input_len;

  /* enterprise_string is optional */
  if (enterprise_string_length == 0)
    return TRUE;

  snmptrapd_nv_context_add_name_value(self->nv_context, "enterprise_oid",
                                      enterprise_string_start, enterprise_string_length);

  return TRUE;
}


static gboolean
_parse_transport_info(SnmpTrapdHeaderParser *self)
{
  if(!scan_expect_char(self->input, (gint *) self->input_len, '['))
    return FALSE;

  _skip_spaces(self);

  const gchar *transport_info_start = *self->input;

  const gchar *transport_info_end = strchr(transport_info_start, '\n');
  if (!transport_info_end)
    return FALSE;

  while(*transport_info_end != ']')
    {
      --transport_info_end;
      if(transport_info_end == transport_info_start)
        return FALSE;
    }

  gsize transport_info_len = transport_info_end - transport_info_start;

  snmptrapd_nv_context_add_name_value(self->nv_context, "transport_info", transport_info_start, transport_info_len);

  *self->input_len -= (transport_info_end + 1) - *self->input;
  *self->input = transport_info_end + 1;
  return TRUE;
}

static gboolean
_parse_hostname(SnmpTrapdHeaderParser *self)
{
  const gchar *hostname_start = *self->input;
  gsize input_left = *self->input_len;

  while (*self->input_len > 0 && !g_ascii_isspace(**self->input))
    {
      ++(*self->input);
      --(*self->input_len);
    }

  gsize hostname_length = input_left - *self->input_len;
  if (hostname_length == 0)
    return FALSE;

  snmptrapd_nv_context_add_name_value(self->nv_context, "hostname", hostname_start, hostname_length);
  return TRUE;
}

static gboolean
scan_snmptrapd_timestamp(const gchar **buf, gint *left, struct tm *tm)
{
  /* YYYY-MM-DD HH:MM:SS */
  if (!scan_int(buf, left, 4, &tm->tm_year) ||
      !scan_expect_char(buf, left, '-') ||
      !scan_int(buf, left, 2, &tm->tm_mon) ||
      !scan_expect_char(buf, left, '-') ||
      !scan_int(buf, left, 2, &tm->tm_mday) ||
      !scan_expect_char(buf, left, ' ') ||
      !scan_int(buf, left, 2, &tm->tm_hour) ||
      !scan_expect_char(buf, left, ':') ||
      !scan_int(buf, left, 2, &tm->tm_min) ||
      !scan_expect_char(buf, left, ':') ||
      !scan_int(buf, left, 2, &tm->tm_sec))
    return FALSE;
  tm->tm_year -= 1900;
  tm->tm_mon -= 1;
  return TRUE;
}

static gboolean
_parse_timestamp(SnmpTrapdHeaderParser *self)
{
  GTimeVal now;
  cached_g_current_time(&now);
  time_t now_tv_sec = (time_t) now.tv_sec;

  LogStamp *stamp = &self->nv_context->msg->timestamps[LM_TS_STAMP];
  stamp->tv_usec = 0;
  stamp->zone_offset = -1;

  /* NOTE: we initialize various unportable fields in tm using a
   * localtime call, as the value of tm_gmtoff does matter but it does
   * not exist on all platforms and 0 initializing it causes trouble on
   * time-zone barriers */

  struct tm tm;
  cached_localtime(&now_tv_sec, &tm);
  if (!scan_snmptrapd_timestamp(self->input, (gint *)self->input_len, &tm))
    return FALSE;

  tm.tm_isdst = -1;
  stamp->tv_sec = cached_mktime(&tm);
  stamp->zone_offset = get_local_timezone_ofs(stamp->tv_sec);

  return TRUE;
}

static gboolean
_try_parse_v1_info(SnmpTrapdHeaderParser *self)
{
  /* detect v1 format */
  const gchar *new_line = strchr(*self->input, '\n');
  if (new_line && new_line[1] != '\t')
    return TRUE;

  SnmpTrapdHeaderParserStep v1_info_parser_steps[] =
  {
    _parse_v1_enterprise_oid,
    _expect_newline,
    _expect_tab,
    _parse_v1_trap_type_and_subtype,
    _parse_v1_uptime
  };

  return _run_header_parser(self, v1_info_parser_steps,
                            sizeof(v1_info_parser_steps) / sizeof(SnmpTrapdHeaderParserStep));
}

gboolean
snmptrapd_header_parser_parse(SnmpTrapdNVContext *nv_context, const gchar **input, gsize *input_len)
{
  /*
   * DATE HOST [TRANSPORT_INFO]: V1_ENTERPRISE_OID
   * <TAB> V1_TRAP_TYPE (V1_TRAP_SUBTYPE) "Uptime:" UPTIME
   */

  SnmpTrapdHeaderParser self =
  {
    .nv_context = nv_context,
    .input = input,
    .input_len = input_len
  };

  SnmpTrapdHeaderParserStep parser_steps[] =
  {
    _parse_timestamp,
    _parse_hostname,
    _parse_transport_info,
    _expect_colon,
    _try_parse_v1_info,
    _expect_newline_or_eom
  };

  return _run_header_parser(&self, parser_steps, sizeof(parser_steps) / sizeof(SnmpTrapdHeaderParserStep));
}
