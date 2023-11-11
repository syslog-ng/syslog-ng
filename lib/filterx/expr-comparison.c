#include "filterx/expr-comparison.h"
#include "object-primitive.h"

typedef struct _FilterXComparison
{
  FilterXBinaryOp super;
  gint operator;
} FilterXComparison;

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXComparison *self = (FilterXComparison *) s;

  FilterXObject *lhs_object = filterx_expr_eval(self->super.lhs);
  if (!lhs_object)
    return NULL;

  FilterXObject *rhs_object = filterx_expr_eval(self->super.rhs);
  if (!rhs_object)
    {
      filterx_object_unref(lhs_object);
      return NULL;
    }

  /* FIXME: perform the comparison */
  gboolean result = TRUE;
  filterx_object_unref(lhs_object);
  filterx_object_unref(rhs_object);
  return filterx_boolean_new(result);
}

/* NOTE: takes the object reference */
FilterXExpr *
filterx_comparison_new(FilterXExpr *lhs, FilterXExpr *rhs, gint operator)
{
  FilterXComparison *self = g_new0(FilterXComparison, 1);

  filterx_binary_op_init_instance(&self->super, lhs, rhs);
  self->super.super.eval = _eval;
  self->operator = operator;
  return &self->super.super;
}
