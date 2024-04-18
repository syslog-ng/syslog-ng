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

#include "filterx/expr-regexp.h"
#include "filterx/object-primitive.h"
#include "filterx/object-string.h"
#include "filterx/object-message-value.h"
#include "compat/pcre.h"

typedef struct FilterXReMatchState_
{
  pcre2_match_data *match_data;
  FilterXObject *lhs_obj;
  const gchar *lhs_str;
  gsize lhs_str_len;
} FilterXReMatchState;

static void
_state_init(FilterXReMatchState *state)
{
  memset(state, 0, sizeof(FilterXReMatchState));
}

static void
_state_cleanup(FilterXReMatchState *state)
{
  if (state->match_data)
    pcre2_match_data_free(state->match_data);
  filterx_object_unref(state->lhs_obj);
  memset(state, 0, sizeof(FilterXReMatchState));
}


static pcre2_code_8 *
_compile_pattern(const gchar *pattern)
{
  gint rc;
  PCRE2_SIZE error_offset;
  gint flags = PCRE2_DUPNAMES;

  pcre2_code_8 *compiled = pcre2_compile((PCRE2_SPTR) pattern, PCRE2_ZERO_TERMINATED, flags, &rc, &error_offset, NULL);
  if (!compiled)
    {
      PCRE2_UCHAR error_message[128];
      pcre2_get_error_message(rc, error_message, sizeof(error_message));
      msg_error("FilterX: Failed to compile regexp pattern",
                evt_tag_str("pattern", pattern),
                evt_tag_str("error", (const gchar *) error_message),
                evt_tag_int("error_offset", (gint) error_offset));
      return NULL;
    }

  rc = pcre2_jit_compile(compiled, PCRE2_JIT_COMPLETE);
  if (rc < 0)
    {
      PCRE2_UCHAR error_message[128];
      pcre2_get_error_message(rc, error_message, sizeof(error_message));
      msg_debug("FilterX: Failed to JIT compile regular expression",
                evt_tag_str("pattern", pattern),
                evt_tag_str("error", (const gchar *) error_message));
    }

  return compiled;
}

/*
 * Returns whether lhs matched the pattern.
 * Populates state if no error happened.
 */
static gboolean
_match(FilterXExpr *lhs_expr, pcre2_code_8 *pattern, FilterXReMatchState *state)
{
  state->lhs_obj = filterx_expr_eval(lhs_expr);
  if (!state->lhs_obj)
    goto error;

  if (filterx_object_is_type(state->lhs_obj, &FILTERX_TYPE_NAME(message_value)))
    {
      if (filterx_message_value_get_type(state->lhs_obj) != LM_VT_STRING)
        {
          msg_error("FilterX: Regexp matching left hand side must be string type",
                    evt_tag_str("type", state->lhs_obj->type->name));
          goto error;
        }
      state->lhs_str = filterx_message_value_get_value(state->lhs_obj, &state->lhs_str_len);
    }
  else if (filterx_object_is_type(state->lhs_obj, &FILTERX_TYPE_NAME(string)))
    {
      state->lhs_str = filterx_string_get_value(state->lhs_obj, &state->lhs_str_len);
    }
  else
    {
      msg_error("FilterX: Regexp matching left hand side must be string type",
                evt_tag_str("type", state->lhs_obj->type->name));
      goto error;
    }

  state->match_data = pcre2_match_data_create_from_pattern(pattern, NULL);
  gint rc = pcre2_match(pattern, (PCRE2_SPTR) state->lhs_str, (PCRE2_SIZE) state->lhs_str_len, (PCRE2_SIZE) 0, 0,
                        state->match_data, NULL);
  if (rc < 0)
    {
      switch (rc)
        {
        case PCRE2_ERROR_NOMATCH:
          return FALSE;
        default:
          /* Handle other special cases */
          msg_error("FilterX: Error while matching regexp", evt_tag_int("error_code", rc));
          goto error;
        }
    }
  else if (rc == 0)
    {
      msg_error("FilterX: Error while storing matching substrings, more than 256 capture groups encountered");
      goto error;
    }

  return TRUE;

error:
  _state_cleanup(state);
  return FALSE;
}

static gboolean
_has_named_capture_groups(pcre2_code_8 *pattern)
{
  guint32 namecount = 0;
  pcre2_pattern_info(pattern, PCRE2_INFO_NAMECOUNT, &namecount);
  return namecount > 0;
}


