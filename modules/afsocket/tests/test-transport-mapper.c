/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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

#include "afsocket.h"
#include "transport-mapper.h"
#include "transport-mapper-lib.h"
#include "apphook.h"

TransportMapper *
transport_mapper_dummy_new(void)
{
  TransportMapper *self = g_new0(TransportMapper, 1);

  transport_mapper_init_instance(self, "dummy");
  return self;
}

Test(transport_mapper, test_set_transport_stores_value)
{
  transport_mapper_set_transport(transport_mapper, "foobar");
  assert_transport_mapper_transport(transport_mapper, "foobar");
}

Test(transport_mapper, test_set_address_value_stores_value)
{
  transport_mapper_set_address_family(transport_mapper, AF_UNIX);
  assert_transport_mapper_address_family(transport_mapper, AF_UNIX);
  transport_mapper_set_address_family(transport_mapper, AF_INET);
  assert_transport_mapper_address_family(transport_mapper, AF_INET);
}

Test(transport_mapper, test_apply_transport_returns_success)
{
  assert_transport_mapper_apply(transport_mapper, "transport_mapper_apply_transport() failed");
}

static void
setup(void)
{
  transport_mapper = transport_mapper_dummy_new();
  app_startup();
}

static void
teardown(void)
{
  transport_mapper_free(transport_mapper);
  app_shutdown();
}

TestSuite(transport_mapper, .init = setup, .fini = teardown);
