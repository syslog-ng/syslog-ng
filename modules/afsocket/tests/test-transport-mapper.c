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

static void
test_set_transport_stores_value(void)
{
  transport_mapper_set_transport(transport_mapper, "foobar");
  assert_transport_mapper_transport(transport_mapper, "foobar");
}

static void
test_set_address_value_stores_value(void)
{
  transport_mapper_set_address_family(transport_mapper, AF_UNIX);
  assert_transport_mapper_address_family(transport_mapper, AF_UNIX);
  transport_mapper_set_address_family(transport_mapper, AF_INET);
  assert_transport_mapper_address_family(transport_mapper, AF_INET);
}

static void
test_apply_transport_returns_success(void)
{
  assert_transport_mapper_apply(transport_mapper, "transport_mapper_apply_transport() failed");
}

static void
test_transport_mapper(void)
{
  TRANSPORT_MAPPER_TESTCASE(dummy, test_set_transport_stores_value);
  TRANSPORT_MAPPER_TESTCASE(dummy, test_set_address_value_stores_value);
  TRANSPORT_MAPPER_TESTCASE(dummy, test_apply_transport_returns_success);
}

int
main(int argc, char *argv[])
{
  app_startup();
  test_transport_mapper();
  app_shutdown();
  return 0;
}
