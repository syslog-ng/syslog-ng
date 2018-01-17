/*
 * Copyright (c) 2002-2013 Balabit
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

#include "syslog-ng.h"
#include <criterion/criterion.h>
#include <criterion/parameterized.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include <sys/time.h>
#include <math.h>

/*
 * NOTE: this macro magic is needed in order to make this program test the
 * internal strtok_r implementation even if the system has an implementation
 * available.
 *
 * We include the implementation file directly, redefining the name of the
 * function to something else.
 */

#ifdef strtok_r
#undef strtok_r
#endif

#define TEST_STRTOK_R 1
#define strtok_r __test_strtok_r

#include "../strtok_r.c"

#undef strtok_r
#undef TEST_STRTOK_R

typedef char *(STRTOK_R_FUN)(char *str, const char *delim, char **saveptr);

void
assert_if_tokenizer_concatenated_result_not_match(STRTOK_R_FUN tokenizer,
                                                  const char *delim,
                                                  const char *input,
                                                  const char *expected)
{
  gchar *token;
  gchar *saveptr;
  gchar *result = (char *)g_malloc(strlen(input)+1);
  gchar *raw_string;
  gchar *result_ref = NULL;
  int result_idx = 0;
  int token_length;

  raw_string = g_strdup(input);

  for (token = tokenizer(raw_string, delim, &saveptr); token; token = tokenizer(NULL, delim, &saveptr))
    {
      token_length = strlen(token);
      memcpy(result + result_idx, token, token_length);
      result_idx += token_length;
    }

  result[result_idx] = '\0';

  if (result_idx)
    result_ref = result;

  if (NULL == expected)
    cr_assert_null(result_ref);
  else
    cr_expect_str_eq(result_ref, expected, "strtok return value mismatch");

  g_free(raw_string);
  g_free(result);
}

struct strtok_params
{
  char *delim;
  char *input;
  char *expected;
};

ParameterizedTestParameters(strtok, with_literals)
{
  static struct strtok_params params[] =
  {
    { ".",       "token1.token2", "token1token2" },
    { ".",       ".token", "token" },
    { ".",       "token.", "token" },
    { ".",       ".", NULL },
    { "...",     ".", NULL },
    { "... ",    "      ", NULL },
    { "..*,;-",  ";-*token1...*****token2**,;;;.", "token1token2" },
    { "..*,;- ", ";-*token1...*****token2**,;;;.token3", "token1token2token3" },
    { "..*,;- ", ";-*token1...*****token2**,;;;.token3 ", "token1token2token3" }
  };

  return cr_make_param_array(struct strtok_params, params, sizeof(params)/sizeof(params[0]));
}

ParameterizedTest(struct strtok_params *param, strtok, with_literals)
{
  assert_if_tokenizer_concatenated_result_not_match(__test_strtok_r, param->delim, param->input, param->expected);
}
