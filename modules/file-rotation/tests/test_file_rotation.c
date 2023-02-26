/*
 * Copyright (c) 2022 Shikhar Vashistha
 * Copyright (c) 2022 László Várady
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

#include "file-rotation.h"
#include "driver.h"

#include <criterion/criterion.h>

Test(file_rotation, test_example)
{
  FileRotationPlugin *fr = file_rotation_new();
  file_rotation_set_size(fr, 100);
  file_rotation_set_interval(fr, "daily");
  file_rotation_set_date_format(fr, "-%Y-%m-%d");
  log_driver_plugin_free((LogDriverPlugin *) fr);

  cr_assert_eq(fr->size, 100);
  cr_assert_str_eq(fr->interval, "daily");
  printf("%s\n", fr->date_format);
  cr_assert_str_eq(fr->date_format, "-%Y-%m-%d");
}
