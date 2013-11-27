/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2012 Balázs Scheidler
 * Copyright (c) 2012-2013 Viktor Juhasz
 * Copyright (c) 2012-2013 Viktor Tusa
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

#include "logmsg-serialize.h"
#include "nvtable-serialize.h"
#include "messages.h"
#include "stdlib.h"

/* Serialization functions */
static gboolean
_serialize_sockaddr(SerializeArchive *sa, GSockAddr *addr)
{
  if (!addr)
    {
      serialize_write_uint16(sa, 0);
      return TRUE;
    }

  serialize_write_uint16(sa, addr->sa.sa_family);
  switch (addr->sa.sa_family)
    {
    case AF_INET:
      {
        struct in_addr ina;

        ina = g_sockaddr_inet_get_address(addr);
        serialize_write_blob(sa, (gchar *) &ina, sizeof(ina));
        serialize_write_uint16(sa, htons(g_sockaddr_get_port(addr)));
        break;
      }
#if ENABLE_IPV6
    case AF_INET6:
      {
        struct in6_addr *in6a;

        in6a = g_sockaddr_inet6_get_address(addr);
        serialize_write_blob(sa, (gchar *) in6a, sizeof(*in6a));
        serialize_write_uint16(sa, htons(g_sockaddr_get_port(addr)));
        break;
      }
#endif
    default:
      break;
    }
  return TRUE;
}

static gboolean
_deserialize_sockaddr(SerializeArchive *sa, GSockAddr **addr)
{
  guint16 family;

  serialize_read_uint16(sa, &family);
  switch (family)
    {
    case 0:
      /* special case, no address were stored */
      *addr = NULL;
      break;
    case AF_INET:
      {
        struct sockaddr_in sin;

        sin.sin_family = AF_INET;
        if (!serialize_read_blob(sa, (gchar *) &sin.sin_addr, sizeof(sin.sin_addr)) ||
            !serialize_read_uint16(sa, &sin.sin_port))
          return FALSE;

        *addr = g_sockaddr_inet_new2(&sin);
        break;
      }
#if ENABLE_IPV6
    case AF_INET6:
      {
        struct sockaddr_in6 sin6;

        sin6.sin6_family = AF_INET6;
        if (!serialize_read_blob(sa, (gchar *) &sin6.sin6_addr, sizeof(sin6.sin6_addr)) ||
            !serialize_read_uint16(sa, &sin6.sin6_port))
          return FALSE;
        *addr = g_sockaddr_inet6_new2(&sin6);
        break;
      }
#endif
#ifndef G_OS_WIN32
    case AF_UNIX:
      *addr = g_sockaddr_unix_new(NULL);
      break;
#endif
    default:
      return FALSE;
      break;
    }
  return TRUE;
}

static gboolean
_deserialize_sd_param(SerializeArchive *sa, gchar *sd_element_name,
                      LogMessage *self, gboolean *has_more)
{
  gchar *name = NULL, *value = NULL;
  gsize name_len, value_len;
  gchar sd_param_name[256]={0};
  gboolean success = FALSE;

  if (!serialize_read_cstring(sa, &name, &name_len) ||
      !serialize_read_cstring(sa, &value, &value_len))
    goto error;

  if (name_len != 0 && value_len != 0)
    {
      strcpy(sd_param_name, sd_element_name);
      strncpy(sd_param_name + strlen(sd_element_name), name, name_len);
      log_msg_set_value(self, log_msg_get_value_handle(sd_param_name), value, value_len);
      *has_more = TRUE;
    }
  else
    {
      *has_more = FALSE;
    }

  success = TRUE;

 error:
  g_free(name);
  g_free(value);
  return success;
}

