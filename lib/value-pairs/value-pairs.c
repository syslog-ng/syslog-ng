/*
 * Copyright (c) 2011-2015 Balabit
 * Copyright (c) 2011-2014 Gergely Nagy <algernon@balabit.hu>
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

#include "value-pairs/value-pairs.h"
#include "logmsg/logmsg.h"
#include "template/templates.h"
#include "template/macros.h"
#include "type-hinting.h"
#include "cfg-parser.h"
#include "string-list.h"
#include "scratch-buffers.h"
#include "cfg.h"

#include <ctype.h>

typedef struct
{
  GPatternSpec *pattern;
  gboolean include;
} VPPatternSpec;

typedef struct
{
  gchar *name;
  LogTemplate *template;
} VPPairConf;

typedef struct
{
  /* we don't own any of the fields here, it is assumed that allocations are
   * managed by the caller */

  GString *name;
  GString *value;
  TypeHint type_hint;
} VPResultValue;

typedef struct
{
  GTree *result_tree;

  /* array of VPResultValue instances */
  GArray *values;
} VPResults;

struct _ValuePairs
{
  GAtomicCounter ref_cnt;
  GPtrArray *builtins;
  GPtrArray *patterns;
  GPtrArray *vpairs;
  GPtrArray *transforms;

  /* guint32 as CfgFlagHandler only supports 32 bit integers */
  guint32 scopes;
};

typedef enum
{
  VPS_NV_PAIRS        = 0x01,
  VPS_DOT_NV_PAIRS    = 0x02,
  VPS_RFC3164         = 0x04,
  VPS_RFC5424         = 0x08,
  VPS_ALL_MACROS      = 0x10,
  VPS_SELECTED_MACROS = 0x20,
  VPS_SDATA           = 0x40,
  VPS_EVERYTHING      = 0x7f,
} ValuePairScope;

enum
{
  VPT_MACRO,
  VPT_NVPAIR,
};

typedef struct
{
  const gchar *name;
  const gchar *alt_name;
  gint type;
  gint id;
} ValuePairSpec;

static ValuePairSpec rfc3164[] =
{
  /* there's one macro named DATE that'll be expanded specially */
  { "FACILITY" },
  { "PRIORITY" },
  { "HOST"     },
  { "PROGRAM"  },
  { "PID"      },
  { "MESSAGE"  },
  { "DATE" },
  { 0 },
};

static ValuePairSpec rfc5424[] =
{
  { "MSGID", },
  { 0 },
};

static ValuePairSpec selected_macros[] =
{
  { "TAGS" },
  { "SOURCEIP" },
  { "SEQNUM" },
  { 0 },
};

static ValuePairSpec *all_macros;

static CfgFlagHandler value_pair_scope[] =
{
  { "nv-pairs",           CFH_SET, offsetof(ValuePairs, scopes), VPS_NV_PAIRS },
  { "dot-nv-pairs",       CFH_SET, offsetof(ValuePairs, scopes), VPS_DOT_NV_PAIRS},
  { "all-nv-pairs",       CFH_SET, offsetof(ValuePairs, scopes), VPS_NV_PAIRS | VPS_DOT_NV_PAIRS },
  { "rfc3164",            CFH_SET, offsetof(ValuePairs, scopes), VPS_RFC3164 },
  { "core",               CFH_SET, offsetof(ValuePairs, scopes), VPS_RFC3164 },
  { "base",               CFH_SET, offsetof(ValuePairs, scopes), VPS_RFC3164 },
  { "rfc5424",            CFH_SET, offsetof(ValuePairs, scopes), VPS_RFC5424 },
  { "syslog-proto",       CFH_SET, offsetof(ValuePairs, scopes), VPS_RFC5424 },
  { "all-macros",         CFH_SET, offsetof(ValuePairs, scopes), VPS_ALL_MACROS },
  { "selected-macros",    CFH_SET, offsetof(ValuePairs, scopes), VPS_SELECTED_MACROS },
  { "sdata",              CFH_SET, offsetof(ValuePairs, scopes), VPS_SDATA },
  { "everything",         CFH_SET, offsetof(ValuePairs, scopes), VPS_EVERYTHING },
  { NULL,                 0,       0,                            0},
};


