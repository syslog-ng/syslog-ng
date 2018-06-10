/*
 * Copyright (c) 2010-2015 Balabit
 * Copyright (c) 2015 Viktor Juhasz <viktor.juhasz@balabit.com>
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

#include "logmsg/logmsg-serialize.h"
#include "logmsg/logmsg-serialize-fixup.h"
#include "logmsg/nvtable-serialize.h"
#include "logmsg/nvtable-serialize-legacy.h"
#include "logmsg/gsockaddr-serialize.h"
#include "logmsg/timestamp-serialize.h"
#include "logmsg/tags-serialize.h"
#include "messages.h"

#include <stdlib.h>


#define OLD_LMM_REF_MATCH 0x0001

static gboolean
_serialize_message(LogMessageSerializationState *state)
{
  LogMessage *msg = state->msg;
  SerializeArchive *sa = state->sa;

  serialize_write_uint8(sa, state->version);
  serialize_write_uint64(sa, msg->rcptid);
  g_assert(sizeof(msg->flags) == 4);
  serialize_write_uint32(sa, msg->flags & ~LF_STATE_MASK);
  serialize_write_uint16(sa, msg->pri);
  g_sockaddr_serialize(sa, msg->saddr);
  timestamp_serialize(sa, msg->timestamps);
  serialize_write_uint32(sa, msg->host_id);
  tags_serialize(msg, sa);
  serialize_write_uint8(sa, msg->initial_parse);
  serialize_write_uint8(sa, msg->num_matches);
  serialize_write_uint8(sa, msg->num_sdata);
  serialize_write_uint8(sa, msg->alloc_sdata);
  serialize_write_uint32_array(sa, (guint32 *) msg->sdata, msg->num_sdata);
  nv_table_serialize(state, msg->payload);
  return TRUE;
}

gboolean
log_msg_serialize(LogMessage *self, SerializeArchive *sa)
{
  LogMessageSerializationState state = { 0 };

  state.version = LGM_V26;
  state.msg = self;
  state.sa = sa;
  return _serialize_message(&state);
}

static gboolean
_deserialize_sdata(LogMessageSerializationState *state)
{
  LogMessage *self = state->msg;
  SerializeArchive *sa = state->sa;

  if (!serialize_read_uint8(sa, &self->num_sdata))
    return FALSE;

  if (!serialize_read_uint8(sa, &self->alloc_sdata))
    return FALSE;

  g_assert(!self->sdata);
  self->sdata = (NVHandle *) g_malloc(sizeof(NVHandle)*self->alloc_sdata);

  if (state->version <= LGM_V20)
    return TRUE;

  if ((state->version < LGM_V26) && !serialize_read_uint16_array(sa, (guint32 *) self->sdata, self->num_sdata))
    return FALSE;

  if ((state->version == LGM_V26) && !serialize_read_uint32_array(sa, (guint32 *) self->sdata, self->num_sdata))
    return FALSE;

  return TRUE;
}

NVTable *
_nv_table_deserialize_selector(LogMessageSerializationState *state)
{
  if (state->version < LGM_V22)
    {
      state->nvtable = nv_table_deserialize_legacy(state->sa);

      return state->nvtable;
    }
  else if (state->version < LGM_V26)
    {
      state->nvtable = nv_table_deserialize_22(state->sa);

      return state->nvtable;
    }
  else if (state->version == LGM_V26)
    {
      return nv_table_deserialize(state);
    }

  return NULL;
}

static gboolean
_deserialize_message_version_2x(LogMessageSerializationState *state)
{
  g_assert(state->version >= LGM_V20);

  guint8 initial_parse = 0;
  LogMessage *msg = state->msg;
  SerializeArchive *sa = state->sa;

  if ((state->version > LGM_V22) && !serialize_read_uint64(sa, &msg->rcptid))
    return FALSE;
  if (!serialize_read_uint32(sa, &msg->flags))
    return FALSE;
  msg->flags |= LF_STATE_MASK;
  if (!serialize_read_uint16(sa, &msg->pri))
    return FALSE;
  if (!g_sockaddr_deserialize(sa, &msg->saddr))
    return FALSE;
  if ((state->version < LGM_V24) && !timestamp_deserialize_legacy(sa, msg->timestamps))
    return FALSE;
  if ((state->version >= LGM_V24) && !timestamp_deserialize(sa, msg->timestamps))
    return FALSE;
  if ((state->version >= LGM_V25) && (!serialize_read_uint32(sa, &msg->host_id)))
    return FALSE;

  if (!tags_deserialize(msg, sa))
    return FALSE;

  if (!serialize_read_uint8(sa, &initial_parse))
    return FALSE;
  msg->initial_parse=initial_parse;

  if (!serialize_read_uint8(sa, &msg->num_matches))
    return FALSE;

  if (!_deserialize_sdata(state))
    return FALSE;

  nv_table_unref(msg->payload);
  msg->payload = _nv_table_deserialize_selector(state);
  if (!msg->payload)
    return FALSE;

  if (!log_msg_fixup_handles_after_deserialization(state))
    return FALSE;
  return TRUE;
}

static gboolean
log_msg_read_tags(LogMessage *self, SerializeArchive *sa)
{
  gchar *buf;
  gsize len;

  while (TRUE)
    {
      if (!serialize_read_cstring(sa, &buf, &len) || !buf)
        return FALSE;
      if (!buf[0])
        {
          /* "" , empty string means: last tag */
          g_free(buf);
          break;
        }
      log_msg_set_tag_by_name(self, buf);
      g_free(buf);
    }

  self->flags |= LF_STATE_OWN_TAGS;

  return TRUE;
}

