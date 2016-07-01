#include "tagrecord-scanner.h"

void
tag_record_scanner_free(TagRecordScanner *self)
{
  if (self && self->free_fn)
    self->free_fn(self);
}
