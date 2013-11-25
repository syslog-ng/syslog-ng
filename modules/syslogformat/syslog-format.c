/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
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
#include "logmsg.h"
#include "messages.h"
#include "timeutils.h"
#include "misc.h"
#include "cfg.h"
#include "str-format.h"

#include <regex.h>
#include <ctype.h>
#include <string.h>

/*
 * Used in log_msg_parse_date(). Need to differentiate because Tru64's strptime
 * works differently than the rest of the supported systems.
 */
#if defined(__digital__) && defined(__osf__)
#define STRPTIME_ISOFORMAT "%Y-%m-%dT%H:%M:%S"
#else
#define STRPTIME_ISOFORMAT "%Y-%m-%d T%H:%M:%S"
#endif

static const char aix_fwd_string[] = "Message forwarded from ";
static const char repeat_msg_string[] = "last message repeated";
static NVHandle is_synced;
static NVHandle cisco_seqid;

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

  while (max_len && left && strchr(chars, *src))
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

  while (left && strchr(delims, *src) == 0)
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

  log_msg_set_value(self, cisco_seqid, (gchar *) *data, *length - left - 1);

  *data = src;
  *length = left;
  return TRUE;
}

/* FIXME: this function should really be exploded to a lot of smaller functions... (Bazsi) */
static gboolean
log_msg_parse_date(LogMessage *self, const guchar **data, gint *length, guint parse_flags, glong assume_timezone)
{
  const guchar *src = *data;
  gint left = *length;
  GTimeVal now;
  struct tm tm;
  gint unnormalized_hour;

  cached_g_current_time(&now);

  if ((parse_flags & LP_SYSLOG_PROTOCOL) == 0)
    {
      /* Cisco timestamp extensions, the first '*' indicates that the clock is
       * unsynced, '.' if it is known to be synced */
      if (G_UNLIKELY(src[0] == '*'))
        {
          log_msg_set_value(self, is_synced, "0", 1);
          src++;
          left--;
        }
      else if (G_UNLIKELY(src[0] == '.'))
        {
          log_msg_set_value(self, is_synced, "1", 1);
          src++;
          left--;
        }
    }
  /* If the next chars look like a date, then read them as a date. */
  if (left >= 19 && src[4] == '-' && src[7] == '-' && src[10] == 'T' && src[13] == ':' && src[16] == ':')
    {
      /* RFC3339 timestamp, expected format: YYYY-MM-DDTHH:MM:SS[.frac]<+/->ZZ:ZZ */
      gint hours, mins;

      self->timestamps[LM_TS_STAMP].tv_usec = 0;

      /* NOTE: we initialize various unportable fields in tm using a
       * localtime call, as the value of tm_gmtoff does matter but it does
       * not exist on all platforms and 0 initializing it causes trouble on
       * time-zone barriers */

      cached_localtime(&now.tv_sec, &tm);
      if (!scan_iso_timestamp((const gchar **) &src, &left, &tm))
        {
          goto error;
        }

      self->timestamps[LM_TS_STAMP].tv_usec = 0;
      if (left > 0 && *src == '.')
        {
          gulong frac = 0;
          gint div = 1;
          /* process second fractions */

          src++;
          left--;
          while (left > 0 && div < 10e5 && isdigit(*src))
            {
              frac = 10 * frac + (*src) - '0';
              div = div * 10;
              src++;
              left--;
            }
          while (isdigit(*src))
            {
              src++;
              left--;
            }
          self->timestamps[LM_TS_STAMP].tv_usec = frac * (1000000 / div);
        }

      if (left > 0 && *src == 'Z')
        {
          /* Z is special, it means UTC */
          self->timestamps[LM_TS_STAMP].zone_offset = 0;
          src++;
          left--;
        }
      else if (left >= 5 && (*src == '+' || *src == '-') &&
          isdigit(*(src+1)) && isdigit(*(src+2)) && *(src+3) == ':' && isdigit(*(src+4)) && isdigit(*(src+5)) && !isdigit(*(src+6)))
        {
          /* timezone offset */
          gint sign = *src == '-' ? -1 : 1;

          hours = (*(src+1) - '0') * 10 + *(src+2) - '0';
          mins = (*(src+4) - '0') * 10 + *(src+5) - '0';
          self->timestamps[LM_TS_STAMP].zone_offset = sign * (hours * 3600 + mins * 60);
          src += 6;
          left -= 6;
        }
      /* we convert it to UTC */

      tm.tm_isdst = -1;
      unnormalized_hour = tm.tm_hour;
      self->timestamps[LM_TS_STAMP].tv_sec = cached_mktime(&tm);
    }
  else if ((parse_flags & LP_SYSLOG_PROTOCOL) == 0)
    {
      if (left >= 21 && src[3] == ' ' && src[6] == ' ' && src[11] == ' ' && src[14] == ':' && src[17] == ':' && (src[20] == ':' || src[20] == ' ') &&
          (isdigit(src[7]) && isdigit(src[8]) && isdigit(src[9]) && isdigit(src[10])))
        {
          /* PIX timestamp, expected format: MMM DD YYYY HH:MM:SS: */
          /* ASA timestamp, expected format: MMM DD YYYY HH:MM:SS */

          cached_localtime(&now.tv_sec, &tm);
          if (!scan_pix_timestamp((const gchar **) &src, &left, &tm))
            goto error;

          if (*src == ':')
            {
              src++;
              left--;
            }

          tm.tm_isdst = -1;

          /* NOTE: no timezone information in the message, assume it is local time */
          unnormalized_hour = tm.tm_hour;
          self->timestamps[LM_TS_STAMP].tv_sec = cached_mktime(&tm);
          self->timestamps[LM_TS_STAMP].tv_usec = 0;
        }
      else if (left >= 21 && src[3] == ' ' && src[6] == ' ' && src[9] == ':' && src[12] == ':' && src[15] == ' ' &&
               isdigit(src[16]) && isdigit(src[17]) && isdigit(src[18]) && isdigit(src[19]) && isspace(src[20]))
        {
          /* LinkSys timestamp, expected format: MMM DD HH:MM:SS YYYY */

          cached_localtime(&now.tv_sec, &tm);
          if (!scan_linksys_timestamp((const gchar **) &src, &left, &tm))
            goto error;
          tm.tm_isdst = -1;

          /* NOTE: no timezone information in the message, assume it is local time */
          unnormalized_hour = tm.tm_hour;
          self->timestamps[LM_TS_STAMP].tv_sec = cached_mktime(&tm);
          self->timestamps[LM_TS_STAMP].tv_usec = 0;

        }
      else if (left >= 15 && src[3] == ' ' && src[6] == ' ' && src[9] == ':' && src[12] == ':')
        {
          /* RFC 3164 timestamp, expected format: MMM DD HH:MM:SS ... */
          struct tm nowtm;
          glong usec = 0;

          cached_localtime(&now.tv_sec, &nowtm);
          tm = nowtm;
          if (!scan_bsd_timestamp((const gchar **) &src, &left, &tm))
            goto error;

          if (left > 0 && src[0] == '.')
            {
              gulong frac = 0;
              gint div = 1;
              gint i = 1;

              /* gee, funny Cisco extension, BSD timestamp with fraction of second support */

              while (i < left && div < 10e5 && isdigit(src[i]))
                {
                  frac = 10 * frac + (src[i]) - '0';
                  div = div * 10;
                  i++;
                }
              while (i < left && isdigit(src[i]))
                i++;

              usec = frac * (1000000 / div);
              left -= i;
              src += i;
            }

          /* detect if the message is coming from last year. If its
           * month is at least one larger than the current month. This
           * handles both clocks that are in the future, or in the
           * past:
           *   in January we receive a message from December (past) => last year
           *   in January we receive a message from February (future) => same year
           *   in December we receive a message from January (future) => next year
           */
          if (tm.tm_mon > nowtm.tm_mon + 1)
            tm.tm_year--;
          if (tm.tm_mon < nowtm.tm_mon - 1)
            tm.tm_year++;

          /* NOTE: no timezone information in the message, assume it is local time */
          unnormalized_hour = tm.tm_hour;
          self->timestamps[LM_TS_STAMP].tv_sec = cached_mktime(&tm);
          self->timestamps[LM_TS_STAMP].tv_usec = usec;
        }
      else
        {
          goto error;
        }
    }
  else
    {
      if (left >= 1 && src[0] == '-')
        {
          /* NILVALUE */
          self->timestamps[LM_TS_STAMP] = self->timestamps[LM_TS_RECVD];
          *length = --left;
          *data = ++src;
          return TRUE;
        }
      else
        return FALSE;
    }

  /* NOTE: mktime() returns the time assuming that the timestamp we
   * received was in local time. This is not true, as there's a
   * zone_offset in the timestamp as well. We need to adjust this offset
   * by adding the local timezone offset at the specific time to get UTC,
   * which means that tv_sec becomes as if tm was in the 00:00 timezone.
   * Also we have to take into account that at the zone barriers an hour
   * might be skipped or played twice this is what the
   * (tm.tm_hour - * unnormalized_hour) part fixes up. */

  if (self->timestamps[LM_TS_STAMP].zone_offset == -1)
    {
      self->timestamps[LM_TS_STAMP].zone_offset = assume_timezone;
    }
  if (self->timestamps[LM_TS_STAMP].zone_offset == -1)
    {
      self->timestamps[LM_TS_STAMP].zone_offset = get_local_timezone_ofs(self->timestamps[LM_TS_STAMP].tv_sec);
    }
  self->timestamps[LM_TS_STAMP].tv_sec = self->timestamps[LM_TS_STAMP].tv_sec +
                                              get_local_timezone_ofs(self->timestamps[LM_TS_STAMP].tv_sec) -
                                              (tm.tm_hour - unnormalized_hour) * 3600 - self->timestamps[LM_TS_STAMP].zone_offset;

  *data = src;
  *length = left;
  return TRUE;
 error:
  /* no recognizable timestamp, use current time */

  self->timestamps[LM_TS_STAMP] = self->timestamps[LM_TS_RECVD];
  return FALSE;
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
      self->flags |= LF_LEGACY_MSGHDR;
    }
  *data = src;
  *length = left;
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
  static guint8 invalid_chars[32];

  src = *data;
  left = *length;

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

  /* If we haven't already found the original hostname,
     look for it now. */

  oldsrc = src;
  oldleft = left;

  while (left && *src != ' ' && *src != ':' && *src != '[' && dst < sizeof(hostname_buf) - 1)
    {
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
sd_step_and_store(LogMessage *self, const guchar **data, gint *left)
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
  NVHandle handle;

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
      sd_step_and_store(self, &src, &left);
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
              sd_step_and_store(self, &src, &left);
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
              /* Standalone sdata */
              handle = log_msg_get_value_handle(sd_value_name);

              log_msg_set_value(self, handle, "", 0);
            }
          else
            {
              sd_value_name[logmsg_sd_prefix_len + pos] = '.';
            }

          /* read sd-element */
          while (left && *src != ']')
            {
              if (left && *src == ' ') /* skip the ' ' before the parameter name */
                sd_step_and_store(self, &src, &left);
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
                  sd_step_and_store(self, &src, &left);
                }
              sd_param_name[pos] = 0;
              strncpy(&sd_value_name[logmsg_sd_prefix_len + 1 + sd_id_len], sd_param_name, sizeof(sd_value_name) - logmsg_sd_prefix_len - 1 - sd_id_len);

              if (left && *src == '=')
                sd_step_and_store(self, &src, &left);
              else
                goto error;

              /* read sd-param-value */

              if (left && *src == '"')
                {
                  gboolean quote = FALSE;
                  /* opening quote */
                  sd_step_and_store(self, &src, &left);
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
                             goto error;
                           }
                         if (pos < sizeof(sd_param_value) - 1)
                           {
                             sd_param_value[pos] = *src;
                             pos++;
                           }
                         quote = FALSE;
                       }
                      sd_step_and_store(self, &src, &left);
                    }
                  sd_param_value[pos] = 0;
                  sd_param_value_len = pos;

                  if (left && *src == '"')/* closing quote */
                    sd_step_and_store(self, &src, &left);
                  else
                    goto error;
                }
              else
                {
                  goto error;
                }

              handle = log_msg_get_value_handle(sd_value_name);

              log_msg_set_value(self, handle, sd_param_value, sd_param_value_len);
            }

          if (left && *src == ']')
            {
              sd_step_and_store(self, &src, &left);
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
              sd_step_and_store(self, &src, &left);
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
                     LogMessage *self)
{
  const guchar *src;
  gint left;
  GTimeVal now;

  src = (const guchar *) data;
  left = length;

  if (!log_msg_parse_pri(self, &src, &left, parse_options->flags, parse_options->default_pri))
    {
      return FALSE;
    }

  log_msg_parse_seq(self, &src, &left);
  log_msg_parse_skip_chars(self, &src, &left, " ", -1);
  cached_g_current_time(&now);
  if (log_msg_parse_date(self, &src, &left, parse_options->flags & ~LP_SYSLOG_PROTOCOL, time_zone_info_get_offset(parse_options->recv_time_zone_info, now.tv_sec)))
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
              log_msg_parse_hostname(self, &src, &left, &hostname_start, &hostname_len, parse_options->flags, parse_options->bad_hostname);

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

  log_msg_set_value(self, LM_V_MESSAGE, (gchar *) src, left);
  if ((parse_options->flags & LP_VALIDATE_UTF8) && g_utf8_validate((gchar *) src, left, NULL))
    self->flags |= LF_UTF8;

  return TRUE;
}

