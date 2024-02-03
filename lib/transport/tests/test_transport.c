/*
 * Copyright (c) 2024 Balazs Scheidler <balazs.scheidler@axoflow.com>
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

#include <criterion/criterion.h>
#include "libtest/mock-transport.h"

#include "transport/logtransport.h"
#include "apphook.h"

#include <errno.h>

Test(transport, test_read_ahead_invokes_only_one_read_operation)
{
  /* this will result in single-byte reads */
  LogTransport *t = log_transport_mock_stream_new("readahead", -1, LTM_EOF);

  gchar buf[12] = {0};
  gboolean moved_forward;
  gint rc;

  for (gint i = 1; i <= 9; i++)
    {
      memset(buf, 0, sizeof(buf));
      rc = log_transport_read_ahead(t, buf, 9, &moved_forward);
      cr_assert(moved_forward == TRUE);
      cr_assert(rc == i, "unexpected rc = %d", rc);
      cr_assert(strncmp(buf, "readahead", i) == 0);
    }
  rc = log_transport_read_ahead(t, buf, 10, &moved_forward);
  cr_assert(rc == 9, "unexpected rc = %d", rc);
  cr_assert(moved_forward == FALSE);

  /* the read() returns the bytes that were read in advance */

  memset(buf, 0, sizeof(buf));
  rc = log_transport_read(t, buf, 9, NULL);
  cr_assert(rc == 9, "unexpected rc = %d", rc);
  cr_assert_str_eq(buf, "readahead");

}

Test(transport, test_read_ahead_bytes_get_shifted_into_the_actual_read)
{
  LogTransport *t = log_transport_mock_records_new("readahead", -1, LTM_EOF);

  gchar buf[12] = {0};
  gboolean moved_forward;
  memset(buf, 0, sizeof(buf));
  gint rc = log_transport_read_ahead(t, buf, 4, &moved_forward);
  cr_assert(rc == 4, "unexpected rc = %d", rc);
  cr_assert_str_eq(buf, "read");

  /* the read() returns the bytes that were read in advance */

  memset(buf, 0, sizeof(buf));
  rc = log_transport_read(t, buf, 4, NULL);
  cr_assert(rc == 4, "unexpected rc = %d", rc);
  cr_assert_str_eq(buf, "read");

}

Test(transport, test_read_ahead_bytes_and_new_read_is_combined)
{
  LogTransport *t = log_transport_mock_records_new("readahead", -1, LTM_EOF);

  gboolean moved_forward;
  gchar buf[12] = {0};
  memset(buf, 0, sizeof(buf));
  gint rc = log_transport_read_ahead(t, buf, 4, &moved_forward);
  cr_assert(rc == 4, "unexpected rc = %d", rc);
  cr_assert_str_eq(buf, "read");

  /* NOTE: the mock will return only a single byte for every read to
   * exercise retry mechanisms, so only read a single character here */

  memset(buf, 0, sizeof(buf));
  rc = log_transport_read(t, buf, 5, NULL);
  cr_assert(rc == 5, "unexpected rc = %d", rc);
  cr_assert_str_eq(buf, "reada");

}

Test(transport, test_read_ahead_returns_the_same_buffer_any_number_of_times)
{
  LogTransport *t = log_transport_mock_records_new("readahead", -1, LTM_EOF);
  gboolean moved_forward;

  gchar buf[12];

  memset(buf, 0, sizeof(buf));
  cr_assert(log_transport_read_ahead(t, buf, 1, &moved_forward) == 1);
  cr_assert_str_eq(buf, "r");
  memset(buf, 0, sizeof(buf));
  cr_assert(log_transport_read_ahead(t, buf, 2, &moved_forward) == 2);
  cr_assert_str_eq(buf, "re");
  memset(buf, 0, sizeof(buf));
  cr_assert(log_transport_read_ahead(t, buf, 8, &moved_forward) == 8);
  cr_assert_str_eq(buf, "readahea");
  memset(buf, 0, sizeof(buf));
  cr_assert(log_transport_read_ahead(t, buf, 4, &moved_forward) == 4);
  cr_assert_str_eq(buf, "read");

  /* the read() returns the bytes that were read in advance */
  cr_assert(log_transport_read(t, buf, 9, NULL) == 9);
  cr_assert_str_eq(buf, "readahead");

}

Test(transport, test_read_ahead_more_than_the_internal_buffer, .signal = SIGABRT)
{
  LogTransport *t = log_transport_mock_records_new("12345678901234567890", -1, LTM_EOF);
  gboolean moved_forward;

  /* 20 bytes, the internal look ahead buffer in LogTransport is 16 bytes which we are overflowing here */
  cr_assert(sizeof(t->ra.buf) == 16);

  gchar buf[32];

  memset(buf, 0, sizeof(buf));
  cr_assert(log_transport_read_ahead(t, buf, 20, &moved_forward) == 20);
}

Test(transport, test_read_ahead_with_packets_split_in_half)
{
  LogTransport *t = log_transport_mock_records_new("1234", -1, "5678", -1, LTM_EOF);
  gboolean moved_forward;

  gchar buf[32];
  memset(buf, 0, sizeof(buf));
  gint rc = log_transport_read_ahead(t, buf, 8, &moved_forward);
  cr_assert(rc == 4, "unexpected rc = %d", rc);
  cr_assert_str_eq(buf, "1234");

  memset(buf, 0, sizeof(buf));
  rc = log_transport_read_ahead(t, buf, 8, &moved_forward);
  cr_assert(rc == 8, "unexpected rc = %d", rc);
  cr_assert_str_eq(buf, "12345678");
}

TestSuite(transport, .init = app_startup, .fini = app_shutdown);
