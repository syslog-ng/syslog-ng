/*
 * Copyright (c) 2002-2008 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
  
#include "templates.h"
#include "messages.h"
#include "logmsg.h"
#include "syslog-names.h"
#include "messages.h"
#include "misc.h"
#include "filter.h"
#include "gsocket.h"

#include <time.h>
#include <string.h>

/* macro IDs */
enum
{
  M_NONE,

  M_FACILITY,
  M_FACILITY_NUM,
  M_LEVEL,
  M_LEVEL_NUM,
  M_TAG,
  M_BSDTAG,
  M_PRI,

  M_DATE,
  M_FULLDATE,
  M_ISODATE,
  M_STAMP,
  M_YEAR,
  M_MONTH,
  M_DAY,
  M_HOUR,
  M_MIN,
  M_SEC,
  M_WEEKDAY,
  M_WEEK,
  M_TZOFFSET,
  M_TZ,
  M_UNIXTIME,

  M_DATE_RECVD,
  M_FULLDATE_RECVD,
  M_ISODATE_RECVD,
  M_STAMP_RECVD,
  M_YEAR_RECVD,
  M_MONTH_RECVD,
  M_DAY_RECVD,
  M_HOUR_RECVD,
  M_MIN_RECVD,
  M_SEC_RECVD,
  M_WEEKDAY_RECVD,
  M_WEEK_RECVD,
  M_TZOFFSET_RECVD,
  M_TZ_RECVD,
  M_UNIXTIME_RECVD,

  M_DATE_STAMP,
  M_FULLDATE_STAMP,
  M_ISODATE_STAMP,
  M_STAMP_STAMP,
  M_YEAR_STAMP,
  M_MONTH_STAMP,
  M_DAY_STAMP,
  M_HOUR_STAMP,
  M_MIN_STAMP,
  M_SEC_STAMP,
  M_WEEKDAY_STAMP,
  M_WEEK_STAMP,
  M_TZOFFSET_STAMP,
  M_TZ_STAMP,
  M_UNIXTIME_STAMP,

  M_FULLHOST,
  M_HOST,
  M_SDATA,
  M_FULLHOST_FROM,

  M_MSGHDR,
  M_MSGONLY,
  M_MESSAGE,
  M_SOURCE_IP,
  M_SEQNUM,
  M_MAX,

  M_MATCH_REF_OFS=256
};

#define M_TIME_MACROS 15

struct macro_def
{
  char *name;
  int id;
}
macros[] =
{
        { "FACILITY", M_FACILITY },
        { "FACILITY_NUM", M_FACILITY_NUM },
        { "PRIORITY", M_LEVEL },
        { "LEVEL", M_LEVEL },
        { "LEVEL_NUM", M_LEVEL_NUM },
        { "TAG", M_TAG },
        { "BSDTAG", M_BSDTAG },
        { "PRI", M_PRI },

        { "DATE", M_DATE },
        { "FULLDATE", M_FULLDATE },
        { "ISODATE", M_ISODATE },
        { "STAMP", M_STAMP },
        { "YEAR", M_YEAR },
        { "MONTH", M_MONTH },
        { "DAY", M_DAY },
        { "HOUR", M_HOUR },
        { "MIN", M_MIN },
        { "SEC", M_SEC },
        { "WEEKDAY", M_WEEKDAY },
        { "WEEK", M_WEEK },
        { "UNIXTIME", M_UNIXTIME },
        { "TZOFFSET", M_TZOFFSET },
        { "TZ", M_TZ },

        { "R_DATE", M_DATE_RECVD },
        { "R_FULLDATE", M_FULLDATE_RECVD },
        { "R_ISODATE", M_ISODATE_RECVD },
        { "R_STAMP", M_STAMP_RECVD },
        { "R_YEAR", M_YEAR_RECVD },
        { "R_MONTH", M_MONTH_RECVD },
        { "R_DAY", M_DAY_RECVD },
        { "R_HOUR", M_HOUR_RECVD },
        { "R_MIN", M_MIN_RECVD },
        { "R_SEC", M_SEC_RECVD },
        { "R_WEEKDAY", M_WEEKDAY_RECVD },
        { "R_WEEK", M_WEEK_RECVD },
        { "R_UNIXTIME", M_UNIXTIME_RECVD },
        { "R_TZOFFSET", M_TZOFFSET_RECVD },
        { "R_TZ", M_TZ_RECVD },

        { "S_DATE", M_DATE_STAMP },
        { "S_FULLDATE", M_FULLDATE_STAMP },
        { "S_ISODATE", M_ISODATE_STAMP },
        { "S_STAMP", M_STAMP_STAMP },
        { "S_YEAR", M_YEAR_STAMP },
        { "S_MONTH", M_MONTH_STAMP },
        { "S_DAY", M_DAY_STAMP },
        { "S_HOUR", M_HOUR_STAMP },
        { "S_MIN", M_MIN_STAMP },
        { "S_SEC", M_SEC_STAMP },
        { "S_WEEKDAY", M_WEEKDAY_STAMP },
        { "S_WEEK", M_WEEK_STAMP },
        { "S_UNIXTIME", M_UNIXTIME_STAMP },
        { "S_TZOFFSET", M_TZOFFSET_STAMP },
        { "S_TZ", M_TZ_STAMP },

        { "FULLHOST_FROM", M_FULLHOST_FROM },
        { "FULLHOST", M_FULLHOST },

        { "SDATA", M_SDATA },
        { "HOST", M_HOST },
        { "MSGHDR", M_MSGHDR },
        
        { "MSG", M_MESSAGE },
        { "MESSAGE", M_MESSAGE },
        { "MSGONLY", M_MSGONLY },
        { "SOURCEIP", M_SOURCE_IP },
        { "SEQNUM", M_SEQNUM }
};

