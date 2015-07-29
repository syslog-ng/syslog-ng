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

LogMessage *pdb_message_generate_message_without_context(PDBMessage *self, gint inherit_mode, LogMessage *msg, GString *buffer);
LogMessage *pdb_message_generate_message_with_context(PDBMessage *self, gint inherit_mode, CorrellationContext *context, GString *buffer);


void pdb_message_apply(PDBMessage *self, CorrellationContext *context, LogMessage *msg, GString *buffer);
void pdb_message_add_tag(PDBMessage *self, const gchar *text);
void pdb_message_clean(PDBMessage *self);
void pdb_message_free(PDBMessage *self);

#endif
