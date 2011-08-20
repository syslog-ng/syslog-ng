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

#include "cfg-lexer.h"
#include "cfg-lex.h"
#include "cfg-grammar.h"
#include "block-ref-parser.h"
#include "pragma-parser.h"
#include "messages.h"
#include "misc.h"

#include <string.h>
#include <sys/stat.h>

struct _CfgArgs
{
  GHashTable *args;
};

/*
 * A token block is a series of tokens to be injected into the tokens
 * fetched by the lexer.  It is assumed to be filled and then depleted, the
 * two operations cannot be intermixed.
 */
struct _CfgTokenBlock
{
  gint pos;
  GArray *tokens;
};

/**
 * CfgBlockGenerator:
 *
 * This class describes a block generator, e.g. a function callback
 * that returns a configuration snippet in a given context. Each
 * user-defined "block" results in a generator to be registered, but
 * theoretically this mechanism can be used to write plugins that
 * generate syslog-ng configuration on the fly, based on system
 * settings for example.
 **/
struct _CfgBlockGenerator
{
  gint context;
  gchar *name;
  CfgBlockGeneratorFunc generator;
  gpointer generator_data;
  GDestroyNotify generator_data_free;
};

/**
 * CfgLexerContext:
 *
 * This object tells the lexer in which context it is operating right
 * now. The context influences the way the lexer works, for example in
 * LL_CONTEXT_BLOCK_DEF/REF all keyword resolutions are disabled.
 *
 * A textual description is also associated with the current context
 * in order to give better error messages.
 **/
typedef struct _CfgLexerContext
{
  gint type;
  CfgLexerKeyword *keywords;
  gchar desc[0];
} CfgLexerContext;

/*
 * cfg_lexer_push_context:
 *
 * This function can be used to push a lexer context to the stack. The top
 * of the stack determines how an error is reported and can also influence
 * the lexer.
 */
void
cfg_lexer_push_context(CfgLexer *self, gint type, CfgLexerKeyword *keywords, const gchar *desc)
{
  CfgLexerContext *context;

  context = g_malloc(sizeof(CfgLexerContext) + strlen(desc) + 1);
  context->type = type ? type : cfg_lexer_get_context_type(self);
  context->keywords = keywords;
  memcpy(&context->desc, desc, strlen(desc) + 1);
  self->context_stack = g_list_prepend(self->context_stack, context);
}

/*
 * cfg_lexer_pop_context:
 *
 * Pop the topmost item off the stack.
 */
void
cfg_lexer_pop_context(CfgLexer *self)
{
  if (self->context_stack)
    {
      g_free((gchar *) self->context_stack->data);
      self->context_stack = g_list_delete_link(self->context_stack, self->context_stack);
    }
}

/*
 * cfg_lexer_get_context_type:
 *
 * Get the current context type (one of LL_CONTEXT_* values).
 */
gint
cfg_lexer_get_context_type(CfgLexer *self)
{
  GList *l;

  l = self->context_stack;
  if (l)
    return ((CfgLexerContext *) l->data)->type;
  return 0;
}

/*
 * cfg_lexer_get_context_description:
 *
 * Get the description of the current context.
 */
const gchar *
cfg_lexer_get_context_description(CfgLexer *self)
{
  GList *l;

  l = self->context_stack;
  if (l)
    return ((CfgLexerContext *) l->data)->desc;
  return "configuration";
}

gchar *
cfg_lexer_subst_args(CfgArgs *globals, CfgArgs *defs, CfgArgs *args, gchar *cptr, gsize *length)
{
  gboolean backtick = FALSE;
  gchar *p, *ref_start = cptr;
  GString *result = g_string_sized_new(32);

  p = cptr;
  while (*p)
    {
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
              const gchar *arg;

              *p = 0;
              if (args && (arg = cfg_args_get(args, ref_start)))
                ;
              else if (defs && (arg = cfg_args_get(defs, ref_start)))
                ;
              else if (globals && (arg = cfg_args_get(globals, ref_start)))
                ;
              else if ((arg = g_getenv(ref_start)))
                ;
              else
                arg = NULL;

              *p = '`';
              g_string_append(result, arg ? arg : "");
            }
        }
      else if (!backtick)
        g_string_append_c(result, *p);
      p++;
    }

  if (backtick)
    {
      g_string_free(result, TRUE);
      return NULL;
    }

  *length = result->len;
  return g_string_free(result, FALSE);
}

