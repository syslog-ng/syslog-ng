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
#include "ack-tracker/consecutive_ack_record_container.h"

typedef struct _TestBookmark
{
  gsize idx;
} TestBookmark;

static TestBookmark *
_ack_record_extract_bookmark(ConsecutiveAckRecord *self)
{
  return (TestBookmark *)&(self->super.bookmark.container);
}

static void
_ack_record_write_idx_to_bookmark(ConsecutiveAckRecord *self, gsize idx)
{
  TestBookmark *bookmark = _ack_record_extract_bookmark(self);
  bookmark->idx = idx;
}

static void
_fill_container(ConsecutiveAckRecordContainer *container, gsize n, gsize idx_shift)
{
  for (gsize i = 0; i < n; i++)
    {
      ConsecutiveAckRecord *rec = consecutive_ack_record_container_request_pending(container);
      rec->acked = FALSE;
      _ack_record_write_idx_to_bookmark(rec, i + idx_shift);
      consecutive_ack_record_container_store_pending(container);
    }
}

static void
_set_ack_range(ConsecutiveAckRecordContainer *ack_records, gsize start, gsize n)
{
  for (gsize i = start; i < start + n; i++)
    {
      ConsecutiveAckRecord *rec = consecutive_ack_record_container_at(ack_records, i);
      rec->acked = TRUE;
    }
}

static void
_assert_ack_range(ConsecutiveAckRecordContainer *ack_records, gsize start, gsize n, gboolean expected_acked)
{
  for (gsize i = start; i < start + n; i++)
    {
      ConsecutiveAckRecord *rec = consecutive_ack_record_container_at(ack_records, i);
      cr_assert_eq(rec->acked, expected_acked);
    }
}

static void
_assert_indexes(ConsecutiveAckRecordContainer *ack_records, gsize n, gsize idx_shift)
{
  for (gsize i = 0; i < n; i++)
    {
      ConsecutiveAckRecord *rec = consecutive_ack_record_container_at(ack_records, i);
      TestBookmark *bookmark = _ack_record_extract_bookmark(rec);
      cr_assert_eq(bookmark->idx, i + idx_shift);
    }
}

Test(consecutive_ack_record_container_static, is_empty)
{
  static const gsize capacity = 32;
  ConsecutiveAckRecordContainer *ack_records = consecutive_ack_record_container_static_new(capacity);
  {
    cr_expect_eq(consecutive_ack_record_container_size(ack_records), 0);

    cr_expect(consecutive_ack_record_container_is_empty(ack_records));
    _fill_container(ack_records, capacity, 0);
    cr_expect_not(consecutive_ack_record_container_is_empty(ack_records));
    cr_expect_eq(consecutive_ack_record_container_size(ack_records), 32);
  }
  consecutive_ack_record_container_free(ack_records);
}

Test(consecutive_ack_record_container_static, request_store_pending)
{
  static const gsize capacity = 32;
  ConsecutiveAckRecordContainer *ack_records = consecutive_ack_record_container_static_new(capacity);
  {
    for (gsize i = 0; i < capacity; i++)
      {
        ConsecutiveAckRecord *rec = consecutive_ack_record_container_request_pending(ack_records);
        _ack_record_write_idx_to_bookmark(rec, i);
        consecutive_ack_record_container_store_pending(ack_records);
        rec = consecutive_ack_record_container_at(ack_records, i);
        TestBookmark *bookmark = _ack_record_extract_bookmark(rec);
        cr_expect_eq(bookmark->idx, i);
      }
  }
  consecutive_ack_record_container_free(ack_records);
}

Test(consecutive_ack_record_container_static, drop_more_than_current_size, .signal = SIGABRT)
{
  static const gsize capacity = 32;
  ConsecutiveAckRecordContainer *ack_records = consecutive_ack_record_container_static_new(capacity);
  {
    _fill_container(ack_records, capacity, 0);
    consecutive_ack_record_container_drop(ack_records, 16);
    cr_expect_eq(consecutive_ack_record_container_size(ack_records), 16);
    // when trying to drop more than current size, should assert (->SIGABRT)
    consecutive_ack_record_container_drop(ack_records, 20);
  }
  consecutive_ack_record_container_free(ack_records);
}

