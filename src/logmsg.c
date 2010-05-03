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

#include "logmsg.h"
#include "misc.h"
#include "messages.h"
#include "logpipe.h"
#include "timeutils.h"
#include "tags.h"
#include "nvtable.h"
#include "stats.h"
#include "templates.h"

#include <sys/types.h>
#include <time.h>
#include <syslog.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

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

/**********************************************************************
 * LogMessage
 **********************************************************************/

static inline gboolean
log_msg_chk_flag(LogMessage *self, gint32 flag)
{
  return self->flags & flag;
}

static inline void
log_msg_set_flag(LogMessage *self, gint32 flag)
{
  self->flags |= flag;
}

static inline void
log_msg_unset_flag(LogMessage *self, gint32 flag)
{
  self->flags &= ~flag;
}

/* the index matches the value id */
const gchar *builtin_value_names[] =
{
  "HOST",
  "HOST_FROM",
  "MESSAGE",
  "PROGRAM",
  "PID",
  "MSGID",
  "SOURCE",
  "LEGACY_MSGHDR",
  NULL,
};

NVRegistry *logmsg_registry;
static const char sd_prefix[] = ".SDATA.";
static const gint sd_prefix_len = sizeof(sd_prefix) - 1;
/* statistics */
static guint32 *count_msg_clones;
static guint32 *count_payload_reallocs;
static guint32 *count_sdata_updates;

static void
log_msg_update_sdata_slow(LogMessage *self, NVHandle handle, const gchar *name, gssize name_len)
{
  guint16 alloc_sdata;
  guint16 prefix_and_block_len;
  gint i;
  const gchar *dot;

  /* this was a structured data element, insert a ref to the sdata array */

  stats_counter_inc(count_sdata_updates);
  if (self->num_sdata == 255)
    {
      msg_error("syslog-ng only supports 255 SD elements right now, just drop an email to the mailing list that it was not enough with your use-case so we can increase it", NULL);
      return;
    }

  if (self->alloc_sdata <= self->num_sdata)
    {
      alloc_sdata = MAX(self->num_sdata + 1, (self->num_sdata + 8) & ~7);
      if (alloc_sdata > 255)
        alloc_sdata = 255;
    }
  else
    alloc_sdata = self->alloc_sdata;

  if (log_msg_chk_flag(self, LF_STATE_OWN_SDATA) && self->sdata)
    {
      if (self->alloc_sdata < alloc_sdata)
        {
          self->sdata = g_realloc(self->sdata, alloc_sdata * sizeof(self->sdata[0]));
          memset(&self->sdata[self->alloc_sdata], 0, (alloc_sdata - self->alloc_sdata) * sizeof(self->sdata[0]));
        }
    }
  else
    {
      NVHandle *sdata;

      sdata = g_malloc(alloc_sdata * sizeof(self->sdata[0]));
      if (self->num_sdata)
        memcpy(sdata, self->sdata, self->num_sdata * sizeof(self->sdata[0]));
      memset(&sdata[self->num_sdata], 0, sizeof(self->sdata[0]) * (self->alloc_sdata - self->num_sdata));
      self->sdata = sdata;
      log_msg_set_flag(self, LF_STATE_OWN_SDATA);
    }
  self->alloc_sdata = alloc_sdata;

  /* ok, we have our own SDATA array now which has at least one free slot */

  if (!self->initial_parse)
    {
      dot = memrchr(name, '.', name_len);
      prefix_and_block_len = dot - name;

      for (i = self->num_sdata - 1; i >= 0; i--)
        {
          gssize sdata_name_len;
          const gchar *sdata_name;

          sdata_name = log_msg_get_value_name(self->sdata[i], &sdata_name_len);
          if (sdata_name_len > prefix_and_block_len &&
              strncmp(sdata_name, name, prefix_and_block_len) == 0)
            {
              /* ok we have found the last SDATA entry that has the same block */
              break;
            }
        }
    }
  else
    i = -1;

  if (i >= 0)
    {
      memmove(&self->sdata[i+1], &self->sdata[i], (self->num_sdata - i) * sizeof(self->sdata[0]));
      self->sdata[i] = handle;
    }
  else
    {
      self->sdata[self->num_sdata] = handle;
    }
  self->num_sdata++;
}

static inline void
log_msg_update_sdata(LogMessage *self, NVHandle handle, const gchar *name, gssize name_len)
{
  guint8 flags;

  flags = nv_registry_get_handle_flags(logmsg_registry, handle);
  if (G_UNLIKELY(flags & LM_VF_SDATA))
    log_msg_update_sdata_slow(self, handle, name, name_len);
}

NVHandle
log_msg_get_value_handle(const gchar *value_name)
{
  NVHandle handle;

  handle = nv_registry_alloc_handle(logmsg_registry, value_name);

  /* check if name starts with sd_prefix and has at least one additional character */
  if (strncmp(value_name, sd_prefix, sizeof(sd_prefix) - 1) == 0 && value_name[6])
    {
      nv_registry_set_handle_flags(logmsg_registry, handle, LM_VF_SDATA);
    }

  return handle;
}

