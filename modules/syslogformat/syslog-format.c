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
#include "logmsg/logmsg.h"
#include "messages.h"
#include "timeutils.h"
#include "find-crlf.h"
#include "cfg.h"
#include "str-format.h"
#include "utf8utils.h"
#include "str-utils.h"

#include <regex.h>
#include <ctype.h>
#include <string.h>

static const char aix_fwd_string[] = "Message forwarded from ";
static const char repeat_msg_string[] = "last message repeated";
static struct
{
  gboolean initialized;
  NVHandle is_synced;
  NVHandle cisco_seqid;
  NVHandle raw_message;
} handles;

static gboolean
log_msg_parse_pri(LogMessage *self, const guchar **data, gint *length, guint flags, guint16 default_pri)
{
  int pri;
  gboolean success = TRUE;
  const guchar *src = *data;
  gint left = *length;

  if (left && src[0] == '<')
    {
      src++;
      left--;
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
          src++;
          left--;
        }
      self->pri = pri;
      if (left)
        {
          src++;
          left--;
        }
    }
  /* No priority info in the buffer? Just assign a default. */
  else
    {
      self->pri = default_pri != 0xFFFF ? default_pri : (EVT_FAC_USER | EVT_PRI_NOTICE);
    }

  *data = src;
  *length = left;
  return success;
}

static gint
log_msg_parse_skip_chars(LogMessage *self, const guchar **data, gint *length, const gchar *chars, gint max_len)
{
  const guchar *src = *data;
  gint left = *length;
  gint num_skipped = 0;

  while (max_len && left && _strchr_optimized_for_single_char_haystack(chars, *src))
    {
      src++;
      left--;
      num_skipped++;
      if (max_len >= 0)
        max_len--;
    }
  *data = src;
  *length = left;
  return num_skipped;
}

static gboolean
log_msg_parse_skip_space(LogMessage *self, const guchar **data, gint *length)
{
  const guchar *src = *data;
  gint left = *length;

  if (left > 0 && *src == ' ')
    {
      src++;
      left--;
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
log_msg_parse_skip_chars_until(LogMessage *self, const guchar **data, gint *length, const gchar *delims)
{
  const guchar *src = *data;
  gint left = *length;
  gint num_skipped = 0;

  while (left && _strchr_optimized_for_single_char_haystack(delims, *src) == 0)
    {
      src++;
      left--;
      num_skipped++;
    }
  *data = src;
  *length = left;
  return num_skipped;
}

static void
log_msg_parse_column(LogMessage *self, NVHandle handle, const guchar **data, gint *length, gint max_length)
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
          log_msg_set_value(self, handle, (gchar *) *data, len);
        }
    }
  *data = src;
  *length = left;
}

static gboolean
log_msg_parse_seq(LogMessage *self, const guchar **data, gint *length)
{
  const guchar *src = *data;
  gint left = *length;


  while (left && *src != ':')
    {
      if (!isdigit(*src))
        return FALSE;
      src++;
      left--;
    }
  src++;
  left--;

  /* if the next char is not space, then we may try to read a date */

  if (*src != ' ')
    return FALSE;

  log_msg_set_value(self, handles.cisco_seqid, (gchar *) *data, *length - left - 1);

  *data = src;
  *length = left;
  return TRUE;
}

static guint32
__parse_iso_timezone(const guchar **data, gint *length)
{
  gint hours, mins;
  const guchar *src = *data;
  guint32 tz = 0;
  /* timezone offset */
  gint sign = *src == '-' ? -1 : 1;

  hours = (*(src + 1) - '0') * 10 + *(src + 2) - '0';
  mins = (*(src + 4) - '0') * 10 + *(src + 5) - '0';
  tz = sign * (hours * 3600 + mins * 60);
  src += 6;
  (*length) -= 6;
  *data = src;
  return tz;
}

