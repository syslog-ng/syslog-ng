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
static int skip_tokens = 3;
static char *read_file_name = NULL;

static GOptionEntry loggen_file_reader_options[] =
{
  { "read-file", 'R', 0, G_OPTION_ARG_STRING, &read_file_name, "Read log messages from file", "<filename>" },
  { "loop-reading", 'l', 0, G_OPTION_ARG_NONE, &loop_reading, "Read the file specified in read-file option in loop (it will restart the reading if reached the end of the file)", NULL },
  { "skip-tokens", 0, 0, G_OPTION_ARG_INT, &skip_tokens, "Skip the given number of tokens (delimined by a space) at the beginning of each line (default value: 3)", "<number>" },
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

static int
parse_line(const char *line, char *host, char *program, char *pid, char **msg)
{
  const char *pos0;
  const char *pos = line;
  const char *end;
  int space = skip_tokens;
  int pid_len;

  /* Find token */
  while (space)
    {
      pos = strchr(pos, ' ');

      if (!pos)
        return -1;
      pos++;

      space --;
    }

  pos = strchr(pos, ':');
  if (!pos)
    return -1;

  /* pid */
  pos0 = pos;
  if (*(--pos) == ']')
    {
      end = pos - 1;
      while (*(--pos) != '[')
        ;

      pid_len = end -
                pos; /* 'end' points to the last character of the pid string (not off by one), *pos = '[' -> pid length = end - pos*/
      memcpy(pid, pos + 1, pid_len);
      pid[pid_len] = '\0';
    }
  else
    {
      pid[0] = '\0';
      ++pos; /* the 'pos' has been decreased in the condition (']'), reset it to the original position */
    }

  /* Program */
  end = pos;
  while (*(--pos) != ' ')
    ;

  memcpy(program, pos + 1, end - pos - 1);
  program[end-pos-1] = '\0';

  /* Host */
  end = pos;
  while (*(--pos) != ' ')
    ;

  memcpy(host, pos + 1, end - pos - 1);
  host[end-pos-1] = '\0';

  *msg = ((char *)pos0) + 2;

  return 1;
}

int
read_next_message_from_file(char *buf, int buflen, int syslog_proto, int thread_index)
{
  static int lineno = 0;
  int linelen;

  char host[128], program[128], pid[16];
  char *msg = NULL;

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
              temp = fgets(buf, buflen, source[thread_index]);
            }
          else
            return -1;
        }

      if (dont_parse)
        break;

      if (parse_line(buf, host, program, pid, &msg) > 0)
        break;

      fprintf(stderr, "\rInvalid line %d                  \n", ++lineno);
    }

  if (dont_parse)
    {
      linelen = strnlen(buf, buflen);
      return linelen;
    }

  char stamp[32];
  int tslen = get_now_timestamp(stamp, sizeof(stamp));

  if (syslog_proto)
    {
      char tmp[11];
      linelen = snprintf(buf + 10, buflen - 10, "<38>1 %.*s %s %s %s - - \xEF\xBB\xBF%s", tslen, stamp, host, program,
                         (pid[0] ? pid : "-"), msg);
      snprintf(tmp, sizeof(tmp), "%09d ", linelen);
      memcpy(buf, tmp, 10);
      linelen += 10;
    }
  else
    {
      if (*pid)
        linelen = snprintf(buf, buflen, "<38>%.*s %s %s[%s]: %s", tslen, stamp, host, program, pid, msg);
      else
        linelen = snprintf(buf, buflen, "<38>%.*s %s %s: %s", tslen, stamp, host, program, msg);
    }

  return linelen;
}
