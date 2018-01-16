/*
 * Copyright (c) 2013 Balabit
 * Copyright (c) 2013 Laszlo Budai
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

#include <criterion/criterion.h>
#include "affile/file-opener.h"
#include "affile/named-pipe.h"
#include "affile/file-specializations.h"
#include "messages.h"

#include <fcntl.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#define CREATE_DIRS 0x01
#define SRC_FILE 0x02
#define DST_FILE 0x04

static mode_t
get_fd_file_mode(gint fd)
{
  struct stat st;

  fstat(fd, &st);
  return st.st_mode;
}

static gboolean
open_fd(FileOpener *file_opener, gchar *fname, gint extra_flags, gint *fd)
{
  FileOpenerOptions open_opts;
  FileDirection dir;

  if (extra_flags & DST_FILE)
    dir = AFFILE_DIR_WRITE;
  else if (extra_flags & SRC_FILE)
    dir = AFFILE_DIR_READ;
  else
    g_assert_not_reached();

  file_opener_options_defaults(&open_opts);
  file_opener_options_init(&open_opts, configuration);
  open_opts.create_dirs = !!(extra_flags & CREATE_DIRS);
  open_opts.needs_privileges = FALSE;

  file_opener_set_options(file_opener, &open_opts);

  gboolean success = file_opener_open_fd(file_opener, fname, dir, fd);
  file_opener_free(file_opener);
  return success;
}

static gboolean
open_regular_source_file(gchar *fname, gint extra_flags, gint *fd)
{
  FileOpener *file_opener = file_opener_for_regular_source_files_new();
  return open_fd(file_opener, fname, extra_flags, fd);
}

static gboolean
open_named_pipe(gchar *fname, gint extra_flags, gint *fd)
{
  FileOpener *file_opener = file_opener_for_named_pipes_new();
  return open_fd(file_opener, fname, extra_flags, fd);
}

Test(file_opener, test_open_regular_file)
{
  gint fd;
  gchar fname[] = "test.log";

  cr_assert(open_regular_source_file(fname, DST_FILE, &fd), "file_opener_open_fd failed: %s", fname);
  cr_assert(S_ISREG(get_fd_file_mode(fd)) != 0, "%s is not regular file", fname);

  close(fd);

  cr_assert(open_regular_source_file(fname, SRC_FILE, &fd), "file_opener_open_fd failed: %s", fname);
  cr_assert(S_ISREG(get_fd_file_mode(fd)) != 0, "%s is not regular file", fname);

  close(fd);

  remove(fname);
}

Test(file_opener, test_open_named_pipe)
{
  gint fd;
  gchar fname[] = "test.pipe";

  cr_assert(open_named_pipe(fname, SRC_FILE, &fd), "failed to open %s", fname);
  cr_assert(S_ISFIFO(get_fd_file_mode(fd)) != 0, "%s is not pipe", fname);

  close(fd);
  remove(fname);
}

Test(file_opener, test_spurious_path)
{
  gint fd;
  gchar fname[] = "./../test.fname";

  cr_assert_not(open_regular_source_file(fname, DST_FILE, &fd), "file_opener_open_fd should not be able to open: %s",
                fname);
}

Test(file_opener, test_create_file_in_nonexistent_dir)
{
  gchar test_dir[] = "nonexistent";
  gchar fname[] = "nonexistent/test.txt";
  gint fd;

  cr_assert_not(open_regular_source_file(fname, DST_FILE, &fd), "file_opener_open_fd failed: %s", fname);
  cr_assert(open_regular_source_file(fname, CREATE_DIRS | DST_FILE, &fd), "file_opener_open_fd failed: %s", fname);

  close(fd);
  remove(fname);
  rmdir(test_dir);
}

Test(file_opener, test_file_flags)
{
  gint fd;
  gchar fname[] = "test_flags.log";

  cr_assert(open_regular_source_file(fname, DST_FILE, &fd), "file_opener_open_fd failed: %s", fname);
  cr_assert((fcntl(fd, F_GETFL) & O_APPEND) == O_APPEND, "invalid open flags");
  close(fd);

  cr_assert(open_regular_source_file(fname, SRC_FILE, &fd), "file_opener_open_fd failed: %s", fname);
  cr_assert((fcntl(fd, F_GETFL) & O_APPEND) != O_APPEND, "invalid open flags");

  close(fd);
  remove(fname);
}

static void
setup(void)
{
  msg_init(FALSE);
  configuration = cfg_new_snippet();
}

void
teardown(void)
{
}

TestSuite(file_opener, .init = setup, .fini = teardown);
