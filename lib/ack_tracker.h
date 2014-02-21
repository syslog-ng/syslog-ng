#ifndef ACK_TRACKER_H_INCLUDED
#define ACK_TRACKER_H_INCLUDED

#include "syslog-ng.h"
#include "logsource.h"

struct _AckTracker
{
  LogSource *source;
  Bookmark* (*request_bookmark)(AckTracker *self);
  void (*track_msg)(AckTracker *self, LogMessage *msg);
  void (*manage_msg_ack)(AckTracker *self, LogMessage *msg, gboolean ack_type); // TODO: AckType
};

struct _AckRecord
{
  AckTracker *tracker;
};

AckTracker* ack_tracker_new(LogSource *source);
void ack_tracker_free(AckTracker *);

static inline Bookmark *
ack_tracker_request_bookmark(AckTracker *self)
{
  return self->request_bookmark(self);
}

static inline void
ack_tracker_track_msg(AckTracker *self, LogMessage *msg)
{
  self->track_msg(self, msg);
}

static inline void
ack_tracker_manage_msg_ack(AckTracker *self, LogMessage *msg, gboolean ack_type)
{
  self->manage_msg_ack(self, msg, ack_type);
}

#endif
