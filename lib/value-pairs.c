/*
 * Copyright (c) 2011 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2011 Gergely Nagy <algernon@balabit.hu>
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

#include "value-pairs.h"
#include "vptransform.h"
#include "logmsg.h"
#include "templates.h"
#include "cfg-parser.h"
#include "misc.h"
#include "scratch-buffers.h"

#include <stdlib.h>
#include <string.h>

typedef struct
{
  GPatternSpec *pattern;
  gboolean include;
} VPPatternSpec;

struct _ValuePairs
{
  VPPatternSpec **patterns;
  GHashTable *vpairs;
  GList *transforms;

  /* guint32 as CfgFlagHandler only supports 32 bit integers */
  guint32 scopes;
  guint32 patterns_size;
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
  gchar *name;
  gchar *alt_name;
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
  { "DATE", "R_DATE" },
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
static gboolean value_pair_sets_initialized;

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
};

gboolean
value_pairs_add_scope(ValuePairs *vp, const gchar *scope)
{
  return cfg_process_flag(value_pair_scope, vp, scope);
}

void
value_pairs_add_glob_pattern(ValuePairs *vp, const gchar *pattern,
                             gboolean include)
{
  gint i;
  VPPatternSpec *p;

  i = vp->patterns_size++;
  vp->patterns = g_renew(VPPatternSpec *, vp->patterns, vp->patterns_size);

  p = g_new(VPPatternSpec, 1);
  p->pattern = g_pattern_spec_new(pattern);
  p->include = include;

  vp->patterns[i] = p;
}

void
value_pairs_add_pair(ValuePairs *vp, GlobalConfig *cfg, const gchar *key, const gchar *value)
{
  LogTemplate *t = NULL;

  t = log_template_new(cfg, NULL);
  log_template_compile(t, value, NULL);
  g_hash_table_insert(vp->vpairs, g_strdup(key), t);
}

static gchar *
vp_transform_apply (ValuePairs *vp, gchar *key)
{
  GList *l;
  gchar *ckey, *okey = g_strdup(key);

  if (!vp->transforms)
    return okey;

  l = vp->transforms;
  while (l)
    {
      ValuePairsTransformSet *t = (ValuePairsTransformSet *)l->data;
      ckey = value_pairs_transform_set_apply(t, okey);
      g_free(okey);
      okey = ckey;
      l = g_list_next (l);
    }

  return ckey;
}

/* runs over the name-value pairs requested by the user (e.g. with value_pairs_add_pair) */
static void
vp_pairs_foreach(gpointer key, gpointer value, gpointer user_data)
{
  ValuePairs *vp = ((gpointer *)user_data)[0];
  LogMessage *msg = ((gpointer *)user_data)[2];
  gint32 seq_num = GPOINTER_TO_INT (((gpointer *)user_data)[3]);
  GHashTable *scope_set = ((gpointer *)user_data)[5];
  ScratchBuffer *sb = scratch_buffer_acquire();

  log_template_format((LogTemplate *)value, msg, NULL, LTZ_LOCAL,
                      seq_num, NULL, sb_string(sb));

  if (!sb_string(sb)->str[0])
    {
      scratch_buffer_release(sb);
      return;
    }

  g_hash_table_insert(scope_set, vp_transform_apply (vp, key), sb_string(sb)->str);
  g_string_steal(sb_string(sb));
  scratch_buffer_release(sb);
}

/* runs over the LogMessage nv-pairs, and inserts them unless excluded */
static gboolean
vp_msg_nvpairs_foreach(NVHandle handle, gchar *name,
                       const gchar *value, gssize value_len,
                       gpointer user_data)
{
  ValuePairs *vp = ((gpointer *)user_data)[0];
  GHashTable *scope_set = ((gpointer *)user_data)[5];
  gint j;
  gboolean inc = FALSE;

  for (j = 0; j < vp->patterns_size; j++)
    {
      if (g_pattern_match_string(vp->patterns[j]->pattern, name))
        inc = vp->patterns[j]->include;
    }

  /* NOTE: dot-nv-pairs include SDATA too */
  if (((name[0] == '.' && (vp->scopes & VPS_DOT_NV_PAIRS)) ||
       (name[0] != '.' && (vp->scopes & VPS_NV_PAIRS)) ||
       (log_msg_is_handle_sdata(handle) && (vp->scopes & VPS_SDATA))) ||
      inc)
    {
      /* NOTE: the key is a borrowed reference in the hash, and value is freed */
      g_hash_table_insert(scope_set, vp_transform_apply(vp, name), g_strndup(value, value_len));
    }

  return FALSE;
}