static gboolean
_deserialize_sd_element(SerializeArchive *sa, LogMessage *self,
                        gboolean *has_more)
{
  gchar *sd_id = NULL;
  gsize sd_id_len;
  gchar sd_element_root[66]={0};
  gboolean has_more_param = TRUE;

  if (!serialize_read_cstring(sa, &sd_id, &sd_id_len))
    return FALSE;

  if (sd_id_len == 0)
    {
      *has_more=FALSE;
      g_free(sd_id);
      return TRUE;
    }
  strcpy(sd_element_root,logmsg_sd_prefix);
  strncpy(sd_element_root + logmsg_sd_prefix_len, sd_id, sd_id_len);
  sd_element_root[logmsg_sd_prefix_len + sd_id_len]='.';

  if (!_deserialize_sd_param(sa, sd_element_root, self, &has_more_param))
    goto error;

  while (has_more_param)
    {
      if (!_deserialize_sd_param(sa, sd_element_root, self, &has_more_param))
        goto error;
    }
  g_free(sd_id);
  *has_more = TRUE;
  return TRUE;

 error:
  g_free(sd_id);
  return FALSE;

}

static gboolean
_deserialize_sd_data(LogMessage *self, SerializeArchive *sa)
{
  gboolean has_more_element = TRUE;
  if (!_deserialize_sd_element(sa, self, &has_more_element))
    goto error;

  while (has_more_element)
    {
      if (!_deserialize_sd_element(sa, self, &has_more_element))
        goto error;
    }
  return TRUE;

 error:
  return FALSE;
}

static gboolean
_serialize_log_stamp(SerializeArchive *sa, LogStamp *stamp)
{
  return serialize_write_uint64(sa, stamp->tv_sec) &&
         serialize_write_uint32(sa, stamp->tv_usec) &&
         serialize_write_uint32(sa, stamp->zone_offset);
}

static gboolean
_deserialize_log_stamp(SerializeArchive *sa, LogStamp *stamp, gboolean is64bit)
{
  guint32 val;

  if (!is64bit)
    {
      if (!serialize_read_uint32(sa, &val))
        return FALSE;
      stamp->tv_sec = (gint32) val;
    }
  else
    {
      guint64 val64;

      if (!serialize_read_uint64(sa, &val64))
        return FALSE;
      stamp->tv_sec = (gint64) val64;
    }

  if (!serialize_read_uint32(sa, &val))
    return FALSE;
  stamp->tv_usec = val;

  if (!serialize_read_uint32(sa, &val))
    return FALSE;
  stamp->zone_offset = (gint) val;
  return TRUE;
}

static gboolean
_deserialize_values(LogMessage *self, SerializeArchive *sa)
{
  gchar *name = NULL, *value = NULL;
  gboolean success = FALSE;

  if (!serialize_read_cstring(sa, &name, NULL) ||
      !serialize_read_cstring(sa, &value, NULL))
    goto error;

  while (name[0])
    {
      log_msg_set_value(self, log_msg_get_value_handle(name), value, -1);
      name = value = NULL;
      if (!serialize_read_cstring(sa, &name, NULL) ||
          !serialize_read_cstring(sa, &value, NULL))
        goto error;
    }
  /* the terminating entries are not added to the hashtable, we need to free them */
  success = TRUE;

 error:
  g_free(name);
  g_free(value);
  return success;
}

static gboolean
_deserialize_tags(LogMessage *self, SerializeArchive *sa)
{
  gchar *buf;
  gchar preallocated_buffer[256];
  guint32 tag_length;

  while (1)
    {
      if (!serialize_read_uint32(sa, &tag_length))
        return FALSE;
      if (tag_length == 0)
        break;
      if (G_LIKELY(tag_length < 256))
        {
          if (!serialize_read_blob(sa, preallocated_buffer, tag_length))
            return FALSE;
          preallocated_buffer[tag_length] = '\0';
          log_msg_set_tag_by_name(self, preallocated_buffer);
        }
      else
        {
          buf = g_try_malloc(tag_length + 1);
          if (!serialize_read_blob(sa, buf, tag_length))
            return FALSE;
          buf[tag_length] = '\0';
          log_msg_set_tag_by_name(self, buf);
          g_free(buf);
        }
    }

  self->flags |= LF_STATE_OWN_TAGS;

  return TRUE;
}

