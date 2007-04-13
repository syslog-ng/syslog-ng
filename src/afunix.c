#include "afunix.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

void afunix_sd_set_uid(LogDriver *self, gchar *owner)
{}

void afunix_sd_set_gid(LogDriver *self, gchar *group)
{}

void afunix_sd_set_perm(LogDriver *self, int perm)
{}

LogDriver *
afunix_sd_new(gchar *filename, guint32 flags)
{
  AFUnixSourceDriver *self = g_new0(AFUnixSourceDriver, 1);

  afsocket_sd_init_instance(&self->super, flags);
  self->super.bind_addr = g_sockaddr_unix_new(filename);
  return &self->super.super;
}

LogDriver *
afunix_dd_new(gchar *filename, guint flags)
{
  AFUnixDestDriver *self = g_new0(AFUnixDestDriver, 1);
  
  afsocket_dd_init_instance(&self->super, flags);
  self->super.bind_addr = NULL;
  self->super.dest_addr = g_sockaddr_unix_new(filename);
  return &self->super.super;
}
