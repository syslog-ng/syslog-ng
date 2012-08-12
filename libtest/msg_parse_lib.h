#ifndef MSG_PARSE_LIB_H_INCLUDED
#define MSG_PARSE_LIB_H_INCLUDED

#include "testutils.h"

#include "cfg.h"
#include "plugin.h"
#include "logmsg.h"

MsgFormatOptions parse_options;

void init_and_load_syslogformat_module();
void deinit_syslogformat_module();

void assert_log_messages_equal(LogMessage *log_message_a, LogMessage *log_message_b);

void assert_log_message_value(LogMessage *self, NVHandle handle, const gchar *expected_value);
void assert_log_message_has_tag(LogMessage *log_message, const gchar *tag_name);
void assert_log_messages_saddr(LogMessage *log_message_a, LogMessage *log_message_b);
void assert_structured_data_of_messages(LogMessage *log_message_a, LogMessage *log_message_b);
void assert_log_message_values_equal(LogMessage *log_message_a, LogMessage *log_message_b, NVHandle handle);

#endif
