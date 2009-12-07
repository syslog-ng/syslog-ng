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

#include <sys/types.h>
#include <time.h>
#include <syslog.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

static const char aix_fwd_string[] = "Message forwarded from ";
static const char repeat_msg_string[] = "last message repeated";
static gchar *null_string = "";

/* clonable LogMessage support with shared data pointers */

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

#define FUNC_MSG_STR_SETTER(field, macro)   \
  void log_msg_set_##field(LogMessage *self, gchar *field, gssize len)  \
  {                                                                     \
    if (log_msg_chk_flag(self, LF_OWN_##macro))                         \
      g_free(self->field);                                              \
    LOG_MESSAGE_WRITABLE_FIELD(self->field) = field;                    \
    self->field ## _len = len > 0 ? len : strlen(field);                \
    log_msg_set_flag(self, LF_OWN_##macro);                             \
  }                                                                     

FUNC_MSG_STR_SETTER(host, HOST);
FUNC_MSG_STR_SETTER(host_from, HOST_FROM);
FUNC_MSG_STR_SETTER(message, MESSAGE);
FUNC_MSG_STR_SETTER(program, PROGRAM);
FUNC_MSG_STR_SETTER(pid, PID);
FUNC_MSG_STR_SETTER(msgid, MSGID);
FUNC_MSG_STR_SETTER(source, SOURCE);

void
log_msg_set_matches(LogMessage *self, gint num_matches, LogMessageMatch *matches)
{
  if (log_msg_chk_flag(self, LF_OWN_MATCHES))
    {
      log_msg_clear_matches(self);
    }
  self->num_matches = num_matches;
  self->matches = matches;
  log_msg_set_flag(self, LF_OWN_MATCHES);
}

void
log_msg_free_matches_elements(LogMessageMatch *matches, gint num_matches)
{
  gint i;
  for (i = 0; i < num_matches; i++)
    {
      if ((matches[i].flags & LMM_REF_MATCH) == 0)
        {
          g_free(matches[i].match);
          matches[i].match = NULL;
        }
    }
}

void
log_msg_clear_matches(LogMessage *self)
{
  if (log_msg_chk_flag(self, LF_OWN_MATCHES) && self->matches)
    {
      log_msg_free_matches_elements(self->matches, self->num_matches);
      g_free(self->matches);
    }

  self->matches = NULL;
  self->num_matches = 0;
}

/* the index matches the value id */
gchar *builtin_value_names[] = 
{
  NULL,
  "HOST", 
  "HOST_FROM", 
  "MESSAGE",
  "PROGRAM",
  "PID",
  "MSGID",
  "SOURCE",
  NULL,
};

#define MAX_BUILTIN_VALUE  8

const gchar *
log_msg_get_value_name(const gchar *value_name)
{
  guint value_id = GPOINTER_TO_UINT(value_name);

  if (value_id < MAX_BUILTIN_VALUE)
    return builtin_value_names[value_id];
  else
    return value_name;
}

const gchar *
log_msg_translate_value_name(const gchar *value_name)
{
  gint i;

  for (i = 1; builtin_value_names[i]; i++)
    {
      if (strcasecmp(builtin_value_names[i], value_name) == 0)
        return GUINT_TO_POINTER(i);
    }
  return g_strdup(value_name);
}

void
log_msg_free_value_name(const gchar *value_name)
{
  guint value_id = GPOINTER_TO_UINT(value_name);

  if (value_id >= MAX_BUILTIN_VALUE)
    g_free((gchar *) value_name);
}

static const char sd_prefix[] = ".SDATA.";
const gint sd_prefix_len = sizeof(sd_prefix) - 1;

/**
 *
 * NOTE: length can be set to NULL in which case length is not
 * returned.
 *
 **/
gchar *
log_msg_get_value(LogMessage *self, const gchar *value_name, gssize *length)
{
  guint value_id = GPOINTER_TO_UINT(value_name);
  gchar *value = NULL;

  if (value_id < MAX_BUILTIN_VALUE)
    {
      switch (value_id)
        {
        case LM_F_HOST:
          *length = self->host_len;
          return self->host;
        case LM_F_HOST_FROM:
          *length = self->host_from_len;
          return self->host_from;
        case LM_F_MESSAGE:
          *length = self->message_len;
          return self->message;
        case LM_F_PROGRAM:
          *length = self->program_len;
          return self->program;
        case LM_F_PID:
          *length = self->pid_len;
          return self->pid;
        case LM_F_MSGID:
          *length = self->msgid_len;
          return self->msgid;
        case LM_F_SOURCE:
          *length = self->source_len;
          return self->source;
        default:
          g_assert_not_reached();
          break;
        }
    }
  else if (strncmp(value_name, sd_prefix, sd_prefix_len) == 0)
    {
      gint value_name_len = strlen(value_name);
      if (self->sdata)
        value = log_msg_lookup_sdata(self, value_name + sd_prefix_len, value_name_len - sd_prefix_len);
      *length = value ? strlen(value) : 0;
    }
  else if (value_name[0] >= '0' && value_name[0] <= '9')
    {
      gint value_int = atoi(value_name);
      
      if (value_int < self->num_matches)
        {
          LogMessageMatch *lmm = &self->matches[value_int];
          
          if (lmm->flags & LMM_REF_MATCH)
            {
              /* this match is a reference */
              gchar *referenced_value;
              gssize builtin_length;
              
              referenced_value = log_msg_get_value(self, (gchar *) GINT_TO_POINTER((gint) lmm->builtin_value), &builtin_length);
              
              if (referenced_value)
                {
                  value = referenced_value + lmm->ofs;
                  *length = lmm->len;
                }
            }
          else if (lmm->match)
            {
              /* this match is a copied string, return that */
              
              value = lmm->match;
              *length = strlen(value);
            }
          else
            {
              value = null_string;
              *length = 0;
            }
        }
    }
  else
    {
      value = log_msg_lookup_dyn_value(self, value_name);
      *length = value ? strlen(value) : 0;
    }
  if (!value)
    {
      msg_debug("No such value known",
                evt_tag_str("value", log_msg_get_value_name(value_name)),
                NULL);
      value = null_string;
      *length = 0;
    }
  return value;
}

/**
 * NOTE: the new_value is taken as a reference, e.g. it'll be assigned to
 * the LogMessage and freed later on.
 **/
void
log_msg_set_value(LogMessage *self, const gchar *value_name, gchar *new_value, gssize length)
{
  guint value_id = GPOINTER_TO_UINT(value_name);
  static const char sd_prefix[] = ".SDATA.";
  const gint sd_prefix_len = sizeof(sd_prefix) - 1;

  if (value_id < MAX_BUILTIN_VALUE)
    {
      gint i;
      /* if the referenced matches use the field being changed, convert ref. matches to duplicated ones */
      for (i = 0; i < self->num_matches; i++)
        {
          LogMessageMatch *lmm = &self->matches[i];

          if ((lmm->flags & LMM_REF_MATCH) && lmm->builtin_value == value_id)
            {
              gssize builtin_length;
              gchar *referenced_value;

              referenced_value = log_msg_get_value(self, (gchar *) GINT_TO_POINTER((gint) lmm->builtin_value), &builtin_length);
              lmm->match = g_strndup(&referenced_value[lmm->ofs], lmm->len);
            }
        }

      switch (value_id)
        {
        case LM_F_HOST:
          log_msg_set_host(self, new_value, length);
          break;
        case LM_F_HOST_FROM:
          log_msg_set_host(self, new_value, length);
          break;
        case LM_F_MESSAGE:
          log_msg_set_message(self, new_value, length);
          break;
        case LM_F_PROGRAM:
          log_msg_set_program(self, new_value, length);
          break;
        case LM_F_PID:
          log_msg_set_pid(self, new_value, length);
          break;
        case LM_F_MSGID:
          log_msg_set_msgid(self, new_value, length);
          break;
        case LM_F_SOURCE:
          log_msg_set_source(self, new_value, length);
          break;
        default:
          g_assert_not_reached();
          break;
        }
    }
  else if (value_name[0] >= '0' && value_name[0] <= '9')
    {
      gint value_int = atoi(value_name);
      
      if (value_int < self->num_matches)
        {
          LogMessageMatch *lmm = &self->matches[value_int];
          
          if ((lmm->flags & LMM_REF_MATCH) == 0 && lmm->match)
            g_free(lmm->match);
            
          lmm->match = g_strndup(new_value, length);
        }
    }
  else if (strncmp(value_name, sd_prefix, sd_prefix_len) == 0)
    {
    }
  else
    {
      log_msg_add_dyn_value_ref(self, g_strdup(value_name), new_value);
    }
}

struct  _LogMessageSDParam
{
  LogMessageSDParam *next_param;
  gchar *name;
  gchar *value;
};

struct _LogMessageSDElement
{
  LogMessageSDElement *next_element;
  gchar *name;
  LogMessageSDParam *params;
};


/* SD parameters  */
static LogMessageSDParam *
log_msg_sd_param_new(const gchar *param_name, const gchar *param_value)
{
  LogMessageSDParam *self = g_new0(LogMessageSDParam, 1);

  self->next_param = NULL;
  self->name = g_strdup(param_name);
  self->value = g_strdup(param_value);
  return self;
}

static void
log_msg_sd_param_free(LogMessageSDParam *self)
{
  g_free(self->name);
  g_free(self->value);
  g_free(self);
}

static LogMessageSDParam *
log_msg_sd_param_append(LogMessageSDParam *self, const gchar *param_name, const gchar *param_value)
{
  g_assert(self);
  g_assert(!self->next_param);

  self->next_param = log_msg_sd_param_new(param_name, param_value);
  return self->next_param;
}

/* SD elements */

static LogMessageSDParam *
log_msg_sd_element_lookup(LogMessageSDElement *self, const gchar *param_name)
{
  LogMessageSDParam *param = self->params;
  
  while (param && strcmp(param->name, param_name) != 0)
    param = param->next_param;
  return param;
}

static LogMessageSDElement *
log_msg_sd_element_new(const gchar *element_name)
{
  LogMessageSDElement *self = g_new0(LogMessageSDElement, 1);

  self->params = NULL;
  self->next_element = NULL;
  self->name = g_strdup(element_name);
  return self;
}

static void
log_msg_sd_element_free(LogMessageSDElement *self)
{
  LogMessageSDParam *param, *param_next;
  
  param = self->params;
  while (param)
    {
      param_next = param->next_param;
      log_msg_sd_param_free(param);
      param = param_next;
    }
  g_free(self->name);
  g_free(self);
}

static void 
log_msg_sd_elements_free(LogMessageSDElement *elems)
{
  LogMessageSDElement *elem, *elem_next;
  
  elem = elems;
  while (elem)
    {
      elem_next = elem->next_element;
      log_msg_sd_element_free(elem);
      elem = elem_next;
    }
}

static LogMessageSDElement *
log_msg_sd_element_append(LogMessageSDElement *self, const gchar *elem_name)
{
  g_assert(!self->next_element);

  self->next_element = log_msg_sd_element_new(elem_name);
  return self->next_element;
}


void
log_msg_append_format_sdata(LogMessage *self, GString *result)
{
  LogMessageSDElement *element = NULL;
  LogMessageSDParam *param = NULL;

  element = self->sdata;
  while (element)
    {
      g_string_append_c(result, '[');
      g_string_append(result, element->name);
      param = element->params;
      while (param)
        {
          g_string_append_c(result, ' ');
          g_string_append(result, param->name);
          g_string_append(result, "=\"");
          g_string_append(result, param->value);
          g_string_append_c(result, '"');
          param = param->next_param;
        }
      g_string_append_c(result, ']');
      element = element->next_element;
    }
}

void
log_msg_format_sdata(LogMessage *self, GString *result)
{
  g_string_truncate(result, 0);
  log_msg_append_format_sdata(self, result);
}

static LogMessageSDElement *
log_msg_lookup_sdata_element(LogMessage *self, const gchar *elem_name)
{
  LogMessageSDElement *element = self->sdata;
  
  while (element && strcmp(element->name, elem_name) != 0)
    element = element->next_element;

  return element;
}


/**
 *
 * This function looks up a structured data element based on a query string
 * composed of "SD-ID.SD-PARAM", a syntax used in user supplied templates.
 *
 * NOTE: this function does not care about LF_OWN_SDATA, it goes
 * straight to self->sd_elements. In order to treat LF_OWN_SDATA
 * properly use the log_msg_lookup_sdata_value function below.
 **/
static gchar *
log_msg_lookup_sdata_internal(LogMessage *self, const gchar *query, gssize query_len)
{
  gchar *elem_name, *param_name, *ret = NULL;
  
  if (query_len < 0)
    query_len = strlen(query);

  elem_name = g_strndup(query, query_len);
  param_name = memchr(elem_name, '.', query_len);
  if (param_name != NULL)
    {
      LogMessageSDElement *element;

      *param_name = '\0';
      param_name++;
      element = log_msg_lookup_sdata_element(self, elem_name);
      if (element)
        {
          LogMessageSDParam *param = log_msg_sd_element_lookup(element, param_name);
          ret = param ? param->value : NULL;
        }
     }
  g_free(elem_name);
  return ret;
}

gchar * 
log_msg_lookup_sdata(LogMessage *self, const gchar *query, gssize query_len)
{
  gchar *value = NULL;

  if (log_msg_chk_flag(self, LF_OWN_SDATA))
    {
      if (self->sdata)
        value = log_msg_lookup_sdata_internal(self, query, query_len);
    }
  else if (self->original)
     value = log_msg_lookup_sdata_internal(self->original, query, query_len);               
  return value;
}

void 
log_msg_set_sdata(LogMessage *self, LogMessageSDElement *new_sdata)
{
  if (!log_msg_chk_flag(self, LF_OWN_SDATA))
    {
      self->sdata = NULL;
      log_msg_set_flag(self, LF_OWN_SDATA);
    }
  if (self->sdata)
    log_msg_sd_elements_free(self->sdata);
  self->sdata = new_sdata;
}


static void 
log_msg_clone_sdata(LogMessage *msg)
{
  LogMessageSDElement *element = NULL;
  LogMessageSDElement *new_element = NULL;
  LogMessageSDParam *param = NULL;
  LogMessageSDParam *new_param = NULL;
  
  g_assert((msg->flags & LF_OWN_SDATA) == 0);
 
  element = msg->sdata;
  while (element)
    {
      if (!new_element)
        { 
          /* First element in the copy, so store the pointer in the log message*/
          msg->sdata = new_element = log_msg_sd_element_new(element->name);
        }
      else
        { 
          /* Append the next element to the previous */
          new_element = log_msg_sd_element_append(new_element, element->name);
        }
        
      new_param = NULL;
      while (param)
        {
          if (new_param)
            new_param = log_msg_sd_param_append(new_param, param->name, param->value);
          else
            new_element->params = new_param = log_msg_sd_param_new(param->name, param->value);
          param = param->next_param;
        }
      element = element->next_element;
    }
  log_msg_set_flag(msg, LF_OWN_SDATA);
}

void 
log_msg_update_sdata(LogMessage *msg, const gchar *elem_name, const gchar *param_name, const gchar *param_value)
{
  LogMessageSDElement *element = NULL;
  LogMessageSDParam *param = NULL;

  if (!log_msg_chk_flag(msg, LF_OWN_SDATA))
    log_msg_clone_sdata(msg);

  element = log_msg_lookup_sdata_element(msg, elem_name);
  if (!element)
    {
      if (msg->sdata)
        { 
          /* sd_elements contains some elements */
          element = msg->sdata;
          /* find the last one*/
          while (element->next_element != NULL)
            element = element->next_element;
          element = log_msg_sd_element_append(element, elem_name);
        }
      else
        { 
          /* sd_elements contain no elements so create a new one and store the pointer in the msg structure*/
          element = log_msg_sd_element_new(elem_name);
          msg->sdata = element; 
        }
    }

  /* no try to find the param in the current element */
  param = log_msg_sd_element_lookup(element, param_name);
  if (!param)
    { 
      if (element->params)
        {
          /* the element has no such param */
          param = element->params;
          while (param->next_param != NULL)
            param = param->next_param;
          log_msg_sd_param_append(param, param_name, param_value);
        }
      else
        {
          element->params = log_msg_sd_param_new(param_name, param_value);
        }
    }
  else
    {
      g_free(param->value);
      param->value = g_strdup(param_value);
    }
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
log_msg_parse_column(LogMessage *self, gchar **result, gint *result_len, const guchar **data, gint *length, gint max_length)
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
      if ((*length - left) == 1 && (*data)[0] == '-')
        {
          *result = NULL;
        }
      else
        {
          *result_len = (*length - left) > max_length ? max_length : (*length - left);
          *result = g_strndup((gchar *) *data, *result_len);
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
      p = (guchar *) strptime((gchar *) date, "%Y-%m-%d T%H:%M:%S", &tm);

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
            
          tm.tm_isdst = -1;
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
    self->timestamps[LM_TS_STAMP].zone_offset = assume_timezone;
   
  if (self->timestamps[LM_TS_STAMP].zone_offset != -1)
    self->timestamps[LM_TS_STAMP].time.tv_sec = self->timestamps[LM_TS_STAMP].time.tv_sec + get_local_timezone_ofs(self->timestamps[LM_TS_STAMP].time.tv_sec) - (tm.tm_hour - unnormalized_hour) * 3600 - self->timestamps[LM_TS_STAMP].zone_offset;
  else
    self->timestamps[LM_TS_STAMP].zone_offset = get_local_timezone_ofs(self->timestamps[LM_TS_STAMP].time.tv_sec);

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
  self->program_len = src - prog_start;
  LOG_MESSAGE_WRITABLE_FIELD(self->program) = g_strndup((gchar *) prog_start, self->program_len);
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
          self->pid_len = src - pid_start;
          LOG_MESSAGE_WRITABLE_FIELD(self->pid) = g_strndup((gchar *) pid_start, self->pid_len);
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
  if (flags & LP_STORE_LEGACY_MSGHDR)
    {
      log_msg_set_value(self, "LEGACY_MSGHDR", g_strndup((gchar *) *data, *length - left), *length - left);
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
  gchar sd_param_name[33];

  /* UTF-8 string */
  gchar sd_param_value[256];
  
  guint open_sd = 0;
  gint left = *length, pos;
  LogMessageSDElement *element = NULL; 
  LogMessageSDParam *param = NULL;

  /* this function will allocate the first element pointer */
  g_assert(!self->sdata);

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
              if (pos < sizeof(sd_id_name))
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
 
          if (pos > 0)
            {
              sd_id_name[pos] = 0;
              if (element)
                {
                  element = log_msg_sd_element_append(element, sd_id_name);
                }
              else
                {
                  self->sdata = log_msg_sd_element_new(sd_id_name);
                  element = self->sdata;
                } 
              /* start a new parameter list to the new element */
              param = NULL;
            }

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
                  if (pos < sizeof(sd_param_name))
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

              if (left && *src == '=')
                sd_step_and_store(self, &src, &left);
              else
                goto error;

             /* read sd-param-value */

             if (left && *src == '"')
               {
                 /* opening quote */
                 sd_step_and_store(self, &src, &left);
                 pos = 0;

                 while (left && *src != '"')
                   {
                     if (pos < sizeof(sd_param_value))
                       {
                         sd_param_value[pos] = *src;
                         pos++;
                       }
                     sd_step_and_store(self, &src, &left);
                   }
                 sd_param_value[pos] = 0;
                 if (left && *src == '"')/* closing quote */
                   sd_step_and_store(self, &src, &left);
                else
                   goto error;
               }
             else
               {
                 goto error;
               }

             if (param)
               param = log_msg_sd_param_append(param, sd_param_name, sd_param_value);
             else
               param = element->params = log_msg_sd_param_new(sd_param_name, sd_param_value);

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
  gint value_len;
  
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
      LOG_MESSAGE_WRITABLE_FIELD(self->host) = g_strndup((gchar *) hostname_start, hostname_len);
      self->host_len = hostname_len;
    }

  /* application name 48 ascii*/
  log_msg_parse_column(self, &LOG_MESSAGE_WRITABLE_FIELD(self->program), &value_len, &src, &left, 48);
  self->program_len = value_len;
  if (!log_msg_parse_skip_space(self, &src, &left))
    return FALSE;
      
  /* process id 128 ascii */
  log_msg_parse_column(self, &LOG_MESSAGE_WRITABLE_FIELD(self->pid), &value_len, &src, &left, 128);
  self->pid_len = value_len;
  if (!log_msg_parse_skip_space(self, &src, &left))
    return FALSE;
 
  /* message id 32 ascii */
  log_msg_parse_column(self, &LOG_MESSAGE_WRITABLE_FIELD(self->msgid), &value_len, &src, &left, 32);
  self->msgid_len = value_len;
  if (!log_msg_parse_skip_space(self, &src, &left))
    return FALSE;

  /* structured data part */
  if (!log_msg_parse_sd(self, &src, &left, flags))
    return FALSE;
    
  /* optional part of the log message [SP MSG]*/
  if (!log_msg_parse_skip_space(self, &src, &left))
    return FALSE;
  
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
  LOG_MESSAGE_WRITABLE_FIELD(self->message) = g_strndup((gchar *) src, left);
  self->message_len = left;
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
          /* It's a regular ol' message. */
          log_msg_parse_hostname(self, &src, &left, &hostname_start, &hostname_len, flags, bad_hostname);
    
          /* Skip whitespace. */
          log_msg_parse_skip_chars(self, &src, &left, " ", -1);

          /* Try to extract a program name */
          log_msg_parse_legacy_program_name(self, &src, &left, flags);
        }

      /* If we did manage to find a hostname, store it. */
      if (hostname_start)
        {
          LOG_MESSAGE_WRITABLE_FIELD(self->host) = g_strndup((gchar *) hostname_start, hostname_len);
          self->host_len = hostname_len;
        }
    }
  else
    {
      /* no timestamp, format is expected to be "program[pid] message" */
      /* Different format */

      /* A kernel message? Use 'kernel' as the program name. */
      if ((self->flags & LF_INTERNAL) == 0 && ((self->pri & LOG_FACMASK) == LOG_KERN))
        {
          LOG_MESSAGE_WRITABLE_FIELD(self->program) = g_strndup("kernel", 6);
          self->program_len = 6;
        }
      /* No, not a kernel message. */
      else
        {
          /* Capture the program name */
          log_msg_parse_legacy_program_name(self, &src, &left, flags);
        }
      self->timestamps[LM_TS_STAMP] = self->timestamps[LM_TS_RECVD];
    }

  LOG_MESSAGE_WRITABLE_FIELD(self->message) = g_strndup((gchar *) src, left);
  self->message_len = left;
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
      LOG_MESSAGE_WRITABLE_FIELD(self->message) = g_strndup((gchar *) data, length);
      self->pri = default_pri;
      self->message_len = length;
      goto exit;
    }
  
  if (G_UNLIKELY(flags & LP_INTERNAL))
    self->flags |= LF_INTERNAL;
  if (flags & LP_LOCAL)
    self->flags |= LF_LOCAL;
  if (flags & LP_ASSUME_UTF8)
    self->flags |= LF_UTF8;

  if (flags & LP_SYSLOG_PROTOCOL)
    success = log_msg_parse_syslog_proto(self, data, length, flags, assume_timezone, default_pri);
  else
    success = log_msg_parse_legacy(self, data, length, flags, bad_hostname, assume_timezone, default_pri);
  if (G_UNLIKELY(!success))
    {
      self->timestamps[LM_TS_STAMP] = self->timestamps[LM_TS_RECVD];
      log_msg_set_host(self, g_strdup(""), 0);
      log_msg_set_message(self, g_strdup_printf("Error processing log message: %.*s", length, data), -1);
      log_msg_set_program(self, g_strndup("syslog-ng", 9), 9);
      log_msg_set_pid(self, g_strdup_printf("%d", (int) getpid()), -1);
      log_msg_set_msgid(self, g_strdup(""), 0);

      if (self->sdata)
        {
          log_msg_sd_elements_free(self->sdata);
          self->sdata = NULL;
        }
      self->pri = LOG_SYSLOG | LOG_ERR;
      goto exit;
    }

  if (G_UNLIKELY(flags & LP_NO_MULTI_LINE))
    {
      p = self->message;
      while ((p = find_cr_or_lf(p, self->message + self->message_len - p)))
        {
          *p = ' ';
          p++;
        }

    }
 exit:

#define ENSURE_STRING_VALUE(field, macro) \
  do                                            \
    {                                           \
      if (!self->field)                         \
        {                                       \
          LOG_MESSAGE_WRITABLE_FIELD(self->field) = null_string; \
          self->field##_len = 0;                \
          self->flags &= ~LF_OWN_##macro;       \
        }                                       \
    }                                           \
  while (0)

  ENSURE_STRING_VALUE(host, HOST);
  ENSURE_STRING_VALUE(host_from, HOST_FROM);
  ENSURE_STRING_VALUE(message, MESSAGE);
  ENSURE_STRING_VALUE(program, PROGRAM);
  ENSURE_STRING_VALUE(pid, PID);
  ENSURE_STRING_VALUE(msgid, MSGID);
  ENSURE_STRING_VALUE(source, SOURCE);

#undef ENSURE_STRING_VALUE

  return;
}

/**
 * NOTE: acquires a reference to the strings, e.g. it takes care of freeing
 * them.
 **/
void
log_msg_add_dyn_value_ref(LogMessage *self, gchar *name, gchar *value)
{
  if (!log_msg_chk_flag(self, LF_OWN_VALUES))
   {
     self->values = NULL;
     log_msg_set_flag(self, LF_OWN_VALUES);
   }
  if (!self->values)
    self->values = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  
  g_hash_table_insert(self->values, name, value);
}

void
log_msg_add_dyn_value(LogMessage *self, const gchar *name, const gchar *value)
{
  log_msg_add_dyn_value_ref(self, g_strdup(name), g_strdup(value));
}

void
log_msg_add_sized_dyn_value(LogMessage *self, const gchar *name, const gchar *value, gsize value_len)
{
  log_msg_add_dyn_value_ref(self, g_strdup(name), g_strndup(value, value_len));
}

gchar * 
log_msg_lookup_dyn_value(LogMessage *self, const gchar *name)
{
  gchar *value = NULL;

  if (log_msg_chk_flag(self, LF_OWN_VALUES))
    {
      if (self->values)
        value = (gchar *) g_hash_table_lookup(self->values, name);
      
      if (!value && self->original)
        value = log_msg_lookup_dyn_value(self->original, name);
    }
  else if (self->original)
    {
      value = log_msg_lookup_dyn_value(self->original, name);
    }
  return value;
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
  if (log_msg_chk_flag(self, LF_OWN_HOST))
    g_free(self->host);
  if (log_msg_chk_flag(self, LF_OWN_HOST_FROM))
    g_free(self->host_from);
  if (log_msg_chk_flag(self, LF_OWN_PID))
    g_free(self->pid);
  if (log_msg_chk_flag(self, LF_OWN_MSGID))
    g_free(self->msgid);
  if (log_msg_chk_flag(self, LF_OWN_PROGRAM))
    g_free(self->program);
  if (log_msg_chk_flag(self, LF_OWN_MESSAGE))
    g_free(self->message);
  if (log_msg_chk_flag(self, LF_OWN_SOURCE))
    g_free(self->source);
  log_msg_clear_matches(self);

  if (log_msg_chk_flag(self, LF_OWN_SDATA) && self->sdata)
    log_msg_sd_elements_free(self->sdata);
  if (log_msg_chk_flag(self, LF_OWN_SADDR))
    g_sockaddr_unref(self->saddr);

  if (log_msg_chk_flag(self, LF_OWN_VALUES) && self->values)
    g_hash_table_destroy(self->values);

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
  g_get_current_time(&self->timestamps[LM_TS_RECVD].time);
  self->timestamps[LM_TS_RECVD].zone_offset = get_local_timezone_ofs(self->timestamps[LM_TS_RECVD].time.tv_sec);
  self->timestamps[LM_TS_STAMP].time.tv_sec = -1;
  self->timestamps[LM_TS_STAMP].zone_offset = -1;
 
  self->sdata = NULL;
  self->saddr = g_sockaddr_ref(saddr);

  self->original = NULL;
  self->flags |= LF_OWN_ALL;


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
  log_msg_parse(self, (guchar *) msg, length, flags, bad_hostname, assume_timezone, default_pri);
  return self;
}

LogMessage *
log_msg_new_empty(void)
{
  LogMessage *self = g_new0(LogMessage, 1);
  
  log_msg_init(self, NULL);
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
  if ((msg->flags & LF_OWN_ALL) == 0)
    {
      /* the message we're cloning has no original content, everything
       * is referenced from its "original", use that with this clone
       * as well, effectively avoiding the "referenced" flag on the
       * clone. */
      msg = msg->original;
    }
  msg->flags |= LF_REFERENCED;

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
  
  self->flags = self->flags & ~LF_OWN_ALL;
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


