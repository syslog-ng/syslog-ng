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

#include "file_reader.h"
#include "loggen_helper.h"

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>


static int loop_reading = 0;
static int dont_parse = 0;
static int skip_tokens = 0;
static char *read_file_name = NULL;

static GOptionEntry loggen_file_reader_options[] =
{
  { "read-file", 'R', 0, G_OPTION_ARG_STRING, &read_file_name, "Read log messages from file", "<filename>" },
  { "loop-reading", 'l', 0, G_OPTION_ARG_NONE, &loop_reading, "Read the file specified in read-file option in loop (it will restart the reading if reached the end of the file)", NULL },
  { "skip-tokens", 0, 0, G_OPTION_ARG_INT, &skip_tokens, "Skip the given number of tokens (delimined by a space) at the beginning of each line (default value: 0)", "<number>" },
  { "dont-parse", 'd', 0, G_OPTION_ARG_NONE, &dont_parse, "Don't parse the lines coming from the readed files. Loggen will send the whole lines as it is in the readed file", NULL },
  { NULL }
};

FILE **source; /* array for file handlers */

GOptionEntry *
get_file_reader_options(void)
{
  return loggen_file_reader_options;
}

int init_file_reader(int nr_threads)
{
  if (!read_file_name)
    {
      DEBUG("file reader not activated\n");
      return 0;
    }

  source = (FILE **)g_new0(FILE *, nr_threads);

  if (read_file_name[0] == '-' && read_file_name[1] == '\0')
    {
      if (nr_threads > 1)
        {
          ERROR("Only one thread can read from stdin. Please start loggen with --active-connections=1\n");
          return -1;
        }

      if (nr_threads == 1)
        source[0] = stdin;
    }
  else
    {
      for (int i=0; i < nr_threads; i++)
        {
          source[i] = fopen(read_file_name, "r");
          if (!source[i])
            {
              ERROR("unable to open file (%s)\n",read_file_name);
              return -1;
            }
        }

      DEBUG("file (%s) opened for reading\n",read_file_name);
    }
  return 1;
}

void close_file_reader(int nr_threads)
{
  if (!source)
    return;

  for (int i=0; i < nr_threads; i++)
    {
      if (source[i])
        fclose(source[i]);
      source[i] = NULL;
    }
  g_free((gpointer)source);
}

static LogFormatType
get_line_format(const char *line)
{
  if (line == NULL)
    return LOG_FORMAT_UNKNOWN;

  char *pri_end = g_strstr_len(line, 5, ">");
  if (!pri_end)
    return LOG_FORMAT_UNKNOWN;

  /* Guess what is the actual logformat. In BSD format
   * priority field <> is followed by timestamp. This starts
   * with upercase letter (Jan Feb Mar ...)
   *
   * In Syslog format the priority field is followed by
   * the version number decimal.
   */
  if (*(pri_end + 1) >= 'A' && *(pri_end + 1) <= 'Z')
    return LOG_FORMAT_RFC3164;
  else if (*(pri_end + 1) >= '0' && *(pri_end + 1) <= '9')
    return LOG_FORMAT_RFC5424;
  else
    return LOG_FORMAT_UNKNOWN;
}

static gsize
safe_copy_n_string(gchar *dest, const gchar *src, int count, int buffsize, const gchar *element_name)
{
  if (!dest || !src)
    {
      ERROR("Invalid src or dest buffer (src=%p, dest=%p)\n", src, dest);
      return 0;
    }

  if (count >= buffsize)
    {
      count = buffsize - 1;
      ERROR("\"%s\" string is too long to fit into buffer. Truncate it to %d chars\n", element_name, count);
    }

  g_strlcpy(dest, src, count + 1);

  return count;
}

static gsize
safe_copy_string(gchar *dest, const gchar *src, int buffsize, const gchar *element_name)
{
  return safe_copy_n_string(dest, src, strlen(src), buffsize, element_name);
}

static void
parse_sdata_and_message(const char *sdata_message, SyslogMsgElements *elements)
{
  /* sdata_message contains both SDATA and message parts. Need to find the
   * SDATA between first "[" and BOM. The rest of the sdata_message contains the
   * message */
  gchar *sdata_start = g_strstr_len(sdata_message, PARS_BUF_SDATA_SIZE, "[");
  gchar *sdata_end   = g_strstr_len(sdata_message, PARS_BUF_SDATA_SIZE, RFC5424_BOM);

  if (!sdata_start || !sdata_end || (sdata_end - sdata_start) == 1)
    {
      DEBUG("no valid sdata found. use \"-\" as sdata\n");
      safe_copy_string(elements->sdata, RFC5424_NIL_VALUE, PARS_BUF_SDATA_SIZE, "sdata");

      gchar *sdata_nil_marker = g_strstr_len(sdata_message, PARS_BUF_SDATA_SIZE, RFC5424_NIL_VALUE);
      if (!sdata_nil_marker)
        safe_copy_string(elements->message, "", PARS_BUF_MSG_SIZE, "msg");
      else
        safe_copy_string(elements->message, sdata_nil_marker + 1, PARS_BUF_MSG_SIZE, "msg");

      g_strchug(elements->message);
    }
  else
    {
      *sdata_end = '\0';
      safe_copy_string(elements->sdata, sdata_start, PARS_BUF_SDATA_SIZE, "sdata");
      g_strchomp(elements->sdata);

      safe_copy_string(elements->message, sdata_end + strlen(RFC5424_BOM), PARS_BUF_MSG_SIZE, "msg");
    }
}

