/*
 * Copyright (c) 2018 Balabit
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

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <glib.h>

#include "logline_generator.h"
#include "loggen_helper.h"

static char line_buf_template[MAX_MESSAGE_LENGTH + 1];
static int pos_timestamp1 = 0;
static int pos_timestamp2 = 0;
static int pos_seq = 0;
static int pos_thread_id = 0;

int
prepare_log_line_template(int syslog_proto, int framing, int message_length, char *sdata_value)
{
  int linelen = 0;
  char padding[] = "PADD";
  int hdr_len = 0;
  struct timeval now;
  gettimeofday(&now, NULL);
  int run_id = now.tv_sec;

  int buffer_length = sizeof(line_buf_template);

  if (framing)
    hdr_len = snprintf(line_buf_template, buffer_length, "%d ", message_length);
  else
    hdr_len = 0;

  if (syslog_proto)
    {
      const char *sdata = sdata_value ? sdata_value : "-";

      linelen = snprintf(line_buf_template + hdr_len, buffer_length - hdr_len,
                         "<38>1 2007-12-24T12:28:51+02:00 localhost prg%05d 1234 - %s \xEF\xBB\xBFseq: %010d, thread: %04d, runid: %-10d, stamp: %-19s ",
                         0, sdata, 0, 0, run_id, "");

      pos_timestamp1 = 6 + hdr_len;
      pos_seq = 68 + hdr_len + strlen(sdata) - 1;
      pos_thread_id = pos_seq + 20;
      pos_timestamp2 = 120 + hdr_len + strlen(sdata) - 1;
    }
  else
    {
      linelen = snprintf(line_buf_template + hdr_len, buffer_length - hdr_len,
                         "<38>2007-12-24T12:28:51 localhost prg%05d[1234]: seq: %010d, thread: %04d, runid: %-10d, stamp: %-19s ", 0, 0,
                         0, run_id, "");
      pos_timestamp1 = 4 + hdr_len;
      pos_seq = 55 + hdr_len;
      pos_thread_id = pos_seq + 20;
      pos_timestamp2 = 107 + hdr_len;
    }

  if (linelen > message_length)
    {
      ERROR("warning: message length is too small, the minimum is %d bytes\n", linelen);
      return 0;
    }

  for (int i = linelen; i < message_length - 1; i++)
    {
      line_buf_template[i + hdr_len] = padding[(i - linelen) % (sizeof(padding) - 1)];
    }

  line_buf_template[hdr_len + message_length - 1] = '\n';
  line_buf_template[hdr_len + message_length] = 0;

  return linelen;
}

int
generate_log_line(char *buffer, int buffer_length, int syslog_proto, int thread_id, unsigned long seq)
{
  if (!buffer)
    {
      ERROR("invalid buffer\n");
      return -1;
    }

  /* make a copy of logline template */
  memcpy(buffer, line_buf_template, buffer_length < MAX_MESSAGE_LENGTH ? buffer_length : MAX_MESSAGE_LENGTH);

  /* create time stamps */
  struct timeval now;
  gettimeofday(&now, NULL);
  struct tm tm;
  char stamp[32];
  localtime_r(&now.tv_sec, &tm);
  int len = strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S", &tm);
  memcpy(&buffer[pos_timestamp2], stamp, len);

  if (syslog_proto)
    format_timezone_offset_with_colon(stamp, sizeof(stamp), &tm);

  memcpy(&buffer[pos_timestamp1], stamp, strlen(stamp));

  /* print sequence number to logline */
  char intbuf[16];
  snprintf(intbuf, sizeof(intbuf), "%010ld", seq);
  memcpy(&buffer[pos_seq], intbuf, 10);

  /* print thread id */
  char thread_id_buff[5];
  snprintf(thread_id_buff, sizeof(thread_id_buff), "%04d", thread_id);
  memcpy(&buffer[pos_thread_id], thread_id_buff, 4);

  return strlen(buffer);
}

