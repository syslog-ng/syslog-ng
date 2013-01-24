#include "testutils.h"
#include "control_server.h"

#include "control_server.c"


typedef struct _PositionedBuffer {
  GString *buffer;
  gint pos;
} PositionedBuffer;

typedef struct _ControlConnectionMoc
{
  ControlConnection super;
  PositionedBuffer *source_buffer;
  PositionedBuffer *destination_buffer;
  gsize transaction_size;
} ControlConnectionMoc;

ControlServer moc_server = {0};
ControlConnectionMoc *moc_connection;
GString *result_string;

void (*next_step)(gpointer s);

PositionedBuffer *
positioned_buffer_new(gsize size)
{
  PositionedBuffer *self = g_new0(PositionedBuffer,1);

  self->buffer = g_string_sized_new(size);
  return self;
}

void
positioned_buffer_free(PositionedBuffer *self)
{
  g_string_free(self->buffer, TRUE);
  g_free(self);
}

int
control_connection_moc_read(ControlConnection *s, gpointer buffer, gsize size)
{
  ControlConnectionMoc *self = (ControlConnectionMoc *)s;
  gsize bytes_to_read;

  if (self->source_buffer->pos >= self->source_buffer->buffer->len)
    {
      return 0;
    }
  if (self->transaction_size > 0)
    {
      bytes_to_read = MIN(self->transaction_size, size);
      bytes_to_read = MIN(self->source_buffer->buffer->len - self->source_buffer->pos, bytes_to_read);
    }
  else
    {
      bytes_to_read = MIN(self->source_buffer->buffer->len - self->source_buffer->pos, size);
    }
  memcpy(buffer, self->source_buffer->buffer->str + self->source_buffer->pos, bytes_to_read);
  self->source_buffer->pos += bytes_to_read;
  return bytes_to_read;
}

int
control_connection_moc_write(ControlConnection *s, gpointer buffer, gsize size)
{
  ControlConnectionMoc *self = (ControlConnectionMoc *)s;
  gsize bytes_to_write;

  if (self->transaction_size > 0)
    {
      bytes_to_write = self->transaction_size;
    }
  else
    {
      bytes_to_write = size;
    }
  g_string_append_len(self->destination_buffer->buffer, buffer, bytes_to_write);
  return bytes_to_write;
}

void
control_connection_moc_free(ControlConnection *s)
{
  ControlConnectionMoc *self = (ControlConnectionMoc *)s;

  positioned_buffer_free(self->source_buffer);
  positioned_buffer_free(self->destination_buffer);
}

ControlConnection *
control_connection_moc_new(ControlServer *server)
{
  ControlConnectionMoc *self =  g_new0(ControlConnectionMoc,1);
  control_connection_init_instance(&self->super, server);

  self->source_buffer = positioned_buffer_new(128);
  self->destination_buffer = positioned_buffer_new(128);

  self->super.read = control_connection_moc_read;
  self->super.write = control_connection_moc_write;

  self->super.free_fn = control_connection_moc_free;

  return &self->super;
}

GString *
test_command(GString *command)
{
  assert_string(command->str,"test command", "Bad command handling");
  return g_string_new("OK");
}

Commands commands[] = {
  { "test", NULL, test_command },
  { NULL, NULL, NULL },
};

void
control_connection_update_watches(ControlConnection *s)
{
  if (s->output_buffer->len > s->pos)
    {
      next_step = s->handle_output;
    }
  else
    {
      next_step = s->handle_input;
    }
}

void
control_connection_stop_watches(ControlConnection *s)
{
  ControlConnectionMoc *self = (ControlConnectionMoc *)s;
  if (result_string)
    {
      g_string_free(result_string, TRUE);
      result_string = NULL;
    }
  result_string = g_string_new(self->destination_buffer->buffer->str);
  next_step = NULL;
}

void
control_connection_start_watches(ControlConnection *s)
{
  next_step = s->handle_input;
  while(next_step)
    {
      next_step(s);
    }
}

void
test_control_connection(gsize transaction_size)
{
  moc_connection = (ControlConnectionMoc *)control_connection_moc_new(&moc_server);
  g_string_assign(moc_connection->source_buffer->buffer,"test command\n");
  moc_connection->transaction_size = transaction_size;
  control_connection_start_watches((ControlConnection *)moc_connection);
  assert_string(result_string->str, "OK\n.\n", "BAD Behaviour transaction_size: %d",transaction_size);
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  moc_server.commands = commands;
  gsize  i = 0;
  for (i = 0; i < 100; i++)
    {
      test_control_connection(i);
    }
  return 0;
}
