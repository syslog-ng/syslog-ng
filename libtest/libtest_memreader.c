#include "libtest_memreader.h"
#include "persist-state.h"
#include "plugin.h"

#include "logreader.c"

LogProto *
create_logproto(gchar *name,LogTransport *transport,LogProtoOptions *options)
{
  GlobalConfig *configuration;
  LogProto *proto = NULL;
  LogProtoFactory *factory;
  configuration = cfg_new(0x0302);
  plugin_load_module("basic-proto", configuration, NULL);
  factory = log_proto_get_factory(configuration, LPT_SERVER, name);
  if (factory)
    {
      proto = factory->create(transport,options,configuration);
    }
  return proto;
}

LogTransportMemoryBuffer *
log_transport_memory_buffer_new(gchar *data, guint32 length)
{
  LogTransportMemoryBuffer *buffer = g_new0(LogTransportMemoryBuffer, 1);
  buffer->data = data;
  buffer->length = length;
  buffer->pos = 0;
  buffer->count = 0;

  return buffer;
}

void
log_transport_memory_buffer_free(LogTransportMemoryBuffer *buffer, gboolean free_raw_data)
{
  if (free_raw_data)
    g_free(buffer->data);

  g_free(buffer);
}

gssize
log_transport_memory_buffer_read(LogTransportMemoryBuffer *buffer, gpointer buf, gsize count)
{
  gsize count_transferred = MIN(buffer->count, count);
  gsize count_transferred_step1 = 0;
  gsize count_transferred_step2 = 0;

  g_assert(buffer->pos < buffer->length);
  g_assert(buffer->count <= buffer->length);

  if (buffer->pos + count_transferred >= buffer->length)
    {
      count_transferred_step1 = MIN(buffer->length - buffer->pos, count_transferred);
      count_transferred_step2 = count_transferred - count_transferred_step1;
    }
  else
    {
      count_transferred_step1 = count_transferred;
      count_transferred_step2 = 0;
    }

  if (count_transferred_step1 > 0)
    {
      memcpy(buf, buffer->data + buffer->pos, count_transferred_step1);
      buffer->pos = (buffer->pos + count_transferred_step1) % buffer->length;
      buffer->count -= count_transferred_step1;

      g_assert(buffer->count >= 0);
    }

  if (count_transferred_step2 > 0)
    {
      g_assert(buffer->pos == 0);

      memcpy(buf + count_transferred_step1, buffer->data + buffer->pos, count_transferred_step2);
      buffer->pos = (buffer->pos + count_transferred_step2) % buffer->length;
      buffer->count -= count_transferred_step2;

      g_assert(buffer->count >= 0);
    }

  return count_transferred;
}

gssize
log_transport_memory_buffer_write(LogTransportMemoryBuffer *buffer, const gchar *buf, gsize count)
{
  gsize count_transferred = MIN(buffer->length - buffer->count, count);
  gsize count_transferred_step1 = 0;
  gsize count_transferred_step2 = 0;
  gsize startpos = (buffer->pos + buffer->count) % buffer->length;

  g_assert(buffer->pos < buffer->length);
  g_assert(buffer->count <= buffer->length);

  if (startpos + count_transferred >= buffer->length)
    {
      count_transferred_step1 = MIN(buffer->length - startpos, count_transferred);
      count_transferred_step2 = count_transferred - count_transferred_step1;
    }
  else
    {
      count_transferred_step1 = count_transferred;
      count_transferred_step2 = 0;
    }

  if (count_transferred_step1 > 0)
    {
      memcpy(buffer->data + startpos, buf, count_transferred_step1);
      startpos = (startpos + count_transferred_step1) % buffer->length;
      buffer->count += count_transferred_step1;

      g_assert(buffer->count <= buffer->length);
    }

  if (count_transferred_step2 > 0)
    {
      g_assert(startpos == 0);

      memcpy(buffer->data + startpos, buf + count_transferred_step1, count_transferred_step2);
      startpos = (startpos + count_transferred_step2) % buffer->length;
      buffer->count += count_transferred_step2;

      g_assert(buffer->count <= buffer->length);
    }

  return count_transferred;
}

void
log_transport_memory_buffer_test_write(LogTransportMemoryBuffer *buffer, gchar *buf)
{
  char buf20[20];
  snprintf((char *)buf20, sizeof(buf20), "%s", buf);
  fprintf(stderr, "WRITE : %s\n", buf20);
  log_transport_memory_buffer_write(buffer, buf20, MIN(sizeof(buf20), strlen(buf20)) );
}

