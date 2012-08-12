#ifndef LIBTEST_TEMPLATE_LIB_H_INCLUDED
#define LIBTEST_TEMPLATE_LIB_H_INCLUDED 1

#include "testutils.h"

void assert_template_format(const gchar *template, const gchar *expected);
void assert_template_failure(const gchar *template, const gchar *expected_failure);

void init_template_tests(void);
void deinit_template_tests(void);


#endif
