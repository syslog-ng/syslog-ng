#ifndef TAGS_H_INCLUDED
#define TAGS_H_INCLUDED

#include "syslog-ng.h"

guint log_tags_get_by_name(const gchar *name);
gchar *log_tags_get_by_id(guint id);

void log_tags_init(void);
void log_tags_deinit(void);

#endif
