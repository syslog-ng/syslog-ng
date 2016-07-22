/*
 * Copyright (c) 2010-2012 Balabit
 * Copyright (c) 2009-2011 Péter Gyöngyösi
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
#include "messages.h"
#include "uuid.h"

#include <stdlib.h>
#include <string.h>

/*
 * NOTE: most of the algorithms come from SLCT and LogHound, written by Risto Vaarandi
 */


/*
 * Constants
 */
#define PTZ_MAXLINELEN 10240
#define PTZ_MAXWORDS 512      /* maximum number of words in one line */
#define PTZ_LOGTABLE_ALLOC_BASE 3000
#define PTZ_WORDLIST_CACHE 3 /* FIXME: make this a commandline parameter? */

static LogTagId cluster_tag_id;

#if 0
static void _ptz_debug_print_word(gpointer key, gpointer value, gpointer dummy)
{
  fprintf(stderr, "%d: %s\n", *((guint *) value), (gchar *) key);
}

static void _ptz_debug_print_cluster(gpointer key, gpointer value, gpointer dummy)
{
  fprintf(stderr, "%s: %s\n", (gchar *) key, ((Cluster *) value)->words[0]);
}
#endif

guint
ptz_str2hash(gchar *string, guint modulo, guint seed)
{
  int i;

  /* fast string hashing algorithm by M.V.Ramakrishna and Justin Zobel */
  for (i = 0; string[i] != 0; ++i)
    {
      seed = seed ^ ((seed << 5) + (seed >> 2) + string[i]);
    }

  return seed % modulo;
}

gchar *
ptz_find_delimiters(gchar *str, const gchar *delimdef)
{
  gchar *remainder;
  GString *delimiters = g_string_sized_new(32);

  remainder = str;
  while (remainder[0] != 0)
    {
      remainder += strcspn(remainder, delimdef);
      if (remainder[0] != 0)
        {
          g_string_append_c(delimiters, remainder[0]);
          remainder++;
        }
    }

  return g_string_free(delimiters, FALSE);
}

gboolean
ptz_find_frequent_words_remove_key_predicate(gpointer key, gpointer value, gpointer support)
{
  return (*((guint *) value) < GPOINTER_TO_UINT(support));
}

GHashTable *
ptz_find_frequent_words(GPtrArray *logs, guint support, const gchar *delimiters, gboolean two_pass)
{
  int i, j, pass;
  guint *curr_count;
  LogMessage *msg;
  gchar *msgstr;
  gssize msglen;
  gchar **words;
  GHashTable *wordlist;
  int *wordlist_cache = NULL;
  guint cachesize = 0, cacheseed = 0, cacheindex = 0;
  gchar *hash_key;

  wordlist = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

  for (pass = (two_pass ? 1 : 2); pass <= 2; ++pass)
    {
      if (pass == 1)
        {
          msg_progress("Finding frequent words",
                       evt_tag_str("phase", "caching"));
          srand(time(NULL));
          cachesize = (guint) ((logs->len * PTZ_WORDLIST_CACHE));
          cacheseed = rand();
          wordlist_cache = g_new0(int, cachesize);
        }
      else
        {
          msg_progress("Finding frequent words",
                       evt_tag_str("phase", "searching"));
        }

      for (i = 0; i < logs->len; ++i)
        {
          msg = (LogMessage *) g_ptr_array_index(logs, i);
          msgstr = (gchar *) log_msg_get_value(msg, LM_V_MESSAGE, &msglen);

          words = g_strsplit_set(msgstr, delimiters, PTZ_MAXWORDS);

          for (j = 0; words[j]; ++j)
            {
              /* NOTE: to calculate the key for the hash, we prefix a word with
               * its position in the row and a space -- as we always split at
               * spaces, this should not create confusion
               */
              hash_key = g_strdup_printf("%d %s", j, words[j]);

              if (two_pass)
                cacheindex = ptz_str2hash(hash_key, cachesize, cacheseed);

              if (pass == 1)
                {
                  wordlist_cache[cacheindex]++;
                }
              else if (pass == 2)
                {
                  if (!two_pass || wordlist_cache[cacheindex] >= support)
                    {
                      curr_count = (guint *) g_hash_table_lookup(wordlist, hash_key);
                      if (!curr_count)
                        {
                          guint *currcount_ref = g_new(guint, 1);
                          (*currcount_ref) = 1;
                          g_hash_table_insert(wordlist, g_strdup(hash_key), currcount_ref);
                        }
                      else
                        {
                          (*curr_count)++;
                        }
                    }
                }

              g_free(hash_key);

            }

          g_strfreev(words);
        }

      /* g_hash_table_foreach(wordlist, _ptz_debug_print_word, NULL); */

      g_hash_table_foreach_remove(wordlist, ptz_find_frequent_words_remove_key_predicate, GUINT_TO_POINTER(support));
    }

  if (wordlist_cache)
    g_free(wordlist_cache);

  return wordlist;
}

