#ifndef SYSLOG_NG_H_INCLUDED
#define SYSLOG_NG_H_INCLUDED

#include <config.h>

#if ENABLE_DMALLOC
#define USE_DMALLOC
#endif

#include <glib.h>

#if ENABLE_DEBUG
#define PATH_SYSLOG_NG_CONF     "syslog-ng.conf"
#define PATH_PIDFILE            "syslog-ng.pid"
#else
#define PATH_SYSLOG_NG_CONF     PATH_SYSCONFDIR "/syslog-ng.conf"
#define PATH_PIDFILE            "/var/run/syslog-ng.pid"
#endif

#define LOG_PRIORITY_LISTEN 100
#define LOG_PRIORITY_READER 0
#define LOG_PRIORITY_WRITER -100
#define LOG_PRIORITY_CONNECT -150

#endif
