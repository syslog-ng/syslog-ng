#ifndef SGROUP_H_INCLUDED
#define SGROUP_H_INCLUDED

#include "syslog-ng.h"
#include "logpipe.h"
#include "driver.h"

typedef struct _LogSourceGroup
{
  LogPipe super;
  GString *name;
  LogDriver *drivers;
  
  gboolean chain_hostnames;
  gboolean keep_hostname;
  gboolean use_dns;
  gboolean use_fqdn;
  
} LogSourceGroup;

static inline LogSourceGroup *
log_sgrp_ref(LogSourceGroup *s)
{
  return (LogSourceGroup *) log_pipe_ref((LogPipe *) s);
}

static inline void
log_sgrp_unref(LogSourceGroup *s)
{
  log_pipe_unref((LogPipe *) s);
}

LogSourceGroup *log_source_group_new(gchar *name, LogDriver *drivers);

#endif
