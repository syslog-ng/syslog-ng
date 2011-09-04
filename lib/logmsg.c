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
#include "tls-support.h"

#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

/*
 * Reference/ACK counting for LogMessage structures
 *
 * Each LogMessage structure is allocated when received by a LogSource
 * instance, and then freed once all destinations finish with it.  Since a
 * LogMessage is processed by different threads, reference counting must be
 * atomic.
 *
 * A similar counter is used to track when a given message is considered to
 * be delivered.  In case flow-control is in use, the number of
 * to-be-expected ACKs are counted in an atomic variable.
 *
 * Code is written in a way, that it is explicitly mentioned whether a
 * function expects a reference (called "consuming" the ref), or whether it
 * gets a 'borrowed' reference (more common).  Also, a function that returns
 * a LogMessage instance generally returns a reference too.
 *
 * Because of these rules, quite a number of refs/unrefs are being made
 * during the processing of a message, even though they wouldn't cause the
 * structure to be freed.  Considering that ref/unref operations are
 * expensive atomic operations, these have a considerable overhead.
 *
 * The solution we employ in this module, is that we try to exploit the
 * fact, that each thread is usually working on a single LogMessage
 * instance, and once it is done with it, it fetches the next one, and so
 * on.  In the LogReader/LogWriter cases, this usage pattern is quite
 * normal.
 *
 * Also, a quite similar usage pattern can be applied to the ACK counter
 * (the one which tracks how much flow-controlled destinations need to
 * confirm the deliver of the message).
 *
 * The solution implemented here is outlined below (and the same rules are
 * used for ACKs and REFs):
 *
 *    - ACKs and REFs are put into the same 32 bit integer value, one half
 *      is used for the ref counter, the other is used for the ACK counter
 *      (see the macros LOGMSG_REFCACHE_ below)
 *
 *    - The processing of a given message in a given thread is enclosed by
 *      calls to log_msg_refcache_start() / log_msg_refcache_stop()
 *
 *    - The calls to these functions is optional, if they are not called,
 *      then normal atomic reference counting is performed
 *
 *    - When refcache_start() is called, the atomic reference count is not
 *      changed by log_msg_ref()/unref(). Rather, a per-thread variable is
 *      used to count the _difference_ to the "would-be" value of the ref counter.
 *
 *    - When refcache_stop() is called, the atomic reference counter is
 *      updated as a single atomic operation. This means, that if a thread
 *      calls refcache_start() early enough, then the ref/unref operations
 *      performed by the given thread can be completely atomic-op free.
 *
 *    - There's one catch: if the producer thread (e.g. LogReader), doesn't
 *      add references to a consumer thread (e.g.  LogWriter), and the
 *      consumer doesn't use the refcache infrastructure, then the counters
 *      could become negative (simply because the reader's real number of
 *      references is not represented in the refcounter).  This is solved by
 *      adding a large-enough number (so called BIAS) to the ref counter in
 *      refcache_start(), which ensures that all possible writers will see a
 *      positive value.  This is then substracted in refcache_stop() the
 *      same way as the other references.
 *
 * Since we use the same atomic variable to store two things, updating that
 * counter becomes somewhat more complicated, therefore a g_atomic_int_add()
 * doesn't suffice.  We're using a CAS loop (compare-and-exchange) to do our
 * stuff, but that shouldn't have that much of an overhead.
 */

TLS_BLOCK_START
{
  /* message that is being processed by the current thread. Its ack/ref changes are cached */
  LogMessage *logmsg_current;
  /* whether the consumer is flow-controlled, (the producer always is) */
  gboolean logmsg_cached_ack_needed;

  /* has to be signed as these can become negative */
  /* number of cached refs by the current thread */
  gint logmsg_cached_refs;
  /* number of cached acks by the current thread */
  gint logmsg_cached_acks;
}
TLS_BLOCK_END;