static int
parse_line_rfc5424(const char *line, SyslogMsgElements *elements)
{
  if (!line)
    return -1;

  /* skip the leading '<' char */
  if (g_str_has_prefix(line, "<"))
    line = line + 1;

  gchar **line_split = g_strsplit_set(line, RFC5424_DELIMITERS, RFC5424_HEADER_TOKEN_NUM);

  if (g_strv_length(line_split) < RFC5424_HEADER_TOKEN_NUM)
    {
      ERROR("Invalid rfc5424 log format. Header is too short.\n");
      g_strfreev(line_split);
      return -1;
    }

  safe_copy_string(elements->pri, line_split[RFC5424_PRI_INDEX], PARS_BUF_PRI_SIZE, "priority");
  safe_copy_string(elements->ver, line_split[RFC5424_VER_INDEX], PARS_BUF_VER_SIZE, "version");
  safe_copy_string(elements->time_stamp, line_split[RFC5424_TIMESTAMP_INDEX], PARS_BUF_TIME_STAMP_SIZE, "time stamp");
  safe_copy_string(elements->host, line_split[RFC5424_HOST_NAME_INDEX], PARS_BUF_HOST_SIZE, "host");
  safe_copy_string(elements->app, line_split[RFC5424_APP_NAME_INDEX], PARS_BUF_APP_SIZE, "app");
  safe_copy_string(elements->pid, line_split[RFC5424_PID_INDEX], PARS_BUF_PID_SIZE, "pid");
  safe_copy_string(elements->msgid, line_split[RFC5424_MSGID_INDEX], PARS_BUF_MSG_ID_SIZE, "msgid");

  parse_sdata_and_message(line_split[RFC5424_SDATA_INDEX], elements);

  g_strfreev(line_split);
  return 1;
}

static int
parse_line_rfc3164(const char *line, SyslogMsgElements *elements)
{
  if (!line)
    return -1;

  safe_copy_string(elements->ver, "", PARS_BUF_VER_SIZE, "version"); /* no version info in RFC3164 */
  safe_copy_string(elements->msgid, "", PARS_BUF_MSG_ID_SIZE, "msg_id"); /* no msg_id in RFC3164 */
  safe_copy_string(elements->sdata, "", PARS_BUF_SDATA_SIZE, "sdata"); /* no sdata in RFC3164 */

  /* skip the leading '<' char */
  if (g_str_has_prefix(line, "<"))
    line = line + 1;

  gchar *pri_end = g_strstr_len(line, PARS_BUF_MSG_SIZE, ">");
  if (!pri_end)
    {
      ERROR("Invalid rfc3164 log format. No '>' found.\n");
      return -1;
    }

  /* pri + time stamp */
  safe_copy_n_string(elements->pri, line, pri_end - line, PARS_BUF_PRI_SIZE, "pri");
  safe_copy_n_string(elements->time_stamp, pri_end+1, RFC3164_TIMESTAMP_SIZE, PARS_BUF_TIME_STAMP_SIZE, "timestamp");
  gchar *timestamp_end = pri_end + 1 + RFC3164_TIMESTAMP_SIZE;

  /* host name */
  gchar *host_begin = timestamp_end + 1;
  gchar *host_end = g_strstr_len(host_begin, PARS_BUF_MSG_SIZE, " ");
  if (host_end)
    safe_copy_n_string(elements->host, host_begin, host_end - host_begin, PARS_BUF_HOST_SIZE, "host");
  else
    {
      safe_copy_string(elements->host, host_begin, PARS_BUF_HOST_SIZE, "host");
      safe_copy_string(elements->pid, "", PARS_BUF_PID_SIZE, "pid");
      safe_copy_string(elements->app, "", PARS_BUF_APP_SIZE, "app");
      safe_copy_string(elements->message, "", PARS_BUF_MSG_SIZE, "msg");

      return 1;
    }

  /* application[pid]: */
  gchar *app_begin = host_end + 1;
  gchar *app_end = g_strstr_len(app_begin, PARS_BUF_MSG_SIZE, " ");

  /* netither PID nor application found */
  if ( !g_strstr_len(app_begin, PARS_BUF_APP_SIZE, ":" ) )
    {
      safe_copy_string(elements->pid, "", PARS_BUF_PID_SIZE, "pid");
      safe_copy_string(elements->app, "", PARS_BUF_APP_SIZE, "app");
      safe_copy_string(elements->message, app_begin, PARS_BUF_MSG_SIZE, "msg");
      return 1;
    }

  gchar *pid_begin = g_strstr_len(app_begin, PARS_BUF_APP_SIZE, "[");
  gchar *pid_end   = g_strstr_len(app_begin, PARS_BUF_APP_SIZE, "]");
  if ( !pid_begin || !pid_end )
    {
      safe_copy_string(elements->pid, "", PARS_BUF_PID_SIZE, "pid");
      safe_copy_n_string(elements->app, app_begin, app_end - app_begin - 1, PARS_BUF_APP_SIZE, "app");
    }
  else
    {
      safe_copy_n_string(elements->pid, pid_begin + 1, pid_end - pid_begin - 1, PARS_BUF_PID_SIZE, "pid");
      safe_copy_n_string(elements->app, app_begin, pid_begin - app_begin, PARS_BUF_APP_SIZE, "app");
    }

  safe_copy_string(elements->message, app_end + 1, PARS_BUF_MSG_SIZE, "msg");

  return 1;
}