static gboolean
vp_pattern_spec_eval(VPPatternSpec *self, const gchar *input)
{
  return g_pattern_match_string(self->pattern, input);
}

static void
vp_pattern_spec_free(VPPatternSpec *self)
{
  g_pattern_spec_free(self->pattern);
  g_free(self);
}

static VPPatternSpec *
vp_pattern_spec_new(const gchar *pattern, gboolean include)
{
  VPPatternSpec *self = g_new0(VPPatternSpec, 1);

  self->pattern = g_pattern_spec_new(pattern);
  self->include = include;
  return self;
}

static VPPairConf *
vp_pair_conf_new(const gchar *key, LogTemplate *value)
{
  VPPairConf *p = g_new(VPPairConf, 1);

  p->name = g_strdup(key);
  p->template = log_template_ref(value);
  return p;
}

static void
vp_pair_conf_free(VPPairConf *vpc)
{
  log_template_unref(vpc->template);
  g_free(vpc->name);
  g_free(vpc);
}

static void
vp_result_value_init(VPResultValue *rv, GString *name, TypeHint type_hint, GString *value)
{
  rv->type_hint = type_hint;
  rv->name = name;
  rv->value = value;
}

static void
vp_results_init(VPResults *results, GCompareFunc compare_func)
{
  results->values = g_array_sized_new(FALSE, FALSE, sizeof(VPResultValue), 16);
  results->result_tree = g_tree_new_full((GCompareDataFunc) compare_func, NULL,
                                         NULL, NULL);
}

static void
vp_results_deinit(VPResults *results)
{
  g_tree_destroy(results->result_tree);
  g_array_free(results->values, TRUE);
}

static void
vp_results_insert(VPResults *results, GString *name, TypeHint type_hint, GString *value)
{
  VPResultValue *rv;
  gint ndx = results->values->len;

  g_array_set_size(results->values, ndx + 1);
  rv = &g_array_index(results->values, VPResultValue, ndx);
  vp_result_value_init(rv, name, type_hint, value);
  /* GTree takes over ownership of name */
  g_tree_insert(results->result_tree, name->str, GINT_TO_POINTER(ndx));
}

static GString *
vp_transform_apply (ValuePairs *vp, const gchar *key)
{
  gint i;
  GString *result = scratch_buffers_alloc();

  g_string_assign(result, key);

  if (vp->transforms->len == 0)
    return result;

  for (i = 0; i < vp->transforms->len; i++)
    {
      ValuePairsTransformSet *t = (ValuePairsTransformSet *) g_ptr_array_index(vp->transforms, i);

      value_pairs_transform_set_apply(t, result);
    }

  return result;
}

/* runs over the name-value pairs requested by the user (e.g. with value_pairs_add_pair) */
static void
vp_pairs_foreach(gpointer data, gpointer user_data)
{
  ValuePairs *vp = ((gpointer *)user_data)[0];
  LogMessage *msg = ((gpointer *)user_data)[2];
  gint32 seq_num = GPOINTER_TO_INT (((gpointer *)user_data)[3]);
  VPResults *results = ((gpointer *)user_data)[5];
  const LogTemplateOptions *template_options = ((gpointer *)user_data)[6];
  GString *sb = scratch_buffers_alloc();
  VPPairConf *vpc = (VPPairConf *)data;
  gint time_zone_mode = GPOINTER_TO_INT (((gpointer *)user_data)[7]);

  log_template_append_format((LogTemplate *)vpc->template, msg,
                             template_options,
                             time_zone_mode, seq_num, NULL, sb);

  vp_results_insert(results, vp_transform_apply(vp, vpc->name), vpc->template->type_hint, sb);
}

