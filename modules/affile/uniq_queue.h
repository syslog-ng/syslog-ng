#ifndef UNIQ_QUEUE_H
#define UNIQ_QUEUE_H

#include <glib.h>

typedef struct _UniqQueue UniqQueue;

UniqQueue *uniq_queue_new();
void uniq_queue_free(UniqQueue *self);

gchar *uniq_queue_pop_head(UniqQueue *self);
void *uniq_queue_push_tail(UniqQueue *self, gchar *element);
gboolean uniq_queue_check_element(UniqQueue *self, gchar *element);
guint uniq_queue_length(UniqQueue *self);
gboolean uniq_queue_is_empty(UniqQueue *self);

#endif
