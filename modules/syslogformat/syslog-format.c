/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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
 *
 */

#include "syslog-format.h"
#include "timeutils/scan-timestamp.h"
#include "timeutils/conv.h"
#include "logmsg/logmsg.h"
#include "messages.h"
#include "timeutils/cache.h"
#include "find-crlf.h"
#include "cfg.h"
#include "str-format.h"
#include "utf8utils.h"
#include "str-utils.h"
#include "syslog-names.h"

#include "logproto/logproto.h"

#include <regex.h>
#include <ctype.h>
#include <string.h>

#define SD_NAME_SIZE 256

static const char aix_fwd_string[] = "Message forwarded from ";
static const char repeat_msg_string[] = "last message repeated";
static struct
{
  gboolean initialized;
  NVHandle is_synced;
  NVHandle cisco_seqid;
} handles;

static inline gboolean
_skip_char(const guchar **data, gint *left)
{
  if (*left < 1)
    return FALSE;

  (*data)++;
  (*left)--;

  return TRUE;
}

static gint
_skip_chars(const guchar **data, gint *length, const gchar *chars, gint max_len)
{
  const guchar *src = *data;
  gint left = *length;
  gint num_skipped = 0;

  while (max_len && left && _strchr_optimized_for_single_char_haystack(chars, *src))
    {
      _skip_char(&src, &left);
      num_skipped++;
      if (max_len >= 0)
        max_len--;
    }
  *data = src;
  *length = left;
  return num_skipped;
}

static gboolean
_skip_space(const guchar **data, gint *length)
{
  const guchar *src = *data;
  gint left = *length;

  if (left > 0 && *src == ' ')
    {
      _skip_char(&src, &left);
    }
  else
    {
      return FALSE;
    }

  *data = src;
  *length = left;
  return TRUE;
}

static gint
_skip_chars_until(const guchar **data, gint *length, const gchar *delims)
{
  const guchar *src = *data;
  gint left = *length;
  gint num_skipped = 0;

  while (left && _strchr_optimized_for_single_char_haystack(delims, *src) == 0)
    {
      _skip_char(&src, &left);
      num_skipped++;
    }
  *data = src;
  *length = left;
  return num_skipped;
}

static gboolean
_syslog_format_parse_pri(LogMessage *msg, const guchar **data, gint *length, guint flags, guint16 default_pri)
{
  int pri;
  gboolean success = TRUE;
  const guchar *src = *data;
  gint left = *length;

  if (left && src[0] == '<')
    {
      _skip_char(&src, &left);
      pri = 0;
      while (left && *src != '>')
        {
          if (isdigit(*src))
            {
              pri = pri * 10 + ((*src) - '0');
            }
          else
            {
              return FALSE;
            }
          _skip_char(&src, &left);
        }
      msg->pri = pri;
      if (left)
        {
          _skip_char(&src, &left);
        }
    }
  /* No priority info in the buffer? Just assign a default. */
  else
    {
      msg->pri = default_pri != 0xFFFF ? default_pri : (EVT_FAC_USER | EVT_PRI_NOTICE);
      log_msg_set_tag_by_id(msg, LM_T_SYSLOG_MISSING_PRI);
    }

  *data = src;
  *length = left;
  return success;
}

static void
_syslog_format_parse_column(LogMessage *msg, NVHandle handle, const guchar **data, gint *length, gint max_length)
{
  const guchar *src, *space;
  gint left;

  src = *data;
  left = *length;
  space = memchr(src, ' ', left);
  if (space)
    {
      left -= space - src;
      src = space;
    }
  else
    {
      src = src + left;
      left = 0;
    }
  if (left)
    {
      if ((*length - left) > 1 || (*data)[0] != '-')
        {
          gint len = (*length - left) > max_length ? max_length : (*length - left);
          log_msg_set_value(msg, handle, (gchar *) *data, len);
        }
    }
  *data = src;
  *length = left;
}

static void
_syslog_format_parse_cisco_sequence_id(LogMessage *msg, const guchar **data, gint *length)
{
  const guchar *src = *data;
  gint left = *length;

  while (left && *src != ':')
    {
      if (!isdigit(*src))
        return;
      if (!_skip_char(&src, &left))
        return;
    }
  if (!_skip_char(&src, &left))
    return;

  /* if the next char is not space, then we may try to read a date */

  if (!left || *src != ' ')
    return;

  log_msg_set_value(msg, handles.cisco_seqid, (gchar *) *data, *length - left - 1);

  *data = src;
  *length = left;
  return;
}

