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
#include "mainloop.h"
#include "pathutils.h"
#include "persist-state.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

gchar *template_string;
gchar *new_diskq_path;
gchar *persist_file_path;
gchar *assign_persist_name;
gboolean relocate_all;
gboolean display_version;
gboolean debug_flag;
gboolean verbose_flag;
gboolean assign_help;

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

static GOptionEntry relocate_options[] =
{
  {
    "new_path", 'n', 0, G_OPTION_ARG_STRING, &new_diskq_path,
    "New path(directory) for diskq file(s)", "<new_path>"
  },
  {
    "persist", 'p', 0, G_OPTION_ARG_STRING, &persist_file_path,
    "syslog-ng persist file", "<persist>"
  },
  {
    "all", 'a', 0, G_OPTION_ARG_NONE, &relocate_all,
    "relocate all persist file"
  },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

static GOptionEntry assign_options[] =
{
  {
    "persist", 'p', 0, G_OPTION_ARG_STRING, &persist_file_path,
    "syslog-ng persist file", "<persist>"
  },
  {
    "persist_name", 'n', 0, G_OPTION_ARG_STRING, &assign_persist_name,
    "persist name", "<persist name>"
  },
  {
    "example", 'e', 0, G_OPTION_ARG_NONE, &assign_help,
    "print examples"
  },
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

static gboolean
_is_read_writable(const gchar *path)
{
  return access(path, R_OK|W_OK) == F_OK;
}

static gboolean
_validate_diskq_path(const gchar *path)
{
  if (!path)
    {
      fprintf(stderr, "missing mandatory option: new_path\n");
      return FALSE;
    }

  if (!(is_file_directory(path) && _is_read_writable(path)))
    {
      fprintf(stderr, "new_path should be point to a readable/writable directory\n");
      return FALSE;
    }

  return TRUE;
}

static gboolean
_validate_persist_file_path(const gchar *path)
{
  if (!path)
    {
      fprintf(stderr, "missing mandatory option: persist\n");
      return FALSE;
    }

  if (!(is_file_regular(path) && _is_read_writable(path)))
    {
      fprintf(stderr, "persist should point to a readable/writable file\n");
      return FALSE;
    }

  return TRUE;
}

static gboolean
_relocate_validate_options(void)
{
  return _validate_diskq_path(new_diskq_path) && _validate_persist_file_path(persist_file_path);
}

static gint
_open_fd_for_reading(const gchar *fname)
{
  gint fd = open(fname, O_RDONLY);

  if (fd == -1)
    fprintf(stderr, "Failed to open %s, reason: %s\n", fname, strerror(errno));

  return fd;
}

static gint
_open_fd_for_writing(const gchar *fname)
{
  gint fd = open(fname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

  if (fd == -1)
    fprintf(stderr, "Error: failed to create %s, reason: %s\n", fname, strerror(errno));

  return fd;
}

static void
_remove_file(const gchar *fname)
{
  if (remove(fname) == -1)
    fprintf(stderr, "Failed to remove %s, reason; %s\n", fname, strerror(errno));
}

static gboolean
_copy_file(const gchar *from, const gchar *to)
{
  gboolean result = FALSE;
  gint src_fd = _open_fd_for_reading(from);
  gint dst_fd = -1;

  if (src_fd == -1)
    goto exit;

  dst_fd = _open_fd_for_writing(to);

  if (dst_fd == -1)
    goto exit;


  char buf[2048] = {0};
  gssize read_n = 0;
  while ((read_n = read(src_fd, buf, sizeof(buf))) > 0)
    {
      if (write(dst_fd, buf, read_n) != read_n)
        {
          fprintf(stderr, "Failed to write file %s\nReason:%s\n", to, strerror(errno));
          goto exit;
        }
    }
  if (read_n == -1)
    {
      fprintf(stderr, "Failed to read file %s\nReason:%s\n", from, strerror(errno));
      goto exit;
    }

  result = TRUE;

exit:
  if (src_fd != -1)
    close(src_fd);

  if (dst_fd != -1)
    close(dst_fd);

  return result;
}

static gboolean
_move_file_between_different_filesystems(const gchar *from, const gchar *to)
{
  if (!_copy_file(from, to))
    return FALSE;

  _remove_file(from);

  return TRUE;
}

static gboolean
_move_file(const gchar *from, const gchar *to)
{
  gint rename_res = rename(from, to);

  if (rename_res == 0)
    return TRUE;

  switch (errno)
    {
    case 0:
      return TRUE;
    case EXDEV:
      return _move_file_between_different_filesystems(from, to);
    default:
      fprintf(stderr, "Failed moving %s to %s, reason: %s\n", from, to, strerror(errno));
      return FALSE;
    }
}

static gboolean
_file_is_diskq(const gchar *filename)
{
  const gchar *filename_ext = get_filename_extension(filename);
  if (!filename_ext)
    return FALSE;

  gboolean reliable = strcmp(filename_ext, "rqf") == 0;
  if (!reliable && strcmp(filename_ext, "qf") != 0)
    return FALSE;

  FILE *f = fopen(filename, "rb");

  if (!f)
    {
      fprintf(stderr, "File not found: %s\n", filename);
      return FALSE;
    }

  gchar idbuf[5];
  if (fread(idbuf, 1, 4, f) != 4)
    {
      fprintf(stderr, "File reading error(cannot read 4 bytes from the file): %s\n", filename);
      fclose(f);
      return FALSE;
    }

  fclose(f);

  idbuf[4] = '\0';
  if (reliable && strcmp(idbuf, "SLRQ") != 0)
    return FALSE;
  if (!reliable && strcmp(idbuf, "SLQF") != 0)
    return FALSE;

  return TRUE;
}


static gboolean
_is_persist_entry_holds_diskq_file(PersistState *state, const gchar *key)
{
  gboolean result = FALSE;
  gchar *value = persist_state_lookup_string(state, key, NULL, NULL);
  if (!value)
    return FALSE;

  if (!is_file_regular(value))
    goto exit;

  result = _file_is_diskq(value);
exit:
  g_free(value);

  return result;
}

static void
_relocate_qfile(PersistState *state, const gchar *name)
{
  if (_is_persist_entry_holds_diskq_file(state, name))
    {
      gchar *qfile = persist_state_lookup_string(state, name, NULL, NULL);
      printf("found qfile, key: %s, path: %s\n", name, qfile);
      gchar *relocated_qfile = g_build_filename(new_diskq_path, basename(qfile), NULL);
      if (!relocated_qfile)
        {
          fprintf(stderr, "Invalid path. new_diskq_dir: %s, qfile: %s\n", new_diskq_path, qfile);
          g_free(qfile);
        }

      if (_move_file(qfile, relocated_qfile))
        {
          printf("new qfile_path: %s\n", relocated_qfile);
          persist_state_alloc_string(state, name, relocated_qfile, -1);
        }
      else
        {
          fprintf(stderr, "Failed to move file to new qfile_path: %s\n", relocated_qfile);
        }
      g_free(qfile);
      g_free(relocated_qfile);
    }
}

static void
_persist_foreach_relocate_all_qfiles(gchar *name, gint size, gpointer entry, gpointer userdata)
{
  PersistState *state = (PersistState *)userdata;
  _relocate_qfile(state, name);
}

static void
_persist_foreach_relocate_selected_qfiles(gchar *name, gint size, gpointer entry, gpointer userdata)
{
  gpointer *args = (gpointer *)userdata;
  PersistState *state = (PersistState *)args[0];
  gint argc = GPOINTER_TO_INT(args[1]);
  const gchar **argv = (const gchar **)args[2];
  gint argc_start = GPOINTER_TO_INT(args[3]);
  gchar *qfile = persist_state_lookup_string(state, name, NULL, NULL);

  for (gint i = argc_start; i < argc; i++)
    {
      if (!strcmp(qfile, argv[i]))
        {
          _relocate_qfile(state, name);
          break;
        }
    }

  g_free(qfile);
}

static gint
dqtool_relocate(int argc, char *argv[])
{
  if (!_relocate_validate_options())
    return 1;

  if (!g_threads_got_initialized)
    {
      g_thread_init(NULL);
    }

  main_thread_handle = get_thread_id();

  PersistState *state = persist_state_new(persist_file_path);
  if (!state)
    {
      fprintf(stderr, "Failed to create PersistState from file %s\n", persist_file_path);
      return 1;
    }

  if (!persist_state_start_edit(state))
    {
      fprintf(stderr, "Failed to load persist file for editing.");
      return 1;
    }

  if (relocate_all)
    {
      persist_state_foreach_entry(state, _persist_foreach_relocate_all_qfiles, state);
    }
  else
    {
      gpointer args[] = { state, GINT_TO_POINTER(argc), argv, GINT_TO_POINTER(optind) };
      persist_state_foreach_entry(state, _persist_foreach_relocate_selected_qfiles, args);
    }

  persist_state_commit(state);
  persist_state_free(state);

  return 0;
}

static gboolean
_assign_validate_options(void)
{
  return _validate_persist_file_path(persist_file_path) && _file_is_diskq(persist_file_path);
}

static void
_assign_print_help(void)
{
  fprintf(stderr, "example:"
          "   bin/dqtool assign -p var/syslog-ng.persist -n \"afsocket_dd_qfile(stream,localhost:15554)\n"
          "                    /tmp/syslog-ng-dq/syslog-ng-00000.rqf\n\n"
          "When only a filename is given for diskq, it will be appended to the current working dir.\n"
          "One bad thing: user need to figure out the correct persist name.\n\n");
  fprintf(stderr, "How it works?\n"
          "When you know what the persist name for the diskq file is and you want to assign"
          " an existing queue file to your destination, then with this feature you can set the queue file"
          " manually (even if you don't have an entry for the diskq file in the persist).\n");
}

static gint
dqtool_assign(int argc, char *argv[])
{
  if (assign_help)
    {
      _assign_print_help();
      return 0;
    }

  if (!_assign_validate_options())
    return 1;

  if (!g_threads_got_initialized)
    {
      g_thread_init(NULL);
    }

  main_thread_handle = get_thread_id();

  PersistState *state = persist_state_new(persist_file_path);
  if (!state)
    {
      fprintf(stderr, "Failed to create PersistState from file %s\n", persist_file_path);
      return 1;
    }

  if (!persist_state_start_edit(state))
    {
      fprintf(stderr, "Failed to load persist file for editing.");
      return 1;
    }

  const gchar *diskq_file = argv[optind];
  gchar *diskq_full_path = g_canonicalize_filename(diskq_file, NULL);

  gchar *old_entry = persist_state_lookup_string(state, assign_persist_name, NULL, NULL);
  if (old_entry)
    {
      fprintf(stderr, "Entry overridden during the assign process. Old entry: %s\n", old_entry);
      g_free(old_entry);
    }

  persist_state_alloc_string(state, assign_persist_name, diskq_full_path, -1);
  g_free(diskq_full_path);

  persist_state_commit(state);
  persist_state_free(state);

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
  { "relocate", relocate_options, "Relocate(rename) diskq file. Note that this option modifies the persist file.", dqtool_relocate },
  { "assign", assign_options, "Assign diskq file to the given persist file with the given persist name.", dqtool_assign },
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

  main_loop_thread_resource_init();
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
  main_loop_thread_resource_deinit();
  return 0;

}
