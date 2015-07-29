#ifndef PATTERNDB_PDB_LOAD_H_INCLUDED
#define PATTERNDB_PDB_LOAD_H_INCLUDED

#include "syslog-ng.h"
#include "pdb-ruleset.h"
#include "cfg.h"

gboolean pdb_rule_set_load(PDBRuleSet *self, GlobalConfig *cfg, const gchar *config, GList **examples);

#endif
