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
  
#ifndef LOGMSG_H_INCLUDED
#define LOGMSG_H_INCLUDED

#include "syslog-ng.h"
#include "gsockaddr.h"
#include "atomic.h"
#include "serialize.h"
#include "logstamp.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <regex.h>

#define LP_NOPARSE         0x0001
#define LP_INTERNAL        0x0002
#define LP_LOCAL           0x0004
#define LP_CHECK_HOSTNAME  0x0008
#define LP_STRICT	   0x0010
#define LP_KERNEL          0x0020
#define LP_SYSLOG_PROTOCOL    0x0040
/* the caller knows the message is valid UTF-8 */
#define LP_ASSUME_UTF8     0x0080
#define LP_VALIDATE_UTF8   0x0100
#define LP_NO_MULTI_LINE   0x0200
#define LP_STORE_LEGACY_MSGHDR 0x0400


typedef struct _LogPathOptions LogPathOptions;

typedef void (*LMAckFunc)(LogMessage *lm, gpointer user_data);

#define RE_MAX_MATCHES 256

#define LM_TS_STAMP 0
#define LM_TS_RECVD 1
#define LM_TS_MAX   2

enum
{
  LM_F_NONE,
  LM_F_HOST,
  LM_F_HOST_FROM,
  LM_F_MESSAGE,
  LM_F_PROGRAM,
  LM_F_PID,
  LM_F_MSGID,
  LM_F_SOURCE,
  LM_F_MAX,
};

#define LOG_MESSAGE_BUILTIN_FIELD(field) ((gchar *) LM_F_##field)
#define LOG_MESSAGE_IS_BUILTIN_FIELD(field) (GPOINTER_TO_INT(field) < LM_F_MAX)

enum
{
  LF_OLD_UNPARSED     = 0x0001,
  LF_UTF8             = 0x0001,
  LF_INTERNAL         = 0x0002,
  LF_LOCAL            = 0x0004,
  LF_MARK             = 0x0008,

  LF_OWN_NONE         = 0x0000,
  LF_OWN_HOST         = 0x0010,
  LF_OWN_HOST_FROM    = 0x0020,
  LF_OWN_MESSAGE      = 0x0040,
  LF_OWN_PROGRAM      = 0x0080,
  LF_OWN_PID          = 0x0100,
  LF_OWN_MSGID        = 0x0200,
  LF_OWN_VALUES       = 0x0400,
  LF_OWN_SDATA        = 0x0800,
  LF_OWN_SADDR        = 0x1000,
  LF_OWN_SOURCE       = 0x2000,
  LF_OWN_MATCHES      = 0x4000,
  LF_OWN_ALL          = 0x7FF0,
  LF_CHAINED_HOSTNAME = 0x8000,

  /* originally parsed from RFC 3164 format and the legacy message header
   * was saved in $LEGACY_MSGHDR. This flag is a hack to avoid a hash lookup
   * in the fast path and indicates that the parser has saved the legacy
   * message header intact in a value named LEGACY_MSGHDR.
   */
  LF_LEGACY_MSGHDR    = 0x00010000,
};

typedef struct _LogMessageSDParam  LogMessageSDParam;
typedef struct _LogMessageSDElement LogMessageSDElement;


enum
{
  /* this flag represents that this match is a reference to a field using offset + length */
  LMM_REF_MATCH = 0x0001,
};

typedef struct _LogMessageMatch
{
  union 
  {
    gchar *match;
    struct
    {
      /* we are attempting to overlay the "flags" field to the least
       * significant byte of the 'match' pointer above. Assuming that the
       * least significant bits of a malloc allocated memory area are unset,
       * we can use the least significant bit to check if we are using a
       * reference or a duplicated string */
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
      guint8 flags;
      guint8 type;
      guint8 builtin_value;
      guint8 __pad;
      guint16 ofs;
      guint16 len;
#else
      guint16 ofs;
      guint16 len;
      guint8 builtin_value;
      guint8 type;
      guint8 __pad;
      guint8 flags;
#endif
    };
  };
} LogMessageMatch;


/* NOTE: the members are ordered according to the presumed use frequency. 
 * The structure itself is 2 cachelines, the border is right after the "msg"
 * member */
struct _LogMessage
{
  GAtomicCounter ref_cnt; 
  GAtomicCounter ack_cnt; 
  /* message parts */ 
  
