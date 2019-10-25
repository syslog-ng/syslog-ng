/*
 * Copyright (c) 2019 Balabit
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

#include "cfg-walker.h"

Arc *
arc_new(LogPipe *from, LogPipe *to, ArcType arc_type)
{
  Arc *self = g_new0(Arc, 1);
  self->from = from;
  self->to = to;
  self->arc_type = arc_type;

  return self;
};

void
arc_free(Arc *self)
{
  g_free(self);
}

static guint
arc_hash(Arc *arc)
{
  return g_direct_hash(arc->from);
}

static gboolean
arc_equal(Arc *arc1, Arc *arc2)
{
  return arc1->to == arc2->to;
}

void
cfg_walker_get_graph(GPtrArray *start_nodes, GHashTable **nodes, GHashTable **arcs)
{
  *nodes = g_hash_table_new(g_direct_hash, g_direct_equal);
  *arcs = g_hash_table_new_full((GHashFunc)arc_hash, (GEqualFunc)arc_equal, (GDestroyNotify)arc_free, NULL);
  return;
}
