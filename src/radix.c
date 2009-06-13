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

#include "radix.h"

#include <string.h>
#include <stdlib.h>


/**************************************************************
 * Parsing nodes.
 **************************************************************/

/* FIXME: maybe we should return gchar with the result */

gboolean
r_parser_string(gchar *str, gint *len, const gchar *param, gpointer state, LogMessageMatch *match)
{
  *len = 0;

  while (g_ascii_isalnum(str[*len]) || (param && strchr(param, str[*len])))
    (*len)++;

  if (*len > 0)
    {
      return TRUE;
    }
  return FALSE;
}

gboolean
r_parser_qstring(gchar *str, gint *len, const gchar *param, gpointer state, LogMessageMatch *match)
{
  gchar *end;

  if ((end = strchr(str + 1, ((gchar *)&state)[0])) != NULL)
    {
      *len = (end - str) + 1;

      if (match)
        {
          /* skip starting and ending quote */
          match->ofs = 1;
          match->len = -2;
        }

      return TRUE;
    }
  else
    return FALSE;
}

gboolean
r_parser_estring(gchar *str, gint *len, const gchar *param, gpointer state, LogMessageMatch *match)
{
  gchar *end;
  
  if (!param)
    return FALSE;

  if ((end = strchr(str, param[0])) != NULL)
    {
      *len = (end - str);
      return TRUE;
    }
  else
    return FALSE;
}

gboolean
r_parser_anystring(gchar *str, gint *len, const gchar *param, gpointer state, LogMessageMatch *match)
{
  *len = strlen(str);
  return TRUE;
}

gboolean
r_parser_ipv4(gchar *str, gint *len, const gchar *param, gpointer state, LogMessageMatch *match)
{
  gint dots = 0;
  gint octet = -1;

  *len = 0;

  while (1)
    {
      if (str[*len] == '.')
        {
          if (octet > 255 || octet == -1)
            return FALSE;

          dots++;
          octet = -1;
        }
      else if (g_ascii_isdigit(str[*len]))
        {
          if (octet == -1)
            octet = 0;
          else
            octet *= 10;

          octet += g_ascii_digit_value(str[*len]);
        }
      else
        break;

      (*len)++;
    }

  if (dots != 3 || octet > 255 || octet == -1)
    return FALSE;

  return TRUE;
}

gboolean
r_parser_number(gchar *str, gint *len, const gchar *param, gpointer state, LogMessageMatch *match)
{
  *len = 0;

  while (g_ascii_isdigit(str[*len]))
    (*len)++;

  if (*len > 0)
    {
      return TRUE;
    }
  return FALSE;
}

/**
 * r_new_pnode:
 * 
 * Create a new parsing node.
 **/
RParserNode *
r_new_pnode(gchar *key)
{
  RParserNode *parser_node = g_new0(RParserNode, 1);
  gchar **params = g_strsplit(key, ":", 3);
  guint params_len = g_strv_length(params);

  if (g_str_has_prefix(params[0], "IPv4"))
    {
      parser_node->parse = r_parser_ipv4;
      parser_node->type = RPT_IPV4;
      parser_node->mask = '0';
      parser_node->first = '9' & '0';
    }
  else if (g_str_has_prefix(params[0], "NUMBER"))
    {
      parser_node->parse = r_parser_number;
      parser_node->type = RPT_NUMBER;
      parser_node->mask = '0';
      parser_node->first = '9' & '0';
    }
  else if (g_str_has_prefix(params[0], "STRING"))
    {
      parser_node->parse = r_parser_string;
      parser_node->type = RPT_STRING;
      parser_node->mask = 0;
      parser_node->first = 0;
    }
  else if (g_str_has_prefix(params[0], "ESTRING"))
    {
      parser_node->parse = r_parser_estring;
      parser_node->type = RPT_ESTRING;
      parser_node->mask = 0;
      parser_node->first = 0;
    }
  else if (g_str_has_prefix(params[0], "ANYSTRING"))
    {
      parser_node->parse = r_parser_anystring;
      parser_node->type = RPT_ANYSTRING;
      parser_node->mask = 0;
      parser_node->first = 0;
    }
  else if (g_str_has_prefix(params[0], "QSTRING") && params_len == 3)
    {
      gchar *state = (gchar *) &(parser_node->state);

      parser_node->parse = r_parser_qstring;
      parser_node->type = RPT_QSTRING;
      parser_node->mask = 0xff;
      parser_node->first = params[2][0];
      
      if (strlen(params[2]) == 2)
        state[0] = params[2][1];
      else
        state[0] = params[2][0];
    }
  else
    {
      g_free(parser_node);
      msg_error("Unknown parser type specified", 
                 evt_tag_str("type", params[0]), NULL);
      parser_node = NULL;
    }

  if (parser_node && params[1])
    {
      parser_node->name = g_strdup(params[1]);
      parser_node->name_len = strlen(parser_node->name);

      if (params[2])
        parser_node->param = g_strdup(params[2]);
    }


  g_strfreev(params);

  return parser_node;
}


