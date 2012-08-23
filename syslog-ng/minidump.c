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

static void
AbortHandler(int signal)
{
  RaiseException(0, 0, 0, NULL);
}

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

BOOL
GetLogicalAddress(VOID *addr, PTSTR moduleName, DWORD len, DWORD *section, DWORD *offset)
{
  MEMORY_BASIC_INFORMATION memoryBasicInformation;
  DWORD allocationBase;
  PIMAGE_DOS_HEADER peDosHeader;
  PIMAGE_NT_HEADERS peNtHeader;
  PIMAGE_SECTION_HEADER peSectionHeader;
  DWORD rva;
  unsigned i;

  if (IsBadReadPtr(addr, sizeof(void *)))
    return FALSE;

  if (!VirtualQuery (addr, &memoryBasicInformation, sizeof (memoryBasicInformation)))
    return FALSE;

  allocationBase = (DWORD) memoryBasicInformation.AllocationBase;
  if (allocationBase == 0)
    return TRUE;

  if (!GetModuleFileName ((HMODULE) allocationBase, moduleName, len))
    return FALSE;

  /* Point to the DOS header in memory */
  peDosHeader = (PIMAGE_DOS_HEADER) allocationBase;

  /* From the DOS header, find the NT (PE) header */
  peNtHeader = (PIMAGE_NT_HEADERS) (allocationBase + peDosHeader->e_lfanew);

  /* From the NT header, find the section header */
  peSectionHeader = IMAGE_FIRST_SECTION (peNtHeader);

  /* RVA is offset from module load address */
  rva = (DWORD) addr - allocationBase;

  /* Iterate through the section table,
     looking for the one that encompasses
   * the linear address. */
  for (i = 0; i < peNtHeader->FileHeader.NumberOfSections; i++, peSectionHeader++)
    {
      DWORD section_start = peSectionHeader->VirtualAddress;
      DWORD section_end = section_start + max(peSectionHeader->SizeOfRawData, peSectionHeader->Misc.VirtualSize);

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
IntelStackWalk (PCONTEXT context, FILE *outfile)
{
  void *framePointer = NULL;
  void *eip = NULL;
  void *prevFrame = NULL;
  char *moduleName = malloc(MAX_FILE_NAME_LENGTH);
#ifdef _X86_
  eip = (void *)context->Eip;
  framePointer = (void *)context->Ebp;
#elif defined(_x86_64_)
  eip = (void *)context->Rip;
  framePointer = (void *)context->Rbp;
#endif
  fprintf(outfile, "Call stack:\n");
  fprintf(outfile, "Address   Frame     Logical addr  Module\n");
  do
    {
      moduleName[0] = '\0';
      DWORD section = 0, offset = 0;
      if (!IsBadReadPtr(eip, sizeof(eip)))
        {
          if (!GetLogicalAddress((VOID*) eip, moduleName, MAX_FILE_NAME_LENGTH, &section, &offset))
            {
              break;
            }
        }
      fprintf(outfile, "%p  %p  %04lx:%08lx %s\n", eip, framePointer, section, offset, moduleName);
      eip = ((void **)framePointer)[1];
      prevFrame = framePointer;
      // precede to next higher frame on stack
      framePointer = ((void **) framePointer)[0];
      if (framePointer == NULL)
        {
          break;
        }
      if (framePointer <= prevFrame)
        {
          break;
        }
      if (IsBadReadPtr (framePointer, sizeof (framePointer) * 2))
        {
          break;
        }
    }
  while (TRUE);
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

  bOK = _MiniDumpWriteDump( GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpWithFullMemory, &ExInfo, NULL, NULL );
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

LONG WINAPI
MinidumpWriter(struct _EXCEPTION_POINTERS *pExceptionInfo)
{
  LONG retval = EXCEPTION_CONTINUE_SEARCH;

  // firstly see if dbghelp.dll is around and has the function we need
  // look next to the EXE first, as the one in System32 might be old
  // (e.g. Windows 2000)
  char *directory = NULL;
  char *fileName = NULL;
  char *dbgHelpPath = malloc(MAX_FILE_NAME_LENGTH + 1);
  char *dumpFileName = malloc(MAX_FILE_NAME_LENGTH + 1);
  char *traceFileName = malloc(MAX_FILE_NAME_LENGTH + 1);
  FILE *traceFile = NULL;
  if (!SplitCurrentModulePath(&directory, &fileName))
    {
      goto exit;
    }

  snprintf(dbgHelpPath, MAX_FILE_NAME_LENGTH, "%s\\DBGHELP.DLL", directory);
  snprintf(dumpFileName, MAX_FILE_NAME_LENGTH, "%s\\%s.dmp", directory, fileName);
  snprintf(traceFileName, MAX_FILE_NAME_LENGTH, "%s\\%s.trace", directory, fileName);

  traceFile = fopen(traceFileName, "w");
  if (traceFile)
    {
      setvbuf(traceFile, NULL, _IONBF, 0);
      fprintf(traceFile, "Start stack walk\n");
      IntelStackWalk(pExceptionInfo->ContextRecord, traceFile);
      fprintf(traceFile, "End of stack walk\n");
      fclose(traceFile);
    }
  else
    {
      fprintf(stderr, "Can't create trace file!\n");
    }

  retval = DropMinidump(pExceptionInfo, dumpFileName, dbgHelpPath);
exit:
  free(dbgHelpPath);
  free(dumpFileName);
  free(traceFileName);
  if (directory)
    free(directory);
  if (fileName)
    free(fileName);
  return retval;
}

void
register_minidump_writer()
{
  SetUnhandledExceptionFilter(MinidumpWriter);
  signal(SIGABRT, AbortHandler);
}
