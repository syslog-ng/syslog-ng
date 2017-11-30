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
#ifndef VALUE_PAIRS_CMDLINE_H_INCLUDED
#define VALUE_PAIRS_CMDLINE_H_INCLUDED 1

#include "value-pairs/value-pairs.h"

ValuePairs *value_pairs_new_from_cmdline(GlobalConfig *cfg,
					 gint *argc, gchar ***argv,
					 gboolean ignore_unknown_options,
					 GError **error);

#endif
