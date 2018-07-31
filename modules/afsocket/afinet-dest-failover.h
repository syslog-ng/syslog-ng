#ifndef AFINET_DEST_FAILOVER_H_INCLUDED
#define AFINET_DEST_FAILOVER_H_INCLUDED

#include "syslog-ng.h"
#include "gsocket.h"
#include "transport-mapper-inet.h"
#include "cfg.h"

typedef struct _AFInetDestDriverFailover AFInetDestDriverFailover;
typedef void (*AFInetOnPrimaryAvailable)(gpointer cookie, gint fd, GSockAddr *addr);

AFInetDestDriverFailover *
afinet_dd_failover_new(void);

void
afinet_dd_failover_init(AFInetDestDriverFailover *self,
                        LogExprNode *owner_expr, TransportMapper *transport_mapper,
                        const gchar *primary, gchar *dest_port,
                        SocketOptions *socket_options, GSockAddr *bind_addr);

void afinet_dd_failover_free(AFInetDestDriverFailover *self);

void afinet_dd_failover_enable_failback(AFInetDestDriverFailover *self, gpointer cookie,
                                        AFInetOnPrimaryAvailable callback);
void afinet_dd_failover_add_servers(AFInetDestDriverFailover *self, GList *failovers);
void afinet_dd_failover_set_tcp_probe_interval(AFInetDestDriverFailover *self, gint tcp_probe_interval);
void afinet_dd_failover_set_successful_probes_required(AFInetDestDriverFailover *self, gint successful_probes_required);

void afinet_dd_failover_next(AFInetDestDriverFailover *self);
const gchar *afinet_dd_failover_get_hostname(AFInetDestDriverFailover *self);


#endif
