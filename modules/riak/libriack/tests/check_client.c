#include <riack/riack.h>

START_TEST (test_riack_client_new_and_free)
{
  riack_client_t *client;

  client = riack_client_new ();
  ck_assert (client != NULL);

  ck_assert_errno (riack_client_free (NULL), EINVAL);
  ck_assert_errno (riack_client_free (client), ENOTCONN);
}
END_TEST

START_TEST (test_riack_client_connect_and_disconnect)
{
  riack_client_t *client;

  ck_assert_errno (riack_client_disconnect (NULL), ENOTCONN);

  client = riack_client_new ();

  ck_assert_errno (riack_client_disconnect (client), ENOTCONN);

  if (network_tests_enabled ())
    {
      /* Connect with default options! */
      ck_assert_errno (riack_client_connect (client, RIACK_CONNECT_OPTION_NONE),
                       0);
      ck_assert_errno (riack_client_disconnect (client), 0);

      /* Connect to "localhost", instead of "127.0.0.1" */
      ck_assert_errno (riack_client_connect
                       (client,
                        RIACK_CONNECT_OPTION_HOST, "localhost",
                        RIACK_CONNECT_OPTION_NONE), 0);
      riack_client_disconnect (client);

      /* Connect to 8087, explicitly. */
      ck_assert_errno (riack_client_connect
                       (client,
                        RIACK_CONNECT_OPTION_PORT, 8087,
                        RIACK_CONNECT_OPTION_NONE), 0);
      riack_client_disconnect (client);
    }

  riack_client_free (client);
}
END_TEST

START_TEST (test_riack_client_send_putreq)
{
  riack_client_t *client;
  riack_put_req_t *putreq;
  riack_content_t *content;
  riack_message_t *message;


  client = riack_client_new ();

  

  if (network_tests_enabled ())
    {
      /* Connect with default options! */
      
      ck_assert_errno (riack_client_connect
                       (client,
                        RIACK_CONNECT_OPTION_HOST, "localhost",
                        RIACK_CONNECT_OPTION_NONE), 0);
                        
                        
      
      content = riack_content_new ();
      putreq = riack_req_put_new ();

      ck_assert_errno
           (riack_content_set (content,
                        RIACK_CONTENT_FIELD_VALUE, "some-value", -1,
                        RIACK_CONTENT_FIELD_CONTENT_TYPE, "text/plain", -1,
                        RIACK_CONTENT_FIELD_CONTENT_ENCODING, "none", -1,
                        RIACK_CONTENT_FIELD_CHARSET, "utf8", -1,
                        RIACK_CONTENT_FIELD_NONE),
     0);
     
      ck_assert_errno
            (riack_req_put_set (putreq,
                         RIACK_REQ_PUT_FIELD_BUCKET, "new-bucket",
                         RIACK_REQ_PUT_FIELD_BUCKET_TYPE, "set",
                         RIACK_REQ_PUT_FIELD_KEY, "030620151900",
                         RIACK_REQ_PUT_FIELD_CONTENT, content,
                         RIACK_REQ_PUT_FIELD_NONE),
                         0);
                         
      message = riack_putreq_serialize(putreq);
      
      ck_assert_errno
            (riack_client_send (client,
                                message),
                                0);
      ck_assert_errno
            (riack_client_recv (client),
                                0);
                                
 
      
  
  riack_req_put_free (putreq);
  
      riack_client_disconnect (client);

      
    }

  riack_client_free (client);
}
END_TEST

START_TEST (test_riack_client_send_dtupdatereq)
{
  riack_client_t *client;
  
  riack_message_t *message;


  client = riack_client_new ();

  

  if (network_tests_enabled ())
    {
      /* Connect with default options! */
      
      ck_assert_errno (riack_client_connect
                       (client,
                        RIACK_CONNECT_OPTION_HOST, "localhost",
                        RIACK_CONNECT_OPTION_NONE), 0);
                        
                        
      
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
  
 

  
  
  
  
                         
      message = riack_dtupdatereq_serialize(dtupdatereq);
      
      ck_assert_errno
            (riack_client_send (client,
                                message),
                                0);
     ck_assert_errno
            (riack_client_recv (client),
                                0);
                                
 
      
  
  riack_req_dt_update_free (dtupdatereq);
  
      riack_client_disconnect (client);

      
    }

  riack_client_free (client);
}
END_TEST

START_TEST (test_riack_client_send_dtupdatereq_bulk)
{
  riack_client_t *client;
  
  riack_message_t *message;


  client = riack_client_new ();

  

  if (network_tests_enabled ())
    {
      /* Connect with default options! */
      
      ck_assert_errno (riack_client_connect
                       (client,
                        RIACK_CONNECT_OPTION_HOST, "localhost",
                        RIACK_CONNECT_OPTION_NONE), 0);
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
                            RIACK_REQ_DT_UPDATE_FIELD_BUCKET, "setbucket", 
                            RIACK_REQ_DT_UPDATE_FIELD_BUCKET_TYPE, "sets", 
                        RIACK_REQ_DT_UPDATE_FIELD_KEY, "new key",
                        RIACK_REQ_DT_UPDATE_FIELD_DT_OP, dtop,
                        RIACK_REQ_DT_UPDATE_FIELD_NONE),
     0);
     
   message = riack_dtupdatereq_serialize(dtupdatereq);
      
       ck_assert_errno
            (riack_client_send (client,
                                message),
                                0);
     ck_assert_errno
            (riack_client_recv (client),
                                0);                 
                        
      
 
      riack_req_dt_update_free (dtupdatereq);
  
      riack_client_disconnect (client);

      
    }

  riack_client_free (client);
}
END_TEST
  


static TCase *
test_riack_client (void)
{
  TCase *test_client;

  test_client = tcase_create ("Client");
  tcase_add_test (test_client, test_riack_client_new_and_free);
  tcase_add_test (test_client, test_riack_client_connect_and_disconnect);
  tcase_add_test (test_client, test_riack_client_send_putreq);
  tcase_add_test (test_client, test_riack_client_send_dtupdatereq);
  tcase_add_test (test_client, test_riack_client_send_dtupdatereq_bulk);

  return test_client;
}
