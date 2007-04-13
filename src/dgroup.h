#ifndef DGROUP_H_INCLUDED
#define DGROUP_H_INCLUDED

#include "syslog-ng.h"
#include "logpipe.h"
#include "driver.h"


typedef struct _LogDestGroup
{
  LogPipe super;
  GString *name;
  LogDriver *drivers;
} LogDestGroup;

#define log_dgrp_ref(s)     ((LogDestGroup *) log_pipe_ref((LogPipe *) s))
#define log_dgrp_unref(s)   log_pipe_unref((LogPipe *) s)

LogDestGroup *log_dest_group_new(gchar *name, LogDriver *drivers);

#endif
