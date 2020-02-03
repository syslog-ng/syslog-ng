/*
 * Copyright (c) 2020 One Identity
 * Copyright (c) 2020 Laszlo Budai <laszlo.budai@outlook.com>
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

#ifndef HTTP_TEST_SLOTS_H_INCLUDED
#define HTTP_TEST_SLOTS_H_INCLUDED

#include "driver.h"

typedef struct _HttpTestSlotsPlugin HttpTestSlotsPlugin;

HttpTestSlotsPlugin *http_test_slots_plugin_new(void);

void http_test_slots_plugin_set_header(HttpTestSlotsPlugin *self, const gchar *header);

#endif
