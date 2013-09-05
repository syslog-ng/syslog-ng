#include "compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include <sys/time.h>
#include <math.h>

#define RAND(min,max) (min+((double)rand()/(double)RAND_MAX)*((double)max-min))
#define RAND_INT(min,max) ((int)RAND(min,max))
#define RAND_CHAR(min,max) ((char)RAND(min,max))
#define RAND_FROM_ARRAY(array, size_of_array) (array[RAND_INT(0,size_of_array-1)])

#define THREAD_NUM 5
#define DOUBLE_EPS 1e-07
#define MAX_STRTOK_SPEED_OVERHEAD_PERCENT 500.0

static const char DELIMITERS[] = " ,.-*+()=;";
static const int NUMBER_OF_TOKENS = 100;

typedef char *(STRTOK_R_FUN)(char *str, const char *delim, char **saveptr);

static inline void
start_time(struct timeval *tv)
{
  gettimeofday(tv, NULL);
}

static inline void
end_time(struct timeval *tv)
{
  gettimeofday(tv, NULL);
}

static inline double
elapsed_time(struct timeval *start_t, struct timeval *end_t)
{
  double t1 = (double) start_t->tv_sec + ((double)start_t->tv_usec/1e06);
  double t2 = (double) end_t->tv_sec + ((double)end_t->tv_usec/1e06);

  return t2-t1;
}

int
mystrcmp(const char *s1, const char *s2)
{
  if ((s1 == NULL) && (s2 == NULL))
    return 0;
  if ((s1==NULL) || (s2==NULL))
    return 2;
  return strcmp(s1,s2);
}

typedef struct _CreateTokensParam
{
  int min_token_length;
  int min_postfix_delimiters;
  int max_postfix_delimiters;
  int min_prefix_delimiters;
  int max_prefix_delimiters;
} CreateTokensParam;

static inline int
get_tokens_param_word_length(CreateTokensParam *tokens_param)
{
  return tokens_param->min_token_length +
         tokens_param->max_postfix_delimiters +
         tokens_param->max_prefix_delimiters;
}

char*
create_tokens(int number_of_tokens, const char *delimiters, CreateTokensParam *token_param)
{
  int word_length = get_tokens_param_word_length(token_param);
  char *ret = (char *)malloc(number_of_tokens * word_length + 1);
  int token_length;
  int number_of_post_delims;
  int number_of_pre_delims;
  int delimiters_length = strlen(delimiters);
  int i,j,k;


  for (i=0; i<number_of_tokens; i++)
    {
      token_length = word_length;
      number_of_post_delims = RAND(token_param->min_postfix_delimiters, token_param->max_postfix_delimiters);
      number_of_pre_delims = RAND(token_param->min_prefix_delimiters, token_param->max_prefix_delimiters);
      token_length -= number_of_post_delims;
      token_length -= number_of_pre_delims;

      assert(token_length + number_of_post_delims + number_of_pre_delims == word_length);

      k = i * word_length;

      for (j = 0; j < number_of_pre_delims; j++)
        ret[k++] = RAND_FROM_ARRAY(delimiters, delimiters_length);

      for (j = 0; j < token_length; j++)
        ret[k++] = RAND_CHAR('A', 'Z');

      for (j = 0; j < number_of_post_delims; j++)
        ret[k++] = RAND_FROM_ARRAY(delimiters, delimiters_length);
    }

  ret[number_of_tokens * word_length] = '\0';
  return ret;
}

void
tokens_param_create_normal(CreateTokensParam *tokens_param)
{
  tokens_param->min_token_length = 50;
  tokens_param->min_postfix_delimiters = 1;
  tokens_param->max_postfix_delimiters = 1;
  tokens_param->min_prefix_delimiters = 0;
  tokens_param->max_prefix_delimiters = 0;
}

void
tokens_param_create_with_long_delimiters(CreateTokensParam *tokens_param)
{
  tokens_param_create_normal(tokens_param);
  tokens_param->min_postfix_delimiters = 10;
  tokens_param->max_postfix_delimiters = 50;
  tokens_param->min_prefix_delimiters = 10;
  tokens_param->max_prefix_delimiters = 50;
}

void
tokens_param_create_long_tokens(CreateTokensParam *tokens_param)
{
  tokens_param_create_normal(tokens_param);
  tokens_param->min_token_length = 1000;
}

