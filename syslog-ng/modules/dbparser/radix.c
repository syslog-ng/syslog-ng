/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#include "radix.h"

#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include <pcre.h>

/**************************************************************
 * Parsing nodes.
 **************************************************************/

/* FIXME: maybe we should return gchar with the result */

gboolean
r_parser_string(gchar *str, gint *len, const gchar *param, gpointer state, RParserMatch *match)
{
  *len = 0;

  while (str[*len] && (g_ascii_isalnum(str[*len]) || (param && strchr(param, str[*len]))))
    (*len)++;

  if (*len > 0)
    {
      return TRUE;
    }
  return FALSE;
}

gboolean
r_parser_qstring(gchar *str, gint *len, const gchar *param, gpointer state, RParserMatch *match)
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
r_parser_estring_c(gchar *str, gint *len, const gchar *param, gpointer state, RParserMatch *match)
{
  gchar *end;

  if (!param)
    return FALSE;

  if ((end = strchr(str, param[0])) != NULL)
    {
      *len = (end - str) + 1;
      if (match)
        match->len = -1;
      return TRUE;
    }
  else
    return FALSE;
}

gboolean
r_parser_nlstring(gchar *str, gint *len, const gchar *param, gpointer state, RParserMatch *match)
{
  gchar *end;

  if ((end = strchr(str, '\n')) != NULL)
    {
      /* drop CR before to LF */
      if (end - str >= 1 && *(end - 1) == '\r')
        end--;
      *len = (end - str);
      return TRUE;
    }
  else
    return FALSE;
}

gboolean
r_parser_estring(gchar *str, gint *len, const gchar *param, gpointer state, RParserMatch *match)
{
  gchar *end;

  if (!param)
    return FALSE;

  if ((end = strstr(str, param)) != NULL)
    {
      *len = (end - str) + GPOINTER_TO_INT(state);
      if (match)
        match->len = -GPOINTER_TO_INT(state);
      return TRUE;
    }
  else
    return FALSE;
}

typedef struct _RParserPCREState
{
  pcre *re;
  pcre_extra *extra;
} RParserPCREState;

gboolean
r_parser_pcre(gchar *str, gint *len, const gchar *param, gpointer state, RParserMatch *match)
{
  RParserPCREState *self = (RParserPCREState *) state;
  gint rc;
  gint num_matches;

  if (pcre_fullinfo(self->re, self->extra, PCRE_INFO_CAPTURECOUNT, &num_matches) < 0)
    g_assert_not_reached();
  if (num_matches > RE_MAX_MATCHES)
    num_matches = RE_MAX_MATCHES;

  gsize matches_size = 3 * (num_matches + 1);
  gint *matches = g_alloca(matches_size * sizeof(gint));

  rc = pcre_exec(self->re, self->extra, str, strlen(str), 0, 0, matches, matches_size);

  if (rc == PCRE_ERROR_NOMATCH)
    {
      return FALSE;
    }

  if (rc < 0)
    {
      msg_error("Error while matching regexp", evt_tag_int("error_code", rc));
      return FALSE;
    }

  if (rc == 0)
    {
      msg_error("Error while storing matching substrings");
      return FALSE;
    }

  *len = matches[1] - matches[0];
  return TRUE;
}

gpointer
r_parser_pcre_compile_state(const gchar *expr)
{
  RParserPCREState *self = g_new0(RParserPCREState, 1);
  const gchar *errptr;
  gint erroffset;
  gint rc;

  self->re = pcre_compile2(expr, PCRE_ANCHORED, &rc, &errptr, &erroffset, NULL);
  if (!self->re)
    {
      msg_error("Error while compiling regular expression",
                evt_tag_str("regular_expression", expr),
                evt_tag_str("error_at", &expr[erroffset]),
                evt_tag_int("error_offset", erroffset),
                evt_tag_str("error_message", errptr),
                evt_tag_int("error_code", rc));
      g_free(self);
      return NULL;
    }
  self->extra = pcre_study(self->re, 0, &errptr);
  if (errptr)
    {
      msg_error("Error while optimizing regular expression",
                evt_tag_str("regular_expression", expr),
                evt_tag_str("error_message", errptr));
      pcre_free(self->re);
      if (self->extra)
        pcre_free(self->extra);
      g_free(self);
      return NULL;
    }
  return (gpointer) self;
}

