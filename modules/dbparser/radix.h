/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
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
  
#ifndef RADIX_H_INCLUDED
#define RADIX_H_INCLUDED

#include "logmsg.h"
#include "messages.h"

/* parser types, these are saved in the serialized log message along with
 * the match information thus they have to remain the same in order to keep
 * compatibility, thus add new stuff at the end */
enum
{
  RPT_STRING,
  RPT_QSTRING,
  RPT_ESTRING,
  RPT_IPV4,
  RPT_NUMBER,
  RPT_ANYSTRING,
  RPT_IPV6,
  RPT_IP,
  RPT_FLOAT
};

typedef struct _RParserMatch
{
  /* if this pointer is non-NULL, it means that we've transformed the
   * original message to something else, and thus we couldn't get a
   * reference from the original payload */

  gchar *match;
  /* the value in which to store this match */
  NVHandle handle;
  guint16 len;
  guint16 ofs;
  guint8 type;
} RParserMatch;

typedef struct _RParserNode
{
  /* user supplied parameters */

  /* user supplied parameters */
  gchar *param;
  /* internal state of the parser node */
  gpointer state;

  guchar first;
  guchar last;
  guint8 type;
  NVHandle handle;

  gboolean (*parse)(gchar *str, gint *len, const gchar *param, gpointer state, RParserMatch *match);
  void (*free_state)(gpointer state);
} RParserNode;

typedef gchar *(*RNodeGetValueFunc) (gpointer value);

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

typedef struct _RDebugInfo
{
  RNode *node;
  RParserNode *pnode;
  gint i;
  gint match_off;
  gint match_len;
} RDebugInfo;

static inline gchar *
r_parser_type_name(guint8 type)
{
  switch (type)
    {
      case RPT_STRING:
        return "STRING";
      case RPT_QSTRING:
        return "QSTRING";
      case RPT_ESTRING:
        return "ESTRING";
      case RPT_IPV4:
        return "IPv4";
      case RPT_NUMBER:
        return "NUMBER";
      case RPT_ANYSTRING:
        return "ANYSTRING";
      case RPT_IPV6:
        return "IPv6";
      case RPT_IP:
        return "IP";
      case RPT_FLOAT:
        return "FLOAT";
      default:
        return "UNKNOWN";
    }
}

RNode *r_new_node(gchar *key, gpointer value);
void r_free_node(RNode *node, void (*free_fn)(gpointer data));
void r_insert_node(RNode *root, gchar *key, gpointer value, gboolean parser, RNodeGetValueFunc value_func);
RNode *r_find_node(RNode *root, gchar *whole_key, gchar *key, gint keylen, GArray *matches);
RNode *r_find_node_dbg(RNode *root, gchar *whole_key, gchar *key, gint keylen, GArray *matches, GArray *dbg_list);

#endif

