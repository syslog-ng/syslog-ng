/*
 * Copyright (c) 2015-2016 Balabit
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
 */
#include <criterion/criterion.h>
#include <stdio.h>
#include "stopwatch.h"

#include "kv-scanner-generic.h"
#include "kv-scanner-simple.h"

static void
_expect_no_more_tokens(KVScanner *scanner)
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
      while (scanner->scan_next(scanner));

      cr_expect(FALSE, "%s", msg->str);
      g_string_free(msg, TRUE);
    }
}

static void
_expect_current_key_equals(KVScanner *scanner, const gchar *expected_key)
{
  const gchar *key = kv_scanner_get_current_key(scanner);

  cr_expect_str_eq(key, expected_key, "expected key mismatch key=%s, expected=%s", key, expected_key);
}

static void
_expect_current_value_equals(KVScanner *scanner, const gchar *expected_value)
{
  const gchar *value = kv_scanner_get_current_value(scanner);

  cr_expect_str_eq(value, expected_value, "expected value mismatch value=%s, expected=%s", value, expected_value);
}

static gboolean
_expect_next_key_value(KVScanner *scanner, const gchar *key, const gchar *value)
{
  g_assert(value);

  gboolean ok = scanner->scan_next(scanner);
  if (ok)
    {
      _expect_current_key_equals(scanner, key);
      _expect_current_value_equals(scanner, value);
      return TRUE;
    }
  else
    {
      cr_expect(ok, "kv_scanner is expected to return TRUE for scan_next(), "
                  "first unconsumed pair: [%s/%s]",
                  key, value);
      return FALSE;
    }
}

typedef struct _ScannerConfig
{
  gchar kv_separator;
  gboolean allow_pair_separator_in_value;
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
  gint line;
  const gchar *function;
  ScannerConfig config[5];
  gchar *input;
  KVContainer expected;
} Testcase;

KVScanner *
create_kv_scanner(const ScannerConfig config)
{
  KVScanner *scanner = (config.allow_pair_separator_in_value ?
          kv_scanner_generic_new(config.kv_separator, NULL) :
          kv_scanner_simple_new(config.kv_separator, NULL));

  scanner->transform_value = config.transform_value;

  KVScanner *clone = kv_scanner_clone(scanner);
  kv_scanner_free(scanner);
  return clone;
}

#define VARARG_STRUCT(VARARG_STRUCT_cont, VARARG_STRUCT_elem, ...) \
  (const VARARG_STRUCT_cont) { \
    .n = sizeof((const VARARG_STRUCT_elem[]) { __VA_ARGS__ }) / sizeof(VARARG_STRUCT_elem), \
    .arg = {__VA_ARGS__}\
  }


static void
_expect_kv_pairs(KVScanner *scanner, const gchar *input, KVContainer args)
{
  g_assert(input);
  for (gsize i = 0; i < args.n; i++)
    {
      if (!_expect_next_key_value(scanner, args.arg[i].key, args.arg[i].value))
        break;
    }
  _expect_no_more_tokens(scanner);
}

static void
_expect_kvq_triplets(KVScanner *scanner, const gchar *input, KVQContainer args)
{
  g_assert(input);
  for (gsize i = 0; i < args.n; i++)
    {
      if (!_expect_next_key_value(scanner, args.arg[i].key, args.arg[i].value))
        break;
      cr_expect(scanner->value_was_quoted == args.arg[i].quoted,
                         "mismatch in value_was_quoted for [%s/%s]",
                         args.arg[i].key, args.arg[i].value);
    }
  _expect_no_more_tokens(scanner);
}

#define INIT_KVQCONTAINER(...)  VARARG_STRUCT(KVQContainer, KVQElement, __VA_ARGS__)

#define _IMPL_EXPECT_KVQ(SCANNER_config, TEST_KV_SCAN_input, ...) \
  do { \
    KVScanner *scanner = create_kv_scanner(SCANNER_config); \
    \
    kv_scanner_input(scanner, TEST_KV_SCAN_input);						\
    _expect_kvq_triplets(scanner, TEST_KV_SCAN_input, INIT_KVQCONTAINER(__VA_ARGS__)); \
    kv_scanner_free(scanner); \
  } while (0)

