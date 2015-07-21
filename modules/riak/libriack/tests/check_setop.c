#include <riack/riack.h>

START_TEST (test_riack_setop_new_and_free)
{
  riack_setop_t *setop;
  setop = riack_setop_new();
  ck_assert (setop != NULL);
  riack_setop_free (setop);
}

END_TEST

START_TEST (test_riack_setop_set)
{
  riack_setop_t *setop;
  setop = riack_setop_new();
  ck_assert_errno
    (riack_setop_set(setop,
                  RIACK_SETOP_FIELD_ADD, "addsomething",
                  RIACK_SETOP_FIELD_NONE),
                  0);
  ck_assert_str_eq(setop->adds[0].data, "addsomething");
  riack_setop_free(setop);
}
END_TEST
     
START_TEST (test_riack_setop_set_bulk)
{
  
  riack_setop_t *setop;
  setop = riack_setop_new();
  ck_assert_errno
      (riack_setop_set(setop,
                  RIACK_SETOP_FIELD_BULK_ADD, 0,
                  "first bulk",
                  RIACK_SETOP_FIELD_NONE),
                  0);
                    
  ck_assert_errno
      (riack_setop_set(setop,
                  RIACK_SETOP_FIELD_BULK_ADD, 1,
                  "second bulk",
                  RIACK_SETOP_FIELD_NONE),
                  0);
  ck_assert_str_eq(setop->adds[0].data, "first bulk");
  ck_assert_str_eq(setop->adds[1].data, "second bulk");
  riack_setop_free (setop);    
}
END_TEST
          

static TCase *
test_riack_setop (void)
{
  TCase *test_setop;

  test_setop = tcase_create ("setop");
  tcase_add_test (test_setop, test_riack_setop_new_and_free);
  tcase_add_test (test_setop, test_riack_setop_set);
  tcase_add_test (test_setop, test_riack_setop_set_bulk);

  return test_setop;
}
