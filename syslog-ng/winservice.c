#include "winservice.h"
#include <stdio.h>
#include <signal.h>
#include "dbghelp.h"

SERVICE_STATUS ServiceStatus;
SERVICE_STATUS_HANDLE hStatus;

static gchar *g_service_name = NULL;
static SERVICE_MAIN g_service_main_function = NULL;
static SERVICE_STOP_FUNCTION g_service_stop_function = NULL;
FILE *global_service_logfile = NULL;

typedef BOOL (WINAPI *MINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType,
                  CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
                  CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
                  CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam
                  );

static void
abort_handler(int signal)
{
  RaiseException(0, 0, 0, NULL);
}


gboolean
get_logical_address (VOID *addr, PTSTR module_name, DWORD len, DWORD *section, DWORD *offset)
{
  MEMORY_BASIC_INFORMATION memory_basic_information;
  DWORD allocation_base;
  PIMAGE_DOS_HEADER pe_dos_header;
  PIMAGE_NT_HEADERS pe_nt_header;
  PIMAGE_SECTION_HEADER pe_section_header;
  DWORD rva;
  unsigned i;

  if (IsBadReadPtr(addr,sizeof(void *)))
    return FALSE;

  if (!VirtualQuery (addr, &memory_basic_information, sizeof (memory_basic_information)))
    return FALSE;

  allocation_base = (DWORD) memory_basic_information.AllocationBase;
  if (allocation_base == 0)
    return TRUE;

  if (!GetModuleFileName ((HMODULE) allocation_base, module_name, len))
    return FALSE;

  /* Point to the DOS header in memory */
  pe_dos_header = (PIMAGE_DOS_HEADER) allocation_base;

  /* From the DOS header, find the NT (PE) header */
  pe_nt_header = (PIMAGE_NT_HEADERS) (allocation_base + pe_dos_header->e_lfanew);

  /* From the NT header, find the section header */
  pe_section_header = IMAGE_FIRST_SECTION (pe_nt_header);

  /* RVA is offset from module load address */
  rva = (DWORD) addr - allocation_base;

  /*
   * Iterate through the section table, looking for the one that encompasses
   * the linear address.
   */

  for (i = 0; i < pe_nt_header->FileHeader.NumberOfSections; i++, pe_section_header++)
    {
      DWORD section_start = pe_section_header->VirtualAddress;
      DWORD section_end = section_start + max(pe_section_header->SizeOfRawData, pe_section_header->Misc.VirtualSize);

      // Is the address in this section???
      if ((rva >= section_start) && (rva <= section_end))
        {
          /*
           * Yes, address is in the section.  Calculate section and offset,
           * and store in the "section" & "offset" params, which were
           * passed by reference.
           */
          *section = i + 1;
          *offset = rva - section_start;
          return TRUE;
        }
    }
  return FALSE;
}


void
intel_stack_walk (PCONTEXT context, FILE *outfile)
{
#ifdef _M_IX86
  DWORD *frame_pointer, *prev_frame;
  DWORD eip = context->Eip;
  frame_pointer = (DWORD*)context->Ebp;
#elif defined(_M_X64)
  DWORD64 *frame_pointer, *prev_frame;
  DWORD64 eip = context->Rip;
  frame_pointer = (DWORD64*)context->Rbp;
#endif
  fprintf(outfile,"Call stack:\n");
  fprintf(outfile,"Address   Frame     Logical addr  Module\n");
  fflush(outfile);
  do
    {
      char module_name[_MAX_PATH] = {0};
      DWORD section = 0, offset = 0;
      if (eip > 0)
        {
          if (!get_logical_address((VOID*) eip, module_name, sizeof (module_name), &section, &offset))
            {
              break;
            }
        }

      fprintf(outfile,"%08lx  %p  %04lx:%08lx %s\n", eip, frame_pointer, section, offset, module_name);
      eip = frame_pointer[1];
      prev_frame = frame_pointer;
#ifdef _M_IX86
      frame_pointer = (DWORD*) frame_pointer[0];  /* precede to next higher frame on stack */
      if ((DWORD) frame_pointer & 3)              /* Frame pointer must be aligned on a DWORD boundary.  Bail if not so.
                                                     (address that is aligned on a 4-BYTE (DWORD) boundary is evenly divisible by 4) 4=100b  3=011b */
        {
          break;
        }
      if (frame_pointer <= prev_frame)
        {
          break;
        }
      /* Can two DWORDs be read from the supposed frame address? */
      if (IsBadReadPtr (frame_pointer, sizeof (DWORD) * 2))
        {
          break;
        }
#elif defined(_M_X64)
      frame_pointer = (DWORD64*) frame_pointer[0];  /* precede to next higher frame on stack */
      if ((DWORD64) frame_pointer & 7)       /* Frame pointer must be aligned on a DWORD64 boundary.  Bail if not so.
                                                (address that is aligned on a 8-BYTE (DWORD64) boundary is evenly divisible by 8) 8=1000b 7=0111b */
        {
          break;
        }
      if (frame_pointer <= prev_frame)
        {
          break;
        }
#endif
   }
  while (TRUE);
}