static void
_syslog_format_parse_cisco_timestamp_attributes(LogMessage *msg, const guchar **data, gint *length, gint parse_flags)
{
  const guchar *src = *data;
  gint left = *length;

  if (!left)
    return;

  /* Cisco timestamp extensions, the first '*' indicates that the clock is
   * unsynced, '.' if it is known to be synced */
  if (G_UNLIKELY(src[0] == '*'))
    {
      if (!(parse_flags & LP_NO_PARSE_DATE))
        log_msg_set_value(msg, handles.is_synced, "0", 1);
      _skip_char(&src, &left);
    }
  else if (G_UNLIKELY(src[0] == '.'))
    {
      if (!(parse_flags & LP_NO_PARSE_DATE))
        log_msg_set_value(msg, handles.is_synced, "1", 1);
      _skip_char(&src, &left);
    }
  *data = src;
  *length = left;
}

static gboolean
_syslog_format_parse_timestamp(LogMessage *msg, UnixTime *stamp,
                               const guchar **data, gint *length,
                               guint parse_flags, glong recv_timezone_ofs)
{
  gboolean result;
  WallClockTime wct = WALL_CLOCK_TIME_INIT;

  if ((parse_flags & LP_SYSLOG_PROTOCOL) == 0)
    result = scan_rfc3164_timestamp(data, length, &wct);
  else
    {
      if (G_UNLIKELY(*length >= 1 && (*data)[0] == '-'))
        {
          log_msg_set_tag_by_id(msg, LM_T_SYSLOG_MISSING_TIMESTAMP);
          unix_time_set_now(stamp);
          (*data)++;
          (*length)--;
          return TRUE;
        }
      result = scan_rfc5424_timestamp(data, length, &wct);
    }

  if (result && (parse_flags & LP_NO_PARSE_DATE) == 0)
    {
      convert_and_normalize_wall_clock_time_to_unix_time_with_tz_hint(&wct, stamp, recv_timezone_ofs);

      if ((parse_flags & LP_GUESS_TIMEZONE) != 0)
        unix_time_fix_timezone_assuming_the_time_matches_real_time(stamp);
    }

  return result;
}

static gboolean
_syslog_format_parse_date(LogMessage *msg, const guchar **data, gint *length, guint parse_flags,
                          glong recv_timezone_ofs)
{
  UnixTime *stamp = &msg->timestamps[LM_TS_STAMP];

  unix_time_unset(stamp);
  if (!_syslog_format_parse_timestamp(msg, stamp, data, length, parse_flags, recv_timezone_ofs))
    {
      *stamp = msg->timestamps[LM_TS_RECVD];
      unix_time_set_timezone(stamp, recv_timezone_ofs);
      log_msg_set_tag_by_id(msg, LM_T_SYSLOG_MISSING_TIMESTAMP);
      return FALSE;
    }

  return TRUE;
}

static gboolean
_syslog_format_parse_version(LogMessage *msg, const guchar **data, gint *length)
{
  const guchar *src = *data;
  gint left = *length;
  gint version = 0;

  while (left && *src != ' ')
    {
      if (isdigit(*src))
        {
          version = version * 10 + ((*src) - '0');
        }
      else
        {
          return FALSE;
        }
      _skip_char(&src, &left);
    }
  if (version != 1)
    return FALSE;

  *data = src;
  *length = left;
  return TRUE;
}

static void
_syslog_format_parse_legacy_program_name(LogMessage *msg, const guchar **data, gint *length, guint flags)
{
  /* the data pointer will not change */
  const guchar *src, *prog_start;
  gint left;

  src = *data;
  left = *length;
  prog_start = src;
  while (left && *src != ' ' && *src != '[' && *src != ':')
    {
      _skip_char(&src, &left);
    }
  log_msg_set_value(msg, LM_V_PROGRAM, (gchar *) prog_start, src - prog_start);
  if (left > 0 && *src == '[')
    {
      const guchar *pid_start = src + 1;
      while (left && *src != ' ' && *src != ']' && *src != ':')
        {
          _skip_char(&src, &left);
        }
      if (left)
        {
          log_msg_set_value(msg, LM_V_PID, (gchar *) pid_start, src - pid_start);
        }
      if (left > 0 && *src == ']')
        {
          _skip_char(&src, &left);
        }
    }
  if (left > 0 && *src == ':')
    {
      _skip_char(&src, &left);
    }
  if (left > 0 && *src == ' ')
    {
      _skip_char(&src, &left);
    }
  if ((flags & LP_STORE_LEGACY_MSGHDR))
    {
      log_msg_set_value(msg, LM_V_LEGACY_MSGHDR, (gchar *) *data, *length - left);
    }
  *data = src;
  *length = left;
}

