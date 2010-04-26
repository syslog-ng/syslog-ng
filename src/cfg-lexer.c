#include "cfg-lexer.h"
#include "cfg-lex.h"
#include "cfg-grammar.h"
#include "messages.h"
#include "cfg.h"

#include <string.h>
#include <sys/stat.h>

struct _CfgBlockGeneratorArgs
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
      self->context_stack = g_list_remove_link(self->context_stack, self->context_stack);
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

/**
 * cfg_lexer_append_string:
 *
 * This function appends a string to the internal pattern buffer. It
 * is used by the generated flex code to store intermediate results
 * (like the unescaped version of the string being parsed).
 **/
void
cfg_lexer_append_string(CfgLexer *self, int length, char *s)
{
  g_string_append_len(self->pattern_buffer, s, length);
}

/**
 * cfg_lexer_append_char:
 *
 * This function appends a character to the internal pattern buffer. It
 * is used by the generated flex code to store intermediate results
 * (like the unescaped version of the string being parsed).
 **/
void
cfg_lexer_append_char(CfgLexer *self, char c)
{
  g_string_append_c(self->pattern_buffer, c);
}

int
cfg_lexer_lookup_keyword_in_table(CfgLexer *self, YYSTYPE *yylval, YYLTYPE *lloc, const char *token, CfgLexerKeyword *keywords)
{
  int i, j;

  for (i = 0; keywords[i].kw_name; i++)
    {
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
          if (keywords[i].kw_req_version > configuration->version)
            {
              msg_warning("WARNING: Your configuration uses a newly introduced reserved word as identifier, please use a different name or enclose it in quotes",
                          evt_tag_str("keyword", keywords[i].kw_name),
                          evt_tag_printf("config-version", "%d.%d", configuration->version >> 8, configuration->version & 0xFF),
                          evt_tag_printf("version", "%d.%d", (keywords[i].kw_req_version >> 8), keywords[i].kw_req_version & 0xFF),
                          lloc ? evt_tag_str("filename", lloc->level->name) : NULL,
                          lloc ? evt_tag_printf("line", "%d:%d", lloc->first_line, lloc->first_column) : NULL,
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
  yylval->cptr = strdup(token);
  return LL_IDENTIFIER;
}

int
cfg_lexer_lookup_keyword(CfgLexer *self, YYSTYPE *yylval, YYLTYPE *yylloc, const char *token)
{
  gint res = LL_IDENTIFIER;
  GList *l;

  if (cfg_lexer_get_context_type(self) == LL_CONTEXT_BLOCK_DEF || cfg_lexer_get_context_type(self) == LL_CONTEXT_BLOCK_REF)
    {
      /* no reserved words inside a block definition/reference */
      yylval->cptr = strdup(token);
      return LL_IDENTIFIER;
    }

  l = self->context_stack;
  while (l)
    {
      CfgLexerContext *context = ((CfgLexerContext *) l->data);

      if (context->keywords)
        {
          res = cfg_lexer_lookup_keyword_in_table(self, yylval, yylloc, token, context->keywords);
          if (res != LL_IDENTIFIER)
            return res;
        }
      l = l->next;
    }

  return res;
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
cfg_lexer_include_file(CfgLexer *self, const gchar *filename)
{
  struct stat st;
  gchar buf[1024];
  CfgIncludeLevel *level;

  if (self->include_depth >= MAX_INCLUDE_DEPTH - 1)
    {
      msg_error("Include file depth is too deep, increase MAX_INCLUDE_DEPTH and recompile",
                evt_tag_str("filename", filename),
                evt_tag_int("depth", self->include_depth),
                NULL);
      return FALSE;
    }

  if (filename[0] != '/')
    {
      g_snprintf(buf, sizeof(buf), "%s/%s", PATH_SYSCONFDIR, filename);
      filename = buf;
    }

  if (stat(filename, &st) < 0)
    {
      msg_error("Include file/directory not found",
                evt_tag_str("filename", filename),
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
          return TRUE;
        }
    }
  else
    {
      g_assert(level->file.files == NULL);
      level->file.files = g_slist_prepend(level->file.files, g_strdup(filename));
    }
  return cfg_lexer_start_next_include(self);
 drop_level:
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

void
cfg_lexer_register_block_generator(CfgLexer *self, gint context, const gchar *name, CfgBlockGeneratorFunc generator, gpointer generator_data, GDestroyNotify generator_data_free)
{
  CfgBlockGenerator *gen = g_new0(CfgBlockGenerator, 1);

  gen->context = context;
  gen->name = g_strdup(name);
  gen->generator = generator;
  gen->generator_data = generator_data;
  gen->generator_data_free = generator_data_free;

  self->generators = g_list_append(self->generators, gen);
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

static gboolean
cfg_lexer_generate_block(CfgLexer *self, gint context, const gchar *name, CfgBlockGenerator *gen, CfgBlockGeneratorArgs *args)
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

 relex:

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
            }
          else if (token->type == LL_IDENTIFIER || token->type == LL_STRING)
            {
              yylval->cptr = strdup(token->cptr);
            }

          goto exit;
        }
      else
        {
          self->token_blocks = g_list_remove_link(self->token_blocks, self->token_blocks);
          cfg_token_block_free(block);
        }
    }
  tok = _cfg_lexer_lex(yylval, yylloc, self->state);
  yylval->type = tok;
  if (tok == KW_INCLUDE)
    {
      gchar *include_file;

      tok = cfg_lexer_lex(self, yylval, yylloc);
      if (tok != LL_STRING && tok != LL_IDENTIFIER)
        {
          return LL_ERROR;
        }

      include_file = g_strdup(yylval->cptr);
      free(yylval->cptr);

      tok = cfg_lexer_lex(self, yylval, yylloc);
      if (tok != ';')
        {
          return LL_ERROR;
        }

      if (!cfg_lexer_include_file(self, include_file))
        {
          return LL_ERROR;
        }

      goto relex;
    }
  return tok;
}

CfgLexer *
cfg_lexer_new(FILE *file, const gchar *filename, gint init_line_num)
{
  CfgLexer *self;
  CfgIncludeLevel *level;

  self = g_new0(CfgLexer, 1);

  _cfg_lexer_lex_init_extra(self, &self->state);
  _cfg_lexer_restart(NULL, self->state);
  self->pattern_buffer = g_string_sized_new(32);

  level = &self->include_stack[0];
  level->lloc.first_line = level->lloc.last_line = init_line_num;
  level->lloc.first_column = level->lloc.last_column = 1;
  level->lloc.level = level;
  level->name = g_strdup(filename);
  level->yybuf = _cfg_lexer__create_buffer(file, YY_BUF_SIZE, self->state);
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
  g_string_free(self->pattern_buffer, TRUE);
  while (self->context_stack)
    cfg_lexer_pop_context(self);
  while (self->generators)
    {
      CfgBlockGenerator *gen = self->generators->data;

      gen->generator_data_free(gen->generator_data);
      g_free(gen->name);
      g_free(gen);
      self->generators = g_list_remove_link(self->generators, self->generators);
    }
  g_free(self);
}

/* token block args */

void
cfg_block_generator_args_set_arg(CfgBlockGeneratorArgs *self, const gchar *name, const gchar *value)
{
  g_hash_table_insert(self->args, g_strdup(name), g_strdup(value));
}

const gchar *
cfg_block_generator_args_get_arg(CfgBlockGeneratorArgs *self, const gchar *name)
{
  return g_hash_table_lookup(self->args, name);
}

CfgBlockGeneratorArgs *
cfg_block_generator_args_new(void)
{
  CfgBlockGeneratorArgs *self = g_new0(CfgBlockGeneratorArgs, 1);

  self->args = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  return self;
}

void
cfg_block_generator_args_free(CfgBlockGeneratorArgs *self)
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
