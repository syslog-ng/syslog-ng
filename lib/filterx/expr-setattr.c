#include "filterx/expr-setattr.h"
#include "filterx/object-primitive.h"

typedef struct _FilterXSetAttr
{
  FilterXExpr super;
  FilterXExpr *object;
  gchar *attr_name;
  FilterXExpr *new_value;
} FilterXSetAttr;

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXSetAttr *self = (FilterXSetAttr *) s;
  FilterXObject *result = NULL;

  FilterXObject *object = filterx_expr_eval_typed(self->object);
  if (!object)
    return NULL;

  FilterXObject *new_value = filterx_expr_eval_typed(self->new_value);
  if (!new_value)
    goto exit;

  if (!filterx_object_setattr(object, self->attr_name, new_value))
    goto exit;
  result = filterx_object_ref(new_value);
exit:
  filterx_object_unref(object);
  filterx_object_unref(new_value);
  return result;
}

static void
_free(FilterXExpr *s)
{
  FilterXSetAttr *self = (FilterXSetAttr *) s;

  g_free(self->attr_name);
  filterx_expr_unref(self->object);
  filterx_expr_unref(self->new_value);
}

FilterXExpr *
filterx_setattr_new(FilterXExpr *object, const gchar *attr_name, FilterXExpr *new_value)
{
  FilterXSetAttr *self = g_new0(FilterXSetAttr, 1);

  filterx_expr_init_instance(&self->super);
  self->super.eval = _eval;
  self->super.free_fn = _free;
  self->object = object;
  self->attr_name = g_strdup(attr_name);
  self->new_value = new_value;
  return &self->super;
}
