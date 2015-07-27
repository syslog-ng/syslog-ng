#ifndef PATTERNDB_PDB_PROGRAM_H_INCLUDED
#define PATTERNDB_PDB_PROGRAM_H_INCLUDED

#include "syslog-ng.h"
#include "radix.h"

/*
 * This class encapsulates a set of program related rules in the
 * pattern database. Its instances are stored as "value" in the
 * program name RADIX tree. It basically contains another RADIX for
 * the per-program patterns.
 */
typedef struct _PDBProgram
{
  guint ref_cnt;
  RNode *rules;
} PDBProgram;

PDBProgram *pdb_program_new(void);
PDBProgram *pdb_program_ref(PDBProgram *self);
void pdb_program_unref(PDBProgram *s);

#endif
