/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2011 Balázs Scheidler
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
%{

#include "syslog-ng.h"
#include "cfg-lexer.h"
#include "cfg-lexer-subst.h"
#include "cfg-grammar.h"
#include "messages.h"
#include "parse-number.h"

#include <string.h>
#include <strings.h>

static gboolean
yy_input_run_backtick_substitution(CfgLexer *self, gchar *buf, gsize buf_size, gsize *len)
{
  gchar *res;
  GError *error = NULL;
  YYLTYPE *cur_lloc = &self->include_stack[self->include_depth].lloc;

  res = cfg_lexer_subst_args_in_input(self->cfg->globals, NULL, NULL, buf, -1, len, &error);
  if (!res)
    {
      msg_error("Error performing backtick substitution in configuration file",
                evt_tag_str("error", error->message),
                evt_tag_str("filename", cur_lloc->level->name),
                evt_tag_printf("line", "%d:%d", cur_lloc->first_line, cur_lloc->first_column));
      g_clear_error(&error);
      goto error;
    }
  else
    {
      if (*len > buf_size)
        {
          msg_error("Error performing backtick substitution in configuration file, the target buffer is too small",
                    evt_tag_int("buf_size", buf_size),
                    evt_tag_str("filename", cur_lloc->level->name),
                    evt_tag_printf("line", "%d:%d", cur_lloc->first_line, cur_lloc->first_column));
          goto error;
        }
      else
        {
          memcpy(buf, res, *len);
        }
      g_free(res);
      return TRUE;
    }
 error:
  return FALSE;
}

static gboolean
yy_patch_include_statement(CfgLexer *self, gchar *buf, gsize buf_size, gsize *len)
{
  YYLTYPE *cur_lloc = &self->include_stack[self->include_depth].lloc;

  gchar *p = buf;
  p += strspn(p, " \t");
  if (strncmp(p, "include", 7) != 0)
    return TRUE;
  p += 7;
  if (*p != ' ' && *p != '\t')
    return TRUE;

  p = strrchr(p, ';');
  if (!p)
    return TRUE;
  *p = ' ';

  if (!cfg_is_config_version_older(configuration, VERSION_VALUE_3_20))
    {
      msg_error("Error parsing old style include statement (e.g. the one without the '@' prefix), "
                "support for it was removed in " VERSION_3_20 ", just add a '@' in front or revert to @version "
                VERSION_3_19 " or older",
                evt_tag_str("filename", cur_lloc->level->name),
                evt_tag_printf("line", "%d:%d", cur_lloc->first_line, cur_lloc->first_column));

      return FALSE;
    }

  if (*len + 1 > buf_size)
    {
      msg_error("Error performing the `include' patching to @include, the target buffer is too small",
                evt_tag_int("buf_size", buf_size),
                evt_tag_str("filename", cur_lloc->level->name),
                evt_tag_printf("line", "%d:%d", cur_lloc->first_line, cur_lloc->first_column));
      return FALSE;
    }

  memmove(buf + 1, buf, *len);
  buf[0] = '@';
  *len += 1;

  msg_warning("WARNING: the `include' keyword based inclusion syntax (e.g. the one without a '@' "
              "character in front) has been removed in " VERSION_3_20 ". Please prepend a '@' "
              "character in front of your include statement while updating your configuration "
              "to match the new version",
              evt_tag_str("filename", cur_lloc->level->name),
              evt_tag_printf("line", "%d:%d", cur_lloc->first_line, cur_lloc->first_column));
  return TRUE;
}

static gint
yy_filter_input(CfgLexer *self, gchar *buf, gsize buf_size)
{
  gsize len = strlen(buf);

  if (!yy_input_run_backtick_substitution(self, buf, buf_size, &len))
    return -1;
  if (!yy_patch_include_statement(self, buf, buf_size, &len))
    return -1;
  return len;
}

#define YY_INPUT(buf, result, max_size)                                 \
  do                                                                    \
    {                                                                   \
      if (!fgets(buf, max_size, yyin))                                  \
        result = YY_NULL;                                               \
      else if (YY_START >= block)                                       \
        result = strlen(buf);                                           \
      else                                                              \
        {                                                               \
          gint rc;                                                      \
          rc = yy_filter_input(yyextra, buf, max_size); 		\
          if (rc < 0)                                                   \
            {                                                           \
              YY_FATAL_ERROR("configuration input filtering failure");  \
              result = YY_NULL;                                         \
            }                                                           \
          else                                                          \
            {                                                           \
              result = rc;                                              \
            }                                                           \
        }                                                               \
    } while(0)

#define YY_USER_ACTION                                                  \
  do {                                                                  \
    YYLTYPE *cur_lloc = &yyextra->include_stack[yyextra->include_depth].lloc; \
    if (YY_START == INITIAL)                                            \
      {                                                                 \
        cur_lloc->first_column = cur_lloc->last_column;                 \
      }                                                                 \
    cur_lloc->last_column = cur_lloc->last_column + yyleng;             \
    if (yyextra->token_text)                                            \
      g_string_append_len(yyextra->token_text, yytext, yyleng);         \
    *yylloc = *cur_lloc;                                                \
  } while(0);

#define YY_BREAK \
  do {                                                                  \
    if (yyextra->token_text && YY_START == INITIAL)                     \
      {                                                                 \
        g_string_append_len(yyextra->token_pretext, yyextra->token_text->str, yyextra->token_text->len); \
        g_string_truncate(yyextra->token_text, 0);                      \
      }                                                                 \
  } while (0);                                                          \
  break;

#define YY_FATAL_ERROR(msg)						\
  do {									\
    struct yyguts_t * __yyg = (struct yyguts_t*) yyscanner;		\
                                                                        \
    msg_error("Fatal error in configuration lexer, giving up",		\
              evt_tag_str("error", msg));							\
    longjmp(__yyg->yyextra_r->fatal_error, 1);				\
  } while(0)

#define _process_commented_line(scanner,content,cond) \
  do {                                                \
    int ch;                                           \
                                                      \
    ch = input(scanner);                              \
    while (ch != '\n' && ch != EOF && ch != 0)        \
      {                                               \
        if (cond)                                     \
          g_string_append_c(content, ch);             \
        ch = input(scanner);                          \
      }                                               \
    if (ch == '\n')                                   \
      {                                               \
        unput(ch);                                    \
      }                                               \
  } while (0)


static void
_cfg_lex_move_token_location_to_new_line(CfgLexer *lexer)
{
  CfgIncludeLevel *level = &lexer->include_stack[lexer->include_depth];

  /* if the last token span multiple lines (e.g.  block references), it
   * might happen that last_line != first_line, in that case, initialize the
   * first_line of the token to the subsequent line after last_line */

  level->lloc.first_line = level->lloc.last_line + 1;
  level->lloc.last_line = level->lloc.first_line;
  level->lloc.first_column = 1;
  level->lloc.last_column = 1;
}

static void
_cfg_lex_extend_token_location_to_next_line(CfgLexer *lexer)
{
  CfgIncludeLevel *level = &lexer->include_stack[lexer->include_depth];

  /* this function is used when encountering an NL character within a
   * multi-line string literal.
   *
   * Example:
   * |    "this is the start of the literal\
   * |    and this is the end"
   *
   *
   * In this case the string contains a escaped NL character, causing the
   * literal to span two distinct lines in the source file, which would be
   * dropped from the parsed string.
   *
   * In this case, we need to move yylloc to point from the end of the
   * current line to the beginning of the next line.  This way, by the time
   * we return the entire token, yylloc would be spanning the
   * starting-ending lines of the string literal as needed.
   * */

  level->lloc.last_line++;
  level->lloc.last_column = 1;
}
%}

