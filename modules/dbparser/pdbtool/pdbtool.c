/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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

#include "syslog-ng.h"
#include "messages.h"
#include "template/templates.h"
#include "patterndb.h"
#include "dbparser.h"
#include "radix.h"
#include "stats/stats-registry.h"
#include "plugin.h"
#include "filter/filter-expr-parser.h"
#include "patternize.h"
#include "pdb-example.h"
#include "pdb-program.h"
#include "pdb-load.h"
#include "pdb-file.h"
#include "apphook.h"
#include "transport/transport-file.h"
#include "logproto/logproto-text-server.h"
#include "reloc.h"
#include "pathutils.h"
#include "resolved-configurable-paths.h"
#include "crypto.h"
#include "compat/openssl_support.h"
#include "scratch-buffers.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <locale.h>

#define BOOL(x) ((x) ? "TRUE" : "FALSE")

static const gchar *full_colors[] =
{
  "\33[01;34m", /* blue */
  "\33[0;33m", /* brown */
  "\33[01;32m", /* green */
  "\33[01;31m"  /* red */
};

static const gchar *empty_colors[] =
{
  "", "", "", ""
};

#define COLOR_TRAILING_JUNK 0
#define COLOR_PARSER 1
#define COLOR_LITERAL 2
#define COLOR_PARTIAL 3

static const gchar *no_color = "\33[01;0m";
static const gchar **colors = empty_colors;


static const gchar *patterndb_file = PATH_PATTERNDB_FILE;
static gboolean color_out = FALSE;

typedef struct _PdbToolMergeState
{
  GString *merged;
  gint version;
  gboolean in_rule;
} PdbToolMergeState;

void
pdbtool_merge_start_element(GMarkupParseContext *context, const gchar *element_name, const gchar **attribute_names,
                            const gchar **attribute_values, gpointer user_data, GError **error)
{
  PdbToolMergeState *state = (PdbToolMergeState *) user_data;
  gchar *buff;
  gint i;

  if (g_str_equal(element_name, "patterndb"))
    {
      for (i = 0; attribute_names[i]; i++)
        {
          if (g_str_equal(attribute_names[i], "version"))
            state->version = strtol(attribute_values[i], NULL, 10);
        }
      return;
    }
  else if (g_str_equal(element_name, "rule"))
    state->in_rule = TRUE;

  if (g_str_equal(element_name, "program"))
    g_string_append(state->merged, "<ruleset");
  else if (state->version == 1 && state->in_rule && g_str_equal(element_name, "pattern"))
    g_string_append_printf(state->merged, "<patterns>\n<%s", element_name);
  else if (state->version == 1 && state->in_rule && g_str_equal(element_name, "url"))
    g_string_append_printf(state->merged, "<urls>\n<%s", element_name);
  else
    g_string_append_printf(state->merged, "<%s", element_name);

  for (i = 0; attribute_names[i]; i++)
    {
      buff = g_markup_printf_escaped(" %s='%s'", attribute_names[i], attribute_values[i]);
      g_string_append_printf(state->merged, "%s", buff);
      g_free(buff);
    }

  g_string_append(state->merged, ">");
}


void
pdbtool_merge_end_element(GMarkupParseContext *context, const gchar *element_name, gpointer user_data, GError **error)
{
  PdbToolMergeState *state = (PdbToolMergeState *) user_data;

  if (g_str_equal(element_name, "patterndb"))
    return;
  else if (g_str_equal(element_name, "rule"))
    state->in_rule = FALSE;

  if (g_str_equal(element_name, "program"))
    g_string_append(state->merged, "</ruleset>");
  else if (state->version == 1 && state->in_rule && g_str_equal(element_name, "pattern"))
    g_string_append_printf(state->merged, "</%s>\n</patterns>", element_name);
  else if (state->version == 1 && state->in_rule && g_str_equal(element_name, "url"))
    g_string_append_printf(state->merged, "</%s>\n</urls>", element_name);
  else
    g_string_append_printf(state->merged, "</%s>", element_name);
}

void
pdbtool_merge_text(GMarkupParseContext *context, const gchar *text, gsize text_len, gpointer user_data, GError **error)
{
  PdbToolMergeState *state = (PdbToolMergeState *) user_data;
  gchar *buff = g_markup_printf_escaped("%s", text);

  g_string_append(state->merged, buff);

  g_free(buff);
}

GMarkupParser pdbtool_merge_parser =
{
  .start_element = pdbtool_merge_start_element,
  .end_element = pdbtool_merge_end_element,
  .text = pdbtool_merge_text,
  .passthrough = NULL,
  .error = NULL
};

