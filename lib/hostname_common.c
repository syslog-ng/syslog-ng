#include "hostname.h"

static gchar local_hostname_fqdn[256];
static gchar local_hostname_short[256];
static gchar *custom_domain;

void
getlonghostname(gchar *buf, gsize buflen)
{
  if (!local_hostname_fqdn[0])
    reset_cached_hostname();
  strncpy(buf, local_hostname_fqdn, buflen);
  buf[buflen - 1] = 0;
}

void
apply_custom_domain(gchar *fqdn, int length, gchar *domain)
{
  if (domain)
    {
      gchar *p = strchr(fqdn,'.');
      int index;
      int rest_length;
      if (p)
        {
          p++;
          index = (p - fqdn);
        }
      else
        {
          index = strlen(fqdn);
          p = fqdn + index;
          p[0] = '.';
          p++;
          index++;
        }
      rest_length = MIN(strlen(domain),length - index - 1);
      strncpy(p, domain,  rest_length);
      p[rest_length] = '\0';
    }
}

void reset_cached_hostname(void)
{
  gchar *s;

  gethostname(local_hostname_fqdn, sizeof(local_hostname_fqdn) - 1);
  local_hostname_fqdn[sizeof(local_hostname_fqdn) - 1] = '\0';
  if (strchr(local_hostname_fqdn, '.') == NULL)
    {
      /* not fully qualified, resolve it using DNS */
      char *result = get_dnsname();
      if (result)
        {
          strncpy(local_hostname_fqdn, result, sizeof(local_hostname_fqdn) - 1);
          local_hostname_fqdn[sizeof(local_hostname_fqdn) - 1] = '\0';
          free(result);
        }
    }
  apply_custom_domain(local_hostname_fqdn,sizeof(local_hostname_fqdn),custom_domain);
  /* NOTE: they are the same size, they'll fit */
  strcpy(local_hostname_short, local_hostname_fqdn);
  s = strchr(local_hostname_short, '.');
  if (s != NULL)
    *s = '\0';
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

void set_custom_domain(const gchar *new_custom_domain)
{
  g_free(custom_domain);
  custom_domain = g_strdup(new_custom_domain);
  return;
}

const gchar *get_custom_domain()
{
  return custom_domain;
}