static guint8 invalid_chars[32];

static void
_init_parse_hostname_invalid_chars(void)
{
  if ((invalid_chars[0] & 0x1) == 0)
    {
      gint i;
      /* we use a bit string to represent valid/invalid characters  when check_hostname is enabled */

      /* not yet initialized */
      for (i = 0; i < 256; i++)
        {
          if (!((i >= 'A' && i <= 'Z') ||
                (i >= 'a' && i <= 'z') ||
                (i >= '0' && i <= '9') ||
                i == '-' || i == '_' ||
                i == '.' || i == ':' ||
                i == '@' || i == '/'))
            {
              invalid_chars[i / 8] |= 1 << (i % 8);
            }
        }
      invalid_chars[0] |= 0x1;
    }
}

static inline gboolean
_is_invalid_hostname_char(guchar c)
{
  return invalid_chars[c / 8] & (1 << (c % 8));
}

typedef struct _IPv6Heuristics
{
  gint8 current_segment;
  gint8 digits_in_segment;
  gboolean heuristic_failed;
} IPv6Heuristics;

static gboolean
ipv6_heuristics_feed_gchar(IPv6Heuristics *self, gchar c)
{
  if (self->heuristic_failed)
    return FALSE;

  if (c != ':' && !g_ascii_isxdigit(c))
    {
      self->heuristic_failed = TRUE;
      return FALSE;
    }

  if (g_ascii_isxdigit(c))
    {
      if (++self->digits_in_segment > 4)
        {
          self->heuristic_failed = TRUE;
          return FALSE;
        }
    }

  if (c == ':')
    {
      self->digits_in_segment = 0;
      if (++self->current_segment >= 8)
        {
          self->heuristic_failed = TRUE;
          return FALSE;
        }
    }

  return TRUE;
}

static void
_syslog_format_parse_hostname(LogMessage *msg, const guchar **data, gint *length,
                              const guchar **hostname_start, int *hostname_len,
                              guint flags, regex_t *bad_hostname)
{
  /* FIXME: support nil value support  with new protocol*/
  const guchar *src, *oldsrc;
  gint left, oldleft;
  gchar hostname_buf[256];
  gint dst = 0;

  IPv6Heuristics ipv6_heuristics = {0};

  src = *data;
  left = *length;

  /* If we haven't already found the original hostname,
     look for it now. */

  oldsrc = src;
  oldleft = left;

  while (left && *src != ' ' && *src != '[' && dst < sizeof(hostname_buf) - 1)
    {
      ipv6_heuristics_feed_gchar(&ipv6_heuristics, *src);

      if (*src == ':' && ipv6_heuristics.heuristic_failed)
        {
          break;
        }

      if (G_UNLIKELY((flags & LP_CHECK_HOSTNAME) && _is_invalid_hostname_char(*src)))
        {
          break;
        }
      hostname_buf[dst++] = *src;
      _skip_char(&src, &left);
    }
  hostname_buf[dst] = 0;

  if (left && *src == ' ' &&
      (!bad_hostname || regexec(bad_hostname, hostname_buf, 0, NULL, 0)))
    {
      /* This was a hostname. It came from a
         syslog-ng, since syslogd doesn't send
         hostnames. It's even better then the one
         we got from the AIX fwd message, if we
         did. */
      *hostname_start = oldsrc;
      *hostname_len = oldleft - left;
    }
  else
    {
      *hostname_start = NULL;
      *hostname_len = 0;

      src = oldsrc;
      left = oldleft;
      log_msg_set_tag_by_id(msg, LM_T_SYSLOG_INVALID_HOSTNAME);
    }

  if (*hostname_len > 255)
    *hostname_len = 255;

  *data = src;
  *length = left;
}