void
tokens_param_create_without_delimiters(CreateTokensParam *tokens_param)
{
  tokens_param_create_normal(tokens_param);
  tokens_param->min_postfix_delimiters = 0;
  tokens_param->max_postfix_delimiters = 0;
}

void
assert_if_tokenizers_results_not_equal(char *input,
                                       const char *delim,
                                       int expected_number_of_tokens,
                                       STRTOK_R_FUN test_tokenizer,
                                       STRTOK_R_FUN expected_tokenizer)
{
  char *test_token;
  char *expected_token;
  char *test_saveptr;
  char *expected_saveptr;
  char *test_input;
  char *expected_input;
  int number_of_tokens = 0;

  test_input = (char *)malloc(strlen(input)+1);
  expected_input = (char *)malloc(strlen(input)+1);
  strcpy(test_input, input);
  strcpy(expected_input, input);

  test_token = test_tokenizer(test_input, delim, &test_saveptr);
  expected_token = expected_tokenizer(expected_input, delim, &expected_saveptr);

  assert(mystrcmp(test_token, expected_token) == 0);

  while (test_token != NULL)
    {
      test_token = test_tokenizer(NULL, delim, &test_saveptr);
      expected_token = expected_tokenizer(NULL, delim, &expected_saveptr);
      assert(mystrcmp(test_token, expected_token) == 0);
      ++number_of_tokens;
    }

  assert(number_of_tokens == expected_number_of_tokens);

  free(test_input);
  free(expected_input);
}

double
measure_strtok_speed(STRTOK_R_FUN tokenizer_func)
{
  static const int number_of_input_tokens = 2e5;
  char *input;
  char *token;
  char *saveptr;
  int number_of_tokens = 0;
  CreateTokensParam tokens_param;
  struct timeval start, end;

  tokens_param_create_normal(&tokens_param);
  input = create_tokens(number_of_input_tokens, DELIMITERS, &tokens_param);

  start_time(&start);
  token = tokenizer_func(input, DELIMITERS, &saveptr);
  while (token != NULL)
    {
      token = tokenizer_func(NULL, DELIMITERS, &saveptr);
      ++number_of_tokens;
    }
  end_time(&end);
  assert(number_of_tokens == number_of_input_tokens);

  free(input);

  return elapsed_time(&start, &end);
}

void
assert_if_slow(double test_strtok_time, double ref_strtok_time, double max_speed_overhead_percent)
{
  double max_percent = 100.0 + max_speed_overhead_percent;
  double act_percent = (test_strtok_time / ref_strtok_time) * 100.0;

  fprintf(stderr,
          "test_strtok_time:%10.10f; ref_strtok_time:%10.10f\n\
          act_percent:%.2f%%; max_percent: %.2f%%\n",
          test_strtok_time,
          ref_strtok_time,
          act_percent, max_percent);

  if (act_percent > max_percent)
    {
      assert(fabs(act_percent-max_percent) < DOUBLE_EPS);
    }
}



void
test_strtok_with_generated_tokens(STRTOK_R_FUN tokenizer_func, CreateTokensParam *tokens_param)
{
  char *input;

  input = create_tokens(NUMBER_OF_TOKENS, DELIMITERS, tokens_param);
  assert_if_tokenizers_results_not_equal(input, DELIMITERS, NUMBER_OF_TOKENS, tokenizer_func, strtok_r);

  free(input);
}

void
test_strtok_with_normal_tokens(STRTOK_R_FUN tokenizer_func)
{
  CreateTokensParam tokens_param;
  tokens_param_create_normal(&tokens_param);
  test_strtok_with_generated_tokens(tokenizer_func, &tokens_param);
}

void
test_strtok_with_long_delimiters(STRTOK_R_FUN tokenizer_func)
{
  CreateTokensParam tokens_param;
  tokens_param_create_with_long_delimiters(&tokens_param);
  test_strtok_with_generated_tokens(tokenizer_func, &tokens_param);
}

void
test_strtok_without_delimiters(STRTOK_R_FUN tokenizer_func)
{
  CreateTokensParam tokens_param;
  char *input;

  tokens_param_create_without_delimiters(&tokens_param);

  input = create_tokens(NUMBER_OF_TOKENS, DELIMITERS, &tokens_param);
  assert_if_tokenizers_results_not_equal(input, DELIMITERS, 1, tokenizer_func, strtok_r);

  free(input);
}

