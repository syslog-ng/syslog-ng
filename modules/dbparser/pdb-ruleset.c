#include "pdb-ruleset.h"
#include "pdb-program.h"

PDBRuleSet *
pdb_rule_set_new(void)
{
  PDBRuleSet *self = g_new0(PDBRuleSet, 1);

  return self;
}

void
pdb_rule_set_free(PDBRuleSet *self)
{
  if (self->programs)
    r_free_node(self->programs, (GDestroyNotify) pdb_program_unref);
  if (self->version)
    g_free(self->version);
  if (self->pub_date)
    g_free(self->pub_date);
  self->programs = NULL;
  self->version = NULL;
  self->pub_date = NULL;

  g_free(self);
}