/**
 * _syslog_format_parse:
 * @msg: LogMessage instance to store parsed information into
 * @data: message
 * @length: length of the message pointed to by @data
 * @flags: value affecting how the message is parsed (bits from LP_*)
 *
 * Parse an http://www.syslog.cc/ietf/drafts/draft-ietf-syslog-protocol-23.txt formatted log
 * message for structured data elements and store the parsed information
 * in @msg.values and dup the SD string. Parsing is affected by the bits set @flags argument.
 **/
gboolean
_syslog_format_parse_sd(LogMessage *msg, const guchar **data, gint *length, const MsgFormatOptions *options)
{
  /*
   * STRUCTURED-DATA = NILVALUE / 1*SD-ELEMENT
   * SD-ELEMENT      = "[" SD-ID *(SP SD-PARAM) "]"
   * SD-PARAM        = PARAM-NAME "=" %d34 PARAM-VALUE %d34
   * SD-ID           = SD-NAME
   * PARAM-NAME      = SD-NAME
   * PARAM-VALUE     = UTF-8-STRING ; characters '"', '\' and
   *                                ; ']' MUST be escaped.
   * SD-NAME         = 1*32PRINTUSASCII ; except '=', SP, ']', %d34 (")
   *
   * Example Structured Data string:
   *
   *   [exampleSDID@0 iut="3" eventSource="Application" eventID="1011"][examplePriority@0 class="high"]
   *
   * NOTE: To increase compatibility, we modified the parser to accept longer than 32 character
   * SD-ID's and SD-PARAM's. However there is still a practical limit (255 characters) for the
   * values, since currently NVPairs can not store longer ID's.
   */

  gboolean ret = FALSE;
  const guchar *src = *data;
  /* ASCII string */
  gchar sd_id_name[SD_NAME_SIZE];
  gsize sd_id_len;
  gchar sd_param_name[SD_NAME_SIZE];

  /* UTF-8 string */
  gchar sd_param_value[options->sdata_param_value_max + 1];
  gsize sd_param_value_len;
  gchar sd_value_name[SD_NAME_SIZE];

  g_assert(options->sdata_prefix_len < SD_NAME_SIZE);

  guint open_sd = 0;
  gint left = *length, pos;

  if (left && src[0] == '-')
    {
      /* Nothing to do here */
      _skip_char(&src, &left);
    }
  else if (left && src[0] == '[')
    {
      _skip_char(&src, &left);
      open_sd++;
      do
        {
          if (!left || !isascii(*src) || *src == '=' || *src == ' ' || *src == ']' || *src == '"')
            goto error;
          /* read sd_id */
          pos = 0;
          while (left && *src != ' ' && *src != ']')
            {
              if (pos < sizeof(sd_id_name) - 1 - options->sdata_prefix_len)
                {
                  if (isascii(*src) && *src != '=' && *src != ' ' && *src != ']' && *src != '"')
                    {
                      sd_id_name[pos] = *src;
                      pos++;
                    }
                  else
                    {
                      goto error;
                    }
                }
              else
                {
                  goto error;
                }
              _skip_char(&src, &left);
            }

          if (pos == 0)
            goto error;

          sd_id_name[pos] = 0;
          sd_id_len = pos;
          strcpy(sd_value_name, options->sdata_prefix);
          g_strlcpy(sd_value_name + options->sdata_prefix_len, sd_id_name, sizeof(sd_value_name) - options->sdata_prefix_len);

          if (left && *src == ']')
            {
              log_msg_set_value_by_name(msg, sd_value_name, "", 0);
            }
          else
            {
              if (options->sdata_prefix_len + pos + 1 >= sizeof(sd_value_name))
                goto error;

              sd_value_name[options->sdata_prefix_len + pos] = '.';
              sd_value_name[options->sdata_prefix_len + pos + 1] = 0;
            }

          g_assert(sd_id_len < sizeof(sd_param_name));

          /* read sd-element */
          while (left && *src != ']')
            {
              if (left && *src == ' ') /* skip the ' ' before the parameter name */
                _skip_char(&src, &left);
              else
                goto error;

              if (!left || !isascii(*src) || *src == '=' || *src == ' ' || *src == ']' || *src == '"')
                goto error;

              /* read sd-param */
              pos = 0;
              while (left && *src != '=')
                {
                  if (pos < sizeof(sd_param_name) - 1 - sd_id_len)
                    {
                      if (isascii(*src) && *src != '=' && *src != ' ' && *src != ']' && *src != '"')
                        {
                          sd_param_name[pos] = *src;
                          pos++;
                        }
                      else
                        goto error;
                    }
                  else
                    {
                      goto error;
                    }
                  _skip_char(&src, &left);
                }
              sd_param_name[pos] = 0;
              gsize sd_param_name_len = g_strlcpy(&sd_value_name[options->sdata_prefix_len + 1 + sd_id_len],
                                                  sd_param_name,
                                                  sizeof(sd_value_name) - options->sdata_prefix_len - 1 - sd_id_len);

              if (sd_param_name_len >= sizeof(sd_value_name) - options->sdata_prefix_len - 1 - sd_id_len)
                goto error;

              if (left && *src == '=')
                _skip_char(&src, &left);
              else
                goto error;

              /* read sd-param-value */

              if (left && *src == '"')
                {
                  gboolean quote = FALSE;
                  /* opening quote */
                  _skip_char(&src, &left);
                  pos = 0;

                  while (left && (*src != '"' || quote))
                    {
                      if (!quote && *src == '\\')
                        {
                          quote = TRUE;
                        }
                      else
                        {
                          if (quote && *src != '"' && *src != ']' && *src != '\\' && pos < sizeof(sd_param_value) - 1)
                            {
                              sd_param_value[pos] = '\\';
                              pos++;
                            }
                          else if (!quote &&  *src == ']')
                            {
                              _skip_char(&src, &left);
                              goto error;
                            }
                          if (pos < sizeof(sd_param_value) - 1)
                            {
                              sd_param_value[pos] = *src;
                              pos++;
                            }
                          quote = FALSE;
                        }
                      _skip_char(&src, &left);
                    }
                  sd_param_value[pos] = 0;
                  sd_param_value_len = pos;

                  if (left && *src == '"')/* closing quote */
                    _skip_char(&src, &left);
                  else
                    goto error;
                }
              else if (left)
                {
                  pos = 0;

                  while (left && (*src != ' ' && *src != ']'))
                    {
                      if (pos < sizeof(sd_param_value) - 1)
                        {
                          sd_param_value[pos] = *src;
                          pos++;
                        }
                      _skip_char(&src, &left);
                    }
                  sd_param_value[pos] = 0;
                  sd_param_value_len = pos;

                }
              else
                {
                  goto error;
                }

              log_msg_set_value_by_name(msg, sd_value_name, sd_param_value, sd_param_value_len);
            }

          if (left && *src == ']')
            {
              _skip_char(&src, &left);
              open_sd--;
            }
          else
            {
              goto error;
            }

          /* if any other sd then continue*/
          if (left && *src == '[')
            {
              /* new structured data begins, thus continue iteration */
              _skip_char(&src, &left);
              open_sd++;
            }
        }
      while (left && open_sd != 0);
    }
  else
    {
      goto error;
    }
  ret = TRUE;
error:
  /* FIXME: what happens if an error occurs? there's no way to return a
   * failure from here, but nevertheless we should do something sane, e.g.
   * don't parse the SD string, but skip to the end so that the $MSG
   * contents are correctly parsed. */

  *data = src;
  *length = left;
  return ret;
}

