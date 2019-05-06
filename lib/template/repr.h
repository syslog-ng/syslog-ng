/*
 * Copyright (c) 2002-2014 Balabit
 * Copyright (c) 1998-2014 Balázs Scheidler
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

#ifndef TEMPLATE_REPR_H_INCLUDED
#define TEMPLATE_REPR_H_INCLUDED

#include "template/function.h"
#include "logmsg/logmsg.h"

enum
{
  LTE_MACRO,
  LTE_VALUE,
  LTE_FUNC
};

typedef struct _LogTemplateElem
{
  gsize text_len;
  gchar *text;
  gchar *default_value;
  guint16 msg_ref;
  guint8 type;
  union
  {
    guint macro;
    NVHandle value_handle;
    struct
    {
      LogTemplateFunction *ops;
      gpointer state;
    } func;
  };
} LogTemplateElem;


LogTemplateElem *log_template_elem_new_macro(const gchar *text, guint macro, gchar *default_value, gint msg_ref);
LogTemplateElem *log_template_elem_new_value(const gchar *text, gchar *value_name, gchar *default_value, gint msg_ref);
void log_template_elem_free_list(GList *el);


#endif
