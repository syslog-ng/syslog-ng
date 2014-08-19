#include "template_lib.h"
#include "apphook.h"
#include "plugin.h"

void
test_hash(void)
{
#if ENABLE_SSL
  assert_template_format("$(sha1 foo)", "0beec7b5ea3f0fdbc95d0dd47f3c5bc275da8a33");
  assert_template_format("$(sha1 bar)", "62cdb7020ff920e5aa642c3d4066950dd1f01f4d");
  assert_template_format("$(md5 foo)", "acbd18db4cc2f85cedef654fccc4a4d8");
  assert_template_format("$(hash foo)", "acbd18db4cc2f85cedef654fccc4a4d8");
  assert_template_format("$(md4 foo)", "0ac6700c491d70fb8650940b1ca1e4b2");
  assert_template_format("$(sha256 foo)", "2c26b46b68ffc68ff99b453c1d30413413422d706483bfa0f98a5e886266e7ae");
  assert_template_format("$(sha512 foo)", "f7fbba6e0636f890e56fbbf3283e524c6fa3204ae298382d624741d0dc6638326e282c41be5e4254d8820772c5518a2c5a8c0c7f7eda19594a7eb539453e1ed7");
  assert_template_failure("$(sha1)", "$(hash) parsing failed, invalid number of arguments");
  assert_template_format("$(sha1 --length 5 foo)", "0beec");
  assert_template_format("$(sha1 --length 0 foo)", "0beec7b5ea3f0fdbc95d0dd47f3c5bc275da8a33");
  assert_template_format("$(sha1 -l 5 foo)", "0beec");
  assert_template_failure("$(sha1 --length 5)", "$(hash) parsing failed, invalid number of arguments");
  assert_template_failure("$(sha1 --length -1 foo)", "$(hash) parsing failed, negative value for length");
  assert_template_failure("$(sha1 --length invalid_length_specification foo)", "Cannot parse integer value 'invalid_length_specification' for --length");
  assert_template_format("$(sha1 --length 99999 foo)", "0beec7b5ea3f0fdbc95d0dd47f3c5bc275da8a33");
  assert_template_format("$(sha1 foo bar)", "8843d7f92416211de9ebb963ff4ce28125932878");
  assert_template_format("$(sha1 \"foo bar\")", "3773dea65156909838fa6c22825cafe090ff8030");
  assert_template_format("$(md5 $(sha1 foo) bar)", "196894290a831b2d2755c8de22619a97");
#endif
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  app_startup();
  init_template_tests();
  plugin_load_module("cryptofuncs", configuration, NULL);

  test_hash();

  deinit_template_tests();
  app_shutdown();
}
