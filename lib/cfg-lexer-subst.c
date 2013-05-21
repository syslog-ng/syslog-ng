#include "cfg-lexer-subst.h"
#include "cfg-args.h"
#include "cfg-lexer.h"
#include "cfg-grammar.h"

#include <string.h>
#include <stdlib.h>

typedef enum _CfgLexerStringTrackState
{
  CLS_NOT_STRING,
  CLS_WITHIN_STRING,
  CLS_WITHIN_STRING_QUOTE,
  CLS_WITHIN_QSTRING,
} CfgLexerStringTrackState;

/* substitute backtick references into lexer input */
struct _CfgLexerSubst
{
  CfgArgs *globals;
  CfgArgs *defs;
  CfgArgs *args;
  CfgLexerStringTrackState string_state;
  GString *result_buffer;
};

static const gchar *
_lookup_value(CfgLexerSubst *self, const gchar *name)
{
  const gchar *arg;

  if (self->args && (arg = cfg_args_get(self->args, name)))
    ;
  else if (self->defs && (arg = cfg_args_get(self->defs, name)))
    ;
  else if (self->globals && (arg = cfg_args_get(self->globals, name)))
    ;
  else if ((arg = g_getenv(name)))
    ;
  else
    arg = NULL;

  return arg;
}

static CfgLexerStringTrackState
_track_string_state(CfgLexerSubst *self, CfgLexerStringTrackState last_state, gchar *p)
{
  switch (last_state)
    {
    case CLS_NOT_STRING:
      if (*p == '"')
        return CLS_WITHIN_STRING;
      else if (*p == '\'')
        return CLS_WITHIN_QSTRING;
      return CLS_NOT_STRING;
    case CLS_WITHIN_STRING:
      if (*p == '\\')
        return CLS_WITHIN_STRING_QUOTE;
      else if (*p == '"')
        return CLS_NOT_STRING;
      return CLS_WITHIN_STRING;
    case CLS_WITHIN_STRING_QUOTE:
      return CLS_WITHIN_STRING;
    case CLS_WITHIN_QSTRING:
      if (*p == '\'')
        return CLS_NOT_STRING;
      return CLS_WITHIN_QSTRING;
    }
  g_assert_not_reached();
}

static gchar *
_extract_string_literal(const gchar *value)
{
  CfgLexer *lexer;
  gint token, look_ahead_token;
  YYSTYPE yylval, look_ahead_yylval;
  YYLTYPE yylloc, look_ahead_yylloc;
  gchar *result = NULL;

  lexer = cfg_lexer_new_buffer(value, strlen(value));
  token = cfg_lexer_lex(lexer, &yylval, &yylloc);
  if (token == LL_STRING)
    {
      look_ahead_token = cfg_lexer_lex(lexer, &look_ahead_yylval, &look_ahead_yylloc);
      if (!look_ahead_token)
        result = g_strdup(yylval.cptr);

      cfg_lexer_free_token(&look_ahead_yylval);
    }
  cfg_lexer_free_token(&yylval);

  cfg_lexer_free(lexer);
  return result;
}

static gboolean
_encode_as_string(CfgLexerSubst *self, const gchar *value, GError **error)
{
  gint i;

  g_return_val_if_fail(error == NULL || (*error) == NULL, FALSE);
  for (i = 0; value[i]; i++)
    {
      gchar p = value[i];

      if (p == '"')
        g_string_append(self->result_buffer, "\\\"");
      else if (p == '\n')
        g_string_append(self->result_buffer, "\\n");
      else if (p == '\r')
        g_string_append(self->result_buffer, "\\r");
      else if (p == '\\')
        g_string_append(self->result_buffer, "\\\\");
      else
        g_string_append_c(self->result_buffer, p);
    }
  return TRUE;
}

static gboolean
_encode_as_qstring(CfgLexerSubst *self, const gchar *value, GError **error)
{
  gint i;

  g_return_val_if_fail(error == NULL || (*error) == NULL, FALSE);
  for (i = 0; value[i]; i++)
    {
      gchar p = value[i];

      if (p == '\'')
        {
          g_set_error(error, CFG_LEXER_ERROR, CFG_LEXER_CANNOT_REPRESENT_APOSTROPHES_IN_QSTRINGS, "cannot represent apostrophes within apostroph-enclosed string");
          return FALSE;
        }
      g_string_append_c(self->result_buffer, p);
    }
  return TRUE;
}

static gboolean
_append_value(CfgLexerSubst *self, const gchar *value, GError **error)
{
  g_return_val_if_fail(error == NULL || (*error) == NULL, FALSE);

  if (self->string_state == CLS_NOT_STRING)
    {
      g_string_append(self->result_buffer, value);
    }
  else
    {
      gchar *string_literal;

      string_literal = _extract_string_literal(value);
      if (string_literal)
        {
          switch (self->string_state)
            {
            case CLS_WITHIN_STRING:
              return _encode_as_string(self, string_literal, error);
            case CLS_WITHIN_QSTRING:
              return _encode_as_qstring(self, string_literal, error);
            case CLS_WITHIN_STRING_QUOTE:
            default:
              break;
            }
          g_free(string_literal);
        }
      else
        g_string_append(self->result_buffer, value);
    }
  return TRUE;
}

gchar *
cfg_lexer_subst_invoke(CfgLexerSubst *self, gchar *cptr, gsize *length, GError **error)
{
  gboolean backtick = FALSE;
  gchar *p, *ref_start = cptr;
  GString *result;

  g_return_val_if_fail(error == NULL || (*error) == NULL, FALSE);

  result = g_string_sized_new(32);
  self->result_buffer = result;
  p = cptr;
  while (*p)
    {
      self->string_state = _track_string_state(self, self->string_state, p);

      if (!backtick && (*p) == '`')
        {
          /* start of reference */
          backtick = TRUE;
          ref_start = p + 1;
        }
      else if (backtick && (*p) == '`')
        {
          /* end of reference */
          backtick = FALSE;

          if (ref_start == p)
            {
              /* empty ref, just include a ` character */
              g_string_append_c(result, '`');
            }
          else
            {
              const gchar *value;

              *p = 0;
              value = _lookup_value(self, ref_start);
              *p = '`';
              if (!_append_value(self, value ? : "", error))
                goto error;
            }
        }
      else if (!backtick)
        g_string_append_c(result, *p);

      p++;
    }
  self->result_buffer = NULL;

  if (backtick)
    {
      g_set_error(error, CFG_LEXER_ERROR, CFG_LEXER_MISSING_BACKTICK_PAIR, "missing closing backtick (`) character");
      goto error;
    }

  *length = result->len;
  return g_string_free(result, FALSE);
 error:
  g_string_free(result, TRUE);
  return NULL;

}

CfgLexerSubst *
cfg_lexer_subst_new(CfgArgs *globals, CfgArgs *defs, CfgArgs *args)
{
  CfgLexerSubst *self = g_new0(CfgLexerSubst, 1);

  self->globals = globals;
  self->defs = defs;
  self->args = args;
  return self;
}

void
cfg_lexer_subst_free(CfgLexerSubst *self)
{
  cfg_args_unref(self->globals);
  cfg_args_unref(self->defs);
  cfg_args_unref(self->args);
  g_free(self);
}