void
r_free_pnode_only(RParserNode *parser)
{
  if (parser->name)
    g_free(parser->name);

  if (parser->param)
    g_free(parser->param);

  if (parser->state && parser->free_state)
    parser->free_state(parser->state);

  g_free(parser);
}

void
r_free_pnode(RNode *node, void (*free_fn)(gpointer data))
{
  r_free_pnode_only(node->parser);
 
  node->key = NULL;

  r_free_node(node, free_fn);
}

/**************************************************************
 * Literal string nodes.
 **************************************************************/


gint 
r_node_cmp(const void *ap, const void *bp)
{
  RNode *a = *(RNode * const *) ap;
  RNode *b = *(RNode * const *) bp;

  if (a->key[0] < b->key[0])
    return -1;
  else if (a->key[0] > b->key[0])
    return 1;
  return 0;
}

void
r_add_child(RNode *parent, RNode *child)
{
  parent->children = g_realloc(parent->children, (sizeof(RNode *) * (parent->num_children + 1)));

  //FIXME: we could do a simple sorted insert without resorting always
  parent->children[(parent->num_children)++] = child;

  qsort(&(parent->children[0]), parent->num_children, sizeof(RNode *), r_node_cmp);
}

static inline void
r_add_child_check(RNode *root, gchar *key, gpointer value, gboolean parser)
{
  gchar *at;

  if (parser && ((at = strchr(key, '@')) != NULL))
    {
      /* there is an @ somewhere in the string */
      if ((at - key) > 0)
        { 
          /* there are some literal characters before @ so add a new child till @ */
          *at = '\0';

          RNode *child = r_new_node(key, NULL);
          r_add_child(root, child);

          /* and insert the rest begining from @ under the newly created literal node */
          *at = '@';
          r_insert_node(child, at, value, parser);
        }
      else
        {
          /* @ is the first so let's insert it simply and let insert_node handle @ */
          r_insert_node(root, key, value, parser);
        }
    }
  else
    {
      /* either we don't care about parser or no @ in the text, so insert everything as
       * a simple literal node */
      RNode *child = r_new_node(key, value);

      r_add_child(root, child);
    }
}

void
r_add_pchild(RNode *parent, RNode *child)
{
  parent->pchildren = realloc(parent->pchildren, (sizeof(RNode *) * (parent->num_pchildren + 1)));

  parent->pchildren[(parent->num_pchildren)++] = child;
}

gboolean
r_equal_pnode(RParserNode *a, RParserNode *b)
{
  return ((a->parse == b->parse) &&
      ((a->name == NULL && b->name == NULL) ||
       (a->name != NULL && b->name != NULL && g_str_equal(a->name, b->name))));
}

RNode *
r_find_pchild(RNode *parent, RParserNode *parser_node)
{
  RNode *node = NULL;
  gint i;

  for (i = 0; i < parent->num_pchildren; i++)
    {
      if (r_equal_pnode(parent->pchildren[i]->parser, parser_node))
        {
          node = parent->pchildren[i];
          break;
        }
    }

  return node;
}

