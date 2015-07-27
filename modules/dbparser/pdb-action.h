#ifndef DBPARSER_PDB_ACTION_H_INCLUDED
#define DBPARSER_PDB_ACTION_H_INCLUDED

#include "syslog-ng.h"
#include "synthetic-message.h"
#include "filter/filter-expr.h"

/* rule action triggers */
typedef enum
 {
  RAT_MATCH = 1,
  RAT_TIMEOUT
} PDBActionTrigger;

/* action content*/
typedef enum
{
  RAC_NONE,
  RAC_MESSAGE
} PDBActionContentType;

/* a rule may contain one or more actions to be performed */
typedef struct _PDBAction
{
  FilterExprNode *condition;
  PDBActionTrigger trigger;
  PDBActionContentType content_type;
  guint32 rate_quantum;
  guint16 rate;
  guint8 id;
  union
  {
    struct {
      PDBMessage message;
      PDBActionMessageInheritMode inherit_mode;
    };
  } content;
} PDBAction;

void pdb_action_set_condition(PDBAction *self, GlobalConfig *cfg, const gchar *filter_string, GError **error);
void pdb_action_set_rate(PDBAction *self, const gchar *rate_);
void pdb_action_set_trigger(PDBAction *self, const gchar *trigger, GError **error);
void pdb_action_set_message_inheritance(PDBAction *self, const gchar *inherit_properties, GError **error);

PDBAction *pdb_action_new(gint id);
void pdb_action_free(PDBAction *self);

#endif
