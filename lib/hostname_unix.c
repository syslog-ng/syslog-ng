#include "hostname.h"
#include <netdb.h>

gchar *
get_dnsname()
{
  struct hostent *host = NULL;
  G_LOCK(resolv_lock);
  host = gethostbyname(get_cached_longhostname());
  gchar *result = NULL;
  if (host)
    {
       result = strdup(host->h_name);
    }
  G_UNLOCK(resolv_lock);
  return result;
}
