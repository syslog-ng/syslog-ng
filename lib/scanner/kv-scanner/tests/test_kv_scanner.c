/*
 * Copyright (c) 2015-2017 Balabit
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
#include <stdio.h>
#include "stopwatch.h"

#include "kv-scanner.h"

static gboolean
_expect_no_more_tokens(KVScanner *scanner, gchar **error)
{
  GString *msg;
  gboolean ok = kv_scanner_scan_next(scanner);

  if (ok)
    {
      msg = g_string_new("kv_scanner is expected to return no more key-value pairs ");
      do
        {
          const gchar *key = kv_scanner_get_current_key(scanner);
          if (!key)
            key = "";
          const gchar *value = kv_scanner_get_current_value(scanner);
          if (!value)
            value = "";
          g_string_append_printf(msg, "[%s/%s]", key, value);
        }
      while (kv_scanner_scan_next(scanner));

      *error = g_string_free(msg, FALSE);
      return FALSE;
    }
  return TRUE;
}

static gboolean
_expect_current_key_equals(KVScanner *scanner, const gchar *expected_key, gchar **error)
{
  const gchar *key = kv_scanner_get_current_key(scanner);

  if (strcmp(key, expected_key) != 0)
    {
      *error = g_strdup_printf("expected key mismatch key=%s, expected=%s", key, expected_key);
      return FALSE;
    }
  return TRUE;
}

static gboolean
_expect_current_value_equals(KVScanner *scanner, const gchar *expected_value, gchar **error)
{
  const gchar *value = kv_scanner_get_current_value(scanner);

  if (strcmp(value, expected_value) != 0)
    {
      *error = g_strdup_printf("expected value mismatch value=%s, expected=%s", value, expected_value);
      return FALSE;
    }
  return TRUE;
}

static gboolean
_expect_next_key_value(KVScanner *scanner, const gchar *key, const gchar *value, gchar **error)
{
  g_assert(value);

  gboolean ok = kv_scanner_scan_next(scanner);
  if (ok)
    {
      return _expect_current_key_equals(scanner, key, error) &&
             _expect_current_value_equals(scanner, value, error);
    }
  else
    {
      *error = g_strdup_printf("kv_scanner is expected to return TRUE for scan_next(), "
                               "first unconsumed pair: [%s/%s]",
                               key, value);
    }
  return FALSE;
}

typedef struct _ScannerConfig
{
  gchar kv_separator;
  const gchar *pair_separator;
  gboolean extract_stray_words;
  KVTransformValueFunc transform_value;
} ScannerConfig;

typedef struct _KVQElement
{
  const gchar *const key;
  const gchar *const value;
  gboolean quoted;
} KVQElement;

typedef struct _KVQContainer
{
  const gsize n;
  const KVQElement arg[8];
} KVQContainer;

typedef struct _KVElement
{
  const gchar *key;
  const gchar *value;
} KVElement;

typedef struct _KVContainer
{
  gsize n;
  const KVElement arg[32];
} KVContainer;

typedef struct Testcase_t
{
  gchar *input;
  KVContainer expected;
} Testcase;

KVScanner *
create_kv_scanner(const ScannerConfig config)
{
  KVScanner *scanner = kv_scanner_new(config.kv_separator, config.pair_separator, config.extract_stray_words);
  scanner->transform_value = config.transform_value;
  return scanner;
}

#define VARARG_STRUCT(VARARG_STRUCT_cont, VARARG_STRUCT_elem, ...) \
  (const VARARG_STRUCT_cont) { \
    .n = sizeof((const VARARG_STRUCT_elem[]) { __VA_ARGS__ }) / sizeof(VARARG_STRUCT_elem), \
    .arg = {__VA_ARGS__}\
  }


static gboolean
_expect_kv_pairs(KVScanner *scanner, KVContainer args, gchar **error)
{
  for (gsize i = 0; i < args.n; i++)
    {
      if (!_expect_next_key_value(scanner, args.arg[i].key, args.arg[i].value, error))
        {
          return FALSE;
        }
    }
  return _expect_no_more_tokens(scanner, error);
}

static gboolean
_expect_kvq_triplets(KVScanner *scanner, KVQContainer args, gchar **error)
{
  for (gsize i = 0; i < args.n; i++)
    {
      if (!_expect_next_key_value(scanner, args.arg[i].key, args.arg[i].value, error))
        return FALSE;

      if (scanner->value_was_quoted != args.arg[i].quoted)
        {
          *error = g_strdup_printf("mismatch in value_was_quoted for [%s/%s]",
                                   args.arg[i].key, args.arg[i].value);
          return FALSE;
        }
    }
  return _expect_no_more_tokens(scanner, error);
}

#define INIT_KVQCONTAINER(...)  VARARG_STRUCT(KVQContainer, KVQElement, __VA_ARGS__)

#define _IMPL_EXPECT_KVQ(SCANNER_config, TEST_KV_SCAN_input, ...) \
  do { \
    KVScanner *scanner = create_kv_scanner(SCANNER_config); \
    gchar *error = NULL; \
    \
    kv_scanner_input(scanner, TEST_KV_SCAN_input);            \
    if (!_expect_kvq_triplets(scanner, INIT_KVQCONTAINER(__VA_ARGS__), &error)) \
      { \
        cr_expect(FALSE, "%s", error); \
        g_free(error);\
      } \
    kv_scanner_free(scanner); \
  } while (0)

#define _EXPECT_KVQ_TRIPLETS(...) \
  do {                    \
    _IMPL_EXPECT_KVQ(((ScannerConfig) {'='}), __VA_ARGS__); \
  } while(0)

#define INIT_KVCONTAINER(...)  VARARG_STRUCT(KVContainer, KVElement, __VA_ARGS__)

#define _IMPL_EXPECT_KV(TEST_KV_SCAN_config, TEST_KV_SCAN_input, ...) \
  do { \
    KVScanner *scanner = create_kv_scanner(TEST_KV_SCAN_config); \
    gchar *error = NULL; \
    \
    kv_scanner_input(scanner, TEST_KV_SCAN_input);            \
    if (!_expect_kv_pairs(scanner, INIT_KVCONTAINER(__VA_ARGS__), &error))  \
      { \
        cr_expect(FALSE, "%s", error); \
        g_free(error);\
      } \
    cr_expect_eq(kv_scanner_get_stray_words(scanner), NULL, \
                     "stray words are not expected but still returned");   \
    kv_scanner_free(scanner); \
  } while (0)

#define _EXPECT_KV_PAIRS(...) \
  do { \
    _IMPL_EXPECT_KV(((ScannerConfig) {'='}), __VA_ARGS__); \
  } while (0)

#define _EXPECT_KV_PAIRS_WITH_CONFIG(TEST_KV_SCAN_config, ...) \
  do {  \
    _IMPL_EXPECT_KV(TEST_KV_SCAN_config, __VA_ARGS__);    \
  } while (0)

#define _EXPECT_KV_AND_STRAY_WORDS(INPUT, STRAY, ...) \
  do { \
    KVScanner *scanner = create_kv_scanner(((ScannerConfig) {     \
      .kv_separator = '=',              \
      .extract_stray_words=TRUE})); \
    gchar *error = NULL; \
    \
    kv_scanner_input(scanner, INPUT);            \
    if (!_expect_kv_pairs(scanner, INIT_KVCONTAINER(__VA_ARGS__), &error))  \
      { \
        cr_expect(FALSE, "%s", error); \
        g_free(error);\
      } \
    cr_expect_str_eq(kv_scanner_get_stray_words(scanner), STRAY, \
                     "Stray words mismatch, value=%s, expected=%s", \
                     kv_scanner_get_stray_words(scanner), STRAY); \
    kv_scanner_free(scanner); \
  } while (0)

Test(kv_scanner, incomplete_string_returns_no_pairs)
{
  _EXPECT_KV_PAIRS("");
  _EXPECT_KV_PAIRS("f");
  _EXPECT_KV_PAIRS("fo");
  _EXPECT_KV_PAIRS("foo");
}

Test(kv_scanner, name_equals_value_returns_a_pair)
{
  _EXPECT_KV_PAIRS("foo=", { "foo", "" });
  _EXPECT_KV_PAIRS("foo=b", { "foo", "b" });
  _EXPECT_KV_PAIRS("foo=bar", { "foo", "bar" });
  _EXPECT_KV_PAIRS("foo=barbar", { "foo", "barbar" });
}

Test(kv_scanner, allowed_characters_in_a_key_are_letters_digits_dash_underscore_and_dot)
{
  _EXPECT_KV_PAIRS("FOOfoo123-_._-321oofOOF=value", {"FOOfoo123-_._-321oofOOF", "value"});
}

Test(kv_scanner, initial_stray_words_are_ignored)
{
  _EXPECT_KV_PAIRS("lorem ipsum foo=bar",
  { "foo", "bar" });

  _EXPECT_KV_PAIRS("lorem ipsum/dolor @sitamen foo=bar",
  { "foo", "bar" });

  _EXPECT_KV_PAIRS("lorem ipsum/dolor = foo=bar\"",
  { "dolor", "" },
  { "foo", "bar\"" });

  _EXPECT_KV_PAIRS("a b c=d",
  {"c", "d"});

  _EXPECT_KV_PAIRS("x *k=v",
  { "k", "v" });
}

Test(kv_scanner, non_initial_stray_words_are_added_to_the_last_value)
{
  _EXPECT_KV_PAIRS("foo=bar lorem ipsum key=value some more values",
  { "foo", "bar lorem ipsum" },
  { "key", "value some more values" });
}

Test(kv_scanner, empty_values_in_a_series_of_key_values)
{
  _EXPECT_KV_PAIRS("k= a=b c=d",
  {"k", ""},
  {"a", "b"},
  {"c", "d"});

  _EXPECT_KV_PAIRS("k=v a= c=d",
  {"k", "v"},
  {"a", ""},
  {"c", "d"});

  _EXPECT_KV_PAIRS("k=v a=b c=",
  {"k", "v"},
  {"a", "b"},
  {"c", ""});

}

Test(kv_scanner, multiple_key_values_return_multiple_pairs)
{
  _EXPECT_KV_PAIRS("key1=value1 key2=value2 key3=value3 ",
  { "key1", "value1" },
  { "key2", "value2" },
  { "key3", "value3" });
}

Test(kv_scanner, spaces_between_values_are_ignored)
{
  _EXPECT_KV_PAIRS("key1=value1    key2=value2     key3=value3 ",
  { "key1", "value1" },
  { "key2", "value2" },
  { "key3", "value3" });
}

Test(kv_scanner, comma_separated_values)
{
  _EXPECT_KV_PAIRS("key1=value1, key2=value2, key3=value3",
  { "key1", "value1" },
  { "key2", "value2" },
  { "key3", "value3" });

  /* NOTE: comma on its own is not a delimiter, as the default delimiter is ", "
   * A testcase below would test single character separator with ';' */

  _EXPECT_KV_PAIRS("key1=value1,key2=value2,key3=value3",
  { "key1", "value1,key2=value2,key3=value3" });

  /* In value2 the spaces are considered part of the value as we don't have
   * a delimiter and spaces get concatenated into the value */
  _EXPECT_KV_PAIRS("key1=value1,   key2=value2  ,    key3=value3",
  { "key1", "value1" },
  { "key2", "value2" },
  { "key3", "value3" });
}