gboolean
_syslog_format_parse_sd_column(LogMessage *msg, const guchar **data, gint *length, const MsgFormatOptions *options)
{
  if (*length == 0)
    return TRUE;

  guchar first_char = (*data)[0];
  if (first_char == '-' || first_char == '[')
    return _syslog_format_parse_sd(msg, data, length, options);

  /* the SDATA block is not there, skip parsing it as this is how we have
   * processed SDATA blocks since we added RFC5424.  This is more forgiving
   * than strict RFC5424 but apps have a bad history of conforming to it
   * anyway.  */
  return TRUE;
}

gboolean
_syslog_format_parse_message_column(LogMessage *msg,
                                    const guchar **data, gint *length,
                                    const MsgFormatOptions *parse_options)
{
  const guchar *src = (guchar *) *data;
  gint left = *length;

  /* checking if there are remaining data in log message */
  if (left != 0)
    {
      /* optional part of the log message [SP MSG] */
      if (!_skip_space(&src, &left))
        {
          return FALSE;
        }

      if (left >= 3 && memcmp(src, "\xEF\xBB\xBF", 3) == 0)
        {
          /* we have a BOM, this is UTF8 */
          msg->flags |= LF_UTF8;
          src += 3;
          left -= 3;

          log_msg_set_value(msg, LM_V_MESSAGE, (gchar *) src, left);
          return TRUE;
        }

      if ((parse_options->flags & LP_SANITIZE_UTF8))
        {
          if (!g_utf8_validate((gchar *) src, left, NULL))
            {
              gchar buf[SANITIZE_UTF8_BUFFER_SIZE(left)];
              gsize sanitized_length;
              optimized_sanitize_utf8_to_escaped_binary(src, left, &sanitized_length, buf, sizeof(buf));
              log_msg_set_value(msg, LM_V_MESSAGE, buf, sanitized_length);
              log_msg_set_tag_by_id(msg, LM_T_MSG_UTF8_SANITIZED);
              msg->flags |= LF_UTF8;
              return TRUE;
            }
          else
            msg->flags |= LF_UTF8;
        }
      else if ((parse_options->flags & LP_VALIDATE_UTF8) && g_utf8_validate((gchar *) src, left, NULL))
        msg->flags |= LF_UTF8;
    }
  log_msg_set_value(msg, LM_V_MESSAGE, (gchar *) src, left);
  return TRUE;
}

