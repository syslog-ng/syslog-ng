#include "control_client.h"
#include <stdio.h>

struct _ControlClient
{
  HANDLE hPipe;
  gchar *path;
};

ControlClient *control_client_new(const gchar *path)
{
  ControlClient *self = g_new0(ControlClient,1);

  self->path = g_strdup(path);
  self->hPipe = INVALID_HANDLE_VALUE;

  return self;
}

gboolean
control_client_connect(ControlClient *self)
{
  while(1)
    {
      self->hPipe = CreateFile(self->path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
      if (self->hPipe == INVALID_HANDLE_VALUE)
        {
          DWORD error = GetLastError();
          if (error == ERROR_PIPE_BUSY)
            {
              if (WaitNamedPipe(self->path, 5000))
                {
                  continue;
                }
            }
          fprintf(stderr,"Can't connect to the control pipe: %s error: %lu", self->path, GetLastError());
          return FALSE;
        }
      break;
    }
  return TRUE;
}

gint
control_client_send_command(ControlClient *self, const gchar *cmd)
{
  DWORD bytes_written;
  if (!WriteFile(self->hPipe, cmd, strlen(cmd), &bytes_written, NULL))
    {
      fprintf(stderr,"Can't send command to the control pipe: %s error: %lu", self->path, GetLastError());
      return -1;
    }
  return (gint)bytes_written;
}

#define BUFF_LEN 8192

GString*
control_client_read_reply(ControlClient *self)
{
  DWORD bytes_read;
  gchar buff[BUFF_LEN];
  GString *reply = g_string_sized_new(256);

  while (1)
    {
      if (!ReadFile(self->hPipe, buff, BUFF_LEN, &bytes_read, NULL))
        {
          fprintf(stderr, "Error reading control socket, error='%lu'\n", GetLastError());
          g_string_free(reply, TRUE);
          return NULL;
        }

      g_string_append_len(reply, buff, bytes_read);

      if (reply->str[reply->len - 1] == '\n' &&
          reply->str[reply->len - 2] == '.' &&
          reply->str[reply->len - 3] == '\n')
        {
          g_string_truncate(reply, reply->len - 3);
          break;
        }
    }
  return reply;
}

void control_client_free(ControlClient *self)
{
  g_free(self->path);
  if (self->hPipe != INVALID_HANDLE_VALUE)
    {
      CloseHandle(self->hPipe);
    }
  g_free(self);
}
