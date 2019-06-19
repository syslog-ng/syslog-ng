/*
 * Copyright (c) 2002-2019 Balabit
 * Copyright (c) 2019 Laszlo Budai
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
#include "ack-tracker/late_ack_record_container.h"

typedef struct _TestBookmark
{
  gsize idx;
} TestBookmark;

static TestBookmark *
_ack_record_extract_bookmark(LateAckRecord *self)
{
  return (TestBookmark *)&(self->bookmark.container);
}

static void
_ack_record_write_idx_to_bookmark(LateAckRecord *self, gsize idx)
{
  TestBookmark *bookmark = _ack_record_extract_bookmark(self);
  bookmark->idx = idx;
}

static void
_fill_container(LateAckRecordContainer *container, gsize n, gsize idx_shift)
{
  for (gsize i = 0; i < n; i++)
    {
      LateAckRecord *rec = late_ack_record_container_request_pending(container);
      rec->acked = FALSE;
      _ack_record_write_idx_to_bookmark(rec, i + idx_shift);
      late_ack_record_container_store_pending(container);
    }
}

static void
_set_ack_range(LateAckRecordContainer *ack_records, gsize start, gsize n)
{
  for (gsize i = start; i < start + n; i++)
    {
      LateAckRecord *rec = late_ack_record_container_at(ack_records, i);
      rec->acked = TRUE;
    }
}

static void
_assert_ack_range(LateAckRecordContainer *ack_records, gsize start, gsize n, gboolean expected_acked)
{
  for (gsize i = start; i < start + n; i++)
    {
      LateAckRecord *rec = late_ack_record_container_at(ack_records, i);
      cr_assert_eq(rec->acked, expected_acked);
    }
}

static void
_assert_indexes(LateAckRecordContainer *ack_records, gsize n, gsize idx_shift)
{
  for (gsize i = 0; i < n; i++)
    {
      LateAckRecord *rec = late_ack_record_container_at(ack_records, i);
      TestBookmark *bookmark = _ack_record_extract_bookmark(rec);
      cr_assert_eq(bookmark->idx, i + idx_shift);
    }
}

Test(late_ack_record_container_static, is_empty)
{
  static const gsize capacity = 32;
  LateAckRecordContainer *ack_records = late_ack_record_container_static_new(capacity);
  {
    cr_expect_eq(late_ack_record_container_size(ack_records), 0);

    cr_expect(late_ack_record_container_is_empty(ack_records));
    _fill_container(ack_records, capacity, 0);
    cr_expect_not(late_ack_record_container_is_empty(ack_records));
    cr_expect_eq(late_ack_record_container_size(ack_records), 32);
  }
  late_ack_record_container_free(ack_records);
}

Test(late_ack_record_container_static, request_store_pending)
{
  static const gsize capacity = 32;
  LateAckRecordContainer *ack_records = late_ack_record_container_static_new(capacity);
  {
    for (gsize i = 0; i < capacity; i++)
      {
        LateAckRecord *rec = late_ack_record_container_request_pending(ack_records);
        _ack_record_write_idx_to_bookmark(rec, i);
        late_ack_record_container_store_pending(ack_records);
        rec = late_ack_record_container_at(ack_records, i);
        TestBookmark *bookmark = _ack_record_extract_bookmark(rec);
        cr_expect_eq(bookmark->idx, i);
      }
  }
  late_ack_record_container_free(ack_records);
}

Test(late_ack_record_container_static, drop_more_than_current_size, .signal = SIGABRT)
{
  static const gsize capacity = 32;
  LateAckRecordContainer *ack_records = late_ack_record_container_static_new(capacity);
  {
    _fill_container(ack_records, capacity, 0);
    late_ack_record_container_drop(ack_records, 16);
    cr_expect_eq(late_ack_record_container_size(ack_records), 16);
    // when trying to drop more than current size, should assert (->SIGABRT)
    late_ack_record_container_drop(ack_records, 20);
  }
  late_ack_record_container_free(ack_records);
}

Test(late_ack_record_container_static, drop_from_the_beginning)
{
  static const gsize capacity = 32;
  LateAckRecordContainer *ack_records = late_ack_record_container_static_new(capacity);
  {
    _fill_container(ack_records, capacity, 0);
    late_ack_record_container_drop(ack_records, 16);
    _assert_indexes(ack_records, 16, 16);
    cr_expect_eq(late_ack_record_container_size(ack_records), 16);
  }
  late_ack_record_container_free(ack_records);
}

Test(late_ack_record_container_static, request_pending_should_return_null_when_capacity_reached)
{
  static const gsize capacity = 32;
  LateAckRecordContainer *ack_records = late_ack_record_container_static_new(capacity);
  {
    _fill_container(ack_records, capacity, 0);
    LateAckRecord *rec = late_ack_record_container_request_pending(ack_records);
    cr_expect_eq(rec, NULL);
  }
  late_ack_record_container_free(ack_records);
}

Test(late_ack_record_container_static, store_pending_should_not_modify_container_when_capacity_reached)
{
  static const gsize capacity = 32;
  LateAckRecordContainer *ack_records = late_ack_record_container_static_new(capacity);
  {
    _fill_container(ack_records, capacity, 0);
    late_ack_record_container_store_pending(ack_records);
    _assert_indexes(ack_records, 32, 0);
    LateAckRecord *rec = late_ack_record_container_request_pending(ack_records);
    cr_expect_eq(rec, NULL);
    late_ack_record_container_store_pending(ack_records);
    _assert_indexes(ack_records, 32, 0);
  }
  late_ack_record_container_free(ack_records);
}

Test(late_ack_record_container_static, after_drop_from_a_full_container_can_store_new_element)
{
  static const gsize capacity = 32;
  LateAckRecordContainer *ack_records = late_ack_record_container_static_new(capacity);
  {
    _fill_container(ack_records, capacity, 0);
    late_ack_record_container_drop(ack_records, 1);
    LateAckRecord *rec = late_ack_record_container_at(ack_records, 0);
    TestBookmark *bookmark = _ack_record_extract_bookmark(rec);
    cr_expect_eq(bookmark->idx, 1);
    _fill_container(ack_records, 1, 32); //append new element, idx = 32
    cr_expect_eq(late_ack_record_container_size(ack_records), capacity);
    // all values shifted by 1
    _assert_indexes(ack_records, capacity, 1);
  }
  late_ack_record_container_free(ack_records);
}

Test(late_ack_record_container_static, get_continual_range_length)
{
  static const gsize capacity = 32;
  LateAckRecordContainer *ack_records = late_ack_record_container_static_new(capacity);
  {
    _fill_container(ack_records, capacity, 0);
    _set_ack_range(ack_records, 1, 15);
    _assert_ack_range(ack_records, 0, 1, FALSE);
    _assert_ack_range(ack_records, 1, 15, TRUE);
    _assert_ack_range(ack_records, 16, 16, FALSE);
    cr_expect_eq(late_ack_record_container_get_continual_range_length(ack_records), 0);
    late_ack_record_container_drop(ack_records, 1); // drop unacked...
    cr_expect_eq(late_ack_record_container_get_continual_range_length(ack_records), 15);
  }
  late_ack_record_container_free(ack_records);
}
