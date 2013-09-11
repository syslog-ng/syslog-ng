#include "stomp.h"
#include "testutils.h"

void
assert_stomp_header(stomp_frame* frame, char* key, char* value)
{
  char* myvalue = g_hash_table_lookup(frame->headers, key);

  assert_string(myvalue, value, "Stomp header assertion failed!");
}

void
assert_stomp_command(stomp_frame* frame, char* command)
{
  assert_string(frame->command, command, "Stomp command assertion failed");
}

void
assert_stomp_body(stomp_frame* frame, char* body)
{
  assert_string(frame->body, body, "Stomp body assertion failed");
}

void
test_only_command()
{
  stomp_frame frame;

  stomp_parse_frame(g_string_new("CONNECTED\n\n"), &frame);
  assert_stomp_command(&frame, "CONNECTED");
  stomp_frame_deinit(&frame);
}

void
test_command_and_data()
{
  stomp_frame frame;

  stomp_parse_frame(g_string_new("CONNECTED\n\nalmafa"), &frame);
  assert_stomp_command(&frame, "CONNECTED");
  assert_stomp_body(&frame, "almafa");
  stomp_frame_deinit(&frame);
};

void
test_command_and_header_and_data()
{
  stomp_frame frame;

  stomp_parse_frame(g_string_new("CONNECTED\nheader_name:header_value\n\nbelafa"), &frame);
  assert_stomp_command(&frame, "CONNECTED");
  assert_stomp_header(&frame, "header_name", "header_value");
  assert_stomp_body(&frame, "belafa");
  stomp_frame_deinit(&frame);
};

void
test_command_and_header()
{
  stomp_frame frame;

  stomp_parse_frame(g_string_new("CONNECTED\nsession:ID:tusa-38077-1378214843533-2:1\n"), &frame);
  assert_stomp_command(&frame, "CONNECTED");
  assert_stomp_header(&frame, "session", "ID:tusa-38077-1378214843533-2:1");
  stomp_frame_deinit(&frame);
};

void
test_generate_gstring_from_frame()
{
  stomp_frame frame;
  GString* actual;

  stomp_frame_init(&frame, "SEND", sizeof("SEND"));
  stomp_frame_add_header(&frame, "header_name", "header_value");
  stomp_frame_set_body(&frame, "body", sizeof("body"));
  actual = create_gstring_from_frame(&frame);
  assert_string(actual->str, "SEND\nheader_name:header_value\n\nbody", "Generated stomp frame does not match");
};

int
main(void)
{
  test_only_command();
  test_command_and_data();
  test_command_and_header_and_data();
  test_command_and_header();
  test_generate_gstring_from_frame();
}