static void
r_parser_pcre_free_state(gpointer s)
{
  RParserPCREState *self = (RParserPCREState *) s;

  if (self->re)
    pcre_free(self->re);
  if (self->extra)
    pcre_free(self->extra);
  g_free(self);
}

gboolean
r_parser_anystring(gchar *str, gint *len, const gchar *param, gpointer state, RParserMatch *match)
{
  *len = strlen(str);
  return TRUE;
}

gboolean
r_parser_set(gchar *str, gint *len, const gchar *param, gpointer state, RParserMatch *match)
{
  *len = 0;

  if (!param)
    return FALSE;

  while (strchr(param, str[*len]))
    (*len)++;

  if (*len > 0)
    {
      return TRUE;
    }
  return FALSE;
}


gboolean
r_parser_email(gchar *str, gint *len, const gchar *param, gpointer state, RParserMatch *match)
{
  gint end;
  int count = 0;

  const gchar *email = "!#$%&'*+-/=?^_`{|}~.";

  *len = 0;

  if (param)
    while (strchr(param, str[*len]))
      (*len)++;

  if (match)
    match->ofs = *len;

  /* first character of e-mail can not be a period */
  if (str[*len] == '.')
    return FALSE;

  while (g_ascii_isalnum(str[*len]) || (strchr(email, str[*len])))
    (*len)++;
  /* last character of e-mail can not be a period */
  if (str[*len-1] == '.')
    return FALSE;

  if (str[*len] == '@' )
    (*len)++;
  else
    return FALSE;

  /* Be accepting of any hostnames - if they are in the logs, they
     probably were in the DNS */
  while (g_ascii_isalnum(str[*len]) || (str[*len] == '-' ))
    {
      (*len)++;
      count++;
      while (g_ascii_isalnum(str[*len]) || (str[*len] == '-' ))
        (*len)++;

      if (str[*len] == '.')
        (*len)++;
    }
  if (count < 2)
    return FALSE;

  end = *len;
  if (param)
    while (strchr(param, str[*len]))
      (*len)++;

  if (match)
    match->len = end - *len - match->ofs;

  if (*len > 0)
    return TRUE;
  return FALSE;
}

gboolean
r_parser_hostname(gchar *str, gint *len, const gchar *param, gpointer state, RParserMatch *match)
{
  int count = 0;

  *len = 0;

  while (g_ascii_isalnum(str[*len]) || (str[*len] == '-' ))
    {
      (*len)++;
      count++;
      while (g_ascii_isalnum(str[*len]) || (str[*len] == '-' ))
        (*len)++;

      if (str[*len] == '.')
        (*len)++;

    }
  if (count < 2)
    return FALSE;

  return TRUE;
}

static gboolean
_r_parser_lladdr(gchar *str, gint *len, gint count, gint parts, gpointer state, RParserMatch *match)
{
  gint i;
  *len = 0;

  for (i = 1; i <= parts; i++)
    {
      if (!g_ascii_isxdigit(str[*len]) || !g_ascii_isxdigit(str[*len + 1]))
        {
          if ( i > 1 )
            {
              (*len) -= 1;
              break;
            }
          else
            return FALSE;
        }
      if (i == parts)
        (*len) += 2;
      else
        {
          if (str[*len + 2] != ':')
            {
              (*len) += 2;
              break;
            }
          else
            (*len) += 3;
        }
    }
  if (G_UNLIKELY(*len > count))
    return FALSE;

  return TRUE;
}

gboolean
r_parser_lladdr(gchar *str, gint *len, const gchar *param, gpointer state, RParserMatch *match)
{
  gint count, parts;

  /* get the maximum octet count from the parameter */
  if (param)
    {
      *len = 0;
      parts = 0;
      while (g_ascii_isdigit(param[*len]))
        {
          parts = parts * 10 + g_ascii_digit_value(param[*len]);
          (*len)++;
        }
    }
  else
    parts = 20;
  count = (parts * 3) - 1;

  return _r_parser_lladdr(str, len, count, parts, state, match);
}