static gboolean
_serialize_tags_callback(LogMessage *self, LogTagId tag_id, const gchar *name, gpointer user_data)
{
  SerializeArchive *sa = ( SerializeArchive *)user_data;

  serialize_write_cstring(sa, name, strlen(name));
  return TRUE;
}

static void
_serialize_tags(LogMessage *self, SerializeArchive *sa)
{
  log_msg_tags_foreach(self, _serialize_tags_callback, (gpointer)sa);
  serialize_write_cstring(sa, "", 0);
}

/**
 * log_msg_serialize:
 *

 * Serializes a log message to the target archive. Useful when the contents
 * of the log message with all associated metainformation needs to be stored
 * on a persistent storage device.
 *
 * Format the on-disk format starts with a 'version' field that makes it
 * possible to change the format in a compatible way. Currently we only
 * generate and support version 0.
 **/

gboolean
log_msg_serialize(LogMessage *self, SerializeArchive *sa)
{
  guint8 version = 24;
  gint i = 0;
  /*
   * version   info
   *   0       first introduced in syslog-ng-2.1
   *   1       dropped self->date
   *   10      added support for values,
   *           sd data,
   *           syslog-protocol fields,
   *           timestamps became 64 bits,
   *           removed source group string
   *           flags & pri became 16 bits
   *   11      flags became 32 bits
   *   12      store tags
   *   20      usage of the nvtable
   *   21      sdata serialization
   *   22      corrected nvtable serialization
   *   23      new RCTPID field (64 bits)
   *   24      new 32 bit NVHandle support
   */

  serialize_write_uint8(sa, version);
  serialize_write_uint64(sa, self->rcptid);
  g_assert(sizeof(self->flags) == 4);
  serialize_write_uint32(sa, self->flags & ~LF_STATE_MASK);
  serialize_write_uint16(sa, self->pri);
  _serialize_sockaddr(sa, self->saddr);
  _serialize_log_stamp(sa, &self->timestamps[LM_TS_STAMP]);
  _serialize_log_stamp(sa, &self->timestamps[LM_TS_RECVD]);
  _serialize_tags(self, sa);
  serialize_write_uint8(sa, self->initial_parse);
  serialize_write_uint8(sa, self->num_matches);
  serialize_write_uint8(sa, self->num_sdata);
  serialize_write_uint8(sa, self->alloc_sdata);
  for(i = 0; i < self->num_sdata; i++)
    serialize_write_nvhandle(sa, self->sdata[i]);
  nv_table_serialize(sa, self->payload);
  return TRUE;
}

static inline gboolean
_is_nventry_name_an_sdata_name(const gchar* name)
{
  return strncmp(name, logmsg_sd_prefix, logmsg_sd_prefix_len) == 0 && name[6];
};

static gboolean
_upgrade_sd_entries(NVHandle handle, const gchar *name, const gchar *value,
                    gssize value_len, gpointer user_data)
{
  LogMessage *lm = (LogMessage *)user_data;

  if (G_LIKELY(lm->sdata))
    {
      if (_is_nventry_name_an_sdata_name(name))
        {
          guint16 flag;
          gchar *dot;

          dot = strrchr(name,'.');
          if (dot - name - logmsg_sd_prefix_len < 0)
            {
              /*Standalone SDATA */
              flag = ((strlen(name) - logmsg_sd_prefix_len) << 8) + LM_VF_SDATA;
            }
          else
            {
              flag = ((dot - name - logmsg_sd_prefix_len) << 8) + LM_VF_SDATA;
            }
          nv_registry_set_handle_flags(logmsg_registry, handle, flag);
        }
    }

  return FALSE;
}

