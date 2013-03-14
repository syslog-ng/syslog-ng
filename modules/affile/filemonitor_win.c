#include "filemonitor.h"
#include "messages.h"
#include <stdio.h>
#include <evtlog.h>
#include <windows.h>
#include <iv_event.h>

#define FILE_MONITOR_BUFFER 64*1024
#define FILE_MONITOR_BUFFER_SIZE 64*1024*sizeof(BYTE)

typedef struct  _FileMonitorWindows
{
  FileMonitor super;
  gchar *base_dir;
  struct iv_event monitor_event;
  HANDLE hDir;
  OVERLAPPED ol;
  BYTE *buffer;
  DWORD buffer_size;
  DWORD notify_flags;
  struct iv_handle monitor_handler;
} FileMonitorWindows;

/*
 Resolve . and ..
 Resolve symlinks
 Resolve tricki symlinks like a -> ../a/../a/./b
*/
gchar*
resolve_to_absolute_path(const gchar *path, const gchar *basedir)
{
       gchar *path_name = NULL;
       gchar szPath[MAX_PATH];
       if (basedir != NULL)
       {
               path_name = g_strdup_printf("%s%s%s",basedir,G_DIR_SEPARATOR_S,path);
       }
       else
       {
               path_name = g_strdup(path);
       }
       GetFullPathName(path_name,MAX_PATH,szPath,NULL);
       g_free(path_name);
       return g_strdup(szPath);
}


/**
 * file_monitor_chk_file:
 *
 * This function checks if the given filename matches the filters.
 **/
static gboolean
file_monitor_chk_file(FileMonitorWindows * monitor, const gchar *base_dir, const gchar *filename, FileActionType action_type)
{
  gboolean ret = FALSE;
  gchar *path = g_build_filename(base_dir, filename, NULL);
  gchar *base_name = g_path_get_basename(filename);
  gboolean match = g_pattern_match_string(monitor->super.compiled_pattern, base_name);
  g_free(base_name);


  if (match &&
      monitor->super.file_callback != NULL)
    {
      /* FIXME: resolve symlink */
      /* callback to affile */
      msg_debug("file_monitor_chk_file filter passed", evt_tag_str("file",path),NULL);
      monitor->super.file_callback(path, monitor->super.user_data, action_type);
      ret = TRUE;
    }
  g_free(path);
  return ret;
}

static gboolean
file_monitor_list_directory(FileMonitorWindows *self, const gchar *basedir)
{
  GDir *dir = NULL;
  GError *error = NULL;
  const gchar *file_name = NULL;
  /* try to open diretory */
  dir = g_dir_open(basedir, 0, &error);
  if (dir == NULL)
    {
      g_clear_error(&error);
      return FALSE;
    }

  while ((file_name = g_dir_read_name(dir)) != NULL)
    {
      gchar * path = resolve_to_absolute_path(file_name, basedir);
      if (g_file_test(path, G_FILE_TEST_IS_DIR))
        {
          /* Recursion is enabled */
          if (self->super.recursion)
            file_monitor_list_directory(self, path); /* construct a new source to monitor the directory */
        }
      else
        {
          /* if file or symlink, match with the filter pattern */
          file_monitor_chk_file(self, basedir, file_name, ACTION_NONE);
        }
      g_free(path);
    }
  g_dir_close(dir);
  if (self->super.file_callback != NULL)
    self->super.file_callback(END_OF_LIST, self->super.user_data,ACTION_NONE);
  return TRUE;
}