gboolean
r_parser_macaddr(gchar *str, gint *len, const gchar *param, gpointer state, RParserMatch *match)
{
  return _r_parser_lladdr(str, len, 17, 6, state, match);
}

gboolean
r_parser_ipv4(gchar *str, gint *len, const gchar *param, gpointer state, RParserMatch *match)
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

          if (G_UNLIKELY(dots == 3))
            break;

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
r_parser_ipv6(gchar *str, gint *len, const gchar *param, gpointer state, RParserMatch *match)
{
  gint colons = 0;
  gint dots = 0;
  gint octet = 0;
  gint digit = 16;
  gboolean shortened = FALSE;

  *len = 0;

  while (1)
    {
      if (str[*len] == ':')
        {
          if (G_UNLIKELY(octet > 0xffff || (octet == -1 && shortened)))
            return FALSE;

          if (G_UNLIKELY(colons == 7 || dots == 3))
            break;

          if (G_UNLIKELY(digit == 10))
            return FALSE;

          if (octet == -1)
            shortened = TRUE;

          colons++;
          octet = -1;
        }
      else if (g_ascii_isxdigit(str[*len]))
        {
          if (octet == -1)
            octet = 0;
          else
            octet *= digit;

          octet += g_ascii_xdigit_value(str[*len]);
        }
      else if (str[*len] == '.')
        {
          if (G_UNLIKELY((digit == 10 && octet > 255)))
            return FALSE;

          if (G_UNLIKELY((digit == 16 && octet > 597) || octet == -1 || colons == 7 || dots == 3))
            break;

          dots++;
          octet = -1;
          digit = 10;
        }
      else
        break;

      (*len)++;
    }

  if (G_UNLIKELY(*len > 0 && str[*len-1] == '.'))
    {
      (*len)--;
      dots--;
    }
  else if (G_UNLIKELY(*len > 1 && str[*len-1] == ':' && str[*len - 2] != ':'))
    {
      (*len)--;
      colons--;
    }

  if (colons < 2 || colons > 7 || (digit == 10 && octet > 255) || (digit == 16 && octet > 0xffff) ||
      !(dots == 0 || dots == 3) || (!shortened && colons < 7 && dots == 0))
    return FALSE;

  return TRUE;
}

gboolean
r_parser_ip(gchar *str, gint *len, const gchar *param, gpointer state, RParserMatch *match)
{
  return r_parser_ipv4(str, len, param, state, match) || r_parser_ipv6(str, len, param, state, match);
}

gboolean
r_parser_float(gchar *str, gint *len, const gchar *param, gpointer state, RParserMatch *match)
{
  gboolean dot = FALSE;

  *len = 0;
  if (str[*len] == '-')
    (*len)++;

  while (g_ascii_isdigit(str[*len]) || (!dot && str[*len] == '.' && (dot = TRUE)))
    (*len)++;

  if (*len > 0 && (str[*len] == 'e' || str[*len] == 'E'))
    {
      (*len)++;

      if (str[*len] == '-')
        (*len)++;

      while (g_ascii_isdigit(str[*len]))
        (*len)++;
    }

  if (*len)
    return TRUE;

  return FALSE;
}