static gboolean
_syslog_format_parse_legacy_header(LogMessage *msg, const guchar **data, gint *length,
                                   const MsgFormatOptions *parse_options)
{
  const guchar *src = *data;
  gint left = *length;
  time_t now;

  _syslog_format_parse_cisco_sequence_id(msg, &src, &left);
  _skip_chars(&src, &left, " ", -1);
  _syslog_format_parse_cisco_timestamp_attributes(msg, &src, &left, parse_options->flags);

  now = get_cached_realtime_sec();
  if (_syslog_format_parse_date(msg, &src, &left, parse_options->flags & ~LP_SYSLOG_PROTOCOL,
                                time_zone_info_get_offset(parse_options->recv_time_zone_info, now)))
    {
      /* Expected format: hostname program[pid]: */
      /* Possibly: Message forwarded from hostname: ... */
      const guchar *hostname_start = NULL;
      int hostname_len = 0;

      _skip_chars(&src, &left, " ", -1);

      /* Detect funny AIX syslogd forwarded message. */
      if (G_UNLIKELY(left >= (sizeof(aix_fwd_string) - 1) &&
                     !memcmp(src, aix_fwd_string, sizeof(aix_fwd_string) - 1)))
        {
          src += sizeof(aix_fwd_string) - 1;
          left -= sizeof(aix_fwd_string) - 1;
          hostname_start = src;
          hostname_len = _skip_chars_until(&src, &left, ":");
          _skip_chars(&src, &left, " :", -1);
        }

      /* Now, try to tell if it's a "last message repeated" line */
      if (G_UNLIKELY(left >= sizeof(repeat_msg_string) &&
                     !memcmp(src, repeat_msg_string, sizeof(repeat_msg_string) - 1)))
        {
          ;     /* It is. Do nothing since there's no hostname or program name coming. */
        }
      else
        {
          if (!hostname_start && (parse_options->flags & LP_EXPECT_HOSTNAME))
            {
              /* Don't parse a hostname if it is local */
              /* It's a regular ol' message. */
              _syslog_format_parse_hostname(msg, &src, &left, &hostname_start, &hostname_len, parse_options->flags,
                                            parse_options->bad_hostname);

              /* Skip whitespace. */
              _skip_chars(&src, &left, " ", -1);
            }

          /* Try to extract a program name */
          _syslog_format_parse_legacy_program_name(msg, &src, &left, parse_options->flags);
        }

      /* If we did manage to find a hostname, store it. */
      if (hostname_start)
        {
          log_msg_set_value(msg, LM_V_HOST, (gchar *) hostname_start, hostname_len);
        }
    }
  else
    {
      /* no timestamp, format is expected to be "program[pid] message" */
      /* Different format */

      /* A kernel message? Use 'kernel' as the program name. */
      if (((msg->pri & SYSLOG_FACMASK) == LOG_KERN && (parse_options->flags & LP_LOCAL) != 0))
        {
          log_msg_set_value(msg, LM_V_PROGRAM, "kernel", 6);
        }
      /* No, not a kernel message. */
      else
        {
          log_msg_set_tag_by_id(msg, LM_T_SYSLOG_RFC3164_MISSING_HEADER);
          /* Capture the program name */
          _syslog_format_parse_legacy_program_name(msg, &src, &left, parse_options->flags);
        }
    }
  *data = src;
  *length = left;
  return TRUE;
}

