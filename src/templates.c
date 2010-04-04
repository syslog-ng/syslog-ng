/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
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
  M_TAGS,
  M_BSDTAG,
  M_PRI,

  M_HOST,
  M_SDATA,

  M_MSGHDR,
  M_MESSAGE,
  M_SOURCE_IP,
  M_SEQNUM,

  M_DATE,
  M_FULLDATE,
  M_ISODATE,
  M_STAMP,
  M_YEAR,
  M_YEAR_DAY,
  M_MONTH,
  M_MONTH_WEEK,
  M_MONTH_ABBREV,
  M_MONTH_NAME,
  M_DAY,
  M_HOUR,
  M_MIN,
  M_SEC,
  M_WEEK_DAY,
  M_WEEK_DAY_ABBREV,
  M_WEEK_DAY_NAME,
  M_WEEK,
  M_TZOFFSET,
  M_TZ,
  M_UNIXTIME,
  M_TIME_FIRST = M_DATE,
  M_TIME_LAST = M_UNIXTIME,
  M_TIME_MACROS_MAX = M_UNIXTIME - M_DATE + 1,
  
  M_RECVD_OFS = M_TIME_MACROS_MAX,
  M_STAMP_OFS = 2 * M_TIME_MACROS_MAX,
};

#define M_TIME_MACROS 15