/* runs over the LogMessage nv-pairs, and inserts them unless excluded */
static gboolean
vp_msg_nvpairs_foreach(NVHandle handle, gchar *name,
                       const gchar *value, gssize value_len,
                       gpointer user_data)
{
  ValuePairs *vp = ((gpointer *)user_data)[0];
  VPResults *results = ((gpointer *)user_data)[5];
  guint j;
  gboolean inc;
  GString *sb;

  inc = (name[0] == '.' && (vp->scopes & VPS_DOT_NV_PAIRS)) ||
  (name[0] != '.' && (vp->scopes & VPS_NV_PAIRS)) ||
  (log_msg_is_handle_sdata(handle) && (vp->scopes & (VPS_SDATA + VPS_RFC5424)));

  for (j = 0; j < vp->patterns->len; j++)
    {
      VPPatternSpec *vps = (VPPatternSpec *) g_ptr_array_index(vp->patterns, j);
      if (vp_pattern_spec_eval(vps, name))
        inc = vps->include;
    }

  if (!inc)
    return FALSE;

  sb = scratch_buffers_alloc();

  g_string_append_len(sb, value, value_len);
  vp_results_insert(results, vp_transform_apply(vp, name), TYPE_HINT_STRING, sb);

  return FALSE;
}

static gboolean
vp_find_in_set(ValuePairs *vp, const gchar *name, gboolean exclude)
{
  guint j;
  gboolean included = exclude;

  for (j = 0; j < vp->patterns->len; j++)
    {
      VPPatternSpec *vps = (VPPatternSpec *) g_ptr_array_index(vp->patterns, j);

      if (vp_pattern_spec_eval(vps, name))
        included = vps->include;
    }

  return included;
}

static void
vp_merge_other_set(ValuePairs *vp, ValuePairSpec *set, gboolean exclude)
{
  gint i;

  for (i = 0; set[i].name; i++)
    {
      if (!vp_find_in_set(vp, set[i].name, exclude))
        continue;

      g_ptr_array_add(vp->builtins, &set[i]);
    }
}

/* runs over the all macros and merges the selected ones by the pattern into the value-pair set */
static void
vp_merge_macros(ValuePairs *vp)
{
  vp_merge_other_set(vp, all_macros, FALSE);
}

/* runs over a set of ValuePairSpec structs and merges them into the value-pair set */
static void
vp_merge_set(ValuePairs *vp, ValuePairSpec *set)
{
  vp_merge_other_set(vp, set, TRUE);
}


static void
vp_update_builtin_list_of_values(ValuePairs *vp)
{
  g_ptr_array_set_size(vp->builtins, 0);

  if (vp->patterns->len > 0)
    vp_merge_macros(vp);

  if (vp->scopes & (VPS_RFC3164 + VPS_RFC5424 + VPS_SELECTED_MACROS))
    vp_merge_set(vp, rfc3164);

  if (vp->scopes & VPS_RFC5424)
    vp_merge_set(vp, rfc5424);

  if (vp->scopes & VPS_SELECTED_MACROS)
    vp_merge_set(vp, selected_macros);

  if (vp->scopes & VPS_ALL_MACROS)
    vp_merge_set(vp, all_macros);
}

static void
vp_merge_builtins(ValuePairs *vp, VPResults *results, LogMessage *msg, gint32 seq_num, gint time_zone_mode,
                  const LogTemplateOptions *template_options)
{
  gint i;
  GString *sb;

  for (i = 0; i < vp->builtins->len; i++)
    {
      ValuePairSpec *spec = (ValuePairSpec *) g_ptr_array_index(vp->builtins, i);

      sb = scratch_buffers_alloc();

      switch (spec->type)
        {
        case VPT_MACRO:
          log_macro_expand(sb, spec->id, FALSE,
                           template_options, time_zone_mode, seq_num, NULL, msg);
          break;
        case VPT_NVPAIR:
        {
          const gchar *nv;
          gssize len;

          nv = log_msg_get_value(msg, (NVHandle) spec->id, &len);
          g_string_append_len(sb, nv, len);
          break;
        }
        default:
          g_assert_not_reached();
        }

      if (sb->len == 0)
        {
          continue;
        }

      vp_results_insert(results, vp_transform_apply(vp, spec->name), TYPE_HINT_STRING, sb);
    }
}

static gboolean
vp_foreach_helper(const gchar *name, gpointer ndx_as_pointer, gpointer data)
{
  VPResults *results = ((gpointer *)data)[0];
  gint ndx = GPOINTER_TO_INT(ndx_as_pointer);
  VPResultValue *rv = &g_array_index(results->values, VPResultValue, ndx);
  VPForeachFunc func = ((gpointer *)data)[1];
  gpointer user_data = ((gpointer *)data)[2];
  gboolean *r = ((gpointer *)data)[3];

  *r &= !func(name, rv->type_hint,
              rv->value->str,
              rv->value->len, user_data);
  return !*r;
}


