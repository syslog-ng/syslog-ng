/*
 * Copyright (c) 2012-2013 Balabit
 * Copyright (c) 2012-2013 Gergely Nagy <algernon@balabit.hu>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "linux-kmsg-format.h"
#include "logmsg/logmsg.h"
#include "messages.h"
#include "cfg.h"
#include "str-format.h"
#include "scratch-buffers.h"
#include "timeutils/misc.h"

#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

static NVHandle KMSG_LM_V_DEV_TYPE;
static NVHandle KMSG_LM_V_DEV_MINOR;
static NVHandle KMSG_LM_V_DEV_MAJOR;
static NVHandle KMSG_LM_V_DEV_NAME;
static NVHandle KMSG_LM_V_NETDEV_INDEX;
static NVHandle KMSG_LM_V_TIMESTAMP;
static struct timeval boot_time;

/*
 * The linux (3.5+) /dev/kmsg format looks like this:
 *
 * 6,802,65338577;ATL1E 0000:02:00.0: eth0: NIC Link is Up <100 Mbps Full Duplex>
 *  SUBSYSTEM=pci
 *  DEVICE=+pci:0000:02:00.0
 *
 * Where the first number is the syslog priority & facility, the
 * second is a 64-bit sequence number, the third is a monotonic
 * timestamp (where 0 is the boot time) in microseconds.
 *
 * Following the timestamp, there may be other items in the future,
 * all comma-separated - these should be gracefully ignored.
 *
 * The message itself starts after the first ';', and lasts until the
 * first newline.
 *
 * After the first newline, we have key-value pairs, each one line,
 * starting with whitespace, the first '=' is the divisor.
 *
 * The "DEVICE" key can be further processed, its value is any of the
 * following formats:
 *
 *  - b1:2 - a block dev_t, with major:minor
 *  - c1:2 - a char dev_t, with major:minor
 *  - n1 - netdev with device index
 *  - +subsystem:device
 *
 * This implementation parses each of those, and also gracefully
 * handles unknown formats.
 */

#ifdef __linux__
static guint64
kmsg_timeval_diff(struct timeval *t1, struct timeval *t2)
{
  return ((guint64)t1->tv_sec - (guint64)t2->tv_sec) * G_USEC_PER_SEC +
         ((guint64)t1->tv_usec - (guint64)t2->tv_usec);
}
#endif

static void
kmsg_to_absolute_time(guint64 timestamp, UnixTime *dest)
{
  guint64 t;

  t = (boot_time.tv_sec + (timestamp / G_USEC_PER_SEC)) * G_USEC_PER_SEC +
      boot_time.tv_usec + (timestamp % G_USEC_PER_SEC);

  dest->ut_sec = t / G_USEC_PER_SEC;
  dest->ut_usec = t % G_USEC_PER_SEC;
}

static gboolean
kmsg_parse_prio(const guchar *data, gsize *pos, gsize length, LogMessage *msg)
{
  gint pri = 0;

  while (*pos < length && data[*pos] != ',')
    {
      if (isdigit(data[*pos]))
        pri = pri * 10 + ((data[*pos]) - '0');
      else
        return FALSE;
      (*pos)++;
    }
  if (data[*pos] != ',' || *pos == length)
    return FALSE;

  msg->pri = pri;
  return TRUE;
}

static gboolean
kmsg_parse_seq(const guchar *data, gsize *pos, gsize length, LogMessage *msg)
{
  gsize start = *pos;

  while (*pos < length && data[*pos] != ',')
    {
      if (!isdigit(data[*pos]))
        return TRUE;
      (*pos)++;
    }
  if (data[*pos] != ',' || *pos == length)
    return FALSE;

  log_msg_set_value(msg, LM_V_MSGID, (const gchar *)data + start, *pos - start);
  return TRUE;
}

static gboolean
kmsg_parse_timestamp(const guchar *data, gsize *pos, gsize length, LogMessage *msg)
{
  guint64 timestamp = 0;
  gsize start = *pos;

  while (*pos < length && data[*pos] != ',' && data[*pos] != ';')
    {
      if (isdigit(data[*pos]))
        timestamp = timestamp * 10 + ((data[*pos]) - '0');
      else
        return FALSE;
      (*pos)++;
    }
  if ((data[*pos] != ',' && data[*pos] != ';') || *pos == length)
    return FALSE;

  log_msg_set_value(msg, KMSG_LM_V_TIMESTAMP,
                    (const gchar *)data + start, *pos - start);
  kmsg_to_absolute_time(timestamp, &msg->timestamps[LM_TS_STAMP]);
  msg->timestamps[LM_TS_STAMP].ut_gmtoff =
    get_local_timezone_ofs(msg->timestamps[LM_TS_STAMP].ut_sec);

  return TRUE;
}

