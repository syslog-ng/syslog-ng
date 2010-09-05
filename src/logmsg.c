/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

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
const char logmsg_sd_prefix[] = ".SDATA.";
const gint logmsg_sd_prefix_len = sizeof(logmsg_sd_prefix) - 1;
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
  if (strncmp(value_name, logmsg_sd_prefix, sizeof(logmsg_sd_prefix) - 1) == 0 && value_name[6])
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
log_msg_tags_foreach(LogMessage *self, LogMessageTagsForeachFunc callback, gpointer user_data)
{
  guint i, j, k;
  LogTagId tag_id;
  guint bitidx;
  gchar *name;

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

              for (bitidx = 0; bitidx != 8; ++bitidx)
                {
                  if ( *(((guint8*) (&self->tags[i])) + swap_index_big_endian(j) * 2 +swap_index_big_endian(k)) & (1 << bitidx))
                    {
                      tag_id = (LogTagId) (i * 32  + j * 16 + k * 8 + bitidx);

                      name = log_tags_get_by_id(tag_id);
                      callback(self, tag_id, name, user_data);
                    }
                }
            }
        }
    }
  return TRUE;
}

static inline void
log_msg_set_tag_by_id_onoff(LogMessage *self, LogTagId id, gboolean on)
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
log_msg_set_tag_by_id(LogMessage *self, LogTagId id)
{
  log_msg_set_tag_by_id_onoff(self, id, TRUE);
}

void
log_msg_set_tag_by_name(LogMessage *self, const gchar *name)
{
  log_msg_set_tag_by_id_onoff(self, log_tags_get_by_name(name), TRUE);
}

void
log_msg_clear_tag_by_id(LogMessage *self, LogTagId id)
{
  log_msg_set_tag_by_id_onoff(self, id, FALSE);
}

void
log_msg_clear_tag_by_name(LogMessage *self, const gchar *name)
{
  log_msg_set_tag_by_id_onoff(self, log_tags_get_by_name(name), FALSE);
}

gboolean
log_msg_is_tag_by_id(LogMessage *self, LogTagId id)
{
  if (G_UNLIKELY(8159 < id))
    {
      msg_error("Invalid tag", evt_tag_int("id", (gint) id), NULL);
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

gboolean
log_msg_append_tags_callback(LogMessage *self, LogTagId tag_id, const gchar *name, gpointer user_data)
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

void
log_msg_clear(LogMessage *self)
{
  if (log_msg_chk_flag(self, LF_STATE_OWN_PAYLOAD))
    nv_table_clear(self->payload);
  else
    self->payload = nv_table_new(LM_V_MAX, 16, 256);

  if (log_msg_chk_flag(self, LF_STATE_OWN_TAGS) && self->tags)
    {
      memset(self->tags, 0, self->num_tags * sizeof(self->tags[0]));
    }
  else
    self->tags = NULL;

  self->num_matches = 0;
  /* alloc_sdata remains */
  self->num_sdata = 0;

  if (log_msg_chk_flag(self, LF_STATE_OWN_SADDR) && self->saddr)
    {
      g_sockaddr_unref(self->saddr);
    }
  self->saddr = NULL;

  if (self->original)
    {
      log_msg_unref(self->original);
      self->original = NULL;
    }
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
            MsgFormatOptions *parse_options)
{
  LogMessage *self = g_new0(LogMessage, 1);
  
  log_msg_init(self, saddr);
  self->payload = nv_table_new(LM_V_MAX, 16, MAX(length * 2, 256));

  if (G_LIKELY(parse_options->format_handler))
    {
      parse_options->format_handler->parse(parse_options, (guchar *) msg, length, self);
    }
  else
    {
      log_msg_set_value(self, LM_V_MESSAGE, "Error parsing message, format module is not loaded", -1);
    }
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
log_msg_new_internal(gint prio, const gchar *msg)
{
  gchar buf[32];
  LogMessage *self;
  
  g_snprintf(buf, sizeof(buf), "%d", (int) getpid());
  self = log_msg_new_empty();
  log_msg_set_value(self, LM_V_PROGRAM, "syslog-ng", 9);
  log_msg_set_value(self, LM_V_PID, buf, -1);
  log_msg_set_value(self, LM_V_MESSAGE, msg, -1);
  self->pri = prio;
  self->flags = LF_INTERNAL | LF_LOCAL;

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
  LogMessage *self = log_msg_new_empty();

  log_msg_set_value(self, LM_V_MESSAGE, "-- MARK --", 10);
  self->pri = LOG_SYSLOG | LOG_INFO;
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
log_msg_registry_deinit(void)
{
  nv_registry_free(logmsg_registry);
  logmsg_registry = NULL;
}

void
log_msg_global_init(void)
{
  log_msg_registry_init();
  stats_register_counter(0, SCS_GLOBAL, "msg_clones", NULL, SC_TYPE_PROCESSED, &count_msg_clones);
  stats_register_counter(0, SCS_GLOBAL, "payload_reallocs", NULL, SC_TYPE_PROCESSED, &count_payload_reallocs);
  stats_register_counter(0, SCS_GLOBAL, "sdata_updates", NULL, SC_TYPE_PROCESSED, &count_sdata_updates);
}

void
log_msg_global_deinit(void)
{
  log_msg_registry_deinit();
}