static const char *
str_skip_tokens(const char *line, int number_of_skips)
{
  while (number_of_skips--)
    {
      line = strchr(line, ' ');

      if (!line)
        return NULL;

      ++line;
    }

  return line;
}

static int
parse_line(const char *line, SyslogMsgElements *elements)
{
  if (!line)
    return -1;

  int ret_val;
  switch (get_line_format(line))
    {
    case LOG_FORMAT_RFC5424:
      ret_val = parse_line_rfc5424(line, elements);
      break;

    case LOG_FORMAT_RFC3164:
      ret_val =  parse_line_rfc3164(line, elements);
      break;

    default:
      DEBUG("unknown logformat detected:%s\n", line);
      ret_val = -1;
    }

  return ret_val;
}

int
read_next_message_from_file(char *buf, int buflen, int syslog_proto, int thread_index)
{
  static int lineno = 0;
  int linelen;

  SyslogMsgElements parsed_elements;
  memset(&parsed_elements, 0, sizeof(SyslogMsgElements));

  if (!source[thread_index])
    return 0;

  while (1)
    {
      if (feof(source[thread_index]))
        {
          if (loop_reading)
            {
              /* Restart reading from the beginning of the file */
              rewind(source[thread_index]);
            }
          else
            return -1;
        }
      char *temp = fgets(buf, buflen, source[thread_index]);
      if (!temp)
        {
          if (loop_reading)
            {
              /* Restart reading from the beginning of the file */
              rewind(source[thread_index]);
              if (!fgets(buf, buflen, source[thread_index]))
                return -1;
            }
          else
            return -1;
        }

      if (dont_parse)
        break;

      if (parse_line(str_skip_tokens(buf, skip_tokens), &parsed_elements) > 0)
        break;

      fprintf(stderr, "Invalid line %d\n", ++lineno);
    }

  if (dont_parse)
    {
      linelen = strnlen(buf, buflen);
      return linelen;
    }

  char stamp[32];
  int tslen;

  if (syslog_proto)
    {
      char tmp[11];
      tslen = get_now_timestamp(stamp, sizeof(stamp));

      linelen = snprintf(buf + 10, buflen - 10, "<%s>%s %.*s %s %s %s %s %s \xEF\xBB\xBF%s",
                         parsed_elements.pri,
                         parsed_elements.ver,
                         tslen, stamp,
                         parsed_elements.host,
                         parsed_elements.app,
                         parsed_elements.pid[0] ? parsed_elements.pid : "-",
                         parsed_elements.msgid[0] ? parsed_elements.msgid : "-",
                         parsed_elements.sdata[0] ? parsed_elements.sdata : "-",
                         parsed_elements.message);
      snprintf(tmp, sizeof(tmp), "%09d ", linelen);
      memcpy(buf, tmp, 10);
      linelen += 10;
    }
  else
    {
      tslen = get_now_timestamp_bsd(stamp, sizeof(stamp));

      if (strlen(parsed_elements.pid))
        linelen = snprintf(buf, buflen, "<38>%.*s %s %s[%s]: %s", tslen, stamp, parsed_elements.host, parsed_elements.app,
                           parsed_elements.pid, parsed_elements.message);
      else
        linelen = snprintf(buf, buflen, "<38>%.*s %s %s: %s", tslen, stamp, parsed_elements.host, parsed_elements.app,
                           parsed_elements.message);
    }

  return linelen;
}