VOID CALLBACK
completition_routine(FileMonitorWindows *self, DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
{
  DWORD offset = 0;
  gchar szFile[MAX_PATH];
  int count = 0;
  PFILE_NOTIFY_INFORMATION pNotify;
  do
    {
      FileActionType action_type = ACTION_NONE;
      pNotify = (PFILE_NOTIFY_INFORMATION) &self->buffer[offset];
      offset += pNotify->NextEntryOffset;
      count = WideCharToMultiByte(CP_ACP, 0, pNotify->FileName, pNotify->FileNameLength / sizeof(WCHAR), szFile,  MAX_PATH - 1, NULL, NULL);
      szFile[count] = 0;
      if (pNotify->Action == FILE_ACTION_ADDED || pNotify->Action == FILE_ACTION_RENAMED_NEW_NAME)
        action_type = ACTION_CREATED;
      else if (pNotify->Action == FILE_ACTION_REMOVED || pNotify->Action == FILE_ACTION_RENAMED_OLD_NAME)
        action_type = ACTION_DELETED;
      else if (pNotify->Action == FILE_ACTION_MODIFIED)
        action_type = ACTION_MODIFIED;
      file_monitor_chk_file(self, self->base_dir, szFile, action_type);
    }
  while(pNotify->NextEntryOffset != 0);
  if (ReadDirectoryChangesW(self->hDir, self->buffer, FILE_MONITOR_BUFFER_SIZE, self->super.recursion, self->notify_flags, NULL, &self->ol, NULL) == 0)
  {
    fprintf(stderr,"FAILED TO READ NEXT CHANGES OF DIRECTORY!\n");
    abort();
  }
  return;
}

void
fd_handler(void *user_data)
{
  FileMonitorWindows *self = (FileMonitorWindows *)user_data;
  DWORD read;
  if (GetOverlappedResult(self->hDir,&self->ol,&read,FALSE))
    {
      int err = GetLastError();
      completition_routine(self,err,read,&self->ol);
    }
}

FileMonitor *
file_monitor_new()
{
  FileMonitorWindows *self = g_new0(FileMonitorWindows, 1);
  self->buffer = (BYTE *)g_malloc0( FILE_MONITOR_BUFFER_SIZE);
  self->buffer_size =  FILE_MONITOR_BUFFER_SIZE;
  self->notify_flags = FILE_NOTIFY_CHANGE_SIZE|FILE_NOTIFY_CHANGE_LAST_ACCESS|FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_CREATION;
  IV_HANDLE_INIT(&self->monitor_handler);
  self->monitor_handler.handle = CreateEvent(NULL, FALSE, FALSE, NULL);
  self->monitor_handler.handler = fd_handler;
  self->monitor_handler.cookie = self;
  return &self->super;
}

void
file_monitor_free(FileMonitor *s)
{
  FileMonitorWindows *self = (FileMonitorWindows *)s;
  free(self->buffer);
  free(self);
  return;
}

void
file_monitor_set_file_callback(FileMonitor *self, FileMonitorCallbackFunc file_callback, gpointer user_data)
{
  self->file_callback = file_callback;
  self->user_data = user_data;
  return;
}

gboolean
file_monitor_watch_directory(FileMonitor *s, const gchar *filename)
{
  gchar *base_dir;
  FileMonitorWindows *self = (FileMonitorWindows *)s;
  g_assert(self);
  g_assert(filename);

  /* if the filename is not a directory then remove the file part and try only the directory part */
  gchar *dir_part = g_path_get_dirname (filename);

  base_dir = resolve_to_absolute_path(dir_part, NULL);
  if (!g_file_test(base_dir,G_FILE_TEST_IS_DIR))
    {
      msg_error("Directory doesn't exist",evt_tag_str("directory",base_dir),evt_tag_id(MSG_DIRECTORY_DOESNT_EXIST), NULL);
      return FALSE;
    }
  if (!self->super.compiled_pattern)
    {
      gchar *pattern = g_path_get_basename(filename);
      self->super.compiled_pattern = g_pattern_spec_new(pattern);
      g_free(pattern);
    }
  self->base_dir = base_dir;

  msg_debug("Monitoring new directory", evt_tag_str("basedir", base_dir), NULL);

  file_monitor_list_directory(self,self->base_dir);

  self->hDir = CreateFile(self->base_dir, FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);

  // The hEvent member is not used when there is a completion
  // function, so it's ok to use it to point to the object.
  self->ol.hEvent = self->monitor_handler.handle;
  if (ReadDirectoryChangesW(self->hDir, self->buffer, FILE_MONITOR_BUFFER_SIZE, self->super.recursion, self->notify_flags, NULL, &self->ol, NULL) == 0)
  {
    msg_error("Can't monitor directory",evt_tag_errno("error",GetLastError()), evt_tag_id(MSG_CANT_MONITOR_DIRECTORY), NULL);
    return FALSE;
  }
  iv_handle_register(&self->monitor_handler);
  return TRUE;
}

gboolean
file_monitor_stop(FileMonitor *s)
{
  FileMonitorWindows *self = (FileMonitorWindows *)s;

  if (iv_handle_registered(&self->monitor_handler))
    {
      iv_handle_unregister(&self->monitor_handler);
    }
  CloseHandle(self->hDir);
  return TRUE;
}

void
file_monitor_start(FileMonitor *s)
{
  return;
}

void
file_monitor_deinit(FileMonitor *s)
{
  return;
}
