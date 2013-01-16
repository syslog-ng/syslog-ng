#ifndef LIBTEST_MEMREADER_H

#define LIBTEST_MEMREADER_H 1
#include "syslog-ng.h"
#include "logmsg.h"
#include "serialize.h"
#include "apphook.h"
#include "gsockaddr.h"
#include "logpipe.h"
#include "logtransport.h"
#include "logproto.h"
#include "logreader.h"
#include "mainloop.h"

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <iv_event.h>

typedef struct {
  LogTransport super;
  gchar *read_buffer;
  guint32 read_buffer_length;
  guint32 read_buffer_pos;
  gchar *write_buffer;
  guint32 write_buffer_length;
  guint32 write_buffer_pos;
} LogTransportMemory;

struct _LogReader
{
  LogSource super;
  LogProto *proto;
  LogReaderWatch *source;
  gboolean immediate_check;
  gboolean waiting_for_preemption;
  LogPipe *control;
  LogReaderOptions *options;
  GSockAddr *peer_addr;
  gchar *follow_filename;
  ino_t inode;
  gint64 size;

  /* NOTE: these used to be LogReaderWatch members, which were merged into
   * LogReader with the multi-thread refactorization */

  struct iv_fd fd_watch;
  struct iv_timer follow_timer;
  struct iv_task restart_task;
  struct iv_task immediate_check_task;
  struct iv_event schedule_wakeup;
  MainLoopIOWorkerJob io_job;
  gboolean suspended:1;
  gint pollable_state;
  gint notify_code;
  regex_t *prefix_matcher;
  regex_t *garbage_matcher;
  /* Because of multiline processing logreader has to store the last read line which should start with the prefix */
  gchar *partial_message;
  /* store if we have to wait for a prefix, because last event was a garbage found */
  gboolean wait_for_prefix;
  gboolean flush;
  time_t last_msg_received;

  gboolean pending_proto_present;
  GCond *pending_proto_cond;
  GStaticMutex pending_proto_lock;
  LogProto *pending_proto;
};

typedef void (*LogReaderNotifyMethod)(LogPipe *s, LogPipe *sender, gint notify_code, gpointer user_data);
typedef void (*LogReaderQueueMethod)(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options, gpointer user_data);

LogTransport *log_transport_memory_new(gchar *read_buffer, guint32 read_buffer_length, gchar *write_buffer, guint32 write_buffer_length, guint flags);
LogReader *log_reader_new_memory_source(LogReaderOptions *options, guint32 read_buffer_length, LogReaderNotifyMethod notif, LogReaderQueueMethod queue, LogTransport **new_transport, const gchar *prefix, const gchar *garbage, GlobalConfig *cfg);
LogReader *log_reader_new_file_source(LogReaderOptions *options, guint32 read_buffer_length, LogReaderNotifyMethod notif, LogReaderQueueMethod queue, LogTransport **new_transport, GlobalConfig *cfg);
void log_test_reader_add_message(LogTransport *transport, const gchar *msg, guint32 msg_size);

void log_reader_read_log(LogReader *self);
#endif
