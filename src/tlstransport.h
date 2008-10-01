/*
 * COPYRIGHTHERE
 */
  
#ifndef TLSTRANSPORT_H_INCLUDED
#define TLSTRANSPORT_H_INCLUDED

#include "logtransport.h"

#include "tlscontext.h"

LogTransport *log_transport_tls_new(TLSSession *tls_session, gint fd, guint flags);

#endif
