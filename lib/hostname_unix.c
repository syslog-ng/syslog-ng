#include "hostname.h"
#include <netdb.h>

gchar *
get_dnsname()
{
  char local_hostname_fqdn[256] = {0};
  struct hostent *host = gethostbyname(get_cached_longhostname());
  gchar *result = NULL;
  if (host)
    {
       result = strdup(result->h_name);
    }
  return result;
}
