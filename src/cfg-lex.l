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



%}

%option bison-bridge bison-locations reentrant
%option prefix="_cfg_lexer_"
%option header-file="lex.yy.h"
%option outfile="lex.yy.c"
%option extra-type="CfgLexer *"
%option noyywrap

white	[ \t]
digit	[0-9]
alpha		[a-zA-Z]
alphanum	[a-zA-Z0-9]
word	[^ \#'"\(\)\{\}\\;\n\t,|\.]

%x string
%x qstring
%%

\#.*$                      ;
\r?\n                      {
                             yyextra->include_stack[yyextra->include_depth].lloc.first_line++;
                             yyextra->include_stack[yyextra->include_depth].lloc.last_line++;
                             yyextra->include_stack[yyextra->include_depth].lloc.first_column = 1;
                             yyextra->include_stack[yyextra->include_depth].lloc.last_column = 1;
                             *yylloc = yyextra->include_stack[yyextra->include_depth].lloc;
                           }
{white}+		   ;
\.\.                       { return LL_DOTDOT; }
(-|\+)?{digit}+\.{digit}+  { yylval->fnum = strtod(yytext, NULL); return LL_FLOAT; }
0x{digit}+ 		   { yylval->num = strtoll(yytext, NULL, 16); return LL_NUMBER; }
0{digit}+		   { yylval->num = strtoll(yytext, NULL, 8); return LL_NUMBER; }
(-|\+)?{digit}+            { yylval->num = strtoll(yytext, NULL, 10); return LL_NUMBER; }
({word}+(\.)?)*{word}+ 	   { return cfg_lexer_lookup_keyword(yyextra, yylval, yytext); }
\(	      		   { return '('; }
\)			   { return ')'; }
\;			   { return ';'; }
\{			   { return '{'; }
\}			   { return '}'; }
\|			   { return '|'; }
\,			   ;

\"                         {
				g_string_truncate(yyextra->pattern_buffer, 0);
				BEGIN(string);
			   }
\'			   {
				g_string_truncate(yyextra->pattern_buffer, 0);
				BEGIN(qstring);
			   }
<string>\\a		   { cfg_lexer_append_char(yyextra, 7); }
<string>\\n	   	   { cfg_lexer_append_char(yyextra, 10); }
<string>\\r		   { cfg_lexer_append_char(yyextra, 13); }
<string>\\t		   { cfg_lexer_append_char(yyextra, 9); }
<string>\\v		   { cfg_lexer_append_char(yyextra, 11); }
<string>\\[^anrtv]	   { cfg_lexer_append_string(yyextra, 1, yytext + 1); }
<string>\"		   { 
				BEGIN(INITIAL);
				/* yy_pop_state();*/
				yylval->cptr = strdup(yyextra->pattern_buffer->str);
				return LL_STRING; 
		           }
<string>[^"\\]+		   { cfg_lexer_append_string(yyextra, strlen(yytext), yytext); }
<qstring>[^']+		   { cfg_lexer_append_string(yyextra, strlen(yytext), yytext); }
<qstring>\'		   { 
				BEGIN(INITIAL);
				yylval->cptr = strdup(yyextra->pattern_buffer->str);
				return LL_STRING;
			   }


<INITIAL><<EOF>>           { if (!cfg_lexer_start_next_include(yyextra, FALSE)) yyterminate(); }

%%


void
cfg_lexer_unput_string(CfgLexer *self, const char *str)
{
  int len = strlen(str);
  int i;
  
  for (i = len - 1; i >= 0; i--)
    {
      yyunput(str[i], _cfg_lexer_get_text(self->state), self->state);
    }
}