Test(kv_scanner, tab_is_not_considered_a_separator)
{
  _EXPECT_KV_PAIRS("key1=value1\tkey2=value2 key3=value3",
  { "key1", "value1\tkey2=value2" },
  { "key3", "value3" });
  _EXPECT_KV_PAIRS("key1=value1,\tkey2=value2 key3=value3",
  { "key1", "value1,\tkey2=value2" },
  { "key3", "value3" });
  _EXPECT_KV_PAIRS("key1=value1\t key2=value2 key3=value3",
  { "key1", "value1\t" },
  { "key2", "value2" },
  { "key3", "value3" });

  _EXPECT_KV_PAIRS("k=\t",
  { "k", "\t" });
  _EXPECT_KV_PAIRS("k=,\t",
  { "k", ",\t" });
}


Test(kv_scanner, quoted_values_are_unquoted_like_c_strings)
{
  _EXPECT_KV_PAIRS("foo=\"\\\"\" bar=baz",
  { "foo", "\"" },
  { "bar", "baz"});

  _EXPECT_KV_PAIRS("foo='\"' bar=baz",
  { "foo", "\"" },
  { "bar", "baz"});

  _EXPECT_KV_PAIRS("foo=\"bar\"",
  { "foo", "bar" });

  _EXPECT_KV_PAIRS("key1=\"value1\" key2=\"value2\"",
  { "key1", "value1" },
  { "key2", "value2" });

  /* embedded quote */
  _EXPECT_KV_PAIRS("key1=\"\\\"value1\"",
  { "key1", "\"value1" });

  /* control sequences */
  _EXPECT_KV_PAIRS("key1=\"\\b \\f \\n \\r \\t \\\\\"",
  { "key1", "\b \f \n \r \t \\" });

  /* unknown backslash escape is left as is */
  _EXPECT_KV_PAIRS("key1=\"\\p\"",
  { "key1", "\\p" });

  _EXPECT_KV_PAIRS("key1='value1' key2='value2'",
  { "key1", "value1" },
  { "key2", "value2" });

  /* embedded quote */
  _EXPECT_KV_PAIRS("key1='\\'value1'",
  { "key1", "'value1" });

  /* control sequences */
  _EXPECT_KV_PAIRS("key1='\\b \\f \\n \\r \\t \\\\'",
  { "key1", "\b \f \n \r \t \\" });

  /* unknown backslash escape is left as is */
  _EXPECT_KV_PAIRS("key1='\\p'",
  { "key1", "\\p" });

  _EXPECT_KV_PAIRS("key1=\\b\\f\\n\\r\\t\\\\",
  { "key1", "\\b\\f\\n\\r\\t\\\\" });
  _EXPECT_KV_PAIRS("key1=\b\f\n\r\\",
  { "key1", "\b\f\n\r\\" });
  _EXPECT_KV_PAIRS("foo=\"bar baz\"",
  {"foo", "bar baz"});
}

