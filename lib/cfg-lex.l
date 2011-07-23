/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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
#include "cfg-grammar.h"
#include "messages.h"
#include "misc.h"

#include <string.h>
#include <strings.h>


#define YY_INPUT(buf, result, max_size)                               \
  do {                                                                \
    buf[0] = 0;                                                       \
    if (fgets(buf, max_size, yyin))                                   \
      {                                                                 \
        gchar *res;                                                     \
        gsize len = strlen(buf);                                        \
                                                                        \
        if (YY_START < block)                                           \
          res = cfg_lexer_subst_args(yyextra->globals, NULL, NULL, buf, &len); \
        else                                                            \
          res = FALSE;                                                  \
        if (!res)                                                       \
          {                                                             \
            result = strlen(buf);                                       \
          }                                                             \
        else                                                            \
          {                                                             \
            result = strlen(res);                                       \
            if (max_size < result)                                      \
              {                                                         \
                YY_FATAL_ERROR("Not enough buffer space in lexer");     \
                result = YY_NULL;                                       \
              }                                                         \
            else                                                        \
              {                                                         \
                memcpy(buf, res, result);                               \
              }                                                         \
            g_free(res);                                                \
          }                                                             \
      }                                                                 \
    else                                                                \
      result = YY_NULL;                                                 \
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

static void
_cfg_lex_new_line(CfgLexer *lexer)
{
  lexer->include_stack[lexer->include_depth].lloc.first_line++;
  lexer->include_stack[lexer->include_depth].lloc.last_line++;
  lexer->include_stack[lexer->include_depth].lloc.first_column = 1;
  lexer->include_stack[lexer->include_depth].lloc.last_column = 1;
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
word	[^ \#'"\(\)\{\}\\;\n\t,|\.@:]

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
                             while (ch != '\n' && ch != EOF)
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
(-|\+)?{digit}+\.{digit}+  { yylval->fnum = strtod(yytext, NULL); return LL_FLOAT; }
0x{xdigit}+ 		   { yylval->num = strtoll(yytext + 2, NULL, 16); return LL_NUMBER; }
0{odigit}+		   { yylval->num = strtoll(yytext + 1, NULL, 8); return LL_NUMBER; }
(-|\+)?{digit}+            { yylval->num = strtoll(yytext, NULL, 10); return LL_NUMBER; }
({word}+(\.)?)*{word}+ 	   { return cfg_lexer_lookup_keyword(yyextra, yylval, yylloc, yytext); }
\(	      		   |
\)			   |
\;			   |
\{			   |
\}			   |
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
<block>[^{ \t]+            { return LL_ERROR; }
<block>\{                  {
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


<block_content>\{          {
                             g_string_append_c(yyextra->string_buffer, yytext[0]);
                             yyextra->brace_count++;
                           }
<block_content>\}          {
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

<block_content>[^{}\"\'\n\r]+   { g_string_append(yyextra->string_buffer, yytext); }

<INITIAL><<EOF>>           { if (!cfg_lexer_start_next_include(yyextra)) yyterminate(); }

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
_cfg_lexer_force_block_state(yyscan_t yyscanner)
{
  yy_push_state(block, yyscanner);
}

/* avoid warnings of unused symbols */
gpointer __dummy[] = { yy_top_state };
