#include "libtest_memreader.h"
#include "persist-state.h"

gssize
log_transport_memory_read_method(LogTransport *s, gpointer buf, gsize count, GSockAddr **sa)
{
  LogTransportMemory *self = (LogTransportMemory *) s;
  gssize result = 0;
  if (self->read_buffer)
    {
      result = count < strlen((self->read_buffer + self->read_buffer_pos)) ? count : strlen((self->read_buffer + self->read_buffer_pos));
      memcpy(buf, self->read_buffer + self->read_buffer_pos, result);
      self->read_buffer_pos += result;
    }
  return result;
}

gssize
log_transport_memory_write_method(LogTransport *s, const gpointer buf, gsize count)
{
  //LogTransportMemory *self = (LogTransportMemory *) s;
  return 0;
}

LogTransport *log_transport_memory_new(gchar *read_buffer, guint32 read_buffer_length, gchar *write_buffer, guint32 write_buffer_length, guint flags)
{
  LogTransportMemory *self = g_new0(LogTransportMemory, 1);

  self->super.fd = 0;
  self->super.cond = 0;
  self->super.flags = flags;
  self->super.read = log_transport_memory_read_method;
  self->super.write = log_transport_memory_write_method;
  self->super.free_fn = log_transport_free_method;

  self->read_buffer = read_buffer;
  self->read_buffer_length = read_buffer_length;
  self->read_buffer_pos = 0;

  self->write_buffer = write_buffer;
  self->write_buffer_length = write_buffer_length;
  self->write_buffer_pos = 0;
  return &self->super;
}

LogReader *log_reader_new_memory_source(LogReaderOptions *options, guint32 read_buffer_length, LogReaderNotifyMethod notif, LogReaderQueueMethod queue, LogTransport **new_transport)
{
  gchar *read_buffer = g_malloc0(read_buffer_length);
  *new_transport = log_transport_memory_new(read_buffer, read_buffer_length, NULL, 0, 0);
  LogProto *proto = log_proto_text_server_new(*new_transport, 8192, LPBS_IGNORE_EOF);
  LogReader *reader = (LogReader*)log_reader_new(proto);
  log_reader_set_options((LogPipe *)reader, (LogPipe *)reader, options, 0, SCS_FILE, "test","test_mem_queue");
  ((LogPipe *)reader)->queue = queue;
  ((LogPipe *)reader)->notify = notif;
  log_pipe_init((LogPipe *)reader, NULL);
  return reader;
}

LogReader *log_reader_new_file_source(LogReaderOptions *options, guint32 read_buffer_length, LogReaderNotifyMethod notif, LogReaderQueueMethod queue, LogTransport **new_transport)
{
  gchar *read_buffer = g_malloc0(read_buffer_length);
  *new_transport = log_transport_memory_new(read_buffer, read_buffer_length, NULL, 0, 0);
  LogProto *proto = log_proto_text_server_new(*new_transport, 8192, LPBS_NOMREAD);
  PersistState *state = persist_state_new("test.persist");
  persist_state_start(state);
  log_proto_restart_with_state(proto,state,"test_state");
  LogReader *reader = (LogReader*)log_reader_new(proto);
  log_reader_set_options((LogPipe *)reader, (LogPipe *)reader, options, 0, SCS_FILE, "test","test_file_queue");
  ((LogPipe *)reader)->queue = queue;
  ((LogPipe *)reader)->notify = notif;
  log_pipe_init((LogPipe *)reader, NULL);
  reader->size = 1;
  return reader;
}

void log_test_reader_add_message(LogTransport *tr, const gchar *msg, guint32 msg_size)
{
  LogTransportMemory *transport = (LogTransportMemory *)tr;
  memcpy(transport->read_buffer + strlen(transport->read_buffer), msg, msg_size);
}
