/*
 * Copyright (c) 2014 Balabit
 * Copyright (c) 2014 Viktor Tusa
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

#ifndef LIBTEST_PERSIST_LIB_H_INCLUDED
#define LIBTEST_PERSIST_LIB_H_INCLUDED

#include "testutils.h"
#include "persist-state.h"

PersistState *create_persist_state_for_test(const gchar *name);
PersistState *clean_and_create_persist_state_for_test(const gchar *name);
void cancel_and_destroy_persist_state(PersistState *state);
void commit_and_destroy_persist_state(PersistState *state);
void commit_and_free_persist_state(PersistState *state);
PersistState *restart_persist_state(PersistState *state);

#endif