void
test_strtok_with_long_tokens(STRTOK_R_FUN tokenizer_func)
{
  CreateTokensParam tokens_param;
  tokens_param_create_long_tokens(&tokens_param);
  test_strtok_with_generated_tokens(tokenizer_func, &tokens_param);
}

void
test_strtok_with_only_delimiters(STRTOK_R_FUN tokenizer_func)
{
  char *input = (char *)malloc(sizeof(DELIMITERS) + 1);
  strcpy(input, DELIMITERS);
  assert_if_tokenizers_results_not_equal(input, DELIMITERS, 0, tokenizer_func, strtok_r);

  free(input);
}

void
assert_if_tokenizer_concatenated_result_not_match(STRTOK_R_FUN tokenizer,
                                                  const char *delim,
                                                  const char *input,
                                                  const char *expected)
{
  char *token;
  char *saveptr;
  char *result = (char *)malloc(strlen(input)+1);
  char *raw_string = (char *)malloc(strlen(input)+1);
  char *result_ref = NULL;
  int result_idx = 0;
  int token_length;

  strcpy(raw_string, input);

  for (token=tokenizer(raw_string, delim, &saveptr); token; token=tokenizer(NULL, delim, &saveptr))
    {
      if (token)
        {
            token_length = strlen(token);
            memcpy(result+result_idx, token, token_length);
            result_idx += token_length;
        }
    }

  result[result_idx] = '\0';

  if (result_idx)
    result_ref = result;

  assert(mystrcmp(result_ref, expected) == 0);

  free(raw_string);
  free(result);
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

void
test_strtok(STRTOK_R_FUN tokenizer_func)
{
  test_strtok_with_normal_tokens(tokenizer_func);
  test_strtok_with_long_delimiters(tokenizer_func);
  test_strtok_without_delimiters(tokenizer_func);
  test_strtok_with_long_tokens(tokenizer_func);
  test_strtok_with_only_delimiters(tokenizer_func);
  test_strtok_with_literals(tokenizer_func);
}

void*
test_strtok_thread_fun(void *arg)
{
  STRTOK_R_FUN *tokenizer_func = (STRTOK_R_FUN*)arg;

  test_strtok(tokenizer_func);

  return NULL;
}

void
test_strtok_threaded(STRTOK_R_FUN tokenizer_func)
{
  pthread_t threads[THREAD_NUM] = {0};
  int i;

  for (i = 0; i < THREAD_NUM; i++)
    pthread_create(&threads[i], NULL, test_strtok_thread_fun, tokenizer_func);

  for (i = 0; i < THREAD_NUM; i++)
    pthread_join(threads[i], NULL);
}

void
test_strtok_speed(STRTOK_R_FUN test_tokenizer, STRTOK_R_FUN ref_tokenizer, double max_distance_percent)
{
  assert_if_slow(measure_strtok_speed(test_tokenizer),
                 measure_strtok_speed(ref_tokenizer),
                 max_distance_percent);
}

void
test_strtok_full(STRTOK_R_FUN tokenizer, char *name)
{
  fprintf(stderr, "%s\n", name);
  test_strtok(tokenizer);
  test_strtok_threaded(tokenizer);
  test_strtok_speed(tokenizer, strtok_r, MAX_STRTOK_SPEED_OVERHEAD_PERCENT);
}

void
setup()
{
  srand(time(NULL));
}

char*
dummy_strtok_r(char *input, const char *delim, char **saveptr)
{
  return strtok(input, delim);
}

int
main(int argc, char *argv[])
{
  setup();

  /* test if our expectations are met with the reference impl. */
  #ifdef HAVE_STRTOK_R_SUPPORT
  test_strtok_full(strtok_r, "strtok_r[reference impl.]");
  #else
  fprintf(stderr, "DUMMY strtok_r[simple strtok]\n");
  test_strtok(dummy_strtok_r);
  test_strtok_speed(dummy_strtok_r, dummy_strtok_r, MAX_STRTOK_SPEED_OVERHEAD_PERCENT);
  #endif

  test_strtok_full(mystrtok_r, "mystrtok_r[my implementation]");

  return EXIT_SUCCESS;
}
