#include <syslog.h>

#ifdef EVENTLOG_SYSLOG_MACROS
#include <evtlog.h>
#endif

int 
main()
{
  openlog("evtsyslog", LOG_PID, 0);
  syslog(LOG_AUTH | LOG_NOTICE, "test message");
  closelog();
  return 0;
}