gboolean
value_pairs_foreach_sorted (ValuePairs *vp, VPForeachFunc func,
                            GCompareFunc compare_func,
                            LogMessage *msg, gint32 seq_num, gint time_zone_mode,
                            const LogTemplateOptions *template_options,
                            gpointer user_data)
{
  gpointer args[] = { vp, func, msg, GINT_TO_POINTER (seq_num), user_data, NULL,
                      /* remove constness, we are not using that pointer non-const anyway */
                      (LogTemplateOptions *) template_options, GINT_TO_POINTER(time_zone_mode)
                    };
  gboolean result = TRUE;
  VPResults results;
  gpointer helper_args[] = { &results, func, user_data, &result };
  ScratchBuffersMarker mark;

  scratch_buffers_mark(&mark);
  vp_results_init(&results, compare_func);
  args[5] = &results;

  /*
   * Build up the base set
   */
  if (vp->scopes & (VPS_NV_PAIRS + VPS_DOT_NV_PAIRS + VPS_SDATA + VPS_RFC5424) ||
      vp->patterns->len > 0)
    nv_table_foreach(msg->payload, logmsg_registry,
                     (NVTableForeachFunc) vp_msg_nvpairs_foreach, args);

  vp_merge_builtins(vp, &results, msg, seq_num, time_zone_mode, template_options);

  /* Merge the explicit key-value pairs too */
  g_ptr_array_foreach(vp->vpairs, (GFunc)vp_pairs_foreach, args);

  /* Aaand we run it through the callback! */
  g_tree_foreach(results.result_tree, (GTraverseFunc)vp_foreach_helper, helper_args);
  vp_results_deinit(&results);
  scratch_buffers_reclaim_marked(mark);

  return result;
}

gboolean
value_pairs_foreach(ValuePairs *vp, VPForeachFunc func,
                    LogMessage *msg, gint32 seq_num, gint time_zone_mode,
                    const LogTemplateOptions *template_options,
                    gpointer user_data)
{
  return value_pairs_foreach_sorted(vp, func, (GCompareFunc) strcmp,
                                    msg, seq_num, time_zone_mode, template_options, user_data);
}

/*******************************************************************************
 * vp_stack (represented by vp_stack_t)
 *
 * A not very generic stack implementation used by vp_walker.
 *******************************************************************************/

#define VP_STACK_INITIAL_SIZE 16

typedef struct
{
  GPtrArray *elems;
} vp_stack_t;

static void
vp_stack_init(vp_stack_t *stack)
{
  stack->elems = g_ptr_array_sized_new(VP_STACK_INITIAL_SIZE);
}

static void
vp_stack_destroy(vp_stack_t *stack)
{
  g_ptr_array_free(stack->elems, TRUE);
}

static void
vp_stack_push(vp_stack_t *stack, gpointer data)
{
  g_ptr_array_add(stack->elems, data);
}

static gpointer
vp_stack_peek(vp_stack_t *stack)
{
  if (stack->elems->len == 0)
    return NULL;

  return g_ptr_array_index(stack->elems, stack->elems->len - 1);
}

static gpointer
vp_stack_pop(vp_stack_t *stack)
{
  gpointer data = NULL;

  data = vp_stack_peek(stack);
  if (data)
    g_ptr_array_remove_index(stack->elems, stack->elems->len - 1);
  return data;
}

static guint
vp_stack_height(vp_stack_t *stack)
{
  return stack->elems->len;
}

/*******************************************************************************
 * vp_walker (represented by vp_walk_state_t),
 *
 * The stuff that translates name-value pairs to a tree with SAX like
 * callbacks. (start/value/end)
 *******************************************************************************/

typedef struct
{
  gchar *key;
  gchar *prefix;
  gint prefix_len;

  gpointer data;
} vp_walk_stack_data_t;

