/*
 * NOTE: most of the algorithms come from SLCT and LogHound, written by Risto Vaarandi
 */

#include <uuid/uuid.h>
#include <stdlib.h>

#include "patternize.h"
#include "logmsg.h"
#include "messages.h"
#include "tags.h"

/*
 * Constants
 */
#define PTZ_MAXLINELEN 10240
#define PTZ_MAXWORDS 512      /* maximum number of words in one line */
#define PTZ_LOGTABLE_ALLOC_BASE 3000
#define PTZ_WORDLIST_CACHE 3 // FIXME: make this a commandline parameter?

#if 0
static void _ptz_debug_print_word(gpointer key, gpointer value, gpointer dummy)
{
  fprintf(stderr, "%d: %s\n", *((guint*) value), (gchar*) key);
}

static void _ptz_debug_print_cluster(gpointer key, gpointer value, gpointer dummy)
{
  fprintf(stderr, "%s: %d\n", (gchar*) key, ((Cluster *) value)->support);
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

guint
ptz_load_file(Patternizer *self, gchar *input_file)
{
  FILE *file;
  guint lines = 0;
  int len;
  MsgFormatOptions parse_options;
  gchar line[PTZ_MAXLINELEN];
  // FIXME: log_msg_new needs a fake socket for msg parsing, which is crazy...
  GSockAddr *addr = g_sockaddr_inet_new("10.10.10.10", 1010);
  LogMessage *msg;

  memset(&parse_options, 0, sizeof(parse_options));
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);

  if (!input_file)
    {
      msg_error("No input file specified", evt_tag_str("filename", NULL), NULL);
      return 0;
    }

  if (strcmp(input_file, "-") != 0)
    {
      if (!(file = fopen(input_file, "r")))
        {
          msg_error("Can't open input file", evt_tag_str("filename", input_file), NULL);
          return 0;
        }
    }
  else
    {
      file = stdin;
    }

  self->logs = g_ptr_array_sized_new(PTZ_LOGTABLE_ALLOC_BASE);

  while (fgets(line, PTZ_MAXLINELEN, file))
    {
      len = strlen(line);
      if (line[len-1] == '\n')
        line[len-1] = 0;

      msg = log_msg_new(line, len, addr, &parse_options);
      g_ptr_array_add(self->logs, msg);
      ++lines;
    }

  return lines;
}

gboolean
ptz_find_frequent_words_remove_key_predicate(gpointer key, gpointer value, gpointer support)
{
  return (*((guint *) value) < GPOINTER_TO_UINT(support));
}

