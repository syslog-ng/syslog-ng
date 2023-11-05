#ifndef FILTERX_EVAL_H_INCLUDED
#define FILTERX_EVAL_H_INCLUDED

#include "filterx/filterx-scope.h"
#include "template/eval.h"

typedef struct _FilterXEvalContext FilterXEvalContext;

struct _FilterXEvalContext
{
  LogMessage **msgs;
  gint num_msg;
  FilterXScope *scope;
  LogTemplateEvalOptions *template_eval_options;
};

FilterXEvalContext *filterx_eval_get_context(void);
FilterXScope *filterx_eval_get_scope(void);
void filterx_eval_set_context(FilterXEvalContext *context);
gboolean filterx_eval_exec_statements(GList *statements, LogMessage **msg, const LogPathOptions *options);

#endif