Test(kv_scanner, quotes_embedded_in_an_unquoted_value_are_left_intact)
{
  _EXPECT_KV_PAIRS("foo=a \"bar baz\" ",
  {"foo", "a \"bar baz\""});
  _EXPECT_KV_PAIRS("foo=a \"bar baz",
  {"foo", "a \"bar baz"});
  _EXPECT_KV_PAIRS("foo=a \"bar baz c=d",
  {"foo", "a \"bar baz"}, {"c", "d"});
  _EXPECT_KV_PAIRS("foo=a \"bar baz\"=f c=d a",
  {"foo", "a \"bar baz\"=f"}, {"c", "d a"});

  _EXPECT_KV_PAIRS("foo=\\\"bar baz\\\"",
  {"foo", "\\\"bar baz\\\""});

}

Test(kv_scanner, separator_in_an_unquoted_value_is_taken_literally)
{
  _EXPECT_KV_PAIRS("k=a=b c=d",
  {"k", "a=b"}, {"c", "d"});
  _EXPECT_KV_PAIRS("a==b=",
  {"a", "=b="});
  _EXPECT_KV_PAIRS("a=,=b=a",
  {"a", ",=b=a"});
  _EXPECT_KV_PAIRS("a= =a",
  {"a", "=a"});
}

Test(kv_scanner, keys_without_value_separator_are_ignored)
{
  _EXPECT_KV_PAIRS("key1 key2=value2 key3 key4=value4",
  { "key2", "value2 key3" },
  { "key4", "value4" });

  _EXPECT_KV_PAIRS("key1= key2=value2 key3= key4=value4 key5= key6=value6",
  { "key1", "" },
  { "key2", "value2" },
  { "key3", "" },
  { "key4", "value4" },
  { "key5", "" },
  { "key6", "value6" });
}