/*
    Read the most common values (HOST, HOST_FROM, PROGRAM, MESSAGE)
    same for all version < 20
*/
static gboolean
_deserialize_common_values(LogMessage *self, SerializeArchive *sa)
{
  gchar *host = NULL;
  gchar *host_from = NULL;
  gchar *program = NULL;
  gchar *message = NULL;
  gsize stored_len=0;

  if (!serialize_read_cstring(sa, &host, &stored_len))
     return FALSE;
  log_msg_set_value(self, LM_V_HOST, host, stored_len);
  free(host);

  if (!serialize_read_cstring(sa, &host_from, &stored_len))
    return FALSE;
  log_msg_set_value(self, LM_V_HOST_FROM, host_from, stored_len);
  free(host_from);

  if (!serialize_read_cstring(sa, &program, &stored_len))
    return FALSE;
  log_msg_set_value(self, LM_V_PROGRAM, program, stored_len);
  free(program);

  if (!serialize_read_cstring(sa, &message, &stored_len))
    return FALSE;
  log_msg_set_value(self, LM_V_MESSAGE, message, stored_len);
  free(message);

  return TRUE;
}

/*
    After the matches are read the details of those have to been read
    this process is same for version < 20
*/
static gboolean
_deserialize_matches_details(LogMessage *self, SerializeArchive *sa)
{
  gint i;

  for (i = 0; i < self->num_matches; i++)
    {
      guint8 stored_flags;
      if (!serialize_read_uint8(sa, &stored_flags))
        return FALSE;

      /* the old LMM_REF_MATCH value */
      if (stored_flags & 0x0001)
        {
          guint8 type;
          guint16 ofs;
          guint16 len;
          guint8 builtin_value;
          guint8 LM_F_MAX = 8;

          if (!serialize_read_uint8(sa, &type) ||
              !serialize_read_uint8(sa, &builtin_value) ||
              builtin_value >= LM_F_MAX ||
              !serialize_read_uint16(sa, &ofs) ||
              !serialize_read_uint16(sa, &len))
                return FALSE;

          log_msg_set_match_indirect(self, i, builtin_value, type, ofs, len);
        }
      else
        {
          gchar *match = NULL;
          gsize match_size;

          if (!serialize_read_cstring(sa, &match, &match_size))
            return FALSE;
          log_msg_set_match(self, i, match, match_size);
          free(match);
         }
    }
  return TRUE;
}

static void
_get_legacy_program_name(LogMessage *self, const guchar **data, gint *length)
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

  *data = src;
  *length = left;
}

/*
    Read the oldest version logmessage
*/
static gboolean
_deserialize_version_0_1(LogMessage *self, SerializeArchive *sa, guint8 version)
{
  guint8 stored_flags8;
  gsize stored_len;
  gint i;
  guint8 stored_pri;
  gchar *source;
  gssize message_len;
  gint delete_chars;
  const guchar *p;
  gint p_len;
  gchar *message;

  if (!serialize_read_uint8(sa, &stored_flags8))
        return FALSE;
  /* the LF_UNPARSED flag was removed in version 10, LF_UTF8 took its
     place, but 2.1 logmsg is never marked as UTF8 */
  self->flags = (stored_flags8 & ~LF_OLD_UNPARSED) | LF_STATE_MASK;

  if (!serialize_read_uint8(sa, &stored_pri))
    return FALSE;
  self->pri = stored_pri;

  if (!serialize_read_cstring(sa, &source, &stored_len))
    return FALSE;
  log_msg_set_value(self, LM_V_SOURCE, source, stored_len);

  if (!_deserialize_sockaddr(sa, &self->saddr) ||
      !_deserialize_log_stamp(sa, &self->timestamps[LM_TS_STAMP], FALSE) ||
      !_deserialize_log_stamp(sa, &self->timestamps[LM_TS_RECVD], FALSE))
    return FALSE;

  if (version < 1)
    {
      gchar *dummy;

      /* version 0 had a self->date stored here */
      if (!serialize_read_cstring(sa, &dummy, NULL))
        return FALSE;

      g_free(dummy);
    }

  if (!_deserialize_common_values(self, sa))
     return FALSE;

  /* remove prog[pid] from the beginning of message */
  message = strdup(log_msg_get_value(self, LM_V_MESSAGE, &message_len));
  p = (guchar *) message;
  p_len = message_len;

  _get_legacy_program_name(self, &p, &p_len);
  delete_chars = (gchar *) p - (gchar *) message;
  memmove(message, message + delete_chars, message_len - delete_chars + 1);
  message_len -= delete_chars;

  log_msg_set_value(self, LM_V_MESSAGE, message, message_len);

  log_msg_set_value(self, LM_V_PID, null_string, -1);
  log_msg_set_value(self, LM_V_MSGID, null_string, -1);
  free(message);

  if (!serialize_read_uint8(sa, &self->num_matches))
    return FALSE;

  for (i = 0; i < self->num_matches; i++)
    {
      gchar *match = NULL;
      gsize match_size;

      if (!serialize_read_cstring(sa, &match, &match_size))
        return FALSE;
      log_msg_set_match(self, i, match, match_size);
      free(match);
    }

  if (!_deserialize_matches_details(self,sa))
    return FALSE;
  return TRUE;
}