const gchar *
log_msg_get_value_name(NVHandle handle, gssize *name_len)
{
  return nv_registry_get_handle_name(logmsg_registry, handle, name_len);
}

const gchar *
log_msg_get_macro_value(LogMessage *self, gint id, gssize *value_len)
{
  static GString *value = NULL;

  if (!value)
    value = g_string_sized_new(256);
  g_string_truncate(value, 0);

  log_macro_expand(value, id, 0, TS_FMT_BSD, NULL, 0, 0, self);
  if (value_len)
    *value_len = value->len;
  return value->str;
}

gboolean
log_msg_is_handle_macro(NVHandle handle)
{
  guint16 flags;

  flags = nv_registry_get_handle_flags(logmsg_registry, handle);
  return !!(flags & LM_VF_MACRO);
}

void
log_msg_set_value(LogMessage *self, NVHandle handle, const gchar *value, gssize value_len)
{
  const gchar *name;
  gssize name_len;
  gboolean new_entry = FALSE;

  if (handle == LM_V_NONE)
    return;

  name = log_msg_get_value_name(handle, &name_len);

  if (value_len < 0)
    value_len = strlen(value);

  if (!log_msg_chk_flag(self, LF_STATE_OWN_PAYLOAD))
    {
      self->payload = nv_table_clone(self->payload, name_len + value_len + 2);
      log_msg_set_flag(self, LF_STATE_OWN_PAYLOAD);
    }

  do
    {
      if (!nv_table_add_value(self->payload, handle, name, name_len, value, value_len, &new_entry))
        {
          /* error allocating string in payload, reallocate */
          self->payload = nv_table_realloc(self->payload);
          stats_counter_inc(count_payload_reallocs);
        }
      else
        {
          break;
        }
    }
  while (1);

  if (new_entry)
    log_msg_update_sdata(self, handle, name, name_len);
}

void
log_msg_set_value_indirect(LogMessage *self, NVHandle handle, NVHandle ref_handle, guint8 type, guint16 ofs, guint16 len)
{
  const gchar *name;
  gssize name_len;
  gboolean new_entry = FALSE;

  if (handle == LM_V_NONE)
    return;

  g_assert(handle >= LM_V_MAX);

  name = log_msg_get_value_name(handle, &name_len);

  if (!log_msg_chk_flag(self, LF_STATE_OWN_PAYLOAD))
    {
      self->payload = nv_table_clone(self->payload, name_len + 1);
      log_msg_set_flag(self, LF_STATE_OWN_PAYLOAD);
    }

  do
    {
      if (!nv_table_add_value_indirect(self->payload, handle, name, name_len, ref_handle, type, ofs, len, &new_entry))
        {
          /* error allocating string in payload, reallocate */
          self->payload = nv_table_realloc(self->payload);
          stats_counter_inc(count_payload_reallocs);
        }
      else
        {
          break;
        }
    }
  while (1);

  if (new_entry)
    log_msg_update_sdata(self, handle, name, name_len);
}

NVHandle match_handles[256];

void
log_msg_set_match(LogMessage *self, gint index, const gchar *value, gssize value_len)
{
  g_assert(index < 256);

  if (index >= self->num_matches)
    self->num_matches = index + 1;
  log_msg_set_value(self, match_handles[index], value, value_len);
}

void
log_msg_set_match_indirect(LogMessage *self, gint index, NVHandle ref_handle, guint8 type, guint16 ofs, guint16 len)
{
  g_assert(index < 256);

  log_msg_set_value_indirect(self, match_handles[index], ref_handle, type, ofs, len);
}

void
log_msg_clear_matches(LogMessage *self)
{
  gint i;

  for (i = 0; i < self->num_matches; i++)
    {
      log_msg_set_value(self, match_handles[i], "", 0);
    }
  self->num_matches = 0;
}

static guint
swap_index_big_endian(guint index)
{
  return G_BYTE_ORDER == G_BIG_ENDIAN ? 1-index : index;
}

gboolean
log_msg_tags_foreach(LogMessage *self, LogMessageTableForeachFunc callback, gpointer user_data)
{
  guint i, j, k;
  guint tag_id;
  for (i = 0; i != self->num_tags; ++i)
    {
      if (G_LIKELY(!self->tags[i]))
        continue;
      for (j = 0; j != 2; ++j)
        {
          if (G_LIKELY(! * ( ((guint16*) (&self->tags[i])) + swap_index_big_endian(j))))
            continue;

          for (k = 0; k != 2; ++k)
            {
              if (G_LIKELY(! * ( ((guint8*) (&self->tags[i])) + swap_index_big_endian(j) * 2 + swap_index_big_endian(k))))
                continue;

              guint bitidx;

              for (bitidx = 0; bitidx != 8; ++bitidx)
                {
                  if ( *(((guint8*) (&self->tags[i])) + swap_index_big_endian(j) * 2 +swap_index_big_endian(k)) & (1 << bitidx))
                    {
                      tag_id = i * 32  + j * 16 + k * 8 + bitidx;
                      gchar *name =  log_tags_get_by_id(tag_id);
                      callback(self, i , tag_id, name, user_data);
                    }
                }
            }
        }
    }
  return TRUE;
}

