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

#include "transport/multitransport.h"
#include "transport/transport-factory.h"
#include "apphook.h"
#include <criterion/criterion.h>

#define DEFINE_TEST_TRANSPORT_WITH_FACTORY(TypePrefix, FunPrefix) \
  typedef struct TypePrefix ## Transport_ TypePrefix ## Transport; \
  typedef struct TypePrefix ## TransportFactory_ TypePrefix ## TransportFactory; \
  struct TypePrefix ## Transport_ \
  { \
    LogTransport super; \
  }; \
  static gssize \
  FunPrefix ## _read(LogTransport *s, gpointer buf, gsize count, LogTransportAuxData *aux) \
  { \
    return count; \
  }\
  static gssize \
  FunPrefix ## _write(LogTransport *s, const gpointer buf, gsize count) \
  { \
    return count; \
  }\
  LogTransport * \
  FunPrefix ## _transport_new(void) \
  { \
    TypePrefix ## Transport *self = g_new0(TypePrefix ## Transport, 1); \
    self->super.read = FunPrefix ## _read; \
    self->super.write = FunPrefix ## _write; \
    return &self->super; \
  } \
  struct TypePrefix ## TransportFactory_\
  { \
    TransportFactory super; \
  }; \
  DEFINE_TRANSPORT_FACTORY_ID_FUN(#FunPrefix, FunPrefix ## _transport_factory_id); \
  static LogTransport * \
  FunPrefix ## _factory_construct(const TransportFactory *s, gint fd) \
  {\
    LogTransport *transport = FunPrefix ## _transport_new(); \
    log_transport_init_instance(transport, fd); \
    return transport; \
  } \
  TransportFactory * \
  FunPrefix ## _transport_factory_new(void) \
  { \
    TypePrefix ## TransportFactory *self = g_new0(TypePrefix ## TransportFactory, 1); \
    self->super.id = FunPrefix ## _transport_factory_id(); \
    self->super.construct_transport = FunPrefix ## _factory_construct; \
    return &self->super; \
  } \

TestSuite(multitransport, .init = app_startup, .fini = app_shutdown);

DEFINE_TEST_TRANSPORT_WITH_FACTORY(Fake, fake);
DEFINE_TEST_TRANSPORT_WITH_FACTORY(Default, default);
DEFINE_TEST_TRANSPORT_WITH_FACTORY(Unregistered, unregistered);

Test(multitransport, test_switch_transport)
{
  TransportFactory *default_factory = default_transport_factory_new();
  TransportFactory *fake_factory = fake_transport_factory_new();
  gint fd = -2;

  MultiTransport *multi_transport = (MultiTransport *) multitransport_new(default_factory, fd);

  cr_expect_eq(multi_transport->active_transport->read, default_read);
  cr_expect_eq(multi_transport->active_transport->write, default_write);
  cr_expect_str_eq(multi_transport->active_transport->name, "default");

  multitransport_add_factory(multi_transport, fake_factory);
  cr_expect_not(multitransport_switch(multi_transport, unregistered_transport_factory_id()));
  cr_expect_eq(multi_transport->active_transport->read, default_read);
  cr_expect_eq(multi_transport->active_transport->write, default_write);
  cr_expect_eq(multi_transport->active_transport_factory, default_factory);
  cr_expect_str_eq(multi_transport->active_transport->name, "default");

  cr_expect(multitransport_switch(multi_transport, fake_transport_factory_id()));
  cr_expect_eq(multi_transport->active_transport->read, fake_read);
  cr_expect_eq(multi_transport->active_transport->write, fake_write);
  cr_expect_eq(multi_transport->active_transport_factory, fake_factory);
  cr_expect_str_eq(multi_transport->active_transport->name, "fake");

  log_transport_free(&multi_transport->super);
}