/*
    Read the version 10-11-12 logmessage
*/
static gboolean
_deserialize_version_1x(LogMessage *self, SerializeArchive *sa, guint8 version)
{
  gsize stored_len;
  gchar *source, *pid;
  gchar *msgid;

  if (version == 10)
    {
      guint16 stored_flags16;

      if (!serialize_read_uint16(sa, &stored_flags16))
        return FALSE;
      self->flags = stored_flags16;
    }
  else
    {
      if (!serialize_read_uint32(sa, &self->flags))
        return FALSE;
    }
  self->flags |= LF_STATE_MASK;

  if (!serialize_read_uint16(sa, &self->pri))
    return FALSE;

  if (!serialize_read_cstring(sa, &source, &stored_len))
    return FALSE;
  log_msg_set_value(self, LM_V_SOURCE, source, stored_len);
  free(source);

  if (!_deserialize_sockaddr(sa, &self->saddr) ||
      !_deserialize_log_stamp(sa, &self->timestamps[LM_TS_STAMP], TRUE) ||
      !_deserialize_log_stamp(sa, &self->timestamps[LM_TS_RECVD], TRUE))
    return FALSE;

  if (version == 12)
    {
      if (!_deserialize_tags(self, sa))
        return FALSE;
    }

  if (!_deserialize_common_values(self, sa))
     return FALSE;

  if (!serialize_read_cstring(sa, &pid, &stored_len))
    return FALSE;
  log_msg_set_value(self, LM_V_PID, pid, stored_len);
  free(pid);

  if (!serialize_read_cstring(sa, &msgid, &stored_len))
    return FALSE;
  log_msg_set_value(self, LM_V_MSGID, msgid, stored_len);
  free(msgid);

  if (!serialize_read_uint8(sa, &self->num_matches))
    return FALSE;

  if (!_deserialize_matches_details(self,sa))
    return FALSE;

  if (!_deserialize_values(self, sa) ||
      !_deserialize_sd_data(self, sa))
    return FALSE;

  return TRUE;
}

static gboolean
_deserialize_sdata_version_2x(LogMessage* self, SerializeArchive *sa,
                              guint8 version)
{
  guint32 i;
  guint32 alloc_sdata_len;
  guint16 old_nv_handle;

  if (version < 24)
    alloc_sdata_len = sizeof(guint16) * self->alloc_sdata;
  else
    alloc_sdata_len = sizeof(NVHandle) * self->alloc_sdata;

  self->sdata = (NVHandle*)g_malloc(alloc_sdata_len);
  memset(self->sdata, 0, alloc_sdata_len);

  if (version > 20)
    {
      for(i = 0; i < self->num_sdata; i++)
        {
          /* guint32 NVHandle*/
          if (version < 24)
            {
              if (!serialize_read_uint16(sa, &old_nv_handle))
                return FALSE;
              self->sdata[i] = old_nv_handle;
            }
          else
            {
              if (!serialize_read_nvhandle(sa, &self->sdata[i]))
                return FALSE;
            }
        }
    }
  return TRUE;
}

