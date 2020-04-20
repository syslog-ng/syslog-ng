/*
 * Copyright (c) 2017 Balabit
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#ifndef VARBINDLIST_SCANNER_H_INCLUDED
#define VARBINDLIST_SCANNER_H_INCLUDED

#include "scanner/kv-scanner/kv-scanner.h"

typedef struct _VarBindListScanner VarBindListScanner;

struct _VarBindListScanner
{
  KVScanner super;
  GString *varbind_type;
};

static inline void
varbindlist_scanner_input(VarBindListScanner *self, const gchar *input)
{
  kv_scanner_input(&self->super, input);
}

static inline const gchar *
varbindlist_scanner_get_current_key(VarBindListScanner *self)
{
  return kv_scanner_get_current_key(&self->super);
}

static inline const gchar *
varbindlist_scanner_get_current_type(VarBindListScanner *self)
{
  return self->varbind_type->str;
}

static inline const gchar *
varbindlist_scanner_get_current_value(VarBindListScanner *self)
{
  return kv_scanner_get_current_value(&self->super);
}

gboolean varbindlist_scanner_scan_next(VarBindListScanner *self);
VarBindListScanner *varbindlist_scanner_new(void);

void varbindlist_scanner_init(VarBindListScanner *self);
void varbindlist_scanner_deinit(VarBindListScanner *self);

static inline void
varbindlist_scanner_free(VarBindListScanner *self)
{
  varbindlist_scanner_deinit(self);
  g_free(self);
}

#endif
