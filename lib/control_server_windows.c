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

struct _ControlServer{
  struct iv_handle hConnected;
  struct iv_handle hInput;
  struct iv_handle hOutput;
  HANDLE hPipe;
  GString *input_buffer;
  GString *output_buffer;
  gchar *name;
  gboolean connected;
  GHashTable *command_list;
  SECURITY_ATTRIBUTES sec_attr;
  OVERLAPPED overlapped_result;
  gboolean set_security;
};

gboolean
create_security_attributes(LPSECURITY_ATTRIBUTES result, GString *error_description)
{
  PSECURITY_DESCRIPTOR  psdSecDescr = NULL;
  PACL pAcl = NULL;
  EXPLICIT_ACCESS eaExplicitAccess;
  PSID pAdminSID = NULL;
  SID_IDENTIFIER_AUTHORITY SIDAuthNT = {SECURITY_NT_AUTHORITY};

  if(!AllocateAndInitializeSid(&SIDAuthNT, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdminSID))
    {
      if (error_description)
        {
          g_string_assign(error_description,"Can't allocate and initialize SID");
        }
      return FALSE;
    }

  ZeroMemory(&eaExplicitAccess, sizeof(EXPLICIT_ACCESS));
  eaExplicitAccess.grfAccessPermissions = GENERIC_ALL | KEY_ALL_ACCESS | FILE_ALL_ACCESS ; // http://windowssdk.msdn.microsoft.com/en-us/library/ms717918.aspx @26.10.06
  eaExplicitAccess.grfAccessMode = SET_ACCESS;
  eaExplicitAccess.grfInheritance= NO_INHERITANCE;
  eaExplicitAccess.Trustee.TrusteeForm = TRUSTEE_IS_SID;
  eaExplicitAccess.Trustee.TrusteeType = TRUSTEE_IS_GROUP;
  eaExplicitAccess.Trustee.ptstrName  = (LPTSTR) pAdminSID;

  if (SetEntriesInAcl(1, &eaExplicitAccess, NULL, &pAcl) != ERROR_SUCCESS)
    {
      if (error_description)
        {
          g_string_assign(error_description,"Failed to set entries in ACL");
        }
      return FALSE;
    }
  psdSecDescr = (PSECURITY_DESCRIPTOR) LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
  if(!InitializeSecurityDescriptor(psdSecDescr, SECURITY_DESCRIPTOR_REVISION))
    {
      if (error_description)
        {
          g_string_assign(error_description,"Can't initialize security descriptor");
        }
      goto cleanup;
    }

  if (!SetSecurityDescriptorDacl(psdSecDescr, TRUE, pAcl, FALSE))
    {
      if (error_description)
        {
          g_string_assign(error_description,"Can't initialize security descriptor");
        }
      goto cleanup;
    }
  result->bInheritHandle = TRUE;
  result->lpSecurityDescriptor = psdSecDescr;
  result->nLength = sizeof(SECURITY_ATTRIBUTES);
  return TRUE;
cleanup:
  if( pAdminSID )        FreeSid(pAdminSID);
  if( pAcl )            LocalFree(pAcl);
  if( psdSecDescr )    LocalFree(psdSecDescr);
  return FALSE;
}


static void control_server_start_listening(ControlServer *self);
static void control_server_read_command(void *cookie);
COMMAND_HANDLER control_server_get_command_handler(ControlServer *self, gchar *cmd);

void
control_server_reply_sent(void *cookie)
{
  ControlServer *self = (ControlServer *)cookie;

  g_string_truncate(self->output_buffer, 0);
  g_string_truncate(self->input_buffer, 0);
   if (iv_handle_registered(&self->hOutput))
    {
      iv_handle_unregister(&self->hOutput);
    }
  control_server_read_command(cookie);
}

