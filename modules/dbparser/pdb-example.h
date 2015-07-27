#ifndef PATTERNDB_PDB_EXAMPLE_H_INCLUDED
#define PATTERNDB_PDB_EXAMPLE_H_INCLUDED

#include "syslog-ng.h"
#include "patterndb-int.h"

/* this class encapsulates an example message in the pattern database
 * used for testing rules and patterns. It contains the message with the
 * program field and the expected rule_id with the expected name/value
 * pairs. */
typedef struct _PDBExample
{
  PDBRule *rule;
  gchar *message;
  gchar *program;
  GPtrArray *values;
} PDBExample;

void pdb_example_free(PDBExample *s);


#endif
