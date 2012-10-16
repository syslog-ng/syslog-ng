#ifndef LIBTEST_TEMPLATE_LIB_H_INCLUDED
#define LIBTEST_TEMPLATE_LIB_H_INCLUDED 1

#include "testutils.h"
#include "logmsg.h"
#include "templates.h"

void assert_template_format(const gchar *template, const gchar *expected);
void assert_template_format_with_context(const gchar *template, const gchar *expected);
void assert_template_failure(const gchar *template, const gchar *expected_failure);

void init_template_tests(void);
void deinit_template_tests(void);

LogMessage * create_sample_message(void);
LogTemplate * compile_template(const gchar *template);
#endif
