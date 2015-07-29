#include "pdb-program.h"
#include "pdb-rule.h"

/*
 * Database based parser. The patterns are stored in an XML database.
 * Data structure is:
 *   - Parser -> programs -> rules -> patterns
 */

PDBProgram *
pdb_program_new(void)
{
  PDBProgram *self = g_new0(PDBProgram, 1);

  self->rules = r_new_node((guint8 *) "", NULL);
  self->ref_cnt = 1;
  return self;
}

PDBProgram *
pdb_program_ref(PDBProgram *self)
{
  self->ref_cnt++;
  return self;
}

void
pdb_program_unref(PDBProgram *s)
{
  PDBProgram *self = (PDBProgram *) s;

  if (--self->ref_cnt == 0)
    {
      if (self->rules)
        r_free_node(self->rules, (void (*)(void *)) pdb_rule_unref);

      g_free(self);
    }
}