static inline void
log_msg_set_tag_by_id_onoff(LogMessage *self, guint id, gboolean on)
{
  guint32 *tags;
  gint old_num_tags;

  if (!log_msg_chk_flag(self, LF_STATE_OWN_TAGS) && self->num_tags)
    {
      tags = self->tags;
      self->tags = g_new0(guint32, self->num_tags);
      memcpy(self->tags, tags, sizeof(guint32) * self->num_tags);
    }
  log_msg_set_flag(self, LF_STATE_OWN_TAGS);

  if ((self->num_tags * 32) <= id)
    {
      if (G_UNLIKELY(8159 < id))
        {
          msg_error("Maximum number of tags reached", NULL);
          return;
        }
      old_num_tags = self->num_tags;
      self->num_tags = (id / 32) + 1;

      tags = self->tags;
      self->tags = g_new0(guint32, self->num_tags);
      if (tags)
        {
          memcpy(self->tags, tags, sizeof(guint32) * old_num_tags);
          g_free(tags);
        }
    }

  if (on)
    self->tags[id / 32] |= (guint32)(1 << (id % 32));
  else
    self->tags[id / 32] &= (guint32) ~((guint32)(1 << (id % 32)));
}

void
log_msg_set_tag_by_id(LogMessage *self, guint id)
{
  log_msg_set_tag_by_id_onoff(self, id, TRUE);
}

void
log_msg_set_tag_by_name(LogMessage *self, const gchar *name)
{
  log_msg_set_tag_by_id_onoff(self, log_tags_get_by_name(name), TRUE);
}

void
log_msg_clear_tag_by_id(LogMessage *self, guint id)
{
  log_msg_set_tag_by_id_onoff(self, id, FALSE);
}

void
log_msg_clear_tag_by_name(LogMessage *self, const gchar *name)
{
  log_msg_set_tag_by_id_onoff(self, log_tags_get_by_name(name), FALSE);
}

gboolean
log_msg_is_tag_by_id(LogMessage *self, guint id)
{
  if (G_UNLIKELY(8159 < id))
    {
      msg_error("Invalid tag", evt_tag_int("id", id), NULL);
      return FALSE;
    }

  return (id < (self->num_tags * 32)) && (self->tags[id / 32] & (guint32) ((guint32) 1 << (id % 32))) != (guint32) 0;
}

gboolean
log_msg_is_tag_by_name(LogMessage *self, const gchar *name)
{
  return log_msg_is_tag_by_id(self, log_tags_get_by_name(name));
}

/* structured data elements */

static void
log_msg_sdata_append_escaped(GString *result, const gchar *sstr, gssize len)
{
  gint i;
  const guchar *ustr = (const guchar *) sstr;

  for (i = 0; i < len; i++)
    {
      if (ustr[i] == '"' || ustr[i] == '\\' || ustr[i] == ']')
        {
          g_string_append_c(result, '\\');
          g_string_append_c(result, ustr[i]);
        }
      else
        g_string_append_c(result, ustr[i]);
    }
}

void
log_msg_append_format_sdata(LogMessage *self, GString *result)
{
  const gchar *value;
  const gchar *sdata_name, *sdata_elem, *sdata_param, *cur_elem = NULL, *dot;
  gssize sdata_name_len, sdata_elem_len, sdata_param_len, cur_elem_len = 0, len;
  gint i;

  for (i = 0; i < self->num_sdata; i++)
    {
      NVHandle handle = self->sdata[i];
      guint16 handle_flags;
      gint sd_id_len;

      sdata_name = log_msg_get_value_name(handle, &sdata_name_len);
      handle_flags = nv_registry_get_handle_flags(logmsg_registry, handle);

      g_assert(handle_flags & LM_VF_SDATA);

      /* sdata_name always begins with .SDATA. */
      g_assert(sdata_name_len > 6);

      sdata_elem = sdata_name + 7;
      sd_id_len = (handle_flags >> 8);

      if (sd_id_len)
        {
          dot = sdata_elem + sd_id_len;
          g_assert((dot - sdata_name < sdata_name_len) && *dot == '.');
        }
      else
        {
          dot = memrchr(sdata_elem, '.', sdata_name_len - 7);
        }

      if (G_LIKELY(dot))
        {
          sdata_elem_len = dot - sdata_elem;

          sdata_param = dot + 1;
          sdata_param_len = sdata_name_len - (dot + 1 - sdata_name);
        }
      else
        {
          sdata_elem_len = sdata_name_len - 7;
          if (sdata_elem_len == 0)
            {
              sdata_elem = "none";
              sdata_elem_len = 4;
            }
          sdata_param = "none";
          sdata_param_len = 4;
        }
      if (!cur_elem || sdata_elem_len != cur_elem_len || strncmp(cur_elem, sdata_elem, sdata_elem_len) != 0)
        {
          if (cur_elem)
            {
              /* close the previous block */
              g_string_append_c(result, ']');
            }

          /* the current SD block has changed, emit a start */
          g_string_append_c(result, '[');
          g_string_append_len(result, sdata_elem, sdata_elem_len);

          /* update cur_elem */
          cur_elem = sdata_elem;
          cur_elem_len = sdata_elem_len;
        }
      g_string_append_c(result, ' ');
      g_string_append_len(result, sdata_param, sdata_param_len);
      g_string_append(result, "=\"");

      value = log_msg_get_value(self, handle, &len);
      log_msg_sdata_append_escaped(result, value, len);
      g_string_append_c(result, '"');
    }
  if (cur_elem)
    {
      g_string_append_c(result, ']');
    }
}

