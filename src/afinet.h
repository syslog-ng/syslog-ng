#ifndef AFINET_H_INCLUDED
#define AFINET_H_INCLUDED

#include "afsocket.h"

typedef struct _AFInetSourceDriver
{
  AFSocketSourceDriver super;
} AFInetSourceDriver;

LogDriver *afinet_sd_new(gchar *ip, gint port, guint flags);
void afinet_sd_set_localport(LogDriver *self, gint port, gchar *service, gchar *proto);
void afinet_sd_set_localip(LogDriver *self, gchar *ip);

#define afinet_sd_set_auth(a,b)
#define afinet_sd_set_mac(a,b)
#define afinet_sd_set_encrypt(a,b)

typedef struct _AFInetDestDriver
{
  AFSocketDestDriver super;
} AFInetDestDriver;

LogDriver *afinet_dd_new(gchar *ip, gint port, guint flags);
void afinet_dd_set_localport(LogDriver *self, gint port, gchar *service, gchar *proto);
void afinet_dd_set_destport(LogDriver *self, gint port, gchar *service, gchar *proto);
void afinet_dd_set_localip(LogDriver *self, gchar *ip);
void afinet_dd_set_sync_freq(LogDriver *self, gint sync_freq);

#endif