gboolean
ptz_find_clusters_remove_cluster_predicate(gpointer key, gpointer value, gpointer data)
{
  Cluster *val = (Cluster *) value;
  gboolean ret;
  LogMessage *msg;
  guint support;
  int i;

  support = GPOINTER_TO_UINT(data);

  ret = (val->loglines->len < support);
  if (ret)
    {
      /* remove cluster reference from the relevant logs */
      for (i = 0; i < val->loglines->len; ++i)
        {
          msg = (LogMessage *) g_ptr_array_index(val->loglines, i);
          log_msg_clear_tag_by_id(msg, cluster_tag_id);
        }
    }

  return ret;

}

static void
cluster_free(Cluster *cluster)
{
  gint i;

  if (cluster->samples)
    {
      for (i = 0; i < cluster->samples->len; i++)
        g_free(g_ptr_array_index(cluster->samples, i));

      g_ptr_array_free(cluster->samples, TRUE);
    }
  g_ptr_array_free(cluster->loglines, TRUE);
  g_strfreev(cluster->words);
  g_free(cluster);
}

GHashTable *
ptz_find_clusters_slct(GPtrArray *logs, guint support, const gchar *delimiters, guint num_of_samples)
{
  GHashTable *wordlist;
  GHashTable *clusters;
  int i, j;
  LogMessage *msg;
  gchar *msgstr;
  gssize msglen;
  gchar **words;
  gchar *hash_key;
  gboolean is_candidate;
  Cluster *cluster;
  GString *cluster_key;
  gchar *msgdelimiters;

  /* get the frequent word list */
  wordlist = ptz_find_frequent_words(logs, support, delimiters, TRUE);
  /* g_hash_table_foreach(wordlist, _ptz_debug_print_word, NULL); */

  /* find the cluster candidates */
  clusters = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) cluster_free);
  cluster_key = g_string_sized_new(0);
  for (i = 0; i < logs->len; ++i)
    {
      msg = (LogMessage *) g_ptr_array_index(logs, i);
      msgstr = (gchar *) log_msg_get_value(msg, LM_V_MESSAGE, &msglen);

      g_string_truncate(cluster_key, 0);

      words = g_strsplit_set(msgstr, delimiters, PTZ_MAXWORDS);
      msgdelimiters = ptz_find_delimiters(msgstr, delimiters);

      is_candidate = FALSE;
      for (j = 0; words[j]; ++j)
        {
          hash_key = g_strdup_printf("%d %s", j, words[j]);

          if (g_hash_table_lookup(wordlist, hash_key))
            {
              is_candidate = TRUE;
              g_string_append(cluster_key, hash_key);
              g_string_append_c(cluster_key, PTZ_SEPARATOR_CHAR);
            }
          else
            {
              g_string_append_printf(cluster_key, "%d %c%c", j, PTZ_PARSER_MARKER_CHAR, PTZ_SEPARATOR_CHAR);
            }

          g_free(hash_key);
        }

      /* append the delimiters of the message to the cluster key to assure unicity
       * otherwise the same words with different delimiters would still show as the
       * same cluster
       */
      g_string_append_printf(cluster_key, "%s%c", msgdelimiters, PTZ_SEPARATOR_CHAR);
      g_free(msgdelimiters);

      if (is_candidate)
        {
          cluster = (Cluster *) g_hash_table_lookup(clusters, cluster_key->str);

          if (!cluster)
            {
              cluster = g_new0(Cluster, 1);

              if (num_of_samples > 0)
                {
                  cluster->samples = g_ptr_array_sized_new(5);
                  g_ptr_array_add(cluster->samples, g_strdup(msgstr));
                }
              cluster->loglines = g_ptr_array_sized_new(64);
              g_ptr_array_add(cluster->loglines, (gpointer) msg);
              cluster->words = g_strdupv(words);

              g_hash_table_insert(clusters, g_strdup(cluster_key->str), (gpointer) cluster);
            }
          else
            {
              g_ptr_array_add(cluster->loglines, (gpointer) msg);
              if (cluster->samples && cluster->samples->len < num_of_samples)
                {
                  g_ptr_array_add(cluster->samples, g_strdup(msgstr));
                }
            }
          log_msg_set_tag_by_id(msg, cluster_tag_id);
        }

      g_strfreev(words);
    }

  g_hash_table_foreach_remove(clusters, ptz_find_clusters_remove_cluster_predicate, GUINT_TO_POINTER(support));

  /* g_hash_table_foreach(clusters, _ptz_debug_print_cluster, NULL); */

  g_hash_table_unref(wordlist);
  g_string_free(cluster_key, TRUE);

  return clusters;
}

