#include "syslog-ng.h"
#include "messages.h"
#include "gsocket.h"
#include "control_server.h"
#include "misc.h"
#include "iv.h"
#include <string.h>
#include <aclapi.h>
#include <stdio.h>

#define BUFSIZE 65535
#define PIPE_TIMEOUT 5000

gboolean
create_security_attributes(LPSECURITY_ATTRIBUTES result, GString *error_description)
{
  PSECURITY_DESCRIPTOR  security_descriptor = NULL;
  PACL acl = NULL;
  EXPLICIT_ACCESS explicit_access;
  PSID admin_sid = NULL;
  SID_IDENTIFIER_AUTHORITY sid_auth_nt = {SECURITY_NT_AUTHORITY};

  if(!AllocateAndInitializeSid(&sid_auth_nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admin_sid))
    {
      if (error_description)
        {
          g_string_assign(error_description, "Can't allocate and initialize SID");
        }
      return FALSE;
    }

  ZeroMemory(&explicit_access, sizeof(EXPLICIT_ACCESS));
  explicit_access.grfAccessPermissions = GENERIC_WRITE | GENERIC_READ;
  explicit_access.grfAccessMode = SET_ACCESS;
  explicit_access.grfInheritance= NO_INHERITANCE;
  explicit_access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
  explicit_access.Trustee.TrusteeType = TRUSTEE_IS_GROUP;
  explicit_access.Trustee.ptstrName  = (LPTSTR) admin_sid;

  if (SetEntriesInAcl(1, &explicit_access, NULL, &acl) != ERROR_SUCCESS)
    {
      if (error_description)
        {
          g_string_assign(error_description, "Failed to set entries in ACL");
        }
      return FALSE;
    }
  security_descriptor = (PSECURITY_DESCRIPTOR) LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
  if(!InitializeSecurityDescriptor(security_descriptor, SECURITY_DESCRIPTOR_REVISION))
    {
      if (error_description)
        {
          g_string_assign(error_description, "Can't initialize security descriptor");
        }
      goto cleanup;
    }

  if (!SetSecurityDescriptorDacl(security_descriptor, TRUE, acl, FALSE))
    {
      if (error_description)
        {
          g_string_assign(error_description, "Can't initialize security descriptor");
        }
      goto cleanup;
    }
  result->bInheritHandle = TRUE;
  result->lpSecurityDescriptor = security_descriptor;
  if (admin_sid)
    {
      FreeSid(admin_sid);
    }
  result->nLength = sizeof(SECURITY_ATTRIBUTES);
  return TRUE;
cleanup:
  if (admin_sid)
    {
      FreeSid(admin_sid);
    }
  if (acl)
    {
      LocalFree(acl);
    }
  if (security_descriptor)
    {
      LocalFree(security_descriptor);
    }
  return FALSE;
}

typedef struct _ControlServerWin32
{
  ControlServer super;
  SECURITY_ATTRIBUTES security_attributes;
  ControlConnection *current_connection;
} ControlServerWin32;

typedef struct _ControlConnectionWin32
{
  ControlConnection super;
  HANDLE pipe_handle;
  struct iv_handle input_handle;
  struct iv_handle output_handle;
  struct iv_handle connect_handle;
  OVERLAPPED o_connect;
  OVERLAPPED o_read;
  OVERLAPPED o_write;
  struct iv_task io_task;
  gboolean read_pending;
  gboolean write_pending;
} ControlConnectionWin32;


void control_server_win32_create_new_instance(ControlServer *s);

void
control_connection_client_connected(void *cookie)
{
  ControlConnectionWin32 *self = (ControlConnectionWin32 *)cookie;
  msg_debug("Incoming connection on the control pipe", NULL);
  iv_handle_unregister(&self->connect_handle);
  control_connection_update_watches(&self->super);
  control_server_win32_create_new_instance(self->super.server);
  return;
}

static gint
control_connection_win32_handle_io_error(DWORD error)
{
  if (error == ERROR_IO_PENDING)
    {
      errno = EAGAIN;
      return -1;
    }
  else
    {
      errno = error;
      return -1;
    }
}

static gint
control_connection_win32_handle_write_error(ControlConnectionWin32 *self)
{
  DWORD error = GetLastError();
  if (error == ERROR_IO_PENDING)
    {
      self->write_pending = TRUE;
    }
  return control_connection_win32_handle_io_error(GetLastError());
}

