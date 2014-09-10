/*
 * test_case.h
 *
 *  Created on: Nov 27, 2013
 *      Author: jviktor
 */

#ifndef TEST_CASE_H_
#define TEST_CASE_H_

typedef struct _TestCase TestCase;

struct _TestCase {
  void (*setup)(TestCase *self);
  void (*run)(TestCase *self);
  void (*teardown)(TestCase *self);
};


static inline void
run_test_case(TestCase *tc)
{
  if (tc->setup) tc->setup(tc);
  tc->run(tc);
  if (tc->teardown) tc->teardown(tc);
}

#define RUN_TESTCASE(class_name, test_case_prefix, test_function) { \
  class_name *tc = g_new0(class_name, 1); \
  tc->super.setup = test_case_prefix ## _setup; \
  tc->super.teardown = test_case_prefix ## _teardown; \
  tc->super.run = test_function;\
  run_test_case(&tc->super); \
  g_free(tc); \
}

#endif /* TEST_CASE_H_ */