typedef struct
{
  VPWalkCallbackFunc obj_start;
  VPWalkCallbackFunc obj_end;
  VPWalkValueCallbackFunc process_value;

  gpointer user_data;
  vp_stack_t stack;
} vp_walk_state_t;

static vp_walk_stack_data_t *
vp_walker_stack_push (vp_stack_t *stack,
                      gchar *key, gchar *prefix)
{
  vp_walk_stack_data_t *nt = g_new(vp_walk_stack_data_t, 1);

  nt->key = key;
  nt->prefix = prefix;
  nt->prefix_len = strlen(nt->prefix);
  nt->data = NULL;

  vp_stack_push(stack, nt);
  return nt;
}

static vp_walk_stack_data_t *
vp_walker_stack_peek(vp_stack_t *stack)
{
  return (vp_walk_stack_data_t *) vp_stack_peek(stack);
}

static vp_walk_stack_data_t *
vp_walker_stack_pop(vp_stack_t *stack)
{
  return (vp_walk_stack_data_t *) vp_stack_pop(stack);
}

static void
vp_walker_free_stack_data(vp_walk_stack_data_t *t)
{
  g_free(t->key);
  g_free(t->prefix);
  g_free(t);
}

static void
vp_walker_stack_unwind_containers_until(vp_walk_state_t *state,
                                        const gchar *name)
{
  vp_walk_stack_data_t *t;

  while ((t = vp_walker_stack_pop(&state->stack)) != NULL)
    {
      vp_walk_stack_data_t *p;

      if (name && strncmp(name, t->prefix, t->prefix_len) == 0)
        {
          /* This one matched, put it back, PUT IT BACK! */
          vp_stack_push(&state->stack, t);
          break;
        }

      p = vp_walker_stack_peek(&state->stack);

      if (p)
        state->obj_end(t->key, t->prefix, &t->data,
                       p->prefix, &p->data,
                       state->user_data);
      else
        state->obj_end(t->key, t->prefix, &t->data,
                       NULL, NULL,
                       state->user_data);
      vp_walker_free_stack_data(t);
    }
}

static void
vp_walker_stack_unwind_all_containers(vp_walk_state_t *state)
{
  vp_walker_stack_unwind_containers_until(state, NULL);
}

static const gchar *
vp_walker_skip_sdata_enterprise_id(const gchar *name)
{
  /* parse .SDATA.foo@1234.56.678 format, starting with the '@'
     character. Assume that any numbers + dots form part of the
     "foo@1234.56.678" key, even if they contain dots */
  do
    {
      /* skip @ or . */
      ++name;
      name += strspn(name, "0123456789");
    }
  while (*name == '.' && isdigit(*(name + 1)));
  return name;
}

static GPtrArray *
vp_walker_split_name_to_tokens(vp_walk_state_t *state, const gchar *name)
{
  const gchar *token_start = name;
  const gchar *token_end = name;

  GPtrArray *array = g_ptr_array_sized_new(VP_STACK_INITIAL_SIZE);

  while (*token_end)
    {
      switch (*token_end)
        {
        case '@':
          token_end = vp_walker_skip_sdata_enterprise_id(token_end);
          break;
        case '.':
          if (token_start != token_end)
            {
              g_ptr_array_add(array, g_strndup(token_start, token_end - token_start));
              ++token_end;
              token_start = token_end;
              break;
            }
        /* fall through, zero length token is not considered a separate token */
        default:
          ++token_end;
          token_end += strcspn(token_end, "@.");
          break;
        }
    }

  if (token_start != token_end)
    g_ptr_array_add(array, g_strndup(token_start, token_end - token_start));

  if (array->len == 0)
    {
      g_ptr_array_free(array, TRUE);
      return NULL;
    }

  return array;
}

static gchar *
vp_walker_name_combine_prefix(GPtrArray *tokens, gint until)
{
  GString *s = scratch_buffers_alloc();
  gchar *str;
  gint i;

  for (i = 0; i < until; i++)
    {
      g_string_append(s, g_ptr_array_index(tokens, i));
      g_string_append_c(s, '.');
    }
  g_string_append(s, g_ptr_array_index(tokens, until));

  str = g_strdup(s->str);
  return str;
}

