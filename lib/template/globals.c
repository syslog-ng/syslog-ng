#include "templates.h"
#include "macros.h"

static LogTemplateOptions global_template_options;

LogTemplateOptions *
log_template_get_global_template_options(void)
{
  return &global_template_options;
}

void
log_template_global_init(void)
{
  log_template_options_global_defaults(&global_template_options);
  log_macros_global_init();
}

void
log_template_global_deinit(void)
{
  log_macros_global_deinit();
}
