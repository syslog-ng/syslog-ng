#include "syslog-ng.h"
#include "messages.h"
#include "templates.h"
#include "misc.h"
#include "logpatterns.h"
#include "logparser.h"
#include "radix.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if HAVE_GETOPT_H
#include <getopt.h>
#endif

#define BOOL(x) ((x) ? "TRUE" : "FALSE")


static gchar *patterndb_file = PATH_PATTERNDB_FILE;

static void
print_indented(FILE *stream, const gchar *str, gsize len)
{
  const gchar *eol;
  const gchar *sol;

  sol = str;
  while (len > 0)
    {
      eol = memchr(sol, '\n', len);
      if (!eol)
        eol = sol + len;

      if (sol != eol)
        fprintf(stream, "    %.*s\n", (gint) (eol - sol), sol);
      len -= eol - sol + 1;
      sol = eol + 1;
    }
}

static gchar *match_program = NULL;
static gchar *match_message = NULL;

void
pdbtool_match_values(gpointer key, gpointer value, gpointer user_data)
{
  gchar *n = key;
  gchar *v = value;
  gint *ret = user_data;

  printf("%s=%s\n", n, v);
  if (g_str_equal(n, ".classifier.rule_id"))
    *ret = 1;
}

static gint
pdbtool_match(int argc, char *argv[])
{
  LogPatternDatabase patterndb;
  LogMessage *msg = log_msg_new_empty();
  gint ret = 1;

  if (!match_message)
    {
      msg_error("No message is given", NULL);
      return ret;
    }

  memset(&patterndb, 0x0, sizeof(LogPatternDatabase));

  if (!log_pattern_database_load(&patterndb, patterndb_file))
    return ret;

  log_msg_set_message(msg, match_message, strlen(match_message));
  if (match_program && match_program[0])
    log_msg_set_program(msg, match_program, strlen(match_program));

  log_db_parser_process_lookup(&patterndb, msg);

  g_hash_table_foreach(msg->values, pdbtool_match_values, &ret);

  log_pattern_database_free(&patterndb);
  log_msg_unref(msg);
  return ret;
}

static GOptionEntry match_options[] =
{
  { "program", 'P', 0, G_OPTION_ARG_STRING, &match_program,
    "Program name to match as $PROGRAM", "<program>" },
  { "message", 'M', 0, G_OPTION_ARG_STRING, &match_message,
    "Message to match as $MSG", "<message>" },
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
    printf("@%s:%s@ ", r_parser_type_name(root->parser->type), root->parser->name ? root->parser->name : "");
  else
    printf("'%s' ", root->key ? root->key : "");

  if (root->value)
    {
      if (!program)
        printf("rule_id='%s'", ((LogDBResult*)root->value)->rule_id);
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
 LogPatternDatabase patterndb;

 memset(&patterndb, 0x0, sizeof(LogPatternDatabase));

 if (!log_pattern_database_load(&patterndb, patterndb_file))
   return 1;

 if (dump_program_tree)
   pdbtool_walk_tree(patterndb.programs, 0, TRUE);
 else if (match_program)
   {
     RNode *ruleset = r_find_node(patterndb.programs, g_strdup(match_program), g_strdup(match_program), strlen(match_program), NULL, NULL);
     if (ruleset && ruleset->value)
       pdbtool_walk_tree(((LogDBProgram *)ruleset->value)->rules, 0, FALSE);
   }

 return 0;
}

static GOptionEntry dump_options[] =
{
  { "program", 'P', 0, G_OPTION_ARG_STRING, &match_program,
    "Program name ($PROGRAM) to dump", "<program>" },
  { "program-tree", 'T', 0, G_OPTION_ARG_NONE, &dump_program_tree,
    "Dump the program ($PROGRAM) tree", NULL },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};



const gchar *
pdbtool_mode(int *argc, char **argv[])
{
  gint i;
  const gchar *mode;
  const gchar *slash;

  slash = strrchr((*argv)[0], '/');

  /*if ((*argc) > 0 && ((slash && strcmp(slash+1, "logcat") == 0) || (strcmp((*argv)[0], "logcat") == 0)))
    {
      return "cat";
    }
*/
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
  { "pdb",       'p', 0, G_OPTION_ARG_STRING, &patterndb_file,
    "Name of the patterndb file", "<patterndb_file>" },
  { "debug",     'd', 0, G_OPTION_ARG_NONE, &debug_flag,
    "Enable debug/diagnostic messages on stderr", NULL },
  { "verbose",   'v', 0, G_OPTION_ARG_NONE, &verbose_flag,
    "Enable verbose messages on stderr", NULL },
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
          break;
        }
    }
  if (!ctx)
    {
      fprintf(stderr, "Unknown command\n");
      usage();
    }

  if (!g_option_context_parse(ctx, &argc, &argv, &error))
    {
      fprintf(stderr, "Error parsing command line arguments: %s", error ? error->message : "Invalid arguments");
      g_clear_error(&error);
      g_option_context_free(ctx);
      return 1;
    }
  g_option_context_free(ctx);

  msg_init(TRUE);
  ret = modes[mode].main(argc, argv);
  msg_deinit();
  return ret;
}
