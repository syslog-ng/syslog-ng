#include "str-format.h"
#include "testutils.h"

void test_format_hex_string__single_byte__perfect()
{
   gchar expected_output[2] = "40";
   gchar output[2];
   gchar input[1] = "@";
   //Act
   format_hex_string(input, sizeof(input), output, sizeof(output));
   //Assert
   assert_nstring(output, sizeof(output), expected_output, sizeof(expected_output), "format_hex_string output does not match!", NULL);
}

void test_format_hex_string__two_bytes__perfect()
{
   gchar expected_output[4] = "4041";
   gchar output[4];
   gchar input[2] = "@A";
   //Act
   format_hex_string(input, sizeof(input), output, sizeof(output));
   //Assert
   assert_nstring(output, sizeof(output), expected_output, sizeof(expected_output), "format_hex_string output does not match with two bytes!", NULL);
}

void test_format_hex_string_with_delimiter__single_byte__perfect()
{
   gchar expected_output[2] = "40";
   gchar output[2];
   gchar input[1] = "@";
   //Act
   format_hex_string_with_delimiter(input, sizeof(input), output, sizeof(output), ' ');
   //Assert
   assert_nstring(output, sizeof(output), expected_output, sizeof(expected_output), "format_hex_string_with_delimiter output does not match!", NULL);
}

void test_format_hex_string_with_delimiter__two_bytes__perfect()
{
   gchar expected_output[5] = "40 41";
   gchar output[5];
   gchar input[2] = "@A";
   //Act
   format_hex_string_with_delimiter(input, sizeof(input), output, sizeof(output), ' ');
   //Assert
   assert_nstring(output, sizeof(output), expected_output, sizeof(expected_output), "format_hex_string_with_delimiter output does not match in case of two bytes!", NULL);
}

int main()
{
   test_format_hex_string__single_byte__perfect();
   test_format_hex_string__two_bytes__perfect();
   test_format_hex_string_with_delimiter__single_byte__perfect();
   test_format_hex_string_with_delimiter__two_bytes__perfect();
}
