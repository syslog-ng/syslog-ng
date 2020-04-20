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
 */

#ifndef SNMPTRAPD_NV_CONTEXT_H_INCLUDED
#define SNMPTRAPD_NV_CONTEXT_H_INCLUDED

typedef struct _SnmpTrapdNVContext SnmpTrapdNVContext;

struct _SnmpTrapdNVContext
{
  GString *key_prefix;
  LogMessage *msg;
  GString *generated_message;

  void (*add_name_value)(SnmpTrapdNVContext *nv_context, const gchar *key, const gchar *value, gsize value_length);
};

static inline void
snmptrapd_nv_context_add_name_value(SnmpTrapdNVContext *nv_context, const gchar *key,
                                    const gchar *value, gsize value_length)
{
  nv_context->add_name_value(nv_context, key, value, value_length);
}

#endif