gboolean
r_parser_number(gchar *str, gint *len, const gchar *param, gpointer state, RParserMatch *match)
{
  gint min_len = 1;

  if (g_str_has_prefix(str, "0x") || g_str_has_prefix(str, "0X"))
    {
      *len = 2;
      min_len += 2;

      while (g_ascii_isxdigit(str[*len]))
        (*len)++;

    }
  else
    {
      *len = 0;

      if (str[*len] == '-')
        {
          (*len)++;
          min_len++;
        }

      while (g_ascii_isdigit(str[*len]))
        (*len)++;
    }

  if (*len >= min_len)
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
static RParserNode *
r_new_pnode(gchar *key)
{
  RParserNode *parser_node = g_new0(RParserNode, 1);
  gchar **params = g_strsplit(key, ":", 3);
  guint params_len = g_strv_length(params);

  parser_node->first = CHAR_MIN;
  parser_node->last = CHAR_MAX;

  if (strcmp(params[0], "IPv4") == 0)
    {
      parser_node->parse = r_parser_ipv4;
      parser_node->type = RPT_IPV4;
      parser_node->first = '0';
      parser_node->last = '9';
    }
  else if (strcmp(params[0], "IPv6") == 0)
    {
      parser_node->parse = r_parser_ipv6;
      parser_node->type = RPT_IPV6;
    }
  else if (strcmp(params[0], "IPvANY") == 0)
    {
      parser_node->parse = r_parser_ip;
      parser_node->type = RPT_IP;
    }
  else if (strcmp(params[0], "MACADDR") == 0)
    {
      parser_node->parse = r_parser_macaddr;
      parser_node->type = RPT_MACADDR;
    }
  else if (strcmp(params[0], "NUMBER") == 0)
    {
      parser_node->parse = r_parser_number;
      parser_node->type = RPT_NUMBER;
      parser_node->first = '-';
      parser_node->last = '9';
    }
  else if (strcmp(params[0], "FLOAT") == 0 || strcmp(params[0], "DOUBLE") == 0)
    {
      /* DOUBLE is a deprecated alias for FLOAT */
      parser_node->parse = r_parser_float;
      parser_node->type = RPT_FLOAT;
      parser_node->first = '-';
      parser_node->last = '9';
    }
  else if (strcmp(params[0], "STRING") == 0)
    {
      parser_node->parse = r_parser_string;
      parser_node->type = RPT_STRING;
    }
  else if (strcmp(params[0], "ESTRING") == 0)
    {
      if (params_len == 3)
        {
          parser_node->parse = r_parser_estring_c;
          parser_node->type = RPT_ESTRING;

          if (params[2] && (strlen(params[2]) > 1))
            {
              gint len = strlen(params[2]);
              parser_node->state = GINT_TO_POINTER(len);
              parser_node->parse = r_parser_estring;
            }
        }
      else
        {
          g_free(parser_node);
          msg_error("Missing ESTRING parser parameters",
                    evt_tag_str("type", params[0]));
          parser_node = NULL;
        }

    }
  else if (strcmp(params[0], "NLSTRING") == 0)
    {
      if (params_len <= 2)
        {
          parser_node->parse = r_parser_nlstring;
          parser_node->type = RPT_NLSTRING;
        }
      else
        {
          g_free(parser_node);
          msg_error("Too many arguments to NLSTRING, no 3rd parameter supported");
          parser_node = NULL;
        }
    }
  else if (strcmp(params[0], "PCRE") == 0)
    {
      parser_node->parse = r_parser_pcre;
      parser_node->type = RPT_PCRE;

      if (params[2])
        {
          parser_node->free_state = r_parser_pcre_free_state;
          parser_node->state = r_parser_pcre_compile_state(params[2]);
        }
      else
        {
          g_free(parser_node);
          msg_error("Missing regular expression as 3rd argument",
                    evt_tag_str("type", params[0]));
          parser_node = NULL;
        }
    }
  else if (strcmp(params[0], "ANYSTRING") == 0)
    {
      parser_node->parse = r_parser_anystring;
      parser_node->type = RPT_ANYSTRING;
    }
  else if (strcmp(params[0], "SET") == 0)
    {
      if (params_len == 3)
        {
          parser_node->parse = r_parser_set;
          parser_node->type = RPT_SET;
        }
      else
        {
          g_free(parser_node);
          msg_error("Missing SET parser parameters",
                    evt_tag_str("type", params[0]));
          parser_node = NULL;
        }
    }
  else if (strcmp(params[0], "EMAIL") == 0)
    {
      parser_node->parse = r_parser_email;
      parser_node->type = RPT_EMAIL;
    }
  else if (strcmp(params[0], "HOSTNAME") == 0)
    {
      parser_node->parse = r_parser_hostname;
      parser_node->type = RPT_HOSTNAME;
    }
  else if (strcmp(params[0], "LLADDR") == 0)
    {
      parser_node->parse = r_parser_lladdr;
      parser_node->type = RPT_LLADDR;
    }
  else if (g_str_has_prefix(params[0], "QSTRING"))
    {
      if (params_len == 3)
        {
          gchar *state = (gchar *) &(parser_node->state);

          parser_node->parse = r_parser_qstring;
          parser_node->type = RPT_QSTRING;
          parser_node->first = params[2][0];
          parser_node->last = params[2][0];

          if (params_len >= 2 && params[2] && strlen(params[2]) == 2)
            state[0] = params[2][1];
          else
            state[0] = params[2][0];
        }
      else
        {
          g_free(parser_node);
          msg_error("Missing QSTRING parser parameters",
                    evt_tag_str("type", params[0]));
          parser_node = NULL;
        }
    }
  else
    {
      g_free(parser_node);
      msg_error("Unknown parser type specified",
                evt_tag_str("type", params[0]));
      parser_node = NULL;
    }

  if (parser_node && params[1])
    {
      if (params[1][0])
        parser_node->handle = log_msg_get_value_handle(params[1]);

      if (params[2])
        parser_node->param = g_strdup(params[2]);
    }


  g_strfreev(params);

  return parser_node;
}


void
r_free_pnode_only(RParserNode *parser)
{
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
r_add_child_check(RNode *root, gchar *key, gpointer value, RNodeGetValueFunc value_func)
{
  gchar *at;

  if (((at = strchr(key, '@')) != NULL))
    {
      /* there is an @ somewhere in the string */
      if ((at - key) > 0)
        {
          /* there are some literal characters before @ so add a new child till @ */
          *at = '\0';

          RNode *child = r_new_node(key, NULL);
          r_add_child(root, child);

          /* and insert the rest beginning from @ under the newly created literal node */
          *at = '@';
          r_insert_node(child, at, value, value_func);
        }
      else
        {
          /* @ is the first so let's insert it simply and let insert_node handle @ */
          r_insert_node(root, key, value, value_func);
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
  parent->pchildren = g_realloc(parent->pchildren, (sizeof(RNode *) * (parent->num_pchildren + 1)));

  parent->pchildren[(parent->num_pchildren)++] = child;
}

gboolean
r_equal_pnode(RParserNode *a, RParserNode *b)
{
  return ((a->parse == b->parse) &&
          (a->handle == b->handle) &&
          ((a->param == NULL && b->param == NULL) ||
           (a->param != NULL && b->param != NULL && g_str_equal(a->param, b->param)))
         );
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
r_find_child_by_first_character(RNode *root, char key)
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
r_insert_node(RNode *root, gchar *key, gpointer value, RNodeGetValueFunc value_func)
{
  RNode *node;
  gint keylen = strlen(key);
  gint nodelen = root->keylen;
  gint i = 0;

  if (key[0] == '@')
    {
      gchar *end;

      if (keylen >= 2 && key[1] == '@')
        {
          /* we found and escape, so check if we already have a child with '@', or add a child like that */
          node = r_find_child_by_first_character(root, key[1]);

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
                msg_error("Duplicate key in parser radix tree",
                          evt_tag_str("key", "@"),
                          evt_tag_str("value", value_func ? value_func(value) : "unknown"),
                          evt_tag_str("other-value", value_func ? value_func(node->value) : "unknown"));
            }

          /* go down building the tree if there is key left */
          if (keylen > 2)
            r_insert_node(node, key + 2, value, value_func);

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
                  r_insert_node(node, end + 1, value, value_func);
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
                                evt_tag_str("name", log_msg_get_value_name(node->parser->handle, NULL)),
                                evt_tag_str("value", value_func ? value_func(value) : "unknown"),
                                evt_tag_str("other-value", value_func ? value_func(node->value) : "unknown"));
                    }
                }
            }

        }
      else
        msg_error("Key contains '@' without escaping",
                  evt_tag_str("key", key),
                  evt_tag_str("value", value_func ? value_func(value) : "unknown"));
    }
  else
    {
      /* we are not starting with @ sign or we are not interested in @ at all */

      while (i < keylen && i < nodelen)
        {
          /* check if key is the same, or if it is a parser */
          if ((key[i] != root->key[i]) || (key[i] == '@'))
            break;

          i++;
        }


      if (nodelen == 0 || i == 0 || (i < keylen && i >= nodelen))
        {
          /*either at the root or we need to go down the tree on the right child */

          node = r_find_child_by_first_character(root, key[i]);

          if (node)
            {
              /* @ is always a singel node, and we also have an @ so insert us under root */
              if (key[i] == '@')
                r_insert_node(root, key + i, value, value_func);
              else
                r_insert_node(node, key + i, value, value_func);
            }
          else
            {
              r_add_child_check(root, key + i, value, value_func);
            }
        }
      else if (i == keylen && i == nodelen)
        {
          /* exact match */

          if (!root->value)
            root->value = value;
          else
            msg_error("Duplicate key in radix tree",
                      evt_tag_str("key", key),
                      evt_tag_str("value", value_func ? value_func(value) : "unknown"));
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
              r_add_child_check(root, key + i, value, value_func);
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
          r_add_child_check(root, key + i, value, value_func);
        }
    }
}