Test(consecutive_ack_record_container_static, drop_from_the_beginning)
{
  static const gsize capacity = 32;
  ConsecutiveAckRecordContainer *ack_records = consecutive_ack_record_container_static_new(capacity);
  {
    _fill_container(ack_records, capacity, 0);
    consecutive_ack_record_container_drop(ack_records, 16);
    _assert_indexes(ack_records, 16, 16);
    cr_expect_eq(consecutive_ack_record_container_size(ack_records), 16);
  }
  consecutive_ack_record_container_free(ack_records);
}

Test(consecutive_ack_record_container_static, request_pending_should_return_null_when_capacity_reached)
{
  static const gsize capacity = 32;
  ConsecutiveAckRecordContainer *ack_records = consecutive_ack_record_container_static_new(capacity);
  {
    _fill_container(ack_records, capacity, 0);
    ConsecutiveAckRecord *rec = consecutive_ack_record_container_request_pending(ack_records);
    cr_expect_eq(rec, NULL);
  }
  consecutive_ack_record_container_free(ack_records);
}

Test(consecutive_ack_record_container_static, store_pending_should_not_modify_container_when_capacity_reached)
{
  static const gsize capacity = 32;
  ConsecutiveAckRecordContainer *ack_records = consecutive_ack_record_container_static_new(capacity);
  {
    _fill_container(ack_records, capacity, 0);
    consecutive_ack_record_container_store_pending(ack_records);
    _assert_indexes(ack_records, 32, 0);
    ConsecutiveAckRecord *rec = consecutive_ack_record_container_request_pending(ack_records);
    cr_expect_eq(rec, NULL);
    consecutive_ack_record_container_store_pending(ack_records);
    _assert_indexes(ack_records, 32, 0);
  }
  consecutive_ack_record_container_free(ack_records);
}

Test(consecutive_ack_record_container_static, after_drop_from_a_full_container_can_store_new_element)
{
  static const gsize capacity = 32;
  ConsecutiveAckRecordContainer *ack_records = consecutive_ack_record_container_static_new(capacity);
  {
    _fill_container(ack_records, capacity, 0);
    consecutive_ack_record_container_drop(ack_records, 1);
    ConsecutiveAckRecord *rec = consecutive_ack_record_container_at(ack_records, 0);
    TestBookmark *bookmark = _ack_record_extract_bookmark(rec);
    cr_expect_eq(bookmark->idx, 1);
    _fill_container(ack_records, 1, 32); //append new element, idx = 32
    cr_expect_eq(consecutive_ack_record_container_size(ack_records), capacity);
    // all values shifted by 1
    _assert_indexes(ack_records, capacity, 1);
  }
  consecutive_ack_record_container_free(ack_records);
}

Test(consecutive_ack_record_container_static, get_continual_range_length)
{
  static const gsize capacity = 32;
  ConsecutiveAckRecordContainer *ack_records = consecutive_ack_record_container_static_new(capacity);
  {
    _fill_container(ack_records, capacity, 0);
    _set_ack_range(ack_records, 1, 15);
    _assert_ack_range(ack_records, 0, 1, FALSE);
    _assert_ack_range(ack_records, 1, 15, TRUE);
    _assert_ack_range(ack_records, 16, 16, FALSE);
    cr_expect_eq(consecutive_ack_record_container_get_continual_range_length(ack_records), 0);
    consecutive_ack_record_container_drop(ack_records, 1); // drop unacked...
    cr_expect_eq(consecutive_ack_record_container_get_continual_range_length(ack_records), 15);
  }
  consecutive_ack_record_container_free(ack_records);
}

Test(consecutive_ack_record_container_dynamic, is_empty)
{
  ConsecutiveAckRecordContainer *ack_records = consecutive_ack_record_container_dynamic_new();
  {
    cr_expect_eq(consecutive_ack_record_container_size(ack_records), 0);

    cr_expect(consecutive_ack_record_container_is_empty(ack_records));
    _fill_container(ack_records, 32, 0);
    cr_expect_not(consecutive_ack_record_container_is_empty(ack_records));
    cr_expect_eq(consecutive_ack_record_container_size(ack_records), 32);
  }
  consecutive_ack_record_container_free(ack_records);
}

