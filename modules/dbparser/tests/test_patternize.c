/*
 * Copyright (c) 2010-2018 Balabit
 * Copyright (c) 2010-2013 Balázs Scheidler <balazs.scheidler@balabit.com>
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

#include "patternize.h"
#include "logmsg/logmsg.h"
#include "cfg.h"
#include "plugin.h"
#include "apphook.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

MsgFormatOptions parse_options;

static void setup(void)
{
  app_startup();
  configuration = cfg_new_snippet();
  cfg_load_module(configuration, "syslogformat");
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);
}

static void teardown(void)
{
  log_tags_global_deinit();
  msg_format_options_destroy(&parse_options);
  app_shutdown();
}

typedef struct _loglinesType
{
  GPtrArray *logmessages;
  guint num_of_logs;
} loglinesType;

loglinesType *
_get_logmessages(const gchar *logs)
{
  int i, len;
  loglinesType *self;
  gchar **input_lines;
  gchar *logline;
  LogMessage *msg;

  self = g_new(loglinesType, 1);
  self->logmessages = g_ptr_array_sized_new(10);
  self->num_of_logs = 0;

  input_lines = g_strsplit(logs, "\n", 0);

  for (i = 0; input_lines[i]; ++i)
    {
      logline = g_strdup_printf("Jul 29 06:25:41 vav zorp/inter_http[27940]: %s", input_lines[i]);
      len = strlen(logline);
      if (logline[len-1] == '\n')
        logline[len-1] = 0;

      msg = log_msg_new(logline, len, &parse_options);
      g_ptr_array_add(self->logmessages, msg);
      ++(self->num_of_logs);
      g_free(logline);
    }

  msg_format_options_destroy(&parse_options);
  g_strfreev(input_lines);
  return self;
}

typedef struct _patternize_params
{
  const gchar *logs;
  guint support;
  const gchar *expected;
} PatternizeParams;

ParameterizedTestParameters(dbparser, test_frequent_words)
{
  static PatternizeParams parser_params[] =
  {
    {
      .logs = "a\n",
      .support = 0,
      .expected = "0 a:1"
    },
    {
      .logs = "a b\n",
      .support = 0,
      .expected = "0 a:1,1 b:1",
    },
    {
      .logs = "a a\nb b",
      .support = 0,
      .expected = "0 a:1,1 a:1,0 b:1,1 b:1",
    },
    {
      .logs = "a b\nb a",
      .support = 0,
      .expected = "0 a:1,1 a:1,0 b:1,1 b:1",
    },
    {
      .logs = "a b\na b",
      .support = 0,
      .expected = "0 a:2,1 b:2",
    },
    /* support threshold tests */
    {
      .logs = "a\n",
      .support = 1,
      .expected = "",
    },
    {
      .logs = "a b\n",
      .support = 1,
      .expected = "",
    },
    {
      .logs = "a b\nb a",
      .support = 1,
      .expected = "0 a:1,1 a:1,0 b:1,1 b:1",
    },
    {
      .logs = "a b\nb a\na c",
      .support = 2,
      .expected = "0 a:2",
    }
  };

  return cr_make_param_array(PatternizeParams, parser_params, G_N_ELEMENTS(parser_params));
}

ParameterizedTest(PatternizeParams *param, dbparser, test_frequent_words, .init = setup, .fini = teardown)
{
  int i, twopass;
  gchar **expecteds;
  GHashTable *wordlist;
  loglinesType *logmessages;
  gchar *delimiters = " :&~?![]=,;()'\"";

  logmessages = _get_logmessages(param->logs);

  expecteds = g_strsplit(param->expected, ",", 0);

  for (twopass = 1; twopass <= 2; ++twopass)
    {
      wordlist = ptz_find_frequent_words(logmessages->logmessages, param->support, delimiters, twopass == 1);

      for (i = 0; expecteds[i]; ++i)
        {
          char **expected_item;
          char *expected_word;
          int expected_occurrence;
          guint ret;
          gpointer retp;

          expected_item = g_strsplit(expecteds[i], ":", 2);

          expected_word = expected_item[0];
          sscanf(expected_item[1], "%d", &expected_occurrence);

          retp = g_hash_table_lookup(wordlist, expected_word);
          if (retp)
            {
              ret = *((guint *) retp);
            }
          else
            {
              ret = 0;
            }

          cr_expect_eq(ret, (guint) expected_occurrence,
                       "Frequent words test case failed; word: '%s', expected=%d, got=%d, support=%d\nInput:%s\n",
                       expected_word, expected_occurrence, ret, param->support, param->logs);

          g_strfreev(expected_item);
        }
    }

  // cleanup
  g_strfreev(expecteds);
  for (i = 0; i < logmessages->num_of_logs; ++i)
    log_msg_unref((LogMessage *) g_ptr_array_index(logmessages->logmessages, i));

  g_ptr_array_free(logmessages->logmessages, TRUE);
  g_free(logmessages);
}


typedef struct _clusterfindData
{
  guint *lines;
  guint num_of_lines;
  guint support;
  GPtrArray *logs;
} clusterfindData;

typedef struct _clusterfind2Data
{
  gchar *search_line;
  gboolean found;
  guint lines_in_cluster;
} clusterfind2Data;

void
_clusters_loglines_find(gpointer value, gpointer user_data)
{
  clusterfind2Data *data;
  LogMessage *msg;
  gchar *msgstr;
  gssize msglen;

  data = (clusterfind2Data *) user_data;
  msg = (LogMessage *) value;

  ++(data->lines_in_cluster);

  msgstr = (gchar *) log_msg_get_value(msg, LM_V_MESSAGE, &msglen);
  if (strcmp(data->search_line, msgstr) == 0)
    data->found = TRUE;
}