void
log_msg_format_sdata(LogMessage *self, GString *result)
{
  g_string_truncate(result, 0);
  log_msg_append_format_sdata(self, result);
}

static inline void
copy_string_fixed_buf(guchar *dest, const guchar *src, gsize dest_len)
{
  strncpy((gchar *) dest, (gchar *) src, dest_len);
  dest[dest_len-1] = 0;
}

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
      self->pri = default_pri != 0xFFFF ? default_pri : (LOG_USER | LOG_NOTICE);
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
log_msg_parse_date(LogMessage *self, const guchar **data, gint *length, guchar *date, gsize date_len, guint parse_flags, glong assume_timezone)
{
  const guchar *src = *data;
  gint left = *length;
  time_t now = time(NULL);
  struct tm tm;
  guchar *p;
  gint unnormalized_hour;
  
  date[0] = 0;

  /* If the next chars look like a date, then read them as a date. */
  if (left >= 19 && src[4] == '-' && src[7] == '-' && src[10] == 'T' && src[13] == ':' && src[16] == ':')
    {
      /* RFC3339 timestamp, expected format: YYYY-MM-DDTHH:MM:SS[.frac]<+/->ZZ:ZZ */
      gint hours, mins;
      
      self->timestamps[LM_TS_STAMP].time.tv_usec = 0;
      
      copy_string_fixed_buf(date, src, MIN(date_len, 20));
      
      /* NOTE: we initialize various unportable fields in tm using a
       * localtime call, as the value of tm_gmtoff does matter but it does
       * not exist on all platforms and 0 initializing it causes trouble on
       * time-zone barriers */
      
      cached_localtime(&now, &tm);
      p = (guchar *) strptime((gchar *) date, STRPTIME_ISOFORMAT, &tm);

      if (!p || (p && *p))
        {
          /* not the complete stamp could be parsed */
          goto error;
        }

      src += p - date;
      left -= p - date;
      
      
      self->timestamps[LM_TS_STAMP].time.tv_usec = 0;
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
          self->timestamps[LM_TS_STAMP].time.tv_usec = frac * (1000000 / div);
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
      self->timestamps[LM_TS_STAMP].time.tv_sec = cached_mktime(&tm);
    }
  else if ((parse_flags & LP_SYSLOG_PROTOCOL) == 0)
    {
      if (left >= 21 && src[3] == ' ' && src[6] == ' ' && src[11] == ' ' && src[14] == ':' && src[17] == ':' && src[20] == ':' &&
          (isdigit(src[7]) && isdigit(src[8]) && isdigit(src[9]) && isdigit(src[10])))
        {
          /* PIX timestamp, expected format: MMM DD YYYY HH:MM:SS: */

          /* Just read the buffer data into a textual
             datestamp. */

          copy_string_fixed_buf(date, src, MIN(date_len, 22));
          src += 21;
          left -= 21;

          /* And also make struct time timestamp for the msg */

          cached_localtime(&now, &tm);
          p = (guchar *) strptime((gchar *) date, "%b %e %Y %H:%M:%S:", &tm);
          if (!p || (p && *p))
            goto error;
            
          tm.tm_isdst = -1;
            
          /* NOTE: no timezone information in the message, assume it is local time */
          unnormalized_hour = tm.tm_hour;
          self->timestamps[LM_TS_STAMP].time.tv_sec = cached_mktime(&tm);
          self->timestamps[LM_TS_STAMP].time.tv_usec = 0;
        }
      else if (left >= 21 && src[3] == ' ' && src[6] == ' ' && src[11] == ' ' && src[14] == ':' && src[17] == ':' && src[20] == ' ' &&
          (isdigit(src[7]) && isdigit(src[8]) && isdigit(src[9]) && isdigit(src[10])))
        {
          /* ASA timestamp, expected format: MMM DD YYYY HH:MM:SS */

          /* Just read the buffer data into a textual
             datestamp. */

          copy_string_fixed_buf(date, src, MIN(date_len, 21));
          src += 20;
          left -= 20;

          /* And also make struct time timestamp for the msg */

          cached_localtime(&now, &tm);
          p = (guchar *) strptime((gchar *) date, "%b %e %Y %H:%M:%S", &tm);
          if (!p || (p && *p))
            goto error;

          tm.tm_isdst = -1;

          /* NOTE: no timezone information in the message, assume it is local time */
          unnormalized_hour = tm.tm_hour;
          self->timestamps[LM_TS_STAMP].time.tv_sec = cached_mktime(&tm);
          self->timestamps[LM_TS_STAMP].time.tv_usec = 0;
        }
      else if (left >= 21 && src[3] == ' ' && src[6] == ' ' && src[9] == ':' && src[12] == ':' && src[15] == ' ' && 
               isdigit(src[16]) && isdigit(src[17]) && isdigit(src[18]) && isdigit(src[19]) && isspace(src[20]))
        {
          /* LinkSys timestamp, expected format: MMM DD HH:MM:SS YYYY */

          /* Just read the buffer data into a textual
             datestamp. */

          copy_string_fixed_buf(date, src, 21);
          src += 20;
          left -= 20;

          /* And also make struct time timestamp for the msg */

          cached_localtime(&now, &tm);
          p = (guchar *) strptime((gchar *) date, "%b %e %H:%M:%S %Y", &tm);
          if (!p || (p && *p))
            goto error;
          tm.tm_isdst = -1;
            
          /* NOTE: no timezone information in the message, assume it is local time */
          unnormalized_hour = tm.tm_hour;
          self->timestamps[LM_TS_STAMP].time.tv_sec = cached_mktime(&tm);
          self->timestamps[LM_TS_STAMP].time.tv_usec = 0;
          
        }
      else if (left >= 15 && src[3] == ' ' && src[6] == ' ' && src[9] == ':' && src[12] == ':')
        {
          /* RFC 3164 timestamp, expected format: MMM DD HH:MM:SS ... */
          struct tm nowtm;
          glong usec = 0;

          /* Just read the buffer data into a textual
             datestamp. */

          copy_string_fixed_buf(date, src, MIN(date_len, 16));
          src += 15;
          left -= 15;

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

          /* And also make struct time timestamp for the msg */

          /* we use a separate variable for the current timestamp as strptime on
           * solaris sometimes touches fields that are not in the format string
           * of strptime
           */
          cached_localtime(&now, &nowtm);
          tm = nowtm;
          p = (guchar *) strptime((gchar *) date, "%b %e %H:%M:%S", &tm);
          if (!p || (p && *p))
            goto error;

          /* In case of daylight saving let's assume that the message came under daylight saving also */
          tm.tm_isdst = nowtm.tm_isdst;
          tm.tm_year = nowtm.tm_year;
          if (tm.tm_mon > nowtm.tm_mon + 1)
            tm.tm_year--;
            
          /* NOTE: no timezone information in the message, assume it is local time */
          unnormalized_hour = tm.tm_hour;
          self->timestamps[LM_TS_STAMP].time.tv_sec = cached_mktime(&tm);
          self->timestamps[LM_TS_STAMP].time.tv_usec = usec;
        }
      else
        {
          goto error;
        }
    }
  else
    {
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
      self->timestamps[LM_TS_STAMP].zone_offset = get_local_timezone_ofs(self->timestamps[LM_TS_STAMP].time.tv_sec);
    }
  self->timestamps[LM_TS_STAMP].time.tv_sec = self->timestamps[LM_TS_STAMP].time.tv_sec +
                                              get_local_timezone_ofs(self->timestamps[LM_TS_STAMP].time.tv_sec) -
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
  if ((flags & LP_DONT_STORE_LEGACY_MSGHDR) == 0)
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
log_msg_parse_sd(LogMessage *self, const guchar **data, gint *length, guint flags)
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
  gsize sd_param_len;

  /* UTF-8 string */
  gchar sd_param_value[256];
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
          while (left && *src != ' ')
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
          strcpy(sd_value_name, sd_prefix);
          /* this strcat is safe, as sd_id_name is at most 32 chars */
          strncpy(sd_value_name + sd_prefix_len, sd_id_name, sizeof(sd_value_name) - sd_prefix_len);
          sd_value_name[sd_prefix_len + pos] = '.';

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
              sd_param_len = pos;
              strncpy(&sd_value_name[sd_prefix_len + 1 + sd_id_len], sd_param_name, sizeof(sd_value_name) - sd_prefix_len - 1 - sd_id_len);

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
              nv_registry_set_handle_flags(logmsg_registry, handle, (sd_id_len << 8) + LM_VF_SDATA);

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
 * log_msg_parse_syslog_proto:
 *
 * Parse a message according to the latest syslog-protocol drafts.
 **/
static gboolean
log_msg_parse_syslog_proto(LogMessage *self, const guchar *data, gint length, guint flags, glong assume_timezone, guint16 default_pri)
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
  guchar date[32];
  const guchar *hostname_start = NULL;
  gint hostname_len = 0;
  
  src = (guchar *) data;
  left = length;


  if (!log_msg_parse_pri(self, &src, &left, flags, default_pri) ||
      !log_msg_parse_version(self, &src, &left))
    {
      return FALSE;
    }

  if (!log_msg_parse_skip_space(self, &src, &left))
    return FALSE;
  
  /* ISO time format */
  if (!log_msg_parse_date(self, &src, &left, date, sizeof(date), flags, assume_timezone))
    return FALSE;

  if (!log_msg_parse_skip_space(self, &src, &left))
    return FALSE;
    
  /* hostname 255 ascii */
  log_msg_parse_hostname(self, &src, &left, &hostname_start, &hostname_len, flags, NULL);
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
  if (!log_msg_parse_sd(self, &src, &left, flags))
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
  else if ((flags & LP_VALIDATE_UTF8) && g_utf8_validate((gchar *) src, left, NULL))
    {
      self->flags |= LF_UTF8;
    }
  log_msg_set_value(self, LM_V_MESSAGE, (gchar *) src, left);
  return TRUE;
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
log_msg_parse_legacy(LogMessage *self,
                     const guchar *data, gint length,
                     guint flags,
                     regex_t *bad_hostname,
                     glong assume_timezone,
                     guint16 default_pri)
{
  const guchar *src;
  gint left;
  guchar date[32];
  
  src = (const guchar *) data;
  left = length;

  if (!log_msg_parse_pri(self, &src, &left, flags, default_pri))
    {
      return FALSE;
    }
    
  log_msg_parse_skip_chars(self, &src, &left, " ", -1);
  log_msg_parse_date(self, &src, &left, date, sizeof(date), flags, assume_timezone);
    
  if (date[0])
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

          if ((flags & LP_LOCAL) == 0)
            {
              /* Don't parse a hostname if it is local */
              /* It's a regular ol' message. */
              log_msg_parse_hostname(self, &src, &left, &hostname_start, &hostname_len, flags, bad_hostname);

              /* Skip whitespace. */
              log_msg_parse_skip_chars(self, &src, &left, " ", -1);
            }

          /* Try to extract a program name */
          log_msg_parse_legacy_program_name(self, &src, &left, flags);
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
      if ((self->flags & LF_INTERNAL) == 0 && ((self->pri & LOG_FACMASK) == LOG_KERN))
        {
          log_msg_set_value(self, LM_V_PROGRAM, "kernel", 6);
        }
      /* No, not a kernel message. */
      else
        {
          /* Capture the program name */
          log_msg_parse_legacy_program_name(self, &src, &left, flags);
        }
      self->timestamps[LM_TS_STAMP] = self->timestamps[LM_TS_RECVD];
    }

  log_msg_set_value(self, LM_V_MESSAGE, (gchar *) src, left);
  if ((flags & LP_VALIDATE_UTF8) && g_utf8_validate((gchar *) src, left, NULL))
    self->flags |= LF_UTF8;

  return TRUE;
}