GHashTable *macro_hash;

static void
result_append(GString *result, const gchar *sstr, gssize len, gboolean escape)
{
  gint i;
  const guchar *ustr = (const guchar *) sstr;
  
  if (len < 0)
    len = strlen(sstr);
  
  if (escape)
    {
      for (i = 0; i < len; i++)
        {
          if (ustr[i] == '\'' || ustr[i] == '"' || ustr[i] == '\\')
            {
              g_string_append_c(result, '\\');
              g_string_append_c(result, ustr[i]);
            }
          else if (ustr[i] < ' ')
            {
              g_string_sprintfa(result, "\\%03o", ustr[i]);
            }
          else
            g_string_append_c(result, ustr[i]);
        }
    }
  else
    g_string_append_len(result, sstr, len);
    
}

gboolean
log_macro_expand(GString *result, gint id, guint32 flags, gint ts_format, TimeZoneInfo *zone_info, gint frac_digits, gint32 seq_num, LogMessage *msg)
{
  switch (id)
    {
    case M_FACILITY:
      {
        /* facility */
        const char *n;
        
        n = syslog_name_lookup_name_by_value(msg->pri & LOG_FACMASK, sl_facilities);
        if (n)
          {
            g_string_append(result, n);
          }
        else
          {
            g_string_sprintfa(result, "%x", (msg->pri & LOG_FACMASK) >> 3);
          }
        break;
      }
    case M_FACILITY_NUM:
      {
        g_string_sprintfa(result, "%d", (msg->pri & LOG_FACMASK) >> 3);
        break;
      }
    case M_LEVEL:
      {
        /* level */
        const char *n;
        
        n = syslog_name_lookup_name_by_value(msg->pri & LOG_PRIMASK, sl_levels);
        if (n)
          {
            g_string_append(result, n);
          }
        else
          {
            g_string_sprintfa(result, "%d", msg->pri & LOG_PRIMASK);
          }

        break;
      }
    case M_LEVEL_NUM:
      {
        g_string_sprintfa(result, "%d", (msg->pri & LOG_PRIMASK));
        break;
      }
    case M_TAG:
      {
        g_string_sprintfa(result, "%02x", msg->pri);
        break;
      }
    case M_BSDTAG:
      {
        g_string_sprintfa(result, "%1d%c", (msg->pri & LOG_PRIMASK), (((msg->pri & LOG_FACMASK) >> 3) + 'A'));
        break;
      }
    case M_PRI:
      {
        g_string_sprintfa(result, "%d", msg->pri);
        break;
      }
    case M_FULLHOST_FROM:
      {
        /* FULLHOST_FROM and $HOST_FROM are the same */

        result_append(result, msg->host_from, msg->host_from_len, !!(flags & LT_ESCAPE));
        break;
      }
    case M_FULLHOST:
      {
        /* full hostname */
        result_append(result, msg->host, msg->host_len, !!(flags & LT_ESCAPE));
        break;
      }
    case M_HOST:
      {
        if (msg->flags & LF_CHAINED_HOSTNAME)
          {
            /* host */
            gchar *p1;
            gchar *p2;
            int remaining, length;
            
            p1 = memchr(msg->host, '@', msg->host_len);
            
            if (p1)
              p1++;
            else
              p1 = msg->host;
            remaining = msg->host_len - (p1 - msg->host);
            p2 = memchr(p1, '/', remaining);
            length = p2 ? p2 - p1 
              : msg->host_len - (p1 - msg->host);
            
            result_append(result, p1, length, !!(flags & LT_ESCAPE));
          }
        else
          {
            result_append(result, msg->host, msg->host_len, !!(flags & LT_ESCAPE));
          }
        break;
      }
    case M_DATE:
    case M_FULLDATE:
    case M_ISODATE:
    case M_STAMP:
    case M_WEEKDAY:
    case M_WEEK:
    case M_YEAR:
    case M_MONTH:
    case M_DAY:
    case M_HOUR:
    case M_MIN:
    case M_SEC:
    case M_TZOFFSET:
    case M_TZ:
    case M_UNIXTIME:

    case M_DATE_RECVD:
    case M_FULLDATE_RECVD:
    case M_ISODATE_RECVD:
    case M_STAMP_RECVD:
    case M_WEEKDAY_RECVD:
    case M_WEEK_RECVD:
    case M_YEAR_RECVD:
    case M_MONTH_RECVD:
    case M_DAY_RECVD:
    case M_HOUR_RECVD:
    case M_MIN_RECVD:
    case M_SEC_RECVD:
    case M_TZOFFSET_RECVD:
    case M_TZ_RECVD:
    case M_UNIXTIME_RECVD:

    case M_DATE_STAMP:
    case M_FULLDATE_STAMP:
    case M_ISODATE_STAMP:
    case M_STAMP_STAMP:
    case M_WEEKDAY_STAMP:
    case M_WEEK_STAMP:
    case M_YEAR_STAMP:
    case M_MONTH_STAMP:
    case M_DAY_STAMP:
    case M_HOUR_STAMP:
    case M_MIN_STAMP:
    case M_SEC_STAMP:
    case M_TZOFFSET_STAMP:
    case M_TZ_STAMP:
    case M_UNIXTIME_STAMP:
      {
        /* year, month, day */
        struct tm *tm, tm_storage;
        gchar buf[64];
        gint length;
        time_t t;
        LogStamp *stamp;
        glong zone_ofs;

        if (id >= M_DATE_RECVD && id <= M_UNIXTIME_RECVD)
          {
            stamp = &msg->timestamps[LM_TS_RECVD];
            id = id - (M_DATE_RECVD - M_DATE);
          }
        else if (id >= M_DATE_STAMP && id <= M_UNIXTIME_STAMP)
          {
            stamp = &msg->timestamps[LM_TS_STAMP];
            id = id - (M_DATE_STAMP - M_DATE);
          }
        else
          {
            if (flags & LT_STAMP_RECVD)
              stamp = &msg->timestamps[LM_TS_RECVD];
            else
              stamp = &msg->timestamps[LM_TS_STAMP];
          }

        /* try to use the following zone values in order:
         *   destination specific timezone, if one is specified
         *   message specific timezone, if one is specified
         *   local timezone
         */
        zone_ofs = (zone_info != NULL ? time_zone_info_get_offset(zone_info, stamp->time.tv_sec) : stamp->zone_offset);
        t = stamp->time.tv_sec + zone_ofs;
        tm = gmtime_r(&t, &tm_storage);

        switch (id)
          {
          case M_WEEKDAY:
            g_string_append_len(result, weekday_names[tm->tm_wday], 3);
            break;
          case M_WEEK:
            g_string_sprintfa(result, "%02d", (tm->tm_yday - (tm->tm_wday - 1 + 7) % 7 + 7) / 7);
            break;
          case M_YEAR:
            g_string_sprintfa(result, "%04d", tm->tm_year + 1900);
            break;
          case M_MONTH:
            g_string_sprintfa(result, "%02d", tm->tm_mon + 1);
            break;
          case M_DAY:
            g_string_sprintfa(result, "%02d", tm->tm_mday);
            break;
          case M_HOUR:
            g_string_sprintfa(result, "%02d", tm->tm_hour);
            break;
          case M_MIN:
            g_string_sprintfa(result, "%02d", tm->tm_min);
            break;
          case M_SEC:
            g_string_sprintfa(result, "%02d", tm->tm_sec);
            break;
          case M_DATE:
          case M_STAMP:
          case M_ISODATE:
          case M_FULLDATE:
          case M_UNIXTIME:
            {
              GString *s = g_string_sized_new(0);
              gint format = id == M_DATE ? TS_FMT_BSD : 
                            id == M_ISODATE ? TS_FMT_ISO :
                            id == M_FULLDATE ? TS_FMT_FULL :
                            id == M_UNIXTIME ? TS_FMT_UNIX :
                            ts_format;
              
              log_stamp_format(stamp, s, format, zone_ofs, frac_digits);
              g_string_append_len(result, s->str, s->len);
              g_string_free(s, TRUE);
              break;
            }
          case M_TZ:
          case M_TZOFFSET:
            length = format_zone_info(buf, sizeof(buf), zone_ofs);
            g_string_append_len(result, buf, length);
            break;
          }
        break;
      }
    case M_SDATA:
      if (flags & LT_ESCAPE)
        {
          GString *sdstr = g_string_sized_new(0);
          
          log_msg_append_format_sdata(msg, sdstr);
          result_append(result, sdstr->str, sdstr->len, TRUE);
          g_string_free(sdstr, TRUE);
        }
      else
        {
          log_msg_append_format_sdata(msg, result);
        }
      break;
    case M_MSGHDR:
      /* message, complete with program name and pid */
      result_append(result, msg->program, msg->program_len, !!(flags & LT_ESCAPE));
      if (msg->program_len > 0)
        {
          if (msg->pid_len > 0)
            {
              result_append(result, "[", 1, !!(flags & LT_ESCAPE));
              result_append(result, msg->pid, msg->pid_len, !!(flags & LT_ESCAPE));
              result_append(result, "]", 1, !!(flags & LT_ESCAPE));
            }
          result_append(result, ": ", 2, !!(flags & LT_ESCAPE));
        }
      break;
    case M_MESSAGE:
      if (!cfg_check_current_config_version(0x0300))
        {
          /* message, complete with program name and pid */
          result_append(result, msg->program, msg->program_len, !!(flags & LT_ESCAPE));
          if (msg->pid_len > 0)
            {
              result_append(result, "[", 1, !!(flags & LT_ESCAPE));
              result_append(result, msg->pid, msg->pid_len, !!(flags & LT_ESCAPE));
              result_append(result, "]", 1, !!(flags & LT_ESCAPE));
            }
          result_append(result, ": ", 2, !!(flags & LT_ESCAPE));
          result_append(result, msg->message, msg->message_len, !!(flags & LT_ESCAPE));
          break;
        } 
    case M_MSGONLY:
      result_append(result, msg->message, msg->message_len, !!(flags & LT_ESCAPE));
      break;
    case M_SOURCE_IP:
      {
        gchar *ip;
        
        if (msg->saddr && g_sockaddr_inet_check(msg->saddr)) 
          {
            gchar buf[16];
            
            g_inet_ntoa(buf, sizeof(buf), ((struct sockaddr_in *) &msg->saddr->sa)->sin_addr);
            ip = buf;
          }
        else 
          {
            ip = "127.0.0.1";
          }
        result_append(result, ip, strlen(ip), !!(flags & LT_ESCAPE));
        break;
      }
    case M_SEQNUM:
      {
        if (seq_num)
          g_string_sprintfa(result, "%d", seq_num);
        break;
      }
    default:
      g_assert_not_reached();
      break;
    }
  return TRUE;
}

