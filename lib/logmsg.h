/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2013 Balázs Scheidler
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
  
#ifndef LOGMSG_H_INCLUDED
#define LOGMSG_H_INCLUDED

#include "syslog-ng.h"
#include "gsockaddr.h"
#include "atomic.h"
#include "serialize.h"
#include "logstamp.h"
#include "nvtable.h"
#include "msg-format.h"
#include "tags.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <iv_list.h>

typedef struct _LogPathOptions LogPathOptions;

typedef void (*LMAckFunc)(LogMessage *lm, gpointer user_data);

#define RE_MAX_MATCHES 256

#define LM_TS_STAMP 0
#define LM_TS_RECVD 1
#define LM_TS_MAX   2

/* builtin values */
enum
{
  LM_V_NONE,
  LM_V_HOST,
  LM_V_HOST_FROM,
  LM_V_MESSAGE,
  LM_V_PROGRAM,
  LM_V_PID,
  LM_V_MSGID,
  LM_V_SOURCE,
  LM_V_LEGACY_MSGHDR,

  /* NOTE: this is used as the number of "statically" allocated elements in
   * an NVTable.  NVTable may impose restrictions on this value (for
   * instance had to be an even number earlier).  So be sure to validate
   * whether LM_V_MAX would fit NVTable if you add further enums here.
   */

  LM_V_MAX,
};

enum
{
  LM_VF_SDATA = 0x0001,
  LM_VF_MATCH = 0x0002,
  LM_VF_MACRO = 0x0004,
};

enum
{
  /* these flags also matter when the message is serialized */
  LF_OLD_UNPARSED      = 0x0001,
  /* message payload is guaranteed to be valid utf8 */
  LF_UTF8              = 0x0001,
  /* message was generated from within syslog-ng, doesn't matter if it came from the internal() source */
  LF_INTERNAL          = 0x0002,
  /* message was received on a local transport, e.g. it was generated on the local machine */
  LF_LOCAL             = 0x0004,
  /* message is a MARK mode */
  LF_MARK              = 0x0008,

  /* state flags that only matter during syslog-ng runtime and never
   * when a message is serialized */
  LF_STATE_MASK        = 0xFFF0,
  LF_STATE_OWN_PAYLOAD = 0x0010,
  LF_STATE_OWN_SADDR   = 0x0020,
  LF_STATE_OWN_TAGS    = 0x0040,
  LF_STATE_OWN_SDATA   = 0x0080,
  LF_STATE_OWN_MASK    = 0x00F0,

  LF_CHAINED_HOSTNAME  = 0x00010000,

  /* originally parsed from RFC 3164 format and the legacy message header
   * was saved in $LEGACY_MSGHDR. This flag is a hack to avoid a hash lookup
   * in the fast path and indicates that the parser has saved the legacy
   * message header intact in a value named LEGACY_MSGHDR.
   */
  LF_LEGACY_MSGHDR    = 0x00020000,
};

typedef struct _LogMessageQueueNode
{
  struct iv_list_head list;
  LogMessage *msg;
  gboolean ack_needed:1, embedded:1;
} LogMessageQueueNode;


/* NOTE: the members are ordered according to the presumed use frequency. 
 * The structure itself is 2 cachelines, the border is right after the "msg"
 * member */
struct _LogMessage
{
  /* if you change any of the fields here, be sure to adjust
   * log_msg_clone_cow() as well to initialize fields properly */

  /* ack_and_ref is a 32 bit integer that is accessed in an atomic way. The
   * upper half contains the ACK count, the lower half the REF count.  It is
   * not a GAtomicCounter as due to ref/ack caching it has a lot of magic
   * behind its implementation.  See the logmsg.c file, around
   * log_msg_ref/unref.
   */

  /* FIXME: the structure has holes, but right now it's 1 byte short to make
   * it smaller (it is possible to create a 7 byte contiguos block but 8
   * byte alignment is needed. Let's check this with the inline-tags stuff */

  gint ack_and_ref;

  LMAckFunc ack_func;
  gpointer ack_userdata;
  LogMessage *original;

  /* message parts */ 
  
  /* the contents of the members below is directly copied into another
   * LogMessage with pointer values.  To change any of the fields please use
   * log_msg_set_*() functions, which will handle borrowed data members
   * correctly.
   */
  /* ==== start of directly copied part ==== */
  LogStamp timestamps[LM_TS_MAX];
  gulong *tags;
  NVHandle *sdata;

  GSockAddr *saddr;
  NVTable *payload;

  guint32 flags;
  guint16 pri;
  guint8 initial_parse:1,
    recursed:1;
  guint8 num_matches;
  guint8 num_tags;
  guint8 alloc_sdata;
  guint8 num_sdata;
  /* ==== end of directly copied part ==== */

  guint8 num_nodes;
  guint8 cur_node;
  guint8 protect_cnt;

  guint64 rcptid;

  /* preallocated LogQueueNodes used to insert this message into a LogQueue */
  LogMessageQueueNode nodes[0];

  /* a preallocated space for the inital NVTable (payload) may follow */
};