RNode *
r_find_child(RNode *root, char key)
{
  register gint l, u, idx;
  register char k = key;

  l = 0;
  u = root->num_children;

  while (l < u)
    {
      idx = (l + u) / 2;

      if (root->children[idx]->key[0] > k)
        u = idx;
      else if (root->children[idx]->key[0] < k)
        l = idx + 1;
      else
        return root->children[idx];
    }

  return NULL;
}

void
r_insert_node(RNode *root, gchar *key, gpointer value, gboolean parser)
{
  RNode *node;
  gint keylen = strlen(key);
  gint nodelen = root->keylen;
  gint i = 0;
  
  if (parser && key[0] == '@')
    {
      gchar *end;

      if (keylen >= 2 && key[1] == '@')
        {
          /* we found and escape, so check if we already have a child with '@', or add a child like that */
          node = r_find_child(root, key[1]);
          
          if (!node)
            {
              /* no child so we create one
               * if we are at the end of the key than use value, otherwise it is just a gap node 
               */
              node = r_new_node("@", (keylen == 2 ? value : NULL));
              r_add_child(root, node);
            }
          else if (keylen == 2)
            {
              /* if we are at the end of the key set the value if it is not already exists,
               * otherwise it is duplicate node
               */
              if (!node->value)
                node->value = value;
              else
                msg_error("Duplicate key in parser radix tree", evt_tag_str("key", "@"), NULL);
            }

          /* go down building the tree if there is key left */
          if (keylen > 2)
            r_insert_node(node, key + 2, value, parser);

        }
      else if ((keylen >= 2) && (end = strchr((const gchar *)key + 1, '@')) != NULL)
        {
          /* we are a parser node */
          *end = '\0';

          RParserNode *parser_node = r_new_pnode(key + 1);

          if (parser_node)
            {
              node = r_find_pchild(root, parser_node);

              if (!node)
                {
                  node = r_new_node(NULL, NULL);
                  node->parser = parser_node;

                  r_add_pchild(root, node);
                }
              else
                {
                  r_free_pnode_only(parser_node);
                }


              if ((end - key) < (keylen - 1))
                {
                  /* the key is not over so go on building the tree */
                  r_insert_node(node, end + 1, value, parser);
                }
              else
                {
                  /* the key is over so set value if it is not yet set */

                  if (!node->value)
                    {
                      node->value = value;
                    }
                  else
                    {
                      /* FIXME: print parser type in string format */
                      msg_error("Duplicate parser node in radix tree",
                                evt_tag_int("type", node->parser->type),
                                evt_tag_str("name", (node->parser->name ? node->parser->name : "")),
                                NULL);
                    }
                }
            }

        }
      else
        msg_error("Key contains '@' without escaping", evt_tag_str("key", key), NULL);
    }
  else
    {
      /* we are not starting with @ sign or we are not interested in @ at all */
      
      while (i < keylen && i < nodelen)
        {
          /* check if key is the same, or if it is a parser */
          if ((key[i] != root->key[i]) || (parser && key[i] == '@'))
            break;
          
          i++;
        }
      
      
      if (nodelen == 0 || i == 0 || (i < keylen && i >= nodelen))
        {
          /*either at the root or we need to go down the tree on the right child */
          
          node = r_find_child(root, key[i]);
          
          if (node)
            {
              /* @ is always a singel node, and we also have an @ so insert us under root */
              if (parser && key[i] == '@')
                r_insert_node(root, key + i, value, parser);
              else
                r_insert_node(node, key + i, value, parser);
            }
          else
            {
              r_add_child_check(root, key + i, value, parser);
            }
        }
      else if (i == keylen && i == nodelen)
        {
          /* exact match */
          
          if (!root->value)
            root->value = value;
          else
            msg_error("Duplicate key in radix tree", evt_tag_str("key", key), NULL);
        }
      else if (i > 0 && i < nodelen)
        {
          RNode *old_tree;
          gchar *new_key;
          
          /* we need to split the current node */
          old_tree = r_new_node(root->key + i, root->value);
          if (root->num_children)
            {
              old_tree->children = root->children;
              old_tree->num_children = root->num_children;
              root->children = NULL;
              root->num_children = 0;
            }
          
          if (root->num_pchildren)
            {
              old_tree->pchildren = root->pchildren;
              old_tree->num_pchildren = root->num_pchildren;
              root->pchildren = NULL;
              root->num_pchildren = 0;
            }
          
          root->value = NULL;
          new_key = g_strndup(root->key, i);
          g_free(root->key);
          root->key = new_key;
          root->keylen = i;
          
          r_add_child(root, old_tree);
          
          if (i < keylen)
            {
              /* we add a new sub tree */
              r_add_child_check(root, key + i, value, parser); 
            }
          else
            {
              /* the split is us */
              root->value = value;
            }
        }
      else
        {
          /* simply a new children */
          r_add_child_check(root, key + i, value, parser);
        }
    }
}