gboolean
log_transport_memory_buffer_test_read(LogTransportMemoryBuffer *buffer, gchar *buf, guint max_read)
{
  char buf20[20];
  memset(buf20, 0x0, sizeof(buf20));

  log_transport_memory_buffer_read(buffer, buf20, MIN(sizeof(buf20), max_read));
  fprintf(stderr, "READ  : %s\n", buf20);
  fprintf(stderr, "EXPECT: %s\n", buf);
  fprintf(stderr, "-------\n");
  return (strcmp(buf20, buf) == 0);
}

gboolean
log_transport_memory_buffer_test()
{
  gchar data[10];
  gboolean res = TRUE;

  LogTransportMemoryBuffer *buffer = log_transport_memory_buffer_new(data, sizeof(data));
  log_transport_memory_buffer_test_write(buffer, "AAA");
  res &= log_transport_memory_buffer_test_read(buffer, "AAA", 20);
  log_transport_memory_buffer_test_write(buffer, "BBBBB");
  res &= log_transport_memory_buffer_test_read(buffer, "BBBBB", 20);
  log_transport_memory_buffer_test_write(buffer, "CCCCC");
  res &= log_transport_memory_buffer_test_read(buffer, "CCCCC", 20);
  log_transport_memory_buffer_test_write(buffer, "DDDDD");
  res &= log_transport_memory_buffer_test_read(buffer, "DDDDD", 20);
  log_transport_memory_buffer_test_write(buffer, "EE");
  res &= log_transport_memory_buffer_test_read(buffer, "EE", 20);
  log_transport_memory_buffer_test_write(buffer, "FFFFFFFFFF");
  res &= log_transport_memory_buffer_test_read(buffer, "FFFFFFFFFF", 20);
  log_transport_memory_buffer_test_write(buffer, "GGG");
  log_transport_memory_buffer_test_write(buffer, "GGG");
  log_transport_memory_buffer_test_write(buffer, "H");
  res &= log_transport_memory_buffer_test_read(buffer, "GGGGGGH", 20);
  log_transport_memory_buffer_test_write(buffer, "II");
  log_transport_memory_buffer_test_write(buffer, "II");
  res &= log_transport_memory_buffer_test_read(buffer, "IIII", 20);
  log_transport_memory_buffer_test_write(buffer, "FFFFFFFFFF");
  res &= log_transport_memory_buffer_test_read(buffer, "FFFFFFFFFF", 20);
  log_transport_memory_buffer_test_write(buffer, "FFFFFFFFFFF");
  res &= log_transport_memory_buffer_test_read(buffer, "FFFFFFFFFF", 20);
  log_transport_memory_buffer_test_write(buffer, "FFFFFF");
  log_transport_memory_buffer_test_write(buffer, "FFFFF");
  res &= log_transport_memory_buffer_test_read(buffer, "FFFFFFFFFF", 20);

  log_transport_memory_buffer_free(buffer, FALSE);
  return res;
}

gssize
log_transport_memory_read_method(LogTransport *s, gpointer buf, gsize count, GSockAddr **sa)
{
  LogTransportMemory *self = (LogTransportMemory *) s;
  if (self->read_buffer)
    {
      gsize bytes = 0;

      if (self->func_try_read)
        self->func_try_read(s);

      bytes = log_transport_memory_buffer_read(self->read_buffer, buf, MIN(count, self->max_read));

      if (bytes > 0)
        {
          if (self->func_read)
            self->func_read(s);
          return bytes;
        }
      else
        {
          errno = EAGAIN;
          return -1;
        }
    }

  return 0;
}

gssize
log_transport_memory_write_method(LogTransport *s, const gpointer buf, gsize count)
{
  LogTransportMemory *self = (LogTransportMemory *) s;
  if (self->write_buffer)
    {
      gsize bytes = 0;

      if (self->func_try_write)
        self->func_try_write(s);

      bytes = log_transport_memory_buffer_write(self->write_buffer, buf, MIN(count, self->max_write));

      if (bytes > 0)
        {
          if( self->func_write)
            self->func_write(s);
          return bytes;
        }
      else
        {
          errno = EAGAIN;
          return -1;
        }
    }

  return 0;
}

void
log_transport_memory_free_method(LogTransport *s)
{
  LogTransportMemory *self = (LogTransportMemory *)s;
  if (self->read_buffer)
    log_transport_memory_buffer_free(self->read_buffer, TRUE);
  if (self->write_buffer)
    log_transport_memory_buffer_free(self->write_buffer, TRUE);
}

