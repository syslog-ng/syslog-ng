#include <string.h>

#include "testutils.h"
#include "ringbuffer.h"

#define RINGBUFFER_TESTCASE(testfunc, ...) { testcase_begin("%s(%s)", #testfunc, #__VA_ARGS__); testfunc(__VA_ARGS__); testcase_end(); }

static const size_t capacity = 47;

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
      assert_true(ring_buffer_push(rb) == td, "Push should return last tail.");
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
  assert_true(range_len == expected, "actual=%d, expected=%d", range_len, expected);
}

static void
assert_test_data_idx_range_in(RingBuffer *rb, int start, int end)
{
  TestData *td;
  int i;

  assert_true(ring_buffer_count(rb) == end-start+1, "invalid ringbuffer size; actual:%d, expected: %d", ring_buffer_count(rb), end-start);

  for (i = start; i <= end; i++)
    {
      td = ring_buffer_element_at(rb, i - start);
      assert_true(td->idx == i, "wrong order: idx:(%d) <-> td->idx(%d)", i, td->idx);
    }
}

static void
test_init_buffer_state()
{
  RingBuffer rb;

  _ringbuffer_init(&rb);

  assert_false(ring_buffer_is_full(&rb), "buffer should not be full");
  assert_true(ring_buffer_is_empty(&rb), "buffer should be empty");
  assert_true(ring_buffer_count(&rb) == 0, "buffer should be empty");
  assert_true(ring_buffer_capacity(&rb) == capacity, "invalid buffer capacity");

  ring_buffer_free(&rb);
}

static void
test_pop_from_empty_buffer()
{
  RingBuffer rb;

  _ringbuffer_init(&rb);
  assert_true(ring_buffer_pop(&rb) == NULL, "cannot pop from empty buffer");

  ring_buffer_free(&rb);
}

static void
test_push_to_full_buffer()
{
  RingBuffer rb;

  _ringbuffer_init(&rb);
  _ringbuffer_fill(&rb, capacity, 1, TRUE);
  assert_true(ring_buffer_push(&rb) == NULL, "cannot push to a full buffer");

  ring_buffer_free(&rb);
}

static void
test_ring_buffer_is_full()
{
  RingBuffer rb;
  int i;
  TestData *last = NULL;

  _ringbuffer_init(&rb);

  for (i = 1; !ring_buffer_is_full(&rb); i++)
    {
      TestData *td = ring_buffer_push(&rb);
      assert_true(td != NULL, "ring_buffer_push failed");
      td->idx = i;
      last = td;
    }

  assert_true(ring_buffer_count(&rb) == capacity, "buffer count(%d) is not equal to capacity(%d)", ring_buffer_count(&rb), capacity);
  assert_true(last->idx == capacity, "buffer is not full, number of inserted items: %d, capacity: %d", last->idx, capacity);

  ring_buffer_free(&rb);
}

static void
test_pop_all_pushed_element_in_correct_order()
{
  RingBuffer rb;
  int cnt = 0;
  const int start_from = 1;
  TestData *td = NULL;

  _ringbuffer_init(&rb);

  _ringbuffer_fill(&rb, capacity, 1, TRUE);

  while ((td = ring_buffer_pop(&rb)))
    {
      assert_true((cnt+start_from) == td->idx, "wrong order; %d != %d", td->idx, cnt);
      ++cnt;
    }

  assert_true(cnt == capacity, "cannot read all element, %d < %d", cnt, capacity);

  ring_buffer_free(&rb);
}

static void
test_drop_elements()
{
  RingBuffer rb;
  const int rb_capacity = 103;
  const int drop = 31;

  ring_buffer_alloc(&rb, sizeof(TestData), rb_capacity);

  _ringbuffer_fill(&rb, rb_capacity, 1, TRUE);

  ring_buffer_drop(&rb, drop);
  assert_true(ring_buffer_count(&rb) == (rb_capacity - drop), "drop failed");

  ring_buffer_free(&rb);
}

static void
test_elements_ordering()
{
  RingBuffer rb;
  TestData *td;
  int start_from = 10;
  int cnt = 0;

  _ringbuffer_init(&rb);

  _ringbuffer_fill(&rb, capacity, start_from, TRUE);

  while ( (td = ring_buffer_pop(&rb)) )
    {
      assert_true((cnt + start_from) == td->idx, "wrong order; %d != %d", cnt, td->idx);
      ++cnt;
    }

  ring_buffer_free(&rb);
}

static void
test_element_at()
{
  RingBuffer rb;
  size_t i;
  TestData *td;

  _ringbuffer_init(&rb);

  _ringbuffer_fill(&rb, capacity, 0, TRUE);

  for ( i = 0; i < ring_buffer_count(&rb); i++ )
    {
      td = ring_buffer_element_at(&rb, i);
      assert_true(td != NULL, "invalid element, i=%d", i);
      assert_true(td->idx == i, "invalid order, actual=%d, expected=%d", td->idx, i);
    }

  ring_buffer_free(&rb);
}

static void
test_continual_range()
{
  RingBuffer rb;

  _ringbuffer_init(&rb);

  _ringbuffer_fill(&rb, capacity, 1, TRUE);
  assert_continual_range_length_equals(&rb, capacity);

  ring_buffer_free(&rb);
}

static void
test_zero_length_continual_range()
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

static void
test_broken_continual_range()
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


static void
test_push_after_pop()
{
}

static void
test_tail()
{
  RingBuffer rb;
  TestData *td_tail;

  ring_buffer_alloc(&rb, sizeof(TestData), 103);
  _ringbuffer_fill2(&rb, 103, 0, TRUE);

  ring_buffer_pop(&rb);

  td_tail = ring_buffer_tail(&rb);
  td_tail->idx = 103;

  assert_true(ring_buffer_push(&rb) == td_tail, "Push should return last tail.");

  assert_test_data_idx_range_in(&rb, 1, 103);

  ring_buffer_free(&rb);
}

int main(int argc, char **argv)
{
  RINGBUFFER_TESTCASE(test_init_buffer_state);
  RINGBUFFER_TESTCASE(test_pop_from_empty_buffer);
  RINGBUFFER_TESTCASE(test_push_to_full_buffer);
  RINGBUFFER_TESTCASE(test_ring_buffer_is_full);
  RINGBUFFER_TESTCASE(test_pop_all_pushed_element_in_correct_order);
  RINGBUFFER_TESTCASE(test_drop_elements);
  RINGBUFFER_TESTCASE(test_elements_ordering);
  RINGBUFFER_TESTCASE(test_element_at);
  RINGBUFFER_TESTCASE(test_continual_range);
  RINGBUFFER_TESTCASE(test_zero_length_continual_range);
  RINGBUFFER_TESTCASE(test_broken_continual_range);
  RINGBUFFER_TESTCASE(test_push_after_pop);
  RINGBUFFER_TESTCASE(test_tail);

  return 0;
}