int
cfg_lexer_lookup_keyword(CfgLexer *self, YYSTYPE *yylval, YYLTYPE *yylloc, const char *token)
{
  GList *l;

  l = self->context_stack;
  while (l)
    {
      CfgLexerContext *context = ((CfgLexerContext *) l->data);
      CfgLexerKeyword *keywords = context->keywords;

      if (keywords)
        {
          int i, j;

          for (i = 0; keywords[i].kw_name; i++)
            {
              if (strcmp(keywords[i].kw_name, CFG_KEYWORD_STOP) == 0)
                {
                  yylval->cptr = strdup(token);
                  return LL_IDENTIFIER;
                }

              for (j = 0; token[j] && keywords[i].kw_name[j]; j++)
                {
                  if (token[j] == '-' || token[j] == '_')
                    {
                      if (keywords[i].kw_name[j] != '_')
                        break;
                    }
                  else if (token[j] != keywords[i].kw_name[j])
                    break;
                }
              if (token[j] == 0 && keywords[i].kw_name[j] == 0)
                {
                  /* match */
                  if (keywords[i].kw_req_version > configuration->version)
                    {
                      msg_warning("WARNING: Your configuration uses a newly introduced reserved word as identifier, please use a different name or enclose it in quotes",
                                  evt_tag_str("keyword", keywords[i].kw_name),
                                  evt_tag_printf("config-version", "%d.%d", configuration->version >> 8, configuration->version & 0xFF),
                                  evt_tag_printf("version", "%d.%d", (keywords[i].kw_req_version >> 8), keywords[i].kw_req_version & 0xFF),
                                  yylloc ? evt_tag_str("filename", yylloc->level->name) : NULL,
                                  yylloc ? evt_tag_printf("line", "%d:%d", yylloc->first_line, yylloc->first_column) : NULL,
                                  NULL);
                      break;
                    }
                  switch (keywords[i].kw_status)
                    {
                    case KWS_OBSOLETE:
                      msg_warning("Your configuration file uses an obsoleted keyword, please update your configuration",
                                  evt_tag_str("keyword", keywords[i].kw_name),
                                  evt_tag_str("change", keywords[i].kw_explain),
                                  NULL);
                      break;
                    default:
                      break;
                    }
                  keywords[i].kw_status = KWS_NORMAL;
                  yylval->type = LL_TOKEN;
                  yylval->token = keywords[i].kw_token;
                  return keywords[i].kw_token;
                }
            }
        }
      l = l->next;
    }
  yylval->cptr = strdup(token);
  return LL_IDENTIFIER;
}

gboolean
cfg_lexer_start_next_include(CfgLexer *self)
{
  CfgIncludeLevel *level = &self->include_stack[self->include_depth];
  gchar *filename;
  gboolean buffer_processed = FALSE;

  if (self->include_depth == 0)
    {
      return FALSE;
    }

  if (level->yybuf)
    {
      msg_debug("Finishing include",
                evt_tag_str((level->include_type == CFGI_FILE ? "filename" : "content"), level->name),
                evt_tag_int("depth", self->include_depth),
                NULL);
      buffer_processed = TRUE;
    }

  /* reset the include state, should also handle initial invocations, in which case everything is NULL */
  if (level->yybuf)
    _cfg_lexer__delete_buffer(level->yybuf, self->state);

  if (level->include_type == CFGI_FILE)
    {
      if (level->file.include_file)
        {
          fclose(level->file.include_file);
        }
    }

  if ((level->include_type == CFGI_BUFFER && buffer_processed) ||
      (level->include_type == CFGI_FILE && !level->file.files))
    {
      /* we finished with an include statement that included a series of
       * files (e.g.  directory include). */
      g_free(level->name);

      if (level->include_type == CFGI_BUFFER)
        g_free(level->buffer.content);

      memset(level, 0, sizeof(*level));

      self->include_depth--;
      _cfg_lexer__switch_to_buffer(self->include_stack[self->include_depth].yybuf, self->state);

      return TRUE;
    }

  /* now populate "level" with the new include information */
  if (level->include_type == CFGI_BUFFER)
    {
      level->yybuf = _cfg_lexer__scan_buffer(level->buffer.content, level->buffer.content_length, self->state);
    }
  else if (level->include_type == CFGI_FILE)
    {
      FILE *include_file;

      filename = (gchar *) level->file.files->data;
      level->file.files = g_slist_delete_link(level->file.files, level->file.files);

      include_file = fopen(filename, "r");
      if (!include_file)
        {
          msg_error("Error opening include file",
                    evt_tag_str("filename", filename),
                    evt_tag_int("depth", self->include_depth),
                    NULL);
          g_free(filename);
          return FALSE;
        }
      msg_debug("Starting to read include file",
                evt_tag_str("filename", filename),
                evt_tag_int("depth", self->include_depth),
                NULL);
      g_free(level->name);
      level->name = filename;

      level->file.include_file = include_file;
      level->yybuf = _cfg_lexer__create_buffer(level->file.include_file, YY_BUF_SIZE, self->state);
    }
  else
    {
      g_assert_not_reached();
    }

  level->lloc.first_line = level->lloc.last_line = 1;
  level->lloc.first_column = level->lloc.last_column = 1;
  level->lloc.level = level;

  _cfg_lexer__switch_to_buffer(level->yybuf, self->state);
  return TRUE;
}

