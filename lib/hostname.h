#include "syslog-ng.h"
#include "gsockaddr.h"

#include <sys/types.h>

void reset_cached_hostname(void);
void getlonghostname(gchar *buf, gsize buflen);

const char *get_cached_longhostname();
const char *get_cached_shorthostname();