static void
log_msg_parse(LogMessage *self,
              const guchar *data, gint length,
              guint flags,
              regex_t *bad_hostname,
              glong assume_timezone,
              guint16 default_pri)
{
  gboolean success;
  gchar *p;
  
  while (length > 0 && (data[length - 1] == '\n' || data[length - 1] == '\0'))
    length--;
    
  if (flags & LP_NOPARSE)
    {
      log_msg_set_value(self, LM_V_MESSAGE, (gchar *) data, length);
      self->pri = default_pri;
      return;
    }
  
  if (G_UNLIKELY(flags & LP_INTERNAL))
    self->flags |= LF_INTERNAL;
  if (flags & LP_LOCAL)
    self->flags |= LF_LOCAL;
  if (flags & LP_ASSUME_UTF8)
    self->flags |= LF_UTF8;

  self->initial_parse = TRUE;
  if (flags & LP_SYSLOG_PROTOCOL)
    success = log_msg_parse_syslog_proto(self, data, length, flags, assume_timezone, default_pri);
  else
    success = log_msg_parse_legacy(self, data, length, flags, bad_hostname, assume_timezone, default_pri);
  self->initial_parse = FALSE;

  if (G_UNLIKELY(!success))
    {
      gchar buf[2048];

      self->timestamps[LM_TS_STAMP] = self->timestamps[LM_TS_RECVD];
      log_msg_set_value(self, LM_V_HOST, "", 0);

      g_snprintf(buf, sizeof(buf), "Error processing log message: %.*s", length, data);
      log_msg_set_value(self, LM_V_MESSAGE, buf, -1);
      log_msg_set_value(self, LM_V_PROGRAM, "syslog-ng", 9);
      g_snprintf(buf, sizeof(buf), "%d", (int) getpid());
      log_msg_set_value(self, LM_V_PID, buf, -1);
      log_msg_set_value(self, LM_V_MSGID, "", 0);

      if (self->sdata)
        {
          g_free(self->sdata);
          self->alloc_sdata = self->num_sdata = 0;
          self->sdata = NULL;
        }
      self->pri = LOG_SYSLOG | LOG_ERR;
      return;
    }

  if (G_UNLIKELY(flags & LP_NO_MULTI_LINE))
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

gboolean
log_msg_append_tags_callback(LogMessage *self, guint32 log_msg_tag_index, guint32 tag_id, const gchar *name, gpointer user_data)
{
  GString *result = (GString *) ((gpointer *) user_data)[0];
  gint original_length = GPOINTER_TO_UINT(((gpointer *) user_data)[1]);

  g_assert(result);

  if (result->len > original_length)
    g_string_append_c(result, ',');

  g_string_append(result, name);
  return TRUE;
}

void
log_msg_print_tags(LogMessage *self, GString *result)
{
  gpointer args[] = { result, GUINT_TO_POINTER(result->len) };

  log_msg_tags_foreach(self, log_msg_append_tags_callback, args);
}

/**
 * log_msg_free:
 * @self: LogMessage instance
 *
 * Frees a LogMessage instance.
 **/
static void
log_msg_free(LogMessage *self)
{
  if (log_msg_chk_flag(self, LF_STATE_OWN_PAYLOAD) && self->payload)
    nv_table_unref(self->payload);
  if (log_msg_chk_flag(self, LF_STATE_OWN_TAGS) && self->tags)
    g_free(self->tags);

  if (log_msg_chk_flag(self, LF_STATE_OWN_SDATA) && self->sdata)
    g_free(self->sdata);
  if (log_msg_chk_flag(self, LF_STATE_OWN_SADDR))
    g_sockaddr_unref(self->saddr);

  if (self->original)
    log_msg_unref(self->original);

  g_free(self);
}

/**
 * log_msg_ref:
 * @self: LogMessage instance
 *
 * Increment reference count of @self and return the new reference.
 **/
LogMessage *
log_msg_ref(LogMessage *self)
{
  g_assert(g_atomic_counter_get(&self->ref_cnt) > 0);
  g_atomic_counter_inc(&self->ref_cnt);
  return self;
}

/**
 * log_msg_unref:
 * @self: LogMessage instance
 *
 * Decrement reference count and free self if the reference count becomes 0.
 **/
void
log_msg_unref(LogMessage *self)
{
  g_assert(g_atomic_counter_get(&self->ref_cnt) > 0);
  if (g_atomic_counter_dec_and_test(&self->ref_cnt))
    {
      log_msg_free(self);
    }
}

/**
 * log_msg_init:
 * @self: LogMessage instance
 * @saddr: sender address 
 *
 * This function initializes a LogMessage instance without allocating it
 * first. It is used internally by the log_msg_new function.
 **/
static void
log_msg_init(LogMessage *self, GSockAddr *saddr)
{
  g_atomic_counter_set(&self->ref_cnt, 1);
  cached_g_current_time(&self->timestamps[LM_TS_RECVD].time);
  self->timestamps[LM_TS_RECVD].zone_offset = get_local_timezone_ofs(self->timestamps[LM_TS_RECVD].time.tv_sec);
  self->timestamps[LM_TS_STAMP].time.tv_sec = -1;
  self->timestamps[LM_TS_STAMP].zone_offset = -1;
 
  self->sdata = NULL;
  self->saddr = g_sockaddr_ref(saddr);

  self->original = NULL;
  self->flags |= LF_STATE_OWN_MASK;
}

/**
 * log_msg_new:
 * @msg: message to parse
 * @length: length of @msg
 * @saddr: sender address
 * @flags: parse flags (LP_*)
 *
 * This function allocates, parses and returns a new LogMessage instance.
 **/
LogMessage *
log_msg_new(const gchar *msg, gint length,
            GSockAddr *saddr,
            guint flags,
            regex_t *bad_hostname,
            glong assume_timezone,
            guint16 default_pri)
{
  LogMessage *self = g_new0(LogMessage, 1);
  
  log_msg_init(self, saddr);
  self->payload = nv_table_new(LM_V_MAX, 16, MAX(length * 2, 256));

  log_msg_parse(self, (guchar *) msg, length, flags, bad_hostname, assume_timezone, default_pri);
  return self;
}

LogMessage *
log_msg_new_empty(void)
{
  LogMessage *self = g_new0(LogMessage, 1);
  
  log_msg_init(self, NULL);
  self->payload = nv_table_new(LM_V_MAX, 16, 256);
  return self;
}

/**
 * log_msg_ack:
 * @msg: LogMessage instance
 * @path_options: path specific options
 *
 * Indicate that the message was processed successfully and the sender can
 * queue further messages.
 **/
void 
log_msg_ack(LogMessage *msg, const LogPathOptions *path_options)
{
  if (path_options->flow_control)
    {
      if (g_atomic_counter_dec_and_test(&msg->ack_cnt))
        {
          msg->ack_func(msg, msg->ack_userdata);
        }
    }
}

void
log_msg_clone_ack(LogMessage *msg, gpointer user_data)
{
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

  g_assert(msg->original);
  path_options.flow_control = TRUE;
  log_msg_ack(msg->original, &path_options);
}

/*
 * log_msg_clone_cow:
 *
 * Clone a copy-on-write (cow) copy of a log message.
 */
LogMessage *
log_msg_clone_cow(LogMessage *msg, const LogPathOptions *path_options)
{
  LogMessage *self = g_new(LogMessage, 1);

  stats_counter_inc(count_msg_clones);
  if ((msg->flags & LF_STATE_OWN_MASK) == 0)
    {
      /* the message we're cloning has no original content, everything
       * is referenced from its "original", use that with this clone
       * as well, effectively avoiding the "referenced" flag on the
       * clone. */
      msg = msg->original;
    }
  msg->flags |= LF_STATE_REFERENCED;

  memcpy(self, msg, sizeof(*msg));

  /* every field _must_ be initialized explicitly if its direct
   * copying would cause problems (like copying a pointer by value) */

  /* reference the original message */
  self->original = log_msg_ref(msg);
  g_atomic_counter_set(&self->ref_cnt, 1);
  g_atomic_counter_set(&self->ack_cnt, 0);

  log_msg_add_ack(self, path_options);
  if (!path_options->flow_control)
    {
      self->ack_func  = NULL;
      self->ack_userdata = NULL;
    }
  else
    {
      self->ack_func = (LMAckFunc) log_msg_clone_ack;
      self->ack_userdata = NULL;
    }
  
  self->flags &= ~LF_STATE_MASK;
  return self;
}

/**
 * log_msg_new_internal:
 * @prio: message priority (LOG_*)
 * @msg: message text
 * @flags: parse flags (LP_*)
 *
 * This function creates a new log message for messages originating 
 * internally to syslog-ng
 **/
LogMessage *
log_msg_new_internal(gint prio, const gchar *msg, guint flags)
{
  gchar *buf;
  LogMessage *self;
  
  buf = g_strdup_printf("<%d> syslog-ng[%d]: %s\n", prio, (int) getpid(), msg);
  self = log_msg_new(buf, strlen(buf), NULL, flags | LP_INTERNAL, NULL, -1, prio);
  g_free(buf);

  return self;
}

/**
 * log_msg_new_mark:
 * 
 * This function returns a new MARK message. MARK messages have the LF_MARK
 * flag set.
 **/
LogMessage *
log_msg_new_mark(void)
{
  LogMessage *self = log_msg_new("-- MARK --", 10, NULL, LP_NOPARSE, NULL, -1, LOG_SYSLOG | LOG_INFO);
  self->flags = LF_LOCAL | LF_MARK | LF_INTERNAL;
  return self;
}

/**
 * log_msg_add_ack:
 * @m: LogMessage instance
 *
 * This function increments the number of required acknowledges.
 **/
void
log_msg_add_ack(LogMessage *msg, const LogPathOptions *path_options)
{
  if (path_options->flow_control)
    g_atomic_counter_inc(&msg->ack_cnt);
}

/**
 * log_msg_drop:
 * @msg: LogMessage instance
 * @path_options: path specific options
 *
 * This function is called whenever a destination driver feels that it is
 * unable to process this message. It acks and unrefs the message.
 **/
void
log_msg_drop(LogMessage *msg, const LogPathOptions *path_options)
{
  log_msg_ack(msg, path_options);
  log_msg_unref(msg);
}

void
log_msg_registry_init(void)
{
  gint i;

  logmsg_registry = nv_registry_new(builtin_value_names);
  nv_registry_add_alias(logmsg_registry, LM_V_MESSAGE, "MSG");
  nv_registry_add_alias(logmsg_registry, LM_V_MESSAGE, "MSGONLY");
  nv_registry_add_alias(logmsg_registry, LM_V_HOST, "FULLHOST");
  nv_registry_add_alias(logmsg_registry, LM_V_HOST_FROM, "FULLHOST_FROM");

  for (i = 0; macros[i].name; i++)
    {
      if (nv_registry_get_handle(logmsg_registry, macros[i].name) == 0)
        {
          NVHandle handle;

          handle = nv_registry_alloc_handle(logmsg_registry, macros[i].name);
          nv_registry_set_handle_flags(logmsg_registry, handle, (macros[i].id << 8) + LM_VF_MACRO);
        }
    }

  /* register $0 - $255 in order */
  for (i = 0; i < 255; i++)
    {
      gchar buf[8];

      g_snprintf(buf, sizeof(buf), "%d", i);
      match_handles[i] = nv_registry_alloc_handle(logmsg_registry, buf);
    }
}

void
log_msg_global_init(void)
{
  log_msg_registry_init();
  stats_register_counter(0, SCS_GLOBAL, "msg_clones", NULL, SC_TYPE_PROCESSED, &count_msg_clones);
  stats_register_counter(0, SCS_GLOBAL, "payload_reallocs", NULL, SC_TYPE_PROCESSED, &count_payload_reallocs);
  stats_register_counter(0, SCS_GLOBAL, "sdata_updates", NULL, SC_TYPE_PROCESSED, &count_sdata_updates);
}
