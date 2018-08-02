/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2018 Mehul Prajapati <mehulprajapati2802@gmail.com>
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

#include "syslog-ng.h"
#include "template/templates.h"
#include "logmsg/logmsg.h"
#include "messages.h"
#include "logpipe.h"
#include "logmsg/logmsg-serialize.h"
#include "scratch-buffers.h"

#include <hiredis/hiredis.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 6379

gboolean display_version;
gboolean deserialize_msg;
gchar *server_ip;
gint port_no = 0;

static struct timeval timeout = {1, 0};

static GOptionEntry cat_options[] =
{
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

static GOptionEntry list_options[] =
{
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

static GOptionEntry info_options[] =
{
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

static redisReply *
get_redis_reply(redisContext *rc, const char *format, va_list ap)
{
  redisReply *reply = redisvCommand(rc, format, ap);

  gboolean retval = reply && (reply->type != REDIS_REPLY_ERROR);

  if (!retval)
    return NULL;

  return reply;
}

static void print_redis_lists(redisReply *reply)
{
  gint idx = 0;

  if (reply->type == REDIS_REPLY_ARRAY && reply->elements > 0)
    {
      for (idx = 0; idx < reply->elements; idx++)
        printf("%u) %s\n", idx, reply->element[idx]->str);
    }
}

static void
print_redis_list_data(redisReply *reply)
{
  gint idx = 0;
  GString *serialized;
  SerializeArchive *sa;
  GString *msg;
  LogMessage *log_msg = NULL;
  LogTemplate *template = NULL;

  template = log_template_new(configuration, NULL);
  log_template_compile(template, "$DATE $HOST $MSGHDR$MSG\n", NULL);
  msg = g_string_sized_new(128);

  if (reply->type == REDIS_REPLY_ARRAY && reply->elements > 0)
    {
      for (idx = 0; idx < reply->elements; idx++)
        {
          if (deserialize_msg)
            {
              serialized = g_string_new_len(reply->element[idx]->str, reply->element[idx]->len);
              g_string_set_size(serialized, reply->element[idx]->len);
              sa = serialize_string_archive_new(serialized);

              log_msg = log_msg_new_empty();

              if (!log_msg_deserialize(log_msg, sa))
                printf("rqtool: Failed to deserialize a message");

              serialize_archive_free(sa);
              g_string_free(serialized, TRUE);

              /* format log */
              log_template_format(template, log_msg, &configuration->template_options, LTZ_LOCAL, 0, NULL, msg);
              log_msg_unref(log_msg);
              log_msg = NULL;

              printf("%s", msg->str);
            }
          else
            printf("%u) %s\n", idx, reply->element[idx]->str);
        }
    }
  g_string_free(msg, TRUE);
}

static redisReply *
send_redis_command(const char *format, ...)
{
  redisContext *rc;
  redisReply *reply = NULL;

  if (!server_ip)
    server_ip = g_strdup(SERVER_IP);

  if (!port_no)
    port_no = SERVER_PORT;

  /* Connect to redis server */
  rc = redisConnectWithTimeout(server_ip, port_no, timeout);

  if (rc == NULL || rc->err)
    {
      if (rc)
        printf("rqtool: redis connection error: %s\n", rc->errstr);
      else
        printf("rqtool: redis connection error: can't allocate redis context\n");
    }
  else
    {
      va_list ap;
      va_start(ap, format);
      reply = get_redis_reply(rc, format, ap);
      va_end(ap);
    }

  if (rc)
    redisFree(rc);

  g_free(server_ip);
  server_ip = NULL;
  port_no = 0;

  return reply;
}

static gint
rqtool_cat(int argc, char *argv[])
{
  gint idx;
  redisReply *reply = NULL;

  for (idx = optind; idx < argc; idx++)
    {
      reply = send_redis_command("LRANGE %s 0 -1", argv[idx]);

      if (reply)
        {
          print_redis_list_data(reply);
          freeReplyObject(reply);
        }
    }

  return 0;
}

static gint
rqtool_info(int argc, char *argv[])
{
  gint idx;
  redisReply *reply = NULL;

  for (idx = optind; idx < argc; idx++)
    {
      reply = send_redis_command("LLEN %s", argv[idx]);

      if (reply)
        {
          if (reply->type == REDIS_REPLY_INTEGER)
            printf("listname = '%s', qlen = '%lld'\n", argv[idx], reply->integer);

          freeReplyObject(reply);
        }
    }

  return 0;
}

static gint
rqtool_list(int argc, char *argv[])
{
  redisReply *reply = NULL;

  reply = send_redis_command("KEYS *");

  if (reply)
    {
      print_redis_lists(reply);
      freeReplyObject(reply);
    }

  return 0;
}

static GOptionEntry rqtool_options[] =
{
  {
    "ipaddr",     's', 0, G_OPTION_ARG_STRING, &server_ip,
    "Redis server ip address", NULL
  },
  {
    "port",   'p', 0, G_OPTION_ARG_INT, &port_no,
    "Redis server port num", NULL
  },
  {
    "version",   'V', 0, G_OPTION_ARG_NONE, &display_version,
    "Display version number (" SYSLOG_NG_VERSION ")", NULL
  },
  {
    "deserialize",   'd', 0, G_OPTION_ARG_NONE, &deserialize_msg,
    "Deserialize log message", NULL
  },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

static struct
{
  const gchar *mode;
  const GOptionEntry *options;
  const gchar *description;
  gint (*main)(gint argc, gchar *argv[]);
} modes[] =
{
  { "cat", cat_options, "Print the contents of a redis list", rqtool_cat },
  { "list", list_options, "Print all available redis lists", rqtool_list },
  { "info", info_options, "Print infos about the given redis list", rqtool_info },
  { NULL, NULL },
};

static const gchar *
rqtool_mode(int *argc, char **argv[])
{
  gint i;
  const gchar *mode;

  for (i = 1; i < (*argc); i++)
    {
      if ((*argv)[i][0] != '-')
        {
          mode = (*argv)[i];
          memmove(&(*argv)[i], &(*argv)[i+1], ((*argc) - i) * sizeof(gchar *));
          (*argc)--;
          return mode;
        }
    }
  return NULL;
}

void
usage(void)
{
  gint mode;

  fprintf(stderr, "Syntax: rqtool <command> [list name] [-s <server ip> -p <port>] [-d]\nPossible commands are:\n");
  for (mode = 0; modes[mode].mode; mode++)
    {
      fprintf(stderr, "    %-12s %s\n", modes[mode].mode, modes[mode].description);
    }
  exit(1);
}

void
version(void)
{
  printf(SYSLOG_NG_VERSION "\n");
}

int
main(int argc, char *argv[])
{
  const gchar *mode_string;
  GOptionContext *ctx;
  gint mode;
  GError *error = NULL;

  mode_string = rqtool_mode(&argc, &argv);
  if (!mode_string)
    {
      usage();
    }

  ctx = NULL;
  for (mode = 0; modes[mode].mode; mode++)
    {
      if (strcmp(modes[mode].mode, mode_string) == 0)
        {
          ctx = g_option_context_new(mode_string);
#if GLIB_CHECK_VERSION (2, 12, 0)
          g_option_context_set_summary(ctx, modes[mode].description);
#endif
          g_option_context_add_main_entries(ctx, rqtool_options, NULL);
          g_option_context_add_main_entries(ctx, modes[mode].options, NULL);
          break;
        }
    }

  if (!ctx)
    {
      fprintf(stderr, "Unknown command\n");
      usage();
    }

  if (!g_option_context_parse(ctx, &argc, &argv, &error))
    {
      fprintf(stderr, "Error parsing command line arguments: %s\n", error ? error->message : "Invalid arguments");
      g_clear_error(&error);
      g_option_context_free(ctx);
      return 1;
    }
  g_option_context_free(ctx);
  if (display_version)
    {
      version();
      return 0;
    }

  configuration = cfg_new_snippet();

  configuration->template_options.frac_digits = 3;
  configuration->template_options.time_zone_info[LTZ_LOCAL] = time_zone_info_new(NULL);

  msg_init(TRUE);
  stats_init();
  scratch_buffers_global_init();
  scratch_buffers_allocator_init();
  log_template_global_init();
  log_msg_registry_init();
  log_tags_global_init();
  modes[mode].main(argc, argv);
  log_tags_global_deinit();
  scratch_buffers_allocator_deinit();
  scratch_buffers_global_deinit();
  stats_destroy();
  msg_deinit();

  return 0;
}
