/*
 * Copyright (c) 2023 shifter <shifter@axoflow.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include <criterion/criterion.h>
#include "libtest/cr_template.h"

#include "filterx/filterx-object.h"
#include "filterx/object-primitive.h"
#include "filterx/expr-comparison.h"
#include "filterx/expr-condition.h"
#include "filterx/filterx-expr.h"
#include "filterx/expr-literal.h"
#include "filterx/object-string.h"
#include "filterx/object-null.h"
#include "filterx/object-datetime.h"
#include "filterx/object-message-value.h"
#include "filterx/expr-assign.h"
#include "filterx/expr-template.h"
#include "filterx/expr-variable.h"
#include "filterx/filterx-private.h"

#include "apphook.h"
#include "scratch-buffers.h"


#define TEST_BUILTIN_FUNCTION_NAME "TEST_BUILTIN_DUMMY_KEY"

FilterXObject *
test_builtin_simple_dummy_function(GPtrArray *args)
{
  return filterx_string_new("test-builtin-functions", -1);
}

Test(builtin_functions, test_builtin_simple_functions_registering_existing_key_returns_false)
{
  GHashTable *ht;
  filterx_builtin_simple_functions_init_private(&ht);
  // first register returns TRUE
  cr_assert(filterx_builtin_simple_function_register_private(ht, TEST_BUILTIN_FUNCTION_NAME,
                                                             test_builtin_simple_dummy_function));
  // second attampt of register must return FALSE
  cr_assert(!filterx_builtin_simple_function_register_private(ht, TEST_BUILTIN_FUNCTION_NAME,
                                                              test_builtin_simple_dummy_function));
  filterx_builtin_simple_functions_deinit_private(ht);
}

Test(builtin_functions, test_builtin_simple_functions_lookup)
{
  GHashTable *ht;
  filterx_builtin_simple_functions_init_private(&ht);

  // func not found
  FilterXSimpleFunctionProto func = filterx_builtin_simple_function_lookup_private(ht, TEST_BUILTIN_FUNCTION_NAME);
  cr_assert(func == NULL);

  // add dummy function
  cr_assert(filterx_builtin_simple_function_register_private(ht, TEST_BUILTIN_FUNCTION_NAME,
                                                             test_builtin_simple_dummy_function));

  // lookup returns dummy function
  func = filterx_builtin_simple_function_lookup_private(ht, TEST_BUILTIN_FUNCTION_NAME);
  cr_assert(func != NULL);

  // check dummy function as result
  FilterXObject *res = func(NULL);
  cr_assert(res != NULL);
  cr_assert(filterx_object_is_type(res, &FILTERX_TYPE_NAME(string)));
  gsize len;
  const gchar *str = filterx_string_get_value(res, &len);
  cr_assert(len > 0);

  cr_assert(strcmp(str, "test-builtin-functions") == 0);

  filterx_builtin_simple_functions_deinit_private(ht);
  filterx_object_unref(res);
}

FilterXObject *
_dummy_eval(FilterXExpr *s)
{
  return filterx_string_new("test-builtin-functions", -1);
}

static FilterXFunction *
_test_builtin_dummy_function_ctor(const gchar *function_name, FilterXFunctionArgs *args, GError **error)
{
  FilterXFunction *self = g_new0(FilterXFunction, 1);
  filterx_function_init_instance(self, function_name);
  self->super.eval = _dummy_eval;

  filterx_function_args_free(args);
  return self;
}

Test(builtin_functions, test_builtin_function_ctors_registering_existing_key_returns_false)
{
  GHashTable *ht;
  filterx_builtin_function_ctors_init_private(&ht);
  // first register returns TRUE
  cr_assert(filterx_builtin_function_ctor_register_private(ht, TEST_BUILTIN_FUNCTION_NAME,
                                                           _test_builtin_dummy_function_ctor));
  // second attampt of register must return FALSE
  cr_assert(!filterx_builtin_function_ctor_register_private(ht, TEST_BUILTIN_FUNCTION_NAME,
                                                            _test_builtin_dummy_function_ctor));
  filterx_builtin_function_ctors_deinit_private(ht);
}

Test(builtin_functions, test_builtin_function_ctors_lookup)
{
  GHashTable *ht;
  filterx_builtin_function_ctors_init_private(&ht);

  // func not found
  FilterXFunctionCtor ctor = filterx_builtin_function_ctor_lookup_private(ht, TEST_BUILTIN_FUNCTION_NAME);
  cr_assert(ctor == NULL);

  // add dummy function
  cr_assert(filterx_builtin_function_ctor_register_private(ht, TEST_BUILTIN_FUNCTION_NAME,
                                                           _test_builtin_dummy_function_ctor));

  // lookup returns dummy ctor
  ctor = filterx_builtin_function_ctor_lookup_private(ht, TEST_BUILTIN_FUNCTION_NAME);
  cr_assert(ctor != NULL);

  // check dummy ctor as result
  FilterXFunction *func_expr = ctor(TEST_BUILTIN_FUNCTION_NAME, filterx_function_args_new(NULL, NULL), NULL);
  cr_assert(func_expr != NULL);

  FilterXObject *res = filterx_expr_eval(&func_expr->super);
  cr_assert(filterx_object_is_type(res, &FILTERX_TYPE_NAME(string)));
  gsize len;
  const gchar *str = filterx_string_get_value(res, &len);
  cr_assert(len > 0);

  cr_assert(strcmp(str, "test-builtin-functions") == 0);

  filterx_builtin_function_ctors_deinit_private(ht);
  filterx_object_unref(res);
  filterx_expr_unref(&func_expr->super);
}

static void
setup(void)
{
  app_startup();
}

static void
teardown(void)
{
  scratch_buffers_explicit_gc();
  app_shutdown();
}

TestSuite(builtin_functions, .init = setup, .fini = teardown);