gboolean
cfg_lexer_include_file(CfgLexer *self, const gchar *filename_)
{
  struct stat st;
  CfgIncludeLevel *level;
  gchar *filename;

  if (self->include_depth >= MAX_INCLUDE_DEPTH - 1)
    {
      msg_error("Include file depth is too deep, increase MAX_INCLUDE_DEPTH and recompile",
                evt_tag_str("filename", filename_),
                evt_tag_int("depth", self->include_depth),
                NULL);
      return FALSE;
    }

  filename = find_file_in_path(cfg_args_get(self->globals, "include-path"), filename_, G_FILE_TEST_EXISTS);
  if (!filename || stat(filename, &st) < 0)
    {
      msg_error("Include file/directory not found",
                evt_tag_str("filename", filename_),
                evt_tag_str("include-path", cfg_args_get(self->globals, "include-path")),
                evt_tag_errno("error", errno),
                NULL);
      return FALSE;
    }
  self->include_depth++;
  level = &self->include_stack[self->include_depth];
  level->include_type = CFGI_FILE;
  if (S_ISDIR(st.st_mode))
    {
      GDir *dir;
      GError *error = NULL;
      const gchar *entry;

      dir = g_dir_open(filename, 0, &error);
      if (!dir)
        {
          msg_error("Error opening directory for reading",
                evt_tag_str("filename", filename),
                evt_tag_str("error", error->message),
                NULL);
          goto drop_level;
        }
      while ((entry = g_dir_read_name(dir)))
        {
          const gchar *p;

          for (p = entry; *p; p++)
            {
              if (!((*p >= 'a' && *p <= 'z') ||
                   (*p >= 'A' && *p <= 'Z') ||
                   (*p >= '0' && *p <= '9') ||
                   (*p == '_') || (*p == '-') || (*p == '.')))
                {
                  msg_debug("Skipping include file, does not match pattern [\\-_a-zA-Z0-9]+",
                            evt_tag_str("filename", entry),
                            NULL);
                  p = NULL;
                  break;
                }
            }
          if (p)
            {
              gchar *full_filename = g_build_filename(filename, entry, NULL);
              if (stat(full_filename, &st) < 0 || S_ISDIR(st.st_mode))
                {
                  msg_debug("Skipping include file as it is a directory",
                            evt_tag_str("filename", entry),
                            NULL);
                  g_free(full_filename);
                  continue;
                }
              level->file.files = g_slist_insert_sorted(level->file.files, full_filename, (GCompareFunc) strcmp);
              msg_debug("Adding include file",
                        evt_tag_str("filename", entry),
                        NULL);
            }
        }
      g_dir_close(dir);
      if (!level->file.files)
        {
          /* no include files in the specified directory */
          msg_debug("No files in this include directory",
                    evt_tag_str("dir", filename),
                    NULL);
          self->include_depth--;
          g_free(filename);
          return TRUE;
        }
    }
  else
    {
      g_assert(level->file.files == NULL);
      level->file.files = g_slist_prepend(level->file.files, g_strdup(filename));
    }
  g_free(filename);
  return cfg_lexer_start_next_include(self);
 drop_level:
  g_free(filename);
  g_slist_foreach(level->file.files, (GFunc) g_free, NULL);
  g_slist_free(level->file.files);
  level->file.files = NULL;
  return FALSE;
}

