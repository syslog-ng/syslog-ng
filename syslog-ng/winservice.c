#include <stdio.h>
#include <signal.h>
#include "winservice.h"
#include "minidump.h"

SERVICE_STATUS ServiceStatus;
SERVICE_STATUS_HANDLE hStatus;

static gchar *g_service_name = NULL;
static SERVICE_MAIN g_service_main_function = NULL;
static SERVICE_STOP_FUNCTION g_service_stop_function = NULL;
FILE *global_service_logfile = NULL;


int
install_service(char * szName, char * szDisplayName, char * szDescription,
                  DWORD dwStartType, char * szServiceArg, char * szDependancy,
                  char * szAccount, char * szPassword, BOOL bInteractive)
{
  /* Open service control manager */
  SC_HANDLE hSCManager;
  SC_HANDLE hService;
  SERVICE_DESCRIPTION description;
  gchar m_name[MAX_PATH] = {0};
  gchar *szPath;

  DWORD dwType = SERVICE_WIN32_OWN_PROCESS;

  hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);

  if (hSCManager == NULL)
    {
      fprintf(stderr,"Install() : SCManager == NULL : Error = 0x%08lx\n", GetLastError());
      return 1;
    }

  /* Interactive service */
  if (bInteractive != FALSE)
    {
      dwType |= SERVICE_INTERACTIVE_PROCESS;
    }

  /* Create the service using this module as the executable */
  GetModuleFileName(NULL, (LPSTR)m_name, MAX_PATH);

  if (szServiceArg)
    {
      szPath = g_strdup_printf("\"%s\" %s",m_name,szServiceArg);
    }
  else
    {
      szPath = g_strdup_printf("\"%s\"",m_name);
    }

  hService = CreateService(hSCManager, szName, szDisplayName, SERVICE_ALL_ACCESS,
                           dwType, dwStartType, SERVICE_ERROR_NORMAL, (LPSTR)szPath,
                           NULL, NULL, szDependancy, szAccount, szPassword);

  if (hService == NULL)
    {
      fprintf(stderr,"Install() : hService == NULL : Error = 0x%08lx\n", GetLastError());
      CloseServiceHandle(hSCManager);
      return 1;
    }
  g_free(szPath);

  /* Set the service description */
  description.lpDescription = szDescription;

  if (ChangeServiceConfig2(hService, SERVICE_CONFIG_DESCRIPTION, &description) == FALSE)
    {
      fprintf(stderr,"Install() : Service description : Error = 0x%08lx\n", GetLastError());
    }

  /* Close opened handles */
  CloseServiceHandle(hService);
  CloseServiceHandle(hSCManager);
  return 0;
}

int
uninstall_service(char * szName)
{

  /* Open the service control manager */
  SC_HANDLE hSCManager;
  SC_HANDLE hService;

  hSCManager = OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_ALL_ACCESS);
  if (hSCManager == NULL)
    {
      fprintf(stderr,"Uninstall() : SCManager == NULL : Error = 0x%08lx\n", GetLastError());
      return 1;
    }

  /* Open the service if it exists */
  hService = OpenService(hSCManager, szName, SERVICE_ALL_ACCESS);
  if (hService == NULL)
    {
      fprintf(stderr,"Uninstall() : hService == NULL : Error = 0x%08lx\n", GetLastError());
      CloseServiceHandle(hSCManager);
      return 1;
    }

  /* Remove the service entry */
  if (DeleteService(hService) == FALSE)
    {
      fprintf(stderr,"Uninstall() : Error = 0x%08lx\n", GetLastError());
      CloseServiceHandle(hService);
      CloseServiceHandle(hSCManager);
      return 1;
    }

  /* Close opened handles */
  CloseServiceHandle(hService);
  CloseServiceHandle(hSCManager);

  fprintf(stderr,"Uninstall() : Service uninstalled\n");

  return 0;
}

int
start_service_by_name(char * szName)
{

  /* Open the service control manager */
  SC_HANDLE hSCManager;
  SC_HANDLE hService;

  hSCManager = OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_ALL_ACCESS);
  if (hSCManager == NULL)
    {
      fprintf(stderr,"Start() : SCManager == NULL : Error = 0x%08lx\n", GetLastError());
      return 1;
    }

  /* Open the service if it exists */
  hService = OpenService(hSCManager, szName, SERVICE_ALL_ACCESS);
  if (hService == NULL)
    {
      fprintf(stderr,"Start() : hService == NULL : Error = 0x%08lx\n", GetLastError());
      CloseServiceHandle(hSCManager);
      return 1;
    }

  /* Remove the service entry */
  if (StartService(hService,0,NULL) == FALSE)
    {
      fprintf(stderr,"Start() : Error = 0x%08lx\n", GetLastError());
      CloseServiceHandle(hService);
      CloseServiceHandle(hSCManager);
      return 1;
    }

  /* Close opened handles */
  CloseServiceHandle(hService);
  CloseServiceHandle(hSCManager);

  fprintf(stderr,"Start() : Service started\n");

  return 0;
}

