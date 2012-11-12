#include <windows.h>
#include <stdio.h>
#include <signal.h>
#include "dbghelp.h"
#include "minidump.h"

typedef BOOL (WINAPI *MINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType,
                  CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
                  CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
                  CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam
                  );



#define MAX_FILE_NAME_LENGTH 65535

#ifdef WIN64
PVOID ExceptionHandler;
#endif

BOOL
SplitCurrentModulePath(char **dir, char **fileName)
{
  char currentFilePath[MAX_FILE_NAME_LENGTH];
  char *dirEnd = NULL;
  char *ext = NULL;
  if (!GetModuleFileName(NULL, currentFilePath, MAX_FILE_NAME_LENGTH))
    return FALSE;
  dirEnd = strrchr(currentFilePath, '\\' );
  if (!dirEnd)
    return FALSE;
  *dirEnd = '\0';
  dirEnd++;
  if (dir)
    {
      size_t dirLength = strlen(currentFilePath);
      *dir = malloc(dirLength + 1);
      strcpy(*dir, currentFilePath);
    }
  ext = strrchr(dirEnd + 1,'.');
  if (ext)
    *ext = '\0';
  if (fileName)
    {
      size_t fileNameLength = strlen(dirEnd);
      *fileName = malloc(fileNameLength + 1);
      strcpy(*fileName, dirEnd);
    }
  return TRUE;
}

LONG WINAPI
DropMinidump(struct _EXCEPTION_POINTERS *pExceptionInfo, char *dumpFileName, char *dbgHelpPath)
{
  LONG retval = EXCEPTION_CONTINUE_SEARCH;
  HMODULE hDll = LoadLibrary(dbgHelpPath);
  HANDLE hFile;
  MINIDUMP_EXCEPTION_INFORMATION ExInfo;
  BOOL bOK;
  MINIDUMPWRITEDUMP _MiniDumpWriteDump = NULL;

  if (!hDll)
    {
      hDll = LoadLibrary("DBGHELP.DLL");
      if (!hDll)
        {
          fprintf(stderr, "DBGHELP.DLL not found\n");
          return retval;
        }
    }

  _MiniDumpWriteDump = GetProcAddress( hDll, "MiniDumpWriteDump" );
  if (!_MiniDumpWriteDump)
    {
      fprintf(stderr, "Can't find MiniDumpWriteDump function in dbghelp.dll (Maybe the dbghelp.dll is too old)\n");
      return retval;
    }

  fprintf(stderr, "Creating Minidump: %s\n", dumpFileName);
  hFile = CreateFile( dumpFileName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
  if (hFile == INVALID_HANDLE_VALUE)
    {
      fprintf(stderr, "Failed to create dump file '%s' (error %lu)\n", dumpFileName, GetLastError());
      return retval;
    }


  ExInfo.ThreadId = GetCurrentThreadId();
  ExInfo.ExceptionPointers = pExceptionInfo;
  ExInfo.ClientPointers = TRUE;

  bOK = _MiniDumpWriteDump( GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpWithFullMemory, pExceptionInfo ? &ExInfo : NULL, NULL, NULL );
  if (bOK)
    {
      fprintf(stderr, "Saved dump file to '%s'\n", dumpFileName );
      retval = EXCEPTION_EXECUTE_HANDLER;
    }
  else
    {
      fprintf(stderr, "Failed to save dump file to '%s' (error %lu)\n", dumpFileName, GetLastError());
    }
  CloseHandle(hFile);
  return retval;
}

#define IS_CRITICAL(x) ((x & 0xF0000000) == 0xC0000000)

LONG WINAPI
MinidumpWriter(struct _EXCEPTION_POINTERS *pExceptionInfo)
{
  LONG retval = EXCEPTION_CONTINUE_SEARCH;
#ifdef WIN64
  if (IS_CRITICAL(pExceptionInfo->ExceptionRecord->ExceptionCode))
    {
      RemoveVectoredExceptionHandler(ExceptionHandler);
    }
  else
    {
      return retval;
    }
#endif

  // firstly see if dbghelp.dll is around and has the function we need
  // look next to the EXE first, as the one in System32 might be old
  // (e.g. Windows 2000)
  char *directory = NULL;
  char *fileName = NULL;
  char *dbgHelpPath = malloc(MAX_FILE_NAME_LENGTH + 1);
  char *dumpFileName = malloc(MAX_FILE_NAME_LENGTH + 1);
  if (!SplitCurrentModulePath(&directory, &fileName))
    {
      goto exit;
    }

  snprintf(dbgHelpPath, MAX_FILE_NAME_LENGTH, "%s\\DBGHELP.DLL", directory);
  snprintf(dumpFileName, MAX_FILE_NAME_LENGTH, "%s\\%s.dmp", directory, fileName);

  retval = DropMinidump(pExceptionInfo, dumpFileName, dbgHelpPath);
exit:
  free(dbgHelpPath);
  free(dumpFileName);
  if (directory)
    free(directory);
  if (fileName)
    free(fileName);
  return retval;
}

static void
AbortHandler(int signal)
{
  RaiseException(STATUS_NONCONTINUABLE_EXCEPTION, EXCEPTION_NONCONTINUABLE, 0, NULL);
}

void
register_minidump_writer()
{
  signal(SIGABRT, AbortHandler);
#ifndef WIN64
  SetUnhandledExceptionFilter(MinidumpWriter);
#else
  ExceptionHandler = AddVectoredExceptionHandler(1, MinidumpWriter);
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOALIGNMENTFAULTEXCEPT | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
#endif
}