static gint
control_connection_win32_write(ControlConnection *s, gpointer buffer, gsize size)
{
  ControlConnectionWin32 *self = (ControlConnectionWin32 *)s;
  DWORD bytes_written;

  if (iv_handle_registered(&self->output_handle))
    {
      if (!GetOverlappedResult(self->pipe_handle, &self->o_write, &bytes_written, FALSE))
        {
          return control_connection_win32_handle_write_error(self);
        }
      iv_handle_unregister(&self->output_handle);
      self->write_pending = FALSE;
    }

  else if (!WriteFile(self->pipe_handle, buffer, size, &bytes_written, &self->o_write))
    {
      return control_connection_win32_handle_write_error(self);
    }
  return bytes_written;
}

static gint
control_connection_win32_handle_read_error(ControlConnectionWin32 *self)
{
  DWORD error = GetLastError();
  if (error == ERROR_BROKEN_PIPE)
    {
      return 0;
    }
  else
    {
      if (error == ERROR_IO_PENDING)
        {
          self->read_pending = TRUE;
        }
      return control_connection_win32_handle_io_error(error);
    }
}

static gint
control_connection_win32_read(ControlConnection *s, gpointer buffer, gsize size)
{
  ControlConnectionWin32 *self = (ControlConnectionWin32 *)s;
  DWORD bytes_read;

  if (iv_handle_registered(&self->input_handle))
    {

      if (!GetOverlappedResult(self->pipe_handle, &self->o_read, &bytes_read, FALSE))
        {
          return control_connection_win32_handle_read_error(self);
        }
      iv_handle_unregister(&self->input_handle);
      self->read_pending = FALSE;
    }

  else if (!ReadFile(self->pipe_handle, buffer, size, &bytes_read, &self->o_read))
    {
      return control_connection_win32_handle_read_error(self);
    }
  return bytes_read;
}

void
control_connection_update_watches(ControlConnection *s)
{
  ControlConnectionWin32 *self = (ControlConnectionWin32 *)s;
  if (iv_task_registered(&self->io_task))
    {
      iv_task_unregister(&self->io_task);
    }
  if (s->output_buffer->len > s->pos)
    {
      if (self->write_pending)
        {
          if (!iv_handle_registered(&self->output_handle))
            {
              iv_handle_register(&self->output_handle);
              iv_handle_set_handler(&self->output_handle, s->handle_output);
            }
        }
      else
        {
          self->io_task.handler = s->handle_output;
          iv_task_register(&self->io_task);
        }
    }
  else
    {
      if (self->read_pending)
        {
          if (!iv_handle_registered(&self->input_handle))
            {
              iv_handle_register(&self->input_handle);
              iv_handle_set_handler(&self->input_handle, s->handle_input);
            }
        }
      else
        {
          self->io_task.handler = s->handle_input;
          iv_task_register(&self->io_task);
        }
    }
}

void
control_connection_start_watches(ControlConnection *s)
{
  ControlConnectionWin32 *self = (ControlConnectionWin32 *)s;

  IV_HANDLE_INIT(&self->input_handle);
  self->input_handle.handle = CreateEvent(NULL, FALSE, FALSE, NULL);
  self->input_handle.cookie = self;
  self->o_read.hEvent = self->input_handle.handle;

  IV_HANDLE_INIT(&self->output_handle);
  self->output_handle.handle = CreateEvent(NULL, FALSE, FALSE, NULL);
  self->input_handle.cookie = self;
  self->o_write.hEvent = self->output_handle.handle;

  IV_HANDLE_INIT(&self->connect_handle);
  self->connect_handle.handle = CreateEvent(NULL, FALSE, FALSE, NULL);
  self->connect_handle.cookie = self;
  self->o_connect.hEvent = self->connect_handle.handle;

  IV_TASK_INIT(&self->io_task);
  self->io_task.cookie = self;

  if (!ConnectNamedPipe(self->pipe_handle, &self->o_connect))
    {
      DWORD error = GetLastError();
      if (error == ERROR_IO_PENDING)
        {
          iv_handle_register(&self->connect_handle);
          iv_handle_set_handler(&self->connect_handle, control_connection_client_connected);
          return;
        }
      if (error == ERROR_PIPE_CONNECTED)
        {
          control_server_win32_create_new_instance(s->server);
          control_connection_update_watches(s);
          return;
        }
    }
  else
    {
      msg_error("Connect named pipe failed",
                evt_tag_str("pipe", s->server->control_socket_name),
                evt_tag_win32_error("error", GetLastError()),
                NULL);
    }
  return;
}

