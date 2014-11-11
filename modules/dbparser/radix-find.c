/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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

/* This is not a standalone .c file compiled on its own, but rather it is
 * included multiple times into radix.c, once with RADIX_DBG defined, once
 * without that.
 */

/* FIXME: a non-recursive algorithm might be faster */

static void
r_add_debug_info(GArray *dbg_list, RNode *node, RParserNode *pnode, gint i, gint match_off, gint match_len)
{
  RDebugInfo dbg_info;

  if (dbg_list)
    {
      dbg_info.node = node;
      dbg_info.pnode = pnode;
      dbg_info.i = i;
      dbg_info.match_off = match_off;
      dbg_info.match_len = match_len;

      g_array_append_val(dbg_list, dbg_info);
    }
}

static void
r_add_literal_match_to_debug_info(GArray *dbg_list, RNode *node, gint literal_prefix)
{
  if (dbg_list)
    r_add_debug_info(dbg_list, node, NULL, literal_prefix, 0, 0);
}

static void
r_truncate_debug_info(GArray *dbg_list, gint truncated_size)
{
  if (dbg_list)
    g_array_set_size(dbg_list, truncated_size);
}

static gint
r_find_matching_literal_prefix(RNode *root, guint8 *key, gint keylen)
{
  gint current_node_key_length = root->keylen;
  gint match_length;

  if (current_node_key_length < 1)
    match_length = 0;
  else if (current_node_key_length == 1)
    match_length = 1;
  else
    {
      gint m = MIN(keylen, current_node_key_length);

      /* this is a prefix match algorithm, we are interested how long the
       * common part between key and root->key is.  Currently this uses a
       * byte-by-byte comparison, using a 64/32/16 bit units would be
       * better.  We had a PoC code to do that, and the performance
       * difference wasn't big enough to offset the complexity so it was
       * removed. We might want to rerun perf tests and see if we could
       * speed things up, but db-parser() seems fast enough as it is.
       */
      match_length = 1;
      while (match_length < m)
        {
          if (key[match_length] != root->key[match_length])
            break;

          match_length++;
        }
    }
  return match_length;
}


typedef struct _RFindNodeState
{
  GArray *matches;
  GArray *dbg_list;
} RFindNodeState;


RNode *
_r_find_node(RFindNodeState *state, RNode *root, guint8 *whole_key, guint8 *key, gint keylen)
{
  gint current_node_key_length = root->keylen;
  register gint literal_prefix_len;
  GArray *matches = state->matches;
  gint dbg_entries;

  literal_prefix_len = r_find_matching_literal_prefix(root, key, keylen);

  r_add_literal_match_to_debug_info(state->dbg_list, root, literal_prefix_len);
  dbg_entries = state->dbg_list ? state->dbg_list->len : 0;

  msg_trace("Looking up node in the radix tree",
            evt_tag_int("literal_prefix_len", literal_prefix_len),
            evt_tag_int("current_node_key_length", current_node_key_length),
            evt_tag_int("keylen", keylen),
            evt_tag_str("root_key", root->key),
            evt_tag_str("key", key),
            NULL);

  if (literal_prefix_len == keylen && (literal_prefix_len == current_node_key_length || current_node_key_length == -1))
    {
      if (root->value)
        return root;
    }
  else if ((current_node_key_length < 1) || (literal_prefix_len < keylen && literal_prefix_len >= current_node_key_length))
    {
      RNode *node, *ret;
      ret = NULL;
      node = r_find_child_by_first_character(root, key[literal_prefix_len]);

      if (node)
        {
#ifndef RADIX_DBG
          ret = r_find_node(node, whole_key, key + literal_prefix_len, keylen - literal_prefix_len, matches);
#else
          ret = r_find_node_dbg(node, whole_key, key + literal_prefix_len, keylen - literal_prefix_len, matches, dbg_list);
#endif
        }

      /* we only search if there is no match */
      if (!ret)
        {
          gint len;
          RParserNode *parser_node;
          gint match_ofs = 0;
          RParserMatch *match = NULL;
          gint parser_ndx;

          if (matches)
            {
              match_ofs = matches->len;

              g_array_set_size(matches, match_ofs + 1);
            }
          for (parser_ndx = 0; parser_ndx < root->num_pchildren; parser_ndx++)
            {
              parser_node = root->pchildren[parser_ndx]->parser;

              if (matches)
                {
                  match = &g_array_index(matches, RParserMatch, match_ofs);
                  memset(match, 0, sizeof(*match));
                }
              r_truncate_debug_info(state->dbg_list, dbg_entries);
              if (((parser_node->first <= key[literal_prefix_len]) && (key[literal_prefix_len] <= parser_node->last)) &&
                  (parser_node->parse(key + literal_prefix_len, &len, parser_node->param, parser_node->state, match)))
                {

                  /* FIXME: we don't try to find the longest match in case
                   * the radix tree is split on a parser node. The correct
                   * approach would be to try all parsers and select the
                   * best match, however it is quite expensive and difficult
                   * to implement and we don't really expect this to be a
                   * realistic case. A log message is printed if such a
                   * collision occurs, so there's a slight chance we'll
                   * recognize if this happens in real life. */

                  ret = _r_find_node(state, root->pchildren[parser_ndx], whole_key, key + literal_prefix_len + len, keylen - (literal_prefix_len + len));

                  r_add_debug_info(state->dbg_list, root, parser_node, len, ((gint16) match->ofs) + (key + literal_prefix_len) - whole_key, ((gint16) match->len) + len);
                  if (matches)
                    {

                      match = &g_array_index(matches, RParserMatch, match_ofs);

                      if (ret)
                        {
                          if (!(match->match))
                            {
                              /* NOTE: we allow the parser to return relative
                               * offset & length to the field parsed, this way
                               * quote characters can still be returned as
                               * REF_MATCH and we only need to duplicate the
                               * result if the string is indeed modified
                               */
                              match->type = parser_node->type;
                              match->ofs = match->ofs + (key + literal_prefix_len) - whole_key;
                              match->len = (gint16) match->len + len;
                              match->handle = parser_node->handle;
                            }
                          break;
                        }
                      else
                        {
                          if (match->match)
                            {
                              /* free the stored match, if this was a dead-end */
                              g_free(match->match);
                              match->match = NULL;
                            }
                        }
                    }
                }
            }
          if (!ret && matches)
            {
              /* the values in the matches array has already been freed if we come here */
              g_array_set_size(matches, match_ofs);
            }
        }

      if (ret)
        return ret;
      else if (root->value)
        return root;
    }

  return NULL;
}


RNode *
r_find_node(RNode *root, guint8 *whole_key, guint8 *key, gint keylen, GArray *matches)
{
  RFindNodeState state = {
    .matches = matches,
  };

  return _r_find_node(&state, root, whole_key, key, keylen);
}

RNode *
r_find_node_dbg(RNode *root, guint8 *whole_key, guint8 *key, gint keylen, GArray *matches, GArray *dbg_list)
{
  RFindNodeState state = {
    .matches = matches,
    .dbg_list = dbg_list,
  };

  return _r_find_node(&state, root, whole_key, key, keylen);
}
