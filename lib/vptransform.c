/*
 * Copyright (c) 2002-2011 BalaBit IT Ltd, Budapest, Hungary
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
#include "template/templates.h"
#include "cfg-parser.h"
#include "misc.h"

#include <string.h>

typedef gboolean (*VPTransMatchFunc)(ValuePairsTransform *t, gchar *name);
typedef gchar *(*VPTransFunc)(ValuePairsTransform *t, gchar *name);
typedef void (*VPTransResetFunc)(ValuePairsTransform *t);
typedef void (*VPTransDestroyFunc)(ValuePairsTransform *t);

struct _ValuePairsTransformSet
{
  GPatternSpec *pattern;

  GList *transforms;
};

struct _ValuePairsTransform
{
  VPTransFunc transform;
  VPTransResetFunc reset;
  VPTransDestroyFunc destroy;
};

typedef struct
{
  ValuePairsTransform super;

  gint amount;
} VPTransShift;

typedef struct
{
  ValuePairsTransform super;

  gchar *prefix;

  GHashTable *cache;
} VPTransAddPrefix;

typedef struct
{
  ValuePairsTransform super;

  gchar *old_prefix;
  gchar *new_prefix;
  gint new_prefix_len;
  gint old_prefix_len;

  GHashTable *cache;
} VPTransReplace;

static void
vp_trans_init(ValuePairsTransform *t,
	      VPTransFunc trans,
	      VPTransResetFunc reset, VPTransDestroyFunc dest)
{
  if (!t)
    return;

  t->transform = trans;
  t->reset = reset;
  t->destroy = dest;
}

static inline void
value_pairs_transform_reset(ValuePairsTransform *t)
{
  if (t->reset)
    t->reset(t);
}

static inline void
value_pairs_transform_free(ValuePairsTransform *t)
{
  if (t->destroy)
    t->destroy(t);
  g_free(t);
}

static inline gchar *
value_pairs_transform_apply(ValuePairsTransform *t, gchar *key)
{
  return t->transform(t, key);
}

/* add_prefix() */

static gchar *
vp_trans_add_prefix(ValuePairsTransform *t, gchar *name)
{
  VPTransAddPrefix *self = (VPTransAddPrefix *)t;
  gpointer r;

  r = g_hash_table_lookup(self->cache, name);
  if (!r)
    {
      r = (gpointer)g_strconcat(self->prefix, name, NULL);
      g_hash_table_insert(self->cache, g_strdup(name), r);
    }
  return r;
}

static void
vp_trans_add_prefix_destroy(ValuePairsTransform *t)
{
  VPTransAddPrefix *self = (VPTransAddPrefix *)t;

  g_hash_table_destroy(self->cache);
  g_free(self->prefix);
}

static void
vp_trans_add_prefix_reset(ValuePairsTransform *t)
{
  VPTransAddPrefix *self = (VPTransAddPrefix *)t;

  if (self->cache)
    g_hash_table_destroy(self->cache);

  self->cache = g_hash_table_new_full(g_str_hash, g_str_equal,
				      g_free, g_free);
}

ValuePairsTransform *
value_pairs_new_transform_add_prefix (const gchar *prefix)
{
  VPTransAddPrefix *vpt;

  vpt = g_new(VPTransAddPrefix, 1);
  vp_trans_init((ValuePairsTransform *)vpt,
		vp_trans_add_prefix,
		vp_trans_add_prefix_reset, vp_trans_add_prefix_destroy);
  vpt->prefix = g_strdup(prefix);
  vpt->cache = NULL;

  vp_trans_add_prefix_reset((ValuePairsTransform *)vpt);

  return (ValuePairsTransform *)vpt;
}

/* shift() */

static gchar *
vp_trans_shift(ValuePairsTransform *t, gchar* name)
{
  VPTransShift *self = (VPTransShift *)t;

  if (self->amount <= 0)
    return name;
  return name + self->amount;
}

