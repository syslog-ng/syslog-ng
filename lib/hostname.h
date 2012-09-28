#include "syslog-ng.h"
#include "gsockaddr.h"

#include <sys/types.h>

void reset_cached_hostname(void);
void getlonghostname(gchar *buf, gsize buflen);

gchar *get_dnsname();
void apply_custom_domain(gchar *fqdn, int length, gchar *domain);

void set_custom_domain(const gchar *new_custom_domain);
const gchar *get_custom_domain();
void normalize_hostname(gchar *result, int *result_len, const gchar *hostname);
gchar *format_hostname(const gchar *hostname,gchar *domain,GlobalConfig *cfg);

const char *get_cached_longhostname();
const char *get_cached_shorthostname();
