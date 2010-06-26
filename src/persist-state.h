#ifndef PERSIST_STATE_H_INCLUDED
#define PERSIST_STATE_H_INCLUDED

#include "syslog-ng.h"

typedef struct _PersistState PersistState;
typedef guint32 PersistEntryHandle;

gpointer persist_state_map_entry(PersistState *self, PersistEntryHandle handle);
void persist_state_unmap_entry(PersistState *self, PersistEntryHandle handle);

PersistEntryHandle persist_state_alloc_entry(PersistState *self, const gchar *persist_name, gsize alloc_size);
PersistEntryHandle persist_state_lookup_entry(PersistState *self, const gchar *persist_name, gsize *size, guint8 *version);

gchar *persist_state_lookup_string(PersistState *self, const gchar *key, gsize *length, guint8 *version);
void persist_state_alloc_string(PersistState *self, const gchar *persist_name, const gchar *value, gssize len);

void persist_state_free_entry(PersistEntryHandle handle);

gboolean persist_state_start(PersistState *self);
gboolean persist_state_commit(PersistState *self);
void persist_state_cancel(PersistState *self);

PersistState *persist_state_new(const gchar *filename);
void persist_state_free(PersistState *self);

#endif