guint
log_macro_lookup(gchar *macro, gint len)
{
  gchar buf[256];
  gint macro_id;
  
  g_strlcpy(buf, macro, MIN(sizeof(buf), len+1));
  if (!macro_hash)
    {
      int i;
      macro_hash = g_hash_table_new(g_str_hash, g_str_equal);
      for (i = 0; i < sizeof(macros) / sizeof(macros[0]); i++)
        {
          g_hash_table_insert(macro_hash, macros[i].name,
                              GINT_TO_POINTER(macros[i].id));
        }
    }
  macro_id = GPOINTER_TO_INT(g_hash_table_lookup(macro_hash, buf));
  if (configuration && configuration->version < 0x0300 && (macro_id == M_MESSAGE))
    {
      static gboolean msg_macro_warning = FALSE;
      
      if (!msg_macro_warning)
        {
          msg_warning("WARNING: template: the meaning of the $MSG/$MESSAGE macros is changing in version 3.0, please prepend a $MSGHDR when upgrading to 3.0 config format", NULL);
          msg_macro_warning = TRUE;
        }
    }
  return macro_id;
}


typedef struct _LogTemplateElem
{
  gsize text_len;
  gchar *text;
  guint macro;
  const gchar *value_name;
  gchar *default_value;
} LogTemplateElem;