gboolean
cfg_lexer_include_buffer(CfgLexer *self, const gchar *name, gchar *buffer, gsize length)
{
  CfgIncludeLevel *level;

  /* lex requires two NUL characters at the end of the input */
  buffer = g_realloc(buffer, length + 2);
  buffer[length] = 0;
  buffer[length + 1] = 0;
  length += 2;

  if (self->include_depth >= MAX_INCLUDE_DEPTH - 1)
    {
      msg_error("Include file depth is too deep, increase MAX_INCLUDE_DEPTH and recompile",
                evt_tag_str("buffer", name),
                evt_tag_int("depth", self->include_depth),
                NULL);
      return FALSE;
    }

  self->include_depth++;
  level = &self->include_stack[self->include_depth];

  level->include_type = CFGI_BUFFER;
  level->buffer.content = buffer;
  level->buffer.content_length = length;
  level->name = g_strdup(name);

  return cfg_lexer_start_next_include(self);
}

void
cfg_lexer_inject_token_block(CfgLexer *self, CfgTokenBlock *block)
{
  self->token_blocks = g_list_append(self->token_blocks, block);
}

static CfgBlockGenerator *
cfg_lexer_find_generator(CfgLexer *self, gint context, const gchar *name)
{
  GList *l;

  for (l = self->generators; l; l = l->next)
    {
      CfgBlockGenerator *gen = (CfgBlockGenerator *) l->data;

      if ((gen->context == 0 || gen->context == context) && strcmp(gen->name, name) == 0)
        {
          return gen;
        }
    }
  return NULL;
}

void
cfg_lexer_register_block_generator(CfgLexer *self, gint context, const gchar *name, CfgBlockGeneratorFunc generator, gpointer generator_data, GDestroyNotify generator_data_free)
{
  CfgBlockGenerator *gen;

  if (cfg_lexer_find_generator(self, context, name))
    {
      msg_debug("Attempted to register the same generator multiple times, ignoring",
                evt_tag_str("context", cfg_lexer_lookup_context_name_by_type(context)),
                evt_tag_str("name", name),
                NULL);
      generator_data_free(generator_data);
      return;
    }

  gen = g_new0(CfgBlockGenerator, 1);

  gen->context = context;
  gen->name = g_strdup(name);
  gen->generator = generator;
  gen->generator_data = generator_data;
  gen->generator_data_free = generator_data_free;

  self->generators = g_list_append(self->generators, gen);
}

static gboolean
cfg_lexer_generate_block(CfgLexer *self, gint context, const gchar *name, CfgBlockGenerator *gen, CfgArgs *args)
{
  return gen->generator(self, context, name, args, gen->generator_data);
}

void
cfg_lexer_unput_token(CfgLexer *self, YYSTYPE *yylval)
{
  CfgTokenBlock *block;

  block = cfg_token_block_new();
  cfg_token_block_add_token(block, yylval);
  cfg_lexer_inject_token_block(self, block);
}

