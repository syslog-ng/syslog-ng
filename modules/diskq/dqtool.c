/*
 * Copyright (c) 2002-2016 Balabit
 * Copyright (c) 2016 Viktor Juhasz <viktor.juhasz@balabit.com>
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
#include "logqueue.h"
#include "template/templates.h"
#include "logmsg/logmsg.h"
#include "messages.h"
#include "logpipe.h"
#include "logqueue-disk.h"
#include "logqueue-disk-reliable.h"
#include "logqueue-disk-non-reliable.h"
#include "logmsg/logmsg-serialize.h"
#include "scratch-buffers.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

gchar *template_string;
gboolean display_version;
gboolean debug_flag;
gboolean verbose_flag;

static GOptionEntry cat_options[] =
{
  {
    "template",  't', 0, G_OPTION_ARG_STRING, &template_string,
    "Template to format the serialized messages", "<template>"
  },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

static GOptionEntry info_options[] =
{
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

static gboolean
open_queue(char *filename, LogQueue **lq, DiskQueueOptions *options)
{
  options->read_only = TRUE;
  options->reliable = FALSE;
  FILE *f = fopen(filename, "rb");
  if (f)
    {
      gchar idbuf[5];
      if (fread(idbuf, 4, 1, f) == 0)
        fprintf(stderr, "File reading error: %s\n", filename);
      fclose(f);
      idbuf[4] = '\0';
      if (!strcmp(idbuf, "SLRQ"))
        options->reliable = TRUE;
    }
  else
    {
      fprintf(stderr, "File not found: %s\n", filename);
      return FALSE;
    }

  if (options->reliable)
    {
      options->disk_buf_size = 128;
      options->mem_buf_size = 1024 * 1024;
      *lq = log_queue_disk_reliable_new(options, NULL);
    }
  else
    {
      options->disk_buf_size = 1;
      options->mem_buf_size = 128;
      options->qout_size = 128;
      *lq = log_queue_disk_non_reliable_new(options, NULL);
    }

  if (!log_queue_disk_load_queue(*lq, filename))
    {
      fprintf(stderr, "Error restoring disk buffer file.\n");
      return FALSE;
    }

  return TRUE;
}

static gint
dqtool_cat(int argc, char *argv[])
{
  GString *msg;
  LogMessage *log_msg = NULL;
  GError *error = NULL;
  LogTemplate *template = NULL;
  DiskQueueOptions options = {0};
  gint i;

  if (template_string)
    {
      template_string = g_strcompress(template_string);
      template = log_template_new(configuration, NULL);
      if (!log_template_compile(template, template_string, &error))
        {
          fprintf(stderr, "Error compiling template: %s, error: %s\n", template->template, error->message);
          g_clear_error(&error);
          return 1;
        }
      g_free(template_string);
    }

  if (!template)
    {
      template = log_template_new(configuration, NULL);
      log_template_compile(template, "$DATE $HOST $MSGHDR$MSG\n", NULL);
    }

  msg = g_string_sized_new(128);
  for (i = optind; i < argc; i++)
    {
      LogPathOptions local_options = LOG_PATH_OPTIONS_INIT;
      LogQueue *lq;

      if (!open_queue(argv[i], &lq, &options))
        continue;

      log_queue_set_use_backlog(lq, TRUE);
      log_queue_rewind_backlog_all(lq);

      while ((log_msg = log_queue_pop_head(lq, &local_options)) != NULL)
        {
          /* format log */
          log_template_format(template, log_msg, &configuration->template_options, LTZ_LOCAL, 0, NULL, msg);
          log_msg_unref(log_msg);
          log_msg = NULL;

          printf("%s", msg->str);
        }

      log_queue_unref(lq);
    }
  g_string_free(msg, TRUE);
  return 0;

}

static gint
dqtool_info(int argc, char *argv[])
{
  gint i;
  for (i = optind; i < argc; i++)
    {
      LogQueue *lq;
      DiskQueueOptions options = {0};

      if (!open_queue(argv[i], &lq, &options))
        continue;
      log_queue_unref(lq);
    }
  return 0;
}

static GOptionEntry dqtool_options[] =
{
  {
    "debug",     'd', 0, G_OPTION_ARG_NONE, &debug_flag,
    "Enable debug/diagnostic messages on stderr", NULL
  },
  {
    "verbose",   'v', 0, G_OPTION_ARG_NONE, &verbose_flag,
    "Enable verbose messages on stderr", NULL
  },
  {
    "version",   'V', 0, G_OPTION_ARG_NONE, &display_version,
    "Display version number (" SYSLOG_NG_VERSION ")", NULL
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
  { "cat", cat_options, "Print the contents of a disk queue file", dqtool_cat },
  { "info", info_options, "Print infos about the given disk queue file", dqtool_info },
  { NULL, NULL },
};

static const gchar *
dqtool_mode(int *argc, char **argv[])
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

  fprintf(stderr, "Syntax: dqtool <command> [options]\nPossible commands are:\n");
  for (mode = 0; modes[mode].mode; mode++)
    {
      fprintf(stderr, "    %-12s %s\n", modes[mode].mode, modes[mode].description);
    }
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

  mode_string = dqtool_mode(&argc, &argv);
  if (!mode_string)
    {
      usage();
      return 1;
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
          g_option_context_add_main_entries(ctx, modes[mode].options, NULL);
          g_option_context_add_main_entries(ctx, dqtool_options, NULL);
          break;
        }
    }

  if (!ctx)
    {
      fprintf(stderr, "Unknown command\n");
      usage();
      return 1;
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
