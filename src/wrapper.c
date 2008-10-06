#include "syslog-ng.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

int 
main(int argc, char *argv[])
{
#ifdef ENV_LD_LIBRARY_PATH
  {
    gchar *cur_ldlibpath;
    gchar ldlibpath[512];
#if _AIX
    const gchar *ldlibpath_name = "LIBPATH";
#else    
    const gchar *ldlibpath_name = "LD_LIBRARY_PATH";
#endif

    cur_ldlibpath = getenv(ldlibpath_name);
    snprintf(ldlibpath, sizeof(ldlibpath), "%s=%s%s%s", ldlibpath_name, ENV_LD_LIBRARY_PATH, cur_ldlibpath ? ":" : "", cur_ldlibpath ? cur_ldlibpath : "");
    putenv(ldlibpath);
  }
#endif
  execv(PATH_SYSLOGNG, argv);
  return 127;
}