int
cfg_lexer_lex(CfgLexer *self, YYSTYPE *yylval, YYLTYPE *yylloc)
{
  CfgBlockGenerator *gen;
  CfgTokenBlock *block;
  YYSTYPE *token;
  gint tok;
  gboolean injected;

 relex:

  injected = FALSE;
  while (self->token_blocks)
    {
      block = self->token_blocks->data;
      token = cfg_token_block_get_token(block);

      if (token)
        {
          *yylval = *token;
          *yylloc = self->include_stack[self->include_depth].lloc;
          tok = token->type;
          if (token->type == LL_TOKEN)
            {
              tok = token->token;
              injected = TRUE;
            }
          else if (token->type == LL_IDENTIFIER || token->type == LL_STRING)
            {
              yylval->cptr = strdup(token->cptr);
            }

          goto exit;
        }
      else
        {
          self->token_blocks = g_list_delete_link(self->token_blocks, self->token_blocks);
          cfg_token_block_free(block);
        }
    }

  if (cfg_lexer_get_context_type(self) == LL_CONTEXT_BLOCK_CONTENT)
    _cfg_lexer_force_block_state(self->state);

  yylval->type = 0;

  g_string_truncate(self->token_text, 0);
  g_string_truncate(self->token_pretext, 0);

  tok = _cfg_lexer_lex(yylval, yylloc, self->state);
  if (yylval->type == 0)
    yylval->type = tok;

  if (self->preprocess_output)
    fprintf(self->preprocess_output, "%s", self->token_pretext->str);
 exit:
  if (tok == LL_PRAGMA)
    {
      gpointer dummy;

      if (self->preprocess_output)
        fprintf(self->preprocess_output, "@");
      if (!cfg_parser_parse(&pragma_parser, self, &dummy, NULL))
        {
          return LL_ERROR;
        }
      goto relex;
    }
  else if (tok == KW_INCLUDE && cfg_lexer_get_context_type(self) != LL_CONTEXT_PRAGMA)
    {
      gchar *include_file;

      self->preprocess_suppress_tokens++;
      tok = cfg_lexer_lex(self, yylval, yylloc);
      if (tok != LL_STRING && tok != LL_IDENTIFIER)
        {
          self->preprocess_suppress_tokens--;
          return LL_ERROR;
        }

      include_file = g_strdup(yylval->cptr);
      free(yylval->cptr);

      tok = cfg_lexer_lex(self, yylval, yylloc);
      if (tok != ';')
        {
          self->preprocess_suppress_tokens--;
          return LL_ERROR;
        }

      if (!cfg_lexer_include_file(self, include_file))
        {
          self->preprocess_suppress_tokens--;
          return LL_ERROR;
        }
      self->preprocess_suppress_tokens--;
      goto relex;
    }
  else if (tok == LL_IDENTIFIER && (gen = cfg_lexer_find_generator(self, cfg_lexer_get_context_type(self), yylval->cptr)))
    {
      CfgArgs *args;

      self->preprocess_suppress_tokens++;
      if (cfg_parser_parse(&block_ref_parser, self, (gpointer *) &args, NULL))
        {
          gboolean success;

          self->preprocess_suppress_tokens--;
          success = cfg_lexer_generate_block(self, cfg_lexer_get_context_type(self), yylval->cptr, gen, args);
          cfg_args_free(args);
          if (success)
            {
              goto relex;
            }
        }
      else
        {
          self->preprocess_suppress_tokens--;
        }
      return LL_ERROR;
    }
  else if (configuration->version == 0 && configuration->parsed_version != 0)
    {
      cfg_set_version(configuration, configuration->parsed_version);
    }
  else if (configuration->version == 0 && configuration->parsed_version == 0 && cfg_lexer_get_context_type(self) != LL_CONTEXT_PRAGMA)
    {
      /* no version selected yet, and we have a non-pragma token, this
      * means that the configuration is meant for syslog-ng 2.1 */

      msg_warning("WARNING: Configuration file has no version number, assuming syslog-ng 2.1 format. Please add @version: maj.min to the beginning of the file",
                  NULL);
      cfg_set_version(configuration, 0x0201);
    }

  if (!injected)
    {
      if (self->preprocess_suppress_tokens == 0)
        {
          if (self->preprocess_output)
            fprintf(self->preprocess_output, "%s", self->token_text->str);
        }
    }
  return tok;
}

static void
cfg_lexer_init(CfgLexer *self)
{
  self->globals = cfg_args_new();
  CfgIncludeLevel *level;

  _cfg_lexer_lex_init_extra(self, &self->state);
  self->string_buffer = g_string_sized_new(32);
  self->token_text = g_string_sized_new(32);
  self->token_pretext = g_string_sized_new(32);

  level = &self->include_stack[0];
  level->lloc.first_line = level->lloc.last_line = 1;
  level->lloc.first_column = level->lloc.last_column = 1;
  level->lloc.level = level;
}

CfgLexer *
cfg_lexer_new(FILE *file, const gchar *filename, const gchar *preprocess_into)
{
  CfgLexer *self;
  CfgIncludeLevel *level;

  self = g_new0(CfgLexer, 1);
  cfg_lexer_init(self);

  if (preprocess_into)
    {
      self->preprocess_output = fopen(preprocess_into, "w");
    }

  level = &self->include_stack[0];
  level->include_type = CFGI_FILE;
  level->name = g_strdup(filename);
  level->yybuf = _cfg_lexer__create_buffer(file, YY_BUF_SIZE, self->state);
  _cfg_lexer__switch_to_buffer(level->yybuf, self->state);

  return self;
}