static gboolean
__is_iso_stamp(const gchar *stamp, gint length)
{
  return (length >= 19
          && stamp[4] == '-'
          && stamp[7] == '-'
          && stamp[10] == 'T'
          && stamp[13] == ':'
          && stamp[16] == ':'
         );
}

static guint32
__parse_usec(const guchar **data, gint *length)
{
  guint32 usec = 0;
  const guchar *src = *data;
  if (*length > 0 && *src == '.')
    {
      gulong frac = 0;
      gint div = 1;
      /* process second fractions */

      src++;
      (*length)--;
      while (*length > 0 && div < 10e5 && isdigit(*src))
        {
          frac = 10 * frac + (*src) - '0';
          div = div * 10;
          src++;
          (*length)--;
        }
      while (isdigit(*src))
        {
          src++;
          (*length)--;
        }
      usec = frac * (1000000 / div);
    }
  *data = src;
  return usec;
}

static gboolean
__has_iso_timezone(const guchar *src, gint length)
{
  return (length >= 5) &&
         (*src == '+' || *src == '-') &&
         isdigit(*(src+1)) &&
         isdigit(*(src+2)) &&
         *(src+3) == ':' &&
         isdigit(*(src+4)) &&
         isdigit(*(src+5)) &&
         !isdigit(*(src+6));
}

static gboolean
__parse_iso_stamp(const GTimeVal *now, LogMessage *self, struct tm *tm, const guchar **data, gint *length)
{
  /* RFC3339 timestamp, expected format: YYYY-MM-DDTHH:MM:SS[.frac]<+/->ZZ:ZZ */
  time_t now_tv_sec = (time_t) now->tv_sec;
  const guchar *src = *data;

  self->timestamps[LM_TS_STAMP].tv_usec = 0;

  /* NOTE: we initialize various unportable fields in tm using a
   * localtime call, as the value of tm_gmtoff does matter but it does
   * not exist on all platforms and 0 initializing it causes trouble on
   * time-zone barriers */

  cached_localtime(&now_tv_sec, tm);
  if (!scan_iso_timestamp((const gchar **) &src, length, tm))
    {
      return FALSE;
    }

  self->timestamps[LM_TS_STAMP].tv_usec = __parse_usec(&src, length);

  if (*length > 0 && *src == 'Z')
    {
      /* Z is special, it means UTC */
      self->timestamps[LM_TS_STAMP].zone_offset = 0;
      src++;
      (*length)--;
    }
  else if (__has_iso_timezone(src, *length))
    {
      self->timestamps[LM_TS_STAMP].zone_offset = __parse_iso_timezone(&src, length);
    }
  else
    {
      self->timestamps[LM_TS_STAMP].zone_offset = -1;
    }

  *data = src;
  return TRUE;
}

static gboolean
__is_bsd_pix_or_asa(const guchar *src, guint32 left)
{
  return (left >= 21
          && src[3] == ' '
          && src[6] == ' '
          && src[11] == ' '
          && src[14] == ':'
          && src[17] == ':'
          && (src[20] == ':' || src[20] == ' ')
          && isdigit(src[7])
          && isdigit(src[8])
          && isdigit(src[9])
          && isdigit(src[10])
         );
}

static gboolean
__is_bsd_rfc_3164(const guchar *src, guint32 left)
{
  return left >= 15 && src[3] == ' ' && src[6] == ' ' && src[9] == ':' && src[12] == ':';
}

static gboolean
__is_bsd_linksys(const guchar *src, guint32 left)
{
  return (left >= 21
          && __is_bsd_rfc_3164(src, left)
          && src[15] == ' '
          && isdigit(src[16])
          && isdigit(src[17])
          && isdigit(src[18])
          && isdigit(src[19])
          && isspace(src[20])
         );
}