/* callback function for g_hash_table_foreach_steal to migrate elements from one hash to the other */
static gboolean
ptz_merge_clusterlists(gpointer _key, gpointer _value, gpointer _target)
{
  gchar *key = _key;
  Cluster *cluster = _value;
  GHashTable *target = _target;

  g_hash_table_insert(target, key, cluster);
  return TRUE;
}

GHashTable *
ptz_find_clusters_step(Patternizer *self, GPtrArray *logs, guint support, guint num_of_samples)
{
  msg_progress("Searching clusters", evt_tag_int("input lines", logs->len));
  if (self->algo == PTZ_ALGO_SLCT)
    return ptz_find_clusters_slct(logs, support, self->delimiters, num_of_samples);
  else
    {
      msg_error("Unknown clustering algorithm", evt_tag_int("algo_id", self->algo));
      return NULL;
    }
}

GHashTable *
ptz_find_clusters(Patternizer *self)
{
  /* FIXME: maybe we should dup everything... this way
   * we give out a clusters hash that contains references
   * to our internal logs array...
   */
  GHashTable *curr_clusters;
  GHashTable *ret_clusters;
  GPtrArray *prev_logs, *curr_logs;
  guint curr_support;
  LogMessage *msg;
  int i;

  prev_logs = NULL;

  if (self->iterate == PTZ_ITERATE_NONE)
    return ptz_find_clusters_step(self, self->logs, self->support, self->num_of_samples);

  if (self->iterate == PTZ_ITERATE_OUTLIERS)
    {
      ret_clusters =  g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) cluster_free);
      curr_logs = self->logs;
      curr_support = self->support;

      while (TRUE)
        {
          curr_clusters = ptz_find_clusters_step(self, curr_logs, curr_support, self->num_of_samples);

          if (g_hash_table_size(curr_clusters) == 0)
            {
              g_hash_table_destroy(curr_clusters);
              break;
            }

          g_hash_table_foreach_steal(curr_clusters, ptz_merge_clusterlists, ret_clusters);
          g_hash_table_destroy(curr_clusters);

          prev_logs = curr_logs;
          curr_logs = g_ptr_array_sized_new(g_hash_table_size(curr_clusters));
          for (i = 0; i < prev_logs->len; ++i)
            {
              msg = (LogMessage *) g_ptr_array_index(prev_logs, i);
              if (!log_msg_is_tag_by_id(msg, cluster_tag_id))
                {
                  /* it's an outlier, include it in the next iteration */
                  g_ptr_array_add(curr_logs, msg);
                }
            }
          curr_support = curr_logs->len * (self->support_treshold / 100.0);

          if (prev_logs != self->logs)
            {
              g_ptr_array_free(prev_logs, TRUE);
              prev_logs = NULL;
            }
        }

      if (prev_logs && prev_logs != self->logs)
        g_ptr_array_free(prev_logs, TRUE);
      if (curr_logs != self->logs)
        g_ptr_array_free(curr_logs, TRUE);

      return ret_clusters;
    }

  msg_error("Invalid iteration type", evt_tag_int("iteration_type", self->iterate));
  return NULL;

}


