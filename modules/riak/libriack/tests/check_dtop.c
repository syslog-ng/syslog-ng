#include <riack/riack.h>

START_TEST (test_riack_dt_op_new_and_free)
{
  riack_dt_op_t *dtop;
  dtop = riack_dt_op_new();
  ck_assert (dtop != NULL);
  riack_dt_op_free (dtop);
}
END_TEST

START_TEST (test_riack_dt_op_set)
{
  
  riack_dt_op_t *dtop;
  riack_setop_t *setop;
  
  
  dtop = riack_dt_op_new();
  setop = riack_setop_new();
  

     
  ck_assert_errno
    (riack_setop_set (setop,
                  RIACK_SETOP_FIELD_ADD, "addsomething",
                  RIACK_SETOP_FIELD_NONE),
                  0);
                  
  ck_assert_errno
    (riack_dt_op_set (dtop,
                RIACK_DT_OP_FIELD_SETOP, setop,
                RIACK_DT_OP_FIELD_NONE),
                0);

  
  
 

  
  
  riack_dt_op_free (dtop);
  
  
  
}
END_TEST

static TCase *
test_riack_dtop (void)
{
  TCase *test_dtop;

  test_dtop = tcase_create ("dtop");
  tcase_add_test (test_dtop, test_riack_dt_op_new_and_free);
  tcase_add_test (test_dtop, test_riack_dt_op_set);

  return test_dtop;
}
