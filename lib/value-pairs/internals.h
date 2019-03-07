/*
 * Copyright (c) 2011-2014 Balabit
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

#ifndef VALUE_PAIRS_INTERNALS_H_INCLUDED
#define VALUE_PAIRS_INTERNALS_H_INCLUDED 1

#include "syslog-ng.h"
#include "atomic.h"

typedef struct _ValuePairs ValuePairs;
struct _ValuePairs
{
  GAtomicCounter ref_cnt;
  GPtrArray *builtins;
  GPtrArray *patterns;
  GPtrArray *vpairs;
  GPtrArray *transforms;

  gboolean omit_empty_values;

  /* guint32 as CfgFlagHandler only supports 32 bit integers */
  guint32 scopes;
};


#endif