/*
    Read the most common values (HOST, HOST_FROM, PROGRAM, MESSAGE)
    same for all version < 20
*/
gboolean
log_msg_read_common_values(LogMessage *self, SerializeArchive *sa)
{
  gchar *host = NULL;
  gchar *host_from = NULL;
  gchar *program = NULL;
  gchar *message = NULL;
  gsize stored_len=0;
  if (!serialize_read_cstring(sa, &host, &stored_len))
    return FALSE;
  log_msg_set_value(self, LM_V_HOST, host, stored_len);
  g_free(host);

  if (!serialize_read_cstring(sa, &host_from, &stored_len))
    return FALSE;
  log_msg_set_value(self, LM_V_HOST_FROM, host_from, stored_len);
  g_free(host_from);

  if (!serialize_read_cstring(sa, &program, &stored_len))
    return FALSE;
  log_msg_set_value(self, LM_V_PROGRAM, program, stored_len);
  g_free(program);

  if (!serialize_read_cstring(sa, &message, &stored_len))
    return FALSE;
  log_msg_set_value(self, LM_V_MESSAGE, message, stored_len);
  g_free(message);
  return TRUE;
}

/*
    After the matches are read the details of those have to been read
    this process is same for version < 20
*/
gboolean
log_msg_read_matches_details(LogMessage *self, SerializeArchive *sa)
{
  gint i;
  for (i = 0; i < self->num_matches; i++)
    {
      guint8 stored_flags;
      if (!serialize_read_uint8(sa, &stored_flags))
        return FALSE;

      if (stored_flags & OLD_LMM_REF_MATCH)
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
          log_msg_set_match_indirect(self,i,builtin_value,type,ofs,len);
        }
      else
        {
          gchar *match = NULL;
          gsize match_size;
          if (!serialize_read_cstring(sa, &match, &match_size))
            return FALSE;
          log_msg_set_match(self, i, match, match_size);
          g_free(match);
        }
    }
  return TRUE;
}