GHashTable *
ptz_find_frequent_words(GPtrArray *logs, guint num_of_logs, guint support, gboolean two_pass)
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
          msg_progress("Finding frequent words", evt_tag_str("phase", "caching"), NULL);
          srand(time(0));
          cachesize = (guint) ((num_of_logs * PTZ_WORDLIST_CACHE));
          cacheseed = rand();
          wordlist_cache = g_new0(int, cachesize);
        }
      else
        {
          msg_progress("Finding frequent words", evt_tag_str("phase", "searching"), NULL);
        }

      for (i = 0; i < num_of_logs; ++i)
        {
          msg = (LogMessage *) g_ptr_array_index(logs, i);
          msgstr = (gchar *) log_msg_get_value(msg, LM_V_MESSAGE, &msglen);
          /* NOTE: we should split on more than a simple space... */
          words = g_strsplit(msgstr, " ", PTZ_MAXWORDS);

          for (j = 0; words[j]; ++j)
            {
              /* NOTE: to calculate the key for the hash, we prefix a word with
               * its position in the row and a space -- as we split at spaces,
               * this should not create confusion
               */
              hash_key = g_strdup_printf("%d %s", j, words[j]);

              if (two_pass)
                cacheindex = ptz_str2hash(hash_key, cachesize, cacheseed);

              if (pass == 1)
                {
                  wordlist_cache[cacheindex]++;
                }
              if (pass == 2)
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

      //g_hash_table_foreach(wordlist, _ptz_debug_print_word, NULL);

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
  guint support, cluster_tag_id;
  int i;

  support = ((ClusterData *) data)->support;
  cluster_tag_id = ((ClusterData *) data)->cluster_tag_id;

  ret = (val->support < support);
  if (ret)
    {
      // remove cluster reference from the relevant logs
      for (i = 0; i < val->support; ++i)
        {
          msg = (LogMessage *) g_ptr_array_index(val->loglines, i);
          log_msg_clear_tag_by_id(msg, cluster_tag_id);
        }
    }

  return ret;

}

GHashTable *
ptz_find_clusters_slct(GPtrArray *logs, guint num_of_logs, guint support, guint cluster_tag_id, guint num_of_samples)
{
  GHashTable *wordlist;
  GHashTable *clusters;
  int i, j;
  LogMessage *msg;
  gchar *msgstr;
  gssize msglen;
  gchar **words;
  gchar *hash_key, *cluster_key;
  gboolean is_candidate;
  Cluster *cluster;
  ClusterData *data;

  /* get the frequent word list */
  wordlist = ptz_find_frequent_words(logs, num_of_logs, support, TRUE);
//  g_hash_table_foreach(wordlist, _debug_print, NULL);

  /* find the cluster candidates */
  clusters = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

  for (i = 0; i < num_of_logs; ++i)
    {
      msg = (LogMessage *) g_ptr_array_index(logs, i);
      msgstr = (gchar *) log_msg_get_value(msg, LM_V_MESSAGE, &msglen);

      /* NOTE: we should split on more than a simple space... */
      words = g_strsplit(msgstr, " ", PTZ_MAXWORDS);

      is_candidate = FALSE;
      cluster_key = g_strnfill(PTZ_MAXLINELEN * 2, 0);

      for (j = 0; words[j]; ++j)
        {
          hash_key = g_strdup_printf("%d %s", j, words[j]);

          if (g_hash_table_lookup(wordlist, hash_key))
            {
              gchar *to_add;
              to_add = g_strdup_printf("%s%c", hash_key, PTZ_SEPARATOR_CHAR);
              is_candidate = TRUE;
              strcat(cluster_key, to_add);
              g_free(to_add);
            }
          else
            {
              gchar *to_add;
              to_add = g_strdup_printf("%d *%c", j, PTZ_SEPARATOR_CHAR);
              strcat(cluster_key, to_add);
              g_free(to_add);
            }

          g_free(hash_key);
        }

      if (is_candidate)
        {
          cluster = (Cluster*) g_hash_table_lookup(clusters, cluster_key);

          if (!cluster)
             {
               cluster = g_new0(Cluster, 1);

               cluster->support = 1;
               if (num_of_samples > 0)
                 {
                   cluster->curr_samples = 1;
                   cluster->samples = g_ptr_array_sized_new(5);
                   g_ptr_array_add(cluster->samples, g_strdup(msgstr));
                 }
               cluster->loglines = g_ptr_array_sized_new(PTZ_LOGTABLE_ALLOC_BASE);
               g_ptr_array_add(cluster->loglines, (gpointer) msg);
               cluster->words = g_strdupv(words);

               g_hash_table_insert(clusters, cluster_key, (gpointer) cluster);
             }
           else
             {
               cluster->support += 1;
               g_ptr_array_add(cluster->loglines, (gpointer) msg);
               if (cluster->curr_samples < num_of_samples)
                 {
                   cluster->curr_samples += 1;
                   g_ptr_array_add(cluster->samples, g_strdup(msgstr));
                 }
               g_free(cluster_key);
             }
          log_msg_set_tag_by_id(msg, cluster_tag_id);
        }

      g_strfreev(words);
    }

  data = g_new0(ClusterData, 1);
  data->support = support;
  data->cluster_tag_id = cluster_tag_id;

  g_hash_table_foreach_remove(clusters, ptz_find_clusters_remove_cluster_predicate, data);

  g_free(data);

//  g_hash_table_foreach(clusters, _ptz_debug_print_cluster, NULL);

  g_hash_table_unref(wordlist);

  return clusters;
}

void
ptz_get_clusters_size_iterator(gpointer key, gpointer value, gpointer user_data)
{
  (*((guint *) user_data))++;
}

static inline guint
ptz_get_clusters_size(GHashTable *clusters)
{
  guint num_of_clusters = 0;

  g_hash_table_foreach(clusters, ptz_get_clusters_size_iterator,  &num_of_clusters);

  return num_of_clusters;
}

void
ptz_merge_clusterlists(gpointer _key, gpointer _value, gpointer _target)
{
  gchar *key = _key;
  Cluster *cluster = _value;
  GHashTable *target = _target;

  // FIXME: leaking here bad...
  g_hash_table_insert(target, g_strdup(key), cluster);
}

GHashTable *
ptz_find_clusters_step(Patternizer *self, GPtrArray *logs, guint num_of_logs, guint support, guint num_of_samples)
{
  msg_progress("Searching clusters", evt_tag_int("input lines", num_of_logs), NULL);
  if (self->algo == PTZ_ALGO_SLCT)
    return ptz_find_clusters_slct(logs, num_of_logs, support, self->cluster_tag_id, num_of_samples);
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
  guint curr_num_of_logs, prev_num_of_logs, curr_support;
  LogMessage *msg;
  int i;

  prev_logs = NULL;

  if (self->iterate == PTZ_ITERATE_NONE)
    return ptz_find_clusters_step(self, self->logs, self->num_of_logs, self->support, self->num_of_samples);

  if (self->iterate == PTZ_ITERATE_OUTLIERS)
    {
      ret_clusters =  g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
      curr_logs = self->logs;
      curr_num_of_logs = self->num_of_logs;
      curr_support= self->support;

      while (TRUE)
        {
          curr_clusters = ptz_find_clusters_step(self, curr_logs, curr_num_of_logs, curr_support, self->num_of_samples);

          if (ptz_get_clusters_size(curr_clusters) == 0)
            break;

          /* FIXME: this is where we leak bad... */
          g_hash_table_foreach(curr_clusters, ptz_merge_clusterlists, ret_clusters);

          prev_logs = curr_logs;
          prev_num_of_logs = curr_num_of_logs;
          curr_logs = g_ptr_array_sized_new(PTZ_LOGTABLE_ALLOC_BASE);
          curr_num_of_logs = 0;
          for (i = 0; i < prev_num_of_logs; ++i)
            {
              msg = (LogMessage *) g_ptr_array_index(prev_logs, i);
              if (!log_msg_is_tag_by_id(msg, self->cluster_tag_id))
                {
                  /* it's an outlier, include it in the next iteration */
                  g_ptr_array_add(curr_logs, msg);
                  ++curr_num_of_logs;
                }
            }
          curr_support = curr_num_of_logs * (self->support_treshold / 100.0);

          if (prev_logs != self->logs)
            {
              g_ptr_array_free(prev_logs, FALSE);
              prev_logs = NULL;
            }
        }

      if (prev_logs && prev_logs != self->logs)
        g_ptr_array_free(prev_logs, FALSE);
      if (curr_logs != self->logs)
        g_ptr_array_free(curr_logs, FALSE);

      return ret_clusters;
    }

  msg_error("Invalid iteration type", evt_tag_int("iteration_type", self->iterate), NULL);
  return NULL;

}

Patternizer *
ptz_new(gchar *input_file, gdouble support_treshold, guint algo, guint iterate, guint num_of_samples)
{
  Patternizer *self = g_new0(Patternizer, 1);

  self->algo = algo;
  self->iterate = iterate;

  self->num_of_logs = ptz_load_file(self, input_file);
  if (self->num_of_logs == 0)
    {
      msg_info("Empty input, exiting", NULL, NULL);
      return NULL;
    }
  self->support = (self->num_of_logs * (support_treshold / 100.0));
  self->support_treshold = support_treshold;
  self->num_of_samples = num_of_samples;
  log_msg_registry_init();
  log_tags_init();
  self->cluster_tag_id = log_tags_get_by_name(".in_patternize_cluster");

  return self;
}

void
ptz_free(Patternizer *self)
{
  int i;

  for (i = 0; i < self->num_of_logs; ++i)
    log_msg_unref((LogMessage *) (LogMessage *) g_ptr_array_index(self->logs, i));

  g_ptr_array_free(self->logs, TRUE);
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
  gboolean named_parsers = *((gboolean*) user_data);
  guint parser_counts[PTZ_NUM_OF_PARSERS];
  int i;
  uuid_t uuid;
  Cluster *cluster;

  if (named_parsers)
    {
      for (i = 0; i < PTZ_NUM_OF_PARSERS; ++i)
        parser_counts[i] = 0;
    }

  uuid_generate(uuid);
  uuid_unparse_lower(uuid, uuid_string);
  printf("      <rule id='%s' class='system' provider='patternize'>\n", uuid_string);
  printf("        <!-- support: %d -->\n", ((Cluster *) value)->support);
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
  for (i = 0; words[i]; ++i)
    {
      gchar **word_parts;

      word_parts = g_strsplit(words[i], " ", 2);

      if (word_parts[1][0] == '*')
        {
          /* NOTE: nasty workaround: do not display last ESTRING as syslog-ng won't handle that well... */
          /* FIXME: enter a simple @STRING@ here instead... */
          if (words[i + 1])
            {
              printf("@ESTRING:");
              if (named_parsers)
                {
                  // TODO: do not hardcode ESTRING here...
                  printf(".dict.string%d", parser_counts[PTZ_PARSER_ESTRING]++);
                }
              printf(": @");
            }
        }
      else
        {
          escapedstr = g_markup_escape_text(word_parts[1], -1);
          if (g_strrstr(escapedstr, "@"))
            {
              escapedparts = g_strsplit(escapedstr, "@", -1);
              g_free(escapedstr);
              escapedstr = g_strjoinv("@@", escapedparts);
              g_strfreev(escapedparts);
            }

          printf("%s", escapedstr);
          g_free(escapedstr);
          if (words[i + 1])
            printf(" ");
        }

      g_strfreev(word_parts);
    }

  g_free(skey);
  g_strfreev(words);

  printf("</pattern>\n");
  printf("        </patterns>\n");

  cluster = (Cluster *) value;
  if (cluster->curr_samples > 0)
    {
      printf("        <examples>\n");
      for (i = 0; i < cluster->curr_samples; ++i)
        {
          samplestr = (gchar *) g_ptr_array_index(cluster->samples, i);
          samplestr_escaped = g_markup_escape_text(samplestr, strlen(samplestr));
          printf("            <example>\n");
          printf("                <test_message program='patternize'>%s</test_message>\n", samplestr_escaped);
          printf("            </example>\n");
          g_free(samplestr);
          g_free(samplestr_escaped);
        }
      printf("        </examples>\n");
      printf("      </rule>\n");
    }

}

void
ptz_print_patterndb(GHashTable *clusters, gboolean named_parsers)
{
  char date[12], uuid_string[37];
  time_t currtime;
  uuid_t uuid;

  // print the header
  time(&currtime);
  strftime(date, 12, "%Y-%m-%d", localtime(&currtime));
  printf("<patterndb version='3' pub_date='%s'>\n", date);
  uuid_generate(uuid);
  uuid_unparse_lower(uuid, uuid_string);
  printf("  <ruleset name='patternize' id='%s'>\n", uuid_string);
  printf("    <rules>\n");

  g_hash_table_foreach(clusters, ptz_print_patterndb_rule, (gpointer) &named_parsers);

  printf("    </rules>\n");
  printf("  </ruleset>\n");
  printf("</patterndb>\n");

}
