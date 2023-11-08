#include "filterx/object-primitive.h"
#include "filterx/object-null.h"
#include "filterx/object-string.h"
#include "filterx/object-datetime.h"
#include "filterx/object-json.h"
#include "filterx/object-list.h"
#include "filterx/filterx-globals.h"

void
filterx_global_init(void)
{
  filterx_type_init(&FILTERX_TYPE_NAME(null));
  filterx_type_init(&FILTERX_TYPE_NAME(integer));
  filterx_type_init(&FILTERX_TYPE_NAME(boolean));
  filterx_type_init(&FILTERX_TYPE_NAME(double));

  filterx_type_init(&FILTERX_TYPE_NAME(string));
  filterx_type_init(&FILTERX_TYPE_NAME(bytes));
  filterx_type_init(&FILTERX_TYPE_NAME(protobuf));

  filterx_type_init(&FILTERX_TYPE_NAME(json));
  filterx_type_init(&FILTERX_TYPE_NAME(datetime));
}

void
filterx_global_deinit(void)
{
}