/* validate that we did not receive an RFC5425 style octet count, which
 * should have already been processed by the time we got here, unless the
 * transport is incorrectly configured */
static void
_syslog_format_check_framing(LogMessage *msg, const guchar **data, gint *length)
{
  const guchar *src = *data;
  gint left = *length;
  gint i = 0;

  while (left > 0 && isdigit(*src))
    {
      if (!_skip_char(&src, &left))
        return;

      i++;

      if (i > RFC6587_MAX_FRAME_LEN_DIGITS)
        return;
    }

  if (i == 0 || *src != ' ')
    return;

  /* we did indeed find a series of digits that look like framing, that's
   * probably not what was intended. */
  msg_debug("RFC5425 style octet count was found at the start of the message, this is probably not what was intended",
            evt_tag_mem("data", data, src - (*data)),
            evt_tag_msg_reference(msg));
  log_msg_set_tag_by_id(msg, LM_T_SYSLOG_UNEXPECTED_FRAMING);
  *data = src;
  *length = left;
}

static void
_syslog_format_parse_legacy_message(LogMessage *msg,
                                    const guchar **data, gint *length,
                                    const MsgFormatOptions *parse_options)
{
  const guchar *src = (const guchar *) *data;
  gint left = *length;

  if (parse_options->flags & LP_SANITIZE_UTF8)
    {
      if (!g_utf8_validate((gchar *) src, left, NULL))
        {
          /* invalid utf8, sanitize it and then remember it is now utf8 clean */
          gchar buf[SANITIZE_UTF8_BUFFER_SIZE(left)];
          gsize sanitized_length;
          optimized_sanitize_utf8_to_escaped_binary(src, left, &sanitized_length, buf, sizeof(buf));
          log_msg_set_value(msg, LM_V_MESSAGE, buf, sanitized_length);
          log_msg_set_tag_by_id(msg, LM_T_MSG_UTF8_SANITIZED);
          msg->flags |= LF_UTF8;
          return;
        }
      else
        {
          /* valid utf8, no need to sanitize, store it and mark it as utf8 clean */
          msg->flags |= LF_UTF8;
        }
    }
  else if ((parse_options->flags & LP_VALIDATE_UTF8) && g_utf8_validate((gchar *) src, left, NULL))
    {
      /* valid utf8, mark it as utf8 clean */
      msg->flags |= LF_UTF8;
    }

  log_msg_set_value(msg, LM_V_MESSAGE, (gchar *) src, left);
}

/**
 * _syslog_format_parse_legacy:
 * @msg: LogMessage instance to store parsed information into
 * @data: message
 * @length: length of the message pointed to by @data
 * @flags: value affecting how the message is parsed (bits from LP_*)
 *
 * Parse an RFC3164 formatted log message and store the parsed information
 * in @msg. Parsing is affected by the bits set @flags argument.
 **/
static gboolean
_syslog_format_parse_legacy(const MsgFormatOptions *parse_options,
                            const guchar *data, gint length,
                            LogMessage *msg, gsize *position)
{
  const guchar *src;
  gint left;

  src = (const guchar *) data;
  left = length;

  _syslog_format_check_framing(msg, &src, &left);
  if (!_syslog_format_parse_pri(msg, &src, &left, parse_options->flags, parse_options->default_pri))
    {
      goto error;
    }

  if ((parse_options->flags & LP_NO_HEADER) == 0)
    _syslog_format_parse_legacy_header(msg, &src, &left, parse_options);

  _syslog_format_parse_legacy_message(msg, &src, &left, parse_options);

  log_msg_set_value_to_string(msg, LM_V_MSGFORMAT, "rfc3164");
  return TRUE;
error:
  *position = src - data;
  return FALSE;
}

/**
 * _syslog_format_parse_syslog_proto:
 *
 * Parse a message according to the latest syslog-protocol drafts.
 **/