static gboolean
__parse_bsd_timestamp(const guchar **data, gint *length, const GTimeVal *now, struct tm *tm, glong *usec)
{
  gint left = *length;
  const guchar *src = *data;
  time_t now_tv_sec = (time_t) now->tv_sec;
  struct tm local_time;
  cached_localtime(&now_tv_sec, tm);
  cached_localtime(&now_tv_sec, &local_time);

  if (__is_bsd_pix_or_asa(src, left))
    {
      if (!scan_pix_timestamp((const gchar **) &src, &left, tm))
        return FALSE;

      if (*src == ':')
        {
          src++;
          left--;
        }
    }
  else if (__is_bsd_linksys(src, left))
    {
      if (!scan_linksys_timestamp((const gchar **) &src, &left, tm))
        return FALSE;
    }
  else if (__is_bsd_rfc_3164(src, left))
    {
      if (!scan_bsd_timestamp((const gchar **) &src, &left, tm))
        return FALSE;

      *usec = __parse_usec(&src, &left);

      tm->tm_year = determine_year_for_month(tm->tm_mon, &local_time);
    }
  else
    {
      return FALSE;
    }
  *data = src;
  *length = left;
  return TRUE;
}

static inline void
__determine_recv_timezone_offset(LogStamp *const timestamp, glong const recv_timezone_ofs)
{
  if (!log_stamp_is_timezone_set(timestamp))
    timestamp->zone_offset = recv_timezone_ofs;

  if (!log_stamp_is_timezone_set(timestamp))
    timestamp->zone_offset = get_local_timezone_ofs(timestamp->tv_sec);
}

static void
__fixup_hour_in_struct_tm_within_transition_periods(LogStamp *stamp, struct tm *tm, glong recv_timezone_ofs)
{
  /* save the tm_hour value as received from the client */
  gint unnormalized_hour = tm->tm_hour;

  /* NOTE: mktime() returns the time assuming that the timestamp we
   * received was in local time. */

  /* tell cached_mktime() that we have no clue whether Daylight Saving is enabled or not */
  tm->tm_isdst = -1;
  stamp->tv_sec = cached_mktime(tm);

  /* We need to determine the timezone we want to assume the message was
   * received from.  This depends on the recv-time-zone() setting and the
   * the tv_sec value as converted by mktime() above. */
  __determine_recv_timezone_offset(stamp, recv_timezone_ofs);

  /* save the tm_hour as adjusted by mktime() */
  gint normalized_hour = tm->tm_hour;

  /* fix up the tv_sec value by transposing it into the target timezone:
   *
   * First we add the local time zone offset then subtract the target time
   * zone offset.  This is not trivial however, as we have to determine
   * exactly what the local timezone offset is at the current second, as
   * used by mktime(). It is composed of these values:
   *
   *  1) get_local_timezone_ofs()
   *  2) then in transition periods, mktime() will change tm->tm_hour
   *     according to its understanding (e.g.  sprint time, 02:01 is changed
   *     to 03:01), which is an additional factor that needs to be taken care
   *     of.  This is the (normalized_hour - unnormalized_hour) part below
   *
   */
  stamp->tv_sec = stamp->tv_sec
                  /* these two components are the zone offset as used by mktime() */
                  + get_local_timezone_ofs(stamp->tv_sec)
                  - (normalized_hour - unnormalized_hour) * 3600
                  /* this is the zone offset value we want to be */
                  - stamp->zone_offset;
}

static gboolean
log_msg_extract_cisco_timestamp_attributes(LogMessage *self, const guchar **data, gint *length, gint parse_flags)
{
  const guchar *src = *data;
  gint left = *length;

  /* Cisco timestamp extensions, the first '*' indicates that the clock is
   * unsynced, '.' if it is known to be synced */
  if (G_UNLIKELY(src[0] == '*'))
    {
      if (!(parse_flags & LP_NO_PARSE_DATE))
        log_msg_set_value(self, handles.is_synced, "0", 1);
      src++;
      left--;
    }
  else if (G_UNLIKELY(src[0] == '.'))
    {
      if (!(parse_flags & LP_NO_PARSE_DATE))
        log_msg_set_value(self, handles.is_synced, "1", 1);
      src++;
      left--;
    }
  *data = src;
  *length = left;
  return TRUE;
}

