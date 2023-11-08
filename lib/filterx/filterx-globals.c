#include "filterx/object-primitive.h"
#include "filterx/object-null.h"
#include "filterx/filterx-globals.h"

void
filterx_global_init(void)
{
  filterx_type_init(&FILTERX_TYPE_NAME(null));
  filterx_type_init(&FILTERX_TYPE_NAME(integer));
  filterx_type_init(&FILTERX_TYPE_NAME(boolean));
  filterx_type_init(&FILTERX_TYPE_NAME(double));
}

void
filterx_global_deinit(void)
{
}