gboolean
_clusters_find(gpointer key, gpointer value, gpointer user_data)
{
  int i;
  gchar *line;
  clusterfindData *data;
  clusterfind2Data *find_data;
  gboolean found;
  guint lines_in_cluster = 0, lines_found = 0;
  gssize msglen;

  data = ((clusterfindData *) user_data);

  found = TRUE;
  for (i = 0; i < data->num_of_lines; ++i)
    {

      line = g_strdup((gchar *) log_msg_get_value((LogMessage *) g_ptr_array_index(data->logs, data->lines[i]), LM_V_MESSAGE,
                                                  &msglen));

      find_data = g_new(clusterfind2Data, 1);
      find_data->search_line = line;
      find_data->found = FALSE;
      /* we count the number of lines in the cluster several times this way,
       * but why on earth should we optimize a unit test?... :)
       */
      find_data->lines_in_cluster = 0;
      g_ptr_array_foreach(((Cluster *) value)->loglines, _clusters_loglines_find, find_data);

      if (find_data->found)
        ++lines_found;
      else
        found = FALSE;

      lines_in_cluster = find_data->lines_in_cluster;

      g_free(find_data);
      g_free(line);

      /* at least one line is missing, this cannot be a match */
      if (!found)
        return FALSE;
    }

  /* if we got to this point, this means we have found all required rows, so we
   * only have to check for completeness
   */
  if (data->num_of_lines == lines_in_cluster)
    return TRUE;
  else
    return FALSE;
}

ParameterizedTestParameters(dbparser, test_find_clusters_slct)
{
  static PatternizeParams parser_params[] =
  {
    {
      .logs = "a\n",
      .support = 0,
      .expected = "0:1",
    },
    {
      .logs = "a\nb\n",
      .support = 0,
      .expected = "0:1|1:1",
    },
    {
      .logs = "a\nb\na\nb\n",
      .support = 2,
      .expected = "0,2:2|1,3:2",
    },
    {
      .logs = "alma korte korte alma\nalma korte\nbela korte\nalma\n",
      .support = 1,
      .expected = "0:1|1:1|2:1|3:1",
    },
    {
      .logs = "alma korte\n"
      "alma korte\n"
      "alma korte\n"
      "alma korte\n"
      "bela korte\n"
      "bela korte\n"
      "alma\n",
      .support = 2,
      .expected = "0,1,2,3:4|4,5:2",
    },
    {
      .logs = "alma korte\n"
      "alma korte\n"
      "alma korte\n"
      "alma korte\n"
      "bela korte\n"
      "bela korte\n"
      "alma\n",
      .support = 3,
      .expected = "0,1,2,3:4",
    },
    {
      .logs = "alma korte asdf1 labda\n"
      "alma korte asdf2 labda\n"
      "alma korte asdf3 labda\n"
      "sallala\n",
      .support = 3,
      .expected = "0,1,2:3",
    },
    {
      .logs = "alma korte asdf1 labda qwe1\n"
      "alma korte asdf2 labda qwe2\n"
      "alma korte asdf3 labda qwe3\n"
      "sallala\n",
      .support = 3,
      .expected = "0,1,2:3",
    }
  };

  return cr_make_param_array(PatternizeParams, parser_params, G_N_ELEMENTS(parser_params));
}

ParameterizedTest(PatternizeParams *param, dbparser, test_find_clusters_slct, .init = setup, .fini = teardown)
{
  int i,j;
  gchar **expecteds;
  loglinesType *logmessages;
  clusterfindData *find_data;
  GHashTable *clusters;
  Cluster *test_cluster;
  gchar *delimiters = " :&~?![]=,;()'\"";

  logmessages = _get_logmessages(param->logs);

  clusters = ptz_find_clusters_slct(logmessages->logmessages, param->support, delimiters, 0);

  expecteds = g_strsplit(param->expected, "|", 0);
  for (i = 0; expecteds[i]; ++i)
    {
      gchar **expected_item, **expected_lines_s;
      guint expected_lines[100];
      guint num_of_expected_lines = 0;
      guint expected_support;

      expected_item = g_strsplit(expecteds[i], ":", 0);
      sscanf(expected_item[1], "%d", &expected_support);

      expected_lines_s = g_strsplit(expected_item[0], ",", 0);

      for (j = 0; expected_lines_s[j]; ++j)
        {
          sscanf(expected_lines_s[j], "%d", &expected_lines[j]);
          ++num_of_expected_lines;
        }

      find_data = g_new(clusterfindData, 1);
      find_data->lines = expected_lines;
      find_data->num_of_lines = num_of_expected_lines;
      find_data->logs = logmessages->logmessages;
      test_cluster = (Cluster *) g_hash_table_find(clusters, _clusters_find, find_data);

      cr_expect_not(!test_cluster || test_cluster->loglines->len != expected_support,
                    "expected_cluster='%s', expected_support='%d'\nInput:\n%s\n", expected_item[0], expected_support, param->logs);

      g_free(find_data);
      g_strfreev(expected_item);
      g_strfreev(expected_lines_s);
    }

  g_hash_table_unref(clusters);
  for (i = 0; i < logmessages->num_of_logs; ++i)
    log_msg_unref((LogMessage *) g_ptr_array_index(logmessages->logmessages, i));

  g_ptr_array_free(logmessages->logmessages, TRUE);
  g_free(logmessages);
  g_strfreev(expecteds);
}