#define logmsg_current              __tls_deref(logmsg_current)
#define logmsg_cached_refs          __tls_deref(logmsg_cached_refs)
#define logmsg_cached_acks          __tls_deref(logmsg_cached_acks)
#define logmsg_cached_ack_needed    __tls_deref(logmsg_cached_ack_needed)

#define LOGMSG_REFCACHE_BIAS                  0x00004000 /* the BIAS we add to the ref counter in refcache_start */
#define LOGMSG_REFCACHE_ACK_SHIFT                     16 /* number of bits to shift to get the ACK counter */
#define LOGMSG_REFCACHE_ACK_MASK              0xFFFF0000 /* bit mask to extract the ACK counter */
#define LOGMSG_REFCACHE_REF_SHIFT                      0 /* number of bits to shift to get the REF counter */
#define LOGMSG_REFCACHE_REF_MASK              0x0000FFFF /* bit mask to extract the ACK counter */


#define LOGMSG_REFCACHE_REF_TO_VALUE(x)    (((x) << LOGMSG_REFCACHE_REF_SHIFT) & LOGMSG_REFCACHE_REF_MASK)
#define LOGMSG_REFCACHE_ACK_TO_VALUE(x)    (((x) << LOGMSG_REFCACHE_ACK_SHIFT) & LOGMSG_REFCACHE_ACK_MASK)

#define LOGMSG_REFCACHE_VALUE_TO_REF(x)    (((x) & LOGMSG_REFCACHE_REF_MASK) >> LOGMSG_REFCACHE_REF_SHIFT)
#define LOGMSG_REFCACHE_VALUE_TO_ACK(x)    (((x) & LOGMSG_REFCACHE_ACK_MASK) >> LOGMSG_REFCACHE_ACK_SHIFT)


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

static NVHandle match_handles[256];
NVRegistry *logmsg_registry;
const char logmsg_sd_prefix[] = ".SDATA.";
const gint logmsg_sd_prefix_len = sizeof(logmsg_sd_prefix) - 1;
gint logmsg_queue_node_max = 1;
/* statistics */
static StatsCounterItem *count_msg_clones;
static StatsCounterItem *count_payload_reallocs;
static StatsCounterItem *count_sdata_updates;
static GStaticPrivate priv_macro_value = G_STATIC_PRIVATE_INIT;

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

static void
__free_macro_value(void *val)
{
  g_string_free((GString *) val, TRUE);
}

