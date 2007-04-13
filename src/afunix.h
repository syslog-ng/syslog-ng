#ifndef SDUNIX_H_INCLUDED
#define SDUNIX_H_INCLUDED

#include "driver.h"
#include "afsocket.h"

typedef struct _AFUnixSourceDriver
{
  AFSocketSourceDriver super;
} AFUnixSourceDriver;

void afunix_sd_set_uid(LogDriver *self, gchar *owner);
void afunix_sd_set_gid(LogDriver *self, gchar *group);
void afunix_sd_set_perm(LogDriver *self, int perm);

LogDriver *afunix_sd_new(gchar *filename, guint32 flags);

typedef struct _AFUnixDestDriver
{
  AFSocketDestDriver super;
} AFUnixDestDriver;

LogDriver *afunix_dd_new(gchar *filename, guint flags);

#endif

