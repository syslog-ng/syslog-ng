#ifndef PATTERNDB_PDB_RULE_H_INCLUDED
#define PATTERNDB_PDB_RULE_H_INCLUDED

#include "syslog-ng.h"
#include "pdb-action.h"

/* this class encapsulates a the verdict of a rule in the pattern
 * database and is stored as the "value" member in the RADIX tree
 * node. It contains a reference the the original rule in the rule
 * database. */
typedef struct _PDBRule PDBRule;
struct _PDBRule
{
  GAtomicCounter ref_cnt;
  gchar *class;
  gchar *rule_id;
  PDBMessage msg;
  gint context_timeout;
  PDBCorrellationScope context_scope;
  LogTemplate *context_id_template;
  GPtrArray *actions;
};

void pdb_rule_set_class(PDBRule *self, const gchar *class);
void pdb_rule_set_rule_id(PDBRule *self, const gchar *rule_id);
void pdb_rule_set_context_id_template(PDBRule *self, LogTemplate *context_id_template);
void pdb_rule_set_context_timeout(PDBRule *self, gint timeout);
void pdb_rule_set_context_scope(PDBRule *self, const gchar *scope, GError **error);
void pdb_rule_add_action(PDBRule *self, PDBAction *action);
gchar *pdb_rule_get_name(PDBRule *self);

PDBRule *pdb_rule_new(void);
PDBRule *pdb_rule_ref(PDBRule *self);
void pdb_rule_unref(PDBRule *self);

#endif
