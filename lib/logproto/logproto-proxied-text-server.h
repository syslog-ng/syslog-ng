#ifndef LOGPROTO_PROXIED_TEXT_SERVER
#define LOGPROTO_PROXIED_TEXT_SERVER

#include "logproto-text-server.h"

#define IP_BUF_SIZE 64

struct ProxyProtoInfo
{
  gboolean unknown;

  gchar src_ip[IP_BUF_SIZE];
  gchar dst_ip[IP_BUF_SIZE];

  int ip_version;
  int src_port;
  int dst_port;
};

typedef struct _LogProtoProxiedTextServer
{
  LogProtoTextServer super;

  // Info received from the proxied that should be added as LogTransportAuxData to
  // any message received through this server instance.
  struct ProxyProtoInfo *info;

  // Flag to only run handshake() once
  gboolean handshake_done;

  // Since LogProtoTextServer doesn't export fetch_from_buffer(), we will save a reference here
  LogProtoStatus (*fetch_from_buffer)(LogProtoServer *s, const guchar **msg, gsize *msg_len, gboolean *may_read,
                                      LogTransportAuxData *aux, Bookmark *bookmark);
} LogProtoProxiedTextServer;

LogProtoServer *log_proto_proxied_text_server_new(LogTransport *transport, const LogProtoServerOptions *options);

#endif
