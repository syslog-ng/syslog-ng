#ifndef PERSIST_TOOL_H
#define PERSIST_TOOL_H 1

#include "cfg.h"
#include "persist-state.h"
#include "state.h"

#define DEFAULT_PERSIST_FILE "syslog-ng.persist"

typedef struct _PersistTool
{
  PersistState *state;
  PersistStateMode mode;
  GlobalConfig *cfg;
  gchar *persist_filename;
}PersistTool;

PersistTool *persist_tool_new(gchar *persist_filename, PersistStateMode open_mode);

void persist_tool_free(PersistTool *self);

StateHandler *persist_tool_get_state_handler(PersistTool *self, gchar *name);

void persist_tool_revert_changes(PersistTool *self);


#endif
