/*
 * Copyright (c) 2018 Balabit
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

#include <syslog-ng.h>

#include <criterion/criterion.h>
#include "mock-transport.h"

#include <errno.h>

Test(mock_transport, test_mock_transport_read)
{

  LogTransportMock *transport = (LogTransportMock *)log_transport_mock_records_new(LTM_EOF);
  cr_assert(transport);

  log_transport_mock_inject_data(transport, "chunk1", sizeof("chunk1"));
  log_transport_mock_inject_data(transport, "chunk2", sizeof("chunk2"));

  gchar buffer[100];
  log_transport_read((LogTransport *)transport, buffer, sizeof(buffer), NULL);
  cr_assert_str_eq(buffer, "chunk1");

  /* Only odd reads result in data transfer. Every even read results in EAGAIN */
  int rc;
  rc = log_transport_read((LogTransport *)transport, buffer, sizeof(buffer), NULL);
  cr_assert_eq(rc, -1);
  cr_assert_eq(errno, EAGAIN);

  log_transport_read((LogTransport *)transport, buffer, sizeof(buffer), NULL);
  cr_assert_str_eq(buffer, "chunk2");

  log_transport_free((LogTransport *)transport);
}

Test(mock_transport, test_mock_transport_simple_write)
{
  LogTransportMock *transport = (LogTransportMock *)log_transport_mock_records_new(LTM_EOF);
  cr_assert(transport);

  gchar buffer[100];

  log_transport_write((LogTransport *)transport, "chunk", sizeof("chunk"));
  log_transport_mock_read_from_write_buffer(transport, buffer, sizeof(buffer));
  cr_assert_str_eq(buffer, "chunk");

  log_transport_free((LogTransport *)transport);
}

Test(mock_transport, test_mock_transport_write_with_chunk_limit)
{
  LogTransportMock *transport = (LogTransportMock *)log_transport_mock_records_new(LTM_EOF);
  cr_assert(transport);

  gchar buffer[100];

  log_transport_mock_set_write_chunk_limit(transport, 2);
  gssize count = log_transport_write((LogTransport *)transport, "chunk", 6);
  cr_assert(count == 2);
  count = log_transport_write((LogTransport *)transport, "unk", 4);
  cr_assert(count == 2);
  count = log_transport_write((LogTransport *)transport, "k", 2);
  cr_assert(count == 2);

  log_transport_mock_read_from_write_buffer(transport, buffer, sizeof(buffer));
  cr_assert_str_eq(buffer, "chunk");

  log_transport_free((LogTransport *)transport);
}

Test(mock_transport, test_mock_transport_writev)
{
  LogTransportMock *transport = (LogTransportMock *)log_transport_mock_records_new(LTM_EOF);
  cr_assert(transport);

  gchar buffer[100] = "chunkofdata";

  struct iovec iov = { .iov_base = buffer, .iov_len = strlen(buffer) + 1 };

  log_transport_writev((LogTransport *)transport, &iov, 1);
  memset(buffer, 0, sizeof(buffer));
  log_transport_mock_read_from_write_buffer(transport, buffer, sizeof(buffer));
  cr_assert_str_eq(buffer, "chunkofdata");

  log_transport_free((LogTransport *)transport);
}

Test(mock_transport, test_mock_transport_writev_with_chunk_limit)
{
  LogTransportMock *transport = (LogTransportMock *)log_transport_mock_records_new(LTM_EOF);
  cr_assert(transport);

  gchar buffer[100] = "chunkofdata";

  struct iovec iov = { .iov_base = buffer, .iov_len = strlen(buffer) + 1 };

  log_transport_mock_set_write_chunk_limit(transport, 2);
  gssize count = log_transport_writev((LogTransport *)transport, &iov, 1);
  cr_assert(count == 2);

  log_transport_mock_set_write_chunk_limit(transport, 0);
  iov.iov_base = &buffer[2];
  iov.iov_len -= 2;
  count = log_transport_writev((LogTransport *)transport, &iov, 1);
  cr_assert(count == 10);

  memset(buffer, 0, sizeof(buffer));
  log_transport_mock_read_from_write_buffer(transport, buffer, sizeof(buffer));
  cr_assert_str_eq(buffer, "chunkofdata");

  log_transport_free((LogTransport *)transport);
}

Test(mock_transport, test_mock_transport_read_from_write_buffer)
{
  LogTransportMock *transport = (LogTransportMock *)log_transport_mock_records_new(LTM_EOF);
  cr_assert(transport);

  gchar buffer[100];

  log_transport_write((LogTransport *)transport, "chunk1", sizeof("chunk1"));
  log_transport_write((LogTransport *)transport, "chunk2", sizeof("chunk2"));

  log_transport_mock_read_from_write_buffer(transport, buffer, sizeof("chunk1"));
  cr_assert_str_eq(buffer, "chunk1");
  log_transport_mock_read_from_write_buffer(transport, buffer, sizeof("chunk2"));
  cr_assert_str_eq(buffer, "chunk2");

  log_transport_free((LogTransport *)transport);
}

Test(mock_transport, test_mock_transport_read_chunk_from_write_buffer)
{
  LogTransportMock *transport = (LogTransportMock *)log_transport_mock_records_new(LTM_EOF);
  cr_assert(transport);

  gchar buffer[100];
  int rc;

  log_transport_write((LogTransport *)transport, "chunk1", sizeof("chunk1"));
  log_transport_write((LogTransport *)transport, "chunk2", sizeof("chunk2"));
  log_transport_write((LogTransport *)transport, "chunk3", sizeof("chunk3"));

  rc = log_transport_mock_read_chunk_from_write_buffer(transport, buffer);
  cr_assert_str_eq(buffer, "chunk1");
  cr_assert_eq(rc, sizeof("chunk1"));

  /* seeking 1 position to step into the middle of the chunk */
  log_transport_mock_read_from_write_buffer(transport, buffer, 1);

  rc = log_transport_mock_read_chunk_from_write_buffer(transport, buffer);
  cr_assert_str_eq(buffer, "hunk2");
  cr_assert_eq(rc, sizeof("hunk2"));

  rc = log_transport_mock_read_chunk_from_write_buffer(transport, buffer);
  cr_assert_str_eq(buffer, "chunk3");
  cr_assert_eq(rc, sizeof("chunk3"));

  log_transport_free((LogTransport *)transport);
}

Test(mock_transport, test_cloning)
{
  LogTransportMock *transport = (LogTransportMock *)log_transport_mock_records_new(LTM_EOF);
  cr_assert(transport);

  gchar buffer[100];

  log_transport_write((LogTransport *)transport, "chunk1", sizeof("chunk1"));
  log_transport_write((LogTransport *)transport, "chunk2", sizeof("chunk2"));

  LogTransportMock *clone = log_transport_mock_clone(transport);

  log_transport_mock_read_from_write_buffer(clone, buffer, sizeof("chunk1"));
  cr_assert_str_eq(buffer, "chunk1");
  log_transport_mock_read_from_write_buffer(clone, buffer, sizeof("chunk2"));
  cr_assert_str_eq(buffer, "chunk2");

  log_transport_free((LogTransport *)transport);
  log_transport_free((LogTransport *)clone);
}
