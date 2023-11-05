#include "filterx/expr-boolalg.h"
#include "filterx/object-primitive.h"

static FilterXObject *
_eval_not(FilterXExpr *s)
{
  FilterXUnaryOp *self = (FilterXUnaryOp *) s;

  FilterXObject *result = filterx_expr_eval(self->operand);
  gboolean truthy = filterx_object_truthy(result);
  filterx_object_unref(result);

  if (truthy)
    return filterx_boolean_new(FALSE);
  else
    return filterx_boolean_new(TRUE);
}

FilterXExpr *
filterx_unary_not_new(FilterXExpr *operand)
{
  FilterXUnaryOp *self = g_new0(FilterXUnaryOp, 1);

  filterx_unary_op_init_instance(self, operand);
  self->super.eval = _eval_not;
  return &self->super;
}

static FilterXObject *
_eval_and(FilterXExpr *s)
{
  FilterXBinaryOp *self = (FilterXBinaryOp *) s;

  /* we only evaluate our rhs if lhs is true, e.g.  we do fast circuit
   * evaluation just like most languages */

  FilterXObject *result = filterx_expr_eval(self->lhs);
  gboolean lhs_truthy = filterx_object_truthy(result);
  filterx_object_unref(result);

  if (!lhs_truthy)
    return filterx_boolean_new(FALSE);

  result = filterx_expr_eval(self->rhs);
  gboolean rhs_truthy = filterx_object_truthy(result);
  filterx_object_unref(result);

  if (!rhs_truthy)
    return filterx_boolean_new(FALSE);

  return filterx_boolean_new(TRUE);
}

FilterXExpr *
filterx_binary_and_new(FilterXExpr *lhs, FilterXExpr *rhs)
{
  FilterXBinaryOp *self = g_new0(FilterXBinaryOp, 1);

  filterx_binary_op_init_instance(self, lhs, rhs);
  self->super.eval = _eval_and;
  return &self->super;
}

static FilterXObject *
_eval_or(FilterXExpr *s)
{
  FilterXBinaryOp *self = (FilterXBinaryOp *) s;

  /* we only evaluate our rhs if lhs is false, e.g.  we do fast circuit
   * evaluation just like most languages */

  FilterXObject *result = filterx_expr_eval(self->lhs);
  gboolean lhs_truthy = filterx_object_truthy(result);
  filterx_object_unref(result);

  if (lhs_truthy)
    return filterx_boolean_new(TRUE);

  result = filterx_expr_eval(self->rhs);
  gboolean rhs_truthy = filterx_object_truthy(result);
  filterx_object_unref(result);

  if (rhs_truthy)
    return filterx_boolean_new(TRUE);

  return filterx_boolean_new(FALSE);
}

FilterXExpr *
filterx_binary_or_new(FilterXExpr *lhs, FilterXExpr *rhs)
{
  FilterXBinaryOp *self = g_new0(FilterXBinaryOp, 1);

  filterx_binary_op_init_instance(self, lhs, rhs);
  self->super.eval = _eval_or;
  return &self->super;
}
