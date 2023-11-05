#include "filterx/expr-template.h"
#include "filterx/object-message-value.h"
#include "template/templates.h"
#include "scratch-buffers.h"

typedef struct _FilterXTemplate
{
  FilterXExpr super;
  LogTemplate *template;
} FilterXTemplate;

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXTemplate *self = (FilterXTemplate *) s;
  FilterXEvalContext *context = filterx_eval_get_context();

  GString *value = scratch_buffers_alloc();
  LogMessageValueType t;

  /* FIXME: we could go directly to filterx_string_new() here to avoid a round trip in FilterXMessageValue */
  /* FIXME/2: let's make this handle literal and trivial templates */

  log_template_format_value_and_type_with_context(self->template, context->msgs, context->num_msg, context->template_eval_options, value, &t);

  /* NOTE: we borrow value->str here which is stored in a scratch buffer
   * that should be valid as long as we are traversing the filter
   * expressions, thus the FilterXObject is shorter lived than the scratch
   * buffer.  */

  return filterx_message_value_new_borrowed(value->str, value->len, t);
}

static void
_free(FilterXExpr *s)
{
  FilterXTemplate *self = (FilterXTemplate *) s;
  log_template_unref(self->template);
}

/* NOTE: takes the object reference */
FilterXExpr *
filterx_template_new(LogTemplate *template)
{
  FilterXTemplate *self = g_new0(FilterXTemplate, 1);

  filterx_expr_init_instance(&self->super);
  self->super.eval = _eval;
  self->super.free_fn = _free;
  self->template = template;
  return &self->super;
}
