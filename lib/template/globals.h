#ifndef TEMPLATES_GLOBALS_INCLUDED
#define TEMPLATES_GLOBALS_INCLUDED

#include "common-template-typedefs.h"

LogTemplateOptions *log_template_get_global_template_options(void);

void log_template_global_init(void);
void log_template_global_deinit(void);

#endif