static gboolean
_syslog_format_parse_syslog_proto(const MsgFormatOptions *parse_options, const guchar *data, gint length,
                                  LogMessage *msg,
                                  gsize *position)
{
  /**
   *  SYSLOG-MSG      = HEADER SP STRUCTURED-DATA [SP MSG]
   *  HEADER          = PRI VERSION SP TIMESTAMP SP HOSTNAME
   *                        SP APP-NAME SP PROCID SP MSGID
   *    SP              = ' ' (space)
   *
   *    <165>1 2003-10-11T22:14:15.003Z mymachine.example.com evntslog - ID47 [exampleSDID@0 iut="3" eventSource="Application" eventID="1011"] BOMAn application
   *    event log entry...
   **/

  const guchar *src;
  gint left;
  const guchar *hostname_start = NULL;
  gint hostname_len = 0;

  src = (guchar *) data;
  left = length;

  _syslog_format_check_framing(msg, &src, &left);

  if (!_syslog_format_parse_pri(msg, &src, &left, parse_options->flags, parse_options->default_pri) ||
      !_syslog_format_parse_version(msg, &src, &left))
    {
      if ((parse_options->flags & LP_NO_RFC3164_FALLBACK) == 0)
        return _syslog_format_parse_legacy(parse_options, data, length, msg, position);
      return FALSE;
    }

  if (!_skip_space(&src, &left))
    {
      goto error;
    }

  /* ISO time format */
  time_t now = get_cached_realtime_sec();
  if (!_syslog_format_parse_date(msg, &src, &left, parse_options->flags,
                                 time_zone_info_get_offset(parse_options->recv_time_zone_info, now)))
    goto error;

  if (!_skip_space(&src, &left))
    goto error;

  /* hostname 255 ascii */
  _syslog_format_parse_hostname(msg, &src, &left, &hostname_start, &hostname_len, parse_options->flags, NULL);
  if (!_skip_space(&src, &left))
    {
      src++;
      goto error;
    }
  /* If we did manage to find a hostname, store it. */
  if (hostname_start && hostname_len == 1 && *hostname_start == '-')
    ;
  else if (hostname_start)
    {
      log_msg_set_value(msg, LM_V_HOST, (gchar *) hostname_start, hostname_len);
    }

  /* application name 48 ascii*/
  _syslog_format_parse_column(msg, LM_V_PROGRAM, &src, &left, 48);
  if (!_skip_space(&src, &left))
    goto error;

  /* process id 128 ascii */
  _syslog_format_parse_column(msg, LM_V_PID, &src, &left, 128);
  if (!_skip_space(&src, &left))
    goto error;

  /* message id 32 ascii */
  _syslog_format_parse_column(msg, LM_V_MSGID, &src, &left, 32);
  if (!_skip_space(&src, &left))
    goto error;

  /* structured data part */
  if (!_syslog_format_parse_sd_column(msg, &src, &left, parse_options))
    goto error;

  if (!_syslog_format_parse_message_column(msg, &src, &left, parse_options))
    goto error;

  log_msg_set_value_to_string(msg, LM_V_MSGFORMAT, "rfc5424");

  return TRUE;
error:
  *position = src - data;
  return FALSE;
}

gboolean
syslog_format_handler(const MsgFormatOptions *parse_options,
                      LogMessage *msg,
                      const guchar *data, gsize length,
                      gsize *problem_position)
{
  gboolean success;

  while (length > 0 && (data[length - 1] == '\n' || data[length - 1] == '\0'))
    length--;

  msg->initial_parse = TRUE;
  if (parse_options->flags & LP_SYSLOG_PROTOCOL)
    success = _syslog_format_parse_syslog_proto(parse_options, data, length, msg, problem_position);
  else
    success = _syslog_format_parse_legacy(parse_options, data, length, msg, problem_position);
  msg->initial_parse = FALSE;

  return success;
}

void
syslog_format_init(void)
{
  if (!handles.initialized)
    {
      handles.is_synced = log_msg_get_value_handle(".SDATA.timeQuality.isSynced");
      handles.cisco_seqid = log_msg_get_value_handle(".SDATA.meta.sequenceId");
      handles.initialized = TRUE;
    }

  _init_parse_hostname_invalid_chars();
}
