#include "ack_tracker.h"

#include "logmsg.h"
#include "bookmark.h"
#include "ringbuffer.h"

#include <glib.h>

typedef struct _UnreliableAckRecord
{
  AckRecord super;
  void* padding;
  /*bookmark contains a binary container which has to be aligned*/
  Bookmark bookmark;
} UnreliableAckRecord;

typedef struct _ReliableAckRecord
{
  AckRecord super;
  gboolean acked;
  Bookmark bookmark;
} ReliableAckRecord;

typedef struct _UnreliableAckTracker
{
  AckTracker super;

  UnreliableAckRecord ack_record_storage;
} UnreliableAckTracker;

typedef struct ReliableAckTracker
{
  AckTracker super;
  ReliableAckRecord *pending_ack_record;
  RingBuffer ack_record_storage;
  GStaticMutex storage_mutex;
} ReliableAckTracker;

static AckTracker* unreliable_ack_tracker_new(LogSource *source);
static AckTracker* reliable_ack_tracker_new(LogSource *source);

static void unreliable_ack_tracker_free(AckTracker *s);
static void reliable_ack_tracker_free(AckTracker *s);

static void unreliable_ack_tracker_init_instance(UnreliableAckTracker *self, LogSource *source);
static void reliable_ack_tracker_init_instance(ReliableAckTracker *self, LogSource *source);

static void unreliable_ack_tracker_track_msg(AckTracker *s, LogMessage *msg);
static void unreliable_ack_tracker_manage_msg_ack(AckTracker *s, LogMessage *msg, gboolean acked);

static void reliable_ack_tracker_track_msg(AckTracker *s, LogMessage *msg);
static void reliable_ack_tracker_manage_msg_ack(AckTracker *s, LogMessage *msg, gboolean acked);

static Bookmark *reliable_ack_tracker_request_bookmark(AckTracker *s);
static Bookmark *unreliable_ack_tracker_request_bookmark(AckTracker *s);

static inline void
reliable_ack_record_destroy(ReliableAckRecord *self)
{
  if (self->bookmark.destroy)
    self->bookmark.destroy(&(self->bookmark));
}

static inline void
_reliable_tracker_lock(ReliableAckTracker *self)
{
  g_static_mutex_lock(&self->storage_mutex);
}

static inline void
_reliable_tracker_unlock(ReliableAckTracker *self)
{
  g_static_mutex_unlock(&self->storage_mutex);
}

static inline gboolean
_ack_range_is_continuous(void *data)
{
  ReliableAckRecord *ack_rec = (ReliableAckRecord *)data;

  return ack_rec->acked;
}

static inline guint32
_get_continuous_range_length(ReliableAckTracker *self)
{
  return ring_buffer_get_continual_range_length(&self->ack_record_storage, _ack_range_is_continuous);
}

static inline void
_drop_range(ReliableAckTracker *self, guint32 n)
{
  int i;
  ReliableAckRecord *ack_rec;

  for (i = 0; i < n; i++)
    {
      ack_rec = ring_buffer_element_at(&self->ack_record_storage, i);
      ack_rec->acked = FALSE;

      reliable_ack_record_destroy(ack_rec);

      // TODO: bookmark_deinit/destroy; ack_record_deinit/destroy;
      ack_rec->bookmark.save = NULL;
      ack_rec->bookmark.destroy = NULL;
    }
  ring_buffer_drop(&self->ack_record_storage, n);
}

AckTracker*
ack_tracker_new(LogSource *source)
{
  // attenni logsource-ba -> dontson o, milyen kell
  AckTracker *self;
  g_assert(source->options);
//->log_source_is_reliable???
  if (source->pos_tracking)
    {
      self = (AckTracker *)reliable_ack_tracker_new(source);
    }
  else
    {
      self = (AckTracker *)unreliable_ack_tracker_new(source);
    }

  return self;
}

void
ack_tracker_free(AckTracker *self)
{
  if (!self)
    return;

  g_assert(self->source);
  g_assert(self->source->options);

  if (self->source->pos_tracking)
    {
      reliable_ack_tracker_free(self);
    }
  else
    {
      unreliable_ack_tracker_free(self);
    }
}

static void
unreliable_ack_tracker_free(AckTracker *s)
{
  UnreliableAckTracker *self = (UnreliableAckTracker *)s;

  g_free(self);
}

static void
reliable_ack_tracker_free(AckTracker *s)
{
  ReliableAckTracker *self = (ReliableAckTracker *)s;
  guint32 count = ring_buffer_count(&self->ack_record_storage);

  _drop_range(self, count);

  ring_buffer_free(&self->ack_record_storage);
  g_free(self);
}

static AckTracker*
unreliable_ack_tracker_new(LogSource *source)
{
  UnreliableAckTracker *self = (UnreliableAckTracker *)g_new0(UnreliableAckTracker, 1);

  unreliable_ack_tracker_init_instance(self, source);

  return (AckTracker *)self;
}