static gboolean
kmsg_skip_to_message(const guchar *data, gsize *pos, gsize length)
{
  while (*pos < length && data[*pos] != ';')
    (*pos)++;

  if (data[*pos] != ';' || *pos == length)
    return FALSE;
  return TRUE;
}

static gsize
kmsg_parse_message(const guchar *data, gsize *pos, gsize length, LogMessage *msg)
{
  gsize start = *pos;

  while (*pos < length && data[*pos] != '\n')
    (*pos)++;
  if (data[*pos] != '\n')
    return FALSE;

  log_msg_set_value(msg, LM_V_MESSAGE, (const gchar *)data + start,
                    *pos - start);
  return TRUE;
}

static gboolean
kmsg_is_key_value_pair_device(const guchar *name, gsize length)
{
  if (strncmp((const char *)name, "DEVICE=", length + 1) == 0)
    return TRUE;
  return FALSE;
}

static void
kmsg_parse_device_dev_t(const gchar *type,
                        const guchar *value, gsize length,
                        LogMessage *msg)
{
  gsize seppos = 0;

  log_msg_set_value(msg, KMSG_LM_V_DEV_TYPE, type, -1);

  while (seppos < length && value[seppos] != ':')
    seppos++;

  log_msg_set_value(msg, KMSG_LM_V_DEV_MAJOR,
                    (const gchar *)value, seppos);
  log_msg_set_value(msg, KMSG_LM_V_DEV_MINOR,
                    (const gchar *)value + seppos + 1, length - seppos - 1);
}

static void
kmsg_parse_device_netdev(const guchar *value, gsize length,
                         LogMessage *msg)
{
  log_msg_set_value(msg, KMSG_LM_V_DEV_TYPE, "netdev", -1);
  log_msg_set_value(msg, KMSG_LM_V_NETDEV_INDEX,
                    (const gchar *)value, length);
}

static void
kmsg_parse_device_subsys(const guchar *value, gsize length,
                         LogMessage *msg)
{
  gsize seppos = 0;

  while (seppos < length && value[seppos] != ':')
    seppos++;

  log_msg_set_value(msg, KMSG_LM_V_DEV_TYPE,
                    (const gchar *)value, seppos);
  log_msg_set_value(msg, KMSG_LM_V_DEV_NAME,
                    (const gchar *)value + seppos + 1, length - seppos - 1);
}

static void
kmsg_parse_device_unknown(const guchar *value, gsize length,
                          LogMessage *msg)
{
  log_msg_set_value(msg, KMSG_LM_V_DEV_TYPE, "<unknown>", -1);
  log_msg_set_value(msg, KMSG_LM_V_DEV_NAME,
                    (const gchar *)value, length);

}

static void
kmsg_parse_device_key_value_pair(const guchar *value, gsize length,
                                 LogMessage *msg)
{
  switch (value[0])
    {
    case 'b':
      kmsg_parse_device_dev_t("block", value + 1, length - 1, msg);
      break;
    case 'c':
      kmsg_parse_device_dev_t("char", value + 1, length - 1, msg);
      break;
    case 'n':
      kmsg_parse_device_netdev(value + 1, length - 1, msg);
      break;
    case '+':
      kmsg_parse_device_subsys(value + 1, length - 1, msg);
      break;
    default:
      kmsg_parse_device_unknown(value, length, msg);
      break;
    }
}

static gboolean
kmsg_parse_key_value_pair(const guchar *data, gsize *pos, gsize length,
                          LogMessage *msg)
{
  gsize name_start, name_len, value_start, value_len;
  GString *name;

  while (*pos < length && (data[*pos] == ' ' || data[*pos] == '\t'))
    (*pos)++;
  if (*pos == length)
    return FALSE;
  name_start = *pos;

  while (*pos < length && data[*pos] != '=')
    (*pos)++;
  if (*pos == length)
    return FALSE;
  name_len = *pos - name_start;
  value_start = ++(*pos);

  while (*pos < length && data[*pos] != '\n')
    (*pos)++;
  if (data[*pos] != '\n')
    return FALSE;
  value_len = *pos - value_start;

  if (kmsg_is_key_value_pair_device(data + name_start, name_len))
    {
      kmsg_parse_device_key_value_pair(data + value_start, value_len,
                                       msg);
      return TRUE;
    }

  name = scratch_buffers_alloc();

  g_string_assign(name, ".linux.");
  g_string_append_len(name, (const gchar *)data + name_start, name_len);

  log_msg_set_value(msg,
                    log_msg_get_value_handle(name->str),
                    (const gchar *)data + value_start, value_len);

  return TRUE;
}