typedef struct _RFindNodeState
{
  gboolean require_complete_match;
  gboolean partial_match_found;
  gchar *whole_key;
  GArray *stored_matches;
  GArray *dbg_list;
  GPtrArray *applicable_nodes;
} RFindNodeState;

static RNode *_find_node_recursively(RFindNodeState *state, RNode *root, gchar *key, gint keylen);

static void
_add_debug_info(RFindNodeState *state, RNode *node, RParserNode *pnode, gint i, gint match_off, gint match_len)
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
_add_literal_match_to_debug_info(RFindNodeState *state, RNode *node, gint literal_prefix)
{
  _add_debug_info(state, node, NULL, literal_prefix, 0, 0);
}

static void
_add_parser_match_debug_info(RFindNodeState *state, RNode *root, RParserNode *parser_node, gchar *remaining_key,
                             gint extracted_match_len, RParserMatch *match_slot)
{
  if (state->dbg_list && match_slot)
    _add_debug_info(state, root, parser_node, extracted_match_len,
                    ((gint16) match_slot->ofs) + remaining_key - state->whole_key, ((gint16) match_slot->len) + extracted_match_len);
}


static void
_truncate_debug_info(RFindNodeState *state, gint truncated_size)
{
  if (state->dbg_list)
    g_array_set_size(state->dbg_list, truncated_size);
}

