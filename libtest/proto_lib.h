#ifndef PROTO_LIB_H_INCLUDED
#define PROTO_LIB_H_INCLUDED

#include "testutils.h"
#include "logproto-server.h"

extern LogProtoServerOptions proto_server_options;


#define log_proto_testcase_begin(desc, ...) 			\
  do                                          			\
    {                                         			\
      testcase_begin(desc, ##__VA_ARGS__);    			\
      log_proto_server_options_defaults(&proto_server_options);	\
    }                                         			\
  while (0)

#define log_proto_testcase_end()				\
  do								\
    {								\
      log_proto_server_options_destroy(&proto_server_options);	\
      testcase_end();						\
    }								\
  while (0)


void assert_proto_server_status(LogProtoServer *proto, LogProtoStatus status, LogProtoStatus expected_status);
void assert_proto_server_fetch(LogProtoServer *proto, const gchar *expected_msg, gssize expected_msg_len);
void assert_proto_server_fetch_single_read(LogProtoServer *proto, const gchar *expected_msg, gssize expected_msg_len);
void assert_proto_server_fetch_failure(LogProtoServer *proto, LogProtoStatus expected_status, const gchar *error_message);
void assert_proto_server_fetch_ignored_eof(LogProtoServer *proto);

LogProtoServer *construct_server_proto_plugin(const gchar *name, LogTransport *transport);

void init_proto_tests(void);
void deinit_proto_tests(void);

static inline LogProtoServerOptions *
get_inited_proto_server_options(void)
{
  log_proto_server_options_init(&proto_server_options, configuration);
  return &proto_server_options;
}


#endif
