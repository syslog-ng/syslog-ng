/*
 * Copyright (c) 2002-2014 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2013-2014 Benke Tibor
 * Copyright (c) 1998-2014 Balázs Scheidler
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

#include "messages.h"
#include "timeutils.h"
#include "misc.h"
#include "cfg.h"
#include "str-format.h"
#include "dateparse.h"

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

static gboolean handles_initialized = FALSE;
static NVHandle is_synced;

static void
init_is_synced_handle()
{
  if (!handles_initialized)
    {
      is_synced = log_msg_get_value_handle(".SDATA.timeQuality.isSynced");
      handles_initialized = TRUE;
    }
}

/* FIXME: this function should really be exploded to a lot of smaller functions... (Bazsi) */
/*
 * log_msg_parse_date
 *
 * Parse a date string, and sets the timestamps field in the logmsg.
 *
 * @self: LogMessage, in which the parsed timestamp is stored
 * @data: the input string to parse
 * @length: the length of @data
 * @parse_flags: flags from msg-format.h
 * @assume_timezone: if -1, the function automagically determines the timezone and offset based on the current time.
 *                    If it's  a positive number (UNIX epoch), the timezone and offset will be calculated at this time.
 *
 * The function returns TRUE when it managed to parse the date successfully, else FALSE.
 *
 * @name:   the name of the tag
 *
 */
gboolean
log_msg_parse_date(LogMessage *self, const guchar **data, gint *length, guint parse_flags, glong assume_timezone)
{
  const guchar *src = *data;
  gint left = *length;
  GTimeVal now;
  struct tm tm;
  gint unnormalized_hour;

  init_is_synced_handle();
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
