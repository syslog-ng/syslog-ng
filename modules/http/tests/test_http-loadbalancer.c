
#include <criterion/criterion.h>
#include "syslog-ng.h"

Test(http_loadbalancer, foo)
{
  cr_assert(TRUE);
}

void
setup(void)
{
}

void
teardown(void)
{
}

TestSuite(http_loadbalancer, .init = setup, .fini = teardown);