#define _EXPECT_KVQ_TRIPLETS(...) \
  do {										\
    _IMPL_EXPECT_KVQ(((ScannerConfig) {'=', FALSE}), __VA_ARGS__);	\
    _IMPL_EXPECT_KVQ(((ScannerConfig) {'=', TRUE}), __VA_ARGS__);	\
  } while(0)

#define INIT_KVCONTAINER(...)  VARARG_STRUCT(KVContainer, KVElement, __VA_ARGS__)

#define _IMPL_EXPECT_KV(TEST_KV_SCAN_config, TEST_KV_SCAN_input, ...) \
  do { \
    KVScanner *scanner = create_kv_scanner(TEST_KV_SCAN_config); \
    \
    kv_scanner_input(scanner, TEST_KV_SCAN_input);						\
    _expect_kv_pairs(scanner, TEST_KV_SCAN_input, INIT_KVCONTAINER(__VA_ARGS__)); \
    kv_scanner_free(scanner); \
  } while (0)


#define _EXPECT_KV_PAIRS_NOSPACE(...) \
  do { \
    _IMPL_EXPECT_KV(((ScannerConfig) {'=', FALSE}), __VA_ARGS__); \
  } while (0)

#define _EXPECT_KV_PAIRS_WITHSPACE(...) \
  do { \
    _IMPL_EXPECT_KV(((ScannerConfig) {'=', TRUE}), __VA_ARGS__); \
  } while (0)

#define _EXPECT_KV_PAIRS(...) \
  do { \
    _IMPL_EXPECT_KV(((ScannerConfig) {'=', FALSE}), __VA_ARGS__); \
    _IMPL_EXPECT_KV(((ScannerConfig) {'=', TRUE}), __VA_ARGS__); \
  } while (0)

