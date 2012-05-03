#include <stdlib.h>

#include "testutils.h"

int
main(int argc, char *argv[])
{
  init_testlogging(argc, argv);

  deinit_testlogging();
  return 0;
}