static gboolean
pdbtool_merge_file(const gchar *filename, GString *merged)
{
  GMarkupParseContext *parse_ctx = NULL;
  PdbToolMergeState state;
  GError *error = NULL;
  gboolean success = TRUE;
  gchar *buff = NULL;
  gsize buff_len;

  if (!g_file_get_contents(filename, &buff, &buff_len, &error))
    {
      fprintf(stderr, "Error reading pattern database file; filename='%s', error='%s'\n",
              filename, error ? error->message : "Unknown error");
      success = FALSE;
      goto error;
    }

  state.version = 0;
  state.merged = merged;
  state.in_rule = FALSE;

  parse_ctx = g_markup_parse_context_new(&pdbtool_merge_parser, 0, &state, NULL);
  if (!g_markup_parse_context_parse(parse_ctx, buff, buff_len, &error))
    {
      fprintf(stderr, "Error parsing pattern database file; filename='%s', error='%s'\n",
              filename, error ? error->message : "Unknown error");
      success = FALSE;
      goto error;
    }

  if (!g_markup_parse_context_end_parse(parse_ctx, &error))
    {
      fprintf(stderr, "Error parsing pattern database file; filename='%s', error='%s'\n",
              filename, error ? error->message : "Unknown error");
      success = FALSE;
      goto error;
    }

error:
  if (buff)
    g_free(buff);

  if (parse_ctx)
    g_markup_parse_context_free(parse_ctx);

  return success;
}

static gchar *merge_dir = NULL;
static gchar *merge_glob = NULL;
static gboolean merge_recursive = FALSE;

static gboolean
pdbtool_merge_dir(const gchar *dir, gboolean recursive, GString *merged)
{
  GDir *pdb_dir;
  gboolean ok = TRUE;
  GError *error = NULL;
  const gchar *filename;

  if ((pdb_dir = g_dir_open(dir, 0, &error)) == NULL)
    {
      fprintf(stderr, "Error opening directory %s, error='%s'\n", merge_dir, error ? error->message : "Unknown error");
      g_clear_error(&error);
      return FALSE;
    }

  while ((filename = g_dir_read_name(pdb_dir)) != NULL && ok)
    {
      gchar *full_name = g_build_filename(dir, filename, NULL);

      if (recursive && is_file_directory(full_name))
        {
          ok = pdbtool_merge_dir(full_name, recursive, merged);
        }
      else if (is_file_regular(full_name) && (!merge_glob || g_pattern_match_simple(merge_glob, filename)))
        {
          ok = pdbtool_merge_file(full_name, merged);
        }
      g_free(full_name);
    }
  g_dir_close(pdb_dir);
  return TRUE;
}

static gint
pdbtool_merge(int argc, char *argv[])
{
  GDate date;
  GError *error = NULL;
  GString *merged = NULL;
  gchar *buff;
  gboolean ok;

  if (!merge_dir)
    {
      fprintf(stderr, "No directory is specified to merge from\n");
      return 1;
    }

  if (!patterndb_file)
    {
      fprintf(stderr, "No patterndb file is specified to merge to\n");
      return 1;
    }

  merged = g_string_sized_new(4096);
  g_date_clear(&date, 1);
  g_date_set_time_t(&date, time (NULL));

  buff = g_markup_printf_escaped("<?xml version='1.0' encoding='UTF-8'?>\n<patterndb version='4' pub_date='%04d-%02d-%02d'>",
                                 g_date_get_year(&date), g_date_get_month(&date), g_date_get_day(&date));
  g_string_append(merged, buff);
  g_free(buff);

  ok = pdbtool_merge_dir(merge_dir, merge_recursive, merged);

  g_string_append(merged, "</patterndb>\n");

  if (ok && !g_file_set_contents(patterndb_file, merged->str, merged->len, &error))
    {
      fprintf(stderr, "Error storing patterndb; filename='%s', errror='%s'\n", patterndb_file,
              error ? error->message : "Unknown error");
      ok = FALSE;
    }

  g_string_free(merged, TRUE);

  return ok ? 0 : 1;
}