LogMacroDef macros[] =
{
        { "FACILITY", M_FACILITY },
        { "FACILITY_NUM", M_FACILITY_NUM },
        { "PRIORITY", M_LEVEL },
        { "LEVEL", M_LEVEL },
        { "LEVEL_NUM", M_LEVEL_NUM },
        { "TAG", M_TAG },
        { "TAGS", M_TAGS },
        { "BSDTAG", M_BSDTAG },
        { "PRI", M_PRI },

        { "DATE",           M_DATE },
        { "FULLDATE",       M_FULLDATE },
        { "ISODATE",        M_ISODATE },
        { "STAMP",          M_STAMP },
        { "YEAR",           M_YEAR },
        { "YEAR_DAY",       M_YEAR_DAY },
        { "MONTH",          M_MONTH },
        { "MONTH_WEEK",     M_MONTH_WEEK },
        { "MONTH_ABBREV",   M_MONTH_ABBREV },
        { "MONTH_NAME",     M_MONTH_NAME },
        { "DAY",            M_DAY },
        { "HOUR",           M_HOUR },
        { "MIN",            M_MIN },
        { "SEC",            M_SEC },
        { "WEEKDAY",        M_WEEK_DAY_ABBREV }, /* deprecated */
        { "WEEK_DAY",       M_WEEK_DAY },
        { "WEEK_DAY_ABBREV",M_WEEK_DAY_ABBREV },
        { "WEEK_DAY_NAME",  M_WEEK_DAY_NAME },
        { "WEEK",           M_WEEK },
        { "TZOFFSET",       M_TZOFFSET },
        { "TZ",             M_TZ },
        { "UNIXTIME",       M_UNIXTIME },

        { "R_DATE",           M_RECVD_OFS + M_DATE },
        { "R_FULLDATE",       M_RECVD_OFS + M_FULLDATE },
        { "R_ISODATE",        M_RECVD_OFS + M_ISODATE },
        { "R_STAMP",          M_RECVD_OFS + M_STAMP },
        { "R_YEAR",           M_RECVD_OFS + M_YEAR },
        { "R_YEAR_DAY",       M_RECVD_OFS + M_YEAR_DAY },
        { "R_MONTH",          M_RECVD_OFS + M_MONTH },
        { "R_MONTH_WEEK",     M_RECVD_OFS + M_MONTH_WEEK },
        { "R_MONTH_ABBREV",   M_RECVD_OFS + M_MONTH_ABBREV },
        { "R_MONTH_NAME",     M_RECVD_OFS + M_MONTH_NAME },
        { "R_DAY",            M_RECVD_OFS + M_DAY },
        { "R_HOUR",           M_RECVD_OFS + M_HOUR },
        { "R_MIN",            M_RECVD_OFS + M_MIN },
        { "R_SEC",            M_RECVD_OFS + M_SEC },
        { "R_WEEKDAY",        M_RECVD_OFS + M_WEEK_DAY_ABBREV }, /* deprecated */
        { "R_WEEK_DAY",       M_RECVD_OFS + M_WEEK_DAY },
        { "R_WEEK_DAY_ABBREV",M_RECVD_OFS + M_WEEK_DAY_ABBREV },
        { "R_WEEK_DAY_NAME",  M_RECVD_OFS + M_WEEK_DAY_NAME },
        { "R_WEEK",           M_RECVD_OFS + M_WEEK },
        { "R_TZOFFSET",       M_RECVD_OFS + M_TZOFFSET },
        { "R_TZ",             M_RECVD_OFS + M_TZ },
        { "R_UNIXTIME",       M_RECVD_OFS + M_UNIXTIME },

        { "S_DATE",           M_STAMP_OFS + M_DATE },
        { "S_FULLDATE",       M_STAMP_OFS + M_FULLDATE },
        { "S_ISODATE",        M_STAMP_OFS + M_ISODATE },
        { "S_STAMP",          M_STAMP_OFS + M_STAMP },
        { "S_YEAR",           M_STAMP_OFS + M_YEAR },
        { "S_YEAR_DAY",       M_STAMP_OFS + M_YEAR_DAY },
        { "S_MONTH",          M_STAMP_OFS + M_MONTH },
        { "S_MONTH_WEEK",     M_STAMP_OFS + M_MONTH_WEEK },
        { "S_MONTH_ABBREV",   M_STAMP_OFS + M_MONTH_ABBREV },
        { "S_MONTH_NAME",     M_STAMP_OFS + M_MONTH_NAME },
        { "S_DAY",            M_STAMP_OFS + M_DAY },
        { "S_HOUR",           M_STAMP_OFS + M_HOUR },
        { "S_MIN",            M_STAMP_OFS + M_MIN },
        { "S_SEC",            M_STAMP_OFS + M_SEC },
        { "S_WEEKDAY",        M_STAMP_OFS + M_WEEK_DAY_ABBREV }, /* deprecated */
        { "S_WEEK_DAY",       M_STAMP_OFS + M_WEEK_DAY },
        { "S_WEEK_DAY_ABBREV",M_STAMP_OFS + M_WEEK_DAY_ABBREV },
        { "S_WEEK_DAY_NAME",  M_STAMP_OFS + M_WEEK_DAY_NAME },
        { "S_WEEK",           M_STAMP_OFS + M_WEEK },
        { "S_TZOFFSET",       M_STAMP_OFS + M_TZOFFSET },
        { "S_TZ",             M_STAMP_OFS + M_TZ },
        { "S_UNIXTIME",       M_STAMP_OFS + M_UNIXTIME },

        { "SDATA", M_SDATA },
        { "MSGHDR", M_MSGHDR },
        { "SOURCEIP", M_SOURCE_IP },
        { "SEQNUM", M_SEQNUM },

        /* values that have specific behaviour with older syslog-ng config versions */
        { "MSG", M_MESSAGE },
        { "MESSAGE", M_MESSAGE },
        { "HOST", M_HOST },
        { NULL, 0 }
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

static void
result_append_value(GString *result, LogMessage *lm, NVHandle handle, gboolean escape)
{
  const gchar *str;
  gssize len = 0;

  str = log_msg_get_value(lm, handle, &len);
  result_append(result, str, len, escape);
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
    case M_TAGS:
      {
        log_msg_print_tags(msg, result);
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
    case M_HOST:
      {
        if (msg->flags & LF_CHAINED_HOSTNAME)
          {
            /* host */
            const gchar *p1, *p2;
            int remaining, length;
            gssize host_len;
            const gchar *host = log_msg_get_value(msg, LM_V_HOST, &host_len);
            
            p1 = memchr(host, '@', host_len);
            
            if (p1)
              p1++;
            else
              p1 = host;
            remaining = host_len - (p1 - host);
            p2 = memchr(p1, '/', remaining);
            length = p2 ? p2 - p1 
              : host_len - (p1 - host);
            
            result_append(result, p1, length, !!(flags & LT_ESCAPE));
          }
        else
          {
            result_append_value(result, msg, LM_V_HOST, !!(flags & LT_ESCAPE));
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
      if ((msg->flags & LF_LEGACY_MSGHDR))
        {
          /* fast path for now, as most messages come from legacy devices */

          result_append_value(result, msg, LM_V_LEGACY_MSGHDR, !!(flags & LT_ESCAPE));
        }
      else
        {
          /* message, complete with program name and pid */
          gssize len;

          len = result->len;
          result_append_value(result, msg, LM_V_PROGRAM, !!(flags & LT_ESCAPE));
          if (len != result->len)
            {
              const gchar *pid = log_msg_get_value(msg, LM_V_PID, &len);
              if (len > 0)
                {
                  result_append(result, "[", 1, FALSE);
                  result_append(result, pid, len, !!(flags & LT_ESCAPE));
                  result_append(result, "]", 1, FALSE);
                }
              result_append(result, ": ", 2, FALSE);
            }
        }
      break;
    case M_MESSAGE:
      if (!cfg_check_current_config_version(0x0300))
        log_macro_expand(result, M_MSGHDR, flags, ts_format, zone_info, frac_digits, seq_num, msg);
      result_append_value(result, msg, LM_V_MESSAGE, !!(flags & LT_ESCAPE));
      break;
    case M_SOURCE_IP:
      {
        gchar *ip;

        if (msg->saddr && (g_sockaddr_inet_check(msg->saddr) ||
#if ENABLE_IPV6
            g_sockaddr_inet6_check(msg->saddr))
#else
            0)
#endif
           )
          {
            gchar buf[MAX_SOCKADDR_STRING];

            g_sockaddr_format(msg->saddr, buf, sizeof(buf), GSA_ADDRESS_ONLY);
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
      {
        /* year, month, day */
        struct tm *tm, tm_storage;
        gchar buf[64];
        gint length;
        time_t t;
        LogStamp *stamp;
        glong zone_ofs;

        if (id >= M_TIME_FIRST && id <= M_TIME_LAST)
          {
            if (flags & LT_STAMP_RECVD)
              stamp = &msg->timestamps[LM_TS_RECVD];
            else
              stamp = &msg->timestamps[LM_TS_STAMP];
          }
        else if (id >= M_TIME_FIRST + M_RECVD_OFS && id <= M_TIME_LAST + M_RECVD_OFS)
          {
            id -= M_RECVD_OFS;
            stamp = &msg->timestamps[LM_TS_RECVD];
          }
        else if (id >= M_TIME_FIRST + M_STAMP_OFS && id <= M_TIME_LAST + M_STAMP_OFS)
          {
            id -= M_STAMP_OFS;
            stamp = &msg->timestamps[LM_TS_STAMP];
          }
        else
          {
            g_assert_not_reached();
            break;
          }

        /* try to use the following zone values in order:
         *   destination specific timezone, if one is specified
         *   message specific timezone, if one is specified
         *   local timezone
         */
        zone_ofs = (zone_info != NULL ? time_zone_info_get_offset(zone_info, stamp->time.tv_sec) : stamp->zone_offset);
        if (zone_ofs == -1)
          zone_ofs = stamp->zone_offset;

        t = stamp->time.tv_sec + zone_ofs;
        tm = gmtime_r(&t, &tm_storage);

        switch (id)
          {
          case M_WEEK_DAY_ABBREV:
            g_string_append_len(result, weekday_names_abbrev[tm->tm_wday], 3);
            break;
          case M_WEEK_DAY_NAME:
            g_string_append(result, weekday_names[tm->tm_wday]);
            break;
          case M_WEEK_DAY:
            g_string_sprintfa(result, "%d", tm->tm_wday + 1);
            break;
          case M_WEEK:
            g_string_sprintfa(result, "%02d", (tm->tm_yday - (tm->tm_wday - 1 + 7) % 7 + 7) / 7);
            break;
          case M_YEAR:
            g_string_sprintfa(result, "%04d", tm->tm_year + 1900);
            break;
          case M_YEAR_DAY:
            g_string_sprintfa(result, "%03d", tm->tm_yday + 1);
            break;
          case M_MONTH:
            g_string_sprintfa(result, "%02d", tm->tm_mon + 1);
            break;
          case M_MONTH_WEEK:
            g_string_sprintfa(result, "%d", ((tm->tm_mday / 7) + ((tm->tm_wday > 0) && ((tm->tm_mday % 7) >= tm->tm_wday))));
            break;
          case M_MONTH_ABBREV:
            g_string_append_len(result, month_names_abbrev[tm->tm_mon], 3);
            break;
          case M_MONTH_NAME:
            g_string_append(result, month_names[tm->tm_mon]);
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
      for (i = 0; macros[i].name; i++)
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
  NVHandle value_handle;
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
  e->value_handle = 0;
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
  e->value_handle = log_msg_get_value_handle(dup);
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
  
  if (self->compiled_template)
    return TRUE;

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

              if (!*p)
                {
                  error_pos = p - self->template;
                  error_info = "Invalid macro, '}' is missing";
                  goto error;
                }

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
              while ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p == '_') || (*p >= '0' && *p <= '9'))
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
  
  if (!log_template_compile(self, NULL))
    return;

  for (p = self->compiled_template; p; p = g_list_next(p))
    {
      e = (LogTemplateElem *) p->data;
      if (e->text)
        {
          g_string_append_len(result, e->text, e->text_len);
        }

      if (e->value_handle)
        {
          const gchar *value = NULL;
          gssize value_len = -1;

          value = log_msg_get_value(lm, e->value_handle, &value_len);
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
