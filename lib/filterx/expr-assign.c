#include "filterx/expr-assign.h"

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXBinaryOp *self = (FilterXBinaryOp *) s;

  FilterXObject *value = filterx_expr_eval(self->rhs);
  if (!value)
    return NULL;

  if (filterx_expr_assign(self->lhs, value))
    return value;

  filterx_object_unref(value);
  return NULL;
}

/* NOTE: takes the object reference */
FilterXExpr *
filterx_assign_new(FilterXExpr *lhs, FilterXExpr *rhs)
{
  FilterXBinaryOp *self = g_new0(FilterXBinaryOp, 1);

  filterx_binary_op_init_instance(self, lhs, rhs);
  self->super.eval = _eval;
  return &self->super;
}
