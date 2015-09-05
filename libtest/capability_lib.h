/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
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

#ifndef CAPABILITY_LIB_H_INCLUDED
#define CAPABILITY_LIB_H_INCLUDED

#include <sys/capability.h>

typedef struct _CapModify
{
  int onoff;
  int capability;
} CapModify;

void capability_mocking_reinit();

void cap_counter_reset();
guint is_number_of_cap_modify_zero();
gboolean check_cap_count(int capability, int onoff, int count);
gboolean check_cap_save(int number);
gboolean check_cap_restore(int number);

#endif
