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
#include "transport-mapper-unix.h"
#include "apphook.h"
#include "stats/stats-registry.h"
#include "transport-mapper-lib.h"

static void
test_transport_mapper_unix_stream_apply_transport_sets_defaults()
{
  assert_transport_mapper_apply(transport_mapper, NULL);
  assert_transport_mapper_transport(transport_mapper, "unix-stream");
  assert_transport_mapper_address_family(transport_mapper, AF_UNIX);
  assert_transport_mapper_sock_type(transport_mapper, SOCK_STREAM);
  assert_transport_mapper_sock_proto(transport_mapper, 0);
  assert_transport_mapper_logproto(transport_mapper, "text");
  assert_transport_mapper_stats_source(transport_mapper, SCS_UNIX_STREAM);
}

static void
test_transport_mapper_unix_dgram_apply_transport_sets_defaults()
{
  assert_transport_mapper_apply(transport_mapper, NULL);
  assert_transport_mapper_transport(transport_mapper, "unix-dgram");
  assert_transport_mapper_address_family(transport_mapper, AF_UNIX);
  assert_transport_mapper_sock_type(transport_mapper, SOCK_DGRAM);
  assert_transport_mapper_sock_proto(transport_mapper, 0);
  assert_transport_mapper_logproto(transport_mapper, "dgram");
  assert_transport_mapper_stats_source(transport_mapper, SCS_UNIX_DGRAM);
}

static void
test_transport_mapper_unix(void)
{
  TRANSPORT_MAPPER_TESTCASE(unix_stream, test_transport_mapper_unix_stream_apply_transport_sets_defaults);
  TRANSPORT_MAPPER_TESTCASE(unix_dgram, test_transport_mapper_unix_dgram_apply_transport_sets_defaults);
}

int
main(int argc, char *argv[])
{
  app_startup();
  test_transport_mapper_unix();
  app_shutdown();
  return 0;
}