void
log_template_set_escape(LogTemplate *self, gboolean enable)
{
  if (enable)
    self->flags |= LT_ESCAPE;
  else
    self->flags &= ~LT_ESCAPE;
}

static void
log_template_reset_compiled(LogTemplate *self)
{
  while (self->compiled_template)
    {
      LogTemplateElem *e;

      e = self->compiled_template->data;
      self->compiled_template = g_list_delete_link(self->compiled_template, self->compiled_template);
      if (e->value_name)
        log_msg_free_value_name(e->value_name);
      if (e->default_value)
        g_free(e->default_value);
      if (e->text)
        g_free(e->text);
      g_free(e);
    }

}

/* 
 * NOTE: this steals the str in @text.
 */
static void
log_template_add_macro_elem(LogTemplate *self, guint macro, GString *text, gchar *default_value)
{
  LogTemplateElem *e;
  
  e = g_new(LogTemplateElem, 1);
  e->text_len = text ? text->len : 0;
  e->text = text ? text->str : 0;
  e->macro = macro;
  e->value_name = NULL;
  e->default_value = default_value;
  self->compiled_template = g_list_prepend(self->compiled_template, e);
}

/* 
 * NOTE: this steals the str in @text.
 */
static void
log_template_add_value_elem(LogTemplate *self, gchar *value_name, gsize value_name_len, GString *text, gchar *default_value)
{
  LogTemplateElem *e;
  gchar *dup;
  
  e = g_new(LogTemplateElem, 1);
  e->text_len = text ? text->len : 0;
  e->text = text ? text->str : 0;
  e->macro = M_NONE;
  /* value_name is not NUL terminated */
  dup = g_strndup(value_name, value_name_len);
  e->value_name = log_msg_translate_value_name(dup);
  g_free(dup);
  e->default_value = default_value;
  self->compiled_template = g_list_prepend(self->compiled_template, e);
}

