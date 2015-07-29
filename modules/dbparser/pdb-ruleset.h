#ifndef PATTERNDB_PDB_RULESET_H_INCLUDED
#define PATTERNDB_PDB_RULESET_H_INCLUDED

#include "syslog-ng.h"
#include "radix.h"

/* rules loaded from a pdb file */
typedef struct _PDBRuleSet
{
  RNode *programs;
  gchar *version;
  gchar *pub_date;
} PDBRuleSet;

PDBRuleSet *pdb_rule_set_new(void);
void pdb_rule_set_free(PDBRuleSet *self);

#endif