/**
 * log_msg_parse_syslog_proto:
 *
 * Parse a message according to the latest syslog-protocol drafts.
 **/
static gboolean
log_msg_parse_syslog_proto(const MsgFormatOptions *parse_options, const guchar *data, gint length, LogMessage *self)
{
  /**
   *	SYSLOG-MSG      = HEADER SP STRUCTURED-DATA [SP MSG]
   *	HEADER          = PRI VERSION SP TIMESTAMP SP HOSTNAME
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
      return log_msg_parse_legacy(parse_options, data, length, self);
    }

  if (!log_msg_parse_skip_space(self, &src, &left))
    return FALSE;

  /* ISO time format */
  if (!log_msg_parse_date(self, &src, &left, parse_options->flags, time_zone_info_get_offset(parse_options->recv_time_zone_info, time(NULL))))
    return FALSE;

  if (!log_msg_parse_skip_space(self, &src, &left))
    return FALSE;

  /* hostname 255 ascii */
  log_msg_parse_hostname(self, &src, &left, &hostname_start, &hostname_len, parse_options->flags, NULL);
  if (!log_msg_parse_skip_space(self, &src, &left))
    return FALSE;

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
    return FALSE;

  /* process id 128 ascii */
  log_msg_parse_column(self, LM_V_PID, &src, &left, 128);
  if (!log_msg_parse_skip_space(self, &src, &left))
    return FALSE;

  /* message id 32 ascii */
  log_msg_parse_column(self, LM_V_MSGID, &src, &left, 32);
  if (!log_msg_parse_skip_space(self, &src, &left))
    return FALSE;

  /* structured data part */
  if (!log_msg_parse_sd(self, &src, &left, parse_options))
    return FALSE;

  /* checking if there are remaining data in log message */
  if (left == 0)
    {
      /*  no message, this is valid */
      return TRUE;
    }

  /* optional part of the log message [SP MSG] */
  if (!log_msg_parse_skip_space(self, &src, &left))
    {
      return FALSE;
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
}


void
syslog_format_handler(const MsgFormatOptions *parse_options,
                      const guchar *data, gsize length,
                      LogMessage *self)
{
  gboolean success;
  gchar *p;

  while (length > 0 && (data[length - 1] == '\n' || data[length - 1] == '\0'))
    length--;

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
    success = log_msg_parse_syslog_proto(parse_options, data, length, self);
  else
    success = log_msg_parse_legacy(parse_options, data, length, self);
  self->initial_parse = FALSE;

  if (G_UNLIKELY(!success))
    {
      msg_format_inject_parse_error(self, data, length);
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
  static gboolean handles_initialized = FALSE;

  if (!handles_initialized)
    {
      is_synced = log_msg_get_value_handle(".SDATA.timeQuality.isSynced");
      cisco_seqid = log_msg_get_value_handle(".SDATA.meta.sequenceId");
      handles_initialized = TRUE;
    }
}