/* FIXME: a non-recursive algorithm might be faster */
RNode *
r_find_node(RNode *root, gchar *whole_key, gchar *key, gint keylen, GArray *matches, GPtrArray *match_names)
{
  RNode *node, *ret;
  gint nodelen = root->keylen;
  gint j, m;
  register gint i;
 
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
        ret = r_find_node(node, whole_key, key + i, keylen - i, matches, match_names);

      /* we only search if there is no match */
      if (!ret)
        {
          gint len;
          RParserNode *parser_node;
          gint match_ofs = 0;
          LogMessageMatch *match = NULL;
          gchar **match_name;

          if (matches)
            {
              match_ofs = matches->len;
              
              g_array_set_size(matches, match_ofs + 1);
              g_ptr_array_set_size(match_names, match_ofs + 1);
            }
          for (j = 0; j < root->num_pchildren; j++)
            {
              parser_node = root->pchildren[j]->parser;

              if (matches)
                {
                  match = &g_array_index(matches, LogMessageMatch, match_ofs);
                  memset(match, 0, sizeof(*match));
                  match->flags = LMM_REF_MATCH;
                }
              if (((key[i] & parser_node->mask) == parser_node->first) &&
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
                  
                  ret = r_find_node(root->pchildren[j], whole_key, key + i + len, keylen - (i + len), matches, match_names);
                  if (matches)
                    {

                      match = &g_array_index(matches, LogMessageMatch, match_ofs);
                      match_name = (gchar **) &g_ptr_array_index(match_names, match_ofs);

                      if (ret)
                        {
                          if ((match->flags & LMM_REF_MATCH))
                            {
                              /* NOTE: we allow the parser to return relative
                               * offset & length to the field parsed, this way
                               * quote characters can still be returned as
                               * REF_MATCH and we only need to duplicate the
                               * result if the string is indeed modified
                               */
                              match->type = parser_node->type;
                              match->ofs = match->ofs + (key + i) - whole_key;
                              match->len = match->len + len;
                            }
                          if (parser_node->name)
                            *match_name = g_strndup(parser_node->name, parser_node->name_len);
                          else
                            *match_name = NULL;
                          break;
                        }
                      else
                        {
                          if ((match->flags & LMM_REF_MATCH) == 0)
                            {
                              /* free the stored match, if this was a dead-end */
                              g_free(match->match);
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

/**
 * r_new_node:
 */
RNode *
r_new_node(gchar *key, gpointer value)
{
  RNode *node = g_malloc(sizeof(RNode));

  node->key = g_strdup(key);
  node->keylen = (key ? strlen(key) : -1);
  node->parser = NULL;
  node->value = value;

  node->num_children = 0;
  node->children = NULL;

  node->num_pchildren = 0;
  node->pchildren = NULL;

  return node;
}

void
r_free_node(RNode *node, void (*free_fn)(gpointer data))
{
  gint i;

  for (i = 0; i < node->num_children; i++)
    r_free_node(node->children[i], free_fn);

  if (node->children)
    g_free(node->children);

  for (i = 0; i < node->num_pchildren; i++)
    r_free_pnode(node->pchildren[i], free_fn);

  if (node->pchildren)
    g_free(node->pchildren);

  if (node->key)
    g_free(node->key);

  if (node->value && free_fn)
    free_fn(node->value);

  g_free(node);
}
