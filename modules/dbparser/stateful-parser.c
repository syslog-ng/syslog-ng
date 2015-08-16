#include "stateful-parser.h"
#include <string.h>

int
stateful_parser_lookup_inject_mode(const gchar *inject_mode)
{
  if (strcasecmp(inject_mode, "internal") == 0)
    return LDBP_IM_INTERNAL;
  else if (strcasecmp(inject_mode, "pass-through") == 0 || strcasecmp(inject_mode, "pass_through") == 0)
    return LDBP_IM_PASSTHROUGH;
  return -1;
}