Test(kv_scanner, quoted_values_are_considered_one_token_thus_space_based_concatenation_does_not_happen)
{
  _EXPECT_KV_PAIRS("key1=\"value foo\" key2=marker",
  { "key1", "value foo" },
  { "key2", "marker" });

  _EXPECT_KV_PAIRS("key1=\"value foo embedded_key=emb_value\" key2=marker",
  { "key1", "value foo embedded_key=emb_value" },
  { "key2", "marker" });

  /* embedded quote */
  _EXPECT_KV_PAIRS("key1=\"value foo\\\"\" key2=marker",
  { "key1", "value foo\"" },
  { "key2", "marker" });

  _EXPECT_KV_PAIRS("key1='value foo\\'' key2=marker",
  { "key1", "value foo'" },
  { "key2", "marker" });

  _EXPECT_KV_PAIRS("key1=\"value foo, foo2 =@,\\\"\" key2='value foo,  a='",
  { "key1", "value foo, foo2 =@,\"" },
  { "key2", "value foo,  a=" });

  /* NOTE: baz is not added to the value, it is a stray word */
  _EXPECT_KV_PAIRS("foo=\"bar\" baz c=d",
  {"foo", "bar"}, {"c", "d"});
}

static gboolean
_parse_value_by_incrementing_all_bytes(KVScanner *self)
{
  gint i;

  g_string_assign(self->decoded_value, self->value->str);
  for (i = 0; i < self->decoded_value->len; i++)
    self->decoded_value->str[i]++;
  return TRUE;
}

Test(kv_scanner, transforms_values_if_transform_value_is_set)
{
  ScannerConfig config =
  {
    .kv_separator = '=',
    .transform_value = _parse_value_by_incrementing_all_bytes
  };

  _EXPECT_KV_PAIRS_WITH_CONFIG(config,
                               "foo=\"bar\"",
  { "foo", "cbs" });
}

Test(kv_scanner, pair_separator_space_disables_space_related_heuristics)
{
  ScannerConfig config =
  {
    .kv_separator = '=',
    .pair_separator = " ",
  };

  /* v2 and v4 below goes to stray words, as the pair-separator is a space,
   * so we disable the heuristics about spaces embedded in non-quoted values
   * */

  _EXPECT_KV_PAIRS_WITH_CONFIG(config, "foo=v1 v2 bar=v3 v4",
  {"foo", "v1"},
  {"bar", "v3"});

  /* if the separator is multiple spaces, we still trim spaces at the end of
   * line (that is a partial separator).
   */
  config.pair_separator = "   ";
  _EXPECT_KV_PAIRS_WITH_CONFIG(config, "foo=v1 v2   bar=v3 v4  ",
  {"foo", "v1 v2"},
  {"bar", "v3 v4"});
}

Test(kv_scanner, pair_separator_causes_values_to_be_split_at_that_character)
{
  ScannerConfig config =
  {
    .kv_separator = '=',
    .pair_separator = ";",
  };

  _EXPECT_KV_PAIRS_WITH_CONFIG(config, "foo=bar; bar=foo;",
  {"foo", "bar"},
  {"bar", "foo"});
  _EXPECT_KV_PAIRS_WITH_CONFIG(config, "foo=bar;bar=foo;baz=foo",
  {"foo", "bar"},
  {"bar", "foo"},
  {"baz", "foo"});
  _EXPECT_KV_PAIRS_WITH_CONFIG(config, "foo=bar;bar=foo;",
  {"foo", "bar"},
  {"bar", "foo"});
  _EXPECT_KV_PAIRS_WITH_CONFIG(config, "foo=bar baz;bar=foo;",
  {"foo", "bar baz"},
  {"bar", "foo"});
  _EXPECT_KV_PAIRS_WITH_CONFIG(config, "foo=bar baz  ;bar=foo;",
  {"foo", "bar baz"},
  {"bar", "foo"});
}


Test(kv_scanner, quotation_is_stored_in_the_was_quoted_value_member)
{
  _EXPECT_KVQ_TRIPLETS("foo=\"bar\"",
  {"foo", "bar", TRUE});
  _EXPECT_KVQ_TRIPLETS("foo='bar'",
  {"foo", "bar", TRUE});
  _EXPECT_KVQ_TRIPLETS("foo=bar",
  {"foo", "bar", FALSE});
  _EXPECT_KVQ_TRIPLETS("foo='bar' k=v",
  {"foo", "bar", TRUE},
  {"k", "v", FALSE});
}