CfgLexer *
cfg_lexer_new_buffer(const gchar *buffer, gsize length)
{
  CfgLexer *self;
  CfgIncludeLevel *level;

  self = g_new0(CfgLexer, 1);
  cfg_lexer_init(self);

  level = &self->include_stack[0];
  level->include_type = CFGI_BUFFER;
  level->buffer.content = g_malloc(length + 2);
  memcpy(level->buffer.content, buffer, length);
  level->buffer.content[length] = 0;
  level->buffer.content[length + 1] = 0;
  level->buffer.content_length = length + 2;
  level->name = g_strdup("<string>");
  level->yybuf = _cfg_lexer__scan_buffer(level->buffer.content, level->buffer.content_length, self->state);
  _cfg_lexer__switch_to_buffer(level->yybuf, self->state);

  return self;
}

void
cfg_lexer_free(CfgLexer *self)
{
  gint i;

  for (i = 0; i <= self->include_depth; i++)
    {
      CfgIncludeLevel *level = &self->include_stack[i];

      g_free(level->name);
      if (level->yybuf)
        _cfg_lexer__delete_buffer(level->yybuf, self->state);

      if (level->include_type == CFGI_FILE)
        {
          if (level->file.include_file)
            fclose(level->file.include_file);
          g_slist_foreach(level->file.files, (GFunc) g_free, NULL);
          g_slist_free(level->file.files);
        }
      else if (level->include_type == CFGI_BUFFER)
        {
          g_free(level->buffer.content);
        }
    }
  self->include_depth = 0;
  _cfg_lexer_lex_destroy(self->state);
  g_string_free(self->string_buffer, TRUE);
  if (self->token_text)
    g_string_free(self->token_text, TRUE);
  if (self->token_pretext)
    g_string_free(self->token_pretext, TRUE);
  if (self->preprocess_output)
    fclose(self->preprocess_output);

  while (self->context_stack)
    cfg_lexer_pop_context(self);
  while (self->generators)
    {
      CfgBlockGenerator *gen = self->generators->data;

      if (gen->generator_data && gen->generator_data_free)
        gen->generator_data_free(gen->generator_data);
      g_free(gen->name);
      g_free(gen);
      self->generators = g_list_remove_link(self->generators, self->generators);
    }
  cfg_args_free(self->globals);
  g_list_foreach(self->token_blocks, (GFunc) cfg_token_block_free, NULL);
  g_list_free(self->token_blocks);
  g_free(self);
}

static const gchar *lexer_contexts[] =
{
  [LL_CONTEXT_ROOT] = "root",
  [LL_CONTEXT_DESTINATION] = "destination",
  [LL_CONTEXT_SOURCE] = "source",
  [LL_CONTEXT_PARSER] = "parser",
  [LL_CONTEXT_REWRITE] = "rewrite",
  [LL_CONTEXT_FILTER] = "filter",
  [LL_CONTEXT_LOG] = "log",
  [LL_CONTEXT_BLOCK_DEF] = "block-def",
  [LL_CONTEXT_BLOCK_REF] = "block-ref",
  [LL_CONTEXT_BLOCK_CONTENT] = "block-content",
  [LL_CONTEXT_PRAGMA] = "pragma",
  [LL_CONTEXT_FORMAT] = "format",
  [LL_CONTEXT_TEMPLATE_FUNC] = "template-func",
  [LL_CONTEXT_INNER_DEST] = "inner-dest",
  [LL_CONTEXT_INNER_SRC] = "inner-src",
};

gint
cfg_lexer_lookup_context_type_by_name(const gchar *name)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS(lexer_contexts); i++)
    {
      if (lexer_contexts[i] && strcmp(lexer_contexts[i], name) == 0)
        return i;
    }
  return 0;
}

const gchar *
cfg_lexer_lookup_context_name_by_type(gint type)
{
  g_assert(type < G_N_ELEMENTS(lexer_contexts));
  return lexer_contexts[type];
}

/* token block args */

static void
cfg_args_validate_callback(gpointer k, gpointer v, gpointer user_data)
{
  CfgArgs *defs = ((gpointer *) user_data)[0];
  gchar **bad_key = (gchar **) &((gpointer *) user_data)[1];
  gchar **bad_value = (gchar **) &((gpointer *) user_data)[2];

  if ((*bad_key == NULL) && (!defs || cfg_args_get(defs, k) == NULL))
    {
      *bad_key = k;
      *bad_value = v;
    }
}