static gchar *
vp_walker_start_containers_for_name(vp_walk_state_t *state,
                                    const gchar *name)
{
  GPtrArray *tokens;
  gchar *key = NULL;
  guint i, start;

  tokens = vp_walker_split_name_to_tokens(state, name);

  start = vp_stack_height(&state->stack);
  for (i = start; i < tokens->len - 1; i++)
    {
      vp_walk_stack_data_t *p, *nt;

      p = vp_walker_stack_peek(&state->stack);
      nt = vp_walker_stack_push(&state->stack,
                                g_strdup(g_ptr_array_index(tokens, i)),
                                vp_walker_name_combine_prefix(tokens, i));

      if (p)
        state->obj_start(nt->key, nt->prefix, &nt->data,
                         p->prefix, &p->data,
                         state->user_data);
      else
        state->obj_start(nt->key, nt->prefix, &nt->data,
                         NULL, NULL, state->user_data);
    }

  /* The last token is the key (well, second to last, last being
     NULL), so treat that normally. */
  key = g_ptr_array_index(tokens, tokens->len - 1);
  g_ptr_array_index(tokens, tokens->len - 1) = NULL;

  g_ptr_array_foreach(tokens, (GFunc)g_free, NULL);
  g_ptr_array_free(tokens, TRUE);

  return key;
}

static gboolean
value_pairs_walker(const gchar *name, TypeHint type, const gchar *value, gsize value_len,
                   gpointer user_data)
{
  vp_walk_state_t *state = (vp_walk_state_t *)user_data;
  vp_walk_stack_data_t *data;
  gchar *key;
  gboolean result;

  vp_walker_stack_unwind_containers_until(state, name);
  key = vp_walker_start_containers_for_name(state, name);
  data = vp_walker_stack_peek(&state->stack);

  if (data != NULL)
    result = state->process_value(key, data->prefix,
                                  type, value, value_len,
                                  &data->data,
                                  state->user_data);
  else
    result = state->process_value(key, NULL,
                                  type, value, value_len,
                                  NULL,
                                  state->user_data);

  g_free(key);

  return result;
}

static gint
vp_walk_cmp(const gchar *s1, const gchar *s2)
{
  return strcmp(s2, s1);
}

/*******************************************************************************
 * Public API
 *******************************************************************************/

gboolean
value_pairs_walk(ValuePairs *vp,
                 VPWalkCallbackFunc obj_start_func,
                 VPWalkValueCallbackFunc process_value_func,
                 VPWalkCallbackFunc obj_end_func,
                 LogMessage *msg, gint32 seq_num, gint time_zone_mode,
                 const LogTemplateOptions *template_options,
                 gpointer user_data)
{
  vp_walk_state_t state;
  gboolean result;

  state.user_data = user_data;
  state.obj_start = obj_start_func;
  state.obj_end = obj_end_func;
  state.process_value = process_value_func;
  vp_stack_init(&state.stack);

  state.obj_start(NULL, NULL, NULL, NULL, NULL, user_data);
  result = value_pairs_foreach_sorted(vp, value_pairs_walker,
                                      (GCompareFunc)vp_walk_cmp, msg,
                                      seq_num, time_zone_mode, template_options, &state);
  vp_walker_stack_unwind_all_containers(&state);
  state.obj_end(NULL, NULL, NULL, NULL, NULL, user_data);
  vp_stack_destroy(&state.stack);

  return result;
}

gboolean
value_pairs_add_scope(ValuePairs *vp, const gchar *scope)
{
  gboolean result;

  if (strcmp(scope, "none") != 0)
    {
      result = cfg_process_flag(value_pair_scope, vp, scope);
      vp_update_builtin_list_of_values(vp);
    }
  else
    {
      result = TRUE;
      vp->scopes = 0;
      vp_update_builtin_list_of_values(vp);
    }
  return result;
}

void
value_pairs_add_glob_pattern(ValuePairs *vp, const gchar *pattern,
                             gboolean include)
{
  g_ptr_array_add(vp->patterns, vp_pattern_spec_new(pattern, include));
  vp_update_builtin_list_of_values(vp);
}

