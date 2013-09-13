#include <libgen.h>

#include "template/templates.h"
#include "testutils.h"
#include "apphook.h"

#define assert_on_error_parse(on_error,expected)                        \
  do                                                                    \
    {                                                                   \
      gint r;                                                           \
                                                                        \
      assert_true(log_template_on_error_parse(on_error, &r),            \
                  "Parsing '%s' works", on_error);                      \
      assert_gint32(r, expected, "'%s' parses down to '%s'",            \
                    on_error, #expected);                               \
    } while(0)

static void
test_template_on_error(void)
{
  gint r;

  testcase_begin("Testing LogTemplate on-error parsing");

  assert_on_error_parse("drop-message", ON_ERROR_DROP_MESSAGE);
  assert_on_error_parse("silently-drop-message",
                        ON_ERROR_DROP_MESSAGE | ON_ERROR_SILENT);

  assert_on_error_parse("drop-property", ON_ERROR_DROP_PROPERTY);
  assert_on_error_parse("silently-drop-property",
                        ON_ERROR_DROP_PROPERTY | ON_ERROR_SILENT);

  assert_on_error_parse("fallback-to-string", ON_ERROR_FALLBACK_TO_STRING);
  assert_on_error_parse("silently-fallback-to-string",
                        ON_ERROR_FALLBACK_TO_STRING | ON_ERROR_SILENT);

  assert_false(log_template_on_error_parse("do-what-i-mean", &r),
               "The 'do-what-i-mean' strictness is not recognised");

  testcase_end();
}

int
main (void)
{
  app_startup();

  test_template_on_error();

  app_shutdown();

  return 0;
}
