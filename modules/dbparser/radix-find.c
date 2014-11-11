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

typedef struct _RFindNodeState
{
  guint8 *whole_key;
  GArray *stored_matches;
  GArray *dbg_list;
} RFindNodeState;


static void
r_add_debug_info(RFindNodeState *state, RNode *node, RParserNode *pnode, gint i, gint match_off, gint match_len)
{
  RDebugInfo dbg_info;

  if (state->dbg_list)
    {
      dbg_info.node = node;
      dbg_info.pnode = pnode;
      dbg_info.i = i;
      dbg_info.match_off = match_off;
      dbg_info.match_len = match_len;

      g_array_append_val(state->dbg_list, dbg_info);
    }
}

static void
r_add_literal_match_to_debug_info(RFindNodeState *state, RNode *node, gint literal_prefix)
{
  r_add_debug_info(state, node, NULL, literal_prefix, 0, 0);
}

static void
r_truncate_debug_info(RFindNodeState *state, gint truncated_size)
{
  if (state->dbg_list)
    g_array_set_size(state->dbg_list, truncated_size);
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


static RNode *
_r_find_node(RFindNodeState *state, RNode *root, guint8 *key, gint keylen);


static RNode *
r_find_child_by_remainding_key(RFindNodeState *state, RNode *root, guint8 *remainding_key, gint remainding_keylen)
{
  RNode *candidate = r_find_child_by_first_character(root, remainding_key[0]);

  if (candidate)
    {
      return _r_find_node(state, candidate, remainding_key, remainding_keylen);
    }
  return NULL;
}

static gint
r_grow_stored_matches(RFindNodeState *state)
{
  gint matches_stored_by_caller;

  if (state->stored_matches)
    {
      matches_stored_by_caller = state->stored_matches->len;

      g_array_set_size(state->stored_matches, matches_stored_by_caller + 1);
    }
  return matches_stored_by_caller;
}

static RParserMatch *
r_get_current_match(RFindNodeState *state, gint matches_stored_by_caller)
{
  return &g_array_index(state->stored_matches, RParserMatch, matches_stored_by_caller);
}

static RParserMatch *
r_clear_current_match(RFindNodeState *state, gint matches_stored_by_caller)
{
  RParserMatch *match = NULL;

  if (state->stored_matches)
    {
      match = r_get_current_match(state, matches_stored_by_caller);
      memset(match, 0, sizeof(*match));
    }
  return match;
}

static RNode *
r_find_child_by_parser(RFindNodeState *state, RNode *root, guint8 *remaining_key, gint remaining_keylen)
{
  GArray *stored_matches = state->stored_matches;
  gint dbg_entries = state->dbg_list ? state->dbg_list->len : 0;
  gint matches_stored_by_caller = 0;
  RParserMatch *match = NULL;
  gint parser_ndx;
  RNode *ret = NULL;
  guint8 remaining_key_first_character = remaining_key[0];

  matches_stored_by_caller = r_grow_stored_matches(state);
  for (parser_ndx = 0; parser_ndx < root->num_pchildren; parser_ndx++)
    {
      RParserNode *parser_node;
      gint extracted_match_len;

      parser_node = root->pchildren[parser_ndx]->parser;

      match = r_clear_current_match(state, matches_stored_by_caller);
      r_truncate_debug_info(state, dbg_entries);

      if (((parser_node->first <= remaining_key_first_character) && (remaining_key_first_character <= parser_node->last)) &&
          (parser_node->parse(remaining_key, &extracted_match_len, parser_node->param, parser_node->state, match)))
        {

          /* FIXME: we don't try to find the longest match in case
           * the radix tree is split on a parser node. The correct
           * approach would be to try all parsers and select the
           * best match, however it is quite expensive and difficult
           * to implement and we don't really expect this to be a
           * realistic case. A log message is printed if such a
           * collision occurs, so there's a slight chance we'll
           * recognize if this happens in real life. */

          ret = _r_find_node(state, root->pchildren[parser_ndx], remaining_key + extracted_match_len, remaining_keylen - extracted_match_len);

          r_add_debug_info(state, root, parser_node, extracted_match_len, ((gint16) match->ofs) + remaining_key - state->whole_key, ((gint16) match->len) + extracted_match_len);
          if (stored_matches)
            {
              /* we have to look up "match" again as the GArray may have moved the data */
              match = r_get_current_match(state, matches_stored_by_caller);
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
                      match->ofs = match->ofs + remaining_key - state->whole_key;
                      match->len = (gint16) match->len + extracted_match_len;
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
  if (!ret && stored_matches)
    {
      /* the values in the stored_matches array has already been freed if we come here */
      g_array_set_size(stored_matches, matches_stored_by_caller);
    }
  return ret;
}

static RNode *
_r_find_node(RFindNodeState *state, RNode *root, guint8 *key, gint keylen)
{
  gint current_node_key_length = root->keylen;
  register gint literal_prefix_len;

  literal_prefix_len = r_find_matching_literal_prefix(root, key, keylen);
  r_add_literal_match_to_debug_info(state, root, literal_prefix_len);

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
      RNode *ret;
      guint8 *remaining_key = key + literal_prefix_len;
      gint remaining_keylen = keylen - literal_prefix_len;

      ret = r_find_child_by_remainding_key(state, root, remaining_key, remaining_keylen);
      /* we only search if there is no match */
      if (!ret)
        {
          ret = r_find_child_by_parser(state, root, remaining_key, remaining_keylen);
        }

      if (ret)
        return ret;
      else if (root->value)
        return root;
    }

  return NULL;
}


RNode *
r_find_node(RNode *root, guint8 *whole_key, guint8 *key, gint keylen, GArray *stored_matches)
{
  RFindNodeState state = {
    .whole_key = whole_key,
    .stored_matches = stored_matches,
  };

  return _r_find_node(&state, root, key, keylen);
}

RNode *
r_find_node_dbg(RNode *root, guint8 *whole_key, guint8 *key, gint keylen, GArray *stored_matches, GArray *dbg_list)
{
  RFindNodeState state = {
    .whole_key = whole_key,
    .stored_matches = stored_matches,
    .dbg_list = dbg_list,
  };

  return _r_find_node(&state, root, key, keylen);
}
