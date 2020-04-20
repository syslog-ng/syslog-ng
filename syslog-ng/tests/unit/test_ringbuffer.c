/*
 * Copyright (c) 2016 Balabit
 * Copyright (c) 2014 Laszlo Budai
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

#include <criterion/criterion.h>
#include <string.h>

#include "ringbuffer.h"

static const guint32 capacity = 47;

typedef struct _TestData
{
  int idx;
  gboolean ack;
} TestData;

static void
_ringbuffer_init(RingBuffer *self)
{
  ring_buffer_alloc(self, sizeof(TestData), capacity);
}

static void
_ringbuffer_fill(RingBuffer *rb, size_t n, int start_idx, gboolean ack)
{
  TestData *td;
  int i;

  for (i = 0; i < n; i++)
    {
      td = ring_buffer_push(rb);
      td->idx = start_idx + i;
      td->ack = ack;
    }
}

static void
_ringbuffer_fill2(RingBuffer *rb, size_t n, int start_idx, gboolean ack)
{
  TestData *td;
  int i;

  for (i = 0; i < n; i++)
    {
      td = ring_buffer_tail(rb);
      td->idx = start_idx + i;
      td->ack = ack;
      cr_assert_eq(ring_buffer_push(rb), td, "Push should return last tail.");
    }
}

static gboolean
_is_continual(void *data)
{
  TestData *td = (TestData *)data;

  return td->ack;
}

static void
assert_continual_range_length_equals(RingBuffer *rb, size_t expected)
{
  size_t range_len = ring_buffer_get_continual_range_length(rb, _is_continual);
  cr_assert_eq(range_len, expected);
}

static void
assert_test_data_idx_range_in(RingBuffer *rb, int start, int end)
{
  TestData *td;
  int i;

  cr_assert_eq(ring_buffer_count(rb), end-start + 1,
               "invalid ringbuffer size; actual:%d, expected: %d",
               ring_buffer_count(rb), end-start+1);

  for (i = start; i <= end; i++)
    {
      td = ring_buffer_element_at(rb, i - start);
      cr_assert_eq(td->idx, i, "wrong order: idx:(%d) <-> td->idx(%d)", i, td->idx);
    }
}


Test(ringbuffer, test_init_buffer_state)
{
  RingBuffer rb;

  _ringbuffer_init(&rb);

  cr_assert_not(ring_buffer_is_full(&rb), "buffer should not be full");
  cr_assert(ring_buffer_is_empty(&rb), "buffer should be empty");
  cr_assert_eq(ring_buffer_count(&rb), 0, "buffer should be empty");
  cr_assert_eq(ring_buffer_capacity(&rb), capacity, "invalid buffer capacity");

  ring_buffer_free(&rb);
}

Test(ringbuffer, test_pop_from_empty_buffer)
{
  RingBuffer rb;

  _ringbuffer_init(&rb);
  cr_assert_null(ring_buffer_pop(&rb), "cannot pop from empty buffer");

  ring_buffer_free(&rb);
}

Test(ringbuffer, test_push_to_full_buffer)
{
  RingBuffer rb;

  _ringbuffer_init(&rb);
  _ringbuffer_fill(&rb, capacity, 1, TRUE);
  cr_assert_null(ring_buffer_push(&rb), "cannot push to a full buffer");

  ring_buffer_free(&rb);
}

Test(ringbuffer, test_ring_buffer_is_full)
{
  RingBuffer rb;
  int i;
  TestData *last = NULL;

  _ringbuffer_init(&rb);

  for (i = 1; !ring_buffer_is_full(&rb); i++)
    {
      TestData *td = ring_buffer_push(&rb);
      cr_assert_not_null(td, "ring_buffer_push failed");
      td->idx = i;
      last = td;
    }

  cr_assert_eq(ring_buffer_count(&rb), capacity, "buffer count(%d) is not equal to capacity(%d)", ring_buffer_count(&rb),
               capacity);
  cr_assert_eq(last->idx, capacity, "buffer is not full, number of inserted items: %d, capacity: %d", last->idx,
               capacity);

  ring_buffer_free(&rb);
}

Test(ringbuffer, test_pop_all_pushed_element_in_correct_order)
{
  RingBuffer rb;
  int cnt = 0;
  const int start_from = 1;
  TestData *td = NULL;

  _ringbuffer_init(&rb);

  _ringbuffer_fill(&rb, capacity, 1, TRUE);

  while ((td = ring_buffer_pop(&rb)))
    {
      cr_assert_eq((cnt+start_from), td->idx, "wrong order; %d != %d", td->idx, cnt);
      ++cnt;
    }

  cr_assert_eq(cnt, capacity, "cannot read all element, %d < %d", cnt, capacity);

  ring_buffer_free(&rb);
}

Test(ringbuffer, test_drop_elements)
{
  RingBuffer rb;
  const int rb_capacity = 103;
  const int drop = 31;

  ring_buffer_alloc(&rb, sizeof(TestData), rb_capacity);

  _ringbuffer_fill(&rb, rb_capacity, 1, TRUE);

  ring_buffer_drop(&rb, drop);
  cr_assert_eq(ring_buffer_count(&rb), (rb_capacity - drop), "drop failed");

  ring_buffer_free(&rb);
}

Test(ringbuffer, test_elements_ordering)
{
  RingBuffer rb;
  TestData *td;
  int start_from = 10;
  int cnt = 0;

  _ringbuffer_init(&rb);

  _ringbuffer_fill(&rb, capacity, start_from, TRUE);

  while ( (td = ring_buffer_pop(&rb)) )
    {
      cr_assert_eq((cnt + start_from), td->idx, "wrong order; %d != %d", cnt, td->idx);
      ++cnt;
    }

  ring_buffer_free(&rb);
}

Test(ringbuffer, test_element_at)
{
  RingBuffer rb;
  guint32 i;
  TestData *td;

  _ringbuffer_init(&rb);

  _ringbuffer_fill(&rb, capacity, 0, TRUE);

  for ( i = 0; i < ring_buffer_count(&rb); i++ )
    {
      td = ring_buffer_element_at(&rb, i);
      cr_assert_not_null(td, "invalid element, i=%d", i);
      cr_assert_eq(td->idx, i, "invalid order, actual=%d, expected=%d", td->idx, i);
    }

  ring_buffer_free(&rb);
}

Test(ringbuffer, test_continual_range)
{
  RingBuffer rb;

  _ringbuffer_init(&rb);

  _ringbuffer_fill(&rb, capacity, 1, TRUE);
  assert_continual_range_length_equals(&rb, capacity);

  ring_buffer_free(&rb);
}

Test(ringbuffer, test_zero_length_continual_range)
{
  RingBuffer rb;
  TestData *td;

  _ringbuffer_init(&rb);

  _ringbuffer_fill(&rb, capacity, 1, TRUE);

  td = ring_buffer_element_at(&rb, 0);
  td->ack = FALSE;

  assert_continual_range_length_equals(&rb, 0);

  ring_buffer_free(&rb);
}

Test(ringbuffer, test_broken_continual_range)
{
  RingBuffer rb;

  TestData *td;

  _ringbuffer_init(&rb);

  _ringbuffer_fill(&rb, capacity, 1, TRUE);

  td = ring_buffer_element_at(&rb, 13);
  td->ack = FALSE;

  assert_continual_range_length_equals(&rb, 13);

  ring_buffer_free(&rb);
}

Test(ringbuffer, test_push_after_pop)
{
}

Test(ringbuffer, test_tail)
{
  RingBuffer rb;
  TestData *td_tail;

  ring_buffer_alloc(&rb, sizeof(TestData), 103);
  _ringbuffer_fill2(&rb, 103, 0, TRUE);

  ring_buffer_pop(&rb);

  td_tail = ring_buffer_tail(&rb);
  td_tail->idx = 103;

  cr_assert_eq(ring_buffer_push(&rb), td_tail, "Push should return last tail.");

  assert_test_data_idx_range_in(&rb, 1, 103);

  ring_buffer_free(&rb);
}
