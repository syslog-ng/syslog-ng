#ifndef __RIACK_TESTS_H__
#define __RIACK_TESTS_H__

#include <check.h>

#define _ck_assert_float(X, O, Y) \
  ck_assert_msg((X) O (Y), \
                "Assertion '"#X#O#Y"' failed: "#X"==%f, "#Y"==%f", X, Y)

#define ck_assert_float_eq(X, Y) \
  _ck_assert_float(X, ==, Y)

#define ck_assert_errno(X, E)                                              \
  {                                                                        \
    int e = (X);                                                           \
    ck_assert_msg(e == -(E),                                               \
                  "Assertion '" #X " == -" #E "' failed: errno==%d (%s), " \
                  "expected==%d (%s)",                                     \
                  -e, (char *)strerror (-e), E, (char *)strerror (E));     \
  }

#endif