LONG WINAPI
drop_minidump( struct _EXCEPTION_POINTERS *pExceptionInfo )
{
  LONG retval = EXCEPTION_CONTINUE_SEARCH;

  // firstly see if dbghelp.dll is around and has the function we need
  // look next to the EXE first, as the one in System32 might be old
  // (e.g. Windows 2000)
  HMODULE hDll = NULL;
  char szDbgHelpPath[_MAX_PATH];
  char currentFilePath[_MAX_PATH];
  char *dir;
  char *filename;
  char core_filename[_MAX_PATH];
  char trace_filename[_MAX_PATH];

  if (GetModuleFileName( NULL, currentFilePath, _MAX_PATH ))
  {
    char *pSlash = strrchr( currentFilePath, '\\' );
    dir = currentFilePath;
    if (pSlash)
    {
      char *ext = NULL;
      filename = pSlash + 1;
      ext = strrchr(filename,'.');
      if (ext)
        *ext = '\0';
      *pSlash = '\0';
      strcpy(szDbgHelpPath,dir);
      strcat(szDbgHelpPath,"\\DBGHELP.DLL");

      strcpy(core_filename,dir);

      strcat(core_filename,"\\");
      strcat(core_filename,filename);

      strcpy(trace_filename,core_filename);

      strcat(core_filename,".dmp");
      strcat(trace_filename,".trace");
      hDll = LoadLibrary( szDbgHelpPath );
    }
  }
  FILE *trace_file = fopen(trace_filename,"a");
  if (trace_file)
    {
      fprintf(trace_file,"Start stack walk\n");
      fflush(trace_file);
      intel_stack_walk(pExceptionInfo->ContextRecord, trace_file);
      fprintf(trace_file,"End of stack walk\n");
      fflush(trace_file);
    }

  if (hDll==NULL)
  {
    // load any version we can
    hDll = LoadLibrary( "DBGHELP.DLL" );
  }

  char *szResult = NULL;
  if (hDll)
  {
    MINIDUMPWRITEDUMP pDump = GetProcAddress( hDll, "MiniDumpWriteDump" );
    if (pDump)
    {
      char szDumpPath[_MAX_PATH];
      char szScratch [_MAX_PATH];

      strcpy( szDumpPath, core_filename);

      // ask the user if they want to save a dump file
      fprintf(stderr,"Creating Minidump: %s\n",szDumpPath);
      // create the file
      HANDLE hFile = CreateFile( szDumpPath, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );

      if (hFile!=INVALID_HANDLE_VALUE)
        {
          MINIDUMP_EXCEPTION_INFORMATION ExInfo;

          ExInfo.ThreadId = GetCurrentThreadId();
          ExInfo.ExceptionPointers = pExceptionInfo;
          ExInfo.ClientPointers = TRUE;

          // write the dump
          BOOL bOK = pDump( GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpWithFullMemory, &ExInfo, NULL, NULL );
          if (bOK)
          {
            sprintf( szScratch, "Saved dump file to '%s'", szDumpPath );
            szResult = szScratch;
            retval = EXCEPTION_EXECUTE_HANDLER;
          }
          else
          {
            sprintf( szScratch, "Failed to save dump file to '%s' (error %lu)", szDumpPath, GetLastError() );
            szResult = szScratch;
          }
          CloseHandle(hFile);
        }
        else
        {
          sprintf( szScratch, "Failed to create dump file '%s' (error %lu)", szDumpPath, GetLastError() );
          szResult = szScratch;
        }
      }
    else
      {
        szResult = "DBGHELP.DLL too old";
      }
    }
  else
    {
      szResult = "DBGHELP.DLL not found";
    }

  if (szResult)
    fprintf(trace_file,"Result: %s\n",szResult);
  fclose(trace_file);

  return retval;
}

void
set_minidumper()
{
  SetUnhandledExceptionFilter(drop_minidump);
  signal(SIGABRT, abort_handler);
}

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
      szPath = g_strdup_printf("%s %s",m_name,szServiceArg);
    }
  else
    {
      szPath = g_strdup(m_name);
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
  set_minidumper();

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