ValuePairsTransform *
value_pairs_new_transform_shift (gint amount)
{
  VPTransShift *vpt;

  vpt = g_new(VPTransShift, 1);
  vp_trans_init((ValuePairsTransform *)vpt,
		vp_trans_shift,
		NULL, NULL);

  vpt->amount = amount;

  return (ValuePairsTransform *)vpt;
}

/* replace() */

static gchar *
vp_trans_replace(ValuePairsTransform *t, gchar *name)
{
  VPTransReplace *self = (VPTransReplace *)t;
  gpointer r;

  if (strncmp (self->old_prefix, name, self->old_prefix_len) != 0)
    return name;

  r = g_hash_table_lookup(self->cache, name);
  if (!r)
    {
      r = g_malloc(strlen(name) - self->old_prefix_len + self->new_prefix_len + 1);

      memcpy (r, self->new_prefix, self->new_prefix_len);
      memcpy (r + self->new_prefix_len, name + self->old_prefix_len,
	      strlen(name) - self->old_prefix_len + 1);
      g_hash_table_insert(self->cache, g_strdup(name), r);
    }
  return r;
}

static void
vp_trans_replace_destroy(ValuePairsTransform *t)
{
  VPTransReplace *self = (VPTransReplace *)t;

  g_hash_table_destroy(self->cache);
  g_free(self->old_prefix);
  g_free(self->new_prefix);
}

static void
vp_trans_replace_reset(ValuePairsTransform *t)
{
  VPTransReplace *self = (VPTransReplace *)t;

  if (self->cache)
    g_hash_table_destroy(self->cache);
  self->cache = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
}

ValuePairsTransform *
value_pairs_new_transform_replace(const gchar *prefix, const gchar *new_prefix)
{
  VPTransReplace *vpt;

  vpt = g_new(VPTransReplace, 1);
  vp_trans_init((ValuePairsTransform *)vpt,
		vp_trans_replace,
		vp_trans_replace_reset, vp_trans_replace_destroy);

  vpt->old_prefix = g_strdup(prefix);
  vpt->old_prefix_len = strlen(prefix);
  vpt->new_prefix = g_strdup(new_prefix);
  vpt->new_prefix_len = strlen(vpt->new_prefix);
  vpt->cache = NULL;

  vp_trans_replace_reset ((ValuePairsTransform *)vpt);

  return (ValuePairsTransform *)vpt;
}

/*
 * ValuePairsTransformSet
 */

ValuePairsTransformSet *
value_pairs_transform_set_new(const gchar *glob)
{
  ValuePairsTransformSet *vpts;

  vpts = g_new(ValuePairsTransformSet, 1);
  vpts->transforms = NULL;
  vpts->pattern = g_pattern_spec_new(glob);

  return vpts;
}

void
value_pairs_transform_set_add_func(ValuePairsTransformSet *vpts,
                                   ValuePairsTransform *vpt)
{
  vpts->transforms = g_list_append(vpts->transforms, vpt);
}

void
value_pairs_transform_set_free(ValuePairsTransformSet *vpts)
{
  GList *l;

  l = vpts->transforms;
  while (l)
    {
      value_pairs_transform_free((ValuePairsTransform *)l->data);
      l = g_list_delete_link(l, l);
    }
  g_pattern_spec_free(vpts->pattern);
  g_free(vpts);
}

void
value_pairs_transform_set_reset(ValuePairsTransformSet *vpts)
{
  GList *l;

  l = vpts->transforms;
  while (l)
    {
      value_pairs_transform_reset((ValuePairsTransform *)l->data);
      l = l->next;
    }
}

gchar *
value_pairs_transform_set_apply(ValuePairsTransformSet *vpts, gchar *key)
{
  gchar *new_key = key;

  if (g_pattern_match_string(vpts->pattern, key))
    {
      GList *l;

      l = vpts->transforms;
      while (l)
        {
          new_key = value_pairs_transform_apply((ValuePairsTransform *)l->data,
                                                new_key);
          l = l->next;
        }
    }
  return new_key;
}