Test(kv_scanner, spaces_around_value_separator_are_ignored)
{
  ScannerConfig config=
  {
    .kv_separator=':',
  };
  _IMPL_EXPECT_KV(config, "key1: \"value1\" key2 : value2 key3 :value3 ",
  {"key1", "value1"},
  {"key2", "value2"},
  {"key3", "value3"});
}

Test(kv_scanner, value_separator_is_used_to_separate_key_from_value)
{
  ScannerConfig config=
  {
    .kv_separator = ':',
  };

  _EXPECT_KV_PAIRS_WITH_CONFIG(config,
                               "key1:value1 key2:value2 key3:value3 ",
  { "key1", "value1" },
  { "key2", "value2" },
  { "key3", "value3" });
}

Test(kv_scanner, invalid_value_encoding_is_copied_literally)
{
  _EXPECT_KV_PAIRS("k=\xc3",
  { "k", "\xc3" });
  _EXPECT_KV_PAIRS("k=\xc3v",
  { "k", "\xc3v" });
  _EXPECT_KV_PAIRS("k=\xff",
  { "k", "\xff" });
  _EXPECT_KV_PAIRS("k=\xffv",
  { "k", "\xffv" });

  _EXPECT_KV_PAIRS("k=\"\xc3\"",
  { "k", "\xc3" });
  _EXPECT_KV_PAIRS("k=\"\xc3v\"",
  { "k", "\xc3v" });
  _EXPECT_KV_PAIRS("k=\"\xff\"",
  { "k", "\xff" });
  _EXPECT_KV_PAIRS(" k=\"\xffv\"",
  { "k", "\xffv" });
}

Test(kv_scanner, separator_in_key)
{
  ScannerConfig config=
  {
    .kv_separator = '-',
  };

  _EXPECT_KV_PAIRS_WITH_CONFIG(config, "k-v",  { "k", "v" });
  _EXPECT_KV_PAIRS_WITH_CONFIG(config, "k--v",  { "k", "-v" });
  _EXPECT_KV_PAIRS_WITH_CONFIG(config, "---",  { "-", "-" });
}

Test(kv_scanner, empty_keys)
{
  _EXPECT_KV_PAIRS("=v");
  _EXPECT_KV_PAIRS("k*=v");
  _EXPECT_KV_PAIRS("=");
  _EXPECT_KV_PAIRS("==");
  _EXPECT_KV_PAIRS("===");
  _EXPECT_KV_PAIRS(" =");
  _EXPECT_KV_PAIRS(" ==");
  _EXPECT_KV_PAIRS(" ===");
  _EXPECT_KV_PAIRS(" = =");
  _EXPECT_KV_PAIRS(" ==k=", { "k", "" });
  _EXPECT_KV_PAIRS(" = =k=", { "k", "" });
  _EXPECT_KV_PAIRS(" =k=",  { "k", "" });
  _EXPECT_KV_PAIRS(" =k=v", { "k", "v" });
  _EXPECT_KV_PAIRS(" ==k=v", { "k", "v" });
  _EXPECT_KV_PAIRS(" =k=v=w", { "k", "v=w" });
}

Test(kv_scanner, unclosed_quotes)
{
  _EXPECT_KV_PAIRS("k=\"a",  { "k", "\"a" });
  _EXPECT_KV_PAIRS("k=\\",   { "k", "\\" });
  _EXPECT_KV_PAIRS("k=\"\\", { "k", "\"\\" });

  _EXPECT_KV_PAIRS("k='a",   { "k", "'a" });
  _EXPECT_KV_PAIRS("k='\\",  { "k", "'\\" });

  /* backslash is not special if not within quotes */
  _EXPECT_KV_PAIRS("k=\\",   {"k", "\\"});
  _EXPECT_KV_PAIRS("foo=bar\"",
  {"foo", "bar\""});
  _EXPECT_KV_PAIRS("foo='bar",
  {"foo", "'bar"});
}


Test(kv_scanner, comma_separator)
{
  _EXPECT_KV_PAIRS(", k=v",  { "k", "v" });
  _EXPECT_KV_PAIRS(",k=v",   { "k", "v" });
  _EXPECT_KV_PAIRS("k=v,",   { "k", "v," });
  _EXPECT_KV_PAIRS("k=v, ",  { "k", "v" });
}

Test(kv_scanner, multiple_separators)
{
  _EXPECT_KV_PAIRS("k==", { "k", "=" });
  _EXPECT_KV_PAIRS("k===", { "k", "==" });
  _EXPECT_KV_PAIRS("k===a", {"k", "==a"});
  _EXPECT_KV_PAIRS("k===a=b", {"k", "==a=b"});
}

Test(kv_scanner, keys_only_use_a_restricted_set_of_characters)
{
  _EXPECT_KV_PAIRS("k-j=v", { "k-j", "v" });
  _EXPECT_KV_PAIRS("0=v", { "0", "v" });
  _EXPECT_KV_PAIRS("_=v", { "_", "v" });
  _EXPECT_KV_PAIRS(":=v");
  _EXPECT_KV_PAIRS(":=");
  _EXPECT_KV_PAIRS("Z=v", { "Z", "v" });

  /* no value, as the key is not in [a-zA-Z0-9-_] */
  _EXPECT_KV_PAIRS("á=v");
  _EXPECT_KV_PAIRS("*k=v",
  { "k", "v" });
}