gboolean
cfg_args_validate(CfgArgs *self, CfgArgs *defs, const gchar *context)
{
  gpointer validate_params[] = { defs, NULL, NULL };

  g_hash_table_foreach(self->args, cfg_args_validate_callback, validate_params);

  if (validate_params[1])
    {
      msg_error("Unknown argument",
                evt_tag_str("context", context),
                evt_tag_str("arg", validate_params[1]),
                evt_tag_str("value", validate_params[2]),
                NULL);
      return FALSE;
    }
  return TRUE;
}

void
cfg_args_set(CfgArgs *self, const gchar *name, const gchar *value)
{
  g_hash_table_insert(self->args, g_strdup(name), g_strdup(value));
}

const gchar *
cfg_args_get(CfgArgs *self, const gchar *name)
{
  return g_hash_table_lookup(self->args, name);
}

CfgArgs *
cfg_args_new(void)
{
  CfgArgs *self = g_new0(CfgArgs, 1);

  self->args = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  return self;
}

void
cfg_args_free(CfgArgs *self)
{
  g_hash_table_destroy(self->args);
  g_free(self);
}

/* token blocks */

void
cfg_token_block_add_token(CfgTokenBlock *self, YYSTYPE *token)
{
  g_assert(self->pos == 0);
  g_array_append_val(self->tokens, *token);
}

YYSTYPE *
cfg_token_block_get_token(CfgTokenBlock *self)
{
  if (self->pos < self->tokens->len)
    {
      YYSTYPE *result;

      result = &g_array_index(self->tokens, YYSTYPE, self->pos);
      self->pos++;
      return result;
    }
  return NULL;
}

CfgTokenBlock *
cfg_token_block_new()
{
  CfgTokenBlock *self = g_new0(CfgTokenBlock, 1);

  self->tokens = g_array_new(FALSE, TRUE, sizeof(YYSTYPE));
  return self;
}

void
cfg_token_block_free(CfgTokenBlock *self)
{
  gint i;

  for (i = 0; i < self->tokens->len; i++)
    {
      YYSTYPE *token = &g_array_index(self->tokens, YYSTYPE, i);

      if (token->type == LL_STRING || token->type == LL_IDENTIFIER)
        g_free(token->cptr);

    }
  g_array_free(self->tokens, TRUE);
  g_free(self);
}

/* user defined blocks */

/*
 * This class encapsulates a configuration block that the user defined
 * via the configuration file. It behaves like a macro, e.g. when
 * referenced the content of the block is expanded.
 *
 * Each block is identified by its name and the context (source,
 * destination, etc.) where it is meant to be used.
 *
 * A block has a set of name-value pairs to allow expansion to be
 * parameterized. The set of allowed NV pairs is defined at block
 * definition time
 */
struct _CfgBlock
{
  gchar *content;
  CfgArgs *arg_defs;
};

/*
 * cfg_block_generate:
 *
 * This is a CfgBlockGeneratorFunc, which takes a CfgBlock defined by
 * the user, substitutes backtick values and generates input tokens
 * for the lexer.
 */
gboolean
cfg_block_generate(CfgLexer *lexer, gint context, const gchar *name, CfgArgs *args, gpointer user_data)
{
  CfgBlock *block = (CfgBlock *) user_data;
  gchar *value;
  gchar buf[256];
  gsize length;

  g_snprintf(buf, sizeof(buf), "%s block %s", cfg_lexer_lookup_context_name_by_type(context), name);
  if (!cfg_args_validate(args, block->arg_defs, buf))
    {
      return FALSE;
    }

  value = cfg_lexer_subst_args(lexer->globals, block->arg_defs, args, block->content, &length);

  if (!value)
    {
      msg_warning("Syntax error while resolving backtick references in block, missing closing '`' character",
                  evt_tag_str("context", cfg_lexer_lookup_context_name_by_type(context)),
                  evt_tag_str("block", name),
                  NULL);
      return FALSE;
    }

  return cfg_lexer_include_buffer(lexer, buf, value, length);
}

/*
 * Construct a user defined block.
 */
CfgBlock *
cfg_block_new(const gchar *content, CfgArgs *arg_defs)
{
  CfgBlock *self = g_new0(CfgBlock, 1);

  self->content = g_strdup(content);
  self->arg_defs = arg_defs;
  return self;
}

/*
 * Free a user defined block.
 */
void
cfg_block_free(CfgBlock *self)
{
  g_free(self->content);
  cfg_args_free(self->arg_defs);
  g_free(self);
}