/* runs over a set of ValuePairSpec structs and merges them into the value-pair set */
static void
vp_merge_set(ValuePairs *vp, LogMessage *msg, gint32 seq_num, ValuePairSpec *set, GHashTable *dest)
{
  gint i;
  ScratchBuffer *sb = scratch_buffer_acquire();

  for (i = 0; set[i].name; i++)
    {
      gint j;
      gboolean exclude = FALSE;

      for (j = 0; j < vp->patterns_size; j++)
        {
          if (g_pattern_match_string(vp->patterns[j]->pattern, set[i].name))
            exclude = !vp->patterns[j]->include;
        }

      if (exclude)
	continue;

      switch (set[i].type)
        {
        case VPT_MACRO:
          log_macro_expand(sb_string(sb), set[i].id, FALSE, NULL, LTZ_LOCAL, seq_num, NULL, msg);
          break;
        case VPT_NVPAIR:
          {
            const gchar *nv;
            gssize len;

            nv = log_msg_get_value(msg, (NVHandle) set[i].id, &len);
            g_string_append_len(sb_string(sb), nv, len);
            break;
          }
        default:
          g_assert_not_reached();
        }

      if (!sb_string(sb)->str[0])
	continue;

      g_hash_table_insert(dest, vp_transform_apply(vp, set[i].name), sb_string(sb)->str);
      g_string_steal(sb_string(sb));
    }
  scratch_buffer_release(sb);
}

void
value_pairs_foreach (ValuePairs *vp, VPForeachFunc func,
		     LogMessage *msg, gint32 seq_num, gpointer user_data)
{
  gpointer args[] = { vp, func, msg, GINT_TO_POINTER (seq_num), user_data, NULL };
  GHashTable *scope_set;

  scope_set = g_hash_table_new_full(g_str_hash, g_str_equal,
                                    (GDestroyNotify) g_free,
				    (GDestroyNotify) g_free);

  args[5] = scope_set;

  /*
   * Build up the base set
   */
  if (vp->scopes & (VPS_NV_PAIRS + VPS_DOT_NV_PAIRS + VPS_SDATA) ||
      vp->patterns_size > 0)
    nv_table_foreach(msg->payload, logmsg_registry,
                     (NVTableForeachFunc) vp_msg_nvpairs_foreach, args);

  if (vp->scopes & (VPS_RFC3164 + VPS_RFC5424 + VPS_SELECTED_MACROS))
    vp_merge_set(vp, msg, seq_num, rfc3164, scope_set);

  if (vp->scopes & VPS_RFC5424)
    vp_merge_set(vp, msg, seq_num, rfc5424, scope_set);

  if (vp->scopes & VPS_SELECTED_MACROS)
    vp_merge_set(vp, msg, seq_num, selected_macros, scope_set);

  if (vp->scopes & VPS_ALL_MACROS)
    vp_merge_set(vp, msg, seq_num, all_macros, scope_set);

  /* Merge the explicit key-value pairs too */
  g_hash_table_foreach(vp->vpairs, (GHFunc) vp_pairs_foreach, args);

  /* Aaand we run it through the callback! */
  g_hash_table_foreach(scope_set, (GHFunc)func, user_data);

  g_hash_table_destroy(scope_set);
}