Test(kv_scanner, unquoted_values_can_have_embedded_control_characters)
{
  _EXPECT_KV_PAIRS("k1=\\b\\f\\n\\r\\t\\\\",
  {"k1", "\\b\\f\\n\\r\\t\\\\"});
  _EXPECT_KV_PAIRS("k1=\b\f\n\r\\",
  {"k1", "\b\f\n\r\\"});
}

Test(kv_scanner, spaces_are_trimmed_between_key_and_separator)
{
  _EXPECT_KV_PAIRS("foo =bar",
  {"foo", "bar"});
  _EXPECT_KV_PAIRS("foo= bar",
  {"foo", "bar"});
}

Test(kv_scanner, space_is_only_a_delimiter_if_a_key_follows)
{
  _EXPECT_KV_PAIRS("foo=bar ggg",
  {"foo", "bar ggg"});

  _EXPECT_KV_PAIRS("foo=bar ggg baz=ez",
  {"foo", "bar ggg"},
  {"baz", "ez"});
}


Test(kv_scanner, spaces_are_trimmed_from_key_names)
{
  _EXPECT_KV_PAIRS(" foo =bar ggg baz=ez",
  {"foo", "bar ggg"},
  {"baz", "ez"});

  _EXPECT_KV_PAIRS("foo =bar ggg baz=ez",
  {"foo", "bar ggg"},
  {"baz", "ez"});

  _EXPECT_KV_PAIRS(" foo=bar ggg baz=ez",
  {"foo", "bar ggg"},
  {"baz", "ez"});

  _EXPECT_KV_PAIRS("foo =  bar ggg baz   =   ez",
  {"foo", "bar ggg"},
  {"baz", "ez"});

  _EXPECT_KV_PAIRS("k===  a",
  {"k", "==  a"});

}

Test(kv_scanner, initial_spaces_are_trimmed_from_values)
{
  _EXPECT_KV_PAIRS(" k= b",
  {"k", "b"});

}

Test(kv_scanner, stray_words_are_stored)
{
  _EXPECT_KV_AND_STRAY_WORDS("foo=bar", "", {"foo", "bar"});
  _EXPECT_KV_AND_STRAY_WORDS("alma foo=bar", "alma", {"foo", "bar"});
  _EXPECT_KV_AND_STRAY_WORDS("alma foo=bar, korte bar=foo",
                             "alma,korte",
  {"foo", "bar"},
  {"bar", "foo"});

  _EXPECT_KV_AND_STRAY_WORDS("alma foo=bar, korte bar=foo, narancs",
                             "alma,korte,narancs",
  {"foo", "bar"},
  {"bar", "foo"});
}

Test(kv_scanner, key_buffer_underrun)
{
  const gchar *buffer = "ab=v";
  const gchar *input = buffer + 2;
  _EXPECT_KV_PAIRS(input);
}

static Testcase *
_provide_cases_for_performance_test_nothing_to_parse(void)
{
  Testcase tc[] =
  {
    {
      .input = "Reducing the compressed framebuffer size. This may lead to less power savings than a non-reduced-size. \
Try to increase stolen memory size if available in BIOS.",
      .expected = INIT_KVCONTAINER(),
    },
    {
      .input = "interrupt took too long (3136 > 3127), lowering kernel.perf_event_max_sample_rate to 63750",
      .expected = INIT_KVCONTAINER(),
    },
    {
      .input = "Linux version 4.6.3-040603-generic (kernel@gomeisa) (gcc version 5.4.0 20160609 (Ubuntu 5.4.0-4ubuntu1) ) \
#201606241434 SMP Fri Jun 24 18:36:33 UTC 2016",
      .expected = INIT_KVCONTAINER(),
    },
    {}
  };

  return g_memdup(tc, sizeof(tc));
}