LogTransport *
log_transport_memory_new(LogTransportMemoryBuffer *read_buffer, LogTransportMemoryBuffer *write_buffer, guint flags)
{
  LogTransportMemory *self = g_new0(LogTransportMemory, 1);

  self->super.fd = 0;
  self->super.cond = 0;
  self->super.flags = flags;
  self->super.read = log_transport_memory_read_method;
  self->super.write = log_transport_memory_write_method;
  self->super.free_fn = log_transport_memory_free_method;
  self->read_buffer = read_buffer;
  self->write_buffer = write_buffer;
  self->max_read = (read_buffer ? read_buffer->length : 0);
  self->max_write = (write_buffer ? write_buffer->length : 0);
  self->name = "TEST";
  return &self->super;
}

LogReader *
log_reader_new_memory_source(LogReaderOptions *options, guint32 read_buffer_length, LogReaderNotifyMethod notif, LogReaderQueueMethod queue, LogTransport **new_transport, const gchar *prefix, const gchar *garbage, GlobalConfig *cfg)
{
  gchar *buf = g_malloc0(read_buffer_length);
  LogTransportMemoryBuffer *read_buffer = log_transport_memory_buffer_new(buf, read_buffer_length);
  LogProto *proto = NULL;
  *new_transport = log_transport_memory_new(read_buffer, NULL, 0);
  LogProtoServerOptions *proto_options = g_malloc0(sizeof(LogProtoServerOptions));

  if (prefix)
    {
      const gchar *error;
      gint erroroffset;
      /*Are we need any options?*/
      int pcreoptions = PCRE_EXTENDED;
      proto_options->opts.prefix_matcher = pcre_compile(prefix, pcreoptions, &error, &erroroffset, NULL);
      if (!proto_options->opts.prefix_matcher)
        {
          fprintf(stderr,"Bad regexp %s\n", prefix);
          return FALSE;
        }
      if (garbage)
        {
          proto_options->opts.garbage_matcher = pcre_compile(garbage, pcreoptions, &error, &erroroffset, NULL);
          if (!proto_options->opts.garbage_matcher)
            {
              fprintf(stderr,"Bad regexp %s\n", garbage);
              return FALSE;
            }
        }
    }
  proto_options->super.size = 8192;
  proto_options->super.flags = LPBS_IGNORE_EOF;
  proto = create_logproto("stream-newline",*new_transport,(LogProtoOptions *)proto_options);
  LogReader *reader = (LogReader*)log_reader_new(proto);
  log_reader_set_options((LogPipe *)reader, (LogPipe *)reader, options, 0, SCS_FILE, "test","test_mem_queue", (LogProtoOptions *) proto_options);
  ((LogPipe *)reader)->queue = queue;
  ((LogPipe *)reader)->notify = notif;
  log_pipe_init((LogPipe *)reader, cfg);
  return reader;
}

LogReader *
log_reader_new_file_source(LogReaderOptions *options, guint32 read_buffer_length, LogReaderNotifyMethod notif, LogReaderQueueMethod queue, LogTransport **new_transport, GlobalConfig *cfg)
{
  LogProtoOptions *proto_options = g_malloc0(sizeof(LogProtoOptions));
  gchar *buf = g_malloc0(read_buffer_length);
  LogTransportMemoryBuffer *read_buffer = log_transport_memory_buffer_new(buf, read_buffer_length);
  *new_transport = log_transport_memory_new(read_buffer, NULL, 0);
  memset(proto_options,0,sizeof(LogProtoOptions));
  proto_options->super.size = 8192;
  proto_options->super.flags = LPBS_NOMREAD;
  LogProto *proto = create_logproto("stream-newline",*new_transport,proto_options);
  PersistState *state = persist_state_new("test.persist");
  persist_state_start(state);
  log_proto_restart_with_state(proto,state,"test_state");
  LogReader *reader = (LogReader*)log_reader_new(proto);
  log_reader_set_options((LogPipe *)reader, (LogPipe *)reader, options, 0, SCS_FILE, "test","test_file_queue", proto_options);
  ((LogPipe *)reader)->queue = queue;
  ((LogPipe *)reader)->notify = notif;
  log_pipe_init((LogPipe *)reader, cfg);
  reader->size = 1;
  return reader;
}

void
log_test_reader_add_message(LogTransport *tr, const gchar *msg, guint32 msg_size)
{
  LogTransportMemory *transport = (LogTransportMemory *)tr;
  log_transport_memory_buffer_write(transport->read_buffer, msg, msg_size);
}