#define _EXPECT_KV_PAIRS_WITH_CONFIG(TEST_KV_SCAN_config, ...) \
  do {	\
    TEST_KV_SCAN_config.allow_pair_separator_in_value = FALSE;		\
    _IMPL_EXPECT_KV(TEST_KV_SCAN_config, __VA_ARGS__);		\
    TEST_KV_SCAN_config.allow_pair_separator_in_value = TRUE;		\
    _IMPL_EXPECT_KV(TEST_KV_SCAN_config, __VA_ARGS__);		\
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

Test(kv_scanner, stray_words_are_ignored)
{
  _EXPECT_KV_PAIRS("lorem ipsum foo=bar",
               { "foo", "bar" });
  _EXPECT_KV_PAIRS("lorem ipsum/dolor @sitamen foo=bar",
               { "foo", "bar" });

  /* FIXME: this difference can be explained by the allow-space option */
  _EXPECT_KV_PAIRS_WITHSPACE("lorem ipsum/dolor = foo=bar\"",
               { "dolor", "" },
               { "foo", "bar\"" });
  _EXPECT_KV_PAIRS_NOSPACE("lorem ipsum/dolor = foo=bar\"",
               { "foo", "bar" });

  /* FIXME: this difference can be explained by the allow-space option */

  _EXPECT_KV_PAIRS_NOSPACE("foo=bar lorem ipsum key=value some more values",
               { "foo", "bar" },
               { "key", "value" });

  _EXPECT_KV_PAIRS_WITHSPACE("foo=bar lorem ipsum key=value some more values",
               { "foo", "bar lorem ipsum" },
               { "key", "value some more values" });

  _EXPECT_KV_PAIRS("*k=v",
               { "k", "v" });
  _EXPECT_KV_PAIRS("x *k=v",
               { "k", "v" });
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
  /* FIXME: this should be fixed up by adding ", " separator to the generic parser */
  _EXPECT_KV_PAIRS_NOSPACE("key1=value1, key2=value2, key3=value3",
               { "key1", "value1" },
               { "key2", "value2" },
               { "key3", "value3" });
}

Test(kv_scanner, comma_separated_values_and_multiple_spaces)
{
  /* FIXME: this should be fixed up by adding ", " separator to the generic parser */
  _EXPECT_KV_PAIRS_NOSPACE("key1=value1,   key2=value2  ,    key3=value3",
               { "key1", "value1" },
               { "key2", "value2" },
               { "key3", "value3" });
}

Test(kv_scanner, comma_without_space_is_not_a_separator)
{
  _EXPECT_KV_PAIRS("key1=value1,key2=value2,key3=value3",
               { "key1", "value1,key2=value2,key3=value3" });
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
  _EXPECT_KV_PAIRS("foo=\"bar\"",
               { "foo", "bar" });

  _EXPECT_KV_PAIRS("key1=\"value1\" key2=\"value2\"",
               { "key1", "value1" },
               { "key2", "value2" });

  /* embedded quote */
  /* FIXME: this should be fixed up by adding backslash support to the generic parser! */
  _EXPECT_KV_PAIRS_NOSPACE("key1=\"\\\"value1\"",
               { "key1", "\"value1" });

  /* control sequences */
  _EXPECT_KV_PAIRS_NOSPACE("key1=\"\\b \\f \\n \\r \\t \\\\\"",
               { "key1", "\b \f \n \r \t \\" });

  /* unknown backslash escape is left as is */
  _EXPECT_KV_PAIRS_NOSPACE("key1=\"\\p\"",
               { "key1", "\\p" });

  _EXPECT_KV_PAIRS("key1='value1' key2='value2'",
               { "key1", "value1" },
               { "key2", "value2" });

  /* embedded quote */
  /* FIXME: this should be fixed up by adding backslash support to the generic parser! */
  _EXPECT_KV_PAIRS_NOSPACE("key1='\\'value1'",
               { "key1", "'value1" });

  /* control sequences */
  _EXPECT_KV_PAIRS_NOSPACE("key1='\\b \\f \\n \\r \\t \\\\'",
               { "key1", "\b \f \n \r \t \\" });

  /* unknown backslash escape is left as is */
  _EXPECT_KV_PAIRS_NOSPACE("key1='\\p'",
               { "key1", "\\p" });

  _EXPECT_KV_PAIRS("key1=\\b\\f\\n\\r\\t\\\\",
               { "key1", "\\b\\f\\n\\r\\t\\\\" });
  _EXPECT_KV_PAIRS("key1=\b\f\n\r\\",
               { "key1", "\b\f\n\r\\" });
}

Test(kv_scanner, keys_without_value_separator_are_ignored)
{
  /* FIXME: different is explained using space in value */
  _EXPECT_KV_PAIRS_NOSPACE("key1 key2=value2 key3 key4=value4",
               { "key2", "value2" },
               { "key4", "value4" });
  _EXPECT_KV_PAIRS_WITHSPACE("key1 key2=value2 key3 key4=value4",
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

Test(kv_scanner, quoted_values_with_special_characters)
{
  _EXPECT_KV_PAIRS("key1='value foo, foo2 =@,\"' key2='value foo,  a='",
               { "key1", "value foo, foo2 =@,\"" },
               { "key2", "value foo,  a=" });
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

Test(kv_scanner, transforms_values_if_parse_value_is_set)
{
  ScannerConfig config = {
    .kv_separator = '=',
    .transform_value = _parse_value_by_incrementing_all_bytes
  };

  _EXPECT_KV_PAIRS_WITH_CONFIG(config,
                           "foo=\"bar\"",
                           { "foo", "cbs" });
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

Test(kv_scanner, value_separator_with_whitespaces_around)
{
  ScannerConfig config= {
    ':',
    FALSE
  };
  _IMPL_EXPECT_KV(config, "key1: value1 key2 : value2 key3 :value3 ",
                           {"key1", "" });
}

Test(kv_scanner, value_separator_is_used_to_separate_key_from_value)
{
  ScannerConfig config= {
    .kv_separator = ':',
  };

  _EXPECT_KV_PAIRS_WITH_CONFIG(config,
                           "key1:value1 key2:value2 key3:value3 ",
                           { "key1", "value1" },
                           { "key2", "value2" },
                           { "key3", "value3" });
}

Test(kv_scanner, invalid_encoding)
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
  ScannerConfig config= {
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
  /* FIXME: NOSPACE & WITHSPACE are different wrt unclosed quote characters */
  _EXPECT_KV_PAIRS_NOSPACE("k=\"a",  { "k", "a" });
  _EXPECT_KV_PAIRS_NOSPACE("k=\\",   { "k", "\\" });
  _EXPECT_KV_PAIRS_NOSPACE("k=\"\\", { "k", "" });

  _EXPECT_KV_PAIRS_NOSPACE("k='a",   { "k", "a" });
  _EXPECT_KV_PAIRS_NOSPACE("k='\\",  { "k", "" });
}


Test(kv_scanner, comma_separator)
{
  _EXPECT_KV_PAIRS(", k=v",  { "k", "v" });
  _EXPECT_KV_PAIRS(",k=v",   { "k", "v" });
  _EXPECT_KV_PAIRS("k=v,",   { "k", "v," });
  /* FIXME: separator can be a ", " */
  _EXPECT_KV_PAIRS_NOSPACE("k=v, ",  { "k", "v" });
}

Test(kv_scanner, multiple_separators)
{
  _EXPECT_KV_PAIRS("k==", { "k", "=" });
  _EXPECT_KV_PAIRS("k===", { "k", "==" });
}

Test(kv_scanner, key_charset)
{
  _EXPECT_KV_PAIRS("k-j=v", { "k-j", "v" });
  _EXPECT_KV_PAIRS("0=v", { "0", "v" });
  _EXPECT_KV_PAIRS("_=v", { "_", "v" });
  _EXPECT_KV_PAIRS(":=v");
  _EXPECT_KV_PAIRS("Z=v", { "Z", "v" });

  /* no value, as the key is not in [a-zA-Z0-9-_] */
  _EXPECT_KV_PAIRS("á=v");
}

Test(kv_scanner, key_buffer_underrun)
{
  const gchar *buffer = "ab=v";
  const gchar *input = buffer + 2;
  _EXPECT_KV_PAIRS(input);
}

#define TEST_KV_SCAN_ARRAY(SCANNER_config, TEST_KV_SCAN_input, TEST_KV_SCAN_expected) \
  do { \
    KVScanner *scanner = create_kv_scanner(SCANNER_config); \
    \
    kv_scanner_input(scanner, TEST_KV_SCAN_input);						\
    _expect_kv_pairs(scanner, TEST_KV_SCAN_input, TEST_KV_SCAN_expected); \
    kv_scanner_free(scanner); \
  } while (0)

#define DEFAULT_CONFIG ((const ScannerConfig) {.kv_separator='=', .allow_pair_separator_in_value=FALSE})
#define COLON_CONFIG ((const ScannerConfig) {.kv_separator=':', .allow_pair_separator_in_value=FALSE})
#define DASH_CONFIG ((const ScannerConfig) {.kv_separator='-', .allow_pair_separator_in_value=FALSE})
#define SPACE_HANDLING_CONFIG ((const ScannerConfig) {.kv_separator='=', .allow_pair_separator_in_value=TRUE})

#define TC_HEAD .line=__LINE__, .function=__FUNCTION__


#define CONFIG_LIST(...) { __VA_ARGS__, {} }

static Testcase *
_provide_common_cases(void)
{
  Testcase tc[] =
  {
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "foo=bar",
      .expected = INIT_KVCONTAINER({ "foo", "bar" }),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "k-j=v",
      .expected = INIT_KVCONTAINER({ "k-j", "v" }),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "0=v",
      .expected = INIT_KVCONTAINER({"0", "v"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "_=v",
      .expected = INIT_KVCONTAINER({"_", "v"})
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "Z=v",
      .expected = INIT_KVCONTAINER({"Z", "v"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "k==",
      .expected = INIT_KVCONTAINER({"k", "="}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "k===",
      .expected = INIT_KVCONTAINER({"k", "=="}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "k===a",
      .expected = INIT_KVCONTAINER({"k", "==a"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "k===a=b",
      .expected = INIT_KVCONTAINER({"k", "==a=b"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = " ==k=",
      .expected = INIT_KVCONTAINER({"k", ""}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = " = =k=",
      .expected = INIT_KVCONTAINER({"k", ""}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = " =k=",
      .expected = INIT_KVCONTAINER({"k", ""}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = " =k=v",
      .expected = INIT_KVCONTAINER({"k", "v"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = " ==k=v",
      .expected = INIT_KVCONTAINER({"k", "v"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "k=\xc3",
      .expected = INIT_KVCONTAINER({"k", "\xc3"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "k=\xc3v",
      .expected = INIT_KVCONTAINER({"k", "\xc3v"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "k=\xff",
      .expected = INIT_KVCONTAINER({"k", "\xff"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "k=\xffv",
      .expected = INIT_KVCONTAINER({"k", "\xffv"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "foo=",
      .expected = INIT_KVCONTAINER({"foo", ""}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "foo=b",
      .expected = INIT_KVCONTAINER({"foo", "b"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "lorem ipsum foo=bar",
      .expected = INIT_KVCONTAINER({"foo", "bar"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "lorem ipsum/dolor @sitamen foo=bar",
      .expected = INIT_KVCONTAINER({"foo", "bar"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "*k=v",
      .expected = INIT_KVCONTAINER({"k", "v"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "x *k=v",
      .expected = INIT_KVCONTAINER({"k", "v"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "k1=v1 k2=v2 k3=v3",
      .expected = INIT_KVCONTAINER({"k1", "v1"}, {"k2", "v2"}, {"k3", "v3"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "k= a=b c=d",
      .expected = INIT_KVCONTAINER({"k", ""}, {"a", "b"}, {"c", "d"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "k=v arg= code=27",
      .expected = INIT_KVCONTAINER({"k", "v"}, {"arg", ""}, {"code", "27"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "k=a=b c=d",
      .expected = INIT_KVCONTAINER({"k", "a=b"}, {"c", "d"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "k1=v1    k2=v2     k3=v3 ",
      .expected = INIT_KVCONTAINER({"k1", "v1"}, {"k2", "v2"}, {"k3", "v3"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "k1=v1,k2=v2,k3=v3",
      .expected = INIT_KVCONTAINER({"k1", "v1,k2=v2,k3=v3"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "k=\\",
      .expected = INIT_KVCONTAINER({"k", "\\"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "k1=v1\tk2=v2 k3=v3",
      .expected = INIT_KVCONTAINER({"k1", "v1\tk2=v2"}, {"k3", "v3"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "k1=v1,\tk2=v2 k3=v3",
      .expected = INIT_KVCONTAINER({"k1", "v1,\tk2=v2"}, {"k3", "v3"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "k1=v1\t k2=v2 k3=v3",
      .expected = INIT_KVCONTAINER({"k1", "v1\t"}, {"k2", "v2"}, {"k3", "v3"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "k=\t",
      .expected = INIT_KVCONTAINER({"k", "\t"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "k=,\t",
      .expected = INIT_KVCONTAINER({"k", ",\t"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "foo=\"bar\"",
      .expected = INIT_KVCONTAINER({"foo", "bar"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "k1=\\b\\f\\n\\r\\t\\\\",
      .expected = INIT_KVCONTAINER({"k1", "\\b\\f\\n\\r\\t\\\\"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "k1=\b\f\n\r\\",
      .expected = INIT_KVCONTAINER({"k1", "\b\f\n\r\\"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "=v",
      .expected = INIT_KVCONTAINER(),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "k*=v",
      .expected = INIT_KVCONTAINER(),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "=",
      .expected = INIT_KVCONTAINER(),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "==",
      .expected = INIT_KVCONTAINER(),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "===",
      .expected = INIT_KVCONTAINER(),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = " =",
      .expected = INIT_KVCONTAINER(),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = " ==",
      .expected = INIT_KVCONTAINER(),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = " ===",
      .expected = INIT_KVCONTAINER(),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = " = =",
      .expected = INIT_KVCONTAINER(),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = ":=",
      .expected = INIT_KVCONTAINER(),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "á=v",
      .expected = INIT_KVCONTAINER(),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "",
      .expected = INIT_KVCONTAINER(),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "f",
      .expected = INIT_KVCONTAINER(),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "fo",
      .expected = INIT_KVCONTAINER(),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "foo",
      .expected = INIT_KVCONTAINER(),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = ", k=v",
      .expected = INIT_KVCONTAINER({"k", "v"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = ",k=v",
      .expected = INIT_KVCONTAINER({"k", "v"}),
    },

    {}
  };

  return g_memdup(tc, sizeof(tc));
}

static Testcase *
_provide_cases_without_allow_pair_separator_in_value(void)
{
  Testcase tc[] =
  {
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG),
      .input = "k=\"a",
      .expected = INIT_KVCONTAINER({"k", "a"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG),
      .input = "k=\"\\",
      .expected = INIT_KVCONTAINER({"k", ""}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG),
      .input = "k='a",
      .expected = INIT_KVCONTAINER({"k", "a"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG),
      .input = "k='\\",
      .expected = INIT_KVCONTAINER({"k", ""}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG),
      .input = " =k=v=w",
      .expected = INIT_KVCONTAINER({"k", "v=w"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG),
      .input = "k=\"\xc3v",
      .expected = INIT_KVCONTAINER({"k", "\xc3v"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG),
      .input = "k=\"\xff",
      .expected = INIT_KVCONTAINER({"k", "\xff"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG),
      .input = "k=\"\xffv",
      .expected = INIT_KVCONTAINER({"k", "\xffv"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG),
      .input = "lorem ipsum/dolor = foo=bar",
      .expected = INIT_KVCONTAINER({"foo", "bar"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG),
      .input = "k1=\"v1\", k2=\"v2\"",
      .expected = INIT_KVCONTAINER({"k1", "v1"}, {"k2", "v2"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG),
      .input = "k1=\"\\\"v1\"",
      .expected = INIT_KVCONTAINER({"k1", "\"v1"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG),
      .input = "k1=\"\\b \\f \\n \\r \\t \\\\\"",
      .expected = INIT_KVCONTAINER({"k1", "\b \f \n \r \t \\"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG),
      .input = "k1=\"\\p\"",
      .expected = INIT_KVCONTAINER({"k1", "\\p"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG),
      .input = "k1='\\'v1'",
      .expected = INIT_KVCONTAINER({"k1", "'v1"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG),
      .input = "k1='\\b \\f \\n \\r \\t \\\\'",
      .expected = INIT_KVCONTAINER({"k1", "\b \f \n \r \t \\"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG),
      .input = "k1='\\p'",
      .expected = INIT_KVCONTAINER({"k1", "\\p"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG),
      .input = "k1=\"v foo, foo2 =@,\\\"\" k2='v foo,  a='",
      .expected = INIT_KVCONTAINER({"k1", "v foo, foo2 =@,\""}, {"k2", "v foo,  a="}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG),
      .input = "k1=v1, k2=v2, k3=v3",
      .expected = INIT_KVCONTAINER({"k1", "v1"}, {"k2", "v2"}, {"k3", "v3"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG),
      .input = "foo=bar lorem ipsum key=value some more values",
      .expected = INIT_KVCONTAINER({"foo", "bar"}, {"key", "value"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG),
      .input = "k1=v1,   k2=v2  ,    k3=v3",
      .expected = INIT_KVCONTAINER({"k1", "v1"}, {"k2", "v2"}, {"k3", "v3"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG),
      .input = "k1 k2=v2, k3, k4=v4",
      .expected = INIT_KVCONTAINER({"k2", "v2"}, {"k4", "v4"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG),
      .input = "k1= k2=v2, k3=, k4=v4 k5= , k6=v6",
      .expected = INIT_KVCONTAINER({"k1", ""}, {"k2", "v2"}, {"k3", ""}, {"k4", "v4"}, {"k5", ""}, {"k6", "v6"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG),
      .input = "k1= v1 k2 = v2 k3 =v3 ",
      .expected = INIT_KVCONTAINER({"k1", ""}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG),
      .input = "k1='v1', k2='v2'",
      .expected = INIT_KVCONTAINER({"k1", "v1"}, {"k2", "v2"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG),
      .input = "k=v,",
      .expected = INIT_KVCONTAINER({"k", "v,"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG),
      .input = "k=v, ",
      .expected = INIT_KVCONTAINER({"k", "v"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(COLON_CONFIG),
      .input = "k1:v1 k2:v2 k3:v3 ",
      .expected = INIT_KVCONTAINER({"k1", "v1"}, {"k2", "v2"}, {"k3", "v3"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(COLON_CONFIG),
      .input = "k1: v1 k2 : v2 k3 :v3 ",
      .expected = INIT_KVCONTAINER({"k1", ""}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DASH_CONFIG),
      .input = "k-v",
      .expected = INIT_KVCONTAINER({"k", "v"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DASH_CONFIG),
      .input = "k--v",
      .expected = INIT_KVCONTAINER({"k", "-v"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DASH_CONFIG),
      .input = "---",
      .expected = INIT_KVCONTAINER({"-", "-"}),
    },
    {}
  };

  return g_memdup(tc, sizeof(tc));
}

static Testcase *
_provide_cases_with_allow_pair_separator_in_value(void)
{
  Testcase tc[] =
  {
    /* FIXME: do we really want spaces to be trimmed from key names like this ? */
    {
      TC_HEAD,
      .config = CONFIG_LIST(SPACE_HANDLING_CONFIG),
      .input = "foo =bar",
      .expected = INIT_KVCONTAINER({"foo", "bar"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(SPACE_HANDLING_CONFIG),
      .input = "foo =bar",
      .expected = INIT_KVCONTAINER({"foo", "bar"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(SPACE_HANDLING_CONFIG),
      .input = "foo=bar ggg",
      .expected = INIT_KVCONTAINER({"foo", "bar ggg"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(SPACE_HANDLING_CONFIG),
      .input = "foo=bar ggg baz=ez",
      .expected = INIT_KVCONTAINER({"foo", "bar ggg"}, {"baz", "ez"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(SPACE_HANDLING_CONFIG),
      .input = " foo =bar ggg baz=ez",
      .expected = INIT_KVCONTAINER({"foo", "bar ggg"}, {"baz", "ez"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(SPACE_HANDLING_CONFIG),
      .input = "foo =bar ggg baz =ez",
      .expected = INIT_KVCONTAINER({"foo", "bar ggg"}, {"baz", "ez"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(SPACE_HANDLING_CONFIG),
      .input = "foo =bar ggg baz   =ez",
      .expected = INIT_KVCONTAINER({"foo", "bar ggg"}, {"baz", "ez"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(SPACE_HANDLING_CONFIG),
      .input = "foo =  bar ggg baz   =   ez",
      .expected = INIT_KVCONTAINER({"foo", "bar ggg"}, {"baz", "ez"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(SPACE_HANDLING_CONFIG),
      .input = "k===  a",
      .expected = INIT_KVCONTAINER({"k", "==  a"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(SPACE_HANDLING_CONFIG),
      .input = "a b c=d",
      .expected = INIT_KVCONTAINER({"c", "d"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(SPACE_HANDLING_CONFIG),
      .input = " k= b",
      .expected = INIT_KVCONTAINER({"k", "b"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(SPACE_HANDLING_CONFIG),
      .input = "foo=a \"bar baz\" ",
      .expected = INIT_KVCONTAINER({"foo", "a \"bar baz\""}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(SPACE_HANDLING_CONFIG),
      .input = "foo=a \"bar baz",
      .expected = INIT_KVCONTAINER({"foo", "a \"bar baz"}),
    },

    /* FIXME: why a single quote handled differently? */
    {
      TC_HEAD,
      .config = CONFIG_LIST(SPACE_HANDLING_CONFIG),
      .input = "foo=a \"bar baz c=d",
      .expected = INIT_KVCONTAINER({"foo", "a \"bar baz"}, {"c", "d"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(SPACE_HANDLING_CONFIG),
      .input = "foo=a \"bar baz\"=f c=d a",
      .expected = INIT_KVCONTAINER({"foo", "a \"bar baz\"=f"}, {"c", "d a"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(SPACE_HANDLING_CONFIG),
      .input = "foo=\\\"bar baz\\\"",
      .expected = INIT_KVCONTAINER({"foo", "\\\"bar baz\\\""}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(SPACE_HANDLING_CONFIG),
      .input = "foo=\"bar\" baz c=d",
      .expected = INIT_KVCONTAINER({"foo", "\"bar\" baz"}, {"c", "d"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(SPACE_HANDLING_CONFIG),
      .input = "foo=bar\"",
      .expected = INIT_KVCONTAINER({"foo", "bar\""}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(SPACE_HANDLING_CONFIG),
      .input = "k===a",
      .expected = INIT_KVCONTAINER({"k", "==a"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(SPACE_HANDLING_CONFIG),
      .input = "k===  a",
      .expected = INIT_KVCONTAINER({"k", "==  a"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(SPACE_HANDLING_CONFIG),
      .input = "k===a=b",
      .expected = INIT_KVCONTAINER({"k", "==a=b"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(SPACE_HANDLING_CONFIG),
      .input = "a==b=",
      .expected = INIT_KVCONTAINER({"a", "=b="}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(SPACE_HANDLING_CONFIG),
      .input = "a=,=b=a",
      .expected = INIT_KVCONTAINER({"a", ",=b=a"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(SPACE_HANDLING_CONFIG),
      .input = "a= =a",
      .expected = INIT_KVCONTAINER({"a", "=a"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(SPACE_HANDLING_CONFIG),
      .input = "foo=\"bar baz\"",
      .expected = INIT_KVCONTAINER({"foo", "bar baz"}),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(SPACE_HANDLING_CONFIG),
      .input = "foo='bar",
      .expected = INIT_KVCONTAINER({"foo", "'bar"}),
    },
    {}
  };

  return g_memdup(tc, sizeof(tc));
}

static Testcase *
_provide_cases_for_performance_test_nothing_to_parse(void)
{
  Testcase tc[] =
  {
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "Reducing the compressed framebuffer size. This may lead to less power savings than a non-reduced-size. \
Try to increase stolen memory size if available in BIOS.",
      .expected = INIT_KVCONTAINER(),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
      .input = "interrupt took too long (3136 > 3127), lowering kernel.perf_event_max_sample_rate to 63750",
      .expected = INIT_KVCONTAINER(),
    },
    {
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
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
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
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
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
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
      TC_HEAD,
      .config = CONFIG_LIST(DEFAULT_CONFIG, SPACE_HANDLING_CONFIG),
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
    {}
  };
  return g_memdup(tc, sizeof(tc));
}

static GString *
_expected_to_string(const KVContainer kvs)
{
  GString *result = g_string_new("");
  gboolean first = TRUE;
  for (gint i = 0; i < kvs.n; i++)
    {
      if (!first)
        {
          g_string_append_c(result, ' ');
        }
      first = FALSE;
      g_string_append_printf(result, "%s=%s", kvs.arg[i].key, kvs.arg[i].value);
    }

  return result;
}

static void
_run_testcase(const Testcase tc)
{
  GString *pretty_expected;
  const ScannerConfig *cfg = tc.config;
  while (cfg->kv_separator != 0)
    {
      pretty_expected = _expected_to_string(tc.expected);
      TEST_KV_SCAN_ARRAY(*cfg, tc.input, tc.expected);
      g_string_free(pretty_expected, TRUE);
      cfg++;
    }
}

static void
_run_testcases(Testcase *cases)
{
  const Testcase *tc = cases;
  while (tc->input)
    {
      _run_testcase(*tc);
      tc++;
    }
  g_free(cases);
}

#define ITERATION_NUMBER 10

static void
_test_performance(Testcase *tcs, gchar *title)
{
  GString *pretty_expected;
  const ScannerConfig *cfg = NULL;
  gint cfg_index = 0;
  const Testcase *tc;
  gint iteration_index = 0;

  if (title)
    {
      printf("Performance test: %s\n", title);
    }

  for (cfg_index = 0; tcs->config[cfg_index].kv_separator != 0; cfg_index++)
    {

      start_stopwatch();

      for (iteration_index = 0; iteration_index < ITERATION_NUMBER; iteration_index++)
        {
          for (tc = tcs; tc->input; tc++)
            {
              cfg = &tc->config[cfg_index];

              pretty_expected = _expected_to_string(tc->expected);
              TEST_KV_SCAN_ARRAY(*cfg, tc->input, tc->expected);
              g_string_free(pretty_expected, TRUE);
            }
        }

      if (cfg != NULL)
        {
          stop_stopwatch_and_display_result(1, "Is pair-separator allowed in values: %s KV-separator: '%c' ",
                                            cfg->allow_pair_separator_in_value?"YES":"NO ",
                                            cfg->kv_separator);
        }
    }
  g_free(tcs);
}



Test(kv_scanner, array_based_tests)
{
  _run_testcases(_provide_common_cases());
  _run_testcases(_provide_cases_without_allow_pair_separator_in_value());
  _run_testcases(_provide_cases_with_allow_pair_separator_in_value());
}

Test(kv_scanner, performance_tests)
{
  /* with criterion asserts are slow, so it's not suitable for perf testing */
  if (0)
    {
      _test_performance(_provide_common_cases(), "Common test cases");
      _test_performance(_provide_cases_for_performance_test_nothing_to_parse(), "Nothing to parse in the message");
      _test_performance(_provide_cases_for_performance_test_parse_long_msg(), "Parse long strings");
    }
}