%option bison-bridge bison-locations reentrant
%option prefix="_cfg_lexer_"
%option header-file="lex.yy.h"
%option outfile="lex.yy.c"
%option extra-type="CfgLexer *"
%option noyywrap
%option stack

white	[ \t]
digit	[0-9]
xdigit	[0-9a-fA-F]
odigit  [0-7]
alpha		[a-zA-Z]
alphanum	[a-zA-Z0-9]
word	[^ \#'"\(\)\{\}\[\]\\;\r\n\t,|\.@:]

/* block related states must be last, as we use this fact in YY_INPUT */
%x string
%x qstring
%x block
%x block_content
%x block_string
%x block_qstring
%%

\#                         {
                             _process_commented_line(yyscanner,yyextra->token_text,yyextra->token_text);
                           }
^@                         {
                             return LL_PRAGMA;
                           }

<*>\\\r?\n                 { _cfg_lex_extend_token_location_to_next_line(yyextra); }
\r?\n                      {
                             *yylloc = yyextra->include_stack[yyextra->include_depth].lloc;
                             _cfg_lex_move_token_location_to_new_line(yyextra);
                             if (yyextra->tokenize_eol)
                               return LL_EOL;
                             else
                               g_string_append(yyextra->string_buffer, yytext);
                           }
{white}+		   ;
\.\.                       { return LL_DOTDOT; }
\.\.\.                     { return LL_DOTDOTDOT; }
(-|\+)?{digit}+\.{digit}+  { yylval->fnum = strtod(yytext, NULL); return LL_FLOAT; }
0x{xdigit}+ 		   {
                             if (!parse_number_with_suffix(yytext, &yylval->num))
                               {
                                 return LL_ERROR;
                               }
                             return LL_NUMBER;
                           }
