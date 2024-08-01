/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2024 OneIdentity
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

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

#include "cfg-helpers.h"


Test(test_cfg_helpers, test_port)
{
  cr_assert(cfg_check_port("1"));
  cr_assert(cfg_check_port("65535"));

  cr_assert_not(cfg_check_port("0"));
  cr_assert_not(cfg_check_port("-1"));
  cr_assert_not(cfg_check_port("65536"));
  cr_assert_not(cfg_check_port("1234567890"));
}
