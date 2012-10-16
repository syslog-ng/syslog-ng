#include "hostname.h"
#include <netdb.h>

gchar *
get_dnsname()
{
  struct hostent *host = gethostbyname(get_cached_longhostname());
  gchar *result = NULL;
  if (host)
    {
       result = strdup(host->h_name);
    }
  return result;
}
