#ifndef PATTERNDB_SYNTHETIC_MESSAGE_H_INCLUDED
#define PATTERNDB_SYNTHETIC_MESSAGE_H_INCLUDED

#include "syslog-ng.h"
#include "correllation-context.h"

typedef enum
{
  RAC_MSG_INHERIT_NONE,
  RAC_MSG_INHERIT_LAST_MESSAGE,
  RAC_MSG_INHERIT_CONTEXT
} PDBActionMessageInheritMode;

typedef struct _PDBMessage
{
  GArray *tags;
  GPtrArray *values;
} PDBMessage;

LogMessage *synthetic_message_generate_without_context(PDBMessage *self, gint inherit_mode, LogMessage *msg, GString *buffer);
LogMessage *synthetic_message_generate_with_context(PDBMessage *self, gint inherit_mode, CorrellationContext *context, GString *buffer);


void synthetic_message_apply(PDBMessage *self, CorrellationContext *context, LogMessage *msg, GString *buffer);
gboolean synthetic_message_add_value_template(PDBMessage *self, GlobalConfig *cfg, const gchar *name, const gchar *value, GError **error);
void synthetic_message_add_tag(PDBMessage *self, const gchar *text);
void synthetic_message_deinit(PDBMessage *self);
void synthetic_message_free(PDBMessage *self);

#endif