0{odigit}+		   {
                             if (!parse_number_with_suffix(yytext, &yylval->num))
                               {
                                 return LL_ERROR;
                               }
                             return LL_NUMBER;
                           }
(-|\+)?{digit}+(M|m|G|g|k|K)?(i|I)?(b|B)? {
                             if (!parse_number_with_suffix(yytext, &yylval->num))
                               {
                                 return LL_ERROR;
                               }
                             return LL_NUMBER;
                           }
({word}+(\.)?)*{word}+ 	   { return cfg_lexer_lookup_keyword(yyextra, yylval, yylloc, yytext); }
\(	      		   |
\)			   |
\;			   |
\{			   |
\}			   |
\[			   |
\]			   |
\:			   |
\|			   { return yytext[0]; }
\,			   ;

\"                         {
			     g_string_truncate(yyextra->string_buffer, 0);
			     yy_push_state(string, yyscanner);
			   }
\'			   {
			     g_string_truncate(yyextra->string_buffer, 0);
			     yy_push_state(qstring, yyscanner);
			   }
<string>\\a		   { g_string_append_c(yyextra->string_buffer, 7); }
<string>\\n	   	   { g_string_append_c(yyextra->string_buffer, 10); }
<string>\\r		   { g_string_append_c(yyextra->string_buffer, 13); }
<string>\\t		   { g_string_append_c(yyextra->string_buffer, 9); }
<string>\\v		   { g_string_append_c(yyextra->string_buffer, 11); }
<string>\\x{xdigit}{1,2}   { g_string_append_c(yyextra->string_buffer, strtol(yytext+2, NULL, 16)); }
<string>\\o{odigit}{1,3}   { g_string_append_c(yyextra->string_buffer, strtol(yytext+2, NULL, 8)); }
<string>\\[^anrtv]	   { g_string_append_c(yyextra->string_buffer, yytext[1]); }
<string>\"		   {
			     yy_pop_state(yyscanner);
			     yylval->cptr = strdup(yyextra->string_buffer->str);
			     return LL_STRING;
		           }