int
stop_service_by_name(char * szName)
{

  /* Open the service control manager */
  SC_HANDLE hSCManager;
  SC_HANDLE hService;
  SERVICE_STATUS Status = {0};

  hSCManager = OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_ALL_ACCESS);
  if (hSCManager == NULL)
    {
      fprintf(stderr,"Stop() : SCManager == NULL : Error = 0x%08lx\n", GetLastError());
      return 1;
    }

  /* Open the service if it exists */
  hService = OpenService(hSCManager, szName, SERVICE_ALL_ACCESS);
  if (hService == NULL)
    {
      fprintf(stderr,"Stop() : hService == NULL : Error = 0x%08lx\n", GetLastError());
      CloseServiceHandle(hSCManager);
      return 1;
    }

  /* Remove the service entry */
  if (ControlService(hService, SERVICE_CONTROL_STOP, &Status) == FALSE)
    {
      fprintf(stderr,"Stop() : Error = 0x%08lx\n", GetLastError());
      CloseServiceHandle(hService);
      CloseServiceHandle(hSCManager);
      return 1;
    }

  /* Close opened handles */
  CloseServiceHandle(hService);
  CloseServiceHandle(hSCManager);

  fprintf(stderr,"Stop() : Service stopped\n");

  return 0;
}

void
main_control_handler(DWORD request)
{
  switch(request)
  {
    case SERVICE_CONTROL_STOP:
      ServiceStatus.dwWin32ExitCode = 0;
      if (g_service_stop_function)
        {
          ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
          g_service_stop_function();
        }
      else
        {
          ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        }
      fclose(global_service_logfile);
      SetServiceStatus(hStatus,&ServiceStatus);
    return;
    case SERVICE_CONTROL_SHUTDOWN:
      ServiceStatus.dwWin32ExitCode = 0;
      ServiceStatus.dwCurrentState = SERVICE_STOPPED;
      SetServiceStatus(hStatus,&ServiceStatus);
    return;
    default:
    break;
  }
  SetServiceStatus(hStatus,&ServiceStatus);
  return;
}


static
int main_service_main(int argc, char *argv[])
{
  int result = 0;
  char currDirectory[_MAX_PATH];
  char *pIdx;
  ServiceStatus.dwServiceType = SERVICE_WIN32;
  ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
  ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
  ServiceStatus.dwWin32ExitCode = 0;
  ServiceStatus.dwServiceSpecificExitCode = 0;
  ServiceStatus.dwCheckPoint = 0;
  ServiceStatus.dwWaitHint = 0;
  hStatus = RegisterServiceCtrlHandler(g_service_name,(LPHANDLER_FUNCTION) main_control_handler);
  register_minidump_writer();

  GetModuleFileName(NULL, currDirectory, _MAX_PATH);
  pIdx = strrchr(currDirectory, '\\');
  if (pIdx)
    *pIdx = '\0';
  pIdx = strrchr(currDirectory, '\\');
  if (pIdx)
    if ((strcmp((pIdx + 1),"bin")==0) || (strcmp((pIdx + 1),"lib")==0))
      {
        *pIdx = '\0';
      }
  SetCurrentDirectory(currDirectory);
  global_service_logfile = freopen( "syslog-ng.output", "w", stderr );

  if (hStatus == (SERVICE_STATUS_HANDLE) 0)
    {
      ServiceStatus.dwWin32ExitCode = -1;
      ServiceStatus.dwCurrentState = SERVICE_STOPPED;
      return -1;
    }

  ServiceStatus.dwCurrentState = SERVICE_RUNNING;
  SetServiceStatus(hStatus,&ServiceStatus);

  result = g_service_main_function();

  fclose(global_service_logfile);

  ServiceStatus.dwWin32ExitCode = 0;
  ServiceStatus.dwCurrentState = SERVICE_STOPPED;
  SetServiceStatus(hStatus,&ServiceStatus);
  return 0;
}

int
start_service(char *szName,SERVICE_MAIN service_main, SERVICE_STOP_FUNCTION service_stop)
{
  g_service_main_function = service_main;
  g_service_stop_function = service_stop;
  g_service_name = g_strdup(szName);
  SERVICE_TABLE_ENTRY ServiceTable[2];

  ServiceTable[0].lpServiceName = szName;
  ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION) main_service_main;
  ServiceTable[1].lpServiceName = NULL;
  ServiceTable[1].lpServiceProc = NULL;
  StartServiceCtrlDispatcher(ServiceTable);
  return 0;
}