void
control_server_send_reply(ControlServer *self, GString *reply)
{
  g_string_assign(self->output_buffer, reply->str);
  g_string_free(reply, TRUE);

  if (iv_handle_registered(&self->hOutput))
    {
      iv_handle_unregister(&self->hOutput);
    }


  if (self->output_buffer->str[self->output_buffer->len - 1] != '\n')
    g_string_append_c(self->output_buffer, '\n');
  g_string_append(self->output_buffer, ".\n");

  if (self->hOutput.handle == INVALID_HANDLE_VALUE)
    {
      self->hOutput.handle = CreateEvent(NULL, FALSE, FALSE, NULL);
    }
  self->overlapped_result.hEvent = self->hOutput.handle;
  if(!WriteFile(self->hPipe, self->output_buffer->str, self->output_buffer->len, NULL, &self->overlapped_result))
    {
      DWORD error = GetLastError();
      if (error == ERROR_IO_PENDING)
        {
          iv_handle_register(&self->hOutput);
          iv_handle_set_handler(&self->hOutput, control_server_reply_sent);
          return;
        }
      else
        {
          msg_error("I/O error occured while writing control pipe",evt_tag_str("pipe",self->name), evt_tag_errno("error",error), NULL);
          control_server_start_listening(self);
        }
    }
  g_string_truncate(self->input_buffer, 0);
  control_server_read_command(self);
}

static void
control_server_read_pipe(ControlServer *self)
{
  DWORD error;
  gint orig_len;

    if (self->input_buffer->len > MAX_CONTROL_LINE_LENGTH)
    {
      /* too much data in input, drop the connection */
      msg_error("Too much data in the control socket input buffer",
                NULL);
      control_server_start_listening(self);
      return;
    }

  orig_len = self->input_buffer->len;

  /* NOTE: plus one for the terminating NUL */
  if (self->hInput.handle == INVALID_HANDLE_VALUE)
    {
      self->hInput.handle = CreateEvent(NULL, FALSE, FALSE, NULL);
    }
  self->overlapped_result.hEvent = self->hInput.handle;

  g_string_set_size(self->input_buffer, self->input_buffer->len + 128 + 1);

  if (!ReadFile(self->hPipe, self->input_buffer->str + orig_len, 128, NULL, &self->overlapped_result))
    {
      error = GetLastError();
      if (error == ERROR_BROKEN_PIPE)
        {
          control_server_start_listening(self);
          return;
        }
      else if (error == ERROR_IO_PENDING)
        {
          iv_handle_register(&self->hInput);
          iv_handle_set_handler(&self->hInput, control_server_read_command);
          return;
        }
      else
        {
          msg_error("Error occured while reading control pipe",evt_tag_str("pipe",self->name), evt_tag_errno("error",error), NULL);
          control_server_start_listening(self);
          return;
        }
    }
  control_server_read_command(self);
}


static void
control_server_read_command(void *cookie)
{
  ControlServer *self = (ControlServer *)cookie;
  GString *command = NULL;
  GString *reply = NULL;
  gchar *nl;
  gchar *pos;
  COMMAND_HANDLER command_handler;

  nl = strchr(self->input_buffer->str,'\n');
  if (!nl)
    {
      /* Start to read */
      if (iv_handle_registered(&self->hInput))
        {
          iv_handle_unregister(&self->hInput);
        }
      control_server_read_pipe(self);
      return;
    }
  command = g_string_sized_new(128);
  g_string_assign_len(command, self->input_buffer->str, nl - self->input_buffer->str);
  g_string_erase(self->input_buffer, 0, command->len + 1);
  pos = strchr(command->str, ' ');
  if (pos)
    *pos = '\0';
  command_handler = control_server_get_command_handler(self, command->str);
  if (pos)
    *pos = ' ';

  if (command_handler)
    {
      reply = command_handler(command);
      control_server_send_reply(self, reply);
    }
  else
    {
      msg_error("Unknown command read on control channel, closing control channel",
                evt_tag_str("command", command->str), NULL);
      goto destroy_connection;
    }
  g_string_free(command, TRUE);
  return;
destroy_connection:
  g_string_free(command, TRUE);
  control_server_start_listening(self);
}

void
control_server_client_connected(void *cookie)
{
  ControlServer *self = (ControlServer *)cookie;
  msg_debug("Incoming connection on the control pipe", NULL);
  iv_handle_unregister(&self->hConnected);
  self->connected = TRUE;
  control_server_read_command(cookie);
  return;
}

