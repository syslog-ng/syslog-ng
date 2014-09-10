#include "logrewrite.h"
#include "value-pairs.h"

typedef struct _LogRewriteGroupSet {
  LogRewrite super;
  ValuePairs *query;
  LogTemplate *replacement;
} LogRewriteGroupSet;

LogRewrite *log_rewrite_groupset_new(LogTemplate *template);
void log_rewrite_groupset_add_fields(LogRewrite *rewrite, GList *fields);
