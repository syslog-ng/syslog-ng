#include "correllation-context.h"
#include "logmsg.h"

#include <string.h>

void
correllation_context_init(CorrellationContext *self, const CorrellationKey *key)
{
  self->messages = g_ptr_array_new();
  memcpy(&self->key, key, sizeof(self->key));

  if (self->key.pid)
    self->key.pid = g_strdup(self->key.pid);
  if (self->key.program)
    self->key.program = g_strdup(self->key.program);
  if (self->key.host)
    self->key.host = g_strdup(self->key.host);
  self->ref_cnt = 1;
  self->free_fn = correllation_context_free_method;
}

void
correllation_context_free_method(CorrellationContext *self)
{
  gint i;

  for (i = 0; i < self->messages->len; i++)
    {
      log_msg_unref((LogMessage *) g_ptr_array_index(self->messages, i));
    }
  g_ptr_array_free(self->messages, TRUE);

  if (self->key.host)
    g_free((gchar *) self->key.host);
  if (self->key.program)
    g_free((gchar *) self->key.program);
  if (self->key.pid)
    g_free((gchar *) self->key.pid);
  g_free(self->key.session_id);
}

CorrellationContext *
correllation_context_ref(CorrellationContext *self)
{
  self->ref_cnt++;
  return self;
}

void
correllation_context_unref(CorrellationContext *self)
{
  if (--self->ref_cnt == 0)
    {
      if (self->free_fn)
        self->free_fn(self);
      g_free(self);
    }
}