void
value_pairs_add_glob_patterns(ValuePairs *vp, GList *patterns, gboolean include)
{
  GList *l = patterns;

  while (l)
    {
      value_pairs_add_glob_pattern(vp, (gchar *)l->data, include);
      l = g_list_next (l);
    }
  string_list_free(patterns);
  vp_update_builtin_list_of_values(vp);
}

void
value_pairs_add_pair(ValuePairs *vp, const gchar *key, LogTemplate *value)
{
  g_ptr_array_add(vp->vpairs, vp_pair_conf_new(key, value));
  vp_update_builtin_list_of_values(vp);
}

void
value_pairs_add_transforms(ValuePairs *vp, ValuePairsTransformSet *vpts)
{
  g_ptr_array_add(vp->transforms, vpts);
  vp_update_builtin_list_of_values(vp);
}

ValuePairs *
value_pairs_new(void)
{
  ValuePairs *vp;

  vp = g_new0(ValuePairs, 1);
  g_atomic_counter_set(&vp->ref_cnt, 1);
  vp->builtins = g_ptr_array_new();
  vp->vpairs = g_ptr_array_new();
  vp->patterns = g_ptr_array_new();
  vp->transforms = g_ptr_array_new();

  return vp;
}

ValuePairs *
value_pairs_new_default(GlobalConfig *cfg)
{
  ValuePairs *vp = value_pairs_new();

  value_pairs_add_scope(vp, "selected-macros");
  value_pairs_add_scope(vp, "nv-pairs");
  value_pairs_add_scope(vp, "sdata");
  return vp;
}

static void
value_pairs_free (ValuePairs *vp)
{
  guint i;

  for (i = 0; i < vp->vpairs->len; i++)
         vp_pair_conf_free(g_ptr_array_index(vp->vpairs, i));

  g_ptr_array_free(vp->vpairs, TRUE);

  for (i = 0; i < vp->patterns->len; i++)
    {
      VPPatternSpec *vps = (VPPatternSpec *) g_ptr_array_index(vp->patterns, i);
      vp_pattern_spec_free(vps);
    }
  g_ptr_array_free(vp->patterns, TRUE);

  for (i = 0; i < vp->transforms->len; i++)
    {
      ValuePairsTransformSet *vpts = (ValuePairsTransformSet *) g_ptr_array_index(vp->transforms, i);
      value_pairs_transform_set_free(vpts);
    }
  g_ptr_array_free(vp->transforms, TRUE);
  g_ptr_array_free(vp->builtins, TRUE);
  g_free(vp);
}

ValuePairs *
value_pairs_ref(ValuePairs *self)
{
  g_assert(!self || g_atomic_counter_get(&self->ref_cnt) > 0);

  if (self)
    {
      g_atomic_counter_inc(&self->ref_cnt);
    }
  return self;
}

void
value_pairs_unref(ValuePairs *self)
{
  if (self)
    {
      g_assert(g_atomic_counter_get(&self->ref_cnt) > 0);

      if (g_atomic_counter_dec_and_test(&self->ref_cnt))
        value_pairs_free(self);
    }
}

static void
value_pairs_init_set(ValuePairSpec *set)
{
  gint i;

  for (i = 0; set[i].name; i++)
    {
      guint id;
      const gchar *name;

      name = set[i].alt_name ? set[i].alt_name : set[i].name;

      if ((id = log_macro_lookup(name, strlen(name))))
        {
          set[i].type = VPT_MACRO;
          set[i].id = id;
        }
      else
        {
          set[i].type = VPT_NVPAIR;
          set[i].id = log_msg_get_value_handle(name);
        }
    }
}

void
value_pairs_global_init(void)
{
  gint i = 0;
  GArray *a;

  value_pairs_init_set(rfc3164);
  value_pairs_init_set(rfc5424);
  value_pairs_init_set(selected_macros);

  a = g_array_new(TRUE, TRUE, sizeof(ValuePairSpec));
  for (i = 0; macros[i].name; i++)
    {
      ValuePairSpec pair;

      pair.name = macros[i].name;
      pair.type = VPT_MACRO;
      pair.id = macros[i].id;
      g_array_append_val(a, pair);
    }
  all_macros = (ValuePairSpec *) g_array_free(a, FALSE);
}

void
value_pairs_global_deinit(void)
{
  g_free(all_macros);
}
