/*
 * Copyright (c) 2002-2018 Balabit
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

#include "transport/transport-factory-id.h"
#include "apphook.h"
#include <criterion/criterion.h>

TestSuite(transport_factory_id, .init = app_startup, .fini = app_shutdown);

Test(transport_factory_id, lifecycle)
{
  GList *ids = transport_factory_id_clone_registered_ids();

  cr_expect_null(ids);

  TransportFactoryId *id = TRANSPORT_FACTORY_ID_NEW("tcp");
  TransportFactoryId *expected = transport_factory_id_clone(id);
  transport_factory_id_register(id);

  ids = transport_factory_id_clone_registered_ids();
  cr_assert_not_null(ids, "transport_factory_id_register failed");
  cr_expect(transport_factory_id_equal(ids->data, expected));
  transport_factory_id_free(expected);
  g_list_free_full(ids, (GDestroyNotify)transport_factory_id_free);

  transport_factory_id_global_deinit();

  ids = transport_factory_id_clone_registered_ids();
  cr_expect_null(ids);
}

Test(transport_factory_id_basic_macros, generated_ids_are_uniq)
{
  TransportFactoryId *id = TRANSPORT_FACTORY_ID_NEW("tcp");
  TransportFactoryId *id2 = TRANSPORT_FACTORY_ID_NEW("tcp");
  cr_expect_not(transport_factory_id_equal(id, id2));
}

Test(transport_factory_id_basic_macros, clones_are_equals)
{
  TransportFactoryId *id = TRANSPORT_FACTORY_ID_NEW("tcp");
  TransportFactoryId *cloned = transport_factory_id_clone(id);
  cr_expect(transport_factory_id_equal(id, cloned));
  cr_expect_eq(transport_factory_id_hash(id), transport_factory_id_hash(cloned));
}