static void
_find_matching_literal_prefix(RNode *root, gchar *key, gint keylen,
                              gint *literal_prefix_inputlen,
                              gint *literal_prefix_radixlen)
{
  gint current_node_key_length = root->keylen;
  gint input_length;
  gint radix_length;

  if (current_node_key_length < 1)
    radix_length = input_length = 0;
  else
    {
      /* this is a prefix match algorithm, we are interested how long the
       * common part between key and root->key is.  Currently this uses a
       * byte-by-byte comparison, using a 64/32/16 bit units would be
       * better.  We had a PoC code to do that, and the performance
       * difference wasn't big enough to offset the complexity so it was
       * removed. We might want to rerun perf tests and see if we could
       * speed things up, but db-parser() seems fast enough as it is.
       */
      input_length = radix_length = 0;
      while (input_length < keylen && radix_length < current_node_key_length)
        {
          if (key[input_length] == '\r' && root->key[radix_length] == '\n')
            {
              /* skip CR from input if the radix contains a newline */
              input_length++;
            }
          if (key[input_length] != root->key[radix_length])
            break;

          input_length++;
          radix_length++;
        }
    }
  *literal_prefix_inputlen = input_length;
  *literal_prefix_radixlen = radix_length;
}

static RNode *
_find_child_by_remaining_key(RFindNodeState *state, RNode *root, gchar *remaining_key, gint remaining_keylen)
{
  RNode *candidate;

  if (remaining_keylen >= 2 && remaining_key[0] == '\r' && remaining_key[1] == '\n')
    {
      remaining_key++;
      remaining_keylen--;
    }
  candidate = r_find_child_by_first_character(root, remaining_key[0]);
  if (candidate)
    return _find_node_recursively(state, candidate, remaining_key, remaining_keylen);
  return NULL;
}

static gint
_alloc_slot_in_matches(RFindNodeState *state)
{
  gint matches_base = 0;

  if (state->stored_matches)
    {
      matches_base = state->stored_matches->len;

      g_array_set_size(state->stored_matches, matches_base + 1);
    }
  return matches_base;
}

