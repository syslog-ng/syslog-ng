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

/* This is not a standalone .c file compiled on its own, but rather it is
 * included multiple times into radix.c, once with RADIX_DBG defined, once
 * without that.
 */

/* FIXME: a non-recursive algorithm might be faster */

#ifdef RADIX_DBG
static void
r_add_debug_info(GArray *dbg_list, RNode *node, RParserNode *pnode, gint i, gint match_off, gint match_len)
{
  RDebugInfo dbg_info;

  dbg_info.node = node;
  dbg_info.pnode = pnode;
  dbg_info.i = i;
  dbg_info.match_off = match_off;
  dbg_info.match_len = match_len;

  g_array_append_val(dbg_list, dbg_info);
}

static void
r_truncate_debug_info(GArray *dbg_list, gint truncated_size)
{
  g_array_set_size(dbg_list, truncated_size);
}
#endif

#ifndef RADIX_DBG
RNode *
r_find_node(RNode *root, gchar *whole_key, gchar *key, gint keylen, GArray *matches)
#else
RNode *
r_find_node_dbg(RNode *root, gchar *whole_key, gchar *key, gint keylen, GArray *matches, GArray *dbg_list)
#endif
{
  RNode *node, *ret;
  gint nodelen = root->keylen;
  gint j, m;
  register gint i;
#ifdef RADIX_DBG
  gint dbg_entries;
#endif

  if (nodelen < 1)
    i = 0;
  else if (nodelen == 1)
    i = 1;
  else
    {
      m = MIN(keylen, nodelen);

      /* this is a prefix match algorithm, we are interested how long
       * the common part between key and root->key is. Currently this
       * uses a byte-by-byte comparison, using a 64/32/16 bit units
       * would be better.
       *
       * The code below to perform aligned comparison is commented out
       * as it does not seem to matter and it is way more complex than
       * the simple algorithm below. */

#if 0
      if (2 <= m && (((unsigned long)key % 2) == 0))
        {
          gushort *keylong = key;
          gushort *rootlong = root->key;
          i = 0;

          while ((i + 2) <= m)
            {
              if (*keylong != *rootlong)
                break;

              i += 2;
              keylong = key + i;
              rootlong = root->key + i;
            }
          /*printf("RESULT %d\n", i); */
        }
      else
#endif
        i = 1;

      while (i < m)
        {
          if (key[i] != root->key[i])
            break;

          i++;
        }
    }

#ifdef RADIX_DBG
  r_add_debug_info(dbg_list, root, NULL, i, 0, 0);
  dbg_entries = dbg_list->len;
#endif

  msg_trace("Looking up node in the radix tree",
            evt_tag_int("i", i),
            evt_tag_int("nodelen", nodelen),
            evt_tag_int("keylen", keylen),
            evt_tag_str("root_key", root->key),
            evt_tag_str("key", key),
            NULL);

  if (i == keylen && (i == nodelen || nodelen == -1))
    {
      if (root->value)
        return root;
    }
  else if ((nodelen < 1) || (i < keylen && i >= nodelen))
    {
      ret = NULL;
      node = r_find_child(root, key[i]);

      if (node)
        {
#ifndef RADIX_DBG
          ret = r_find_node(node, whole_key, key + i, keylen - i, matches);
#else
          ret = r_find_node_dbg(node, whole_key, key + i, keylen - i, matches, dbg_list);
#endif
        }

      /* we only search if there is no match */
      if (!ret)
        {
          gint len;
          RParserNode *parser_node;
          gint match_ofs = 0;
          RParserMatch *match = NULL;

          if (matches)
            {
              match_ofs = matches->len;

              g_array_set_size(matches, match_ofs + 1);
            }
          for (j = 0; j < root->num_pchildren; j++)
            {
              parser_node = root->pchildren[j]->parser;

              if (matches)
                {
                  match = &g_array_index(matches, RParserMatch, match_ofs);
                  memset(match, 0, sizeof(*match));
                }
#ifdef RADIX_DBG
              r_truncate_debug_info(dbg_list, dbg_entries);
#endif
              if (((parser_node->first <= key[i]) && (key[i] <= parser_node->last)) &&
                  (parser_node->parse(key + i, &len, parser_node->param, parser_node->state, match)))
                {

                  /* FIXME: we don't try to find the longest match in case
                   * the radix tree is split on a parser node. The correct
                   * approach would be to try all parsers and select the
                   * best match, however it is quite expensive and difficult
                   * to implement and we don't really expect this to be a
                   * realistic case. A log message is printed if such a
                   * collision occurs, so there's a slight chance we'll
                   * recognize if this happens in real life. */

#ifndef RADIX_DBG
                  ret = r_find_node(root->pchildren[j], whole_key, key + i + len, keylen - (i + len), matches);
#else
                  r_add_debug_info(dbg_list, root, parser_node, len, ((gint16) match->ofs) + (key + i) - whole_key, ((gint16) match->len) + len);
                  ret = r_find_node_dbg(root->pchildren[j], whole_key, key + i + len, keylen - (i + len), matches, dbg_list);
#endif
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
                              match->ofs = match->ofs + (key + i) - whole_key;
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
