#ifndef CENTER_H_INCLUDED
#define CENTER_H_INCLUDED

#include "syslog-ng.h"

#include "logpipe.h"

#define EP_SOURCE 1
#define EP_FILTER 2
#define EP_DESTINATION 3

typedef struct _LogEndpoint
{
  struct _LogEndpoint *ep_next;
  GString *name;
  gint type;
  gpointer ref;
} LogEndpoint;

LogEndpoint *log_endpoint_new(gint type, gchar *name);
void log_endpoint_append(LogEndpoint *a, LogEndpoint *b);
void log_endpoint_free(LogEndpoint *self);


#define LC_CATCHALL 1
#define LC_FALLBACK 2
#define LC_FINAL    4
#define LC_FLOW_CONTROL 8

typedef struct _LogConnection
{
  GHashTable *source_cache;
  GPtrArray *sources;
  GPtrArray *filters;
  GPtrArray *destinations;
  guint32 flags;
} LogConnection;

LogConnection *log_connection_new(LogEndpoint *endpoints, guint32 flags);
void log_connection_free(LogConnection *self);

#define LC_STATE_INIT_SOURCES 1
#define LC_STATE_INIT_DESTS 2
#define LC_STATE_WORKING 3

typedef struct _LogCenter 
{
  LogPipe super;
  GlobalConfig *cfg;
  PersistentConfig *persist;
  gint state;
  gboolean success;
} LogCenter;

LogCenter *log_center_new(void);

static inline LogCenter *
log_center_ref(LogCenter *s)
{
  return (LogCenter *) log_pipe_ref((LogPipe *) s);
}

static inline void
log_center_unref(LogCenter *s)
{
  log_pipe_unref((LogPipe *) s);
}

#endif
