#include "dnscache.h"
#include "apphook.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

int
main()
{
  int i;

  app_startup();
  dns_cache_init();
  dns_cache_set_params(50000, 2, 1, NULL);

  for (i = 0; i < 10000; i++)
    {
      guint32 ni = htonl(i);

      dns_cache_store(FALSE, AF_INET, (void *) &ni, "hostname");
    }

  for (i = 0; i < 10000; i++)
    {
      guint32 ni = htonl(i);
      const gchar *hn = NULL;

      if (!dns_cache_lookup(AF_INET, (void *) &ni, &hn) || strcmp(hn, "hostname") != 0)
        {
          fprintf(stderr, "hmmm cache forgot the hostname, i=%d, hn=%s\n", i, hn);
          return 1;
        }

    }

  sleep(3);

  for (i = 0; i < 10000; i++)
    {
      guint32 ni = htonl(i);
      const gchar *hn = NULL;

      if (dns_cache_lookup(AF_INET, (void *) &ni, &hn))
        {
          fprintf(stderr, "hmmm cache did not forget the hostname, i=%d\n", i);
          return 1;
        }
    }

  app_shutdown();
  return 0;
}
