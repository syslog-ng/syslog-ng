#ifndef AFSOCKET_H_INCLUDED
#define AFSOCKET_H_INCLUDED

#include "driver.h"
#include "logreader.h"
#include "logwriter.h"

#define AFSOCKET_DGRAM               0x0001
#define AFSOCKET_STREAM              0x0002
#define AFSOCKET_LOCAL               0x0004

#define AFSOCKET_KEEP_ALIVE          0x0100
#define AFSOCKET_LISTENER_KEEP_ALIVE 0x0200

#define AFSOCKET_PROTO_RFC3164	     0x0400

typedef struct _AFSocketSourceDriver AFSocketSourceDriver;

struct _AFSocketSourceDriver
{
  LogDriver super;
  guint32 flags;
  gint fd;
  guint source_id;
  LogPipe *reader;
  LogReaderOptions reader_options;

  GSockAddr *bind_addr;
  gint max_connections;
  gint num_connections;
  gint listen_backlog;
  GList *connections;
  void (*open_connection)(struct _AFSocketSourceDriver *s, gint fd, struct sockaddr *sa, socklen_t salen);
};

void afsocket_sd_set_keep_alive(LogDriver *self, gint enable);
void afsocket_sd_set_max_connections(LogDriver *self, gint max_connections);

void afsocket_sd_init_instance(AFSocketSourceDriver *self, guint32 flags);

typedef struct _AFSocketDestDriver
{
  LogDriver super;
  guint32 flags;
  gint fd;
  guint source_id;
  LogPipe *writer;
  LogWriterOptions writer_options;

  GSockAddr *bind_addr;
  GSockAddr *dest_addr;
  gint time_reopen;
  guint reconnect_timer;
} AFSocketDestDriver;

void afsocket_dd_init_instance(AFSocketDestDriver *self, guint32 flags);


#endif
