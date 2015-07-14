#include <check.h>

#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "tests.h"

static int
network_tests_enabled (void)
{
  struct addrinfo hints;
  struct addrinfo *res, *rp;
  int fd = -1, s;

  memset (&hints, 0, sizeof (struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  s = getaddrinfo ("127.0.0.1", "8087", &hints, &res);
  if (s != 0)
    return 0;

  for (rp = res; rp != NULL; rp = rp->ai_next)
    {
      fd = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
      if (fd == -1)
        continue;

      if (connect (fd, rp->ai_addr, rp->ai_addrlen) != -1)
        break;

      close (fd);
    }
  freeaddrinfo (res);

  if (rp == NULL)
    return 0;

  if (fd != -1)
    close (fd);

  return 1;
}

#include "check_client.c"
#include "check_content.c"
#include "check_putreq.c"
#include "check_dtupdatereq.c"
#include "check_setop.c"
#include "check_dtop.c"

int
main (void)
{
  Suite *suite;
  SRunner *runner;

  int nfailed;

  suite = suite_create ("Riack tests");

  suite_add_tcase (suite, test_riack_client ());
  suite_add_tcase (suite, test_riack_content ());
  suite_add_tcase (suite, test_riack_putreq ());
  suite_add_tcase (suite, test_riack_dtupdatereq ());
  suite_add_tcase (suite, test_riack_setop ());
  suite_add_tcase (suite, test_riack_dtop ());

  runner = srunner_create (suite);

  srunner_run_all (runner, CK_ENV);
  nfailed = srunner_ntests_failed (runner);
  srunner_free (runner);

  return (nfailed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
