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
#include "kv-scanner-generic.h"
#include "kv-scanner-simple.h"
#include "testutils.h"

static void
_assert_no_more_tokens(KVScanner *scanner)
{
  GString *msg;
  gboolean ok = scanner->scan_next(scanner);

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
      expect_not_reached(msg->str);
      g_string_free(msg, TRUE);
    }
}

static gboolean
_assert_current_key_is(KVScanner *scanner, const gchar *expected_key)
{
  const gchar *key = kv_scanner_get_current_key(scanner);

  return expect_nstring(key, -1, expected_key, -1, "current key mismatch");
}

static gboolean
_assert_current_value_is(KVScanner *scanner, const gchar *expected_value)
{
  const gchar *value = kv_scanner_get_current_value(scanner);

  return expect_nstring(value, -1, expected_value, -1, "current value mismatch");
}

static gboolean
_compare_key_value(KVScanner *scanner, const gchar *key, const gchar *value)
{
  g_assert(value);

  gboolean ok = scanner->scan_next(scanner);
  if (ok)
    {
      _assert_current_key_is(scanner, key);
      _assert_current_value_is(scanner, value);
      return TRUE;
    }
  else
    {
      expect_true(ok, "kv_scanner is expected to return TRUE for scan_next(), "
                  "first unconsumed pair: [%s/%s]",
                  key, value);
      return FALSE;
    }
}

typedef const gchar *const VAElement;


typedef struct _KVQElement
{
  const gchar *const key;
  const gchar *const value;
  gboolean quoted;
} KVQElement;

typedef struct _KVQContainer
{
  const gsize n;
  const KVQElement *const arg;
} KVQContainer;

#define VARARG_STRUCT(VARARG_STRUCT_cont, VARARG_STRUCT_elem, ...) \
  (const VARARG_STRUCT_cont) { \
    sizeof((const VARARG_STRUCT_elem[]) { __VA_ARGS__ }) / sizeof(VARARG_STRUCT_elem), \
    (const VARARG_STRUCT_elem[]){__VA_ARGS__}\
  }

static void
_scan_kv_pairs_quoted(KVScanner *scanner, const gchar *input, KVQContainer args)
{
  g_assert(input);
  kv_scanner_input(scanner, input);
  for (gsize i = 0; i < args.n; i++)
    {
      if (!_compare_key_value(scanner, args.arg[i].key, args.arg[i].value))
        break;
      expect_gboolean(scanner->value_was_quoted, args.arg[i].quoted,
                      "mismatch in value_was_quoted for [%s/%s]",
                      args.arg[i].key, args.arg[i].value);
    }
  _assert_no_more_tokens(scanner);
  kv_scanner_free(scanner);
}

