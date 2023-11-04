#include <criterion/criterion.h>
#include "filterx/filterx-expr.h"
#include "apphook.h"

Test(filterx_expr, test_filterx_expr_construction_and_free)
{
  FilterXExpr *fexpr = filterx_expr_new();

  filterx_expr_unref(fexpr);
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

TestSuite(filterx_expr, .init = setup, .fini = teardown);
