#include "tagrecord-scanner.h"
#include "csv-tagrecord-scanner.h"
#include "messages.h"
#include <string.h>

void
tag_record_scanner_free(TagRecordScanner *self)
{
  if (self && self->free_fn)
    self->free_fn(self);
}

void
tag_record_scanner_set_name_prefix(TagRecordScanner *self, const gchar *prefix)
{
  self->name_prefix = prefix;
}

TagRecordScanner*
create_tag_record_scanner_by_type(const gchar *type)
{
  TagRecordScanner *scanner = NULL;

  if (type == NULL)
    return NULL;

  if (!strncmp(type, "csv", 3))
    {
      scanner = csv_tag_record_scanner_new();
    }

  if (!scanner)
    msg_warning("Unknown TagRecordScanner", evt_tag_str("type", type));

  return scanner;
}
