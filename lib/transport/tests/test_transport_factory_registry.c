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

#include "transport/transport-factory-registry.h"
#include "apphook.h"
#include <criterion/criterion.h>

typedef struct _TestTransportFactory
{
  TransportFactory super;
}TestTransportFactory;

DEFINE_TRANSPORT_FACTORY_ID_FUN(test_transport_factory_id);

static TestTransportFactory *
_test_transport_factory_new(void)
{
  TestTransportFactory *instance = g_new(TestTransportFactory, 1);

  instance->super.id = test_transport_factory_id();

  return instance;
}

TestSuite(transport_factory_registry, .init = app_startup, .fini = app_shutdown);

Test(transport_factory_registry, basic_functionality)
{
  TransportFactoryRegistry *registry = transport_factory_registry_new();

  TestTransportFactory *factory = _test_transport_factory_new();

  transport_factory_registry_add(registry, &factory->super);
  const TransportFactory *looked_up_factory = (TransportFactory*)transport_factory_registry_lookup(registry, test_transport_factory_id());
  cr_expect_eq(factory, looked_up_factory);

  transport_factory_registry_free(registry);
}