static gboolean
log_msg_parse_rfc3164_date_unnormalized(LogMessage *self, const guchar **data, gint *length, guint parse_flags, struct tm *tm)
{
  GTimeVal now;
  const guchar *src = *data;
  gint left = *length;

  cached_g_current_time(&now);

  log_msg_extract_cisco_timestamp_attributes(self, &src, &left, parse_flags);

  /* If the next chars look like a date, then read them as a date. */
  if (__is_iso_stamp((const gchar *)src, left))
    {
      if (!__parse_iso_stamp(&now, self, tm, &src, &left))
        return FALSE;
    }
  else
    {
      glong usec = 0;
      if (!__parse_bsd_timestamp(&src, &left, &now, tm, &usec))
        return FALSE;

      self->timestamps[LM_TS_STAMP].tv_usec = usec;
    }

  /* we might have a closing colon at the end of the timestamp, "Cisco" I am
   * looking at you, skip that as well, so we can reliably detect IPv6
   * addresses as hostnames, which would be using ":" as well. */

  if (*src == ':')
    {
      ++src;
      --left;
    }
  *data = src;
  *length = left;
  return TRUE;
}

static gboolean
log_msg_parse_rfc5424_date_unnormalized(LogMessage *self, const guchar **data, gint *length, guint parse_flags, struct tm *tm)
{
  GTimeVal now;
  const guchar *src = *data;
  gint left = *length;

  cached_g_current_time(&now);

  /* NILVALUE */
  if (G_UNLIKELY(left >= 1 && src[0] == '-'))
    {
      self->timestamps[LM_TS_STAMP] = self->timestamps[LM_TS_RECVD];
      left--;
      src++;
    }
  else if (!__parse_iso_stamp(&now, self, tm, &src, &left))
    return FALSE;

  *data = src;
  *length = left;
  return TRUE;
}

static gboolean
log_msg_parse_date_unnormalized(LogMessage *self, const guchar **data, gint *length, guint parse_flags, struct tm *tm)
{
  if ((parse_flags & LP_SYSLOG_PROTOCOL) == 0)
    return log_msg_parse_rfc3164_date_unnormalized(self, data, length, parse_flags, tm);
  else
    return log_msg_parse_rfc5424_date_unnormalized(self, data, length, parse_flags, tm);
}

static gboolean
log_msg_parse_date(LogMessage *self, const guchar **data, gint *length, guint parse_flags, glong recv_timezone_ofs)
{
  struct tm tm;

  LogStamp *stamp = &self->timestamps[LM_TS_STAMP];
  stamp->tv_sec = -1;
  stamp->tv_usec = 0;
  stamp->zone_offset = -1;

  if (!log_msg_parse_date_unnormalized(self, data, length, parse_flags, &tm))
    {
      *stamp = self->timestamps[LM_TS_RECVD];
      return FALSE;
    }

  if (parse_flags & LP_NO_PARSE_DATE)
    {
      *stamp = self->timestamps[LM_TS_RECVD];
    }
  else
    {
      __fixup_hour_in_struct_tm_within_transition_periods(stamp, &tm, recv_timezone_ofs);
    }

  return TRUE;
}

static gboolean
log_msg_parse_version(LogMessage *self, const guchar **data, gint *length)
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
      src++;
      left--;
    }
  if (version != 1)
    return FALSE;

  *data = src;
  *length = left;
  return TRUE;
}