extern NVRegistry *logmsg_registry;
extern const char logmsg_sd_prefix[];
extern const gint logmsg_sd_prefix_len;
extern gint logmsg_node_max;

LogMessage *log_msg_ref(LogMessage *m);
void log_msg_unref(LogMessage *m);
void log_msg_write_protect(LogMessage *m);
void log_msg_write_unprotect(LogMessage *m);
LogMessage *log_msg_clone_cow(LogMessage *msg, const LogPathOptions *path_options);
LogMessage *log_msg_make_writable(LogMessage **pmsg, const LogPathOptions *path_options);

gboolean log_msg_write(LogMessage *self, SerializeArchive *sa);
gboolean log_msg_read(LogMessage *self, SerializeArchive *sa);

/* generic values that encapsulate log message fields, dynamic values and structured data */
NVHandle log_msg_get_value_handle(const gchar *value_name);
const gchar *log_msg_get_value_name(NVHandle handle, gssize *name_len);
gboolean log_msg_is_value_name_valid(const gchar *value);

gboolean log_msg_is_handle_macro(NVHandle handle);
gboolean log_msg_is_handle_sdata(NVHandle handle);
gboolean log_msg_is_handle_match(NVHandle handle);

static inline gboolean
log_msg_is_handle_settable_with_an_indirect_value(NVHandle handle)
{
  return (handle >= LM_V_MAX);
}

const gchar *log_msg_get_macro_value(LogMessage *self, gint id, gssize *value_len);

static inline const gchar *
log_msg_get_value(LogMessage *self, NVHandle handle, gssize *value_len)
{
  guint16 flags;

  flags = nv_registry_get_handle_flags(logmsg_registry, handle);
  if ((flags & LM_VF_MACRO) == 0)
    return __nv_table_get_value(self->payload, handle, LM_V_MAX, value_len);
  else
    return log_msg_get_macro_value(self, flags >> 8, value_len);
}

typedef gboolean (*LogMessageTagsForeachFunc)(LogMessage *self, LogTagId tag_id, const gchar *name, gpointer user_data);

void log_msg_set_value(LogMessage *self, NVHandle handle, const gchar *new_value, gssize length);
void log_msg_set_value_indirect(LogMessage *self, NVHandle handle, NVHandle ref_handle, guint8 type, guint16 ofs, guint16 len);
void log_msg_set_match(LogMessage *self, gint index, const gchar *value, gssize value_len);
void log_msg_set_match_indirect(LogMessage *self, gint index, NVHandle ref_handle, guint8 type, guint16 ofs, guint16 len);
void log_msg_clear_matches(LogMessage *self);

void log_msg_append_format_sdata(LogMessage *self, GString *result, guint32 seq_num);
void log_msg_format_sdata(LogMessage *self, GString *result, guint32 seq_num);

void log_msg_set_tag_by_id_onoff(LogMessage *self, LogTagId id, gboolean on);
void log_msg_set_tag_by_id(LogMessage *self, LogTagId id);
void log_msg_set_tag_by_name(LogMessage *self, const gchar *name);
void log_msg_clear_tag_by_id(LogMessage *self, LogTagId id);
void log_msg_clear_tag_by_name(LogMessage *self, const gchar *name);
gboolean log_msg_is_tag_by_id(LogMessage *self, LogTagId id);
gboolean log_msg_is_tag_by_name(LogMessage *self, const gchar *name);
void log_msg_tags_foreach(LogMessage *self, LogMessageTagsForeachFunc callback, gpointer user_data);
void log_msg_print_tags(LogMessage *self, GString *result);

LogMessageQueueNode *log_msg_alloc_queue_node(LogMessage *msg, const LogPathOptions *path_options);
LogMessageQueueNode *log_msg_alloc_dynamic_queue_node(LogMessage *msg, const LogPathOptions *path_options);
void log_msg_free_queue_node(LogMessageQueueNode *node);

void log_msg_clear(LogMessage *self);
LogMessage *log_msg_new(const gchar *msg, gint length,
                        GSockAddr *saddr,
                        MsgFormatOptions *parse_options);
LogMessage *log_msg_new_mark(void);
LogMessage *log_msg_new_internal(gint prio, const gchar *msg);
LogMessage *log_msg_new_empty(void);

void log_msg_add_ack(LogMessage *msg, const LogPathOptions *path_options);
void log_msg_ack(LogMessage *msg, const LogPathOptions *path_options);
void log_msg_drop(LogMessage *msg, const LogPathOptions *path_options);
const LogPathOptions *log_msg_break_ack(LogMessage *msg, const LogPathOptions *path_options, LogPathOptions *local_options);

void log_msg_refcache_start_producer(LogMessage *self);
void log_msg_refcache_start_consumer(LogMessage *self, const LogPathOptions *path_options);
void log_msg_refcache_stop(void);

void log_msg_registry_init();
void log_msg_registry_deinit();
void log_msg_global_init();
void log_msg_global_deinit(void);

gboolean log_msg_nv_table_foreach(NVTable *self, NVTableForeachFunc func, gpointer user_data);

#endif
