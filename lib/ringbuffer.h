#ifndef RINGBUFFER_H_INCLUDED
#define RINGBUFFER_H_INCLUDED

#include <glib.h>

typedef struct _RingBuffer
{
  gpointer buffer;
  guint32 head;
  guint32 tail;
  guint32 count;
  guint32 capacity;
  guint32 element_size;
} RingBuffer;

typedef gboolean(*RingBufferIsContinuousPredicate)(gpointer element);

void ring_buffer_init(RingBuffer *self);
void ring_buffer_alloc(RingBuffer *self, guint32 size_of_element, guint32 capacity);
gboolean ring_buffer_is_allocated(RingBuffer *self);
void ring_buffer_free(RingBuffer *self);
gboolean ring_buffer_is_full(RingBuffer *self);
gboolean ring_buffer_is_empty(RingBuffer *self);
gpointer ring_buffer_push(RingBuffer *self);
gpointer ring_buffer_tail(RingBuffer *self);
gpointer ring_buffer_pop(RingBuffer *self);
gboolean ring_buffer_drop(RingBuffer *self, guint32 n);
guint32 ring_buffer_capacity(RingBuffer *self);
guint32 ring_buffer_count(RingBuffer *self);
gpointer ring_buffer_element_at(RingBuffer *self, guint32 idx);
guint32 ring_buffer_get_continual_range_length(RingBuffer *self, RingBufferIsContinuousPredicate pred);

#endif /*RINGBUFFER_H_INCLUDED*/