void
ptz_print_patterndb_rule(gpointer key, gpointer value, gpointer user_data)
{
  char uuid_string[37];
  gchar **words;
  gchar *skey;
  gchar *splitstr;
  gchar *escapedstr;
  gchar **escapedparts;
  gchar *samplestr, *samplestr_escaped;
  gboolean named_parsers = *((gboolean *) user_data);
  guint parser_counts[PTZ_NUM_OF_PARSERS];
  int i;
  Cluster *cluster;
  GString *pattern = g_string_new("");
  guint wordcount;
  gchar *delimiters;

  cluster = (Cluster *) value;

  if (named_parsers)
    {
      for (i = 0; i < PTZ_NUM_OF_PARSERS; ++i)
        parser_counts[i] = 0;
    }

  uuid_gen_random(uuid_string, sizeof(uuid_string));

  printf("      <rule id='%s' class='system' provider='patternize'>\n", uuid_string);
  printf("        <!-- support: %d -->\n", cluster->loglines->len);
  printf("        <patterns>\n");
  printf("          <pattern>");

  /* we have to help strsplit a bit here so that we
   * won't get junk as the last word
   */
  skey = g_strdup((gchar *) key);
  if (skey[strlen(skey) -1] == PTZ_SEPARATOR_CHAR)
    skey[strlen(skey) -1] = 0;

  splitstr = g_strdup_printf("%c", PTZ_SEPARATOR_CHAR);
  words = g_strsplit(skey, splitstr, 0);
  g_free(splitstr);

  /* pop the delimiters from the cluster key */
  wordcount = g_strv_length(words);
  delimiters = words[wordcount-1];
  words[wordcount-1] = 0;

  for (i = 0; words[i]; ++i)
    {
      g_string_truncate(pattern, 0);

      gchar **word_parts;
      word_parts = g_strsplit(words[i], " ", 2);

      if (word_parts[1][0] == PTZ_PARSER_MARKER_CHAR)
        {
          /* NOTE: nasty workaround: do not display last ESTRING as syslog-ng won't handle that well... */
          /* FIXME: enter a simple @STRING@ here instead... */
          if (words[i + 1])
            {
              g_string_append(pattern, "@ESTRING:");
              if (named_parsers)
                {
                  /* TODO: do not hardcode ESTRING here... */
                  g_string_append_printf(pattern, ".dict.string%d", parser_counts[PTZ_PARSER_ESTRING]++);
                }
              g_string_append_printf(pattern, ":%c@", delimiters[i]);
              escapedstr = g_markup_escape_text(pattern->str, -1);
              printf("%s", escapedstr);
              g_free(escapedstr);
            }
        }
      else
        {
          g_string_append(pattern, word_parts[1]);

          if (words[i + 1])
            g_string_append_printf(pattern, "%c", delimiters[i]);

          escapedstr = g_markup_escape_text(pattern->str, -1);
          if (g_strrstr(escapedstr, "@"))
            {
              escapedparts = g_strsplit(escapedstr, "@", -1);
              g_free(escapedstr);
              escapedstr = g_strjoinv("@@", escapedparts);
              g_strfreev(escapedparts);
            }
          printf("%s", escapedstr);
          g_free(escapedstr);
        }
      g_strfreev(word_parts);
    }

  g_free(skey);
  g_free(delimiters);
  g_strfreev(words);
  g_string_free(pattern, TRUE);

  printf("</pattern>\n");
  printf("        </patterns>\n");

  if (cluster->samples->len > 0)
    {
      printf("        <examples>\n");
      for (i = 0; i < cluster->samples->len; ++i)
        {
          samplestr = (gchar *) g_ptr_array_index(cluster->samples, i);
          samplestr_escaped = g_markup_escape_text(samplestr, strlen(samplestr));
          printf("            <example>\n");
          printf("                <test_message program='patternize'>%s</test_message>\n", samplestr_escaped);
          printf("            </example>\n");
          g_free(samplestr_escaped);
        }
      printf("        </examples>\n");
      printf("      </rule>\n");
    }

}

