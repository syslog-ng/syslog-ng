#include <criterion/criterion.h>
#include "libtest/cr_template.h"

#include "filterx/expr-literal.h"
#include "filterx/expr-template.h"
#include "filterx/object-primitive.h"

#include "apphook.h"
#include "scratch-buffers.h"

static void
assert_marshaled_object(FilterXObject *obj, const gchar *repr, LogMessageValueType type)
{
  GString *b = g_string_sized_new(0);
  LogMessageValueType t;

  filterx_object_marshal(obj, b, &t);
  cr_assert_str_eq(b->str, repr);
  cr_assert_eq(t, type);
  g_string_free(b, TRUE);
}

Test(filterx_expr, test_filterx_expr_construction_and_free)
{
  FilterXExpr *fexpr = filterx_expr_new();

  filterx_expr_unref(fexpr);
}

Test(filterx_expr, test_filterx_literal_evaluates_to_the_literal_object)
{
  FilterXExpr *fexpr = filterx_literal_new(filterx_integer_new(42));
  FilterXObject *fobj = filterx_expr_eval(fexpr);

  assert_marshaled_object(fobj, "42", LM_VT_INTEGER);

  filterx_expr_unref(fexpr);
  filterx_object_unref(fobj);
}

Test(filterx_expr, test_filterx_template_evaluates_to_the_expanded_value)
{
  LogMessage *msg = create_sample_message();
  FilterXScope *scope = filterx_scope_new();

  FilterXEvalContext context =
  {
    .msgs = &msg,
    .num_msg = 1,
    .template_eval_options = &DEFAULT_TEMPLATE_EVAL_OPTIONS,
    .scope = scope,
  };

  filterx_eval_set_context(&context);
  FilterXExpr *fexpr = filterx_template_new(compile_template("$HOST $PROGRAM"));
  FilterXObject *fobj = filterx_expr_eval(fexpr);

  assert_marshaled_object(fobj, "bzorp syslog-ng", LM_VT_STRING);

  filterx_expr_unref(fexpr);
  log_msg_unref(msg);
  filterx_object_unref(fobj);
  filterx_scope_free(scope);
  filterx_eval_set_context(NULL);
}

static void
setup(void)
{
  app_startup();
  init_template_tests();
}

static void
teardown(void)
{
  scratch_buffers_explicit_gc();
  deinit_template_tests();
  app_shutdown();
}

TestSuite(filterx_expr, .init = setup, .fini = teardown);
