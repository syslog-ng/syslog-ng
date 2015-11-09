/*
 * Copyright (c) 2002-2015 BalaBit IT Ltd, Budapest, Hungary
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
#include "apphook.h"
#include "gprocess.h"
#include "testutils.h"
#include "file-perms.h"
#include "cfg.h"
#include "mainloop.h"
#include "capability_lib.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include "modules/affile/affile-common.h"
#include "modules/afsocket/afunix-source.h"

#define CAP_TESTCASE(testfunc, ...) { testcase_begin("%s(%s)", #testfunc, #__VA_ARGS__); testfunc(__VA_ARGS__); testcase_end(); }

#define CREATE_DIRS 0x01
#define PIPE 0x02

#define PIPE_OPEN_FLAGS (O_RDWR | O_NOCTTY | O_NONBLOCK | O_LARGEFILE)
#define REGULAR_FILE_OPEN_FLAGS (O_CREAT | O_NOCTTY | O_LARGEFILE)


static gboolean
_open_file(gchar *fname, int open_flags, gint extra_flags, gint *fd)
{
  FilePermOptions perm_opts;
  FileOpenOptions open_opts;

  file_perm_options_defaults(&perm_opts);

  open_opts.open_flags = open_flags;
  open_opts.create_dirs = !!(extra_flags & CREATE_DIRS);
  open_opts.is_pipe = !!(extra_flags & PIPE);
  open_opts.needs_privileges = FALSE;

  return affile_open_file(fname, &open_opts, &perm_opts, fd);
}

static void
afunix_source_file_permission()
{
  AFUnixSourceDriver *self = g_new0(AFUnixSourceDriver, 1);

  cap_counter_reset();

  file_perm_options_defaults(&self->file_perm_options);

  afunix_sd_apply_perms_to_socket(self);

  assert_false(is_number_of_cap_modify_zero(), "Caps were not modified");
  assert_true(check_cap_count(CAP_CHOWN, 1, 1), "Cap not raised: CAP_CHOWN");
  assert_true(check_cap_count(CAP_FOWNER, 1, 1), "Cap not raised: CAP_FOWNER");
  assert_true(check_cap_count(CAP_DAC_OVERRIDE, 1, 1), "Cap not raised: CAP_DAC_OVERRIDE");
  assert_true(check_cap_save(1), "Caps were not saved");
  assert_true(check_cap_restore(1), "Caps were not restored");

  g_free(self);
}

static void
affile_common_file_permission_open_file()
{
  gint fd;
  gchar fname[] = "test";

  cap_counter_reset();

  _open_file(fname, REGULAR_FILE_OPEN_FLAGS, 0, &fd);
  close(fd);
  remove(fname);

  assert_false(is_number_of_cap_modify_zero(), "Caps were not modified");
  assert_true(check_cap_count(CAP_CHOWN, 1, 1), "Cap not raised: CAP_CHOWN");
  assert_true(check_cap_count(CAP_FOWNER, 1, 1), "Cap not raised: CAP_FOWNER");
  assert_true(check_cap_count(CAP_DAC_OVERRIDE, 1, 1), "Cap not raised: CAP_DAC_OVERRIDE");
  assert_true(check_cap_save(1), "Caps were not saved");
  assert_true(check_cap_restore(1), "Caps were not restored");

}

static void
affile_common_file_permission_open_pipe()
{
  gint fd;
  gchar fname[] = "test";

  cap_counter_reset();

  _open_file(fname, PIPE_OPEN_FLAGS, PIPE, &fd);
  close(fd);
  remove(fname);

  assert_false(is_number_of_cap_modify_zero(), "Caps were not modified");
  assert_true(check_cap_count(CAP_CHOWN, 1, 1), "Cap not raised: CAP_CHOWN");
  assert_true(check_cap_count(CAP_FOWNER, 1, 1), "Cap not raised: CAP_FOWNER");
  assert_true(check_cap_count(CAP_DAC_OVERRIDE, 1, 1), "Cap not raised: CAP_DAC_OVERRIDE");
  assert_true(check_cap_save(1), "Caps were not saved");
  assert_true(check_cap_restore(1), "Caps were not restored");
}

static void
affile_common_file_permission_open_file_with_create_dir()
{
  gint fd;
  gchar dir[] = "nonexistent";
  gchar dir_fname[] = "nonexistent/test.txt";

  cap_counter_reset();

  _open_file(dir_fname, REGULAR_FILE_OPEN_FLAGS, CREATE_DIRS, &fd);
  close(fd);
  remove(dir_fname);
  rmdir(dir);

  assert_false(is_number_of_cap_modify_zero(), "Caps were not modified");
  assert_true(check_cap_count(CAP_CHOWN, 1, 2), "Cap not raised: CAP_CHOWN by 2");
  assert_true(check_cap_count(CAP_FOWNER, 1, 2), "Cap not raised: CAP_FOWNER by 2");
  assert_true(check_cap_count(CAP_DAC_OVERRIDE, 1, 1), "Cap not raised: CAP_DAC_OVERRIDE");
  assert_true(check_cap_save(2), "Caps were not saved by 2");
  assert_true(check_cap_restore(2), "Caps were not restored by 2");
}

static void
affile_common_file_permission_create_write_only_file()
{
  gint fd;
  gchar fname[] = "test";

  cap_counter_reset();

  _open_file(fname, O_CREAT|O_WRONLY, 0, &fd);
  close(fd);
  remove(fname);

  assert_false(is_number_of_cap_modify_zero(), "Caps were not modified");
  assert_true(check_cap_count(CAP_CHOWN, 1, 1), "Cap not raised: CAP_CHOWN");
  assert_true(check_cap_count(CAP_FOWNER, 1, 1), "Cap not raised: CAP_FOWNER");
  assert_true(check_cap_count(CAP_DAC_OVERRIDE, 1, 1), "Cap not raised: CAP_DAC_OVERRIDE");
  assert_true(check_cap_save(1), "Caps were not saved");
  assert_true(check_cap_restore(1), "Caps were not restored");
}

static void
affile_common_file_permission_open_nonexist_file()
{
  gint fd;
  gchar nonexist_fname[] = "./../test.fname";

  cap_counter_reset();

  _open_file(nonexist_fname, REGULAR_FILE_OPEN_FLAGS, 0, &fd);

  assert_true(is_number_of_cap_modify_zero(), "Caps were modified");
  assert_true(check_cap_save(0), "Caps were saved");
  assert_true(check_cap_restore(0), "Caps were restored");
}

int
main()
{
  app_startup();
  capability_mocking_reinit();

  CAP_TESTCASE(affile_common_file_permission_open_file_with_create_dir);
  CAP_TESTCASE(afunix_source_file_permission);
  CAP_TESTCASE(affile_common_file_permission_open_file);
  CAP_TESTCASE(affile_common_file_permission_open_pipe);
  CAP_TESTCASE(affile_common_file_permission_create_write_only_file);
  CAP_TESTCASE(affile_common_file_permission_open_nonexist_file);

  app_shutdown();
  return 0;
}