typedef struct FilterXExprRegexpMatch_
{
  FilterXExpr super;
  FilterXExpr *lhs;
  pcre2_code_8 *pattern;
} FilterXExprRegexpMatch;

static FilterXObject *
_regexp_match_eval(FilterXExpr *s)
{
  FilterXExprRegexpMatch *self = (FilterXExprRegexpMatch *) s;

  FilterXObject *result = NULL;
  FilterXReMatchState state;
  _state_init(&state);

  gboolean matched = _match(self->lhs, self->pattern, &state);
  if (!state.match_data)
    {
      /* Error happened during matching. */
      goto exit;
    }

  result = filterx_boolean_new(matched);

exit:
  _state_cleanup(&state);
  return result;
}

static void
_regexp_match_free(FilterXExpr *s)
{
  FilterXExprRegexpMatch *self = (FilterXExprRegexpMatch *) s;

  filterx_expr_unref(self->lhs);
  if (self->pattern)
    pcre2_code_free(self->pattern);
  filterx_expr_free_method(s);
}

/* Takes reference of lhs */
FilterXExpr *
filterx_expr_regexp_match_new(FilterXExpr *lhs, const gchar *pattern)
{
  FilterXExprRegexpMatch *self = g_new0(FilterXExprRegexpMatch, 1);

  filterx_expr_init_instance(&self->super);
  self->super.eval = _regexp_match_eval;
  self->super.free_fn = _regexp_match_free;

  self->lhs = lhs;
  self->pattern = _compile_pattern(pattern);
  if (!self->pattern)
    {
      filterx_expr_unref(&self->super);
      return NULL;
    }

  return &self->super;
}


typedef struct FilterXExprRegexpSearchGenerator_
{
  FilterXExprGenerator super;
  FilterXExpr *lhs;
  pcre2_code_8 *pattern;
} FilterXExprRegexpSearchGenerator;

static FilterXObject *
_regexp_search_generator_eval(FilterXExpr *s)
{
  FilterXExprRegexpSearchGenerator *self = (FilterXExprRegexpSearchGenerator *) s;

  FilterXObject *fillable = filterx_expr_eval_typed(self->super.fillable);
  if (!fillable)
    return NULL;

  FilterXObject *result = NULL;
  FilterXReMatchState state;
  _state_init(&state);

  gboolean matched = _match(self->lhs, self->pattern, &state);
  if (!state.match_data)
    {
      /* Error happened during matching. */
      goto exit;
    }

  /* TODO: store matches */

  result = filterx_boolean_new(TRUE);

exit:
  _state_cleanup(&state);
  filterx_object_unref(fillable);
  return result;
}

static FilterXObject *
_regexp_search_generator_create_container(FilterXExprGenerator *s, FilterXExpr *fillable_parent)
{
  FilterXExprRegexpSearchGenerator *self = (FilterXExprRegexpSearchGenerator *) s;

  FilterXObject *fillable_parent_obj = filterx_expr_eval_typed(fillable_parent);
  if (!fillable_parent_obj)
    return NULL;

  FilterXObject *result;
  if (_has_named_capture_groups(self->pattern))
    result = filterx_object_create_dict(fillable_parent_obj);
  else
    result = filterx_object_create_list(fillable_parent_obj);

  filterx_object_unref(fillable_parent_obj);
  return result;
}

static void
_regexp_search_generator_free(FilterXExpr *s)
{
  FilterXExprRegexpSearchGenerator *self = (FilterXExprRegexpSearchGenerator *) s;

  filterx_expr_unref(self->lhs);
  if (self->pattern)
    pcre2_code_free(self->pattern);
  filterx_generator_free_method(s);
}

/* Takes reference of lhs */
FilterXExpr *
filterx_expr_regexp_search_generator_new(FilterXExpr *lhs, const gchar *pattern)
{
  FilterXExprRegexpSearchGenerator *self = g_new0(FilterXExprRegexpSearchGenerator, 1);

  filterx_generator_init_instance(&self->super.super);
  self->super.super.eval = _regexp_search_generator_eval;
  self->super.super.free_fn = _regexp_search_generator_free;
  self->super.create_container = _regexp_search_generator_create_container;

  self->lhs = lhs;
  self->pattern = _compile_pattern(pattern);
  if (!self->pattern)
    {
      filterx_expr_unref(&self->super.super);
      return NULL;
    }

  return &self->super.super;
}