struct RestoreSDATACallbackUserdata
{
   LogMessage* message;
   int counter;
};

static gboolean
_restore_sdata_entry_callback(NVHandle handle, const gchar *name,
                              const gchar *value, gssize value_len, gpointer user_data)
{
  struct RestoreSDATACallbackUserdata* callback_data =
    (struct RestoreSDATACallbackUserdata*) user_data;
  LogMessage *message = callback_data->message;

  if (_is_nventry_name_an_sdata_name(name))
    {
      message->sdata[callback_data->counter] = handle;
      callback_data->counter++;
    }
  return FALSE;
}

static void
_restore_sdata_from_v20(LogMessage* self)
{
  struct RestoreSDATACallbackUserdata userdata;

  userdata.message = self;
  userdata.counter = 0;
  nv_table_foreach(self->payload, logmsg_registry, _restore_sdata_entry_callback,
                   &userdata);
};

/*
    Read the version 20-23 logmessage
*/
static gboolean
_deserialize_version_2x(LogMessage *self, SerializeArchive *sa, guint8 version)
{
  guint8 bf = 0;

  /*read $RCTPID from version 23 or latter*/
  if ((version > 22) && (!serialize_read_uint64(sa, &self->rcptid)))
     return FALSE;

  if (!serialize_read_uint32(sa, &self->flags))
     return FALSE;
  self->flags |= LF_STATE_MASK;

  if (!serialize_read_uint16(sa, &self->pri))
     return FALSE;

  if (!_deserialize_sockaddr(sa, &self->saddr) ||
      !_deserialize_log_stamp(sa, &self->timestamps[LM_TS_STAMP], TRUE) ||
      !_deserialize_log_stamp(sa, &self->timestamps[LM_TS_RECVD], TRUE))
    return FALSE;

  if (!_deserialize_tags(self, sa))
    return FALSE;

  if (!serialize_read_uint8(sa, &bf))
    return FALSE;
  self->initial_parse=bf;

  if (!serialize_read_uint8(sa, &self->num_matches))
    return FALSE;

  if (!serialize_read_uint8(sa, &self->num_sdata))
    return FALSE;

  if (!serialize_read_uint8(sa, &self->alloc_sdata))
    return FALSE;

  if (!_deserialize_sdata_version_2x(self, sa, version))
    return FALSE;

  nv_table_unref(self->payload);
  self->payload = NULL;
  self->payload = nv_table_deserialize(sa, version);
  if (!self->payload)
    return FALSE;

  /* Update the NVTable

     First upgrade the indirect entries and their handles
  */
  nv_table_update_ids(self->payload,logmsg_registry, self->sdata, self->num_sdata);

  /* upgrade sd_data */
  if (self->sdata)
    nv_table_foreach(self->payload, logmsg_registry, _upgrade_sd_entries, self);

  /* Version 20 of LogMessage serialization did not serialize sdata array,
     but we can restore the sdata-s from the NVTable */
  if (version == 20 && self->sdata)
    _restore_sdata_from_v20(self);

  return TRUE;
}

gboolean
log_msg_deserialize(LogMessage *self, SerializeArchive *sa)
{
  guint8 version;

  if (!serialize_read_uint8(sa, &version))
    return FALSE;

  if ((version > 1 && version < 10) || version > 24)
    goto error;

  if (version < 10)
    return _deserialize_version_0_1(self, sa, version);
  else if (version < 20)
    return _deserialize_version_1x(self, sa, version);
  else if (version <= 24)
    return _deserialize_version_2x(self, sa, version);

 error:
  msg_error("Error deserializing log message, unsupported version",
            evt_tag_int("version", version),
            NULL);
  return FALSE;
}
