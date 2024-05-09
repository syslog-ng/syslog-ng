/*
 * Copyright (c) 2024 Attila Szakacs
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
#include "filterx-lib.h"
#include "cr_template.h"
#include "filterx/object-json.h"
#include "filterx/object-string.h"
#include "filterx/expr-shorthand.h"
#include "filterx/expr-literal.h"
#include "filterx/filterx-eval.h"

void
assert_marshaled_object(FilterXObject *obj, const gchar *repr, LogMessageValueType type)
{
  GString *b = g_string_sized_new(0);
  LogMessageValueType t;

  /* check if we _overwrite_ the string with the marshalled value */
  g_string_append(b, "PREFIX");

  cr_assert(filterx_object_marshal(obj, b, &t) == TRUE);
  cr_assert_str_eq(b->str, repr);
  cr_assert_eq(t, type);
  g_string_free(b, TRUE);
}

void
assert_object_json_equals(FilterXObject *obj, const gchar *expected_json_repr)
{
  struct json_object *jso = NULL;
  FilterXObject *assoc_object = NULL;

  cr_assert(filterx_object_map_to_json(obj, &jso, &assoc_object) == TRUE, "error mapping to json, expected json was: %s",
            expected_json_repr);
  const gchar *json_repr = json_object_to_json_string_ext(jso, JSON_C_TO_STRING_PLAIN);
  cr_assert_str_eq(json_repr, expected_json_repr);
  json_object_put(jso);
  filterx_object_unref(assoc_object);
}


FilterXObject *
filterx_test_dict_new(void)
{
  FilterXObject *result = filterx_json_object_new_empty();
  result->type = &FILTERX_TYPE_NAME(test_dict);
  return result;
}

FilterXObject *
filterx_test_list_new(void)
{
  FilterXObject *result = filterx_json_array_new_empty();
  result->type = &FILTERX_TYPE_NAME(test_list);
  return result;
}

const gchar *
filterx_test_unknown_object_marshaled_repr(gssize *len)
{
  static const gchar *marshaled = "test_unknown_object_marshaled";
  if (len)
    *len = strlen(marshaled);
  return marshaled;
}

const gchar *
filterx_test_unknown_object_repr(gssize *len)
{
  static const gchar *repr = "test_unknown_object_repr";
  if (len)
    *len = strlen(repr);
  return repr;
}

static gboolean
_unknown_marshal(FilterXObject *s, GString *repr, LogMessageValueType *t)
{
  *t = LM_VT_STRING;
  g_string_append(repr, filterx_test_unknown_object_marshaled_repr(NULL));
  return TRUE;
}

static gboolean
_unknown_repr(FilterXObject *s, GString *repr)
{
  g_string_append(repr, filterx_test_unknown_object_repr(NULL));
  return TRUE;
}

static gboolean
_unknown_truthy(FilterXObject *s)
{
  return TRUE;
}

static gboolean
_unknown_map_to_json(FilterXObject *s, struct json_object **object, FilterXObject **assoc_object)
{
  gssize len;
  const gchar *repr = filterx_test_unknown_object_marshaled_repr(&len);
  *object = json_object_new_string_len(repr, len);
  *assoc_object = filterx_string_new(repr, len);
  return TRUE;
}

FilterXObject *
filterx_test_unknown_object_new(void)
{
  return filterx_object_new(&FILTERX_TYPE_NAME(test_unknown_object));
}

FilterXExpr *
filterx_non_literal_new(FilterXObject *object)
{
  FilterXExpr *shorthand = filterx_shorthand_new();
  filterx_shorthand_add(shorthand, filterx_literal_new(object));
  return shorthand;
}

FilterXExpr *
filterx_error_expr_new(void)
{
  return filterx_literal_new(NULL);
}

static struct
{
  LogMessage *msg;
  FilterXScope *scope;
  FilterXEvalContext context;
} filterx_env;

void
init_libtest_filterx(void)
{
  filterx_type_init(&FILTERX_TYPE_NAME(test_unknown_object));

  /* These will use the json implementations, but won't be json typed. */
  filterx_type_init(&FILTERX_TYPE_NAME(test_dict));
  filterx_type_init(&FILTERX_TYPE_NAME(test_list));
  FILTERX_TYPE_NAME(test_dict) = FILTERX_TYPE_NAME(json_object);
  FILTERX_TYPE_NAME(test_list) = FILTERX_TYPE_NAME(json_array);

  filterx_env.msg = create_sample_message();
  filterx_eval_init_context(&filterx_env.context, NULL);
  filterx_env.context.msgs = &filterx_env.msg;
  filterx_env.context.num_msg = 1;

}

void
deinit_libtest_filterx(void)
{
  log_msg_unref(filterx_env.msg);
  filterx_eval_deinit_context(&filterx_env.context);
}

FILTERX_DEFINE_TYPE(test_dict, FILTERX_TYPE_NAME(object));
FILTERX_DEFINE_TYPE(test_list, FILTERX_TYPE_NAME(object));
FILTERX_DEFINE_TYPE(test_unknown_object, FILTERX_TYPE_NAME(object),
                    .is_mutable = FALSE,
                    .truthy = _unknown_truthy,
                    .marshal = _unknown_marshal,
                    .repr = _unknown_repr,
                    .map_to_json = _unknown_map_to_json,
                    .list_factory = filterx_test_list_new,
                    .dict_factory = filterx_test_dict_new);
