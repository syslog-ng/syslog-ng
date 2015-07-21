#include <riack/riack.h>

START_TEST (test_riack_dtupdatereq_new_and_free)
{
  riack_dt_update_req_t *dtupdatereq;

  dtupdatereq = riack_req_dt_update_new ();
  ck_assert (dtupdatereq != NULL);
  riack_req_dt_update_free (dtupdatereq);
}
END_TEST

START_TEST (test_riack_req_dt_update_set)
{
  riack_dt_update_req_t *dtupdatereq;
  riack_dt_op_t *dtop;
  riack_setop_t *setop;
  
  dtupdatereq = riack_req_dt_update_new ();
  dtop = riack_dt_op_new();
  setop = riack_setop_new();
  

  ck_assert_errno
    (riack_req_dt_update_set
     (NULL, RIACK_REQ_DT_UPDATE_FIELD_NONE),
     EINVAL);
     
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

  ck_assert_errno
    (riack_req_dt_update_set (dtupdatereq,
                        RIACK_REQ_DT_UPDATE_FIELD_BUCKET, "setbucket", 
                        RIACK_REQ_DT_UPDATE_FIELD_BUCKET_TYPE, "sets", 
                        RIACK_REQ_DT_UPDATE_FIELD_KEY, "new key",
                        RIACK_REQ_DT_UPDATE_FIELD_DT_OP, dtop,
                        RIACK_REQ_DT_UPDATE_FIELD_NONE),
     0);
     
   

  
  ck_assert_str_eq (dtupdatereq->bucket.data, "setbucket");
  ck_assert_str_eq (dtupdatereq->type.data , "sets");
  ck_assert_str_eq (dtupdatereq->key.data , "new key");
  ck_assert_str_eq (dtupdatereq->op->set_op->adds[0].data, "addsomething");
  
 

  
  riack_req_dt_update_free (dtupdatereq);
  
  
  
}
END_TEST

START_TEST (test_riack_req_dt_update_set_bulk)
{

  
  riack_dt_update_req_t *dtupdatereq;
  riack_dt_op_t *dtop;
  riack_setop_t *setop;
  
  dtupdatereq = riack_req_dt_update_new ();
  dtop = riack_dt_op_new();
  
  setop = riack_setop_new();
  

  ck_assert_errno
    (riack_req_dt_update_set
     (NULL, RIACK_REQ_DT_UPDATE_FIELD_NONE),
     EINVAL);
     

    ck_assert_errno
      (riack_setop_set (setop,
                  RIACK_SETOP_FIELD_BULK_ADD, 0,
                  "first bulk",
                  RIACK_SETOP_FIELD_NONE),
                  0);
                  
   ck_assert_errno
      (riack_setop_set (setop,
                  RIACK_SETOP_FIELD_BULK_ADD, 1,
                  "second bulk",
                  RIACK_SETOP_FIELD_NONE),
                  0);
                  
  ck_assert_errno
    (riack_dt_op_set (dtop,
                RIACK_DT_OP_FIELD_SETOP, setop,
                RIACK_DT_OP_FIELD_NONE),
                0);

  ck_assert_errno
    (riack_req_dt_update_set (dtupdatereq,
                        
                        RIACK_REQ_DT_UPDATE_FIELD_BUCKET, "setbucket1", 
                        RIACK_REQ_DT_UPDATE_FIELD_BUCKET_TYPE, "sets", 
                        RIACK_REQ_DT_UPDATE_FIELD_KEY, "new key1",
                        RIACK_REQ_DT_UPDATE_FIELD_DT_OP, dtop,
                        RIACK_REQ_DT_UPDATE_FIELD_NONE),
     0);
     
   
  
  
  
  
 

  
  riack_req_dt_update_free (dtupdatereq);
  
  
  
}
END_TEST

static TCase *
test_riack_dtupdatereq (void)
{
  TCase *test_dtupdatereq;

  test_dtupdatereq = tcase_create ("dtupdate");
  tcase_add_test (test_dtupdatereq, test_riack_dtupdatereq_new_and_free);
  tcase_add_test (test_dtupdatereq, test_riack_req_dt_update_set);
  tcase_add_test (test_dtupdatereq, test_riack_req_dt_update_set_bulk);

  return test_dtupdatereq;
}