static void
log_msg_parse_legacy_program_name(LogMessage *self, const guchar **data, gint *length, guint flags)
{
  /* the data pointer will not change */
  const guchar *src, *prog_start;
  gint left;

  src = *data;
  left = *length;
  prog_start = src;
  while (left && *src != ' ' && *src != '[' && *src != ':')
    {
      src++;
      left--;
    }
  log_msg_set_value(self, LM_V_PROGRAM, (gchar *) prog_start, src - prog_start);
  if (left > 0 && *src == '[')
    {
      const guchar *pid_start = src + 1;
      while (left && *src != ' ' && *src != ']' && *src != ':')
        {
          src++;
          left--;
        }
      if (left)
        {
          log_msg_set_value(self, LM_V_PID, (gchar *) pid_start, src - pid_start);
        }
      if (left > 0 && *src == ']')
        {
          src++;
          left--;
        }
    }
  if (left > 0 && *src == ':')
    {
      src++;
      left--;
    }
  if (left > 0 && *src == ' ')
    {
      src++;
      left--;
    }
  if ((flags & LP_STORE_LEGACY_MSGHDR))
    {
      log_msg_set_value(self, LM_V_LEGACY_MSGHDR, (gchar *) *data, *length - left);
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
              invalid_chars[i >> 8] |= 1 << (i % 8);
            }
        }
      invalid_chars[0] |= 0x1;
    }
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
log_msg_parse_hostname(LogMessage *self, const guchar **data, gint *length,
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

      if (G_UNLIKELY((flags & LP_CHECK_HOSTNAME) && (invalid_chars[((guint) *src) >> 8] & (1 << (((guint) *src) % 8)))))
        {
          break;
        }
      hostname_buf[dst++] = *src;
      src++;
      left--;
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
    }

  if (*hostname_len > 255)
    *hostname_len = 255;

  *data = src;
  *length = left;
}


static inline void
sd_step(const guchar **data, gint *left)
{
  (*data)++;
  (*left)--;
}

/**
 * log_msg_parse:
 * @self: LogMessage instance to store parsed information into
 * @data: message
 * @length: length of the message pointed to by @data
 * @flags: value affecting how the message is parsed (bits from LP_*)
 *
 * Parse an http://www.syslog.cc/ietf/drafts/draft-ietf-syslog-protocol-23.txt formatted log
 * message for structured data elements and store the parsed information
 * in @self.values and dup the SD string. Parsing is affected by the bits set @flags argument.
 **/