static RParserMatch *
_get_match_slot(RFindNodeState *state, gint matches_base)
{
  if (state->stored_matches)
    return &g_array_index(state->stored_matches, RParserMatch, matches_base);
  return NULL;
}

static RParserMatch *
_clear_match_slot(RFindNodeState *state, gint matches_base)
{
  RParserMatch *match = NULL;

  if (state->stored_matches)
    {
      match = _get_match_slot(state, matches_base);
      memset(match, 0, sizeof(*match));
    }
  return match;
}


static void
_reset_matches_to_original_state(RFindNodeState *state, gint matches_base)
{
  if (state->stored_matches)
    g_array_set_size(state->stored_matches, matches_base);
}

static gboolean
_is_pnode_matching_initial_character(RParserNode *parser_node, gchar *key)
{
  return (parser_node->first <= key[0]) && (key[0] <= parser_node->last);
}

static gboolean
_pnode_try_parse(RParserNode *parser_node, gchar *key, gint *extracted_match_len, RParserMatch *match)
{
  if (!_is_pnode_matching_initial_character(parser_node, key))
    return FALSE;

  if (!parser_node->parse(key, extracted_match_len, parser_node->param, parser_node->state, match))
    return FALSE;

  return TRUE;
}

static void
_fixup_match_offsets(RFindNodeState *state, RParserNode *parser_node, gint extracted_match_len, gchar *remaining_key,
                     RParserMatch *match)
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
}

static void
_clear_match_content(RParserMatch *match)
{
  if (match->match)
    {
      /* free the stored match, if this was a dead-end */
      g_free(match->match);
      match->match = NULL;
    }
}

static RNode *
_try_parse_with_a_given_child(RFindNodeState *state, RNode *root, gint parser_ndx, gint matches_slot_index,
                              gchar *remaining_key, gint remaining_keylen)
{
  RNode *child_node = root->pchildren[parser_ndx];
  RParserNode *parser_node = child_node->parser;
  RParserMatch *match_slot = NULL;
  gint extracted_match_len;
  RNode *ret = NULL;

  match_slot = _clear_match_slot(state, matches_slot_index);

  if (_pnode_try_parse(parser_node, remaining_key, &extracted_match_len, match_slot))
    {

      /* FIXME: we don't try to find the longest match in case
       * the radix tree is split on a parser node. The correct
       * approach would be to try all parsers and select the
       * best match, however it is quite expensive and difficult
       * to implement and we don't really expect this to be a
       * realistic case. A log message is printed if such a
       * collision occurs, so there's a slight chance we'll
       * recognize if this happens in real life. */

      _add_parser_match_debug_info(state, root, parser_node, remaining_key, extracted_match_len, match_slot);
      ret = _find_node_recursively(state, root->pchildren[parser_ndx], remaining_key + extracted_match_len,
                                   remaining_keylen - extracted_match_len);

      /* we have to look up "match_slot" again as the GArray may have
       * moved the data in case r_find_node() expanded it above */
      match_slot = _get_match_slot(state, matches_slot_index);
      if (match_slot)
        {
          if (ret)
            _fixup_match_offsets(state, parser_node, extracted_match_len, remaining_key, match_slot);
          else
            _clear_match_content(match_slot);
        }
    }
  return ret;

}

static RNode *
_find_child_by_parser(RFindNodeState *state, RNode *root, gchar *remaining_key, gint remaining_keylen)
{
  gint dbg_list_base = state->dbg_list ? state->dbg_list->len : 0;
  gint matches_slot_index = 0;
  gint parser_ndx;
  RNode *ret = NULL;

  matches_slot_index = _alloc_slot_in_matches(state);
  for (parser_ndx = 0; !ret && parser_ndx < root->num_pchildren; parser_ndx++)
    {
      _truncate_debug_info(state, dbg_list_base);
      ret = _try_parse_with_a_given_child(state, root, parser_ndx, matches_slot_index, remaining_key, remaining_keylen);
    }
  if (!ret && state->stored_matches)
    {
      /* the values in the stored_matches array has already been freed if we come here */
      _reset_matches_to_original_state(state, matches_slot_index);
    }
  return ret;
}

