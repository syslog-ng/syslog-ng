#include "macros.h"
#include "logmsg.h"
#include "syslog-names.h"
#include "messages.h"

#include "filter.h"

#include <time.h>

struct macro_def
{
  char *name;
  int id;
}
macros[] =
{
        { "FACILITY", M_FACILITY },
        { "PRIORITY", M_LEVEL },
        { "LEVEL", M_LEVEL },
        { "TAG", M_TAG },

        { "DATE", M_DATE },
        { "FULLDATE", M_FULLDATE },
        { "ISODATE", M_ISODATE },
        { "YEAR", M_YEAR },
        { "MONTH", M_MONTH },
        { "DAY", M_DAY },
        { "HOUR", M_HOUR },
        { "MIN", M_MIN },
        { "SEC", M_SEC },
        { "WEEKDAY", M_WEEKDAY },
        { "UNIXTIME", M_UNIXTIME },
        { "TZOFFSET", M_TZOFFSET },
        { "TZ", M_TZ },

        { "R_DATE", M_DATE_RECVD },
        { "R_FULLDATE", M_FULLDATE_RECVD },
        { "R_ISODATE", M_ISODATE_RECVD },
        { "R_YEAR", M_YEAR_RECVD },
        { "R_MONTH", M_MONTH_RECVD },
        { "R_DAY", M_DAY_RECVD },
        { "R_HOUR", M_HOUR_RECVD },
        { "R_MIN", M_MIN_RECVD },
        { "R_SEC", M_SEC_RECVD },
        { "R_WEEKDAY", M_WEEKDAY_RECVD },
        { "R_UNIXTIME", M_UNIXTIME_RECVD },
        { "R_TZOFFSET", M_TZOFFSET_RECVD },
        { "R_TZ", M_TZ_RECVD },

        { "S_DATE", M_DATE_STAMP },
        { "S_FULLDATE", M_FULLDATE_STAMP },
        { "S_ISODATE", M_ISODATE_STAMP },
        { "S_YEAR", M_YEAR_STAMP },
        { "S_MONTH", M_MONTH_STAMP },
        { "S_DAY", M_DAY_STAMP },
        { "S_HOUR", M_HOUR_STAMP },
        { "S_MIN", M_MIN_STAMP },
        { "S_SEC", M_SEC_STAMP },
        { "S_WEEKDAY", M_WEEKDAY_STAMP },
        { "S_UNIXTIME", M_UNIXTIME_STAMP },
        { "S_TZOFFSET", M_TZOFFSET_STAMP },
        { "S_TZ", M_TZ_STAMP },

        { "HOST_FROM", M_HOST_FROM },
        { "FULLHOST_FROM", M_FULLHOST_FROM },
        { "HOST", M_HOST },
        { "FULLHOST", M_FULLHOST },

        { "PROGRAM", M_PROGRAM },
        { "MSG", M_MESSAGE },
        { "MESSAGE", M_MESSAGE },
        { "SOURCEIP", M_SOURCE_IP }
};

GHashTable *macro_hash;

static void
result_append(GString *result, gchar *str, gint len, gboolean escape)
{
  gint i;
  
  if (escape)
    {
      for (i = 0; i < len; i++)
        {
          if (str[i] == '\'' || str[i] == '"' || str[i] == '\\')
            {
              g_string_append_c(result, '\\');
              g_string_append_c(result, str[i]);
            }
          else if (str[i] < ' ')
            {
              g_string_sprintfa(result, "\\%03o", (unsigned int) str[i]);
            }
          else
            g_string_append_c(result, str[i]);
        }
    }
  else
    g_string_append_len(result, str, len);
    
}