static gboolean
log_msg_parse_sd(LogMessage *self, const guchar **data, gint *length, const MsgFormatOptions *options)
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
   */

  gboolean ret = FALSE;
  const guchar *src = *data;
  /* ASCII string */
  gchar sd_id_name[33];
  gsize sd_id_len;
  gchar sd_param_name[33];

  /* UTF-8 string */
  gchar sd_param_value[options->sdata_param_value_max + 1];
  gsize sd_param_value_len;
  gchar sd_value_name[66];

  guint open_sd = 0;
  gint left = *length, pos;

  if (left && src[0] == '-')
    {
      /* Nothing to do here */
      src++;
      left--;
    }
  else if (left && src[0] == '[')
    {
      sd_step(&src, &left);
      open_sd++;
      do
        {
          if (!isascii(*src) || *src == '=' || *src == ' ' || *src == ']' || *src == '"')
            goto error;
          /* read sd_id */
          pos = 0;
          while (left && *src != ' ' && *src != ']')
            {
              /* the sd_id_name is max 32, the other chars are only stored in the self->sd_str*/
              if (pos < sizeof(sd_id_name) - 1)
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
              sd_step(&src, &left);
            }

          if (pos == 0)
            goto error;

          sd_id_name[pos] = 0;
          sd_id_len = pos;
          strcpy(sd_value_name, logmsg_sd_prefix);
          /* this strcat is safe, as sd_id_name is at most 32 chars */
          strncpy(sd_value_name + logmsg_sd_prefix_len, sd_id_name, sizeof(sd_value_name) - logmsg_sd_prefix_len);
          if (*src == ']')
            {
              log_msg_set_value_by_name(self, sd_value_name, "", 0);
            }
          else
            {
              sd_value_name[logmsg_sd_prefix_len + pos] = '.';
            }

          /* read sd-element */
          while (left && *src != ']')
            {
              if (left && *src == ' ') /* skip the ' ' before the parameter name */
                sd_step(&src, &left);
              else
                goto error;

              if (!isascii(*src) || *src == '=' || *src == ' ' || *src == ']' || *src == '"')
                goto error;

              /* read sd-param */
              pos = 0;
              while (left && *src != '=')
                {
                  if (pos < sizeof(sd_param_name) - 1)
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
                  sd_step(&src, &left);
                }
              sd_param_name[pos] = 0;
              strncpy(&sd_value_name[logmsg_sd_prefix_len + 1 + sd_id_len], sd_param_name,
                      sizeof(sd_value_name) - logmsg_sd_prefix_len - 1 - sd_id_len);

              if (left && *src == '=')
                sd_step(&src, &left);
              else
                goto error;

              /* read sd-param-value */

              if (left && *src == '"')
                {
                  gboolean quote = FALSE;
                  /* opening quote */
                  sd_step(&src, &left);
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
                              sd_step(&src, &left);
                              goto error;
                            }
                          if (pos < sizeof(sd_param_value) - 1)
                            {
                              sd_param_value[pos] = *src;
                              pos++;
                            }
                          quote = FALSE;
                        }
                      sd_step(&src, &left);
                    }
                  sd_param_value[pos] = 0;
                  sd_param_value_len = pos;

                  if (left && *src == '"')/* closing quote */
                    sd_step(&src, &left);
                  else
                    goto error;
                }
              else
                {
                  goto error;
                }

              log_msg_set_value_by_name(self, sd_value_name, sd_param_value, sd_param_value_len);
            }

          if (left && *src == ']')
            {
              sd_step(&src, &left);
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
              sd_step(&src, &left);
              open_sd++;
            }
        }
      while (left && open_sd != 0);
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


/**
 * log_msg_parse_legacy:
 * @self: LogMessage instance to store parsed information into
 * @data: message
 * @length: length of the message pointed to by @data
 * @flags: value affecting how the message is parsed (bits from LP_*)
 *
 * Parse an RFC3164 formatted log message and store the parsed information
 * in @self. Parsing is affected by the bits set @flags argument.
 **/
