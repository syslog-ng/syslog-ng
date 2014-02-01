#ifndef PERSIST_TOOL_H
#define PERSIST_TOOL_H 1

#include "cfg.h"
#include "persist-state.h"
#include "persistable-state-presenter.h"

#define DEFAULT_PERSIST_FILE "syslog-ng.persist"

typedef enum _PersistStateMode {
  persist_mode_normal = 0,
  persist_mode_dump,
  persist_mode_edit
} PersistStateMode;


typedef struct _PersistTool
{
  PersistState *state;
  PersistStateMode mode;
  GlobalConfig *cfg;
  gchar *persist_filename;
}PersistTool;

PersistTool *persist_tool_new(gchar *persist_filename, PersistStateMode open_mode);

void persist_tool_free(PersistTool *self);

PersistableStatePresenter *persist_tool_get_state_presenter(PersistTool *self, gchar *name);

void persist_tool_revert_changes(PersistTool *self);


#endif
