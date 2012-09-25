#include "hostname.h"
#include <netdb.h>

static gchar local_hostname_fqdn[256];
static gchar local_hostname_short[256];

void
reset_cached_hostname(void)
{
  gchar *s;

  gethostname(local_hostname_fqdn, sizeof(local_hostname_fqdn) - 1);
  local_hostname_fqdn[sizeof(local_hostname_fqdn) - 1] = '\0';
  if (strchr(local_hostname_fqdn, '.') == NULL)
    {
      /* not fully qualified, resolve it using DNS or /etc/hosts */
      struct hostent *result = gethostbyname(local_hostname_fqdn);
      if (result)
        {
          strncpy(local_hostname_fqdn, result->h_name, sizeof(local_hostname_fqdn) - 1);
          local_hostname_fqdn[sizeof(local_hostname_fqdn) - 1] = '\0';
        }
    }
  /* NOTE: they are the same size, they'll fit */
  strcpy(local_hostname_short, local_hostname_fqdn);
  s = strchr(local_hostname_short, '.');
  if (s != NULL)
    *s = '\0';
}

void
getlonghostname(gchar *buf, gsize buflen)
{
  if (!local_hostname_fqdn[0])
    reset_cached_hostname();
  strncpy(buf, local_hostname_fqdn, buflen);
  buf[buflen - 1] = 0;
}

const char *
get_cached_longhostname()
{
  return local_hostname_fqdn;
}

const char *
get_cached_shorthostname()
{
  return local_hostname_short;
}