static gboolean
log_msg_parse_legacy(const MsgFormatOptions *parse_options,
                     const guchar *data, gint length,
                     LogMessage *self, gint *position)
{
  const guchar *src;
  gint left;
  GTimeVal now;

  src = (const guchar *) data;
  left = length;

  if (!log_msg_parse_pri(self, &src, &left, parse_options->flags, parse_options->default_pri))
    {
      goto error;
    }

  log_msg_parse_seq(self, &src, &left);
  log_msg_parse_skip_chars(self, &src, &left, " ", -1);
  cached_g_current_time(&now);
  if (log_msg_parse_date(self, &src, &left, parse_options->flags & ~LP_SYSLOG_PROTOCOL,
                         time_zone_info_get_offset(parse_options->recv_time_zone_info, (time_t)now.tv_sec)))
    {
      /* Expected format: hostname program[pid]: */
      /* Possibly: Message forwarded from hostname: ... */
      const guchar *hostname_start = NULL;
      int hostname_len = 0;

      log_msg_parse_skip_chars(self, &src, &left, " ", -1);

      /* Detect funny AIX syslogd forwarded message. */
      if (G_UNLIKELY(left >= (sizeof(aix_fwd_string) - 1) &&
                     !memcmp(src, aix_fwd_string, sizeof(aix_fwd_string) - 1)))
        {
          src += sizeof(aix_fwd_string) - 1;
          left -= sizeof(aix_fwd_string) - 1;
          hostname_start = src;
          hostname_len = log_msg_parse_skip_chars_until(self, &src, &left, ":");
          log_msg_parse_skip_chars(self, &src, &left, " :", -1);
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
              log_msg_parse_hostname(self, &src, &left, &hostname_start, &hostname_len, parse_options->flags,
                                     parse_options->bad_hostname);

              /* Skip whitespace. */
              log_msg_parse_skip_chars(self, &src, &left, " ", -1);
            }

          /* Try to extract a program name */
          log_msg_parse_legacy_program_name(self, &src, &left, parse_options->flags);
        }

      /* If we did manage to find a hostname, store it. */
      if (hostname_start)
        {
          log_msg_set_value(self, LM_V_HOST, (gchar *) hostname_start, hostname_len);
        }
    }
  else
    {
      /* no timestamp, format is expected to be "program[pid] message" */
      /* Different format */

      /* A kernel message? Use 'kernel' as the program name. */
      if ((self->flags & LF_INTERNAL) == 0 && ((self->pri & LOG_FACMASK) == LOG_KERN &&
                                               (self->flags & LF_LOCAL) != 0))
        {
          log_msg_set_value(self, LM_V_PROGRAM, "kernel", 6);
        }
      /* No, not a kernel message. */
      else
        {
          /* Capture the program name */
          log_msg_parse_legacy_program_name(self, &src, &left, parse_options->flags);
        }
      self->timestamps[LM_TS_STAMP] = self->timestamps[LM_TS_RECVD];
    }

  if (parse_options->flags & LP_SANITIZE_UTF8 && !g_utf8_validate((gchar *) src, left, NULL))
    {
      GString sanitized_message;
      gchar buf[left * 6 + 1];

      /* avoid GString allocation */
      sanitized_message.str = buf;
      sanitized_message.len = 0;
      sanitized_message.allocated_len = sizeof(buf);

      append_unsafe_utf8_as_escaped_binary(&sanitized_message, (const gchar *) src, left, NULL);

      /* MUST NEVER BE REALLOCATED */
      g_assert(sanitized_message.str == buf);
      log_msg_set_value(self, LM_V_MESSAGE, sanitized_message.str, sanitized_message.len);
      self->flags |= LF_UTF8;
    }
  else
    {
      log_msg_set_value(self, LM_V_MESSAGE, (gchar *) src, left);

      /* we don't need revalidation if sanitize already said it was valid utf8 */
      if ((parse_options->flags & LP_VALIDATE_UTF8) &&
          ((parse_options->flags & LP_SANITIZE_UTF8) == 0) &&
          g_utf8_validate((gchar *) src, left, NULL))
        self->flags |= LF_UTF8;
    }

  return TRUE;
error:
  *position = src - data;
  return FALSE;
}

/**
 * log_msg_parse_syslog_proto:
 *
 * Parse a message according to the latest syslog-protocol drafts.
 **/
