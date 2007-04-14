#include "dnscache.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

int 
main()
{
  int i;
  
  dns_cache_init(50000, 10, 1);
  
  for (i = 0; i < 10000; i++)
    {
      guint32 ni = htonl(i);
      
      dns_cache_store(AF_INET, (void *) &ni, "hostname");
    }
  
  for (i = 0; i < 10000; i++)
    {
      guint32 ni = htonl(i);
      const gchar *hn = NULL;
      
      if (!dns_cache_lookup(AF_INET, (void *) &ni, &hn) || strcmp(hn, "hostname") != 0)
        {
          fprintf(stderr, "hmmm cache forgot the hostname, i=%d, hn=%s\n", i, hn);
          exit(1);
        }
      
    }
    
  sleep(11);

  for (i = 0; i < 10000; i++)
    {
      guint32 ni = htonl(i);
      const gchar *hn = NULL;
      
      if (dns_cache_lookup(AF_INET, (void *) &ni, &hn))
        {
          fprintf(stderr, "hmmm cache did not forget the hostname, i=%d\n", i);
        }
    }
  
  dns_cache_destroy();
  return 0;
}