static RNode *
_find_node_recursively(RFindNodeState *state, RNode *root, gchar *key, gint keylen)
{
  gint literal_prefix_inputlen, literal_prefix_radixlen;

  _find_matching_literal_prefix(root, key, keylen,
                                &literal_prefix_inputlen,
                                &literal_prefix_radixlen);
  _add_literal_match_to_debug_info(state, root, literal_prefix_inputlen);

  msg_trace("Looking up node in the radix tree",
            evt_tag_int("literal_prefix_inputlen", literal_prefix_inputlen),
            evt_tag_int("literal_prefix_radixlen", literal_prefix_radixlen),
            evt_tag_int("root->keylen", root->keylen),
            evt_tag_int("keylen", keylen),
            evt_tag_str("root_key", root->key),
            evt_tag_str("key", key));

  if (literal_prefix_inputlen == keylen && (literal_prefix_radixlen == root->keylen || root->keylen == -1))
    {
      /* key completely consumed by the literal */

      if (state->applicable_nodes)
        {
          /* collect all matching nodes */
          g_ptr_array_add(state->applicable_nodes, root);
          return NULL;
        }

      if (root->value)
        return root;
    }
  else if ((root->keylen < 1) || (literal_prefix_inputlen < keylen && literal_prefix_radixlen >= root->keylen))
    {
      /* we matched the key partially, go on with child nodes */
      RNode *ret;
      gchar *remaining_key = key + literal_prefix_inputlen;
      gint remaining_keylen = keylen - literal_prefix_inputlen;

      /* prefer a literal match over parsers */
      ret = _find_child_by_remaining_key(state, root, remaining_key, remaining_keylen);

      /* then try parsers in order */
      if (!ret)
        ret = _find_child_by_parser(state, root, remaining_key, remaining_keylen);

      if (!ret && root->value)
        {
          if (!state->require_complete_match)
            return root;
          state->partial_match_found = TRUE;
        }

      return ret;
    }

  return NULL;
}

static RNode *
_find_node_with_state(RFindNodeState *state, RNode *root, gchar *key, gint keylen)
{
  RNode *ret;

  state->require_complete_match = TRUE;
  state->partial_match_found = FALSE;
  ret = _find_node_recursively(state, root, key, keylen);
  if (!ret && state->partial_match_found)
    {
      state->require_complete_match = FALSE;
      ret = _find_node_recursively(state, root, key, keylen);
    }
  return ret;
}

RNode *
r_find_node(RNode *root, gchar *key, gint keylen, GArray *stored_matches)
{
  RFindNodeState state =
  {
    .whole_key = key,
    .stored_matches = stored_matches,
  };

  return _find_node_with_state(&state, root, key, keylen);
}

RNode *
r_find_node_dbg(RNode *root, gchar *key, gint keylen, GArray *stored_matches, GArray *dbg_list)
{
  RFindNodeState state =
  {
    .whole_key = key,
    .stored_matches = stored_matches,
    .dbg_list = dbg_list,
  };

  return _find_node_with_state(&state, root, key, keylen);
}

gchar **
r_find_all_applicable_nodes(RNode *root, gchar *key, gint keylen, RNodeGetValueFunc value_func)
{
  RFindNodeState state =
  {
    .whole_key = key,
  };
  gint i;
  GPtrArray *result;

  state.applicable_nodes = g_ptr_array_new();
  state.require_complete_match = TRUE;
  _find_node_recursively(&state, root, key, keylen);

  result = g_ptr_array_new();
  for (i = 0; i < state.applicable_nodes->len; i++)
    {
      RNode *node = (RNode *) g_ptr_array_index(state.applicable_nodes, i);
      g_ptr_array_add(result, g_strdup(value_func(node->value)));
    }
  g_ptr_array_add(result, NULL);
  g_ptr_array_free(state.applicable_nodes, TRUE);
  return (gchar **) g_ptr_array_free(result, FALSE);
}

/**
 * r_new_node:
 */
RNode *
r_new_node(const gchar *key, gpointer value)
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