static void
value_pairs_init_set(ValuePairSpec *set)
{
  gint i;

  for (i = 0; set[i].name; i++)
    {
      guint id;
      gchar *name;

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

ValuePairs *
value_pairs_new(void)
{
  ValuePairs *vp;
  gint i = 0;
  GArray *a;

  vp = g_new0(ValuePairs, 1);
  vp->vpairs = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
				     (GDestroyNotify) log_template_unref);

  if (!value_pair_sets_initialized)
    {

      /* NOTE: that we're being only called during config parsing,
       * thus this code is never threaded. And we only want to perform
       * it once anyway. If it would be threaded, we'd need to convert
       * this to a value_pairs_init() function called before anything
       * else. */

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

      value_pair_sets_initialized = TRUE;
    }

  return vp;
}

void
value_pairs_free (ValuePairs *vp)
{
  gint i;
  GList *l;

  g_hash_table_destroy(vp->vpairs);

  for (i = 0; i < vp->patterns_size; i++)
    {
      g_pattern_spec_free(vp->patterns[i]->pattern);
      g_free(vp->patterns[i]);
    }
  g_free(vp->patterns);

  l = vp->transforms;
  while (l)
    {
      value_pairs_transform_set_free((ValuePairsTransformSet *)l->data);

      l = g_list_delete_link (l, l);
    }

  g_free(vp);
}

void
value_pairs_add_transforms(ValuePairs *vp, gpointer *vpts)
{
  vp->transforms = g_list_append(vp->transforms, vpts);
}

static void
vp_cmdline_parse_rekey_finish (gpointer data)
{
  gpointer *args = (gpointer *) data;
  ValuePairs *vp = (ValuePairs *) args[1];
  ValuePairsTransformSet *vpts = (ValuePairsTransformSet *) args[2];

  if (vpts)
    value_pairs_add_transforms (vp, args[2]);
  args[2] = NULL;
  g_free(args[3]);
  args[3] = NULL;
}

/* parse a value-pair specification from a command-line like environment */
static gboolean
vp_cmdline_parse_scope(const gchar *option_name, const gchar *value,
                       gpointer data, GError **error)
{
  gpointer *args = (gpointer *) data;
  ValuePairs *vp = (ValuePairs *) args[1];

  vp_cmdline_parse_rekey_finish (data);

  if (!value_pairs_add_scope (vp, value))
    {
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		   "Unknon value-pairs scope, scope=%s", value);
      return FALSE;
    }
  return TRUE;
}

static gboolean
vp_cmdline_parse_exclude(const gchar *option_name, const gchar *value,
                         gpointer data, GError **error)
{
  gpointer *args = (gpointer *) data;
  ValuePairs *vp = (ValuePairs *) args[1];

  vp_cmdline_parse_rekey_finish (data);
  value_pairs_add_glob_pattern(vp, value, FALSE);
  return TRUE;
}

static gboolean
vp_cmdline_parse_key(const gchar *option_name, const gchar *value,
		      gpointer data, GError **error)
{
  gpointer *args = (gpointer *) data;
  ValuePairs *vp = (ValuePairs *) args[1];

  vp_cmdline_parse_rekey_finish (data);
  value_pairs_add_glob_pattern(vp, value, TRUE);
  args[3] = g_strdup(value);
  return TRUE;
}

static gboolean
vp_cmdline_parse_pair (const gchar *option_name, const gchar *value,
		       gpointer data, GError **error)
{
  gpointer *args = (gpointer *) data;
  ValuePairs *vp = (ValuePairs *) args[1];
  GlobalConfig *cfg = (GlobalConfig *) args[0];
  gchar **kv;

  vp_cmdline_parse_rekey_finish (data);
  if (!g_strstr_len (value, strlen (value), "="))
    {
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		   "Error parsing value-pairs' key=value pair");
      return FALSE;
    }

  kv = g_strsplit(value, "=", 2);
  value_pairs_add_pair (vp, cfg, kv[0], kv[1]);

  g_free (kv[0]);
  g_free (kv[1]);
  g_free (kv);

  return TRUE;
}

static gboolean
vp_cmdline_parse_rekey (const gchar *option_name, const gchar *value,
                        gpointer data, GError **error)
{
  gpointer *args = (gpointer *) data;
  ValuePairsTransformSet *vpts = (ValuePairsTransformSet *) args[2];
  gchar *key = (gchar *) args[3];

  vpts = value_pairs_transform_set_new (key);
  vp_cmdline_parse_rekey_finish (data);

  args[2] = vpts;
  return TRUE;
}