static GOptionEntry merge_options[] =
{
  {
    "pdb",       'p', 0, G_OPTION_ARG_STRING, &patterndb_file,
    "Name of the patterndb output file", "<patterndb_file>"
  },
  {
    "recursive", 'r', 0, G_OPTION_ARG_NONE, &merge_recursive,
    "Recurse into subdirectories", NULL
  },
  {
    "glob",      'G', 0, G_OPTION_ARG_STRING, &merge_glob,
    "Filenames to consider for merging", "<pattern>"
  },
  {
    "directory", 'D', 0, G_OPTION_ARG_STRING, &merge_dir,
    "Directory from merge pattern databases", "<directory>"
  },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

static gchar *match_program = NULL;
static gchar *match_message = NULL;
static gchar *match_file = NULL;
static gchar *template_string = NULL;
static gchar *filter_string = NULL;
static gboolean debug_pattern = FALSE;
static gboolean debug_pattern_parse = FALSE;

gboolean
pdbtool_match_values(NVHandle handle, const gchar *name, const gchar *value, gssize length, gpointer user_data)
{
  gint *ret = user_data;

  printf("%s=%.*s\n", name, (gint) length, value);
  if (g_str_equal(name, ".classifier.rule_id") && ret)
    *ret = 0;
  if (ret && (g_str_equal(name, ".classifier.class") && g_str_equal(value, "unknown")))
    *ret = 1;
  return FALSE;
}

static void
pdbtool_pdb_emit(LogMessage *msg, gboolean synthetic, gpointer user_data)
{
  gpointer *args = (gpointer *) user_data;
  FilterExprNode *filter = (FilterExprNode *) args[0];
  LogTemplate *template = (LogTemplate *) args[1];
  gint *ret = (gint *) args[2];
  GString *output = (GString *) args[3];
  gboolean matched;

  matched = !filter || filter_expr_eval(filter, msg);
  if (matched)
    {
      if (G_UNLIKELY(!template))
        {
          if (debug_pattern && !debug_pattern_parse)
            printf("\nValues:\n");

          nv_table_foreach(msg->payload, logmsg_registry, pdbtool_match_values, ret);
          g_string_truncate(output, 0);
          log_msg_print_tags(msg, output);
          printf("TAGS=%s\n", output->str);
          printf("\n");
        }
      else
        {
          log_template_format(template, msg, NULL, LTZ_LOCAL, 0, NULL, output);
          printf("%s", output->str);
        }
    }
}

static gint
pdbtool_match(int argc, char *argv[])
{
  PatternDB *patterndb;
  GArray *dbg_list = NULL;
  RDebugInfo *dbg_info;
  gint i = 0, pos = 0;
  gint ret = 0;
  const gchar *name = NULL;
  gssize name_len = 0;
  MsgFormatOptions parse_options;
  gboolean eof = FALSE;
  const guchar *buf = NULL;
  gsize buflen;
  LogMessage *msg = NULL;
  LogTemplate *template = NULL;
  GString *output = NULL;
  FilterExprNode *filter = NULL;
  LogProtoServer *proto = NULL;
  LogProtoServerOptions proto_options;
  gboolean may_read = TRUE;
  gpointer args[4];

  memset(&parse_options, 0, sizeof(parse_options));

  if (!match_message && !match_file)
    {
      fprintf(stderr, "Either -M or -f is required to specify which message to match\n");
      return ret;
    }

  if (template_string)
    {
      gchar *t;

      t = g_strcompress(template_string);
      template = log_template_new(configuration, NULL);
      log_template_compile(template, t, NULL);
      g_free(t);

    }
  output = g_string_sized_new(512);

  if (filter_string)
    {
      CfgLexer *lexer;

      lexer = cfg_lexer_new_buffer(configuration, filter_string, strlen(filter_string));
      if (!cfg_run_parser(configuration, lexer, &filter_expr_parser, (gpointer *) &filter, NULL))
        {
          fprintf(stderr, "Error parsing filter expression\n");
          return 1;
        }
    }


  msg_format_options_defaults(&parse_options);
  /* the syslog protocol parser automatically falls back to RFC3164 format */
  parse_options.flags |= LP_SYSLOG_PROTOCOL | LP_EXPECT_HOSTNAME;
  msg_format_options_init(&parse_options, configuration);
  log_proto_server_options_defaults(&proto_options);
  proto_options.max_msg_size = 65536;
  log_proto_server_options_init(&proto_options, configuration);

  patterndb = pattern_db_new();
  if (!pattern_db_reload_ruleset(patterndb, configuration, patterndb_file))
    {
      goto error;
    }

  msg = log_msg_new_empty();
  if (!match_file)
    {
      log_msg_set_value(msg, LM_V_MESSAGE, match_message, strlen(match_message));
      if (match_program && match_program[0])
        log_msg_set_value(msg, LM_V_PROGRAM, match_program, strlen(match_program));
    }
  else
    {
      LogTransport *transport;
      gint fd;

      if (strcmp(match_file, "-") == 0)
        {
          fd = 0;
        }
      else
        {

          fd = open(match_file, O_RDONLY);
          if (fd < 0)
            {
              fprintf(stderr, "Error opening file to be processed: %s\n", g_strerror(errno));
              goto error;
            }
        }
      transport = log_transport_file_new(fd);
      proto = log_proto_text_server_new(transport, &proto_options);
      eof = log_proto_server_fetch(proto, &buf, &buflen, &may_read, NULL, NULL) != LPS_SUCCESS;
    }

  if (!debug_pattern)
    {
      args[0] = filter;
      args[1] = template;
      args[2] = &ret;
      args[3] = output;
      pattern_db_set_emit_func(patterndb, pdbtool_pdb_emit, args);
    }
  else
    {
      dbg_list = g_array_new(FALSE, FALSE, sizeof(RDebugInfo));
    }
  while (!eof && (buf || match_message))
    {
      invalidate_cached_time();
      if (G_LIKELY(proto))
        {
          log_msg_unref(msg);
          msg = log_msg_new_empty();
          parse_options.format_handler->parse(&parse_options, buf, buflen, msg);
        }

      if (G_UNLIKELY(debug_pattern))
        {
          const gchar *msg_string;

          pattern_db_debug_ruleset(patterndb, msg, dbg_list);

          msg_string = log_msg_get_value(msg, LM_V_MESSAGE, NULL);
          pos = 0;
          if (!debug_pattern_parse)
            {
              printf("Pattern matching part:\n");
              for (i = 0; i < dbg_list->len; i++)
                {
                  dbg_info = &g_array_index(dbg_list, RDebugInfo, i);

                  pos += dbg_info->i;

                  if (dbg_info->pnode)
                    {
                      name = nv_registry_get_handle_name(logmsg_registry, dbg_info->pnode->handle, &name_len);

                      printf("%s@%s:%s=%.*s@%s",
                             colors[COLOR_PARSER],
                             r_parser_type_name(dbg_info->pnode->type),
                             name_len ? name : "",
                             name_len ? dbg_info->match_len : 0,
                             name_len ? msg_string + dbg_info->match_off : "",
                             no_color
                            );
                    }
                  else if (dbg_info->i == dbg_info->node->keylen)
                    {
                      printf("%s%s%s", colors[COLOR_LITERAL], dbg_info->node->key, no_color);
                    }
                  else
                    {
                      printf("%s%.*s%s", colors[COLOR_PARTIAL], dbg_info->node->key ? dbg_info->i : 0,
                             dbg_info->node->key ? (gchar *) dbg_info->node->key : "", no_color);
                    }

                }
              printf("%s%s%s", colors[COLOR_TRAILING_JUNK], msg_string + pos, no_color);

              printf("\nMatching part:\n");
            }

          if (debug_pattern_parse)
            printf("PDBTOOL_HEADER=i:len:key;keylen:match_off;match_len:parser_type:parser_name\n");

          pos = 0;
          for (i = 0; i < dbg_list->len; i++)
            {
              /* NOTE: if i is smaller than node->keylen than we did not match the full node
               * so matching failed on this node, we need to highlight it somehow
               */
              dbg_info = &g_array_index(dbg_list, RDebugInfo, i);
              if (debug_pattern_parse)
                {
                  if (dbg_info->pnode)
                    name = nv_registry_get_handle_name(logmsg_registry, dbg_info->pnode->handle, &name_len);

                  printf("PDBTOOL_DEBUG=%d:%d:%d:%d:%d:%s:%s\n",
                         i, dbg_info->i, dbg_info->node->keylen, dbg_info->match_off, dbg_info->match_len,
                         dbg_info->pnode ? r_parser_type_name(dbg_info->pnode->type) : "",
                         dbg_info->pnode && name_len ? name : ""
                        );
                }
              else
                {
                  if (dbg_info->i == dbg_info->node->keylen || dbg_info->pnode)
                    printf("%s%.*s%s", dbg_info->pnode ? colors[COLOR_PARSER] : colors[COLOR_LITERAL], dbg_info->i, msg_string + pos,
                           no_color);
                  else
                    printf("%s%.*s%s", colors[COLOR_PARTIAL], dbg_info->i, msg_string + pos, no_color);
                  pos += dbg_info->i;
                }
            }
          g_array_set_size(dbg_list, 0);
          dbg_info = NULL;
          {
            gpointer nulls[] = { NULL, NULL, &ret, output };
            pdbtool_pdb_emit(msg, FALSE, nulls);
          }
        }
      else
        {
          pattern_db_process(patterndb, msg);
        }

      if (G_LIKELY(proto))
        {
          buf = NULL;
          eof = log_proto_server_fetch(proto, &buf, &buflen, &may_read, NULL, NULL) != LPS_SUCCESS;
        }
      else
        {
          eof = TRUE;
        }
    }
  pattern_db_expire_state(patterndb);
error:
  if (proto)
    log_proto_server_free(proto);
  if (template)
    log_template_unref(template);
  if (output)
    g_string_free(output, TRUE);
  pattern_db_free(patterndb);
  if (msg)
    log_msg_unref(msg);
  msg_format_options_destroy(&parse_options);

  return ret;
}

static GOptionEntry match_options[] =
{
  {
    "pdb",       'p', 0, G_OPTION_ARG_STRING, &patterndb_file,
    "Name of the patterndb file", "<patterndb_file>"
  },
  {
    "program", 'P', 0, G_OPTION_ARG_STRING, &match_program,
    "Program name to match as $PROGRAM", "<program>"
  },
  {
    "message", 'M', 0, G_OPTION_ARG_STRING, &match_message,
    "Message to match as $MSG", "<message>"
  },
  {
    "debug-pattern", 'D', 0, G_OPTION_ARG_NONE, &debug_pattern,
    "Print debuging information on pattern matching", NULL
  },
  {
    "debug-csv", 'C', 0, G_OPTION_ARG_NONE, &debug_pattern_parse,
    "Output debuging information in parseable format", NULL
  },
  {
    "color-out", 'c', 0, G_OPTION_ARG_NONE, &color_out,
    "Color terminal output", NULL
  },
  {
    "template", 'T', 0, G_OPTION_ARG_STRING, &template_string,
    "Template string to be used to format the output", "template"
  },
  {
    "file", 'f', 0, G_OPTION_ARG_STRING, &match_file,
    "Read the messages from the file specified", NULL
  },
  {
    "filter", 'F', 0, G_OPTION_ARG_STRING, &filter_string,
    "Only print messages matching the specified syslog-ng filter", "expr"
  },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

static gboolean test_validate = FALSE;
static gchar *test_ruleid = NULL;

static gboolean
pdbtool_test_value(LogMessage *msg, const gchar *name, const gchar *test_value)
{
  const gchar *value;
  gssize value_len;
  gboolean ret = TRUE;

  value = log_msg_get_value_by_name(msg, name, &value_len);
  if (!(value && strncmp(value, test_value, value_len) == 0 && value_len == strlen(test_value)))
    {
      if (value)
        printf(" Wrong match name='%s', value='%.*s', expected='%s'\n", name, (gint) value_len, value, test_value);
      else
        printf(" No value to match name='%s', expected='%s'\n", name, test_value);

      ret = FALSE;
    }
  else if (verbose_flag)
    printf(" Match name='%s', value='%.*s', expected='%s'\n", name, (gint) value_len, value, test_value);

  return ret;
}

static void
pdbtool_test_find_conflicts(PatternDB *patterndb, LogMessage *msg)
{
  const gchar *program;
  const gchar *message;
  RNode *node;

  program = log_msg_get_value(msg, LM_V_PROGRAM, NULL);
  message = log_msg_get_value(msg, LM_V_MESSAGE, NULL);

  node = r_find_node(pattern_db_get_ruleset(patterndb)->programs, (gchar *) program, strlen(program), NULL);
  if (node)
    {
      PDBProgram *program_rules = (PDBProgram *) node->value;
      gchar **matching_ids;
      gint matching_ids_len;

      matching_ids = r_find_all_applicable_nodes(program_rules->rules, (guint8 *) message, strlen(message),
                                                 (RNodeGetValueFunc) pdb_rule_get_name);
      matching_ids_len = g_strv_length(matching_ids);

      if (matching_ids_len > 1)
        {
          gint i;

          printf(" Rule conflict! Multiple rules match this message, list of IDs follow\n");
          for (i = 0; matching_ids[i]; i++)
            {
              printf("  %s\n", matching_ids[i]);
            }
        }


      g_strfreev(matching_ids);
    }
}

static gint
pdbtool_test(int argc, char *argv[])
{
  PatternDB *patterndb;
  PDBExample *example;
  LogMessage *msg;
  GList *examples = NULL;
  gint i, arg_pos;
  gboolean failed_to_load = FALSE;
  gboolean failed_to_match = FALSE;
  gboolean failed_to_validate = FALSE;
  gboolean failed_to_find_id = TRUE;

  for (arg_pos = 1; arg_pos < argc; arg_pos++)
    {
      if (access(argv[arg_pos], R_OK) == -1)
        {
          fprintf(stderr, "%s: Unable to access the patterndb file\n", argv[arg_pos]);
          failed_to_validate = TRUE;
          continue;
        }

      if (test_validate)
        {
          GError *error = NULL;

          if (!pdb_file_validate(argv[arg_pos], &error))
            {
              fprintf(stderr, "%s: error validating pdb file: %s\n", argv[arg_pos], error->message);
              g_clear_error(&error);
              failed_to_validate = TRUE;
              continue;
            }
        }

      patterndb = pattern_db_new();
      if (!pdb_rule_set_load(pattern_db_get_ruleset(patterndb), configuration, argv[arg_pos], &examples))
        {
          failed_to_load = TRUE;
          continue;
        }

      while (examples)
        {
          example = examples->data;

          if (example->message && example->program)
            {
              if (test_ruleid)
                {
                  if (strcmp(example->rule->rule_id, test_ruleid) != 0)
                    {
                      pdb_example_free(example);
                      examples = g_list_delete_link(examples, examples);
                      continue;
                    }
                  else
                    failed_to_find_id = FALSE;
                }

              msg = log_msg_new_empty();
              log_msg_set_value(msg, LM_V_MESSAGE, example->message, strlen(example->message));
              if (example->program && example->program[0])
                log_msg_set_value(msg, LM_V_PROGRAM, example->program, strlen(example->program));

              printf("Testing message: program='%s' message='%s'\n", example->program, example->message);

              pdbtool_test_find_conflicts(patterndb, msg);
              pattern_db_process(patterndb, msg);

              if (!pdbtool_test_value(msg, ".classifier.rule_id", example->rule->rule_id))
                {
                  failed_to_match = TRUE;
                  if (debug_pattern)
                    {
                      match_message = example->message;
                      match_program = example->program;
                      patterndb_file = argv[arg_pos];
                      pdbtool_match(0, NULL);
                    }
                }

              for (i = 0; example->values && i < example->values->len; i++)
                {
                  gchar **nv = g_ptr_array_index(example->values, i);
                  if (!pdbtool_test_value(msg, nv[0], nv[1]))
                    failed_to_match = TRUE;
                }

              log_msg_unref(msg);
            }
          else if (!example->program && example->message)
            {
              printf("NOT Testing message as program is unset: message='%s'\n", example->message);
            }
          else if (example->program && !example->message)
            {
              printf("NOT Testing message as message is unset: program='%s'\n", example->program);
            }

          pdb_example_free(example);
          examples = g_list_delete_link(examples, examples);
        }

      pattern_db_free(patterndb);
    }
  if (failed_to_load || failed_to_validate)
    return 1;
  if (failed_to_match)
    return 2;
  if (failed_to_find_id && test_ruleid)
    {
      printf("Could not find the specified ID, or the defined rule doesn't have an example message.\n");
      return 3;
    }
  return 0;
}

static GOptionEntry test_options[] =
{
  {
    "validate", 0, 0, G_OPTION_ARG_NONE, &test_validate,
    "Validate the pdb file against the xsd (requires xmllint from libxml2)", NULL
  },
  {
    "rule-id", 'r', 0, G_OPTION_ARG_STRING, &test_ruleid,
    "Rule ID of the patterndb rule to be tested against its example", NULL
  },
  {
    "debug", 'D', 0, G_OPTION_ARG_NONE, &debug_pattern,
    "Print debuging information on non-matching patterns", NULL
  },
  {
    "color-out", 'c', 0, G_OPTION_ARG_NONE, &color_out,
    "Color terminal output", NULL
  },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

static gboolean dump_program_tree = FALSE;

void
pdbtool_walk_tree(RNode *root, gint level, gboolean program)
{
  gint i;

  for (i = 0; i < level; i++)
         printf(" ");

  if (root->parser)
    printf("@%s:%s@ ", r_parser_type_name(root->parser->type), log_msg_get_value_name(root->parser->handle, NULL));
  printf("'%s' ", root->key ? (gchar *) root->key : "");

  if (root->value)
    {
      if (!program)
        printf("rule_id='%s'", ((PDBRule *) root->value)->rule_id);
      else
        printf("RULES");
    }

  printf("\n");

  for (i = 0; i < root->num_children; i++)
         pdbtool_walk_tree(root->children[i], level + 1, program);

  for (i = 0; i < root->num_pchildren; i++)
         pdbtool_walk_tree(root->pchildren[i], level + 1, program);
}

static gint
pdbtool_dump(int argc, char *argv[])
{
  PatternDB *patterndb;

  patterndb = pattern_db_new();
  if (!pattern_db_reload_ruleset(patterndb, configuration, patterndb_file))
    return 1;

  if (dump_program_tree)
    pdbtool_walk_tree(pattern_db_get_ruleset(patterndb)->programs, 0, TRUE);
  else if (match_program)
    {
      RNode *ruleset = r_find_node(pattern_db_get_ruleset(patterndb)->programs, match_program, strlen(match_program), NULL);
      if (ruleset && ruleset->value)
        {
          RNode *root = ((PDBProgram *) ruleset->value)->rules;
          if (root)
            pdbtool_walk_tree(root, 0, FALSE);
        }
    }
  else
    {
      fprintf(stderr, "Neither --program-tree nor --program was specified, doing nothing\n");
    }

  pattern_db_free(patterndb);

  return 0;
}

static GOptionEntry dump_options[] =
{
  {
    "pdb",       'p', 0, G_OPTION_ARG_STRING, &patterndb_file,
    "Name of the patterndb file", "<patterndb_file>"
  },
  {
    "program", 'P', 0, G_OPTION_ARG_STRING, &match_program,
    "Program name ($PROGRAM) to dump", "<program>"
  },
  {
    "program-tree", 'T', 0, G_OPTION_ARG_NONE, &dump_program_tree,
    "Dump the program ($PROGRAM) tree", NULL
  },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

static gboolean dictionary_tags = FALSE;

static void
pdbtool_dictionary_walk(RNode *root, const gchar *progname)
{
  gint i;

  if (root->parser && root->parser->handle && !dictionary_tags)
    printf("%s\n", log_msg_get_value_name(root->parser->handle, NULL));

  if (root->value)
    {
      if (!progname)
        {
          pdbtool_dictionary_walk(((PDBProgram *)root->value)->rules, (gchar *)root->key);
        }
      else
        {
          PDBRule *rule = (PDBRule *)root->value;
          LogTemplate *template;
          guint tag_id;

          if (!dictionary_tags && rule->msg.values)
            {
              for (i = 0; i < rule->msg.values->len; i++)
                {
                  template = (LogTemplate *)g_ptr_array_index(rule->msg.values, i);
                  printf("%s\n", template->name);
                }
            }

          if (dictionary_tags && rule->msg.tags)
            {
              for (i = 0; i < rule->msg.tags->len; i++)
                {
                  tag_id = g_array_index(rule->msg.tags, guint, i);
                  printf("%s\n", log_tags_get_by_id(tag_id));
                }
            }
        }
    }

  for (i = 0; i < root->num_children; i++)
         pdbtool_dictionary_walk(root->children[i], progname);

  for (i = 0; i < root->num_pchildren; i++)
         pdbtool_dictionary_walk(root->pchildren[i], progname);
}

static GOptionEntry dictionary_options[] =
{
  {
    "pdb",       'p', 0, G_OPTION_ARG_STRING, &patterndb_file,
    "Name of the patterndb file", "<patterndb_file>"
  },
  {
    "program", 'P', 0, G_OPTION_ARG_STRING, &match_program,
    "Program name ($PROGRAM) to dump", "<program>"
  },
  {
    "dump-tags", 'T', 0, G_OPTION_ARG_NONE, &dictionary_tags,
    "Dump the tags in the rules instead of the value names", NULL
  },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

static gint
pdbtool_dictionary(int argc, char *argv[])
{
  PDBRuleSet rule_set;

  memset(&rule_set, 0x0, sizeof(PDBRuleSet));

  if (!pdb_rule_set_load(&rule_set, configuration, patterndb_file, NULL))
    return 1;

  if (match_program)
    {
      RNode *ruleset = r_find_node(rule_set.programs, match_program, strlen(match_program), NULL);
      if (ruleset && ruleset->value)
        pdbtool_dictionary_walk(((PDBProgram *)ruleset->value)->rules, (gchar *)ruleset->key);
    }
  else
    pdbtool_dictionary_walk(rule_set.programs, NULL);

  return 0;
}

static gboolean
pdbtool_load_module(const gchar *option_name, const gchar *value, gpointer data, GError **error)
{
  return cfg_load_module(configuration, value);
}

static gchar *input_logfile = NULL;
static gboolean no_parse = FALSE;
static gdouble support_treshold = 4.0;
static gboolean iterate_outliers = FALSE;
static gboolean named_parsers = FALSE;
static gint num_of_samples = 1;
static const gchar *delimiters = " :&~?![]=,;()'\"";

static gint
pdbtool_patternize(int argc, char *argv[])
{
  Patternizer *ptz;
  GHashTable *clusters;
  guint iterate = PTZ_ITERATE_NONE;
  gint i;
  GError *error = NULL;
  GString *delimcheck = g_string_new(" "); /* delims should always include a space */

  if (iterate_outliers)
    iterate = PTZ_ITERATE_OUTLIERS;

  /* make sure that every character is unique in the delimiter list */
  for (i = 0; delimiters[i]; i++)
    {
      if (strchr(delimcheck->str, delimiters[i]) == NULL)
        g_string_append_c(delimcheck, delimiters[i]);
    }
  delimiters = g_strdup(delimcheck->str);
  g_string_free(delimcheck, TRUE);

  if (!(ptz = ptz_new(support_treshold, PTZ_ALGO_SLCT, iterate, num_of_samples, delimiters)))
    {
      return 1;
    }

  argv[0] = input_logfile;
  for (i = 0; i < argc; i++)
    {
      if (argv[i] && !ptz_load_file(ptz, argv[i], no_parse, &error))
        {
          fprintf(stderr, "Error adding log file as patternize input: %s\n", error->message);
          g_clear_error(&error);
          goto exit;
        }
    }

  clusters = ptz_find_clusters(ptz);
  ptz_print_patterndb(clusters, delimiters, named_parsers);
  g_hash_table_destroy(clusters);

exit:
  ptz_free(ptz);

  return 0;
}

static GOptionEntry patternize_options[] =
{
  {
    "file",             'f', 0, G_OPTION_ARG_STRING, &input_logfile,
    "Logfile to create pattern database from, use '-' for stdin", "<path>"
  },
  {
    "no-parse", 'p', 0, G_OPTION_ARG_NONE, &no_parse,
    "Do try to parse the input file, consider the whole lines as the message part of the log", NULL
  },
  {
    "support",          'S', 0, G_OPTION_ARG_DOUBLE, &support_treshold,
    "Percentage of lines that have to support a pattern (default: 4.0)", "<support>"
  },
  {
    "iterate-outliers", 'o', 0, G_OPTION_ARG_NONE, &iterate_outliers,
    "Recursively iterate on the log lines that do not make it into a cluster in the previous step", NULL
  },
  {
    "named-parsers",    'n', 0, G_OPTION_ARG_NONE, &named_parsers,
    "Give the parsers a name in the patterns, eg.: .dict.string1, .dict.string2... (default: no)", NULL
  },
  {
    "delimiters",       'd', 0, G_OPTION_ARG_STRING, &delimiters,
    "Set of characters based on which the log messages are tokenized, defaults to :&~?![]=,;()'\"", "<delimiters>"
  },
  {
    "samples",           0, 0, G_OPTION_ARG_INT, &num_of_samples,
    "Number of example lines to add for the patterns (default: 1)", "<samples>"
  },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};


const gchar *
pdbtool_mode(int *argc, char **argv[])
{
  gint i;
  const gchar *mode;

  for (i = 1; i < (*argc); i++)
    {
      if ((*argv)[i][0] != '-')
        {
          mode = (*argv)[i];
          memmove(&(*argv)[i], &(*argv)[i+1], ((*argc) - i) * sizeof(gchar *));
          (*argc)--;
          return mode;
        }
    }
  return NULL;
}

static GOptionEntry pdbtool_options[] =
{
  {
    "debug",     'd', 0, G_OPTION_ARG_NONE, &debug_flag,
    "Enable debug/diagnostic messages on stderr", NULL
  },
  {
    "verbose",   'v', 0, G_OPTION_ARG_NONE, &verbose_flag,
    "Enable verbose messages on stderr", NULL
  },
  {
    "module", 0, 0, G_OPTION_ARG_CALLBACK, pdbtool_load_module,
    "Load the module specified as parameter", "<module>"
  },
  {
    "module-path",         0,         0, G_OPTION_ARG_STRING, &resolvedConfigurablePaths.initial_module_path,
    "Set the list of colon separated directories to search for modules, default=" SYSLOG_NG_MODULE_PATH, "<path>"
  },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

static struct
{
  const gchar *mode;
  const GOptionEntry *options;
  const gchar *description;
  gint (*main)(gint argc, gchar *argv[]);
} modes[] =
{
  { "match", match_options, "Match a message against the pattern database", pdbtool_match },
  { "dump", dump_options, "Dump pattern datebase tree", pdbtool_dump },
  { "merge", merge_options, "Merge pattern databases", pdbtool_merge },
  { "test", test_options, "Test pattern databases", pdbtool_test },
  { "patternize", patternize_options, "Create a pattern database from logs", pdbtool_patternize },
  { "dictionary", dictionary_options, "Dump pattern dictionary", pdbtool_dictionary },
  { NULL, NULL },
};

void
usage(void)
{
  gint mode;

  fprintf(stderr, "Syntax: pdbtool <command> [options]\nPossible commands are:\n");
  for (mode = 0; modes[mode].mode; mode++)
    {
      fprintf(stderr, "    %-12s %s\n", modes[mode].mode, modes[mode].description);
    }
  exit(1);
}

int
main(int argc, char *argv[])
{
  const gchar *mode_string;
  GOptionContext *ctx;
  gint mode, ret = 0;
  GError *error = NULL;

  mode_string = pdbtool_mode(&argc, &argv);
  if (!mode_string)
    {
      usage();
    }

  ctx = NULL;
  for (mode = 0; modes[mode].mode; mode++)
    {
      if (strcmp(modes[mode].mode, mode_string) == 0)
        {
          ctx = g_option_context_new(mode_string);
          g_option_context_set_summary(ctx, modes[mode].description);
          g_option_context_add_main_entries(ctx, modes[mode].options, NULL);
          g_option_context_add_main_entries(ctx, pdbtool_options, NULL);
          msg_add_option_group(ctx);
          break;
        }
    }
  if (!ctx)
    {
      fprintf(stderr, "Unknown command\n");
      usage();
    }

  setlocale(LC_ALL, "");

  msg_init(TRUE);
  resolved_configurable_paths_init(&resolvedConfigurablePaths);
  stats_init();
  scratch_buffers_global_init();
  scratch_buffers_allocator_init();
  log_msg_global_init();
  log_template_global_init();
  log_tags_global_init();
  pattern_db_global_init();
  crypto_init();

  configuration = cfg_new_snippet();

  if (!g_option_context_parse(ctx, &argc, &argv, &error))
    {
      fprintf(stderr, "Error parsing command line arguments: %s\n", error ? error->message : "Invalid arguments");
      g_clear_error(&error);
      g_option_context_free(ctx);
      return 1;
    }
  g_option_context_free(ctx);

  cfg_load_module(configuration, "syslogformat");
  cfg_load_module(configuration, "basicfuncs");

  if (color_out)
    colors = full_colors;

  ret = modes[mode].main(argc, argv);
  scratch_buffers_allocator_deinit();
  scratch_buffers_global_deinit();
  stats_destroy();
  log_tags_global_deinit();
  log_msg_global_deinit();

  cfg_free(configuration);
  configuration = NULL;
  crypto_deinit();
  msg_deinit();
  return ret;
}