  /* the contents of this struct is directly copied into another
   * LogMessage with pointer values. To change any of the fields
   * please use log_msg_set_*() functions.
   */
  struct
  {
    guint32 flags;
    guint32 message_len;
    guint16 pri;
    /* 6 bytes hole */
    
    LogStamp timestamps[LM_TS_MAX];
    gchar * const host;
    gchar * const host_from;
    gchar * const message;
    gchar * const program;
    gchar * const pid;
    gchar * const msgid;
    gchar * const source;
    guint8 host_len;
    guint8 host_from_len;
    guint8 program_len;
    guint8 pid_len;
    guint8 msgid_len;
    guint8 source_len;
    guint8 recurse_count;
    guint8 num_matches;

    LogMessageMatch *matches;
    GHashTable *values;
    LogMessageSDElement *sdata;
    GSockAddr *saddr;
  };

  /* if you change any of the fields here, be sure to adjust
   * log_msg_clone_cow() as well to initialize fields properly */
  
  LMAckFunc ack_func;
  gpointer ack_userdata;
  LogMessage *original;
};

#define LOG_MESSAGE_WRITABLE_FIELD(msgfield) *((gchar **) &(msgfield))

LogMessage *log_msg_ref(LogMessage *m);
void log_msg_unref(LogMessage *m);
LogMessage *log_msg_clone_cow(LogMessage *m, const LogPathOptions *path_options);

gboolean log_msg_write(LogMessage *self, SerializeArchive *sa);
gboolean log_msg_read(LogMessage *self, SerializeArchive *sa);

/* generic values that encapsulate log message fields, dynamic values and structured data */
const gchar *log_msg_get_value_name(const gchar *value_name);
const gchar *log_msg_translate_value_name(const gchar *value_name);
void log_msg_free_value_name(const gchar *value_name);
gchar *log_msg_get_value(LogMessage *self, const gchar *value_name, gssize *length);
void log_msg_set_value(LogMessage *self, const gchar *value_name, gchar *new_value, gssize length);


/* dynamic values */
void log_msg_add_dyn_value_ref(LogMessage *self, gchar *name, gchar *value);
void log_msg_add_dyn_value(LogMessage *self, const gchar *name, const gchar *value);
void log_msg_add_sized_dyn_value(LogMessage *self, const gchar *name, const gchar *value, gsize value_len);
gchar *log_msg_lookup_dyn_value(LogMessage *self, const gchar *name);


void log_msg_set_host(LogMessage *self, gchar *new_host, gssize len);
void log_msg_set_host_from(LogMessage *self, gchar *new_host_from, gssize len);
void log_msg_set_message(LogMessage *self, gchar *new_msg, gssize len);
void log_msg_set_program(LogMessage *self, gchar *new_program, gssize len);
void log_msg_set_pid(LogMessage *self, gchar *new_process_id, gssize len);
void log_msg_set_msgid(LogMessage *self, gchar *new_message_id, gssize len);
void log_msg_set_source(LogMessage *self, gchar *new_source, gssize len);
void log_msg_set_sdata(LogMessage *self, LogMessageSDElement *elements);
void log_msg_set_matches(LogMessage *self, gint num_matches, LogMessageMatch *matches);
void log_msg_clear_matches(LogMessage *self);
void log_msg_free_matches_elements(LogMessageMatch *matches, gint num_matches);

/* runtime field get/set support */
const gchar *log_msg_get_field_name(gint field);
gint log_msg_get_field_id(const gchar *field_name);
gchar *log_msg_get_field(LogMessage *msg, gint field, gsize *length);
void log_msg_set_field(LogMessage *msg, gint field, gchar *new_value, gsize length);

LogMessage *log_msg_new(const gchar *msg, gint length, GSockAddr *saddr, guint flags, regex_t *bad_hostname, glong assume_timezone);
LogMessage *log_msg_new_mark(void);
LogMessage *log_msg_new_internal(gint prio, const gchar *msg, guint flags);
LogMessage *log_msg_new_empty(void);

void log_msg_add_ack(LogMessage *msg, const LogPathOptions *path_options);
void log_msg_ack(LogMessage *msg, const LogPathOptions *path_options);
void log_msg_drop(LogMessage *msg, const LogPathOptions *path_options);

gchar *log_msg_lookup_sdata(LogMessage *self, const gchar *name, gssize param_len);
void log_msg_append_format_sdata(LogMessage *self, GString *result);
void log_msg_format_sdata(LogMessage *self, GString *result);
void log_msg_update_sdata(LogMessage *self, const gchar *element_id, const gchar *param_name, const gchar *param_value);



#endif
