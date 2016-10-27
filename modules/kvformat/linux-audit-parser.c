/*
 * Copyright (c) 2015-2016 Balabit
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
 */
#include "linux-audit-parser.h"
#include "linux-audit-scanner.h"

static gboolean
_init_linux_audit(LogPipe *s)
{
  KVParser *self = (KVParser *)s;
  g_assert(self->kv_scanner == NULL);

  if (!kv_parser_init_method(s))
    return FALSE;
  kv_scanner_set_parse_value(self->kv_scanner, parse_linux_audit_style_hexdump);

  return TRUE;
}

static LogPipe *
_clone_linux_audit(LogPipe *s)
{
  KVParser *self = (KVParser *) s;
  LogParser *cloned = linux_audit_parser_new(s->cfg);

  return kv_parser_clone_fields(cloned, self);
}

LogParser *
linux_audit_parser_new(GlobalConfig *cfg)
{
  KVParser *self = kv_parser_init_instance(cfg);

  self->super.super.init = _init_linux_audit;
  self->super.super.clone = _clone_linux_audit;

  return &self->super;
}