static gboolean
log_msg_parse_kmsg(LogMessage *msg, const guchar *data, gsize length, gint *position)
{
  gsize pos = 0;

  if (!kmsg_parse_prio(data, &pos, length, msg))
    goto error;

  pos++;
  if (!kmsg_parse_seq(data, &pos, length, msg))
    goto error;

  pos++;
  if (!kmsg_parse_timestamp(data, &pos, length, msg))
    goto error;

  if (!kmsg_skip_to_message(data, &pos, length))
    goto error;

  pos++;
  if (!kmsg_parse_message(data, &pos, length, msg))
    goto error;

  if (pos + 1 >= length)
    return TRUE;

  do
    {
      pos++;
      if (!kmsg_parse_key_value_pair(data, &pos, length, msg))
        goto error;
    }
  while (pos < length);

  return TRUE;

error:
  *position = pos;
  return FALSE;
}

void
linux_kmsg_format_handler(const MsgFormatOptions *parse_options,
                          const guchar *data, gsize length,
                          LogMessage *self)
{
  gboolean success;
  gint problem_position = 0;

  while (length > 0 && (data[length - 1] == '\n' || data[length - 1] == '\0'))
    length--;

  if (parse_options->flags & LP_NOPARSE)
    {
      log_msg_set_value(self, LM_V_MESSAGE, (gchar *) data, length);
      self->pri = parse_options->default_pri;
      return;
    }

  self->flags |= LF_UTF8;

  if (parse_options->flags & LP_LOCAL)
    self->flags |= LF_LOCAL;

  self->initial_parse = TRUE;

  success = log_msg_parse_kmsg(self, data, length, &problem_position);

  self->initial_parse = FALSE;

  if (G_UNLIKELY(!success))
    {
      msg_format_inject_parse_error(self, data, length, problem_position);
      return;
    }
}

#ifdef __linux__
static void
kmsg_init_boot_time(void)
{
  int fd, pos = 0;
  gchar buf[1024];
  ssize_t rc;
  struct timeval curr_time;
  guint64 tdiff;

  if ((fd = open ("/proc/uptime", O_RDONLY)) == -1)
    return;

  if ((rc = read (fd, buf, sizeof(buf))) <= 0)
    {
      close(fd);
      return;
    }
  close(fd);

  gettimeofday(&curr_time, NULL);

  /* Read the seconds part */
  while (pos < rc && buf[pos] != '.')
    {
      if (isdigit(buf[pos]))
        boot_time.tv_sec = boot_time.tv_sec * 10 + ((buf[pos]) - '0');
      else
        {
          boot_time.tv_sec = 0;
          return;
        }
      pos++;
    }
  pos++;

  /* Then the microsecond part */
  while (pos < rc && buf[pos] != ' ')
    {
      if (isdigit(buf[pos]))
        boot_time.tv_usec = boot_time.tv_usec * 10 + ((buf[pos]) - '0');
      else
        {
          boot_time.tv_sec = 0;
          boot_time.tv_usec = 0;
          return;
        }
      pos++;
    }

  tdiff = kmsg_timeval_diff(&curr_time, &boot_time);
  boot_time.tv_sec = tdiff / G_USEC_PER_SEC;
  boot_time.tv_usec = tdiff % G_USEC_PER_SEC;
}
#endif

void
linux_msg_format_init(void)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      KMSG_LM_V_DEV_TYPE = log_msg_get_value_handle(".linux.DEVICE.type");
      KMSG_LM_V_DEV_MINOR = log_msg_get_value_handle(".linux.DEVICE.minor");
      KMSG_LM_V_DEV_MAJOR = log_msg_get_value_handle(".linux.DEVICE.major");
      KMSG_LM_V_DEV_NAME = log_msg_get_value_handle(".linux.DEVICE.name");
      KMSG_LM_V_NETDEV_INDEX = log_msg_get_value_handle(".linux.DEVICE.index");
      KMSG_LM_V_TIMESTAMP = log_msg_get_value_handle(".linux.timestamp");

#ifdef __linux__
      kmsg_init_boot_time();
#endif

      initialized = TRUE;
    }
}