void
control_connection_stop_watches(ControlConnection *s)
{
  ControlConnectionWin32 *self = (ControlConnectionWin32 *)s;

  if (iv_handle_registered(&self->input_handle))
    {
      iv_handle_unregister(&self->input_handle);
    }
  if (iv_handle_registered(&self->output_handle))
    {
      iv_handle_unregister(&self->output_handle);
    }
  if (iv_handle_registered(&self->connect_handle))
    {
      iv_handle_unregister(&self->connect_handle);
    }
  if (iv_task_registered(&self->io_task))
    {
      iv_task_unregister(&self->io_task);
    }
}

void
control_connection_win32_free(ControlConnection *s)
{
  ControlConnectionWin32 *self = (ControlConnectionWin32 *)s;

  DisconnectNamedPipe(self->pipe_handle);
  CloseHandle(self->pipe_handle);
  CloseHandle(self->input_handle.handle);
  CloseHandle(self->output_handle.handle);
  CloseHandle(self->connect_handle.handle);
  return;
}

ControlConnection *
control_connection_new(ControlServer *server, HANDLE pipe_handle)
{
  ControlConnectionWin32 *self = g_new0(ControlConnectionWin32,1);

  control_connection_init_instance(&self->super, server);

  self->super.free_fn = control_connection_win32_free;
  self->super.read = control_connection_win32_read;
  self->super.write = control_connection_win32_write;

  self->pipe_handle = pipe_handle;
  control_connection_start_watches(&self->super);

  return &self->super;
}

void
control_server_win32_create_new_instance(ControlServer *s)
{
  ControlServerWin32 *self = (ControlServerWin32 *)s;
  HANDLE pipe_handle = CreateNamedPipe(s->control_socket_name,
                          PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                          PIPE_WAIT,
                          PIPE_UNLIMITED_INSTANCES,
                          BUFSIZE,
                          BUFSIZE,
                          PIPE_TIMEOUT,
                          &self->security_attributes);
   if (pipe_handle != INVALID_HANDLE_VALUE)
     {
       self->current_connection = control_connection_new(s, pipe_handle);
     }
   else
     {
       msg_error("Can't create named pipe",
                 evt_tag_win32_error("error", GetLastError()),
                 NULL);
     }
  return;
}

void
control_server_start(ControlServer *s)
{
  ControlServerWin32 *self = (ControlServerWin32 *)s;
  GString *error_description = g_string_sized_new(128);
  if (!create_security_attributes(&self->security_attributes, error_description))
    {
      msg_error("Can't create security descriptor for the control pipe",
                evt_tag_str("message", error_description->str),
                evt_tag_win32_error("error", GetLastError()),
                NULL);
    }
  else
    {
      control_server_win32_create_new_instance(s);
    }
  g_string_free(error_description, TRUE);
  return;
}

void
control_server_win32_free(ControlServer *s)
{
  ControlServerWin32 *self = (ControlServerWin32 *)s;
  if (self->current_connection)
    {
      control_connection_stop_watches(self->current_connection);
      control_connection_free(self->current_connection);
      self->current_connection = NULL;
    }
  if (self->security_attributes.lpSecurityDescriptor)
    {
      BOOL is_acl_present;
      PACL acl = NULL;
      BOOL is_default;
      if (GetSecurityDescriptorDacl(self->security_attributes.lpSecurityDescriptor, &is_acl_present, &acl, &is_default))
        {
          if (is_acl_present && acl)
            {
              LocalFree(acl);
            }
        }
      LocalFree(self->security_attributes.lpSecurityDescriptor);
    }
}

ControlServer *
control_server_new(const gchar *name, Commands *commands)
{
  ControlServerWin32 *self = g_new0(ControlServerWin32, 1);

  control_server_init_instance(&self->super, name, commands);
  self->super.free_fn = control_server_win32_free;

  return &self->super;
}