<string>[^"\\\r\n]+	   { g_string_append(yyextra->string_buffer, yytext); }
<qstring>[^'\r\n]+	   { g_string_append(yyextra->string_buffer, yytext); }
<qstring>\'		   {
			     yy_pop_state(yyscanner);
			     yylval->cptr = strdup(yyextra->string_buffer->str);
			     return LL_STRING;
			   }

    /* the rule below will only be applied to embedded newlines within
     * string/qstring.  There's a difference how we handle backslash quotation
     * of newline though.
     *
     * If a \\\n sequence is in a <string> (e.g. quoted newline)
     *   - the backslash is explicitly not allowed in the greedy rule that eats string characters
     *   - thus <*>\\\r?\n rule gets applied, at the top of the file, removing the newline
     *   - location tracking is preserved
     *
     * If a \n sequence is in a <string> (e.g. embedded newline)
     *   - the newline character is processed by the rule below, thus it is
     *     included as a literal newline
     *
     * If a \\\n sequence is in a <qstring> (e.g. quotation is not supported)
     *   - the backslash is eaten by the greedy rule for qstring
     *   - <*>\\\r?\n is not matching as the backslash is not there
     *   - the rule below gets matched and the newline is included in the string
     *
     * If a \n sequence is in a <qstring> (e.g. embedded newline)
     *   - the rule below gets matched and the newline is included in the string
     */

<string,qstring>\r?\n	   {
                             g_string_append(yyextra->string_buffer, yytext);
                             _cfg_lex_extend_token_location_to_next_line(yyextra);
                           }


    /* rules to parse a block as a LL_BLOCK token */
<block>{white}+            ;
<block>[^{( \t\r\n]+       { YY_FATAL_ERROR("Expected opening bracket as block boundary, e.g. opening brace or parenthesis"); return LL_ERROR; }
<block>\r?\n		   { _cfg_lex_move_token_location_to_new_line(yyextra); };
<block>\({white}*\)        { yy_pop_state(yyscanner); yylval->cptr = NULL; return LL_BLOCK; }
<block>\({white}*\"\"{white}*\) { yy_pop_state(yyscanner); yylval->cptr = g_strdup(""); return LL_BLOCK; }
<block>\({white}*\'\'{white}*\) { yy_pop_state(yyscanner); yylval->cptr = g_strdup(""); return LL_BLOCK; }

    /* start character of the block */
<block>[{(]                {
                             if (yytext[0] != yyextra->block_boundary[0])
                               REJECT;
                             g_string_truncate(yyextra->string_buffer, 0);
                             yyextra->brace_count = 1;
                             yy_push_state(block_content, yyscanner);
                           }

    /* block_content, handle embedded string/qstring/comments properly, count
     * braces so we only end the block if they are pairing up, unless they are
     * in a string/qstring/comment. */

<block_content>\"          {
                             g_string_append_c(yyextra->string_buffer, yytext[0]);
                             yy_push_state(block_string, yyscanner);
                           }

<block_content>\'          {
                             g_string_append_c(yyextra->string_buffer, yytext[0]);
                             yy_push_state(block_qstring, yyscanner);
                           }

<block_content>\#          {
                             g_string_append_c(yyextra->string_buffer, yytext[0]);
                             _process_commented_line(yyscanner, yyextra->string_buffer, TRUE);
                           }

<block_content>[{(]        {
                             g_string_append_c(yyextra->string_buffer, yytext[0]);
                             if (yytext[0] == yyextra->block_boundary[0])
                               yyextra->brace_count++;
                           }
<block_content>[})]        {
                             if (yytext[0] == yyextra->block_boundary[1])
                               yyextra->brace_count--;
                             if (yyextra->brace_count == 0)
                               {
                                 yy_pop_state(yyscanner);
                                 yy_pop_state(yyscanner);
                                 yylval->cptr = strdup(yyextra->string_buffer->str);
                                 return LL_BLOCK;
                               }
                             else
                               {
                                 g_string_append_c(yyextra->string_buffer, yytext[0]);
                               }

                           }

<block_content>[^{}()\"\'\n\r#]+   { g_string_append(yyextra->string_buffer, yytext); }

<block_string>[^"\r\n]+    { g_string_append(yyextra->string_buffer, yytext); }
<block_string>\\\"         { g_string_append(yyextra->string_buffer, yytext); }
<block_string>\"           {
                             g_string_append_c(yyextra->string_buffer, yytext[0]);
                             yy_pop_state(yyscanner);
                           }

<block_qstring>\'          {
                             g_string_append_c(yyextra->string_buffer, yytext[0]);
                             yy_pop_state(yyscanner);
                           }
<block_qstring>[^'\r\n]+   {
                             g_string_append(yyextra->string_buffer, yytext);
                           }

    /* The newlines embedded in block_string/block_qstring/block_content is
     * literally coped into the block token, even if there's backslash as the
     * last character.  Basically we behave just as if the entire block was a
     * qstring.
     */
<block_string,block_qstring,block_content>\r?\n    {
                             g_string_append(yyextra->string_buffer, yytext);
                             _cfg_lex_extend_token_location_to_next_line(yyextra);
                           }

<INITIAL><<EOF>>           {
                             if (!cfg_lexer_start_next_include(yyextra))
                               {
                                 if (yyextra->include_depth == 0)
                                   {
                                     *yylloc = yyextra->include_stack[0].lloc;
                                     yyterminate();
                                   }
                                 else
                                    return LL_ERROR;
                               }
                           }

%%


void
cfg_lexer_unput_string(CfgLexer *self, const char *str)
{
  int len = strlen(str);
  int i;

  for (i = len - 1; i >= 0; i--)
    {
      yyunput(str[i], _cfg_lexer_get_text(self->state), self->state);
      self->include_stack[self->include_depth].lloc.first_column--;
      self->include_stack[self->include_depth].lloc.last_column--;
    }
}

void
cfg_lexer_start_block_state(CfgLexer *self, const gchar block_boundary[2])
{
  memcpy(&self->block_boundary, block_boundary, sizeof(self->block_boundary));
  yy_push_state(block, self->state);
}

/* avoid warnings of unused symbols */
gpointer __dummy[] = { yy_top_state, yy_fatal_error };
