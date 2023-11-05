#include "filterx/expr-message-ref.h"
#include "filterx/object-message-value.h"
#include "filterx/filterx-scope.h"
#include "logmsg/logmsg.h"

typedef struct _FilterXMessageRefExpr
{
  FilterXExpr super;
  NVHandle handle;
} FilterXMessageRefExpr;

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXMessageRefExpr *self = (FilterXMessageRefExpr *) s;
  FilterXEvalContext *context = filterx_eval_get_context();
  LogMessage *msg = context->msgs[0];
  FilterXObject *msg_ref;

  msg_ref = filterx_scope_lookup_message_ref(context->scope, self->handle);
  if (msg_ref)
    return msg_ref;

  gssize value_len;
  LogMessageValueType t;
  const gchar *value = log_msg_get_value_if_set_with_type(msg, self->handle, &value_len, &t);
  if (!value)
    return FALSE;

  msg_ref = filterx_message_value_new_borrowed(value, value_len, t);
  filterx_scope_register_message_ref(context->scope, self->handle, msg_ref);
  return msg_ref;
}

static FilterXObject *
_eval_typed(FilterXExpr *s)
{
  FilterXMessageRefExpr *self = (FilterXMessageRefExpr *) s;
  FilterXEvalContext *context = filterx_eval_get_context();

  FilterXObject *result = filterx_expr_eval(s);
  if (!result)
    return NULL;

  if (filterx_object_is_type(result, &FILTERX_TYPE_NAME(message_value)))
    {
      FilterXObject *unmarshalled = filterx_object_unmarshal(result);
      if (!unmarshalled)
        {
          filterx_object_unref(result);
          return NULL;
        }
      if (result != unmarshalled)
        {
          filterx_object_unref(result);
          filterx_scope_register_message_ref(context->scope, self->handle, unmarshalled);
          result = unmarshalled;
        }
    }
  return result;
}

static gboolean
_assign(FilterXExpr *s, FilterXObject *new_value)
{
  FilterXMessageRefExpr *self = (FilterXMessageRefExpr *) s;
  FilterXScope *scope = filterx_eval_get_scope();

  /* this only clones mutable objects */
  new_value = filterx_object_clone(new_value);

  filterx_scope_register_message_ref(scope, self->handle, new_value);
  new_value->assigned = TRUE;

  filterx_object_unref(new_value);
  return TRUE;
}

static void
_free(FilterXExpr *s)
{
//  FilterXMessageRefExpr *self = (FilterXMessageRefExpr *) s;
}

FilterXExpr *
filterx_message_ref_expr_new(NVHandle handle)
{
  FilterXMessageRefExpr *self = g_new0(FilterXMessageRefExpr, 1);

  filterx_expr_init_instance(&self->super);
  self->super.eval = _eval;
  self->super.eval_typed = _eval_typed;
  self->super.assign = _assign;
  self->super.free_fn = _free;
  self->handle = handle;
  return &self->super;
}
