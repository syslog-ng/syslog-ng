#include <criterion/criterion.h>
#include "filterx/filterx-object.h"
#include "apphook.h"

Test(filterx_object, test_filterx_object_construction_and_free)
{
  FilterXObject *fobj = filterx_object_new();

  filterx_object_unref(fobj);
}

static void
setup(void)
{
  app_startup();
}

static void
teardown(void)
{
  app_shutdown();
}

TestSuite(filterx_object, .init = setup, .fini = teardown);