static gboolean
vp_cmdline_parse_rekey_replace (const gchar *option_name, const gchar *value,
                                gpointer data, GError **error)
{
  gpointer *args = (gpointer *) data;
  ValuePairsTransformSet *vpts = (ValuePairsTransformSet *) args[2];
  gchar **kv;

  if (!vpts)
    {
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                   "Error parsing value-pairs: --replace used without --rekey");
      return FALSE;
    }

  if (!g_strstr_len (value, strlen (value), "="))
    {
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                   "Error parsing value-pairs' rekey replace construct");
      return FALSE;
    }

  kv = g_strsplit(value, "=", 2);
  value_pairs_transform_set_add_func
    (vpts, value_pairs_new_transform_replace (kv[0], kv[1]));

  g_free (kv[0]);
  g_free (kv[1]);
  g_free (kv);

  return TRUE;
}

static gboolean
vp_cmdline_parse_rekey_add_prefix (const gchar *option_name, const gchar *value,
                                   gpointer data, GError **error)
{
  gpointer *args = (gpointer *) data;
  ValuePairsTransformSet *vpts = (ValuePairsTransformSet *) args[2];

  if (!vpts)
    {
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                   "Error parsing value-pairs: --add-prefix used without --rekey");
      return FALSE;
    }

  value_pairs_transform_set_add_func
    (vpts, value_pairs_new_transform_add_prefix (value));
  return TRUE;
}

static gboolean
vp_cmdline_parse_rekey_shift (const gchar *option_name, const gchar *value,
                              gpointer data, GError **error)
{
  gpointer *args = (gpointer *) data;
  ValuePairsTransformSet *vpts = (ValuePairsTransformSet *) args[2];

  if (!vpts)
    {
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                   "Error parsing value-pairs: --shift used without --rekey");
      return FALSE;
    }

  value_pairs_transform_set_add_func
    (vpts, value_pairs_new_transform_shift (atoi (value)));
  return TRUE;
}

ValuePairs *
value_pairs_new_from_cmdline (GlobalConfig *cfg,
			      gint argc, gchar **argv,
			      GError **error)
{
  ValuePairs *vp;
  GOptionContext *ctx;

  GOptionEntry vp_options[] = {
    { "scope", 's', 0, G_OPTION_ARG_CALLBACK, vp_cmdline_parse_scope,
      NULL, NULL },
    { "exclude", 'x', 0, G_OPTION_ARG_CALLBACK, vp_cmdline_parse_exclude,
      NULL, NULL },
    { "key", 'k', 0, G_OPTION_ARG_CALLBACK, vp_cmdline_parse_key,
      NULL, NULL },
    { "pair", 'p', 0, G_OPTION_ARG_CALLBACK, vp_cmdline_parse_pair,
      NULL, NULL },
    { "rekey", 'r', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
      vp_cmdline_parse_rekey, NULL, NULL },
    { "shift", 'S', 0, G_OPTION_ARG_CALLBACK, vp_cmdline_parse_rekey_shift,
      NULL, NULL },
    { "add-prefix", 'A', 0, G_OPTION_ARG_CALLBACK, vp_cmdline_parse_rekey_add_prefix,
      NULL, NULL },
    { "replace", 'R', 0, G_OPTION_ARG_CALLBACK, vp_cmdline_parse_rekey_replace,
      NULL, NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_CALLBACK, vp_cmdline_parse_pair,
      NULL, NULL },
    { NULL }
  };
  GOptionGroup *og;
  gpointer user_data_args[4];

  vp = value_pairs_new();
  user_data_args[0] = cfg;
  user_data_args[1] = vp;
  user_data_args[2] = NULL;
  user_data_args[3] = NULL;

  ctx = g_option_context_new ("value-pairs");
  og = g_option_group_new (NULL, NULL, NULL, user_data_args, NULL);
  g_option_group_add_entries (og, vp_options);
  g_option_context_set_main_group (ctx, og);

  if (!g_option_context_parse (ctx, &argc, &argv, error))
    {
      g_free(user_data_args[3]);
      value_pairs_free (vp);
      vp = NULL;
    }
  g_option_context_free (ctx);
  vp_cmdline_parse_rekey_finish (user_data_args);

  return vp;
}
