#include "testutils.h"
#include "misc.h"
#include <stdio.h>

void
test_data_to_hex()
{
  guint8 input[] = {0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xa,0xb,0xc,0xd,0xe,0xf,0x10,0x20,0xff,0};
  gchar *expected_output = "01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 10 20 FF";
  gchar *output = data_to_hex_string(input,strlen(input));
  assert_string(output,expected_output,"Bad working!\n");
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  test_data_to_hex();
  return 0;
}
