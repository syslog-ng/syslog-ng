/*
 * Copyright (c) 2016 Balabit
 * Copyright (c) 2014 Laszlo Budai
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

#include "syslog-ng.h"
#include "host-id.h"
#include "logmsg/logmsg.h"
#include "apphook.h"
#include "cfg.h"
#include "mainloop.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static GlobalConfig *
_create_cfg(void)
{
  GlobalConfig *cfg = cfg_new_snippet();
  return cfg;
}

static PersistState *
_create_persist_state(const gchar *persist_file)
{
  PersistState *state = persist_state_new(persist_file);

  cr_assert(persist_state_start(state),
            "Error starting persist state object [%s]", persist_file);

  return state;
}

static guint32
_load_hostid_from_persist(const gchar *persist_file)
{
  PersistState *state = _create_persist_state(persist_file);

  gsize size;
  guint8 version;
  PersistEntryHandle handle = persist_state_lookup_entry(state, HOST_ID_PERSIST_KEY, &size, &version);

  cr_assert_neq(handle, 0, "cannot find hostid in persist file");

  HostIdState *host_id_state = persist_state_map_entry(state, handle);
  guint32 result = host_id_state->host_id;

  persist_state_unmap_entry(state, handle);
  persist_state_free(state);
  return result;
}

static void
_init_mainloop_with_persist_file(const gchar *persist_file)
{
  GlobalConfig *cfg = _create_cfg();

  cr_assert(main_loop_initialize_state(cfg, persist_file),
            "main_loop_initialize_state failed");

  cfg_free(cfg);
}

static void
_init_mainloop_with_newly_created_persist_file(const gchar *persist_file)
{
  unlink(persist_file);

  _init_mainloop_with_persist_file(persist_file);
}

static void
_create_persist_file_with_hostid(const gchar *persist_file, guint32 hostid)
{
  unlink(persist_file);
  PersistState *state = _create_persist_state(persist_file);
  PersistEntryHandle handle = persist_state_alloc_entry(state, HOST_ID_PERSIST_KEY, sizeof(HostIdState));
  HostIdState *host_id_state = persist_state_map_entry(state, handle);

  host_id_state->host_id = hostid;

  persist_state_unmap_entry(state, handle);
  persist_state_commit(state);
  persist_state_free(state);
}


TestSuite(hostid, .init = app_startup, .fini = app_shutdown);

#ifndef __hpux

Test(hostid, test_if_hostid_generated_when_persist_file_not_exists)
{
  const gchar *persist_file = "test_hostid1.persist";
  guint32 hostid;
  _init_mainloop_with_newly_created_persist_file(persist_file);

  hostid = _load_hostid_from_persist(persist_file);

  cr_assert_eq(hostid, host_id_get(),
               "read hostid(%u) differs from the newly generated hostid(%u)",
               hostid, host_id_get());

  unlink(persist_file);
}

Test(hostid, test_if_hostid_remain_unchanged_when_persist_file_exists)
{
  const gchar *persist_file = "test_hostid2.persist";
  const int hostid = 323;
  _create_persist_file_with_hostid(persist_file, hostid);

  _init_mainloop_with_persist_file(persist_file);

  cr_assert_eq(host_id_get(), hostid,
               "loaded hostid(%d) differs from expected (%d)",
               host_id_get(), hostid);

  unlink(persist_file);
}

#endif