Test(consecutive_ack_record_container_dynamic, request_store_pending)
{
  ConsecutiveAckRecordContainer *ack_records = consecutive_ack_record_container_dynamic_new();
  {
    for (gsize i = 0; i < 32; i++)
      {
        ConsecutiveAckRecord *rec = consecutive_ack_record_container_request_pending(ack_records);
        _ack_record_write_idx_to_bookmark(rec, i);
        consecutive_ack_record_container_store_pending(ack_records);
        rec = consecutive_ack_record_container_at(ack_records, i);
        TestBookmark *bookmark = _ack_record_extract_bookmark(rec);
        cr_expect_eq(bookmark->idx, i);
      }
  }
  consecutive_ack_record_container_free(ack_records);
}

Test(consecutive_ack_record_container_dynamic, drop_more_than_current_size, .signal = SIGABRT)
{
  ConsecutiveAckRecordContainer *ack_records = consecutive_ack_record_container_dynamic_new();
  {
    _fill_container(ack_records, 32, 0);
    consecutive_ack_record_container_drop(ack_records, 16);
    cr_expect_eq(consecutive_ack_record_container_size(ack_records), 16);
    // when trying to drop more than current size, remove all the elements
    consecutive_ack_record_container_drop(ack_records, 20);
  }
  consecutive_ack_record_container_free(ack_records);
}

Test(consecutive_ack_record_container_dynamic, drop_from_the_beginning)
{
  ConsecutiveAckRecordContainer *ack_records = consecutive_ack_record_container_dynamic_new();
  {
    _fill_container(ack_records, 32, 0);
    consecutive_ack_record_container_drop(ack_records, 16);
    _assert_indexes(ack_records, 16, 16);
    cr_expect_eq(consecutive_ack_record_container_size(ack_records), 16);
  }
  consecutive_ack_record_container_free(ack_records);
}

Test(consecutive_ack_record_container_dynamic, get_continual_range_length)
{
  ConsecutiveAckRecordContainer *ack_records = consecutive_ack_record_container_dynamic_new();
  {
    _fill_container(ack_records, 32, 0);
    _set_ack_range(ack_records, 1, 15);
    _assert_ack_range(ack_records, 0, 1, FALSE);
    _assert_ack_range(ack_records, 1, 15, TRUE);
    _assert_ack_range(ack_records, 16, 16, FALSE);
    cr_expect_eq(consecutive_ack_record_container_get_continual_range_length(ack_records), 0);
    consecutive_ack_record_container_drop(ack_records, 1); // drop unacked...
    cr_expect_eq(consecutive_ack_record_container_get_continual_range_length(ack_records), 15);
  }
  consecutive_ack_record_container_free(ack_records);
}

Test(consecutive_ack_record_container_dynamic, store_pending_after_drop_all)
{
  ConsecutiveAckRecordContainer *ack_records = consecutive_ack_record_container_dynamic_new();
  {
    _fill_container(ack_records, 32, 0);
    ConsecutiveAckRecord *pending = consecutive_ack_record_container_request_pending(ack_records);
    cr_assert_not_null(pending);
    TestBookmark *bookmark = _ack_record_extract_bookmark(pending);
    bookmark->idx = 111;
    consecutive_ack_record_container_drop(ack_records, 32);
    cr_expect(consecutive_ack_record_container_is_empty(ack_records));
    consecutive_ack_record_container_store_pending(ack_records);
    cr_expect_eq(consecutive_ack_record_container_size(ack_records), 1);
    ConsecutiveAckRecord *rec = consecutive_ack_record_container_at(ack_records, 0);
    bookmark = _ack_record_extract_bookmark(rec);
    cr_expect_eq(bookmark->idx, 111);
  }
  consecutive_ack_record_container_free(ack_records);
}