static AckTracker*
reliable_ack_tracker_new(LogSource *source)
{
  ReliableAckTracker *self = g_new0(ReliableAckTracker, 1);

  reliable_ack_tracker_init_instance(self, source);

  return (AckTracker *)self;
}

static void
unreliable_ack_tracker_init_instance(UnreliableAckTracker *self, LogSource *source)
{
  self->super.source = source;
  source->ack_tracker = (AckTracker *)self;
  self->super.request_bookmark = unreliable_ack_tracker_request_bookmark;
  self->super.track_msg = unreliable_ack_tracker_track_msg;
  self->super.manage_msg_ack = unreliable_ack_tracker_manage_msg_ack;
  self->ack_record_storage.super.tracker = (AckTracker *)self;
}

static Bookmark *
unreliable_ack_tracker_request_bookmark(AckTracker *s)
{
  UnreliableAckTracker *self = (UnreliableAckTracker *)s;
  return &(self->ack_record_storage.bookmark);
}

static void
unreliable_ack_tracker_track_msg(AckTracker *s, LogMessage *msg)
{
  UnreliableAckTracker *self = (UnreliableAckTracker *)s;
  log_pipe_ref((LogPipe *)self->super.source);
  msg->ack_record = (AckRecord *)(&self->ack_record_storage);
}

static void
unreliable_ack_tracker_manage_msg_ack(AckTracker *s, LogMessage *msg, gboolean ack_type)
{
  UnreliableAckTracker *self = (UnreliableAckTracker *)s;

  log_source_finalize_ack(self->super.source, 1);
  log_msg_unref(msg);
  log_pipe_unref((LogPipe *)self->super.source);
}

static void
reliable_ack_tracker_init_instance(ReliableAckTracker *self, LogSource *source)
{
  self->super.source = source;
  source->ack_tracker = (AckTracker *)self;
  self->super.request_bookmark = reliable_ack_tracker_request_bookmark;
  self->super.track_msg = reliable_ack_tracker_track_msg;
  self->super.manage_msg_ack = reliable_ack_tracker_manage_msg_ack;
  ring_buffer_alloc(&self->ack_record_storage, sizeof(ReliableAckRecord), log_source_get_init_window_size(source));
  g_static_mutex_init(&self->storage_mutex);
}

static Bookmark *
reliable_ack_tracker_request_bookmark(AckTracker *s)
{
  ReliableAckTracker *self = (ReliableAckTracker *)s;

  _reliable_tracker_lock(self);
    {
      self->pending_ack_record = ring_buffer_tail(&self->ack_record_storage);
    }
  _reliable_tracker_unlock(self);

  if (self->pending_ack_record)
    {
      self->pending_ack_record->bookmark.persist_state = s->source->super.cfg->state;

      self->pending_ack_record->super.tracker = (AckTracker *)self;

      return &(self->pending_ack_record->bookmark);
    }

  return NULL;
}

static void
reliable_ack_tracker_track_msg(AckTracker *s, LogMessage *msg)
{
  ReliableAckTracker *self = (ReliableAckTracker *)s;
  LogSource *source = self->super.source;

  g_assert(self->pending_ack_record != NULL);

  log_pipe_ref((LogPipe *)source);

  msg->ack_record = (AckRecord *)self->pending_ack_record;

  _reliable_tracker_lock(self);
    {
      ReliableAckRecord *ack_rec;
      ack_rec = (ReliableAckRecord *)ring_buffer_push(&self->ack_record_storage);
      g_assert(ack_rec == self->pending_ack_record);
    }
  _reliable_tracker_unlock(self);

  self->pending_ack_record = NULL;
}

static void
reliable_ack_tracker_manage_msg_ack(AckTracker *s, LogMessage *msg, gboolean acked)
{
  ReliableAckTracker *self = (ReliableAckTracker *)s;
  ReliableAckRecord *ack_rec = (ReliableAckRecord *)msg->ack_record;
  ReliableAckRecord *last_in_range = NULL;
  guint32 ack_range_length = 0;

  ack_rec->acked = TRUE;

  _reliable_tracker_lock(self);
    {
      ack_range_length = _get_continuous_range_length(self);
      if (ack_range_length > 0)
        {
          last_in_range = ring_buffer_element_at(&self->ack_record_storage, ack_range_length - 1);
          if (acked)
            {
              Bookmark *bookmark = &(last_in_range->bookmark);
              bookmark->save(bookmark);
            }
          _drop_range(self, ack_range_length);
          log_source_finalize_ack(self->super.source, ack_range_length);
        }
    }
  _reliable_tracker_unlock(self);

  log_msg_unref(msg);
  log_pipe_unref((LogPipe *)self->super.source);
}
