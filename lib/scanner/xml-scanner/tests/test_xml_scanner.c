/*
 * Copyright (c) 2018 Balabit
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

#include <criterion/criterion.h>
#include <criterion/parameterized.h>
#include "xml-scanner.h"
#include "apphook.h"
#include "scratch-buffers.h"
#include "cfg.h"


typedef struct
{
  gchar *name;
  gchar *value;
} NameValuePair;

typedef struct
{
  NameValuePair *expected_pairs;
  PushCurrentKeyValueCB test_push_function;
} Expected;

typedef struct
{
  gboolean strip_whitespaces;
  GList *exclude_tags;
  Expected  expected;
} XMLScannerTestOptions;


static void
setup(void)
{
  configuration = cfg_new_snippet();
  app_startup();
}

static void
teardown(void)
{
  scratch_buffers_explicit_gc();
  app_shutdown();
  cfg_free(configuration);
}

static XMLScannerOptions *
_create_new_scanner_options(XMLScannerTestOptions *test_options)
{
  XMLScannerOptions *scanner_options = g_new0(XMLScannerOptions, 1);
  xml_scanner_options_defaults(scanner_options);
  if (test_options->strip_whitespaces)
    xml_scanner_options_set_strip_whitespaces(scanner_options, test_options->strip_whitespaces);
  if (test_options->exclude_tags)
    xml_scanner_options_set_and_compile_exclude_tags(scanner_options, test_options->exclude_tags);
  return scanner_options;
}

static XMLScanner *
_construct_xml_scanner(XMLScannerTestOptions *test_options)
{
  XMLScanner *scanner = g_new(XMLScanner, 1);
  XMLScannerOptions *scanner_options = _create_new_scanner_options(test_options);

  xml_scanner_init(scanner, scanner_options, test_options->expected.test_push_function, test_options, NULL);
  return scanner;
}

static void
_destroy_xml_scanner(XMLScanner *scanner)
{
  xml_scanner_options_destroy(scanner->options);
  g_free(scanner->options);

  xml_scanner_deinit(scanner);
  g_free(scanner);
}

#define REPORT_POSSIBLE_PARSE_ERROR(x) (_report_possible_parse_error((x), __LINE__))

static void
_report_possible_parse_error(GError *error, gint line)
{
  cr_expect(error == NULL, "Unexpected error happened in %d", line);
  if (error)
    {
      fprintf(stderr, "Parsing error happened: %s\n", error->message);
      g_error_free(error);
    }
}

TestSuite(xml_scanner, .init = setup, .fini = teardown);


Test(xml_scanner, shouldreverse)
{
  GList *with_wildcard;

  with_wildcard = NULL;
  with_wildcard = g_list_append(with_wildcard, "tag1");
  with_wildcard = g_list_append(with_wildcard, "tag2");
  with_wildcard = g_list_append(with_wildcard, "tag*");
  cr_assert(joker_or_wildcard(with_wildcard));
  g_list_free(with_wildcard);

  GList *with_joker = NULL;
  with_joker = g_list_append(with_joker, "tag1");
  with_joker = g_list_append(with_joker, "t?g2");
  with_joker = g_list_append(with_joker, "tag3");
  cr_assert(joker_or_wildcard(with_joker));
  g_list_free(with_joker);

  GList *no_joker_or_wildcard = NULL;
  no_joker_or_wildcard = g_list_append(no_joker_or_wildcard, "tag1");
  no_joker_or_wildcard = g_list_append(no_joker_or_wildcard, "tag2");
  no_joker_or_wildcard = g_list_append(no_joker_or_wildcard, "tag3");
  cr_assert_not(joker_or_wildcard(no_joker_or_wildcard));
  g_list_free(no_joker_or_wildcard);

}



static void
_test_strip(const gchar *name, const gchar *value, gssize value_length, gpointer user_data)
{
  static guint times_called = 0;
  XMLScannerTestOptions *test_options = (XMLScannerTestOptions *) user_data;
  NameValuePair *expected_pairs = test_options->expected.expected_pairs;

  fprintf(stderr, "Name-value pushed!!! name:%s\tvalue:%s (value_len:%ld)\n", name, value, value_length);

  cr_assert_str_eq(name, expected_pairs[times_called].name);
  cr_assert_str_eq(value, expected_pairs[times_called].value);
  times_called++;
}

Test(xml_scanner, test_strip_whitespaces)
{
  const gchar *input = "<tag> \n\t part1 <tag2/> part2 \n\n</tag>";

  XMLScanner *xml_scanner = _construct_xml_scanner(&(XMLScannerTestOptions)
  {
    .strip_whitespaces = TRUE,
     .expected.test_push_function = _test_strip,
               .expected.expected_pairs = (NameValuePair [])
    {
      {".tag", "part1part2"},
    }
  });

  GError *error = NULL;
  xml_scanner_parse(xml_scanner, input, strlen(input), &error);
  REPORT_POSSIBLE_PARSE_ERROR(error);

  _destroy_xml_scanner(xml_scanner);
}