void
ptz_print_patterndb(GHashTable *clusters, const gchar *delimiters, gboolean named_parsers)
{
  char date[12], uuid_string[37];
  time_t currtime;

  /* print the header */
  time(&currtime);
  strftime(date, 12, "%Y-%m-%d", localtime(&currtime));
  printf("<patterndb version='4' pub_date='%s'>\n", date);
  uuid_gen_random(uuid_string, sizeof(uuid_string));

  printf("  <ruleset name='patternize' id='%s'>\n", uuid_string);
  printf("    <rules>\n");

  g_hash_table_foreach(clusters, ptz_print_patterndb_rule, (gpointer *) &named_parsers);

  printf("    </rules>\n");
  printf("  </ruleset>\n");
  printf("</patterndb>\n");

}

gboolean
ptz_load_file(Patternizer *self, gchar *input_file, gboolean no_parse, GError **error)
{
  FILE *file;
  int len;
  MsgFormatOptions parse_options;
  gchar line[PTZ_MAXLINELEN];
  LogMessage *msg;

  if (!input_file)
    {
      g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_IO, "No input file specified");
      return FALSE;
    }

  if (strcmp(input_file, "-") != 0)
    {
      if (!(file = fopen(input_file, "r")))
        {
          g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_IO, "Error opening input file %s", input_file);
          return FALSE;
        }
    }
  else
    {
      file = stdin;
    }

  memset(&parse_options, 0, sizeof(parse_options));
  msg_format_options_defaults(&parse_options);
  if (no_parse)
    parse_options.flags |= LP_NOPARSE;
  else
    parse_options.flags |= LP_SYSLOG_PROTOCOL;
  msg_format_options_init(&parse_options, configuration);

  while (fgets(line, PTZ_MAXLINELEN, file))
    {
      len = strlen(line);
      if (line[len-1] == '\n')
        line[len-1] = 0;

      msg = log_msg_new(line, len, NULL, &parse_options);
      g_ptr_array_add(self->logs, msg);
    }

  self->support = (self->logs->len * (self->support_treshold / 100.0));
  msg_format_options_destroy(&parse_options);
  return TRUE;
}

Patternizer *
ptz_new(gdouble support_treshold, guint algo, guint iterate, guint num_of_samples, const gchar *delimiters)
{
  Patternizer *self = g_new0(Patternizer, 1);

  self->algo = algo;
  self->iterate = iterate;

  self->support_treshold = support_treshold;
  self->num_of_samples = num_of_samples;
  self->delimiters = delimiters;
  self->logs = g_ptr_array_sized_new(PTZ_LOGTABLE_ALLOC_BASE);

  cluster_tag_id = log_tags_get_by_name(".in_patternize_cluster");
  return self;
}

void
ptz_free(Patternizer *self)
{
  int i;

  for (i = 0; i < self->logs->len; ++i)
    log_msg_unref((LogMessage *) (LogMessage *) g_ptr_array_index(self->logs, i));

  g_ptr_array_free(self->logs, TRUE);
  g_free(self);
}
