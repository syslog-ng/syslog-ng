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
  gchar *data;
  guint32 length;
  guint32 pos;
  guint32 count;
} LogTransportMemoryBuffer;

typedef struct {
  LogTransport super;
  LogTransportMemoryBuffer *read_buffer;
  LogTransportMemoryBuffer *write_buffer;
  guchar *name;
  gsize max_read;
  gsize max_write;
  void (*func_read)(void *);
  void (*func_write)(void *);
  void (*func_try_read)(void *);
  void (*func_try_write)(void *);
} LogTransportMemory;

typedef void (*LogReaderNotifyMethod)(LogPipe *s, LogPipe *sender, gint notify_code, gpointer user_data);
typedef void (*LogReaderQueueMethod)(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options, gpointer user_data);

LogTransportMemoryBuffer *log_transport_memory_buffer_new(gchar *data, guint32 length);
void log_transport_memory_buffer_free(LogTransportMemoryBuffer *buffer, gboolean free_raw_data);
gssize log_transport_memory_buffer_read(LogTransportMemoryBuffer *buffer, gpointer buf, gsize count);
gssize log_transport_memory_buffer_write(LogTransportMemoryBuffer *buffer, gpointer buf, gsize count);

gboolean log_transport_memory_buffer_test();

LogTransport *log_transport_memory_new(LogTransportMemoryBuffer *read_buffer, LogTransportMemoryBuffer *write_buffer, guint flags);

LogReader *log_reader_new_memory_source(LogReaderOptions *options, guint32 read_buffer_length, LogReaderNotifyMethod notif, LogReaderQueueMethod queue, LogTransport **new_transport, const gchar *prefix, const gchar *garbage, GlobalConfig *cfg);
LogReader *log_reader_new_file_source(LogReaderOptions *options, guint32 read_buffer_length, LogReaderNotifyMethod notif, LogReaderQueueMethod queue, LogTransport **new_transport, GlobalConfig *cfg);

void log_test_reader_add_message(LogTransport *transport, const gchar *msg, guint32 msg_size);

#endif
