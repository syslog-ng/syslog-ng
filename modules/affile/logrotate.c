/*
 * Copyright (c) 2024 Balabit
 * Copyright (c) 2024 Alexander Zikulnig
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

#include "logrotate.h"
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include "messages.h"

#define LR_DEFAULT_SIZE 10000000 // 10MB
#define LR_DEFAULT_ROTATIONS 1
#define LR_DEFAULT_DATE_FORMAT "-%Y-%m-%d"

void logrotate_options_defaults(LogRotateOptions *logrotate_options)
{
  logrotate_options->enable=FALSE;
  logrotate_options->size= LR_DEFAULT_SIZE;
  logrotate_options->max_rotations = LR_DEFAULT_ROTATIONS;
  logrotate_options->interval = NONE;
  logrotate_options->date_format = LR_DEFAULT_DATE_FORMAT;
}

gboolean is_logrotate_enabled(LogRotateOptions *logrotate_options)
{
  if (logrotate_options == NULL) return FALSE;
  else return logrotate_options->enable;
}

gboolean is_logrotate_pending(LogRotateOptions *logrotate_options, const gchar *filename)
{
  if (logrotate_options == NULL || filename == NULL) return FALSE;

  struct stat st;
  int res = stat(filename, &st);
  if (res == -1)
    {
      msg_error("LOGROTATE: Error reading file stats",
                evt_tag_str("filename", filename),
                evt_tag_errno("errno", errno));
      return LR_ERROR;
    }
  gsize size = st.st_size;

  return (size >= logrotate_options->size);
}

/* TODO:
 * Do renaming in separate thread?
 * i.e. in writer thread delete oldest log file and create a new temp logfile --> filename_tmp
 * the writer only does this if not already a tmp file is created (otherwise the renaming thread is working)
 * checked is this condition via the gcond-lock waiting for the thread to finish.
 *
 * The renaming thread renames all other files including the tmp file to filename.1
 *
 */
LogRotateStatus do_logrotate(LogRotateOptions *logrotate_options, const gchar *filename)
{
  if (logrotate_options == NULL || filename == NULL) return LR_ERROR;

  g_assert (logrotate_options->max_rotations > 0);

  msg_info("LOGROTATE: Maximum log-file size reached, rotating log file ... ",
           evt_tag_str("filename", filename));

  int res;
  gchar *current_filename;
  gchar *rotated_filename;

  // (1) check if max_rotations is already reached, if so delete oldest file
  // assuming 'filename' is the prefix of all logfiles, whereas the orginal log is named after the prefix
  current_filename = g_strdup_printf("%s.%ld", filename, logrotate_options->max_rotations);
  if (g_file_test(current_filename, G_FILE_TEST_EXISTS))
    {
      msg_debug("LOGROTATE: Deleting oldest log-file",
                evt_tag_str("filename", current_filename));
      res = remove(current_filename);

      if (res == -1) goto logrotate_error_free_current_filename;
    }
  g_free(current_filename);


  // (2) rename existing rotated files, shift file name by postfix
  for (gsize i = logrotate_options->max_rotations-1; i > 0; i--)
    {
      current_filename = g_strdup_printf("%s.%ld", filename, i);
      if (g_file_test(current_filename, G_FILE_TEST_EXISTS))
        {
          rotated_filename = g_strdup_printf("%s.%ld", filename, i+1);
          msg_debug("LOGROTATE: Rotating log-file",
                    evt_tag_str("filename", current_filename),
                    evt_tag_str("new_filename", rotated_filename));
          res = rename(current_filename, rotated_filename);

          if (res == -1) goto logrotate_error_free_rotated_filename;

          g_free(rotated_filename);
        }
      g_free(current_filename);
    }

  // (3) rename current logfile
  if (g_file_test(filename, G_FILE_TEST_EXISTS))
    {
      current_filename = g_strdup(filename);
      rotated_filename = g_strdup_printf("%s.%d", filename, 1);
      msg_debug("LOGROTATE: Rotating ACTIVE log-file",
                evt_tag_str("filename", current_filename),
                evt_tag_str("new_filename", rotated_filename));
      res = rename(current_filename, rotated_filename);

      if (res == -1) goto logrotate_error_free_rotated_filename;

      g_free(rotated_filename);
      g_free(current_filename);
    }


  // (4) report the log file to reopened
  return LR_SUCCESS;

  // error handling
logrotate_error_free_rotated_filename:
  g_free(rotated_filename);
logrotate_error_free_current_filename:
  msg_error("LOGROTATE: Error renaming or deleting log file",
            evt_tag_str("filename", current_filename),
            evt_tag_errno("errno", errno));
  g_free(current_filename);

  return LR_ERROR;
}