static void
control_server_start_listening(ControlServer *self)
{
  if (self->hConnected.handle == INVALID_HANDLE_VALUE)
    {
      self->hConnected.handle = CreateEvent(NULL, FALSE, FALSE, NULL);
    }
  self->overlapped_result.hEvent = self->hConnected.handle;

  if (self->connected)
    {
      DisconnectNamedPipe(self->hPipe);
      g_string_truncate(self->input_buffer, 0);
      g_string_truncate(self->output_buffer, 0);
      if (iv_handle_registered(&self->hInput))
        {
          iv_handle_unregister(&self->hInput);
        }
      if (iv_handle_registered(&self->hOutput))
        {
          iv_handle_unregister(&self->hOutput);
        }
      self->connected = FALSE;
    }

  if (!ConnectNamedPipe(self->hPipe, &self->overlapped_result))
    {
      DWORD error = GetLastError();
      if (error == ERROR_IO_PENDING)
        {
          iv_handle_register(&self->hConnected);
          iv_handle_set_handler(&self->hConnected, control_server_client_connected);
          return;
        }
      msg_error("Can't listen named pipe",evt_tag_str("pipe",self->name),evt_tag_errno("error",error),NULL);
    }
  return;
}

void
control_server_start(ControlServer *self)
{
  GString *error_description = g_string_sized_new(128);
  if (!create_security_attributes(&self->sec_attr, error_description))
    {
      msg_error("Can't create security descriptor for the control pipe",evt_tag_str("message",error_description->str), evt_tag_errno("error",GetLastError()), NULL);
    }
  else
    {
      self->hPipe = CreateNamedPipe(self->name,
                          PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                          PIPE_WAIT,
                          PIPE_UNLIMITED_INSTANCES,
                          BUFSIZE,
                          BUFSIZE,
                          PIPE_TIMEOUT,
                          &self->sec_attr);
      if (self->hPipe != INVALID_HANDLE_VALUE)
        {
          control_server_start_listening(self);
        }
      else
        {
          msg_error("Can't create named pipe", evt_tag_errno("error",GetLastError()), NULL);
        }
    }
  g_string_free(error_description, TRUE);
  return;
}

ControlServer *
control_server_new(const gchar *name)
{
  ControlServer *self = g_new0(ControlServer, 1);

  self->name = g_strdup(name);
  self->connected = FALSE;
  self->hPipe = INVALID_HANDLE_VALUE;

  IV_HANDLE_INIT(&self->hConnected);
  self->hConnected.cookie = self;
  self->hConnected.handle = INVALID_HANDLE_VALUE;

  IV_HANDLE_INIT(&self->hInput);
  self->hInput.cookie = self;
  self->hInput.handle = INVALID_HANDLE_VALUE;

  IV_HANDLE_INIT(&self->hOutput);
  self->hOutput.cookie = self;
  self->hOutput.handle = INVALID_HANDLE_VALUE;

  self->output_buffer = g_string_sized_new(256);
  self->input_buffer = g_string_sized_new(128);

  self->command_list = g_hash_table_new_full(g_str_hash,g_str_equal,g_free,NULL);
  return self;
}

void
control_server_free(ControlServer *self)
{
  if (self->hPipe != INVALID_HANDLE_VALUE)
    {
      DisconnectNamedPipe(self->hPipe);
      CloseHandle(self->hPipe);
    }

  if (iv_handle_registered(&self->hConnected))
    iv_handle_unregister(&self->hConnected);

  if (iv_handle_registered(&self->hInput))
    iv_handle_unregister(&self->hInput);

  if (iv_handle_registered(&self->hOutput))
    iv_handle_unregister(&self->hOutput);

  g_hash_table_destroy(self->command_list);
  CloseHandle(self->hInput.handle);
  CloseHandle(self->hOutput.handle);
  CloseHandle(self->hConnected.handle);

  free(self);
  return;
}

void
control_server_register_command_handler(ControlServer *self,const gchar *cmd, COMMAND_HANDLER handler)
{
  g_hash_table_insert(self->command_list, g_strdup(cmd), handler);
}

COMMAND_HANDLER
control_server_get_command_handler(ControlServer *self, gchar *cmd)
{
  return (COMMAND_HANDLER)g_hash_table_lookup(self->command_list, cmd);
}