gboolean
log_template_compile(LogTemplate *self, GError **error)
{
  gchar *start, *p;
  guint last_macro = M_NONE;
  GString *last_text = NULL;
  gchar *error_info;
  gint error_pos = 0;
  
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
  
  p = self->template;
  
  while (*p)
    {
      if (*p == '$')
        {
          gboolean finished = FALSE;
          p++;
          /* macro reference */
          if (*p == '{')
            {
              gchar *colon, *fncode;
              gint macro_len;
              gchar *default_value = NULL;

              p++;
              start = p;
              while (*p && *p != '}')
                p++;
              p++;
              
              colon = memchr(start, ':', p - start - 1);
              if (colon)
                {
                  macro_len = colon - start;
                  fncode = colon < p ? colon + 1 : NULL;
                }
              else
                {
                  macro_len = p - start - 1;
                  fncode = NULL;
                }
              
              if (fncode)
                {
                  if (*fncode == '-')
                    {
                      default_value = g_strndup(fncode + 1, p - fncode - 2);
                    }
                  else
                    {
                      error_pos = fncode - self->template;
                      error_info = "Unknown substitution function";
                      goto error;
                    }
                }
              last_macro = log_macro_lookup(start, macro_len);
              if (last_macro == M_NONE)
                {
                  /* this was not a known macro, take it as a "value" reference  */
                  log_template_add_value_elem(self, start, macro_len, last_text, default_value);
                }
              else
                {
                  log_template_add_macro_elem(self, last_macro, last_text, default_value);
                }
              finished = TRUE;
            }
          else
            {
              start = p;
              while ((*p >= 'A' && *p <= 'Z') || (*p == '_') || (*p >= '0' && *p <= '9'))
                {
                  p++;
                }
              last_macro = log_macro_lookup(start, p - start);
              if (last_macro == M_NONE)
                {
                  /* this was not a known macro, take it as a "value" reference  */
                  log_template_add_value_elem(self, start, p-start, last_text, NULL);
                }
              else
                {
                  log_template_add_macro_elem(self, last_macro, last_text, NULL);
                }
              finished = TRUE;
            }
          if (finished)
            {
              if (last_text)
                g_string_free(last_text, FALSE);
              last_macro = M_NONE;
              last_text = NULL;
            }
        }
      else
        {
          if (last_text == NULL)
            last_text = g_string_sized_new(32);
          g_string_append_c(last_text, *p);
          p++;
        }
    }
  if (last_macro != M_NONE || last_text)
    {
      log_template_add_macro_elem(self, last_macro, last_text, NULL);
      g_string_free(last_text, FALSE);
    }
  self->compiled_template = g_list_reverse(self->compiled_template);
  return TRUE;
  
 error:
  g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE, "%s, error_pos='%d'", error_info, error_pos);
  log_template_reset_compiled(self);
  if (!last_text)
    last_text = g_string_sized_new(0);
  g_string_sprintf(last_text, "error in template: %s", self->template);
  log_template_add_macro_elem(self, M_NONE, last_text, NULL);
  g_string_free(last_text, FALSE);
  return FALSE;
}