static gboolean
log_msg_parse_syslog_proto(const MsgFormatOptions *parse_options, const guchar *data, gint length, LogMessage *self,
                           gint *position)
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


  if (!log_msg_parse_pri(self, &src, &left, parse_options->flags, parse_options->default_pri) ||
      !log_msg_parse_version(self, &src, &left))
    {
      return log_msg_parse_legacy(parse_options, data, length, self, position);
    }

  if (!log_msg_parse_skip_space(self, &src, &left))
    {
      goto error;
    }

  /* ISO time format */
  if (!log_msg_parse_date(self, &src, &left, parse_options->flags,
                          time_zone_info_get_offset(parse_options->recv_time_zone_info, time(NULL))))
    goto error;

  if (!log_msg_parse_skip_space(self, &src, &left))
    goto error;

  /* hostname 255 ascii */
  log_msg_parse_hostname(self, &src, &left, &hostname_start, &hostname_len, parse_options->flags, NULL);
  if (!log_msg_parse_skip_space(self, &src, &left))
    {
      src++;
      goto error;
    }
  /* If we did manage to find a hostname, store it. */
  if (hostname_start && hostname_len == 1 && *hostname_start == '-')
    ;
  else if (hostname_start)
    {
      log_msg_set_value(self, LM_V_HOST, (gchar *) hostname_start, hostname_len);
    }

  /* application name 48 ascii*/
  log_msg_parse_column(self, LM_V_PROGRAM, &src, &left, 48);
  if (!log_msg_parse_skip_space(self, &src, &left))
    goto error;

  /* process id 128 ascii */
  log_msg_parse_column(self, LM_V_PID, &src, &left, 128);
  if (!log_msg_parse_skip_space(self, &src, &left))
    goto error;

  /* message id 32 ascii */
  log_msg_parse_column(self, LM_V_MSGID, &src, &left, 32);
  if (!log_msg_parse_skip_space(self, &src, &left))
    goto error;

  /* structured data part */
  if (!log_msg_parse_sd(self, &src, &left, parse_options))
    goto error;

  /* checking if there are remaining data in log message */
  if (left == 0)
    {
      /*  no message, this is valid */
      return TRUE;
    }

  /* optional part of the log message [SP MSG] */
  if (!log_msg_parse_skip_space(self, &src, &left))
    {
      goto error;
    }

  if (left >= 3 && memcmp(src, "\xEF\xBB\xBF", 3) == 0)
    {
      /* we have a BOM, this is UTF8 */
      self->flags |= LF_UTF8;
      src += 3;
      left -= 3;
    }
  else if ((parse_options->flags & LP_VALIDATE_UTF8) && g_utf8_validate((gchar *) src, left, NULL))
    {
      self->flags |= LF_UTF8;
    }
  log_msg_set_value(self, LM_V_MESSAGE, (gchar *) src, left);
  return TRUE;
error:
  *position = src - data;
  return FALSE;
}


void
syslog_format_handler(const MsgFormatOptions *parse_options,
                      const guchar *data, gsize length,
                      LogMessage *self)
{
  gboolean success;
  gint problem_position = 0;
  gchar *p;

  while (length > 0 && (data[length - 1] == '\n' || data[length - 1] == '\0'))
    length--;

  if (parse_options->flags & LP_STORE_RAW_MESSAGE)
    log_msg_set_value(self, handles.raw_message, (gchar *) data, length);

  if (parse_options->flags & LP_NOPARSE)
    {
      log_msg_set_value(self, LM_V_MESSAGE, (gchar *) data, length);
      self->pri = parse_options->default_pri;
      return;
    }

  if (parse_options->flags & LP_ASSUME_UTF8)
    self->flags |= LF_UTF8;

  if (parse_options->flags & LP_LOCAL)
    self->flags |= LF_LOCAL;

  self->initial_parse = TRUE;
  if (parse_options->flags & LP_SYSLOG_PROTOCOL)
    success = log_msg_parse_syslog_proto(parse_options, data, length, self, &problem_position);
  else
    success = log_msg_parse_legacy(parse_options, data, length, self, &problem_position);
  self->initial_parse = FALSE;

  if (G_UNLIKELY(!success))
    {
      msg_format_inject_parse_error(self, data, length, problem_position);
      return;
    }

  if (G_UNLIKELY(parse_options->flags & LP_NO_MULTI_LINE))
    {
      gssize msglen;
      gchar *msg;

      p = msg = (gchar *) log_msg_get_value(self, LM_V_MESSAGE, &msglen);
      while ((p = find_cr_or_lf(p, msg + msglen - p)))
        {
          *p = ' ';
          p++;
        }

    }
}

void
syslog_format_init(void)
{
  if (!handles.initialized)
    {
      handles.is_synced = log_msg_get_value_handle(".SDATA.timeQuality.isSynced");
      handles.cisco_seqid = log_msg_get_value_handle(".SDATA.meta.sequenceId");
      handles.raw_message = log_msg_get_value_handle("RAWMSG");
      handles.initialized = TRUE;
    }

  _init_parse_hostname_invalid_chars();
}
