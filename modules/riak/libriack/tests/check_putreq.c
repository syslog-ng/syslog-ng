#include <riack/riack.h>

START_TEST (test_riack_putreq_new_and_free)
{
  riack_put_req_t *putreq;

  putreq = riack_req_put_new ();
  ck_assert (putreq != NULL);
  riack_req_put_free (putreq);
}
END_TEST

START_TEST (test_riack_putreq_set)
{
  riack_put_req_t *putreq;
  riack_content_t *content;
  
  content = riack_content_new ();
  putreq = riack_req_put_new ();

  ck_assert_errno
    (riack_req_put_set
     (NULL, RIACK_CONTENT_FIELD_NONE),
     EINVAL);

  ck_assert_errno
    (riack_content_set(content,
                        RIACK_CONTENT_FIELD_VALUE, "some-value", -1,
                        RIACK_CONTENT_FIELD_CONTENT_TYPE, "text/plain", -1,
                        RIACK_CONTENT_FIELD_CONTENT_ENCODING, "none", -1,
                        RIACK_CONTENT_FIELD_CHARSET, "utf8", -1,
                        RIACK_CONTENT_FIELD_NONE),
                        0);
     
  ck_assert_errno
     (riack_req_put_set(putreq,
                         RIACK_REQ_PUT_FIELD_BUCKET, "new-bucket",
                         RIACK_REQ_PUT_FIELD_BUCKET_TYPE, "set",
                         RIACK_REQ_PUT_FIELD_KEY, "030620151900",
                         RIACK_REQ_PUT_FIELD_CONTENT, content,
                         RIACK_REQ_PUT_FIELD_NONE),
                         0);
  
  ck_assert_str_eq (putreq->bucket.data, "new-bucket");
  ck_assert_str_eq (putreq->type.data , "set");
  ck_assert_str_eq (putreq->key.data, "030620151900");
  ck_assert_str_eq (putreq->content->value.data, "some-value");
  ck_assert_str_eq (putreq->content->content_type.data, "text/plain");
  ck_assert_str_eq (putreq->content->content_encoding.data, "none");
  ck_assert_str_eq (putreq->content->charset.data, "utf8");
  
  riack_req_put_free (putreq);
}
END_TEST

static TCase *
test_riack_putreq (void)
{
  TCase *test_putreq;

  test_putreq = tcase_create ("Content");
  tcase_add_test (test_putreq, test_riack_putreq_new_and_free);
  tcase_add_test (test_putreq, test_riack_putreq_set);

  return test_putreq;
}
