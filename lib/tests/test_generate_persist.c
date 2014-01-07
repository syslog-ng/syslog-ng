#include "cfg.h"
#include "testutils.h"
#include "persist-state.h"
#include "libtest/persist_lib.h"
#include "apphook.h"

gboolean called;

void test_generate_callback_func(PersistState* state, gpointer userdata)
{
   gchar* str = (gchar*) userdata;
   called = TRUE;
   assert_string(str, "test", "Expected userdata parameter is not equal");
};

void test_generate_persist(void)
{
   called = FALSE;
   PersistState *state = clean_and_create_persist_state_for_test("test_generate_persist.persist");

   cfg_register_generate_persist_callback(test_generate_callback_func, "test");
   cfg_generate_persist_file(state);
   assert_true(called, "Callback is not called!");

   cancel_and_destroy_persist_state(state);
};

int main()
{
   app_startup();
   test_generate_persist();
   app_shutdown();
};