#define TEST_KV_SCAN_Q(SCANNER_input, TEST_KV_SCAN_input, ...) \
  do { \
    testcase_begin("TEST_KV_SCAN_Q(%s, %s)", #TEST_KV_SCAN_input, #__VA_ARGS__); \
    _scan_kv_pairs_quoted(SCANNER_input, TEST_KV_SCAN_input, VARARG_STRUCT(KVQContainer, KVQElement, __VA_ARGS__)); \
    testcase_end(); \
  } while (0)

typedef struct _ScannerConfig
{
  gchar kv_separator;
  gboolean allow_pair_separator_in_value;
} ScannerConfig;

typedef struct _KV
{
  gchar *key;
  gchar *value;
} KV;

typedef struct Testcase_t
{
  gint line;
  const gchar *function;
  ScannerConfig config[5];
  gchar *input;
  KV expected[25];
} Testcase;

KVScanner *
create_kv_scanner(const ScannerConfig config)
{
  return (config.allow_pair_separator_in_value ?
          kv_scanner_generic_new(config.kv_separator, NULL) :
          kv_scanner_simple_new(config.kv_separator, NULL));
}

static void
_scan_kv_pairs_scanner(KVScanner *scanner, const gchar *input, const KV kvs[])
{
  g_assert(input);
  kv_scanner_input(scanner, input);

  while (kvs->key)
    {
      if (!kvs->value || !_compare_key_value(scanner, kvs->key, kvs->value))
        break;
      kvs++;
    }
  _assert_no_more_tokens(scanner);
  kv_scanner_free(scanner);
}

static void
_test_key_buffer_underrun(void)
{
  const gchar *buffer = "ab=v";
  const gchar *input = buffer + 2;

  KV expect_nothing[] =
  {
    {}
  };

  _scan_kv_pairs_scanner(create_kv_scanner((ScannerConfig)
  {'=', FALSE
  }), input, expect_nothing);
}

static void
_test_quotation_is_stored_in_the_was_quoted_value_member(void)
{
  ScannerConfig config = {'=', FALSE};
  TEST_KV_SCAN_Q(create_kv_scanner(config), "foo=\"bar\"", {"foo", "bar", TRUE});
  TEST_KV_SCAN_Q(create_kv_scanner(config), "foo='bar'", {"foo", "bar", TRUE});
  TEST_KV_SCAN_Q(create_kv_scanner(config), "foo=bar", {"foo", "bar", FALSE});
  TEST_KV_SCAN_Q(create_kv_scanner(config), "foo='bar", {"foo", "bar", TRUE});
  TEST_KV_SCAN_Q(create_kv_scanner(config), "foo='bar' k=v", {"foo", "bar", TRUE}, {"k", "v", FALSE});
}

static void
_test_quotation_is_stored_in_the_was_quoted_value_member_with_space_separator_option(void)
{
  ScannerConfig config = {'=', TRUE};
  TEST_KV_SCAN_Q(create_kv_scanner(config), "foo=\"bar\"", {"foo", "bar", TRUE});
  TEST_KV_SCAN_Q(create_kv_scanner(config), "foo='bar'", {"foo", "bar", TRUE});
  TEST_KV_SCAN_Q(create_kv_scanner(config), "foo=bar", {"foo", "bar", FALSE});
  TEST_KV_SCAN_Q(create_kv_scanner(config), "foo='bar", {"foo", "'bar", FALSE});
  TEST_KV_SCAN_Q(create_kv_scanner(config), "foo='bar' k=v", {"foo", "bar", TRUE}, {"k", "v", FALSE});
}

static gboolean
_transform_value_by_incrementing_all_bytes(KVScanner *self)
{
  gint i;

  g_string_assign(self->decoded_value, self->value->str);
  for (i = 0; i < self->decoded_value->len; i++)
    self->decoded_value->str[i]++;
  return TRUE;
}

static void
_test_transforms_values_if_transform_value_is_set(void)
{
  KVScanner *scanner = create_kv_scanner((ScannerConfig)
  {'=', FALSE
  });
  scanner->transform_value = _transform_value_by_incrementing_all_bytes;

  _scan_kv_pairs_scanner(scanner, "foo=\"bar\"", (KV[])
  { {"foo", "cbs"}, {}
  });
}

static void
_test_transforms_values_if_transform_value_is_set_with_space_separator_option(void)
{
  KVScanner *scanner = create_kv_scanner((ScannerConfig)
  {'=', TRUE
  });
  scanner->transform_value = _transform_value_by_incrementing_all_bytes;

  _scan_kv_pairs_scanner(scanner, "foo=\"bar\"", (KV[])
  { {"foo", "cbs"}, {}
  });
}

static void
_test_value_separator_clone(void)
{
  KVScanner *scanner = kv_scanner_simple_new(':', NULL);
  KVScanner *cloned_scanner = scanner->clone(scanner);
  kv_scanner_free(scanner);

  _scan_kv_pairs_scanner(
    cloned_scanner,
    "key1:value1 key2:value2 key3:value3 ",
    (KV[])
  { {"key1", "value1"}, {"key2", "value2"}, {"key3", "value3"}, {}
  }
  );
}

static void
_test_transform_value_clone(void)
{
  KVScanner *scanner = kv_scanner_simple_new('=', _transform_value_by_incrementing_all_bytes);
  KVScanner *cloned_scanner = scanner->clone(scanner);
  kv_scanner_free(scanner);

  _scan_kv_pairs_scanner(cloned_scanner, "foo=\"hal\"", (KV[])
  { {"foo", "ibm"}, {}
  }
                        );
}

#define DEFAULT_CONFIG {.kv_separator='=', .allow_pair_separator_in_value=FALSE}
#define SPACE_HANDLING_CONFIG {.kv_separator='=', .allow_pair_separator_in_value=TRUE}

#define TC_HEAD .line=__LINE__, .function=__FUNCTION__

static const Testcase *
_provide_common_cases(void)
{
  static const Testcase common_cases[] =
  {
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "foo=bar",
      { {"foo", "bar"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "k-j=v",
      { {"k-j", "v"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "0=v",
      { {"0", "v"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "_=v",
      { {"_", "v"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "Z=v",
      { {"Z", "v"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "k==",
      { {"k", "="}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "k===",
      { {"k", "=="}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "k===a",
      { {"k", "==a"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "k===a=b",
      { {"k", "==a=b"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      " ==k=",
      { {"k", ""}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      " = =k=",
      { {"k", ""}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      " =k=",
      { {"k", ""}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      " =k=v",
      { {"k", "v"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      " ==k=v",
      { {"k", "v"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "k=\xc3",
      { {"k", "\xc3"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "k=\xc3v",
      { {"k", "\xc3v"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "k=\xff",
      { {"k", "\xff"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "k=\xffv",
      { {"k", "\xffv"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "foo=",
      { {"foo", ""}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "foo=b",
      { {"foo", "b"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "lorem ipsum foo=bar",
      { {"foo", "bar"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "lorem ipsum/dolor @sitamen foo=bar",
      { {"foo", "bar"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "*k=v",
      { {"k", "v"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "x *k=v",
      { {"k", "v"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "k1=v1 k2=v2 k3=v3",
      { {"k1", "v1"}, {"k2", "v2"}, {"k3", "v3"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "k= a=b c=d",
      { {"k", ""}, {"a", "b"}, {"c", "d"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "k=v arg= code=27",
      { {"k", "v"}, {"arg", ""}, {"code", "27"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "k=a=b c=d",
      { {"k", "a=b"}, {"c", "d"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "k1=v1    k2=v2     k3=v3 ",
      { {"k1", "v1"}, {"k2", "v2"}, {"k3", "v3"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "k1=v1,k2=v2,k3=v3",
      { {"k1", "v1,k2=v2,k3=v3"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "k=\\",
      { {"k", "\\"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "k1=v1\tk2=v2 k3=v3",
      { {"k1", "v1\tk2=v2"}, {"k3", "v3"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "k1=v1,\tk2=v2 k3=v3",
      { {"k1", "v1,\tk2=v2"}, {"k3", "v3"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "k1=v1\t k2=v2 k3=v3",
      { {"k1", "v1\t"}, {"k2", "v2"}, {"k3", "v3"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "k=\t",
      { {"k", "\t"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "k=,\t",
      { {"k", ",\t"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "foo=\"bar\"",
      { {"foo", "bar"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "k1=\\b\\f\\n\\r\\t\\\\",
      { {"k1", "\\b\\f\\n\\r\\t\\\\"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "k1=\b\f\n\r\\",
      { {"k1", "\b\f\n\r\\"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "=v",
      { {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "k*=v",
      { {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "=",
      { {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "==",
      { {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "===",
      { {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      " =",
      { {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      " ==",
      { {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      " ===",
      { {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      " = =",
      { {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      ":=",
      { {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "á=v",
      { {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "",
      { {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "f",
      { {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "fo",
      { {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "foo",
      { {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      ", k=v",
      { {"k", "v"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      ",k=v",
      { {"k", "v"}, {} }
    },

    {}
  };

  return common_cases;
}

static const Testcase *
_provide_cases_without_allow_pair_separator_in_value(void)
{
  static const Testcase cases_without_allow_pair_separator_in_value[] =
  {
    {
      TC_HEAD,
      { DEFAULT_CONFIG, {} },
      "k=\"a",
      { {"k", "a"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, {} },
      "k=\"\\",
      { {"k", ""}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, {} },
      "k='a",
      { {"k", "a"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, {} },
      "k='\\",
      { {"k", ""}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, {} },
      " =k=v=w",
      { {"k", "v=w"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, {} },
      "k=\"\xc3v",
      { {"k", "\xc3v"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, {} },
      "k=\"\xff",
      { {"k", "\xff"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, {} },
      "k=\"\xffv",
      { {"k", "\xffv"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, {} },
      "lorem ipsum/dolor = foo=bar",
      { {"foo", "bar"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, {} },
      "k1=\"v1\", k2=\"v2\"",
      { {"k1", "v1"}, {"k2", "v2"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, {} },
      "k1=\"\\\"v1\"",
      { {"k1", "\"v1"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, {} },
      "k1=\"\\b \\f \\n \\r \\t \\\\\"",
      { {"k1", "\b \f \n \r \t \\"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, {} },
      "k1=\"\\p\"",
      { {"k1", "\\p"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, {} },
      "k1='\\'v1'",
      { {"k1", "'v1"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, {} },
      "k1='\\b \\f \\n \\r \\t \\\\'",
      { {"k1", "\b \f \n \r \t \\"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, {} },
      "k1='\\p'",
      { {"k1", "\\p"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, {} },
      "k1=\"v foo, foo2 =@,\\\"\" k2='v foo,  a='",
      { {"k1", "v foo, foo2 =@,\""}, {"k2", "v foo,  a="}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, {} },
      "k1=v1, k2=v2, k3=v3",
      { {"k1", "v1"}, {"k2", "v2"}, {"k3", "v3"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, {} },
      "foo=bar lorem ipsum key=value some more values",
      { {"foo", "bar"}, {"key", "value"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, {} },
      "k1=v1,   k2=v2  ,    k3=v3",
      { {"k1", "v1"}, {"k2", "v2"}, {"k3", "v3"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, {} },
      "k1 k2=v2, k3, k4=v4",
      { {"k2", "v2"}, {"k4", "v4"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, {} },
      "k1= k2=v2, k3=, k4=v4 k5= , k6=v6",
      { {"k1", ""}, {"k2", "v2"}, {"k3", ""}, {"k4", "v4"}, {"k5", ""}, {"k6", "v6"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, {} },
      "k1= v1 k2 = v2 k3 =v3 ",
      { {"k1", ""}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, {} },
      "k1='v1', k2='v2'",
      { {"k1", "v1"}, {"k2", "v2"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, {} },
      "k=v,",
      { {"k", "v,"}, {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, {} },
      "k=v, ",
      { {"k", "v"}, {} }
    },
    {
      TC_HEAD,
      { {':', FALSE}, {} },
      "k1:v1 k2:v2 k3:v3 ",
      { {"k1", "v1"}, {"k2", "v2"}, {"k3", "v3"}, {} }
    },
    {
      TC_HEAD,
      { {':', FALSE}, {} },
      "k1: v1 k2 : v2 k3 :v3 ",
      { {"k1", ""}, {} }
    },
    {
      TC_HEAD,
      { {'-', FALSE}, {} },
      "k-v",
      { {"k", "v"}, {} }
    },
    {
      TC_HEAD,
      { {'-', FALSE}, {} },
      "k--v",
      { {"k", "-v"}, {} }
    },
    {
      TC_HEAD,
      { {'-', FALSE}, {} },
      "---",
      { {"-", "-"}, {} }
    },

    {}
  };

  return cases_without_allow_pair_separator_in_value;
}

static const Testcase *
_provide_cases_with_allow_pair_separator_in_value(void)
{
  static const Testcase cases_with_allow_pair_separator_in_value[] =
  {
    {
      TC_HEAD,
      { SPACE_HANDLING_CONFIG, {} },
      "foo =bar",
      { {"foo", "bar"}, {} }
    },
    {
      TC_HEAD,
      { SPACE_HANDLING_CONFIG, {} },
      "foo =bar",
      { {"foo", "bar"}, {} }
    },
    {
      TC_HEAD,
      { SPACE_HANDLING_CONFIG, {} },
      "foo=bar ggg",
      { {"foo", "bar ggg"}, {} }
    },
    {
      TC_HEAD,
      { SPACE_HANDLING_CONFIG, {} },
      "foo=bar ggg baz=ez",
      { {"foo", "bar ggg"}, {"baz", "ez"}, {} }
    },
    {
      TC_HEAD,
      { SPACE_HANDLING_CONFIG, {} },
      " foo =bar ggg baz=ez",
      { {"foo", "bar ggg"}, {"baz", "ez"}, {} }
    },
    {
      TC_HEAD,
      { SPACE_HANDLING_CONFIG, {} },
      "foo =bar ggg baz =ez",
      { {"foo", "bar ggg"}, {"baz", "ez"}, {} }
    },
    {
      TC_HEAD,
      { SPACE_HANDLING_CONFIG, {} },
      "foo =bar ggg baz   =ez",
      { {"foo", "bar ggg"}, {"baz", "ez"}, {} }
    },
    {
      TC_HEAD,
      { SPACE_HANDLING_CONFIG, {} },
      "foo =  bar ggg baz   =   ez",
      { {"foo", "bar ggg"}, {"baz", "ez"}, {} }
    },
    {
      TC_HEAD,
      { SPACE_HANDLING_CONFIG, {} },
      "k===  a",
      { {"k", "==  a"}, {} }
    },
    {
      TC_HEAD,
      { SPACE_HANDLING_CONFIG, {} },
      "a b c=d",
      { {"c", "d"}, {} }
    },
    {
      TC_HEAD,
      { SPACE_HANDLING_CONFIG, {} },
      " k= b",
      { {"k", "b"}, {} }
    },
    {
      TC_HEAD,
      { SPACE_HANDLING_CONFIG, {} },
      "foo=a \"bar baz\" ",
      { {"foo", "a \"bar baz\""}, {} }
    },
    {
      TC_HEAD,
      { SPACE_HANDLING_CONFIG, {} },
      "foo=a \"bar baz",
      { {"foo", "a \"bar baz"}, {} }
    },
    {
      TC_HEAD,
      { SPACE_HANDLING_CONFIG, {} },
      "foo=a \"bar baz c=d",
      { {"foo", "a \"bar baz"}, {"c", "d"}, {} }
    },
    {
      TC_HEAD,
      { SPACE_HANDLING_CONFIG, {} },
      "foo=a \"bar baz\"=f c=d a",
      { {"foo", "a \"bar baz\"=f"}, {"c", "d a"}, {} }
    },
    {
      TC_HEAD,
      { SPACE_HANDLING_CONFIG, {} },
      "foo=\\\"bar baz\\\"",
      { {"foo", "\\\"bar baz\\\""}, {} }
    },
    {
      TC_HEAD,
      { SPACE_HANDLING_CONFIG, {} },
      "foo=\"bar\" baz c=d",
      { {"foo", "\"bar\" baz"}, {"c", "d"}, {} }
    },
    {
      TC_HEAD,
      { SPACE_HANDLING_CONFIG, {} },
      "foo=bar\"",
      { {"foo", "bar\""}, {} }
    },
    {
      TC_HEAD,
      { SPACE_HANDLING_CONFIG, {} },
      "k===a",
      { {"k", "==a"}, {} }
    },
    {
      TC_HEAD,
      { SPACE_HANDLING_CONFIG, {} },
      "k===  a",
      { {"k", "==  a"}, {} }
    },
    {
      TC_HEAD,
      { SPACE_HANDLING_CONFIG, {} },
      "k===a=b",
      { {"k", "==a=b"}, {} }
    },
    {
      TC_HEAD,
      { SPACE_HANDLING_CONFIG, {} },
      "a==b=",
      { {"a", "=b="}, {} }
    },
    {
      TC_HEAD,
      { SPACE_HANDLING_CONFIG, {} },
      "a=,=b=a",
      { {"a", ",=b=a"}, {} }
    },
    {
      TC_HEAD,
      { SPACE_HANDLING_CONFIG, {} },
      "a= =a",
      { {"a", "=a"}, {} }
    },
    {
      TC_HEAD,
      { SPACE_HANDLING_CONFIG, {} },
      "foo=\"bar baz\"",
      { {"foo", "bar baz"}, {} }
    },
    {
      TC_HEAD,
      { SPACE_HANDLING_CONFIG, {} },
      "foo='bar",
      { {"foo", "'bar"}, {} }
    },

    {}
  };

  return cases_with_allow_pair_separator_in_value;
}

static const Testcase *
_provide_cases_for_performance_test_nothing_to_parse(void)
{
  static const Testcase cases_for_performance_test_nothing_to_parse[] =
  {
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "Reducing the compressed framebuffer size. This may lead to less power savings than a non-reduced-size. \
Try to increase stolen memory size if available in BIOS.",
      { {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "interrupt took too long (3136 > 3127), lowering kernel.perf_event_max_sample_rate to 63750",
      { {} }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "Linux version 4.6.3-040603-generic (kernel@gomeisa) (gcc version 5.4.0 20160609 (Ubuntu 5.4.0-4ubuntu1) ) \
#201606241434 SMP Fri Jun 24 18:36:33 UTC 2016",
      { {} }
    },

    {}
  };

  return cases_for_performance_test_nothing_to_parse;
}

static const Testcase *
_provide_cases_for_performance_test_parse_long_msg(void)
{
  static const Testcase cases_for_performance_test_parse_long_msg[] =
  {
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "PF: filter/forward DROP \
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
      { {"IN", "15dd205a6ac8b0c80ab3bcdcc5649c9c830074cdbdc094ff1d79f20f17c56843"},
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
        {"LEN", "c2356069e9d1e79ca924378153cfbbfb4d4416b1f99d41a2940bfdb66c5319db"},
        {}
      }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "fw=108.53.156.38 pri=6 c=262144 m=98 msg=\"Connection Opened\" f=2 sess=\"None\" \
      n=16351474 src=10.0.5.200:57719:X0:MOGWAI dst=71.250.0.14:53:X1 dstMac=f8:c0:01:73:c7:c1 proto=udp/dns sent=66",
      { {"fw", "108.53.156.38"},
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
        {"sent", "66"},
        {}
      }
    },
    {
      TC_HEAD,
      { DEFAULT_CONFIG, SPACE_HANDLING_CONFIG, {} },
      "sn=C0EAE484E43E time=\"2016-07-08 13:42:58\" fw=132.237.143.192 pri=5 c=4 m=16 msg=\"Web site access allowed\" \
      app=11 sess=\"Auto\" n=5086 usr=\"DEMO\\primarystudent\" src=10.2.3.64:50682:X2-V3023 dst=157.55.240.220:443:X1 \
      srcMac=00:50:56:8e:55:8e dstMac=c0:ea:e4:84:e4:40 proto=tcp/https dstname=sls.update.microsoft.com arg= code=27 \
      Category=\"Information Technology/Computers\" fw_action=\"process\"",
      { {"sn", "C0EAE484E43E"},
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
        {"fw_action", "process"},
        {}
      }
    },

    {}
  };
  return cases_for_performance_test_parse_long_msg;
}

static GString *
_expected_to_string(const KV *kvs)
{
  GString *result = g_string_new("");
  gboolean first = TRUE;
  while (kvs->key)
    {
      if (!first)
        {
          g_string_append_c(result, ' ');
        }
      first = FALSE;
      g_string_append_printf(result, "%s=%s", kvs->key, kvs->value);
      kvs++;
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
      testcase_begin("line:(%d), function:(%s), input:(%s), expected:(%s), separator(%c), separator_in_values(%d)",
                     tc.line,
                     tc.function,
                     tc.input,
                     pretty_expected->str,
                     cfg->kv_separator,
                     cfg->allow_pair_separator_in_value);
      _scan_kv_pairs_scanner(create_kv_scanner(*cfg), tc.input, tc.expected);
      testcase_end();
      g_string_free(pretty_expected, TRUE);
      cfg++;
    }
}

static void
_run_testcases(const Testcase *cases)
{
  const Testcase *tc = cases;
  while (tc->input)
    {
      _run_testcase(*tc);
      tc++;
    }
}

#define ITERATION_NUMBER 10000

static void
_test_performance(const Testcase *tcs, gchar *title)
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
              testcase_begin("input:(%s), expected:(%s), separator(%c), separator_in_values(%d)",
                             tc->input, pretty_expected->str, cfg->kv_separator, cfg->allow_pair_separator_in_value);
              _scan_kv_pairs_scanner(create_kv_scanner(*cfg), tc->input, tc->expected);
              testcase_end();
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
}

int main(int argc, char *argv[])
{
  _test_quotation_is_stored_in_the_was_quoted_value_member();
  _test_quotation_is_stored_in_the_was_quoted_value_member_with_space_separator_option();
  _test_key_buffer_underrun();
  _test_transforms_values_if_transform_value_is_set();
  _test_transforms_values_if_transform_value_is_set_with_space_separator_option();
  _test_value_separator_clone();
  _test_transform_value_clone();
  _run_testcases(_provide_cases_without_allow_pair_separator_in_value());
  _run_testcases(_provide_common_cases());
  _run_testcases(_provide_cases_with_allow_pair_separator_in_value());
  if (0)
    {
      _test_performance(_provide_common_cases(), "Common test cases");
      _test_performance(_provide_cases_for_performance_test_nothing_to_parse(), "Nothing to parse in the message");
      _test_performance(_provide_cases_for_performance_test_parse_long_msg(), "Parse long strings");
    }
  if (testutils_deinit())
    return 0;
  else
    return 1;
}
