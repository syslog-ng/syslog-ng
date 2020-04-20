/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#include "pacct-format.h"
#include "logmsg/logmsg.h"
#include "logproto/logproto-record-server.h"

/* we're using the Linux header as the glibc one is incomplete */
#include <linux/acct.h>

typedef struct acct_v3 acct_t;

#define PACCT_PREFIX ".pacct."

static gboolean handles_registered = FALSE;
static NVHandle handle_ac_flag;
static NVHandle handle_ac_tty;
static NVHandle handle_ac_exitcode;
static NVHandle handle_ac_uid;
static NVHandle handle_ac_gid;
static NVHandle handle_ac_pid;
static NVHandle handle_ac_ppid;
static NVHandle handle_ac_btime;
static NVHandle handle_ac_etime;
static NVHandle handle_ac_utime;
static NVHandle handle_ac_stime;
static NVHandle handle_ac_mem;
static NVHandle handle_ac_io;
static NVHandle handle_ac_rw;
static NVHandle handle_ac_minflt;
static NVHandle handle_ac_majflt;
static NVHandle handle_ac_swaps;
static NVHandle handle_ac_comm;

#define PACCT_REGISTER(field) \
  do {                        \
    handle_##field = log_msg_get_value_handle(PACCT_PREFIX # field);        \
  } while(0)

#define PACCT_CONVERT_NOP(x) (x)

#define PACCT_CONVERT_COMP_TO_ULONG(x) ((ulong) ((x & 0x1fff) << (((x >> 13) & 0x7) * 3)))

#define PACCT_CONVERT_COMP_TO_DOUBLE(x) ((double) x)

#define PACCT_FORMAT_CONVERT(msg, rec, field, format, convert)                  \
  do {                                                                  \
    gchar __buf[64];                                                    \
    gsize __len;                                                        \
                                                                        \
    __len = g_snprintf(__buf, sizeof(__buf), format, convert(rec->field)); \
    log_msg_set_value(msg, handle_##field, __buf, __len);                     \
  } while (0)

#define PACCT_FORMAT(msg, rec, field, format) \
  PACCT_FORMAT_CONVERT(msg, rec, field, format, PACCT_CONVERT_NOP)

void
pacct_register_handles(void)
{
  PACCT_REGISTER(ac_flag);
  PACCT_REGISTER(ac_tty);
  PACCT_REGISTER(ac_exitcode);
  PACCT_REGISTER(ac_uid);
  PACCT_REGISTER(ac_gid);
  PACCT_REGISTER(ac_pid);
  PACCT_REGISTER(ac_ppid);
  PACCT_REGISTER(ac_btime);
  PACCT_REGISTER(ac_etime);
  PACCT_REGISTER(ac_utime);
  PACCT_REGISTER(ac_stime);
  PACCT_REGISTER(ac_mem);
  PACCT_REGISTER(ac_io);
  PACCT_REGISTER(ac_rw);
  PACCT_REGISTER(ac_minflt);
  PACCT_REGISTER(ac_majflt);
  PACCT_REGISTER(ac_swaps);
  PACCT_REGISTER(ac_comm);
}

void
pacct_format_handler(const MsgFormatOptions *options, const guchar *data, gsize length, LogMessage *msg)
{
  acct_t *rec;
  gsize len;

  if (length < sizeof(*rec))
    {
      gchar *buf;

      buf = g_strdup_printf("Error parsing process accounting record, record too small; rec_size='%d', expected_size='%d'",
                            (gint) length, (gint) sizeof(*rec));
      log_msg_set_value(msg, LM_V_MESSAGE, buf, -1);
      g_free(buf);
      return;
    }
  rec = (acct_t *) data;
  if (rec->ac_version != 3)
    {
      gchar *buf;

      buf = g_strdup_printf("Error parsing process accounting record, only the v3 format is supported; version='%d'",
                            rec->ac_version);
      log_msg_set_value(msg, LM_V_MESSAGE, buf, -1);
      g_free(buf);
      return;
    }
  if (G_UNLIKELY(!handles_registered))
    {
      pacct_register_handles();
      handles_registered = TRUE;
    }
  PACCT_FORMAT(msg, rec, ac_flag, "%02x");
  PACCT_FORMAT(msg, rec, ac_tty, "%u");
  PACCT_FORMAT(msg, rec, ac_exitcode, "%u");
  PACCT_FORMAT(msg, rec, ac_uid, "%u");
  PACCT_FORMAT(msg, rec, ac_gid, "%u");
  PACCT_FORMAT(msg, rec, ac_pid, "%u");
  PACCT_FORMAT(msg, rec, ac_ppid, "%u");
  PACCT_FORMAT_CONVERT(msg, rec, ac_btime, "%lu.00", PACCT_CONVERT_COMP_TO_ULONG);
  PACCT_FORMAT_CONVERT(msg, rec, ac_etime, "%9.2f", PACCT_CONVERT_COMP_TO_DOUBLE);
  PACCT_FORMAT_CONVERT(msg, rec, ac_utime, "%lu.00", PACCT_CONVERT_COMP_TO_ULONG);
  PACCT_FORMAT_CONVERT(msg, rec, ac_stime, "%lu.00", PACCT_CONVERT_COMP_TO_ULONG);
  PACCT_FORMAT_CONVERT(msg, rec, ac_mem, "%lu", PACCT_CONVERT_COMP_TO_ULONG);
  PACCT_FORMAT_CONVERT(msg, rec, ac_io, "%lu", PACCT_CONVERT_COMP_TO_ULONG);
  PACCT_FORMAT_CONVERT(msg, rec, ac_rw, "%lu", PACCT_CONVERT_COMP_TO_ULONG);
  PACCT_FORMAT_CONVERT(msg, rec, ac_minflt, "%lu", PACCT_CONVERT_COMP_TO_ULONG);
  PACCT_FORMAT_CONVERT(msg, rec, ac_majflt, "%lu", PACCT_CONVERT_COMP_TO_ULONG);
  PACCT_FORMAT_CONVERT(msg, rec, ac_swaps, "%lu", PACCT_CONVERT_COMP_TO_ULONG);
  if (rec->ac_comm[ACCT_COMM - 1] == 0)
    len = strlen(rec->ac_comm);
  else
    len = ACCT_COMM;
  log_msg_set_value(msg, handle_ac_comm, rec->ac_comm, len);
}

static LogProtoServer *
pacct_construct_proto(const MsgFormatOptions *options, LogTransport *transport,
                      const LogProtoServerOptions *proto_options)
{
  return log_proto_binary_record_server_new(transport, proto_options, sizeof(acct_t));
}

MsgFormatHandler pacct_handler =
{
  .construct_proto = pacct_construct_proto,
  .parse = &pacct_format_handler
};
