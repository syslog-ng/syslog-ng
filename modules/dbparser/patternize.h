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

#ifndef PATTERNIZE_H_INCLUDED
#define PATTERNIZE_H_INCLUDED

#define PTZ_ALGO_SLCT 1
#define PTZ_ALGO_LOGHOUND 2

#define PTZ_ITERATE_NONE 0
#define PTZ_ITERATE_OUTLIERS 1
#define PTZ_ITERATE_HIEARARCH 2

#define PTZ_SEPARATOR_CHAR 0x1E
#define PTZ_PARSER_MARKER_CHAR 0x1A

#define PTZ_NUM_OF_PARSERS 1
#define PTZ_PARSER_ESTRING 0

#include "syslog-ng.h"

typedef struct _Patternizer
{
  guint algo;
  guint iterate;
  guint support;
  guint num_of_samples;
  gdouble support_treshold;
  const gchar *delimiters;

  // NOTE: for now, we store all logs read in in the memory.
  // This brings in some obvious constraints and should be solved
  // in a more optimized way later.
  GPtrArray *logs;

} Patternizer;

typedef struct _Cluster
{
  GPtrArray *loglines;
  char **words;
  GPtrArray *samples;
} Cluster;

/* only declared for the test program */
GHashTable *ptz_find_frequent_words(GPtrArray *logs, guint support, const gchar *delimiters, gboolean two_pass);
GHashTable *ptz_find_clusters_slct(GPtrArray *logs, guint support, const gchar *delimiters, guint num_of_samples);


GHashTable *ptz_find_clusters(Patternizer *self);
void ptz_print_patterndb(GHashTable *clusters, const gchar *delimiters, gboolean named_parsers);

gboolean ptz_load_file(Patternizer *self, gchar *input_file, gboolean no_parse, GError **error);

Patternizer *ptz_new(gdouble support_treshold, guint algo, guint iterate, guint num_of_samples,
                     const gchar *delimiters);
void ptz_free(Patternizer *self);

#endif