static Testcase *
_provide_cases_for_performance_test_parse_long_msg(void)
{
  Testcase tc[] =
  {
    {
      .input = "PF: filter/forward DROP \
      IN=15dd205a6ac8b0c80ab3bcdcc5649c9c830074cdbdc094ff1d79f20f17c56843 \
      OUT=980816f36b77e58d342de41f85854376d10cf9bf33aa1934e129ffd77ddc833d \
      SRC=cc8177fc0c8681d3d5d2a42bc1ed86990f773589592fa3100c23fae445f8a260 \
      DST=5fee25396500fc798e10b4dcb0b3fb315618ff11843be59978c0d5b41cd9f12c \
      LEN=71ee45a3c0db9a9865f7313dd3372cf60dca6479d46261f3542eb9346e4a04d6 \
      TOS=c4dd67368286d02d62bdaa7a775b7594765d5210c9ad20cc3c24148d493353d7 \
      PREC=c4dd67368286d02d62bdaa7a775b7594765d5210c9ad20cc3c24148d493353d7 \
      TTL=da4ea2a5506f2693eae190d9360a1f31793c98a1adade51d93533a6f520ace1c \
      ID=242a9377518dd1afaf021b2d0bfe6484e3fe48a878152f76dec99a396160022c \
      PROTO=dc4030f9688d6e67dfc4c5f8f7afcbdbf5c30de866d8a3c6e1dd038768ab91c3 \
      SPT=1e7996c7b0181429bba237ac2799ee5edc31aca2d5d90c39a48f9e9a3d4078bd \
      DPT=ca902d4a8acbdea132ada81a004081f51c5c9279d409cee414de5a39a139fab6 \
      LEN=c2356069e9d1e79ca924378153cfbbfb4d4416b1f99d41a2940bfdb66c5319db",
      .expected = INIT_KVCONTAINER(
      {"IN", "15dd205a6ac8b0c80ab3bcdcc5649c9c830074cdbdc094ff1d79f20f17c56843"},
      {"OUT", "980816f36b77e58d342de41f85854376d10cf9bf33aa1934e129ffd77ddc833d"},
      {"SRC", "cc8177fc0c8681d3d5d2a42bc1ed86990f773589592fa3100c23fae445f8a260"},
      {"DST", "5fee25396500fc798e10b4dcb0b3fb315618ff11843be59978c0d5b41cd9f12c"},
      {"LEN", "71ee45a3c0db9a9865f7313dd3372cf60dca6479d46261f3542eb9346e4a04d6"},
      {"TOS", "c4dd67368286d02d62bdaa7a775b7594765d5210c9ad20cc3c24148d493353d7"},
      {"PREC", "c4dd67368286d02d62bdaa7a775b7594765d5210c9ad20cc3c24148d493353d7"},
      {"TTL", "da4ea2a5506f2693eae190d9360a1f31793c98a1adade51d93533a6f520ace1c"},
      {"ID", "242a9377518dd1afaf021b2d0bfe6484e3fe48a878152f76dec99a396160022c"},
      {"PROTO", "dc4030f9688d6e67dfc4c5f8f7afcbdbf5c30de866d8a3c6e1dd038768ab91c3"},
      {"SPT", "1e7996c7b0181429bba237ac2799ee5edc31aca2d5d90c39a48f9e9a3d4078bd"},
      {"DPT", "ca902d4a8acbdea132ada81a004081f51c5c9279d409cee414de5a39a139fab6"},
      {"LEN", "c2356069e9d1e79ca924378153cfbbfb4d4416b1f99d41a2940bfdb66c5319db"}),
    },
    {
      .input = "PF: filter/forward DROP \
      IN='15dd205a6ac8b0c80ab3bcdcc5649c9c830074cdbdc094ff1d79f20f17c56843' \
      OUT='980816f36b77e58d342de41f85854376d10cf9bf33aa1934e129ffd77ddc833d' \
      SRC='cc8177fc0c8681d3d5d2a42bc1ed86990f773589592fa3100c23fae445f8a260' \
      DST='5fee25396500fc798e10b4dcb0b3fb315618ff11843be59978c0d5b41cd9f12c' \
      LEN='71ee45a3c0db9a9865f7313dd3372cf60dca6479d46261f3542eb9346e4a04d6' \
      TOS='c4dd67368286d02d62bdaa7a775b7594765d5210c9ad20cc3c24148d493353d7' \
      PREC='c4dd67368286d02d62bdaa7a775b7594765d5210c9ad20cc3c24148d493353d7' \
      TTL='da4ea2a5506f2693eae190d9360a1f31793c98a1adade51d93533a6f520ace1c' \
      ID='242a9377518dd1afaf021b2d0bfe6484e3fe48a878152f76dec99a396160022c' \
      PROTO='dc4030f9688d6e67dfc4c5f8f7afcbdbf5c30de866d8a3c6e1dd038768ab91c3' \
      SPT='1e7996c7b0181429bba237ac2799ee5edc31aca2d5d90c39a48f9e9a3d4078bd' \
      DPT='ca902d4a8acbdea132ada81a004081f51c5c9279d409cee414de5a39a139fab6' \
      LEN='c2356069e9d1e79ca924378153cfbbfb4d4416b1f99d41a2940bfdb66c5319db'",
      .expected = INIT_KVCONTAINER(
      {"IN", "15dd205a6ac8b0c80ab3bcdcc5649c9c830074cdbdc094ff1d79f20f17c56843"},
      {"OUT", "980816f36b77e58d342de41f85854376d10cf9bf33aa1934e129ffd77ddc833d"},
      {"SRC", "cc8177fc0c8681d3d5d2a42bc1ed86990f773589592fa3100c23fae445f8a260"},
      {"DST", "5fee25396500fc798e10b4dcb0b3fb315618ff11843be59978c0d5b41cd9f12c"},
      {"LEN", "71ee45a3c0db9a9865f7313dd3372cf60dca6479d46261f3542eb9346e4a04d6"},
      {"TOS", "c4dd67368286d02d62bdaa7a775b7594765d5210c9ad20cc3c24148d493353d7"},
      {"PREC", "c4dd67368286d02d62bdaa7a775b7594765d5210c9ad20cc3c24148d493353d7"},
      {"TTL", "da4ea2a5506f2693eae190d9360a1f31793c98a1adade51d93533a6f520ace1c"},
      {"ID", "242a9377518dd1afaf021b2d0bfe6484e3fe48a878152f76dec99a396160022c"},
      {"PROTO", "dc4030f9688d6e67dfc4c5f8f7afcbdbf5c30de866d8a3c6e1dd038768ab91c3"},
      {"SPT", "1e7996c7b0181429bba237ac2799ee5edc31aca2d5d90c39a48f9e9a3d4078bd"},
      {"DPT", "ca902d4a8acbdea132ada81a004081f51c5c9279d409cee414de5a39a139fab6"},
      {"LEN", "c2356069e9d1e79ca924378153cfbbfb4d4416b1f99d41a2940bfdb66c5319db"}),
    },
    {
      .input = "fw=108.53.156.38 pri=6 c=262144 m=98 msg=\"Connection Opened\" f=2 sess=\"None\" \
      n=16351474 src=10.0.5.200:57719:X0:MOGWAI dst=71.250.0.14:53:X1 dstMac=f8:c0:01:73:c7:c1 proto=udp/dns sent=66",
      .expected = INIT_KVCONTAINER(
      {"fw", "108.53.156.38"},
      {"pri", "6"},
      {"c", "262144"},
      {"m", "98"},
      {"msg", "Connection Opened"},
      {"f", "2"},
      {"sess", "None"},
      {"n", "16351474"},
      {"src", "10.0.5.200:57719:X0:MOGWAI"},
      {"dst", "71.250.0.14:53:X1"},
      {"dstMac", "f8:c0:01:73:c7:c1"},
      {"proto", "udp/dns"},
      {"sent", "66"}),
    },
    {
      .input = "sn=C0EAE484E43E time=\"2016-07-08 13:42:58\" fw=132.237.143.192 pri=5 c=4 m=16 msg=\"Web site access allowed\" \
      app=11 sess=\"Auto\" n=5086 usr=\"DEMO\\primarystudent\" src=10.2.3.64:50682:X2-V3023 dst=157.55.240.220:443:X1 \
      srcMac=00:50:56:8e:55:8e dstMac=c0:ea:e4:84:e4:40 proto=tcp/https dstname=sls.update.microsoft.com arg= code=27 \
      Category=\"Information Technology/Computers\" fw_action=\"process\"",
      .expected = INIT_KVCONTAINER(
      {"sn", "C0EAE484E43E"},
      {"time", "2016-07-08 13:42:58"},
      {"fw", "132.237.143.192"},
      {"pri", "5"},
      {"c", "4"},
      {"m", "16"},
      {"msg", "Web site access allowed"},
      {"app", "11"},
      {"sess", "Auto"},
      {"n", "5086"},
      {"usr", "DEMO\\primarystudent"},
      {"src", "10.2.3.64:50682:X2-V3023"},
      {"dst", "157.55.240.220:443:X1"},
      {"srcMac", "00:50:56:8e:55:8e"},
      {"dstMac", "c0:ea:e4:84:e4:40"},
      {"proto", "tcp/https"},
      {"dstname", "sls.update.microsoft.com"},
      {"arg", ""},
      {"code", "27"},
      {"Category", "Information Technology/Computers"},
      {"fw_action", "process"}),
    },
    {
      .input = "IN=em1 OUT= MAC=a2:be:d2:ab:11:af:e2:f2:00:00 SRC=192.168.2.115 DST=192.168.1.23 "
      "LEN=52 TOS=0x00 PREC=0x00 TTL=127 ID=9434 DF PROTO=TCP SPT=58428 DPT=443 WINDOW=8192 "
      "RES=0x00 SYN URGP=0",
      .expected = INIT_KVCONTAINER(
      {"IN", "em1"},
      {"OUT", ""},
      {"MAC", "a2:be:d2:ab:11:af:e2:f2:00:00"},
      {"SRC", "192.168.2.115"},
      {"DST", "192.168.1.23"},
      {"LEN", "52"},
      {"TOS", "0x00"},
      {"PREC", "0x00"},
      {"TTL", "127"},
      {"ID", "9434 DF"},
      {"PROTO", "TCP"},
      {"SPT", "58428"},
      {"DPT", "443"},
      {"WINDOW", "8192"},
      {"RES", "0x00 SYN"},
      {"URGP", "0"},
      ),
    },
    {}
  };
  return g_memdup(tc, sizeof(tc));
}

#define ITERATION_NUMBER 100000

static void
_test_performance(Testcase *tcs, gchar *title)
{
  const Testcase *tc;
  gint iteration_index = 0;

  if (title)
    {
      printf("Performance test: %s\n", title);
    }

  for (tc = tcs; tc->input; tc++)
    {
      KVScanner scanner;

      start_stopwatch();
      for (iteration_index = 0; iteration_index < ITERATION_NUMBER; iteration_index++)
        {
          gchar *error = NULL;

          kv_scanner_init(&scanner, '=', ",", FALSE);
          kv_scanner_input(&scanner, tc->input);
          if (!_expect_kv_pairs(&scanner, tc->expected, &error))
            {
              cr_expect(FALSE, "%s", error);
              g_free(error);
            }
          kv_scanner_deinit(&scanner);
        }
      stop_stopwatch_and_display_result(iteration_index, "%.64s...",
                                        tc->input);
    }
  g_free(tcs);
}

Test(kv_scanner, performance_tests)
{
  _test_performance(_provide_cases_for_performance_test_nothing_to_parse(), "Nothing to parse in the message");
  _test_performance(_provide_cases_for_performance_test_parse_long_msg(), "Parse long strings");
}