const gchar *
log_msg_get_macro_value(LogMessage *self, gint id, gssize *value_len)
{
  GString *value;

  value = g_static_private_get(&priv_macro_value);
  if (!value)
    {
      value = g_string_sized_new(256);
      g_static_private_set(&priv_macro_value, value, __free_macro_value);
    }
  g_string_truncate(value, 0);

  log_macro_expand(value, id, FALSE, NULL, LTZ_LOCAL, 0, NULL, self);
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

gboolean
log_msg_is_handle_sdata(NVHandle handle)
{
  guint16 flags;

  flags = nv_registry_get_handle_flags(logmsg_registry, handle);
  return !!(flags & LM_VF_SDATA);
}

gboolean
log_msg_is_handle_match(NVHandle handle)
{
  g_assert(match_handles[0] && match_handles[255] && match_handles[0] < match_handles[255]);

  /* NOTE: match_handles are allocated sequentially in log_msg_registry_init(),
   * so this simple & fast check is enough */
  return (match_handles[0] <= handle && handle <= match_handles[255]);
}

static void
log_msg_init_queue_node(LogMessage *msg, LogMessageQueueNode *node, const LogPathOptions *path_options)
{
  INIT_LIST_HEAD(&node->list);
  node->ack_needed = path_options->ack_needed;
  node->msg = log_msg_ref(msg);
  msg->flags |= LF_STATE_REFERENCED;
}

/*
 * Allocates a new LogMessageQueueNode instance to be enqueued in a
 * LogQueue.
 *
 * NOTE: Assumed to be runnning in the source thread, and that the same
 * LogMessage instance is only put into queue from the same thread (e.g. 
 * the related fields are _NOT_ locked).
 */
LogMessageQueueNode *
log_msg_alloc_queue_node(LogMessage *msg, const LogPathOptions *path_options)
{
  LogMessageQueueNode *node;

  if (msg->cur_node < msg->num_nodes)
    {
      node = &msg->nodes[msg->cur_node++];
      node->embedded = TRUE;
    }
  else
    {
      gint nodes = (volatile gint) logmsg_queue_node_max;

      /* this is a racy update, but it doesn't really hurt if we lose an
       * update or if we continue with a smaller value in parallel threads
       * for some time yet, since the smaller number only means that we
       * pre-allocate somewhat less LogMsgQueueNodes in the message
       * structure, but will be fine regardless (if we'd overflow the
       * pre-allocated space, we start allocating nodes dynamically from
       * heap.
       */
      if (nodes < 32 && nodes <= msg->num_nodes)
        logmsg_queue_node_max = msg->num_nodes + 1;
      node = g_slice_new(LogMessageQueueNode);
      node->embedded = FALSE;
    }
  log_msg_init_queue_node(msg, node, path_options);
  return node;
}

LogMessageQueueNode *
log_msg_alloc_dynamic_queue_node(LogMessage *msg, const LogPathOptions *path_options)
{
  LogMessageQueueNode *node;
  node = g_slice_new(LogMessageQueueNode);
  node->embedded = FALSE;
  log_msg_init_queue_node(msg, node, path_options);
  return node;
}

void
log_msg_free_queue_node(LogMessageQueueNode *node)
{
  if (!node->embedded)
    g_slice_free(LogMessageQueueNode, node);
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
      NVTable *payload = self->payload;

      self->payload = nv_table_clone(payload, name_len + value_len + 2);
      log_msg_set_flag(self, LF_STATE_OWN_PAYLOAD);
      nv_table_unref(payload);
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
  if (handle == LM_V_PROGRAM)
    log_msg_unset_flag(self, LF_LEGACY_MSGHDR);
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

#if GLIB_SIZEOF_LONG != GLIB_SIZEOF_VOID_P
#error "The tags bit array assumes that long is the same size as the pointer"
#endif

#if GLIB_SIZEOF_LONG == 8
#define LOGMSG_TAGS_NDX_SHIFT 6
#define LOGMSG_TAGS_NDX_MASK  0x3F
#define LOGMSG_TAGS_BITS      64
#elif GLIB_SIZEOF_LONG == 4
#define LOGMSG_TAGS_NDX_SHIFT 5
#define LOGMSG_TAGS_NDX_MASK  0x1F
#define LOGMSG_TAGS_BITS      32
#else
#error "Unsupported word length, only 32 or 64 bit platforms are supported"
#endif

static inline void
log_msg_tags_foreach_item(LogMessage *self, gint base, gulong item, LogMessageTagsForeachFunc callback, gpointer user_data)
{
  gint i;

  for (i = 0; i < LOGMSG_TAGS_BITS; i++)
    {
      if (G_LIKELY(!item))
        return;
      if (item & 1)
        {
          LogTagId id = (LogTagId) base + i;

          callback(self, id, log_tags_get_by_id(id), user_data);
        }
      item >>= 1;
    }
}


void
log_msg_tags_foreach(LogMessage *self, LogMessageTagsForeachFunc callback, gpointer user_data)
{
  guint i;

  if (self->num_tags == 0)
    {
      log_msg_tags_foreach_item(self, 0, (gulong) self->tags, callback, user_data);
    }
  else
    {
      for (i = 0; i != self->num_tags; ++i)
        {
          log_msg_tags_foreach_item(self, i * LOGMSG_TAGS_BITS, self->tags[i], callback, user_data);
        }
    }
}


static inline void
log_msg_set_bit(gulong *tags, gint index, gboolean value)
{
  if (value)
    tags[index >> LOGMSG_TAGS_NDX_SHIFT] |= ((gulong) (1UL << (index & LOGMSG_TAGS_NDX_MASK)));
  else
    tags[index >> LOGMSG_TAGS_NDX_SHIFT] &= ~((gulong) (1UL << (index & LOGMSG_TAGS_NDX_MASK)));
}

static inline gboolean
log_msg_get_bit(gulong *tags, gint index)
{
  return !!(tags[index >> LOGMSG_TAGS_NDX_SHIFT] & ((gulong) (1UL << (index & LOGMSG_TAGS_NDX_MASK))));
}

static inline void
log_msg_set_tag_by_id_onoff(LogMessage *self, LogTagId id, gboolean on)
{
  gulong *old_tags;
  gint old_num_tags;
  gboolean inline_tags;

  if (!log_msg_chk_flag(self, LF_STATE_OWN_TAGS) && self->num_tags)
    {
      self->tags = g_memdup(self->tags, sizeof(self->tags[0]) * self->num_tags);
    }
  log_msg_set_flag(self, LF_STATE_OWN_TAGS);

  /* if num_tags is 0, it means that we use inline storage of tags */
  inline_tags = self->num_tags == 0;
  if (inline_tags && id < LOGMSG_TAGS_BITS)
    {
      /* store this tag inline */
      log_msg_set_bit((gulong *) &self->tags, id, on);
    }
  else
    {
      /* we can't put this tag inline, either because it is too large, or we don't have the inline space any more */

      if ((self->num_tags * LOGMSG_TAGS_BITS) <= id)
        {
          if (G_UNLIKELY(8159 < id))
            {
              msg_error("Maximum number of tags reached", NULL);
              return;
            }
          old_num_tags = self->num_tags;
          self->num_tags = (id / LOGMSG_TAGS_BITS) + 1;

          old_tags = self->tags;
          if (old_num_tags)
            self->tags = g_realloc(self->tags, sizeof(self->tags[0]) * self->num_tags);
          else
            self->tags = g_malloc(sizeof(self->tags[0]) * self->num_tags);
          memset(&self->tags[old_num_tags], 0, (self->num_tags - old_num_tags) * sizeof(self->tags[0]));

          if (inline_tags)
            self->tags[0] = (gulong) old_tags;
        }

      log_msg_set_bit(self->tags, id, on);
    }
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
  if (self->num_tags == 0 && id < LOGMSG_TAGS_BITS)
    return log_msg_get_bit((gulong *) &self->tags, id);
  else if (id < self->num_tags * LOGMSG_TAGS_BITS)
    return log_msg_get_bit(self->tags, id);
  else
    return FALSE;
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
          if (dot - sdata_name != sdata_name_len)
            {
              g_assert((dot - sdata_name < sdata_name_len) && *dot == '.');
            }
          else
            {
              /* Standalone sdata e.g. [[UserData.updatelist@18372.4]] */
              dot = NULL;
            }
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
          sdata_param = "";
          sdata_param_len = 0;
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
      if (sdata_param_len)
        {
          g_string_append_c(result, ' ');
          g_string_append_len(result, sdata_param, sdata_param_len);
          g_string_append(result, "=\"");
          value = log_msg_get_value(self, handle, &len);
          log_msg_sdata_append_escaped(result, value, len);
          g_string_append_c(result, '"');
        }
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
  GTimeVal tv;

  /* ref is set to 1, ack is set to 0 */
  self->ack_and_ref = LOGMSG_REFCACHE_REF_TO_VALUE(1);
  cached_g_current_time(&tv);
  self->timestamps[LM_TS_RECVD].tv_sec = tv.tv_sec;
  self->timestamps[LM_TS_RECVD].tv_usec = tv.tv_usec;
  self->timestamps[LM_TS_RECVD].zone_offset = get_local_timezone_ofs(self->timestamps[LM_TS_RECVD].tv_sec);
  self->timestamps[LM_TS_STAMP].tv_sec = -1;
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

static inline LogMessage *
log_msg_alloc(gsize payload_size)
{
  LogMessage *msg;
  gsize payload_space = payload_size ? nv_table_get_alloc_size(LM_V_MAX, 16, payload_size) : 0;
  gsize alloc_size, payload_ofs = 0;

  /* NOTE: logmsg_node_max is updated from parallel threads without locking. */
  gint nodes = (volatile gint) logmsg_queue_node_max;

  alloc_size = sizeof(LogMessage) + sizeof(LogMessageQueueNode) * nodes;
  /* align to 8 boundary */
  if (payload_size)
    {
      alloc_size = (alloc_size + 7) & ~7;
      payload_ofs = alloc_size;
      alloc_size += payload_space;
    }
  msg = g_malloc(alloc_size);

  memset(msg, 0, sizeof(LogMessage));

  if (payload_size)
    msg->payload = nv_table_init_borrowed(((gchar *) msg) + payload_ofs, payload_space, LM_V_MAX);

  msg->num_nodes = nodes;
  return msg;
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
  LogMessage *self = log_msg_alloc(length * 2);
  
  log_msg_init(self, saddr);

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
  LogMessage *self = log_msg_alloc(256);
  
  log_msg_init(self, NULL);
  return self;
}


void
log_msg_clone_ack(LogMessage *msg, gpointer user_data)
{
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

  g_assert(msg->original);
  path_options.ack_needed = TRUE;
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
  LogMessage *self = log_msg_alloc(0);

  stats_counter_inc(count_msg_clones);
  if ((msg->flags & LF_STATE_OWN_MASK) == 0 || ((msg->flags & LF_STATE_OWN_MASK) == LF_STATE_OWN_TAGS && msg->num_tags == 0))
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
  self->ack_and_ref = LOGMSG_REFCACHE_REF_TO_VALUE(1) + LOGMSG_REFCACHE_ACK_TO_VALUE(0);
  self->cur_node = 0;

  log_msg_add_ack(self, path_options);
  if (!path_options->ack_needed)
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

  if (self->num_tags == 0)
    self->flags |= LF_STATE_OWN_TAGS;
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
  self->flags |= LF_INTERNAL | LF_LOCAL;

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
  self->flags |= LF_LOCAL | LF_MARK | LF_INTERNAL;
  return self;
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
  if (log_msg_chk_flag(self, LF_STATE_OWN_TAGS) && self->tags && self->num_tags > 0)
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


/***************************************************************************************
 * In order to read & understand this code, reading the comment on the top
 * of this file about ref/ack handling is strongly recommended.
 ***************************************************************************************/

/* Function to update the combined ACK and REF counter. */
static inline gint
log_msg_update_ack_and_ref(LogMessage *self, gint add_ref, gint add_ack)
{
  gint old_value, new_value;

  do
    {
      new_value = old_value = (volatile gint) self->ack_and_ref;
      new_value = (new_value & ~LOGMSG_REFCACHE_REF_MASK) + LOGMSG_REFCACHE_REF_TO_VALUE((LOGMSG_REFCACHE_VALUE_TO_REF(old_value) + add_ref));
      new_value = (new_value & ~LOGMSG_REFCACHE_ACK_MASK) + LOGMSG_REFCACHE_ACK_TO_VALUE((LOGMSG_REFCACHE_VALUE_TO_ACK(old_value) + add_ack));
    }
  while (!g_atomic_int_compare_and_exchange(&self->ack_and_ref, old_value, new_value));
  return old_value;
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
  gint old_value;

  if (G_LIKELY(logmsg_current == self))
    {
      /* fastpath, @self is the current message, ref/unref processing is
       * delayed until log_msg_refcache_stop() is called */

      logmsg_cached_refs++;
      return self;
    }

  /* slow path, refcache is not used, do the ordinary way */
  old_value = log_msg_update_ack_and_ref(self, 1, 0);
  g_assert(LOGMSG_REFCACHE_VALUE_TO_REF(old_value) >= 1);
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
  gint old_value;

  if (G_LIKELY(logmsg_current == self))
    {
      /* fastpath, @self is the current message, ref/unref processing is
       * delayed until log_msg_refcache_stop() is called */

      logmsg_cached_refs--;
      return;
    }

  old_value = log_msg_update_ack_and_ref(self, -1, 0);
  g_assert(LOGMSG_REFCACHE_VALUE_TO_REF(old_value) >= 1);

  if (LOGMSG_REFCACHE_VALUE_TO_REF(old_value) == 1)
    {
      log_msg_free(self);
    }
}


/**
 * log_msg_add_ack:
 * @m: LogMessage instance
 *
 * This function increments the number of required acknowledges.
 **/
void
log_msg_add_ack(LogMessage *self, const LogPathOptions *path_options)
{
  if (path_options->ack_needed)
    {
      if (G_LIKELY(logmsg_current == self))
        {
          /* fastpath, @self is the current message, add_ack/ack processing is
           * delayed until log_msg_refcache_stop() is called */

          logmsg_cached_acks++;
          return;
        }
      log_msg_update_ack_and_ref(self, 0, 1);
    }
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
log_msg_ack(LogMessage *self, const LogPathOptions *path_options)
{
  gint old_value;

  if (path_options->ack_needed)
    {
      if (G_LIKELY(logmsg_current == self))
        {
          /* fastpath, @self is the current message, add_ack/ack processing is
           * delayed until log_msg_refcache_stop() is called */

          logmsg_cached_acks--;
          return;
        }

      old_value = log_msg_update_ack_and_ref(self, 0, -1);
      if (LOGMSG_REFCACHE_VALUE_TO_ACK(old_value) == 1)
        {
          self->ack_func(self, self->ack_userdata);
        }
    }
}

/*
 * Break out of an acknowledgement chain. The incoming message is
 * ACKed and a new path options structure is returned that can be used
 * to send to further consuming pipes.
 */
const LogPathOptions *
log_msg_break_ack(LogMessage *msg, const LogPathOptions *path_options, LogPathOptions *local_options)
{
  /* NOTE: in case the user requested flow control, we can't break the
   * ACK chain, as that would lead to early acks, that would cause
   * message loss */

  g_assert(!path_options->flow_control_requested);

  log_msg_ack(msg, path_options);

  *local_options = *path_options;
  local_options->ack_needed = FALSE;

  return local_options;
}


/*
 * Start caching ref/unref/ack/add-ack operations in the current thread for
 * the message specified by @self.  See the comment at the top of this file
 * for more information.
 *
 * This function is to be called by the producer thread (e.g. the one
 * that generates new messages). You should use
 * log_msg_refcache_start_consumer() in consumer threads instead.
 *
 * This function cannot be called for the same message from multiple
 * threads.
 *
 */
void
log_msg_refcache_start_producer(LogMessage *self)
{
  g_assert(logmsg_current == NULL);

  logmsg_current = self;
  /* we're the producer of said message, and thus we want to inhibit
   * freeing/acking it due to our cached refs, add a bias large enough
   * to cover any possible unrefs/acks of the consumer side */

  /* we don't need to be thread-safe here, as a producer has just created this message and no parallel access is yet possible */

  self->ack_and_ref = (self->ack_and_ref & ~LOGMSG_REFCACHE_REF_MASK) + LOGMSG_REFCACHE_REF_TO_VALUE((LOGMSG_REFCACHE_VALUE_TO_REF(self->ack_and_ref) + LOGMSG_REFCACHE_BIAS));
  self->ack_and_ref = (self->ack_and_ref & ~LOGMSG_REFCACHE_ACK_MASK) + LOGMSG_REFCACHE_ACK_TO_VALUE((LOGMSG_REFCACHE_VALUE_TO_ACK(self->ack_and_ref) + LOGMSG_REFCACHE_BIAS));
  logmsg_cached_refs = -LOGMSG_REFCACHE_BIAS;
  logmsg_cached_acks = -LOGMSG_REFCACHE_BIAS;
  logmsg_cached_ack_needed = TRUE;
}

/*
 * Start caching ref/unref/ack/add-ack operations in the current thread for
 * the message specified by @self.  See the comment at the top of this file
 * for more information.
 *
 * This function is to be called by the consumer threads (e.g. the
 * ones that consume messages).
 *
 * This function can be called from mutliple consumer threads at the
 * same time, even for the same message.
 *
 */
void
log_msg_refcache_start_consumer(LogMessage *self, const LogPathOptions *path_options)
{
  g_assert(logmsg_current == NULL);

  logmsg_current = self;
  logmsg_cached_ack_needed = path_options->ack_needed;
  logmsg_cached_refs = 0;
  logmsg_cached_acks = 0;
}


/*
 * Stop caching ref/unref/ack/add-ack operations in the current thread for
 * the message specified by the log_msg_refcache_start() function.
 *
 * See the comment at the top of this file for more information.
 */
void
log_msg_refcache_stop(void)
{
  gint old_value;
  gint current_cached_refs, current_cached_acks;

  g_assert(logmsg_current != NULL);

  /* validate that we didn't overflow the counters:
   *
   * Both counters must be:
   *
   * - at least 1 smaller than the bias, rationale:
   *
   *      - if we are caching "bias" number of refs, it may happen
   *        that there are bias number of unrefs, potentially running
   *        in consumer threads
   *
   *      - if the potential unrefs is larger than the bias value, it may
   *        happen that the producer sets the bias (trying to avoid
   *        the freeing of the LogMessage), but still it gets freed.
   *
   * - not smaller than the "-bias" value, rationale:
   *      - if we are caching "bias" number of unrefs the same can happen
   *        as with the ref case.
   *
   */
  g_assert((logmsg_cached_acks < LOGMSG_REFCACHE_BIAS - 1) && (logmsg_cached_acks >= -LOGMSG_REFCACHE_BIAS));
  g_assert((logmsg_cached_refs < LOGMSG_REFCACHE_BIAS - 1) && (logmsg_cached_refs >= -LOGMSG_REFCACHE_BIAS));


  /* save the differences in local variables to make it possible to know
   * that the ACK handler recursively changed them */

  current_cached_refs = logmsg_cached_refs;
  logmsg_cached_refs = 0;
  current_cached_acks = logmsg_cached_acks;
  logmsg_cached_acks = 0;
  old_value = log_msg_update_ack_and_ref(logmsg_current, current_cached_refs, current_cached_acks);

  if ((LOGMSG_REFCACHE_VALUE_TO_ACK(old_value) == -current_cached_acks) && logmsg_cached_ack_needed)
    {
      /* ack processing */
      logmsg_current->ack_func(logmsg_current, logmsg_current->ack_userdata);
    }

  /* we need to process the ref count difference in two steps:
   *   1) we add in the difference that was present when entering the function,
   *   2) we add in the difference that was created by the ACK callback
   */

  if (LOGMSG_REFCACHE_VALUE_TO_REF(old_value) == -current_cached_refs)
    {
      /* NOTE: if we already decided that this message is to be freed, then
       * the ACK handler may not have done additional ref/unref operations (above)
       */
      g_assert(logmsg_cached_refs == 0);
      log_msg_free(logmsg_current);
    }
  else if (logmsg_cached_refs != 0)
    {
      /* process ref count offset that the ack func changed, atomically */
      old_value = log_msg_update_ack_and_ref(logmsg_current, logmsg_cached_refs, 0);

      if (LOGMSG_REFCACHE_VALUE_TO_REF(old_value) == -logmsg_cached_refs)
        log_msg_free(logmsg_current);
    }
  logmsg_current = NULL;
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
  stats_lock();
  stats_register_counter(0, SCS_GLOBAL, "msg_clones", NULL, SC_TYPE_PROCESSED, &count_msg_clones);
  stats_register_counter(0, SCS_GLOBAL, "payload_reallocs", NULL, SC_TYPE_PROCESSED, &count_payload_reallocs);
  stats_register_counter(0, SCS_GLOBAL, "sdata_updates", NULL, SC_TYPE_PROCESSED, &count_sdata_updates);
  stats_unlock();
}

void
log_msg_global_deinit(void)
{
  log_msg_registry_deinit();
}
