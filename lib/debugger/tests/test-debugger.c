#include "debugger/debugger.h"
#include "apphook.h"

void
test_debugger(void)
{
  Debugger *debugger = debugger_new(configuration);
  debugger_free(debugger);
}

int
main()
{
  app_startup();
  configuration = cfg_new(VERSION_VALUE);
  test_debugger();
  cfg_free(configuration);
}
