#include "syslog-ng.h"
#include "testutils.h"

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

  assert_string(result_ref, expected, "strtok return value mismatch");

  g_free(raw_string);
  g_free(result);
}

void
test_strtok_with_literals(STRTOK_R_FUN tokenizer_func)
{
  assert_if_tokenizer_concatenated_result_not_match(tokenizer_func, ".",
                                                    "token1.token2",
                                                    "token1token2");

  assert_if_tokenizer_concatenated_result_not_match(tokenizer_func, ".",
                                                    ".token",
                                                    "token");

  assert_if_tokenizer_concatenated_result_not_match(tokenizer_func, ".",
                                                    "token.",
                                                    "token");

  assert_if_tokenizer_concatenated_result_not_match(tokenizer_func, ".",
                                                    ".",
                                                    NULL);

  assert_if_tokenizer_concatenated_result_not_match(tokenizer_func, "...",
                                                    ".",
                                                    NULL);

  assert_if_tokenizer_concatenated_result_not_match(tokenizer_func, "... ",
                                                    "      ",
                                                    NULL);

  assert_if_tokenizer_concatenated_result_not_match(tokenizer_func, "..*,;-",
                                                    ";-*token1...*****token2**,;;;.",
                                                    "token1token2");

  assert_if_tokenizer_concatenated_result_not_match(tokenizer_func, "..*,;- ",
                                                    ";-*token1...*****token2**,;;;.token3",
                                                    "token1token2token3");

  assert_if_tokenizer_concatenated_result_not_match(tokenizer_func, "..*,;- ",
                                                    ";-*token1...*****token2**,;;;.token3 ",
                                                    "token1token2token3");
}

int
main(int argc, char *argv[])
{
  test_strtok_with_literals(__test_strtok_r);

  return EXIT_SUCCESS;
}