static gboolean
log_msg_read_values(LogMessage *self, SerializeArchive *sa)
{
  gchar *name = NULL, *value = NULL;
  gboolean success = FALSE;

  if (!serialize_read_cstring(sa, &name, NULL) ||
      !serialize_read_cstring(sa, &value, NULL))
    goto error;
  while (name[0])
    {
      log_msg_set_value(self,log_msg_get_value_handle(name),value,-1);
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
log_msg_read_sd_param(SerializeArchive *sa, gchar *sd_element_name, LogMessage *self, gboolean *has_more)
{
  gchar *name = NULL, *value = NULL;
  gsize name_len, value_len;
  gchar sd_param_name[256]= {0};
  gboolean success = FALSE;

  if (!serialize_read_cstring(sa, &name, &name_len) ||
      !serialize_read_cstring(sa, &value, &value_len))
    goto error;

  if (name_len != 0 && value_len != 0)
    {
      strncpy(sd_param_name, sd_element_name,256);
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
log_msg_read_sd_param_first(SerializeArchive *sa, gchar *sd_element_name, gsize sd_element_name_len, LogMessage *self,
                            gboolean *has_more)
{
  gchar *name = NULL, *value = NULL;
  gsize name_len, value_len;
  gchar sd_param_name[256]= {0};
  gboolean success = FALSE;

  if (!serialize_read_cstring(sa, &name, &name_len) ||
      !serialize_read_cstring(sa, &value, &value_len))
    goto error;

  if (name_len != 0 && value_len != 0)
    {
      strncpy(sd_param_name, sd_element_name, sd_element_name_len);
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
log_msg_read_sd_element(SerializeArchive *sa, LogMessage *self, gboolean *has_more)
{
  gchar *sd_id = NULL;
  gsize sd_id_len;
  gchar sd_element_root[66]= {0};
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


  if (!log_msg_read_sd_param_first(sa, sd_element_root, G_N_ELEMENTS(sd_element_root), self, &has_more_param))
    goto error;

  while (has_more_param)
    {
      if (!log_msg_read_sd_param(sa, sd_element_root, self, &has_more_param))
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
log_msg_read_sd_data(LogMessage *self, SerializeArchive *sa)
{
  gboolean has_more_element = TRUE;
  if (!log_msg_read_sd_element(sa, self, &has_more_element))
    goto error;


  while (has_more_element)
    {
      if (!log_msg_read_sd_element(sa, self, &has_more_element))
        goto error;
    }
  return TRUE;
error:

  return FALSE;
}

static gboolean
_deserialize_message_version_1x(LogMessageSerializationState *state)
{
  gsize stored_len;
  gchar *source, *pid;
  gchar *msgid;

  LogMessage *msg = state->msg;
  SerializeArchive *sa = state->sa;

  if(state->version == LGM_V10)
    {
      guint16 stored_flags16;
      if (!serialize_read_uint16(sa, &stored_flags16))
        return FALSE;
      msg->flags = stored_flags16;
    }
  else
    {
      if (!serialize_read_uint32(sa, &msg->flags))
        return FALSE;
    }
  msg->flags |= LF_STATE_MASK;
  if (!serialize_read_uint16(sa, &msg->pri))
    return FALSE;

  if (!serialize_read_cstring(sa, &source, &stored_len))
    return FALSE;
  log_msg_set_value(msg, LM_V_SOURCE, source, stored_len);
  g_free(source);
  if (!g_sockaddr_deserialize(sa, &msg->saddr))
    return FALSE;

  if (!timestamp_deserialize_legacy(sa, msg->timestamps))
    return FALSE;

  if(state->version == LGM_V12)
    {
      if (!log_msg_read_tags(msg, sa))
        return FALSE;
    }
  if(!log_msg_read_common_values(msg, sa))
    return FALSE;
  if (!serialize_read_cstring(sa, &pid, &stored_len))
    return FALSE;
  log_msg_set_value(msg, LM_V_PID, pid, stored_len);
  g_free(pid);

  if (!serialize_read_cstring(sa, &msgid, &stored_len))
    return FALSE;
  log_msg_set_value(msg, LM_V_MSGID, msgid, stored_len);
  g_free(msgid);
  if (!serialize_read_uint8(sa, &msg->num_matches))
    return FALSE;
  if(!log_msg_read_matches_details(msg,sa))
    return FALSE;
  if (!log_msg_read_values(msg, sa) ||
      !log_msg_read_sd_data(msg, sa))
    return FALSE;
  return TRUE;
}

static gboolean
_check_msg_version(LogMessageSerializationState *state)
{
  if (!serialize_read_uint8(state->sa, &state->version))
    return FALSE;

  if (state->version < LGM_V10 || state->version > LGM_V26)
    {
      msg_error("Error deserializing log message, unsupported version",
                evt_tag_int("version", state->version));
      return FALSE;
    }
  return TRUE;
}

gboolean
log_msg_deserialize(LogMessage *self, SerializeArchive *sa)
{
  LogMessageSerializationState state = { 0 };

  state.sa = sa;
  state.msg = self;
  if (!_check_msg_version(&state))
    {
      return FALSE;
    }

  if (state.version < LGM_V20)
    return _deserialize_message_version_1x(&state);

  return _deserialize_message_version_2x(&state);
}
