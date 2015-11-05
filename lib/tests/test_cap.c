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
#include "gprocess.h"
#include "testutils.h"
#include "privileged-linux.h"

#include <sys/capability.h>

#define CAP_TESTCASE(testfunc, ...) { testcase_begin("%s(%s)", #testfunc, #__VA_ARGS__); testfunc(__VA_ARGS__); testcase_end(); }

#define PERMITTED_CAPS "cap_chown,cap_fowner,cap_dac_override=p "

#define OPEN_RO_FLAGS (O_RDONLY | O_NOCTTY | O_NONBLOCK | O_LARGEFILE)
#define OPEN_WO_FLAGS (O_WRONLY | O_NOCTTY | O_NONBLOCK | O_LARGEFILE)
#define OPEN_RW_FLAGS (O_RDWR | O_NOCTTY | O_NONBLOCK | O_LARGEFILE)

static gint
privileged_function()
{
  return 1;
}

static void
setup_defaults()
{
  cap_t caps;

  caps = cap_from_text(PERMITTED_CAPS);
  cap_set_proc(caps);
  g_process_set_caps(PERMITTED_CAPS);
}

static gboolean
check_string_or(const gchar *actual, const gchar *expected, const gchar *or_expected)
{
  gint actual_len;
  gint expected_len;
  gint or_expected_len;

  if (expected == NULL || or_expected == NULL || actual == NULL)
    return FALSE;

  actual_len = strlen(actual);
  expected_len = strlen(expected);
  or_expected_len = strlen(or_expected);

  if ((actual_len == expected_len &&
       memcmp(actual, expected, actual_len) == 0) ||
     (actual_len == or_expected_len &&
      memcmp(actual, or_expected, actual_len) == 0))
    return TRUE;

  return FALSE;
}

static gboolean
__check_root_rights()
{
  setup_permitted_caps(PERMITTED_CAPS);
  if (cap_compare(cap_get_proc(), cap_from_text(PERMITTED_CAPS)) != 0)
    {
      g_process_message("Please run this test with root user!");
      return FALSE;
    }
  return TRUE;
}

static void
__test_privileged_call_with_a_permitted_cap()
{
  gint result = 0;

  setup_defaults();
  PRIVILEGED_CALL("cap_fowner+e", privileged_function, result);
  assert_gint(result, 1, "Error: privileged_function() function is not called");
  assert_gint(result, 1, "Error raise 'cap_chown,cap_fowner,cap_dac_override=p,cap_fowner+e' caps");
}

static void
__test_privileged_call_with_a_not_permitted_cap()
{
  gint result = 0;
  cap_t caps;

  setup_defaults();
  PRIVILEGED_CALL("cap_sys_admin+e", privileged_function, result);
  assert_gint(result, 1, "Error: privileged_function() function is not called");
  caps = cap_from_text(PERMITTED_CAPS"cap_sys_admin+e");
  assert_gint(cap_compare(cap_get_proc(), caps), 1, "Error raised 'cap_chown,cap_fowner,cap_dac_override=p,cap_sys_admin+e' caps");
}

static void
__test_raise_and_restore_a_permitted_cap()
{
  cap_t caps;
  cap_t saved_caps;
  gint raised_caps_result;
  gint restored_caps_result;

  setup_defaults();
  caps = cap_from_text(PERMITTED_CAPS);
  raised_caps_result = raise_caps("cap_chown+e", &saved_caps);
  assert_gint(cap_compare(saved_caps, caps), 0, "Error: saved caps are wrong");
  assert_gint(raised_caps_result, 0, "Error: not raised 'cap_chown' cap");
  restored_caps_result = restore_caps(saved_caps);
  assert_gint(cap_compare(cap_get_proc(), caps), 0, "Error: restored caps are wrong");
  assert_gint(restored_caps_result, 0, "Error: not restored ''cap_chown,cap_fowner,cap_dac_override=p' caps");
}

static void
__test_get_open_file_caps()
{
  FileOpenOptions open_opts;

  open_opts.needs_privileges = FALSE;
  open_opts.open_flags = OPEN_RO_FLAGS;
  assert_string(get_open_file_caps(&open_opts), OPEN_AS_READ_CAPS, "");

  open_opts.needs_privileges = TRUE;
  open_opts.open_flags = OPEN_RO_FLAGS;
  assert_true(check_string_or(get_open_file_caps(&open_opts), SYSADMIN_CAPS, SYSLOG_CAPS), "");

  open_opts.needs_privileges = FALSE;
  open_opts.open_flags = OPEN_WO_FLAGS;
  assert_string(get_open_file_caps(&open_opts), OPEN_AS_READ_AND_WRITE_CAPS, "");
}

int
main()
{
  g_process_set_name("capability test");
  if (!__check_root_rights())
    return 0;

  CAP_TESTCASE(__test_privileged_call_with_a_permitted_cap);
  CAP_TESTCASE(__test_privileged_call_with_a_not_permitted_cap);
  CAP_TESTCASE(__test_raise_and_restore_a_permitted_cap);
  CAP_TESTCASE(__test_get_open_file_caps);

  return 0;
}