void
log_template_append_format(LogTemplate *self, LogMessage *lm, guint flags, gint ts_format, TimeZoneInfo *zone_info, gint frac_digits, gint32 seq_num, GString *result)
{
  GList *p;
  LogTemplateElem *e;
  
  flags |= self->flags;
  
  if (self->compiled_template == NULL && !log_template_compile(self, NULL))
    return;

  for (p = self->compiled_template; p; p = g_list_next(p))
    {
      e = (LogTemplateElem *) p->data;
      if (e->text)
        {
          g_string_append_len(result, e->text, e->text_len);
        }

      if (e->value_name)
        {
          gchar *value = NULL;
          gssize value_len = -1;

          value = log_msg_get_value(lm, e->value_name, &value_len);
          if (value && value[0])
            result_append(result, value, value_len, !!(flags & LT_ESCAPE));
          else if (e->default_value)
            result_append(result, e->default_value, -1, !!(flags & LT_ESCAPE));
        }
      else if (e->macro != M_NONE)
        {
          gint len = result->len;
          log_macro_expand(result, e->macro, 
                           flags,
                           ts_format,
                           zone_info,
                           frac_digits,
                           seq_num,
                           lm);
          if (len == result->len && e->default_value)
            g_string_append(result, e->default_value);
        }
    }
}

void
log_template_format(LogTemplate *self, LogMessage *lm, guint macro_flags, gint ts_format, TimeZoneInfo *zone_info, gint frac_digits, gint32 seq_num, GString *result)
{
  g_string_truncate(result, 0);
  log_template_append_format(self, lm, macro_flags, ts_format, zone_info, frac_digits, seq_num, result);
}

LogTemplate *
log_template_new(gchar *name, const gchar *template)
{
  LogTemplate *self = g_new0(LogTemplate, 1);
  
  self->name = g_strdup(name);
  self->template = template ? g_strdup(template) : NULL;
  self->ref_cnt = 1;
  if (configuration && configuration->version < 0x0300)
    {
      static gboolean warn_written = FALSE;
      
      if (!warn_written)
        {
          msg_warning("WARNING: template: the default value for template-escape is changing to 'no' in version 3.0, please update your configuration file accordingly",
                      NULL);
          warn_written = TRUE;
        }
      self->flags = LT_ESCAPE;
    }
  return self;
}

static void 
log_template_free(LogTemplate *self)
{
  log_template_reset_compiled(self);
  g_free(self->name);
  g_free(self->template);
  g_free(self);
}

LogTemplate *
log_template_ref(LogTemplate *s)
{
  if (s)
    {
      g_assert(s->ref_cnt > 0);
      s->ref_cnt++;
    }
  return s;
}

void 
log_template_unref(LogTemplate *s)
{
  if (s)
    {
      g_assert(s->ref_cnt > 0);
      if (--s->ref_cnt == 0)
        log_template_free(s);
    }
}

GQuark
log_template_error_quark()
{
  return g_quark_from_static_string("log-template-error-quark");
}
