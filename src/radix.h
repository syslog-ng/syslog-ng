/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
  
#ifndef RADIX_H_INCLUDED
#define RADIX_H_INCLUDED

#include "logmsg.h"
#include "messages.h"

/* parser types */
enum
{
  RPT_STRING,
  RPT_QSTRING,
  RPT_ESTRING,
  RPT_IPV4,
  RPT_NUMBER
};

typedef struct _RParserNode
{
  /* user supplied parameters */

  /* name of the parsed value */
  gchar *name;
  /* user supplied parameters */
  gchar *param;
  /* internal state of the parser node */
  gpointer state;

  guchar mask;
  gchar first;
  guint8 name_len;
  guint8 type;

  gboolean (*parse)(gchar *str, gint *len, const gchar *param, gpointer state, LogMessageMatch *match);
  void (*free_state)(gpointer state);
} RParserNode;

typedef struct _RNode RNode;

struct _RNode
{
  gchar *key;
  gint keylen;
  RParserNode *parser;
  gpointer value;
  guint num_children;
  RNode **children;

  guint num_pchildren;
  RNode **pchildren;
};


RNode *r_new_node(gchar *key, gpointer value);
void r_free_node(RNode *node, void (*free_fn)(gpointer data));
void r_insert_node(RNode *root, gchar *key, gpointer value, gboolean parser);
RNode *r_find_node(RNode *root, gchar *whole_key, gchar *key, gint keylen, GArray *matches, GPtrArray *match_names);

#endif

