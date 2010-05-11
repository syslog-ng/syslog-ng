/***************************************************************************
 *
 * Copyright (c) 1999 Balázs Scheidler
 * Copyright (c) 1999-2007 BalaBit IT Ltd.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Inspired by nsyslog, originally written by Darren Reed.
 *
 * $Id: cfg-lex.l,v 1.8 2003/01/22 11:11:18 bazsi Exp $
 *
 ***************************************************************************/
%{

#include "syslog-ng.h"
#include "cfg-lexer.h"
#include "cfg-grammar.h"
#include "messages.h"


#include <string.h>
#include <strings.h>


#define YY_USER_ACTION do { \
  yyextra->include_stack[yyextra->include_depth].lloc.first_column = yyextra->include_stack[yyextra->include_depth].lloc.last_column;         \
  yyextra->include_stack[yyextra->include_depth].lloc.last_column = yyextra->include_stack[yyextra->include_depth].lloc.last_column + yyleng; \
  *yylloc = yyextra->include_stack[yyextra->include_depth].lloc;                                                              \
} while(0);

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
word	[^ \#'"\(\)\{\}\\;\n\t,|\.]

%x string
%x qstring
%x block
%x block_content
%x block_string
%x block_qstring
%%

\#                         {
                             char ch;

                             ch = input(yyscanner);
                             while (ch != '\n' && ch != EOF)
                               {
                                 ch = input(yyscanner);
                               }
                             if (ch == '\n')
                               {
                                 _cfg_lex_new_line(yyextra);
                               }
                           }
<*>\r?\n                   {
                             *yylloc = yyextra->include_stack[yyextra->include_depth].lloc;
                             _cfg_lex_new_line(yyextra);
                             cfg_lexer_append_string(yyextra, strlen(yytext), yytext);
                           }
{white}+		   ;
\.\.                       { return LL_DOTDOT; }
(-|\+)?{digit}+\.{digit}+  { yylval->fnum = strtod(yytext, NULL); return LL_FLOAT; }
0x{xdigit}+ 		   { yylval->num = strtoll(yytext + 2, NULL, 16); return LL_NUMBER; }
0{odigit}+		   { yylval->num = strtoll(yytext + 1, NULL, 8); return LL_NUMBER; }
(-|\+)?{digit}+            { yylval->num = strtoll(yytext, NULL, 10); return LL_NUMBER; }
({word}+(\.)?)*{word}+ 	   { return cfg_lexer_lookup_keyword(yyextra, yylval, yylloc, yytext); }
\(	      		   { return '('; }
\)			   { return ')'; }
\;			   { return ';'; }
\{			   { return '{'; }
\}			   { return '}'; }
\|			   { return '|'; }
\,			   ;

\"                         {
				g_string_truncate(yyextra->pattern_buffer, 0);
				yy_push_state(string, yyscanner);
			   }
\'			   {
				g_string_truncate(yyextra->pattern_buffer, 0);
				yy_push_state(qstring, yyscanner);
			   }
<string>\\a		   { cfg_lexer_append_char(yyextra, 7); }
<string>\\n	   	   { cfg_lexer_append_char(yyextra, 10); }
<string>\\r		   { cfg_lexer_append_char(yyextra, 13); }
<string>\\t		   { cfg_lexer_append_char(yyextra, 9); }
<string>\\v		   { cfg_lexer_append_char(yyextra, 11); }
<string>\\x{xdigit}+       { cfg_lexer_append_char(yyextra, strtol(yytext+2, NULL, 16)); }
<string>\\{odigit}+        { cfg_lexer_append_char(yyextra, strtol(yytext+2, NULL, 8)); }
<string>\\[^anrtv]	   { cfg_lexer_append_string(yyextra, 1, yytext + 1); }
<string>\"		   { 
				yy_pop_state(yyscanner);
				yylval->cptr = strdup(yyextra->pattern_buffer->str);
				return LL_STRING; 
		           }
<string>[^"\\]+		   { cfg_lexer_append_string(yyextra, strlen(yytext), yytext); }
<qstring>[^']+		   { cfg_lexer_append_string(yyextra, strlen(yytext), yytext); }
<qstring>\'		   { 
				yy_pop_state(yyscanner);
				yylval->cptr = strdup(yyextra->pattern_buffer->str);
				return LL_STRING;
			   }


     /* rules to parse a block as a LL_BLOCK token */
<block>{white}+            ;
<block>[^{ \t]+            { return LL_ERROR; }
<block>\{                  {
				g_string_truncate(yyextra->pattern_buffer, 0);
                                yyextra->brace_count = 1;
                                yy_push_state(block_content, yyscanner);
                           }

<block_content>\"          {
                                cfg_lexer_append_char(yyextra, yytext[0]);
                                yy_push_state(block_string, yyscanner);
                           }

<block_content>\'          {
                                cfg_lexer_append_char(yyextra, yytext[0]);
                                yy_push_state(block_qstring, yyscanner);
                           }

<block_string>[^"]+        {    cfg_lexer_append_string(yyextra, strlen(yytext), yytext); }
<block_string>\\\"         {    cfg_lexer_append_string(yyextra, strlen(yytext), yytext); }
<block_string>\"           {
                                cfg_lexer_append_char(yyextra, yytext[0]);
                                yy_pop_state(yyscanner);
                           }

<block_qstring>\'          {
                                cfg_lexer_append_char(yyextra, yytext[0]);
                                yy_pop_state(yyscanner);
                           }
<block_qstring>[^']+       {
                                cfg_lexer_append_string(yyextra, strlen(yytext), yytext);
                           }


<block_content>\{          {
                                cfg_lexer_append_char(yyextra, yytext[0]);
                                yyextra->brace_count++;
                           }
<block_content>\}          {
                                yyextra->brace_count--;
                                if (yyextra->brace_count == 0)
                                  {
                                    yy_pop_state(yyscanner);
                                    yy_pop_state(yyscanner);
                                    yylval->cptr = strdup(yyextra->pattern_buffer->str);
                                    return LL_BLOCK;
                                  }
                                else
                                  {
                                    cfg_lexer_append_char(yyextra, yytext[0]);
                                  }

                           }

<block_content>[^}\"\'\n\r]+   { cfg_lexer_append_string(yyextra, strlen(yytext), yytext); }

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
gpointer __dummy[] = { input, yy_top_state };
