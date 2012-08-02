#include <glib.h>
#include <windows.h>

typedef int (*SERVICE_MAIN)(void);
typedef void (*SERVICE_STOP_FUNCTION)(void);

int install_service(char * szName,
                         char * szDisplayName,
                         char * szDescription,
                         DWORD dwStartType,
                         char * szServiceArg,
                         char * szDependancy,
                         char * szAccount,
                         char * szPassword,
                         BOOL bInteractive);

int uninstall_service(char * szName);

int start_service_by_name(char * szName);
int stop_service_by_name(char * szName);

int start_service(char *szName,SERVICE_MAIN service_main,SERVICE_STOP_FUNCTION service_stop);

void set_minidump_hook();
