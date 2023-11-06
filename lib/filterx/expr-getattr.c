#include "filterx/expr-getattr.h"

typedef struct _FilterXGetAttr
{
  FilterXExpr super;
  FilterXExpr *operand;
  gchar *attr_name;
} FilterXGetAttr;

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXGetAttr *self = (FilterXGetAttr *) s;

  FilterXObject *variable = filterx_expr_eval_typed(self->operand);

  if (!variable)
    return NULL;

  FilterXObject *attr = filterx_object_getattr(variable, self->attr_name);
  if (!attr)
    goto exit;
exit:
  filterx_object_unref(variable);
  return attr;
}

static void
_free(FilterXExpr *s)
{
  FilterXGetAttr *self = (FilterXGetAttr *) s;
  g_free(self->attr_name);
  filterx_expr_unref(self->operand);
}

/* NOTE: takes the object reference */
FilterXExpr *
filterx_getattr_new(FilterXExpr *operand, const gchar *attr_name)
{
  FilterXGetAttr *self = g_new0(FilterXGetAttr, 1);

  filterx_expr_init_instance(&self->super);
  self->super.eval = _eval;
  self->super.free_fn = _free;
  self->operand = operand;
  self->attr_name = g_strdup(attr_name);
  return &self->super;
}