gboolean
log_macro_expand(GString *result, gint id, guint32 flags, glong zone_offset, LogMessage *msg)
{
  if (id >= M_MATCH_REF_OFS)
    {
      gint ndx = id - M_MATCH_REF_OFS;
      /* match reference */
      if (re_matches[ndx])
        result_append(result, re_matches[ndx], strlen(re_matches[ndx]), !!(flags & MF_ESCAPE_RESULT));
      
      return TRUE;
    }
  switch (id)
    {
    case M_FACILITY:
      {
        /* facility */
        char *n;
        
        n = syslog_lookup_value(msg->pri & LOG_FACMASK, sl_facilities);
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
    case M_LEVEL:
      {
        /* level */
        char *n;
        
        n = syslog_lookup_value(msg->pri & LOG_PRIMASK, sl_levels);
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
    case M_TAG:
      {
        g_string_sprintfa(result, "%02x", msg->pri);
        break;
      }
    case M_FULLHOST:
      {
        /* full hostname */
        result_append(result, msg->host->str, msg->host->len, !!(flags & MF_ESCAPE_RESULT));
        break;
      }
    case M_HOST:
      {
        /* host */
        gchar *p1 = memchr(msg->host->str, '@', msg->host->len);
        gchar *p2;
        int remaining, length;

        if (p1)
          p1++;
        else
          p1 = msg->host->str;
        remaining = msg->host->len - (p1 - msg->host->str);
        p2 = memchr(p1, '/', remaining);
        length = p2 ? p2 - p1 
                    : msg->host->len - (p1 - msg->host->str);
        result_append(result, p1, length, !!(flags & MF_ESCAPE_RESULT));
        break;
      }
    case M_PROGRAM:
      {
        /* program */
        if (msg->program->len)
          {
            result_append(result, msg->program->str, msg->program->len, !!(flags & MF_ESCAPE_RESULT));
          }
        break;
      }
    case M_FULLDATE:
    case M_ISODATE:
    case M_WEEKDAY:
    case M_DATE:
    case M_YEAR:
    case M_MONTH:
    case M_DAY:
    case M_HOUR:
    case M_MIN:
    case M_SEC:
    case M_TZOFFSET:
    case M_TZ:
    case M_UNIXTIME:

    case M_FULLDATE_RECVD:
    case M_ISODATE_RECVD:
    case M_WEEKDAY_RECVD:
    case M_DATE_RECVD:
    case M_YEAR_RECVD:
    case M_MONTH_RECVD:
    case M_DAY_RECVD:
    case M_HOUR_RECVD:
    case M_MIN_RECVD:
    case M_SEC_RECVD:
    case M_TZOFFSET_RECVD:
    case M_TZ_RECVD:
    case M_UNIXTIME_RECVD:

    case M_FULLDATE_STAMP:
    case M_ISODATE_STAMP:
    case M_WEEKDAY_STAMP:
    case M_DATE_STAMP:
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
        struct tm *tm;
        gchar buf[64];
        gint length;
        time_t t;

        if (id >= M_FULLDATE_RECVD && id <= M_UNIXTIME_RECVD)
          {
            t = msg->recvd.time.tv_sec - zone_offset;
            id = id - (M_FULLDATE_RECVD - M_FULLDATE);
          }
        else if (id >= M_FULLDATE_STAMP && id <= M_UNIXTIME_STAMP)
          {
            t = msg->stamp.time.tv_sec - zone_offset;
            id = id - (M_FULLDATE_STAMP - M_FULLDATE);
          }
        else
          {
            if (flags & MF_STAMP_RECVD)
              t = msg->recvd.time.tv_sec - zone_offset;
            else
              t = msg->stamp.time.tv_sec - zone_offset;
          }
          
        tm = gmtime(&t);

        switch (id)
          {
          case M_WEEKDAY:
            length = strftime(buf, sizeof(buf), "%a", tm);
            g_string_append_len(result, buf, length);
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
          case M_ISODATE:
            length = strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S%z", tm);
            g_string_append_len(result, buf, length);
            break;
          case M_FULLDATE:
            length = strftime(buf, sizeof(buf), "%Y %h %e %H:%M:%S", tm);
            g_string_append_len(result, buf, length);
            break;
          case M_DATE:
            g_string_append_len(result, msg->date->str, msg->date->len);
            break;
          case M_TZOFFSET:
            length = strftime(buf, sizeof(buf), "%z", tm);
            g_string_append_len(result, buf, length);
            break;
          case M_TZ:
            length = strftime(buf, sizeof(buf), "%Z", tm);
            g_string_append_len(result, buf, length);
            break;
          case M_UNIXTIME:
            g_string_sprintfa(result, "%d", (int) t);
            break;
          }
        break;
      }
    case M_MESSAGE:
      /* message */
      result_append(result, msg->msg->str, msg->msg->len, !!(flags & MF_ESCAPE_RESULT));
      break;
    default:
      msg_fatal("Internal error, unknown macro referenced;", NULL);
      return FALSE;
    }
  return TRUE;
}

guint
log_macro_lookup(gchar *macro, gint len)
{
  gchar buf[256];
  
  g_strlcpy(buf, macro, MIN(sizeof(buf), len+1));
  if (!macro_hash)
    {
      int i;
      macro_hash = g_hash_table_new(g_str_hash, g_str_equal);
      for (i = 0; i < sizeof(macros) / sizeof(macros[0]); i++)
        {
          g_hash_table_insert(macro_hash, macros[i].name,
                              (gpointer) macros[i].id);
        }
    }
  return (gint) g_hash_table_lookup(macro_hash, buf);

}

