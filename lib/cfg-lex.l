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

static gint
yy_input_run_backtick_substitution(CfgLexer *self, gchar *buf, gsize buf_size)
{
  gchar *res;
  gsize len;
  GError *error = NULL;
  YYLTYPE *cur_lloc = &self->include_stack[self->include_depth].lloc;

  res = cfg_lexer_subst_args_in_input(self->cfg->globals, NULL, NULL, buf, -1, &len, &error);
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
      if (len > buf_size)
        {
          msg_error("Error performing backtick substitution in configuration file",
                    evt_tag_str("error", "lexer buffer is too small to hold substituted result"),
                    evt_tag_str("filename", cur_lloc->level->name),
                    evt_tag_printf("line", "%d:%d", cur_lloc->first_line, cur_lloc->first_column));
          goto error;
        }
      else
        {
          memcpy(buf, res, len);
        }
      g_free(res);
      return len;
    }
 error:
  return -1;
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
          rc = yy_input_run_backtick_substitution(yyextra, buf, max_size); \
          if (rc < 0)                                                   \
            {                                                           \
              YY_FATAL_ERROR("backtick substitution failure");          \
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
              evt_tag_str("error", msg),				\
              NULL);							\
    longjmp(__yyg->yyextra_r->fatal_error, 1);				\
  } while(0)


static void
_cfg_lex_new_line(CfgLexer *lexer)
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
                             int ch;

                             ch = input(yyscanner);
                             while (ch != '\n' && ch != EOF && ch != 0)
                               {
                                 if (yyextra->token_text)
                                   g_string_append_c(yyextra->token_text, ch);
                                 ch = input(yyscanner);
                               }
                             if (ch == '\n')
                               {
                                 unput(ch);
                               }
                           }
^@                         {
                             return LL_PRAGMA;
                           }
<*>\r?\n                   {
                             *yylloc = yyextra->include_stack[yyextra->include_depth].lloc;
                             _cfg_lex_new_line(yyextra);
                             if (yyextra->tokenize_eol)
                               return LL_EOL;
                             else
                               g_string_append(yyextra->string_buffer, yytext);
                           }
{white}+		   ;
\.\.                       { return LL_DOTDOT; }
\.\.\.                     { return LL_DOTDOTDOT; }
(-|\+)?{digit}+\.{digit}+  { yylval->fnum = strtod(yytext, NULL); return LL_FLOAT; }
0x{xdigit}+ 		   { yylval->num = strtoll(yytext + 2, NULL, 16); return LL_NUMBER; }
0{odigit}+		   { yylval->num = strtoll(yytext + 1, NULL, 8); return LL_NUMBER; }
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
<string>[^"\\]+		   { g_string_append(yyextra->string_buffer, yytext); }
<qstring>[^']+		   { g_string_append(yyextra->string_buffer, yytext); }
<qstring>\'		   {
			     yy_pop_state(yyscanner);
			     yylval->cptr = strdup(yyextra->string_buffer->str);
			     return LL_STRING;
			   }


     /* rules to parse a block as a LL_BLOCK token */
<block>{white}+            ;
<block>[^{( \t]+           { return LL_ERROR; }
<block>\({white}*\)        { yy_pop_state(yyscanner); yylval->cptr = NULL; return LL_BLOCK; }
<block>\({white}*\"\"{white}*\) { yy_pop_state(yyscanner); yylval->cptr = g_strdup(""); return LL_BLOCK; }
<block>\({white}*\'\'{white}*\) { yy_pop_state(yyscanner); yylval->cptr = g_strdup(""); return LL_BLOCK; }
<block>[{(]                {
                             if (yytext[0] != yyextra->block_boundary[0])
                               REJECT;
                             g_string_truncate(yyextra->string_buffer, 0);
                             yyextra->brace_count = 1;
                             yy_push_state(block_content, yyscanner);
                           }

<block_content>\"          {
                             g_string_append_c(yyextra->string_buffer, yytext[0]);
                             yy_push_state(block_string, yyscanner);
                           }

<block_content>\'          {
                             g_string_append_c(yyextra->string_buffer, yytext[0]);
                             yy_push_state(block_qstring, yyscanner);
                           }

<block_string>[^"]+        { g_string_append(yyextra->string_buffer, yytext); }
<block_string>\\\"         { g_string_append(yyextra->string_buffer, yytext); }
<block_string>\"           {
                             g_string_append_c(yyextra->string_buffer, yytext[0]);
                             yy_pop_state(yyscanner);
                           }

<block_qstring>\'          {
                             g_string_append_c(yyextra->string_buffer, yytext[0]);
                             yy_pop_state(yyscanner);
                           }
<block_qstring>[^']+       {
                             g_string_append(yyextra->string_buffer, yytext);
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

<block_content>[^{}()\"\'\n\r]+   { g_string_append(yyextra->string_buffer, yytext); }

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
